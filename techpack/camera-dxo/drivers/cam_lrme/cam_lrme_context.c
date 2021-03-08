// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_debug_util.h"
#include "cam_lrme_context.h"

static const char lrme_dev_name[] = "cam-lrme";

static int __cam_lrme_ctx_acquire_dev_in_available(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc = 0;
	uintptr_t ctxt_to_hw_map = (uintptr_t)ctx->ctxt_to_hw_map;
	struct cam_lrme_context *lrme_ctx = ctx->ctx_priv;

	CAM_DBG(CAM_LRME, "Enter ctx %d", ctx->ctx_id);

	rc = cam_context_acquire_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to acquire");
		return rc;
	}

	ctxt_to_hw_map |= (lrme_ctx->index << CAM_LRME_CTX_INDEX_SHIFT);
	ctx->ctxt_to_hw_map = (void *)ctxt_to_hw_map;

	ctx->state = CAM_CTX_ACQUIRED;

	return rc;
}

static int __cam_lrme_ctx_release_dev_in_acquired(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc = 0;

	CAM_DBG(CAM_LRME, "Enter ctx %d", ctx->ctx_id);

	rc = cam_context_release_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to release");
		return rc;
	}

	ctx->state = CAM_CTX_AVAILABLE;

	return rc;
}

static int __cam_lrme_ctx_start_dev_in_acquired(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;

	CAM_DBG(CAM_LRME, "Enter ctx %d", ctx->ctx_id);

	rc = cam_context_start_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to start");
		return rc;
	}

	ctx->state = CAM_CTX_ACTIVATED;

	return rc;
}

static int __cam_lrme_ctx_config_dev_in_activated(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc;

	CAM_DBG(CAM_LRME, "Enter ctx %d", ctx->ctx_id);

	rc = cam_context_prepare_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to config");
		return rc;
	}

	return rc;
}

static int __cam_lrme_ctx_flush_dev_in_activated(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{
	int rc;

	CAM_DBG(CAM_LRME, "Enter ctx %d", ctx->ctx_id);

	rc = cam_context_flush_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_LRME, "Failed to flush device");

	return rc;
}
static int __cam_lrme_ctx_stop_dev_in_activated(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc = 0;

	CAM_DBG(CAM_LRME, "Enter ctx %d", ctx->ctx_id);

	rc = cam_context_stop_dev_to_hw(ctx);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to stop dev");
		return rc;
	}

	ctx->state = CAM_CTX_ACQUIRED;

	return rc;
}

static int __cam_lrme_ctx_release_dev_in_activated(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc = 0;

	CAM_DBG(CAM_LRME, "Enter ctx %d", ctx->ctx_id);

	rc = __cam_lrme_ctx_stop_dev_in_activated(ctx, NULL);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to stop");
		return rc;
	}

	rc = cam_context_release_dev_to_hw(ctx, cmd);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to release");
		return rc;
	}

	ctx->state = CAM_CTX_AVAILABLE;

	return rc;
}

static int __cam_lrme_ctx_handle_irq_in_activated(void *context,
	uint32_t evt_id, void *evt_data)
{
	int rc;

	CAM_DBG(CAM_LRME, "Enter");

	rc = cam_context_buf_done_from_hw(context, evt_data, evt_id);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed in buf done, rc=%d", rc);
		return rc;
	}

	return rc;
}

/* top state machine */
static struct cam_ctx_ops
	cam_lrme_ctx_state_machine[CAM_CTX_STATE_MAX] = {
	/* Uninit */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Available */
	{
		.ioctl_ops = {
			.acquire_dev = __cam_lrme_ctx_acquire_dev_in_available,
		},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Acquired */
	{
		.ioctl_ops = {
			.config_dev = __cam_lrme_ctx_config_dev_in_activated,
			.release_dev = __cam_lrme_ctx_release_dev_in_acquired,
			.start_dev = __cam_lrme_ctx_start_dev_in_acquired,
		},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Ready */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Flushed */
	{},
	/* Activate */
	{
		.ioctl_ops = {
			.config_dev = __cam_lrme_ctx_config_dev_in_activated,
			.release_dev = __cam_lrme_ctx_release_dev_in_activated,
			.stop_dev = __cam_lrme_ctx_stop_dev_in_activated,
			.flush_dev = __cam_lrme_ctx_flush_dev_in_activated,
		},
		.crm_ops = {},
		.irq_ops = __cam_lrme_ctx_handle_irq_in_activated,
	},
};

int cam_lrme_context_init(struct cam_lrme_context *lrme_ctx,
	struct cam_context *base_ctx,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t index)
{
	int rc = 0;

	CAM_DBG(CAM_LRME, "Enter");

	if (!base_ctx || !lrme_ctx) {
		CAM_ERR(CAM_LRME, "Invalid input");
		return -EINVAL;
	}

	memset(lrme_ctx, 0, sizeof(*lrme_ctx));

	rc = cam_context_init(base_ctx, lrme_dev_name, CAM_LRME, index,
		NULL, hw_intf, lrme_ctx->req_base, CAM_CTX_REQ_MAX);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to init context");
		return rc;
	}
	lrme_ctx->base = base_ctx;
	lrme_ctx->index = index;
	base_ctx->ctx_priv = lrme_ctx;
	base_ctx->state_machine = cam_lrme_ctx_state_machine;

	return rc;
}

int cam_lrme_context_deinit(struct cam_lrme_context *lrme_ctx)
{
	int rc = 0;

	CAM_DBG(CAM_LRME, "Enter");

	if (!lrme_ctx) {
		CAM_ERR(CAM_LRME, "No ctx to deinit");
		return -EINVAL;
	}

	rc = cam_context_deinit(lrme_ctx->base);

	memset(lrme_ctx, 0, sizeof(*lrme_ctx));
	return rc;
}
