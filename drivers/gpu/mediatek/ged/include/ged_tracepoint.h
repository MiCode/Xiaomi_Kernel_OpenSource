/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ged

#if !defined(_TRACE_GED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GED_H

#include <linux/tracepoint.h>

/* common tracepoints */
TRACE_EVENT(tracing_mark_write,

	TP_PROTO(int pid, const char *name, long long value),

	TP_ARGS(pid, name, value),

	TP_STRUCT__entry(
		__field(int, pid)
		__string(name, name)
		__field(long long, value)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__assign_str(name, name);
		__entry->value = value;
	),

	TP_printk("C|%d|%s|%lld", __entry->pid, __get_str(name), __entry->value)
);

TRACE_EVENT(GPU_DVFS__Frequency,

	TP_PROTO(unsigned int virtual, unsigned int real),

	TP_ARGS(virtual, real),

	TP_STRUCT__entry(
		__field(unsigned int, virtual)
		__field(unsigned int, real)
	),

	TP_fast_assign(
		__entry->virtual = virtual;
		__entry->real = real;
	),

	TP_printk("virtual=%u, real=%u", __entry->virtual, __entry->real)
);

TRACE_EVENT(GPU_DVFS__Loading,

	TP_PROTO(unsigned int active, unsigned int tiler, unsigned int frag,
		unsigned int comp, unsigned int iter, unsigned int mcu),

	TP_ARGS(active, tiler, frag, comp, iter, mcu),

	TP_STRUCT__entry(
		__field(unsigned int, active)
		__field(unsigned int, tiler)
		__field(unsigned int, frag)
		__field(unsigned int, comp)
		__field(unsigned int, iter)
		__field(unsigned int, mcu)
	),

	TP_fast_assign(
		__entry->active = active;
		__entry->tiler = tiler;
		__entry->frag = frag;
		__entry->comp = comp;
		__entry->iter = iter;
		__entry->mcu = mcu;
	),

	TP_printk("active=%u, tiler=%u, frag=%u, comp=%u, iter=%u, mcu=%u",
		__entry->active, __entry->tiler, __entry->frag, __entry->comp,
		__entry->iter, __entry->mcu)
);

TRACE_EVENT(GPU_DVFS__Policy__Common,

	TP_PROTO(unsigned int commit_type, unsigned int policy_state),

	TP_ARGS(commit_type, policy_state),

	TP_STRUCT__entry(
		__field(unsigned int, commit_type)
		__field(unsigned int, policy_state)
	),

	TP_fast_assign(
		__entry->commit_type = commit_type;
		__entry->policy_state = policy_state;
	),

	TP_printk("commit_type=%u, policy_state=%u",
		__entry->commit_type, __entry->policy_state)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Commit_Reason,

	TP_PROTO(unsigned int same, unsigned int diff),

	TP_ARGS(same, diff),

	TP_STRUCT__entry(
		__field(unsigned int, same)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->same = same;
		__entry->diff = diff;
	),

	TP_printk("same=%u, diff=%u", __entry->same, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Commit_Reason__TID,

	TP_PROTO(int pid, int bqid, int count),

	TP_ARGS(pid, bqid, count),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, bqid)
		__field(int, count)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->bqid = bqid;
		__entry->count = count;
	),

	TP_printk("%d_%d=%d", __entry->pid, __entry->bqid, __entry->count)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Sync_Api,

	TP_PROTO(int hint),

	TP_ARGS(hint),

	TP_STRUCT__entry(
		__field(int, hint)
	),

	TP_fast_assign(
		__entry->hint = hint;
	),

	TP_printk("hint=%d", __entry->hint)
);

/* frame-based policy tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Frequency,

	TP_PROTO(int target, int floor),

	TP_ARGS(target, floor),

	TP_STRUCT__entry(
		__field(int, target)
		__field(int, floor)
	),

	TP_fast_assign(
		__entry->target = target;
		__entry->floor = floor;
	),

	TP_printk("target=%d, floor=%d", __entry->target, __entry->floor)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Workload,

	TP_PROTO(int cur, int avg, int real, int pipe, unsigned int mode),

	TP_ARGS(cur, avg, real, pipe, mode),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, avg)
		__field(int, real)
		__field(int, pipe)
		__field(unsigned int, mode)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->avg = avg;
		__entry->real = real;
		__entry->pipe = pipe;
		__entry->mode = mode;
	),

	TP_printk("cur=%d, avg=%d, real=%d, pipe=%d, mode=%u", __entry->cur,
		__entry->avg, __entry->real, __entry->pipe, __entry->mode)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__GPU_Time,

	TP_PROTO(int cur, int target, int target_hd, int real, int pipe),

	TP_ARGS(cur, target, target_hd, real, pipe),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, target)
		__field(int, target_hd)
		__field(int, real)
		__field(int, pipe)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->target = target;
		__entry->target_hd = target_hd;
		__entry->real = real;
		__entry->pipe = pipe;
	),

	TP_printk("cur=%d, target=%d, target_hd=%d, real=%d, pipe=%d",
		__entry->cur, __entry->target, __entry->target_hd, __entry->real,
		__entry->pipe)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Margin,

	TP_PROTO(int ceil, int cur, int floor),

	TP_ARGS(ceil, cur, floor),

	TP_STRUCT__entry(
		__field(int, ceil)
		__field(int, cur)
		__field(int, floor)
	),

	TP_fast_assign(
		__entry->ceil = ceil;
		__entry->cur = cur;
		__entry->floor = floor;
	),

	TP_printk("ceil=%d, cur=%d, floor=%d", __entry->ceil, __entry->cur, __entry->floor)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Margin__Detail,

	TP_PROTO(unsigned int margin_mode, int target_fps_margin,
		int min_margin_inc_step, int min_margin),

	TP_ARGS(margin_mode, target_fps_margin, min_margin_inc_step, min_margin),

	TP_STRUCT__entry(
		__field(unsigned int, margin_mode)
		__field(int, target_fps_margin)
		__field(int, min_margin_inc_step)
		__field(int, min_margin)
	),

	TP_fast_assign(
		__entry->margin_mode = margin_mode;
		__entry->target_fps_margin = target_fps_margin;
		__entry->min_margin_inc_step = min_margin_inc_step;
		__entry->min_margin = min_margin;
	),

	TP_printk("margin_mode=%u, target_fps_margin=%d, min_margin_inc_step=%d, min_margin=%d",
		__entry->margin_mode, __entry->target_fps_margin,
		__entry->min_margin_inc_step, __entry->min_margin)
);

/* loading-based policy tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Opp,

	TP_PROTO(int target),

	TP_ARGS(target),

	TP_STRUCT__entry(
		__field(int, target)
	),

	TP_fast_assign(
		__entry->target = target;
	),

	TP_printk("target=%d", __entry->target)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Loading,

	TP_PROTO(unsigned int cur, unsigned int mode),

	TP_ARGS(cur, mode),

	TP_STRUCT__entry(
		__field(unsigned int, cur)
		__field(unsigned int, mode)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->mode = mode;
	),

	TP_printk("cur=%u, mode=%u", __entry->cur, __entry->mode)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Bound,

	TP_PROTO(int ultra_high, int high, int low, int ultra_low),

	TP_ARGS(ultra_high, high, low, ultra_low),

	TP_STRUCT__entry(
		__field(int, ultra_high)
		__field(int, high)
		__field(int, low)
		__field(int, ultra_low)
	),

	TP_fast_assign(
		__entry->ultra_high = ultra_high;
		__entry->high = high;
		__entry->low = low;
		__entry->ultra_low = ultra_low;
	),

	TP_printk("ultra_high=%d, high=%d, low=%d, ultra_low=%d",
		__entry->ultra_high, __entry->high, __entry->low, __entry->ultra_low)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__GPU_Time,

	TP_PROTO(int cur, int target, int target_hd, int complete, int uncomplete),

	TP_ARGS(cur, target, target_hd, complete, uncomplete),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, target)
		__field(int, target_hd)
		__field(int, complete)
		__field(int, uncomplete)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->target = target;
		__entry->target_hd = target_hd;
		__entry->complete = complete;
		__entry->uncomplete = uncomplete;
	),

	TP_printk("cur=%d, target=%d, target_hd=%d, complete=%d, uncomplete=%d",
		__entry->cur, __entry->target, __entry->target_hd, __entry->complete,
		__entry->uncomplete)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Margin,

	TP_PROTO(int ceil, int cur, int floor),

	TP_ARGS(ceil, cur, floor),

	TP_STRUCT__entry(
		__field(int, ceil)
		__field(int, cur)
		__field(int, floor)
	),

	TP_fast_assign(
		__entry->ceil = ceil;
		__entry->cur = cur;
		__entry->floor = floor;
	),

	TP_printk("ceil=%d, cur=%d, floor=%d", __entry->ceil, __entry->cur, __entry->floor)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Margin__Detail,

	TP_PROTO(unsigned int margin_mode, int margin_step, int min_margin),

	TP_ARGS(margin_mode, margin_step, min_margin),

	TP_STRUCT__entry(
		__field(unsigned int, margin_mode)
		__field(int, margin_step)
		__field(int, min_margin)
	),

	TP_fast_assign(
		__entry->margin_mode = margin_mode;
		__entry->margin_step = margin_step;
		__entry->min_margin = min_margin;
	),

	TP_printk("margin_mode=%u, margin_step=%d, min_margin=%d",
		__entry->margin_mode, __entry->margin_step,
		__entry->min_margin)
);

/* DCS tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__DCS,

	TP_PROTO(int max_core, int current_core),

	TP_ARGS(max_core, current_core),

	TP_STRUCT__entry(
		__field(int, max_core)
		__field(int, current_core)
	),

	TP_fast_assign(
		__entry->max_core = max_core;
		__entry->current_core = current_core;
	),

	TP_printk("max_core=%d, current_core=%d", __entry->max_core, __entry->current_core)
);

TRACE_EVENT(GPU_DVFS__Policy__DCS__Detail,

	TP_PROTO(unsigned int core_mask),

	TP_ARGS(core_mask),

	TP_STRUCT__entry(
		__field(unsigned int, core_mask)
	),

	TP_fast_assign(
		__entry->core_mask = core_mask;
	),

	TP_printk("core_mask=%u", __entry->core_mask)
);

#endif /* _TRACE_GED_H */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/mediatek/ged/include
#define TRACE_INCLUDE_FILE ged_tracepoint
#include <trace/define_trace.h>
