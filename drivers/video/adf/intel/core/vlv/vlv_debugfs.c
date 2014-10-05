/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Authors:	Deepak S <deepak.s@linux.intel.com>
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include "intel_adf.h"
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dpst.h>

static ssize_t
intel_dpst_disable_get(struct file *file, char __user *ubuf, size_t max,
		   loff_t *ppos)
{
	struct vlv_dc_config *vlv_config = file->private_data;
	char buf[] = "dpst read is not defined";
	u32 len;

	len = snprintf(buf, sizeof(buf),
			"%d\n",  vlv_config->dpst.kernel_disable);

	if (len > sizeof(buf))
		len = sizeof(buf);
	return simple_read_from_buffer(ubuf, max, ppos, buf, len);
}


static ssize_t
intel_dpst_disable_set(struct file *file, const char __user *ubuf, size_t cnt,
		    loff_t *ppos)
{
	struct vlv_dc_config *config = file->private_data;
	char buf[20];
	int ret;
	u32 val;

	if (cnt > 0) {
		if (cnt > sizeof(buf) - 1)
			return -EINVAL;

		if (copy_from_user(buf, ubuf, cnt))
			return -EFAULT;

		buf[cnt] = 0;

		ret = kstrtoul(buf, 0,
				(unsigned long *)&val);
		if (ret)
			return -EINVAL;

		pr_err("Setting DPST disable %s\n",
			 val ? "true" : "false");

		vlv_dpst_set_kernel_disable(config, val);

	}

	return cnt;
}

static const struct file_operations intel_dpst_disable_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = intel_dpst_disable_get,
	.write = intel_dpst_disable_set,
	.llseek = default_llseek,
};

static ssize_t
intel_dpst_status_get(struct file *file, char __user *ubuf, size_t max,
		   loff_t *ppos)
{
	struct vlv_dc_config *config = file->private_data;
	char buf[800];
	u32 len = 0;
	int i;
	const int columns = 4;
	u32 blm_hist_ctl, blm_hist_guard;

	blm_hist_ctl = REG_READ(config->dpst.reg.blm_hist_ctl);
	blm_hist_guard = REG_READ(config->dpst.reg.blm_hist_guard);

	len = scnprintf(buf, sizeof(buf),
			"histogram logic: %s\n",
			blm_hist_ctl & IE_HISTOGRAM_ENABLE ?
			"enabled" : "disabled");

	len += scnprintf(&buf[len], (sizeof(buf) - len),
			"histogram interrupts: %s\n",
			blm_hist_guard & HISTOGRAM_INTERRUPT_ENABLE ?
			"enabled" : "disabled");

	len += scnprintf(&buf[len], (sizeof(buf) - len),
			"backlight adjustment: %u%%\n",
			config->dpst.blc_adjustment * 100 / DPST_MAX_FACTOR);

	len += scnprintf(&buf[len], (sizeof(buf) - len),
			"IE modification table: %s\n",
			blm_hist_ctl & IE_MOD_TABLE_ENABLE ?
			"enabled" : "disabled");

	blm_hist_ctl |= BIN_REG_FUNCTION_SELECT_IE;
	blm_hist_ctl &= ~BIN_REGISTER_INDEX_MASK;
	REG_WRITE(BLM_HIST_CTL, blm_hist_ctl);

	len += scnprintf(&buf[len], (sizeof(buf) - len),
			 "IE modification table values...");
	for (i = 0; i < DPST_DIET_ENTRY_COUNT; i++) {
		if (i % columns == 0)
			len += scnprintf(&buf[len], (sizeof(buf) - len),
				"\nbins %02d-%02d:", i, i + columns - 1);
		len += scnprintf(&buf[len], (sizeof(buf) - len),
			"%10x", REG_READ(config->dpst.reg.blm_hist_bin));
	}
	len += scnprintf(&buf[len], (sizeof(buf) - len), "\n");


	return simple_read_from_buffer(ubuf, max, ppos, buf, len);
}


static ssize_t
intel_dpst_status_set(struct file *file, const char __user *ubuf, size_t cnt,
		    loff_t *ppos)
{
	return cnt;
}

static const struct file_operations intel_dpst_status_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = intel_dpst_status_get,
	.write = intel_dpst_status_set,
	.llseek = default_llseek,
};

int vlv_debugfs_init(struct vlv_dc_config *vlv_config)
{
	struct dentry		*root;
	struct dentry		*file;
	int			ret;

	root = debugfs_create_dir("intel_adf", NULL);
	if (!root) {
		ret = -ENOMEM;
		goto err0;
	}

	file = debugfs_create_file("intel_dpst_status", S_IRUGO, root,
			vlv_config, &intel_dpst_status_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file("intel_dpst_disable", S_IRUGO, root,
			vlv_config, &intel_dpst_disable_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	vlv_config->debugfs_root = root;

	return 0;

err1:
	debugfs_remove_recursive(root);

err0:
	return ret;
}

void vlv_debugfs_teardown(struct vlv_dc_config *config)
{
	debugfs_remove_recursive(config->debugfs_root);
}
