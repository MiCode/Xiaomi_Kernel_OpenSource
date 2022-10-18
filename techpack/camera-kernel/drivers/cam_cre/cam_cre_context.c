// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "cam_trace.h"
#include "cam_mem_mgr.h"
#include "cam_cre_context.h"
#include "cam_context_utils.h"
#include "cam_debug_util.h"
#include "cam_packet_util.h"
#include "cam_context.h"

static const char cre_dev_name[] = "cam-cre";

static int __cam_cre_start_dev_in_acquired(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_start_dev_to_hw(ctx, cmd);
	if (!rc) {
		ctx->state = CAM_CTX_READY;
		trace_cam_context_state("CRE", ctx);
	}

	return rc;
}


static int __cam_cre_ctx_flush_dev_in_ready(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_flush_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_CRE, "Failed to flush device");

	return rc;
}

static int __cam_cre_ctx_dump_dev_in_ready(struct cam_context *ctx,
	struct cam_dump_req_cmd *cmd)
{
	int rc;

	rc = cam_context_dump_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_CRE, "Failed to dump device");

	return rc;
}

static int __cam_cre_ctx_config_dev_in_ready(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc;
	size_t len;
	uintptr_t packet_addr;

	rc = cam_mem_get_cpu_buf((int32_t) cmd->packet_handle,
		&packet_addr, &len);
	if (rc) {
		CAM_ERR(CAM_CRE, "[%s][%d] Can not get packet address",
			ctx->dev_name, ctx->ctx_id);
		rc = -EINVAL;
		return rc;
	}

	rc = cam_context_prepare_dev_to_hw(ctx, cmd);

	if (rc)
		CAM_ERR(CAM_CRE, "Failed to prepare device");

	return rc;
}

static int __cam_cre_ctx_stop_dev_in_ready(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_stop_dev_to_hw(ctx);
	if (rc)
		CAM_ERR(CAM_CRE, "Failed to stop device");

	ctx->state = CAM_CTX_ACQUIRED;
	trace_cam_context_state("CRE", ctx);
	return rc;
}

static int __cam_cre_ctx_release_dev_in_acquired(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_release_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_CRE, "Unable to release device %d", rc);

	ctx->state = CAM_CTX_AVAILABLE;

	return rc;
}

static int __cam_cre_ctx_release_dev_in_ready(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = __cam_cre_ctx_stop_dev_in_ready(ctx, NULL);
	if (rc)
		CAM_ERR(CAM_CRE, "Failed to stop device");

	rc = __cam_cre_ctx_release_dev_in_acquired(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_CRE, "Failed to release device");

	return rc;
}

static int __cam_cre_ctx_handle_buf_done_in_ready(void *ctx,
	uint32_t evt_id, void *done)
{
	return cam_context_buf_done_from_hw(ctx, done, evt_id);
}

static int cam_cre_context_dump_active_request(void *data,
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
		CAM_ERR(CAM_CRE, "Invalid ctx");
		return -EINVAL;
	}

	CAM_INFO(CAM_CRE, "iommu fault for cre ctx %d state %d",
		ctx->ctx_id, ctx->state);

	list_for_each_entry_safe(req, req_temp,
			&ctx->active_req_list, list) {
		pf_dbg_entry = &(req->pf_data);
		closest_port = -1;
		CAM_INFO(CAM_CRE, "req_id : %lld ", req->request_id);

		rc = cam_context_dump_pf_info_to_hw(ctx, pf_dbg_entry->packet,
			&b_mem_found, &b_ctx_found, &resource_type, pf_info);
		if (rc)
			CAM_ERR(CAM_CRE, "Failed to dump pf info");

		if (b_mem_found)
			CAM_ERR(CAM_CRE, "Found page fault in req %lld %d",
				req->request_id, rc);
	}
	return rc;
}

static int __cam_cre_ctx_acquire_dev_in_available(struct cam_context *ctx,
	struct cam_acquire_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_acquire_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_CRE, "Unable to Acquire device %d", rc);
	else
		ctx->state = CAM_CTX_ACQUIRED;

	return rc;
}

/* top state machine */
static struct cam_ctx_ops
	cam_cre_ctx_state_machine[CAM_CTX_STATE_MAX] = {
	/* Uninit */
	{
		.ioctl_ops = { },
		.crm_ops = { },
		.irq_ops = NULL,
	},
	/* Available */
	{
		.ioctl_ops = {
			.acquire_dev = __cam_cre_ctx_acquire_dev_in_available,
		},
		.crm_ops = { },
		.irq_ops = NULL,
	},
	/* Acquired */
	{
		.ioctl_ops = {
			.release_dev = __cam_cre_ctx_release_dev_in_acquired,
			.start_dev = __cam_cre_start_dev_in_acquired,
			.config_dev = __cam_cre_ctx_config_dev_in_ready,
			.flush_dev = __cam_cre_ctx_flush_dev_in_ready,
			.dump_dev = __cam_cre_ctx_dump_dev_in_ready,
		},
		.crm_ops = { },
		.irq_ops = __cam_cre_ctx_handle_buf_done_in_ready,
		.pagefault_ops = cam_cre_context_dump_active_request,
	},
	/* Ready */
	{
		.ioctl_ops = {
			.stop_dev = __cam_cre_ctx_stop_dev_in_ready,
			.release_dev = __cam_cre_ctx_release_dev_in_ready,
			.config_dev = __cam_cre_ctx_config_dev_in_ready,
			.flush_dev = __cam_cre_ctx_flush_dev_in_ready,
			.dump_dev = __cam_cre_ctx_dump_dev_in_ready,
		},
		.crm_ops = {},
		.irq_ops = __cam_cre_ctx_handle_buf_done_in_ready,
		.pagefault_ops = cam_cre_context_dump_active_request,
	},
	/* Activated */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
		.pagefault_ops = cam_cre_context_dump_active_request,
	},
};

int cam_cre_context_init(struct cam_cre_context *ctx,
	struct cam_hw_mgr_intf *hw_intf,
	uint32_t ctx_id, int img_iommu_hdl)
{
	int rc;
	int i;

	if (!ctx || !ctx->base) {
		CAM_ERR(CAM_CRE, "Invalid Context");
		rc = -EFAULT;
		goto err;
	}

	for (i = 0; i < CAM_CTX_REQ_MAX; i++)
		ctx->req_base[i].req_priv = ctx;

	rc = cam_context_init(ctx->base, cre_dev_name, CAM_CRE, ctx_id,
		NULL, hw_intf, ctx->req_base, CAM_CTX_REQ_MAX, img_iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_CRE, "Camera Context Base init failed");
		goto err;
	}

	ctx->base->state_machine = cam_cre_ctx_state_machine;
	ctx->base->ctx_priv = ctx;

	ctx->base->max_hw_update_entries = CAM_CTX_CFG_MAX;
	ctx->base->max_in_map_entries = CAM_CTX_CFG_MAX;
	ctx->base->max_out_map_entries = CAM_CTX_CFG_MAX;
err:
	return rc;
}

int cam_cre_context_deinit(struct cam_cre_context *ctx)
{
	if (!ctx || !ctx->base) {
		CAM_ERR(CAM_CRE, "Invalid params: %pK", ctx);
		return -EINVAL;
	}

	cam_context_deinit(ctx->base);

	memset(ctx, 0, sizeof(*ctx));

	return 0;
}
