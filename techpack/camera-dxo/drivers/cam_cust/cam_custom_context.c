// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

static const char custom_dev_name[] = "custom hw";

static int __cam_custom_ctx_handle_irq_in_activated(
	void *context, uint32_t evt_id, void *evt_data);

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
					CAM_SYNC_STATE_SIGNALED_ERROR);
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

static int __cam_custom_ctx_flush_req_in_top_state(
	struct cam_context *ctx,
	struct cam_req_mgr_flush_request *flush_req)
{
	int rc = 0;

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		CAM_INFO(CAM_CUSTOM, "Last request id to flush is %lld",
			flush_req->req_id);
		ctx->last_flush_req = flush_req->req_id;
	}

	spin_lock_bh(&ctx->lock);
	rc = __cam_custom_ctx_flush_req(ctx, &ctx->pending_req_list, flush_req);
	spin_unlock_bh(&ctx->lock);

	return rc;
}

static int __cam_custom_ctx_flush_req_in_ready(
	struct cam_context *ctx,
	struct cam_req_mgr_flush_request *flush_req)
{
	int rc = 0;

	CAM_DBG(CAM_CUSTOM, "try to flush pending list");
	spin_lock_bh(&ctx->lock);
	rc = __cam_custom_ctx_flush_req(ctx, &ctx->pending_req_list, flush_req);

	/* if nothing is in pending req list, change state to acquire */
	if (list_empty(&ctx->pending_req_list))
		ctx->state = CAM_CTX_ACQUIRED;
	spin_unlock_bh(&ctx->lock);

	CAM_DBG(CAM_CUSTOM, "Flush request in ready state. next state %d",
		 ctx->state);
	return rc;
}

static int __cam_custom_ctx_unlink_in_ready(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	ctx->link_hdl = -1;
	ctx->ctx_crm_intf = NULL;
	ctx->state = CAM_CTX_ACQUIRED;

	return 0;
}

static int __cam_custom_stop_dev_core(
	struct cam_context *ctx, struct cam_start_stop_dev_cmd *stop_cmd)
{
	int rc = 0;
	uint32_t i;
	struct cam_custom_context          *ctx_custom =
		(struct cam_custom_context *) ctx->ctx_priv;
	struct cam_ctx_request          *req;
	struct cam_custom_dev_ctx_req       *req_custom;
	struct cam_hw_stop_args          stop;

	if (ctx_custom->hw_ctx) {
		stop.ctxt_to_hw_map = ctx_custom->hw_ctx;

		stop.args = NULL;
		if (ctx->hw_mgr_intf->hw_stop)
			ctx->hw_mgr_intf->hw_stop(ctx->hw_mgr_intf->hw_mgr_priv,
			&stop);
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
					CAM_SYNC_STATE_SIGNALED_ERROR);
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
					CAM_SYNC_STATE_SIGNALED_ERROR);
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
					CAM_SYNC_STATE_SIGNALED_ERROR);
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

static int __cam_custom_release_dev_in_acquired(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;
	struct cam_custom_context *ctx_custom =
		(struct cam_custom_context *) ctx->ctx_priv;
	struct cam_req_mgr_flush_request flush_req;

	rc = cam_context_release_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Unable to release device");

	ctx->ctx_crm_intf = NULL;
	ctx->last_flush_req = 0;
	ctx_custom->frame_id = 0;
	ctx_custom->active_req_cnt = 0;
	ctx_custom->init_received = false;

	if (!list_empty(&ctx->active_req_list))
		CAM_ERR(CAM_CUSTOM, "Active list is not empty");

	/* Flush all the pending request list  */
	flush_req.type = CAM_REQ_MGR_FLUSH_TYPE_ALL;
	flush_req.link_hdl = ctx->link_hdl;
	flush_req.dev_hdl = ctx->dev_hdl;

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

static int __cam_custom_ctx_apply_req_in_activated_state(
	struct cam_context *ctx, struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_ctx_request          *req;
	struct cam_custom_dev_ctx_req   *req_custom;
	struct cam_custom_context       *custom_ctx = NULL;
	struct cam_hw_config_args        cfg;

	if (list_empty(&ctx->pending_req_list)) {
		CAM_ERR(CAM_CUSTOM, "No available request for Apply id %lld",
			apply->request_id);
		rc = -EFAULT;
		goto end;
	}

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
		list_del_init(&req->list);
		if (!req->num_out_map_entries) {
			list_add_tail(&req->list, &ctx->free_req_list);
			spin_unlock_bh(&ctx->lock);
		} else {
			list_add_tail(&req->list, &ctx->active_req_list);
			spin_unlock_bh(&ctx->lock);
			/*
			 * for test purposes only-this should be
			 * triggered based on irq
			 */
			 __cam_custom_ctx_handle_irq_in_activated(ctx, 0, NULL);
		}
	}

end:
	return rc;
}

static int __cam_custom_ctx_acquire_dev_in_available(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;
	struct cam_custom_context *custom_ctx;

	custom_ctx = (struct cam_custom_context *) ctx->ctx_priv;

	if (cmd->num_resources > CAM_CUSTOM_DEV_CTX_RES_MAX) {
		CAM_ERR(CAM_CUSTOM, "Too much resources in the acquire");
		rc = -ENOMEM;
		return rc;
	}

	if (cmd->handle_type != 1)	{
		CAM_ERR(CAM_CUSTOM, "Only user pointer is supported");
		rc = -EINVAL;
		return rc;
	}

	rc = cam_context_acquire_dev_to_hw(ctx, cmd);
	if (!rc) {
		ctx->state = CAM_CTX_ACQUIRED;
		custom_ctx->hw_ctx = ctx->ctxt_to_hw_map;
	}

	CAM_DBG(CAM_CUSTOM, "Acquire done %d", ctx->ctx_id);
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
	cfg.in_map_entries = req_custom->fence_map_in;
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
		if (ctx->state >= CAM_CTX_READY && ctx->ctx_crm_intf->add_req) {
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

static int __cam_custom_ctx_config_dev_in_acquired(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc = 0;

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

static int __cam_custom_ctx_unlink_in_acquired(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	ctx->link_hdl = -1;
	ctx->ctx_crm_intf = NULL;

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

static int __cam_custom_ctx_start_dev_in_ready(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_hw_config_args        hw_config;
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

	hw_config.ctxt_to_hw_map = ctx_custom->hw_ctx;
	hw_config.request_id = req->request_id;
	hw_config.hw_update_entries = req_custom->cfg;
	hw_config.num_hw_update_entries = req_custom->num_cfg;
	hw_config.priv  = &req_custom->hw_update_data;
	hw_config.init_packet = 1;

	ctx->state = CAM_CTX_ACTIVATED;
	rc = ctx->hw_mgr_intf->hw_start(ctx->hw_mgr_intf->hw_mgr_priv,
		&hw_config);
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
	if (req_custom->num_fence_map_out)
		list_add_tail(&req->list, &ctx->active_req_list);
	else
		list_add_tail(&req->list, &ctx->free_req_list);
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

static int __cam_custom_ctx_process_evt(struct cam_context *ctx,
	struct cam_req_mgr_link_evt_data *link_evt_data)
{
	switch (link_evt_data->evt_type) {
	case CAM_REQ_MGR_LINK_EVT_ERR:
		/* Handle error/bubble related issues */
		break;
	default:
		CAM_WARN(CAM_CUSTOM, "Unknown event from CRM");
		break;
	}

	return 0;
}

static int __cam_custom_ctx_handle_irq_in_activated(void *context,
	uint32_t evt_id, void *evt_data)
{
	int rc;
	struct cam_context *ctx =
		(struct cam_context *)context;

	CAM_DBG(CAM_CUSTOM, "Enter %d", ctx->ctx_id);

	/*
	 * handle based on different irq's currently
	 * triggering only buf done if there are fences
	 */
	rc = cam_context_buf_done_from_hw(ctx, evt_data, 0);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Failed in buf done, rc=%d", rc);

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
			.release_dev = __cam_custom_release_dev_in_acquired,
			.config_dev = __cam_custom_ctx_config_dev_in_acquired,
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
		},
		.crm_ops = {
			.unlink = __cam_custom_ctx_unlink_in_ready,
			.flush_req = __cam_custom_ctx_flush_req_in_ready,
		},
		.irq_ops = NULL,
		.pagefault_ops = NULL,
	},
	/* Flushed */
	{},
	/* Activated */
	{
		.ioctl_ops = {
			.stop_dev = __cam_custom_stop_dev_in_activated,
			.release_dev =
				__cam_custom_ctx_release_dev_in_activated,
			.config_dev = __cam_custom_ctx_config_dev,
		},
		.crm_ops = {
			.unlink = __cam_custom_ctx_unlink_in_activated,
			.apply_req =
				__cam_custom_ctx_apply_req_in_activated_state,
			.flush_req = __cam_custom_ctx_flush_req_in_top_state,
			.process_evt = __cam_custom_ctx_process_evt,
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
