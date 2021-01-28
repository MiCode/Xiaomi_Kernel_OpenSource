/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __aee_helper_h
#define __aee_helper_h

#include <asm/stacktrace.h>

/* for aee_aed.ko */
extern const char *aee_arch_vma_name(struct vm_area_struct *vma);

/* for mrdump.ko */
extern unsigned long aee_get_stext(void);
extern unsigned long aee_get_etext(void);
extern unsigned long aee_get_text(void);
extern unsigned long aee_get_sdata(void);
extern unsigned long aee_get_edata(void);
#if defined(CONFIG_ARM64)
extern unsigned long aee_get_kimage_vaddr(void);
#endif

#ifdef CONFIG_ARM64
extern int aee_unwind_frame(struct task_struct *tsk, struct stackframe *frame);
#else
extern int aee_unwind_frame(struct stackframe *frame);
#endif
extern u32 aee_log_buf_len_get(void);
extern char *aee_log_buf_addr_get(void);
extern phys_addr_t aee_memblock_start_of_DRAM(void);
extern phys_addr_t aee_memblock_end_of_DRAM(void);
extern unsigned long aee_get_swapper_pg_dir(void);
#ifdef CONFIG_SYSFS
extern struct kset *aee_get_module_kset(void);
#endif
#ifdef __aarch64__
extern bool aee_on_irq_stack(unsigned long sp, struct stack_info *info);
#endif
extern void aee_print_modules(void);
extern int aee_save_modules(char *mbuf, int mbufsize);
extern void aee_show_regs(struct pt_regs *regs);
extern pgd_t *aee_pgd_offset_k(unsigned long addr);
extern unsigned long aee_cpu_rq(int cpu);
extern struct task_struct *aee_cpu_curr(int cpu);
extern int get_HW_cpuid(void);
extern unsigned long aee_get_kallsyms_addresses(void);
extern void aee__flush_dcache_area(void *addr, size_t len);
extern void aee_zap_locks(void);
#endif
