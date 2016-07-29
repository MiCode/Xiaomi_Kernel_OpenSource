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

#endif /* _SDE_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>
