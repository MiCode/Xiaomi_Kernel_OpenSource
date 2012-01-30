/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#include "msm_gemini_platform.h"
#include "msm_gemini_sync.h"
#include "msm_gemini_common.h"
#include "msm_gemini_hw.h"

/* AXI rate in KHz */
#define MSM_SYSTEM_BUS_RATE	160000
struct ion_client *gemini_client;

void msm_gemini_platform_p2v(struct file  *file,
				struct ion_handle **ionhandle)
{
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_unmap_iommu(gemini_client, *ionhandle, CAMERA_DOMAIN, GEN_POOL);
	ion_free(gemini_client, *ionhandle);
	*ionhandle = NULL;
#elif CONFIG_ANDROID_PMEM
	put_pmem_file(file);
#endif
}

uint32_t msm_gemini_platform_v2p(int fd, uint32_t len, struct file **file_p,
				struct ion_handle **ionhandle)
{
	unsigned long paddr;
	unsigned long size;
	int rc;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	*ionhandle = ion_import_dma_buf(gemini_client, fd);
	if (IS_ERR_OR_NULL(*ionhandle))
		return 0;

	rc = ion_map_iommu(gemini_client, *ionhandle, CAMERA_DOMAIN, GEN_POOL,
			SZ_4K, 0, &paddr, (unsigned long *)&size, UNCACHED, 0);
#elif CONFIG_ANDROID_PMEM
	unsigned long kvstart;
	rc = get_pmem_file(fd, &paddr, &kvstart, &size, file_p);
#else
	rc = 0;
	paddr = 0;
	size = 0;
#endif
	if (rc < 0) {
		GMN_PR_ERR("%s: get_pmem_file fd %d error %d\n", __func__, fd,
			rc);
		goto error1;
	}

	/* validate user input */
	if (len > size) {
		GMN_PR_ERR("%s: invalid offset + len\n", __func__);
		goto error1;
	}

	return paddr;
error1:
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_free(gemini_client, *ionhandle);
#endif
	return 0;
}

static struct msm_cam_clk_info gemini_8x_clk_info[] = {
	{"core_clk", 228571000},
	{"iface_clk", -1},
};

static struct msm_cam_clk_info gemini_7x_clk_info[] = {
	{"core_clk", 153600000},
	{"iface_clk", -1},
};

static struct msm_cam_clk_info gemini_imem_clk_info[] = {
	{"mem_clk", -1},
};

int msm_gemini_platform_init(struct platform_device *pdev,
	struct resource **mem,
	void **base,
	int *irq,
	irqreturn_t (*handler) (int, void *),
	void *context)
{
	int rc = -1;
	int gemini_irq;
	struct resource *gemini_mem, *gemini_io, *gemini_irq_res;
	void *gemini_base;
	struct msm_gemini_device *pgmn_dev =
		(struct msm_gemini_device *) context;

	gemini_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!gemini_mem) {
		GMN_PR_ERR("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}

	gemini_irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!gemini_irq_res) {
		GMN_PR_ERR("no irq resource?\n");
		return -ENODEV;
	}
	gemini_irq = gemini_irq_res->start;

	gemini_io = request_mem_region(gemini_mem->start,
		resource_size(gemini_mem), pdev->name);
	if (!gemini_io) {
		GMN_PR_ERR("%s: region already claimed\n", __func__);
		return -EBUSY;
	}

	gemini_base = ioremap(gemini_mem->start, resource_size(gemini_mem));
	if (!gemini_base) {
		rc = -ENOMEM;
		GMN_PR_ERR("%s: ioremap failed\n", __func__);
		goto fail1;
	}

	pgmn_dev->hw_version = GEMINI_8X60;
	rc = msm_cam_clk_enable(&pgmn_dev->pdev->dev, gemini_8x_clk_info,
	 pgmn_dev->gemini_clk, ARRAY_SIZE(gemini_8x_clk_info), 1);
	if (rc < 0) {
		pgmn_dev->hw_version = GEMINI_7X;
		rc = msm_cam_clk_enable(&pgmn_dev->pdev->dev,
			gemini_7x_clk_info, pgmn_dev->gemini_clk,
			ARRAY_SIZE(gemini_7x_clk_info), 1);
		if (rc < 0) {
			GMN_PR_ERR("%s: clk failed rc = %d\n", __func__, rc);
			goto fail2;
		}
	} else {
		rc = msm_cam_clk_enable(&pgmn_dev->pdev->dev,
				gemini_imem_clk_info, &pgmn_dev->gemini_clk[2],
				ARRAY_SIZE(gemini_imem_clk_info), 1);
		if (!rc)
			pgmn_dev->hw_version = GEMINI_8960;
	}

	if (pgmn_dev->hw_version != GEMINI_7X) {
		if (pgmn_dev->gemini_fs == NULL) {
			pgmn_dev->gemini_fs =
				regulator_get(&pgmn_dev->pdev->dev, "vdd");
			if (IS_ERR(pgmn_dev->gemini_fs)) {
				pr_err("%s: Regulator FS_ijpeg get failed %ld\n",
					__func__, PTR_ERR(pgmn_dev->gemini_fs));
				pgmn_dev->gemini_fs = NULL;
				goto gemini_fs_failed;
			} else if (regulator_enable(pgmn_dev->gemini_fs)) {
				pr_err("%s: Regulator FS_ijpeg enable failed\n",
								__func__);
				regulator_put(pgmn_dev->gemini_fs);
				pgmn_dev->gemini_fs = NULL;
				goto gemini_fs_failed;
			}
		}
	}

	msm_gemini_hw_init(gemini_base, resource_size(gemini_mem));
	rc = request_irq(gemini_irq, handler, IRQF_TRIGGER_RISING, "gemini",
		context);
	if (rc) {
		GMN_PR_ERR("%s: request_irq failed, %d\n", __func__,
			gemini_irq);
		goto fail3;
	}

	*mem  = gemini_mem;
	*base = gemini_base;
	*irq  = gemini_irq;

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	gemini_client = msm_ion_client_create(-1, "camera/gemini");
#endif
	GMN_DBG("%s:%d] success\n", __func__, __LINE__);

	return rc;

fail3:
	if (pgmn_dev->hw_version != GEMINI_7X) {
		regulator_disable(pgmn_dev->gemini_fs);
		regulator_put(pgmn_dev->gemini_fs);
		pgmn_dev->gemini_fs = NULL;
	}
gemini_fs_failed:
	if (pgmn_dev->hw_version == GEMINI_8960)
		msm_cam_clk_enable(&pgmn_dev->pdev->dev, gemini_imem_clk_info,
		 &pgmn_dev->gemini_clk[2], ARRAY_SIZE(gemini_imem_clk_info), 0);
	if (pgmn_dev->hw_version != GEMINI_7X)
		msm_cam_clk_enable(&pgmn_dev->pdev->dev, gemini_8x_clk_info,
		pgmn_dev->gemini_clk, ARRAY_SIZE(gemini_8x_clk_info), 0);
	else
		msm_cam_clk_enable(&pgmn_dev->pdev->dev, gemini_7x_clk_info,
		pgmn_dev->gemini_clk, ARRAY_SIZE(gemini_7x_clk_info), 0);
fail2:
	iounmap(gemini_base);
fail1:
	release_mem_region(gemini_mem->start, resource_size(gemini_mem));
	GMN_DBG("%s:%d] fail\n", __func__, __LINE__);
	return rc;
}

int msm_gemini_platform_release(struct resource *mem, void *base, int irq,
	void *context)
{
	int result = 0;
	struct msm_gemini_device *pgmn_dev =
		(struct msm_gemini_device *) context;

	free_irq(irq, context);

	if (pgmn_dev->hw_version != GEMINI_7X) {
		regulator_disable(pgmn_dev->gemini_fs);
		regulator_put(pgmn_dev->gemini_fs);
		pgmn_dev->gemini_fs = NULL;
	}

	if (pgmn_dev->hw_version == GEMINI_8960)
		msm_cam_clk_enable(&pgmn_dev->pdev->dev, gemini_imem_clk_info,
		 &pgmn_dev->gemini_clk[2], ARRAY_SIZE(gemini_imem_clk_info), 0);
	if (pgmn_dev->hw_version != GEMINI_7X)
		msm_cam_clk_enable(&pgmn_dev->pdev->dev, gemini_8x_clk_info,
		pgmn_dev->gemini_clk, ARRAY_SIZE(gemini_8x_clk_info), 0);
	else
		msm_cam_clk_enable(&pgmn_dev->pdev->dev, gemini_7x_clk_info,
		pgmn_dev->gemini_clk, ARRAY_SIZE(gemini_7x_clk_info), 0);

	iounmap(base);
	release_mem_region(mem->start, resource_size(mem));
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_client_destroy(gemini_client);
#endif
	GMN_DBG("%s:%d] success\n", __func__, __LINE__);
	return result;
}

