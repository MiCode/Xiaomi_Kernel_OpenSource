/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM perf_tracker

#if !defined(_PERF_TRACKER_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _PERF_TRACKER_TRACE_H

#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <perf_tracker_internal.h>

TRACE_EVENT(perf_index_s,
	TP_PROTO(
		unsigned int sf0,
		unsigned int sf1,
		unsigned int sf2,
		int dram_freq,
		int bw_c,
		int bw_g,
		int bw_mm,
		int bw_total,
		int vcore_uv,
		unsigned int cf0,
		unsigned int cf1,
		unsigned int cf2
	),

	TP_ARGS(sf0, sf1, sf2, dram_freq, bw_c, bw_g, bw_mm, bw_total, vcore_uv, cf0, cf1, cf2),

	TP_STRUCT__entry(
		__field(unsigned int, sf0)
		__field(unsigned int, sf1)
		__field(unsigned int, sf2)
		__field(int, dram_freq)
		__field(int, bw_c)
		__field(int, bw_g)
		__field(int, bw_mm)
		__field(int, bw_total)
		__field(int, vcore_uv)
		__field(unsigned int, cf0)
		__field(unsigned int, cf1)
		__field(unsigned int, cf2)
	),

	TP_fast_assign(
		__entry->sf0       =  sf0;
		__entry->sf1       =  sf1;
		__entry->sf2       =  sf2;
		__entry->dram_freq = dram_freq;
		__entry->bw_c      = bw_c;
		__entry->bw_g      = bw_g;
		__entry->bw_mm     = bw_mm;
		__entry->bw_total  = bw_total;
		__entry->vcore_uv  = vcore_uv;
		__entry->cf0       =  cf0;
		__entry->cf1       =  cf1;
		__entry->cf2       =  cf2;
	),

	TP_printk("sched_freq=%u|%u|%u dram_freq=%d bw=%d|%d|%d|%d vcore=%d cpu_mcupm_freq=%u|%u|%u",
		__entry->sf0,
		__entry->sf1,
		__entry->sf2,
		__entry->dram_freq,
		__entry->bw_c,
		__entry->bw_g,
		__entry->bw_mm,
		__entry->bw_total,
		__entry->vcore_uv,
		__entry->cf0,
		__entry->cf1,
		__entry->cf2)
);

TRACE_EVENT(perf_index_l,

	TP_PROTO(
		long free_mem,
		long avail_mem,
		struct mtk_btag_mictx_iostat_struct *iostatptr,
		int *stall
	),

	TP_ARGS(free_mem,
		avail_mem,
		iostatptr,
		stall
	),

	TP_STRUCT__entry(
		__field(long, free_mem)
		__field(long, avail_mem)
		__field(int, io_wl)
		__field(int, io_req_r)
		__field(int, io_all_r)
		__field(int, io_reqsz_r)
		__field(int, io_reqc_r)
		__field(int, io_req_w)
		__field(int, io_all_w)
		__field(int, io_reqsz_w)
		__field(int, io_reqc_w)
		__field(int, io_dur)
		__field(int, io_q_dept)
		__array(int, stall, 8)
	),

	TP_fast_assign(
		__entry->free_mem   = free_mem;
		__entry->avail_mem  = avail_mem;
		__entry->io_wl      = iostatptr->wl;
		__entry->io_req_r   = iostatptr->tp_req_r;
		__entry->io_all_r   = iostatptr->tp_all_r;
		__entry->io_reqsz_r = iostatptr->reqsize_r;
		__entry->io_reqc_r  = iostatptr->reqcnt_r;
		__entry->io_req_w   = iostatptr->tp_req_w;
		__entry->io_all_w   = iostatptr->tp_all_w;
		__entry->io_reqsz_w = iostatptr->reqsize_w;
		__entry->io_reqc_w  = iostatptr->reqcnt_w;
		__entry->io_dur     = iostatptr->duration;
		__entry->io_q_dept  = iostatptr->q_depth;
		memcpy(__entry->stall, stall, sizeof(int)*8);
	),

	TP_printk(
		"free_mem=%ld avail_mem=%ld iostats=%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d stall=%d|%d|%d|%d|%d|%d|%d|%d",
		__entry->free_mem,
		__entry->avail_mem,
		__entry->io_wl,
		__entry->io_req_r,
		__entry->io_all_r,
		__entry->io_reqsz_r,
		__entry->io_reqc_r,
		__entry->io_req_w,
		__entry->io_all_w,
		__entry->io_reqsz_w,
		__entry->io_reqc_w,
		__entry->io_dur,
		__entry->io_q_dept,
		__entry->stall[0],
		__entry->stall[1],
		__entry->stall[2],
		__entry->stall[3],
		__entry->stall[4],
		__entry->stall[5],
		__entry->stall[6],
		__entry->stall[7]
		)
);

TRACE_EVENT(fuel_gauge,
	TP_PROTO(
		int cur,
		int volt,
		int uisoc
	),

	TP_ARGS(cur, volt, uisoc),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, volt)
		__field(int, uisoc)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->volt = volt;
		__entry->uisoc = uisoc;
	),

	TP_printk("cur=%d vol=%d UISOC=%d",
		__entry->cur,
		__entry->volt,
		__entry->uisoc
	)
);

TRACE_EVENT(charger,
	TP_PROTO(
		int temp,
		int volt
	),

	TP_ARGS(temp, volt),

	TP_STRUCT__entry(
		__field(int, temp)
		__field(int, volt)
	),

	TP_fast_assign(
		__entry->temp = temp;
		__entry->volt = volt;
	),

	TP_printk("ICHG=%d IBUS=%d",
		__entry->temp,
		__entry->volt
	)
);

TRACE_EVENT(perf_index_sbin,
	TP_PROTO(u32 *raw_data, u32 lens),
	TP_ARGS(raw_data, lens),
	TP_STRUCT__entry(
		__dynamic_array(u32, raw_data, lens)
		__field(u32, lens)
	),
	TP_fast_assign(
		memcpy(__get_dynamic_array(raw_data), raw_data,
			lens * sizeof(u32));
		__entry->lens = lens;
	),
	TP_printk("data=%s", __print_array(__get_dynamic_array(raw_data),
		__entry->lens, sizeof(u32)))
);

TRACE_EVENT(freq_qos_user_setting,
	TP_PROTO(
		int cid,
		int type,
		int value,
		const char *caller
	),

	TP_ARGS(cid, type, value, caller),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(int, type)
		__field(int, value)
		__string(caller, caller)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->type = type;
		__entry->value = value;
		__assign_str(caller, caller);
	),

	TP_printk("cid=%d type=%d value=%d caller=%s",
		__entry->cid,
		__entry->type,
		__entry->value,
		__get_str(caller)
	)
);

#endif /*_PERF_TRACKER_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE perf_tracker_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
