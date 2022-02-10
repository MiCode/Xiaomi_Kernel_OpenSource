/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

extern void get_task_util(struct task_struct *p, unsigned long *util);

/* For heavy task detection */
extern void sched_update_nr_heavy_prod(int invoker,
	struct task_struct *p, int cpu, int heavy_nr_inc, bool ack_cap);
extern void overutil_thresh_chg_notify(void);
extern int get_overutil_stats(char *buf, int buf_size);
extern unsigned long get_cpu_orig_capacity(unsigned int cpu);

unsigned int get_overutil_threshold(int index);
