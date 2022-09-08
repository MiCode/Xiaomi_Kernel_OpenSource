// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include "mtk_lp_sysfs.h"
#include "mtk_lp_kernfs.h"

#define CUSTOM_SYSFS_ROOT 1
#define MTK_LPM_SYSFS_NAME	"mtk_lpm"

static struct kobject *mtklpm_kobj;

int mtk_lp_sysfs_kernfs_entry_create_plat(const char *name,
		int mode, struct mtk_lp_sysfs_handle *parent,
		struct mtk_lp_sysfs_handle *handle)
{
	struct kernfs_node *pHandle = NULL;
	struct kobject *priv;
	int bRet = 0;
	(void)mode;

	if (!handle)
		return -EINVAL;
	if (CUSTOM_SYSFS_ROOT) {
		/* create dir from /sys/kernel/<MTK_LPM_SYSFS_NAME> */
		mtklpm_kobj = mtklpm_kobj
		?: kobject_create_and_add(MTK_LPM_SYSFS_NAME, kernel_kobj);
	} else {
		/* create dir from /sys/kernel */
		mtklpm_kobj = kernel_kobj;
	}
	if (!mtklpm_kobj) {
		handle->_current = NULL;
		return -ENOMEM;
	}

	if (parent && parent->_current)
		pHandle = (struct kernfs_node *)parent->_current;
	else
		pHandle = mtklpm_kobj->sd;

	if (kernfs_type(pHandle) != KERNFS_DIR)
		return -EINVAL;

	priv = pHandle->priv;
	//handle->_current =
	//	(void *)kernfs_create_dir(pHandle, name,
		//			mode, priv);
	return bRet;
}
int mtk_lp_sysfs_kernfs_entry_node_add_plat(const char *name,
		int mode, const struct mtk_lp_sysfs_op *op,
		struct mtk_lp_sysfs_handle *parent,
		struct mtk_lp_sysfs_handle *node)
{
	struct kernfs_node *c = NULL;
	struct kernfs_node *p =
		(struct kernfs_node *)parent->_current;

	int bRet = 0;

	if (!parent || !p || (kernfs_type(p) != KERNFS_DIR))
		return -EINVAL;

	bRet = mtk_lp_kernfs_create_file(p, &c, MTK_LP_KERNFS_IDIOTYPE,
				  name, mode, (void *)op);

	if (node)
		node->_current = (void *)c;

	return bRet;
}

int mtk_lp_sysfs_kernfs_entry_node_remove_plat(
		struct mtk_lp_sysfs_handle *node)
{
	int ret = 0;

	ret = mtk_lp_kernfs_remove_file((struct kernfs_node *)node->_current);
	node->_current = NULL;
	return ret;
}
