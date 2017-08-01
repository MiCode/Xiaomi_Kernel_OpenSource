/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include "main.h"
#include "debug.h"

#define CNSS_IPC_LOG_PAGES		32

void *cnss_ipc_log_context;

static int cnss_pin_connect_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *cnss_priv = s->private;

	seq_puts(s, "Pin connect results\n");
	seq_printf(s, "FW power pin result: %04x\n",
		   cnss_priv->pin_result.fw_pwr_pin_result);
	seq_printf(s, "FW PHY IO pin result: %04x\n",
		   cnss_priv->pin_result.fw_phy_io_pin_result);
	seq_printf(s, "FW RF pin result: %04x\n",
		   cnss_priv->pin_result.fw_rf_pin_result);
	seq_printf(s, "Host pin result: %04x\n",
		   cnss_priv->pin_result.host_pin_result);
	seq_puts(s, "\n");

	return 0;
}

static int cnss_pin_connect_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_pin_connect_show, inode->i_private);
}

static const struct file_operations cnss_pin_connect_fops = {
	.read		= seq_read,
	.release	= single_release,
	.open		= cnss_pin_connect_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_stats_show_state(struct seq_file *s,
				 struct cnss_plat_data *plat_priv)
{
	enum cnss_driver_state i;
	int skip = 0;
	unsigned long state;

	seq_printf(s, "\nState: 0x%lx(", plat_priv->driver_state);
	for (i = 0, state = plat_priv->driver_state; state != 0;
	     state >>= 1, i++) {
		if (!(state & 0x1))
			continue;

		if (skip++)
			seq_puts(s, " | ");

		switch (i) {
		case CNSS_QMI_WLFW_CONNECTED:
			seq_puts(s, "QMI_WLFW_CONNECTED");
			continue;
		case CNSS_FW_MEM_READY:
			seq_puts(s, "FW_MEM_READY");
			continue;
		case CNSS_FW_READY:
			seq_puts(s, "FW_READY");
			continue;
		case CNSS_COLD_BOOT_CAL:
			seq_puts(s, "COLD_BOOT_CAL");
			continue;
		case CNSS_DRIVER_LOADING:
			seq_puts(s, "DRIVER_LOADING");
			continue;
		case CNSS_DRIVER_UNLOADING:
			seq_puts(s, "DRIVER_UNLOADING");
			continue;
		case CNSS_DRIVER_PROBED:
			seq_puts(s, "DRIVER_PROBED");
			continue;
		case CNSS_DRIVER_RECOVERY:
			seq_puts(s, "DRIVER_RECOVERY");
			continue;
		case CNSS_FW_BOOT_RECOVERY:
			seq_puts(s, "FW_BOOT_RECOVERY");
			continue;
		case CNSS_DEV_ERR_NOTIFY:
			seq_puts(s, "DEV_ERR");
		}

		seq_printf(s, "UNKNOWN-%d", i);
	}
	seq_puts(s, ")\n");

	return 0;
}

static int cnss_stats_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	cnss_stats_show_state(s, plat_priv);

	return 0;
}

static int cnss_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_stats_show, inode->i_private);
}

static const struct file_operations cnss_stats_fops = {
	.read           = seq_read,
	.release        = single_release,
	.open           = cnss_stats_open,
	.owner          = THIS_MODULE,
	.llseek         = seq_lseek,
};

int cnss_debugfs_create(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("cnss", 0);
	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		cnss_pr_err("Unable to create debugfs %d\n", ret);
		goto out;
	}
	plat_priv->root_dentry = root_dentry;
	debugfs_create_file("pin_connect_result", 0644, root_dentry, plat_priv,
			    &cnss_pin_connect_fops);
	debugfs_create_file("stats", 0644, root_dentry, plat_priv,
			    &cnss_stats_fops);
out:
	return ret;
}

void cnss_debugfs_destroy(struct cnss_plat_data *plat_priv)
{
	debugfs_remove_recursive(plat_priv->root_dentry);
}

int cnss_debug_init(void)
{
	cnss_ipc_log_context = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
						      "cnss", 0);
	if (!cnss_ipc_log_context) {
		cnss_pr_err("Unable to create IPC log context!\n");
		return -EINVAL;
	}

	return 0;
}

void cnss_debug_deinit(void)
{
	if (cnss_ipc_log_context) {
		ipc_log_context_destroy(cnss_ipc_log_context);
		cnss_ipc_log_context = NULL;
	}
}
