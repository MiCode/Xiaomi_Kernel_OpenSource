// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include "mtk_lpm_sysfs.h"

#define MTK_IDLE_SYS_FS_NAME	"lpm"
#define MTK_IDLE_SYS_FS_MODE	0644

static struct mtk_lp_sysfs_handle mtk_lpm_fs_root = {
	NULL
};

int mtk_lpm_sysfs_entry_group_add(const char *name
		, int mode, struct mtk_lp_sysfs_group *_group
		, struct mtk_lp_sysfs_handle *handle)
{
	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lpm_fs_root))
		mtk_lpm_sysfs_root_entry_create();

	return mtk_lp_sysfs_entry_func_group_create(name
			, mode, _group, &mtk_lpm_fs_root, handle);
}
EXPORT_SYMBOL(mtk_lpm_sysfs_entry_group_add);

int mtk_lpm_sysfs_entry_node_add(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *handle)
{
	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lpm_fs_root))
		mtk_lpm_sysfs_root_entry_create();

	return mtk_lp_sysfs_entry_func_node_add(name
			, mode, op, &mtk_lpm_fs_root, handle);
}
EXPORT_SYMBOL(mtk_lpm_sysfs_entry_node_add);

int mtk_lpm_sysfs_entry_node_remove(
		struct mtk_lp_sysfs_handle *handle)
{
	return mtk_lp_sysfs_entry_func_node_remove(handle);
}
EXPORT_SYMBOL(mtk_lpm_sysfs_entry_node_remove);

int mtk_lpm_sysfs_root_entry_create(void)
{
	int bRet = 0;

	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lpm_fs_root)) {
		bRet = mtk_lp_sysfs_entry_func_create(
			MTK_IDLE_SYS_FS_NAME, MTK_IDLE_SYS_FS_MODE
			, NULL, &mtk_lpm_fs_root);
	}
	return bRet;
}
EXPORT_SYMBOL(mtk_lpm_sysfs_root_entry_create);

int mtk_lpm_sysfs_entry_root_get(struct mtk_lp_sysfs_handle **handle)
{
	if (!handle ||
		!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lpm_fs_root)
	)
		return -1;
	*handle = &mtk_lpm_fs_root;
	return 0;
}
EXPORT_SYMBOL(mtk_lpm_sysfs_entry_root_get);

int mtk_lpm_sysfs_sub_entry_add(const char *name, int mode,
					struct mtk_lp_sysfs_handle *parent,
					struct mtk_lp_sysfs_handle *handle)
{
	int bRet = 0;
	struct mtk_lp_sysfs_handle *p;

	p = parent;

	if (!p) {
		if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lpm_fs_root)) {
			bRet = mtk_lpm_sysfs_root_entry_create();
			if (bRet)
				return bRet;
		}
		p = &mtk_lpm_fs_root;
	}

	bRet = mtk_lp_sysfs_entry_func_create(name, mode, p, handle);
	return bRet;
}
EXPORT_SYMBOL(mtk_lpm_sysfs_sub_entry_add);

int mtk_lpm_sysfs_sub_entry_node_add(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	return mtk_lp_sysfs_entry_func_node_add(name
			, mode, op, parent, handle);
}
EXPORT_SYMBOL(mtk_lpm_sysfs_sub_entry_node_add);

