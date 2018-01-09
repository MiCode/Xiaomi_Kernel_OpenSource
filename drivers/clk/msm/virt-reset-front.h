/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef __VIRT_RESET_FRONT_H
#define __VIRT_RESET_FRONT_H

#include <linux/platform_device.h>
#include <linux/reset-controller.h>

struct virt_reset_map {
	const char *clk_name;
	int clk_id;
};

struct virtrc_front {
	struct virt_reset_map *reset_map;
	struct reset_controller_dev rcdev;
};

#define to_virtrc_front(r) \
	container_of(r, struct virtrc_front, rcdev)

extern struct reset_control_ops virtrc_front_ops;

int msm_virtrc_front_register(struct platform_device *pdev,
		struct virt_reset_map *map, unsigned int nr_resets);
#endif
