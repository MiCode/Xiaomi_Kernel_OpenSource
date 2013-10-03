/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * UFS debugfs - add debugfs interface to the ufshcd.
 * This is currently used for statistics collection and exporting from the
 * UFS driver.
 * This infrastructure can be used for debugging or direct tweaking
 * of the driver from userspace.
 *
 */

#include "debugfs.h"

#define BUFF_LINE_CAPACITY 16

static int ufsdbg_tag_stats_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;
	struct ufs_stats *ufs_stats;
	int i;
	int max_depth;
	bool is_tag_empty = true;

	if (!hba)
		goto exit;

	ufs_stats = &hba->ufs_stats;

	if (!ufs_stats->enabled) {
		pr_debug("%s: ufs statistics are disabled\n", __func__);
		goto exit;
	}

	max_depth = hba->nutrs;

	mutex_lock(&ufs_stats->lock);

	pr_debug("%s: UFS tag statistics:\n", __func__);
	pr_debug("%s: Max tagged command queue depth is %d",
		__func__, max_depth);

	for (i = 0 ; i < max_depth ; ++i) {
		if (hba->ufs_stats.tag_stats[i] != 0) {
			is_tag_empty = false;
			seq_printf(file,
				 "%s: Dispatched tag %d - %llu times\n",
				__func__, i, ufs_stats->tag_stats[i]);

			pr_debug("%s: Dispatched tag %d - %llu times\n",
				__func__, i, ufs_stats->tag_stats[i]);
		}
	}
	mutex_unlock(&ufs_stats->lock);

	if (is_tag_empty)
		pr_debug("%s: All tags statistics are empty", __func__);

exit:
	return 0;
}

static int ufsdbg_tag_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_tag_stats_show, inode->i_private);
}

static ssize_t ufsdbg_tag_stats_write(struct file *filp,
				      const char __user *ubuf, size_t cnt,
				       loff_t *ppos)
{
	struct ufs_hba *hba = filp->f_mapping->host->i_private;
	struct ufs_stats *ufs_stats;
	int val = 0;
	int ret;

	ret = kstrtoint_from_user(ubuf, cnt, 0, &val);
	if (ret) {
		dev_err(hba->dev, "%s: Invalid argument\n", __func__);
		return ret;
	}

	ufs_stats = &hba->ufs_stats;
	mutex_lock(&ufs_stats->lock);

	if (!val) {
		ufs_stats->enabled = false;
		pr_debug("%s: Disabling UFS tag statistics", __func__);
	} else {
		ufs_stats->enabled = true;
		pr_debug("%s: Enabling & Resetting UFS tag statistics",
			 __func__);
		memset(ufs_stats->tag_stats, 0,
		       sizeof(unsigned int) * hba->nutrs);
	}

	mutex_unlock(&ufs_stats->lock);
	return cnt;
}

static const struct file_operations ufsdbg_tag_stats_fops = {
	.open		= ufsdbg_tag_stats_open,
	.read		= seq_read,
	.write		= ufsdbg_tag_stats_write,
};

static int ufshcd_init_tag_statistics(struct ufs_hba *hba)
{
	int ret = 0;

	mutex_init(&hba->ufs_stats.lock);

	hba->ufs_stats.tag_stats = kzalloc(hba->nutrs * sizeof(u64),
					   GFP_KERNEL);
	if (!hba->ufs_stats.tag_stats) {
		dev_err(hba->dev,
			"%s: Unable to allocate UFS tag_stats", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	hba->ufs_stats.enabled = false;

exit:
	return ret;
}

static void
ufsdbg_pr_buf_to_std(struct seq_file *file, void *buff, int size, char *str)
{
	int i;
	char linebuf[38];
	int lines = size/BUFF_LINE_CAPACITY +
			(size % BUFF_LINE_CAPACITY ? 1 : 0);

	for (i = 0; i < lines; i++) {
		hex_dump_to_buffer(buff + i * BUFF_LINE_CAPACITY,
				BUFF_LINE_CAPACITY, BUFF_LINE_CAPACITY, 4,
				linebuf, sizeof(linebuf), false);
		seq_printf(file, "%s [%x]: %s\n", str, i * BUFF_LINE_CAPACITY,
				linebuf);
	}
}

static int ufsdbg_host_regs_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;

	ufsdbg_pr_buf_to_std(file, hba->mmio_base, UFSHCI_REG_SPACE_SIZE,
			"host regs");
	return 0;
}

static int ufsdbg_host_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_host_regs_show, inode->i_private);
}

static const struct file_operations ufsdbg_host_regs_fops = {
	.open		= ufsdbg_host_regs_open,
	.read		= seq_read,
};

static int ufsdbg_show_hba_show(struct seq_file *file, void *data)
{
	struct ufs_hba *hba = (struct ufs_hba *)file->private;

	seq_printf(file, "hba->outstanding_tasks = 0x%x\n",
			(u32)hba->outstanding_tasks);
	seq_printf(file, "hba->outstanding_reqs = 0x%x\n",
			(u32)hba->outstanding_reqs);

	seq_printf(file, "hba->capabilities = 0x%x\n", hba->capabilities);
	seq_printf(file, "hba->nutrs = %d\n", hba->nutrs);
	seq_printf(file, "hba->nutmrs = %d\n", hba->nutmrs);
	seq_printf(file, "hba->ufs_version = 0x%x\n", hba->ufs_version);
	seq_printf(file, "hba->irq = 0x%x\n", hba->irq);
	seq_printf(file, "hba->auto_bkops_enabled = %d\n",
			hba->auto_bkops_enabled);

	seq_printf(file, "hba->ufshcd_state = 0x%x\n", hba->ufshcd_state);
	seq_printf(file, "hba->eh_flags = 0x%x\n", hba->eh_flags);
	seq_printf(file, "hba->intr_mask = 0x%x\n", hba->intr_mask);
	seq_printf(file, "hba->ee_ctrl_mask = 0x%x\n", hba->ee_ctrl_mask);

	/* HBA Errors */
	seq_printf(file, "hba->errors = 0x%x\n", hba->errors);
	seq_printf(file, "hba->uic_error = 0x%x\n", hba->uic_error);
	seq_printf(file, "hba->saved_err = 0x%x\n", hba->saved_err);
	seq_printf(file, "hba->saved_uic_err = 0x%x\n", hba->saved_uic_err);

	return 0;
}

static int ufsdbg_show_hba_open(struct inode *inode, struct file *file)
{
	return single_open(file, ufsdbg_show_hba_show, inode->i_private);
}

static const struct file_operations ufsdbg_show_hba_fops = {
	.open		= ufsdbg_show_hba_open,
	.read		= seq_read,
};

void ufsdbg_add_debugfs(struct ufs_hba *hba)
{
	if (!hba) {
		dev_err(hba->dev, "%s: NULL hba, exiting", __func__);
		goto err_no_root;
	}

	hba->debugfs_files.debugfs_root = debugfs_create_dir("ufs", NULL);
	if (IS_ERR(hba->debugfs_files.debugfs_root))
		/* Don't complain -- debugfs just isn't enabled */
		goto err_no_root;
	if (!hba->debugfs_files.debugfs_root) {
		/*
		 * Complain -- debugfs is enabled, but it failed to
		 * create the directory
		 */
		dev_err(hba->dev,
			"%s: NULL debugfs root directory, exiting", __func__);
		goto err_no_root;
	}

	hba->debugfs_files.tag_stats =
		debugfs_create_file("tag_stats", S_IRUSR,
					   hba->debugfs_files.debugfs_root, hba,
					   &ufsdbg_tag_stats_fops);
	if (!hba->debugfs_files.tag_stats) {
		dev_err(hba->dev, "%s:  NULL tag stats file, exiting",
			__func__);
		goto err;
	}

	if (ufshcd_init_tag_statistics(hba)) {
		dev_err(hba->dev, "%s: Error initializing tag stats",
			__func__);
		goto err;
	}

	hba->debugfs_files.host_regs = debugfs_create_file("host_regs", S_IRUSR,
				hba->debugfs_files.debugfs_root, hba,
				&ufsdbg_host_regs_fops);
	if (!hba->debugfs_files.host_regs) {
		dev_err(hba->dev, "%s:  NULL hcd regs file, exiting", __func__);
		goto err;
	}

	hba->debugfs_files.show_hba = debugfs_create_file("show_hba", S_IRUSR,
				hba->debugfs_files.debugfs_root, hba,
				&ufsdbg_show_hba_fops);
	if (!hba->debugfs_files.show_hba) {
		dev_err(hba->dev, "%s:  NULL hba file, exiting", __func__);
		goto err;
	}

	return;

err:
	debugfs_remove_recursive(hba->debugfs_files.debugfs_root);
	hba->debugfs_files.debugfs_root = NULL;
err_no_root:
	dev_err(hba->dev, "%s: failed to initialize debugfs\n", __func__);
}

void ufsdbg_remove_debugfs(struct ufs_hba *hba)
{
	debugfs_remove_recursive(hba->debugfs_files.debugfs_root);
	kfree(hba->ufs_stats.tag_stats);

}
