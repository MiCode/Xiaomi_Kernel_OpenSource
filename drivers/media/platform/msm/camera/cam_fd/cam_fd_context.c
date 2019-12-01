/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_debug_util.h"
#include "cam_fd_context.h"
#include "cam_trace.h"

static const char fd_dev_name[] = "fd";

/* Functions in Available state */
static int __cam_fd_ctx_acquire_dev_in_available(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_acquire_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in Acquire dev, rc=%d", rc);
		return rc;
	}

	ctx->state = CAM_CTX_ACQUIRED;
	trace_cam_context_state("FD", ctx);

	return rc;
}

/* Functions in Acquired state */
static int __cam_fd_ctx_release_dev_in_acquired(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_release_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in Release dev, rc=%d", rc);
		return rc;
	}

	ctx->state = CAM_CTX_AVAILABLE;
	trace_cam_context_state("FD", ctx);

	return rc;
}

static int __cam_fd_ctx_config_dev_in_acquired(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_prepare_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in Prepare dev, rc=%d", rc);
		return rc;
	}

	return rc;
}

static int __cam_fd_ctx_start_dev_in_acquired(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_start_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in Start dev, rc=%d", rc);
		return rc;
	}

	ctx->state = CAM_CTX_ACTIVATED;
	trace_cam_context_state("FD", ctx);

	return rc;
}

/* Functions in Activated state */
static int __cam_fd_ctx_stop_dev_in_activated(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_stop_dev_to_hw(ctx);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in Stop dev, rc=%d", rc);
		return rc;
	}

	ctx->state = CAM_CTX_ACQUIRED;
	trace_cam_context_state("FD", ctx);

	return rc;
}

static int __cam_fd_ctx_release_dev_in_activated(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = __cam_fd_ctx_stop_dev_in_activated(ctx, NULL);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in Stop dev, rc=%d", rc);
		return rc;
	}

	rc = __cam_fd_ctx_release_dev_in_acquired(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in Release dev, rc=%d", rc);
		return rc;
	}

	return rc;
}

static int __cam_fd_ctx_flush_dev_in_activated(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_flush_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_ICP, "Failed to flush device, rc=%d", rc);

	return rc;
}
static int __cam_fd_ctx_config_dev_in_activated(
	struct cam_context *ctx, struct cam_config_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_prepare_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in Prepare dev, rc=%d", rc);
		return rc;
	}

	return rc;
}

static int __cam_fd_ctx_handle_irq_in_activated(void *context,
	uint32_t evt_id, void *evt_data)
{
	int rc;

	rc = cam_context_buf_done_from_hw(context, evt_data, evt_id);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in buf done, rc=%d", rc);
		return rc;
	}

	return rc;
}

/* top state machine */
static struct cam_ctx_ops
	cam_fd_ctx_state_machine[CAM_CTX_STATE_MAX] = {
	/* Uninit */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Available */
	{
		.ioctl_ops = {
			.acquire_dev = __cam_fd_ctx_acquire_dev_in_available,
		},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Acquired */
	{
		.ioctl_ops = {
			.release_dev = __cam_fd_ctx_release_dev_in_acquired,
			.config_dev = __cam_fd_ctx_config_dev_in_acquired,
			.start_dev = __cam_fd_ctx_start_dev_in_acquired,
		},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Ready */
	{
		.ioctl_ops = { },
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Activated */
	{
		.ioctl_ops = {
			.stop_dev = __cam_fd_ctx_stop_dev_in_activated,
			.release_dev = __cam_fd_ctx_release_dev_in_activated,
			.config_dev = __cam_fd_ctx_config_dev_in_activated,
			.flush_dev = __cam_fd_ctx_flush_dev_in_activated,
		},
		.crm_ops = {},
		.irq_ops = __cam_fd_ctx_handle_irq_in_activated,
	},
};


int cam_fd_context_init(struct cam_fd_context *fd_ctx,
	struct cam_context *base_ctx, struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id)
{
	int rc;

	if (!base_ctx || !fd_ctx) {
		CAM_ERR(CAM_FD, "Invalid Context %pK %pK", base_ctx, fd_ctx);
		return -EINVAL;
	}

	memset(fd_ctx, 0, sizeof(*fd_ctx));

	rc = cam_context_init(base_ctx, fd_dev_name, CAM_FD, ctx_id,
		NULL, hw_intf, fd_ctx->req_base, CAM_CTX_REQ_MAX);
	if (rc) {
		CAM_ERR(CAM_FD, "Camera Context Base init failed, rc=%d", rc);
		return rc;
	}

	fd_ctx->base = base_ctx;
	base_ctx->ctx_priv = fd_ctx;
	base_ctx->state_machine = cam_fd_ctx_state_machine;

	return rc;
}

int cam_fd_context_deinit(struct cam_fd_context *fd_ctx)
{
	int rc = 0;

	if (!fd_ctx || !fd_ctx->base) {
		CAM_ERR(CAM_FD, "Invalid inputs %pK", fd_ctx);
		return -EINVAL;
	}

	rc = cam_context_deinit(fd_ctx->base);
	if (rc)
		CAM_ERR(CAM_FD, "Error in base deinit, rc=%d", rc);

	memset(fd_ctx, 0, sizeof(*fd_ctx));

	return rc;
}
