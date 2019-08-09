/*
 * Copyright (C) 2016 MediaTek Inc.
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

/* sched: add rt exec info*/
DEFINE_PER_CPU(u64, old_rt_time);
DEFINE_PER_CPU(u64, init_rt_time);
DEFINE_PER_CPU(u64, rt_period_time);
DEFINE_PER_CPU(u64, rt_throttling_start);
DEFINE_PER_CPU(u64, exec_delta_time);
DEFINE_PER_CPU(u64, clock_task);
DEFINE_PER_CPU(u64, update_curr_exec_start);
DEFINE_PER_CPU(u64, pick_exec_start);
DEFINE_PER_CPU(u64, sched_pick_exec_start);
DEFINE_PER_CPU(u64, set_curr_exec_start);
DEFINE_PER_CPU(u64, sched_set_curr_exec_start);
DEFINE_PER_CPU(u64, update_exec_start);
DEFINE_PER_CPU(u64, sched_update_exec_start);
DEFINE_PER_CPU(struct task_struct, exec_task);

/* sched: print __disable_runtime unthrottled */
static inline void print_disable_runtime_unthrottle(struct rt_rq *rt_rq)
{
#ifdef CONFIG_RT_GROUP_SCHED
	struct rq *rq = rt_rq->rq;
#else
	struct rq *rq = container_of(rt_rq, struct rq, rt);
#endif

	rt_rq->rt_throttled = 0;
	printk_deferred("[name:rt&]sched: disable_runtime: RT throttling inactivated cpu=%d\n",
			cpu_of(rq));
	printk_deferred("[name:rt&]sched: cpu=%d, rt_time[%llu] rt_throttled=%d, rt_runtime[%llu]\n",
			cpu_of(rq),
			rt_rq->rt_time,
			rt_rq->rt_throttled,
			rt_rq->rt_runtime);
}

/* sched: print throttle info */
static inline void print_rt_throttle_info(int cpu, struct rt_rq *rt_rq,
					u64 runtime_pre, u64 runtime)
{
	/* sched: print throttle*/
	printk_deferred("[name:rt&]sched: initial rt_time %llu, start at %llu\n",
			per_cpu(init_rt_time, cpu),
			per_cpu(rt_period_time, cpu));
	printk_deferred("[name:rt&]sched: cpu=%d rt_time %llu <-> runtime[%llu -> %llu]",
			cpu, rt_rq->rt_time, runtime_pre, runtime);
	printk_deferred("exec_task[%d: %s] prio:%d exec_delta[%llu] clock[%llu] exec_start[%llu]\n",
			per_cpu(exec_task, cpu).pid,
			per_cpu(exec_task, cpu).comm,
			per_cpu(exec_task, cpu).prio,
			per_cpu(exec_delta_time, cpu),
			per_cpu(clock_task, cpu),
			per_cpu(update_exec_start, cpu));
	printk_deferred("[name:rt&]sched: update[%llu, %llu] pick[%llu, %llu] set_curr[%llu, %llu]\n",
			per_cpu(update_exec_start, cpu),
			per_cpu(sched_update_exec_start, cpu),
			per_cpu(pick_exec_start, cpu),
			per_cpu(sched_pick_exec_start, cpu),
			per_cpu(set_curr_exec_start, cpu),
			per_cpu(sched_set_curr_exec_start, cpu));
}

/* sched: update rt exec info*/
static inline void update_rt_exec_info(struct task_struct *curr,
				u64 delta_exec, struct rq *rq)
{
	per_cpu(exec_task, rq->cpu).pid = curr->pid;
	per_cpu(exec_task, rq->cpu).prio = curr->prio;
	strncpy(per_cpu(exec_task, rq->cpu).comm,
		curr->comm, sizeof(per_cpu(exec_task, rq->cpu).comm));
	per_cpu(exec_delta_time, rq->cpu) = delta_exec;
	per_cpu(clock_task, rq->cpu) = rq->clock_task;
	per_cpu(update_exec_start, rq->cpu) = curr->se.exec_start;
}
