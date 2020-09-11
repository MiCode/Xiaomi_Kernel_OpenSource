// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uidgid.h>

#include <mtk_lp_kernfs.h>

#define MTK_LP_SYSFS_POWER_BUFFER_SZ	8192

#define LP_SYSFS_STATUS_INITIAL			0
#define LP_SYSFS_STATUS_READY			(1<<0)
#define LP_SYSFS_STATUS_READ_MORE		(1<<1)

struct mtk_lp_kernfs_info {
	int status;
	struct mutex locker;
};

#define MTK_LP_INFO_SZ	sizeof(struct mtk_lp_kernfs_info)

static const struct sysfs_ops *mtk_lp_file_ops(struct kernfs_node *kn)
{
	struct kobject *kobj = kn->parent->priv;

	if (kn->flags & KERNFS_LOCKDEP)
		lockdep_assert_held(kn);
	return kobj->ktype ? kobj->ktype->sysfs_ops : NULL;
}

void *mtk_lp_kernfs_seq_start(struct seq_file *sf, loff_t *ppos)
{
	void *bRet = NULL;

	if (*ppos == 0) {
		struct mtk_lp_kernfs_info *lp_fs_ctrl = NULL;

		lp_fs_ctrl =
			kmalloc(MTK_LP_INFO_SZ, GFP_KERNEL | __GFP_NOWARN);

		if (lp_fs_ctrl) {
			mutex_init(&lp_fs_ctrl->locker);
			bRet = (void *)lp_fs_ctrl;
			lp_fs_ctrl->status = LP_SYSFS_STATUS_READY;
			bRet = (void *)lp_fs_ctrl;
		}
	}

	return bRet;
}

void *mtk_lp_kernfs_seq_next(struct seq_file *sf, void *v, loff_t *ppos)
{
	void *bRet = NULL;
	struct mtk_lp_kernfs_info *lp =
		v ?: (struct mtk_lp_kernfs_info *)v;

	*ppos += 1;

	if (lp && (lp->status & LP_SYSFS_STATUS_READ_MORE))
		bRet = v;

	return bRet;
}

static int mtk_lp_kernfs_seq_show(struct seq_file *sf, void *v)
{
	struct mtk_lp_kernfs_info *lp =
		v ?: (struct mtk_lp_kernfs_info *)v;

	if (lp && (lp->status & LP_SYSFS_STATUS_READY)) {
		struct kernfs_open_file *of = sf->private;
		struct kobject *kobj = of->kn->parent->priv;
		const struct sysfs_ops *ops = mtk_lp_file_ops(of->kn);
		size_t out_sz = 0, buf_sz = 0;
		char *buf;

		buf_sz = seq_get_buf(sf, &buf);

		/* acquire buffer size is larger enough */
		if (buf_sz < MTK_LP_SYSFS_POWER_BUFFER_SZ)
			seq_commit(sf, -1);
		else {
			memset(buf, 0, buf_sz);
			if (ops && ops->show)
				out_sz = ops->show(kobj, of->kn->priv, buf);
			seq_commit(sf, out_sz);
		}
		mutex_lock(&lp->locker);
		lp->status &= ~LP_SYSFS_STATUS_READ_MORE;
		mutex_unlock(&lp->locker);
	}

	return 0;
}

void mtk_lp_kernfs_seq_stop(struct seq_file *sf, void *v)
{
	kfree(v);
}

static ssize_t mtk_lp_kernfs_write(struct kernfs_open_file *of, char *buf,
			      size_t count, loff_t pos)
{
	const struct sysfs_ops *ops = mtk_lp_file_ops(of->kn);
	struct kobject *kobj = of->kn->parent->priv;

	if (!ops || !count)
		return 0;

	return ops->store(kobj, of->kn->priv, buf, count);
}

static const struct kernfs_ops mtk_lp_kernfs_kfops_rw = {
	.seq_show = mtk_lp_kernfs_seq_show,
	.seq_start = mtk_lp_kernfs_seq_start,
	.seq_next = mtk_lp_kernfs_seq_next,
	.seq_stop = mtk_lp_kernfs_seq_stop,
	.write = mtk_lp_kernfs_write,
};

int mtk_lp_kernfs_create_file(struct kernfs_node *parent
		, const struct attribute *attr)
{
	struct kernfs_node *kn;

	kn = __kernfs_create_file(parent, attr->name
				, attr->mode & 0755
				, 4096, &mtk_lp_kernfs_kfops_rw
				, (void *)attr, NULL, NULL);

	if (IS_ERR(kn))
		return PTR_ERR(kn);

	return 0;
}
EXPORT_SYMBOL(mtk_lp_kernfs_create_file);

int mtk_lp_kernfs_create_group(struct kobject *kobj
						, struct attribute_group *grp)
{
	struct kernfs_node *kn;
	struct attribute *const *attr;
	int error = 0, i;

	kn = kernfs_create_dir(kobj->sd, grp->name,
				0755, kobj);

	if (IS_ERR(kn))
		return PTR_ERR(kn);

	kernfs_get(kn);
	if (grp->attrs) {
		for (i = 0, attr = grp->attrs; *attr && !error; i++, attr++)
			mtk_lp_kernfs_create_file(kn, *attr);
	}
	kernfs_put(kn);
	return 0;
}
EXPORT_SYMBOL(mtk_lp_kernfs_create_group);

size_t get_mtk_lp_kernfs_bufsz_max(void)
{
	return MTK_LP_SYSFS_POWER_BUFFER_SZ;
}
EXPORT_SYMBOL(get_mtk_lp_kernfs_bufsz_max);

