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

#ifndef MSM_JPEG_PLATFORM_H
#define MSM_JPEG_PLATFORM_H

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <mach/iommu.h>
#include "msm_jpeg_sync.h"

void msm_jpeg_platform_p2v(struct msm_jpeg_device *pgmn_dev, struct file *file,
	struct ion_handle **ionhandle, int domain_num);
uint32_t msm_jpeg_platform_v2p(struct msm_jpeg_device *pgmn_dev, int fd,
	uint32_t len, struct file **file, struct ion_handle **ionhandle,
	int domain_num);

int msm_jpeg_platform_clk_enable(void);
int msm_jpeg_platform_clk_disable(void);

int msm_jpeg_platform_init(struct platform_device *pdev,
	struct resource **mem,
	void **base,
	int *irq,
	irqreturn_t (*handler) (int, void *),
	void *context);
int msm_jpeg_platform_release(struct resource *mem, void *base, int irq,
	void *context);

#endif /* MSM_JPEG_PLATFORM_H */
