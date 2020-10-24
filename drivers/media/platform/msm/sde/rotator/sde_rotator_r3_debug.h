/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_ROTATOR_R3_DEBUG_H__
#define __SDE_ROTATOR_R3_DEBUG_H__

#include <linux/types.h>
#include <linux/dcache.h>

struct sde_rot_mgr;

#if defined(CONFIG_DEBUG_FS)
int sde_rotator_r3_create_debugfs(struct sde_rot_mgr *mgr,
		struct dentry *debugfs_root);
#else
static inline
int sde_rotator_r3_create_debugfs(struct sde_rot_mgr *mgr,
		struct dentry *debugfs_root)
{
	return 0;
}
#endif
#endif /* __SDE_ROTATOR_R3_DEBUG_H__ */
