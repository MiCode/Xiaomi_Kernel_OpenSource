// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include <media/cam_isp.h>

#include "cam_compat.h"
#include "cam_smmu_api.h"
#include "cam_req_mgr_workq.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_vfe_hw_intf.h"
#include "cam_sfe_hw_intf.h"
#include "cam_isp_packet_parser.h"
#include "cam_ife_hw_mgr.h"
#include "cam_cdm_intf_api.h"
#include "cam_packet_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_mem_mgr.h"
#include "cam_mem_mgr_api.h"
#include "cam_common_util.h"
#include "cam_presil_hw_access.h"

#define CAM_IFE_SAFE_DISABLE 0
#define CAM_IFE_SAFE_ENABLE 1
#define SMMU_SE_IFE 0

#define CAM_FRAME_HEADER_BUFFER_SIZE      64
#define CAM_FRAME_HEADER_ADDR_ALIGNMENT   256

#define CAM_ISP_PACKET_META_MAX                     \
	(CAM_ISP_PACKET_META_GENERIC_BLOB_COMMON + 1)

#define CAM_ISP_GENERIC_BLOB_TYPE_MAX               \
	(CAM_ISP_GENERIC_BLOB_TYPE_CSID_QCFA_CONFIG + 1)

static uint32_t blob_type_hw_cmd_map[CAM_ISP_GENERIC_BLOB_TYPE_MAX] = {
	CAM_ISP_HW_CMD_GET_HFR_UPDATE,
	CAM_ISP_HW_CMD_CLOCK_UPDATE,
	CAM_ISP_HW_CMD_BW_UPDATE,
	CAM_ISP_HW_CMD_UBWC_UPDATE,
	CAM_ISP_HW_CMD_CSID_CLOCK_UPDATE,
	CAM_ISP_GENERIC_BLOB_TYPE_FE_CONFIG,
	CAM_ISP_HW_CMD_UBWC_UPDATE_V2,
	CAM_ISP_HW_CMD_CORE_CONFIG,
	CAM_ISP_HW_CMD_WM_CONFIG_UPDATE,
	CAM_ISP_HW_CMD_BW_UPDATE_V2,
	CAM_ISP_HW_CMD_BLANKING_UPDATE,
};

static struct cam_ife_hw_mgr g_ife_hw_mgr;
static uint32_t g_num_ife, g_num_ife_lite;
static uint32_t max_ife_out_res;

static int cam_isp_blob_ife_clock_update(
	struct cam_isp_clock_config           *clock_config,
	struct cam_ife_hw_mgr_ctx             *ctx);

static int cam_isp_blob_sfe_clock_update(
	struct cam_isp_clock_config           *clock_config,
	struct cam_ife_hw_mgr_ctx             *ctx);


static int cam_ife_hw_mgr_event_handler(
	void                                *priv,
	uint32_t                             evt_id,
	void                                *evt_info);

static int cam_ife_mgr_prog_default_settings(
	bool need_rup_aup, struct cam_ife_hw_mgr_ctx *ctx);

static int cam_ife_mgr_finish_clk_bw_update(
	struct cam_ife_hw_mgr_ctx *ctx,
	uint64_t request_id, bool skip_clk_data_rst)
{
	int i, rc = 0;
	struct cam_isp_apply_clk_bw_args clk_bw_args;

	clk_bw_args.request_id = request_id;
	clk_bw_args.skip_clk_data_rst = skip_clk_data_rst;
	for (i = 0; i < ctx->num_base; i++) {
		clk_bw_args.hw_intf = NULL;
		CAM_DBG(CAM_PERF,
			"Clock/BW Update for req_id:%d i:%d num_vfe_out:%d num_sfe_out:%d in_rd:%d",
			request_id, i, ctx->num_acq_vfe_out, ctx->num_acq_sfe_out,
			!list_empty(&ctx->res_list_ife_in_rd));
		if ((ctx->base[i].hw_type == CAM_ISP_HW_TYPE_VFE) &&
			(ctx->num_acq_vfe_out || (!list_empty(&ctx->res_list_ife_in_rd))))
			clk_bw_args.hw_intf = g_ife_hw_mgr.ife_devices[ctx->base[i].idx]->hw_intf;
		else if ((ctx->base[i].hw_type == CAM_ISP_HW_TYPE_SFE) &&
			(ctx->num_acq_sfe_out || (!list_empty(&ctx->res_list_ife_in_rd))))
			clk_bw_args.hw_intf = g_ife_hw_mgr.sfe_devices[ctx->base[i].idx]->hw_intf;
		else
			continue;

		CAM_DBG(CAM_PERF,
			"Apply Clock/BW for req_id:%d i:%d hw_idx=%d hw_type:%d num_vfe_out:%d num_sfe_out:%d in_rd:%d",
			request_id, i, clk_bw_args.hw_intf->hw_idx, clk_bw_args.hw_intf->hw_type,
			ctx->num_acq_vfe_out, ctx->num_acq_sfe_out,
			!list_empty(&ctx->res_list_ife_in_rd));
		rc = clk_bw_args.hw_intf->hw_ops.process_cmd(clk_bw_args.hw_intf->hw_priv,
			CAM_ISP_HW_CMD_APPLY_CLK_BW_UPDATE, &clk_bw_args,
			sizeof(struct cam_isp_apply_clk_bw_args));
		if (rc) {
			CAM_ERR(CAM_PERF,
				"Finish Clock/BW Update failed req_id:%d i:%d hw_idx=%d hw_type:%d rc:%d",
				request_id, i, ctx->base[i].idx, ctx->base[i].hw_type, rc);
			break;
		}
	}

	return rc;
}

static inline int __cam_ife_mgr_get_hw_soc_info(
	struct list_head          *res_list,
	enum cam_isp_hw_split_id   split_id,
	enum cam_isp_hw_type       hw_type,
	struct cam_hw_soc_info   **soc_info_ptr)
{
	int rc  = -EINVAL;
	struct cam_hw_soc_info    *soc_info = NULL;
	struct cam_hw_intf        *hw_intf = NULL;
	struct cam_isp_hw_mgr_res *hw_mgr_res;
	struct cam_isp_hw_mgr_res *hw_mgr_res_temp;

	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		res_list, list) {
		if (!hw_mgr_res->hw_res[split_id])
			continue;

		hw_intf = hw_mgr_res->hw_res[split_id]->hw_intf;
		if (hw_intf && hw_intf->hw_ops.process_cmd) {
			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA, &soc_info,
				sizeof(void *));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Failed in %d regspace data query res_id: %u split idx: %d rc : %d",
					hw_type, hw_mgr_res->res_id, split_id, rc);
				return rc;
			}

			*soc_info_ptr = soc_info;
			CAM_DBG(CAM_ISP,
				"Obtained soc info for split %d for hw_type %d",
				split_id, hw_type);
			break;
		}
	}

	return rc;
}

static const char *__cam_ife_hw_mgr_evt_type_to_string(
	enum cam_isp_hw_event_type evt_type)
{
	switch (evt_type) {
	case CAM_ISP_HW_EVENT_ERROR:
		return "ERROR";
	case CAM_ISP_HW_EVENT_SOF:
		return "SOF";
	case CAM_ISP_HW_EVENT_REG_UPDATE:
		return "REG_UPDATE";
	case CAM_ISP_HW_EVENT_EPOCH:
		return "EPOCH";
	case CAM_ISP_HW_EVENT_EOF:
		return "EOF";
	case CAM_ISP_HW_EVENT_DONE:
		return "BUF_DONE";
	default:
		return "INVALID_EVT";
	}
}

static int cam_ife_mgr_regspace_data_cb(uint32_t reg_base_type,
	void *hw_mgr_ctx, struct cam_hw_soc_info **soc_info_ptr,
	uint32_t *reg_base_idx)
{
	int rc = -EINVAL;
	struct cam_ife_hw_mgr_ctx *ctx =
		(struct cam_ife_hw_mgr_ctx *) hw_mgr_ctx;

	*soc_info_ptr = NULL;
	switch (reg_base_type) {
	case CAM_REG_DUMP_BASE_TYPE_CAMNOC:
	case CAM_REG_DUMP_BASE_TYPE_ISP_LEFT:
		rc = __cam_ife_mgr_get_hw_soc_info(
			&ctx->res_list_ife_src,
			CAM_ISP_HW_SPLIT_LEFT, CAM_ISP_HW_TYPE_VFE,
			soc_info_ptr);
		if (rc)
			return rc;

		if (reg_base_type == CAM_REG_DUMP_BASE_TYPE_ISP_LEFT)
			*reg_base_idx = 0;
		else
			*reg_base_idx = 1;

		break;
	case CAM_REG_DUMP_BASE_TYPE_ISP_RIGHT:
		rc = __cam_ife_mgr_get_hw_soc_info(
			&ctx->res_list_ife_src,
			CAM_ISP_HW_SPLIT_RIGHT, CAM_ISP_HW_TYPE_VFE,
			soc_info_ptr);
		if (rc)
			return rc;

		*reg_base_idx = 0;
		break;
	case CAM_REG_DUMP_BASE_TYPE_CSID_WRAPPER:
	case CAM_REG_DUMP_BASE_TYPE_CSID_LEFT:
		rc = __cam_ife_mgr_get_hw_soc_info(
			&ctx->res_list_ife_csid,
			CAM_ISP_HW_SPLIT_LEFT, CAM_ISP_HW_TYPE_CSID,
			soc_info_ptr);
		if (rc)
			return rc;

		if (reg_base_type == CAM_REG_DUMP_BASE_TYPE_CSID_LEFT)
			*reg_base_idx = 0;
		else
			*reg_base_idx = 1;

		break;
	case CAM_REG_DUMP_BASE_TYPE_CSID_RIGHT:
		rc = __cam_ife_mgr_get_hw_soc_info(
			&ctx->res_list_ife_csid,
			CAM_ISP_HW_SPLIT_RIGHT, CAM_ISP_HW_TYPE_CSID,
			soc_info_ptr);
		if (rc)
			return rc;

		*reg_base_idx = 0;
		break;
	case CAM_REG_DUMP_BASE_TYPE_SFE_LEFT:
		rc = __cam_ife_mgr_get_hw_soc_info(
			&ctx->res_list_sfe_src,
			CAM_ISP_HW_SPLIT_LEFT, CAM_ISP_HW_TYPE_SFE,
			soc_info_ptr);
		if (rc)
			return rc;

		*reg_base_idx = 0;
		break;
	case CAM_REG_DUMP_BASE_TYPE_SFE_RIGHT:
		rc = __cam_ife_mgr_get_hw_soc_info(
			&ctx->res_list_sfe_src,
			CAM_ISP_HW_SPLIT_RIGHT, CAM_ISP_HW_TYPE_SFE,
			soc_info_ptr);
		if (rc)
			return rc;

		*reg_base_idx = 0;
		break;
	default:
		CAM_ERR(CAM_ISP,
			"Unrecognized reg base type: %u",
			reg_base_type);
		return rc;
	}

	return rc;
}

static int cam_ife_mgr_handle_reg_dump(struct cam_ife_hw_mgr_ctx *ctx,
	struct cam_cmd_buf_desc *reg_dump_buf_desc, uint32_t num_reg_dump_buf,
	uint32_t meta_type,
	void *soc_dump_args,
	bool user_triggered_dump)
{
	int rc = 0, i;

	if (!num_reg_dump_buf || !reg_dump_buf_desc) {
		CAM_DBG(CAM_ISP,
			"Invalid args for reg dump req_id: [%llu] ctx idx: [%u] meta_type: [%u] num_reg_dump_buf: [%u] reg_dump_buf_desc: [%pK]",
			ctx->applied_req_id, ctx->ctx_index, meta_type,
			num_reg_dump_buf, reg_dump_buf_desc);
		return rc;
	}

	if (!atomic_read(&ctx->cdm_done))
		CAM_WARN_RATE_LIMIT(CAM_ISP,
			"Reg dump values might be from more than one request");

	for (i = 0; i < num_reg_dump_buf; i++) {
		CAM_DBG(CAM_ISP, "Reg dump cmd meta data: %u req_type: %u",
			reg_dump_buf_desc[i].meta_data, meta_type);
		if (reg_dump_buf_desc[i].meta_data == meta_type) {
			rc = cam_soc_util_reg_dump_to_cmd_buf(ctx,
				&reg_dump_buf_desc[i],
				ctx->applied_req_id,
				cam_ife_mgr_regspace_data_cb,
				soc_dump_args,
				user_triggered_dump);
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Reg dump failed at idx: %d, rc: %d req_id: %llu meta type: %u",
					i, rc, ctx->applied_req_id, meta_type);
				return rc;
			}
		}
	}

	return rc;
}

static inline void cam_ife_mgr_update_hw_entries_util(
	enum cam_isp_cdm_bl_type               cdm_bl_type,
	uint32_t                               total_used_bytes,
	struct cam_kmd_buf_info               *kmd_buf_info,
	struct cam_hw_prepare_update_args     *prepare)
{
	uint32_t num_ent;

	num_ent = prepare->num_hw_update_entries;
	prepare->hw_update_entries[num_ent].handle =
		kmd_buf_info->handle;
	prepare->hw_update_entries[num_ent].len =
		total_used_bytes;
	prepare->hw_update_entries[num_ent].offset =
		kmd_buf_info->offset;
	prepare->hw_update_entries[num_ent].flags = cdm_bl_type;

	num_ent++;
	kmd_buf_info->used_bytes += total_used_bytes;
	kmd_buf_info->offset     += total_used_bytes;
	prepare->num_hw_update_entries = num_ent;

	CAM_DBG(CAM_ISP, "Handle: 0x%x len: %u offset: %u flags: %u num_ent: %u",
		prepare->hw_update_entries[num_ent - 1].handle,
		prepare->hw_update_entries[num_ent - 1].len,
		prepare->hw_update_entries[num_ent - 1].offset,
		prepare->hw_update_entries[num_ent - 1].flags,
		num_ent);
}

static inline int cam_ife_mgr_allocate_cdm_cmd(
	bool is_sfe_en,
	struct cam_cdm_bl_request **cdm_cmd)
{
	int rc = 0;
	uint32_t cfg_max = CAM_ISP_CTX_CFG_MAX;

	if (is_sfe_en)
		cfg_max = CAM_ISP_SFE_CTX_CFG_MAX;

	*cdm_cmd = kzalloc(((sizeof(struct cam_cdm_bl_request)) +
		((cfg_max - 1) *
		sizeof(struct cam_cdm_bl_cmd))), GFP_KERNEL);

	if (!(*cdm_cmd)) {
		CAM_ERR(CAM_ISP, "Failed to allocate cdm bl memory");
		rc = -ENOMEM;
	}

	return rc;
}

static inline void cam_ife_mgr_free_cdm_cmd(
	struct cam_cdm_bl_request **cdm_cmd)
{
	kfree(*cdm_cmd);
	*cdm_cmd = NULL;
}

static int cam_ife_mgr_get_hw_caps(void *hw_mgr_priv,
	void *hw_caps_args)
{
	int rc = 0;
	int i;
	struct cam_ife_hw_mgr             *hw_mgr = hw_mgr_priv;
	struct cam_query_cap_cmd          *query = hw_caps_args;
	struct cam_isp_query_cap_cmd       query_isp;
	struct cam_isp_dev_cap_info       *ife_full_hw_info = NULL;
	struct cam_isp_dev_cap_info       *ife_lite_hw_info = NULL;
	struct cam_isp_dev_cap_info       *csid_full_hw_info = NULL;
	struct cam_isp_dev_cap_info       *csid_lite_hw_info = NULL;
	struct cam_ife_csid_hw_caps       *ife_csid_caps = {0};

	CAM_DBG(CAM_ISP, "enter");

	if (copy_from_user(&query_isp,
		u64_to_user_ptr(query->caps_handle),
		sizeof(struct cam_isp_query_cap_cmd))) {
		rc = -EFAULT;
		return rc;
	}

	query_isp.device_iommu.non_secure = hw_mgr->mgr_common.img_iommu_hdl;
	query_isp.device_iommu.secure = hw_mgr->mgr_common.img_iommu_hdl_secure;
	query_isp.cdm_iommu.non_secure = hw_mgr->mgr_common.cmd_iommu_hdl;
	query_isp.cdm_iommu.secure = hw_mgr->mgr_common.cmd_iommu_hdl_secure;
	query_isp.num_dev = 0;

	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (!hw_mgr->ife_devices[i])
			continue;

		if (hw_mgr->ife_dev_caps[i].is_lite) {
			if (ife_lite_hw_info == NULL) {
				ife_lite_hw_info =
					&query_isp.dev_caps[query_isp.num_dev];
				query_isp.num_dev++;

				ife_lite_hw_info->hw_type = CAM_ISP_HW_IFE_LITE;
				ife_lite_hw_info->hw_version.major =
					hw_mgr->ife_dev_caps[i].major;
				ife_lite_hw_info->hw_version.minor =
					hw_mgr->ife_dev_caps[i].minor;
				ife_lite_hw_info->hw_version.incr =
					hw_mgr->ife_dev_caps[i].incr;
				ife_lite_hw_info->hw_version.reserved = 0;
				ife_lite_hw_info->num_hw = 0;
			}

			ife_lite_hw_info->num_hw++;

		} else {
			if (ife_full_hw_info == NULL) {
				ife_full_hw_info =
					&query_isp.dev_caps[query_isp.num_dev];
				query_isp.num_dev++;

				ife_full_hw_info->hw_type = CAM_ISP_HW_IFE;
				ife_full_hw_info->hw_version.major =
					hw_mgr->ife_dev_caps[i].major;
				ife_full_hw_info->hw_version.minor =
					hw_mgr->ife_dev_caps[i].minor;
				ife_full_hw_info->hw_version.incr =
					hw_mgr->ife_dev_caps[i].incr;
				ife_full_hw_info->hw_version.reserved = 0;
				ife_full_hw_info->num_hw = 0;
			}

			ife_full_hw_info->num_hw++;
		}
	}

	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (!hw_mgr->csid_devices[i])
			continue;

		ife_csid_caps = (struct cam_ife_csid_hw_caps *)
			&hw_mgr->csid_hw_caps[i];

		if (ife_csid_caps->is_lite) {
			if (csid_lite_hw_info == NULL) {
				csid_lite_hw_info =
					&query_isp.dev_caps[query_isp.num_dev];
				query_isp.num_dev++;

				csid_lite_hw_info->hw_type =
					CAM_ISP_HW_CSID_LITE;
				csid_lite_hw_info->hw_version.major =
					ife_csid_caps->major_version;
				csid_lite_hw_info->hw_version.minor =
					ife_csid_caps->minor_version;
				csid_lite_hw_info->hw_version.incr =
					ife_csid_caps->version_incr;
				csid_lite_hw_info->hw_version.reserved = 0;
				csid_lite_hw_info->num_hw = 0;
			}

			csid_lite_hw_info->num_hw++;

		} else {
			if (csid_full_hw_info == NULL) {
				csid_full_hw_info =
					&query_isp.dev_caps[query_isp.num_dev];
				query_isp.num_dev++;

				csid_full_hw_info->hw_type = CAM_ISP_HW_CSID;
				csid_full_hw_info->hw_version.major =
					ife_csid_caps->major_version;
				csid_full_hw_info->hw_version.minor =
					ife_csid_caps->minor_version;
				csid_full_hw_info->hw_version.incr =
					ife_csid_caps->version_incr;
				csid_full_hw_info->hw_version.reserved = 0;
				csid_full_hw_info->num_hw = 0;
			}

			csid_full_hw_info->num_hw++;
		}
	}

	if (copy_to_user(u64_to_user_ptr(query->caps_handle),
		&query_isp, sizeof(struct cam_isp_query_cap_cmd)))
		rc = -EFAULT;

	CAM_DBG(CAM_ISP, "exit rc :%d", rc);

	return rc;
}

static inline int cam_ife_hw_mgr_is_sfe_rdi_for_fetch(
	uint32_t res_id)
{
	int rc = 0;

	switch (res_id) {
	case CAM_ISP_SFE_OUT_RES_RDI_0:
	case CAM_ISP_SFE_OUT_RES_RDI_1:
	case CAM_ISP_SFE_OUT_RES_RDI_2:
		rc = 1;
		break;
	default:
		break;
	}

	return rc;
}

static inline int cam_ife_hw_mgr_is_shdr_fs_rdi_res(
	uint32_t res_id, bool is_sfe_shdr, bool is_sfe_fs)
{
	return (cam_ife_hw_mgr_is_sfe_rdi_for_fetch(res_id) &&
		(is_sfe_shdr || is_sfe_fs));
}

static int cam_ife_hw_mgr_is_sfe_rdi_res(uint32_t res_id)
{
	int rc = 0;

	switch (res_id) {
	case CAM_ISP_SFE_OUT_RES_RDI_0:
	case CAM_ISP_SFE_OUT_RES_RDI_1:
	case CAM_ISP_SFE_OUT_RES_RDI_2:
	case CAM_ISP_SFE_OUT_RES_RDI_3:
	case CAM_ISP_SFE_OUT_RES_RDI_4:
		rc = 1;
		break;
	default:
		break;
	}

	return rc;
}

static int cam_ife_hw_mgr_is_rdi_res(uint32_t res_id)
{
	int rc = 0;

	switch (res_id) {
	case CAM_ISP_IFE_OUT_RES_RDI_0:
	case CAM_ISP_IFE_OUT_RES_RDI_1:
	case CAM_ISP_IFE_OUT_RES_RDI_2:
	case CAM_ISP_IFE_OUT_RES_RDI_3:
		rc = 1;
		break;
	default:
		break;
	}

	return rc;
}

static inline bool cam_ife_hw_mgr_is_ife_out_port(uint32_t res_id)
{
	bool is_ife_out = false;

	if ((res_id >= CAM_ISP_IFE_OUT_RES_BASE) &&
		(res_id <= (CAM_ISP_IFE_OUT_RES_BASE +
		max_ife_out_res)))
		is_ife_out = true;

	return is_ife_out;
}

static inline bool cam_ife_hw_mgr_is_sfe_out_port(uint32_t res_id)
{
	bool is_sfe_out = false;

	if ((res_id >= CAM_ISP_SFE_OUT_RES_BASE) &&
		(res_id < CAM_ISP_SFE_OUT_RES_MAX))
		is_sfe_out = true;

	return is_sfe_out;
}

static int cam_ife_hw_mgr_notify_overflow(
	struct cam_isp_hw_event_info    *evt,
	void                            *ctx)
{
	int                             i;
	int                             res_id;
	int                             ife_res_id = -1;
	int                             sfe_res_id = -1;
	struct cam_hw_intf             *hw_if = NULL;
	struct cam_ife_hw_mgr_ctx      *hw_mgr_ctx = ctx;

	switch(evt->res_id) {
	case  CAM_IFE_PIX_PATH_RES_IPP:
		ife_res_id = CAM_ISP_HW_VFE_IN_CAMIF;
		sfe_res_id = CAM_ISP_HW_SFE_IN_PIX;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_0:
		ife_res_id = CAM_ISP_HW_VFE_IN_RDI0;
		sfe_res_id = CAM_ISP_HW_SFE_IN_RDI0;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_1:
		ife_res_id = CAM_ISP_HW_VFE_IN_RDI1;
		sfe_res_id = CAM_ISP_HW_SFE_IN_RDI1;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_2:
		ife_res_id = CAM_ISP_HW_VFE_IN_RDI2;
		sfe_res_id = CAM_ISP_HW_SFE_IN_RDI2;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_3:
		ife_res_id = CAM_ISP_HW_VFE_IN_RDI3;
		sfe_res_id = CAM_ISP_HW_SFE_IN_RDI3;
		break;
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		sfe_res_id = CAM_ISP_HW_SFE_IN_RDI4;
		break;
	default:
		break;
	}

	for (i = 0; i < hw_mgr_ctx->num_base; i++) {

		res_id = -1;

		if (hw_mgr_ctx->base[i].hw_type == CAM_ISP_HW_TYPE_VFE) {
			if (hw_mgr_ctx->base[i].idx != evt->hw_idx)
				continue;

			hw_if = g_ife_hw_mgr.ife_devices[evt->hw_idx]->hw_intf;
			res_id = ife_res_id;
		} else if (hw_mgr_ctx->base[i].hw_type == CAM_ISP_HW_TYPE_SFE) {
			if (hw_mgr_ctx->base[i].idx != evt->in_core_idx)
				continue;

			hw_if = g_ife_hw_mgr.sfe_devices[evt->in_core_idx]->hw_intf;
			res_id = sfe_res_id;
		} else {
			continue;
		}

		if (!hw_if) {
			CAM_ERR_RATE_LIMIT(CAM_ISP, "hw_intf is null");
			return -EINVAL;
		}

		if (hw_if->hw_ops.process_cmd)
			hw_if->hw_ops.process_cmd(hw_if->hw_priv,
				CAM_ISP_HW_NOTIFY_OVERFLOW,
				&res_id, sizeof(int));
	}

	return 0;
}

static enum cam_ife_pix_path_res_id
	cam_ife_hw_mgr_get_csid_rdi_type_for_offline(
	uint32_t                 rd_res_type)
{
	enum cam_ife_pix_path_res_id path_id;

	/* Allow only RD0 for offline */
	switch (rd_res_type) {
	case CAM_ISP_SFE_IN_RD_0:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_0;
		break;
	default:
		path_id = CAM_IFE_PIX_PATH_RES_MAX;
		CAM_ERR(CAM_ISP,
			"maximum rdi output type exceeded 0x%x",
			rd_res_type);
		break;
	}

	CAM_DBG(CAM_ISP, "out_port: %x path_id: %d",
		rd_res_type, path_id);

	return path_id;
}

static enum cam_isp_hw_sfe_in cam_ife_hw_mgr_get_sfe_rd_res_id(
	uint32_t sfe_in_path_type)
{
	enum cam_isp_hw_sfe_in path_id;

	switch (sfe_in_path_type) {
	case CAM_ISP_SFE_IN_RD_0:
		path_id = CAM_ISP_HW_SFE_IN_RD0;
		break;
	case CAM_ISP_SFE_IN_RD_1:
		path_id = CAM_ISP_HW_SFE_IN_RD1;
		break;
	case CAM_ISP_SFE_IN_RD_2:
		path_id = CAM_ISP_HW_SFE_IN_RD2;
		break;
	default:
		path_id = CAM_ISP_HW_SFE_IN_MAX;
		break;
	}

	CAM_DBG(CAM_ISP,
		"sfe_in_path_type: 0x%x path_id: 0x%x",
		sfe_in_path_type, path_id);

	return path_id;
}

static int cam_ife_hw_mgr_reset_csid(
	struct cam_ife_hw_mgr_ctx  *ctx,
	int reset_type)
{
	int i;
	int rc = 0;
	struct cam_hw_intf      *hw_intf;
	struct cam_csid_reset_cfg_args  reset_args;
	struct cam_isp_hw_mgr_res *hw_mgr_res;
	struct cam_ife_hw_mgr          *hw_mgr;
	bool hw_idx_map[CAM_IFE_CSID_HW_NUM_MAX] = {0};

	hw_mgr = ctx->hw_mgr;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {

			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;

			if ((hw_mgr->csid_global_reset_en) &&
				(hw_idx_map[hw_intf->hw_idx]))
				continue;

			reset_args.reset_type = reset_type;
			reset_args.node_res = hw_mgr_res->hw_res[i];
			rc  = hw_intf->hw_ops.reset(hw_intf->hw_priv,
				&reset_args, sizeof(reset_args));
			if (rc)
				goto err;
			hw_idx_map[hw_intf->hw_idx] = true;
		}
	}

	return rc;
err:
	CAM_ERR(CAM_ISP, "RESET HW res failed: (type:%d, id:%d)",
		hw_mgr_res->res_type, hw_mgr_res->res_id);
	return rc;
}

static int cam_ife_hw_mgr_init_hw_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res)
{
	int i;
	int rc = -1;
	struct cam_hw_intf      *hw_intf;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		CAM_DBG(CAM_ISP, "enabled vfe hardware %d",
			hw_intf->hw_idx);
		if (hw_intf->hw_ops.init) {
			rc = hw_intf->hw_ops.init(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
			if (rc)
				goto err;
		}
	}

	return 0;
err:
	CAM_ERR(CAM_ISP, "INIT HW res failed: (type:%d, id:%d)",
		isp_hw_res->res_type, isp_hw_res->res_id);
	return rc;
}

static int cam_ife_mgr_csid_start_hw(
	struct cam_ife_hw_mgr_ctx *ctx,
	uint32_t primary_rdi_csid_res)
{
	struct cam_isp_hw_mgr_res      *hw_mgr_res;
	struct cam_isp_resource_node   *isp_res;
	struct cam_isp_resource_node   *res[CAM_IFE_PIX_PATH_RES_MAX - 1];
	struct cam_csid_hw_start_args   start_args;
	struct cam_hw_intf             *hw_intf;
	uint32_t  cnt;
	int j;

	for (j = ctx->num_base - 1 ; j >= 0; j--) {
		cnt = 0;

		if (ctx->base[j].hw_type != CAM_ISP_HW_TYPE_CSID)
			continue;

		list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
			isp_res = hw_mgr_res->hw_res[ctx->base[j].split_id];

			if (!isp_res || ctx->base[j].idx != isp_res->hw_intf->hw_idx)
				continue;

			if (primary_rdi_csid_res == hw_mgr_res->res_id) {
				hw_mgr_res->hw_res[0]->rdi_only_ctx =
				ctx->flags.is_rdi_only_context;
			}

			CAM_DBG(CAM_ISP, "csid[%u] res:%s res_id %d cnt %u",
				isp_res->hw_intf->hw_idx,
				isp_res->res_name, isp_res->res_id, cnt);
			res[cnt] = isp_res;
			cnt++;
		}

		if (cnt) {
			hw_intf =  res[0]->hw_intf;
			start_args.num_res = cnt;
			start_args.node_res = res;
			hw_intf->hw_ops.start(hw_intf->hw_priv, &start_args,
			    sizeof(start_args));
		}
	}

	return 0;
}

static int cam_ife_hw_mgr_start_hw_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res,
	struct cam_ife_hw_mgr_ctx   *ctx)
{
	int i;
	int rc = -1;
	struct cam_hw_intf      *hw_intf;

	/* Start slave (which is right split) first */
	for (i = CAM_ISP_HW_SPLIT_MAX - 1; i >= 0; i--) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.start) {
			rc = hw_intf->hw_ops.start(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Can not start HW:%d resources",
					hw_intf->hw_idx);
				goto err;
			}
		} else {
			CAM_ERR(CAM_ISP, "function null");
			goto err;
		}
	}

	return 0;
err:
	CAM_ERR(CAM_ISP, "Start hw res failed (type:%d, id:%d)",
		isp_hw_res->res_type, isp_hw_res->res_id);
	return rc;
}

static void cam_ife_hw_mgr_stop_hw_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res)
{
	int i;
	struct cam_hw_intf      *hw_intf;
	uint32_t dummy_args;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		isp_hw_res->hw_res[i]->rdi_only_ctx = false;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;

		if (isp_hw_res->hw_res[i]->res_state !=
			CAM_ISP_RESOURCE_STATE_STREAMING)
			continue;

		if (hw_intf->hw_ops.stop) {
			hw_intf->hw_ops.stop(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
		}
		else
			CAM_ERR(CAM_ISP, "stop null");
		if (hw_intf->hw_ops.process_cmd &&
			isp_hw_res->res_type == CAM_ISP_RESOURCE_VFE_OUT) {
			hw_intf->hw_ops.process_cmd(hw_intf->hw_priv,
				CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ,
				&dummy_args, sizeof(dummy_args));
		}
	}
}

static void cam_ife_hw_mgr_deinit_hw_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res)
{
	int i;
	struct cam_hw_intf      *hw_intf;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.deinit)
			hw_intf->hw_ops.deinit(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
	}
}

static void cam_ife_hw_mgr_deinit_hw(
	struct cam_ife_hw_mgr_ctx *ctx)
{
	struct cam_isp_hw_mgr_res *hw_mgr_res;
	struct cam_ife_hw_mgr          *hw_mgr;
	int i = 0;

	if (!ctx->flags.init_done) {
		CAM_WARN(CAM_ISP, "ctx is not in init state");
		return;
	}

	hw_mgr = ctx->hw_mgr;

	if (hw_mgr->csid_global_reset_en)
		cam_ife_hw_mgr_reset_csid(ctx, CAM_IFE_CSID_RESET_GLOBAL);

	/* Deinit IFE CSID */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		CAM_DBG(CAM_ISP, "%s: Going to DeInit IFE CSID\n", __func__);
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	if (ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
		/* Deint SFE RSRC */
		list_for_each_entry(hw_mgr_res, &ctx->res_list_sfe_src, list) {
			CAM_DBG(CAM_ISP, "Going to DeInit SFE SRC %u",
				hw_mgr_res->res_id);
			cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
		}

		/* Deinit SFE OUT */
		for (i = 0; i < CAM_SFE_HW_OUT_RES_MAX; i++)
			cam_ife_hw_mgr_deinit_hw_res(&ctx->res_list_sfe_out[i]);
	}

	/* Deint BUS RD */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		CAM_DBG(CAM_ISP, "Going to DeInit BUS RD %u",
			hw_mgr_res->res_id);
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deint IFE MUX(SRC) */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		CAM_DBG(CAM_ISP, "Going to DeInit IFE SRC %u",
			hw_mgr_res->res_id);
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deinit IFE OUT */
	for (i = 0; i < max_ife_out_res; i++)
		cam_ife_hw_mgr_deinit_hw_res(&ctx->res_list_ife_out[i]);

	/* Check if any cache needs to be de-activated */
	for (i = CAM_LLCC_SMALL_1; i < CAM_LLCC_MAX; i++) {
		if (ctx->flags.sys_cache_usage[i])
			cam_cpas_deactivate_llcc(i);
		ctx->flags.sys_cache_usage[i] = false;
	}
	ctx->flags.init_done = false;
}

static int cam_ife_hw_mgr_init_hw(
	struct cam_ife_hw_mgr_ctx *ctx)
{
	struct cam_isp_hw_mgr_res *hw_mgr_res;
	struct cam_ife_hw_mgr          *hw_mgr;
	int rc = 0, i;

	/* INIT IFE SRC */
	CAM_DBG(CAM_ISP, "INIT IFE SRC in ctx id:%d",
		ctx->ctx_index);
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT IFE SRC (%d)",
				 hw_mgr_res->res_id);
			goto deinit;
		}
	}

	if (ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
		/* INIT SFE RSRC */
		CAM_DBG(CAM_ISP, "INIT SFE Resource in ctx id:%d",
			ctx->ctx_index);
		list_for_each_entry(hw_mgr_res, &ctx->res_list_sfe_src, list) {
			rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Can not INIT SFE SRC res (%d)",
					hw_mgr_res->res_id);
				goto deinit;
			}
		}

		CAM_DBG(CAM_ISP, "INIT SFE OUT RESOURCES in ctx id:%d",
			ctx->ctx_index);
		for (i = 0; i < CAM_SFE_HW_OUT_RES_MAX; i++) {
			rc = cam_ife_hw_mgr_init_hw_res(
				&ctx->res_list_sfe_out[i]);
			if (rc) {
				CAM_ERR(CAM_ISP, "Can not INIT SFE OUT (%d)",
					ctx->res_list_sfe_out[i].res_id);
				goto deinit;
			}
		}
	}

	CAM_DBG(CAM_ISP, "INIT IFE csid ... in ctx id:%d",
	ctx->ctx_index);

	/* INIT IFE BUS RD */
	CAM_DBG(CAM_ISP, "INIT IFE BUS RD in ctx id:%d",
		ctx->ctx_index);
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not IFE BUS RD (%d)",
				 hw_mgr_res->res_id);
			return rc;
		}
	}

	/* INIT IFE OUT */
	CAM_DBG(CAM_ISP, "INIT IFE OUT RESOURCES in ctx id:%d",
		ctx->ctx_index);

	for (i = 0; i < max_ife_out_res; i++) {
		rc = cam_ife_hw_mgr_init_hw_res(&ctx->res_list_ife_out[i]);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT IFE OUT (%d)",
				 ctx->res_list_ife_out[i].res_id);
			goto deinit;
		}
	}

	/* INIT IFE csid */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT IFE CSID(id :%d)",
				 hw_mgr_res->res_id);
			goto deinit;
		}
	}

	hw_mgr = ctx->hw_mgr;

	if (hw_mgr->csid_global_reset_en) {
		rc = cam_ife_hw_mgr_reset_csid(ctx,
			CAM_IFE_CSID_RESET_GLOBAL);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID reset failed");
			goto deinit;
		}
	}

	/* Check if any cache needs to be activated */
	for (i = CAM_LLCC_SMALL_1; i < CAM_LLCC_MAX; i++) {
		if (ctx->flags.sys_cache_usage[i]) {
			rc = cam_cpas_activate_llcc(i);
			if (rc) {
				CAM_ERR(CAM_ISP,
				"Failed to activate cache: %d", i);
				goto deinit;
			}
		}
	}

	return rc;
deinit:
	ctx->flags.init_done = true;
	cam_ife_hw_mgr_deinit_hw(ctx);
	return rc;
}

static int cam_ife_hw_mgr_put_res(
	struct list_head                *src_list,
	struct cam_isp_hw_mgr_res      **res)
{
	int rc                              = 0;
	struct cam_isp_hw_mgr_res *res_ptr  = NULL;

	res_ptr = *res;
	if (res_ptr)
		list_add_tail(&res_ptr->list, src_list);

	return rc;
}

static int cam_ife_hw_mgr_get_res(
	struct list_head                *src_list,
	struct cam_isp_hw_mgr_res      **res)
{
	int rc = 0;
	struct cam_isp_hw_mgr_res *res_ptr  = NULL;

	if (!list_empty(src_list)) {
		res_ptr = list_first_entry(src_list,
			struct cam_isp_hw_mgr_res, list);
		list_del_init(&res_ptr->list);
	} else {
		CAM_ERR(CAM_ISP, "No more free ife hw mgr ctx");
		rc = -1;
	}
	*res = res_ptr;

	return rc;
}

static int cam_ife_hw_mgr_free_hw_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res)
{
	int rc = 0;
	int i;
	struct cam_hw_intf      *hw_intf;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.release) {
			rc = hw_intf->hw_ops.release(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
			if (rc)
				CAM_ERR(CAM_ISP,
					"Release HW:%d Res: %s resource id %d failed",
					hw_intf->hw_idx,
					isp_hw_res->hw_res[i]->res_name,
					isp_hw_res->res_id);
			isp_hw_res->hw_res[i] = NULL;
		} else
			CAM_ERR(CAM_ISP, "Release null");
	}
	/* caller should make sure the resource is in a list */
	list_del_init(&isp_hw_res->list);
	memset(isp_hw_res, 0, sizeof(*isp_hw_res));
	INIT_LIST_HEAD(&isp_hw_res->list);

	return 0;
}

static const char *cam_ife_hw_mgr_get_res_state(
	uint32_t res_state)
{
	switch (res_state) {
	case CAM_ISP_RESOURCE_STATE_UNAVAILABLE:
		return "UNAVAILABLE";
	case CAM_ISP_RESOURCE_STATE_AVAILABLE:
		return "AVAILABLE";
	case CAM_ISP_RESOURCE_STATE_RESERVED:
		return "RESERVED";
	case CAM_ISP_RESOURCE_STATE_INIT_HW:
		return "HW INIT DONE";
	case CAM_ISP_RESOURCE_STATE_STREAMING:
		return "STREAMING";
	default:
		return "INVALID STATE";
	}
}

static inline bool cam_ife_hw_mgr_check_path_port_compat(
	uint32_t in_type, uint32_t out_type)
{
	int i;
	const struct cam_isp_hw_path_port_map *map = &g_ife_hw_mgr.path_port_map;

	for (i = 0; i < map->num_entries; i++) {
		if (map->entry[i][1] == out_type)
			return (map->entry[i][0] == in_type);
	}

	return (in_type == CAM_ISP_HW_VFE_IN_CAMIF);
}

static void cam_ife_hw_mgr_dump_all_ctx(void)
{
	uint32_t i;
	struct cam_ife_hw_mgr_ctx       *ctx;
	struct cam_isp_hw_mgr_res       *hw_mgr_res;
	struct cam_isp_hw_mgr_res       *hw_mgr_res_temp;
	struct cam_ife_hw_mgr_ctx       *ctx_temp;

	mutex_lock(&g_ife_hw_mgr.ctx_mutex);
	if (list_empty(&g_ife_hw_mgr.used_ctx_list)) {
		CAM_INFO(CAM_ISP, "Currently no ctx in use");
		mutex_unlock(&g_ife_hw_mgr.ctx_mutex);
		return;
	}

	list_for_each_entry_safe(ctx, ctx_temp,
		&g_ife_hw_mgr.used_ctx_list, list) {
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"ctx id:%d is_dual:%d num_base:%d rdi only:%d",
			ctx->ctx_index, ctx->flags.is_dual,
			ctx->num_base, ctx->flags.is_rdi_only_context);

		list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
			&ctx->res_list_ife_csid, list) {
			for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
				if (!hw_mgr_res->hw_res[i])
					continue;

				CAM_INFO_RATE_LIMIT(CAM_ISP,
					"csid:%d res_type:%d res: %s res_id:%d res_state:%d",
					hw_mgr_res->hw_res[i]->hw_intf->hw_idx,
					hw_mgr_res->hw_res[i]->res_type,
					hw_mgr_res->hw_res[i]->res_name,
					hw_mgr_res->hw_res[i]->res_id,
					hw_mgr_res->hw_res[i]->res_state);
			}
		}

		list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
			&ctx->res_list_ife_src, list) {
			for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
				if (!hw_mgr_res->hw_res[i])
					continue;

				CAM_INFO_RATE_LIMIT(CAM_ISP,
					"ife IN:%d res_type:%d res_id:%d res_state:%d",
					hw_mgr_res->hw_res[i]->hw_intf->hw_idx,
					hw_mgr_res->hw_res[i]->res_type,
					hw_mgr_res->hw_res[i]->res_id,
					hw_mgr_res->hw_res[i]->res_state);
			}
		}

		list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
			&ctx->res_list_sfe_src, list) {
			for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
				if (!hw_mgr_res->hw_res[i])
					continue;

				CAM_INFO_RATE_LIMIT(CAM_ISP,
					"sfe IN:%d res_type:%d res_id:%d res_state:%d",
					hw_mgr_res->hw_res[i]->hw_intf->hw_idx,
					hw_mgr_res->hw_res[i]->res_type,
					hw_mgr_res->hw_res[i]->res_id,
					hw_mgr_res->hw_res[i]->res_state);
			}
		}
	}
	mutex_unlock(&g_ife_hw_mgr.ctx_mutex);
}

static void cam_ife_hw_mgr_print_acquire_info(
	struct cam_ife_hw_mgr_ctx *hw_mgr_ctx, uint32_t num_pix_port,
	uint32_t num_pd_port, uint32_t num_rdi_port, int acquire_failed)
{
	struct cam_isp_hw_mgr_res    *hw_mgr_res = NULL;
	struct cam_isp_hw_mgr_res    *hw_mgr_res_temp = NULL;
	struct cam_isp_resource_node *hw_res = NULL;
	char log_info[128];
	int hw_idx[CAM_ISP_HW_SPLIT_MAX] = {-1, -1};
	int sfe_hw_idx[CAM_ISP_HW_SPLIT_MAX] = {-1, -1};
	int i, len = 0;
	uint64_t ms, sec, min, hrs;

	if (!list_empty(&hw_mgr_ctx->res_list_ife_src)) {
		hw_mgr_res = list_first_entry(&hw_mgr_ctx->res_list_ife_src,
			struct cam_isp_hw_mgr_res, list);

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				hw_idx[i] = hw_res->hw_intf->hw_idx;
		}
	}

	if (!list_empty(&hw_mgr_ctx->res_list_sfe_src)) {
		hw_mgr_res = list_first_entry(&hw_mgr_ctx->res_list_sfe_src,
			struct cam_isp_hw_mgr_res, list);

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				sfe_hw_idx[i] = hw_res->hw_intf->hw_idx;
		}
	}

	if (acquire_failed)
		goto fail;

	/* 128 char in log_info */
	if (hw_mgr_ctx->flags.is_dual) {
		len += scnprintf(log_info + len, (128 - len), "Dual IFE[%d: %d]",
			hw_idx[CAM_ISP_HW_SPLIT_LEFT],
			hw_idx[CAM_ISP_HW_SPLIT_RIGHT]);
		if (hw_mgr_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE)
			len += scnprintf(log_info + len, (128 - len), " SFE[%d: %d]",
				sfe_hw_idx[CAM_ISP_HW_SPLIT_LEFT],
				sfe_hw_idx[CAM_ISP_HW_SPLIT_RIGHT]);
	} else {
		len += scnprintf(log_info + len, (128 - len), "Single IFE[%d]",
			hw_idx[CAM_ISP_HW_SPLIT_LEFT]);
		if (hw_mgr_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE)
			len += scnprintf(log_info + len, (128 - len), " SFE[%d]",
				sfe_hw_idx[CAM_ISP_HW_SPLIT_LEFT]);
	}

	if (hw_mgr_ctx->flags.is_sfe_shdr)
		len += scnprintf(log_info + len, (128 - len), " sHDR: Y");

	if (hw_mgr_ctx->flags.is_sfe_fs)
		len += scnprintf(log_info + len, (128 - len), " SFE_FS: Y");

	if (hw_mgr_ctx->flags.dsp_enabled)
		len += scnprintf(log_info + len, (128 - len), " DSP: Y");

	if (hw_mgr_ctx->flags.is_offline)
		len += scnprintf(log_info + len, (128 - len), " OFFLINE: Y");

	CAM_GET_TIMESTAMP(hw_mgr_ctx->ts);
	CAM_CONVERT_TIMESTAMP_FORMAT(hw_mgr_ctx->ts, hrs, min, sec, ms);

	CAM_INFO(CAM_ISP,
		"%llu:%llu:%llu.%llu Acquired %s with [%u pix] [%u pd] [%u rdi] ports for ctx:%u",
		hrs, min, sec, ms,
		log_info,
		num_pix_port, num_pd_port, num_rdi_port,
		hw_mgr_ctx->ctx_index);

	return;

fail:
	CAM_ERR(CAM_ISP,
		"Failed to acquire %s-IFE/SFE with [%u pix] [%u pd] [%u rdi] ports for ctx:%u",
		(hw_mgr_ctx->flags.is_dual) ? "dual" : "single",
		num_pix_port, num_pd_port, num_rdi_port, hw_mgr_ctx->ctx_index);
	CAM_INFO(CAM_ISP, "Previously acquired IFEs[%d %d] SFEs[%d %d]",
		hw_idx[CAM_ISP_HW_SPLIT_LEFT], hw_idx[CAM_ISP_HW_SPLIT_RIGHT],
		sfe_hw_idx[CAM_ISP_HW_SPLIT_LEFT], sfe_hw_idx[CAM_ISP_HW_SPLIT_RIGHT]);

	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&hw_mgr_ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"IFE src split_id:%d res:%s hw_idx:%u state:%s",
					i,
					hw_res->res_name,
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}

	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&hw_mgr_ctx->res_list_sfe_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"SFE src split_id:%d res:%s hw_idx:%u state:%s",
					i,
					hw_res->res_name,
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}
	cam_ife_hw_mgr_dump_all_ctx();
}

static void cam_ife_hw_mgr_dump_acq_data(
	struct cam_ife_hw_mgr_ctx    *hwr_mgr_ctx)
{
	struct cam_isp_hw_mgr_res    *hw_mgr_res = NULL;
	struct cam_isp_hw_mgr_res    *hw_mgr_res_temp = NULL;
	struct cam_isp_resource_node *hw_res = NULL;
	uint64_t ms, hrs, min, sec;
	int i = 0, j = 0;

	CAM_CONVERT_TIMESTAMP_FORMAT(hwr_mgr_ctx->ts, hrs, min, sec, ms);

	CAM_INFO(CAM_ISP,
		"**** %llu:%llu:%llu.%llu ctx_idx: %u rdi_only: %s is_dual: %s acquired ****",
		hrs, min, sec, ms,
		hwr_mgr_ctx->ctx_index,
		(hwr_mgr_ctx->flags.is_rdi_only_context ? "true" : "false"),
		(hwr_mgr_ctx->flags.is_dual ? "true" : "false"));

	/* Iterate over CSID resources */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&hwr_mgr_ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"CSID split_id: %d res: %s hw_idx: %u state: %s",
					i,
					hw_res->res_name,
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}

	/* Iterate over IFE IN resources */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&hwr_mgr_ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"IFE src split_id: %d res: %s hw_idx: %u state: %s",
					i,
					hw_res->res_name,
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}

	/* Iterate over IFE RD resources */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&hwr_mgr_ctx->res_list_ife_in_rd, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"IFE src_rd split_id: %d res: %s hw_idx: %u state: %s",
					i,
					hw_res->res_name,
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}

	/* Iterate over IFE OUT resources */
	for (i = 0; i < max_ife_out_res; i++) {
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			hw_mgr_res = &hwr_mgr_ctx->res_list_ife_out[i];
			hw_res = hw_mgr_res->hw_res[j];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"IFE out split_id: %d res: %s res_id: 0x%x hw_idx: %u state: %s",
					j, hw_res->res_name, hw_res->res_id,
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}
}

static int cam_ife_mgr_csid_change_halt_mode(struct cam_ife_hw_mgr_ctx *ctx,
	enum cam_ife_csid_halt_mode halt_mode)
{
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_isp_resource_node     *isp_res;
	struct cam_ife_csid_hw_halt_args halt;
	struct cam_hw_intf               *hw_intf;
	uint32_t i;
	int rc = 0;

	if (!ctx->flags.is_dual)
		return 0;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (i == CAM_ISP_HW_SPLIT_LEFT)
				continue;

			if (!hw_mgr_res->hw_res[i] ||
				(hw_mgr_res->hw_res[i]->res_state !=
				CAM_ISP_RESOURCE_STATE_STREAMING))
				continue;

			isp_res = hw_mgr_res->hw_res[i];

			if ((isp_res->res_type == CAM_ISP_RESOURCE_PIX_PATH) &&
				(isp_res->res_id == CAM_IFE_PIX_PATH_RES_IPP)) {
				hw_intf         = isp_res->hw_intf;
				halt.node_res   = isp_res;
				halt.halt_mode  = halt_mode;
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CSID_CHANGE_HALT_MODE,
					&halt,
					sizeof(
					struct cam_ife_csid_hw_halt_args));
				if (rc)
					CAM_ERR(CAM_ISP, "Halt update failed");
				break;
			}
		}
	}

	return rc;
}

static int cam_ife_mgr_csid_stop_hw(
	struct cam_ife_hw_mgr_ctx *ctx, struct list_head  *stop_list,
		uint32_t  base_idx, uint32_t stop_cmd)
{
	struct cam_isp_hw_mgr_res      *hw_mgr_res;
	struct cam_isp_resource_node   *isp_res;
	struct cam_isp_resource_node   *stop_res[CAM_IFE_PIX_PATH_RES_MAX - 1];
	struct cam_csid_hw_stop_args    stop;
	struct cam_hw_intf             *hw_intf;
	uint32_t i, cnt;

	cnt = 0;
	list_for_each_entry(hw_mgr_res, stop_list, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i] ||
				(hw_mgr_res->hw_res[i]->res_state !=
				CAM_ISP_RESOURCE_STATE_STREAMING))
				continue;

			isp_res = hw_mgr_res->hw_res[i];
			isp_res->rdi_only_ctx = false;
			if (isp_res->hw_intf->hw_idx != base_idx)
				continue;
			CAM_DBG(CAM_ISP, "base_idx %d res:%s res_id %d cnt %u",
				base_idx, isp_res->res_name,
				isp_res->res_id, cnt);
			stop_res[cnt] = isp_res;
			cnt++;
		}
	}

	if (cnt) {
		hw_intf =  stop_res[0]->hw_intf;
		stop.num_res = cnt;
		stop.node_res = stop_res;
		stop.stop_cmd = stop_cmd;
		hw_intf->hw_ops.stop(hw_intf->hw_priv, &stop, sizeof(stop));
	}

	return 0;
}

static int cam_ife_hw_mgr_release_hw_for_ctx(
	struct cam_ife_hw_mgr_ctx  *ife_ctx)
{
	uint32_t                          i;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_isp_hw_mgr_res        *hw_mgr_res_temp;

	/* ife leaf resource */
	for (i = 0; i < max_ife_out_res; i++) {
		cam_ife_hw_mgr_free_hw_res(&ife_ctx->res_list_ife_out[i]);
		ife_ctx->num_acq_vfe_out--;
	}

	/* fetch rd resource */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&ife_ctx->res_list_ife_in_rd, list) {
		cam_ife_hw_mgr_free_hw_res(hw_mgr_res);
		cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &hw_mgr_res);
	}

	/* ife source resource */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&ife_ctx->res_list_ife_src, list) {
		cam_ife_hw_mgr_free_hw_res(hw_mgr_res);
		cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &hw_mgr_res);
	}

	if (ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
		/* sfe leaf resource */
		for (i = 0; i < CAM_SFE_HW_OUT_RES_MAX; i++) {
			cam_ife_hw_mgr_free_hw_res(
				&ife_ctx->res_list_sfe_out[i]);
			ife_ctx->num_acq_sfe_out--;
		}

		/* sfe source resource */
		list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
			&ife_ctx->res_list_sfe_src, list) {
			cam_ife_hw_mgr_free_hw_res(hw_mgr_res);
			cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list,
				&hw_mgr_res);
		}
	}

	/* ife csid resource */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&ife_ctx->res_list_ife_csid, list) {
		cam_ife_hw_mgr_free_hw_res(hw_mgr_res);
		cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &hw_mgr_res);
	}

	/* ife root node */
	if (ife_ctx->res_list_ife_in.res_type != CAM_ISP_RESOURCE_UNINT)
		cam_ife_hw_mgr_free_hw_res(&ife_ctx->res_list_ife_in);

	/* clean up the callback function */
	ife_ctx->common.cb_priv = NULL;
	ife_ctx->common.event_cb = NULL;

	ife_ctx->flags.need_csid_top_cfg = false;

	CAM_DBG(CAM_ISP, "release context completed ctx id:%d",
		ife_ctx->ctx_index);

	return 0;
}


static int cam_ife_hw_mgr_put_ctx(
	struct list_head                 *src_list,
	struct cam_ife_hw_mgr_ctx       **ife_ctx)
{
	int rc                              = 0;
	struct cam_ife_hw_mgr_ctx *ctx_ptr  = NULL;

	mutex_lock(&g_ife_hw_mgr.ctx_mutex);
	ctx_ptr = *ife_ctx;
	if (ctx_ptr)
		list_add_tail(&ctx_ptr->list, src_list);
	*ife_ctx = NULL;
	mutex_unlock(&g_ife_hw_mgr.ctx_mutex);
	return rc;
}

static int cam_ife_hw_mgr_get_ctx(
	struct list_head                *src_list,
	struct cam_ife_hw_mgr_ctx       **ife_ctx)
{
	int rc                              = 0;
	struct cam_ife_hw_mgr_ctx *ctx_ptr  = NULL;

	mutex_lock(&g_ife_hw_mgr.ctx_mutex);
	if (!list_empty(src_list)) {
		ctx_ptr = list_first_entry(src_list,
			struct cam_ife_hw_mgr_ctx, list);
		list_del_init(&ctx_ptr->list);
	} else {
		CAM_ERR(CAM_ISP, "No more free ife hw mgr ctx");
		rc = -1;
	}
	*ife_ctx = ctx_ptr;
	mutex_unlock(&g_ife_hw_mgr.ctx_mutex);

	return rc;
}

static void cam_ife_mgr_add_base_info(
	struct cam_ife_hw_mgr_ctx       *ctx,
	enum cam_isp_hw_split_id         split_id,
	uint32_t                         base_idx,
	enum cam_isp_hw_type             hw_type)
{
	uint32_t    i;

	if (!ctx->num_base) {
		ctx->base[0].split_id = split_id;
		ctx->base[0].idx      = base_idx;
		ctx->base[0].hw_type = hw_type;
		ctx->num_base++;
		CAM_DBG(CAM_ISP,
			"Add split id = %d for base idx = %d num_base=%d hw_type=%d",
			split_id, base_idx, ctx->num_base, hw_type);
	} else {
		/*Check if base index already exists in the list */
		for (i = 0; i < ctx->num_base; i++) {
			if ((ctx->base[i].idx == base_idx) &&
				(ctx->base[i].hw_type == hw_type)) {
				if (split_id != CAM_ISP_HW_SPLIT_MAX &&
					ctx->base[i].split_id ==
						CAM_ISP_HW_SPLIT_MAX)
					ctx->base[i].split_id = split_id;

				break;
			}
		}

		if (i == ctx->num_base) {
			ctx->base[ctx->num_base].split_id = split_id;
			ctx->base[ctx->num_base].idx      = base_idx;
			ctx->base[ctx->num_base].hw_type = hw_type;
			ctx->num_base++;
			CAM_DBG(CAM_ISP,
				"Add split_id=%d for base idx=%d num_base=%d hw_type=%d",
				 split_id, base_idx, ctx->num_base, hw_type);
		}
	}
}

/* Update base info for IFE & SFE HWs */
static int cam_ife_mgr_process_base_info(
	struct cam_ife_hw_mgr_ctx        *ctx)
{
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_isp_resource_node     *res = NULL;
	uint32_t i;
	struct cam_ife_csid_hw_caps      *csid_caps = NULL;
	struct cam_ife_hw_mgr            *hw_mgr;
	bool   hw_idx_map[CAM_IFE_CSID_HW_NUM_MAX] = {0};

	if (list_empty(&ctx->res_list_ife_src) &&
		list_empty(&ctx->res_list_sfe_src)) {
		CAM_ERR(CAM_ISP, "Mux List empty");
		return -ENODEV;
	}

	hw_mgr = ctx->hw_mgr;

	/* IFE mux in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		if (hw_mgr_res->res_type == CAM_ISP_RESOURCE_UNINT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			res = hw_mgr_res->hw_res[i];
			cam_ife_mgr_add_base_info(ctx, i,
				res->hw_intf->hw_idx,
				CAM_ISP_HW_TYPE_VFE);
			CAM_DBG(CAM_ISP, "add IFE base info for hw %d",
				res->hw_intf->hw_idx);
		}
	}

	/*CSID resources */

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		if (hw_mgr_res->res_type == CAM_ISP_RESOURCE_UNINT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			res = hw_mgr_res->hw_res[i];

			if (hw_idx_map[res->hw_intf->hw_idx])
				continue;

			csid_caps = &hw_mgr->csid_hw_caps[res->hw_intf->hw_idx];

			cam_ife_mgr_add_base_info(ctx, i,
				res->hw_intf->hw_idx,
				CAM_ISP_HW_TYPE_CSID);
			hw_idx_map[res->hw_intf->hw_idx] = true;
			CAM_DBG(CAM_ISP, "add CSID base info for hw %d",
				res->hw_intf->hw_idx);
		}
	}

	/* SFE in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_sfe_src, list) {
		if (hw_mgr_res->res_type == CAM_ISP_RESOURCE_UNINT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			res = hw_mgr_res->hw_res[i];
			cam_ife_mgr_add_base_info(ctx, i,
				res->hw_intf->hw_idx,
				CAM_ISP_HW_TYPE_SFE);
			CAM_DBG(CAM_ISP, "add SFE base info for hw %d",
				res->hw_intf->hw_idx);
		}
	}
	CAM_DBG(CAM_ISP, "ctx base num = %d", ctx->num_base);

	return 0;
}

static int cam_ife_hw_mgr_acquire_res_ife_out_rdi(
	struct cam_ife_hw_mgr_ctx       *ife_ctx,
	struct cam_isp_hw_mgr_res       *ife_src_res,
	struct cam_isp_in_port_generic_info     *in_port)
{
	int rc = -EINVAL;
	struct cam_vfe_acquire_args               vfe_acquire;
	struct cam_isp_out_port_generic_info     *out_port = NULL;
	struct cam_isp_hw_mgr_res                *ife_out_res;
	struct cam_hw_intf                       *hw_intf;
	uint32_t  i, vfe_out_res_id, vfe_in_res_id;

	/* take left resource */
	vfe_in_res_id = ife_src_res->hw_res[0]->res_id;

	switch (vfe_in_res_id) {
	case CAM_ISP_HW_VFE_IN_RDI0:
		vfe_out_res_id = CAM_ISP_IFE_OUT_RES_RDI_0;
		break;
	case CAM_ISP_HW_VFE_IN_RDI1:
		vfe_out_res_id = CAM_ISP_IFE_OUT_RES_RDI_1;
		break;
	case CAM_ISP_HW_VFE_IN_RDI2:
		vfe_out_res_id = CAM_ISP_IFE_OUT_RES_RDI_2;
		break;
	case CAM_ISP_HW_VFE_IN_RDI3:
		vfe_out_res_id = CAM_ISP_IFE_OUT_RES_RDI_3;
		break;
	default:
		CAM_ERR(CAM_ISP, "invalid resource type");
		goto err;
	}
	CAM_DBG(CAM_ISP, "vfe_in_res_id = %d, vfe_out_red_id = %d",
		vfe_in_res_id, vfe_out_res_id);

	vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_OUT;
	vfe_acquire.tasklet = ife_ctx->common.tasklet_info;

	ife_out_res = &ife_ctx->res_list_ife_out[vfe_out_res_id & 0xFF];
	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];

		CAM_DBG(CAM_ISP, "i = %d, vfe_out_res_id = %d, out_port: %d",
			i, vfe_out_res_id, out_port->res_type);

		if (vfe_out_res_id != out_port->res_type)
			continue;

		vfe_acquire.vfe_out.cdm_ops = ife_ctx->cdm_ops;
		vfe_acquire.priv = ife_ctx;
		vfe_acquire.vfe_out.out_port_info = out_port;
		vfe_acquire.vfe_out.split_id = CAM_ISP_HW_SPLIT_LEFT;
		vfe_acquire.vfe_out.unique_id = ife_ctx->ctx_index;
		vfe_acquire.vfe_out.is_dual = 0;
		vfe_acquire.vfe_out.disable_ubwc_comp =
			g_ife_hw_mgr.debug_cfg.disable_ubwc_comp;
		vfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;
		vfe_acquire.buf_done_controller = ife_ctx->buf_done_controller;
		hw_intf = ife_src_res->hw_res[0]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&vfe_acquire,
			sizeof(struct cam_vfe_acquire_args));
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not acquire out resource 0x%x",
				 out_port->res_type);
			goto err;
		}
		break;
	}

	if (i == in_port->num_out_res) {
		CAM_ERR(CAM_ISP,
			"Cannot acquire out resource, i=%d, num_out_res=%d",
			i, in_port->num_out_res);
		goto err;
	}

	ife_out_res->hw_res[0] = vfe_acquire.vfe_out.rsrc_node;
	ife_out_res->is_dual_isp = 0;
	ife_out_res->res_id = vfe_out_res_id;
	ife_out_res->res_type = CAM_ISP_RESOURCE_VFE_OUT;
	ife_src_res->num_children++;
	ife_ctx->num_acq_vfe_out++;

	return 0;
err:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_out_pixel(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_hw_mgr_res           *ife_src_res,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -1;
	uint32_t  i, j, k;
	struct cam_vfe_acquire_args               vfe_acquire;
	struct cam_isp_out_port_generic_info     *out_port;
	struct cam_isp_hw_mgr_res                *ife_out_res;
	struct cam_hw_intf                       *hw_intf;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		/* Skip output ports for SFE */
		if (!cam_ife_hw_mgr_is_ife_out_port(out_port->res_type))
			continue;

		if (cam_ife_hw_mgr_is_rdi_res(out_port->res_type))
			continue;

		if (!cam_ife_hw_mgr_check_path_port_compat(ife_src_res->res_id,
			out_port->res_type))
			continue;

		CAM_DBG(CAM_ISP, "res_type 0x%x", out_port->res_type);

		k = out_port->res_type & 0xFF;
		ife_out_res = &ife_ctx->res_list_ife_out[k];
		ife_out_res->is_dual_isp = in_port->usage_type;

		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_OUT;
		vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		vfe_acquire.vfe_out.cdm_ops = ife_ctx->cdm_ops;
		vfe_acquire.priv = ife_ctx;
		vfe_acquire.vfe_out.out_port_info =  out_port;
		vfe_acquire.vfe_out.is_dual       = ife_src_res->is_dual_isp;
		vfe_acquire.vfe_out.unique_id     = ife_ctx->ctx_index;
		vfe_acquire.vfe_out.disable_ubwc_comp =
			g_ife_hw_mgr.debug_cfg.disable_ubwc_comp;
		vfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;
		vfe_acquire.buf_done_controller = ife_ctx->buf_done_controller;

		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!ife_src_res->hw_res[j])
				continue;

			hw_intf = ife_src_res->hw_res[j]->hw_intf;

			if (j == CAM_ISP_HW_SPLIT_LEFT) {
				vfe_acquire.vfe_out.split_id  =
					CAM_ISP_HW_SPLIT_LEFT;
				if (ife_src_res->is_dual_isp) {
					/*TBD */
					vfe_acquire.vfe_out.is_master     = 1;
					vfe_acquire.vfe_out.dual_slave_core =
						ife_ctx->right_hw_idx;
				} else {
					vfe_acquire.vfe_out.is_master   = 0;
					vfe_acquire.vfe_out.dual_slave_core =
						0;
				}
			} else {
				vfe_acquire.vfe_out.split_id  =
					CAM_ISP_HW_SPLIT_RIGHT;
				vfe_acquire.vfe_out.is_master       = 0;
				vfe_acquire.vfe_out.dual_slave_core =
					ife_ctx->left_hw_idx;
			}
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&vfe_acquire,
				sizeof(struct cam_vfe_acquire_args));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Can not acquire out resource 0x%x",
					out_port->res_type);
				goto err;
			}

			ife_out_res->hw_res[j] =
				vfe_acquire.vfe_out.rsrc_node;
			CAM_DBG(CAM_ISP, "resource type :0x%x res id:0x%x",
				ife_out_res->hw_res[j]->res_type,
				ife_out_res->hw_res[j]->res_id);

		}
		ife_out_res->res_type = CAM_ISP_RESOURCE_VFE_OUT;
		ife_out_res->res_id = out_port->res_type;
		ife_src_res->num_children++;
		ife_ctx->num_acq_vfe_out++;
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_sfe_out_rdi(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_hw_mgr_res           *sfe_src_res,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -EINVAL;
	uint32_t  i, sfe_out_res_id, sfe_in_res_id;
	struct cam_sfe_acquire_args               sfe_acquire;
	struct cam_isp_out_port_generic_info     *out_port = NULL;
	struct cam_isp_hw_mgr_res                *sfe_out_res;
	struct cam_hw_intf                       *hw_intf;

	/* take left resource */
	sfe_in_res_id = sfe_src_res->hw_res[0]->res_id;

	switch (sfe_in_res_id) {
	case CAM_ISP_HW_SFE_IN_RDI0:
		sfe_out_res_id = CAM_ISP_SFE_OUT_RES_RDI_0;
		break;
	case CAM_ISP_HW_SFE_IN_RDI1:
		sfe_out_res_id = CAM_ISP_SFE_OUT_RES_RDI_1;
		break;
	case CAM_ISP_HW_SFE_IN_RDI2:
		sfe_out_res_id = CAM_ISP_SFE_OUT_RES_RDI_2;
		break;
	case CAM_ISP_HW_SFE_IN_RDI3:
		sfe_out_res_id = CAM_ISP_SFE_OUT_RES_RDI_3;
		break;
	case CAM_ISP_HW_SFE_IN_RDI4:
		sfe_out_res_id = CAM_ISP_SFE_OUT_RES_RDI_4;
		break;
	default:
		CAM_ERR(CAM_ISP,
			"invalid SFE RDI resource type 0x%x",
			sfe_in_res_id);
		goto err;
	}

	CAM_DBG(CAM_ISP,
		"sfe_in_res_id: 0x%x sfe_out_res_id: 0x%x",
		sfe_in_res_id, sfe_out_res_id);

	sfe_acquire.rsrc_type = CAM_ISP_RESOURCE_SFE_OUT;
	sfe_acquire.tasklet = ife_ctx->common.tasklet_info;

	sfe_out_res = &ife_ctx->res_list_sfe_out[sfe_out_res_id & 0xFF];
	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		CAM_DBG(CAM_ISP,
			"i: %d sfe_out_res_id: 0x%x out_port: 0x%x",
			i, sfe_out_res_id, out_port->res_type);

		if (sfe_out_res_id != out_port->res_type)
			continue;

		sfe_acquire.sfe_out.cdm_ops = ife_ctx->cdm_ops;
		sfe_acquire.priv = ife_ctx;
		sfe_acquire.sfe_out.out_port_info = out_port;
		sfe_acquire.sfe_out.split_id = CAM_ISP_HW_SPLIT_LEFT;
		sfe_acquire.sfe_out.unique_id = ife_ctx->ctx_index;
		sfe_acquire.sfe_out.is_dual = 0;
		sfe_acquire.buf_done_controller = ife_ctx->buf_done_controller;
		sfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;
		hw_intf = sfe_src_res->hw_res[0]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&sfe_acquire,
			sizeof(struct cam_sfe_acquire_args));
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Can not acquire out resource: 0x%x",
				 out_port->res_type);
			goto err;
		}
		break;
	}

	if (i == in_port->num_out_res) {
		CAM_ERR(CAM_ISP,
			"Cannot acquire out resource i: %d, num_out_res: %u",
			i, in_port->num_out_res);
		goto err;
	}

	sfe_out_res->hw_res[0] = sfe_acquire.sfe_out.rsrc_node;
	sfe_out_res->is_dual_isp = 0;
	sfe_out_res->res_id = sfe_out_res_id;
	sfe_out_res->res_type = CAM_ISP_RESOURCE_SFE_OUT;
	sfe_src_res->num_children++;
	ife_ctx->num_acq_sfe_out++;
	return 0;

err:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_sfe_out_pix(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_hw_mgr_res           *sfe_src_res,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -1, k;
	uint32_t  i, j;
	struct cam_sfe_acquire_args               sfe_acquire;
	struct cam_isp_out_port_generic_info     *out_port;
	struct cam_isp_hw_mgr_res                *sfe_out_res;
	struct cam_hw_intf                       *hw_intf;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];

		/* Skip IFE ports */
		if (!cam_ife_hw_mgr_is_sfe_out_port(out_port->res_type))
			continue;

		if (cam_ife_hw_mgr_is_sfe_rdi_res(out_port->res_type))
			continue;

		k = out_port->res_type & 0xFF;
		CAM_DBG(CAM_ISP, "res_type: 0x%x", out_port->res_type);

		sfe_out_res = &ife_ctx->res_list_sfe_out[k];
		sfe_out_res->is_dual_isp = in_port->usage_type;

		sfe_acquire.rsrc_type = CAM_ISP_RESOURCE_SFE_OUT;
		sfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		sfe_acquire.sfe_out.cdm_ops = ife_ctx->cdm_ops;
		sfe_acquire.priv = ife_ctx;
		sfe_acquire.sfe_out.out_port_info =  out_port;
		sfe_acquire.sfe_out.is_dual       = sfe_src_res->is_dual_isp;
		sfe_acquire.sfe_out.unique_id     = ife_ctx->ctx_index;
		sfe_acquire.buf_done_controller = ife_ctx->buf_done_controller;
		sfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;

		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!sfe_src_res->hw_res[j])
				continue;

			hw_intf = sfe_src_res->hw_res[j]->hw_intf;

			if (j == CAM_ISP_HW_SPLIT_LEFT) {
				sfe_acquire.sfe_out.split_id  =
					CAM_ISP_HW_SPLIT_LEFT;
				if (sfe_src_res->is_dual_isp)
					sfe_acquire.sfe_out.is_master = 1;
				else
					sfe_acquire.sfe_out.is_master = 0;
			} else {
				sfe_acquire.sfe_out.split_id  =
					CAM_ISP_HW_SPLIT_RIGHT;
				sfe_acquire.sfe_out.is_master       = 0;
			}
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&sfe_acquire,
				sizeof(struct cam_sfe_acquire_args));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Can not acquire out resource 0x%x",
					out_port->res_type);
				goto err;
			}

			sfe_out_res->hw_res[j] =
				sfe_acquire.sfe_out.rsrc_node;
			CAM_DBG(CAM_ISP, "resource type: 0x%x res: %s res id: 0x%x",
				sfe_out_res->hw_res[j]->res_type,
				sfe_out_res->hw_res[j]->res_name,
				sfe_out_res->hw_res[j]->res_id);

		}
		sfe_out_res->res_type = CAM_ISP_RESOURCE_SFE_OUT;
		sfe_out_res->res_id = out_port->res_type;
		sfe_src_res->num_children++;
		ife_ctx->num_acq_sfe_out++;
	}

	return 0;
err:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_sfe_out(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -EINVAL;
	struct cam_isp_hw_mgr_res *sfe_res_iterator;

	if (list_empty(&ife_ctx->res_list_sfe_src)) {
		CAM_WARN(CAM_ISP, "SFE src list empty");
		return 0;
	}

	list_for_each_entry(sfe_res_iterator,
		&ife_ctx->res_list_sfe_src, list) {
		if (sfe_res_iterator->num_children)
			continue;

		switch (sfe_res_iterator->res_id) {
		case CAM_ISP_HW_SFE_IN_PIX:
			rc = cam_ife_hw_mgr_acquire_res_sfe_out_pix(ife_ctx,
				sfe_res_iterator, in_port);
			break;
		case CAM_ISP_HW_SFE_IN_RDI0:
		case CAM_ISP_HW_SFE_IN_RDI1:
		case CAM_ISP_HW_SFE_IN_RDI2:
			/* for sHDR acquire both RDI and PIX ports */
			rc = cam_ife_hw_mgr_acquire_res_sfe_out_rdi(ife_ctx,
				sfe_res_iterator, in_port);
			if (rc)
				goto err;

			rc = cam_ife_hw_mgr_acquire_res_sfe_out_pix(ife_ctx,
				sfe_res_iterator, in_port);
			break;
		case CAM_ISP_HW_SFE_IN_RDI3:
		case CAM_ISP_HW_SFE_IN_RDI4:
			rc = cam_ife_hw_mgr_acquire_res_sfe_out_rdi(ife_ctx,
				sfe_res_iterator, in_port);
			break;
		default:
			CAM_ERR(CAM_ISP, "Unknown SFE IN resource: %d",
				sfe_res_iterator->res_id);
			rc = -EINVAL;
			break;
		}
		if (rc)
			goto err;
	}

	return 0;
err:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_out(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -EINVAL;
	struct cam_isp_hw_mgr_res       *ife_src_res;

	if (list_empty(&ife_ctx->res_list_ife_src)) {
		CAM_WARN(CAM_ISP, "IFE src list empty");
		return 0;
	}

	list_for_each_entry(ife_src_res, &ife_ctx->res_list_ife_src, list) {
		if (ife_src_res->num_children)
			continue;

		switch (ife_src_res->res_id) {
		case CAM_ISP_HW_VFE_IN_CAMIF:
		case CAM_ISP_HW_VFE_IN_PDLIB:
		case CAM_ISP_HW_VFE_IN_RD:
		case CAM_ISP_HW_VFE_IN_LCR:
			rc = cam_ife_hw_mgr_acquire_res_ife_out_pixel(ife_ctx,
				ife_src_res, in_port);
			break;
		case CAM_ISP_HW_VFE_IN_RDI0:
		case CAM_ISP_HW_VFE_IN_RDI1:
		case CAM_ISP_HW_VFE_IN_RDI2:
		case CAM_ISP_HW_VFE_IN_RDI3:
			rc = cam_ife_hw_mgr_acquire_res_ife_out_rdi(ife_ctx,
				ife_src_res, in_port);
			break;
		default:
			CAM_ERR(CAM_ISP, "Unknown IFE SRC resource: %d",
				ife_src_res->res_id);
			break;
		}
		if (rc)
			goto err;
	}

	return 0;
err:
	/* release resource on entry function */
	return rc;
}

static inline void cam_ife_mgr_count_ife(void)
{
	int i;

	g_num_ife = 0;
	g_num_ife_lite = 0;

	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (g_ife_hw_mgr.ife_devices[i]) {
			if (g_ife_hw_mgr.ife_dev_caps[i].is_lite)
				g_num_ife_lite++;
			else
				g_num_ife++;
		}
	}
	CAM_DBG(CAM_ISP, "counted %d IFE and %d IFE lite", g_num_ife, g_num_ife_lite);
}

static int cam_convert_hw_idx_to_ife_hw_num(int hw_idx)
{
	if (hw_idx < g_num_ife) {
		switch (hw_idx) {
		case 0: return CAM_ISP_IFE0_HW;
		case 1: return CAM_ISP_IFE1_HW;
		case 2: return CAM_ISP_IFE2_HW;
		}
	} else if (hw_idx < g_num_ife + g_num_ife_lite) {
		switch (hw_idx - g_num_ife) {
		case 0: return CAM_ISP_IFE0_LITE_HW;
		case 1: return CAM_ISP_IFE1_LITE_HW;
		case 2: return CAM_ISP_IFE2_LITE_HW;
		case 3: return CAM_ISP_IFE3_LITE_HW;
		case 4: return CAM_ISP_IFE4_LITE_HW;
		}
	} else {
		CAM_ERR(CAM_ISP, "hw idx %d out-of-bounds", hw_idx);
	}
	return 0;
}

static int cam_convert_rdi_out_res_id_to_src(int res_id)
{
	if (res_id == CAM_ISP_IFE_OUT_RES_RDI_0)
		return CAM_ISP_HW_VFE_IN_RDI0;
	else if (res_id == CAM_ISP_IFE_OUT_RES_RDI_1)
		return CAM_ISP_HW_VFE_IN_RDI1;
	else if (res_id == CAM_ISP_IFE_OUT_RES_RDI_2)
		return CAM_ISP_HW_VFE_IN_RDI2;
	else if (res_id == CAM_ISP_IFE_OUT_RES_RDI_3)
		return CAM_ISP_HW_VFE_IN_RDI3;
	return CAM_ISP_HW_VFE_IN_MAX;
}

static int cam_convert_csid_res_to_path(int res_id)
{
	if (res_id == CAM_IFE_PIX_PATH_RES_IPP)
		return CAM_ISP_PXL_PATH;
	else if (res_id == CAM_IFE_PIX_PATH_RES_PPP)
		return CAM_ISP_PPP_PATH;
	else if (res_id == CAM_IFE_PIX_PATH_RES_RDI_0)
		return CAM_ISP_RDI0_PATH;
	else if (res_id == CAM_IFE_PIX_PATH_RES_RDI_1)
		return CAM_ISP_RDI1_PATH;
	else if (res_id == CAM_IFE_PIX_PATH_RES_RDI_2)
		return CAM_ISP_RDI2_PATH;
	else if (res_id == CAM_IFE_PIX_PATH_RES_RDI_3)
		return CAM_ISP_RDI3_PATH;
	return 0;
}

static int cam_convert_res_id_to_hw_path(int res_id)
{
	if (res_id == CAM_ISP_HW_VFE_IN_LCR)
		return CAM_ISP_LCR_PATH;
	else if (res_id == CAM_ISP_HW_VFE_IN_PDLIB)
		return CAM_ISP_PPP_PATH;
	else if (res_id == CAM_ISP_HW_VFE_IN_CAMIF)
		return CAM_ISP_PXL_PATH;
	else if (res_id == CAM_ISP_HW_VFE_IN_RDI0)
		return CAM_ISP_RDI0_PATH;
	else if (res_id == CAM_ISP_HW_VFE_IN_RDI1)
		return CAM_ISP_RDI1_PATH;
	else if (res_id == CAM_ISP_HW_VFE_IN_RDI2)
		return CAM_ISP_RDI2_PATH;
	else if (res_id == CAM_ISP_HW_VFE_IN_RDI3)
		return CAM_ISP_RDI3_PATH;
	return 0;
}

static int cam_ife_hw_mgr_acquire_sfe_hw(
	bool                                use_lower_idx,
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_sfe_acquire_args        *sfe_acquire)
{
	int i, rc = -EINVAL;
	struct cam_hw_intf    *hw_intf = NULL;
	struct cam_ife_hw_mgr *ife_hw_mgr = ife_ctx->hw_mgr;

	/* Use lower index for RDIs in case of dual */
	if ((ife_ctx->flags.is_fe_enabled) || (use_lower_idx)) {
		for (i = 0; i < CAM_SFE_HW_NUM_MAX; i++) {
			if (!ife_hw_mgr->sfe_devices[i])
				continue;

			hw_intf = ife_hw_mgr->sfe_devices[i]->hw_intf;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
					sfe_acquire,
					sizeof(struct cam_sfe_acquire_args));
			if (rc) {
				CAM_DBG(CAM_ISP,
					"Can not acquire SFE HW: %d for res: %d",
					i, sfe_acquire->sfe_in.res_id);
				continue;
			} else {
				break;
			}
		}
	} else {
		for (i = CAM_SFE_HW_NUM_MAX - 1; i >= 0; i--) {
			if (!ife_hw_mgr->sfe_devices[i])
				continue;

			hw_intf = ife_hw_mgr->sfe_devices[i]->hw_intf;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
					sfe_acquire,
					sizeof(struct cam_sfe_acquire_args));
			if (rc) {
				CAM_DBG(CAM_ISP,
					"Can not acquire SFE HW: %d for res: %d",
					i, sfe_acquire->sfe_in.res_id);
				continue;
			} else {
				break;
			}
		}
	}

	return rc;
}

static int cam_ife_hw_mgr_acquire_res_sfe_src(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -1, i;
	bool is_rdi = false;
	struct cam_sfe_acquire_args          sfe_acquire;
	struct cam_isp_hw_mgr_res           *csid_res;
	struct cam_isp_hw_mgr_res           *sfe_src_res;

	list_for_each_entry(csid_res, &ife_ctx->res_list_ife_csid, list) {
		if (csid_res->num_children)
			continue;

		if (csid_res->res_id == CAM_IFE_PIX_PATH_RES_PPP)
			continue;

		rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&sfe_src_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto err;
		}

		cam_ife_hw_mgr_put_res(&ife_ctx->res_list_sfe_src,
			&sfe_src_res);

		is_rdi = false;
		sfe_acquire.rsrc_type = CAM_ISP_RESOURCE_SFE_IN;
		sfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		sfe_acquire.sfe_in.cdm_ops = ife_ctx->cdm_ops;
		sfe_acquire.sfe_in.in_port = in_port;
		sfe_acquire.sfe_in.is_offline = ife_ctx->flags.is_offline;
		sfe_acquire.priv = ife_ctx;
		sfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;

		switch (csid_res->res_id) {
		case CAM_IFE_PIX_PATH_RES_IPP:
			sfe_acquire.sfe_in.res_id =
					CAM_ISP_HW_SFE_IN_PIX;
			sfe_acquire.sfe_in.is_dual =
				csid_res->is_dual_isp;
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_0:
			sfe_acquire.sfe_in.res_id = CAM_ISP_HW_SFE_IN_RDI0;
			is_rdi = true;
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_1:
			sfe_acquire.sfe_in.res_id = CAM_ISP_HW_SFE_IN_RDI1;
			is_rdi = true;
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_2:
			sfe_acquire.sfe_in.res_id = CAM_ISP_HW_SFE_IN_RDI2;
			is_rdi = true;
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_3:
			sfe_acquire.sfe_in.res_id = CAM_ISP_HW_SFE_IN_RDI3;
			is_rdi = true;
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_4:
			sfe_acquire.sfe_in.res_id = CAM_ISP_HW_SFE_IN_RDI4;
			is_rdi = true;
			break;
		default:
			CAM_ERR(CAM_ISP,
				"Wrong CSID Path Resource ID: %u",
				csid_res->res_id);
			goto err;
		}

		sfe_src_res->res_type = sfe_acquire.rsrc_type;
		sfe_src_res->res_id = sfe_acquire.sfe_in.res_id;
		sfe_src_res->is_dual_isp = csid_res->is_dual_isp;
		for (i = sfe_src_res->is_dual_isp; i >= 0; i--) {
			rc = cam_ife_hw_mgr_acquire_sfe_hw(
				((is_rdi) && (!sfe_src_res->is_dual_isp) &&
				(ife_ctx->flags.is_dual)),
				ife_ctx, &sfe_acquire);

			if (rc || !sfe_acquire.sfe_in.rsrc_node) {
				CAM_ERR(CAM_ISP,
					"Failed to acquire split_id: %d SFE for res_type: %u id: %u",
					i, sfe_src_res->res_type, sfe_src_res->res_id);
				goto err;
			}

			sfe_src_res->hw_res[i] =
				sfe_acquire.sfe_in.rsrc_node;
			CAM_DBG(CAM_ISP,
				"acquire success %s SFE: %u res_name: %s res_type: %u res_id: %u",
				((i == CAM_ISP_HW_SPLIT_LEFT) ? "LEFT" : "RIGHT"),
				sfe_src_res->hw_res[i]->hw_intf->hw_idx,
				sfe_src_res->hw_res[i]->res_name,
				sfe_src_res->hw_res[i]->res_type,
				sfe_src_res->hw_res[i]->res_id);
		}
		csid_res->num_children++;
	}

	return 0;

err:
	return rc;
}

static bool cam_ife_mgr_check_can_use_lite(
	struct cam_csid_hw_reserve_resource_args  *csid_acquire,
	struct cam_ife_hw_mgr_ctx                 *ife_ctx)
{
	bool can_use_lite = false;

	if (ife_ctx->flags.is_rdi_only_context ||
		csid_acquire->in_port->can_use_lite) {
		can_use_lite = true;
		goto end;
	}

	switch (csid_acquire->res_id) {

	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
		can_use_lite = true;
		break;
	default:
		can_use_lite = false;
		goto end;
	}

	CAM_DBG(CAM_ISP,
		"in_port lite hint %d, rdi_only: %d can_use_lite: %d res_id: %u",
		csid_acquire->in_port->can_use_lite,
		ife_ctx->flags.is_rdi_only_context,
		can_use_lite, csid_acquire->res_id);

end:
	return can_use_lite;
}

static int cam_ife_hw_mgr_acquire_res_ife_bus_rd(
	struct cam_ife_hw_mgr_ctx                  *ife_ctx,
	struct cam_isp_in_port_generic_info        *in_port)
{
	int                                         rc = -EINVAL, j;
	int                                         i = CAM_ISP_HW_SPLIT_LEFT;
	struct cam_vfe_acquire_args                 vfe_acquire;
	struct cam_isp_hw_mgr_res                  *ife_bus_rd_res;
	struct cam_hw_intf                         *hw_intf;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &ife_bus_rd_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto end;
	}

	vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_BUS_RD;
	vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
	vfe_acquire.priv = ife_ctx;
	vfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;

	vfe_acquire.vfe_bus_rd.cdm_ops = ife_ctx->cdm_ops;
	vfe_acquire.vfe_bus_rd.is_dual = (uint32_t)ife_ctx->flags.is_dual;
	vfe_acquire.vfe_bus_rd.is_offline = ife_ctx->flags.is_offline;
	vfe_acquire.vfe_bus_rd.res_id = CAM_ISP_HW_VFE_IN_RD;
	vfe_acquire.vfe_bus_rd.unpacker_fmt = in_port->fe_unpacker_fmt;

	for (j = 0; j < CAM_IFE_HW_NUM_MAX; j++) {
		if (!ife_hw_mgr->ife_devices[j])
			continue;

		hw_intf = ife_hw_mgr->ife_devices[j]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&vfe_acquire, sizeof(struct cam_vfe_acquire_args));

		if (!rc) {
			ife_bus_rd_res->hw_res[i] =
				vfe_acquire.vfe_bus_rd.rsrc_node;

			CAM_DBG(CAM_ISP, "Acquired VFE:%d BUS RD for LEFT", j);
			break;
		}
	}

	if (j == CAM_IFE_HW_NUM_MAX || !vfe_acquire.vfe_bus_rd.rsrc_node) {
		CAM_ERR(CAM_ISP, "Failed to acquire BUS RD for LEFT", i);
		goto put_res;
	}

	ife_bus_rd_res->res_type = vfe_acquire.rsrc_type;
	ife_bus_rd_res->res_id = vfe_acquire.vfe_in.res_id;
	ife_bus_rd_res->is_dual_isp = (uint32_t)ife_ctx->flags.is_dual;
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_in_rd, &ife_bus_rd_res);

	if (ife_ctx->flags.is_dual) {
		for (j = 0; j < CAM_IFE_HW_NUM_MAX; j++) {
			if (!ife_hw_mgr->ife_devices[j])
				continue;

			if (j == ife_bus_rd_res->hw_res[i]->hw_intf->hw_idx)
				continue;

			hw_intf = ife_hw_mgr->ife_devices[j]->hw_intf;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&vfe_acquire,
				sizeof(struct cam_vfe_acquire_args));

			if (!rc) {
				ife_bus_rd_res->hw_res[++i] =
					vfe_acquire.vfe_bus_rd.rsrc_node;

				CAM_DBG(CAM_ISP,
					"Acquired VFE:%d BUS RD for RIGHT", j);
				break;
			}
		}

		if (j == CAM_IFE_HW_NUM_MAX ||
			!vfe_acquire.vfe_bus_rd.rsrc_node) {
			CAM_ERR(CAM_ISP, "Failed to acquire BUS RD for RIGHT");
			goto end;
		}
	}

	return 0;

put_res:
	cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &ife_bus_rd_res);
end:
	return rc;
}

static int cam_ife_hw_mgr_acquire_sfe_bus_rd(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -1;
	int i, path_res_id;
	uint32_t acquired_cnt = CAM_ISP_HW_SPLIT_LEFT;
	struct cam_sfe_acquire_args           sfe_acquire;
	struct cam_ife_hw_mgr                *ife_hw_mgr;
	struct cam_hw_intf                   *hw_intf;
	struct cam_isp_hw_mgr_res            *sfe_rd_res, *sfe_res_iterator;

	ife_hw_mgr = ife_ctx->hw_mgr;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &sfe_rd_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto err;
	}

	path_res_id = cam_ife_hw_mgr_get_sfe_rd_res_id(
		in_port->sfe_in_path_type);
	if (path_res_id == CAM_ISP_HW_SFE_IN_MAX) {
		CAM_ERR(CAM_ISP, "Invalid sfe rd path type: %u",
			in_port->sfe_in_path_type);
		rc = -EINVAL;
		goto put_res;
	}

	if (in_port->usage_type)
		CAM_WARN(CAM_ISP,
			"DUAL mode not supported for BUS RD [RDIs]");

	sfe_acquire.rsrc_type = CAM_ISP_RESOURCE_SFE_RD;
	sfe_acquire.tasklet = ife_ctx->common.tasklet_info;
	sfe_acquire.priv = ife_ctx;
	sfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;
	sfe_acquire.sfe_rd.cdm_ops = ife_ctx->cdm_ops;
	sfe_acquire.sfe_rd.is_offline = ife_ctx->flags.is_offline;
	sfe_acquire.sfe_rd.unpacker_fmt = in_port->fe_unpacker_fmt;
	sfe_acquire.sfe_rd.res_id = path_res_id;
	sfe_acquire.sfe_rd.secure_mode = in_port->secure_mode;

	list_for_each_entry(sfe_res_iterator, &ife_ctx->res_list_ife_in_rd,
		list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!sfe_res_iterator->hw_res[i])
				continue;

			/* Check for secure */
			if (sfe_res_iterator->is_secure != in_port->secure_mode)
				continue;

			hw_intf = sfe_res_iterator->hw_res[i]->hw_intf;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&sfe_acquire, sizeof(sfe_acquire));
			if (rc) {
				CAM_DBG(CAM_ISP,
					"No SFE RD rsrc: %u from hw: %u",
					path_res_id,
					hw_intf->hw_idx);
				continue;
			}

		sfe_rd_res->hw_res[acquired_cnt++] =
				sfe_acquire.sfe_rd.rsrc_node;

			CAM_DBG(CAM_ISP,
				"acquired from old SFE(%s): %u path: %u successfully",
				(i == 0) ? "left" : "right",
				hw_intf->hw_idx, path_res_id);

			/* With SFE the below condition should never be met */
			if ((in_port->usage_type) && (acquired_cnt == 1))
				continue;

			if (acquired_cnt)
				goto acquire_successful;
		}
	}

	/*
	 * SFEx can be connected to any CSID/IFE in
	 * single/dual use cases hence always iterating
	 * from 0 to HW max. IFE also will reserve from lower idx
	 * as well since SFE works with FULL IFEs only
	 */
	for (i = 0; i < CAM_SFE_HW_NUM_MAX; i++) {
		if (!ife_hw_mgr->sfe_devices[i])
			continue;

		hw_intf = ife_hw_mgr->sfe_devices[i]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&sfe_acquire, sizeof(struct cam_sfe_acquire_args));
		if (rc)
			continue;
		else
			break;
	}

	if (!sfe_acquire.sfe_rd.rsrc_node || rc) {
		CAM_ERR(CAM_ISP,
			"Failed to acquire SFE RD for path: %u",
			path_res_id);
		goto put_res;
	}

	sfe_rd_res->hw_res[acquired_cnt++] = sfe_acquire.sfe_rd.rsrc_node;

acquire_successful:
	CAM_DBG(CAM_ISP,
		"SFE RD left [%u] acquired success for path: %u is_dual: %d res: %s res_id: 0x%x",
		sfe_rd_res->hw_res[0]->hw_intf->hw_idx, path_res_id,
		in_port->usage_type, sfe_rd_res->hw_res[0]->res_name,
		sfe_rd_res->hw_res[0]->res_id);

	sfe_rd_res->res_id = in_port->sfe_in_path_type;
	sfe_rd_res->res_type = sfe_acquire.rsrc_type;
	sfe_rd_res->is_dual_isp = in_port->usage_type;
	sfe_rd_res->is_secure = in_port->secure_mode;
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_in_rd, &sfe_rd_res);

	/* DUAL SFE for Fetch engine - unused for now */
	if (in_port->usage_type && acquired_cnt == 1) {
		sfe_acquire.sfe_rd.rsrc_node = NULL;
		for (i = 0; i < CAM_SFE_HW_NUM_MAX; i++) {
			if (!ife_hw_mgr->sfe_devices[i])
				continue;

			if (i == sfe_rd_res->hw_res[0]->hw_intf->hw_idx)
				continue;

			hw_intf = ife_hw_mgr->sfe_devices[i]->hw_intf;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&sfe_acquire, sizeof(sfe_acquire));
			if (rc)
				continue;
			else
				break;
		}

		if (!sfe_acquire.sfe_rd.rsrc_node || rc) {
			CAM_ERR(CAM_ISP,
				"Can not acquire SFE RD right resource");
			goto err;
		}

		sfe_rd_res->hw_res[1] = sfe_acquire.sfe_rd.rsrc_node;
		CAM_DBG(CAM_ISP,
			"SFE right [%u] acquire success for path: %u",
			sfe_rd_res->hw_res[1]->hw_intf->hw_idx, path_res_id);
	}

	return 0;
put_res:
	cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &sfe_rd_res);
err:
	return rc;
}

static int cam_ife_hw_mgr_acquire_ife_src_for_sfe(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	bool acquire_lcr, uint32_t *acquired_hw_id,
	uint32_t *acquired_hw_path)
{
	int rc = -1, i;
	struct cam_vfe_acquire_args                 vfe_acquire;
	struct cam_isp_hw_mgr_res                  *ife_src_res;
	struct cam_hw_intf                         *hw_intf = NULL;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;
	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
		&ife_src_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto end;
	}

	vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_IN;
	vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
	vfe_acquire.vfe_in.cdm_ops = ife_ctx->cdm_ops;
	vfe_acquire.vfe_in.in_port = in_port;
	vfe_acquire.vfe_in.is_fe_enabled = ife_ctx->flags.is_fe_enabled;
	vfe_acquire.vfe_in.is_offline = ife_ctx->flags.is_offline;
	vfe_acquire.priv = ife_ctx;
	vfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;
	if (!acquire_lcr)
		vfe_acquire.vfe_in.res_id =
			CAM_ISP_HW_VFE_IN_CAMIF;
	else
		vfe_acquire.vfe_in.res_id =
			CAM_ISP_HW_VFE_IN_LCR;

	if (ife_ctx->flags.is_dual) {
		vfe_acquire.vfe_in.sync_mode =
			CAM_ISP_HW_SYNC_MASTER;
		vfe_acquire.vfe_in.dual_hw_idx =
			ife_ctx->right_hw_idx;
	} else
		vfe_acquire.vfe_in.sync_mode =
			CAM_ISP_HW_SYNC_NONE;

	vfe_acquire.vfe_in.is_dual =
		(uint32_t)ife_ctx->flags.is_dual;


	ife_src_res->res_type = vfe_acquire.rsrc_type;
	ife_src_res->res_id = vfe_acquire.vfe_in.res_id;
	ife_src_res->is_dual_isp = (uint32_t)ife_ctx->flags.is_dual;
	for (i =  0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (!ife_hw_mgr->ife_devices[i])
			continue;

		if (i != ife_ctx->left_hw_idx)
			continue;

		hw_intf = ife_hw_mgr->ife_devices[i]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&vfe_acquire,
			sizeof(struct cam_vfe_acquire_args));
		if (rc)
			continue;
		else
			break;
	}

	if (i == CAM_IFE_HW_NUM_MAX || rc ||
			!vfe_acquire.vfe_in.rsrc_node) {
		CAM_ERR(CAM_ISP, "Unable to acquire LEFT IFE res: %d",
			vfe_acquire.vfe_in.res_id);
		return -EAGAIN;
	}

	ife_src_res->hw_res[0] = vfe_acquire.vfe_in.rsrc_node;
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_src,
		&ife_src_res);
	*acquired_hw_id |=
		cam_convert_hw_idx_to_ife_hw_num(hw_intf->hw_idx);
	acquired_hw_path[0] |= cam_convert_res_id_to_hw_path(
				ife_src_res->hw_res[0]->res_id);
	CAM_DBG(CAM_ISP,
		"acquire success LEFT IFE: %d res type: 0x%x res: %s res id: 0x%x",
		hw_intf->hw_idx,
		ife_src_res->hw_res[0]->res_type,
		ife_src_res->hw_res[0]->res_name,
		ife_src_res->hw_res[0]->res_id);

	if (ife_ctx->flags.is_dual) {
		vfe_acquire.vfe_in.rsrc_node = NULL;
		vfe_acquire.vfe_in.sync_mode =
			CAM_ISP_HW_SYNC_SLAVE;
		vfe_acquire.vfe_in.dual_hw_idx =
			ife_ctx->left_hw_idx;
		for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
			if (!ife_hw_mgr->ife_devices[i])
				continue;

			if (i == ife_src_res->hw_res[0]->hw_intf->hw_idx)
				continue;

			if (i != ife_ctx->right_hw_idx)
				continue;

			hw_intf = ife_hw_mgr->ife_devices[i]->hw_intf;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&vfe_acquire,
				sizeof(struct cam_vfe_acquire_args));
			if (rc)
				continue;
			else
				break;
		}

		if (i == CAM_IFE_HW_NUM_MAX || rc ||
				!vfe_acquire.vfe_in.rsrc_node) {
			CAM_ERR(CAM_ISP, "Unable to acquire right IFE res: %u",
				vfe_acquire.vfe_in.res_id);
			rc = -EAGAIN;
			goto end;
		}

		ife_src_res->hw_res[1] = vfe_acquire.vfe_in.rsrc_node;
		*acquired_hw_id |=
			cam_convert_hw_idx_to_ife_hw_num(hw_intf->hw_idx);
		acquired_hw_path[1] |= cam_convert_res_id_to_hw_path(
				ife_src_res->hw_res[0]->res_id);
		CAM_DBG(CAM_ISP,
			"acquire success RIGHT IFE: %u res type: 0x%x res: %s res id: 0x%x",
			hw_intf->hw_idx,
			ife_src_res->hw_res[1]->res_type,
			ife_src_res->hw_res[1]->res_name,
			ife_src_res->hw_res[1]->res_id);
	}

	return 0;

end:
	/* release resource at the entry function */
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_src(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	bool acquire_lcr, bool acquire_ppp,
	uint32_t *acquired_hw_id,
	uint32_t *acquired_hw_path)
{
	int rc                = -1;
	int i;
	struct cam_isp_hw_mgr_res                  *csid_res;
	struct cam_isp_hw_mgr_res                  *ife_src_res;
	struct cam_vfe_acquire_args                 vfe_acquire;
	struct cam_hw_intf                         *hw_intf;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;

	list_for_each_entry(csid_res, &ife_ctx->res_list_ife_csid, list) {
		if (csid_res->num_children && !acquire_lcr)
			continue;

		if (acquire_lcr && csid_res->res_id != CAM_IFE_PIX_PATH_RES_IPP)
			continue;

		if (csid_res->res_id == CAM_IFE_PIX_PATH_RES_PPP && !acquire_ppp)
			continue;

		rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&ife_src_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto err;
		}
		cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_src,
			&ife_src_res);

		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_IN;
		vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		vfe_acquire.vfe_in.cdm_ops = ife_ctx->cdm_ops;
		vfe_acquire.vfe_in.in_port = in_port;
		vfe_acquire.vfe_in.is_fe_enabled = ife_ctx->flags.is_fe_enabled;
		vfe_acquire.vfe_in.is_offline = ife_ctx->flags.is_offline;
		vfe_acquire.priv = ife_ctx;
		vfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;

		switch (csid_res->res_id) {
		case CAM_IFE_PIX_PATH_RES_IPP:
			if (!acquire_lcr)
				vfe_acquire.vfe_in.res_id =
					CAM_ISP_HW_VFE_IN_CAMIF;
			else
				vfe_acquire.vfe_in.res_id =
					CAM_ISP_HW_VFE_IN_LCR;
			if (csid_res->is_dual_isp)
				vfe_acquire.vfe_in.sync_mode =
				CAM_ISP_HW_SYNC_MASTER;
			else
				vfe_acquire.vfe_in.sync_mode =
				CAM_ISP_HW_SYNC_NONE;
			vfe_acquire.vfe_in.is_dual = csid_res->is_dual_isp;

			break;
		case CAM_IFE_PIX_PATH_RES_PPP:
			vfe_acquire.vfe_in.res_id =
				CAM_ISP_HW_VFE_IN_PDLIB;
			vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;

			break;
		case CAM_IFE_PIX_PATH_RES_RDI_0:
			vfe_acquire.vfe_in.res_id = CAM_ISP_HW_VFE_IN_RDI0;
			vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_1:
			vfe_acquire.vfe_in.res_id = CAM_ISP_HW_VFE_IN_RDI1;
			vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_2:
			vfe_acquire.vfe_in.res_id = CAM_ISP_HW_VFE_IN_RDI2;
			vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;
			break;
		case CAM_IFE_PIX_PATH_RES_RDI_3:
			vfe_acquire.vfe_in.res_id = CAM_ISP_HW_VFE_IN_RDI3;
			vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;
			break;
		default:
			CAM_ERR(CAM_ISP, "Wrong IFE CSID Path Resource ID : %d",
				csid_res->res_id);
			goto err;
		}
		ife_src_res->res_type = vfe_acquire.rsrc_type;
		ife_src_res->res_id = vfe_acquire.vfe_in.res_id;
		ife_src_res->is_dual_isp = csid_res->is_dual_isp;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!csid_res->hw_res[i])
				continue;

			hw_intf = ife_hw_mgr->ife_devices[
				csid_res->hw_res[i]->hw_intf->hw_idx]->hw_intf;

			if (i == CAM_ISP_HW_SPLIT_LEFT &&
				ife_src_res->is_dual_isp) {
				vfe_acquire.vfe_in.dual_hw_idx =
					ife_ctx->right_hw_idx;
			}

			/* fill in more acquire information as needed */
			/* slave Camif resource, */
			if (i == CAM_ISP_HW_SPLIT_RIGHT &&
				ife_src_res->is_dual_isp) {
				vfe_acquire.vfe_in.sync_mode =
				CAM_ISP_HW_SYNC_SLAVE;
				vfe_acquire.vfe_in.dual_hw_idx =
					ife_ctx->left_hw_idx;
			}

			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
					&vfe_acquire,
					sizeof(struct cam_vfe_acquire_args));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Can not acquire IFE HW res %d",
					csid_res->res_id);
				goto err;
			}
			ife_src_res->hw_res[i] = vfe_acquire.vfe_in.rsrc_node;

			*acquired_hw_id |=
				cam_convert_hw_idx_to_ife_hw_num(
				hw_intf->hw_idx);

			if (i >= CAM_MAX_HW_SPLIT) {
				CAM_ERR(CAM_ISP, "HW split is invalid: %d", i);
				return -EINVAL;
			}

			acquired_hw_path[i] |= cam_convert_res_id_to_hw_path(
				ife_src_res->hw_res[i]->res_id);

			CAM_DBG(CAM_ISP,
				"acquire success IFE:%d res type :0x%x res: %s res id:0x%x",
				hw_intf->hw_idx,
				ife_src_res->hw_res[i]->res_type,
				ife_src_res->hw_res[i]->res_name,
				ife_src_res->hw_res[i]->res_id);

		}
		csid_res->num_children++;
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_ife_hw_mgr_acquire_csid_hw(
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_csid_hw_reserve_resource_args  *csid_acquire,
	struct cam_isp_in_port_generic_info *in_port)
{
	int i;
	int rc = -EINVAL;
	struct cam_hw_intf  *hw_intf;
	struct cam_ife_hw_mgr *ife_hw_mgr;
	bool is_start_lower_idx = false;
	struct cam_isp_hw_mgr_res *csid_res_iterator;
	struct cam_isp_out_port_generic_info *out_port = NULL;
	struct cam_ife_csid_hw_caps *csid_caps = NULL;
	bool can_use_lite = false;
	int busy_count = 0, compat_count = 0;

	if (!ife_ctx || !csid_acquire) {
		CAM_ERR(CAM_ISP,
			"Invalid args ife hw ctx %pK csid_acquire %pK",
			ife_ctx, csid_acquire);
		return -EINVAL;
	}

	if (ife_ctx->flags.is_fe_enabled || ife_ctx->flags.dsp_enabled)
		is_start_lower_idx =  true;

	ife_hw_mgr = ife_ctx->hw_mgr;

	if (ife_ctx->flags.is_rdi_only_context)
		csid_acquire->can_use_lite = true;

	if (in_port->num_out_res)
		out_port = &(in_port->data[0]);
	ife_ctx->flags.is_dual = (bool)in_port->usage_type;
	can_use_lite = cam_ife_mgr_check_can_use_lite(
			csid_acquire, ife_ctx);

	/* Try acquiring CSID from previously acquired HW */
	list_for_each_entry(csid_res_iterator, &ife_ctx->res_list_ife_csid,
		list) {

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!csid_res_iterator->hw_res[i])
				continue;
			if (in_port->num_out_res &&
				((csid_res_iterator->is_secure == 1 &&
				out_port->secure_mode == 0) ||
				(csid_res_iterator->is_secure == 0 &&
				out_port->secure_mode == 1)))
				continue;
			if (!in_port->num_out_res &&
				csid_res_iterator->is_secure == 1)
				continue;

			hw_intf = csid_res_iterator->hw_res[i]->hw_intf;
			csid_caps =
				&ife_hw_mgr->csid_hw_caps[hw_intf->hw_idx];

			if (csid_caps->is_lite && !can_use_lite) {
				CAM_DBG(CAM_ISP, "CSID[%u] cannot use lite",
					hw_intf->hw_idx);
				continue;
			}

			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				csid_acquire, sizeof(*csid_acquire));
			if (rc) {
				CAM_DBG(CAM_ISP,
					"No ife resource from hw %d",
					hw_intf->hw_idx);
				continue;
			}
			CAM_DBG(CAM_ISP,
				"acquired from old csid(%s)=%d successfully",
				(i == 0) ? "left" : "right",
				hw_intf->hw_idx);
			goto acquire_successful;
		}
	}

	for (i = (is_start_lower_idx) ? 0 : (CAM_IFE_CSID_HW_NUM_MAX - 1);
		(is_start_lower_idx) ? (i < CAM_IFE_CSID_HW_NUM_MAX) : (i >= 0);
		(is_start_lower_idx) ? i++ : i--) {
		if (!ife_hw_mgr->csid_devices[i])
			continue;
		hw_intf = ife_hw_mgr->csid_devices[i];

		if (ife_hw_mgr->csid_hw_caps[hw_intf->hw_idx].is_lite &&
			!can_use_lite) {
			CAM_DBG(CAM_ISP, "CSID[%u] cannot use lite",
				hw_intf->hw_idx);
			continue;
		}

		compat_count++;

		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			csid_acquire,
			sizeof(struct
				cam_csid_hw_reserve_resource_args));
		if (!rc)
			return rc;

		if (rc == -EBUSY)
			busy_count++;
		else
			CAM_ERR(CAM_ISP, "CSID[%d] acquire failed (rc=%d)", i, rc);
	}

	if (compat_count == busy_count)
		CAM_ERR(CAM_ISP, "all compatible CSIDs are busy");

acquire_successful:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_csid_pxl(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	bool                                 is_ipp,
	bool                                 crop_enable)
{
	int rc = -1;
	int i;
	struct cam_isp_out_port_generic_info     *out_port = NULL;
	struct cam_ife_hw_mgr                    *ife_hw_mgr;
	struct cam_isp_hw_mgr_res                *csid_res;
	struct cam_hw_intf                       *hw_intf;
	struct cam_csid_hw_reserve_resource_args  csid_acquire = {0};
	enum cam_ife_pix_path_res_id              path_res_id;
	struct cam_ife_csid_dual_sync_args        dual_sync_args = {0};

	ife_hw_mgr = ife_ctx->hw_mgr;

	if (is_ipp)
		path_res_id = CAM_IFE_PIX_PATH_RES_IPP;
	else
		path_res_id = CAM_IFE_PIX_PATH_RES_PPP;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &csid_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto end;
	}

	csid_res->res_type = CAM_ISP_RESOURCE_PIX_PATH;

	csid_res->res_id = path_res_id;
	ife_ctx->flags.is_dual = (bool)in_port->usage_type;

	if (in_port->usage_type && is_ipp)
		csid_res->is_dual_isp = 1;
	else {
		csid_res->is_dual_isp = 0;
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;
	}

	if (in_port->num_out_res) {
		out_port = &(in_port->data[0]);
		csid_res->is_secure = out_port->secure_mode;
	}

	CAM_DBG(CAM_ISP, "CSID Acq: E");

	/* for dual ife, acquire the right ife first */
	for (i = csid_res->is_dual_isp; i >= 0 ; i--) {
		CAM_DBG(CAM_ISP, "i %d is_dual %d", i, csid_res->is_dual_isp);

		csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_acquire.res_id = path_res_id;
		csid_acquire.in_port = in_port;
		csid_acquire.out_port = in_port->data;
		csid_acquire.node_res = NULL;
		csid_acquire.event_cb = cam_ife_hw_mgr_event_handler;
		csid_acquire.cb_priv = ife_ctx;
		csid_acquire.crop_enable = crop_enable;
		csid_acquire.drop_enable = false;

		if (csid_res->is_dual_isp)
			csid_acquire.sync_mode = i == CAM_ISP_HW_SPLIT_LEFT ?
				CAM_ISP_HW_SYNC_MASTER : CAM_ISP_HW_SYNC_SLAVE;

		csid_acquire.event_cb = cam_ife_hw_mgr_event_handler;
		csid_acquire.tasklet = ife_ctx->common.tasklet_info;
		csid_acquire.cb_priv = ife_ctx;
		csid_acquire.cdm_ops = ife_ctx->cdm_ops;

		rc = cam_ife_hw_mgr_acquire_csid_hw(ife_ctx,
			&csid_acquire,
			in_port);

		if (rc) {
			CAM_ERR(CAM_ISP,
				"Cannot acquire ife csid pxl path rsrc %s",
				(is_ipp) ? "IPP" : "PPP");
			goto put_res;
		}

		csid_res->hw_res[i] = csid_acquire.node_res;
		hw_intf = csid_res->hw_res[i]->hw_intf;

		if (i == CAM_ISP_HW_SPLIT_LEFT) {
			ife_ctx->left_hw_idx = hw_intf->hw_idx;
			ife_ctx->buf_done_controller =
				csid_acquire.buf_done_controller;
		} else {
			ife_ctx->right_hw_idx = hw_intf->hw_idx;
		}

		ife_ctx->flags.need_csid_top_cfg = csid_acquire.need_top_cfg;

		CAM_DBG(CAM_ISP,
			"acquired csid(%s)=%d pxl path rsrc %s successfully",
			(i == 0) ? "left" : "right", hw_intf->hw_idx,
			(is_ipp) ? "IPP" : "PPP");
	}
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_csid, &csid_res);

	if (!is_ipp)
		goto end;

	if (csid_res->is_dual_isp && ife_ctx->flags.need_csid_top_cfg) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {

			if (!csid_res->hw_res[i])
				continue;

			hw_intf = csid_res->hw_res[i]->hw_intf;

			if (i == CAM_ISP_HW_SPLIT_LEFT) {
				dual_sync_args.sync_mode =
					CAM_ISP_HW_SYNC_MASTER;
				dual_sync_args.dual_core_id =
					ife_ctx->right_hw_idx;

			} else if (i == CAM_ISP_HW_SPLIT_RIGHT) {
				dual_sync_args.sync_mode =
					CAM_ISP_HW_SYNC_SLAVE;
				dual_sync_args.dual_core_id =
					ife_ctx->left_hw_idx;
			}

			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_IFE_CSID_SET_DUAL_SYNC_CONFIG,
				&dual_sync_args,
				sizeof(
				struct cam_ife_csid_dual_sync_args));
		}
	}

	return 0;
put_res:
	cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &csid_res);
end:
	return rc;
}

static enum cam_ife_pix_path_res_id
	cam_ife_hw_mgr_get_ife_csid_rdi_res_type(
	uint32_t                 out_port_type)
{
	enum cam_ife_pix_path_res_id path_id;

	switch (out_port_type) {
	case CAM_ISP_IFE_OUT_RES_RDI_0:
	case CAM_ISP_SFE_OUT_RES_RDI_0:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_0;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_1:
	case CAM_ISP_SFE_OUT_RES_RDI_1:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_1;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_2:
	case CAM_ISP_SFE_OUT_RES_RDI_2:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_2;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_3:
	case CAM_ISP_SFE_OUT_RES_RDI_3:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_3;
		break;
	case CAM_ISP_SFE_OUT_RES_RDI_4:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_4;
		break;
	default:
		path_id = CAM_IFE_PIX_PATH_RES_MAX;
		CAM_DBG(CAM_ISP, "maximum rdi output type exceeded");
		break;
	}

	CAM_DBG(CAM_ISP, "out_port: 0x%x path_id: 0x%x",
		out_port_type, path_id);

	return path_id;
}

static int cam_ife_hw_mgr_acquire_res_ife_csid_rdi(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	uint32_t                            *acquired_hw_path)
{
	int rc = -EINVAL;
	int i;

	struct cam_isp_out_port_generic_info  *out_port = NULL;
	struct cam_ife_hw_mgr                 *ife_hw_mgr;
	struct cam_isp_hw_mgr_res             *csid_res;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;
	enum cam_ife_pix_path_res_id         path_res_id;

	ife_hw_mgr = ife_ctx->hw_mgr;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		path_res_id = cam_ife_hw_mgr_get_ife_csid_rdi_res_type(
			out_port->res_type);
		if (path_res_id == CAM_IFE_PIX_PATH_RES_MAX)
			continue;

		rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&csid_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto end;
		}

		csid_res->is_secure = out_port->secure_mode;
		memset(&csid_acquire, 0, sizeof(csid_acquire));
		csid_acquire.res_id = path_res_id;
		csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_acquire.in_port = in_port;
		csid_acquire.out_port = out_port;
		csid_acquire.node_res = NULL;
		csid_acquire.event_cb = cam_ife_hw_mgr_event_handler;
		csid_acquire.tasklet = ife_ctx->common.tasklet_info;
		csid_acquire.cb_priv = ife_ctx;
		csid_acquire.cdm_ops = ife_ctx->cdm_ops;
		if (cam_ife_hw_mgr_is_shdr_fs_rdi_res(
			out_port->res_type,
			ife_ctx->flags.is_sfe_shdr, ife_ctx->flags.is_sfe_fs)) {
			CAM_DBG(CAM_ISP, "setting inline shdr mode for res: 0x%x",
				out_port->res_type);
			csid_acquire.sfe_inline_shdr = true;

			/*
			 * Merged output will only be from the first n RDIs
			 * starting from RDI0. Any other RDI[1:2] resource
			 * if only being dumped will be considered as a
			 * no merge resource
			 */
			if (ife_ctx->flags.is_aeb_mode) {
				if ((out_port->res_type - CAM_ISP_SFE_OUT_RES_RDI_0) >=
					ife_ctx->sfe_info.num_fetches) {
					csid_acquire.sec_evt_config.en_secondary_evt = true;
					csid_acquire.sec_evt_config.evt_type = CAM_IFE_CSID_EVT_SOF;
					CAM_DBG(CAM_ISP,
						"Secondary SOF evt enabled for path: 0x%x",
						out_port->res_type);
				}

				/* Enable EPOCH/SYNC frame drop for error monitoring on master */
				if (out_port->res_type == CAM_ISP_SFE_OUT_RES_RDI_0) {
					csid_acquire.sec_evt_config.en_secondary_evt = true;
					csid_acquire.sec_evt_config.evt_type =
						CAM_IFE_CSID_EVT_EPOCH |
						CAM_IFE_CSID_EVT_SENSOR_SYNC_FRAME_DROP;
					CAM_DBG(CAM_ISP,
						"Secondary EPOCH & frame drop evt enabled for path: 0x%x",
						out_port->res_type);
				}
			}
		}

		/*
		 * Enable RDI pixel drop by default. CSID will enable only for
		 * ver 480 HW to allow userspace to control pixel drop pattern.
		 */
		csid_acquire.drop_enable = true;
		csid_acquire.crop_enable = true;

		if (in_port->usage_type)
			csid_acquire.sync_mode = CAM_ISP_HW_SYNC_MASTER;
		else
			csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;

		rc = cam_ife_hw_mgr_acquire_csid_hw(ife_ctx,
			&csid_acquire,
			in_port);

		if (rc) {
			CAM_ERR(CAM_ISP,
				"CSID Path reserve failed  rc=%d res_id=%d",
				rc,
				path_res_id);

			goto put_res;
		}

		if (csid_acquire.node_res == NULL) {
			CAM_ERR(CAM_ISP, "Acquire CSID RDI rsrc failed");

			goto put_res;
		}

		ife_ctx->flags.need_csid_top_cfg = csid_acquire.need_top_cfg;
		csid_res->res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_res->res_id = csid_acquire.res_id;
		csid_res->is_dual_isp = 0;
		csid_res->hw_res[0] = csid_acquire.node_res;
		csid_res->hw_res[1] = NULL;
		if ((ife_ctx->flags.is_rdi_only_context) ||
			(ife_ctx->flags.is_sfe_fs) ||
			(ife_ctx->flags.is_sfe_shdr)) {
			ife_ctx->buf_done_controller =
				csid_acquire.buf_done_controller;
			ife_ctx->left_hw_idx =
				csid_res->hw_res[0]->hw_intf->hw_idx;
		}
		if (ife_ctx->flags.is_sfe_shdr)
			*acquired_hw_path |= cam_convert_csid_res_to_path(
					csid_res->res_id);
		cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_csid, &csid_res);
	}

	return 0;
put_res:
	cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &csid_res);
end:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_root(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -1;

	if (ife_ctx->res_list_ife_in.res_type == CAM_ISP_RESOURCE_UNINT) {
		/* first acquire */
		ife_ctx->res_list_ife_in.res_type = CAM_ISP_RESOURCE_SRC;
		ife_ctx->res_list_ife_in.res_id = in_port->res_type;
		ife_ctx->res_list_ife_in.is_dual_isp = in_port->usage_type;
	} else if ((ife_ctx->res_list_ife_in.res_id !=
		in_port->res_type) && (!ife_ctx->flags.is_fe_enabled))  {
		CAM_ERR(CAM_ISP, "No Free resource for this context");
		goto err;
	} else {
		/* else do nothing */
	}
	return 0;
err:
	/* release resource in entry function */
	return rc;
}

static int cam_ife_mgr_check_and_update_fe_v0(
	struct cam_ife_hw_mgr_ctx         *ife_ctx,
	struct cam_isp_acquire_hw_info    *acquire_hw_info,
	uint32_t                           acquire_info_size)
{
	int i;
	struct cam_isp_in_port_info       *in_port = NULL;
	uint32_t                           in_port_length = 0;
	uint32_t                           total_in_port_length = 0;

	if (acquire_hw_info->input_info_offset >=
		acquire_hw_info->input_info_size) {
		CAM_ERR(CAM_ISP,
			"Invalid size offset 0x%x is greater then size 0x%x",
			acquire_hw_info->input_info_offset,
			acquire_hw_info->input_info_size);
		return -EINVAL;
	}

	in_port = (struct cam_isp_in_port_info *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset);
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {

		if (((uint8_t *)in_port +
			sizeof(struct cam_isp_in_port_info)) >
			((uint8_t *)acquire_hw_info +
			acquire_info_size)) {
			CAM_ERR(CAM_ISP, "Invalid size");
			return -EINVAL;
		}

		if ((in_port->num_out_res > max_ife_out_res) ||
			(in_port->num_out_res <= 0)) {
			CAM_ERR(CAM_ISP, "Invalid num output res %u",
				in_port->num_out_res);
			return -EINVAL;
		}

		in_port_length = sizeof(struct cam_isp_in_port_info) +
			(in_port->num_out_res - 1) *
			sizeof(struct cam_isp_out_port_info);
		total_in_port_length += in_port_length;

		if (total_in_port_length > acquire_hw_info->input_info_size) {
			CAM_ERR(CAM_ISP, "buffer size is not enough");
			return -EINVAL;
		}
		CAM_DBG(CAM_ISP, "in_port%d res_type %d", i,
			in_port->res_type);
		if (in_port->res_type == CAM_ISP_IFE_IN_RES_RD) {
			ife_ctx->flags.is_fe_enabled = true;
			break;
		}

		in_port = (struct cam_isp_in_port_info *)((uint8_t *)in_port +
			in_port_length);
	}
	CAM_DBG(CAM_ISP, "is_fe_enabled %d", ife_ctx->flags.is_fe_enabled);

	return 0;
}

static bool cam_ife_mgr_check_for_sfe_rd(
	uint32_t sfe_in_path_type)
{
	if (((sfe_in_path_type & 0xFFFF) == CAM_ISP_SFE_IN_RD_0) ||
		((sfe_in_path_type & 0xFFFF) == CAM_ISP_SFE_IN_RD_1) ||
		((sfe_in_path_type & 0xFFFF) == CAM_ISP_SFE_IN_RD_2))
		return true;
	else
		return false;
}

static int cam_ife_mgr_check_and_update_fe_v2(
	struct cam_ife_hw_mgr_ctx         *ife_ctx,
	struct cam_isp_acquire_hw_info    *acquire_hw_info,
	uint32_t                           acquire_info_size)
{
	int i;
	bool is_sfe_rd = false, fetch_cfg = false;
	struct cam_isp_in_port_info_v2    *in_port = NULL;
	uint32_t                           in_port_length = 0;
	uint32_t                           total_in_port_length = 0;

	if (acquire_hw_info->input_info_offset >=
		acquire_hw_info->input_info_size) {
		CAM_ERR(CAM_ISP,
			"Invalid size offset 0x%x is greater then size 0x%x",
			acquire_hw_info->input_info_offset,
			acquire_hw_info->input_info_size);
		return -EINVAL;
	}

	in_port = (struct cam_isp_in_port_info_v2 *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset);
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {

		if (((uint8_t *)in_port +
			sizeof(struct cam_isp_in_port_info)) >
			((uint8_t *)acquire_hw_info +
			acquire_info_size)) {
			CAM_ERR(CAM_ISP, "Invalid size");
			return -EINVAL;
		}

		if ((in_port->num_out_res > (max_ife_out_res +
			g_ife_hw_mgr.isp_bus_caps.max_sfe_out_res_type)) ||
			(in_port->num_out_res <= 0)) {
			CAM_ERR(CAM_ISP, "Invalid num output res %u",
				in_port->num_out_res);
			return -EINVAL;
		}

		in_port_length = sizeof(struct cam_isp_in_port_info_v2) +
			(in_port->num_out_res - 1) *
			sizeof(struct cam_isp_out_port_info_v2);
		total_in_port_length += in_port_length;

		if (total_in_port_length > acquire_hw_info->input_info_size) {
			CAM_ERR(CAM_ISP, "buffer size is not enough");
			return -EINVAL;
		}
		CAM_DBG(CAM_ISP, "in_port%d res_type %d", i,
			in_port->res_type);
		is_sfe_rd = cam_ife_mgr_check_for_sfe_rd(in_port->sfe_in_path_type);
		if (is_sfe_rd)
			ife_ctx->sfe_info.num_fetches++;

		if ((!fetch_cfg) && ((in_port->res_type == CAM_ISP_IFE_IN_RES_RD) ||
			(is_sfe_rd))) {
			ife_ctx->flags.is_fe_enabled = true;

			/* Check for offline */
			if (in_port->offline_mode)
				ife_ctx->flags.is_offline = true;

			/* Check for inline fetch modes */
			if ((is_sfe_rd) && (!ife_ctx->flags.is_offline)) {
				/* Check for SFE FS mode - SFE PP bypass */
				if (in_port->feature_flag & CAM_ISP_SFE_FS_MODE_EN)
					ife_ctx->flags.is_sfe_fs = true;
				else
					ife_ctx->flags.is_sfe_shdr = true;
			}

			/* If once configured skip these checks thereafter */
			fetch_cfg = true;
		}

		in_port = (struct cam_isp_in_port_info_v2 *)
			((uint8_t *)in_port + in_port_length);
	}
	CAM_DBG(CAM_ISP,
		"is_fe_enabled %d is_offline %d sfe_fs %d sfe_shdr: %d num_sfe_fetches: %u",
		ife_ctx->flags.is_fe_enabled,
		ife_ctx->flags.is_offline,
		ife_ctx->flags.is_sfe_fs,
		ife_ctx->flags.is_sfe_shdr,
		ife_ctx->sfe_info.num_fetches);

	return 0;
}

static int cam_ife_mgr_check_and_update_fe(
	struct cam_ife_hw_mgr_ctx         *ife_ctx,
	struct cam_isp_acquire_hw_info    *acquire_hw_info,
	uint32_t                           acquire_info_size)
{
	uint32_t major_ver = 0, minor_ver = 0;

	if (acquire_hw_info == NULL || ife_ctx == NULL)
		return -EINVAL;

	major_ver = (acquire_hw_info->common_info_version >> 12) & 0xF;
	minor_ver = (acquire_hw_info->common_info_version) & 0xFFF;

	switch (major_ver) {
	case 1:
		return cam_ife_mgr_check_and_update_fe_v0(
			ife_ctx, acquire_hw_info, acquire_info_size);
	case 2:
		return cam_ife_mgr_check_and_update_fe_v2(
			ife_ctx, acquire_hw_info, acquire_info_size);
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid ver of common info from user");
		return -EINVAL;
	}

	return 0;
}

static int cam_ife_hw_mgr_preprocess_port(
	struct cam_ife_hw_mgr_ctx   *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	uint32_t i;
	struct cam_isp_out_port_generic_info *out_port;

	if (in_port->res_type == CAM_ISP_IFE_IN_RES_RD ||
		in_port->sfe_in_path_type == CAM_ISP_SFE_IN_RD_0 ||
		in_port->sfe_in_path_type == CAM_ISP_SFE_IN_RD_1 ||
		in_port->sfe_in_path_type == CAM_ISP_SFE_IN_RD_2)
		in_port->ife_rd_count++;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		if (cam_ife_hw_mgr_is_rdi_res(out_port->res_type)) {
			in_port->rdi_count++;
			in_port->lite_path_count++;
		}
		else if (cam_ife_hw_mgr_is_sfe_rdi_res(out_port->res_type))
			in_port->rdi_count++;
		else if (cam_ife_hw_mgr_check_path_port_compat(CAM_ISP_HW_VFE_IN_PDLIB,
				out_port->res_type))
			in_port->ppp_count++;
		else if (cam_ife_hw_mgr_check_path_port_compat(CAM_ISP_HW_VFE_IN_LCR,
				out_port->res_type))
			in_port->lcr_count++;
		else {
			CAM_DBG(CAM_ISP, "out_res_type %d",
			out_port->res_type);
			in_port->ipp_count++;
			if (in_port->can_use_lite) {
				switch(out_port->res_type) {
				case CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW:
				case CAM_ISP_IFE_LITE_OUT_RES_STATS_BG:
					in_port->lite_path_count++;
				break;
				default:
					CAM_WARN(CAM_ISP, "Output port 0x%x cannot use lite",
							out_port->res_type);
				}
			}
		}
	}

	CAM_DBG(CAM_ISP, "rdi: %d ipp: %d ppp: %d ife_rd: %d lcr: %d",
		in_port->rdi_count, in_port->ipp_count,
		in_port->ppp_count, in_port->ife_rd_count,
		in_port->lcr_count);

	return 0;
}

static int cam_ife_hw_mgr_acquire_offline_res_ife_camif(
	struct cam_ife_hw_mgr_ctx                  *ife_ctx,
	struct cam_isp_in_port_generic_info        *in_port,
	bool                                        acquire_lcr,
	uint32_t                                   *acquired_hw_id,
	uint32_t                                   *acquired_hw_path)
{
	int                                         rc = -1;
	int                                         i;
	struct cam_vfe_acquire_args                 vfe_acquire;
	struct cam_hw_intf                         *hw_intf = NULL;
	struct cam_isp_hw_mgr_res                  *ife_src_res;
	struct cam_isp_hw_mgr_res                  *isp_bus_rd_res;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;

	isp_bus_rd_res = list_first_entry(&ife_ctx->res_list_ife_in_rd,
		struct cam_isp_hw_mgr_res, list);
	if (!isp_bus_rd_res) {
		CAM_ERR(CAM_ISP, "BUS RD resource has not been acquired");
		rc = -EINVAL;
		goto end;
	}

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &ife_src_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No free resource");
		goto end;
	}

	vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_IN;
	vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
	vfe_acquire.priv = ife_ctx;
	vfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;

	vfe_acquire.vfe_in.cdm_ops = ife_ctx->cdm_ops;
	vfe_acquire.vfe_in.in_port = in_port;
	vfe_acquire.vfe_in.is_fe_enabled = ife_ctx->flags.is_fe_enabled;
	vfe_acquire.vfe_in.is_offline = ife_ctx->flags.is_offline;

	if (!acquire_lcr)
		vfe_acquire.vfe_in.res_id = CAM_ISP_HW_VFE_IN_CAMIF;
	else
		vfe_acquire.vfe_in.res_id = CAM_ISP_HW_VFE_IN_LCR;

	if (ife_ctx->flags.is_dual)
		vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_MASTER;
	else
		vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;

	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (!ife_hw_mgr->ife_devices[i])
			continue;

		hw_intf = ife_hw_mgr->ife_devices[i]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv, &vfe_acquire,
			sizeof(struct cam_vfe_acquire_args));

		if (rc)
			continue;
		else
			break;
	}

	if (i == CAM_IFE_HW_NUM_MAX || rc ||
			!vfe_acquire.vfe_in.rsrc_node) {
		CAM_ERR(CAM_ISP, "Failed to acquire IFE LEFT rc: %d",
			rc);
		goto put_res;
	}

	ife_src_res->hw_res[0] = vfe_acquire.vfe_in.rsrc_node;
	*acquired_hw_id |= cam_convert_hw_idx_to_ife_hw_num(
		hw_intf->hw_idx);
	acquired_hw_path[i] |= cam_convert_res_id_to_hw_path(
		ife_src_res->hw_res[0]->res_id);

	CAM_DBG(CAM_ISP, "Acquired VFE:%d CAMIF for LEFT",
		ife_src_res->hw_res[0]->hw_intf->hw_idx);

	ife_src_res->res_type = vfe_acquire.rsrc_type;
	ife_src_res->res_id = vfe_acquire.vfe_in.res_id;
	ife_src_res->is_dual_isp = (uint32_t)ife_ctx->flags.is_dual;
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_src, &ife_src_res);

	if (ife_ctx->flags.is_dual) {
		vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_SLAVE;
		vfe_acquire.vfe_in.rsrc_node = NULL;
		for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
			if (!ife_hw_mgr->ife_devices[i])
				continue;

			if (i == ife_src_res->hw_res[0]->hw_intf->hw_idx)
				continue;

			hw_intf = ife_hw_mgr->ife_devices[i]->hw_intf;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&vfe_acquire,
				sizeof(struct cam_vfe_acquire_args));

			if (rc)
				continue;
			else
				break;
		}

		if (rc || !vfe_acquire.vfe_in.rsrc_node) {
			CAM_ERR(CAM_ISP, "Failed to acquire IFE RIGHT rc: %d",
				rc);
			goto end;
		}

		ife_src_res->hw_res[1] = vfe_acquire.vfe_in.rsrc_node;

		*acquired_hw_id |= cam_convert_hw_idx_to_ife_hw_num(
			hw_intf->hw_idx);

		acquired_hw_path[i] |= cam_convert_res_id_to_hw_path(
			ife_src_res->hw_res[1]->res_id);

		CAM_DBG(CAM_ISP, "Acquired VFE:%d CAMIF for RIGHT",
			ife_src_res->hw_res[1]->hw_intf->hw_idx);
	}

	return rc;

put_res:
	cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &ife_src_res);

end:
	return rc;
}

static int cam_ife_hw_mgr_acquire_offline_res_sfe(
	struct cam_ife_hw_mgr_ctx                  *ife_ctx,
	struct cam_isp_in_port_generic_info        *in_port)
{
	int                                         rc = -1;
	int                                         i = CAM_ISP_HW_SPLIT_LEFT;
	struct cam_sfe_acquire_args                 sfe_acquire;
	struct cam_isp_hw_mgr_res                  *sfe_src_res;
	struct cam_isp_hw_mgr_res                  *sfe_bus_rd_res;
	struct cam_hw_intf                         *hw_intf;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;

	sfe_bus_rd_res = list_first_entry(&ife_ctx->res_list_ife_in_rd,
		struct cam_isp_hw_mgr_res, list);

	if (!sfe_bus_rd_res) {
		CAM_ERR(CAM_ISP, "BUS RD resource has not been acquired");
		rc = -EINVAL;
		goto end;
	}

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &sfe_src_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No free resource");
		goto end;
	}

	sfe_acquire.rsrc_type = CAM_ISP_RESOURCE_SFE_IN;
	sfe_acquire.tasklet = ife_ctx->common.tasklet_info;
	sfe_acquire.priv = ife_ctx;
	sfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;
	sfe_acquire.sfe_in.cdm_ops = ife_ctx->cdm_ops;
	sfe_acquire.sfe_in.in_port = in_port;
	sfe_acquire.sfe_in.is_offline = ife_ctx->flags.is_offline;
	sfe_acquire.sfe_in.res_id = CAM_ISP_HW_SFE_IN_PIX;

	hw_intf = ife_hw_mgr->sfe_devices[
		sfe_bus_rd_res->hw_res[i]->hw_intf->hw_idx]->hw_intf;

	rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv, &sfe_acquire,
		sizeof(struct cam_sfe_acquire_args));
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Failed to acquire SFE PIX for offline");
		goto put_res;
	}

	sfe_src_res->hw_res[i] = sfe_acquire.sfe_in.rsrc_node;
	CAM_DBG(CAM_ISP, "Acquired SFE: %u PIX LEFT for offline",
		sfe_src_res->hw_res[i]->hw_intf->hw_idx);

	sfe_src_res->res_type = sfe_acquire.rsrc_type;
	sfe_src_res->res_id = sfe_acquire.sfe_in.res_id;
	sfe_src_res->is_dual_isp = in_port->usage_type;
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_sfe_src, &sfe_src_res);

	if (ife_ctx->flags.is_dual) {
		CAM_WARN(CAM_ISP,
			"DUAL not supported for offline use-case");

		sfe_acquire.sfe_in.rsrc_node = NULL;
		hw_intf = ife_hw_mgr->sfe_devices[
			sfe_bus_rd_res->hw_res[++i]->hw_intf->hw_idx]->hw_intf;

		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv, &sfe_acquire,
			sizeof(struct cam_sfe_acquire_args));

		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed to acquire SFE PIX for RIGHT");
			goto end;
		}

		sfe_src_res->hw_res[i] = sfe_acquire.sfe_in.rsrc_node;
		CAM_DBG(CAM_ISP, "Acquired SFE:%d PIX RIGHT for offline",
			sfe_src_res->hw_res[i]->hw_intf->hw_idx);
	}

	sfe_bus_rd_res->num_children++;

	return rc;

put_res:
	cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &sfe_src_res);
end:
	return rc;
}

/* Acquire CSID for offline SFE */
static int cam_ife_hw_mgr_acquire_offline_res_csid(
	struct cam_ife_hw_mgr_ctx                  *ife_ctx,
	struct cam_isp_in_port_generic_info        *in_port)
{
	int                                         rc = -1;
	uint32_t                                    path_res_id = 0;
	struct cam_csid_hw_reserve_resource_args    csid_acquire;
	struct cam_isp_hw_mgr_res                  *sfe_bus_rd_res;
	struct cam_isp_hw_mgr_res                  *csid_res;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;

	sfe_bus_rd_res = list_first_entry(&ife_ctx->res_list_ife_in_rd,
		struct cam_isp_hw_mgr_res, list);

	if (!sfe_bus_rd_res) {
		CAM_ERR(CAM_ISP, "BUS RD resource has not been acquired");
		rc = -EINVAL;
		goto end;
	}

	path_res_id = cam_ife_hw_mgr_get_csid_rdi_type_for_offline(
			sfe_bus_rd_res->res_id);

	if (path_res_id == CAM_IFE_PIX_PATH_RES_MAX)
		return -EINVAL;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&csid_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto end;
	}

	memset(&csid_acquire, 0, sizeof(csid_acquire));
	csid_acquire.res_id = path_res_id;
	csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
	csid_acquire.in_port = in_port;
	csid_acquire.out_port = in_port->data;
	csid_acquire.node_res = NULL;
	csid_acquire.event_cb = cam_ife_hw_mgr_event_handler;
	csid_acquire.tasklet = ife_ctx->common.tasklet_info;
	csid_acquire.cb_priv = ife_ctx;
	csid_acquire.cdm_ops = ife_ctx->cdm_ops;
	csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;
	csid_acquire.is_offline = true;

	rc = cam_ife_hw_mgr_acquire_csid_hw(ife_ctx,
		&csid_acquire, in_port);

	if (rc || (csid_acquire.node_res == NULL)) {
		CAM_ERR(CAM_ISP,
			"CSID Path reserve failed  rc=%d res_id=%d",
			rc, path_res_id);
		goto end;
	}

	ife_ctx->buf_done_controller =
		csid_acquire.buf_done_controller;

	ife_ctx->flags.need_csid_top_cfg = csid_acquire.need_top_cfg;
	csid_res->res_type = CAM_ISP_RESOURCE_PIX_PATH;
	csid_res->res_id = csid_acquire.res_id;
	csid_res->is_dual_isp = 0;
	csid_res->hw_res[0] = csid_acquire.node_res;
	csid_res->hw_res[1] = NULL;
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_csid, &csid_res);

end:
	return rc;
}

/* Acquire CSID & SFE for offline */
static int cam_ife_mgr_acquire_hw_sfe_offline(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	uint32_t                            *acquired_hw_id,
	uint32_t                            *acquired_hw_path)
{
	int rc;

	rc = cam_ife_hw_mgr_acquire_sfe_bus_rd(
		ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire SFE BUS RD resource Failed");
		goto err;
	}

	rc = cam_ife_hw_mgr_acquire_offline_res_csid(ife_ctx,
			in_port);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Acquire IFE CSID RDI0 resource Failed");
			goto err;
	}

	rc = cam_ife_hw_mgr_acquire_offline_res_sfe(
		ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Acquire SFE PIX SRC resource Failed");
		goto err;
	}

	if (in_port->sfe_ife_enable) {
		if (in_port->ipp_count) {
			rc = cam_ife_hw_mgr_acquire_offline_res_ife_camif(
				ife_ctx, in_port, false,
				acquired_hw_id, acquired_hw_path);
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Acquire IFE IPP SRC resource Failed");
				goto err;
			}
		}

		if (in_port->lcr_count) {
			rc = cam_ife_hw_mgr_acquire_offline_res_ife_camif(
					ife_ctx, in_port, true,
					acquired_hw_id, acquired_hw_path);
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Acquire IFE LCR SRC resource Failed");
				goto err;
			}
		}

		rc = cam_ife_hw_mgr_acquire_res_ife_out(
			ife_ctx, in_port);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE OUT resource Failed");
			goto err;
		}
	}

	rc = cam_ife_hw_mgr_acquire_res_sfe_out(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Acquire SFE OUT resource Failed");
			goto err;
	}

	return 0;
err:
	return rc;
}

/* Acquire HWs for IFE fetch engine archs */
static int cam_ife_mgr_acquire_hw_ife_offline(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	uint32_t                            *acquired_hw_id,
	uint32_t                            *acquired_hw_path)
{
	int                                  rc = -1;

	rc = cam_ife_hw_mgr_acquire_res_ife_bus_rd(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE BUS RD resource Failed");
		goto err;
	}

	if (in_port->ipp_count)
		rc = cam_ife_hw_mgr_acquire_offline_res_ife_camif(ife_ctx,
			in_port, false, acquired_hw_id, acquired_hw_path);

	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE IPP SRC resource Failed");
		goto err;
	}

	if (in_port->lcr_count)
		rc = cam_ife_hw_mgr_acquire_offline_res_ife_camif(ife_ctx,
			in_port, true, acquired_hw_id, acquired_hw_path);

	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE LCR SRC resource Failed");
		goto err;
	}

	rc = cam_ife_hw_mgr_acquire_res_ife_out(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE OUT resource Failed");
		goto err;
	}

	return 0;

err:
	return rc;
}

static int cam_ife_mgr_acquire_hw_for_offline_ctx(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	uint32_t                            *acquired_hw_id,
	uint32_t                            *acquired_hw_path)
{
	int rc = -EINVAL;

	ife_ctx->flags.is_dual = (bool)in_port->usage_type;
	if ((!in_port->ipp_count && !in_port->lcr_count) ||
		!in_port->ife_rd_count) {
		CAM_ERR(CAM_ISP,
			"Invalid %d BUS RD %d PIX %d LCR ports for FE ctx");
		return -EINVAL;
	}

	if (in_port->rdi_count || in_port->ppp_count) {
		CAM_ERR(CAM_ISP,
			"%d RDI %d PPP ports invalid for FE ctx",
			in_port->rdi_count, in_port->ppp_count);
		return -EINVAL;
	}

	if (ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE)
		rc = cam_ife_mgr_acquire_hw_sfe_offline(
			ife_ctx, in_port, acquired_hw_id, acquired_hw_path);
	else
		rc = cam_ife_mgr_acquire_hw_ife_offline(
			ife_ctx, in_port, acquired_hw_id, acquired_hw_path);

	return rc;
}


static int cam_ife_mgr_acquire_hw_for_ctx(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	uint32_t *acquired_hw_id,
	uint32_t *acquired_hw_path)
{
	int rc                                    = -1;
	int is_dual_isp                           = 0;
	bool crop_enable                          = true;

	is_dual_isp = in_port->usage_type;
	ife_ctx->flags.dsp_enabled = (bool)in_port->dsp_mode;
	ife_ctx->flags.is_dual = (bool)in_port->usage_type;

	/* Update aeb mode for the given in_port once */
	if ((in_port->aeb_mode) && (!ife_ctx->flags.is_aeb_mode))
		ife_ctx->flags.is_aeb_mode = true;

	/* get root node resource */
	rc = cam_ife_hw_mgr_acquire_res_root(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Can not acquire root resource");
		goto err;
	}

	if (!in_port->ipp_count && !in_port->rdi_count &&
		!in_port->ppp_count && !in_port->lcr_count) {

		CAM_ERR(CAM_ISP,
			"No PIX or RDI or PPP or LCR resource");
		return -EINVAL;
	}

	if (in_port->ife_rd_count) {
		if (ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE)
			rc = cam_ife_hw_mgr_acquire_sfe_bus_rd(
				ife_ctx, in_port);
		else
			rc = cam_ife_hw_mgr_acquire_res_ife_bus_rd(
				ife_ctx, in_port);
		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire %s BUS RD resource Failed",
				(ife_ctx->ctx_type ==
				CAM_IFE_CTX_TYPE_SFE) ? "SFE" : "IFE");
			goto err;
		}
	}

	if ((ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) &&
		(in_port->ife_rd_count))
		goto skip_csid_pxl;

	/* Skip CSID PIX for SFE sHDR & FS */
	if (in_port->ipp_count || in_port->lcr_count) {
		/* get ife csid IPP resource */
		rc = cam_ife_hw_mgr_acquire_res_ife_csid_pxl(ife_ctx,
			in_port, true, crop_enable);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE CSID IPP/LCR resource Failed");
			goto err;
		}
	}


skip_csid_pxl:

	if (in_port->rdi_count) {
		/* get ife csid RDI resource */
		rc = cam_ife_hw_mgr_acquire_res_ife_csid_rdi(ife_ctx, in_port,
			acquired_hw_path);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE CSID RDI resource Failed");
			goto err;
		}
	}

	if (in_port->ppp_count) {
		/* get ife csid PPP resource */

		/* If both IPP and PPP paths are requested with the same vc dt
		 * it is implied that the sensor is a type 3 PD sensor. Crop
		 * must be enabled for this sensor on PPP path as well.
		 */
		if (!in_port->ipp_count)
			crop_enable = false;

		rc = cam_ife_hw_mgr_acquire_res_ife_csid_pxl(ife_ctx,
			in_port, false, crop_enable);

		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE CSID PPP resource Failed");
			goto err;
		}
	}

	/* acquire SFE input resource */
	if ((ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) &&
		(in_port->ipp_count || in_port->rdi_count)) {
		rc = cam_ife_hw_mgr_acquire_res_sfe_src(ife_ctx,
			in_port);
		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire SFE SRC resource failed");
			goto err;
		}
	}

	/* get ife IPP src resource */
	if (in_port->ipp_count) {
		if (ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
			if (in_port->sfe_ife_enable)
				rc = cam_ife_hw_mgr_acquire_ife_src_for_sfe(
					ife_ctx, in_port, false,
					acquired_hw_id, acquired_hw_path);
		} else {
			rc = cam_ife_hw_mgr_acquire_res_ife_src(ife_ctx,
				in_port, false, false,
				acquired_hw_id, acquired_hw_path);
		}

		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE IPP SRC resource Failed");
			goto err;
		}
	}

	/* get ife RDI src resource for non SFE streams */
	if (in_port->rdi_count) {
		if (ife_ctx->ctx_type != CAM_IFE_CTX_TYPE_SFE) {
			rc = cam_ife_hw_mgr_acquire_res_ife_src(ife_ctx,
				in_port, false, false,
				acquired_hw_id, acquired_hw_path);

			if (rc) {
				CAM_ERR(CAM_ISP,
					"Acquire IFE RDI SRC resource Failed");
				goto err;
			}
		}
	}

	if (in_port->lcr_count) {
		if (ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
			if (in_port->sfe_ife_enable)
				rc = cam_ife_hw_mgr_acquire_ife_src_for_sfe(
					ife_ctx, in_port, true,
					acquired_hw_id, acquired_hw_path);
		} else {
			rc = cam_ife_hw_mgr_acquire_res_ife_src(
				ife_ctx, in_port, true, false,
				acquired_hw_id, acquired_hw_path);
		}

		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire IFE LCR SRC resource Failed");
			goto err;
		}
	}

	/* PPP path is from CSID->IFE bypassing SFE */
	if (in_port->ppp_count) {
		rc = cam_ife_hw_mgr_acquire_res_ife_src(ife_ctx, in_port,
				false, true, acquired_hw_id, acquired_hw_path);
		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire IFE PPP SRC resource Failed");
			goto err;
		}
	}

	rc = cam_ife_hw_mgr_acquire_res_ife_out(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE OUT resource Failed");
		goto err;
	}

	if (ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
		rc = cam_ife_hw_mgr_acquire_res_sfe_out(ife_ctx, in_port);
		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire SFE OUT resource Failed");
			goto err;
		}
	}

	if (in_port->dynamic_sensor_switch_en)
		ife_ctx->ctx_config |= CAM_IFE_CTX_CFG_DYNAMIC_SWITCH_ON;

	return 0;
err:
	/* release resource at the acquire entry funciton */
	return rc;
}

void cam_ife_cam_cdm_callback(uint32_t handle, void *userdata,
	enum cam_cdm_cb_status status, uint64_t cookie)
{
	struct cam_isp_prepare_hw_update_data   *hw_update_data = NULL;
	struct cam_ife_hw_mgr_ctx               *ctx = NULL;
	int                                      reg_dump_done;

	if (!userdata) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)userdata;
	hw_update_data = ctx->cdm_userdata.hw_update_data;

	if (status == CAM_CDM_CB_STATUS_BL_SUCCESS) {
		complete_all(&ctx->config_done_complete);
		reg_dump_done = atomic_read(&ctx->cdm_done);
		atomic_set(&ctx->cdm_done, 1);
		ctx->last_cdm_done_req = cookie;
		if ((g_ife_hw_mgr.debug_cfg.per_req_reg_dump) &&
			(!reg_dump_done)) {
			if (ctx->cdm_userdata.request_id == cookie) {
				cam_ife_mgr_handle_reg_dump(ctx,
					hw_update_data->reg_dump_buf_desc,
					hw_update_data->num_reg_dump_buf,
					CAM_ISP_PACKET_META_REG_DUMP_PER_REQUEST,
					NULL, false);
			} else {
				CAM_INFO(CAM_ISP, "CDM delay, Skip dump req: %llu, cdm_req: %llu",
					cookie, ctx->cdm_userdata.request_id);
			}
		}
		CAM_DBG(CAM_ISP,
			"CDM hdl=0x%x, udata=%pK, status=%d, cookie=%llu ctx_index=%d cdm_req=%llu",
			 handle, userdata, status, cookie, ctx->ctx_index,
			 ctx->cdm_userdata.request_id);
	} else {
		CAM_WARN(CAM_ISP,
			"Called by CDM hdl=0x%x, udata=%pK, status=%d, cookie=%llu, cdm_req=%llu",
			 handle, userdata, status, cookie, ctx->cdm_userdata.request_id);
	}
}

static int cam_ife_mgr_acquire_get_unified_structure_v0(
	struct cam_isp_acquire_hw_info *acquire_hw_info,
	uint32_t offset, uint32_t *input_size,
	struct cam_isp_in_port_generic_info *in_port)
{
	struct cam_isp_in_port_info *in = NULL;
	uint32_t in_port_length = 0;
	int32_t rc = 0, i;

	in = (struct cam_isp_in_port_info *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset + *input_size);

	in_port_length = sizeof(struct cam_isp_in_port_info) +
		(in->num_out_res - 1) *
		sizeof(struct cam_isp_out_port_info);

	*input_size += in_port_length;

	if (!in_port || ((*input_size) > acquire_hw_info->input_info_size)) {
		CAM_ERR(CAM_ISP, "Input is not proper");
		rc = -EINVAL;
		goto err;
	}

	in_port->major_ver       =
		(acquire_hw_info->input_info_version >> 16) & 0xFFFF;
	in_port->minor_ver       =
		acquire_hw_info->input_info_version & 0xFFFF;
	in_port->res_type        =  in->res_type;
	in_port->lane_type       =  in->lane_type;
	in_port->lane_num        =  in->lane_num;
	in_port->lane_cfg        =  in->lane_cfg;
	in_port->vc[0]           =  in->vc;
	in_port->dt[0]           =  in->dt;
	in_port->num_valid_vc_dt = 1;
	in_port->format[0]       =  in->format;
	in_port->test_pattern    =  in->test_pattern;
	in_port->usage_type      =  in->usage_type;
	in_port->left_start      =  in->left_start;
	in_port->left_stop       =  in->left_stop;
	in_port->left_width      =  in->left_width;
	in_port->right_start     =  in->right_start;
	in_port->right_stop      =  in->right_stop;
	in_port->right_width     =  in->right_width;
	in_port->line_start      =  in->line_start;
	in_port->line_stop       =  in->line_stop;
	in_port->height          =  in->height;
	in_port->pixel_clk       =  in->pixel_clk;
	in_port->batch_size      =  in->batch_size;
	in_port->dsp_mode        =  in->dsp_mode;
	in_port->hbi_cnt         =  in->hbi_cnt;
	in_port->cust_node       =  0;
	in_port->horizontal_bin  =  0;
	in_port->qcfa_bin        =  0;
	in_port->num_out_res     =  in->num_out_res;

	in_port->data = kcalloc(in->num_out_res,
		sizeof(struct cam_isp_out_port_generic_info),
		GFP_KERNEL);
	if (in_port->data == NULL) {
		rc = -ENOMEM;
		goto err;
	}

	for (i = 0; i < in->num_out_res; i++) {
		in_port->data[i].res_type     = in->data[i].res_type;
		in_port->data[i].format       = in->data[i].format;
		in_port->data[i].width        = in->data[i].width;
		in_port->data[i].height       = in->data[i].height;
		in_port->data[i].comp_grp_id  = in->data[i].comp_grp_id;
		in_port->data[i].split_point  = in->data[i].split_point;
		in_port->data[i].secure_mode  = in->data[i].secure_mode;
		in_port->data[i].reserved     = in->data[i].reserved;
	}

	return 0;
err:
	return rc;
}

static inline int cam_ife_mgr_hw_check_in_res_type(
	u32 res_type)
{
	switch (res_type) {
	case CAM_ISP_IFE_IN_RES_RD:
		return 0;
	case CAM_ISP_SFE_IN_RD_0:
		return 0;
	case CAM_ISP_SFE_IN_RD_1:
		return 0;
	case CAM_ISP_SFE_IN_RD_2:
		return 0;
	default:
		return -EINVAL;
	}
}

static inline void cam_ife_mgr_acquire_get_feature_flag_params(
	struct cam_isp_in_port_info_v2      *in,
	struct cam_isp_in_port_generic_info *in_port)
{
	in_port->secure_mode              = in->feature_flag & CAM_ISP_PARAM_FETCH_SECURITY_MODE;
	in_port->dynamic_sensor_switch_en = in->feature_flag & CAM_ISP_DYNAMIC_SENOR_SWITCH_EN;
	in_port->can_use_lite             = in->feature_flag & CAM_ISP_CAN_USE_LITE_MODE;
	in_port->sfe_binned_epoch_cfg     = in->feature_flag & CAM_ISP_SFE_BINNED_EPOCH_CFG_ENABLE;
	in_port->epd_supported            = in->feature_flag & CAM_ISP_EPD_SUPPORT;
	in_port->aeb_mode                 = in->feature_flag & CAM_ISP_AEB_MODE_EN;
}

static int cam_ife_mgr_acquire_get_unified_structure_v2(
	struct cam_isp_acquire_hw_info *acquire_hw_info,
	uint32_t offset, uint32_t *input_size,
	struct cam_isp_in_port_generic_info *in_port)
{
	struct cam_isp_in_port_info_v2 *in = NULL;
	uint32_t in_port_length = 0;
	int32_t rc = 0, i;

	in = (struct cam_isp_in_port_info_v2 *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset + *input_size);

	in_port_length = sizeof(struct cam_isp_in_port_info_v2) +
		(in->num_out_res - 1) *
		sizeof(struct cam_isp_out_port_info_v2);

	*input_size += in_port_length;

	if (!in_port || ((*input_size) > acquire_hw_info->input_info_size)) {
		CAM_ERR(CAM_ISP, "Input is not proper");
		rc = -EINVAL;
		goto err;
	}

	in_port->major_ver       =
		(acquire_hw_info->input_info_version >> 16) & 0xFFFF;
	in_port->minor_ver       =
		acquire_hw_info->input_info_version & 0xFFFF;
	in_port->res_type        =  in->res_type;
	in_port->lane_type       =  in->lane_type;
	in_port->lane_num        =  in->lane_num;
	in_port->lane_cfg        =  in->lane_cfg;
	in_port->num_valid_vc_dt =  in->num_valid_vc_dt;

	if (in_port->num_valid_vc_dt == 0 ||
		in_port->num_valid_vc_dt >= CAM_ISP_VC_DT_CFG) {
		if (cam_ife_mgr_hw_check_in_res_type(in->res_type)) {
			CAM_ERR(CAM_ISP, "Invalid i/p arg invalid vc-dt: %d",
				in->num_valid_vc_dt);
			rc = -EINVAL;
			goto err;
		}
	}

	for (i = 0; i < in_port->num_valid_vc_dt; i++) {
		in_port->vc[i]        =  in->vc[i];
		in_port->dt[i]        =  in->dt[i];
	}

	for (i = 0; i < in_port->num_valid_vc_dt; i++) {
		in_port->format[i] = (in->format >> (i * CAM_IFE_DECODE_FORMAT_SHIFT_VAL)) &
			CAM_IFE_DECODE_FORMAT_MASK;
	}

	in_port->test_pattern             =  in->test_pattern;
	in_port->usage_type               =  in->usage_type;
	in_port->left_start               =  in->left_start;
	in_port->left_stop                =  in->left_stop;
	in_port->left_width               =  in->left_width;
	in_port->right_start              =  in->right_start;
	in_port->right_stop               =  in->right_stop;
	in_port->right_width              =  in->right_width;
	in_port->line_start               =  in->line_start;
	in_port->line_stop                =  in->line_stop;
	in_port->height                   =  in->height;
	in_port->pixel_clk                =  in->pixel_clk;
	in_port->batch_size               =  in->batch_size;
	in_port->dsp_mode                 =  in->dsp_mode;
	in_port->fe_unpacker_fmt          =  in->format;
	in_port->hbi_cnt                  =  in->hbi_cnt;
	in_port->cust_node                =  in->cust_node;
	in_port->horizontal_bin           =  (in->bidirectional_bin & 0xFFFF);
	in_port->vertical_bin             =  (in->bidirectional_bin >> 16);
	in_port->qcfa_bin                 =  in->qcfa_bin;
	in_port->num_out_res              =  in->num_out_res;
	in_port->sfe_in_path_type         =  (in->sfe_in_path_type & 0xFFFF);
	in_port->sfe_ife_enable           =  in->sfe_in_path_type >> 16;

	cam_ife_mgr_acquire_get_feature_flag_params(in, in_port);

	in_port->data = kcalloc(in->num_out_res,
		sizeof(struct cam_isp_out_port_generic_info),
		GFP_KERNEL);
	if (in_port->data == NULL) {
		rc = -ENOMEM;
		goto err;
	}

	for (i = 0; i < in_port->num_out_res; i++) {
		in_port->data[i].res_type     = in->data[i].res_type;
		in_port->data[i].format       = in->data[i].format;
		in_port->data[i].width        = in->data[i].width;
		in_port->data[i].height       = in->data[i].height;
		in_port->data[i].comp_grp_id  = in->data[i].comp_grp_id;
		in_port->data[i].split_point  = in->data[i].split_point;
		in_port->data[i].secure_mode  = in->data[i].secure_mode;
	}

	return 0;

err:
	return rc;
}

static int cam_ife_mgr_acquire_get_unified_structure(
	struct cam_isp_acquire_hw_info *acquire_hw_info,
	uint32_t offset, uint32_t *input_size,
	struct cam_isp_in_port_generic_info *in_port)
{
	uint32_t major_ver = 0, minor_ver = 0;

	if (acquire_hw_info == NULL || input_size == NULL)
		return -EINVAL;

	major_ver = (acquire_hw_info->common_info_version >> 12) & 0xF;
	minor_ver = (acquire_hw_info->common_info_version) & 0xFFF;

	switch (major_ver) {
	case 1:
		return cam_ife_mgr_acquire_get_unified_structure_v0(
			acquire_hw_info, offset, input_size, in_port);
	case 2:
		return cam_ife_mgr_acquire_get_unified_structure_v2(
			acquire_hw_info, offset, input_size, in_port);
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid ver of i/p port info from user");
		return -EINVAL;
	}

	return 0;
}

/* entry function: acquire_hw */
static int cam_ife_mgr_acquire_hw(void *hw_mgr_priv, void *acquire_hw_args)
{
	struct cam_ife_hw_mgr *ife_hw_mgr            = hw_mgr_priv;
	struct cam_hw_acquire_args *acquire_args     = acquire_hw_args;
	int rc                                       = -1;
	int i, j;
	struct cam_ife_hw_mgr_ctx           *ife_ctx;
	struct cam_isp_in_port_generic_info *in_port = NULL;
	struct cam_cdm_acquire_data        cdm_acquire;
	uint32_t                           total_pix_port = 0;
	uint32_t                           total_rdi_port = 0;
	uint32_t                           total_pd_port = 0;
	uint32_t                           total_lite_port = 0;
	struct cam_isp_acquire_hw_info    *acquire_hw_info = NULL;
	uint32_t                           input_size = 0;

	CAM_DBG(CAM_ISP, "Enter...");

	if (!acquire_args || acquire_args->num_acq <= 0) {
		CAM_ERR(CAM_ISP, "Nothing to acquire. Seems like error");
		return -EINVAL;
	}

	/* get the ife ctx */
	rc = cam_ife_hw_mgr_get_ctx(&ife_hw_mgr->free_ctx_list, &ife_ctx);
	if (rc || !ife_ctx) {
		CAM_ERR(CAM_ISP, "Get ife hw context failed");
		goto err;
	}

	cam_cpas_get_cpas_hw_version(&ife_ctx->hw_version);
	ife_ctx->ctx_config = 0;
	ife_ctx->cdm_handle = 0;
	ife_ctx->ctx_type = CAM_IFE_CTX_TYPE_NONE;
	ife_ctx->num_acq_vfe_out = 0;
	ife_ctx->num_acq_sfe_out = 0;

	ife_ctx->common.cb_priv = acquire_args->context_data;
	ife_ctx->common.mini_dump_cb = acquire_args->mini_dump_cb;
	ife_ctx->flags.internal_cdm = false;
	ife_ctx->common.event_cb = acquire_args->event_cb;
	ife_ctx->hw_mgr = ife_hw_mgr;
	ife_ctx->cdm_ops =  cam_cdm_publish_ops();

	acquire_hw_info =
		(struct cam_isp_acquire_hw_info *)acquire_args->acquire_info;

	rc = cam_ife_mgr_check_and_update_fe(ife_ctx, acquire_hw_info,
		acquire_args->acquire_info_size);
	if (rc) {
		CAM_ERR(CAM_ISP, "buffer size is not enough");
		goto free_ctx;
	}

	in_port = kcalloc(acquire_hw_info->num_inputs,
			sizeof(struct cam_isp_in_port_generic_info),
			GFP_KERNEL);

	if (!in_port) {
		CAM_ERR(CAM_ISP, "No memory available");
		rc = -ENOMEM;
		goto free_ctx;
	}

	/* Update in_port structure */
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {
		rc = cam_ife_mgr_acquire_get_unified_structure(acquire_hw_info,
			i, &input_size, &in_port[i]);

		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Failed in parsing: %d", rc);
			goto free_mem;
		}

		cam_ife_hw_mgr_preprocess_port(ife_ctx, &in_port[i]);
		total_pix_port += in_port[i].ipp_count +
					in_port[i].ife_rd_count +
					in_port[i].lcr_count;
		total_rdi_port += in_port[i].rdi_count;
		total_pd_port += in_port[i].ppp_count;
		total_lite_port += in_port[i].lite_path_count;
	}

	/* Check whether context has only RDI resource */
	if (!total_pix_port && !total_pd_port) {
		ife_ctx->flags.is_rdi_only_context = true;
		CAM_DBG(CAM_ISP, "RDI only context");
	}

	/* Check if all output ports are of lite  */
	if (total_lite_port == total_pix_port + total_rdi_port)
		ife_ctx->flags.is_lite_context = true;

	/* acquire HW resources */
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {
		CAM_DBG(CAM_ISP, "in_res_type %x", in_port[i].res_type);
		if (!ife_ctx->ctx_type) {
			if (in_port[i].cust_node) {
				ife_ctx->ctx_type = CAM_IFE_CTX_TYPE_CUSTOM;
				/* These can be obtained from uapi */
				ife_ctx->ctx_config |=
					CAM_IFE_CTX_CFG_FRAME_HEADER_TS;
				ife_ctx->ctx_config |=
					CAM_IFE_CTX_CFG_SW_SYNC_ON;
			} else if (in_port[i].sfe_in_path_type) {
				ife_ctx->ctx_type = CAM_IFE_CTX_TYPE_SFE;
			}
		}

		CAM_DBG(CAM_ISP,
			"in_res_type: 0x%x sfe_in_path_type: 0x%x sfe_ife_enable: 0x%x",
			in_port[i].res_type, in_port[i].sfe_in_path_type,
			in_port[i].sfe_ife_enable);

		if (ife_ctx->flags.is_offline)
			rc = cam_ife_mgr_acquire_hw_for_offline_ctx(
				ife_ctx, &in_port[i],
				&acquire_args->acquired_hw_id[i],
				acquire_args->acquired_hw_path[i]);
		else
			rc = cam_ife_mgr_acquire_hw_for_ctx(ife_ctx,
				&in_port[i],
				&acquire_args->acquired_hw_id[i],
				acquire_args->acquired_hw_path[i]);

		if (rc) {
			cam_ife_hw_mgr_print_acquire_info(ife_ctx,
				(in_port[i].ipp_count +
				in_port[i].ife_rd_count +
				in_port[i].lcr_count),
				in_port[i].ppp_count,
				in_port[i].rdi_count, rc);
			goto free_res;
		}

		kfree(in_port[i].data);
		in_port[i].data = NULL;
	}

	kfree(in_port);
	in_port = NULL;

	/* Process base info */
	rc = cam_ife_mgr_process_base_info(ife_ctx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Process base info failed");
		goto free_res;
	}

	rc = cam_ife_mgr_allocate_cdm_cmd(
		(ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE ? true : false),
		&ife_ctx->cdm_cmd);
	if (rc)
		goto free_res;

	if (ife_ctx->flags.is_dual)
		memcpy(cdm_acquire.identifier, "dualife", sizeof("dualife"));
	else
		memcpy(cdm_acquire.identifier, "ife", sizeof("ife"));

	if (ife_ctx->flags.is_dual)
		cdm_acquire.cell_index = ife_ctx->left_hw_idx;
	else
		cdm_acquire.cell_index = ife_ctx->base[0].idx;
	cdm_acquire.handle = 0;
	cdm_acquire.userdata = ife_ctx;
	cdm_acquire.base_array_cnt = CAM_IFE_HW_NUM_MAX;
	for (i = 0, j = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (ife_hw_mgr->cdm_reg_map[i])
			cdm_acquire.base_array[j++] =
				ife_hw_mgr->cdm_reg_map[i];
	}
	cdm_acquire.base_array_cnt = j;
	cdm_acquire.priority = CAM_CDM_BL_FIFO_0;
	cdm_acquire.id = CAM_CDM_VIRTUAL;
	cdm_acquire.cam_cdm_callback = cam_ife_cam_cdm_callback;
	rc = cam_cdm_acquire(&cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to acquire the CDM HW");
		goto free_cdm_cmd;
	}

	CAM_DBG(CAM_ISP,
		"Successfully acquired CDM Id: %d, CDM HW hdl=%x, is_dual=%d",
		cdm_acquire.id, cdm_acquire.handle, ife_ctx->flags.is_dual);
	ife_ctx->cdm_handle = cdm_acquire.handle;
	ife_ctx->cdm_id = cdm_acquire.id;
	if (cdm_acquire.id == CAM_CDM_IFE)
		ife_ctx->flags.internal_cdm = true;
	atomic_set(&ife_ctx->cdm_done, 1);
	ife_ctx->last_cdm_done_req = 0;

	if (g_ife_hw_mgr.isp_bus_caps.support_consumed_addr)
		acquire_args->op_flags |=
			CAM_IFE_CTX_CONSUME_ADDR_EN;

	if ((ife_ctx->flags.is_sfe_shdr) ||
		(ife_ctx->flags.is_sfe_fs)) {
		acquire_args->op_flags |=
			CAM_IFE_CTX_APPLY_DEFAULT_CFG;
		ife_ctx->sfe_info.scratch_config =
			kzalloc(sizeof(struct cam_sfe_scratch_buf_cfg), GFP_KERNEL);
		if (!ife_ctx->sfe_info.scratch_config) {
			CAM_ERR(CAM_ISP, "Failed to allocate scratch config");
			rc = -ENOMEM;
			goto free_cdm_cmd;
		}
		/* Set scratch by default at stream on */
		ife_ctx->sfe_info.skip_scratch_cfg_streamon = false;
	}

	acquire_args->ctxt_to_hw_map = ife_ctx;
	if (ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_CUSTOM)
		acquire_args->op_flags |= CAM_IFE_CTX_CUSTOM_EN;

	if (ife_ctx->ctx_config &
		CAM_IFE_CTX_CFG_FRAME_HEADER_TS)
		acquire_args->op_flags |=
			CAM_IFE_CTX_FRAME_HEADER_EN;

	if (ife_ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE)
		acquire_args->op_flags |=
			CAM_IFE_CTX_SFE_EN;

	if (ife_ctx->flags.is_aeb_mode)
		acquire_args->op_flags |= CAM_IFE_CTX_AEB_EN;

	ife_ctx->flags.ctx_in_use = true;
	ife_ctx->num_reg_dump_buf = 0;

	acquire_args->valid_acquired_hw =
		acquire_hw_info->num_inputs;
	acquire_args->op_params.num_valid_params = 2;
	acquire_args->op_params.param_list[0] = max_ife_out_res;
	acquire_args->op_params.param_list[1] =
		ife_hw_mgr->isp_bus_caps.max_sfe_out_res_type;

	cam_ife_hw_mgr_print_acquire_info(ife_ctx, total_pix_port,
		total_pd_port, total_rdi_port, rc);

	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->used_ctx_list, &ife_ctx);

	return 0;

free_cdm_cmd:
	cam_ife_mgr_free_cdm_cmd(&ife_ctx->cdm_cmd);
free_res:
	cam_ife_hw_mgr_release_hw_for_ctx(ife_ctx);
free_mem:
	if (in_port) {
		for (i = 0; i < acquire_hw_info->num_inputs; i++) {
			kfree(in_port[i].data);
			in_port[i].data = NULL;
		}

		kfree(in_port);
		in_port = NULL;
	}
free_ctx:
	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->free_ctx_list, &ife_ctx);
err:
	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

void cam_ife_mgr_acquire_get_unified_dev_str(struct cam_isp_in_port_info *in,
	struct cam_isp_in_port_generic_info *gen_port_info)
{
	int i;

	gen_port_info->res_type        =  in->res_type;
	gen_port_info->lane_type       =  in->lane_type;
	gen_port_info->lane_num        =  in->lane_num;
	gen_port_info->lane_cfg        =  in->lane_cfg;
	gen_port_info->vc[0]           =  in->vc;
	gen_port_info->dt[0]           =  in->dt;
	gen_port_info->num_valid_vc_dt = 1;
	gen_port_info->format[0]       =  in->format;
	gen_port_info->test_pattern    =  in->test_pattern;
	gen_port_info->usage_type      =  in->usage_type;
	gen_port_info->left_start      =  in->left_start;
	gen_port_info->left_stop       =  in->left_stop;
	gen_port_info->left_width      =  in->left_width;
	gen_port_info->right_start     =  in->right_start;
	gen_port_info->right_stop      =  in->right_stop;
	gen_port_info->right_width     =  in->right_width;
	gen_port_info->line_start      =  in->line_start;
	gen_port_info->line_stop       =  in->line_stop;
	gen_port_info->height          =  in->height;
	gen_port_info->pixel_clk       =  in->pixel_clk;
	gen_port_info->batch_size      =  in->batch_size;
	gen_port_info->dsp_mode        =  in->dsp_mode;
	gen_port_info->hbi_cnt         =  in->hbi_cnt;
	gen_port_info->fe_unpacker_fmt =  in->format;
	gen_port_info->cust_node       =  0;
	gen_port_info->num_out_res     =  in->num_out_res;

	for (i = 0; i < in->num_out_res; i++) {
		gen_port_info->data[i].res_type     = in->data[i].res_type;
		gen_port_info->data[i].format       = in->data[i].format;
		gen_port_info->data[i].width        = in->data[i].width;
		gen_port_info->data[i].height       = in->data[i].height;
		gen_port_info->data[i].comp_grp_id  = in->data[i].comp_grp_id;
		gen_port_info->data[i].split_point  = in->data[i].split_point;
		gen_port_info->data[i].secure_mode  = in->data[i].secure_mode;
	}
}

/* entry function: acquire_hw */
static int cam_ife_mgr_acquire_dev(void *hw_mgr_priv, void *acquire_hw_args)
{
	struct cam_ife_hw_mgr *ife_hw_mgr            = hw_mgr_priv;
	struct cam_hw_acquire_args *acquire_args     = acquire_hw_args;
	int rc                                       = -1;
	int i, j;
	struct cam_ife_hw_mgr_ctx             *ife_ctx;
	struct cam_isp_in_port_info           *in_port = NULL;
	struct cam_isp_resource               *isp_resource = NULL;
	struct cam_cdm_acquire_data            cdm_acquire;
	struct cam_isp_in_port_generic_info   *gen_port_info = NULL;
	uint32_t                               total_pd_port = 0;
	uint32_t                               total_pix_port = 0;
	uint32_t                               total_rdi_port = 0;
	uint32_t                               in_port_length = 0;

	CAM_DBG(CAM_ISP, "Enter...");

	if (!acquire_args || acquire_args->num_acq <= 0) {
		CAM_ERR(CAM_ISP, "Nothing to acquire. Seems like error");
		return -EINVAL;
	}

	/* get the ife ctx */
	rc = cam_ife_hw_mgr_get_ctx(&ife_hw_mgr->free_ctx_list, &ife_ctx);
	if (rc || !ife_ctx) {
		CAM_ERR(CAM_ISP, "Get ife hw context failed");
		goto err;
	}

	ife_ctx->cdm_handle = 0;
	ife_ctx->common.cb_priv = acquire_args->context_data;
	ife_ctx->common.event_cb = acquire_args->event_cb;

	ife_ctx->hw_mgr = ife_hw_mgr;
	ife_ctx->cdm_ops = cam_cdm_publish_ops();

	isp_resource = (struct cam_isp_resource *)acquire_args->acquire_info;

	gen_port_info = kcalloc(acquire_args->num_acq,
			    sizeof(struct cam_isp_in_port_generic_info),
			    GFP_KERNEL);

	if (!gen_port_info) {
		CAM_ERR(CAM_ISP, "No memory available");
		rc = -ENOMEM;
		goto err;
	}

	for (i = 0; i < acquire_args->num_acq; i++) {
		if (isp_resource[i].resource_id != CAM_ISP_RES_ID_PORT)
			continue;

		CAM_DBG(CAM_ISP, "acquire no = %d total = %d", i,
			acquire_args->num_acq);
		CAM_DBG(CAM_ISP,
			"start copy from user handle %lld with len = %d",
			isp_resource[i].res_hdl,
			isp_resource[i].length);

		in_port_length = sizeof(struct cam_isp_in_port_info);

		if (in_port_length > isp_resource[i].length) {
			CAM_ERR(CAM_ISP, "buffer size is not enough");
			rc = -EINVAL;
			goto free_res;
		}

		in_port = memdup_user(
			u64_to_user_ptr(isp_resource[i].res_hdl),
			isp_resource[i].length);
		if (!IS_ERR(in_port)) {
			if (in_port->num_out_res > max_ife_out_res) {
				CAM_ERR(CAM_ISP, "too many output res %d",
					in_port->num_out_res);
				rc = -EINVAL;
				kfree(in_port);
				goto free_res;
			}

			in_port_length = sizeof(struct cam_isp_in_port_info) +
				(in_port->num_out_res - 1) *
				sizeof(struct cam_isp_out_port_info);
			if (in_port_length > isp_resource[i].length) {
				CAM_ERR(CAM_ISP, "buffer size is not enough");
				rc = -EINVAL;
				kfree(in_port);
				goto free_res;
			}

			gen_port_info[i].data = kcalloc(
				in_port->num_out_res,
				sizeof(struct cam_isp_out_port_generic_info),
				GFP_KERNEL);
			if (gen_port_info[i].data == NULL) {
				rc = -ENOMEM;
				goto free_res;
			}

			cam_ife_mgr_acquire_get_unified_dev_str(in_port,
				&gen_port_info[i]);
			cam_ife_hw_mgr_preprocess_port(ife_ctx,
				&gen_port_info[i]);

			total_pix_port += gen_port_info[i].ipp_count +
						gen_port_info[i].ife_rd_count +
						gen_port_info[i].lcr_count;
			total_rdi_port += gen_port_info[i].rdi_count;
			total_pd_port += gen_port_info[i].ppp_count;

			kfree(in_port);
		} else {
			CAM_ERR(CAM_ISP,
				"Copy from user failed with in_port = %pK",
				in_port);
			rc = -EFAULT;
			goto free_mem;
		}
	}

	/* Check whether context has only RDI resource */
	if (!total_pix_port || !total_pd_port) {
		ife_ctx->flags.is_rdi_only_context = true;
		CAM_DBG(CAM_ISP, "RDI only context");
	}

	/* acquire HW resources */
	for (i = 0; i < acquire_args->num_acq; i++) {
		if (isp_resource[i].resource_id != CAM_ISP_RES_ID_PORT)
			continue;

		rc = cam_ife_mgr_acquire_hw_for_ctx(ife_ctx,
			&gen_port_info[i],
			&acquire_args->acquired_hw_id[i],
			acquire_args->acquired_hw_path[i]);

		if (rc) {
			cam_ife_hw_mgr_print_acquire_info(ife_ctx,
				total_pix_port, total_pd_port,
				total_rdi_port, rc);
			goto free_res;
		}

		kfree(gen_port_info[i].data);
		gen_port_info[i].data = NULL;
	}

	kfree(gen_port_info);
	gen_port_info = NULL;

	/* Process base info */
	rc = cam_ife_mgr_process_base_info(ife_ctx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Process base info failed");
		goto free_res;
	}

	rc = cam_ife_mgr_allocate_cdm_cmd(false,
		&ife_ctx->cdm_cmd);
	if (rc)
		goto free_res;

	cam_cpas_get_cpas_hw_version(&ife_ctx->hw_version);
	ife_ctx->flags.internal_cdm = false;

	if (ife_ctx->flags.is_dual)
		memcpy(cdm_acquire.identifier, "dualife", sizeof("dualife"));
	else
		memcpy(cdm_acquire.identifier, "ife", sizeof("ife"));
	cdm_acquire.cell_index = ife_ctx->base[0].idx;
	cdm_acquire.handle = 0;
	cdm_acquire.userdata = ife_ctx;
	cdm_acquire.base_array_cnt = CAM_IFE_HW_NUM_MAX;
	for (i = 0, j = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (ife_hw_mgr->cdm_reg_map[i])
			cdm_acquire.base_array[j++] =
				ife_hw_mgr->cdm_reg_map[i];
	}
	cdm_acquire.base_array_cnt = j;
	cdm_acquire.priority = CAM_CDM_BL_FIFO_0;
	cdm_acquire.id = CAM_CDM_VIRTUAL;
	cdm_acquire.cam_cdm_callback = cam_ife_cam_cdm_callback;
	rc = cam_cdm_acquire(&cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to acquire the CDM HW");
		goto free_cdm_cmd;
	}

	CAM_DBG(CAM_ISP, "Successfully acquired CDM ID:%d, CDM HW hdl=%x",
		cdm_acquire.id, cdm_acquire.handle);

	if (cdm_acquire.id == CAM_CDM_IFE)
		ife_ctx->flags.internal_cdm = true;
	ife_ctx->cdm_handle = cdm_acquire.handle;
	ife_ctx->cdm_id = cdm_acquire.id;
	atomic_set(&ife_ctx->cdm_done, 1);
	ife_ctx->last_cdm_done_req = 0;

	acquire_args->ctxt_to_hw_map = ife_ctx;
	ife_ctx->flags.ctx_in_use = true;
	ife_ctx->num_reg_dump_buf = 0;

	cam_ife_hw_mgr_print_acquire_info(ife_ctx, total_pix_port,
		total_pd_port, total_rdi_port, rc);

	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->used_ctx_list, &ife_ctx);

	return 0;

free_cdm_cmd:
	cam_ife_mgr_free_cdm_cmd(&ife_ctx->cdm_cmd);
free_res:
	cam_ife_hw_mgr_release_hw_for_ctx(ife_ctx);
	cam_cdm_release(ife_ctx->cdm_handle);
	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->free_ctx_list, &ife_ctx);
free_mem:
	if (gen_port_info) {
		for (i = 0; i < acquire_args->num_acq; i++) {
			kfree(gen_port_info[i].data);
			gen_port_info[i].data = NULL;
		}
		kfree(gen_port_info);
		gen_port_info = NULL;
	}
err:
	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

/* entry function: acquire_hw */
static int cam_ife_mgr_acquire(void *hw_mgr_priv,
					void *acquire_hw_args)
{
	struct cam_hw_acquire_args *acquire_args     = acquire_hw_args;
	int rc                                       = -1;

	CAM_DBG(CAM_ISP, "Enter...");

	if (!acquire_args || acquire_args->num_acq <= 0) {
		CAM_ERR(CAM_ISP, "Nothing to acquire. Seems like error");
		return -EINVAL;
	}

	if (acquire_args->num_acq == CAM_API_COMPAT_CONSTANT)
		rc = cam_ife_mgr_acquire_hw(hw_mgr_priv, acquire_hw_args);
	else
		rc = cam_ife_mgr_acquire_dev(hw_mgr_priv, acquire_hw_args);

	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

static const char *cam_isp_util_usage_data_to_string(
	uint32_t usage_data)
{
	switch (usage_data) {
	case CAM_ISP_USAGE_LEFT_PX:
		return "LEFT_PX";
	case CAM_ISP_USAGE_RIGHT_PX:
		return "RIGHT_PX";
	case CAM_ISP_USAGE_RDI:
		return "RDI";
	case CAM_ISP_USAGE_SFE_LEFT:
		return "SFE_LEFT_PX";
	case CAM_ISP_USAGE_SFE_RIGHT:
		return "SFE_RIGHT_PX";
	case CAM_ISP_USAGE_SFE_RDI:
		return "SFE_RDI";
	default:
		return "USAGE_INVALID";
	}
}

static void cam_ife_mgr_print_blob_info(struct cam_ife_hw_mgr_ctx *ctx, uint64_t request_id,
	struct cam_isp_prepare_hw_update_data *hw_update_data)
{
	int i;
	struct cam_isp_bw_config_v2 *bw_config =
		(struct cam_isp_bw_config_v2 *) &hw_update_data->bw_clk_config.bw_config_v2;
	struct cam_isp_clock_config *ife_clock_config =
		(struct cam_isp_clock_config *) &hw_update_data->bw_clk_config.ife_clock_config;
	struct cam_isp_clock_config *sfe_clock_config =
		(struct cam_isp_clock_config *) &hw_update_data->bw_clk_config.sfe_clock_config;

	CAM_INFO(CAM_ISP, "ctx: %d req_id:%llu config_valid[BW VFE_CLK SFE_CLK]:[%d %d %d]",
		ctx->ctx_index, request_id, hw_update_data->bw_clk_config.bw_config_valid,
		hw_update_data->bw_clk_config.ife_clock_config_valid,
		hw_update_data->bw_clk_config.sfe_clock_config_valid);

	if (!hw_update_data->bw_clk_config.bw_config_valid)
		goto ife_clk;

	for (i = 0; i < bw_config->num_paths; i++) {
		CAM_INFO(CAM_PERF,
			"ISP_BLOB usage_type=%u [%s] [%s] [%s] [%llu] [%llu] [%llu]",
			bw_config->usage_type,
			cam_isp_util_usage_data_to_string(
			bw_config->axi_path[i].usage_data),
			cam_cpas_axi_util_path_type_to_string(
			bw_config->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			bw_config->axi_path[i].transac_type),
			bw_config->axi_path[i].camnoc_bw,
			bw_config->axi_path[i].mnoc_ab_bw,
			bw_config->axi_path[i].mnoc_ib_bw);
	}

ife_clk:
	if (!hw_update_data->bw_clk_config.ife_clock_config_valid)
		goto sfe_clk;

	CAM_INFO(CAM_PERF, "IFE clk update usage=%u left_clk= %lu right_clk=%lu",
		ife_clock_config->usage_type, ife_clock_config->left_pix_hz,
		ife_clock_config->right_pix_hz);

sfe_clk:
	if (!hw_update_data->bw_clk_config.sfe_clock_config_valid)
		goto end;

	CAM_INFO(CAM_PERF, "SFE clk update usage: %u left_clk: %lu right_clk: %lu",
		sfe_clock_config->usage_type, sfe_clock_config->left_pix_hz,
		sfe_clock_config->right_pix_hz);

end:
	return;
}

static int cam_isp_classify_vote_info(
	struct cam_isp_hw_mgr_res            *hw_mgr_res,
	struct cam_isp_bw_config_v2          *bw_config,
	struct cam_axi_vote                  *isp_vote,
	uint32_t                              hw_type,
	uint32_t                              split_idx,
	bool                                 *nrdi_l_bw_updated,
	bool                                 *nrdi_r_bw_updated)
{
	int                                   rc = 0, i, j = 0;

	if (hw_type == CAM_ISP_HW_TYPE_VFE) {
		if ((hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF)
			|| (hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_RD) ||
			(hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_PDLIB) ||
			(hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_LCR)) {
			if (split_idx == CAM_ISP_HW_SPLIT_LEFT) {
				if (*nrdi_l_bw_updated)
					return rc;

				for (i = 0; i < bw_config->num_paths; i++) {
					if (bw_config->axi_path[i].usage_data ==
						CAM_ISP_USAGE_LEFT_PX) {
						memcpy(&isp_vote->axi_path[j],
							&bw_config->axi_path[i],
							sizeof(struct
							cam_axi_per_path_bw_vote));
						j++;
					}
				}
				isp_vote->num_paths = j;

				*nrdi_l_bw_updated = true;
			} else {
				if (*nrdi_r_bw_updated)
					return rc;

				for (i = 0; i < bw_config->num_paths; i++) {
					if (bw_config->axi_path[i].usage_data ==
						CAM_ISP_USAGE_RIGHT_PX) {
						memcpy(&isp_vote->axi_path[j],
							&bw_config->axi_path[i],
							sizeof(struct
							cam_axi_per_path_bw_vote));
						j++;
					}
				}
				isp_vote->num_paths = j;

				*nrdi_r_bw_updated = true;
			}
		} else if ((hw_mgr_res->res_id >= CAM_ISP_HW_VFE_IN_RDI0)
			&& (hw_mgr_res->res_id <=
			CAM_ISP_HW_VFE_IN_RDI3)) {
			for (i = 0; i < bw_config->num_paths; i++) {
				if ((bw_config->axi_path[i].usage_data ==
					CAM_ISP_USAGE_RDI) &&
					((bw_config->axi_path[i].path_data_type -
					CAM_AXI_PATH_DATA_IFE_RDI0) ==
					(hw_mgr_res->res_id -
					CAM_ISP_HW_VFE_IN_RDI0))) {
					memcpy(&isp_vote->axi_path[j],
						&bw_config->axi_path[i],
						sizeof(struct
						cam_axi_per_path_bw_vote));
					j++;
				}
			}
			isp_vote->num_paths = j;

		} else {
			if (hw_mgr_res->hw_res[split_idx]) {
				CAM_ERR(CAM_ISP, "Invalid res_id %u, split_idx: %u",
					hw_mgr_res->res_id, split_idx);
				rc = -EINVAL;
				return rc;
			}
		}
	} else {
		if (hw_mgr_res->res_id == CAM_ISP_HW_SFE_IN_PIX) {
			if (split_idx == CAM_ISP_HW_SPLIT_LEFT) {
				if (*nrdi_l_bw_updated)
					return rc;

				for (i = 0; i < bw_config->num_paths; i++) {
					if (bw_config->axi_path[i].usage_data ==
						CAM_ISP_USAGE_SFE_LEFT) {
						memcpy(&isp_vote->axi_path[j],
							&bw_config->axi_path[i],
							sizeof(struct
							cam_axi_per_path_bw_vote));
						j++;
					}
				}
				isp_vote->num_paths = j;

				*nrdi_l_bw_updated = true;
			} else {
				if (*nrdi_r_bw_updated)
					return rc;

				for (i = 0; i < bw_config->num_paths; i++) {
					if (bw_config->axi_path[i].usage_data ==
						CAM_ISP_USAGE_SFE_RIGHT) {
						memcpy(&isp_vote->axi_path[j],
							&bw_config->axi_path[i],
							sizeof(struct
							cam_axi_per_path_bw_vote));
						j++;
					}
				}
				isp_vote->num_paths = j;

				*nrdi_r_bw_updated = true;
			}
		} else if ((hw_mgr_res->res_id >= CAM_ISP_HW_SFE_IN_RDI0)
			&& (hw_mgr_res->res_id <=
			CAM_ISP_HW_SFE_IN_RDI4)) {
			for (i = 0; i < bw_config->num_paths; i++) {
				if ((bw_config->axi_path[i].usage_data ==
					CAM_ISP_USAGE_SFE_RDI) &&
					((bw_config->axi_path[i].path_data_type -
					CAM_AXI_PATH_DATA_SFE_RDI0) ==
					(hw_mgr_res->res_id -
					CAM_ISP_HW_SFE_IN_RDI0))) {
					memcpy(&isp_vote->axi_path[j],
						&bw_config->axi_path[i],
						sizeof(struct
						cam_axi_per_path_bw_vote));
					j++;
				}
			}
			isp_vote->num_paths = j;

		} else {
			if (hw_mgr_res->hw_res[split_idx]) {
				CAM_ERR(CAM_ISP, "Invalid res_id %u, split_idx: %u",
					hw_mgr_res->res_id, split_idx);
				rc = -EINVAL;
				return rc;
			}
		}
	}

	for (i = 0; i < isp_vote->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"CLASSIFY_VOTE [%s] [%s] [%s] [%llu] [%llu] [%llu]",
			cam_isp_util_usage_data_to_string(
			isp_vote->axi_path[i].usage_data),
			cam_cpas_axi_util_path_type_to_string(
			isp_vote->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			isp_vote->axi_path[i].transac_type),
			isp_vote->axi_path[i].camnoc_bw,
			isp_vote->axi_path[i].mnoc_ab_bw,
			isp_vote->axi_path[i].mnoc_ib_bw);
	}

	return rc;
}

static int cam_isp_blob_bw_update_v2(
	struct cam_isp_bw_config_v2           *bw_config,
	struct cam_ife_hw_mgr_ctx             *ctx)
{
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_vfe_bw_update_args_v2       bw_upd_args;
	struct cam_sfe_bw_update_args          sfe_bw_update_args;
	int                                    rc = -EINVAL;
	uint32_t                               i, split_idx = INT_MIN;
	bool                                   nrdi_l_bw_updated = false;
	bool                                   nrdi_r_bw_updated = false;

	for (i = 0; i < bw_config->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"ISP_BLOB usage_type=%u [%s] [%s] [%s] [%llu] [%llu] [%llu]",
			bw_config->usage_type,
			cam_isp_util_usage_data_to_string(
			bw_config->axi_path[i].usage_data),
			cam_cpas_axi_util_path_type_to_string(
			bw_config->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			bw_config->axi_path[i].transac_type),
			bw_config->axi_path[i].camnoc_bw,
			bw_config->axi_path[i].mnoc_ab_bw,
			bw_config->axi_path[i].mnoc_ib_bw);
	}

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (split_idx = 0; split_idx < CAM_ISP_HW_SPLIT_MAX;
			split_idx++) {
			if (!hw_mgr_res->hw_res[split_idx])
				continue;

			memset(&bw_upd_args.isp_vote, 0,
				sizeof(struct cam_axi_vote));
			rc = cam_isp_classify_vote_info(hw_mgr_res, bw_config,
				&bw_upd_args.isp_vote, CAM_ISP_HW_TYPE_VFE,
				split_idx, &nrdi_l_bw_updated, &nrdi_r_bw_updated);
			if (rc)
				return rc;

			if (!bw_upd_args.isp_vote.num_paths)
				continue;

			hw_intf = hw_mgr_res->hw_res[split_idx]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				bw_upd_args.node_res =
					hw_mgr_res->hw_res[split_idx];

				/*
				 * Update BW values to top, actual apply to hw will happen when
				 * CAM_ISP_HW_CMD_APPLY_CLK_BW_UPDATE is called
				 */
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_BW_UPDATE_V2,
					&bw_upd_args,
					sizeof(
					struct cam_vfe_bw_update_args_v2));
				if (rc)
					CAM_ERR(CAM_PERF,
						"BW Update failed rc: %d", rc);
			} else {
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
			}
		}
	}

	nrdi_l_bw_updated = false;
	nrdi_r_bw_updated = false;
	list_for_each_entry(hw_mgr_res, &ctx->res_list_sfe_src, list) {
		for (split_idx = 0; split_idx < CAM_ISP_HW_SPLIT_MAX;
			split_idx++) {
			if (!hw_mgr_res->hw_res[split_idx])
				continue;

			memset(&sfe_bw_update_args.sfe_vote, 0,
				sizeof(struct cam_axi_vote));
			rc = cam_isp_classify_vote_info(hw_mgr_res, bw_config,
				&sfe_bw_update_args.sfe_vote, CAM_ISP_HW_TYPE_SFE,
				split_idx, &nrdi_l_bw_updated, &nrdi_r_bw_updated);
			if (rc)
				return rc;

			if (!sfe_bw_update_args.sfe_vote.num_paths)
				continue;

			hw_intf = hw_mgr_res->hw_res[split_idx]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				sfe_bw_update_args.node_res =
					hw_mgr_res->hw_res[split_idx];

				/*
				 * Update BW values to top, actual apply to hw will happen when
				 * CAM_ISP_HW_CMD_APPLY_CLK_BW_UPDATE is called
				 */
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_BW_UPDATE_V2,
					&sfe_bw_update_args,
					sizeof(
					struct cam_sfe_bw_update_args));
				if (rc)
					CAM_ERR(CAM_PERF,
						"BW Update failed rc: %d", rc);
			} else {
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
			}
		}
	}

	return rc;
}

static int cam_isp_blob_bw_update(
	struct cam_isp_bw_config              *bw_config,
	struct cam_ife_hw_mgr_ctx             *ctx)
{
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_vfe_bw_update_args          bw_upd_args;
	uint64_t                               cam_bw_bps = 0;
	uint64_t                               ext_bw_bps = 0;
	int                                    rc = -EINVAL;
	uint32_t                               i;
	bool                                   camif_l_bw_updated = false;
	bool                                   camif_r_bw_updated = false;

	CAM_DBG(CAM_PERF,
		"ISP_BLOB usage=%u left cam_bw_bps=%llu ext_bw_bps=%llu, right cam_bw_bps=%llu ext_bw_bps=%llu",
		bw_config->usage_type,
		bw_config->left_pix_vote.cam_bw_bps,
		bw_config->left_pix_vote.ext_bw_bps,
		bw_config->right_pix_vote.cam_bw_bps,
		bw_config->right_pix_vote.ext_bw_bps);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			if ((hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF) ||
				(hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_RD) ||
				(hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_PDLIB)
				|| (hw_mgr_res->res_id ==
				CAM_ISP_HW_VFE_IN_LCR))
				if (i == CAM_ISP_HW_SPLIT_LEFT) {
					if (camif_l_bw_updated)
						continue;

					cam_bw_bps =
					bw_config->left_pix_vote.cam_bw_bps;
					ext_bw_bps =
					bw_config->left_pix_vote.ext_bw_bps;

					camif_l_bw_updated = true;
				} else {
					if (camif_r_bw_updated)
						continue;

					cam_bw_bps =
					bw_config->right_pix_vote.cam_bw_bps;
					ext_bw_bps =
					bw_config->right_pix_vote.ext_bw_bps;

					camif_r_bw_updated = true;
				}
			else if ((hw_mgr_res->res_id >= CAM_ISP_HW_VFE_IN_RDI0)
					&& (hw_mgr_res->res_id <=
					CAM_ISP_HW_VFE_IN_RDI3)) {
				uint32_t idx = hw_mgr_res->res_id -
						CAM_ISP_HW_VFE_IN_RDI0;
				if (idx >= bw_config->num_rdi)
					continue;

				cam_bw_bps =
					bw_config->rdi_vote[idx].cam_bw_bps;
				ext_bw_bps =
					bw_config->rdi_vote[idx].ext_bw_bps;
			} else {
				if (hw_mgr_res->hw_res[i]) {
					CAM_ERR(CAM_ISP, "Invalid res_id %u",
						hw_mgr_res->res_id);
					rc = -EINVAL;
					return rc;
				}
			}

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				bw_upd_args.node_res =
					hw_mgr_res->hw_res[i];
				bw_upd_args.camnoc_bw_bytes = cam_bw_bps;
				bw_upd_args.external_bw_bytes = ext_bw_bps;

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_BW_UPDATE,
					&bw_upd_args,
					sizeof(struct cam_vfe_bw_update_args));
				if (rc)
					CAM_ERR(CAM_PERF, "BW Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

/* entry function: config_hw */
static int cam_ife_mgr_config_hw(void *hw_mgr_priv,
					void *config_hw_args)
{
	int rc = -1, i, skip = 0;
	struct cam_hw_config_args *cfg;
	struct cam_hw_update_entry *cmd;
	struct cam_cdm_bl_request *cdm_cmd;
	struct cam_ife_hw_mgr_ctx *ctx;
	struct cam_isp_prepare_hw_update_data *hw_update_data;
	unsigned long rem_jiffies = 0;
	bool cdm_hang_detect = false;

	if (!hw_mgr_priv || !config_hw_args) {
		CAM_ERR(CAM_ISP,
			"Invalid arguments, hw_mgr_priv=%pK, config_hw_args=%pK",
			hw_mgr_priv, config_hw_args);
		return -EINVAL;
	}

	cfg = config_hw_args;
	ctx = (struct cam_ife_hw_mgr_ctx *)cfg->ctxt_to_hw_map;
	if (!ctx) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EINVAL;
	}

	if (!ctx->flags.ctx_in_use || !ctx->cdm_cmd) {
		CAM_ERR(CAM_ISP,
			"Invalid context parameters : ctx_in_use=%d, cdm_cmd=%pK",
			ctx->flags.ctx_in_use, ctx->cdm_cmd);
		return -EPERM;
	}

	if (atomic_read(&ctx->overflow_pending)) {
		CAM_DBG(CAM_ISP,
			"Ctx[%pK][%d] Overflow pending, cannot apply req %llu",
			ctx, ctx->ctx_index, cfg->request_id);
		return -EPERM;
	}

	hw_update_data = (struct cam_isp_prepare_hw_update_data  *) cfg->priv;
	hw_update_data->isp_mgr_ctx = ctx;
	ctx->cdm_userdata.request_id = cfg->request_id;
	ctx->cdm_userdata.hw_update_data = hw_update_data;

	CAM_DBG(CAM_ISP, "Ctx[%pK][%d] : Applying Req %lld, init_packet=%d",
		ctx, ctx->ctx_index, cfg->request_id, cfg->init_packet);

	if (cfg->reapply_type && cfg->cdm_reset_before_apply) {
		if (ctx->last_cdm_done_req < cfg->request_id) {
			cdm_hang_detect =
				cam_cdm_detect_hang_error(ctx->cdm_handle, CAM_ISP);
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"CDM callback not received for req: %lld, last_cdm_done_req: %lld, cdm_hang_detect: %d",
				cfg->request_id, ctx->last_cdm_done_req,
				cdm_hang_detect);
			rc = cam_cdm_reset_hw(ctx->cdm_handle);
			if (rc) {
				CAM_ERR_RATE_LIMIT(CAM_ISP,
					"CDM reset unsuccessful for req: %lld. ctx: %d, rc: %d",
					cfg->request_id, ctx->ctx_index, rc);
				ctx->last_cdm_done_req = 0;
				return rc;
			}
		} else {
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"CDM callback received, should wait for buf done for req: %lld",
				cfg->request_id);
			return -EALREADY;
		}
		ctx->last_cdm_done_req = 0;
	}

	CAM_DBG(CAM_PERF,
		"ctx_idx=%d, bw_config_version=%d config_valid[BW VFE_CLK SFE_CLK]:[%d %d %d]",
		ctx->ctx_index, ctx->bw_config_version,
		hw_update_data->bw_clk_config.bw_config_valid,
		hw_update_data->bw_clk_config.ife_clock_config_valid,
		hw_update_data->bw_clk_config.sfe_clock_config_valid);

	/*
	 * Update clock and bw values to top layer, the actual application of these
	 * votes to hw will happen for all relevant hw indices at once, in a separate
	 * finish update call
	 */
	if (hw_update_data->bw_clk_config.ife_clock_config_valid) {
		rc = cam_isp_blob_ife_clock_update((struct cam_isp_clock_config *)
			&hw_update_data->bw_clk_config.ife_clock_config, ctx);
		if (rc) {
			CAM_ERR(CAM_PERF, "Clock Update Failed, rc=%d", rc);
			return rc;
		}
	}

	if (hw_update_data->bw_clk_config.sfe_clock_config_valid) {
		rc = cam_isp_blob_sfe_clock_update((struct cam_isp_clock_config *)
			&hw_update_data->bw_clk_config.sfe_clock_config, ctx);
		if (rc) {
			CAM_ERR(CAM_PERF, "Clock Update Failed, rc=%d", rc);
			return rc;
		}
	}

	if (hw_update_data->bw_clk_config.bw_config_valid) {
		if (ctx->bw_config_version == CAM_ISP_BW_CONFIG_V1) {
			rc = cam_isp_blob_bw_update(
				(struct cam_isp_bw_config *)
				&hw_update_data->bw_clk_config.bw_config, ctx);
			if (rc) {
				CAM_ERR(CAM_PERF, "Bandwidth Update Failed rc: %d", rc);
				return rc;
			}
		} else if (ctx->bw_config_version == CAM_ISP_BW_CONFIG_V2) {
			rc = cam_isp_blob_bw_update_v2((struct cam_isp_bw_config_v2 *)
				&hw_update_data->bw_clk_config.bw_config_v2, ctx);
			if (rc) {
				CAM_ERR(CAM_PERF, "Bandwidth Update Failed rc: %d", rc);
				return rc;
			}
		} else {
			CAM_ERR(CAM_PERF, "Invalid bw config version: %d", ctx->bw_config_version);
		}
	}

	/* Apply the updated values in top layer to the HW*/
	rc = cam_ife_mgr_finish_clk_bw_update(ctx, cfg->request_id, false);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed in finishing clk/bw update rc: %d", rc);
		cam_ife_mgr_print_blob_info(ctx, cfg->request_id, hw_update_data);
		return rc;
	}

	CAM_DBG(CAM_ISP,
		"Enter ctx id:%d num_hw_upd_entries %d request id: %llu",
		ctx->ctx_index, cfg->num_hw_update_entries, cfg->request_id);

	if (cfg->num_hw_update_entries > 0) {
		cdm_cmd = ctx->cdm_cmd;
		cdm_cmd->type = CAM_CDM_BL_CMD_TYPE_MEM_HANDLE;
		cdm_cmd->flag = true;
		cdm_cmd->userdata = ctx;
		cdm_cmd->cookie = cfg->request_id;
		cdm_cmd->gen_irq_arb = false;

		for (i = 0 ; i < cfg->num_hw_update_entries; i++) {
			cmd = (cfg->hw_update_entries + i);

			if ((cfg->reapply_type == CAM_CONFIG_REAPPLY_IO) &&
				(cmd->flags == CAM_ISP_IQ_BL)) {
				skip++;
				continue;
			}

			if ((cfg->reapply_type == CAM_CONFIG_REAPPLY_IQ) &&
				(cmd->flags == CAM_ISP_IOCFG_BL)) {
				skip++;
				continue;
			}

			if (cmd->flags == CAM_ISP_UNUSED_BL ||
				cmd->flags >= CAM_ISP_BL_MAX)
				CAM_ERR(CAM_ISP, "Unexpected BL type %d",
					cmd->flags);

			cdm_cmd->cmd[i - skip].bl_addr.mem_handle = cmd->handle;
			cdm_cmd->cmd[i - skip].offset = cmd->offset;
			cdm_cmd->cmd[i - skip].len = cmd->len;
			cdm_cmd->cmd[i - skip].arbitrate = false;
		}
		cdm_cmd->cmd_arrary_count = cfg->num_hw_update_entries - skip;

		if (cam_presil_mode_enabled()) {
			CAM_INFO(CAM_ISP, "Sending relevant buffers for request:%llu to presil",
				cfg->request_id);
			rc = cam_presil_send_buffers_from_packet(hw_update_data->packet,
				g_ife_hw_mgr.mgr_common.img_iommu_hdl,
				g_ife_hw_mgr.mgr_common.cmd_iommu_hdl);
			if (rc) {
				CAM_ERR(CAM_ISP, "Error sending buffers for request:%llu to presil",
					cfg->request_id);
				return rc;
			}
		}

		reinit_completion(&ctx->config_done_complete);
		ctx->applied_req_id = cfg->request_id;

		CAM_DBG(CAM_ISP, "Submit to CDM");
		atomic_set(&ctx->cdm_done, 0);
		rc = cam_cdm_submit_bls(ctx->cdm_handle, cdm_cmd);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed to apply the configs for req %llu, rc %d",
				cfg->request_id, rc);
			return rc;
		}

		if (cfg->init_packet || hw_update_data->mup_en ||
			(ctx->ctx_config & CAM_IFE_CTX_CFG_SW_SYNC_ON)) {
			rem_jiffies = cam_common_wait_for_completion_timeout(
				&ctx->config_done_complete,
				msecs_to_jiffies(60));
			if (rem_jiffies == 0) {
				CAM_ERR(CAM_ISP,
					"config done completion timeout for req_id=%llu ctx_index %d",
					cfg->request_id, ctx->ctx_index);
				rc = cam_cdm_detect_hang_error(ctx->cdm_handle, CAM_ISP);
				if (rc < 0) {
					cam_cdm_dump_debug_registers(
						ctx->cdm_handle);
					rc = -ETIMEDOUT;
				} else {
					CAM_DBG(CAM_ISP,
						"Wq delayed but IRQ CDM done");
				}
			} else {
				CAM_DBG(CAM_ISP,
					"config done Success for req_id=%llu ctx_index %d",
					cfg->request_id, ctx->ctx_index);
				/* Update last applied MUP */
				if (hw_update_data->mup_en) {
					ctx->current_mup = hw_update_data->mup_val;
					ctx->curr_num_exp = hw_update_data->num_exp;
				}
				hw_update_data->mup_en = false;
			}
		}
	} else {
		CAM_ERR(CAM_ISP, "No commands to config");
	}

	CAM_DBG(CAM_ISP, "Exit: Config Done: %llu",  cfg->request_id);
	return rc;
}

static int cam_ife_mgr_stop_hw_in_overflow(void *stop_hw_args)
{
	int                               rc        = 0;
	struct cam_hw_stop_args          *stop_args = stop_hw_args;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_ife_hw_mgr_ctx        *ctx;
	uint32_t                          i, master_base_idx = 0;

	if (!stop_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}
	ctx = (struct cam_ife_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
	if (!ctx || !ctx->flags.ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "Enter...ctx id:%d",
		ctx->ctx_index);

	if (!ctx->num_base) {
		CAM_ERR(CAM_ISP, "Number of bases are zero");
		return -EINVAL;
	}

	/* get master base index first */
	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].split_id == CAM_ISP_HW_SPLIT_LEFT) {
			master_base_idx = ctx->base[i].idx;
			break;
		}
	}

	if (i == ctx->num_base)
		master_base_idx = ctx->base[0].idx;

	/* stop the master CSID path first */
	cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_csid,
		master_base_idx, CAM_CSID_HALT_IMMEDIATELY);

	/* Stop rest of the CSID paths  */
	for (i = 0; i < ctx->num_base; i++) {
		if (i == master_base_idx)
			continue;

		cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_csid,
			ctx->base[i].idx, CAM_CSID_HALT_IMMEDIATELY);
	}

	/* IFE mux in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		cam_ife_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	/* IFE bus rd resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		cam_ife_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	/* IFE out resources */
	for (i = 0; i < max_ife_out_res; i++)
		cam_ife_hw_mgr_stop_hw_res(&ctx->res_list_ife_out[i]);


	/* Stop tasklet for context */
	cam_tasklet_stop(ctx->common.tasklet_info);
	CAM_DBG(CAM_ISP, "Exit...ctx id:%d rc :%d",
		ctx->ctx_index, rc);

	return rc;
}

static int cam_ife_mgr_bw_control(struct cam_ife_hw_mgr_ctx *ctx,
	enum cam_isp_bw_control_action action)
{
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_isp_bw_control_args         bw_ctrl_args;
	int                                    rc = -EINVAL;
	uint32_t                               i;

	CAM_DBG(CAM_ISP, "Enter...ctx id:%d", ctx->ctx_index);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				bw_ctrl_args.node_res =
					hw_mgr_res->hw_res[i];
				bw_ctrl_args.action = action;

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_BW_CONTROL,
					&bw_ctrl_args,
					sizeof(struct cam_isp_bw_control_args));
				if (rc)
					CAM_ERR(CAM_ISP, "BW Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	list_for_each_entry(hw_mgr_res, &ctx->res_list_sfe_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				bw_ctrl_args.node_res =
					hw_mgr_res->hw_res[i];
				bw_ctrl_args.action = action;

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_BW_CONTROL,
					&bw_ctrl_args,
					sizeof(struct cam_isp_bw_control_args));
				if (rc)
					CAM_ERR(CAM_ISP, "BW Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_ife_mgr_pause_hw(struct cam_ife_hw_mgr_ctx *ctx)
{
	return cam_ife_mgr_bw_control(ctx, CAM_ISP_BW_CONTROL_EXCLUDE);
}

/* entry function: stop_hw */
static int cam_ife_mgr_stop_hw(void *hw_mgr_priv, void *stop_hw_args)
{
	int                               rc        = 0;
	struct cam_hw_stop_args          *stop_args = stop_hw_args;
	struct cam_isp_stop_args         *stop_isp;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_ife_hw_mgr_ctx        *ctx;
	enum cam_ife_csid_halt_cmd        csid_halt_type;
	uint32_t                          i, master_base_idx = 0;
	unsigned long                     rem_jiffies = 0;

	if (!hw_mgr_priv || !stop_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
	if (!ctx || !ctx->flags.ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	if (!ctx->num_base) {
		CAM_ERR(CAM_ISP, "number of bases are zero");
		return -EINVAL;
	}

	/* Cancel all scheduled recoveries without affecting future recoveries */
	atomic_inc(&ctx->recovery_id);

	CAM_DBG(CAM_ISP, " Enter...ctx id:%d", ctx->ctx_index);
	stop_isp = (struct cam_isp_stop_args    *)stop_args->args;

	/* Set the csid halt command */
	if ((stop_isp->hw_stop_cmd == CAM_ISP_HW_STOP_AT_FRAME_BOUNDARY) ||
		ctx->flags.dsp_enabled)
		csid_halt_type = CAM_CSID_HALT_AT_FRAME_BOUNDARY;
	else
		csid_halt_type = CAM_CSID_HALT_IMMEDIATELY;

	/* Note:stop resource will remove the irq mask from the hardware */

	CAM_DBG(CAM_ISP, "Halting CSIDs");

	/* get master base index first */
	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].split_id == CAM_ISP_HW_SPLIT_LEFT) {
			master_base_idx = ctx->base[i].idx;
			break;
		}
	}

	/*
	 * If Context does not have PIX resources and has only RDI resource
	 * then take the first base index.
	 */
	if (i == ctx->num_base)
		master_base_idx = ctx->base[0].idx;

	/*Change slave mode*/
	if (csid_halt_type == CAM_CSID_HALT_IMMEDIATELY)
		cam_ife_mgr_csid_change_halt_mode(ctx,
			CAM_CSID_HALT_MODE_INTERNAL);


	CAM_DBG(CAM_ISP, "Stopping master CSID idx %d", master_base_idx);


	/* Stop the master CSID path first */
	cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_csid,
		master_base_idx, csid_halt_type);

	/* stop rest of the CSID paths  */
	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].idx == master_base_idx)
			continue;
		CAM_DBG(CAM_ISP, "Stopping CSID idx %d i %d master %d",
			ctx->base[i].idx, i, master_base_idx);

		cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_csid,
			ctx->base[i].idx, csid_halt_type);
	}

	/* Ensure HW layer does not reset any clk data since it's
	 * internal stream off/resume
	 */
	if (stop_isp->internal_trigger)
		cam_ife_mgr_finish_clk_bw_update(ctx, 0, true);

	/* check to avoid iterating loop */
	if (ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
		CAM_DBG(CAM_ISP, "Going to stop SFE Out");

		/* SFE out resources */
		for (i = 0; i < CAM_SFE_HW_OUT_RES_MAX; i++)
			cam_ife_hw_mgr_stop_hw_res(&ctx->res_list_sfe_out[i]);

		CAM_DBG(CAM_ISP, "Going to stop SFE SRC resources");

		/* SFE in resources */
		list_for_each_entry(hw_mgr_res, &ctx->res_list_sfe_src, list)
			cam_ife_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	CAM_DBG(CAM_ISP, "Going to stop IFE out resources");

	/* IFE out resources */
	for (i = 0; i < max_ife_out_res; i++)
		cam_ife_hw_mgr_stop_hw_res(&ctx->res_list_ife_out[i]);

	CAM_DBG(CAM_ISP, "Going to stop IFE Mux");

	/* IFE mux in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		cam_ife_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	/* bus rd resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		cam_ife_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	cam_tasklet_stop(ctx->common.tasklet_info);

	/* reset scratch buffer/mup expect INIT again for UMD triggered stop/flush */
	if (!stop_isp->internal_trigger) {
		ctx->current_mup = 0;
		if (ctx->sfe_info.scratch_config)
			memset(ctx->sfe_info.scratch_config, 0,
				sizeof(struct cam_sfe_scratch_buf_cfg));
	}
	ctx->sfe_info.skip_scratch_cfg_streamon = false;

	cam_ife_mgr_pause_hw(ctx);

	rem_jiffies = cam_common_wait_for_completion_timeout(
		&ctx->config_done_complete,
		msecs_to_jiffies(10));
	if (rem_jiffies == 0)
		CAM_WARN(CAM_ISP,
			"config done completion timeout for last applied req_id=%llu ctx_index %",
			ctx->applied_req_id, ctx->ctx_index);

	/* Reset CDM for KMD internal stop */
	if (stop_isp->internal_trigger) {
		rc = cam_cdm_reset_hw(ctx->cdm_handle);
		if (rc) {
			CAM_WARN(CAM_ISP, "CDM: %u reset failed rc: %d in ctx: %u",
				ctx->cdm_id, rc, ctx->ctx_index);
			rc = 0;
		}
	}

	if (stop_isp->stop_only)
		goto end;

	if (cam_cdm_stream_off(ctx->cdm_handle))
		CAM_ERR(CAM_ISP, "CDM stream off failed %d", ctx->cdm_handle);


	cam_ife_hw_mgr_deinit_hw(ctx);
	CAM_DBG(CAM_ISP,
		"Stop success for ctx id:%d rc :%d", ctx->ctx_index, rc);

	mutex_lock(&g_ife_hw_mgr.ctx_mutex);
	if (!atomic_dec_return(&g_ife_hw_mgr.active_ctx_cnt)) {
		rc = cam_ife_notify_safe_lut_scm(CAM_IFE_SAFE_DISABLE);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"SAFE SCM call failed:Check TZ/HYP dependency");
			rc = 0;
		}
	}
	mutex_unlock(&g_ife_hw_mgr.ctx_mutex);

end:
	ctx->flags.dump_on_error = false;
	ctx->flags.dump_on_flush = false;
	return rc;
}

static int cam_ife_mgr_reset_vfe_hw(struct cam_ife_hw_mgr *hw_mgr,
	uint32_t hw_idx)
{
	uint32_t i = 0;
	struct cam_hw_intf             *vfe_hw_intf;
	uint32_t vfe_reset_type;

	if (!hw_mgr) {
		CAM_DBG(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}
	/* Reset VFE HW*/
	vfe_reset_type = CAM_VFE_HW_RESET_HW;

	for (i = 0; i < CAM_VFE_HW_NUM_MAX; i++) {
		if (!hw_mgr->ife_devices[i])
			continue;

		if (hw_idx != hw_mgr->ife_devices[i]->hw_intf->hw_idx)
			continue;
		CAM_DBG(CAM_ISP, "VFE (id = %d) reset", hw_idx);
		vfe_hw_intf = hw_mgr->ife_devices[i]->hw_intf;
		vfe_hw_intf->hw_ops.reset(vfe_hw_intf->hw_priv,
			&vfe_reset_type, sizeof(vfe_reset_type));
		break;
	}

	CAM_DBG(CAM_ISP, "Exit Successfully");
	return 0;
}

static int cam_ife_mgr_unmask_bus_wr_irq(struct cam_ife_hw_mgr *hw_mgr,
	uint32_t hw_idx)
{
	uint32_t i = 0, dummy_args = 0;
	struct cam_hw_intf *vfe_hw_intf;

	if (!hw_mgr) {
		CAM_DBG(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	for (i = 0; i < CAM_VFE_HW_NUM_MAX; i++) {
		if (!hw_mgr->ife_devices[i])
			continue;

		if (hw_idx != hw_mgr->ife_devices[i]->hw_intf->hw_idx)
			continue;

		CAM_DBG(CAM_ISP, "Unmask VFE:%d BUS_WR IRQ", hw_idx);

		vfe_hw_intf = hw_mgr->ife_devices[i]->hw_intf;

		vfe_hw_intf->hw_ops.process_cmd(vfe_hw_intf->hw_priv,
			CAM_ISP_HW_CMD_UNMASK_BUS_WR_IRQ,
			&dummy_args,
			sizeof(dummy_args));

		break;
	}

	return 0;
}

static int cam_ife_mgr_restart_hw(void *start_hw_args)
{
	int                               rc = -1;
	struct cam_hw_start_args         *start_args = start_hw_args;
	struct cam_ife_hw_mgr_ctx        *ctx;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	uint32_t                          i;

	if (!start_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)start_args->ctxt_to_hw_map;
	if (!ctx || !ctx->flags.ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "START IFE OUT ... in ctx id:%d", ctx->ctx_index);

	cam_tasklet_start(ctx->common.tasklet_info);

	/* start the IFE out devices */
	for (i = 0; i < max_ife_out_res; i++) {
		rc = cam_ife_hw_mgr_start_hw_res(
			&ctx->res_list_ife_out[i], ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE OUT (%d)", i);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START IFE SRC ... in ctx id:%d", ctx->ctx_index);

	/* Start IFE BUS RD device */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE BUS RD (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	/* Start the IFE mux in devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE MUX (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START CSID HW ... in ctx id:%d", ctx->ctx_index);
	/* Start the IFE CSID HW devices */
	cam_ife_mgr_csid_start_hw(ctx, CAM_IFE_PIX_PATH_RES_MAX);

	/* Start IFE root node: do nothing */
	CAM_DBG(CAM_ISP, "Exit...(success)");
	return 0;

err:
	cam_ife_mgr_stop_hw_in_overflow(start_hw_args);
	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

/* To find SFE core idx for CSID wrapper config */
static int cam_ife_mgr_find_sfe_core_idx(
	int split_id,
	struct cam_ife_hw_mgr_ctx *ctx,
	uint32_t *core_idx)
{
	int i;

	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].hw_type != CAM_ISP_HW_TYPE_SFE)
			continue;

		if (ctx->base[i].split_id == split_id) {
			CAM_DBG(CAM_ISP, "Found SFE core: %u for split_id: %d",
				ctx->base[i].idx, split_id);
			*core_idx = ctx->base[i].idx;
			goto end;
		}
	}

	CAM_ERR(CAM_ISP,
		"Failed to find SFE core idx for split_id %d",
		split_id);
	return -EINVAL;

end:
	return 0;
}

static int cam_ife_mgr_start_hw(void *hw_mgr_priv, void *start_hw_args)
{
	int                                  rc = -1;
	struct cam_isp_start_args           *start_isp = start_hw_args;
	struct cam_hw_stop_args              stop_args;
	struct cam_isp_stop_args             stop_isp;
	struct cam_ife_hw_mgr_ctx           *ctx;
	struct cam_isp_hw_mgr_res           *hw_mgr_res;
	struct cam_isp_resource_node        *rsrc_node = NULL;
	uint32_t                             i;
	uint32_t                             camif_debug;
	bool                                 res_rdi_context_set = false;
	uint32_t                             primary_rdi_src_res;
	uint32_t                             primary_rdi_out_res;
	uint32_t                             primary_rdi_csid_res;
	struct cam_ife_csid_top_config_args  csid_top_args = {0};
	struct cam_hw_intf                  *hw_intf;

	primary_rdi_src_res = CAM_ISP_HW_VFE_IN_MAX;
	primary_rdi_out_res = g_ife_hw_mgr.isp_bus_caps.max_vfe_out_res_type;
	primary_rdi_csid_res = CAM_IFE_PIX_PATH_RES_MAX;

	if (!hw_mgr_priv || !start_isp) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)
		start_isp->hw_config.ctxt_to_hw_map;
	if (!ctx || !ctx->flags.ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	if ((!ctx->flags.init_done) && start_isp->start_only) {
		CAM_ERR(CAM_ISP, "Invalid args init_done %d start_only %d",
			ctx->flags.init_done, start_isp->start_only);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Enter... ctx id:%d",
		ctx->ctx_index);

	/* update Bandwidth should be done at the hw layer */

	cam_tasklet_start(ctx->common.tasklet_info);

	if (ctx->flags.init_done && start_isp->start_only) {
		/* Unmask BUS_WR bit in VFE top */
		for (i = 0; i < ctx->num_base; i++) {
			rc = cam_ife_mgr_unmask_bus_wr_irq(hw_mgr_priv,
				ctx->base[i].idx);
			if (rc)
				CAM_ERR(CAM_ISP,
					"Failed to unmask VFE:%d BUS_WR IRQ rc:%d",
					ctx->base[i].idx, rc);
		}
		goto start_only;
	}

	/* set current csid debug information to CSID HW */
	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (g_ife_hw_mgr.csid_devices[i]) {
			rc = g_ife_hw_mgr.csid_devices[i]->hw_ops.process_cmd(
				g_ife_hw_mgr.csid_devices[i]->hw_priv,
				CAM_IFE_CSID_SET_CSID_DEBUG,
				&g_ife_hw_mgr.debug_cfg.csid_debug,
				sizeof(g_ife_hw_mgr.debug_cfg.csid_debug));
		}
	}

	/* set current SFE debug information to SFE HW */
	for (i = 0; i < CAM_SFE_HW_NUM_MAX; i++) {
		struct cam_sfe_debug_cfg_params debug_cfg;

		debug_cfg.cache_config = false;
		debug_cfg.u.dbg_cfg.sfe_debug_cfg = g_ife_hw_mgr.debug_cfg.sfe_debug;
		debug_cfg.u.dbg_cfg.sfe_sensor_sel = g_ife_hw_mgr.debug_cfg.sfe_sensor_diag_cfg;
		if (g_ife_hw_mgr.sfe_devices[i]) {
			rc = g_ife_hw_mgr.sfe_devices[i]->hw_intf->hw_ops.process_cmd(
				g_ife_hw_mgr.sfe_devices[i]->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_SET_SFE_DEBUG_CFG,
				&debug_cfg,
				sizeof(debug_cfg));
		}
	}

	/* set IFE bus WR MMU config */
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (g_ife_hw_mgr.ife_devices[i]) {
			rc = g_ife_hw_mgr.ife_devices[i]->hw_intf->hw_ops.process_cmd(
				g_ife_hw_mgr.ife_devices[i]->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_IFE_BUS_DEBUG_CFG,
				&g_ife_hw_mgr.debug_cfg.disable_ife_mmu_prefetch,
				sizeof(g_ife_hw_mgr.debug_cfg.disable_ife_mmu_prefetch));
			if (rc)
				CAM_DBG(CAM_ISP,
					"Failed to set IFE_%d bus wr debug cfg", i);
		}
	}

	if (ctx->flags.need_csid_top_cfg) {
		list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid,
				list) {
			for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
				if (!hw_mgr_res->hw_res[i])
					continue;

				/* Updated based on sfe context */
				if (ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
					csid_top_args.input_core_type =
						CAM_IFE_CSID_INPUT_CORE_SFE_IFE;
					rc = cam_ife_mgr_find_sfe_core_idx(
						i, ctx, &csid_top_args.core_idx);
					if (rc)
						goto tasklet_stop;
				} else {
					csid_top_args.input_core_type =
						CAM_IFE_CSID_INPUT_CORE_IFE;
				}

				if ((ctx->flags.is_offline) ||
					(ctx->flags.is_sfe_fs))
					csid_top_args.is_sfe_offline = true;

				hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_IFE_CSID_TOP_CONFIG,
					&csid_top_args,
					sizeof(csid_top_args));

				CAM_DBG(CAM_ISP,
					"CSID: %u split_id: %d core_idx: %u core_type: %u is_sfe_offline: %d",
					hw_intf->hw_idx, i, csid_top_args.core_idx,
					csid_top_args.input_core_type,
					csid_top_args.is_sfe_offline);
			}
		}
	}

	camif_debug = g_ife_hw_mgr.debug_cfg.camif_debug;
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			rsrc_node = hw_mgr_res->hw_res[i];
			if (rsrc_node->process_cmd && (rsrc_node->res_id ==
				CAM_ISP_HW_VFE_IN_CAMIF)) {
				rc = hw_mgr_res->hw_res[i]->process_cmd(
					hw_mgr_res->hw_res[i],
					CAM_ISP_HW_CMD_SET_CAMIF_DEBUG,
					&camif_debug,
					sizeof(camif_debug));
			}
		}
	}

	rc = cam_ife_hw_mgr_init_hw(ctx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Init failed");
		goto tasklet_stop;
	}

	ctx->flags.init_done = true;

	mutex_lock(&g_ife_hw_mgr.ctx_mutex);
	if (!atomic_fetch_inc(&g_ife_hw_mgr.active_ctx_cnt)) {
		rc = cam_ife_notify_safe_lut_scm(CAM_IFE_SAFE_ENABLE);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"SAFE SCM call failed:Check TZ/HYP dependency");
			rc = -EFAULT;
			goto deinit_hw;
		}
	}
	mutex_unlock(&g_ife_hw_mgr.ctx_mutex);

	rc = cam_cdm_stream_on(ctx->cdm_handle);
	if (rc) {
		CAM_ERR(CAM_ISP, "Can not start cdm (%d)", ctx->cdm_handle);
		goto safe_disable;
	}

start_only:

	atomic_set(&ctx->overflow_pending, 0);

	/* Apply initial configuration */
	CAM_DBG(CAM_ISP, "Config HW");
	rc = cam_ife_mgr_config_hw(hw_mgr_priv, &start_isp->hw_config);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Config HW failed, start_only=%d, rc=%d",
			start_isp->start_only, rc);
		goto cdm_streamoff;
	}

	CAM_DBG(CAM_ISP, "START IFE OUT ... in ctx id:%d",
		ctx->ctx_index);
	/* start the IFE out devices */
	for (i = 0; i < max_ife_out_res; i++) {
		hw_mgr_res = &ctx->res_list_ife_out[i];
		switch (hw_mgr_res->res_id) {
		case CAM_ISP_IFE_OUT_RES_RDI_0:
		case CAM_ISP_IFE_OUT_RES_RDI_1:
		case CAM_ISP_IFE_OUT_RES_RDI_2:
		case CAM_ISP_IFE_OUT_RES_RDI_3:
			if (!res_rdi_context_set && ctx->flags.is_rdi_only_context) {
				hw_mgr_res->hw_res[0]->rdi_only_ctx =
					ctx->flags.is_rdi_only_context;
				res_rdi_context_set = true;
				primary_rdi_out_res = hw_mgr_res->res_id;
			}
			break;
		default:
			break;
		}
		rc = cam_ife_hw_mgr_start_hw_res(
			&ctx->res_list_ife_out[i], ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE OUT (%d)",
				 i);
			goto err;
		}
	}

	if (primary_rdi_out_res < g_ife_hw_mgr.isp_bus_caps.max_vfe_out_res_type) {
		primary_rdi_src_res =
			cam_convert_rdi_out_res_id_to_src(primary_rdi_out_res);
		primary_rdi_csid_res =
			cam_ife_hw_mgr_get_ife_csid_rdi_res_type(
			primary_rdi_out_res);
	}

	CAM_DBG(CAM_ISP, "START IFE SRC ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the IFE mux in devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		if (primary_rdi_src_res == hw_mgr_res->res_id) {
			hw_mgr_res->hw_res[0]->rdi_only_ctx =
				ctx->flags.is_rdi_only_context;
		}
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE Mux (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	if (ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE) {
		CAM_DBG(CAM_ISP, "START SFE OUT ... in ctx id:%d",
			ctx->ctx_index);
		for (i = 0; i < CAM_SFE_HW_OUT_RES_MAX; i++) {
			hw_mgr_res = &ctx->res_list_sfe_out[i];
			rc = cam_ife_hw_mgr_start_hw_res(
				&ctx->res_list_sfe_out[i], ctx);
			if (rc) {
				CAM_ERR(CAM_ISP, "Can not start SFE OUT (%d)",
					i);
				goto err;
			}
		}

		CAM_DBG(CAM_ISP, "START SFE SRC RSRC ... in ctx id:%d",
			ctx->ctx_index);
		list_for_each_entry(hw_mgr_res, &ctx->res_list_sfe_src, list) {
			rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
			if (rc) {
				CAM_ERR(CAM_ISP, "Can not start SFE SRC (%d)",
					hw_mgr_res->res_id);
				goto err;
			}
		}
	}

	CAM_DBG(CAM_ISP, "START BUS RD ... in ctx id:%d",
		ctx->ctx_index);
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start BUS RD (%d)",
				hw_mgr_res->res_id);
			goto err;
		}
	}

	if ((ctx->flags.is_sfe_fs || ctx->flags.is_sfe_shdr) &&
		(!ctx->sfe_info.skip_scratch_cfg_streamon)) {
		rc = cam_ife_mgr_prog_default_settings(false, ctx);
		if (rc)
			goto err;
		ctx->sfe_info.skip_scratch_cfg_streamon = false;
	}

	CAM_DBG(CAM_ISP, "START CSID HW ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the IFE CSID HW devices */
	cam_ife_mgr_csid_start_hw(ctx, primary_rdi_csid_res);

	/* Start IFE root node: do nothing */
	CAM_DBG(CAM_ISP, "Start success for ctx id:%d", ctx->ctx_index);

	return 0;

err:
	stop_isp.stop_only = false;
	stop_isp.hw_stop_cmd = CAM_ISP_HW_STOP_IMMEDIATELY;
	stop_args.ctxt_to_hw_map = start_isp->hw_config.ctxt_to_hw_map;
	stop_args.args = (void *)(&stop_isp);

	cam_ife_mgr_stop_hw(hw_mgr_priv, &stop_args);
	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;

cdm_streamoff:
	cam_cdm_stream_off(ctx->cdm_handle);
safe_disable:
	cam_ife_notify_safe_lut_scm(CAM_IFE_SAFE_DISABLE);

deinit_hw:
	cam_ife_hw_mgr_deinit_hw(ctx);

tasklet_stop:
	cam_tasklet_stop(ctx->common.tasklet_info);

	return rc;
}

static int cam_ife_mgr_read(void *hw_mgr_priv, void *read_args)
{
	return -EPERM;
}

static int cam_ife_mgr_write(void *hw_mgr_priv, void *write_args)
{
	return -EPERM;
}

static int cam_ife_mgr_reset(void *hw_mgr_priv, void *hw_reset_args)
{
	struct cam_ife_hw_mgr            *hw_mgr = hw_mgr_priv;
	struct cam_hw_reset_args         *reset_args = hw_reset_args;
	struct cam_ife_hw_mgr_ctx        *ctx;
	int                               rc = 0, i = 0;

	if (!hw_mgr_priv || !hw_reset_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)reset_args->ctxt_to_hw_map;
	if (!ctx || !ctx->flags.ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	if (hw_mgr->csid_global_reset_en) {
		CAM_DBG(CAM_ISP, "Path reset not supported");
		return 0;
	}

	CAM_DBG(CAM_ISP, "Reset CSID and VFE");

	rc = cam_ife_hw_mgr_reset_csid(ctx, CAM_IFE_CSID_RESET_PATH);

	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to reset CSID:%d rc: %d",
			rc);
		goto end;
	}

	for (i = 0; i < ctx->num_base; i++) {
		rc = cam_ife_mgr_reset_vfe_hw(hw_mgr, ctx->base[i].idx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Failed to reset VFE:%d rc: %d",
				ctx->base[i].idx, rc);
			goto end;
		}
	}

end:
	return rc;
}

static int cam_ife_mgr_release_hw(void *hw_mgr_priv,
					void *release_hw_args)
{
	int                               rc           = 0;
	struct cam_hw_release_args       *release_args = release_hw_args;
	struct cam_ife_hw_mgr            *hw_mgr       = hw_mgr_priv;
	struct cam_ife_hw_mgr_ctx        *ctx;
	uint32_t                          i;
	uint64_t                          ms, sec, min, hrs;

	if (!hw_mgr_priv || !release_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)release_args->ctxt_to_hw_map;
	if (!ctx || !ctx->flags.ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "Enter...ctx id:%d",
		ctx->ctx_index);

	if (ctx->flags.init_done)
		cam_ife_hw_mgr_deinit_hw(ctx);

	/* we should called the stop hw before this already */
	cam_ife_hw_mgr_release_hw_for_ctx(ctx);

	/* reset base info */
	ctx->num_base = 0;
	memset(ctx->base, 0, sizeof(ctx->base));

	/* release cdm handle */
	cam_cdm_release(ctx->cdm_handle);

	/* clean context */
	list_del_init(&ctx->list);
	ctx->cdm_handle = 0;
	ctx->cdm_ops = NULL;
	ctx->num_reg_dump_buf = 0;
	ctx->ctx_type = CAM_IFE_CTX_TYPE_NONE;
	ctx->ctx_config = 0;
	ctx->num_reg_dump_buf = 0;
	ctx->last_cdm_done_req = 0;
	ctx->left_hw_idx = 0;
	ctx->right_hw_idx = 0;
	ctx->sfe_info.num_fetches = 0;
	ctx->num_acq_vfe_out = 0;
	ctx->num_acq_sfe_out = 0;

	kfree(ctx->sfe_info.scratch_config);
	ctx->sfe_info.scratch_config = NULL;

	memset(&ctx->flags, 0, sizeof(struct cam_ife_hw_mgr_ctx_flags));
	atomic_set(&ctx->overflow_pending, 0);
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		ctx->sof_cnt[i] = 0;
		ctx->eof_cnt[i] = 0;
		ctx->epoch_cnt[i] = 0;
	}

	cam_ife_mgr_free_cdm_cmd(&ctx->cdm_cmd);

	CAM_GET_TIMESTAMP(ctx->ts);
	CAM_CONVERT_TIMESTAMP_FORMAT(ctx->ts, hrs, min, sec, ms);

	CAM_INFO(CAM_ISP, "%llu:%llu:%llu.%llu Release HW success ctx id: %u",
		hrs, min, sec, ms,
		ctx->ctx_index);

	memset(&ctx->ts, 0, sizeof(struct timespec64));
	cam_ife_hw_mgr_put_ctx(&hw_mgr->free_ctx_list, &ctx);
	return rc;
}

static int cam_isp_blob_fe_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_fe_config                  *fe_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	int                                    rc = -EINVAL;
	uint32_t                               i;
	struct cam_vfe_fe_update_args          fe_upd_args;

	ctx = prepare->ctxt_to_hw_map;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				fe_upd_args.node_res =
					hw_mgr_res->hw_res[i];

			memcpy(&fe_upd_args.fe_config, fe_config,
				sizeof(struct cam_fe_config));

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_FE_UPDATE_BUS_RD,
					&fe_upd_args,
					sizeof(
					struct cam_fe_config));
				if (rc)
					CAM_ERR(CAM_ISP, "fs Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			if (hw_mgr_res->res_id != CAM_ISP_HW_VFE_IN_RD)
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				fe_upd_args.node_res =
					hw_mgr_res->hw_res[i];

				memcpy(&fe_upd_args.fe_config, fe_config,
					sizeof(struct cam_fe_config));

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_FE_UPDATE_IN_RD,
					&fe_upd_args,
					sizeof(
					struct cam_vfe_fe_update_args));
				if (rc)
					CAM_ERR(CAM_ISP, "fe Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}
	return rc;
}

static int cam_isp_blob_ubwc_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_ubwc_config                *ubwc_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_ubwc_plane_cfg_v1          *ubwc_plane_cfg;
	struct cam_kmd_buf_info               *kmd_buf_info;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    rc = 0;

	ctx = prepare->ctxt_to_hw_map;
	if (!ctx) {
		CAM_ERR(CAM_ISP, "Invalid ctx");
		rc = -EINVAL;
		goto end;
	}

	if ((prepare->num_hw_update_entries + 1) >=
		prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient HW entries :%d max:%d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		rc = -EINVAL;
		goto end;
	}

	switch (ubwc_config->api_version) {
	case CAM_UBWC_CFG_VERSION_1:
		CAM_DBG(CAM_ISP, "num_ports= %d", ubwc_config->num_ports);

		kmd_buf_info = blob_info->kmd_buf_info;
		for (i = 0; i < ubwc_config->num_ports; i++) {
			ubwc_plane_cfg = &ubwc_config->ubwc_plane_cfg[i][0];
			res_id_out = ubwc_plane_cfg->port_type & 0xFF;

			CAM_DBG(CAM_ISP, "UBWC config idx %d, port_type=%d", i,
				ubwc_plane_cfg->port_type);

			if (res_id_out >= max_ife_out_res) {
				CAM_ERR(CAM_ISP, "Invalid port type:%x",
					ubwc_plane_cfg->port_type);
				rc = -EINVAL;
				goto end;
			}

			if ((kmd_buf_info->used_bytes
				+ total_used_bytes) < kmd_buf_info->size) {
				kmd_buf_remain_size = kmd_buf_info->size -
					(kmd_buf_info->used_bytes
					+ total_used_bytes);
			} else {
				CAM_ERR(CAM_ISP,
				"no free kmd memory for base=%d bytes_used=%u buf_size=%u",
					blob_info->base_info->idx, bytes_used,
					kmd_buf_info->size);
				rc = -ENOMEM;
				goto end;
			}

			cmd_buf_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes/4 +
				total_used_bytes/4;
			hw_mgr_res = &ctx->res_list_ife_out[res_id_out];

			if (!hw_mgr_res) {
				CAM_ERR(CAM_ISP, "Invalid hw_mgr_res");
				rc = -EINVAL;
				goto end;
			}

			rc = cam_isp_add_cmd_buf_update(
				hw_mgr_res, blob_type,
				blob_type_hw_cmd_map[blob_type],
				blob_info->base_info->idx,
				(void *)cmd_buf_addr,
				kmd_buf_remain_size,
				(void *)ubwc_plane_cfg,
				&bytes_used);
			if (rc < 0) {
				CAM_ERR(CAM_ISP,
					"Failed cmd_update, base_idx=%d, bytes_used=%u, res_id_out=0x%X",
					blob_info->base_info->idx,
					bytes_used,
					ubwc_plane_cfg->port_type);
				goto end;
			}

			total_used_bytes += bytes_used;
		}

		if (total_used_bytes)
			cam_ife_mgr_update_hw_entries_util(
				CAM_ISP_UNUSED_BL, total_used_bytes, kmd_buf_info, prepare);
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid UBWC API Version %d",
			ubwc_config->api_version);
		rc = -EINVAL;
		break;
	}
end:
	return rc;
}

static int cam_isp_get_generic_ubwc_data_v2(
	struct cam_ubwc_plane_cfg_v2       *ubwc_cfg,
	uint32_t                            version,
	struct cam_vfe_generic_ubwc_config *generic_ubwc_cfg)
{
	int i = 0;

	generic_ubwc_cfg->api_version = version;
	for (i = 0; i < CAM_PACKET_MAX_PLANES - 1; i++) {
		generic_ubwc_cfg->ubwc_plane_cfg[i].port_type             =
			ubwc_cfg[i].port_type;
		generic_ubwc_cfg->ubwc_plane_cfg[i].meta_stride           =
			ubwc_cfg[i].meta_stride;
		generic_ubwc_cfg->ubwc_plane_cfg[i].meta_size             =
			ubwc_cfg[i].meta_size;
		generic_ubwc_cfg->ubwc_plane_cfg[i].meta_offset           =
			ubwc_cfg[i].meta_offset;
		generic_ubwc_cfg->ubwc_plane_cfg[i].packer_config         =
			ubwc_cfg[i].packer_config;
		generic_ubwc_cfg->ubwc_plane_cfg[i].mode_config_0         =
			ubwc_cfg[i].mode_config_0;
		generic_ubwc_cfg->ubwc_plane_cfg[i].mode_config_1         =
			ubwc_cfg[i].mode_config_1;
		generic_ubwc_cfg->ubwc_plane_cfg[i].tile_config           =
			ubwc_cfg[i].tile_config;
		generic_ubwc_cfg->ubwc_plane_cfg[i].h_init                =
			ubwc_cfg[i].h_init;
		generic_ubwc_cfg->ubwc_plane_cfg[i].v_init                =
			ubwc_cfg[i].v_init;
		generic_ubwc_cfg->ubwc_plane_cfg[i].static_ctrl           =
			ubwc_cfg[i].static_ctrl;
		generic_ubwc_cfg->ubwc_plane_cfg[i].ctrl_2                =
			ubwc_cfg[i].ctrl_2;
		generic_ubwc_cfg->ubwc_plane_cfg[i].stats_ctrl_2          =
			ubwc_cfg[i].stats_ctrl_2;
		generic_ubwc_cfg->ubwc_plane_cfg[i].lossy_threshold_0     =
			ubwc_cfg[i].lossy_threshold_0;
		generic_ubwc_cfg->ubwc_plane_cfg[i].lossy_threshold_1     =
			ubwc_cfg[i].lossy_threshold_1;
		generic_ubwc_cfg->ubwc_plane_cfg[i].lossy_var_offset =
			ubwc_cfg[i].lossy_var_offset;
		generic_ubwc_cfg->ubwc_plane_cfg[i].bandwidth_limit       =
			ubwc_cfg[i].bandwidth_limit;
	}

	return 0;
}

static int cam_isp_blob_ubwc_update_v2(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_ubwc_config_v2             *ubwc_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_ubwc_plane_cfg_v2          *ubwc_plane_cfg;
	struct cam_kmd_buf_info               *kmd_buf_info;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    rc = 0;
	struct cam_vfe_generic_ubwc_config     generic_ubwc_cfg;

	ctx = prepare->ctxt_to_hw_map;
	if (!ctx) {
		CAM_ERR(CAM_ISP, "Invalid ctx");
		rc = -EINVAL;
		goto end;
	}

	if (prepare->num_hw_update_entries + 1 >=
		prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient HW entries :%d max:%d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_ISP, "num_ports= %d", ubwc_config->num_ports);

	kmd_buf_info = blob_info->kmd_buf_info;
	for (i = 0; i < ubwc_config->num_ports; i++) {
		ubwc_plane_cfg = &ubwc_config->ubwc_plane_cfg[i][0];
		res_id_out = ubwc_plane_cfg->port_type & 0xFF;

		CAM_DBG(CAM_ISP, "UBWC config idx %d, port_type=%d", i,
			ubwc_plane_cfg->port_type);

		if (res_id_out >= max_ife_out_res) {
			CAM_ERR(CAM_ISP, "Invalid port type:%x",
				ubwc_plane_cfg->port_type);
			rc = -EINVAL;
			goto end;
		}

		if ((kmd_buf_info->used_bytes
			+ total_used_bytes) < kmd_buf_info->size) {
			kmd_buf_remain_size = kmd_buf_info->size -
				(kmd_buf_info->used_bytes
				+ total_used_bytes);
		} else {
			CAM_ERR(CAM_ISP,
				"no free kmd memory for base=%d bytes_used=%u buf_size=%u",
				blob_info->base_info->idx, bytes_used,
				kmd_buf_info->size);
			rc = -ENOMEM;
			goto end;
		}

		cmd_buf_addr = kmd_buf_info->cpu_addr +
			kmd_buf_info->used_bytes/4 +
			total_used_bytes/4;
		hw_mgr_res = &ctx->res_list_ife_out[res_id_out];

		if (!hw_mgr_res) {
			CAM_ERR(CAM_ISP, "Invalid hw_mgr_res");
			rc = -EINVAL;
			goto end;
		}

		rc = cam_isp_get_generic_ubwc_data_v2(ubwc_plane_cfg,
			ubwc_config->api_version, &generic_ubwc_cfg);

		rc = cam_isp_add_cmd_buf_update(
			hw_mgr_res, blob_type,
			blob_type_hw_cmd_map[blob_type],
			blob_info->base_info->idx,
			(void *)cmd_buf_addr,
			kmd_buf_remain_size,
			(void *)&generic_ubwc_cfg,
			&bytes_used);
		if (rc < 0) {
			CAM_ERR(CAM_ISP,
				"Failed cmd_update, base_idx=%d, bytes_used=%u, res_id_out=0x%X",
				blob_info->base_info->idx,
				bytes_used,
				ubwc_plane_cfg->port_type);
			goto end;
		}

		total_used_bytes += bytes_used;
	}

	if (total_used_bytes)
		cam_ife_mgr_update_hw_entries_util(
			CAM_ISP_UNUSED_BL, total_used_bytes, kmd_buf_info, prepare);

end:
	return rc;
}

static int cam_isp_blob_sfe_scratch_buf_update(
	struct cam_isp_sfe_init_scratch_buf_config  *scratch_config,
	struct cam_hw_prepare_update_args           *prepare)
{
	int rc, i, mmu_hdl;
	uint32_t                               res_id_out;
	bool                                   is_buf_secure;
	dma_addr_t                             io_addr;
	size_t                                 size;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_sfe_scratch_buf_info   *buffer_info;
	struct cam_sfe_scratch_buf_info       *port_info;
	struct cam_isp_hw_mgr_res             *sfe_out_res;
	struct cam_ife_hw_mgr                 *ife_hw_mgr;

	ctx = prepare->ctxt_to_hw_map;
	ife_hw_mgr = ctx->hw_mgr;
	if (scratch_config->num_ports != ctx->sfe_info.num_fetches)
		CAM_WARN(CAM_ISP,
			"Getting scratch buffer for %u ports on ctx: %u with num_fetches: %u",
			scratch_config->num_ports, ctx->ctx_index, ctx->sfe_info.num_fetches);

	CAM_DBG(CAM_ISP, "num_ports: %u", scratch_config->num_ports);

	for (i = 0; i < scratch_config->num_ports; i++) {
		buffer_info = &scratch_config->port_scratch_cfg[i];
		res_id_out = buffer_info->resource_type & 0xFF;

		CAM_DBG(CAM_ISP, "scratch config idx: %d res: 0x%x",
			i, buffer_info->resource_type);

		if (res_id_out >= CAM_SFE_FE_RDI_NUM_MAX) {
			CAM_ERR(CAM_ISP, "invalid out res type: 0x%x",
				buffer_info->resource_type);
			return -EINVAL;
		}

		sfe_out_res = &ctx->res_list_sfe_out[res_id_out];
		if (!sfe_out_res->hw_res[0]) {
			CAM_ERR(CAM_ISP,
				"SFE rsrc_type: 0x%x not acquired, failing scratch config",
				buffer_info->resource_type);
			return -EINVAL;
		}

		port_info = &ctx->sfe_info.scratch_config->buf_info[res_id_out];
		is_buf_secure = cam_mem_is_secure_buf(buffer_info->mem_handle);
		if (is_buf_secure) {
			port_info->is_secure = true;
			mmu_hdl = ife_hw_mgr->mgr_common.img_iommu_hdl_secure;
		} else {
			port_info->is_secure = false;
			mmu_hdl = ife_hw_mgr->mgr_common.img_iommu_hdl;
		}

		rc = cam_mem_get_io_buf(buffer_info->mem_handle,
			mmu_hdl, &io_addr, &size, NULL);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"no scratch buf addr for res: 0x%x",
				buffer_info->resource_type);
			rc = -ENOMEM;
			return rc;
		}

		port_info->res_id = buffer_info->resource_type;
		port_info->io_addr = io_addr + buffer_info->offset;
		port_info->width = buffer_info->width;
		port_info->height = buffer_info->height;
		port_info->stride = buffer_info->stride;
		port_info->slice_height = buffer_info->slice_height;
		port_info->offset = 0;
		port_info->config_done = true;
		CAM_DBG(CAM_ISP,
			"res_id: 0x%x w: 0x%x h: 0x%x s: 0x%x sh: 0x%x addr: 0x%x",
			port_info->res_id, port_info->width,
			port_info->height, port_info->stride,
			port_info->slice_height, port_info->io_addr);
	}

	ctx->sfe_info.scratch_config->num_config = scratch_config->num_ports;
	return 0;
}

static inline int __cam_isp_sfe_send_cache_config(
	int32_t                                   cmd_type,
	struct cam_isp_sfe_bus_sys_cache_config  *wm_rm_cache_cfg)
{
	int rc = 0;
	struct cam_isp_resource_node *hw_res = wm_rm_cache_cfg->res;

	rc = hw_res->hw_intf->hw_ops.process_cmd(
		hw_res->hw_intf->hw_priv,
		cmd_type, wm_rm_cache_cfg,
		sizeof(struct cam_isp_sfe_bus_sys_cache_config));
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Failed in sending cache config for: %u",
			hw_res->res_id);
	}

	return rc;
}

static int cam_isp_blob_sfe_exp_order_update(
	uint32_t                             base_idx,
	struct cam_isp_sfe_exp_config       *exp_config,
	struct cam_hw_prepare_update_args   *prepare)
{
	int rc = 0, i, j;
	bool send_config;
	uint32_t exp_order_max = 0;
	uint32_t res_id_out, res_id_in;
	struct cam_ife_hw_mgr_ctx               *ctx;
	struct cam_isp_hw_mgr_res               *hw_mgr_res;
	struct cam_isp_hw_mgr_res               *tmp;
	struct cam_isp_sfe_wm_exp_order_config  *order_cfg;
	struct cam_ife_hw_mgr                   *hw_mgr;
	struct cam_isp_sfe_bus_sys_cache_config  wm_rm_cache_cfg;

	ctx = prepare->ctxt_to_hw_map;
	hw_mgr = ctx->hw_mgr;
	memset(ctx->flags.sys_cache_usage, false, sizeof(ctx->flags.sys_cache_usage));

	if (!hw_mgr->num_caches_found) {
		CAM_DBG(CAM_ISP, "No caches found during probe");
		return 0;
	}

	if (!exp_config->num_ports) {
		CAM_ERR(CAM_ISP, "Invalid number of ports: %d", exp_config->num_ports);
		return -EINVAL;
	}

	/*
	 *  The last resource in the array will be considered as
	 *  last exposure
	 */
	exp_order_max = exp_config->num_ports - 1;
	for (i = 0; i < exp_config->num_ports; i++) {
		order_cfg = &exp_config->wm_config[i];

		rc = cam_ife_hw_mgr_is_sfe_rdi_for_fetch(
			order_cfg->res_type);
		if (!rc) {
			CAM_ERR(CAM_ISP,
				"Not a SFE fetch RDI: 0x%x", order_cfg->res_type);
			return -EINVAL;
		}

		if ((order_cfg->res_type - CAM_ISP_SFE_OUT_RES_RDI_0) >=
			ctx->sfe_info.num_fetches) {
			CAM_DBG(CAM_ISP,
				"Skip cache config for resource: 0x%x, active fetches: %u [exp_order: %d %d] in %u ctx",
				order_cfg->res_type, ctx->sfe_info.num_fetches,
				i, exp_order_max, ctx->ctx_index);
			continue;
		}

		/* Add more params if needed */
		wm_rm_cache_cfg.wr_cfg_done = false;
		wm_rm_cache_cfg.rd_cfg_done = false;
		wm_rm_cache_cfg.use_cache =
			(exp_order_max == i) ? true : false;
		wm_rm_cache_cfg.scid = 0;
		send_config = false;

		/* Currently using cache for short only */
		if (wm_rm_cache_cfg.use_cache) {
			if (base_idx == CAM_SFE_CORE_0) {
				wm_rm_cache_cfg.scid =
					hw_mgr->sys_cache_info[CAM_LLCC_SMALL_1].scid;
				if (wm_rm_cache_cfg.scid <= 0)
					goto end;

				ctx->flags.sys_cache_usage[CAM_LLCC_SMALL_1] = true;
			} else if (base_idx == CAM_SFE_CORE_1) {
				wm_rm_cache_cfg.scid =
					hw_mgr->sys_cache_info[CAM_LLCC_SMALL_2].scid;
				if (wm_rm_cache_cfg.scid <= 0)
					goto end;

				ctx->flags.sys_cache_usage[CAM_LLCC_SMALL_2] = true;
			}
		}

		/* Configure cache config for WM */
		res_id_out = order_cfg->res_type & 0xFF;
		if (res_id_out >= CAM_SFE_HW_OUT_RES_MAX) {
			CAM_ERR_RATE_LIMIT(CAM_ISP, "res_id_out: %d exceeds max size: %d",
				res_id_out, CAM_SFE_HW_OUT_RES_MAX);
			return -EINVAL;
		}

		hw_mgr_res = &ctx->res_list_sfe_out[res_id_out];
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			wm_rm_cache_cfg.res = hw_mgr_res->hw_res[j];
			rc = __cam_isp_sfe_send_cache_config(
				CAM_ISP_HW_SFE_SYS_CACHE_WM_CONFIG,
				&wm_rm_cache_cfg);
			send_config = true;
			break;
		}

		if (rc || !send_config) {
			CAM_ERR(CAM_ISP,
				"Failed to send cache config for WR res: 0x%x base_idx: %u send_config: %d rc: %d",
				order_cfg->res_type, base_idx, send_config, rc);
			return -EINVAL;
		}

		send_config = false;
		/* RDI WMs have been validated find corresponding RM */
		if (order_cfg->res_type == CAM_ISP_SFE_OUT_RES_RDI_0)
			res_id_in = CAM_ISP_HW_SFE_IN_RD0;
		else if (order_cfg->res_type == CAM_ISP_SFE_OUT_RES_RDI_1)
			res_id_in = CAM_ISP_HW_SFE_IN_RD1;
		else
			res_id_in = CAM_ISP_HW_SFE_IN_RD2;

		/* Configure cache config for RM */
		list_for_each_entry_safe(hw_mgr_res, tmp, &ctx->res_list_ife_in_rd, list) {
			for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
				if (!hw_mgr_res->hw_res[j])
					continue;

				if (hw_mgr_res->hw_res[j]->res_id != res_id_in)
					continue;

				if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
					continue;

				wm_rm_cache_cfg.res = hw_mgr_res->hw_res[j];
				rc = __cam_isp_sfe_send_cache_config(
					CAM_ISP_HW_SFE_SYS_CACHE_RM_CONFIG,
					&wm_rm_cache_cfg);
				send_config = true;
				break;
			}
		}

		if (rc || !send_config) {
			CAM_ERR(CAM_ISP,
				"Failed to send cache config for RD res: 0x%x base_idx: %u send_config: %d rc: %d",
				res_id_in, base_idx, send_config, rc);
			return -EINVAL;
		}

		if (!wm_rm_cache_cfg.rd_cfg_done && !wm_rm_cache_cfg.wr_cfg_done) {
			wm_rm_cache_cfg.use_cache = false;
			if (base_idx == CAM_SFE_CORE_0)
				ctx->flags.sys_cache_usage[CAM_LLCC_SMALL_1] = false;
			else if (base_idx == CAM_SFE_CORE_1)
				ctx->flags.sys_cache_usage[CAM_LLCC_SMALL_2] = false;
		}

		CAM_DBG(CAM_ISP,
			"cache %s on exp order: %u [max: %u] for out: 0x%x",
			(wm_rm_cache_cfg.use_cache ? "enabled" : "not enabled"),
			i, exp_order_max, order_cfg->res_type);
	}

	return rc;

end:
	return 0;
}

static int cam_isp_blob_sfe_update_fetch_core_cfg(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_hw_prepare_update_args     *prepare)
{
	int                                rc;
	uint32_t                           used_bytes = 0, total_used_bytes = 0;
	uint32_t                           remain_size, res_id;
	uint32_t                          *cpu_addr = NULL;
	bool                               enable = true;
	struct cam_isp_hw_mgr_res         *hw_mgr_res;
	struct cam_kmd_buf_info           *kmd_buf_info;
	struct cam_ife_hw_mgr_ctx         *ctx = NULL;

	ctx = prepare->ctxt_to_hw_map;
	if (prepare->num_hw_update_entries + 1 >=
		prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient HW entries :%d",
			prepare->num_hw_update_entries);
		return -EINVAL;
	}

	kmd_buf_info = blob_info->kmd_buf_info;
	list_for_each_entry(hw_mgr_res,  &ctx->res_list_ife_in_rd, list) {
		if ((kmd_buf_info->used_bytes
			+ total_used_bytes) < kmd_buf_info->size) {
			remain_size = kmd_buf_info->size -
				(kmd_buf_info->used_bytes +
				total_used_bytes);
		} else {
			CAM_ERR(CAM_ISP,
				"No free kmd memory for base idx: %d",
				blob_info->base_info->idx);
				rc = -ENOMEM;
				return rc;
		}

		cpu_addr = kmd_buf_info->cpu_addr +
			(kmd_buf_info->used_bytes / 4) +
			(total_used_bytes / 4);

		res_id = hw_mgr_res->res_id;

		/* check for active fetches */
		if ((ctx->ctx_config &
			CAM_IFE_CTX_CFG_DYNAMIC_SWITCH_ON) &&
			((res_id - CAM_ISP_SFE_IN_RD_0) >=
			ctx->sfe_info.scratch_config->curr_num_exp))
			enable = false;
		else
			enable = true;

		cpu_addr = kmd_buf_info->cpu_addr +
			kmd_buf_info->used_bytes  / 4 +
			total_used_bytes / 4;

		CAM_DBG(CAM_ISP,
			"SFE:%u RM: %u res_id: 0x%x enable: %u num_exp: %u",
			blob_info->base_info->idx,
			(res_id - CAM_ISP_SFE_IN_RD_0), res_id, enable,
			ctx->sfe_info.scratch_config->curr_num_exp);

		rc = cam_isp_add_cmd_buf_update(
			hw_mgr_res, blob_type,
			CAM_ISP_HW_CMD_RM_ENABLE_DISABLE,
			blob_info->base_info->idx,
			(void *)cpu_addr, remain_size,
			(void *)&enable, &used_bytes);
		if (rc < 0) {
			CAM_ERR(CAM_ISP,
				"Failed to dynamically %s SFE: %u RM: %u bytes_used: %u rc: %d",
				(enable ? "enable" : "disable"),
				blob_info->base_info->idx, res_id,
				used_bytes, rc);
			return rc;
		}

		total_used_bytes += used_bytes;
	}

	if (total_used_bytes)
		cam_ife_mgr_update_hw_entries_util(
			false, total_used_bytes, kmd_buf_info, prepare);

	return 0;
}

static int cam_isp_blob_hfr_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_resource_hfr_config    *hfr_config,
	struct cam_hw_prepare_update_args     *prepare,
	uint32_t                               out_max,
	enum cam_isp_hw_type                   hw_type)
{
	struct cam_isp_port_hfr_config        *port_hfr_config;
	struct cam_kmd_buf_info               *kmd_buf_info;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    rc = 0;

	ctx = prepare->ctxt_to_hw_map;
	CAM_DBG(CAM_ISP, "num_ports= %d",
		hfr_config->num_ports);

	/* Max one hw entries required for hfr config update */
	if (prepare->num_hw_update_entries + 1 >=
			prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient  HW entries :%d %d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		return -EINVAL;
	}

	kmd_buf_info = blob_info->kmd_buf_info;
	for (i = 0; i < hfr_config->num_ports; i++) {
		port_hfr_config = &hfr_config->port_hfr_config[i];
		res_id_out = port_hfr_config->resource_type & 0xFF;

		CAM_DBG(CAM_ISP, "type %d hfr config idx %d, type=%d",
			hw_type, i, res_id_out);

		if (res_id_out >= out_max) {
			CAM_ERR(CAM_ISP, "invalid out restype:%x",
				port_hfr_config->resource_type);
			return -EINVAL;
		}

		if ((kmd_buf_info->used_bytes
			+ total_used_bytes) < kmd_buf_info->size) {
			kmd_buf_remain_size = kmd_buf_info->size -
			(kmd_buf_info->used_bytes +
			total_used_bytes);
		} else {
			CAM_ERR(CAM_ISP,
			"no free kmd memory for base %d",
			blob_info->base_info->idx);
			rc = -ENOMEM;
			return rc;
		}

		cmd_buf_addr = kmd_buf_info->cpu_addr +
			kmd_buf_info->used_bytes/4 +
			total_used_bytes/4;
		if (hw_type == CAM_ISP_HW_TYPE_SFE)
			hw_mgr_res = &ctx->res_list_sfe_out[res_id_out];
		else
			hw_mgr_res = &ctx->res_list_ife_out[res_id_out];

		rc = cam_isp_add_cmd_buf_update(
			hw_mgr_res, blob_type,
			CAM_ISP_HW_CMD_GET_HFR_UPDATE,
			blob_info->base_info->idx,
			(void *)cmd_buf_addr,
			kmd_buf_remain_size,
			(void *)port_hfr_config,
			&bytes_used);
		if (rc < 0) {
			CAM_ERR(CAM_ISP,
				"Failed cmd_update, base_idx=%d, rc=%d, res_id_out=0x%X hw_type=%d",
				blob_info->base_info->idx, bytes_used,
				port_hfr_config->resource_type, hw_type);
			return rc;
		}

		total_used_bytes += bytes_used;
	}

	if (total_used_bytes)
		cam_ife_mgr_update_hw_entries_util(
			CAM_ISP_IQ_BL, total_used_bytes, kmd_buf_info, prepare);

	return rc;
}

static int cam_isp_blob_csid_discard_init_frame_update(
	struct cam_isp_generic_blob_info       *blob_info,
	struct cam_isp_discard_initial_frames  *discard_config,
	struct cam_hw_prepare_update_args      *prepare)
{
	struct cam_ife_hw_mgr_ctx                   *ctx = NULL;
	struct cam_ife_hw_mgr                       *ife_hw_mgr;
	struct cam_hw_intf                          *hw_intf;
	struct cam_isp_hw_mgr_res                   *hw_mgr_res;
	struct cam_isp_resource_node                *res;
	struct cam_ife_csid_discard_init_frame_args discard_args;
	int rc = -EINVAL, i;

	ctx = prepare->ctxt_to_hw_map;
	ife_hw_mgr = ctx->hw_mgr;
	discard_args.num_frames = discard_config->num_frames;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			res = hw_mgr_res->hw_res[i];
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				if (hw_intf->hw_idx != blob_info->base_info->idx)
					continue;

				discard_args.res = res;
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CSID_DISCARD_INIT_FRAMES,
					&discard_args,
					sizeof(struct cam_ife_csid_discard_init_frame_args));
				if (rc) {
					CAM_ERR(CAM_ISP,
						"Failed to update discard frame cfg for res: %s on CSID[%u]",
						res->res_name, blob_info->base_info->idx);
					break;
				}
			}
		}
	}

	return rc;
}


static int cam_isp_blob_csid_dynamic_switch_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_mode_switch_info       *mup_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_ife_hw_mgr_ctx                  *ctx = NULL;
	struct cam_hw_intf                         *hw_intf;
	struct cam_ife_csid_mode_switch_update_args csid_mup_upd_args;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;
	struct cam_isp_prepare_hw_update_data      *prepare_hw_data;
	int                                         i, rc = -EINVAL;

	ctx = prepare->ctxt_to_hw_map;
	ife_hw_mgr = ctx->hw_mgr;

	CAM_DBG(CAM_ISP,
		"csid mup value=%u", mup_config->mup);

	prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;
	prepare_hw_data->mup_en = true;
	prepare_hw_data->mup_val = mup_config->mup;

	csid_mup_upd_args.mup_args.mup = mup_config->mup;
	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].hw_type != CAM_ISP_HW_TYPE_CSID)
			continue;

		if (ctx->base[i].split_id != CAM_ISP_HW_SPLIT_LEFT)
			continue;

		/* For sHDR dynamic switch update num starting exposures to CSID for INIT */
		if ((prepare_hw_data->packet_opcode_type == CAM_ISP_PACKET_INIT_DEV) &&
			(ctx->flags.is_sfe_shdr)) {
			csid_mup_upd_args.exp_update_args.reset_discard_cfg = true;
			csid_mup_upd_args.exp_update_args.num_exposures =
				mup_config->num_expoures;
		}

		hw_intf = ife_hw_mgr->csid_devices[ctx->base[i].idx];
		if (hw_intf && hw_intf->hw_ops.process_cmd) {
			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_ISP_HW_CMD_CSID_DYNAMIC_SWITCH_UPDATE,
				&csid_mup_upd_args,
				sizeof(struct cam_ife_csid_mode_switch_update_args));
			if (rc)
				CAM_ERR(CAM_ISP, "Dynamic switch update failed");
		}
	}

	return rc;
}

static int cam_isp_blob_csid_clock_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_csid_clock_config      *clock_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_ife_csid_clock_update_args  csid_clock_upd_args;
	uint64_t                               clk_rate = 0;
	int                                    rc = -EINVAL;
	uint32_t                               i;

	ctx = prepare->ctxt_to_hw_map;

	CAM_DBG(CAM_ISP,
		"csid clk=%llu", clock_config->csid_clock);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i])
				continue;
			clk_rate = clock_config->csid_clock;
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				csid_clock_upd_args.clk_rate = clk_rate;
				CAM_DBG(CAM_ISP, "i= %d clk=%llu\n",
				i, csid_clock_upd_args.clk_rate);

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					blob_type_hw_cmd_map[blob_type],
					&csid_clock_upd_args,
					sizeof(
					struct cam_ife_csid_clock_update_args));
				if (rc)
					CAM_ERR(CAM_ISP, "Clock Update failed");
			} else
				CAM_ERR(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_isp_blob_csid_qcfa_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_csid_qcfa_config       *qcfa_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_ife_csid_qcfa_update_args   csid_qcfa_upd_args;
	int                                    rc = -EINVAL;
	uint32_t                               i;

	ctx = prepare->ctxt_to_hw_map;

	CAM_DBG(CAM_ISP,
		"csid binning=%d", qcfa_config->csid_binning);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {

			if (!hw_mgr_res->hw_res[i] ||
				hw_mgr_res->res_id != CAM_IFE_PIX_PATH_RES_IPP)
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				csid_qcfa_upd_args.qcfa_binning =
						qcfa_config->csid_binning;
				csid_qcfa_upd_args.res =
					hw_mgr_res->hw_res[i];

				CAM_DBG(CAM_ISP, "i= %d QCFA binning=%d\n",
				i, csid_qcfa_upd_args.qcfa_binning);

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CSID_QCFA_SUPPORTED,
					&csid_qcfa_upd_args,
					sizeof(
					struct cam_ife_csid_qcfa_update_args));
				if (rc)
					CAM_ERR(CAM_ISP, "QCFA Update failed");
			} else
				CAM_ERR(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_isp_blob_core_cfg_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_core_config            *core_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	uint64_t                               clk_rate = 0;
	int                                    rc = 0, i;
	struct cam_vfe_core_config_args        vfe_core_config;

	ctx = prepare->ctxt_to_hw_map;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i])
				continue;

			if ((hw_mgr_res->res_id ==
				CAM_ISP_HW_VFE_IN_CAMIF) ||
				(hw_mgr_res->res_id ==
				CAM_ISP_HW_VFE_IN_PDLIB)) {
				hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
				if (hw_intf && hw_intf->hw_ops.process_cmd) {
					vfe_core_config.node_res =
						hw_mgr_res->hw_res[i];

					memcpy(&vfe_core_config.core_config,
						core_config,
						sizeof(
						struct cam_isp_core_config));

					rc = hw_intf->hw_ops.process_cmd(
						hw_intf->hw_priv,
						CAM_ISP_HW_CMD_CORE_CONFIG,
						&vfe_core_config,
						sizeof(
						struct cam_vfe_core_config_args)
						);
					if (rc)
						CAM_ERR(CAM_ISP,
						"Core cfg parse fail");
				} else {
					CAM_WARN(CAM_ISP, "NULL hw_intf!");
				}
			}
		}
	}

	return rc;
}

static int cam_isp_blob_sfe_core_cfg_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_sfe_core_config        *core_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	int                                   rc = -EINVAL, i, idx;
	struct cam_sfe_core_config_args       sfe_core_config;
	struct cam_ife_hw_mgr_ctx            *ctx = NULL;
	struct cam_hw_intf                   *hw_intf;
	struct cam_ife_hw_mgr                *ife_hw_mgr;

	ctx = prepare->ctxt_to_hw_map;
	ife_hw_mgr = ctx->hw_mgr;
	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].hw_type != CAM_ISP_HW_TYPE_SFE)
			continue;

		idx = ctx->base[i].idx;
		if (idx >= CAM_SFE_HW_NUM_MAX ||
			!ife_hw_mgr->sfe_devices[idx])
			continue;

		hw_intf = ife_hw_mgr->sfe_devices[idx]->hw_intf;
		if (hw_intf && hw_intf->hw_ops.process_cmd) {
			memcpy(&sfe_core_config.core_config,
				core_config,
				sizeof(struct cam_isp_sfe_core_config));
			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_ISP_HW_CMD_CORE_CONFIG,
				&sfe_core_config,
				sizeof(struct cam_sfe_core_config_args));
				if (rc)
					CAM_ERR(CAM_ISP,
						"SFE core cfg parse fail");
		} else {
			CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_isp_blob_ife_clock_update(
	struct cam_isp_clock_config           *clock_config,
	struct cam_ife_hw_mgr_ctx             *ctx)
{
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_vfe_clock_update_args       clock_upd_args;
	uint64_t                               clk_rate = 0;
	int                                    rc = -EINVAL;
	uint32_t                               i, j;
	bool                                   camif_l_clk_updated = false;
	bool                                   camif_r_clk_updated = false;

	CAM_DBG(CAM_PERF, "IFE clk update usage=%u left_clk= %lu right_clk=%lu",
		clock_config->usage_type, clock_config->left_pix_hz, clock_config->right_pix_hz);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i])
				continue;

			if ((hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF) ||
				(hw_mgr_res->res_id ==
				CAM_ISP_HW_VFE_IN_PDLIB)) {
				if (i == CAM_ISP_HW_SPLIT_LEFT) {
					if (camif_l_clk_updated)
						continue;

					clk_rate =
						clock_config->left_pix_hz;

					camif_l_clk_updated = true;
				} else {
					if (camif_r_clk_updated)
						continue;

					clk_rate =
						clock_config->right_pix_hz;

					camif_r_clk_updated = true;
				}
			} else if ((hw_mgr_res->res_id >=
				CAM_ISP_HW_VFE_IN_RD) && (hw_mgr_res->res_id
				<= CAM_ISP_HW_VFE_IN_RDI3))
				for (j = 0; j < clock_config->num_rdi; j++)
					clk_rate = max(clock_config->rdi_hz[j],
						clk_rate);
			else
				if (hw_mgr_res->res_id != CAM_ISP_HW_VFE_IN_LCR
					&& hw_mgr_res->hw_res[i]) {
					CAM_ERR(CAM_ISP, "Invalid res_id %u",
						hw_mgr_res->res_id);
					rc = -EINVAL;
					return rc;
				}

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				clock_upd_args.node_res = hw_mgr_res->hw_res[i];
				CAM_DBG(CAM_PERF,
					"Update Clock value res_id=%u i= %d clk=%llu",
					hw_mgr_res->res_id, i, clk_rate);

				clock_upd_args.clk_rate = clk_rate;

				/*
				 * Update clock values to top, actual apply to hw will happen when
				 * CAM_ISP_HW_CMD_APPLY_CLK_BW_UPDATE is called
				 */
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CLOCK_UPDATE,
					&clock_upd_args,
					sizeof(
					struct cam_vfe_clock_update_args));
				if (rc) {
					CAM_ERR(CAM_PERF,
						"IFE:%d Clock Update failed clk_rate:%llu rc:%d",
						hw_intf->hw_idx, clk_rate, rc);
					goto end;
				}
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

end:
	return rc;
}


static int cam_isp_blob_sfe_clock_update(
	struct cam_isp_clock_config           *clock_config,
	struct cam_ife_hw_mgr_ctx             *ctx)
{
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_sfe_clock_update_args       clock_upd_args;
	uint64_t                               clk_rate = 0;
	int                                    rc = -EINVAL;
	uint32_t                               i, j;
	bool                                   l_clk_updated = false;
	bool                                   r_clk_updated = false;


	CAM_DBG(CAM_PERF,
		"SFE clk update usage: %u left_clk: %lu right_clk: %lu",
		clock_config->usage_type, clock_config->left_pix_hz, clock_config->right_pix_hz);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_sfe_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i])
				continue;

			if (hw_mgr_res->res_id == CAM_ISP_HW_SFE_IN_PIX) {
				if (i == CAM_ISP_HW_SPLIT_LEFT) {
					if (l_clk_updated)
						continue;

					clk_rate =
						clock_config->left_pix_hz;
					l_clk_updated = true;
				} else {
					if (r_clk_updated)
						continue;

					clk_rate =
						clock_config->right_pix_hz;
					r_clk_updated = true;
				}
			} else {
				for (j = 0; j < clock_config->num_rdi; j++)
					clk_rate = max(clock_config->rdi_hz[j],
						clk_rate);
			}

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				clock_upd_args.node_res =
					hw_mgr_res->hw_res[i];
				CAM_DBG(CAM_PERF,
				"SFE res_id: %u i: %d clk: %llu",
				hw_mgr_res->res_id, i, clk_rate);

				clock_upd_args.clk_rate = clk_rate;
				/*
				 * Update clock values to top, actual apply to hw will happen when
				 * CAM_ISP_HW_CMD_APPLY_CLK_BW_UPDATE is called
				 */
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CLOCK_UPDATE,
					&clock_upd_args,
					sizeof(
					struct cam_sfe_clock_update_args));
				if (rc)
					CAM_ERR(CAM_PERF,
						"SFE clock update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_isp_blob_sfe_rd_update(
	uint32_t                               blob_type,
	uint32_t                               kmd_buf_remain_size,
	uint32_t                              *cmd_buf_addr,
	uint32_t                              *total_used_bytes,
	struct cam_ife_hw_mgr_ctx             *ctx,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_vfe_wm_config          *wm_config)
{
	int                                   rc;
	uint32_t                              bytes_used = 0;
	bool                                  found = false;
	struct cam_isp_hw_mgr_res            *sfe_rd_res;

	list_for_each_entry(sfe_rd_res, &ctx->res_list_ife_in_rd,
		list) {
		if (sfe_rd_res->res_id == wm_config->port_type) {
			found = true;
			break;
		}
	}

	if (!found) {
		CAM_ERR(CAM_ISP,
			"Failed to find SFE rd resource: %u, check if rsrc is acquired",
			wm_config->port_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "SFE RM config for port: 0x%x",
		wm_config->port_type);

	rc = cam_isp_add_cmd_buf_update(
		sfe_rd_res, blob_type,
		CAM_ISP_HW_CMD_FE_UPDATE_BUS_RD,
		blob_info->base_info->idx,
		(void *)cmd_buf_addr,
		kmd_buf_remain_size,
		(void *)wm_config,
		&bytes_used);
	if (rc < 0) {
		CAM_ERR(CAM_ISP,
			"Failed to update SFE RM config out_type:0x%X base_idx:%d bytes_used:%u rc:%d",
			wm_config->port_type, blob_info->base_info->idx,
			bytes_used, rc);
		return rc;
	}

	*total_used_bytes += bytes_used;
	return rc;
}

static int cam_ife_hw_mgr_update_scratch_offset(
	struct cam_ife_hw_mgr_ctx             *ctx,
	struct cam_isp_vfe_wm_config          *wm_config)
{
	uint32_t res_id;
	struct cam_sfe_scratch_buf_info       *port_info;

	if ((wm_config->port_type - CAM_ISP_SFE_OUT_RES_RDI_0) >=
		ctx->sfe_info.num_fetches)
		return 0;

	res_id = wm_config->port_type & 0xFF;

	if (res_id >= CAM_SFE_FE_RDI_NUM_MAX) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "res_id: %d exceeds max size: %d",
			res_id, CAM_SFE_FE_RDI_NUM_MAX);
		return -EINVAL;
	}

	if (!ctx->sfe_info.scratch_config->buf_info[res_id].config_done) {
		CAM_ERR(CAM_ISP,
			"Scratch buffer not configured on ctx: %u for res: %u",
			ctx->ctx_index, res_id);
		return -EINVAL;
	}

	port_info = &ctx->sfe_info.scratch_config->buf_info[res_id];
	port_info->offset = wm_config->offset;

	CAM_DBG(CAM_ISP, "Scratch addr: 0x%x offset: %u updated for: %s",
		port_info->io_addr, port_info->offset, wm_config->port_type);

	return 0;
}

static int cam_isp_blob_vfe_out_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_vfe_out_config         *vfe_out_config,
	struct cam_hw_prepare_update_args     *prepare,
	uint32_t                               size_isp_out,
	enum cam_isp_hw_type                   hw_type)
{
	struct cam_isp_vfe_wm_config          *wm_config;
	struct cam_kmd_buf_info               *kmd_buf_info;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *isp_out_res;
	enum cam_isp_hw_sfe_in                 rd_path = CAM_ISP_HW_SFE_IN_MAX;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    rc = 0;
	bool                                   rm_config = false;

	ctx = prepare->ctxt_to_hw_map;

	if (prepare->num_hw_update_entries + 1 >=
			prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient HW entries :%d",
			prepare->num_hw_update_entries);
		return -EINVAL;
	}

	kmd_buf_info = blob_info->kmd_buf_info;
	for (i = 0; i < vfe_out_config->num_ports; i++) {
		wm_config = &vfe_out_config->wm_config[i];
		if ((hw_type == CAM_ISP_HW_TYPE_VFE) &&
			(!cam_ife_hw_mgr_is_ife_out_port(wm_config->port_type)))
			continue;

		if (hw_type == CAM_ISP_HW_TYPE_SFE) {
			rd_path = cam_ife_hw_mgr_get_sfe_rd_res_id(wm_config->port_type);
			if ((!cam_ife_hw_mgr_is_sfe_out_port(wm_config->port_type)) &&
				(rd_path == CAM_ISP_HW_SFE_IN_MAX))
				continue;

			if (rd_path != CAM_ISP_HW_SFE_IN_MAX)
				rm_config = true;
		}

		if ((kmd_buf_info->used_bytes
			+ total_used_bytes) < kmd_buf_info->size) {
			kmd_buf_remain_size = kmd_buf_info->size -
			(kmd_buf_info->used_bytes +
			total_used_bytes);
		} else {
			CAM_ERR(CAM_ISP,
			"No free kmd memory for base idx: %d",
			blob_info->base_info->idx);
			rc = -ENOMEM;
			return rc;
		}

		cmd_buf_addr = kmd_buf_info->cpu_addr +
			(kmd_buf_info->used_bytes / 4) +
			(total_used_bytes / 4);

		if (rm_config) {
			rc = cam_isp_blob_sfe_rd_update(blob_type,
				kmd_buf_remain_size, cmd_buf_addr,
				&total_used_bytes, ctx, blob_info, wm_config);
			if (rc)
				return rc;

			rm_config = false;
			continue;
		}

		res_id_out = wm_config->port_type & 0xFF;

		CAM_DBG(CAM_ISP, "%s out config idx: %d port: 0x%x",
			(hw_type == CAM_ISP_HW_TYPE_SFE ? "SFE" : "VFE"),
			i, wm_config->port_type);

		if (res_id_out >= size_isp_out) {
			CAM_ERR(CAM_ISP, "Invalid out port:0x%x",
				wm_config->port_type);
			return -EINVAL;
		}

		if (hw_type == CAM_ISP_HW_TYPE_SFE) {
			/* Update offset for scratch to compare for buf done */
			if ((ctx->flags.is_sfe_shdr) &&
				(cam_ife_hw_mgr_is_sfe_rdi_for_fetch(wm_config->port_type))) {
				rc = cam_ife_hw_mgr_update_scratch_offset(ctx, wm_config);
				if (rc)
					return rc;
			}

			isp_out_res = &ctx->res_list_sfe_out[res_id_out];
		} else {
			isp_out_res = &ctx->res_list_ife_out[res_id_out];
		}

		rc = cam_isp_add_cmd_buf_update(
			isp_out_res, blob_type,
			CAM_ISP_HW_CMD_WM_CONFIG_UPDATE,
			blob_info->base_info->idx,
			(void *)cmd_buf_addr,
			kmd_buf_remain_size,
			(void *)wm_config,
			&bytes_used);
		if (rc < 0) {
			CAM_ERR(CAM_ISP,
				"Failed to update %s Out out_type:0x%X base_idx:%d bytes_used:%u rc:%d",
				((hw_type == CAM_ISP_HW_TYPE_SFE) ?
				"SFE" : "VFE"),
				wm_config->port_type, blob_info->base_info->idx,
				bytes_used, rc);
			return rc;
		}

		total_used_bytes += bytes_used;
	}

	if (total_used_bytes)
		cam_ife_mgr_update_hw_entries_util(
			CAM_ISP_UNUSED_BL, total_used_bytes, kmd_buf_info, prepare);

	return rc;
}

static int cam_isp_blob_sensor_blanking_config(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_sensor_blanking_config *sensor_blanking_config,
	struct cam_hw_prepare_update_args     *prepare)

{
	struct cam_ife_hw_mgr_ctx       *ctx = NULL;
	struct cam_isp_hw_mgr_res       *hw_mgr_res;
	struct cam_hw_intf              *hw_intf;
	struct cam_isp_blanking_config  blanking_config;
	int                             rc = 0, i;

	ctx = prepare->ctxt_to_hw_map;
	if (list_empty(&ctx->res_list_ife_src)) {
		CAM_ERR(CAM_ISP, "Mux List empty");
		return -ENODEV;
	}

	list_for_each_entry(hw_mgr_res,
		&ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			blanking_config.node_res = hw_mgr_res->hw_res[i];
			blanking_config.vbi = sensor_blanking_config->vbi;
			blanking_config.hbi = sensor_blanking_config->hbi;

			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_BLANKING_UPDATE,
					&blanking_config,
					sizeof(
					struct cam_isp_blanking_config));
				if (rc)
					CAM_ERR(CAM_ISP,
					"blanking update failed");
			}
		}
	}

	return rc;
}

static int cam_isp_blob_bw_limit_update(
	uint32_t                                   blob_type,
	struct cam_isp_generic_blob_info          *blob_info,
	struct cam_isp_out_rsrc_bw_limiter_config *bw_limit_cfg,
	struct cam_hw_prepare_update_args         *prepare,
	enum cam_isp_hw_type                       hw_type)
{
	struct cam_isp_wm_bw_limiter_config   *wm_bw_limit_cfg;
	struct cam_kmd_buf_info               *kmd_buf_info;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *isp_out_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    rc = 0;

	ctx = prepare->ctxt_to_hw_map;

	if ((prepare->num_hw_update_entries + 1) >=
			prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient HW entries: %d max: %d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		return -EINVAL;
	}

	kmd_buf_info = blob_info->kmd_buf_info;
	for (i = 0; i < bw_limit_cfg->num_ports; i++) {
		wm_bw_limit_cfg = &bw_limit_cfg->bw_limiter_config[i];
		res_id_out = wm_bw_limit_cfg->res_type & 0xFF;

		if ((hw_type == CAM_ISP_HW_TYPE_SFE) &&
			!((wm_bw_limit_cfg->res_type >=
			CAM_ISP_SFE_OUT_RES_BASE) &&
			(wm_bw_limit_cfg->res_type <
			CAM_ISP_SFE_OUT_RES_MAX)))
			continue;

		if ((hw_type == CAM_ISP_HW_TYPE_VFE) &&
			!((wm_bw_limit_cfg->res_type >=
			CAM_ISP_IFE_OUT_RES_BASE) &&
			(wm_bw_limit_cfg->res_type <
			(CAM_ISP_IFE_OUT_RES_BASE + max_ife_out_res))))
			continue;

		CAM_DBG(CAM_ISP, "%s BW limit config idx: %d port: 0x%x enable: %d [0x%x:0x%x]",
			(hw_type == CAM_ISP_HW_TYPE_SFE ? "SFE" : "VFE"),
			i, wm_bw_limit_cfg->res_type,
			wm_bw_limit_cfg->enable_limiter,
			wm_bw_limit_cfg->counter_limit[0],
			wm_bw_limit_cfg->counter_limit[1]);

		if ((kmd_buf_info->used_bytes
			+ total_used_bytes) < kmd_buf_info->size) {
			kmd_buf_remain_size = kmd_buf_info->size -
			(kmd_buf_info->used_bytes +
			total_used_bytes);
		} else {
			CAM_ERR(CAM_ISP,
				"No free kmd memory for base idx: %d",
				blob_info->base_info->idx);
			rc = -ENOMEM;
			return rc;
		}

		cmd_buf_addr = kmd_buf_info->cpu_addr +
			(kmd_buf_info->used_bytes / 4) +
			(total_used_bytes / 4);

		if (hw_type == CAM_ISP_HW_TYPE_SFE)
			isp_out_res = &ctx->res_list_sfe_out[res_id_out];
		else
			isp_out_res = &ctx->res_list_ife_out[res_id_out];

		rc = cam_isp_add_cmd_buf_update(
			isp_out_res, blob_type,
			CAM_ISP_HW_CMD_WM_BW_LIMIT_CONFIG,
			blob_info->base_info->idx,
			(void *)cmd_buf_addr,
			kmd_buf_remain_size,
			(void *)wm_bw_limit_cfg,
			&bytes_used);
		if (rc < 0) {
			CAM_ERR(CAM_ISP,
				"Failed to update %s BW limiter config for res:0x%x enable:%d [0x%x:0x%x] base_idx:%d bytes_used:%u rc:%d",
				((hw_type == CAM_ISP_HW_TYPE_SFE) ?
				"SFE" : "VFE"),
				wm_bw_limit_cfg->res_type,
				wm_bw_limit_cfg->enable_limiter,
				wm_bw_limit_cfg->counter_limit[0],
				wm_bw_limit_cfg->counter_limit[1],
				blob_info->base_info->idx, bytes_used, rc);
			return rc;
		}

		total_used_bytes += bytes_used;
	}

	if (total_used_bytes)
		cam_ife_mgr_update_hw_entries_util(
			CAM_ISP_IQ_BL, total_used_bytes, kmd_buf_info, prepare);

	return rc;
}

static inline int cam_isp_validate_bw_limiter_blob(
	uint32_t blob_size,
	struct cam_isp_out_rsrc_bw_limiter_config *bw_limit_config)
{
	if ((bw_limit_config->num_ports >  (max_ife_out_res +
		g_ife_hw_mgr.isp_bus_caps.max_sfe_out_res_type)) ||
		(bw_limit_config->num_ports == 0)) {
		CAM_ERR(CAM_ISP,
			"Invalid num_ports:%u in bw limit config",
			bw_limit_config->num_ports);
			return -EINVAL;
	}

	/* Check for integer overflow */
	if (bw_limit_config->num_ports != 1) {
		if (sizeof(struct cam_isp_wm_bw_limiter_config) > ((UINT_MAX -
			sizeof(struct cam_isp_out_rsrc_bw_limiter_config)) /
			(bw_limit_config->num_ports - 1))) {
			CAM_ERR(CAM_ISP,
				"Max size exceeded in bw limit config num_ports:%u size per port:%lu",
				bw_limit_config->num_ports,
				sizeof(struct cam_isp_wm_bw_limiter_config));
			return -EINVAL;
		}
	}

	if (blob_size < (sizeof(struct cam_isp_out_rsrc_bw_limiter_config) +
		(bw_limit_config->num_ports - 1) *
		sizeof(struct cam_isp_wm_bw_limiter_config))) {
		CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
			blob_size, sizeof(struct cam_isp_out_rsrc_bw_limiter_config)
			+ (bw_limit_config->num_ports - 1) *
			sizeof(struct cam_isp_wm_bw_limiter_config));
		return -EINVAL;
	}

	return 0;
}

static int cam_isp_blob_ife_init_config_update(
	struct cam_hw_prepare_update_args *prepare,
	struct cam_isp_init_config        *init_config)
{
	int i, rc = -EINVAL;
	struct cam_hw_intf                    *hw_intf;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_isp_hw_init_config_update   init_cfg_update;

	ctx = prepare->ctxt_to_hw_map;

	/* Assign init config */
	init_cfg_update.init_config = init_config;
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			if (hw_mgr_res->res_id != CAM_ISP_HW_VFE_IN_CAMIF)
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				init_cfg_update.node_res =
					hw_mgr_res->hw_res[i];
				CAM_DBG(CAM_ISP, "Init config update for res_id: %u",
					hw_mgr_res->res_id);

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_INIT_CONFIG_UPDATE,
					&init_cfg_update,
					sizeof(
					struct cam_isp_hw_init_config_update));
				if (rc)
					CAM_ERR(CAM_ISP, "Init cfg update failed rc: %d", rc);
			}
		}
	}

	return rc;
}

static int cam_isp_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	int rc = 0;
	struct cam_isp_generic_blob_info *blob_info = user_data;
	struct cam_ife_hw_mgr_ctx *ife_mgr_ctx = NULL;
	struct cam_hw_prepare_update_args *prepare = NULL;

	if (!blob_data || (blob_size == 0) || !blob_info) {
		CAM_ERR(CAM_ISP, "Invalid args data %pK size %d info %pK",
			blob_data, blob_size, blob_info);
		return -EINVAL;
	}

	prepare = blob_info->prepare;
	if (!prepare || !prepare->ctxt_to_hw_map) {
		CAM_ERR(CAM_ISP, "Failed. prepare is NULL, blob_type %d",
			blob_type);
		return -EINVAL;
	}

	ife_mgr_ctx = prepare->ctxt_to_hw_map;
	CAM_DBG(CAM_ISP, "Context[%pK][%d] blob_type=%d, blob_size=%d",
		ife_mgr_ctx, ife_mgr_ctx->ctx_index, blob_type, blob_size);

	switch (blob_type) {
	case CAM_ISP_GENERIC_BLOB_TYPE_HFR_CONFIG: {
		struct cam_isp_resource_hfr_config    *hfr_config;

		if (blob_size < sizeof(struct cam_isp_resource_hfr_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		hfr_config = (struct cam_isp_resource_hfr_config *)blob_data;

		if (hfr_config->num_ports > g_ife_hw_mgr.isp_bus_caps.max_vfe_out_res_type ||
			hfr_config->num_ports == 0) {
			CAM_ERR(CAM_ISP, "Invalid num_ports %u in HFR config",
				hfr_config->num_ports);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (hfr_config->num_ports != 1) {
			if (sizeof(struct cam_isp_port_hfr_config) >
				((UINT_MAX -
				sizeof(struct cam_isp_resource_hfr_config)) /
				(hfr_config->num_ports - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in hfr config num_ports:%u size per port:%lu",
					hfr_config->num_ports,
					sizeof(struct cam_isp_port_hfr_config));
				return -EINVAL;
			}
		}

		if (blob_size < (sizeof(struct cam_isp_resource_hfr_config) +
			(hfr_config->num_ports - 1) *
			sizeof(struct cam_isp_port_hfr_config))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(struct cam_isp_resource_hfr_config) +
				(hfr_config->num_ports - 1) *
				sizeof(struct cam_isp_port_hfr_config));
			return -EINVAL;
		}

		rc = cam_isp_blob_hfr_update(blob_type, blob_info,
			hfr_config, prepare, max_ife_out_res,
			CAM_ISP_HW_TYPE_VFE);
		if (rc)
			CAM_ERR(CAM_ISP, "HFR Update Failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_CLOCK_CONFIG: {
		size_t clock_config_size = 0;
		struct cam_isp_clock_config    *clock_config;
		struct cam_isp_prepare_hw_update_data   *prepare_hw_data;

		if (blob_size < sizeof(struct cam_isp_clock_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		clock_config = (struct cam_isp_clock_config *)blob_data;

		if (clock_config->num_rdi > CAM_IFE_RDI_NUM_MAX) {
			CAM_ERR(CAM_ISP, "Invalid num_rdi %u in clock config",
				clock_config->num_rdi);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (clock_config->num_rdi > 1) {
			if (sizeof(uint64_t) > ((UINT_MAX -
				sizeof(struct cam_isp_clock_config)) /
				(clock_config->num_rdi - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in clock config num_rdi:%u size per port:%lu",
					clock_config->num_rdi,
					sizeof(uint64_t));
				return -EINVAL;
			}
		}

		if ((clock_config->num_rdi != 0) && (blob_size <
			(sizeof(struct cam_isp_clock_config) +
			sizeof(uint64_t) * (clock_config->num_rdi - 1)))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(uint32_t) * 2 + sizeof(uint64_t) *
				(clock_config->num_rdi + 2));
			return -EINVAL;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;
		clock_config_size = sizeof(struct cam_isp_clock_config) +
			((clock_config->num_rdi - 1) *
			sizeof(clock_config->rdi_hz));
		memcpy(&prepare_hw_data->bw_clk_config.ife_clock_config, clock_config,
			clock_config_size);
		prepare_hw_data->bw_clk_config.ife_clock_config_valid = true;
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG: {
		struct cam_isp_bw_config    *bw_config;
		struct cam_isp_prepare_hw_update_data   *prepare_hw_data;

		CAM_WARN_RATE_LIMIT_CUSTOM(CAM_PERF, 300, 1,
			"Deprecated Blob TYPE_BW_CONFIG");
		if (blob_size < sizeof(struct cam_isp_bw_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		bw_config = (struct cam_isp_bw_config *)blob_data;

		if (bw_config->num_rdi > CAM_IFE_RDI_NUM_MAX) {
			CAM_ERR(CAM_ISP, "Invalid num_rdi %u in bw config",
				bw_config->num_rdi);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (bw_config->num_rdi > 1) {
			if (sizeof(struct cam_isp_bw_vote) > ((UINT_MAX -
				sizeof(struct cam_isp_bw_config)) /
				(bw_config->num_rdi - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in bw config num_rdi:%u size per port:%lu",
					bw_config->num_rdi,
					sizeof(struct cam_isp_bw_vote));
				return -EINVAL;
			}
		}

		if ((bw_config->num_rdi != 0) && (blob_size <
			(sizeof(struct cam_isp_bw_config) +
			(bw_config->num_rdi - 1) *
			sizeof(struct cam_isp_bw_vote)))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size, sizeof(struct cam_isp_bw_config) +
				(bw_config->num_rdi - 1) *
				sizeof(struct cam_isp_bw_vote));
			return -EINVAL;
		}

		if (!prepare || !prepare->priv ||
			(bw_config->usage_type >= CAM_ISP_HW_USAGE_TYPE_MAX)) {
			CAM_ERR(CAM_ISP, "Invalid inputs usage type %d",
				bw_config->usage_type);
			return -EINVAL;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;

		memcpy(&prepare_hw_data->bw_clk_config.bw_config, bw_config,
			sizeof(prepare_hw_data->bw_clk_config.bw_config));
		ife_mgr_ctx->bw_config_version = CAM_ISP_BW_CONFIG_V1;
		prepare_hw_data->bw_clk_config.bw_config_valid = true;
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG_V2: {
		size_t bw_config_size = 0;
		struct cam_isp_bw_config_v2    *bw_config;
		struct cam_isp_prepare_hw_update_data   *prepare_hw_data;

		if (blob_size < sizeof(struct cam_isp_bw_config_v2)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		bw_config = (struct cam_isp_bw_config_v2 *)blob_data;

		if (bw_config->num_paths > CAM_ISP_MAX_PER_PATH_VOTES ||
			!bw_config->num_paths) {
			CAM_ERR(CAM_ISP, "Invalid num paths %d",
				bw_config->num_paths);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (bw_config->num_paths > 1) {
			if (sizeof(struct cam_axi_per_path_bw_vote) >
				((UINT_MAX -
				sizeof(struct cam_isp_bw_config_v2)) /
				(bw_config->num_paths - 1))) {
				CAM_ERR(CAM_ISP,
					"Size exceeds limit paths:%u size per path:%lu",
					bw_config->num_paths - 1,
					sizeof(
					struct cam_axi_per_path_bw_vote));
				return -EINVAL;
			}
		}

		if ((bw_config->num_paths != 0) && (blob_size <
			(sizeof(struct cam_isp_bw_config_v2) +
			(bw_config->num_paths - 1) *
			sizeof(struct cam_axi_per_path_bw_vote)))) {
			CAM_ERR(CAM_ISP,
				"Invalid blob size: %u, num_paths: %u, bw_config size: %lu, per_path_vote size: %lu",
				blob_size, bw_config->num_paths,
				sizeof(struct cam_isp_bw_config_v2),
				sizeof(struct cam_axi_per_path_bw_vote));
			return -EINVAL;
		}

		if (!prepare || !prepare->priv ||
			(bw_config->usage_type >= CAM_ISP_HW_USAGE_TYPE_MAX)) {
			CAM_ERR(CAM_ISP, "Invalid inputs usage type %d",
				bw_config->usage_type);
			return -EINVAL;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;

		memset(&prepare_hw_data->bw_clk_config.bw_config_v2, 0,
			sizeof(prepare_hw_data->bw_clk_config.bw_config_v2));
		bw_config_size = sizeof(struct cam_isp_bw_config_v2) +
			((bw_config->num_paths - 1) *
			sizeof(struct cam_axi_per_path_bw_vote));
		memcpy(&prepare_hw_data->bw_clk_config.bw_config_v2, bw_config,
			bw_config_size);

		ife_mgr_ctx->bw_config_version = CAM_ISP_BW_CONFIG_V2;
		prepare_hw_data->bw_clk_config.bw_config_valid = true;
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_UBWC_CONFIG: {
		struct cam_ubwc_config *ubwc_config;

		if (blob_size < sizeof(struct cam_ubwc_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob_size %u", blob_size);
			return -EINVAL;
		}

		ubwc_config = (struct cam_ubwc_config *)blob_data;

		if (ubwc_config->num_ports > CAM_VFE_MAX_UBWC_PORTS ||
			ubwc_config->num_ports == 0) {
			CAM_ERR(CAM_ISP, "Invalid num_ports %u in ubwc config",
				ubwc_config->num_ports);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (ubwc_config->num_ports != 1) {
			if (sizeof(struct cam_ubwc_plane_cfg_v1) >
				((UINT_MAX - sizeof(struct cam_ubwc_config)) /
				((ubwc_config->num_ports - 1) * 2))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in ubwc config num_ports:%u size per port:%lu",
					ubwc_config->num_ports,
					sizeof(struct cam_ubwc_plane_cfg_v1) *
					2);
				return -EINVAL;
			}
		}

		if (blob_size < (sizeof(struct cam_ubwc_config) +
			(ubwc_config->num_ports - 1) *
			sizeof(struct cam_ubwc_plane_cfg_v1) * 2)) {
			CAM_ERR(CAM_ISP, "Invalid blob_size %u expected %lu",
				blob_size,
				sizeof(struct cam_ubwc_config) +
				(ubwc_config->num_ports - 1) *
				sizeof(struct cam_ubwc_plane_cfg_v1) * 2);
			return -EINVAL;
		}

		rc = cam_isp_blob_ubwc_update(blob_type, blob_info,
			ubwc_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "UBWC Update Failed rc: %d", rc);
	}
		break;

	case CAM_ISP_GENERIC_BLOB_TYPE_UBWC_CONFIG_V2: {
		struct cam_ubwc_config_v2 *ubwc_config;

		if (blob_size < sizeof(struct cam_ubwc_config_v2)) {
			CAM_ERR(CAM_ISP, "Invalid blob_size %u", blob_size);
			return -EINVAL;
		}

		ubwc_config = (struct cam_ubwc_config_v2 *)blob_data;

		if (ubwc_config->num_ports > CAM_VFE_MAX_UBWC_PORTS ||
			ubwc_config->num_ports == 0) {
			CAM_ERR(CAM_ISP, "Invalid num_ports %u in ubwc config",
				ubwc_config->num_ports);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (ubwc_config->num_ports != 1) {
			if (sizeof(struct cam_ubwc_plane_cfg_v2) >
				((UINT_MAX - sizeof(struct cam_ubwc_config_v2))
				/ ((ubwc_config->num_ports - 1) * 2))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in ubwc config num_ports:%u size per port:%lu",
					ubwc_config->num_ports,
					sizeof(struct cam_ubwc_plane_cfg_v2) *
					2);
				return -EINVAL;
			}
		}

		if (blob_size < (sizeof(struct cam_ubwc_config_v2) +
			(ubwc_config->num_ports - 1) *
			sizeof(struct cam_ubwc_plane_cfg_v2) * 2)) {
			CAM_ERR(CAM_ISP, "Invalid blob_size %u expected %lu",
				blob_size,
				sizeof(struct cam_ubwc_config_v2) +
				(ubwc_config->num_ports - 1) *
				sizeof(struct cam_ubwc_plane_cfg_v2) * 2);
			return -EINVAL;
		}

		rc = cam_isp_blob_ubwc_update_v2(blob_type, blob_info,
			ubwc_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "UBWC Update Failed rc: %d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_CSID_CLOCK_CONFIG: {
		struct cam_isp_csid_clock_config    *clock_config;

		if (blob_size < sizeof(struct cam_isp_csid_clock_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(struct cam_isp_csid_clock_config));
			return -EINVAL;
		}

		clock_config = (struct cam_isp_csid_clock_config *)blob_data;

		rc = cam_isp_blob_csid_clock_update(blob_type, blob_info,
			clock_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "Clock Update Failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_CSID_QCFA_CONFIG: {
		struct cam_isp_csid_qcfa_config *qcfa_config;

		if (blob_size < sizeof(struct cam_isp_csid_qcfa_config)) {
			CAM_ERR(CAM_ISP,
				"Invalid qcfa blob size %u expected %u",
				blob_size,
				sizeof(struct cam_isp_csid_qcfa_config));
			return -EINVAL;
		}

		qcfa_config = (struct cam_isp_csid_qcfa_config *)blob_data;

		rc = cam_isp_blob_csid_qcfa_update(blob_type, blob_info,
				qcfa_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "QCFA Update Failed rc: %d", rc);

	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_FE_CONFIG: {
		struct cam_fe_config *fe_config;

		if (blob_size < sizeof(struct cam_fe_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size, sizeof(struct cam_fe_config));
			return -EINVAL;
		}

		fe_config = (struct cam_fe_config *)blob_data;

		rc = cam_isp_blob_fe_update(blob_type, blob_info,
			fe_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "FS Update Failed rc: %d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_IFE_CORE_CONFIG: {
		struct cam_isp_core_config *core_config;

		if (blob_size < sizeof(struct cam_isp_core_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size, sizeof(struct cam_isp_core_config));
			return -EINVAL;
		}

		core_config = (struct cam_isp_core_config *)blob_data;

		rc = cam_isp_blob_core_cfg_update(blob_type, blob_info,
			core_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "Core cfg update fail: %d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_VFE_OUT_CONFIG: {
		struct cam_isp_vfe_out_config *vfe_out_config;

		if (blob_size < sizeof(struct cam_isp_vfe_out_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u",
				blob_size,
				sizeof(struct cam_isp_vfe_out_config));
			return -EINVAL;
		}

		vfe_out_config = (struct cam_isp_vfe_out_config *)blob_data;

		if (vfe_out_config->num_ports >  max_ife_out_res ||
			vfe_out_config->num_ports == 0) {
			CAM_ERR(CAM_ISP,
				"Invalid num_ports:%u in vfe out config",
				vfe_out_config->num_ports);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (vfe_out_config->num_ports != 1) {
			if (sizeof(struct cam_isp_vfe_wm_config) > ((UINT_MAX -
				sizeof(struct cam_isp_vfe_out_config)) /
				(vfe_out_config->num_ports - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in vfe out config num_ports:%u size per port:%lu",
					vfe_out_config->num_ports,
					sizeof(struct cam_isp_vfe_wm_config));
				return -EINVAL;
			}
		}

		if (blob_size < (sizeof(struct cam_isp_vfe_out_config) +
			(vfe_out_config->num_ports - 1) *
			sizeof(struct cam_isp_vfe_wm_config))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size, sizeof(struct cam_isp_vfe_out_config)
				+ (vfe_out_config->num_ports - 1) *
				sizeof(struct cam_isp_vfe_wm_config));
			return -EINVAL;
		}

		rc = cam_isp_blob_vfe_out_update(blob_type, blob_info,
			vfe_out_config, prepare, max_ife_out_res,
			CAM_ISP_HW_TYPE_VFE);
		if (rc)
			CAM_ERR(CAM_ISP, "VFE out update failed rc: %d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_SENSOR_BLANKING_CONFIG: {
		struct cam_isp_sensor_blanking_config  *sensor_blanking_config;

		if (blob_size < sizeof(struct cam_isp_sensor_blanking_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %zu expected %zu",
				blob_size,
				sizeof(struct cam_isp_sensor_blanking_config));
			return -EINVAL;
		}
		sensor_blanking_config =
			(struct cam_isp_sensor_blanking_config *)blob_data;

		rc = cam_isp_blob_sensor_blanking_config(blob_type, blob_info,
			sensor_blanking_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP,
				"Epoch Configuration Update Failed rc:%d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_DISCARD_INITIAL_FRAMES: {
		struct cam_isp_discard_initial_frames *discard_config;

		if (blob_size < sizeof(struct cam_isp_discard_initial_frames)) {
			CAM_ERR(CAM_ISP,
				"Invalid discard frames blob size %u expected %u",
				blob_size,
				sizeof(struct cam_isp_discard_initial_frames));
			return -EINVAL;
		}

		discard_config = (struct cam_isp_discard_initial_frames *)blob_data;

		rc = cam_isp_blob_csid_discard_init_frame_update(
			blob_info, discard_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "Discard initial frames update failed rc: %d", rc);

	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_CLOCK_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_CORE_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_OUT_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_HFR_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_FE_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_SCRATCH_BUF_CFG:
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_EXP_ORDER_CFG:
	case CAM_ISP_GENERIC_BLOB_TYPE_FPS_CONFIG:
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_DYNAMIC_MODE_SWITCH: {
		struct cam_isp_mode_switch_info    *mup_config;

		if (blob_size < sizeof(struct cam_isp_mode_switch_info)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(struct cam_isp_mode_switch_info));
			return -EINVAL;
		}

		mup_config = (struct cam_isp_mode_switch_info *)blob_data;

		rc = cam_isp_blob_csid_dynamic_switch_update(
			blob_type, blob_info, mup_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "MUP Update Failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_BW_LIMITER_CFG: {
		struct cam_isp_out_rsrc_bw_limiter_config *bw_limit_config;

		if (blob_size <
			sizeof(struct cam_isp_out_rsrc_bw_limiter_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u",
				blob_size,
				sizeof(struct cam_isp_out_rsrc_bw_limiter_config));
			return -EINVAL;
		}

		bw_limit_config =
			(struct cam_isp_out_rsrc_bw_limiter_config *)blob_data;

		rc = cam_isp_validate_bw_limiter_blob(blob_size,
			bw_limit_config);
		if (rc)
			return rc;

		rc = cam_isp_blob_bw_limit_update(blob_type, blob_info,
			bw_limit_config, prepare, CAM_ISP_HW_TYPE_VFE);
		if (rc)
			CAM_ERR(CAM_ISP,
				"BW limit update failed for IFE rc: %d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_INIT_CONFIG: {
		struct cam_isp_init_config            *init_config;
		struct cam_isp_prepare_hw_update_data *prepare_hw_data;

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data *)
			prepare->priv;

		if (prepare_hw_data->packet_opcode_type !=
			CAM_ISP_PACKET_INIT_DEV) {
			CAM_ERR(CAM_ISP,
				"Init config blob not supported for packet type: %u req: %llu",
				prepare_hw_data->packet_opcode_type,
				 prepare->packet->header.request_id);
			return -EINVAL;
		}

		if (blob_size < sizeof(struct cam_isp_init_config)) {
			CAM_ERR(CAM_ISP,
				"Invalid init config blob size %u expected %u",
				blob_size, sizeof(struct cam_isp_init_config));
			return -EINVAL;
		}

		init_config = (struct cam_isp_init_config *)blob_data;
		rc = cam_isp_blob_ife_init_config_update(
			prepare, init_config);
		if (rc)
			CAM_ERR(CAM_ISP,
				"Init config failed for req: %llu rc: %d",
				 prepare->packet->header.request_id, rc);
	}
		break;
	default:
		CAM_WARN(CAM_ISP, "Invalid blob type %d", blob_type);
		break;
	}

	return rc;
}

static int cam_ife_mgr_util_insert_frame_header(
	struct cam_kmd_buf_info *kmd_buf,
	struct cam_isp_prepare_hw_update_data *prepare_hw_data)
{
	int mmu_hdl = -1, rc = 0;
	dma_addr_t iova_addr;
	uint32_t frame_header_iova, padded_bytes = 0;
	size_t len;
	struct cam_ife_hw_mgr *hw_mgr = &g_ife_hw_mgr;

	mmu_hdl = cam_mem_is_secure_buf(
			kmd_buf->handle) ?
			hw_mgr->mgr_common.img_iommu_hdl_secure :
			hw_mgr->mgr_common.img_iommu_hdl;

	rc = cam_mem_get_io_buf(kmd_buf->handle, mmu_hdl,
		&iova_addr, &len, NULL);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Failed to get io addr for handle = %d for mmu_hdl = %u",
			kmd_buf->handle, mmu_hdl);
		return rc;
	}

	/* CDM buffer is within 32-bit address space */
	frame_header_iova = (uint32_t)iova_addr;
	frame_header_iova += kmd_buf->offset;

	/* frame header address needs to be 256 byte aligned */
	if (frame_header_iova % CAM_FRAME_HEADER_ADDR_ALIGNMENT) {
		padded_bytes = (uint32_t)(CAM_FRAME_HEADER_ADDR_ALIGNMENT -
			(frame_header_iova % CAM_FRAME_HEADER_ADDR_ALIGNMENT));
		frame_header_iova += padded_bytes;
	}

	prepare_hw_data->frame_header_iova = frame_header_iova;

	/* update the padding if any for the cpu addr as well */
	prepare_hw_data->frame_header_cpu_addr = kmd_buf->cpu_addr +
			(padded_bytes / 4);

	CAM_DBG(CAM_ISP,
		"Frame Header iova_addr: %pK cpu_addr: %pK padded_bytes: %llu",
		prepare_hw_data->frame_header_iova,
		prepare_hw_data->frame_header_cpu_addr,
		padded_bytes);

	/* Reserve memory for frame header */
	kmd_buf->used_bytes += (padded_bytes + CAM_FRAME_HEADER_BUFFER_SIZE);
	kmd_buf->offset += kmd_buf->used_bytes;

	return rc;
}

static int cam_sfe_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	int rc = 0;
	struct cam_isp_generic_blob_info *blob_info = user_data;
	struct cam_ife_hw_mgr_ctx *ife_mgr_ctx = NULL;
	struct cam_hw_prepare_update_args *prepare = NULL;

	if (!blob_data || (blob_size == 0) || !blob_info) {
		CAM_ERR(CAM_ISP, "Invalid args data %pK size %d info %pK",
			blob_data, blob_size, blob_info);
		return -EINVAL;
	}

	prepare = blob_info->prepare;
	if (!prepare || !prepare->ctxt_to_hw_map) {
		CAM_ERR(CAM_ISP, "Failed. prepare is NULL, blob_type %d",
			blob_type);
		return -EINVAL;
	}

	ife_mgr_ctx = prepare->ctxt_to_hw_map;
	CAM_DBG(CAM_ISP, "Context[%pK][%d] blob_type: %d, blob_size: %d",
		ife_mgr_ctx, ife_mgr_ctx->ctx_index, blob_type, blob_size);

	switch (blob_type) {
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_CLOCK_CONFIG: {
		size_t clock_config_size = 0;
		struct cam_isp_clock_config    *clock_config;
		struct cam_isp_prepare_hw_update_data *prepare_hw_data;

		if (blob_size < sizeof(struct cam_isp_clock_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		clock_config = (struct cam_isp_clock_config *)blob_data;

		if (clock_config->num_rdi > CAM_SFE_RDI_NUM_MAX) {
			CAM_ERR(CAM_ISP, "Invalid num_rdi %u in clock config",
				clock_config->num_rdi);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (clock_config->num_rdi > 1) {
			if (sizeof(uint64_t) > ((UINT_MAX -
				sizeof(struct cam_isp_clock_config)) /
				(clock_config->num_rdi - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in clock config num_rdi:%u size per port:%lu",
					clock_config->num_rdi,
					sizeof(uint64_t));
				return -EINVAL;
			}
		}

		if ((clock_config->num_rdi != 0) && (blob_size <
			(sizeof(struct cam_isp_clock_config) +
			sizeof(uint64_t) * (clock_config->num_rdi - 1)))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(uint32_t) * 2 + sizeof(uint64_t) *
				(clock_config->num_rdi + 2));
			return -EINVAL;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;
		clock_config_size = sizeof(struct cam_isp_clock_config) +
			((clock_config->num_rdi - 1) *
			sizeof(clock_config->rdi_hz));
		memcpy(&prepare_hw_data->bw_clk_config.sfe_clock_config, clock_config,
			clock_config_size);
		prepare_hw_data->bw_clk_config.sfe_clock_config_valid = true;
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_OUT_CONFIG: {
		struct cam_isp_vfe_out_config *vfe_out_config;

		if (blob_size < sizeof(struct cam_isp_vfe_out_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u",
				blob_size,
				sizeof(struct cam_isp_vfe_out_config));
			return -EINVAL;
		}

		vfe_out_config = (struct cam_isp_vfe_out_config *)blob_data;


		if (vfe_out_config->num_ports > CAM_SFE_HW_OUT_RES_MAX ||
			vfe_out_config->num_ports == 0) {
			CAM_ERR(CAM_ISP,
				"Invalid num_ports:%u in sfe out config",
				vfe_out_config->num_ports,
				CAM_SFE_HW_OUT_RES_MAX);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (vfe_out_config->num_ports != 1) {
			if (sizeof(struct cam_isp_vfe_wm_config) > ((UINT_MAX -
				sizeof(struct cam_isp_vfe_out_config)) /
				(vfe_out_config->num_ports - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in sfe out config num_ports:%u size per port:%lu",
					vfe_out_config->num_ports,
					sizeof(struct cam_isp_vfe_wm_config));
				return -EINVAL;
			}
		}

		if (blob_size < (sizeof(struct cam_isp_vfe_out_config) +
			(vfe_out_config->num_ports - 1) *
			sizeof(struct cam_isp_vfe_wm_config))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size, sizeof(struct cam_isp_vfe_out_config)
				+ (vfe_out_config->num_ports - 1) *
				sizeof(struct cam_isp_vfe_wm_config));
			return -EINVAL;
		}

		rc = cam_isp_blob_vfe_out_update(blob_type, blob_info,
			vfe_out_config, prepare, CAM_SFE_HW_OUT_RES_MAX,
			CAM_ISP_HW_TYPE_SFE);
		if (rc)
			CAM_ERR(CAM_ISP, "SFE out update failed rc: %d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_HFR_CONFIG: {
		struct cam_isp_resource_hfr_config    *hfr_config;

		if (blob_size < sizeof(struct cam_isp_resource_hfr_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		hfr_config = (struct cam_isp_resource_hfr_config *)blob_data;

		if (hfr_config->num_ports > CAM_ISP_SFE_OUT_RES_MAX ||
			hfr_config->num_ports == 0) {
			CAM_ERR(CAM_ISP, "Invalid num_ports %u in HFR config",
				hfr_config->num_ports);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (hfr_config->num_ports != 1) {
			if (sizeof(struct cam_isp_port_hfr_config) >
				((UINT_MAX -
				sizeof(struct cam_isp_resource_hfr_config)) /
				(hfr_config->num_ports - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in hfr config num_ports:%u size per port:%lu",
					hfr_config->num_ports,
					sizeof(struct cam_isp_port_hfr_config));
				return -EINVAL;
			}
		}

		if (blob_size < (sizeof(struct cam_isp_resource_hfr_config) +
			(hfr_config->num_ports - 1) *
			sizeof(struct cam_isp_port_hfr_config))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(struct cam_isp_resource_hfr_config) +
				(hfr_config->num_ports - 1) *
				sizeof(struct cam_isp_port_hfr_config));
			return -EINVAL;
		}

		rc = cam_isp_blob_hfr_update(blob_type, blob_info,
			hfr_config, prepare, CAM_SFE_HW_OUT_RES_MAX,
			CAM_ISP_HW_TYPE_SFE);
		if (rc)
			CAM_ERR(CAM_ISP, "HFR Update Failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_CORE_CONFIG: {
		struct cam_isp_sfe_core_config *core_cfg;

		if (blob_size < sizeof(struct cam_isp_sfe_core_config)) {
			CAM_ERR(CAM_ISP,
				"Invalid blob size: %u expected: %u", blob_size,
				sizeof(struct cam_isp_sfe_core_config));
			return -EINVAL;
		}

		core_cfg = (struct cam_isp_sfe_core_config *)blob_data;
		rc = cam_isp_blob_sfe_core_cfg_update(blob_type, blob_info,
			core_cfg, prepare);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_SCRATCH_BUF_CFG: {
		struct cam_isp_sfe_init_scratch_buf_config *scratch_config;

		if (!(ife_mgr_ctx->flags.is_sfe_fs ||
			ife_mgr_ctx->flags.is_sfe_shdr)) {
			CAM_ERR(CAM_ISP,
				"Not SFE sHDR/FS context: %u scratch buf blob not supported",
				ife_mgr_ctx->ctx_index);
			return -EINVAL;
		}

		if (blob_size <
			sizeof(struct cam_isp_sfe_init_scratch_buf_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		scratch_config =
			(struct cam_isp_sfe_init_scratch_buf_config *)blob_data;

		if (scratch_config->num_ports > CAM_SFE_FE_RDI_NUM_MAX ||
			scratch_config->num_ports == 0) {
			CAM_ERR(CAM_ISP,
				"Invalid num_ports %u in scratch buf config",
				scratch_config->num_ports);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (scratch_config->num_ports != 1) {
			if (sizeof(struct cam_isp_sfe_scratch_buf_info) >
				((UINT_MAX -
				sizeof(
				struct cam_isp_sfe_init_scratch_buf_config)) /
				(scratch_config->num_ports - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in scratch config num_ports: %u size per port: %lu",
					scratch_config->num_ports,
					sizeof(
					struct cam_isp_sfe_scratch_buf_info));
				return -EINVAL;
			}
		}

		if (blob_size <
			(sizeof(struct cam_isp_sfe_init_scratch_buf_config) +
			(scratch_config->num_ports - 1) *
			sizeof(struct cam_isp_sfe_scratch_buf_info))) {
			CAM_ERR(CAM_ISP, "Invalid blob size: %u expected: %lu",
				blob_size,
				sizeof(
				struct cam_isp_sfe_init_scratch_buf_config) +
				(scratch_config->num_ports - 1) *
				sizeof(
				struct cam_isp_sfe_scratch_buf_info));
			return -EINVAL;
		}

		rc = cam_isp_blob_sfe_scratch_buf_update(
			scratch_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "SFE scratch buffer update failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_FE_CONFIG: {
		struct cam_fe_config *fe_config;

		if (blob_size < sizeof(struct cam_fe_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size, sizeof(struct cam_fe_config));
			return -EINVAL;
		}

		fe_config = (struct cam_fe_config *)blob_data;

		rc = cam_isp_blob_fe_update(blob_type, blob_info,
			fe_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "FS Update Failed rc: %d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_DYNAMIC_MODE_SWITCH: {
		struct cam_isp_mode_switch_info       *mup_config;
		struct cam_isp_prepare_hw_update_data *prepare_hw_data;

		if (blob_size < sizeof(struct cam_isp_mode_switch_info)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(struct cam_isp_mode_switch_info));
			return -EINVAL;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;
		mup_config = (struct cam_isp_mode_switch_info *)blob_data;
		if (ife_mgr_ctx->flags.is_sfe_shdr) {
			ife_mgr_ctx->sfe_info.scratch_config->curr_num_exp =
				mup_config->num_expoures;
			prepare_hw_data->num_exp = mup_config->num_expoures;

			rc = cam_isp_blob_sfe_update_fetch_core_cfg(
				blob_type, blob_info, prepare);
			if (rc)
				CAM_ERR(CAM_ISP,
					"SFE dynamic enable/disable for fetch failed");
		}
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_SFE_EXP_ORDER_CFG: {
		struct cam_isp_sfe_exp_config *exp_config;

		if (!ife_mgr_ctx->flags.is_sfe_shdr) {
			CAM_ERR(CAM_ISP,
				"Blob %u supported only for sHDR streams",
				blob_type);
			return -EINVAL;
		}

		if (blob_size <
			sizeof(struct cam_isp_sfe_exp_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		exp_config =
			(struct cam_isp_sfe_exp_config *)blob_data;

		if ((exp_config->num_ports > CAM_SFE_FE_RDI_NUM_MAX) ||
			(exp_config->num_ports == 0)) {
			CAM_ERR(CAM_ISP,
				"Invalid num_ports %u in exp order config",
				exp_config->num_ports);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (exp_config->num_ports != 1) {
			if (sizeof(struct cam_isp_sfe_wm_exp_order_config) >
				((UINT_MAX -
				sizeof(
				struct cam_isp_sfe_exp_config)) /
				(exp_config->num_ports - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in exp order config num_ports: %u size per port: %lu",
					exp_config->num_ports,
					sizeof(
					struct cam_isp_sfe_wm_exp_order_config));
				return -EINVAL;
			}
		}

		if (blob_size <
			(sizeof(struct cam_isp_sfe_exp_config) +
			(exp_config->num_ports - 1) *
			sizeof(struct cam_isp_sfe_wm_exp_order_config))) {
			CAM_ERR(CAM_ISP, "Invalid blob size: %u expected: %lu",
				blob_size,
				sizeof(
				struct cam_isp_sfe_exp_config) +
				(exp_config->num_ports - 1) *
				sizeof(
				struct cam_isp_sfe_wm_exp_order_config));
			return -EINVAL;
		}

		rc = cam_isp_blob_sfe_exp_order_update(
			blob_info->base_info->idx, exp_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "SFE exp order update failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_BW_LIMITER_CFG: {
		struct cam_isp_out_rsrc_bw_limiter_config *bw_limit_config;

		if (blob_size <
			sizeof(struct cam_isp_out_rsrc_bw_limiter_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u",
				blob_size,
				sizeof(struct cam_isp_out_rsrc_bw_limiter_config));
			return -EINVAL;
		}

		bw_limit_config =
			(struct cam_isp_out_rsrc_bw_limiter_config *)blob_data;

		rc = cam_isp_validate_bw_limiter_blob(blob_size,
			bw_limit_config);
		if (rc)
			return rc;

		rc = cam_isp_blob_bw_limit_update(blob_type, blob_info,
			bw_limit_config, prepare, CAM_ISP_HW_TYPE_SFE);
		if (rc)
			CAM_ERR(CAM_ISP,
				"BW limit update failed for SFE rc: %d", rc);
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_HFR_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_CLOCK_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_UBWC_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_CSID_CLOCK_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_FE_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_UBWC_CONFIG_V2:
	case CAM_ISP_GENERIC_BLOB_TYPE_IFE_CORE_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_VFE_OUT_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG_V2:
	case CAM_ISP_GENERIC_BLOB_TYPE_CSID_QCFA_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_SENSOR_BLANKING_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_DISCARD_INITIAL_FRAMES:
	case CAM_ISP_GENERIC_BLOB_TYPE_INIT_CONFIG:
	case CAM_ISP_GENERIC_BLOB_TYPE_FPS_CONFIG:
		break;
	default:
		CAM_WARN(CAM_ISP, "Invalid blob type: %u", blob_type);
		break;
	}

	return rc;
}

static inline bool cam_isp_sfe_validate_for_scratch_buf_config(
	uint32_t res_idx, struct cam_ife_hw_mgr_ctx  *ctx,
	bool default_settings)
{
	uint32_t curr_num_exp;

	/* check for num exposures for static mode but using RDI1-2 without RD1-2 */
	if (res_idx >= ctx->sfe_info.num_fetches)
		return true;

	if (default_settings)
		curr_num_exp = ctx->curr_num_exp;
	else
		curr_num_exp = ctx->sfe_info.scratch_config->curr_num_exp;

	/* check for num exposures for dynamic mode */
	if ((ctx->ctx_config &
		CAM_IFE_CTX_CFG_DYNAMIC_SWITCH_ON) &&
		(res_idx >= curr_num_exp))
		return true;

	return false;
}

static int cam_isp_sfe_send_scratch_buf_upd(
	uint32_t                         remaining_size,
	enum cam_isp_hw_cmd_type         cmd_type,
	struct cam_isp_resource_node    *hw_res,
	struct cam_sfe_scratch_buf_info *buf_info,
	uint32_t                        *cpu_addr,
	uint32_t                        *used_bytes)
{
	int rc;
	struct cam_isp_hw_get_cmd_update   update_buf;
	struct cam_isp_hw_get_wm_update    wm_update;
	dma_addr_t                         io_addr[CAM_PACKET_MAX_PLANES];

	memset(io_addr, 0, sizeof(io_addr));
	update_buf.res = hw_res;
	update_buf.cmd_type = cmd_type;
	update_buf.cmd.cmd_buf_addr = cpu_addr;
	update_buf.use_scratch_cfg = true;

	wm_update.num_buf = 1;
	io_addr[0] = buf_info->io_addr;
	wm_update.image_buf = io_addr;
	wm_update.width = buf_info->width;
	wm_update.height = buf_info->height;
	wm_update.stride = buf_info->stride;
	wm_update.slice_height = buf_info->slice_height;
	wm_update.io_cfg = NULL;

	update_buf.wm_update = &wm_update;
	update_buf.cmd.size = remaining_size;

	rc = hw_res->hw_intf->hw_ops.process_cmd(
		hw_res->hw_intf->hw_priv,
		cmd_type, &update_buf,
		sizeof(struct cam_isp_hw_get_cmd_update));
	if (rc) {
		CAM_ERR(CAM_ISP, "get buf cmd error:%d",
			hw_res->res_id);
		rc = -ENOMEM;
		return rc;
	}

	CAM_DBG(CAM_ISP, "Scratch buf configured for: 0x%x",
		hw_res->res_id);

	/* Update used bytes if update is via CDM */
	if ((cmd_type == CAM_ISP_HW_CMD_GET_BUF_UPDATE) ||
		(cmd_type == CAM_ISP_HW_CMD_GET_BUF_UPDATE_RM))
		*used_bytes = update_buf.cmd.used_bytes;

	return rc;
}

static int cam_isp_sfe_add_scratch_buffer_cfg(
	uint32_t                              base_idx,
	uint32_t                              sfe_rdi_cfg_mask,
	struct cam_hw_prepare_update_args    *prepare,
	struct cam_kmd_buf_info              *kmd_buf_info,
	struct cam_isp_hw_mgr_res            *res_list_isp_out,
	struct list_head                     *res_list_in_rd,
	struct cam_ife_hw_mgr_ctx            *ctx)
{
	int i, j, res_id, rc = 0;
	uint32_t used_bytes = 0, remain_size = 0;
	uint32_t io_cfg_used_bytes;
	uint32_t *cpu_addr = NULL;
	struct cam_sfe_scratch_buf_info   *buf_info;
	struct cam_isp_hw_mgr_res         *hw_mgr_res;

	if (prepare->num_hw_update_entries + 1 >=
			prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient  HW entries :%d %d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		return -EINVAL;
	}

	io_cfg_used_bytes = 0;
	CAM_DBG(CAM_ISP, "num_ports: %u",
		ctx->sfe_info.scratch_config->num_config);

	/* Update RDI WMs */
	for (i = 0; i < CAM_SFE_FE_RDI_NUM_MAX; i++) {
		hw_mgr_res = &res_list_isp_out[i];
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			if ((kmd_buf_info->used_bytes + io_cfg_used_bytes) <
				kmd_buf_info->size) {
				remain_size = kmd_buf_info->size -
					(kmd_buf_info->used_bytes +
					io_cfg_used_bytes);
			} else {
				CAM_ERR(CAM_ISP,
					"no free kmd memory for base %d",
					base_idx);
				rc = -ENOMEM;
				return rc;
			}

			res_id = hw_mgr_res->hw_res[j]->res_id;

			if (cam_isp_sfe_validate_for_scratch_buf_config(
				(res_id - CAM_ISP_SFE_OUT_RES_RDI_0), ctx, false))
				continue;

			/* check if buffer provided for this RDI is from userspace */
			if (sfe_rdi_cfg_mask & (1 << (res_id - CAM_ISP_SFE_OUT_RES_RDI_0)))
				continue;

			cpu_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes / 4 + io_cfg_used_bytes / 4;
			buf_info = &ctx->sfe_info.scratch_config->buf_info[
				res_id - CAM_ISP_SFE_OUT_RES_RDI_0];

			/* Check if scratch available for this resource */
			if (!buf_info->config_done) {
				CAM_ERR(CAM_ISP,
					"No scratch buffer config found for res: %u on ctx: %u",
					res_id, ctx->ctx_index);
				return -EFAULT;
			}

			CAM_DBG(CAM_ISP, "WM res_id: 0x%x idx: %u io_addr: %pK",
				hw_mgr_res->hw_res[j]->res_id,
				(res_id - CAM_ISP_SFE_OUT_RES_RDI_0),
				buf_info->io_addr);

			rc = cam_isp_sfe_send_scratch_buf_upd(
				remain_size,
				CAM_ISP_HW_CMD_GET_BUF_UPDATE,
				hw_mgr_res->hw_res[j], buf_info,
				cpu_addr, &used_bytes);
			if (rc)
				return rc;

			io_cfg_used_bytes += used_bytes;
		}
	}

	/* Update RDI RMs */
	list_for_each_entry(hw_mgr_res, res_list_in_rd, list) {
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			if ((kmd_buf_info->used_bytes + io_cfg_used_bytes) <
				kmd_buf_info->size) {
				remain_size = kmd_buf_info->size -
					(kmd_buf_info->used_bytes +
					io_cfg_used_bytes);
			} else {
				CAM_ERR(CAM_ISP,
					"no free kmd memory for base %d",
					base_idx);
				rc = -ENOMEM;
				return rc;
			}

			res_id = hw_mgr_res->hw_res[j]->res_id;

			if (cam_isp_sfe_validate_for_scratch_buf_config(
				(res_id - CAM_ISP_HW_SFE_IN_RD0), ctx, false))
				continue;

			/* check if buffer provided for this RM is from userspace */
			if (sfe_rdi_cfg_mask & (1 << (res_id - CAM_ISP_HW_SFE_IN_RD0)))
				continue;

			cpu_addr = kmd_buf_info->cpu_addr +
				kmd_buf_info->used_bytes  / 4 +
				io_cfg_used_bytes / 4;
			buf_info = &ctx->sfe_info.scratch_config->buf_info[
				res_id - CAM_ISP_HW_SFE_IN_RD0];

			CAM_DBG(CAM_ISP, "RM res_id: 0x%x idx: %u io_addr: %pK",
				hw_mgr_res->hw_res[j]->res_id,
				(res_id - CAM_ISP_HW_SFE_IN_RD0),
				buf_info->io_addr);

			rc = cam_isp_sfe_send_scratch_buf_upd(remain_size,
				CAM_ISP_HW_CMD_GET_BUF_UPDATE_RM,
				hw_mgr_res->hw_res[j], buf_info,
				cpu_addr, &used_bytes);
			if (rc)
				return rc;

			io_cfg_used_bytes += used_bytes;
		}
	}

	if (io_cfg_used_bytes)
		cam_ife_mgr_update_hw_entries_util(
			CAM_ISP_IOCFG_BL, io_cfg_used_bytes, kmd_buf_info, prepare);

	return rc;
}

static int cam_ife_mgr_csid_add_reg_update(struct cam_ife_hw_mgr_ctx *ctx,
	struct cam_hw_prepare_update_args *prepare,
	struct cam_kmd_buf_info *kmd_buf)
{
	int                                   i;
	int                                   rc = 0;
	uint32_t                              hw_idx;
	struct cam_ife_hw_mgr                *hw_mgr;
	struct cam_isp_hw_mgr_res            *hw_mgr_res;
	struct cam_ife_csid_hw_caps          *csid_caps;
	struct cam_isp_resource_node         *res;
	struct cam_isp_change_base_args       change_base_info = {0};
	struct cam_isp_csid_reg_update_args
			rup_args[CAM_IFE_CSID_HW_NUM_MAX]  = {0};

	hw_mgr = ctx->hw_mgr;
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {

		if (hw_mgr_res->res_type == CAM_ISP_RESOURCE_UNINT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			res = hw_mgr_res->hw_res[i];
			hw_idx = res->hw_intf->hw_idx;
			csid_caps = &hw_mgr->csid_hw_caps[hw_idx];

			if (i == CAM_ISP_HW_SPLIT_RIGHT &&
				csid_caps->only_master_rup)
				continue;

			rup_args[hw_idx].res[rup_args[hw_idx].num_res] = res;
			rup_args[hw_idx].num_res++;

			CAM_DBG(CAM_ISP,
				"Reg update queued for res %d hw_id %d",
				res->res_id, res->hw_intf->hw_idx);
		}
	}

	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (!rup_args[i].num_res)
			continue;

		change_base_info.base_idx = i;
		change_base_info.cdm_id = ctx->cdm_id;
		rc = cam_isp_add_change_base(prepare,
			&ctx->res_list_ife_csid,
			&change_base_info, kmd_buf);

		CAM_DBG(CAM_ISP, "Ctx:%d Change base added for num_res %d",
			ctx->ctx_index, rup_args[i].num_res);

		if (rc) {
			CAM_ERR(CAM_ISP,
				"Change base Failed Ctx:%d hw_idx=%d, rc=%d",
				ctx->ctx_index, i, rc);
			break;
		}

		rc = cam_isp_add_csid_reg_update(prepare, kmd_buf,
			&rup_args[i]);

		if (rc) {
			CAM_ERR(CAM_ISP, "Ctx:%u Reg Update failed idx:%u",
				ctx->ctx_index, i);
			break;
		}

		CAM_DBG(CAM_ISP, "Ctx:%d Reg update added id:%d num_res %d",
			ctx->ctx_index, i, rup_args[i].num_res);
	}

	return rc;
}

static int cam_ife_mgr_isp_add_reg_update(struct cam_ife_hw_mgr_ctx *ctx,
	struct cam_hw_prepare_update_args *prepare,
	struct cam_kmd_buf_info *kmd_buf)
{
	int i;
	int rc = 0;
	struct cam_isp_change_base_args   change_base_info = {0};

	for (i = 0; i < ctx->num_base; i++) {

		change_base_info.base_idx = ctx->base[i].idx;
		change_base_info.cdm_id = ctx->cdm_id;

		/* Add change base */
		if (!ctx->flags.internal_cdm) {
			rc = cam_isp_add_change_base(prepare,
				&ctx->res_list_ife_src,
				&change_base_info, kmd_buf);

			if (rc) {
				CAM_ERR(CAM_ISP,
					"Add Change base cmd Failed i=%d, idx=%d, rc=%d",
					i, ctx->base[i].idx, rc);
				break;
			}

			CAM_DBG(CAM_ISP,
				"Add Change base cmd i=%d, idx=%d, rc=%d",
				i, ctx->base[i].idx, rc);
		}

		rc = cam_isp_add_reg_update(prepare,
			&ctx->res_list_ife_src,
			ctx->base[i].idx, kmd_buf);

		if (rc) {
			CAM_ERR(CAM_ISP,
				"Add Reg Update cmd Failed i=%d, idx=%d, rc=%d",
				i, ctx->base[i].idx, rc);
			break;
		}

		CAM_DBG(CAM_ISP,
			"Add Reg Update cmd i=%d, idx=%d, rc=%d",
			i, ctx->base[i].idx, rc);
	}
	return rc;
}

static int cam_ife_hw_mgr_update_cmd_buffer(
	struct cam_ife_hw_mgr_ctx               *ctx,
	struct cam_hw_prepare_update_args       *prepare,
	struct cam_kmd_buf_info                 *kmd_buf,
	struct cam_isp_cmd_buf_count            *cmd_buf_count,
	uint32_t                                 base_idx)
{
	struct list_head                     *res_list = NULL;
	struct cam_isp_change_base_args       change_base_info = {0};
	int                                   rc = 0;

	if (ctx->base[base_idx].hw_type == CAM_ISP_HW_TYPE_SFE) {
		res_list = &ctx->res_list_sfe_src;
	} else if (ctx->base[base_idx].hw_type == CAM_ISP_HW_TYPE_VFE) {
		res_list = &ctx->res_list_ife_src;
	}else if (ctx->base[base_idx].hw_type == CAM_ISP_HW_TYPE_CSID) {
		if (!cmd_buf_count->csid_cnt)
			return rc;
		res_list = &ctx->res_list_ife_csid;
	}else {
		CAM_ERR(CAM_ISP,
			"Invalide hw_type=%d", ctx->base[base_idx].hw_type);
		return -EINVAL;
	}

	if (!ctx->flags.internal_cdm) {
		change_base_info.base_idx = ctx->base[base_idx].idx;
		change_base_info.cdm_id = ctx->cdm_id;
		rc = cam_isp_add_change_base(prepare,
			res_list,
			&change_base_info, kmd_buf);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed change base, i=%d, split_id=%d, hw_type=%d",
				base_idx, ctx->base[base_idx].split_id,
				ctx->base[base_idx].hw_type);
			return rc;
		}

		CAM_DBG(CAM_ISP,
			"changing the base hw_type: %u core_id: %u CDM ID: %d",
			ctx->base[base_idx].hw_type, ctx->base[base_idx].idx,
			ctx->cdm_id);
	}

	if (ctx->base[base_idx].hw_type == CAM_ISP_HW_TYPE_SFE)
		rc = cam_sfe_add_command_buffers(
			prepare, kmd_buf, &ctx->base[base_idx],
			cam_sfe_packet_generic_blob_handler,
			ctx->res_list_sfe_out,
			CAM_ISP_SFE_OUT_RES_BASE,
			CAM_ISP_SFE_OUT_RES_MAX);
	else if (ctx->base[base_idx].hw_type == CAM_ISP_HW_TYPE_VFE)
		rc = cam_isp_add_command_buffers(
			prepare, kmd_buf, &ctx->base[base_idx],
			cam_isp_packet_generic_blob_handler,
			ctx->res_list_ife_out,
			CAM_ISP_IFE_OUT_RES_BASE,
			(CAM_ISP_IFE_OUT_RES_BASE +
			max_ife_out_res));
	else if (ctx->base[base_idx].hw_type == CAM_ISP_HW_TYPE_CSID)
		rc = cam_isp_add_csid_command_buffers(prepare,
			kmd_buf, &ctx->base[base_idx]);

	CAM_DBG(CAM_ISP,
		"Add cmdbuf, i=%d, split_id=%d, hw_type=%d",
		base_idx, ctx->base[base_idx].split_id,
		ctx->base[base_idx].hw_type);

	if (rc)
		CAM_ERR(CAM_ISP,
			"Failed in add cmdbuf, i=%d, split_id=%d, rc=%d hw_type=%d",
			base_idx, ctx->base[base_idx].split_id, rc,
			ctx->base[base_idx].hw_type);
	return rc;
}

static int cam_ife_mgr_prepare_hw_update(void *hw_mgr_priv,
	void *prepare_hw_update_args)
{
	int rc = 0;
	struct cam_hw_prepare_update_args *prepare =
		(struct cam_hw_prepare_update_args *) prepare_hw_update_args;

	struct cam_ife_hw_mgr_ctx               *ctx;
	struct cam_ife_hw_mgr                   *hw_mgr;
	struct cam_kmd_buf_info                  kmd_buf;
	uint32_t                                 i;
	bool                                     fill_ife_fence = true;
	bool                                     fill_sfe_fence = true;
	bool                                     frame_header_enable = false;
	struct cam_isp_prepare_hw_update_data   *prepare_hw_data;
	struct cam_isp_frame_header_info         frame_header_info;
	struct list_head                        *res_list_ife_rd_tmp = NULL;
	struct cam_isp_check_sfe_fe_io_cfg       sfe_fe_chk_cfg;
	struct cam_isp_cmd_buf_count             cmd_buf_count = {0};

	if (!hw_mgr_priv || !prepare_hw_update_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
		prepare->priv;

	ctx = (struct cam_ife_hw_mgr_ctx *) prepare->ctxt_to_hw_map;
	hw_mgr = (struct cam_ife_hw_mgr *)hw_mgr_priv;


	CAM_DBG(CAM_REQ, "ctx[%pK][%d] Enter for req_id %lld",
		ctx, ctx->ctx_index, prepare->packet->header.request_id);

	rc = cam_packet_util_validate_packet(prepare->packet,
		prepare->remain_len);
	if (rc)
		return rc;

	/* Pre parse the packet*/
	rc = cam_packet_util_get_kmd_buffer(prepare->packet, &kmd_buf);
	if (rc)
		return rc;

	if (ctx->ctx_config & CAM_IFE_CTX_CFG_FRAME_HEADER_TS) {
		rc = cam_ife_mgr_util_insert_frame_header(&kmd_buf,
			prepare_hw_data);
		if (rc)
			return rc;

		frame_header_enable = true;
		prepare_hw_data->frame_header_res_id = 0x0;
	}

	if (ctx->flags.internal_cdm)
		rc = cam_packet_util_process_patches(prepare->packet,
			hw_mgr->mgr_common.img_iommu_hdl,
			hw_mgr->mgr_common.img_iommu_hdl_secure);
	else
		rc = cam_packet_util_process_patches(prepare->packet,
			hw_mgr->mgr_common.cmd_iommu_hdl,
			hw_mgr->mgr_common.cmd_iommu_hdl_secure);

	if (rc) {
		CAM_ERR(CAM_ISP, "Patch ISP packet failed.");
		return rc;
	}

	prepare->num_hw_update_entries = 0;
	prepare->num_in_map_entries = 0;
	prepare->num_out_map_entries = 0;
	prepare->num_reg_dump_buf = 0;

	/* Assign IFE RD for non SFE targets */
	if (ctx->ctx_type != CAM_IFE_CTX_TYPE_SFE)
		res_list_ife_rd_tmp = &ctx->res_list_ife_in_rd;

	rc = cam_isp_get_cmd_buf_count(prepare, &cmd_buf_count);

	if (rc) {
		CAM_ERR(CAM_ISP, "Invalid cmd buffers");
		return rc;
	}

	if (((prepare->packet->header.op_code + 1) & 0xF) ==
		CAM_ISP_PACKET_INIT_DEV)
		prepare_hw_data->packet_opcode_type = CAM_ISP_PACKET_INIT_DEV;
	else
		prepare_hw_data->packet_opcode_type = CAM_ISP_PACKET_UPDATE_DEV;

	for (i = 0; i < ctx->num_base; i++) {

		memset(&frame_header_info, 0,
			sizeof(struct cam_isp_frame_header_info));
		if (frame_header_enable) {
			frame_header_info.frame_header_enable = true;
			frame_header_info.frame_header_iova_addr =
				prepare_hw_data->frame_header_iova;
		}

		rc = cam_ife_hw_mgr_update_cmd_buffer(ctx, prepare,
			&kmd_buf, &cmd_buf_count, i);

		if (rc) {
			CAM_ERR(CAM_ISP, "Add cmd buffer failed base_idx: %d hw_type %d",
				i, ctx->base[i].hw_type);
			goto end;
		}

		sfe_fe_chk_cfg.sfe_fe_enabled = false;
		if ((ctx->base[i].hw_type == CAM_ISP_HW_TYPE_SFE) &&
			((ctx->flags.is_sfe_fs) || (ctx->flags.is_sfe_shdr))) {
			sfe_fe_chk_cfg.sfe_fe_enabled = true;
			sfe_fe_chk_cfg.sfe_rdi_cfg_mask = 0;
			sfe_fe_chk_cfg.num_active_fe_rdis =
				ctx->sfe_info.num_fetches;
		}

		/* get IO buffers */
		if (ctx->base[i].hw_type == CAM_ISP_HW_TYPE_VFE)
			rc = cam_isp_add_io_buffers(
				hw_mgr->mgr_common.img_iommu_hdl,
				hw_mgr->mgr_common.img_iommu_hdl_secure,
				prepare, ctx->base[i].idx,
				&kmd_buf, ctx->res_list_ife_out,
				res_list_ife_rd_tmp,
				CAM_ISP_IFE_OUT_RES_BASE,
				(CAM_ISP_IFE_OUT_RES_BASE + max_ife_out_res),
				fill_ife_fence,
				CAM_ISP_HW_TYPE_VFE, &frame_header_info,
				&sfe_fe_chk_cfg);
		else if (ctx->base[i].hw_type == CAM_ISP_HW_TYPE_SFE)
			rc = cam_isp_add_io_buffers(
				hw_mgr->mgr_common.img_iommu_hdl,
				hw_mgr->mgr_common.img_iommu_hdl_secure,
				prepare, ctx->base[i].idx,
				&kmd_buf, ctx->res_list_sfe_out,
				&ctx->res_list_ife_in_rd,
				CAM_ISP_SFE_OUT_RES_BASE,
				CAM_ISP_SFE_OUT_RES_MAX, fill_sfe_fence,
				CAM_ISP_HW_TYPE_SFE, &frame_header_info,
				&sfe_fe_chk_cfg);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed in io buffers, i=%d, rc=%d hw_type=%s",
				i, rc,
				(ctx->base[i].hw_type == CAM_ISP_HW_TYPE_SFE ? "SFE" : "IFE"));
			goto end;
		}

		/* fence map table entries need to fill only once in the loop */
		if ((ctx->base[i].hw_type ==
			CAM_ISP_HW_TYPE_SFE) &&
			fill_sfe_fence)
			fill_sfe_fence = false;
		else if ((ctx->base[i].hw_type ==
			CAM_ISP_HW_TYPE_VFE) &&
			fill_ife_fence)
			fill_ife_fence = false;

		/*
		 * Add scratch buffer if there no output buffer for RDI WMs/RMs
		 * only for UPDATE packets. For INIT we could have ePCR enabled
		 * based on that decide to configure scratch via AHB at
		 * stream on or not
		 */
		if (sfe_fe_chk_cfg.sfe_fe_enabled) {
			if ((sfe_fe_chk_cfg.sfe_rdi_cfg_mask) !=
				((1 << ctx->sfe_info.num_fetches) - 1)) {
				if (prepare_hw_data->packet_opcode_type ==
					CAM_ISP_PACKET_UPDATE_DEV) {
					CAM_DBG(CAM_ISP,
						"Adding scratch buffer cfg_mask expected: 0x%x actual: 0x%x",
						((1 << ctx->sfe_info.num_fetches) - 1),
						sfe_fe_chk_cfg.sfe_rdi_cfg_mask);
					rc = cam_isp_sfe_add_scratch_buffer_cfg(
						ctx->base[i].idx, sfe_fe_chk_cfg.sfe_rdi_cfg_mask,
						prepare, &kmd_buf, ctx->res_list_sfe_out,
						&ctx->res_list_ife_in_rd, ctx);
					if (rc)
						goto end;
				}
			} else {
				if (prepare_hw_data->packet_opcode_type == CAM_ISP_PACKET_INIT_DEV)
					ctx->sfe_info.skip_scratch_cfg_streamon = true;
			}
		}

		memset(&sfe_fe_chk_cfg, 0, sizeof(sfe_fe_chk_cfg));
		if (frame_header_info.frame_header_res_id &&
			frame_header_enable) {
			frame_header_enable = false;
			prepare_hw_data->frame_header_res_id =
				frame_header_info.frame_header_res_id;

			CAM_DBG(CAM_ISP,
				"Frame header enabled for res_id 0x%x cpu_addr %pK",
				prepare_hw_data->frame_header_res_id,
				prepare_hw_data->frame_header_cpu_addr);
		}
	}

	/* Check if frame header was enabled for any WM */
	if ((ctx->ctx_config & CAM_IFE_CTX_CFG_FRAME_HEADER_TS) &&
		(prepare->num_out_map_entries) &&
		(!prepare_hw_data->frame_header_res_id)) {
		CAM_ERR(CAM_ISP, "Failed to configure frame header");
		goto end;
	}

	/*
	 * reg update will be done later for the initial configure.
	 * need to plus one to the op_code and only take the lower
	 * bits to get the type of operation since UMD definition
	 * of op_code has some difference from KMD.
	 */
	if (prepare_hw_data->packet_opcode_type == CAM_ISP_PACKET_INIT_DEV) {
		if ((!prepare->num_reg_dump_buf) || (prepare->num_reg_dump_buf >
			CAM_REG_DUMP_MAX_BUF_ENTRIES))
			goto end;

		if (!ctx->num_reg_dump_buf) {
			ctx->num_reg_dump_buf = prepare->num_reg_dump_buf;

			memcpy(ctx->reg_dump_buf_desc,
				prepare->reg_dump_buf_desc,
				sizeof(struct cam_cmd_buf_desc) *
				prepare->num_reg_dump_buf);
		} else {
			prepare_hw_data->num_reg_dump_buf =
				prepare->num_reg_dump_buf;
			memcpy(prepare_hw_data->reg_dump_buf_desc,
				prepare->reg_dump_buf_desc,
				sizeof(struct cam_cmd_buf_desc) *
				prepare_hw_data->num_reg_dump_buf);
		}

		goto end;
	} else {
		prepare_hw_data->num_reg_dump_buf = prepare->num_reg_dump_buf;
		if ((prepare_hw_data->num_reg_dump_buf) &&
			(prepare_hw_data->num_reg_dump_buf <
			CAM_REG_DUMP_MAX_BUF_ENTRIES)) {
			memcpy(prepare_hw_data->reg_dump_buf_desc,
				prepare->reg_dump_buf_desc,
				sizeof(struct cam_cmd_buf_desc) *
				prepare_hw_data->num_reg_dump_buf);
		}
	}

	/* add reg update commands */
	if (hw_mgr->csid_rup_en)
		rc = cam_ife_mgr_csid_add_reg_update(ctx,
			prepare, &kmd_buf);

	else
		rc = cam_ife_mgr_isp_add_reg_update(ctx,
			prepare, &kmd_buf);

	if (rc) {
		CAM_ERR(CAM_ISP, "Add RUP fail csid_rup_en %d",
			hw_mgr->csid_rup_en);
		goto end;
	}

	/* add go_cmd for offline context */
	if (prepare->num_out_map_entries &&
		prepare->num_in_map_entries &&
		ctx->flags.is_offline) {
		if (ctx->ctx_type != CAM_IFE_CTX_TYPE_SFE)
			rc = cam_isp_add_go_cmd(prepare, &ctx->res_list_ife_in_rd,
				ctx->base[i].idx, &kmd_buf);
		else
			rc = cam_isp_add_csid_offline_cmd(prepare,
				&ctx->res_list_ife_csid,
				ctx->base[i].idx, &kmd_buf);
		if (rc)
			CAM_ERR(CAM_ISP,
				"Add %s GO_CMD faled i: %d, idx: %d, rc: %d",
				(ctx->ctx_type == CAM_IFE_CTX_TYPE_SFE ?
				"CSID" : "IFE RD"),
				i, ctx->base[i].idx, rc);
	}

end:
	return rc;
}

static int cam_ife_mgr_resume_hw(struct cam_ife_hw_mgr_ctx *ctx)
{
	return cam_ife_mgr_bw_control(ctx, CAM_ISP_BW_CONTROL_INCLUDE);
}

static int cam_ife_mgr_sof_irq_debug(
	struct cam_ife_hw_mgr_ctx *ctx,
	uint32_t sof_irq_enable)
{
	int rc = 0;
	uint32_t i = 0;
	struct cam_isp_hw_mgr_res     *hw_mgr_res = NULL;
	struct cam_hw_intf            *hw_intf = NULL;
	struct cam_isp_resource_node  *rsrc_node = NULL;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf->hw_ops.process_cmd) {
				rc |= hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_IFE_CSID_SOF_IRQ_DEBUG,
					&sof_irq_enable,
					sizeof(sof_irq_enable));
			}
		}
	}

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			rsrc_node = hw_mgr_res->hw_res[i];
			if (rsrc_node->process_cmd && (rsrc_node->res_id ==
				CAM_ISP_HW_VFE_IN_CAMIF)) {
				rc |= hw_mgr_res->hw_res[i]->process_cmd(
					hw_mgr_res->hw_res[i],
					CAM_ISP_HW_CMD_SOF_IRQ_DEBUG,
					&sof_irq_enable,
					sizeof(sof_irq_enable));
			}
		}
	}

	return rc;
}

static void cam_ife_mgr_print_io_bufs(struct cam_ife_hw_mgr  *hw_mgr,
		uint32_t res_id, struct cam_packet *packet,
		bool    *ctx_found, struct cam_ife_hw_mgr_ctx *ctx)
{

	struct cam_buf_io_cfg  *io_cfg = NULL;
	int32_t      mmu_hdl, iommu_hdl, sec_mmu_hdl;
	dma_addr_t   iova_addr;
	size_t        src_buf_size;
	int  i, j, rc = 0;

	iommu_hdl = hw_mgr->mgr_common.img_iommu_hdl;
	sec_mmu_hdl = hw_mgr->mgr_common.img_iommu_hdl_secure;

	io_cfg = (struct cam_buf_io_cfg *)((uint32_t *)&packet->payload +
		packet->io_configs_offset / 4);

	for (i = 0; i < packet->num_io_configs; i++) {
		if (io_cfg[i].resource_type != res_id)
			continue;
		else
			break;
		}

		if (i == packet->num_io_configs) {
			*ctx_found = false;
			CAM_ERR(CAM_ISP,
				"getting io port for mid resource id failed ctx id:%d req id:%lld res id:0x%x",
				ctx->ctx_index, packet->header.request_id,
				res_id);
			return;
		}

		for (j = 0; j < CAM_PACKET_MAX_PLANES; j++) {
			if (!io_cfg[i].mem_handle[j])
				break;

			CAM_INFO(CAM_ISP, "port: 0x%x f: %u format: %d dir %d",
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
				CAM_ERR(CAM_ISP,
					"get src buf address fail mem_handle 0x%x",
					io_cfg[i].mem_handle[j]);
				continue;
			}

			CAM_INFO(CAM_ISP,
				"pln %d w %d h %d s %u size %zu addr 0x%llx end_addr 0x%llx offset %x memh %x",
				j, io_cfg[i].planes[j].width,
				io_cfg[i].planes[j].height,
				io_cfg[i].planes[j].plane_stride,
				src_buf_size, iova_addr,
				iova_addr + src_buf_size,
				io_cfg[i].offsets[j],
				io_cfg[i].mem_handle[j]);
		}
}

static void cam_ife_mgr_pf_dump(uint32_t res_id,
	struct cam_ife_hw_mgr_ctx *ctx)
{
	struct cam_isp_hw_mgr_res      *hw_mgr_res;
	struct cam_hw_intf             *hw_intf;
	struct cam_isp_hw_event_info    event_info;
	uint32_t                        res_id_out;
	int  i, rc = 0;

	/* dump the registers  */
	rc = cam_ife_mgr_handle_reg_dump(ctx, ctx->reg_dump_buf_desc,
		ctx->num_reg_dump_buf,
		CAM_ISP_PACKET_META_REG_DUMP_ON_ERROR, NULL, false);
	if (rc)
		CAM_ERR(CAM_ISP,
			"Reg dump on pf failed req id: %llu rc: %d",
			ctx->applied_req_id, rc);


	/* dump the acquire data */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_IFE_CSID_LOG_ACQUIRE_DATA,
					hw_mgr_res->hw_res[i],
					sizeof(void *));
				if (rc)
					CAM_ERR(CAM_ISP,
						"csid acquire data dump failed");
			} else
				CAM_ERR(CAM_ISP, "NULL hw_intf!");
		}
	}

	event_info.res_id = res_id;
	res_id_out = res_id & 0xFF;

	if (res_id_out >= max_ife_out_res) {
		CAM_ERR(CAM_ISP, "Invalid out resource id :%x",
			res_id);
		return;
	}

	hw_mgr_res = &ctx->res_list_ife_out[res_id_out];
	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!hw_mgr_res->hw_res[i])
			continue;
		hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.process_cmd) {
			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_ISP_HW_CMD_DUMP_BUS_INFO,
				(void *)&event_info,
				sizeof(struct cam_isp_hw_event_info));
		}
	}
}

static void cam_ife_mgr_pf_dump_mid_info(
	struct cam_ife_hw_mgr_ctx    *ctx,
	struct cam_hw_cmd_args       *hw_cmd_args,
	struct cam_isp_hw_intf_data  *hw_intf_data)
{
	struct cam_packet                  *packet;
	struct cam_isp_hw_get_cmd_update    cmd_update;
	struct cam_isp_hw_get_res_for_mid   get_res;
	int                                 rc = 0;

	packet  = hw_cmd_args->u.pf_args.pf_data.packet;
	get_res.mid = hw_cmd_args->u.pf_args.mid;
	cmd_update.cmd_type = CAM_ISP_HW_CMD_GET_RES_FOR_MID;
	cmd_update.data = (void *) &get_res;

	/* get resource id for given mid */
	rc = hw_intf_data->hw_intf->hw_ops.process_cmd(hw_intf_data->hw_intf->hw_priv,
		cmd_update.cmd_type, &cmd_update, sizeof(struct cam_isp_hw_get_cmd_update));

	if (rc) {
		CAM_ERR(CAM_ISP,
			"getting mid port resource id failed ctx id:%d req id:%lld",
			ctx->ctx_index, packet->header.request_id);
		return;
	}

	*(hw_cmd_args->u.pf_args.resource_type) = get_res.out_res_id;
	ctx->flags.pf_mid_found = true;
	ctx->pf_info.mid = get_res.mid;
	ctx->pf_info.out_port_id = get_res.out_res_id;
	CAM_ERR(CAM_ISP,
		"Page fault on resource id:(0x%x) ctx id:%d req id:%lld",
		get_res.out_res_id, ctx->ctx_index, packet->header.request_id);
}

static void cam_ife_mgr_dump_pf_data(
	struct cam_ife_hw_mgr  *hw_mgr,
	struct cam_hw_cmd_args *hw_cmd_args)
{
	struct cam_ife_hw_mgr_ctx          *ctx;
	struct cam_packet                  *packet;
	struct cam_isp_hw_intf_data        *hw_intf_data;
	uint32_t                           *resource_type;
	bool                               *ctx_found;
	int                                 i, j;

	ctx = (struct cam_ife_hw_mgr_ctx *)hw_cmd_args->ctxt_to_hw_map;

	packet  = hw_cmd_args->u.pf_args.pf_data.packet;
	ctx_found = hw_cmd_args->u.pf_args.ctx_found;
	resource_type = hw_cmd_args->u.pf_args.resource_type;

	if ((*ctx_found) && (*resource_type))
		goto outportlog;

	if (ctx->flags.pf_mid_found)
		goto outportlog;

	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].hw_type == CAM_ISP_HW_TYPE_VFE)
			hw_intf_data = g_ife_hw_mgr.ife_devices[ctx->base[i].idx];
		else if (ctx->base[i].hw_type == CAM_ISP_HW_TYPE_SFE)
			hw_intf_data = g_ife_hw_mgr.sfe_devices[ctx->base[i].idx];
		else
			continue;

		/*
		 * Few old targets do not have support for PID, for such targets,
		 * we need to print mid for all the bases, SFE enabled targets
		 * are expected to have PID support.
		 */
		if (!g_ife_hw_mgr.hw_pid_support) {
			if (ctx->base[i].split_id == CAM_ISP_HW_SPLIT_LEFT)
				cam_ife_mgr_pf_dump_mid_info(ctx, hw_cmd_args, hw_intf_data);
			continue;
		}

		for (j = 0; j < hw_intf_data->num_hw_pid; j++) {
			if (hw_intf_data->hw_pid[j] == hw_cmd_args->u.pf_args.pid) {
				*ctx_found = true;
				CAM_ERR(CAM_ISP, "PF found for %s%d pid: %u",
					ctx->base[i].hw_type == CAM_ISP_HW_TYPE_VFE ? "VFE" : "SFE",
					ctx->base[i].idx, hw_cmd_args->u.pf_args.pid);
				cam_ife_mgr_pf_dump_mid_info(ctx, hw_cmd_args, hw_intf_data);
				break;
			}
		}

		if (*ctx_found)
			break;
	}

	if (g_ife_hw_mgr.hw_pid_support && (i == ctx->num_base || !*ctx_found))
		CAM_INFO(CAM_ISP,
			"This context does not cause pf:pid:%d ctx_id:%d",
			hw_cmd_args->u.pf_args.pid, ctx->ctx_index);
	cam_ife_mgr_pf_dump(ctx->pf_info.out_port_id, ctx);

outportlog:
	cam_ife_mgr_print_io_bufs(hw_mgr, *resource_type, packet,
		ctx_found, ctx);
}

int cam_isp_config_csid_rup_aup(
	struct cam_ife_hw_mgr_ctx *ctx)
{
	int rc = 0, i, j, hw_idx;
	struct cam_isp_hw_mgr_res       *hw_mgr_res;
	struct list_head                *res_list;
	struct cam_isp_resource_node    *res;
	struct cam_isp_csid_reg_update_args
		rup_args[CAM_IFE_CSID_HW_NUM_MAX] = {0};

	res_list = &ctx->res_list_ife_csid;
	for (j = 0; j < ctx->num_base; j++) {
		if (ctx->base[j].hw_type != CAM_ISP_HW_TYPE_CSID)
			continue;

		list_for_each_entry(hw_mgr_res, res_list, list) {
			if (hw_mgr_res->res_type == CAM_ISP_RESOURCE_UNINT)
				continue;

			for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
				if (!hw_mgr_res->hw_res[i])
					continue;

				res = hw_mgr_res->hw_res[i];
				if (res->hw_intf->hw_idx != ctx->base[j].idx)
					continue;

				hw_idx = res->hw_intf->hw_idx;
				rup_args[hw_idx].res[rup_args[hw_idx].num_res] = res;
				rup_args[hw_idx].num_res++;

				CAM_DBG(CAM_ISP,
					"Reg update for res %d hw_id %d cdm_idx %d",
					res->res_id, res->hw_intf->hw_idx, ctx->base[j].idx);
			}
		}
	}

	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (!rup_args[i].num_res)
			continue;

		rup_args[i].cmd.cmd_buf_addr = NULL;
		rup_args[i].cmd.size = 0;
		rup_args[i].reg_write = true;
		rup_args[i].last_applied_mup = ctx->current_mup;
		res = rup_args[i].res[0];

		rc = res->hw_intf->hw_ops.process_cmd(
			res->hw_intf->hw_priv,
			CAM_ISP_HW_CMD_GET_REG_UPDATE, &rup_args[i],
			sizeof(struct cam_isp_csid_reg_update_args));
		if (rc)
			return rc;

		CAM_DBG(CAM_ISP,
			"Reg update for CSID: %u mup: %u",
			res->hw_intf->hw_idx, ctx->current_mup);
	}

	return rc;
}

/*
 * Scratch buffer is for sHDR/FS usescases involing SFE RDI0-2
 * There is no possibility of dual in this case, hence
 * using the scratch buffer provided during INIT corresponding
 * to each left RDIs
 */
static int cam_ife_mgr_prog_default_settings(
	bool need_rup_aup, struct cam_ife_hw_mgr_ctx *ctx)
{
	int i, j, res_id, rc = 0;
	struct cam_isp_hw_mgr_res       *hw_mgr_res;
	struct cam_sfe_scratch_buf_info *buf_info;
	struct list_head                *res_list_in_rd = NULL;
	struct cam_isp_hw_mgr_res       *res_list_sfe_out = NULL;

	res_list_in_rd = &ctx->res_list_ife_in_rd;
	res_list_sfe_out = ctx->res_list_sfe_out;

	for (i = 0; i < CAM_SFE_FE_RDI_NUM_MAX; i++) {
		hw_mgr_res = &res_list_sfe_out[i];
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			/* j = 1 is not valid for this use-case */
			if (!hw_mgr_res->hw_res[j])
				continue;

			res_id = hw_mgr_res->hw_res[j]->res_id;

			if (cam_isp_sfe_validate_for_scratch_buf_config(
				(res_id - CAM_ISP_SFE_OUT_RES_RDI_0), ctx, true))
				continue;

			buf_info = &ctx->sfe_info.scratch_config->buf_info[
				res_id - CAM_ISP_SFE_OUT_RES_RDI_0];

			/* Check if scratch available for this resource */
			if (!buf_info->config_done) {
				CAM_ERR(CAM_ISP,
					"No scratch buffer config found for res: %u on ctx: %u",
					res_id, ctx->ctx_index);
				return -EFAULT;
			}

			CAM_DBG(CAM_ISP,
				"RDI%d res_id 0x%x idx %u io_addr %pK",
				i,
				hw_mgr_res->hw_res[j]->res_id,
				(res_id - CAM_ISP_SFE_OUT_RES_RDI_0),
				buf_info->io_addr);

			rc = cam_isp_sfe_send_scratch_buf_upd(0x0,
				CAM_ISP_HW_CMD_BUF_UPDATE,
				hw_mgr_res->hw_res[j], buf_info,
				NULL, NULL);
			if (rc)
				return rc;
		}
	}

	list_for_each_entry(hw_mgr_res, res_list_in_rd, list) {
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!hw_mgr_res->hw_res[j])
				continue;

			res_id = hw_mgr_res->hw_res[j]->res_id;

			if (cam_isp_sfe_validate_for_scratch_buf_config(
				(res_id - CAM_ISP_HW_SFE_IN_RD0), ctx, true))
				continue;

			buf_info = &ctx->sfe_info.scratch_config->buf_info
				[res_id - CAM_ISP_HW_SFE_IN_RD0];
			CAM_DBG(CAM_ISP,
				"RD res_id 0x%x idx %u io_addr %pK",
				hw_mgr_res->hw_res[j]->res_id,
				(res_id - CAM_ISP_HW_SFE_IN_RD0),
				buf_info->io_addr);
			rc = cam_isp_sfe_send_scratch_buf_upd(0x0,
				CAM_ISP_HW_CMD_BUF_UPDATE_RM,
				hw_mgr_res->hw_res[j], buf_info,
				NULL, NULL);
			if (rc)
				return rc;
		}
	}

	/* Program rup & aup only at run time */
	if (need_rup_aup) {
		rc = cam_isp_config_csid_rup_aup(ctx);
		if (rc)
			CAM_ERR(CAM_ISP,
				"RUP/AUP update failed for scratch buffers");
	}

	return rc;
}

static int cam_ife_mgr_cmd(void *hw_mgr_priv, void *cmd_args)
{
	int rc = 0;
	struct cam_hw_cmd_args *hw_cmd_args = cmd_args;
	struct cam_ife_hw_mgr  *hw_mgr = hw_mgr_priv;
	struct cam_ife_hw_mgr_ctx *ctx = (struct cam_ife_hw_mgr_ctx *)
		hw_cmd_args->ctxt_to_hw_map;
	struct cam_isp_hw_cmd_args *isp_hw_cmd_args = NULL;
	struct cam_packet          *packet;
	unsigned long rem_jiffies = 0;

	if (!hw_mgr_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	if (!ctx || !ctx->flags.ctx_in_use) {
		CAM_ERR(CAM_ISP, "Fatal: Invalid context is used");
		return -EPERM;
	}

	switch (hw_cmd_args->cmd_type) {
	case CAM_HW_MGR_CMD_INTERNAL:
		if (!hw_cmd_args->u.internal_args) {
			CAM_ERR(CAM_ISP, "Invalid cmd arguments");
			return -EINVAL;
		}

		isp_hw_cmd_args = (struct cam_isp_hw_cmd_args *)
			hw_cmd_args->u.internal_args;

		switch (isp_hw_cmd_args->cmd_type) {
		case CAM_ISP_HW_MGR_CMD_PAUSE_HW:
			cam_ife_mgr_pause_hw(ctx);
			break;
		case CAM_ISP_HW_MGR_CMD_RESUME_HW:
			cam_ife_mgr_resume_hw(ctx);
			break;
		case CAM_ISP_HW_MGR_CMD_SOF_DEBUG:
			cam_ife_mgr_sof_irq_debug(ctx,
				isp_hw_cmd_args->u.sof_irq_enable);
			break;
		case CAM_ISP_HW_MGR_CMD_CTX_TYPE:
			if (ctx->flags.is_fe_enabled && ctx->flags.is_offline)
				isp_hw_cmd_args->u.ctx_type =
					CAM_ISP_CTX_OFFLINE;
			else if (ctx->flags.is_fe_enabled && !ctx->flags.is_offline &&
				ctx->ctx_type != CAM_IFE_CTX_TYPE_SFE)
				isp_hw_cmd_args->u.ctx_type = CAM_ISP_CTX_FS2;
			else if (ctx->flags.is_rdi_only_context || ctx->flags.is_lite_context)
				isp_hw_cmd_args->u.ctx_type = CAM_ISP_CTX_RDI;
			else
				isp_hw_cmd_args->u.ctx_type = CAM_ISP_CTX_PIX;
			break;
		case CAM_ISP_HW_MGR_GET_PACKET_OPCODE:
			packet = (struct cam_packet *)
				isp_hw_cmd_args->cmd_data;
			if (((packet->header.op_code + 1) & 0xF) ==
				CAM_ISP_PACKET_INIT_DEV)
				isp_hw_cmd_args->u.packet_op_code =
				CAM_ISP_PACKET_INIT_DEV;
			else
				isp_hw_cmd_args->u.packet_op_code =
				CAM_ISP_PACKET_UPDATE_DEV;
			break;
		case CAM_ISP_HW_MGR_GET_LAST_CDM_DONE:
			isp_hw_cmd_args->u.last_cdm_done =
				ctx->last_cdm_done_req;
			break;
		case CAM_ISP_HW_MGR_CMD_PROG_DEFAULT_CFG:
			rc = cam_ife_mgr_prog_default_settings(true, ctx);
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid HW mgr command:0x%x",
				hw_cmd_args->cmd_type);
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_HW_MGR_CMD_DUMP_PF_INFO:
		cam_ife_mgr_dump_pf_data(hw_mgr, hw_cmd_args);

		break;
	case CAM_HW_MGR_CMD_REG_DUMP_ON_FLUSH:
		if (ctx->flags.dump_on_flush)
			return 0;

		ctx->flags.dump_on_flush = true;
		rem_jiffies = cam_common_wait_for_completion_timeout(
			&ctx->config_done_complete, msecs_to_jiffies(30));
		if (rem_jiffies == 0)
			CAM_ERR(CAM_ISP,
				"config done completion timeout, Reg dump will be unreliable rc=%d ctx_index %d",
				rc, ctx->ctx_index);

		rc = cam_ife_mgr_handle_reg_dump(ctx, ctx->reg_dump_buf_desc,
			ctx->num_reg_dump_buf,
			CAM_ISP_PACKET_META_REG_DUMP_ON_FLUSH, NULL, false);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Reg dump on flush failed req id: %llu rc: %d",
				ctx->applied_req_id, rc);
			return rc;
		}
		break;
	case CAM_HW_MGR_CMD_REG_DUMP_ON_ERROR:
		if (ctx->flags.dump_on_error)
			return 0;

		ctx->flags.dump_on_error = true;
		rc = cam_ife_mgr_handle_reg_dump(ctx, ctx->reg_dump_buf_desc,
			ctx->num_reg_dump_buf,
			CAM_ISP_PACKET_META_REG_DUMP_ON_ERROR, NULL, false);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Reg dump on error failed req id: %llu rc: %d",
				ctx->applied_req_id, rc);
			return rc;
		}
		break;
	case CAM_HW_MGR_CMD_DUMP_ACQ_INFO:
		cam_ife_hw_mgr_dump_acq_data(ctx);
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid cmd");
	}

	return rc;
}

static int cam_ife_mgr_user_dump_hw(
		struct cam_ife_hw_mgr_ctx *ife_ctx,
		struct cam_hw_dump_args *dump_args)
{
	int rc = 0;
	struct cam_hw_soc_dump_args soc_dump_args;

	if (!ife_ctx || !dump_args) {
		CAM_ERR(CAM_ISP, "Invalid parameters %pK %pK",
			ife_ctx, dump_args);
		rc = -EINVAL;
		goto end;
	}
	soc_dump_args.buf_handle = dump_args->buf_handle;
	soc_dump_args.request_id = dump_args->request_id;
	soc_dump_args.offset = dump_args->offset;

	rc = cam_ife_mgr_handle_reg_dump(ife_ctx,
		ife_ctx->reg_dump_buf_desc,
		ife_ctx->num_reg_dump_buf,
		CAM_ISP_PACKET_META_REG_DUMP_ON_ERROR,
		&soc_dump_args,
		true);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Dump failed req: %lld handle %u offset %u",
			dump_args->request_id,
			dump_args->buf_handle,
			dump_args->offset);
		goto end;
	}
	dump_args->offset = soc_dump_args.offset;
end:
	return rc;
}

static int cam_ife_mgr_dump(void *hw_mgr_priv, void *args)
{
	struct cam_isp_hw_dump_args isp_hw_dump_args;
	struct cam_hw_dump_args *dump_args = (struct cam_hw_dump_args *)args;
	struct cam_isp_hw_mgr_res            *hw_mgr_res;
	struct cam_hw_intf                   *hw_intf;
	struct cam_ife_hw_mgr_ctx *ife_ctx = (struct cam_ife_hw_mgr_ctx *)
						dump_args->ctxt_to_hw_map;
	int i;
	int rc = 0;

	/* for some targets, information about the IFE registers to be dumped
	 * is already submitted with the hw manager. In this case, we
	 * can dump just the related registers and skip going to core files.
	 */
	if (ife_ctx->num_reg_dump_buf) {
		cam_ife_mgr_user_dump_hw(ife_ctx, dump_args);
		goto end;
	}

	rc  = cam_mem_get_cpu_buf(dump_args->buf_handle,
		&isp_hw_dump_args.cpu_addr,
		&isp_hw_dump_args.buf_len);
	if (rc) {
		CAM_ERR(CAM_ISP, "Invalid handle %u rc %d",
			dump_args->buf_handle, rc);
		return rc;
	}

	isp_hw_dump_args.offset = dump_args->offset;
	isp_hw_dump_args.req_id = dump_args->request_id;

	list_for_each_entry(hw_mgr_res, &ife_ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			switch (hw_mgr_res->hw_res[i]->res_id) {
			case CAM_IFE_PIX_PATH_RES_RDI_0:
			case CAM_IFE_PIX_PATH_RES_RDI_1:
			case CAM_IFE_PIX_PATH_RES_RDI_2:
			case CAM_IFE_PIX_PATH_RES_RDI_3:
				if (ife_ctx->flags.is_rdi_only_context &&
					hw_intf->hw_ops.process_cmd) {
					rc = hw_intf->hw_ops.process_cmd(
						hw_intf->hw_priv,
						CAM_ISP_HW_CMD_DUMP_HW,
						&isp_hw_dump_args,
						sizeof(struct
						    cam_isp_hw_dump_args));
				}
				break;
			case CAM_IFE_PIX_PATH_RES_IPP:
				if (hw_intf->hw_ops.process_cmd) {
					rc = hw_intf->hw_ops.process_cmd(
						hw_intf->hw_priv,
						CAM_ISP_HW_CMD_DUMP_HW,
						&isp_hw_dump_args,
						sizeof(struct
						    cam_isp_hw_dump_args));
				}
				break;
			default:
				CAM_DBG(CAM_ISP, "not a valid res %d",
				hw_mgr_res->res_id);
				break;
			}
		}
	}

	list_for_each_entry(hw_mgr_res, &ife_ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			switch (hw_mgr_res->res_id) {
			case CAM_ISP_HW_VFE_IN_RDI0:
			case CAM_ISP_HW_VFE_IN_RDI1:
			case CAM_ISP_HW_VFE_IN_RDI2:
			case CAM_ISP_HW_VFE_IN_RDI3:
				if (ife_ctx->flags.is_rdi_only_context &&
					hw_intf->hw_ops.process_cmd) {
					rc = hw_intf->hw_ops.process_cmd(
						hw_intf->hw_priv,
						CAM_ISP_HW_CMD_DUMP_HW,
						&isp_hw_dump_args,
						sizeof(struct
						    cam_isp_hw_dump_args));
				}
				break;
			case CAM_ISP_HW_VFE_IN_CAMIF:
				if (hw_intf->hw_ops.process_cmd) {
					rc = hw_intf->hw_ops.process_cmd(
						hw_intf->hw_priv,
						CAM_ISP_HW_CMD_DUMP_HW,
						&isp_hw_dump_args,
						sizeof(struct
						    cam_isp_hw_dump_args));
				}
				break;
			default:
				CAM_DBG(CAM_ISP, "not a valid res %d",
					hw_mgr_res->res_id);
				break;
			}
		}
	}
	dump_args->offset = isp_hw_dump_args.offset;
end:
	CAM_DBG(CAM_ISP, "offset %u", dump_args->offset);
	return rc;
}

static inline void cam_ife_hw_mgr_get_offline_sof_timestamp(
	uint64_t                             *timestamp,
	uint64_t                             *boot_time)
{
	struct timespec64                     ts;

	ktime_get_boottime_ts64(&ts);
	*timestamp = (uint64_t)((ts.tv_sec * 1000000000) + ts.tv_nsec);
	*boot_time = *timestamp;
}

static int cam_ife_mgr_cmd_get_sof_timestamp(
	struct cam_ife_hw_mgr_ctx            *ife_ctx,
	uint64_t                             *time_stamp,
	uint64_t                             *boot_time_stamp)
{
	int                                   rc = -EINVAL;
	uint32_t                              i;
	struct cam_isp_hw_mgr_res            *hw_mgr_res;
	struct cam_hw_intf                   *hw_intf;
	struct cam_csid_get_time_stamp_args   csid_get_time;

	hw_mgr_res = list_first_entry(&ife_ctx->res_list_ife_csid,
		struct cam_isp_hw_mgr_res, list);

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!hw_mgr_res->hw_res[i])
			continue;

		/*
		 * Get the SOF time stamp from left resource only.
		 * Left resource is master for dual vfe case and
		 * Rdi only context case left resource only hold
		 * the RDI resource
		 */

		hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.process_cmd) {
			/*
			 * Single VFE case, Get the time stamp from
			 * available one csid hw in the context
			 * Dual VFE case, get the time stamp from
			 * master(left) would be sufficient
			 */

			csid_get_time.node_res =
				hw_mgr_res->hw_res[i];
			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_IFE_CSID_CMD_GET_TIME_STAMP,
				&csid_get_time,
				sizeof(
				struct cam_csid_get_time_stamp_args));
			if (!rc && (i == CAM_ISP_HW_SPLIT_LEFT)) {
				*time_stamp =
					csid_get_time.time_stamp_val;
				*boot_time_stamp =
					csid_get_time.boot_timestamp;
			}
		}
	}

	if (rc)
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Getting sof time stamp failed");

	return rc;
}

static int cam_ife_mgr_recover_hw(void *priv, void *data)
{
	int32_t rc = 0;
	struct cam_ife_hw_event_recovery_data   *recovery_data = data;
	struct cam_hw_start_args                 start_args;
	struct cam_hw_stop_args                  stop_args;
	struct cam_ife_hw_mgr                   *ife_hw_mgr = priv;
	uint32_t                                 i = 0;
	bool cancel = false;
	uint32_t error_type = recovery_data->error_type;
	struct cam_ife_hw_mgr_ctx        *ctx = NULL;

	for (i = 0; i < recovery_data->no_of_context; i++) {
		ctx = recovery_data->affected_ctx[i];
		if (recovery_data->id[i] != atomic_read(&ctx->recovery_id)) {
			CAM_INFO(CAM_ISP, "recovery for ctx:%d error-type:%d cancelled",
				ctx->ctx_index, error_type);
			cancel = true;
		}
	}
	if (cancel)
		goto end;

	/* Here recovery is performed */
	CAM_DBG(CAM_ISP, "ErrorType = %d", error_type);

	switch (error_type) {
	case CAM_ISP_HW_ERROR_OVERFLOW:
	case CAM_ISP_HW_ERROR_BUSIF_OVERFLOW:
	case CAM_ISP_HW_ERROR_VIOLATION:
		if (!recovery_data->affected_ctx[0]) {
			CAM_ERR(CAM_ISP,
				"No context is affected but recovery called");
			kfree(recovery_data);
			return 0;
		}
		/* stop resources here */
		CAM_DBG(CAM_ISP, "STOP: Number of affected context: %d",
			recovery_data->no_of_context);
		for (i = 0; i < recovery_data->no_of_context; i++) {
			stop_args.ctxt_to_hw_map =
				recovery_data->affected_ctx[i];
			rc = cam_ife_mgr_stop_hw_in_overflow(&stop_args);
			if (rc) {
				CAM_ERR(CAM_ISP, "CTX stop failed(%d)", rc);
				return rc;
			}
		}

		if (!g_ife_hw_mgr.debug_cfg.enable_recovery)
			break;

		CAM_DBG(CAM_ISP, "RESET: CSID PATH");
		for (i = 0; i < recovery_data->no_of_context; i++) {
			ctx = recovery_data->affected_ctx[i];
			rc = cam_ife_hw_mgr_reset_csid(ctx,
				CAM_IFE_CSID_RESET_PATH);

			if (rc) {
				CAM_ERR(CAM_ISP, "Failed RESET");
				return rc;
			}
		}

		CAM_DBG(CAM_ISP, "RESET: Calling VFE reset");

		for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
			if (recovery_data->affected_core[i])
				cam_ife_mgr_reset_vfe_hw(ife_hw_mgr, i);
		}

		CAM_DBG(CAM_ISP, "START: Number of affected context: %d",
			recovery_data->no_of_context);

		for (i = 0; i < recovery_data->no_of_context; i++) {
			ctx =  recovery_data->affected_ctx[i];
			start_args.ctxt_to_hw_map = ctx;

			atomic_set(&ctx->overflow_pending, 0);

			rc = cam_ife_mgr_restart_hw(&start_args);
			if (rc) {
				CAM_ERR(CAM_ISP, "CTX start failed(%d)", rc);
				return rc;
			}
			CAM_DBG(CAM_ISP, "Started resources rc (%d)", rc);
		}
		CAM_DBG(CAM_ISP, "Recovery Done rc (%d)", rc);

		break;

	case CAM_ISP_HW_ERROR_P2I_ERROR:
		break;

	default:
		CAM_ERR(CAM_ISP, "Invalid Error");
	}
	CAM_DBG(CAM_ISP, "Exit: ErrorType = %d", error_type);

end:
	kfree(recovery_data);
	return rc;
}

static int cam_ife_hw_mgr_do_error_recovery(
	struct cam_ife_hw_event_recovery_data  *ife_mgr_recovery_data)
{
	int32_t                                 rc, i;
	struct crm_workq_task                  *task = NULL;
	struct cam_ife_hw_event_recovery_data  *recovery_data = NULL;
	struct cam_ife_hw_mgr_ctx *ctx;

	recovery_data = kmemdup(ife_mgr_recovery_data,
		sizeof(struct cam_ife_hw_event_recovery_data), GFP_ATOMIC);
	if (!recovery_data)
		return -ENOMEM;

	CAM_DBG(CAM_ISP, "Enter: error_type (%d)", recovery_data->error_type);

	task = cam_req_mgr_workq_get_task(g_ife_hw_mgr.workq);
	if (!task) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No empty task frame");
		kfree(recovery_data);
		return -ENOMEM;
	}

	task->process_cb = &cam_context_handle_hw_recovery;
	task->payload = recovery_data;
	for (i = 0; i < recovery_data->no_of_context; i++) {
		ctx = recovery_data->affected_ctx[i];
		recovery_data->id[i] = atomic_inc_return(&ctx->recovery_id);
	}

	rc = cam_req_mgr_workq_enqueue_task(task,
		recovery_data->affected_ctx[0]->common.cb_priv,
		CRM_TASK_PRIORITY_0);
	return rc;
}

/*
 * This function checks if any of the valid entry in affected_core[]
 * is associated with this context. if YES
 *  a. It fills the other cores associated with this context.in
 *      affected_core[]
 *  b. Return true
 */
static bool cam_ife_hw_mgr_is_ctx_affected(
	struct cam_ife_hw_mgr_ctx   *ife_hwr_mgr_ctx,
	uint32_t                    *affected_core,
	uint32_t                     size)
{

	bool                  rc = false;
	uint32_t              i = 0, j = 0;
	uint32_t              max_idx =  ife_hwr_mgr_ctx->num_base;
	uint32_t              ctx_affected_core_idx[CAM_IFE_HW_NUM_MAX] = {0};

	CAM_DBG(CAM_ISP, "Enter:max_idx = %d", max_idx);

	if ((max_idx >= CAM_IFE_HW_NUM_MAX) || (size > CAM_IFE_HW_NUM_MAX)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "invalid parameter = %d", max_idx);
		return rc;
	}

	for (i = 0; i < max_idx; i++) {
		if (affected_core[ife_hwr_mgr_ctx->base[i].idx])
			rc = true;
		else {
			ctx_affected_core_idx[j] = ife_hwr_mgr_ctx->base[i].idx;
			j = j + 1;
		}
	}

	if (rc) {
		while (j) {
			if (affected_core[ctx_affected_core_idx[j-1]] != 1)
				affected_core[ctx_affected_core_idx[j-1]] = 1;
			j = j - 1;
		}
	}
	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

/*
 * For any dual VFE context, if non-affected VFE is also serving
 * another context, then that context should also be notified with fatal error
 * So Loop through each context and -
 *   a. match core_idx
 *   b. Notify CTX with fatal error
 */
static int  cam_ife_hw_mgr_find_affected_ctx(
	struct cam_isp_hw_error_event_data        *error_event_data,
	uint32_t                                   curr_core_idx,
	struct cam_ife_hw_event_recovery_data     *recovery_data)
{
	uint32_t affected_core[CAM_IFE_HW_NUM_MAX] = {0};
	struct cam_ife_hw_mgr_ctx   *ife_hwr_mgr_ctx = NULL;
	cam_hw_event_cb_func         notify_err_cb;
	struct cam_ife_hw_mgr       *ife_hwr_mgr = NULL;
	uint32_t i = 0;

	if (!recovery_data) {
		CAM_ERR(CAM_ISP, "recovery_data parameter is NULL");
		return -EINVAL;
	}

	recovery_data->no_of_context = 0;
	affected_core[curr_core_idx] = 1;
	ife_hwr_mgr = &g_ife_hw_mgr;

	list_for_each_entry(ife_hwr_mgr_ctx,
		&ife_hwr_mgr->used_ctx_list, list) {
		/*
		 * Check if current core_idx matches the HW associated
		 * with this context
		 */
		if (!cam_ife_hw_mgr_is_ctx_affected(ife_hwr_mgr_ctx,
			affected_core, CAM_IFE_HW_NUM_MAX))
			continue;

		if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending)) {
			CAM_INFO(CAM_ISP, "CTX:%d already error reported",
				ife_hwr_mgr_ctx->ctx_index);
			continue;
		}

		atomic_set(&ife_hwr_mgr_ctx->overflow_pending, 1);
		notify_err_cb = ife_hwr_mgr_ctx->common.event_cb;

		/* Add affected_context in list of recovery data */
		CAM_DBG(CAM_ISP, "Add affected ctx %d to list",
			ife_hwr_mgr_ctx->ctx_index);
		if (recovery_data->no_of_context < CAM_IFE_CTX_MAX)
			recovery_data->affected_ctx[
				recovery_data->no_of_context++] =
				ife_hwr_mgr_ctx;

		/*
		 * In the call back function corresponding ISP context
		 * will update CRM about fatal Error
		 */
		if (notify_err_cb)
			notify_err_cb(ife_hwr_mgr_ctx->common.cb_priv,
				CAM_ISP_HW_EVENT_ERROR,
			        (void *)error_event_data);
		else {
			CAM_WARN(CAM_ISP, "Error call back is not set");
			goto end;
		}
	}

	/* fill the affected_core in recovery data */
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		recovery_data->affected_core[i] = affected_core[i];
		CAM_DBG(CAM_ISP, "Vfe core %d is affected (%d)",
			 i, recovery_data->affected_core[i]);
	}
end:
	return 0;
}

static int cam_ife_hw_mgr_handle_csid_frame_drop(
	struct cam_isp_hw_event_info         *event_info,
	struct cam_ife_hw_mgr_ctx            *ctx)
{
	int rc = 0;
	cam_hw_event_cb_func ife_hw_irq_cb = ctx->common.event_cb;

	/*
	 * Support frame drop as secondary event
	 */
	if (event_info->is_secondary_evt) {
		struct cam_isp_hw_secondary_event_data sec_evt_data;

		CAM_DBG(CAM_ISP,
			"Received CSID[%u] sensor sync frame drop res: %d as secondary evt",
			event_info->hw_idx, event_info->res_id);

		sec_evt_data.evt_type = CAM_ISP_HW_SEC_EVENT_OUT_OF_SYNC_FRAME_DROP;
		rc = ife_hw_irq_cb(ctx->common.cb_priv,
			CAM_ISP_HW_SECONDARY_EVENT, (void *)&sec_evt_data);
	}

	return rc;
}

static int cam_ife_hw_mgr_handle_csid_error(
	struct cam_ife_hw_mgr_ctx      *ctx,
	struct cam_isp_hw_event_info   *event_info)
{
	int                                      rc = -EINVAL;
	uint32_t                                 err_type;
	struct cam_isp_hw_error_event_info      *err_evt_info;
	struct cam_isp_hw_error_event_data       error_event_data = {0};
	struct cam_ife_hw_event_recovery_data    recovery_data = {0};

	if (!event_info->event_data) {
		CAM_ERR(CAM_ISP,
			"No additional error event data failed to process for CSID[%u] ctx: %u",
			event_info->hw_idx, ctx->ctx_index);
		return -EINVAL;
	}

	err_evt_info = (struct cam_isp_hw_error_event_info *)event_info->event_data;
	err_type = err_evt_info->err_type;
	CAM_DBG(CAM_ISP, "Entry CSID[%u] error %d", event_info->hw_idx, err_type);

	spin_lock(&g_ife_hw_mgr.ctx_lock);
	if (err_type & CAM_ISP_HW_ERROR_CSID_SENSOR_FRAME_DROP)
		cam_ife_hw_mgr_handle_csid_frame_drop(event_info, ctx);

	if ((err_type & CAM_ISP_HW_ERROR_CSID_FATAL) &&
		g_ife_hw_mgr.debug_cfg.enable_csid_recovery) {

		error_event_data.error_type = CAM_ISP_HW_ERROR_CSID_FATAL;
		error_event_data.error_code = CAM_REQ_MGR_CSID_FATAL_ERROR;
		rc = cam_ife_hw_mgr_find_affected_ctx(&error_event_data,
			event_info->hw_idx, &recovery_data);
		goto end;
	}

	if (err_type & (CAM_ISP_HW_ERROR_CSID_FIFO_OVERFLOW |
		CAM_ISP_HW_ERROR_RECOVERY_OVERFLOW |
		CAM_ISP_HW_ERROR_CSID_FRAME_SIZE)) {

		cam_ife_hw_mgr_notify_overflow(event_info, ctx);
		error_event_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;
		if (err_type & CAM_ISP_HW_ERROR_CSID_FIFO_OVERFLOW)
			error_event_data.error_code |=
				CAM_REQ_MGR_CSID_FIFO_OVERFLOW_ERROR;
		if (err_type & CAM_ISP_HW_ERROR_RECOVERY_OVERFLOW)
			error_event_data.error_code |=
				CAM_REQ_MGR_CSID_RECOVERY_OVERFLOW_ERROR;
		if (err_type & CAM_ISP_HW_ERROR_CSID_FRAME_SIZE)
			error_event_data.error_code |=
				CAM_REQ_MGR_CSID_PIXEL_COUNT_MISMATCH;
		rc = cam_ife_hw_mgr_find_affected_ctx(&error_event_data,
			event_info->hw_idx, &recovery_data);
	}

end:

	if (rc || !recovery_data.no_of_context)
		goto skip_recovery;

	recovery_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;
	cam_ife_hw_mgr_do_error_recovery(&recovery_data);
	CAM_DBG(CAM_ISP, "Exit CSID[%u] error %d", event_info->hw_idx,
		err_type);

skip_recovery:
	spin_unlock(&g_ife_hw_mgr.ctx_lock);
	return 0;
}

static int cam_ife_hw_mgr_handle_csid_rup(
	struct cam_ife_hw_mgr_ctx        *ife_hw_mgr_ctx,
	struct cam_isp_hw_event_info     *event_info)
{
	cam_hw_event_cb_func                     ife_hwr_irq_rup_cb;
	struct cam_isp_hw_reg_update_event_data  rup_event_data;

	ife_hwr_irq_rup_cb = ife_hw_mgr_ctx->common.event_cb;

	switch (event_info->res_id) {
	case CAM_IFE_PIX_PATH_RES_IPP:
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
	case CAM_IFE_PIX_PATH_RES_PPP:
		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;
		ife_hwr_irq_rup_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_REG_UPDATE, &rup_event_data);
		break;
	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "RUP done for CSID:%d source %d", event_info->hw_idx,
		event_info->res_id);

	return 0;
}

static int cam_ife_hw_mgr_handle_csid_camif_sof(
	struct cam_ife_hw_mgr_ctx            *ctx,
	struct cam_isp_hw_event_info         *event_info)
{
	int rc = 0;
	cam_hw_event_cb_func ife_hw_irq_sof_cb = ctx->common.event_cb;

	if (event_info->is_secondary_evt) {
		struct cam_isp_hw_secondary_event_data sec_evt_data;

		CAM_DBG(CAM_ISP,
			"Received CSID[%u] CAMIF SOF res: %d as secondary evt",
			event_info->hw_idx, event_info->res_id);

		sec_evt_data.evt_type = CAM_ISP_HW_SEC_EVENT_SOF;
		rc = ife_hw_irq_sof_cb(ctx->common.cb_priv,
			CAM_ISP_HW_SECONDARY_EVENT, (void *)&sec_evt_data);
	}

	return rc;
}

static int cam_ife_hw_mgr_handle_csid_camif_epoch(
	struct cam_ife_hw_mgr_ctx            *ctx,
	struct cam_isp_hw_event_info         *event_info)
{
	int rc = 0;
	cam_hw_event_cb_func ife_hw_irq_epoch_cb = ctx->common.event_cb;

	if (event_info->is_secondary_evt) {
		struct cam_isp_hw_secondary_event_data sec_evt_data;

		CAM_DBG(CAM_ISP,
			"Received CSID[%u] CAMIF EPOCH res: %d as secondary evt",
			event_info->hw_idx, event_info->res_id);

		sec_evt_data.evt_type = CAM_ISP_HW_SEC_EVENT_EPOCH;
		rc = ife_hw_irq_epoch_cb(ctx->common.cb_priv,
			CAM_ISP_HW_SECONDARY_EVENT, (void *)&sec_evt_data);
	}

	return rc;
}

static int cam_ife_hw_mgr_handle_hw_dump_info(
	void                                 *ctx,
	void                                 *evt_info)
{
	struct cam_ife_hw_mgr_ctx     *ife_hw_mgr_ctx =
		(struct cam_ife_hw_mgr_ctx *)ctx;
	struct cam_isp_hw_event_info  *event_info =
		(struct cam_isp_hw_event_info *)evt_info;
	struct cam_isp_hw_mgr_res     *hw_mgr_res = NULL;
	struct cam_isp_resource_node  *rsrc_node = NULL;
	struct cam_hw_intf            *hw_intf;
	uint32_t i, out_port_id;
	uint64_t dummy_args;
	int rc = 0;

	list_for_each_entry(hw_mgr_res,
		&ife_hw_mgr_ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			rsrc_node = hw_mgr_res->hw_res[i];
			if (rsrc_node->res_id ==
				CAM_ISP_HW_VFE_IN_CAMIF) {
				hw_intf = rsrc_node->hw_intf;
				if (hw_intf &&
					hw_intf->hw_ops.process_cmd)
					rc =
					hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CAMIF_DATA,
					rsrc_node,
					sizeof(
					struct
					cam_isp_resource_node));
			}
		}
	}

	list_for_each_entry(hw_mgr_res,
		&ife_hw_mgr_ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf->hw_ops.process_cmd) {
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CSID_CLOCK_DUMP,
					&dummy_args,
					sizeof(uint64_t));
				if (rc)
					CAM_ERR(CAM_ISP,
						"CSID Clock Dump failed");
			}
		}
	}

	if (event_info->res_type == CAM_ISP_RESOURCE_VFE_OUT) {
		out_port_id = event_info->res_id & 0xFF;
		hw_mgr_res =
			&ife_hw_mgr_ctx->res_list_ife_out[out_port_id];
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf->hw_ops.process_cmd) {
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_DUMP_BUS_INFO,
					(void *)event_info,
					sizeof(struct cam_isp_hw_event_info));
			}
		}
	}

	return rc;
}

static int cam_ife_hw_mgr_handle_sfe_hw_error(
	struct cam_ife_hw_mgr_ctx       *ctx,
	struct cam_isp_hw_event_info    *event_info)
{
	struct cam_isp_hw_error_event_info     *err_evt_info;
	struct cam_isp_hw_error_event_data      error_event_data = {0};
	struct cam_ife_hw_event_recovery_data   recovery_data = {0};

	if (!event_info->event_data) {
		CAM_ERR(CAM_ISP,
			"No additional error event data failed to process for SFE[%u] ctx: %u",
			event_info->hw_idx, ctx->ctx_index);
		return -EINVAL;
	}

	err_evt_info = (struct cam_isp_hw_error_event_info *)event_info->event_data;

	CAM_DBG(CAM_ISP, "SFE[%u] error [%u] on res_type %u",
		event_info->hw_idx, err_evt_info->err_type,
		event_info->res_type);

	spin_lock(&g_ife_hw_mgr.ctx_lock);
	/* Only report error to userspace */
	if (err_evt_info->err_type & CAM_SFE_IRQ_STATUS_VIOLATION) {
		error_event_data.error_type = CAM_ISP_HW_ERROR_VIOLATION;
		error_event_data.error_code = CAM_REQ_MGR_ISP_UNREPORTED_ERROR;
		CAM_DBG(CAM_ISP, "Notify context for SFE error");
		cam_ife_hw_mgr_find_affected_ctx(&error_event_data,
			event_info->hw_idx, &recovery_data);
	}
	spin_unlock(&g_ife_hw_mgr.ctx_lock);

	return 0;
}

static int cam_ife_hw_mgr_handle_hw_err(
	struct cam_ife_hw_mgr_ctx         *ife_hw_mgr_ctx,
	struct cam_isp_hw_event_info      *event_info)
{
	uint32_t                                 core_idx, err_type;
	struct cam_isp_hw_error_event_info      *err_evt_info;
	struct cam_isp_hw_error_event_data       error_event_data = {0};
	struct cam_ife_hw_event_recovery_data    recovery_data = {0};
	int                                      rc = -EINVAL;

	if (!event_info->event_data) {
		CAM_ERR(CAM_ISP,
			"No additional error event data failed to process for IFE[%u] ctx: %u",
			event_info->hw_idx, ife_hw_mgr_ctx->ctx_index);
		return -EINVAL;
	}

	err_evt_info = (struct cam_isp_hw_error_event_info *)event_info->event_data;
	err_type =  err_evt_info->err_type;

	spin_lock(&g_ife_hw_mgr.ctx_lock);
	if (event_info->res_type ==
		CAM_ISP_RESOURCE_VFE_IN &&
		!ife_hw_mgr_ctx->flags.is_rdi_only_context &&
		event_info->res_id !=
		CAM_ISP_HW_VFE_IN_CAMIF)
		cam_ife_hw_mgr_handle_hw_dump_info(ife_hw_mgr_ctx, event_info);

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION)
		error_event_data.error_type = CAM_ISP_HW_ERROR_VIOLATION;
	else if (event_info->res_type == CAM_ISP_RESOURCE_VFE_IN)
		error_event_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;
	else if (event_info->res_type == CAM_ISP_RESOURCE_VFE_OUT)
		error_event_data.error_type = CAM_ISP_HW_ERROR_BUSIF_OVERFLOW;

	core_idx = event_info->hw_idx;

	if (g_ife_hw_mgr.debug_cfg.enable_recovery)
		error_event_data.recovery_enabled = true;

	if (g_ife_hw_mgr.debug_cfg.enable_req_dump)
		error_event_data.enable_req_dump = true;

	error_event_data.error_code = CAM_REQ_MGR_ISP_UNREPORTED_ERROR;

	rc = cam_ife_hw_mgr_find_affected_ctx(&error_event_data,
		core_idx, &recovery_data);

	if (rc || !recovery_data.no_of_context)
		goto end;

	if (err_type == CAM_VFE_IRQ_STATUS_VIOLATION)
		recovery_data.error_type = CAM_ISP_HW_ERROR_VIOLATION;
	else
		recovery_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;

	cam_ife_hw_mgr_do_error_recovery(&recovery_data);
end:
	spin_unlock(&g_ife_hw_mgr.ctx_lock);
	return rc;
}

static int cam_ife_hw_mgr_handle_hw_rup(
	struct cam_ife_hw_mgr_ctx               *ife_hw_mgr_ctx,
	struct cam_isp_hw_event_info            *event_info)
{
	cam_hw_event_cb_func                     ife_hwr_irq_rup_cb;
	struct cam_isp_hw_reg_update_event_data  rup_event_data;

	ife_hwr_irq_rup_cb = ife_hw_mgr_ctx->common.event_cb;

	switch (event_info->res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
		if ((ife_hw_mgr_ctx->flags.is_dual) &&
			(event_info->hw_idx !=
			ife_hw_mgr_ctx->left_hw_idx))
			break;

		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;
		ife_hwr_irq_rup_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_REG_UPDATE, (void *)&rup_event_data);
		break;

	case CAM_ISP_HW_VFE_IN_RDI0:
	case CAM_ISP_HW_VFE_IN_RDI1:
	case CAM_ISP_HW_VFE_IN_RDI2:
	case CAM_ISP_HW_VFE_IN_RDI3:
		if (!ife_hw_mgr_ctx->flags.is_rdi_only_context)
			break;
		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;
		ife_hwr_irq_rup_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_REG_UPDATE, (void *)&rup_event_data);
		break;

	case CAM_ISP_HW_VFE_IN_PDLIB:
	case CAM_ISP_HW_VFE_IN_LCR:
	case CAM_ISP_HW_VFE_IN_RD:
		break;
	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "RUP done for VFE:%d source %d", event_info->hw_idx,
		event_info->res_id);

	return 0;
}

static int cam_ife_hw_mgr_handle_hw_epoch(
	struct cam_ife_hw_mgr_ctx            *ife_hw_mgr_ctx,
	struct cam_isp_hw_event_info         *event_info)
{
	cam_hw_event_cb_func                  ife_hw_irq_epoch_cb;
	struct cam_isp_hw_epoch_event_data    epoch_done_event_data;

	ife_hw_irq_epoch_cb = ife_hw_mgr_ctx->common.event_cb;

	switch (event_info->res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;

		epoch_done_event_data.frame_id_meta = event_info->reg_val;
		ife_hw_irq_epoch_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_EPOCH, (void *)&epoch_done_event_data);

		break;

	case CAM_ISP_HW_VFE_IN_RDI0:
	case CAM_ISP_HW_VFE_IN_RDI1:
	case CAM_ISP_HW_VFE_IN_RDI2:
	case CAM_ISP_HW_VFE_IN_RDI3:
	case CAM_ISP_HW_VFE_IN_PDLIB:
	case CAM_ISP_HW_VFE_IN_LCR:
		break;

	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "Epoch for VFE:%d source %d", event_info->hw_idx,
		event_info->res_id);

	return 0;
}

static int cam_ife_hw_mgr_handle_hw_sof(
	struct cam_ife_hw_mgr_ctx            *ife_hw_mgr_ctx,
	struct cam_isp_hw_event_info         *event_info)
{
	cam_hw_event_cb_func                  ife_hw_irq_sof_cb;
	struct cam_isp_hw_sof_event_data      sof_done_event_data;
	struct timespec64 ts;

	memset(&sof_done_event_data, 0, sizeof(sof_done_event_data));

	ife_hw_irq_sof_cb = ife_hw_mgr_ctx->common.event_cb;

	switch (event_info->res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
	case CAM_ISP_HW_VFE_IN_RD:
		/* if frame header is enabled reset qtimer ts */
		if (ife_hw_mgr_ctx->ctx_config &
			CAM_IFE_CTX_CFG_FRAME_HEADER_TS) {
			sof_done_event_data.timestamp = 0x0;
			ktime_get_boottime_ts64(&ts);
			sof_done_event_data.boot_time =
			(uint64_t)((ts.tv_sec * 1000000000) +
			ts.tv_nsec);
			CAM_DBG(CAM_ISP, "boot_time 0x%llx",
				sof_done_event_data.boot_time);
		} else {
			if (ife_hw_mgr_ctx->flags.is_offline)
				cam_ife_hw_mgr_get_offline_sof_timestamp(
				&sof_done_event_data.timestamp,
				&sof_done_event_data.boot_time);
			else
				cam_ife_mgr_cmd_get_sof_timestamp(
				ife_hw_mgr_ctx,
				&sof_done_event_data.timestamp,
				&sof_done_event_data.boot_time);
		}

		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;

		ife_hw_irq_sof_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_SOF, (void *)&sof_done_event_data);

		break;

	case CAM_ISP_HW_VFE_IN_RDI0:
	case CAM_ISP_HW_VFE_IN_RDI1:
	case CAM_ISP_HW_VFE_IN_RDI2:
	case CAM_ISP_HW_VFE_IN_RDI3:
		if (!ife_hw_mgr_ctx->flags.is_rdi_only_context)
			break;
		cam_ife_mgr_cmd_get_sof_timestamp(ife_hw_mgr_ctx,
			&sof_done_event_data.timestamp,
			&sof_done_event_data.boot_time);
		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;
		ife_hw_irq_sof_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_SOF, (void *)&sof_done_event_data);
		break;

	case CAM_ISP_HW_VFE_IN_PDLIB:
	case CAM_ISP_HW_VFE_IN_LCR:
		break;

	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "SOF for VFE:%d source %d", event_info->hw_idx,
		event_info->res_id);

	return 0;
}

static int cam_ife_hw_mgr_handle_hw_eof(
	struct cam_ife_hw_mgr_ctx            *ife_hw_mgr_ctx,
	struct cam_isp_hw_event_info         *event_info)
{
	cam_hw_event_cb_func                  ife_hw_irq_eof_cb;
	struct cam_isp_hw_eof_event_data      eof_done_event_data;

	ife_hw_irq_eof_cb = ife_hw_mgr_ctx->common.event_cb;

	switch (event_info->res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;

		ife_hw_irq_eof_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_EOF, (void *)&eof_done_event_data);

		break;

	case CAM_ISP_HW_VFE_IN_RDI0:
	case CAM_ISP_HW_VFE_IN_RDI1:
	case CAM_ISP_HW_VFE_IN_RDI2:
	case CAM_ISP_HW_VFE_IN_RDI3:
	case CAM_ISP_HW_VFE_IN_PDLIB:
	case CAM_ISP_HW_VFE_IN_LCR:
		break;

	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "EOF for VFE:%d source %d", event_info->hw_idx,
		event_info->res_id);

	return 0;
}

static int cam_ife_hw_mgr_check_rdi_scratch_buf_done(
	const uint32_t ctx_index, struct cam_sfe_scratch_buf_cfg *scratch_cfg,
	uint32_t res_id, uint32_t last_consumed_addr)
{
	int rc = 0;
	struct cam_sfe_scratch_buf_info *buf_info;
	dma_addr_t final_addr;
	uint32_t cmp_addr = 0;

	switch (res_id) {
	case CAM_ISP_SFE_OUT_RES_RDI_0:
	case CAM_ISP_SFE_OUT_RES_RDI_1:
	case CAM_ISP_SFE_OUT_RES_RDI_2:
		buf_info = &scratch_cfg->buf_info[res_id - CAM_ISP_SFE_OUT_RES_RDI_0];
		if (!buf_info->config_done)
			return 0;

		final_addr = buf_info->io_addr + buf_info->offset;
		cmp_addr = cam_smmu_is_expanded_memory() ?
			CAM_36BIT_INTF_GET_IOVA_BASE(final_addr) : final_addr;
		if (cmp_addr == last_consumed_addr) {
			CAM_DBG(CAM_ISP, "SFE RDI%u buf done for scratch - skip ctx notify",
				(res_id - CAM_ISP_SFE_OUT_RES_BASE));
			rc = -EAGAIN;
		}
		break;
	default:
		break;
	}

	return rc;
}

static int cam_ife_hw_mgr_handle_hw_buf_done(
	struct cam_ife_hw_mgr_ctx        *ife_hw_mgr_ctx,
	struct cam_isp_hw_event_info     *event_info)
{
	cam_hw_event_cb_func                   ife_hwr_irq_wm_done_cb;
	struct cam_isp_hw_done_event_data      buf_done_event_data = {0};
	struct cam_isp_hw_compdone_event_info *compdone_evt_info = NULL;
	int32_t                                rc = 0, i;

	if (!event_info->event_data) {
		CAM_ERR(CAM_ISP,
			"No additional buf done data failed to process for HW: %u",
			event_info->hw_type);
		return -EINVAL;
	}

	ife_hwr_irq_wm_done_cb = ife_hw_mgr_ctx->common.event_cb;
	compdone_evt_info = (struct cam_isp_hw_compdone_event_info *)event_info->event_data;
	buf_done_event_data.num_handles = compdone_evt_info->num_res;

	for (i = 0; i < compdone_evt_info->num_res; i++) {
		buf_done_event_data.resource_handle[i] =
			compdone_evt_info->res_id[i];
		buf_done_event_data.last_consumed_addr[i] =
			compdone_evt_info->last_consumed_addr[i];

		CAM_DBG(CAM_ISP, "Buf done for %s: %d res_id: 0x%x last consumed addr: 0x%x",
		((event_info->hw_type == CAM_ISP_HW_TYPE_SFE) ? "SFE" : "IFE"),
		event_info->hw_idx, compdone_evt_info->res_id[i],
		compdone_evt_info->last_consumed_addr[i]);

		if (cam_ife_hw_mgr_is_shdr_fs_rdi_res(compdone_evt_info->res_id[i],
			ife_hw_mgr_ctx->flags.is_sfe_shdr,
			ife_hw_mgr_ctx->flags.is_sfe_fs)) {
			rc = cam_ife_hw_mgr_check_rdi_scratch_buf_done(
				ife_hw_mgr_ctx->ctx_index,
				ife_hw_mgr_ctx->sfe_info.scratch_config,
				compdone_evt_info->res_id[i],
				compdone_evt_info->last_consumed_addr[i]);
			if (rc)
				goto end;
		}
	}


	if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
		return 0;

	if (buf_done_event_data.num_handles > 0 && ife_hwr_irq_wm_done_cb) {
		CAM_DBG(CAM_ISP, "Notify ISP context");
		ife_hwr_irq_wm_done_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_DONE, (void *)&buf_done_event_data);
	}

end:
	return 0;
}

static int cam_ife_hw_mgr_handle_ife_event(
	struct cam_ife_hw_mgr_ctx           *ctx,
	uint32_t                             evt_id,
	struct cam_isp_hw_event_info        *event_info)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "Handle IFE[%u] %s event in ctx: %u",
		event_info->hw_idx,
		__cam_ife_hw_mgr_evt_type_to_string(evt_id),
		ctx->ctx_index);

	switch (evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
		rc = cam_ife_hw_mgr_handle_hw_sof(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_REG_UPDATE:
		rc = cam_ife_hw_mgr_handle_hw_rup(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_EPOCH:
		rc = cam_ife_hw_mgr_handle_hw_epoch(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_EOF:
		rc = cam_ife_hw_mgr_handle_hw_eof(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_DONE:
		rc = cam_ife_hw_mgr_handle_hw_buf_done(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_ERROR:
		rc = cam_ife_hw_mgr_handle_hw_err(ctx, event_info);
		break;

	default:
		CAM_ERR(CAM_ISP, "Event: %u not handled for IFE", evt_id);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_ife_hw_mgr_handle_csid_event(
	struct cam_ife_hw_mgr_ctx       *ctx,
	uint32_t                         evt_id,
	struct cam_isp_hw_event_info    *event_info)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "Handle CSID[%u] %s event in ctx: %u",
		event_info->hw_idx,
		__cam_ife_hw_mgr_evt_type_to_string(evt_id),
		ctx->ctx_index);

	switch (evt_id) {
	case CAM_ISP_HW_EVENT_REG_UPDATE:
		rc = cam_ife_hw_mgr_handle_csid_rup(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_ERROR:
		rc = cam_ife_hw_mgr_handle_csid_error(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_SOF:
		rc = cam_ife_hw_mgr_handle_csid_camif_sof(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_EPOCH:
		rc = cam_ife_hw_mgr_handle_csid_camif_epoch(ctx, event_info);
		break;

	default:
		CAM_ERR(CAM_ISP, "Event: %u not handled for CSID", evt_id);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_ife_hw_mgr_handle_sfe_event(
	struct cam_ife_hw_mgr_ctx       *ctx,
	uint32_t                         evt_id,
	struct cam_isp_hw_event_info    *event_info)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "Handle SFE[%u] %s event in ctx: %u",
		event_info->hw_idx,
		__cam_ife_hw_mgr_evt_type_to_string(evt_id),
		ctx->ctx_index);

	switch (evt_id) {
	case CAM_ISP_HW_EVENT_ERROR:
		rc = cam_ife_hw_mgr_handle_sfe_hw_error(ctx, event_info);
		break;

	case CAM_ISP_HW_EVENT_DONE:
		rc = cam_ife_hw_mgr_handle_hw_buf_done(ctx, event_info);
		break;

	default:
		CAM_WARN(CAM_ISP, "Event: %u not handled for SFE", evt_id);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_ife_hw_mgr_event_handler(
	void                                *priv,
	uint32_t                             evt_id,
	void                                *evt_info)
{
	int rc = -EINVAL;
	struct cam_ife_hw_mgr_ctx           *ctx;
	struct cam_isp_hw_event_info        *event_info;

	if (!evt_info || !priv) {
		CAM_ERR(CAM_ISP,
			"Invalid data evt_info: %pK priv: %pK",
			evt_info, priv);
		return rc;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)priv;
	event_info = (struct cam_isp_hw_event_info *)evt_info;

	switch (event_info->hw_type) {
	case CAM_ISP_HW_TYPE_CSID:
		rc = cam_ife_hw_mgr_handle_csid_event(ctx, evt_id, event_info);
		break;

	case CAM_ISP_HW_TYPE_SFE:
		rc = cam_ife_hw_mgr_handle_sfe_event(ctx, evt_id, event_info);
		break;

	case CAM_ISP_HW_TYPE_VFE:
		rc = cam_ife_hw_mgr_handle_ife_event(ctx, evt_id, event_info);
		break;

	default:
		break;
	}

	if (rc)
		CAM_ERR(CAM_ISP, "Failed to handle %s [%u] event from hw %u in ctx %u rc %d",
			__cam_ife_hw_mgr_evt_type_to_string(evt_id),
			evt_id, event_info->hw_type, ctx->ctx_index, rc);

	return rc;
}

static int cam_ife_hw_mgr_sort_dev_with_caps(
	struct cam_ife_hw_mgr *ife_hw_mgr)
{
	int i;

	/* get caps for csid devices */
	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (!ife_hw_mgr->csid_devices[i])
			continue;

		if (!ife_hw_mgr->csid_devices[i]->hw_ops.get_hw_caps)
			continue;

		ife_hw_mgr->csid_devices[i]->hw_ops.get_hw_caps(
			ife_hw_mgr->csid_devices[i]->hw_priv,
			&ife_hw_mgr->csid_hw_caps[i],
			sizeof(ife_hw_mgr->csid_hw_caps[i]));

		ife_hw_mgr->csid_global_reset_en =
			ife_hw_mgr->csid_hw_caps[i].global_reset_en;
		ife_hw_mgr->csid_rup_en =
			ife_hw_mgr->csid_hw_caps[i].rup_en;
	}

	/* get caps for ife devices */
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (!ife_hw_mgr->ife_devices[i])
			continue;
		if (ife_hw_mgr->ife_devices[i]->hw_intf->hw_ops.get_hw_caps) {
			ife_hw_mgr->ife_devices[i]->hw_intf->hw_ops.get_hw_caps(
				ife_hw_mgr->ife_devices[i]->hw_intf->hw_priv,
				&ife_hw_mgr->ife_dev_caps[i],
				sizeof(ife_hw_mgr->ife_dev_caps[i]));
		}
	}

	return 0;
}

static int cam_ife_set_sfe_cache_debug(void *data, u64 val)
{
	int i, rc = -EINVAL;
	uint32_t hw_idx = 0;
	struct cam_sfe_debug_cfg_params debug_cfg;
	struct cam_hw_intf *hw_intf = NULL;

	debug_cfg.cache_config = true;

	/* BITS [0:3] is for hw_idx */
	hw_idx = val & 0xF;
	for (i = 0; i < CAM_SFE_HW_NUM_MAX; i++) {
		if ((g_ife_hw_mgr.sfe_devices[i]) && (i == hw_idx)) {
			hw_intf = g_ife_hw_mgr.sfe_devices[i]->hw_intf;

			debug_cfg.u.cache_cfg.sfe_cache_dbg = (val >> 4);
			g_ife_hw_mgr.debug_cfg.sfe_cache_debug[i] =
				debug_cfg.u.cache_cfg.sfe_cache_dbg;
			rc = hw_intf->hw_ops.process_cmd(hw_intf->hw_priv,
				CAM_ISP_HW_CMD_SET_SFE_DEBUG_CFG,
				&debug_cfg,
				sizeof(struct cam_sfe_debug_cfg_params));
		}
	}

	CAM_DBG(CAM_ISP, "Set SFE cache debug value: 0x%llx", val);
	return rc;
}

static int cam_ife_get_sfe_cache_debug(void *data, u64 *val)
{
	*val = g_ife_hw_mgr.debug_cfg.sfe_cache_debug[CAM_SFE_CORE_1];
	*val = *val << 32;
	*val |=  g_ife_hw_mgr.debug_cfg.sfe_cache_debug[CAM_SFE_CORE_0];
	CAM_DBG(CAM_ISP, "Get SFE cace debug value: 0x%llx", *val);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cam_ife_sfe_cache_debug,
	cam_ife_get_sfe_cache_debug,
	cam_ife_set_sfe_cache_debug, "%16llu");

static int cam_ife_set_csid_debug(void *data, u64 val)
{
	g_ife_hw_mgr.debug_cfg.csid_debug = val;
	CAM_DBG(CAM_ISP, "Set CSID Debug value :%lld", val);
	return 0;
}

static int cam_ife_get_csid_debug(void *data, u64 *val)
{
	*val = g_ife_hw_mgr.debug_cfg.csid_debug;
	CAM_DBG(CAM_ISP, "Get CSID Debug value :%lld",
		g_ife_hw_mgr.debug_cfg.csid_debug);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cam_ife_csid_debug,
	cam_ife_get_csid_debug,
	cam_ife_set_csid_debug, "%16llu");

static int cam_ife_set_camif_debug(void *data, u64 val)
{
	g_ife_hw_mgr.debug_cfg.camif_debug = val;
	CAM_DBG(CAM_ISP,
		"Set camif enable_diag_sensor_status value :%lld", val);
	return 0;
}

static int cam_ife_get_camif_debug(void *data, u64 *val)
{
	*val = g_ife_hw_mgr.debug_cfg.camif_debug;
	CAM_DBG(CAM_ISP,
		"Set camif enable_diag_sensor_status value :%lld",
		g_ife_hw_mgr.debug_cfg.csid_debug);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cam_ife_camif_debug,
	cam_ife_get_camif_debug,
	cam_ife_set_camif_debug, "%16llu");

static int cam_ife_set_sfe_debug(void *data, u64 val)
{
	g_ife_hw_mgr.debug_cfg.sfe_debug = (uint32_t)val;
	CAM_DBG(CAM_ISP, "Set SFE Debug value :%u",
		g_ife_hw_mgr.debug_cfg.sfe_debug);
	return 0;
}

static int cam_ife_get_sfe_debug(void *data, u64 *val)
{
	*val = (uint64_t)g_ife_hw_mgr.debug_cfg.sfe_debug;
	CAM_DBG(CAM_ISP, "Get SFE Debug value :%u",
		g_ife_hw_mgr.debug_cfg.sfe_debug);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cam_ife_sfe_debug,
	cam_ife_get_sfe_debug,
	cam_ife_set_sfe_debug, "%16llu");

static int cam_ife_set_sfe_sensor_diag_debug(void *data, u64 val)
{
	g_ife_hw_mgr.debug_cfg.sfe_sensor_diag_cfg = (uint32_t)val;
	CAM_DBG(CAM_ISP, "Set SFE Sensor diag value :%u",
		g_ife_hw_mgr.debug_cfg.sfe_sensor_diag_cfg);
	return 0;
}

static int cam_ife_get_sfe_sensor_diag_debug(void *data, u64 *val)
{
	*val = (uint64_t)g_ife_hw_mgr.debug_cfg.sfe_sensor_diag_cfg;
	CAM_DBG(CAM_ISP, "Get SFE Sensor diag value :%u",
		g_ife_hw_mgr.debug_cfg.sfe_sensor_diag_cfg);
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cam_ife_sfe_sensor_diag_debug,
	cam_ife_get_sfe_sensor_diag_debug,
	cam_ife_set_sfe_sensor_diag_debug, "%16llu");

static int cam_ife_hw_mgr_debug_register(void)
{
	int rc = 0;
	struct dentry *dbgfileptr = NULL;

	dbgfileptr = debugfs_create_dir("camera_ife", NULL);
	if (!dbgfileptr) {
		CAM_ERR(CAM_ISP,"DebugFS could not create directory!");
		rc = -ENOENT;
		goto end;
	}
	/* Store parent inode for cleanup in caller */
	g_ife_hw_mgr.debug_cfg.dentry = dbgfileptr;

	dbgfileptr = debugfs_create_file("ife_csid_debug", 0644,
		g_ife_hw_mgr.debug_cfg.dentry, NULL, &cam_ife_csid_debug);
	debugfs_create_u32("enable_recovery", 0644, g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.enable_recovery);
	dbgfileptr = debugfs_create_bool("enable_req_dump", 0644,
		g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.enable_req_dump);
	debugfs_create_u32("enable_csid_recovery", 0644,
		g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.enable_csid_recovery);
	dbgfileptr = debugfs_create_file("ife_camif_debug", 0644,
		g_ife_hw_mgr.debug_cfg.dentry, NULL, &cam_ife_camif_debug);
	dbgfileptr = debugfs_create_bool("per_req_reg_dump", 0644,
		g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.per_req_reg_dump);
	dbgfileptr = debugfs_create_bool("disable_ubwc_comp", 0644,
		g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.disable_ubwc_comp);
	dbgfileptr = debugfs_create_file("sfe_debug", 0644,
		g_ife_hw_mgr.debug_cfg.dentry, NULL, &cam_ife_sfe_debug);
	dbgfileptr = debugfs_create_file("sfe_sensor_diag_sel", 0644,
		g_ife_hw_mgr.debug_cfg.dentry, NULL, &cam_ife_sfe_sensor_diag_debug);
	dbgfileptr = debugfs_create_bool("disable_ife_mmu_prefetch", 0644,
		g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.disable_ife_mmu_prefetch);
	dbgfileptr = debugfs_create_file("sfe_cache_debug", 0644,
		g_ife_hw_mgr.debug_cfg.dentry, NULL, &cam_ife_sfe_cache_debug);

	if (IS_ERR(dbgfileptr)) {
		if (PTR_ERR(dbgfileptr) == -ENODEV)
			CAM_WARN(CAM_ISP, "DebugFS not enabled in kernel!");
		else
			rc = PTR_ERR(dbgfileptr);
	}
end:
	g_ife_hw_mgr.debug_cfg.enable_csid_recovery = 1;
	return rc;
}

static void cam_req_mgr_process_workq_cam_ife_worker(struct work_struct *w)
{
	cam_req_mgr_process_workq(w);
}

static unsigned long cam_ife_hw_mgr_mini_dump_cb(void *dst, unsigned long len)
{
	struct cam_ife_hw_mini_dump_data   *mgr_md;
	struct cam_ife_hw_mini_dump_ctx    *ctx_md;
	struct cam_ife_hw_mgr_ctx          *ctx_temp;
	struct cam_ife_hw_mgr_ctx          *ctx;
	uint32_t                            j;
	uint32_t                            hw_idx = 0;
	struct cam_hw_intf                 *hw_intf = NULL;
	struct cam_ife_hw_mgr              *hw_mgr;
	struct cam_hw_mini_dump_args        hw_dump_args;
	unsigned long                       remain_len = len;
	unsigned long                       dumped_len = 0;
	uint32_t                            i = 0;
	int                                 rc = 0;

	if (len < sizeof(*mgr_md)) {
		CAM_ERR(CAM_ISP, "Insufficent received length: %u",
			len);
		return 0;
	}

	mgr_md = (struct cam_ife_hw_mini_dump_data *)dst;
	mgr_md->num_ctx = 0;
	hw_mgr = &g_ife_hw_mgr;
	dumped_len += sizeof(*mgr_md);
	remain_len -= dumped_len;

	list_for_each_entry_safe(ctx, ctx_temp,
		&hw_mgr->used_ctx_list, list) {

		if (remain_len < sizeof(*ctx_md)) {
			CAM_ERR(CAM_ISP,
			"Insufficent received length: %u, dumped_len %u",
			len, dumped_len);
			goto end;
		}

		ctx_md = (struct cam_ife_hw_mini_dump_ctx *)
				((uint8_t *)dst + dumped_len);
		mgr_md->ctx[i] = ctx_md;
		ctx_md->ctx_index = ctx->ctx_index;
		ctx_md->left_hw_idx = ctx->left_hw_idx;
		ctx_md->right_hw_idx = ctx->right_hw_idx;
		ctx_md->cdm_handle = ctx->cdm_handle;
		ctx_md->num_base = ctx->num_base;
		ctx_md->cdm_id = ctx->cdm_id;
		ctx_md->last_cdm_done_req = ctx->last_cdm_done_req;
		ctx_md->applied_req_id = ctx->applied_req_id;
		ctx_md->ctx_type = ctx->ctx_type;
		ctx_md->overflow_pending =
			atomic_read(&ctx->overflow_pending);
		ctx_md->cdm_done = atomic_read(&ctx->cdm_done);
		memcpy(&ctx_md->pf_info, &ctx->pf_info,
			sizeof(struct cam_ife_hw_mgr_ctx_pf_info));
		memcpy(&ctx_md->flags, &ctx->flags,
			sizeof(struct cam_ife_hw_mgr_ctx_flags));

		dumped_len += sizeof(*ctx_md);

		for (j = 0; j < ctx->num_base; j++) {
			memcpy(&ctx_md->base[j], &ctx->base[j],
				sizeof(struct cam_isp_ctx_base_info));
			hw_idx = ctx->base[j].idx;
			if (ctx->base[j].hw_type == CAM_ISP_HW_TYPE_CSID) {
				hw_intf = hw_mgr->csid_devices[hw_idx];
				ctx_md->csid_md[hw_idx] = (void *)((uint8_t *)dst + dumped_len);
				memset(&hw_dump_args, 0, sizeof(hw_dump_args));
				hw_dump_args.start_addr = ctx_md->csid_md[hw_idx];
				hw_dump_args.len = remain_len;
				hw_intf->hw_ops.process_cmd(hw_intf->hw_priv,
					CAM_ISP_HW_CSID_MINI_DUMP, &hw_dump_args,
					sizeof(hw_dump_args));
				if (hw_dump_args.bytes_written == 0)
					goto end;
				dumped_len += hw_dump_args.bytes_written;
				remain_len = len - dumped_len;
			} else if (ctx->base[j].hw_type ==
				CAM_ISP_HW_TYPE_VFE) {
				hw_intf = hw_mgr->ife_devices[hw_idx]->hw_intf;
				ctx_md->vfe_md[hw_idx] = (void *)((uint8_t *)dst + dumped_len);
				memset(&hw_dump_args, 0, sizeof(hw_dump_args));
				hw_dump_args.start_addr = ctx_md->vfe_md[hw_idx];
				hw_dump_args.len = remain_len;
				hw_intf->hw_ops.process_cmd(hw_intf->hw_priv,
					CAM_ISP_HW_BUS_MINI_DUMP, &hw_dump_args,
					sizeof(hw_dump_args));
				if (hw_dump_args.bytes_written == 0)
					goto end;
				dumped_len += hw_dump_args.bytes_written;
				remain_len = len - dumped_len;
			}
		}

		if (ctx->common.mini_dump_cb) {
			hw_dump_args.start_addr = (void *)((uint8_t *)dst + dumped_len);
			hw_dump_args.len = remain_len;
			hw_dump_args.bytes_written = 0;
			rc = ctx->common.mini_dump_cb(ctx->common.cb_priv, &hw_dump_args);
			if (rc || (hw_dump_args.bytes_written + dumped_len > len))
				goto end;

			ctx_md->ctx_priv = hw_dump_args.start_addr;
			dumped_len += hw_dump_args.bytes_written;
			remain_len = len - dumped_len;
		}

		i++;
	}
end:
	mgr_md->num_ctx = i;
	return dumped_len;
}

int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl)
{
	int rc = -EFAULT;
	int i, j;
	struct cam_iommu_handle cdm_handles;
	struct cam_ife_hw_mgr_ctx *ctx_pool;
	struct cam_isp_hw_mgr_res *res_list_ife_out;
	struct cam_isp_hw_bus_cap isp_bus_cap = {0};
	struct cam_isp_hw_path_port_map path_port_map;
	struct cam_isp_hw_mgr_res *res_list_sfe_out;

	memset(&g_ife_hw_mgr, 0, sizeof(g_ife_hw_mgr));
	memset(&path_port_map, 0, sizeof(path_port_map));

	mutex_init(&g_ife_hw_mgr.ctx_mutex);
	spin_lock_init(&g_ife_hw_mgr.ctx_lock);

	if (CAM_IFE_HW_NUM_MAX != CAM_IFE_CSID_HW_NUM_MAX) {
		CAM_ERR(CAM_ISP, "CSID num is different then IFE num");
		return -EINVAL;
	}

	/* fill ife hw intf information */
	for (i = 0, j = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		rc = cam_vfe_hw_init(&g_ife_hw_mgr.ife_devices[i], i);
		if (!rc) {
			struct cam_hw_intf *ife_device =
				g_ife_hw_mgr.ife_devices[i]->hw_intf;
			struct cam_hw_info *vfe_hw =
				(struct cam_hw_info *)
				ife_device->hw_priv;
			struct cam_hw_soc_info *soc_info = &vfe_hw->soc_info;

			if (j == 0) {
				ife_device->hw_ops.process_cmd(
					vfe_hw,
					CAM_ISP_HW_CMD_QUERY_BUS_CAP,
					&isp_bus_cap,
					sizeof(struct cam_isp_hw_bus_cap));
				CAM_DBG(CAM_ISP, "max VFE out resources: 0x%x",
					isp_bus_cap.max_out_res_type);

				ife_device->hw_ops.process_cmd(
					vfe_hw,
					CAM_ISP_HW_CMD_GET_PATH_PORT_MAP,
					&path_port_map,
					sizeof(struct cam_isp_hw_path_port_map));
				CAM_DBG(CAM_ISP, "received %d path-port mappings",
					path_port_map.num_entries);
			}

			j++;
			g_ife_hw_mgr.cdm_reg_map[i] = &soc_info->reg_map[0];
			CAM_DBG(CAM_ISP,
				"reg_map: mem base = %pK cam_base = 0x%llx",
				(void __iomem *)soc_info->reg_map[0].mem_base,
				(uint64_t) soc_info->reg_map[0].mem_cam_base);

			if (g_ife_hw_mgr.ife_devices[i]->num_hw_pid)
				g_ife_hw_mgr.hw_pid_support = true;

		} else {
			g_ife_hw_mgr.cdm_reg_map[i] = NULL;
		}
	}
	if (j == 0) {
		CAM_ERR(CAM_ISP, "no valid IFE HW");
		return -EINVAL;
	}

	g_ife_hw_mgr.isp_bus_caps.support_consumed_addr =
		isp_bus_cap.support_consumed_addr;
	g_ife_hw_mgr.isp_bus_caps.max_vfe_out_res_type =
		isp_bus_cap.max_out_res_type;
	max_ife_out_res =
		g_ife_hw_mgr.isp_bus_caps.max_vfe_out_res_type & 0xFF;
	memset(&isp_bus_cap, 0x0, sizeof(struct cam_isp_hw_bus_cap));

	for (i = 0; i < path_port_map.num_entries; i++) {
		g_ife_hw_mgr.path_port_map.entry[i][0] = path_port_map.entry[i][0];
		g_ife_hw_mgr.path_port_map.entry[i][1] = path_port_map.entry[i][1];
	}
	g_ife_hw_mgr.path_port_map.num_entries = path_port_map.num_entries;

	/* fill csid hw intf information */
	for (i = 0, j = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		rc = cam_ife_csid_hw_init(&g_ife_hw_mgr.csid_devices[i], i);
		if (!rc)
			j++;
	}
	if (!j) {
		CAM_ERR(CAM_ISP, "no valid IFE CSID HW");
		return -EINVAL;
	}

	/* fill sfe hw intf info */
	for (i = 0, j = 0; i < CAM_SFE_HW_NUM_MAX; i++) {
		rc = cam_sfe_hw_init(&g_ife_hw_mgr.sfe_devices[i], i);
		if (!rc) {
			if (j == 0) {
				struct cam_hw_intf *sfe_device =
					g_ife_hw_mgr.sfe_devices[i]->hw_intf;
				struct cam_hw_info *sfe_hw =
					(struct cam_hw_info *)
					sfe_device->hw_priv;

				rc = sfe_device->hw_ops.process_cmd(
					sfe_hw,
					CAM_ISP_HW_CMD_QUERY_BUS_CAP,
					&isp_bus_cap,
					sizeof(struct cam_isp_hw_bus_cap));
				CAM_DBG(CAM_ISP, "max SFE out resources: 0x%x",
					isp_bus_cap.max_out_res_type);
				if (!rc)
					g_ife_hw_mgr.isp_bus_caps.max_sfe_out_res_type =
						isp_bus_cap.max_out_res_type;

				if (g_ife_hw_mgr.sfe_devices[i]->num_hw_pid)
					g_ife_hw_mgr.hw_pid_support = true;
			}
			j++;
		}
	}
	if (!j)
		CAM_ERR(CAM_ISP, "no valid SFE HW devices");

	cam_ife_hw_mgr_sort_dev_with_caps(&g_ife_hw_mgr);

	/* setup ife context list */
	INIT_LIST_HEAD(&g_ife_hw_mgr.free_ctx_list);
	INIT_LIST_HEAD(&g_ife_hw_mgr.used_ctx_list);

	/*
	 *  for now, we only support one iommu handle. later
	 *  we will need to setup more iommu handle for other
	 *  use cases.
	 *  Also, we have to release them once we have the
	 *  deinit support
	 */
	rc = cam_smmu_get_handle("ife",
		&g_ife_hw_mgr.mgr_common.img_iommu_hdl);

	if (rc && rc != -EALREADY) {
		CAM_ERR(CAM_ISP, "Can not get iommu handle");
		return -EINVAL;
	}

	if (cam_smmu_get_handle("cam-secure",
		&g_ife_hw_mgr.mgr_common.img_iommu_hdl_secure)) {
		CAM_ERR(CAM_ISP, "Failed to get secure iommu handle");
		goto secure_fail;
	}

	CAM_DBG(CAM_ISP, "iommu_handles: non-secure[0x%x], secure[0x%x]",
		g_ife_hw_mgr.mgr_common.img_iommu_hdl,
		g_ife_hw_mgr.mgr_common.img_iommu_hdl_secure);

	if (!cam_cdm_get_iommu_handle("ife3", &cdm_handles)) {
		CAM_DBG(CAM_ISP,
			"Successfully acquired CDM iommu handles 0x%x, 0x%x",
			cdm_handles.non_secure, cdm_handles.secure);
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl = cdm_handles.non_secure;
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl_secure =
			cdm_handles.secure;
	} else {
		CAM_ERR(CAM_ISP, "Failed to acquire CDM iommu handle");
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl = -1;
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl_secure = -1;
	}

	atomic_set(&g_ife_hw_mgr.active_ctx_cnt, 0);
	for (i = 0; i < CAM_IFE_CTX_MAX; i++) {
		memset(&g_ife_hw_mgr.ctx_pool[i], 0,
			sizeof(g_ife_hw_mgr.ctx_pool[i]));
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].list);

		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_in.list);
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_csid);
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_src);
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_sfe_src);
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_in_rd);
		ctx_pool = &g_ife_hw_mgr.ctx_pool[i];
		ctx_pool->res_list_ife_out = kzalloc((max_ife_out_res *
			sizeof(struct cam_isp_hw_mgr_res)), GFP_KERNEL);
		if (!ctx_pool->res_list_ife_out) {
			rc = -ENOMEM;
			CAM_ERR(CAM_ISP, "Alloc failed for ife out res list");
			goto end;
		}

		for (j = 0; j < max_ife_out_res; j++) {
			res_list_ife_out = &ctx_pool->res_list_ife_out[j];
			INIT_LIST_HEAD(&res_list_ife_out->list);
		}

		for (j = 0; j < CAM_SFE_HW_OUT_RES_MAX; j++) {
			res_list_sfe_out = &ctx_pool->res_list_sfe_out[j];
			INIT_LIST_HEAD(&res_list_sfe_out->list);
		}

		/* init context pool */
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].free_res_list);
		for (j = 0; j < CAM_IFE_HW_RES_POOL_MAX; j++) {
			INIT_LIST_HEAD(
				&g_ife_hw_mgr.ctx_pool[i].res_pool[j].list);
			list_add_tail(
				&g_ife_hw_mgr.ctx_pool[i].res_pool[j].list,
				&g_ife_hw_mgr.ctx_pool[i].free_res_list);
		}

		g_ife_hw_mgr.ctx_pool[i].ctx_index = i;
		g_ife_hw_mgr.ctx_pool[i].hw_mgr = &g_ife_hw_mgr;

		cam_tasklet_init(&g_ife_hw_mgr.mgr_common.tasklet_pool[i],
			&g_ife_hw_mgr.ctx_pool[i], i);
		g_ife_hw_mgr.ctx_pool[i].common.tasklet_info =
			g_ife_hw_mgr.mgr_common.tasklet_pool[i];

		init_completion(&g_ife_hw_mgr.ctx_pool[i].config_done_complete);
		list_add_tail(&g_ife_hw_mgr.ctx_pool[i].list,
			&g_ife_hw_mgr.free_ctx_list);
	}

	/* Create Worker for ife_hw_mgr with 10 tasks */
	rc = cam_req_mgr_workq_create("cam_ife_worker", 10,
			&g_ife_hw_mgr.workq, CRM_WORKQ_USAGE_NON_IRQ, 0,
			cam_req_mgr_process_workq_cam_ife_worker);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "Unable to create worker");
		goto end;
	}

	/* Populate sys cache info */
	g_ife_hw_mgr.num_caches_found = 0;
	for (i = CAM_LLCC_SMALL_1; i < CAM_LLCC_MAX; i++) {
		g_ife_hw_mgr.sys_cache_info[i].scid =
			cam_cpas_get_scid(i);
		g_ife_hw_mgr.sys_cache_info[i].type = i;
		CAM_DBG(CAM_ISP, "Cache_%d scid: %d",
			i, g_ife_hw_mgr.sys_cache_info[i].scid);
		if (g_ife_hw_mgr.sys_cache_info[i].scid > 0)
			g_ife_hw_mgr.num_caches_found++;
	}

	/* fill return structure */
	hw_mgr_intf->hw_mgr_priv = &g_ife_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_ife_mgr_get_hw_caps;
	hw_mgr_intf->hw_acquire = cam_ife_mgr_acquire;
	hw_mgr_intf->hw_start = cam_ife_mgr_start_hw;
	hw_mgr_intf->hw_stop = cam_ife_mgr_stop_hw;
	hw_mgr_intf->hw_read = cam_ife_mgr_read;
	hw_mgr_intf->hw_write = cam_ife_mgr_write;
	hw_mgr_intf->hw_release = cam_ife_mgr_release_hw;
	hw_mgr_intf->hw_prepare_update = cam_ife_mgr_prepare_hw_update;
	hw_mgr_intf->hw_config = cam_ife_mgr_config_hw;
	hw_mgr_intf->hw_cmd = cam_ife_mgr_cmd;
	hw_mgr_intf->hw_reset = cam_ife_mgr_reset;
	hw_mgr_intf->hw_dump = cam_ife_mgr_dump;
	hw_mgr_intf->hw_recovery = cam_ife_mgr_recover_hw;

	if (iommu_hdl)
		*iommu_hdl = g_ife_hw_mgr.mgr_common.img_iommu_hdl;

	cam_ife_hw_mgr_debug_register();
	cam_ife_mgr_count_ife();
	cam_common_register_mini_dump_cb(cam_ife_hw_mgr_mini_dump_cb,
		"CAM_ISP");

	CAM_DBG(CAM_ISP, "Exit");

	return 0;
end:
	if (rc) {
		for (i = 0; i < CAM_IFE_CTX_MAX; i++) {
			cam_tasklet_deinit(
				&g_ife_hw_mgr.mgr_common.tasklet_pool[i]);
			g_ife_hw_mgr.ctx_pool[i].cdm_cmd = NULL;
			kfree(g_ife_hw_mgr.ctx_pool[i].res_list_ife_out);
			g_ife_hw_mgr.ctx_pool[i].res_list_ife_out = NULL;
			g_ife_hw_mgr.ctx_pool[i].common.tasklet_info = NULL;
		}
	}
	cam_smmu_destroy_handle(
		g_ife_hw_mgr.mgr_common.img_iommu_hdl_secure);
	g_ife_hw_mgr.mgr_common.img_iommu_hdl_secure = -1;
secure_fail:
	cam_smmu_destroy_handle(g_ife_hw_mgr.mgr_common.img_iommu_hdl);
	g_ife_hw_mgr.mgr_common.img_iommu_hdl = -1;
	return rc;
}

void cam_ife_hw_mgr_deinit(void)
{
	int i = 0;

	cam_req_mgr_workq_destroy(&g_ife_hw_mgr.workq);
	debugfs_remove_recursive(g_ife_hw_mgr.debug_cfg.dentry);
	g_ife_hw_mgr.debug_cfg.dentry = NULL;

	for (i = 0; i < CAM_IFE_CTX_MAX; i++) {
		cam_tasklet_deinit(
			&g_ife_hw_mgr.mgr_common.tasklet_pool[i]);
		g_ife_hw_mgr.ctx_pool[i].cdm_cmd = NULL;
		kfree(g_ife_hw_mgr.ctx_pool[i].res_list_ife_out);
		g_ife_hw_mgr.ctx_pool[i].res_list_ife_out = NULL;
		g_ife_hw_mgr.ctx_pool[i].common.tasklet_info = NULL;
	}

	cam_smmu_destroy_handle(
		g_ife_hw_mgr.mgr_common.img_iommu_hdl_secure);
	g_ife_hw_mgr.mgr_common.img_iommu_hdl_secure = -1;

	cam_smmu_destroy_handle(g_ife_hw_mgr.mgr_common.img_iommu_hdl);
	g_ife_hw_mgr.mgr_common.img_iommu_hdl = -1;
	g_ife_hw_mgr.num_caches_found = 0;
}
