/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <media/cam_sync.h>
#include <media/cam_defs.h>

#include "cam_context.h"
#include "cam_context_utils.h"
#include "cam_mem_mgr.h"
#include "cam_node.h"
#include "cam_req_mgr_util.h"
#include "cam_sync_api.h"
#include "cam_trace.h"
#include "cam_debug_util.h"

static uint cam_debug_ctx_req_list;
module_param(cam_debug_ctx_req_list, uint, 0644);

static inline int cam_context_validate_thread(void)
{
	if (in_interrupt()) {
		WARN(1, "Invalid execution context\n");
		return -EINVAL;
	}
	return 0;
}

int cam_context_buf_done_from_hw(struct cam_context *ctx,
	void *done_event_data, uint32_t bubble_state)
{
	int j;
	int result;
	struct cam_ctx_request *req;
	struct cam_hw_done_event_data *done =
		(struct cam_hw_done_event_data *)done_event_data;
	int rc;

	if (!ctx || !done) {
		CAM_ERR(CAM_CTXT, "Invalid input params %pK %pK", ctx, done);
		return -EINVAL;
	}

	rc = cam_context_validate_thread();
	if (rc)
		return rc;

	spin_lock(&ctx->lock);
	if (list_empty(&ctx->active_req_list)) {
		CAM_ERR(CAM_CTXT, "[%s][%d] no active request",
			ctx->dev_name, ctx->ctx_id);
		spin_unlock(&ctx->lock);
		return -EIO;
	}
	req = list_first_entry(&ctx->active_req_list,
		struct cam_ctx_request, list);

	trace_cam_buf_done("UTILS", ctx, req);

	if (done->request_id != req->request_id) {
		CAM_ERR(CAM_CTXT,
			"[%s][%d] mismatch: done req[%lld], active req[%lld]",
			ctx->dev_name, ctx->ctx_id,
			done->request_id, req->request_id);
		spin_unlock(&ctx->lock);
		return -EIO;
	}

	if (!req->num_out_map_entries) {
		CAM_ERR(CAM_CTXT, "[%s][%d] no output fence to signal",
			ctx->dev_name, ctx->ctx_id);
		spin_unlock(&ctx->lock);
		return -EIO;
	}

	/*
	 * since another thread may be adding/removing from active
	 * list, so hold the lock
	 */
	list_del_init(&req->list);
	spin_unlock(&ctx->lock);
	if (!bubble_state) {
		result = CAM_SYNC_STATE_SIGNALED_SUCCESS;
	} else {
		CAM_DBG(CAM_REQ,
			"[%s][ctx_id %d] : req[%llu] is done with error",
			ctx->dev_name, ctx->ctx_id, req->request_id);

		for (j = 0; j < req->num_out_map_entries; j++)
			CAM_DBG(CAM_REQ, "fence %d signaled with error",
				req->out_map_entries[j].sync_id);

		result = CAM_SYNC_STATE_SIGNALED_ERROR;
	}

	for (j = 0; j < req->num_out_map_entries; j++) {
		cam_sync_signal(req->out_map_entries[j].sync_id, result);
		req->out_map_entries[j].sync_id = -1;
	}

	if (cam_debug_ctx_req_list & ctx->dev_id)
		CAM_INFO(CAM_CTXT,
			"[%s][%d] : Moving req[%llu] from active_list to free_list",
			ctx->dev_name, ctx->ctx_id, req->request_id);

	/*
	 * another thread may be adding/removing from free list,
	 * so hold the lock
	 */
	spin_lock(&ctx->lock);
	list_add_tail(&req->list, &ctx->free_req_list);
	req->ctx = NULL;
	spin_unlock(&ctx->lock);

	return 0;
}

static int cam_context_apply_req_to_hw(struct cam_ctx_request *req,
	struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_context *ctx = req->ctx;
	struct cam_hw_config_args cfg;

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_CTXT, "[%s][%d] HW interface is not ready",
			ctx->dev_name, ctx->ctx_id);
		rc = -EFAULT;
		goto end;
	}

	spin_lock(&ctx->lock);
	list_del_init(&req->list);
	list_add_tail(&req->list, &ctx->active_req_list);
	spin_unlock(&ctx->lock);

	if (cam_debug_ctx_req_list & ctx->dev_id)
		CAM_INFO(CAM_CTXT,
			"[%s][%d] : Moving req[%llu] from pending_list to active_list",
			ctx->dev_name, ctx->ctx_id, req->request_id);

	cfg.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
	cfg.request_id = req->request_id;
	cfg.hw_update_entries = req->hw_update_entries;
	cfg.num_hw_update_entries = req->num_hw_update_entries;
	cfg.out_map_entries = req->out_map_entries;
	cfg.num_out_map_entries = req->num_out_map_entries;
	cfg.priv = req->req_priv;

	rc = ctx->hw_mgr_intf->hw_config(ctx->hw_mgr_intf->hw_mgr_priv, &cfg);
	if (rc) {
		spin_lock(&ctx->lock);
		list_del_init(&req->list);
		list_add_tail(&req->list, &ctx->free_req_list);
		spin_unlock(&ctx->lock);

		if (cam_debug_ctx_req_list & ctx->dev_id)
			CAM_INFO(CAM_CTXT,
				"[%s][%d] : Moving req[%llu] from active_list to free_list",
				ctx->dev_name, ctx->ctx_id, req->request_id);
	}

end:
	return rc;
}

static void cam_context_sync_callback(int32_t sync_obj, int status, void *data)
{
	struct cam_ctx_request *req = data;
	struct cam_context *ctx = NULL;
	struct cam_flush_dev_cmd flush_cmd;
	struct cam_req_mgr_apply_request apply;
	int rc;

	if (!req) {
		CAM_ERR(CAM_CTXT, "Invalid input param");
		return;
	}
	rc = cam_context_validate_thread();
	if (rc)
		return;

	ctx = req->ctx;
	if (!ctx) {
		CAM_ERR(CAM_CTXT, "Invalid ctx for req %llu", req->request_id);
		return;
	}

	if (atomic_inc_return(&req->num_in_acked) == req->num_in_map_entries) {
		apply.request_id = req->request_id;
		/*
		 * take mutex to ensure that another thread does
		 * not flush the request while this
		 * thread is submitting it to h/w. The submit to
		 * h/w and adding to the active list should happen
		 * in a critical section which is provided by this
		 * mutex.
		 */
		if (status == CAM_SYNC_STATE_SIGNALED_ERROR) {
			CAM_DBG(CAM_CTXT, "fence error: %d", sync_obj);
			flush_cmd.req_id = req->request_id;
			cam_context_flush_req_to_hw(ctx, &flush_cmd);
		}

		mutex_lock(&ctx->sync_mutex);
		if (!req->flushed) {
			cam_context_apply_req_to_hw(req, &apply);
			mutex_unlock(&ctx->sync_mutex);
		} else {
			req->flushed = 0;
			req->ctx = NULL;
			mutex_unlock(&ctx->sync_mutex);
			spin_lock(&ctx->lock);
			list_del_init(&req->list);
			list_add_tail(&req->list, &ctx->free_req_list);
			spin_unlock(&ctx->lock);

			if (cam_debug_ctx_req_list & ctx->dev_id)
				CAM_INFO(CAM_CTXT,
					"[%s][%d] : Moving req[%llu] from pending_list to free_list",
					ctx->dev_name, ctx->ctx_id,
					req->request_id);
		}
	}
	cam_context_putref(ctx);
}

int32_t cam_context_release_dev_to_hw(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	struct cam_hw_release_args arg;

	if (!ctx) {
		CAM_ERR(CAM_CTXT, "Invalid input param");
		return -EINVAL;
	}

	if ((!ctx->hw_mgr_intf) || (!ctx->hw_mgr_intf->hw_release)) {
		CAM_ERR(CAM_CTXT, "[%s][%d] HW interface is not ready",
			ctx->dev_name, ctx->ctx_id);
		return -EINVAL;
	}

	arg.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
	arg.active_req = false;

	ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv, &arg);
	ctx->ctxt_to_hw_map = NULL;

	ctx->session_hdl = -1;
	ctx->dev_hdl = -1;
	ctx->link_hdl = -1;

	return 0;
}

int32_t cam_context_prepare_dev_to_hw(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_ctx_request *req = NULL;
	struct cam_hw_prepare_update_args cfg;
	uint64_t packet_addr;
	struct cam_packet *packet;
	size_t len = 0;
	int32_t i = 0;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CTXT, "Invalid input params %pK %pK", ctx, cmd);
		rc = -EINVAL;
		goto end;
	}

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_CTXT, "[%s][%d] HW interface is not ready",
			ctx->dev_name, ctx->ctx_id);
		rc = -EFAULT;
		goto end;
	}
	rc = cam_context_validate_thread();
	if (rc)
		return rc;

	spin_lock(&ctx->lock);
	if (!list_empty(&ctx->free_req_list)) {
		req = list_first_entry(&ctx->free_req_list,
			struct cam_ctx_request, list);
		list_del_init(&req->list);
	}
	spin_unlock(&ctx->lock);

	if (!req) {
		CAM_ERR(CAM_CTXT, "[%s][%d] No more request obj free",
			ctx->dev_name, ctx->ctx_id);
		rc = -ENOMEM;
		goto end;
	}

	memset(req, 0, sizeof(*req));
	INIT_LIST_HEAD(&req->list);
	req->ctx = ctx;

	/* for config dev, only memory handle is supported */
	/* map packet from the memhandle */
	rc = cam_mem_get_cpu_buf((int32_t) cmd->packet_handle,
		(uint64_t *) &packet_addr,
		&len);
	if (rc != 0) {
		CAM_ERR(CAM_CTXT, "[%s][%d] Can not get packet address",
			ctx->dev_name, ctx->ctx_id);
		rc = -EINVAL;
		goto free_req;
	}

	packet = (struct cam_packet *) (packet_addr + cmd->offset);

	/* preprocess the configuration */
	memset(&cfg, 0, sizeof(cfg));
	cfg.packet = packet;
	cfg.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
	cfg.max_hw_update_entries = CAM_CTX_CFG_MAX;
	cfg.num_hw_update_entries = req->num_hw_update_entries;
	cfg.hw_update_entries = req->hw_update_entries;
	cfg.max_out_map_entries = CAM_CTX_CFG_MAX;
	cfg.out_map_entries = req->out_map_entries;
	cfg.max_in_map_entries = CAM_CTX_CFG_MAX;
	cfg.in_map_entries = req->in_map_entries;

	rc = ctx->hw_mgr_intf->hw_prepare_update(
		ctx->hw_mgr_intf->hw_mgr_priv, &cfg);
	if (rc != 0) {
		CAM_ERR(CAM_CTXT,
			"[%s][%d] Prepare config packet failed in HW layer",
			ctx->dev_name, ctx->ctx_id);
		rc = -EFAULT;
		goto free_req;
	}
	req->num_hw_update_entries = cfg.num_hw_update_entries;
	req->num_out_map_entries = cfg.num_out_map_entries;
	req->num_in_map_entries = cfg.num_in_map_entries;
	atomic_set(&req->num_in_acked, 0);
	req->request_id = packet->header.request_id;
	req->status = 1;
	req->req_priv = cfg.priv;

	if (req->num_in_map_entries > 0) {
		spin_lock(&ctx->lock);
		list_add_tail(&req->list, &ctx->pending_req_list);
		spin_unlock(&ctx->lock);

		if (cam_debug_ctx_req_list & ctx->dev_id)
			CAM_INFO(CAM_CTXT,
				"[%s][%d] : Moving req[%llu] from free_list to pending_list",
				ctx->dev_name, ctx->ctx_id, req->request_id);

		for (i = 0; i < req->num_in_map_entries; i++) {
			cam_context_getref(ctx);
			rc = cam_sync_register_callback(
					cam_context_sync_callback,
					(void *)req,
					req->in_map_entries[i].sync_id);
			if (rc) {
				CAM_ERR(CAM_CTXT,
					"[%s][%d] Failed register fence cb: %d ret = %d",
					ctx->dev_name, ctx->ctx_id,
					req->in_map_entries[i].sync_id, rc);
				spin_lock(&ctx->lock);
				list_del_init(&req->list);
				spin_unlock(&ctx->lock);

				if (cam_debug_ctx_req_list & ctx->dev_id)
					CAM_INFO(CAM_CTXT,
						"[%s][%d] : Moving req[%llu] from pending_list to free_list",
						ctx->dev_name, ctx->ctx_id,
						req->request_id);

				cam_context_putref(ctx);

				goto free_req;
			}
			CAM_DBG(CAM_CTXT, "register in fence cb: %d ret = %d",
				req->in_map_entries[i].sync_id, rc);
		}
		goto end;
	}

	return rc;

free_req:
	spin_lock(&ctx->lock);
	list_add_tail(&req->list, &ctx->free_req_list);
	req->ctx = NULL;
	spin_unlock(&ctx->lock);
end:
	return rc;
}

int32_t cam_context_acquire_dev_to_hw(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;
	struct cam_hw_acquire_args param;
	struct cam_create_dev_hdl req_hdl_param;
	struct cam_hw_release_args release;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CTXT, "Invalid input params %pK %pK", ctx, cmd);
		rc = -EINVAL;
		goto end;
	}

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_CTXT, "[%s][%d] HW interface is not ready",
			ctx->dev_name, ctx->ctx_id);
		rc = -EFAULT;
		goto end;
	}

	CAM_DBG(CAM_CTXT, "ses hdl: %x, num_res: %d, type: %d, res: %lld",
		cmd->session_handle, cmd->num_resources, cmd->handle_type,
		cmd->resource_hdl);

	if (cmd->num_resources > CAM_CTX_RES_MAX) {
		CAM_ERR(CAM_CTXT, "[%s][%d] resource limit exceeded",
			ctx->dev_name, ctx->ctx_id);
		rc = -ENOMEM;
		goto end;
	}

	/* for now we only support user pointer */
	if (cmd->handle_type != 1)  {
		CAM_ERR(CAM_CTXT, "[%s][%d] Only user pointer is supported",
			ctx->dev_name, ctx->ctx_id);
		rc = -EINVAL;
		goto end;
	}

	/* fill in parameters */
	param.context_data = ctx;
	param.event_cb = ctx->irq_cb_intf;
	param.num_acq = cmd->num_resources;
	param.acquire_info = cmd->resource_hdl;

	/* call HW manager to reserve the resource */
	rc = ctx->hw_mgr_intf->hw_acquire(ctx->hw_mgr_intf->hw_mgr_priv,
		&param);
	if (rc != 0) {
		CAM_ERR(CAM_CTXT, "[%s][%d] Acquire device failed",
			ctx->dev_name, ctx->ctx_id);
		goto end;
	}

	ctx->ctxt_to_hw_map = param.ctxt_to_hw_map;

	/* if hw resource acquire successful, acquire dev handle */
	req_hdl_param.session_hdl = cmd->session_handle;
	/* bridge is not ready for these flags. so false for now */
	req_hdl_param.v4l2_sub_dev_flag = 0;
	req_hdl_param.media_entity_flag = 0;
	req_hdl_param.priv = ctx;
	req_hdl_param.ops = ctx->crm_ctx_intf;

	ctx->dev_hdl = cam_create_device_hdl(&req_hdl_param);
	if (ctx->dev_hdl <= 0) {
		rc = -EFAULT;
		CAM_ERR(CAM_CTXT, "[%s][%d] Can not create device handle",
			ctx->dev_name, ctx->ctx_id);
		goto free_hw;
	}
	cmd->dev_handle = ctx->dev_hdl;

	/* store session information */
	ctx->session_hdl = cmd->session_handle;

	return rc;

free_hw:
	release.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
	ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv, &release);
	ctx->ctxt_to_hw_map = NULL;
end:
	return rc;
}

int32_t cam_context_flush_ctx_to_hw(struct cam_context *ctx)
{
	struct cam_hw_flush_args flush_args;
	struct list_head temp_list;
	struct cam_ctx_request *req;
	uint32_t i;
	int rc = 0;
	bool free_req;

	CAM_DBG(CAM_CTXT, "[%s] E: NRT flush ctx", ctx->dev_name);

	/*
	 * flush pending requests, take the sync lock to synchronize with the
	 * sync callback thread so that the sync cb thread does not try to
	 * submit request to h/w while the request is being flushed
	 */
	mutex_lock(&ctx->sync_mutex);
	INIT_LIST_HEAD(&temp_list);
	spin_lock(&ctx->lock);
	list_splice_init(&ctx->pending_req_list, &temp_list);
	spin_unlock(&ctx->lock);

	if (cam_debug_ctx_req_list & ctx->dev_id)
		CAM_INFO(CAM_CTXT,
			"[%s][%d] : Moving all pending requests from pending_list to temp_list",
			ctx->dev_name, ctx->ctx_id);

	flush_args.num_req_pending = 0;
	while (true) {
		spin_lock(&ctx->lock);
		if (list_empty(&temp_list)) {
			spin_unlock(&ctx->lock);
			break;
		}

		req = list_first_entry(&temp_list,
				struct cam_ctx_request, list);

		list_del_init(&req->list);
		spin_unlock(&ctx->lock);
		req->flushed = 1;

		flush_args.flush_req_pending[flush_args.num_req_pending++] =
			req->req_priv;

		free_req = false;
		for (i = 0; i < req->num_in_map_entries; i++) {
			rc = cam_sync_deregister_callback(
				cam_context_sync_callback,
				(void *)req,
				req->in_map_entries[i].sync_id);
			if (!rc) {
				cam_context_putref(ctx);
				if (atomic_inc_return(&req->num_in_acked) ==
					req->num_in_map_entries)
					free_req = true;
			}
		}

		for (i = 0; i < req->num_out_map_entries; i++) {
			if (req->out_map_entries[i].sync_id != -1) {
				rc = cam_sync_signal(
					req->out_map_entries[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
				if (rc == -EALREADY) {
					CAM_ERR(CAM_CTXT,
					"Req: %llu already signalled, sync_id:%d",
					req->request_id,
					req->out_map_entries[i].sync_id);
					break;
				}
			}
		}

		/*
		 * If we have deregistered the last sync callback, req will
		 * not be put on the free list. So put it on the free list here
		 */
		if (free_req) {
			req->ctx = NULL;
			spin_lock(&ctx->lock);
			list_add_tail(&req->list, &ctx->free_req_list);
			spin_unlock(&ctx->lock);
		}

		if (cam_debug_ctx_req_list & ctx->dev_id)
			CAM_INFO(CAM_CTXT,
				"[%s][%d] : Deleting req[%llu] from temp_list",
				ctx->dev_name, ctx->ctx_id, req->request_id);
	}
	mutex_unlock(&ctx->sync_mutex);

	if (ctx->hw_mgr_intf->hw_flush) {
		flush_args.num_req_active = 0;
		spin_lock(&ctx->lock);
		list_for_each_entry(req, &ctx->active_req_list, list) {
			flush_args.flush_req_active[flush_args.num_req_active++]
				= req->req_priv;
		}
		spin_unlock(&ctx->lock);

		if (flush_args.num_req_pending || flush_args.num_req_active) {
			flush_args.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
			flush_args.flush_type = CAM_FLUSH_TYPE_ALL;
			ctx->hw_mgr_intf->hw_flush(
				ctx->hw_mgr_intf->hw_mgr_priv, &flush_args);
		}
	}

	INIT_LIST_HEAD(&temp_list);
	spin_lock(&ctx->lock);
	list_splice_init(&ctx->active_req_list, &temp_list);
	INIT_LIST_HEAD(&ctx->active_req_list);
	spin_unlock(&ctx->lock);

	if (cam_debug_ctx_req_list & ctx->dev_id)
		CAM_INFO(CAM_CTXT,
			"[%s][%d] : Moving all requests from active_list to temp_list",
			ctx->dev_name, ctx->ctx_id);

	while (true) {
		spin_lock(&ctx->lock);
		if (list_empty(&temp_list)) {
			spin_unlock(&ctx->lock);
			break;
		}
		req = list_first_entry(&temp_list,
			struct cam_ctx_request, list);
		list_del_init(&req->list);
		spin_unlock(&ctx->lock);

		for (i = 0; i < req->num_out_map_entries; i++) {
			if (req->out_map_entries[i].sync_id != -1) {
				rc = cam_sync_signal(
					req->out_map_entries[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
				if (rc == -EALREADY) {
					CAM_ERR(CAM_CTXT,
						"Req: %llu already signalled ctx: %pK dev_name: %s dev_handle: %d ctx_state: %d",
						req->request_id, req->ctx,
						req->ctx->dev_name,
						req->ctx->dev_hdl,
						req->ctx->state);
					break;
				}
			}
		}

		spin_lock(&ctx->lock);
		list_add_tail(&req->list, &ctx->free_req_list);
		spin_unlock(&ctx->lock);
		req->ctx = NULL;

		if (cam_debug_ctx_req_list & ctx->dev_id)
			CAM_INFO(CAM_CTXT,
				"[%s][%d] : Moving req[%llu] from temp_list to free_list",
				ctx->dev_name, ctx->ctx_id, req->request_id);
	}

	CAM_DBG(CAM_CTXT, "[%s] X: NRT flush ctx", ctx->dev_name);

	return 0;
}

int32_t cam_context_flush_req_to_hw(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{
	struct cam_ctx_request *req = NULL;
	struct cam_hw_flush_args flush_args;
	uint32_t i;
	int32_t sync_id = 0;
	int rc = 0;
	bool free_req = false;

	CAM_DBG(CAM_CTXT, "[%s] E: NRT flush req", ctx->dev_name);

	flush_args.num_req_pending = 0;
	flush_args.num_req_active = 0;
	mutex_lock(&ctx->sync_mutex);
	spin_lock(&ctx->lock);
	list_for_each_entry(req, &ctx->pending_req_list, list) {
		if (req->request_id != cmd->req_id)
			continue;

		if (cam_debug_ctx_req_list & ctx->dev_id)
			CAM_INFO(CAM_CTXT,
				"[%s][%d] : Deleting req[%llu] from pending_list",
				ctx->dev_name, ctx->ctx_id, req->request_id);

		list_del_init(&req->list);
		req->flushed = 1;

		flush_args.flush_req_pending[flush_args.num_req_pending++] =
			req->req_priv;
		break;
	}
	spin_unlock(&ctx->lock);
	mutex_unlock(&ctx->sync_mutex);

	if (ctx->hw_mgr_intf->hw_flush) {
		if (!flush_args.num_req_pending) {
			spin_lock(&ctx->lock);
			list_for_each_entry(req, &ctx->active_req_list, list) {
				if (req->request_id != cmd->req_id)
					continue;

				list_del_init(&req->list);

				flush_args.flush_req_active[
					flush_args.num_req_active++] =
					req->req_priv;
				break;
			}
			spin_unlock(&ctx->lock);
		}

		if (flush_args.num_req_pending || flush_args.num_req_active) {
			flush_args.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
			flush_args.flush_type = CAM_FLUSH_TYPE_REQ;
			ctx->hw_mgr_intf->hw_flush(
				ctx->hw_mgr_intf->hw_mgr_priv, &flush_args);
		}
	}

	if (req) {
		if (flush_args.num_req_pending) {
			for (i = 0; i < req->num_in_map_entries; i++) {
				rc = cam_sync_deregister_callback(
					cam_context_sync_callback,
					(void *)req,
					req->in_map_entries[i].sync_id);
				if (rc)
					continue;

				cam_context_putref(ctx);
				if (atomic_inc_return(&req->num_in_acked) ==
					req->num_in_map_entries)
					free_req = true;
			}
		}

		if (flush_args.num_req_pending || flush_args.num_req_active) {
			for (i = 0; i < req->num_out_map_entries; i++) {
				sync_id =
					req->out_map_entries[i].sync_id;
				if (sync_id != -1) {
					rc = cam_sync_signal(sync_id,
						CAM_SYNC_STATE_SIGNALED_ERROR);
					if (rc == -EALREADY) {
						CAM_ERR(CAM_CTXT,
						"Req: %llu already signalled, sync_id:%d",
						req->request_id, sync_id);
						break;
					}
				}
			}
			if (flush_args.num_req_active || free_req) {
				req->ctx = NULL;
				spin_lock(&ctx->lock);
				list_add_tail(&req->list, &ctx->free_req_list);
				spin_unlock(&ctx->lock);

				if (cam_debug_ctx_req_list & ctx->dev_id)
					CAM_INFO(CAM_CTXT,
						"[%s][%d] : Moving req[%llu] from %s to free_list",
						ctx->dev_name, ctx->ctx_id,
						req->request_id,
						flush_args.num_req_active ?
							"active_list" :
							"pending_list");
			}
		}
	}
	CAM_DBG(CAM_CTXT, "[%s] X: NRT flush req", ctx->dev_name);

	return 0;
}

int32_t cam_context_flush_dev_to_hw(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{

	int rc = 0;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CTXT, "Invalid input params %pK %pK", ctx, cmd);
		rc = -EINVAL;
		goto end;
	}

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_CTXT, "[%s][%d] HW interface is not ready",
			ctx->dev_name, ctx->ctx_id);
		rc = -EFAULT;
		goto end;
	}

	if (cmd->flush_type == CAM_FLUSH_TYPE_ALL)
		rc = cam_context_flush_ctx_to_hw(ctx);
	else if (cmd->flush_type == CAM_FLUSH_TYPE_REQ)
		rc = cam_context_flush_req_to_hw(ctx, cmd);
	else {
		rc = -EINVAL;
		CAM_ERR(CAM_CORE, "[%s][%d] Invalid flush type %d",
			ctx->dev_name, ctx->ctx_id, cmd->flush_type);
	}

end:
	return rc;
}

int32_t cam_context_start_dev_to_hw(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_hw_start_args arg;

	if (!ctx || !cmd) {
		CAM_ERR(CAM_CTXT, "Invalid input params %pK %pK", ctx, cmd);
		rc = -EINVAL;
		goto end;
	}

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_CTXT, "[%s][%d] HW interface is not ready",
			ctx->dev_name, ctx->ctx_id);
		rc = -EFAULT;
		goto end;
	}

	if ((cmd->session_handle != ctx->session_hdl) ||
		(cmd->dev_handle != ctx->dev_hdl)) {
		CAM_ERR(CAM_CTXT,
			"[%s][%d] Invalid session hdl[%d], dev_handle[%d]",
			ctx->dev_name, ctx->ctx_id,
			cmd->session_handle, cmd->dev_handle);
		rc = -EPERM;
		goto end;
	}

	if (ctx->hw_mgr_intf->hw_start) {
		arg.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
		rc = ctx->hw_mgr_intf->hw_start(ctx->hw_mgr_intf->hw_mgr_priv,
				&arg);
		if (rc) {
			/* HW failure. user need to clean up the resource */
			CAM_ERR(CAM_CTXT, "[%s][%d] Start HW failed",
				ctx->dev_name, ctx->ctx_id);
			goto end;
		}
	}

end:
	return rc;
}

int32_t cam_context_stop_dev_to_hw(struct cam_context *ctx)
{
	int rc = 0;
	struct cam_hw_stop_args stop;

	if (!ctx) {
		CAM_ERR(CAM_CTXT, "Invalid input param");
		rc = -EINVAL;
		goto end;
	}

	if (!ctx->hw_mgr_intf) {
		CAM_ERR(CAM_CTXT, "[%s][%d] HW interface is not ready",
			ctx->dev_name, ctx->ctx_id);
		rc = -EFAULT;
		goto end;
	}

	rc = cam_context_validate_thread();
	if (rc)
		goto end;

	rc = cam_context_flush_ctx_to_hw(ctx);
	if (rc)
		goto end;

	/* stop hw first */
	if (ctx->hw_mgr_intf->hw_stop) {
		stop.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
		ctx->hw_mgr_intf->hw_stop(ctx->hw_mgr_intf->hw_mgr_priv,
			&stop);
	}

end:
	return rc;
}
