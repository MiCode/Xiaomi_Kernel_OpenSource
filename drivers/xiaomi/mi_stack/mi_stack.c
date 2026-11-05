// SPDX-License-Identifier: GPL-2.0+
/*
 * mi stack will show kernel stacktrace of the specified task.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define pr_fmt(fmt) "mi_stack: "fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/pid_namespace.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>

struct __mi_stack {
	pid_t pid;
	struct task_struct *task;
	struct proc_dir_entry *stack_proc_dir;
	struct mutex lock;
} mi_stack = {
	.pid = -1,
	.task = NULL,
};

static kuid_t proc_kuid;
static int proc_uid = 1000;

static kgid_t proc_kgid;
static int proc_gid = 1000;

#define MAX_STACK_TRACE_DEPTH	64
/*
 * cat /proc/mi_stack/stack
 *
 * [1, init] Kernel calltrace:
 * [<0>] ep_poll+0x86c/0x93c
 * [<1>] do_epoll_wait+0xc8/0xf4
 * [<2>] __arm64_sys_epoll_pwait+0xd0/0x180
 * [<3>] invoke_syscall+0x58/0x114
 * [<4>] el0_svc_common+0xac/0xe8
 * [<5>] do_el0_svc+0x1c/0x28
 * [<6>] el0_svc+0x3c/0x74
 * [<7>] el0t_64_sync_handler+0x68/0xbc
 * [<8>] el0t_64_sync+0x1a8/0x1ac
 */
static int stack_show(struct seq_file *m, void *v)
{
	unsigned long *entries;
	unsigned int i, nr_entries;
	pid_t pid;
	struct mutex *lock = &mi_stack.lock;
	struct task_struct *task = NULL;

	mutex_lock(lock);
	pid = mi_stack.pid;
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (task == NULL) {
		mutex_unlock(lock);
		pr_err("can not find pid %d\n", pid);
		return -ESRCH;
	}

	get_task_struct(task);
	entries = kmalloc_array(MAX_STACK_TRACE_DEPTH, sizeof(*entries),
				GFP_KERNEL);
	if (!entries) {
		mutex_unlock(lock);
		return -ENOMEM;
	}

	nr_entries = stack_trace_save_tsk(task, entries,
						MAX_STACK_TRACE_DEPTH, 0);

	seq_printf(m, "[%d, %s] Kernel calltrace: \n", task->pid, task->comm);
	put_task_struct(task);

	mutex_unlock(lock);
	for (i = 0; i < nr_entries; i++) {
		seq_printf(m, "[<%d>] %pB\n", i, (void *)entries[i]);
	}

	kfree(entries);

	return 0;
}

static char kbuf[128];
/*
 * cat /proc/mi_stack/pid
 *
 * [1, init]
 */
static ssize_t pid_read(struct file *file, char __user *buf,
				      size_t len, loff_t *offset)
{
	int nbytes;
	struct mutex *lock = &mi_stack.lock;

	mutex_lock(lock);
	nbytes = sprintf(kbuf, "%d\n", mi_stack.pid);
	mutex_unlock(lock);

	return simple_read_from_buffer(buf, len, offset, kbuf, nbytes);
}

/* echo $pid > /proc/mi_stack/pid */
static ssize_t pid_write(struct file *file, const char __user *buf,
			  size_t len, loff_t *ppos)
{
	ssize_t ret;
	pid_t tmp_pid;
	struct mutex *lock = &mi_stack.lock;

	mutex_lock(lock);
	ret = simple_write_to_buffer(kbuf, len, ppos, buf, len);
	if (sscanf(kbuf, "%d", &tmp_pid) != 1) {
		mutex_unlock(lock);
		return -EINVAL;
	}

	if (tmp_pid < 0) {
		mutex_unlock(lock);
		pr_err("pid %d is invalid\n", tmp_pid);
		return -EINVAL;
	}

	mi_stack.pid = tmp_pid;
	mutex_unlock(lock);
	pr_info("User set pid:%d\n", tmp_pid);

	return ret;
}

static const struct proc_ops pid_pops = {
	.proc_read = pid_read,
	.proc_write = pid_write,
};

static int __init mi_stack_init(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry **stack_proc_dir = &mi_stack.stack_proc_dir;

	proc_kuid = make_kuid(&init_user_ns, proc_uid);
	proc_kgid = make_kgid(&init_user_ns, proc_gid);
	if (!uid_valid(proc_kuid) || !gid_valid(proc_kgid))
		return -EINVAL;

	*stack_proc_dir = proc_mkdir("mi_stack", NULL);
	if (!*stack_proc_dir)
		return -ENOMEM;

	entry = proc_create("pid", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP, *stack_proc_dir, &pid_pops);
	if (!entry)
		goto fail1;
	proc_set_user(entry, proc_kuid, proc_kgid);

	entry = proc_create_single("stack", S_IRUSR|S_IRGRP, *stack_proc_dir, stack_show);
	if (!entry)
		goto fail2;
	proc_set_user(entry, proc_kuid, proc_kgid);

	mutex_init(&mi_stack.lock);

	pr_err("%s succeed\n", __func__);

	return 0;

fail2:
	remove_proc_entry("pid", *stack_proc_dir);
fail1:
	remove_proc_entry("mi_stack", NULL);

	return -ENOMEM;
}

static void __exit mi_stack_exit(void)
{
	struct proc_dir_entry *stack_proc_dir = mi_stack.stack_proc_dir;

	mutex_destroy(&mi_stack.lock);
	remove_proc_entry("stack", stack_proc_dir);
	remove_proc_entry("pid", stack_proc_dir);
	remove_proc_entry("mi_stack", NULL);

	pr_err("%s\n", __func__);
}

module_init(mi_stack_init);
module_exit(mi_stack_exit);
MODULE_AUTHOR("gaoxiang17 <gaoxiang17@xiaomi.com>");
MODULE_DESCRIPTION("Register Mi Stack driver");
MODULE_LICENSE("GPL v2");
