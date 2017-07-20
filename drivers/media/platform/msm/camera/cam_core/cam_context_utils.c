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
 */

#define pr_fmt(fmt) "CTXT-UTILS %s:%d " fmt, __func__, __LINE__

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/cam_sync.h>
#include <media/cam_defs.h>

#include "cam_context.h"
#include "cam_mem_mgr.h"
#include "cam_node.h"
#include "cam_req_mgr_util.h"
#include "cam_sync_api.h"
#include "cam_trace.h"

int cam_context_buf_done_from_hw(struct cam_context *ctx,
	void *done_event_data, uint32_t bubble_state)
{
	int j;
	int result;
	struct cam_ctx_request *req;
	struct cam_hw_done_event_data *done =
		(struct cam_hw_done_event_data *)done_event_data;

	if (list_empty(&ctx->active_req_list)) {
		pr_err("Buf done with no active request\n");
		return -EIO;
	}

	req = list_first_entry(&ctx->active_req_list,
		struct cam_ctx_request, list);

	trace_cam_buf_done("UTILS", ctx, req);

	if (done->request_id != req->request_id) {
		pr_err("mismatch: done request [%lld], active request [%lld]\n",
			done->request_id, req->request_id);
		return -EIO;
	}

	if (!req->num_out_map_entries) {
		pr_err("active request with no output fence objects to signal\n");
		return -EIO;
	}

	list_del_init(&req->list);
	if (!bubble_state)
		result = CAM_SYNC_STATE_SIGNALED_SUCCESS;
	else
		result = CAM_SYNC_STATE_SIGNALED_ERROR;

	for (j = 0; j < req->num_out_map_entries; j++) {
		cam_sync_signal(req->out_map_entries[j].sync_id, result);
		req->out_map_entries[j].sync_id = -1;
	}

	list_add_tail(&req->list, &ctx->free_req_list);

	return 0;
}

int cam_context_apply_req_to_hw(struct cam_context *ctx,
	struct cam_req_mgr_apply_request *apply)
{
	int rc = 0;
	struct cam_ctx_request *req;
	struct cam_hw_config_args cfg;

	if (!ctx->hw_mgr_intf) {
		pr_err("HW interface is not ready\n");
		rc = -EFAULT;
		goto end;
	}

	if (list_empty(&ctx->pending_req_list)) {
		pr_err("No available request for Apply id %lld\n",
			apply->request_id);
		rc = -EFAULT;
		goto end;
	}

	spin_lock(&ctx->lock);
	req = list_first_entry(&ctx->pending_req_list,
		struct cam_ctx_request, list);
	list_del_init(&req->list);
	spin_unlock(&ctx->lock);

	cfg.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
	cfg.hw_update_entries = req->hw_update_entries;
	cfg.num_hw_update_entries = req->num_hw_update_entries;
	cfg.out_map_entries = req->out_map_entries;
	cfg.num_out_map_entries = req->num_out_map_entries;
	cfg.priv = (void *)&req->request_id;
	list_add_tail(&req->list, &ctx->active_req_list);

	rc = ctx->hw_mgr_intf->hw_config(ctx->hw_mgr_intf->hw_mgr_priv, &cfg);
	if (rc)
		list_del_init(&req->list);

end:
	return rc;
}

static void cam_context_sync_callback(int32_t sync_obj, int status, void *data)
{
	struct cam_context *ctx = data;
	struct cam_ctx_request *req = NULL;
	struct cam_req_mgr_apply_request apply;

	spin_lock(&ctx->lock);
	if (!list_empty(&ctx->pending_req_list))
		req = list_first_entry(&ctx->pending_req_list,
			struct cam_ctx_request, list);
	spin_unlock(&ctx->lock);

	if (!req) {
		pr_err("No more request obj free\n");
		return;
	}

	req->num_in_acked++;
	if (req->num_in_acked == req->num_in_map_entries) {
		apply.request_id = req->request_id;
		cam_context_apply_req_to_hw(ctx, &apply);
	}
}

int32_t cam_context_release_dev_to_hw(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int i;
	struct cam_hw_release_args arg;
	struct cam_ctx_request *req;

	if ((!ctx->hw_mgr_intf) || (!ctx->hw_mgr_intf->hw_release)) {
		pr_err("HW interface is not ready\n");
		return -EINVAL;
	}

	arg.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
	if ((list_empty(&ctx->active_req_list)) &&
		(list_empty(&ctx->pending_req_list)))
		arg.active_req = false;
	else
		arg.active_req = true;

	ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv, &arg);
	ctx->ctxt_to_hw_map = NULL;

	ctx->session_hdl = 0;
	ctx->dev_hdl = 0;
	ctx->link_hdl = 0;

	while (!list_empty(&ctx->active_req_list)) {
		req = list_first_entry(&ctx->active_req_list,
			struct cam_ctx_request, list);
		list_del_init(&req->list);
		pr_debug("signal fence in active list. fence num %d\n",
			req->num_out_map_entries);
		for (i = 0; i < req->num_out_map_entries; i++) {
			if (req->out_map_entries[i].sync_id > 0)
				cam_sync_signal(req->out_map_entries[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
		}
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	while (!list_empty(&ctx->pending_req_list)) {
		req = list_first_entry(&ctx->pending_req_list,
			struct cam_ctx_request, list);
		list_del_init(&req->list);
		for (i = 0; i < req->num_in_map_entries; i++)
			if (req->in_map_entries[i].sync_id > 0)
				cam_sync_deregister_callback(
					cam_context_sync_callback, ctx,
					req->in_map_entries[i].sync_id);
		pr_debug("signal out fence in pending list. fence num %d\n",
			req->num_out_map_entries);
		for (i = 0; i < req->num_out_map_entries; i++)
			if (req->out_map_entries[i].sync_id > 0)
				cam_sync_signal(req->out_map_entries[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
		list_add_tail(&req->list, &ctx->free_req_list);
	}

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

	if (!ctx->hw_mgr_intf) {
		pr_err("HW interface is not ready\n");
		rc = -EFAULT;
		goto end;
	}

	spin_lock(&ctx->lock);
	if (!list_empty(&ctx->free_req_list)) {
		req = list_first_entry(&ctx->free_req_list,
			struct cam_ctx_request, list);
		list_del_init(&req->list);
	}
	spin_unlock(&ctx->lock);

	if (!req) {
		pr_err("No more request obj free\n");
		rc = -ENOMEM;
		goto end;
	}

	memset(req, 0, sizeof(*req));
	INIT_LIST_HEAD(&req->list);

	/* for config dev, only memory handle is supported */
	/* map packet from the memhandle */
	rc = cam_mem_get_cpu_buf((int32_t) cmd->packet_handle,
		(uint64_t *) &packet_addr,
		&len);
	if (rc != 0) {
		pr_err("Can not get packet address\n");
		rc = -EINVAL;
		goto free_req;
	}

	packet = (struct cam_packet *) (packet_addr + cmd->offset);
	pr_debug("pack_handle %llx\n", cmd->packet_handle);
	pr_debug("packet address is 0x%llx\n", packet_addr);
	pr_debug("packet with length %zu, offset 0x%llx\n",
		len, cmd->offset);
	pr_debug("Packet request id 0x%llx\n",
		packet->header.request_id);
	pr_debug("Packet size 0x%x\n", packet->header.size);
	pr_debug("packet op %d\n", packet->header.op_code);

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
		pr_err("Prepare config packet failed in HW layer\n");
		rc = -EFAULT;
		goto free_req;
	}
	req->num_hw_update_entries = cfg.num_hw_update_entries;
	req->num_out_map_entries = cfg.num_out_map_entries;
	req->num_in_map_entries = cfg.num_in_map_entries;
	req->request_id = packet->header.request_id;
	req->status = 1;
	req->req_priv = cfg.priv;

	if (req->num_in_map_entries > 0) {
		spin_lock(&ctx->lock);
		list_add_tail(&req->list, &ctx->pending_req_list);
		spin_unlock(&ctx->lock);
		for (i = 0; i < req->num_in_map_entries; i++) {
			rc = cam_sync_register_callback(
					cam_context_sync_callback,
					(void *)ctx,
					req->in_map_entries[i].sync_id);
			pr_debug("register in fence callback: %d ret = %d\n",
				req->in_map_entries[i].sync_id, rc);
		}
		goto end;
	}

	return rc;

free_req:
	spin_lock(&ctx->lock);
	list_add_tail(&req->list, &ctx->free_req_list);
	spin_unlock(&ctx->lock);
end:
	pr_debug("Config dev successful\n");
	return rc;
}

int32_t cam_context_acquire_dev_to_hw(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;
	struct cam_hw_acquire_args param;
	struct cam_create_dev_hdl req_hdl_param;
	struct cam_hw_release_args release;

	if (!ctx->hw_mgr_intf) {
		pr_err("HW interface is not ready\n");
		rc = -EFAULT;
		goto end;
	}

	pr_debug("acquire cmd: session_hdl 0x%x, num_resources %d\n",
		cmd->session_handle, cmd->num_resources);
	pr_debug(" handle type %d, res %lld\n", cmd->handle_type,
		cmd->resource_hdl);

	if (cmd->num_resources > CAM_CTX_RES_MAX) {
		pr_err("Too much resources in the acquire\n");
		rc = -ENOMEM;
		goto end;
	}

	/* for now we only support user pointer */
	if (cmd->handle_type != 1)  {
		pr_err("Only user pointer is supported");
		rc = -EINVAL;
		goto end;
	}

	/* fill in parameters */
	param.context_data = ctx;
	param.event_cb = ctx->irq_cb_intf;
	param.num_acq = cmd->num_resources;
	param.acquire_info = cmd->resource_hdl;

	pr_debug("ctx %pK: acquire hw resource: hw_intf: 0x%pK, priv 0x%pK",
		ctx, ctx->hw_mgr_intf, ctx->hw_mgr_intf->hw_mgr_priv);
	pr_debug("acquire_hw_func 0x%pK\n", ctx->hw_mgr_intf->hw_acquire);

	/* call HW manager to reserve the resource */
	rc = ctx->hw_mgr_intf->hw_acquire(ctx->hw_mgr_intf->hw_mgr_priv,
		&param);
	if (rc != 0) {
		pr_err("Acquire device failed\n");
		goto end;
	}

	ctx->ctxt_to_hw_map = param.ctxt_to_hw_map;

	/* if hw resource acquire successful, acquire dev handle */
	req_hdl_param.session_hdl = cmd->session_handle;
	/* bridge is not ready for these flags. so false for now */
	req_hdl_param.v4l2_sub_dev_flag = 0;
	req_hdl_param.media_entity_flag = 0;
	req_hdl_param.priv = ctx;

	pr_debug("get device handle from bridge\n");
	ctx->dev_hdl = cam_create_device_hdl(&req_hdl_param);
	if (ctx->dev_hdl <= 0) {
		rc = -EFAULT;
		pr_err("Can not create device handle\n");
		goto free_hw;
	}
	cmd->dev_handle = ctx->dev_hdl;

	/* store session information */
	ctx->session_hdl = cmd->session_handle;

	pr_err("dev_handle = %x\n", cmd->dev_handle);
	return rc;

free_hw:
	release.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
	ctx->hw_mgr_intf->hw_release(ctx->hw_mgr_intf->hw_mgr_priv, &release);
	ctx->ctxt_to_hw_map = NULL;
end:
	return rc;
}

int32_t cam_context_start_dev_to_hw(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;
	struct cam_hw_start_args arg;

	if (!ctx->hw_mgr_intf) {
		pr_err("HW interface is not ready\n");
		rc = -EFAULT;
		goto end;
	}

	if ((cmd->session_handle != ctx->session_hdl) ||
		(cmd->dev_handle != ctx->dev_hdl)) {
		pr_err("Invalid session hdl[%d], dev_handle[%d]\n",
			cmd->session_handle, cmd->dev_handle);
		rc = -EPERM;
		goto end;
	}

	if (ctx->hw_mgr_intf->hw_start) {
		rc = ctx->hw_mgr_intf->hw_start(ctx->hw_mgr_intf->hw_mgr_priv,
				&arg);
		if (rc) {
			/* HW failure. user need to clean up the resource */
			pr_err("Start HW failed\n");
			goto end;
		}
	}

	pr_debug("start device success\n");
end:
	return rc;
}

int32_t cam_context_stop_dev_to_hw(struct cam_context *ctx)
{
	int rc = 0;
	uint32_t i;
	struct cam_hw_stop_args stop;
	struct cam_ctx_request *req;

	if (!ctx->hw_mgr_intf) {
		pr_err("HW interface is not ready\n");
		rc = -EFAULT;
		goto end;
	}

	/* stop hw first */
	if (ctx->ctxt_to_hw_map) {
		stop.ctxt_to_hw_map = ctx->ctxt_to_hw_map;
		if (ctx->hw_mgr_intf->hw_stop)
			ctx->hw_mgr_intf->hw_stop(ctx->hw_mgr_intf->hw_mgr_priv,
				&stop);
	}

	/* flush pending and active queue */
	while (!list_empty(&ctx->pending_req_list)) {
		req = list_first_entry(&ctx->pending_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		pr_debug("signal fence in pending list. fence num %d\n",
			req->num_out_map_entries);
		for (i = 0; i < req->num_out_map_entries; i++)
			if (req->out_map_entries[i].sync_id != -1)
				cam_sync_signal(req->out_map_entries[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
		list_add_tail(&req->list, &ctx->free_req_list);
	}

	while (!list_empty(&ctx->active_req_list)) {
		req = list_first_entry(&ctx->active_req_list,
				struct cam_ctx_request, list);
		list_del_init(&req->list);
		pr_debug("signal fence in active list. fence num %d\n",
			req->num_out_map_entries);
		for (i = 0; i < req->num_out_map_entries; i++)
			if (req->out_map_entries[i].sync_id != -1)
				cam_sync_signal(req->out_map_entries[i].sync_id,
					CAM_SYNC_STATE_SIGNALED_ERROR);
		list_add_tail(&req->list, &ctx->free_req_list);
	}

end:
	return rc;
}

