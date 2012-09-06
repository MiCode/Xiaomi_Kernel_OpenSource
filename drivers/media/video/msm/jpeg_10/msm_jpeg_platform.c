/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <mach/clk.h>
#include <linux/io.h>
#include <linux/android_pmem.h>
#include <mach/camera.h>
#include <mach/iommu_domains.h>

#include "msm_jpeg_platform.h"
#include "msm_jpeg_sync.h"
#include "msm_jpeg_common.h"
#include "msm_jpeg_hw.h"

/* AXI rate in KHz */
struct ion_client *jpeg_client;
static void *jpeg_vbif;

void msm_jpeg_platform_p2v(struct file  *file,
				struct ion_handle **ionhandle, int domain_num)
{
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_unmap_iommu(jpeg_client, *ionhandle, domain_num, 0);
	ion_free(jpeg_client, *ionhandle);
	*ionhandle = NULL;
#elif CONFIG_ANDROID_PMEM
	put_pmem_file(file);
#endif
}

uint32_t msm_jpeg_platform_v2p(int fd, uint32_t len, struct file **file_p,
				struct ion_handle **ionhandle, int domain_num)
{
	unsigned long paddr;
	unsigned long size;
	int rc;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	*ionhandle = ion_import_dma_buf(jpeg_client, fd);
	if (IS_ERR_OR_NULL(*ionhandle))
		return 0;

	rc = ion_map_iommu(jpeg_client, *ionhandle, domain_num, 0,
			 SZ_4K, 0, &paddr, (unsigned long *)&size, UNCACHED, 0);
	JPEG_DBG("%s:%d] addr 0x%x size %ld", __func__, __LINE__,
							(uint32_t)paddr, size);

#elif CONFIG_ANDROID_PMEM
	unsigned long kvstart;
	rc = get_pmem_file(fd, &paddr, &kvstart, &size, file_p);
#else
	rc = 0;
	paddr = 0;
	size = 0;
#endif
	if (rc < 0) {
		JPEG_PR_ERR("%s: get_pmem_file fd %d error %d\n", __func__, fd,
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
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_free(jpeg_client, *ionhandle);
#endif
	return 0;
}

static struct msm_cam_clk_info jpeg_8x_clk_info[] = {
	{"core_clk", 228570000},
	{"iface_clk", -1},
	{"bus_clk0", -1},
	{"alt_bus_clk", -1},
	{"camss_top_ahb_clk", -1},
};

static void set_vbif_params(void *jpeg_vbif_base)
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
	writel_relaxed(0x00001010,
		jpeg_vbif_base + JPEG_VBIF_OUT_WR_LIM_CONF0);
	writel_relaxed(0x00000707,
		jpeg_vbif_base + JPEG_VBIF_DDR_OUT_MAX_BURST);
	writel_relaxed(0x00000707,
		jpeg_vbif_base + JPEG_VBIF_OCMEM_OUT_MAX_BURST);
}


int msm_jpeg_platform_init(struct platform_device *pdev,
	struct resource **mem,
	void **base,
	int *irq,
	irqreturn_t (*handler) (int, void *),
	void *context)
{
	int rc = -1;
	int i = 0;
	int jpeg_irq;
	struct resource *jpeg_mem, *jpeg_io, *jpeg_irq_res;
	void *jpeg_base;
	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *) context;

	jpeg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!jpeg_mem) {
		JPEG_PR_ERR("%s: no mem resource?\n", __func__);
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
		goto fail1;
	}

	jpeg_vbif = ioremap(VBIF_BASE_ADDRESS, VBIF_REGION_SIZE);
	if (!jpeg_vbif) {
		rc = -ENOMEM;
		JPEG_PR_ERR("%s:%d] ioremap failed\n", __func__, __LINE__);
		goto fail1;
	}
	JPEG_DBG("%s:%d] jpeg_vbif 0x%x", __func__, __LINE__,
						(uint32_t)jpeg_vbif);

	pgmn_dev->jpeg_fs = regulator_get(&pgmn_dev->pdev->dev, "vdd");
	rc = regulator_enable(pgmn_dev->jpeg_fs);
	if (rc) {
		JPEG_PR_ERR("%s:%d]jpeg regulator get failed\n",
				__func__, __LINE__); }

	pgmn_dev->hw_version = JPEG_8974;
	rc = msm_cam_clk_enable(&pgmn_dev->pdev->dev, jpeg_8x_clk_info,
	 pgmn_dev->jpeg_clk, ARRAY_SIZE(jpeg_8x_clk_info), 1);
	if (rc < 0) {
		JPEG_PR_ERR("%s: clk failed rc = %d\n", __func__, rc);
		goto fail2;
	}

#ifdef CONFIG_MSM_IOMMU
	for (i = 0; i < pgmn_dev->iommu_cnt; i++) {
		rc = iommu_attach_device(pgmn_dev->domain,
				pgmn_dev->iommu_ctx_arr[i]);
		if (rc < 0) {
			rc = -ENODEV;
			JPEG_PR_ERR("%s: Device attach failed\n", __func__);
			goto fail;
		}
		JPEG_DBG("%s:%d] dom 0x%x ctx 0x%x", __func__, __LINE__,
					(uint32_t)pgmn_dev->domain,
					(uint32_t)pgmn_dev->iommu_ctx_arr[i]);
	}
#endif
	set_vbif_params(jpeg_vbif);

	msm_jpeg_hw_init(jpeg_base, resource_size(jpeg_mem));
	rc = request_irq(jpeg_irq, handler, IRQF_TRIGGER_RISING, "jpeg",
		context);
	if (rc) {
		JPEG_PR_ERR("%s: request_irq failed, %d\n", __func__,
			jpeg_irq);
		goto fail3;
	}

	*mem  = jpeg_mem;
	*base = jpeg_base;
	*irq  = jpeg_irq;

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	jpeg_client = msm_ion_client_create(-1, "camera/jpeg");
#endif
	JPEG_DBG("%s:%d] success\n", __func__, __LINE__);

	return rc;

fail3:
	msm_cam_clk_enable(&pgmn_dev->pdev->dev, jpeg_8x_clk_info,
	pgmn_dev->jpeg_clk, ARRAY_SIZE(jpeg_8x_clk_info), 0);

	regulator_put(pgmn_dev->jpeg_fs);
	regulator_disable(pgmn_dev->jpeg_fs);
	pgmn_dev->jpeg_fs = NULL;
fail2:
	iounmap(jpeg_base);
fail1:
#ifdef CONFIG_MSM_IOMMU
	for (i = 0; i < pgmn_dev->iommu_cnt; i++) {
		JPEG_PR_ERR("%s:%d] dom 0x%x ctx 0x%x", __func__, __LINE__,
					(uint32_t)pgmn_dev->domain,
					(uint32_t)pgmn_dev->iommu_ctx_arr[i]);
		iommu_detach_device(pgmn_dev->domain,
					pgmn_dev->iommu_ctx_arr[i]);
	}
#endif
fail:
	release_mem_region(jpeg_mem->start, resource_size(jpeg_mem));
	JPEG_DBG("%s:%d] fail\n", __func__, __LINE__);
	return rc;
}

int msm_jpeg_platform_release(struct resource *mem, void *base, int irq,
	void *context)
{
	int result = 0;
	int i = 0;
	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *) context;

	free_irq(irq, context);

#ifdef CONFIG_MSM_IOMMU
	for (i = 0; i < pgmn_dev->iommu_cnt; i++) {
		iommu_detach_device(pgmn_dev->domain,
				pgmn_dev->iommu_ctx_arr[i]);
		JPEG_DBG("%s:%d]", __func__, __LINE__);
	}
#endif

	msm_cam_clk_enable(&pgmn_dev->pdev->dev, jpeg_8x_clk_info,
	pgmn_dev->jpeg_clk, ARRAY_SIZE(jpeg_8x_clk_info), 0);
	JPEG_DBG("%s:%d] clock disbale done", __func__, __LINE__);

	if (pgmn_dev->jpeg_fs) {
		regulator_put(pgmn_dev->jpeg_fs);
		regulator_disable(pgmn_dev->jpeg_fs);
		pgmn_dev->jpeg_fs = NULL;
	}
	iounmap(jpeg_vbif);
	iounmap(base);
	release_mem_region(mem->start, resource_size(mem));
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_client_destroy(jpeg_client);
#endif
	JPEG_DBG("%s:%d] success\n", __func__, __LINE__);
	return result;
}

