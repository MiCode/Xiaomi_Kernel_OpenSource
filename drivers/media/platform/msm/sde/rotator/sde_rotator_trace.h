/* Copyright (c) 2014, 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#if !defined(TRACE_SDE_ROTATOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define TRACE_SDE_ROTATOR_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM sde_rotator
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sde_rotator_trace

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(rot_entry_template,
	TP_PROTO(u32 ss_id, u32 sq_id, u32 pr_id, u32 flags,
			u32 src_fmt, u32 src_bw, u32 src_bh,
			u32 src_x, u32 src_y, u32 src_w, u32 src_h,
			u32 dst_fmt, u32 dst_bw, u32 dst_bh,
			u32 dst_x, u32 dst_y, u32 dst_w, u32 dst_h),
	TP_ARGS(ss_id, sq_id, pr_id, flags,
			src_fmt, src_bw, src_bh, src_x, src_y, src_w, src_h,
			dst_fmt, dst_bw, dst_bh, dst_x, dst_y, dst_w, dst_h),
	TP_STRUCT__entry(
			__field(u32, ss_id)
			__field(u32, sq_id)
			__field(u32, pr_id)
			__field(u32, flags)
			__field(u32, src_fmt)
			__field(u16, src_bw)
			__field(u16, src_bh)
			__field(u16, src_x)
			__field(u16, src_y)
			__field(u16, src_w)
			__field(u16, src_h)
			__field(u32, dst_fmt)
			__field(u16, dst_bw)
			__field(u16, dst_bh)
			__field(u16, dst_x)
			__field(u16, dst_y)
			__field(u16, dst_w)
			__field(u16, dst_h)
	),
	TP_fast_assign(
			__entry->ss_id = ss_id;
			__entry->sq_id = sq_id;
			__entry->pr_id = pr_id;
			__entry->flags = flags;
			__entry->src_fmt = src_fmt;
			__entry->src_bw = src_bw;
			__entry->src_bh = src_bh;
			__entry->src_x = src_x;
			__entry->src_y = src_y;
			__entry->src_w = src_w;
			__entry->src_h = src_h;
			__entry->dst_fmt = dst_fmt;
			__entry->dst_bw = dst_bw;
			__entry->dst_bh = dst_bh;
			__entry->dst_x = dst_x;
			__entry->dst_y = dst_y;
			__entry->dst_w = dst_w;
			__entry->dst_h = dst_h;
	),

	TP_printk("%d.%d|%d|%x|%x|%u,%u|%u,%u,%u,%u|%x|%u,%u|%u,%u,%u,%u|",
			__entry->ss_id, __entry->sq_id, __entry->pr_id,
			__entry->flags,
			__entry->src_fmt, __entry->src_bw, __entry->src_bh,
			__entry->src_x, __entry->src_y,
			__entry->src_w, __entry->src_h,
			__entry->dst_fmt, __entry->dst_bw, __entry->dst_bh,
			__entry->dst_x, __entry->dst_y,
			__entry->dst_w, __entry->dst_h)
);

DEFINE_EVENT(rot_entry_template, rot_entry_fence,
	TP_PROTO(u32 ss_id, u32 sq_id, u32 pr_id, u32 flags,
			u32 src_fmt, u32 src_bw, u32 src_bh,
			u32 src_x, u32 src_y, u32 src_w, u32 src_h,
			u32 dst_fmt, u32 dst_bw, u32 dst_bh,
			u32 dst_x, u32 dst_y, u32 dst_w, u32 dst_h),
	TP_ARGS(ss_id, sq_id, pr_id, flags,
			src_fmt, src_bw, src_bh, src_x, src_y, src_w, src_h,
			dst_fmt, dst_bw, dst_bh, dst_x, dst_y, dst_w, dst_h)
);

DEFINE_EVENT(rot_entry_template, rot_entry_commit,
	TP_PROTO(u32 ss_id, u32 sq_id, u32 pr_id, u32 flags,
			u32 src_fmt, u32 src_bw, u32 src_bh,
			u32 src_x, u32 src_y, u32 src_w, u32 src_h,
			u32 dst_fmt, u32 dst_bw, u32 dst_bh,
			u32 dst_x, u32 dst_y, u32 dst_w, u32 dst_h),
	TP_ARGS(ss_id, sq_id, pr_id, flags,
			src_fmt, src_bw, src_bh, src_x, src_y, src_w, src_h,
			dst_fmt, dst_bw, dst_bh, dst_x, dst_y, dst_w, dst_h)
);

DEFINE_EVENT(rot_entry_template, rot_entry_done,
	TP_PROTO(u32 ss_id, u32 sq_id, u32 pr_id, u32 flags,
			u32 src_fmt, u32 src_bw, u32 src_bh,
			u32 src_x, u32 src_y, u32 src_w, u32 src_h,
			u32 dst_fmt, u32 dst_bw, u32 dst_bh,
			u32 dst_x, u32 dst_y, u32 dst_w, u32 dst_h),
	TP_ARGS(ss_id, sq_id, pr_id, flags,
			src_fmt, src_bw, src_bh, src_x, src_y, src_w, src_h,
			dst_fmt, dst_bw, dst_bh, dst_x, dst_y, dst_w, dst_h)
);

TRACE_EVENT(rot_perf_set_qos_luts,
	TP_PROTO(u32 pnum, u32 fmt, u32 lut, bool linear),
	TP_ARGS(pnum, fmt, lut, linear),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, fmt)
			__field(u32, lut)
			__field(bool, linear)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->fmt = fmt;
			__entry->lut = lut;
			__entry->linear = linear;
	),
	TP_printk("pnum=%d fmt=%d lut=0x%x lin:%d",
			__entry->pnum, __entry->fmt,
			__entry->lut, __entry->linear)
);

TRACE_EVENT(rot_perf_set_panic_luts,
	TP_PROTO(u32 pnum, u32 fmt, u32 mode, u32 panic_lut,
		u32 robust_lut),
	TP_ARGS(pnum, fmt, mode, panic_lut, robust_lut),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, fmt)
			__field(u32, mode)
			__field(u32, panic_lut)
			__field(u32, robust_lut)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->fmt = fmt;
			__entry->mode = mode;
			__entry->panic_lut = panic_lut;
			__entry->robust_lut = robust_lut;
	),
	TP_printk("pnum=%d fmt=%d mode=%d luts[0x%x, 0x%x]",
			__entry->pnum, __entry->fmt,
			__entry->mode, __entry->panic_lut,
			__entry->robust_lut)
);

TRACE_EVENT(rot_perf_set_wm_levels,
	TP_PROTO(u32 pnum, u32 use_space, u32 priority_bytes, u32 wm0, u32 wm1,
		u32 wm2, u32 mb_cnt, u32 mb_size),
	TP_ARGS(pnum, use_space, priority_bytes, wm0, wm1, wm2, mb_cnt,
		mb_size),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, use_space)
			__field(u32, priority_bytes)
			__field(u32, wm0)
			__field(u32, wm1)
			__field(u32, wm2)
			__field(u32, mb_cnt)
			__field(u32, mb_size)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->use_space = use_space;
			__entry->priority_bytes = priority_bytes;
			__entry->wm0 = wm0;
			__entry->wm1 = wm1;
			__entry->wm2 = wm2;
			__entry->mb_cnt = mb_cnt;
			__entry->mb_size = mb_size;
	),
	TP_printk(
		"pnum:%d useable_space:%d priority_bytes:%d watermark:[%d | %d | %d] nmb=%d mb_size=%d",
			__entry->pnum, __entry->use_space,
			__entry->priority_bytes, __entry->wm0, __entry->wm1,
			__entry->wm2, __entry->mb_cnt, __entry->mb_size)
);

TRACE_EVENT(rot_perf_set_ot,
	TP_PROTO(u32 pnum, u32 xin_id, u32 rd_lim),
	TP_ARGS(pnum, xin_id, rd_lim),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, xin_id)
			__field(u32, rd_lim)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->xin_id = xin_id;
			__entry->rd_lim = rd_lim;
	),
	TP_printk("pnum:%d xin_id:%d ot:%d",
			__entry->pnum, __entry->xin_id, __entry->rd_lim)
);

TRACE_EVENT(rot_perf_prefill_calc,
	TP_PROTO(u32 pnum, u32 latency_buf, u32 ot, u32 y_buf, u32 y_scaler,
		u32 pp_lines, u32 pp_bytes, u32 post_sc, u32 fbc_bytes,
		u32 prefill_bytes),
	TP_ARGS(pnum, latency_buf, ot, y_buf, y_scaler, pp_lines, pp_bytes,
		post_sc, fbc_bytes, prefill_bytes),
	TP_STRUCT__entry(
			__field(u32, pnum)
			__field(u32, latency_buf)
			__field(u32, ot)
			__field(u32, y_buf)
			__field(u32, y_scaler)
			__field(u32, pp_lines)
			__field(u32, pp_bytes)
			__field(u32, post_sc)
			__field(u32, fbc_bytes)
			__field(u32, prefill_bytes)
	),
	TP_fast_assign(
			__entry->pnum = pnum;
			__entry->latency_buf = latency_buf;
			__entry->ot = ot;
			__entry->y_buf = y_buf;
			__entry->y_scaler = y_scaler;
			__entry->pp_lines = pp_lines;
			__entry->pp_bytes = pp_bytes;
			__entry->post_sc = post_sc;
			__entry->fbc_bytes = fbc_bytes;
			__entry->prefill_bytes = prefill_bytes;
	),
	TP_printk(
		"pnum:%d latency_buf:%d ot:%d y_buf:%d y_scaler:%d pp_lines:%d, pp_bytes=%d post_sc:%d fbc_bytes:%d prefill:%d",
			__entry->pnum, __entry->latency_buf, __entry->ot,
			__entry->y_buf, __entry->y_scaler, __entry->pp_lines,
			__entry->pp_bytes, __entry->post_sc,
			__entry->fbc_bytes, __entry->prefill_bytes)
);

TRACE_EVENT(rot_mark_write,
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
);

TRACE_EVENT(rot_trace_counter,
	TP_PROTO(int pid, char *name, s64 value),
	TP_ARGS(pid, name, value),
	TP_STRUCT__entry(
			__field(int, pid)
			__string(counter_name, name)
			__field(s64, value)
	),
	TP_fast_assign(
			__entry->pid = current->tgid;
			__assign_str(counter_name, name);
			__entry->value = value;
	),
	TP_printk("%d|%s|%lld", __entry->pid,
			__get_str(counter_name), __entry->value)
);

TRACE_EVENT(rot_bw_ao_as_context,
	TP_PROTO(u32 state),
	TP_ARGS(state),
	TP_STRUCT__entry(
			__field(u32, state)
	),
	TP_fast_assign(
			__entry->state = state;
	),
	TP_printk("Rotator bw context %s",
			__entry->state ? "Active Only" : "Active+Sleep")

);

#define SDE_ROT_TRACE_EVTLOG_SIZE	15
TRACE_EVENT(sde_rot_evtlog,
	TP_PROTO(const char *tag, u32 tag_id, u32 cnt, u32 data[]),
	TP_ARGS(tag, tag_id, cnt, data),
	TP_STRUCT__entry(
			__field(int, pid)
			__string(evtlog_tag, tag)
			__field(u32, tag_id)
			__array(u32, data, SDE_ROT_TRACE_EVTLOG_SIZE)
	),
	TP_fast_assign(
			__entry->pid = current->tgid;
			__assign_str(evtlog_tag, tag);
			__entry->tag_id = tag_id;
			if (cnt > SDE_ROT_TRACE_EVTLOG_SIZE)
				cnt = SDE_ROT_TRACE_EVTLOG_SIZE;
			memcpy(__entry->data, data, cnt * sizeof(u32));
			memset(&__entry->data[cnt], 0,
				(SDE_ROT_TRACE_EVTLOG_SIZE - cnt) *
				sizeof(u32));
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

#endif /* if !defined(TRACE_SDE_ROTATOR_H) ||
	*		defined(TRACE_HEADER_MULTI_READ)
	*/

/* This part must be outside protection */
#include <trace/define_trace.h>
