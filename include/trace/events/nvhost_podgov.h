/*
 * include/trace/events/nvhost_podgov.h
 *
 * Nvhost event logging to ftrace.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM nvhost_podgov

#if !defined(_TRACE_NVHOST_PODGOV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVHOST_PODGOV_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(podgov_update_freq,
	TP_PROTO(unsigned long old_freq, unsigned long new_freq),

	TP_ARGS(old_freq, new_freq),

	TP_STRUCT__entry(
		__field(unsigned long, old_freq)
		__field(unsigned long, new_freq)
	),

	TP_fast_assign(
		__entry->old_freq = old_freq;
		__entry->new_freq = new_freq;
	),

	TP_printk("old_freq=%lu, new_freq=%lu",
	  __entry->old_freq, __entry->new_freq)
);

DEFINE_EVENT(podgov_update_freq, podgov_do_scale,
	TP_PROTO(unsigned long old_freq, unsigned long new_freq),
	TP_ARGS(old_freq, new_freq)
);

DEFINE_EVENT(podgov_update_freq, podgov_scaling_state_check,
	TP_PROTO(unsigned long old_freq, unsigned long new_freq),
	TP_ARGS(old_freq, new_freq)
);

DEFINE_EVENT(podgov_update_freq, podgov_estimate_freq,
	TP_PROTO(unsigned long old_freq, unsigned long new_freq),
	TP_ARGS(old_freq, new_freq)
);

DEFINE_EVENT(podgov_update_freq, podgov_clocks_handler,
	TP_PROTO(unsigned long old_freq, unsigned long new_freq),
	TP_ARGS(old_freq, new_freq)
);

TRACE_EVENT(podgov_enabled,
	TP_PROTO(int enable),

	TP_ARGS(enable),

	TP_STRUCT__entry(
		__field(int, enable)
	),

	TP_fast_assign(
		__entry->enable = enable;
	),

	TP_printk("scaling_enabled=%d", __entry->enable)
);

TRACE_EVENT(podgov_set_user_ctl,
	TP_PROTO(int user_ctl),

	TP_ARGS(user_ctl),

	TP_STRUCT__entry(
		__field(int, user_ctl)
	),

	TP_fast_assign(
		__entry->user_ctl = user_ctl;
	),

	TP_printk("userspace control=%d", __entry->user_ctl)
);

TRACE_EVENT(podgov_set_freq_request,
	TP_PROTO(int freq_request),

	TP_ARGS(freq_request),

	TP_STRUCT__entry(
		__field(int, freq_request)
	),

	TP_fast_assign(
		__entry->freq_request = freq_request;
	),

	TP_printk("freq_request=%d", __entry->freq_request)
);

TRACE_EVENT(podgov_busy,
	TP_PROTO(unsigned long busyness),

	TP_ARGS(busyness),

	TP_STRUCT__entry(
		__field(unsigned long, busyness)
	),

	TP_fast_assign(
		__entry->busyness = busyness;
	),

	TP_printk("busyness=%lu", __entry->busyness)
);

TRACE_EVENT(podgov_hint,
	TP_PROTO(long idle_estimate, int hint),

	TP_ARGS(idle_estimate, hint),

	TP_STRUCT__entry(
		__field(long, idle_estimate)
		__field(int, hint)
	),

	TP_fast_assign(
		__entry->idle_estimate = idle_estimate;
		__entry->hint = hint;
	),

	TP_printk("podgov: idle %ld, hint %d", __entry->idle_estimate,
		__entry->hint)
);

TRACE_EVENT(podgov_idle,
	TP_PROTO(unsigned long idleness),

	TP_ARGS(idleness),

	TP_STRUCT__entry(
		__field(unsigned long, idleness)
	),

	TP_fast_assign(
		__entry->idleness = idleness;
	),

	TP_printk("idleness=%lu", __entry->idleness)
);

TRACE_EVENT(podgov_print_target,
	TP_PROTO(long busy, int avg_busy, long curr, long target, int hint,
		int avg_hint),

	TP_ARGS(busy, avg_busy, curr, target, hint, avg_hint),

	TP_STRUCT__entry(
		__field(long, busy)
		__field(int, avg_busy)
		__field(long, curr)
		__field(long, target)
		__field(int, hint)
		__field(int, avg_hint)
	),

	TP_fast_assign(
		__entry->busy = busy;
		__entry->avg_busy = avg_busy;
		__entry->curr = curr;
		__entry->target = target;
		__entry->hint = hint;
		__entry->avg_hint = avg_hint;
	),

	TP_printk("podgov: busy %ld <%d>, curr %ld, t %ld, hint %d <%d>\n",
		__entry->busy, __entry->avg_busy, __entry->curr,
		__entry->target, __entry->hint, __entry->avg_hint)
);

TRACE_EVENT(podgov_stats,
	TP_PROTO(int fast_up_count, int slow_down_count, unsigned int idle_min,
		unsigned int idle_max),

	TP_ARGS(fast_up_count, slow_down_count, idle_min, idle_max),

	TP_STRUCT__entry(
		__field(int, fast_up_count)
		__field(int, slow_down_count)
		__field(unsigned int, idle_min)
		__field(unsigned int, idle_max)
	),

	TP_fast_assign(
		__entry->fast_up_count = fast_up_count;
		__entry->slow_down_count = slow_down_count;
		__entry->idle_min = idle_min;
		__entry->idle_max = idle_max;
	),

	TP_printk("podgov stats: + %d - %d min %u max %u\n",
		__entry->fast_up_count,	__entry->slow_down_count,
		__entry->idle_min, __entry->idle_max)
);

#endif /*  _TRACE_NVHOST_PODGOV_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
