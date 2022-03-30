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

extern void *aee_log_buf_addr_get(void);

extern struct list_head *aee_get_modules(void);
extern void aee_show_regs(struct pt_regs *regs);
extern unsigned long aee_get_kallsyms_addresses(void);
extern unsigned long aee_get_kti_addresses(void);
extern unsigned long aee_get_kn_off(void);
extern unsigned long aee_get_kns_off(void);
extern unsigned long aee_get_km_off(void);
extern unsigned long aee_get_ktt_off(void);
extern unsigned long aee_get_kti_off(void);
extern void aee_reinit_die_lock(void);
#if IS_ENABLED(CONFIG_MODULES)
extern void init_ko_addr_list_late(void);
#endif
#ifdef MODULE
int mrdump_ka_init(void *vaddr);
extern void mrdump_mini_add_klog(void);
extern void mrdump_mini_add_kallsyms(void);
#endif
extern void sysrq_sched_debug_show_at_AEE(void);
#endif
