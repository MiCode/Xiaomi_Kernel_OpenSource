/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#if !defined(_CAM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CAM_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM camera
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ../../techpack/camera/drivers/cam_utils/cam_trace

#include <linux/tracepoint.h>
#include <media/cam_req_mgr.h>
#include "cam_req_mgr_core.h"
#include "cam_req_mgr_interface.h"
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

TRACE_EVENT(cam_icp_fw_dbg,
	TP_PROTO(char *dbg_message, uint64_t timestamp),
	TP_ARGS(dbg_message, timestamp),
	TP_STRUCT__entry(
		__string(dbg_message, dbg_message)
		__field(uint64_t, timestamp)
	),
	TP_fast_assign(
		__assign_str(dbg_message, dbg_message);
		__entry->timestamp = timestamp;
	),
	TP_printk(
		"%llu %s: ",
		 __entry->timestamp, __get_str(dbg_message)
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
	TP_PROTO(const char *entity, uint64_t req_id),
	TP_ARGS(entity, req_id),
	TP_STRUCT__entry(
		__string(entity, entity)
		__field(uint64_t, req_id)
	),
	TP_fast_assign(
		__assign_str(entity, entity);
		__entry->req_id = req_id;
	),
	TP_printk(
		"%8s: ApplyRequest request=%llu",
			__get_str(entity), __entry->req_id
	)
);

TRACE_EVENT(cam_flush_req,
	TP_PROTO(struct cam_req_mgr_core_link *link,
		struct cam_req_mgr_flush_info *info),
	TP_ARGS(link, info),
	TP_STRUCT__entry(
		__field(uint32_t, type)
		__field(int64_t, req_id)
		__field(void*, link)
		__field(void*, session)
	),
	TP_fast_assign(
		__entry->type    = info->flush_type;
		__entry->req_id  = info->req_id;
		__entry->link    = link;
		__entry->session = link->parent;
	),
	TP_printk(
		"FlushRequest type=%u request=%llu link=%pK session=%pK",
			__entry->type, __entry->req_id, __entry->link,
			__entry->session
	)
);

TRACE_EVENT(cam_req_mgr_connect_device,
	TP_PROTO(struct cam_req_mgr_core_link *link,
		struct cam_req_mgr_device_info *info),
	TP_ARGS(link, info),
	TP_STRUCT__entry(
		__string(name, info->name)
		__field(uint32_t, id)
		__field(uint32_t, delay)
		__field(void*, link)
		__field(void*, session)
	),
	TP_fast_assign(
		__assign_str(name, info->name);
		__entry->id      = info->dev_id;
		__entry->delay   = info->p_delay;
		__entry->link    = link;
		__entry->session = link->parent;
	),
	TP_printk(
		"ReqMgr Connect name=%s id=%u pd=%d link=%pK session=%pK",
			__get_str(name), __entry->id, __entry->delay,
			__entry->link, __entry->session
	)
);

TRACE_EVENT(cam_req_mgr_apply_request,
	TP_PROTO(struct cam_req_mgr_core_link *link,
		struct cam_req_mgr_apply_request *req,
		struct cam_req_mgr_connected_device *dev),
	TP_ARGS(link, req, dev),
	TP_STRUCT__entry(
		__string(name, dev->dev_info.name)
		__field(uint32_t, dev_id)
		__field(uint64_t, req_id)
		__field(void*, link)
		__field(void*, session)
	),
	TP_fast_assign(
		__assign_str(name, dev->dev_info.name);
		__entry->dev_id  = dev->dev_info.dev_id;
		__entry->req_id  = req->request_id;
		__entry->link    = link;
		__entry->session = link->parent;
	),
	TP_printk(
		"ReqMgr ApplyRequest devname=%s devid=%u request=%lld link=%pK session=%pK",
			__get_str(name), __entry->dev_id, __entry->req_id,
			__entry->link, __entry->session
	)
);

TRACE_EVENT(cam_req_mgr_add_req,
	TP_PROTO(struct cam_req_mgr_core_link *link,
		int idx, struct cam_req_mgr_add_request *add_req,
		struct cam_req_mgr_req_tbl *tbl,
		struct cam_req_mgr_connected_device *dev),
	TP_ARGS(link, idx, add_req, tbl, dev),
	TP_STRUCT__entry(
		__string(name, dev->dev_info.name)
		__field(uint32_t, dev_id)
		__field(uint64_t, req_id)
		__field(uint32_t, slot_id)
		__field(uint32_t, delay)
		__field(uint32_t, readymap)
		__field(uint32_t, devicemap)
		__field(void*, link)
		__field(void*, session)
	),
	TP_fast_assign(
		__assign_str(name, dev->dev_info.name);
		__entry->dev_id    = dev->dev_info.dev_id;
		__entry->req_id    = add_req->req_id;
		__entry->slot_id   = idx;
		__entry->delay     = tbl->pd;
		__entry->readymap  = tbl->slot[idx].req_ready_map;
		__entry->devicemap = tbl->dev_mask;
		__entry->link      = link;
		__entry->session   = link->parent;
	),
	TP_printk(
		"ReqMgr AddRequest devname=%s devid=%d request=%lld slot=%d pd=%d readymap=%x devicemap=%d link=%pK session=%pK",
			__get_str(name), __entry->dev_id, __entry->req_id,
			__entry->slot_id, __entry->delay, __entry->readymap,
			__entry->devicemap, __entry->link, __entry->session
	)
);

TRACE_EVENT(cam_submit_to_hw,
	TP_PROTO(const char *entity, uint64_t req_id),
	TP_ARGS(entity, req_id),
	TP_STRUCT__entry(
		__string(entity, entity)
		__field(uint64_t, req_id)
	),
	TP_fast_assign(
		__assign_str(entity, entity);
		__entry->req_id = req_id;
	),
	TP_printk(
		"%8s: submit request=%llu",
			__get_str(entity), __entry->req_id
	)
);

TRACE_EVENT(cam_irq_activated,
	TP_PROTO(const char *entity, uint32_t irq_type),
	TP_ARGS(entity, irq_type),
	TP_STRUCT__entry(
		__string(entity, entity)
		__field(uint32_t, irq_type)
	),
	TP_fast_assign(
		__assign_str(entity, entity);
		__entry->irq_type = irq_type;
	),
	TP_printk(
		"%8s: got irq type=%d",
			__get_str(entity), __entry->irq_type
	)
);

TRACE_EVENT(cam_irq_handled,
	TP_PROTO(const char *entity, uint32_t irq_type),
	TP_ARGS(entity, irq_type),
	TP_STRUCT__entry(
		__string(entity, entity)
		__field(uint32_t, irq_type)
	),
	TP_fast_assign(
		__assign_str(entity, entity);
		__entry->irq_type = irq_type;
	),
	TP_printk(
		"%8s: handled irq type=%d",
			__get_str(entity), __entry->irq_type
	)
);

TRACE_EVENT(cam_cdm_cb,
	TP_PROTO(const char *entity, uint32_t status),
	TP_ARGS(entity, status),
	TP_STRUCT__entry(
		__string(entity, entity)
		__field(uint32_t, status)
	),
	TP_fast_assign(
		__assign_str(entity, entity);
		__entry->status = status;
	),
	TP_printk(
		"%8s: cdm cb status=%d",
			__get_str(entity), __entry->status
	)
);

#endif /* _CAM_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
