// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include "apu.h"

static struct platform_device *g_apu_pdev;

struct apusys_rv_seq_data {
	size_t len;
	uint32_t i;
	void *base;
};

static struct proc_dir_entry *procfs_root;

static struct apusys_rv_seq_data *p_seqdata_coredump;
static struct apusys_rv_seq_data *p_seqdata_xfile;
static struct apusys_rv_seq_data *p_seqdata_regdump;

static void *coredump_seq_start(struct seq_file *s, loff_t *pos)
{
	struct mtk_apu *apu = (struct mtk_apu *) platform_get_drvdata(g_apu_pdev);

	if (p_seqdata_coredump == NULL) {
		p_seqdata_coredump = kzalloc(sizeof(struct apusys_rv_seq_data),
			GFP_KERNEL);
		if (!p_seqdata_coredump)
			return NULL;
		p_seqdata_coredump->len = sizeof(struct apu_coredump);
		p_seqdata_coredump->i = 0;
		if (apu->platdata->flags & F_SECURE_COREDUMP)
			p_seqdata_coredump->base = apu->apu_aee_coredump_mem_base +
				apu->apusys_aee_coredump_info->up_coredump_ofs;
		else
			p_seqdata_coredump->base = apu->coredump_buf;
	} else if (p_seqdata_coredump->i >= p_seqdata_coredump->len) {
		kfree(p_seqdata_coredump);
		p_seqdata_coredump = NULL;
		return NULL;
	}

	return p_seqdata_coredump;
}

static void *coredump_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct apusys_rv_seq_data *pSData = v;

	if (pSData == NULL) {
		dev_info(&g_apu_pdev->dev, "%s: pSData == NULL\n", __func__);
		return NULL;
	}

	pSData->i = pSData->i + PAGE_SIZE;

	/* prevent kernel warning */
	*pos = pSData->i;

	if (pSData->i >= pSData->len)
		return NULL;

	return v;
}

static void coredump_seq_stop(struct seq_file *s, void *v)
{
}

static int coredump_seq_show(struct seq_file *s, void *v)
{
	struct apusys_rv_seq_data *pSData = v;

	if (pSData->i + PAGE_SIZE <= pSData->len)
		seq_write(s, pSData->base + pSData->i, PAGE_SIZE);
	else
		seq_write(s, pSData->base + pSData->i, pSData->len - pSData->i);

	return 0;
}

static void *xfile_seq_start(struct seq_file *s, loff_t *pos)
{
	struct mtk_apu *apu = (struct mtk_apu *) platform_get_drvdata(g_apu_pdev);

	if (p_seqdata_xfile == NULL) {
		p_seqdata_xfile = kzalloc(sizeof(struct apusys_rv_seq_data),
			GFP_KERNEL);
		if (!p_seqdata_xfile)
			return NULL;
		if (apu->platdata->flags & F_PRELOAD_FIRMWARE)
			p_seqdata_xfile->len = apu->apusys_aee_coredump_info->up_xfile_sz;
		else
			p_seqdata_xfile->len = 0;
		p_seqdata_xfile->i = 0;
		p_seqdata_xfile->base = apu->apu_aee_coredump_mem_base +
			apu->apusys_aee_coredump_info->up_xfile_ofs;
	} else if (p_seqdata_xfile->i >= p_seqdata_xfile->len) {
		kfree(p_seqdata_xfile);
		p_seqdata_xfile = NULL;
		return NULL;
	}

	return p_seqdata_xfile;
}

static void *xfile_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct apusys_rv_seq_data *pSData = v;

	if (pSData == NULL) {
		dev_info(&g_apu_pdev->dev, "%s: pSData == NULL\n", __func__);
		return NULL;
	}

	pSData->i = pSData->i + PAGE_SIZE;

	/* prevent kernel warning */
	*pos = pSData->i;

	if (pSData->i >= pSData->len)
		return NULL;

	return v;
}

static void xfile_seq_stop(struct seq_file *s, void *v)
{
}

static int xfile_seq_show(struct seq_file *s, void *v)
{
	struct apusys_rv_seq_data *pSData = v;

	if (pSData->i + PAGE_SIZE <= pSData->len)
		seq_write(s, pSData->base + pSData->i, PAGE_SIZE);
	else
		seq_write(s, pSData->base + pSData->i, pSData->len - pSData->i);

	return 0;
}

/* TODO: add regdump base and len */
static void *regdump_seq_start(struct seq_file *s, loff_t *pos)
{
	if (p_seqdata_regdump == NULL) {
		p_seqdata_regdump = kzalloc(sizeof(struct apusys_rv_seq_data),
			GFP_KERNEL);
		if (!p_seqdata_regdump)
			return NULL;

		p_seqdata_regdump->len = 0;
		p_seqdata_regdump->i = 0;
		p_seqdata_regdump->base = NULL;
	} else if (p_seqdata_regdump->i >= p_seqdata_regdump->len) {
		kfree(p_seqdata_regdump);
		p_seqdata_regdump = NULL;
		return NULL;
	}

	return p_seqdata_regdump;
}

static void *regdump_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct apusys_rv_seq_data *pSData = v;

	if (pSData == NULL) {
		dev_info(&g_apu_pdev->dev, "%s: pSData == NULL\n", __func__);
		return NULL;
	}

	pSData->i = pSData->i + PAGE_SIZE;

	/* prevent kernel warning */
	*pos = pSData->i;

	if (pSData->i >= pSData->len)
		return NULL;

	return v;
}

static void regdump_seq_stop(struct seq_file *s, void *v)
{
}

static int regdump_seq_show(struct seq_file *s, void *v)
{
	struct apusys_rv_seq_data *pSData = v;

	if (pSData->i + PAGE_SIZE <= pSData->len)
		seq_write(s, pSData->base + pSData->i, PAGE_SIZE);
	else
		seq_write(s, pSData->base + pSData->i, pSData->len - pSData->i);

	return 0;
}

static const struct seq_operations coredump_seq_ops = {
	.start = coredump_seq_start,
	.next  = coredump_seq_next,
	.stop  = coredump_seq_stop,
	.show  = coredump_seq_show
};

static const struct seq_operations xfile_seq_ops = {
	.start = xfile_seq_start,
	.next  = xfile_seq_next,
	.stop  = xfile_seq_stop,
	.show  = xfile_seq_show
};

static const struct seq_operations regdump_seq_ops = {
	.start = regdump_seq_start,
	.next  = regdump_seq_next,
	.stop  = regdump_seq_stop,
	.show  = regdump_seq_show
};

static int coredump_sqopen(struct inode *inode, struct file *file)
{
	return seq_open(file, &coredump_seq_ops);
}

static int xfile_sqopen(struct inode *inode, struct file *file)
{
	return seq_open(file, &xfile_seq_ops);
}

static int regdump_sqopen(struct inode *inode, struct file *file)
{
	return seq_open(file, &regdump_seq_ops);
}

static const struct proc_ops coredump_file_ops = {
	.proc_open		= coredump_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= seq_release
};

static const struct proc_ops xfile_file_ops = {
	.proc_open		= xfile_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= seq_release
};

static const struct proc_ops regdump_file_ops = {
	.proc_open		= regdump_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= seq_release
};

int apu_procfs_init(struct platform_device *pdev)
{
	int ret;
	struct proc_dir_entry *coredump_seqlog;
	struct proc_dir_entry *xfile_seqlog;
	struct proc_dir_entry *regdump_seqlog;

	g_apu_pdev = pdev;

	procfs_root = proc_mkdir("apusys_rv", NULL);
	ret = IS_ERR_OR_NULL(procfs_root);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv dir\n", ret);
		goto out;
	}

	coredump_seqlog = proc_create("apusys_rv_coredump", 0444,
		procfs_root, &coredump_file_ops);
	ret = IS_ERR_OR_NULL(coredump_seqlog);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_rv_coredump)\n",
			ret);
		goto out;
	}

	xfile_seqlog = proc_create("apusys_rv_xfile", 0444,
		procfs_root, &xfile_file_ops);
	ret = IS_ERR_OR_NULL(xfile_seqlog);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_rv_xfile)\n",
			ret);
		goto out;
	}

	regdump_seqlog = proc_create("apusys_regdump", 0444,
		procfs_root, &regdump_file_ops);
	ret = IS_ERR_OR_NULL(regdump_seqlog);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_regdump)\n",
			ret);
		goto out;
	}

out:
	return ret;
}

void apu_procfs_remove(struct platform_device *pdev)
{
	remove_proc_entry("apusys_regdump", procfs_root);
	remove_proc_entry("apusys_rv_xfile", procfs_root);
	remove_proc_entry("apusys_rv_coredump", procfs_root);
	remove_proc_entry("apusys_rv", NULL);
}
