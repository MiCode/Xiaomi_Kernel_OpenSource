/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Gene Chen <gene_chen@richtek.com>
 */

#ifndef _GENERIC_DEBUGFS_H
#define _GENERIC_DEBUGFS_H

#include <linux/mutex.h>

struct dbg_internal {
	struct dentry *rt_root;
	struct dentry *ic_root;
	bool rt_dir_create;
	struct mutex io_lock;
	u16 reg;
	u16 size;
	u16 data_buffer_size;
	void *data_buffer;
	bool access_lock;
};

struct dbg_info {
	const char *dirname;
	const char *devname;
	const char *typestr;
	void *io_drvdata;
	int (*io_read)(void *drvdata, u16 reg, void *val, u16 size);
	int (*io_write)(void *drvdata, u16 reg, const void *val, u16 size);
	struct dbg_internal internal;
};

#if IS_ENABLED(CONFIG_MTK_SUBPMIC_DEBUGFS)
extern int generic_debugfs_init(struct dbg_info *di);
extern void generic_debugfs_exit(struct dbg_info *di);
#else
static inline int generic_debugfs_init(struct dbg_info *di)
{
	return 0;
}

static inline void generic_debugfs_exit(struct dbg_info *di) {}
#endif /* CONFIG_DEBUG_FS */

#endif /* _GENERIC_DEBUGFS_H */
