// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/genalloc.h>
#include <linux/sprd_iommu.h>
#include "sprd_iommu_sysfs.h"

static struct dentry *iommu_debugfs_dir;

static int iova_show(struct seq_file *s, void *unused)
{
	struct sprd_iommu_dev *iommu_dev = (struct sprd_iommu_dev *)s->private;
	size_t freesize = 0;

	freesize = gen_pool_avail(iommu_dev->pool);
	seq_printf(s, "iova name:%s base:0x%lx size:0x%zx free:0x%zx\n",
		iommu_dev->init_data->name, iommu_dev->init_data->iova_base,
		iommu_dev->init_data->iova_size, freesize);

	return 0;
}

static int iova_open(struct inode *inode, struct file *file)
{
	return single_open(file, iova_show, inode->i_private);
}

static const struct file_operations iova_fops = {
	.owner = THIS_MODULE,
	.open  = iova_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pgt_show(struct seq_file *s, void *unused)
{
	struct sprd_iommu_dev *iommu_dev = (struct sprd_iommu_dev *)s->private;
	struct sprd_iommu_init_data *pdata = iommu_dev->init_data;
	unsigned int i;
	unsigned int reg_val;
	unsigned int pgt_show_size;

	if (!pdata->pagt_base_virt || !pdata->pagt_base_ddr) {
		seq_printf(s, "The page table is empty, cannot be fetched!");
		return 0;
	}

	seq_printf(s, "%s virt:0x%lx, phy:0x%lx, size:0x%lx",
			pdata->name,
			pdata->pagt_base_virt,
			pdata->pagt_base_ddr,
			pdata->pagt_ddr_size);

	pgt_show_size = pdata->pagt_ddr_size / 4 < 0x100 ?
				 pdata->pagt_ddr_size / 4 : 0x100;
	for (i = 0; i < pgt_show_size; i++) {
		if (i % 0x4 == 0)
			seq_printf(s, "\n0x%lx[0x%lx]:",
					pdata->pagt_base_virt + i * 0x4,
					pdata->pagt_base_ddr + i * 0x4);
		reg_val = readl_relaxed(
			(void *)(pdata->pagt_base_virt + i * 0x4));
		seq_printf(s, "0x%08x ", reg_val);
	}
	seq_printf(s, "\nOnly part of the page table is shown above.\n");

	return 0;
}

static int pgt_open(struct inode *inode, struct file *file)
{
	return single_open(file, pgt_show, inode->i_private);
}

static const struct file_operations pgt_fops = {
	.owner = THIS_MODULE,
	.open  = pgt_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sprd_iommu_sysfs_create(struct sprd_iommu_dev *device,
							const char *dev_name)
{
	iommu_debugfs_dir = debugfs_create_dir(dev_name, NULL);

	if (ERR_PTR(-ENODEV) == iommu_debugfs_dir)
		iommu_debugfs_dir = NULL;
	else {
		if (iommu_debugfs_dir != NULL) {
			debugfs_create_file("iova",
							0444,
							iommu_debugfs_dir,
							device,
							&iova_fops);

			debugfs_create_file("pgtable",
				0444,
				iommu_debugfs_dir,
				device,
				&pgt_fops);
		}
	}
	return 0;
}

int sprd_iommu_sysfs_destroy(struct sprd_iommu_dev *device,
							const char *dev_name)
{
	if (iommu_debugfs_dir != NULL)
		debugfs_remove_recursive(iommu_debugfs_dir);

	return 0;
}
