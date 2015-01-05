/*
 * Copyright (c) 2015, Linux Foundation. All rights reserved.
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
 */

#include <linux/debugfs.h>
#include "ufs-qcom.h"
#include "qcom-debugfs.h"
#include "debugfs.h"

static void ufs_qcom_dbg_remove_debugfs(struct ufs_qcom_host *host);

static int ufs_qcom_dbg_print_en_read(void *data, u64 *attr_val)
{
	struct ufs_qcom_host *host = data;

	if (!host)
		return -EINVAL;

	*attr_val = (u64)host->dbg_print_en;
	return 0;
}

static int ufs_qcom_dbg_print_en_set(void *data, u64 attr_id)
{
	struct ufs_qcom_host *host = data;

	if (!host)
		return -EINVAL;

	if (attr_id & ~UFS_QCOM_DBG_PRINT_ALL)
		return -EINVAL;

	host->dbg_print_en = (u32)attr_id;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(ufs_qcom_dbg_print_en_ops,
			ufs_qcom_dbg_print_en_read,
			ufs_qcom_dbg_print_en_set,
			"%llu\n");

static int ufs_qcom_dbg_dbg_regs_show(struct seq_file *file, void *data)
{
	struct ufs_qcom_host *host = (struct ufs_qcom_host *)file->private;
	bool dbg_print_reg = !!(host->dbg_print_en &
				UFS_QCOM_DBG_PRINT_REGS_EN);

	ufshcd_hold(host->hba, false);
	pm_runtime_get_sync(host->hba->dev);

	/* Temporarily override the debug print enable */
	host->dbg_print_en |= UFS_QCOM_DBG_PRINT_REGS_EN;
	ufs_qcom_print_hw_debug_reg_all(host->hba, file, ufsdbg_pr_buf_to_std);
	/* Restore previous debug print enable value */
	if (!dbg_print_reg)
		host->dbg_print_en &= ~UFS_QCOM_DBG_PRINT_REGS_EN;

	pm_runtime_put_sync(host->hba->dev);
	ufshcd_release(host->hba, false);

	return 0;
}

static int ufs_qcom_dbg_dbg_regs_open(struct inode *inode,
					      struct file *file)
{
	return single_open(file, ufs_qcom_dbg_dbg_regs_show,
				inode->i_private);
}

static const struct file_operations ufs_qcom_dbg_dbg_regs_desc = {
	.open		= ufs_qcom_dbg_dbg_regs_open,
	.read		= seq_read,
};

void ufs_qcom_dbg_add_debugfs(struct ufs_hba *hba, struct dentry *root)
{
	struct ufs_qcom_host *host;

	if (!hba || !hba->priv) {
		pr_err("%s: NULL host, exiting\n", __func__);
		return;
	}

	host = hba->priv;
	host->debugfs_files.debugfs_root = debugfs_create_dir("qcom", root);
	if (IS_ERR(host->debugfs_files.debugfs_root))
		/* Don't complain -- debugfs just isn't enabled */
		goto err_no_root;
	if (!host->debugfs_files.debugfs_root) {
		/*
		 * Complain -- debugfs is enabled, but it failed to
		 * create the directory
		 */
		dev_err(host->hba->dev,
			"%s: NULL debugfs root directory, exiting", __func__);
		goto err_no_root;
	}

	host->debugfs_files.dbg_print_en =
		debugfs_create_file("dbg_print_en", S_IRUSR | S_IWUSR,
				    host->debugfs_files.debugfs_root, host,
				    &ufs_qcom_dbg_print_en_ops);
	if (!host->debugfs_files.dbg_print_en) {
		dev_err(host->hba->dev,
			"%s: failed to create dbg_print_en debugfs entry\n",
			__func__);
		goto err;
	}

	host->debugfs_files.dbg_regs =
		debugfs_create_file("debug-regs", S_IRUSR,
				    host->debugfs_files.debugfs_root, host,
				    &ufs_qcom_dbg_dbg_regs_desc);
	if (!host->debugfs_files.dbg_regs) {
		dev_err(host->hba->dev,
			"%s: failed create dbg_regs debugfs entry\n",
			__func__);
		goto err;
	}

	return;

err:
	ufs_qcom_dbg_remove_debugfs(host);
err_no_root:
	dev_err(host->hba->dev, "%s: failed to initialize debugfs\n", __func__);
}

static void ufs_qcom_dbg_remove_debugfs(struct ufs_qcom_host *host)
{
	debugfs_remove_recursive(host->debugfs_files.debugfs_root);
	host->debugfs_files.debugfs_root = NULL;
}
