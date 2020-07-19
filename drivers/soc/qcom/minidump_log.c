// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/cache.h>
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
#include <linux/sched/task.h>
#include <linux/vmalloc.h>

static bool is_vmap_stack __read_mostly;

#ifdef CONFIG_QCOM_DYN_MINIDUMP_STACK

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
	md_entry.id = MINIDUMP_DEFAULT_ID;
	if (msm_minidump_add_region(&md_entry) < 0)
		pr_err("Failed to add logbuf in Minidump\n");
}

static int register_stack_entry(struct md_region *ksp_entry, u64 sp, u64 size,
				 u32 cpu)
{
	struct page *sp_page;
	int entry;

	ksp_entry->id = MINIDUMP_DEFAULT_ID;
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
		pr_err("Failed to add stack of cpu %d in Minidump\n", cpu);
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
	ksec_entry.id = MINIDUMP_DEFAULT_ID;
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
		ksec_entry.id = MINIDUMP_DEFAULT_ID;
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
			(void)register_stack_entry(&ksp_entry, sp,
						   PAGE_SIZE, cpu);
			sp += PAGE_SIZE;
		}
	} else {
		sp &= ~(THREAD_SIZE - 1);
		scnprintf(ksp_entry.name, sizeof(ksp_entry.name), "KSTACK%d",
			  cpu);
		(void)register_stack_entry(&ksp_entry, sp, THREAD_SIZE, cpu);
	}

	scnprintf(ktsk_entry.name, sizeof(ktsk_entry.name), "KTASK%d", cpu);
	ktsk_entry.virt_addr = (u64)current;
	ktsk_entry.phys_addr = virt_to_phys((uintptr_t *)current);
	ktsk_entry.size = sizeof(struct task_struct);
	ktsk_entry.id = MINIDUMP_DEFAULT_ID;
	if (msm_minidump_add_region(&ktsk_entry) < 0)
		pr_err("Failed to add current task %d in Minidump\n", cpu);
}

#ifdef CONFIG_QCOM_DYN_MINIDUMP_STACK
static void update_stack_entry(struct md_region *ksp_entry, u64 sp,
			       int mdno, u32 cpu)
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
		pr_err("Failed to update cpu[%d] current stack in minidump\n",
		       cpu);
	}
}

static void register_vmapped_stack(struct md_stack_cpu_data *md_stack_cpu_d,
				   struct vm_struct *stack_area, u32 cpu,
				   bool update)
{
	u64 sp;
	u64 tsk_stack_base = (u64)stack_area->addr;
	struct md_region *mdr;
	int *mdno;
	int i;

	sp = tsk_stack_base & ~(PAGE_SIZE - 1);
	for (i = 0; i < STACK_NUM_PAGES; i++) {
		mdr = md_stack_cpu_d->stack_mdr + i;
		mdno = md_stack_cpu_d->stack_mdidx + i;
		if (unlikely(!update)) {
			scnprintf(mdr->name, sizeof(mdr->name),
				  "KSTACK%d_%d", cpu, i);
			*mdno = register_stack_entry(mdr, sp, PAGE_SIZE, cpu);
		} else {
			update_stack_entry(mdr, sp, *mdno, cpu);
		}
		sp += PAGE_SIZE;
	}
}

static void register_normal_stack(struct md_stack_cpu_data *md_stack_cpu_d,
				  u64 sp, u32 cpu, bool update)
{
	struct md_region *mdr;

	mdr = md_stack_cpu_d->stack_mdr;
	sp &= ~(THREAD_SIZE - 1);
	if (unlikely(!update)) {
		scnprintf(mdr->name, sizeof(mdr->name), "KSTACK%d", cpu);
		*md_stack_cpu_d->stack_mdidx = register_stack_entry(
						mdr, sp, THREAD_SIZE, cpu);
	} else {
		update_stack_entry(mdr, sp,
				   *md_stack_cpu_d->stack_mdidx, cpu);
	}
}

void update_md_current_stack(void *data)
{
	u32 cpu = smp_processor_id();
	unsigned int i;
	u64 sp = current_stack_pointer;
	struct md_stack_cpu_data *md_stack_cpu_d =
				&per_cpu(md_stack_data, cpu);
	int *mdno;
	struct vm_struct *stack_vm_area;

	if (is_idle_task(current) || !md_current_stack_init)
		return;

	if (likely(is_vmap_stack)) {
		for (i = 0; i < STACK_NUM_PAGES; i++) {
			mdno = md_stack_cpu_d->stack_mdidx + i;
			if (unlikely(*mdno < 0))
				return;
		}
		stack_vm_area = task_stack_vm_area(current);
		register_vmapped_stack(md_stack_cpu_d, stack_vm_area,
				       cpu, true);
	} else {
		if (unlikely(*md_stack_cpu_d->stack_mdidx < 0))
			return;
		register_normal_stack(md_stack_cpu_d, sp, cpu, true);
	}
}

static void register_current_stack(void)
{
	int cpu;
	u64 sp = current_stack_pointer;
	struct md_stack_cpu_data *md_stack_cpu_d;
	struct vm_struct *stack_vm_area;

	stack_vm_area = task_stack_vm_area(current);
	/*
	 * Since stacks are now allocated with vmalloc, the translation to
	 * physical address is not a simple linear transformation like it is
	 * for kernel logical addresses, since vmalloc creates a virtual
	 * mapping. Thus, virt_to_phys() should not be used in this context;
	 * instead the page table must be walked to acquire the physical
	 * address of all pages of the stack.
	 */
	for_each_possible_cpu(cpu) {
		/*
		 * Let's register dummies for now,
		 * once system up and running, let the cpu update its currents.
		 */
		md_stack_cpu_d = &per_cpu(md_stack_data, cpu);
		if (is_vmap_stack) {
			register_vmapped_stack(md_stack_cpu_d, stack_vm_area,
				       cpu, false);
		} else {
			register_normal_stack(md_stack_cpu_d, sp, cpu, false);
		}
	}

	md_current_stack_init = 1;
	/* Let online cpus update currents now */
	smp_call_function(update_md_current_stack, NULL, 1);
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
						     PAGE_SIZE, cpu);
				sp += PAGE_SIZE;
			}
		} else {
			sp = irq_stack_base;
			scnprintf(irq_sp_entry.name, sizeof(irq_sp_entry.name),
				  "KISTACK%d", cpu);
			register_stack_entry(&irq_sp_entry, sp, IRQ_STACK_SIZE,
					     cpu);
		}
	}
}
#else
static inline void register_irq_stack(void) {}
#endif

static int __init msm_minidump_log_init(void)
{
	register_kernel_sections();
	is_vmap_stack = IS_ENABLED(CONFIG_VMAP_STACK);
	register_irq_stack();
#ifdef CONFIG_QCOM_DYN_MINIDUMP_STACK
	register_current_stack();
#endif
	register_log_buf();
	return 0;
}
subsys_initcall(msm_minidump_log_init);
