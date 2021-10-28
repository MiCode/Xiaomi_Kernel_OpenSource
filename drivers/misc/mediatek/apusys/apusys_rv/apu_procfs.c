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
#include "apu_regdump.h"

static struct platform_device *g_apu_pdev;

static struct proc_dir_entry *procfs_root;

static int coredump_seq_show(struct seq_file *s, void *v)
{
	struct mtk_apu *apu = (struct mtk_apu *) platform_get_drvdata(g_apu_pdev);
	size_t len;
	void *base;

	len = sizeof(struct apu_coredump);
	if (apu->platdata->flags & F_SECURE_COREDUMP)
		base = apu->apu_aee_coredump_mem_base +
			apu->apusys_aee_coredump_info->up_coredump_ofs;
	else
		base = apu->coredump_buf;

	seq_write(s, base, len);

	return 0;
}

static int xfile_seq_show(struct seq_file *s, void *v)
{
	struct mtk_apu *apu = (struct mtk_apu *) platform_get_drvdata(g_apu_pdev);
	size_t len;
	void *base;

	if (apu->platdata->flags & F_PRELOAD_FIRMWARE)
		len = apu->apusys_aee_coredump_info->up_xfile_sz;
	else
		return 0;
	base = apu->apu_aee_coredump_mem_base +
		apu->apusys_aee_coredump_info->up_xfile_ofs;

	seq_write(s, base, len);

	return 0;
}

static int regdump_seq_show(struct seq_file *s, void *v)
{
	struct mtk_apu *apu = NULL;
	const struct apusys_regdump_info *info = NULL;
	const struct apusys_regdump_region_info *region_info = NULL;
	void *base_va = NULL;
	uint32_t region_info_num = 0;
	unsigned int region_offset = 0;
	int i;

	apu = (struct mtk_apu *) platform_get_drvdata(g_apu_pdev);

	base_va = apu->apu_aee_coredump_mem_base +
		apu->apusys_aee_coredump_info->regdump_ofs;
	info = (struct apusys_regdump_info *) base_va;

	if (info == NULL) {
		dev_info(&g_apu_pdev->dev, "%s: apusys_regdump_info == NULL\n",
			__func__);
		return 0;
	}

	if (info->region_info_num == 0) {
		dev_info(&g_apu_pdev->dev, "%s: region_info_num == 0\n",
			__func__);
		return 0;
	}

	region_info = info->region_info;
	region_info_num = info->region_info_num;

	for (i = 0; i < region_info_num; i++) {
		seq_printf(s, "---- dump %s from 0x%x to 0x%x ----\n",
			region_info[i].name, region_info[i].start,
			region_info[i].start + region_info[i].size);
		seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 16, 4,
			(void *)(base_va + info->size + region_offset),
			region_info[i].size, false);
		region_offset += region_info[i].size;
	}

	return 0;
}

static int coredump_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, coredump_seq_show, NULL);
}

static int xfile_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, xfile_seq_show, NULL);
}

static int regdump_sqopen(struct inode *inode, struct file *file)
{
	bool is_do_regdump = false;

	if (is_do_regdump)
		apu_regdump();

	return single_open(file, regdump_seq_show, NULL);
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
	.proc_release	= single_release
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
	apu_regdump_init(pdev);
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
