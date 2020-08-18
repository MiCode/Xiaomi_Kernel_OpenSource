// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include "mtk_spm_sysfs.h"

#define MTK_SPM_SYS_FS_NAME	"spm"
#define MTK_SPM_SYS_FS_MODE	0644

static struct mtk_lp_sysfs_handle mtk_spm_fs_root;

int mtk_spm_sysfs_entry_node_add(const char *name,
		int mode, const struct mtk_lp_sysfs_op *op,
		struct mtk_lp_sysfs_handle *handle)
{
	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_spm_fs_root))
		mtk_spm_sysfs_root_entry_create();

	return mtk_lp_sysfs_entry_func_node_add(name
			, mode, op, &mtk_spm_fs_root, handle);
}
EXPORT_SYMBOL(mtk_spm_sysfs_entry_node_add);

int mtk_spm_sysfs_entry_node_remove(
		struct mtk_lp_sysfs_handle *handle)
{
	return mtk_lp_sysfs_entry_func_node_remove(handle);
}
EXPORT_SYMBOL(mtk_spm_sysfs_entry_node_remove);

int mtk_spm_sysfs_root_entry_create(void)
{
	int bRet = 0;

	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_spm_fs_root)) {
		bRet = mtk_lp_sysfs_entry_func_create(
			MTK_SPM_SYS_FS_NAME, MTK_SPM_SYS_FS_MODE
			, NULL, &mtk_spm_fs_root);
	}
	return bRet;
}
EXPORT_SYMBOL(mtk_spm_sysfs_root_entry_create);

int mtk_spm_sysfs_entry_root_get(struct mtk_lp_sysfs_handle **handle)
{
	if (!handle ||
		!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_spm_fs_root)
	)
		return -1;
	*handle = &mtk_spm_fs_root;
	return 0;
}
EXPORT_SYMBOL(mtk_spm_sysfs_entry_root_get);

int mtk_spm_sysfs_remove(void)
{
	return mtk_lp_sysfs_entry_func_node_remove(&mtk_spm_fs_root);
}

