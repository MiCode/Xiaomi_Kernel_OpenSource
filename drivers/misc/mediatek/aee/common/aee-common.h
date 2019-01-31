/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#if !defined(AEE_COMMON_H)
#define AEE_COMMON_H
#include <linux/console.h>

int get_memory_size(void);

int in_fiq_handler(void);

int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom,
				unsigned long top);
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void ram_console_write(struct console *console, const char *s,
				unsigned int count);
#endif

#ifdef CONFIG_MTK_AEE_IPANIC
extern void aee_dumpnative(void);
#endif
#ifdef CONFIG_SCHED_DEBUG
extern void sysrq_sched_debug_show(void);
extern void sysrq_sched_debug_show_at_AEE(void);
#endif
extern int aee_rr_reboot_reason_show(struct seq_file *m, void *v);
extern int aee_rr_last_fiq_step(void);
extern void aee_rr_rec_exp_type(unsigned int type);
extern void mrdump_mini_per_cpu_regs(int cpu, struct pt_regs *regs,
			struct task_struct *tsk);
extern int debug_locks;
#ifdef WDT_DEBUG_VERBOSE
extern int dump_localtimer_info(char *buffer, int size);
extern int dump_idle_info(char *buffer, int size);
#endif
#ifdef CONFIG_SMP
extern void dump_log_idle(void);
extern void irq_raise_softirq(const struct cpumask *mask, unsigned int irq);
#endif

/* for test case only */
extern int no_zap_locks;

/* dedicated reboot flow for exception */
extern void aee_exception_reboot(void);

#endif				/* AEE_COMMON_H */
