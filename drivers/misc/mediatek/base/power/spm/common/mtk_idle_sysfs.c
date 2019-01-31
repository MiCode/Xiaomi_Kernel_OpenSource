/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#include "mtk_idle_sysfs.h"

#ifndef __weak
#define __weak __attribute__((weak))
#endif

#define MTK_IDLE_SYS_FS_NAME	"cpuidle"
#define MTK_IDLE_SYS_FS_MODE	0644

int __weak mtk_idle_sysfs_entry_create_plat(const char *name
		, int mode, struct mtk_idle_sysfs_handle *parent
		, struct mtk_idle_sysfs_handle *handle)
{
	return 0;
}
int __weak mtk_idle_sysfs_entry_node_add_plat(const char *name
		, int mode, const struct mtk_idle_sysfs_op *op
		, struct mtk_idle_sysfs_handle *parent
		, struct mtk_idle_sysfs_handle *handle)
{
	return 0;
}

static struct mtk_idle_sysfs_handle mtk_idle_fs_root = {
	NULL
};

int mtk_idle_sysfs_entry_func_create(const char *name
		, int mode, struct mtk_idle_sysfs_handle *parent
		, struct mtk_idle_sysfs_handle *handle)
{
	return mtk_idle_sysfs_entry_create_plat(name, mode, parent, handle);
}

int mtk_idle_sysfs_entry_func_node_add(const char *name
		, int mode, const struct mtk_idle_sysfs_op *op
		, struct mtk_idle_sysfs_handle *parent
		, struct mtk_idle_sysfs_handle *node)
{
	return mtk_idle_sysfs_entry_node_add_plat(name
			, mode, op, parent, node);
}

int mtk_idle_sysfs_entry_create(void)
{
	int bRet = 0;

	if (!mtk_idle_fs_root._current) {
		bRet = mtk_idle_sysfs_entry_create_plat(
			MTK_IDLE_SYS_FS_NAME, MTK_IDLE_SYS_FS_MODE
			, NULL, &mtk_idle_fs_root);
	}
	return bRet;
}

int mtk_idle_sysfs_entry_node_add(const char *name
		, int mode, const struct mtk_idle_sysfs_op *op
		, struct mtk_idle_sysfs_handle *handle)
{
	if (!mtk_idle_fs_root._current)
		mtk_idle_sysfs_entry_create();

	return mtk_idle_sysfs_entry_node_add_plat(name
			, mode, op, &mtk_idle_fs_root, handle);
}

int mtk_idle_sysfs_entry_root_get(struct mtk_idle_sysfs_handle **handle)
{
	if (!handle || !mtk_idle_fs_root._current)
		return -1;
	*handle = &mtk_idle_fs_root;
	return 0;
}

