/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include "sde_rotator_r3_debug.h"
#include "sde_rotator_core.h"
#include "sde_rotator_r3.h"
#include "sde_rotator_r3_internal.h"

/*
 * sde_rotator_r3_create_debugfs - Setup rotator r3 debugfs directory structure.
 * @rot_dev: Pointer to rotator device
 */
int sde_rotator_r3_create_debugfs(struct sde_rot_mgr *mgr,
		struct dentry *debugfs_root)
{
	struct sde_hw_rotator *hw_data;

	if (!mgr || !debugfs_root || !mgr->hw_data)
		return -EINVAL;

	hw_data = mgr->hw_data;

	if (!debugfs_create_bool("dbgmem", 0644,
			debugfs_root, &hw_data->dbgmem)) {
		SDEROT_ERR("fail create dbgmem\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("koff_timeout", 0644,
			debugfs_root, &hw_data->koff_timeout)) {
		SDEROT_ERR("fail create koff_timeout\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("vid_trigger", 0644,
			debugfs_root, &hw_data->vid_trigger)) {
		SDEROT_ERR("fail create vid_trigger\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("cmd_trigger", 0644,
			debugfs_root, &hw_data->cmd_trigger)) {
		SDEROT_ERR("fail create cmd_trigger\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("sbuf_headroom", 0644,
			debugfs_root, &hw_data->sbuf_headroom)) {
		SDEROT_ERR("fail create sbuf_headroom\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("solid_fill", 0644,
			debugfs_root, &hw_data->solid_fill)) {
		SDEROT_ERR("fail create solid_fill\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("constant_color", 0644,
			debugfs_root, &hw_data->constant_color)) {
		SDEROT_ERR("fail create constant_color\n");
		return -EINVAL;
	}

	return 0;
}
