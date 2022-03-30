// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include "mtk_swpm_sysfs.h"

#define MTK_SWPM_SYS_FS_NAME	"others"
#define MTK_SWPM_SYS_FS_MODE	0644

static struct mtk_swpm_sysfs_handle mtk_swpm_fs_root;

int mtk_swpm_sysfs_entry_node_add(const char *name
		, int mode, const struct mtk_swpm_sysfs_op *op
		, struct mtk_swpm_sysfs_handle *handle)
{
	if (!IS_MTK_SWPM_SYS_HANDLE_VALID(&mtk_swpm_fs_root))
		mtk_swpm_sysfs_root_entry_create();

	return mtk_swpm_sysfs_entry_func_node_add(name
			, mode, op, &mtk_swpm_fs_root, handle);
}
EXPORT_SYMBOL(mtk_swpm_sysfs_entry_node_add);

int mtk_swpm_sysfs_entry_node_remove(
		struct mtk_swpm_sysfs_handle *handle)
{
	return mtk_swpm_sysfs_entry_func_node_remove(handle);
}
EXPORT_SYMBOL(mtk_swpm_sysfs_entry_node_remove);

int mtk_swpm_sysfs_root_entry_create(void)
{
	int bRet = -EACCES;

	if (!IS_MTK_SWPM_SYS_HANDLE_VALID(&mtk_swpm_fs_root)) {
		bRet = mtk_swpm_sysfs_entry_func_create(
			MTK_SWPM_SYS_FS_NAME, MTK_SWPM_SYS_FS_MODE
			, NULL, &mtk_swpm_fs_root);
	}
	return bRet;
}
EXPORT_SYMBOL(mtk_swpm_sysfs_root_entry_create);

int mtk_swpm_sysfs_entry_root_get(struct mtk_swpm_sysfs_handle **handle)
{
	if (!handle ||
		!IS_MTK_SWPM_SYS_HANDLE_VALID(&mtk_swpm_fs_root)
	)
		return -1;
	*handle = &mtk_swpm_fs_root;
	return 0;
}
EXPORT_SYMBOL(mtk_swpm_sysfs_entry_root_get);

int mtk_swpm_sysfs_sub_entry_add(const char *name, int mode,
					struct mtk_swpm_sysfs_handle *parent,
					struct mtk_swpm_sysfs_handle *handle)
{
	int bRet = 0;
	struct mtk_swpm_sysfs_handle *p;

	p = parent;

	if (!p) {
		if (!IS_MTK_SWPM_SYS_HANDLE_VALID(&mtk_swpm_fs_root)) {
			bRet = mtk_swpm_sysfs_root_entry_create();
			if (bRet)
				return bRet;
		}
		p = &mtk_swpm_fs_root;
	}

	bRet = mtk_swpm_sysfs_entry_func_create(name, mode, p, handle);
	return bRet;
}
EXPORT_SYMBOL(mtk_swpm_sysfs_sub_entry_add);

int mtk_swpm_sysfs_sub_entry_node_add(const char *name
		, int mode, const struct mtk_swpm_sysfs_op *op
		, struct mtk_swpm_sysfs_handle *parent
		, struct mtk_swpm_sysfs_handle *handle)
{
	return mtk_swpm_sysfs_entry_func_node_add(name
			, mode, op, parent, handle);
}
EXPORT_SYMBOL(mtk_swpm_sysfs_sub_entry_node_add);

int mtk_swpm_sysfs_remove(void)
{
	return mtk_swpm_sysfs_entry_func_node_remove(&mtk_swpm_fs_root);
}
EXPORT_SYMBOL(mtk_swpm_sysfs_remove);
