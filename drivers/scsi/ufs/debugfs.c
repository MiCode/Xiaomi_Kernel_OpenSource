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
