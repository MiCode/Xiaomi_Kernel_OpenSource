// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include "mtk_suspend_sysfs.h"

#define MTK_SUSPEND_SYS_FS_NAME	"suspend"
#define MTK_SUSPEND_SYS_FS_MODE	0644

static struct mtk_lp_sysfs_handle mtk_suspend_fs_root = {
	NULL
};

int mtk_suspend_sysfs_entry_group_add(const char *name,
		int mode, struct mtk_lp_sysfs_group *_group,
		struct mtk_lp_sysfs_handle *handle)
{
	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_suspend_fs_root))
		mtk_suspend_sysfs_root_entry_create();

	return mtk_lp_sysfs_entry_func_group_create(name
			, mode, _group, &mtk_suspend_fs_root, handle);
}
EXPORT_SYMBOL(mtk_suspend_sysfs_entry_group_add);

int mtk_suspend_sysfs_entry_node_add(const char *name,
		int mode, const struct mtk_lp_sysfs_op *op,
		struct mtk_lp_sysfs_handle *handle)
{
	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_suspend_fs_root))
		mtk_suspend_sysfs_root_entry_create();

	return mtk_lp_sysfs_entry_func_node_add(name
			, mode, op, &mtk_suspend_fs_root, handle);
}
EXPORT_SYMBOL(mtk_suspend_sysfs_entry_node_add);

int mtk_suspend_sysfs_entry_node_remove(
		struct mtk_lp_sysfs_handle *handle)
{
	return mtk_lp_sysfs_entry_func_node_remove(handle);
}
EXPORT_SYMBOL(mtk_suspend_sysfs_entry_node_remove);

int mtk_suspend_sysfs_root_entry_create(void)
{
	int bRet = 0;

	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_suspend_fs_root)) {
		bRet = mtk_lp_sysfs_entry_func_create(
			MTK_SUSPEND_SYS_FS_NAME, MTK_SUSPEND_SYS_FS_MODE
			, NULL, &mtk_suspend_fs_root);
	}
	return bRet;
}
EXPORT_SYMBOL(mtk_suspend_sysfs_root_entry_create);

int mtk_suspend_sysfs_entry_root_get(struct mtk_lp_sysfs_handle **handle)
{
	if (!handle ||
		!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_suspend_fs_root)
	)
		return -1;
	*handle = &mtk_suspend_fs_root;
	return 0;
}
EXPORT_SYMBOL(mtk_suspend_sysfs_entry_root_get);

int mtk_suspend_sysfs_power_create_group(struct attribute_group *grp)
{
	return mtk_lp_kernfs_create_group(power_kobj, grp);
}
EXPORT_SYMBOL(mtk_suspend_sysfs_power_create_group);

size_t get_mtk_suspend_sysfs_power_bufsz_max(void)
{
	return get_mtk_lp_kernfs_bufsz_max();
}

