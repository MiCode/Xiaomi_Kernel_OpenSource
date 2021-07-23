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

#include "mtk_lp_sysfs.h"

#define MTK_LP_DEBUGFS_ROOT_NAME	MTK_LP_SYSFS_ENTRY_NAME
#define MTK_LP_DEBUGFS_ROOT_MODE	0644

#if MTK_LP_SYSFS_HAS_ENTRY
static struct mtk_lp_sysfs_handle mtk_lp_debugfs_root;
#endif

static int lp_sys_debugfs_offset;
void *mtk_lp_sysfs_debugfs_seq_start(struct seq_file *sf, loff_t *ppos)
{
	void *bRet = NULL;

	if (*ppos == 0) {
		lp_sys_debugfs_offset = 0;
		bRet = (void *)(&lp_sys_debugfs_offset);
	}

	return bRet;
}

void *mtk_lp_sysfs_debugfs_seq_next(struct seq_file *sf
			, void *v, loff_t *ppos)
{
	return NULL;
}

static int mtk_lp_sysfs_debugfs_seq_show(struct seq_file *sf, void *v)
{
	size_t out_sz = 0, buf_sz = 0;
	char *buf;

	buf_sz = seq_get_buf(sf, &buf);

	/* acquire buffer size is larger enough */
	if (buf_sz < MTK_LP_SYSFS_BUF_READSZ)
		seq_commit(sf, -1);
	else {
		struct mtk_lp_sysfs_op *pOp;

		pOp = sf->private ?: sf->private;
		memset(buf, 0, buf_sz);
		if (pOp && pOp->fs_read)
			out_sz = pOp->fs_read(buf, buf_sz - 1, pOp->priv);
		buf[buf_sz - 1] = '\0';
		seq_commit(sf, out_sz);
	}

	return 0;
}

void mtk_lp_sysfs_debugfs_seq_stop(struct seq_file *sf, void *v)
{
}

static const struct seq_operations mtk_lp_sysfs_debugfs_seq_ops = {
	.start = mtk_lp_sysfs_debugfs_seq_start,
	.next = mtk_lp_sysfs_debugfs_seq_next,
	.stop = mtk_lp_sysfs_debugfs_seq_stop,
	.show = mtk_lp_sysfs_debugfs_seq_show,
};

static int mtk_lp_sysfs_debugfs_open(
	struct inode *inode, struct file *filp)
{
	int error = -EACCES;

	error = seq_open(filp, &mtk_lp_sysfs_debugfs_seq_ops);

	if (error == 0) {
		((struct seq_file *)filp->private_data)->private
			= inode->i_private;
	}
	return 0;
}
static int mtk_lp_sysfs_debugfs_close(
	struct inode *inode, struct file *filp)
{
	return seq_release(inode, filp);
}

static ssize_t mtk_lp_sysfs_debugfs_read(
	struct file *filp, char __user *userbuf
				, size_t count, loff_t *f_pos)
{
	return seq_read(filp, userbuf, count, f_pos);
}

static ssize_t mtk_lp_sysfs_debugfs_write(
	struct file *filp, const char __user *userbuf
					, size_t count, loff_t *f_pos)
{
	char BufFromUser[MTK_LP_SYSFS_BUF_WRITESZ];
	ssize_t bSz = -EINVAL;

	struct mtk_lp_sysfs_op *pOp = (struct mtk_lp_sysfs_op *)
			((struct seq_file *)filp->private_data)->private;

	count = min(count, sizeof(BufFromUser));

	memset(&BufFromUser[0], 0, sizeof(BufFromUser));
	if (copy_from_user(&BufFromUser[0], userbuf, count))
		return -EFAULT;

	if (pOp && pOp->fs_write)
		bSz = pOp->fs_write(&BufFromUser[0], count, pOp->priv);
	return bSz;
}

static const struct file_operations mtk_lpsysfs_debug_op = {
	.open = mtk_lp_sysfs_debugfs_open,
	.read = mtk_lp_sysfs_debugfs_read,
	.write = mtk_lp_sysfs_debugfs_write,
	.llseek = seq_lseek,
	.release = mtk_lp_sysfs_debugfs_close,
};

#if MTK_LP_SYSFS_HAS_ENTRY
int mtk_lp_debugfs_root_get(void)
{
	struct dentry *p = NULL;
	int ret = 0;

	if (IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lp_debugfs_root))
		return ret;

	p = debugfs_create_dir(MTK_LP_DEBUGFS_ROOT_NAME, NULL);

	if (p)
		mtk_lp_debugfs_root._current = (void *)p;
	else
		ret = -EPERM;
	return ret;
}

int mtk_lp_debugfs_root_put(void)
{
	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lp_debugfs_root)) {
		debugfs_remove((struct dentry *)
			     mtk_lp_debugfs_root._current);
		mtk_lp_debugfs_root._current = NULL;
	}
	return 0;
}
#endif

int mtk_lp_sysfs_entry_create_plat(const char *name
		, int mode, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	struct dentry *pHandle = NULL;
	int bRet = 0;
	(void)mode;

	if (!handle)
		return -EINVAL;

#if MTK_LP_SYSFS_HAS_ENTRY
	bRet = mtk_lp_debugfs_root_get();
#endif
	if (!bRet) {
		if (parent && parent->_current)
			pHandle = (struct dentry *)parent->_current;
#if MTK_LP_SYSFS_HAS_ENTRY
		else
			pHandle = (struct dentry *)
				  mtk_lp_debugfs_root._current;
#endif

		if (pHandle)
			handle->_current =
				(void *)debugfs_create_dir(name, pHandle);
		else
			handle->_current =
				(void *)debugfs_create_dir(name, NULL);

		if (!handle->_current)
			bRet = -EPERM;
	}
	return bRet;
}
int mtk_lp_sysfs_entry_node_add_plat(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *node)
{
	struct dentry *c = NULL;
	struct dentry *pHandle = NULL;

	int bRet = 0;

	if (!op)
		return -EINVAL;

	if (!parent || !parent->_current) {
#if MTK_LP_SYSFS_HAS_ENTRY
		if (!mtk_lp_debugfs_root_get()) {
			pHandle = (struct dentry *)
				  mtk_lp_debugfs_root._current;
		} else
			return -EINVAL;
#else
		return -EINVAL;
#endif
	} else
		pHandle = (struct dentry *)parent->_current;

	c = debugfs_create_file(name, mode
		, (struct dentry *)parent->_current
		, (void *)op, &mtk_lpsysfs_debug_op);

	if (node)
		node->_current = (void *)c;

	return bRet;
}

int mtk_lp_sysfs_entry_node_remove_plat(
		struct mtk_lp_sysfs_handle *node)
{
	int bRet = 0;

	debugfs_remove((struct dentry *)node->_current);
	node->_current = NULL;
	return bRet;
}

int mtk_lp_sysfs_entry_group_create_plat(const char *name
		, int mode, struct mtk_lp_sysfs_group *_group
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	int bRet = 0;
	int idx = 0;
	struct mtk_lp_sysfs_handle Grouper;
	struct mtk_lp_sysfs_handle *pGrouper = &Grouper;

	if (handle)
		pGrouper = handle;

	mtk_lp_sysfs_entry_create_plat(name, mode, parent, pGrouper);

	if (_group && IS_MTK_LP_SYS_HANDLE_VALID(pGrouper)) {
		for (idx = 0;; ++idx) {
			if ((_group->attrs[idx] == NULL) ||
				(idx >= _group->attr_num))
				break;

			mtk_lp_sysfs_entry_node_add_plat(
				_group->attrs[idx]->name
				, _group->attrs[idx]->mode
				, &_group->attrs[idx]->sysfs_op
				, pGrouper
				, NULL);
		}
	}

	return bRet;
}
