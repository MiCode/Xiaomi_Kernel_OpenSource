// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <media/cam_tfe.h>
#include "cam_smmu_api.h"
#include "cam_req_mgr_workq.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_tfe_csid_hw_intf.h"
#include "cam_tfe_hw_intf.h"
#include "cam_isp_packet_parser.h"
#include "cam_tfe_hw_mgr.h"
#include "cam_cdm_intf_api.h"
#include "cam_packet_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_mem_mgr_api.h"
#include "cam_common_util.h"
#include "cam_compat.h"
#include "cam_req_mgr_debug.h"
#include "cam_trace.h"

#define CAM_TFE_HW_ENTRIES_MAX  20
#define CAM_TFE_HW_CONFIG_TIMEOUT 60
#define CAM_TFE_HW_CONFIG_WAIT_MAX_TRY  3

#define TZ_SVC_SMMU_PROGRAM 0x15
#define TZ_SAFE_SYSCALL_ID  0x3
#define CAM_TFE_SAFE_DISABLE 0
#define CAM_TFE_SAFE_ENABLE 1
#define SMMU_SE_TFE 0


static struct cam_tfe_hw_mgr g_tfe_hw_mgr;

static int cam_tfe_hw_mgr_event_handler(
	void                                *priv,
	uint32_t                             evt_id,
	void                                *evt_info);

static int cam_tfe_mgr_regspace_data_cb(uint32_t reg_base_type,
	void *hw_mgr_ctx, struct cam_hw_soc_info **soc_info_ptr,
	uint32_t *reg_base_idx)
{
	int rc = 0;
	struct cam_isp_hw_mgr_res *hw_mgr_res;
	struct cam_hw_soc_info    *soc_info = NULL;
	struct cam_isp_resource_node       *res;
	struct cam_tfe_hw_mgr_ctx *ctx =
		(struct cam_tfe_hw_mgr_ctx *) hw_mgr_ctx;

	*soc_info_ptr = NULL;
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		if ((hw_mgr_res->res_id != CAM_ISP_HW_TFE_IN_CAMIF) &&
			!ctx->is_rdi_only_context)
			continue;

		switch (reg_base_type) {
		case CAM_REG_DUMP_BASE_TYPE_CAMNOC:
		case CAM_REG_DUMP_BASE_TYPE_ISP_LEFT:
			if (!hw_mgr_res->hw_res[CAM_ISP_HW_SPLIT_LEFT])
				continue;

			res = hw_mgr_res->hw_res[CAM_ISP_HW_SPLIT_LEFT];
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA,
				&soc_info, sizeof(void *));

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


			res = hw_mgr_res->hw_res[CAM_ISP_HW_SPLIT_RIGHT];
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_QUERY_REGSPACE_DATA,
				&soc_info, sizeof(void *));

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
				"Unrecognized reg base type: %d",
				reg_base_type);
			return -EINVAL;
		}
	}

	return rc;
}

static int cam_tfe_mgr_handle_reg_dump(struct cam_tfe_hw_mgr_ctx *ctx,
	struct cam_cmd_buf_desc *reg_dump_buf_desc, uint32_t num_reg_dump_buf,
	uint32_t meta_type,
	void *soc_dump_args,
	bool user_triggered_dump)
{
	int rc = -EINVAL, i;

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
				cam_tfe_mgr_regspace_data_cb,
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

static int cam_tfe_mgr_get_hw_caps(void *hw_mgr_priv,
	void *hw_caps_args)
{
	int rc = 0;
	int i;
	uint32_t num_dev = 0;
	struct cam_tfe_hw_mgr                  *hw_mgr = hw_mgr_priv;
	struct cam_query_cap_cmd               *query = hw_caps_args;
	struct cam_isp_tfe_query_cap_cmd        query_isp;

	CAM_DBG(CAM_ISP, "enter");

	if (copy_from_user(&query_isp,
		u64_to_user_ptr(query->caps_handle),
		sizeof(struct cam_isp_tfe_query_cap_cmd))) {
		rc = -EFAULT;
		return rc;
	}

	query_isp.device_iommu.non_secure = hw_mgr->mgr_common.img_iommu_hdl;
	query_isp.device_iommu.secure = hw_mgr->mgr_common.img_iommu_hdl_secure;
	query_isp.cdm_iommu.non_secure = hw_mgr->mgr_common.cmd_iommu_hdl;
	query_isp.cdm_iommu.secure = hw_mgr->mgr_common.cmd_iommu_hdl_secure;

	for (i = 0; i < CAM_TFE_CSID_HW_NUM_MAX; i++) {
		if (!hw_mgr->csid_devices[i])
			continue;

		query_isp.dev_caps[i].hw_type = CAM_ISP_TFE_HW_TFE;
		query_isp.dev_caps[i].hw_version.major = 5;
		query_isp.dev_caps[i].hw_version.minor = 3;
		query_isp.dev_caps[i].hw_version.incr = 0;

		/*
		 * device number is based on number of full tfe
		 * if pix is not supported, set reserve to 1
		 */
		if (hw_mgr->tfe_csid_dev_caps[i].num_pix) {
			query_isp.dev_caps[i].hw_version.reserved = 0;
			num_dev++;
		} else
			query_isp.dev_caps[i].hw_version.reserved = 1;
	}

	query_isp.num_dev = num_dev;

	if (copy_to_user(u64_to_user_ptr(query->caps_handle),
		&query_isp, sizeof(struct cam_isp_tfe_query_cap_cmd)))
		rc = -EFAULT;

	CAM_DBG(CAM_ISP, "exit rc :%d", rc);

	return rc;
}

static int cam_tfe_hw_mgr_is_rdi_res(uint32_t res_id)
{
	int rc = 0;

	switch (res_id) {
	case CAM_ISP_TFE_OUT_RES_RDI_0:
	case CAM_ISP_TFE_OUT_RES_RDI_1:
	case CAM_ISP_TFE_OUT_RES_RDI_2:
		rc = 1;
		break;
	default:
		break;
	}

	return rc;
}

static int cam_tfe_hw_mgr_convert_rdi_out_res_id_to_in_res(int res_id)
{
	if (res_id == CAM_ISP_TFE_OUT_RES_RDI_0)
		return CAM_ISP_HW_TFE_IN_RDI0;
	else if (res_id == CAM_ISP_TFE_OUT_RES_RDI_1)
		return CAM_ISP_HW_TFE_IN_RDI1;
	else if (res_id == CAM_ISP_TFE_OUT_RES_RDI_2)
		return CAM_ISP_HW_TFE_IN_RDI1;

	return CAM_ISP_HW_TFE_IN_MAX;
}

static int cam_tfe_hw_mgr_reset_csid_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res)
{
	int i;
	int rc = 0;
	struct cam_hw_intf      *hw_intf;
	struct cam_tfe_csid_reset_cfg_args  csid_reset_args;

	csid_reset_args.reset_type = CAM_TFE_CSID_RESET_PATH;

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
				sizeof(struct cam_tfe_csid_reset_cfg_args));
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

static int cam_tfe_hw_mgr_init_hw_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res)
{
	int i;
	int rc = -EINVAL;
	struct cam_hw_intf      *hw_intf;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		CAM_DBG(CAM_ISP, "hw type %d hw index:%d",
			hw_intf->hw_type, hw_intf->hw_idx);
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

static int cam_tfe_hw_mgr_start_hw_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res,
	struct cam_tfe_hw_mgr_ctx   *ctx)
{
	int i;
	int rc = -EINVAL;
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
			CAM_DBG(CAM_ISP, "Start hw type:%d HW idx %d Res %d",
				hw_intf->hw_type,
				hw_intf->hw_idx,
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

static void cam_tfe_hw_mgr_stop_hw_res(
	struct cam_isp_hw_mgr_res   *isp_hw_res)
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
			isp_hw_res->res_type == CAM_ISP_RESOURCE_TFE_OUT) {
			hw_intf->hw_ops.process_cmd(hw_intf->hw_priv,
				CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ,
				&dummy_args, sizeof(dummy_args));
		}
	}
}

static void cam_tfe_hw_mgr_deinit_hw_res(
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

static void cam_tfe_hw_mgr_deinit_hw(
	struct cam_tfe_hw_mgr_ctx *ctx)
{
	struct cam_isp_hw_mgr_res *hw_mgr_res;

	if (!ctx->init_done) {
		CAM_WARN(CAM_ISP, "ctx is not in init state");
		return;
	}

	/* Deinit TFE CSID hw */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		CAM_DBG(CAM_ISP, "Going to DeInit TFE CSID");
		cam_tfe_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deint TFE HW */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		cam_tfe_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	if (ctx->is_tpg)
		cam_tfe_hw_mgr_deinit_hw_res(&ctx->res_list_tpg);

	ctx->init_done = false;
}

static int cam_tfe_hw_mgr_init_hw(
	struct cam_tfe_hw_mgr_ctx *ctx)
{
	struct cam_isp_hw_mgr_res *hw_mgr_res;
	int rc = 0;

	if (ctx->is_tpg) {
		CAM_DBG(CAM_ISP, "INIT TPG ... in ctx id:%d",
			ctx->ctx_index);
		rc = cam_tfe_hw_mgr_init_hw_res(&ctx->res_list_tpg);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT TFE TPG(id :%d)",
				ctx->res_list_tpg.hw_res[0]->hw_intf->hw_idx);
			goto deinit;
		}
	}

	CAM_DBG(CAM_ISP, "INIT TFE csid ... in ctx id:%d",
		ctx->ctx_index);
	/* INIT TFE csid */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		rc = cam_tfe_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT TFE CSID(id :%d)",
				 hw_mgr_res->res_id);
			goto deinit;
		}
	}

	/* INIT TFE IN */
	CAM_DBG(CAM_ISP, "INIT TFE in resource ctx id:%d",
		ctx->ctx_index);
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		rc = cam_tfe_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT TFE SRC (%d)",
				 hw_mgr_res->res_id);
			goto deinit;
		}
	}

	return rc;
deinit:
	ctx->init_done = true;
	cam_tfe_hw_mgr_deinit_hw(ctx);
	return rc;
}

static int cam_tfe_hw_mgr_put_res(
	struct list_head                *src_list,
	struct cam_isp_hw_mgr_res      **res)
{
	struct cam_isp_hw_mgr_res *res_ptr  = NULL;

	res_ptr = *res;
	if (res_ptr)
		list_add_tail(&res_ptr->list, src_list);

	return 0;
}

static int cam_tfe_hw_mgr_get_res(
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
		CAM_ERR(CAM_ISP, "No more free tfe hw mgr ctx");
		rc = -EINVAL;
	}
	*res = res_ptr;

	return rc;
}

static int cam_tfe_hw_mgr_free_hw_res(
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

static int cam_tfe_mgr_csid_change_halt_mode(struct cam_tfe_hw_mgr_ctx *ctx,
	enum cam_tfe_csid_halt_mode halt_mode)
{
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_isp_resource_node     *isp_res;
	struct cam_tfe_csid_hw_halt_args halt;
	struct cam_hw_intf               *hw_intf;
	uint32_t i;
	int rc = 0;

	if (!ctx->is_dual)
		return 0;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (i == CAM_ISP_HW_SPLIT_LEFT)
				continue;

			if (!hw_mgr_res->hw_res[i] ||
				(hw_mgr_res->hw_res[i]->res_state !=
				CAM_ISP_RESOURCE_STATE_STREAMING))
				continue;

			isp_res = hw_mgr_res->hw_res[i];

			if ((isp_res->res_type == CAM_ISP_RESOURCE_PIX_PATH) &&
				(isp_res->res_id ==
					CAM_TFE_CSID_PATH_RES_IPP)) {
				hw_intf         = isp_res->hw_intf;
				halt.node_res   = isp_res;
				halt.halt_mode  = halt_mode;
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CSID_CHANGE_HALT_MODE,
					&halt,
					sizeof(
					struct cam_tfe_csid_hw_halt_args));
				if (rc)
					CAM_ERR(CAM_ISP, "Halt update failed");
				break;
			}
		}
	}

	return rc;
}

static int cam_tfe_mgr_csid_stop_hw(
	struct cam_tfe_hw_mgr_ctx *ctx, struct list_head  *stop_list,
		uint32_t  base_idx, uint32_t stop_cmd)
{
	struct cam_isp_hw_mgr_res      *hw_mgr_res;
	struct cam_isp_resource_node   *isp_res;
	struct cam_isp_resource_node   *stop_res[CAM_TFE_CSID_PATH_RES_MAX];
	struct cam_tfe_csid_hw_stop_args    stop;
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

static int cam_tfe_hw_mgr_release_hw_for_ctx(
	struct cam_tfe_hw_mgr_ctx  *tfe_ctx)
{
	uint32_t                          i;
	int rc = 0;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_isp_hw_mgr_res        *hw_mgr_res_temp;
	struct cam_hw_intf               *hw_intf;

	/* tfe out resource */
	for (i = 0; i < CAM_TFE_HW_OUT_RES_MAX; i++)
		cam_tfe_hw_mgr_free_hw_res(&tfe_ctx->res_list_tfe_out[i]);

	/* tfe in resource */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&tfe_ctx->res_list_tfe_in, list) {
		cam_tfe_hw_mgr_free_hw_res(hw_mgr_res);
		cam_tfe_hw_mgr_put_res(&tfe_ctx->free_res_list, &hw_mgr_res);
	}

	/* tfe csid resource */
	list_for_each_entry_safe(hw_mgr_res, hw_mgr_res_temp,
		&tfe_ctx->res_list_tfe_csid, list) {
		cam_tfe_hw_mgr_free_hw_res(hw_mgr_res);
		cam_tfe_hw_mgr_put_res(&tfe_ctx->free_res_list, &hw_mgr_res);
	}

	/* release tpg resource */
	if (tfe_ctx->is_tpg) {
		hw_intf = tfe_ctx->res_list_tpg.hw_res[0]->hw_intf;
		if (hw_intf->hw_ops.release) {
			rc = hw_intf->hw_ops.release(hw_intf->hw_priv,
				tfe_ctx->res_list_tpg.hw_res[0],
				sizeof(struct cam_isp_resource_node));
			if (rc)
				CAM_ERR(CAM_ISP,
					"TPG Release hw failed");
			tfe_ctx->res_list_tpg.hw_res[0] = NULL;
		} else
			CAM_ERR(CAM_ISP, "TPG resource Release null");
	}

	/* clean up the callback function */
	tfe_ctx->common.cb_priv = NULL;
	memset(tfe_ctx->common.event_cb, 0, sizeof(tfe_ctx->common.event_cb));

	CAM_DBG(CAM_ISP, "release context completed ctx id:%d",
		tfe_ctx->ctx_index);

	return 0;
}


static int cam_tfe_hw_mgr_put_ctx(
	struct list_head                 *src_list,
	struct cam_tfe_hw_mgr_ctx       **tfe_ctx)
{
	struct cam_tfe_hw_mgr_ctx *ctx_ptr  = NULL;

	mutex_lock(&g_tfe_hw_mgr.ctx_mutex);
	ctx_ptr = *tfe_ctx;
	if (ctx_ptr)
		list_add_tail(&ctx_ptr->list, src_list);
	*tfe_ctx = NULL;
	mutex_unlock(&g_tfe_hw_mgr.ctx_mutex);
	return 0;
}

static int cam_tfe_hw_mgr_get_ctx(
	struct list_head                *src_list,
	struct cam_tfe_hw_mgr_ctx       **tfe_ctx)
{
	int rc                              = 0;
	struct cam_tfe_hw_mgr_ctx *ctx_ptr  = NULL;

	mutex_lock(&g_tfe_hw_mgr.ctx_mutex);
	if (!list_empty(src_list)) {
		ctx_ptr = list_first_entry(src_list,
			struct cam_tfe_hw_mgr_ctx, list);
		list_del_init(&ctx_ptr->list);
	} else {
		CAM_ERR(CAM_ISP, "No more free tfe hw mgr ctx");
		rc = -EINVAL;
	}
	*tfe_ctx = ctx_ptr;
	mutex_unlock(&g_tfe_hw_mgr.ctx_mutex);

	return rc;
}

static void cam_tfe_hw_mgr_dump_all_ctx(void)
{
	uint32_t i;
	struct cam_tfe_hw_mgr_ctx       *ctx;
	struct cam_isp_hw_mgr_res       *hw_mgr_res;

	mutex_lock(&g_tfe_hw_mgr.ctx_mutex);
	list_for_each_entry(ctx, &g_tfe_hw_mgr.used_ctx_list, list) {
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"ctx id:%d is_dual:%d is_tpg:%d num_base:%d rdi only:%d",
			ctx->ctx_index, ctx->is_dual, ctx->is_tpg,
			ctx->num_base, ctx->is_rdi_only_context);

		if (ctx->res_list_tpg.res_type == CAM_ISP_RESOURCE_TPG) {
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"Acquired TPG HW:%d",
				ctx->res_list_tpg.hw_res[0]->hw_intf->hw_idx);
		}

		list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid,
			list) {
			for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
				if (!hw_mgr_res->hw_res[i])
					continue;

				CAM_INFO_RATE_LIMIT(CAM_ISP,
					"csid:%d res_type:%d res_id:%d res_state:%d",
					hw_mgr_res->hw_res[i]->hw_intf->hw_idx,
					hw_mgr_res->hw_res[i]->res_type,
					hw_mgr_res->hw_res[i]->res_id,
					hw_mgr_res->hw_res[i]->res_state);
			}
		}

		list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in,
			list) {
			for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
				if (!hw_mgr_res->hw_res[i])
					continue;

				CAM_INFO_RATE_LIMIT(CAM_ISP,
					"TFE IN:%d res_type:%d res_id:%d res_state:%d",
					hw_mgr_res->hw_res[i]->hw_intf->hw_idx,
					hw_mgr_res->hw_res[i]->res_type,
					hw_mgr_res->hw_res[i]->res_id,
					hw_mgr_res->hw_res[i]->res_state);
			}
		}
	}
	mutex_unlock(&g_tfe_hw_mgr.ctx_mutex);

}

static void cam_tfe_mgr_add_base_info(
	struct cam_tfe_hw_mgr_ctx       *ctx,
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

static int cam_tfe_mgr_process_base_info(
	struct cam_tfe_hw_mgr_ctx        *ctx)
{
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_isp_resource_node     *res = NULL;
	uint32_t i;

	if (list_empty(&ctx->res_list_tfe_in)) {
		CAM_ERR(CAM_ISP, "tfe in list empty");
		return -ENODEV;
	}

	/* TFE in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		if (hw_mgr_res->res_type == CAM_ISP_RESOURCE_UNINT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			res = hw_mgr_res->hw_res[i];
			cam_tfe_mgr_add_base_info(ctx, i,
					res->hw_intf->hw_idx);
			CAM_DBG(CAM_ISP, "add base info for hw %d",
				res->hw_intf->hw_idx);
		}
	}
	CAM_DBG(CAM_ISP, "ctx base num = %d", ctx->num_base);

	return 0;
}

static int cam_tfe_hw_mgr_acquire_res_tfe_out_rdi(
	struct cam_tfe_hw_mgr_ctx         *tfe_ctx,
	struct cam_isp_hw_mgr_res         *tfe_in_res,
	struct cam_isp_tfe_in_port_info   *in_port)
{
	int rc = -EINVAL;
	struct cam_tfe_acquire_args              tfe_acquire;
	struct cam_isp_tfe_out_port_info         *out_port = NULL;
	struct cam_isp_hw_mgr_res                *tfe_out_res;
	struct cam_hw_intf                       *hw_intf;
	uint32_t  i, tfe_out_res_id, tfe_in_res_id;

	/* take left resource */
	tfe_in_res_id = tfe_in_res->hw_res[0]->res_id;

	switch (tfe_in_res_id) {
	case CAM_ISP_HW_TFE_IN_RDI0:
		tfe_out_res_id = CAM_ISP_TFE_OUT_RES_RDI_0;
		break;
	case CAM_ISP_HW_TFE_IN_RDI1:
		tfe_out_res_id = CAM_ISP_TFE_OUT_RES_RDI_1;
		break;
	case CAM_ISP_HW_TFE_IN_RDI2:
		tfe_out_res_id = CAM_ISP_TFE_OUT_RES_RDI_2;
		break;
	default:
		CAM_ERR(CAM_ISP, "invalid resource type");
		goto err;
	}
	CAM_DBG(CAM_ISP, "tfe_in_res_id = %d, tfe_out_red_id = %d",
		tfe_in_res_id, tfe_out_res_id);

	tfe_acquire.rsrc_type = CAM_ISP_RESOURCE_TFE_OUT;
	tfe_acquire.tasklet = tfe_ctx->common.tasklet_info;

	tfe_out_res = &tfe_ctx->res_list_tfe_out[tfe_out_res_id & 0xFF];
	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];

		CAM_DBG(CAM_ISP, "i = %d, tfe_out_res_id = %d, out_port: %d",
			i, tfe_out_res_id, out_port->res_id);

		if (tfe_out_res_id != out_port->res_id)
			continue;

		tfe_acquire.tfe_out.cdm_ops = tfe_ctx->cdm_ops;
		tfe_acquire.priv = tfe_ctx;
		tfe_acquire.tfe_out.out_port_info = out_port;
		tfe_acquire.tfe_out.split_id = CAM_ISP_HW_SPLIT_LEFT;
		tfe_acquire.tfe_out.unique_id = tfe_ctx->ctx_index;
		tfe_acquire.tfe_out.is_dual = 0;
		tfe_acquire.event_cb = cam_tfe_hw_mgr_event_handler;
		hw_intf = tfe_in_res->hw_res[0]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&tfe_acquire,
			sizeof(struct cam_tfe_acquire_args));
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not acquire out resource 0x%x",
				 out_port->res_id);
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

	tfe_out_res->hw_res[0] = tfe_acquire.tfe_out.rsrc_node;
	tfe_out_res->is_dual_isp = 0;
	tfe_out_res->res_id = tfe_out_res_id;
	tfe_out_res->res_type = CAM_ISP_RESOURCE_TFE_OUT;
	tfe_in_res->num_children++;

	return 0;
err:
	return rc;
}

static int cam_tfe_hw_mgr_acquire_res_tfe_out_pixel(
	struct cam_tfe_hw_mgr_ctx           *tfe_ctx,
	struct cam_isp_hw_mgr_res           *tfe_in_res,
	struct cam_isp_tfe_in_port_info     *in_port)
{
	int rc = -EINVAL;
	uint32_t  i, j, k;
	struct cam_tfe_acquire_args               tfe_acquire;
	struct cam_isp_tfe_out_port_info          *out_port;
	struct cam_isp_hw_mgr_res                *tfe_out_res;
	struct cam_hw_intf                       *hw_intf;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		k = out_port->res_id & 0xFF;
		if (k >= CAM_TFE_HW_OUT_RES_MAX) {
			CAM_ERR(CAM_ISP, "invalid output resource type 0x%x",
				 out_port->res_id);
			continue;
		}

		if (cam_tfe_hw_mgr_is_rdi_res(out_port->res_id))
			continue;

		CAM_DBG(CAM_ISP, "res_type 0x%x", out_port->res_id);

		tfe_out_res = &tfe_ctx->res_list_tfe_out[k];
		tfe_out_res->is_dual_isp = in_port->usage_type;

		tfe_acquire.rsrc_type = CAM_ISP_RESOURCE_TFE_OUT;
		tfe_acquire.tasklet = tfe_ctx->common.tasklet_info;
		tfe_acquire.tfe_out.cdm_ops = tfe_ctx->cdm_ops;
		tfe_acquire.priv = tfe_ctx;
		tfe_acquire.tfe_out.out_port_info =  out_port;
		tfe_acquire.tfe_out.is_dual       = tfe_in_res->is_dual_isp;
		tfe_acquire.tfe_out.unique_id     = tfe_ctx->ctx_index;
		tfe_acquire.event_cb = cam_tfe_hw_mgr_event_handler;

		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!tfe_in_res->hw_res[j])
				continue;

			hw_intf = tfe_in_res->hw_res[j]->hw_intf;

			if (j == CAM_ISP_HW_SPLIT_LEFT) {
				tfe_acquire.tfe_out.split_id  =
					CAM_ISP_HW_SPLIT_LEFT;
				if (tfe_in_res->is_dual_isp)
					tfe_acquire.tfe_out.is_master   = 1;
				else
					tfe_acquire.tfe_out.is_master   = 0;
			} else {
				tfe_acquire.tfe_out.split_id  =
					CAM_ISP_HW_SPLIT_RIGHT;
				tfe_acquire.tfe_out.is_master       = 0;
			}

			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&tfe_acquire,
				sizeof(struct cam_tfe_acquire_args));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Can not acquire out resource 0x%x",
					out_port->res_id);
				goto err;
			}

			tfe_out_res->hw_res[j] =
				tfe_acquire.tfe_out.rsrc_node;
			CAM_DBG(CAM_ISP, "resource type :0x%x res id:0x%x",
				tfe_out_res->hw_res[j]->res_type,
				tfe_out_res->hw_res[j]->res_id);

		}
		tfe_out_res->res_type = CAM_ISP_RESOURCE_TFE_OUT;
		tfe_out_res->res_id = out_port->res_id;
		tfe_in_res->num_children++;
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_tfe_hw_mgr_acquire_res_tfe_out(
	struct cam_tfe_hw_mgr_ctx         *tfe_ctx,
	struct cam_isp_tfe_in_port_info   *in_port)
{
	int rc = -EINVAL;
	struct cam_isp_hw_mgr_res       *tfe_in_res;

	list_for_each_entry(tfe_in_res, &tfe_ctx->res_list_tfe_in, list) {
		if (tfe_in_res->num_children)
			continue;

		switch (tfe_in_res->res_id) {
		case CAM_ISP_HW_TFE_IN_CAMIF:
			rc = cam_tfe_hw_mgr_acquire_res_tfe_out_pixel(tfe_ctx,
				tfe_in_res, in_port);
			break;
		case CAM_ISP_HW_TFE_IN_RDI0:
		case CAM_ISP_HW_TFE_IN_RDI1:
		case CAM_ISP_HW_TFE_IN_RDI2:
			rc = cam_tfe_hw_mgr_acquire_res_tfe_out_rdi(tfe_ctx,
				tfe_in_res, in_port);
			break;
		default:
			CAM_ERR(CAM_ISP, "Unknown TFE SRC resource: %d",
				tfe_in_res->res_id);
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

static int cam_tfe_hw_mgr_acquire_res_tfe_in(
	struct cam_tfe_hw_mgr_ctx         *tfe_ctx,
	struct cam_isp_tfe_in_port_info   *in_port,
	uint32_t                          *pdaf_enable)
{
	int rc                = -EINVAL;
	int i;
	struct cam_isp_hw_mgr_res                  *csid_res;
	struct cam_isp_hw_mgr_res                  *tfe_src_res;
	struct cam_tfe_acquire_args                 tfe_acquire;
	struct cam_hw_intf                         *hw_intf;
	struct cam_tfe_hw_mgr                      *tfe_hw_mgr;

	tfe_hw_mgr = tfe_ctx->hw_mgr;

	list_for_each_entry(csid_res, &tfe_ctx->res_list_tfe_csid, list) {
		if (csid_res->num_children)
			continue;

		rc = cam_tfe_hw_mgr_get_res(&tfe_ctx->free_res_list,
			&tfe_src_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto err;
		}
		cam_tfe_hw_mgr_put_res(&tfe_ctx->res_list_tfe_in,
			&tfe_src_res);
		tfe_src_res->hw_res[0] = NULL;
		tfe_src_res->hw_res[1] = NULL;

		tfe_acquire.rsrc_type = CAM_ISP_RESOURCE_TFE_IN;
		tfe_acquire.tasklet = tfe_ctx->common.tasklet_info;
		tfe_acquire.tfe_in.cdm_ops = tfe_ctx->cdm_ops;
		tfe_acquire.tfe_in.in_port = in_port;
		tfe_acquire.tfe_in.camif_pd_enable = *pdaf_enable;
		tfe_acquire.priv = tfe_ctx;
		tfe_acquire.event_cb = cam_tfe_hw_mgr_event_handler;

		switch (csid_res->res_id) {
		case CAM_TFE_CSID_PATH_RES_IPP:
			tfe_acquire.tfe_in.res_id =
				CAM_ISP_HW_TFE_IN_CAMIF;

			if (csid_res->is_dual_isp)
				tfe_acquire.tfe_in.sync_mode =
				CAM_ISP_HW_SYNC_MASTER;
			else
				tfe_acquire.tfe_in.sync_mode =
				CAM_ISP_HW_SYNC_NONE;

			break;
		case CAM_TFE_CSID_PATH_RES_RDI_0:
			tfe_acquire.tfe_in.res_id = CAM_ISP_HW_TFE_IN_RDI0;
			tfe_acquire.tfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;
			break;
		case CAM_TFE_CSID_PATH_RES_RDI_1:
			tfe_acquire.tfe_in.res_id = CAM_ISP_HW_TFE_IN_RDI1;
			tfe_acquire.tfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;
			break;
		case CAM_TFE_CSID_PATH_RES_RDI_2:
			tfe_acquire.tfe_in.res_id = CAM_ISP_HW_TFE_IN_RDI2;
			tfe_acquire.tfe_in.sync_mode = CAM_ISP_HW_SYNC_NONE;
			break;
		default:
			CAM_ERR(CAM_ISP, "Wrong TFE CSID Resource Node");
			goto err;
		}
		tfe_src_res->res_type = tfe_acquire.rsrc_type;
		tfe_src_res->res_id = tfe_acquire.tfe_in.res_id;
		tfe_src_res->is_dual_isp = csid_res->is_dual_isp;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!csid_res->hw_res[i])
				continue;

			hw_intf = tfe_hw_mgr->tfe_devices[
				csid_res->hw_res[i]->hw_intf->hw_idx]->hw_intf;

			/* fill in more acquire information as needed */
			/* slave Camif resource, */
			if (i == CAM_ISP_HW_SPLIT_RIGHT &&
				tfe_src_res->is_dual_isp) {
				tfe_acquire.tfe_in.sync_mode =
				CAM_ISP_HW_SYNC_SLAVE;
				tfe_acquire.tfe_in.dual_tfe_sync_sel_idx =
					csid_res->hw_res[0]->hw_intf->hw_idx;
			} else if (i == CAM_ISP_HW_SPLIT_LEFT &&
				tfe_src_res->is_dual_isp)
				tfe_acquire.tfe_in.dual_tfe_sync_sel_idx =
					csid_res->hw_res[1]->hw_intf->hw_idx;

			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
					&tfe_acquire,
					sizeof(struct cam_tfe_acquire_args));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Can not acquire TFE HW res %d",
					csid_res->res_id);
				goto err;
			}
			tfe_src_res->hw_res[i] = tfe_acquire.tfe_in.rsrc_node;
			CAM_DBG(CAM_ISP,
				"acquire success TFE:%d  res type :0x%x res id:0x%x",
				hw_intf->hw_idx,
				tfe_src_res->hw_res[i]->res_type,
				tfe_src_res->hw_res[i]->res_id);

		}
		csid_res->num_children++;
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_tfe_hw_mgr_acquire_res_tfe_csid_pxl(
	struct cam_tfe_hw_mgr_ctx              *tfe_ctx,
	struct cam_isp_tfe_in_port_info        *in_port)
{
	int rc = -EINVAL;
	int i, j;
	uint32_t acquired_cnt = 0;
	struct cam_tfe_hw_mgr                        *tfe_hw_mgr;
	struct cam_isp_hw_mgr_res                    *csid_res;
	struct cam_hw_intf                           *hw_intf;
	struct cam_tfe_csid_hw_reserve_resource_args  csid_acquire;
	enum cam_tfe_csid_path_res_id                 path_res_id;
	struct cam_isp_hw_mgr_res        *csid_res_temp, *csid_res_iterator;
	struct cam_isp_tfe_out_port_info        *out_port = NULL;

	tfe_hw_mgr = tfe_ctx->hw_mgr;
	/* get csid resource */
	path_res_id = CAM_TFE_CSID_PATH_RES_IPP;

	rc = cam_tfe_hw_mgr_get_res(&tfe_ctx->free_res_list, &csid_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto end;
	}

	csid_res_temp = csid_res;

	csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
	csid_acquire.res_id = path_res_id;
	csid_acquire.in_port = in_port;
	csid_acquire.out_port = in_port->data;
	csid_acquire.node_res = NULL;
	csid_acquire.event_cb_prv = tfe_ctx;
	csid_acquire.event_cb = cam_tfe_hw_mgr_event_handler;
	if (in_port->num_out_res)
		out_port = &(in_port->data[0]);

	if (tfe_ctx->is_tpg) {
		if (tfe_ctx->res_list_tpg.hw_res[0]->hw_intf->hw_idx == 0)
			csid_acquire.phy_sel = CAM_ISP_TFE_IN_RES_PHY_0;
		else
			csid_acquire.phy_sel = CAM_ISP_TFE_IN_RES_PHY_1;
	}

	if (in_port->usage_type)
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_MASTER;
	else
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;

	/* Try acquiring CSID resource from previously acquired HW */
	list_for_each_entry(csid_res_iterator, &tfe_ctx->res_list_tfe_csid,
		list) {

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!csid_res_iterator->hw_res[i])
				continue;

			if (csid_res_iterator->is_secure == 1 ||
				(csid_res_iterator->is_secure == 0 &&
				in_port->num_out_res &&
				out_port->secure_mode == 1))
				continue;

			hw_intf = csid_res_iterator->hw_res[i]->hw_intf;
			csid_acquire.master_idx = hw_intf->hw_idx;

			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc) {
				CAM_DBG(CAM_ISP,
					"No tfe csid resource from hw %d",
					hw_intf->hw_idx);
				continue;
			}

			csid_res_temp->hw_res[acquired_cnt++] =
				csid_acquire.node_res;

			CAM_DBG(CAM_ISP,
				"acquired from old csid(%s)=%d CSID rsrc successfully",
				(i == 0) ? "left" : "right",
				hw_intf->hw_idx);

			if (in_port->usage_type && acquired_cnt == 1 &&
				path_res_id == CAM_TFE_CSID_PATH_RES_IPP)
				/*
				 * Continue to acquire Right for IPP.
				 * Dual TFE for RDI is not currently
				 * supported.
				 */
				continue;

			if (acquired_cnt)
				/*
				 * If successfully acquired CSID from
				 * previously acquired HW, skip the next
				 * part
				 */
				goto acquire_successful;
		}
	}

	/*
	 * If successfully acquired CSID from
	 * previously acquired HW, skip the next
	 * part
	 */
	if (acquired_cnt)
		goto acquire_successful;

	/* Acquire Left if not already acquired */
	if (in_port->usage_type) {
		for (i = 0; i < CAM_TFE_CSID_HW_NUM_MAX; i++) {
			if (!tfe_hw_mgr->csid_devices[i])
				continue;

			hw_intf = tfe_hw_mgr->csid_devices[i];
			csid_acquire.master_idx = hw_intf->hw_idx;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc)
				continue;
			else {
				csid_res_temp->hw_res[acquired_cnt++] =
					csid_acquire.node_res;
				break;
			}
		}

		if (i == CAM_TFE_CSID_HW_NUM_MAX || !csid_acquire.node_res) {
			CAM_ERR(CAM_ISP,
				"Can not acquire left tfe csid path resource %d",
				path_res_id);
			goto put_res;
		}
	} else {
		for (i = (CAM_TFE_CSID_HW_NUM_MAX - 1); i >= 0; i--) {
			if (!tfe_hw_mgr->csid_devices[i])
				continue;

			hw_intf = tfe_hw_mgr->csid_devices[i];
			csid_acquire.master_idx = hw_intf->hw_idx;
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc)
				continue;
			else {
				csid_res_temp->hw_res[acquired_cnt++] =
					csid_acquire.node_res;
				break;
			}
		}

		if (i == -1 || !csid_acquire.node_res) {
			CAM_ERR(CAM_ISP,
				"Can not acquire tfe csid path resource %d",
				path_res_id);
			goto put_res;
		}
	}
acquire_successful:
	CAM_DBG(CAM_ISP, "CSID path left acquired success. is_dual %d",
		in_port->usage_type);

	csid_res_temp->res_type = CAM_ISP_RESOURCE_PIX_PATH;
	csid_res_temp->res_id = path_res_id;

	if (in_port->usage_type) {
		csid_res_temp->is_dual_isp = 1;
		tfe_ctx->is_dual = true;
		tfe_ctx->master_hw_idx =
			csid_res_temp->hw_res[0]->hw_intf->hw_idx;
	} else
		csid_res_temp->is_dual_isp = 0;

	if (in_port->num_out_res)
		csid_res_temp->is_secure = out_port->secure_mode;

	cam_tfe_hw_mgr_put_res(&tfe_ctx->res_list_tfe_csid, &csid_res);

	/*
	 * Acquire Right if not already acquired.
	 * Dual TFE for RDI is not currently supported.
	 */
	if (in_port->usage_type && (path_res_id == CAM_TFE_CSID_PATH_RES_IPP)
		&& (acquired_cnt == 1)) {
		memset(&csid_acquire, 0, sizeof(csid_acquire));
		csid_acquire.node_res = NULL;
		csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_acquire.res_id = path_res_id;
		csid_acquire.in_port = in_port;
		csid_acquire.master_idx =
			csid_res_temp->hw_res[0]->hw_intf->hw_idx;
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_SLAVE;
		csid_acquire.node_res = NULL;
		csid_acquire.out_port = in_port->data;
		csid_acquire.event_cb_prv = tfe_ctx;
		csid_acquire.event_cb = cam_tfe_hw_mgr_event_handler;

		if (tfe_ctx->is_tpg) {
			if (tfe_ctx->res_list_tpg.hw_res[0]->hw_intf->hw_idx
				== 0)
				csid_acquire.phy_sel = CAM_ISP_TFE_IN_RES_PHY_0;
			else
				csid_acquire.phy_sel = CAM_ISP_TFE_IN_RES_PHY_1;
		}

		for (j = 0; j < CAM_TFE_CSID_HW_NUM_MAX; j++) {
			if (!tfe_hw_mgr->csid_devices[j])
				continue;

			if (j == csid_res_temp->hw_res[0]->hw_intf->hw_idx)
				continue;

			hw_intf = tfe_hw_mgr->csid_devices[j];
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc)
				continue;
			else
				break;
		}

		if (j == CAM_TFE_CSID_HW_NUM_MAX) {
			CAM_ERR(CAM_ISP,
				"Can not acquire tfe csid pixel resource");
			goto end;
		}
		csid_res_temp->hw_res[1] = csid_acquire.node_res;
		tfe_ctx->slave_hw_idx =
			csid_res_temp->hw_res[1]->hw_intf->hw_idx;
		CAM_DBG(CAM_ISP, "CSID right acquired success is_dual %d",
			in_port->usage_type);
	}

	return 0;
put_res:
	cam_tfe_hw_mgr_put_res(&tfe_ctx->free_res_list, &csid_res);
end:
	return rc;
}

static int cam_tfe_hw_mgr_acquire_tpg(
	struct cam_tfe_hw_mgr_ctx               *tfe_ctx,
	struct cam_isp_tfe_in_port_info        **in_port,
	uint32_t                                 num_inport)
{
	int rc = -EINVAL;
	uint32_t i, j = 0;
	struct cam_tfe_hw_mgr                        *tfe_hw_mgr;
	struct cam_hw_intf                           *hw_intf;
	struct cam_top_tpg_ver1_reserve_args          tpg_reserve;

	tfe_hw_mgr = tfe_ctx->hw_mgr;

	for (i = 0; i < CAM_TOP_TPG_HW_NUM_MAX; i++) {
		if (!tfe_hw_mgr->tpg_devices[i])
			continue;

		hw_intf = tfe_hw_mgr->tpg_devices[i];
		tpg_reserve.num_inport = num_inport;
		tpg_reserve.node_res = NULL;
		for (j = 0; j < num_inport; j++)
			tpg_reserve.in_port[j] = in_port[j];

		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&tpg_reserve, sizeof(tpg_reserve));
		if (!rc)
			break;
	}

	if (i == CAM_TOP_TPG_HW_NUM_MAX || !tpg_reserve.node_res) {
		CAM_ERR(CAM_ISP, "Can not acquire tfe TPG");
		rc = -EINVAL;
		goto end;
	}

	tfe_ctx->res_list_tpg.res_type = CAM_ISP_RESOURCE_TPG;
	tfe_ctx->res_list_tpg.hw_res[0] = tpg_reserve.node_res;

end:
	return rc;
}

static enum cam_tfe_csid_path_res_id
	cam_tfe_hw_mgr_get_tfe_csid_rdi_res_type(
	uint32_t                 out_port_type)
{
	enum cam_tfe_csid_path_res_id path_id;

	CAM_DBG(CAM_ISP, "out_port_type %x", out_port_type);
	switch (out_port_type) {
	case CAM_ISP_TFE_OUT_RES_RDI_0:
		path_id = CAM_TFE_CSID_PATH_RES_RDI_0;
		break;
	case CAM_ISP_TFE_OUT_RES_RDI_1:
		path_id = CAM_TFE_CSID_PATH_RES_RDI_1;
		break;
	case CAM_ISP_TFE_OUT_RES_RDI_2:
		path_id = CAM_TFE_CSID_PATH_RES_RDI_2;
		break;
	default:
		path_id = CAM_TFE_CSID_PATH_RES_MAX;
		CAM_DBG(CAM_ISP, "maximum rdi type exceeded out_port_type:%d ",
			out_port_type);
		break;
	}

	CAM_DBG(CAM_ISP, "out_port %x path_id %d", out_port_type, path_id);

	return path_id;
}

static int cam_tfe_hw_mgr_acquire_res_tfe_csid_rdi(
	struct cam_tfe_hw_mgr_ctx         *tfe_ctx,
	struct cam_isp_tfe_in_port_info   *in_port)
{
	int rc = -EINVAL;
	int i, j;

	struct cam_tfe_hw_mgr               *tfe_hw_mgr;
	struct cam_isp_hw_mgr_res           *csid_res;
	struct cam_hw_intf                  *hw_intf;
	struct cam_isp_tfe_out_port_info    *out_port;
	struct cam_tfe_csid_hw_reserve_resource_args  csid_acquire;
	struct cam_isp_hw_mgr_res             *csid_res_iterator;
	enum cam_tfe_csid_path_res_id        path_res_id;

	tfe_hw_mgr = tfe_ctx->hw_mgr;

	for (j = 0; j < in_port->num_out_res; j++) {
		out_port = &in_port->data[j];
		path_res_id = cam_tfe_hw_mgr_get_tfe_csid_rdi_res_type(
			out_port->res_id);

		if (path_res_id == CAM_TFE_CSID_PATH_RES_MAX)
			continue;

		rc = cam_tfe_hw_mgr_get_res(&tfe_ctx->free_res_list, &csid_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto end;
		}

		memset(&csid_acquire, 0, sizeof(csid_acquire));
		csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_acquire.res_id = path_res_id;
		csid_acquire.in_port = in_port;
		csid_acquire.out_port = out_port;
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;
		csid_acquire.node_res = NULL;
		csid_acquire.event_cb = cam_tfe_hw_mgr_event_handler;
		csid_acquire.event_cb_prv = tfe_ctx;

		if (tfe_ctx->is_tpg) {
			if (tfe_ctx->res_list_tpg.hw_res[0]->hw_intf->hw_idx ==
				0)
				csid_acquire.phy_sel = CAM_ISP_TFE_IN_RES_PHY_0;
			else
				csid_acquire.phy_sel = CAM_ISP_TFE_IN_RES_PHY_1;
		}

		/* Try acquiring CSID resource from previously acquired HW */
		list_for_each_entry(csid_res_iterator,
			&tfe_ctx->res_list_tfe_csid, list) {

			for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
				if (!csid_res_iterator->hw_res[i])
					continue;

				if (csid_res_iterator->is_secure == 1 ||
					(csid_res_iterator->is_secure == 0 &&
					in_port->num_out_res &&
					out_port->secure_mode == 1))
					continue;

				hw_intf = csid_res_iterator->hw_res[i]->hw_intf;

				rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
					&csid_acquire, sizeof(csid_acquire));
				if (rc) {
					CAM_DBG(CAM_ISP,
						"No tfe csid resource from hw %d",
						hw_intf->hw_idx);
					continue;
				}

				if (csid_acquire.node_res == NULL) {
					CAM_ERR(CAM_ISP,
						"Acquire RDI:%d rsrc failed",
						path_res_id);
					goto put_res;
				}

				csid_res->hw_res[0] = csid_acquire.node_res;

				CAM_DBG(CAM_ISP,
					"acquired from old csid(%s)=%d CSID rsrc successfully",
					(i == 0) ? "left" : "right",
					hw_intf->hw_idx);
				/*
				 * If successfully acquired CSID from
				 * previously acquired HW, skip the next
				 * part
				 */
				goto acquire_successful;
			}
		}

		/* Acquire if not already acquired */
		if (tfe_ctx->is_dual) {
			for (i = 0; i < CAM_TFE_CSID_HW_NUM_MAX; i++) {
				if (!tfe_hw_mgr->csid_devices[i])
					continue;

				hw_intf = tfe_hw_mgr->csid_devices[i];
				rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
					&csid_acquire, sizeof(csid_acquire));
				if (rc)
					continue;
				else {
					csid_res->hw_res[0] =
						csid_acquire.node_res;
					break;
				}
			}

			if (i == CAM_TFE_CSID_HW_NUM_MAX ||
				!csid_acquire.node_res) {
				CAM_ERR(CAM_ISP,
					"Can not acquire tfe csid rdi path%d",
					path_res_id);

				rc = -EINVAL;
				goto put_res;
			}
		} else {
			for (i = CAM_TFE_CSID_HW_NUM_MAX - 1; i >= 0; i--) {
				if (!tfe_hw_mgr->csid_devices[i])
					continue;

				hw_intf = tfe_hw_mgr->csid_devices[i];
				rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
					&csid_acquire, sizeof(csid_acquire));
				if (rc)
					continue;
				else {
					csid_res->hw_res[0] =
						csid_acquire.node_res;
					break;
				}
			}

			if (i == -1 || !csid_acquire.node_res) {
				CAM_ERR(CAM_ISP,
					"Can not acquire tfe csid rdi path %d",
					path_res_id);

				rc = -EINVAL;
				goto put_res;
			}
		}

acquire_successful:
		CAM_DBG(CAM_ISP, "CSID path :%d acquired success", path_res_id);
		csid_res->res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_res->res_id = path_res_id;
		csid_res->hw_res[1] = NULL;
		csid_res->is_dual_isp = 0;

		if (in_port->num_out_res)
			csid_res->is_secure = out_port->secure_mode;

		cam_tfe_hw_mgr_put_res(&tfe_ctx->res_list_tfe_csid, &csid_res);
	}

	return 0;
put_res:
	cam_tfe_hw_mgr_put_res(&tfe_ctx->free_res_list, &csid_res);
end:
	return rc;
}

static int cam_tfe_hw_mgr_preprocess_port(
	struct cam_tfe_hw_mgr_ctx       *tfe_ctx,
	struct cam_isp_tfe_in_port_info *in_port,
	int                             *ipp_count,
	int                             *rdi_count,
	int                             *pdaf_enable)
{
	int ipp_num        = 0;
	int rdi_num        = 0;
	bool rdi2_enable   = false;
	uint32_t i;
	struct cam_isp_tfe_out_port_info      *out_port;
	struct cam_tfe_hw_mgr                 *tfe_hw_mgr;

	tfe_hw_mgr = tfe_ctx->hw_mgr;


	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		CAM_DBG(CAM_ISP, "out_res id %d", out_port->res_id);

		if (cam_tfe_hw_mgr_is_rdi_res(out_port->res_id)) {
			rdi_num++;
			if (out_port->res_id == CAM_ISP_TFE_OUT_RES_RDI_2)
				rdi2_enable = true;
		} else {
			ipp_num++;
			if (out_port->res_id == CAM_ISP_TFE_OUT_RES_PDAF)
				*pdaf_enable = 1;
		}
	}

	if (*pdaf_enable && rdi2_enable) {
		CAM_ERR(CAM_ISP, "invalid outports both RDI2 and PDAF enabled");
		return -EINVAL;
	}

	*ipp_count = ipp_num;
	*rdi_count = rdi_num;

	CAM_DBG(CAM_ISP, "rdi: %d ipp: %d pdaf:%d", rdi_num, ipp_num,
		*pdaf_enable);

	return 0;
}

static int cam_tfe_mgr_acquire_hw_for_ctx(
	struct cam_tfe_hw_mgr_ctx              *tfe_ctx,
	struct cam_isp_tfe_in_port_info        *in_port,
	uint32_t *num_pix_port, uint32_t  *num_rdi_port,
	uint32_t *pdaf_enable)
{
	int rc                                    = -EINVAL;
	int is_dual_isp                           = 0;
	int ipp_count                             = 0;
	int rdi_count                             = 0;

	is_dual_isp = in_port->usage_type;

	cam_tfe_hw_mgr_preprocess_port(tfe_ctx, in_port, &ipp_count,
		&rdi_count, pdaf_enable);

	if (!ipp_count && !rdi_count) {
		CAM_ERR(CAM_ISP,
			"No PIX or RDI");
		return -EINVAL;
	}

	if (ipp_count) {
		/* get tfe csid IPP resource */
		rc = cam_tfe_hw_mgr_acquire_res_tfe_csid_pxl(tfe_ctx,
			in_port);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire TFE CSID IPP resource Failed dual:%d",
				in_port->usage_type);
			goto err;
		}
	}

	if (rdi_count) {
		/* get tfe csid rdi resource */
		rc = cam_tfe_hw_mgr_acquire_res_tfe_csid_rdi(tfe_ctx, in_port);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire TFE CSID RDI resource Failed dual:%d",
				in_port->usage_type);
			goto err;
		}
	}

	rc = cam_tfe_hw_mgr_acquire_res_tfe_in(tfe_ctx, in_port, pdaf_enable);
	if (rc) {
		CAM_ERR(CAM_ISP,
		"Acquire TFE IN resource Failed dual:%d", in_port->usage_type);
		goto err;
	}

	CAM_DBG(CAM_ISP, "Acquiring TFE OUT resource...");
	rc = cam_tfe_hw_mgr_acquire_res_tfe_out(tfe_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire TFE OUT resource Failed dual:%d",
			in_port->usage_type);
		goto err;
	}

	*num_pix_port += ipp_count;
	*num_rdi_port += rdi_count;

	return 0;
err:
	/* release resource at the acquire entry funciton */
	return rc;
}

void cam_tfe_cam_cdm_callback(uint32_t handle, void *userdata,
	enum cam_cdm_cb_status status, uint64_t cookie)
{
	struct cam_isp_prepare_hw_update_data *hw_update_data = NULL;
	struct cam_tfe_hw_mgr_ctx *ctx = NULL;
	uint32_t *buf_start, *buf_end;
	int i, rc = 0;
	size_t len = 0;
	uint32_t *buf_addr;

	if (!userdata) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return;
	}

	if (status == CAM_CDM_CB_STATUS_BL_SUCCESS) {
		hw_update_data =
			(struct cam_isp_prepare_hw_update_data *)userdata;
		ctx =
		(struct cam_tfe_hw_mgr_ctx *)hw_update_data->isp_mgr_ctx;
		complete_all(&ctx->config_done_complete);
		atomic_set(&ctx->cdm_done, 1);
		ctx->last_cdm_done_req = cookie;
		if (g_tfe_hw_mgr.debug_cfg.per_req_reg_dump)
			cam_tfe_mgr_handle_reg_dump(ctx,
				hw_update_data->reg_dump_buf_desc,
				hw_update_data->num_reg_dump_buf,
				CAM_ISP_TFE_PACKET_META_REG_DUMP_PER_REQUEST,
				NULL, false);
		CAM_DBG(CAM_ISP,
			"Called by CDM hdl=%x, udata=%pK, status=%d, cookie=%llu ctx_index=%d",
			 handle, userdata, status, cookie, ctx->ctx_index);
	} else if (status == CAM_CDM_CB_STATUS_PAGEFAULT ||
		status == CAM_CDM_CB_STATUS_INVALID_BL_CMD ||
		status == CAM_CDM_CB_STATUS_HW_ERROR) {
		ctx = userdata;
		CAM_INFO(CAM_ISP,
			"req_id =%d ctx_id =%d Bl_cmd_count =%d status=%d",
			ctx->applied_req_id, ctx->ctx_index,
			ctx->last_submit_bl_cmd.bl_count, status);

		for (i = 0; i < ctx->last_submit_bl_cmd.bl_count; i++) {
			CAM_INFO(CAM_ISP,
				"BL(%d) hdl=0x%x addr=0x%x len=%d input_len =%d offset=0x%x type=%d",
				i, ctx->last_submit_bl_cmd.cmd[i].mem_handle,
				ctx->last_submit_bl_cmd.cmd[i].hw_addr,
				ctx->last_submit_bl_cmd.cmd[i].len,
				ctx->last_submit_bl_cmd.cmd[i].input_len,
				ctx->last_submit_bl_cmd.cmd[i].offset,
				ctx->last_submit_bl_cmd.cmd[i].type);

			rc = cam_packet_util_get_cmd_mem_addr(
				ctx->last_submit_bl_cmd.cmd[i].mem_handle,
				&buf_addr, &len);

			buf_start = (uint32_t *)((uint8_t *) buf_addr +
				ctx->last_submit_bl_cmd.cmd[i].offset);
			buf_end = (uint32_t *)((uint8_t *) buf_start +
				ctx->last_submit_bl_cmd.cmd[i].input_len - 1);

			cam_cdm_util_dump_cmd_buf(buf_start, buf_end);
		}
		if (ctx->packet != NULL)
			cam_packet_dump_patch_info(ctx->packet,
				g_tfe_hw_mgr.mgr_common.img_iommu_hdl,
				g_tfe_hw_mgr.mgr_common.img_iommu_hdl_secure);
	} else {
		CAM_WARN(CAM_ISP,
			"Called by CDM hdl=%x, udata=%pK, status=%d, cookie=%llu",
			 handle, userdata, status, cookie);
	}
}

/* entry function: acquire_hw */
static int cam_tfe_mgr_acquire_hw(void *hw_mgr_priv, void *acquire_hw_args)
{
	struct cam_tfe_hw_mgr *tfe_hw_mgr            = hw_mgr_priv;
	struct cam_hw_acquire_args *acquire_args     = acquire_hw_args;
	int rc                                       = -EINVAL;
	int i, j;
	struct cam_tfe_hw_mgr_ctx          *tfe_ctx;
	struct cam_isp_tfe_in_port_info    *in_port = NULL;
	struct cam_cdm_acquire_data         cdm_acquire;
	uint32_t                            num_pix_port_per_in = 0;
	uint32_t                            num_rdi_port_per_in = 0;
	uint32_t                            pdaf_enable = 0;
	uint32_t                            total_pix_port = 0;
	uint32_t                            total_rdi_port = 0;
	uint32_t                            in_port_length = 0;
	uint32_t                            total_in_port_length = 0;
	struct cam_isp_tfe_acquire_hw_info *acquire_hw_info = NULL;
	struct cam_isp_tfe_in_port_info
		*tpg_inport[CAM_TOP_TPG_MAX_SUPPORTED_DT] = {0, 0, 0, 0};

	CAM_DBG(CAM_ISP, "Enter...");

	if (!acquire_args || acquire_args->num_acq <= 0) {
		CAM_ERR(CAM_ISP, "Nothing to acquire. Seems like error");
		return -EINVAL;
	}

	/* get the tfe ctx */
	rc = cam_tfe_hw_mgr_get_ctx(&tfe_hw_mgr->free_ctx_list, &tfe_ctx);
	if (rc || !tfe_ctx) {
		CAM_ERR(CAM_ISP, "Get tfe hw context failed");
		goto err;
	}

	tfe_ctx->common.cb_priv = acquire_args->context_data;
	for (i = 0; i < CAM_ISP_HW_EVENT_MAX; i++)
		tfe_ctx->common.event_cb[i] = acquire_args->event_cb;

	tfe_ctx->hw_mgr = tfe_hw_mgr;

	memcpy(cdm_acquire.identifier, "tfe", sizeof("tfe"));
	cdm_acquire.cell_index = 0;
	cdm_acquire.handle = 0;
	cdm_acquire.userdata = tfe_ctx;
	cdm_acquire.priority = CAM_CDM_BL_FIFO_0;
	cdm_acquire.base_array_cnt = CAM_TFE_HW_NUM_MAX;
	for (i = 0, j = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
		if (tfe_hw_mgr->cdm_reg_map[i])
			cdm_acquire.base_array[j++] =
				tfe_hw_mgr->cdm_reg_map[i];
	}
	cdm_acquire.base_array_cnt = j;

	cdm_acquire.id = CAM_CDM_VIRTUAL;
	cdm_acquire.cam_cdm_callback = cam_tfe_cam_cdm_callback;
	rc = cam_cdm_acquire(&cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to acquire the CDM HW");
		goto free_ctx;
	}

	CAM_DBG(CAM_ISP, "Successfully acquired the CDM HW hdl=%x",
		cdm_acquire.handle);
	tfe_ctx->cdm_handle = cdm_acquire.handle;
	tfe_ctx->cdm_ops = cdm_acquire.ops;
	atomic_set(&tfe_ctx->cdm_done, 1);
	tfe_ctx->last_cdm_done_req = 0;

	acquire_hw_info = (struct cam_isp_tfe_acquire_hw_info *)
		acquire_args->acquire_info;
	in_port = (struct cam_isp_tfe_in_port_info *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset);

	/* Check any inport has dual tfe usage  */
	tfe_ctx->is_dual = false;
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {
		if (in_port->usage_type)
			tfe_ctx->is_dual = true;

		in_port_length =
			sizeof(struct cam_isp_tfe_in_port_info) +
			(in_port->num_out_res - 1) *
			sizeof(struct cam_isp_tfe_out_port_info);
		total_in_port_length += in_port_length;
		if (total_in_port_length >
			acquire_hw_info->input_info_size) {
			CAM_ERR(CAM_ISP,
				"buffer size is not enough %d %d",
				total_in_port_length,
				acquire_hw_info->input_info_size);
			rc = -EINVAL;
			goto free_cdm;
		}

		in_port = (struct cam_isp_tfe_in_port_info *)
			((uint8_t *)in_port + in_port_length);
	}

	in_port_length = 0;
	total_in_port_length = 0;
	in_port = (struct cam_isp_tfe_in_port_info *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset);

	if (in_port->res_id == CAM_ISP_TFE_IN_RES_TPG) {
		if (acquire_hw_info->num_inputs >
			CAM_TOP_TPG_MAX_SUPPORTED_DT) {
			CAM_ERR(CAM_ISP, "too many number inport:%d for TPG ",
				acquire_hw_info->num_inputs);
			rc = -EINVAL;
			goto free_cdm;
		}

		for (i = 0; i < acquire_hw_info->num_inputs; i++) {
			if (in_port->res_id != CAM_ISP_TFE_IN_RES_TPG) {
				CAM_ERR(CAM_ISP, "Inval :%d inport res id:0x%x",
					i, in_port->res_id);
				rc = -EINVAL;
				goto free_cdm;
			}

			tpg_inport[i] = in_port;
			in_port_length =
				sizeof(struct cam_isp_tfe_in_port_info) +
				(in_port->num_out_res - 1) *
				sizeof(struct cam_isp_tfe_out_port_info);
			total_in_port_length += in_port_length;
			if (total_in_port_length >
				acquire_hw_info->input_info_size) {
				CAM_ERR(CAM_ISP,
					"buffer size is not enough %d %d",
					total_in_port_length,
					acquire_hw_info->input_info_size);
				rc = -EINVAL;
				goto free_cdm;
			}

			in_port = (struct cam_isp_tfe_in_port_info *)
				((uint8_t *)in_port + in_port_length);
		}

		rc = cam_tfe_hw_mgr_acquire_tpg(tfe_ctx, tpg_inport,
			acquire_hw_info->num_inputs);
		if (rc)
			goto free_cdm;

		tfe_ctx->is_tpg = true;
	}

	in_port = (struct cam_isp_tfe_in_port_info *)
		((uint8_t *)&acquire_hw_info->data +
		 acquire_hw_info->input_info_offset);
	in_port_length = 0;
	total_in_port_length = 0;

	/* acquire HW resources */
	for (i = 0; i < acquire_hw_info->num_inputs; i++) {

		if (in_port->num_out_res > CAM_TFE_HW_OUT_RES_MAX) {
			CAM_ERR(CAM_ISP, "too many output res %d",
				in_port->num_out_res);
			rc = -EINVAL;
			goto free_res;
		}

		in_port_length = sizeof(struct cam_isp_tfe_in_port_info) +
			(in_port->num_out_res - 1) *
			sizeof(struct cam_isp_tfe_out_port_info);
		total_in_port_length += in_port_length;

		if (total_in_port_length > acquire_hw_info->input_info_size) {
			CAM_ERR(CAM_ISP, "buffer size is not enough");
			rc = -EINVAL;
			goto free_res;
		}
		CAM_DBG(CAM_ISP, "in_res_id %x", in_port->res_id);
		rc = cam_tfe_mgr_acquire_hw_for_ctx(tfe_ctx, in_port,
			&num_pix_port_per_in, &num_rdi_port_per_in,
			&pdaf_enable);
		total_pix_port += num_pix_port_per_in;
		total_rdi_port += num_rdi_port_per_in;

		if (rc) {
			CAM_ERR(CAM_ISP, "can not acquire resource");
			goto free_res;
		}
		in_port = (struct cam_isp_tfe_in_port_info *)
			((uint8_t *)in_port + in_port_length);
	}

	/* Check whether context has only RDI resource */
	if (!total_pix_port) {
		tfe_ctx->is_rdi_only_context = 1;
		CAM_DBG(CAM_ISP, "RDI only context");
	} else
		tfe_ctx->is_rdi_only_context = 0;

	/* Process base info */
	rc = cam_tfe_mgr_process_base_info(tfe_ctx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Process base info failed");
		goto free_res;
	}

	acquire_args->ctxt_to_hw_map = tfe_ctx;
	tfe_ctx->ctx_in_use = 1;
	tfe_ctx->num_reg_dump_buf = 0;

	acquire_args->support_consumed_addr =
		g_tfe_hw_mgr.support_consumed_addr;

	cam_tfe_hw_mgr_put_ctx(&tfe_hw_mgr->used_ctx_list, &tfe_ctx);

	CAM_DBG(CAM_ISP, "Exit...(success)");

	return 0;
free_res:
	cam_tfe_hw_mgr_release_hw_for_ctx(tfe_ctx);
	tfe_ctx->ctx_in_use = 0;
	tfe_ctx->is_rdi_only_context = 0;
	tfe_ctx->cdm_handle = 0;
	tfe_ctx->cdm_ops = NULL;
	tfe_ctx->init_done = false;
	tfe_ctx->is_dual = false;
	tfe_ctx->is_tpg  = false;
	tfe_ctx->res_list_tpg.res_type = CAM_ISP_RESOURCE_MAX;
free_cdm:
	cam_cdm_release(tfe_ctx->cdm_handle);
free_ctx:
	cam_tfe_hw_mgr_put_ctx(&tfe_hw_mgr->free_ctx_list, &tfe_ctx);
err:
	/* Dump all the current acquired HW */
	cam_tfe_hw_mgr_dump_all_ctx();

	CAM_ERR_RATE_LIMIT(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

/* entry function: acquire_hw */
static int cam_tfe_mgr_acquire_dev(void *hw_mgr_priv, void *acquire_hw_args)
{
	struct cam_tfe_hw_mgr *tfe_hw_mgr            = hw_mgr_priv;
	struct cam_hw_acquire_args *acquire_args     = acquire_hw_args;
	int rc                                       = -EINVAL;
	int i, j;
	struct cam_tfe_hw_mgr_ctx         *tfe_ctx;
	struct cam_isp_tfe_in_port_info   *in_port = NULL;
	struct cam_isp_resource           *isp_resource = NULL;
	struct cam_cdm_acquire_data        cdm_acquire;
	uint32_t                           num_pix_port_per_in = 0;
	uint32_t                           num_rdi_port_per_in = 0;
	uint32_t                           pdad_enable         = 0;
	uint32_t                           total_pix_port = 0;
	uint32_t                           total_rdi_port = 0;
	uint32_t                           in_port_length = 0;

	CAM_DBG(CAM_ISP, "Enter...");

	if (!acquire_args || acquire_args->num_acq <= 0) {
		CAM_ERR(CAM_ISP, "Nothing to acquire. Seems like error");
		return -EINVAL;
	}

	/* get the tfe ctx */
	rc = cam_tfe_hw_mgr_get_ctx(&tfe_hw_mgr->free_ctx_list, &tfe_ctx);
	if (rc || !tfe_ctx) {
		CAM_ERR(CAM_ISP, "Get tfe hw context failed");
		goto err;
	}

	tfe_ctx->common.cb_priv = acquire_args->context_data;
	for (i = 0; i < CAM_ISP_HW_EVENT_MAX; i++)
		tfe_ctx->common.event_cb[i] = acquire_args->event_cb;

	tfe_ctx->hw_mgr = tfe_hw_mgr;

	memcpy(cdm_acquire.identifier, "tfe", sizeof("tfe"));
	cdm_acquire.cell_index = 0;
	cdm_acquire.handle = 0;
	cdm_acquire.userdata = tfe_ctx;
	cdm_acquire.base_array_cnt = CAM_TFE_HW_NUM_MAX;
	for (i = 0, j = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
		if (tfe_hw_mgr->cdm_reg_map[i])
			cdm_acquire.base_array[j++] =
				tfe_hw_mgr->cdm_reg_map[i];
	}
	cdm_acquire.base_array_cnt = j;


	cdm_acquire.id = CAM_CDM_VIRTUAL;
	cdm_acquire.cam_cdm_callback = cam_tfe_cam_cdm_callback;
	rc = cam_cdm_acquire(&cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed to acquire the CDM HW");
		goto free_ctx;
	}

	CAM_DBG(CAM_ISP, "Successfully acquired the CDM HW hdl=%x",
		cdm_acquire.handle);
	tfe_ctx->cdm_handle = cdm_acquire.handle;
	tfe_ctx->cdm_ops = cdm_acquire.ops;
	atomic_set(&tfe_ctx->cdm_done, 1);
	tfe_ctx->last_cdm_done_req = 0;

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

		in_port_length = sizeof(struct cam_isp_tfe_in_port_info);

		if (in_port_length > isp_resource[i].length) {
			CAM_ERR(CAM_ISP, "buffer size is not enough");
			rc = -EINVAL;
			goto free_res;
		}

		in_port = memdup_user(
			u64_to_user_ptr(isp_resource[i].res_hdl),
			isp_resource[i].length);
		if (!IS_ERR(in_port)) {
			if (in_port->num_out_res > CAM_TFE_HW_OUT_RES_MAX) {
				CAM_ERR(CAM_ISP, "too many output res %d",
					in_port->num_out_res);
				rc = -EINVAL;
				kfree(in_port);
				goto free_res;
			}

			in_port_length =
				sizeof(struct cam_isp_tfe_in_port_info) +
				(in_port->num_out_res - 1) *
				sizeof(struct cam_isp_tfe_out_port_info);
			if (in_port_length > isp_resource[i].length) {
				CAM_ERR(CAM_ISP, "buffer size is not enough");
				rc = -EINVAL;
				kfree(in_port);
				goto free_res;
			}

			rc = cam_tfe_mgr_acquire_hw_for_ctx(tfe_ctx, in_port,
				&num_pix_port_per_in, &num_rdi_port_per_in,
				&pdad_enable);
			total_pix_port += num_pix_port_per_in;
			total_rdi_port += num_rdi_port_per_in;

			kfree(in_port);
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
		tfe_ctx->is_rdi_only_context = 1;
		CAM_DBG(CAM_ISP, "RDI only context");
	} else
		tfe_ctx->is_rdi_only_context = 0;

	/* Process base info */
	rc = cam_tfe_mgr_process_base_info(tfe_ctx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Process base info failed");
		goto free_res;
	}

	acquire_args->ctxt_to_hw_map = tfe_ctx;
	tfe_ctx->ctx_in_use = 1;

	cam_tfe_hw_mgr_put_ctx(&tfe_hw_mgr->used_ctx_list, &tfe_ctx);

	CAM_DBG(CAM_ISP, "Exit...(success)");

	return 0;
free_res:
	cam_tfe_hw_mgr_release_hw_for_ctx(tfe_ctx);
	cam_cdm_release(tfe_ctx->cdm_handle);
free_ctx:
	cam_tfe_hw_mgr_put_ctx(&tfe_hw_mgr->free_ctx_list, &tfe_ctx);
err:
	CAM_ERR_RATE_LIMIT(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

/* entry function: acquire_hw */
static int cam_tfe_mgr_acquire(void *hw_mgr_priv,
	void *acquire_hw_args)
{
	struct cam_hw_acquire_args *acquire_args     = acquire_hw_args;
	int rc                                       = -EINVAL;

	CAM_DBG(CAM_ISP, "Enter...");

	if (!acquire_args || acquire_args->num_acq <= 0) {
		CAM_ERR(CAM_ISP, "Nothing to acquire. Seems like error");
		return -EINVAL;
	}

	if (acquire_args->num_acq == CAM_API_COMPAT_CONSTANT)
		rc = cam_tfe_mgr_acquire_hw(hw_mgr_priv, acquire_hw_args);
	else
		rc = cam_tfe_mgr_acquire_dev(hw_mgr_priv, acquire_hw_args);

	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

static const char *cam_tfe_util_usage_data_to_string(
	uint32_t usage_data)
{
	switch (usage_data) {
	case CAM_ISP_TFE_USAGE_LEFT_PX:
		return "LEFT_PX";
	case CAM_ISP_TFE_USAGE_RIGHT_PX:
		return "RIGHT_PX";
	case CAM_ISP_TFE_USAGE_RDI:
		return "RDI";
	default:
		return "USAGE_INVALID";
	}
}

static int cam_tfe_classify_vote_info(
	struct cam_isp_hw_mgr_res            *hw_mgr_res,
	struct cam_isp_bw_config_internal_v2 *bw_config,
	struct cam_axi_vote                  *isp_vote,
	uint32_t                              split_idx,
	bool                                 *camif_l_bw_updated,
	bool                                 *camif_r_bw_updated)
{
	int                                   rc = 0, i, j = 0;

	if (hw_mgr_res->res_id == CAM_ISP_HW_TFE_IN_CAMIF) {
		if (split_idx == CAM_ISP_HW_SPLIT_LEFT) {
			if (*camif_l_bw_updated)
				return rc;

			for (i = 0; i < bw_config->num_paths; i++) {
				if (bw_config->axi_path[i].usage_data ==
					CAM_ISP_TFE_USAGE_LEFT_PX) {
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
					CAM_ISP_TFE_USAGE_RIGHT_PX) {
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
	} else if ((hw_mgr_res->res_id >= CAM_ISP_HW_TFE_IN_RDI0)
		&& (hw_mgr_res->res_id <=
		CAM_ISP_HW_TFE_IN_RDI2)) {
		for (i = 0; i < bw_config->num_paths; i++) {
			if ((bw_config->axi_path[i].usage_data ==
				CAM_ISP_TFE_USAGE_RDI) &&
				((bw_config->axi_path[i].path_data_type -
				CAM_AXI_PATH_DATA_IFE_RDI0) ==
				(hw_mgr_res->res_id -
				CAM_ISP_HW_TFE_IN_RDI0))) {
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
			cam_tfe_util_usage_data_to_string(
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

static int cam_isp_tfe_blob_bw_update(
	struct cam_isp_bw_config_internal_v2  *bw_config,
	struct cam_tfe_hw_mgr_ctx             *ctx)
{
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_tfe_bw_update_args          *bw_upd_args = NULL;
	int                                    rc = -EINVAL;
	uint32_t                               i, split_idx;
	bool                                   camif_l_bw_updated = false;
	bool                                   camif_r_bw_updated = false;

	for (i = 0; i < bw_config->num_paths; i++) {
		CAM_DBG(CAM_PERF,
			"ISP_BLOB usage_type=%u [%s] [%s] [%s] [%llu] [%llu] [%llu]",
			bw_config->usage_type,
			cam_tfe_util_usage_data_to_string(
			bw_config->axi_path[i].usage_data),
			cam_cpas_axi_util_path_type_to_string(
			bw_config->axi_path[i].path_data_type),
			cam_cpas_axi_util_trans_type_to_string(
			bw_config->axi_path[i].transac_type),
			bw_config->axi_path[i].camnoc_bw,
			bw_config->axi_path[i].mnoc_ab_bw,
			bw_config->axi_path[i].mnoc_ib_bw);
	}

	bw_upd_args = kzalloc(sizeof(struct cam_tfe_bw_update_args),
		GFP_KERNEL);
	if (!bw_upd_args) {
		CAM_ERR(CAM_ISP, "Out of memory");
		return -ENOMEM;
	}
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		for (split_idx = 0; split_idx < CAM_ISP_HW_SPLIT_MAX;
			split_idx++) {
			if (!hw_mgr_res->hw_res[split_idx])
				continue;

			memset(&bw_upd_args->isp_vote, 0,
				sizeof(struct cam_axi_vote));
			rc = cam_tfe_classify_vote_info(hw_mgr_res, bw_config,
				&bw_upd_args->isp_vote, split_idx,
				&camif_l_bw_updated, &camif_r_bw_updated);
			if (rc)
				goto end;

			if (!bw_upd_args->isp_vote.num_paths)
				continue;

			hw_intf = hw_mgr_res->hw_res[split_idx]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				bw_upd_args->node_res =
					hw_mgr_res->hw_res[split_idx];

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_BW_UPDATE_V2,
					bw_upd_args,
					sizeof(
					struct cam_tfe_bw_update_args));
				if (rc)
					CAM_ERR(CAM_ISP,
						"BW Update failed rc: %d", rc);
			} else {
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
			}
		}
	}

end:
	kzfree(bw_upd_args);
	bw_upd_args = NULL;
	return rc;
}

/* entry function: config_hw */
static int cam_tfe_mgr_config_hw(void *hw_mgr_priv,
	void *config_hw_args)
{
	int rc = -EINVAL, i, skip = 0;
	struct cam_hw_config_args *cfg;
	struct cam_hw_update_entry *cmd;
	struct cam_cdm_bl_request *cdm_cmd;
	struct cam_tfe_hw_mgr_ctx *ctx;
	struct cam_isp_prepare_hw_update_data *hw_update_data;
	bool cdm_hang_detect = false;

	if (!hw_mgr_priv || !config_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	cfg = config_hw_args;
	ctx = (struct cam_tfe_hw_mgr_ctx *)cfg->ctxt_to_hw_map;
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
	hw_update_data->isp_mgr_ctx = ctx;

	if (cfg->reapply && cfg->cdm_reset_before_apply) {
		if (ctx->last_cdm_done_req < cfg->request_id) {
			cdm_hang_detect =
				cam_cdm_detect_hang_error(ctx->cdm_handle);
			CAM_ERR_RATE_LIMIT(CAM_ISP,
				"CDM callback not received for req: %lld, last_cdm_done_req: %lld, cdm_hang_detect: %d",
				cfg->request_id, ctx->last_cdm_done_req,
				cdm_hang_detect);
			rc = cam_cdm_reset_hw(ctx->cdm_handle);
			if (rc) {
				CAM_ERR_RATE_LIMIT(CAM_ISP,
					"CDM reset unsuccessful for req: %lld, ctx: %d, rc: %d",
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

	for (i = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
		if (hw_update_data->bw_config_valid[i] == true) {

			CAM_DBG(CAM_ISP, "idx=%d, bw_config_version=%d",
				ctx->ctx_index, i,
				hw_update_data->bw_config_version);
			if (hw_update_data->bw_config_version ==
				CAM_ISP_BW_CONFIG_V2) {
				rc = cam_isp_tfe_blob_bw_update(
					&hw_update_data->bw_config_v2[i], ctx);
				if (rc)
					CAM_ERR(CAM_ISP,
					"Bandwidth Update Failed rc: %d", rc);
			} else {
				CAM_ERR(CAM_ISP,
					"Invalid bw config version: %d",
					hw_update_data->bw_config_version);
			}
		}
	}

	CAM_DBG(CAM_ISP,
		"Enter ctx id:%d num_hw_upd_entries %d request id: %llu",
		ctx->ctx_index, cfg->num_hw_update_entries, cfg->request_id);

	if (cfg->num_hw_update_entries <= 0) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"Enter ctx id:%d no valid hw entries:%d request id: %llu",
			ctx->ctx_index, cfg->num_hw_update_entries,
			cfg->request_id);
		goto end;
	}

	cdm_cmd                       = ctx->cdm_cmd;
	cdm_cmd->cmd_arrary_count     = cfg->num_hw_update_entries;
	cdm_cmd->type                 = CAM_CDM_BL_CMD_TYPE_MEM_HANDLE;
	cdm_cmd->flag                 = true;
	cdm_cmd->userdata             = hw_update_data;
	cdm_cmd->cookie               = cfg->request_id;
	cdm_cmd->gen_irq_arb          = false;

	for (i = 0; i < cfg->num_hw_update_entries; i++) {
		cmd = (cfg->hw_update_entries + i);
		if (cfg->reapply && cmd->flags == CAM_ISP_IQ_BL) {
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

	ctx->packet = (struct cam_packet *)hw_update_data->packet;
	ctx->last_submit_bl_cmd.bl_count = cdm_cmd->cmd_arrary_count;

	for (i = 0; i < cdm_cmd->cmd_arrary_count; i++) {
		if (cdm_cmd->type == CAM_CDM_BL_CMD_TYPE_MEM_HANDLE) {
			ctx->last_submit_bl_cmd.cmd[i].mem_handle =
				cdm_cmd->cmd[i].bl_addr.mem_handle;

			rc = cam_mem_get_io_buf(
			cdm_cmd->cmd[i].bl_addr.mem_handle,
			g_tfe_hw_mgr.mgr_common.cmd_iommu_hdl,
			&ctx->last_submit_bl_cmd.cmd[i].hw_addr,
			&ctx->last_submit_bl_cmd.cmd[i].len);
		} else if (cdm_cmd->type ==
			CAM_CDM_BL_CMD_TYPE_HW_IOVA) {
			if (!cdm_cmd->cmd[i].bl_addr.hw_iova) {
				CAM_ERR(CAM_CDM,
					"Submitted Hw bl hw_iova is invalid %d:%d",
					i, cdm_cmd->cmd_arrary_count);
				rc = -EINVAL;
				break;
			}
			rc = 0;
			ctx->last_submit_bl_cmd.cmd[i].hw_addr =
			(uint64_t)cdm_cmd->cmd[i].bl_addr.hw_iova;
			ctx->last_submit_bl_cmd.cmd[i].len =
			cdm_cmd->cmd[i].len + cdm_cmd->cmd[i].offset;
			ctx->last_submit_bl_cmd.cmd[i].mem_handle = 0;
		} else
			CAM_INFO(CAM_ISP,
				"submitted invalid bl cmd addr type :%d for Bl(%d)",
				cdm_cmd->type, i);

		ctx->last_submit_bl_cmd.cmd[i].offset =
			cdm_cmd->cmd[i].offset;
		ctx->last_submit_bl_cmd.cmd[i].type =
			cdm_cmd->type;
		ctx->last_submit_bl_cmd.cmd[i].input_len =
		 cdm_cmd->cmd[i].len;
	}

	if (!cfg->init_packet)
		goto end;

	for (i = 0; i < CAM_TFE_HW_CONFIG_WAIT_MAX_TRY; i++) {
		rc = wait_for_completion_timeout(
			&ctx->config_done_complete,
			msecs_to_jiffies(
			CAM_TFE_HW_CONFIG_TIMEOUT));
		if (rc <= 0) {
			if (!cam_cdm_detect_hang_error(ctx->cdm_handle)) {
				CAM_ERR(CAM_ISP,
					"CDM workqueue delay detected, wait for some more time req_id=%llu rc=%d ctx_index %d",
					cfg->request_id, rc,
					ctx->ctx_index);
				cam_req_mgr_debug_delay_detect();
				trace_cam_delay_detect("CDM",
					"CDM workqueue delay detected",
					cfg->request_id, ctx->ctx_index,
					CAM_DEFAULT_VALUE,
					CAM_DEFAULT_VALUE, rc);
				continue;
			}

			CAM_ERR(CAM_ISP,
				"config done completion timeout for req_id=%llu rc=%d ctx_index %d",
				cfg->request_id, rc,
				ctx->ctx_index);

			cam_req_mgr_debug_delay_detect();
			trace_cam_delay_detect("ISP",
				"config done completion timeout",
				cfg->request_id, ctx->ctx_index,
				CAM_DEFAULT_VALUE, CAM_DEFAULT_VALUE,
				rc);

			if (rc == 0)
				rc = -ETIMEDOUT;

			goto end;
		} else {
			rc = 0;
			CAM_DBG(CAM_ISP,
				"config done Success for req_id=%llu ctx_index %d",
				cfg->request_id, ctx->ctx_index);
			break;
		}
	}

	if ((i == CAM_TFE_HW_CONFIG_WAIT_MAX_TRY) && (rc == 0))
		rc = -ETIMEDOUT;

end:
	CAM_DBG(CAM_ISP, "Exit: Config Done: %llu",  cfg->request_id);

	return rc;
}

static int cam_tfe_mgr_stop_hw_in_overflow(void *stop_hw_args)
{
	int                               rc        = 0;
	struct cam_hw_stop_args          *stop_args = stop_hw_args;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_tfe_hw_mgr_ctx        *ctx;
	uint32_t                          i, master_base_idx = 0;

	if (!stop_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}
	ctx = (struct cam_tfe_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
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


	/* stop the master CSID path first */
	cam_tfe_mgr_csid_stop_hw(ctx, &ctx->res_list_tfe_csid,
		master_base_idx, CAM_TFE_CSID_HALT_IMMEDIATELY);

	/* Stop rest of the CSID paths  */
	for (i = 0; i < ctx->num_base; i++) {
		if (i == master_base_idx)
			continue;

		cam_tfe_mgr_csid_stop_hw(ctx, &ctx->res_list_tfe_csid,
			ctx->base[i].idx, CAM_TFE_CSID_HALT_IMMEDIATELY);
	}

	/* TFE in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		cam_tfe_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	/* TFE out resources */
	for (i = 0; i < CAM_TFE_HW_OUT_RES_MAX; i++)
		cam_tfe_hw_mgr_stop_hw_res(&ctx->res_list_tfe_out[i]);

	if (ctx->is_tpg)
		cam_tfe_hw_mgr_stop_hw_res(&ctx->res_list_tpg);

	/* Stop tasklet for context */
	cam_tasklet_stop(ctx->common.tasklet_info);
	CAM_DBG(CAM_ISP, "Exit...ctx id:%d rc :%d",
		ctx->ctx_index, rc);

	return rc;
}

static int cam_tfe_mgr_bw_control(struct cam_tfe_hw_mgr_ctx *ctx,
	enum cam_tfe_bw_control_action action)
{
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_tfe_bw_control_args         bw_ctrl_args;
	int                                    rc = -EINVAL;
	uint32_t                               i;

	CAM_DBG(CAM_ISP, "Enter...ctx id:%d", ctx->ctx_index);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
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
					sizeof(struct cam_tfe_bw_control_args));
				if (rc)
					CAM_ERR(CAM_ISP, "BW Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_tfe_mgr_pause_hw(struct cam_tfe_hw_mgr_ctx *ctx)
{
	return cam_tfe_mgr_bw_control(ctx, CAM_TFE_BW_CONTROL_EXCLUDE);
}

/* entry function: stop_hw */
static int cam_tfe_mgr_stop_hw(void *hw_mgr_priv, void *stop_hw_args)
{
	int                               rc        = 0;
	struct cam_hw_stop_args          *stop_args = stop_hw_args;
	struct cam_isp_stop_args         *stop_isp;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_tfe_hw_mgr_ctx        *ctx;
	enum cam_tfe_csid_halt_cmd        csid_halt_type;
	uint32_t                          i, master_base_idx = 0;

	if (!hw_mgr_priv || !stop_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_tfe_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, " Enter...ctx id:%d", ctx->ctx_index);
	stop_isp = (struct cam_isp_stop_args    *)stop_args->args;

	/* Set the csid halt command */
	if (stop_isp->hw_stop_cmd == CAM_ISP_HW_STOP_AT_FRAME_BOUNDARY)
		csid_halt_type = CAM_TFE_CSID_HALT_AT_FRAME_BOUNDARY;
	else
		csid_halt_type = CAM_TFE_CSID_HALT_IMMEDIATELY;

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

	/*Change slave mode*/
	if (csid_halt_type == CAM_CSID_HALT_IMMEDIATELY)
		cam_tfe_mgr_csid_change_halt_mode(ctx,
			CAM_TFE_CSID_HALT_MODE_INTERNAL);

	CAM_DBG(CAM_ISP, "Stopping master CSID idx %d", master_base_idx);

	/* Stop the master CSID path first */
	cam_tfe_mgr_csid_stop_hw(ctx, &ctx->res_list_tfe_csid,
		master_base_idx, csid_halt_type);

	/* stop rest of the CSID paths  */
	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].idx == master_base_idx)
			continue;
		CAM_DBG(CAM_ISP, "Stopping CSID idx %d i %d master %d",
			ctx->base[i].idx, i, master_base_idx);

		cam_tfe_mgr_csid_stop_hw(ctx, &ctx->res_list_tfe_csid,
			ctx->base[i].idx, csid_halt_type);
	}

	CAM_DBG(CAM_ISP, "Going to stop TFE Out");

	/* TFE out resources */
	for (i = 0; i < CAM_TFE_HW_OUT_RES_MAX; i++)
		cam_tfe_hw_mgr_stop_hw_res(&ctx->res_list_tfe_out[i]);

	CAM_DBG(CAM_ISP, "Going to stop TFE IN");

	/* TFE in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		cam_tfe_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	cam_tasklet_stop(ctx->common.tasklet_info);

	cam_tfe_mgr_pause_hw(ctx);

	wait_for_completion_timeout(&ctx->config_done_complete,
		msecs_to_jiffies(5));

	if (ctx->is_tpg)
		cam_tfe_hw_mgr_stop_hw_res(&ctx->res_list_tpg);

	if (stop_isp->stop_only)
		goto end;

	if (cam_cdm_stream_off(ctx->cdm_handle))
		CAM_ERR(CAM_ISP, "CDM stream off failed %d", ctx->cdm_handle);

	cam_tfe_hw_mgr_deinit_hw(ctx);

	CAM_DBG(CAM_ISP,
		"Stop success for ctx id:%d rc :%d", ctx->ctx_index, rc);

	mutex_lock(&g_tfe_hw_mgr.ctx_mutex);
	atomic_dec_return(&g_tfe_hw_mgr.active_ctx_cnt);
	mutex_unlock(&g_tfe_hw_mgr.ctx_mutex);

	for (i = 0; i < ctx->last_submit_bl_cmd.bl_count; i++) {
		ctx->last_submit_bl_cmd.cmd[i].mem_handle = 0;
		ctx->last_submit_bl_cmd.cmd[i].hw_addr = 0;
		ctx->last_submit_bl_cmd.cmd[i].len = 0;
		ctx->last_submit_bl_cmd.cmd[i].offset = 0;
		ctx->last_submit_bl_cmd.cmd[i].type = 0;
		ctx->last_submit_bl_cmd.cmd[i].input_len = 0;
	}
	ctx->last_submit_bl_cmd.bl_count = 0;
	ctx->packet = NULL;

end:
	return rc;
}

static int cam_tfe_mgr_reset_tfe_hw(struct cam_tfe_hw_mgr *hw_mgr,
	uint32_t hw_idx)
{
	uint32_t i = 0;
	struct cam_hw_intf             *tfe_hw_intf;
	uint32_t tfe_reset_type;

	if (!hw_mgr) {
		CAM_DBG(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}
	/* Reset TFE HW*/
	tfe_reset_type = CAM_TFE_HW_RESET_HW;

	for (i = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
		if (!hw_mgr->tfe_devices[i])
			continue;

		if (hw_idx != hw_mgr->tfe_devices[i]->hw_intf->hw_idx)
			continue;
		CAM_DBG(CAM_ISP, "TFE (id = %d) reset", hw_idx);
		tfe_hw_intf = hw_mgr->tfe_devices[i]->hw_intf;
		tfe_hw_intf->hw_ops.reset(tfe_hw_intf->hw_priv,
			&tfe_reset_type, sizeof(tfe_reset_type));
		break;
	}

	CAM_DBG(CAM_ISP, "Exit Successfully");
	return 0;
}

static int cam_tfe_mgr_restart_hw(void *start_hw_args)
{
	int                               rc = -EINVAL;
	struct cam_hw_start_args         *start_args = start_hw_args;
	struct cam_tfe_hw_mgr_ctx        *ctx;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	uint32_t                          i;

	if (!start_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_tfe_hw_mgr_ctx *)start_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "START TFE OUT ... in ctx id:%d", ctx->ctx_index);

	cam_tasklet_start(ctx->common.tasklet_info);

	/* start the TFE out devices */
	for (i = 0; i < CAM_TFE_HW_OUT_RES_MAX; i++) {
		rc = cam_tfe_hw_mgr_start_hw_res(
			&ctx->res_list_tfe_out[i], ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start TFE OUT (%d)", i);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START TFE SRC ... in ctx id:%d", ctx->ctx_index);

	/* Start the TFE in devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		rc = cam_tfe_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start TFE IN (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START CSID HW ... in ctx id:%d", ctx->ctx_index);
	/* Start the TFE CSID HW devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		rc = cam_tfe_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start TFE CSID (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "Exit...(success)");
	return 0;

err:
	cam_tfe_mgr_stop_hw_in_overflow(start_hw_args);
	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;
}

static int cam_tfe_mgr_start_hw(void *hw_mgr_priv, void *start_hw_args)
{
	int                               rc = -EINVAL;
	struct cam_isp_start_args        *start_isp = start_hw_args;
	struct cam_hw_stop_args           stop_args;
	struct cam_isp_stop_args          stop_isp;
	struct cam_tfe_hw_mgr_ctx        *ctx;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_hw_intf               *hw_intf;
	uint32_t                          i;
	bool                              res_rdi_context_set = false;
	uint32_t                          primary_rdi_in_res;
	uint32_t                          primary_rdi_out_res;
	bool                              hw_id[CAM_TFE_HW_NUM_MAX] = {0};

	primary_rdi_in_res = CAM_ISP_HW_TFE_IN_MAX;
	primary_rdi_out_res = CAM_ISP_TFE_OUT_RES_MAX;

	if (!hw_mgr_priv || !start_isp) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_tfe_hw_mgr_ctx *)
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

	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_id[hw_intf->hw_idx])
				continue;

			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_TFE_CSID_SET_CSID_DEBUG,
				&g_tfe_hw_mgr.debug_cfg.csid_debug,
				sizeof(g_tfe_hw_mgr.debug_cfg.csid_debug));
			hw_id[hw_intf->hw_idx] = true;
		}
	}

	memset(&hw_id[0], 0, sizeof(hw_id));
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_id[hw_intf->hw_idx])
				continue;

			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_ISP_HW_CMD_SET_CAMIF_DEBUG,
				&g_tfe_hw_mgr.debug_cfg.camif_debug,
				sizeof(g_tfe_hw_mgr.debug_cfg.camif_debug));
			hw_id[hw_intf->hw_idx] = true;
		}
	}

	rc = cam_tfe_hw_mgr_init_hw(ctx);
	if (rc) {
		CAM_ERR(CAM_ISP, "Init failed");
		goto tasklet_stop;
	}

	ctx->init_done = true;

	mutex_lock(&g_tfe_hw_mgr.ctx_mutex);
	atomic_fetch_inc(&g_tfe_hw_mgr.active_ctx_cnt);
	mutex_unlock(&g_tfe_hw_mgr.ctx_mutex);

	CAM_DBG(CAM_ISP, "start cdm interface");
	rc = cam_cdm_stream_on(ctx->cdm_handle);
	if (rc) {
		CAM_ERR(CAM_ISP, "Can not start cdm (%d)",
			 ctx->cdm_handle);
		goto deinit_hw;
	}

start_only:
	/* Apply initial configuration */
	CAM_DBG(CAM_ISP, "Config HW");
	rc = cam_tfe_mgr_config_hw(hw_mgr_priv, &start_isp->hw_config);
	if (rc) {
		CAM_ERR(CAM_ISP, "Config HW failed");
		goto cdm_streamoff;
	}

	CAM_DBG(CAM_ISP, "START TFE OUT ... in ctx id:%d",
		ctx->ctx_index);
	/* start the TFE out devices */
	for (i = 0; i < CAM_TFE_HW_OUT_RES_MAX; i++) {
		hw_mgr_res = &ctx->res_list_tfe_out[i];
		switch (hw_mgr_res->res_id) {
		case CAM_ISP_TFE_OUT_RES_RDI_0:
		case CAM_ISP_TFE_OUT_RES_RDI_1:
		case CAM_ISP_TFE_OUT_RES_RDI_2:
			if (!res_rdi_context_set && ctx->is_rdi_only_context) {
				hw_mgr_res->hw_res[0]->rdi_only_ctx =
					ctx->is_rdi_only_context;
				res_rdi_context_set = true;
				primary_rdi_out_res = hw_mgr_res->res_id;
			}
		}

		rc = cam_tfe_hw_mgr_start_hw_res(
			&ctx->res_list_tfe_out[i], ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start TFE OUT (%d)",
				 i);
			goto err;
		}
	}

	if (primary_rdi_out_res < CAM_ISP_TFE_OUT_RES_MAX)
		primary_rdi_in_res =
			cam_tfe_hw_mgr_convert_rdi_out_res_id_to_in_res(
			primary_rdi_out_res);

	CAM_DBG(CAM_ISP, "START TFE IN ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the TFE in resources devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		/*
		 * if rdi only context has two rdi resources then only one irq
		 * subscription should be sufficient
		 */
		if (primary_rdi_in_res == hw_mgr_res->res_id)
			hw_mgr_res->hw_res[0]->rdi_only_ctx =
				ctx->is_rdi_only_context;

		rc = cam_tfe_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start TFE in resource (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START CSID HW ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the TFE CSID HW devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		rc = cam_tfe_hw_mgr_start_hw_res(hw_mgr_res, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start TFE CSID (%d)",
				 hw_mgr_res->res_id);
			goto err;
		}
	}

	if (ctx->is_tpg) {
		CAM_DBG(CAM_ISP, "START TPG HW ... in ctx id:%d",
			ctx->ctx_index);
		rc = cam_tfe_hw_mgr_start_hw_res(&ctx->res_list_tpg, ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start TFE TPG (%d)",
				ctx->res_list_tpg.res_id);
			goto err;
		}
	}

	return 0;

err:
	stop_isp.stop_only = false;
	stop_isp.hw_stop_cmd = CAM_ISP_HW_STOP_IMMEDIATELY;
	stop_args.ctxt_to_hw_map = start_isp->hw_config.ctxt_to_hw_map;
	stop_args.args = (void *)(&stop_isp);

	cam_tfe_mgr_stop_hw(hw_mgr_priv, &stop_args);
	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
	return rc;

cdm_streamoff:
	cam_cdm_stream_off(ctx->cdm_handle);

deinit_hw:
	cam_tfe_hw_mgr_deinit_hw(ctx);

tasklet_stop:
	cam_tasklet_stop(ctx->common.tasklet_info);

	return rc;
}

static int cam_tfe_mgr_read(void *hw_mgr_priv, void *read_args)
{
	return -EPERM;
}

static int cam_tfe_mgr_write(void *hw_mgr_priv, void *write_args)
{
	return -EPERM;
}

static int cam_tfe_mgr_user_dump_hw(
		struct cam_tfe_hw_mgr_ctx *tfe_ctx,
		struct cam_hw_dump_args *dump_args)
{
	int rc = 0;
	struct cam_hw_soc_dump_args soc_dump_args;

	if (!tfe_ctx || !dump_args) {
		CAM_ERR(CAM_ISP, "Invalid parameters %pK %pK",
			tfe_ctx, dump_args);
		return -EINVAL;
	}
	soc_dump_args.buf_handle = dump_args->buf_handle;
	soc_dump_args.request_id = dump_args->request_id;
	soc_dump_args.offset = dump_args->offset;

	rc = cam_tfe_mgr_handle_reg_dump(tfe_ctx,
		tfe_ctx->reg_dump_buf_desc,
		tfe_ctx->num_reg_dump_buf,
		CAM_ISP_TFE_PACKET_META_REG_DUMP_ON_ERROR,
		&soc_dump_args,
		true);
	if (rc) {
		CAM_DBG(CAM_ISP,
			"Dump failed req: %lld handle %u offset %u rc %d",
			dump_args->request_id,
			dump_args->buf_handle,
			dump_args->offset,
			rc);
		return rc;
	}
	dump_args->offset = soc_dump_args.offset;
	return rc;
}

static int cam_tfe_mgr_dump(void *hw_mgr_priv, void *args)
{

	struct cam_isp_hw_dump_args isp_hw_dump_args;
	struct cam_hw_dump_args *dump_args = (struct cam_hw_dump_args *)args;
	struct cam_isp_hw_mgr_res            *hw_mgr_res;
	struct cam_hw_intf                   *hw_intf;
	struct cam_tfe_hw_mgr_ctx *tfe_ctx = (struct cam_tfe_hw_mgr_ctx *)
						dump_args->ctxt_to_hw_map;
	int i;
	int rc = 0;

	/* for some targets, information about the TFE registers to be dumped
	 * is already submitted with the hw manager. In this case, we
	 * can dump just the related registers and skip going to core files.
	 * If dump to this buffer falis due to any reason, fallback to dump
	 * to the LDAR buffer
	 */
	isp_hw_dump_args.is_dump_all = true;
	if (tfe_ctx->num_reg_dump_buf) {
		rc = cam_tfe_mgr_user_dump_hw(tfe_ctx, dump_args);
		if (!rc)
			isp_hw_dump_args.is_dump_all = false;
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

	list_for_each_entry(hw_mgr_res, &tfe_ctx->res_list_tfe_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			switch (hw_mgr_res->hw_res[i]->res_id) {
			case CAM_TFE_CSID_PATH_RES_RDI_0:
			case CAM_TFE_CSID_PATH_RES_RDI_1:
			case CAM_TFE_CSID_PATH_RES_RDI_2:
				if (tfe_ctx->is_rdi_only_context &&
					hw_intf->hw_ops.process_cmd) {
					rc = hw_intf->hw_ops.process_cmd(
						hw_intf->hw_priv,
						CAM_ISP_HW_CMD_DUMP_HW,
						&isp_hw_dump_args,
						sizeof(struct
						    cam_isp_hw_dump_args));
				}
				break;

			case CAM_TFE_CSID_PATH_RES_IPP:
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

	list_for_each_entry(hw_mgr_res, &tfe_ctx->res_list_tfe_in, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			switch (hw_mgr_res->hw_res[i]->res_id) {
			case CAM_ISP_HW_TFE_IN_RDI0:
			case CAM_ISP_HW_TFE_IN_RDI1:
			case CAM_ISP_HW_TFE_IN_RDI2:
				if (tfe_ctx->is_rdi_only_context &&
					hw_intf->hw_ops.process_cmd) {
					rc = hw_intf->hw_ops.process_cmd(
						hw_intf->hw_priv,
						CAM_ISP_HW_CMD_DUMP_HW,
						&isp_hw_dump_args,
						sizeof(struct
						    cam_isp_hw_dump_args));
				}
				break;

			case CAM_ISP_HW_TFE_IN_CAMIF:
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
	CAM_DBG(CAM_ISP, "offset %u", dump_args->offset);
	return rc;
}

static int cam_tfe_mgr_reset(void *hw_mgr_priv, void *hw_reset_args)
{
	struct cam_tfe_hw_mgr            *hw_mgr = hw_mgr_priv;
	struct cam_hw_reset_args         *reset_args = hw_reset_args;
	struct cam_tfe_hw_mgr_ctx        *ctx;
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	int                               rc = 0, i = 0;

	if (!hw_mgr_priv || !hw_reset_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_tfe_hw_mgr_ctx *)reset_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "Reset CSID and TFE");
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		rc = cam_tfe_hw_mgr_reset_csid_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Failed to reset CSID:%d rc: %d",
				hw_mgr_res->res_id, rc);
			goto end;
		}
	}

	for (i = 0; i < ctx->num_base; i++) {
		rc = cam_tfe_mgr_reset_tfe_hw(hw_mgr, ctx->base[i].idx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Failed to reset TFE:%d rc: %d",
				ctx->base[i].idx, rc);
			goto end;
		}
	}

	atomic_set(&ctx->overflow_pending, 0);
end:
	return rc;
}

static int cam_tfe_mgr_release_hw(void *hw_mgr_priv,
					void *release_hw_args)
{
	int                               rc           = 0;
	struct cam_hw_release_args       *release_args = release_hw_args;
	struct cam_tfe_hw_mgr            *hw_mgr       = hw_mgr_priv;
	struct cam_tfe_hw_mgr_ctx        *ctx;
	uint32_t                          i;

	if (!hw_mgr_priv || !release_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_tfe_hw_mgr_ctx *)release_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "Enter...ctx id:%d",
		ctx->ctx_index);

	if (ctx->init_done)
		cam_tfe_hw_mgr_deinit_hw(ctx);

	/* we should called the stop hw before this already */
	cam_tfe_hw_mgr_release_hw_for_ctx(ctx);

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
	ctx->init_done = false;
	ctx->is_dual = false;
	ctx->is_tpg  = false;
	ctx->num_reg_dump_buf = 0;
	ctx->res_list_tpg.res_type = CAM_ISP_RESOURCE_MAX;
	ctx->last_cdm_done_req = 0;
	atomic_set(&ctx->overflow_pending, 0);

	for (i = 0; i < ctx->last_submit_bl_cmd.bl_count; i++) {
		ctx->last_submit_bl_cmd.cmd[i].mem_handle = 0;
		ctx->last_submit_bl_cmd.cmd[i].hw_addr = 0;
		ctx->last_submit_bl_cmd.cmd[i].len = 0;
		ctx->last_submit_bl_cmd.cmd[i].offset = 0;
		ctx->last_submit_bl_cmd.cmd[i].type = 0;
		ctx->last_submit_bl_cmd.cmd[i].input_len = 0;
	}
	ctx->last_submit_bl_cmd.bl_count = 0;
	ctx->packet = NULL;

	CAM_DBG(CAM_ISP, "Exit...ctx id:%d",
		ctx->ctx_index);
	cam_tfe_hw_mgr_put_ctx(&hw_mgr->free_ctx_list, &ctx);
	return rc;
}

static int cam_isp_tfe_blob_hfr_update(
	uint32_t                                  blob_type,
	struct cam_isp_generic_blob_info         *blob_info,
	struct cam_isp_tfe_resource_hfr_config   *hfr_config,
	struct cam_hw_prepare_update_args        *prepare)
{
	struct cam_isp_tfe_port_hfr_config    *port_hfr_config;
	struct cam_kmd_buf_info               *kmd_buf_info;
	struct cam_tfe_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	uint32_t                               res_id_out, i;
	uint32_t                               total_used_bytes = 0;
	uint32_t                               kmd_buf_remain_size;
	uint32_t                              *cmd_buf_addr;
	uint32_t                               bytes_used = 0;
	int                                    num_ent, rc = 0;

	ctx = prepare->ctxt_to_hw_map;
	CAM_DBG(CAM_ISP, "num_ports= %d", hfr_config->num_ports);

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

		if (res_id_out >= CAM_TFE_HW_OUT_RES_MAX) {
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
		hw_mgr_res = &ctx->res_list_tfe_out[res_id_out];

		rc = cam_isp_add_cmd_buf_update(
			hw_mgr_res, blob_type, CAM_ISP_HW_CMD_GET_HFR_UPDATE,
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
		kmd_buf_info->offset     += total_used_bytes;
		prepare->num_hw_update_entries = num_ent;
	}

	return rc;
}

static int cam_isp_tfe_blob_csid_clock_update(
	uint32_t                                   blob_type,
	struct cam_isp_generic_blob_info          *blob_info,
	struct cam_isp_tfe_csid_clock_config      *clock_config,
	struct cam_hw_prepare_update_args         *prepare)
{
	struct cam_tfe_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_tfe_csid_clock_update_args  csid_clock_upd_args;
	struct cam_top_tpg_clock_update_args   tpg_clock_upd_args;
	uint64_t                               clk_rate = 0;
	int                                    rc = -EINVAL;
	uint32_t                               i;

	ctx = prepare->ctxt_to_hw_map;

	CAM_DBG(CAM_ISP, "csid clk=%llu", clock_config->csid_clock);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i])
				continue;
			clk_rate = clock_config->csid_clock;
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				csid_clock_upd_args.clk_rate = clk_rate;
				CAM_DBG(CAM_ISP, "i= %d csid clk=%llu",
				i, csid_clock_upd_args.clk_rate);

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CSID_CLOCK_UPDATE,
					&csid_clock_upd_args,
					sizeof(
					struct cam_tfe_csid_clock_update_args));
				if (rc)
					CAM_ERR(CAM_ISP, "Clock Update failed");
			} else
				CAM_ERR(CAM_ISP, "NULL hw_intf!");
		}
	}

	if (ctx->res_list_tpg.res_type == CAM_ISP_RESOURCE_TPG) {
		tpg_clock_upd_args.clk_rate = clock_config->phy_clock;
		hw_intf = ctx->res_list_tpg.hw_res[0]->hw_intf;
		if (hw_intf && hw_intf->hw_ops.process_cmd) {
			CAM_DBG(CAM_ISP, "i= %d phy clk=%llu",
				tpg_clock_upd_args.clk_rate);
			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_ISP_HW_CMD_TPG_PHY_CLOCK_UPDATE,
				&tpg_clock_upd_args,
				sizeof(struct cam_top_tpg_clock_update_args));
			if (rc)
				CAM_ERR(CAM_ISP, "Clock Update failed");
		} else
			CAM_ERR(CAM_ISP, "NULL hw_intf!");
	}

	return rc;
}

static int cam_isp_tfe_blob_clock_update(
	uint32_t                               blob_type,
	struct cam_isp_generic_blob_info      *blob_info,
	struct cam_isp_tfe_clock_config       *clock_config,
	struct cam_hw_prepare_update_args     *prepare)
{
	struct cam_tfe_hw_mgr_ctx             *ctx = NULL;
	struct cam_isp_hw_mgr_res             *hw_mgr_res;
	struct cam_hw_intf                    *hw_intf;
	struct cam_tfe_clock_update_args       clock_upd_args;
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

	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i])
				continue;

			if (hw_mgr_res->res_id == CAM_ISP_HW_TFE_IN_CAMIF) {
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
				CAM_ISP_HW_TFE_IN_RDI0) && (hw_mgr_res->res_id
				<= CAM_ISP_HW_TFE_IN_RDI2)) {
				for (j = 0; j < clock_config->num_rdi; j++)
					clk_rate = max(clock_config->rdi_hz[j],
						clk_rate);
			} else {
				CAM_ERR(CAM_ISP, "Invalid res_id %u",
					hw_mgr_res->res_id);
				rc = -EINVAL;
				return rc;
			}

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				clock_upd_args.node_res =
					hw_mgr_res->hw_res[i];
				CAM_DBG(CAM_ISP,
				"res_id=%u i= %d clk=%llu",
				hw_mgr_res->res_id, i, clk_rate);

				clock_upd_args.clk_rate = clk_rate;

				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_ISP_HW_CMD_CLOCK_UPDATE,
					&clock_upd_args,
					sizeof(
					struct cam_tfe_clock_update_args));
				if (rc)
					CAM_ERR(CAM_ISP, "Clock Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_isp_tfe_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	int rc = 0;
	struct cam_isp_generic_blob_info  *blob_info = user_data;
	struct cam_hw_prepare_update_args *prepare = NULL;

	if (!blob_data || (blob_size == 0) || !blob_info) {
		CAM_ERR(CAM_ISP, "Invalid args data %pK size %d info %pK",
			blob_data, blob_size, blob_info);
		return -EINVAL;
	}

	prepare = blob_info->prepare;
	if (!prepare) {
		CAM_ERR(CAM_ISP, "Failed. prepare is NULL, blob_type %d",
			blob_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "BLOB Type: %d", blob_type);
	switch (blob_type) {
	case CAM_ISP_TFE_GENERIC_BLOB_TYPE_HFR_CONFIG: {
		struct cam_isp_tfe_resource_hfr_config    *hfr_config =
			(struct cam_isp_tfe_resource_hfr_config *)blob_data;

		if (blob_size <
			sizeof(struct cam_isp_tfe_resource_hfr_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		if (hfr_config->num_ports > CAM_ISP_TFE_OUT_RES_MAX) {
			CAM_ERR(CAM_ISP, "Invalid num_ports %u in hfr config",
				hfr_config->num_ports);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (hfr_config->num_ports > 1) {
			if (sizeof(struct cam_isp_tfe_resource_hfr_config) >
				((UINT_MAX -
				sizeof(struct cam_isp_tfe_resource_hfr_config))
				/ (hfr_config->num_ports - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in hfr config num_ports:%u size per port:%lu",
					hfr_config->num_ports,
					sizeof(struct
					cam_isp_tfe_resource_hfr_config));
				return -EINVAL;
			}
		}

		if ((hfr_config->num_ports != 0) && (blob_size <
			(sizeof(struct cam_isp_tfe_resource_hfr_config) +
			(hfr_config->num_ports - 1) *
			sizeof(struct cam_isp_tfe_port_hfr_config)))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(struct cam_isp_tfe_resource_hfr_config) +
				(hfr_config->num_ports - 1) *
				sizeof(struct cam_isp_tfe_resource_hfr_config));
			return -EINVAL;
		}

		rc = cam_isp_tfe_blob_hfr_update(blob_type, blob_info,
			hfr_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "HFR Update Failed");
	}
		break;
	case CAM_ISP_TFE_GENERIC_BLOB_TYPE_CLOCK_CONFIG: {
		struct cam_isp_tfe_clock_config    *clock_config =
			(struct cam_isp_tfe_clock_config *)blob_data;

		if (blob_size < sizeof(struct cam_isp_tfe_clock_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		if (clock_config->num_rdi > CAM_TFE_RDI_NUM_MAX) {
			CAM_ERR(CAM_ISP, "Invalid num_rdi %u in clock config",
				clock_config->num_rdi);
			return -EINVAL;
		}
		/* Check integer overflow */
		if (clock_config->num_rdi > 1) {
			if (sizeof(uint64_t) > ((UINT_MAX-
				sizeof(struct cam_isp_tfe_clock_config))/
				(clock_config->num_rdi - 1))) {
				CAM_ERR(CAM_ISP,
					"Max size exceeded in clock config num_rdi:%u size per port:%lu",
					clock_config->num_rdi,
					sizeof(uint64_t));
				return -EINVAL;
			}
		}

		if ((clock_config->num_rdi != 0) && (blob_size <
			(sizeof(struct cam_isp_tfe_clock_config) +
			sizeof(uint64_t) * (clock_config->num_rdi - 1)))) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %lu",
				blob_size,
				sizeof(uint32_t) * 2 + sizeof(uint64_t) *
				(clock_config->num_rdi + 2));
			return -EINVAL;
		}

		rc = cam_isp_tfe_blob_clock_update(blob_type, blob_info,
			clock_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "Clock Update Failed");
	}
		break;
	case CAM_ISP_TFE_GENERIC_BLOB_TYPE_BW_CONFIG_V2: {
		size_t bw_config_size = 0;
		struct cam_isp_tfe_bw_config_v2    *bw_config =
			(struct cam_isp_tfe_bw_config_v2 *)blob_data;
		struct cam_isp_prepare_hw_update_data   *prepare_hw_data;

		if (blob_size < sizeof(struct cam_isp_tfe_bw_config_v2)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u", blob_size);
			return -EINVAL;
		}

		if ((bw_config->num_paths > CAM_ISP_MAX_PER_PATH_VOTES) ||
			!bw_config->num_paths) {
			CAM_ERR(CAM_ISP, "Invalid num paths %d",
				bw_config->num_paths);
			return -EINVAL;
		}

		/* Check for integer overflow */
		if (bw_config->num_paths > 1) {
			if (sizeof(struct cam_axi_per_path_bw_vote) >
				((UINT_MAX -
				sizeof(struct cam_isp_tfe_bw_config_v2)) /
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
			(sizeof(struct cam_isp_tfe_bw_config_v2) +
			((bw_config->num_paths - 1) *
			sizeof(struct cam_axi_per_path_bw_vote))))) {
			CAM_ERR(CAM_ISP,
				"Invalid blob size: %u, num_paths: %u, bw_config size: %lu, per_path_vote size: %lu",
				blob_size, bw_config->num_paths,
				sizeof(struct cam_isp_tfe_bw_config_v2),
				sizeof(struct cam_axi_per_path_bw_vote));
			return -EINVAL;
		}

		if (!prepare || !prepare->priv ||
			(bw_config->usage_type >= CAM_TFE_HW_NUM_MAX)) {
			CAM_ERR(CAM_ISP, "Invalid inputs");
			return -EINVAL;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;

		memset(&prepare_hw_data->bw_config_v2[bw_config->usage_type],
			0, sizeof(
			prepare_hw_data->bw_config_v2[bw_config->usage_type]));
		bw_config_size = sizeof(struct cam_isp_tfe_bw_config_v2) +
			((bw_config->num_paths - 1) *
			sizeof(struct cam_axi_per_path_bw_vote));
		memcpy(&prepare_hw_data->bw_config_v2[bw_config->usage_type],
			bw_config, bw_config_size);

		prepare_hw_data->bw_config_version = CAM_ISP_BW_CONFIG_V2;
		prepare_hw_data->bw_config_valid[bw_config->usage_type] = true;
	}

		break;
	case CAM_ISP_TFE_GENERIC_BLOB_TYPE_CSID_CLOCK_CONFIG: {
		struct cam_isp_tfe_csid_clock_config    *clock_config =
			(struct cam_isp_tfe_csid_clock_config *)blob_data;

		if (blob_size < sizeof(struct cam_isp_tfe_csid_clock_config)) {
			CAM_ERR(CAM_ISP, "Invalid blob size %u expected %u",
				blob_size,
				sizeof(struct cam_isp_tfe_csid_clock_config));
			return -EINVAL;
		}
		rc = cam_isp_tfe_blob_csid_clock_update(blob_type, blob_info,
			clock_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "Clock Update Failed");
	}
		break;
	default:
		CAM_WARN(CAM_ISP, "Invalid blob type %d", blob_type);
		break;
	}

	return rc;
}

static int cam_tfe_update_dual_config(
	struct cam_cmd_buf_desc            *cmd_desc,
	uint32_t                            split_id,
	uint32_t                            base_idx,
	struct cam_isp_hw_mgr_res          *res_list_isp_out,
	uint32_t                            size_isp_out)
{
	int rc = -EINVAL;
	struct cam_isp_tfe_dual_config             *dual_config;
	struct cam_isp_hw_mgr_res                  *hw_mgr_res;
	struct cam_isp_resource_node               *res;
	struct cam_tfe_dual_update_args             dual_isp_update_args;
	uint32_t                                    outport_id;
	size_t                                      len = 0, remain_len = 0;
	uint32_t                                   *cpu_addr;
	uint32_t                                    i, j, stp_index;

	CAM_DBG(CAM_ISP, "cmd des size %d, length: %d",
		cmd_desc->size, cmd_desc->length);

	rc = cam_packet_util_get_cmd_mem_addr(
		cmd_desc->mem_handle, &cpu_addr, &len);
	if (rc) {
		CAM_DBG(CAM_ISP, "unable to get cmd mem addr handle:0x%x",
			cmd_desc->mem_handle);
		return rc;
	}

	if ((len < sizeof(struct cam_isp_tfe_dual_config)) ||
		(cmd_desc->offset >=
			(len - sizeof(struct cam_isp_tfe_dual_config)))) {
		CAM_ERR(CAM_ISP, "not enough buffer provided");
		return -EINVAL;
	}

	remain_len = len - cmd_desc->offset;
	cpu_addr += (cmd_desc->offset / 4);
	dual_config = (struct cam_isp_tfe_dual_config *)cpu_addr;

	if ((dual_config->num_ports *
		sizeof(struct cam_isp_tfe_dual_stripe_config)) >
		(remain_len -
			offsetof(struct cam_isp_tfe_dual_config, stripes))) {
		CAM_ERR(CAM_ISP, "not enough buffer for all the dual configs");
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "num_ports:%d", dual_config->num_ports);
	if (dual_config->num_ports >= size_isp_out) {
		CAM_ERR(CAM_UTIL,
			"inval num ports %d max num tfe ports:%d",
			dual_config->num_ports, size_isp_out);
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < dual_config->num_ports; i++) {
		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			stp_index = (i * CAM_PACKET_MAX_PLANES) +
				(j * (CAM_PACKET_MAX_PLANES *
				dual_config->num_ports));

			if (!dual_config->stripes[stp_index].port_id)
				continue;

			outport_id = dual_config->stripes[stp_index].port_id;
			if (outport_id >= size_isp_out) {
				CAM_ERR(CAM_UTIL,
					"inval outport id:%d i:%d j:%d num ports:%d ",
					outport_id, i, j,
					dual_config->num_ports);
					rc = -EINVAL;
					goto end;
			}

			hw_mgr_res = &res_list_isp_out[outport_id];
			if (!hw_mgr_res->hw_res[j])
				continue;

			if (hw_mgr_res->hw_res[j]->hw_intf->hw_idx != base_idx)
				continue;

			res = hw_mgr_res->hw_res[j];

			if (res->res_id < CAM_ISP_TFE_OUT_RES_BASE ||
				res->res_id >= CAM_ISP_TFE_OUT_RES_MAX) {
				CAM_DBG(CAM_ISP, "res id :%d", res->res_id);
				continue;
			}

			dual_isp_update_args.split_id = j;
			dual_isp_update_args.res      = res;
			dual_isp_update_args.stripe_config =
				&dual_config->stripes[stp_index];
			rc = res->hw_intf->hw_ops.process_cmd(
				res->hw_intf->hw_priv,
				CAM_ISP_HW_CMD_STRIPE_UPDATE,
				&dual_isp_update_args,
				sizeof(struct cam_tfe_dual_update_args));
			if (rc)
				goto end;
		}
	}

end:
	return rc;
}

int cam_tfe_add_command_buffers(
	struct cam_hw_prepare_update_args  *prepare,
	struct cam_kmd_buf_info            *kmd_buf_info,
	struct cam_isp_ctx_base_info       *base_info,
	cam_packet_generic_blob_handler     blob_handler_cb,
	struct cam_isp_hw_mgr_res          *res_list_isp_out,
	uint32_t                            size_isp_out)
{
	int rc = 0;
	uint32_t                           cmd_meta_data, num_ent, i;
	uint32_t                           base_idx;
	enum cam_isp_hw_split_id           split_id;
	struct cam_cmd_buf_desc           *cmd_desc = NULL;
	struct cam_hw_update_entry        *hw_entry = NULL;

	split_id = base_info->split_id;
	base_idx = base_info->idx;
	hw_entry = prepare->hw_update_entries;
	/*
	 * set the cmd_desc to point the first command descriptor in the
	 * packet
	 */
	cmd_desc = (struct cam_cmd_buf_desc *)
			((uint8_t *)&prepare->packet->payload +
			prepare->packet->cmd_buf_offset);

	CAM_DBG(CAM_ISP, "split id = %d, number of command buffers:%d",
		split_id, prepare->packet->num_cmd_buf);

	for (i = 0; i < prepare->packet->num_cmd_buf; i++) {
		num_ent = prepare->num_hw_update_entries;
		if (!cmd_desc[i].length)
			continue;

		/* One hw entry space required for left or right or common */
		if (num_ent + 1 >= prepare->max_hw_update_entries) {
			CAM_ERR(CAM_ISP, "Insufficient  HW entries :%d %d",
				num_ent, prepare->max_hw_update_entries);
			return -EINVAL;
		}

		rc = cam_packet_util_validate_cmd_desc(&cmd_desc[i]);
		if (rc)
			return rc;

		cmd_meta_data = cmd_desc[i].meta_data;

		CAM_DBG(CAM_ISP, "meta type: %d, split_id: %d",
			cmd_meta_data, split_id);

		switch (cmd_meta_data) {
		case CAM_ISP_TFE_PACKET_META_BASE:
		case CAM_ISP_TFE_PACKET_META_LEFT:
			if (split_id == CAM_ISP_HW_SPLIT_LEFT) {
				hw_entry[num_ent].len = cmd_desc[i].length;
				hw_entry[num_ent].handle =
					cmd_desc[i].mem_handle;
				hw_entry[num_ent].offset = cmd_desc[i].offset;
				hw_entry[num_ent].flags = CAM_ISP_IQ_BL;
				CAM_DBG(CAM_ISP,
					"Meta_Left num_ent=%d handle=0x%x, len=%u, offset=%u",
					num_ent,
					hw_entry[num_ent].handle,
					hw_entry[num_ent].len,
					hw_entry[num_ent].offset);

				num_ent++;
			}
			break;
		case CAM_ISP_TFE_PACKET_META_RIGHT:
			if (split_id == CAM_ISP_HW_SPLIT_RIGHT) {
				hw_entry[num_ent].len = cmd_desc[i].length;
				hw_entry[num_ent].handle =
					cmd_desc[i].mem_handle;
				hw_entry[num_ent].offset = cmd_desc[i].offset;
				hw_entry[num_ent].flags = CAM_ISP_IQ_BL;
				CAM_DBG(CAM_ISP,
					"Meta_Right num_ent=%d handle=0x%x, len=%u, offset=%u",
					num_ent,
					hw_entry[num_ent].handle,
					hw_entry[num_ent].len,
					hw_entry[num_ent].offset);

				num_ent++;
			}
			break;
		case CAM_ISP_TFE_PACKET_META_COMMON:
			hw_entry[num_ent].len = cmd_desc[i].length;
			hw_entry[num_ent].handle =
				cmd_desc[i].mem_handle;
			hw_entry[num_ent].offset = cmd_desc[i].offset;
			hw_entry[num_ent].flags = CAM_ISP_IQ_BL;
			CAM_DBG(CAM_ISP,
				"Meta_Common num_ent=%d handle=0x%x, len=%u, offset=%u",
				num_ent,
				hw_entry[num_ent].handle,
				hw_entry[num_ent].len,
				hw_entry[num_ent].offset);
			if (cmd_meta_data == CAM_ISP_PACKET_META_DMI_COMMON)
				hw_entry[num_ent].flags = 0x1;

			num_ent++;
			break;
		case CAM_ISP_TFE_PACKET_META_DUAL_CONFIG:

			rc = cam_tfe_update_dual_config(&cmd_desc[i], split_id,
				base_idx, res_list_isp_out, size_isp_out);

			if (rc)
				return rc;
			break;
		case CAM_ISP_TFE_PACKET_META_GENERIC_BLOB_COMMON: {
			struct cam_isp_generic_blob_info   blob_info;

			prepare->num_hw_update_entries = num_ent;
			blob_info.prepare = prepare;
			blob_info.base_info = base_info;
			blob_info.kmd_buf_info = kmd_buf_info;

			rc = cam_packet_util_process_generic_cmd_buffer(
				&cmd_desc[i],
				blob_handler_cb,
				&blob_info);
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Failed in processing blobs %d", rc);
				return rc;
			}
			hw_entry[num_ent].flags = CAM_ISP_IQ_BL;
			num_ent = prepare->num_hw_update_entries;
		}
			break;
		case CAM_ISP_TFE_PACKET_META_REG_DUMP_ON_FLUSH:
		case CAM_ISP_TFE_PACKET_META_REG_DUMP_ON_ERROR:
		case CAM_ISP_TFE_PACKET_META_REG_DUMP_PER_REQUEST:
			if (split_id == CAM_ISP_HW_SPLIT_LEFT) {
				if (prepare->num_reg_dump_buf >=
					CAM_REG_DUMP_MAX_BUF_ENTRIES) {
					CAM_ERR(CAM_ISP,
					"Descriptor count out of bounds: %d",
					prepare->num_reg_dump_buf);
					return -EINVAL;
				}
				prepare->reg_dump_buf_desc[
					prepare->num_reg_dump_buf] =
					cmd_desc[i];
				prepare->num_reg_dump_buf++;
				CAM_DBG(CAM_ISP,
					"Added command buffer: %d desc_count: %d",
					cmd_desc[i].meta_data,
					prepare->num_reg_dump_buf);
			}
			break;
		default:
			CAM_ERR(CAM_ISP, "invalid cdm command meta data %d",
				cmd_meta_data);
			return -EINVAL;
		}
		prepare->num_hw_update_entries = num_ent;
	}

	return rc;
}

static int cam_tfe_mgr_prepare_hw_update(void *hw_mgr_priv,
	void *prepare_hw_update_args)
{
	int rc = 0;
	struct cam_hw_prepare_update_args       *prepare =
		(struct cam_hw_prepare_update_args *) prepare_hw_update_args;
	struct cam_tfe_hw_mgr_ctx               *ctx;
	struct cam_tfe_hw_mgr                   *hw_mgr;
	struct cam_kmd_buf_info                  kmd_buf;
	uint32_t                                 i;
	bool                                     fill_fence = true;
	struct cam_isp_prepare_hw_update_data   *prepare_hw_data;
	struct cam_isp_frame_header_info         frame_header_info;
	struct cam_isp_change_base_args          change_base_info = {0};

	if (!hw_mgr_priv || !prepare_hw_update_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	CAM_DBG(CAM_REQ, "Enter for req_id %lld",
		prepare->packet->header.request_id);

	prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
		prepare->priv;

	ctx = (struct cam_tfe_hw_mgr_ctx *) prepare->ctxt_to_hw_map;
	hw_mgr = (struct cam_tfe_hw_mgr *)hw_mgr_priv;

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
		CAM_TFE_HW_NUM_MAX);
	memset(&prepare_hw_data->bw_config_valid[0], 0x0,
		sizeof(prepare_hw_data->bw_config_valid[0]) *
		CAM_TFE_HW_NUM_MAX);

	for (i = 0; i < ctx->num_base; i++) {
		CAM_DBG(CAM_ISP, "process cmd buffer for device %d", i);

		CAM_DBG(CAM_ISP,
			"change base i=%d, idx=%d",
			i, ctx->base[i].idx);

		change_base_info.base_idx = ctx->base[i].idx;
		change_base_info.cdm_id = CAM_CDM_MAX;

		/* Add change base */
		rc = cam_isp_add_change_base(prepare, &ctx->res_list_tfe_in,
			&change_base_info, &kmd_buf);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed in change base i=%d, idx=%d, rc=%d",
				i, ctx->base[i].idx, rc);
			goto end;
		}


		/* get command buffers */
		if (ctx->base[i].split_id != CAM_ISP_HW_SPLIT_MAX) {
			rc = cam_tfe_add_command_buffers(prepare, &kmd_buf,
				&ctx->base[i],
				cam_isp_tfe_packet_generic_blob_handler,
				ctx->res_list_tfe_out, CAM_TFE_HW_OUT_RES_MAX);
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Failed in add cmdbuf, i=%d, split_id=%d, rc=%d",
					i, ctx->base[i].split_id, rc);
				goto end;
			}
		}

		memset(&frame_header_info, 0,
			sizeof(struct cam_isp_frame_header_info));
		frame_header_info.frame_header_enable = false;

		/* get IO buffers */
		rc = cam_isp_add_io_buffers(hw_mgr->mgr_common.img_iommu_hdl,
			hw_mgr->mgr_common.img_iommu_hdl_secure,
			prepare, ctx->base[i].idx,
			&kmd_buf, ctx->res_list_tfe_out,
			NULL,
			CAM_TFE_HW_OUT_RES_MAX, fill_fence,
			&frame_header_info);

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

	CAM_DBG(CAM_ISP,
		"num_reg_dump_buf=%d ope code:%d",
		prepare->num_reg_dump_buf, prepare->packet->header.op_code);

	/* reg update will be done later for the initial configure */
	if (((prepare->packet->header.op_code) & 0xF) ==
		CAM_ISP_PACKET_INIT_DEV) {
		prepare_hw_data->packet_opcode_type =
			CAM_ISP_TFE_PACKET_INIT_DEV;

		if ((!prepare->num_reg_dump_buf) || (prepare->num_reg_dump_buf >
			CAM_REG_DUMP_MAX_BUF_ENTRIES))
			goto end;

		if (!ctx->num_reg_dump_buf) {
			ctx->num_reg_dump_buf =
				prepare->num_reg_dump_buf;
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
	} else  {
		prepare_hw_data->packet_opcode_type =
		CAM_ISP_TFE_PACKET_CONFIG_DEV;
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
		change_base_info.base_idx = ctx->base[i].idx;
		change_base_info.cdm_id = CAM_CDM_MAX;
		/* Add change base */
		rc = cam_isp_add_change_base(prepare, &ctx->res_list_tfe_in,
			&change_base_info, &kmd_buf);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed in change base adding reg_update cmd i=%d, idx=%d, rc=%d",
				i, ctx->base[i].idx, rc);
			goto end;
		}

		/*Add reg update */
		rc = cam_isp_add_reg_update(prepare, &ctx->res_list_tfe_in,
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

static int cam_tfe_mgr_resume_hw(struct cam_tfe_hw_mgr_ctx *ctx)
{
	return cam_tfe_mgr_bw_control(ctx, CAM_TFE_BW_CONTROL_INCLUDE);
}

static int cam_tfe_mgr_sof_irq_debug(
	struct cam_tfe_hw_mgr_ctx *ctx,
	uint32_t sof_irq_enable)
{
	int rc = 0;
	uint32_t i = 0;
	struct cam_isp_hw_mgr_res     *hw_mgr_res = NULL;
	struct cam_hw_intf            *hw_intf = NULL;
	struct cam_isp_resource_node  *rsrc_node = NULL;

	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf->hw_ops.process_cmd) {
				rc |= hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_TFE_CSID_SOF_IRQ_DEBUG,
					&sof_irq_enable,
					sizeof(sof_irq_enable));
			}
		}
	}

	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			rsrc_node = hw_mgr_res->hw_res[i];
			if (rsrc_node->process_cmd && (rsrc_node->res_id ==
				CAM_ISP_HW_TFE_IN_CAMIF)) {
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

static void cam_tfe_mgr_print_io_bufs(struct cam_tfe_hw_mgr  *hw_mgr,
		uint32_t res_id, struct cam_packet *packet,
		bool    *ctx_found, struct cam_tfe_hw_mgr_ctx *ctx)
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

static void cam_tfe_mgr_pf_dump(uint32_t res_id,
	struct cam_tfe_hw_mgr_ctx *ctx)
{
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_hw_intf               *hw_intf;
	struct cam_isp_hw_get_cmd_update  cmd_update;
	uint32_t                          res_id_out;
	int  i, rc = 0;

	/* dump the registers  */
	rc = cam_tfe_mgr_handle_reg_dump(ctx, ctx->reg_dump_buf_desc,
		ctx->num_reg_dump_buf,
		CAM_ISP_TFE_PACKET_META_REG_DUMP_ON_ERROR, NULL, false);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Reg dump on pf failed req id: %llu rc: %d",
			ctx->applied_req_id, rc);
	}

	/* dump the acquire data */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf && hw_intf->hw_ops.process_cmd) {
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_TFE_CSID_LOG_ACQUIRE_DATA,
					hw_mgr_res->hw_res[i],
					sizeof(void *));
				if (rc)
					CAM_ERR(CAM_ISP,
						"acquire dump data failed");
			} else
				CAM_ERR(CAM_ISP, "NULL hw_intf!");
		}
	}

	res_id_out = res_id & 0xFF;

	if (res_id_out >= CAM_TFE_HW_OUT_RES_MAX) {
		CAM_ERR(CAM_ISP, "Invalid out resource id :%x",
			res_id);
		return;
	}

	hw_mgr_res =
		&ctx->res_list_tfe_out[res_id_out];
	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!hw_mgr_res->hw_res[i])
			continue;

		cmd_update.cmd_type = CAM_ISP_HW_CMD_DUMP_BUS_INFO;
		cmd_update.res = hw_mgr_res->hw_res[i];
		hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.process_cmd) {
			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_ISP_HW_CMD_DUMP_BUS_INFO,
				(void *)&cmd_update,
				sizeof(struct cam_isp_hw_get_cmd_update));
		}
	}
}

static void cam_tfe_mgr_dump_pf_data(
	struct cam_tfe_hw_mgr  *hw_mgr,
	struct cam_hw_cmd_args *hw_cmd_args)
{
	struct cam_tfe_hw_mgr_ctx           *ctx;
	struct cam_isp_hw_mgr_res           *hw_mgr_res;
	struct cam_isp_hw_get_cmd_update     cmd_update;
	struct cam_isp_hw_get_res_for_mid    get_res;
	struct cam_packet                   *packet;
	uint32_t  *resource_type;
	uint32_t   hw_id;
	bool      *ctx_found, hw_id_found = false;
	int        i, j, rc = 0;

	ctx = (struct cam_tfe_hw_mgr_ctx *)hw_cmd_args->ctxt_to_hw_map;

	packet  = hw_cmd_args->u.pf_args.pf_data.packet;
	ctx_found = hw_cmd_args->u.pf_args.ctx_found;
	resource_type = hw_cmd_args->u.pf_args.resource_type;

	if ((*ctx_found) && (*resource_type))
		goto outportlog;

	for (i = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
		if (!g_tfe_hw_mgr.tfe_devices[i])
			continue;

		for (j = 0; j < g_tfe_hw_mgr.tfe_devices[i]->num_hw_pid; j++) {
			if (g_tfe_hw_mgr.tfe_devices[i]->hw_pid[j] ==
				hw_cmd_args->u.pf_args.pid) {
				hw_id_found = true;
				hw_id = i;
				break;
			}
		}
		if (hw_id_found)
			break;
	}

	if (i == CAM_TFE_HW_NUM_MAX) {
		CAM_INFO(CAM_ISP,
			"PID:%d  is not matching with any TFE HW PIDs ctx id:%d",
			hw_cmd_args->u.pf_args.pid,  ctx->ctx_index);
		return;
	}

	for (i = 0; i < ctx->num_base; i++) {
		if (ctx->base[i].idx == hw_id) {
			*ctx_found = true;
			break;
		}
	}

	if (!(*ctx_found)) {
		CAM_INFO(CAM_ISP,
			"This context does not cause pf:pid:%d hw id:%d ctx_id:%d",
			hw_cmd_args->u.pf_args.pid, hw_id, ctx->ctx_index);
		return;
	}

	for (i = 0; i < CAM_TFE_HW_OUT_RES_MAX; i++) {
		hw_mgr_res = &ctx->res_list_tfe_out[i];
		if (!hw_mgr_res->hw_res[0])
			continue;

		break;
	}

	if (i >= CAM_TFE_HW_OUT_RES_MAX) {
		CAM_ERR(CAM_ISP,
			"NO valid outport resources ctx id:%d req id:%lld",
			ctx->ctx_index, packet->header.request_id);
		return;
	}

	get_res.mid = hw_cmd_args->u.pf_args.mid;
	cmd_update.res = hw_mgr_res->hw_res[0];
	cmd_update.cmd_type = CAM_ISP_HW_CMD_GET_RES_FOR_MID;
	cmd_update.data = (void *) &get_res;

	/* get resource id for given mid */
	rc = hw_mgr_res->hw_res[0]->hw_intf->hw_ops.process_cmd(
		hw_mgr_res->hw_res[0]->hw_intf->hw_priv,
		cmd_update.cmd_type, &cmd_update,
		sizeof(struct cam_isp_hw_get_cmd_update));

	if (rc) {
		CAM_ERR(CAM_ISP,
			"getting mid port resource id failed ctx id:%d req id:%lld",
			ctx->ctx_index, packet->header.request_id);
		return;
	}
	CAM_ERR(CAM_ISP,
		"Page fault on resource id:0x%x ctx id:%d req id:%lld",
		get_res.out_res_id, ctx->ctx_index, packet->header.request_id);
	*resource_type = get_res.out_res_id;

	cam_tfe_mgr_pf_dump(get_res.out_res_id, ctx);

outportlog:
	cam_tfe_mgr_print_io_bufs(hw_mgr, *resource_type, packet,
		ctx_found, ctx);


}

static int cam_tfe_mgr_cmd(void *hw_mgr_priv, void *cmd_args)
{
	int rc = 0;
	struct cam_hw_cmd_args *hw_cmd_args = cmd_args;
	struct cam_tfe_hw_mgr  *hw_mgr = hw_mgr_priv;
	struct cam_tfe_hw_mgr_ctx *ctx = (struct cam_tfe_hw_mgr_ctx *)
		hw_cmd_args->ctxt_to_hw_map;
	struct cam_isp_hw_cmd_args *isp_hw_cmd_args = NULL;
	struct cam_packet          *packet;

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
			cam_tfe_mgr_pause_hw(ctx);
			break;
		case CAM_ISP_HW_MGR_CMD_RESUME_HW:
			cam_tfe_mgr_resume_hw(ctx);
			break;
		case CAM_ISP_HW_MGR_CMD_SOF_DEBUG:
			cam_tfe_mgr_sof_irq_debug(ctx,
				isp_hw_cmd_args->u.sof_irq_enable);
			break;
		case CAM_ISP_HW_MGR_CMD_CTX_TYPE:
			if (ctx->is_rdi_only_context)
				isp_hw_cmd_args->u.ctx_type = CAM_ISP_CTX_RDI;
			else
				isp_hw_cmd_args->u.ctx_type = CAM_ISP_CTX_PIX;
			break;
		case CAM_ISP_HW_MGR_GET_PACKET_OPCODE:
			packet = (struct cam_packet *)
				isp_hw_cmd_args->cmd_data;
			if ((packet->header.op_code & 0xF) ==
				CAM_ISP_TFE_PACKET_INIT_DEV)
				isp_hw_cmd_args->u.packet_op_code =
				CAM_ISP_TFE_PACKET_INIT_DEV;
			else
				isp_hw_cmd_args->u.packet_op_code =
				CAM_ISP_TFE_PACKET_CONFIG_DEV;
			break;
		case CAM_ISP_HW_MGR_GET_LAST_CDM_DONE:
			isp_hw_cmd_args->u.last_cdm_done =
				ctx->last_cdm_done_req;
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid HW mgr command:0x%x",
				hw_cmd_args->cmd_type);
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_HW_MGR_CMD_DUMP_PF_INFO:
		cam_tfe_mgr_dump_pf_data(hw_mgr, hw_cmd_args);
		break;
	case CAM_HW_MGR_CMD_REG_DUMP_ON_FLUSH:
		if (ctx->last_dump_flush_req_id == ctx->applied_req_id)
			return 0;

		ctx->last_dump_flush_req_id = ctx->applied_req_id;

		rc = cam_tfe_mgr_handle_reg_dump(ctx, ctx->reg_dump_buf_desc,
			ctx->num_reg_dump_buf,
			CAM_ISP_TFE_PACKET_META_REG_DUMP_ON_FLUSH,
			NULL, false);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Reg dump on flush failed req id: %llu num_reg_dump:0x%x rc: %d",
				ctx->applied_req_id, ctx->num_reg_dump_buf, rc);
			return rc;
		}

		break;
	case CAM_HW_MGR_CMD_REG_DUMP_ON_ERROR:
		if (ctx->last_dump_err_req_id == ctx->applied_req_id)
			return 0;

		ctx->last_dump_err_req_id = ctx->applied_req_id;
		rc = cam_tfe_mgr_handle_reg_dump(ctx, ctx->reg_dump_buf_desc,
			ctx->num_reg_dump_buf,
			CAM_ISP_TFE_PACKET_META_REG_DUMP_ON_ERROR,
			NULL, false);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Reg dump on error failed req id:%llu num_reg_dump:0x%x rc: %d",
				ctx->applied_req_id, ctx->num_reg_dump_buf, rc);
			return rc;
		}
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid cmd");
	}

	return rc;
}

static int cam_tfe_mgr_cmd_get_sof_timestamp(
	struct cam_tfe_hw_mgr_ctx            *tfe_ctx,
	uint64_t                             *time_stamp,
	uint64_t                             *boot_time_stamp)
{
	int                                        rc = -EINVAL;
	uint32_t                                   i;
	struct cam_isp_hw_mgr_res                 *hw_mgr_res;
	struct cam_hw_intf                        *hw_intf;
	struct cam_tfe_csid_get_time_stamp_args    csid_get_time;

	hw_mgr_res = list_first_entry(&tfe_ctx->res_list_tfe_csid,
		struct cam_isp_hw_mgr_res, list);

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!hw_mgr_res->hw_res[i])
			continue;

		/*
		 * Get the SOF time stamp from left resource only.
		 * Left resource is master for dual tfe case and
		 * Rdi only context case left resource only hold
		 * the RDI resource
		 */

		hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.process_cmd) {
			/*
			 * Single TFE case, Get the time stamp from
			 * available one csid hw in the context
			 * Dual TFE case, get the time stamp from
			 * master(left) would be sufficient
			 */

			csid_get_time.node_res =
				hw_mgr_res->hw_res[i];
			rc = hw_intf->hw_ops.process_cmd(
				hw_intf->hw_priv,
				CAM_TFE_CSID_CMD_GET_TIME_STAMP,
				&csid_get_time,
				sizeof(struct
				cam_tfe_csid_get_time_stamp_args));
			if (!rc && (i == CAM_ISP_HW_SPLIT_LEFT)) {
				*time_stamp =
					csid_get_time.time_stamp_val;
				*boot_time_stamp =
					csid_get_time.boot_timestamp;
				break;
			}
		}
	}

	if (rc)
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Getting sof time stamp failed");

	return rc;
}

static void cam_tfe_mgr_ctx_reg_dump(struct cam_tfe_hw_mgr_ctx  *ctx)
{
	struct cam_isp_hw_mgr_res        *hw_mgr_res;
	struct cam_hw_intf               *hw_intf;
	struct cam_isp_hw_get_cmd_update  cmd_update;
	int i = 0;


	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_in,
		list) {
		if (hw_mgr_res->res_type == CAM_ISP_RESOURCE_UNINT)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			switch (hw_mgr_res->hw_res[i]->res_id) {
			case CAM_ISP_HW_TFE_IN_CAMIF:
				hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
				cmd_update.res = hw_mgr_res->hw_res[i];
				cmd_update.cmd_type =
					CAM_ISP_HW_CMD_GET_REG_DUMP;
				hw_intf->hw_ops.process_cmd(hw_intf->hw_priv,
					CAM_ISP_HW_CMD_GET_REG_DUMP,
					&cmd_update, sizeof(cmd_update));
				break;
			default:
				break;
			}
		}
	}

	/* Dump the TFE CSID registers */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid,
		list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf->hw_ops.process_cmd) {
				hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_TFE_CSID_CMD_GET_REG_DUMP,
					hw_mgr_res->hw_res[i],
					sizeof(struct cam_isp_resource_node));
			}
		}
	}
}

static int cam_tfe_mgr_process_recovery_cb(void *priv, void *data)
{
	int32_t rc = 0;
	struct cam_tfe_hw_event_recovery_data   *recovery_data = data;
	struct cam_hw_start_args             start_args;
	struct cam_hw_stop_args              stop_args;
	struct cam_tfe_hw_mgr               *tfe_hw_mgr = priv;
	struct cam_isp_hw_mgr_res           *hw_mgr_res;
	struct cam_tfe_hw_mgr_ctx           *tfe_hw_mgr_ctx;
	uint32_t                             i = 0;

	uint32_t error_type = recovery_data->error_type;
	struct cam_tfe_hw_mgr_ctx        *ctx = NULL;

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
			tfe_hw_mgr_ctx = recovery_data->affected_ctx[i];

			if (g_tfe_hw_mgr.debug_cfg.enable_reg_dump)
				cam_tfe_mgr_ctx_reg_dump(tfe_hw_mgr_ctx);

			if (g_tfe_hw_mgr.debug_cfg.enable_recovery) {
				rc = cam_tfe_mgr_stop_hw_in_overflow(
					&stop_args);
				if (rc) {
					CAM_ERR(CAM_ISP,
						"CTX stop failed(%d)", rc);
					return rc;
				}
			}
		}

		if (!g_tfe_hw_mgr.debug_cfg.enable_recovery) {
			CAM_INFO(CAM_ISP, "reg dumping is done ");
			return 0;
		}

		CAM_DBG(CAM_ISP, "RESET: CSID PATH");
		for (i = 0; i < recovery_data->no_of_context; i++) {
			ctx = recovery_data->affected_ctx[i];
			list_for_each_entry(hw_mgr_res, &ctx->res_list_tfe_csid,
				list) {
				rc = cam_tfe_hw_mgr_reset_csid_res(hw_mgr_res);
				if (rc) {
					CAM_ERR(CAM_ISP, "Failed RESET (%d)",
						hw_mgr_res->res_id);
					return rc;
				}
			}
		}

		CAM_DBG(CAM_ISP, "RESET: Calling TFE reset");

		for (i = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
			if (recovery_data->affected_core[i])
				cam_tfe_mgr_reset_tfe_hw(tfe_hw_mgr, i);
		}

		CAM_DBG(CAM_ISP, "START: Number of affected context: %d",
			recovery_data->no_of_context);

		for (i = 0; i < recovery_data->no_of_context; i++) {
			ctx =  recovery_data->affected_ctx[i];
			start_args.ctxt_to_hw_map = ctx;

			atomic_set(&ctx->overflow_pending, 0);

			rc = cam_tfe_mgr_restart_hw(&start_args);
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

static int cam_tfe_hw_mgr_do_error_recovery(
	struct cam_tfe_hw_event_recovery_data  *tfe_mgr_recovery_data)
{
	int32_t                             rc = 0;
	struct crm_workq_task              *task = NULL;
	struct cam_tfe_hw_event_recovery_data  *recovery_data = NULL;

	recovery_data = kmemdup(tfe_mgr_recovery_data,
		sizeof(struct cam_tfe_hw_event_recovery_data), GFP_ATOMIC);

	if (!recovery_data)
		return -ENOMEM;

	CAM_DBG(CAM_ISP, "Enter: error_type (%d)", recovery_data->error_type);

	task = cam_req_mgr_workq_get_task(g_tfe_hw_mgr.workq);
	if (!task) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No empty task frame");
		kfree(recovery_data);
		return -ENOMEM;
	}

	task->process_cb = &cam_tfe_mgr_process_recovery_cb;
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
static bool cam_tfe_hw_mgr_is_ctx_affected(
	struct cam_tfe_hw_mgr_ctx   *tfe_hwr_mgr_ctx,
	uint32_t                    *affected_core,
	uint32_t                     size)
{

	bool                  rc = false;
	uint32_t              i = 0, j = 0;
	uint32_t              max_idx =  tfe_hwr_mgr_ctx->num_base;
	uint32_t              ctx_affected_core_idx[CAM_TFE_HW_NUM_MAX] = {0};

	CAM_DBG(CAM_ISP, "Enter:max_idx = %d", max_idx);

	if ((max_idx >= CAM_TFE_HW_NUM_MAX) || (size > CAM_TFE_HW_NUM_MAX)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "invalid parameter = %d", max_idx);
		return rc;
	}

	for (i = 0; i < max_idx; i++) {
		if (affected_core[tfe_hwr_mgr_ctx->base[i].idx])
			rc = true;
		else {
			ctx_affected_core_idx[j] = tfe_hwr_mgr_ctx->base[i].idx;
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
 * For any dual TFE context, if non-affected TFE is also serving
 * another context, then that context should also be notified with fatal error
 * So Loop through each context and -
 *   a. match core_idx
 *   b. Notify CTX with fatal error
 */
static int  cam_tfe_hw_mgr_find_affected_ctx(
	struct cam_isp_hw_error_event_data    *error_event_data,
	uint32_t                               curr_core_idx,
	struct cam_tfe_hw_event_recovery_data     *recovery_data)
{
	uint32_t affected_core[CAM_TFE_HW_NUM_MAX] = {0};
	struct cam_tfe_hw_mgr_ctx   *tfe_hwr_mgr_ctx = NULL;
	cam_hw_event_cb_func         notify_err_cb;
	struct cam_tfe_hw_mgr       *tfe_hwr_mgr = NULL;
	enum cam_isp_hw_event_type   event_type = CAM_ISP_HW_EVENT_ERROR;
	uint32_t i = 0;

	if (!recovery_data) {
		CAM_ERR(CAM_ISP, "recovery_data parameter is NULL");
		return -EINVAL;
	}

	recovery_data->no_of_context = 0;
	affected_core[curr_core_idx] = 1;
	tfe_hwr_mgr = &g_tfe_hw_mgr;

	list_for_each_entry(tfe_hwr_mgr_ctx,
		&tfe_hwr_mgr->used_ctx_list, list) {
		/*
		 * Check if current core_idx matches the HW associated
		 * with this context
		 */
		if (!cam_tfe_hw_mgr_is_ctx_affected(tfe_hwr_mgr_ctx,
			affected_core, CAM_TFE_HW_NUM_MAX))
			continue;

		if (atomic_read(&tfe_hwr_mgr_ctx->overflow_pending)) {
			CAM_INFO(CAM_ISP, "CTX:%d already error reported",
				tfe_hwr_mgr_ctx->ctx_index);
			continue;
		}

		atomic_set(&tfe_hwr_mgr_ctx->overflow_pending, 1);
		notify_err_cb = tfe_hwr_mgr_ctx->common.event_cb[event_type];

		/* Add affected_context in list of recovery data */
		CAM_DBG(CAM_ISP, "Add affected ctx %d to list",
			tfe_hwr_mgr_ctx->ctx_index);
		if (recovery_data->no_of_context < CAM_TFE_CTX_MAX)
			recovery_data->affected_ctx[
				recovery_data->no_of_context++] =
				tfe_hwr_mgr_ctx;

		/*
		 * In the call back function corresponding ISP context
		 * will update CRM about fatal Error
		 */
		if (notify_err_cb) {
			notify_err_cb(tfe_hwr_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_ERROR, error_event_data);
		} else {
			CAM_WARN(CAM_ISP, "Error call back is not set");
			goto end;
		}
	}

	/* fill the affected_core in recovery data */
	for (i = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
		recovery_data->affected_core[i] = affected_core[i];
		CAM_DBG(CAM_ISP, "tfe core %d is affected (%d)",
			 i, recovery_data->affected_core[i]);
	}
end:
	return 0;
}

static int cam_tfe_hw_mgr_handle_csid_event(
	struct cam_isp_hw_event_info *event_info)
{
	struct cam_isp_hw_error_event_data  error_event_data = {0};
	struct cam_tfe_hw_event_recovery_data     recovery_data = {0};

	/* this can be extended based on the types of error
	 * received from CSID
	 */
	switch (event_info->err_type) {
	case CAM_ISP_HW_ERROR_CSID_FATAL: {

		if (!g_tfe_hw_mgr.debug_cfg.enable_csid_recovery)
			break;

		error_event_data.error_type = event_info->err_type;
		cam_tfe_hw_mgr_find_affected_ctx(&error_event_data,
			event_info->hw_idx,
			&recovery_data);
		break;
	}
	default:
		break;
	}
	return 0;
}

static int cam_tfe_hw_mgr_handle_hw_err(
	void                                *evt_info)
{
	struct cam_isp_hw_event_info            *event_info = evt_info;
	struct cam_isp_hw_error_event_data       error_event_data = {0};
	struct cam_tfe_hw_event_recovery_data    recovery_data = {0};
	int    rc = -EINVAL;
	uint32_t core_idx;

	if (event_info->err_type == CAM_TFE_IRQ_STATUS_VIOLATION)
		error_event_data.error_type = CAM_ISP_HW_ERROR_VIOLATION;
	else if (event_info->res_type == CAM_ISP_RESOURCE_TFE_IN ||
		event_info->res_type == CAM_ISP_RESOURCE_PIX_PATH)
		error_event_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;
	else if (event_info->res_type == CAM_ISP_RESOURCE_TFE_OUT)
		error_event_data.error_type = CAM_ISP_HW_ERROR_BUSIF_OVERFLOW;

	spin_lock(&g_tfe_hw_mgr.ctx_lock);
	if (event_info->err_type == CAM_ISP_HW_ERROR_CSID_FATAL) {
		rc = cam_tfe_hw_mgr_handle_csid_event(event_info);
		spin_unlock(&g_tfe_hw_mgr.ctx_lock);
		return rc;
	}

	core_idx = event_info->hw_idx;

	if (g_tfe_hw_mgr.debug_cfg.enable_recovery)
		error_event_data.recovery_enabled = true;
	else
		error_event_data.recovery_enabled = false;

	rc = cam_tfe_hw_mgr_find_affected_ctx(&error_event_data,
		core_idx, &recovery_data);
	if (rc || !(recovery_data.no_of_context))
		goto end;

	if (event_info->res_type == CAM_ISP_RESOURCE_TFE_OUT) {
		spin_unlock(&g_tfe_hw_mgr.ctx_lock);
		return rc;
	}

	if (g_tfe_hw_mgr.debug_cfg.enable_recovery) {
		/* Trigger for recovery */
		if (event_info->err_type == CAM_TFE_IRQ_STATUS_VIOLATION)
			recovery_data.error_type = CAM_ISP_HW_ERROR_VIOLATION;
		else
			recovery_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;
		cam_tfe_hw_mgr_do_error_recovery(&recovery_data);
	} else {
		CAM_DBG(CAM_ISP, "recovery is not enabled");
		rc = 0;
	}
end:
	spin_unlock(&g_tfe_hw_mgr.ctx_lock);
	return rc;
}

static int cam_tfe_hw_mgr_handle_hw_rup(
	void                                    *ctx,
	void                                    *evt_info)
{
	struct cam_isp_hw_event_info            *event_info = evt_info;
	struct cam_tfe_hw_mgr_ctx               *tfe_hw_mgr_ctx = ctx;
	cam_hw_event_cb_func                     tfe_hwr_irq_rup_cb;
	struct cam_isp_hw_reg_update_event_data  rup_event_data;

	tfe_hwr_irq_rup_cb =
		tfe_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_REG_UPDATE];

	switch (event_info->res_id) {
	case CAM_ISP_HW_TFE_IN_CAMIF:
		if (tfe_hw_mgr_ctx->is_dual)
			if (event_info->hw_idx != tfe_hw_mgr_ctx->master_hw_idx)
				break;

		if (atomic_read(&tfe_hw_mgr_ctx->overflow_pending))
			break;

		tfe_hwr_irq_rup_cb(tfe_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_REG_UPDATE, &rup_event_data);
		break;

	case CAM_ISP_HW_TFE_IN_RDI0:
	case CAM_ISP_HW_TFE_IN_RDI1:
	case CAM_ISP_HW_TFE_IN_RDI2:
		if (!tfe_hw_mgr_ctx->is_rdi_only_context)
			break;
		if (atomic_read(&tfe_hw_mgr_ctx->overflow_pending))
			break;
		tfe_hwr_irq_rup_cb(tfe_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_REG_UPDATE, &rup_event_data);
		break;

	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "RUP done for TFE source %d",
		event_info->res_id);

	return 0;
}

static int cam_tfe_hw_mgr_handle_hw_epoch(
	void                                 *ctx,
	void                                 *evt_info)
{
	struct cam_isp_hw_event_info         *event_info = evt_info;
	struct cam_tfe_hw_mgr_ctx            *tfe_hw_mgr_ctx = ctx;
	cam_hw_event_cb_func                  tfe_hw_irq_epoch_cb;
	struct cam_isp_hw_epoch_event_data    epoch_done_event_data;

	tfe_hw_irq_epoch_cb =
		tfe_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_EPOCH];

	switch (event_info->res_id) {
	case CAM_ISP_HW_TFE_IN_CAMIF:
		if (atomic_read(&tfe_hw_mgr_ctx->overflow_pending))
			break;
		tfe_hw_irq_epoch_cb(tfe_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_EPOCH, &epoch_done_event_data);
		break;

	case CAM_ISP_HW_TFE_IN_RDI0:
	case CAM_ISP_HW_TFE_IN_RDI1:
	case CAM_ISP_HW_TFE_IN_RDI2:
		break;

	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "Epoch for TFE source %d", event_info->res_id);

	return 0;
}

static int cam_tfe_hw_mgr_handle_hw_sof(
	void                                 *ctx,
	void                                 *evt_info)
{
	struct cam_isp_hw_event_info         *event_info = evt_info;
	struct cam_tfe_hw_mgr_ctx            *tfe_hw_mgr_ctx = ctx;
	cam_hw_event_cb_func                  tfe_hw_irq_sof_cb;
	struct cam_isp_hw_sof_event_data      sof_done_event_data;

	tfe_hw_irq_sof_cb =
		tfe_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_SOF];

	switch (event_info->res_id) {
	case CAM_ISP_HW_TFE_IN_CAMIF:
		cam_tfe_mgr_cmd_get_sof_timestamp(tfe_hw_mgr_ctx,
			&sof_done_event_data.timestamp,
			&sof_done_event_data.boot_time);

		if (atomic_read(&tfe_hw_mgr_ctx->overflow_pending))
			break;

		tfe_hw_irq_sof_cb(tfe_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_SOF, &sof_done_event_data);

		break;

	case CAM_ISP_HW_TFE_IN_RDI0:
	case CAM_ISP_HW_TFE_IN_RDI1:
	case CAM_ISP_HW_TFE_IN_RDI2:
		if (!tfe_hw_mgr_ctx->is_rdi_only_context)
			break;
		cam_tfe_mgr_cmd_get_sof_timestamp(tfe_hw_mgr_ctx,
			&sof_done_event_data.timestamp,
			&sof_done_event_data.boot_time);
		if (atomic_read(&tfe_hw_mgr_ctx->overflow_pending))
			break;
		tfe_hw_irq_sof_cb(tfe_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_SOF, &sof_done_event_data);
		break;

	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "SOF for TFE source %d", event_info->res_id);

	return 0;
}

static int cam_tfe_hw_mgr_handle_hw_eof(
	void                                 *ctx,
	void                                 *evt_info)
{
	struct cam_isp_hw_event_info         *event_info = evt_info;
	struct cam_tfe_hw_mgr_ctx            *tfe_hw_mgr_ctx = ctx;
	cam_hw_event_cb_func                  tfe_hw_irq_eof_cb;
	struct cam_isp_hw_eof_event_data      eof_done_event_data;

	tfe_hw_irq_eof_cb =
		tfe_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_EOF];

	switch (event_info->res_id) {
	case CAM_ISP_HW_TFE_IN_CAMIF:
		if (atomic_read(&tfe_hw_mgr_ctx->overflow_pending))
			break;
		tfe_hw_irq_eof_cb(tfe_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_EOF, &eof_done_event_data);

		break;

	case CAM_ISP_HW_TFE_IN_RDI0:
	case CAM_ISP_HW_TFE_IN_RDI1:
	case CAM_ISP_HW_TFE_IN_RDI2:
		break;

	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid res_id: %d",
			event_info->res_id);
		break;
	}

	CAM_DBG(CAM_ISP, "EOF for out_res->res_id: 0x%x",
		event_info->res_id);

	return 0;
}

static int cam_tfe_hw_mgr_handle_hw_buf_done(
	void                                *ctx,
	void                                *evt_info)
{
	cam_hw_event_cb_func                 tfe_hwr_irq_wm_done_cb;
	struct cam_tfe_hw_mgr_ctx           *tfe_hw_mgr_ctx = ctx;
	struct cam_isp_hw_done_event_data    buf_done_event_data = {0};
	struct cam_isp_hw_event_info        *event_info = evt_info;

	tfe_hwr_irq_wm_done_cb =
		tfe_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_DONE];

	buf_done_event_data.num_handles = 1;
	buf_done_event_data.resource_handle[0] = event_info->res_id;
	buf_done_event_data.last_consumed_addr[0] = event_info->reg_val;

	if (atomic_read(&tfe_hw_mgr_ctx->overflow_pending))
		return 0;

	if (buf_done_event_data.num_handles > 0 && tfe_hwr_irq_wm_done_cb) {
		CAM_DBG(CAM_ISP, "Notify ISP context");
		tfe_hwr_irq_wm_done_cb(tfe_hw_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_DONE, &buf_done_event_data);
	}

	CAM_DBG(CAM_ISP, "Buf done for out_res->res_id: 0x%x",
		event_info->res_id);

	return 0;
}

static int cam_tfe_hw_mgr_event_handler(
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
		rc = cam_tfe_hw_mgr_handle_hw_sof(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_REG_UPDATE:
		rc = cam_tfe_hw_mgr_handle_hw_rup(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_EPOCH:
		rc = cam_tfe_hw_mgr_handle_hw_epoch(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_EOF:
		rc = cam_tfe_hw_mgr_handle_hw_eof(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_DONE:
		rc = cam_tfe_hw_mgr_handle_hw_buf_done(priv, evt_info);
		break;

	case CAM_ISP_HW_EVENT_ERROR:
		rc = cam_tfe_hw_mgr_handle_hw_err(evt_info);
		break;

	default:
		CAM_ERR(CAM_ISP, "Invalid event ID %d", evt_id);
		break;
	}

	return rc;
}

static int cam_tfe_hw_mgr_sort_dev_with_caps(
	struct cam_tfe_hw_mgr *tfe_hw_mgr)
{
	int i;

	/* get caps for csid devices */
	for (i = 0; i < CAM_TFE_CSID_HW_NUM_MAX; i++) {
		if (!tfe_hw_mgr->csid_devices[i])
			continue;
		if (tfe_hw_mgr->csid_devices[i]->hw_ops.get_hw_caps) {
			tfe_hw_mgr->csid_devices[i]->hw_ops.get_hw_caps(
				tfe_hw_mgr->csid_devices[i]->hw_priv,
				&tfe_hw_mgr->tfe_csid_dev_caps[i],
				sizeof(tfe_hw_mgr->tfe_csid_dev_caps[i]));
		}
	}

	/* get caps for tfe devices */
	for (i = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
		if (!tfe_hw_mgr->tfe_devices[i])
			continue;

		if (tfe_hw_mgr->tfe_devices[i]->hw_intf->hw_ops.get_hw_caps) {
			tfe_hw_mgr->tfe_devices[i]->hw_intf->hw_ops.get_hw_caps(
				tfe_hw_mgr->tfe_devices[i]->hw_intf->hw_priv,
				&tfe_hw_mgr->tfe_dev_caps[i],
				sizeof(tfe_hw_mgr->tfe_dev_caps[i]));
		}
	}

	return 0;
}

static int cam_tfe_set_csid_debug(void *data, u64 val)
{
	g_tfe_hw_mgr.debug_cfg.csid_debug = val;
	CAM_DBG(CAM_ISP, "Set CSID Debug value :%lld", val);
	return 0;
}

static int cam_tfe_get_csid_debug(void *data, u64 *val)
{
	*val = g_tfe_hw_mgr.debug_cfg.csid_debug;
	CAM_DBG(CAM_ISP, "Get CSID Debug value :%lld",
		g_tfe_hw_mgr.debug_cfg.csid_debug);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cam_tfe_csid_debug,
	cam_tfe_get_csid_debug,
	cam_tfe_set_csid_debug, "%16llu");

static int cam_tfe_set_camif_debug(void *data, u64 val)
{
	g_tfe_hw_mgr.debug_cfg.camif_debug = val;
	CAM_DBG(CAM_ISP,
		"Set camif enable_diag_sensor_status value :%lld", val);
	return 0;
}

static int cam_tfe_get_camif_debug(void *data, u64 *val)
{
	*val = g_tfe_hw_mgr.debug_cfg.camif_debug;
	CAM_DBG(CAM_ISP,
		"Set camif enable_diag_sensor_status value :%lld",
		g_tfe_hw_mgr.debug_cfg.csid_debug);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cam_tfe_camif_debug,
	cam_tfe_get_camif_debug,
	cam_tfe_set_camif_debug, "%16llu");

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int cam_tfe_hw_mgr_debug_register(void)
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
	g_tfe_hw_mgr.debug_cfg.dentry = dbgfileptr;

	dbgfileptr = debugfs_create_file("tfe_csid_debug", 0644,
		g_tfe_hw_mgr.debug_cfg.dentry, NULL, &cam_tfe_csid_debug);
	dbgfileptr = debugfs_create_u32("enable_recovery", 0644,
		g_tfe_hw_mgr.debug_cfg.dentry,
		&g_tfe_hw_mgr.debug_cfg.enable_recovery);
	dbgfileptr = debugfs_create_u32("enable_reg_dump", 0644,
		g_tfe_hw_mgr.debug_cfg.dentry,
		&g_tfe_hw_mgr.debug_cfg.enable_reg_dump);
	dbgfileptr = debugfs_create_u32("enable_csid_recovery", 0644,
		g_tfe_hw_mgr.debug_cfg.dentry,
		&g_tfe_hw_mgr.debug_cfg.enable_csid_recovery);
	dbgfileptr = debugfs_create_file("tfe_camif_debug", 0644,
		g_tfe_hw_mgr.debug_cfg.dentry, NULL, &cam_tfe_camif_debug);
	dbgfileptr = debugfs_create_u32("per_req_reg_dump", 0644,
		g_tfe_hw_mgr.debug_cfg.dentry,
		&g_tfe_hw_mgr.debug_cfg.per_req_reg_dump);
	if (IS_ERR(dbgfileptr)) {
		if (PTR_ERR(dbgfileptr) == -ENODEV)
			CAM_WARN(CAM_ISP, "DebugFS not enabled in kernel!");
		else
			rc = PTR_ERR(dbgfileptr);
	}
end:
	g_tfe_hw_mgr.debug_cfg.enable_recovery = 0;
	return rc;
}
#else
static inline int cam_tfe_hw_mgr_debug_register(void)
{
	g_tfe_hw_mgr.debug_cfg.enable_recovery = 0;
	CAM_WARN(CAM_ISP, "DebugFS not enabled in kernel");
	return 0;
}
#endif

static void cam_req_mgr_process_tfe_worker(struct work_struct *w)
{
	cam_req_mgr_process_workq(w);
}

int cam_tfe_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl)
{
	int rc = -EFAULT;
	int i, j;
	struct cam_iommu_handle cdm_handles;
	struct cam_tfe_hw_mgr_ctx *ctx_pool;
	struct cam_isp_hw_mgr_res *res_list_tfe_out;
	bool support_consumed_addr = false;

	CAM_DBG(CAM_ISP, "Enter");

	memset(&g_tfe_hw_mgr, 0, sizeof(g_tfe_hw_mgr));

	mutex_init(&g_tfe_hw_mgr.ctx_mutex);
	spin_lock_init(&g_tfe_hw_mgr.ctx_lock);

	if (CAM_TFE_HW_NUM_MAX != CAM_TFE_CSID_HW_NUM_MAX) {
		CAM_ERR(CAM_ISP, "CSID num is different then TFE num");
		return -EINVAL;
	}

	/* fill tfe hw intf information */
	for (i = 0, j = 0; i < CAM_TFE_HW_NUM_MAX; i++) {
		rc = cam_tfe_hw_init(&g_tfe_hw_mgr.tfe_devices[i], i);
		if (!rc) {
			struct cam_hw_intf *tfe_device =
				g_tfe_hw_mgr.tfe_devices[i]->hw_intf;
			struct cam_hw_info *tfe_hw = (struct cam_hw_info *)
				g_tfe_hw_mgr.tfe_devices[i]->hw_intf->hw_priv;
			struct cam_hw_soc_info *soc_info = &tfe_hw->soc_info;

			if (j == 0)
				tfe_device->hw_ops.process_cmd(
					tfe_hw,
					CAM_ISP_HW_CMD_IS_CONSUMED_ADDR_SUPPORT,
					&support_consumed_addr,
					sizeof(support_consumed_addr));

			j++;

			g_tfe_hw_mgr.cdm_reg_map[i] = &soc_info->reg_map[0];
			CAM_DBG(CAM_ISP,
				"reg_map: mem base = %pK cam_base = 0x%llx",
				(void __iomem *)soc_info->reg_map[0].mem_base,
				(uint64_t) soc_info->reg_map[0].mem_cam_base);
		} else {
			g_tfe_hw_mgr.cdm_reg_map[i] = NULL;
		}
	}
	if (j == 0) {
		CAM_ERR(CAM_ISP, "no valid TFE HW");
		return -EINVAL;
	}

	g_tfe_hw_mgr.support_consumed_addr = support_consumed_addr;
	/* fill csid hw intf information */
	for (i = 0, j = 0; i < CAM_TFE_CSID_HW_NUM_MAX; i++) {
		rc = cam_tfe_csid_hw_init(&g_tfe_hw_mgr.csid_devices[i], i);
		if (!rc)
			j++;
	}
	if (!j) {
		CAM_ERR(CAM_ISP, "no valid TFE CSID HW");
		return -EINVAL;
	}

	/* fill tpg hw intf information */
	for (i = 0, j = 0; i < CAM_TOP_TPG_HW_NUM_MAX; i++) {
		rc = cam_top_tpg_hw_init(&g_tfe_hw_mgr.tpg_devices[i], i);
		if (!rc)
			j++;
	}
	if (!j) {
		CAM_ERR(CAM_ISP, "no valid TFE TPG HW");
		return -EINVAL;
	}

	cam_tfe_hw_mgr_sort_dev_with_caps(&g_tfe_hw_mgr);

	/* setup tfe context list */
	INIT_LIST_HEAD(&g_tfe_hw_mgr.free_ctx_list);
	INIT_LIST_HEAD(&g_tfe_hw_mgr.used_ctx_list);

	/*
	 *  for now, we only support one iommu handle. later
	 *  we will need to setup more iommu handle for other
	 *  use cases.
	 *  Also, we have to release them once we have the
	 *  deinit support
	 */
	if (cam_smmu_get_handle("tfe",
		&g_tfe_hw_mgr.mgr_common.img_iommu_hdl)) {
		CAM_ERR(CAM_ISP, "Can not get iommu handle");
		return -EINVAL;
	}

	if (cam_smmu_get_handle("cam-secure",
		&g_tfe_hw_mgr.mgr_common.img_iommu_hdl_secure)) {
		CAM_ERR(CAM_ISP, "Failed to get secure iommu handle");
		goto secure_fail;
	}

	CAM_DBG(CAM_ISP, "iommu_handles: non-secure[0x%x], secure[0x%x]",
		g_tfe_hw_mgr.mgr_common.img_iommu_hdl,
		g_tfe_hw_mgr.mgr_common.img_iommu_hdl_secure);

	if (!cam_cdm_get_iommu_handle("tfe0", &cdm_handles)) {
		CAM_DBG(CAM_ISP, "Successfully acquired the CDM iommu handles");
		g_tfe_hw_mgr.mgr_common.cmd_iommu_hdl = cdm_handles.non_secure;
		g_tfe_hw_mgr.mgr_common.cmd_iommu_hdl_secure =
			cdm_handles.secure;
	} else {
		CAM_DBG(CAM_ISP, "Failed to acquire the CDM iommu handles");
		g_tfe_hw_mgr.mgr_common.cmd_iommu_hdl = -1;
		g_tfe_hw_mgr.mgr_common.cmd_iommu_hdl_secure = -1;
	}

	atomic_set(&g_tfe_hw_mgr.active_ctx_cnt, 0);
	for (i = 0; i < CAM_TFE_CTX_MAX; i++) {
		memset(&g_tfe_hw_mgr.ctx_pool[i], 0,
			sizeof(g_tfe_hw_mgr.ctx_pool[i]));
		INIT_LIST_HEAD(&g_tfe_hw_mgr.ctx_pool[i].list);
		INIT_LIST_HEAD(&g_tfe_hw_mgr.ctx_pool[i].res_list_tfe_csid);
		INIT_LIST_HEAD(&g_tfe_hw_mgr.ctx_pool[i].res_list_tfe_in);
		ctx_pool = &g_tfe_hw_mgr.ctx_pool[i];
		for (j = 0; j < CAM_TFE_HW_OUT_RES_MAX; j++) {
			res_list_tfe_out = &ctx_pool->res_list_tfe_out[j];
			INIT_LIST_HEAD(&res_list_tfe_out->list);
		}

		/* init context pool */
		INIT_LIST_HEAD(&g_tfe_hw_mgr.ctx_pool[i].free_res_list);
		for (j = 0; j < CAM_TFE_HW_RES_POOL_MAX; j++) {
			INIT_LIST_HEAD(
				&g_tfe_hw_mgr.ctx_pool[i].res_pool[j].list);
			list_add_tail(
				&g_tfe_hw_mgr.ctx_pool[i].res_pool[j].list,
				&g_tfe_hw_mgr.ctx_pool[i].free_res_list);
		}

		g_tfe_hw_mgr.ctx_pool[i].cdm_cmd =
			kzalloc(((sizeof(struct cam_cdm_bl_request)) +
				((CAM_TFE_HW_ENTRIES_MAX - 1) *
				 sizeof(struct cam_cdm_bl_cmd))), GFP_KERNEL);
		if (!g_tfe_hw_mgr.ctx_pool[i].cdm_cmd) {
			rc = -ENOMEM;
			CAM_ERR(CAM_ISP, "Allocation Failed for cdm command");
			goto end;
		}

		g_tfe_hw_mgr.ctx_pool[i].ctx_index = i;
		g_tfe_hw_mgr.ctx_pool[i].hw_mgr = &g_tfe_hw_mgr;

		cam_tasklet_init(&g_tfe_hw_mgr.mgr_common.tasklet_pool[i],
			&g_tfe_hw_mgr.ctx_pool[i], i);
		g_tfe_hw_mgr.ctx_pool[i].common.tasklet_info =
			g_tfe_hw_mgr.mgr_common.tasklet_pool[i];


		init_completion(&g_tfe_hw_mgr.ctx_pool[i].config_done_complete);
		list_add_tail(&g_tfe_hw_mgr.ctx_pool[i].list,
			&g_tfe_hw_mgr.free_ctx_list);
	}

	/* Create Worker for tfe_hw_mgr with 10 tasks */
	rc = cam_req_mgr_workq_create("cam_tfe_worker", 10,
		&g_tfe_hw_mgr.workq, CRM_WORKQ_USAGE_NON_IRQ, 0,
		cam_req_mgr_process_tfe_worker);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "Unable to create worker");
		goto end;
	}

	/* fill return structure */
	hw_mgr_intf->hw_mgr_priv = &g_tfe_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_tfe_mgr_get_hw_caps;
	hw_mgr_intf->hw_acquire = cam_tfe_mgr_acquire;
	hw_mgr_intf->hw_start = cam_tfe_mgr_start_hw;
	hw_mgr_intf->hw_stop = cam_tfe_mgr_stop_hw;
	hw_mgr_intf->hw_read = cam_tfe_mgr_read;
	hw_mgr_intf->hw_write = cam_tfe_mgr_write;
	hw_mgr_intf->hw_release = cam_tfe_mgr_release_hw;
	hw_mgr_intf->hw_prepare_update = cam_tfe_mgr_prepare_hw_update;
	hw_mgr_intf->hw_config = cam_tfe_mgr_config_hw;
	hw_mgr_intf->hw_cmd = cam_tfe_mgr_cmd;
	hw_mgr_intf->hw_reset = cam_tfe_mgr_reset;
	hw_mgr_intf->hw_dump = cam_tfe_mgr_dump;

	if (iommu_hdl)
		*iommu_hdl = g_tfe_hw_mgr.mgr_common.img_iommu_hdl;

	cam_tfe_hw_mgr_debug_register();
	CAM_DBG(CAM_ISP, "Exit");

	return 0;
end:
	if (rc) {
		for (i = 0; i < CAM_TFE_CTX_MAX; i++) {
			cam_tasklet_deinit(
				&g_tfe_hw_mgr.mgr_common.tasklet_pool[i]);
			kfree(g_tfe_hw_mgr.ctx_pool[i].cdm_cmd);
			g_tfe_hw_mgr.ctx_pool[i].cdm_cmd = NULL;
			g_tfe_hw_mgr.ctx_pool[i].common.tasklet_info = NULL;
		}
	}
	cam_smmu_destroy_handle(
		g_tfe_hw_mgr.mgr_common.img_iommu_hdl_secure);
	g_tfe_hw_mgr.mgr_common.img_iommu_hdl_secure = -1;
secure_fail:
	cam_smmu_destroy_handle(g_tfe_hw_mgr.mgr_common.img_iommu_hdl);
	g_tfe_hw_mgr.mgr_common.img_iommu_hdl = -1;
	return rc;
}

void cam_tfe_hw_mgr_deinit(void)
{
	int i = 0;

	cam_req_mgr_workq_destroy(&g_tfe_hw_mgr.workq);
	debugfs_remove_recursive(g_tfe_hw_mgr.debug_cfg.dentry);
	g_tfe_hw_mgr.debug_cfg.dentry = NULL;

	for (i = 0; i < CAM_TFE_CTX_MAX; i++) {
		cam_tasklet_deinit(
			&g_tfe_hw_mgr.mgr_common.tasklet_pool[i]);
		kfree(g_tfe_hw_mgr.ctx_pool[i].cdm_cmd);
		g_tfe_hw_mgr.ctx_pool[i].cdm_cmd = NULL;
		g_tfe_hw_mgr.ctx_pool[i].common.tasklet_info = NULL;
	}

	cam_smmu_destroy_handle(
		g_tfe_hw_mgr.mgr_common.img_iommu_hdl_secure);
	g_tfe_hw_mgr.mgr_common.img_iommu_hdl_secure = -1;

	cam_smmu_destroy_handle(g_tfe_hw_mgr.mgr_common.img_iommu_hdl);
	g_tfe_hw_mgr.mgr_common.img_iommu_hdl = -1;
}
