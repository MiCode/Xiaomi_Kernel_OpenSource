/* binder.c
 *
 * Android IPC Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
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
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <asm/cacheflush.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/nsproxy.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/pid_namespace.h>
#include <linux/security.h>
#include <linux/spinlock.h>

#include "binder.h"
#include "binder_alloc.h"
#include "binder_trace.h"

static HLIST_HEAD(binder_deferred_list);
static DEFINE_MUTEX(binder_deferred_lock);

static HLIST_HEAD(binder_devices);
static HLIST_HEAD(binder_procs);
static DEFINE_MUTEX(binder_procs_lock);

static DEFINE_MUTEX(binder_context_mgr_node_lock);

static struct dentry *binder_debugfs_dir_entry_root;
static struct dentry *binder_debugfs_dir_entry_proc;

static int binder_last_id;
static struct workqueue_struct *binder_deferred_workqueue;

#define BINDER_DEBUG_ENTRY(name) \
static int binder_##name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, binder_##name##_show, inode->i_private); \
} \
\
static const struct file_operations binder_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = binder_##name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

static int binder_proc_show(struct seq_file *m, void *unused);
BINDER_DEBUG_ENTRY(proc);

/* This is only defined in include/asm-arm/sizes.h */
#ifndef SZ_1K
#define SZ_1K                               0x400
#endif

#ifndef SZ_4M
#define SZ_4M                               0x400000
#endif

#define FORBIDDEN_MMAP_FLAGS                (VM_WRITE)

#define BINDER_SMALL_BUF_SIZE (PAGE_SIZE * 64)

#define BINDER_NODE_INC                     1
#define BINDER_NODE_DEC                     2

enum {
	BINDER_DEBUG_USER_ERROR             = 1U << 0,
	BINDER_DEBUG_FAILED_TRANSACTION     = 1U << 1,
	BINDER_DEBUG_DEAD_TRANSACTION       = 1U << 2,
	BINDER_DEBUG_OPEN_CLOSE             = 1U << 3,
	BINDER_DEBUG_DEAD_BINDER            = 1U << 4,
	BINDER_DEBUG_DEATH_NOTIFICATION     = 1U << 5,
	BINDER_DEBUG_READ_WRITE             = 1U << 6,
	BINDER_DEBUG_USER_REFS              = 1U << 7,
	BINDER_DEBUG_THREADS                = 1U << 8,
	BINDER_DEBUG_TRANSACTION            = 1U << 9,
	BINDER_DEBUG_TRANSACTION_COMPLETE   = 1U << 10,
	BINDER_DEBUG_FREE_BUFFER            = 1U << 11,
	BINDER_DEBUG_INTERNAL_REFS          = 1U << 12,
	BINDER_DEBUG_BUFFER_ALLOC           = 1U << 13,
	BINDER_DEBUG_PRIORITY_CAP           = 1U << 14,
	BINDER_DEBUG_BUFFER_ALLOC_ASYNC     = 1U << 15,
	BINDER_DEBUG_SPINLOCKS              = 1U << 16,
	BINDER_DEBUG_TODO_LISTS             = 1U << 17,
};
static uint32_t binder_debug_mask = BINDER_DEBUG_USER_ERROR |
	BINDER_DEBUG_FAILED_TRANSACTION | BINDER_DEBUG_DEAD_TRANSACTION;
module_param_named(debug_mask, binder_debug_mask, uint, S_IWUSR | S_IRUGO);

static char *binder_devices_param = CONFIG_ANDROID_BINDER_DEVICES;
module_param_named(devices, binder_devices_param, charp, S_IRUGO);

static DECLARE_WAIT_QUEUE_HEAD(binder_user_error_wait);
static int binder_stop_on_user_error;

static int binder_set_stop_on_user_error(const char *val,
					 struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (binder_stop_on_user_error < 2)
		wake_up(&binder_user_error_wait);
	return ret;
}
module_param_call(stop_on_user_error, binder_set_stop_on_user_error,
	param_get_int, &binder_stop_on_user_error, S_IWUSR | S_IRUGO);

#define binder_debug(mask, x...) \
	do { \
		if (binder_debug_mask & mask) \
			pr_info(x); \
	} while (0)

#define binder_user_error(x...) \
	do { \
		if (binder_debug_mask & BINDER_DEBUG_USER_ERROR) \
			pr_info(x); \
		if (binder_stop_on_user_error) \
			binder_stop_on_user_error = 2; \
	} while (0)

#define to_flat_binder_object(hdr) \
	container_of(hdr, struct flat_binder_object, hdr)

#define to_binder_fd_object(hdr) container_of(hdr, struct binder_fd_object, hdr)

#define to_binder_buffer_object(hdr) \
	container_of(hdr, struct binder_buffer_object, hdr)

#define to_binder_fd_array_object(hdr) \
	container_of(hdr, struct binder_fd_array_object, hdr)

enum binder_stat_types {
	BINDER_STAT_PROC,
	BINDER_STAT_THREAD,
	BINDER_STAT_NODE,
	BINDER_STAT_REF,
	BINDER_STAT_DEATH,
	BINDER_STAT_TRANSACTION,
	BINDER_STAT_TRANSACTION_COMPLETE,
	BINDER_STAT_COUNT
};

struct binder_stats {
	atomic_t br[_IOC_NR(BR_FAILED_REPLY) + 1];
	atomic_t bc[_IOC_NR(BC_REPLY_SG) + 1];
	atomic_t obj_created[BINDER_STAT_COUNT];
	atomic_t obj_deleted[BINDER_STAT_COUNT];
	atomic_t obj_zombie[BINDER_STAT_COUNT];
};

static struct binder_stats binder_stats;

static inline void binder_stats_deleted(enum binder_stat_types type)
{
	atomic_inc(&binder_stats.obj_deleted[type]);
}

static inline void binder_stats_created(enum binder_stat_types type)
{
	atomic_inc(&binder_stats.obj_created[type]);
}

static inline void binder_stats_zombie(enum binder_stat_types type)
{
	atomic_inc(&binder_stats.obj_zombie[type]);
}

static inline void binder_stats_delete_zombie(enum binder_stat_types type)
{
	atomic_dec(&binder_stats.obj_zombie[type]);
	binder_stats_deleted(type);
}

struct binder_transaction_log_entry {
	int debug_id;
	int call_type;
	int from_proc;
	int from_thread;
	int target_handle;
	int to_proc;
	int to_thread;
	int to_node;
	int data_size;
	int offsets_size;
	int return_error_line;
	uint32_t return_error;
	uint32_t return_error_param;
	const char *context_name;
};
struct binder_transaction_log {
	int next;
	int full;
	struct binder_transaction_log_entry entry[32];
};
static struct binder_transaction_log binder_transaction_log;
static struct binder_transaction_log binder_transaction_log_failed;

static struct binder_transaction_log_entry *binder_transaction_log_add(
	struct binder_transaction_log *log)
{
	struct binder_transaction_log_entry *e;

	e = &log->entry[log->next];
	memset(e, 0, sizeof(*e));
	log->next++;
	if (log->next == ARRAY_SIZE(log->entry)) {
		log->next = 0;
		log->full = 1;
	}
	return e;
}

struct binder_context {
	struct binder_node *binder_context_mgr_node;
	kuid_t binder_context_mgr_uid;
	const char *name;
	bool inherit_fifo_prio;
};

struct binder_device {
	struct hlist_node hlist;
	struct miscdevice miscdev;
	struct binder_context context;
};

struct binder_worklist {
	spinlock_t lock;
	struct list_head list;
	bool freeze;
};

struct binder_work {
	struct list_head entry;
	struct binder_worklist *wlist;
	int last_line;

	enum {
		BINDER_WORK_TRANSACTION = 1,
		BINDER_WORK_TRANSACTION_COMPLETE,
		BINDER_WORK_NODE,
		BINDER_WORK_DEAD_BINDER,
		BINDER_WORK_DEAD_BINDER_AND_CLEAR,
		BINDER_WORK_CLEAR_DEATH_NOTIFICATION,
	} type;
};

struct binder_node {
	int debug_id;
	int last_op;
	int last_line;
	struct binder_work work;
	union {
		struct rb_node rb_node;
		struct hlist_node dead_node;
	};
	struct binder_proc *proc;
	struct hlist_head refs;
	int internal_strong_refs;
	int local_weak_refs;
	int local_strong_refs;
	binder_uintptr_t ptr;
	binder_uintptr_t cookie;
	unsigned has_strong_ref:1;
	unsigned pending_strong_ref:1;
	unsigned has_weak_ref:1;
	unsigned pending_weak_ref:1;
	unsigned has_async_transaction:1;
	unsigned accept_fds:1;
	unsigned min_priority:8;
	bool is_zombie;
	struct binder_worklist async_todo;
};

struct binder_ref_death {
	struct binder_work work;
	binder_uintptr_t cookie;
	struct binder_proc *wait_proc;
};

struct binder_ref {
	/* Lookups needed: */
	/*   node + proc => ref (transaction) */
	/*   desc + proc => ref (transaction, inc/dec ref) */
	/*   node => refs + procs (proc exit) */
	int debug_id;
	struct rb_node rb_node_desc;
	struct rb_node rb_node_node;
	struct hlist_node node_entry;
	struct binder_proc *proc;
	struct binder_node *node;
	uint32_t desc;
	atomic_t strong;
	atomic_t weak;
	bool is_zombie;
	struct hlist_node zombie_ref;
	struct binder_ref_death *death;
};

enum binder_deferred_state {
	BINDER_DEFERRED_PUT_FILES    = 0x01,
	BINDER_DEFERRED_FLUSH        = 0x02,
	BINDER_DEFERRED_RELEASE      = 0x04,
	BINDER_ZOMBIE_CLEANUP        = 0x08,
};

struct binder_seq_head {
	int active_count;
	int max_active_count;
	spinlock_t lock;
	struct list_head active_threads;
	u64 lowest_seq;
};

#define SEQ_BUCKETS 16
struct binder_seq_head binder_active_threads[SEQ_BUCKETS];
struct binder_seq_head zombie_procs;

static inline int binder_seq_hash(struct binder_thread *thread)
{
	u64 tp = (u64)thread;

	return ((tp>>8) ^ (tp>>12)) % SEQ_BUCKETS;
}

struct binder_seq_node {
	struct list_head list_node;
	u64 active_seq;
};

struct binder_proc {
	struct hlist_node proc_node;
	struct rb_root threads;
	struct rb_root nodes;
	struct rb_root refs_by_desc;
	struct rb_root refs_by_node;
	struct list_head waiting_threads;
	int pid;
	int active_thread_count;
	struct task_struct *tsk;
	struct files_struct *files;
	struct files_struct *zombie_files;
	struct hlist_node deferred_work_node;
	int deferred_work;
	spinlock_t proc_lock;
	spinlock_t todo_lock;
	struct binder_worklist todo;
	struct binder_stats stats;
	struct binder_worklist delivered_death;
	int max_threads;
	int requested_threads;
	int requested_threads_started;
	atomic_t ready_threads;
	long default_priority;
	struct dentry *debugfs_entry;
	struct binder_seq_node zombie_proc;
	bool is_zombie;
	struct hlist_head zombie_nodes;
	struct hlist_head zombie_refs;
	struct hlist_head zombie_threads;
	struct binder_alloc alloc;
	struct binder_context *context;
};

enum {
	BINDER_LOOPER_STATE_REGISTERED  = 0x01,
	BINDER_LOOPER_STATE_ENTERED     = 0x02,
	BINDER_LOOPER_STATE_EXITED      = 0x04,
	BINDER_LOOPER_STATE_INVALID     = 0x08,
	BINDER_LOOPER_STATE_WAITING     = 0x10,
	BINDER_LOOPER_STATE_POLL        = 0x20,
};

struct binder_thread {
	struct binder_proc *proc;
	union {
		struct rb_node rb_node;
		struct hlist_node zombie_thread;
	};
	struct binder_seq_node active_node;
	struct list_head waiting_thread_node;

	int pid;
	int looper;              /* only modified by this thread */
	bool looper_need_return; /* can be written by other thread */
	struct binder_transaction *transaction_stack;
	struct binder_worklist todo;
	uint32_t return_error; /* Write failed, return error code in read buf */
	uint32_t return_error2; /* Write failed, return error code in read */
		/* buffer. Used when sending a reply to a dead process that */
		/* we are also waiting on */
	wait_queue_head_t wait;
	bool is_zombie;
	struct binder_stats stats;
	struct task_struct *task;
};

static void binder_init_worklist(struct binder_worklist *wlist)
{
	spin_lock_init(&wlist->lock);
	INIT_LIST_HEAD(&wlist->list);
	wlist->freeze = false;
}

static void binder_freeze_worklist(struct binder_worklist *wlist)
{
	wlist->freeze = true;
}

static void binder_unfreeze_worklist(struct binder_worklist *wlist)
{
	wlist->freeze = false;
}

static inline bool _binder_worklist_empty(struct binder_worklist *wlist)
{
	BUG_ON(!spin_is_locked(&wlist->lock));
	return wlist->freeze || list_empty(&wlist->list);
}

static inline bool binder_worklist_empty(struct binder_worklist *wlist)
{
	bool ret;

	spin_lock(&wlist->lock);
	ret = _binder_worklist_empty(wlist);
	spin_unlock(&wlist->lock);
	return ret;
}

static void
binder_proc_lock(struct binder_proc *proc, int line)
{
	binder_debug(BINDER_DEBUG_SPINLOCKS,
		     "%s: line=%d\n", __func__, line);
	spin_lock(&proc->proc_lock);
}

static void
binder_proc_unlock(struct binder_proc *proc, int line)
{
	binder_debug(BINDER_DEBUG_SPINLOCKS,
		     "%s: line=%d\n", __func__, line);
	spin_unlock(&proc->proc_lock);
}

static inline void
binder_enqueue_work(struct binder_work *work,
		    struct binder_worklist *target_wlist,
		    int line)
{
	binder_debug(BINDER_DEBUG_TODO_LISTS,
		     "%s: line=%d last_line=%d\n", __func__,
		     line, work->last_line);
	spin_lock(&target_wlist->lock);
	BUG_ON(work->wlist != NULL);
	BUG_ON(target_wlist == NULL);
	work->wlist = target_wlist;
	list_add_tail(&work->entry, &target_wlist->list);
	work->last_line = line;
	spin_unlock(&target_wlist->lock);
}

static inline void
_binder_dequeue_work(struct binder_work *work, int line)
{
	binder_debug(BINDER_DEBUG_TODO_LISTS,
		     "%s: line=%d last_line=%d\n", __func__,
		     line, work->last_line);
	list_del_init(&work->entry);
	/* Add barrier to ensure list delete is seen */
	smp_mb();
	work->wlist = NULL;
	work->last_line = -line;
}

static inline void
binder_dequeue_work(struct binder_work *work, int line)
{
	struct binder_worklist *wlist = work->wlist;

	while (wlist) {
		spin_lock(&wlist->lock);
		if (wlist == work->wlist) {
			_binder_dequeue_work(work, line);
			spin_unlock(&wlist->lock);
			return;
		}
		spin_unlock(&wlist->lock);
		wlist = work->wlist;
	}
	/* Add barrier to ensure list delete is visible */
	smp_mb();
}

struct binder_priority {
	int sched_policy;
	long nice;       /* valid when sched_policy = SCHED_NORMAL */
	int rt_priority; /* valid when sched_policy = SCHED_RR or SCHED_FIFO */
};

struct binder_transaction {
	int debug_id;
	struct binder_work work;
	struct binder_thread *from;
	struct binder_transaction *from_parent;
	struct binder_proc *to_proc;
	struct binder_thread *to_thread;
	struct binder_transaction *to_parent;
	unsigned need_reply:1;
	/* unsigned is_dead:1; */	/* not used at the moment */

	struct binder_buffer *buffer;
	unsigned int	code;
	unsigned int	flags;
	struct binder_priority priority;
	struct binder_priority saved_priority;
	bool    set_priority_called;
	kuid_t	sender_euid;
};

static void
binder_defer_work(struct binder_proc *proc, enum binder_deferred_state defer);
static inline void binder_queue_for_zombie_cleanup(struct binder_proc *proc);

static void binder_put_thread(struct binder_thread *thread);
static struct binder_thread *binder_get_thread(struct binder_proc *proc);

static int task_get_unused_fd_flags(struct binder_proc *proc, int flags)
{
	struct files_struct *files = proc->files;
	unsigned long rlim_cur;
	unsigned long irqs;

	if (files == NULL)
		return -ESRCH;

	if (!lock_task_sighand(proc->tsk, &irqs))
		return -EMFILE;

	rlim_cur = task_rlimit(proc->tsk, RLIMIT_NOFILE);
	unlock_task_sighand(proc->tsk, &irqs);

	return __alloc_fd(files, 0, rlim_cur, flags);
}

/*
 * copied from fd_install
 */
static void task_fd_install(
	struct binder_proc *proc, unsigned int fd, struct file *file)
{
	if (proc->files)
		__fd_install(proc->files, fd, file);
}

/*
 * copied from sys_close
 */
static long task_close_fd(struct binder_proc *proc, unsigned int fd)
{
	int retval;

	if (proc->files == NULL)
		return -ESRCH;

	retval = __close_fd(proc->files, fd);
	/* can't restart close syscall because file table entry was cleared */
	if (unlikely(retval == -ERESTARTSYS ||
		     retval == -ERESTARTNOINTR ||
		     retval == -ERESTARTNOHAND ||
		     retval == -ERESTART_RESTARTBLOCK))
		retval = -EINTR;

	return retval;
}

static inline bool binder_has_work(struct binder_thread *thread,
				   bool do_proc_work)
{
	return !binder_worklist_empty(&thread->todo) ||
		thread->return_error != BR_OK ||
		READ_ONCE(thread->looper_need_return) ||
		(do_proc_work && !binder_worklist_empty(&thread->proc->todo));
}

static bool binder_available_for_proc_work(struct binder_thread *thread)
{
	return !thread->transaction_stack &&
		binder_worklist_empty(&thread->todo) &&
		(thread->looper & (BINDER_LOOPER_STATE_ENTERED |
				   BINDER_LOOPER_STATE_REGISTERED));
}

static void binder_wakeup_poll_threads(struct binder_proc *proc, bool sync)
{
	struct rb_node *n;
	struct binder_thread *thread;

	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n)) {
		thread = rb_entry(n, struct binder_thread, rb_node);
		if (thread->looper & BINDER_LOOPER_STATE_POLL &&
		    binder_available_for_proc_work(thread)) {
			if (sync)
				wake_up_interruptible_sync(&thread->wait);
			else
				wake_up_interruptible(&thread->wait);
		}
	}
}

/**
 * binder_select_thread() - selects a thread for doing proc work.
 * @proc:	process to select a thread from
 *
 * Note that calling this function moves the thread off the waiting_threads
 * list, so it can only be woken up by the caller of this function, or a
 * signal. Therefore, callers *should* always wake up the thread this function
 * returns.
 *
 * Return:	If there's a thread currently waiting for process work,
 *		returns that thread. Otherwise returns NULL.
 */
static struct binder_thread *binder_select_thread(struct binder_proc *proc)
{
	struct binder_thread *thread;

	BUG_ON(!spin_is_locked(&proc->proc_lock));
	thread = list_first_entry_or_null(&proc->waiting_threads,
					  struct binder_thread,
					  waiting_thread_node);

	if (thread)
		list_del_init(&thread->waiting_thread_node);

	return thread;
}

static void binder_wakeup_thread(struct binder_proc *proc, bool sync)
{
	struct binder_thread *thread;

	BUG_ON(!spin_is_locked(&proc->proc_lock));
	thread = binder_select_thread(proc);
	if (thread) {
		if (sync)
			wake_up_interruptible_sync(&thread->wait);
		else
			wake_up_interruptible(&thread->wait);
		return;
	}

	/* Didn't find a thread waiting for proc work; this can happen
	 * in two scenarios:
	 * 1. All threads are busy handling transactions
	 *    In that case, one of those threads should call back into
	 *    the kernel driver soon and pick up this work.
	 * 2. Threads are using the (e)poll interface, in which case
	 *    they may be blocked on the waitqueue without having been
	 *    added to waiting_threads. For this case, we just iterate
	 *    over all threads not handling transaction work, and
	 *    wake them all up. We wake all because we don't know whether
	 *    a thread that called into (e)poll is handling non-binder
	 *    work currently.
	 */
	binder_wakeup_poll_threads(proc, sync);
}

static void binder_set_nice(struct task_struct *task, long nice)
{
	long min_nice;

	if (can_nice(task, nice)) {
		set_user_nice(task, nice);
		return;
	}
	min_nice = rlimit_to_nice(task->signal->rlim[RLIMIT_NICE].rlim_cur);
	binder_debug(BINDER_DEBUG_PRIORITY_CAP,
		     "%d: nice value %ld not allowed use %ld instead\n",
		      task->pid, nice, min_nice);
	set_user_nice(task, min_nice);
	if (min_nice <= MAX_NICE)
		return;
	binder_user_error("%d RLIMIT_NICE not set\n", task->pid);
}

static inline int is_rt_policy(int sched_policy)
{
	return (sched_policy == SCHED_FIFO || sched_policy == SCHED_RR);
}

static void binder_set_priority(
	struct task_struct *task,
	struct binder_transaction *t, struct binder_node *target_node)
{
	bool oneway = !!(t->flags & TF_ONE_WAY);
	bool inherit_fifo = target_node->proc->context->inherit_fifo_prio;

	t->saved_priority.sched_policy = task->policy;
	t->saved_priority.nice = task_nice(task);
	t->set_priority_called = true;

	if (!oneway && inherit_fifo &&
	    is_rt_policy(t->priority.sched_policy) &&
	    !is_rt_policy(task->policy)) {
		/* Transaction was initiated with a real-time policy,
		 * but we are not; temporarily upgrade this thread to RT.
		 */
		struct sched_param params = {t->priority.rt_priority};

		sched_setscheduler_nocheck(task,
					   t->priority.sched_policy |
					   SCHED_RESET_ON_FORK,
					   &params);
	} else if (!is_rt_policy(task->policy)) {
		/* Neither policy is real-time, fall back to setting nice. */
		if (t->priority.nice < target_node->min_priority && !oneway)
			binder_set_nice(task, t->priority.nice);
		else if (!oneway ||
			 t->saved_priority.nice > target_node->min_priority)
			binder_set_nice(task, target_node->min_priority);
	} else {
		/* Cases where we do nothing:
		 * 1. Both source and target threads have a real-time policy
		 * 2. Source does not have a real-time policy, but the target
		 * does.
		 */
	}
}

static void binder_restore_priority(
		const struct binder_priority *saved_priority)
{
	struct sched_param params = {0};

	if (current->policy != saved_priority->sched_policy) {
		/* Binder only transitions from a non-RT to a RT
		 * policy; therefore, the restore should always
		 * be a non-RT policy, and params.priority is not
		 * relevant.
		 */
		sched_setscheduler_nocheck(current,
					   saved_priority->sched_policy,
					   &params);
	}
	if (!is_rt_policy(saved_priority->sched_policy))
		binder_set_nice(current, saved_priority->nice);
}

static struct binder_node *binder_get_node(struct binder_proc *proc,
					   binder_uintptr_t ptr)
{
	struct rb_node *n;
	struct binder_node *node;

	binder_proc_lock(proc, __LINE__);
	n = proc->nodes.rb_node;
	while (n) {
		node = rb_entry(n, struct binder_node, rb_node);

		if (ptr < node->ptr)
			n = n->rb_left;
		else if (ptr > node->ptr)
			n = n->rb_right;
		else {
			node->local_weak_refs++;
			binder_proc_unlock(proc, __LINE__);
			return node;
		}
	}
	binder_proc_unlock(proc, __LINE__);

	return NULL;
}

static void _binder_make_node_zombie(struct binder_node *node)
{
	struct binder_proc *proc = node->proc;

	BUG_ON(node->is_zombie);
	BUG_ON(!spin_is_locked(&proc->proc_lock));
	rb_erase(&node->rb_node, &proc->nodes);
	INIT_HLIST_NODE(&node->dead_node);
	node->is_zombie = true;
	hlist_add_head(&node->dead_node, &proc->zombie_nodes);
	binder_queue_for_zombie_cleanup(proc);
	binder_stats_zombie(BINDER_STAT_NODE);
}

static struct binder_node *binder_new_node(struct binder_proc *proc,
					   binder_uintptr_t ptr,
					   binder_uintptr_t cookie)
{
	struct rb_node **p = &proc->nodes.rb_node;
	struct rb_node *parent = NULL;
	struct binder_node *node, *temp_node;

	temp_node = kzalloc(sizeof(*node), GFP_KERNEL);

	binder_proc_lock(proc, __LINE__);
	while (*p) {
		parent = *p;
		node = rb_entry(parent, struct binder_node, rb_node);

		if (ptr < node->ptr)
			p = &(*p)->rb_left;
		else if (ptr > node->ptr)
			p = &(*p)->rb_right;
		else {
			node->local_weak_refs++;
			binder_proc_unlock(proc, __LINE__);
			kfree(temp_node);
			return node;
		}
	}

	node = temp_node;
	if (node == NULL) {
		binder_proc_unlock(proc, __LINE__);
		return NULL;
	}
	binder_stats_created(BINDER_STAT_NODE);

	node->debug_id = ++binder_last_id;
	node->proc = proc;
	node->ptr = ptr;
	node->cookie = cookie;
	node->work.type = BINDER_WORK_NODE;
	INIT_LIST_HEAD(&node->work.entry);
	INIT_HLIST_HEAD(&node->refs);
	binder_init_worklist(&node->async_todo);

	binder_debug(BINDER_DEBUG_INTERNAL_REFS,
		     "%d:%d node %d u%016llx c%016llx created\n",
		     proc->pid, current->pid, node->debug_id,
		     (u64)node->ptr, (u64)node->cookie);

	rb_link_node(&node->rb_node, parent, p);
	rb_insert_color(&node->rb_node, &proc->nodes);
	node->local_weak_refs++;
	binder_proc_unlock(proc, __LINE__);

	return node;
}

static int binder_inc_node(struct binder_node *node, int strong, int internal,
			   struct binder_worklist *target_list, int line)
{
	int ret = 0;
	struct binder_proc *proc = node->proc;

	binder_proc_lock(proc, __LINE__);

	node->last_line = line;
	node->last_op = BINDER_NODE_INC;

	if (strong) {
		if (internal) {
			if (target_list == NULL &&
			    node->internal_strong_refs == 0 &&
			    !(node->proc &&
			      node == node->proc->context->
				      binder_context_mgr_node &&
			      node->has_strong_ref)) {
				pr_err("invalid inc strong node for %d\n",
					node->debug_id);
				ret = -EINVAL;
				goto done;
			}
			node->internal_strong_refs++;
		} else
			node->local_strong_refs++;

		if (!node->has_strong_ref && target_list) {
			binder_dequeue_work(&node->work, __LINE__);
			binder_enqueue_work(&node->work, target_list,
					    __LINE__);
		}

	} else {
		if (!internal)
			node->local_weak_refs++;
		if (!node->has_weak_ref && list_empty(&node->work.entry)) {
			if (target_list == NULL) {
				pr_err("invalid inc weak node for %d\n",
					node->debug_id);
				ret = -EINVAL;
				goto done;
			}

			binder_enqueue_work(&node->work, target_list,
					    __LINE__);
		}
	}
done:
	binder_proc_unlock(proc, __LINE__);
	return ret;
}

static int binder_dec_node(struct binder_node *node, int strong, int internal,
			   int line)
{
	bool do_wakeup = false;
	struct binder_proc *proc = node->proc;

	binder_proc_lock(proc, __LINE__);

	node->last_line = line;
	node->last_op = BINDER_NODE_DEC;

	if (strong) {
		if (internal)
			node->internal_strong_refs--;
		else
			node->local_strong_refs--;
		if (node->local_strong_refs || node->internal_strong_refs)
			goto done;
	} else {
		if (!internal)
			node->local_weak_refs--;
		if (node->local_weak_refs || !hlist_empty(&node->refs))
			goto done;
	}

	if (node->has_strong_ref || node->has_weak_ref) {
		if (list_empty(&node->work.entry)) {
			binder_enqueue_work(&node->work,
					    &proc->todo, __LINE__);
			do_wakeup = true;
		}

	} else {
		if (hlist_empty(&node->refs) && !node->local_strong_refs &&
		    !node->local_weak_refs) {
			binder_dequeue_work(&node->work, __LINE__);
			if (!node->is_zombie) {
				_binder_make_node_zombie(node);
				binder_debug(BINDER_DEBUG_INTERNAL_REFS,
					     "refless node %d deleted\n",
					     node->debug_id);
			} else {
				binder_debug(BINDER_DEBUG_INTERNAL_REFS,
					     "dead node %d deleted\n",
					     node->debug_id);
			}
		}
	}
done:
	if (do_wakeup)
		binder_wakeup_thread(proc, false);

	binder_proc_unlock(proc, __LINE__);

	return 0;
}

static inline void binder_put_node(struct binder_node *node)
{
	binder_dec_node(node, 0, 0, __LINE__);
}

static struct binder_ref *binder_get_ref(struct binder_proc *proc,
					 uint32_t desc, bool need_strong_ref)
{
	struct rb_node *n;
	struct binder_ref *ref;

	binder_proc_lock(proc, __LINE__);
	n = proc->refs_by_desc.rb_node;
	while (n) {
		ref = rb_entry(n, struct binder_ref, rb_node_desc);

		if (desc < ref->desc) {
			n = n->rb_left;
		} else if (desc > ref->desc) {
			n = n->rb_right;
		} else if (need_strong_ref && !atomic_read(&ref->strong)) {
			binder_user_error("tried to use weak ref as strong ref\n");
			binder_proc_unlock(proc, __LINE__);
			return NULL;
		} else {
			/*
			 * We take an implicit weak reference to ensure
			 * that the ref is not used and deref'd before the
			 * caller has a chance to add a reference. The caller
			 * must use binder_put_ref to indicate completion
			 * of the operation on the ref
			 *
			 * This ref is orthogonal to any existing weak/strong
			 * reference taken by the callers (no relation to the
			 * passed-in need_strong_ref).
			 */
			atomic_inc(&ref->weak);
			binder_proc_unlock(proc, __LINE__);
			return ref;
		}
	}
	binder_proc_unlock(proc, __LINE__);
	return NULL;
}

static struct binder_ref *binder_get_ref_for_node(struct binder_proc *proc,
					struct binder_node *node,
					struct binder_worklist *target_list)
{
	struct rb_node *n;
	struct rb_node **p = &proc->refs_by_node.rb_node;
	struct rb_node *parent = NULL;
	struct binder_ref *ref, *new_ref;
	struct binder_context *context = proc->context;
	struct binder_proc *node_proc = node->proc;

	binder_proc_lock(proc, __LINE__);
	while (*p) {
		parent = *p;
		ref = rb_entry(parent, struct binder_ref, rb_node_node);

		if (node < ref->node)
			p = &(*p)->rb_left;
		else if (node > ref->node)
			p = &(*p)->rb_right;
		else {
			atomic_inc(&ref->weak);
			binder_proc_unlock(proc, __LINE__);
			return ref;
		}
	}
	binder_proc_unlock(proc, __LINE__);

	/* Need to allocate a new ref */
	new_ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (new_ref == NULL)
		return NULL;

	binder_stats_created(BINDER_STAT_REF);
	new_ref->debug_id = ++binder_last_id;
	new_ref->proc = proc;
	new_ref->node = node;
	atomic_set(&new_ref->strong, 0);
	/*
	 * We take an implicit weak reference to ensure
	 * that the ref is not used and deref'd before the
	 * caller has a chance to add a reference. The caller
	 * must use binder_put_ref to indicate completion
	 * of the operation on the ref
	 */
	atomic_set(&new_ref->weak, 1);

	/*
	 * Attach the new ref to the node before
	 * making it visible for lookups (needed
	 * to insure a weak ref on the node)
	 */
	binder_proc_lock(node_proc, __LINE__);
	if (node->is_zombie && hlist_empty(&node->refs)) {
		/*
		 * Do not allow new refs to unreferenced zombie nodes
		 */
		binder_proc_unlock(node_proc, __LINE__);
		binder_debug(BINDER_DEBUG_INTERNAL_REFS,
		     "%d attempt to take new ref %d desc %d on unreferenced zombie node\n",
		      proc->pid, new_ref->debug_id, new_ref->desc);
		kfree(new_ref);
		binder_stats_deleted(BINDER_STAT_REF);
		return NULL;
	}
	INIT_HLIST_NODE(&new_ref->node_entry);
	hlist_add_head(&new_ref->node_entry, &node->refs);

	binder_debug(BINDER_DEBUG_INTERNAL_REFS,
		     "%d new ref %d desc %d for node\n",
		      proc->pid, new_ref->debug_id, new_ref->desc);
	binder_proc_unlock(node_proc, __LINE__);

	binder_proc_lock(proc, __LINE__);
	/*
	 * Since we dropped the proc lock, we need to
	 * recompute the insertion point
	 */
	p = &proc->refs_by_node.rb_node;
	while (*p) {
		parent = *p;
		ref = rb_entry(parent, struct binder_ref, rb_node_node);

		if (node < ref->node)
			p = &(*p)->rb_left;
		else if (node > ref->node)
			p = &(*p)->rb_right;
		else {
			/*
			 * ref already created by another thread
			 * disconnect and free the new ref
			 */
			if (!ref->is_zombie)
				atomic_inc(&ref->weak);
			else
				ref = NULL;
			binder_proc_unlock(proc, __LINE__);
			binder_proc_lock(node_proc, __LINE__);
			hlist_del(&new_ref->node_entry);
			binder_proc_unlock(node_proc, __LINE__);
			kfree(new_ref);
			binder_stats_deleted(BINDER_STAT_REF);
			return ref;
		}
	}
	rb_link_node(&new_ref->rb_node_node, parent, p);
	rb_insert_color(&new_ref->rb_node_node, &proc->refs_by_node);

	new_ref->desc = (node == context->binder_context_mgr_node) ? 0 : 1;
	for (n = rb_first(&proc->refs_by_desc); n != NULL; n = rb_next(n)) {
		ref = rb_entry(n, struct binder_ref, rb_node_desc);
		if (ref->desc > new_ref->desc)
			break;
		new_ref->desc = ref->desc + 1;
	}

	p = &proc->refs_by_desc.rb_node;
	while (*p) {
		parent = *p;
		ref = rb_entry(parent, struct binder_ref, rb_node_desc);

		if (new_ref->desc < ref->desc)
			p = &(*p)->rb_left;
		else if (new_ref->desc > ref->desc)
			p = &(*p)->rb_right;
		else
			BUG();
	}
	rb_link_node(&new_ref->rb_node_desc, parent, p);
	rb_insert_color(&new_ref->rb_node_desc, &proc->refs_by_desc);
	binder_proc_unlock(proc, __LINE__);
	smp_mb();
	/*
	 * complete the implicit weak inc_ref by incrementing
	 * the node.
	 */
	binder_inc_node(new_ref->node, 0, 1, target_list, __LINE__);
	return new_ref;
}

static void binder_delete_ref(struct binder_ref *ref, bool force, int line)
{
	struct binder_proc *node_proc = ref->node->proc;

	binder_debug(BINDER_DEBUG_INTERNAL_REFS,
		     "%d delete ref %d desc %d for node %d\n",
		      ref->proc->pid, ref->debug_id, ref->desc,
		      ref->node->debug_id);

	binder_proc_lock(ref->proc, __LINE__);
	if (ref->is_zombie ||
	    (!force && ((atomic_read(&ref->strong) != 0) ||
				(atomic_read(&ref->weak) != 0)))) {
		/*
		 * Multiple threads could observe the ref counts
		 * going to 0. The first one to get the lock will
		 * make it a zombie. Subsequent callers bail out
		 * here.
		 */
		binder_proc_unlock(ref->proc, __LINE__);
		return;
	}

	ref->is_zombie = true;
	rb_erase(&ref->rb_node_desc, &ref->proc->refs_by_desc);
	rb_erase(&ref->rb_node_node, &ref->proc->refs_by_node);
	if (ref->death) {
		binder_debug(BINDER_DEBUG_DEAD_BINDER,
			     "%d delete ref %d desc %d has death notification\n",
			      ref->proc->pid, ref->debug_id, ref->desc);
		if (ref->death->work.wlist)
			binder_dequeue_work(&ref->death->work, __LINE__);
		binder_stats_zombie(BINDER_STAT_DEATH);
	}
	INIT_HLIST_NODE(&ref->zombie_ref);
	hlist_add_head(&ref->zombie_ref, &ref->proc->zombie_refs);
	binder_queue_for_zombie_cleanup(ref->proc);
	binder_proc_unlock(ref->proc, __LINE__);

	binder_proc_lock(node_proc, __LINE__);
	if (ref->node->is_zombie)
		binder_queue_for_zombie_cleanup(node_proc);
	hlist_del(&ref->node_entry);
	binder_proc_unlock(node_proc, __LINE__);

	if (atomic_read(&ref->strong))
		binder_dec_node(ref->node, 1, 1, line);
	binder_dec_node(ref->node, 0, 1, line);

	binder_stats_zombie(BINDER_STAT_REF);
}

static int binder_inc_ref(struct binder_ref *ref, int strong,
			  struct binder_worklist *target_list, int line)
{
	int ret = 0;

	/* atomic_inc_return does not require explicit barrier */
	if ((strong && atomic_inc_return(&ref->strong) == 1) ||
	    (!strong && atomic_inc_return(&ref->weak) == 1)) {
		ret = binder_inc_node(ref->node, strong, 1, target_list, line);
		if (ret) {
			atomic_dec(strong ? &ref->strong : &ref->weak);
			smp_mb__after_atomic();
		}
	}

	return ret;
}

static int binder_dec_ref(struct binder_ref *ref, int strong, int line)
{
	if (strong) {
		/* atomic_dec_if_positive does not require explicit barrier */
		int newval = atomic_dec_if_positive(&ref->strong);

		if (newval < 0) {
			binder_user_error("%d invalid dec strong, ref %d desc %d s %d w %d\n",
					  ref->proc->pid, ref->debug_id,
					  ref->desc, atomic_read(&ref->strong),
					  atomic_read(&ref->weak));
			return -EINVAL;
		}
		if (newval == 0) {
			int ret = binder_dec_node(ref->node, strong, 1, line);
			if (ret)
				return ret;
		}
	} else {
		int newval = atomic_dec_if_positive(&ref->weak);

		if (newval < 0) {
			binder_user_error("%d invalid dec weak, ref %d desc %d s %d w %d\n",
					  ref->proc->pid, ref->debug_id,
					  ref->desc, atomic_read(&ref->strong),
					  atomic_read(&ref->weak));
			return -EINVAL;
		}
	}

	if (atomic_read(&ref->strong) == 0 && atomic_read(&ref->weak) == 0)
		/* it is possible that multiple threads could observe the
		 * strong/weak references going to 0 at the same time.
		 * This case is handled when the proc lock is acquired
		 * in binder_delete_ref()
		 */
		binder_delete_ref(ref, false, line);
	return 0;
}

static inline void binder_put_ref(struct binder_ref *ref)
{
	binder_dec_ref(ref, 0, __LINE__);
}

static void binder_pop_transaction(struct binder_thread *target_thread,
				   struct binder_transaction *t)
{
	BUG_ON(!target_thread);
	BUG_ON(!spin_is_locked(&target_thread->proc->proc_lock));

	BUG_ON(target_thread->transaction_stack != t);
	/*
	 * It is possible that the target_thread has died so
	 * transaction_stack->from could already be NULL
	 */
	BUG_ON(target_thread->transaction_stack->from &&
	       target_thread->transaction_stack->from != target_thread);
	target_thread->transaction_stack =
		target_thread->transaction_stack->from_parent;
	t->from = NULL;
}

static void binder_free_transaction(struct binder_transaction *t)
{
	t->need_reply = 0;
	if (t->buffer)
		t->buffer->transaction = NULL;
	kfree(t);
	binder_stats_deleted(BINDER_STAT_TRANSACTION);
}

static void binder_send_failed_reply(struct binder_transaction *t,
				     uint32_t error_code)
{
	struct binder_thread *target_thread;
	struct binder_transaction *next;

	BUG_ON(t->flags & TF_ONE_WAY);
	while (1) {
		target_thread = t->from;
		if (target_thread) {
			binder_proc_lock(target_thread->proc, __LINE__);
			if (target_thread->return_error != BR_OK &&
			   target_thread->return_error2 == BR_OK) {
				target_thread->return_error2 =
					target_thread->return_error;
				target_thread->return_error = BR_OK;
			}
			if (target_thread->return_error == BR_OK) {
				binder_debug(BINDER_DEBUG_FAILED_TRANSACTION,
					     "send failed reply for transaction %d to %d:%d\n",
					      t->debug_id,
					      target_thread->proc->pid,
					      target_thread->pid);

				binder_pop_transaction(target_thread, t);
				target_thread->return_error = error_code;
				binder_proc_unlock(target_thread->proc,
						   __LINE__);
				wake_up_interruptible(&target_thread->wait);
				binder_free_transaction(t);
			} else {
				binder_proc_unlock(target_thread->proc,
						   __LINE__);
				pr_err("reply failed, target thread, %d:%d, has error code %d already\n",
					target_thread->proc->pid,
					target_thread->pid,
					target_thread->return_error);
			}
			return;
		}
		next = t->from_parent;

		binder_debug(BINDER_DEBUG_FAILED_TRANSACTION,
			     "send failed reply for transaction %d, target dead\n",
			     t->debug_id);

		binder_free_transaction(t);
		if (next == NULL) {
			binder_debug(BINDER_DEBUG_DEAD_BINDER,
				     "reply failed, no target thread at root\n");
			return;
		}
		t = next;
		binder_debug(BINDER_DEBUG_DEAD_BINDER,
			     "reply failed, no target thread -- retry %d\n",
			      t->debug_id);
	}
}

/**
 * binder_validate_object() - checks for a valid metadata object in a buffer.
 * @buffer:	binder_buffer that we're parsing.
 * @offset:	offset in the buffer at which to validate an object.
 *
 * Return:	If there's a valid metadata object at @offset in @buffer, the
 *		size of that object. Otherwise, it returns zero.
 */
static size_t binder_validate_object(struct binder_buffer *buffer, u64 offset)
{
	/* Check if we can read a header first */
	struct binder_object_header *hdr;
	size_t object_size = 0;

	if (offset > buffer->data_size - sizeof(*hdr) ||
	    buffer->data_size < sizeof(*hdr) ||
	    !IS_ALIGNED(offset, sizeof(u32)))
		return 0;

	/* Ok, now see if we can read a complete object. */
	hdr = (struct binder_object_header *)(buffer->data + offset);
	switch (hdr->type) {
	case BINDER_TYPE_BINDER:
	case BINDER_TYPE_WEAK_BINDER:
	case BINDER_TYPE_HANDLE:
	case BINDER_TYPE_WEAK_HANDLE:
		object_size = sizeof(struct flat_binder_object);
		break;
	case BINDER_TYPE_FD:
		object_size = sizeof(struct binder_fd_object);
		break;
	case BINDER_TYPE_PTR:
		object_size = sizeof(struct binder_buffer_object);
		break;
	case BINDER_TYPE_FDA:
		object_size = sizeof(struct binder_fd_array_object);
		break;
	default:
		return 0;
	}
	if (offset <= buffer->data_size - object_size &&
	    buffer->data_size >= object_size)
		return object_size;
	else
		return 0;
}

/**
 * binder_validate_ptr() - validates binder_buffer_object in a binder_buffer.
 * @b:		binder_buffer containing the object
 * @index:	index in offset array at which the binder_buffer_object is
 *		located
 * @start:	points to the start of the offset array
 * @num_valid:	the number of valid offsets in the offset array
 *
 * Return:	If @index is within the valid range of the offset array
 *		described by @start and @num_valid, and if there's a valid
 *		binder_buffer_object at the offset found in index @index
 *		of the offset array, that object is returned. Otherwise,
 *		%NULL is returned.
 *		Note that the offset found in index @index itself is not
 *		verified; this function assumes that @num_valid elements
 *		from @start were previously verified to have valid offsets.
 */
static struct binder_buffer_object *binder_validate_ptr(struct binder_buffer *b,
							binder_size_t index,
							binder_size_t *start,
							binder_size_t num_valid)
{
	struct binder_buffer_object *buffer_obj;
	binder_size_t *offp;

	if (index >= num_valid)
		return NULL;

	offp = start + index;
	buffer_obj = (struct binder_buffer_object *)(b->data + *offp);
	if (buffer_obj->hdr.type != BINDER_TYPE_PTR)
		return NULL;

	return buffer_obj;
}

/**
 * binder_validate_fixup() - validates pointer/fd fixups happen in order.
 * @b:			transaction buffer
 * @objects_start	start of objects buffer
 * @buffer:		binder_buffer_object in which to fix up
 * @offset:		start offset in @buffer to fix up
 * @last_obj:		last binder_buffer_object that we fixed up in
 * @last_min_offset:	minimum fixup offset in @last_obj
 *
 * Return:		%true if a fixup in buffer @buffer at offset @offset is
 *			allowed.
 *
 * For safety reasons, we only allow fixups inside a buffer to happen
 * at increasing offsets; additionally, we only allow fixup on the last
 * buffer object that was verified, or one of its parents.
 *
 * Example of what is allowed:
 *
 * A
 *   B (parent = A, offset = 0)
 *   C (parent = A, offset = 16)
 *     D (parent = C, offset = 0)
 *   E (parent = A, offset = 32) // min_offset is 16 (C.parent_offset)
 *
 * Examples of what is not allowed:
 *
 * Decreasing offsets within the same parent:
 * A
 *   C (parent = A, offset = 16)
 *   B (parent = A, offset = 0) // decreasing offset within A
 *
 * Referring to a parent that wasn't the last object or any of its parents:
 * A
 *   B (parent = A, offset = 0)
 *   C (parent = A, offset = 0)
 *   C (parent = A, offset = 16)
 *     D (parent = B, offset = 0) // B is not A or any of A's parents
 */
static bool binder_validate_fixup(struct binder_buffer *b,
				  binder_size_t *objects_start,
				  struct binder_buffer_object *buffer,
				  binder_size_t fixup_offset,
				  struct binder_buffer_object *last_obj,
				  binder_size_t last_min_offset)
{
	if (!last_obj) {
		/* Nothing to fix up in */
		return false;
	}

	while (last_obj != buffer) {
		/*
		 * Safe to retrieve the parent of last_obj, since it
		 * was already previously verified by the driver.
		 */
		if ((last_obj->flags & BINDER_BUFFER_FLAG_HAS_PARENT) == 0)
			return false;
		last_min_offset = last_obj->parent_offset + sizeof(uintptr_t);
		last_obj = (struct binder_buffer_object *)
			(b->data + *(objects_start + last_obj->parent));
	}
	return (fixup_offset >= last_min_offset);
}

static void binder_transaction_buffer_release(struct binder_proc *proc,
					      struct binder_buffer *buffer,
					      binder_size_t *failed_at)
{
	binder_size_t *offp, *off_start, *off_end;
	int debug_id = buffer->debug_id;

	binder_debug(BINDER_DEBUG_TRANSACTION,
		     "%d buffer release %d, size %zd-%zd, failed at %pK\n",
		     proc->pid, buffer->debug_id,
		     buffer->data_size, buffer->offsets_size, failed_at);

	if (buffer->target_node)
		binder_dec_node(buffer->target_node, 1, 0, __LINE__);

	off_start = (binder_size_t *)(buffer->data +
				      ALIGN(buffer->data_size, sizeof(void *)));
	if (failed_at)
		off_end = failed_at;
	else
		off_end = (void *)off_start + buffer->offsets_size;
	for (offp = off_start; offp < off_end; offp++) {
		struct binder_object_header *hdr;
		size_t object_size = binder_validate_object(buffer, *offp);

		if (object_size == 0) {
			pr_err("transaction release %d bad object at offset %lld, size %zd\n",
			       debug_id, (u64)*offp, buffer->data_size);
			continue;
		}
		hdr = (struct binder_object_header *)(buffer->data + *offp);
		switch (hdr->type) {
		case BINDER_TYPE_BINDER:
		case BINDER_TYPE_WEAK_BINDER: {
			struct flat_binder_object *fp;
			struct binder_node *node;

			fp = to_flat_binder_object(hdr);
			node = binder_get_node(proc, fp->binder);
			if (node == NULL) {
				pr_err("transaction release %d bad node %016llx, target died\n",
				       debug_id, (u64)fp->binder);
				break;
			}
			binder_debug(BINDER_DEBUG_TRANSACTION,
				     "        node %d u%016llx\n",
				     node->debug_id, (u64)node->ptr);
			binder_dec_node(node, hdr->type == BINDER_TYPE_BINDER,
					0, __LINE__);
			binder_put_node(node);
		} break;
		case BINDER_TYPE_HANDLE:
		case BINDER_TYPE_WEAK_HANDLE: {
			struct flat_binder_object *fp;
			struct binder_ref *ref;

			fp = to_flat_binder_object(hdr);
			ref = binder_get_ref(proc, fp->handle,
					     hdr->type == BINDER_TYPE_HANDLE);
			if (ref == NULL) {
				pr_err("transaction release %d bad handle %d, target died\n",
				 debug_id, fp->handle);
				break;
			}
			binder_debug(BINDER_DEBUG_TRANSACTION,
				     "        ref %d desc %d (node %d)\n",
				     ref->debug_id, ref->desc, ref->node->debug_id);
			binder_dec_ref(ref, hdr->type == BINDER_TYPE_HANDLE,
				       __LINE__);
			binder_put_ref(ref);
		} break;

		case BINDER_TYPE_FD: {
			struct binder_fd_object *fp = to_binder_fd_object(hdr);

			binder_debug(BINDER_DEBUG_TRANSACTION,
				     "        fd %d\n", fp->fd);
			if (failed_at)
				task_close_fd(proc, fp->fd);
		} break;
		case BINDER_TYPE_PTR:
			/*
			 * Nothing to do here, this will get cleaned up when the
			 * transaction buffer gets freed
			 */
			break;
		case BINDER_TYPE_FDA: {
			struct binder_fd_array_object *fda;
			struct binder_buffer_object *parent;
			uintptr_t parent_buffer;
			u32 *fd_array;
			size_t fd_index;
			binder_size_t fd_buf_size;

			fda = to_binder_fd_array_object(hdr);
			parent = binder_validate_ptr(buffer, fda->parent,
						     off_start,
						     offp - off_start);
			if (!parent) {
				pr_err("transaction release %d bad parent offset",
				       debug_id);
				continue;
			}
			/*
			 * Since the parent was already fixed up, convert it
			 * back to kernel address space to access it
			 */
			parent_buffer = parent->buffer -
				binder_alloc_get_user_buffer_offset(
						&proc->alloc);

			fd_buf_size = sizeof(u32) * fda->num_fds;
			if (fda->num_fds >= SIZE_MAX / sizeof(u32)) {
				pr_err("transaction release %d invalid number of fds (%lld)\n",
				       debug_id, (u64)fda->num_fds);
				continue;
			}
			if (fd_buf_size > parent->length ||
			    fda->parent_offset > parent->length - fd_buf_size) {
				/* No space for all file descriptors here. */
				pr_err("transaction release %d not enough space for %lld fds in buffer\n",
				       debug_id, (u64)fda->num_fds);
				continue;
			}
			fd_array = (u32 *)(parent_buffer + fda->parent_offset);
			for (fd_index = 0; fd_index < fda->num_fds; fd_index++)
				task_close_fd(proc, fd_array[fd_index]);
		} break;
		default:
			pr_err("transaction release %d bad object type %x\n",
				debug_id, hdr->type);
			break;
		}
	}
}

static int binder_translate_binder(struct flat_binder_object *fp,
				   struct binder_transaction *t,
				   struct binder_thread *thread)
{
	struct binder_node *node;
	struct binder_ref *ref;
	struct binder_proc *proc = thread->proc;
	struct binder_proc *target_proc = t->to_proc;

	node = binder_get_node(proc, fp->binder);
	if (!node) {
		node = binder_new_node(proc, fp->binder, fp->cookie);
		if (!node)
			return -ENOMEM;

		binder_proc_lock(node->proc, __LINE__);
		node->min_priority = fp->flags & FLAT_BINDER_FLAG_PRIORITY_MASK;
		node->accept_fds = !!(fp->flags & FLAT_BINDER_FLAG_ACCEPTS_FDS);
		binder_proc_unlock(node->proc, __LINE__);
	}
	if (fp->cookie != node->cookie) {
		binder_user_error("%d:%d sending u%016llx node %d, cookie mismatch %016llx != %016llx\n",
				  proc->pid, thread->pid, (u64)fp->binder,
				  node->debug_id, (u64)fp->cookie,
				  (u64)node->cookie);
		binder_put_node(node);
		return -EINVAL;
	}
	if (security_binder_transfer_binder(proc->tsk, target_proc->tsk)) {
		binder_put_node(node);
		return -EPERM;
	}

	ref = binder_get_ref_for_node(target_proc, node, &thread->todo);
	if (!ref) {
		binder_put_node(node);
		return -EINVAL;
	}

	if (fp->hdr.type == BINDER_TYPE_BINDER)
		fp->hdr.type = BINDER_TYPE_HANDLE;
	else
		fp->hdr.type = BINDER_TYPE_WEAK_HANDLE;
	fp->binder = 0;
	fp->handle = ref->desc;
	fp->cookie = 0;
	binder_inc_ref(ref, fp->hdr.type == BINDER_TYPE_HANDLE, &thread->todo,
		       __LINE__);

	trace_binder_transaction_node_to_ref(t, node, ref);
	binder_debug(BINDER_DEBUG_TRANSACTION,
		     "        node %d u%016llx -> ref %d desc %d\n",
		     node->debug_id, (u64)node->ptr,
		     ref->debug_id, ref->desc);
	binder_put_ref(ref);
	binder_put_node(node);
	return 0;
}

static int binder_translate_handle(struct flat_binder_object *fp,
				   struct binder_transaction *t,
				   struct binder_thread *thread)
{
	struct binder_ref *ref;
	struct binder_proc *proc = thread->proc;
	struct binder_proc *target_proc = t->to_proc;

	ref = binder_get_ref(proc, fp->handle,
			     fp->hdr.type == BINDER_TYPE_HANDLE);
	if (!ref) {
		binder_user_error("%d:%d got transaction with invalid handle, %d\n",
				  proc->pid, thread->pid, fp->handle);
		return -EINVAL;
	}
	if (security_binder_transfer_binder(proc->tsk, target_proc->tsk)) {
		binder_put_ref(ref);
		return -EPERM;
	}

	if (ref->node->proc == target_proc) {
		if (fp->hdr.type == BINDER_TYPE_HANDLE)
			fp->hdr.type = BINDER_TYPE_BINDER;
		else
			fp->hdr.type = BINDER_TYPE_WEAK_BINDER;
		fp->binder = ref->node->ptr;
		fp->cookie = ref->node->cookie;
		binder_inc_node(ref->node, fp->hdr.type == BINDER_TYPE_BINDER,
				0, NULL, __LINE__);
		trace_binder_transaction_ref_to_node(t, ref);
		binder_debug(BINDER_DEBUG_TRANSACTION,
			     "        ref %d desc %d -> node %d u%016llx\n",
			     ref->debug_id, ref->desc, ref->node->debug_id,
			     (u64)ref->node->ptr);
	} else {
		struct binder_ref *new_ref;

		new_ref = binder_get_ref_for_node(target_proc, ref->node, NULL);
		if (!new_ref) {
			binder_put_ref(ref);
			return -EINVAL;
		}

		fp->binder = 0;
		fp->handle = new_ref->desc;
		fp->cookie = 0;
		binder_inc_ref(new_ref, fp->hdr.type == BINDER_TYPE_HANDLE,
			       NULL, __LINE__);
		trace_binder_transaction_ref_to_ref(t, ref, new_ref);
		binder_debug(BINDER_DEBUG_TRANSACTION,
			     "        ref %d desc %d -> ref %d desc %d (node %d)\n",
			     ref->debug_id, ref->desc, new_ref->debug_id,
			     new_ref->desc, ref->node->debug_id);
		binder_put_ref(new_ref);
	}
	binder_put_ref(ref);
	return 0;
}

static int binder_translate_fd(int fd,
			       struct binder_transaction *t,
			       struct binder_thread *thread,
			       struct binder_transaction *in_reply_to)
{
	struct binder_proc *proc = thread->proc;
	struct binder_proc *target_proc = t->to_proc;
	int target_fd;
	struct file *file;
	int ret;
	bool target_allows_fd;

	if (in_reply_to)
		target_allows_fd = !!(in_reply_to->flags & TF_ACCEPT_FDS);
	else
		target_allows_fd = t->buffer->target_node->accept_fds;
	if (!target_allows_fd) {
		binder_user_error("%d:%d got %s with fd, %d, but target does not allow fds\n",
				  proc->pid, thread->pid,
				  in_reply_to ? "reply" : "transaction",
				  fd);
		ret = -EPERM;
		goto err_fd_not_accepted;
	}

	file = fget(fd);
	if (!file) {
		binder_user_error("%d:%d got transaction with invalid fd, %d\n",
				  proc->pid, thread->pid, fd);
		ret = -EBADF;
		goto err_fget;
	}
	ret = security_binder_transfer_file(proc->tsk, target_proc->tsk, file);
	if (ret < 0) {
		ret = -EPERM;
		goto err_security;
	}

	target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);
	if (target_fd < 0) {
		ret = -ENOMEM;
		goto err_get_unused_fd;
	}
	task_fd_install(target_proc, target_fd, file);
	trace_binder_transaction_fd(t, fd, target_fd);
	binder_debug(BINDER_DEBUG_TRANSACTION, "        fd %d -> %d\n",
		     fd, target_fd);

	return target_fd;

err_get_unused_fd:
err_security:
	fput(file);
err_fget:
err_fd_not_accepted:
	return ret;
}

static int binder_translate_fd_array(struct binder_fd_array_object *fda,
				     struct binder_buffer_object *parent,
				     struct binder_transaction *t,
				     struct binder_thread *thread,
				     struct binder_transaction *in_reply_to)
{
	binder_size_t fdi, fd_buf_size, num_installed_fds;
	int target_fd;
	uintptr_t parent_buffer;
	u32 *fd_array;
	struct binder_proc *proc = thread->proc;
	struct binder_proc *target_proc = t->to_proc;

	fd_buf_size = sizeof(u32) * fda->num_fds;
	if (fda->num_fds >= SIZE_MAX / sizeof(u32)) {
		binder_user_error("%d:%d got transaction with invalid number of fds (%lld)\n",
				  proc->pid, thread->pid, (u64)fda->num_fds);
		return -EINVAL;
	}
	if (fd_buf_size > parent->length ||
	    fda->parent_offset > parent->length - fd_buf_size) {
		/* No space for all file descriptors here. */
		binder_user_error("%d:%d not enough space to store %lld fds in buffer\n",
				  proc->pid, thread->pid, (u64)fda->num_fds);
		return -EINVAL;
	}
	/*
	 * Since the parent was already fixed up, convert it
	 * back to the kernel address space to access it
	 */
	parent_buffer = parent->buffer -
		binder_alloc_get_user_buffer_offset(&target_proc->alloc);
	fd_array = (u32 *)(parent_buffer + fda->parent_offset);
	if (!IS_ALIGNED((unsigned long)fd_array, sizeof(u32))) {
		binder_user_error("%d:%d parent offset not aligned correctly.\n",
				  proc->pid, thread->pid);
		return -EINVAL;
	}
	for (fdi = 0; fdi < fda->num_fds; fdi++) {
		target_fd = binder_translate_fd(fd_array[fdi], t, thread,
						in_reply_to);
		if (target_fd < 0)
			goto err_translate_fd_failed;
		fd_array[fdi] = target_fd;
	}
	return 0;

err_translate_fd_failed:
	/*
	 * Failed to allocate fd or security error, free fds
	 * installed so far.
	 */
	num_installed_fds = fdi;
	for (fdi = 0; fdi < num_installed_fds; fdi++)
		task_close_fd(target_proc, fd_array[fdi]);
	return target_fd;
}

static int binder_fixup_parent(struct binder_transaction *t,
			       struct binder_thread *thread,
			       struct binder_buffer_object *bp,
			       binder_size_t *off_start,
			       binder_size_t num_valid,
			       struct binder_buffer_object *last_fixup_obj,
			       binder_size_t last_fixup_min_off)
{
	struct binder_buffer_object *parent;
	u8 *parent_buffer;
	struct binder_buffer *b = t->buffer;
	struct binder_proc *proc = thread->proc;
	struct binder_proc *target_proc = t->to_proc;

	if (!(bp->flags & BINDER_BUFFER_FLAG_HAS_PARENT))
		return 0;

	parent = binder_validate_ptr(b, bp->parent, off_start, num_valid);
	if (!parent) {
		binder_user_error("%d:%d got transaction with invalid parent offset or type\n",
				  proc->pid, thread->pid);
		return -EINVAL;
	}

	if (!binder_validate_fixup(b, off_start,
				   parent, bp->parent_offset,
				   last_fixup_obj,
				   last_fixup_min_off)) {
		binder_user_error("%d:%d got transaction with out-of-order buffer fixup\n",
				  proc->pid, thread->pid);
		return -EINVAL;
	}

	if (parent->length < sizeof(binder_uintptr_t) ||
	    bp->parent_offset > parent->length - sizeof(binder_uintptr_t)) {
		/* No space for a pointer here! */
		binder_user_error("%d:%d got transaction with invalid parent offset\n",
				  proc->pid, thread->pid);
		return -EINVAL;
	}
	parent_buffer = (u8 *)(parent->buffer -
			binder_alloc_get_user_buffer_offset(
				&target_proc->alloc));
	*(binder_uintptr_t *)(parent_buffer + bp->parent_offset) = bp->buffer;

	return 0;
}

static void binder_transaction(struct binder_proc *proc,
			       struct binder_thread *thread,
			       struct binder_transaction_data *tr, int reply,
			       binder_size_t extra_buffers_size)
{
	int ret;
	struct binder_transaction *t;
	struct binder_work *tcomplete;
	binder_size_t *offp, *off_end, *off_start;
	binder_size_t off_min;
	u8 *sg_bufp, *sg_buf_end;
	struct binder_proc *target_proc;
	struct binder_thread *target_thread = NULL;
	struct binder_node *target_node = NULL;
	struct binder_ref *target_ref = NULL;
	struct binder_worklist *target_list;
	wait_queue_head_t *target_wait;
	struct binder_transaction *in_reply_to = NULL;
	struct binder_transaction_log_entry *e;
	uint32_t return_error = 0;
	uint32_t return_error_param = 0;
	uint32_t return_error_line = 0;
	struct binder_buffer_object *last_fixup_obj = NULL;
	binder_size_t last_fixup_min_off = 0;
	struct binder_context *context = proc->context;
	bool oneway;
	struct binder_priority saved_priority;

	e = binder_transaction_log_add(&binder_transaction_log);
	e->call_type = reply ? 2 : !!(tr->flags & TF_ONE_WAY);
	e->from_proc = proc->pid;
	e->from_thread = thread->pid;
	e->target_handle = tr->target.handle;
	e->data_size = tr->data_size;
	e->offsets_size = tr->offsets_size;
	e->context_name = proc->context->name;

	if (reply) {
		binder_proc_lock(thread->proc, __LINE__);
		in_reply_to = thread->transaction_stack;
		if (in_reply_to == NULL) {
			binder_proc_unlock(thread->proc, __LINE__);
			binder_user_error("%d:%d got reply transaction with no transaction stack\n",
					  proc->pid, thread->pid);
			return_error = BR_FAILED_REPLY;
			return_error_line = __LINE__;
			goto err_empty_call_stack;
		}
		if (in_reply_to->to_thread != thread) {
			binder_user_error("%d:%d got reply transaction with bad transaction stack, transaction %d has target %d:%d\n",
				proc->pid, thread->pid, in_reply_to->debug_id,
				in_reply_to->to_proc ?
				in_reply_to->to_proc->pid : 0,
				in_reply_to->to_thread ?
				in_reply_to->to_thread->pid : 0);
			binder_proc_unlock(thread->proc, __LINE__);
			return_error = BR_FAILED_REPLY;
			return_error_line = __LINE__;
			in_reply_to = NULL;
			goto err_bad_call_stack;
		}
		thread->transaction_stack = in_reply_to->to_parent;
		binder_proc_unlock(thread->proc, __LINE__);
		saved_priority = in_reply_to->saved_priority;
		target_thread = in_reply_to->from;
		if (target_thread == NULL) {
			return_error = BR_DEAD_REPLY;
			return_error_line = __LINE__;
			goto err_dead_binder;
		}
		binder_proc_lock(target_thread->proc, __LINE__);
		if (target_thread->transaction_stack != in_reply_to) {
			binder_user_error("%d:%d got reply transaction with bad target transaction stack %d, expected %d\n",
				proc->pid, thread->pid,
				target_thread->transaction_stack ?
				target_thread->transaction_stack->debug_id : 0,
				in_reply_to->debug_id);
			binder_proc_unlock(target_thread->proc, __LINE__);
			return_error = BR_FAILED_REPLY;
			return_error_line = __LINE__;
			in_reply_to = NULL;
			target_thread = NULL;
			goto err_dead_binder;
		}
		binder_proc_unlock(target_thread->proc, __LINE__);
		target_proc = target_thread->proc;
	} else {
		if (tr->target.handle) {
			target_ref = binder_get_ref(proc, tr->target.handle,
						    true);
			if (target_ref == NULL) {
				binder_user_error("%d:%d got transaction to invalid handle\n",
					proc->pid, thread->pid);
				return_error = BR_FAILED_REPLY;
				return_error_line = __LINE__;
				goto err_invalid_target_handle;
			}
			target_node = target_ref->node;
		} else {
			target_node = context->binder_context_mgr_node;
			if (target_node == NULL) {
				return_error = BR_DEAD_REPLY;
				return_error_line = __LINE__;
				goto err_no_context_mgr_node;
			}
		}
		e->to_node = target_node->debug_id;
		target_proc = target_node->proc;
		if (target_node->is_zombie) {
			return_error = BR_DEAD_REPLY;
			return_error_line = __LINE__;
			goto err_dead_binder;
		}
		if (security_binder_transaction(proc->tsk, target_proc->tsk) < 0) {
			return_error = BR_FAILED_REPLY;
			return_error_line = __LINE__;
			goto err_invalid_target_handle;
		}
		binder_proc_lock(thread->proc, __LINE__);
		if (!(tr->flags & TF_ONE_WAY) && thread->transaction_stack) {
			struct binder_transaction *tmp;

			tmp = thread->transaction_stack;
			if (tmp->to_thread != thread) {
				binder_user_error("%d:%d got new transaction with bad transaction stack, transaction %d has target %d:%d\n",
					proc->pid, thread->pid, tmp->debug_id,
					tmp->to_proc ? tmp->to_proc->pid : 0,
					tmp->to_thread ?
					tmp->to_thread->pid : 0);
				binder_proc_unlock(thread->proc, __LINE__);
				return_error = BR_FAILED_REPLY;
				return_error_line = __LINE__;
				goto err_bad_call_stack;
			}
			while (tmp) {
				if (tmp->from && tmp->from->proc == target_proc)
					target_thread = tmp->from;
				tmp = tmp->from_parent;
			}
		}
		binder_proc_unlock(thread->proc, __LINE__);
	}
	if (target_thread) {
		e->to_thread = target_thread->pid;
		target_list = &target_thread->todo;
		target_wait = &target_thread->wait;
	} else {
		target_list = &target_proc->todo;
		target_wait = NULL;
	}
	e->to_proc = target_proc->pid;

	/* TODO: reuse incoming transaction for reply */
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL) {
		return_error = BR_FAILED_REPLY;
		return_error_line = __LINE__;
		goto err_alloc_t_failed;
	}
	binder_stats_created(BINDER_STAT_TRANSACTION);

	tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);
	if (tcomplete == NULL) {
		return_error = BR_FAILED_REPLY;
		return_error_line = __LINE__;
		goto err_alloc_tcomplete_failed;
	}
	binder_stats_created(BINDER_STAT_TRANSACTION_COMPLETE);

	t->debug_id = ++binder_last_id;
	e->debug_id = t->debug_id;

	if (reply)
		binder_debug(BINDER_DEBUG_TRANSACTION,
			     "%d:%d BC_REPLY %d -> %d:%d, data %016llx-%016llx size %lld-%lld-%lld\n",
			     proc->pid, thread->pid, t->debug_id,
			     target_proc->pid, target_thread->pid,
			     (u64)tr->data.ptr.buffer,
			     (u64)tr->data.ptr.offsets,
			     (u64)tr->data_size, (u64)tr->offsets_size,
			     (u64)extra_buffers_size);
	else
		binder_debug(BINDER_DEBUG_TRANSACTION,
			     "%d:%d BC_TRANSACTION %d -> %d - node %d, data %016llx-%016llx size %lld-%lld-%lld\n",
			     proc->pid, thread->pid, t->debug_id,
			     target_proc->pid, target_node->debug_id,
			     (u64)tr->data.ptr.buffer,
			     (u64)tr->data.ptr.offsets,
			     (u64)tr->data_size, (u64)tr->offsets_size,
			     (u64)extra_buffers_size);

	if (!reply && !(tr->flags & TF_ONE_WAY))
		t->from = thread;
	else
		t->from = NULL;
	t->sender_euid = task_euid(proc->tsk);
	t->to_proc = target_proc;
	t->to_thread = target_thread;
	t->code = tr->code;
	t->flags = tr->flags;
	t->priority.sched_policy = current->policy;
	t->priority.nice = task_nice(current);
	t->priority.rt_priority = current->rt_priority;

	trace_binder_transaction(reply, t, target_node);

	t->buffer = binder_alloc_new_buf(&target_proc->alloc, tr->data_size,
		tr->offsets_size, extra_buffers_size,
		!reply && (t->flags & TF_ONE_WAY));
	if (IS_ERR(t->buffer)) {
		return_error = !(target_proc->tsk->flags & PF_EXITING) ?
					BR_FAILED_REPLY : BR_DEAD_REPLY;
		return_error_param = PTR_ERR(t->buffer);
		return_error_line = __LINE__;
		t->buffer = NULL;
		goto err_binder_alloc_buf_failed;
	}
	t->buffer->allow_user_free = 0;
	t->buffer->debug_id = t->debug_id;
	t->buffer->transaction = t;
	t->buffer->target_node = target_node;
	trace_binder_transaction_alloc_buf(t->buffer);
	if (target_node) {
		binder_inc_node(target_node, 1, 0, NULL, __LINE__);
		if (target_ref)
			binder_put_ref(target_ref);
		target_ref = NULL;
	}

	off_start = (binder_size_t *)(t->buffer->data +
				      ALIGN(tr->data_size, sizeof(void *)));
	offp = off_start;

	if (copy_from_user(t->buffer->data, (const void __user *)(uintptr_t)
			   tr->data.ptr.buffer, tr->data_size)) {
		binder_user_error("%d:%d got transaction with invalid data ptr\n",
				proc->pid, thread->pid);
		return_error = BR_FAILED_REPLY;
		return_error_line = __LINE__;
		goto err_copy_data_failed;
	}
	if (copy_from_user(offp, (const void __user *)(uintptr_t)
			   tr->data.ptr.offsets, tr->offsets_size)) {
		binder_user_error("%d:%d got transaction with invalid offsets ptr\n",
				proc->pid, thread->pid);
		return_error = BR_FAILED_REPLY;
		return_error_line = __LINE__;
		goto err_copy_data_failed;
	}
	if (!IS_ALIGNED(tr->offsets_size, sizeof(binder_size_t))) {
		binder_user_error("%d:%d got transaction with invalid offsets size, %lld\n",
				proc->pid, thread->pid, (u64)tr->offsets_size);
		return_error = BR_FAILED_REPLY;
		return_error_line = __LINE__;
		goto err_bad_offset;
	}
	if (!IS_ALIGNED(extra_buffers_size, sizeof(u64))) {
		binder_user_error("%d:%d got transaction with unaligned buffers size, %lld\n",
				  proc->pid, thread->pid,
				  extra_buffers_size);
		return_error = BR_FAILED_REPLY;
		return_error_line = __LINE__;
		goto err_bad_offset;
	}
	off_end = (void *)off_start + tr->offsets_size;
	sg_bufp = (u8 *)(PTR_ALIGN(off_end, sizeof(void *)));
	sg_buf_end = sg_bufp + extra_buffers_size;
	off_min = 0;
	for (; offp < off_end; offp++) {
		struct binder_object_header *hdr;
		size_t object_size = binder_validate_object(t->buffer, *offp);

		if (object_size == 0 || *offp < off_min) {
			binder_user_error("%d:%d got transaction with invalid offset (%lld, min %lld max %lld) or object.\n",
					  proc->pid, thread->pid, (u64)*offp,
					  (u64)off_min,
					  (u64)t->buffer->data_size);
			return_error = BR_FAILED_REPLY;
			return_error_line = __LINE__;
			goto err_bad_offset;
		}

		hdr = (struct binder_object_header *)(t->buffer->data + *offp);
		off_min = *offp + object_size;
		switch (hdr->type) {
		case BINDER_TYPE_BINDER:
		case BINDER_TYPE_WEAK_BINDER: {
			struct flat_binder_object *fp;

			fp = to_flat_binder_object(hdr);
			ret = binder_translate_binder(fp, t, thread);
			if (ret < 0) {
				return_error = BR_FAILED_REPLY;
				return_error_param = ret;
				return_error_line = __LINE__;
				goto err_translate_failed;
			}
		} break;
		case BINDER_TYPE_HANDLE:
		case BINDER_TYPE_WEAK_HANDLE: {
			struct flat_binder_object *fp;

			fp = to_flat_binder_object(hdr);
			ret = binder_translate_handle(fp, t, thread);
			if (ret < 0) {
				return_error = BR_FAILED_REPLY;
				return_error_param = ret;
				return_error_line = __LINE__;
				goto err_translate_failed;
			}
		} break;

		case BINDER_TYPE_FD: {
			struct binder_fd_object *fp = to_binder_fd_object(hdr);
			int target_fd = binder_translate_fd(fp->fd, t, thread,
							    in_reply_to);

			if (target_fd < 0) {
				return_error = BR_FAILED_REPLY;
				return_error_param = target_fd;
				return_error_line = __LINE__;
				goto err_translate_failed;
			}
			fp->pad_binder = 0;
			fp->fd = target_fd;
		} break;
		case BINDER_TYPE_FDA: {
			struct binder_fd_array_object *fda =
				to_binder_fd_array_object(hdr);
			struct binder_buffer_object *parent =
				binder_validate_ptr(t->buffer, fda->parent,
						    off_start,
						    offp - off_start);
			if (!parent) {
				binder_user_error("%d:%d got transaction with invalid parent offset or type\n",
						  proc->pid, thread->pid);
				return_error = BR_FAILED_REPLY;
				return_error_line = __LINE__;
				goto err_bad_parent;
			}
			if (!binder_validate_fixup(t->buffer, off_start,
						   parent, fda->parent_offset,
						   last_fixup_obj,
						   last_fixup_min_off)) {
				binder_user_error("%d:%d got transaction with out-of-order buffer fixup\n",
						  proc->pid, thread->pid);
				return_error = BR_FAILED_REPLY;
				return_error_line = __LINE__;
				goto err_bad_parent;
			}
			ret = binder_translate_fd_array(fda, parent, t, thread,
							in_reply_to);
			if (ret < 0) {
				return_error = BR_FAILED_REPLY;
				return_error_param = ret;
				return_error_line = __LINE__;
				goto err_translate_failed;
			}
			last_fixup_obj = parent;
			last_fixup_min_off =
				fda->parent_offset + sizeof(u32) * fda->num_fds;
		} break;
		case BINDER_TYPE_PTR: {
			struct binder_buffer_object *bp =
				to_binder_buffer_object(hdr);
			size_t buf_left = sg_buf_end - sg_bufp;

			if (bp->length > buf_left) {
				binder_user_error("%d:%d got transaction with too large buffer\n",
						  proc->pid, thread->pid);
				return_error = BR_FAILED_REPLY;
				return_error_line = __LINE__;
				goto err_bad_offset;
			}
			if (copy_from_user(sg_bufp,
					   (const void __user *)(uintptr_t)
					    bp->buffer, bp->length)) {
				binder_user_error("%d:%d got transaction with invalid offsets ptr\n",
						  proc->pid, thread->pid);
				return_error = BR_FAILED_REPLY;
				return_error_line = __LINE__;
				goto err_copy_data_failed;
			}
			/* Fixup buffer pointer to target proc address space */
			bp->buffer = (uintptr_t)sg_bufp +
				binder_alloc_get_user_buffer_offset(
						&target_proc->alloc);
			sg_bufp += ALIGN(bp->length, sizeof(u64));

			ret = binder_fixup_parent(t, thread, bp, off_start,
						  offp - off_start,
						  last_fixup_obj,
						  last_fixup_min_off);
			if (ret < 0) {
				return_error = BR_FAILED_REPLY;
				return_error_param = ret;
				return_error_line = __LINE__;
				goto err_translate_failed;
			}
			last_fixup_obj = bp;
			last_fixup_min_off = 0;
		} break;
		default:
			binder_user_error("%d:%d got transaction with invalid object type, %x\n",
				proc->pid, thread->pid, hdr->type);
			return_error = BR_FAILED_REPLY;
			return_error_param = hdr->type;
			return_error_line = __LINE__;
			goto err_bad_object_type;
		}
	}

	BUG_ON(!target_list);
	t->work.type = BINDER_WORK_TRANSACTION;
	tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
	binder_enqueue_work(tcomplete, &thread->todo, __LINE__);
	oneway = !!(t->flags & TF_ONE_WAY);

	if (reply) {
		BUG_ON(t->buffer->async_transaction != 0);
		binder_proc_lock(target_thread->proc, __LINE__);
		binder_pop_transaction(target_thread, in_reply_to);
		binder_enqueue_work(&t->work, target_list, __LINE__);
		binder_proc_unlock(target_thread->proc, __LINE__);
		binder_free_transaction(in_reply_to);
		wake_up_interruptible_sync(target_wait);
		binder_restore_priority(&saved_priority);
	} else if (!(t->flags & TF_ONE_WAY)) {
		BUG_ON(t->buffer->async_transaction != 0);
		binder_proc_lock(thread->proc, __LINE__);
		t->need_reply = 1;
		t->from_parent = thread->transaction_stack;
		thread->transaction_stack = t;
		binder_proc_unlock(thread->proc, __LINE__);
		binder_proc_lock(target_proc, __LINE__);
		if (!target_thread) {
			/* See if we can find a thread to take this */
			target_thread = binder_select_thread(target_proc);
			if (target_thread) {
				target_wait = &target_thread->wait;
				target_list = &target_thread->todo;
				binder_set_priority(target_thread->task, t,
						    target_node);
			}
		}
		binder_enqueue_work(&t->work, target_list, __LINE__);
		if (target_wait)
			wake_up_interruptible_sync(target_wait);
		else
			binder_wakeup_poll_threads(target_proc,
						   true /* sync */);
		binder_proc_unlock(target_proc, __LINE__);
	} else {
		BUG_ON(target_node == NULL);
		BUG_ON(t->buffer->async_transaction != 1);

		binder_proc_lock(target_node->proc, __LINE__);
		if (target_node->has_async_transaction) {
			target_list = &target_node->async_todo;
		} else {
			target_node->has_async_transaction = 1;
			binder_wakeup_thread(target_proc, false /*sync */);
		}
		/*
		 * Test/set of has_async_transaction
		 * must be atomic with enqueue on
		 * async_todo
		 */
		binder_enqueue_work(&t->work, target_list, __LINE__);
		binder_proc_unlock(target_node->proc, __LINE__);
	}
	return;

err_translate_failed:
err_bad_object_type:
err_bad_offset:
err_bad_parent:
err_copy_data_failed:
	trace_binder_transaction_failed_buffer_release(t->buffer);
	binder_transaction_buffer_release(target_proc, t->buffer, offp);
	t->buffer->transaction = NULL;
	binder_alloc_free_buf(&target_proc->alloc, t->buffer);
err_binder_alloc_buf_failed:
	kfree(tcomplete);
	binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
err_alloc_tcomplete_failed:
	kfree(t);
	binder_stats_deleted(BINDER_STAT_TRANSACTION);
err_alloc_t_failed:
err_bad_call_stack:
err_empty_call_stack:
err_dead_binder:
err_invalid_target_handle:
err_no_context_mgr_node:
	if (target_ref)
		binder_put_ref(target_ref);

	binder_debug(BINDER_DEBUG_FAILED_TRANSACTION,
		     "%d:%d transaction failed %d/%d, size %lld-%lld line %d\n",
		     proc->pid, thread->pid, return_error, return_error_param,
		     (u64)tr->data_size, (u64)tr->offsets_size,
		     return_error_line);

	{
		struct binder_transaction_log_entry *fe;

		e->return_error = return_error;
		e->return_error_param = return_error_param;
		e->return_error_line = return_error_line;
		fe = binder_transaction_log_add(&binder_transaction_log_failed);
		*fe = *e;
	}

	binder_proc_lock(thread->proc, __LINE__);
	BUG_ON(thread->return_error != BR_OK);
	if (in_reply_to) {
		thread->return_error = BR_TRANSACTION_COMPLETE;
		binder_proc_unlock(thread->proc, __LINE__);
		binder_restore_priority(&saved_priority);
		binder_send_failed_reply(in_reply_to, return_error);
	} else {
		thread->return_error = return_error;
		binder_proc_unlock(thread->proc, __LINE__);
	}
}

static int binder_thread_write(struct binder_proc *proc,
			struct binder_thread *thread,
			binder_uintptr_t binder_buffer, size_t size,
			binder_size_t *consumed)
{
	uint32_t cmd;
	struct binder_context *context = proc->context;
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	while (ptr < end && thread->return_error == BR_OK) {
		if (get_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		trace_binder_command(cmd);
		if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.bc)) {
			atomic_inc(&binder_stats.bc[_IOC_NR(cmd)]);
			atomic_inc(&proc->stats.bc[_IOC_NR(cmd)]);
			atomic_inc(&thread->stats.bc[_IOC_NR(cmd)]);
		}
		switch (cmd) {
		case BC_INCREFS:
		case BC_ACQUIRE:
		case BC_RELEASE:
		case BC_DECREFS: {
			uint32_t target;
			struct binder_ref *ref;
			const char *debug_string;
			struct binder_node *ctx_mgr_node =
				context->binder_context_mgr_node;

			if (get_user(target, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			if (target == 0 && ctx_mgr_node &&
			    (cmd == BC_INCREFS || cmd == BC_ACQUIRE)) {
				ref = binder_get_ref_for_node(proc,
							      ctx_mgr_node,
							      NULL);
				if (ref && ref->desc != target) {
					binder_user_error("%d:%d tried to acquire reference to desc 0, got %d instead\n",
						proc->pid, thread->pid,
						ref->desc);
				}
			} else
				ref = binder_get_ref(proc, target,
						     cmd == BC_ACQUIRE ||
						     cmd == BC_RELEASE);
			if (ref == NULL) {
				binder_user_error("%d:%d refcount change on invalid ref %d\n",
					proc->pid, thread->pid, target);
				break;
			}
			switch (cmd) {
			case BC_INCREFS:
				debug_string = "IncRefs";
				binder_inc_ref(ref, 0, NULL, __LINE__);
				break;
			case BC_ACQUIRE:
				debug_string = "Acquire";
				binder_inc_ref(ref, 1, NULL, __LINE__);
				break;
			case BC_RELEASE:
				debug_string = "Release";
				binder_dec_ref(ref, 1, __LINE__);
				break;
			case BC_DECREFS:
			default:
				debug_string = "DecRefs";
				binder_dec_ref(ref, 0, __LINE__);
				break;
			}
			binder_debug(BINDER_DEBUG_USER_REFS,
				     "%d:%d %s ref %d desc %d s %d w %d for node %d\n",
				     proc->pid, thread->pid, debug_string,
				     ref->debug_id, ref->desc,
				     atomic_read(&ref->strong),
				     atomic_read(&ref->weak),
				     ref->node->debug_id);
			binder_put_ref(ref);
			break;
		}
		case BC_INCREFS_DONE:
		case BC_ACQUIRE_DONE: {
			binder_uintptr_t node_ptr;
			binder_uintptr_t cookie;
			struct binder_node *node;

			if (get_user(node_ptr, (binder_uintptr_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(binder_uintptr_t);
			if (get_user(cookie, (binder_uintptr_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(binder_uintptr_t);
			node = binder_get_node(proc, node_ptr);
			if (node == NULL) {
				binder_user_error("%d:%d %s u%016llx no match\n",
					proc->pid, thread->pid,
					cmd == BC_INCREFS_DONE ?
					"BC_INCREFS_DONE" :
					"BC_ACQUIRE_DONE",
					(u64)node_ptr);
				break;
			}
			if (cookie != node->cookie) {
				binder_user_error("%d:%d %s u%016llx node %d cookie mismatch %016llx != %016llx\n",
					proc->pid, thread->pid,
					cmd == BC_INCREFS_DONE ?
					"BC_INCREFS_DONE" : "BC_ACQUIRE_DONE",
					(u64)node_ptr, node->debug_id,
					(u64)cookie, (u64)node->cookie);
				binder_put_node(node);
				break;
			}
			binder_proc_lock(node->proc, __LINE__);
			if (cmd == BC_ACQUIRE_DONE) {
				if (node->pending_strong_ref == 0) {
					binder_user_error("%d:%d BC_ACQUIRE_DONE node %d has no pending acquire request\n",
						proc->pid, thread->pid,
						node->debug_id);
					binder_proc_unlock(node->proc, __LINE__);
					binder_put_node(node);
					break;
				}
				node->pending_strong_ref = 0;
			} else {
				if (node->pending_weak_ref == 0) {
					binder_user_error("%d:%d BC_INCREFS_DONE node %d has no pending increfs request\n",
						proc->pid, thread->pid,
						node->debug_id);
					binder_proc_unlock(node->proc, __LINE__);
					binder_put_node(node);
					break;
				}
				node->pending_weak_ref = 0;
			}
			binder_proc_unlock(node->proc, __LINE__);
			binder_dec_node(node, cmd == BC_ACQUIRE_DONE, 0,
					__LINE__);
			binder_debug(BINDER_DEBUG_USER_REFS,
				     "%d:%d %s node %d ls %d lw %d\n",
				     proc->pid, thread->pid,
				     cmd == BC_INCREFS_DONE ? "BC_INCREFS_DONE" : "BC_ACQUIRE_DONE",
				     node->debug_id, node->local_strong_refs, node->local_weak_refs);
			binder_put_node(node);
			break;
		}
		case BC_ATTEMPT_ACQUIRE:
			pr_err("BC_ATTEMPT_ACQUIRE not supported\n");
			return -EINVAL;
		case BC_ACQUIRE_RESULT:
			pr_err("BC_ACQUIRE_RESULT not supported\n");
			return -EINVAL;

		case BC_FREE_BUFFER: {
			binder_uintptr_t data_ptr;
			struct binder_buffer *buffer;

			if (get_user(data_ptr, (binder_uintptr_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(binder_uintptr_t);

			buffer = binder_alloc_buffer_lookup(&proc->alloc,
							    data_ptr);
			if (buffer == NULL) {
				binder_user_error("%d:%d BC_FREE_BUFFER u%016llx no match\n",
					proc->pid, thread->pid, (u64)data_ptr);
				break;
			}
			if (!buffer->allow_user_free) {
				binder_user_error("%d:%d BC_FREE_BUFFER u%016llx matched unreturned buffer\n",
					proc->pid, thread->pid, (u64)data_ptr);
				break;
			}
			binder_debug(BINDER_DEBUG_FREE_BUFFER,
				     "%d:%d BC_FREE_BUFFER u%016llx found buffer %d for %s transaction\n",
				     proc->pid, thread->pid, (u64)data_ptr,
				     buffer->debug_id,
				     buffer->transaction ? "active" : "finished");

			if (buffer->transaction) {
				buffer->transaction->buffer = NULL;
				buffer->transaction = NULL;
			}
			if (buffer->async_transaction && buffer->target_node) {
				struct binder_node *buf_node;

				buf_node = buffer->target_node;
				binder_proc_lock(buf_node->proc, __LINE__);
				BUG_ON(!buf_node->has_async_transaction);
				if (binder_worklist_empty(
							&buf_node->async_todo))
					buf_node->has_async_transaction = 0;
				else {
					struct binder_work *w;

					w = container_of(
						buf_node->async_todo.list.next,
						struct binder_work, entry);
					binder_dequeue_work(w, __LINE__);
					binder_enqueue_work(w, &proc->todo,
							__LINE__);
				}
				binder_proc_unlock(buffer->target_node->proc,
						   __LINE__);
			}
			trace_binder_transaction_buffer_release(buffer);
			binder_transaction_buffer_release(proc, buffer, NULL);
			binder_alloc_free_buf(&proc->alloc, buffer);
			break;
		}

		case BC_TRANSACTION_SG:
		case BC_REPLY_SG: {
			struct binder_transaction_data_sg tr;

			if (copy_from_user(&tr, ptr, sizeof(tr)))
				return -EFAULT;
			ptr += sizeof(tr);
			binder_transaction(proc, thread, &tr.transaction_data,
					   cmd == BC_REPLY_SG, tr.buffers_size);
			break;
		}
		case BC_TRANSACTION:
		case BC_REPLY: {
			struct binder_transaction_data tr;

			if (copy_from_user(&tr, ptr, sizeof(tr)))
				return -EFAULT;
			ptr += sizeof(tr);
			binder_transaction(proc, thread, &tr,
					   cmd == BC_REPLY, 0);
			break;
		}

		case BC_REGISTER_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "%d:%d BC_REGISTER_LOOPER\n",
				     proc->pid, thread->pid);
			binder_proc_lock(proc, __LINE__);
			if (thread->looper & BINDER_LOOPER_STATE_ENTERED) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_REGISTER_LOOPER called after BC_ENTER_LOOPER\n",
					proc->pid, thread->pid);
			} else if (proc->requested_threads == 0) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_REGISTER_LOOPER called without request\n",
					proc->pid, thread->pid);
			} else {
				proc->requested_threads--;
				proc->requested_threads_started++;
			}
			thread->looper |= BINDER_LOOPER_STATE_REGISTERED;
			binder_proc_unlock(proc, __LINE__);
			break;
		case BC_ENTER_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "%d:%d BC_ENTER_LOOPER\n",
				     proc->pid, thread->pid);
			if (thread->looper & BINDER_LOOPER_STATE_REGISTERED) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_ENTER_LOOPER called after BC_REGISTER_LOOPER\n",
					proc->pid, thread->pid);
			}
			thread->looper |= BINDER_LOOPER_STATE_ENTERED;
			break;
		case BC_EXIT_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "%d:%d BC_EXIT_LOOPER\n",
				     proc->pid, thread->pid);
			thread->looper |= BINDER_LOOPER_STATE_EXITED;
			break;

		case BC_REQUEST_DEATH_NOTIFICATION:
		case BC_CLEAR_DEATH_NOTIFICATION: {
			uint32_t target;
			binder_uintptr_t cookie;
			struct binder_ref *ref;
			struct binder_ref_death *death;

			if (get_user(target, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			if (get_user(cookie, (binder_uintptr_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(binder_uintptr_t);
			ref = binder_get_ref(proc, target, false);
			if (ref == NULL) {
				binder_user_error("%d:%d %s invalid ref %d\n",
					proc->pid, thread->pid,
					cmd == BC_REQUEST_DEATH_NOTIFICATION ?
					"BC_REQUEST_DEATH_NOTIFICATION" :
					"BC_CLEAR_DEATH_NOTIFICATION",
					target);
				break;
			}

			binder_debug(BINDER_DEBUG_DEATH_NOTIFICATION,
				     "%d:%d %s %016llx ref %d desc %d s %d w %d for node %d\n",
				     proc->pid, thread->pid,
				     cmd == BC_REQUEST_DEATH_NOTIFICATION ?
				     "BC_REQUEST_DEATH_NOTIFICATION" :
				     "BC_CLEAR_DEATH_NOTIFICATION",
				     (u64)cookie, ref->debug_id, ref->desc,
				     atomic_read(&ref->strong),
				     atomic_read(&ref->weak),
				     ref->node->debug_id);

			if (cmd == BC_REQUEST_DEATH_NOTIFICATION) {
				if (ref->death) {
					binder_user_error("%d:%d BC_REQUEST_DEATH_NOTIFICATION death notification already set\n",
						proc->pid, thread->pid);
					binder_put_ref(ref);
					break;
				}
				death = kzalloc(sizeof(*death), GFP_KERNEL);
				if (death == NULL) {
					binder_proc_lock(thread->proc,
							 __LINE__);
					WARN_ON(thread->return_error != BR_OK);
					thread->return_error = BR_ERROR;
					binder_debug(BINDER_DEBUG_FAILED_TRANSACTION,
						     "%d:%d BC_REQUEST_DEATH_NOTIFICATION failed\n",
						     proc->pid, thread->pid);
					binder_proc_unlock(thread->proc,
							   __LINE__);
					binder_put_ref(ref);
					break;
				}
				binder_stats_created(BINDER_STAT_DEATH);
				INIT_LIST_HEAD(&death->work.entry);
				death->cookie = cookie;
				binder_proc_lock(proc, __LINE__);
				ref->death = death;
				if (ref->node->is_zombie &&
				    list_empty(&ref->death->work.entry)) {
					ref->death->work.type = BINDER_WORK_DEAD_BINDER;
					if (thread->looper &
					    (BINDER_LOOPER_STATE_REGISTERED |
					     BINDER_LOOPER_STATE_ENTERED))
						binder_enqueue_work(
							&ref->death->work,
							&thread->todo,
							__LINE__);
					else {
						binder_enqueue_work(
							&ref->death->work,
							&proc->todo,
							__LINE__);
						binder_wakeup_thread(proc,
								     false);
					}
				}
				binder_proc_unlock(proc, __LINE__);
			} else {
				if (ref->death == NULL) {
					binder_user_error("%d:%d BC_CLEAR_DEATH_NOTIFICATION death notification not active\n",
						proc->pid, thread->pid);
					binder_put_ref(ref);
					break;
				}
				death = ref->death;
				if (death->cookie != cookie) {
					binder_user_error("%d:%d BC_CLEAR_DEATH_NOTIFICATION death notification cookie mismatch %016llx != %016llx\n",
						proc->pid, thread->pid,
						(u64)death->cookie,
						(u64)cookie);
					binder_put_ref(ref);
					break;
				}
				binder_proc_lock(proc, __LINE__);
				ref->death = NULL;
				if (list_empty(&death->work.entry)) {
					death->work.type = BINDER_WORK_CLEAR_DEATH_NOTIFICATION;
					if (thread->looper &
					    (BINDER_LOOPER_STATE_REGISTERED |
					     BINDER_LOOPER_STATE_ENTERED))
						binder_enqueue_work(
								&death->work,
								&thread->todo,
								__LINE__);
					else {
						binder_enqueue_work(
								&death->work,
								&proc->todo,
								__LINE__);
						binder_wakeup_thread(proc,
								     false);
					}
				} else {
					BUG_ON(death->work.type != BINDER_WORK_DEAD_BINDER);
					death->work.type = BINDER_WORK_DEAD_BINDER_AND_CLEAR;
				}
				binder_proc_unlock(proc, __LINE__);
			}
			binder_put_ref(ref);
		} break;
		case BC_DEAD_BINDER_DONE: {
			struct binder_work *w;
			binder_uintptr_t cookie;
			struct binder_ref_death *death = NULL;

			if (get_user(cookie, (binder_uintptr_t __user *)ptr))
				return -EFAULT;

			ptr += sizeof(void *);
			spin_lock(&proc->delivered_death.lock);
			list_for_each_entry(w, &proc->delivered_death.list,
					    entry) {
				struct binder_ref_death *tmp_death =
					container_of(w,
						     struct binder_ref_death,
						     work);

				if (tmp_death->cookie == cookie) {
					death = tmp_death;
					break;
				}
			}
			spin_unlock(&proc->delivered_death.lock);
			binder_debug(BINDER_DEBUG_DEAD_BINDER,
				     "%d:%d BC_DEAD_BINDER_DONE %016llx found %pK\n",
				     proc->pid, thread->pid, (u64)cookie,
				     death);
			if (death == NULL) {
				binder_user_error("%d:%d BC_DEAD_BINDER_DONE %016llx not found\n",
					proc->pid, thread->pid, (u64)cookie);
				break;
			}
			binder_proc_lock(proc, __LINE__);
			binder_dequeue_work(&death->work, __LINE__);
			if (death->work.type == BINDER_WORK_DEAD_BINDER_AND_CLEAR) {
				death->work.type = BINDER_WORK_CLEAR_DEATH_NOTIFICATION;
				if (thread->looper &
					(BINDER_LOOPER_STATE_REGISTERED |
					 BINDER_LOOPER_STATE_ENTERED))
					binder_enqueue_work(&death->work,
							    &thread->todo,
							    __LINE__);
				else {
					binder_enqueue_work(&death->work,
							    &proc->todo,
							    __LINE__);
					binder_wakeup_thread(proc, false);
				}
			}
			binder_proc_unlock(proc, __LINE__);
		} break;

		default:
			pr_err("%d:%d unknown command %d\n",
			       proc->pid, thread->pid, cmd);
			return -EINVAL;
		}
		*consumed = ptr - buffer;
	}
	return 0;
}

static void binder_stat_br(struct binder_proc *proc,
			   struct binder_thread *thread, uint32_t cmd)
{
	trace_binder_return(cmd);
	if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.br)) {
		atomic_inc(&binder_stats.br[_IOC_NR(cmd)]);
		atomic_inc(&proc->stats.br[_IOC_NR(cmd)]);
		atomic_inc(&thread->stats.br[_IOC_NR(cmd)]);
	}
}

static int binder_wait_for_work(struct binder_thread *thread,
				bool do_proc_work)
{
	DEFINE_WAIT(wait);
	struct binder_proc *proc = thread->proc;
	struct binder_thread *get_thread;
	int ret = 0;

	binder_put_thread(thread);
	freezer_do_not_count();
	binder_proc_lock(proc, __LINE__);
	for (;;) {
		prepare_to_wait(&thread->wait, &wait, TASK_INTERRUPTIBLE);
		if (binder_has_work(thread, do_proc_work))
			break;
		if (do_proc_work)
			list_add(&thread->waiting_thread_node,
				 &proc->waiting_threads);
		binder_proc_unlock(proc, __LINE__);
		schedule();
		binder_proc_lock(proc, __LINE__);
		list_del_init(&thread->waiting_thread_node);
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
	}
	finish_wait(&thread->wait, &wait);
	binder_proc_unlock(proc, __LINE__);
	freezer_count();
	get_thread = binder_get_thread(proc);
	BUG_ON(get_thread != thread);

	return ret;
}

static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread **threadp,
			      binder_uintptr_t binder_buffer, size_t size,
			      binder_size_t *consumed, int non_block)
{
	struct binder_thread *thread = *threadp;
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;
	struct binder_worklist *wlist = NULL;
	int ret = 0;
	bool wait_for_proc_work;
	uint32_t return_error;

	if (*consumed == 0) {
		if (put_user(BR_NOOP, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
	}

retry:
	binder_proc_lock(proc, __LINE__);
	return_error = thread->return_error;
	if (return_error != BR_OK && ptr < end) {
		uint32_t return_error2 = thread->return_error2;

		if (return_error2 != BR_OK) {
			/* Clear return_error2, we always have space */
			thread->return_error2 = BR_OK;
			if (ptr + sizeof(uint32_t) < end) {
				/* have space for return_error as well */
				thread->return_error = BR_OK;
			}
		} else {
			/* No return_error2, always space for return_error */
			thread->return_error = BR_OK;
		}
		binder_proc_unlock(proc, __LINE__);
		if (return_error2 != BR_OK) {
			if (put_user(return_error2, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			binder_stat_br(proc, thread, return_error2);
			if (ptr == end)
				goto done;
		}

		if (put_user(return_error, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		binder_stat_br(proc, thread, return_error);
		goto done;
	}

	wait_for_proc_work = binder_available_for_proc_work(thread);
	binder_proc_unlock(proc, __LINE__);

	if (wait_for_proc_work)
		atomic_inc(&proc->ready_threads);

	trace_binder_wait_for_work(wait_for_proc_work,
				   !!thread->transaction_stack,
				   !binder_worklist_empty(&thread->todo));

	thread->looper |= BINDER_LOOPER_STATE_WAITING;

	if (wait_for_proc_work) {
		BUG_ON(!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
					   BINDER_LOOPER_STATE_ENTERED)));
		binder_set_nice(current, proc->default_priority);
	}

	if (non_block) {
		if (!binder_has_work(thread, wait_for_proc_work))
			ret = -EAGAIN;
	} else {
		ret = binder_wait_for_work(thread, wait_for_proc_work);
	}

	if (wait_for_proc_work)
		atomic_dec(&proc->ready_threads);

	if (ret)
		return ret;

	thread->looper &= ~BINDER_LOOPER_STATE_WAITING;

	while (1) {
		uint32_t cmd;
		struct binder_transaction_data tr;
		struct binder_transaction *t = NULL;
		struct binder_work *w = NULL;
		wlist = NULL;

		binder_proc_lock(thread->proc, __LINE__);
		spin_lock(&thread->todo.lock);
		if (!_binder_worklist_empty(&thread->todo)) {
			w = list_first_entry(&thread->todo.list,
					     struct binder_work,
					     entry);
			wlist = &thread->todo;
			binder_freeze_worklist(wlist);
		}
		spin_unlock(&thread->todo.lock);
		if (!w) {
			spin_lock(&proc->todo.lock);
			if (!_binder_worklist_empty(&proc->todo) &&
					wait_for_proc_work) {
				w = list_first_entry(&proc->todo.list,
						     struct binder_work,
						     entry);
				wlist = &proc->todo;
				binder_freeze_worklist(wlist);
			}
			spin_unlock(&proc->todo.lock);
			if (!w) {
				binder_proc_unlock(thread->proc, __LINE__);
				/* no data added */
				if (ptr - buffer == 4 &&
				    !READ_ONCE(thread->looper_need_return))
					goto retry;
				break;
			}
		}
		binder_proc_unlock(thread->proc, __LINE__);
		if (end - ptr < sizeof(tr) + 4) {
			if (wlist)
				binder_unfreeze_worklist(wlist);
			break;
		}

		switch (w->type) {
		case BINDER_WORK_TRANSACTION: {
			t = container_of(w, struct binder_transaction, work);
		} break;
		case BINDER_WORK_TRANSACTION_COMPLETE: {
			cmd = BR_TRANSACTION_COMPLETE;
			if (put_user(cmd, (uint32_t __user *)ptr)) {
				binder_unfreeze_worklist(wlist);
				return -EFAULT;
			}
			ptr += sizeof(uint32_t);

			binder_stat_br(proc, thread, cmd);
			binder_debug(BINDER_DEBUG_TRANSACTION_COMPLETE,
				     "%d:%d BR_TRANSACTION_COMPLETE\n",
				     proc->pid, thread->pid);
			binder_dequeue_work(w, __LINE__);
			binder_unfreeze_worklist(wlist);
			kfree(w);
			binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
		} break;
		case BINDER_WORK_NODE: {
			struct binder_node *node = container_of(w, struct binder_node, work);
			uint32_t cmd[2];
			const char *cmd_name[2];
			int cmd_count = 0;
			int strong, weak;
			struct binder_proc *proc = node->proc;
			int i;

			binder_proc_lock(proc, __LINE__);
			strong = node->internal_strong_refs ||
					node->local_strong_refs;
			weak = !hlist_empty(&node->refs) ||
					node->local_weak_refs || strong;

			if (weak && !node->has_weak_ref) {
				cmd[cmd_count] = BR_INCREFS;
				cmd_name[cmd_count++] = "BR_INCREFS";
				node->has_weak_ref = 1;
				node->pending_weak_ref = 1;
				node->local_weak_refs++;
			}
			if (strong && !node->has_strong_ref) {
				cmd[cmd_count] = BR_ACQUIRE;
				cmd_name[cmd_count++] = "BR_ACQUIRE";
				node->has_strong_ref = 1;
				node->pending_strong_ref = 1;
				node->local_strong_refs++;
			}
			if (!strong && node->has_strong_ref) {
				cmd[cmd_count] = BR_RELEASE;
				cmd_name[cmd_count++] = "BR_RELEASE";
				node->has_strong_ref = 0;
			}
			if (!weak && node->has_weak_ref) {
				cmd[cmd_count] = BR_DECREFS;
				cmd_name[cmd_count++] = "BR_DECREFS";
				node->has_weak_ref = 0;
			}
			BUG_ON(cmd_count > 2);

			if (!weak && !strong && !node->is_zombie) {
				binder_debug(BINDER_DEBUG_INTERNAL_REFS,
					     "%d:%d node %d u%016llx c%016llx deleted\n",
					     proc->pid, thread->pid,
					     node->debug_id,
					     (u64)node->ptr,
					     (u64)node->cookie);
				_binder_make_node_zombie(node);
			} else {
				binder_debug(BINDER_DEBUG_INTERNAL_REFS,
					     "%d:%d node %d u%016llx c%016llx state unchanged\n",
					     proc->pid, thread->pid,
					     node->debug_id,
					     (u64)node->ptr,
					     (u64)node->cookie);
			}
			binder_dequeue_work(w, __LINE__);
			binder_unfreeze_worklist(wlist);
			binder_proc_unlock(proc, __LINE__);

			for (i = 0; i < cmd_count; i++) {
				if (put_user(cmd[i], (uint32_t __user *)ptr))
					return -EFAULT;
				ptr += sizeof(uint32_t);

				if (put_user(node->ptr,
					     (binder_uintptr_t __user *)ptr))
					return -EFAULT;
				ptr += sizeof(binder_uintptr_t);

				if (put_user(node->cookie,
					     (binder_uintptr_t __user *)ptr))
					return -EFAULT;
				ptr += sizeof(binder_uintptr_t);

				binder_stat_br(proc, thread, cmd[i]);
				binder_debug(BINDER_DEBUG_USER_REFS,
					     "%d:%d %s %d u%016llx c%016llx\n",
					     proc->pid, thread->pid,
					     cmd_name[i], node->debug_id,
					     (u64)node->ptr, (u64)node->cookie);
			}
		} break;
		case BINDER_WORK_DEAD_BINDER:
		case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
		case BINDER_WORK_CLEAR_DEATH_NOTIFICATION: {
			struct binder_ref_death *death;
			uint32_t cmd;

			death = container_of(w, struct binder_ref_death, work);
			if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION)
				cmd = BR_CLEAR_DEATH_NOTIFICATION_DONE;
			else
				cmd = BR_DEAD_BINDER;
			if (put_user(cmd, (uint32_t __user *)ptr)) {
				binder_unfreeze_worklist(wlist);
				return -EFAULT;
			}
			ptr += sizeof(uint32_t);
			if (put_user(death->cookie,
				     (binder_uintptr_t __user *)ptr)) {
				binder_unfreeze_worklist(wlist);
				return -EFAULT;
			}
			ptr += sizeof(binder_uintptr_t);
			binder_stat_br(proc, thread, cmd);
			binder_debug(BINDER_DEBUG_DEATH_NOTIFICATION,
				     "%d:%d %s %016llx\n",
				      proc->pid, thread->pid,
				      cmd == BR_DEAD_BINDER ?
				      "BR_DEAD_BINDER" :
				      "BR_CLEAR_DEATH_NOTIFICATION_DONE",
				      (u64)death->cookie);

			binder_proc_lock(proc, __LINE__);
			if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION) {
				binder_dequeue_work(w, __LINE__);
				binder_proc_unlock(proc, __LINE__);
				kfree(death);
				binder_stats_deleted(BINDER_STAT_DEATH);
			} else {
				binder_dequeue_work(w, __LINE__);
				binder_enqueue_work(w, &proc->delivered_death,
						    __LINE__);
				binder_proc_unlock(proc, __LINE__);

			}
			binder_unfreeze_worklist(wlist);
			if (cmd == BR_DEAD_BINDER)
				goto done; /* DEAD_BINDER notifications can cause transactions */
		} break;
		default:
			pr_err("%s: Unknown work type: %d\n",
					__func__, w->type);
			BUG();
		}

		if (!t)
			continue;

		BUG_ON(!wlist || !wlist->freeze);
		BUG_ON(t->buffer == NULL);
		if (t->buffer->target_node) {
			struct binder_node *target_node = t->buffer->target_node;

			tr.target.ptr = target_node->ptr;
			tr.cookie =  target_node->cookie;
			/* Don't need a lock to check set_priority_called, since
			 * the lock was held when pulling t of the workqueue,
			 * and it hasn't changed since then
			 */
			if (!t->set_priority_called)
				binder_set_priority(current, t, target_node);
			cmd = BR_TRANSACTION;
		} else {
			tr.target.ptr = 0;
			tr.cookie = 0;
			cmd = BR_REPLY;
		}
		tr.code = t->code;
		tr.flags = t->flags;
		tr.sender_euid = from_kuid(current_user_ns(), t->sender_euid);

		if (t->from) {
			struct task_struct *sender = t->from->proc->tsk;

			tr.sender_pid = task_tgid_nr_ns(sender,
							task_active_pid_ns(current));
		} else {
			tr.sender_pid = 0;
		}

		tr.data_size = t->buffer->data_size;
		tr.offsets_size = t->buffer->offsets_size;
		tr.data.ptr.buffer = (binder_uintptr_t)
			((uintptr_t)t->buffer->data +
			binder_alloc_get_user_buffer_offset(&proc->alloc));
		tr.data.ptr.offsets = tr.data.ptr.buffer +
					ALIGN(t->buffer->data_size,
					    sizeof(void *));

		if (put_user(cmd, (uint32_t __user *)ptr)) {
			binder_unfreeze_worklist(wlist);
			return -EFAULT;
		}
		ptr += sizeof(uint32_t);
		if (copy_to_user(ptr, &tr, sizeof(tr))) {
			binder_unfreeze_worklist(wlist);
			return -EFAULT;
		}
		ptr += sizeof(tr);

		trace_binder_transaction_received(t);
		binder_stat_br(proc, thread, cmd);
		binder_debug(BINDER_DEBUG_TRANSACTION,
			     "%d:%d %s %d %d:%d, cmd %d size %zd-%zd ptr %016llx-%016llx\n",
			     proc->pid, thread->pid,
			     (cmd == BR_TRANSACTION) ? "BR_TRANSACTION" :
			     "BR_REPLY",
			     t->debug_id, t->from ? t->from->proc->pid : 0,
			     t->from ? t->from->pid : 0, cmd,
			     t->buffer->data_size, t->buffer->offsets_size,
			     (u64)tr.data.ptr.buffer, (u64)tr.data.ptr.offsets);

		binder_dequeue_work(&t->work, __LINE__);
		binder_unfreeze_worklist(wlist);
		t->buffer->allow_user_free = 1;
		if (cmd == BR_TRANSACTION && !(t->flags & TF_ONE_WAY)) {
			binder_proc_lock(thread->proc, __LINE__);
			t->to_parent = thread->transaction_stack;
			t->to_thread = thread;
			thread->transaction_stack = t;
			binder_proc_unlock(thread->proc, __LINE__);
		} else {
			binder_free_transaction(t);
		}
		break;
	}

done:
	*consumed = ptr - buffer;
	binder_proc_lock(thread->proc, __LINE__);
	if (proc->requested_threads +
			atomic_read(&proc->ready_threads) == 0 &&
			proc->requested_threads_started < proc->max_threads &&
			(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
					   BINDER_LOOPER_STATE_ENTERED))
		    /* the user-space code fails to */
		    /* spawn a new thread if we leave this out */) {
		proc->requested_threads++;
		binder_proc_unlock(thread->proc, __LINE__);

		binder_debug(BINDER_DEBUG_THREADS,
			     "%d:%d BR_SPAWN_LOOPER\n",
			     proc->pid, thread->pid);
		if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
			return -EFAULT;
		binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
	} else
		binder_proc_unlock(thread->proc, __LINE__);

	return 0;
}

static void binder_release_work(struct binder_worklist *wlist)
{
	struct binder_work *w;

	spin_lock(&wlist->lock);
	while (!list_empty(&wlist->list)) {

		if (wlist->freeze) {
			/* Very rare race. We can't release work now
			 * since the list is in use by a worker thread.
			 * This can be safely done when the zombie object
			 * is being reaped.
			 */
			spin_unlock(&wlist->lock);
			return;
		}
		w = list_first_entry(&wlist->list, struct binder_work, entry);
		_binder_dequeue_work(w, __LINE__);

		spin_unlock(&wlist->lock);

		switch (w->type) {
		case BINDER_WORK_TRANSACTION: {
			struct binder_transaction *t;

			t = container_of(w, struct binder_transaction, work);
			if (t->buffer->target_node &&
			    !(t->flags & TF_ONE_WAY)) {
				binder_send_failed_reply(t, BR_DEAD_REPLY);
			} else {
				binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
					"undelivered transaction %d\n",
					t->debug_id);
				t->buffer->transaction = NULL;
				kfree(t);
				binder_stats_deleted(BINDER_STAT_TRANSACTION);
			}
		} break;
		case BINDER_WORK_TRANSACTION_COMPLETE: {
			binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
				"undelivered TRANSACTION_COMPLETE\n");
			kfree(w);
			binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
		} break;
		case BINDER_WORK_DEAD_BINDER:
		case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
		case BINDER_WORK_CLEAR_DEATH_NOTIFICATION: {
			struct binder_ref_death *death;

			death = container_of(w, struct binder_ref_death, work);
			binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
				"undelivered death notification, %016llx\n",
				(u64)death->cookie);
			/*
			 * For the non-CLEAR cases, kfree ref->death freeing
			 * done in zombie cleanup path so avoid doing it here
			 */
			if (w->type == BINDER_WORK_DEAD_BINDER)
				break;
			kfree(death);
			binder_stats_deleted(BINDER_STAT_DEATH);
		} break;
		case BINDER_WORK_NODE:
			pr_info("unfinished BINDER_WORK_NODE, proc has died\n");
			break;
		default:
			pr_err("unexpected work type, %d, not freed\n",
			       w->type);
			BUG();
			break;
		}
		spin_lock(&wlist->lock);
	}
	spin_unlock(&wlist->lock);
}

static u64 binder_get_seq(struct binder_seq_head *tracker)
{
	u64 seq;
	/*
	 * No lock needed, worst case we return an overly conservative
	 * value.
	 */
	seq = READ_ONCE(tracker->lowest_seq);
	return seq;
}

atomic64_t binder_seq_count;

static inline u64 binder_get_next_seq(void)
{
	return atomic64_inc_return(&binder_seq_count);
}

static void binder_add_seq(struct binder_seq_node *node,
			   struct binder_seq_head *tracker)
{
	spin_lock(&tracker->lock);
	/*
	 * Was the node previously added?
	 * - binder_get_thread/put_thread should never be nested
	 * - binder_queue_for_zombie_cleanup should first delete and then
	 * enqueue, so this shouldn't happen.
	 */
	BUG_ON(!list_empty(&node->list_node));

	node->active_seq = binder_get_next_seq();
	list_add_tail(&node->list_node, &tracker->active_threads);
	if (node->active_seq < READ_ONCE(tracker->lowest_seq))
		WRITE_ONCE(tracker->lowest_seq, node->active_seq);

	tracker->active_count++;
	if (tracker->active_count > tracker->max_active_count)
		tracker->max_active_count = tracker->active_count;
	spin_unlock(&tracker->lock);
}

static void binder_del_seq(struct binder_seq_node *node,
				 struct binder_seq_head *tracker)
{
	spin_lock(&tracker->lock);
	/*
	 * No need to track leftmost node, the queue tracks it already
	 */
	list_del_init(&node->list_node);

	if (!list_empty(&tracker->active_threads)) {
		struct binder_seq_node *tmp;

		tmp = list_first_entry(&tracker->active_threads, typeof(*tmp),
				       list_node);
		WRITE_ONCE(tracker->lowest_seq, tmp->active_seq);
	} else {
		WRITE_ONCE(tracker->lowest_seq, ~0ULL);
	}
	spin_unlock(&tracker->lock);
}

static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
	struct binder_thread *thread = NULL;
	struct rb_node *parent = NULL;
	struct rb_node **p = &proc->threads.rb_node;
	bool need_alloc;

	binder_proc_lock(proc, __LINE__);
	while (*p) {
		parent = *p;
		thread = rb_entry(parent, struct binder_thread, rb_node);

		if (current->pid < thread->pid)
			p = &(*p)->rb_left;
		else if (current->pid > thread->pid)
			p = &(*p)->rb_right;
		else
			break;
	}
	need_alloc = *p == NULL;
	binder_proc_unlock(proc, __LINE__);

	if (need_alloc) {
		struct binder_thread *new_thread =
			kzalloc(sizeof(*thread), GFP_KERNEL);
		if (new_thread == NULL)
			return NULL;
		binder_stats_created(BINDER_STAT_THREAD);
		new_thread->proc = proc;
		new_thread->pid = current->pid;
		init_waitqueue_head(&new_thread->wait);
		binder_init_worklist(&new_thread->todo);
		WRITE_ONCE(new_thread->looper_need_return, true);
		new_thread->return_error = BR_OK;
		new_thread->return_error2 = BR_OK;
		INIT_LIST_HEAD(&new_thread->active_node.list_node);
		INIT_LIST_HEAD(&new_thread->waiting_thread_node);
		get_task_struct(current);
		new_thread->task = current;

		binder_proc_lock(proc, __LINE__);
		/*
		 * Since we gave up the proc lock, we need
		 * to recalc the insertion point in the rb tree.
		 */
		p = &proc->threads.rb_node;
		while (*p) {
			parent = *p;
			thread = rb_entry(parent,
					  struct binder_thread, rb_node);

			if (current->pid < thread->pid)
				p = &(*p)->rb_left;
			else if (current->pid > thread->pid)
				p = &(*p)->rb_right;
			else
				break;
		}
		/* This thread can't have been added */
		BUG_ON(*p != NULL);

		rb_link_node(&new_thread->rb_node, parent, p);
		rb_insert_color(&new_thread->rb_node, &proc->threads);
		thread = new_thread;
		binder_proc_unlock(proc, __LINE__);
	}
	/*
	 * Add to active threads
	 */
	binder_add_seq(&thread->active_node,
		       &binder_active_threads[binder_seq_hash(thread)]);
	proc->active_thread_count++;
	return thread;
}

static inline void binder_queue_for_zombie_cleanup(struct binder_proc *proc)
{
	binder_del_seq(&proc->zombie_proc, &zombie_procs);
	binder_add_seq(&proc->zombie_proc, &zombie_procs);
}
static inline void binder_dequeue_for_zombie_cleanup(struct binder_proc *proc)
{
	binder_del_seq(&proc->zombie_proc, &zombie_procs);
}

static void binder_put_thread(struct binder_thread *thread)
{
	binder_del_seq(&thread->active_node,
		       &binder_active_threads[binder_seq_hash(thread)]);
}

static int binder_free_thread(struct binder_proc *proc,
			      struct binder_thread *thread)
{
	struct binder_transaction *t;
	struct binder_transaction *send_reply = NULL;
	int active_transactions = 0;

	binder_proc_lock(thread->proc, __LINE__);
	if (thread->is_zombie) {
		/*
		 * Can be called twice: by binder_deferred_release
		 * and binder_ioctl(BINDER_THREAD_EXIT). Only process
		 * it the first time.
		 */
		binder_proc_unlock(thread->proc, __LINE__);
		return 0;
	}
	thread->is_zombie = true;
	rb_erase(&thread->rb_node, &proc->threads);
	t = thread->transaction_stack;
	if (t && t->to_thread == thread)
		send_reply = t;
	while (t) {
		active_transactions++;
		binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
			     "release %d:%d transaction %d %s, still active\n",
			      proc->pid, thread->pid,
			     t->debug_id,
			     (t->to_thread == thread) ? "in" : "out");

		if (t->to_thread == thread) {
			t->to_proc = NULL;
			t->to_thread = NULL;
			if (t->buffer) {
				t->buffer->transaction = NULL;
				t->buffer = NULL;
			}
			t = t->to_parent;
		} else if (t->from == thread) {
			t->from = NULL;
			t = t->from_parent;
		} else
			BUG();
	}
	binder_release_work(&thread->todo);
	INIT_HLIST_NODE(&thread->zombie_thread);
	hlist_add_head(&thread->zombie_thread, &proc->zombie_threads);
	binder_queue_for_zombie_cleanup(proc);
	binder_proc_unlock(thread->proc, __LINE__);

	if (send_reply)
		binder_send_failed_reply(send_reply, BR_DEAD_REPLY);
	binder_stats_zombie(BINDER_STAT_THREAD);
	return active_transactions;
}

static unsigned int binder_poll(struct file *filp,
				struct poll_table_struct *wait)
{
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread = NULL;
	bool wait_for_proc_work;

	thread = binder_get_thread(proc);

	binder_proc_lock(thread->proc, __LINE__);
	thread->looper |= BINDER_LOOPER_STATE_POLL;
	wait_for_proc_work = binder_available_for_proc_work(thread);
	binder_proc_unlock(thread->proc, __LINE__);

	if (binder_has_work(thread, wait_for_proc_work))
		goto ret_pollin;
	binder_put_thread(thread);
	poll_wait(filp, &thread->wait, wait);
	thread = binder_get_thread(proc);
	if (!thread)
		return -ENOENT;
	if (binder_has_work(thread, wait_for_proc_work))
		goto ret_pollin;
	binder_put_thread(thread);
	return 0;

ret_pollin:
	binder_put_thread(thread);
	return POLLIN;

}

static int binder_ioctl_write_read(struct file *filp,
				unsigned int cmd, unsigned long arg,
				struct binder_thread **threadp)
{
	int ret = 0;
	int thread_pid = (*threadp)->pid;
	struct binder_proc *proc = filp->private_data;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;
	struct binder_write_read bwr;

	if (size != sizeof(struct binder_write_read)) {
		ret = -EINVAL;
		goto out;
	}
	if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
		ret = -EFAULT;
		goto out;
	}
	binder_debug(BINDER_DEBUG_READ_WRITE,
		     "%d:%d write %lld at %016llx, read %lld at %016llx\n",
		     proc->pid, thread_pid,
		     (u64)bwr.write_size, (u64)bwr.write_buffer,
		     (u64)bwr.read_size, (u64)bwr.read_buffer);

	if (bwr.write_size > 0) {
		ret = binder_thread_write(proc, *threadp,
					  bwr.write_buffer,
					  bwr.write_size,
					  &bwr.write_consumed);
		trace_binder_write_done(ret);
		if (ret < 0) {
			bwr.read_consumed = 0;
			if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
				ret = -EFAULT;
			goto out;
		}
	}
	if (bwr.read_size > 0) {
		ret = binder_thread_read(proc, threadp, bwr.read_buffer,
					 bwr.read_size,
					 &bwr.read_consumed,
					 filp->f_flags & O_NONBLOCK);
		trace_binder_read_done(ret);
		if (!binder_worklist_empty(&proc->todo)) {
			binder_proc_lock(proc, __LINE__);
			binder_wakeup_thread(proc, false);
			binder_proc_unlock(proc, __LINE__);
		}
		if (ret < 0) {
			if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
				ret = -EFAULT;
			goto out;
		}
	}
	binder_debug(BINDER_DEBUG_READ_WRITE,
		     "%d:%d wrote %lld of %lld, read return %lld of %lld\n",
		     proc->pid, thread_pid,
		     (u64)bwr.write_consumed, (u64)bwr.write_size,
		     (u64)bwr.read_consumed, (u64)bwr.read_size);
	if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
		ret = -EFAULT;
		goto out;
	}
out:
	return ret;
}

static int binder_ioctl_set_inherit_fifo_prio(struct file *filp)
{
	int ret = 0;
	struct binder_proc *proc = filp->private_data;
	struct binder_context *context = proc->context;

	kuid_t curr_euid = current_euid();
	mutex_lock(&binder_context_mgr_node_lock);

	if (uid_valid(context->binder_context_mgr_uid)) {
		if (!uid_eq(context->binder_context_mgr_uid, curr_euid)) {
			pr_err("BINDER_SET_INHERIT_FIFO_PRIO bad uid %d != %d\n",
			       from_kuid(&init_user_ns, curr_euid),
			       from_kuid(&init_user_ns,
					 context->binder_context_mgr_uid));
			ret = -EPERM;
			goto out;
		}
	}

	context->inherit_fifo_prio = true;

 out:
	mutex_unlock(&binder_context_mgr_node_lock);
	return ret;
}


static int binder_ioctl_set_ctx_mgr(struct file *filp)
{
	int ret = 0;
	struct binder_proc *proc = filp->private_data;
	struct binder_context *context = proc->context;

	kuid_t curr_euid = current_euid();
	struct binder_node *temp;

	mutex_lock(&binder_context_mgr_node_lock);
	if (context->binder_context_mgr_node) {
		pr_err("BINDER_SET_CONTEXT_MGR already set\n");
		ret = -EBUSY;
		goto out;
	}
	ret = security_binder_set_context_mgr(proc->tsk);
	if (ret < 0)
		goto out;
	if (uid_valid(context->binder_context_mgr_uid)) {
		if (!uid_eq(context->binder_context_mgr_uid, curr_euid)) {
			pr_err("BINDER_SET_CONTEXT_MGR bad uid %d != %d\n",
			       from_kuid(&init_user_ns, curr_euid),
			       from_kuid(&init_user_ns,
					 context->binder_context_mgr_uid));
			ret = -EPERM;
			goto out;
		}
	} else {
		context->binder_context_mgr_uid = curr_euid;
	}

	temp = binder_new_node(proc, 0, 0);
	if (temp == NULL) {
		context->binder_context_mgr_uid = INVALID_UID;
		ret = -ENOMEM;
		goto out;
	}
	temp->local_weak_refs++;
	temp->local_strong_refs++;
	temp->has_strong_ref = 1;
	temp->has_weak_ref = 1;
	context->binder_context_mgr_node = temp;
	binder_put_node(temp);
out:
	mutex_unlock(&binder_context_mgr_node_lock);
	return ret;
}

static inline u64 binder_get_thread_seq(void)
{
	u64 thread_seq = ~0ULL;
	int i;

	for (i = 0; i < SEQ_BUCKETS; i++) {
		u64 ts = binder_get_seq(&binder_active_threads[i]);

		thread_seq = min(ts, thread_seq);
	}
	return thread_seq;
}

static void zombie_cleanup_check(struct binder_proc *proc)
{
	u64 thread_seq = binder_get_thread_seq();
	u64 zombie_seq = binder_get_seq(&zombie_procs);

	if (thread_seq > zombie_seq)
		binder_defer_work(proc, BINDER_ZOMBIE_CLEANUP);
}

static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	trace_binder_ioctl(cmd, arg);

	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret)
		goto err_wait_event;

	thread = binder_get_thread(proc);
	if (thread == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	switch (cmd) {
	case BINDER_WRITE_READ:
		ret = binder_ioctl_write_read(filp, cmd, arg, &thread);
		if (ret)
			goto err;
		break;
	case BINDER_SET_MAX_THREADS: {
		int max_threads;

		if (copy_from_user(&max_threads, ubuf,
				   sizeof(max_threads))) {
			ret = -EINVAL;
			goto err;
		}
		binder_proc_lock(proc, __LINE__);
		proc->max_threads = max_threads;
		binder_proc_unlock(proc, __LINE__);
		break;
	}
	case BINDER_SET_CONTEXT_MGR:
		ret = binder_ioctl_set_ctx_mgr(filp);
		if (ret)
			goto err;
		break;
	case BINDER_SET_INHERIT_FIFO_PRIO:
		ret = binder_ioctl_set_inherit_fifo_prio(filp);
		if (ret)
			goto err;
		break;

	case BINDER_THREAD_EXIT:
		binder_debug(BINDER_DEBUG_THREADS, "%d:%d exit\n",
			     proc->pid, thread->pid);
		binder_free_thread(proc, thread);
		binder_put_thread(thread);
		thread = NULL;
		break;
	case BINDER_VERSION: {
		struct binder_version __user *ver = ubuf;

		if (size != sizeof(struct binder_version)) {
			ret = -EINVAL;
			goto err;
		}

		if (put_user(BINDER_CURRENT_PROTOCOL_VERSION,
			     &ver->protocol_version)) {
			ret = -EINVAL;
			goto err;
		}
		break;
	}
	default:
		ret = -EINVAL;
		goto err;
	}
	ret = 0;
err:
	if (thread) {
		binder_proc_lock(thread->proc, __LINE__);
		WRITE_ONCE(thread->looper_need_return, false);
		binder_proc_unlock(thread->proc, __LINE__);
		binder_put_thread(thread);
		zombie_cleanup_check(proc);
	}
	wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret && ret != -ERESTARTSYS)
		pr_info("%d:%d ioctl %x %lx returned %d\n", proc->pid, current->pid, cmd, arg, ret);
err_wait_event:
	trace_binder_ioctl_done(ret);
	return ret;
}

static void binder_vma_open(struct vm_area_struct *vma)
{
	struct binder_proc *proc = vma->vm_private_data;

	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "%d open vm area %lx-%lx (%ld K) vma %lx pagep %lx\n",
		     proc->pid, vma->vm_start, vma->vm_end,
		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
		     (unsigned long)pgprot_val(vma->vm_page_prot));
}

static void binder_vma_close(struct vm_area_struct *vma)
{
	struct binder_proc *proc = vma->vm_private_data;

	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "%d close vm area %lx-%lx (%ld K) vma %lx pagep %lx\n",
		     proc->pid, vma->vm_start, vma->vm_end,
		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
		     (unsigned long)pgprot_val(vma->vm_page_prot));
	binder_alloc_vma_close(&proc->alloc);
	binder_defer_work(proc, BINDER_DEFERRED_PUT_FILES);
}

static int binder_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	return VM_FAULT_SIGBUS;
}

static struct vm_operations_struct binder_vm_ops = {
	.open = binder_vma_open,
	.close = binder_vma_close,
	.fault = binder_vm_fault,
};

static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	const char *failure_string;

	if (proc->tsk != current->group_leader)
		return -EINVAL;

	if ((vma->vm_end - vma->vm_start) > SZ_4M)
		vma->vm_end = vma->vm_start + SZ_4M;

	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder_mmap: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",
		     proc->pid, vma->vm_start, vma->vm_end,
		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
		     (unsigned long)pgprot_val(vma->vm_page_prot));

	if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {
		ret = -EPERM;
		failure_string = "bad vm_flags";
		goto err_bad_arg;
	}
	vma->vm_flags = (vma->vm_flags | VM_DONTCOPY) & ~VM_MAYWRITE;
	vma->vm_ops = &binder_vm_ops;
	vma->vm_private_data = proc;

	ret = binder_alloc_mmap_handler(&proc->alloc, vma);

	if (!ret) {
		proc->files = get_files_struct(current);
		return 0;
	}

err_bad_arg:
	pr_err("binder_mmap: %d %lx-%lx %s failed %d\n",
	       proc->pid, vma->vm_start, vma->vm_end, failure_string, ret);
	return ret;
}

static int binder_open(struct inode *nodp, struct file *filp)
{
	struct binder_proc *proc;
	struct binder_device *binder_dev;

	binder_debug(BINDER_DEBUG_OPEN_CLOSE, "binder_open: %d:%d\n",
		     current->group_leader->pid, current->pid);

	proc = kzalloc(sizeof(*proc), GFP_KERNEL);
	if (proc == NULL)
		return -ENOMEM;

	get_task_struct(current->group_leader);
	proc->tsk = current->group_leader;
	binder_init_worklist(&proc->todo);
	proc->default_priority = task_nice(current);
	binder_dev = container_of(filp->private_data, struct binder_device,
				  miscdev);
	proc->context = &binder_dev->context;
	binder_alloc_init(&proc->alloc);

	mutex_lock(&binder_procs_lock);

	binder_stats_created(BINDER_STAT_PROC);
	hlist_add_head(&proc->proc_node, &binder_procs);
	proc->pid = current->group_leader->pid;
	spin_lock_init(&proc->proc_lock);
	binder_init_worklist(&proc->delivered_death);
	atomic_set(&proc->ready_threads, 0);
	proc->max_threads = 0;
	proc->requested_threads = 0;
	proc->requested_threads_started = 0;
	INIT_LIST_HEAD(&proc->zombie_proc.list_node);
	INIT_HLIST_HEAD(&proc->zombie_refs);
	INIT_HLIST_HEAD(&proc->zombie_nodes);
	INIT_HLIST_HEAD(&proc->zombie_threads);
	INIT_LIST_HEAD(&proc->waiting_threads);
	filp->private_data = proc;

	mutex_unlock(&binder_procs_lock);

	if (binder_debugfs_dir_entry_proc) {
		char strbuf[11];

		snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
		/*
		 * proc debug entries are shared between contexts, so
		 * this will fail if the process tries to open the driver
		 * again with a different context. The priting code will
		 * anyway print all contexts that a given PID has, so this
		 * is not a problem.
		 */
		proc->debugfs_entry = debugfs_create_file(strbuf, S_IRUGO,
			binder_debugfs_dir_entry_proc,
			(void *)(unsigned long)proc->pid,
			&binder_proc_fops);
	}

	return 0;
}

static int binder_flush(struct file *filp, fl_owner_t id)
{
	struct binder_proc *proc = filp->private_data;

	binder_defer_work(proc, BINDER_DEFERRED_FLUSH);

	return 0;
}

static void binder_deferred_flush(struct binder_proc *proc)
{
	struct rb_node *n;
	int wake_count = 0;
	int count, i;

	do {
		wait_queue_head_t **waits;

		count = 0;
		binder_proc_lock(proc, __LINE__);
		for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n)) {
			struct binder_thread *thread;

			thread = rb_entry(n, struct binder_thread, rb_node);
			if (thread->looper & BINDER_LOOPER_STATE_WAITING)
				count++;
		}
		binder_proc_unlock(proc, __LINE__);

		waits = kcalloc(count, sizeof(*waits), GFP_KERNEL);
		if (waits == NULL)
			break;

		binder_proc_lock(proc, __LINE__);
		for (i = 0, n = rb_first(&proc->threads);
				n != NULL; n = rb_next(n)) {
			struct binder_thread *thread;

			thread = rb_entry(n, struct binder_thread, rb_node);
			WRITE_ONCE(thread->looper_need_return, true);
			if (thread->looper & BINDER_LOOPER_STATE_WAITING) {
				if (i < count)
					waits[i] = &thread->wait;
				i++;
			}
		}
		binder_proc_unlock(proc, __LINE__);

		if (i <= count) {
			while (--i >= 0) {
				wake_up_interruptible(waits[i]);
				wake_count++;
			}
		}
		kfree(waits);

		/* if another thread grew the tree, try again */
	} while (i > count);

	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder_flush: %d woke %d threads\n", proc->pid,
		     wake_count);
}

static int binder_release(struct inode *nodp, struct file *filp)
{
	struct binder_proc *proc = filp->private_data;

	debugfs_remove(proc->debugfs_entry);
	binder_defer_work(proc, BINDER_DEFERRED_RELEASE);

	return 0;
}

static int binder_node_release(struct binder_node *node, int refs)
{
	struct binder_ref *ref;
	int death = 0;
	struct binder_proc *proc = node->proc;
	struct binder_worklist tmplist;
	struct binder_work *w;

	BUG_ON(!proc);
	binder_proc_lock(proc, __LINE__);

	binder_dequeue_work(&node->work, __LINE__);
	binder_release_work(&node->async_todo);

	_binder_make_node_zombie(node);

	if (hlist_empty(&node->refs)) {
		binder_proc_unlock(proc, __LINE__);
		return refs;
	}

	node->local_strong_refs = 0;
	node->local_weak_refs = 0;

	binder_init_worklist(&tmplist);
	hlist_for_each_entry(ref, &node->refs, node_entry) {
		refs++;

		if (!ref->death)
			continue;

		death++;

		if (list_empty(&ref->death->work.entry)) {
			ref->death->work.type = BINDER_WORK_DEAD_BINDER;
			ref->death->wait_proc = ref->proc;
			binder_enqueue_work(&ref->death->work,
					    &tmplist,
					    __LINE__);
		} else
			BUG();
	}
	binder_proc_unlock(proc, __LINE__);

	while (!list_empty(&tmplist.list)) {
		struct binder_ref_death *death;
		struct binder_proc *wait_proc;

		w = list_first_entry(&tmplist.list, struct binder_work, entry);
		death = container_of(w, struct binder_ref_death, work);
		binder_proc_lock(proc, __LINE__);
		binder_dequeue_work(w, __LINE__);
		/*
		 * It's not safe to touch death after
		 * enqueuing since it may be processed
		 * remotely and kfree'd
		 */
		wait_proc = death->wait_proc;
		binder_enqueue_work(w, &death->wait_proc->todo,
				    __LINE__);
		binder_proc_unlock(proc, __LINE__);
		binder_proc_lock(wait_proc, __LINE__);
		binder_wakeup_thread(wait_proc, false);
		binder_proc_unlock(wait_proc, __LINE__);
	}
	binder_debug(BINDER_DEBUG_DEAD_BINDER,
		     "node %d now dead, refs %d, death %d\n",
		     node->debug_id, refs, death);

	return refs;
}

static void binder_deferred_release(struct binder_proc *proc)
{
	struct binder_context *context = proc->context;
	struct rb_node *n;
	int threads, nodes, incoming_refs, outgoing_refs, active_transactions;

	BUG_ON(proc->files);

	binder_proc_lock(proc, __LINE__);
	binder_queue_for_zombie_cleanup(proc);
	binder_proc_unlock(proc, __LINE__);

	mutex_lock(&binder_procs_lock);
	hlist_del_init(&proc->proc_node);
	proc->is_zombie = true;
	mutex_unlock(&binder_procs_lock);

	mutex_lock(&binder_context_mgr_node_lock);
	if (context->binder_context_mgr_node &&
	    context->binder_context_mgr_node->proc == proc) {
		binder_debug(BINDER_DEBUG_DEAD_BINDER,
			     "%s: %d context_mgr_node gone\n",
			     __func__, proc->pid);
		context->binder_context_mgr_node = NULL;
	}
	mutex_unlock(&binder_context_mgr_node_lock);

	threads = 0;
	active_transactions = 0;
	binder_proc_lock(proc, __LINE__);
	while ((n = rb_first(&proc->threads))) {
		struct binder_thread *thread;

		thread = rb_entry(n, struct binder_thread, rb_node);
		binder_proc_unlock(proc, __LINE__);
		threads++;
		active_transactions += binder_free_thread(proc, thread);
		binder_proc_lock(proc, __LINE__);
	}

	nodes = 0;
	incoming_refs = 0;
	while ((n = rb_first(&proc->nodes))) {
		struct binder_node *node;

		node = rb_entry(n, struct binder_node, rb_node);
		nodes++;
		binder_proc_unlock(proc, __LINE__);
		incoming_refs = binder_node_release(node, incoming_refs);
		binder_proc_lock(proc, __LINE__);
	}

	outgoing_refs = 0;
	while ((n = rb_first(&proc->refs_by_desc))) {
		struct binder_ref *ref;

		ref = rb_entry(n, struct binder_ref, rb_node_desc);
		outgoing_refs++;
		binder_proc_unlock(proc, __LINE__);
		binder_delete_ref(ref, true, __LINE__);
		binder_proc_lock(proc, __LINE__);
	}
	binder_proc_unlock(proc, __LINE__);

	binder_release_work(&proc->todo);
	binder_release_work(&proc->delivered_death);
	binder_stats_zombie(BINDER_STAT_PROC);

	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "%s: %d threads %d, nodes %d (ref %d), refs %d, active transactions %d\n",
		     __func__, proc->pid, threads, nodes, incoming_refs,
		     outgoing_refs, active_transactions);
}

static int cleared_nodes;
static int cleared_threads;
static int cleared_procs;

static bool binder_proc_clear_zombies(struct binder_proc *proc)
{
	struct binder_node *node;
	struct hlist_node *tmp;
	struct binder_thread *thread;
	struct binder_ref *ref;
	struct hlist_head nodes_to_free;
	struct hlist_head threads_to_free;
	struct hlist_head refs_to_free;
	bool needs_requeue = false;
	struct files_struct *files;

	INIT_HLIST_HEAD(&nodes_to_free);
	INIT_HLIST_HEAD(&threads_to_free);
	INIT_HLIST_HEAD(&refs_to_free);

	binder_proc_lock(proc, __LINE__);
	if (!list_empty(&proc->zombie_proc.list_node)) {
		/* The proc has been re-queued with new zombie objects */
		binder_proc_unlock(proc, __LINE__);
		return 0;
	}
	hlist_for_each_entry_safe(ref, tmp, &proc->zombie_refs, zombie_ref) {
		hlist_del_init(&ref->zombie_ref);
		hlist_add_head(&ref->zombie_ref, &refs_to_free);
	}
	if (!RB_EMPTY_ROOT(&proc->refs_by_desc))
		needs_requeue = true;

	hlist_for_each_entry_safe(node, tmp, &proc->zombie_nodes, dead_node)
		if (hlist_empty(&node->refs)) {
			hlist_del_init(&node->dead_node);
			hlist_add_head(&node->dead_node, &nodes_to_free);
		}
	if (!hlist_empty(&proc->zombie_nodes) || !RB_EMPTY_ROOT(&proc->nodes))
		needs_requeue = true;

	hlist_for_each_entry_safe(thread, tmp, &proc->zombie_threads,
				  zombie_thread) {
		hlist_del_init(&thread->zombie_thread);
		hlist_add_head(&thread->zombie_thread, &threads_to_free);
	}
	if (!RB_EMPTY_ROOT(&proc->threads))
		needs_requeue = true;

	files = proc->zombie_files;
	proc->zombie_files = NULL;

	binder_proc_unlock(proc, __LINE__);

	if (files)
		put_files_struct(files);

	hlist_for_each_entry_safe(node, tmp, &nodes_to_free, dead_node) {
		int work_line = node->work.last_line;
		hlist_del_init(&node->dead_node);
		binder_dequeue_work(&node->work, __LINE__);
		if (!list_empty(&node->work.entry)) {
			pr_err("binder work node reinserted. last node op: %d, line; %d",
			       node->last_op, node->last_line);
			pr_err("last work insertion at line %d\n", work_line);
			BUG();
		}
		BUG_ON(node->async_todo.freeze);
		binder_release_work(&node->async_todo);
		BUG_ON(!binder_worklist_empty(&node->async_todo));
		kfree(node);
		binder_stats_delete_zombie(BINDER_STAT_NODE);
		cleared_nodes++;
	}
	hlist_for_each_entry_safe(thread, tmp, &threads_to_free,
				  zombie_thread) {
		hlist_del_init(&thread->zombie_thread);
		BUG_ON(thread->todo.freeze);
		binder_release_work(&thread->todo);
		BUG_ON(!binder_worklist_empty(&thread->todo));
		put_task_struct(thread->task);
		kfree(thread);
		binder_stats_delete_zombie(BINDER_STAT_THREAD);
		cleared_threads++;
	}
	hlist_for_each_entry_safe(ref, tmp, &refs_to_free, zombie_ref) {
		hlist_del_init(&ref->zombie_ref);
		if (ref->death) {
			binder_dequeue_work(&ref->death->work, __LINE__);
			kfree(ref->death);
			binder_stats_delete_zombie(BINDER_STAT_DEATH);
		}
		kfree(ref);
		binder_stats_delete_zombie(BINDER_STAT_REF);
	}

	return proc->is_zombie && !needs_requeue;
}

static void binder_clear_zombies(void)
{
	struct binder_proc *proc;
	struct binder_seq_node *z;

	spin_lock(&zombie_procs.lock);
	if (list_empty(&zombie_procs.active_threads)) {
		spin_unlock(&zombie_procs.lock);
		return;
	}

	while ((z = list_first_entry_or_null(&zombie_procs.active_threads,
					     typeof(*z), list_node)) != NULL) {
		if (binder_get_thread_seq() < z->active_seq)
			break;
		list_del_init(&z->list_node);

		if (!list_empty(&zombie_procs.active_threads)) {
			struct binder_seq_node *tmp;

			tmp = list_first_entry(&zombie_procs.active_threads,
					       typeof(*tmp), list_node);
			WRITE_ONCE(zombie_procs.lowest_seq, tmp->active_seq);
		} else {
			WRITE_ONCE(zombie_procs.lowest_seq, ~0ULL);
		}

		spin_unlock(&zombie_procs.lock);

		proc = container_of(z, struct binder_proc, zombie_proc);
		if (binder_proc_clear_zombies(proc)) {
			BUG_ON(proc->todo.freeze);
			BUG_ON(!list_empty(&proc->zombie_proc.list_node));
			binder_release_work(&proc->todo);
			binder_alloc_deferred_release(&proc->alloc);
			put_task_struct(proc->tsk);
			kfree(proc);
			cleared_procs++;
			binder_stats_delete_zombie(BINDER_STAT_PROC);
		}
		spin_lock(&zombie_procs.lock);
	}
	spin_unlock(&zombie_procs.lock);
}


static void binder_deferred_func(struct work_struct *work)
{
	struct binder_proc *proc;
	int defer;

	do {
		mutex_lock(&binder_deferred_lock);
		if (!hlist_empty(&binder_deferred_list)) {
			proc = hlist_entry(binder_deferred_list.first,
					struct binder_proc, deferred_work_node);
			hlist_del_init(&proc->deferred_work_node);
			defer = proc->deferred_work;
			proc->deferred_work = 0;
		} else {
			proc = NULL;
			defer = 0;
		}
		mutex_unlock(&binder_deferred_lock);

		if (defer & BINDER_DEFERRED_PUT_FILES) {
			binder_proc_lock(proc, __LINE__);
			if (proc->files) {
				BUG_ON(proc->zombie_files);
				proc->zombie_files = proc->files;
				proc->files = NULL;
				binder_queue_for_zombie_cleanup(proc);
				defer |= BINDER_ZOMBIE_CLEANUP;
			}
			binder_proc_unlock(proc, __LINE__);
		}

		if (defer & BINDER_DEFERRED_FLUSH)
			binder_deferred_flush(proc);

		if (defer & BINDER_DEFERRED_RELEASE)
			binder_deferred_release(proc); /* frees proc */

		if (defer & BINDER_ZOMBIE_CLEANUP)
			binder_clear_zombies();
	} while (proc);

}
static DECLARE_WORK(binder_deferred_work, binder_deferred_func);

static void
binder_defer_work(struct binder_proc *proc, enum binder_deferred_state defer)
{
	mutex_lock(&binder_deferred_lock);
	proc->deferred_work |= defer;
	if (hlist_unhashed(&proc->deferred_work_node)) {
		hlist_add_head(&proc->deferred_work_node,
				&binder_deferred_list);
		queue_work(binder_deferred_workqueue, &binder_deferred_work);
	}
	mutex_unlock(&binder_deferred_lock);
}

static void _print_binder_transaction(struct seq_file *m,
				      const char *prefix,
				      struct binder_transaction *t)
{
	seq_printf(m,
		   "%s %d: %pK from %d:%d to %d:%d code %x flags %x pri %ld r%d",
		   prefix, t->debug_id, t,
		   t->from ? t->from->proc->pid : 0,
		   t->from ? t->from->pid : 0,
		   t->to_proc ? t->to_proc->pid : 0,
		   t->to_thread ? t->to_thread->pid : 0,
		   t->code, t->flags, t->priority.nice, t->need_reply);
	if (t->buffer == NULL) {
		seq_puts(m, " buffer free\n");
		return;
	}
	if (t->buffer->target_node)
		seq_printf(m, " node %d",
			   t->buffer->target_node->debug_id);
	seq_printf(m, " size %zd:%zd data %pK\n",
		   t->buffer->data_size, t->buffer->offsets_size,
		   t->buffer->data);
}

static void _print_binder_work(struct seq_file *m, const char *prefix,
			       const char *transaction_prefix,
			       struct binder_work *w)
{
	struct binder_node *node;
	struct binder_transaction *t;

	BUG_ON(!spin_is_locked(&w->wlist->lock));

	switch (w->type) {
	case BINDER_WORK_TRANSACTION:
		t = container_of(w, struct binder_transaction, work);
		_print_binder_transaction(m, transaction_prefix, t);
		break;
	case BINDER_WORK_TRANSACTION_COMPLETE:
		seq_printf(m, "%stransaction complete\n", prefix);
		break;
	case BINDER_WORK_NODE:
		node = container_of(w, struct binder_node, work);
		seq_printf(m, "%snode work %d: u%016llx c%016llx\n",
			   prefix, node->debug_id,
			   (u64)node->ptr, (u64)node->cookie);
		break;
	case BINDER_WORK_DEAD_BINDER:
		seq_printf(m, "%shas dead binder\n", prefix);
		break;
	case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
		seq_printf(m, "%shas cleared dead binder\n", prefix);
		break;
	case BINDER_WORK_CLEAR_DEATH_NOTIFICATION:
		seq_printf(m, "%shas cleared death notification\n", prefix);
		break;
	default:
		seq_printf(m, "%sunknown work: type %d\n", prefix, w->type);
		break;
	}
}

static void _print_binder_thread(struct seq_file *m,
				 struct binder_thread *thread,
				 int print_always)
{
	struct binder_transaction *t;
	struct binder_work *w;
	size_t start_pos = m->count;
	size_t header_pos;

	BUG_ON(!spin_is_locked(&thread->proc->proc_lock));

	seq_printf(m, "  thread %d: l %02x need_return %d\n",
			thread->pid, thread->looper,
			READ_ONCE(thread->looper_need_return));
	header_pos = m->count;
	t = thread->transaction_stack;
	while (t) {
		if (t->from == thread) {
			_print_binder_transaction(m,
					"    outgoing transaction", t);
			t = t->from_parent;
		} else if (t->to_thread == thread) {
			_print_binder_transaction(m,
					"    incoming transaction", t);
			t = t->to_parent;
		} else {
			_print_binder_transaction(m,
					"    bad transaction", t);
			t = NULL;
		}
	}
	spin_lock(&thread->todo.lock);
	list_for_each_entry(w, &thread->todo.list, entry) {
		_print_binder_work(m, "    ", "    pending transaction", w);
	}
	spin_unlock(&thread->todo.lock);
	if (!print_always && m->count == header_pos)
		m->count = start_pos;
}

static void _print_binder_node(struct seq_file *m,
				     struct binder_node *node)
{
	struct binder_ref *ref;
	struct binder_work *w;
	int count;
	struct binder_proc *proc = node->proc;

	BUG_ON(!spin_is_locked(&proc->proc_lock));

	count = 0;
	hlist_for_each_entry(ref, &node->refs, node_entry)
		count++;

	seq_printf(m, "  node %d: u%016llx c%016llx hs %d hw %d ls %d lw %d is %d iw %d",
		   node->debug_id, (u64)node->ptr, (u64)node->cookie,
		   node->has_strong_ref, node->has_weak_ref,
		   node->local_strong_refs, node->local_weak_refs,
		   node->internal_strong_refs, count);
	if (count) {
		seq_puts(m, " proc");
		hlist_for_each_entry(ref, &node->refs, node_entry)
			seq_printf(m, " %d", ref->proc->pid);
	}
	seq_puts(m, "\n");
	spin_lock(&node->async_todo.lock);
	list_for_each_entry(w, &node->async_todo.list, entry)
		_print_binder_work(m, "    ",
				   "    pending async transaction", w);
	spin_unlock(&node->async_todo.lock);
}

static void _print_binder_ref(struct seq_file *m,
			      struct binder_ref *ref)
{
	BUG_ON(!spin_is_locked(&ref->proc->proc_lock));
	seq_printf(m, "  ref %d: desc %d %snode %d s %d w %d d %pK\n",
		   ref->debug_id, ref->desc, ref->node->proc ? "" : "dead ",
		   ref->node->debug_id, atomic_read(&ref->strong),
		   atomic_read(&ref->weak), ref->death);
}

static void print_binder_proc(struct seq_file *m,
			      struct binder_proc *proc, int print_all)
{
	struct binder_work *w;
	struct rb_node *n;
	size_t start_pos = m->count;
	size_t header_pos;

	seq_printf(m, "proc %d\n", proc->pid);
	seq_printf(m, "context %s\n", proc->context->name);
	header_pos = m->count;

	binder_proc_lock(proc, __LINE__);
	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n))
		_print_binder_thread(m, rb_entry(n, struct binder_thread,
				     rb_node), print_all);
	for (n = rb_first(&proc->nodes); n != NULL; n = rb_next(n)) {
		struct binder_node *node = rb_entry(n, struct binder_node,
						    rb_node);
		if (print_all || node->has_async_transaction)
			_print_binder_node(m, node);
	}
	if (print_all) {
		for (n = rb_first(&proc->refs_by_desc);
		     n != NULL;
		     n = rb_next(n))
			_print_binder_ref(m, rb_entry(n, struct binder_ref,
					  rb_node_desc));
	}
	binder_proc_unlock(proc, __LINE__);

	binder_alloc_print_allocated(m, &proc->alloc);
	spin_lock(&proc->todo.lock);
	list_for_each_entry(w, &proc->todo.list, entry)
		_print_binder_work(m, "  ", "  pending transaction", w);
	spin_unlock(&proc->todo.lock);
	spin_lock(&proc->delivered_death.lock);
	list_for_each_entry(w, &proc->delivered_death.list, entry) {
		seq_puts(m, "  has delivered dead binder\n");
		break;
	}
	spin_unlock(&proc->delivered_death.lock);
	if (!print_all && m->count == header_pos)
		m->count = start_pos;
}

static const char * const binder_return_strings[] = {
	"BR_ERROR",
	"BR_OK",
	"BR_TRANSACTION",
	"BR_REPLY",
	"BR_ACQUIRE_RESULT",
	"BR_DEAD_REPLY",
	"BR_TRANSACTION_COMPLETE",
	"BR_INCREFS",
	"BR_ACQUIRE",
	"BR_RELEASE",
	"BR_DECREFS",
	"BR_ATTEMPT_ACQUIRE",
	"BR_NOOP",
	"BR_SPAWN_LOOPER",
	"BR_FINISHED",
	"BR_DEAD_BINDER",
	"BR_CLEAR_DEATH_NOTIFICATION_DONE",
	"BR_FAILED_REPLY"
};

static const char * const binder_command_strings[] = {
	"BC_TRANSACTION",
	"BC_REPLY",
	"BC_ACQUIRE_RESULT",
	"BC_FREE_BUFFER",
	"BC_INCREFS",
	"BC_ACQUIRE",
	"BC_RELEASE",
	"BC_DECREFS",
	"BC_INCREFS_DONE",
	"BC_ACQUIRE_DONE",
	"BC_ATTEMPT_ACQUIRE",
	"BC_REGISTER_LOOPER",
	"BC_ENTER_LOOPER",
	"BC_EXIT_LOOPER",
	"BC_REQUEST_DEATH_NOTIFICATION",
	"BC_CLEAR_DEATH_NOTIFICATION",
	"BC_DEAD_BINDER_DONE",
	"BC_TRANSACTION_SG",
	"BC_REPLY_SG",
};

static const char * const binder_objstat_strings[] = {
	"proc",
	"thread",
	"node",
	"ref",
	"death",
	"transaction",
	"transaction_complete"
};

static void print_binder_stats(struct seq_file *m, const char *prefix,
			       struct binder_stats *stats)
{
	int i;

	BUILD_BUG_ON(ARRAY_SIZE(stats->bc) !=
		     ARRAY_SIZE(binder_command_strings));

	for (i = 0; i < ARRAY_SIZE(stats->bc); i++) {
		int temp = atomic_read(&stats->bc[i]);

		if (temp)
			seq_printf(m, "%s%s: %d\n", prefix,
				   binder_command_strings[i], temp);
	}

	BUILD_BUG_ON(ARRAY_SIZE(stats->br) !=
		     ARRAY_SIZE(binder_return_strings));

	for (i = 0; i < ARRAY_SIZE(stats->br); i++) {
		int temp = atomic_read(&stats->br[i]);

		if (temp)
			seq_printf(m, "%s%s: %d\n", prefix,
				   binder_return_strings[i], temp);
	}

	BUILD_BUG_ON(ARRAY_SIZE(stats->obj_created) !=
		     ARRAY_SIZE(binder_objstat_strings));
	BUILD_BUG_ON(ARRAY_SIZE(stats->obj_created) !=
		     ARRAY_SIZE(stats->obj_deleted));

	for (i = 0; i < ARRAY_SIZE(stats->obj_created); i++) {
		int created = atomic_read(&stats->obj_created[i]);
		int deleted = atomic_read(&stats->obj_deleted[i]);
		int zombie = atomic_read(&stats->obj_zombie[i]);

		if (created || deleted || zombie)
			seq_printf(m, "%s%s: active %d zombie %d total %d\n",
				prefix,
				binder_objstat_strings[i],
				created - deleted - zombie,
				zombie,
				created);
	}
}

static void print_binder_proc_stats(struct seq_file *m,
				    struct binder_proc *proc)
{
	struct binder_work *w;
	struct rb_node *n;
	struct binder_node *node;
	struct binder_thread *thread;
	struct binder_ref *ref;
	int count, strong, weak;
	int zombie_threads;
	int zombie_nodes;
	int zombie_refs;

	seq_printf(m, "proc %d%s\n", proc->pid,
			proc->is_zombie ? " (ZOMBIE)" : "");
	seq_printf(m, "context %s\n", proc->context->name);
	seq_printf(m, "context FIFO: %d\n", proc->context->inherit_fifo_prio);
	seq_printf(m, "  cleared: procs=%d nodes=%d threads=%d\n",
			cleared_procs, cleared_nodes, cleared_threads);
	zombie_threads = zombie_nodes = zombie_refs = count = 0;

	binder_proc_lock(proc, __LINE__);
	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n))
		count++;
	binder_proc_unlock(proc, __LINE__);
	seq_printf(m, "  threads: %d\n", count);
	seq_printf(m, "  requested threads: %d+%d/%d\n"
			"  ready threads %d\n"
			"  free async space %zd\n",
			proc->requested_threads,
			proc->requested_threads_started,
			proc->max_threads,
			atomic_read(&proc->ready_threads),
			binder_alloc_get_free_async_space(&proc->alloc));
	count = 0;
	binder_proc_lock(proc, __LINE__);
	if (!proc->is_zombie)
		for (n = rb_first(&proc->nodes); n != NULL; n = rb_next(n))
			count++;
	else {
		hlist_for_each_entry(node, &proc->zombie_nodes, dead_node)
			zombie_nodes++;
		hlist_for_each_entry(thread, &proc->zombie_threads,
				     zombie_thread)
			zombie_threads++;
		hlist_for_each_entry(ref, &proc->zombie_refs, zombie_ref)
			zombie_refs++;
	}
	binder_proc_unlock(proc, __LINE__);
	seq_printf(m, "  active threads: %d\n", proc->active_thread_count);
	seq_printf(m, "  nodes: %d\n", count);
	seq_printf(m, "  zombie nodes: %d\n", zombie_nodes);
	seq_printf(m, "  zombie threads: %d\n", zombie_threads);
	seq_printf(m, "  zombie refs: %d\n", zombie_refs);
	count = 0;
	strong = 0;
	weak = 0;
	binder_proc_lock(proc, __LINE__);
	for (n = rb_first(&proc->refs_by_desc); n != NULL; n = rb_next(n)) {
		struct binder_ref *ref = rb_entry(n, struct binder_ref,
						  rb_node_desc);
		count++;
		strong += atomic_read(&ref->strong);
		weak += atomic_read(&ref->weak);
	}
	binder_proc_unlock(proc, __LINE__);
	seq_printf(m, "  refs: %d s %d w %d\n", count, strong, weak);

	count = binder_alloc_get_allocated_count(&proc->alloc);
	seq_printf(m, "  buffers: %d\n", count);

	count = 0;
	spin_lock(&proc->todo.lock);
	list_for_each_entry(w, &proc->todo.list, entry) {
		if (w->type == BINDER_WORK_TRANSACTION)
			count++;
	}
	spin_unlock(&proc->todo.lock);
	seq_printf(m, "  pending transactions: %d\n", count);

	print_binder_stats(m, "  ", &proc->stats);
}


static int binder_state_show(struct seq_file *m, void *unused)
{
	struct binder_proc *proc;

	seq_puts(m, "binder state:\n");

	mutex_lock(&binder_procs_lock);
	hlist_for_each_entry(proc, &binder_procs, proc_node)
		print_binder_proc(m, proc, 1);
	mutex_unlock(&binder_procs_lock);
	return 0;
}

static int binder_stats_show(struct seq_file *m, void *unused)
{
	struct binder_proc *proc;
	int proc_count = 0;
	int i, sum = 0, maxactive = 0;

	seq_puts(m, "binder stats:\n");

	print_binder_stats(m, "", &binder_stats);

	mutex_lock(&binder_procs_lock);
	hlist_for_each_entry(proc, &binder_procs, proc_node) {
		proc_count++;
		print_binder_proc_stats(m, proc);
	}
	mutex_unlock(&binder_procs_lock);

	for (i = 0; i < SEQ_BUCKETS; i++) {
		sum += binder_active_threads[i].active_count;
		maxactive = max(maxactive,
				binder_active_threads[i].max_active_count);
		seq_printf(m, "  activeThread[%d]: %d/%d\n", i,
			binder_active_threads[i].active_count,
			binder_active_threads[i].max_active_count);
	}

	seq_printf(m, "procs=%d active_threads=%d/%d zombie_procs=%d/%d\n",
			proc_count,
			sum, maxactive,
			zombie_procs.active_count,
			zombie_procs.max_active_count);
	return 0;
}

static int binder_transactions_show(struct seq_file *m, void *unused)
{
	struct binder_proc *proc;

	seq_puts(m, "binder transactions:\n");
	mutex_lock(&binder_procs_lock);
	hlist_for_each_entry(proc, &binder_procs, proc_node)
		print_binder_proc(m, proc, 0);
	mutex_unlock(&binder_procs_lock);
	return 0;
}

static int binder_proc_show(struct seq_file *m, void *unused)
{
	struct binder_proc *itr;
	int pid = (unsigned long)m->private;

	mutex_lock(&binder_procs_lock);
	hlist_for_each_entry(itr, &binder_procs, proc_node) {
		if (itr->pid == pid) {
			seq_puts(m, "binder proc state:\n");
			print_binder_proc(m, itr, 1);
		}
	}
	mutex_unlock(&binder_procs_lock);

	return 0;
}

static void print_binder_transaction_log_entry(struct seq_file *m,
					struct binder_transaction_log_entry *e)
{
	seq_printf(m,
		   "%d: %s from %d:%d to %d:%d context %s node %d handle %d size %d:%d ret %d/%d l=%d\n",
		   e->debug_id, (e->call_type == 2) ? "reply" :
		   ((e->call_type == 1) ? "async" : "call "), e->from_proc,
		   e->from_thread, e->to_proc, e->to_thread, e->context_name,
		   e->to_node, e->target_handle, e->data_size, e->offsets_size,
		   e->return_error, e->return_error_param,
		   e->return_error_line);
}

static int binder_transaction_log_show(struct seq_file *m, void *unused)
{
	struct binder_transaction_log *log = m->private;
	int i;

	if (log->full) {
		for (i = log->next; i < ARRAY_SIZE(log->entry); i++)
			print_binder_transaction_log_entry(m, &log->entry[i]);
	}
	for (i = 0; i < log->next; i++)
		print_binder_transaction_log_entry(m, &log->entry[i]);
	return 0;
}

static const struct file_operations binder_fops = {
	.owner = THIS_MODULE,
	.poll = binder_poll,
	.unlocked_ioctl = binder_ioctl,
	.compat_ioctl = binder_ioctl,
	.mmap = binder_mmap,
	.open = binder_open,
	.flush = binder_flush,
	.release = binder_release,
};

BINDER_DEBUG_ENTRY(state);
BINDER_DEBUG_ENTRY(stats);
BINDER_DEBUG_ENTRY(transactions);
BINDER_DEBUG_ENTRY(transaction_log);

static int __init init_binder_device(const char *name)
{
	int ret;
	struct binder_device *binder_device;

	binder_device = kzalloc(sizeof(*binder_device), GFP_KERNEL);
	if (!binder_device)
		return -ENOMEM;

	binder_device->miscdev.fops = &binder_fops;
	binder_device->miscdev.minor = MISC_DYNAMIC_MINOR;
	binder_device->miscdev.name = name;

	binder_device->context.binder_context_mgr_uid = INVALID_UID;
	binder_device->context.name = name;

	ret = misc_register(&binder_device->miscdev);
	if (ret < 0) {
		kfree(binder_device);
		return ret;
	}

	hlist_add_head(&binder_device->hlist, &binder_devices);

	return ret;
}

static int __init binder_init(void)
{
	char *device_name, *device_names;
	struct binder_device *device;
	struct hlist_node *tmp;
	int ret, i;

	atomic_set(&binder_seq_count, 0);

	binder_deferred_workqueue = create_singlethread_workqueue("binder");
	if (!binder_deferred_workqueue)
		return -ENOMEM;

	binder_debugfs_dir_entry_root = debugfs_create_dir("binder", NULL);
	if (binder_debugfs_dir_entry_root)
		binder_debugfs_dir_entry_proc = debugfs_create_dir("proc",
						 binder_debugfs_dir_entry_root);

	if (binder_debugfs_dir_entry_root) {
		debugfs_create_file("state",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_state_fops);
		debugfs_create_file("stats",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_stats_fops);
		debugfs_create_file("transactions",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_transactions_fops);
		debugfs_create_file("transaction_log",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    &binder_transaction_log,
				    &binder_transaction_log_fops);
		debugfs_create_file("failed_transaction_log",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    &binder_transaction_log_failed,
				    &binder_transaction_log_fops);
	}

	/*
	 * Copy the module_parameter string, because we don't want to
	 * tokenize it in-place.
	 */
	device_names = kzalloc(strlen(binder_devices_param) + 1, GFP_KERNEL);
	if (!device_names) {
		ret = -ENOMEM;
		goto err_alloc_device_names_failed;
	}
	strcpy(device_names, binder_devices_param);

	while ((device_name = strsep(&device_names, ","))) {
		ret = init_binder_device(device_name);
		if (ret)
			goto err_init_binder_device_failed;
	}

	for (i = 0; i < SEQ_BUCKETS; i++) {
		spin_lock_init(&binder_active_threads[i].lock);
		INIT_LIST_HEAD(&binder_active_threads[i].active_threads);
		WRITE_ONCE(binder_active_threads[i].lowest_seq, ~0ULL);
	}

	INIT_LIST_HEAD(&zombie_procs.active_threads);
	spin_lock_init(&zombie_procs.lock);
	WRITE_ONCE(zombie_procs.lowest_seq, ~0ULL);

	return ret;

err_init_binder_device_failed:
	hlist_for_each_entry_safe(device, tmp, &binder_devices, hlist) {
		misc_deregister(&device->miscdev);
		hlist_del(&device->hlist);
		kfree(device);
	}
err_alloc_device_names_failed:
	debugfs_remove_recursive(binder_debugfs_dir_entry_root);

	destroy_workqueue(binder_deferred_workqueue);

	return ret;
}

device_initcall(binder_init);

#define CREATE_TRACE_POINTS
#include "binder_trace.h"

MODULE_LICENSE("GPL v2");
