/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
static ssize_t npu_debug_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t npu_debug_reg_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t npu_debug_off_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t npu_debug_off_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t npu_debug_log_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos);
static ssize_t npu_debug_ctrl_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos);

/* -------------------------------------------------------------------------
 * Variables
 * -------------------------------------------------------------------------
 */
struct npu_device *g_npu_dev;

static const struct file_operations npu_reg_fops = {
	.open = npu_debug_open,
	.release = npu_debug_release,
	.read = npu_debug_reg_read,
	.write = npu_debug_reg_write,
};

static const struct file_operations npu_off_fops = {
	.open = npu_debug_open,
	.release = npu_debug_release,
	.read = npu_debug_off_read,
	.write = npu_debug_off_write,
};

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
	struct npu_device *npu_dev = file->private_data;
	struct npu_debugfs_ctx *debugfs;

	debugfs = &npu_dev->debugfs_ctx;

	kfree(debugfs->buf);
	debugfs->buf_len = 0;
	debugfs->buf = NULL;
	return 0;
}

/* -------------------------------------------------------------------------
 * Function Implementations - Reg Read/Write
 * -------------------------------------------------------------------------
 */
static ssize_t npu_debug_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	size_t off;
	uint32_t data, cnt;
	struct npu_device *npu_dev = file->private_data;
	char buf[24];

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	cnt = sscanf(buf, "%zx %x", &off, &data);
	pr_debug("%s %s 0x%zx, 0x%08x\n", __func__, buf, off, data);

	return count;
	if (cnt < 2)
		return -EINVAL;

	if (npu_enable_core_power(npu_dev))
		return -EPERM;

	REGW(npu_dev, off, data);

	npu_disable_core_power(npu_dev);

	pr_debug("write: addr=%zx data=%x\n", off, data);

	return count;
}

static ssize_t npu_debug_reg_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct npu_device *npu_dev = file->private_data;
	struct npu_debugfs_ctx *debugfs;
	size_t len;

	debugfs = &npu_dev->debugfs_ctx;

	if (debugfs->reg_cnt == 0)
		return 0;

	if (!debugfs->buf) {
		char dump_buf[64];
		char *ptr;
		int cnt, tot, off;

		debugfs->buf_len = sizeof(dump_buf) *
			DIV_ROUND_UP(debugfs->reg_cnt, ROW_BYTES);
		debugfs->buf = kzalloc(debugfs->buf_len, GFP_KERNEL);

		if (!debugfs->buf)
			return -ENOMEM;

		ptr = npu_dev->npu_base + debugfs->reg_off;
		tot = 0;
		off = (int)debugfs->reg_off;

		if (npu_enable_core_power(npu_dev))
			return -EPERM;

		for (cnt = debugfs->reg_cnt * 4; cnt > 0; cnt -= ROW_BYTES) {
			hex_dump_to_buffer(ptr, min(cnt, ROW_BYTES),
					   ROW_BYTES, GROUP_BYTES, dump_buf,
					   sizeof(dump_buf), false);
			len = scnprintf(debugfs->buf + tot,
				debugfs->buf_len - tot, "0x%08x: %s\n",
				((int) (unsigned long) ptr) -
				((int) (unsigned long) npu_dev->npu_base),
				dump_buf);

			ptr += ROW_BYTES;
			tot += len;
			if (tot >= debugfs->buf_len)
				break;
		}
		npu_disable_core_power(npu_dev);

		debugfs->buf_len = tot;
	}

	if (*ppos >= debugfs->buf_len)
		return 0; /* done reading */

	len = min(count, debugfs->buf_len - (size_t) *ppos);
	pr_debug("read %zi %zi", count, debugfs->buf_len - (size_t) *ppos);
	if (copy_to_user(user_buf, debugfs->buf + *ppos, len)) {
		pr_err("failed to copy to user\n");
		return -EFAULT;
	}

	*ppos += len;	/* increase offset */
	return len;
}

/* -------------------------------------------------------------------------
 * Function Implementations - Offset Read/Write
 * -------------------------------------------------------------------------
 */
static ssize_t npu_debug_off_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	size_t off = 0;
	uint32_t cnt, reg_cnt;
	char buf[24];
	struct npu_device *npu_dev = file->private_data;
	struct npu_debugfs_ctx *debugfs;

	pr_debug("npu_dev %pK %pK\n", npu_dev, g_npu_dev);
	npu_dev = g_npu_dev;
	debugfs = &npu_dev->debugfs_ctx;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	cnt = sscanf(buf, "%zx %x", &off, &reg_cnt);
	if (cnt == 1)
		reg_cnt = DEFAULT_REG_DUMP_NUM;
	pr_debug("reg off = %zx, %d	cnt=%d\n", off, reg_cnt, cnt);
	if (cnt >= 1) {
		debugfs->reg_off = off;
		debugfs->reg_cnt = reg_cnt;
	}

	return count;
}

static ssize_t npu_debug_off_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	size_t len;
	char buf[64];
	struct npu_device *npu_dev = file->private_data;
	struct npu_debugfs_ctx *debugfs;

	pr_debug("npu_dev %pK %pK\n", npu_dev, g_npu_dev);
	npu_dev = g_npu_dev;
	debugfs = &npu_dev->debugfs_ctx;

	if (*ppos)
		return 0;	/* the end */

	len = scnprintf(buf, sizeof(buf), "offset=0x%08x cnt=%d\n",
		debugfs->reg_off, debugfs->reg_cnt);

	if (copy_to_user(user_buf, buf, len)) {
		pr_err("failed to copy to user\n");
		return -EFAULT;
	}

	*ppos += len;	/* increase offset */
	return len;
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
		if ((debugfs->log_read_index +
			debugfs->log_num_bytes_buffered) >
			debugfs->log_buf_size) {
			/* Wrap around case */
			uint32_t remaining_to_end = debugfs->log_buf_size -
				debugfs->log_read_index;
			uint8_t *src_addr = debugfs->log_buf +
				debugfs->log_read_index;
			uint8_t *dst_addr = user_buf;

			if (copy_to_user(dst_addr, src_addr,
				remaining_to_end)) {
				pr_err("%s failed to copy to user", __func__);
				return -EFAULT;
			}
			src_addr = debugfs->log_buf;
			dst_addr = user_buf + remaining_to_end;
			if (copy_to_user(dst_addr, src_addr,
				debugfs->log_num_bytes_buffered -
				remaining_to_end)) {
				pr_err("%s failed to copy to user", __func__);
				return -EFAULT;
			}
			debugfs->log_read_index =
				debugfs->log_num_bytes_buffered -
				remaining_to_end;
		} else {
			if (copy_to_user(user_buf, (debugfs->log_buf +
				debugfs->log_read_index),
				debugfs->log_num_bytes_buffered)) {
				pr_err("%s failed to copy to user", __func__);
				return -EFAULT;
			}
			debugfs->log_read_index +=
				debugfs->log_num_bytes_buffered;
			if (debugfs->log_read_index == debugfs->log_buf_size)
				debugfs->log_read_index = 0;
		}
		len = debugfs->log_num_bytes_buffered;
		debugfs->log_num_bytes_buffered = 0;
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
		fw_deinit(npu_dev, true);
	} else if (strcmp(buf, "ssr") == 0) {
		pr_info("trigger error irq\n");
		if (npu_enable_core_power(npu_dev))
			return -EPERM;

		REGW(npu_dev, NPU_MASTERn_ERROR_IRQ_SET(0), 2);
		npu_disable_core_power(npu_dev);
	} else if (strcmp(buf, "loopback") == 0) {
		pr_debug("loopback test\n");
		rc = npu_host_loopback_test(npu_dev);
		pr_debug("loopback test end: %d\n", rc);
	} else if (strcmp(buf, "0") == 0) {
		pr_info("setting power state to 0\n");
		npu_dev->pwrctrl.active_pwrlevel = 0;
	} else if (strcmp(buf, "1") == 0) {
		pr_info("setting power state to 1\n");
		npu_dev->pwrctrl.active_pwrlevel = 1;
	} else if (strcmp(buf, "2") == 0) {
		pr_info("setting power state to 2\n");
		npu_dev->pwrctrl.active_pwrlevel = 2;
	} else if (strcmp(buf, "3") == 0) {
		pr_info("setting power state to 3\n");
		npu_dev->pwrctrl.active_pwrlevel = 3;
	} else if (strcmp(buf, "4") == 0) {
		pr_info("setting power state to 4\n");
		npu_dev->pwrctrl.active_pwrlevel = 4;
	} else if (strcmp(buf, "5") == 0) {
		pr_info("setting power state to 5\n");
		npu_dev->pwrctrl.active_pwrlevel = 5;
	} else {
		pr_info("ctrl invalid value\n");
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

	if (!debugfs_create_file("reg", 0644, debugfs->root,
		npu_dev, &npu_reg_fops)) {
		pr_err("debugfs_create_file reg fail\n");
		goto err;
	}

	if (!debugfs_create_file("off", 0644, debugfs->root,
		npu_dev, &npu_off_fops)) {
		pr_err("debugfs_create_file off fail\n");
		goto err;
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

	if (!debugfs_create_u32("fw_state", 0444,
		debugfs->root, &(host_ctx->fw_state))) {
		pr_err("debugfs_creat_bool fail for fw_state\n");
		goto err;
	}

	if (!debugfs_create_u32("pwr_level", 0444,
		debugfs->root, &(pwr->active_pwrlevel))) {
		pr_err("debugfs_creat_bool fail for power level\n");
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
