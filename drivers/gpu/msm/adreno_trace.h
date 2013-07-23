/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#if !defined(_ADRENO_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _ADRENO_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM kgsl
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE adreno_trace

#include <linux/tracepoint.h>

TRACE_EVENT(adreno_cmdbatch_queued,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, unsigned int queued),
	TP_ARGS(cmdbatch, queued),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, queued)
		__field(unsigned int, flags)
	),
	TP_fast_assign(
		__entry->id = cmdbatch->context->id;
		__entry->timestamp = cmdbatch->timestamp;
		__entry->queued = queued;
		__entry->flags = cmdbatch->flags;
	),
	TP_printk(
		"ctx=%u ts=%u queued=%u flags=%s",
			__entry->id, __entry->timestamp, __entry->queued,
			__entry->flags ? __print_flags(__entry->flags, "|",
				{ KGSL_CONTEXT_SYNC, "SYNC" },
				{ KGSL_CONTEXT_END_OF_FRAME, "EOF" })
				: "none"
	)
);

DECLARE_EVENT_CLASS(adreno_cmdbatch_template,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, int inflight),
	TP_ARGS(cmdbatch, inflight),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, inflight)
	),
	TP_fast_assign(
		__entry->id = cmdbatch->context->id;
		__entry->timestamp = cmdbatch->timestamp;
		__entry->inflight = inflight;
	),
	TP_printk(
		"ctx=%u ts=%u inflight=%u",
			__entry->id, __entry->timestamp,
			__entry->inflight
	)
);

DEFINE_EVENT(adreno_cmdbatch_template, adreno_cmdbatch_submitted,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, int inflight),
	TP_ARGS(cmdbatch, inflight)
);

TRACE_EVENT(adreno_cmdbatch_retired,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, int inflight),
	TP_ARGS(cmdbatch, inflight),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, inflight)
		__field(unsigned int, recovery)
	),
	TP_fast_assign(
		__entry->id = cmdbatch->context->id;
		__entry->timestamp = cmdbatch->timestamp;
		__entry->inflight = inflight;
		__entry->recovery = cmdbatch->fault_recovery;
	),
	TP_printk(
		"ctx=%u ts=%u inflight=%u recovery=%s",
			__entry->id, __entry->timestamp,
			__entry->inflight,
			__entry->recovery ?
				__print_flags(__entry->recovery, "|",
				ADRENO_FT_TYPES) : "none"
	)
);

TRACE_EVENT(adreno_cmdbatch_fault,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, unsigned int fault),
	TP_ARGS(cmdbatch, fault),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, fault)
	),
	TP_fast_assign(
		__entry->id = cmdbatch->context->id;
		__entry->timestamp = cmdbatch->timestamp;
		__entry->fault = fault;
	),
	TP_printk(
		"ctx=%u ts=%u type=%s",
			__entry->id, __entry->timestamp,
			__print_symbolic(__entry->fault,
				{ 0, "none" },
				{ ADRENO_SOFT_FAULT, "soft" },
				{ ADRENO_HARD_FAULT, "hard" },
				{ ADRENO_TIMEOUT_FAULT, "timeout" })
	)
);

TRACE_EVENT(adreno_cmdbatch_recovery,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, unsigned int action),
	TP_ARGS(cmdbatch, action),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, action)
	),
	TP_fast_assign(
		__entry->id = cmdbatch->context->id;
		__entry->timestamp = cmdbatch->timestamp;
		__entry->action = action;
	),
	TP_printk(
		"ctx=%u ts=%u action=%s",
			__entry->id, __entry->timestamp,
			__print_symbolic(__entry->action, ADRENO_FT_TYPES)
	)
);

DECLARE_EVENT_CLASS(adreno_drawctxt_template,
	TP_PROTO(struct adreno_context *drawctxt),
	TP_ARGS(drawctxt),
	TP_STRUCT__entry(
		__field(unsigned int, id)
	),
	TP_fast_assign(
		__entry->id = drawctxt->base.id;
	),
	TP_printk("ctx=%u", __entry->id)
);

DEFINE_EVENT(adreno_drawctxt_template, adreno_drawctxt_sleep,
	TP_PROTO(struct adreno_context *drawctxt),
	TP_ARGS(drawctxt)
);

DEFINE_EVENT(adreno_drawctxt_template, adreno_drawctxt_wake,
	TP_PROTO(struct adreno_context *drawctxt),
	TP_ARGS(drawctxt)
);

DEFINE_EVENT(adreno_drawctxt_template, dispatch_queue_context,
	TP_PROTO(struct adreno_context *drawctxt),
	TP_ARGS(drawctxt)
);

DEFINE_EVENT(adreno_drawctxt_template, adreno_drawctxt_invalidate,
	TP_PROTO(struct adreno_context *drawctxt),
	TP_ARGS(drawctxt)
);

TRACE_EVENT(adreno_drawctxt_wait_start,
	TP_PROTO(unsigned int id, unsigned int ts),
	TP_ARGS(id, ts),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, ts)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->ts = ts;
	),
	TP_printk(
		"ctx=%u ts=%u",
			__entry->id, __entry->ts
	)
);

TRACE_EVENT(adreno_drawctxt_wait_done,
	TP_PROTO(unsigned int id, unsigned int ts, int status),
	TP_ARGS(id, ts, status),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, ts)
		__field(int, status)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->ts = ts;
		__entry->status = status;
	),
	TP_printk(
		"ctx=%u ts=%u status=%d",
			__entry->id, __entry->ts, __entry->status
	)
);

TRACE_EVENT(adreno_gpu_fault,
	TP_PROTO(unsigned int ctx, unsigned int ts,
		unsigned int status, unsigned int rptr, unsigned int wptr,
		unsigned int ib1base, unsigned int ib1size,
		unsigned int ib2base, unsigned int ib2size),
	TP_ARGS(ctx, ts, status, rptr, wptr, ib1base, ib1size, ib2base,
		ib2size),
	TP_STRUCT__entry(
		__field(unsigned int, ctx)
		__field(unsigned int, ts)
		__field(unsigned int, status)
		__field(unsigned int, rptr)
		__field(unsigned int, wptr)
		__field(unsigned int, ib1base)
		__field(unsigned int, ib1size)
		__field(unsigned int, ib2base)
		__field(unsigned int, ib2size)
	),
	TP_fast_assign(
		__entry->ctx = ctx;
		__entry->ts = ts;
		__entry->status = status;
		__entry->rptr = rptr;
		__entry->wptr = wptr;
		__entry->ib1base = ib1base;
		__entry->ib1size = ib1size;
		__entry->ib2base = ib2base;
		__entry->ib2size = ib2size;
	),
	TP_printk("ctx=%d ts=%d status=%X RB=%X/%X IB1=%X/%X IB2=%X/%X",
		__entry->ctx, __entry->ts, __entry->status, __entry->wptr,
		__entry->rptr, __entry->ib1base, __entry->ib1size,
		__entry->ib2base, __entry->ib2size)
);

#endif /* _ADRENO_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
