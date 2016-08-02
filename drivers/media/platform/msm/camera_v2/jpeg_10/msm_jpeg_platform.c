/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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


int msm_jpeg_get_clock_index(struct msm_jpeg_device *pgmn_dev,
	const char *clk_name)
{
	uint32_t i = 0;

	for (i = 0; i < pgmn_dev->num_clk; i++) {
		if (!strcmp(clk_name, pgmn_dev->jpeg_clk_info[i].clk_name))
			return i;
	}
	return -EINVAL;
}

int msm_jpeg_platform_set_clk_rate(struct msm_jpeg_device *pgmn_dev,
		long clk_rate)
{
	int rc = 0;
	uint32_t msm_jpeg_idx;

	/* retrieve clock index from list of clocks */
	msm_jpeg_idx = msm_jpeg_get_clock_index(pgmn_dev,
		"core_clk");
	if (msm_jpeg_idx < 0)  {
		JPEG_PR_ERR("%s:Fail to get clock index\n", __func__);
		return -EINVAL;
	}

	/* set the rate */
	msm_camera_clk_set_rate(&pgmn_dev->pdev->dev,
		pgmn_dev->jpeg_clk[msm_jpeg_idx], clk_rate);

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
			JPEG_DBG("%s:%d] %pK %08x\n",
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


int msm_jpeg_platform_init(irqreturn_t (*handler)(int, void *),
	void *context)
{
	int rc = -1;
	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *) context;
	struct platform_device *pdev = pgmn_dev->pdev;

	pgmn_dev->state = MSM_JPEG_IDLE;

	/* enable all regulators */
	rc = msm_camera_regulator_enable(pgmn_dev->jpeg_vdd,
		pgmn_dev->num_reg, true);
	if (rc < 0) {
		JPEG_PR_ERR("%s: failed to enable regulators\n", __func__);
		goto err_reg_enable;
	}

	/* enable all clocks */
	rc = msm_camera_clk_enable(&pgmn_dev->pdev->dev,
			pgmn_dev->jpeg_clk_info, pgmn_dev->jpeg_clk,
			pgmn_dev->num_clk, true);
	if (rc < 0) {
		JPEG_PR_ERR("%s: clk enable failed\n", __func__);
		goto err_clk_enable;
	}

	/* attach the smmu context banks */
	rc = msm_jpeg_attach_iommu(pgmn_dev);
	if (rc < 0) {
		JPEG_PR_ERR("%s: iommu attach failed\n", __func__);
		goto err_fail_iommu;
	}
	rc = msm_jpeg_set_init_dt_parms(pgmn_dev, "qcom,vbif-reg-settings",
		pgmn_dev->vbif_base);
	if (rc == -ENOENT) {
		JPEG_DBG("%s: No qcom,vbif-reg-settings property\n", __func__);
		set_vbif_params(pgmn_dev, pgmn_dev->vbif_base);
	} else if (rc < 0) {
		JPEG_PR_ERR("%s: vbif params set fail\n", __func__);
		goto err_fail_set_vbif;
	}

	/* register the interrupt handler */
	rc = msm_camera_register_irq(pgmn_dev->pdev,
		pgmn_dev->jpeg_irq_res, handler, IRQF_TRIGGER_RISING,
		"jpeg", context);
	if (rc < 0) {
		JPEG_PR_ERR("%s: irq request fail\n", __func__);
		goto err_reg_irq_fail;
	}

	pgmn_dev->hw_version = msm_camera_io_r(pgmn_dev->base +
		JPEG_HW_VERSION);
	JPEG_DBG_HIGH("%s:%d] jpeg HW version 0x%x", __func__, __LINE__,
		pgmn_dev->hw_version);
	pgmn_dev->state = MSM_JPEG_INIT;

	return 0;
err_reg_irq_fail:
err_fail_set_vbif:
	msm_jpeg_detach_iommu(pgmn_dev);
err_fail_iommu:
	msm_camera_clk_enable(&pdev->dev, pgmn_dev->jpeg_clk_info,
		pgmn_dev->jpeg_clk, pgmn_dev->num_clk, false);
err_clk_enable:
	msm_camera_regulator_enable(pgmn_dev->jpeg_vdd,
		pgmn_dev->num_reg, false);
err_reg_enable:
	return rc;
}

int msm_jpeg_platform_setup(struct msm_jpeg_device *pgmn_dev)
{
	int rc = -1;
	int i;
	struct resource *jpeg_irq_res;
	void *jpeg_base, *vbif_base;
	struct platform_device *pdev = pgmn_dev->pdev;

	/* get the jpeg hardware device address */
	jpeg_base = msm_camera_get_reg_base(pdev, "jpeg_hw", true);
	if (!jpeg_base) {
		JPEG_PR_ERR("%s: jpeg no mem resource?\n", __func__);
		rc = -ENXIO;
		goto out;
	}

	/* get the jpeg vbif device address */
	vbif_base = msm_camera_get_reg_base(pdev, "jpeg_vbif", false);
	if (!vbif_base) {
		JPEG_PR_ERR("%s: vbif no mem resource?\n", __func__);
		rc = -ENXIO;
		goto err_vbif_base;
	}

	/* get the irq resource for the jpeg hardware */
	jpeg_irq_res = msm_camera_get_irq(pdev, "jpeg");
	if (!jpeg_irq_res) {
		JPEG_PR_ERR("%s: no irq resource?\n", __func__);
		rc = -ENXIO;
		goto err_jpeg_irq_res;
	}

	/* get all the clocks information */
	rc = msm_camera_get_clk_info(pdev, &pgmn_dev->jpeg_clk_info,
		&pgmn_dev->jpeg_clk, &pgmn_dev->num_clk);
	if (rc < 0) {
		JPEG_PR_ERR("%s: failed to get the clocks\n", __func__);
		rc = -ENXIO;
		goto err_jpeg_clk;
	}

	/*set memcore and mem periphery logic flags to 0*/
	for (i = 0; i < pgmn_dev->num_clk; i++) {
		if ((strcmp(pgmn_dev->jpeg_clk_info[i].clk_name,
				"core_clk") == 0) ||
			(strcmp(pgmn_dev->jpeg_clk_info[i].clk_name,
				"mmss_camss_jpeg_axi_clk") == 0)) {
			msm_camera_set_clk_flags(pgmn_dev->jpeg_clk[i],
				CLKFLAG_NORETAIN_MEM);
			msm_camera_set_clk_flags(pgmn_dev->jpeg_clk[i],
				CLKFLAG_NORETAIN_PERIPH);
		}
	}

	/* get all the regulators information */
	rc = msm_camera_get_regulator_info(pdev, &pgmn_dev->jpeg_vdd,
		&pgmn_dev->num_reg);
	if (rc < 0) {
		JPEG_PR_ERR("%s: failed to get the regulators\n", __func__);
		rc = -ENXIO;
		goto err_jpeg_get_reg;
	}

	/* map the dtsi cell id to bus client id */
	switch (pgmn_dev->pdev->id) {
	case 0:
		pgmn_dev->bus_client = CAM_BUS_CLIENT_JPEG_ENC0;
		break;
	case 1:
		pgmn_dev->bus_client = CAM_BUS_CLIENT_JPEG_ENC1;
		break;
	case 2:
		pgmn_dev->bus_client = CAM_BUS_CLIENT_JPEG_DEC;
		break;
	case 3:
		pgmn_dev->bus_client = CAM_BUS_CLIENT_JPEG_DMA;
		break;
	default:
		JPEG_PR_ERR("%s: invalid cell id :%d\n",
			__func__, pgmn_dev->pdev->id);
		goto err_jpeg_get_reg;
	}

	/* register the bus client */
	rc = msm_camera_register_bus_client(pgmn_dev->pdev,
			pgmn_dev->bus_client);
	if (rc < 0) {
		JPEG_PR_ERR("Fail to register bus client\n");
		rc = -EINVAL;
		goto err_reg_bus;
	}

	/* get the resource size of jpeg hardware */
	pgmn_dev->res_size = msm_camera_get_res_size(pdev, "jpeg_hw");
	if (!pgmn_dev->res_size) {
		JPEG_PR_ERR("Fail to resource size\n");
		rc = -EINVAL;
		goto err_res_size;
	}

	pgmn_dev->base = jpeg_base;
	pgmn_dev->vbif_base = vbif_base;
	pgmn_dev->jpeg_irq_res = jpeg_irq_res;

	return 0;

err_res_size:
	msm_camera_unregister_bus_client(pgmn_dev->bus_client);
err_reg_bus:
	msm_camera_put_regulators(pdev, &pgmn_dev->jpeg_vdd,
		pgmn_dev->num_reg);
err_jpeg_get_reg:
	msm_camera_put_clk_info(pdev, &pgmn_dev->jpeg_clk_info,
		&pgmn_dev->jpeg_clk, pgmn_dev->num_clk);
err_jpeg_clk:
err_jpeg_irq_res:
	msm_camera_put_reg_base(pdev, vbif_base, "jpeg_vbif", false);
err_vbif_base:
	msm_camera_put_reg_base(pdev, jpeg_base, "jpeg_hw", true);
out:
	return rc;
}

void msm_jpeg_platform_cleanup(struct msm_jpeg_device *pgmn_dev)
{
	/* unregister the bus client */
	msm_camera_unregister_bus_client(pgmn_dev->bus_client);
	/* release the regulators */
	msm_camera_put_regulators(pgmn_dev->pdev, &pgmn_dev->jpeg_vdd,
		pgmn_dev->num_reg);
	/* release all the clocks */
	msm_camera_put_clk_info(pgmn_dev->pdev, &pgmn_dev->jpeg_clk_info,
		&pgmn_dev->jpeg_clk, pgmn_dev->num_clk);
	/* release the jpeg device memory */
	msm_camera_put_reg_base(pgmn_dev->pdev, pgmn_dev->vbif_base,
		"jpeg_vbif", false);
	/* release the jpeg vbif device memory */
	msm_camera_put_reg_base(pgmn_dev->pdev, pgmn_dev->base,
		"jpeg_hw", true);
}

int msm_jpeg_platform_release(void *context)
{
	int result = 0;

	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *) context;

	/* release the irq */
	msm_camera_unregister_irq(pgmn_dev->pdev,
		pgmn_dev->jpeg_irq_res, context);

	msm_jpeg_detach_iommu(pgmn_dev);

	if (pgmn_dev->bus_client) {
		if (pgmn_dev->jpeg_bus_vote) {
			/* update the bw with zeroth vector */
			msm_camera_update_bus_vector(pgmn_dev->bus_client, 0);
			JPEG_BUS_UNVOTED(pgmn_dev);
			JPEG_DBG("%s:%d] Bus unvoted\n", __func__, __LINE__);
		}
	}

	/* disable all the clocks */
	msm_camera_clk_enable(&pgmn_dev->pdev->dev, pgmn_dev->jpeg_clk_info,
		pgmn_dev->jpeg_clk, pgmn_dev->num_clk, false);
	JPEG_DBG("%s:%d] clock disbale done", __func__, __LINE__);

	/* disable all the regulators */
	msm_camera_regulator_enable(pgmn_dev->jpeg_vdd,
		pgmn_dev->num_reg, false);
	JPEG_DBG("%s:%d] regulator disable done", __func__, __LINE__);

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

