// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

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
static int npu_debug_reg_open(struct inode *inode, struct file *file);
static int npu_debug_reg_release(struct inode *inode, struct file *file);
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
static struct npu_device *g_npu_dev;

static const struct file_operations npu_reg_fops = {
	.open = npu_debug_reg_open,
	.release = npu_debug_reg_release,
	.read = npu_debug_reg_read,
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
	return 0;
}

static int npu_debug_reg_open(struct inode *inode, struct file *file)
{
	struct npu_debugfs_reg_ctx *reg_ctx;

	reg_ctx = kzalloc(sizeof(*reg_ctx), GFP_KERNEL);
	if (!reg_ctx)
		return -ENOMEM;

	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	reg_ctx->npu_dev = inode->i_private;
	file->private_data = reg_ctx;
	return 0;
}

static int npu_debug_reg_release(struct inode *inode, struct file *file)
{
	struct npu_debugfs_reg_ctx *reg_ctx = file->private_data;

	kfree(reg_ctx->buf);
	kfree(reg_ctx);
	file->private_data = NULL;
	return 0;
}

/* -------------------------------------------------------------------------
 * Function Implementations - Reg Read/Write
 * -------------------------------------------------------------------------
 */
static ssize_t npu_debug_reg_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct npu_debugfs_reg_ctx *reg_ctx = file->private_data;
	struct npu_device *npu_dev = reg_ctx->npu_dev;
	struct npu_debugfs_ctx *debugfs;
	size_t len;

	debugfs = &npu_dev->debugfs_ctx;

	if (debugfs->reg_cnt == 0)
		return 0;

	if (!reg_ctx->buf) {
		char dump_buf[64];
		char *ptr;
		int cnt, tot, off;

		reg_ctx->buf_len = sizeof(dump_buf) *
			DIV_ROUND_UP(debugfs->reg_cnt, ROW_BYTES);
		reg_ctx->buf = kzalloc(reg_ctx->buf_len, GFP_KERNEL);

		if (!reg_ctx->buf)
			return -ENOMEM;

		ptr = npu_dev->core_io.base + debugfs->reg_off;
		tot = 0;
		off = (int)debugfs->reg_off;

		if (npu_enable_core_power(npu_dev))
			return -EPERM;

		for (cnt = debugfs->reg_cnt * 4; cnt > 0; cnt -= ROW_BYTES) {
			hex_dump_to_buffer(ptr, min(cnt, ROW_BYTES),
					   ROW_BYTES, GROUP_BYTES, dump_buf,
					   sizeof(dump_buf), false);
			len = scnprintf(reg_ctx->buf + tot,
				reg_ctx->buf_len - tot, "0x%08x: %s\n",
				((int) (unsigned long) ptr) -
				((int) (unsigned long) npu_dev->core_io.base),
				dump_buf);

			ptr += ROW_BYTES;
			tot += len;
			if (tot >= reg_ctx->buf_len)
				break;
		}
		npu_disable_core_power(npu_dev);

		reg_ctx->buf_len = tot;
	}

	if (*ppos >= reg_ctx->buf_len)
		return 0; /* done reading */

	len = min(count, reg_ctx->buf_len - (size_t) *ppos);
	NPU_DBG("read %zi %zi\n", count, reg_ctx->buf_len - (size_t) *ppos);
	if (copy_to_user(user_buf, reg_ctx->buf + *ppos, len)) {
		NPU_ERR("failed to copy to user\n");
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

	NPU_DBG("npu_dev %pK %pK\n", npu_dev, g_npu_dev);
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
	NPU_DBG("reg off = %zx, %d cnt=%d\n", off, reg_cnt, cnt);
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

	NPU_DBG("npu_dev %pK %pK\n", npu_dev, g_npu_dev);
	npu_dev = g_npu_dev;
	debugfs = &npu_dev->debugfs_ctx;

	if (*ppos)
		return 0;	/* the end */

	len = scnprintf(buf, sizeof(buf), "offset=0x%08x cnt=%d\n",
		debugfs->reg_off, debugfs->reg_cnt);
	len = min(len, count);

	if (copy_to_user(user_buf, buf, len)) {
		NPU_ERR("failed to copy to user\n");
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

	NPU_DBG("npu_dev %pK %pK\n", npu_dev, g_npu_dev);
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
			NPU_ERR("failed to copy to user\n");
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

	NPU_DBG("npu_dev %pK %pK\n", npu_dev, g_npu_dev);
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
		NPU_INFO("triggering fw_init\n");
		if (enable_fw(npu_dev) != 0)
			NPU_INFO("error in fw_init\n");
	} else if (strcmp(buf, "off") == 0) {
		NPU_INFO("triggering fw_deinit\n");
		disable_fw(npu_dev);
	} else if (strcmp(buf, "loopback") == 0) {
		NPU_DBG("loopback test\n");
		rc = npu_host_loopback_test(npu_dev);
		NPU_DBG("loopback test end: %d\n", rc);
	} else {
		rc = kstrtou32(buf, 10, &val);
		if (rc) {
			NPU_ERR("Invalid input for power level settings\n");
		} else {
			val = min(val, npu_dev->pwrctrl.max_pwrlevel);
			npu_dev->pwrctrl.active_pwrlevel = val;
			NPU_INFO("setting power state to %d\n", val);
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
		NPU_ERR("debugfs_create_dir for npu failed, error %ld\n",
			PTR_ERR(debugfs->root));
		return -ENODEV;
	}

	if (!debugfs_create_file("reg", 0644, debugfs->root,
		npu_dev, &npu_reg_fops)) {
		NPU_ERR("debugfs_create_file reg fail\n");
		goto err;
	}

	if (!debugfs_create_file("off", 0644, debugfs->root,
		npu_dev, &npu_off_fops)) {
		NPU_ERR("debugfs_create_file off fail\n");
		goto err;
	}

	if (!debugfs_create_file("log", 0644, debugfs->root,
		npu_dev, &npu_log_fops)) {
		NPU_ERR("debugfs_create_file log fail\n");
		goto err;
	}

	if (!debugfs_create_file("ctrl", 0644, debugfs->root,
		npu_dev, &npu_ctrl_fops)) {
		NPU_ERR("debugfs_create_file ctrl fail\n");
		goto err;
	}

	if (!debugfs_create_bool("sys_cache_disable", 0644,
		debugfs->root, &(host_ctx->sys_cache_disable))) {
		NPU_ERR("debugfs_creat_bool fail for sys cache\n");
		goto err;
	}

	if (!debugfs_create_bool("auto_pil_disable", 0644,
		debugfs->root, &(host_ctx->auto_pil_disable))) {
		NPU_ERR("debugfs_creat_bool fail for auto pil\n");
		goto err;
	}

	if (!debugfs_create_u32("fw_dbg_mode", 0644,
		debugfs->root, &(host_ctx->fw_dbg_mode))) {
		NPU_ERR("debugfs_create_u32 fail for fw_dbg_mode\n");
		goto err;
	}

	if (!debugfs_create_u32("fw_state", 0444,
		debugfs->root, &(host_ctx->fw_state))) {
		NPU_ERR("debugfs_create_u32 fail for fw_state\n");
		goto err;
	}

	if (!debugfs_create_u32("pwr_level", 0444,
		debugfs->root, &(pwr->active_pwrlevel))) {
		NPU_ERR("debugfs_create_u32 fail for pwr_level\n");
		goto err;
	}

	if (!debugfs_create_u32("exec_flags", 0644,
		debugfs->root, &(host_ctx->exec_flags_override))) {
		NPU_ERR("debugfs_create_u32 fail for exec_flags\n");
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
