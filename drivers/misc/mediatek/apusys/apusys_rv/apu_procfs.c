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
#include <mt-plat/mrdump.h>

#include "apu.h"
#include "apu_regdump.h"

#define PT_MAGIC (0x58901690)

static struct platform_device *g_apu_pdev;
static struct proc_dir_entry *procfs_root;
static size_t coredump_len, xfile_len;
static void *coredump_base, *xfile_base;

static int coredump_seq_show(struct seq_file *s, void *v)
{
	seq_write(s, coredump_base, coredump_len);

	return 0;
}

static int xfile_seq_show(struct seq_file *s, void *v)
{
	seq_write(s, xfile_base, xfile_len);

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
	.proc_release	= single_release
};

static const struct proc_ops xfile_file_ops = {
	.proc_open		= xfile_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops regdump_file_ops = {
	.proc_open		= regdump_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static void apu_mrdump_register(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;
	const struct apusys_regdump_info *info = NULL;
	unsigned long base_va = 0;
	unsigned long base_pa = 0;
	unsigned long size = 0;

	if (apu->platdata->flags & F_SECURE_COREDUMP) {
		base_pa = apu->apusys_aee_coredump_mem_start +
			apu->apusys_aee_coredump_info->up_coredump_ofs;
		base_va = (unsigned long) apu->apu_aee_coredump_mem_base +
			apu->apusys_aee_coredump_info->up_coredump_ofs;
		size = sizeof(struct apu_coredump);
	} else {
		base_pa = __pa_nodebug(apu->coredump_buf);
		base_va = (unsigned long) apu->coredump_buf;
		size = sizeof(struct apu_coredump);
	}
	ret = mrdump_mini_add_extra_file(base_va, base_pa, size,
		"APUSYS_RV_COREDUMP");
	if (ret)
		dev_info(dev, "%s: APUSYS_RV_COREDUMP add fail(%d)\n",
			__func__, ret);
	coredump_len = (size_t) size;
	coredump_base = (void *) base_va;

	if (apu->platdata->flags & F_PRELOAD_FIRMWARE) {
		base_pa = apu->apusys_aee_coredump_mem_start +
			apu->apusys_aee_coredump_info->up_xfile_ofs;
		base_va = (unsigned long) apu->apu_aee_coredump_mem_base +
			apu->apusys_aee_coredump_info->up_xfile_ofs;
		if (ioread32((void *) base_va) != PT_MAGIC) {
			dev_info(dev, "%s: reserve memory corrupted!\n", __func__);
			size = 0;
		} else {
			size = apu->apusys_aee_coredump_info->up_xfile_sz;
			dev_info(dev, "%s: up_xfile_sz = 0x%x\n", __func__, size);
		}

		ret = mrdump_mini_add_extra_file(base_va, base_pa, size,
			"APUSYS_RV_XFILE");
		if (ret)
			dev_info(dev, "%s: APUSYS_RV_XFILE add fail(%d)\n",
				__func__, ret);
	}
	xfile_len = (size_t) size;
	xfile_base = (void *) base_va;

	base_pa = apu->apusys_aee_coredump_mem_start +
		apu->apusys_aee_coredump_info->regdump_ofs;
	base_va = (unsigned long) apu->apu_aee_coredump_mem_base +
		apu->apusys_aee_coredump_info->regdump_ofs;
	info = (struct apusys_regdump_info *) base_va;
	size = apu->apusys_aee_coredump_info->regdump_sz;

	if (info != NULL) {
		ret = mrdump_mini_add_extra_file(base_va, base_pa, size,
			"APUSYS_REGDUMP");
		if (ret)
			dev_info(dev, "%s: APUSYS_REGDUMP add fail(%d)\n",
				__func__, ret);
	}
}

int apu_procfs_init(struct platform_device *pdev)
{
	int ret;
	struct proc_dir_entry *coredump_seqlog;
	struct proc_dir_entry *xfile_seqlog;
	struct proc_dir_entry *regdump_seqlog;
	struct mtk_apu *apu = (struct mtk_apu *) platform_get_drvdata(pdev);

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

	apu_mrdump_register(apu);
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
