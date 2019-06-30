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
#include <perf_tracker.h>

TRACE_EVENT(perf_index_s,
	TP_PROTO(
		unsigned int sf0,
		unsigned int sf1,
		unsigned int sf2,
		int dram_freq,
		int bw_c,
		int bw_g,
		int bw_mm,
		int bw_total
	),

	TP_ARGS(sf0, sf1, sf2, dram_freq, bw_c, bw_g, bw_mm, bw_total),

	TP_STRUCT__entry(
		__field(unsigned int, sf0)
		__field(unsigned int, sf1)
		__field(unsigned int, sf2)
		__field(int, dram_freq)
		__field(int, bw_c)
		__field(int, bw_g)
		__field(int, bw_mm)
		__field(int, bw_total)
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
	),

	TP_printk("sched_freq=%d|%d|%d dram_freq=%d bw=%d|%d|%d|%d",
		__entry->sf0,
		__entry->sf1,
		__entry->sf2,
		__entry->dram_freq,
		__entry->bw_c,
		__entry->bw_g,
		__entry->bw_mm,
		__entry->bw_total)
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

#endif /*_PERF_TRACKER_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE perf_tracker_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
