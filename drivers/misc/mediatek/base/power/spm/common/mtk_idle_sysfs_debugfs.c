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

#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/uaccess.h>

#include "mtk_idle_sysfs.h"

#define MTK_IDLE_SYSFS_INIT			NULL
#define MTK_IDLE_SYSFS_BUF_READSZ	1024
#define MTK_IDLE_SYSFS_BUF_WRITESZ	512

static int mtk_idle_sysfs_debugfs_open(
	struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}
static int mtk_idle_sysfs_debugfs_close(
	struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t mtk_idle_sysfs_debugfs_read(
	struct file *filp, char __user *userbuf
				, size_t count, loff_t *f_pos)
{
	char Buf2user[MTK_IDLE_SYSFS_BUF_READSZ];
	ssize_t bSz = -EINVAL;
	struct mtk_idle_sysfs_op *pOp =
		(struct mtk_idle_sysfs_op *)filp->private_data;

	if (pOp && pOp->fs_read) {
		bSz = pOp->fs_read(&Buf2user[0]
			, MTK_IDLE_SYSFS_BUF_READSZ - 1, pOp->priv);

		if (bSz >= 0) {
			if (bSz > (MTK_IDLE_SYSFS_BUF_READSZ - 1))
				bSz = (MTK_IDLE_SYSFS_BUF_READSZ - 1);
			Buf2user[bSz] = '\0';
			bSz = simple_read_from_buffer(userbuf
			, count, f_pos, Buf2user, bSz);
		}
	}
	return bSz;
}

static ssize_t mtk_idle_sysfs_debugfs_write(
	struct file *filp, const char __user *userbuf
					, size_t count, loff_t *f_pos)
{
	char BufFromUser[MTK_IDLE_SYSFS_BUF_WRITESZ];
	ssize_t bSz = -EINVAL;

	struct mtk_idle_sysfs_op *pOp =
		(struct mtk_idle_sysfs_op *)filp->private_data;

	count = min(count, sizeof(BufFromUser));

	memset(&BufFromUser[0], 0, sizeof(BufFromUser));
	if (copy_from_user(&BufFromUser[0], userbuf, count))
		return -EFAULT;

	if (pOp && pOp->fs_write)
		bSz = pOp->fs_write(&BufFromUser[0], count, pOp->priv);
	return bSz;
}

static const struct file_operations mtk_idle_sysfs_op = {
	.open = mtk_idle_sysfs_debugfs_open,
	.read = mtk_idle_sysfs_debugfs_read,
	.write = mtk_idle_sysfs_debugfs_write,
	.llseek = seq_lseek,
	.release = mtk_idle_sysfs_debugfs_close,
};

int mtk_idle_sysfs_entry_create_plat(const char *name
		, int mode, struct mtk_idle_sysfs_handle *parent
		, struct mtk_idle_sysfs_handle *handle)
{
	struct dentry *pHandle = NULL;
	int bRet = 0;
	(void)mode;

	if (!handle)
		return -1;
	if (parent && parent->_current)
		pHandle = (struct dentry *)parent->_current;

	if (pHandle)
	handle->_current =
		(void *)debugfs_create_dir(name, pHandle);
	else
		handle->_current =
			(void *)debugfs_create_dir(name, NULL);
	return bRet;
}
int mtk_idle_sysfs_entry_node_add_plat(const char *name
		, int mode, const struct mtk_idle_sysfs_op *op
		, struct mtk_idle_sysfs_handle *parent
		, struct mtk_idle_sysfs_handle *node)
{
	struct dentry *c = NULL;
	int bRet = 0;

	if (!parent || !parent->_current)
		return -1;

	c = debugfs_create_file(name, mode
		, (struct dentry *)parent->_current
		, (void *)op, &mtk_idle_sysfs_op);

	if (node)
		node->_current = (void *)c;
	return bRet;
}

