/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#if !defined(__MRDUMP_PRIVATE_H__)
#define __MRDUMP_PRIVATE_H__

#include <asm/cputype.h>
#include <asm/memory.h>
#include <asm/smp_plat.h>
#include <asm-generic/sections.h>

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mboot_params.h>
#endif

#include "mrdump_helper.h"

extern int kernel_addr_valid(unsigned long addr);
#define mrdump_virt_addr_valid(kaddr) \
	kernel_addr_valid((unsigned long)kaddr)

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

#ifdef MODULE
int mrdump_module_init_mboot_params(void);
#endif
int mrdump_hw_init(void);
void mrdump_cblock_init(void);
int mrdump_full_init(void);
int mrdump_wdt_init(void);

void mrdump_save_control_register(void *creg);

extern void mrdump_mini_add_entry(unsigned long addr, unsigned long size);

int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom,
				unsigned long top);

int in_fiq_handler(void);

extern void mrdump_mini_per_cpu_regs(int cpu, struct pt_regs *regs,
		struct task_struct *tsk);
extern void mrdump_mini_ke_cpu_regs(struct pt_regs *regs);
extern int mrdump_task_info(unsigned char *buffer, size_t sz_buf);

/* for WDT timeout case : dump timer/schedule/irq/softirq etc...
 * debug information
 */
#if IS_ENABLED(CONFIG_SCHED_DEBUG)
extern void sysrq_sched_debug_show_at_AEE(void);
#endif
#if IS_ENABLED(CONFIG_MTK_WQ_DEBUG)
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

__weak void dis_D_inner_flush_all(void)
{
	pr_notice("%s:weak function.\n", __func__);
}

#if IS_ENABLED(CONFIG_MEDIATEK_CACHE_API)
extern void dis_D_inner_flush_all(void);
#endif

#endif /* __MRDUMP_PRIVATE_H__ */
