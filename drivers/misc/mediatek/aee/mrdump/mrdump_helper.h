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

extern u32 aee_log_buf_len_get(void);
extern char *aee_log_buf_addr_get(void);
extern phys_addr_t aee_memblock_start_of_DRAM(void);
extern phys_addr_t aee_memblock_end_of_DRAM(void);
#ifdef CONFIG_SYSFS
extern struct kset *aee_get_module_kset(void);
#endif
extern struct list_head *aee_get_modules(void);
extern void aee_show_regs(struct pt_regs *regs);
extern pgd_t *aee_pgd_offset_k(unsigned long addr);
extern unsigned long aee_get_kallsyms_addresses(void);
extern unsigned long aee_get_kti_addresses(void);
extern void aee_zap_locks(void);
extern void aee_reinit_die_lock(void);
#ifdef MODULE
int mrdump_ka_init(void);
#endif
extern void sysrq_sched_debug_show_at_AEE(void);
#endif
