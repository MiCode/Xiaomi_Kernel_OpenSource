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
 */

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/sizes.h>

#include "vpu_debug.h"
#include "vpu_v4l2.h"
#include "vpu_ioctl_internal.h"
#include "vpu_channel.h"
#include "vpu_hfi.h"
#include "vpu_bus_clock.h"

#define BUF_SIZE	(SZ_4K)
#define RW_MODE		(S_IRUSR | S_IWUSR)


u32 vpu_pil_timeout = 500; /* ms */
u32 vpu_shutdown_delay = 1000; /* ms */
u32 vpu_ipc_timeout = 1000; /* ms */

struct fw_log_info {
	/* wq woken by hfi layer when fw log msg received */
	wait_queue_head_t wq;
	/* wq only woken by hfi layer if this flag set */
	int wake_up_request;
	/* buf used for formatting log msgs */
	char *fmt_buf;
};

/* SMEM controller data */
struct smem_ctrl_data {
	/* number of bytes to read */
	u32 size;
	/* offset from shared memory base address */
	u32 offset;
};

static struct smem_ctrl_data smem_ctrl = {
	.size = 1024,
	.offset = 0x00000000
};


static struct fw_log_info fw_log;

void vpu_wakeup_fw_logging_wq(void)
{
	if (fw_log.wake_up_request) {
		fw_log.wake_up_request = 0;
		wake_up_interruptible_all(&fw_log.wq);
	}
}

static int open_fw_log(struct inode *inode, struct file *file)
{
	char *fmt_buf;

	fw_log.wake_up_request = 0;
	init_waitqueue_head(&fw_log.wq);

	fmt_buf = kzalloc(BUF_SIZE, GFP_KERNEL);
	if (unlikely(!fmt_buf)) {
		pr_err("Failed to allocated fmt_buf\n");
		return -ENOMEM;
	}

	fw_log.fmt_buf = fmt_buf;
	return 0;
}



static ssize_t read_fw_log(struct file *file, char __user *user_buf,
	size_t len, loff_t *ppos)
{
	int ret;
	int bytes_read = 0;
	int buf_size = len;

	/* fmt buffer is only BUF_SIZE */
	buf_size = min(buf_size, BUF_SIZE);

	do {
		/* read data into user buffer */
		bytes_read = vpu_hw_sys_print_log(user_buf,
				fw_log.fmt_buf, buf_size);

		/*
		 * The Logging queue might not be ready yet. If that is the case
		 * try again after queue init.
		 * Also, when we read 0 bytes we wait until firmware writes
		 * something in the logging queue.
		 */
		if ((bytes_read == -EAGAIN) || (bytes_read == 0)) {
			fw_log.wake_up_request = 1;
			ret = wait_event_interruptible(fw_log.wq,
					fw_log.wake_up_request == 0);
			if (ret < 0)
				return ret;
		} else if (bytes_read < 0) {
			pr_err("Error: bytes_read=%d\n", bytes_read);
			return bytes_read;
		}

	} while ((bytes_read == -EAGAIN) || (bytes_read == 0));

	return bytes_read;
}

static int release_fw_log(struct inode *inode, struct file *file)
{
	kfree(fw_log.fmt_buf);
	fw_log.fmt_buf = NULL;

	return 0;
}

static const struct file_operations fw_logging_ops = {
	.open = open_fw_log,
	.read = read_fw_log,
	.release = release_fw_log,
};

static ssize_t read_queue_state(struct file *file, char __user *user_buf,
	size_t len, loff_t *ppos)
{
	char *dbg_buf;
	size_t size, ret;

	dbg_buf = kzalloc(BUF_SIZE, GFP_KERNEL);
	if (!dbg_buf) {
		pr_err("failed to alloc\n");
		return -ENOMEM;
	}

	size = vpu_hfi_print_queues(dbg_buf, BUF_SIZE);
	ret = simple_read_from_buffer(user_buf, len, ppos, dbg_buf, size);

	kfree(dbg_buf);
	return ret;
}

static const struct file_operations queue_state_ops = {
	.open = simple_open,
	.read = read_queue_state,
};

static ssize_t read_csr_regs(struct file *file, char __user *user_buf,
	size_t len, loff_t *ppos)
{
	char *dbg_buf;
	size_t size, ret;

	dbg_buf = kzalloc(BUF_SIZE, GFP_KERNEL);
	if (!dbg_buf) {
		pr_err("failed to alloc\n");
		return -ENOMEM;
	}

	size = vpu_hw_dump_csr_regs(dbg_buf, BUF_SIZE);
	if (size > 0)
		ret = simple_read_from_buffer(user_buf, len, ppos,
				dbg_buf, size);
	else
		ret = -EIO; /* device not readable */
	kfree(dbg_buf);
	return ret;
}

static const struct file_operations csr_regs_ops = {
	.open = simple_open,
	.read = read_csr_regs,
};

static void debugon(void)
{
	/* make the timeout very long */
	vpu_pil_timeout = 10000000;
	vpu_ipc_timeout = 10000000;

	vpu_hfi_set_watchdog(0);
}

static void debugoff(void)
{
	/* enable the timeouts */
	vpu_pil_timeout = 500;
	vpu_ipc_timeout = 1000;
	vpu_hfi_set_watchdog(1);
}

static ssize_t write_cmd(struct file *file, const char __user *user_buf,
	size_t len, loff_t *ppos)
{
	char buf[10];
	char *cmp;

	len = min(len, sizeof(buf) - 1);
	if (copy_from_user(&buf[0], user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	cmp = strstrip(buf);

	if (strcmp(cmp, "crash") == 0)
		vpu_hw_sys_cmd_ext(VPU_SYS_CMD_DEBUG_CRASH, NULL, 0);
	else if (strcmp(cmp, "svs") == 0)
		vpu_hw_sys_set_power_mode(VPU_POWER_SVS);
	else if (strcmp(cmp, "nominal") == 0)
		vpu_hw_sys_set_power_mode(VPU_POWER_NOMINAL);
	else if (strcmp(cmp, "turbo") == 0)
		vpu_hw_sys_set_power_mode(VPU_POWER_TURBO);
	else if (strcmp(cmp, "dynamic") == 0)
		vpu_hw_sys_set_power_mode(VPU_POWER_DYNAMIC);
	else if (strcmp(cmp, "debugon") == 0)
		debugon();
	else if (strcmp(cmp, "debugoff") == 0)
		debugoff();

	return len;
}

static ssize_t read_cmd(struct file *file, char __user *user_buf,
	size_t len, loff_t *ppos)
{
	char str[] =
		"Usage: echo <cmd> > <this_file> to send a command to VPU\n";

	return simple_read_from_buffer(user_buf, len, ppos, str, sizeof(str));
}

static const struct file_operations vpu_cmd_ops = {
	.open = simple_open,
	.write = write_cmd,
	.read = read_cmd,
};

static int smem_data_show(struct seq_file *m, void *private)
{
	char cbuf[SZ_64];
	struct smem_ctrl_data *smem = m->private;
	u32 offset = smem->offset;

	if (((offset >> 2) << 2) != offset) {
		seq_printf(m, "Error: offset (0x%x) must be a multiple of 4!\n",
				offset);
		goto smem_exit;
	}
	/*
	 * Print each memory line (4 32-bit words) containing the incremented
	 * offset. Stop reading if lower layer does not print anymore (or error)
	 */
	for (; offset <= smem->offset + smem->size; offset += 4 * sizeof(u32)) {
		int ret;
		ret = vpu_hfi_dump_smem_line(cbuf, sizeof(cbuf), offset);
		if (ret > 0) {
			seq_printf(m, "%s", cbuf);
		} else {
			if (ret == -EACCES)
				pr_err("Cannot read outside of VPU mem!\n");
			if (ret == -ENOMEM)
				pr_err("cbuf too small!\n");

			/* break 'for' loop if ret <= 0 */
			break;
		}
	}

smem_exit:
	return 0;
}

static int smem_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, smem_data_show, inode->i_private);
}

static const struct file_operations smem_data_ops = {
	.open = smem_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static struct dentry *init_smem_dir(struct dentry *root)
{
	struct dentry *smem, *attr;

	smem = debugfs_create_dir("smem", root);
	if (IS_ERR_OR_NULL(root)) {
		pr_err("Failed to create smem directory\n");
		goto smem_dir_err;
	}

	attr = debugfs_create_x32("offset", RW_MODE, smem, &smem_ctrl.offset);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create smem/offset entry\n");
		goto smem_dir_err;
	}

	attr = debugfs_create_u32("size", RW_MODE, smem, &smem_ctrl.size);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create smem/size entry\n");
		goto smem_dir_err;
	}

	attr = debugfs_create_file("data", RW_MODE, smem, &smem_ctrl,
			&smem_data_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create smem/data entry\n");
		goto smem_dir_err;
	}

	return smem;
smem_dir_err:
	return NULL;
}

static struct vpu_client *debug_client;

static ssize_t write_client(struct file *file, const char __user *user_buf,
	size_t len, loff_t *ppos)
{
	char buf[10];
	char *cmp;
	struct vpu_dev_core *core = file->private_data;

	if (!core)
		return -ENODEV;

	len = min(len, sizeof(buf) - 1);
	if (copy_from_user(&buf[0], user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	cmp = strstrip(buf);

	if (strcmp(cmp, "get") == 0) {
		if (!debug_client)
			debug_client = vpu_open_kernel_client(core);
	} else if (strcmp(cmp, "put") == 0) {
		if (debug_client) {
			vpu_close_client(debug_client);
			debug_client = NULL;
		}
	} else {
		return -EINVAL;
	}

	return len;
}

static ssize_t read_client(struct file *file, char __user *user_buf,
	size_t len, loff_t *ppos)
{
	char str[] =
		"Usage: echo get/put > <this_file> to inc/dec VPU client\n";

	return simple_read_from_buffer(user_buf, len, ppos, str, sizeof(str));
}

static const struct file_operations vpu_client_ops = {
	.open = simple_open,
	.write = write_client,
	.read = read_client,
};

struct dentry *init_vpu_debugfs(struct vpu_dev_core *core)
{
	struct dentry *attr;
	struct dentry *root = debugfs_create_dir(VPU_DRV_NAME, NULL);

	if (IS_ERR_OR_NULL(root)) {
		if (PTR_ERR(root) != -ENODEV) /* DEBUG_FS is defined */
			pr_err("Failed to create debugfs directory\n");
		goto failed_create_root;
	}

	attr = debugfs_create_u32("shutdown_delay_ms", S_IRUGO | S_IWUSR,
			root, &vpu_shutdown_delay);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create shutdown_delay file\n");
		goto failed_create_attr;
	}

	/* create firmware_log file */
	attr = debugfs_create_file("firmware_log", S_IRUGO, root, NULL,
			&fw_logging_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create firmware logging attribute\n");
		goto failed_create_attr;
	}

	/* create queue state file */
	attr = debugfs_create_file("queue_state", S_IRUGO, root, NULL,
			&queue_state_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create queue state attribute\n");
		goto failed_create_attr;
	}

	/* create csr regs file */
	attr = debugfs_create_file("csr_regs", S_IRUGO, root, NULL,
			&csr_regs_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create csr regs attribute\n");
		goto failed_create_attr;
	}

	/* create cmd entry */
	attr = debugfs_create_file("cmd", RW_MODE, root, NULL,
			&vpu_cmd_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create cmd attribute\n");
		goto failed_create_attr;
	}

	/* create shared mem entry (smem dir + files) */
	attr = init_smem_dir(root);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create smem dir\n");
		goto failed_create_attr;
	}

	/* create client entry */
	attr = debugfs_create_file("client", RW_MODE, root, core,
			&vpu_client_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create client attribute\n");
		goto failed_create_attr;
	}

	return root;

failed_create_attr:
	cleanup_vpu_debugfs(root);
	return attr ? attr : ERR_PTR(-ENOMEM);
failed_create_root:
	return root ? root : ERR_PTR(-ENOMEM);
}

void cleanup_vpu_debugfs(struct dentry *root)
{
	debugfs_remove_recursive(root);
}
