/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>

#include "rs_base.h"
#include "rs_log.h"
#include "rs_trace.h"
#include "rs_usage.h"

struct dentry *rsm_debugfs_dir;

static int __init rs_init(void)
{
	struct proc_dir_entry *hps_dir = NULL;
	struct dentry *rs_debugfs_dir = NULL;

	RS_LOGI("[RS_MAIN] init\n");

	rs_debugfs_dir = debugfs_create_dir("resym", NULL);
	if (!rs_debugfs_dir) {
		RS_LOGE("debugfs_create_dir resym failed");
		goto err;
	}

	rsm_debugfs_dir = rs_debugfs_dir;

	rs_init_trace(rs_debugfs_dir);
	rs_usage_init(rs_debugfs_dir, hps_dir);

	return 0;
err:
	return -EFAULT;
}

static void __exit rs_exit(void)
{
}

module_init(rs_init);
module_exit(rs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Resource Symphony");
MODULE_AUTHOR("MediaTek Inc.");

