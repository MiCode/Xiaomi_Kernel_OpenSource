// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "mtk_lp_sysfs.h"


#define MTK_LP_PROC_ROOT_NAME	MTK_LP_SYSFS_ENTRY_NAME
#define MTK_LP_PROC_ROOT_MODE	0644

#if MTK_LP_SYSFS_HAS_ENTRY
static struct mtk_lp_sysfs_handle mtk_lp_proc_root;
#endif

static int lp_sys_proc_offset;
void *mtk_lp_sysfs_procfs_seq_start(struct seq_file *sf, loff_t *ppos)
{
	/* return null and seq_next will stop */
	void *bRet = NULL;

	if (*ppos == 0) {
		lp_sys_proc_offset = 0;
		bRet = (void *)(&lp_sys_proc_offset);
	}
	return bRet;
}

void *mtk_lp_sysfs_procfs_seq_next(struct seq_file *sf
			, void *v, loff_t *ppos)
{
	return NULL;
}

static int mtk_lp_sysfs_procfs_seq_show(struct seq_file *sf, void *v)
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
		pr_info("[%s:%d] buf_sz=%lu\n", __func__, __LINE__, buf_sz);
	}

	return 0;
}

void mtk_lp_sysfs_procfs_seq_stop(struct seq_file *sf, void *v)
{
}

static const struct seq_operations mtk_lp_sysfs_procfs_seq_ops = {
	.start = mtk_lp_sysfs_procfs_seq_start,
	.next = mtk_lp_sysfs_procfs_seq_next,
	.stop = mtk_lp_sysfs_procfs_seq_stop,
	.show = mtk_lp_sysfs_procfs_seq_show,
};

static int mtk_lp_sysfs_procfs_open(
	struct inode *inode, struct file *filp)
{
	int error = -EACCES;

	error = seq_open(filp, &mtk_lp_sysfs_procfs_seq_ops);

	if (error == 0) {
		((struct seq_file *)filp->private_data)->private
			= PDE_DATA(inode);
	}
	return 0;
}
static int mtk_lp_sysfs_procfs_close(
	struct inode *inode, struct file *filp)
{
	return seq_release(inode, filp);
}

static ssize_t mtk_lp_sysfs_procfs_read(struct file *filp,
				char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	return seq_read(filp, userbuf, count, f_pos);
}

static ssize_t mtk_lp_sysfs_procfs_write(struct file *filp,
					const char __user *userbuf,
					size_t count, loff_t *f_pos)
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

static const struct proc_ops mtk_lpsysfs_proc_op = {
	.proc_open = mtk_lp_sysfs_procfs_open,
	.proc_read = mtk_lp_sysfs_procfs_read,
	.proc_write = mtk_lp_sysfs_procfs_write,
	.proc_lseek = seq_lseek,
	.proc_release = mtk_lp_sysfs_procfs_close,
};

#if MTK_LP_SYSFS_HAS_ENTRY
int mtk_lp_proc_root_get(void)
{
	struct proc_dir_entry *p = NULL;
	int ret = 0;

	if (IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lp_proc_root))
		return ret;

	p = proc_mkdir(MTK_LP_PROC_ROOT_NAME, NULL);

	if (p)
		mtk_lp_proc_root._current = (void *)p;
	else
		ret = -EPERM;
	return ret;
}

int mtk_lp_proc_root_put(void)
{
	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_lp_proc_root)) {
		proc_remove((struct proc_dir_entry *)
			     mtk_lp_proc_root._current);
		mtk_lp_proc_root._current = NULL;
	}
	return 0;
}
#endif

int mtk_lp_sysfs_entry_create_plat(const char *name,
		int mode, struct mtk_lp_sysfs_handle *parent,
		struct mtk_lp_sysfs_handle *handle)
{
	struct proc_dir_entry *pHandle = NULL;
	int bRet = 0;
	(void)mode;

	if (!handle)
		return -EINVAL;

#if MTK_LP_SYSFS_HAS_ENTRY
	bRet = mtk_lp_proc_root_get();
#endif
	if (!bRet) {
		if (parent && parent->_current)
			pHandle = (struct proc_dir_entry *)parent->_current;
#if MTK_LP_SYSFS_HAS_ENTRY
		else
			pHandle = (struct proc_dir_entry *)
				  mtk_lp_proc_root._current;
#endif

		if (pHandle)
			handle->_current =
				(void *)proc_mkdir(name, pHandle);
		else
			handle->_current =
				(void *)proc_mkdir(name, NULL);

		if (!handle->_current)
			bRet = -EPERM;
	}
	return bRet;
}

int mtk_lp_sysfs_entry_node_add_plat(const char *name,
		int mode, const struct mtk_lp_sysfs_op *op,
		struct mtk_lp_sysfs_handle *parent,
		struct mtk_lp_sysfs_handle *node)
{
	struct proc_dir_entry *c = NULL;
	struct proc_dir_entry *pHandle = NULL;
	int bRet = 0;

	if (!parent || !parent->_current) {
#if MTK_LP_SYSFS_HAS_ENTRY
		if (!mtk_lp_proc_root_get()) {
			pHandle = (struct proc_dir_entry *)
				  mtk_lp_proc_root._current;
		} else
			return -EINVAL;
#else
		return -EINVAL;
#endif
	} else
		pHandle =
			(struct proc_dir_entry *)parent->_current;

	c = proc_create_data(name, mode, pHandle,
			     (void *)&mtk_lpsysfs_proc_op,
			     (void *)op);

	if (node)
		node->_current = (void *)c;
	return bRet;
}

int mtk_lp_sysfs_entry_node_remove_plat(
		struct mtk_lp_sysfs_handle *node)
{
	int bRet = 0;

	if (!node)
		return -EINVAL;
	pr_info("FS remove %s\n", node->name);
	return bRet;
}

int mtk_lp_sysfs_entry_group_create_plat(const char *name,
		int mode, struct mtk_lp_sysfs_group *_group,
		struct mtk_lp_sysfs_handle *parent,
		struct mtk_lp_sysfs_handle *handle)
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
