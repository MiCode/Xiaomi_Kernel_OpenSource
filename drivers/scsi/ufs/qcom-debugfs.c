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
#include <linux/scsi/ufs/ufs-qcom.h>
#include "qcom-debugfs.h"

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
