/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CPUIDLE_SYSFS__
#define __MTK_CPUIDLE_SYSFS__

#include "mtk_lp_sysfs.h"
#include "mtk_lp_kernfs.h"

#define MTK_CPUIDLE_SYS_FS_MODE      0644

#define CPUIDLE_CONTROL_NODE_INIT(_n, _r, _w) ({\
		_n.op.fs_read = _r;\
		_n.op.fs_write = _w;\
		_n.op.priv = &_n; })

#undef mtk_dbg_cpuidle_log
#define mtk_dbg_cpuidle_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

struct MTK_CPUIDLE_NODE {
	const char *name;
	int type;
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};

/*Get the mtk idle system fs root entry handle*/
int mtk_cpuidle_sysfs_entry_root_get(struct mtk_lp_sysfs_handle **handle);

/*Creat the entry for mtk idle systme fs*/
int mtk_cpuidle_sysfs_entry_group_add(const char *name,
		int mode, struct mtk_lp_sysfs_group *_group,
		struct mtk_lp_sysfs_handle *handle);

/*Add the child file node to mtk idle system*/
int mtk_cpuidle_sysfs_entry_node_add(const char *name, int mode,
			const struct mtk_lp_sysfs_op *op,
			struct mtk_lp_sysfs_handle *node);

int mtk_cpuidle_sysfs_entry_node_remove(
		struct mtk_lp_sysfs_handle *handle);

int mtk_cpuidle_sysfs_root_entry_create(void);

int mtk_cpuidle_sysfs_remove(void);

int mtk_cpuidle_sysfs_sub_entry_add(const char *name, int mode,
					struct mtk_lp_sysfs_handle *parent,
					struct mtk_lp_sysfs_handle *handle);

int mtk_cpuidle_sysfs_sub_entry_node_add(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle);

#endif
