/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/cred.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/rcupdate.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/printk.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/init.h>

#ifdef CONFIG_MTK_GPU_SUPPORT
#include <mt-plat/mtk_gpu_utility.h>
#endif

#ifdef CONFIG_MTK_ION
#include <mtk/ion_drv.h>
#endif

#ifdef CONFIG_ZRAM
#include <zram_drv.h>
#endif

#define MLOG_BUF_SHIFT		16	/* 64KB for 32bit, 128kB for 64bit */
#define MLOG_STR_LEN		32
#define MLOG_BUF_LEN		((1 << MLOG_BUF_SHIFT) >> 2)
#define MLOG_BUF_MASK		(MLOG_BUF_LEN-1)
#define MLOG_BUF(idx)		(mlog_buffer[(idx) & MLOG_BUF_MASK])

#define MLOG_ID			ULONG_MAX
#define MLOG_TRIGGER_TIMER	0

#define P2K(x)	(((unsigned long)x) << (PAGE_SHIFT - 10))
#define B2K(x)	(((unsigned long)x) >> (10))

static DEFINE_SPINLOCK(mlogbuf_lock);
DECLARE_WAIT_QUEUE_HEAD(mlog_wait);
static long mlog_buffer[MLOG_BUF_LEN];
static unsigned int mlog_start;
static unsigned int mlog_end;
static struct timer_list mlog_timer;
static unsigned long timer_intval = HZ;

/* Configurations for log dump */
const char * const meminfo_text[] = {
	"memfr",
	"swpfr",
	"cache",
	"kernel_stack",
	"page_table",
	"slab",
	"gpu",
	"gpu_page_cache",
	"mlock",	/* unevictable */
	"zram",
	"active",
	"inactive",
	"shmem",
	"ion",
};

const char * const vmstat_partial_text[] = {
	"swpin",
	"swpout",
	"fmflt",
};

const char * const proc_text[] = {
	"score_adj",
	"rss",
	"rswp",
	"pswpin",
	"pswpout",
	"pfmflt",
};

#define NR_MEMINFO_ITEMS	ARRAY_SIZE(meminfo_text)
#define NR_VMSTAT_PARTIAL_ITEMS	ARRAY_SIZE(vmstat_partial_text)
#define NR_PROC_ITEMS		ARRAY_SIZE(proc_text)

#define SWITCH_ON		UINT_MAX

/* 4 switches */
static unsigned int meminfo_switch = SWITCH_ON;
static unsigned int vmstat_switch = SWITCH_ON;
static unsigned int buddyinfo_switch = SWITCH_ON;
static unsigned int proc_switch = SWITCH_ON;

/* Sub controls for proc */
static int min_adj = OOM_SCORE_ADJ_MIN;
static int max_adj = OOM_SCORE_ADJ_MAX;
static int limit_pid = -1;

/* Refinement for analyses */
#define TRACE_HUNGER_PERCENTAGE	(50)
static unsigned long hungersize;

/* strfmt control */
static const char **strfmt_list;
static int strfmt_idx;
static int strfmt_len;
static int strfmt_proc;

/* Session control */
static atomic_t sessions_in_dump = ATOMIC_INIT(0);

/* Format string */
static const char fmt_hdr[] = "<type>,    [time]";
static const char cr_str[] = "%c";
static const char type_str[] = "<%ld>";
static const char time_sec_str[] = ",[%5lu";
static const char time_nanosec_str[] = ".%06lu]";
static const char mem_size_str[] = ",%6lu";
static const char acc_count_str[] = ",%7lu";
static const char pid_str[] = ",[%lu]";
static const char pname_str[] = ", %s";
static const char adj_str[] = ", %5ld";

/* Format string for buddyinfo */
static const char order_start_str[] = ", [%6lu";
static const char order_middle_str[] = ", %6lu";
static const char order_end_str[] = ", %6lu]";

/* Structures for one-shot logging */
struct mlog_header {
	char *buffer;
	size_t index;
	size_t len;
};
struct mlog_session {
	unsigned int start;
	unsigned int end;
	int fmt_idx;
	bool is_header_dump;
	struct mlog_header header;
};

static void mlog_emit(long v)
{
	MLOG_BUF(mlog_end) = v;
	mlog_end++;
	if (mlog_end - mlog_start > MLOG_BUF_LEN)
		mlog_start = mlog_end - MLOG_BUF_LEN;
}

/* Calculate strfmt length and try to allocate corresponding buffer */
static void *get_length_and_buffer(void)
{
	int len = 4;	/* id, type, sec, nanosec */
	void *tmp = NULL;

	/* meminfo items */
	if (meminfo_switch)
		len += NR_MEMINFO_ITEMS;

	/* vm status items */
	if (vmstat_switch)
		len += NR_VMSTAT_PARTIAL_ITEMS;

	/* buddy information items */
	if (buddyinfo_switch) {
		struct zone *zone;

		for_each_populated_zone(zone) {
			len += MAX_ORDER;
		}
	}

	/* proc items */
	if (proc_switch) {
		len++;	/* PID */
		len += NR_PROC_ITEMS;
	}

	/* allocate buffer for strfmt_list */
	if (!strfmt_list || strfmt_len != len)
		tmp = kmalloc_array(len, sizeof(char *), GFP_ATOMIC);
	else
		tmp = strfmt_list;

	/* release previous allocated buffer */
	if (tmp && tmp != strfmt_list) {
		kfree(strfmt_list);
		strfmt_list = tmp;
		strfmt_len = len;
	}

	return tmp;
}

static void __mlog_reset(void)
{
	int len, i;

	/* Setup str format */
	len = 0;
	strfmt_proc = 0;
	strfmt_list[len++] = cr_str;
	strfmt_list[len++] = type_str;
	strfmt_list[len++] = time_sec_str;
	strfmt_list[len++] = time_nanosec_str;

	/* str format for meminfo items */
	if (meminfo_switch) {
		for (i = 0; i < NR_MEMINFO_ITEMS; ++i)
			strfmt_list[len++] = mem_size_str;
	}

	/* str format for vm status items */
	if (vmstat_switch) {
		for (i = 0; i < NR_VMSTAT_PARTIAL_ITEMS; ++i)
			strfmt_list[len++] = acc_count_str;
	}

	/* str format for buddy information items */
	if (buddyinfo_switch) {
		struct zone *zone;

		for_each_populated_zone(zone) {
			strfmt_list[len++] = order_start_str;
			for (i = 0; i < MAX_ORDER - 2; ++i)
				strfmt_list[len++] = order_middle_str;
			strfmt_list[len++] = order_end_str;
		}
	}

	/* str format for proc items */
	if (proc_switch) {
		strfmt_proc = len;
		strfmt_list[len++] = pid_str;	/* PID */
		strfmt_list[len++] = adj_str;	/* ADJ */
		for (i = 1; i < NR_PROC_ITEMS; ++i)
			strfmt_list[len++] = mem_size_str;
	}

	/* reset str format index */
	strfmt_idx = 0;

	/* reset mlog buffer index */
	mlog_end = mlog_start = 0;
}

static int mlog_reset(void)
{
	int ret = 0;

	spin_lock_bh(&mlogbuf_lock);

	/* check whether someone is in dump session */
	while (atomic_read(&sessions_in_dump) > 0) {
		spin_unlock_bh(&mlogbuf_lock);
		cond_resched();
		spin_lock_bh(&mlogbuf_lock);
	}

	/* initialization for strfmt and log buffer */
	if (get_length_and_buffer())
		__mlog_reset();
	else
		ret = -1;

	spin_unlock_bh(&mlogbuf_lock);

	return ret;
}

/*
 * Record basic memory information
 */
static void mlog_meminfo(void)
{
	unsigned long memfree;
	unsigned long swapfree;
	unsigned long cached;
	unsigned long kernel_stack;
	unsigned long page_table;
	unsigned long slab;
	unsigned int gpuuse = 0;
	unsigned int gpu_page_cache = 0;
	unsigned long unevictable;
	unsigned long zram = 0;
	unsigned long active;
	unsigned long inactive;
	unsigned long shmem;
	unsigned long ion = 0;

	/* available memory */
	memfree = P2K(global_zone_page_state(NR_FREE_PAGES));
	swapfree = P2K(atomic_long_read(&nr_swap_pages));
	cached = P2K(global_node_page_state(NR_FILE_PAGES) -
			total_swapcache_pages());

	/* kernel memory usage */
	kernel_stack = global_zone_page_state(NR_KERNEL_STACK_KB);
	page_table = P2K(global_zone_page_state(NR_PAGETABLE));
	slab = P2K(global_node_page_state(NR_SLAB_UNRECLAIMABLE) +
			global_node_page_state(NR_SLAB_RECLAIMABLE));

#ifdef CONFIG_MTK_GPU_SUPPORT
	/* gpu memory usage */
	if (mtk_get_gpu_memory_usage(&gpuuse))
		gpuuse = B2K(gpuuse);
	if (mtk_get_gpu_page_cache(&gpu_page_cache))
		gpu_page_cache = B2K(gpu_page_cache);
#endif

#ifdef CONFIG_ZRAM
	/* zram memory usage */
	zram = P2K(global_zone_page_state(NR_ZSPAGES));
#endif

	/* user pages */
	active = P2K(global_node_page_state(NR_ACTIVE_ANON) +
			global_node_page_state(NR_ACTIVE_FILE));
	inactive = P2K(global_node_page_state(NR_INACTIVE_ANON) +
			global_node_page_state(NR_INACTIVE_FILE));
	unevictable = P2K(global_node_page_state(NR_UNEVICTABLE));

	shmem = P2K(global_node_page_state(NR_SHMEM));

#ifdef CONFIG_MTK_ION
	/* ION memory usage */
	ion = B2K((unsigned long)ion_mm_heap_total_memory());
#endif

	/* emit logs */
	spin_lock_bh(&mlogbuf_lock);
	mlog_emit(memfree);
	mlog_emit(swapfree);
	mlog_emit(cached);
	mlog_emit(kernel_stack);
	mlog_emit(page_table);
	mlog_emit(slab);
	mlog_emit(gpuuse);
	mlog_emit(gpu_page_cache);
	mlog_emit(unevictable);
	mlog_emit(zram);
	mlog_emit(active);
	mlog_emit(inactive);
	mlog_emit(shmem);
	mlog_emit(ion);
	spin_unlock_bh(&mlogbuf_lock);
}

/*
 * Entry for recording vmstat - record partial VM status
 */
static void mlog_vmstat(void)
{
	int cpu;
	unsigned long v[NR_VM_EVENT_ITEMS];

	memset(v, 0, NR_VM_EVENT_ITEMS * sizeof(unsigned long));

	for_each_online_cpu(cpu) {
		struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

		v[PSWPIN] += this->event[PSWPIN];
		v[PSWPOUT] += this->event[PSWPOUT];
		v[PGFMFAULT] += this->event[PGFMFAULT];
	}

	/* emit logs */
	spin_lock_bh(&mlogbuf_lock);
	mlog_emit(v[PSWPIN]);
	mlog_emit(v[PSWPOUT]);
	mlog_emit(v[PGFMFAULT]);
	spin_unlock_bh(&mlogbuf_lock);
}

/*
 * Record buddy information
 */
static void mlog_buddyinfo(void)
{
	struct zone *zone;

	for_each_populated_zone(zone) {
		unsigned long flags;
		unsigned int order;
		unsigned long nr[MAX_ORDER] = {0};

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; ++order)
			nr[order] = zone->free_area[order].nr_free;
		spin_unlock_irqrestore(&zone->lock, flags);

		/* emit logs */
		spin_lock_bh(&mlogbuf_lock);
		for (order = 0; order < MAX_ORDER; ++order)
			mlog_emit(nr[order]);
		spin_unlock_bh(&mlogbuf_lock);
	}
}

static struct task_struct *trylock_task_mm(struct task_struct *t)
{
	if (spin_trylock(&t->alloc_lock)) {
		if (likely(t->mm))
			return t;
		task_unlock(t);
	}
	return NULL;
}

static bool filter_out_process(struct task_struct *p, pid_t pid,
		short oom_score_adj, unsigned long sz)
{
	/* if the size is too large, just show it */
	if (sz >= hungersize)
		return false;

	/* by oom_score_adj */
	if (oom_score_adj > max_adj || oom_score_adj < min_adj)
		return true;

	/* by pid */
	if (limit_pid != -1 && pid != limit_pid)
		return true;

	/* by ppid, bypass process whose parent is init */
	if (pid_alive(p)) {
		if (task_pid_nr(rcu_dereference(p->real_parent)) == 1)
			return true;
	} else {
		return true;
	}

	return false;
}

/*
 * Record process information
 */
static void mlog_procinfo(void)
{
	struct task_struct *tsk;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		pid_t pid;
		short oom_score_adj;
		unsigned long rss;
		unsigned long rswap;

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = trylock_task_mm(tsk);
		if (!p)
			continue;

		if (!p->signal) {
			task_unlock(p);
			continue;
		}

		pid = p->pid;
		oom_score_adj = p->signal->oom_score_adj;
		rss = get_mm_rss(p->mm);
		rswap = get_mm_counter(p->mm, MM_SWAPENTS);
		if (filter_out_process(p, pid, oom_score_adj, rss + rswap)) {
			task_unlock(p);
			continue;
		}
		task_unlock(p);

		/* emit logs */
		spin_lock_bh(&mlogbuf_lock);
		mlog_emit(pid);
		mlog_emit(oom_score_adj);
		mlog_emit(P2K(rss));
		mlog_emit(P2K(rswap));
		mlog_emit(0);	/* pswpin */
		mlog_emit(0);	/* pswpout */
		mlog_emit(0);	/* pfmflt */
		spin_unlock_bh(&mlogbuf_lock);
	}
	rcu_read_unlock();
}

/*
 * Main entry of recording information
 */
static void mlog(int type)
{
	unsigned long nanosec_rem;
	unsigned long long t = local_clock();

	/* record time stamp */
	nanosec_rem = do_div(t, NSEC_PER_SEC);

	spin_lock_bh(&mlogbuf_lock);
	mlog_emit(MLOG_ID);	/* tag for correct start point */
	mlog_emit(type);
	mlog_emit((unsigned long)t);
	mlog_emit(nanosec_rem / 1000);
	spin_unlock_bh(&mlogbuf_lock);

	/* basic memory information */
	if (meminfo_switch)
		mlog_meminfo();

	/* vm status */
	if (vmstat_switch)
		mlog_vmstat();

	/* buddy information */
	if (buddyinfo_switch)
		mlog_buddyinfo();

	/* process information */
	if (proc_switch)
		mlog_procinfo();

	/* wake up pending request for dump */
	if (waitqueue_active(&mlog_wait))
		wake_up_interruptible(&mlog_wait);
}

int mlog_snprint_fmt(char *buf, size_t len)
{
	int ret = 0, i;

	ret = snprintf(buf, len, "%s", fmt_hdr);

	/* basic memory information */
	if (meminfo_switch) {
		for (i = 0; i < NR_MEMINFO_ITEMS; i++)
			ret += snprintf(buf + ret, len - ret, ", %s",
					meminfo_text[i]);
	}

	/* vm status */
	if (vmstat_switch) {
		for (i = 0; i < NR_VMSTAT_PARTIAL_ITEMS; i++)
			ret += snprintf(buf + ret, len - ret, ", %s",
					vmstat_partial_text[i]);
	}

	/* buddy information */
	if (buddyinfo_switch) {
		struct zone *zone;
		int order;

		for_each_populated_zone(zone) {
			ret += snprintf(buf + ret, len - ret, ", [%s: 0",
					zone->name);
			for (order = 1; order < MAX_ORDER; ++order)
				ret += snprintf(buf + ret, len - ret, ", %d",
						order);
			ret += snprintf(buf + ret, len - ret, "]");
		}
	}

	/* process information */
	if (proc_switch) {
		ret += snprintf(buf + ret, len - ret, ", [pid]");
		for (i = 0; i < NR_PROC_ITEMS; i++)
			ret += snprintf(buf + ret, len - ret, ", %s",
					proc_text[i]);
	}

	return ret;
}

int mlog_header_dump(char __user *buf, size_t len, struct mlog_header *header)
{
	int ret = min(len, header->len - header->index);

	if (__copy_to_user(buf, header->buffer + header->index, ret))
		return -EFAULT;

	header->index += ret;
	return ret;
}

int mlog_print_fmt(struct seq_file *m)
{
	int i;

	seq_printf(m, "%s", fmt_hdr);

	/* basic memory information */
	if (meminfo_switch) {
		for (i = 0; i < NR_MEMINFO_ITEMS; i++)
			seq_printf(m, ", %s", meminfo_text[i]);
	}

	/* vm status */
	if (vmstat_switch) {
		for (i = 0; i < NR_VMSTAT_PARTIAL_ITEMS; i++)
			seq_printf(m, ", %s", vmstat_partial_text[i]);
	}

	/* buddy information */
	if (buddyinfo_switch) {
		struct zone *zone;
		int order;

		for_each_populated_zone(zone) {
			seq_printf(m, ", [%s: 0", zone->name);
			for (order = 1; order < MAX_ORDER; ++order)
				seq_printf(m,	", %d", order);
			seq_puts(m,	"]");
		}
	}

	/* process information */
	if (proc_switch) {
		seq_puts(m, ", [pid]");
		for (i = 0; i < NR_PROC_ITEMS; i++)
			seq_printf(m, ", %s", proc_text[i]);
	}

	return 0;
}

static void start_dump_session(void)
{
	spin_lock_bh(&mlogbuf_lock);
	atomic_inc(&sessions_in_dump);
	spin_unlock_bh(&mlogbuf_lock);
}

static void stop_dump_session(void)
{
	atomic_dec(&sessions_in_dump);
}

static int mlog_unread(void)
{
	return mlog_end - mlog_start;
}

static int _doread(char __user *buf, size_t len, unsigned int *start,
		unsigned int *end, int *fmt_idx)
{
	int size = 0;
	long v = 0;
	bool no_update = true;
	char mlog_buf[MLOG_STR_LEN];

	spin_lock_bh(&mlogbuf_lock);

	/* mlog_start go over session->start, no data to dump */
	if (unlikely(*start < mlog_start))
		goto exit_dump;

	/* retrieve value */
	while (*start < *end) {
		v = MLOG_BUF(*start);
		*start += 1;
		no_update = false;

		/* check for start point */
		if (*fmt_idx == 0 && v != MLOG_ID)
			continue;
		else if (v == MLOG_ID && (*fmt_idx != 0))
			*fmt_idx = 0;

		break;
	}

	/* no more data */
	if (*start > *end || ((*start == *end) && no_update))
		goto exit_dump;

	/* hit start point, change to the next line */
	if (*fmt_idx == 0)
		v = '\n';

	size = snprintf(mlog_buf, MLOG_STR_LEN, strfmt_list[*fmt_idx], v);
	*fmt_idx += 1;

	/*
	 * strfmt_list is exhausted,
	 * just set it to strfmt_proc for next processes.
	 */
	if (*fmt_idx >= strfmt_len)
		*fmt_idx = strfmt_proc;

	spin_unlock_bh(&mlogbuf_lock);

	if (__copy_to_user(buf, mlog_buf, size))
		return -EFAULT;

	return size;

exit_dump:
	spin_unlock_bh(&mlogbuf_lock);
	return size;
}

static int mlog_doread(char __user *buf, size_t len)
{
	int error;
	size_t size = 0;
	size_t ret;

	if (!buf || len < 0)
		return -EINVAL;
	if (len == 0)
		return 0;
	if (!access_ok(VERIFY_WRITE, buf, len))
		return -EFAULT;

	error = wait_event_interruptible(mlog_wait, (mlog_start - mlog_end));
	if (error)
		return error;

	while (len > size + MLOG_STR_LEN) {
		start_dump_session();

		ret = _doread(buf + size, len - size, &mlog_start,
				&mlog_end, &strfmt_idx);

		stop_dump_session();

		if (ret <= 0)
			break;

		size = size + ret;
		cond_resched();
	}
	return size;
}

#if defined(CONFIG_DEBUG_FS)
static int dmlog_open(struct inode *inode, struct file *file)
{
#define FMT_HEADER_LENGTH	sizeof(fmt_hdr)
#define EXTRA_FMT_LENGTH	(3)	/* ", " & "\0" */

	struct mlog_session *session;
	struct mlog_header *header;
	int fmt_buf_len = FMT_HEADER_LENGTH;
	int i;

	/* start a log session */
	start_dump_session();

	if (meminfo_switch) {
		for (i = 0; i < NR_MEMINFO_ITEMS; i++)
			fmt_buf_len += strlen(meminfo_text[i]);
		fmt_buf_len += NR_MEMINFO_ITEMS * EXTRA_FMT_LENGTH;
	}

	if (vmstat_switch) {
		for (i = 0; i < NR_VMSTAT_PARTIAL_ITEMS; i++)
			fmt_buf_len += strlen(vmstat_partial_text[i]);
		fmt_buf_len += NR_VMSTAT_PARTIAL_ITEMS * EXTRA_FMT_LENGTH;
	}

	if (buddyinfo_switch) {
		struct zone *zone;

		for_each_populated_zone(zone) {
			fmt_buf_len += strlen(zone->name) + 7;
			fmt_buf_len += (2 + EXTRA_FMT_LENGTH) * (MAX_ORDER - 1);
			fmt_buf_len += strlen("]") + 1;
		}
	}

	if (proc_switch) {
		fmt_buf_len += strlen(", [pid]");
		for (i = 0; i < NR_PROC_ITEMS; i++)
			fmt_buf_len += strlen(proc_text[i]);
		fmt_buf_len += NR_PROC_ITEMS * EXTRA_FMT_LENGTH;
	}

	session = kzalloc(sizeof(struct mlog_session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	session->start = mlog_start;
	session->end = mlog_end;
	session->fmt_idx = 0;
	header = &session->header;
	header->buffer = kmalloc(fmt_buf_len, GFP_KERNEL);
	if (!header->buffer) {
		kfree(session);
		return -ENOMEM;
	}

	header->len = mlog_snprint_fmt(header->buffer, fmt_buf_len);

	file->private_data = session;
	return 0;

#undef EXTRA_FMT_LENGTH
#undef FMT_HEADER_LENGTH
}

static int dmlog_release(struct inode *inode, struct file *file)
{
	struct mlog_session *session = file->private_data;
	struct mlog_header *header = &session->header;

	kfree(header->buffer);
	kfree(file->private_data);

	/* terminate a log session */
	stop_dump_session();

	return 0;
}

static ssize_t dmlog_read(struct file *file, char __user *buf, size_t len,
		loff_t *ppos)
{
	size_t size = 0;
	size_t ret;
	struct mlog_session *session = file->private_data;
	struct mlog_header *header = &session->header;

	if (!buf || len < 0)
		return -EINVAL;
	if (len == 0)
		return 0;
	if (!access_ok(VERIFY_WRITE, buf, len))
		return -EFAULT;

	if (!session->is_header_dump) {
		ret = mlog_header_dump(buf, len, header);
		size = size + ret;
		if (header->index >= header->len)
			session->is_header_dump = true;
	}

	while (len > size + MLOG_STR_LEN) {
		ret = _doread(buf + size, len - size, &session->start,
				&session->end, &session->fmt_idx);

		if (ret <= 0)
			break;

		size = size + ret;
		cond_resched();
	}
	return size;
}
#endif

module_param(min_adj, int, 0644);
module_param(max_adj, int, 0644);
module_param(limit_pid, int, 0644);

static int do_switch_handler(const char *val, const struct kernel_param *kp)
{
	unsigned int oldval = *(unsigned int *)kp->arg;
	int ret;

	spin_lock_bh(&mlogbuf_lock);
	if (atomic_read(&sessions_in_dump) > 0) {
		ret = -EBUSY;
		goto exit_handler;
	}

	ret = param_set_uint(val, kp);
	if (ret < 0)
		goto exit_handler;

	/* try to allocate new buffer */
	if (get_length_and_buffer()) {
		__mlog_reset();
	} else {
		/* restore switch and reset buffer index */
		if (strfmt_list) {
			*(unsigned int *)kp->arg = oldval;
			strfmt_idx = 0;
			mlog_end = mlog_start = 0;
		}
	}

exit_handler:
	spin_unlock_bh(&mlogbuf_lock);

	return ret;
}

static const struct kernel_param_ops param_ops_change_switch = {
	.set = &do_switch_handler,
	.get = &param_get_uint,
	.free = NULL,
};

static int do_time_intval_handler(const char *val,
		const struct kernel_param *kp)
{
	int ret;

	ret = param_set_ulong(val, kp);
	if (ret < 0)
		return ret;

	mod_timer(&mlog_timer, jiffies);

	return 0;
}

static const struct kernel_param_ops param_ops_change_time_intval = {
	.set = &do_time_intval_handler,
	.get = &param_get_ulong,
	.free = NULL,
};

module_param_cb(meminfo_switch, &param_ops_change_switch,
		&meminfo_switch, 0644);
module_param_cb(vmstat_switch, &param_ops_change_switch,
		&vmstat_switch, 0644);
module_param_cb(buddyinfo_switch, &param_ops_change_switch,
		&buddyinfo_switch, 0644);
module_param_cb(proc_switch, &param_ops_change_switch,
		&proc_switch, 0644);

param_check_ulong(timer_intval, &timer_intval);
module_param_cb(timer_intval, &param_ops_change_time_intval,
		&timer_intval, 0644);
__MODULE_PARM_TYPE(timer_intval, ulong);

static int mlog_open(struct inode *inode, struct file *file)
{
	spin_lock_bh(&mlogbuf_lock);
	strfmt_idx = 0;
	spin_unlock_bh(&mlogbuf_lock);

	return 0;
}

static int mlog_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mlog_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	if (file->f_flags & O_NONBLOCK) {
		if (!mlog_unread())
			return -EAGAIN;
	}

	return mlog_doread(buf, count);
}

static unsigned int mlog_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &mlog_wait, wait);
	if (mlog_unread())
		return POLLIN | POLLRDNORM;

	return 0;
}

static const struct file_operations proc_mlog_operations = {
	.read = mlog_read,
	.poll = mlog_poll,
	.open = mlog_open,
	.release = mlog_release,
	.llseek = generic_file_llseek,
};

static int mlog_fmt_proc_show(struct seq_file *m, void *v)
{
	return mlog_print_fmt(m);
}

static int mlog_fmt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mlog_fmt_proc_show, NULL);
}

static const struct file_operations mlog_fmt_proc_fops = {
	.open = mlog_fmt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#if defined(CONFIG_DEBUG_FS)
static const struct file_operations proc_dmlog_operations = {
	.open = dmlog_open,
	.read = dmlog_read,
	.release = dmlog_release,
};
#elif defined(CONFIG_SYSFS)
static int _doread2(char *buf, unsigned int *start,
		unsigned int *end, int *fmt_idx)
{
	int size = 0;
	long v = 0;
	bool no_update = true;

	spin_lock_bh(&mlogbuf_lock);

	/* mlog_start go over session->start, no data to dump */
	if (unlikely(*start < mlog_start))
		goto exit_dump;

	/* retrieve value */
	while (*start < *end) {
		v = MLOG_BUF(*start);
		*start += 1;
		no_update = false;

		/* check for start point */
		if (*fmt_idx == 0 && v != MLOG_ID)
			continue;
		else if (v == MLOG_ID && (*fmt_idx != 0))
			*fmt_idx = 0;

		break;
	}

	/* no more data */
	if (*start > *end || ((*start == *end) && no_update))
		goto exit_dump;

	/* hit start point, change to the next line */
	if (*fmt_idx == 0)
		v = '\n';

	size = snprintf(buf, MLOG_STR_LEN, strfmt_list[*fmt_idx], v);
	*fmt_idx += 1;

	/*
	 * strfmt_list is exhausted, reset it to the beginning.
	 */
	if (*fmt_idx >= strfmt_len)
		*fmt_idx = 0;

	spin_unlock_bh(&mlogbuf_lock);

	return size;

exit_dump:
	spin_unlock_bh(&mlogbuf_lock);
	return size;
}

static ssize_t dump_show(struct kobject *kobj,
			 struct kobj_attribute *attr, char *buf)
{
	int size, ret;
	int fmt_idx = 0;
	unsigned int start, end;

	/* dump fmt */
	size = mlog_snprint_fmt(buf, PAGE_SIZE);

	/* dump data */
	start_dump_session();
	start = mlog_start;
	end = mlog_end;
	while (size < (PAGE_SIZE - MLOG_STR_LEN)) {
		ret = _doread2(buf + size, &start, &end, &fmt_idx);

		if (ret <= 0)
			break;

		size = size + ret;
	}
	stop_dump_session();

	return size;
}

static struct kobj_attribute mlog_attr = __ATTR_RO(dump);
static struct attribute *mlog_attrs[] = {
	&mlog_attr.attr,
	NULL,
};
struct attribute_group mlog_attr_group = {
	.attrs = mlog_attrs,
	.name = "mlog",
};
#endif

static void __init mlog_init_debugfs(void)
{
	debugfs_create_file("mlog_fmt", 0444, NULL, NULL,
			&mlog_fmt_proc_fops);
	debugfs_create_file("mlog", 0444, NULL, NULL,
			&proc_mlog_operations);
#if defined(CONFIG_DEBUG_FS)
	debugfs_create_file("dmlog", 0444, NULL, NULL,
			&proc_dmlog_operations);
#elif defined(CONFIG_SYSFS)
	/* No DEBUGFS, choose SYSFS as backup */
	if (sysfs_create_group(mm_kobj, &mlog_attr_group))
		pr_info("Failed to create sysfs interface\n");
#endif
}

static void mlog_timer_handler(unsigned long data)
{
	mlog(MLOG_TRIGGER_TIMER);
	mod_timer(&mlog_timer, round_jiffies(jiffies + timer_intval));
}

static int __init mlog_init_logger(void)
{
	int ret = 0;

	spin_lock_init(&mlogbuf_lock);
	ret = mlog_reset();
	if (ret)
		return ret;

	setup_timer(&mlog_timer, mlog_timer_handler, 0);
	mlog_timer.expires = jiffies + timer_intval;
	add_timer(&mlog_timer);

	return ret;
}

static void __exit mlog_exit_logger(void)
{
	kfree(strfmt_list);
	strfmt_list = NULL;
}

static int __init mlog_init(void)
{
	hungersize = totalram_pages * TRACE_HUNGER_PERCENTAGE / 100;

	if (mlog_init_logger())
		return -1;

	mlog_init_debugfs();
	return 0;
}

static void __exit mlog_exit(void)
{
	mlog_exit_logger();
}

module_init(mlog_init);
module_exit(mlog_exit);

MODULE_DESCRIPTION("Mediatek memory log driver");
MODULE_LICENSE("GPL");
