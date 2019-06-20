/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#if !defined(AEE_COMMON_H)
#define AEE_COMMON_H
#include <linux/console.h>

#ifdef CONFIG_MTK_AEE_IPANIC
extern void mboot_params_write(struct console *console, const char *s,
				unsigned int count);
#endif

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

#endif				/* AEE_COMMON_H */
