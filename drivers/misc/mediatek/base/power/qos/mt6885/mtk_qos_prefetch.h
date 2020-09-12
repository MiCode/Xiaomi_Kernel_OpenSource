/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MTK_QOS_PREFETCH_H__
#define __MTK_QOS_PREFETCH_H__

extern unsigned int prefetch_val;

#ifdef CONFIG_MTK_QOS_FRAMEWORK
extern int register_prefetch_notifier(struct notifier_block *nb);
extern int unregister_prefetch_notifier(struct notifier_block *nb);
extern int prefetch_notifier_call_chain(unsigned long val, void *v);
extern int is_qos_prefetch_enabled(void);
extern void qos_prefetch_enable(int enable);
extern void qos_prefetch_force(int force);
extern int is_qos_prefetch_force(void);
extern int is_qos_prefetch_log_enabled(void);
extern void qos_prefetch_log_enable(int enable);
extern unsigned int get_qos_prefetch_count(void);
extern unsigned int *get_qos_prefetch_buf(void);
extern void qos_prefetch_update_all(void);
extern void qos_prefetch_init(void);
#else
__weak int register_prefetch_notifier(struct notifier_block *nb) { return 0; }
__weak int unregister_prefetch_notifier(struct notifier_block *nb) { return 0; }
__weak int prefetch_notifier_call_chain(unsigned long val, void *v)
{ return 0; }
__weak int is_qos_prefetch_enabled(void) { return 0; }
__weak void qos_prefetch_enable(int enable) { }
__weak void qos_prefetch_force(int force) { }
__weak int is_qos_prefetch_force(void) { return -1; }
__weak int is_qos_prefetch_log_enabled(void) { return 0; }
__weak void qos_prefetch_log_enable(int enable) { }
__weak unsigned int get_qos_prefetch_count(void) { return 0; }
__weak unsigned int *get_qos_prefetch_buf(void) { return NULL; }
__weak void qos_prefetch_update_all(void) { }
__weak void qos_prefetch_init(void) { }
#endif

#endif
