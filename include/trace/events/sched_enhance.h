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

sched_trace(sched_eas_energy_calc);
sched_trace(sched_log);
/* mtk scheduling interopertion enhancement*/
sched_trace(sched_interop);

#endif
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

