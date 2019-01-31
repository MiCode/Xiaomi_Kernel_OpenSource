/*
 * Copyright (C) 2017 MediaTek Inc.
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

#define CPU_LOAD_AVG_DEFAULT_MS 20
#define CPU_LOAD_AVG_TOLERANCE 2

struct rq_data {
	unsigned int rq_avg;
	unsigned long rq_poll_jiffies;
	unsigned long def_timer_jiffies;
	unsigned long rq_poll_last_jiffy;
	unsigned long rq_poll_total_jiffies;
	unsigned long def_timer_last_jiffy;
	unsigned int def_interval;
	unsigned int hotplug_disabled;
	int64_t def_start_time;
	struct attribute_group *attr_group;
	struct kobject *kobj;
	struct work_struct def_timer_work;
	int init;
};

extern spinlock_t rq_lock;
extern struct rq_data rq_info;
extern struct workqueue_struct *rq_wq;

/* For heavy task detection */
extern int sched_get_nr_heavy_running_avg(int cid, int *avg);
extern void sched_update_nr_heavy_prod(int invoker,
	struct task_struct *p, int cpu, int heavy_nr_inc, bool ack_cap);
extern int reset_heavy_task_stats(int cpu);
extern int is_ack_curcap(int cpu);
extern int is_heavy_task(struct task_struct *p);
/* need to invoke if any threshold of htasks changed */
extern void heavy_thresh_chg_notify(void);
/* For over-utilized task tracking */
extern void overutil_thresh_chg_notify(void);
extern int get_overutil_stats(char *buf, int buf_size);
extern unsigned long get_cpu_orig_capacity(unsigned int cpu);
extern int get_overutil_threshold(void);
#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
extern bool is_game_mode;
#endif
extern int show_btask(char *buf, int buf_size);
extern void cal_cpu_load(int cpu);
extern unsigned int sched_get_cpu_load(int cpu);
