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
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/android_pmem.h>
#include <mach/clk.h>
#include <mach/camera.h>
#include <mach/msm_subsystem_map.h>

#include "msm_mercury_platform.h"
#include "msm_mercury_sync.h"
#include "msm_mercury_common.h"
#include "msm_mercury_hw.h"


struct ion_client *mercury_client;

static struct msm_cam_clk_info mercury_jpegd_clk_info[] = {
	{"core_clk", 200000000},
	{"iface_clk", -1}
};

void msm_mercury_platform_p2v(struct file  *file,
	struct ion_handle **ionhandle)
{
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_unmap_iommu(mercury_client, *ionhandle, CAMERA_DOMAIN,
		GEN_POOL);
	ion_free(mercury_client, *ionhandle);
	*ionhandle = NULL;
#elif CONFIG_ANDROID_PMEM
	put_pmem_file(file);
#endif
}

uint32_t msm_mercury_platform_v2p(int fd, uint32_t len,
	struct file **file_p,
	struct ion_handle **ionhandle)
{
	unsigned long paddr;
	unsigned long size;
	int rc;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	*ionhandle = ion_import_dma_buf(mercury_client, fd);
	if (IS_ERR_OR_NULL(*ionhandle))
		return 0;

	rc = ion_map_iommu(mercury_client, *ionhandle, CAMERA_DOMAIN,
		GEN_POOL, SZ_4K, 0, &paddr,
		(unsigned long *)&size, 0, 0);
#elif CONFIG_ANDROID_PMEM
	unsigned long kvstart;
	rc = get_pmem_file(fd, &paddr, &kvstart, &size, file_p);
#else
	rc = 0;
	paddr = 0;
	size = 0;
#endif
	if (rc < 0) {
		MCR_PR_ERR("%s: get_pmem_file fd %d error %d\n", __func__, fd,
			rc);
		goto error1;
	}

	/* validate user input */
	if (len > size) {
		MCR_PR_ERR("%s: invalid offset + len\n", __func__);
		goto error1;
	}

	return paddr;
error1:
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_free(mercury_client, *ionhandle);
#endif
	return 0;
}

int msm_mercury_platform_init(struct platform_device *pdev,
	struct resource **mem,
	void **base,
	int *irq,
	irqreturn_t (*handler) (int, void *),
	void *context)
{
	int rc = 0;
	int mercury_irq;
	struct resource *mercury_mem, *mercury_io, *mercury_irq_res;
	void *mercury_base;
	struct msm_mercury_device *pmercury_dev =
		(struct msm_mercury_device *) context;

	MCR_DBG("%s:%d]\n", __func__, __LINE__);

	mercury_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mercury_mem) {
		MCR_PR_ERR("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}

	mercury_irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!mercury_irq_res) {
		MCR_PR_ERR("no irq resource?\n");
		return -ENODEV;
	}
	mercury_irq = mercury_irq_res->start;

	mercury_io = request_mem_region(mercury_mem->start,
		resource_size(mercury_mem), pdev->name);
	if (!mercury_io) {
		MCR_PR_ERR("%s: region already claimed\n", __func__);
		return -EBUSY;
	}
	MCR_DBG("%s:%d]\n", __func__, __LINE__);
	mercury_base = ioremap(mercury_mem->start,
		resource_size(mercury_mem));
	if (!mercury_base) {
		rc = -ENOMEM;
		MCR_PR_ERR("%s: ioremap failed\n", __func__);
		goto fail1;
	}
	MCR_DBG("%s:%d]\n", __func__, __LINE__);

	rc = msm_cam_clk_enable(&pmercury_dev->pdev->dev,
		mercury_jpegd_clk_info, pmercury_dev->mercury_clk,
		ARRAY_SIZE(mercury_jpegd_clk_info), 1);
	if (rc < 0)
		MCR_PR_ERR("%s:%d] rc = %d\n", __func__, __LINE__, rc);

	MCR_DBG("%s:%d]\n", __func__, __LINE__);
	msm_mercury_hw_init(mercury_base, resource_size(mercury_mem));
	rc = request_irq(mercury_irq, handler, IRQF_TRIGGER_RISING,
		"mercury", context);
	if (rc) {
		MCR_PR_ERR("%s: request_irq failed, %d\n", __func__,
			mercury_irq);
		goto fail3;
	}
	MCR_DBG("%s:%d]\n", __func__, __LINE__);
	*mem  = mercury_mem;
	*base = mercury_base;
	*irq  = mercury_irq;
	MCR_DBG("%s:%d]\n", __func__, __LINE__);
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	mercury_client = msm_ion_client_create(-1, "camera/mercury");
#endif
	MCR_PR_ERR("%s:%d] success\n", __func__, __LINE__);
	return rc;
fail3:
	MCR_DBG("%s:%d]\n", __func__, __LINE__);
	msm_cam_clk_enable(&pmercury_dev->pdev->dev, mercury_jpegd_clk_info,
		pmercury_dev->mercury_clk,
		ARRAY_SIZE(mercury_jpegd_clk_info), 0);
	MCR_DBG("%s:%d]\n", __func__, __LINE__);
	iounmap(mercury_base);
fail1:
	MCR_DBG("%s:%d]\n", __func__, __LINE__);
	release_mem_region(mercury_mem->start, resource_size(mercury_mem));
	MCR_DBG("%s:%d]\n", __func__, __LINE__);
	return rc;
}

int msm_mercury_platform_release(struct resource *mem, void *base,
	int irq, void *context)
{
	int result = 0;
	struct msm_mercury_device *pmercury_dev =
		(struct msm_mercury_device *) context;

	free_irq(irq, context);
	msm_cam_clk_enable(&pmercury_dev->pdev->dev, mercury_jpegd_clk_info,
		pmercury_dev->mercury_clk, ARRAY_SIZE(mercury_jpegd_clk_info),
		0);
	iounmap(base);
	release_mem_region(mem->start, resource_size(mem));
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	ion_client_destroy(mercury_client);
#endif
	MCR_DBG("%s:%d] success\n", __func__, __LINE__);
	return result;
}

