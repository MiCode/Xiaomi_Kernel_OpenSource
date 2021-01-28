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
#include <linux/mutex.h>

#include "mtk_lp_sysfs.h"

#ifndef __weak
#define __weak __attribute__((weak))
#endif

DEFINE_MUTEX(mtk_lp_sysfs_locker);

int __weak mtk_lp_sysfs_entry_create_plat(const char *name
		, int mode, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	return 0;
}
int __weak mtk_lp_sysfs_entry_node_add_plat(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	return 0;
}

int __weak mtk_lp_sysfs_entry_group_create_plat(const char *name
		, int mode, struct mtk_lp_sysfs_group *_group
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	return 0;
}

int mtk_lp_sysfs_entry_func_create(const char *name
		, int mode, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	int bRet = 0;

	mutex_lock(&mtk_lp_sysfs_locker);
	bRet = mtk_lp_sysfs_entry_create_plat(name
			, mode, parent, handle);
	mutex_unlock(&mtk_lp_sysfs_locker);
	return bRet;
}

int mtk_lp_sysfs_entry_func_node_add(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *node)
{
	int bRet = 0;

	mutex_lock(&mtk_lp_sysfs_locker);
	bRet = mtk_lp_sysfs_entry_node_add_plat(name
			, mode, op, parent, node);
	mutex_unlock(&mtk_lp_sysfs_locker);
	return bRet;
}

int mtk_lp_sysfs_entry_func_group_create(const char *name
		, int mode, struct mtk_lp_sysfs_group *_group
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	int bRet = 0;

	mutex_lock(&mtk_lp_sysfs_locker);
	bRet = mtk_lp_sysfs_entry_group_create_plat(name
			, mode, _group, parent, handle);
	mutex_unlock(&mtk_lp_sysfs_locker);
	return bRet;
}

