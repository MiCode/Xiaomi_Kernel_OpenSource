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

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/cam_sync.h>
#include <media/cam_defs.h>
#include "cam_sync_api.h"
#include "cam_node.h"
#include "cam_context.h"
#include "cam_context_utils.h"
#include "cam_icp_context.h"
#include "cam_req_mgr_util.h"
#include "cam_mem_mgr.h"
#include "cam_trace.h"
#include "cam_debug_util.h"

static const char icp_dev_name[] = "icp";

static int __cam_icp_acquire_dev_in_available(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_acquire_dev_to_hw(ctx, cmd);
	if (!rc) {
		ctx->state = CAM_CTX_ACQUIRED;
		trace_cam_context_state("ICP", ctx);
	}

	return rc;
}

static int __cam_icp_release_dev_in_acquired(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_release_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_ICP, "Unable to release device");

	ctx->state = CAM_CTX_AVAILABLE;
	trace_cam_context_state("ICP", ctx);
	return rc;
}

static int __cam_icp_start_dev_in_acquired(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_start_dev_to_hw(ctx, cmd);
	if (!rc) {
		ctx->state = CAM_CTX_READY;
		trace_cam_context_state("ICP", ctx);
	}

	return rc;
}

static int __cam_icp_flush_dev_in_ready(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_flush_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_ICP, "Failed to flush device");

	return rc;
}

static int __cam_icp_config_dev_in_ready(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_prepare_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_ICP, "Failed to prepare device");

	return rc;
}

static int __cam_icp_stop_dev_in_ready(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_stop_dev_to_hw(ctx);
	if (rc)
		CAM_ERR(CAM_ICP, "Failed to stop device");

	ctx->state = CAM_CTX_ACQUIRED;
	trace_cam_context_state("ICP", ctx);
	return rc;
}

static int __cam_icp_release_dev_in_ready(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = __cam_icp_stop_dev_in_ready(ctx, NULL);
	if (rc)
		CAM_ERR(CAM_ICP, "Failed to stop device");

	rc = __cam_icp_release_dev_in_acquired(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_ICP, "Failed to release device");

	return rc;
}

static int __cam_icp_handle_buf_done_in_ready(void *ctx,
	uint32_t evt_id, void *done)
{
	return cam_context_buf_done_from_hw(ctx, done, evt_id);
}

static struct cam_ctx_ops
	cam_icp_ctx_state_machine[CAM_CTX_STATE_MAX] = {
	/* Uninit */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Available */
	{
		.ioctl_ops = {
			.acquire_dev = __cam_icp_acquire_dev_in_available,
		},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Acquired */
	{
		.ioctl_ops = {
			.release_dev = __cam_icp_release_dev_in_acquired,
			.start_dev = __cam_icp_start_dev_in_acquired,
			.config_dev = __cam_icp_config_dev_in_ready,
			.flush_dev = __cam_icp_flush_dev_in_ready,
		},
		.crm_ops = {},
		.irq_ops = __cam_icp_handle_buf_done_in_ready,
	},
	/* Ready */
	{
		.ioctl_ops = {
			.stop_dev = __cam_icp_stop_dev_in_ready,
			.release_dev = __cam_icp_release_dev_in_ready,
			.config_dev = __cam_icp_config_dev_in_ready,
			.flush_dev = __cam_icp_flush_dev_in_ready,
		},
		.crm_ops = {},
		.irq_ops = __cam_icp_handle_buf_done_in_ready,
	},
	/* Activated */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
};

int cam_icp_context_init(struct cam_icp_context *ctx,
	struct cam_hw_mgr_intf *hw_intf, uint32_t ctx_id)
{
	int rc;

	if ((!ctx) || (!ctx->base) || (!hw_intf)) {
		CAM_ERR(CAM_ICP, "Invalid params: %pK %pK", ctx, hw_intf);
		rc = -EINVAL;
		goto err;
	}

	rc = cam_context_init(ctx->base, icp_dev_name, CAM_ICP, ctx_id,
		NULL, hw_intf, ctx->req_base, CAM_CTX_REQ_MAX);
	if (rc) {
		CAM_ERR(CAM_ICP, "Camera Context Base init failed");
		goto err;
	}

	ctx->base->state_machine = cam_icp_ctx_state_machine;
	ctx->base->ctx_priv = ctx;
	ctx->ctxt_to_hw_map = NULL;

err:
	return rc;
}

int cam_icp_context_deinit(struct cam_icp_context *ctx)
{
	if ((!ctx) || (!ctx->base)) {
		CAM_ERR(CAM_ICP, "Invalid params: %pK", ctx);
		return -EINVAL;
	}

	cam_context_deinit(ctx->base);
	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

