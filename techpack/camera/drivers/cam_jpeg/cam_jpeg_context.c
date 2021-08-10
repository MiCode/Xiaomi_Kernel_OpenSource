// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "cam_mem_mgr.h"
#include "cam_jpeg_context.h"
#include "cam_context_utils.h"
#include "cam_debug_util.h"
#include "cam_packet_util.h"

static const char jpeg_dev_name[] = "cam-jpeg";

static int cam_jpeg_context_dump_active_request(void *data,
	struct cam_smmu_pf_info *pf_info)
{

	struct cam_context *ctx = (struct cam_context *)data;
	struct cam_ctx_request          *req = NULL;
	struct cam_ctx_request          *req_temp = NULL;
	struct cam_hw_mgr_dump_pf_data  *pf_dbg_entry = NULL;
	uint32_t  resource_type = 0;
	int rc = 0;
	int closest_port;
	bool b_mem_found = false, b_ctx_found = false;


	if (!ctx) {
		CAM_ERR(CAM_JPEG, "Invalid ctx");
		return -EINVAL;
	}

	CAM_INFO(CAM_JPEG, "iommu fault for jpeg ctx %d state %d",
		ctx->ctx_id, ctx->state);

	list_for_each_entry_safe(req, req_temp,
			&ctx->active_req_list, list) {
		pf_dbg_entry = &(req->pf_data);
		closest_port = -1;
		CAM_INFO(CAM_JPEG, "req_id : %lld ", req->request_id);

		rc = cam_context_dump_pf_info_to_hw(ctx, pf_dbg_entry->packet,
			&b_mem_found, &b_ctx_found, &resource_type, pf_info);
		if (rc)
			CAM_ERR(CAM_JPEG, "Failed to dump pf info");

		if (b_mem_found)
			CAM_ERR(CAM_JPEG, "Found page fault in req %lld %d",
				req->request_id, rc);
	}
	return rc;
}

static int __cam_jpeg_ctx_acquire_dev_in_available(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_acquire_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_JPEG, "Unable to Acquire device %d", rc);
	else
		ctx->state = CAM_CTX_ACQUIRED;

	return rc;
}

static int __cam_jpeg_ctx_release_dev_in_acquired(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_release_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_JPEG, "Unable to release device %d", rc);

	ctx->state = CAM_CTX_AVAILABLE;

	return rc;
}

static int __cam_jpeg_ctx_dump_dev_in_acquired(
	struct cam_context      *ctx,
	struct cam_dump_req_cmd *cmd)
{
	int rc;

	rc = cam_context_dump_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_JPEG, "Failed to dump device, rc=%d", rc);

	return rc;
}

static int __cam_jpeg_ctx_flush_dev_in_acquired(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_flush_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_ICP, "Failed to flush device");

	return rc;
}

static int __cam_jpeg_ctx_config_dev_in_acquired(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	return cam_context_prepare_dev_to_hw(ctx, cmd);
}

static int __cam_jpeg_ctx_handle_buf_done_in_acquired(void *ctx,
	uint32_t evt_id, void *done)
{
	return cam_context_buf_done_from_hw(ctx, done, evt_id);
}

static int __cam_jpeg_ctx_stop_dev_in_acquired(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_stop_dev_to_hw(ctx);
	if (rc) {
		CAM_ERR(CAM_JPEG, "Failed in Stop dev, rc=%d", rc);
		return rc;
	}

	return rc;
}

/* top state machine */
static struct cam_ctx_ops
	cam_jpeg_ctx_state_machine[CAM_CTX_STATE_MAX] = {
	/* Uninit */
	{
		.ioctl_ops = { },
		.crm_ops = { },
		.irq_ops = NULL,
	},
	/* Available */
	{
		.ioctl_ops = {
			.acquire_dev = __cam_jpeg_ctx_acquire_dev_in_available,
		},
		.crm_ops = { },
		.irq_ops = NULL,
	},
	/* Acquired */
	{
		.ioctl_ops = {
			.release_dev = __cam_jpeg_ctx_release_dev_in_acquired,
			.config_dev = __cam_jpeg_ctx_config_dev_in_acquired,
			.stop_dev = __cam_jpeg_ctx_stop_dev_in_acquired,
			.flush_dev = __cam_jpeg_ctx_flush_dev_in_acquired,
			.dump_dev = __cam_jpeg_ctx_dump_dev_in_acquired,
		},
		.crm_ops = { },
		.irq_ops = __cam_jpeg_ctx_handle_buf_done_in_acquired,
		.pagefault_ops = cam_jpeg_context_dump_active_request,
	},
};

int cam_jpeg_context_init(struct cam_jpeg_context *ctx,
	struct cam_context *ctx_base,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id)
{
	int rc;
	int i;

	if (!ctx || !ctx_base) {
		CAM_ERR(CAM_JPEG, "Invalid Context");
		rc = -EFAULT;
		goto err;
	}

	memset(ctx, 0, sizeof(*ctx));

	ctx->base = ctx_base;

	for (i = 0; i < CAM_CTX_REQ_MAX; i++)
		ctx->req_base[i].req_priv = ctx;

	rc = cam_context_init(ctx_base, jpeg_dev_name, CAM_JPEG, ctx_id,
		NULL, hw_intf, ctx->req_base, CAM_CTX_REQ_MAX);
	if (rc) {
		CAM_ERR(CAM_JPEG, "Camera Context Base init failed");
		goto err;
	}

	ctx_base->state_machine = cam_jpeg_ctx_state_machine;
	ctx_base->ctx_priv = ctx;

err:
	return rc;
}

int cam_jpeg_context_deinit(struct cam_jpeg_context *ctx)
{
	if (!ctx || !ctx->base) {
		CAM_ERR(CAM_JPEG, "Invalid params: %pK", ctx);
		return -EINVAL;
	}

	cam_context_deinit(ctx->base);

	memset(ctx, 0, sizeof(*ctx));

	return 0;
}
