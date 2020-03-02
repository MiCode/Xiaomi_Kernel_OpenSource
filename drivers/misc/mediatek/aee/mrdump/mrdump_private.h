/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#if !defined(__MRDUMP_PRIVATE_H__)
#define __MRDUMP_PRIVATE_H__

#include <asm/cputype.h>
#include <asm/memory.h>
#include <asm/smp_plat.h>
#include <asm/cputype.h>
#include <asm-generic/sections.h>

extern int kernel_addr_valid(unsigned long addr);
#define mrdump_virt_addr_valid(kaddr) \
	kernel_addr_valid((unsigned long)kaddr)

static inline int get_HW_cpuid(void)
{
	u64 mpidr;
	u32 id;

	mpidr = read_cpuid_mpidr();
	id = get_logical_index(mpidr & MPIDR_HWID_BITMASK);

	return id;
}

struct pt_regs;

extern struct mrdump_rsvmem_block mrdump_sram_cb;
extern struct mrdump_control_block *mrdump_cblock;
extern const unsigned long kallsyms_addresses[] __weak;
extern const u8 kallsyms_names[] __weak;
extern const u8 kallsyms_token_table[] __weak;
extern const u16 kallsyms_token_index[] __weak;
extern const unsigned long kallsyms_markers[] __weak;
extern const unsigned long kallsyms_num_syms
__attribute__((weak, section(".rodata")));


int mrdump_hw_init(void);
void mrdump_cblock_init(void);
int mrdump_full_init(void);
int mrdump_wdt_init(void);

void mrdump_save_control_register(void *creg);

extern void dis_D_inner_flush_all(void);
extern void __inner_flush_dcache_all(void);
extern void mrdump_mini_add_entry(unsigned long addr, unsigned long size);

int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom,
				unsigned long top);

extern void aee_rr_rec_kaslr_offset(uint64_t offset);
#if defined(CONFIG_RANDOMIZE_BASE) && defined(CONFIG_ARM64)
static inline void show_kaslr(void)
{
	u64 const kaslr_offset = kimage_vaddr - KIMAGE_VADDR;

	pr_notice("Kernel Offset: 0x%llx from 0x%lx\n",
			kaslr_offset, KIMAGE_VADDR);
	pr_notice("PHYS_OFFSET: 0x%llx\n", PHYS_OFFSET);
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_kaslr_offset(kaslr_offset);
#endif
}
#else
static inline void show_kaslr(void)
{
	pr_notice("Kernel Offset: disabled\n");
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_kaslr_offset(0);
#endif
}
#endif

int in_fiq_handler(void);

void aee_disable_api(void);

extern void mrdump_mini_per_cpu_regs(int cpu, struct pt_regs *regs,
		struct task_struct *tsk);
extern void mrdump_mini_ke_cpu_regs(struct pt_regs *regs);
extern void mrdump_mini_add_misc(unsigned long addr, unsigned long size,
		unsigned long start, char *name);
extern int mrdump_task_info(unsigned char *buffer, size_t sz_buf);
extern int mrdump_modules_info(unsigned char *buffer, size_t sz_buf);

/* for WDT timeout case : dump timer/schedule/irq/softirq etc...
 * debug information
 */
#ifdef CONFIG_SCHED_DEBUG
extern void sysrq_sched_debug_show_at_AEE(void);
#endif
#ifdef CONFIG_MTK_WQ_DEBUG
extern void wq_debug_dump(void);
#endif

#if defined(__arm__)
static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	if (oldregs) {
		memcpy(newregs, oldregs, sizeof(*newregs));
	} else {
		__asm__ __volatile__ (
			"stmia	%[regs_base], {r0-r12}\n\t"
			"mov	%[_ARM_sp], sp\n\t"
			"str	lr, %[_ARM_lr]\n\t"
			"adr	%[_ARM_pc], 1f\n\t"
			"mrs	%[_ARM_cpsr], cpsr\n\t"
		"1:"
			: [_ARM_pc] "=r" (newregs->ARM_pc),
			  [_ARM_cpsr] "=r" (newregs->ARM_cpsr),
			  [_ARM_sp] "=r" (newregs->ARM_sp),
			  [_ARM_lr] "=o" (newregs->ARM_lr)
			: [regs_base] "r" (&newregs->ARM_r0)
			: "memory"
		);
	}
}
#endif

#endif /* __MRDUMP_PRIVATE_H__ */
