// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <soc/qcom/scm.h>
#include <uapi/media/cam_isp.h>
#include "cam_smmu_api.h"
#include "cam_req_mgr_workq.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_vfe_hw_intf.h"
#include "cam_isp_packet_parser.h"
#include "cam_ife_hw_mgr.h"
#include "cam_cdm_intf_api.h"
#include "cam_packet_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_mem_mgr_api.h"
#include "cam_common_util.h"

#define CAM_IFE_HW_ENTRIES_MAX  20

#define TZ_SVC_SMMU_PROGRAM 0x15
#define TZ_SAFE_SYSCALL_ID  0x3
#define CAM_IFE_SAFE_DISABLE 0
#define CAM_IFE_SAFE_ENABLE 1
#define SMMU_SE_IFE 0

#define CAM_ISP_PACKET_META_MAX                     \
	(CAM_ISP_PACKET_META_GENERIC_BLOB_COMMON + 1)

#define CAM_ISP_GENERIC_BLOB_TYPE_MAX               \
	(CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG_V2 + 1)

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
};

static struct cam_ife_hw_mgr g_ife_hw_mgr;

static int cam_ife_hw_mgr_event_handler(
	void                                *priv,
	uint32_t                             evt_id,
	void                                *evt_info);

static int cam_ife_mgr_regspace_data_cb(uint32_t reg_base_type,
	void *hw_mgr_ctx, struct cam_hw_soc_info **soc_info_ptr,
	uint32_t *reg_base_idx)
{
	int rc = 0;
	struct cam_ife_hw_mgr_res *hw_mgr_res;
	struct cam_ife_hw_mgr_res *hw_mgr_res_temp;
	struct cam_hw_soc_info    *soc_info = NULL;
	struct cam_ife_hw_mgr_ctx *ctx =
		(struct cam_ife_hw_mgr_ctx *) hw_mgr_ctx;

	*soc_info_ptr = NULL;
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&ctx->res_list_ife_src, list) {
		if (hw_mgr_res->res_id != CAM_ISP_HW_VFE_IN_CAMIF)
			continue;

		switch (reg_base_type) {
		case CAM_REG_DUMP_BASE_TYPE_CAMNOC:
		case CAM_REG_DUMP_BASE_TYPE_ISP_LEFT:
			if (!hw_mgr_res->hw_res[CAM_ISP_HW_SPLIT_LEFT])
				continue;

			rc = hw_mgr_res->hw_res[
				CAM_ISP_HW_SPLIT_LEFT]->process_cmd(
				hw_mgr_res->hw_res[CAM_ISP_HW_SPLIT_LEFT],
				CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA, &soc_info,
				sizeof(void *));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Failed in regspace data query split idx: %d rc : %d",
					CAM_ISP_HW_SPLIT_LEFT, rc);
				return rc;
			}

			if (reg_base_type == CAM_REG_DUMP_BASE_TYPE_ISP_LEFT)
				*reg_base_idx = 0;
			else
				*reg_base_idx = 1;

			*soc_info_ptr = soc_info;
			break;
		case CAM_REG_DUMP_BASE_TYPE_ISP_RIGHT:
			if (!hw_mgr_res->hw_res[CAM_ISP_HW_SPLIT_RIGHT])
				continue;

			rc = hw_mgr_res->hw_res[
				CAM_ISP_HW_SPLIT_RIGHT]->process_cmd(
				hw_mgr_res->hw_res[CAM_ISP_HW_SPLIT_RIGHT],
				CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA, &soc_info,
				sizeof(void *));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Failed in regspace data query split idx: %d rc : %d",
					CAM_ISP_HW_SPLIT_RIGHT, rc);
				return rc;
			}

			*reg_base_idx = 0;
			*soc_info_ptr = soc_info;
			break;
		default:
			CAM_ERR(CAM_ISP,
				"Unrecognized reg base type: %u",
				reg_base_type);
			return -EINVAL;
		}

		break;
	}

	return rc;
}

static int cam_ife_mgr_handle_reg_dump(struct cam_ife_hw_mgr_ctx *ctx,
	struct cam_cmd_buf_desc *reg_dump_buf_desc, uint32_t num_reg_dump_buf,
	uint32_t meta_type)
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
				cam_ife_mgr_regspace_data_cb);
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

static int cam_ife_notify_safe_lut_scm(bool safe_trigger)
{
	uint32_t camera_hw_version, rc = 0;
	struct scm_desc desc = {0};

	rc = cam_cpas_get_cpas_hw_version(&camera_hw_version);
	if (!rc) {
		switch (camera_hw_version) {
		case CAM_CPAS_TITAN_170_V100:
		case CAM_CPAS_TITAN_170_V110:
		case CAM_CPAS_TITAN_175_V100:

			desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
			desc.args[0] = SMMU_SE_IFE;
			desc.args[1] = safe_trigger;

			CAM_DBG(CAM_ISP, "Safe scm call %d", safe_trigger);
			if (scm_call2(SCM_SIP_FNID(TZ_SVC_SMMU_PROGRAM,
					TZ_SAFE_SYSCALL_ID), &desc)) {
				CAM_ERR(CAM_ISP,
					"scm call to Enable Safe failed");
				rc = -EINVAL;
			}
			break;
		default:
			break;
		}
	}

	return rc;
}

static int cam_ife_mgr_get_hw_caps(void *hw_mgr_priv,
	void *hw_caps_args)
{
	int rc = 0;
	int i;
	struct cam_ife_hw_mgr             *hw_mgr = hw_mgr_priv;
	struct cam_query_cap_cmd          *query = hw_caps_args;
	struct cam_isp_query_cap_cmd       query_isp;

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
	query_isp.num_dev = 2;
	for (i = 0; i < query_isp.num_dev; i++) {
		query_isp.dev_caps[i].hw_type = CAM_ISP_HW_IFE;
		query_isp.dev_caps[i].hw_version.major = 1;
		query_isp.dev_caps[i].hw_version.minor = 7;
		query_isp.dev_caps[i].hw_version.incr = 0;
		query_isp.dev_caps[i].hw_version.reserved = 0;
	}

	if (copy_to_user(u64_to_user_ptr(query->caps_handle),
		&query_isp, sizeof(struct cam_isp_query_cap_cmd)))
		rc = -EFAULT;

	CAM_DBG(CAM_ISP, "exit rc :%d", rc);

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

static int cam_ife_hw_mgr_reset_csid_res(
	struct cam_ife_hw_mgr_res   *isp_hw_res)
{
	int i;
	int rc = 0;
	struct cam_hw_intf      *hw_intf;
	struct cam_csid_reset_cfg_args  csid_reset_args;

	csid_reset_args.reset_type = CAM_IFE_CSID_RESET_PATH;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		csid_reset_args.node_res = isp_hw_res->hw_res[i];
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		CAM_DBG(CAM_ISP, "Resetting csid hardware %d",
			hw_intf->hw_idx);
		if (hw_intf->hw_ops.reset) {
			rc = hw_intf->hw_ops.reset(hw_intf->hw_priv,
				&csid_reset_args,
				sizeof(struct cam_csid_reset_cfg_args));
			if (rc <= 0)
				goto err;
		}
	}

	return 0;
err:
	CAM_ERR(CAM_ISP, "RESET HW res failed: (type:%d, id:%d)",
		isp_hw_res->res_type, isp_hw_res->res_id);
	return rc;
}

static int cam_ife_hw_mgr_init_hw_res(
	struct cam_ife_hw_mgr_res   *isp_hw_res)
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

static int cam_ife_hw_mgr_start_hw_res(
	struct cam_ife_hw_mgr_res   *isp_hw_res,
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
				CAM_ERR(CAM_ISP, "Can not start HW resources");
				goto err;
			}
			CAM_DBG(CAM_ISP, "Start HW %d Res %d", hw_intf->hw_idx,
				isp_hw_res->hw_res[i]->res_id);
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
	struct cam_ife_hw_mgr_res   *isp_hw_res)
{
	int i;
	struct cam_hw_intf      *hw_intf;
	uint32_t dummy_args;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;

		if (isp_hw_res->hw_res[i]->res_state !=
			CAM_ISP_RESOURCE_STATE_STREAMING)
			continue;

		if (hw_intf->hw_ops.stop)
			hw_intf->hw_ops.stop(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
		else
			CAM_ERR(CAM_ISP, "stop null");
		if (hw_intf->hw_ops.process_cmd &&
			isp_hw_res->res_type == CAM_IFE_HW_MGR_RES_IFE_OUT) {
			hw_intf->hw_ops.process_cmd(hw_intf->hw_priv,
				CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ,
				&dummy_args, sizeof(dummy_args));
		}
	}
}

static void cam_ife_hw_mgr_deinit_hw_res(
	struct cam_ife_hw_mgr_res   *isp_hw_res)
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
	struct cam_ife_hw_mgr_res *hw_mgr_res;
	int i = 0;

	if (!ctx->init_done) {
		CAM_WARN(CAM_ISP, "ctx is not in init state");
		return;
	}

	/* Deinit IFE CID */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_cid, list) {
		CAM_DBG(CAM_ISP, "%s: Going to DeInit IFE CID\n", __func__);
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deinit IFE CSID */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		CAM_DBG(CAM_ISP, "%s: Going to DeInit IFE CSID\n", __func__);
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deint IFE MUX(SRC) */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deint IFE RD */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deinit IFE OUT */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++)
		cam_ife_hw_mgr_deinit_hw_res(&ctx->res_list_ife_out[i]);

	ctx->init_done = false;
}

static int cam_ife_hw_mgr_init_hw(
	struct cam_ife_hw_mgr_ctx *ctx)
{
	struct cam_ife_hw_mgr_res *hw_mgr_res;
	int rc = 0, i;

	CAM_DBG(CAM_ISP, "INIT IFE CID ... in ctx id:%d",
		ctx->ctx_index);
	/* INIT IFE CID */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_cid, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT IFE CID(id :%d)",
				 hw_mgr_res->res_id);
			goto deinit;
		}
	}

	CAM_DBG(CAM_ISP, "INIT IFE csid ... in ctx id:%d",
		ctx->ctx_index);

	/* INIT IFE csid */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT IFE CSID(id :%d)",
				 hw_mgr_res->res_id);
			goto deinit;
		}
	}

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

	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		rc = cam_ife_hw_mgr_init_hw_res(&ctx->res_list_ife_out[i]);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT IFE OUT (%d)",
				 ctx->res_list_ife_out[i].res_id);
			goto deinit;
		}
	}

	return rc;
deinit:
	ctx->init_done = true;
	cam_ife_hw_mgr_deinit_hw(ctx);
	return rc;
}

static int cam_ife_hw_mgr_put_res(
	struct list_head                *src_list,
	struct cam_ife_hw_mgr_res      **res)
{
	int rc                              = 0;
	struct cam_ife_hw_mgr_res *res_ptr  = NULL;

	res_ptr = *res;
	if (res_ptr)
		list_add_tail(&res_ptr->list, src_list);

	return rc;
}

static int cam_ife_hw_mgr_get_res(
	struct list_head                *src_list,
	struct cam_ife_hw_mgr_res      **res)
{
	int rc = 0;
	struct cam_ife_hw_mgr_res *res_ptr  = NULL;

	if (!list_empty(src_list)) {
		res_ptr = list_first_entry(src_list,
			struct cam_ife_hw_mgr_res, list);
		list_del_init(&res_ptr->list);
	} else {
		CAM_ERR(CAM_ISP, "No more free ife hw mgr ctx");
		rc = -1;
	}
	*res = res_ptr;

	return rc;
}

static int cam_ife_hw_mgr_free_hw_res(
	struct cam_ife_hw_mgr_res   *isp_hw_res)
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
					"Release hw resource id %d failed",
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

static const char *cam_ife_hw_mgr_get_csid_res_id(
	uint32_t res_id)
{
	switch (res_id) {
	case CAM_IFE_PIX_PATH_RES_RDI_0:
		return "RDI_0";
	case CAM_IFE_PIX_PATH_RES_RDI_1:
		return "RDI_1";
	case CAM_IFE_PIX_PATH_RES_RDI_2:
		return "RDI_2";
	case CAM_IFE_PIX_PATH_RES_RDI_3:
		return "RDI_3";
	case CAM_IFE_PIX_PATH_RES_IPP:
		return "IPP";
	case CAM_IFE_PIX_PATH_RES_PPP:
		return "PPP";
	default:
		return "INVALID";
	}
}

static const char *cam_ife_hw_mgr_get_src_res_id(
	uint32_t res_id)
{
	switch (res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
		return "CAMIF";
	case CAM_ISP_HW_VFE_IN_TESTGEN:
		return "TESTGEN";
	case CAM_ISP_HW_VFE_IN_RD:
		return "BUS_RD";
	case CAM_ISP_HW_VFE_IN_RDI0:
		return "RDI_0";
	case CAM_ISP_HW_VFE_IN_RDI1:
		return "RDI_1";
	case CAM_ISP_HW_VFE_IN_RDI2:
		return "RDI_2";
	case CAM_ISP_HW_VFE_IN_RDI3:
		return "RDI_3";
	case CAM_ISP_HW_VFE_IN_PDLIB:
		return "PDLIB";
	case CAM_ISP_HW_VFE_IN_LCR:
		return "LCR";
	default:
		return "INVALID";
	}
}

static void cam_ife_hw_mgr_dump_src_acq_info(
	struct cam_ife_hw_mgr_ctx    *hwr_mgr_ctx,
	uint32_t num_pix_port, uint32_t num_rdi_port)
{
	struct cam_ife_hw_mgr_res    *hw_mgr_res = NULL;
	struct cam_ife_hw_mgr_res    *hw_mgr_res_temp = NULL;
	struct cam_isp_resource_node *hw_res = NULL;
	int i = 0;

	CAM_INFO(CAM_ISP,
		"Acquired HW for ctx: %u with pix_port: %u rdi_port: %u",
		hwr_mgr_ctx->ctx_index, num_pix_port, num_rdi_port);
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&hwr_mgr_ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"IFE src split_id: %d res_id: %s hw_idx: %u state: %s",
					i,
					cam_ife_hw_mgr_get_src_res_id(
					hw_res->res_id),
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}
}

static void cam_ife_hw_mgr_dump_acq_data(
	struct cam_ife_hw_mgr_ctx    *hwr_mgr_ctx)
{
	struct cam_ife_hw_mgr_res    *hw_mgr_res = NULL;
	struct cam_ife_hw_mgr_res    *hw_mgr_res_temp = NULL;
	struct cam_isp_resource_node *hw_res = NULL;
	struct timespec64            *ts = NULL;
	uint64_t ms, tmp, hrs, min, sec;
	int i = 0, j = 0;

	ts = &hwr_mgr_ctx->ts;
	tmp = ts->tv_sec;
	ms = (ts->tv_nsec) / 1000000;
	sec = do_div(tmp, 60);
	min = do_div(tmp, 60);
	hrs = do_div(tmp, 24);

	CAM_INFO(CAM_ISP,
		"**** %llu:%llu:%llu.%llu ctx_idx: %u rdi_only: %s is_dual: %s acquired ****",
		hrs, min, sec, ms,
		hwr_mgr_ctx->ctx_index,
		(hwr_mgr_ctx->is_rdi_only_context ? "true" : "false"),
		(hwr_mgr_ctx->is_dual ? "true" : "false"));

	/* Iterate over CID resources */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&hwr_mgr_ctx->res_list_ife_cid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf) {
				CAM_INFO(CAM_ISP,
					"CID split_id: %d res_id: %u hw_idx: %u state: %s",
					i, hw_res->res_id,
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
			}
		}
	}

	/* Iterate over CSID resources */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&hwr_mgr_ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			hw_res = hw_mgr_res->hw_res[i];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"CSID split_id: %d res_id: %s hw_idx: %u state: %s",
					i,
					cam_ife_hw_mgr_get_csid_res_id(
					hw_res->res_id),
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
					"IFE src split_id: %d res_id: %s hw_idx: %u state: %s",
					i,
					cam_ife_hw_mgr_get_src_res_id(
					hw_res->res_id),
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
					"IFE src_rd split_id: %d res_id: %s hw_idx: %u state: %s",
					i,
					cam_ife_hw_mgr_get_src_res_id(
					hw_res->res_id),
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}

	/* Iterate over IFE OUT resources */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			hw_mgr_res = &hwr_mgr_ctx->res_list_ife_out[i];
			hw_res = hw_mgr_res->hw_res[j];
			if (hw_res && hw_res->hw_intf)
				CAM_INFO(CAM_ISP,
					"IFE out split_id: %d res_id: 0x%x hw_idx: %u state: %s",
					j, hw_res->res_id,
					hw_res->hw_intf->hw_idx,
					cam_ife_hw_mgr_get_res_state
					(hw_res->res_state));
		}
	}
}

static int cam_ife_mgr_csid_stop_hw(
	struct cam_ife_hw_mgr_ctx *ctx, struct list_head  *stop_list,
		uint32_t  base_idx, uint32_t stop_cmd)
{
	struct cam_ife_hw_mgr_res      *hw_mgr_res;
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
			if (isp_res->hw_intf->hw_idx != base_idx)
				continue;
			CAM_DBG(CAM_ISP, "base_idx %d res_id %d cnt %u",
				base_idx, isp_res->res_id, cnt);
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
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	struct cam_ife_hw_mgr_res        *hw_mgr_res_temp;

	/* ife leaf resource */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++)
		cam_ife_hw_mgr_free_hw_res(&ife_ctx->res_list_ife_out[i]);

	/* ife bus rd resource */
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

	/* ife csid resource */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&ife_ctx->res_list_ife_csid, list) {
		cam_ife_hw_mgr_free_hw_res(hw_mgr_res);
		cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &hw_mgr_res);
	}

	/* ife cid resource */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&ife_ctx->res_list_ife_cid, list) {
		cam_ife_hw_mgr_free_hw_res(hw_mgr_res);
		cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, &hw_mgr_res);
	}

	/* ife root node */
	if (ife_ctx->res_list_ife_in.res_type != CAM_IFE_HW_MGR_RES_UNINIT)
		cam_ife_hw_mgr_free_hw_res(&ife_ctx->res_list_ife_in);

	/* clean up the callback function */
	ife_ctx->common.cb_priv = NULL;
	memset(ife_ctx->common.event_cb, 0, sizeof(ife_ctx->common.event_cb));

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
	uint32_t                         base_idx)
{
	uint32_t    i;

	if (!ctx->num_base) {
		ctx->base[0].split_id = split_id;
		ctx->base[0].idx      = base_idx;
		ctx->num_base++;
		CAM_DBG(CAM_ISP,
			"Add split id = %d for base idx = %d num_base=%d",
			split_id, base_idx, ctx->num_base);
	} else {
		/*Check if base index already exists in the list */
		for (i = 0; i < ctx->num_base; i++) {
			if (ctx->base[i].idx == base_idx) {
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
			ctx->num_base++;
			CAM_DBG(CAM_ISP,
				"Add split_id=%d for base idx=%d num_base=%d",
				 split_id, base_idx, ctx->num_base);
		}
	}
}

static int cam_ife_mgr_process_base_info(
	struct cam_ife_hw_mgr_ctx        *ctx)
{
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	struct cam_isp_resource_node     *res = NULL;
	uint32_t i;

	if (list_empty(&ctx->res_list_ife_src)) {
		CAM_ERR(CAM_ISP, "Mux List empty");
		return -ENODEV;
	}

	/* IFE mux in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		if (hw_mgr_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			res = hw_mgr_res->hw_res[i];
			cam_ife_mgr_add_base_info(ctx, i,
					res->hw_intf->hw_idx);
			CAM_DBG(CAM_ISP, "add base info for hw %d",
				res->hw_intf->hw_idx);
		}
	}
	CAM_DBG(CAM_ISP, "ctx base num = %d", ctx->num_base);

	return 0;
}

static int cam_ife_hw_mgr_acquire_res_bus_rd(
	struct cam_ife_hw_mgr_ctx       *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -EINVAL;
	struct cam_vfe_acquire_args               vfe_acquire;
	struct cam_ife_hw_mgr_res                *ife_in_rd_res;
	struct cam_hw_intf                       *hw_intf;
	struct cam_ife_hw_mgr_res                *ife_src_res;
	int i;

	CAM_DBG(CAM_ISP, "Enter");

	list_for_each_entry(ife_src_res, &ife_ctx->res_list_ife_src, list) {
		if (ife_src_res->res_id != CAM_ISP_HW_VFE_IN_RD)
			continue;

		rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&ife_in_rd_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto err;
		}
		cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_in_rd,
			&ife_in_rd_res);

		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_BUS_RD;
		vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		vfe_acquire.vfe_out.cdm_ops = ife_ctx->cdm_ops;
		vfe_acquire.priv = ife_ctx;
		vfe_acquire.vfe_out.unique_id = ife_ctx->ctx_index;
		vfe_acquire.vfe_out.is_dual = ife_src_res->is_dual_vfe;
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!ife_src_res->hw_res[i])
				continue;

			hw_intf = ife_src_res->hw_res[i]->hw_intf;
			if (i == CAM_ISP_HW_SPLIT_LEFT) {
				vfe_acquire.vfe_out.split_id  =
					CAM_ISP_HW_SPLIT_LEFT;
				if (ife_src_res->is_dual_vfe) {
					/*TBD */
					vfe_acquire.vfe_out.is_master     = 1;
					vfe_acquire.vfe_out.dual_slave_core =
						(hw_intf->hw_idx == 0) ? 1 : 0;
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
					(hw_intf->hw_idx == 0) ? 1 : 0;
			}
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&vfe_acquire,
				sizeof(struct cam_vfe_acquire_args));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Can not acquire out resource 0x%x",
					vfe_acquire.rsrc_type);
				goto err;
			}

			ife_in_rd_res->hw_res[i] =
				vfe_acquire.vfe_out.rsrc_node;
			CAM_DBG(CAM_ISP, "resource type :0x%x res id:0x%x",
				ife_in_rd_res->hw_res[i]->res_type,
				ife_in_rd_res->hw_res[i]->res_id);

		}
		ife_in_rd_res->is_dual_vfe = in_port->usage_type;
		ife_in_rd_res->res_type = (enum cam_ife_hw_mgr_res_type)
			CAM_ISP_RESOURCE_VFE_BUS_RD;
	}

	return 0;
err:
	CAM_DBG(CAM_ISP, "Exit rc(0x%x)", rc);
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_out_rdi(
	struct cam_ife_hw_mgr_ctx       *ife_ctx,
	struct cam_ife_hw_mgr_res       *ife_src_res,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -EINVAL;
	struct cam_vfe_acquire_args               vfe_acquire;
	struct cam_isp_out_port_generic_info     *out_port = NULL;
	struct cam_ife_hw_mgr_res                *ife_out_res;
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
		vfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;
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
	ife_out_res->is_dual_vfe = 0;
	ife_out_res->res_id = vfe_out_res_id;
	ife_out_res->res_type = (enum cam_ife_hw_mgr_res_type)
		CAM_ISP_RESOURCE_VFE_OUT;
	ife_src_res->child[ife_src_res->num_children++] = ife_out_res;
	CAM_DBG(CAM_ISP, "IFE SRC num_children = %d",
		ife_src_res->num_children);

	return 0;
err:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_out_pixel(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_ife_hw_mgr_res *ife_src_res,
	struct cam_isp_in_port_generic_info *in_port,
	bool                             acquire_lcr)
{
	int rc = -1;
	uint32_t  i, j, k;
	struct cam_vfe_acquire_args               vfe_acquire;
	struct cam_isp_out_port_generic_info     *out_port;
	struct cam_ife_hw_mgr_res                *ife_out_res;
	struct cam_hw_intf                       *hw_intf;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		k = out_port->res_type & 0xFF;
		if (k >= CAM_IFE_HW_OUT_RES_MAX) {
			CAM_ERR(CAM_ISP, "invalid output resource type 0x%x",
				 out_port->res_type);
			continue;
		}

		if (cam_ife_hw_mgr_is_rdi_res(out_port->res_type))
			continue;

		if ((acquire_lcr &&
			out_port->res_type != CAM_ISP_IFE_OUT_RES_LCR) ||
			(!acquire_lcr &&
			out_port->res_type == CAM_ISP_IFE_OUT_RES_LCR))
			continue;

		if ((out_port->res_type == CAM_ISP_IFE_OUT_RES_2PD &&
			ife_src_res->res_id != CAM_ISP_HW_VFE_IN_PDLIB) ||
			(ife_src_res->res_id == CAM_ISP_HW_VFE_IN_PDLIB &&
			out_port->res_type != CAM_ISP_IFE_OUT_RES_2PD))
			continue;

		CAM_DBG(CAM_ISP, "res_type 0x%x", out_port->res_type);

		ife_out_res = &ife_ctx->res_list_ife_out[k];
		ife_out_res->is_dual_vfe = in_port->usage_type;

		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_OUT;
		vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		vfe_acquire.vfe_out.cdm_ops = ife_ctx->cdm_ops;
		vfe_acquire.priv = ife_ctx;
		vfe_acquire.vfe_out.out_port_info =  out_port;
		vfe_acquire.vfe_out.is_dual       = ife_src_res->is_dual_vfe;
		vfe_acquire.vfe_out.unique_id     = ife_ctx->ctx_index;
		vfe_acquire.event_cb = cam_ife_hw_mgr_event_handler;

		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!ife_src_res->hw_res[j])
				continue;

			hw_intf = ife_src_res->hw_res[j]->hw_intf;

			if (j == CAM_ISP_HW_SPLIT_LEFT) {
				vfe_acquire.vfe_out.split_id  =
					CAM_ISP_HW_SPLIT_LEFT;
				if (ife_src_res->is_dual_vfe) {
					/*TBD */
					vfe_acquire.vfe_out.is_master     = 1;
					vfe_acquire.vfe_out.dual_slave_core =
						(hw_intf->hw_idx == 0) ? 1 : 0;
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
					(hw_intf->hw_idx == 0) ? 1 : 0;
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
		ife_out_res->res_type =
			(enum cam_ife_hw_mgr_res_type)CAM_ISP_RESOURCE_VFE_OUT;
		ife_out_res->res_id = out_port->res_type;
		ife_out_res->parent = ife_src_res;
		ife_src_res->child[ife_src_res->num_children++] = ife_out_res;
		CAM_DBG(CAM_ISP, "IFE SRC num_children = %d",
			ife_src_res->num_children);
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_out(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -EINVAL;
	struct cam_ife_hw_mgr_res       *ife_src_res;

	list_for_each_entry(ife_src_res, &ife_ctx->res_list_ife_src, list) {
		if (ife_src_res->num_children)
			continue;

		switch (ife_src_res->res_id) {
		case CAM_ISP_HW_VFE_IN_CAMIF:
		case CAM_ISP_HW_VFE_IN_PDLIB:
		case CAM_ISP_HW_VFE_IN_RD:
			rc = cam_ife_hw_mgr_acquire_res_ife_out_pixel(ife_ctx,
				ife_src_res, in_port, false);
			break;
		case CAM_ISP_HW_VFE_IN_LCR:
			rc = cam_ife_hw_mgr_acquire_res_ife_out_pixel(ife_ctx,
				ife_src_res, in_port, true);
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

static int cam_ife_hw_mgr_acquire_res_ife_rd_src(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc                = -1;
	struct cam_ife_hw_mgr_res                  *csid_res;
	struct cam_ife_hw_mgr_res                  *ife_src_res;
	struct cam_vfe_acquire_args                 vfe_acquire;
	struct cam_hw_intf                         *hw_intf;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;
	int vfe_idx = -1, i = 0;

	ife_hw_mgr = ife_ctx->hw_mgr;

	CAM_DBG(CAM_ISP, "Enter");
	list_for_each_entry(csid_res, &ife_ctx->res_list_ife_csid, list) {
		if (csid_res->res_id != CAM_IFE_PIX_PATH_RES_RDI_0) {
			CAM_DBG(CAM_ISP, "not RDI0: %d", csid_res->res_id);
			continue;
		}

		rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&ife_src_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto err;
		}
		cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_src,
			&ife_src_res);

		CAM_DBG(CAM_ISP, "csid_res_id %d", csid_res->res_id);
		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_IN;
		vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		vfe_acquire.vfe_in.cdm_ops = ife_ctx->cdm_ops;
		vfe_acquire.vfe_in.in_port = in_port;
		vfe_acquire.vfe_in.res_id = CAM_ISP_HW_VFE_IN_RD;
		vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;

		ife_src_res->res_type =
			(enum cam_ife_hw_mgr_res_type)vfe_acquire.rsrc_type;
		ife_src_res->res_id = vfe_acquire.vfe_in.res_id;
		ife_src_res->is_dual_vfe = csid_res->is_dual_vfe;

		hw_intf =
			ife_hw_mgr->ife_devices[csid_res->hw_res[
			CAM_ISP_HW_SPLIT_LEFT]->hw_intf->hw_idx];

		vfe_idx = csid_res->hw_res[
			CAM_ISP_HW_SPLIT_LEFT]->hw_intf->hw_idx;

		/*
		 * fill in more acquire information as needed
		 */
		if (ife_src_res->is_dual_vfe)
			vfe_acquire.vfe_in.sync_mode = CAM_ISP_HW_SYNC_MASTER;

		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&vfe_acquire,
				sizeof(struct cam_vfe_acquire_args));
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Can not acquire IFE HW res %d",
				csid_res->res_id);
			goto err;
		}
		ife_src_res->hw_res[CAM_ISP_HW_SPLIT_LEFT] =
			vfe_acquire.vfe_in.rsrc_node;
		CAM_DBG(CAM_ISP,
			"acquire success IFE:%d  res type :0x%x res id:0x%x",
			hw_intf->hw_idx,
			ife_src_res->hw_res[CAM_ISP_HW_SPLIT_LEFT]->res_type,
			ife_src_res->hw_res[CAM_ISP_HW_SPLIT_LEFT]->res_id);

		if (!ife_src_res->is_dual_vfe)
			goto acq;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (i == CAM_ISP_HW_SPLIT_LEFT) {
				CAM_DBG(CAM_ISP, "vfe_idx %d is acquired",
					vfe_idx);
				continue;
			}

			hw_intf = ife_hw_mgr->ife_devices[i];

			/* fill in more acquire information as needed */
			if (i == CAM_ISP_HW_SPLIT_RIGHT)
				vfe_acquire.vfe_in.sync_mode =
					CAM_ISP_HW_SYNC_SLAVE;

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
			CAM_DBG(CAM_ISP,
				"acquire success IFE:%d  res type :0x%x res id:0x%x",
				hw_intf->hw_idx,
				ife_src_res->hw_res[i]->res_type,
				ife_src_res->hw_res[i]->res_id);
		}
acq:
		/*
		 * It should be one to one mapping between
		 * csid resource and ife source resource
		 */
		csid_res->child[0] = ife_src_res;
		ife_src_res->parent = csid_res;
		csid_res->child[csid_res->num_children++] = ife_src_res;
		CAM_DBG(CAM_ISP,
			"csid_res=%d  CSID num_children=%d ife_src_res=%d",
			csid_res->res_id, csid_res->num_children,
			ife_src_res->res_id);
	}

err:
	/* release resource at the entry function */
	CAM_DBG(CAM_ISP, "Exit rc %d", rc);
	return rc;
}

static int cam_convert_hw_idx_to_ife_hw_num(int hw_idx)
{
	uint32_t hw_version, rc = 0;

	rc = cam_cpas_get_cpas_hw_version(&hw_version);
	if (!rc) {
		switch (hw_version) {
		case CAM_CPAS_TITAN_170_V100:
		case CAM_CPAS_TITAN_170_V110:
		case CAM_CPAS_TITAN_170_V120:
		case CAM_CPAS_TITAN_175_V100:
		case CAM_CPAS_TITAN_175_V101:
		case CAM_CPAS_TITAN_175_V120:
		case CAM_CPAS_TITAN_175_V130:
		case CAM_CPAS_TITAN_480_V100:
			if (hw_idx == 0)
				return CAM_ISP_IFE0_HW;
			else if (hw_idx == 1)
				return CAM_ISP_IFE1_HW;
			else if (hw_idx == 2)
				return CAM_ISP_IFE0_LITE_HW;
			else if (hw_idx == 3)
				return CAM_ISP_IFE1_LITE_HW;
			else if (hw_idx == 4)
				return CAM_ISP_IFE2_LITE_HW;
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid hw_version: 0x%X",
				hw_version);
			rc = -EINVAL;
			break;
		}
	}

	return rc;
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

static int cam_ife_hw_mgr_acquire_res_ife_src(
	struct cam_ife_hw_mgr_ctx *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	bool acquire_lcr, uint32_t *acquired_hw_id,
	uint32_t *acquired_hw_path)
{
	int rc                = -1;
	int i;
	struct cam_ife_hw_mgr_res                  *csid_res;
	struct cam_ife_hw_mgr_res                  *ife_src_res;
	struct cam_vfe_acquire_args                 vfe_acquire;
	struct cam_hw_intf                         *hw_intf;
	struct cam_ife_hw_mgr                      *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;

	list_for_each_entry(csid_res, &ife_ctx->res_list_ife_csid, list) {
		if (csid_res->num_children && !acquire_lcr)
			continue;

		if (acquire_lcr && csid_res->res_id != CAM_IFE_PIX_PATH_RES_IPP)
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
			if (csid_res->is_dual_vfe)
				vfe_acquire.vfe_in.sync_mode =
				CAM_ISP_HW_SYNC_MASTER;
			else
				vfe_acquire.vfe_in.sync_mode =
				CAM_ISP_HW_SYNC_NONE;

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
			CAM_ERR(CAM_ISP, "Wrong IFE CSID Resource Node");
			goto err;
		}
		ife_src_res->res_type =
			(enum cam_ife_hw_mgr_res_type)vfe_acquire.rsrc_type;
		ife_src_res->res_id = vfe_acquire.vfe_in.res_id;
		ife_src_res->is_dual_vfe = csid_res->is_dual_vfe;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!csid_res->hw_res[i])
				continue;

			hw_intf = ife_hw_mgr->ife_devices[
				csid_res->hw_res[i]->hw_intf->hw_idx];

			/* fill in more acquire information as needed */
			/* slave Camif resource, */
			if (i == CAM_ISP_HW_SPLIT_RIGHT &&
				ife_src_res->is_dual_vfe)
				vfe_acquire.vfe_in.sync_mode =
				CAM_ISP_HW_SYNC_SLAVE;

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
				"acquire success IFE:%d  res type :0x%x res id:0x%x",
				hw_intf->hw_idx,
				ife_src_res->hw_res[i]->res_type,
				ife_src_res->hw_res[i]->res_id);

		}

		ife_src_res->parent = csid_res;
		csid_res->child[csid_res->num_children++] = ife_src_res;
		CAM_DBG(CAM_ISP,
			"csid_res=%d  CSID num_children=%d ife_src_res=%d",
			csid_res->res_id, csid_res->num_children,
			ife_src_res->res_id);
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_ife_mgr_acquire_cid_res(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	struct cam_ife_hw_mgr_res          **cid_res,
	enum cam_ife_pix_path_res_id         path_res_id)
{
	int rc = -1;
	int i, j;
	struct cam_ife_hw_mgr                *ife_hw_mgr;
	struct cam_hw_intf                   *hw_intf;
	struct cam_ife_hw_mgr_res            *cid_res_temp, *cid_res_iterator;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;
	uint32_t acquired_cnt = 0;
	struct cam_isp_out_port_generic_info *out_port = NULL;

	ife_hw_mgr = ife_ctx->hw_mgr;
	*cid_res = NULL;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, cid_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto end;
	}

	cid_res_temp = *cid_res;

	csid_acquire.res_type = CAM_ISP_RESOURCE_CID;
	csid_acquire.in_port = in_port;
	csid_acquire.res_id =  path_res_id;
	CAM_DBG(CAM_ISP, "path_res_id %d", path_res_id);

	if (in_port->num_out_res)
		out_port = &(in_port->data[0]);

	/* Try acquiring CID resource from previously acquired HW */
	list_for_each_entry(cid_res_iterator, &ife_ctx->res_list_ife_cid,
		list) {

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!cid_res_iterator->hw_res[i])
				continue;

			if (in_port->num_out_res &&
				((cid_res_iterator->is_secure == 1 &&
				out_port->secure_mode == 0) ||
				(cid_res_iterator->is_secure == 0 &&
				out_port->secure_mode == 1)))
				continue;

			if (!in_port->num_out_res &&
				cid_res_iterator->is_secure == 1)
				continue;

			hw_intf = cid_res_iterator->hw_res[i]->hw_intf;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc) {
				CAM_DBG(CAM_ISP,
					"No ife cid resource from hw %d",
					hw_intf->hw_idx);
				continue;
			}

			cid_res_temp->hw_res[acquired_cnt++] =
				csid_acquire.node_res;

			CAM_DBG(CAM_ISP,
				"acquired from old csid(%s)=%d CID rsrc successfully",
				(i == 0) ? "left" : "right",
				hw_intf->hw_idx);

			if (in_port->usage_type && acquired_cnt == 1 &&
				path_res_id == CAM_IFE_PIX_PATH_RES_IPP)
				/*
				 * Continue to acquire Right for IPP.
				 * Dual IFE for RDI and PPP is not currently
				 * supported.
				 */

				continue;

			if (acquired_cnt)
				/*
				 * If successfully acquired CID from
				 * previously acquired HW, skip the next
				 * part
				 */
				goto acquire_successful;
		}
	}

	/* Acquire Left if not already acquired */
	if (ife_ctx->is_fe_enable) {
		for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
			if (!ife_hw_mgr->csid_devices[i])
				continue;

			hw_intf = ife_hw_mgr->csid_devices[i];
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc)
				continue;
			else {
				cid_res_temp->hw_res[acquired_cnt++] =
					csid_acquire.node_res;
				break;
			}
		}
		if (i == CAM_IFE_CSID_HW_NUM_MAX || !csid_acquire.node_res) {
			CAM_ERR(CAM_ISP,
				"Can not acquire ife cid resource for path %d",
				path_res_id);
			goto put_res;
		}
	} else {
		for (i = CAM_IFE_CSID_HW_NUM_MAX - 1; i >= 0; i--) {
			if (!ife_hw_mgr->csid_devices[i])
				continue;

			hw_intf = ife_hw_mgr->csid_devices[i];
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc)
				continue;
			else {
				cid_res_temp->hw_res[acquired_cnt++] =
					csid_acquire.node_res;
				break;
			}
		}
		if (i == -1 || !csid_acquire.node_res) {
			CAM_ERR(CAM_ISP,
				"Can not acquire ife cid resource for path %d",
				path_res_id);
			goto put_res;
		}
	}


acquire_successful:
	CAM_DBG(CAM_ISP, "CID left acquired success is_dual %d",
		in_port->usage_type);

	cid_res_temp->res_type = CAM_IFE_HW_MGR_RES_CID;
	/* CID(DT_ID) value of acquire device, require for path */
	cid_res_temp->res_id = csid_acquire.node_res->res_id;
	cid_res_temp->is_dual_vfe = in_port->usage_type;
	ife_ctx->is_dual = (bool)in_port->usage_type;

	if (in_port->num_out_res)
		cid_res_temp->is_secure = out_port->secure_mode;

	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_cid, cid_res);

	/*
	 * Acquire Right if not already acquired.
	 * Dual IFE for RDI and PPP is not currently supported.
	 */
	if (cid_res_temp->is_dual_vfe && path_res_id
		== CAM_IFE_PIX_PATH_RES_IPP && acquired_cnt == 1) {
		csid_acquire.node_res = NULL;
		csid_acquire.res_type = CAM_ISP_RESOURCE_CID;
		csid_acquire.in_port = in_port;
		for (j = 0; j < CAM_IFE_CSID_HW_NUM_MAX; j++) {
			if (!ife_hw_mgr->csid_devices[j])
				continue;

			if (j == cid_res_temp->hw_res[0]->hw_intf->hw_idx)
				continue;

			hw_intf = ife_hw_mgr->csid_devices[j];
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc)
				continue;
			else
				break;
		}

		if (j == CAM_IFE_CSID_HW_NUM_MAX) {
			CAM_ERR(CAM_ISP,
				"Can not acquire ife csid rdi resource");
			goto end;
		}
		cid_res_temp->hw_res[1] = csid_acquire.node_res;
		CAM_DBG(CAM_ISP, "CID right acquired success is_dual %d",
			in_port->usage_type);
	}
	cid_res_temp->parent = &ife_ctx->res_list_ife_in;
	ife_ctx->res_list_ife_in.child[
		ife_ctx->res_list_ife_in.num_children++] = cid_res_temp;
	CAM_DBG(CAM_ISP, "IFE IN num_children = %d",
		ife_ctx->res_list_ife_in.num_children);

	return 0;
put_res:
	cam_ife_hw_mgr_put_res(&ife_ctx->free_res_list, cid_res);
end:
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
	int master_idx = -1;

	struct cam_ife_hw_mgr                    *ife_hw_mgr;
	struct cam_ife_hw_mgr_res                *csid_res;
	struct cam_ife_hw_mgr_res                *cid_res;
	struct cam_hw_intf                       *hw_intf;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;
	enum cam_ife_pix_path_res_id              path_res_id;

	ife_hw_mgr = ife_ctx->hw_mgr;
	/* get cid resource */
	if (is_ipp)
		path_res_id = CAM_IFE_PIX_PATH_RES_IPP;
	else
		path_res_id = CAM_IFE_PIX_PATH_RES_PPP;

	rc = cam_ife_mgr_acquire_cid_res(ife_ctx, in_port, &cid_res,
		path_res_id);

	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE CID resource Failed");
		goto end;
	}

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &csid_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto end;
	}

	csid_res->res_type =
		(enum cam_ife_hw_mgr_res_type)CAM_ISP_RESOURCE_PIX_PATH;

	csid_res->res_id = path_res_id;

	if (in_port->usage_type && is_ipp)
		csid_res->is_dual_vfe = 1;
	else {
		csid_res->is_dual_vfe = 0;
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;
	}

	CAM_DBG(CAM_ISP, "CSID Acq: E");
	/* IPP resource needs to be from same HW as CID resource */
	for (i = 0; i <= csid_res->is_dual_vfe; i++) {
		CAM_DBG(CAM_ISP, "i %d is_dual %d", i, csid_res->is_dual_vfe);

		csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_acquire.res_id = path_res_id;
		csid_acquire.cid = cid_res->hw_res[i]->res_id;
		csid_acquire.in_port = in_port;
		csid_acquire.out_port = in_port->data;
		csid_acquire.node_res = NULL;
		csid_acquire.crop_enable = crop_enable;
		csid_acquire.drop_enable = false;

		hw_intf = cid_res->hw_res[i]->hw_intf;

		if (csid_res->is_dual_vfe) {
			if (i == CAM_ISP_HW_SPLIT_LEFT) {
				master_idx = hw_intf->hw_idx;
				csid_acquire.sync_mode =
					CAM_ISP_HW_SYNC_MASTER;
			} else {
				if (master_idx == -1) {
					CAM_ERR(CAM_ISP,
						"No Master found");
					goto put_res;
				}
				csid_acquire.sync_mode =
					CAM_ISP_HW_SYNC_SLAVE;
				csid_acquire.master_idx = master_idx;
			}
		}

		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&csid_acquire, sizeof(csid_acquire));
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Cannot acquire ife csid pxl path rsrc %s",
				(is_ipp) ? "IPP" : "PPP");
			goto put_res;
		}

		csid_res->hw_res[i] = csid_acquire.node_res;
		CAM_DBG(CAM_ISP,
			"acquired csid(%s)=%d pxl path rsrc %s successfully",
			(i == 0) ? "left" : "right", hw_intf->hw_idx,
			(is_ipp) ? "IPP" : "PPP");
	}
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_csid, &csid_res);

	csid_res->parent = cid_res;
	cid_res->child[cid_res->num_children++] = csid_res;

	CAM_DBG(CAM_ISP, "acquire res %d CID children = %d",
		csid_acquire.res_id, cid_res->num_children);
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
	CAM_DBG(CAM_ISP, "out_port_type %x", out_port_type);

	switch (out_port_type) {
	case CAM_ISP_IFE_OUT_RES_RDI_0:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_0;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_1:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_1;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_2:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_2;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_3:
		path_id = CAM_IFE_PIX_PATH_RES_RDI_3;
		break;
	default:
		path_id = CAM_IFE_PIX_PATH_RES_MAX;
		CAM_DBG(CAM_ISP, "maximum rdi output type exceeded");
		break;
	}

	CAM_DBG(CAM_ISP, "out_port %x path_id %d", out_port_type, path_id);

	return path_id;
}

static int cam_ife_hw_mgr_acquire_res_ife_csid_rdi(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port)
{
	int rc = -EINVAL;
	int i;

	struct cam_ife_hw_mgr                *ife_hw_mgr;
	struct cam_ife_hw_mgr_res            *csid_res;
	struct cam_ife_hw_mgr_res            *cid_res;
	struct cam_hw_intf                   *hw_intf;
	struct cam_isp_out_port_generic_info *out_port;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;
	enum cam_ife_pix_path_res_id         path_res_id;

	ife_hw_mgr = ife_ctx->hw_mgr;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		path_res_id = cam_ife_hw_mgr_get_ife_csid_rdi_res_type(
			out_port->res_type);
		if (path_res_id == CAM_IFE_PIX_PATH_RES_MAX)
			continue;

		/* get cid resource */
		rc = cam_ife_mgr_acquire_cid_res(ife_ctx, in_port, &cid_res,
			path_res_id);
		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire IFE CID resource Failed");
			goto end;
		}

		/* For each RDI we need CID + PATH resource */
		rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&csid_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto end;
		}

		memset(&csid_acquire, 0, sizeof(csid_acquire));
		csid_acquire.res_id = path_res_id;
		csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_acquire.cid = cid_res->hw_res[0]->res_id;
		csid_acquire.in_port = in_port;
		csid_acquire.out_port = out_port;
		csid_acquire.node_res = NULL;

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

		hw_intf = cid_res->hw_res[0]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&csid_acquire, sizeof(csid_acquire));
		if (rc) {
			CAM_ERR(CAM_ISP,
				"CSID Path reserve failed hw=%d rc=%d cid=%d",
				hw_intf->hw_idx, rc,
				cid_res->hw_res[0]->res_id);

			goto put_res;
		}

		if (csid_acquire.node_res == NULL) {
			CAM_ERR(CAM_ISP, "Acquire CSID RDI rsrc failed");

			goto put_res;
		}

		csid_res->res_type = (enum cam_ife_hw_mgr_res_type)
			CAM_ISP_RESOURCE_PIX_PATH;
		csid_res->res_id = csid_acquire.res_id;
		csid_res->is_dual_vfe = 0;
		csid_res->hw_res[0] = csid_acquire.node_res;
		csid_res->hw_res[1] = NULL;
		csid_res->parent = cid_res;
		cid_res->child[cid_res->num_children++] =
			csid_res;
		CAM_DBG(CAM_ISP, "acquire res %d CID children = %d",
			csid_acquire.res_id, cid_res->num_children);
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

	if (ife_ctx->res_list_ife_in.res_type == CAM_IFE_HW_MGR_RES_UNINIT) {
		/* first acquire */
		ife_ctx->res_list_ife_in.res_type = CAM_IFE_HW_MGR_RES_ROOT;
		ife_ctx->res_list_ife_in.res_id = in_port->res_type;
		ife_ctx->res_list_ife_in.is_dual_vfe = in_port->usage_type;
	} else if ((ife_ctx->res_list_ife_in.res_id !=
		in_port->res_type) && (!ife_ctx->is_fe_enable))  {
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
	struct cam_isp_acquire_hw_info    *acquire_hw_info)
{
	int i;
	struct cam_isp_in_port_info       *in_port = NULL;
	uint32_t                           in_port_length = 0;
	uint32_t                           total_in_port_length = 0;

	in_port = (struct cam_isp_in_port_info *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset);
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {

		if ((in_port->num_out_res > CAM_IFE_HW_OUT_RES_MAX) ||
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
			ife_ctx->is_fe_enable = true;
			break;
		}

		in_port = (struct cam_isp_in_port_info *)((uint8_t *)in_port +
			in_port_length);
	}
	CAM_DBG(CAM_ISP, "is_fe_enable %d", ife_ctx->is_fe_enable);

	return 0;
}

static int cam_ife_mgr_check_and_update_fe_v2(
	struct cam_ife_hw_mgr_ctx         *ife_ctx,
	struct cam_isp_acquire_hw_info    *acquire_hw_info)
{
	int i;
	struct cam_isp_in_port_info_v2    *in_port = NULL;
	uint32_t                           in_port_length = 0;
	uint32_t                           total_in_port_length = 0;

	in_port = (struct cam_isp_in_port_info_v2 *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset);
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {

		if ((in_port->num_out_res > CAM_IFE_HW_OUT_RES_MAX) ||
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
		if (in_port->res_type == CAM_ISP_IFE_IN_RES_RD) {
			ife_ctx->is_fe_enable = true;
			break;
		}

		in_port = (struct cam_isp_in_port_info_v2 *)
			((uint8_t *)in_port + in_port_length);
	}
	CAM_DBG(CAM_ISP, "is_fe_enable %d", ife_ctx->is_fe_enable);

	return 0;
}

static int cam_ife_mgr_check_and_update_fe(
	struct cam_ife_hw_mgr_ctx         *ife_ctx,
	struct cam_isp_acquire_hw_info    *acquire_hw_info)
{
	uint32_t major_ver = 0, minor_ver = 0;

	if (acquire_hw_info == NULL || ife_ctx == NULL)
		return -EINVAL;

	major_ver = (acquire_hw_info->common_info_version >> 12) & 0xF;
	minor_ver = (acquire_hw_info->common_info_version) & 0xFFF;

	switch (major_ver) {
	case 1:
		return cam_ife_mgr_check_and_update_fe_v0(
			ife_ctx, acquire_hw_info);
	case 2:
		return cam_ife_mgr_check_and_update_fe_v2(
			ife_ctx, acquire_hw_info);
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid ver of common info from user");
		return -EINVAL;
	}

	return 0;
}

static int cam_ife_hw_mgr_preprocess_port(
	struct cam_ife_hw_mgr_ctx   *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	int                         *ipp_count,
	int                         *rdi_count,
	int                         *ppp_count,
	int                         *ife_rd_count,
	int                         *lcr_count)
{
	int ipp_num        = 0;
	int rdi_num        = 0;
	int ppp_num        = 0;
	int ife_rd_num     = 0;
	int lcr_num        = 0;
	uint32_t i;
	struct cam_isp_out_port_generic_info *out_port;
	struct cam_ife_hw_mgr *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;

	if (in_port->res_type == CAM_ISP_IFE_IN_RES_RD) {
		ife_rd_num++;
	} else {
		for (i = 0; i < in_port->num_out_res; i++) {
			out_port = &in_port->data[i];
			if (cam_ife_hw_mgr_is_rdi_res(out_port->res_type))
				rdi_num++;
			else if (out_port->res_type == CAM_ISP_IFE_OUT_RES_2PD)
				ppp_num++;
			else if (out_port->res_type == CAM_ISP_IFE_OUT_RES_LCR)
				lcr_num++;
			else {
				CAM_DBG(CAM_ISP, "out_res_type %d",
				out_port->res_type);
				ipp_num++;
			}
		}
	}

	*ipp_count = ipp_num;
	*rdi_count = rdi_num;
	*ppp_count = ppp_num;
	*ife_rd_count = ife_rd_num;
	*lcr_count = lcr_num;

	CAM_DBG(CAM_ISP, "rdi: %d ipp: %d ppp: %d ife_rd: %d lcr: %d",
		rdi_num, ipp_num, ppp_num, ife_rd_num, lcr_num);

	return 0;
}

static int cam_ife_mgr_acquire_hw_for_ctx(
	struct cam_ife_hw_mgr_ctx           *ife_ctx,
	struct cam_isp_in_port_generic_info *in_port,
	uint32_t  *num_pix_port, uint32_t  *num_rdi_port,
	uint32_t *acquired_hw_id, uint32_t *acquired_hw_path)
{
	int rc                                    = -1;
	int is_dual_vfe                           = 0;
	int ipp_count                             = 0;
	int rdi_count                             = 0;
	int ppp_count                             = 0;
	int ife_rd_count                          = 0;
	int lcr_count                             = 0;
	bool crop_enable                          = true;

	is_dual_vfe = in_port->usage_type;

	/* get root node resource */
	rc = cam_ife_hw_mgr_acquire_res_root(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Can not acquire csid rx resource");
		goto err;
	}

	cam_ife_hw_mgr_preprocess_port(ife_ctx, in_port, &ipp_count,
		&rdi_count, &ppp_count, &ife_rd_count, &lcr_count);

	if (!ipp_count && !rdi_count && !ppp_count && !ife_rd_count
		&& !lcr_count) {
		CAM_ERR(CAM_ISP,
			"No PIX or RDI or PPP or IFE RD or LCR resource");
		return -EINVAL;
	}

	if (ipp_count || lcr_count) {
		/* get ife csid IPP resource */
		rc = cam_ife_hw_mgr_acquire_res_ife_csid_pxl(ife_ctx,
			in_port, true, crop_enable);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE CSID IPP/LCR resource Failed");
			goto err;
		}
	}

	if (rdi_count) {
		/* get ife csid RDI resource */
		rc = cam_ife_hw_mgr_acquire_res_ife_csid_rdi(ife_ctx, in_port);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE CSID RDI resource Failed");
			goto err;
		}
	}

	if (ppp_count) {
		/* get ife csid PPP resource */

		/* If both IPP and PPP paths are requested with the same vc dt
		 * it is implied that the sensor is a type 3 PD sensor. Crop
		 * must be enabled for this sensor on PPP path as well.
		 */
		if (!ipp_count)
			crop_enable = false;

		rc = cam_ife_hw_mgr_acquire_res_ife_csid_pxl(ife_ctx,
			in_port, false, crop_enable);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE CSID PPP resource Failed");
			goto err;
		}
	}

	/* get ife src resource */
	if (ife_rd_count) {
		rc = cam_ife_hw_mgr_acquire_res_ife_rd_src(ife_ctx, in_port);
		rc = cam_ife_hw_mgr_acquire_res_bus_rd(ife_ctx, in_port);

		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire IFE RD SRC resource Failed");
			goto err;
		}
	} else if (ipp_count || ppp_count || rdi_count) {
		rc = cam_ife_hw_mgr_acquire_res_ife_src(ife_ctx,
			in_port, false,
			acquired_hw_id, acquired_hw_path);

		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE IPP/PPP SRC resource Failed");
			goto err;
		}
	}

	if (lcr_count) {
		rc = cam_ife_hw_mgr_acquire_res_ife_src(ife_ctx, in_port, true,
		acquired_hw_id, acquired_hw_path);

		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire IFE LCR SRC resource Failed");
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "Acquiring IFE OUT resource...");
	rc = cam_ife_hw_mgr_acquire_res_ife_out(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE OUT resource Failed");
		goto err;
	}

	*num_pix_port = ipp_count + ppp_count + ife_rd_count + lcr_count;
	*num_rdi_port = rdi_count;

	return 0;
err:
	/* release resource at the acquire entry funciton */
	return rc;
}

void cam_ife_cam_cdm_callback(uint32_t handle, void *userdata,
	enum cam_cdm_cb_status status, uint64_t cookie)
{
	struct cam_isp_prepare_hw_update_data *hw_update_data = NULL;
	struct cam_ife_hw_mgr_ctx *ctx = NULL;

	if (!userdata) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return;
	}

	hw_update_data = (struct cam_isp_prepare_hw_update_data *)userdata;
	ctx = (struct cam_ife_hw_mgr_ctx *)hw_update_data->ife_mgr_ctx;

	if (status == CAM_CDM_CB_STATUS_BL_SUCCESS) {
		complete_all(&ctx->config_done_complete);
		atomic_set(&ctx->cdm_done, 1);
		if (g_ife_hw_mgr.debug_cfg.per_req_reg_dump)
			cam_ife_mgr_handle_reg_dump(ctx,
				hw_update_data->reg_dump_buf_desc,
				hw_update_data->num_reg_dump_buf,
				CAM_ISP_PACKET_META_REG_DUMP_PER_REQUEST);

		CAM_DBG(CAM_ISP,
			"Called by CDM hdl=%x, udata=%pK, status=%d, cookie=%llu ctx_index=%d",
			 handle, userdata, status, cookie, ctx->ctx_index);
	} else {
		CAM_WARN(CAM_ISP,
			"Called by CDM hdl=%x, udata=%pK, status=%d, cookie=%llu",
			 handle, userdata, status, cookie);
	}
}

static int cam_ife_mgr_acquire_get_unified_structure_v0(
	struct cam_isp_acquire_hw_info *acquire_hw_info,
	uint32_t offset, uint32_t *input_size,
	struct cam_isp_in_port_generic_info **in_port)
{
	struct cam_isp_in_port_info *in = NULL;
	uint32_t in_port_length = 0;
	struct cam_isp_in_port_generic_info *port_info = NULL;
	int32_t rc = 0, i;

	in = (struct cam_isp_in_port_info *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset + *input_size);

	in_port_length = sizeof(struct cam_isp_in_port_info) +
		(in->num_out_res - 1) *
		sizeof(struct cam_isp_out_port_info);

	*input_size += in_port_length;

	if ((*input_size) > acquire_hw_info->input_info_size) {
		CAM_ERR(CAM_ISP, "Input is not proper");
		rc = -EINVAL;
	}

	port_info = kzalloc(
		sizeof(struct cam_isp_in_port_generic_info), GFP_KERNEL);

	if (!port_info)
		return -ENOMEM;

	port_info->major_ver       =
		(acquire_hw_info->input_info_version >> 16) & 0xFFFF;
	port_info->minor_ver       =
		acquire_hw_info->input_info_version & 0xFFFF;
	port_info->res_type        =  in->res_type;
	port_info->lane_type       =  in->lane_type;
	port_info->lane_num        =  in->lane_num;
	port_info->lane_cfg        =  in->lane_cfg;
	port_info->vc[0]           =  in->vc;
	port_info->dt[0]           =  in->dt;
	port_info->num_valid_vc_dt = 1;
	port_info->format          =  in->format;
	port_info->test_pattern    =  in->test_pattern;
	port_info->usage_type      =  in->usage_type;
	port_info->left_start      =  in->left_start;
	port_info->left_stop       =  in->left_stop;
	port_info->left_width      =  in->left_width;
	port_info->right_start     =  in->right_start;
	port_info->right_stop      =  in->right_stop;
	port_info->right_width     =  in->right_width;
	port_info->line_start      =  in->line_start;
	port_info->line_stop       =  in->line_stop;
	port_info->height          =  in->height;
	port_info->pixel_clk       =  in->pixel_clk;
	port_info->batch_size      =  in->batch_size;
	port_info->dsp_mode        =  in->dsp_mode;
	port_info->hbi_cnt         =  in->hbi_cnt;
	port_info->cust_node       =  0;
	port_info->horizontal_bin  =  0;
	port_info->qcfa_bin        =  0;
	port_info->num_out_res     =  in->num_out_res;

	port_info->data = kcalloc(in->num_out_res,
		sizeof(struct cam_isp_out_port_generic_info),
		GFP_KERNEL);
	if (port_info->data == NULL) {
		rc = -ENOMEM;
		goto release_port_mem;
	}

	for (i = 0; i < in->num_out_res; i++) {
		port_info->data[i].res_type     = in->data[i].res_type;
		port_info->data[i].format       = in->data[i].format;
		port_info->data[i].width        = in->data[i].width;
		port_info->data[i].height       = in->data[i].height;
		port_info->data[i].comp_grp_id  = in->data[i].comp_grp_id;
		port_info->data[i].split_point  = in->data[i].split_point;
		port_info->data[i].secure_mode  = in->data[i].secure_mode;
		port_info->data[i].reserved     = in->data[i].reserved;
	}
	*in_port = port_info;

	return 0;
release_port_mem:
	kfree(port_info);
	return rc;
}

static int cam_ife_mgr_acquire_get_unified_structure_v2(
	struct cam_isp_acquire_hw_info *acquire_hw_info,
	uint32_t offset, uint32_t *input_size,
	struct cam_isp_in_port_generic_info **in_port)
{
	struct cam_isp_in_port_info_v2 *in = NULL;
	uint32_t in_port_length = 0;
	struct cam_isp_in_port_generic_info *port_info = NULL;
	int32_t rc = 0, i;

	in = (struct cam_isp_in_port_info_v2 *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset + *input_size);

	in_port_length = sizeof(struct cam_isp_in_port_info_v2) +
		(in->num_out_res - 1) *
		sizeof(struct cam_isp_out_port_info_v2);

	*input_size += in_port_length;

	if ((*input_size) > acquire_hw_info->input_info_size) {
		CAM_ERR(CAM_ISP, "Input is not proper");
		rc = -EINVAL;
	}

	port_info = kzalloc(
		sizeof(struct cam_isp_in_port_generic_info), GFP_KERNEL);

	if (!port_info)
		return -ENOMEM;

	port_info->major_ver       =
		(acquire_hw_info->input_info_version >> 16) & 0xFFFF;
	port_info->minor_ver       =
		acquire_hw_info->input_info_version & 0xFFFF;
	port_info->res_type        =  in->res_type;
	port_info->lane_type       =  in->lane_type;
	port_info->lane_num        =  in->lane_num;
	port_info->lane_cfg        =  in->lane_cfg;
	port_info->num_valid_vc_dt =  in->num_valid_vc_dt;

	if (port_info->num_valid_vc_dt == 0 ||
		port_info->num_valid_vc_dt >= CAM_ISP_VC_DT_CFG) {
		CAM_ERR(CAM_ISP, "Invalid i/p arg invalid vc-dt: %d",
			in->num_valid_vc_dt);
		rc = -EINVAL;
		goto release_mem;
	}

	for (i = 0; i < port_info->num_valid_vc_dt; i++) {
		port_info->vc[i]      =  in->vc[i];
		port_info->dt[i]      =  in->dt[i];
	}

	port_info->format         =  in->format;
	port_info->test_pattern   =  in->test_pattern;
	port_info->usage_type     =  in->usage_type;
	port_info->left_start     =  in->left_start;
	port_info->left_stop      =  in->left_stop;
	port_info->left_width     =  in->left_width;
	port_info->right_start    =  in->right_start;
	port_info->right_stop     =  in->right_stop;
	port_info->right_width    =  in->right_width;
	port_info->line_start     =  in->line_start;
	port_info->line_stop      =  in->line_stop;
	port_info->height         =  in->height;
	port_info->pixel_clk      =  in->pixel_clk;
	port_info->batch_size     =  in->batch_size;
	port_info->dsp_mode       =  in->dsp_mode;
	port_info->hbi_cnt        =  in->hbi_cnt;
	port_info->cust_node      =  in->cust_node;
	port_info->horizontal_bin =  in->horizontal_bin;
	port_info->qcfa_bin       =  in->qcfa_bin;
	port_info->num_out_res    =  in->num_out_res;

	port_info->data = kcalloc(in->num_out_res,
		sizeof(struct cam_isp_out_port_generic_info),
		GFP_KERNEL);
	if (port_info->data == NULL) {
		rc = -ENOMEM;
		goto release_mem;
	}

	for (i = 0; i < port_info->num_out_res; i++) {
		port_info->data[i].res_type     = in->data[i].res_type;
		port_info->data[i].format       = in->data[i].format;
		port_info->data[i].width        = in->data[i].width;
		port_info->data[i].height       = in->data[i].height;
		port_info->data[i].comp_grp_id  = in->data[i].comp_grp_id;
		port_info->data[i].split_point  = in->data[i].split_point;
		port_info->data[i].secure_mode  = in->data[i].secure_mode;
	}

	*in_port = port_info;

	return 0;

release_mem:
	kfree(port_info);
	return rc;
}

static int cam_ife_mgr_acquire_get_unified_structure(
	struct cam_isp_acquire_hw_info *acquire_hw_info,
	uint32_t offset, uint32_t *input_size,
	struct cam_isp_in_port_generic_info **in_port)
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
	struct cam_ife_hw_mgr_ctx         *ife_ctx;
	struct cam_isp_in_port_generic_info   *in_port = NULL;
	struct cam_cdm_acquire_data        cdm_acquire;
	uint32_t                           num_pix_port_per_in = 0;
	uint32_t                           num_rdi_port_per_in = 0;
	uint32_t                           total_pix_port = 0;
	uint32_t                           total_rdi_port = 0;
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

	ife_ctx->common.cb_priv = acquire_args->context_data;
	for (i = 0; i < CAM_ISP_HW_EVENT_MAX; i++)
		ife_ctx->common.event_cb[i] = acquire_args->event_cb;

	ife_ctx->hw_mgr = ife_hw_mgr;


	memcpy(cdm_acquire.identifier, "ife", sizeof("ife"));
	cdm_acquire.cell_index = 0;
	cdm_acquire.handle = 0;
	cdm_acquire.userdata = ife_ctx;
	cdm_acquire.base_array_cnt = CAM_IFE_HW_NUM_MAX;
	for (i = 0, j = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (ife_hw_mgr->cdm_reg_map[i])
			cdm_acquire.base_array[j++] =
				ife_hw_mgr->cdm_reg_map[i];
	}
	cdm_acquire.base_array_cnt = j;

	cdm_acquire.id = CAM_CDM_VIRTUAL;
	cdm_acquire.cam_cdm_callback = cam_ife_cam_cdm_callback;
	rc = cam_cdm_acquire(&cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to acquire the CDM HW");
		goto free_ctx;
	}

	CAM_DBG(CAM_ISP, "Successfully acquired the CDM HW hdl=%x",
		cdm_acquire.handle);
	ife_ctx->cdm_handle = cdm_acquire.handle;
	ife_ctx->cdm_ops = cdm_acquire.ops;
	atomic_set(&ife_ctx->cdm_done, 1);

	acquire_hw_info =
		(struct cam_isp_acquire_hw_info *)acquire_args->acquire_info;

	rc = cam_ife_mgr_check_and_update_fe(ife_ctx, acquire_hw_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "buffer size is not enough");
		goto free_cdm;
	}

	/* acquire HW resources */
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {
		rc = cam_ife_mgr_acquire_get_unified_structure(acquire_hw_info,
			i, &input_size, &in_port);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Failed in parsing: %d", rc);
			goto free_res;
		}
		CAM_DBG(CAM_ISP, "in_res_type %x", in_port->res_type);

		rc = cam_ife_mgr_acquire_hw_for_ctx(ife_ctx, in_port,
			&num_pix_port_per_in, &num_rdi_port_per_in,
			&acquire_args->acquired_hw_id[i],
			acquire_args->acquired_hw_path[i]);

		total_pix_port += num_pix_port_per_in;
		total_rdi_port += num_rdi_port_per_in;

		if (rc) {
			CAM_ERR(CAM_ISP, "can not acquire resource");
			cam_ife_hw_mgr_dump_src_acq_info(ife_ctx,
				total_pix_port, total_rdi_port);
			goto free_mem;
		}

		kfree(in_port->data);
		kfree(in_port);
		in_port = NULL;
	}

	/* Check whether context has only RDI resource */
	if (!total_pix_port) {
		ife_ctx->is_rdi_only_context = 1;
		CAM_DBG(CAM_ISP, "RDI only context");
	}

	/* Process base info */
	rc = cam_ife_mgr_process_base_info(ife_ctx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Process base info failed");
		goto free_res;
	}

	acquire_args->ctxt_to_hw_map = ife_ctx;
	ife_ctx->ctx_in_use = 1;

	acquire_args->valid_acquired_hw =
		acquire_hw_info->num_inputs;

	getnstimeofday64(&ife_ctx->ts);
	CAM_INFO(CAM_ISP,
		"Acquire HW success with total_pix: %u total_rdi: %u is_dual: %u in ctx: %u",
		total_pix_port, total_rdi_port,
		ife_ctx->is_dual, ife_ctx->ctx_index);

	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->used_ctx_list, &ife_ctx);

	return 0;
free_mem:
	kfree(in_port->data);
	kfree(in_port);
free_res:
	cam_ife_hw_mgr_release_hw_for_ctx(ife_ctx);
free_cdm:
	cam_cdm_release(ife_ctx->cdm_handle);
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
	gen_port_info->format          =  in->format;
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
	uint32_t                               num_pix_port_per_in = 0;
	uint32_t                               num_rdi_port_per_in = 0;
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

	ife_ctx->common.cb_priv = acquire_args->context_data;
	for (i = 0; i < CAM_ISP_HW_EVENT_MAX; i++)
		ife_ctx->common.event_cb[i] = acquire_args->event_cb;

	ife_ctx->hw_mgr = ife_hw_mgr;


	memcpy(cdm_acquire.identifier, "ife", sizeof("ife"));
	cdm_acquire.cell_index = 0;
	cdm_acquire.handle = 0;
	cdm_acquire.userdata = ife_ctx;
	cdm_acquire.base_array_cnt = CAM_IFE_HW_NUM_MAX;
	for (i = 0, j = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (ife_hw_mgr->cdm_reg_map[i])
			cdm_acquire.base_array[j++] =
				ife_hw_mgr->cdm_reg_map[i];
	}
	cdm_acquire.base_array_cnt = j;


	cdm_acquire.id = CAM_CDM_VIRTUAL;
	cdm_acquire.cam_cdm_callback = cam_ife_cam_cdm_callback;
	rc = cam_cdm_acquire(&cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to acquire the CDM HW");
		goto free_ctx;
	}

	CAM_DBG(CAM_ISP, "Successfully acquired the CDM HW hdl=%x",
		cdm_acquire.handle);
	ife_ctx->cdm_handle = cdm_acquire.handle;
	ife_ctx->cdm_ops = cdm_acquire.ops;
	atomic_set(&ife_ctx->cdm_done, 1);

	isp_resource = (struct cam_isp_resource *)acquire_args->acquire_info;

	/* acquire HW resources */
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
			if (in_port->num_out_res > CAM_IFE_HW_OUT_RES_MAX) {
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

			gen_port_info = kzalloc(
				sizeof(struct cam_isp_in_port_generic_info),
				GFP_KERNEL);
			if (gen_port_info == NULL) {
				rc = -ENOMEM;
				goto free_res;
			}

			gen_port_info->data = kcalloc(
				sizeof(struct cam_isp_out_port_generic_info),
				in_port->num_out_res, GFP_KERNEL);
			if (gen_port_info->data == NULL) {
				kfree(gen_port_info);
				gen_port_info = NULL;
				rc = -ENOMEM;
				goto free_res;
			}

			cam_ife_mgr_acquire_get_unified_dev_str(in_port,
				gen_port_info);

			rc = cam_ife_mgr_acquire_hw_for_ctx(ife_ctx,
				gen_port_info, &num_pix_port_per_in,
				&num_rdi_port_per_in,
				&acquire_args->acquired_hw_id[i],
				acquire_args->acquired_hw_path[i]);

			total_pix_port += num_pix_port_per_in;
			total_rdi_port += num_rdi_port_per_in;

			kfree(in_port);
			if (gen_port_info != NULL) {
				kfree(gen_port_info->data);
				kfree(gen_port_info);
				gen_port_info = NULL;
			}
			if (rc) {
				CAM_ERR(CAM_ISP, "can not acquire resource");
				goto free_res;
			}
		} else {
			CAM_ERR(CAM_ISP,
				"Copy from user failed with in_port = %pK",
				in_port);
			rc = -EFAULT;
			goto free_res;
		}
	}

	/* Check whether context has only RDI resource */
	if (!total_pix_port) {
		ife_ctx->is_rdi_only_context = 1;
		CAM_DBG(CAM_ISP, "RDI only context");
	}

	/* Process base info */
	rc = cam_ife_mgr_process_base_info(ife_ctx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Process base info failed");
		goto free_res;
	}

	acquire_args->ctxt_to_hw_map = ife_ctx;
	ife_ctx->ctx_in_use = 1;

	CAM_INFO(CAM_ISP,
		"Acquire HW success with total_pix: %u total_rdi: %u is_dual: %u in ctx: %u",
		total_pix_port, total_rdi_port,
		ife_ctx->is_dual, ife_ctx->ctx_index);

	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->used_ctx_list, &ife_ctx);

	return 0;
free_res:
	cam_ife_hw_mgr_release_hw_for_ctx(ife_ctx);
	cam_cdm_release(ife_ctx->cdm_handle);
free_ctx:
	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->free_ctx_list, &ife_ctx);
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
	default:
		return "USAGE_INVALID";
	}
}

static int cam_isp_classify_vote_info(
	struct cam_ife_hw_mgr_res            *hw_mgr_res,
	struct cam_isp_bw_config_v2          *bw_config,
	struct cam_axi_vote                  *isp_vote,
	uint32_t                              split_idx,
	bool                                 *camif_l_bw_updated,
	bool                                 *camif_r_bw_updated)
{
	int                                   rc = 0, i, j = 0;

	if ((hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF)
		|| (hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_RD) ||
		(hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_PDLIB) ||
		(hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_LCR)) {
		if (split_idx == CAM_ISP_HW_SPLIT_LEFT) {
			if (*camif_l_bw_updated)
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

			*camif_l_bw_updated = true;
		} else {
			if (*camif_r_bw_updated)
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

			*camif_r_bw_updated = true;
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
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_vfe_bw_update_args_v2       bw_upd_args;
	int                                    rc = -EINVAL;
	uint32_t                               i, split_idx;
	bool                                   camif_l_bw_updated = false;
	bool                                   camif_r_bw_updated = false;

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
				&bw_upd_args.isp_vote, split_idx,
				&camif_l_bw_updated, &camif_r_bw_updated);
			if (rc)
				return rc;

			if (!bw_upd_args.isp_vote.num_paths)
				continue;

			hw_intf = hw_mgr_res->hw_res[split_idx]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				bw_upd_args.node_res =
					hw_mgr_res->hw_res[split_idx];

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

	return rc;
}

static int cam_isp_blob_bw_update(
	struct cam_isp_bw_config              *bw_config,
	struct cam_ife_hw_mgr_ctx             *ctx)
{
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
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

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_mgr_priv || !config_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	cfg = config_hw_args;
	ctx = (struct cam_ife_hw_mgr_ctx *)cfg->ctxt_to_hw_map;
	if (!ctx) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	if (!ctx->ctx_in_use || !ctx->cdm_cmd) {
		CAM_ERR(CAM_ISP, "Invalid context parameters");
		return -EPERM;
	}
	if (atomic_read(&ctx->overflow_pending))
		return -EINVAL;

	hw_update_data = (struct cam_isp_prepare_hw_update_data  *) cfg->priv;
	hw_update_data->ife_mgr_ctx = ctx;

	CAM_DBG(CAM_ISP, "Ctx[%pK][%d] : Applying Req %lld",
		ctx, ctx->ctx_index, cfg->request_id);

	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (hw_update_data->bw_config_valid[i] == true) {

			CAM_DBG(CAM_PERF, "idx=%d, bw_config_version=%d",
				ctx, ctx->ctx_index, i,
				hw_update_data->bw_config_version);

			if (hw_update_data->bw_config_version ==
				CAM_ISP_BW_CONFIG_V1) {
				rc = cam_isp_blob_bw_update(
					(struct cam_isp_bw_config *)
					&hw_update_data->bw_config[i], ctx);
				if (rc)
					CAM_ERR(CAM_PERF,
					"Bandwidth Update Failed rc: %d", rc);
			} else if (hw_update_data->bw_config_version ==
				CAM_ISP_BW_CONFIG_V2) {
				rc = cam_isp_blob_bw_update_v2(
					(struct cam_isp_bw_config_v2 *)
					&hw_update_data->bw_config_v2[i], ctx);
				if (rc)
					CAM_ERR(CAM_PERF,
					"Bandwidth Update Failed rc: %d", rc);

			} else {
				CAM_ERR(CAM_PERF,
					"Invalid bw config version: %d",
					hw_update_data->bw_config_version);
			}
		}
	}

	CAM_DBG(CAM_ISP,
		"Enter ctx id:%d num_hw_upd_entries %d request id: %llu",
		ctx->ctx_index, cfg->num_hw_update_entries, cfg->request_id);

	if (cfg->num_hw_update_entries > 0) {
		cdm_cmd = ctx->cdm_cmd;
		cdm_cmd->type = CAM_CDM_BL_CMD_TYPE_MEM_HANDLE;
		cdm_cmd->flag = true;
		cdm_cmd->userdata = hw_update_data;
		cdm_cmd->cookie = cfg->request_id;

		for (i = 0 ; i < cfg->num_hw_update_entries; i++) {
			cmd = (cfg->hw_update_entries + i);

			if (cfg->reapply &&
				cmd->flags == CAM_ISP_IQ_BL) {
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
		}
		cdm_cmd->cmd_arrary_count = cfg->num_hw_update_entries - skip;

		reinit_completion(&ctx->config_done_complete);
		ctx->applied_req_id = cfg->request_id;

		CAM_DBG(CAM_ISP, "Submit to CDM");
		atomic_set(&ctx->cdm_done, 0);
		rc = cam_cdm_submit_bls(ctx->cdm_handle, cdm_cmd);
		if (rc) {
			CAM_ERR(CAM_ISP, "Failed to apply the configs");
			return rc;
		}

		if (cfg->init_packet) {
			rc = wait_for_completion_timeout(
				&ctx->config_done_complete,
				msecs_to_jiffies(100));
			if (rc <= 0) {
				CAM_ERR(CAM_ISP,
					"config done completion timeout for req_id=%llu rc=%d ctx_index %d",
					cfg->request_id, rc, ctx->ctx_index);
				if (rc == 0)
					rc = -ETIMEDOUT;
			} else {
				rc = 0;
				CAM_DBG(CAM_ISP,
					"config done Success for req_id=%llu ctx_index %d",
					cfg->request_id, ctx->ctx_index);
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
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	struct cam_ife_hw_mgr_ctx        *ctx;
	uint32_t                          i, master_base_idx = 0;

	if (!stop_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}
	ctx = (struct cam_ife_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
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


	/* stop the master CIDs first */
	cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_cid,
		master_base_idx, CAM_CSID_HALT_IMMEDIATELY);

	/* stop rest of the CIDs  */
	for (i = 0; i < ctx->num_base; i++) {
		if (i == master_base_idx)
			continue;
		cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_cid,
			ctx->base[i].idx, CAM_CSID_HALT_IMMEDIATELY);
	}

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
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++)
		cam_ife_hw_mgr_stop_hw_res(&ctx->res_list_ife_out[i]);


	/* Stop tasklet for context */
	cam_tasklet_stop(ctx->common.tasklet_info);
	CAM_DBG(CAM_ISP, "Exit...ctx id:%d rc :%d",
		ctx->ctx_index, rc);

	return rc;
}

static int cam_ife_mgr_bw_control(struct cam_ife_hw_mgr_ctx *ctx,
	enum cam_vfe_bw_control_action action)
{
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_vfe_bw_control_args         bw_ctrl_args;
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
					sizeof(struct cam_vfe_bw_control_args));
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
	return cam_ife_mgr_bw_control(ctx, CAM_VFE_BW_CONTROL_EXCLUDE);
}

/* entry function: stop_hw */
static int cam_ife_mgr_stop_hw(void *hw_mgr_priv, void *stop_hw_args)
{
	int                               rc        = 0;
	struct cam_hw_stop_args          *stop_args = stop_hw_args;
	struct cam_isp_stop_args         *stop_isp;
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	struct cam_ife_hw_mgr_ctx        *ctx;
	enum cam_ife_csid_halt_cmd        csid_halt_type;
	uint32_t                          i, master_base_idx = 0;

	if (!hw_mgr_priv || !stop_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, " Enter...ctx id:%d", ctx->ctx_index);
	stop_isp = (struct cam_isp_stop_args    *)stop_args->args;

	/* Set the csid halt command */
	if (stop_isp->hw_stop_cmd == CAM_ISP_HW_STOP_AT_FRAME_BOUNDARY)
		csid_halt_type = CAM_CSID_HALT_AT_FRAME_BOUNDARY;
	else
		csid_halt_type = CAM_CSID_HALT_IMMEDIATELY;

	/* Note:stop resource will remove the irq mask from the hardware */

	if (!ctx->num_base) {
		CAM_ERR(CAM_ISP, "number of bases are zero");
		return -EINVAL;
	}

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

	CAM_DBG(CAM_ISP, "Stopping master CID idx %d", master_base_idx);

	/* Stop the master CIDs first */
	cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_cid,
			master_base_idx, csid_halt_type);

	/* stop rest of the CIDs */
	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].idx == master_base_idx)
			continue;
		CAM_DBG(CAM_ISP, "Stopping CID idx %d i %d master %d",
			ctx->base[i].idx, i, master_base_idx);
		cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_cid,
			ctx->base[i].idx, csid_halt_type);
	}

	CAM_DBG(CAM_ISP, "Going to stop IFE Out");

	/* IFE out resources */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++)
		cam_ife_hw_mgr_stop_hw_res(&ctx->res_list_ife_out[i]);

	/* IFE bus rd resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		cam_ife_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	CAM_DBG(CAM_ISP, "Going to stop IFE Mux");

	/* IFE mux in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		cam_ife_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	cam_tasklet_stop(ctx->common.tasklet_info);

	cam_ife_mgr_pause_hw(ctx);

	wait_for_completion(&ctx->config_done_complete);

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
		if (hw_idx != hw_mgr->ife_devices[i]->hw_idx)
			continue;
		CAM_DBG(CAM_ISP, "VFE (id = %d) reset", hw_idx);
		vfe_hw_intf = hw_mgr->ife_devices[i];
		vfe_hw_intf->hw_ops.reset(vfe_hw_intf->hw_priv,
			&vfe_reset_type, sizeof(vfe_reset_type));
		break;
	}

	CAM_DBG(CAM_ISP, "Exit Successfully");
	return 0;
}

static int cam_ife_mgr_restart_hw(void *start_hw_args)
{
	int                               rc = -1;
	struct cam_hw_start_args         *start_args = start_hw_args;
	struct cam_ife_hw_mgr_ctx        *ctx;
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	uint32_t                          i;

	if (!start_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)start_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "START IFE OUT ... in ctx id:%d", ctx->ctx_index);

	cam_tasklet_start(ctx->common.tasklet_info);

	/* start the IFE out devices */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
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
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE CSID (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START CID SRC ... in ctx id:%d", ctx->ctx_index);
	/* Start IFE root node: do nothing */
	CAM_DBG(CAM_ISP, "Exit...(success)");
	return 0;

err:
	cam_ife_mgr_stop_hw_in_overflow(start_hw_args);
	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

static int cam_ife_mgr_start_hw(void *hw_mgr_priv, void *start_hw_args)
{
	int                               rc = -1;
	struct cam_isp_start_args        *start_isp = start_hw_args;
	struct cam_hw_stop_args           stop_args;
	struct cam_isp_stop_args          stop_isp;
	struct cam_ife_hw_mgr_ctx        *ctx;
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	struct cam_isp_resource_node     *rsrc_node = NULL;
	uint32_t                          i, camif_debug;
	bool                              res_rdi_context_set = false;
	uint32_t                          primary_rdi_src_res;
	uint32_t                          primary_rdi_out_res;
	uint32_t                          last_rdi_src_res, last_rdi_res;
	uint32_t                          last_rdi_out_res;

	primary_rdi_src_res = CAM_ISP_HW_VFE_IN_MAX;
	primary_rdi_out_res = CAM_ISP_IFE_OUT_RES_MAX;

	if (!hw_mgr_priv || !start_isp) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)
		start_isp->hw_config.ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	if ((!ctx->init_done) && start_isp->start_only) {
		CAM_ERR(CAM_ISP, "Invalid args init_done %d start_only %d",
			ctx->init_done, start_isp->start_only);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "Enter... ctx id:%d",
		ctx->ctx_index);

	/* update Bandwidth should be done at the hw layer */

	cam_tasklet_start(ctx->common.tasklet_info);

	if (ctx->init_done && start_isp->start_only)
		goto start_only;

	/* set current csid debug information to CSID HW */
	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (g_ife_hw_mgr.csid_devices[i])
			rc = g_ife_hw_mgr.csid_devices[i]->hw_ops.process_cmd(
				g_ife_hw_mgr.csid_devices[i]->hw_priv,
				CAM_IFE_CSID_SET_CSID_DEBUG,
				&g_ife_hw_mgr.debug_cfg.csid_debug,
				sizeof(g_ife_hw_mgr.debug_cfg.csid_debug));
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

	ctx->init_done = true;

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

	CAM_DBG(CAM_ISP, "start cdm interface");
	rc = cam_cdm_stream_on(ctx->cdm_handle);
	if (rc) {
		CAM_ERR(CAM_ISP, "Can not start cdm (%d)",
			 ctx->cdm_handle);
		goto safe_disable;
	}

start_only:
	/* Apply initial configuration */
	CAM_DBG(CAM_ISP, "Config HW");
	rc = cam_ife_mgr_config_hw(hw_mgr_priv, &start_isp->hw_config);
	if (rc) {
		CAM_ERR(CAM_ISP, "Config HW failed");
		goto cdm_streamoff;
	}

	CAM_DBG(CAM_ISP, "START IFE OUT ... in ctx id:%d",
		ctx->ctx_index);

	/* Find out last rdi out resource */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		hw_mgr_res = &ctx->res_list_ife_out[i];
		switch (hw_mgr_res->res_id) {
		case CAM_ISP_IFE_OUT_RES_RDI_0:
		case CAM_ISP_IFE_OUT_RES_RDI_1:
		case CAM_ISP_IFE_OUT_RES_RDI_2:
		case CAM_ISP_IFE_OUT_RES_RDI_3:
			if (ctx->is_rdi_only_context) {
				last_rdi_res = i;
				last_rdi_out_res = hw_mgr_res->res_id;
			}
		default:
			break;
		}
	}
	if (ctx->is_rdi_only_context) {
		hw_mgr_res = &ctx->res_list_ife_out[last_rdi_res];
		hw_mgr_res->hw_res[0]->rdi_only_last_res =
				ctx->is_rdi_only_context;
	}

	/* start the IFE out devices */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		hw_mgr_res = &ctx->res_list_ife_out[i];
		switch (hw_mgr_res->res_id) {
		case CAM_ISP_IFE_OUT_RES_RDI_0:
		case CAM_ISP_IFE_OUT_RES_RDI_1:
		case CAM_ISP_IFE_OUT_RES_RDI_2:
		case CAM_ISP_IFE_OUT_RES_RDI_3:
			if (!res_rdi_context_set && ctx->is_rdi_only_context) {
				hw_mgr_res->hw_res[0]->rdi_only_ctx =
					ctx->is_rdi_only_context;
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

	CAM_DBG(CAM_ISP, "START IFE BUS RD ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the IFE mux in devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_in_rd, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE BUS RD (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	if (primary_rdi_out_res < CAM_ISP_IFE_OUT_RES_MAX)
		primary_rdi_src_res =
			cam_convert_rdi_out_res_id_to_src(primary_rdi_out_res);
	if (last_rdi_out_res < CAM_ISP_IFE_OUT_RES_MAX)
		last_rdi_src_res =
			cam_convert_rdi_out_res_id_to_src(last_rdi_out_res);

	CAM_DBG(CAM_ISP, "START IFE SRC ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the IFE mux in devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		if (primary_rdi_src_res == hw_mgr_res->res_id) {
			hw_mgr_res->hw_res[0]->rdi_only_ctx =
				ctx->is_rdi_only_context;
		}
		if (last_rdi_src_res == hw_mgr_res->res_id) {
			hw_mgr_res->hw_res[0]->rdi_only_last_res =
				ctx->is_rdi_only_context;
		}

		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE MUX (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START CSID HW ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the IFE CSID HW devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE CSID (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START CID SRC ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the IFE CID HW devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_cid, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE CSID (%d)",
				hw_mgr_res->res_id);
			goto err;
		}
	}

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
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	int                               rc = 0, i = 0;

	if (!hw_mgr_priv || !hw_reset_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)reset_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "Reset CSID and VFE");
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		rc = cam_ife_hw_mgr_reset_csid_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Failed to reset CSID:%d rc: %d",
				hw_mgr_res->res_id, rc);
			goto end;
		}
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

	if (!hw_mgr_priv || !release_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)release_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "Enter...ctx id:%d",
		ctx->ctx_index);

	if (ctx->init_done)
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
	ctx->ctx_in_use = 0;
	ctx->is_rdi_only_context = 0;
	ctx->cdm_handle = 0;
	ctx->cdm_ops = NULL;
	atomic_set(&ctx->overflow_pending, 0);
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		ctx->sof_cnt[i] = 0;
		ctx->eof_cnt[i] = 0;
		ctx->epoch_cnt[i] = 0;
	}

	CAM_INFO(CAM_ISP, "Release HW success ctx id: %u",
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
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
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
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    num_ent, rc = 0;

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

			if (res_id_out >= CAM_IFE_HW_OUT_RES_MAX) {
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
					"Failed cmd_update, base_idx=%d, bytes_used=%u, res_id_out=0x%x",
					blob_info->base_info->idx,
					bytes_used,
					res_id_out);
				goto end;
			}

			total_used_bytes += bytes_used;
		}

		if (total_used_bytes) {
			/* Update the HW entries */
			num_ent = prepare->num_hw_update_entries;
			prepare->hw_update_entries[num_ent].handle =
				kmd_buf_info->handle;
			prepare->hw_update_entries[num_ent].len =
				total_used_bytes;
			prepare->hw_update_entries[num_ent].offset =
				kmd_buf_info->offset;
			num_ent++;

			kmd_buf_info->used_bytes += total_used_bytes;
			kmd_buf_info->offset     += total_used_bytes;
			prepare->num_hw_update_entries = num_ent;
		}
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
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    num_ent, rc = 0;
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

		if (res_id_out >= CAM_IFE_HW_OUT_RES_MAX) {
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
				"Failed cmd_update, base_idx=%d, bytes_used=%u, res_id_out=0x%x",
				blob_info->base_info->idx,
				bytes_used,
				res_id_out);
			goto end;
		}

		total_used_bytes += bytes_used;
	}

	if (total_used_bytes) {
		/* Update the HW entries */
		num_ent = prepare->num_hw_update_entries;
		prepare->hw_update_entries[num_ent].handle =
			kmd_buf_info->handle;
		prepare->hw_update_entries[num_ent].len =
			total_used_bytes;
		prepare->hw_update_entries[num_ent].offset =
			kmd_buf_info->offset;
		num_ent++;

		kmd_buf_info->used_bytes += total_used_bytes;
		kmd_buf_info->offset     += total_used_bytes;
		prepare->num_hw_update_entries = num_ent;
	}
end:
	return rc;
}

static int cam_isp_blob_hfr_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_resource_hfr_config    *hfr_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_isp_port_hfr_config        *port_hfr_config;
	struct cam_kmd_buf_info               *kmd_buf_info;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    num_ent, rc = 0;

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

		CAM_DBG(CAM_ISP, "hfr config idx %d, type=%d", i,
			res_id_out);

		if (res_id_out >= CAM_IFE_HW_OUT_RES_MAX) {
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
		hw_mgr_res = &ctx->res_list_ife_out[res_id_out];

		rc = cam_isp_add_cmd_buf_update(
			hw_mgr_res, blob_type,
			blob_type_hw_cmd_map[blob_type],
			blob_info->base_info->idx,
			(void *)cmd_buf_addr,
			kmd_buf_remain_size,
			(void *)port_hfr_config,
			&bytes_used);
		if (rc < 0) {
			CAM_ERR(CAM_ISP,
				"Failed cmd_update, base_idx=%d, rc=%d",
				blob_info->base_info->idx, bytes_used);
			return rc;
		}

		total_used_bytes += bytes_used;
	}

	if (total_used_bytes) {
		/* Update the HW entries */
		num_ent = prepare->num_hw_update_entries;
		prepare->hw_update_entries[num_ent].handle =
			kmd_buf_info->handle;
		prepare->hw_update_entries[num_ent].len = total_used_bytes;
		prepare->hw_update_entries[num_ent].offset =
			kmd_buf_info->offset;
		num_ent++;

		kmd_buf_info->used_bytes += total_used_bytes;
		kmd_buf_info->offset += total_used_bytes;
		prepare->num_hw_update_entries = num_ent;
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
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
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
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
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
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	uint64_t                               clk_rate = 0;
	int                                    rc = 0, i;
	struct cam_vfe_core_config_args        vfe_core_config;

	ctx = prepare->ctxt_to_hw_map;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i] ||
				hw_mgr_res->res_id != CAM_ISP_HW_VFE_IN_CAMIF)
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				vfe_core_config.node_res =
					hw_mgr_res->hw_res[i];

				memcpy(&vfe_core_config.core_config,
					core_config,
					sizeof(struct cam_isp_core_config));

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CORE_CONFIG,
					&vfe_core_config,
					sizeof(
					struct cam_vfe_core_config_args));
				if (rc)
					CAM_ERR(CAM_ISP, "Core cfg parse fail");
			} else {
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
			}
		}
	}

	return rc;
}

static int cam_isp_blob_clock_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_clock_config           *clock_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_ife_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_vfe_clock_update_args       clock_upd_args;
	uint64_t                               clk_rate = 0;
	int                                    rc = -EINVAL;
	uint32_t                               i;
	uint32_t                               j;
	bool                                   camif_l_clk_updated = false;
	bool                                   camif_r_clk_updated = false;

	ctx = prepare->ctxt_to_hw_map;

	CAM_DBG(CAM_PERF,
		"usage=%u left_clk= %lu right_clk=%lu",
		clock_config->usage_type,
		clock_config->left_pix_hz,
		clock_config->right_pix_hz);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i])
				continue;

			if (hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF) {
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
			} else if (hw_mgr_res->res_id ==
				CAM_ISP_HW_VFE_IN_PDLIB) {
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
				clock_upd_args.node_res =
					hw_mgr_res->hw_res[i];
				CAM_DBG(CAM_PERF,
				"res_id=%u i= %d clk=%llu\n",
				hw_mgr_res->res_id, i, clk_rate);

				clock_upd_args.clk_rate = clk_rate;

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CLOCK_UPDATE,
					&clock_upd_args,
					sizeof(
					struct cam_vfe_clock_update_args));
				if (rc)
					CAM_ERR(CAM_PERF,
						"Clock Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_isp_blob_vfe_out_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_vfe_out_config         *vfe_out_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_isp_vfe_wm_config          *wm_config;
	struct cam_kmd_buf_info               *kmd_buf_info;
	struct cam_ife_hw_mgr_ctx             *ctx = NULL;
	struct cam_ife_hw_mgr_res             *ife_out_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    num_ent, rc = 0;

	ctx = prepare->ctxt_to_hw_map;

	if (prepare->num_hw_update_entries + 1 >=
			prepare->max_hw_update_entries) {
		CAM_ERR(CAM_ISP, "Insufficient HW entries :%d %d",
			prepare->num_hw_update_entries,
			prepare->max_hw_update_entries);
		return -EINVAL;
	}

	kmd_buf_info = blob_info->kmd_buf_info;
	for (i = 0; i < vfe_out_config->num_ports; i++) {
		wm_config = &vfe_out_config->wm_config[i];
		res_id_out = wm_config->port_type & 0xFF;

		CAM_DBG(CAM_ISP, "VFE out config idx: %d port: 0x%x",
			i, wm_config->port_type);

		if (res_id_out >= CAM_IFE_HW_OUT_RES_MAX) {
			CAM_ERR(CAM_ISP, "Invalid out port:0x%x",
				wm_config->port_type);
			return -EINVAL;
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
		ife_out_res = &ctx->res_list_ife_out[res_id_out];

		rc = cam_isp_add_cmd_buf_update(
			ife_out_res, blob_type,
			blob_type_hw_cmd_map[blob_type],
			blob_info->base_info->idx,
			(void *)cmd_buf_addr,
			kmd_buf_remain_size,
			(void *)wm_config,
			&bytes_used);
		if (rc < 0) {
			CAM_ERR(CAM_ISP,
				"Failed to update VFE Out out_type:0x%X base_idx:%d bytes_used:%u rc:%d",
				wm_config->port_type, blob_info->base_info->idx,
				bytes_used, rc);
			return rc;
		}

		total_used_bytes += bytes_used;
	}

	if (total_used_bytes) {
		num_ent = prepare->num_hw_update_entries;
		prepare->hw_update_entries[num_ent].handle =
			kmd_buf_info->handle;
		prepare->hw_update_entries[num_ent].len = total_used_bytes;
		prepare->hw_update_entries[num_ent].offset =
			kmd_buf_info->offset;
		num_ent++;

		kmd_buf_info->used_bytes += total_used_bytes;
		kmd_buf_info->offset     += total_used_bytes;
		prepare->num_hw_update_entries = num_ent;
	}

	return rc;
}

static int cam_isp_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	int rc = 0;
	struct cam_isp_generic_blob_info  *blob_info = user_data;
	struct cam_hw_prepare_update_args *prepare = NULL;
	struct cam_ife_hw_mgr_ctx         *ife_mgr_ctx = NULL;

	if (!blob_data || (blob_size == 0) || !blob_info) {
		CAM_ERR(CAM_ISP, "Invalid args data %pK size %d info %pK",
			blob_data, blob_size, blob_info);
		return -EINVAL;
	}

	if (blob_type >= CAM_ISP_GENERIC_BLOB_TYPE_MAX) {
		CAM_ERR(CAM_ISP, "Invalid Blob Type %d Max %d", blob_type,
			CAM_ISP_GENERIC_BLOB_TYPE_MAX);
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

		if (hfr_config->num_ports > CAM_ISP_IFE_OUT_RES_MAX ||
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
			hfr_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "HFR Update Failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_CLOCK_CONFIG: {
		struct cam_isp_clock_config    *clock_config;

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

		rc = cam_isp_blob_clock_update(blob_type, blob_info,
			clock_config, prepare);
		if (rc)
			CAM_ERR(CAM_PERF, "Clock Update Failed, rc=%d", rc);
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
			(bw_config->usage_type >= CAM_IFE_HW_NUM_MAX)) {
			CAM_ERR(CAM_ISP, "Invalid inputs");
			return -EINVAL;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;

		memcpy(&prepare_hw_data->bw_config[bw_config->usage_type],
			bw_config, sizeof(prepare_hw_data->bw_config[0]));
		prepare_hw_data->bw_config_version = CAM_ISP_BW_CONFIG_V1;
		prepare_hw_data->bw_config_valid[bw_config->usage_type] = true;
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

		if (bw_config->num_paths > CAM_ISP_MAX_PER_PATH_VOTES) {
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
			(bw_config->usage_type >= CAM_IFE_HW_NUM_MAX)) {
			CAM_ERR(CAM_ISP, "Invalid inputs");
			return -EINVAL;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;

		memset(&prepare_hw_data->bw_config_v2[bw_config->usage_type],
			0, sizeof(
			prepare_hw_data->bw_config_v2[bw_config->usage_type]));
		bw_config_size = sizeof(struct cam_isp_bw_config_internal_v2) +
			((bw_config->num_paths - 1) *
			sizeof(struct cam_axi_per_path_bw_vote));
		memcpy(&prepare_hw_data->bw_config_v2[bw_config->usage_type],
			bw_config, bw_config_size);

		prepare_hw_data->bw_config_version = CAM_ISP_BW_CONFIG_V2;
		prepare_hw_data->bw_config_valid[bw_config->usage_type] = true;
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

		if (vfe_out_config->num_ports > CAM_IFE_HW_OUT_RES_MAX ||
			vfe_out_config->num_ports == 0) {
			CAM_ERR(CAM_ISP,
				"Invalid num_ports:%u in vfe out config",
				vfe_out_config->num_ports,
				CAM_IFE_HW_OUT_RES_MAX);
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
			vfe_out_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "VFE out update failed rc: %d", rc);
	}
		break;

	default:
		CAM_WARN(CAM_ISP, "Invalid blob type %d", blob_type);
		break;
	}

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
	bool                                     fill_fence = true;
	struct cam_isp_prepare_hw_update_data   *prepare_hw_data;

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

	memset(&prepare_hw_data->bw_config[0], 0x0,
		sizeof(prepare_hw_data->bw_config[0]) *
		CAM_IFE_HW_NUM_MAX);
	memset(&prepare_hw_data->bw_config_valid[0], 0x0,
		sizeof(prepare_hw_data->bw_config_valid[0]) *
		CAM_IFE_HW_NUM_MAX);

	for (i = 0; i < ctx->num_base; i++) {
		CAM_DBG(CAM_ISP, "process cmd buffer for device %d", i);

		/* Add change base */
		rc = cam_isp_add_change_base(prepare, &ctx->res_list_ife_src,
			ctx->base[i].idx, &kmd_buf);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed in change base i=%d, idx=%d, rc=%d",
				i, ctx->base[i].idx, rc);
			goto end;
		}


		/* get command buffers */
		if (ctx->base[i].split_id != CAM_ISP_HW_SPLIT_MAX) {
			rc = cam_isp_add_command_buffers(prepare, &kmd_buf,
				&ctx->base[i],
				cam_isp_packet_generic_blob_handler,
				ctx->res_list_ife_out, CAM_IFE_HW_OUT_RES_MAX);
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Failed in add cmdbuf, i=%d, split_id=%d, rc=%d",
					i, ctx->base[i].split_id, rc);
				goto end;
			}
		}

		/* get IO buffers */
		rc = cam_isp_add_io_buffers(hw_mgr->mgr_common.img_iommu_hdl,
			hw_mgr->mgr_common.img_iommu_hdl_secure,
			prepare, ctx->base[i].idx,
			&kmd_buf, ctx->res_list_ife_out,
			&ctx->res_list_ife_in_rd,
			CAM_IFE_HW_OUT_RES_MAX, fill_fence);

		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed in io buffers, i=%d, rc=%d",
				i, rc);
			goto end;
		}

		/* fence map table entries need to fill only once in the loop */
		if (fill_fence)
			fill_fence = false;
	}

	/*
	 * reg update will be done later for the initial configure.
	 * need to plus one to the op_code and only take the lower
	 * bits to get the type of operation since UMD definition
	 * of op_code has some difference from KMD.
	 */
	if (((prepare->packet->header.op_code + 1) & 0xF) ==
		CAM_ISP_PACKET_INIT_DEV) {
		prepare_hw_data->packet_opcode_type = CAM_ISP_PACKET_INIT_DEV;
		if ((prepare->num_reg_dump_buf) && (prepare->num_reg_dump_buf <
			CAM_REG_DUMP_MAX_BUF_ENTRIES)) {
			ctx->num_reg_dump_buf = prepare->num_reg_dump_buf;
			memcpy(ctx->reg_dump_buf_desc,
				prepare->reg_dump_buf_desc,
				sizeof(struct cam_cmd_buf_desc) *
				prepare->num_reg_dump_buf);
		}

		goto end;
	} else {
		prepare_hw_data->packet_opcode_type = CAM_ISP_PACKET_UPDATE_DEV;
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
	for (i = 0; i < ctx->num_base; i++) {
		/* Add change base */
		rc = cam_isp_add_change_base(prepare, &ctx->res_list_ife_src,
			ctx->base[i].idx, &kmd_buf);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed in change base adding reg_update cmd i=%d, idx=%d, rc=%d",
				i, ctx->base[i].idx, rc);
			goto end;
		}

		/*Add reg update */
		rc = cam_isp_add_reg_update(prepare, &ctx->res_list_ife_src,
			ctx->base[i].idx, &kmd_buf);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Add Reg_update cmd Failed i=%d, idx=%d, rc=%d",
				i, ctx->base[i].idx, rc);
			goto end;
		}
	}

end:
	return rc;
}

static int cam_ife_mgr_resume_hw(struct cam_ife_hw_mgr_ctx *ctx)
{
	return cam_ife_mgr_bw_control(ctx, CAM_VFE_BW_CONTROL_INCLUDE);
}

static int cam_ife_mgr_sof_irq_debug(
	struct cam_ife_hw_mgr_ctx *ctx,
	uint32_t sof_irq_enable)
{
	int rc = 0;
	uint32_t i = 0;
	struct cam_ife_hw_mgr_res     *hw_mgr_res = NULL;
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

static void cam_ife_mgr_print_io_bufs(struct cam_packet *packet,
	int32_t iommu_hdl, int32_t sec_mmu_hdl, uint32_t pf_buf_info,
	bool *mem_found)
{
	dma_addr_t   iova_addr;
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

			if (pf_buf_info &&
				GET_FD_FROM_HANDLE(io_cfg[i].mem_handle[j]) ==
				GET_FD_FROM_HANDLE(pf_buf_info)) {
				CAM_INFO(CAM_ISP,
					"Found PF at port: 0x%x mem 0x%x fd: 0x%x",
					io_cfg[i].resource_type,
					io_cfg[i].mem_handle[j],
					pf_buf_info);
				if (mem_found)
					*mem_found = true;
			}

			CAM_INFO(CAM_ISP, "port: 0x%x f: %u format: %d dir %d",
				io_cfg[i].resource_type,
				io_cfg[i].fence,
				io_cfg[i].format,
				io_cfg[i].direction);

			mmu_hdl = cam_mem_is_secure_buf(
				io_cfg[i].mem_handle[j]) ? sec_mmu_hdl :
				iommu_hdl;
			rc = cam_mem_get_io_buf(io_cfg[i].mem_handle[j],
				mmu_hdl, &iova_addr, &src_buf_size);
			if (rc < 0) {
				CAM_ERR(CAM_ISP,
					"get src buf address fail mem_handle 0x%x",
					io_cfg[i].mem_handle[j]);
				continue;
			}
			if ((iova_addr & 0xFFFFFFFF) != iova_addr) {
				CAM_ERR(CAM_ISP, "Invalid mapped address");
				rc = -EINVAL;
				continue;
			}

			CAM_INFO(CAM_ISP,
				"pln %d w %d h %d s %u size 0x%x addr 0x%x end_addr 0x%x offset %x memh %x",
				j, io_cfg[i].planes[j].width,
				io_cfg[i].planes[j].height,
				io_cfg[i].planes[j].plane_stride,
				(unsigned int)src_buf_size,
				(unsigned int)iova_addr,
				(unsigned int)iova_addr +
				(unsigned int)src_buf_size,
				io_cfg[i].offsets[j],
				io_cfg[i].mem_handle[j]);
		}
	}
}

static int cam_ife_mgr_cmd(void *hw_mgr_priv, void *cmd_args)
{
	int rc = 0;
	struct cam_hw_cmd_args *hw_cmd_args = cmd_args;
	struct cam_ife_hw_mgr  *hw_mgr = hw_mgr_priv;
	struct cam_ife_hw_mgr_ctx *ctx = (struct cam_ife_hw_mgr_ctx *)
		hw_cmd_args->ctxt_to_hw_map;
	struct cam_isp_hw_cmd_args *isp_hw_cmd_args = NULL;

	if (!hw_mgr_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	if (!ctx || !ctx->ctx_in_use) {
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
			if (ctx->is_fe_enable)
				isp_hw_cmd_args->u.ctx_type = CAM_ISP_CTX_FS2;
			else if (ctx->is_rdi_only_context)
				isp_hw_cmd_args->u.ctx_type = CAM_ISP_CTX_RDI;
			else
				isp_hw_cmd_args->u.ctx_type = CAM_ISP_CTX_PIX;
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid HW mgr command:0x%x",
				hw_cmd_args->cmd_type);
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_HW_MGR_CMD_DUMP_PF_INFO:
		cam_ife_mgr_print_io_bufs(
			hw_cmd_args->u.pf_args.pf_data.packet,
			hw_mgr->mgr_common.img_iommu_hdl,
			hw_mgr->mgr_common.img_iommu_hdl_secure,
			hw_cmd_args->u.pf_args.buf_info,
			hw_cmd_args->u.pf_args.mem_found);
		break;
	case CAM_HW_MGR_CMD_REG_DUMP_ON_FLUSH:
		if (ctx->last_dump_flush_req_id == ctx->applied_req_id)
			return 0;

		rc = wait_for_completion_timeout(
			&ctx->config_done_complete,
			msecs_to_jiffies(30));
		if (rc <= 0) {
			CAM_ERR(CAM_ISP,
				"config done completion timeout, Reg dump will be unreliable rc=%d ctx_index %d",
				rc, ctx->ctx_index);
			rc = 0;
		}

		ctx->last_dump_flush_req_id = ctx->applied_req_id;
		rc = cam_ife_mgr_handle_reg_dump(ctx, ctx->reg_dump_buf_desc,
			ctx->num_reg_dump_buf,
			CAM_ISP_PACKET_META_REG_DUMP_ON_FLUSH);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Reg dump on flush failed req id: %llu rc: %d",
				ctx->applied_req_id, rc);
			return rc;
		}

		break;
	case CAM_HW_MGR_CMD_REG_DUMP_ON_ERROR:
		if (ctx->last_dump_err_req_id == ctx->applied_req_id)
			return 0;

		ctx->last_dump_err_req_id = ctx->applied_req_id;
		rc = cam_ife_mgr_handle_reg_dump(ctx, ctx->reg_dump_buf_desc,
			ctx->num_reg_dump_buf,
			CAM_ISP_PACKET_META_REG_DUMP_ON_ERROR);
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

static int cam_ife_mgr_cmd_get_sof_timestamp(
	struct cam_ife_hw_mgr_ctx            *ife_ctx,
	uint64_t                             *time_stamp,
	uint64_t                             *boot_time_stamp)
{
	int                                   rc = -EINVAL;
	uint32_t                              i;
	struct cam_ife_hw_mgr_res            *hw_mgr_res;
	struct cam_hw_intf                   *hw_intf;
	struct cam_csid_get_time_stamp_args   csid_get_time;

	hw_mgr_res = list_first_entry(&ife_ctx->res_list_ife_csid,
		struct cam_ife_hw_mgr_res, list);

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

static int cam_ife_mgr_process_recovery_cb(void *priv, void *data)
{
	int32_t rc = 0;
	struct cam_hw_event_recovery_data   *recovery_data = data;
	struct cam_hw_start_args             start_args;
	struct cam_hw_stop_args              stop_args;
	struct cam_ife_hw_mgr               *ife_hw_mgr = priv;
	struct cam_ife_hw_mgr_res           *hw_mgr_res;
	uint32_t                             i = 0;

	uint32_t error_type = recovery_data->error_type;
	struct cam_ife_hw_mgr_ctx        *ctx = NULL;

	/* Here recovery is performed */
	CAM_DBG(CAM_ISP, "ErrorType = %d", error_type);

	switch (error_type) {
	case CAM_ISP_HW_ERROR_OVERFLOW:
	case CAM_ISP_HW_ERROR_BUSIF_OVERFLOW:
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

		CAM_DBG(CAM_ISP, "RESET: CSID PATH");
		for (i = 0; i < recovery_data->no_of_context; i++) {
			ctx = recovery_data->affected_ctx[i];
			list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid,
				list) {
				rc = cam_ife_hw_mgr_reset_csid_res(hw_mgr_res);
				if (rc) {
					CAM_ERR(CAM_ISP, "Failed RESET (%d)",
						hw_mgr_res->res_id);
					return rc;
				}
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

	case CAM_ISP_HW_ERROR_VIOLATION:
		break;

	default:
		CAM_ERR(CAM_ISP, "Invalid Error");
	}
	CAM_DBG(CAM_ISP, "Exit: ErrorType = %d", error_type);

	kfree(recovery_data);
	return rc;
}

static int cam_ife_hw_mgr_do_error_recovery(
	struct cam_hw_event_recovery_data  *ife_mgr_recovery_data)
{
	int32_t                             rc = 0;
	struct crm_workq_task              *task = NULL;
	struct cam_hw_event_recovery_data  *recovery_data = NULL;

	recovery_data = kzalloc(sizeof(struct cam_hw_event_recovery_data),
		GFP_ATOMIC);
	if (!recovery_data)
		return -ENOMEM;

	memcpy(recovery_data, ife_mgr_recovery_data,
			sizeof(struct cam_hw_event_recovery_data));

	CAM_DBG(CAM_ISP, "Enter: error_type (%d)", recovery_data->error_type);

	task = cam_req_mgr_workq_get_task(g_ife_hw_mgr.workq);
	if (!task) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No empty task frame");
		kfree(recovery_data);
		return -ENOMEM;
	}

	task->process_cb = &cam_ife_mgr_process_recovery_cb;
	task->payload = recovery_data;
	rc = cam_req_mgr_workq_enqueue_task(task,
		recovery_data->affected_ctx[0]->hw_mgr,
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
	struct cam_isp_hw_error_event_data    *error_event_data,
	uint32_t                               curr_core_idx,
	struct cam_hw_event_recovery_data     *recovery_data)
{
	uint32_t affected_core[CAM_IFE_HW_NUM_MAX] = {0};
	struct cam_ife_hw_mgr_ctx   *ife_hwr_mgr_ctx = NULL;
	cam_hw_event_cb_func         notify_err_cb;
	struct cam_ife_hw_mgr       *ife_hwr_mgr = NULL;
	enum cam_isp_hw_event_type   event_type = CAM_ISP_HW_EVENT_ERROR;
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

		atomic_set(&ife_hwr_mgr_ctx->overflow_pending, 1);
		notify_err_cb = ife_hwr_mgr_ctx->common.event_cb[event_type];

		/* Add affected_context in list of recovery data */
		CAM_DBG(CAM_ISP, "Add affected ctx %d to list",
			ife_hwr_mgr_ctx->ctx_index);
		if (recovery_data->no_of_context < CAM_CTX_MAX)
			recovery_data->affected_ctx[
				recovery_data->no_of_context++] =
				ife_hwr_mgr_ctx;

		/*
		 * In the call back function corresponding ISP context
		 * will update CRM about fatal Error
		 */
		notify_err_cb(ife_hwr_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_ERROR, error_event_data);
	}

	/* fill the affected_core in recovery data */
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		recovery_data->affected_core[i] = affected_core[i];
		CAM_DBG(CAM_ISP, "Vfe core %d is affected (%d)",
			 i, recovery_data->affected_core[i]);
	}

	return 0;
}

static int cam_ife_hw_mgr_handle_hw_err(
	void                                *evt_info)
{
	struct cam_isp_hw_event_info        *event_info = evt_info;
	uint32_t core_idx;
	struct cam_isp_hw_error_event_data   error_event_data = {0};
	struct cam_hw_event_recovery_data    recovery_data = {0};
	int                                  rc = -EINVAL;

	if (event_info->err_type == CAM_VFE_IRQ_STATUS_VIOLATION)
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

	rc = cam_ife_hw_mgr_find_affected_ctx(&error_event_data,
		core_idx, &recovery_data);

	if (event_info->res_type == CAM_ISP_RESOURCE_VFE_OUT)
		return rc;

	if (g_ife_hw_mgr.debug_cfg.enable_recovery) {
		CAM_DBG(CAM_ISP, "IFE Mgr recovery is enabled");

		/* Trigger for recovery */
		if (event_info->err_type == CAM_VFE_IRQ_STATUS_VIOLATION)
			recovery_data.error_type = CAM_ISP_HW_ERROR_VIOLATION;
		else
			recovery_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;
		cam_ife_hw_mgr_do_error_recovery(&recovery_data);
	} else {
		CAM_DBG(CAM_ISP, "recovery is not enabled");
		rc = 0;
	}

	return rc;
}

static int cam_ife_hw_mgr_handle_hw_rup(
	void                                    *ctx,
	void                                    *evt_info)
{
	struct cam_isp_hw_event_info            *event_info = evt_info;
	struct cam_ife_hw_mgr_ctx               *ife_hw_mgr_ctx = ctx;
	cam_hw_event_cb_func                     ife_hwr_irq_rup_cb;
	struct cam_isp_hw_reg_update_event_data  rup_event_data;

	ife_hwr_irq_rup_cb =
		ife_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_REG_UPDATE];

	switch (event_info->res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
		if (ife_hw_mgr_ctx->is_dual)
			if (event_info->hw_idx != 1)
				break;

		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;
		ife_hwr_irq_rup_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_REG_UPDATE, &rup_event_data);
		break;

	case CAM_ISP_HW_VFE_IN_RDI0:
	case CAM_ISP_HW_VFE_IN_RDI1:
	case CAM_ISP_HW_VFE_IN_RDI2:
	case CAM_ISP_HW_VFE_IN_RDI3:
		if (!ife_hw_mgr_ctx->is_rdi_only_context)
			break;
		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;
		ife_hwr_irq_rup_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_REG_UPDATE, &rup_event_data);
		break;

	case CAM_ISP_HW_VFE_IN_PDLIB:
	case CAM_ISP_HW_VFE_IN_LCR:
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

static int cam_ife_hw_mgr_check_irq_for_dual_vfe(
	struct cam_ife_hw_mgr_ctx            *ife_hw_mgr_ctx,
	uint32_t                              hw_event_type)
{
	int32_t                               rc = -1;
	uint32_t                             *event_cnt = NULL;
	uint32_t                              core_idx0 = 0;
	uint32_t                              core_idx1 = 1;

	if (!ife_hw_mgr_ctx->is_dual)
		return 0;

	switch (hw_event_type) {
	case CAM_ISP_HW_EVENT_SOF:
		event_cnt = ife_hw_mgr_ctx->sof_cnt;
		break;
	case CAM_ISP_HW_EVENT_EPOCH:
		event_cnt = ife_hw_mgr_ctx->epoch_cnt;
		break;
	case CAM_ISP_HW_EVENT_EOF:
		event_cnt = ife_hw_mgr_ctx->eof_cnt;
		break;
	default:
		return 0;
	}

	if (event_cnt[core_idx0] == event_cnt[core_idx1]) {

		event_cnt[core_idx0] = 0;
		event_cnt[core_idx1] = 0;

		rc = 0;
		return rc;
	}

	if ((event_cnt[core_idx0] &&
		(event_cnt[core_idx0] - event_cnt[core_idx1] > 1)) ||
		(event_cnt[core_idx1] &&
		(event_cnt[core_idx1] - event_cnt[core_idx0] > 1))) {

		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"One of the VFE could not generate hw event %d core_0_cnt %d core_1_cnt %d",
			hw_event_type, event_cnt[core_idx0],
			event_cnt[core_idx1]);
		rc = -1;
		return rc;
	}

	CAM_DBG(CAM_ISP, "Only one core_index has given hw event %d",
			hw_event_type);

	return rc;
}

static int cam_ife_hw_mgr_handle_hw_epoch(
	void                                 *ctx,
	void                                 *evt_info)
{
	struct cam_isp_hw_event_info         *event_info = evt_info;
	struct cam_ife_hw_mgr_ctx            *ife_hw_mgr_ctx = ctx;
	cam_hw_event_cb_func                  ife_hw_irq_epoch_cb;
	struct cam_isp_hw_epoch_event_data    epoch_done_event_data;
	int                                   rc = 0;

	ife_hw_irq_epoch_cb =
		ife_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_EPOCH];

	switch (event_info->res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
		ife_hw_mgr_ctx->epoch_cnt[event_info->hw_idx]++;
		rc = cam_ife_hw_mgr_check_irq_for_dual_vfe(ife_hw_mgr_ctx,
			CAM_ISP_HW_EVENT_EPOCH);
		if (!rc) {
			if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
				break;
			ife_hw_irq_epoch_cb(ife_hw_mgr_ctx->common.cb_priv,
				CAM_ISP_HW_EVENT_EPOCH, &epoch_done_event_data);
		}
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
	void                                 *ctx,
	void                                 *evt_info)
{
	struct cam_isp_hw_event_info         *event_info = evt_info;
	struct cam_ife_hw_mgr_ctx            *ife_hw_mgr_ctx = ctx;
	cam_hw_event_cb_func                  ife_hw_irq_sof_cb;
	struct cam_isp_hw_sof_event_data      sof_done_event_data;
	int                                   rc = 0;

	ife_hw_irq_sof_cb =
		ife_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_SOF];

	switch (event_info->res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
	case CAM_ISP_HW_VFE_IN_RD:
		ife_hw_mgr_ctx->sof_cnt[event_info->hw_idx]++;
		rc = cam_ife_hw_mgr_check_irq_for_dual_vfe(ife_hw_mgr_ctx,
			CAM_ISP_HW_EVENT_SOF);
		if (!rc) {
			cam_ife_mgr_cmd_get_sof_timestamp(ife_hw_mgr_ctx,
				&sof_done_event_data.timestamp,
				&sof_done_event_data.boot_time);

			if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
				break;

			ife_hw_irq_sof_cb(ife_hw_mgr_ctx->common.cb_priv,
				CAM_ISP_HW_EVENT_SOF, &sof_done_event_data);
		}
		break;

	case CAM_ISP_HW_VFE_IN_RDI0:
	case CAM_ISP_HW_VFE_IN_RDI1:
	case CAM_ISP_HW_VFE_IN_RDI2:
	case CAM_ISP_HW_VFE_IN_RDI3:
		if (!ife_hw_mgr_ctx->is_rdi_only_context)
			break;
		cam_ife_mgr_cmd_get_sof_timestamp(ife_hw_mgr_ctx,
			&sof_done_event_data.timestamp,
			&sof_done_event_data.boot_time);
		if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
			break;
		ife_hw_irq_sof_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_SOF, &sof_done_event_data);
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
	void                                 *ctx,
	void                                 *evt_info)
{
	struct cam_isp_hw_event_info         *event_info = evt_info;
	struct cam_ife_hw_mgr_ctx            *ife_hw_mgr_ctx = ctx;
	cam_hw_event_cb_func                  ife_hw_irq_eof_cb;
	struct cam_isp_hw_eof_event_data      eof_done_event_data;
	int                                   rc = 0;

	ife_hw_irq_eof_cb =
		ife_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_EOF];

	switch (event_info->res_id) {
	case CAM_ISP_HW_VFE_IN_CAMIF:
		ife_hw_mgr_ctx->eof_cnt[event_info->hw_idx]++;
		rc = cam_ife_hw_mgr_check_irq_for_dual_vfe(ife_hw_mgr_ctx,
			CAM_ISP_HW_EVENT_EOF);
		if (!rc) {
			if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
				break;
			ife_hw_irq_eof_cb(ife_hw_mgr_ctx->common.cb_priv,
				CAM_ISP_HW_EVENT_EOF, &eof_done_event_data);
		}
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

static int cam_ife_hw_mgr_handle_hw_buf_done(
	void                                *ctx,
	void                                *evt_info)
{
	cam_hw_event_cb_func                 ife_hwr_irq_wm_done_cb;
	struct cam_ife_hw_mgr_ctx           *ife_hw_mgr_ctx = ctx;
	struct cam_isp_hw_done_event_data    buf_done_event_data = {0};
	struct cam_isp_hw_event_info        *event_info = evt_info;

	ife_hwr_irq_wm_done_cb =
		ife_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_DONE];

	buf_done_event_data.num_handles = 1;
	buf_done_event_data.resource_handle[0] = event_info->res_id;

	if (atomic_read(&ife_hw_mgr_ctx->overflow_pending))
		return 0;

	if (buf_done_event_data.num_handles > 0 && ife_hwr_irq_wm_done_cb) {
		CAM_DBG(CAM_ISP, "Notify ISP context");
		ife_hwr_irq_wm_done_cb(ife_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_DONE, &buf_done_event_data);
	}

	CAM_DBG(CAM_ISP, "Buf done for VFE:%d out_res->res_id: 0x%x",
		event_info->hw_idx, event_info->res_id);

	return 0;
}

static int cam_ife_hw_mgr_event_handler(
	void                                *priv,
	uint32_t                             evt_id,
	void                                *evt_info)
{
	int                                  rc = 0;

	if (!evt_info)
		return -EINVAL;

	if (!priv)
		if (evt_id != CAM_ISP_HW_EVENT_ERROR)
			return -EINVAL;

	CAM_DBG(CAM_ISP, "Event ID 0x%x", evt_id);

	switch (evt_id) {
	case CAM_ISP_HW_EVENT_SOF:
		rc = cam_ife_hw_mgr_handle_hw_sof(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_REG_UPDATE:
		rc = cam_ife_hw_mgr_handle_hw_rup(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_EPOCH:
		rc = cam_ife_hw_mgr_handle_hw_epoch(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_EOF:
		rc = cam_ife_hw_mgr_handle_hw_eof(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_DONE:
		rc = cam_ife_hw_mgr_handle_hw_buf_done(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_ERROR:
		rc = cam_ife_hw_mgr_handle_hw_err(evt_info);
		break;

	default:
		CAM_ERR(CAM_ISP, "Invalid event ID %d", evt_id);
		break;
	}

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
		if (ife_hw_mgr->csid_devices[i]->hw_ops.get_hw_caps) {
			ife_hw_mgr->csid_devices[i]->hw_ops.get_hw_caps(
				ife_hw_mgr->csid_devices[i]->hw_priv,
				&ife_hw_mgr->ife_csid_dev_caps[i],
				sizeof(ife_hw_mgr->ife_csid_dev_caps[i]));
		}
	}

	/* get caps for ife devices */
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (!ife_hw_mgr->ife_devices[i])
			continue;
		if (ife_hw_mgr->ife_devices[i]->hw_ops.get_hw_caps) {
			ife_hw_mgr->ife_devices[i]->hw_ops.get_hw_caps(
				ife_hw_mgr->ife_devices[i]->hw_priv,
				&ife_hw_mgr->ife_dev_caps[i],
				sizeof(ife_hw_mgr->ife_dev_caps[i]));
		}
	}

	return 0;
}

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

static int cam_ife_hw_mgr_debug_register(void)
{
	g_ife_hw_mgr.debug_cfg.dentry = debugfs_create_dir("camera_ife",
		NULL);

	if (!g_ife_hw_mgr.debug_cfg.dentry) {
		CAM_ERR(CAM_ISP, "failed to create dentry");
		return -ENOMEM;
	}

	if (!debugfs_create_file("ife_csid_debug",
		0644,
		g_ife_hw_mgr.debug_cfg.dentry, NULL,
		&cam_ife_csid_debug)) {
		CAM_ERR(CAM_ISP, "failed to create cam_ife_csid_debug");
		goto err;
	}

	if (!debugfs_create_u32("enable_recovery",
		0644,
		g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.enable_recovery)) {
		CAM_ERR(CAM_ISP, "failed to create enable_recovery");
		goto err;
	}

	if (!debugfs_create_bool("enable_req_dump",
		0644,
		g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.enable_req_dump)) {
		CAM_ERR(CAM_ISP, "failed to create enable_req_dump");
		goto err;
	}

	if (!debugfs_create_file("ife_camif_debug",
		0644,
		g_ife_hw_mgr.debug_cfg.dentry, NULL,
		&cam_ife_camif_debug)) {
		CAM_ERR(CAM_ISP, "failed to create cam_ife_camif_debug");
		goto err;
	}

	if (!debugfs_create_bool("per_req_reg_dump",
		0644,
		g_ife_hw_mgr.debug_cfg.dentry,
		&g_ife_hw_mgr.debug_cfg.per_req_reg_dump)) {
		CAM_ERR(CAM_ISP, "failed to create per_req_reg_dump entry");
		goto err;
	}

	g_ife_hw_mgr.debug_cfg.enable_recovery = 0;

	return 0;

err:
	debugfs_remove_recursive(g_ife_hw_mgr.debug_cfg.dentry);
	return -ENOMEM;
}

int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl)
{
	int rc = -EFAULT;
	int i, j;
	struct cam_iommu_handle cdm_handles;
	struct cam_ife_hw_mgr_ctx *ctx_pool;
	struct cam_ife_hw_mgr_res *res_list_ife_out;

	CAM_DBG(CAM_ISP, "Enter");

	memset(&g_ife_hw_mgr, 0, sizeof(g_ife_hw_mgr));

	mutex_init(&g_ife_hw_mgr.ctx_mutex);

	if (CAM_IFE_HW_NUM_MAX != CAM_IFE_CSID_HW_NUM_MAX) {
		CAM_ERR(CAM_ISP, "CSID num is different then IFE num");
		return -EINVAL;
	}

	/* fill ife hw intf information */
	for (i = 0, j = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		rc = cam_vfe_hw_init(&g_ife_hw_mgr.ife_devices[i], i);
		if (!rc) {
			struct cam_hw_info *vfe_hw =
				(struct cam_hw_info *)
				g_ife_hw_mgr.ife_devices[i]->hw_priv;
			struct cam_hw_soc_info *soc_info = &vfe_hw->soc_info;

			j++;

			g_ife_hw_mgr.cdm_reg_map[i] = &soc_info->reg_map[0];
			CAM_DBG(CAM_ISP,
				"reg_map: mem base = %pK cam_base = 0x%llx",
				(void __iomem *)soc_info->reg_map[0].mem_base,
				(uint64_t) soc_info->reg_map[0].mem_cam_base);
		} else {
			g_ife_hw_mgr.cdm_reg_map[i] = NULL;
		}
	}
	if (j == 0) {
		CAM_ERR(CAM_ISP, "no valid IFE HW");
		return -EINVAL;
	}

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
	if (cam_smmu_get_handle("ife",
		&g_ife_hw_mgr.mgr_common.img_iommu_hdl)) {
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

	if (!cam_cdm_get_iommu_handle("ife", &cdm_handles)) {
		CAM_DBG(CAM_ISP, "Successfully acquired the CDM iommu handles");
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl = cdm_handles.non_secure;
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl_secure =
			cdm_handles.secure;
	} else {
		CAM_DBG(CAM_ISP, "Failed to acquire the CDM iommu handles");
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl = -1;
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl_secure = -1;
	}

	atomic_set(&g_ife_hw_mgr.active_ctx_cnt, 0);
	for (i = 0; i < CAM_CTX_MAX; i++) {
		memset(&g_ife_hw_mgr.ctx_pool[i], 0,
			sizeof(g_ife_hw_mgr.ctx_pool[i]));
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].list);

		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_in.list);
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_cid);
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_csid);
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_src);
		INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].res_list_ife_in_rd);
		ctx_pool = &g_ife_hw_mgr.ctx_pool[i];
		for (j = 0; j < CAM_IFE_HW_OUT_RES_MAX; j++) {
			res_list_ife_out = &ctx_pool->res_list_ife_out[j];
			INIT_LIST_HEAD(&res_list_ife_out->list);
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

		g_ife_hw_mgr.ctx_pool[i].cdm_cmd =
			kzalloc(((sizeof(struct cam_cdm_bl_request)) +
				((CAM_IFE_HW_ENTRIES_MAX - 1) *
				 sizeof(struct cam_cdm_bl_cmd))), GFP_KERNEL);
		if (!g_ife_hw_mgr.ctx_pool[i].cdm_cmd) {
			rc = -ENOMEM;
			CAM_ERR(CAM_ISP, "Allocation Failed for cdm command");
			goto end;
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
			&g_ife_hw_mgr.workq, CRM_WORKQ_USAGE_NON_IRQ, 0);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "Unable to create worker");
		goto end;
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

	if (iommu_hdl)
		*iommu_hdl = g_ife_hw_mgr.mgr_common.img_iommu_hdl;

	cam_ife_hw_mgr_debug_register();
	CAM_DBG(CAM_ISP, "Exit");

	return 0;
end:
	if (rc) {
		for (i = 0; i < CAM_CTX_MAX; i++) {
			cam_tasklet_deinit(
				&g_ife_hw_mgr.mgr_common.tasklet_pool[i]);
			kfree(g_ife_hw_mgr.ctx_pool[i].cdm_cmd);
			g_ife_hw_mgr.ctx_pool[i].cdm_cmd = NULL;
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
