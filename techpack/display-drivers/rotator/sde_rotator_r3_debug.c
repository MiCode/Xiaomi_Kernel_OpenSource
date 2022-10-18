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

#include "sde_rotator_r3_debug.h"
#include "sde_rotator_core.h"
#include "sde_rotator_r3.h"
#include "sde_rotator_r3_internal.h"

#if defined(CONFIG_MSM_SDE_ROTATOR_EVTLOG_DEBUG) && \
	defined(CONFIG_DEBUG_FS)
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

	debugfs_create_u32("koff_timeout", 0644, debugfs_root, &hw_data->koff_timeout);

	debugfs_create_u32("vid_trigger", 0644, debugfs_root, &hw_data->vid_trigger);

	debugfs_create_u32("cmd_trigger", 0644, debugfs_root, &hw_data->cmd_trigger);

	debugfs_create_u32("sbuf_headroom", 0644, debugfs_root, &hw_data->sbuf_headroom);

	debugfs_create_u32("solid_fill", 0644, debugfs_root, &hw_data->solid_fill);

	return 0;
}
#endif
