/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#if !defined(_CAM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CAM_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM camera
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE cam_trace

#include <linux/tracepoint.h>
#include <media/cam_req_mgr.h>
#include "cam_context.h"

TRACE_EVENT(cam_context_state,
	TP_PROTO(const char *name, struct cam_context *ctx),
	TP_ARGS(name, ctx),
	TP_STRUCT__entry(
		__field(void*, ctx)
		__field(uint32_t, state)
		__string(name, name)
	),
	TP_fast_assign(
		__entry->ctx = ctx;
		__entry->state = ctx->state;
		__assign_str(name, name);
	),
	TP_printk(
		"%s: State ctx=%p ctx_state=%u",
			__get_str(name), __entry->ctx, __entry->state
	)
);

TRACE_EVENT(cam_isp_activated_irq,
	TP_PROTO(struct cam_context *ctx, unsigned int substate,
		unsigned int event, uint64_t timestamp),
	TP_ARGS(ctx, substate, event, timestamp),
	TP_STRUCT__entry(
		__field(void*, ctx)
		__field(uint32_t, state)
		__field(uint32_t, substate)
		__field(uint32_t, event)
		__field(uint64_t, ts)
	),
	TP_fast_assign(
		__entry->ctx = ctx;
		__entry->state = ctx->state;
		__entry->substate = substate;
		__entry->event = event;
		__entry->ts = timestamp;
	),
	TP_printk(
		"ISP: IRQ ctx=%p ctx_state=%u substate=%u event=%u ts=%llu",
			__entry->ctx, __entry->state, __entry->substate,
			__entry->event, __entry->ts
	)
);

TRACE_EVENT(cam_buf_done,
	TP_PROTO(const char *ctx_type, struct cam_context *ctx,
		struct cam_ctx_request *req),
	TP_ARGS(ctx_type, ctx, req),
	TP_STRUCT__entry(
		__string(ctx_type, ctx_type)
		__field(void*, ctx)
		__field(uint64_t, request)
	),
	TP_fast_assign(
		__assign_str(ctx_type, ctx_type);
		__entry->ctx = ctx;
		__entry->request = req->request_id;
	),
	TP_printk(
		"%5s: BufDone ctx=%p request=%llu",
			__get_str(ctx_type), __entry->ctx, __entry->request
	)
);

TRACE_EVENT(cam_apply_req,
	TP_PROTO(const char *entity, struct cam_req_mgr_apply_request *req),
	TP_ARGS(entity, req),
	TP_STRUCT__entry(
		__string(entity, entity)
		__field(uint64_t, req_id)
	),
	TP_fast_assign(
		__assign_str(entity, entity);
		__entry->req_id = req->request_id;
	),
	TP_printk(
		"%8s: ApplyRequest request=%llu",
			__get_str(entity), __entry->req_id
	)
);

TRACE_EVENT(cam_flush_req,
	TP_PROTO(struct cam_req_mgr_flush_info *info),
	TP_ARGS(info),
	TP_STRUCT__entry(
		__field(uint32_t, type)
		__field(int64_t, req_id)
	),
	TP_fast_assign(
		__entry->type   = info->flush_type;
		__entry->req_id = info->req_id;
	),
	TP_printk(
		"FlushRequest type=%u request=%llu",
			__entry->type, __entry->req_id
	)
);
#endif /* _CAM_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
