/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef __DRIVERS_CLK_RESET_H
#define __DRIVERS_CLK_RESET_H

#include <linux/platform_device.h>
#include <linux/reset-controller.h>

struct msm_reset_map {
	unsigned int reg;
	u8 bit;
};

struct msm_reset_controller {
	const struct msm_reset_map *reset_map;
	struct reset_controller_dev rcdev;
	void __iomem  *base;
};

#define to_msm_reset_controller(r) \
	container_of(r, struct msm_reset_controller, rcdev)

extern struct reset_control_ops msm_reset_ops;

int msm_reset_controller_register(struct platform_device *pdev,
		const struct msm_reset_map *map, unsigned int nr_resets,
		void __iomem *virt_base);
#endif
