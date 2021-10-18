// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/* -------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------
 */
#include <linux/debugfs.h>

#include "npu_hw.h"
#include "npu_hw_access.h"
#include "npu_common.h"

/* -------------------------------------------------------------------------
 * Defines
 * -------------------------------------------------------------------------
 */
#define NPU_LOG_BUF_SIZE 4096

/* -------------------------------------------------------------------------
 * Function Prototypes
 * -------------------------------------------------------------------------
 */
static int npu_debug_open(struct inode *inode, struct file *file);
static int npu_debug_release(struct inode *inode, struct file *file);
static ssize_t npu_debug_log_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t npu_debug_ctrl_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos);

/* -------------------------------------------------------------------------
 * Variables
 * -------------------------------------------------------------------------
 */
static struct npu_device *g_npu_dev;

static const struct file_operations npu_log_fops = {
	.open = npu_debug_open,
	.release = npu_debug_release,
	.read = npu_debug_log_read,
	.write = NULL,
};

static const struct file_operations npu_ctrl_fops = {
	.open = npu_debug_open,
	.release = npu_debug_release,
	.read = NULL,
	.write = npu_debug_ctrl_write,
};

/* -------------------------------------------------------------------------
 * Function Implementations
 * -------------------------------------------------------------------------
 */
static int npu_debug_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static int npu_debug_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* -------------------------------------------------------------------------
 * Function Implementations - DebugFS Log
 * -------------------------------------------------------------------------
 */
static ssize_t npu_debug_log_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	size_t len = 0;
	struct npu_device *npu_dev = file->private_data;
	struct npu_debugfs_ctx *debugfs;

	pr_debug("npu_dev %pK %pK\n", npu_dev, g_npu_dev);
	npu_dev = g_npu_dev;
	debugfs = &npu_dev->debugfs_ctx;

	/* mutex log lock */
	mutex_lock(&debugfs->log_lock);

	if (debugfs->log_num_bytes_buffered != 0) {
		len = min(debugfs->log_num_bytes_buffered,
			debugfs->log_buf_size - debugfs->log_read_index);
		len = min(count, len);
		if (copy_to_user(user_buf, (debugfs->log_buf +
			debugfs->log_read_index), len)) {
			pr_err("%s failed to copy to user\n", __func__);
			mutex_unlock(&debugfs->log_lock);
			return -EFAULT;
		}
		debugfs->log_read_index += len;
		if (debugfs->log_read_index == debugfs->log_buf_size)
			debugfs->log_read_index = 0;

		debugfs->log_num_bytes_buffered -= len;
		*ppos += len;
	}

	/* mutex log unlock */
	mutex_unlock(&debugfs->log_lock);

	return len;
}

/* -------------------------------------------------------------------------
 * Function Implementations - DebugFS Control
 * -------------------------------------------------------------------------
 */
static ssize_t npu_debug_ctrl_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[24];
	struct npu_device *npu_dev = file->private_data;
	struct npu_debugfs_ctx *debugfs;
	int32_t rc = 0;
	uint32_t val;

	pr_debug("npu_dev %pK %pK\n", npu_dev, g_npu_dev);
	npu_dev = g_npu_dev;
	debugfs = &npu_dev->debugfs_ctx;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	if (count >= 2)
		buf[count-1] = 0;/* remove line feed */

	if (strcmp(buf, "on") == 0) {
		pr_info("triggering fw_init\n");
		if (fw_init(npu_dev) != 0)
			pr_info("error in fw_init\n");
	} else if (strcmp(buf, "off") == 0) {
		pr_info("triggering fw_deinit\n");
		fw_deinit(npu_dev, false, true);
	} else if (strcmp(buf, "ssr") == 0) {
		pr_info("trigger error irq\n");
		if (npu_enable_core_power(npu_dev))
			return -EPERM;

		REGW(npu_dev, NPU_MASTERn_ERROR_IRQ_SET(1), 2);
		REGW(npu_dev, NPU_MASTERn_ERROR_IRQ_SET(0), 2);
		npu_disable_core_power(npu_dev);
	} else if (strcmp(buf, "ssr_wdt") == 0) {
		pr_info("trigger wdt irq\n");
		npu_disable_post_pil_clocks(npu_dev);
	} else if (strcmp(buf, "loopback") == 0) {
		pr_debug("loopback test\n");
		rc = npu_host_loopback_test(npu_dev);
		pr_debug("loopback test end: %d\n", rc);
	} else {
		rc = kstrtou32(buf, 10, &val);
		if (rc) {
			pr_err("Invalid input for power level settings\n");
		} else {
			val = min(val, npu_dev->pwrctrl.max_pwrlevel);
			npu_dev->pwrctrl.active_pwrlevel = val;
			pr_info("setting power state to %d\n", val);
		}
	}

	return count;
}
/* -------------------------------------------------------------------------
 * Function Implementations - DebugFS
 * -------------------------------------------------------------------------
 */
int npu_debugfs_init(struct npu_device *npu_dev)
{
	struct npu_debugfs_ctx *debugfs = &npu_dev->debugfs_ctx;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;

	g_npu_dev = npu_dev;

	debugfs->root = debugfs_create_dir("npu", NULL);
	if (IS_ERR_OR_NULL(debugfs->root)) {
		pr_err("debugfs_create_dir for npu failed, error %ld\n",
			PTR_ERR(debugfs->root));
		return -ENODEV;
	}

	if (!debugfs_create_file("log", 0644, debugfs->root,
		npu_dev, &npu_log_fops)) {
		pr_err("debugfs_create_file log fail\n");
		goto err;
	}

	if (!debugfs_create_file("ctrl", 0644, debugfs->root,
		npu_dev, &npu_ctrl_fops)) {
		pr_err("debugfs_create_file ctrl fail\n");
		goto err;
	}

	if (!debugfs_create_bool("sys_cache_disable", 0644,
		debugfs->root, &(host_ctx->sys_cache_disable))) {
		pr_err("debugfs_creat_bool fail for sys cache\n");
		goto err;
	}

	if (!debugfs_create_u32("fw_dbg_mode", 0644,
		debugfs->root, &(host_ctx->fw_dbg_mode))) {
		pr_err("debugfs_create_u32 fail for fw_dbg_mode\n");
		goto err;
	}

	if (!debugfs_create_u32("fw_state", 0444,
		debugfs->root, &(host_ctx->fw_state))) {
		pr_err("debugfs_create_u32 fail for fw_state\n");
		goto err;
	}

	if (!debugfs_create_u32("pwr_level", 0444,
		debugfs->root, &(pwr->active_pwrlevel))) {
		pr_err("debugfs_create_u32 fail for pwr_level\n");
		goto err;
	}

	if (!debugfs_create_u32("exec_flags", 0644,
		debugfs->root, &(host_ctx->exec_flags_override))) {
		pr_err("debugfs_create_u32 fail for exec_flags\n");
		goto err;
	}

	debugfs->log_num_bytes_buffered = 0;
	debugfs->log_read_index = 0;
	debugfs->log_write_index = 0;
	debugfs->log_buf_size = NPU_LOG_BUF_SIZE;
	debugfs->log_buf = kzalloc(debugfs->log_buf_size, GFP_KERNEL);
	if (!debugfs->log_buf)
		goto err;
	mutex_init(&debugfs->log_lock);

	return 0;

err:
	npu_debugfs_deinit(npu_dev);
	return -ENODEV;
}

void npu_debugfs_deinit(struct npu_device *npu_dev)
{
	struct npu_debugfs_ctx *debugfs = &npu_dev->debugfs_ctx;

	debugfs->log_num_bytes_buffered = 0;
	debugfs->log_read_index = 0;
	debugfs->log_write_index = 0;
	debugfs->log_buf_size = 0;
	kfree(debugfs->log_buf);

	if (!IS_ERR_OR_NULL(debugfs->root)) {
		debugfs_remove_recursive(debugfs->root);
		debugfs->root = NULL;
	}
}
