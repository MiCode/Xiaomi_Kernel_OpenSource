/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_SWPM_SYSFS__
#define __MTK_SWPM_SYSFS__

#include "mtk_swpm_common_sysfs.h"
#include "mtk_swpm_common_kernfs.h"

/*Get the mtk swpm fs root entry handle*/
int mtk_swpm_sysfs_entry_root_get(struct mtk_swpm_sysfs_handle **handle);

/*Creat the entry for mtk swpm fs*/
int mtk_swpm_sysfs_entry_group_add(const char *name
		, int mode, struct mtk_swpm_sysfs_group *_group
		, struct mtk_swpm_sysfs_handle *handle);

/*Add the child file node to mtk swpm */
int mtk_swpm_sysfs_entry_node_add(const char *name, int mode
			, const struct mtk_swpm_sysfs_op *op
			, struct mtk_swpm_sysfs_handle *node);

int mtk_swpm_sysfs_entry_node_remove(
		struct mtk_swpm_sysfs_handle *handle);

int mtk_swpm_sysfs_root_entry_create(void);

int mtk_swpm_sysfs_sub_entry_add(const char *name, int mode,
				struct mtk_swpm_sysfs_handle *parent,
				struct mtk_swpm_sysfs_handle *handle);

int mtk_swpm_sysfs_sub_entry_node_add(const char *name
		, int mode, const struct mtk_swpm_sysfs_op *op
		, struct mtk_swpm_sysfs_handle *parent
		, struct mtk_swpm_sysfs_handle *handle);

int mtk_swpm_sysfs_remove(void);

#endif
