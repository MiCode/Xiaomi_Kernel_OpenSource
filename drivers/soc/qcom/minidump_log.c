// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/cache.h>
#include <linux/freezer.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/thread_info.h>
#include <soc/qcom/minidump.h>
#include <soc/qcom/secure_buffer.h>
#include <asm/page.h>
#include <asm/memory.h>
#include <asm/sections.h>
#include <asm/stacktrace.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>
#include <linux/notifier.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/sched/task.h>
#include <linux/suspend.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
#include <linux/bits.h>
#include <linux/sched/prio.h>
#include <linux/seq_buf.h>

#include <asm/memory.h>

#include "../../../kernel/sched/sched.h"

#include <linux/kdebug.h>
#include <linux/thread_info.h>
#include <asm/ptrace.h>
#include <linux/uaccess.h>
#include <linux/percpu.h>

#include <linux/module.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#endif

#ifdef CONFIG_QCOM_DYN_MINIDUMP_STACK

#include <trace/events/sched.h>

#ifdef CONFIG_VMAP_STACK
#define STACK_NUM_PAGES (THREAD_SIZE / PAGE_SIZE)
#else
#define STACK_NUM_PAGES 1
#endif	/* !CONFIG_VMAP_STACK */

struct md_stack_cpu_data {
	int stack_mdidx[STACK_NUM_PAGES];
	struct md_region stack_mdr[STACK_NUM_PAGES];
} ____cacheline_aligned_in_smp;

static int md_current_stack_init __read_mostly;

static DEFINE_PER_CPU_SHARED_ALIGNED(struct md_stack_cpu_data, md_stack_data);

struct md_suspend_context_data {
	int task_mdno;
	int stack_mdidx[STACK_NUM_PAGES];
	struct md_region stack_mdr[STACK_NUM_PAGES];
	struct md_region task_mdr;
	bool init;
};

static struct md_suspend_context_data md_suspend_context;
#endif

static bool is_vmap_stack __read_mostly;

#ifdef CONFIG_QCOM_MINIDUMP_FTRACE
#define MD_FTRACE_BUF_SIZE	SZ_2M

static char *md_ftrace_buf_addr;
static size_t md_ftrace_buf_current;
#endif

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
/* Rnqueue information */
#define MD_RUNQUEUE_PAGES	8

static bool md_in_oops_handler;
static struct seq_buf *md_runq_seq_buf;
static md_align_offset;

/* CPU context information */
#ifdef CONFIG_QCOM_MINIDUMP_PANIC_CPU_CONTEXT
#define MD_CPU_CNTXT_PAGES	32

static int die_cpu = -1;
static struct seq_buf *md_cntxt_seq_buf;
#endif

/* Meminfo */
#define MD_MEMINFO_PAGES	1

struct seq_buf *md_meminfo_seq_buf;

/* Slabinfo */
#define MD_SLABINFO_PAGES	8

struct seq_buf *md_slabinfo_seq_buf;

#ifdef CONFIG_PAGE_OWNER
size_t md_pageowner_dump_size = SZ_2M;
char *md_pageowner_dump_addr;
#endif

#ifdef CONFIG_SLUB_DEBUG
size_t md_slabowner_dump_size = SZ_2M;
char *md_slabowner_dump_addr;
#endif

/* Modules information */
#ifdef CONFIG_MODULES
#define NUM_MD_MODULES	200

static struct list_head md_mod_list_head;

struct md_module_data {
	struct list_head entry;
	char name[MODULE_NAME_LEN];
	void *base;
	unsigned int size;
};

static struct seq_buf *md_mod_info_seq_buf;
static int mod_curr_count;
static DEFINE_SPINLOCK(md_modules_lock);
#endif	/* CONFIG_MODULES */
#endif

static void __init register_log_buf(void)
{
	char *log_bufp;
	uint32_t log_buf_len;
	struct md_region md_entry;

	log_bufp = log_buf_addr_get();
	log_buf_len = log_buf_len_get();

	if (!log_bufp || !log_buf_len) {
		pr_err("Unable to locate log_buf!\n");
		return;
	}
	/*Register logbuf to minidump, first idx would be from bss section */
	strlcpy(md_entry.name, "KLOGBUF", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t) log_bufp;
	md_entry.phys_addr = virt_to_phys(log_bufp);
	md_entry.size = log_buf_len;
	if (msm_minidump_add_region(&md_entry) < 0)
		pr_err("Failed to add logbuf in Minidump\n");
}

static int register_stack_entry(struct md_region *ksp_entry, u64 sp, u64 size)
{
	struct page *sp_page;
	int entry;

	ksp_entry->virt_addr = sp;
	ksp_entry->size = size;
	if (is_vmap_stack) {
		sp_page = vmalloc_to_page((const void *) sp);
		ksp_entry->phys_addr = page_to_phys(sp_page);
	} else {
		ksp_entry->phys_addr = virt_to_phys((uintptr_t *)sp);
	}

	entry = msm_minidump_add_region(ksp_entry);
	if (entry < 0)
		pr_err("Failed to add stack of entry %s in Minidump\n",
				ksp_entry->name);
	return entry;
}

static void __init register_kernel_sections(void)
{
	struct md_region ksec_entry;
	char *data_name = "KDATABSS";
	char *rodata_name = "KROAIDATA";
#ifdef CONFIG_SMP
	const size_t static_size = __per_cpu_end - __per_cpu_start;
	void __percpu *base = (void __percpu *)__per_cpu_start;
	unsigned int cpu;
#endif

	strlcpy(ksec_entry.name, data_name, sizeof(ksec_entry.name));
	ksec_entry.virt_addr = (uintptr_t)_sdata;
	ksec_entry.phys_addr = virt_to_phys(_sdata);
	ksec_entry.size = roundup((__bss_stop - _sdata), 4);
	if (msm_minidump_add_region(&ksec_entry) < 0)
		pr_err("Failed to add data section in Minidump\n");

	strlcpy(ksec_entry.name, rodata_name, sizeof(ksec_entry.name));
	ksec_entry.virt_addr = (uintptr_t)__start_ro_after_init;
	ksec_entry.phys_addr = virt_to_phys(__start_ro_after_init);
	ksec_entry.size = roundup((__end_ro_after_init - __start_ro_after_init), 4);
	if (msm_minidump_add_region(&ksec_entry) < 0)
		pr_err("Failed to add rodata section in Minidump\n");

#ifdef CONFIG_SMP
	/* Add percpu static sections */
	for_each_possible_cpu(cpu) {
		void *start = per_cpu_ptr(base, cpu);

		memset(&ksec_entry, 0, sizeof(ksec_entry));
		scnprintf(ksec_entry.name, sizeof(ksec_entry.name),
			"KSPERCPU%d", cpu);
		ksec_entry.virt_addr = (uintptr_t)start;
		ksec_entry.phys_addr = per_cpu_ptr_to_phys(start);
		ksec_entry.size = static_size;
		if (msm_minidump_add_region(&ksec_entry) < 0)
			pr_err("Failed to add percpu sections in Minidump\n");
	}
#endif
}

static inline bool in_stack_range(
		u64 sp, u64 base_addr, unsigned int stack_size)
{
	u64 min_addr = base_addr;
	u64 max_addr = base_addr + stack_size;

	return (min_addr <= sp && sp < max_addr);
}

static unsigned int calculate_copy_pages(u64 sp, struct vm_struct *stack_area)
{
	u64 tsk_stack_base = (u64) stack_area->addr;
	u64 offset;
	unsigned int stack_pages, copy_pages;

	if (in_stack_range(sp, tsk_stack_base, get_vm_area_size(stack_area))) {
		offset = sp - tsk_stack_base;
		stack_pages = get_vm_area_size(stack_area) / PAGE_SIZE;
		copy_pages = stack_pages - (offset / PAGE_SIZE);
	} else {
		copy_pages = 0;
	}
	return copy_pages;
}

void dump_stack_minidump(u64 sp)
{
	struct md_region ksp_entry, ktsk_entry;
	u32 cpu = smp_processor_id();
	struct vm_struct *stack_vm_area;
	unsigned int i, copy_pages;

	if (IS_ENABLED(CONFIG_QCOM_DYN_MINIDUMP_STACK))
		return;

	if (is_idle_task(current))
		return;

	is_vmap_stack = IS_ENABLED(CONFIG_VMAP_STACK);

	if (sp < MODULES_END || sp > -256UL)
		sp = current_stack_pointer;

	/*
	 * Since stacks are now allocated with vmalloc, the translation to
	 * physical address is not a simple linear transformation like it is
	 * for kernel logical addresses, since vmalloc creates a virtual
	 * mapping. Thus, virt_to_phys() should not be used in this context;
	 * instead the page table must be walked to acquire the physical
	 * address of one page of the stack.
	 */
	stack_vm_area = task_stack_vm_area(current);
	if (is_vmap_stack) {
		sp &= ~(PAGE_SIZE - 1);
		copy_pages = calculate_copy_pages(sp, stack_vm_area);
		for (i = 0; i < copy_pages; i++) {
			scnprintf(ksp_entry.name, sizeof(ksp_entry.name),
				  "KSTACK%d_%d", cpu, i);
			(void)register_stack_entry(&ksp_entry, sp, PAGE_SIZE);
			sp += PAGE_SIZE;
		}
	} else {
		sp &= ~(THREAD_SIZE - 1);
		scnprintf(ksp_entry.name, sizeof(ksp_entry.name), "KSTACK%d",
			  cpu);
		(void)register_stack_entry(&ksp_entry, sp, THREAD_SIZE);
	}

	scnprintf(ktsk_entry.name, sizeof(ktsk_entry.name), "KTASK%d", cpu);
	ktsk_entry.virt_addr = (u64)current;
	ktsk_entry.phys_addr = virt_to_phys((uintptr_t *)current);
	ktsk_entry.size = sizeof(struct task_struct);
	if (msm_minidump_add_region(&ktsk_entry) < 0)
		pr_err("Failed to add current task %d in Minidump\n", cpu);
}

#ifdef CONFIG_QCOM_DYN_MINIDUMP_STACK
static void update_stack_entry(struct md_region *ksp_entry, u64 sp,
			       int mdno)
{
	struct page *sp_page;

	ksp_entry->virt_addr = sp;
	if (likely(is_vmap_stack)) {
		sp_page = vmalloc_to_page((const void *) sp);
		ksp_entry->phys_addr = page_to_phys(sp_page);
	} else {
		ksp_entry->phys_addr = virt_to_phys((uintptr_t *)sp);
	}
	if (msm_minidump_update_region(mdno, ksp_entry) < 0) {
		pr_err_ratelimited(
			"Failed to update stack entry %s in minidump\n",
			ksp_entry->name);
	}
}

static void register_vmapped_stack(struct md_region *mdr, int *mdno,
				   u64 sp, char *name_str, bool update)
{
	int i;

	sp &= ~(PAGE_SIZE - 1);
	for (i = 0; i < STACK_NUM_PAGES; i++) {
		if (unlikely(!update)) {
			scnprintf(mdr->name, sizeof(mdr->name), "%s_%d",
					  name_str, i);
			*mdno = register_stack_entry(mdr, sp, PAGE_SIZE);
		} else {
			update_stack_entry(mdr, sp, *mdno);
		}
		sp += PAGE_SIZE;
		mdr++;
		mdno++;
	}
}

static void register_normal_stack(struct md_region *mdr, int *mdno,
				  u64 sp, char *name_str, bool update)
{
	sp &= ~(THREAD_SIZE - 1);
	if (unlikely(!update)) {
		scnprintf(mdr->name, sizeof(mdr->name), name_str);
		*mdno = register_stack_entry(mdr, sp, THREAD_SIZE);
	} else {
		update_stack_entry(mdr, sp, *mdno);
	}
}

static void update_md_stack(struct md_region *stack_mdr,
			    int *stack_mdno, u64 sp)
{
	unsigned int i;
	int *mdno;

	if (likely(is_vmap_stack)) {
		for (i = 0; i < STACK_NUM_PAGES; i++) {
			mdno = stack_mdno + i;
			if (unlikely(*mdno < 0))
				return;
		}
		register_vmapped_stack(stack_mdr, stack_mdno, sp, NULL, true);
	} else {
		if (unlikely(*stack_mdno < 0))
			return;
		register_normal_stack(stack_mdr, stack_mdno, sp, NULL, true);
	}
}

static void update_md_cpu_stack(u32 cpu, u64 sp)
{
	struct md_stack_cpu_data *md_stack_cpu_d = &per_cpu(md_stack_data, cpu);

	if (!md_current_stack_init)
		return;

	update_md_stack(md_stack_cpu_d->stack_mdr,
			md_stack_cpu_d->stack_mdidx, sp);
}

void md_current_stack_notifer(void *ignore, bool preempt,
		struct task_struct *prev, struct task_struct *next)
{
	u32 cpu = task_cpu(next);
	u64 sp = (u64)next->stack;

	if (is_idle_task(next))
		return;
	update_md_cpu_stack(cpu, sp);
}

void md_current_stack_ipi_handler(void *data)
{
	u32 cpu = smp_processor_id();
	struct vm_struct *stack_vm_area;
	u64 sp = current_stack_pointer;

	if (is_idle_task(current))
		return;
	if (likely(is_vmap_stack)) {
		stack_vm_area = task_stack_vm_area(current);
		sp = (u64)stack_vm_area->addr;
	}
	update_md_cpu_stack(cpu, sp);
}

static void update_md_current_task(struct md_region *mdr, int mdno)
{
	mdr->virt_addr = (u64)current;
	mdr->phys_addr = virt_to_phys((uintptr_t *)current);
	if (msm_minidump_update_region(mdno, mdr) < 0)
		pr_err("Failed to update %s current task in minidump\n",
			   mdr->name);
}

static void update_md_suspend_current_stack(void)
{
	u64 sp = current_stack_pointer;
	struct vm_struct *stack_vm_area;

	if (likely(is_vmap_stack)) {
		stack_vm_area = task_stack_vm_area(current);
		sp = (u64)stack_vm_area->addr;
	}
	update_md_stack(md_suspend_context.stack_mdr,
			md_suspend_context.stack_mdidx, sp);
}

static void update_md_suspend_current_task(void)
{
	if (unlikely(md_suspend_context.task_mdno < 0))
		return;
	update_md_current_task(&md_suspend_context.task_mdr,
			md_suspend_context.task_mdno);
}

static void update_md_suspend_currents(void)
{
	if (!md_suspend_context.init)
		return;
	update_md_suspend_current_stack();
	update_md_suspend_current_task();
}

static void register_current_stack(void)
{
	int cpu;
	u64 sp = current_stack_pointer;
	struct md_stack_cpu_data *md_stack_cpu_d;
	struct vm_struct *stack_vm_area;
	char name_str[MAX_NAME_LENGTH];

	/*
	 * Since stacks are now allocated with vmalloc, the translation to
	 * physical address is not a simple linear transformation like it is
	 * for kernel logical addresses, since vmalloc creates a virtual
	 * mapping. Thus, virt_to_phys() should not be used in this context;
	 * instead the page table must be walked to acquire the physical
	 * address of all pages of the stack.
	 */
	if (likely(is_vmap_stack)) {
		stack_vm_area = task_stack_vm_area(current);
		sp = (u64)stack_vm_area->addr;
	}
	for_each_possible_cpu(cpu) {
		/*
		 * Let's register dummies for now,
		 * once system up and running, let the cpu update its currents.
		 */
		md_stack_cpu_d = &per_cpu(md_stack_data, cpu);
		scnprintf(name_str, sizeof(name_str), "KSTACK%d", cpu);
		if (is_vmap_stack)
			register_vmapped_stack(md_stack_cpu_d->stack_mdr,
				md_stack_cpu_d->stack_mdidx, sp,
				name_str, false);
		else
			register_normal_stack(md_stack_cpu_d->stack_mdr,
				md_stack_cpu_d->stack_mdidx, sp,
				name_str, false);
	}

	register_trace_sched_switch(md_current_stack_notifer, NULL);
	md_current_stack_init = 1;
	smp_call_function(md_current_stack_ipi_handler, NULL, 1);
}

static void register_suspend_stack(void)
{
	char name_str[MAX_NAME_LENGTH];
	u64 sp = current_stack_pointer;
	struct vm_struct *stack_vm_area = task_stack_vm_area(current);

	scnprintf(name_str, sizeof(name_str), "KSUSPSTK");
	if (is_vmap_stack) {
		sp = (u64)stack_vm_area->addr;
		register_vmapped_stack(md_suspend_context.stack_mdr,
				md_suspend_context.stack_mdidx,
				sp, name_str, false);
	} else {
		register_normal_stack(md_suspend_context.stack_mdr,
			md_suspend_context.stack_mdidx,
			sp, name_str, false);
	}
}

static void register_current_task(struct md_region *mdr, int *mdno,
				  char *name_str)
{
	scnprintf(mdr->name, sizeof(mdr->name), name_str);
	mdr->virt_addr = (u64)current;
	mdr->phys_addr = virt_to_phys((uintptr_t *)current);
	mdr->size = sizeof(struct task_struct);
	*mdno = msm_minidump_add_region(mdr);
	if (*mdno < 0)
		pr_err("Failed to add current task %s in Minidump\n",
		       mdr->name);
}

static void register_suspend_current_task(void)
{
	char name_str[MAX_NAME_LENGTH];

	scnprintf(name_str, sizeof(name_str), "KSUSPTASK");
	register_current_task(&md_suspend_context.task_mdr,
			&md_suspend_context.task_mdno, name_str);
}

static int minidump_pm_notifier(struct notifier_block *nb,
				unsigned long event, void *unused)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		update_md_suspend_currents();
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block minidump_pm_nb = {
	.notifier_call = minidump_pm_notifier,
};

static void register_suspend_context(void)
{
	register_suspend_stack();
	register_suspend_current_task();
	register_pm_notifier(&minidump_pm_nb);
	md_suspend_context.init = true;
}
#endif

#ifdef CONFIG_ARM64
static void register_irq_stack(void)
{
	int cpu;
	unsigned int i;
	int irq_stack_pages_count;
	u64 irq_stack_base;
	struct md_region irq_sp_entry;
	u64 sp;

	for_each_possible_cpu(cpu) {
		irq_stack_base = (u64)per_cpu(irq_stack_ptr, cpu);
		if (is_vmap_stack) {
			irq_stack_pages_count = IRQ_STACK_SIZE / PAGE_SIZE;
			sp = irq_stack_base & ~(PAGE_SIZE - 1);
			for (i = 0; i < irq_stack_pages_count; i++) {
				scnprintf(irq_sp_entry.name,
					  sizeof(irq_sp_entry.name),
					  "KISTACK%d_%d", cpu, i);
				register_stack_entry(&irq_sp_entry, sp,
						     PAGE_SIZE);
				sp += PAGE_SIZE;
			}
		} else {
			sp = irq_stack_base;
			scnprintf(irq_sp_entry.name, sizeof(irq_sp_entry.name),
				  "KISTACK%d", cpu);
			register_stack_entry(&irq_sp_entry, sp, IRQ_STACK_SIZE);
		}
	}
}
#else
static inline void register_irq_stack(void) {}
#endif

#ifdef CONFIG_QCOM_MINIDUMP_FTRACE
void minidump_add_trace_event(char *buf, size_t size)
{
	char *addr;

	if (!READ_ONCE(md_ftrace_buf_addr) ||
	    (size > (size_t)MD_FTRACE_BUF_SIZE))
		return;

	if ((md_ftrace_buf_current + size) > (size_t)MD_FTRACE_BUF_SIZE)
		md_ftrace_buf_current = 0;
	addr = md_ftrace_buf_addr + md_ftrace_buf_current;
	memcpy(addr, buf, size);
	md_ftrace_buf_current += size;
}

static void md_register_trace_buf(void)
{
	struct md_region md_entry;
	void *buffer_start;

	buffer_start = kzalloc(MD_FTRACE_BUF_SIZE, GFP_KERNEL);

	if (!buffer_start)
		return;

	strlcpy(md_entry.name, "KFTRACE", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t)buffer_start;
	md_entry.phys_addr = virt_to_phys(buffer_start);
	md_entry.size = MD_FTRACE_BUF_SIZE;
	if (msm_minidump_add_region(&md_entry) < 0)
		pr_err("Failed to add ftrace buffer entry in Minidump\n");

	/* Complete registration before adding enteries */
	smp_mb();
	WRITE_ONCE(md_ftrace_buf_addr, buffer_start);
}
#endif

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP

static void md_dump_align(void)
{
	int tab_offset = md_align_offset;

	while (tab_offset--)
		seq_buf_printf(md_runq_seq_buf, " | ");
	seq_buf_printf(md_runq_seq_buf, " |--");
}

static void md_dump_task_info(struct task_struct *task, char *status,
			      struct task_struct *curr)
{
	struct sched_entity *se;

	md_dump_align();
	if (!task) {
		seq_buf_printf(md_runq_seq_buf, "%s : None(0)\n", status);
		return;
	}

	se = &task->se;
	if (task == curr) {
		seq_buf_printf(md_runq_seq_buf,
			       "[status: curr] pid: %d comm: %s preempt: %#x\n",
			       task_pid_nr(task), task->comm,
			       task->thread_info.preempt_count);
		return;
	}

	seq_buf_printf(md_runq_seq_buf,
		       "[status: %s] pid: %d tsk: %#lx comm: %s stack: %#lx",
		       status, task_pid_nr(task),
		       (unsigned long)task,
		       task->comm,
		       (unsigned long)task->stack);
	seq_buf_printf(md_runq_seq_buf,
		       " prio: %d aff: %*pb",
		       task->prio, cpumask_pr_args(&task->cpus_mask));
#ifdef CONFIG_SCHED_WALT
	seq_buf_printf(md_runq_seq_buf, " enq: %lu wake: %lu sleep: %lu",
		       task->wts.last_enqueued_ts, task->wts.last_wake_ts,
		       task->wts.last_sleep_ts);
#endif
	seq_buf_printf(md_runq_seq_buf,
		       " vrun: %lu arr: %lu sum_ex: %lu\n",
		       (unsigned long)se->vruntime,
		       (unsigned long)se->exec_start,
		       (unsigned long)se->sum_exec_runtime);
}

static void md_dump_cfs_rq(struct cfs_rq *cfs, struct task_struct *curr);

static void md_dump_cgroup_state(char *status, struct sched_entity *se_p,
				 struct task_struct *curr)
{
	struct task_struct *task;
	struct cfs_rq *my_q = NULL;
	unsigned int nr_running;

	if (!se_p) {
		md_dump_task_info(NULL, status, NULL);
		return;
	}
#ifdef CONFIG_FAIR_GROUP_SCHED
	my_q = se_p->my_q;
#endif
	if (!my_q) {
		task = container_of(se_p, struct task_struct, se);
		md_dump_task_info(task, status, curr);
		return;
	}
	nr_running = my_q->nr_running;
	md_dump_align();
	seq_buf_printf(md_runq_seq_buf, "%s: %d process is grouping\n",
				   status, nr_running);
	md_align_offset++;
	md_dump_cfs_rq(my_q, curr);
	md_align_offset--;
}

static void md_dump_cfs_node_func(struct rb_node *node,
				  struct task_struct *curr)
{
	struct sched_entity *se_p = container_of(node, struct sched_entity,
						 run_node);

	md_dump_cgroup_state("pend", se_p, curr);
}

static void md_rb_walk_cfs(struct rb_root_cached *rb_root_cached_p,
			   struct task_struct *curr)
{
	int max_walk = 200;	/* Bail out, in case of loop */
	struct rb_node *leftmost = rb_root_cached_p->rb_leftmost;
	struct rb_root *root = &rb_root_cached_p->rb_root;
	struct rb_node *rb_node = rb_first(root);

	if (!leftmost)
		return;
	while (rb_node && max_walk--) {
		md_dump_cfs_node_func(rb_node, curr);
		rb_node = rb_next(rb_node);
	}
}

static void md_dump_cfs_rq(struct cfs_rq *cfs, struct task_struct *curr)
{
	struct rb_root_cached *rb_root_cached_p = &cfs->tasks_timeline;

	md_dump_cgroup_state("curr", cfs->curr, curr);
	md_dump_cgroup_state("next", cfs->next, curr);
	md_dump_cgroup_state("last", cfs->last, curr);
	md_dump_cgroup_state("skip", cfs->skip, curr);
	md_rb_walk_cfs(rb_root_cached_p, curr);
}

static void md_dump_rt_rq(struct rt_rq  *rt_rq, struct task_struct *curr)
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
			md_dump_task_info(p, "pend", curr);
		}
		idx = find_next_bit(array->bitmap, MAX_RT_PRIO, idx + 1);
	}
}

static void md_dump_runqueues(void)
{
	int cpu;
	struct rq *rq;
	struct rt_rq  *rt;
	struct cfs_rq *cfs;

	if (!md_runq_seq_buf)
		return;

	for_each_possible_cpu(cpu) {
		rq = cpu_rq(cpu);
		rt = &rq->rt;
		cfs = &rq->cfs;
		seq_buf_printf(md_runq_seq_buf,
			       "CPU%d %d process is running\n",
			       cpu, rq->nr_running);
		md_dump_task_info(cpu_curr(cpu), "curr", NULL);
		seq_buf_printf(md_runq_seq_buf,
			       "CFS %d process is pending\n",
			       cfs->nr_running);
		md_dump_cfs_rq(cfs, cpu_curr(cpu));
		seq_buf_printf(md_runq_seq_buf,
			       "RT %d process is pending\n",
			       rt->rt_nr_running);
		md_dump_rt_rq(rt, cpu_curr(cpu));
		seq_buf_printf(md_runq_seq_buf, "\n");
	}
}

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_CPU_CONTEXT
/*
 * dump a block of kernel memory from around the given address.
 * Bulk of the code is lifted from arch/arm64/kernel/proccess.c.
 */
static void md_dump_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < PAGE_OFFSET || addr > -256UL)
		return;

	seq_buf_printf(md_cntxt_seq_buf, "\n%s: %#lx:\n", name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;


	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		seq_buf_printf(md_cntxt_seq_buf, "%04lx ",
			       (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32	data;

			if (__is_lm_address(p) &&
			    kern_addr_valid((unsigned long)p) &&
			    page_accessible(page_to_pfn(virt_to_page(p))) &&
			    !probe_kernel_address(p, data))
				seq_buf_printf(md_cntxt_seq_buf, " %08x",
						data);
			else
				seq_buf_printf(md_cntxt_seq_buf, " ********");
			++p;
		}
		seq_buf_printf(md_cntxt_seq_buf, "\n");
	}
}

static void md_reg_context_data(struct pt_regs *regs)
{
	mm_segment_t fs;
	unsigned int i;
	int nbytes = 128;

	if (user_mode(regs) ||  !regs->pc)
		return;

	fs = get_fs();
	set_fs(KERNEL_DS);
	md_dump_data(regs->pc - nbytes, nbytes * 2, "PC");
	md_dump_data(regs->regs[30] - nbytes, nbytes * 2, "LR");
	md_dump_data(regs->sp - nbytes, nbytes * 2, "SP");
	for (i = 0; i < 30; i++) {
		char name[4];

		snprintf(name, sizeof(name), "X%u", i);
		md_dump_data(regs->regs[i] - nbytes, nbytes * 2, name);
	}
	set_fs(fs);
}

static inline void md_dump_panic_regs(void)
{
	struct pt_regs regs;
	u64 tmp1, tmp2;

	/* Lifted from crash_setup_regs() */
	__asm__ __volatile__ (
		"stp	 x0,   x1, [%2, #16 *  0]\n"
		"stp	 x2,   x3, [%2, #16 *  1]\n"
		"stp	 x4,   x5, [%2, #16 *  2]\n"
		"stp	 x6,   x7, [%2, #16 *  3]\n"
		"stp	 x8,   x9, [%2, #16 *  4]\n"
		"stp	x10,  x11, [%2, #16 *  5]\n"
		"stp	x12,  x13, [%2, #16 *  6]\n"
		"stp	x14,  x15, [%2, #16 *  7]\n"
		"stp	x16,  x17, [%2, #16 *  8]\n"
		"stp	x18,  x19, [%2, #16 *  9]\n"
		"stp	x20,  x21, [%2, #16 * 10]\n"
		"stp	x22,  x23, [%2, #16 * 11]\n"
		"stp	x24,  x25, [%2, #16 * 12]\n"
		"stp	x26,  x27, [%2, #16 * 13]\n"
		"stp	x28,  x29, [%2, #16 * 14]\n"
		"mov	 %0,  sp\n"
		"stp	x30,  %0,  [%2, #16 * 15]\n"

		"/* faked current PSTATE */\n"
		"mrs	 %0, CurrentEL\n"
		"mrs	 %1, SPSEL\n"
		"orr	 %0, %0, %1\n"
		"mrs	 %1, DAIF\n"
		"orr	 %0, %0, %1\n"
		"mrs	 %1, NZCV\n"
		"orr	 %0, %0, %1\n"
		/* pc */
		"adr	 %1, 1f\n"
		"1:\n"
		"stp	 %1, %0,   [%2, #16 * 16]\n"
		: "=&r" (tmp1), "=&r" (tmp2)
		: "r" (&regs)
		: "memory"
		);

	seq_buf_printf(md_cntxt_seq_buf, "PANIC CPU : %d\n",
				   raw_smp_processor_id());
	md_reg_context_data(&regs);
}

static void md_dump_other_cpus_context(void)
{
	unsigned long ipi_stop_addr = kallsyms_lookup_name("regs_before_stop");
	int cpu;
	struct pt_regs *regs;

	for_each_possible_cpu(cpu) {
		regs = (struct pt_regs *)(ipi_stop_addr + per_cpu_offset(cpu));
		seq_buf_printf(md_cntxt_seq_buf, "\nSTOPPED CPU : %d\n", cpu);
		md_reg_context_data(regs);
	}
}

static int md_die_context_notify(struct notifier_block *self,
				 unsigned long val, void *data)
{
	struct die_args *args = (struct die_args *)data;

	if (md_in_oops_handler)
		return NOTIFY_DONE;
	md_in_oops_handler = true;
	if (!md_cntxt_seq_buf) {
		md_in_oops_handler = false;
		return NOTIFY_DONE;
	}
	die_cpu = raw_smp_processor_id();
	seq_buf_printf(md_cntxt_seq_buf, "\nDIE CPU : %d\n", die_cpu);
	md_reg_context_data(args->regs);
	md_in_oops_handler = false;
	return NOTIFY_DONE;
}

static struct notifier_block md_die_context_nb = {
	.notifier_call = md_die_context_notify,
	.priority = INT_MAX - 2, /* < msm watchdog die notifier */
};
#endif

#ifdef CONFIG_MODULES
static void md_dump_module_data(void)
{
	struct md_module_data *md_mod_data_p;

	if (!md_mod_info_seq_buf)
		return;
	seq_buf_printf(md_mod_info_seq_buf, "=== MODULE INFO ===\n");
	list_for_each_entry(md_mod_data_p, &md_mod_list_head, entry) {
		seq_buf_printf(md_mod_info_seq_buf,
			       "name: %s, base: %p size: %#x\n",
			       md_mod_data_p->name, md_mod_data_p->base,
			       md_mod_data_p->size);
	}
}
#endif

static int md_panic_handler(struct notifier_block *this,
			    unsigned long event, void *ptr)
{
	if (md_in_oops_handler)
		return NOTIFY_DONE;
	md_in_oops_handler = true;
#ifdef CONFIG_QCOM_MINIDUMP_PANIC_CPU_CONTEXT
	if (!md_cntxt_seq_buf)
		goto dump_rq;
	if (raw_smp_processor_id() != die_cpu)
		md_dump_panic_regs();
	md_dump_other_cpus_context();
dump_rq:
#endif
	md_dump_runqueues();
#ifdef CONFIG_MODULES
	md_dump_module_data();
#endif
	if (md_meminfo_seq_buf)
		md_dump_meminfo();

	if (md_slabinfo_seq_buf)
		md_dump_slabinfo();

#ifdef CONFIG_SLUB_DEBUG
	if (md_slabowner_dump_addr)
		md_dump_slabowner();
#endif

#ifdef CONFIG_PAGE_OWNER
	if (md_pageowner_dump_addr)
		md_dump_pageowner();
#endif
	md_in_oops_handler = false;
	return NOTIFY_DONE;
}

static struct notifier_block md_panic_blk = {
	.notifier_call = md_panic_handler,
	.priority = INT_MAX - 2, /* < msm watchdog panic notifier */
};

static int md_register_minidump_entry(char *name, u64 virt_addr,
				      u64 phys_addr, u64 size)
{
	struct md_region md_entry;
	int ret;

	strlcpy(md_entry.name, name, sizeof(md_entry.name));
	md_entry.virt_addr = virt_addr;
	md_entry.phys_addr = phys_addr;
	md_entry.size = size;
	ret = msm_minidump_add_region(&md_entry);
	if (ret < 0)
		pr_err("Failed to add %s entry in Minidump\n", name);
	return ret;
}

static int md_register_panic_entries(int num_pages, char *name,
				      struct seq_buf **global_buf)
{
	char *buf;
	struct seq_buf *seq_buf_p;
	int ret;

	buf = kzalloc(num_pages * PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -EINVAL;

	seq_buf_p = kzalloc(sizeof(*seq_buf_p), GFP_KERNEL);
	if (!seq_buf_p) {
		ret = -EINVAL;
		goto err_seq_buf;
	}

	ret = md_register_minidump_entry(name, (uintptr_t)buf,
					 virt_to_phys(buf),
					 num_pages * PAGE_SIZE);
	if (ret < 0)
		goto err_entry_reg;

	seq_buf_init(seq_buf_p, buf, num_pages * PAGE_SIZE);

	/* Complete registration before populating data */
	smp_mb();
	WRITE_ONCE(*global_buf, seq_buf_p);
	return 0;

err_entry_reg:
	kfree(seq_buf_p);
err_seq_buf:
	kfree(buf);
	return ret;
}

static bool md_register_memory_dump(int size, char *name)
{
	void *buffer_start;
	struct page *page;
	int ret;

	page  = cma_alloc(dev_get_cma_area(NULL), size >> PAGE_SHIFT,
			0, false);

	if (!page) {
		pr_err("Failed to allocate %s minidump, increase cma size\n",
			name);
		return false;
	}

	buffer_start = page_to_virt(page);
	ret = md_register_minidump_entry(name, (uintptr_t)buffer_start,
			virt_to_phys(buffer_start), size);
	if (ret < 0) {
		cma_release(dev_get_cma_area(NULL), page, size >> PAGE_SHIFT);
		return false;
	}

	/* Complete registration before adding enteries */
	smp_mb();

#ifdef CONFIG_PAGE_OWNER
	if (!strcmp(name, "PAGEOWNER"))
		WRITE_ONCE(md_pageowner_dump_addr, buffer_start);
#endif
#ifdef CONFIG_SLUB_DEBUG
	if (!strcmp(name, "SLABOWNER"))
		WRITE_ONCE(md_slabowner_dump_addr, buffer_start);
#endif
	return true;
}

static bool md_unregister_memory_dump(char *name)
{
	struct page *page;
	struct md_region *mdr;
	struct md_region md_entry;

	mdr = md_get_region(name);
	if (!mdr) {
		pr_err("minidump entry for %s not found\n", name);
		return false;
	}
	strlcpy(md_entry.name, mdr->name, sizeof(md_entry.name));
	md_entry.virt_addr = mdr->virt_addr;
	md_entry.phys_addr = mdr->phys_addr;
	md_entry.size = mdr->size;
	page = virt_to_page(mdr->virt_addr);

	if (msm_minidump_remove_region(&md_entry) < 0)
		return false;

	cma_release(dev_get_cma_area(NULL), page,
			(md_entry.size) >> PAGE_SHIFT);
	return true;
}

static void update_dump_size(char *name, size_t size,
		char **addr, size_t *dump_size)
{
	if ((*dump_size) == 0) {
		if (md_register_memory_dump(size * SZ_1M,
						name)) {
			*dump_size = size * SZ_1M;
			pr_info_ratelimited("%s Minidump set to %zd MB size\n",
					name, size);
		}
		return;
	}
	if (md_unregister_memory_dump(name)) {
		*addr = NULL;
		if (size == 0) {
			*dump_size = 0;
			pr_info_ratelimited("%s Minidump : disabled\n", name);
			return;
		}
		if (md_register_memory_dump(size * SZ_1M,
						name)) {
			*dump_size = size * SZ_1M;
			pr_info_ratelimited("%s Minidump : set to %zd MB\n",
					name, size);
		} else if (md_register_memory_dump(*dump_size,
							name)) {
			pr_info_ratelimited("%s Minidump : Fallback to %zd MB\n",
					name, (*dump_size) / SZ_1M);
		} else {
			pr_err_ratelimited("%s Minidump : disabled, Can't fallback to %zd MB,\n",
						name, (*dump_size) / SZ_1M);
			*dump_size = 0;
		}
	} else {
		pr_err_ratelimited("Failed to unregister %s Minidump\n", name);
	}
}

#ifdef CONFIG_PAGE_OWNER
static DEFINE_MUTEX(page_owner_dump_size_lock);

static ssize_t page_owner_dump_size_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset)
{
	unsigned long long  size;

	if (kstrtoull_from_user(ubuf, count, 0, &size)) {
		pr_err_ratelimited("Invalid format for size\n");
		return -EINVAL;
	}
	mutex_lock(&page_owner_dump_size_lock);
	update_dump_size("PAGEOWNER", size,
			&md_pageowner_dump_addr, &md_pageowner_dump_size);
	mutex_unlock(&page_owner_dump_size_lock);
	return count;
}

static ssize_t page_owner_dump_size_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *offset)
{
	char buf[100];

	snprintf(buf, sizeof(buf), "%llu MB\n",
			md_pageowner_dump_size / SZ_1M);
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations proc_page_owner_dump_size_ops = {
	.open	= simple_open,
	.write	= page_owner_dump_size_write,
	.read	= page_owner_dump_size_read,
};
#endif

#ifdef CONFIG_SLUB_DEBUG
static ssize_t slab_owner_dump_size_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset)
{
	unsigned long long  size;

	if (kstrtoull_from_user(ubuf, count, 0, &size)) {
		pr_err_ratelimited("Invalid format for size\n");
		return -EINVAL;
	}
	update_dump_size("SLABOWNER", size,
			&md_slabowner_dump_addr, &md_slabowner_dump_size);
	return count;
}

static ssize_t slab_owner_dump_size_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *offset)
{
	char buf[100];

	snprintf(buf, sizeof(buf), "%llu MB\n", md_slabowner_dump_size/SZ_1M);
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations proc_slab_owner_dump_size_ops = {
	.open	= simple_open,
	.write	= slab_owner_dump_size_write,
	.read	= slab_owner_dump_size_read,
};
#endif

static void md_register_panic_data(void)
{
	md_register_panic_entries(MD_RUNQUEUE_PAGES, "KRUNQUEUE",
				  &md_runq_seq_buf);
#ifdef CONFIG_QCOM_MINIDUMP_PANIC_CPU_CONTEXT
	md_register_panic_entries(MD_CPU_CNTXT_PAGES, "KCNTXT",
				  &md_cntxt_seq_buf);
#endif
	md_register_panic_entries(MD_MEMINFO_PAGES, "MEMINFO",
				  &md_meminfo_seq_buf);
	md_register_panic_entries(MD_SLABINFO_PAGES, "SLABINFO",
				  &md_slabinfo_seq_buf);
#ifdef CONFIG_PAGE_OWNER
	if (is_page_owner_enabled()) {
		md_register_memory_dump(md_pageowner_dump_size, "PAGEOWNER");
		debugfs_create_file("page_owner_dump_size_mb", 0400, NULL, NULL,
			    &proc_page_owner_dump_size_ops);
	}
#endif
#ifdef CONFIG_SLUB_DEBUG
	if (is_slub_debug_enabled()) {
		md_register_memory_dump(md_slabowner_dump_size, "SLABOWNER");
		debugfs_create_file("slab_owner_dump_size_mb", 0400, NULL, NULL,
			    &proc_slab_owner_dump_size_ops);
	}
#endif
}

#ifdef CONFIG_MODULES
static int md_module_notify(struct notifier_block *self,
			    unsigned long val, void *data)
{
	struct module *mod = data;
	struct md_module_data *md_mod_data_p;
	struct md_module_data *md_mod_data_p_next;

	spin_lock(&md_modules_lock);
	switch (val) {
	case MODULE_STATE_COMING:
		if (mod_curr_count >= NUM_MD_MODULES) {
			spin_unlock(&md_modules_lock);
			return 0;
		}

		md_mod_data_p = kzalloc(sizeof(*md_mod_data_p), GFP_ATOMIC);
		if (!md_mod_data_p) {
			spin_unlock(&md_modules_lock);
			return 0;
		}
		strlcpy(md_mod_data_p->name, mod->name,
			    sizeof(md_mod_data_p->name));
		md_mod_data_p->base = mod->core_layout.base;
		md_mod_data_p->size = mod->core_layout.size;
		list_add(&md_mod_data_p->entry, &md_mod_list_head);
		mod_curr_count++;
		break;
	case MODULE_STATE_GOING:
		list_for_each_entry_safe(md_mod_data_p, md_mod_data_p_next,
					 &md_mod_list_head, entry) {
			if (!strcmp(md_mod_data_p->name, mod->name)) {
				list_del(&md_mod_data_p->entry);
				kfree(md_mod_data_p);
				mod_curr_count--;
				break;
			}
		}
		break;
	}
	spin_unlock(&md_modules_lock);
	return 0;
}

static struct notifier_block md_module_nb = {
	.notifier_call = md_module_notify,
};

static void md_register_module_data(void)
{
	int ret;

	ret = register_module_notifier(&md_module_nb);
	if (ret) {
		pr_err("Failed to register minidump module notifier\n");
		return;
	}

	ret = md_register_panic_entries(1, "KMODULES",
					&md_mod_info_seq_buf);
	if (ret)
		unregister_module_notifier(&md_module_nb);
}
#endif	/* CONFIG_MODULES */
#endif	/* CONFIG_QCOM_MINIDUMP_PANIC_DUMP */

static int __init msm_minidump_log_init(void)
{
	register_kernel_sections();
	is_vmap_stack = IS_ENABLED(CONFIG_VMAP_STACK);
	register_irq_stack();
#ifdef CONFIG_QCOM_DYN_MINIDUMP_STACK
	register_current_stack();
	register_suspend_context();
#endif
	register_log_buf();
#ifdef CONFIG_QCOM_MINIDUMP_FTRACE
	md_register_trace_buf();
#endif
#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
#ifdef CONFIG_MODULES
	INIT_LIST_HEAD(&md_mod_list_head);
	md_register_module_data();
#endif
	md_register_panic_data();
	atomic_notifier_chain_register(&panic_notifier_list, &md_panic_blk);
#ifdef CONFIG_QCOM_MINIDUMP_PANIC_CPU_CONTEXT
	register_die_notifier(&md_die_context_nb);
#endif
#endif
	return 0;
}
late_initcall(msm_minidump_log_init);
