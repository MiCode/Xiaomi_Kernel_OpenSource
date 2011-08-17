/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/pm_qos_params.h>
#include <linux/clk.h>
#include <mach/clk.h>
#include <linux/io.h>
#include <linux/android_pmem.h>
#include <mach/msm_reqs.h>
#include <mach/camera.h>

#include "msm_gemini_platform.h"
#include "msm_gemini_common.h"
#include "msm_gemini_hw.h"

#ifdef CONFIG_MSM_NPA_SYSTEM_BUS
/* NPA Flow ID */
#define MSM_SYSTEM_BUS_RATE	MSM_AXI_FLOW_JPEG_12MP
#else
/* AXI rate in KHz */
#define MSM_SYSTEM_BUS_RATE	160000
#endif

void msm_gemini_platform_p2v(struct file  *file)
{
#ifdef CONFIG_ANDROID_PMEM
	put_pmem_file(file);
#endif
}

uint32_t msm_gemini_platform_v2p(int fd, uint32_t len, struct file **file_p)
{
	unsigned long paddr;
	unsigned long size;
	int rc;

#ifdef CONFIG_ANDROID_PMEM
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
		return 0;
	}

	/* validate user input */
	if (len > size) {
		GMN_PR_ERR("%s: invalid offset + len\n", __func__);
		return 0;
	}

	return paddr;
}

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

	rc = msm_camio_jpeg_clk_enable();
	if (rc) {
		GMN_PR_ERR("%s: clk failed rc = %d\n", __func__, rc);
		goto fail2;
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
	GMN_DBG("%s:%d] success\n", __func__, __LINE__);

	return rc;

fail3:
	msm_camio_jpeg_clk_disable();
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
	int result;

	free_irq(irq, context);
	result = msm_camio_jpeg_clk_disable();
	iounmap(base);
	release_mem_region(mem->start, resource_size(mem));

	GMN_DBG("%s:%d] success\n", __func__, __LINE__);
	return result;
}

