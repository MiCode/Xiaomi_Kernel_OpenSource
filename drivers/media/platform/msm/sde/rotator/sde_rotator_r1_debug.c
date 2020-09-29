// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

