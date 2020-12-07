// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/refcount.h>

#include "cam_context.h"
#include "cam_debug_util.h"
#include "cam_node.h"

static int cam_context_handle_hw_event(void *context, uint32_t evt_id,
	void *evt_data)
{
	int rc = 0;
	struct cam_context *ctx = (struct cam_context *)context;

	if (!ctx || !ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (ctx->state_machine[ctx->state].irq_ops)
		rc = ctx->state_machine[ctx->state].irq_ops(ctx, evt_id,
			evt_data);
	else
		CAM_DBG(CAM_CORE,
			"No function to handle event %d in dev %d, state %d",
			evt_id, ctx->dev_hdl, ctx->state);
	return rc;
}

int cam_context_shutdown(struct cam_context *ctx)
{
	int rc = 0;
	struct cam_release_dev_cmd cmd;

	if (ctx->state > CAM_CTX_AVAILABLE && ctx->state < CAM_CTX_STATE_MAX) {
		cmd.session_handle = ctx->session_hdl;
		cmd.dev_handle = ctx->dev_hdl;
		rc = cam_context_handle_release_dev(ctx, &cmd);
		if (rc)
			CAM_ERR(CAM_CORE,
				"context release failed for dev_name %s",
				ctx->dev_name);
		else
			cam_context_putref(ctx);
	} else {
		CAM_WARN(CAM_CORE,
			"dev %s context id %u state %d invalid to release hdl",
			ctx->dev_name, ctx->ctx_id, ctx->state);
		rc = -EINVAL;
	}

	if (ctx->dev_hdl != -1) {
		rc = cam_destroy_device_hdl(ctx->dev_hdl);
		if (rc)
			CAM_ERR(CAM_CORE,
				"destroy device hdl failed for node %s",
				ctx->dev_name);
		else
			ctx->dev_hdl = -1;
	}

	return rc;
}

int cam_context_handle_crm_get_dev_info(struct cam_context *ctx,
	struct cam_req_mgr_device_info *info)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!info) {
		CAM_ERR(CAM_CORE, "Invalid get device info payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.get_dev_info) {
		rc = ctx->state_machine[ctx->state].crm_ops.get_dev_info(
			ctx, info);
	} else {
		CAM_ERR(CAM_CORE, "No get device info in dev %d, state %d",
			ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_crm_link(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *link)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!link) {
		CAM_ERR(CAM_CORE, "Invalid link payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.link) {
		rc = ctx->state_machine[ctx->state].crm_ops.link(ctx, link);
	} else {
		CAM_ERR(CAM_CORE, "No crm link in dev %d, state %d",
			ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_crm_unlink(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!unlink) {
		CAM_ERR(CAM_CORE, "Invalid unlink payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.unlink) {
		rc = ctx->state_machine[ctx->state].crm_ops.unlink(
			ctx, unlink);
	} else {
		CAM_ERR(CAM_CORE, "No crm unlink in dev %d, name %s, state %d",
			ctx->dev_hdl, ctx->dev_name, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_crm_apply_req(struct cam_context *ctx,
	struct cam_req_mgr_apply_request *apply)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!apply) {
		CAM_ERR(CAM_CORE, "Invalid apply request payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.apply_req) {
		rc = ctx->state_machine[ctx->state].crm_ops.apply_req(ctx,
			apply);
	} else {
		CAM_ERR(CAM_CORE, "No crm apply req in dev %d, state %d",
			ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_crm_flush_req(struct cam_context *ctx,
	struct cam_req_mgr_flush_request *flush)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.flush_req) {
		rc = ctx->state_machine[ctx->state].crm_ops.flush_req(ctx,
			flush);
	} else {
		CAM_ERR(CAM_CORE, "No crm flush req in dev %d, state %d",
			ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_crm_process_evt(struct cam_context *ctx,
	struct cam_req_mgr_link_evt_data *process_evt)
{
	int rc = 0;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.process_evt) {
		rc = ctx->state_machine[ctx->state].crm_ops.process_evt(ctx,
			process_evt);
	} else {
		/* handling of this message is optional */
		CAM_DBG(CAM_CORE, "No crm process evt in dev %d, state %d",
			ctx->dev_hdl, ctx->state);
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_crm_dump_req(struct cam_context *ctx,
	struct cam_req_mgr_dump_info *dump)
{
	int rc = 0;

	if (!ctx) {
		CAM_ERR(CAM_CORE, "Invalid Context");
		return -EINVAL;
	}
	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context %s ctx_id %d is not ready",
			ctx->dev_name, ctx->ctx_id);
		return -EINVAL;
	}
	mutex_lock(&ctx->ctx_mutex);

	if (ctx->state_machine[ctx->state].crm_ops.dump_req)
		rc = ctx->state_machine[ctx->state].crm_ops.dump_req(ctx,
			dump);
	else
		CAM_ERR(CAM_CORE, "No crm dump req for %s dev %d, state %d",
			ctx->dev_name, ctx->dev_hdl, ctx->state);

	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_dump_pf_info(struct cam_context *ctx, unsigned long iova,
	uint32_t buf_info)
{
	int rc = 0;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if ((ctx->state > CAM_CTX_AVAILABLE) &&
		(ctx->state < CAM_CTX_STATE_MAX)) {
		if (ctx->state_machine[ctx->state].pagefault_ops) {
			rc = ctx->state_machine[ctx->state].pagefault_ops(
				ctx, iova, buf_info);
		} else {
			CAM_WARN(CAM_CORE, "No dump ctx in dev %d, state %d",
				ctx->dev_hdl, ctx->state);
		}
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_acquire_dev(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;
	int i;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_CORE, "Invalid acquire device command payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.acquire_dev) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.acquire_dev(
			ctx, cmd);
	} else {
		CAM_ERR(CAM_CORE, "No acquire device in dev %d, state %d",
			cmd->dev_handle, ctx->state);
		rc = -EPROTO;
	}

	INIT_LIST_HEAD(&ctx->active_req_list);
	INIT_LIST_HEAD(&ctx->wait_req_list);
	INIT_LIST_HEAD(&ctx->pending_req_list);
	INIT_LIST_HEAD(&ctx->free_req_list);

	for (i = 0; i < ctx->req_size; i++) {
		INIT_LIST_HEAD(&ctx->req_list[i].list);
		list_add_tail(&ctx->req_list[i].list, &ctx->free_req_list);
	}

	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_acquire_hw(struct cam_context *ctx,
	void *args)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!args) {
		CAM_ERR(CAM_CORE, "Invalid acquire device hw command payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.acquire_hw) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.acquire_hw(
			ctx, args);
	} else {
		CAM_ERR(CAM_CORE, "No acquire hw for dev %s, state %d",
			ctx->dev_name, ctx->state);
		rc = -EPROTO;
	}

	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_release_dev(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_CORE, "Invalid release device command payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.release_dev) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.release_dev(
			ctx, cmd);
	} else {
		CAM_ERR(CAM_CORE, "No release device in dev %d, state %d",
			ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_release_hw(struct cam_context *ctx,
	void *args)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!args) {
		CAM_ERR(CAM_CORE, "Invalid release HW command payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.release_hw) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.release_hw(
			ctx, args);
	} else {
		CAM_ERR(CAM_CORE, "No release hw for dev %s, state %d",
			ctx->dev_name, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_flush_dev(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{
	int rc = 0;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_CORE, "Invalid flush device command payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.flush_dev) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.flush_dev(
			ctx, cmd);
	} else {
		CAM_WARN(CAM_CORE, "No flush device in dev %d, state %d",
			ctx->dev_hdl, ctx->state);
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_config_dev(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc;

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "context is not ready");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_CORE, "Invalid config device command payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.config_dev) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.config_dev(
			ctx, cmd);
	} else {
		CAM_ERR(CAM_CORE, "No config device in dev %d, state %d",
			ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_start_dev(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;

	if (!ctx || !ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_CORE, "Invalid start device command payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.start_dev)
		rc = ctx->state_machine[ctx->state].ioctl_ops.start_dev(
			ctx, cmd);
	else
		/* start device can be optional for some driver */
		CAM_DBG(CAM_CORE, "No start device in dev %d, state %d",
			ctx->dev_hdl, ctx->state);

	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_stop_dev(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;

	if (!ctx || !ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_CORE, "Invalid stop device command payload");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.stop_dev)
		rc = ctx->state_machine[ctx->state].ioctl_ops.stop_dev(
			ctx, cmd);
	else
		/* stop device can be optional for some driver */
		CAM_WARN(CAM_CORE, "No stop device in dev %d, name %s state %d",
			ctx->dev_hdl, ctx->dev_name, ctx->state);

	ctx->last_flush_req = 0;
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_info_dump(void *context,
	enum cam_context_dump_id id)
{
	int rc = 0;
	struct cam_context *ctx = (struct cam_context *)context;

	if (!ctx || !ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context is not ready");
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].dumpinfo_ops)
		rc = ctx->state_machine[ctx->state].dumpinfo_ops(ctx,
			id);
	mutex_unlock(&ctx->ctx_mutex);

	if (rc)
		CAM_WARN(CAM_CORE,
			"Dump for id %u failed on ctx_id %u name %s state %d",
			id, ctx->ctx_id, ctx->dev_name, ctx->state);

	return rc;
}

int cam_context_handle_dump_dev(struct cam_context *ctx,
	struct cam_dump_req_cmd *cmd)
{
	int rc = 0;

	if (!ctx) {
		CAM_ERR(CAM_CORE, "Invalid Context");
		return -EINVAL;
	}

	if (!ctx->state_machine) {
		CAM_ERR(CAM_CORE, "Context %s ctx_id %d is not ready",
			ctx->dev_name, ctx->ctx_id);
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_CORE,
			"Context %s ctx_id %d Invalid dump command payload",
			ctx->dev_name, ctx->ctx_id);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	CAM_DBG(CAM_CORE, "dump device in dev %d, name %s state %d",
		ctx->dev_hdl, ctx->dev_name, ctx->state);
	if (ctx->state_machine[ctx->state].ioctl_ops.dump_dev)
		rc = ctx->state_machine[ctx->state].ioctl_ops.dump_dev(
			ctx, cmd);
	else
		CAM_WARN(CAM_CORE, "No dump device in dev %d, name %s state %d",
			ctx->dev_hdl, ctx->dev_name, ctx->state);
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_init(struct cam_context *ctx,
	const char *dev_name,
	uint64_t dev_id,
	uint32_t ctx_id,
	struct cam_req_mgr_kmd_ops *crm_node_intf,
	struct cam_hw_mgr_intf *hw_mgr_intf,
	struct cam_ctx_request *req_list,
	uint32_t req_size)
{
	int i;

	/* crm_node_intf is optinal */
	if (!ctx || !hw_mgr_intf || !req_list) {
		CAM_ERR(CAM_CORE, "Invalid input parameters");
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));
	ctx->dev_hdl = -1;
	ctx->link_hdl = -1;
	ctx->session_hdl = -1;
	INIT_LIST_HEAD(&ctx->list);
	mutex_init(&ctx->ctx_mutex);
	mutex_init(&ctx->sync_mutex);
	spin_lock_init(&ctx->lock);

	strlcpy(ctx->dev_name, dev_name, CAM_CTX_DEV_NAME_MAX_LENGTH);
	ctx->dev_id = dev_id;
	ctx->ctx_id = ctx_id;
	ctx->last_flush_req = 0;
	ctx->ctx_crm_intf = NULL;
	ctx->crm_ctx_intf = crm_node_intf;
	ctx->hw_mgr_intf = hw_mgr_intf;
	ctx->irq_cb_intf = cam_context_handle_hw_event;

	INIT_LIST_HEAD(&ctx->active_req_list);
	INIT_LIST_HEAD(&ctx->wait_req_list);
	INIT_LIST_HEAD(&ctx->pending_req_list);
	INIT_LIST_HEAD(&ctx->free_req_list);
	ctx->req_list = req_list;
	ctx->req_size = req_size;
	for (i = 0; i < req_size; i++) {
		INIT_LIST_HEAD(&ctx->req_list[i].list);
		list_add_tail(&ctx->req_list[i].list, &ctx->free_req_list);
		ctx->req_list[i].ctx = ctx;
	}
	ctx->state = CAM_CTX_AVAILABLE;
	ctx->state_machine = NULL;
	ctx->ctx_priv = NULL;

	return 0;
}

int cam_context_deinit(struct cam_context *ctx)
{
	if (!ctx)
		return -EINVAL;

	/**
	 * This is called from platform device remove.
	 * Everyting should be released at this moment.
	 * so we just free the memory for the context
	 */
	if (ctx->state != CAM_CTX_AVAILABLE)
		CAM_ERR(CAM_CORE, "Device did not shutdown cleanly");

	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

void cam_context_putref(struct cam_context *ctx)
{
	if (kref_read(&ctx->refcount))
		kref_put(&ctx->refcount, cam_node_put_ctxt_to_free_list);
	else
		WARN(1, "ctx %s %d state %d devhdl %X\n", ctx->dev_name,
			ctx->ctx_id, ctx->state, ctx->dev_hdl);

	CAM_DBG(CAM_CORE,
		"ctx device hdl %ld, ref count %d, dev_name %s",
		ctx->dev_hdl, refcount_read(&(ctx->refcount.refcount)),
		ctx->dev_name);
}

void cam_context_getref(struct cam_context *ctx)
{
	if (kref_get_unless_zero(&ctx->refcount) == 0) {
		/* should never happen */
		WARN(1, "%s fail\n", __func__);
	}
	CAM_DBG(CAM_CORE,
		"ctx device hdl %ld, ref count %d, dev_name %s",
		ctx->dev_hdl, refcount_read(&(ctx->refcount.refcount)),
		ctx->dev_name);
}
