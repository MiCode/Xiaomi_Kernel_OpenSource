// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>

#include "mdw_cmn.h"

struct dentry *mdw_dbg_root;
struct dentry *mdw_dbg_trace;
struct dentry *mdw_dbg_device;

u32 g_mdw_klog;
u8 cfg_apusys_trace;

//----------------------------------------------
int mdw_dbg_init(struct apusys_core_info *info)
{
	g_mdw_klog = 0;

	/* create debug root */
	mdw_dbg_root = debugfs_create_dir("midware", info->dbg_root);

	/* create log level */
	debugfs_create_u32("klog", 0644,
		mdw_dbg_root, &g_mdw_klog);

	/* create log level */
	cfg_apusys_trace = 0;
	debugfs_create_u8("trace_en", 0644,
		mdw_dbg_root, &cfg_apusys_trace);

	mdw_dbg_device = debugfs_create_dir("device", mdw_dbg_root);

	return 0;
}

void mdw_dbg_deinit(void)
{
	/* remove by core */
	//debugfs_remove_recursive(mdw_dbg_root);
}
