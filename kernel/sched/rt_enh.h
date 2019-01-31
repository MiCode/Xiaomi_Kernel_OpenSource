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

extern struct list_head hmp_domains;
DECLARE_PER_CPU(struct hmp_domain *, hmp_cpu_domain);
/* Check if cpu is in slowest hmp_domain */
extern unsigned int hmp_cpu_is_slowest(int cpu);

extern int select_task_rq_rt_boost(struct task_struct *p, int cpu);
extern int
find_lowest_rq_in_hmp(struct task_struct *task, struct cpumask *lowest_mask);
extern int mt_post_schedule(struct rq *rq);

extern bool sched_rt_boost(void);
extern void move_task(struct task_struct *p, struct lb_env *env);
