// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sysdump kernel stats for Unisoc SoCs
 *
 * Copyright (c) 2022, Unisoc, Inc.
 */

#define pr_fmt(fmt)  "unisoc-dump-info: " fmt

#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kmsg_dump.h>
#include <linux/panic_notifier.h>
#include <linux/sched/debug.h>
#include <linux/sched/task_stack.h>
#include <linux/spinlock.h>
#include <linux/stacktrace.h>
#include <linux/kconfig.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/mmzone.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/hugetlb.h>
#include <linux/percpu.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>

#ifdef CONFIG_CMA
#include <linux/cma.h>
#endif

#include <asm/stacktrace.h>
#include <asm/memory.h>
#include <asm/page.h>

#include <linux/android_debug_symbols.h>
#include <linux/tracepoint.h>
#if IS_ENABLED(CONFIG_SCHED_WALT)
#include <linux/unisoc_vd_def.h>
#endif
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/debug.h>
#include <trace/hooks/ftrace_dump.h>

#include "unisoc_dump_info.h"
#include "unisoc_sysdump.h"
#include "sysdump.h"
#include <linux/module.h>
#include <linux/sched.h>
#include "../../../kernel/sched/sched.h"


#ifdef CONFIG_UNISOC_MINIDUMP_FTRACE
#include <linux/ring_buffer.h>

#define FTRACE_BUF_SIZE		(SZ_8K * CONFIG_UNISOC_MINIDUMP_FTRACE_SIZE_KB)
#define FTRACE_BUF_SIZE_PER_CPU	(FTRACE_BUF_SIZE / 8)

static char *ftrace_buf_addr;
static size_t ftrace_buf_current;
static bool minidump_ftrace_in_oops;
static bool minidump_ftrace_dump = true;
#endif

static struct seq_buf *unisoc_task_seq_buf;
static struct seq_buf *unisoc_rq_seq_buf;
static struct seq_buf *unisoc_sr_seq_buf;
static struct seq_buf *unisoc_mem_seq_buf;

static DEFINE_RAW_SPINLOCK(stop_lock);
static DEFINE_RAW_SPINLOCK(dump_lock);

#ifdef CONFIG_ARM64
static unsigned long __percpu *irq_stack_symbol;
#endif

int minidump_add_section(const char *name, int size, struct seq_buf **save_buf)
{
	int ret;
	char *buf;
	struct seq_buf *seq_buf_temp;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	seq_buf_temp = kzalloc(sizeof(*seq_buf_temp), GFP_KERNEL);
	if (!seq_buf_temp) {
		ret = -ENOMEM;
		goto err_buf;
	}

	if (minidump_save_extend_information(name, __pa((unsigned long)buf), __pa((unsigned long)(buf+size))) != 0) {
		ret = -EINVAL;
		goto err_save;
	 }

	seq_buf_init(seq_buf_temp, buf, size);

	/* Complete init before populating data */
	smp_mb();
	WRITE_ONCE(*save_buf, seq_buf_temp);

	return 0;

err_save:
	kfree(seq_buf_temp);
err_buf:
	kfree(buf);

	return ret;
}

void minidump_release_section(const char *name, struct seq_buf *save_buf)
{
	if (!save_buf)
		return;

	kfree(save_buf->buffer);
	kfree(save_buf);
}
#if defined(CONFIG_ARM64)
static void minidump_add_irq_stack(void)
{
	int cpu;
	unsigned long irq_stack_base, sp;
	char name[MAX_NAME_LEN];
#ifdef CONFIG_VMAP_STACK
	int page_count;
	unsigned int i;
#endif

	irq_stack_symbol  = android_debug_per_cpu_symbol(ADS_IRQ_STACK_PTR);

	if (!irq_stack_symbol)
		return;

	for_each_possible_cpu(cpu) {
		irq_stack_base = (unsigned long) per_cpu_ptr(irq_stack_symbol, cpu);
		if (!irq_stack_base)
			return;
#ifdef CONFIG_VMAP_STACK
		page_count = IRQ_STACK_SIZE / PAGE_SIZE;
		sp = irq_stack_base & ~(PAGE_SIZE - 1);
		for (i = 0; i < page_count; i++) {
			struct page *sp_page;
			unsigned long phys_addr;

			scnprintf(name, MAX_NAME_LEN, "irqstack%d_%d", cpu, i);
			sp_page = vmalloc_to_page((const void *) sp);
			phys_addr = page_to_phys(sp_page);

			if (minidump_save_extend_information(name, phys_addr, phys_addr + PAGE_SIZE))
				return;

			sp += PAGE_SIZE;
		}
#else
		sp = irq_stack_base;
		scnprintf(name, MAX_NAME_LEN, "irqstack%d", cpu);
		minidump_save_extend_information(name, __pa(sp), __pa(sp + IRQ_STACK_SIZE));
#endif
	}
}
#else
static inline void minidump_add_irq_stack(void) {}
#endif
static int currstack_inited;

static void minidump_add_current_stack(void)
{
	char name[MAX_NAME_LEN];
	int cpu;
#ifdef CONFIG_VMAP_STACK
	int i, page_count;

	page_count = THREAD_SIZE / PAGE_SIZE;
	for_each_possible_cpu(cpu) {
		for (i = 0; i < page_count; i++) {
			scnprintf(name, MAX_NAME_LEN, "cpustack%d_%d", cpu, i);
			if (minidump_save_extend_information(name, 0, PAGE_SIZE))
				return;

		}
	}
#else
	for_each_possible_cpu(cpu) {
		scnprintf(name, MAX_NAME_LEN, "cpustack%d", cpu);
                if(minidump_save_extend_information(name, 0, THREAD_SIZE))
			return;
	}
#endif
	currstack_inited = 1;
}

void minidump_update_current_stack(int cpu, struct pt_regs *regs)
{
	unsigned long sp;
	char name[MAX_NAME_LEN];
#ifdef CONFIG_VMAP_STACK
	struct vm_struct *stack_vm_area;
	int i, page_count;

	if (!currstack_inited || user_mode(regs) || is_idle_task(current))
		return;

	stack_vm_area = task_stack_vm_area(current);
	sp = (unsigned long)stack_vm_area->addr;

	sp &= ~(PAGE_SIZE - 1);
	page_count = THREAD_SIZE / PAGE_SIZE;
	for (i = 0; i < page_count; i++) {
		struct page *sp_page;
		unsigned long phys_addr;

		scnprintf(name, MAX_NAME_LEN, "cpustack%d_%d", cpu, i);
		sp_page = vmalloc_to_page((const void *) sp);
		phys_addr = page_to_phys(sp_page);
		if (minidump_change_extend_information(name, phys_addr, phys_addr + PAGE_SIZE))
			return;
		sp += PAGE_SIZE;
	}
#else
	if (!currstack_inited || user_mode(regs) || is_idle_task(current))
		return;

	sp = (unsigned long)current->stack;
	scnprintf(name, MAX_NAME_LEN, "cpustack%d", cpu);
	minidump_change_extend_information(name, __pa(sp), __pa(sp + THREAD_SIZE));
#endif
}
EXPORT_SYMBOL_GPL(minidump_update_current_stack);

/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000000);
		return -nsec;
	}
	do_div(nsec, 1000000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000000);
}

static int align_offset;

static void dump_align(void)
{
	int tab_offset = align_offset;

	while (tab_offset--)
		SEQ_printf(unisoc_rq_seq_buf, " | ");
	SEQ_printf(unisoc_rq_seq_buf, " |--");
}

static void dump_task_info(struct task_struct *task, char *status,
			      struct task_struct *curr)
{
	struct sched_entity *se;
#if IS_ENABLED(CONFIG_SCHED_WALT)
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)task->android_vendor_data1;
#endif

	dump_align();
	if (!task) {
		SEQ_printf(unisoc_rq_seq_buf, "%s : None(0)\n", status);
		return;
	}

	se = &task->se;
	if (task == curr) {
		SEQ_printf(unisoc_rq_seq_buf, "[status: curr] pid: %d comm: %s preempt: %#llx\n",
			task_pid_nr(task), task->comm,
			task_thread_info(task)->preempt_count);
		return;
	}

	SEQ_printf(unisoc_rq_seq_buf, "[status: %s] pid: %d tsk: %#lx comm: %s stack: %#lx",
		status, task_pid_nr(task),
		(unsigned long)task,
		task->comm,
		(unsigned long)task->stack);
	SEQ_printf(unisoc_rq_seq_buf, " prio: %d aff: %*pb",
		       task->prio, cpumask_pr_args(&task->cpus_mask));
#if IS_ENABLED(CONFIG_SCHED_WALT)
	SEQ_printf(unisoc_rq_seq_buf, " enqueue: %llu", uni_tsk->last_enqueue_ts);
	SEQ_printf(unisoc_rq_seq_buf, " sleep: %llu", uni_tsk->last_sleep_ts);
	SEQ_printf(unisoc_rq_seq_buf, " wake: %llu", uni_tsk->last_wakeup_ts);
#endif
	SEQ_printf(unisoc_rq_seq_buf, " vrun: %llu exec_start: %llu sum_ex: %llu\n",
		se->vruntime, se->exec_start, se->sum_exec_runtime);
}

static void dump_cfs_rq(struct cfs_rq *cfs, struct task_struct *curr);

static void dump_cgroup_state(char *status, struct sched_entity *se_p,
				   struct task_struct *curr)
{
	struct task_struct *task;
	struct cfs_rq *my_q = NULL;
	unsigned int nr_running;

	if (!se_p) {
		dump_task_info(NULL, status, NULL);
		return;
	}
#ifdef CONFIG_FAIR_GROUP_SCHED
	my_q = se_p->my_q;
#endif
	if (!my_q) {
		task = container_of(se_p, struct task_struct, se);
		dump_task_info(task, status, curr);
		return;
	}
	nr_running = my_q->nr_running;
	dump_align();
	SEQ_printf(unisoc_rq_seq_buf, "%s: %d process is grouping\n", status, nr_running);
	align_offset++;
	dump_cfs_rq(my_q, curr);

	align_offset--;
}

static void dump_cfs_node_func(struct rb_node *node,
				    struct task_struct *curr)
{
	struct sched_entity *se_p = container_of(node, struct sched_entity,
						 run_node);

	dump_cgroup_state("pend", se_p, curr);
}

static void rb_walk_cfs(struct rb_root_cached *rb_root_cached_p,
			     struct task_struct *curr)
{
	int max_walk = 100;	/* Bail out, in case of loop */
	struct rb_node *leftmost = rb_root_cached_p->rb_leftmost;
	struct rb_root *root = &rb_root_cached_p->rb_root;
	struct rb_node *rb_node = rb_first(root);

	if (!leftmost)
		return;
	while (rb_node && max_walk--) {
		dump_cfs_node_func(rb_node, curr);
		rb_node = rb_next(rb_node);
	}
}

static void dump_cfs_rq(struct cfs_rq *cfs, struct task_struct *curr)
{
	struct rb_root_cached *rb_root_cached_p = &cfs->tasks_timeline;

	dump_cgroup_state("curr", cfs->curr, curr);
	dump_cgroup_state("next", cfs->next, curr);
	dump_cgroup_state("last", cfs->last, curr);
	dump_cgroup_state("skip", cfs->skip, curr);
	rb_walk_cfs(rb_root_cached_p, curr);
}

static void dump_rt_rq(struct rt_rq  *rt_rq, struct task_struct *curr)
{
	struct rt_prio_array *array = &rt_rq->active;
	struct sched_rt_entity *rt_se;
	int idx;

	/* Lifted most of the below code from dump_throttled_rt_tasks() */
	if (bitmap_empty(array->bitmap, MAX_RT_PRIO))
		return;

	idx = sched_find_first_bit(array->bitmap);
	while (idx < MAX_RT_PRIO) {
		list_for_each_entry(rt_se, array->queue + idx, run_list) {
			struct task_struct *p;

#ifdef CONFIG_RT_GROUP_SCHED
			if (rt_se->my_q)
				continue;
#endif

			p = container_of(rt_se, struct task_struct, rt);
			dump_task_info(p, "pend", curr);
		}
		idx = find_next_bit(array->bitmap, MAX_RT_PRIO, idx + 1);
	}
}

void unisoc_dump_runqueues(void)
{
	int cpu;
	struct rq *rq;
	struct rt_rq  *rt;
	struct cfs_rq *cfs;

	if (!unisoc_rq_seq_buf)
		return;

	for_each_possible_cpu(cpu) {
		rq = cpu_rq(cpu);
		rt = &rq->rt;
		cfs = &rq->cfs;
		SEQ_printf(unisoc_rq_seq_buf, "CPU%d %d process is running\n",
					cpu, rq->nr_running);
		dump_task_info(cpu_curr(cpu), "curr", NULL);
		SEQ_printf(unisoc_rq_seq_buf, " CFS %d process is pending\n", cfs->nr_running);
		dump_cfs_rq(cfs, cpu_curr(cpu));
		SEQ_printf(unisoc_rq_seq_buf, " RT %d process is pending\n", rt->rt_nr_running);
		dump_rt_rq(rt, cpu_curr(cpu));
		SEQ_printf(unisoc_rq_seq_buf, "\n");
	}
	flush_cache_all();
}
EXPORT_SYMBOL_GPL(unisoc_dump_runqueues);

static void show_val_kb_pf(struct seq_buf *m, const char *s, unsigned long num)
{
	seq_buf_printf(m, s, num);
}

static void show_val_kb(struct seq_buf *m, const char *s, unsigned long num)
{
	seq_buf_printf(m, "%s  %ldkB\n", s,
					num << (PAGE_SHIFT - 10));
}

static long nr_blockdev_pages_nolock(void)
{
	struct inode *inode;
	long ret = 0;

	list_for_each_entry(inode, &blockdev_superblock->s_inodes, i_sb_list)
		ret += inode->i_mapping->nrpages;

	return ret;
}

static void si_meminfo_nolock(struct sysinfo *val)
{
	val->totalram = totalram_pages();
	val->sharedram = global_node_page_state(NR_SHMEM);
	val->freeram = global_zone_page_state(NR_FREE_PAGES);
	val->bufferram = nr_blockdev_pages_nolock();
	val->totalhigh = totalhigh_pages();
	val->freehigh = nr_free_highpages();
	val->mem_unit = PAGE_SIZE;
}

static int meminfo_proc_show(struct seq_buf *m)
{
	struct sysinfo i;
	unsigned long committed;
	long cached;
	long available;
	unsigned long pages[NR_LRU_LISTS];
	unsigned long sreclaimable, sunreclaim;
	int lru;
	unsigned long *addr;

	si_meminfo_nolock(&i);
	si_swapinfo(&i);
	committed = vm_memory_committed();

	cached = global_node_page_state(NR_FILE_PAGES) -
			total_swapcache_pages() - i.bufferram;

	if (cached < 0)
		cached = 0;

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_node_page_state(NR_LRU_BASE + lru);

	available = si_mem_available();
	sreclaimable = global_node_page_state_pages(NR_SLAB_RECLAIMABLE_B);
	sunreclaim = global_node_page_state_pages(NR_SLAB_UNRECLAIMABLE_B);

	show_val_kb(m, "MmeTotal:		", i.totalram);
	show_val_kb(m, "MemFree:		", i.freeram);
	show_val_kb(m, "MemAvailable:	", available);
	show_val_kb(m, "Buffers:		", i.bufferram);
	show_val_kb(m, "Cached:			", cached);
	show_val_kb(m, "SwapCached:		", total_swapcache_pages());
	show_val_kb(m, "Active:			", pages[LRU_ACTIVE_ANON] +
							pages[LRU_ACTIVE_FILE]);
	show_val_kb(m, "Inactive:		", pages[LRU_INACTIVE_ANON] +
							pages[LRU_INACTIVE_FILE]);
	show_val_kb(m, "Active(anon):	", pages[LRU_ACTIVE_ANON]);
	show_val_kb(m, "Inactive(anon):	", pages[LRU_INACTIVE_ANON]);
	show_val_kb(m, "Active(file):	", pages[LRU_ACTIVE_FILE]);
	show_val_kb(m, "Inactive(file):	", pages[LRU_INACTIVE_FILE]);
	show_val_kb(m, "Unevictable:	", pages[LRU_UNEVICTABLE]);
	show_val_kb(m, "Mlocked:		", global_zone_page_state(NR_MLOCK));

#ifdef CONFIG_HIGHMEM
	show_val_kb(m, "HighTotal:		", i.totalhigh);
	show_val_kb(m, "HighFree:		", i.freehigh);
	show_val_kb(m, "LowTotal:		", i.totalram - i.totalhigh);
	show_val_kb(m, "LowFree:		", i.freeram - i.freehigh);
#endif

#ifndef CONFIG_MMU
	show_val_kb(m, "MmapCopy:		",
			(unsigned long)atomic_long_read(&mmap_pages_allocated));
#endif

	show_val_kb(m, "SwapTotal:		", i.totalswap);
	show_val_kb(m, "SwapFree:		", i.freeswap);
	show_val_kb(m, "Dirty:			",
			global_node_page_state(NR_FILE_DIRTY));
	show_val_kb(m, "Writeback:		",
			global_node_page_state(NR_WRITEBACK));
	show_val_kb(m, "AnonPages:		",
			global_node_page_state(NR_ANON_MAPPED));
	show_val_kb(m, "Mapped:			",
			global_node_page_state(NR_FILE_MAPPED));
	show_val_kb(m, "Shmem:			", i.sharedram);
	show_val_kb(m, "Kreclaimable:	", sreclaimable +
			global_node_page_state(NR_KERNEL_MISC_RECLAIMABLE));
	show_val_kb(m, "Slab:			", sreclaimable + sunreclaim);
	show_val_kb(m, "SReclaimable:	", sreclaimable);
	show_val_kb(m, "SUnreclaim:		", sunreclaim);
	show_val_kb_pf(m, "KernelStack:	%8lukB\n",
			global_node_page_state(NR_KERNEL_STACK_KB));
#ifdef CONFIG_SHADOW_CALL_STACK
	show_val_kb_pf(m, "ShadowCallStack:%8lukB\n",
			global_node_page_state(NR_KERNEL_SCS_KB));
#endif
	show_val_kb(m, "PateTables:		",
			global_node_page_state(NR_PAGETABLE));
	show_val_kb(m, "NFS_Unstable:	", 0);
	show_val_kb(m, "Bounce:			",
			global_zone_page_state(NR_BOUNCE));
	show_val_kb(m, "WritebackTmp:	",
			global_node_page_state(NR_WRITEBACK_TEMP));
	show_val_kb(m, "Committed_AS:	", committed);
	show_val_kb_pf(m, "VmallocTotal:	%8lukB\n",
			(unsigned long)VMALLOC_TOTAL >> 10);
	show_val_kb(m, "VmallocUsed:	", vmalloc_nr_pages());
	show_val_kb(m, "VmallocChunk:	", 0ul);
	show_val_kb(m, "Percpu:			", pcpu_nr_pages());

#ifdef CONFIG_MEMORY_FAILURE
	show_val_kb_pf(m, "HardwareCorrupted: %5lu kB\n",
			atomic_long_read(&num_poisoned_pages) << (PAGE_SHIFT - 10));
#endif

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	show_val_kb(m, "AnonHugePages:	",
			global_node_page_state(NR_ANON_THPS));
	show_val_kb(m, "ShmemHugePages:	",
			global_node_page_state(NR_SHMEM_THPS));
	show_val_kb(m, "ShmemPmdMapped:	",
			global_node_page_state(NR_SHMEM_PMDMAPPED));
	show_val_kb(m, "FileHugePages:	",
			global_node_page_state(NR_FILE_THPS));
	show_val_kb(m, "FilePmnMapped:	",
			global_node_page_state(NR_FILE_PMDMAPPED));
#endif

#ifdef CONFIG_CMA
	addr = (unsigned long *)android_debug_symbol(ADS_TOTAL_CMA);
	if (addr)
		show_val_kb(m, "CmaTotal:		", *addr);
	show_val_kb(m, "CmaFree:		",
			global_zone_page_state(NR_FREE_CMA_PAGES));
#endif
	return 0;
}

void unisoc_dump_mem_info(void)
{
	if (!unisoc_mem_seq_buf)
		return;

	SEQ_printf(unisoc_mem_seq_buf, "-----MemInfo-----\n\n");
	meminfo_proc_show(unisoc_mem_seq_buf);
	flush_cache_all();
}
EXPORT_SYMBOL_GPL(unisoc_dump_mem_info);

static void unisoc_print_task_stats(int cpu, struct rq *rq, struct task_struct *p)
{
	struct seq_buf *task_seq_buf;
#if IS_ENABLED(CONFIG_SCHED_WALT)
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
#endif

	task_seq_buf = unisoc_task_seq_buf;

	SEQ_printf(task_seq_buf, "  %d ", cpu);
	if (rq->curr == p)
		SEQ_printf(task_seq_buf, ">R");
	else
		SEQ_printf(task_seq_buf, " %c", task_state_to_char(p));

	SEQ_printf(task_seq_buf, " %15s %5d %5d %13lld  ",
				p->comm, task_pid_nr(p), p->prio,
				(long long)(p->nvcsw + p->nivcsw));

#ifdef CONFIG_SCHED_INFO
	SEQ_printf(task_seq_buf, "%6lld.%09ld  %6lld.%09ld  %6lld.%09ld",
				nsec_high(p->sched_info.last_arrival),
				nsec_low(p->sched_info.last_arrival),
				nsec_high(p->sched_info.last_queued),
				nsec_low(p->sched_info.last_queued),
				nsec_high(p->sched_info.run_delay),
				nsec_low(p->sched_info.run_delay));
#endif
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(p->se.sum_exec_runtime),
				nsec_low(p->se.sum_exec_runtime));
#if IS_ENABLED(CONFIG_SCHED_WALT)
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(uni_tsk->last_enqueue_ts),
				nsec_low(uni_tsk->last_enqueue_ts));
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(uni_tsk->last_sleep_ts),
				nsec_low(uni_tsk->last_sleep_ts));
	SEQ_printf(task_seq_buf, "   %6lld.%09ld",
				nsec_high(uni_tsk->last_wakeup_ts),
				nsec_low(uni_tsk->last_wakeup_ts));
#endif
	SEQ_printf(task_seq_buf, "\n");
}

void unisoc_dump_task_stats(void)
{
	struct task_struct *g, *p;
	int cpu;
	struct rq *rq;

	if (!unisoc_task_seq_buf)
		return;

	SEQ_printf(unisoc_task_seq_buf, "cpu  S       task_comm   PID  prio   num_of_exec");
#ifdef CONFIG_SCHED_INFO
	SEQ_printf(unisoc_task_seq_buf, "   last_arrival_ts    last_queued_ts   total_wait_time ");
#endif
	SEQ_printf(unisoc_task_seq_buf, "   total_exec_time");
#if IS_ENABLED(CONFIG_SCHED_WALT)
	SEQ_printf(unisoc_task_seq_buf, "    last_enqueue_ts");
	SEQ_printf(unisoc_task_seq_buf, "      last_sleep_ts");
	SEQ_printf(unisoc_task_seq_buf, "      last_wakeup_ts");
#endif
	SEQ_printf(unisoc_task_seq_buf, "\n-------------------------------------------------------------------"
		"-------------------------------------------------------------------------------------------\n");

	for_each_process_thread(g, p) {
		cpu = task_cpu(p);
		rq = cpu_rq(cpu);

		unisoc_print_task_stats(cpu, rq, p);
	}
	flush_cache_all();
}
EXPORT_SYMBOL_GPL(unisoc_dump_task_stats);

#ifdef CONFIG_ARM64
static void unisoc_dump_regs(struct pt_regs *regs)
{
	int i, top_reg;
	u64 lr, sp;

	if (compat_user_mode(regs)) {
		lr = regs->compat_lr;
		sp = regs->compat_sp;
		top_reg = 12;
	} else {
		lr = regs->regs[30];
		sp = regs->sp;
		top_reg = 29;
	}
	if (!user_mode(regs)) {
		SEQ_printf(unisoc_sr_seq_buf, "pc : %pS\n", (void *)regs->pc);
		SEQ_printf(unisoc_sr_seq_buf, "lr : %pS\n", (void *)ptrauth_strip_insn_pac(lr));
	} else {
		SEQ_printf(unisoc_sr_seq_buf, "pc : %016llx\n", regs->pc);
		SEQ_printf(unisoc_sr_seq_buf, "lr : %016llx\n", lr);
	}
	SEQ_printf(unisoc_sr_seq_buf, "sp : %016llx pstate : %08llx\n", sp, regs->pstate);

	if (system_uses_irq_prio_masking())
		SEQ_printf(unisoc_sr_seq_buf, "pmr_save: %08llx\n", regs->pmr_save);

	i = top_reg;
	while (i >= 0) {
		SEQ_printf(unisoc_sr_seq_buf, "x%-2d: %016llx ", i, regs->regs[i]);
		i--;

		if (i % 2 == 0) {
			SEQ_printf(unisoc_sr_seq_buf, "x%-2d: %016llx ", i, regs->regs[i]);
			i--;
		}

		SEQ_printf(unisoc_sr_seq_buf, "\n");
	}
	SEQ_printf(unisoc_sr_seq_buf, "\n");
}
#else
static const char *processor_modes[] __maybe_unused = {
"USER_26", "FIQ_26", "IRQ_26",  "SVC_26",  "UK4_26",  "UK5_26",  "UK6_26",  "UK7_26",
"UK8_26",  "UK9_26", "UK10_26", "UK11_26", "UK12_26", "UK13_26", "UK14_26", "UK15_26",
"USER_32", "FIQ_32", "IRQ_32",  "SVC_32",  "UK4_32",  "UK5_32",  "MON_32",  "ABT_32",
"UK8_32",  "UK9_32", "HYP_32",  "UND_32",  "UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static const char *isa_modes[] __maybe_unused = {
"ARM", "Thumb", "Jazelle", "ThumbEE"
};

static void unisoc_dump_regs(struct pt_regs *regs)
{
	unsigned long flags;
	char buf[64];
#ifndef CONFIG_CPU_V7M
	unsigned int domain;
#ifdef CONFIG_CPU_SW_DOMAIN_PAN
	/*
	 * Get the domain register for the parent context. In user
	 * mode, we don't save the DACR, so lets use what it should
	 * be. For other modes, we place it after the pt_regs struct.
	 */
	if (user_mode(regs)) {
		domain = DACR_UACCESS_ENABLE;
	} else {
		domain = to_svc_pt_regs(regs)->dacr;
	}
#else
	domain = get_domain();
#endif
#endif
	SEQ_printf(unisoc_sr_seq_buf, "pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n",
	       regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr);
	SEQ_printf(unisoc_sr_seq_buf, "sp : %08lx  ip : %08lx  fp : %08lx\n",
	       regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	SEQ_printf(unisoc_sr_seq_buf, "r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	SEQ_printf(unisoc_sr_seq_buf, "r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);
	SEQ_printf(unisoc_sr_seq_buf, "r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2,
		regs->ARM_r1, regs->ARM_r0);

	flags = regs->ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

#ifndef CONFIG_CPU_V7M
	{
		const char *segment;

		if ((domain & domain_mask(DOMAIN_USER)) ==
		    domain_val(DOMAIN_USER, DOMAIN_NOACCESS))
			segment = "none";
		else
			segment = "user";

		SEQ_printf(unisoc_sr_seq_buf, "Flags: %s  IRQs o%s  FIQs o%s  Mode %s  ISA %s  Segment %s\n",
			buf, interrupts_enabled(regs) ? "n" : "ff",
			fast_interrupts_enabled(regs) ? "n" : "ff",
			processor_modes[processor_mode(regs)],
			isa_modes[isa_mode(regs)], segment);
	}
#else
	SEQ_printf(unisoc_sr_seq_buf, "xPSR: %08lx\n", regs->ARM_cpsr);
#endif

#ifdef CONFIG_CPU_CP15
	{
		unsigned int ctrl;

		buf[0] = '\0';
#ifdef CONFIG_CPU_CP15_MMU
		{
			unsigned int transbase;

			asm("mrc p15, 0, %0, c2, c0\n\t"
			    : "=r" (transbase));
			snprintf(buf, sizeof(buf), "  Table: %08x  DAC: %08x",
				transbase, domain);
		}
#endif
		asm("mrc p15, 0, %0, c1, c0\n" : "=r" (ctrl));

		SEQ_printf(unisoc_sr_seq_buf, "Control: %08x%s\n", ctrl, buf);
	}
#endif
	SEQ_printf(unisoc_sr_seq_buf, "\n");
}
#endif

void unisoc_dump_stack_reg(int cpu, struct pt_regs *pregs)
{
	int i;
	struct stackframe frame;
	unsigned long sp;

	raw_spin_lock(&dump_lock);
	if (!unisoc_sr_seq_buf)
		goto unlock;

	SEQ_printf(unisoc_sr_seq_buf, "-----cpu%d stack info-----\n", cpu);

	if (user_mode(pregs)) {
		SEQ_printf(unisoc_sr_seq_buf, "-----cpu%d in user mode-----\n\n", cpu);
		goto unlock;
	}


#ifdef CONFIG_ARM64
	frame.fp = pregs->regs[29];
	frame.pc = pregs->pc;
	sp = pregs->sp;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = 0;
#endif
	bitmap_zero(frame.stacks_done, __NR_STACK_TYPES);
	frame.prev_fp = 0;
	frame.prev_type = STACK_TYPE_UNKNOWN;
#else
	frame.fp = pregs->ARM_fp;
	frame.sp = pregs->ARM_sp;
	frame.lr = pregs->ARM_lr;
	frame.pc = pregs->ARM_pc;
	sp = pregs->ARM_sp;
#endif
#ifdef CONFIG_VMAP_STACK
	if (!((sp >= VMALLOC_START) && (sp < VMALLOC_END))) {
		SEQ_printf(unisoc_sr_seq_buf, "%s sp out of kernel addr space %08lx\n", __func__, sp);
		goto unlock;
	}
#else
	if (!virt_addr_valid(sp)) {
		SEQ_printf(unisoc_sr_seq_buf, "invalid sp[%lx]\n", sp);
		goto unlock;
	}
#endif
	SEQ_printf(unisoc_sr_seq_buf, "callstack:\n");
	SEQ_printf(unisoc_sr_seq_buf, "[<%08lx>] (%pS)\n", frame.pc, (void *)frame.pc);

	for (i = 0; i < MAX_CALLBACK_LEVEL; i++) {

#ifdef CONFIG_ARM64
		void *ipc;

		ipc = return_address(i + 1);

		if (!ipc)
			break;

		SEQ_printf(unisoc_sr_seq_buf, "[<%08lx>] (%pS)\n", (unsigned long)ipc, ipc);
#else
		int urc;

		urc = unwind_frame(&frame);

		if (urc < 0)
			break;

		if (!virt_addr_valid(frame.pc)) {
			SEQ_printf(unisoc_sr_seq_buf, "i=%d, virt_addr_valid fail\n", i);
			break;
		}

		SEQ_printf(unisoc_sr_seq_buf, "[<%08lx>] (%pS)\n", frame.pc, (void *)frame.pc);
#endif
	}

	SEQ_printf(unisoc_sr_seq_buf, "\n-----cpu%d regs info-----\n", cpu);

	unisoc_dump_regs(pregs);

unlock:
	raw_spin_unlock(&dump_lock);
	flush_cache_all();
}
EXPORT_SYMBOL_GPL(unisoc_dump_stack_reg);

static inline void unisoc_dump_panic_regs(void)
{
	struct pt_regs regs;
	int cpu = raw_smp_processor_id();

	crash_setup_regs(&regs, NULL);

	unisoc_dump_stack_reg(cpu, &regs);
	minidump_update_current_stack(cpu, &regs);
}

static void trace_ipi_stop(void *data, struct pt_regs *regs)
{
	unsigned long flags;
	int cpu = raw_smp_processor_id();

	raw_spin_lock_irqsave(&stop_lock, flags);
	pr_info("CPU%u: stopping...\n", cpu);
	unisoc_dump_stack_reg(cpu, regs);
	minidump_update_current_stack(cpu, regs);
	raw_spin_unlock_irqrestore(&stop_lock, flags);
}

static void unisoc_free_stack_regs_stats(void)
{
	unregister_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
	minidump_release_section("stack_regs", unisoc_sr_seq_buf);
	unisoc_sr_seq_buf = NULL;
}

#ifdef CONFIG_UNISOC_MINIDUMP_FTRACE
static void minidump_add_trace_event(char *buf, size_t size)
{
	char *addr;

	if (!READ_ONCE(ftrace_buf_addr) || (size > (size_t)FTRACE_BUF_SIZE))
		return;

	if ((ftrace_buf_current + size) > (size_t)FTRACE_BUF_SIZE)
		ftrace_buf_current = 0;
	addr = ftrace_buf_addr + ftrace_buf_current;
	memcpy(addr, buf, size);
	ftrace_buf_current += size;
}

static void unisoc_ftrace_oops_enter(void *unused, bool *enter_check)
{
	if (!minidump_ftrace_in_oops) {
		minidump_ftrace_in_oops = true;
		*enter_check = false;
	} else {
		*enter_check = true;
	}
}

static void unisoc_ftrace_oops_exit(void *unused, bool *exit_check)
{
	minidump_ftrace_in_oops = false;
	flush_cache_all();
}

static void unisoc_update_trace_fmt(void *unused, bool *format_check)
{
	*format_check = false;
}

static void unisoc_buf_size_check(void *unused, unsigned long buffer_size,
			      bool *size_check)
{
	if (!minidump_ftrace_dump) {
		*size_check = true;
		return;
	}

	if (buffer_size > (FTRACE_BUF_SIZE_PER_CPU + PAGE_SIZE)) {
		pr_err("Skip md ftrace buffer dump for: %#lx\n", buffer_size);
		minidump_ftrace_dump = false;
		*size_check = true;
	}
}

static void unisoc_dump_trace_buf(void *unused, struct trace_seq *trace_buf, bool *dump_printk)
{
	*dump_printk = false;

	if (minidump_ftrace_in_oops && minidump_ftrace_dump)
		minidump_add_trace_event(trace_buf->buffer, trace_buf->seq.len);
}

static void unisoc_minidump_add_ftrace_section(void)
{
	void *buf;

	buf = kzalloc(FTRACE_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return;

	if (minidump_save_extend_information("kftrace", __pa((unsigned long)buf),
					    __pa((unsigned long)(buf + FTRACE_BUF_SIZE))) != 0) {
		kfree(buf);
		return;
	}

	register_trace_android_vh_ftrace_oops_enter(unisoc_ftrace_oops_enter, NULL);
	register_trace_android_vh_ftrace_oops_exit(unisoc_ftrace_oops_exit, NULL);
	register_trace_android_vh_ftrace_size_check(unisoc_buf_size_check, NULL);
	register_trace_android_vh_ftrace_format_check(unisoc_update_trace_fmt, NULL);
	register_trace_android_vh_ftrace_dump_buffer(unisoc_dump_trace_buf, NULL);

	/* Complete registration before adding enteries */
	smp_mb();
	WRITE_ONCE(ftrace_buf_addr, buf);
}

static void unisoc_minidump_release_ftrace_section(void)
{
	unregister_trace_android_vh_ftrace_oops_enter(unisoc_ftrace_oops_enter, NULL);
	unregister_trace_android_vh_ftrace_oops_exit(unisoc_ftrace_oops_exit, NULL);
	unregister_trace_android_vh_ftrace_size_check(unisoc_buf_size_check, NULL);
	unregister_trace_android_vh_ftrace_format_check(unisoc_update_trace_fmt, NULL);
	unregister_trace_android_vh_ftrace_dump_buffer(unisoc_dump_trace_buf, NULL);

	kfree(ftrace_buf_addr);
	ftrace_buf_addr = NULL;
}
#else
static void unisoc_ftrace_oops_enter(void *unused, bool *ftrace_check)
{
	*ftrace_check = true;
}

static void unisoc_dump_trace_buf(void *unused, struct trace_seq *trace_buf, bool *dump_printk)
{
	*dump_printk = false;
}

static void unisoc_minidump_add_ftrace_section(void)
{
	register_trace_android_vh_ftrace_oops_enter(unisoc_ftrace_oops_enter, NULL);
	register_trace_android_vh_ftrace_dump_buffer(unisoc_dump_trace_buf, NULL);
}

static void unisoc_minidump_release_ftrace_section(void)
{
	unregister_trace_android_vh_ftrace_oops_enter(unisoc_ftrace_oops_enter, NULL);
	unregister_trace_android_vh_ftrace_dump_buffer(unisoc_dump_trace_buf, NULL);
}

#endif
static int unisoc_kinfo_panic_event(struct notifier_block *self,
				  unsigned long val, void *reason)
{
	unisoc_dump_panic_regs();
	unisoc_dump_runqueues();
	unisoc_dump_task_stats();
	unisoc_dump_mem_info();
	return NOTIFY_DONE;
}

static struct notifier_block unisoc_kinfo_panic_event_nb = {
	.notifier_call	= unisoc_kinfo_panic_event,
	.priority	= INT_MAX,
};

static int __init unisoc_dumpinfo_init(void)
{

	pr_info("%s\n", __func__);
	minidump_add_section("task_stats", UNISOC_DUMP_TASK_SIZE, &unisoc_task_seq_buf);
	minidump_add_section("runqueue", UNISOC_DUMP_RQ_SIZE, &unisoc_rq_seq_buf);
	minidump_add_section("meminfo", UNISOC_DUMP_MEM_SIZE, &unisoc_mem_seq_buf);
	if (!minidump_add_section("stack_regs", UNISOC_DUMP_STACK_SIZE, &unisoc_sr_seq_buf))
		register_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
	minidump_add_current_stack();
	minidump_add_irq_stack();
	unisoc_minidump_add_ftrace_section();
	atomic_notifier_chain_register(&panic_notifier_list,
					&unisoc_kinfo_panic_event_nb);

	sprd_dump_io_init();

	return 0;
}

static void __exit unisoc_dumpinfo_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &unisoc_kinfo_panic_event_nb);
	minidump_release_section("task_stats", unisoc_task_seq_buf);
	unisoc_task_seq_buf = NULL;
	minidump_release_section("runqueue", unisoc_rq_seq_buf);
	unisoc_rq_seq_buf = NULL;
	minidump_release_section("meminfo", unisoc_mem_seq_buf);
	unisoc_mem_seq_buf = NULL;
	unisoc_free_stack_regs_stats();
	unisoc_minidump_release_ftrace_section();

	sprd_dump_io_exit();
}

late_initcall_sync(unisoc_dumpinfo_init);
module_exit(unisoc_dumpinfo_exit);

MODULE_IMPORT_NS(MINIDUMP);
MODULE_DESCRIPTION("unisoc dump last kernel information");
MODULE_LICENSE("GPL");
