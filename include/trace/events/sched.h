#undef TRACE_SYSTEM
#define TRACE_SYSTEM sched

#if !defined(_TRACE_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCHED_H

#include <linux/sched.h>
#include <linux/tracepoint.h>
#include <linux/binfmts.h>

#ifdef CONFIG_MT65XX_TRACER
#include <trace/events/mt65xx_mon_trace.h>
#endif

#ifdef CONFIG_MET_SCHED_HMP
void TaskTh(unsigned int B_th,unsigned int L_th);
void HmpStat(struct hmp_statisic *hmp_stats);
void HmpLoad(int big_load_avg, int little_load_avg);
void RqLen(int cpu, int length);
void CfsLen(int cpu, int length);
void RtLen(int cpu, int length);
#endif

#ifdef CONFIG_MTK_SCHED_TRACERS
/* mtk04259: states for tracking I/O & mutex events 
 * notice avoid to conflict with linux/sched.h
 *
 * A bug linux not fixed: 
 * 'K' for TASK_WAKEKILL specified in linux/sched.h
 * but marked 'K' in sched_switch will cause Android systrace parser confused
 * therefore for sched_switch events, these extra states will be printed
 * in the end of each line
 */
#define _MT_TASK_BLOCKED_RTMUX  ( TASK_STATE_MAX << 1)
#define _MT_TASK_BLOCKED_MUTEX  ( TASK_STATE_MAX << 2)
#define _MT_TASK_BLOCKED_IO     ( TASK_STATE_MAX << 3)
#define _MT_TASK_BLOCKED_STATE_MASK ( _MT_TASK_BLOCKED_RTMUX | _MT_TASK_BLOCKED_MUTEX | _MT_TASK_BLOCKED_IO )
#endif
#define _MT_TASK_STATE_MASK		( (TASK_STATE_MAX-1) & ~(TASK_WAKEKILL | TASK_PARKED) )

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
static inline long __trace_sched_switch_state(struct task_struct *p)
{
	long state = p->state;

#ifdef CONFIG_PREEMPT
	/*
	 * For all intents and purposes a preempted task is a running task.
	 */
	if (task_thread_info(p)->preempt_count & PREEMPT_ACTIVE)
		state = TASK_RUNNING | TASK_STATE_MAX;
#endif

#ifdef CONFIG_MTK_SCHED_TRACERS
#ifdef CONFIG_RT_MUTEXES
    if(p->pi_blocked_on)
        state |= _MT_TASK_BLOCKED_RTMUX;
#endif
#ifdef CONFIG_DEBUG_MUTEXES
    if(p->blocked_on) 
        state |= _MT_TASK_BLOCKED_MUTEX;
#endif
    if((p->state & TASK_UNINTERRUPTIBLE) && p->in_iowait)
        state |= _MT_TASK_BLOCKED_IO;
#endif

	return state;
}

# if defined(CONFIG_FAIR_GROUP_SCHED) && defined(CONFIG_MTK_SCHED_TRACERS)
/*
 * legacy cgroup hierarchy depth is no more than 3, and here we limit the
 * size of each load printing no more than 10, 9 chars with a slash '/'.
 * thus, making MTK_FAIR_DBG_SZ = 100 is pretty safe from array overflow,
 * because 100 is much larger than 60, ((3 * 10) * 2), 2 for @prev and @next
 * tasks.
 */
#  define MTK_FAIR_DBG_SZ	100
/*
 * snprintf writes at most @size bytes (including the trailing null bytes
 * ('\0'), so increment 10 to 11
 */
#  define MTK_FAIR_DBG_LEN	(10 + 1)
#  define MTK_FAIR_DBG_DEP	3

static int fair_cgroup_load(char *buf, int cnt, struct task_struct *p)
{
	int loc = cnt;
	int t, depth = 0;
	unsigned long w[MTK_FAIR_DBG_DEP];
	struct sched_entity *se = p->se.parent;

	for (; se && (depth < MTK_FAIR_DBG_DEP); se = se->parent) {
		w[depth++] = se->load.weight;
	}

	switch (p->policy) {
	case SCHED_NORMAL:
		loc += snprintf(&buf[loc], 7, "NORMAL"); break;
	case SCHED_IDLE:
		loc += snprintf(&buf[loc], 5, "IDLE"); break;
	case SCHED_BATCH:
		loc += snprintf(&buf[loc], 6, "BATCH"); break;
	}

	for (depth--; depth >= 0; depth--) {
		t = snprintf(&buf[loc], MTK_FAIR_DBG_LEN, "/%lu", w[depth]);
		if ((t < MTK_FAIR_DBG_LEN) && (t > 0)) {
			loc += t;
		} else {
			loc += snprintf(&buf[loc], 7, "/ERROR");
		}
	}

	t = snprintf(&buf[loc], MTK_FAIR_DBG_LEN, "/%lu", p->se.load.weight);
	if ((t < MTK_FAIR_DBG_LEN) && (t > 0)) {
		loc += t;
	} else {
		loc += snprintf(&buf[loc], 7, "/ERROR");
	}

	return loc;
}

static int is_fair_preempt(char *buf, struct task_struct *prev,
			   struct task_struct *next)
{
	int cnt;
	/* nothing needs to be clarified for RT class or yielding from IDLE */
	if ((task_pid_nr(prev) == 0) || (rt_task(next) || rt_task(prev)))
		return 0;

	/* take care about preemption only */
	if (prev->state &&
	    !(task_thread_info(prev)->preempt_count & PREEMPT_ACTIVE)) {
		return 0;
	}

	memset(buf, 0, MTK_FAIR_DBG_SZ);
	cnt = fair_cgroup_load(buf, 0, prev);
	cnt += snprintf(&buf[cnt], 6, " ==> ");
	fair_cgroup_load(buf, cnt, next);
	return 1;
}
# endif
#endif /* CREATE_TRACE_POINTS */

/*
 * Tracepoint for waking up a task:
 */
DECLARE_EVENT_CLASS(sched_wakeup_template,

	TP_PROTO(struct task_struct *p, int success),

	TP_ARGS(p, success),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
		__field(	int,	success			)
		__field(	int,	target_cpu		)
#ifdef CONFIG_MTK_SCHED_TRACERS
        __field(    long,    state           )
#endif
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
		__entry->success	= success;
		__entry->target_cpu	= task_cpu(p);
#ifdef CONFIG_MTK_SCHED_TRACERS
        __entry->state      =__trace_sched_switch_state(p);
#endif
	)
	TP_perf_assign(
		__perf_task(p);
	),

	TP_printk(
#ifdef CONFIG_MTK_SCHED_TRACERS
            "comm=%s pid=%d prio=%d success=%d target_cpu=%03d state=%s",
#else
            "comm=%s pid=%d prio=%d success=%d target_cpu=%03d",
#endif
		  __entry->comm, __entry->pid, __entry->prio,
		  __entry->success, __entry->target_cpu
#ifdef CONFIG_MTK_SCHED_TRACERS
        ,
		__entry->state & ~TASK_STATE_MAX ?
		  __print_flags(__entry->state & ~TASK_STATE_MAX, "|",
				{ TASK_INTERRUPTIBLE, "S"} , { TASK_UNINTERRUPTIBLE, "D" }, { __TASK_STOPPED, "T" }, { __TASK_TRACED, "t" },
				{ EXIT_ZOMBIE, "Z" }, { EXIT_DEAD, "X" }, { TASK_DEAD, "x" },
                { TASK_WAKEKILL, "K" }, { TASK_WAKING, "W"}, {_MT_TASK_BLOCKED_RTMUX, "r"}, {_MT_TASK_BLOCKED_MUTEX, "m"}, {_MT_TASK_BLOCKED_IO, "d"}) : "R"
#endif
          )
);

DEFINE_EVENT(sched_wakeup_template, sched_wakeup,
	     TP_PROTO(struct task_struct *p, int success),
	     TP_ARGS(p, success));

/*
 * Tracepoint for waking up a new task:
 */
DEFINE_EVENT(sched_wakeup_template, sched_wakeup_new,
	     TP_PROTO(struct task_struct *p, int success),
	     TP_ARGS(p, success));

/*
 * Tracepoint for task switches, performed by the scheduler:
 */
TRACE_EVENT(sched_switch,

	TP_PROTO(struct task_struct *prev,
		 struct task_struct *next),

	TP_ARGS(prev, next),

	TP_STRUCT__entry(
		__array(	char,	prev_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	prev_pid			)
		__field(	int,	prev_prio			)
		__field(	long,	prev_state			)
		__array(	char,	next_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	next_pid			)
		__field(	int,	next_prio			)
#if defined(CONFIG_FAIR_GROUP_SCHED) && defined(CONFIG_MTK_SCHED_TRACERS)
		__field(	int,	fair_preempt			)
		__array(	char,	fair_dbg_buf,	MTK_FAIR_DBG_SZ )
#endif
	),

	TP_fast_assign(
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->prev_pid	= prev->pid;
		__entry->prev_prio	= prev->prio;
		__entry->prev_state	= __trace_sched_switch_state(prev);
		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
		__entry->next_pid	= next->pid;
		__entry->next_prio	= next->prio;
#if defined(CONFIG_FAIR_GROUP_SCHED) && defined(CONFIG_MTK_SCHED_TRACERS)
		__entry->fair_preempt	= is_fair_preempt(__entry->fair_dbg_buf,
							  prev, next);
#endif
	),

	TP_printk(
#ifdef CONFIG_MTK_SCHED_TRACERS
            "prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s%s ==> next_comm=%s next_pid=%d next_prio=%d%s%s %s",
#else
            "prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s%s ==> next_comm=%s next_pid=%d next_prio=%d",
#endif
		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,
		__entry->prev_state & (_MT_TASK_STATE_MASK) ?
	  __print_flags(__entry->prev_state & (_MT_TASK_STATE_MASK), "|",
				{ 1, "S"} , { 2, "D" }, { 4, "T" }, { 8, "t" },
				{ 16, "Z" }, { 32, "X" }, { 64, "x" },
                {128, "K"}, { 256, "W"}) : "R",
		__entry->prev_state & TASK_STATE_MAX ? "+" : "",
		__entry->next_comm, __entry->next_pid, __entry->next_prio
#ifdef CONFIG_MTK_SCHED_TRACERS
        ,
        (__entry->prev_state & (TASK_WAKEKILL | _MT_TASK_BLOCKED_STATE_MASK))?" extra_prev_state=":"",
        __print_flags(__entry->prev_state & (TASK_WAKEKILL | _MT_TASK_BLOCKED_STATE_MASK), "|",
            { TASK_WAKEKILL, "K" },
            { TASK_PARKED, "P" },
            { _MT_TASK_BLOCKED_RTMUX, "r" },
            { _MT_TASK_BLOCKED_MUTEX, "m" },
            { _MT_TASK_BLOCKED_IO, "d" })
# ifdef CONFIG_FAIR_GROUP_SCHED
		, (__entry->fair_preempt ? __entry->fair_dbg_buf : "")
# else
		, ""
# endif
#endif
        )
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
        __field(    long,    state           )
#endif
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
		__entry->orig_cpu	= task_cpu(p);
		__entry->dest_cpu	= dest_cpu;
#ifdef CONFIG_MTK_SCHED_TRACERS
        __entry->state      =__trace_sched_switch_state(p);
#endif
	),

#ifdef CONFIG_MTK_SCHED_TRACERS
	TP_printk("comm=%s pid=%d prio=%d orig_cpu=%d dest_cpu=%d state=%s",
#else
	TP_printk("comm=%s pid=%d prio=%d orig_cpu=%d dest_cpu=%d",
#endif
		  __entry->comm, __entry->pid, __entry->prio,
		  __entry->orig_cpu, __entry->dest_cpu
#ifdef CONFIG_MTK_SCHED_TRACERS
        ,
		__entry->state & ~TASK_STATE_MAX ?
		  __print_flags(__entry->state & ~TASK_STATE_MAX, "|",
				{ TASK_INTERRUPTIBLE, "S"} , { TASK_UNINTERRUPTIBLE, "D" },
                { __TASK_STOPPED, "T" }, { __TASK_TRACED, "t" },
				{ EXIT_ZOMBIE, "Z" }, { EXIT_DEAD, "X" }, { TASK_DEAD, "x" },
                { TASK_WAKEKILL, "K" }, { TASK_WAKING, "W"},
                {_MT_TASK_BLOCKED_RTMUX, "r"}, {_MT_TASK_BLOCKED_MUTEX, "m"},
                {_MT_TASK_BLOCKED_IO, "d"}) : "R"
#endif
          )
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

	TP_ARGS(tsk, delay),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
		__field( u64,	delay			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid	= tsk->pid;
		__entry->delay	= delay;
	)
	TP_perf_assign(
		__perf_count(delay);
		__perf_task(tsk);
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
 * Tracepoint for accounting runtime (time the task is executing
 * on a CPU).
 */
TRACE_EVENT(sched_stat_runtime,

	TP_PROTO(struct task_struct *tsk, u64 runtime, u64 vruntime),

	TP_ARGS(tsk, runtime, vruntime),

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
	)
	TP_perf_assign(
		__perf_count(runtime);
	),

	TP_printk("comm=%s pid=%d runtime=%Lu [ns] vruntime=%Lu [ns]",
			__entry->comm, __entry->pid,
			(unsigned long long)__entry->runtime,
			(unsigned long long)__entry->vruntime)
);

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

#ifdef CONFIG_MT_RT_SCHED_CRIT 
TRACE_EVENT(sched_rt_crit,

	TP_PROTO(int cpu,
		 int rt_throttled),

	TP_ARGS(cpu, rt_throttled),

	TP_STRUCT__entry(
		__field(	int,	cpu				)
		__field(	int,	rt_throttled			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->rt_throttled 	= rt_throttled; 
	),

	TP_printk(
            "cpu=%d rt_throttled=%d",
		__entry->cpu, __entry->rt_throttled)
        
);
#endif

#ifdef CONFIG_MT_RT_SCHED_LOG
TRACE_EVENT(sched_rt_log,

    TP_PROTO(char *strings),

    TP_ARGS(strings),

    TP_STRUCT__entry(
	__array(    char,  strings, 128)
    ),

    TP_fast_assign(
	memcpy(__entry->strings, strings, 128);
    ),

    TP_printk("%s",__entry->strings)
);
#endif 

#ifdef CONFIG_MT_SCHED_NOTICE
TRACE_EVENT(sched_log,

    TP_PROTO(char *strings),

    TP_ARGS(strings),

    TP_STRUCT__entry(
        __array(    char,  strings, 128)
    ),

    TP_fast_assign(
        memcpy(__entry->strings, strings, 128);
    ),

    TP_printk("%s",__entry->strings)
);
#endif

TRACE_EVENT(sched_task_entity_avg,

	TP_PROTO(unsigned int tag, struct task_struct *tsk, struct sched_avg *avg),

	TP_ARGS(tag, tsk, avg),

	TP_STRUCT__entry(
		__field(	u32, 		tag	)
		__array(	char, 		comm,	TASK_COMM_LEN	)
		__field(	pid_t, 		tgid			)
		__field(	pid_t, 		pid			)
		__field(	unsigned long, 	contrib			)
		__field(	unsigned long, 	ratio			)
		__field(	u32,		usage_sum		)
		__field(	unsigned long, 	rq_time			)
		__field(	unsigned long, 	live_time		)
	),

	TP_fast_assign(
		__entry->tag       = tag;
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->tgid      = task_pid_nr(tsk->group_leader);
		__entry->pid       = task_pid_nr(tsk);
		__entry->contrib   = avg->load_avg_contrib;
#ifdef CONFIG_MTK_SCHED_CMP
		__entry->ratio     = avg->load_avg_ratio;
		__entry->usage_sum = avg->usage_avg_sum;
#else
		__entry->ratio     = 0;
		__entry->usage_sum = -1;
#endif
		__entry->rq_time   = avg->runnable_avg_sum;
		__entry->live_time = avg->runnable_avg_period;
	),

	TP_printk("[%d]comm=%s tgid=%d pid=%d contrib=%lu ratio=%lu exe_time=%d rq_time=%lu live_time=%lu",
		  __entry->tag, __entry->comm, __entry->tgid, __entry->pid,
		  __entry->contrib, __entry->ratio, __entry->usage_sum,
		  __entry->rq_time, __entry->live_time)
);

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
 * Tracepoint for showing tracked load contribution.
 */
TRACE_EVENT(sched_task_load_contrib,

	TP_PROTO(struct task_struct *tsk, unsigned long load_contrib),

	TP_ARGS(tsk, load_contrib),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(unsigned long, load_contrib)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid            = tsk->pid;
		__entry->load_contrib   = load_contrib;
	),

	TP_printk("comm=%s pid=%d load_contrib=%lu",
			__entry->comm, __entry->pid,
			__entry->load_contrib)
);

/*
 * Tracepoint for showing tracked task runnable ratio [0..1023].
 */
TRACE_EVENT(sched_task_runnable_ratio,

	TP_PROTO(struct task_struct *tsk, unsigned long ratio),

	TP_ARGS(tsk, ratio),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(unsigned long, ratio)
	),

	TP_fast_assign(
	memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid   = tsk->pid;
		__entry->ratio = ratio;
	),

	TP_printk("comm=%s pid=%d ratio=%lu",
			__entry->comm, __entry->pid,
			__entry->ratio)
);

#ifdef CONFIG_MTK_SCHED_TRACERS
/*
 * Tracepoint for showing the result of task runqueue selection
 */
TRACE_EVENT(sched_select_task_rq,

	TP_PROTO(struct task_struct *tsk, int policy, int prev_cpu, int target_cpu),

	TP_ARGS(tsk, policy, prev_cpu, target_cpu),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, prev_cpu)
		__field(int, target_cpu)
		__field(int, prev_load)
		__field(int, target_load)
		__field(int, task_load)
	),

	TP_fast_assign(
		__entry->pid              = tsk->pid;
		__entry->policy           = policy;
		__entry->prev_cpu         = prev_cpu;
		__entry->target_cpu       = target_cpu;
		__entry->prev_load        = cpu_rq(prev_cpu)->cfs.avg.load_avg_ratio;
		__entry->target_load      = cpu_rq(target_cpu)->cfs.avg.load_avg_ratio;
		__entry->task_load        = tsk->se.avg.load_avg_ratio
	),

	TP_printk("pid=%4d(load:%4d) policy=0x%08x pre-cpu=%d(load:%4d) target=%d(load:%4d)",
			__entry->pid,
			__entry->task_load,
			__entry->policy,
			__entry->prev_cpu,
			__entry->prev_load,
			__entry->target_cpu,
			__entry->target_load)
);
#endif

#ifdef CONFIG_HMP_TRACER
/*
 * Tracepoint for showing tracked migration information
 */
TRACE_EVENT(sched_dynamic_threshold,

	TP_PROTO(struct task_struct *tsk, unsigned int threshold, 
			unsigned int status, int curr_cpu, int target_cpu, int task_load,
			struct clb_stats *B, struct clb_stats *L),

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
		__entry->pid              = tsk->pid;
		__entry->prio             = tsk->prio;
		__entry->threshold        = threshold;
		__entry->status           = status;
		__entry->curr_cpu         = curr_cpu;
		__entry->target_cpu       = target_cpu;
		__entry->curr_load        = cpu_rq(curr_cpu)->cfs.avg.load_avg_ratio;
		__entry->target_load      = cpu_rq(target_cpu)->cfs.avg.load_avg_ratio;
		__entry->task_load        = task_load;
		__entry->B_load_avg       = B->load_avg;
		__entry->L_load_avg       = L->load_avg;
	),

	TP_printk("pid=%4d prio=%d status=0x%4x dyn=%4u task-load=%4d curr-cpu=%d(%4d) target=%d(%4d) L-load-avg=%4d B-load-avg=%4d comm=%s",
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
		__entry->pid              = tsk->pid;
		__entry->prio             = tsk->prio;
		__entry->step             = step;
		__entry->sd_flag          = sd_flag;
		__entry->prev_cpu         = prev_cpu;
		__entry->target_cpu       = target_cpu;
		__entry->prev_load        = cpu_rq(prev_cpu)->cfs.avg.load_avg_ratio;
		__entry->target_load      = cpu_rq(target_cpu)->cfs.avg.load_avg_ratio;
		__entry->task_load        = task_load;
		__entry->B_load_avg       = B->load_avg;
		__entry->L_load_avg       = L->load_avg;
	),

	TP_printk("pid=%4d prio=%d task-load=%4d sd-flag=%2d step=%d pre-cpu=%d(%4d) target=%d(%4d) L-load-avg=%4d B-load-avg=%4d comm=%s",
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

	TP_PROTO(int B_load_avg, int L_load_avg),

	TP_ARGS(B_load_avg, L_load_avg),

	TP_STRUCT__entry(
		__field(int, B_load_avg)
		__field(int, L_load_avg)				
	),

	TP_fast_assign(
		__entry->B_load_avg = B_load_avg;		
		__entry->L_load_avg = L_load_avg;
	),

	TP_printk("B-load-avg=%4d L-load-avg=%4d",
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
 * Tracepoint for cfs runqueue load ratio update
 */
TRACE_EVENT(sched_cfs_load_update,

	TP_PROTO(struct task_struct *tsk, int tsk_load, int tsk_delta, int cpu_id),

	TP_ARGS(tsk, tsk_load, tsk_delta, cpu_id),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, tsk_pid)
		__field(int, tsk_load)
		__field(int, tsk_delta)
		__field(int, cpu_id)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->tsk_pid = tsk->pid;
		__entry->tsk_load = tsk_load;
		__entry->tsk_delta = tsk_delta;
		__entry->cpu_id = cpu_id;
	),

	TP_printk("cpu-id=%d task-pid=%4d task-load=%4d(%d) comm=%s",
			__entry->cpu_id,
			__entry->tsk_pid,
			__entry->tsk_load,
			__entry->tsk_delta,
			__entry->comm)
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

TRACE_EVENT(sched_rt_length,

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

	TP_printk("cpu=%d rt-length=%2d",
			__entry->cpu,                
			__entry->length)
);

/*
 * Tracepoint for profiling power-aware activity
 */
TRACE_EVENT(sched_power_aware_active,

	TP_PROTO(int active_module, int task_pid, int from_cpu, int to_cpu),

	TP_ARGS(active_module, task_pid, from_cpu, to_cpu),

	TP_STRUCT__entry(
		__field(int, active_module)
		__field(int, task_pid)
		__field(int, from_cpu)
		__field(int, to_cpu)
	),

	TP_fast_assign(
		__entry->active_module = active_module;
		__entry->task_pid = task_pid;
		__entry->from_cpu = from_cpu;
		__entry->to_cpu = to_cpu;
	),

	TP_printk("module=%d task-pid=%4d from=%d to=%d",
			__entry->active_module,
			__entry->task_pid,
			__entry->from_cpu,
			__entry->to_cpu)
);

#endif /* CONFIG_HMP_TRACER */

/*
 * Tracepoint for showing tracked rq runnable ratio [0..1023].
 */
TRACE_EVENT(sched_rq_runnable_ratio,

	TP_PROTO(int cpu, unsigned long ratio),

	TP_ARGS(cpu, ratio),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, ratio)
	),

	TP_fast_assign(
		__entry->cpu   = cpu;
		__entry->ratio = ratio;
	),

	TP_printk("cpu=%d ratio=%lu",
			__entry->cpu,
			__entry->ratio)
);

/*
 * Tracepoint for showing tracked rq runnable load.
 */
TRACE_EVENT(sched_rq_runnable_load,

	TP_PROTO(int cpu, u64 load),

	TP_ARGS(cpu, load),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(u64, load)
	),

	TP_fast_assign(
		__entry->cpu  = cpu;
		__entry->load = load;
	),

	TP_printk("cpu=%d load=%llu",
			__entry->cpu,
			__entry->load)
);

/*
 * Tracepoint for showing tracked task cpu usage ratio [0..1023].
 */
TRACE_EVENT(sched_task_usage_ratio,

	TP_PROTO(struct task_struct *tsk, unsigned long ratio),

	TP_ARGS(tsk, ratio),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(unsigned long, ratio)
	),

	TP_fast_assign(
	memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid   = tsk->pid;
		__entry->ratio = ratio;
	),

	TP_printk("comm=%s pid=%d ratio=%lu",
			__entry->comm, __entry->pid,
			__entry->ratio)
);

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

#endif /* _TRACE_SCHED_H */

#ifdef CONFIG_MT_LOAD_BALANCE_PROFILER

TRACE_EVENT(sched_lbprof_status,

    TP_PROTO(char *strings),

    TP_ARGS(strings),

    TP_STRUCT__entry(
	__array(    char,  strings, 128)
    ),

    TP_fast_assign(
	memcpy(__entry->strings, strings, 128);
    ),

    TP_printk("%s",__entry->strings)
);

TRACE_EVENT(sched_lbprof_update,

    TP_PROTO(char *strings),

    TP_ARGS(strings),

    TP_STRUCT__entry(
	__array(    char,  strings, 128)
    ),

    TP_fast_assign(
	memcpy(__entry->strings, strings, 128);
    ),

    TP_printk("%s",__entry->strings)
);

TRACE_EVENT(sched_lbprof_log,

    TP_PROTO(char *strings),

    TP_ARGS(strings),

    TP_STRUCT__entry(
	__array(    char,  strings, 128)
    ),

    TP_fast_assign(
	memcpy(__entry->strings, strings, 128);
    ),

    TP_printk("%s",__entry->strings)
);

#endif
/* This part must be outside protection */
#include <trace/define_trace.h>
