#define pr_fmt(fmt) "MI_LOG: " fmt

#include <linux/swap.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>
#include <linux/sched/stat.h>
#include <linux/proc_fs.h>
#include <linux/delayacct.h>
#include <linux/uaccess.h>
#include <trace/hooks/binder.h>
#include <uapi/linux/android/binder.h>
#include <uapi/linux/sched/types.h>
#include <../../android/binder_internal.h>
#include <../../../kernel/sched/sched.h>
#include "mi_log.h"

struct proc_dir_entry *mi_log_proc;
struct proc_dir_entry *delay_entry;
struct proc_dir_entry *binder_delay_entry;
static struct rb_root task_tree = RB_ROOT;
static struct kmem_cache *task_node_cachep;
static DEFINE_RAW_SPINLOCK(binder_log_lock);

static inline struct task_node *task_node_alloc(gfp_t gfp)
{
	return kmem_cache_zalloc(task_node_cachep, gfp);
}

static void task_node_free(struct task_node *node)
{
	kmem_cache_free(task_node_cachep, node);
}

static int task_node_init(void)
{
	task_node_cachep = kmem_cache_create("task_node", sizeof(struct task_node),
			0, 0, NULL);
	return task_node_cachep != NULL;
}

static struct task_node *get_task_node_by_pid(pid_t pid)
{
	struct rb_node *node = task_tree.rb_node;
	struct task_node *ret = NULL;
	while (node != NULL) {
		ret = rb_entry(node, struct task_node, rb_node);
		if (ret->pid > pid) {
			node = node->rb_right;
		} else if (ret->pid < pid) {
			node = node->rb_left;
		} else {
			return ret;
		}
	}
	return NULL;
}

static struct task_node *insert_task_node_by_pid(struct task_node *t_node)
{
	struct rb_node **p = &task_tree.rb_node;
	struct rb_node *parent = NULL;
	struct task_node *ret = NULL;
	while (*p) {
		parent = *p;
		ret = rb_entry(*p, struct task_node, rb_node);
		if (ret->pid > t_node->pid) {
			p = &parent->rb_right;
		} else if (ret->pid < t_node->pid){
			p = &parent->rb_left;
		} else {
			return ret;
		}
	}
	rb_link_node(&t_node->rb_node, parent, p);
	rb_insert_color(&t_node->rb_node, &task_tree);
	return NULL;
}

static void fill_delay_info_by_task(struct task_struct *task, struct task_node *t)
{
	unsigned long flags;
	u64 blkio_delay, swapin_delay, freepages_delay;
	if (!task)
		return;

	get_task_struct(task);
	raw_spin_lock_irqsave(&task->delays->lock, flags);
	blkio_delay = task->delays->blkio_delay;
	swapin_delay = task->delays->swapin_delay;
	freepages_delay = task->delays->freepages_delay;
	raw_spin_unlock_irqrestore(&task->delays->lock, flags);

	t->delay_info.binder_target_tid = task->pid;
	t->blkio_delay = blkio_delay;
	t->swapin_delay = swapin_delay;
	t->freepages_delay = freepages_delay;
	if (likely(sched_info_on())) {
		t->cpu_runtime = task->se.sum_exec_runtime;
		t->cpu_run_delay = task->sched_info.run_delay;
	}
	t->utime = task->utime;
	t->stime = task->stime;

	put_task_struct(task);
}

static void binder_proc_transaction_handler(void *d, struct task_struct *caller_task,
					    struct task_struct *binder_proc_task,
					    struct task_struct *binder_th_task, int node_debug_id,
					    unsigned int code, bool pending_async)
{
	struct task_node *t;
	unsigned long lock_flags;
	//We check binder info for junk, just save main thread binder info.
	if (caller_task->pid != caller_task->tgid) {
		return;
	}
	raw_spin_lock_irqsave(&binder_log_lock, lock_flags);
	//TODO add hook to check oneway flag
	t = get_task_node_by_pid(caller_task->pid);
	if (t == NULL) {
		t = task_node_alloc(GFP_ATOMIC);
		if (t == NULL) {
			raw_spin_unlock_irqrestore(&binder_log_lock, lock_flags);
			return;
		}
		t->pid = caller_task->pid;
		insert_task_node_by_pid(t);
	}

	t->fill_state = FILL_STATE_NONE;
	get_task_comm(t->delay_info.binder_target_comm, binder_proc_task);
	t->version = t->delay_info.version = 1;
	t->delay_info.pid = caller_task->pid;
	t->delay_info.binder_target_pid = binder_proc_task->tgid;

	if (binder_th_task) {
		fill_delay_info_by_task(binder_th_task, t);
		t->fill_state = FILL_STATE_ONGOING;
		t->delay_info.binder_target_thread_full = 0;
	} else {
		t->fill_state = FILL_STATE_NONE_THREAD;
		t->delay_info.binder_target_thread_full = 1;
	}
	raw_spin_unlock_irqrestore(&binder_log_lock, lock_flags);
}

static void binder_reply_handler(void *d, struct binder_proc *target_proc, struct binder_proc *proc,
				 struct binder_thread *thread, struct binder_transaction_data *tr)
{
	struct task_node *t;
	unsigned long flags;
	unsigned long lock_flags;
	u64 blkio_delay, swapin_delay, freepages_delay;
	if (!target_proc->tsk || !thread->task || !proc->tsk) {
		return;
	}
	raw_spin_lock_irqsave(&binder_log_lock, lock_flags);
	t = get_task_node_by_pid(target_proc->pid);
	if (t == NULL || t->fill_state == FILL_STATE_NONE_THREAD) {
		raw_spin_unlock_irqrestore(&binder_log_lock, lock_flags);
		return;
	}

	get_task_struct(thread->task);
	if (t->delay_info.binder_target_tid != thread->task->pid) {
		put_task_struct(thread->task);
		raw_spin_unlock_irqrestore(&binder_log_lock, lock_flags);
		return;
	}

	raw_spin_lock_irqsave(&thread->task->delays->lock, flags);
	blkio_delay = thread->task->delays->blkio_delay;
	swapin_delay = thread->task->delays->swapin_delay;
	freepages_delay = thread->task->delays->freepages_delay;
	raw_spin_unlock_irqrestore(&thread->task->delays->lock, flags);

	t->delay_info.blkio_delay = blkio_delay - t->blkio_delay;
	t->delay_info.swapin_delay = swapin_delay - t->swapin_delay;
	t->delay_info.freepages_delay = freepages_delay - t->freepages_delay;
	if (likely(sched_info_on())) {
		t->delay_info.cpu_runtime =
			thread->task->se.sum_exec_runtime - t->cpu_runtime;
		t->delay_info.cpu_run_delay =
			thread->task->sched_info.run_delay - t->cpu_run_delay;
	}
	t->delay_info.utime = thread->task->utime - t->utime;
	t->delay_info.stime = thread->task->stime - t->stime;

	put_task_struct(thread->task);

	t->fill_state = FILL_STATE_FINISHED;
	raw_spin_unlock_irqrestore(&binder_log_lock, lock_flags);
}

static ssize_t binder_delay_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct task_struct *task = current;
	struct task_node *insert_node;
	loff_t dummy_pos = 0;
	unsigned long lock_flags;
	struct binder_delay_info delta_info = {};
	if (!task) {
		return -ESRCH;
	}

	raw_spin_lock_irqsave(&binder_log_lock, lock_flags);
	insert_node = get_task_node_by_pid(task->pid);
	if (insert_node == NULL || insert_node->fill_state == FILL_STATE_NONE
			|| insert_node->fill_state == FILL_STATE_ONGOING) {
		raw_spin_unlock_irqrestore(&binder_log_lock, lock_flags);
		return -ESRCH;
	}
	delta_info.version = insert_node->delay_info.version;
	delta_info.pid = insert_node->delay_info.pid;
	strcpy(delta_info.binder_target_comm, insert_node->delay_info.binder_target_comm);
	delta_info.binder_target_pid = insert_node->delay_info.binder_target_pid;
	delta_info.binder_target_thread_full = insert_node->delay_info.binder_target_thread_full;
	if (insert_node->fill_state == FILL_STATE_FINISHED) {
		delta_info.binder_target_tid = insert_node->delay_info.binder_target_tid;
		delta_info.blkio_delay = insert_node->delay_info.blkio_delay;
		delta_info.swapin_delay = insert_node->delay_info.swapin_delay;
		delta_info.freepages_delay = insert_node->delay_info.freepages_delay;
		delta_info.cpu_runtime = insert_node->delay_info.cpu_runtime;
		delta_info.cpu_run_delay = insert_node->delay_info.cpu_run_delay;
		delta_info.utime = insert_node->delay_info.utime;
		delta_info.stime = insert_node->delay_info.stime;
	} else if (insert_node->fill_state == FILL_STATE_NONE_THREAD) {
		delta_info.binder_target_tid = 0;
		delta_info.blkio_delay = 0;
		delta_info.swapin_delay = 0;
		delta_info.freepages_delay = 0;
		delta_info.cpu_runtime = 0;
		delta_info.cpu_run_delay = 0;
		delta_info.utime = 0;
		delta_info.stime = 0;
	}
	raw_spin_unlock_irqrestore(&binder_log_lock, lock_flags);

	return simple_read_from_buffer(buf, count, &dummy_pos, &delta_info,
			sizeof(struct binder_delay_info));
}

static ssize_t binder_delay_write(struct file *file, const char __user *buf,
		                   size_t count, loff_t *ppos)
{
	struct task_node *t;
	char input[256];
	int argc, pid, state;
	unsigned long lock_flags;
	if (count >= sizeof(input)) {
		pr_err("Err write binder_delay count %zu", count);
		return -EFAULT;
	}

	if (copy_from_user(input, buf, count)) {
		pr_err("Err write binder_delay copy_from_user");
		return -EFAULT;
	}

	input[count] = '\0';
	argc = sscanf(input, "%d %d", &pid, &state);

	if (argc != 2) {
		pr_err("Err write binder_delay argc not 2");
		return -EFAULT;
	}

	raw_spin_lock_irqsave(&binder_log_lock, lock_flags);
	t = get_task_node_by_pid(pid);
	if (t != NULL) {
		rb_erase(&t->rb_node, &task_tree);
		RB_CLEAR_NODE(&t->rb_node);
		task_node_free(t);
	}
	raw_spin_unlock_irqrestore(&binder_log_lock, lock_flags);

	return count;
}

static ssize_t delay_read(struct file *file, char __user *buf,
                        size_t count, loff_t *ppos)
{
	struct task_struct *task = current;
	unsigned long flags;
	struct delay_struct d = {};
	u64 blkio_delay, swapin_delay, freepages_delay;
	loff_t dummy_pos = 0;
	if (!task)
		return -ESRCH;

	get_task_struct(task);
	raw_spin_lock_irqsave(&task->delays->lock, flags);
	blkio_delay = task->delays->blkio_delay;
	swapin_delay = task->delays->swapin_delay;
	freepages_delay = task->delays->freepages_delay;
	raw_spin_unlock_irqrestore(&task->delays->lock, flags);

	d.version = 3;
	d.blkio_delay = blkio_delay;
	d.swapin_delay = swapin_delay;
	d.freepages_delay = freepages_delay;
	if (likely(sched_info_on())) {
		d.cpu_runtime = task->se.sum_exec_runtime;
		d.cpu_run_delay = task->sched_info.run_delay;
	}
	d.utime = task->utime;
	d.stime = task->stime;

	put_task_struct(task);

	return simple_read_from_buffer(buf, count, &dummy_pos, &d, sizeof(struct delay_struct));
}

static const struct proc_ops proc_binder_start_file_operations = {
	.proc_write = binder_delay_write,
	.proc_read = binder_delay_read,
};

static const struct proc_ops proc_delay_file_operations = {
	.proc_read = delay_read,
	.proc_lseek = default_llseek,
};

static int __init mi_log_init(void)
{
	mi_log_proc = proc_mkdir("mi_log", NULL);
	if (!mi_log_proc) {
		pr_err("failed to create mi log node.\n");
		goto failed_to_create_sysfs;
	}

	delay_entry = proc_create("delay", S_IRUGO,
			mi_log_proc, &proc_delay_file_operations);

	if (task_node_init()) {
		binder_delay_entry = proc_create("binder_delay", S_IRUGO | S_IWUGO,
				mi_log_proc, &proc_binder_start_file_operations);

		register_trace_android_vh_binder_proc_transaction(binder_proc_transaction_handler, NULL);
		register_trace_android_vh_binder_reply(binder_reply_handler, NULL);
	}

	pr_info("mi_log init ok\n");
	return RET_OK;

failed_to_create_sysfs:
	return RET_FAIL;
}

static void __exit mi_log_exit(void)
{
	if (mi_log_proc) {
		if (delay_entry)
			proc_remove(delay_entry);

		if (binder_delay_entry)
			proc_remove(binder_delay_entry);

		unregister_trace_android_vh_binder_proc_transaction(binder_proc_transaction_handler, NULL);
		unregister_trace_android_vh_binder_reply(binder_reply_handler, NULL);

		proc_remove(mi_log_proc);
	}
}

module_init(mi_log_init);
module_exit(mi_log_exit);

MODULE_AUTHOR("wangzhaoliang<wangzhaoliang@xaiomi.com>");
MODULE_DESCRIPTION("mi log");
MODULE_LICENSE("GPL");
