/* Copyright (c) 2012, Free Software Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM  mpdcvs_trace

#if !defined(_TRACE_MPDCVS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MPDCVS_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(msm_mp,

	TP_PROTO(const char *name, int mp_val),

	TP_ARGS(name, mp_val),

	TP_STRUCT__entry(
		__string(name, name)
		__field(int, mp_val)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->mp_val = mp_val;
	),

	TP_printk("ev_name=%s ev_level=%d",
		__get_str(name),
		__entry->mp_val)
);

/* Core function of run_q */

DEFINE_EVENT(msm_mp, msm_mp_runq,

	TP_PROTO(const char *name, int mp_val),

	TP_ARGS(name, mp_val)
);

DEFINE_EVENT(msm_mp, msm_mp_cpusonline,

	TP_PROTO(const char *name, int mp_val),

	TP_ARGS(name, mp_val)
);

DEFINE_EVENT(msm_mp, msm_mp_slacktime,

	TP_PROTO(const char *name, int mp_val),

	TP_ARGS(name, mp_val)
);

DECLARE_EVENT_CLASS(msm_dcvs,

	TP_PROTO(const char *name, const char *cpuid, int val),

	TP_ARGS(name, cpuid, val),

	TP_STRUCT__entry(
		__string(name, name)
		__string(cpuid, cpuid)
		__field(int, val)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__assign_str(cpuid, cpuid);
		__entry->val = val;
	),

	TP_printk("ev_name=%s d_name=%s ev_level=%d",
		__get_str(name),
		__get_str(cpuid),
		__entry->val)
);

/* Core function of dcvs */

DEFINE_EVENT(msm_dcvs, msm_dcvs_idle,

	TP_PROTO(const char *name, const char *cpuid, int val),

	TP_ARGS(name, cpuid, val)
);

DEFINE_EVENT(msm_dcvs, msm_dcvs_iowait,

	TP_PROTO(const char *name, const char *cpuid, int val),

	TP_ARGS(name, cpuid, val)
);

DEFINE_EVENT(msm_dcvs, msm_dcvs_slack_time,

	TP_PROTO(const char *name, const char *cpuid, int val),

	TP_ARGS(name, cpuid, val)
);

DECLARE_EVENT_CLASS(msm_dcvs_scm,

	TP_PROTO(unsigned long cpuid, int ev_type, unsigned long param0,
		unsigned long param1, unsigned long ret0, unsigned long ret1),

	TP_ARGS(cpuid, ev_type, param0, param1, ret0, ret1),

	TP_STRUCT__entry(
		__field(unsigned long, cpuid)
		__field(int, ev_type)
		__field(unsigned long, param0)
		__field(unsigned long, param1)
		__field(unsigned long, ret0)
		__field(unsigned long, ret1)
	),

	TP_fast_assign(
		__entry->cpuid = cpuid;
		__entry->ev_type = ev_type;
		__entry->param0 = param0;
		__entry->param1 = param1;
		__entry->ret0 = ret0;
		__entry->ret1 = ret1;
	),

	TP_printk("dev=%lu ev_type=%d ev_param0=%lu ev_param1=%lu ev_ret0=%lu ev_ret1=%lu",
		__entry->cpuid,
		__entry->ev_type,
		__entry->param0,
		__entry->param1,
		__entry->ret0,
		__entry->ret1)
);

DEFINE_EVENT(msm_dcvs_scm, msm_dcvs_scm_event,

	TP_PROTO(unsigned long cpuid, int ev_type, unsigned long param0,
		unsigned long param1, unsigned long ret0, unsigned long ret1),

	TP_ARGS(cpuid, ev_type, param0, param1, ret0, ret1)
);

#endif /* _TRACE_MPDCVS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
