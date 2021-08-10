// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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
#include "cam_custom_context.h"
#include "cam_common_util.h"

static const char custom_dev_name[] = "cam-custom";

static int __cam_custom_ctx_handle_irq_in_activated(
	void *context, uint32_t evt_id, void *evt_data);

static int __cam_custom_ctx_start_dev_in_ready(
	struct cam_context *ctx, struct cam_start_stop_dev_cmd *cmd);

static int __cam_custom_ctx_apply_req_in_activated_state(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply,
	uint32_t next_state);

static int __cam_custom_ctx_apply_default_settings(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply);

static int __cam_custom_ctx_apply_req_in_activated(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_custom_context *custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;

	rc = __cam_custom_ctx_apply_req_in_activated_state(
		ctx, apply, CAM_CUSTOM_CTX_ACTIVATED_APPLIED);
	CAM_DBG(CAM_CUSTOM, "new substate %d", custom_ctx->substate_activated);

	if (rc)
		CAM_ERR(CAM_CUSTOM, "Apply failed in state %d rc %d",
			custom_ctx->substate_activated, rc);

	return rc;
}

static int __cam_custom_ctx_handle_error(
	struct cam_custom_context *custom_ctx, void *evt_data)
{
	/*
	 * Handle any HW error scenerios here, all the
	 * requests in all the lists can be signaled error.
	 * Notify UMD about this error if needed.
	 */

	return 0;
}

static int __cam_custom_ctx_reg_upd_in_sof(
	struct cam_custom_context *custom_ctx, void *evt_data)
{
	struct cam_ctx_request *req = NULL;
	struct cam_custom_dev_ctx_req *req_custom;
	struct cam_context *ctx = custom_ctx->base;

	custom_ctx->frame_id++;

	/*
	 * This is for the first update before streamon.
	 * The initial setting will cause the reg_upd in the
	 * first frame.
	 */
	if (!list_empty(&ctx->wait_req_list)) {
		req = list_first_entry(&ctx->wait_req_list,
			struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;
		if (req_custom->num_fence_map_out == req_custom->num_acked) {
			list_add_tail(&req->list, &ctx->free_req_list);
		} else {
			list_add_tail(&req->list, &ctx->active_req_list);
			custom_ctx->active_req_cnt++;
			CAM_DBG(CAM_REQ,
				"move request %lld to active list(cnt = %d), ctx %u",
				req->request_id, custom_ctx->active_req_cnt,
				ctx->ctx_id);
		}
	}

	return 0;
}

static int __cam_custom_ctx_reg_upd_in_applied_state(
	struct cam_custom_context *custom_ctx, void *evt_data)
{
	struct cam_ctx_request         *req;
	struct cam_context             *ctx = custom_ctx->base;
	struct cam_custom_dev_ctx_req  *req_custom;

	custom_ctx->frame_id++;
	if (list_empty(&ctx->wait_req_list)) {
		CAM_ERR(CAM_CUSTOM,
				"Reg upd ack with no waiting request");
		goto end;
	}
	req = list_first_entry(&ctx->wait_req_list,
			struct cam_ctx_request, list);
	list_del_init(&req->list);

	req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;
	if (req_custom->num_fence_map_out != 0) {
		list_add_tail(&req->list, &ctx->active_req_list);
		custom_ctx->active_req_cnt++;
		CAM_DBG(CAM_REQ,
			"move request %lld to active list(cnt = %d), ctx %u",
			req->request_id, custom_ctx->active_req_cnt,
			ctx->ctx_id);
	} else {
		/* no io config, so the request is completed. */
		list_add_tail(&req->list, &ctx->free_req_list);
		CAM_DBG(CAM_ISP,
			"move active request %lld to free list(cnt = %d), ctx %u",
			req->request_id, custom_ctx->active_req_cnt,
			ctx->ctx_id);
	}

	custom_ctx->substate_activated = CAM_CUSTOM_CTX_ACTIVATED_SOF;
	CAM_DBG(CAM_CUSTOM, "next substate %d", custom_ctx->substate_activated);

end:
	return 0;
}

static int __cam_custom_ctx_frame_done(
	struct cam_custom_context *custom_ctx, void *evt_data)
{
	int rc = 0, i, j;
	uint64_t frame_done_req_id;
	struct cam_ctx_request  *req;
	struct cam_custom_dev_ctx_req  *req_custom;
	struct cam_context *ctx = custom_ctx->base;
	struct cam_custom_hw_done_event_data *done_data =
		(struct cam_custom_hw_done_event_data *)evt_data;

	if (list_empty(&ctx->active_req_list)) {
		CAM_DBG(CAM_CUSTOM, "Frame done with no active request");
		return 0;
	}

	req = list_first_entry(&ctx->active_req_list,
			struct cam_ctx_request, list);
	req_custom = req->req_priv;

	for (i = 0; i < done_data->num_handles; i++) {
		for (j = 0; j < req_custom->num_fence_map_out; j++) {
			if (done_data->resource_handle[i] ==
				req_custom->fence_map_out[j].resource_handle)
				break;
		}

		if (j == req_custom->num_fence_map_out) {
			CAM_ERR(CAM_CUSTOM,
				"Can not find matching rsrc handle 0x%x!",
				done_data->resource_handle[i]);
			rc = -EINVAL;
			continue;
		}

		if (req_custom->fence_map_out[j].sync_id == -1) {
			CAM_WARN(CAM_CUSTOM,
				"Duplicate frame done for req %lld",
				req->request_id);
			continue;
		}

		if (!req_custom->bubble_detected) {
			rc = cam_sync_signal(
				req_custom->fence_map_out[j].sync_id,
				CAM_SYNC_STATE_SIGNALED_SUCCESS,
				CAM_SYNC_COMMON_EVENT_SUCCESS);
			if (rc)
				CAM_ERR(CAM_CUSTOM,
					"Sync failed with rc = %d", rc);
		} else if (!req_custom->bubble_report) {
			rc = cam_sync_signal(
				req_custom->fence_map_out[j].sync_id,
				CAM_SYNC_STATE_SIGNALED_ERROR,
				CAM_SYNC_ISP_EVENT_BUBBLE);
			if (rc)
				CAM_ERR(CAM_CUSTOM,
					"Sync failed with rc = %d", rc);
		} else {
			req_custom->num_acked++;
			CAM_DBG(CAM_CUSTOM, "frame done with bubble for %llu",
				req->request_id);
			continue;
		}

		req_custom->num_acked++;
		req_custom->fence_map_out[j].sync_id = -1;
	}

	if (req_custom->num_acked > req_custom->num_fence_map_out) {
		CAM_ERR(CAM_CUSTOM,
			"WARNING: req_id %lld num_acked %d > map_out %d, ctx %u",
			req->request_id, req_custom->num_acked,
			req_custom->num_fence_map_out, ctx->ctx_id);
	}

	if (req_custom->num_acked != req_custom->num_fence_map_out)
		return rc;

	custom_ctx->active_req_cnt--;
	frame_done_req_id = req->request_id;
	if (req_custom->bubble_detected && req_custom->bubble_report) {
		req_custom->num_acked = 0;
		req_custom->bubble_detected = false;
		list_del_init(&req->list);
		if (frame_done_req_id <= ctx->last_flush_req) {
			for (i = 0; i < req_custom->num_fence_map_out; i++)
				rc = cam_sync_signal(
					req_custom->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR,
					CAM_SYNC_ISP_EVENT_BUBBLE);

			list_add_tail(&req->list, &ctx->free_req_list);
			atomic_set(&custom_ctx->process_bubble, 0);
			CAM_DBG(CAM_REQ,
				"Move active request %lld to free list(cnt = %d) [flushed], ctx %u",
				frame_done_req_id, custom_ctx->active_req_cnt,
				ctx->ctx_id);
		} else {
			list_add(&req->list, &ctx->pending_req_list);
			atomic_set(&custom_ctx->process_bubble, 0);
			CAM_DBG(CAM_REQ,
				"Move active request %lld to pending list in ctx %u",
				frame_done_req_id, ctx->ctx_id);
		}
	} else {
		list_del_init(&req->list);
		list_add_tail(&req->list, &ctx->free_req_list);
		CAM_DBG(CAM_REQ,
			"Move active request %lld to free list(cnt = %d) [all fences done], ctx %u",
			frame_done_req_id,
			custom_ctx->active_req_cnt,
			ctx->ctx_id);
	}

	return rc;
}

static int __cam_custom_ctx_handle_bubble(
	struct cam_context *ctx, uint64_t req_id)
{
	int                              rc = -EINVAL;
	bool                             found = false;
	struct cam_ctx_request          *req = NULL;
	struct cam_ctx_request          *req_temp;
	struct cam_custom_dev_ctx_req   *req_custom;

	list_for_each_entry_safe(req, req_temp,
		&ctx->wait_req_list, list) {
		if (req->request_id == req_id) {
			req_custom =
				(struct cam_custom_dev_ctx_req *)req->req_priv;
			if (!req_custom->bubble_report) {
				CAM_DBG(CAM_CUSTOM,
					"Skip bubble recovery for %llu",
					req_id);
				goto end;
			}

			req_custom->bubble_detected = true;
			found = true;
			CAM_DBG(CAM_CUSTOM,
				"Found bubbled req %llu in wait list",
				req_id);
		}
	}

	if (found) {
		rc = 0;
		goto end;
	}

	list_for_each_entry_safe(req, req_temp,
		&ctx->active_req_list, list) {
		if (req->request_id == req_id) {
			req_custom =
				(struct cam_custom_dev_ctx_req *)req->req_priv;
			if (!req_custom->bubble_report) {
				CAM_DBG(CAM_CUSTOM,
					"Skip bubble recovery for %llu",
					req_id);
				goto end;
			}

			req_custom->bubble_detected = true;
			found = true;
			CAM_DBG(CAM_CUSTOM,
				"Found bubbled req %llu in active list",
				req_id);
		}
	}

	if (found)
		rc = 0;
	else
		CAM_ERR(CAM_CUSTOM,
			"req %llu not found in wait or active list bubble recovery failed ctx: %u",
			req_id, ctx->ctx_id);

end:
	return rc;
}

static int __cam_custom_ctx_handle_evt(
	struct cam_context *ctx,
	struct cam_req_mgr_link_evt_data *evt_data)
{
	int rc = -1;
	struct cam_custom_context *custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;

	if (evt_data->u.error == CRM_KMD_ERR_BUBBLE) {
		rc = __cam_custom_ctx_handle_bubble(ctx, evt_data->req_id);
		if (rc)
			return rc;
	} else {
		CAM_WARN(CAM_CUSTOM, "Unsupported error type %d",
			evt_data->u.error);
	}

	CAM_DBG(CAM_CUSTOM, "Set bubble flag for req %llu in ctx %u",
		evt_data->req_id, ctx->ctx_id);
	atomic_set(&custom_ctx->process_bubble, 1);
	return 0;
}

static struct cam_ctx_ops
	cam_custom_ctx_activated_state_machine
	[CAM_CUSTOM_CTX_ACTIVATED_MAX] = {
	/* SOF */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_custom_ctx_apply_req_in_activated,
			.notify_frame_skip =
				__cam_custom_ctx_apply_default_settings,
		},
		.irq_ops = NULL,
	},
	/* APPLIED */
	{
		.ioctl_ops = {},
		.crm_ops = {
			.apply_req = __cam_custom_ctx_apply_req_in_activated,
			.notify_frame_skip =
				__cam_custom_ctx_apply_default_settings,
		},
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

static struct cam_custom_ctx_irq_ops
	cam_custom_ctx_activated_state_machine_irq
	[CAM_CUSTOM_CTX_ACTIVATED_MAX] = {
	/* SOF */
	{
		.irq_ops = {
			__cam_custom_ctx_handle_error,
			__cam_custom_ctx_reg_upd_in_sof,
			__cam_custom_ctx_frame_done,
		},
	},
	/* APPLIED */
	{
		.irq_ops = {
			__cam_custom_ctx_handle_error,
			__cam_custom_ctx_reg_upd_in_applied_state,
			__cam_custom_ctx_frame_done,
		},
	},
	/* HW ERROR */
	{
		.irq_ops = {
			NULL,
			NULL,
			NULL,
		},
	},
	/* HALT */
	{
	},
};

static int __cam_custom_ctx_enqueue_request_in_order(
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
				CAM_WARN(CAM_CUSTOM,
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

static int __cam_custom_ctx_flush_req(struct cam_context *ctx,
	struct list_head *req_list, struct cam_req_mgr_flush_request *flush_req)
{
	int i, rc;
	uint32_t cancel_req_id_found = 0;
	struct cam_ctx_request           *req;
	struct cam_ctx_request           *req_temp;
	struct cam_custom_dev_ctx_req    *req_custom;
	struct list_head                  flush_list;

	INIT_LIST_HEAD(&flush_list);
	if (list_empty(req_list)) {
		CAM_DBG(CAM_CUSTOM, "request list is empty");
		if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
			CAM_ERR(CAM_CUSTOM, "no request to cancel");
			return -EINVAL;
		} else {
			return 0;
		}
	}

	CAM_DBG(CAM_CUSTOM, "Flush [%u] in progress for req_id %llu",
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
		req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;
		for (i = 0; i < req_custom->num_fence_map_out; i++) {
			if (req_custom->fence_map_out[i].sync_id != -1) {
				CAM_DBG(CAM_CUSTOM,
					"Flush req 0x%llx, fence %d",
					 req->request_id,
					req_custom->fence_map_out[i].sync_id);
				rc = cam_sync_signal(
					req_custom->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_CANCEL,
					CAM_SYNC_COMMON_EVENT_FLUSH);
				if (rc)
					CAM_ERR_RATE_LIMIT(CAM_CUSTOM,
						"signal fence failed\n");
				req_custom->fence_map_out[i].sync_id = -1;
			}
		}
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ &&
		!cancel_req_id_found)
		CAM_DBG(CAM_CUSTOM,
			"Flush request id:%lld is not found in the list",
			flush_req->req_id);

	return 0;
}

static int __cam_custom_ctx_unlink_in_acquired(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	ctx->link_hdl = -1;
	ctx->ctx_crm_intf = NULL;

	return 0;
}

static int __cam_custom_ctx_unlink_in_ready(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	ctx->link_hdl = -1;
	ctx->ctx_crm_intf = NULL;
	ctx->state = CAM_CTX_ACQUIRED;

	return 0;
}

static int __cam_custom_ctx_get_dev_info_in_acquired(struct cam_context *ctx,
	struct cam_req_mgr_device_info *dev_info)
{
	dev_info->dev_hdl = ctx->dev_hdl;
	strlcpy(dev_info->name, CAM_CUSTOM_DEV_NAME, sizeof(dev_info->name));
	dev_info->dev_id = CAM_REQ_MGR_DEVICE_CUSTOM_HW;
	dev_info->p_delay = 1;
	dev_info->trigger = CAM_TRIGGER_POINT_SOF;

	return 0;
}

static int __cam_custom_ctx_flush_req_in_top_state(
	struct cam_context *ctx,
	struct cam_req_mgr_flush_request *flush_req)
{
	int rc = 0;
	struct cam_custom_context      *custom_ctx;
	struct cam_hw_reset_args        reset_args;
	struct cam_hw_stop_args         stop_args;
	struct cam_custom_stop_args     custom_stop;

	custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;

	CAM_DBG(CAM_CUSTOM, "Flushing pending list");
	spin_lock_bh(&ctx->lock);
	__cam_custom_ctx_flush_req(ctx, &ctx->pending_req_list, flush_req);
	spin_unlock_bh(&ctx->lock);

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		if (ctx->state <= CAM_CTX_READY) {
			ctx->state = CAM_CTX_ACQUIRED;
			goto end;
		}

		spin_lock_bh(&ctx->lock);
		ctx->state = CAM_CTX_FLUSHED;
		spin_unlock_bh(&ctx->lock);

		CAM_INFO(CAM_CUSTOM, "Last request id to flush is %lld",
			flush_req->req_id);
		ctx->last_flush_req = flush_req->req_id;

		/* stop hw first */
		if (ctx->hw_mgr_intf->hw_stop) {
			custom_stop.stop_only = true;
			stop_args.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
			stop_args.args = (void *) &custom_stop;
			rc = ctx->hw_mgr_intf->hw_stop(
				ctx->hw_mgr_intf->hw_mgr_priv, &stop_args);
			if (rc)
				CAM_ERR(CAM_CUSTOM,
					"HW stop failed in flush rc %d", rc);
		}

		spin_lock_bh(&ctx->lock);
		if (!list_empty(&ctx->wait_req_list))
			__cam_custom_ctx_flush_req(ctx, &ctx->wait_req_list,
			flush_req);

		if (!list_empty(&ctx->active_req_list))
			__cam_custom_ctx_flush_req(ctx, &ctx->active_req_list,
			flush_req);

		custom_ctx->active_req_cnt = 0;
		spin_unlock_bh(&ctx->lock);

		reset_args.ctxt_to_hw_map = custom_ctx->hw_ctx;
		rc = ctx->hw_mgr_intf->hw_reset(ctx->hw_mgr_intf->hw_mgr_priv,
			&reset_args);
		if (rc)
			CAM_ERR(CAM_CUSTOM,
				"Reset HW failed in flush rc %d", rc);

		custom_ctx->init_received = false;
	}

end:
	atomic_set(&custom_ctx->process_bubble, 0);
	return rc;
}

static int __cam_custom_ctx_flush_req_in_ready(
	struct cam_context *ctx,
	struct cam_req_mgr_flush_request *flush_req)
{
	int rc = 0;
	struct cam_custom_context *custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;

	CAM_DBG(CAM_CUSTOM, "try to flush pending list");
	spin_lock_bh(&ctx->lock);
	rc = __cam_custom_ctx_flush_req(ctx, &ctx->pending_req_list, flush_req);

	/* if nothing is in pending req list, change state to acquire */
	if (list_empty(&ctx->pending_req_list))
		ctx->state = CAM_CTX_ACQUIRED;
	spin_unlock_bh(&ctx->lock);

	atomic_set(&custom_ctx->process_bubble, 0);
	CAM_DBG(CAM_CUSTOM, "Flush request in ready state. next state %d",
		 ctx->state);
	return rc;
}

static int __cam_custom_stop_dev_core(
	struct cam_context *ctx, struct cam_start_stop_dev_cmd *stop_cmd)
{
	int rc = 0;
	uint32_t i;
	struct cam_custom_context          *ctx_custom =
		(struct cam_custom_context *)   ctx->ctx_priv;
	struct cam_ctx_request             *req;
	struct cam_custom_dev_ctx_req      *req_custom;
	struct cam_hw_stop_args             stop;
	struct cam_custom_stop_args         custom_stop;

	if ((ctx->state != CAM_CTX_FLUSHED) && (ctx_custom->hw_ctx) &&
		(ctx->hw_mgr_intf->hw_stop)) {
		custom_stop.stop_only = false;
		stop.ctxt_to_hw_map = ctx_custom->hw_ctx;
		stop.args = (void *) &custom_stop;
		rc = ctx->hw_mgr_intf->hw_stop(ctx->hw_mgr_intf->hw_mgr_priv,
			&stop);
		if (rc)
			CAM_ERR(CAM_CUSTOM, "HW stop failed rc %d", rc);
	}

	while (!list_empty(&ctx->pending_req_list)) {
		req = list_first_entry(&ctx->pending_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;
		CAM_DBG(CAM_CUSTOM,
			"signal fence in pending list. fence num %d",
			 req_custom->num_fence_map_out);
		for (i = 0; i < req_custom->num_fence_map_out; i++)
			if (req_custom->fence_map_out[i].sync_id != -1) {
				cam_sync_signal(
					req_custom->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_CANCEL,
					CAM_SYNC_COMMON_EVENT_STOP);
			}
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	while (!list_empty(&ctx->wait_req_list)) {
		req = list_first_entry(&ctx->wait_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;
		CAM_DBG(CAM_CUSTOM, "signal fence in wait list. fence num %d",
			 req_custom->num_fence_map_out);
		for (i = 0; i < req_custom->num_fence_map_out; i++)
			if (req_custom->fence_map_out[i].sync_id != -1) {
				cam_sync_signal(
					req_custom->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_CANCEL,
					CAM_SYNC_COMMON_EVENT_STOP);
			}
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	while (!list_empty(&ctx->active_req_list)) {
		req = list_first_entry(&ctx->active_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;
		CAM_DBG(CAM_CUSTOM, "signal fence in active list. fence num %d",
			 req_custom->num_fence_map_out);
		for (i = 0; i < req_custom->num_fence_map_out; i++)
			if (req_custom->fence_map_out[i].sync_id != -1) {
				cam_sync_signal(
					req_custom->fence_map_out[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_CANCEL,
					CAM_SYNC_COMMON_EVENT_STOP);
			}
		list_add_tail(&req->list, &ctx->free_req_list);
	}
	ctx_custom->frame_id = 0;
	ctx_custom->active_req_cnt = 0;

	CAM_DBG(CAM_CUSTOM, "Stop device success next state %d on ctx %u",
		ctx->state, ctx->ctx_id);

	if (!stop_cmd) {
		rc = __cam_custom_ctx_unlink_in_ready(ctx, NULL);
		if (rc)
			CAM_ERR(CAM_CUSTOM, "Unlink failed rc=%d", rc);
	}
	return rc;
}

static int __cam_custom_stop_dev_in_activated(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	struct cam_custom_context *ctx_custom =
		(struct cam_custom_context *)ctx->ctx_priv;

	__cam_custom_stop_dev_core(ctx, cmd);
	ctx_custom->init_received = false;
	ctx->state = CAM_CTX_ACQUIRED;

	return 0;
}

static int __cam_custom_ctx_release_hw_in_top_state(
	struct cam_context *ctx, void *cmd)
{
	int rc = 0;
	struct cam_hw_release_args        rel_arg;
	struct cam_req_mgr_flush_request  flush_req;
	struct cam_custom_context        *custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;

	if (custom_ctx->hw_ctx) {
		rel_arg.ctxt_to_hw_map = custom_ctx->hw_ctx;
		rc = ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv,
			&rel_arg);
		custom_ctx->hw_ctx = NULL;
		if (rc)
			CAM_ERR(CAM_CUSTOM,
				"Failed to release HW for ctx:%u", ctx->ctx_id);
	} else {
		CAM_ERR(CAM_CUSTOM, "No HW resources acquired for this ctx");
	}

	ctx->last_flush_req = 0;
	custom_ctx->frame_id = 0;
	custom_ctx->active_req_cnt = 0;
	custom_ctx->hw_acquired = false;
	custom_ctx->init_received = false;

	/* check for active requests as well */
	flush_req.type = CAM_REQ_MGR_FLUSH_TYPE_ALL;
	flush_req.link_hdl = ctx->link_hdl;
	flush_req.dev_hdl = ctx->dev_hdl;
	flush_req.req_id = 0;

	CAM_DBG(CAM_CUSTOM, "try to flush pending list");
	spin_lock_bh(&ctx->lock);
	rc = __cam_custom_ctx_flush_req(ctx, &ctx->pending_req_list,
		&flush_req);
	spin_unlock_bh(&ctx->lock);
	ctx->state = CAM_CTX_ACQUIRED;

	CAM_DBG(CAM_CUSTOM, "Release HW success[%u] next state %d",
		ctx->ctx_id, ctx->state);

	return rc;
}

static int __cam_custom_ctx_release_hw_in_activated_state(
	struct cam_context *ctx, void *cmd)
{
	int rc = 0;

	rc = __cam_custom_stop_dev_in_activated(ctx, NULL);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Stop device failed rc=%d", rc);

	rc = __cam_custom_ctx_release_hw_in_top_state(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Release hw failed rc=%d", rc);

	return rc;
}

static int __cam_custom_release_dev_in_acquired(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;
	struct cam_custom_context *ctx_custom =
		(struct cam_custom_context *) ctx->ctx_priv;
	struct cam_req_mgr_flush_request flush_req;

	if (cmd && ctx_custom->hw_ctx) {
		CAM_ERR(CAM_CUSTOM, "releasing hw");
		__cam_custom_ctx_release_hw_in_top_state(ctx, NULL);
	}

	ctx->ctx_crm_intf = NULL;
	ctx->last_flush_req = 0;
	ctx_custom->frame_id = 0;
	ctx_custom->active_req_cnt = 0;
	ctx_custom->hw_acquired = false;
	ctx_custom->init_received = false;

	if (!list_empty(&ctx->active_req_list))
		CAM_ERR(CAM_CUSTOM, "Active list is not empty");

	/* Flush all the pending request list  */
	flush_req.type = CAM_REQ_MGR_FLUSH_TYPE_ALL;
	flush_req.link_hdl = ctx->link_hdl;
	flush_req.dev_hdl = ctx->dev_hdl;
	flush_req.req_id = 0;

	CAM_DBG(CAM_CUSTOM, "try to flush pending list");
	spin_lock_bh(&ctx->lock);
	rc = __cam_custom_ctx_flush_req(ctx, &ctx->pending_req_list,
		&flush_req);
	spin_unlock_bh(&ctx->lock);
	ctx->state = CAM_CTX_AVAILABLE;

	CAM_DBG(CAM_CUSTOM, "Release device success[%u] next state %d",
		ctx->ctx_id, ctx->state);

	return rc;
}

static int __cam_custom_ctx_apply_default_settings(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_custom_context *custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;
	struct cam_hw_cmd_args        hw_cmd_args;
	struct cam_custom_hw_cmd_args custom_hw_cmd_args;

	hw_cmd_args.ctxt_to_hw_map = custom_ctx->hw_ctx;
	hw_cmd_args.cmd_type = CAM_HW_MGR_CMD_INTERNAL;
	custom_hw_cmd_args.cmd_type =
		CAM_CUSTOM_HW_MGR_PROG_DEFAULT_CONFIG;
	hw_cmd_args.u.internal_args = (void *)&custom_hw_cmd_args;

	rc = ctx->hw_mgr_intf->hw_cmd(ctx->hw_mgr_intf->hw_mgr_priv,
			&hw_cmd_args);
	if (rc)
		CAM_ERR(CAM_CUSTOM,
			"Failed to apply default settings rc %d", rc);
	else
		CAM_DBG(CAM_CUSTOM, "Applied default settings rc %d", rc);

	return rc;
}

static int __cam_custom_ctx_apply_req_in_activated_state(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply,
	uint32_t next_state)
{
	int rc = 0;
	struct cam_ctx_request          *req;
	struct cam_custom_dev_ctx_req   *req_custom;
	struct cam_custom_context       *custom_ctx = NULL;
	struct cam_hw_config_args        cfg;

	if (atomic_read(&custom_ctx->process_bubble)) {
		CAM_WARN(CAM_CUSTOM,
			"ctx_id:%d Processing bubble cannot apply Request Id %llu",
			ctx->ctx_id, apply->request_id);
		rc = -EAGAIN;
		goto end;
	}

	if (list_empty(&ctx->pending_req_list)) {
		CAM_ERR(CAM_CUSTOM, "No available request for Apply id %lld",
			apply->request_id);
		rc = -EFAULT;
		goto end;
	}

	if (!list_empty(&ctx->wait_req_list))
		CAM_WARN(CAM_CUSTOM, "Apply invoked with a req in wait list");

	custom_ctx = (struct cam_custom_context *) ctx->ctx_priv;
	spin_lock_bh(&ctx->lock);
	req = list_first_entry(&ctx->pending_req_list, struct cam_ctx_request,
		list);
	spin_unlock_bh(&ctx->lock);

	/*
	 * Check whether the request id is matching the tip
	 */
	if (req->request_id != apply->request_id) {
		CAM_ERR_RATE_LIMIT(CAM_CUSTOM,
			"Invalid Request Id asking %llu existing %llu",
			apply->request_id, req->request_id);
		rc = -EFAULT;
		goto end;
	}

	req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;
	req_custom->bubble_report = apply->report_if_bubble;
	cfg.ctxt_to_hw_map = custom_ctx->hw_ctx;
	cfg.request_id = req->request_id;
	cfg.hw_update_entries = req_custom->cfg;
	cfg.num_hw_update_entries = req_custom->num_cfg;
	cfg.priv  = &req_custom->hw_update_data;
	cfg.init_packet = 0;

	rc = ctx->hw_mgr_intf->hw_config(ctx->hw_mgr_intf->hw_mgr_priv, &cfg);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_CUSTOM,
			"Can not apply the configuration");
	} else {
		spin_lock_bh(&ctx->lock);
		custom_ctx->substate_activated = next_state;
		list_del_init(&req->list);
		list_add_tail(&req->list, &ctx->wait_req_list);
		spin_unlock_bh(&ctx->lock);
	}

end:
	return rc;
}

static int __cam_custom_ctx_acquire_hw_v1(
	struct cam_context *ctx, void *args)
{
	int rc = 0;
	struct cam_acquire_hw_cmd_v1 *cmd =
		(struct cam_acquire_hw_cmd_v1 *)args;
	struct cam_hw_acquire_args         param;
	struct cam_custom_context         *ctx_custom =
		(struct cam_custom_context *)  ctx->ctx_priv;
	struct cam_custom_acquire_hw_info *acquire_hw_info = NULL;

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_CUSTOM, "HW interface is not ready");
		rc = -EFAULT;
		goto end;
	}

	CAM_DBG(CAM_CUSTOM,
		"session_hdl 0x%x, hdl type %d, res %lld",
		cmd->session_handle, cmd->handle_type, cmd->resource_hdl);

	if (cmd->handle_type != 1)  {
		CAM_ERR(CAM_CUSTOM, "Only user pointer is supported");
		rc = -EINVAL;
		goto end;
	}

	if (cmd->data_size < sizeof(*acquire_hw_info)) {
		CAM_ERR(CAM_CUSTOM, "data_size is not a valid value");
		goto end;
	}

	acquire_hw_info = kzalloc(cmd->data_size, GFP_KERNEL);
	if (!acquire_hw_info) {
		rc = -ENOMEM;
		goto end;
	}

	CAM_DBG(CAM_CUSTOM, "start copy resources from user");

	if (copy_from_user(acquire_hw_info, (void __user *)cmd->resource_hdl,
		cmd->data_size)) {
		rc = -EFAULT;
		goto free_res;
	}

	memset(&param, 0, sizeof(param));
	param.context_data = ctx;
	param.event_cb = ctx->irq_cb_intf;
	param.acquire_info_size = cmd->data_size;
	param.acquire_info = (uint64_t) acquire_hw_info;

	/* call HW manager to reserve the resource */
	rc = ctx->hw_mgr_intf->hw_acquire(ctx->hw_mgr_intf->hw_mgr_priv,
		&param);
	if (rc != 0) {
		CAM_ERR(CAM_CUSTOM, "Acquire HW failed");
		goto free_res;
	}

	ctx_custom->substate_machine_irq =
		cam_custom_ctx_activated_state_machine_irq;
	ctx_custom->substate_machine =
		cam_custom_ctx_activated_state_machine;
	ctx_custom->hw_ctx = param.ctxt_to_hw_map;
	ctx_custom->hw_acquired = true;
	ctx->ctxt_to_hw_map = param.ctxt_to_hw_map;

	CAM_DBG(CAM_CUSTOM,
		"Acquire HW success on session_hdl 0x%xs for ctx_id %u",
		ctx->session_hdl, ctx->ctx_id);

	kfree(acquire_hw_info);
	return rc;

free_res:
	kfree(acquire_hw_info);
end:
	return rc;
}

static int __cam_custom_ctx_acquire_dev_in_available(
	struct cam_context *ctx, struct cam_acquire_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_create_dev_hdl  req_hdl_param;

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_CUSTOM, "HW interface is not ready");
		rc = -EFAULT;
		return rc;
	}

	CAM_DBG(CAM_CUSTOM,
		"session_hdl 0x%x, num_resources %d, hdl type %d, res %lld",
		cmd->session_handle, cmd->num_resources,
		cmd->handle_type, cmd->resource_hdl);

	if (cmd->num_resources != CAM_API_COMPAT_CONSTANT) {
		CAM_ERR(CAM_CUSTOM, "Invalid num_resources 0x%x",
			cmd->num_resources);
		return -EINVAL;
	}

	req_hdl_param.session_hdl = cmd->session_handle;
	req_hdl_param.v4l2_sub_dev_flag = 0;
	req_hdl_param.media_entity_flag = 0;
	req_hdl_param.ops = ctx->crm_ctx_intf;
	req_hdl_param.priv = ctx;

	CAM_DBG(CAM_CUSTOM, "get device handle from bridge");
	ctx->dev_hdl = cam_create_device_hdl(&req_hdl_param);
	if (ctx->dev_hdl <= 0) {
		rc = -EFAULT;
		CAM_ERR(CAM_CUSTOM, "Can not create device handle");
		return rc;
	}

	cmd->dev_handle = ctx->dev_hdl;
	ctx->session_hdl = cmd->session_handle;
	ctx->state = CAM_CTX_ACQUIRED;

	CAM_DBG(CAM_CUSTOM,
		"Acquire dev success on session_hdl 0x%x for ctx %u",
		cmd->session_handle, ctx->ctx_id);

	return rc;
}

static int __cam_custom_ctx_enqueue_init_request(
	struct cam_context *ctx, struct cam_ctx_request *req)
{
	int rc = 0;
	struct cam_ctx_request           *req_old;
	struct cam_custom_dev_ctx_req    *req_custom_old;
	struct cam_custom_dev_ctx_req    *req_custom_new;

	spin_lock_bh(&ctx->lock);
	if (list_empty(&ctx->pending_req_list)) {
		list_add_tail(&req->list, &ctx->pending_req_list);
		goto end;
	}

	req_old = list_first_entry(&ctx->pending_req_list,
		struct cam_ctx_request, list);
	req_custom_old = (struct cam_custom_dev_ctx_req *) req_old->req_priv;
	req_custom_new = (struct cam_custom_dev_ctx_req *) req->req_priv;
	if (req_custom_old->hw_update_data.packet_opcode_type ==
		CAM_CUSTOM_PACKET_INIT_DEV) {
		if ((req_custom_old->num_cfg + req_custom_new->num_cfg) >=
			CAM_CUSTOM_CTX_CFG_MAX) {
			CAM_WARN(CAM_CUSTOM, "Can not merge INIT pkt");
			rc = -ENOMEM;
		}

		if (req_custom_old->num_fence_map_out != 0 ||
			req_custom_old->num_fence_map_in != 0) {
			CAM_WARN(CAM_CUSTOM, "Invalid INIT pkt sequence");
			rc = -EINVAL;
		}

		if (!rc) {
			memcpy(req_custom_old->fence_map_out,
				req_custom_new->fence_map_out,
				sizeof(req_custom_new->fence_map_out[0])*
				req_custom_new->num_fence_map_out);
			req_custom_old->num_fence_map_out =
				req_custom_new->num_fence_map_out;

			memcpy(req_custom_old->fence_map_in,
				req_custom_new->fence_map_in,
				sizeof(req_custom_new->fence_map_in[0])*
				req_custom_new->num_fence_map_in);
			req_custom_old->num_fence_map_in =
				req_custom_new->num_fence_map_in;

			memcpy(&req_custom_old->cfg[req_custom_old->num_cfg],
				req_custom_new->cfg,
				sizeof(req_custom_new->cfg[0])*
				req_custom_new->num_cfg);
			req_custom_old->num_cfg += req_custom_new->num_cfg;

			req_old->request_id = req->request_id;

			list_add_tail(&req->list, &ctx->free_req_list);
		}
	} else {
		CAM_WARN(CAM_CUSTOM,
			"Received Update pkt before INIT pkt. req_id= %lld",
			req->request_id);
		rc = -EINVAL;
	}
end:
	spin_unlock_bh(&ctx->lock);
	return rc;
}

static int __cam_custom_ctx_config_dev(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc = 0, i;
	struct cam_ctx_request           *req = NULL;
	struct cam_custom_dev_ctx_req    *req_custom;
	uintptr_t                         packet_addr;
	struct cam_packet                *packet;
	size_t                            len = 0;
	struct cam_hw_prepare_update_args cfg;
	struct cam_req_mgr_add_request    add_req;
	struct cam_custom_context        *ctx_custom =
		(struct cam_custom_context *) ctx->ctx_priv;

	/* get free request */
	spin_lock_bh(&ctx->lock);
	if (!list_empty(&ctx->free_req_list)) {
		req = list_first_entry(&ctx->free_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
	}
	spin_unlock_bh(&ctx->lock);

	if (!req) {
		CAM_ERR(CAM_CUSTOM, "No more request obj free");
		return -ENOMEM;
	}

	req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;

	/* for config dev, only memory handle is supported */
	/* map packet from the memhandle */
	rc = cam_mem_get_cpu_buf((int32_t) cmd->packet_handle,
		&packet_addr, &len);
	if (rc != 0) {
		CAM_ERR(CAM_CUSTOM, "Can not get packet address");
		rc = -EINVAL;
		goto free_req;
	}

	packet = (struct cam_packet *)(packet_addr + (uint32_t)cmd->offset);
	CAM_DBG(CAM_CUSTOM, "pack_handle %llx", cmd->packet_handle);
	CAM_DBG(CAM_CUSTOM, "packet address is 0x%zx", packet_addr);
	CAM_DBG(CAM_CUSTOM, "packet with length %zu, offset 0x%llx",
		len, cmd->offset);
	CAM_DBG(CAM_CUSTOM, "Packet request id %lld",
		packet->header.request_id);
	CAM_DBG(CAM_CUSTOM, "Packet size 0x%x", packet->header.size);
	CAM_DBG(CAM_CUSTOM, "packet op %d", packet->header.op_code);

	if ((((packet->header.op_code) & 0xF) ==
		CAM_CUSTOM_PACKET_UPDATE_DEV)
		&& (packet->header.request_id <= ctx->last_flush_req)) {
		CAM_DBG(CAM_CUSTOM,
			"request %lld has been flushed, reject packet",
			packet->header.request_id);
		rc = -EINVAL;
		goto free_req;
	}

	/* preprocess the configuration */
	memset(&cfg, 0, sizeof(cfg));
	cfg.packet = packet;
	cfg.ctxt_to_hw_map = ctx_custom->hw_ctx;
	cfg.out_map_entries = req_custom->fence_map_out;
	cfg.max_out_map_entries = CAM_CUSTOM_DEV_CTX_RES_MAX;
	cfg.in_map_entries = req_custom->fence_map_in;
	cfg.max_in_map_entries = CAM_CUSTOM_DEV_CTX_RES_MAX;
	cfg.priv  = &req_custom->hw_update_data;
	cfg.pf_data = &(req->pf_data);

	rc = ctx->hw_mgr_intf->hw_prepare_update(
		ctx->hw_mgr_intf->hw_mgr_priv, &cfg);
	if (rc != 0) {
		CAM_ERR(CAM_CUSTOM, "Prepare config packet failed in HW layer");
		rc = -EFAULT;
		goto free_req;
	}

	req_custom->num_cfg = cfg.num_hw_update_entries;
	req_custom->num_fence_map_out = cfg.num_out_map_entries;
	req_custom->num_fence_map_in = cfg.num_in_map_entries;
	req_custom->num_acked = 0;
	req_custom->hw_update_data.num_cfg = cfg.num_out_map_entries;

	for (i = 0; i < req_custom->num_fence_map_out; i++) {
		rc = cam_sync_get_obj_ref(req_custom->fence_map_out[i].sync_id);
		if (rc) {
			CAM_ERR(CAM_CUSTOM, "Can't get ref for fence %d",
				req_custom->fence_map_out[i].sync_id);
			goto put_ref;
		}
	}

	CAM_DBG(CAM_CUSTOM,
		"num_entry: %d, num fence out: %d, num fence in: %d",
		req_custom->num_cfg, req_custom->num_fence_map_out,
		req_custom->num_fence_map_in);

	req->request_id = packet->header.request_id;
	req->status = 1;

	CAM_DBG(CAM_CUSTOM, "Packet request id %lld packet opcode:%d",
		packet->header.request_id,
		req_custom->hw_update_data.packet_opcode_type);

	if (req_custom->hw_update_data.packet_opcode_type ==
		CAM_CUSTOM_PACKET_INIT_DEV) {
		if (ctx->state < CAM_CTX_ACTIVATED) {
			rc = __cam_custom_ctx_enqueue_init_request(ctx, req);
			if (rc)
				CAM_ERR(CAM_CUSTOM, "Enqueue INIT pkt failed");
			ctx_custom->init_received = true;
		} else {
			rc = -EINVAL;
			CAM_ERR(CAM_CUSTOM, "Recevied INIT pkt in wrong state");
		}
	} else {
		if ((ctx->state != CAM_CTX_FLUSHED) &&
			(ctx->state >= CAM_CTX_READY) &&
			(ctx->ctx_crm_intf->add_req)) {
			add_req.link_hdl = ctx->link_hdl;
			add_req.dev_hdl  = ctx->dev_hdl;
			add_req.req_id   = req->request_id;
			add_req.skip_before_applying = 0;
			rc = ctx->ctx_crm_intf->add_req(&add_req);
			if (rc) {
				CAM_ERR(CAM_CUSTOM,
					"Add req failed: req id=%llu",
					req->request_id);
			} else {
				__cam_custom_ctx_enqueue_request_in_order(
					ctx, req);
			}
		} else {
			rc = -EINVAL;
			CAM_ERR(CAM_CUSTOM, "Recevied Update in wrong state");
		}
	}

	if (rc)
		goto put_ref;

	CAM_DBG(CAM_CUSTOM,
		"Preprocessing Config req_id %lld successful on ctx %u",
		req->request_id, ctx->ctx_id);

	return rc;

put_ref:
	for (--i; i >= 0; i--) {
		if (cam_sync_put_obj_ref(req_custom->fence_map_out[i].sync_id))
			CAM_ERR(CAM_CUSTOM, "Failed to put ref of fence %d",
				req_custom->fence_map_out[i].sync_id);
	}
free_req:
	spin_lock_bh(&ctx->lock);
	list_add_tail(&req->list, &ctx->free_req_list);
	spin_unlock_bh(&ctx->lock);

	return rc;

}

static int __cam_custom_ctx_config_dev_in_flushed(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_start_stop_dev_cmd start_cmd;
	struct cam_custom_context *custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;

	if (!custom_ctx->hw_acquired) {
		CAM_ERR(CAM_CUSTOM, "HW is not acquired, reject packet");
		rc = -EINVAL;
		goto end;
	}

	rc = __cam_custom_ctx_config_dev(ctx, cmd);
	if (rc)
		goto end;

	if (!custom_ctx->init_received) {
		CAM_WARN(CAM_CUSTOM,
			"Received update packet in flushed state, skip start");
		goto end;
	}

	start_cmd.dev_handle = cmd->dev_handle;
	start_cmd.session_handle = cmd->session_handle;
	rc = __cam_custom_ctx_start_dev_in_ready(ctx, &start_cmd);
	if (rc)
		CAM_ERR(CAM_CUSTOM,
			"Failed to re-start HW after flush rc: %d", rc);
	else
		CAM_INFO(CAM_CUSTOM,
			"Received init after flush. Re-start HW complete.");

end:
	return rc;
}

static int __cam_custom_ctx_config_dev_in_acquired(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_custom_context        *ctx_custom =
		(struct cam_custom_context *) ctx->ctx_priv;

	if (!ctx_custom->hw_acquired) {
		CAM_ERR(CAM_CUSTOM, "HW not acquired, reject config packet");
		return -EAGAIN;
	}

	rc = __cam_custom_ctx_config_dev(ctx, cmd);

	if (!rc && (ctx->link_hdl >= 0))
		ctx->state = CAM_CTX_READY;

	return rc;
}

static int __cam_custom_ctx_link_in_acquired(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_custom_context *ctx_custom =
		(struct cam_custom_context *) ctx->ctx_priv;

	ctx->link_hdl = link->link_hdl;
	ctx->ctx_crm_intf = link->crm_cb;
	ctx_custom->subscribe_event = link->subscribe_event;

	/* change state only if we had the init config */
	if (ctx_custom->init_received)
		ctx->state = CAM_CTX_READY;

	CAM_DBG(CAM_CUSTOM, "next state %d", ctx->state);

	return 0;
}

static int __cam_custom_ctx_start_dev_in_ready(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_custom_start_args     custom_start;
	struct cam_ctx_request          *req;
	struct cam_custom_dev_ctx_req   *req_custom;
	struct cam_custom_context       *ctx_custom =
		(struct cam_custom_context *) ctx->ctx_priv;

	if (cmd->session_handle != ctx->session_hdl ||
		cmd->dev_handle != ctx->dev_hdl) {
		rc = -EPERM;
		goto end;
	}

	if (list_empty(&ctx->pending_req_list)) {
		/* should never happen */
		CAM_ERR(CAM_CUSTOM, "Start device with empty configuration");
		rc = -EFAULT;
		goto end;
	} else {
		req = list_first_entry(&ctx->pending_req_list,
			struct cam_ctx_request, list);
	}
	req_custom = (struct cam_custom_dev_ctx_req *) req->req_priv;

	if (!ctx_custom->hw_ctx) {
		CAM_ERR(CAM_CUSTOM, "Wrong hw context pointer.");
		rc = -EFAULT;
		goto end;
	}

	custom_start.hw_config.ctxt_to_hw_map = ctx_custom->hw_ctx;
	custom_start.hw_config.request_id = req->request_id;
	custom_start.hw_config.hw_update_entries = req_custom->cfg;
	custom_start.hw_config.num_hw_update_entries = req_custom->num_cfg;
	custom_start.hw_config.priv  = &req_custom->hw_update_data;
	custom_start.hw_config.init_packet = 1;
	if (ctx->state == CAM_CTX_FLUSHED)
		custom_start.start_only = true;
	else
		custom_start.start_only = false;

	ctx_custom->frame_id = 0;
	ctx_custom->active_req_cnt = 0;
	atomic_set(&ctx_custom->process_bubble, 0);
	ctx_custom->substate_activated =
		(req_custom->num_fence_map_out) ?
		CAM_CUSTOM_CTX_ACTIVATED_APPLIED :
		CAM_CUSTOM_CTX_ACTIVATED_SOF;

	ctx->state = CAM_CTX_ACTIVATED;
	rc = ctx->hw_mgr_intf->hw_start(ctx->hw_mgr_intf->hw_mgr_priv,
		&custom_start);
	if (rc) {
		/* HW failure. User need to clean up the resource */
		CAM_ERR(CAM_CUSTOM, "Start HW failed");
		ctx->state = CAM_CTX_READY;
		goto end;
	}

	CAM_DBG(CAM_CUSTOM, "start device success ctx %u",
		ctx->ctx_id);

	spin_lock_bh(&ctx->lock);
	list_del_init(&req->list);
	list_add_tail(&req->list, &ctx->wait_req_list);
	spin_unlock_bh(&ctx->lock);

end:
	return rc;
}

static int __cam_custom_ctx_release_dev_in_activated(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc = 0;

	rc = __cam_custom_stop_dev_core(ctx, NULL);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Stop device failed rc=%d", rc);

	rc = __cam_custom_release_dev_in_acquired(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Release device failed rc=%d", rc);

	return rc;
}

static int __cam_custom_ctx_unlink_in_activated(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	int rc = 0;

	CAM_WARN(CAM_CUSTOM,
		"Received unlink in activated state. It's unexpected");

	rc = __cam_custom_stop_dev_in_activated(ctx, NULL);
	if (rc)
		CAM_WARN(CAM_CUSTOM, "Stop device failed rc=%d", rc);

	rc = __cam_custom_ctx_unlink_in_ready(ctx, unlink);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Unlink failed rc=%d", rc);

	return rc;
}

static int __cam_custom_ctx_handle_irq_in_activated(void *context,
	uint32_t evt_id, void *evt_data)
{
	int rc = 0;
	struct cam_custom_ctx_irq_ops *custom_irq_ops = NULL;
	struct cam_context *ctx = (struct cam_context *)context;
	struct cam_custom_context *ctx_custom =
		(struct cam_custom_context *)ctx->ctx_priv;

	spin_lock(&ctx->lock);
	CAM_DBG(CAM_CUSTOM, "Enter: State %d, Substate %d, evt id %d",
		 ctx->state, ctx_custom->substate_activated, evt_id);
	custom_irq_ops = &ctx_custom->substate_machine_irq[
				ctx_custom->substate_activated];
	if (custom_irq_ops->irq_ops[evt_id])
		rc = custom_irq_ops->irq_ops[evt_id](ctx_custom,
			evt_data);
	else
		CAM_DBG(CAM_CUSTOM, "No handle function for substate %d",
			ctx_custom->substate_activated);

	CAM_DBG(CAM_CUSTOM, "Exit: State %d Substate %d",
		 ctx->state, ctx_custom->substate_activated);

	spin_unlock(&ctx->lock);
	return rc;
}

static int __cam_custom_ctx_acquire_hw_in_acquired(
	struct cam_context *ctx, void *args)
{
	int rc = -EINVAL;
	uint32_t api_version;

	if (!ctx || !args) {
		CAM_ERR(CAM_CUSTOM, "Invalid input pointer");
		return rc;
	}

	api_version = *((uint32_t *)args);
	if (api_version == 1)
		rc = __cam_custom_ctx_acquire_hw_v1(ctx, args);
	else
		CAM_ERR(CAM_CUSTOM, "Unsupported api version %d",
			api_version);

	return rc;
}

static int __cam_custom_ctx_apply_req(struct cam_context *ctx,
	struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_ctx_ops *ctx_ops = NULL;
	struct cam_custom_context *custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;

	CAM_DBG(CAM_CUSTOM,
		"Enter: apply req in Substate %d request _id:%lld",
		 custom_ctx->substate_activated, apply->request_id);

	ctx_ops = &custom_ctx->substate_machine[
		custom_ctx->substate_activated];
	if (ctx_ops->crm_ops.apply_req) {
		rc = ctx_ops->crm_ops.apply_req(ctx, apply);
	} else {
		CAM_WARN_RATE_LIMIT(CAM_CUSTOM,
			"No handle function in activated substate %d",
			custom_ctx->substate_activated);
		rc = -EFAULT;
	}

	if (rc)
		CAM_WARN_RATE_LIMIT(CAM_CUSTOM,
			"Apply failed in active substate %d rc %d",
			custom_ctx->substate_activated, rc);
	return rc;
}

static int __cam_custom_ctx_apply_default_req(
	struct cam_context *ctx,
	struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_ctx_ops *ctx_ops = NULL;
	struct cam_custom_context *custom_ctx =
		(struct cam_custom_context *) ctx->ctx_priv;

	CAM_DBG(CAM_CUSTOM,
		"Enter: apply req in Substate %d request _id:%lld",
		 custom_ctx->substate_activated, apply->request_id);

	ctx_ops = &custom_ctx->substate_machine[
		custom_ctx->substate_activated];
	if (ctx_ops->crm_ops.notify_frame_skip) {
		rc = ctx_ops->crm_ops.notify_frame_skip(ctx, apply);
	} else {
		CAM_WARN_RATE_LIMIT(CAM_CUSTOM,
			"No handle function in activated substate %d",
			custom_ctx->substate_activated);
		rc = -EFAULT;
	}

	if (rc)
		CAM_WARN_RATE_LIMIT(CAM_CUSTOM,
			"Apply default failed in active substate %d rc %d",
			custom_ctx->substate_activated, rc);
	return rc;
}

/* top state machine */
static struct cam_ctx_ops
	cam_custom_dev_ctx_top_state_machine[CAM_CTX_STATE_MAX] = {
	/* Uninit */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Available */
	{
		.ioctl_ops = {
			.acquire_dev =
				__cam_custom_ctx_acquire_dev_in_available,
		},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Acquired */
	{
		.ioctl_ops = {
			.acquire_hw = __cam_custom_ctx_acquire_hw_in_acquired,
			.release_dev = __cam_custom_release_dev_in_acquired,
			.config_dev = __cam_custom_ctx_config_dev_in_acquired,
			.release_hw = __cam_custom_ctx_release_hw_in_top_state,
		},
		.crm_ops = {
			.link = __cam_custom_ctx_link_in_acquired,
			.unlink = __cam_custom_ctx_unlink_in_acquired,
			.get_dev_info =
				__cam_custom_ctx_get_dev_info_in_acquired,
			.flush_req = __cam_custom_ctx_flush_req_in_top_state,
		},
		.irq_ops = NULL,
		.pagefault_ops = NULL,
	},
	/* Ready */
	{
		.ioctl_ops = {
			.start_dev = __cam_custom_ctx_start_dev_in_ready,
			.release_dev = __cam_custom_release_dev_in_acquired,
			.config_dev = __cam_custom_ctx_config_dev,
			.release_hw = __cam_custom_ctx_release_hw_in_top_state,
		},
		.crm_ops = {
			.unlink = __cam_custom_ctx_unlink_in_ready,
			.flush_req = __cam_custom_ctx_flush_req_in_ready,
		},
		.irq_ops = NULL,
		.pagefault_ops = NULL,
	},
	/* Flushed */
	{
		.ioctl_ops = {
			.stop_dev = __cam_custom_stop_dev_in_activated,
			.release_dev =
				__cam_custom_ctx_release_dev_in_activated,
			.config_dev = __cam_custom_ctx_config_dev_in_flushed,
			.release_hw =
				__cam_custom_ctx_release_hw_in_activated_state,
		},
		.crm_ops = {
			.unlink = __cam_custom_ctx_unlink_in_ready,
			.process_evt = __cam_custom_ctx_handle_evt,
		},
		.irq_ops = NULL,
	},
	/* Activated */
	{
		.ioctl_ops = {
			.stop_dev = __cam_custom_stop_dev_in_activated,
			.release_dev =
				__cam_custom_ctx_release_dev_in_activated,
			.config_dev = __cam_custom_ctx_config_dev,
			.release_hw =
				__cam_custom_ctx_release_hw_in_activated_state,
		},
		.crm_ops = {
			.unlink = __cam_custom_ctx_unlink_in_activated,
			.apply_req = __cam_custom_ctx_apply_req,
			.notify_frame_skip =
				__cam_custom_ctx_apply_default_req,
			.flush_req = __cam_custom_ctx_flush_req_in_top_state,
			.process_evt = __cam_custom_ctx_handle_evt,
		},
		.irq_ops = __cam_custom_ctx_handle_irq_in_activated,
		.pagefault_ops = NULL,
	},
};

int cam_custom_dev_context_init(struct cam_custom_context *ctx,
	struct cam_context *ctx_base,
	struct cam_req_mgr_kmd_ops *crm_node_intf,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id)
{
	int rc = -1, i = 0;

	if (!ctx || !ctx_base) {
		CAM_ERR(CAM_CUSTOM, "Invalid Context");
		return -EINVAL;
	}

	/* Custom HW context setup */
	memset(ctx, 0, sizeof(*ctx));

	ctx->base = ctx_base;
	ctx->frame_id = 0;
	ctx->active_req_cnt = 0;
	ctx->hw_ctx = NULL;

	for (i = 0; i < CAM_CTX_REQ_MAX; i++) {
		ctx->req_base[i].req_priv = &ctx->req_custom[i];
		ctx->req_custom[i].base = &ctx->req_base[i];
	}

	/* camera context setup */
	rc = cam_context_init(ctx_base, custom_dev_name, CAM_CUSTOM, ctx_id,
		crm_node_intf, hw_intf, ctx->req_base, CAM_CTX_REQ_MAX);
	if (rc) {
		CAM_ERR(CAM_CUSTOM, "Camera Context Base init failed");
		return rc;
	}

	/* link camera context with custom HW context */
	ctx_base->state_machine = cam_custom_dev_ctx_top_state_machine;
	ctx_base->ctx_priv = ctx;

	return rc;
}

int cam_custom_dev_context_deinit(struct cam_custom_context *ctx)
{
	if (ctx->base)
		cam_context_deinit(ctx->base);

	memset(ctx, 0, sizeof(*ctx));
	return 0;
}
