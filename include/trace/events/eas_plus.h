/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

TRACE_EVENT(sched_select_task_rq,

	TP_PROTO(struct task_struct *tsk,
		int policy, int prev_cpu, int target_cpu,
		int task_util, int boost, bool prefer, int wake_flags),

	TP_ARGS(tsk, policy, prev_cpu, target_cpu, task_util, boost,
		prefer, wake_flags),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, prev_cpu)
		__field(int, target_cpu)
		__field(int, task_util)
		__field(int, boost)
		__field(long, task_mask)
		__field(bool, prefer)
#ifdef CONFIG_MTK_SCHED_CPU_PREFER
		__field(int, cpu_prefer)
#endif
		__field(int, wake_flags)
		),

	TP_fast_assign(
		__entry->pid        = tsk->pid;
		__entry->policy     = policy;
		__entry->prev_cpu   = prev_cpu;
		__entry->target_cpu = target_cpu;
		__entry->task_util	= task_util;
		__entry->boost		= boost;
		__entry->task_mask	= tsk->cpus_allowed.bits[0];
		__entry->prefer		= prefer;
#ifdef CONFIG_MTK_SCHED_CPU_PREFER
		__entry->cpu_prefer = tsk->cpu_prefer;
#endif
		__entry->wake_flags	= wake_flags;
		),

	TP_printk("pid=%4d policy=0x%08x pre-cpu=%d target=%d util=%d boost=%d mask=0x%lx prefer=%d cpu_prefer=%d flags=%d",
		__entry->pid,
		__entry->policy,
		__entry->prev_cpu,
		__entry->target_cpu,
		__entry->task_util,
		__entry->boost,
		__entry->task_mask,
		__entry->prefer,
#ifdef CONFIG_MTK_SCHED_CPU_PREFER
		__entry->cpu_prefer,
#else
		0,
#endif
		__entry->wake_flags)

);

/*
 * Tracepoint for task migrations.
 */
TRACE_EVENT(sched_migrate,

	TP_PROTO(struct task_struct *tsk, int dest, int force),

	TP_ARGS(tsk, dest, force),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int,  dest)
		__field(int,  force)
		),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid   = tsk->pid;
		__entry->dest  = dest;
		__entry->force = force;
		),

	TP_printk("comm=%s pid=%d dest=%d force=%d",
		__entry->comm, __entry->pid,
		__entry->dest, __entry->force)
);

#ifdef CONFIG_MTK_SCHED_CPU_PREFER
/*
 * Tracepoint for set task cpu prefer
 */
TRACE_EVENT(sched_set_cpuprefer,

	TP_PROTO(struct task_struct *tsk),

	TP_ARGS(tsk),

	TP_STRUCT__entry(
		__array(char,  comm,   TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int,   cpu_prefer)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid            = tsk->pid;
		__entry->cpu_prefer     = tsk->cpu_prefer;
	),

	TP_printk("pid=%d comm=%s cpu_prefer=%d",
		__entry->pid, __entry->comm, __entry->cpu_prefer)
);
#endif

