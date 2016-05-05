/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE adreno_trace

#include <linux/tracepoint.h>
#include "adreno_a3xx.h"
#include "adreno_a4xx.h"
#include "adreno_a5xx.h"

TRACE_EVENT(adreno_cmdbatch_queued,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, unsigned int queued),
	TP_ARGS(cmdbatch, queued),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, queued)
		__field(unsigned int, flags)
		__field(unsigned int, prio)
	),
	TP_fast_assign(
		__entry->id = cmdbatch->context->id;
		__entry->timestamp = cmdbatch->timestamp;
		__entry->queued = queued;
		__entry->flags = cmdbatch->flags;
		__entry->prio = cmdbatch->context->priority;
	),
	TP_printk(
		"ctx=%u ctx_prio=%u ts=%u queued=%u flags=%s",
			__entry->id, __entry->prio,
			__entry->timestamp, __entry->queued,
			__entry->flags ? __print_flags(__entry->flags, "|",
						KGSL_CMDBATCH_FLAGS) : "none"
	)
);

TRACE_EVENT(adreno_cmdbatch_submitted,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, int inflight, uint64_t ticks,
		unsigned long secs, unsigned long usecs,
		struct adreno_ringbuffer *rb),
	TP_ARGS(cmdbatch, inflight, ticks, secs, usecs, rb),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(int, inflight)
		__field(unsigned int, flags)
		__field(uint64_t, ticks)
		__field(unsigned long, secs)
		__field(unsigned long, usecs)
		__field(int, prio)
		__field(int, rb_id)
		__field(unsigned int, rptr)
		__field(unsigned int, wptr)
		__field(int, q_inflight)
	),
	TP_fast_assign(
		__entry->id = cmdbatch->context->id;
		__entry->timestamp = cmdbatch->timestamp;
		__entry->inflight = inflight;
		__entry->flags = cmdbatch->flags;
		__entry->ticks = ticks;
		__entry->secs = secs;
		__entry->usecs = usecs;
		__entry->prio = cmdbatch->context->priority;
		__entry->rb_id = rb->id;
		__entry->rptr = rb->rptr;
		__entry->wptr = rb->wptr;
		__entry->q_inflight = rb->dispatch_q.inflight;
	),
	TP_printk(
		"ctx=%u ctx_prio=%d ts=%u inflight=%d flags=%s ticks=%lld time=%lu.%0lu rb_id=%d r/w=%x/%x, q_inflight=%d",
			__entry->id, __entry->prio, __entry->timestamp,
			__entry->inflight,
			__entry->flags ? __print_flags(__entry->flags, "|",
				KGSL_CMDBATCH_FLAGS) : "none",
			__entry->ticks, __entry->secs, __entry->usecs,
			__entry->rb_id, __entry->rptr, __entry->wptr,
			__entry->q_inflight
	)
);

TRACE_EVENT(adreno_cmdbatch_retired,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, int inflight,
		uint64_t start, uint64_t retire,
		struct adreno_ringbuffer *rb),
	TP_ARGS(cmdbatch, inflight, start, retire, rb),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(int, inflight)
		__field(unsigned int, recovery)
		__field(unsigned int, flags)
		__field(uint64_t, start)
		__field(uint64_t, retire)
		__field(int, prio)
		__field(int, rb_id)
		__field(unsigned int, rptr)
		__field(unsigned int, wptr)
		__field(int, q_inflight)
	),
	TP_fast_assign(
		__entry->id = cmdbatch->context->id;
		__entry->timestamp = cmdbatch->timestamp;
		__entry->inflight = inflight;
		__entry->recovery = cmdbatch->fault_recovery;
		__entry->flags = cmdbatch->flags;
		__entry->start = start;
		__entry->retire = retire;
		__entry->prio = cmdbatch->context->priority;
		__entry->rb_id = rb->id;
		__entry->rptr = rb->rptr;
		__entry->wptr = rb->wptr;
		__entry->q_inflight = rb->dispatch_q.inflight;
	),
	TP_printk(
		"ctx=%u ctx_prio=%d ts=%u inflight=%d recovery=%s flags=%s start=%lld retire=%lld rb_id=%d, r/w=%x/%x, q_inflight=%d",
			__entry->id, __entry->prio, __entry->timestamp,
			__entry->inflight,
			__entry->recovery ?
				__print_flags(__entry->recovery, "|",
				ADRENO_FT_TYPES) : "none",
			__entry->flags ? __print_flags(__entry->flags, "|",
				KGSL_CMDBATCH_FLAGS) : "none",
			__entry->start,
			__entry->retire,
			__entry->rb_id, __entry->rptr, __entry->wptr,
			__entry->q_inflight
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
		__field(unsigned int, priority)
	),
	TP_fast_assign(
		__entry->id = drawctxt->base.id;
		__entry->priority = drawctxt->base.priority;
	),
	TP_printk("ctx=%u priority=%u", __entry->id, __entry->priority)
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
	TP_PROTO(unsigned int rb_id, unsigned int ctx_id, unsigned int ts),
	TP_ARGS(rb_id, ctx_id, ts),
	TP_STRUCT__entry(
		__field(unsigned int, rb_id)
		__field(unsigned int, ctx_id)
		__field(unsigned int, ts)
	),
	TP_fast_assign(
		__entry->rb_id = rb_id;
		__entry->ctx_id = ctx_id;
		__entry->ts = ts;
	),
	TP_printk(
		"rb=%u ctx=%u ts=%u",
			__entry->rb_id, __entry->ctx_id, __entry->ts
	)
);

TRACE_EVENT(adreno_drawctxt_wait_done,
	TP_PROTO(unsigned int rb_id, unsigned int ctx_id,
			unsigned int ts, int status),
	TP_ARGS(rb_id, ctx_id, ts, status),
	TP_STRUCT__entry(
		__field(unsigned int, rb_id)
		__field(unsigned int, ctx_id)
		__field(unsigned int, ts)
		__field(int, status)
	),
	TP_fast_assign(
		__entry->rb_id = rb_id;
		__entry->ctx_id = ctx_id;
		__entry->ts = ts;
		__entry->status = status;
	),
	TP_printk(
		"rb=%u ctx=%u ts=%u status=%d",
		__entry->rb_id, __entry->ctx_id, __entry->ts, __entry->status
	)
);

TRACE_EVENT(adreno_drawctxt_switch,
	TP_PROTO(struct adreno_ringbuffer *rb,
		struct adreno_context *newctx,
		unsigned int flags),
	TP_ARGS(rb, newctx, flags),
	TP_STRUCT__entry(
		__field(int, rb_level)
		__field(unsigned int, oldctx)
		__field(unsigned int, newctx)
		__field(unsigned int, flags)
	),
	TP_fast_assign(
		__entry->rb_level = rb->id;
		__entry->oldctx = rb->drawctxt_active ?
			rb->drawctxt_active->base.id : 0;
		__entry->newctx = newctx ? newctx->base.id : 0;
	),
	TP_printk(
		"rb level=%d oldctx=%u newctx=%u flags=%X",
		__entry->rb_level, __entry->oldctx, __entry->newctx, flags
	)
);

TRACE_EVENT(adreno_gpu_fault,
	TP_PROTO(unsigned int ctx, unsigned int ts,
		unsigned int status, unsigned int rptr, unsigned int wptr,
		unsigned int ib1base, unsigned int ib1size,
		unsigned int ib2base, unsigned int ib2size, int rb_id),
	TP_ARGS(ctx, ts, status, rptr, wptr, ib1base, ib1size, ib2base,
		ib2size, rb_id),
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
		__field(int, rb_id)
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
		__entry->rb_id = rb_id;
	),
	TP_printk("ctx=%d ts=%d rb_id=%d status=%X RB=%X/%X IB1=%X/%X IB2=%X/%X",
		__entry->ctx, __entry->ts, __entry->rb_id, __entry->status,
		__entry->wptr, __entry->rptr, __entry->ib1base,
		__entry->ib1size, __entry->ib2base, __entry->ib2size)
);

TRACE_EVENT(adreno_sp_tp,

	TP_PROTO(unsigned long ip),

	TP_ARGS(ip),

	TP_STRUCT__entry(
		__field(unsigned long, ip)
	),

	TP_fast_assign(
		__entry->ip = ip;
	),

	TP_printk(
		"func=%pf", (void *) __entry->ip
	)
);

/*
 * Tracepoint for a3xx irq. Includes status info
 */
TRACE_EVENT(kgsl_a3xx_irq_status,

	TP_PROTO(struct adreno_device *adreno_dev, unsigned int status),

	TP_ARGS(adreno_dev, status),

	TP_STRUCT__entry(
		__string(device_name, adreno_dev->dev.name)
		__field(unsigned int, status)
	),

	TP_fast_assign(
		__assign_str(device_name, adreno_dev->dev.name);
		__entry->status = status;
	),

	TP_printk(
		"d_name=%s status=%s",
		__get_str(device_name),
		__entry->status ? __print_flags(__entry->status, "|",
			A3XX_IRQ_FLAGS) : "None"
	)
);

/*
 * Tracepoint for a4xx irq. Includes status info
 */
TRACE_EVENT(kgsl_a4xx_irq_status,

	TP_PROTO(struct adreno_device *adreno_dev, unsigned int status),

	TP_ARGS(adreno_dev, status),

	TP_STRUCT__entry(
		__string(device_name, adreno_dev->dev.name)
		__field(unsigned int, status)
	),

	TP_fast_assign(
		__assign_str(device_name, adreno_dev->dev.name);
		__entry->status = status;
	),

	TP_printk(
		"d_name=%s status=%s",
		__get_str(device_name),
		__entry->status ? __print_flags(__entry->status, "|",
			A4XX_IRQ_FLAGS) : "None"
	)
);

/*
 * Tracepoint for a5xx irq. Includes status info
 */
TRACE_EVENT(kgsl_a5xx_irq_status,

	TP_PROTO(struct adreno_device *adreno_dev, unsigned int status),

	TP_ARGS(adreno_dev, status),

	TP_STRUCT__entry(
		__string(device_name, adreno_dev->dev.name)
		__field(unsigned int, status)
	),

	TP_fast_assign(
		__assign_str(device_name, adreno_dev->dev.name);
		__entry->status = status;
	),

	TP_printk(
		"d_name=%s status=%s",
		__get_str(device_name),
		__entry->status ? __print_flags(__entry->status, "|",
			A5XX_IRQ_FLAGS) : "None"
	)
);

DECLARE_EVENT_CLASS(adreno_hw_preempt_template,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb),
	TP_ARGS(cur_rb, new_rb),
	TP_STRUCT__entry(__field(int, cur_level)
			__field(int, new_level)
			__field(unsigned int, cur_rptr)
			__field(unsigned int, new_rptr)
			__field(unsigned int, cur_wptr)
			__field(unsigned int, new_wptr)
			__field(unsigned int, cur_rbbase)
			__field(unsigned int, new_rbbase)
	),
	TP_fast_assign(__entry->cur_level = cur_rb->id;
			__entry->new_level = new_rb->id;
			__entry->cur_rptr = cur_rb->rptr;
			__entry->new_rptr = new_rb->rptr;
			__entry->cur_wptr = cur_rb->wptr;
			__entry->new_wptr = new_rb->wptr;
			__entry->cur_rbbase = cur_rb->buffer_desc.gpuaddr;
			__entry->new_rbbase = new_rb->buffer_desc.gpuaddr;
	),
	TP_printk(
	"cur_rb_lvl=%d rptr=%x wptr=%x rbbase=%x new_rb_lvl=%d rptr=%x wptr=%x rbbase=%x",
		__entry->cur_level, __entry->cur_rptr,
		__entry->cur_wptr, __entry->cur_rbbase,
		__entry->new_level, __entry->new_rptr,
		__entry->new_wptr, __entry->new_rbbase
	)
);

DEFINE_EVENT(adreno_hw_preempt_template, adreno_hw_preempt_clear_to_trig,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb),
	TP_ARGS(cur_rb, new_rb)
);

DEFINE_EVENT(adreno_hw_preempt_template, adreno_hw_preempt_trig_to_comp,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb),
	TP_ARGS(cur_rb, new_rb)
);

DEFINE_EVENT(adreno_hw_preempt_template, adreno_hw_preempt_trig_to_comp_int,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb),
	TP_ARGS(cur_rb, new_rb)
);

TRACE_EVENT(adreno_hw_preempt_comp_to_clear,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb),
	TP_ARGS(cur_rb, new_rb),
	TP_STRUCT__entry(__field(int, cur_level)
			__field(int, new_level)
			__field(unsigned int, cur_rptr)
			__field(unsigned int, new_rptr)
			__field(unsigned int, cur_wptr)
			__field(unsigned int, new_wptr_end)
			__field(unsigned int, new_wptr)
			__field(unsigned int, cur_rbbase)
			__field(unsigned int, new_rbbase)
	),
	TP_fast_assign(__entry->cur_level = cur_rb->id;
			__entry->new_level = new_rb->id;
			__entry->cur_rptr = cur_rb->rptr;
			__entry->new_rptr = new_rb->rptr;
			__entry->cur_wptr = cur_rb->wptr;
			__entry->new_wptr_end = new_rb->wptr_preempt_end;
			__entry->new_wptr = new_rb->wptr;
			__entry->cur_rbbase = cur_rb->buffer_desc.gpuaddr;
			__entry->new_rbbase = new_rb->buffer_desc.gpuaddr;
	),
	TP_printk(
	"cur_rb_lvl=%d rptr=%x wptr=%x rbbase=%x prev_rb_lvl=%d rptr=%x wptr_preempt_end=%x wptr=%x rbbase=%x",
		__entry->cur_level, __entry->cur_rptr,
		__entry->cur_wptr, __entry->cur_rbbase,
		__entry->new_level, __entry->new_rptr,
		__entry->new_wptr_end, __entry->new_wptr, __entry->new_rbbase
	)
);

TRACE_EVENT(adreno_hw_preempt_token_submit,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb),
	TP_ARGS(cur_rb, new_rb),
	TP_STRUCT__entry(__field(int, cur_level)
		__field(int, new_level)
		__field(unsigned int, cur_rptr)
		__field(unsigned int, new_rptr)
		__field(unsigned int, cur_wptr)
		__field(unsigned int, cur_wptr_end)
		__field(unsigned int, new_wptr)
		__field(unsigned int, cur_rbbase)
		__field(unsigned int, new_rbbase)
	),
	TP_fast_assign(__entry->cur_level = cur_rb->id;
			__entry->new_level = new_rb->id;
			__entry->cur_rptr = cur_rb->rptr;
			__entry->new_rptr = new_rb->rptr;
			__entry->cur_wptr = cur_rb->wptr;
			__entry->cur_wptr_end = cur_rb->wptr_preempt_end;
			__entry->new_wptr = new_rb->wptr;
			__entry->cur_rbbase = cur_rb->buffer_desc.gpuaddr;
			__entry->new_rbbase = new_rb->buffer_desc.gpuaddr;
	),
	TP_printk(
		"cur_rb_lvl=%d rptr=%x wptr_preempt_end=%x wptr=%x rbbase=%x new_rb_lvl=%d rptr=%x wptr=%x rbbase=%x",
		__entry->cur_level, __entry->cur_rptr,
		__entry->cur_wptr_end, __entry->cur_wptr,
		__entry->cur_rbbase,
		__entry->new_level, __entry->new_rptr,
		__entry->new_wptr, __entry->new_rbbase
	)
);

TRACE_EVENT(adreno_rb_starve,
	TP_PROTO(struct adreno_ringbuffer *rb),
	TP_ARGS(rb),
	TP_STRUCT__entry(__field(int, id)
		__field(unsigned int, rptr)
		__field(unsigned int, wptr)
	),
	TP_fast_assign(__entry->id = rb->id;
		__entry->rptr = rb->rptr;
		__entry->wptr = rb->wptr;
	),
	TP_printk(
		"rb %d r/w %x/%x starved", __entry->id, __entry->rptr,
		__entry->wptr
	)
);

#endif /* _ADRENO_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
