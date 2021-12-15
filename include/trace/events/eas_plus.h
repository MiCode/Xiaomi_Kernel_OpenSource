/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifdef CONFIG_MTK_SCHED_EXTENSION

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
		__entry->cpu_prefer = cpu_prefer(tsk);
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

	TP_PROTO(struct task_struct *tsk, int src, int dest, int force),

	TP_ARGS(tsk, src, dest, force),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int,  src)
		__field(int,  dest)
		__field(int,  force)
		),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid   = tsk->pid;
		__entry->src  = src;
		__entry->dest  = dest;
		__entry->force = force;
		),

	TP_printk("pid=%d comm=%s src=%d dest=%d force=%d",
		__entry->pid, __entry->comm,
		__entry->src, __entry->dest,
		__entry->force)
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

#ifdef CONFIG_MTK_SCHED_BIG_TASK_MIGRATE
/*
 * Tracepoint for big task migration
 */
TRACE_EVENT(sched_big_task_migration,

	TP_PROTO(int pid, int src_cpu, int dst_cpu),

	TP_ARGS(pid, src_cpu, dst_cpu),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, src_cpu)
		__field(int, dst_cpu)
	),

	TP_fast_assign(
		__entry->pid		= pid;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
	),

	TP_printk("p->pid=%d src_cpu=%d dst_cpu=%d",
		__entry->pid, __entry->src_cpu, __entry->dst_cpu)
);

/*
 * Tracepoint for big task rotation
 */
TRACE_EVENT(sched_big_task_rotation,

	TP_PROTO(int src_cpu, int dst_cpu, int src_pid, int dst_pid,
		int fin, int set_uclamp),

	TP_ARGS(src_cpu, dst_cpu, src_pid, dst_pid, fin, set_uclamp),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, src_pid)
		__field(int, dst_pid)
		__field(int, fin)
		__field(int, set_uclamp)
	),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->src_pid	= src_pid;
		__entry->dst_pid	= dst_pid;
		__entry->fin		= fin;
		__entry->set_uclamp	= set_uclamp;
	),

	TP_printk("src_cpu=%d dst_cpu=%d src_pid=%d dst_pid=%d fin=%d set=%d",
		__entry->src_cpu, __entry->dst_cpu,
		__entry->src_pid, __entry->dst_pid,
		__entry->fin, __entry->set_uclamp)
);

TRACE_EVENT(sched_big_task_rotation_reset,

	TP_PROTO(int set_uclamp),

	TP_ARGS(set_uclamp),

	TP_STRUCT__entry(
		__field(int, set_uclamp)
	),

	TP_fast_assign(
		__entry->set_uclamp	= set_uclamp;
	),

	TP_printk("set_uclamp=%d",
		__entry->set_uclamp)
);
#endif
/**
 * sched_isolate - called when cores are isolated/unisolated
 *
 * @acutal_mask: mask of cores actually isolated/unisolated
 * @req_mask: mask of cores requested isolated/unisolated
 * @online_mask: cpu online mask
 * @time: amount of time in us it took to isolate/unisolate
 * @isolate: 1 if isolating, 0 if unisolating
 *
 */
TRACE_EVENT(sched_isolate,

	TP_PROTO(unsigned int requested_cpu, unsigned int isolated_cpus,
		 u64 start_time, unsigned char isolate),

	TP_ARGS(requested_cpu, isolated_cpus, start_time, isolate),

	TP_STRUCT__entry(
		__field(u32, requested_cpu)
		__field(u32, isolated_cpus)
		__field(u32, time)
		__field(unsigned char, isolate)
	),

	TP_fast_assign(
		__entry->requested_cpu = requested_cpu;
		__entry->isolated_cpus = isolated_cpus;
		__entry->time = div64_u64(sched_clock() - start_time, 1000);
		__entry->isolate = isolate;
	),

	TP_printk("iso cpu=%u cpus=0x%x time=%u us isolated=%d",
		  __entry->requested_cpu, __entry->isolated_cpus,
		  __entry->time, __entry->isolate)
);
#endif /* CONFIG_MTK_SCHED_EXTENSION */
#ifdef CONFIG_MTK_TASK_TURBO
TRACE_EVENT(sched_set_user_nice,
	TP_PROTO(struct task_struct *task, int prio, int is_turbo),
	TP_ARGS(task, prio, is_turbo),
	TP_STRUCT__entry(
		__field(int, pid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, prio)
		__field(int, is_turbo)
	),

	TP_fast_assign(
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
		__entry->pid	  = task->pid;
		__entry->prio	  = prio;
		__entry->is_turbo = is_turbo;
	),

	TP_printk("comm=%s pid=%d prio=%d is_turbo=%d",
		__entry->comm, __entry->pid, __entry->prio, __entry->is_turbo)
)
#endif
