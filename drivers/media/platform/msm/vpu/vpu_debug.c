/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/atomic.h>

#include "vpu_debug.h"
#include "vpu_v4l2.h"
#include "vpu_ioctl_internal.h"
#include "vpu_channel.h"
#include "vpu_bus_clock.h"

#define BUF_SIZE		(SZ_4K)
#define RW_MODE			(S_IRUSR | S_IWUSR)
#define FW_LOG_TIMEOUT_MS	500

static int vpu_debug_on;

struct fw_log_info {
	/* wq woken by hfi layer when fw log msg received */
	wait_queue_head_t wq;
	/* wq only woken by hfi layer if this flag set */
	int log_available;
	/* buf used for formatting log msgs */
	char *fmt_buf;
	/* only one thread may read fw logs at a time */
	atomic_t num_readers;
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
	fw_log.log_available = 1;
	wake_up_interruptible(&fw_log.wq);
}

static int open_fw_log(struct inode *inode, struct file *file)
{
	char *fmt_buf;

	/* Only one user thread may read firmware logs at a time.
	 * We decrement number of readers upon release of the
	 * firmware logs.
	 */
	if (atomic_inc_return(&fw_log.num_readers) > 1) {
		atomic_dec(&fw_log.num_readers);
		return -EBUSY;
	}

	fmt_buf = kzalloc(BUF_SIZE, GFP_KERNEL);
	if (unlikely(!fmt_buf)) {
		pr_err("Failed to allocate fmt_buf\n");
		atomic_dec(&fw_log.num_readers);
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
			fw_log.log_available = 0;
			ret = wait_event_interruptible_timeout(fw_log.wq,
				fw_log.log_available == 1,
				msecs_to_jiffies(FW_LOG_TIMEOUT_MS));
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

	/* Allow another reader to access firmware logs */
	atomic_dec(&fw_log.num_readers);

	return 0;
}

static const struct file_operations fw_logging_ops = {
	.open = open_fw_log,
	.read = read_fw_log,
	.release = release_fw_log,
};

static ssize_t write_fw_log_level(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	char buf[4];
	int log_level;
	int len;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	if (kstrtou32(buf, 0, &log_level))
		return -EINVAL;

	if (log_level < VPU_LOGGING_NONE || log_level > VPU_LOGGING_ALL) {
		pr_err("Invalid logging level %d\n", log_level);
		return -EINVAL;
	}

	ret = vpu_hw_sys_set_log_level(log_level);
	if (ret)
		return ret;

	pr_debug("Firmware log level set to %s\n", buf);
	return count;
}

static ssize_t read_fw_log_level(struct file *file, char __user *user_buf,
	size_t len, loff_t *ppos)
{
	int ret;
	char buf[4];
	int log_level;

	ret = vpu_hw_sys_get_log_level();
	if (ret < 0)
		return ret;
	log_level = ret;

	ret = snprintf(buf, sizeof(buf), "%d\n", log_level);
	if (ret < 0) {
		pr_err("Error converting log level from int to char\n");
		return ret;
	}

	return simple_read_from_buffer(user_buf, len, ppos, buf, sizeof(buf));
}

static const struct file_operations fw_log_level_ops = {
	.open = simple_open,
	.write = write_fw_log_level,
	.read = read_fw_log_level,
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

	size = vpu_hw_print_queues(dbg_buf, BUF_SIZE);
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

	/* If debug mode is on, a lock may still be
	 * held (while in process of booting up firmware).
	 * We need to still be able to dump csr registers
	 * in this case. Do not attempt to acquire the lock.
	 */
	if (vpu_debug_on)
		size = vpu_hw_dump_csr_regs_no_lock(dbg_buf, BUF_SIZE);
	else
		size = vpu_hw_dump_csr_regs(dbg_buf, BUF_SIZE);
	if (size > 0)
		ret = simple_read_from_buffer(user_buf, len, ppos,
				dbg_buf, size);
	else
		ret = -EIO; /* device not readable */
	kfree(dbg_buf);
	return ret;
}

int write_csr_reg(struct file *file, const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	int ret;
	int len;
	char buf[24];
	char *sptr, *token;
	u32 reg_off;
	u32 reg_val;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	sptr = buf;
	token = strsep(&sptr, ":");
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_off))
		return -EINVAL;

	if (kstrtou32(sptr, 0, &reg_val))
		return -EINVAL;

	ret = vpu_hw_write_csr_reg(reg_off, reg_val);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations csr_regs_ops = {
	.open = simple_open,
	.read = read_csr_regs,
	.write = write_csr_reg,
};

static void debug_on(void)
{
	vpu_debug_on = 1;
	vpu_hw_debug_on();
}

static void debug_off(void)
{
	vpu_hw_debug_off();
	vpu_debug_on = 0;
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
		debug_on();
	else if (strcmp(cmp, "debugoff") == 0)
		debug_off();

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
		ret = vpu_hw_dump_smem_line(cbuf, sizeof(cbuf), offset);
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

static ssize_t read_streaming_state(struct file *file, char __user *user_buf,
		size_t len, loff_t *ppos)
{
	struct vpu_dev_session *session = file->private_data;
	char temp_buf[8];
	size_t size, ret;

	size = snprintf(temp_buf, sizeof(temp_buf), "%d\n",
			session->streaming_state == ALL_STREAMING);
	ret = simple_read_from_buffer(user_buf, len, ppos, temp_buf, size);

	return ret;
}

static const struct file_operations streaming_state_ops = {
	.open = simple_open,
	.read = read_streaming_state,
};

static int init_vpu_session_info_dir(struct dentry *root,
		struct vpu_dev_session *session)
{
	struct dentry *attr_root, *attr;
	char attr_name[SZ_16];
	if (!session || !root)
		goto failed_create_dir;

	/* create session debugfs directory */
	snprintf(attr_name, SZ_16, "session_%d", session->id);

	attr_root = debugfs_create_dir(attr_name, root);
	if (IS_ERR_OR_NULL(attr_root)) {
		pr_err("Failed to create %s info directory\n", attr_name);
		goto failed_create_dir;
	}

	/* create number of clients attribute */
	attr = debugfs_create_u32("num_clients", S_IRUGO, attr_root,
			&session->client_count);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create number of clients attribute\n");
		goto failed_create_attr;
	}

	/* create streaming state attribute file */
	attr = debugfs_create_file("streaming", S_IRUGO,
			attr_root, session, &streaming_state_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create streaming state attribute\n");
		goto failed_create_attr;
	}

	/* create resolution attribute files */
	attr = debugfs_create_u32("in_width", S_IRUGO, attr_root,
			&session->port_info[INPUT_PORT].format.width);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create input width attribute\n");
		goto failed_create_attr;
	}

	attr = debugfs_create_u32("in_height", S_IRUGO, attr_root,
			&session->port_info[INPUT_PORT].format.height);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create input height attribute\n");
		goto failed_create_attr;
	}

	attr = debugfs_create_u32("out_width", S_IRUGO, attr_root,
			&session->port_info[OUTPUT_PORT].format.width);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create output width attribute\n");
		goto failed_create_attr;
	}

	attr = debugfs_create_u32("out_height", S_IRUGO, attr_root,
			&session->port_info[OUTPUT_PORT].format.height);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create output height attribute\n");
		goto failed_create_attr;
	}

	return 0;

failed_create_attr:
	debugfs_remove_recursive(attr_root);
failed_create_dir:
	return -ENOENT;
}

struct dentry *init_vpu_debugfs(struct vpu_dev_core *core)
{
	struct dentry *root, *attr;
	int i;

	root = debugfs_create_dir(VPU_DRV_NAME, NULL);
	if (IS_ERR_OR_NULL(root)) {
		pr_err("Failed to create debugfs directory\n");
		goto failed_create_dir;
	}

	/* create shutdown delay file */
	attr = debugfs_create_u32("shutdown_delay_ms", S_IRUGO | S_IWUSR,
			root, &vpu_shutdown_delay);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create shutdown_delay attribute\n");
		goto failed_create_attr;
	}

	/* create firmware log file */
	init_waitqueue_head(&fw_log.wq);
	atomic_set(&fw_log.num_readers, 0);
	attr = debugfs_create_file("firmware_log", S_IRUGO, root, NULL,
			&fw_logging_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create firmware logging attribute\n");
		goto failed_create_attr;
	}

	/* create firmware log level file */
	attr = debugfs_create_file("firmware_log_level", RW_MODE, root, NULL,
			&fw_log_level_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create firmware logging level attribute\n");
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
	attr = debugfs_create_file("csr_regs", RW_MODE, root, NULL,
			&csr_regs_ops);
	if (IS_ERR_OR_NULL(attr)) {
		pr_err("Failed to create csr regs attribute\n");
		goto failed_create_attr;
	}

	/* create cmd entry */
	vpu_debug_on = 0;
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

	/* create sessions station information directories */
	for (i = 0; i < VPU_NUM_SESSIONS; i++)
		init_vpu_session_info_dir(root, core->sessions[i]);

	return root;

failed_create_attr:
	cleanup_vpu_debugfs(root);
failed_create_dir:
	return NULL;
}

void cleanup_vpu_debugfs(struct dentry *root)
{
	debugfs_remove_recursive(root);
}
