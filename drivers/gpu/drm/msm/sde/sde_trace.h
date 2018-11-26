/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
		u32 lut, u32 lut_usage),
	TP_ARGS(pnum, fmt, rt, fl, lut, lut_usage),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, fmt)
			__field(bool, rt)
			__field(u32, fl)
			__field(u64, lut)
			__field(u32, lut_usage)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->fmt = fmt;
			__entry->rt = rt;
			__entry->fl = fl;
			__entry->lut = lut;
			__entry->lut_usage = lut_usage;
	),
	TP_printk("pnum=%d fmt=%x rt=%d fl=%d lut=0x%llx lut_usage=%d",
			__entry->pnum, __entry->fmt,
			__entry->rt, __entry->fl,
			__entry->lut, __entry->lut_usage)
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
	TP_PROTO(int client, u32 bus_id, unsigned long long ab_quota,
	unsigned long long ib_quota),
	TP_ARGS(client, bus_id, ab_quota, ib_quota),
	TP_STRUCT__entry(
			__field(int, client)
			__field(u32, bus_id);
			__field(u64, ab_quota)
			__field(u64, ib_quota)
	),
	TP_fast_assign(
			__entry->client = client;
			__entry->bus_id = bus_id;
			__entry->ab_quota = ab_quota;
			__entry->ib_quota = ib_quota;
	),
	TP_printk("Request client:%d bus_id:%d ab=%llu ib=%llu",
			__entry->client,
			__entry->bus_id,
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

TRACE_EVENT(sde_encoder_underrun,
	TP_PROTO(u32 enc_id, u32 underrun_cnt),
	TP_ARGS(enc_id, underrun_cnt),
	TP_STRUCT__entry(
			__field(u32, enc_id)
			__field(u32, underrun_cnt)
	),
	TP_fast_assign(
			__entry->enc_id = enc_id;
			__entry->underrun_cnt = underrun_cnt;

	),
	TP_printk("enc:%d underrun_cnt:%d", __entry->enc_id,
		__entry->underrun_cnt)
);

TRACE_EVENT(tracing_mark_write,
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

#define SDE_TRACE_EVTLOG_SIZE	15
TRACE_EVENT(sde_evtlog,
	TP_PROTO(const char *tag, u32 tag_id, u32 cnt, u32 data[]),
	TP_ARGS(tag, tag_id, cnt, data),
	TP_STRUCT__entry(
			__field(int, pid)
			__string(evtlog_tag, tag)
			__field(u32, tag_id)
			__array(u32, data, SDE_TRACE_EVTLOG_SIZE)
	),
	TP_fast_assign(
			__entry->pid = current->tgid;
			__assign_str(evtlog_tag, tag);
			__entry->tag_id = tag_id;
			if (cnt > SDE_TRACE_EVTLOG_SIZE)
				cnt = SDE_TRACE_EVTLOG_SIZE;
			memcpy(__entry->data, data, cnt * sizeof(u32));
			memset(&__entry->data[cnt], 0,
				(SDE_TRACE_EVTLOG_SIZE - cnt) * sizeof(u32));
	),
	TP_printk("%d|%s:%d|%x|%x|%x|%x|%x|%x|%x|%x|%x|%x|%x|%x|%x|%x|%x",
			__entry->pid, __get_str(evtlog_tag),
			__entry->tag_id,
			__entry->data[0], __entry->data[1],
			__entry->data[2], __entry->data[3],
			__entry->data[4], __entry->data[5],
			__entry->data[6], __entry->data[7],
			__entry->data[8], __entry->data[9],
			__entry->data[10], __entry->data[11],
			__entry->data[12], __entry->data[13],
			__entry->data[14])
)

TRACE_EVENT(sde_perf_crtc_update,
	TP_PROTO(u32 crtc,
			u64 bw_ctl_mnoc, u64 per_pipe_ib_mnoc,
			u64 bw_ctl_llcc, u64 per_pipe_ib_llcc,
			u64 bw_ctl_ebi, u64 per_pipe_ib_ebi,
			u32 core_clk_rate, bool stop_req,
			u32 update_bus, u32 update_clk, int params),
	TP_ARGS(crtc,
		bw_ctl_mnoc, per_pipe_ib_mnoc,
		bw_ctl_llcc, per_pipe_ib_llcc,
		bw_ctl_ebi, per_pipe_ib_ebi,
		core_clk_rate, stop_req,
		update_bus, update_clk, params),
	TP_STRUCT__entry(
			__field(u32, crtc)
			__field(u64, bw_ctl_mnoc)
			__field(u64, per_pipe_ib_mnoc)
			__field(u64, bw_ctl_llcc)
			__field(u64, per_pipe_ib_llcc)
			__field(u64, bw_ctl_ebi)
			__field(u64, per_pipe_ib_ebi)
			__field(u32, core_clk_rate)
			__field(bool, stop_req)
			__field(u32, update_bus)
			__field(u32, update_clk)
			__field(int, params)
	),
	TP_fast_assign(
			__entry->crtc = crtc;
			__entry->bw_ctl_mnoc = bw_ctl_mnoc;
			__entry->per_pipe_ib_mnoc = per_pipe_ib_mnoc;
			__entry->bw_ctl_llcc = bw_ctl_llcc;
			__entry->per_pipe_ib_llcc = per_pipe_ib_llcc;
			__entry->bw_ctl_ebi = bw_ctl_ebi;
			__entry->per_pipe_ib_ebi = per_pipe_ib_ebi;
			__entry->core_clk_rate = core_clk_rate;
			__entry->stop_req = stop_req;
			__entry->update_bus = update_bus;
			__entry->update_clk = update_clk;
			__entry->params = params;
	),
	 TP_printk(
		"crtc=%d mnoc=[%llu %llu] llcc=[%llu %llu] ebi=[%llu %llu] clk=%u stop=%d ubus=%d uclk=%d %d",
			__entry->crtc,
			__entry->bw_ctl_mnoc,
			__entry->per_pipe_ib_mnoc,
			__entry->bw_ctl_llcc,
			__entry->per_pipe_ib_llcc,
			__entry->bw_ctl_ebi,
			__entry->per_pipe_ib_ebi,
			__entry->core_clk_rate,
			__entry->stop_req,
			__entry->update_bus,
			__entry->update_clk,
			__entry->params)
);

TRACE_EVENT(sde_perf_calc_crtc,
	TP_PROTO(u32 crtc,
			u64 bw_ctl_mnoc,
			u64 bw_ctl_llcc,
			u64 bw_ctl_ebi,
			u64 ib_mnoc,
			u64 ib_llcc,
			u64 ib_ebi,
			u32 core_clk_rate
			),
	TP_ARGS(crtc,
			bw_ctl_mnoc,
			bw_ctl_llcc,
			bw_ctl_ebi,
			ib_mnoc,
			ib_llcc,
			ib_ebi,
			core_clk_rate),
	TP_STRUCT__entry(
			__field(u32, crtc)
			__field(u64, bw_ctl_mnoc)
			__field(u64, bw_ctl_llcc)
			__field(u64, bw_ctl_ebi)
			__field(u64, ib_mnoc)
			__field(u64, ib_llcc)
			__field(u64, ib_ebi)
			__field(u32, core_clk_rate)

	),
	TP_fast_assign(
			__entry->crtc = crtc;
			__entry->bw_ctl_mnoc = bw_ctl_mnoc;
			__entry->bw_ctl_llcc = bw_ctl_llcc;
			__entry->bw_ctl_ebi = bw_ctl_ebi;
			__entry->ib_mnoc = ib_mnoc;
			__entry->ib_llcc = ib_llcc;
			__entry->ib_ebi = ib_ebi;
			__entry->core_clk_rate = core_clk_rate;
	),
	 TP_printk(
		"crtc=%d mnoc=[%llu, %llu] llcc=[%llu %llu] ebi=[%llu, %llu] clk_rate=%u",
			__entry->crtc,
			__entry->bw_ctl_mnoc,
			__entry->ib_mnoc,
			__entry->bw_ctl_llcc,
			__entry->ib_llcc,
			__entry->bw_ctl_ebi,
			__entry->ib_ebi,
			__entry->core_clk_rate)
);


#define SDE_ATRACE_END(name) trace_tracing_mark_write(current->tgid, name, 0)
#define SDE_ATRACE_BEGIN(name) trace_tracing_mark_write(current->tgid, name, 1)
#define SDE_ATRACE_FUNC() SDE_ATRACE_BEGIN(__func__)

#define SDE_ATRACE_INT(name, value) \
	trace_sde_trace_counter(current->tgid, name, value)

#endif /* _SDE_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>
