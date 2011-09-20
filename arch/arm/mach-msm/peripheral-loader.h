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
#ifndef __MSM_PERIPHERAL_LOADER_H
#define __MSM_PERIPHERAL_LOADER_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

struct pil_device {
	const char *name;
	const char *depends_on;
	int count;
	struct mutex lock;
	struct platform_device pdev;
	struct list_head list;
	struct pil_reset_ops *ops;
};

struct pil_reset_ops {
	int (*init_image)(struct pil_device *pil, const u8 *metadata,
			  size_t size);
	int (*verify_blob)(struct pil_device *pil, u32 phy_addr, size_t size);
	int (*auth_and_reset)(struct pil_device *pil);
	int (*shutdown)(struct pil_device *pil);
};

extern int msm_pil_add_device(struct pil_device *pil);

#endif
