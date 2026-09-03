/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Unisoc Inc.
 */

#include "iomonitor.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/workqueue.h>
#include <linux/kprobes.h>
#include <linux/timekeeping.h>
#include <linux/blkdev.h>
#include <linux/kernel.h>
#include <uapi/linux/magic.h>


static unsigned int output_rows = DEFAULT_OUTPUT_ROWS;
static unsigned int iomonitor_interval_ms = MAX_INTERAVL_MS; /* msecs */
static unsigned int iomonitor_enable;
static unsigned int probe_count;
static unsigned long last_monitor_jiffies;
static unsigned long collision_count;

static struct sprd_task task_buffer;
static struct sprd_task io_data;
static struct proc_dir_entry *iomonitor_dir;

static struct kprobe sprd_vfs_write_kp = {
	.symbol_name	= "generic_perform_write",
};

static int sort_column(const void *obj1, const void *obj2)
{
	struct sprd_task_io_info *s1 = (struct sprd_task_io_info *)obj1;
	struct sprd_task_io_info *s2 = (struct sprd_task_io_info *)obj2;

	if (!s1->write_bytes)
		return !s2->write_bytes ? 0 : 1;
	else if (!s2->write_bytes)
		return -1;
	else if (s2->write_bytes == s1->write_bytes)
		return 0;
	else
		return s1->write_bytes < s2->write_bytes ? 1 : -1;
}

static void sprd_task_status_get_value(struct seq_file *m, void *v)
{
	int i;
	struct sprd_task_io_info *ti;
	unsigned long cur_time_ms;
	unsigned long last_time_ms;
	unsigned long duration_ms;

	spin_lock(&io_data.t_lock);
	if (probe_count < 1) {
		last_monitor_jiffies = jiffies;
		spin_unlock(&io_data.t_lock);
		return;
	}

	memcpy(task_buffer.taskio_list, io_data.taskio_list,
	       sizeof(struct sprd_task_io_info) * PID_HASH_LEGNTH);
	memset(io_data.taskio_list, 0, sizeof(struct sprd_task_io_info) * PID_HASH_LEGNTH);

	cur_time_ms = jiffies_to_msecs(jiffies);
	last_time_ms = jiffies_to_msecs(last_monitor_jiffies);
	duration_ms = cur_time_ms - last_time_ms;
	seq_printf(m, "duration %lu ms collision %lu total_tsk %u\n%-10s\t%10s\t%32s\t%20s\t\n",
		   duration_ms, collision_count, probe_count,
		   "PID", "UID", "TOTAL_WRITE(bytes)", "COMM");

	last_monitor_jiffies = jiffies;
	collision_count = 0;
	probe_count = 0;
	spin_unlock(&io_data.t_lock);

	sprd_qsort(task_buffer.taskio_list, PID_HASH_LEGNTH,
	      sizeof(struct sprd_task_io_info), sort_column);

	for (i = 0; i < output_rows; i++) {
		ti = task_buffer.taskio_list + i;
		if (ti->tgid)
			seq_printf(m, "%-10d\t%10d\t%32llu\t%20s\t\n",
				   ti->tgid, ti->uid, ti->write_bytes, ti->comm);
	}
}

static int sprd_iomonitor_dataflow_show(struct seq_file *m, void *v)
{
	sprd_task_status_get_value(m, v);
	return 0;
}

static int sprd_iomonitor_dataflow_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_iomonitor_dataflow_show, inode->i_private);
}

static int sprd_iomonitor_dataflow_release(struct inode *inode, struct file *file)
{
	spin_lock(&io_data.t_lock);
	memset(io_data.taskio_list, 0, sizeof(struct sprd_task_io_info) * PID_HASH_LEGNTH);
	spin_unlock(&io_data.t_lock);
	return single_release(inode, file);
}

static const struct proc_ops proc_dataflow_operations = {
	.proc_open = sprd_iomonitor_dataflow_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = sprd_iomonitor_dataflow_release,
};

static int sprd_iomonitor_enable_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", iomonitor_enable ? 1 : 0);
	return 0;
}

static int sprd_iomonitor_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_iomonitor_enable_show, inode->i_private);
}

static ssize_t sprd_iomonitor_enable_write(struct file *file,
					   const char __user *buffer, size_t count, loff_t *pos)
{
	char val;
	unsigned int new_enable;

	if (!count)
		return count;

	if (get_user(val, buffer)) {
		pr_err("io_monitor: write fail cause get_user error\n");
		return -EFAULT;
	}

	if (val == '0')
		new_enable = 0;
	else
		new_enable = 1;

	if (new_enable != iomonitor_enable) {
		if (!iomonitor_enable)
			last_monitor_jiffies = jiffies;
		iomonitor_enable = new_enable;
	}

	return count;
}

static const struct proc_ops proc_enable_operations = {
	.proc_open = sprd_iomonitor_enable_open,
	.proc_read = seq_read,
	.proc_write = sprd_iomonitor_enable_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t sprd_set_output_rows_write(struct file *file,
					  const char __user *buffer, size_t count, loff_t *pos)
{
	char buf[8] = { 0 };
	ssize_t buf_size;
	unsigned int v;
	int ret;

	if (!count)
		return 0;

	buf_size = min(count, (size_t)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, buf_size))
		return -EFAULT;

	buf[buf_size-1] = '\0';

	ret = kstrtouint(buf, 10, &v);
	if (ret)
		return ret;

	if (v > PID_HASH_LEGNTH)
		v = PID_HASH_LEGNTH;

	output_rows = v;

	return count;
}

static int sprd_output_rows_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", output_rows);
	return 0;
}

static int sprd_set_output_rows_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_output_rows_show, inode->i_private);
}

static const struct proc_ops proc_set_output_rows_operations = {
	.proc_open = sprd_set_output_rows_open,
	.proc_read = seq_read,
	.proc_write = sprd_set_output_rows_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void sprd_buf_clear(void)
{
	memset(io_data.taskio_list, 0, sizeof(struct sprd_task_io_info) * PID_HASH_LEGNTH);
	last_monitor_jiffies = jiffies;
	probe_count = 0;
	collision_count = 0;
}

static bool sprd_check_fs_magic(struct file *file)
{
	unsigned int s_magic;

	s_magic = le32_to_cpu(file->f_inode->i_sb->s_magic);

	if (s_magic == F2FS_SUPER_MAGIC || s_magic == EXT4_SUPER_MAGIC ||
	    s_magic == FUSE_SUPER_MAGIC || s_magic == DEVPTS_SUPER_MAGIC)
		return true;
	else
		return false;
}

static void sprd_write_monitor(struct kprobe *p, struct pt_regs *regs,
			       unsigned long flags)
{
#ifdef CONFIG_ARM64
	struct iov_iter *iov = (struct iov_iter *)regs->regs[1];
	struct file *file = (struct file *)regs->regs[0];
#endif
#ifdef CONFIG_ARM
	struct iov_iter *iov = (struct iov_iter *)regs->uregs[1];
	struct file *file = (struct file *)regs->uregs[0];
#endif

	pid_t tgid = current->tgid;
	pid_t pid = current->pid;

	int slot = tgid % PID_HASH_LEGNTH;
	struct sprd_task_io_info *ti;
	int i;
	size_t bytes = iov->count;

	if (!iomonitor_enable)
		return;

	if (!file->f_inode->i_sb)
		return;

	if (!sprd_check_fs_magic(file))
		return;

	spin_lock(&io_data.t_lock);
	for (i = 0; i < PID_HASH_LEGNTH; i++) {
		ti = &io_data.taskio_list[(slot + i) % PID_HASH_LEGNTH];
		if (ti->used) {
			if (ti->tgid == tgid) {
				ti->write_bytes += bytes;
				goto end;
			}
			collision_count += 1;
		} else {
			probe_count++;
			ti->tgid = tgid;
			ti->uid = current->group_leader->cred->uid.val;
			/*
			 * Get the name of the main thread to ensure that the tgid is consistent
			 * with the process name.
			 */
			memcpy(ti->comm, pid == tgid ?
			       current->comm : current->group_leader->comm, TASK_COMM_LEN);
			ti->used = true;
			ti->write_bytes = bytes;
			goto end;
		}
	}

end:
	if (time_before(jiffies, last_monitor_jiffies +
		msecs_to_jiffies(iomonitor_interval_ms))) {
		spin_unlock(&io_data.t_lock);
		return;
	}
	sprd_buf_clear();
	spin_unlock(&io_data.t_lock);
}

static void sprd_iomonitor_exit(void)
{
	kfree(task_buffer.taskio_list);
	kfree(io_data.taskio_list);
	unregister_kprobe(&sprd_vfs_write_kp);
}

static int sprd_iomonitor_init(void)
{
	int ret;

	sprd_vfs_write_kp.post_handler = sprd_write_monitor;
	ret = register_kprobe(&sprd_vfs_write_kp);
	if (ret < 0) {
		pr_err("io_monitor: register_kprobe failed, returned %d\n", ret);
		return ret;
	}

	task_buffer.taskio_list = kzalloc(sizeof(struct sprd_task_io_info)
					  * PID_HASH_LEGNTH, GFP_KERNEL);
	if (!task_buffer.taskio_list) {
		ret = -ENOMEM;
		goto err;
	}

	io_data.taskio_list = kzalloc(sizeof(struct sprd_task_io_info)
				      * PID_HASH_LEGNTH, GFP_KERNEL);
	if (!io_data.taskio_list) {
		ret = -ENOMEM;
		goto err;
	}

	return 0;

err:
	sprd_iomonitor_exit();
	return ret;
}

static int __init kprobe_iomonitor_init(void)
{
	int ret;
	struct proc_dir_entry *io_stat_entry;
	struct proc_dir_entry *enable_entry;
	struct proc_dir_entry *output_rows_entry;

	ret = sprd_iomonitor_init();
	if (ret) {
		pr_err("io_monitor: resource init failed, ret %d\n", ret);
		return ret;
	}

	iomonitor_dir = proc_mkdir("io_monitor", NULL);
	if (!iomonitor_dir) {
		pr_err("io_monitor: create io_monitor dir failed.\n");
		goto err;
	}

	io_stat_entry =
		proc_create("io_stat", 0444, iomonitor_dir,
			    &proc_dataflow_operations);
	if (!io_stat_entry) {
		pr_err("io_monitor: create io_stat_entry failed.\n");
		goto err_rmdir;
	}

	enable_entry =
		proc_create("enable", 0664, iomonitor_dir,
			    &proc_enable_operations);
	if (!enable_entry) {
		pr_err("io_monitor: create enable_entry failed.\n");
		goto err_rmdir;
	}

	output_rows_entry =
		proc_create("output_rows", 0664, iomonitor_dir,
			    &proc_set_output_rows_operations);
	if (!output_rows_entry) {
		pr_err("io_monitor: create output_rows_entry failed.\n");
		goto err_rmdir;
	}

	return 0;

err_rmdir:
	remove_proc_subtree("io_monitor", NULL);
	iomonitor_dir = NULL;
err:
	sprd_iomonitor_exit();
	return -1;
}

static void __exit kprobe_iomonitor_exit(void)
{
	sprd_iomonitor_exit();
	if (iomonitor_dir)
		remove_proc_subtree("io_monitor", NULL);
	iomonitor_dir = NULL;
}

module_init(kprobe_iomonitor_init)
module_exit(kprobe_iomonitor_exit)

MODULE_AUTHOR("Dongliang cui <dongliang.cui@unisoc.com>");
MODULE_LICENSE("GPL");
