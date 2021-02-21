/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
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
#include "adreno_a5xx.h"

#define ADRENO_FT_TYPES \
	{ BIT(KGSL_FT_OFF), "off" }, \
	{ BIT(KGSL_FT_REPLAY), "replay" }, \
	{ BIT(KGSL_FT_SKIPIB), "skipib" }, \
	{ BIT(KGSL_FT_SKIPFRAME), "skipframe" }, \
	{ BIT(KGSL_FT_DISABLE), "disable" }, \
	{ BIT(KGSL_FT_TEMP_DISABLE), "temp" }, \
	{ BIT(KGSL_FT_THROTTLE), "throttle"}, \
	{ BIT(KGSL_FT_SKIPCMD), "skipcmd" }

TRACE_EVENT(adreno_cmdbatch_queued,
	TP_PROTO(struct kgsl_drawobj *drawobj, unsigned int queued),
	TP_ARGS(drawobj, queued),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, queued)
		__field(unsigned int, flags)
		__field(unsigned int, prio)
	),
	TP_fast_assign(
		__entry->id = drawobj->context->id;
		__entry->timestamp = drawobj->timestamp;
		__entry->queued = queued;
		__entry->flags = drawobj->flags;
		__entry->prio = drawobj->context->priority;
	),
	TP_printk(
		"ctx=%u ctx_prio=%u ts=%u queued=%u flags=%s",
			__entry->id, __entry->prio,
			__entry->timestamp, __entry->queued,
			__entry->flags ? __print_flags(__entry->flags, "|",
						KGSL_DRAWOBJ_FLAGS) : "none"
	)
);

TRACE_EVENT(adreno_cmdbatch_submitted,
	TP_PROTO(struct kgsl_drawobj *drawobj, struct submission_info *info,
		uint64_t ticks, unsigned long secs, unsigned long usecs,
		int q_inflight),
	TP_ARGS(drawobj, info, ticks, secs, usecs, q_inflight),
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
		__field(int, dispatch_queue)
	),
	TP_fast_assign(
		__entry->id = drawobj->context->id;
		__entry->timestamp = drawobj->timestamp;
		__entry->inflight = info->inflight;
		__entry->flags = drawobj->flags;
		__entry->ticks = ticks;
		__entry->secs = secs;
		__entry->usecs = usecs;
		__entry->prio = drawobj->context->priority;
		__entry->rb_id = info->rb_id;
		__entry->rptr = info->rptr;
		__entry->wptr = info->wptr;
		__entry->q_inflight = q_inflight;
		__entry->dispatch_queue = info->gmu_dispatch_queue;
	),
	TP_printk(
		"ctx=%u ctx_prio=%d ts=%u inflight=%d flags=%s ticks=%lld time=%lu.%0lu rb_id=%d r/w=%x/%x, q_inflight=%d dq_id=%d",
			__entry->id, __entry->prio, __entry->timestamp,
			__entry->inflight,
			__entry->flags ? __print_flags(__entry->flags, "|",
				KGSL_DRAWOBJ_FLAGS) : "none",
			__entry->ticks, __entry->secs, __entry->usecs,
			__entry->rb_id, __entry->rptr, __entry->wptr,
			__entry->q_inflight, __entry->dispatch_queue
	)
);

TRACE_EVENT(adreno_cmdbatch_retired,
		TP_PROTO(struct kgsl_context *context, struct retire_info *info,
			unsigned int flags, int q_inflight,
			unsigned long fault_recovery),
	TP_ARGS(context, info, flags, q_inflight, fault_recovery),
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
		__field(unsigned long, fault_recovery)
		__field(unsigned int, dispatch_queue)
		__field(uint64_t, submitted_to_rb)
		__field(uint64_t, retired_on_gmu)
		),
	TP_fast_assign(
		__entry->id = context->id;
		__entry->timestamp = info->timestamp;
		__entry->inflight = info->inflight;
		__entry->recovery = fault_recovery;
		__entry->flags = flags;
		__entry->start = info->sop;
		__entry->retire = info->eop;
		__entry->prio = context->priority;
		__entry->rb_id = info->rb_id;
		__entry->rptr = info->rptr;
		__entry->wptr = info->wptr;
		__entry->q_inflight = q_inflight;
		__entry->dispatch_queue = info->gmu_dispatch_queue;
		__entry->submitted_to_rb = info->submitted_to_rb;
		__entry->retired_on_gmu = info->retired_on_gmu;
		),

	TP_printk(
		"ctx=%u ctx_prio=%d ts=%u inflight=%d recovery=%s flags=%s start=%llu retire=%llu rb_id=%d, r/w=%x/%x, q_inflight=%d, dq_id=%u, submitted_to_rb=%llu retired_on_gmu=%llu",
			__entry->id, __entry->prio, __entry->timestamp,
			__entry->inflight,
			__entry->recovery ?
				__print_flags(__entry->fault_recovery, "|",
				ADRENO_FT_TYPES) : "none",
			__entry->flags ? __print_flags(__entry->flags, "|",
				KGSL_DRAWOBJ_FLAGS) : "none",
			__entry->start,
			__entry->retire,
			__entry->rb_id, __entry->rptr, __entry->wptr,
			__entry->q_inflight,
			__entry->dispatch_queue,
			__entry->submitted_to_rb, __entry->retired_on_gmu
	 )
);

TRACE_EVENT(gmu_ao_sync,
	TP_PROTO(u64 ticks),
	TP_ARGS(ticks),
	TP_STRUCT__entry(
		__field(u64, ticks)
	),
	TP_fast_assign(
		__entry->ticks = ticks;
	),
	TP_printk(
		"ticks=%llu", __entry->ticks
	)
);

TRACE_EVENT(gmu_event,
	TP_PROTO(u32 *event_info),
	TP_ARGS(event_info),
	TP_STRUCT__entry(
		__field(u32, event)
		__field(u32, ticks)
		__field(u32, data1)
		__field(u32, data2)
	),
	TP_fast_assign(
		__entry->event = event_info[0];
		__entry->ticks = event_info[1];
		__entry->data1 = event_info[2];
		__entry->data2 = event_info[3];
	),
	TP_printk(
		"event=%08u ticks=%08u data1=%08x data2=%08x",
		__entry->event, __entry->ticks, __entry->data1, __entry->data2
	)
);

TRACE_EVENT(adreno_cmdbatch_sync,
	TP_PROTO(unsigned int ctx_id, unsigned int ctx_prio,
		unsigned int timestamp,	uint64_t ticks),
	TP_ARGS(ctx_id, ctx_prio, timestamp, ticks),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(uint64_t, ticks)
		__field(int, prio)
	),
	TP_fast_assign(
		__entry->id = ctx_id;
		__entry->timestamp = timestamp;
		__entry->ticks = ticks;
		__entry->prio = ctx_prio;
	),
	TP_printk(
		"ctx=%u ctx_prio=%d ts=%u ticks=%lld",
			__entry->id, __entry->prio, __entry->timestamp,
			__entry->ticks
	)
);

TRACE_EVENT(adreno_cmdbatch_fault,
	TP_PROTO(struct kgsl_drawobj_cmd *cmdobj, unsigned int fault),
	TP_ARGS(cmdobj, fault),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, fault)
	),
	TP_fast_assign(
		__entry->id = cmdobj->base.context->id;
		__entry->timestamp = cmdobj->base.timestamp;
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
	TP_PROTO(struct kgsl_drawobj_cmd *cmdobj, unsigned int action),
	TP_ARGS(cmdobj, action),
	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned int, timestamp)
		__field(unsigned int, action)
	),
	TP_fast_assign(
		__entry->id = cmdobj->base.context->id;
		__entry->timestamp = cmdobj->base.timestamp;
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
		struct adreno_context *newctx),
	TP_ARGS(rb, newctx),
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
		"rb level=%d oldctx=%u newctx=%u",
		__entry->rb_level, __entry->oldctx, __entry->newctx
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
	TP_printk(
		"ctx=%d ts=%d rb_id=%d status=%X RB=%X/%X IB1=%X/%X IB2=%X/%X",
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
		"func=%pS", (void *) __entry->ip
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
			{ BIT(A3XX_INT_RBBM_GPU_IDLE), "RBBM_GPU_IDLE" },
			{ BIT(A3XX_INT_RBBM_AHB_ERROR), "RBBM_AHB_ERR" },
			{ BIT(A3XX_INT_RBBM_REG_TIMEOUT), "RBBM_REG_TIMEOUT" },
			{ BIT(A3XX_INT_RBBM_ME_MS_TIMEOUT),
				"RBBM_ME_MS_TIMEOUT" },
			{ BIT(A3XX_INT_RBBM_PFP_MS_TIMEOUT),
				"RBBM_PFP_MS_TIMEOUT" },
			{ BIT(A3XX_INT_RBBM_ATB_BUS_OVERFLOW),
				"RBBM_ATB_BUS_OVERFLOW" },
			{ BIT(A3XX_INT_VFD_ERROR), "RBBM_VFD_ERROR" },
			{ BIT(A3XX_INT_CP_SW_INT), "CP_SW" },
			{ BIT(A3XX_INT_CP_T0_PACKET_IN_IB),
				"CP_T0_PACKET_IN_IB" },
			{ BIT(A3XX_INT_CP_OPCODE_ERROR), "CP_OPCODE_ERROR" },
			{ BIT(A3XX_INT_CP_RESERVED_BIT_ERROR),
				"CP_RESERVED_BIT_ERROR" },
			{ BIT(A3XX_INT_CP_HW_FAULT), "CP_HW_FAULT" },
			{ BIT(A3XX_INT_CP_DMA), "CP_DMA" },
			{ BIT(A3XX_INT_CP_IB2_INT), "CP_IB2_INT" },
			{ BIT(A3XX_INT_CP_IB1_INT), "CP_IB1_INT" },
			{ BIT(A3XX_INT_CP_RB_INT), "CP_RB_INT" },
			{ BIT(A3XX_INT_CP_REG_PROTECT_FAULT),
				"CP_REG_PROTECT_FAULT" },
			{ BIT(A3XX_INT_CP_RB_DONE_TS), "CP_RB_DONE_TS" },
			{ BIT(A3XX_INT_CP_VS_DONE_TS), "CP_VS_DONE_TS" },
			{ BIT(A3XX_INT_CP_PS_DONE_TS), "CP_PS_DONE_TS" },
			{ BIT(A3XX_INT_CACHE_FLUSH_TS), "CACHE_FLUSH_TS" },
			{ BIT(A3XX_INT_CP_AHB_ERROR_HALT),
				"CP_AHB_ERROR_HALT" },
			{ BIT(A3XX_INT_MISC_HANG_DETECT), "MISC_HANG_DETECT" },
			{ BIT(A3XX_INT_UCHE_OOB_ACCESS), "UCHE_OOB_ACCESS" })
			: "None"
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
			{ BIT(A5XX_INT_RBBM_GPU_IDLE), "RBBM_GPU_IDLE" },
			{ BIT(A5XX_INT_RBBM_AHB_ERROR), "RBBM_AHB_ERR" },
			{ BIT(A5XX_INT_RBBM_TRANSFER_TIMEOUT),
				"RBBM_TRANSFER_TIMEOUT" },
			{ BIT(A5XX_INT_RBBM_ME_MS_TIMEOUT),
				"RBBM_ME_MS_TIMEOUT" },
			{ BIT(A5XX_INT_RBBM_PFP_MS_TIMEOUT),
				"RBBM_PFP_MS_TIMEOUT" },
			{ BIT(A5XX_INT_RBBM_ETS_MS_TIMEOUT),
				"RBBM_ETS_MS_TIMEOUT" },
			{ BIT(A5XX_INT_RBBM_ATB_ASYNC_OVERFLOW),
				"RBBM_ATB_ASYNC_OVERFLOW" },
			{ BIT(A5XX_INT_RBBM_GPC_ERROR), "RBBM_GPC_ERR" },
			{ BIT(A5XX_INT_CP_SW), "CP_SW" },
			{ BIT(A5XX_INT_CP_HW_ERROR), "CP_OPCODE_ERROR" },
			{ BIT(A5XX_INT_CP_CCU_FLUSH_DEPTH_TS),
				"CP_CCU_FLUSH_DEPTH_TS" },
			{ BIT(A5XX_INT_CP_CCU_FLUSH_COLOR_TS),
				"CP_CCU_FLUSH_COLOR_TS" },
			{ BIT(A5XX_INT_CP_CCU_RESOLVE_TS),
				"CP_CCU_RESOLVE_TS" },
			{ BIT(A5XX_INT_CP_IB2), "CP_IB2_INT" },
			{ BIT(A5XX_INT_CP_IB1), "CP_IB1_INT" },
			{ BIT(A5XX_INT_CP_RB), "CP_RB_INT" },
			{ BIT(A5XX_INT_CP_UNUSED_1), "CP_UNUSED_1" },
			{ BIT(A5XX_INT_CP_RB_DONE_TS), "CP_RB_DONE_TS" },
			{ BIT(A5XX_INT_CP_WT_DONE_TS), "CP_WT_DONE_TS" },
			{ BIT(A5XX_INT_UNKNOWN_1), "UNKNOWN_1" },
			{ BIT(A5XX_INT_CP_CACHE_FLUSH_TS),
				"CP_CACHE_FLUSH_TS" },
			{ BIT(A5XX_INT_UNUSED_2), "UNUSED_2" },
			{ BIT(A5XX_INT_RBBM_ATB_BUS_OVERFLOW),
				"RBBM_ATB_BUS_OVERFLOW" },
			{ BIT(A5XX_INT_MISC_HANG_DETECT), "MISC_HANG_DETECT" },
			{ BIT(A5XX_INT_UCHE_OOB_ACCESS), "UCHE_OOB_ACCESS" },
			{ BIT(A5XX_INT_UCHE_TRAP_INTR), "UCHE_TRAP_INTR" },
			{ BIT(A5XX_INT_DEBBUS_INTR_0), "DEBBUS_INTR_0" },
			{ BIT(A5XX_INT_DEBBUS_INTR_1), "DEBBUS_INTR_1" },
			{ BIT(A5XX_INT_GPMU_VOLTAGE_DROOP),
				"GPMU_VOLTAGE_DROOP" },
			{ BIT(A5XX_INT_GPMU_FIRMWARE), "GPMU_FIRMWARE" },
			{ BIT(A5XX_INT_ISDB_CPU_IRQ), "ISDB_CPU_IRQ" },
			{ BIT(A5XX_INT_ISDB_UNDER_DEBUG), "ISDB_UNDER_DEBUG" })
			: "None"
	)
);

DECLARE_EVENT_CLASS(adreno_hw_preempt_template,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb,
		unsigned int cur_rptr, unsigned int new_rptr),
	TP_ARGS(cur_rb, new_rb, cur_rptr, new_rptr),
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
			__entry->cur_rptr = cur_rptr;
			__entry->new_rptr = new_rptr;
			__entry->cur_wptr = cur_rb->wptr;
			__entry->new_wptr = new_rb->wptr;
			__entry->cur_rbbase = cur_rb->buffer_desc->gpuaddr;
			__entry->new_rbbase = new_rb->buffer_desc->gpuaddr;
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
		struct adreno_ringbuffer *new_rb,
		unsigned int cur_rptr, unsigned int new_rptr),
	TP_ARGS(cur_rb, new_rb, cur_rptr, new_rptr)
);

DEFINE_EVENT(adreno_hw_preempt_template, adreno_hw_preempt_trig_to_comp,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb,
		unsigned int cur_rptr, unsigned int new_rptr),
	TP_ARGS(cur_rb, new_rb, cur_rptr, new_rptr)
);

DEFINE_EVENT(adreno_hw_preempt_template, adreno_hw_preempt_trig_to_comp_int,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb,
		unsigned int cur_rptr, unsigned int new_rptr),
	TP_ARGS(cur_rb, new_rb, cur_rptr, new_rptr)
);

TRACE_EVENT(adreno_hw_preempt_comp_to_clear,
	TP_PROTO(struct adreno_ringbuffer *cur_rb,
		struct adreno_ringbuffer *new_rb,
		unsigned int cur_rptr, unsigned int new_rptr),
	TP_ARGS(cur_rb, new_rb, cur_rptr, new_rptr),
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
			__entry->cur_rptr = cur_rptr;
			__entry->new_rptr = new_rptr;
			__entry->cur_wptr = cur_rb->wptr;
			__entry->new_wptr_end = new_rb->wptr_preempt_end;
			__entry->new_wptr = new_rb->wptr;
			__entry->cur_rbbase = cur_rb->buffer_desc->gpuaddr;
			__entry->new_rbbase = new_rb->buffer_desc->gpuaddr;
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
		struct adreno_ringbuffer *new_rb,
		unsigned int cur_rptr, unsigned int new_rptr),
	TP_ARGS(cur_rb, new_rb, cur_rptr, new_rptr),
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
			__entry->cur_rptr = cur_rptr;
			__entry->new_rptr = new_rptr;
			__entry->cur_wptr = cur_rb->wptr;
			__entry->cur_wptr_end = cur_rb->wptr_preempt_end;
			__entry->new_wptr = new_rb->wptr;
			__entry->cur_rbbase = cur_rb->buffer_desc->gpuaddr;
			__entry->new_rbbase = new_rb->buffer_desc->gpuaddr;
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

TRACE_EVENT(adreno_preempt_trigger,
	TP_PROTO(struct adreno_ringbuffer *cur, struct adreno_ringbuffer *next,
		unsigned int cntl),
	TP_ARGS(cur, next, cntl),
	TP_STRUCT__entry(
		__field(unsigned int, cur)
		__field(unsigned int, next)
		__field(unsigned int, cntl)
	),
	TP_fast_assign(
		__entry->cur = cur->id;
		__entry->next = next->id;
		__entry->cntl = cntl;
	),
	TP_printk("trigger from id=%d to id=%d cntl=%x",
		__entry->cur, __entry->next, __entry->cntl
	)
);

TRACE_EVENT(adreno_preempt_done,
	TP_PROTO(struct adreno_ringbuffer *cur, struct adreno_ringbuffer *next,
		unsigned int level),
	TP_ARGS(cur, next, level),
	TP_STRUCT__entry(
		__field(unsigned int, cur)
		__field(unsigned int, next)
		__field(unsigned int, level)
	),
	TP_fast_assign(
		__entry->cur = cur->id;
		__entry->next = next->id;
		__entry->level = level;
	),
	TP_printk("done switch to id=%d from id=%d level=%x",
		__entry->next, __entry->cur, __entry->level
	)
);

TRACE_EVENT(adreno_ifpc_count,
	TP_PROTO(unsigned int ifpc_count),
	TP_ARGS(ifpc_count),
	TP_STRUCT__entry(
		__field(unsigned int, ifpc_count)
	),
	TP_fast_assign(
		__entry->ifpc_count = ifpc_count;
	),
	TP_printk("total times GMU entered IFPC = %d", __entry->ifpc_count)
);

#endif /* _ADRENO_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
