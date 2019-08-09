#undef TRACE_SYSTEM
#define TRACE_SYSTEM sched

#if !defined(_TRACE_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCHED_H

#include <linux/sched.h>
#include <linux/tracepoint.h>
#include <linux/binfmts.h>

#ifdef CONFIG_MTK_SCHED_TRACERS
/* M: states for tracking I/O & mutex events
 * notice avoid to conflict with linux/sched.h
 *
 * A bug linux not fixed:
 * 'K' for TASK_WAKEKILL specified in linux/sched.h
 * but marked 'K' in sched_switch will cause Android systrace parser confused
 * therefore for sched_switch events, these extra states will be printed
 * in the end of each line
 */
#define _MT_TASK_BLOCKED_RTMUX		(TASK_STATE_MAX << 1)
#define _MT_TASK_BLOCKED_MUTEX		(TASK_STATE_MAX << 2)
#define _MT_TASK_BLOCKED_IO		(TASK_STATE_MAX << 3)
#define _MT_EXTRA_STATE_MASK (_MT_TASK_BLOCKED_RTMUX | \
			      _MT_TASK_BLOCKED_MUTEX | \
			      _MT_TASK_BLOCKED_IO | \
			      TASK_WAKEKILL | \
			      TASK_PARKED | \
			      TASK_NOLOAD)
#endif
#define _MT_TASK_STATE_MASK  ((TASK_STATE_MAX - 1) & \
			      ~(TASK_WAKEKILL | TASK_PARKED | TASK_NOLOAD))
/*
 * Tracepoint for calling kthread_stop, performed to end a kthread:
 */
TRACE_EVENT(sched_kthread_stop,

	TP_PROTO(struct task_struct *t),

	TP_ARGS(t),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, t->comm, TASK_COMM_LEN);
		__entry->pid	= t->pid;
	),

	TP_printk("comm=%s pid=%d", __entry->comm, __entry->pid)
);

/*
 * Tracepoint for the return value of the kthread stopping:
 */
TRACE_EVENT(sched_kthread_stop_ret,

	TP_PROTO(int ret),

	TP_ARGS(ret),

	TP_STRUCT__entry(
		__field(	int,	ret	)
	),

	TP_fast_assign(
		__entry->ret	= ret;
	),

	TP_printk("ret=%d", __entry->ret)
);

#ifdef CREATE_TRACE_POINTS
static inline long __trace_sched_switch_state(bool preempt,
						struct task_struct *p);
#endif

extern bool system_overutilized(int cpu);
/*
 * Tracepoint for waking up a task:
 */
DECLARE_EVENT_CLASS(sched_wakeup_template,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(__perf_task(p)),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
		__field(	int,	success			)
		__field(	int,	target_cpu		)
#ifdef CONFIG_MTK_SCHED_TRACERS
		__field(long, state)
		__field(bool, overutil)
#endif
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
		__entry->success	= 1; /* rudiment, kill when possible */
		__entry->target_cpu	= task_cpu(p);
#ifdef CONFIG_MTK_SCHED_TRACERS
		__entry->state	= __trace_sched_switch_state(false, p);
		__entry->overutil = system_overutilized(task_rq(p)->cpu);
#endif
	),
#ifdef CONFIG_MTK_SCHED_TRACERS
	TP_printk("comm=%s pid=%d prio=%d success=%d target_cpu=%03d state=%s overutil=%d",
		  __entry->comm, __entry->pid, __entry->prio,
		  __entry->success, __entry->target_cpu,
		  __entry->state & (~TASK_STATE_MAX) ?
		  __print_flags(__entry->state & (~TASK_STATE_MAX), "|",
				{ TASK_INTERRUPTIBLE, "S"},
				{ TASK_UNINTERRUPTIBLE, "D"},
				{ __TASK_STOPPED, "T"},
				{ __TASK_TRACED, "t"},
				{ EXIT_ZOMBIE, "Z"},
				{ EXIT_DEAD, "X"},
				{ TASK_DEAD, "x"},
				{ TASK_WAKEKILL, "K"},
				{ TASK_WAKING, "W"},
				{ TASK_PARKED, "P"},
				{ TASK_NOLOAD, "N"},
				{ _MT_TASK_BLOCKED_RTMUX, "r"},
				{ _MT_TASK_BLOCKED_MUTEX, "m"},
				{ _MT_TASK_BLOCKED_IO, "d"}) : "R",
				__entry->overutil)
#else
	TP_printk("comm=%s pid=%d prio=%d target_cpu=%03d",
		  __entry->comm, __entry->pid, __entry->prio,
		  __entry->target_cpu)
#endif
);

/*
 * Tracepoint called when waking a task; this tracepoint is guaranteed to be
 * called from the waking context.
 */
DEFINE_EVENT(sched_wakeup_template, sched_waking,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

/*
 * Tracepoint called when the task is actually woken; p->state == TASK_RUNNNG.
 * It it not always called from the waking context.
 */
DEFINE_EVENT(sched_wakeup_template, sched_wakeup,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

/*
 * Tracepoint for waking up a new task:
 */
DEFINE_EVENT(sched_wakeup_template, sched_wakeup_new,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

#ifdef CREATE_TRACE_POINTS
static inline long __trace_sched_switch_state(bool preempt, struct task_struct *p)
{
#ifdef CONFIG_MTK_SCHED_TRACERS
	long state = p->state;
#endif
	/*
	 * M:mark as comment to export more task state for
	 * migration & wakeup
	 */
#ifdef CONFIG_SCHED_DEBUG
	//BUG_ON(p != current);
#endif /* CONFIG_SCHED_DEBUG */

	/*
	 * Preemption ignores task state, therefore preempted tasks are always
	 * RUNNING (we will not have dequeued if state != RUNNING).
	 */
#ifdef CONFIG_MTK_SCHED_TRACERS
	if (preempt)
		state = TASK_RUNNING | TASK_STATE_MAX;
#ifdef CONFIG_RT_MUTEXES
	if (p->pi_blocked_on)
		state |= _MT_TASK_BLOCKED_RTMUX;
#endif
#ifdef CONFIG_DEBUG_MUTEXES
	if (p->blocked_on)
		state |= _MT_TASK_BLOCKED_MUTEX;
#endif
	if ((p->state & TASK_UNINTERRUPTIBLE) && p->in_iowait)
		state |= _MT_TASK_BLOCKED_IO;

	return state;
#else
	return preempt ? TASK_RUNNING | TASK_STATE_MAX : p->state;
#endif
}
#endif /* CREATE_TRACE_POINTS */

/*
 * Tracepoint for task switches, performed by the scheduler:
 */
TRACE_EVENT(sched_switch,

	TP_PROTO(bool preempt,
		 struct task_struct *prev,
		 struct task_struct *next),

	TP_ARGS(preempt, prev, next),

	TP_STRUCT__entry(
		__array(	char,	prev_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	prev_pid			)
		__field(	int,	prev_prio			)
		__field(	long,	prev_state			)
		__array(	char,	next_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	next_pid			)
		__field(	int,	next_prio			)
#if defined(CONFIG_MTK_SCHED_TRACERS) && defined(CONFIG_CGROUPS)
		__field(int,	prev_cgrp_id)
		__field(int,	next_cgrp_id)
		__field(int,	prev_st_cgrp_id)
		__field(int,	next_st_cgrp_id)
#endif
	),

	TP_fast_assign(
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->prev_pid	= prev->pid;
		__entry->prev_prio	= prev->prio;
		__entry->prev_state	= __trace_sched_switch_state(preempt, prev);
		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
		__entry->next_pid	= next->pid;
		__entry->next_prio	= next->prio;
#if defined(CONFIG_MTK_SCHED_TRACERS) && defined(CONFIG_CGROUPS)
#if defined(CONFIG_CPUSETS)
		__entry->prev_cgrp_id	= prev->cgroups->subsys[0]->cgroup->id;
		__entry->next_cgrp_id	= next->cgroups->subsys[0]->cgroup->id;
#else
		__entry->prev_cgrp_id	= 0;
		__entry->next_cgrp_id	= 0;
#endif
#if defined(CONFIG_CGROUP_SCHEDTUNE)
		__entry->prev_st_cgrp_id = prev->cgroups->subsys[3]->cgroup->id;
		__entry->next_st_cgrp_id = next->cgroups->subsys[3]->cgroup->id;
#else
		__entry->prev_st_cgrp_id = 0;
		__entry->next_st_cgrp_id = 0;
#endif
#endif
	),

#ifdef CONFIG_MTK_SCHED_TRACERS
	TP_printk(
#if defined(CONFIG_CGROUPS)
	"prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s%s ==> next_comm=%s next_pid=%d next_prio=%d%s%s prev->cgrp=%d next->cgrp=%d prev->st=%d next->st=%d",
#else
	"prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s%s ==> next_comm=%s next_pid=%d next_prio=%d%s%s",
#endif
		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,
		__entry->prev_state & (_MT_TASK_STATE_MASK) ?
		__print_flags(__entry->prev_state & (_MT_TASK_STATE_MASK), "|",
				{ TASK_INTERRUPTIBLE, "S" },
				{ TASK_UNINTERRUPTIBLE, "D" },
				{ __TASK_STOPPED, "T" },
				{ __TASK_TRACED, "t" },
				{ EXIT_DEAD, "X" },
				{ EXIT_ZOMBIE, "Z" },
				{ TASK_DEAD, "x" },
				{ TASK_WAKEKILL, "K"},
				{ TASK_WAKING, "W"}) : "R",
		__entry->prev_state & TASK_STATE_MAX ? "+" : "",
		__entry->next_comm, __entry->next_pid, __entry->next_prio,
		(__entry->prev_state & _MT_EXTRA_STATE_MASK) ?
			" extra_prev_state=" : "",
		__print_flags(__entry->prev_state & _MT_EXTRA_STATE_MASK, "|",
				{ TASK_WAKEKILL, "K" },
				{ TASK_PARKED, "P" },
				{ TASK_NOLOAD, "N" },
				{ _MT_TASK_BLOCKED_RTMUX, "r" },
				{ _MT_TASK_BLOCKED_MUTEX, "m" },
				{ _MT_TASK_BLOCKED_IO, "d" })
#if defined(CONFIG_CGROUPS)
				, __entry->prev_cgrp_id
				, __entry->next_cgrp_id
				, __entry->prev_st_cgrp_id
				, __entry->next_st_cgrp_id
#endif
	)
#else
	TP_printk("prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s%s ==> next_comm=%s next_pid=%d next_prio=%d",
		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,
		__entry->prev_state & (TASK_STATE_MAX-1) ?
		  __print_flags(__entry->prev_state & (TASK_STATE_MAX-1), "|",
				{ 1, "S"} , { 2, "D" }, { 4, "T" }, { 8, "t" },
				{ 16, "Z" }, { 32, "X" }, { 64, "x" },
				{ 128, "K" }, { 256, "W" }, { 512, "P" },
				{ 1024, "N" }) : "R",
		__entry->prev_state & TASK_STATE_MAX ? "+" : "",
		__entry->next_comm, __entry->next_pid, __entry->next_prio)
#endif
);

/*
 * Tracepoint for a task being migrated:
 */
TRACE_EVENT(sched_migrate_task,

	TP_PROTO(struct task_struct *p, int dest_cpu),

	TP_ARGS(p, dest_cpu),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
		__field(	int,	orig_cpu		)
		__field(	int,	dest_cpu		)
#ifdef CONFIG_MTK_SCHED_TRACERS
		__field(long, state)
#endif
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
		__entry->orig_cpu	= task_cpu(p);
		__entry->dest_cpu	= dest_cpu;
#ifdef CONFIG_MTK_SCHED_TRACERS
		__entry->state      =	__trace_sched_switch_state(false, p);
#endif
	),

#ifdef CONFIG_MTK_SCHED_TRACERS
	TP_printk("comm=%s pid=%d prio=%d orig_cpu=%d dest_cpu=%d state=%s",
		__entry->comm, __entry->pid, __entry->prio,
		__entry->orig_cpu, __entry->dest_cpu,
		__entry->state & (~TASK_STATE_MAX) ?
		__print_flags(__entry->state & (~TASK_STATE_MAX), "|",
				{ TASK_INTERRUPTIBLE, "S"},
				{ TASK_UNINTERRUPTIBLE, "D" },
				{ __TASK_STOPPED, "T" },
				{ __TASK_TRACED, "t" },
				{ EXIT_ZOMBIE, "Z" },
				{ EXIT_DEAD, "X" },
				{ TASK_DEAD, "x" },
				{ TASK_WAKEKILL, "K" },
				{ TASK_WAKING, "W" },
				{ TASK_PARKED, "P" },
				{ TASK_NOLOAD, "N" },
				{ _MT_TASK_BLOCKED_RTMUX, "r" },
				{ _MT_TASK_BLOCKED_MUTEX, "m"},
				{ _MT_TASK_BLOCKED_IO, "d"}) : "R")
#else
	TP_printk("comm=%s pid=%d prio=%d orig_cpu=%d dest_cpu=%d",
		  __entry->comm, __entry->pid, __entry->prio,
		  __entry->orig_cpu, __entry->dest_cpu)
#endif
);

DECLARE_EVENT_CLASS(sched_process_template,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
	),

	TP_printk("comm=%s pid=%d prio=%d",
		  __entry->comm, __entry->pid, __entry->prio)
);

/*
 * Tracepoint for freeing a task:
 */
DEFINE_EVENT(sched_process_template, sched_process_free,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));


/*
 * Tracepoint for a task exiting:
 */
DEFINE_EVENT(sched_process_template, sched_process_exit,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

/*
 * Tracepoint for waiting on task to unschedule:
 */
DEFINE_EVENT(sched_process_template, sched_wait_task,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));

/*
 * Tracepoint for a waiting task:
 */
TRACE_EVENT(sched_process_wait,

	TP_PROTO(struct pid *pid),

	TP_ARGS(pid),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
		__entry->pid		= pid_nr(pid);
		__entry->prio		= current->prio;
	),

	TP_printk("comm=%s pid=%d prio=%d",
		  __entry->comm, __entry->pid, __entry->prio)
);

/*
 * Tracepoint for do_fork:
 */
TRACE_EVENT(sched_process_fork,

	TP_PROTO(struct task_struct *parent, struct task_struct *child),

	TP_ARGS(parent, child),

	TP_STRUCT__entry(
		__array(	char,	parent_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	parent_pid			)
		__array(	char,	child_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	child_pid			)
	),

	TP_fast_assign(
		memcpy(__entry->parent_comm, parent->comm, TASK_COMM_LEN);
		__entry->parent_pid	= parent->pid;
		memcpy(__entry->child_comm, child->comm, TASK_COMM_LEN);
		__entry->child_pid	= child->pid;
	),

	TP_printk("comm=%s pid=%d child_comm=%s child_pid=%d",
		__entry->parent_comm, __entry->parent_pid,
		__entry->child_comm, __entry->child_pid)
);

#ifdef CONFIG_MTK_SCHED_BOOST
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

/*
 * Tracepoint for load balance sched group calculation
 */
TRACE_EVENT(sched_update_lb_sg,

	TP_PROTO(unsigned long avg_load, unsigned long group_load,
		unsigned long group_capacity,
		int group_no_capacity, int group_type),

	TP_ARGS(avg_load, group_load, group_capacity,
		group_no_capacity, group_type),

	TP_STRUCT__entry(
		__field(unsigned long, avg_load)
		__field(unsigned long, group_load)
		__field(unsigned long, group_capacity)
		__field(int, group_no_capacity)
		__field(int, group_type)
	),

	TP_fast_assign(
		__entry->avg_load       = avg_load;
		__entry->group_load     = group_load;
		__entry->group_capacity = group_capacity;
		__entry->group_no_capacity = group_no_capacity;
		__entry->group_type = group_type;
	),

	TP_printk("avg_load=%lu group_load=%lu group_capacity=%lu group_no_capacity=%d group_type=%d",
		__entry->avg_load, __entry->group_load, __entry->group_capacity,
		__entry->group_no_capacity, __entry->group_type)
);

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
/*
 * Tracepoint for share buck calculation
 */
TRACE_EVENT(sched_share_buck,

	TP_PROTO(int cpu_idx, int cid, int cap_idx, int co_buck_cid,
			int co_buck_cap_idx),

	TP_ARGS(cpu_idx, cid, cap_idx, co_buck_cid, co_buck_cap_idx),

	TP_STRUCT__entry(
		__field(int, cpu_idx)
		__field(int, cid)
		__field(int, cap_idx)
		__field(int, co_buck_cid)
		__field(int, co_buck_cap_idx)
	),

	TP_fast_assign(
		__entry->cpu_idx        = cpu_idx;
		__entry->cid            = cid;
		__entry->cap_idx        = cap_idx;
		__entry->co_buck_cid    = co_buck_cid;
		__entry->co_buck_cap_idx = co_buck_cap_idx;
	),

	TP_printk("cpu_idx=%d cid%d=%d co_cid%d=%d",
		__entry->cpu_idx, __entry->cid, __entry->cap_idx,
		__entry->co_buck_cid, __entry->co_buck_cap_idx)
);

/*
 * Tracepoint for idle power calculation
 */
TRACE_EVENT(sched_idle_power,

	TP_PROTO(int sd_level, int cap_idx, int leak_pwr, int energy_cost),

	TP_ARGS(sd_level, cap_idx, leak_pwr, energy_cost),

	TP_STRUCT__entry(
		__field(int, sd_level)
		__field(int, cap_idx)
		__field(int, leak_pwr)
		__field(int, energy_cost)
	),

	TP_fast_assign(
		__entry->sd_level       = sd_level;
		__entry->cap_idx        = cap_idx;
		__entry->leak_pwr       = leak_pwr;
		__entry->energy_cost    = energy_cost;
	),

	TP_printk("lv=%d tlb[%d].leak=(%d) total=%d",
		__entry->sd_level, __entry->cap_idx, __entry->leak_pwr,
		__entry->energy_cost)
);

/*
 * Tracepoint for busy_power calculation
 */
TRACE_EVENT(sched_busy_power,

	TP_PROTO(int sd_level, int cap_idx, unsigned long dyn_pwr,
			unsigned long volt_f, unsigned long buck_pwr,
			int co_cap_idx, int leak_pwr, int energy_cost),

	TP_ARGS(sd_level, cap_idx, dyn_pwr, volt_f, buck_pwr, co_cap_idx,
		leak_pwr, energy_cost),

	TP_STRUCT__entry(
		__field(int, sd_level)
		__field(int, cap_idx)
		__field(unsigned long, dyn_pwr)
		__field(unsigned long, volt_f)
		__field(unsigned long, buck_pwr)
		__field(int, co_cap_idx)
		__field(unsigned long, leak_pwr)
		__field(int, energy_cost)
	),

	TP_fast_assign(
		__entry->sd_level       = sd_level;
		__entry->cap_idx        = cap_idx;
		__entry->dyn_pwr        = dyn_pwr;
		__entry->volt_f         = volt_f;
		__entry->buck_pwr       = buck_pwr;
		__entry->co_cap_idx     = co_cap_idx;
		__entry->leak_pwr       = leak_pwr;
		__entry->energy_cost    = energy_cost;
	),

	TP_printk("lv=%d tlb[%d].pwr=%ld volt_f=%ld buck.pwr=%ld tlb[%d].leak=(%ld) total=%d",
		__entry->sd_level, __entry->cap_idx,  __entry->dyn_pwr,
		__entry->volt_f, __entry->buck_pwr,  __entry->co_cap_idx,
		__entry->leak_pwr, __entry->energy_cost)
);
#endif

/*
 * Tracepoint for HMP (CONFIG_SCHED_HMP) task migrations.
 */
TRACE_EVENT(sched_hmp_migrate,

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

/*
 * Tracepoint for accounting sched group energy
 */
TRACE_EVENT(sched_energy_diff,

	TP_PROTO(struct task_struct *tsk, int scpu, int dcpu, int udelta,
		int nrgb, int nrga, int nrgd),

	TP_ARGS(tsk, scpu, dcpu, udelta,
		nrgb, nrga, nrgd),

	TP_STRUCT__entry(
		__array(char,  comm,   TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int,   scpu)
		__field(int,   dcpu)
		__field(int,   udelta)
		__field(int,   nrgb)
		__field(int,   nrga)
		__field(int,   nrgd)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid            = tsk->pid;
		__entry->scpu           = scpu;
		__entry->dcpu           = dcpu;
		__entry->udelta         = udelta;
		__entry->nrgb           = nrgb;
		__entry->nrga           = nrga;
		__entry->nrgd           = nrgd;
	),

	TP_printk("pid=%d comm=%s src_cpu=%d dst_cpu=%d usage_delta=%d nrg_before=%d nrg_after=%d nrg_diff=%d",
		__entry->pid, __entry->comm,
		__entry->scpu, __entry->dcpu, __entry->udelta,
		__entry->nrgb, __entry->nrga, __entry->nrgd)
);


/*
 * Tracepoint for showing the result of task runqueue selection
 */
TRACE_EVENT(sched_select_task_rq,

	TP_PROTO(struct task_struct *tsk,
		int policy, int prev_cpu, int target_cpu,
		int task_util, int boost, bool prefer, int wake_flags),

	TP_ARGS(tsk, policy, prev_cpu, target_cpu, task_util, boost, prefer,
			wake_flags),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, prev_cpu)
		__field(int, target_cpu)
		__field(int, task_util)
		__field(int, boost)
		__field(long, task_mask)
		__field(bool, prefer)
#ifdef CONFIG_MTK_SCHED_BOOST
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
		__entry->task_mask	= tsk_cpus_allowed(tsk)->bits[0];
		__entry->prefer		= prefer;
#ifdef CONFIG_MTK_SCHED_BOOST
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
#ifdef CONFIG_MTK_SCHED_BOOST
		__entry->cpu_prefer,
#else
		0,
#endif
		__entry->wake_flags)
);

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
TRACE_EVENT(sched_cpufreq_fastpath_request,
	TP_PROTO(int cpu, unsigned long req_cap,
		unsigned long util,
		unsigned long boosted, int rt),

	TP_ARGS(cpu, req_cap, util, boosted, rt),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, req_cap)
		__field(unsigned long, util)
		__field(unsigned long, boosted)
		__field(int, rt)
		),

	TP_fast_assign(
		__entry->cpu              = cpu;
		__entry->req_cap          = req_cap;
		__entry->util             = util;
		__entry->boosted          = boosted;
		__entry->rt               = rt;
		),

	TP_printk("cpu=%d req_cap=%lu util=%lu boosted=%lu rt=%d",
		__entry->cpu,
		__entry->req_cap,
		__entry->util,
		__entry->boosted,
		__entry->rt
		)
);

TRACE_EVENT(sched_cpufreq_fastpath,
	TP_PROTO(int cid, unsigned long req_cap, int freq_new),

		TP_ARGS(cid, req_cap, freq_new),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(unsigned long, req_cap)
		__field(int, freq_new)
		),

	TP_fast_assign(
		__entry->cid              = cid;
		__entry->req_cap          = req_cap;
		__entry->freq_new         = freq_new;
		),

	TP_printk("cid=%d req_cap=%lu freq_new=%dKHZ",
		__entry->cid,
		__entry->req_cap,
		__entry->freq_new
		)
);
#endif

/*
 * Tracepoint for showing tracked migration information
 */
TRACE_EVENT(sched_dynamic_threshold,

	TP_PROTO(struct task_struct *tsk, unsigned int threshold,
		unsigned int status, int curr_cpu, int target_cpu,
		int task_load, struct clb_stats *B, struct clb_stats *L),

	TP_ARGS(tsk, threshold, status, curr_cpu, target_cpu, task_load, B, L),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int, prio)
		__field(unsigned int, threshold)
		__field(unsigned int, status)
		__field(int, curr_cpu)
		__field(int, target_cpu)
		__field(int, curr_load)
		__field(int, target_load)
		__field(int, task_load)
		__field(int, B_load_avg)
		__field(int, L_load_avg)
		),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid         = tsk->pid;
		__entry->prio        = tsk->prio;
		__entry->threshold   = threshold;
		__entry->status      = status;
		__entry->curr_cpu    = curr_cpu;
		__entry->target_cpu  = target_cpu;
		__entry->curr_load   = cpu_rq(curr_cpu)->cfs.avg.loadwop_avg;
		__entry->target_load = cpu_rq(target_cpu)->cfs.avg.loadwop_avg;
		__entry->task_load   = task_load;
		__entry->B_load_avg  = B->load_avg;
		__entry->L_load_avg  = L->load_avg;
		  ),

	TP_printk(
		"pid=%4d prio=%d status=0x%4x dyn=%4u task-load=%4d curr-cpu=%d(%4d) target=%d(%4d) L-load-avg=%4d B-load-avg=%4d comm=%s",
		__entry->pid,
		__entry->prio,
		__entry->status,
		__entry->threshold,
		__entry->task_load,
		__entry->curr_cpu,
		__entry->curr_load,
		__entry->target_cpu,
		__entry->target_load,
		__entry->L_load_avg,
		__entry->B_load_avg,
		__entry->comm)
	);

TRACE_EVENT(sched_dynamic_threshold_draw,

		TP_PROTO(unsigned int B_threshold, unsigned int L_threshold),

		TP_ARGS(B_threshold, L_threshold),

		TP_STRUCT__entry(
			__field(unsigned int, up_threshold)
			__field(unsigned int, down_threshold)
			),

		TP_fast_assign(
				__entry->up_threshold	= B_threshold;
				__entry->down_threshold	= L_threshold;
			),

		TP_printk(
				"%4u, %4u",
				__entry->up_threshold,
				__entry->down_threshold)
		);

/*
 * Tracepoint for showing the result of hmp task runqueue selection
 */
TRACE_EVENT(sched_hmp_select_task_rq,

	TP_PROTO(struct task_struct *tsk, int step, int sd_flag, int prev_cpu,
		int target_cpu, int task_load, struct clb_stats *B,
		struct clb_stats *L),

	TP_ARGS(tsk, step, sd_flag, prev_cpu, target_cpu, task_load, B, L),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int, prio)
		__field(int, step)
		__field(int, sd_flag)
		__field(int, prev_cpu)
		__field(int, target_cpu)
		__field(int, prev_load)
		__field(int, target_load)
		__field(int, task_load)
		__field(int, B_load_avg)
		__field(int, L_load_avg)
		),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid         = tsk->pid;
		__entry->prio        = tsk->prio;
		__entry->step        = step;
		__entry->sd_flag     = sd_flag;
		__entry->prev_cpu    = prev_cpu;
		__entry->target_cpu  = target_cpu;
		__entry->prev_load   = cpu_rq(prev_cpu)->cfs.avg.loadwop_avg;
		__entry->target_load = cpu_rq(target_cpu)->cfs.avg.loadwop_avg;
		__entry->task_load   = task_load;
		__entry->B_load_avg  = B->load_avg;
		__entry->L_load_avg  = L->load_avg;
		  ),

	TP_printk(
		"pid=%4d prio=%d task-load=%4d sd-flag=%2d step=%d pre-cpu=%d(%4d) target=%d(%4d) L-load-avg=%4d B-load-avg=%4d comm=%s",
		__entry->pid,
		__entry->prio,
		__entry->task_load,
		__entry->sd_flag,
		__entry->step,
		__entry->prev_cpu,
		__entry->prev_load,
		__entry->target_cpu,
		__entry->target_load,
		__entry->L_load_avg,
		__entry->B_load_avg,
		__entry->comm)
	);

/*
 * Tracepoint for dumping hmp cluster load ratio
 */
TRACE_EVENT(sched_hmp_load,

		TP_PROTO(int step, int B_load_avg, int L_load_avg),

		TP_ARGS(step, B_load_avg, L_load_avg),

		TP_STRUCT__entry(
			__field(int, step)
			__field(int, B_load_avg)
			__field(int, L_load_avg)
			),

		TP_fast_assign(
			__entry->step = step;
			__entry->B_load_avg = B_load_avg;
			__entry->L_load_avg = L_load_avg;
			),

		TP_printk("[%d]: B-load-avg=%4d L-load-avg=%4d",
			__entry->step,
			__entry->B_load_avg,
			__entry->L_load_avg)
	   );

/*
 * Tracepoint for dumping hmp statistics
 */
TRACE_EVENT(sched_hmp_stats,

		TP_PROTO(struct hmp_statisic *hmp_stats),

		TP_ARGS(hmp_stats),

		TP_STRUCT__entry(
			__field(unsigned int, nr_force_up)
			__field(unsigned int, nr_force_down)
			),

		TP_fast_assign(
			__entry->nr_force_up = hmp_stats->nr_force_up;
			__entry->nr_force_down = hmp_stats->nr_force_down;
			),

		TP_printk("nr-force-up=%d nr-force-down=%2d",
			__entry->nr_force_up,
			__entry->nr_force_down)
	   );

/*
 * Tracepoint for cfs task enqueue event
 */
TRACE_EVENT(sched_cfs_enqueue_task,

		TP_PROTO(struct task_struct *tsk, int tsk_load, int cpu_id),

		TP_ARGS(tsk, tsk_load, cpu_id),

		TP_STRUCT__entry(
			__array(char, comm, TASK_COMM_LEN)
			__field(pid_t, tsk_pid)
			__field(int, tsk_load)
			__field(int, cpu_id)
			),

		TP_fast_assign(
			memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
			__entry->tsk_pid = tsk->pid;
			__entry->tsk_load = tsk_load;
			__entry->cpu_id = cpu_id;
			),

		TP_printk("cpu-id=%d task-pid=%4d task-load=%4d comm=%s",
			__entry->cpu_id,
			__entry->tsk_pid,
			__entry->tsk_load,
			__entry->comm)
		);

/*
 * Tracepoint for cfs task dequeue event
 */
TRACE_EVENT(sched_cfs_dequeue_task,

		TP_PROTO(struct task_struct *tsk, int tsk_load, int cpu_id),

		TP_ARGS(tsk, tsk_load, cpu_id),

		TP_STRUCT__entry(
			__array(char, comm, TASK_COMM_LEN)
			__field(pid_t, tsk_pid)
			__field(int, tsk_load)
			__field(int, cpu_id)
			),

		TP_fast_assign(
			memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
			__entry->tsk_pid = tsk->pid;
			__entry->tsk_load = tsk_load;
			__entry->cpu_id = cpu_id;
			),

		TP_printk("cpu-id=%d task-pid=%4d task-load=%4d comm=%s",
			__entry->cpu_id,
			__entry->tsk_pid,
			__entry->tsk_load,
			__entry->comm)
		);

/*
 * Tracepoint for showing cluster statistics in HMP
 */
TRACE_EVENT(sched_cluster_stats,

		TP_PROTO(int target, unsigned long loadwop_avg,
			unsigned int h_nr_running, unsigned long cluster_mask,
			int nr_task, int load_avg, int capacity,
			int acap, int scaled_atask,	int threshold),

		TP_ARGS(target, loadwop_avg, h_nr_running,
			cluster_mask, nr_task, load_avg, capacity,
			acap, scaled_atask, threshold),

		TP_STRUCT__entry(
			__field(int, target)
			__field(unsigned long, loadwop_avg)
			__field(unsigned int, h_nr_running)
			__field(unsigned long, cluster_mask)
			__field(int, nr_task)
			__field(int, load_avg)
			__field(int, capacity)
			__field(int, acap)
			__field(int, scaled_atask)
			__field(int, threshold)
			),

		TP_fast_assign(
			__entry->target = target;
			__entry->loadwop_avg = loadwop_avg;
			__entry->h_nr_running = h_nr_running;
			__entry->cluster_mask = cluster_mask;
			__entry->nr_task = nr_task;
			__entry->load_avg = load_avg;
			__entry->capacity = capacity;
			__entry->acap = acap;
			__entry->scaled_atask = scaled_atask;
			__entry->threshold = threshold;
			),

		TP_printk("cpu[%d]:load=%lu len=%u, cluster[%lx]: nr_task=%d load_avg=%d capacity=%d acap=%d scaled_atask=%d threshold=%d",
			__entry->target,
			__entry->loadwop_avg,
			__entry->h_nr_running,
			__entry->cluster_mask,
			__entry->nr_task,
			__entry->load_avg,
			__entry->capacity,
			__entry->acap,
			__entry->scaled_atask,
			__entry->threshold)
		);
/*
 * Tracepoint for showing adjusted threshold in HMP
 */
TRACE_EVENT(sched_adj_threshold,

		TP_PROTO(int b_threshold, int l_threshold, int l_target,
			int l_cap, int b_target, int b_cap),

		TP_ARGS(b_threshold, l_threshold, l_target,
			l_cap, b_target, b_cap),

		TP_STRUCT__entry(
			__field(int, b_threshold)
			__field(int, l_threshold)
			__field(int, l_target)
			__field(int, l_cap)
			__field(int, b_target)
			__field(int, b_cap)
			),

		TP_fast_assign(
			__entry->b_threshold = b_threshold;
			__entry->l_threshold = l_threshold;
			__entry->l_target = l_target;
			__entry->l_cap = l_cap;
			__entry->b_target = b_target;
			__entry->b_cap = b_cap;
			),

		TP_printk("up=%4d down=%4d L_cpu=%d(%4u) B_cpu=%d(%4u)",
			__entry->b_threshold, __entry->l_threshold,
			__entry->l_target, __entry->l_cap,
			__entry->b_target, __entry->b_cap)
		);

/*
 * Tracepoint for showing tracked cfs runqueue runnable load.
 */
TRACE_EVENT(sched_cfs_runnable_load,

		TP_PROTO(int cpu_id, int cpu_load, int cpu_ntask),

		TP_ARGS(cpu_id, cpu_load, cpu_ntask),

		TP_STRUCT__entry(
			__field(int, cpu_id)
			__field(int, cpu_load)
			__field(int, cpu_ntask)
			),

		TP_fast_assign(
			__entry->cpu_id = cpu_id;
			__entry->cpu_load = cpu_load;
			__entry->cpu_ntask = cpu_ntask;
			),

		TP_printk("cpu-id=%d cfs-load=%4d, cfs-ntask=%2d",
			__entry->cpu_id,
			__entry->cpu_load,
			__entry->cpu_ntask)
		);

/*
 * Tracepoint for profiling runqueue length
 */
TRACE_EVENT(sched_runqueue_length,

		TP_PROTO(int cpu, int length),

		TP_ARGS(cpu, length),

		TP_STRUCT__entry(
			__field(int, cpu)
			__field(int, length)
			),

		TP_fast_assign(
			__entry->cpu = cpu;
			__entry->length = length;
			),

		TP_printk("cpu=%d rq-length=%2d",
			__entry->cpu,
			__entry->length)
	   );

TRACE_EVENT(sched_cfs_length,

		TP_PROTO(int cpu, int length),

		TP_ARGS(cpu, length),

		TP_STRUCT__entry(
			__field(int, cpu)
			__field(int, length)
			),

		TP_fast_assign(
			__entry->cpu = cpu;
			__entry->length = length;
			),

		TP_printk("cpu=%d cfs-length=%2d",
			__entry->cpu,
			__entry->length)
	   );

/*
 * Tracepoint for fork time:
 */
TRACE_EVENT(sched_fork_time,

	TP_PROTO(struct task_struct *parent,
		 struct task_struct *child, unsigned long long dur),

	TP_ARGS(parent, child, dur),

	TP_STRUCT__entry(
		__array(char,	parent_comm,	TASK_COMM_LEN)
		__field(pid_t,	parent_pid)
		__array(char,	child_comm,	TASK_COMM_LEN)
		__field(pid_t,	child_pid)
		__field(unsigned long long,	dur)
	),

	TP_fast_assign(
		memcpy(__entry->parent_comm, parent->comm, TASK_COMM_LEN);
		__entry->parent_pid	= parent->pid;
		memcpy(__entry->child_comm, child->comm, TASK_COMM_LEN);
		__entry->child_pid	= child->pid;
		__entry->dur = dur;
	),

	TP_printk("comm=%s pid=%d child_comm=%s child_pid=%d fork_time=%llu us",
		__entry->parent_comm, __entry->parent_pid,
		__entry->child_comm, __entry->child_pid, __entry->dur)
);


/*
 * Tracepoint for exec:
 */
TRACE_EVENT(sched_process_exec,

	TP_PROTO(struct task_struct *p, pid_t old_pid,
		 struct linux_binprm *bprm),

	TP_ARGS(p, old_pid, bprm),

	TP_STRUCT__entry(
		__string(	filename,	bprm->filename	)
		__field(	pid_t,		pid		)
		__field(	pid_t,		old_pid		)
	),

	TP_fast_assign(
		__assign_str(filename, bprm->filename);
		__entry->pid		= p->pid;
		__entry->old_pid	= old_pid;
	),

	TP_printk("filename=%s pid=%d old_pid=%d", __get_str(filename),
		  __entry->pid, __entry->old_pid)
);

/*
 * XXX the below sched_stat tracepoints only apply to SCHED_OTHER/BATCH/IDLE
 *     adding sched_stat support to SCHED_FIFO/RR would be welcome.
 */
DECLARE_EVENT_CLASS(sched_stat_template,

	TP_PROTO(struct task_struct *tsk, u64 delay),

	TP_ARGS(__perf_task(tsk), __perf_count(delay)),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
		__field( u64,	delay			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid	= tsk->pid;
		__entry->delay	= delay;
	),

	TP_printk("comm=%s pid=%d delay=%Lu [ns]",
			__entry->comm, __entry->pid,
			(unsigned long long)__entry->delay)
);


/*
 * Tracepoint for accounting wait time (time the task is runnable
 * but not actually running due to scheduler contention).
 */
DEFINE_EVENT(sched_stat_template, sched_stat_wait,
	     TP_PROTO(struct task_struct *tsk, u64 delay),
	     TP_ARGS(tsk, delay));

/*
 * Tracepoint for accounting sleep time (time the task is not runnable,
 * including iowait, see below).
 */
DEFINE_EVENT(sched_stat_template, sched_stat_sleep,
	     TP_PROTO(struct task_struct *tsk, u64 delay),
	     TP_ARGS(tsk, delay));

/*
 * Tracepoint for accounting iowait time (time the task is not runnable
 * due to waiting on IO to complete).
 */
DEFINE_EVENT(sched_stat_template, sched_stat_iowait,
	     TP_PROTO(struct task_struct *tsk, u64 delay),
	     TP_ARGS(tsk, delay));

/*
 * Tracepoint for accounting blocked time (time the task is in uninterruptible).
 */
DEFINE_EVENT(sched_stat_template, sched_stat_blocked,
	     TP_PROTO(struct task_struct *tsk, u64 delay),
	     TP_ARGS(tsk, delay));

/*
 * Tracepoint for recording the cause of uninterruptible sleep.
 */
TRACE_EVENT(sched_blocked_reason,

	TP_PROTO(struct task_struct *tsk),

	TP_ARGS(tsk),

	TP_STRUCT__entry(
		__field( pid_t,	pid	)
		__field( void*, caller	)
		__field( bool, io_wait	)
	),

	TP_fast_assign(
		__entry->pid	= tsk->pid;
		__entry->caller = (void*)get_wchan(tsk);
		__entry->io_wait = tsk->in_iowait;
	),

	TP_printk("pid=%d iowait=%d caller=%pS", __entry->pid, __entry->io_wait, __entry->caller)
);

/*
 * Tracepoint for accounting runtime (time the task is executing
 * on a CPU).
 */
DECLARE_EVENT_CLASS(sched_stat_runtime,

	TP_PROTO(struct task_struct *tsk, u64 runtime, u64 vruntime),

	TP_ARGS(tsk, __perf_count(runtime), vruntime),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
		__field( u64,	runtime			)
		__field( u64,	vruntime			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid		= tsk->pid;
		__entry->runtime	= runtime;
		__entry->vruntime	= vruntime;
	),

	TP_printk("comm=%s pid=%d runtime=%Lu [ns] vruntime=%Lu [ns]",
			__entry->comm, __entry->pid,
			(unsigned long long)__entry->runtime,
			(unsigned long long)__entry->vruntime)
);

DEFINE_EVENT(sched_stat_runtime, sched_stat_runtime,
	     TP_PROTO(struct task_struct *tsk, u64 runtime, u64 vruntime),
	     TP_ARGS(tsk, runtime, vruntime));

/*
 * Tracepoint for showing priority inheritance modifying a tasks
 * priority.
 */
TRACE_EVENT(sched_pi_setprio,

	TP_PROTO(struct task_struct *tsk, int newprio),

	TP_ARGS(tsk, newprio),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
		__field( int,	oldprio			)
		__field( int,	newprio			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid		= tsk->pid;
		__entry->oldprio	= tsk->prio;
		__entry->newprio	= newprio;
	),

	TP_printk("comm=%s pid=%d oldprio=%d newprio=%d",
			__entry->comm, __entry->pid,
			__entry->oldprio, __entry->newprio)
);

#ifdef CONFIG_DETECT_HUNG_TASK
TRACE_EVENT(sched_process_hang,
	TP_PROTO(struct task_struct *tsk),
	TP_ARGS(tsk),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid = tsk->pid;
	),

	TP_printk("comm=%s pid=%d", __entry->comm, __entry->pid)
);
#endif /* CONFIG_DETECT_HUNG_TASK */

DECLARE_EVENT_CLASS(sched_move_task_template,

	TP_PROTO(struct task_struct *tsk, int src_cpu, int dst_cpu),

	TP_ARGS(tsk, src_cpu, dst_cpu),

	TP_STRUCT__entry(
		__field( pid_t,	pid			)
		__field( pid_t,	tgid			)
		__field( pid_t,	ngid			)
		__field( int,	src_cpu			)
		__field( int,	src_nid			)
		__field( int,	dst_cpu			)
		__field( int,	dst_nid			)
	),

	TP_fast_assign(
		__entry->pid		= task_pid_nr(tsk);
		__entry->tgid		= task_tgid_nr(tsk);
		__entry->ngid		= task_numa_group_id(tsk);
		__entry->src_cpu	= src_cpu;
		__entry->src_nid	= cpu_to_node(src_cpu);
		__entry->dst_cpu	= dst_cpu;
		__entry->dst_nid	= cpu_to_node(dst_cpu);
	),

	TP_printk("pid=%d tgid=%d ngid=%d src_cpu=%d src_nid=%d dst_cpu=%d dst_nid=%d",
			__entry->pid, __entry->tgid, __entry->ngid,
			__entry->src_cpu, __entry->src_nid,
			__entry->dst_cpu, __entry->dst_nid)
);

/*
 * Tracks migration of tasks from one runqueue to another. Can be used to
 * detect if automatic NUMA balancing is bouncing between nodes
 */
DEFINE_EVENT(sched_move_task_template, sched_move_numa,
	TP_PROTO(struct task_struct *tsk, int src_cpu, int dst_cpu),

	TP_ARGS(tsk, src_cpu, dst_cpu)
);

DEFINE_EVENT(sched_move_task_template, sched_stick_numa,
	TP_PROTO(struct task_struct *tsk, int src_cpu, int dst_cpu),

	TP_ARGS(tsk, src_cpu, dst_cpu)
);

TRACE_EVENT(sched_swap_numa,

	TP_PROTO(struct task_struct *src_tsk, int src_cpu,
		 struct task_struct *dst_tsk, int dst_cpu),

	TP_ARGS(src_tsk, src_cpu, dst_tsk, dst_cpu),

	TP_STRUCT__entry(
		__field( pid_t,	src_pid			)
		__field( pid_t,	src_tgid		)
		__field( pid_t,	src_ngid		)
		__field( int,	src_cpu			)
		__field( int,	src_nid			)
		__field( pid_t,	dst_pid			)
		__field( pid_t,	dst_tgid		)
		__field( pid_t,	dst_ngid		)
		__field( int,	dst_cpu			)
		__field( int,	dst_nid			)
	),

	TP_fast_assign(
		__entry->src_pid	= task_pid_nr(src_tsk);
		__entry->src_tgid	= task_tgid_nr(src_tsk);
		__entry->src_ngid	= task_numa_group_id(src_tsk);
		__entry->src_cpu	= src_cpu;
		__entry->src_nid	= cpu_to_node(src_cpu);
		__entry->dst_pid	= task_pid_nr(dst_tsk);
		__entry->dst_tgid	= task_tgid_nr(dst_tsk);
		__entry->dst_ngid	= task_numa_group_id(dst_tsk);
		__entry->dst_cpu	= dst_cpu;
		__entry->dst_nid	= cpu_to_node(dst_cpu);
	),

	TP_printk("src_pid=%d src_tgid=%d src_ngid=%d src_cpu=%d src_nid=%d dst_pid=%d dst_tgid=%d dst_ngid=%d dst_cpu=%d dst_nid=%d",
			__entry->src_pid, __entry->src_tgid, __entry->src_ngid,
			__entry->src_cpu, __entry->src_nid,
			__entry->dst_pid, __entry->dst_tgid, __entry->dst_ngid,
			__entry->dst_cpu, __entry->dst_nid)
);

/*
 * Tracepoint for waking a polling cpu without an IPI.
 */
TRACE_EVENT(sched_wake_idle_without_ipi,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(	int,	cpu	)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
	),

	TP_printk("cpu=%d", __entry->cpu)
);

TRACE_EVENT(sched_contrib_scale_f,

	TP_PROTO(int cpu, unsigned long freq_scale_factor,
		 unsigned long cpu_scale_factor),

	TP_ARGS(cpu, freq_scale_factor, cpu_scale_factor),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, freq_scale_factor)
		__field(unsigned long, cpu_scale_factor)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->freq_scale_factor = freq_scale_factor;
		__entry->cpu_scale_factor = cpu_scale_factor;
	),

	TP_printk("cpu=%d freq_scale_factor=%lu cpu_scale_factor=%lu",
		  __entry->cpu, __entry->freq_scale_factor,
		  __entry->cpu_scale_factor)
);

#ifdef CONFIG_SMP

#ifdef CONFIG_SCHED_WALT
extern unsigned int sysctl_sched_use_walt_cpu_util;
extern unsigned int sysctl_sched_use_walt_task_util;
extern unsigned int walt_ravg_window;
extern bool walt_disabled;
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

/*
 * Tracepoint for accounting sched averages for tasks.
 */
TRACE_EVENT(sched_load_avg_task,

	TP_PROTO(struct task_struct *tsk, struct sched_avg *avg, void *_ravg),

	TP_ARGS(tsk, avg, _ravg),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN		)
		__field( pid_t,	pid				)
		__field( int,	cpu				)
		__field( unsigned long,	load_avg		)
		__field( unsigned long,	util_avg		)
		__field( unsigned long,	util_avg_pelt		)
		__field( uint64_t,	util_avg_walt		)
		__field( u64,		load_sum		)
		__field( u32,		util_sum		)
		__field( u32,		period_contrib		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->cpu			= task_cpu(tsk);
		__entry->load_avg		= avg->load_avg;
		__entry->util_avg		= avg->util_avg;
		__entry->load_sum		= avg->load_sum;
		__entry->util_sum		= avg->util_sum;
		__entry->period_contrib		= avg->period_contrib;
		__entry->util_avg_pelt  = avg->util_avg;
		__entry->util_avg_walt  = 0;
#ifdef CONFIG_SCHED_WALT
		__entry->util_avg_walt = ((struct ravg*)_ravg)->demand /
					 (walt_ravg_window >> SCHED_CAPACITY_SHIFT);
		if (!walt_disabled && sysctl_sched_use_walt_task_util)
			__entry->util_avg = __entry->util_avg_walt;
#endif
	),
	TP_printk("comm=%s pid=%d cpu=%d load_avg=%lu util_avg=%lu "
			"util_avg_pelt=%lu util_avg_walt=%llu load_sum=%llu"
		  " util_sum=%u period_contrib=%u",
		  __entry->comm,
		  __entry->pid,
		  __entry->cpu,
		  __entry->load_avg,
		  __entry->util_avg,
		  __entry->util_avg_pelt,
		  __entry->util_avg_walt,
		  (u64)__entry->load_sum,
		  (u32)__entry->util_sum,
		  (u32)__entry->period_contrib)
);

/*
 * Tracepoint for accounting sched averages for cpus.
 */
TRACE_EVENT(sched_load_avg_cpu,

	TP_PROTO(int cpu, struct cfs_rq *cfs_rq),

	TP_ARGS(cpu, cfs_rq),

	TP_STRUCT__entry(
		__field( int,	cpu				)
		__field( unsigned long,	load_avg		)
		__field( unsigned long,	util_avg		)
		__field( unsigned long,	util_avg_pelt		)
		__field( uint64_t,		util_avg_walt	)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->load_avg		= cfs_rq->avg.load_avg;
		__entry->util_avg		= cfs_rq->avg.util_avg;
		__entry->util_avg_pelt	= cfs_rq->avg.util_avg;
		__entry->util_avg_walt	= 0;
#ifdef CONFIG_SCHED_WALT
		__entry->util_avg_walt = div64_ul(cpu_rq(cpu)->prev_runnable_sum,
					 walt_ravg_window >> SCHED_CAPACITY_SHIFT);
		if (!walt_disabled && sysctl_sched_use_walt_cpu_util)
			__entry->util_avg		= __entry->util_avg_walt;
#endif
	),

	TP_printk("cpu=%d load_avg=%lu util_avg=%lu "
			  "util_avg_pelt=%lu util_avg_walt=%llu",
		  __entry->cpu, __entry->load_avg, __entry->util_avg,
		  __entry->util_avg_pelt, __entry->util_avg_walt)
);

/*
 * Tracepoint for sched_tune_config settings
 */
TRACE_EVENT(sched_tune_config,

	TP_PROTO(int boost),

	TP_ARGS(boost),

	TP_STRUCT__entry(
		__field( int,	boost		)
	),

	TP_fast_assign(
		__entry->boost 	= boost;
	),

	TP_printk("boost=%d ", __entry->boost)
);

/*
 * Tracepoint for accounting CPU  boosted utilization
 */
TRACE_EVENT(sched_boost_cpu,

	TP_PROTO(int cpu, unsigned long util, long margin),

	TP_ARGS(cpu, util, margin),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( unsigned long,	util			)
		__field(long,		margin			)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
		__entry->util	= util;
		__entry->margin	= margin;
	),

	TP_printk("cpu=%d util=%lu margin=%ld",
		  __entry->cpu,
		  __entry->util,
		  __entry->margin)
);

/*
 * Tracepoint for schedtune_tasks_update
 */
TRACE_EVENT(sched_tune_tasks_update,

	TP_PROTO(struct task_struct *tsk, int cpu, int tasks, int idx,
		int boost, int max_boost,
		int capacity_min, int max_capacity_min),

	TP_ARGS(tsk, cpu, tasks, idx, boost, max_boost,
		capacity_min, max_capacity_min),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid		)
		__field( int,		cpu		)
		__field( int,		tasks		)
		__field( int,		idx		)
		__field( int,		boost		)
		__field( int,		max_boost	)
		__field(int,		capacity_min)
		__field(int,		max_capacity_min)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid		= tsk->pid;
		__entry->cpu 		= cpu;
		__entry->tasks		= tasks;
		__entry->idx 		= idx;
		__entry->boost		= boost;
		__entry->max_boost	= max_boost;
		__entry->capacity_min	= capacity_min;
		__entry->max_capacity_min = max_capacity_min;
	),

	TP_printk("pid=%d comm=%s cpu=%d tasks=%d idx=%d boost=%d max_boost=%d cap_min=%d max_cap_min=%d",
		__entry->pid, __entry->comm,
		__entry->cpu, __entry->tasks, __entry->idx,
		__entry->boost, __entry->max_boost,
		__entry->capacity_min, __entry->max_capacity_min)
);

/*
 * Tracepoint for schedtune_boostgroup_update
 */
TRACE_EVENT(sched_tune_boostgroup_update,

	TP_PROTO(int cpu, int variation, int max_boost),

	TP_ARGS(cpu, variation, max_boost),

	TP_STRUCT__entry(
		__field( int,	cpu		)
		__field( int,	variation	)
		__field( int,	max_boost	)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->variation	= variation;
		__entry->max_boost	= max_boost;
	),

	TP_printk("cpu=%d variation=%d max_boost=%d",
		__entry->cpu, __entry->variation, __entry->max_boost)
);

/*
 * Tracepoint for accounting task boosted utilization
 */
TRACE_EVENT(sched_boost_task,

	TP_PROTO(struct task_struct *tsk, unsigned long util, long margin),

	TP_ARGS(tsk, util, margin),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN		)
		__field( pid_t,		pid			)
		__field( unsigned long,	util			)
		__field( long,		margin			)

	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid	= tsk->pid;
		__entry->util	= util;
		__entry->margin	= margin;
	),

	TP_printk("comm=%s pid=%d util=%lu margin=%ld",
		  __entry->comm, __entry->pid,
		  __entry->util,
		  __entry->margin)
);

/*
 * Tracepoint for find_best_target
 */
TRACE_EVENT(sched_find_best_target,

	TP_PROTO(struct task_struct *tsk, bool prefer_idle,
		unsigned long min_util, int start_cpu,
		int best_idle, int best_active, int target),

	TP_ARGS(tsk, prefer_idle, min_util, start_cpu,
		best_idle, best_active, target),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
		__field( unsigned long,	min_util	)
		__field( bool,	prefer_idle		)
		__field( int,	start_cpu		)
		__field( int,	best_idle		)
		__field( int,	best_active		)
		__field( int,	target			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid		= tsk->pid;
		__entry->min_util	= min_util;
		__entry->prefer_idle	= prefer_idle;
		__entry->start_cpu 	= start_cpu;
		__entry->best_idle	= best_idle;
		__entry->best_active	= best_active;
		__entry->target		= target;
	),

	TP_printk("pid=%d comm=%s prefer_idle=%d start_cpu=%d "
		  "best_idle=%d best_active=%d target=%d",
		__entry->pid, __entry->comm,
		__entry->prefer_idle, __entry->start_cpu,
		__entry->best_idle, __entry->best_active,
		__entry->target)
);

/*
 * Tracepoint for schedtune_tasks_update
 */
TRACE_EVENT(sched_tune_filter,

	TP_PROTO(int nrg_delta, int cap_delta,
		 int nrg_gain,  int cap_gain,
		 int payoff, int region),

	TP_ARGS(nrg_delta, cap_delta, nrg_gain, cap_gain, payoff, region),

	TP_STRUCT__entry(
		__field( int,	nrg_delta	)
		__field( int,	cap_delta	)
		__field( int,	nrg_gain	)
		__field( int,	cap_gain	)
		__field( int,	payoff		)
		__field( int,	region		)
	),

	TP_fast_assign(
		__entry->nrg_delta	= nrg_delta;
		__entry->cap_delta	= cap_delta;
		__entry->nrg_gain	= nrg_gain;
		__entry->cap_gain	= cap_gain;
		__entry->payoff		= payoff;
		__entry->region		= region;
	),

	TP_printk("nrg_delta=%d cap_delta=%d nrg_gain=%d cap_gain=%d payoff=%d region=%d",
		__entry->nrg_delta, __entry->cap_delta,
		__entry->nrg_gain, __entry->cap_gain,
		__entry->payoff, __entry->region)
);

/*
 * Tracepoint for system overutilized flag
 */
TRACE_EVENT(sched_overutilized,

	TP_PROTO(bool overutilized),

	TP_ARGS(overutilized),

	TP_STRUCT__entry(
		__field( bool,	overutilized	)
	),

	TP_fast_assign(
		__entry->overutilized	= overutilized;
	),

	TP_printk("overutilized=%d",
		__entry->overutilized ? 1 : 0)
);

/*
 * MT: Tracepoint for system overutilized indicator
 */
TRACE_EVENT(sched_system_overutilized,

	TP_PROTO(bool overutilized),

	TP_ARGS(overutilized),

	TP_STRUCT__entry(
		__field(bool, overutilized)
	),

	TP_fast_assign(
		__entry->overutilized = overutilized;
	),

	TP_printk("system overutilized=%d",
		__entry->overutilized ? 1 : 0)
);

/*
 * Tracepoint for active balance at check_for_migration
 */
TRACE_EVENT(sched_active_balance,

	TP_PROTO(int src_cpu, int dst_cpu, int pid),

	TP_ARGS(src_cpu, dst_cpu, pid),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, pid)
	),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->pid		= pid;
	),

	TP_printk("src_cpu=%d dst_cpu=%d pid=%d",
		__entry->src_cpu, __entry->dst_cpu, __entry->pid)
);

/*
 * Tracepoint for big task rotation
 */
TRACE_EVENT(sched_big_task_rotation,

	TP_PROTO(int src_cpu, int dst_cpu, int src_pid, int dst_pid),

	TP_ARGS(src_cpu, dst_cpu, src_pid, dst_pid),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, src_pid)
		__field(int, dst_pid)
	),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->src_pid	= src_pid;
		__entry->dst_pid	= dst_pid;
	),

	TP_printk("src_cpu=%d dst_cpu=%d src_pid=%d dst_pid=%d",
		__entry->src_cpu, __entry->dst_cpu,
		__entry->src_pid, __entry->dst_pid)
);

#ifdef CONFIG_SCHED_WALT
struct rq;

TRACE_EVENT(walt_update_task_ravg,

	TP_PROTO(struct task_struct *p, struct rq *rq, int evt,
						u64 wallclock, u64 irqtime),

	TP_ARGS(p, rq, evt, wallclock, irqtime),

	TP_STRUCT__entry(
		__array(	char,	comm,   TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	pid_t,	cur_pid			)
		__field(	u64,	wallclock		)
		__field(	u64,	mark_start		)
		__field(	u64,	delta_m			)
		__field(	u64,	win_start		)
		__field(	u64,	delta			)
		__field(	u64,	irqtime			)
		__field(        int,    evt			)
		__field(unsigned int,	demand			)
		__field(unsigned int,	sum			)
		__field(	 int,	cpu			)
		__field(	u64,	cs			)
		__field(	u64,	ps			)
		__field(uint64_t,	util			)
		__field(	u32,	curr_window		)
		__field(	u32,	prev_window		)
		__field(	u64,	nt_cs			)
		__field(	u64,	nt_ps			)
		__field(	u32,	active_windows		)
	),

	TP_fast_assign(
		__entry->wallclock      = wallclock;
		__entry->win_start      = rq->window_start;
		__entry->delta          = (wallclock - rq->window_start);
		__entry->evt            = evt;
		__entry->cpu            = rq->cpu;
		__entry->cur_pid        = rq->curr->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->mark_start     = p->ravg.mark_start;
		__entry->delta_m        = (wallclock - p->ravg.mark_start);
		__entry->demand         = p->ravg.demand;
		__entry->sum            = p->ravg.sum;
		__entry->irqtime        = irqtime;
		__entry->cs             = rq->curr_runnable_sum;
		__entry->ps             = rq->prev_runnable_sum;
		__entry->util           = rq->prev_runnable_sum << SCHED_CAPACITY_SHIFT;
		do_div(__entry->util, walt_ravg_window);
		__entry->curr_window	= p->ravg.curr_window;
		__entry->prev_window	= p->ravg.prev_window;
		__entry->nt_cs		= rq->nt_curr_runnable_sum;
		__entry->nt_ps		= rq->nt_prev_runnable_sum;
		__entry->active_windows	= p->ravg.active_windows;
	),

	TP_printk("wc %llu ws %llu delta %llu event %d cpu %d cur_pid %d task %d (%s) ms %llu delta %llu demand %u sum %u irqtime %llu"
		" cs %llu ps %llu util %llu cur_window %u prev_window %u active_wins %u"
		, __entry->wallclock, __entry->win_start, __entry->delta,
		__entry->evt, __entry->cpu, __entry->cur_pid,
		__entry->pid, __entry->comm, __entry->mark_start,
		__entry->delta_m, __entry->demand,
		__entry->sum, __entry->irqtime,
		__entry->cs, __entry->ps, __entry->util,
		__entry->curr_window, __entry->prev_window,
		  __entry->active_windows
		)
);

TRACE_EVENT(walt_update_history,

	TP_PROTO(struct rq *rq, struct task_struct *p, u32 runtime, int samples,
			int evt),

	TP_ARGS(rq, p, runtime, samples, evt),

	TP_STRUCT__entry(
		__array(	char,	comm,   TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(unsigned int,	runtime			)
		__field(	 int,	samples			)
		__field(	 int,	evt			)
		__field(	 u64,	demand			)
		__field(	 u64,	walt_avg		)
		__field(unsigned int,	pelt_avg		)
		__array(	 u32,	hist, RAVG_HIST_SIZE_MAX)
		__field(	 int,	cpu			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->runtime        = runtime;
		__entry->samples        = samples;
		__entry->evt            = evt;
		__entry->demand         = p->ravg.demand;
		__entry->walt_avg	= (__entry->demand << SCHED_CAPACITY_SHIFT);
		__entry->walt_avg	= div_u64(__entry->walt_avg,
						  walt_ravg_window);
		__entry->pelt_avg	= p->se.avg.util_avg;
		memcpy(__entry->hist, p->ravg.sum_history,
					RAVG_HIST_SIZE_MAX * sizeof(u32));
		__entry->cpu            = rq->cpu;
	),

	TP_printk("%d (%s): runtime %u samples %d event %d demand %llu"
		" walt %llu pelt %u (hist: %u %u %u %u %u) cpu %d",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->samples, __entry->evt,
		__entry->demand,
		__entry->walt_avg,
		__entry->pelt_avg,
		__entry->hist[0], __entry->hist[1],
		__entry->hist[2], __entry->hist[3],
		__entry->hist[4], __entry->cpu)
);

TRACE_EVENT(walt_migration_update_sum,

	TP_PROTO(struct rq *rq, struct task_struct *p),

	TP_ARGS(rq, p),

	TP_STRUCT__entry(
		__field(int,		cpu			)
		__field(int,		pid			)
		__field(	u64,	cs			)
		__field(	u64,	ps			)
		__field(	s64,	nt_cs			)
		__field(	s64,	nt_ps			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu_of(rq);
		__entry->cs		= rq->curr_runnable_sum;
		__entry->ps		= rq->prev_runnable_sum;
		__entry->nt_cs		= (s64)rq->nt_curr_runnable_sum;
		__entry->nt_ps		= (s64)rq->nt_prev_runnable_sum;
		__entry->pid		= p->pid;
	),

	TP_printk("cpu %d: cs %llu ps %llu nt_cs %lld nt_ps %lld pid %d",
		  __entry->cpu, __entry->cs, __entry->ps,
		  __entry->nt_cs, __entry->nt_ps, __entry->pid)
);
#endif /* CONFIG_SCHED_WALT */

#endif /* CONFIG_SMP */

#ifdef CONFIG_MTK_SCHED_TRACE
#define sched_trace(event) \
TRACE_EVENT(event,                      \
	TP_PROTO(char *strings),                    \
	TP_ARGS(strings),                           \
	TP_STRUCT__entry(                           \
		__array(char,  strings, 128)        \
	),                                          \
	TP_fast_assign(                             \
		memcpy(__entry->strings, strings, 128); \
	),                                          \
	TP_printk("%s", __entry->strings))

sched_trace(sched_log);
/* mtk rt enhancement */
sched_trace(sched_rt);
sched_trace(sched_rt_info);
sched_trace(sched_lb);
sched_trace(sched_lb_info);
sched_trace(sched_eas_energy_calc);

// mtk scheduling interopertion enhancement
sched_trace(sched_interop);
#endif

/*
 * Tracepoint for uclamp
 */
TRACE_EVENT(sched_dvfs_uclamp,
	TP_PROTO(int cpu, unsigned int min, unsigned int max),
	TP_ARGS(cpu, min, max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, min)
		__field(unsigned int, max)
		),
	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->min	= min;
		__entry->max		= max;
		),
	TP_printk("cpu=%d min=%u max=%u",
		__entry->cpu,
		__entry->min,
		__entry->max
		)
);

TRACE_EVENT(uclamp_cpu_get_id,
	TP_PROTO(int cpu, pid_t pid, int clamp_id, int tsk_cvalue,
		int cpu_cvalue),
	TP_ARGS(cpu, pid, clamp_id, tsk_cvalue, cpu_cvalue),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(pid_t, pid)
		__field(int, clamp_id)
		__field(int, tsk_cvalue)
		__field(int, cpu_cvalue)
		),
	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->pid	= pid;
		__entry->clamp_id	= clamp_id;
		__entry->tsk_cvalue	= tsk_cvalue;
		__entry->cpu_cvalue	= cpu_cvalue;
		),
	TP_printk("cpu=%d pid=%d clamp_id=%d tsk_cvalue=%d cpu_cvalue=%d",
		__entry->cpu,
		__entry->pid,
		__entry->clamp_id,
		__entry->tsk_cvalue,
		__entry->cpu_cvalue
		)
);

TRACE_EVENT(uclamp_group_get,
	TP_PROTO(struct task_struct *t, struct cgroup_subsys_state *css,
		int clamp_id, unsigned int clamp_value, int group_id),
	TP_ARGS(t, css, clamp_id, clamp_value, group_id),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, css)
		__field(int, clamp_id)
		__field(unsigned int, clamp_value)
		__field(int, group_id)
		),
	TP_fast_assign(
		__entry->pid = t ? t->pid : -1;
		__entry->css = css ? css->id : -1;
		__entry->clamp_id	= clamp_id;
		__entry->clamp_value		= clamp_value;
		__entry->group_id	= group_id;
		),
	TP_printk("pid=%d css=%d clamp_id=%d clamp_value=%u group_id=%d",
		__entry->pid,
		__entry->css,
		__entry->clamp_id,
		__entry->clamp_value,
		__entry->group_id
		)
);

TRACE_EVENT(uclamp_cpu_update,
	TP_PROTO(int cpu, int clamp_id, int value),
	TP_ARGS(cpu, clamp_id, value),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, clamp_id)
		__field(int, value)
		),
	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->clamp_id	= clamp_id;
		__entry->value		= value;
		),
	TP_printk("cpu=%d clamp_id=%d value=%d",
		__entry->cpu,
		__entry->clamp_id,
		__entry->value
		)
);

/*
 * Traceopint for dvfs governor
 */
TRACE_EVENT(sched_dvfs,
		TP_PROTO(int cpu, int cid, int type, unsigned int cur_freq,
			unsigned int new_freq, int min, int max,
			int thro_type, long long thro_time),
		TP_ARGS(cpu, cid, type, cur_freq, new_freq,
			min, max, thro_type, thro_time),
		TP_STRUCT__entry(
			__field(int, cpu)
			__field(int, cid)
			__field(int, type)
			__field(unsigned int, cur_freq)
			__field(unsigned int, new_freq)
			__field(int, min)
			__field(int, max)
			__field(int, thro_type)
			__field(long long, thro_time)
			),
		TP_fast_assign(
			__entry->cpu		= cpu;
			__entry->cid		= cid;
			__entry->type		= type;
			__entry->cur_freq	= cur_freq;
			__entry->new_freq	= new_freq;
			__entry->min		= min;
			__entry->max		= max;
			__entry->thro_type	= thro_type;
			__entry->thro_time	= thro_time;
			),
		TP_printk("cpu=%d cid=%d type=%d cur=%d new=%d min=%d max=%d thro_type=%d thro_time=%lld",
			__entry->cpu,
			__entry->cid,
			__entry->type,
			__entry->cur_freq,
			__entry->new_freq,
			__entry->min,
			__entry->max,
			__entry->thro_type,
			__entry->thro_time
			)
);

/*
 * Tracepoint for schedutil governor
 */
TRACE_EVENT(sched_util,
		TP_PROTO(int cid, unsigned int next_freq, u64 time),
		TP_ARGS(cid, next_freq, time),
		TP_STRUCT__entry(
			__field(int, cid)
			__field(unsigned int, next_freq)
			__field(u64, time)
			),
		TP_fast_assign(
			__entry->cid		= cid;
			__entry->next_freq	= next_freq;
			__entry->time		= time;
			),
		TP_printk("cid=%d next=%u last_freq_update_time=%lld",
			__entry->cid,
			__entry->next_freq,
			__entry->time
			)
);

/*
 * Tracepoint for walt debug info.
 */
TRACE_EVENT(sched_ctl_walt,
		TP_PROTO(unsigned int user, int walted),
		TP_ARGS(user, walted),
		TP_STRUCT__entry(
			__field(unsigned int, user)
			__field(int, walted)
			),
		TP_fast_assign(
			__entry->user		= user;
			__entry->walted		= walted;
			),
		TP_printk("user_mask=0x%x walted=%d",
			__entry->user,
			__entry->walted
		)
);

/*
 * Tracepoint for average heavy task calculation.
 */
TRACE_EVENT(sched_heavy_task,
		TP_PROTO(const char *s),
		TP_ARGS(s),
		TP_STRUCT__entry(
			__string(s, s)
			),
		TP_fast_assign(
			__assign_str(s, s);
			),
		TP_printk("%s", __get_str(s))
);

TRACE_EVENT(sched_avg_heavy_task,

	TP_PROTO(int last_poll1, int last_poll2,
		int avg, int cluster_id, int max),

	TP_ARGS(last_poll1, last_poll2, avg, cluster_id, max),

	TP_STRUCT__entry(
		__field(int, last_poll1)
		__field(int, last_poll2)
		__field(int, avg)
		__field(int, cid)
		__field(int, max)
	),

	TP_fast_assign(
		__entry->last_poll1 = last_poll1;
		__entry->last_poll2 = last_poll2;
		__entry->avg = avg;
		__entry->cid = cluster_id;
		__entry->max = max;
	),

	TP_printk("last_poll1=%d last_poll2=%d, avg=%d, max:%d, cid:%d",
		__entry->last_poll1,
		__entry->last_poll2,
		__entry->avg,
		__entry->max,
		__entry->cid)
);

TRACE_EVENT(sched_avg_heavy_nr,
	TP_PROTO(int invoker, int nr_heavy,
		long long int diff, int ack_cap, int cpu),

	TP_ARGS(invoker, nr_heavy, diff, ack_cap, cpu),

	TP_STRUCT__entry(
		__field(int, invoker)
		__field(int, nr_heavy)
		__field(long long int, diff)
		__field(int, ack_cap)
		__field(int, cpu)
	),

	TP_fast_assign(
		__entry->invoker = invoker;
		__entry->nr_heavy = nr_heavy;
		__entry->diff = diff;
		__entry->ack_cap = ack_cap;
		__entry->cpu = cpu;
	),

	TP_printk("invoker=%d nr_heavy=%d time diff:%lld ack_cap:%d cpu:%d",
		__entry->invoker,
		__entry->nr_heavy, __entry->diff, __entry->ack_cap, __entry->cpu
	)
);

TRACE_EVENT(sched_avg_heavy_time,
	TP_PROTO(long long int time_period,
		long long int last_get_heavy_time, int cid),

	TP_ARGS(time_period, last_get_heavy_time, cid),

	TP_STRUCT__entry(
		__field(long long int, time_period)
		__field(long long int, last_get_heavy_time)
		__field(int, cid)
	),

	TP_fast_assign(
		__entry->time_period = time_period;
		__entry->last_get_heavy_time = last_get_heavy_time;
		__entry->cid = cid;
	),

	TP_printk("time_period:%lld last_get_heavy_time:%lld cid:%d",
		__entry->time_period, __entry->last_get_heavy_time, __entry->cid
	)
)

TRACE_EVENT(sched_avg_heavy_task_load,
	TP_PROTO(struct task_struct *t),

	TP_ARGS(t),

	TP_STRUCT__entry(
		__array(char,   comm,   TASK_COMM_LEN)
		__field(pid_t,  pid)
		__field(long long int,  load)
	),

	TP_fast_assign(
		memcpy(__entry->comm, t->comm, TASK_COMM_LEN);
		__entry->pid = t->pid;
		__entry->load = t->se.avg.load_avg;
	),

	TP_printk("heavy_task_detect comm:%s pid:%d load:%lld",
		__entry->comm, __entry->pid, __entry->load
	)
)
#endif /* _TRACE_SCHED_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
