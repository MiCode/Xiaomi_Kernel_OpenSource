/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ratelimit.h>

#include "cam_mem_mgr.h"
#include "cam_sync_api.h"
#include "cam_req_mgr_dev.h"
#include "cam_trace.h"
#include "cam_debug_util.h"
#include "cam_packet_util.h"
#include "cam_context_utils.h"
#include "cam_cdm_util.h"
#include "cam_isp_context.h"
#include "cam_common_util.h"

static const char isp_dev_name[] = "isp";

#define INC_STATE_MONITOR_HEAD(head) \
	(atomic64_add_return(1, head) % \
	CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES)

static int cam_isp_context_dump_active_request(void *data, unsigned long iova,
	uint32_t buf_info);

static void __cam_isp_ctx_update_state_monitor_array(
	struct cam_isp_context *ctx_isp,
	enum cam_isp_state_change_trigger trigger_type,
	uint32_t req_id)
{
	int iterator = 0;

	iterator = INC_STATE_MONITOR_HEAD(&ctx_isp->state_monitor_head);
	ctx_isp->cam_isp_ctx_state_monitor[iterator].curr_state =
		ctx_isp->substate_activated;
	ctx_isp->cam_isp_ctx_state_monitor[iterator].trigger =
		trigger_type;
	ctx_isp->cam_isp_ctx_state_monitor[iterator].req_id =
		req_id;
	ctx_isp->cam_isp_ctx_state_monitor[iterator].evt_time_stamp =
		jiffies_to_msecs(jiffies);
}

static const char *__cam_isp_ctx_substate_val_to_type(
	uint32_t type)
{
	switch (type) {
	case CAM_ISP_CTX_ACTIVATED_SOF:
		return "SOF";
	case CAM_ISP_CTX_ACTIVATED_APPLIED:
		return "APPLIED";
	case CAM_ISP_CTX_ACTIVATED_EPOCH:
		return "EPOCH";
	case CAM_ISP_CTX_ACTIVATED_BUBBLE:
		return "BUBBLE";
	case CAM_ISP_CTX_ACTIVATED_BUBBLE_APPLIED:
		return "BUBBLE_APPLIED";
	case CAM_ISP_CTX_ACTIVATED_HALT:
		return "HALT";
	default:
		return "CAM_ISP_CTX_INVALID_STATE";
	}
}

static const char *__cam_isp_hw_evt_val_to_type(
	uint32_t evt_id)
{
	switch (evt_id) {
	case CAM_ISP_STATE_CHANGE_TRIGGER_ERROR:
		return "ERROR";
	case CAM_ISP_STATE_CHANGE_TRIGGER_SOF:
		return "SOF";
	case CAM_ISP_STATE_CHANGE_TRIGGER_REG_UPDATE:
		return "REG_UPDATE";
	case CAM_ISP_STATE_CHANGE_TRIGGER_EPOCH:
		return "EPOCH";
	case CAM_ISP_STATE_CHANGE_TRIGGER_EOF:
		return "EOF";
	case CAM_ISP_STATE_CHANGE_TRIGGER_DONE:
		return "DONE";
	default:
		return "CAM_ISP_EVENT_INVALID";
	}
}

static void __cam_isp_ctx_dump_state_monitor_array(
	struct cam_isp_context *ctx_isp)
{
	int i = 0;
	uint64_t state_head = 0;
	uint64_t index;

	state_head = atomic64_read(&ctx_isp->state_monitor_head);
	CAM_ERR_RATE_LIMIT(CAM_ISP,
		"Dumping state information for preceding requests");

	for (i = CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES - 1; i >= 0;
		i--) {
		index = (((state_head - i) +
			CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES) %
			CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES);
		CAM_ERR_RATE_LIMIT(CAM_ISP,
		"time[0x%llx] req_id[%u] state[%s] evt_type[%s]",
		ctx_isp->cam_isp_ctx_state_monitor[index].evt_time_stamp,
		ctx_isp->cam_isp_ctx_state_monitor[index].req_id,
		__cam_isp_ctx_substate_val_to_type(
		ctx_isp->cam_isp_ctx_state_monitor[index].curr_state),
		__cam_isp_hw_evt_val_to_type(
		ctx_isp->cam_isp_ctx_state_monitor[index].trigger));
	}
}

static void cam_isp_ctx_dump_req(struct cam_isp_ctx_req *req_isp)
{
	int i = 0, rc = 0;
	size_t len = 0;
	uint32_t *buf_addr;
	uint32_t *buf_start, *buf_end;
	size_t   remain_len = 0;
	bool     need_put = false;

	for (i = 0; i < req_isp->num_cfg; i++) {
		rc = cam_packet_util_get_cmd_mem_addr(
			req_isp->cfg[i].handle, &buf_addr, &len);
		if (rc) {
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"Failed to get_cmd_mem_addr, rc=%d",
				rc);
		} else {
			if (req_isp->cfg[i].offset >= len) {
				CAM_ERR(CAM_ISP, "Invalid offset");
				need_put = true;
				goto put;
			}
			remain_len = len - req_isp->cfg[i].offset;

			if (req_isp->cfg[i].len > remain_len) {
				CAM_ERR(CAM_ISP, "Invalid offset");
				need_put = true;
			}
put:
			if (need_put) {
				if (cam_mem_put_cpu_buf(req_isp->cfg[i].handle))
					CAM_WARN(CAM_ISP,
						"Failed to put cpu buf: 0x%x",
						req_isp->cfg[i].handle);
				need_put = false;
				continue;
			}

			buf_start = (uint32_t *)((uint8_t *) buf_addr +
				req_isp->cfg[i].offset);
			buf_end = (uint32_t *)((uint8_t *) buf_start +
				req_isp->cfg[i].len - 1);
			cam_cdm_util_dump_cmd_buf(buf_start, buf_end);
			if (cam_mem_put_cpu_buf(req_isp->cfg[i].handle))
				CAM_WARN(CAM_ISP, "Failed to put cpu buf: 0x%x",
					req_isp->cfg[i].handle);
		}
	}
}

static int __cam_isp_ctx_enqueue_request_in_order(
	struct cam_context *ctx, struct cam_ctx_request *req)
{
	struct cam_ctx_request           *req_current;
	struct cam_ctx_request           *req_prev;
	struct list_head                  temp_list;

	INIT_LIST_HEAD(&temp_list);
	spin_lock_bh(&ctx->lock);
	if (list_empty(&ctx->pending_req_list)) {
		list_add_tail(&req->list, &ctx->pending_req_list);
	} else {
		list_for_each_entry_safe_reverse(
			req_current, req_prev, &ctx->pending_req_list, list) {
			if (req->request_id < req_current->request_id) {
				list_del_init(&req_current->list);
				list_add(&req_current->list, &temp_list);
				continue;
			} else if (req->request_id == req_current->request_id) {
				CAM_WARN(CAM_ISP,
					"Received duplicated request %lld",
					req->request_id);
			}
			break;
		}
		list_add_tail(&req->list, &ctx->pending_req_list);

		if (!list_empty(&temp_list)) {
			list_for_each_entry_safe(
				req_current, req_prev, &temp_list, list) {
				list_del_init(&req_current->list);
				list_add_tail(&req_current->list,
					&ctx->pending_req_list);
			}
		}
	}
	spin_unlock_bh(&ctx->lock);
	return 0;
}

static int __cam_isp_ctx_enqueue_init_request(
	struct cam_context *ctx, struct cam_ctx_request *req)
{
	int rc = 0;
	struct cam_ctx_request           *req_old;
	struct cam_isp_ctx_req           *req_isp_old;
	struct cam_isp_ctx_req           *req_isp_new;

	spin_lock_bh(&ctx->lock);
	if (list_empty(&ctx->pending_req_list)) {
		list_add_tail(&req->list, &ctx->pending_req_list);
		CAM_DBG(CAM_ISP, "INIT packet added req id= %d",
			req->request_id);
		goto end;
	}

	req_old = list_first_entry(&ctx->pending_req_list,
		struct cam_ctx_request, list);
	req_isp_old = (struct cam_isp_ctx_req *) req_old->req_priv;
	req_isp_new = (struct cam_isp_ctx_req *) req->req_priv;
	if (req_isp_old->hw_update_data.packet_opcode_type ==
		CAM_ISP_PACKET_INIT_DEV) {
		if ((req_isp_old->num_cfg + req_isp_new->num_cfg) >=
			CAM_ISP_CTX_CFG_MAX) {
			CAM_WARN(CAM_ISP, "Can not merge INIT pkt");
			rc = -ENOMEM;
		}

		if (req_isp_old->num_fence_map_out != 0 ||
			req_isp_old->num_fence_map_in != 0) {
			CAM_WARN(CAM_ISP, "Invalid INIT pkt sequence");
			rc = -EINVAL;
		}

		if (!rc) {
			memcpy(req_isp_old->fence_map_out,
				req_isp_new->fence_map_out,
				sizeof(req_isp_new->fence_map_out[0])*
				req_isp_new->num_fence_map_out);
			req_isp_old->num_fence_map_out =
				req_isp_new->num_fence_map_out;

			memcpy(req_isp_old->fence_map_in,
				req_isp_new->fence_map_in,
				sizeof(req_isp_new->fence_map_in[0])*
				req_isp_new->num_fence_map_in);
			req_isp_old->num_fence_map_in =
				req_isp_new->num_fence_map_in;

			memcpy(&req_isp_old->cfg[req_isp_old->num_cfg],
				req_isp_new->cfg,
				sizeof(req_isp_new->cfg[0])*
				req_isp_new->num_cfg);
			req_isp_old->num_cfg += req_isp_new->num_cfg;

			req_old->request_id = req->request_id;

			list_add_tail(&req->list, &ctx->free_req_list);
		}
	} else {
		CAM_WARN(CAM_ISP,
			"Received Update pkt before INIT pkt. req_id= %lld",
			req->request_id);
		rc = -EINVAL;
	}
end:
	spin_unlock_bh(&ctx->lock);
	return rc;
}

static const char *__cam_isp_resource_handle_id_to_type(
	uint32_t resource_handle)
{
	switch (resource_handle) {
	case CAM_ISP_IFE_OUT_RES_FULL:
		return "FULL";
	case CAM_ISP_IFE_OUT_RES_DS4:
		return "DS4";
	case CAM_ISP_IFE_OUT_RES_DS16:
		return "DS16";
	case CAM_ISP_IFE_OUT_RES_RAW_DUMP:
		return "RAW_DUMP";
	case CAM_ISP_IFE_OUT_RES_FD:
		return "FD";
	case CAM_ISP_IFE_OUT_RES_PDAF:
		return "PDAF";
	case CAM_ISP_IFE_OUT_RES_2PD:
		return "2PD";
	case CAM_ISP_IFE_OUT_RES_RDI_0:
		return "RDI_0";
	case CAM_ISP_IFE_OUT_RES_RDI_1:
		return "RDI_1";
	case CAM_ISP_IFE_OUT_RES_RDI_2:
		return "RDI_2";
	case CAM_ISP_IFE_OUT_RES_RDI_3:
		return "RDI_3";
	case CAM_ISP_IFE_OUT_RES_STATS_HDR_BE:
		return "STATS_HDR_BE";
	case CAM_ISP_IFE_OUT_RES_STATS_HDR_BHIST:
		return "STATS_HDR_BHIST";
	case CAM_ISP_IFE_OUT_RES_STATS_TL_BG:
		return "STATS_TL_BG";
	case CAM_ISP_IFE_OUT_RES_STATS_BF:
		return "STATS_BF";
	case CAM_ISP_IFE_OUT_RES_STATS_AWB_BG:
		return "STATS_AWB_BG";
	case CAM_ISP_IFE_OUT_RES_STATS_BHIST:
		return "STATS_BHIST";
	case CAM_ISP_IFE_OUT_RES_STATS_RS:
		return "STATS_RS";
	case CAM_ISP_IFE_OUT_RES_STATS_CS:
		return "STATS_CS";
	default:
		return "CAM_ISP_Invalid_Resource_Type";
	}
}

static uint64_t __cam_isp_ctx_get_event_ts(uint32_t evt_id, void *evt_data)
{
	uint64_t ts = 0;

	if (!evt_data)
		return 0;

	switch (evt_id) {
	case CAM_ISP_HW_EVENT_ERROR:
		ts = ((struct cam_isp_hw_error_event_data *)evt_data)->
			timestamp;
		break;
	case CAM_ISP_HW_EVENT_SOF:
		ts = ((struct cam_isp_hw_sof_event_data *)evt_data)->
			timestamp;
		break;
	case CAM_ISP_HW_EVENT_REG_UPDATE:
		ts = ((struct cam_isp_hw_reg_update_event_data *)evt_data)->
			timestamp;
		break;
	case CAM_ISP_HW_EVENT_EPOCH:
		ts = ((struct cam_isp_hw_epoch_event_data *)evt_data)->
			timestamp;
		break;
	case CAM_ISP_HW_EVENT_EOF:
		ts = ((struct cam_isp_hw_eof_event_data *)evt_data)->
			timestamp;
		break;
	case CAM_ISP_HW_EVENT_DONE:
		break;
	default:
		CAM_DBG(CAM_ISP, "Invalid Event Type %d", evt_id);
	}

	return ts;
}

static void __cam_isp_ctx_handle_buf_done_fail_log(
	struct cam_isp_ctx_req *req_isp)
{
	int i;

	if (req_isp->num_fence_map_out >= CAM_ISP_CTX_RES_MAX) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Num Resources exceed mMAX %d >= %d ",
			req_isp->num_fence_map_out, CAM_ISP_CTX_RES_MAX);
		return;
	}

	CAM_ERR_RATE_LIMIT(CAM_ISP,
		"Resource Handles that fail to generate buf_done in prev frame");
	for (i = 0; i < req_isp->num_fence_map_out; i++) {
		if (req_isp->fence_map_out[i].sync_id != -1)
			CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Resource_Handle: [%s] Sync_ID: [0x%x]",
			__cam_isp_resource_handle_id_to_type(
			req_isp->fence_map_out[i].resource_handle),
			req_isp->fence_map_out[i].sync_id);
	}
}

static int __cam_isp_ctx_handle_buf_done_in_activated_state(
	struct cam_isp_context *ctx_isp,
	struct cam_isp_hw_done_event_data *done,
	uint32_t bubble_state)
{
	int rc = 0;
	int i, j;
	struct cam_ctx_request  *req;
	struct cam_isp_ctx_req  *req_isp;
	struct cam_context *ctx = ctx_isp->base;

	if (list_empty(&ctx->active_req_list)) {
		CAM_DBG(CAM_ISP, "Buf done with no active request!");
		goto end;
	}

	CAM_DBG(CAM_ISP, "Enter with bubble_state %d", bubble_state);

	req = list_first_entry(&ctx->active_req_list,
			struct cam_ctx_request, list);

	trace_cam_buf_done("ISP", ctx, req);

	req_isp = (struct cam_isp_ctx_req *) req->req_priv;
	for (i = 0; i < done->num_handles; i++) {
		for (j = 0; j < req_isp->num_fence_map_out; j++) {
			if (done->resource_handle[i] ==
				req_isp->fence_map_out[j].resource_handle)
				break;
		}

		if (j == req_isp->num_fence_map_out) {
			CAM_ERR(CAM_ISP,
				"Can not find matching lane handle 0x%x!",
				done->resource_handle[i]);
			rc = -EINVAL;
			continue;
		}

		if (req_isp->fence_map_out[j].sync_id == -1) {
			__cam_isp_ctx_handle_buf_done_fail_log(req_isp);
			continue;
		}

		if (!req_isp->bubble_detected) {
			CAM_DBG(CAM_ISP,
				"Sync with success: req %lld res 0x%x fd 0x%x, ctx %u",
				req->request_id,
				req_isp->fence_map_out[j].resource_handle,
				req_isp->fence_map_out[j].sync_id,
				ctx->ctx_id);

			rc = cam_sync_signal(req_isp->fence_map_out[j].sync_id,
				CAM_SYNC_STATE_SIGNALED_SUCCESS);
			if (rc)
				CAM_DBG(CAM_ISP, "Sync failed with rc = %d",
					 rc);
		} else if (!req_isp->bubble_report) {
			CAM_ERR(CAM_ISP,
				"Sync with failure: req %lld res 0x%x fd 0x%x, ctx %u",
				req->request_id,
				req_isp->fence_map_out[j].resource_handle,
				req_isp->fence_map_out[j].sync_id,
				ctx->ctx_id);

			rc = cam_sync_signal(req_isp->fence_map_out[j].sync_id,
				CAM_SYNC_STATE_SIGNALED_ERROR);
			if (rc)
				CAM_ERR(CAM_ISP, "Sync failed with rc = %d",
					rc);
		} else {
			/*
			 * Ignore the buffer done if bubble detect is on
			 * Increment the ack number here, and queue the
			 * request back to pending list whenever all the
			 * buffers are done.
			 */
			req_isp->num_acked++;
			CAM_DBG(CAM_ISP,
				"buf done with bubble state %d recovery %d",
				bubble_state, req_isp->bubble_report);
			continue;
		}

		CAM_DBG(CAM_ISP, "req %lld, reset sync id 0x%x ctx %u",
			req->request_id,
			req_isp->fence_map_out[j].sync_id, ctx->ctx_id);
		if (!rc) {
			req_isp->num_acked++;
			req_isp->fence_map_out[j].sync_id = -1;
		}
	}

	if (req_isp->num_acked > req_isp->num_fence_map_out) {
		/* Should not happen */
		CAM_ERR(CAM_ISP,
			"WARNING: req_id %lld num_acked %d > map_out %d, ctx %u",
			req->request_id, req_isp->num_acked,
			req_isp->num_fence_map_out, ctx->ctx_id);
		WARN_ON(req_isp->num_acked > req_isp->num_fence_map_out);
	}

	if (req_isp->num_acked != req_isp->num_fence_map_out)
		return rc;

	ctx_isp->active_req_cnt--;

	if (req_isp->bubble_detected && req_isp->bubble_report) {
		req_isp->num_acked = 0;
		req_isp->bubble_detected = false;
		list_del_init(&req->list);
		list_add(&req->list, &ctx->pending_req_list);
		atomic_set(&ctx_isp->process_bubble, 0);

		CAM_DBG(CAM_REQ,
			"Move active request %lld to pending list(cnt = %d) [bubble recovery], ctx %u",
			 req->request_id, ctx_isp->active_req_cnt, ctx->ctx_id);
	} else {
		list_del_init(&req->list);
		list_add_tail(&req->list, &ctx->free_req_list);

		CAM_DBG(CAM_REQ,
			"Move active request %lld to free list(cnt = %d) [all fences done], ctx %u",
			 req->request_id, ctx_isp->active_req_cnt, ctx->ctx_id);
	}

end:
	__cam_isp_ctx_update_state_monitor_array(ctx_isp,
		CAM_ISP_STATE_CHANGE_TRIGGER_DONE,
		ctx_isp->base->req_list->request_id);
	return rc;
}

static void __cam_isp_ctx_send_sof_boot_timestamp(
	struct cam_isp_context *ctx_isp, uint64_t request_id,
	uint32_t sof_event_status)
{
	struct cam_req_mgr_message   req_msg;

	req_msg.session_hdl = ctx_isp->base->session_hdl;
	req_msg.u.frame_msg.frame_id = ctx_isp->frame_id;
	req_msg.u.frame_msg.request_id = request_id;
	req_msg.u.frame_msg.timestamp = ctx_isp->boot_timestamp;
	req_msg.u.frame_msg.link_hdl = ctx_isp->base->link_hdl;
	req_msg.u.frame_msg.sof_status = sof_event_status;

	CAM_DBG(CAM_ISP,
		"request id:%lld frame number:%lld boot time stamp:0x%llx",
		 request_id, ctx_isp->frame_id,
		 ctx_isp->boot_timestamp);

	if (cam_req_mgr_notify_message(&req_msg,
		V4L_EVENT_CAM_REQ_MGR_SOF_BOOT_TS,
		V4L_EVENT_CAM_REQ_MGR_EVENT))
		CAM_ERR(CAM_ISP,
			"Error in notifying the boot time for req id:%lld",
			request_id);
}


static void __cam_isp_ctx_send_sof_timestamp(
	struct cam_isp_context *ctx_isp, uint64_t request_id,
	uint32_t sof_event_status)
{
	struct cam_req_mgr_message   req_msg;

	req_msg.session_hdl = ctx_isp->base->session_hdl;
	req_msg.u.frame_msg.frame_id = ctx_isp->frame_id;
	req_msg.u.frame_msg.request_id = request_id;
	req_msg.u.frame_msg.timestamp = ctx_isp->sof_timestamp_val;
	req_msg.u.frame_msg.link_hdl = ctx_isp->base->link_hdl;
	req_msg.u.frame_msg.sof_status = sof_event_status;

	CAM_DBG(CAM_ISP,
		"request id:%lld frame number:%lld SOF time stamp:0x%llx",
		 request_id, ctx_isp->frame_id,
		ctx_isp->sof_timestamp_val);
	CAM_DBG(CAM_ISP, "sof status:%d", sof_event_status);

	if (cam_req_mgr_notify_message(&req_msg,
		V4L_EVENT_CAM_REQ_MGR_SOF, V4L_EVENT_CAM_REQ_MGR_EVENT))
		CAM_ERR(CAM_ISP,
			"Error in notifying the sof time for req id:%lld",
			request_id);

	__cam_isp_ctx_send_sof_boot_timestamp(ctx_isp,
		request_id, sof_event_status);

}

static int __cam_isp_ctx_reg_upd_in_epoch_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	if (ctx_isp->frame_id == 1)
		CAM_DBG(CAM_ISP, "Reg update for early PCR");
	else
		CAM_WARN(CAM_ISP,
			"Unexpected reg update in activated substate:%d for frame_id:%lld",
			ctx_isp->substate_activated, ctx_isp->frame_id);
	return 0;
}

static int __cam_isp_ctx_reg_upd_in_activated_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_ctx_request  *req;
	struct cam_context      *ctx = ctx_isp->base;
	struct cam_isp_ctx_req  *req_isp;

	if (list_empty(&ctx->wait_req_list)) {
		CAM_ERR(CAM_ISP, "Reg upd ack with no waiting request");
		goto end;
	}
	req = list_first_entry(&ctx->wait_req_list,
			struct cam_ctx_request, list);
	list_del_init(&req->list);

	req_isp = (struct cam_isp_ctx_req *) req->req_priv;
	if (req_isp->num_fence_map_out != 0) {
		list_add_tail(&req->list, &ctx->active_req_list);
		ctx_isp->active_req_cnt++;
		CAM_DBG(CAM_REQ,
			"move request %lld to active list(cnt = %d), ctx %u",
			req->request_id, ctx_isp->active_req_cnt, ctx->ctx_id);
	} else {
		/* no io config, so the request is completed. */
		list_add_tail(&req->list, &ctx->free_req_list);
		CAM_DBG(CAM_ISP,
			"move active request %lld to free list(cnt = %d), ctx %u",
			req->request_id, ctx_isp->active_req_cnt, ctx->ctx_id);
	}

	/*
	 * This function only called directly from applied and bubble applied
	 * state so change substate here.
	 */
	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_EPOCH;
	CAM_DBG(CAM_ISP, "next substate %d", ctx_isp->substate_activated);

end:
	return rc;
}

static int __cam_isp_ctx_notify_sof_in_activated_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_req_mgr_trigger_notify  notify;
	struct cam_context *ctx = ctx_isp->base;
	struct cam_ctx_request  *req;
	uint64_t  request_id  = 0;

	/*
	 * notify reqmgr with sof signal. Note, due to scheduling delay
	 * we can run into situation that two active requests has already
	 * be in the active queue while we try to do the notification.
	 * In this case, we need to skip the current notification. This
	 * helps the state machine to catch up the delay.
	 */
	if (ctx->ctx_crm_intf && ctx->ctx_crm_intf->notify_trigger &&
		ctx_isp->active_req_cnt <= 2) {
		if (ctx_isp->subscribe_event & CAM_TRIGGER_POINT_SOF) {
			notify.link_hdl = ctx->link_hdl;
			notify.dev_hdl = ctx->dev_hdl;
			notify.frame_id = ctx_isp->frame_id;
			notify.trigger = CAM_TRIGGER_POINT_SOF;

			ctx->ctx_crm_intf->notify_trigger(&notify);
			CAM_DBG(CAM_ISP, "Notify CRM  SOF frame %lld ctx %u",
				ctx_isp->frame_id, ctx->ctx_id);
		}

		list_for_each_entry(req, &ctx->active_req_list, list) {
			if (req->request_id > ctx_isp->reported_req_id) {
				request_id = req->request_id;
				ctx_isp->reported_req_id = request_id;
				break;
			}
		}

		if (ctx_isp->substate_activated == CAM_ISP_CTX_ACTIVATED_BUBBLE)
			request_id = 0;

		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);
	} else {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Can not notify SOF to CRM for ctx %u",
			ctx->ctx_id);
		rc = -EFAULT;
	}

	return 0;
}

static int __cam_isp_ctx_notify_eof_in_activated_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_req_mgr_trigger_notify  notify;
	struct cam_context *ctx = ctx_isp->base;

	if (!(ctx_isp->subscribe_event & CAM_TRIGGER_POINT_EOF))
		return rc;

	/* notify reqmgr with eof signal */
	if (ctx->ctx_crm_intf && ctx->ctx_crm_intf->notify_trigger) {
		notify.link_hdl = ctx->link_hdl;
		notify.dev_hdl = ctx->dev_hdl;
		notify.frame_id = ctx_isp->frame_id;
		notify.trigger = CAM_TRIGGER_POINT_EOF;

		ctx->ctx_crm_intf->notify_trigger(&notify);
		CAM_DBG(CAM_ISP, "Notify CRM EOF frame %lld ctx %u",
			ctx_isp->frame_id, ctx->ctx_id);
	} else {
		CAM_ERR(CAM_ISP, "Can not notify EOF to CRM for ctx %u",
			ctx->ctx_id);
		rc = -EFAULT;
	}

	return rc;
}

static int __cam_isp_ctx_reg_upd_in_hw_error(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_SOF;
	return 0;
}

static int __cam_isp_ctx_sof_in_activated_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_isp_hw_sof_event_data      *sof_event_data = evt_data;
	struct cam_ctx_request *req;
	struct cam_context *ctx = ctx_isp->base;

	req = list_last_entry(&ctx->pending_req_list,
		struct cam_ctx_request, list);

	if (!evt_data) {
		CAM_ERR(CAM_ISP, "in valid sof event data");
		return -EINVAL;
	}

	ctx_isp->frame_id++;
	ctx_isp->sof_timestamp_val = sof_event_data->timestamp;
	ctx_isp->boot_timestamp = sof_event_data->boot_time;
	__cam_isp_ctx_update_state_monitor_array(ctx_isp,
		CAM_ISP_STATE_CHANGE_TRIGGER_SOF, req->request_id);
	CAM_DBG(CAM_ISP, "frame id: %lld time stamp:0x%llx, ctx %u",
		ctx_isp->frame_id, ctx_isp->sof_timestamp_val, ctx->ctx_id);

	return rc;
}

static int __cam_isp_ctx_reg_upd_in_sof(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int rc = 0;
	struct cam_ctx_request *req = NULL;
	struct cam_isp_ctx_req *req_isp;
	struct cam_context *ctx = ctx_isp->base;

	if (ctx->state != CAM_CTX_ACTIVATED && ctx_isp->frame_id > 1) {
		CAM_DBG(CAM_ISP, "invalid RUP");
		goto end;
	}

	/*
	 * This is for the first update. The initial setting will
	 * cause the reg_upd in the first frame.
	 */
	if (!list_empty(&ctx->wait_req_list)) {
		req = list_first_entry(&ctx->wait_req_list,
			struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		if (req_isp->num_fence_map_out == req_isp->num_acked)
			list_add_tail(&req->list, &ctx->free_req_list);
		else
			CAM_ERR(CAM_ISP,
				"receive rup in unexpected state");
	}
	if (req != NULL) {
		__cam_isp_ctx_update_state_monitor_array(ctx_isp,
			CAM_ISP_STATE_CHANGE_TRIGGER_REG_UPDATE,
			req->request_id);
	}
end:
	return rc;
}

static int __cam_isp_ctx_epoch_in_applied(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	struct cam_ctx_request    *req;
	struct cam_isp_ctx_req    *req_isp;
	struct cam_context        *ctx = ctx_isp->base;
	uint64_t  request_id = 0;

	if (list_empty(&ctx->wait_req_list)) {
		/*
		 * If no wait req in epoch, this is an error case.
		 * The recovery is to go back to sof state
		 */
		CAM_ERR(CAM_ISP, "No wait request");
		ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_SOF;

		/* Send SOF event as empty frame*/
		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);

		goto end;
	}

	req = list_first_entry(&ctx->wait_req_list, struct cam_ctx_request,
		list);
	req_isp = (struct cam_isp_ctx_req *)req->req_priv;
	req_isp->bubble_detected = true;

	CAM_DBG(CAM_ISP, "Report Bubble flag %d", req_isp->bubble_report);
	if (req_isp->bubble_report && ctx->ctx_crm_intf &&
		ctx->ctx_crm_intf->notify_err) {
		struct cam_req_mgr_error_notify notify;

		notify.link_hdl = ctx->link_hdl;
		notify.dev_hdl = ctx->dev_hdl;
		notify.req_id = req->request_id;
		notify.error = CRM_KMD_ERR_BUBBLE;
		ctx->ctx_crm_intf->notify_err(&notify);
		atomic_set(&ctx_isp->process_bubble, 1);
		CAM_DBG(CAM_ISP, "Notify CRM about Bubble frame %lld, ctx %u",
			ctx_isp->frame_id, ctx->ctx_id);
	} else {
		req_isp->bubble_report = 0;
	}

	/*
	 * Always move the request to active list. Let buf done
	 * function handles the rest.
	 */
	CAM_DBG(CAM_REQ, "move request %lld to active list(cnt = %d), ctx %u",
		req->request_id, ctx_isp->active_req_cnt, ctx->ctx_id);
	ctx_isp->active_req_cnt++;
	list_del_init(&req->list);
	list_add_tail(&req->list, &ctx->active_req_list);

	if (req->request_id > ctx_isp->reported_req_id) {
		request_id = req->request_id;
		ctx_isp->reported_req_id = request_id;
	}
	__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
		CAM_REQ_MGR_SOF_EVENT_ERROR);

	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_BUBBLE;
	CAM_DBG(CAM_ISP, "next substate %d",
		ctx_isp->substate_activated);
end:
	if (request_id == 0) {
		req = list_last_entry(&ctx->active_req_list,
			struct cam_ctx_request, list);
		__cam_isp_ctx_update_state_monitor_array(ctx_isp,
			CAM_ISP_STATE_CHANGE_TRIGGER_EPOCH, req->request_id);
	} else {
		__cam_isp_ctx_update_state_monitor_array(ctx_isp,
			CAM_ISP_STATE_CHANGE_TRIGGER_EPOCH, request_id);
	}
	return 0;
}


static int __cam_isp_ctx_buf_done_in_applied(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int rc = 0;
	struct cam_isp_hw_done_event_data *done =
		(struct cam_isp_hw_done_event_data *) evt_data;

	rc = __cam_isp_ctx_handle_buf_done_in_activated_state(ctx_isp, done, 0);
	return rc;
}


static int __cam_isp_ctx_sof_in_epoch(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int rc = 0;
	struct cam_context                    *ctx = ctx_isp->base;
	struct cam_isp_hw_sof_event_data      *sof_event_data = evt_data;
	struct cam_ctx_request *req;

	if (!evt_data) {
		CAM_ERR(CAM_ISP, "in valid sof event data");
		return -EINVAL;
	}

	ctx_isp->frame_id++;
	ctx_isp->sof_timestamp_val = sof_event_data->timestamp;
	ctx_isp->boot_timestamp = sof_event_data->boot_time;

	if (list_empty(&ctx->active_req_list))
		ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_SOF;
	else
		CAM_DBG(CAM_ISP, "Still need to wait for the buf done");

	req = list_last_entry(&ctx->active_req_list,
		struct cam_ctx_request, list);
	if (req)
		__cam_isp_ctx_update_state_monitor_array(ctx_isp,
			CAM_ISP_STATE_CHANGE_TRIGGER_SOF,
			ctx->req_list->request_id);
	CAM_DBG(CAM_ISP, "next substate %d",
		ctx_isp->substate_activated);

	return rc;
}

static int __cam_isp_ctx_buf_done_in_epoch(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int rc = 0;
	struct cam_isp_hw_done_event_data *done =
		(struct cam_isp_hw_done_event_data *) evt_data;

	rc = __cam_isp_ctx_handle_buf_done_in_activated_state(ctx_isp, done, 0);
	return rc;
}

static int __cam_isp_ctx_buf_done_in_bubble(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_isp_hw_done_event_data *done =
		(struct cam_isp_hw_done_event_data *) evt_data;

	rc = __cam_isp_ctx_handle_buf_done_in_activated_state(ctx_isp, done, 1);
	return rc;
}

static int __cam_isp_ctx_epoch_in_bubble_applied(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	struct cam_ctx_request    *req;
	struct cam_isp_ctx_req    *req_isp;
	struct cam_context        *ctx = ctx_isp->base;
	uint64_t  request_id = 0;

	/*
	 * This means we missed the reg upd ack. So we need to
	 * transition to BUBBLE state again.
	 */

	if (list_empty(&ctx->wait_req_list)) {
		/*
		 * If no pending req in epoch, this is an error case.
		 * Just go back to the bubble state.
		 */
		CAM_ERR(CAM_ISP, "No pending request.");
		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);

		ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_BUBBLE;
		goto end;
	}

	req = list_first_entry(&ctx->wait_req_list, struct cam_ctx_request,
		list);
	req_isp = (struct cam_isp_ctx_req *)req->req_priv;
	req_isp->bubble_detected = true;

	if (req_isp->bubble_report && ctx->ctx_crm_intf &&
		ctx->ctx_crm_intf->notify_err) {
		struct cam_req_mgr_error_notify notify;

		notify.link_hdl = ctx->link_hdl;
		notify.dev_hdl = ctx->dev_hdl;
		notify.req_id = req->request_id;
		notify.error = CRM_KMD_ERR_BUBBLE;
		ctx->ctx_crm_intf->notify_err(&notify);
		CAM_DBG(CAM_REQ,
			"Notify CRM about Bubble req_id %llu frame %lld, ctx %u",
			req->request_id, ctx_isp->frame_id, ctx->ctx_id);
	} else {
		req_isp->bubble_report = 0;
	}

	/*
	 * Always move the request to active list. Let buf done
	 * function handles the rest.
	 */
	CAM_DBG(CAM_ISP, "move request %lld to active list(cnt = %d) ctx %u",
		req->request_id, ctx_isp->active_req_cnt);
	ctx_isp->active_req_cnt++;
	list_del_init(&req->list);
	list_add_tail(&req->list, &ctx->active_req_list);

	if (!req_isp->bubble_report) {
		if (req->request_id > ctx_isp->reported_req_id) {
			request_id = req->request_id;
			ctx_isp->reported_req_id = request_id;
			__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_ERROR);
		} else
			__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
				CAM_REQ_MGR_SOF_EVENT_SUCCESS);
	} else
		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);

	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_BUBBLE;
	CAM_DBG(CAM_ISP, "next substate %d", ctx_isp->substate_activated);
end:
	req = list_last_entry(&ctx->active_req_list, struct cam_ctx_request,
		list);
	if (req)
		__cam_isp_ctx_update_state_monitor_array(ctx_isp,
			CAM_ISP_STATE_CHANGE_TRIGGER_EPOCH, req->request_id);
	return 0;
}

static int __cam_isp_ctx_buf_done_in_bubble_applied(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_isp_hw_done_event_data *done =
		(struct cam_isp_hw_done_event_data *) evt_data;

	rc = __cam_isp_ctx_handle_buf_done_in_activated_state(ctx_isp, done, 1);
	__cam_isp_ctx_update_state_monitor_array(ctx_isp,
		CAM_ISP_STATE_CHANGE_TRIGGER_DONE,
		ctx_isp->base->req_list->request_id);
	return rc;
}

static int __cam_isp_ctx_handle_error(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int                              rc = 0;
	uint32_t                         i = 0;
	bool                             found = 0;
	struct cam_ctx_request          *req = NULL;
	struct cam_ctx_request          *req_to_report = NULL;
	struct cam_ctx_request          *req_to_dump = NULL;
	struct cam_ctx_request          *req_temp;
	struct cam_isp_ctx_req          *req_isp = NULL;
	struct cam_isp_ctx_req          *req_isp_to_report = NULL;
	struct cam_req_mgr_error_notify  notify;
	uint64_t                         error_request_id;
	struct cam_hw_fence_map_entry   *fence_map_out = NULL;
	struct cam_req_mgr_message       req_msg;

	struct cam_context *ctx = ctx_isp->base;
	struct cam_isp_hw_error_event_data  *error_event_data =
			(struct cam_isp_hw_error_event_data *)evt_data;

	uint32_t error_type = error_event_data->error_type;

	CAM_DBG(CAM_ISP, "Enter error_type = %d", error_type);
	if ((error_type == CAM_ISP_HW_ERROR_OVERFLOW) ||
		(error_type == CAM_ISP_HW_ERROR_BUSIF_OVERFLOW))
		notify.error = CRM_KMD_ERR_OVERFLOW;

	/*
	 * The error is likely caused by first request on the active list.
	 * If active list is empty check wait list (maybe error hit as soon
	 * as RUP and we handle error before RUP.
	 */
	if (list_empty(&ctx->active_req_list)) {
		CAM_DBG(CAM_ISP,
			"handling error with no active request");
		if (list_empty(&ctx->wait_req_list)) {
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"Error with no active/wait request");
			goto end;
		} else {
			req_to_dump = list_first_entry(&ctx->wait_req_list,
				struct cam_ctx_request, list);
		}
	} else {
		req_to_dump = list_first_entry(&ctx->active_req_list,
			struct cam_ctx_request, list);
	}

	req_isp = (struct cam_isp_ctx_req *) req_to_dump->req_priv;

	if (error_event_data->enable_reg_dump)
		cam_isp_ctx_dump_req(req_isp);

	__cam_isp_ctx_update_state_monitor_array(ctx_isp,
		CAM_ISP_STATE_CHANGE_TRIGGER_ERROR, req_to_dump->request_id);

	list_for_each_entry_safe(req, req_temp,
		&ctx->active_req_list, list) {
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		if (!req_isp->bubble_report) {
			for (i = 0; i < req_isp->num_fence_map_out; i++) {
				fence_map_out =
					&req_isp->fence_map_out[i];
				CAM_ERR(CAM_ISP,
					"req %llu, Sync fd 0x%x ctx %u",
					req->request_id,
					req_isp->fence_map_out[i].sync_id,
					ctx->ctx_id);
				if (req_isp->fence_map_out[i].sync_id != -1) {
					rc = cam_sync_signal(
						fence_map_out->sync_id,
						CAM_SYNC_STATE_SIGNALED_ERROR);
					fence_map_out->sync_id = -1;
				}
			}
			list_del_init(&req->list);
			list_add_tail(&req->list, &ctx->free_req_list);
			ctx_isp->active_req_cnt--;
		} else {
			found = 1;
			break;
		}
	}

	if (found)
		goto move_to_pending;

	list_for_each_entry_safe(req, req_temp,
		&ctx->wait_req_list, list) {
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		if (!req_isp->bubble_report) {
			for (i = 0; i < req_isp->num_fence_map_out; i++) {
				fence_map_out =
					&req_isp->fence_map_out[i];
				CAM_ERR(CAM_ISP,
					"req %llu, Sync fd 0x%x ctx %u",
					req->request_id,
					req_isp->fence_map_out[i].sync_id,
					ctx->ctx_id);
				if (req_isp->fence_map_out[i].sync_id != -1) {
					rc = cam_sync_signal(
						fence_map_out->sync_id,
						CAM_SYNC_STATE_SIGNALED_ERROR);
					fence_map_out->sync_id = -1;
				}
			}
			list_del_init(&req->list);
			list_add_tail(&req->list, &ctx->free_req_list);
			ctx_isp->active_req_cnt--;
		} else {
			found = 1;
			break;
		}
	}

move_to_pending:
	/*
	 * If bubble recovery is enabled on any request we need to move that
	 * request and all the subsequent requests to the pending list.
	 * Note:
	 * We need to traverse the active list in reverse order and add
	 * to head of pending list.
	 * e.g. pending current state: 10, 11 | active current state: 8, 9
	 * intermittent for loop iteration- pending: 9, 10, 11 | active: 8
	 * final state - pending: 8, 9, 10, 11 | active: NULL
	 */
	if (found) {
		list_for_each_entry_safe_reverse(req, req_temp,
			&ctx->active_req_list, list) {
			req_isp = (struct cam_isp_ctx_req *) req->req_priv;
			list_del_init(&req->list);
			list_add(&req->list, &ctx->pending_req_list);
			ctx_isp->active_req_cnt--;
		}
		list_for_each_entry_safe_reverse(req, req_temp,
			&ctx->wait_req_list, list) {
			req_isp = (struct cam_isp_ctx_req *) req->req_priv;
			list_del_init(&req->list);
			list_add(&req->list, &ctx->pending_req_list);
			ctx_isp->active_req_cnt--;
		}
	}

end:
	do {
		if (list_empty(&ctx->pending_req_list)) {
			error_request_id = ctx_isp->last_applied_req_id + 1;
			req_isp = NULL;
			break;
		}
		req = list_first_entry(&ctx->pending_req_list,
			struct cam_ctx_request, list);
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		error_request_id = ctx_isp->last_applied_req_id;

		if (req_isp->bubble_report) {
			req_to_report = req;
			req_isp_to_report = req_to_report->req_priv;
			break;
		}

		for (i = 0; i < req_isp->num_fence_map_out; i++) {
			if (req_isp->fence_map_out[i].sync_id != -1)
				rc = cam_sync_signal(
					req_isp->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
			req_isp->fence_map_out[i].sync_id = -1;
		}
		list_del_init(&req->list);
		list_add_tail(&req->list, &ctx->free_req_list);

	} while (req->request_id < ctx_isp->last_applied_req_id);

	if (ctx->ctx_crm_intf && ctx->ctx_crm_intf->notify_err) {
		notify.link_hdl = ctx->link_hdl;
		notify.dev_hdl = ctx->dev_hdl;
		notify.req_id = error_request_id;

		if (req_isp_to_report && req_isp_to_report->bubble_report) {
			if (error_event_data->recovery_enabled)
				notify.error = CRM_KMD_ERR_BUBBLE;
		} else {
			notify.error = CRM_KMD_ERR_FATAL;
		}

		CAM_WARN(CAM_ISP, "Notify CRM: req %lld, frame %lld ctx %u",
			error_request_id, ctx_isp->frame_id, ctx->ctx_id);

		ctx->ctx_crm_intf->notify_err(&notify);

		/*
		 * Need to send error occurred in KMD
		 * This will help UMD to take necessary action
		 * and to dump relevant info
		 */

		if (notify.error == CRM_KMD_ERR_OVERFLOW) {
			req_msg.session_hdl = ctx_isp->base->session_hdl;
			req_msg.u.err_msg.device_hdl = ctx_isp->base->dev_hdl;
			req_msg.u.err_msg.error_type =
				CAM_REQ_MGR_ERROR_TYPE_RECOVERY;
			req_msg.u.err_msg.link_hdl = ctx_isp->base->link_hdl;
			req_msg.u.err_msg.request_id = error_request_id;
			req_msg.u.err_msg.resource_size = 0x0;

			if (cam_req_mgr_notify_message(&req_msg,
					V4L_EVENT_CAM_REQ_MGR_ERROR,
					V4L_EVENT_CAM_REQ_MGR_EVENT))
				CAM_ERR(CAM_ISP,
					"Error in notifying the error time for req id:%lld ctx %u",
						ctx_isp->last_applied_req_id,
						ctx->ctx_id);
		}
		ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_HW_ERROR;
	} else {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Can not notify ERRROR to CRM for ctx %u",
			ctx->ctx_id);
		rc = -EFAULT;
	}

	CAM_DBG(CAM_ISP, "Exit");

	return rc;
}

static int __cam_isp_ctx_fs2_sof_in_sof_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_isp_hw_sof_event_data      *sof_event_data = evt_data;
	struct cam_ctx_request *req;
	struct cam_context *ctx = ctx_isp->base;
	struct cam_req_mgr_trigger_notify  notify;
	uint64_t  request_id  = 0;


	req = list_last_entry(&ctx->pending_req_list,
		struct cam_ctx_request, list);

	if (!evt_data) {
		CAM_ERR(CAM_ISP, "in valid sof event data");
		return -EINVAL;
	}

	ctx_isp->frame_id++;
	ctx_isp->sof_timestamp_val = sof_event_data->timestamp;
	ctx_isp->boot_timestamp = sof_event_data->boot_time;
	__cam_isp_ctx_update_state_monitor_array(ctx_isp,
		CAM_ISP_STATE_CHANGE_TRIGGER_SOF, req->request_id);
	CAM_DBG(CAM_ISP, "frame id: %lld time stamp:0x%llx",
		ctx_isp->frame_id, ctx_isp->sof_timestamp_val);

	if (!(list_empty(&ctx->wait_req_list)))
		goto end;

	if (ctx->ctx_crm_intf && ctx->ctx_crm_intf->notify_trigger &&
		ctx_isp->active_req_cnt <= 2) {
		if (ctx_isp->subscribe_event & CAM_TRIGGER_POINT_SOF) {
			notify.link_hdl = ctx->link_hdl;
			notify.dev_hdl = ctx->dev_hdl;
			notify.frame_id = ctx_isp->frame_id;
			notify.trigger = CAM_TRIGGER_POINT_SOF;

			ctx->ctx_crm_intf->notify_trigger(&notify);
			CAM_DBG(CAM_ISP, "Notify CRM  SOF frame %lld",
				ctx_isp->frame_id);
		}

		list_for_each_entry(req, &ctx->active_req_list, list) {
			if (req->request_id > ctx_isp->reported_req_id) {
				request_id = req->request_id;
				ctx_isp->reported_req_id = request_id;
				break;
			}
		}

		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);
	} else {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Can not notify SOF to CRM");
		rc = -EFAULT;
	}

end:
	return rc;
}

static int __cam_isp_ctx_fs2_buf_done(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int rc = 0;
	struct cam_isp_hw_done_event_data *done =
		(struct cam_isp_hw_done_event_data *) evt_data;
	struct cam_context *ctx = ctx_isp->base;
	int prev_active_req_cnt = 0;
	int curr_req_id = 0;
	struct cam_ctx_request  *req;

	prev_active_req_cnt = ctx_isp->active_req_cnt;
	req = list_first_entry(&ctx->active_req_list,
		struct cam_ctx_request, list);
	if (req)
		curr_req_id = req->request_id;

	rc = __cam_isp_ctx_handle_buf_done_in_activated_state(ctx_isp, done, 0);

	if (prev_active_req_cnt == ctx_isp->active_req_cnt + 1) {
		if (list_empty(&ctx->wait_req_list) &&
			list_empty(&ctx->active_req_list)) {
			CAM_DBG(CAM_ISP, "No request, move to SOF");
			ctx_isp->substate_activated =
				CAM_ISP_CTX_ACTIVATED_SOF;
			if (ctx_isp->reported_req_id < curr_req_id) {
				ctx_isp->reported_req_id = curr_req_id;
				__cam_isp_ctx_send_sof_timestamp(ctx_isp,
					curr_req_id,
					CAM_REQ_MGR_SOF_EVENT_SUCCESS);
			}
		}
	}

	return rc;
}

static int __cam_isp_ctx_fs2_buf_done_in_epoch(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int rc = 0;

	rc =  __cam_isp_ctx_fs2_buf_done(ctx_isp, evt_data);
	return rc;
}

static int __cam_isp_ctx_fs2_buf_done_in_applied(
	struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int rc = 0;

	rc =  __cam_isp_ctx_fs2_buf_done(ctx_isp, evt_data);
	return rc;
}

static int __cam_isp_ctx_fs2_reg_upd_in_sof(struct cam_isp_context *ctx_isp,
	void *evt_data)
{
	int rc = 0;
	struct cam_ctx_request *req = NULL;
	struct cam_isp_ctx_req *req_isp;
	struct cam_context *ctx = ctx_isp->base;

	if (ctx->state != CAM_CTX_ACTIVATED && ctx_isp->frame_id > 1) {
		CAM_DBG(CAM_ISP, "invalid RUP");
		goto end;
	}

	/*
	 * This is for the first update. The initial setting will
	 * cause the reg_upd in the first frame.
	 */
	if (!list_empty(&ctx->wait_req_list)) {
		req = list_first_entry(&ctx->wait_req_list,
			struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		if (req_isp->num_fence_map_out == req_isp->num_acked)
			list_add_tail(&req->list, &ctx->free_req_list);
		else
			CAM_ERR(CAM_ISP,
				"receive rup in unexpected state");
	}
	if (req != NULL) {
		__cam_isp_ctx_update_state_monitor_array(ctx_isp,
			CAM_ISP_STATE_CHANGE_TRIGGER_REG_UPDATE,
			req->request_id);
	}
end:
	return rc;
}

static int __cam_isp_ctx_fs2_reg_upd_in_applied_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_ctx_request  *req = NULL;
	struct cam_context      *ctx = ctx_isp->base;
	struct cam_isp_ctx_req  *req_isp;
	struct cam_req_mgr_trigger_notify  notify;
	uint64_t  request_id  = 0;

	if (list_empty(&ctx->wait_req_list)) {
		CAM_ERR(CAM_ISP, "Reg upd ack with no waiting request");
		goto end;
	}
	req = list_first_entry(&ctx->wait_req_list,
			struct cam_ctx_request, list);
	list_del_init(&req->list);

	req_isp = (struct cam_isp_ctx_req *) req->req_priv;
	if (req_isp->num_fence_map_out != 0) {
		list_add_tail(&req->list, &ctx->active_req_list);
		ctx_isp->active_req_cnt++;
		CAM_DBG(CAM_REQ, "move request %lld to active list(cnt = %d)",
			 req->request_id, ctx_isp->active_req_cnt);
	} else {
		/* no io config, so the request is completed. */
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	/*
	 * This function only called directly from applied and bubble applied
	 * state so change substate here.
	 */
	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_EPOCH;
	if (req_isp->num_fence_map_out != 1)
		goto end;

	if (ctx->ctx_crm_intf && ctx->ctx_crm_intf->notify_trigger &&
		ctx_isp->active_req_cnt <= 2) {
		list_for_each_entry(req, &ctx->active_req_list, list) {
			if (req->request_id > ctx_isp->reported_req_id) {
				request_id = req->request_id;
				ctx_isp->reported_req_id = request_id;
				break;
			}
		}

		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);

		if (ctx_isp->subscribe_event & CAM_TRIGGER_POINT_SOF) {
			notify.link_hdl = ctx->link_hdl;
			notify.dev_hdl = ctx->dev_hdl;
			notify.frame_id = ctx_isp->frame_id;
			notify.trigger = CAM_TRIGGER_POINT_SOF;

			ctx->ctx_crm_intf->notify_trigger(&notify);
			CAM_DBG(CAM_ISP, "Notify CRM  SOF frame %lld",
				ctx_isp->frame_id);
		}
	} else {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Can not notify SOF to CRM");
		rc = -EFAULT;
	}

	CAM_DBG(CAM_ISP, "next substate %d", ctx_isp->substate_activated);
end:
	if (req != NULL && !rc) {
		__cam_isp_ctx_update_state_monitor_array(ctx_isp,
			CAM_ISP_STATE_CHANGE_TRIGGER_EPOCH,
			req->request_id);
	}
	return rc;
}

static struct cam_isp_ctx_irq_ops
	cam_isp_ctx_activated_state_machine_irq[CAM_ISP_CTX_ACTIVATED_MAX] = {
	/* SOF */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_activated_state,
			__cam_isp_ctx_reg_upd_in_sof,
			__cam_isp_ctx_notify_sof_in_activated_state,
			__cam_isp_ctx_notify_eof_in_activated_state,
			NULL,
		},
	},
	/* APPLIED */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_activated_state,
			__cam_isp_ctx_reg_upd_in_activated_state,
			__cam_isp_ctx_epoch_in_applied,
			__cam_isp_ctx_notify_eof_in_activated_state,
			__cam_isp_ctx_buf_done_in_applied,
		},
	},
	/* EPOCH */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_epoch,
			__cam_isp_ctx_reg_upd_in_epoch_state,
			__cam_isp_ctx_notify_sof_in_activated_state,
			__cam_isp_ctx_notify_eof_in_activated_state,
			__cam_isp_ctx_buf_done_in_epoch,
		},
	},
	/* BUBBLE */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_activated_state,
			NULL,
			__cam_isp_ctx_notify_sof_in_activated_state,
			__cam_isp_ctx_notify_eof_in_activated_state,
			__cam_isp_ctx_buf_done_in_bubble,
		},
	},
	/* Bubble Applied */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_activated_state,
			__cam_isp_ctx_reg_upd_in_activated_state,
			__cam_isp_ctx_epoch_in_bubble_applied,
			NULL,
			__cam_isp_ctx_buf_done_in_bubble_applied,
		},
	},
	/* HW ERROR */
	{
		.irq_ops = {
			NULL,
			__cam_isp_ctx_sof_in_activated_state,
			__cam_isp_ctx_reg_upd_in_hw_error,
			NULL,
			NULL,
			NULL,
		},
	},
	/* HALT */
	{
	},
};

static struct cam_isp_ctx_irq_ops
	cam_isp_ctx_fs2_state_machine_irq[CAM_ISP_CTX_ACTIVATED_MAX] = {
	/* SOF */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_fs2_sof_in_sof_state,
			__cam_isp_ctx_fs2_reg_upd_in_sof,
			__cam_isp_ctx_fs2_sof_in_sof_state,
			__cam_isp_ctx_notify_eof_in_activated_state,
			NULL,
		},
	},
	/* APPLIED */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_activated_state,
			__cam_isp_ctx_fs2_reg_upd_in_applied_state,
			__cam_isp_ctx_epoch_in_applied,
			__cam_isp_ctx_notify_eof_in_activated_state,
			__cam_isp_ctx_fs2_buf_done_in_applied,
		},
	},
	/* EPOCH */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_epoch,
			__cam_isp_ctx_reg_upd_in_epoch_state,
			__cam_isp_ctx_notify_sof_in_activated_state,
			__cam_isp_ctx_notify_eof_in_activated_state,
			__cam_isp_ctx_fs2_buf_done_in_epoch,
		},
	},
	/* BUBBLE */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_activated_state,
			NULL,
			__cam_isp_ctx_notify_sof_in_activated_state,
			__cam_isp_ctx_notify_eof_in_activated_state,
			__cam_isp_ctx_buf_done_in_bubble,
		},
	},
	/* Bubble Applied */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_sof_in_activated_state,
			__cam_isp_ctx_reg_upd_in_activated_state,
			__cam_isp_ctx_epoch_in_bubble_applied,
			NULL,
			__cam_isp_ctx_buf_done_in_bubble_applied,
		},
	},
	/* HW ERROR */
	{
		.irq_ops = {
			NULL,
			__cam_isp_ctx_sof_in_activated_state,
			__cam_isp_ctx_reg_upd_in_hw_error,
			NULL,
			NULL,
			NULL,
		},
	},
	/* HALT */
	{
	},
};

static int __cam_isp_ctx_apply_req_in_activated_state(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply,
	uint32_t next_state)
{
	int rc = 0;
	struct cam_ctx_request          *req;
	struct cam_ctx_request          *active_req = NULL;
	struct cam_isp_ctx_req          *req_isp;
	struct cam_isp_ctx_req          *active_req_isp;
	struct cam_isp_context          *ctx_isp = NULL;
	struct cam_hw_config_args        cfg;

	if (list_empty(&ctx->pending_req_list)) {
		CAM_ERR(CAM_ISP, "No available request for Apply id %lld",
			apply->request_id);
		rc = -EFAULT;
		goto end;
	}

	/*
	 * When the pipeline has issue, the requests can be queued up in the
	 * pipeline. In this case, we should reject the additional request.
	 * The maximum number of request allowed to be outstanding is 2.
	 *
	 */
	ctx_isp = (struct cam_isp_context *) ctx->ctx_priv;

	if (atomic_read(&ctx_isp->process_bubble)) {
		CAM_DBG(CAM_ISP,
			"Processing bubble cannot apply Request Id %llu",
			apply->request_id);
		rc = -EAGAIN;
		goto end;
	}

	spin_lock_bh(&ctx->lock);
	req = list_first_entry(&ctx->pending_req_list, struct cam_ctx_request,
		list);
	spin_unlock_bh(&ctx->lock);

	/*
	 * Check whether the request id is matching the tip, if not, this means
	 * we are in the middle of the error handling. Need to reject this apply
	 */
	if (req->request_id != apply->request_id) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Invalid Request Id asking %llu existing %llu",
			apply->request_id, req->request_id);
		rc = -EFAULT;
		goto end;
	}

	CAM_DBG(CAM_REQ, "Apply request %lld in substate %d ctx %u",
		req->request_id, ctx_isp->substate_activated, ctx->ctx_id);
	req_isp = (struct cam_isp_ctx_req *) req->req_priv;

	if (ctx_isp->active_req_cnt >=  2) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Reject apply request (id %lld) due to congestion(cnt = %d) ctx %u",
			req->request_id,
			ctx_isp->active_req_cnt,
			ctx->ctx_id);

		spin_lock_bh(&ctx->lock);
		if (!list_empty(&ctx->active_req_list))
			active_req = list_first_entry(&ctx->active_req_list,
				struct cam_ctx_request, list);
		else
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"WARNING: should not happen (cnt = %d) but active_list empty",
				ctx_isp->active_req_cnt);
		spin_unlock_bh(&ctx->lock);

		if (active_req) {
			active_req_isp =
				(struct cam_isp_ctx_req *) active_req->req_priv;
			__cam_isp_ctx_handle_buf_done_fail_log(active_req_isp);
		}

		rc = -EFAULT;
		goto end;
	}
	req_isp->bubble_report = apply->report_if_bubble;

	cfg.ctxt_to_hw_map = ctx_isp->hw_ctx;
	cfg.request_id = req->request_id;
	cfg.hw_update_entries = req_isp->cfg;
	cfg.num_hw_update_entries = req_isp->num_cfg;
	cfg.priv  = &req_isp->hw_update_data;
	cfg.init_packet = 0;

	rc = ctx->hw_mgr_intf->hw_config(ctx->hw_mgr_intf->hw_mgr_priv, &cfg);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Can not apply the configuration");
	} else {
		spin_lock_bh(&ctx->lock);
		ctx_isp->substate_activated = next_state;
		ctx_isp->last_applied_req_id = apply->request_id;
		list_del_init(&req->list);
		list_add_tail(&req->list, &ctx->wait_req_list);
		CAM_DBG(CAM_ISP, "new substate state %d, applied req %lld",
			next_state, ctx_isp->last_applied_req_id);
		spin_unlock_bh(&ctx->lock);
	}
end:
	if (ctx_isp != NULL) {
		__cam_isp_ctx_update_state_monitor_array(ctx_isp,
			CAM_ISP_STATE_CHANGE_TRIGGER_SOF,
			ctx->req_list->request_id);
	}
	return rc;
}

static int __cam_isp_ctx_apply_req_in_sof(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	CAM_DBG(CAM_ISP, "current substate %d",
		ctx_isp->substate_activated);
	rc = __cam_isp_ctx_apply_req_in_activated_state(ctx, apply,
		CAM_ISP_CTX_ACTIVATED_APPLIED);
	CAM_DBG(CAM_ISP, "new substate %d", ctx_isp->substate_activated);

	return rc;
}

static int __cam_isp_ctx_apply_req_in_epoch(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	CAM_DBG(CAM_ISP, "current substate %d",
		ctx_isp->substate_activated);
	rc = __cam_isp_ctx_apply_req_in_activated_state(ctx, apply,
		CAM_ISP_CTX_ACTIVATED_APPLIED);
	CAM_DBG(CAM_ISP, "new substate %d", ctx_isp->substate_activated);

	return rc;
}

static int __cam_isp_ctx_apply_req_in_bubble(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	CAM_DBG(CAM_ISP, "current substate %d",
		ctx_isp->substate_activated);
	rc = __cam_isp_ctx_apply_req_in_activated_state(ctx, apply,
		CAM_ISP_CTX_ACTIVATED_BUBBLE_APPLIED);
	CAM_DBG(CAM_ISP, "new substate %d", ctx_isp->substate_activated);

	return rc;
}

static int __cam_isp_ctx_flush_req(struct cam_context *ctx,
	struct list_head *req_list, struct cam_req_mgr_flush_request *flush_req)
{
	int i, rc;
	uint32_t cancel_req_id_found = 0;
	struct cam_ctx_request           *req;
	struct cam_ctx_request           *req_temp;
	struct cam_isp_ctx_req           *req_isp;
	struct list_head                  flush_list;

	INIT_LIST_HEAD(&flush_list);
	if (list_empty(req_list)) {
		CAM_DBG(CAM_ISP, "request list is empty");
		if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
			CAM_ERR(CAM_ISP, "no request to cancel");
			return -EINVAL;
		} else
			return 0;
	}

	CAM_DBG(CAM_REQ, "Flush [%u] in progress for req_id %llu",
		flush_req->type, flush_req->req_id);
	list_for_each_entry_safe(req, req_temp, req_list, list) {
		if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
			if (req->request_id != flush_req->req_id) {
				continue;
			} else {
				list_del_init(&req->list);
				list_add_tail(&req->list, &flush_list);
				cancel_req_id_found = 1;
				break;
			}
		}
		list_del_init(&req->list);
		list_add_tail(&req->list, &flush_list);
	}

	list_for_each_entry_safe(req, req_temp, &flush_list, list) {
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		for (i = 0; i < req_isp->num_fence_map_out; i++) {
			if (req_isp->fence_map_out[i].sync_id != -1) {
				CAM_DBG(CAM_ISP, "Flush req 0x%llx, fence %d",
					 req->request_id,
					req_isp->fence_map_out[i].sync_id);
				rc = cam_sync_signal(
					req_isp->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
				if (rc)
					CAM_ERR_RATE_LIMIT(CAM_ISP,
						"signal fence failed\n");
				req_isp->fence_map_out[i].sync_id = -1;
			}
		}
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ &&
		!cancel_req_id_found)
		CAM_DBG(CAM_ISP,
			"Flush request id:%lld is not found in the list",
			flush_req->req_id);

	return 0;
}

static int __cam_isp_ctx_flush_req_in_top_state(
	struct cam_context *ctx,
	struct cam_req_mgr_flush_request *flush_req)
{
	int rc = 0;
	struct cam_isp_context *ctx_isp;

	ctx_isp = (struct cam_isp_context *) ctx->ctx_priv;
	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		CAM_INFO(CAM_ISP, "Last request id to flush is %lld",
			flush_req->req_id);
		ctx->last_flush_req = flush_req->req_id;
	}

	CAM_DBG(CAM_ISP, "try to flush pending list");
	spin_lock_bh(&ctx->lock);
	rc = __cam_isp_ctx_flush_req(ctx, &ctx->pending_req_list, flush_req);
	spin_unlock_bh(&ctx->lock);

	atomic_set(&ctx_isp->process_bubble, 0);
	return rc;
}

static int __cam_isp_ctx_flush_req_in_ready(
	struct cam_context *ctx,
	struct cam_req_mgr_flush_request *flush_req)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "try to flush pending list");
	spin_lock_bh(&ctx->lock);
	rc = __cam_isp_ctx_flush_req(ctx, &ctx->pending_req_list, flush_req);

	/* if nothing is in pending req list, change state to acquire */
	if (list_empty(&ctx->pending_req_list))
		ctx->state = CAM_CTX_ACQUIRED;
	spin_unlock_bh(&ctx->lock);

	trace_cam_context_state("ISP", ctx);

	CAM_DBG(CAM_ISP, "Flush request in ready state. next state %d",
		 ctx->state);
	return rc;
}

static struct cam_ctx_ops
	cam_isp_ctx_activated_state_machine[CAM_ISP_CTX_ACTIVATED_MAX] = {
	/* SOF */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_isp_ctx_apply_req_in_sof,
		},
		.irq_ops = NULL,
	},
	/* APPLIED */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* EPOCH */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_isp_ctx_apply_req_in_epoch,
		},
		.irq_ops = NULL,
	},
	/* BUBBLE */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_isp_ctx_apply_req_in_bubble,
		},
		.irq_ops = NULL,
	},
	/* Bubble Applied */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* HW ERROR */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* HALT */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
};

static struct cam_ctx_ops
	cam_isp_ctx_fs2_state_machine[CAM_ISP_CTX_ACTIVATED_MAX] = {
	/* SOF */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_isp_ctx_apply_req_in_sof,
		},
		.irq_ops = NULL,
	},
	/* APPLIED */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* EPOCH */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_isp_ctx_apply_req_in_epoch,
		},
		.irq_ops = NULL,
	},
	/* BUBBLE */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_isp_ctx_apply_req_in_bubble,
		},
		.irq_ops = NULL,
	},
	/* Bubble Applied */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* HW ERROR */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* HALT */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
};

static int __cam_isp_ctx_rdi_only_sof_in_top_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	int rc = 0;
	struct cam_context                    *ctx = ctx_isp->base;
	struct cam_req_mgr_trigger_notify      notify;
	struct cam_isp_hw_sof_event_data      *sof_event_data = evt_data;
	uint64_t                               request_id  = 0;

	if (!evt_data) {
		CAM_ERR(CAM_ISP, "in valid sof event data");
		return -EINVAL;
	}

	ctx_isp->frame_id++;
	ctx_isp->sof_timestamp_val = sof_event_data->timestamp;
	ctx_isp->boot_timestamp = sof_event_data->boot_time;

	CAM_DBG(CAM_ISP, "frame id: %lld time stamp:0x%llx",
		ctx_isp->frame_id, ctx_isp->sof_timestamp_val);

	/*
	 * notify reqmgr with sof signal. Note, due to scheduling delay
	 * we can run into situation that two active requests has already
	 * be in the active queue while we try to do the notification.
	 * In this case, we need to skip the current notification. This
	 * helps the state machine to catch up the delay.
	 */
	if (ctx->ctx_crm_intf && ctx->ctx_crm_intf->notify_trigger &&
		ctx_isp->active_req_cnt <= 2) {
		notify.link_hdl = ctx->link_hdl;
		notify.dev_hdl = ctx->dev_hdl;
		notify.frame_id = ctx_isp->frame_id;
		notify.trigger = CAM_TRIGGER_POINT_SOF;

		ctx->ctx_crm_intf->notify_trigger(&notify);
		CAM_DBG(CAM_ISP, "Notify CRM  SOF frame %lld",
			ctx_isp->frame_id);

		/*
		 * It is idle frame with out any applied request id, send
		 * request id as zero
		 */
		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);
	} else {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Can not notify SOF to CRM");
	}

	if (list_empty(&ctx->active_req_list))
		ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_SOF;
	else
		CAM_DBG(CAM_ISP, "Still need to wait for the buf done");

	CAM_DBG(CAM_ISP, "next substate %d",
		ctx_isp->substate_activated);
	return rc;
}

static int __cam_isp_ctx_rdi_only_sof_in_applied_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	struct cam_isp_hw_sof_event_data      *sof_event_data = evt_data;

	if (!evt_data) {
		CAM_ERR(CAM_ISP, "in valid sof event data");
		return -EINVAL;
	}

	ctx_isp->frame_id++;
	ctx_isp->sof_timestamp_val = sof_event_data->timestamp;
	ctx_isp->boot_timestamp = sof_event_data->boot_time;
	CAM_DBG(CAM_ISP, "frame id: %lld time stamp:0x%llx",
		ctx_isp->frame_id, ctx_isp->sof_timestamp_val);

	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_BUBBLE_APPLIED;
	CAM_DBG(CAM_ISP, "next substate %d", ctx_isp->substate_activated);

	return 0;
}

static int __cam_isp_ctx_rdi_only_sof_in_bubble_applied(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	struct cam_ctx_request    *req;
	struct cam_isp_ctx_req    *req_isp;
	struct cam_context        *ctx = ctx_isp->base;
	struct cam_isp_hw_sof_event_data      *sof_event_data = evt_data;
	uint64_t  request_id = 0;

	/*
	 * Sof in bubble applied state means, reg update not received.
	 * before increment frame id and override time stamp value, send
	 * the previous sof time stamp that got captured in the
	 * sof in applied state.
	 */
	CAM_DBG(CAM_ISP, "frame id: %lld time stamp:0x%llx",
		ctx_isp->frame_id, ctx_isp->sof_timestamp_val);
	__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
		CAM_REQ_MGR_SOF_EVENT_SUCCESS);

	ctx_isp->frame_id++;
	ctx_isp->sof_timestamp_val = sof_event_data->timestamp;
	ctx_isp->boot_timestamp = sof_event_data->boot_time;
	CAM_DBG(CAM_ISP, "frame id: %lld time stamp:0x%llx",
		ctx_isp->frame_id, ctx_isp->sof_timestamp_val);

	if (list_empty(&ctx->wait_req_list)) {
		/*
		 * If no pending req in epoch, this is an error case.
		 * The recovery is to go back to sof state
		 */
		CAM_ERR(CAM_ISP, "No wait request");
		ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_SOF;

		/* Send SOF event as empty frame*/
		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);

		goto end;
	}

	req = list_first_entry(&ctx->wait_req_list, struct cam_ctx_request,
		list);
	req_isp = (struct cam_isp_ctx_req *)req->req_priv;
	req_isp->bubble_detected = true;

	CAM_DBG(CAM_ISP, "Report Bubble flag %d", req_isp->bubble_report);
	if (req_isp->bubble_report && ctx->ctx_crm_intf &&
		ctx->ctx_crm_intf->notify_err) {
		struct cam_req_mgr_error_notify notify;

		notify.link_hdl = ctx->link_hdl;
		notify.dev_hdl = ctx->dev_hdl;
		notify.req_id = req->request_id;
		notify.error = CRM_KMD_ERR_BUBBLE;
		ctx->ctx_crm_intf->notify_err(&notify);
		CAM_DBG(CAM_ISP, "Notify CRM about Bubble frame %lld",
			ctx_isp->frame_id);
	} else {
		req_isp->bubble_report = 0;
	}

	/*
	 * Always move the request to active list. Let buf done
	 * function handles the rest.
	 */
	ctx_isp->active_req_cnt++;
	list_del_init(&req->list);
	list_add_tail(&req->list, &ctx->active_req_list);
	CAM_DBG(CAM_ISP, "move request %lld to active list(cnt = %d)",
			req->request_id, ctx_isp->active_req_cnt);

	if (!req_isp->bubble_report) {
		if (req->request_id > ctx_isp->reported_req_id) {
			request_id = req->request_id;
			ctx_isp->reported_req_id = request_id;
			__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_ERROR);
		} else
			__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
				CAM_REQ_MGR_SOF_EVENT_SUCCESS);
	} else
		__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
			CAM_REQ_MGR_SOF_EVENT_SUCCESS);

	/* change the state to bubble, as reg update has not come */
	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_BUBBLE;
	CAM_DBG(CAM_ISP, "next substate %d", ctx_isp->substate_activated);
end:
	return 0;
}

static int __cam_isp_ctx_rdi_only_sof_in_bubble_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	uint32_t i;
	struct cam_ctx_request                *req;
	struct cam_context                    *ctx = ctx_isp->base;
	struct cam_req_mgr_trigger_notify      notify;
	struct cam_isp_hw_sof_event_data      *sof_event_data = evt_data;
	struct cam_isp_ctx_req                *req_isp;
	uint64_t                               request_id  = 0;

	if (!evt_data) {
		CAM_ERR(CAM_ISP, "in valid sof event data");
		return -EINVAL;
	}

	ctx_isp->frame_id++;
	ctx_isp->sof_timestamp_val = sof_event_data->timestamp;
	ctx_isp->boot_timestamp = sof_event_data->boot_time;
	CAM_DBG(CAM_ISP, "frame id: %lld time stamp:0x%llx",
		ctx_isp->frame_id, ctx_isp->sof_timestamp_val);
	/*
	 * Signal all active requests with error and move the  all the active
	 * requests to free list
	 */
	while (!list_empty(&ctx->active_req_list)) {
		req = list_first_entry(&ctx->active_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		CAM_DBG(CAM_ISP, "signal fence in active list. fence num %d",
			req_isp->num_fence_map_out);
		for (i = 0; i < req_isp->num_fence_map_out; i++)
			if (req_isp->fence_map_out[i].sync_id != -1) {
				cam_sync_signal(
					req_isp->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
			}
		list_add_tail(&req->list, &ctx->free_req_list);
		ctx_isp->active_req_cnt--;
	}

	/* notify reqmgr with sof signal */
	if (ctx->ctx_crm_intf && ctx->ctx_crm_intf->notify_trigger) {
		notify.link_hdl = ctx->link_hdl;
		notify.dev_hdl = ctx->dev_hdl;
		notify.frame_id = ctx_isp->frame_id;
		notify.trigger = CAM_TRIGGER_POINT_SOF;

		ctx->ctx_crm_intf->notify_trigger(&notify);
		CAM_DBG(CAM_ISP, "Notify CRM  SOF frame %lld",
			ctx_isp->frame_id);

	} else {
		CAM_ERR(CAM_ISP, "Can not notify SOF to CRM");
	}

	/*
	 * It is idle frame with out any applied request id, send
	 * request id as zero
	 */
	__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
		CAM_REQ_MGR_SOF_EVENT_SUCCESS);

	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_SOF;

	CAM_DBG(CAM_ISP, "next substate %d",
		ctx_isp->substate_activated);

	return 0;
}

static int __cam_isp_ctx_rdi_only_reg_upd_in_bubble_applied_state(
	struct cam_isp_context *ctx_isp, void *evt_data)
{
	struct cam_ctx_request  *req;
	struct cam_context      *ctx = ctx_isp->base;
	struct cam_isp_ctx_req  *req_isp;
	struct cam_req_mgr_trigger_notify  notify;
	uint64_t  request_id  = 0;

	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_EPOCH;
	/* notify reqmgr with sof signal*/
	if (ctx->ctx_crm_intf && ctx->ctx_crm_intf->notify_trigger) {
		if (list_empty(&ctx->wait_req_list)) {
			CAM_ERR(CAM_ISP, "Reg upd ack with no waiting request");
			goto error;
		}
		req = list_first_entry(&ctx->wait_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);

		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		request_id =
			(req_isp->hw_update_data.packet_opcode_type ==
				CAM_ISP_PACKET_INIT_DEV) ?
			0 : req->request_id;

		if (req_isp->num_fence_map_out != 0) {
			list_add_tail(&req->list, &ctx->active_req_list);
			ctx_isp->active_req_cnt++;
			CAM_DBG(CAM_ISP,
				"move request %lld to active list(cnt = %d)",
				req->request_id, ctx_isp->active_req_cnt);
			/* if packet has buffers, set correct request id */
			request_id = req->request_id;
		} else {
			/* no io config, so the request is completed. */
			list_add_tail(&req->list, &ctx->free_req_list);
			CAM_DBG(CAM_ISP,
				"move active req %lld to free list(cnt=%d)",
				req->request_id, ctx_isp->active_req_cnt);
		}

		notify.link_hdl = ctx->link_hdl;
		notify.dev_hdl = ctx->dev_hdl;
		notify.frame_id = ctx_isp->frame_id;
		notify.trigger = CAM_TRIGGER_POINT_SOF;

		ctx->ctx_crm_intf->notify_trigger(&notify);
		CAM_DBG(CAM_ISP, "Notify CRM  SOF frame %lld",
			ctx_isp->frame_id);
	} else {
		CAM_ERR(CAM_ISP, "Can not notify SOF to CRM");
	}
	if (request_id)
		ctx_isp->reported_req_id = request_id;

	__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
		CAM_REQ_MGR_SOF_EVENT_SUCCESS);
	CAM_DBG(CAM_ISP, "next substate %d", ctx_isp->substate_activated);

	return 0;
error:
	/* Send SOF event as idle frame*/
	__cam_isp_ctx_send_sof_timestamp(ctx_isp, request_id,
		CAM_REQ_MGR_SOF_EVENT_SUCCESS);

	/*
	 * There is no request in the pending list, move the sub state machine
	 * to SOF sub state
	 */
	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_SOF;

	return 0;
}

static struct cam_isp_ctx_irq_ops
	cam_isp_ctx_rdi_only_activated_state_machine_irq
			[CAM_ISP_CTX_ACTIVATED_MAX] = {
	/* SOF */
	{
		.irq_ops = {
			NULL,
			__cam_isp_ctx_rdi_only_sof_in_top_state,
			__cam_isp_ctx_reg_upd_in_sof,
			NULL,
			NULL,
			NULL,
		},
	},
	/* APPLIED */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_rdi_only_sof_in_applied_state,
			NULL,
			NULL,
			NULL,
			__cam_isp_ctx_buf_done_in_applied,
		},
	},
	/* EPOCH */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_rdi_only_sof_in_top_state,
			NULL,
			NULL,
			NULL,
			__cam_isp_ctx_buf_done_in_epoch,
		},
	},
	/* BUBBLE*/
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_rdi_only_sof_in_bubble_state,
			NULL,
			NULL,
			NULL,
			__cam_isp_ctx_buf_done_in_bubble,
		},
	},
	/* BUBBLE APPLIED ie PRE_BUBBLE */
	{
		.irq_ops = {
			__cam_isp_ctx_handle_error,
			__cam_isp_ctx_rdi_only_sof_in_bubble_applied,
			__cam_isp_ctx_rdi_only_reg_upd_in_bubble_applied_state,
			NULL,
			NULL,
			__cam_isp_ctx_buf_done_in_bubble_applied,
		},
	},
	/* HW ERROR */
	{
	},
	/* HALT */
	{
	},
};

static int __cam_isp_ctx_rdi_only_apply_req_top_state(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	CAM_DBG(CAM_ISP, "current substate %d",
		ctx_isp->substate_activated);
	rc = __cam_isp_ctx_apply_req_in_activated_state(ctx, apply,
		CAM_ISP_CTX_ACTIVATED_APPLIED);
	CAM_DBG(CAM_ISP, "new substate %d", ctx_isp->substate_activated);

	return rc;
}

static struct cam_ctx_ops
	cam_isp_ctx_rdi_only_activated_state_machine
		[CAM_ISP_CTX_ACTIVATED_MAX] = {
	/* SOF */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_isp_ctx_rdi_only_apply_req_top_state,
		},
		.irq_ops = NULL,
	},
	/* APPLIED */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* EPOCH */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_isp_ctx_rdi_only_apply_req_top_state,
		},
		.irq_ops = NULL,
	},
	/* PRE BUBBLE */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* BUBBLE */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* HW ERROR */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* HALT */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
};

static int __cam_isp_ctx_release_hw_in_top_state(struct cam_context *ctx,
	void *cmd)
{
	int rc = 0;
	struct cam_hw_release_args       rel_arg;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;
	struct cam_req_mgr_flush_request flush_req;

	if (ctx_isp->hw_ctx) {
		rel_arg.ctxt_to_hw_map = ctx_isp->hw_ctx;
		ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv,
			&rel_arg);
		ctx_isp->hw_ctx = NULL;
	} else {
		CAM_ERR(CAM_ISP, "No hw resources acquired for this ctx");
	}

	ctx->last_flush_req = 0;
	ctx_isp->frame_id = 0;
	ctx_isp->active_req_cnt = 0;
	ctx_isp->reported_req_id = 0;
	ctx_isp->hw_acquired = false;
	ctx_isp->init_received = false;

	/*
	 * Ideally, we should never have any active request here.
	 * But we still add some sanity check code here to help the debug
	 */
	if (!list_empty(&ctx->active_req_list))
		CAM_WARN(CAM_ISP, "Active list is not empty");

	/* Flush all the pending request list  */
	flush_req.type = CAM_REQ_MGR_FLUSH_TYPE_ALL;
	flush_req.link_hdl = ctx->link_hdl;
	flush_req.dev_hdl = ctx->dev_hdl;

	CAM_DBG(CAM_ISP, "try to flush pending list");
	spin_lock_bh(&ctx->lock);
	rc = __cam_isp_ctx_flush_req(ctx, &ctx->pending_req_list, &flush_req);
	spin_unlock_bh(&ctx->lock);
	ctx->state = CAM_CTX_ACQUIRED;

	trace_cam_context_state("ISP", ctx);
	CAM_DBG(CAM_ISP, "Release device success[%u] next state %d",
		ctx->ctx_id, ctx->state);
	return rc;
}

/* top level state machine */
static int __cam_isp_ctx_release_dev_in_top_state(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_hw_release_args       rel_arg;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;
	struct cam_req_mgr_flush_request flush_req;

	if (cmd && ctx_isp->hw_ctx) {
		CAM_ERR(CAM_ISP, "releasing hw");
		__cam_isp_ctx_release_hw_in_top_state(ctx, NULL);
	}

	if (ctx_isp->hw_ctx) {
		rel_arg.ctxt_to_hw_map = ctx_isp->hw_ctx;
		ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv,
			&rel_arg);
		ctx_isp->hw_ctx = NULL;
	}

	ctx->session_hdl = -1;
	ctx->dev_hdl = -1;
	ctx->link_hdl = -1;
	ctx->ctx_crm_intf = NULL;
	ctx->last_flush_req = 0;
	ctx_isp->frame_id = 0;
	ctx_isp->active_req_cnt = 0;
	ctx_isp->reported_req_id = 0;
	ctx_isp->hw_acquired = false;
	ctx_isp->init_received = false;

	/*
	 * Ideally, we should never have any active request here.
	 * But we still add some sanity check code here to help the debug
	 */
	if (!list_empty(&ctx->active_req_list))
		CAM_ERR(CAM_ISP, "Active list is not empty");

	/* Flush all the pending request list  */
	flush_req.type = CAM_REQ_MGR_FLUSH_TYPE_ALL;
	flush_req.link_hdl = ctx->link_hdl;
	flush_req.dev_hdl = ctx->dev_hdl;

	CAM_DBG(CAM_ISP, "try to flush pending list");
	spin_lock_bh(&ctx->lock);
	rc = __cam_isp_ctx_flush_req(ctx, &ctx->pending_req_list, &flush_req);
	spin_unlock_bh(&ctx->lock);
	ctx->state = CAM_CTX_AVAILABLE;

	trace_cam_context_state("ISP", ctx);
	CAM_DBG(CAM_ISP, "Release device success[%u] next state %d",
		ctx->ctx_id, ctx->state);
	return rc;
}

static int __cam_isp_ctx_config_dev_in_top_state(
	struct cam_context *ctx, struct cam_config_dev_cmd *cmd)
{
	int rc = 0, i;
	struct cam_ctx_request           *req = NULL;
	struct cam_isp_ctx_req           *req_isp;
	uintptr_t                         packet_addr;
	struct cam_packet                *packet;
	size_t                            len = 0;
	size_t                            remain_len = 0;
	struct cam_hw_prepare_update_args cfg;
	struct cam_req_mgr_add_request    add_req;
	struct cam_isp_context           *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	CAM_DBG(CAM_ISP, "get free request object......");

	/* get free request */
	spin_lock_bh(&ctx->lock);
	if (!list_empty(&ctx->free_req_list)) {
		req = list_first_entry(&ctx->free_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
	}
	spin_unlock_bh(&ctx->lock);

	if (!req) {
		CAM_ERR(CAM_ISP, "No more request obj free");
		return -ENOMEM;
	}

	req_isp = (struct cam_isp_ctx_req *) req->req_priv;

	/* for config dev, only memory handle is supported */
	/* map packet from the memhandle */
	rc = cam_mem_get_cpu_buf((int32_t) cmd->packet_handle,
		&packet_addr, &len);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Can not get packet address");
		rc = -EINVAL;
		goto free_req;
	}

	remain_len = len;
	if ((len < sizeof(struct cam_packet)) ||
		((size_t)cmd->offset >= len - sizeof(struct cam_packet))) {
		CAM_ERR(CAM_ISP, "invalid buff length: %zu or offset", len);
		rc = -EINVAL;
		goto free_cpu_buf;
	}

	remain_len -= (size_t)cmd->offset;
	packet = (struct cam_packet *)(packet_addr + (uint32_t)cmd->offset);
	CAM_DBG(CAM_ISP, "pack_handle %llx", cmd->packet_handle);
	CAM_DBG(CAM_ISP, "packet address is 0x%zx", packet_addr);
	CAM_DBG(CAM_ISP, "packet with length %zu, offset 0x%llx",
		len, cmd->offset);
	CAM_DBG(CAM_ISP, "Packet request id %lld",
		packet->header.request_id);
	CAM_DBG(CAM_ISP, "Packet size 0x%x", packet->header.size);
	CAM_DBG(CAM_ISP, "packet op %d", packet->header.op_code);

	if ((((packet->header.op_code + 1) & 0xF) == CAM_ISP_PACKET_UPDATE_DEV)
		&& (packet->header.request_id <= ctx->last_flush_req)) {
		CAM_INFO(CAM_ISP,
			"request %lld has been flushed, reject packet",
			packet->header.request_id);
		rc = -EINVAL;
		goto free_cpu_buf;
	}

	/* preprocess the configuration */
	memset(&cfg, 0, sizeof(cfg));
	cfg.packet = packet;
	cfg.remain_len = remain_len;
	cfg.ctxt_to_hw_map = ctx_isp->hw_ctx;
	cfg.max_hw_update_entries = CAM_ISP_CTX_CFG_MAX;
	cfg.hw_update_entries = req_isp->cfg;
	cfg.max_out_map_entries = CAM_ISP_CTX_RES_MAX;
	cfg.max_in_map_entries = CAM_ISP_CTX_RES_MAX;
	cfg.out_map_entries = req_isp->fence_map_out;
	cfg.in_map_entries = req_isp->fence_map_in;
	cfg.priv  = &req_isp->hw_update_data;
	cfg.pf_data = &(req->pf_data);

	CAM_DBG(CAM_ISP, "try to prepare config packet......");

	rc = ctx->hw_mgr_intf->hw_prepare_update(
		ctx->hw_mgr_intf->hw_mgr_priv, &cfg);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Prepare config packet failed in HW layer");
		rc = -EFAULT;
		goto free_cpu_buf;
	}
	req_isp->num_cfg = cfg.num_hw_update_entries;
	req_isp->num_fence_map_out = cfg.num_out_map_entries;
	req_isp->num_fence_map_in = cfg.num_in_map_entries;
	req_isp->num_acked = 0;
	req_isp->bubble_detected = false;

	for (i = 0; i < req_isp->num_fence_map_out; i++) {
		rc = cam_sync_get_obj_ref(req_isp->fence_map_out[i].sync_id);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can't get ref for fence %d",
				req_isp->fence_map_out[i].sync_id);
			goto put_ref;
		}
	}

	CAM_DBG(CAM_ISP, "num_entry: %d, num fence out: %d, num fence in: %d",
		req_isp->num_cfg, req_isp->num_fence_map_out,
		req_isp->num_fence_map_in);

	req->request_id = packet->header.request_id;
	req->status = 1;

	CAM_DBG(CAM_ISP, "Packet request id %lld packet opcode:%d",
		packet->header.request_id,
		req_isp->hw_update_data.packet_opcode_type);

	if (req_isp->hw_update_data.packet_opcode_type ==
		CAM_ISP_PACKET_INIT_DEV) {
		if (ctx->state < CAM_CTX_ACTIVATED) {
			rc = __cam_isp_ctx_enqueue_init_request(ctx, req);
			if (rc)
				CAM_ERR(CAM_ISP, "Enqueue INIT pkt failed");
			ctx_isp->init_received = true;
		} else {
			rc = -EINVAL;
			CAM_ERR(CAM_ISP, "Recevied INIT pkt in wrong state");
		}
	} else {
		if (ctx->state >= CAM_CTX_READY && ctx->ctx_crm_intf->add_req) {
			add_req.link_hdl = ctx->link_hdl;
			add_req.dev_hdl  = ctx->dev_hdl;
			add_req.req_id   = req->request_id;
			add_req.skip_before_applying = 0;
			rc = ctx->ctx_crm_intf->add_req(&add_req);
			if (rc) {
				CAM_ERR(CAM_ISP, "Add req failed: req id=%llu",
					req->request_id);
			} else {
				__cam_isp_ctx_enqueue_request_in_order(
					ctx, req);
			}
		} else {
			rc = -EINVAL;
			CAM_ERR(CAM_ISP, "Recevied Update in wrong state");
		}
	}
	if (rc)
		goto put_ref;

	if (cam_mem_put_cpu_buf((int32_t) cmd->packet_handle))
		CAM_WARN(CAM_ISP, "Can not put packet address : 0x%x",
			cmd->packet_handle);

	CAM_DBG(CAM_REQ,
		"Preprocessing Config req_id %lld successful on ctx %u",
		req->request_id, ctx->ctx_id);

	return rc;

put_ref:
	for (--i; i >= 0; i--) {
		if (cam_sync_put_obj_ref(req_isp->fence_map_out[i].sync_id))
			CAM_ERR(CAM_CTXT, "Failed to put ref of fence %d",
				req_isp->fence_map_out[i].sync_id);
	}
free_cpu_buf:
	if (cam_mem_put_cpu_buf((int32_t) cmd->packet_handle))
		CAM_WARN(CAM_ISP, "Can not put packet address: 0x%x",
			cmd->packet_handle);
free_req:
	spin_lock_bh(&ctx->lock);
	list_add_tail(&req->list, &ctx->free_req_list);
	spin_unlock_bh(&ctx->lock);

	return rc;
}

static int __cam_isp_ctx_acquire_dev_in_available(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_hw_acquire_args       param;
	struct cam_isp_resource         *isp_res = NULL;
	struct cam_create_dev_hdl        req_hdl_param;
	struct cam_hw_release_args       release;
	struct cam_isp_context          *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;
	struct cam_hw_cmd_args           hw_cmd_args;
	struct cam_isp_hw_cmd_args       isp_hw_cmd_args;

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_ISP, "HW interface is not ready");
		rc = -EFAULT;
		goto end;
	}

	CAM_DBG(CAM_ISP,
		"session_hdl 0x%x, num_resources %d, hdl type %d, res %lld",
		cmd->session_handle, cmd->num_resources,
		cmd->handle_type, cmd->resource_hdl);

	if (cmd->num_resources == CAM_API_COMPAT_CONSTANT) {
		ctx_isp->split_acquire = true;
		CAM_DBG(CAM_ISP, "Acquire dev handle");
		goto get_dev_handle;
	}

	if (cmd->num_resources > CAM_ISP_CTX_RES_MAX) {
		CAM_ERR(CAM_ISP, "Too much resources in the acquire");
		rc = -ENOMEM;
		goto end;
	}

	/* for now we only support user pointer */
	if (cmd->handle_type != 1)  {
		CAM_ERR(CAM_ISP, "Only user pointer is supported");
		rc = -EINVAL;
		goto end;
	}

	isp_res = kzalloc(
		sizeof(*isp_res)*cmd->num_resources, GFP_KERNEL);
	if (!isp_res) {
		rc = -ENOMEM;
		goto end;
	}

	CAM_DBG(CAM_ISP, "start copy %d resources from user",
		 cmd->num_resources);

	if (copy_from_user(isp_res, u64_to_user_ptr(cmd->resource_hdl),
		sizeof(*isp_res)*cmd->num_resources)) {
		rc = -EFAULT;
		goto free_res;
	}

	param.context_data = ctx;
	param.event_cb = ctx->irq_cb_intf;
	param.num_acq = cmd->num_resources;
	param.acquire_info = (uintptr_t) isp_res;

	/* call HW manager to reserve the resource */
	rc = ctx->hw_mgr_intf->hw_acquire(ctx->hw_mgr_intf->hw_mgr_priv,
		&param);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Acquire device failed");
		goto free_res;
	}

	/* Query the context has rdi only resource */
	hw_cmd_args.ctxt_to_hw_map = param.ctxt_to_hw_map;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;
	isp_hw_cmd_args.cmd_type = CAM_ISP_HW_MGR_CMD_CTX_TYPE;
	hw_cmd_args.u.internal_args = (void *)&isp_hw_cmd_args;
	rc = ctx->hw_mgr_intf->hw_cmd(ctx->hw_mgr_intf->hw_mgr_priv,
				&hw_cmd_args);
	if (rc) {
		CAM_ERR(CAM_ISP, "HW command failed");
		goto free_hw;
	}

	if (isp_hw_cmd_args.u.ctx_type == CAM_ISP_CTX_RDI) {
		/*
		 * this context has rdi only resource assign rdi only
		 * state machine
		 */
		CAM_DBG(CAM_ISP, "RDI only session Context");

		ctx_isp->substate_machine_irq =
			cam_isp_ctx_rdi_only_activated_state_machine_irq;
		ctx_isp->substate_machine =
			cam_isp_ctx_rdi_only_activated_state_machine;
		ctx_isp->rdi_only_context = true;
	} else if (isp_hw_cmd_args.u.ctx_type == CAM_ISP_CTX_FS2) {
		CAM_DBG(CAM_ISP, "FS2 Session has PIX ,RD and RDI");
		ctx_isp->substate_machine_irq =
			cam_isp_ctx_fs2_state_machine_irq;
		ctx_isp->substate_machine =
			cam_isp_ctx_fs2_state_machine;
	} else {
		CAM_DBG(CAM_ISP, "Session has PIX or PIX and RDI resources");
		ctx_isp->substate_machine_irq =
			cam_isp_ctx_activated_state_machine_irq;
		ctx_isp->substate_machine =
			cam_isp_ctx_activated_state_machine;
	}

	ctx_isp->hw_ctx = param.ctxt_to_hw_map;
	ctx_isp->hw_acquired = true;
	ctx_isp->split_acquire = false;
	ctx->ctxt_to_hw_map = param.ctxt_to_hw_map;

	kfree(isp_res);
	isp_res = NULL;

get_dev_handle:

	req_hdl_param.session_hdl = cmd->session_handle;
	/* bridge is not ready for these flags. so false for now */
	req_hdl_param.v4l2_sub_dev_flag = 0;
	req_hdl_param.media_entity_flag = 0;
	req_hdl_param.ops = ctx->crm_ctx_intf;
	req_hdl_param.priv = ctx;

	CAM_DBG(CAM_ISP, "get device handle form bridge");
	ctx->dev_hdl = cam_create_device_hdl(&req_hdl_param);
	if (ctx->dev_hdl <= 0) {
		rc = -EFAULT;
		CAM_ERR(CAM_ISP, "Can not create device handle");
		goto free_hw;
	}
	cmd->dev_handle = ctx->dev_hdl;

	/* store session information */
	ctx->session_hdl = cmd->session_handle;
	ctx->state = CAM_CTX_ACQUIRED;

	trace_cam_context_state("ISP", ctx);
	CAM_DBG(CAM_ISP,
		"Acquire success on session_hdl 0x%x num_rsrces %d ctx %u",
		cmd->session_handle, cmd->num_resources, ctx->ctx_id);

	return rc;

free_hw:
	release.ctxt_to_hw_map = ctx_isp->hw_ctx;
	if (ctx_isp->hw_acquired)
		ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv,
			&release);
	ctx_isp->hw_ctx = NULL;
	ctx_isp->hw_acquired = false;
free_res:
	kfree(isp_res);
end:
	return rc;
}

static int __cam_isp_ctx_acquire_hw_v1(struct cam_context *ctx,
	void *args)
{
	int rc = 0;
	struct cam_acquire_hw_cmd_v1 *cmd =
		(struct cam_acquire_hw_cmd_v1 *)args;
	struct cam_hw_acquire_args       param;
	struct cam_hw_release_args       release;
	struct cam_isp_context          *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;
	struct cam_hw_cmd_args           hw_cmd_args;
	struct cam_isp_hw_cmd_args       isp_hw_cmd_args;
	struct cam_isp_acquire_hw_info  *acquire_hw_info = NULL;

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_ISP, "HW interface is not ready");
		rc = -EFAULT;
		goto end;
	}

	CAM_DBG(CAM_ISP,
		"session_hdl 0x%x, hdl type %d, res %lld",
		cmd->session_handle, cmd->handle_type, cmd->resource_hdl);

	/* for now we only support user pointer */
	if (cmd->handle_type != 1)  {
		CAM_ERR(CAM_ISP, "Only user pointer is supported");
		rc = -EINVAL;
		goto end;
	}

	if (cmd->data_size < sizeof(*acquire_hw_info)) {
		CAM_ERR(CAM_ISP, "data_size is not a valid value");
		goto end;
	}

	acquire_hw_info = kzalloc(cmd->data_size, GFP_KERNEL);
	if (!acquire_hw_info) {
		rc = -ENOMEM;
		goto end;
	}

	CAM_DBG(CAM_ISP, "start copy resources from user");

	if (copy_from_user(acquire_hw_info, (void __user *)cmd->resource_hdl,
		cmd->data_size)) {
		rc = -EFAULT;
		goto free_res;
	}

	param.context_data = ctx;
	param.event_cb = ctx->irq_cb_intf;
	param.num_acq = CAM_API_COMPAT_CONSTANT;
	param.acquire_info_size = cmd->data_size;
	param.acquire_info = (uint64_t) acquire_hw_info;

	/* call HW manager to reserve the resource */
	rc = ctx->hw_mgr_intf->hw_acquire(ctx->hw_mgr_intf->hw_mgr_priv,
		&param);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Acquire device failed");
		goto free_res;
	}

	/* Query the context has rdi only resource */
	hw_cmd_args.ctxt_to_hw_map = param.ctxt_to_hw_map;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;
	isp_hw_cmd_args.cmd_type = CAM_ISP_HW_MGR_CMD_CTX_TYPE;
	hw_cmd_args.u.internal_args = (void *)&isp_hw_cmd_args;
	rc = ctx->hw_mgr_intf->hw_cmd(ctx->hw_mgr_intf->hw_mgr_priv,
				&hw_cmd_args);
	if (rc) {
		CAM_ERR(CAM_ISP, "HW command failed");
		goto free_hw;
	}

	if (isp_hw_cmd_args.u.ctx_type == CAM_ISP_CTX_RDI) {
		/*
		 * this context has rdi only resource assign rdi only
		 * state machine
		 */
		CAM_DBG(CAM_ISP, "RDI only session Context");

		ctx_isp->substate_machine_irq =
			cam_isp_ctx_rdi_only_activated_state_machine_irq;
		ctx_isp->substate_machine =
			cam_isp_ctx_rdi_only_activated_state_machine;
		ctx_isp->rdi_only_context = true;
	} else if (isp_hw_cmd_args.u.ctx_type == CAM_ISP_CTX_FS2) {
		CAM_DBG(CAM_ISP, "FS2 Session has PIX ,RD and RDI");
		ctx_isp->substate_machine_irq =
			cam_isp_ctx_fs2_state_machine_irq;
		ctx_isp->substate_machine =
			cam_isp_ctx_fs2_state_machine;
	} else {
		CAM_DBG(CAM_ISP, "Session has PIX or PIX and RDI resources");
		ctx_isp->substate_machine_irq =
			cam_isp_ctx_activated_state_machine_irq;
		ctx_isp->substate_machine =
			cam_isp_ctx_activated_state_machine;
	}

	ctx_isp->hw_ctx = param.ctxt_to_hw_map;
	ctx_isp->hw_acquired = true;
	ctx->ctxt_to_hw_map = param.ctxt_to_hw_map;

	trace_cam_context_state("ISP", ctx);
	CAM_DBG(CAM_ISP,
		"Acquire success on session_hdl 0x%xs ctx_type %d ctx_id %u",
		ctx->session_hdl, isp_hw_cmd_args.u.ctx_type, ctx->ctx_id);
	kfree(acquire_hw_info);
	return rc;

free_hw:
	release.ctxt_to_hw_map = ctx_isp->hw_ctx;
	ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv, &release);
	ctx_isp->hw_ctx = NULL;
	ctx_isp->hw_acquired = false;
free_res:
	kfree(acquire_hw_info);
end:
	return rc;
}

static int __cam_isp_ctx_acquire_hw_in_acquired(struct cam_context *ctx,
	void *args)
{
	int rc = -EINVAL;
	uint32_t api_version;

	if (!ctx || !args) {
		CAM_ERR(CAM_ISP, "Invalid input pointer");
		return rc;
	}

	api_version = *((uint32_t *)args);
	if (api_version == 1)
		rc = __cam_isp_ctx_acquire_hw_v1(ctx, args);
	else
		CAM_ERR(CAM_ISP, "Unsupported api version %d", api_version);

	return rc;
}

static int __cam_isp_ctx_config_dev_in_acquired(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	if (!ctx_isp->hw_acquired) {
		CAM_ERR(CAM_ISP, "HW is not acquired, reject packet");
		return -EINVAL;
	}

	rc = __cam_isp_ctx_config_dev_in_top_state(ctx, cmd);

	if (!rc && (ctx->link_hdl >= 0)) {
		ctx->state = CAM_CTX_READY;
		trace_cam_context_state("ISP", ctx);
	}

	CAM_DBG(CAM_ISP, "next state %d", ctx->state);
	return rc;
}

static int __cam_isp_ctx_link_in_acquired(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *link)
{
	int rc = 0;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	CAM_DBG(CAM_ISP, "Enter.........");

	ctx->link_hdl = link->link_hdl;
	ctx->ctx_crm_intf = link->crm_cb;
	ctx_isp->subscribe_event = link->subscribe_event;

	/* change state only if we had the init config */
	if (ctx_isp->init_received) {
		ctx->state = CAM_CTX_READY;
		trace_cam_context_state("ISP", ctx);
	}

	CAM_DBG(CAM_ISP, "next state %d", ctx->state);

	return rc;
}

static int __cam_isp_ctx_unlink_in_acquired(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	int rc = 0;

	ctx->link_hdl = -1;
	ctx->ctx_crm_intf = NULL;

	return rc;
}

static int __cam_isp_ctx_get_dev_info_in_acquired(struct cam_context *ctx,
	struct cam_req_mgr_device_info *dev_info)
{
	int rc = 0;

	dev_info->dev_hdl = ctx->dev_hdl;
	strlcpy(dev_info->name, CAM_ISP_DEV_NAME, sizeof(dev_info->name));
	dev_info->dev_id = CAM_REQ_MGR_DEVICE_IFE;
	dev_info->p_delay = 1;
	dev_info->trigger = CAM_TRIGGER_POINT_SOF;

	return rc;
}

static int __cam_isp_ctx_start_dev_in_ready(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_isp_start_args        start_isp;
	struct cam_ctx_request          *req;
	struct cam_isp_ctx_req          *req_isp;
	struct cam_isp_context          *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	if (cmd->session_handle != ctx->session_hdl ||
		cmd->dev_handle != ctx->dev_hdl) {
		rc = -EPERM;
		goto end;
	}

	if (list_empty(&ctx->pending_req_list)) {
		/* should never happen */
		CAM_ERR(CAM_ISP, "Start device with empty configuration");
		rc = -EFAULT;
		goto end;
	} else {
		req = list_first_entry(&ctx->pending_req_list,
			struct cam_ctx_request, list);
	}
	req_isp = (struct cam_isp_ctx_req *) req->req_priv;

	if (!ctx_isp->hw_ctx) {
		CAM_ERR(CAM_ISP, "Wrong hw context pointer.");
		rc = -EFAULT;
		goto end;
	}

	start_isp.hw_config.ctxt_to_hw_map = ctx_isp->hw_ctx;
	start_isp.hw_config.request_id = req->request_id;
	start_isp.hw_config.hw_update_entries = req_isp->cfg;
	start_isp.hw_config.num_hw_update_entries = req_isp->num_cfg;
	start_isp.hw_config.priv  = &req_isp->hw_update_data;
	start_isp.hw_config.init_packet = 1;
	start_isp.start_only = false;

	atomic_set(&ctx_isp->process_bubble, 0);
	ctx_isp->frame_id = 0;
	ctx_isp->active_req_cnt = 0;
	ctx_isp->reported_req_id = 0;
	ctx_isp->substate_activated = ctx_isp->rdi_only_context ?
		CAM_ISP_CTX_ACTIVATED_APPLIED :
		(req_isp->num_fence_map_out) ? CAM_ISP_CTX_ACTIVATED_EPOCH :
		CAM_ISP_CTX_ACTIVATED_SOF;

	/*
	 * Only place to change state before calling the hw due to
	 * hardware tasklet has higher priority that can cause the
	 * irq handling comes early
	 */
	ctx->state = CAM_CTX_ACTIVATED;
	trace_cam_context_state("ISP", ctx);
	rc = ctx->hw_mgr_intf->hw_start(ctx->hw_mgr_intf->hw_mgr_priv,
		&start_isp);
	if (rc) {
		/* HW failure. user need to clean up the resource */
		CAM_ERR(CAM_ISP, "Start HW failed");
		ctx->state = CAM_CTX_READY;
		trace_cam_context_state("ISP", ctx);
		goto end;
	}
	CAM_DBG(CAM_ISP, "start device success ctx %u", ctx->ctx_id);

	list_del_init(&req->list);

	if (req_isp->num_fence_map_out) {
		list_add_tail(&req->list, &ctx->active_req_list);
		ctx_isp->active_req_cnt++;
	} else {
		list_add_tail(&req->list, &ctx->wait_req_list);
	}
end:
	return rc;
}

static int __cam_isp_ctx_unlink_in_ready(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	int rc = 0;

	ctx->link_hdl = -1;
	ctx->ctx_crm_intf = NULL;
	ctx->state = CAM_CTX_ACQUIRED;
	trace_cam_context_state("ISP", ctx);

	return rc;
}

static int __cam_isp_ctx_stop_dev_in_activated_unlock(
	struct cam_context *ctx, struct cam_start_stop_dev_cmd *stop_cmd)
{
	int rc = 0;
	uint32_t i;
	struct cam_hw_stop_args          stop;
	struct cam_ctx_request          *req;
	struct cam_isp_ctx_req          *req_isp;
	struct cam_isp_context          *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;
	struct cam_isp_stop_args         stop_isp;

	/* Mask off all the incoming hardware events */
	spin_lock_bh(&ctx->lock);
	ctx_isp->substate_activated = CAM_ISP_CTX_ACTIVATED_HALT;
	spin_unlock_bh(&ctx->lock);
	CAM_DBG(CAM_ISP, "next substate %d", ctx_isp->substate_activated);

	/* stop hw first */
	if (ctx_isp->hw_ctx) {
		stop.ctxt_to_hw_map = ctx_isp->hw_ctx;

		if (stop_cmd)
			stop_isp.hw_stop_cmd =
				CAM_ISP_HW_STOP_AT_FRAME_BOUNDARY;
		else
			stop_isp.hw_stop_cmd = CAM_ISP_HW_STOP_IMMEDIATELY;

		stop_isp.stop_only = false;
		stop.args = (void *) &stop_isp;
		ctx->hw_mgr_intf->hw_stop(ctx->hw_mgr_intf->hw_mgr_priv,
			&stop);
	}

	while (!list_empty(&ctx->pending_req_list)) {
		req = list_first_entry(&ctx->pending_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		CAM_DBG(CAM_ISP, "signal fence in pending list. fence num %d",
			 req_isp->num_fence_map_out);
		for (i = 0; i < req_isp->num_fence_map_out; i++)
			if (req_isp->fence_map_out[i].sync_id != -1) {
				cam_sync_signal(
					req_isp->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
			}
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	while (!list_empty(&ctx->wait_req_list)) {
		req = list_first_entry(&ctx->wait_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		CAM_DBG(CAM_ISP, "signal fence in wait list. fence num %d",
			 req_isp->num_fence_map_out);
		for (i = 0; i < req_isp->num_fence_map_out; i++)
			if (req_isp->fence_map_out[i].sync_id != -1) {
				cam_sync_signal(
					req_isp->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
			}
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	while (!list_empty(&ctx->active_req_list)) {
		req = list_first_entry(&ctx->active_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		CAM_DBG(CAM_ISP, "signal fence in active list. fence num %d",
			 req_isp->num_fence_map_out);
		for (i = 0; i < req_isp->num_fence_map_out; i++)
			if (req_isp->fence_map_out[i].sync_id != -1) {
				cam_sync_signal(
					req_isp->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
			}
		list_add_tail(&req->list, &ctx->free_req_list);
	}
	ctx_isp->frame_id = 0;
	ctx_isp->active_req_cnt = 0;
	ctx_isp->reported_req_id = 0;
	atomic_set(&ctx_isp->process_bubble, 0);

	CAM_DBG(CAM_ISP, "Stop device success next state %d on ctx %u",
		ctx->state, ctx->ctx_id);

	if (!stop_cmd) {
		rc = __cam_isp_ctx_unlink_in_ready(ctx, NULL);
		if (rc)
			CAM_ERR(CAM_ISP, "Unlink failed rc=%d", rc);
	}
	return rc;
}

static int __cam_isp_ctx_stop_dev_in_activated(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *)ctx->ctx_priv;

	__cam_isp_ctx_stop_dev_in_activated_unlock(ctx, cmd);
	ctx_isp->init_received = false;
	ctx->state = CAM_CTX_ACQUIRED;
	trace_cam_context_state("ISP", ctx);
	return rc;
}

static int __cam_isp_ctx_release_dev_in_activated(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc = 0;

	rc = __cam_isp_ctx_stop_dev_in_activated_unlock(ctx, NULL);
	if (rc)
		CAM_ERR(CAM_ISP, "Stop device failed rc=%d", rc);

	rc = __cam_isp_ctx_release_dev_in_top_state(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_ISP, "Release device failed rc=%d", rc);

	return rc;
}

static int __cam_isp_ctx_release_hw_in_activated(struct cam_context *ctx,
	void *cmd)
{
	int rc = 0;

	rc = __cam_isp_ctx_stop_dev_in_activated_unlock(ctx, NULL);
	if (rc)
		CAM_ERR(CAM_ISP, "Stop device failed rc=%d", rc);

	rc = __cam_isp_ctx_release_hw_in_top_state(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_ISP, "Release hw failed rc=%d", rc);

	return rc;
}

static int __cam_isp_ctx_link_pause(struct cam_context *ctx)
{
	int rc = 0;
	struct cam_hw_cmd_args       hw_cmd_args;
	struct cam_isp_hw_cmd_args   isp_hw_cmd_args;
	struct cam_isp_context      *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	hw_cmd_args.ctxt_to_hw_map = ctx_isp->hw_ctx;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;
	isp_hw_cmd_args.cmd_type = CAM_ISP_HW_MGR_CMD_PAUSE_HW;
	hw_cmd_args.u.internal_args = (void *)&isp_hw_cmd_args;
	rc = ctx->hw_mgr_intf->hw_cmd(ctx->hw_mgr_intf->hw_mgr_priv,
		&hw_cmd_args);

	return rc;
}

static int __cam_isp_ctx_link_resume(struct cam_context *ctx)
{
	int rc = 0;
	struct cam_hw_cmd_args       hw_cmd_args;
	struct cam_isp_hw_cmd_args   isp_hw_cmd_args;
	struct cam_isp_context      *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	hw_cmd_args.ctxt_to_hw_map = ctx_isp->hw_ctx;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;
	isp_hw_cmd_args.cmd_type = CAM_ISP_HW_MGR_CMD_RESUME_HW;
	hw_cmd_args.u.internal_args = (void *)&isp_hw_cmd_args;
	rc = ctx->hw_mgr_intf->hw_cmd(ctx->hw_mgr_intf->hw_mgr_priv,
		&hw_cmd_args);

	return rc;
}

static int __cam_isp_ctx_handle_sof_freeze_evt(
	struct cam_context *ctx)
{
	int rc = 0;
	struct cam_hw_cmd_args       hw_cmd_args;
	struct cam_isp_hw_cmd_args   isp_hw_cmd_args;
	struct cam_isp_context      *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	hw_cmd_args.ctxt_to_hw_map = ctx_isp->hw_ctx;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;
	isp_hw_cmd_args.cmd_type = CAM_ISP_HW_MGR_CMD_SOF_DEBUG;
	isp_hw_cmd_args.u.sof_irq_enable = 1;
	hw_cmd_args.u.internal_args = (void *)&isp_hw_cmd_args;

	rc = ctx->hw_mgr_intf->hw_cmd(ctx->hw_mgr_intf->hw_mgr_priv,
		&hw_cmd_args);

	return rc;
}

static int __cam_isp_ctx_process_evt(struct cam_context *ctx,
	struct cam_req_mgr_link_evt_data *link_evt_data)
{
	int rc = 0;

	switch (link_evt_data->evt_type) {
	case CAM_REQ_MGR_LINK_EVT_ERR:
		/* No need to handle this message now */
		break;
	case CAM_REQ_MGR_LINK_EVT_PAUSE:
		__cam_isp_ctx_link_pause(ctx);
		break;
	case CAM_REQ_MGR_LINK_EVT_RESUME:
		__cam_isp_ctx_link_resume(ctx);
		break;
	case CAM_REQ_MGR_LINK_EVT_SOF_FREEZE:
		__cam_isp_ctx_handle_sof_freeze_evt(ctx);
		break;
	default:
		CAM_WARN(CAM_ISP, "Unknown event from CRM");
		break;
	}
	return rc;
}

static int __cam_isp_ctx_unlink_in_activated(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	int rc = 0;

	CAM_WARN(CAM_ISP,
		"Received unlink in activated state. It's unexpected");

	rc = __cam_isp_ctx_stop_dev_in_activated_unlock(ctx, NULL);
	if (rc)
		CAM_WARN(CAM_ISP, "Stop device failed rc=%d", rc);

	rc = __cam_isp_ctx_unlink_in_ready(ctx, unlink);
	if (rc)
		CAM_ERR(CAM_ISP, "Unlink failed rc=%d", rc);

	return rc;
}

static int __cam_isp_ctx_apply_req(struct cam_context *ctx,
	struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_ctx_ops *ctx_ops = NULL;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *) ctx->ctx_priv;

	trace_cam_apply_req("ISP", apply->request_id);
	CAM_DBG(CAM_ISP, "Enter: apply req in Substate %d request _id:%lld",
		 ctx_isp->substate_activated, apply->request_id);
	ctx_ops = &ctx_isp->substate_machine[ctx_isp->substate_activated];
	if (ctx_ops->crm_ops.apply_req) {
		rc = ctx_ops->crm_ops.apply_req(ctx, apply);
	} else {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No handle function in activated substate %d",
			ctx_isp->substate_activated);
		rc = -EFAULT;
	}

	if (rc)
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Apply failed in active substate %d",
			ctx_isp->substate_activated);
	return rc;
}



static int __cam_isp_ctx_handle_irq_in_activated(void *context,
	uint32_t evt_id, void *evt_data)
{
	int rc = 0;
	struct cam_isp_ctx_irq_ops *irq_ops = NULL;
	struct cam_context *ctx = (struct cam_context *)context;
	struct cam_isp_context *ctx_isp =
		(struct cam_isp_context *)ctx->ctx_priv;

	spin_lock(&ctx->lock);

	trace_cam_isp_activated_irq(ctx, ctx_isp->substate_activated, evt_id,
		__cam_isp_ctx_get_event_ts(evt_id, evt_data));

	CAM_DBG(CAM_ISP, "Enter: State %d, Substate %d, evt id %d",
		 ctx->state, ctx_isp->substate_activated, evt_id);
	irq_ops = &ctx_isp->substate_machine_irq[ctx_isp->substate_activated];
	if (irq_ops->irq_ops[evt_id]) {
		rc = irq_ops->irq_ops[evt_id](ctx_isp, evt_data);
	} else {
		CAM_DBG(CAM_ISP, "No handle function for substate %d",
			ctx_isp->substate_activated);
		__cam_isp_ctx_dump_state_monitor_array(ctx_isp);
	}

	CAM_DBG(CAM_ISP, "Exit: State %d Substate %d",
		 ctx->state, ctx_isp->substate_activated);
	spin_unlock(&ctx->lock);
	return rc;
}

/* top state machine */
static struct cam_ctx_ops
	cam_isp_ctx_top_state_machine[CAM_CTX_STATE_MAX] = {
	/* Uninit */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Available */
	{
		.ioctl_ops = {
			.acquire_dev = __cam_isp_ctx_acquire_dev_in_available,
		},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Acquired */
	{
		.ioctl_ops = {
			.acquire_hw = __cam_isp_ctx_acquire_hw_in_acquired,
			.release_dev = __cam_isp_ctx_release_dev_in_top_state,
			.config_dev = __cam_isp_ctx_config_dev_in_acquired,
			.release_hw = __cam_isp_ctx_release_hw_in_top_state,
		},
		.crm_ops = {
			.link = __cam_isp_ctx_link_in_acquired,
			.unlink = __cam_isp_ctx_unlink_in_acquired,
			.get_dev_info = __cam_isp_ctx_get_dev_info_in_acquired,
			.flush_req = __cam_isp_ctx_flush_req_in_top_state,
		},
		.irq_ops = NULL,
		.pagefault_ops = cam_isp_context_dump_active_request,
	},
	/* Ready */
	{
		.ioctl_ops = {
			.start_dev = __cam_isp_ctx_start_dev_in_ready,
			.release_dev = __cam_isp_ctx_release_dev_in_top_state,
			.config_dev = __cam_isp_ctx_config_dev_in_top_state,
			.release_hw = __cam_isp_ctx_release_hw_in_top_state,
		},
		.crm_ops = {
			.unlink = __cam_isp_ctx_unlink_in_ready,
			.flush_req = __cam_isp_ctx_flush_req_in_ready,
		},
		.irq_ops = NULL,
		.pagefault_ops = cam_isp_context_dump_active_request,
	},
	/* Activated */
	{
		.ioctl_ops = {
			.stop_dev = __cam_isp_ctx_stop_dev_in_activated,
			.release_dev = __cam_isp_ctx_release_dev_in_activated,
			.config_dev = __cam_isp_ctx_config_dev_in_top_state,
			.release_hw = __cam_isp_ctx_release_hw_in_activated,
		},
		.crm_ops = {
			.unlink = __cam_isp_ctx_unlink_in_activated,
			.apply_req = __cam_isp_ctx_apply_req,
			.flush_req = __cam_isp_ctx_flush_req_in_top_state,
			.process_evt = __cam_isp_ctx_process_evt,
		},
		.irq_ops = __cam_isp_ctx_handle_irq_in_activated,
		.pagefault_ops = cam_isp_context_dump_active_request,
	},
};


static int cam_isp_context_dump_active_request(void *data, unsigned long iova,
	uint32_t buf_info)
{

	struct cam_context *ctx = (struct cam_context *)data;
	struct cam_ctx_request *req = NULL;
	struct cam_ctx_request *req_temp = NULL;
	struct cam_isp_ctx_req *req_isp  = NULL;
	struct cam_isp_prepare_hw_update_data *hw_update_data = NULL;
	struct cam_hw_mgr_dump_pf_data *pf_dbg_entry = NULL;
	bool mem_found = false;
	int rc = 0;

	struct cam_isp_context *isp_ctx =
		(struct cam_isp_context *)ctx->ctx_priv;

	if (!isp_ctx) {
		CAM_ERR(CAM_ISP, "Invalid isp ctx");
		return -EINVAL;
	}

	CAM_INFO(CAM_ISP, "iommu fault handler for isp ctx %d state %d",
		ctx->ctx_id, ctx->state);

	list_for_each_entry_safe(req, req_temp,
		&ctx->active_req_list, list) {
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		hw_update_data = &req_isp->hw_update_data;
		pf_dbg_entry = &(req->pf_data);
		CAM_INFO(CAM_ISP, "req_id : %lld ", req->request_id);

		rc = cam_context_dump_pf_info_to_hw(ctx, pf_dbg_entry->packet,
			iova, buf_info, &mem_found);
		if (rc)
			CAM_ERR(CAM_ISP, "Failed to dump pf info");

		if (mem_found)
			CAM_ERR(CAM_ISP, "Found page fault in req %lld %d",
				req->request_id, rc);
	}

	CAM_INFO(CAM_ISP, "Iterating over wait_list of isp ctx %d state %d",
			ctx->ctx_id, ctx->state);

	list_for_each_entry_safe(req, req_temp,
		&ctx->wait_req_list, list) {
		req_isp = (struct cam_isp_ctx_req *) req->req_priv;
		hw_update_data = &req_isp->hw_update_data;
		pf_dbg_entry = &(req->pf_data);
		CAM_INFO(CAM_ISP, "req_id : %lld ", req->request_id);

		rc = cam_context_dump_pf_info_to_hw(ctx, pf_dbg_entry->packet,
			iova, buf_info, &mem_found);
		if (rc)
			CAM_ERR(CAM_ISP, "Failed to dump pf info");

		if (mem_found)
			CAM_ERR(CAM_ISP, "Found page fault in req %lld %d",
				req->request_id, rc);
	}

	return rc;
}

int cam_isp_context_init(struct cam_isp_context *ctx,
	struct cam_context *ctx_base,
	struct cam_req_mgr_kmd_ops *crm_node_intf,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id)

{
	int rc = -1;
	int i;

	if (!ctx || !ctx_base) {
		CAM_ERR(CAM_ISP, "Invalid Context");
		goto err;
	}

	/* ISP context setup */
	memset(ctx, 0, sizeof(*ctx));

	ctx->base = ctx_base;
	ctx->frame_id = 0;
	ctx->active_req_cnt = 0;
	ctx->reported_req_id = 0;
	ctx->hw_ctx = NULL;
	ctx->substate_activated = CAM_ISP_CTX_ACTIVATED_SOF;
	ctx->substate_machine = cam_isp_ctx_activated_state_machine;
	ctx->substate_machine_irq = cam_isp_ctx_activated_state_machine_irq;

	for (i = 0; i < CAM_CTX_REQ_MAX; i++) {
		ctx->req_base[i].req_priv = &ctx->req_isp[i];
		ctx->req_isp[i].base = &ctx->req_base[i];
	}

	/* camera context setup */
	rc = cam_context_init(ctx_base, isp_dev_name, CAM_ISP, ctx_id,
		crm_node_intf, hw_intf, ctx->req_base, CAM_CTX_REQ_MAX);
	if (rc) {
		CAM_ERR(CAM_ISP, "Camera Context Base init failed");
		goto err;
	}

	/* link camera context with isp context */
	ctx_base->state_machine = cam_isp_ctx_top_state_machine;
	ctx_base->ctx_priv = ctx;

	/* initializing current state for error logging */
	for (i = 0; i < CAM_ISP_CTX_STATE_MONITOR_MAX_ENTRIES; i++) {
		ctx->cam_isp_ctx_state_monitor[i].curr_state =
		CAM_ISP_CTX_ACTIVATED_MAX;
	}
	atomic64_set(&ctx->state_monitor_head, -1);
err:
	return rc;
}

int cam_isp_context_deinit(struct cam_isp_context *ctx)
{
	int rc = 0;

	if (ctx->base)
		cam_context_deinit(ctx->base);

	if (ctx->substate_activated != CAM_ISP_CTX_ACTIVATED_SOF)
		CAM_ERR(CAM_ISP, "ISP context substate is invalid");

	memset(ctx, 0, sizeof(*ctx));
	return rc;
}
