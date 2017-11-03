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

#include "sde_rotator_r1_debug.h"
#include "sde_rotator_core.h"
#include "sde_rotator_r1.h"
#include "sde_rotator_r1_internal.h"

/*
 * sde_rotator_r1_create_debugfs - Setup rotator r1 debugfs directory structure.
 * @rot_dev: Pointer to rotator device
 */
int sde_rotator_r1_create_debugfs(struct sde_rot_mgr *mgr,
		struct dentry *debugfs_root)
{
	struct sde_rotator_r1_data *hw_data;

	if (!mgr || !debugfs_root || !mgr->hw_data)
		return -EINVAL;

	hw_data = mgr->hw_data;

	/* add debugfs */

	return 0;
}

