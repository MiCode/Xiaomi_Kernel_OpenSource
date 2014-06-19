/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/msm_iommu_domains.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/msm_ion.h>
#include <linux/iommu.h>

#include "msm_jpeg_platform.h"
#include "msm_jpeg_sync.h"
#include "msm_jpeg_common.h"
#include "msm_jpeg_hw.h"
#include "msm_camera_io_util.h"

#define JPEG_CLK_INFO_MAX 16

static struct msm_cam_clk_info jpeg_8x_clk_info[JPEG_CLK_INFO_MAX];

static int msm_jpeg_get_clk_info(struct msm_jpeg_device *jpeg_dev,
	struct platform_device *pdev)
{
	uint32_t count;
	int i, rc;
	uint32_t rates[JPEG_CLK_INFO_MAX];

	struct device_node *of_node;
	of_node = pdev->dev.of_node;

	count = of_property_count_strings(of_node, "clock-names");

	JPEG_DBG("count = %d\n", count);
	if (count == 0) {
		pr_err("no clocks found in device tree, count=%d", count);
		return 0;
	}

	if (count > JPEG_CLK_INFO_MAX) {
		pr_err("invalid count=%d, max is %d\n", count,
			JPEG_CLK_INFO_MAX);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "clock-names",
				i, &(jpeg_8x_clk_info[i].clk_name));
		JPEG_DBG("clock-names[%d] = %s\n",
			 i, jpeg_8x_clk_info[i].clk_name);
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
		jpeg_8x_clk_info[i].clk_rate =
			(rates[i] == 0) ? -1 : rates[i];
		JPEG_DBG("clk_rate[%d] = %ld\n",
			i, jpeg_8x_clk_info[i].clk_rate);
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
		return rc;
	}

	rc = clk_set_rate(jpeg_clk, clk_rate);

	return rc;
}

void msm_jpeg_platform_p2v(struct msm_jpeg_device *pgmn_dev, struct file  *file,
	struct ion_handle **ionhandle, int domain_num)
{
	ion_unmap_iommu(pgmn_dev->jpeg_client, *ionhandle, domain_num, 0);
	ion_free(pgmn_dev->jpeg_client, *ionhandle);
	*ionhandle = NULL;
}

uint32_t msm_jpeg_platform_v2p(struct msm_jpeg_device *pgmn_dev, int fd,
	uint32_t len, struct file **file_p, struct ion_handle **ionhandle,
	int domain_num) {
	dma_addr_t paddr;
	unsigned long size;
	int rc;
	*ionhandle = ion_import_dma_buf(pgmn_dev->jpeg_client, fd);
	if (IS_ERR_OR_NULL(*ionhandle))
		return 0;

	rc = ion_map_iommu(pgmn_dev->jpeg_client, *ionhandle, domain_num, 0,
		SZ_4K, 0, &paddr, (unsigned long *)&size, 0, 0);
	JPEG_DBG("%s:%d] addr 0x%x size %ld", __func__, __LINE__,
		(uint32_t)paddr, size);

	if (rc < 0) {
		JPEG_PR_ERR("%s: ion_map_iommu fd %d error %d\n", __func__, fd,
			rc);
		goto error1;
	}

	/* validate user input */
	if (len > size) {
		JPEG_PR_ERR("%s: invalid offset + len\n", __func__);
		goto error1;
	}

	return paddr;
error1:
	ion_free(pgmn_dev->jpeg_client, *ionhandle);
	return 0;
}

static void set_vbif_params(struct msm_jpeg_device *pgmn_dev,
	 void *jpeg_vbif_base)
{
	writel_relaxed(0x1,
		jpeg_vbif_base + JPEG_VBIF_CLKON);
	writel_relaxed(0x10101010,
		jpeg_vbif_base + JPEG_VBIF_IN_RD_LIM_CONF0);
	writel_relaxed(0x10101010,
		jpeg_vbif_base + JPEG_VBIF_IN_RD_LIM_CONF1);
	writel_relaxed(0x10101010,
		jpeg_vbif_base + JPEG_VBIF_IN_RD_LIM_CONF2);
	writel_relaxed(0x10101010,
		jpeg_vbif_base + JPEG_VBIF_IN_WR_LIM_CONF0);
	writel_relaxed(0x10101010,
		jpeg_vbif_base + JPEG_VBIF_IN_WR_LIM_CONF1);
	writel_relaxed(0x10101010,
		jpeg_vbif_base + JPEG_VBIF_IN_WR_LIM_CONF2);
	writel_relaxed(0x00001010,
		jpeg_vbif_base + JPEG_VBIF_OUT_RD_LIM_CONF0);
	writel_relaxed(0x00000110,
		jpeg_vbif_base + JPEG_VBIF_OUT_WR_LIM_CONF0);
	writel_relaxed(0x00000707,
		jpeg_vbif_base + JPEG_VBIF_DDR_OUT_MAX_BURST);
	writel_relaxed(0x7,
		jpeg_vbif_base + JPEG_VBIF_OCMEM_OUT_MAX_BURST);
	writel_relaxed(0x00000030,
		jpeg_vbif_base + JPEG_VBIF_ARB_CTL);
	writel_relaxed(0x00000FFF,
		jpeg_vbif_base + JPEG_VBIF_OUT_AXI_AOOO_EN);
	writel_relaxed(0x0FFF0FFF,
		jpeg_vbif_base + JPEG_VBIF_OUT_AXI_AOOO);
	/*FE and WE QOS configuration need to be set when
	QOS RR arbitration is enabled*/
	if (pgmn_dev->hw_version != JPEG_8974_V1)
		writel_relaxed(0x00000003,
				jpeg_vbif_base + JPEG_VBIF_ROUND_ROBIN_QOS_ARB);
	else
		writel_relaxed(0x00000001,
				jpeg_vbif_base + JPEG_VBIF_ROUND_ROBIN_QOS_ARB);

	writel_relaxed(0x22222222,
		jpeg_vbif_base + JPEG_VBIF_OUT_AXI_AMEMTYPE_CONF0);
	writel_relaxed(0x2222,
		jpeg_vbif_base + JPEG_VBIF_OUT_AXI_AMEMTYPE_CONF1);
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

#ifdef CONFIG_MSM_IOMMU
static int msm_jpeg_attach_iommu(struct msm_jpeg_device *pgmn_dev)
{
	int i;

	for (i = 0; i < pgmn_dev->iommu_cnt; i++) {
		int rc = iommu_attach_device(pgmn_dev->domain,
				pgmn_dev->iommu_ctx_arr[i]);
		if (rc < 0) {
			JPEG_PR_ERR("%s: Device attach failed\n", __func__);
			return -ENODEV;
		}
		JPEG_DBG("%s:%d] dom 0x%lx ctx 0x%lx", __func__, __LINE__,
				(unsigned long)pgmn_dev->domain,
				(unsigned long)pgmn_dev->iommu_ctx_arr[i]);
	}
	return 0;
}
static int msm_jpeg_detach_iommu(struct msm_jpeg_device *pgmn_dev)
{
	int i;

	for (i = 0; i < pgmn_dev->iommu_cnt; i++) {
		JPEG_DBG("%s:%d] dom 0x%lx ctx 0x%lx", __func__, __LINE__,
				(unsigned long)pgmn_dev->domain,
				(unsigned long)pgmn_dev->iommu_ctx_arr[i]);
		iommu_detach_device(pgmn_dev->domain,
				pgmn_dev->iommu_ctx_arr[i]);
	}
	return 0;
}
#else
static int msm_jpeg_attach_iommu(struct msm_jpeg_device *pgmn_dev)
{
	return 0;
}
static int msm_jpeg_detach_iommu(struct msm_jpeg_device *pgmn_dev)
{
	return 0;
}
#endif



int msm_jpeg_platform_init(struct platform_device *pdev,
	struct resource **mem,
	void **base,
	int *irq,
	irqreturn_t (*handler) (int, void *),
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
	JPEG_DBG("%s base address: 0x%x, jpeg irq number: %d\n", __func__,
		jpeg_mem->start, jpeg_irq);

	pgmn_dev->jpeg_bus_client =
		msm_bus_scale_register_client(&msm_jpeg_bus_client_pdata);
	if (!pgmn_dev->jpeg_bus_client) {
		JPEG_PR_ERR("%s: Registration Failed!\n", __func__);
		pgmn_dev->jpeg_bus_client = 0;
		return -EINVAL;
	}
	msm_bus_scale_client_update_request(
		pgmn_dev->jpeg_bus_client, 1);

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

	pgmn_dev->jpeg_fs = regulator_get(&pgmn_dev->pdev->dev, "vdd");
	rc = regulator_enable(pgmn_dev->jpeg_fs);
	if (rc) {
		JPEG_PR_ERR("%s:%d]jpeg regulator get failed\n",
				__func__, __LINE__);
		goto fail_fs;
	}

	if (msm_jpeg_get_clk_info(pgmn_dev, pgmn_dev->pdev) < 0) {
		JPEG_PR_ERR("%s:%d]jpeg clock get failed\n",
				__func__, __LINE__);
		goto fail_fs;
	}

	rc = msm_cam_clk_enable(&pgmn_dev->pdev->dev, jpeg_8x_clk_info,
	 pgmn_dev->jpeg_clk, pgmn_dev->num_clk, 1);
	if (rc < 0) {
		JPEG_PR_ERR("%s: clk failed rc = %d\n", __func__, rc);
		goto fail_clk;
	}

	pgmn_dev->hw_version = readl_relaxed(jpeg_base +
		JPEG_HW_VERSION);
	JPEG_DBG_HIGH("%s:%d] jpeg HW version 0x%x", __func__, __LINE__,
		pgmn_dev->hw_version);

	pgmn_dev->jpeg_vbif = ioremap(vbif_mem->start, resource_size(vbif_mem));
	if (!pgmn_dev->jpeg_vbif) {
		rc = -ENOMEM;
		JPEG_PR_ERR("%s: ioremap failed\n", __func__);
		goto fail_vbif;
	}

	JPEG_DBG("%s:%d] jpeg_vbif 0x%x", __func__, __LINE__,
		(uint32_t)pgmn_dev->jpeg_vbif);

	rc = msm_jpeg_attach_iommu(pgmn_dev);
	if (rc < 0)
		goto fail_iommu;

	set_vbif_params(pgmn_dev, pgmn_dev->jpeg_vbif);

	rc = request_irq(jpeg_irq, handler, IRQF_TRIGGER_RISING, "jpeg",
		context);
	if (rc) {
		JPEG_PR_ERR("%s: request_irq failed, %d\n", __func__,
			jpeg_irq);
		goto fail_request_irq;
	}

	*mem  = jpeg_mem;
	*base = jpeg_base;
	*irq  = jpeg_irq;

	pgmn_dev->jpeg_client = msm_ion_client_create(-1, "camera/jpeg");
	JPEG_DBG("%s:%d] success\n", __func__, __LINE__);

	pgmn_dev->state = MSM_JPEG_INIT;
	return rc;

fail_request_irq:
	msm_jpeg_detach_iommu(pgmn_dev);

fail_iommu:
	iounmap(pgmn_dev->jpeg_vbif);


fail_vbif:
	msm_cam_clk_enable(&pgmn_dev->pdev->dev, jpeg_8x_clk_info,
	pgmn_dev->jpeg_clk, pgmn_dev->num_clk, 0);

fail_clk:
	rc = regulator_disable(pgmn_dev->jpeg_fs);
	if (!rc)
		regulator_put(pgmn_dev->jpeg_fs);
	else
		JPEG_PR_ERR("%s:%d] regulator disable failed %d",
			__func__, __LINE__, rc);
	pgmn_dev->jpeg_fs = NULL;

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
		msm_bus_scale_client_update_request(
			pgmn_dev->jpeg_bus_client, 0);
		msm_bus_scale_unregister_client(pgmn_dev->jpeg_bus_client);
	}

	msm_cam_clk_enable(&pgmn_dev->pdev->dev, jpeg_8x_clk_info,
	pgmn_dev->jpeg_clk, pgmn_dev->num_clk, 0);
	JPEG_DBG("%s:%d] clock disbale done", __func__, __LINE__);

	if (pgmn_dev->jpeg_fs) {
		result = regulator_disable(pgmn_dev->jpeg_fs);
		if (!result)
			regulator_put(pgmn_dev->jpeg_fs);
		else
			JPEG_PR_ERR("%s:%d] regulator disable failed %d",
				__func__, __LINE__, result);
		pgmn_dev->jpeg_fs = NULL;
	}
	iounmap(pgmn_dev->jpeg_vbif);
	iounmap(base);
	release_mem_region(mem->start, resource_size(mem));
	ion_client_destroy(pgmn_dev->jpeg_client);
	pgmn_dev->state = MSM_JPEG_IDLE;
	JPEG_DBG("%s:%d] success\n", __func__, __LINE__);
	return result;
}

