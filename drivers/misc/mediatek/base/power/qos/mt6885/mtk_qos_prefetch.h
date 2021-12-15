/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_QOS_PREFETCH_H__
#define __MTK_QOS_PREFETCH_H__

#define QOS_CPUS_NR 3

extern unsigned int qos_cpu_opp_bound[QOS_CPUS_NR];
extern unsigned int qos_cpu_power_ratio_up;
extern unsigned int qos_cpu_power_ratio_dn;

extern unsigned int prefetch_val;

#ifdef CONFIG_MTK_QOS_FRAMEWORK
extern int register_prefetch_notifier(struct notifier_block *nb);
extern int unregister_prefetch_notifier(struct notifier_block *nb);
extern int prefetch_notifier_call_chain(unsigned long val, void *v);
extern int is_qos_prefetch_enabled(void);
extern void qos_prefetch_enable(int enable);
extern void qos_prefetch_force(int force);
extern int is_qos_prefetch_force(void);
extern ssize_t qos_prefetch_setting_get(char *buf);
extern ssize_t qos_prefetch_setting_set(const char *buf, size_t count);
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
__weak ssize_t qos_prefetch_setting_get(char *buf) { return 0; }
__weak ssize_t qos_prefetch_setting_set(const char *buf, size_t count)
{ return 0; }
__weak int is_qos_prefetch_log_enabled(void) { return 0; }
__weak void qos_prefetch_log_enable(int enable) { }
__weak unsigned int get_qos_prefetch_count(void) { return 0; }
__weak unsigned int *get_qos_prefetch_buf(void) { return NULL; }
__weak void qos_prefetch_update_all(void) { }
__weak void qos_prefetch_init(void) { }
#endif

#endif
