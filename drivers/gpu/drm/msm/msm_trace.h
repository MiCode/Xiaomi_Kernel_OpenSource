/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#if !defined(_MSM_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _MSM_TRACE_H_

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM msm_drm
#define TRACE_INCLUDE_FILE msm_trace

TRACE_EVENT(msm_queued,
	TP_PROTO(struct msm_gem_submit *submit),
	TP_ARGS(submit),
	TP_STRUCT__entry(
		__field(uint32_t, queue_id)
		__field(uint32_t, fence_id)
		__field(int, ring)
	),
	TP_fast_assign(
		__entry->queue_id = submit->queue->id;
		__entry->fence_id = submit->fence;
		__entry->ring = submit->ring;
	),
	TP_printk(
		"queue=%u fence=%u ring=%d",
		__entry->queue_id, __entry->fence_id, __entry->ring
	)
);

TRACE_EVENT(msm_submitted,
	TP_PROTO(struct msm_gem_submit *submit, uint64_t ticks, uint64_t nsecs),
	TP_ARGS(submit, ticks, nsecs),
	TP_STRUCT__entry(
		__field(uint32_t, queue_id)
		__field(uint32_t, fence_id)
		__field(int, ring)
		__field(uint64_t, ticks)
		__field(uint64_t, nsecs)
	),
	TP_fast_assign(
		__entry->queue_id = submit->queue->id;
		__entry->fence_id = submit->fence;
		__entry->ring = submit->ring;
		__entry->ticks = ticks;
		__entry->nsecs = nsecs;
	),
	TP_printk(
		"queue=%u fence=%u ring=%d ticks=%lld nsecs=%llu",
		__entry->queue_id, __entry->fence_id, __entry->ring,
		__entry->ticks, __entry->nsecs
	)
);

TRACE_EVENT(msm_retired,
	TP_PROTO(struct msm_gem_submit *submit, uint64_t start_ticks,
		uint64_t retire_ticks),
	TP_ARGS(submit, start_ticks, retire_ticks),
	TP_STRUCT__entry(
		__field(uint32_t, queue_id)
		__field(uint32_t, fence_id)
		__field(int, ring)
		__field(uint64_t, start_ticks)
		__field(uint64_t, retire_ticks)
	),
	TP_fast_assign(
		__entry->queue_id = submit->queue->id;
		__entry->fence_id = submit->fence;
		__entry->ring = submit->ring;
		__entry->start_ticks = start_ticks;
		__entry->retire_ticks = retire_ticks;
	),
	TP_printk(
		"queue=%u fence=%u ring=%d started=%lld retired=%lld",
		__entry->queue_id, __entry->fence_id, __entry->ring,
		__entry->start_ticks, __entry->retire_ticks
	)
);


#endif

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#include <trace/define_trace.h>

