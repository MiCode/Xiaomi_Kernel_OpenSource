// MIUI ADD: Stability_DebugEnhance
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Dump binder buffer support
 *
 * Copyright (C) 2025.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/seq_file.h>

#include <trace/hooks/binder.h>
#include <uapi/linux/android/binder.h>
#include <../../android/binder_internal.h>

#define BINDER_BUFFER_INFO_MAX_SIZE 512
#define BINDER_BUFFER_COLLECTION_MIN_INTERVAL_SEC 5
#define BUFF_WRITE_SIZE 128
#define BIT_PRE_LOW_EVENT_MASK   (0x03)    // low_buffer_status_mask第0-1位：对应lower buffer状态

int start_monitor(void);
int stop_monitor(void);

struct binder_node_info {
	binder_uintptr_t ptr;
	binder_uintptr_t cookie;
};

struct binder_transaction_info {
	unsigned int code;
	ktime_t start_time;
};

struct binder_buffer_info {
	size_t buffer_size;
	int pid;
	struct list_head entry;
	struct binder_node_info node_info;
	struct binder_transaction_info transaction_info;
};

struct binder_alloc_info {
	size_t buffer_size;
	size_t free_async_space;
	size_t total_alloc_size;
	int valid_buffer_size;
	struct list_head buffers;
};

struct binder_proc_info {
	int pid;
	char process_name[TASK_COMM_LEN];
	int oneway_spam_suspect_pid;
	struct timespec64 collect_time;
	struct binder_alloc_info alloc_info;
};

enum {
	ERR_SINGLE_CLIENT_BUFFER_OVERFLOW = (1 << 0),
	ERR_MULTI_CLIENT_BUFFER_OVERFLOW = (1 << 1),
	EPOLL_EVENT_TRIGGERED = (1 << 2)
};

static DEFINE_SPINLOCK(binder_info_lock);
static struct binder_proc_info binder_info;
static wait_queue_head_t epoll_event_wait;

static atomic_t is_monitoring = ATOMIC_INIT(0);

static atomic_t low_buffer_status_mask = ATOMIC_INIT(0);

static size_t calc_alloc_buffer_size(struct binder_alloc *alloc,
				     struct binder_buffer *buffer)
{
	if (list_is_last(&buffer->entry, &alloc->buffers))
		return alloc->buffer + alloc->buffer_size - buffer->user_data;
	return list_entry(buffer->entry.next, struct binder_buffer, entry)
		       ->user_data -
	       buffer->user_data;
}

static void enqueue_binder_info(struct binder_buffer_info *buffer_info,
				struct binder_alloc_info *alloc_info)
{
	list_add_tail(&buffer_info->entry, &alloc_info->buffers);
}

static void clear_binder_info_locked(void)
{
	struct binder_buffer_info *buffer_info, *tmp;
	list_for_each_entry_safe(buffer_info, tmp,
				 &binder_info.alloc_info.buffers, entry) {
		if (buffer_info) {
			list_del(&buffer_info->entry);
			kfree(buffer_info);
		}
	}
	memset(&binder_info, 0, sizeof(struct binder_proc_info));
	INIT_LIST_HEAD(&binder_info.alloc_info.buffers);
}

static void extend_android_vh_binder_proc_transaction_finish(
	void *data, struct binder_proc *proc, struct binder_transaction *t,
	struct task_struct *binder_th_task, bool pending_async, bool sync)
{
	struct rb_node *n;
	int allocated_buffer_count = 0;
	struct timespec64 collect_time;
	struct binder_buffer_info *buffer_info;
	struct binder_node *target_node;
	struct binder_transaction *transaction;
	size_t total_alloc_size = 0;
	int cur_low_buffer_event_type = 0;
	int low_val_locked;

	if (sync) {
		return;
	}

	if (proc == NULL || proc->tsk == NULL || t == NULL ||
	    t->buffer == NULL) {
		return;
	}

	if (strcmp("system_server", proc->tsk->comm) != 0) {
		return;
	}

	/*
	 * Only start dumping once we have less than 20% of async
	 * space left (which is less than 10% of total buffer size).
	 */
	if (proc->alloc.free_async_space > proc->alloc.buffer_size / 10) {
		/*
		 * When the insufficient buffer state is lifted, attempt to acquire the lock and clear the cached data. 
		 * Cache clearing is not an urgent operation; if lock acquisition fails, retry during the next transaction.
		 */ 
		if (!spin_trylock(&binder_info_lock)) {
			return;
		}
		if (atomic_fetch_and(~BIT_PRE_LOW_EVENT_MASK, &low_buffer_status_mask) & BIT_PRE_LOW_EVENT_MASK) {
			clear_binder_info_locked();
			spin_unlock(&binder_info_lock);
			atomic_or(EPOLL_EVENT_TRIGGERED, &low_buffer_status_mask);
			wake_up_interruptible(&epoll_event_wait);
		} else {
			spin_unlock(&binder_info_lock);
		}
		return;
	}

	if (!spin_trylock(&proc->alloc.lock)) {
		return;
	}
	if (!proc->alloc.oneway_spam_detected) {
		cur_low_buffer_event_type = ERR_MULTI_CLIENT_BUFFER_OVERFLOW;
	} else if (t->buffer->oneway_spam_suspect) {
		/* 
		 * process which has more than 50% of async space (which 
		 * is 25% of total buffer size) cause ERR_SINGLE_CLIENT_BUFFER_OVERFLOW
		 * process which has more than 50 transactions cause
		 * ERR_SINGLE_CLIENT_BUFFER_OVERFLOW
		 */
		for (n = rb_first(&proc->alloc.allocated_buffers); n != NULL;
		     n = rb_next(n)) {
			struct binder_buffer *buffer =
				rb_entry(n, struct binder_buffer, rb_node);
			if (buffer->pid != t->buffer->pid)
				continue;
			if (!buffer->async_transaction)
				continue;
			total_alloc_size +=
				calc_alloc_buffer_size(&proc->alloc, buffer);
		}
		if (total_alloc_size >= proc->alloc.buffer_size / 4) {
			cur_low_buffer_event_type =
				ERR_SINGLE_CLIENT_BUFFER_OVERFLOW;
		} else {
			cur_low_buffer_event_type =
				ERR_MULTI_CLIENT_BUFFER_OVERFLOW;
		}
	} else {
		spin_unlock(&proc->alloc.lock);
		return;
	}

	low_val_locked = (int)atomic_read(&low_buffer_status_mask) & BIT_PRE_LOW_EVENT_MASK;
	if (cur_low_buffer_event_type & low_val_locked) {
		spin_unlock(&proc->alloc.lock);
		return;
	}
	atomic_or(cur_low_buffer_event_type, &low_buffer_status_mask);

	if (!spin_trylock(&binder_info_lock)) {
		spin_unlock(&proc->alloc.lock);
		return;
	}

	clear_binder_info_locked();

	ktime_get_real_ts64(&collect_time);
	binder_info.collect_time = collect_time;
	binder_info.pid = proc->pid;
	strlcpy(binder_info.process_name, proc->tsk->comm, TASK_COMM_LEN);

	if (cur_low_buffer_event_type == ERR_SINGLE_CLIENT_BUFFER_OVERFLOW) {
		binder_info.oneway_spam_suspect_pid = t->buffer->pid;
		binder_info.alloc_info.total_alloc_size = total_alloc_size;
	}

	binder_info.alloc_info.buffer_size = proc->alloc.buffer_size;
	binder_info.alloc_info.free_async_space = proc->alloc.free_async_space;
	for (n = rb_first(&proc->alloc.allocated_buffers); n != NULL;
	     n = rb_next(n)) {
		struct binder_buffer *buffer =
			rb_entry(n, struct binder_buffer, rb_node);

		/*
		 * If the issue is specific to a single client, only collect data
		 * for that client. If it is a multi-client issue which
		 * binder_info.oneway_spam_suspect_pid is 0, collect all data.
		 */
		if (binder_info.oneway_spam_suspect_pid != 0 &&
		    buffer->pid != binder_info.oneway_spam_suspect_pid) {
			continue;
		}
		target_node = buffer->target_node;
		transaction = buffer->transaction;
		if (target_node == NULL || transaction == NULL ||
		    !buffer->async_transaction)
			continue;

		buffer_info =
			kzalloc(sizeof(struct binder_buffer_info), GFP_ATOMIC);
		if (buffer_info == NULL) {
			printk(KERN_ERR "%s: kzalloc fail\n", __func__);
			spin_unlock(&binder_info_lock);
			spin_unlock(&proc->alloc.lock);
			return;
		}

		buffer_info->buffer_size =
			calc_alloc_buffer_size(&proc->alloc, buffer);
		buffer_info->pid = buffer->pid;
		buffer_info->node_info.ptr = target_node->ptr;
		buffer_info->node_info.cookie = target_node->cookie;
		buffer_info->transaction_info.code = transaction->code;
		buffer_info->transaction_info.start_time =
			buffer->transaction->start_time;
		enqueue_binder_info(buffer_info, &binder_info.alloc_info);
		if (++allocated_buffer_count >= BINDER_BUFFER_INFO_MAX_SIZE) {
			printk(KERN_WARNING
			       "%s: binder buffer too large, count:%d\n",
			       __func__, allocated_buffer_count);
			break;
		}
	}
	binder_info.alloc_info.valid_buffer_size = allocated_buffer_count;
	spin_unlock(&binder_info_lock);
	spin_unlock(&proc->alloc.lock);
	printk(KERN_ERR
	       "%s: extend_android_vh_binder_proc_transaction_finish start \n",
	       __func__);
	atomic_or(EPOLL_EVENT_TRIGGERED, &low_buffer_status_mask);
	wake_up_interruptible(&epoll_event_wait);
}

static int bindermeminfo_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	struct binder_buffer_info *buffer_info;
	ktime_t cur_time;
	if (atomic_read(&is_monitoring) == 0) {
		seq_printf(seq, "monitor not start\n");
		return 0;
	}
	atomic_and(~EPOLL_EVENT_TRIGGERED, &low_buffer_status_mask);
	if ((atomic_read(&low_buffer_status_mask) & BIT_PRE_LOW_EVENT_MASK) == 0) {
		seq_printf(seq, "binder free_async_space is sufficient\n");
		return 0;
	}
	spin_lock(&binder_info_lock);
	rtc_time64_to_tm(binder_info.collect_time.tv_sec, &tm);
	seq_printf(
		seq,
		"binder mem info collect time(utc0):%d-%02d-%02d %02d:%02d:%02d\n",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec);
	seq_printf(
		seq,
		"proc info pid: %d, process_name: %s, oneway_spam_suspect_pid:%d\n",
		binder_info.pid, binder_info.process_name,
		binder_info.oneway_spam_suspect_pid);
	seq_printf(
		seq,
		"alloc info buffer_size: %zu, free_async_space: %zu, total_alloc_size: %zu, valid_buffer_size:%d\n",
		binder_info.alloc_info.buffer_size,
		binder_info.alloc_info.free_async_space,
		binder_info.alloc_info.total_alloc_size,
		binder_info.alloc_info.valid_buffer_size);

	cur_time = ktime_get();
	list_for_each_entry(buffer_info, &binder_info.alloc_info.buffers,
			    entry) {
		seq_printf(
			seq,
			"buffer_info buffer_size: %zu, pid: %d, node_info.ptr: %llx, node_info.cookie: %llx, code:%u, elapsed %lldms\n",
			buffer_info->buffer_size, buffer_info->pid,
			buffer_info->node_info.ptr,
			buffer_info->node_info.cookie,
			buffer_info->transaction_info.code,
			ktime_ms_delta(
				cur_time,
				buffer_info->transaction_info.start_time));
	}
	spin_unlock(&binder_info_lock);
	return 0;
}

static __poll_t binder_mem_event_poll(struct file *file, poll_table *wait)
{
	__poll_t ret = 0;

	poll_wait(file, &epoll_event_wait, wait);
	// check EPOLL_EVENT_TRIGGERED
	if ((atomic_read(&low_buffer_status_mask) & EPOLL_EVENT_TRIGGERED) > 0) {
		ret = POLLIN;
	}

	return ret;
}

static int bindermeminfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, bindermeminfo_show, NULL);
}

static ssize_t bindermeminfo_write(struct file *file,
				   const char __user *userbuf, size_t count,
				   loff_t *data)
{
	char buf[BUFF_WRITE_SIZE];
	unsigned long in_data = 0;
	static atomic_t bindermeminfo_write_busy = ATOMIC_INIT(0);

	if (atomic_xchg(&bindermeminfo_write_busy, 1)) {
		return -EBUSY;
	}

	if (count == 0 || count >= BUFF_WRITE_SIZE - 1) {
		atomic_set(&bindermeminfo_write_busy, 0);
		return -EINVAL;
	}

	if (copy_from_user(buf, userbuf, count)) {
		atomic_set(&bindermeminfo_write_busy, 0);
		return -EFAULT;
	}

	buf[count] = '\0';

	if (kstrtoul(buf, 10, &in_data)) {
		atomic_set(&bindermeminfo_write_busy, 0);
		return -EINVAL;
	}

	if (in_data == 0 && atomic_read(&is_monitoring) == 1) {
		stop_monitor();
	} else if (in_data == 1 && atomic_read(&is_monitoring) == 0) {
		start_monitor();
	}
	printk(KERN_INFO "%s: callser:%s, in_data:%lu\n", __func__,
	       current->comm, in_data);

	atomic_set(&bindermeminfo_write_busy, 0);
	return count;
}

static const struct proc_ops bindermeminfo_ops = {
	.proc_open = bindermeminfo_open,
	.proc_poll = binder_mem_event_poll,
	.proc_read = seq_read,
	.proc_write = bindermeminfo_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int init_bindermeminfo_proc(void)
{
	kuid_t uid;
	kgid_t gid;
	struct proc_dir_entry *binder_meminfo_entry;
	binder_meminfo_entry =
		proc_create("mi_bindermeminfo", 0600, NULL, &bindermeminfo_ops);

	if (!binder_meminfo_entry) {
		printk(KERN_ERR "%s: create bindermeminfo node failed\n",
		       __func__);
		return -EIO;
	}

	uid = make_kuid(&init_user_ns, 1000);
	gid = make_kgid(&init_user_ns, 1000);
	if (!uid_valid(uid) || !gid_valid(gid)) {
		printk(KERN_ERR "%s: invalid uid or gid\n", __func__);
		return -EINVAL;
	}

	proc_set_user(binder_meminfo_entry, uid, gid);
	return 0;
}

int start_monitor(void)
{
	int ret;
	ret = register_trace_android_vh_binder_proc_transaction_finish(
		extend_android_vh_binder_proc_transaction_finish, NULL);
	if (ret == 0) {
		atomic_set(&is_monitoring, 1);
	}
	printk(KERN_INFO "%s: start_monitor ret:%d\n", __func__, ret);
	return ret;
}

int stop_monitor(void)
{
	int ret = unregister_trace_android_vh_binder_proc_transaction_finish(
		extend_android_vh_binder_proc_transaction_finish, NULL);
	spin_lock(&binder_info_lock);
	clear_binder_info_locked();
	spin_unlock(&binder_info_lock);
	if (ret == 0) {
		atomic_set(&is_monitoring, 0);
	}
	printk(KERN_INFO "%s: stop_monitor ret:%d\n", __func__, ret);
	return ret;
}

static int __init dump_binder_init(void)
{
	INIT_LIST_HEAD(&binder_info.alloc_info.buffers);
	init_waitqueue_head(&epoll_event_wait);
	return init_bindermeminfo_proc();
}

static void __exit dump_binder_exit(void)
{
	remove_proc_entry("mi_bindermeminfo", NULL);
	if (atomic_read(&is_monitoring) == 1) {
		stop_monitor();
	}
}

MODULE_DESCRIPTION("Xiaomi binder async buffer monitor");
module_init(dump_binder_init);
module_exit(dump_binder_exit);
MODULE_LICENSE("GPL v2");
// END Stability_DebugEnhance
