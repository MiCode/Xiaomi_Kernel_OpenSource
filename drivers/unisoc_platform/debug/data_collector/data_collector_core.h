/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Unisoc Inc.
 */

#ifndef __DATA_COLLECTOR_CORE_H__
#define __DATA_COLLECTOR_CORE_H__


#define ONE_SEC_IN_NS	1000000000

extern void __init vmscan_statistic_init(void);
extern int __init vmscan_trace_register(void);
extern void __init unregister_vmscan_trace_at_init(void);

extern void __init sched_statistic_data_init(void);
extern int __init sched_trace_register(void);

extern int simple_open(struct inode *inode, struct file *file);
extern loff_t default_llseek(struct file *file, loff_t offset, int whence);

extern void period_mem_stat_check(void);

#endif // __DATA_COLLECTOR_CORE_H__
