// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Chinwen Chang <chinwen.chang@mediatek.com>
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
#include <linux/init.h>

#define CREATE_TRACE_POINTS
#include "trace_mmstat.h"

#define P2K(x)	(((unsigned long)x) << (PAGE_SHIFT - 10))
#define B2K(x)	(((unsigned long)x) >> (10))

static struct delayed_work mmstat_work;
static unsigned long timer_intval = HZ;

const char * const meminfo_text[] = {
	"memfr",	/* available memory */
	"swpfr",
	"cache",
	"active",	/* (LRU) user pages */
	"inactive",
	"unevictable",
	"shmem",	/* kernel memory */
	"slab",
	"kernel_stack",
	"page_table",
	"gpu",		/* HW memory */
	"ion",
	"zram",		/* misc */
};

const char * const vmstat_partial_text[] = {
	"swpin",	/* vm events */
	"swpout",
	"majflt",
	"refault",	/* vm stats */
};

const char * const proc_text[] = {
	"pid",
	"score_adj",
	"rss",
	"rswp",
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

/* for kallsyms lookup */
#ifdef CONFIG_SWAP
static unsigned long (*total_swapcache_pages_addr)(void);
#else
#define total_swapcache_pages_addr()	0UL
#endif
static struct pglist_data *(*first_online_pgdat_addr)(void);
static struct zone *(*next_zone_addr)(struct zone *zone);

const char *
mmstat_trace_print_arrayset_seq(struct trace_seq *p,
		const void *buf, int count, int sets)
{
	const char *ret = trace_seq_buffer_ptr(p);
	const char *prefix = "";
	void *ptr = (void *)buf;
	size_t buf_len = count * sizeof(unsigned long);
	int entries = 0;

	while (ptr < buf + buf_len) {
		if (likely(*(long *)ptr >= 0))
			trace_seq_printf(p, "%s%lu", prefix,
					*(unsigned long *)ptr);
		else
			trace_seq_printf(p, "%s%ld", prefix,
					*(long *)ptr);

		prefix = ",";

		if (++entries == count)
			break;

		if (entries % sets == 0)
			prefix = "|";

		ptr += sizeof(unsigned long);
	}

	trace_seq_putc(p, 0);

	return ret;
}

size_t __weak ion_mm_heap_total_memory(void)
{
	return 0;
}

/*
 * Record basic memory information
 */
static void mmstat_trace_meminfo(void)
{
	unsigned long meminfo[NR_MEMINFO_ITEMS] = {0};
	size_t num_entries = 0;
	unsigned int gpuuse = 0;

	/* available memory */
	meminfo[num_entries++] = P2K(global_zone_page_state(NR_FREE_PAGES));
	meminfo[num_entries++] = P2K(atomic_long_read(&nr_swap_pages));
	meminfo[num_entries++] = P2K(global_node_page_state(NR_FILE_PAGES) -
			total_swapcache_pages_addr());

	/* user pages */
	meminfo[num_entries++] = P2K(global_node_page_state(NR_ACTIVE_ANON) +
			global_node_page_state(NR_ACTIVE_FILE));
	meminfo[num_entries++] = P2K(global_node_page_state(NR_INACTIVE_ANON) +
			global_node_page_state(NR_INACTIVE_FILE));
	meminfo[num_entries++] = P2K(global_node_page_state(NR_UNEVICTABLE));

	/* kernel memory usage */
	meminfo[num_entries++] = P2K(global_node_page_state(NR_SHMEM));
	meminfo[num_entries++] =
		P2K(global_node_page_state(NR_SLAB_UNRECLAIMABLE) +
				global_node_page_state(NR_SLAB_RECLAIMABLE));
	meminfo[num_entries++] = global_zone_page_state(NR_KERNEL_STACK_KB);
	meminfo[num_entries++] = P2K(global_zone_page_state(NR_PAGETABLE));

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
	/* HW memory */
	if (mtk_get_gpu_memory_usage(&gpuuse))
		gpuuse = B2K(gpuuse);
#endif
	meminfo[num_entries++] = gpuuse;
	meminfo[num_entries++] = B2K((unsigned long)ion_mm_heap_total_memory());

	/* misc */
#if IS_ENABLED(CONFIG_ZSMALLOC)
	meminfo[num_entries++] = P2K(global_zone_page_state(NR_ZSPAGES));
#else
	meminfo[num_entries++] = 0;
#endif

	trace_mmstat_trace_meminfo((unsigned long *)meminfo, num_entries,
			NR_MEMINFO_ITEMS);
}

/*
 * Entry for recording vmstat - record partial VM status
 */
static void mmstat_trace_vmstat(void)
{
	int cpu;
	unsigned long v[NR_VMSTAT_PARTIAL_ITEMS] = {0};
	unsigned int nr_vm_events = NR_VMSTAT_PARTIAL_ITEMS - 1;
	size_t num_entries = 0;

	for_each_online_cpu(cpu) {
		struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

		v[num_entries++ % nr_vm_events] +=
			this->event[PSWPIN];
		v[num_entries++ % nr_vm_events] +=
			this->event[PSWPOUT];
		v[num_entries++ % nr_vm_events] +=
			this->event[PGMAJFAULT];
	}

	/* workingset_refsult */
	v[NR_VMSTAT_PARTIAL_ITEMS - 1] =
		global_node_page_state(WORKINGSET_REFAULT);

	trace_mmstat_trace_vmstat((unsigned long *)v, NR_VMSTAT_PARTIAL_ITEMS,
			NR_VMSTAT_PARTIAL_ITEMS);
}

/*
 * Record buddy information
 */
static void mmstat_trace_buddyinfo(void)
{
	struct zone *zone;

	/* imitate for_each_populated_zone */
	for (zone = (first_online_pgdat_addr())->node_zones;
	     zone; zone = next_zone_addr(zone)) {
		if (populated_zone(zone)) {
			unsigned long flags;
			unsigned int order;
			unsigned long buddyinfo[MAX_ORDER + 1] = {0};

			buddyinfo[0] = zone_idx(zone);
			spin_lock_irqsave(&zone->lock, flags);
			for (order = 0; order < MAX_ORDER; ++order)
				buddyinfo[order + 1] =
					zone->free_area[order].nr_free;
			spin_unlock_irqrestore(&zone->lock, flags);

			trace_mmstat_trace_buddyinfo((unsigned long *)buddyinfo,
					MAX_ORDER + 1, MAX_ORDER + 1);
		}
	}
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
static void mmstat_trace_procinfo(void)
{
#define PROC_SET_SIZE	NR_PROC_ITEMS
#define PROC_ARRAY_SIZE	(PROC_SET_SIZE * 4)

	struct task_struct *p;
	long proc_array[PROC_ARRAY_SIZE] = {0};
	size_t num_entries = 0;

	rcu_read_lock();
	for_each_process(p) {
		pid_t pid;
		short oom_score_adj;
		unsigned long rss;
		unsigned long rswap;

		if (p->flags & PF_KTHREAD)
			continue;

		task_lock(p);
		if (!p->mm || !p->signal) {
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

		proc_array[num_entries++] = pid;
		proc_array[num_entries++] = oom_score_adj;
		proc_array[num_entries++] = P2K(rss);
		proc_array[num_entries++] = P2K(rswap);

		if (num_entries == PROC_ARRAY_SIZE) {
			trace_mmstat_trace_proc((unsigned long *)proc_array,
					num_entries, PROC_SET_SIZE);
			num_entries = 0;
		}
	}
	rcu_read_unlock();

	/* remaining entries */
	if (num_entries)
		trace_mmstat_trace_proc((unsigned long *)proc_array,
				num_entries, PROC_SET_SIZE);

#undef PROC_ARRAY_SIZE
#undef PROC_SET_SIZE
}

/*
 * Main entry of recording information
 */
static void trace_mmstat_entry(void)
{
	/* basic memory information */
	if (meminfo_switch)
		mmstat_trace_meminfo();

	/* vm status */
	if (vmstat_switch)
		mmstat_trace_vmstat();

	/* buddy information */
	if (buddyinfo_switch)
		mmstat_trace_buddyinfo();

	/* process information */
	if (proc_switch)
		mmstat_trace_procinfo();
}

module_param(min_adj, int, 0644);
module_param(max_adj, int, 0644);
module_param(limit_pid, int, 0644);

static int do_switch_handler(const char *val, const struct kernel_param *kp)
{
	const int ret = param_set_uint(val, kp);

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

	queue_delayed_work(system_unbound_wq, &mmstat_work, 0);

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
__MODULE_PARM_TYPE(timer_intval, "ulong");

static void mmstat_work_handler(struct work_struct *work)
{
	trace_mmstat_entry();
	queue_delayed_work(system_unbound_wq, &mmstat_work, timer_intval);
}

int mmstat_print_fmt(struct seq_file *m)
{
	int i;

	/* basic memory information */
	if (meminfo_switch) {
		seq_puts(m, "mmstat_trace_meminfo: ");
		seq_printf(m, "%s", meminfo_text[0]);
		for (i = 1; i < NR_MEMINFO_ITEMS; i++)
			seq_printf(m, ",%s", meminfo_text[i]);
		seq_putc(m, '\n');
	}

	/* vm status */
	if (vmstat_switch) {
		seq_puts(m, "mmstat_trace_vmstat: ");
		seq_printf(m, "%s", vmstat_partial_text[0]);
		for (i = 1; i < NR_VMSTAT_PARTIAL_ITEMS; i++)
			seq_printf(m, ",%s", vmstat_partial_text[i]);
		seq_putc(m, '\n');
	}

	/* buddy information */
	if (buddyinfo_switch) {
		struct zone *zone;
		int order;

		/* imitate for_each_populated_zone */
		for (zone = (first_online_pgdat_addr())->node_zones;
		     zone; zone = next_zone_addr(zone)) {
			if (populated_zone(zone)) {
				seq_puts(m, "mmstat_trace_buddyinfo: ");
				seq_printf(m, "%s", zone->name);
				for (order = 0; order < MAX_ORDER; ++order)
					seq_printf(m, ",%d", order);
				seq_putc(m, '\n');
			}
		}
	}

	/* process information */
	if (proc_switch) {
		seq_puts(m, "mmstat_trace_proc: ");
		seq_printf(m, "%s", proc_text[0]);
		for (i = 1; i < NR_PROC_ITEMS; i++)
			seq_printf(m, ",%s", proc_text[i]);
		seq_putc(m, '\n');
	}

	return 0;
}

static int mmstat_fmt_proc_show(struct seq_file *m, void *v)
{
	return mmstat_print_fmt(m);
}

static int mmstat_fmt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmstat_fmt_proc_show, NULL);
}

static const struct file_operations mmstat_fmt_proc_fops = {
	.open = mmstat_fmt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init trace_mmstat_init(void)
{
#ifdef CONFIG_SWAP
	if (total_swapcache_pages_addr == 0)
		total_swapcache_pages_addr =
			(void *)kallsyms_lookup_name("total_swapcache_pages");
#endif

	if (first_online_pgdat_addr == 0)
		first_online_pgdat_addr =
			(void *)kallsyms_lookup_name("first_online_pgdat");

	if (next_zone_addr == 0)
		next_zone_addr = (void *)kallsyms_lookup_name("next_zone");

	hungersize = totalram_pages * TRACE_HUNGER_PERCENTAGE / 100;

	debugfs_create_file("mmstat_fmt", 0444, NULL, NULL,
			&mmstat_fmt_proc_fops);

	INIT_DELAYED_WORK(&mmstat_work, mmstat_work_handler);
	queue_delayed_work(system_unbound_wq, &mmstat_work, timer_intval);

	return 0;
}

static void __exit trace_mmstat_exit(void)
{
	cancel_delayed_work_sync(&mmstat_work);
}

module_init(trace_mmstat_init);
module_exit(trace_mmstat_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek mmstat tracer");
MODULE_AUTHOR("MediaTek Inc.");
