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

#ifndef MSM_GEMINI_PLATFORM_H
#define MSM_GEMINI_PLATFORM_H

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/ion.h>
#include <linux/iommu.h>
void msm_gemini_platform_p2v(struct file  *file,
				struct ion_handle **ionhandle);
uint32_t msm_gemini_platform_v2p(int fd, uint32_t len, struct file **file,
				struct ion_handle **ionhandle);

int msm_gemini_platform_clk_enable(void);
int msm_gemini_platform_clk_disable(void);

int msm_gemini_platform_init(struct platform_device *pdev,
	struct resource **mem,
	void **base,
	int *irq,
	irqreturn_t (*handler) (int, void *),
	void *context);
int msm_gemini_platform_release(struct resource *mem, void *base, int irq,
	void *context);

#endif /* MSM_GEMINI_PLATFORM_H */
