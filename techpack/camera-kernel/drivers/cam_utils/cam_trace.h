/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#if !defined(_CAM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CAM_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM camera
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ./cam_trace

#include <linux/tracepoint.h>
#include <media/cam_req_mgr.h>
#include "cam_req_mgr_core.h"
#include "cam_req_mgr_interface.h"
#include "cam_context.h"

#define CAM_DEFAULT_VALUE 0xFF
#define CAM_TRACE_PRINT_MAX_LEN 512

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
		__field(int32_t, link_hdl)
	),
	TP_fast_assign(
		__entry->ctx = ctx;
		__entry->state = ctx->state;
		__entry->substate = substate;
		__entry->event = event;
		__entry->ts = timestamp;
		__entry->link_hdl = ctx->link_hdl;

	),
	TP_printk(
		"ISP: IRQ ctx=%p ctx_state=%u substate=%u event=%u ts=%llu link_hdl=0x%x",
			__entry->ctx, __entry->state, __entry->substate,
			__entry->event, __entry->ts, __entry->link_hdl
	)
);

TRACE_EVENT(cam_log_event,
	TP_PROTO(const char *string1, const char *string2,
		uint64_t val1, uint64_t val2),
	TP_ARGS(string1, string2, val1, val2),
	TP_STRUCT__entry(
		__string(string1, string1)
		__string(string2, string2)
		__field(uint64_t, val1)
		__field(uint64_t, val2)
	),
	TP_fast_assign(
		__assign_str(string1, string1);
		__assign_str(string2, string2);
		__entry->val1 = val1;
		__entry->val2 = val2;
	),
	TP_printk(
		"%s: %s val1=%llu val2=%llu",
			__get_str(string1), __get_str(string2),
			__entry->val1, __entry->val2
	)
);
/* xiaomi add I2C trace begin */
TRACE_EVENT(cam_i2c_write_log_event,
	TP_PROTO(const char *flag_name, const char *device_name, uint64_t req_id, int32_t j,
			const char *w_r_status, uint32_t reg_addr, uint32_t reg_data),
	TP_ARGS(flag_name, device_name, req_id, j, w_r_status, reg_addr, reg_data),
	TP_STRUCT__entry(
		__string(flag_name, flag_name)
		__string(device_name, device_name)
		__string(w_r_status, w_r_status)
		__field(uint64_t, req_id)
		__field(int32_t, j)
		__field(uint32_t, reg_addr)
		__field(uint32_t, reg_data)
	),
	TP_fast_assign(
		__assign_str(flag_name, flag_name);
		__assign_str(device_name, device_name);
		__assign_str(w_r_status, w_r_status);
		__entry->req_id = req_id;
		__entry->j = j;
		__entry->reg_addr = reg_addr;
		__entry->reg_data = reg_data;
	),
	TP_printk(
		"%s %s req_id %llu-%04d %s addr 0x%04X data 0x%04X",
		__get_str(flag_name), __get_str(device_name),
		__entry->req_id, __entry->j, __get_str(w_r_status),
		__entry->reg_addr, __entry->reg_data
	)
);

TRACE_EVENT(poll_i2c_compare,
	TP_PROTO(uint32_t data, uint32_t reg_data),
	TP_ARGS(data, reg_data),
	TP_STRUCT__entry(
		__field(uint32_t, data)
		__field(uint32_t, reg_data)
	),
	TP_fast_assign(
		__entry->data = data;
		__entry->reg_data = reg_data;
	),
	TP_printk(
		"[POLL_I2C_COMPARE] data 0x%04X reg_data 0x%04X",
		__entry->data, __entry->reg_data
	)
);

TRACE_EVENT(opcode_name,
	TP_PROTO(int32_t opcode_value, const char *opcode_name),
	TP_ARGS(opcode_value, opcode_name),
	TP_STRUCT__entry(
		__field(int32_t, opcode_value)
		__string(opcode_name, opcode_name)
	),
	TP_fast_assign(
		__entry->opcode_value = opcode_value;
		__assign_str(opcode_name, opcode_name);
	),
	TP_printk(
		"sensor request_id = 0 opcode value = %d opcode_name %s start",
		__entry->opcode_value, __get_str(opcode_name)
	)
);
/* xiaomi add I2C trace end */
TRACE_EVENT(cam_log_debug,
	TP_PROTO(const char *fmt, va_list *args),
	TP_ARGS(fmt, args),
	TP_STRUCT__entry(
		__dynamic_array(char, msg, CAM_TRACE_PRINT_MAX_LEN)
	),
	TP_fast_assign(
		va_list ap;

		va_copy(ap, *args);
		vsnprintf(__get_dynamic_array(msg), CAM_TRACE_PRINT_MAX_LEN, fmt, ap);
		va_end(ap);
	),
	TP_printk("%s", __get_str(msg))
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
		__field(int32_t, link_hdl)
		__field(uint64_t, request)
	),
	TP_fast_assign(
		__assign_str(ctx_type, ctx_type);
		__entry->ctx = ctx;
		__entry->link_hdl = ctx->link_hdl;
		__entry->request = req->request_id;
	),
	TP_printk(
		"%5s: BufDone ctx=%p request=%llu link_hdl=0x%x",
			__get_str(ctx_type), __entry->ctx, __entry->request, __entry->link_hdl
	)
);

TRACE_EVENT(cam_apply_req,
	TP_PROTO(const char *entity, uint32_t id, uint64_t req_id,int32_t link_hdl),
	TP_ARGS(entity, id, req_id, link_hdl),
	TP_STRUCT__entry(
		__string(entity, entity)
		__field(uint32_t, id)
		__field(uint64_t, req_id)
		__field(int32_t, link_hdl)
	),
	TP_fast_assign(
		__assign_str(entity, entity);
		__entry->id = id;
		__entry->req_id = req_id;
		__entry->link_hdl = link_hdl;
	),
	TP_printk(
		"%8s: ApplyRequest id=%u request=%llu link_hdl=0x%x",
			__get_str(entity), __entry->id, __entry->req_id, __entry->link_hdl
	)
);

TRACE_EVENT(cam_notify_frame_skip,
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
		"%8s: NotifyFrameSkip request=%llu",
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
		__field(int32_t, link_hdl)
		__field(void*, link)
		__field(void*, session)
	),
	TP_fast_assign(
		__assign_str(name, dev->dev_info.name);
		__entry->dev_id  = dev->dev_info.dev_id;
		__entry->req_id  = req->request_id;
		__entry->link    = link;
		__entry->session = link->parent;
		__entry->link_hdl = req->link_hdl;
	),
	TP_printk(
		"ReqMgr ApplyRequest devname=%s devid=%u request=%lld link=%pK session=%pK link_hdl=0x%x",
			__get_str(name), __entry->dev_id, __entry->req_id,
			__entry->link, __entry->session, __entry->link_hdl
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
		__field(int32_t, link_hdl)
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
		__entry->link_hdl  = link->link_hdl;
		__entry->session   = link->parent;
	),
	TP_printk(
		"ReqMgr AddRequest devname=%s devid=%d request=%lld slot=%d pd=%d readymap=%x devicemap=%d link=%pK session=%pK link_hdl=0x%x",
			__get_str(name), __entry->dev_id, __entry->req_id,
			__entry->slot_id, __entry->delay, __entry->readymap,
			__entry->devicemap, __entry->link, __entry->session,
			__entry->link_hdl
	)
);

TRACE_EVENT(cam_delay_detect,
	TP_PROTO(const char *entity,
		const char *text, uint64_t req_id,
		uint32_t ctx_id, int32_t link_hdl,
		int32_t session_hdl, int rc),
	TP_ARGS(entity, text, req_id, ctx_id,
		link_hdl, session_hdl, rc),
	TP_STRUCT__entry(
		__string(entity, entity)
		__string(text, text)
		__field(uint64_t, req_id)
		__field(uint64_t, ctx_id)
		__field(int32_t, link_hdl)
		__field(int32_t, session_hdl)
		__field(int32_t, rc)
	),
	TP_fast_assign(
		__assign_str(entity, entity);
		__assign_str(text, text);
		__entry->req_id      = req_id;
		__entry->ctx_id      = ctx_id;
		__entry->link_hdl    = link_hdl;
		__entry->session_hdl = session_hdl;
		__entry->rc          = rc;
	),
	TP_printk(
		"%s: %s request=%lld ctx_id=%d link_hdl=0x%x session_hdl=0x%x rc=%d",
			__get_str(entity), __get_str(text), __entry->req_id,
			__entry->ctx_id, __entry->link_hdl,
			__entry->session_hdl, __entry->rc
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
