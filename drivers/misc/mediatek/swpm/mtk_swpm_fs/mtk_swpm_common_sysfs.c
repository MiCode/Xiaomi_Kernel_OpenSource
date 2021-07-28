// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "mtk_swpm_common_sysfs.h"

#ifndef __weak
#define __weak __attribute__((weak))
#endif

DEFINE_MUTEX(mtk_swpm_sysfs_locker);
static LIST_HEAD(mtk_swpm_sysfs_parent);

int __weak mtk_swpm_sysfs_entry_create_plat(const char *name,
		int mode, struct mtk_swpm_sysfs_handle *parent,
		struct mtk_swpm_sysfs_handle *handle)
{
	return 0;
}
int __weak mtk_swpm_sysfs_entry_node_add_plat(const char *name,
		int mode, const struct mtk_swpm_sysfs_op *op,
		struct mtk_swpm_sysfs_handle *parent,
		struct mtk_swpm_sysfs_handle *handle)
{
	return 0;
}

int __weak mtk_swpm_sysfs_entry_node_remove_plat(
		struct mtk_swpm_sysfs_handle *node)
{
	return 0;
}

int __weak mtk_swpm_sysfs_entry_group_create_plat(const char *name,
		int mode, struct mtk_swpm_sysfs_group *_group,
		struct mtk_swpm_sysfs_handle *parent,
		struct mtk_swpm_sysfs_handle *handle)
{
	return 0;
}

static
int __mtk_swpm_sysfs_handle_add(struct mtk_swpm_sysfs_handle *parent,
				      struct mtk_swpm_sysfs_handle *node)
{
	if (!node)
		return -EINVAL;

	INIT_LIST_HEAD(&node->dr);
	INIT_LIST_HEAD(&node->np);

	if (parent)
		list_add(&node->np, &parent->dr);
	else
		list_add(&node->np, &mtk_swpm_sysfs_parent);
	return 0;
}

int mtk_swpm_sysfs_entry_func_create(const char *name,
		int mode, struct mtk_swpm_sysfs_handle *parent,
		struct mtk_swpm_sysfs_handle *handle)
{
	int bRet;
	struct mtk_swpm_sysfs_handle *p = NULL;

	mutex_lock(&mtk_swpm_sysfs_locker);

	do {
		if (!handle) {
			p = kzalloc(sizeof(*p), GFP_KERNEL);
			if (!p) {
				bRet = -ENOMEM;
				break;
			}
			p->flag = MTK_SWPM_SYSFS_FREEZABLE;
		} else
			p = handle;

		bRet = mtk_swpm_sysfs_entry_create_plat(name,
					mode, parent, p);

		if (!bRet) {
			p->flag |= MTK_SWPM_SYSFS_TYPE_ENTRY;
			p->name = name;
			__mtk_swpm_sysfs_handle_add(parent, p);
		} else
			kfree(p);
	} while (0);
	mutex_unlock(&mtk_swpm_sysfs_locker);
	return bRet;
}
EXPORT_SYMBOL(mtk_swpm_sysfs_entry_func_create);

int mtk_swpm_sysfs_entry_func_node_add(const char *name,
		int mode, const struct mtk_swpm_sysfs_op *op,
		struct mtk_swpm_sysfs_handle *parent,
		struct mtk_swpm_sysfs_handle *node)
{
	int bRet;
	struct mtk_swpm_sysfs_handle *p = NULL;

	mutex_lock(&mtk_swpm_sysfs_locker);

	do {
		if (!node) {
			p = kzalloc(sizeof(*p), GFP_KERNEL);
			if (!p) {
				bRet = -ENOMEM;
				break;
			}
			p->flag = MTK_SWPM_SYSFS_FREEZABLE;
		} else
			p = node;

		bRet = mtk_swpm_sysfs_entry_node_add_plat(name,
					mode, op, parent, p);
		if (!bRet) {
			p->flag &= ~MTK_SWPM_SYSFS_TYPE_ENTRY;
			p->name = name;
			__mtk_swpm_sysfs_handle_add(parent, p);
		} else
			kfree(p);
	} while (0);

	mutex_unlock(&mtk_swpm_sysfs_locker);
	return bRet;
}
EXPORT_SYMBOL(mtk_swpm_sysfs_entry_func_node_add);

static
int __mtk_swpm_sysfs_handle_remove(struct mtk_swpm_sysfs_handle *node)
{
	int ret = 0;

	if (!node)
		return -EINVAL;

	ret = mtk_swpm_sysfs_entry_node_remove_plat(node);

	if (!ret) {
		list_del(&node->np);
		INIT_LIST_HEAD(&node->np);
		if (node->flag & MTK_SWPM_SYSFS_FREEZABLE)
			kfree(node);
	}
	return ret;
}

static
int __mtk_swpm_sysfs_entry_rm(struct mtk_swpm_sysfs_handle *node)
{
	int bret = 0;
	struct mtk_swpm_sysfs_handle *n;
	struct mtk_swpm_sysfs_handle *cur;

	cur = list_first_entry(&node->dr, struct mtk_swpm_sysfs_handle, np);
	do {
		if (list_is_last(&cur->np, &node->dr))
			n = NULL;
		else
			n = list_next_entry(cur, np);

		if (cur->flag & MTK_SWPM_SYSFS_TYPE_ENTRY)
			__mtk_swpm_sysfs_entry_rm(cur);
		__mtk_swpm_sysfs_handle_remove(cur);
		cur = n;
	} while (cur);

	INIT_LIST_HEAD(&node->dr);
	return bret;
}

int mtk_swpm_sysfs_entry_func_node_remove(
		struct mtk_swpm_sysfs_handle *node)
{
	int bRet = 0;

	if (!node)
		return -EINVAL;

	mutex_lock(&mtk_swpm_sysfs_locker);
	if (node->flag & MTK_SWPM_SYSFS_TYPE_ENTRY)
		__mtk_swpm_sysfs_entry_rm(node);
	__mtk_swpm_sysfs_handle_remove(node);
	mutex_unlock(&mtk_swpm_sysfs_locker);
	return bRet;
}
EXPORT_SYMBOL(mtk_swpm_sysfs_entry_func_node_remove);

int mtk_swpm_sysfs_entry_func_group_create(const char *name,
		int mode, struct mtk_swpm_sysfs_group *_group,
		struct mtk_swpm_sysfs_handle *parent,
		struct mtk_swpm_sysfs_handle *handle)
{
	int bRet;

	mutex_lock(&mtk_swpm_sysfs_locker);
	bRet = mtk_swpm_sysfs_entry_group_create_plat(name
			, mode, _group, parent, handle);
	mutex_unlock(&mtk_swpm_sysfs_locker);
	return bRet;
}
EXPORT_SYMBOL(mtk_swpm_sysfs_entry_func_group_create);
