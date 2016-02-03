/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */



#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/clk/msm-clk.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/iommu.h>
#include <asm/dma-iommu.h>
#include <linux/dma-direction.h>
#include <linux/dma-attrs.h>
#include <linux/dma-buf.h>

#include "msm_camera_io_util.h"
#include "msm_jpeg_platform.h"
#include "msm_jpeg_sync.h"
#include "msm_jpeg_common.h"
#include "msm_jpeg_hw.h"

#define JPEG_DT_PROP_CNT 2

static int msm_jpeg_get_regulator_info(struct msm_jpeg_device *jpeg_dev,
	struct platform_device *pdev)
{
	uint32_t count;
	int i, rc;

	struct device_node *of_node;
	of_node = pdev->dev.of_node;

	if (of_get_property(of_node, "qcom,vdd-names", NULL)) {

		count = of_property_count_strings(of_node, "qcom,vdd-names");

		JPEG_DBG("count = %d\n", count);
		if ((count == 0) || (count == -EINVAL)) {
			pr_err("no regulators found in device tree, count=%d",
				count);
			return -EINVAL;
		}

		if (count > JPEG_REGULATOR_MAX) {
			pr_err("invalid count=%d, max is %d\n", count,
				JPEG_REGULATOR_MAX);
			return -EINVAL;
		}

		for (i = 0; i < count; i++) {
			rc = of_property_read_string_index(of_node,
				"qcom,vdd-names", i,
				&(jpeg_dev->regulator_names[i]));
			JPEG_DBG("regulator-names[%d] = %s\n",
			i, jpeg_dev->regulator_names[i]);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				return rc;
			}
		}
	} else {
		jpeg_dev->regulator_names[0] = "vdd";
		count = 1;
	}
	jpeg_dev->num_regulator = count;
	return 0;
}

static int msm_jpeg_regulator_enable(struct device *dev, const char **reg_names,
	struct regulator **reg_ptr, int num_reg, int enable)
{
	int i;
	int rc = 0;
	if (enable) {
		for (i = 0; i < num_reg; i++) {
			JPEG_DBG("%s enable %s\n", __func__, reg_names[i]);
			reg_ptr[i] = regulator_get(dev, reg_names[i]);
			if (IS_ERR(reg_ptr[i])) {
				pr_err("%s get failed\n", reg_names[i]);
				rc = PTR_ERR(reg_ptr[i]);
				reg_ptr[i] = NULL;
				goto cam_reg_get_err;
			}

			rc = regulator_enable(reg_ptr[i]);
			if (rc < 0) {
				pr_err("%s enable failed\n", reg_names[i]);
				goto cam_reg_enable_err;
			}
		}
	} else {
		for (i = num_reg - 1; i >= 0; i--) {
			if (reg_ptr[i] != NULL) {
				JPEG_DBG("%s disable %s\n", __func__,
					reg_names[i]);
				regulator_disable(reg_ptr[i]);
				regulator_put(reg_ptr[i]);
			}
		}
	}
	return rc;

cam_reg_enable_err:
	regulator_put(reg_ptr[i]);
cam_reg_get_err:
	for (i--; i >= 0; i--) {
		if (reg_ptr[i] != NULL) {
			regulator_disable(reg_ptr[i]);
			regulator_put(reg_ptr[i]);
		}
	}
	return rc;
}


static int msm_jpeg_get_clk_info(struct msm_jpeg_device *jpeg_dev,
	struct platform_device *pdev)
{
	uint32_t count;
	int i, rc;
	uint32_t rates[JPEG_CLK_MAX];

	struct device_node *of_node;
	of_node = pdev->dev.of_node;

	count = of_property_count_strings(of_node, "clock-names");

	JPEG_DBG("count = %d\n", count);
	if (count == 0) {
		pr_err("no clocks found in device tree, count=%d", count);
		return 0;
	}

	if (count > JPEG_CLK_MAX) {
		pr_err("invalid count=%d, max is %d\n", count,
			JPEG_CLK_MAX);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &(jpeg_dev->jpeg_clk_info[i].clk_name));
		JPEG_DBG("clock-names[%d] = %s\n",
			 i, jpeg_dev->jpeg_clk_info[i].clk_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}
	}
	rc = of_property_read_u32_array(of_node, "qcom,clock-rates",
		rates, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}
	for (i = 0; i < count; i++) {
		jpeg_dev->jpeg_clk_info[i].clk_rate =
			(rates[i] == 0) ? (long) -1 : (long) rates[i];
		JPEG_DBG("clk_rate[%d] = %ld\n",
			i, jpeg_dev->jpeg_clk_info[i].clk_rate);
	}
	jpeg_dev->num_clk = count;
	return 0;
}


int msm_jpeg_platform_set_clk_rate(struct msm_jpeg_device *pgmn_dev,
		long clk_rate)
{
	int rc = 0;
	struct clk *jpeg_clk;

	jpeg_clk = clk_get(&pgmn_dev->pdev->dev, "core_clk");
	if (IS_ERR(jpeg_clk)) {
		JPEG_PR_ERR("%s get failed\n", "core_clk");
		rc = PTR_ERR(jpeg_clk);
		goto error;
	}

	clk_rate = clk_round_rate(jpeg_clk, clk_rate);
	if (clk_rate < 0) {
		JPEG_PR_ERR("%s:%d] round rate failed", __func__, __LINE__);
		rc = -EINVAL;
		goto error;
	}
	JPEG_DBG("%s:%d] jpeg clk rate %ld", __func__, __LINE__, clk_rate);

	rc = clk_set_rate(jpeg_clk, clk_rate);

error:
	return rc;
}

void msm_jpeg_platform_p2v(int iommu_hdl, int fd)
{
	cam_smmu_put_phy_addr(iommu_hdl, fd);
	return;
}

uint32_t msm_jpeg_platform_v2p(struct msm_jpeg_device *pgmn_dev, int fd,
	uint32_t len, int iommu_hdl)
{
	dma_addr_t paddr;
	size_t size;
	int rc;

	rc = cam_smmu_get_phy_addr(pgmn_dev->iommu_hdl, fd, CAM_SMMU_MAP_RW,
			&paddr, &size);
	JPEG_DBG("%s:%d] addr 0x%x size %zu", __func__, __LINE__,
		(uint32_t)paddr, size);

	if (rc < 0) {
		JPEG_PR_ERR("%s: fd %d got phy addr error %d\n", __func__, fd,
			rc);
		goto err_get_phy;
	}

	/* validate user input */
	if (len > size) {
		JPEG_PR_ERR("%s: invalid offset + len\n", __func__);
		goto err_size;
	}

	return paddr;
err_size:
	cam_smmu_put_phy_addr(pgmn_dev->iommu_hdl, fd);
err_get_phy:
	return 0;
}

static void set_vbif_params(struct msm_jpeg_device *pgmn_dev,
	 void *jpeg_vbif_base)
{
	msm_camera_io_w(0x1,
		jpeg_vbif_base + JPEG_VBIF_CLKON);

	if (pgmn_dev->hw_version != JPEG_8994) {
		msm_camera_io_w(0x10101010,
			jpeg_vbif_base + JPEG_VBIF_IN_RD_LIM_CONF0);
		msm_camera_io_w(0x10101010,
			jpeg_vbif_base + JPEG_VBIF_IN_RD_LIM_CONF1);
		msm_camera_io_w(0x10101010,
			jpeg_vbif_base + JPEG_VBIF_IN_RD_LIM_CONF2);
		msm_camera_io_w(0x10101010,
			jpeg_vbif_base + JPEG_VBIF_IN_WR_LIM_CONF0);
		msm_camera_io_w(0x10101010,
			jpeg_vbif_base + JPEG_VBIF_IN_WR_LIM_CONF1);
		msm_camera_io_w(0x10101010,
			jpeg_vbif_base + JPEG_VBIF_IN_WR_LIM_CONF2);
		msm_camera_io_w(0x00001010,
			jpeg_vbif_base + JPEG_VBIF_OUT_RD_LIM_CONF0);
		msm_camera_io_w(0x00000110,
			jpeg_vbif_base + JPEG_VBIF_OUT_WR_LIM_CONF0);
		msm_camera_io_w(0x00000707,
			jpeg_vbif_base + JPEG_VBIF_DDR_OUT_MAX_BURST);
		msm_camera_io_w(0x00000FFF,
			jpeg_vbif_base + JPEG_VBIF_OUT_AXI_AOOO_EN);
		msm_camera_io_w(0x0FFF0FFF,
			jpeg_vbif_base + JPEG_VBIF_OUT_AXI_AOOO);
		msm_camera_io_w(0x2222,
			jpeg_vbif_base + JPEG_VBIF_OUT_AXI_AMEMTYPE_CONF1);
	}

	msm_camera_io_w(0x7,
		jpeg_vbif_base + JPEG_VBIF_OCMEM_OUT_MAX_BURST);
	msm_camera_io_w(0x00000030,
		jpeg_vbif_base + JPEG_VBIF_ARB_CTL);

	/*FE and WE QOS configuration need to be set when
	QOS RR arbitration is enabled*/
	if (pgmn_dev->hw_version != JPEG_8974_V1)
		msm_camera_io_w(0x00000003,
				jpeg_vbif_base + JPEG_VBIF_ROUND_ROBIN_QOS_ARB);
	else
		msm_camera_io_w(0x00000001,
				jpeg_vbif_base + JPEG_VBIF_ROUND_ROBIN_QOS_ARB);

	msm_camera_io_w(0x22222222,
		jpeg_vbif_base + JPEG_VBIF_OUT_AXI_AMEMTYPE_CONF0);

}

/*
 * msm_jpeg_set_init_dt_parms() - get device tree config and write to registers.
 * @pgmn_dev: Pointer to jpeg device.
 * @dt_prop_name: Device tree property name.
 * @base: Base address.
 *
 * This function reads register offsets and values from dtsi based on
 * device tree property name and writes to jpeg registers.
 *
 * Return: 0 on success and negative error on failure.
 */
static int32_t msm_jpeg_set_init_dt_parms(struct msm_jpeg_device *pgmn_dev,
	const char *dt_prop_name,
	void *base)
{
	struct device_node *of_node;
	int32_t i = 0 , rc = 0;
	uint32_t *dt_reg_settings = NULL;
	uint32_t dt_count = 0;

	of_node = pgmn_dev->pdev->dev.of_node;
	JPEG_DBG("%s:%d E\n", __func__, __LINE__);

	if (!of_get_property(of_node, dt_prop_name,
				&dt_count)) {
		JPEG_DBG("%s: Error property does not exist\n",
				__func__);
		return -ENOENT;
	}
	if (dt_count % 8) {
		JPEG_PR_ERR("%s: Error invalid entries\n",
				__func__);
		return -EINVAL;
	}
	dt_count /= 4;
	if (dt_count != 0) {
		dt_reg_settings = kcalloc(dt_count, sizeof(uint32_t),
			GFP_KERNEL);
		if (!dt_reg_settings) {
			JPEG_PR_ERR("%s:%d No memory\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		rc = of_property_read_u32_array(of_node,
				dt_prop_name,
				dt_reg_settings,
				dt_count);
		if (rc < 0) {
			JPEG_PR_ERR("%s: No reg info\n",
				__func__);
			kfree(dt_reg_settings);
			return -EINVAL;
		}
		for (i = 0; i < dt_count; i = i + 2) {
			JPEG_DBG("%s:%d] %p %08x\n",
					__func__, __LINE__,
					base + dt_reg_settings[i],
					dt_reg_settings[i + 1]);
			msm_camera_io_w(dt_reg_settings[i + 1],
					base + dt_reg_settings[i]);
		}
		kfree(dt_reg_settings);
	}
	return 0;
}

static struct msm_bus_vectors msm_jpeg_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_JPEG,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors msm_jpeg_vectors[] = {
	{
		.src = MSM_BUS_MASTER_JPEG,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = JPEG_CLK_RATE * 2.5,
		.ib  = JPEG_CLK_RATE * 2.5,
	},
};

static struct msm_bus_paths msm_jpeg_bus_client_config[] = {
	{
		ARRAY_SIZE(msm_jpeg_init_vectors),
		msm_jpeg_init_vectors,
	},
	{
		ARRAY_SIZE(msm_jpeg_vectors),
		msm_jpeg_vectors,
	},
};

static struct msm_bus_scale_pdata msm_jpeg_bus_client_pdata = {
	msm_jpeg_bus_client_config,
	ARRAY_SIZE(msm_jpeg_bus_client_config),
	.name = "msm_jpeg",
};

static int msm_jpeg_attach_iommu(struct msm_jpeg_device *pgmn_dev)
{
	int rc;
	rc = cam_smmu_ops(pgmn_dev->iommu_hdl, CAM_SMMU_ATTACH);
	if (rc < 0) {
		JPEG_PR_ERR("%s: Device attach failed\n", __func__);
		return -ENODEV;
	}
	JPEG_DBG("%s:%d] handle %d attach\n",
			__func__, __LINE__, pgmn_dev->iommu_hdl);
	return 0;
}

static int msm_jpeg_detach_iommu(struct msm_jpeg_device *pgmn_dev)
{
	JPEG_DBG("%s:%d] handle %d detach\n",
			__func__, __LINE__, pgmn_dev->iommu_hdl);
	cam_smmu_ops(pgmn_dev->iommu_hdl, CAM_SMMU_DETACH);
	return 0;
}



int msm_jpeg_platform_init(struct platform_device *pdev,
	struct resource **mem,
	void **base,
	int *irq,
	irqreturn_t (*handler)(int, void *),
	void *context)
{
	int rc = -1;
	int jpeg_irq;
	struct resource *jpeg_mem, *vbif_mem, *jpeg_io, *jpeg_irq_res;
	void *jpeg_base;
	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *) context;

	pgmn_dev->state = MSM_JPEG_IDLE;

	jpeg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!jpeg_mem) {
		JPEG_PR_ERR("%s: jpeg no mem resource?\n", __func__);
		return -ENODEV;
	}

	vbif_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!vbif_mem) {
		JPEG_PR_ERR("%s: vbif no mem resource?\n", __func__);
		return -ENODEV;
	}

	jpeg_irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!jpeg_irq_res) {
		JPEG_PR_ERR("no irq resource?\n");
		return -ENODEV;
	}
	jpeg_irq = jpeg_irq_res->start;
	JPEG_DBG("%s base address: 0x%lx, jpeg irq number: %d\n", __func__,
		(unsigned long)jpeg_mem->start, jpeg_irq);

	pgmn_dev->jpeg_bus_client =
		msm_bus_scale_register_client(&msm_jpeg_bus_client_pdata);
	if (!pgmn_dev->jpeg_bus_client) {
		JPEG_PR_ERR("%s: Registration Failed!\n", __func__);
		pgmn_dev->jpeg_bus_client = 0;
		return -EINVAL;
	}

	jpeg_io = request_mem_region(jpeg_mem->start,
		resource_size(jpeg_mem), pdev->name);
	if (!jpeg_io) {
		JPEG_PR_ERR("%s: region already claimed\n", __func__);
		return -EBUSY;
	}

	jpeg_base = ioremap(jpeg_mem->start, resource_size(jpeg_mem));
	if (!jpeg_base) {
		rc = -ENOMEM;
		JPEG_PR_ERR("%s: ioremap failed\n", __func__);
		goto fail_remap;
	}

	rc = msm_jpeg_get_regulator_info(pgmn_dev, pgmn_dev->pdev);
	if (rc < 0) {
		JPEG_PR_ERR("%s:%d]jpeg regulator get failed\n",
				__func__, __LINE__);
		goto fail_fs;
	}

	rc = msm_jpeg_regulator_enable(&pgmn_dev->pdev->dev,
		pgmn_dev->regulator_names, pgmn_dev->jpeg_fs,
		pgmn_dev->num_regulator, 1);
	if (rc < 0) {
		JPEG_PR_ERR("%s:%d] jpeg regulator enable failed rc = %d\n",
				 __func__, __LINE__, rc);
	goto fail_fs;
	}

	if (msm_jpeg_get_clk_info(pgmn_dev, pgmn_dev->pdev) < 0) {
		JPEG_PR_ERR("%s:%d]jpeg clock get failed\n",
				__func__, __LINE__);
		goto fail_fs;
	}

	rc = msm_cam_clk_enable(&pgmn_dev->pdev->dev, pgmn_dev->jpeg_clk_info,
	 pgmn_dev->jpeg_clk, pgmn_dev->num_clk, 1);
	if (rc < 0) {
		JPEG_PR_ERR("%s: clk failed rc = %d\n", __func__, rc);
		goto fail_clk;
	}

	pgmn_dev->hw_version = msm_camera_io_r(jpeg_base +
		JPEG_HW_VERSION);
	JPEG_DBG_HIGH("%s:%d] jpeg HW version 0x%x", __func__, __LINE__,
		pgmn_dev->hw_version);

	pgmn_dev->jpeg_vbif = ioremap(vbif_mem->start, resource_size(vbif_mem));
	if (!pgmn_dev->jpeg_vbif) {
		rc = -ENOMEM;
		JPEG_PR_ERR("%s: ioremap failed\n", __func__);
		goto fail_vbif;
	}
	JPEG_DBG("%s:%d] jpeg_vbif 0x%lx", __func__, __LINE__,
		(unsigned long)pgmn_dev->jpeg_vbif);

	rc = msm_jpeg_attach_iommu(pgmn_dev);
	if (rc < 0)
		goto fail_iommu;

	rc = msm_jpeg_set_init_dt_parms(pgmn_dev, "qcom,vbif-reg-settings",
		pgmn_dev->jpeg_vbif);
	if (rc == -ENOENT) {
		JPEG_DBG("%s: No qcom,vbif-reg-settings property\n", __func__);
		set_vbif_params(pgmn_dev, pgmn_dev->jpeg_vbif);
	} else if (rc < 0) {
		JPEG_PR_ERR("%s: vbif params set fail\n", __func__);
		goto fail_set_vbif;
	}

	rc = request_irq(jpeg_irq, handler, IRQF_TRIGGER_RISING,
		dev_name(&pdev->dev), context);
	if (rc) {
		JPEG_PR_ERR("%s: request_irq failed, %d\n", __func__,
			jpeg_irq);
		goto fail_request_irq;
	}

	*mem  = jpeg_mem;
	*base = jpeg_base;
	*irq  = jpeg_irq;

	pgmn_dev->state = MSM_JPEG_INIT;
	return rc;

fail_request_irq:
fail_set_vbif:
	msm_jpeg_detach_iommu(pgmn_dev);

fail_iommu:
	iounmap(pgmn_dev->jpeg_vbif);

fail_vbif:
	msm_cam_clk_enable(&pgmn_dev->pdev->dev, pgmn_dev->jpeg_clk_info,
	pgmn_dev->jpeg_clk, pgmn_dev->num_clk, 0);

fail_clk:
	msm_jpeg_regulator_enable(&pgmn_dev->pdev->dev,
	pgmn_dev->regulator_names, pgmn_dev->jpeg_fs,
	pgmn_dev->num_regulator, 0);

fail_fs:
	iounmap(jpeg_base);

fail_remap:
	release_mem_region(jpeg_mem->start, resource_size(jpeg_mem));
	JPEG_DBG("%s:%d] fail\n", __func__, __LINE__);
	return rc;
}

int msm_jpeg_platform_release(struct resource *mem, void *base, int irq,
	void *context)
{
	int result = 0;

	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *) context;

	free_irq(irq, context);

	msm_jpeg_detach_iommu(pgmn_dev);

	if (pgmn_dev->jpeg_bus_client) {
		if (pgmn_dev->jpeg_bus_vote) {
			msm_bus_scale_client_update_request(
				pgmn_dev->jpeg_bus_client, 0);
			JPEG_BUS_UNVOTED(pgmn_dev);
			JPEG_DBG("%s:%d] Bus unvoted\n", __func__, __LINE__);
		}
		msm_bus_scale_unregister_client(pgmn_dev->jpeg_bus_client);
	}

	msm_cam_clk_enable(&pgmn_dev->pdev->dev, pgmn_dev->jpeg_clk_info,
	pgmn_dev->jpeg_clk, pgmn_dev->num_clk, 0);
	JPEG_DBG("%s:%d] clock disbale done", __func__, __LINE__);

	msm_jpeg_regulator_enable(&pgmn_dev->pdev->dev,
	pgmn_dev->regulator_names, pgmn_dev->jpeg_fs,
	pgmn_dev->num_regulator, 0);
	JPEG_DBG("%s:%d] regulator disable done", __func__, __LINE__);

	iounmap(pgmn_dev->jpeg_vbif);
	iounmap(base);
	release_mem_region(mem->start, resource_size(mem));
	pgmn_dev->state = MSM_JPEG_IDLE;
	JPEG_DBG("%s:%d] success\n", __func__, __LINE__);
	return result;
}

/*
 * msm_jpeg_platform_set_dt_config() - set jpeg device tree configuration.
 * @pgmn_dev: Pointer to jpeg device.
 *
 * This function holds an array of device tree property names and calls
 * msm_jpeg_set_init_dt_parms() for each property.
 *
 * Return: 0 on success and negative error on failure.
 */
int msm_jpeg_platform_set_dt_config(struct msm_jpeg_device *pgmn_dev)
{
	int rc = 0;
	uint8_t dt_prop_cnt = JPEG_DT_PROP_CNT;
	char *dt_prop_name[JPEG_DT_PROP_CNT] = {"qcom,qos-reg-settings",
		"qcom,prefetch-reg-settings"};

	while (dt_prop_cnt) {
		dt_prop_cnt--;
		rc = msm_jpeg_set_init_dt_parms(pgmn_dev,
			dt_prop_name[dt_prop_cnt],
			pgmn_dev->base);
		if (rc == -ENOENT) {
			JPEG_DBG("%s: No %s property\n", __func__,
				dt_prop_name[dt_prop_cnt]);
		} else if (rc < 0) {
			JPEG_PR_ERR("%s: %s params set fail\n", __func__,
				dt_prop_name[dt_prop_cnt]);
			return rc;
		}
	}
	return rc;
}

