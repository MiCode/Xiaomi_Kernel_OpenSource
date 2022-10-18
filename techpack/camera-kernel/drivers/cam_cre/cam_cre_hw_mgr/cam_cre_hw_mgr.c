// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <media/cam_defs.h>
#include <media/cam_cre.h>
#include <media/cam_cpas.h>

#include "cam_sync_api.h"
#include "cam_packet_util.h"
#include "cam_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_cre_hw_mgr_intf.h"
#include "cam_cre_hw_mgr.h"
#include "cre_hw.h"
#include "cam_smmu_api.h"
#include "cam_mem_mgr.h"
#include "cam_req_mgr_workq.h"
#include "cam_mem_mgr.h"
#include "cam_debug_util.h"
#include "cam_soc_util.h"
#include "cam_cpas_api.h"
#include "cam_common_util.h"
#include "cre_dev_intf.h"
#include "cam_compat.h"

static struct cam_cre_hw_mgr *cre_hw_mgr;

static bool cam_cre_debug_clk_update(struct cam_cre_clk_info *hw_mgr_clk_info)
{
	if (cre_hw_mgr->cre_debug_clk &&
		cre_hw_mgr->cre_debug_clk != hw_mgr_clk_info->curr_clk) {
		hw_mgr_clk_info->base_clk = cre_hw_mgr->cre_debug_clk;
		hw_mgr_clk_info->curr_clk = cre_hw_mgr->cre_debug_clk;
		hw_mgr_clk_info->uncompressed_bw = cre_hw_mgr->cre_debug_clk;
		hw_mgr_clk_info->compressed_bw = cre_hw_mgr->cre_debug_clk;
		CAM_DBG(CAM_PERF, "bc = %d cc = %d ub %d cb %d",
			hw_mgr_clk_info->base_clk, hw_mgr_clk_info->curr_clk,
			hw_mgr_clk_info->uncompressed_bw,
			hw_mgr_clk_info->compressed_bw);
		return true;
	}

	return false;
}

static struct cam_cre_io_buf_info *cam_cre_mgr_get_rsc(
	struct cam_cre_ctx *ctx_data,
	struct cam_buf_io_cfg *in_io_buf)
{
	int k = 0;

	if (in_io_buf->direction == CAM_BUF_INPUT) {
		for (k = 0; k < CRE_MAX_IN_RES; k++) {
			if (ctx_data->cre_acquire.in_res[k].res_id ==
				in_io_buf->resource_type)
				return &ctx_data->cre_acquire.in_res[k];
		}
		if (k == CRE_MAX_IN_RES) {
			CAM_ERR(CAM_CRE, "Invalid res_id %d",
				in_io_buf->resource_type);
			goto end;
		}
	} else if (in_io_buf->direction == CAM_BUF_OUTPUT) {
		for (k = 0; k < CRE_MAX_OUT_RES; k++) {
			if (ctx_data->cre_acquire.out_res[k].res_id ==
				in_io_buf->resource_type)
				return &ctx_data->cre_acquire.out_res[k];
		}
		if (k == CRE_MAX_OUT_RES) {
			CAM_ERR(CAM_CRE, "Invalid res_id %d",
				in_io_buf->resource_type);
			goto end;
		}
	}

end:
	return NULL;
}

static int cam_cre_mgr_update_reg_set(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_request *cre_req, int batch_index)
{
	struct cam_cre_dev_reg_set_update reg_set_upd_cmd;
	int i;

	reg_set_upd_cmd.cre_reg_buf = cre_req->cre_reg_buf[batch_index];

	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv,
			CRE_HW_REG_SET_UPDATE,
			&reg_set_upd_cmd, sizeof(reg_set_upd_cmd));
	}

	return 0;
}

static void cam_cre_free_io_config(struct cam_cre_request *req)
{
	int i, j;

	for (i = 0; i < CRE_MAX_BATCH_SIZE; i++) {
		for (j = 0; j < CRE_MAX_IO_BUFS; j++) {
			if (req->io_buf[i][j]) {
				cam_free_clear(req->io_buf[i][j]);
				req->io_buf[i][j] = NULL;
			}
		}
	}
}

static int cam_cre_mgr_process_cmd_io_buf_req(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_packet *packet, struct cam_cre_ctx *ctx_data,
	uint32_t req_idx)
{
	int rc = 0;
	int i, j, k;
	dma_addr_t iova_addr;

	size_t len;
	struct cam_cre_request *cre_request;
	struct cre_io_buf *io_buf;
	struct plane_info *plane_info;

	uint32_t alignment;
	bool     is_secure;
	struct   cam_buf_io_cfg *io_cfg_ptr = NULL;
	struct   cam_cre_io_buf_info *acq_io_buf;

	io_cfg_ptr = (struct cam_buf_io_cfg *)((uint32_t *)&packet->payload +
			packet->io_configs_offset / 4);

	cre_request = ctx_data->req_list[req_idx];
	cre_request->num_batch = ctx_data->cre_acquire.batch_size;
	CAM_DBG(CAM_CRE, "num_io_configs %d", packet->num_io_configs);

	for (i = 0; i < cre_request->num_batch; i++) {
		for (j = 0; j < packet->num_io_configs; j++) {
			cre_request->num_io_bufs[i]++;
			acq_io_buf = cam_cre_mgr_get_rsc(ctx_data, &io_cfg_ptr[j]);
			if (!acq_io_buf) {
				CAM_ERR(CAM_CRE, "get rsc failed");
				return -EINVAL;
			}

			cre_request->io_buf[i][j] =
				kzalloc(sizeof(struct cre_io_buf), GFP_KERNEL);
			if (!cre_request->io_buf[i][j]) {
				CAM_ERR(CAM_CRE,
					"IO config allocation failure");
				cam_cre_free_io_config(cre_request);
				return -ENOMEM;
			}

			io_buf = cre_request->io_buf[i][j];
			io_buf->num_planes = acq_io_buf->num_planes;
			io_buf->resource_type = acq_io_buf->res_id;
			io_buf->direction = acq_io_buf->direction;
			io_buf->format = acq_io_buf->format;

			alignment = acq_io_buf->alignment;
			io_buf->fence = io_cfg_ptr[j].fence;

			CAM_DBG(CAM_CRE,
				"i %d j %d Number of planes %d res_type %d dir %d, fence %d format %d align %d",
				i, j, io_buf->num_planes, io_buf->resource_type,
				io_buf->direction, io_buf->fence, io_buf->format, alignment);

			for (k = 0; k < io_buf->num_planes; k++) {
				is_secure = cam_mem_is_secure_buf(
					io_cfg_ptr[j].mem_handle[k]);
				if (is_secure)
					rc = cam_mem_get_io_buf(
						io_cfg_ptr[j].mem_handle[k],
						hw_mgr->iommu_sec_hdl,
						&iova_addr, &len, NULL);
				else
					rc = cam_mem_get_io_buf(
						io_cfg_ptr[j].mem_handle[k],
						hw_mgr->iommu_hdl,
						&iova_addr, &len, NULL);

				if (rc) {
					CAM_ERR(CAM_CRE, "get buf failed: %d",
						rc);
					return -EINVAL;
				}
				iova_addr += io_cfg_ptr[j].offsets[k];
				plane_info = &io_buf->p_info[k];

				plane_info->offset = io_cfg_ptr[j].offsets[k];
				plane_info->format = acq_io_buf->format;
				plane_info->iova_addr = iova_addr +
					((io_cfg_ptr[j].planes[k].plane_stride *
					  io_cfg_ptr[j].planes[k].slice_height) * k);

				plane_info->stride    =
					io_cfg_ptr[j].planes[k].plane_stride;

				/* Width for WE has to be updated in number of pixels */
				if (acq_io_buf->direction == CAM_BUF_OUTPUT) {
					/* PLAIN 128/8 = 16 Bytes per pixel */
					plane_info->width =
						io_cfg_ptr[j].planes[k].plane_stride/16;
				} else {
					/* FE width should be in bytes */
					plane_info->width     =
						io_cfg_ptr[j].planes[k].plane_stride;
				}
				plane_info->height    =
					io_cfg_ptr[j].planes[k].height;
				plane_info->len       = len;
				plane_info->alignment = alignment;
			}
		}
	}

	return rc;
}

static void cam_cre_device_timer_reset(struct cam_cre_hw_mgr *hw_mgr)
{

	if (hw_mgr->clk_info.watch_dog) {
		CAM_DBG(CAM_CRE, "reset timer");
		crm_timer_reset(hw_mgr->clk_info.watch_dog);
			hw_mgr->clk_info.watch_dog_reset_counter++;
	}
}

static int cam_cre_mgr_reset_hw(void)
{
	struct cam_cre_hw_mgr *hw_mgr = cre_hw_mgr;
	int i, rc = 0;

	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		rc = hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv, CRE_HW_RESET,
			NULL, 0);
		if (rc) {
			CAM_ERR(CAM_CRE, "CRE Reset failed: %d", rc);
			return rc;
		}
	}

	return rc;
}


static void cam_cre_ctx_wait_for_idle_irq(struct cam_cre_ctx *ctx)
{
	int rc;

	if (ctx->ctx_state != CRE_CTX_STATE_ACQUIRED) {
		CAM_ERR(CAM_CRE, "ctx %u is in %d state",
			ctx->ctx_id, ctx->ctx_state);
		return;
	}

	rc = cam_common_wait_for_completion_timeout(
		&ctx->cre_top->idle_done,
		msecs_to_jiffies(CAM_CRE_RESPONSE_TIME_THRESHOLD));
	if (!rc) {
		cam_cre_device_timer_reset(cre_hw_mgr);
	} else {
		CAM_DBG(CAM_CRE, "IDLE done for req idx %d",
			ctx->last_req_idx);
	}
}

static int cam_cre_mgr_create_cre_reg_buf(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_packet *packet,
	struct cam_hw_prepare_update_args *prepare_args,
	struct cam_cre_ctx *ctx_data, uint32_t req_idx)
{
	int i, rc = 0;
	struct cam_cre_dev_prepare_req prepare_req;

	prepare_req.ctx_data = ctx_data;
	prepare_req.hw_mgr = hw_mgr;
	prepare_req.packet = packet;
	prepare_req.prepare_args = prepare_args;
	prepare_req.req_idx = req_idx;

	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		rc = hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv,
			CRE_HW_PREPARE, &prepare_req, sizeof(prepare_req));
		if (rc) {
			CAM_ERR(CAM_CRE, "CRE Dev prepare failed: %d", rc);
			goto end;
		}
	}

end:
	return rc;
}

static int cam_cre_mgr_calculate_num_path(
	struct cam_cre_clk_bw_req_internal_v2 *clk_info,
	struct cam_cre_ctx *ctx_data)
{
	int i, path_index = 0;

	for (i = 0; i < CAM_CRE_MAX_PER_PATH_VOTES; i++) {
		if ((clk_info->axi_path[i].path_data_type <
			CAM_AXI_PATH_DATA_CRE_START_OFFSET) ||
			(clk_info->axi_path[i].path_data_type >
			CAM_AXI_PATH_DATA_CRE_MAX_OFFSET) ||
			((clk_info->axi_path[i].path_data_type -
			CAM_AXI_PATH_DATA_CRE_START_OFFSET) >=
			CAM_CRE_MAX_PER_PATH_VOTES)) {
			CAM_DBG(CAM_CRE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i].path_data_type,
				CAM_AXI_PATH_DATA_CRE_START_OFFSET,
				CAM_CRE_MAX_PER_PATH_VOTES);
			continue;
		}

		path_index = clk_info->axi_path[i].path_data_type -
			CAM_AXI_PATH_DATA_CRE_START_OFFSET;

		CAM_DBG(CAM_CRE,
			"clk_info: i[%d]: [%s %s] bw [%lld %lld] num_path: %d",
			i,
			cam_cpas_axi_util_trans_type_to_string(
			clk_info->axi_path[i].transac_type),
			cam_cpas_axi_util_path_type_to_string(
			clk_info->axi_path[i].path_data_type),
			clk_info->axi_path[i].camnoc_bw,
			clk_info->axi_path[i].mnoc_ab_bw,
			clk_info->num_paths);
	}
	return 0;
}

static int cam_cre_update_cpas_vote(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data)
{
	int i = 0;
	struct cam_cre_clk_info *clk_info;
	struct cam_cre_dev_bw_update bw_update = {{0}, {0}, 0, 0};

	clk_info = &hw_mgr->clk_info;

	bw_update.ahb_vote.type = CAM_VOTE_DYNAMIC;
	bw_update.ahb_vote.vote.freq = 0;
	bw_update.ahb_vote_valid = false;

	bw_update.axi_vote.num_paths = clk_info->num_paths;
	memcpy(&bw_update.axi_vote.axi_path[0],
		&clk_info->axi_path[0],
		bw_update.axi_vote.num_paths *
		sizeof(struct cam_axi_per_path_bw_vote));

	bw_update.axi_vote_valid = true;
	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv,
			CRE_HW_BW_UPDATE,
			&bw_update, sizeof(bw_update));
	}
	return 0;
}

static int cam_cre_mgr_remove_bw(struct cam_cre_hw_mgr *hw_mgr, int ctx_id)
{
	int path_index, i, rc = 0;
	struct cam_cre_ctx *ctx_data = NULL;
	struct cam_cre_clk_info *hw_mgr_clk_info;

	ctx_data = &hw_mgr->ctx[ctx_id];
	hw_mgr_clk_info = &hw_mgr->clk_info;
	for (i = 0; i < ctx_data->clk_info.num_paths; i++) {
		path_index =
		ctx_data->clk_info.axi_path[i].path_data_type -
			CAM_AXI_PATH_DATA_CRE_START_OFFSET;
		if (path_index >= CAM_CRE_MAX_PER_PATH_VOTES) {
			CAM_WARN(CAM_CRE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i].path_data_type,
				CAM_AXI_PATH_DATA_CRE_START_OFFSET,
				CAM_CRE_MAX_PER_PATH_VOTES);
			continue;
		}

		hw_mgr_clk_info->axi_path[path_index].camnoc_bw -=
			ctx_data->clk_info.axi_path[i].camnoc_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ab_bw -=
			ctx_data->clk_info.axi_path[i].mnoc_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ib_bw -=
			ctx_data->clk_info.axi_path[i].mnoc_ib_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ab_bw -=
			ctx_data->clk_info.axi_path[i].ddr_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ib_bw -=
			ctx_data->clk_info.axi_path[i].ddr_ib_bw;
	}

	rc = cam_cre_update_cpas_vote(hw_mgr, ctx_data);

	return rc;
}

static bool cam_cre_update_bw_v2(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data,
	struct cam_cre_clk_info *hw_mgr_clk_info,
	struct cam_cre_clk_bw_req_internal_v2 *clk_info,
	bool busy)
{
	int i, path_index;
	bool update_required = true;

	for (i = 0; i < clk_info->num_paths; i++)
		CAM_DBG(CAM_CRE, "clk_info camnoc = %lld busy = %d",
			clk_info->axi_path[i].camnoc_bw, busy);

	if (clk_info->num_paths == ctx_data->clk_info.num_paths) {
		update_required = false;
		for (i = 0; i < clk_info->num_paths; i++) {
			if ((clk_info->axi_path[i].transac_type ==
			ctx_data->clk_info.axi_path[i].transac_type) &&
				(clk_info->axi_path[i].path_data_type ==
			ctx_data->clk_info.axi_path[i].path_data_type) &&
				(clk_info->axi_path[i].camnoc_bw ==
			ctx_data->clk_info.axi_path[i].camnoc_bw) &&
				(clk_info->axi_path[i].mnoc_ab_bw ==
			ctx_data->clk_info.axi_path[i].mnoc_ab_bw)) {
				continue;
			} else {
				update_required = true;
				break;
			}
		}
	}
	if (!update_required) {
		CAM_DBG(CAM_CRE,
		"Incoming BW hasn't changed, no update required");
		return false;
	}

	/*
	 * Remove previous vote of this context from hw mgr first.
	 * hw_mgr_clk_info has all valid paths, with each path in its own index
	 */
	for (i = 0; i < ctx_data->clk_info.num_paths; i++) {
		path_index =
		ctx_data->clk_info.axi_path[i].path_data_type -
			CAM_AXI_PATH_DATA_CRE_START_OFFSET;

		if (path_index >= CAM_CRE_MAX_PER_PATH_VOTES) {
			CAM_WARN(CAM_CRE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i].path_data_type,
				CAM_AXI_PATH_DATA_CRE_START_OFFSET,
				CAM_CRE_MAX_PER_PATH_VOTES);
			continue;
		}

		hw_mgr_clk_info->axi_path[path_index].camnoc_bw -=
			ctx_data->clk_info.axi_path[i].camnoc_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ab_bw -=
			ctx_data->clk_info.axi_path[i].mnoc_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ib_bw -=
			ctx_data->clk_info.axi_path[i].mnoc_ib_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ab_bw -=
			ctx_data->clk_info.axi_path[i].ddr_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ib_bw -=
			ctx_data->clk_info.axi_path[i].ddr_ib_bw;
	}

	ctx_data->clk_info.num_paths =
		cam_cre_mgr_calculate_num_path(clk_info, ctx_data);

	memcpy(&ctx_data->clk_info.axi_path[0],
		&clk_info->axi_path[0],
		clk_info->num_paths * sizeof(struct cam_axi_per_path_bw_vote));

	/*
	 * Add new vote of this context in hw mgr.
	 * hw_mgr_clk_info has all paths, with each path in its own index
	 */
	for (i = 0; i < ctx_data->clk_info.num_paths; i++) {
		path_index =
		ctx_data->clk_info.axi_path[i].path_data_type -
			CAM_AXI_PATH_DATA_CRE_START_OFFSET;

		if (path_index >= CAM_CRE_MAX_PER_PATH_VOTES) {
			CAM_WARN(CAM_CRE,
				"Invalid path %d, start offset=%d, max=%d",
				ctx_data->clk_info.axi_path[i].path_data_type,
				CAM_AXI_PATH_DATA_CRE_START_OFFSET,
				CAM_CRE_MAX_PER_PATH_VOTES);
			continue;
		}

		hw_mgr_clk_info->axi_path[path_index].path_data_type =
			ctx_data->clk_info.axi_path[i].path_data_type;
		hw_mgr_clk_info->axi_path[path_index].transac_type =
			ctx_data->clk_info.axi_path[i].transac_type;
		hw_mgr_clk_info->axi_path[path_index].camnoc_bw +=
			ctx_data->clk_info.axi_path[i].camnoc_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ab_bw +=
			ctx_data->clk_info.axi_path[i].mnoc_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].mnoc_ib_bw +=
			ctx_data->clk_info.axi_path[i].mnoc_ib_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ab_bw +=
			ctx_data->clk_info.axi_path[i].ddr_ab_bw;
		hw_mgr_clk_info->axi_path[path_index].ddr_ib_bw +=
			ctx_data->clk_info.axi_path[i].ddr_ib_bw;
		CAM_DBG(CAM_CRE,
			"Consolidate Path Vote : Dev[%s] i[%d] path_idx[%d] : [%s %s] [%lld %lld]",
			ctx_data->cre_acquire.dev_name,
			i, path_index,
			cam_cpas_axi_util_trans_type_to_string(
			hw_mgr_clk_info->axi_path[path_index].transac_type),
			cam_cpas_axi_util_path_type_to_string(
			hw_mgr_clk_info->axi_path[path_index].path_data_type),
			hw_mgr_clk_info->axi_path[path_index].camnoc_bw,
			hw_mgr_clk_info->axi_path[path_index].mnoc_ab_bw);
	}

	if (hw_mgr_clk_info->num_paths < ctx_data->clk_info.num_paths)
		hw_mgr_clk_info->num_paths = ctx_data->clk_info.num_paths;

	return true;
}

static bool cam_cre_check_bw_update(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data, int idx)
{
	bool busy = false, bw_updated = false;
	int i;
	struct cam_cre_clk_bw_req_internal_v2 *clk_info_v2;
	struct cam_cre_clk_info *hw_mgr_clk_info;
	uint64_t req_id;

	hw_mgr_clk_info = &hw_mgr->clk_info;
	req_id = ctx_data->req_list[idx]->request_id;
	if (ctx_data->req_cnt > 1)
		busy = true;

	clk_info_v2 = &ctx_data->req_list[idx]->clk_info_v2;

	bw_updated = cam_cre_update_bw_v2(hw_mgr, ctx_data,
		hw_mgr_clk_info, clk_info_v2, busy);
	for (i = 0; i < hw_mgr_clk_info->num_paths; i++) {
		CAM_DBG(CAM_CRE,
			"Final path_type: %s, transac_type: %s, camnoc_bw = %lld mnoc_ab_bw = %lld, mnoc_ib_bw = %lld, device: %s",
			cam_cpas_axi_util_path_type_to_string(
			hw_mgr_clk_info->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			hw_mgr_clk_info->axi_path[i].transac_type),
			hw_mgr_clk_info->axi_path[i].camnoc_bw,
			hw_mgr_clk_info->axi_path[i].mnoc_ab_bw,
			hw_mgr_clk_info->axi_path[i].mnoc_ib_bw,
			ctx_data->cre_acquire.dev_name);
	}

	return bw_updated;
}

static int cam_cre_mgr_handle_config_err(
	struct cam_hw_config_args *config_args,
	struct cam_cre_ctx *ctx_data)
{
	struct cam_hw_done_event_data err_data;
	struct cam_cre_request *cre_req;
	uint32_t req_idx;

	cre_req = config_args->priv;

	err_data.request_id = cre_req->request_id;
	err_data.evt_param = CAM_SYNC_CRE_EVENT_CONFIG_ERR;
	ctx_data->ctxt_event_cb(ctx_data->context_priv, CAM_CTX_EVT_ID_ERROR,
		&err_data);

	req_idx = cre_req->req_idx;
	cre_req->request_id = 0;
	cam_cre_free_io_config(ctx_data->req_list[req_idx]);
	cam_free_clear(ctx_data->req_list[req_idx]);
	ctx_data->req_list[req_idx] = NULL;
	clear_bit(req_idx, ctx_data->bitmap);
	return 0;
}

static bool cam_cre_is_pending_request(struct cam_cre_ctx *ctx_data)
{
	return !bitmap_empty(ctx_data->bitmap, CAM_CTX_REQ_MAX);
}

static int cam_cre_supported_clk_rates(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data)
{
	int i;
	struct cam_hw_soc_info *soc_info;
	struct cam_hw_intf *dev_intf = NULL;
	struct cam_hw_info *dev = NULL;

	dev_intf = hw_mgr->cre_dev_intf[0];
	if (!dev_intf) {
		CAM_ERR(CAM_CRE, "dev_intf is invalid");
		return -EINVAL;
	}

	dev = (struct cam_hw_info *)dev_intf->hw_priv;
	soc_info = &dev->soc_info;

	for (i = 0; i < CAM_MAX_VOTE; i++) {
		ctx_data->clk_info.clk_rate[i] =
			soc_info->clk_rate[i][soc_info->src_clk_idx];
		CAM_DBG(CAM_CRE, "clk_info[%d] = %d",
			i, ctx_data->clk_info.clk_rate[i]);
	}

	return 0;
}

static int cam_cre_ctx_clk_info_init(struct cam_cre_ctx *ctx_data)
{
	int i;

	ctx_data->clk_info.curr_fc = 0;
	ctx_data->clk_info.base_clk = 0;

	for (i = 0; i < CAM_CRE_MAX_PER_PATH_VOTES; i++) {
		ctx_data->clk_info.axi_path[i].camnoc_bw = 0;
		ctx_data->clk_info.axi_path[i].mnoc_ab_bw = 0;
		ctx_data->clk_info.axi_path[i].mnoc_ib_bw = 0;
	}

	cam_cre_supported_clk_rates(cre_hw_mgr, ctx_data);

	return 0;
}

static int32_t cam_cre_deinit_idle_clk(void *priv, void *data)
{
	struct cam_cre_hw_mgr *hw_mgr = (struct cam_cre_hw_mgr *)priv;
	struct cre_clk_work_data *task_data = (struct cre_clk_work_data *)data;
	struct cam_cre_clk_info *clk_info =
		(struct cam_cre_clk_info *)task_data->data;
	uint32_t id;
	uint32_t i;
	struct cam_cre_ctx *ctx_data;
	struct cam_hw_intf *dev_intf = NULL;
	int rc = 0;
	bool busy = false;

	clk_info->base_clk = 0;
	clk_info->curr_clk = 0;
	clk_info->over_clked = 0;

	mutex_lock(&hw_mgr->hw_mgr_mutex);

	for (i = 0; i < CRE_CTX_MAX; i++) {
		ctx_data = &hw_mgr->ctx[i];
		mutex_lock(&ctx_data->ctx_mutex);
		if (ctx_data->ctx_state == CRE_CTX_STATE_ACQUIRED) {
			busy = cam_cre_is_pending_request(ctx_data);
			if (busy) {
				mutex_unlock(&ctx_data->ctx_mutex);
				break;
			}
			cam_cre_ctx_clk_info_init(ctx_data);
		}
		mutex_unlock(&ctx_data->ctx_mutex);
	}

	if (busy) {
		cam_cre_device_timer_reset(hw_mgr);
		rc = -EBUSY;
		goto done;
	}

	dev_intf = hw_mgr->cre_dev_intf[0];
	id = CRE_HW_CLK_DISABLE;

	CAM_DBG(CAM_CRE, "Disable %d", clk_info->hw_type);

	dev_intf->hw_ops.process_cmd(dev_intf->hw_priv, id, NULL, 0);

done:
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static void cam_cre_device_timer_cb(struct timer_list *timer_data)
{
	unsigned long flags;
	struct crm_workq_task *task;
	struct cre_clk_work_data *task_data;
	struct cam_req_mgr_timer *timer =
		container_of(timer_data, struct cam_req_mgr_timer, sys_timer);

	spin_lock_irqsave(&cre_hw_mgr->hw_mgr_lock, flags);
	task = cam_req_mgr_workq_get_task(cre_hw_mgr->timer_work);
	if (!task) {
		CAM_ERR(CAM_CRE, "no empty task");
		spin_unlock_irqrestore(&cre_hw_mgr->hw_mgr_lock, flags);
		return;
	}

	task_data = (struct cre_clk_work_data *)task->payload;
	task_data->data = timer->parent;
	task_data->type = CRE_WORKQ_TASK_MSG_TYPE;
	task->process_cb = cam_cre_deinit_idle_clk;
	cam_req_mgr_workq_enqueue_task(task, cre_hw_mgr,
		CRM_TASK_PRIORITY_0);
	spin_unlock_irqrestore(&cre_hw_mgr->hw_mgr_lock, flags);
}

static int cam_cre_device_timer_start(struct cam_cre_hw_mgr *hw_mgr)
{
	int rc = 0;

	if (!hw_mgr->clk_info.watch_dog) {
		rc = crm_timer_init(&hw_mgr->clk_info.watch_dog,
			CRE_DEVICE_IDLE_TIMEOUT, &hw_mgr->clk_info,
			&cam_cre_device_timer_cb);
		if (rc)
			CAM_ERR(CAM_CRE, "Failed to start timer");
		hw_mgr->clk_info.watch_dog_reset_counter = 0;
	}

	return rc;
}

static void cam_cre_device_timer_stop(struct cam_cre_hw_mgr *hw_mgr)
{
	if (hw_mgr->clk_info.watch_dog) {
		hw_mgr->clk_info.watch_dog_reset_counter = 0;
		crm_timer_exit(&hw_mgr->clk_info.watch_dog);
		hw_mgr->clk_info.watch_dog = NULL;
	}
}

static int cam_cre_mgr_process_cmd(void *priv, void *data)
{
	int rc = 0, i = 0;
	struct cre_cmd_work_data *task_data = NULL;
	struct cam_cre_ctx *ctx_data;
	struct cam_cre_request *cre_req;
	struct cam_cre_hw_mgr *hw_mgr = cre_hw_mgr;
	uint32_t num_batch;

	if (!data || !priv) {
		CAM_ERR(CAM_CRE, "Invalid params%pK %pK", data, priv);
		return -EINVAL;
	}

	ctx_data = priv;
	task_data = (struct cre_cmd_work_data *)data;

	mutex_lock(&hw_mgr->hw_mgr_mutex);

	if (ctx_data->ctx_state != CRE_CTX_STATE_ACQUIRED) {
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_CRE, "ctx id :%u is not in use",
			ctx_data->ctx_id);
		return -EINVAL;
	}

	if (task_data->req_idx >= CAM_CTX_REQ_MAX) {
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_CRE, "Invalid reqIdx = %llu",
				task_data->req_idx);
		return -EINVAL;
	}

	cre_req = ctx_data->req_list[task_data->req_idx];
	if (cre_req->request_id > ctx_data->last_flush_req)
		ctx_data->last_flush_req = 0;

	if (cre_req->request_id <= ctx_data->last_flush_req) {
		CAM_WARN(CAM_CRE,
			"request %lld has been flushed, reject packet",
			cre_req->request_id, ctx_data->last_flush_req);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}

	if (!cam_cre_is_pending_request(ctx_data)) {
		CAM_WARN(CAM_CRE, "no pending req, req %lld last flush %lld",
			cre_req->request_id, ctx_data->last_flush_req);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return -EINVAL;
	}
	hw_mgr = task_data->data;
	num_batch = cre_req->num_batch;

	CAM_DBG(CAM_CRE,
		"Going to configure cre for req %d, req_idx %d num_batch %d",
		cre_req->request_id, cre_req->req_idx, num_batch);

	for (i = 0; i < num_batch; i++) {
		if (i != 0) {
			rc = cam_common_wait_for_completion_timeout(
					&ctx_data->cre_top->bufdone,
					msecs_to_jiffies(100));
			if (!rc) {
				cam_cre_device_timer_reset(cre_hw_mgr);
				CAM_ERR(CAM_CRE,
					"Timedout waiting for bufdone on last frame");
				return -ETIMEDOUT;
			} else {
				reinit_completion(&ctx_data->cre_top->bufdone);
				CAM_INFO(CAM_CRE,
					"done for frame %d in batch of %d",
					i-1, num_batch);
			}
		}

		cam_cre_mgr_update_reg_set(hw_mgr, cre_req, i);
		cam_cre_ctx_wait_for_idle_irq(ctx_data);
	}
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return rc;
}

static int cam_get_valid_ctx_id(void)
{
	struct cam_cre_hw_mgr *hw_mgr = cre_hw_mgr;
	int i;

	for (i = 0; i < CRE_CTX_MAX; i++) {
		if (hw_mgr->ctx[i].ctx_state == CRE_CTX_STATE_ACQUIRED)
			break;
	}

	if (i == CRE_CTX_MAX)
		return -EINVAL;

	return i;
}

static int32_t cam_cre_mgr_process_msg(void *priv, void *data)
{
	struct cre_msg_work_data *task_data;
	struct cam_hw_done_event_data buf_data;
	struct cam_cre_hw_mgr *hw_mgr;
	struct cam_cre_ctx *ctx;
	struct cam_cre_request *active_req;
	struct cam_cre_irq_data irq_data;
	int32_t ctx_id;
	uint32_t evt_id;
	uint32_t active_req_idx;
	int rc = 0;

	if (!data || !priv) {
		CAM_ERR(CAM_CRE, "Invalid data");
		return -EINVAL;
	}

	task_data = data;
	hw_mgr = priv;
	ctx_id = cam_get_valid_ctx_id();
	if (ctx_id < 0) {
		CAM_ERR(CAM_CRE, "No valid context to handle error");
		return ctx_id;
	}

	ctx = &hw_mgr->ctx[ctx_id];

	mutex_lock(&ctx->ctx_mutex);
	irq_data = task_data->irq_data;
	if (ctx->ctx_state != CRE_CTX_STATE_ACQUIRED) {
		CAM_DBG(CAM_CRE, "ctx id: %d not in right state: %d",
			ctx_id, ctx->ctx_state);
		mutex_unlock(&ctx->ctx_mutex);
		return -EINVAL;
	}

	active_req_idx = find_next_bit(ctx->bitmap, ctx->bits, ctx->last_done_req_idx);
	CAM_DBG(CAM_CRE, "active_req_idx %d last_done_req_idx %d",
		active_req_idx, ctx->last_done_req_idx);

	active_req = ctx->req_list[active_req_idx];
	if (!active_req)
		CAM_ERR(CAM_CRE, "Active req cannot be null");

	if (irq_data.error) {
		evt_id = CAM_CTX_EVT_ID_ERROR;
		buf_data.evt_param = CAM_SYNC_CRE_EVENT_HW_ERR;
		buf_data.request_id = active_req->request_id;
		ctx->ctxt_event_cb(ctx->context_priv, evt_id, &buf_data);
		rc = cam_cre_mgr_reset_hw();
		clear_bit(active_req_idx, ctx->bitmap);
		cam_cre_free_io_config(active_req);
		cam_free_clear((void *)active_req);
		ctx->req_cnt--;
		ctx->req_list[active_req_idx] = NULL;
	} else if (irq_data.wr_buf_done) {
		/* Signal Buf done */
		active_req->frames_done++;
		CAM_DBG(CAM_CRE, "Received frames_done %d num_batch %d req id %d",
			active_req->frames_done, active_req->num_batch,
			active_req->request_id);
		complete(&ctx->cre_top->bufdone);
		if (active_req->frames_done == active_req->num_batch) {
			ctx->last_done_req_idx = active_req_idx;
			CAM_DBG(CAM_CRE, "signaling buff done for req %d",
				active_req->request_id);
			evt_id = CAM_CTX_EVT_ID_SUCCESS;
			buf_data.evt_param = CAM_SYNC_COMMON_EVENT_SUCCESS;
			buf_data.request_id = active_req->request_id;
			ctx->ctxt_event_cb(ctx->context_priv, evt_id, &buf_data);
			clear_bit(active_req_idx, ctx->bitmap);
			cam_cre_free_io_config(active_req);
			cam_free_clear((void *)active_req);
			ctx->req_cnt--;
			ctx->req_list[active_req_idx] = NULL;
		}
	}
	mutex_unlock(&ctx->ctx_mutex);
	return rc;
}

static int cam_cre_get_actual_clk_rate_idx(
	struct cam_cre_ctx *ctx_data, uint32_t base_clk)
{
	int i;

	for (i = 0; i < CAM_MAX_VOTE; i++)
		if (ctx_data->clk_info.clk_rate[i] >= base_clk)
			return i;

	/*
	 * Caller has to ensure returned index is within array
	 * size bounds while accessing that index.
	 */

	return i;
}

static bool cam_cre_is_over_clk(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data,
	struct cam_cre_clk_info *hw_mgr_clk_info)
{
	int base_clk_idx;
	int curr_clk_idx;

	base_clk_idx = cam_cre_get_actual_clk_rate_idx(ctx_data,
		hw_mgr_clk_info->base_clk);

	curr_clk_idx = cam_cre_get_actual_clk_rate_idx(ctx_data,
		hw_mgr_clk_info->curr_clk);

	CAM_DBG(CAM_CRE, "bc_idx = %d cc_idx = %d %d %d",
		base_clk_idx, curr_clk_idx, hw_mgr_clk_info->base_clk,
		hw_mgr_clk_info->curr_clk);

	if (curr_clk_idx > base_clk_idx)
		return true;

	return false;
}

static int cam_cre_get_lower_clk_rate(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data, uint32_t base_clk)
{
	int i;

	i = cam_cre_get_actual_clk_rate_idx(ctx_data, base_clk);

	while (i > 0) {
		if (ctx_data->clk_info.clk_rate[i - 1])
			return ctx_data->clk_info.clk_rate[i - 1];
		i--;
	}

	CAM_DBG(CAM_CRE, "Already clk at lower level");

	return base_clk;
}

static int cam_cre_get_next_clk_rate(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data, uint32_t base_clk)
{
	int i;

	i = cam_cre_get_actual_clk_rate_idx(ctx_data, base_clk);

	while (i < CAM_MAX_VOTE - 1) {
		if (ctx_data->clk_info.clk_rate[i + 1])
			return ctx_data->clk_info.clk_rate[i + 1];
		i++;
	}

	CAM_DBG(CAM_CRE, "Already clk at higher level");
	return base_clk;
}

static bool cam_cre_update_clk_overclk_free(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data,
	struct cam_cre_clk_info *hw_mgr_clk_info,
	struct cam_cre_clk_bw_request *clk_info,
	uint32_t base_clk)
{
	int rc = false;

	/*
	 * In caseof no pending packets case
	 *    1. In caseof overclk cnt is less than threshold, increase
	 *       overclk count and no update in the clock rate
	 *    2. In caseof overclk cnt is greater than or equal to threshold
	 *       then lower clock rate by one level and update hw_mgr current
	 *       clock value.
	 *        a. In case of new clock rate greater than sum of clock
	 *           rates, reset overclk count value to zero if it is
	 *           overclock
	 *        b. if it is less than sum of base clocks then go to next
	 *           level of clock and make overclk count to zero
	 *        c. if it is same as sum of base clock rates update overclock
	 *           cnt to 0
	 */
	if (hw_mgr_clk_info->over_clked < hw_mgr_clk_info->threshold) {
		hw_mgr_clk_info->over_clked++;
		rc = false;
	} else {
		hw_mgr_clk_info->curr_clk =
			cam_cre_get_lower_clk_rate(hw_mgr, ctx_data,
			hw_mgr_clk_info->curr_clk);
		if (hw_mgr_clk_info->curr_clk > hw_mgr_clk_info->base_clk) {
			if (cam_cre_is_over_clk(hw_mgr, ctx_data,
				hw_mgr_clk_info))
				hw_mgr_clk_info->over_clked = 0;
		} else if (hw_mgr_clk_info->curr_clk <
			hw_mgr_clk_info->base_clk) {
			hw_mgr_clk_info->curr_clk =
			cam_cre_get_next_clk_rate(hw_mgr, ctx_data,
				hw_mgr_clk_info->curr_clk);
				hw_mgr_clk_info->over_clked = 0;
		} else if (hw_mgr_clk_info->curr_clk ==
			hw_mgr_clk_info->base_clk) {
			hw_mgr_clk_info->over_clked = 0;
		}
		rc = true;
	}

	return rc;
}

static int cam_cre_calc_total_clk(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_clk_info *hw_mgr_clk_info, uint32_t dev_type)
{
	int i;
	struct cam_cre_ctx *ctx_data;

	hw_mgr_clk_info->base_clk = 0;
	for (i = 0; i < CRE_CTX_MAX; i++) {
		ctx_data = &hw_mgr->ctx[i];
		if (ctx_data->ctx_state == CRE_CTX_STATE_ACQUIRED)
			hw_mgr_clk_info->base_clk +=
				ctx_data->clk_info.base_clk;
	}

	return 0;
}

static int cam_cre_get_actual_clk_rate(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data, uint32_t base_clk)
{
	int i;

	for (i = 0; i < CAM_MAX_VOTE; i++)
		if (ctx_data->clk_info.clk_rate[i] >= base_clk)
			return ctx_data->clk_info.clk_rate[i];

	return base_clk;
}

static bool cam_cre_update_clk_busy(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data,
	struct cam_cre_clk_info *hw_mgr_clk_info,
	struct cam_cre_clk_bw_request *clk_info,
	uint32_t base_clk)
{
	uint32_t next_clk_level;
	uint32_t actual_clk;
	bool rc = false;

	/* 1. if current request frame cycles(fc) are more than previous
	 *      frame fc
	 *      Calculate the new base clock.
	 *      if sum of base clocks are more than next available clk level
	 *       Update clock rate, change curr_clk_rate to sum of base clock
	 *       rates and make over_clked to zero
	 *      else
	 *       Update clock rate to next level, update curr_clk_rate and make
	 *       overclked cnt to zero
	 * 2. if current fc is less than or equal to previous  frame fc
	 *      Still Bump up the clock to next available level
	 *      if it is available, then update clock, make overclk cnt to
	 *      zero. If the clock is already at highest clock rate then
	 *      no need to update the clock
	 */
	ctx_data->clk_info.base_clk = base_clk;
	hw_mgr_clk_info->over_clked = 0;
	if (clk_info->frame_cycles > ctx_data->clk_info.curr_fc) {
		cam_cre_calc_total_clk(hw_mgr, hw_mgr_clk_info,
			ctx_data->cre_acquire.dev_type);
		actual_clk = cam_cre_get_actual_clk_rate(hw_mgr,
			ctx_data, base_clk);
		if (hw_mgr_clk_info->base_clk > actual_clk) {
			hw_mgr_clk_info->curr_clk = hw_mgr_clk_info->base_clk;
		} else {
			next_clk_level = cam_cre_get_next_clk_rate(hw_mgr,
				ctx_data, hw_mgr_clk_info->curr_clk);
			hw_mgr_clk_info->curr_clk = next_clk_level;
		}
		rc = true;
	} else {
		next_clk_level =
			cam_cre_get_next_clk_rate(hw_mgr, ctx_data,
				hw_mgr_clk_info->curr_clk);
		if (hw_mgr_clk_info->curr_clk < next_clk_level) {
			hw_mgr_clk_info->curr_clk = next_clk_level;
			rc = true;
		}
	}
	ctx_data->clk_info.curr_fc = clk_info->frame_cycles;

	return rc;
}

static bool cam_cre_update_clk_free(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data,
	struct cam_cre_clk_info *hw_mgr_clk_info,
	struct cam_cre_clk_bw_request *clk_info,
	uint32_t base_clk)
{
	int rc = false;
	bool over_clocked = false;

	ctx_data->clk_info.curr_fc = clk_info->frame_cycles;
	ctx_data->clk_info.base_clk = base_clk;
	cam_cre_calc_total_clk(hw_mgr, hw_mgr_clk_info,
		ctx_data->cre_acquire.dev_type);

	/*
	 * Current clock is not always sum of base clocks, due to
	 * clock scales update to next higher or lower levels, it
	 * equals to one of discrete clock values supported by hardware.
	 * So even current clock is higher than sum of base clocks, we
	 * can not consider it is over clocked. if it is greater than
	 * discrete clock level then only it is considered as over clock.
	 * 1. Handle over clock case
	 * 2. If current clock is less than sum of base clocks
	 *    update current clock
	 * 3. If current clock is same as sum of base clocks no action
	 */

	over_clocked = cam_cre_is_over_clk(hw_mgr, ctx_data,
		hw_mgr_clk_info);

	if (hw_mgr_clk_info->curr_clk > hw_mgr_clk_info->base_clk &&
		over_clocked) {
		rc = cam_cre_update_clk_overclk_free(hw_mgr, ctx_data,
			hw_mgr_clk_info, clk_info, base_clk);
	} else if (hw_mgr_clk_info->curr_clk > hw_mgr_clk_info->base_clk) {
		hw_mgr_clk_info->over_clked = 0;
		rc = false;
	}  else if (hw_mgr_clk_info->curr_clk < hw_mgr_clk_info->base_clk) {
		hw_mgr_clk_info->curr_clk = cam_cre_get_actual_clk_rate(hw_mgr,
			ctx_data, hw_mgr_clk_info->base_clk);
		rc = true;
	}

	return rc;
}

static uint32_t cam_cre_mgr_calc_base_clk(uint32_t frame_cycles,
	uint64_t budget)
{
	uint64_t mul = 1000000000;
	uint64_t base_clk = frame_cycles * mul;

	do_div(base_clk, budget);

	CAM_DBG(CAM_CRE, "budget = %lld fc = %d ib = %lld base_clk = %lld",
		budget, frame_cycles,
		(long long)(frame_cycles * mul), base_clk);

	return base_clk;
}

static bool cam_cre_check_clk_update(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data, int idx)
{
	bool busy = false, rc = false;
	uint64_t base_clk;
	struct cam_cre_clk_bw_request *clk_info;
	uint64_t req_id;
	struct cam_cre_clk_info *hw_mgr_clk_info;

	cam_cre_device_timer_reset(hw_mgr);
	hw_mgr_clk_info = &hw_mgr->clk_info;
	req_id = ctx_data->req_list[idx]->request_id;
	if (ctx_data->req_cnt > 1)
		busy = true;

	CAM_DBG(CAM_CRE, "busy = %d req_id = %lld", busy, req_id);

	clk_info = &ctx_data->req_list[idx]->clk_info;

	/* Calculate base clk rate */
	base_clk = cam_cre_mgr_calc_base_clk(
		clk_info->frame_cycles, clk_info->budget_ns);
	ctx_data->clk_info.rt_flag = clk_info->rt_flag;

	if (cre_hw_mgr->cre_debug_clk)
		return cam_cre_debug_clk_update(hw_mgr_clk_info);

	if (busy)
		rc = cam_cre_update_clk_busy(hw_mgr, ctx_data,
			hw_mgr_clk_info, clk_info, base_clk);
	else
		rc = cam_cre_update_clk_free(hw_mgr, ctx_data,
			hw_mgr_clk_info, clk_info, base_clk);

	CAM_DBG(CAM_CRE, "bc = %d cc = %d busy = %d overclk = %d uc = %d",
		hw_mgr_clk_info->base_clk, hw_mgr_clk_info->curr_clk,
		busy, hw_mgr_clk_info->over_clked, rc);

	return rc;
}

static int cam_cre_mgr_update_clk_rate(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data)
{
	struct cam_cre_dev_clk_update clk_upd_cmd;
	int i;

	clk_upd_cmd.clk_rate = hw_mgr->clk_info.curr_clk;

	CAM_DBG(CAM_PERF, "clk_rate %u for dev_type %d", clk_upd_cmd.clk_rate,
		ctx_data->cre_acquire.dev_type);

	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv,
			CRE_HW_CLK_UPDATE,
			&clk_upd_cmd, sizeof(clk_upd_cmd));
	}

	return 0;
}

static int cam_cre_mgr_cre_clk_remove(struct cam_cre_hw_mgr *hw_mgr, int ctx_id)
{
	struct cam_cre_ctx *ctx_data = NULL;
	struct cam_cre_clk_info *hw_mgr_clk_info;

	ctx_data = &hw_mgr->ctx[ctx_id];
	hw_mgr_clk_info = &hw_mgr->clk_info;

	if (hw_mgr_clk_info->base_clk >= ctx_data->clk_info.base_clk)
		hw_mgr_clk_info->base_clk -= ctx_data->clk_info.base_clk;

	/* reset clock info */
	ctx_data->clk_info.curr_fc = 0;
	ctx_data->clk_info.base_clk = 0;
	return 0;
}


static int cam_cre_mgr_cre_clk_update(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data, int idx)
{
	int rc = 0;

	if (cam_cre_check_clk_update(hw_mgr, ctx_data, idx))
		rc = cam_cre_mgr_update_clk_rate(hw_mgr, ctx_data);

	if (cam_cre_check_bw_update(hw_mgr, ctx_data, idx))
		rc |= cam_cre_update_cpas_vote(hw_mgr, ctx_data);

	return rc;
}

int32_t cam_cre_hw_mgr_cb(void *irq_data, int32_t result_size, void *data)
{
	int32_t rc = 0;
	unsigned long flags;
	struct cam_cre_hw_mgr *hw_mgr = data;
	struct crm_workq_task *task;
	struct cre_msg_work_data *task_data;
	struct cam_cre_irq_data *local_irq_data = irq_data;

	if (!data) {
		CAM_ERR(CAM_CRE, "irq cb data is NULL");
		return rc;
	}

	spin_lock_irqsave(&hw_mgr->hw_mgr_lock, flags);
	task = cam_req_mgr_workq_get_task(cre_hw_mgr->msg_work);
	if (!task) {
		CAM_ERR(CAM_CRE, "no empty task");
		spin_unlock_irqrestore(&hw_mgr->hw_mgr_lock, flags);
		return -ENOMEM;
	}

	task_data = (struct cre_msg_work_data *)task->payload;
	task_data->data = hw_mgr;
	task_data->irq_data = *local_irq_data;
	task_data->type = CRE_WORKQ_TASK_MSG_TYPE;
	task->process_cb = cam_cre_mgr_process_msg;
	rc = cam_req_mgr_workq_enqueue_task(task, cre_hw_mgr,
		CRM_TASK_PRIORITY_0);
	spin_unlock_irqrestore(&hw_mgr->hw_mgr_lock, flags);

	return rc;
}

static int cam_cre_mgr_process_io_cfg(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_packet *packet,
	struct cam_cre_ctx *ctx_data,
	uint32_t req_idx,
	struct cam_hw_prepare_update_args *prep_arg)
{
	int i, j = 0, k = 0, l, rc = 0;
	struct cre_io_buf *io_buf;
	int32_t sync_in_obj[CRE_MAX_IN_RES];
	int32_t merged_sync_in_obj;
	struct cam_cre_request *cre_request;

	prep_arg->pf_data->packet = packet;

	rc = cam_cre_mgr_process_cmd_io_buf_req(hw_mgr, packet, ctx_data,
		req_idx);
	if (rc) {
		CAM_ERR(CAM_CRE, "Process CRE cmd io request is failed: %d",
			rc);
		goto end;
	}

	cre_request = ctx_data->req_list[req_idx];
	prep_arg->num_out_map_entries = 0;
	prep_arg->num_in_map_entries = 0;

	CAM_DBG(CAM_CRE, "E: req_idx = %u %x num batch%d",
			req_idx, packet, cre_request->num_batch);

	for (i = 0; i < cre_request->num_batch; i++) {
		for (l = 0; l < cre_request->num_io_bufs[i]; l++) {
			io_buf = cre_request->io_buf[i][l];
			if (io_buf->direction == CAM_BUF_INPUT) {
				if (io_buf->fence != -1) {
					if (j < CRE_MAX_IN_RES) {
						sync_in_obj[j++] =
							io_buf->fence;
						prep_arg->num_in_map_entries++;
					} else {
						CAM_ERR(CAM_CRE,
						"reached max in_res %d %d",
						io_buf->resource_type,
						cre_request->request_id);
					}
				} else {
					CAM_ERR(CAM_CRE, "Invalid fence %d %d",
						io_buf->resource_type,
						cre_request->request_id);
				}
			} else {
				if (io_buf->fence != -1) {
					prep_arg->out_map_entries[k].sync_id =
						io_buf->fence;
					k++;
					prep_arg->num_out_map_entries++;
				}
			}
			CAM_DBG(CAM_CRE,
				"ctx_id: %u req_id: %llu dir[%d] %u, fence: %d",
				ctx_data->ctx_id, packet->header.request_id, i,
				io_buf->direction, io_buf->fence);
		}
	}

	if (prep_arg->num_in_map_entries > 1 &&
		prep_arg->num_in_map_entries <= CRE_MAX_IN_RES)
		prep_arg->num_in_map_entries =
			cam_common_util_remove_duplicate_arr(
			sync_in_obj, prep_arg->num_in_map_entries);

	if (prep_arg->num_in_map_entries > 1 &&
		prep_arg->num_in_map_entries <= CRE_MAX_IN_RES) {
		rc = cam_sync_merge(&sync_in_obj[0],
			prep_arg->num_in_map_entries, &merged_sync_in_obj);
		if (rc) {
			prep_arg->num_out_map_entries = 0;
			prep_arg->num_in_map_entries = 0;
			return rc;
		}

		cre_request->in_resource = merged_sync_in_obj;

		prep_arg->in_map_entries[0].sync_id = merged_sync_in_obj;
		prep_arg->num_in_map_entries = 1;
		CAM_DBG(CAM_CRE, "ctx_id: %u req_id: %llu Merged Sync obj: %d",
			ctx_data->ctx_id, packet->header.request_id,
			merged_sync_in_obj);
	} else if (prep_arg->num_in_map_entries == 1) {
		prep_arg->in_map_entries[0].sync_id = sync_in_obj[0];
		prep_arg->num_in_map_entries = 1;
		cre_request->in_resource = 0;
	} else {
		CAM_ERR(CAM_CRE,
			"Invalid count of input fences, count: %d",
			prep_arg->num_in_map_entries);
		prep_arg->num_in_map_entries = 0;
		cre_request->in_resource = 0;
		rc = -EINVAL;
	}
end:
	return rc;
}

static bool cam_cre_mgr_is_valid_inconfig(struct cam_packet *packet)
{
	int i, num_in_map_entries = 0;
	bool in_config_valid = false;
	struct cam_buf_io_cfg *io_cfg_ptr = NULL;

	io_cfg_ptr = (struct cam_buf_io_cfg *) ((uint32_t *) &packet->payload +
					packet->io_configs_offset/4);

	for (i = 0 ; i < packet->num_io_configs; i++)
		if (io_cfg_ptr[i].direction == CAM_BUF_INPUT)
			num_in_map_entries++;

	if (num_in_map_entries <= CRE_MAX_IN_RES) {
		in_config_valid = true;
	} else {
		CAM_ERR(CAM_CRE, "In config entries(%u) more than allowed(%u)",
				num_in_map_entries, CRE_MAX_IN_RES);
	}

	CAM_DBG(CAM_CRE, "number of in_config info: %u %u %u %u",
			packet->num_io_configs, CRE_MAX_IO_BUFS,
			num_in_map_entries, CRE_MAX_IN_RES);

	return in_config_valid;
}

static bool cam_cre_mgr_is_valid_outconfig(struct cam_packet *packet)
{
	int i, num_out_map_entries = 0;
	bool out_config_valid = false;
	struct cam_buf_io_cfg *io_cfg_ptr = NULL;

	io_cfg_ptr = (struct cam_buf_io_cfg *) ((uint32_t *) &packet->payload +
					packet->io_configs_offset/4);

	for (i = 0 ; i < packet->num_io_configs; i++)
		if (io_cfg_ptr[i].direction == CAM_BUF_OUTPUT)
			num_out_map_entries++;

	if (num_out_map_entries <= CRE_MAX_OUT_RES) {
		out_config_valid = true;
	} else {
		CAM_ERR(CAM_CRE, "Out config entries(%u) more than allowed(%u)",
				num_out_map_entries, CRE_MAX_OUT_RES);
	}

	CAM_DBG(CAM_CRE, "number of out_config info: %u %u %u %u",
			packet->num_io_configs, CRE_MAX_IO_BUFS,
			num_out_map_entries, CRE_MAX_OUT_RES);

	return out_config_valid;
}

static int cam_cre_mgr_pkt_validation(struct cam_packet *packet)
{
	if ((packet->header.op_code & 0xff) !=
		CAM_CRE_OPCODE_CONFIG) {
		CAM_ERR(CAM_CRE, "Invalid Opcode in pkt: %d",
			packet->header.op_code & 0xff);
		return -EINVAL;
	}

	if (packet->num_io_configs > CRE_MAX_IO_BUFS) {
		CAM_ERR(CAM_CRE, "Invalid number of io configs: %d %d",
			CRE_MAX_IO_BUFS, packet->num_io_configs);
		return -EINVAL;
	}

	if (packet->num_cmd_buf > CRE_PACKET_MAX_CMD_BUFS) {
		CAM_ERR(CAM_CRE, "Invalid number of cmd buffers: %d %d",
			CRE_PACKET_MAX_CMD_BUFS, packet->num_cmd_buf);
		return -EINVAL;
	}

	if (!cam_cre_mgr_is_valid_inconfig(packet) ||
		!cam_cre_mgr_is_valid_outconfig(packet)) {
		return -EINVAL;
	}

	CAM_DBG(CAM_CRE, "number of cmd/patch info: %u %u %u %u",
			packet->num_cmd_buf,
			packet->num_io_configs, CRE_MAX_IO_BUFS,
			packet->num_patches);
	return 0;
}

static int cam_cre_validate_acquire_res_info(
	struct cam_cre_acquire_dev_info *cre_acquire)
{
	int i;

	if (cre_acquire->num_out_res > CRE_MAX_OUT_RES) {
		CAM_ERR(CAM_CRE, "num of out resources exceeding : %u",
			cre_acquire->num_out_res);
		return -EINVAL;
	}

	if (cre_acquire->num_in_res > CRE_MAX_IN_RES) {
		CAM_ERR(CAM_CRE, "num of in resources exceeding : %u",
			cre_acquire->num_in_res);
		return -EINVAL;
	}

	if (cre_acquire->dev_type >= CAM_CRE_DEV_TYPE_MAX) {
		CAM_ERR(CAM_CRE, "Invalid device type: %d",
			cre_acquire->dev_type);
		return -EINVAL;
	}

	for (i = 0; i < cre_acquire->num_in_res; i++) {
		switch (cre_acquire->in_res[i].format) {
			case CAM_FORMAT_MIPI_RAW_10:
			case CAM_FORMAT_MIPI_RAW_12:
			case CAM_FORMAT_MIPI_RAW_14:
			case CAM_FORMAT_MIPI_RAW_20:
			case CAM_FORMAT_PLAIN128:
				break;
			default:
				CAM_ERR(CAM_CRE, "Invalid input format %d",
					cre_acquire->in_res[i].format);
				return -EINVAL;
		}
	}

	for (i = 0; i < cre_acquire->num_out_res; i++) {
		switch (cre_acquire->out_res[i].format) {
			case CAM_FORMAT_PLAIN16_8:
			case CAM_FORMAT_PLAIN16_10:
			case CAM_FORMAT_PLAIN16_12:
			case CAM_FORMAT_PLAIN16_14:
			case CAM_FORMAT_PLAIN16_16:
			case CAM_FORMAT_PLAIN32_20:
			case CAM_FORMAT_PLAIN32:
			case CAM_FORMAT_PLAIN128:
				break;
			default:
				CAM_ERR(CAM_CRE, "Invalid output format %d",
					cre_acquire->out_res[i].format);
				return -EINVAL;
		}
	}

	return 0;
}

static int cam_cre_get_acquire_info(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_hw_acquire_args *args,
	struct cam_cre_ctx *ctx)
{
	int i = 0;

	if (args->num_acq > CRE_DEV_MAX) {
		CAM_ERR(CAM_CRE, "Invalid number of resources: %d",
			args->num_acq);
		return -EINVAL;
	}

	if (copy_from_user(&ctx->cre_acquire,
		(void __user *)args->acquire_info,
		sizeof(struct cam_cre_acquire_dev_info))) {
		CAM_ERR(CAM_CRE, "Failed in acquire");
		return -EFAULT;
	}

	CAM_DBG(CAM_CRE, "top: %u %s %u %u %u",
		ctx->cre_acquire.dev_type,
		ctx->cre_acquire.dev_name,
		ctx->cre_acquire.secure_mode,
		ctx->cre_acquire.num_in_res, ctx->cre_acquire.num_out_res);

	for (i = 0; i < ctx->cre_acquire.num_in_res; i++) {
		CAM_DBG(CAM_CRE, "IN: %u %u %u %u",
		ctx->cre_acquire.in_res[i].res_id,
		ctx->cre_acquire.in_res[i].width,
		ctx->cre_acquire.in_res[i].height,
		ctx->cre_acquire.in_res[i].format);
	}

	for (i = 0; i < ctx->cre_acquire.num_out_res; i++) {
		CAM_DBG(CAM_CRE, "OUT: %u %u %u %u",
		ctx->cre_acquire.out_res[i].res_id,
		ctx->cre_acquire.out_res[i].width,
		ctx->cre_acquire.out_res[i].height,
		ctx->cre_acquire.out_res[i].format);
	}

	if (cam_cre_validate_acquire_res_info(&ctx->cre_acquire))
		return -EINVAL;

	return 0;
}

static int cam_cre_get_free_ctx(struct cam_cre_hw_mgr *hw_mgr)
{
	int i;

	i = find_first_zero_bit(hw_mgr->ctx_bitmap, hw_mgr->ctx_bits);
	if (i >= CRE_CTX_MAX || i < 0) {
		CAM_ERR(CAM_CRE, "Invalid ctx id = %d", i);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->ctx[i].ctx_mutex);
	if (hw_mgr->ctx[i].ctx_state != CRE_CTX_STATE_FREE) {
		CAM_ERR(CAM_CRE, "Invalid ctx %d state %d",
			i, hw_mgr->ctx[i].ctx_state);
		mutex_unlock(&hw_mgr->ctx[i].ctx_mutex);
		return -EINVAL;
	}
	set_bit(i, hw_mgr->ctx_bitmap);
	mutex_unlock(&hw_mgr->ctx[i].ctx_mutex);

	return i;
}


static int cam_cre_put_free_ctx(struct cam_cre_hw_mgr *hw_mgr, uint32_t ctx_id)
{
	if (ctx_id >= CRE_CTX_MAX) {
		CAM_ERR(CAM_CRE, "Invalid ctx_id: %d", ctx_id);
		return 0;
	}

	hw_mgr->ctx[ctx_id].ctx_state = CRE_CTX_STATE_FREE;
	clear_bit(ctx_id, hw_mgr->ctx_bitmap);

	return 0;
}

static int cam_cre_mgr_get_hw_caps(void *hw_priv, void *hw_caps_args)
{
	struct cam_cre_hw_mgr *hw_mgr;
	struct cam_query_cap_cmd *query_cap = hw_caps_args;
	struct cam_cre_hw_ver hw_ver;
	int rc = 0, i;

	if (!hw_priv || !hw_caps_args) {
		CAM_ERR(CAM_CRE, "Invalid args: %x %x", hw_priv, hw_caps_args);
		return -EINVAL;
	}

	hw_mgr = hw_priv;
	mutex_lock(&hw_mgr->hw_mgr_mutex);
	if (copy_from_user(&hw_mgr->cre_caps,
		u64_to_user_ptr(query_cap->caps_handle),
		sizeof(struct cam_cre_query_cap_cmd))) {
		CAM_ERR(CAM_CRE, "copy_from_user failed: size = %d",
			sizeof(struct cam_cre_query_cap_cmd));
		rc = -EFAULT;
		goto end;
	}

	for (i = 0; i < hw_mgr->num_cre; i++) {
		rc = hw_mgr->cre_dev_intf[i]->hw_ops.get_hw_caps(
			hw_mgr->cre_dev_intf[i]->hw_priv,
			&hw_ver, sizeof(hw_ver));
		if (rc)
			goto end;

		hw_mgr->cre_caps.dev_ver[i] = hw_ver;
	}

	hw_mgr->cre_caps.dev_iommu_handle.non_secure = hw_mgr->iommu_hdl;
	hw_mgr->cre_caps.dev_iommu_handle.secure = hw_mgr->iommu_sec_hdl;

	CAM_DBG(CAM_CRE, "iommu sec %d iommu ns %d",
		hw_mgr->cre_caps.dev_iommu_handle.secure,
		hw_mgr->cre_caps.dev_iommu_handle.non_secure);

	if (copy_to_user(u64_to_user_ptr(query_cap->caps_handle),
		&hw_mgr->cre_caps, sizeof(struct cam_cre_query_cap_cmd))) {
		CAM_ERR(CAM_CRE, "copy_to_user failed: size = %d",
			sizeof(struct cam_cre_query_cap_cmd));
		rc = -EFAULT;
	}

end:
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static int cam_cre_mgr_acquire_hw(void *hw_priv, void *hw_acquire_args)
{
	int rc = 0, i;
	int ctx_id;
	struct cam_cre_hw_mgr *hw_mgr = hw_priv;
	struct cam_cre_ctx *ctx;
	struct cam_hw_acquire_args *args = hw_acquire_args;
	struct cam_cre_dev_acquire cre_dev_acquire;
	struct cam_cre_dev_release cre_dev_release;
	struct cam_cre_dev_init init;
	struct cam_cre_dev_clk_update clk_update;
	struct cam_cre_dev_bw_update *bw_update;
	struct cam_cre_set_irq_cb irq_cb;
	struct cam_hw_info *dev = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	int32_t idx;

	if ((!hw_priv) || (!hw_acquire_args)) {
		CAM_ERR(CAM_CRE, "Invalid args: %x %x",
			hw_priv, hw_acquire_args);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	ctx_id = cam_cre_get_free_ctx(hw_mgr);
	if (ctx_id < 0) {
		CAM_ERR(CAM_CRE, "No free ctx");
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return ctx_id;
	}

	ctx = &hw_mgr->ctx[ctx_id];
	ctx->ctx_id = ctx_id;
	mutex_lock(&ctx->ctx_mutex);
	rc = cam_cre_get_acquire_info(hw_mgr, args, ctx);
	if (rc < 0) {
		CAM_ERR(CAM_CRE, "get_acquire info failed: %d", rc);
		goto end;
	}

	if (!hw_mgr->cre_ctx_cnt) {
		for (i = 0; i < cre_hw_mgr->num_cre; i++) {
			rc = hw_mgr->cre_dev_intf[i]->hw_ops.init(
				hw_mgr->cre_dev_intf[i]->hw_priv, &init,
				sizeof(init));
			if (rc) {
				CAM_ERR(CAM_CRE, "CRE Dev init failed: %d", rc);
				goto end;
			}
		}

		/* Install IRQ CB */
		irq_cb.cre_hw_mgr_cb = cam_cre_hw_mgr_cb;
		irq_cb.data = hw_mgr;
		for (i = 0; i < cre_hw_mgr->num_cre; i++) {
			rc = hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
				hw_mgr->cre_dev_intf[i]->hw_priv,
				CRE_HW_SET_IRQ_CB,
				&irq_cb, sizeof(irq_cb));
			if (rc) {
				CAM_ERR(CAM_CRE, "CRE Dev init failed: %d", rc);
				goto cre_irq_set_failed;
			}
		}

		dev = (struct cam_hw_info *)hw_mgr->cre_dev_intf[0]->hw_priv;
		soc_info = &dev->soc_info;
		idx = soc_info->src_clk_idx;

		hw_mgr->clk_info.base_clk =
			soc_info->clk_rate[CAM_TURBO_VOTE][idx];
		hw_mgr->clk_info.threshold = 5;
		hw_mgr->clk_info.over_clked = 0;

		for (i = 0; i < CAM_CRE_MAX_PER_PATH_VOTES; i++) {
			hw_mgr->clk_info.axi_path[i].camnoc_bw = 0;
			hw_mgr->clk_info.axi_path[i].mnoc_ab_bw = 0;
			hw_mgr->clk_info.axi_path[i].mnoc_ib_bw = 0;
			hw_mgr->clk_info.axi_path[i].ddr_ab_bw = 0;
			hw_mgr->clk_info.axi_path[i].ddr_ib_bw = 0;
		}
	}

	cre_dev_acquire.ctx_id = ctx_id;
	cre_dev_acquire.cre_acquire = &ctx->cre_acquire;

	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		rc = hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv, CRE_HW_ACQUIRE,
			&cre_dev_acquire, sizeof(cre_dev_acquire));
		if (rc) {
			CAM_ERR(CAM_CRE, "CRE Dev acquire failed: %d", rc);
			goto cre_dev_acquire_failed;
		}
	}

	ctx->cre_top = cre_dev_acquire.cre_top;

	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		dev = (struct cam_hw_info *)hw_mgr->cre_dev_intf[i]->hw_priv;
		soc_info = &dev->soc_info;
		idx = soc_info->src_clk_idx;
		clk_update.clk_rate = soc_info->clk_rate[CAM_TURBO_VOTE][idx];
		hw_mgr->clk_info.curr_clk =
			soc_info->clk_rate[CAM_TURBO_VOTE][idx];

		rc = hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv, CRE_HW_CLK_UPDATE,
			&clk_update, sizeof(clk_update));
		if (rc) {
			CAM_ERR(CAM_CRE, "CRE Dev clk update failed: %d", rc);
			goto cre_clk_update_failed;
		}
	}

	bw_update = kzalloc(sizeof(struct cam_cre_dev_bw_update), GFP_KERNEL);
	if (!bw_update) {
		CAM_ERR(CAM_ISP, "Out of memory");
		goto cre_clk_update_failed;
	}
	bw_update->ahb_vote_valid = false;
	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		bw_update->axi_vote.num_paths = 1;
		bw_update->axi_vote_valid = true;
		bw_update->axi_vote.axi_path[0].camnoc_bw = 600000000;
		bw_update->axi_vote.axi_path[0].mnoc_ab_bw = 600000000;
		bw_update->axi_vote.axi_path[0].mnoc_ib_bw = 600000000;
		bw_update->axi_vote.axi_path[0].ddr_ab_bw = 600000000;
		bw_update->axi_vote.axi_path[0].ddr_ib_bw = 600000000;
		bw_update->axi_vote.axi_path[0].transac_type =
			CAM_AXI_TRANSACTION_WRITE;
		bw_update->axi_vote.axi_path[0].path_data_type =
			CAM_AXI_PATH_DATA_ALL;
		rc = hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv, CRE_HW_BW_UPDATE,
			bw_update, sizeof(*bw_update));
		if (rc) {
			CAM_ERR(CAM_CRE, "CRE Dev clk update failed: %d", rc);
			goto free_bw_update;
		}
	}

	cam_cre_device_timer_start(hw_mgr);
	hw_mgr->cre_ctx_cnt++;
	ctx->context_priv = args->context_data;
	args->ctxt_to_hw_map = ctx;
	ctx->ctxt_event_cb = args->event_cb;
	cam_cre_ctx_clk_info_init(ctx);
	ctx->ctx_state = CRE_CTX_STATE_ACQUIRED;
	cam_free_clear(bw_update);
	bw_update = NULL;

	mutex_unlock(&ctx->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	CAM_INFO(CAM_CRE, "CRE: %d acquire succesfull rc %d", ctx_id, rc);
	return rc;

free_bw_update:
	cam_free_clear(bw_update);
	bw_update = NULL;
cre_clk_update_failed:
	cre_dev_release.ctx_id = ctx_id;
	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		if (hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv, CRE_HW_RELEASE,
			&cre_dev_release, sizeof(cre_dev_release)))
			CAM_ERR(CAM_CRE, "CRE Dev release failed");
	}
cre_dev_acquire_failed:
	if (!hw_mgr->cre_ctx_cnt) {
		irq_cb.cre_hw_mgr_cb = NULL;
		irq_cb.data = hw_mgr;
		for (i = 0; i < cre_hw_mgr->num_cre; i++) {
			if (hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
				hw_mgr->cre_dev_intf[i]->hw_priv,
				CRE_HW_SET_IRQ_CB,
				&irq_cb, sizeof(irq_cb)))
				CAM_ERR(CAM_CRE,
					"CRE IRQ de register failed");
		}
	}
cre_irq_set_failed:
	if (!hw_mgr->cre_ctx_cnt) {
		for (i = 0; i < cre_hw_mgr->num_cre; i++) {
			if (hw_mgr->cre_dev_intf[i]->hw_ops.deinit(
				hw_mgr->cre_dev_intf[i]->hw_priv, NULL, 0))
				CAM_ERR(CAM_CRE, "CRE deinit fail");
		}
	}
end:
	args->ctxt_to_hw_map = NULL;
	cam_cre_put_free_ctx(hw_mgr, ctx_id);
	mutex_unlock(&ctx->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static int cam_cre_mgr_release_ctx(struct cam_cre_hw_mgr *hw_mgr, int ctx_id)
{
	int i = 0, rc = 0;
	struct cam_cre_dev_release cre_dev_release;

	if (ctx_id >= CRE_CTX_MAX) {
		CAM_ERR(CAM_CRE, "ctx_id is wrong: %d", ctx_id);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->ctx[ctx_id].ctx_mutex);
	if (hw_mgr->ctx[ctx_id].ctx_state !=
		CRE_CTX_STATE_ACQUIRED) {
		mutex_unlock(&hw_mgr->ctx[ctx_id].ctx_mutex);
		CAM_DBG(CAM_CRE, "ctx id: %d not in right state: %d",
			ctx_id, hw_mgr->ctx[ctx_id].ctx_state);
		return 0;
	}

	hw_mgr->ctx[ctx_id].ctx_state = CRE_CTX_STATE_RELEASE;

	for (i = 0; i < cre_hw_mgr->num_cre; i++) {
		cre_dev_release.ctx_id = ctx_id;
		rc = hw_mgr->cre_dev_intf[i]->hw_ops.process_cmd(
			hw_mgr->cre_dev_intf[i]->hw_priv, CRE_HW_RELEASE,
			&cre_dev_release, sizeof(cre_dev_release));
		if (rc)
			CAM_ERR(CAM_CRE, "CRE Dev release failed: %d", rc);
	}

	for (i = 0; i < CAM_CTX_REQ_MAX; i++) {
		if (!hw_mgr->ctx[ctx_id].req_list[i])
			continue;
		cam_cre_free_io_config(hw_mgr->ctx[ctx_id].req_list[i]);
		cam_free_clear(hw_mgr->ctx[ctx_id].req_list[i]);
		hw_mgr->ctx[ctx_id].req_list[i] = NULL;
		clear_bit(i, hw_mgr->ctx[ctx_id].bitmap);
	}

	hw_mgr->ctx[ctx_id].req_cnt = 0;
	hw_mgr->ctx[ctx_id].last_flush_req = 0;
	hw_mgr->ctx[ctx_id].last_req_idx = 0;
	hw_mgr->ctx[ctx_id].last_done_req_idx = 0;
	cam_cre_put_free_ctx(hw_mgr, ctx_id);

	rc = cam_cre_mgr_cre_clk_remove(hw_mgr, ctx_id);
	if (rc)
		CAM_ERR(CAM_CRE, "CRE clk update failed: %d", rc);

	hw_mgr->cre_ctx_cnt--;
	mutex_unlock(&hw_mgr->ctx[ctx_id].ctx_mutex);
	CAM_DBG(CAM_CRE, "X: ctx_id = %d", ctx_id);

	return 0;
}

static int cam_cre_mgr_release_hw(void *hw_priv, void *hw_release_args)
{
	int i, rc = 0;
	int ctx_id = 0;
	struct cam_hw_release_args *release_hw = hw_release_args;
	struct cam_cre_hw_mgr *hw_mgr = hw_priv;
	struct cam_cre_ctx *ctx_data = NULL;
	struct cam_cre_set_irq_cb irq_cb;
	struct cam_hw_intf *dev_intf;

	if (!release_hw || !hw_mgr) {
		CAM_ERR(CAM_CRE, "Invalid args: %pK %pK", release_hw, hw_mgr);
		return -EINVAL;
	}

	ctx_data = release_hw->ctxt_to_hw_map;
	if (!ctx_data) {
		CAM_ERR(CAM_CRE, "NULL ctx data");
		return -EINVAL;
	}

	ctx_id = ctx_data->ctx_id;
	if (ctx_id < 0 || ctx_id >= CRE_CTX_MAX) {
		CAM_ERR(CAM_CRE, "Invalid ctx id: %d", ctx_id);
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->ctx[ctx_id].ctx_mutex);
	if (hw_mgr->ctx[ctx_id].ctx_state != CRE_CTX_STATE_ACQUIRED) {
		CAM_DBG(CAM_CRE, "ctx is not in use: %d", ctx_id);
		mutex_unlock(&hw_mgr->ctx[ctx_id].ctx_mutex);
		return -EINVAL;
	}
	mutex_unlock(&hw_mgr->ctx[ctx_id].ctx_mutex);

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	rc = cam_cre_mgr_release_ctx(hw_mgr, ctx_id);
	if (!hw_mgr->cre_ctx_cnt) {
		CAM_DBG(CAM_CRE, "Last Release #of CRE %d", cre_hw_mgr->num_cre);
		for (i = 0; i < cre_hw_mgr->num_cre; i++) {
			dev_intf = hw_mgr->cre_dev_intf[i];
			irq_cb.cre_hw_mgr_cb = NULL;
			irq_cb.data = NULL;
			rc = dev_intf->hw_ops.process_cmd(
				hw_mgr->cre_dev_intf[i]->hw_priv,
				CRE_HW_SET_IRQ_CB,
				&irq_cb, sizeof(irq_cb));
			if (rc)
				CAM_ERR(CAM_CRE, "IRQ dereg failed: %d", rc);
		}
		for (i = 0; i < cre_hw_mgr->num_cre; i++) {
			dev_intf = hw_mgr->cre_dev_intf[i];
			rc = dev_intf->hw_ops.deinit(
				hw_mgr->cre_dev_intf[i]->hw_priv,
				NULL, 0);
			if (rc)
				CAM_ERR(CAM_CRE, "deinit failed: %d", rc);
		}
		cam_cre_device_timer_stop(hw_mgr);
	}

	rc = cam_cre_mgr_remove_bw(hw_mgr, ctx_id);
	if (rc)
		CAM_ERR(CAM_CRE, "CRE remove bw failed: %d", rc);

	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	CAM_DBG(CAM_CRE, "Release done for ctx_id %d", ctx_id);
	return rc;
}

static int cam_cre_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	struct cam_cre_clk_bw_request *clk_info;
	struct cam_cre_clk_bw_req_internal_v2 *clk_info_v2;
	struct cre_clk_bw_request_v2 *soc_req;

	struct cre_cmd_generic_blob *blob;
	struct cam_cre_ctx *ctx_data;
	uint32_t index;
	size_t clk_update_size = 0;
	int rc = 0;

	if (!blob_data || (blob_size == 0)) {
		CAM_ERR(CAM_CRE, "Invalid blob info %pK %d", blob_data,
		blob_size);
		return -EINVAL;
	}

	blob = (struct cre_cmd_generic_blob *)user_data;
	ctx_data = blob->ctx;
	index = blob->req_idx;

	switch (blob_type) {
	case CAM_CRE_CMD_GENERIC_BLOB_CLK_V2:
		if (blob_size < sizeof(struct cre_clk_bw_request_v2)) {
			CAM_ERR(CAM_CRE, "Mismatch blob size %d expected %lu",
				blob_size,
				sizeof(struct cre_clk_bw_request_v2));
			return -EINVAL;
		}

		soc_req = (struct cre_clk_bw_request_v2 *)blob_data;
		if (soc_req->num_paths > CAM_CRE_MAX_PER_PATH_VOTES) {
			CAM_ERR(CAM_CRE, "Invalid num paths: %d",
				soc_req->num_paths);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (soc_req->num_paths != 1) {
			if (sizeof(struct cam_axi_per_path_bw_vote) >
				((UINT_MAX -
				sizeof(struct cre_clk_bw_request_v2)) /
				(soc_req->num_paths - 1))) {
				CAM_ERR(CAM_CRE,
					"Size exceeds limit paths:%u size per path:%lu",
					soc_req->num_paths - 1,
					sizeof(
					struct cam_axi_per_path_bw_vote));
			return -EINVAL;
			}
		}

		clk_update_size = sizeof(struct cre_clk_bw_request_v2) +
			((soc_req->num_paths - 1) *
			sizeof(struct cam_axi_per_path_bw_vote));
		if (blob_size < clk_update_size) {
			CAM_ERR(CAM_CRE, "Invalid blob size: %u",
				blob_size);
			return -EINVAL;
		}

		clk_info = &ctx_data->req_list[index]->clk_info;
		clk_info_v2 = &ctx_data->req_list[index]->clk_info_v2;

		memcpy(clk_info_v2, soc_req, clk_update_size);

		/* Use v2 structure for clk fields */
		clk_info->budget_ns = clk_info_v2->budget_ns;
		clk_info->frame_cycles = clk_info_v2->frame_cycles;
		clk_info->rt_flag = clk_info_v2->rt_flag;

		CAM_DBG(CAM_CRE, "budget=%llu, frame_cycle=%llu, rt_flag=%d",
			clk_info_v2->budget_ns, clk_info_v2->frame_cycles,
			clk_info_v2->rt_flag);
		break;

	default:
		CAM_WARN(CAM_CRE, "Invalid blob type %d", blob_type);
		break;
	}
	return rc;
}

static int cam_cre_process_generic_cmd_buffer(
	struct cam_packet *packet,
	struct cam_cre_ctx *ctx_data,
	int32_t index,
	uint64_t *io_buf_addr)
{
	int i, rc = 0;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct cre_cmd_generic_blob cmd_generic_blob;

	cmd_generic_blob.ctx = ctx_data;
	cmd_generic_blob.req_idx = index;
	cmd_generic_blob.io_buf_addr = io_buf_addr;

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *) &packet->payload + packet->cmd_buf_offset/4);

	for (i = 0; i < packet->num_cmd_buf; i++) {
		if (!cmd_desc[i].length)
			continue;

	if (cmd_desc[i].meta_data != CAM_CRE_CMD_META_GENERIC_BLOB)
		continue;

	rc = cam_packet_util_process_generic_cmd_buffer(&cmd_desc[i],
		cam_cre_packet_generic_blob_handler, &cmd_generic_blob);
	if (rc)
		CAM_ERR(CAM_CRE, "Failed in processing blobs %d", rc);
	}

	return rc;
}

static int cam_cre_mgr_prepare_hw_update(void *hw_priv,
	void *hw_prepare_update_args)
{
	int rc = 0;
	struct cam_packet *packet = NULL;
	struct cam_cre_hw_mgr *hw_mgr = hw_priv;
	struct cam_hw_prepare_update_args *prepare_args =
		hw_prepare_update_args;
	struct cam_cre_ctx *ctx_data = NULL;
	uint32_t request_idx = 0;
	struct cam_cre_request *cre_req;
	struct timespec64 ts;

	if ((!prepare_args) || (!hw_mgr) || (!prepare_args->packet)) {
		CAM_ERR(CAM_CRE, "Invalid args: %x %x",
			prepare_args, hw_mgr);
		return -EINVAL;
	}

	ctx_data = prepare_args->ctxt_to_hw_map;
	if (!ctx_data) {
		CAM_ERR(CAM_CRE, "Invalid Context");
		return -EINVAL;
	}

	mutex_lock(&ctx_data->ctx_mutex);
	if (ctx_data->ctx_state != CRE_CTX_STATE_ACQUIRED) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_CRE, "ctx id %u is not acquired state: %d",
			ctx_data->ctx_id, ctx_data->ctx_state);
		return -EINVAL;
	}

	packet = prepare_args->packet;
	rc = cam_packet_util_validate_packet(packet, prepare_args->remain_len);
	if (rc) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_CRE,
			"packet validation failed: %d req_id: %d ctx: %d",
			rc, packet->header.request_id, ctx_data->ctx_id);
		return rc;
	}

	rc = cam_cre_mgr_pkt_validation(packet);
	if (rc) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_CRE,
			"cre packet validation failed: %d req_id: %d ctx: %d",
			rc, packet->header.request_id, ctx_data->ctx_id);
		return -EINVAL;
	}

	rc = cam_packet_util_process_patches(packet, hw_mgr->iommu_hdl,
			hw_mgr->iommu_sec_hdl);
	if (rc) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_CRE, "Patch processing failed %d", rc);
		return rc;
	}

	request_idx  = find_next_zero_bit(ctx_data->bitmap,
			ctx_data->bits, ctx_data->last_req_idx);
	if (request_idx >= CAM_CTX_REQ_MAX || request_idx < 0) {
		mutex_unlock(&ctx_data->ctx_mutex);
		CAM_ERR(CAM_CRE, "Invalid ctx req slot = %d", request_idx);
		return -EINVAL;
	}
	CAM_DBG(CAM_CRE, "req_idx %d last_req_idx %d bitmap size in bits %d",
		request_idx, ctx_data->last_req_idx, ctx_data->bits);
	ctx_data->last_req_idx = request_idx;

	ctx_data->req_list[request_idx] =
		kzalloc(sizeof(struct cam_cre_request), GFP_KERNEL);
	if (!ctx_data->req_list[request_idx]) {
		CAM_ERR(CAM_CRE, "mem allocation failed ctx:%d req_idx:%d",
			ctx_data->ctx_id, request_idx);
		rc = -ENOMEM;
		goto req_mem_alloc_failed;
	}
	memset(ctx_data->req_list[request_idx], 0,
			sizeof(struct cam_cre_request));

	cre_req = ctx_data->req_list[request_idx];
	cre_req->request_id = packet->header.request_id;
	cre_req->frames_done = 0;
	cre_req->req_idx = request_idx;

	rc = cam_cre_mgr_process_io_cfg(hw_mgr, packet, ctx_data,
			request_idx, prepare_args);
	if (rc) {
		CAM_ERR(CAM_CRE,
			"IO cfg processing failed: %d ctx: %d req_id:%d",
			rc, ctx_data->ctx_id, packet->header.request_id);
		goto end;
	}

	rc = cam_cre_mgr_create_cre_reg_buf(hw_mgr, packet, prepare_args,
		ctx_data, request_idx);
	if (rc) {
		CAM_ERR(CAM_CRE,
			"create kmd buf failed: %d ctx: %d request_id:%d",
			rc, ctx_data->ctx_id, packet->header.request_id);
		goto end;
	}

	rc = cam_cre_process_generic_cmd_buffer(packet, ctx_data,
		request_idx, NULL);
	if (rc) {
		CAM_ERR(CAM_CRE, "Failed: %d ctx: %d req_id: %d req_idx: %d",
			rc, ctx_data->ctx_id, packet->header.request_id,
			request_idx);
		goto end;
	}

	prepare_args->num_hw_update_entries = 1;
	prepare_args->priv = ctx_data->req_list[request_idx];
	prepare_args->pf_data->packet = packet;
	cre_req->hang_data.packet = packet;
	ktime_get_boottime_ts64(&ts);
	ctx_data->last_req_time = (uint64_t)((ts.tv_sec * 1000000000) +
		ts.tv_nsec);
	CAM_DBG(CAM_REQ, "req_id= %llu ctx_id= %d lrt=%llu",
		packet->header.request_id, ctx_data->ctx_id,
		ctx_data->last_req_time);
	set_bit(request_idx, ctx_data->bitmap);
	mutex_unlock(&ctx_data->ctx_mutex);

	CAM_DBG(CAM_REQ, "Prepare Hw update Successful request_id: %d  ctx: %d",
		packet->header.request_id, ctx_data->ctx_id);
	return rc;

end:
	cam_free_clear((void *)ctx_data->req_list[request_idx]);
	ctx_data->req_list[request_idx] = NULL;
req_mem_alloc_failed:
	clear_bit(request_idx, ctx_data->bitmap);
	mutex_unlock(&ctx_data->ctx_mutex);
	return rc;
}

static int cam_cre_mgr_enqueue_config(struct cam_cre_hw_mgr *hw_mgr,
	struct cam_cre_ctx *ctx_data,
	struct cam_hw_config_args *config_args)
{
	int rc = 0;
	uint64_t request_id = 0;
	struct crm_workq_task *task;
	struct cre_cmd_work_data *task_data;
	struct cam_hw_update_entry *hw_update_entries;
	struct cam_cre_request *cre_req = NULL;

	cre_req = config_args->priv;
	request_id = config_args->request_id;
	hw_update_entries = config_args->hw_update_entries;

	CAM_DBG(CAM_CRE, "req_id = %lld %pK", request_id, config_args->priv);

	task = cam_req_mgr_workq_get_task(cre_hw_mgr->cmd_work);
	if (!task) {
		CAM_ERR(CAM_CRE, "no empty task");
		return -ENOMEM;
	}

	task_data = (struct cre_cmd_work_data *)task->payload;
	task_data->data = (void *)hw_mgr;
	task_data->req_idx = cre_req->req_idx;
	task_data->type = CRE_WORKQ_TASK_CMD_TYPE;
	task->process_cb = cam_cre_mgr_process_cmd;

	if (ctx_data->cre_acquire.dev_type == CAM_CRE_DEV_TYPE_RT)
		rc = cam_req_mgr_workq_enqueue_task(task, ctx_data,
			CRM_TASK_PRIORITY_0);
	else
		rc = cam_req_mgr_workq_enqueue_task(task, ctx_data,
			CRM_TASK_PRIORITY_1);

	return rc;
}

static int cam_cre_mgr_config_hw(void *hw_priv, void *hw_config_args)
{
	int rc = 0;
	struct cam_cre_hw_mgr *hw_mgr = hw_priv;
	struct cam_hw_config_args *config_args = hw_config_args;
	struct cam_cre_ctx *ctx_data = NULL;
	struct cam_cre_request *cre_req = NULL;

	CAM_DBG(CAM_CRE, "E");
	if (!hw_mgr || !config_args) {
		CAM_ERR(CAM_CRE, "Invalid arguments %pK %pK",
			hw_mgr, config_args);
		return -EINVAL;
	}

	if (!config_args->num_hw_update_entries) {
		CAM_ERR(CAM_CRE, "No hw update enteries are available");
		return -EINVAL;
	}

	ctx_data = config_args->ctxt_to_hw_map;
	mutex_lock(&hw_mgr->hw_mgr_mutex);
	mutex_lock(&ctx_data->ctx_mutex);
	if (ctx_data->ctx_state != CRE_CTX_STATE_ACQUIRED) {
		mutex_unlock(&ctx_data->ctx_mutex);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		CAM_ERR(CAM_CRE, "ctx id :%u is not in use",
			ctx_data->ctx_id);
		return -EINVAL;
	}

	cre_req = config_args->priv;

	cam_cre_mgr_cre_clk_update(hw_mgr, ctx_data, cre_req->req_idx);
	ctx_data->req_list[cre_req->req_idx]->submit_timestamp = ktime_get();

	if (cre_req->request_id <= ctx_data->last_flush_req)
		CAM_WARN(CAM_CRE,
			"Anomaly submitting flushed req %llu [last_flush %llu] in ctx %u",
			cre_req->request_id, ctx_data->last_flush_req,
			ctx_data->ctx_id);

	rc = cam_cre_mgr_enqueue_config(hw_mgr, ctx_data, config_args);
	if (rc)
		goto config_err;

	CAM_DBG(CAM_REQ, "req_id %llu, ctx_id %u io config",
		cre_req->request_id, ctx_data->ctx_id);

	mutex_unlock(&ctx_data->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);

	return rc;
config_err:
	cam_cre_mgr_handle_config_err(config_args, ctx_data);
	mutex_unlock(&ctx_data->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return rc;
}

static void cam_cre_mgr_print_io_bufs(struct cam_packet *packet,
	int32_t iommu_hdl, int32_t sec_mmu_hdl, uint32_t pf_buf_info,
	bool *mem_found)
{
	dma_addr_t iova_addr;
	size_t     src_buf_size;
	int        i;
	int        j;
	int        rc = 0;
	int32_t    mmu_hdl;

	struct cam_buf_io_cfg  *io_cfg = NULL;

	if (mem_found)
		*mem_found = false;

	io_cfg = (struct cam_buf_io_cfg *)((uint32_t *)&packet->payload +
		packet->io_configs_offset / 4);

	for (i = 0; i < packet->num_io_configs; i++) {
		for (j = 0; j < CAM_PACKET_MAX_PLANES; j++) {
			if (!io_cfg[i].mem_handle[j])
				break;

			if (GET_FD_FROM_HANDLE(io_cfg[i].mem_handle[j]) ==
				GET_FD_FROM_HANDLE(pf_buf_info)) {
				CAM_INFO(CAM_CRE,
					"Found PF at port: %d mem %x fd: %x",
					io_cfg[i].resource_type,
					io_cfg[i].mem_handle[j],
					pf_buf_info);
				if (mem_found)
					*mem_found = true;
			}

			CAM_INFO(CAM_CRE, "port: %d f: %d format: %d dir %d",
				io_cfg[i].resource_type,
				io_cfg[i].fence,
				io_cfg[i].format,
				io_cfg[i].direction);

			mmu_hdl = cam_mem_is_secure_buf(
				io_cfg[i].mem_handle[j]) ? sec_mmu_hdl :
				iommu_hdl;
			rc = cam_mem_get_io_buf(io_cfg[i].mem_handle[j],
				mmu_hdl, &iova_addr, &src_buf_size, NULL);
			if (rc < 0) {
				CAM_ERR(CAM_UTIL,
					"get src buf address fail rc %d mem %x",
					rc, io_cfg[i].mem_handle[j]);
				continue;
			}

			CAM_INFO(CAM_CRE,
				"pln %d dir %d w %d h %d s %u sh %u sz %zu addr 0x%llx off 0x%x memh %x",
				j, io_cfg[i].direction,
				io_cfg[i].planes[j].width,
				io_cfg[i].planes[j].height,
				io_cfg[i].planes[j].plane_stride,
				io_cfg[i].planes[j].slice_height,
				src_buf_size, iova_addr,
				io_cfg[i].offsets[j],
				io_cfg[i].mem_handle[j]);

			iova_addr += io_cfg[i].offsets[j];

		}
	}
	cam_packet_dump_patch_info(packet, cre_hw_mgr->iommu_hdl,
		cre_hw_mgr->iommu_sec_hdl);
}

static int cam_cre_mgr_cmd(void *hw_mgr_priv, void *cmd_args)
{
	int rc = 0;
	struct cam_hw_cmd_args *hw_cmd_args = cmd_args;
	struct cam_cre_hw_mgr  *hw_mgr = hw_mgr_priv;

	if (!hw_mgr_priv || !cmd_args) {
		CAM_ERR(CAM_CRE, "Invalid arguments");
		return -EINVAL;
	}

	switch (hw_cmd_args->cmd_type) {
	case CAM_HW_MGR_CMD_DUMP_PF_INFO:
		cam_cre_mgr_print_io_bufs(
			hw_cmd_args->u.pf_args.pf_data.packet,
			hw_mgr->iommu_hdl,
			hw_mgr->iommu_sec_hdl,
			hw_cmd_args->u.pf_args.buf_info,
			hw_cmd_args->u.pf_args.mem_found);

		break;
	default:
		CAM_ERR(CAM_CRE, "Invalid cmd");
	}

	return rc;
}

static int cam_cre_mgr_flush_req(struct cam_cre_ctx *ctx_data,
	struct cam_hw_flush_args *flush_args)
{
	int idx;
	int64_t request_id;
	uint32_t evt_id;
	struct cam_hw_done_event_data buf_data;

	request_id = *(int64_t *)flush_args->flush_req_pending[0];
	for (idx = 0; idx < CAM_CTX_REQ_MAX; idx++) {
		if (!ctx_data->req_list[idx])
			continue;

		if (ctx_data->req_list[idx]->request_id != request_id)
			continue;

		evt_id = CAM_CTX_EVT_ID_ERROR;
		buf_data.evt_param = CAM_SYNC_CRE_EVENT_HW_ERR;
		buf_data.request_id = ctx_data->req_list[idx]->request_id;
		ctx_data->ctxt_event_cb(ctx_data->context_priv, evt_id, &buf_data);
		ctx_data->req_list[idx]->request_id = 0;
		cam_cre_free_io_config(ctx_data->req_list[idx]);
		cam_free_clear(ctx_data->req_list[idx]);
		ctx_data->req_list[idx] = NULL;
		clear_bit(idx, ctx_data->bitmap);
	}

	return 0;
}

static int cam_cre_mgr_flush_all(struct cam_cre_ctx *ctx_data,
	struct cam_hw_flush_args *flush_args)
{
	int i, rc;
	uint32_t evt_id;
	struct cam_hw_done_event_data buf_data;

	mutex_lock(&ctx_data->ctx_mutex);
	rc = cam_cre_mgr_reset_hw();

	for (i = 0; i < CAM_CTX_REQ_MAX; i++) {
		if (!ctx_data->req_list[i])
			continue;

		evt_id = CAM_CTX_EVT_ID_ERROR;
		buf_data.evt_param = CAM_SYNC_CRE_EVENT_HW_ERR;
		buf_data.request_id = ctx_data->req_list[i]->request_id;
		ctx_data->ctxt_event_cb(ctx_data->context_priv, evt_id, &buf_data);
		ctx_data->req_list[i]->request_id = 0;
		cam_cre_free_io_config(ctx_data->req_list[i]);
		cam_free_clear(ctx_data->req_list[i]);
		ctx_data->req_list[i] = NULL;
		clear_bit(i, ctx_data->bitmap);
	}
	mutex_unlock(&ctx_data->ctx_mutex);

	return rc;
}

static int cam_cre_mgr_hw_dump(void *hw_priv, void *hw_dump_args)
{
	struct cam_cre_ctx *ctx_data;
	struct cam_cre_hw_mgr *hw_mgr = hw_priv;
	struct cam_hw_dump_args  *dump_args;
	int idx;
	ktime_t cur_time;
	struct timespec64 cur_ts, req_ts;
	uint64_t diff;

	if ((!hw_priv) || (!hw_dump_args)) {
		CAM_ERR(CAM_CRE, "Invalid params %pK %pK",
			hw_priv, hw_dump_args);
		return -EINVAL;
	}

	dump_args = (struct cam_hw_dump_args *)hw_dump_args;
	ctx_data = dump_args->ctxt_to_hw_map;

	if (!ctx_data) {
		CAM_ERR(CAM_CRE, "Invalid context");
		return -EINVAL;
	}

	mutex_lock(&hw_mgr->hw_mgr_mutex);
	mutex_lock(&ctx_data->ctx_mutex);

	CAM_INFO(CAM_CRE, "Req %lld", dump_args->request_id);
	for (idx = 0; idx < CAM_CTX_REQ_MAX; idx++) {
		if (!ctx_data->req_list[idx])
			continue;

		if (ctx_data->req_list[idx]->request_id ==
			dump_args->request_id)
			break;
	}

	/* no matching request found */
	if (idx == CAM_CTX_REQ_MAX) {
		mutex_unlock(&ctx_data->ctx_mutex);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return 0;
	}

	cur_time = ktime_get();
	diff = ktime_us_delta(cur_time,
			ctx_data->req_list[idx]->submit_timestamp);
	cur_ts = ktime_to_timespec64(cur_time);
	req_ts = ktime_to_timespec64(ctx_data->req_list[idx]->submit_timestamp);

	if (diff < (CRE_REQUEST_TIMEOUT * 1000)) {
		CAM_INFO(CAM_CRE, "No Error req %llu %ld:%06ld %ld:%06ld",
			dump_args->request_id,
			req_ts.tv_sec,
			req_ts.tv_nsec/NSEC_PER_USEC,
			cur_ts.tv_sec,
			cur_ts.tv_nsec/NSEC_PER_USEC);
		mutex_unlock(&ctx_data->ctx_mutex);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		return 0;
	}

	CAM_ERR(CAM_CRE, "Error req %llu %ld:%06ld %ld:%06ld",
		dump_args->request_id,
		req_ts.tv_sec,
		req_ts.tv_nsec/NSEC_PER_USEC,
		cur_ts.tv_sec,
		cur_ts.tv_nsec/NSEC_PER_USEC);

	mutex_unlock(&ctx_data->ctx_mutex);
	mutex_unlock(&hw_mgr->hw_mgr_mutex);
	return 0;
}

static int cam_cre_mgr_hw_flush(void *hw_priv, void *hw_flush_args)
{
	struct cam_hw_flush_args *flush_args = hw_flush_args;
	struct cam_cre_ctx *ctx_data;
	struct cam_cre_hw_mgr *hw_mgr = cre_hw_mgr;

	if ((!hw_priv) || (!hw_flush_args)) {
		CAM_ERR(CAM_CRE, "Input params are Null");
		return -EINVAL;
	}

	ctx_data = flush_args->ctxt_to_hw_map;
	if (!ctx_data) {
		CAM_ERR(CAM_CRE, "Ctx data is NULL");
		return -EINVAL;
	}

	if ((flush_args->flush_type >= CAM_FLUSH_TYPE_MAX) ||
		(flush_args->flush_type < CAM_FLUSH_TYPE_REQ)) {
		CAM_ERR(CAM_CRE, "Invalid flush type: %d",
			flush_args->flush_type);
		return -EINVAL;
	}

	switch (flush_args->flush_type) {
	case CAM_FLUSH_TYPE_ALL:
		mutex_lock(&hw_mgr->hw_mgr_mutex);
		ctx_data->last_flush_req = flush_args->last_flush_req;

		CAM_DBG(CAM_REQ, "ctx_id %d Flush type %d last_flush_req %u",
				ctx_data->ctx_id, flush_args->flush_type,
				ctx_data->last_flush_req);

		cam_cre_mgr_flush_all(ctx_data, flush_args);
		mutex_unlock(&hw_mgr->hw_mgr_mutex);
		break;
	case CAM_FLUSH_TYPE_REQ:
		mutex_lock(&ctx_data->ctx_mutex);
		if (flush_args->num_req_active) {
			CAM_ERR(CAM_CRE, "Flush request is not supported");
			mutex_unlock(&ctx_data->ctx_mutex);
			return -EINVAL;
		}
		if (flush_args->num_req_pending)
			cam_cre_mgr_flush_req(ctx_data, flush_args);
		mutex_unlock(&ctx_data->ctx_mutex);
		break;
	default:
		CAM_ERR(CAM_CRE, "Invalid flush type: %d",
				flush_args->flush_type);
		return -EINVAL;
	}

	return 0;
}

static int cam_cre_mgr_alloc_devs(struct device_node *of_node)
{
	int rc;
	uint32_t num_dev;

	rc = of_property_read_u32(of_node, "num-cre", &num_dev);
	if (rc) {
		CAM_ERR(CAM_CRE, "getting num of cre failed: %d", rc);
		return -EINVAL;
	}

	cre_hw_mgr->devices[CRE_DEV_CRE] = kzalloc(
		sizeof(struct cam_hw_intf *) * num_dev, GFP_KERNEL);
	if (!cre_hw_mgr->devices[CRE_DEV_CRE])
		return -ENOMEM;

	return 0;
}

static int cam_cre_mgr_init_devs(struct device_node *of_node)
{
	int rc = 0;
	int count, i;
	const char *name = NULL;
	struct device_node *child_node = NULL;
	struct platform_device *child_pdev = NULL;
	struct cam_hw_intf *child_dev_intf = NULL;
	struct cam_hw_info *cre_dev;
	struct cam_hw_soc_info *soc_info = NULL;

	rc = cam_cre_mgr_alloc_devs(of_node);
	if (rc)
		return rc;

	count = of_property_count_strings(of_node, "compat-hw-name");
	if (!count) {
		CAM_ERR(CAM_CRE, "no compat hw found in dev tree, cnt = %d",
			count);
		rc = -EINVAL;
		goto compat_hw_name_failed;
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "compat-hw-name",
			i, &name);
		if (rc) {
			CAM_ERR(CAM_CRE, "getting dev object name failed");
			goto compat_hw_name_failed;
		}

		child_node = of_find_node_by_name(NULL, name);
		if (!child_node) {
			CAM_ERR(CAM_CRE, "Cannot find node in dtsi %s", name);
			rc = -ENODEV;
			goto compat_hw_name_failed;
		}

		child_pdev = of_find_device_by_node(child_node);
		if (!child_pdev) {
			CAM_ERR(CAM_CRE, "failed to find device on bus %s",
				child_node->name);
			rc = -ENODEV;
			of_node_put(child_node);
			goto compat_hw_name_failed;
		}

		child_dev_intf = (struct cam_hw_intf *)platform_get_drvdata(
			child_pdev);
		if (!child_dev_intf) {
			CAM_ERR(CAM_CRE, "no child device");
			of_node_put(child_node);
			goto compat_hw_name_failed;
		}
		cre_hw_mgr->devices[child_dev_intf->hw_type]
			[child_dev_intf->hw_idx] = child_dev_intf;

		if (!child_dev_intf->hw_ops.process_cmd)
			goto compat_hw_name_failed;

		of_node_put(child_node);
	}

	cre_hw_mgr->num_cre = count;
	for (i = 0; i < count; i++) {
		cre_hw_mgr->cre_dev_intf[i] =
			cre_hw_mgr->devices[CRE_DEV_CRE][i];
			cre_dev = cre_hw_mgr->cre_dev_intf[i]->hw_priv;
			soc_info = &cre_dev->soc_info;
	}

	return 0;
compat_hw_name_failed:
	kfree(cre_hw_mgr->devices[CRE_DEV_CRE]);
	cre_hw_mgr->devices[CRE_DEV_CRE] = NULL;
	return rc;
}

static void cam_req_mgr_process_cre_command_queue(struct work_struct *w)
{
	cam_req_mgr_process_workq(w);
}

static void cam_req_mgr_process_cre_msg_queue(struct work_struct *w)
{
	cam_req_mgr_process_workq(w);
}

static void cam_req_mgr_process_cre_timer_queue(struct work_struct *w)
{
	cam_req_mgr_process_workq(w);
}

static int cam_cre_mgr_create_wq(void)
{

	int rc;
	int i;

	rc = cam_req_mgr_workq_create("cre_command_queue", CRE_WORKQ_NUM_TASK,
		&cre_hw_mgr->cmd_work, CRM_WORKQ_USAGE_NON_IRQ,
		0, cam_req_mgr_process_cre_command_queue);
	if (rc) {
		CAM_ERR(CAM_CRE, "unable to create a command worker");
		goto cmd_work_failed;
	}

	rc = cam_req_mgr_workq_create("cre_message_queue", CRE_WORKQ_NUM_TASK,
		&cre_hw_mgr->msg_work, CRM_WORKQ_USAGE_IRQ, 0,
		cam_req_mgr_process_cre_msg_queue);
	if (rc) {
		CAM_ERR(CAM_CRE, "unable to create a message worker");
		goto msg_work_failed;
	}

	rc = cam_req_mgr_workq_create("cre_timer_queue", CRE_WORKQ_NUM_TASK,
		&cre_hw_mgr->timer_work, CRM_WORKQ_USAGE_IRQ, 0,
		cam_req_mgr_process_cre_timer_queue);
	if (rc) {
		CAM_ERR(CAM_CRE, "unable to create a timer worker");
		goto timer_work_failed;
	}

	cre_hw_mgr->cmd_work_data =
		kzalloc(sizeof(struct cre_cmd_work_data) * CRE_WORKQ_NUM_TASK,
		GFP_KERNEL);
	if (!cre_hw_mgr->cmd_work_data) {
		rc = -ENOMEM;
		goto cmd_work_data_failed;
	}

	cre_hw_mgr->msg_work_data =
		kzalloc(sizeof(struct cre_msg_work_data) * CRE_WORKQ_NUM_TASK,
		GFP_KERNEL);
	if (!cre_hw_mgr->msg_work_data) {
		rc = -ENOMEM;
		goto msg_work_data_failed;
	}

	cre_hw_mgr->timer_work_data =
		kzalloc(sizeof(struct cre_clk_work_data) * CRE_WORKQ_NUM_TASK,
		GFP_KERNEL);
	if (!cre_hw_mgr->timer_work_data) {
		rc = -ENOMEM;
		goto timer_work_data_failed;
	}

	for (i = 0; i < CRE_WORKQ_NUM_TASK; i++)
		cre_hw_mgr->msg_work->task.pool[i].payload =
				&cre_hw_mgr->msg_work_data[i];

	for (i = 0; i < CRE_WORKQ_NUM_TASK; i++)
		cre_hw_mgr->cmd_work->task.pool[i].payload =
				&cre_hw_mgr->cmd_work_data[i];

	for (i = 0; i < CRE_WORKQ_NUM_TASK; i++)
		cre_hw_mgr->timer_work->task.pool[i].payload =
				&cre_hw_mgr->timer_work_data[i];
	return 0;

timer_work_data_failed:
	kfree(cre_hw_mgr->msg_work_data);
msg_work_data_failed:
	kfree(cre_hw_mgr->cmd_work_data);
cmd_work_data_failed:
	cam_req_mgr_workq_destroy(&cre_hw_mgr->timer_work);
timer_work_failed:
	cam_req_mgr_workq_destroy(&cre_hw_mgr->msg_work);
msg_work_failed:
	cam_req_mgr_workq_destroy(&cre_hw_mgr->cmd_work);
cmd_work_failed:
	return rc;
}

static int cam_cre_set_dbg_default_clk(void *data, u64 val)
{
	cre_hw_mgr->cre_debug_clk = val;
	return 0;
}

static int cam_cre_get_dbg_default_clk(void *data, u64 *val)
{
	*val = cre_hw_mgr->cre_debug_clk;
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(cam_cre_debug_default_clk,
	cam_cre_get_dbg_default_clk,
	cam_cre_set_dbg_default_clk, "%16llu");

static int cam_cre_create_debug_fs(void)
{
	struct dentry *dbgfileptr = NULL;
	int rc = 0;
	cre_hw_mgr->dentry = debugfs_create_dir("camera_cre",
		NULL);

	if (!cre_hw_mgr->dentry) {
		CAM_ERR(CAM_CRE, "failed to create dentry");
		return -ENOMEM;
	}

	if (!debugfs_create_bool("dump_req_data_enable",
		0644,
		cre_hw_mgr->dentry,
		&cre_hw_mgr->dump_req_data_enable)) {
		CAM_ERR(CAM_CRE,
			"failed to create dump_enable_debug");
		goto err;
	}

	dbgfileptr = debugfs_create_file("cre_debug_clk", 0644,
		cre_hw_mgr->dentry, NULL, &cam_cre_debug_default_clk);

	if (IS_ERR(dbgfileptr)) {
		if (PTR_ERR(dbgfileptr) == -ENODEV)
			CAM_WARN(CAM_CRE, "DebugFS not enabled in kernel!");
		else
			rc = PTR_ERR(dbgfileptr);
	}
	return 0;
err:
	debugfs_remove_recursive(cre_hw_mgr->dentry);
	return -ENOMEM;
}

int cam_cre_hw_mgr_init(struct device_node *of_node, void *hw_mgr,
	int *iommu_hdl)
{
	int i, rc = 0, j;
	struct cam_hw_mgr_intf *hw_mgr_intf;

	if (!of_node || !hw_mgr) {
		CAM_ERR(CAM_CRE, "Invalid args of_node %pK hw_mgr %pK",
			of_node, hw_mgr);
		return -EINVAL;
	}
	hw_mgr_intf = (struct cam_hw_mgr_intf *)hw_mgr;

	cre_hw_mgr = kzalloc(sizeof(struct cam_cre_hw_mgr), GFP_KERNEL);
	if (!cre_hw_mgr) {
		CAM_ERR(CAM_CRE, "Unable to allocate mem for: size = %d",
			sizeof(struct cam_cre_hw_mgr));
		return -ENOMEM;
	}

	hw_mgr_intf->hw_mgr_priv = cre_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_cre_mgr_get_hw_caps;
	hw_mgr_intf->hw_acquire = cam_cre_mgr_acquire_hw;
	hw_mgr_intf->hw_release = cam_cre_mgr_release_hw;
	hw_mgr_intf->hw_start   = NULL;
	hw_mgr_intf->hw_stop    = NULL;
	hw_mgr_intf->hw_prepare_update = cam_cre_mgr_prepare_hw_update;
	hw_mgr_intf->hw_config_stream_settings = NULL;
	hw_mgr_intf->hw_config = cam_cre_mgr_config_hw;
	hw_mgr_intf->hw_read   = NULL;
	hw_mgr_intf->hw_write  = NULL;
	hw_mgr_intf->hw_cmd = cam_cre_mgr_cmd;
	hw_mgr_intf->hw_flush = cam_cre_mgr_hw_flush;
	hw_mgr_intf->hw_dump = cam_cre_mgr_hw_dump;

	cre_hw_mgr->secure_mode = false;
	mutex_init(&cre_hw_mgr->hw_mgr_mutex);
	spin_lock_init(&cre_hw_mgr->hw_mgr_lock);

	for (i = 0; i < CRE_CTX_MAX; i++) {
		cre_hw_mgr->ctx[i].bitmap_size =
			BITS_TO_LONGS(CAM_CTX_REQ_MAX) *
			sizeof(long);
		cre_hw_mgr->ctx[i].bitmap = kzalloc(
			cre_hw_mgr->ctx[i].bitmap_size, GFP_KERNEL);
		if (!cre_hw_mgr->ctx[i].bitmap) {
			CAM_ERR(CAM_CRE, "bitmap allocation failed: size = %d",
				cre_hw_mgr->ctx[i].bitmap_size);
			rc = -ENOMEM;
			goto cre_ctx_bitmap_failed;
		}
		cre_hw_mgr->ctx[i].bits = cre_hw_mgr->ctx[i].bitmap_size *
			BITS_PER_BYTE;
		mutex_init(&cre_hw_mgr->ctx[i].ctx_mutex);
	}

	rc = cam_cre_mgr_init_devs(of_node);
	if (rc)
		goto dev_init_failed;

	cre_hw_mgr->ctx_bitmap_size =
		BITS_TO_LONGS(CRE_CTX_MAX) * sizeof(long);
	cre_hw_mgr->ctx_bitmap = kzalloc(cre_hw_mgr->ctx_bitmap_size,
		GFP_KERNEL);
	if (!cre_hw_mgr->ctx_bitmap) {
		rc = -ENOMEM;
		goto ctx_bitmap_alloc_failed;
	}

	cre_hw_mgr->ctx_bits = cre_hw_mgr->ctx_bitmap_size *
		BITS_PER_BYTE;

	rc = cam_smmu_get_handle("cre", &cre_hw_mgr->iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_CRE, "get mmu handle failed: %d", rc);
		goto cre_get_hdl_failed;
	}

	rc = cam_smmu_get_handle("cam-secure", &cre_hw_mgr->iommu_sec_hdl);
	if (rc) {
		CAM_ERR(CAM_CRE, "get secure mmu handle failed: %d", rc);
		goto secure_hdl_failed;
	}

	rc = cam_cre_mgr_create_wq();
	if (rc)
		goto cre_wq_create_failed;

	cam_cre_create_debug_fs();

	if (iommu_hdl)
		*iommu_hdl = cre_hw_mgr->iommu_hdl;

	return rc;

cre_wq_create_failed:
	cam_smmu_destroy_handle(cre_hw_mgr->iommu_sec_hdl);
	cre_hw_mgr->iommu_sec_hdl = -1;
secure_hdl_failed:
	cam_smmu_destroy_handle(cre_hw_mgr->iommu_hdl);
	cre_hw_mgr->iommu_hdl = -1;
cre_get_hdl_failed:
	cam_free_clear(cre_hw_mgr->ctx_bitmap);
	cre_hw_mgr->ctx_bitmap = NULL;
	cre_hw_mgr->ctx_bitmap_size = 0;
	cre_hw_mgr->ctx_bits = 0;
ctx_bitmap_alloc_failed:
	cam_free_clear(cre_hw_mgr->devices[CRE_DEV_CRE]);
	cre_hw_mgr->devices[CRE_DEV_CRE] = NULL;
dev_init_failed:
cre_ctx_bitmap_failed:
	mutex_destroy(&cre_hw_mgr->hw_mgr_mutex);
	for (j = i - 1; j >= 0; j--) {
		mutex_destroy(&cre_hw_mgr->ctx[j].ctx_mutex);
		cam_free_clear(cre_hw_mgr->ctx[j].bitmap);
		cre_hw_mgr->ctx[j].bitmap = NULL;
		cre_hw_mgr->ctx[j].bitmap_size = 0;
		cre_hw_mgr->ctx[j].bits = 0;
	}
	cam_free_clear(cre_hw_mgr);
	cre_hw_mgr = NULL;

	return rc;
}

