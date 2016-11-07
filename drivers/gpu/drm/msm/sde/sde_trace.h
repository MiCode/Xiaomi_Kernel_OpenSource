/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#if !defined(_SDE_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _SDE_TRACE_H_

#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM sde
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sde_trace

TRACE_EVENT(sde_perf_set_qos_luts,
	TP_PROTO(u32 pnum, u32 fmt, bool rt, u32 fl,
		u32 lut, bool linear),
	TP_ARGS(pnum, fmt, rt, fl, lut, linear),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, fmt)
			__field(bool, rt)
			__field(u32, fl)
			__field(u32, lut)
			__field(bool, linear)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->fmt = fmt;
			__entry->rt = rt;
			__entry->fl = fl;
			__entry->lut = lut;
			__entry->linear = linear;
	),
	TP_printk("pnum=%d fmt=%x rt=%d fl=%d lut=0x%x lin=%d",
			__entry->pnum, __entry->fmt,
			__entry->rt, __entry->fl,
			__entry->lut, __entry->linear)
);

TRACE_EVENT(sde_perf_set_danger_luts,
	TP_PROTO(u32 pnum, u32 fmt, u32 mode, u32 danger_lut,
		u32 safe_lut),
	TP_ARGS(pnum, fmt, mode, danger_lut, safe_lut),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, fmt)
			__field(u32, mode)
			__field(u32, danger_lut)
			__field(u32, safe_lut)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->fmt = fmt;
			__entry->mode = mode;
			__entry->danger_lut = danger_lut;
			__entry->safe_lut = safe_lut;
	),
	TP_printk("pnum=%d fmt=%x mode=%d luts[0x%x, 0x%x]",
			__entry->pnum, __entry->fmt,
			__entry->mode, __entry->danger_lut,
			__entry->safe_lut)
);

TRACE_EVENT(sde_perf_set_ot,
	TP_PROTO(u32 pnum, u32 xin_id, u32 rd_lim, u32 vbif_idx),
	TP_ARGS(pnum, xin_id, rd_lim, vbif_idx),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, xin_id)
			__field(u32, rd_lim)
			__field(u32, vbif_idx)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->xin_id = xin_id;
			__entry->rd_lim = rd_lim;
			__entry->vbif_idx = vbif_idx;
	),
	TP_printk("pnum:%d xin_id:%d ot:%d vbif:%d",
			__entry->pnum, __entry->xin_id, __entry->rd_lim,
			__entry->vbif_idx)
)

TRACE_EVENT(sde_perf_update_bus,
	TP_PROTO(int client, unsigned long long ab_quota,
	unsigned long long ib_quota),
	TP_ARGS(client, ab_quota, ib_quota),
	TP_STRUCT__entry(
			__field(int, client)
			__field(u64, ab_quota)
			__field(u64, ib_quota)
	),
	TP_fast_assign(
			__entry->client = client;
			__entry->ab_quota = ab_quota;
			__entry->ib_quota = ib_quota;
	),
	TP_printk("Request client:%d ab=%llu ib=%llu",
			__entry->client,
			__entry->ab_quota,
			__entry->ib_quota)
)


TRACE_EVENT(sde_cmd_release_bw,
	TP_PROTO(u32 crtc_id),
	TP_ARGS(crtc_id),
	TP_STRUCT__entry(
			__field(u32, crtc_id)
	),
	TP_fast_assign(
			__entry->crtc_id = crtc_id;
	),
	TP_printk("crtc:%d", __entry->crtc_id)
);

TRACE_EVENT(sde_mark_write,
	TP_PROTO(int pid, const char *name, bool trace_begin),
	TP_ARGS(pid, name, trace_begin),
	TP_STRUCT__entry(
			__field(int, pid)
			__string(trace_name, name)
			__field(bool, trace_begin)
	),
	TP_fast_assign(
			__entry->pid = pid;
			__assign_str(trace_name, name);
			__entry->trace_begin = trace_begin;
	),
	TP_printk("%s|%d|%s", __entry->trace_begin ? "B" : "E",
		__entry->pid, __get_str(trace_name))
)

TRACE_EVENT(sde_trace_counter,
	TP_PROTO(int pid, char *name, int value),
	TP_ARGS(pid, name, value),
	TP_STRUCT__entry(
			__field(int, pid)
			__string(counter_name, name)
			__field(int, value)
	),
	TP_fast_assign(
			__entry->pid = current->tgid;
			__assign_str(counter_name, name);
			__entry->value = value;
	),
	TP_printk("%d|%s|%d", __entry->pid,
			__get_str(counter_name), __entry->value)
)

TRACE_EVENT(sde_evtlog,
	TP_PROTO(const char *tag, u32 tag_id, u64 value1, u64 value2),
	TP_ARGS(tag, tag_id, value1, value2),
	TP_STRUCT__entry(
			__field(int, pid)
			__string(evtlog_tag, tag)
			__field(u32, tag_id)
			__field(u64, value1)
			__field(u64, value2)
	),
	TP_fast_assign(
			__entry->pid = current->tgid;
			__assign_str(evtlog_tag, tag);
			__entry->tag_id = tag_id;
			__entry->value1 = value1;
			__entry->value2 = value2;
	),
	TP_printk("%d|%s:%d|%llu|%llu", __entry->pid, __get_str(evtlog_tag),
			__entry->tag_id, __entry->value1, __entry->value2)
)

#define SDE_ATRACE_END(name) trace_sde_mark_write(current->tgid, name, 0)
#define SDE_ATRACE_BEGIN(name) trace_sde_mark_write(current->tgid, name, 1)
#define SDE_ATRACE_FUNC() SDE_ATRACE_BEGIN(__func__)

#define SDE_ATRACE_INT(name, value) \
	trace_sde_trace_counter(current->tgid, name, value)

#endif /* _SDE_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>
