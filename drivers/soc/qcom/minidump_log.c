// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/cache.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/thread_info.h>
#include <soc/qcom/minidump.h>
#include <asm/page.h>
#include <asm/memory.h>
#include <asm/sections.h>
#include <asm/stacktrace.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/sched/task.h>
#include <linux/suspend.h>
#include <linux/vmalloc.h>

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

static void __init register_log_buf(void)
{
	char **log_bufp;
	uint32_t *log_buf_lenp;
	struct md_region md_entry;

	log_bufp = (char **)kallsyms_lookup_name("log_buf");
	log_buf_lenp = (uint32_t *)kallsyms_lookup_name("log_buf_len");
	if (!log_bufp || !log_buf_lenp) {
		pr_err("Unable to find log_buf by kallsyms!\n");
		return;
	}
	/*Register logbuf to minidump, first idx would be from bss section */
	strlcpy(md_entry.name, "KLOGBUF", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t) (*log_bufp);
	md_entry.phys_addr = virt_to_phys(*log_bufp);
	md_entry.size = *log_buf_lenp;
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
	const size_t static_size = __per_cpu_end - __per_cpu_start;
	void __percpu *base = (void __percpu *)__per_cpu_start;
	unsigned int cpu;

	strlcpy(ksec_entry.name, data_name, sizeof(ksec_entry.name));
	ksec_entry.virt_addr = (uintptr_t)_sdata;
	ksec_entry.phys_addr = virt_to_phys(_sdata);
	ksec_entry.size = roundup((__bss_stop - _sdata), 4);
	if (msm_minidump_add_region(&ksec_entry) < 0)
		pr_err("Failed to add data section in Minidump\n");

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

	if (sp < KIMAGE_VADDR || sp > -256UL)
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

	if (is_idle_task(current) || !md_current_stack_init)
		return;

	update_md_stack(md_stack_cpu_d->stack_mdr,
			md_stack_cpu_d->stack_mdidx, sp);
}

void md_current_stack_notifer(void *ignore, bool preempt,
		struct task_struct *prev, struct task_struct *next)
{
	u32 cpu = task_cpu(next);
	u64 sp = (u64)next->stack;

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

	buffer_start = kmalloc(MD_FTRACE_BUF_SIZE, GFP_KERNEL);

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
	return 0;
}
late_initcall(msm_minidump_log_init);
