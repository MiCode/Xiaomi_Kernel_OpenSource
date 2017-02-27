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

#include <linux/slab.h>
#include <linux/uaccess.h>
#include "cam_context.h"

static int cam_context_handle_hw_event(void *context, uint32_t evt_id,
	void *evt_data)
{
	int rc = 0;
	struct cam_context *ctx = (struct cam_context *)context;

	if (!ctx || !ctx->state_machine) {
		pr_err("%s: Context is not ready.\n", __func__);
		return -EINVAL;
	}

	if (ctx->state_machine[ctx->state].irq_ops)
		rc = ctx->state_machine[ctx->state].irq_ops(ctx, evt_id,
			evt_data);
	else
		pr_debug("%s: No function to handle event %d in dev %d, state %d\n",
				__func__, evt_id, ctx->dev_hdl, ctx->state);
	return rc;
}

int cam_context_handle_get_dev_info(struct cam_context *ctx,
	struct cam_req_mgr_device_info *info)
{
	int rc;

	if (!ctx->state_machine) {
		pr_err("%s: Context is not ready.\n'", __func__);
		return -EINVAL;
	}

	if (!info) {
		pr_err("%s: Invalid get device info payload.\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.get_dev_info) {
		rc = ctx->state_machine[ctx->state].crm_ops.get_dev_info(
			ctx, info);
	} else {
		pr_err("%s: No get device info in dev %d, state %d\n",
			__func__, ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_link(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *link)
{
	int rc;

	if (!ctx->state_machine) {
		pr_err("%s: Context is not ready.\n", __func__);
		return -EINVAL;
	}

	if (!link) {
		pr_err("%s: Invalid link payload.\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.link) {
		rc = ctx->state_machine[ctx->state].crm_ops.link(ctx, link);
	} else {
		pr_err("%s: No crm link in dev %d, state %d\n", __func__,
			ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_unlink(struct cam_context *ctx,
	struct cam_req_mgr_core_dev_link_setup *unlink)
{
	int rc;

	if (!ctx->state_machine) {
		pr_err("%s: Context is not ready!\n", __func__);
		return -EINVAL;
	}

	if (!unlink) {
		pr_err("%s: Invalid unlink payload.\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.unlink) {
		rc = ctx->state_machine[ctx->state].crm_ops.unlink(
			ctx, unlink);
	} else {
		pr_err("%s: No crm unlink in dev %d, state %d\n",
			__func__, ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_apply_req(struct cam_context *ctx,
	struct cam_req_mgr_apply_request *apply)
{
	int rc;

	if (!ctx->state_machine) {
		pr_err("%s: Context is not ready.\n'", __func__);
		return -EINVAL;
	}

	if (!apply) {
		pr_err("%s: Invalid apply request payload.\n'", __func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].crm_ops.apply_req) {
		rc = ctx->state_machine[ctx->state].crm_ops.apply_req(ctx,
			apply);
	} else {
		pr_err("%s: No crm apply req in dev %d, state %d\n",
			__func__, ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}


int cam_context_handle_acquire_dev(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;

	if (!ctx->state_machine) {
		pr_err("%s: Context is not ready.\n", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		pr_err("%s: Invalid acquire device command payload.\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.acquire_dev) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.acquire_dev(
			ctx, cmd);
	} else {
		pr_err("%s: No acquire device in dev %d, state %d\n",
			__func__, cmd->dev_handle, ctx->state);
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
		pr_err("%s: Context is not ready.\n", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		pr_err("%s: Invalid release device command payload.\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.release_dev) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.release_dev(
			ctx, cmd);
	} else {
		pr_err("%s: No release device in dev %d, state %d\n",
			__func__, ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_config_dev(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc;

	if (!ctx->state_machine) {
		pr_err("%s: context is not ready\n'", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		pr_err("%s: Invalid config device command payload.\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.config_dev) {
		rc = ctx->state_machine[ctx->state].ioctl_ops.config_dev(
			ctx, cmd);
	} else {
		pr_err("%s: No config device in dev %d, state %d\n",
			__func__, ctx->dev_hdl, ctx->state);
		rc = -EPROTO;
	}
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_start_dev(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;

	if (!ctx->state_machine) {
		pr_err("%s: Context is not ready.\n", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		pr_err("%s: Invalid start device command payload.\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.start_dev)
		rc = ctx->state_machine[ctx->state].ioctl_ops.start_dev(
			ctx, cmd);
	else
		/* start device can be optional for some driver */
		pr_debug("%s: No start device in dev %d, state %d\n",
			__func__, ctx->dev_hdl, ctx->state);

	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_handle_stop_dev(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;

	if (!ctx->state_machine) {
		pr_err("%s: Context is not ready.\n'", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		pr_err("%s: Invalid stop device command payload.\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->ctx_mutex);
	if (ctx->state_machine[ctx->state].ioctl_ops.stop_dev)
		rc = ctx->state_machine[ctx->state].ioctl_ops.stop_dev(
			ctx, cmd);
	else
		/* stop device can be optional for some driver */
		pr_warn("%s: No stop device in dev %d, state %d\n",
			__func__, ctx->dev_hdl, ctx->state);
	mutex_unlock(&ctx->ctx_mutex);

	return rc;
}

int cam_context_init(struct cam_context *ctx,
	struct cam_req_mgr_kmd_ops *crm_node_intf,
	struct cam_hw_mgr_intf *hw_mgr_intf,
	struct cam_ctx_request *req_list,
	uint32_t req_size)
{
	int i;

	/* crm_node_intf is optinal */
	if (!ctx || !hw_mgr_intf || !req_list) {
		pr_err("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	memset(ctx, 0, sizeof(*ctx));

	INIT_LIST_HEAD(&ctx->list);
	mutex_init(&ctx->ctx_mutex);
	spin_lock_init(&ctx->lock);

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
		pr_err("%s: Device did not shutdown cleanly.\n", __func__);

	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

