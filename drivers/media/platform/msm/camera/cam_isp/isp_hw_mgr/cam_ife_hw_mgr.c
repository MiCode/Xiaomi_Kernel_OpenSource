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

#define CAM_IFE_HW_ENTRIES_MAX  20

#define TZ_SVC_SMMU_PROGRAM 0x15
#define TZ_SAFE_SYSCALL_ID  0x3
#define CAM_IFE_SAFE_DISABLE 0
#define CAM_IFE_SAFE_ENABLE 1
#define SMMU_SE_IFE 0

#define CAM_ISP_PACKET_META_MAX                     \
	(CAM_ISP_PACKET_META_GENERIC_BLOB_COMMON + 1)

#define CAM_ISP_GENERIC_BLOB_TYPE_MAX               \
	(CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG + 1)

static uint32_t blob_type_hw_cmd_map[CAM_ISP_GENERIC_BLOB_TYPE_MAX] = {
	CAM_ISP_HW_CMD_GET_HFR_UPDATE,
	CAM_ISP_HW_CMD_CLOCK_UPDATE,
	CAM_ISP_HW_CMD_BW_UPDATE,
};

static struct cam_ife_hw_mgr g_ife_hw_mgr;

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

	if (copy_from_user(&query_isp, (void __user *)query->caps_handle,
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

	if (copy_to_user((void __user *)query->caps_handle, &query_isp,
		sizeof(struct cam_isp_query_cap_cmd)))
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

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.start) {
			isp_hw_res->hw_res[i]->rdi_only_ctx =
				ctx->is_rdi_only_context;
			rc = hw_intf->hw_ops.start(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
			if (rc) {
				CAM_ERR(CAM_ISP, "Can not start HW resources");
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
	struct cam_ife_hw_mgr_res   *isp_hw_res)
{
	int i;
	struct cam_hw_intf      *hw_intf;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.stop)
			hw_intf->hw_ops.stop(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
		else
			CAM_ERR(CAM_ISP, "stop null");
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
					"Release hw resrouce id %d failed",
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
			if (!hw_mgr_res->hw_res[i])
				continue;

			isp_res = hw_mgr_res->hw_res[i];
			if (isp_res->hw_intf->hw_idx != base_idx)
				continue;

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
		/*Check if base index is alreay exist in the list */
		for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
			if (ctx->base[i].idx == base_idx) {
				if (split_id != CAM_ISP_HW_SPLIT_MAX &&
					ctx->base[i].split_id ==
						CAM_ISP_HW_SPLIT_MAX)
					ctx->base[i].split_id = split_id;

				break;
			}
		}

		if (i == CAM_IFE_HW_NUM_MAX) {
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

static int cam_ife_hw_mgr_acquire_res_ife_out_rdi(
	struct cam_ife_hw_mgr_ctx       *ife_ctx,
	struct cam_ife_hw_mgr_res       *ife_src_res,
	struct cam_isp_in_port_info     *in_port)
{
	int rc = -EINVAL;
	struct cam_vfe_acquire_args               vfe_acquire;
	struct cam_isp_out_port_info             *out_port = NULL;
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
		vfe_acquire.vfe_out.ctx = ife_ctx;
		vfe_acquire.vfe_out.out_port_info = out_port;
		vfe_acquire.vfe_out.split_id = CAM_ISP_HW_SPLIT_LEFT;
		vfe_acquire.vfe_out.unique_id = ife_ctx->ctx_index;
		vfe_acquire.vfe_out.is_dual = 0;
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
	ife_out_res->res_type = CAM_ISP_RESOURCE_VFE_OUT;
	ife_src_res->child[ife_src_res->num_children++] = ife_out_res;

	return 0;
err:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_out_pixel(
	struct cam_ife_hw_mgr_ctx       *ife_ctx,
	struct cam_ife_hw_mgr_res       *ife_src_res,
	struct cam_isp_in_port_info     *in_port)
{
	int rc = -1;
	uint32_t  i, j, k;
	struct cam_vfe_acquire_args               vfe_acquire;
	struct cam_isp_out_port_info             *out_port;
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

		CAM_DBG(CAM_ISP, "res_type 0x%x",
			 out_port->res_type);

		ife_out_res = &ife_ctx->res_list_ife_out[k];
		ife_out_res->is_dual_vfe = in_port->usage_type;

		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_OUT;
		vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		vfe_acquire.vfe_out.cdm_ops = ife_ctx->cdm_ops;
		vfe_acquire.vfe_out.ctx = ife_ctx;
		vfe_acquire.vfe_out.out_port_info =  out_port;
		vfe_acquire.vfe_out.is_dual       = ife_src_res->is_dual_vfe;
		vfe_acquire.vfe_out.unique_id     = ife_ctx->ctx_index;

		for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
			if (!ife_src_res->hw_res[j])
				continue;

			if (j == CAM_ISP_HW_SPLIT_LEFT) {
				vfe_acquire.vfe_out.split_id  =
					CAM_ISP_HW_SPLIT_LEFT;
				if (ife_src_res->is_dual_vfe) {
					/*TBD */
					vfe_acquire.vfe_out.is_master     = 1;
					vfe_acquire.vfe_out.dual_slave_core =
						1;
				} else {
					vfe_acquire.vfe_out.is_master   = 0;
					vfe_acquire.vfe_out.dual_slave_core =
						0;
				}
			} else {
				vfe_acquire.vfe_out.split_id  =
					CAM_ISP_HW_SPLIT_RIGHT;
				vfe_acquire.vfe_out.is_master       = 0;
				vfe_acquire.vfe_out.dual_slave_core = 0;
			}

			hw_intf = ife_src_res->hw_res[j]->hw_intf;
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
		ife_out_res->parent = ife_src_res;
		ife_src_res->child[ife_src_res->num_children++] = ife_out_res;
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_out(
	struct cam_ife_hw_mgr_ctx     *ife_ctx,
	struct cam_isp_in_port_info   *in_port)
{
	int rc = -EINVAL;
	struct cam_ife_hw_mgr_res       *ife_src_res;

	list_for_each_entry(ife_src_res, &ife_ctx->res_list_ife_src, list) {
		if (ife_src_res->num_children)
			continue;

		switch (ife_src_res->res_id) {
		case CAM_ISP_HW_VFE_IN_CAMIF:
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

static int cam_ife_hw_mgr_acquire_res_ife_src(
	struct cam_ife_hw_mgr_ctx     *ife_ctx,
	struct cam_isp_in_port_info   *in_port)
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
		if (csid_res->num_children)
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

		switch (csid_res->res_id) {
		case CAM_IFE_PIX_PATH_RES_IPP:
			vfe_acquire.vfe_in.res_id = CAM_ISP_HW_VFE_IN_CAMIF;
			vfe_acquire.vfe_in.in_port = in_port;
			if (csid_res->is_dual_vfe)
				vfe_acquire.vfe_in.sync_mode =
				CAM_ISP_HW_SYNC_MASTER;
			else
				vfe_acquire.vfe_in.sync_mode =
				CAM_ISP_HW_SYNC_NONE;

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
		ife_src_res->res_type = vfe_acquire.rsrc_type;
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
			CAM_DBG(CAM_ISP,
				"acquire success res type :0x%x res id:0x%x",
				ife_src_res->hw_res[i]->res_type,
				ife_src_res->hw_res[i]->res_id);

		}

		/* It should be one to one mapping between
		 * csid resource and ife source resource
		 */
		csid_res->child[0] = ife_src_res;
		ife_src_res->parent = csid_res;
		csid_res->child[csid_res->num_children++] = ife_src_res;
		CAM_DBG(CAM_ISP, "csid_res=%d  num_children=%d ife_src_res=%d",
			csid_res->res_id, csid_res->num_children,
			ife_src_res->res_id);
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_ife_mgr_acquire_cid_res(
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_isp_in_port_info        *in_port,
	uint32_t                           *cid_res_id,
	enum cam_ife_pix_path_res_id        csid_path)
{
	int rc = -1;
	int i, j;
	struct cam_ife_hw_mgr               *ife_hw_mgr;
	struct cam_ife_hw_mgr_res           *cid_res;
	struct cam_hw_intf                  *hw_intf;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;

	ife_hw_mgr = ife_ctx->hw_mgr;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &cid_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto err;
	}
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_cid, &cid_res);

	csid_acquire.res_type = CAM_ISP_RESOURCE_CID;
	csid_acquire.in_port = in_port;
	csid_acquire.res_id =  csid_path;

	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (!ife_hw_mgr->csid_devices[i])
			continue;

		hw_intf = ife_hw_mgr->csid_devices[i];
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv, &csid_acquire,
			sizeof(csid_acquire));
		if (rc)
			continue;
		else
			break;
	}

	if (i == CAM_IFE_CSID_HW_NUM_MAX || !csid_acquire.node_res) {
		CAM_ERR(CAM_ISP, "Can not acquire ife csid rdi resource");
		goto err;
	}

	cid_res->res_type = CAM_IFE_HW_MGR_RES_CID;
	cid_res->res_id = csid_acquire.node_res->res_id;
	cid_res->is_dual_vfe = in_port->usage_type;
	cid_res->hw_res[0] = csid_acquire.node_res;
	cid_res->hw_res[1] = NULL;
	/* CID(DT_ID) value of acquire device, require for path */
	*cid_res_id = csid_acquire.node_res->res_id;

	if (cid_res->is_dual_vfe) {
		csid_acquire.node_res = NULL;
		csid_acquire.res_type = CAM_ISP_RESOURCE_CID;
		csid_acquire.in_port = in_port;
		for (j = i + 1; j < CAM_IFE_CSID_HW_NUM_MAX; j++) {
			if (!ife_hw_mgr->csid_devices[j])
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
			goto err;
		}
		cid_res->hw_res[1] = csid_acquire.node_res;
	}
	cid_res->parent = &ife_ctx->res_list_ife_in;
	ife_ctx->res_list_ife_in.child[
		ife_ctx->res_list_ife_in.num_children++] = cid_res;

	return 0;
err:
	return rc;

}

static int cam_ife_hw_mgr_acquire_res_ife_csid_ipp(
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_isp_in_port_info        *in_port)
{
	int rc = -1;
	int i;

	struct cam_ife_hw_mgr               *ife_hw_mgr;
	struct cam_ife_hw_mgr_res           *csid_res;
	struct cam_ife_hw_mgr_res           *cid_res;
	struct cam_hw_intf                  *hw_intf;
	uint32_t                             cid_res_id;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;

	/* get cid resource */
	rc = cam_ife_mgr_acquire_cid_res(ife_ctx, in_port, &cid_res_id,
		CAM_IFE_PIX_PATH_RES_IPP);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE CID resource Failed");
		goto err;
	}

	ife_hw_mgr = ife_ctx->hw_mgr;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &csid_res);
	if (rc) {
		CAM_ERR(CAM_ISP, "No more free hw mgr resource");
		goto err;
	}
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_csid, &csid_res);

	csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
	csid_acquire.res_id = CAM_IFE_PIX_PATH_RES_IPP;
	csid_acquire.cid = cid_res_id;
	csid_acquire.in_port = in_port;
	csid_acquire.out_port = in_port->data;

	csid_res->res_type = CAM_ISP_RESOURCE_PIX_PATH;
	csid_res->res_id = CAM_IFE_PIX_PATH_RES_IPP;
	csid_res->is_dual_vfe = in_port->usage_type;

	if (in_port->usage_type)
		csid_res->is_dual_vfe = 1;
	else {
		csid_res->is_dual_vfe = 0;
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;
	}

	list_for_each_entry(cid_res, &ife_ctx->res_list_ife_cid,
		list) {
		if (cid_res->res_id != cid_res_id)
			continue;

		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!cid_res->hw_res[i])
				continue;

			csid_acquire.node_res = NULL;
			if (csid_res->is_dual_vfe) {
				if (i == CAM_ISP_HW_SPLIT_LEFT)
					csid_acquire.sync_mode =
						CAM_ISP_HW_SYNC_MASTER;
				else
					csid_acquire.sync_mode =
						CAM_ISP_HW_SYNC_SLAVE;
			}

			hw_intf = ife_hw_mgr->csid_devices[
				cid_res->hw_res[i]->hw_intf->hw_idx];
			rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
				&csid_acquire, sizeof(csid_acquire));
			if (rc) {
				CAM_ERR(CAM_ISP,
					"Cannot acquire ife csid ipp resource");
				goto err;
			}

			csid_res->hw_res[i] = csid_acquire.node_res;
			CAM_DBG(CAM_ISP,
				"acquired csid(%s)=%d ipp rsrc successfully",
				(i == 0) ? "left" : "right",
				hw_intf->hw_idx);

		}

		if (i == CAM_IFE_CSID_HW_NUM_MAX) {
			CAM_ERR(CAM_ISP,
				"Can not acquire ife csid ipp resource");
			goto err;
		}

		csid_res->parent = cid_res;
		cid_res->child[cid_res->num_children++] = csid_res;
	}

	CAM_DBG(CAM_ISP, "acquire res %d", csid_acquire.res_id);

	return 0;
err:
	return rc;
}

static enum cam_ife_pix_path_res_id
	cam_ife_hw_mgr_get_ife_csid_rdi_res_type(
	uint32_t                 out_port_type)
{
	enum cam_ife_pix_path_res_id path_id;
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

	CAM_DBG(CAM_ISP, "out_port %d path_id %d", out_port_type, path_id);

	return path_id;
}

static int cam_ife_hw_mgr_acquire_res_ife_csid_rdi(
	struct cam_ife_hw_mgr_ctx     *ife_ctx,
	struct cam_isp_in_port_info   *in_port)
{
	int rc = -1;
	int i, j;

	struct cam_ife_hw_mgr               *ife_hw_mgr;
	struct cam_ife_hw_mgr_res           *csid_res;
	struct cam_ife_hw_mgr_res           *cid_res;
	struct cam_hw_intf                  *hw_intf;
	struct cam_isp_out_port_info        *out_port;
	uint32_t                            cid_res_id;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;

	ife_hw_mgr = ife_ctx->hw_mgr;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		if (!cam_ife_hw_mgr_is_rdi_res(out_port->res_type))
			continue;

		/* get cid resource */
		rc = cam_ife_mgr_acquire_cid_res(ife_ctx, in_port, &cid_res_id,
			cam_ife_hw_mgr_get_ife_csid_rdi_res_type(
			out_port->res_type));
		if (rc) {
			CAM_ERR(CAM_ISP, "Acquire IFE CID resource Failed");
			goto err;
		}

		rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&csid_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "No more free hw mgr resource");
			goto err;
		}
		cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_csid, &csid_res);

		/*
		 * no need to check since we are doing one to one mapping
		 * between the csid rdi type and out port rdi type
		 */

		memset(&csid_acquire, 0, sizeof(csid_acquire));
		csid_acquire.res_id =
			cam_ife_hw_mgr_get_ife_csid_rdi_res_type(
				out_port->res_type);

		csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_acquire.cid = cid_res_id;
		csid_acquire.in_port = in_port;
		csid_acquire.out_port = out_port;
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;

		list_for_each_entry(cid_res, &ife_ctx->res_list_ife_cid,
			list) {
			if (cid_res->res_id != cid_res_id)
				continue;

			for (j = 0; j < CAM_ISP_HW_SPLIT_MAX; j++) {
				if (!cid_res->hw_res[j])
					continue;

				csid_acquire.node_res = NULL;

				hw_intf = ife_hw_mgr->csid_devices[
					cid_res->hw_res[j]->hw_intf->hw_idx];
				rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
					&csid_acquire, sizeof(csid_acquire));
				if (rc) {
					CAM_DBG(CAM_ISP,
					 "CSID Path reserve failed hw=%d rc=%d",
					 hw_intf->hw_idx, rc);
					continue;
				}

				/* RDI does not need Dual ISP. Break */
				break;
			}

			if (j == CAM_ISP_HW_SPLIT_MAX &&
				csid_acquire.node_res == NULL) {
				CAM_ERR(CAM_ISP,
					"acquire csid rdi rsrc failed, cid %d",
					cid_res_id);
				goto err;
			}

			csid_res->res_type = CAM_ISP_RESOURCE_PIX_PATH;
			csid_res->res_id = csid_acquire.res_id;
			csid_res->is_dual_vfe = 0;
			csid_res->hw_res[0] = csid_acquire.node_res;
			csid_res->hw_res[1] = NULL;
			CAM_DBG(CAM_ISP, "acquire res %d",
				csid_acquire.res_id);
			csid_res->parent = cid_res;
			cid_res->child[cid_res->num_children++] =
				csid_res;

			/* Done with cid_res_id. Break */
			break;
		}
	}

	return 0;
err:
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_root(
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_isp_in_port_info        *in_port)
{
	int rc = -1;

	if (ife_ctx->res_list_ife_in.res_type == CAM_IFE_HW_MGR_RES_UNINIT) {
		/* first acquire */
		ife_ctx->res_list_ife_in.res_type = CAM_IFE_HW_MGR_RES_ROOT;
		ife_ctx->res_list_ife_in.res_id = in_port->res_type;
		ife_ctx->res_list_ife_in.is_dual_vfe = in_port->usage_type;
	} else if (ife_ctx->res_list_ife_in.res_id != in_port->res_type) {
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

static int cam_ife_hw_mgr_preprocess_out_port(
	struct cam_ife_hw_mgr_ctx   *ife_ctx,
	struct cam_isp_in_port_info *in_port,
	int                         *pixel_count,
	int                         *rdi_count)
{
	int pixel_num      = 0;
	int rdi_num        = 0;
	uint32_t i;
	struct cam_isp_out_port_info      *out_port;
	struct cam_ife_hw_mgr             *ife_hw_mgr;

	ife_hw_mgr = ife_ctx->hw_mgr;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		if (cam_ife_hw_mgr_is_rdi_res(out_port->res_type))
			rdi_num++;
		else
			pixel_num++;
	}

	*pixel_count = pixel_num;
	*rdi_count = rdi_num;

	return 0;
}

static int cam_ife_mgr_acquire_hw_for_ctx(
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_isp_in_port_info        *in_port,
	uint32_t  *num_pix_port, uint32_t  *num_rdi_port)
{
	int rc                                    = -1;
	int is_dual_vfe                           = 0;
	int pixel_count                           = 0;
	int rdi_count                             = 0;

	is_dual_vfe = in_port->usage_type;

	/* get root node resource */
	rc = cam_ife_hw_mgr_acquire_res_root(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Can not acquire csid rx resource");
		goto err;
	}

	cam_ife_hw_mgr_preprocess_out_port(ife_ctx, in_port,
		&pixel_count, &rdi_count);

	if (!pixel_count && !rdi_count) {
		CAM_ERR(CAM_ISP, "No PIX or RDI resource");
		return -EINVAL;
	}

	if (pixel_count) {
		/* get ife csid IPP resrouce */
		rc = cam_ife_hw_mgr_acquire_res_ife_csid_ipp(ife_ctx, in_port);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE CSID IPP resource Failed");
			goto err;
		}
	}

	if (rdi_count) {
		/* get ife csid rdi resource */
		rc = cam_ife_hw_mgr_acquire_res_ife_csid_rdi(ife_ctx, in_port);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Acquire IFE CSID RDI resource Failed");
			goto err;
		}
	}

	/* get ife src resource */
	rc = cam_ife_hw_mgr_acquire_res_ife_src(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE SRC resource Failed");
		goto err;
	}

	rc = cam_ife_hw_mgr_acquire_res_ife_out(ife_ctx, in_port);
	if (rc) {
		CAM_ERR(CAM_ISP, "Acquire IFE OUT resource Failed");
		goto err;
	}

	*num_pix_port += pixel_count;
	*num_rdi_port += rdi_count;

	return 0;
err:
	/* release resource at the acquire entry funciton */
	return rc;
}

void cam_ife_cam_cdm_callback(uint32_t handle, void *userdata,
	enum cam_cdm_cb_status status, uint32_t cookie)
{
	CAM_DBG(CAM_ISP,
		"Called by CDM hdl=%x, udata=%pK, status=%d, cookie=%d",
		 handle, userdata, status, cookie);
}

/* entry function: acquire_hw */
static int cam_ife_mgr_acquire_hw(void *hw_mgr_priv,
					void *acquire_hw_args)
{
	struct cam_ife_hw_mgr *ife_hw_mgr            = hw_mgr_priv;
	struct cam_hw_acquire_args *acquire_args     = acquire_hw_args;
	int rc                                       = -1;
	int i, j;
	struct cam_ife_hw_mgr_ctx         *ife_ctx;
	struct cam_isp_in_port_info       *in_port = NULL;
	struct cam_isp_resource           *isp_resource = NULL;
	struct cam_cdm_acquire_data        cdm_acquire;
	uint32_t                           num_pix_port_per_in = 0;
	uint32_t                           num_rdi_port_per_in = 0;
	uint32_t                           total_pix_port = 0;
	uint32_t                           total_rdi_port = 0;

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

	isp_resource = (struct cam_isp_resource *)acquire_args->acquire_info;

	/* acquire HW resources */
	for (i = 0; i < acquire_args->num_acq; i++) {
		if (isp_resource[i].resource_id != CAM_ISP_RES_ID_PORT)
			continue;

		CAM_DBG(CAM_ISP,
			"start copy from user handle %lld with len = %d",
			isp_resource[i].res_hdl,
			isp_resource[i].length);

		in_port = memdup_user((void __user *)isp_resource[i].res_hdl,
			isp_resource[i].length);
		if (in_port > 0) {
			rc = cam_ife_mgr_acquire_hw_for_ctx(ife_ctx, in_port,
				&num_pix_port_per_in, &num_rdi_port_per_in);
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
	ife_ctx->ctx_state = CAM_IFE_HW_MGR_CTX_ACQUIRED;

	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->used_ctx_list, &ife_ctx);

	CAM_DBG(CAM_ISP, "Exit...(success)");

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

	CAM_DBG(CAM_ISP,
		"usage=%u left cam_bw_bps=%llu ext_bw_bps=%llu\n"
		"right cam_bw_bps=%llu ext_bw_bps=%llu",
		bw_config->usage_type,
		bw_config->left_pix_vote.cam_bw_bps,
		bw_config->left_pix_vote.ext_bw_bps,
		bw_config->right_pix_vote.cam_bw_bps,
		bw_config->right_pix_vote.ext_bw_bps);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i])
				continue;

			if (hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF)
				if (i == CAM_ISP_HW_SPLIT_LEFT) {
					cam_bw_bps =
					bw_config->left_pix_vote.cam_bw_bps;
					ext_bw_bps =
					bw_config->left_pix_vote.ext_bw_bps;
				} else {
					cam_bw_bps =
					bw_config->right_pix_vote.cam_bw_bps;
					ext_bw_bps =
					bw_config->right_pix_vote.ext_bw_bps;
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
			} else
				if (hw_mgr_res->hw_res[i]) {
					CAM_ERR(CAM_ISP, "Invalid res_id %u",
						hw_mgr_res->res_id);
					rc = -EINVAL;
					return rc;
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
					CAM_ERR(CAM_ISP, "BW Update failed");
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
	int rc = -1, i;
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

	if ((ctx->ctx_state < CAM_IFE_HW_MGR_CTX_ACQUIRED) || !ctx->cdm_cmd) {
		CAM_ERR(CAM_ISP, "Invalid context parameters");
		return -EPERM;
	}
	if (atomic_read(&ctx->overflow_pending))
		return -EINVAL;

	hw_update_data = (struct cam_isp_prepare_hw_update_data  *) cfg->priv;

	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		if (hw_update_data->bw_config_valid[i] == true) {
			rc = cam_isp_blob_bw_update(
				(struct cam_isp_bw_config *)
				&hw_update_data->bw_config[i], ctx);
			if (rc)
				CAM_ERR(CAM_ISP, "Bandwidth Update Failed");
			}
	}

	CAM_DBG(CAM_ISP, "Enter ctx id:%d num_hw_upd_entries %d",
		ctx->ctx_index, cfg->num_hw_update_entries);

	if (cfg->num_hw_update_entries > 0) {
		cdm_cmd = ctx->cdm_cmd;
		cdm_cmd->cmd_arrary_count = cfg->num_hw_update_entries;
		cdm_cmd->type = CAM_CDM_BL_CMD_TYPE_MEM_HANDLE;
		cdm_cmd->flag = false;
		cdm_cmd->userdata = NULL;
		cdm_cmd->cookie = 0;

		for (i = 0 ; i <= cfg->num_hw_update_entries; i++) {
			cmd = (cfg->hw_update_entries + i);
			cdm_cmd->cmd[i].bl_addr.mem_handle = cmd->handle;
			cdm_cmd->cmd[i].offset = cmd->offset;
			cdm_cmd->cmd[i].len = cmd->len;
		}

		CAM_DBG(CAM_ISP, "Submit to CDM");
		rc = cam_cdm_submit_bls(ctx->cdm_handle, cdm_cmd);
		if (rc)
			CAM_ERR(CAM_ISP, "Failed to apply the configs");
	} else {
		CAM_ERR(CAM_ISP, "No commands to config");
	}
	CAM_DBG(CAM_ISP, "Exit");

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
	if (!ctx || (ctx->ctx_state < CAM_IFE_HW_MGR_CTX_STARTED)) {
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

	/* IFE out resources */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++)
		cam_ife_hw_mgr_stop_hw_res(&ctx->res_list_ife_out[i]);


	/* Stop tasklet for context */
	cam_tasklet_stop(ctx->common.tasklet_info);
	ctx->ctx_state = CAM_IFE_HW_MGR_CTX_STOPPED;
	CAM_DBG(CAM_ISP, "Exit...ctx id:%d rc :%d",
		ctx->ctx_index, rc);

	return rc;
}

/* entry function: stop_hw */
static int cam_ife_mgr_stop_hw(void *hw_mgr_priv, void *stop_hw_args)
{
	int                               rc        = 0;
	struct cam_hw_stop_args          *stop_args = stop_hw_args;
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	struct cam_ife_hw_mgr_ctx        *ctx;
	enum cam_ife_csid_halt_cmd        csid_halt_type;
	uint32_t                          i, master_base_idx = 0;

	if (!hw_mgr_priv || !stop_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}
	ctx = (struct cam_ife_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
	if (!ctx || (ctx->ctx_state < CAM_IFE_HW_MGR_CTX_STARTED)) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, " Enter...ctx id:%d",
		ctx->ctx_index);

	/* Set the csid halt command */
	if (!stop_args->args || (ctx->ctx_state == CAM_IFE_HW_MGR_CTX_PAUSED))
		csid_halt_type = CAM_CSID_HALT_IMMEDIATELY;
	else
		csid_halt_type = CAM_CSID_HALT_AT_FRAME_BOUNDARY;

	/* Note:stop resource will remove the irq mask from the hardware */

	if (!ctx->num_base) {
		CAM_ERR(CAM_ISP, "number of bases are zero");
		return -EINVAL;
	}

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

	/* Stop the master CSID path first */
	cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_csid,
			master_base_idx, csid_halt_type);

	/* stop rest of the CSID paths  */
	for (i = 0; i < ctx->num_base; i++) {
		if (i == master_base_idx)
			continue;

		cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_csid,
			ctx->base[i].idx, csid_halt_type);
	}

	/* Stop the master CIDs first */
	cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_cid,
			master_base_idx, csid_halt_type);

	/* stop rest of the CIDs  */
	for (i = 0; i < ctx->num_base; i++) {
		if (i == master_base_idx)
			continue;
		cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_cid,
			ctx->base[i].idx, csid_halt_type);
	}

	if (cam_cdm_stream_off(ctx->cdm_handle))
		CAM_ERR(CAM_ISP, "CDM stream off failed %d",
			ctx->cdm_handle);

	/* IFE mux in resources */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		cam_ife_hw_mgr_stop_hw_res(hw_mgr_res);
	}

	/* IFE out resources */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++)
		cam_ife_hw_mgr_stop_hw_res(&ctx->res_list_ife_out[i]);

	/* Update vote bandwidth should be done at the HW layer */

	cam_tasklet_stop(ctx->common.tasklet_info);

	/* Deinit IFE root node: do nothing */

	/* Deinit IFE CID */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_cid, list) {
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deinit IFE CSID */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deint IFE MUX(SRC) */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		cam_ife_hw_mgr_deinit_hw_res(hw_mgr_res);
	}

	/* Deinit IFE OUT */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++)
		cam_ife_hw_mgr_deinit_hw_res(&ctx->res_list_ife_out[i]);

	ctx->ctx_state = CAM_IFE_HW_MGR_CTX_STOPPED;
	CAM_DBG(CAM_ISP, "Exit...ctx id:%d rc :%d", ctx->ctx_index, rc);

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
	if (!ctx || (ctx->ctx_state < CAM_IFE_HW_MGR_CTX_ACQUIRED)) {
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
	ctx->ctx_state = CAM_IFE_HW_MGR_CTX_STARTED;
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
	struct cam_hw_config_args        *start_args = start_hw_args;
	struct cam_ife_hw_mgr_ctx        *ctx;
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	uint32_t                          i;

	if (!hw_mgr_priv || !start_hw_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)start_args->ctxt_to_hw_map;
	if (!ctx || (ctx->ctx_state < CAM_IFE_HW_MGR_CTX_ACQUIRED)) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "Enter... ctx id:%d",
		ctx->ctx_index);

	/* update Bandwidth should be done at the hw layer */

	cam_tasklet_start(ctx->common.tasklet_info);

	/* set current csid debug information to CSID HW */
	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (g_ife_hw_mgr.csid_devices[i])
			rc = g_ife_hw_mgr.csid_devices[i]->hw_ops.process_cmd(
				g_ife_hw_mgr.csid_devices[i]->hw_priv,
				CAM_IFE_CSID_SET_CSID_DEBUG,
				&g_ife_hw_mgr.debug_cfg.csid_debug,
				sizeof(g_ife_hw_mgr.debug_cfg.csid_debug));
	}

	/* INIT IFE Root: do nothing */

	CAM_DBG(CAM_ISP, "INIT IFE CID ... in ctx id:%d",
		ctx->ctx_index);
	/* INIT IFE CID */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_cid, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not INIT IFE CID(id :%d)",
				 hw_mgr_res->res_id);
			goto err;
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
			goto err;
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
			goto err;
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
			goto err;
		}
	}

	mutex_lock(&g_ife_hw_mgr.ctx_mutex);
	if (!atomic_fetch_inc(&g_ife_hw_mgr.active_ctx_cnt)) {
		rc = cam_ife_notify_safe_lut_scm(CAM_IFE_SAFE_ENABLE);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"SAFE SCM call failed:Check TZ/HYP dependency");
			rc = -1;
		}
	}
	mutex_unlock(&g_ife_hw_mgr.ctx_mutex);

	CAM_DBG(CAM_ISP, "start cdm interface");
	rc = cam_cdm_stream_on(ctx->cdm_handle);
	if (rc) {
		CAM_ERR(CAM_ISP, "Can not start cdm (%d)",
			 ctx->cdm_handle);
		goto err;
	}

	/* Apply initial configuration */
	CAM_DBG(CAM_ISP, "Config HW");
	rc = cam_ife_mgr_config_hw(hw_mgr_priv, start_hw_args);
	if (rc) {
		CAM_ERR(CAM_ISP, "Config HW failed");
		goto err;
	}

	CAM_DBG(CAM_ISP, "START IFE OUT ... in ctx id:%d",
		ctx->ctx_index);
	/* start the IFE out devices */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		rc = cam_ife_hw_mgr_start_hw_res(
			&ctx->res_list_ife_out[i], ctx);
		if (rc) {
			CAM_ERR(CAM_ISP, "Can not start IFE OUT (%d)",
				 i);
			goto err;
		}
	}

	CAM_DBG(CAM_ISP, "START IFE SRC ... in ctx id:%d",
		ctx->ctx_index);
	/* Start the IFE mux in devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
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
	ctx->ctx_state = CAM_IFE_HW_MGR_CTX_STARTED;

	/* Start IFE root node: do nothing */
	CAM_DBG(CAM_ISP, "Exit...(success)");
	return 0;
err:
	cam_ife_mgr_stop_hw(hw_mgr_priv, start_hw_args);
	CAM_DBG(CAM_ISP, "Exit...(rc=%d)", rc);
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
	if (!ctx || (ctx->ctx_state < CAM_IFE_HW_MGR_CTX_ACQUIRED)) {
		CAM_ERR(CAM_ISP, "Invalid context is used");
		return -EPERM;
	}

	CAM_DBG(CAM_ISP, "Enter...ctx id:%d",
		ctx->ctx_index);

	/* we should called the stop hw before this already */
	cam_ife_hw_mgr_release_hw_for_ctx(ctx);

	/* reset base info */
	ctx->num_base = 0;
	memset(ctx->base, 0, sizeof(ctx->base));

	/* release cdm handle */
	cam_cdm_release(ctx->cdm_handle);

	/* clean context */
	list_del_init(&ctx->list);
	ctx->ctx_state = CAM_IFE_HW_MGR_CTX_AVAILABLE;
	ctx->is_rdi_only_context = 0;
	ctx->cdm_handle = 0;
	ctx->cdm_ops = NULL;
	atomic_set(&ctx->overflow_pending, 0);
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		ctx->sof_cnt[i] = 0;
		ctx->eof_cnt[i] = 0;
		ctx->epoch_cnt[i] = 0;
	}
	CAM_DBG(CAM_ISP, "Exit...ctx id:%d",
		ctx->ctx_index);
	cam_ife_hw_mgr_put_ctx(&hw_mgr->free_ctx_list, &ctx);
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
		kmd_buf_info->offset     += total_used_bytes;
		prepare->num_hw_update_entries = num_ent;
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

	ctx = prepare->ctxt_to_hw_map;

	CAM_DBG(CAM_ISP,
		"usage=%u left_clk= %lu right_clk=%lu",
		clock_config->usage_type,
		clock_config->left_pix_hz,
		clock_config->right_pix_hz);

	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			clk_rate = 0;
			if (!hw_mgr_res->hw_res[i])
				continue;

			if (hw_mgr_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF)
				if (i == CAM_ISP_HW_SPLIT_LEFT)
					clk_rate =
						clock_config->left_pix_hz;
				else
					clk_rate =
						clock_config->right_pix_hz;
			else if ((hw_mgr_res->res_id >= CAM_ISP_HW_VFE_IN_RDI0)
					&& (hw_mgr_res->res_id <=
					CAM_ISP_HW_VFE_IN_RDI3))
				for (j = 0; j < clock_config->num_rdi; j++)
					clk_rate = max(clock_config->rdi_hz[j],
						clk_rate);
			else
				if (hw_mgr_res->hw_res[i]) {
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
					CAM_ERR(CAM_ISP, "Clock Update failed");
			} else
				CAM_WARN(CAM_ISP, "NULL hw_intf!");
		}
	}

	return rc;
}

static int cam_isp_packet_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	int rc = 0;
	struct cam_isp_generic_blob_info  *blob_info = user_data;
	struct cam_hw_prepare_update_args *prepare = NULL;

	if (!blob_data || (blob_size == 0) || !blob_info) {
		CAM_ERR(CAM_ISP, "Invalid info blob %pK %d prepare %pK",
			blob_data, blob_size, prepare);
		return -EINVAL;
	}

	if (blob_type >= CAM_ISP_GENERIC_BLOB_TYPE_MAX) {
		CAM_ERR(CAM_ISP, "Invalid Blob Type %d Max %d", blob_type,
			CAM_ISP_GENERIC_BLOB_TYPE_MAX);
		return -EINVAL;
	}

	prepare = blob_info->prepare;
	if (!prepare) {
		CAM_ERR(CAM_ISP, "Failed. prepare is NULL, blob_type %d",
			blob_type);
		return -EINVAL;
	}

	switch (blob_type) {
	case CAM_ISP_GENERIC_BLOB_TYPE_HFR_CONFIG: {
		struct cam_isp_resource_hfr_config    *hfr_config =
			(struct cam_isp_resource_hfr_config *)blob_data;

		rc = cam_isp_blob_hfr_update(blob_type, blob_info,
			hfr_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "HFR Update Failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_CLOCK_CONFIG: {
		struct cam_isp_clock_config    *clock_config =
			(struct cam_isp_clock_config *)blob_data;

		rc = cam_isp_blob_clock_update(blob_type, blob_info,
			clock_config, prepare);
		if (rc)
			CAM_ERR(CAM_ISP, "Clock Update Failed");
	}
		break;
	case CAM_ISP_GENERIC_BLOB_TYPE_BW_CONFIG: {
		struct cam_isp_bw_config    *bw_config =
			(struct cam_isp_bw_config *)blob_data;
		struct cam_isp_prepare_hw_update_data   *prepare_hw_data;

		if (!prepare || !prepare->priv ||
			(bw_config->usage_type >= CAM_IFE_HW_NUM_MAX)) {
			CAM_ERR(CAM_ISP, "Invalid inputs");
			rc = -EINVAL;
			break;
		}

		prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
			prepare->priv;

		memcpy(&prepare_hw_data->bw_config[bw_config->usage_type],
			bw_config, sizeof(prepare_hw_data->bw_config[0]));
		prepare_hw_data->bw_config_valid[bw_config->usage_type] = true;

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

	CAM_DBG(CAM_ISP, "enter");

	prepare_hw_data = (struct cam_isp_prepare_hw_update_data  *)
		prepare->priv;

	ctx = (struct cam_ife_hw_mgr_ctx *) prepare->ctxt_to_hw_map;
	hw_mgr = (struct cam_ife_hw_mgr *)hw_mgr_priv;

	rc = cam_packet_util_validate_packet(prepare->packet);
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
		goto end;
	} else
		prepare_hw_data->packet_opcode_type = CAM_ISP_PACKET_UPDATE_DEV;

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

static int cam_ife_mgr_resume_hw(struct cam_ife_hw_mgr_ctx *ctx)
{
	return cam_ife_mgr_bw_control(ctx, CAM_VFE_BW_CONTROL_INCLUDE);
}

static int cam_ife_mgr_cmd(void *hw_mgr_priv, void *cmd_args)
{
	int rc = 0;
	struct cam_isp_hw_cmd_args  *hw_cmd_args  = cmd_args;
	struct cam_ife_hw_mgr_ctx   *ctx;

	if (!hw_mgr_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)hw_cmd_args->ctxt_to_hw_map;
	if (!ctx || (ctx->ctx_state < CAM_IFE_HW_MGR_CTX_ACQUIRED)) {
		CAM_ERR(CAM_ISP, "Fatal: Invalid context is used");
		return -EPERM;
	}

	switch (hw_cmd_args->cmd_type) {
	case CAM_ISP_HW_MGR_CMD_IS_RDI_ONLY_CONTEXT:
		if (ctx->is_rdi_only_context)
			hw_cmd_args->u.is_rdi_only_context = 1;
		else
			hw_cmd_args->u.is_rdi_only_context = 0;

		break;
	case CAM_ISP_HW_MGR_CMD_PAUSE_HW:
		cam_ife_mgr_pause_hw(ctx);
		ctx->ctx_state = CAM_IFE_HW_MGR_CTX_PAUSED;
		break;
	case CAM_ISP_HW_MGR_CMD_RESUME_HW:
		cam_ife_mgr_resume_hw(ctx);
		ctx->ctx_state = CAM_IFE_HW_MGR_CTX_STARTED;
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid HW mgr command:0x%x",
			hw_cmd_args->cmd_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_ife_mgr_cmd_get_sof_timestamp(
	struct cam_ife_hw_mgr_ctx      *ife_ctx,
	uint64_t                       *time_stamp)
{
	int rc = -EINVAL;
	uint32_t i;
	struct cam_ife_hw_mgr_res            *hw_mgr_res;
	struct cam_hw_intf                   *hw_intf;
	struct cam_csid_get_time_stamp_args   csid_get_time;

	list_for_each_entry(hw_mgr_res, &ife_ctx->res_list_ife_csid, list) {
		for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
			if (!hw_mgr_res->hw_res[i] ||
				(i == CAM_ISP_HW_SPLIT_RIGHT))
				continue;
			/*
			 * Get the SOF time stamp from left resource only.
			 * Left resource is master for dual vfe case and
			 * Rdi only context case left resource only hold
			 * the RDI resource
			 */
			hw_intf = hw_mgr_res->hw_res[i]->hw_intf;
			if (hw_intf->hw_ops.process_cmd) {
				csid_get_time.node_res =
					hw_mgr_res->hw_res[i];
				rc = hw_intf->hw_ops.process_cmd(
					hw_intf->hw_priv,
					CAM_IFE_CSID_CMD_GET_TIME_STAMP,
					&csid_get_time,
					sizeof(
					struct cam_csid_get_time_stamp_args));
				if (!rc)
					*time_stamp =
						csid_get_time.time_stamp_val;
			/*
			 * Single VFE case, Get the time stamp from available
			 * one csid hw in the context
			 * Dual VFE case, get the time stamp from master(left)
			 * would be sufficient
			 */
				goto end;
			}
		}
	}
end:
	if (rc)
		CAM_ERR(CAM_ISP, "Getting sof time stamp failed");

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

		for (i = 0; i < CAM_VFE_HW_NUM_MAX; i++) {
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
	int32_t rc = 0;
	struct crm_workq_task        *task = NULL;
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
		CAM_ERR(CAM_ISP, "No empty task frame");
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
 *  b. Return 0 i.e.SUCCESS
 */
static int cam_ife_hw_mgr_is_ctx_affected(
	struct cam_ife_hw_mgr_ctx   *ife_hwr_mgr_ctx,
	uint32_t *affected_core, uint32_t size)
{

	int32_t rc = -EPERM;
	uint32_t i = 0, j = 0;
	uint32_t max_idx =  ife_hwr_mgr_ctx->num_base;
	uint32_t ctx_affected_core_idx[CAM_IFE_HW_NUM_MAX] = {0};

	CAM_DBG(CAM_ISP, "Enter:max_idx = %d", max_idx);

	if ((max_idx >= CAM_IFE_HW_NUM_MAX) ||
		(size > CAM_IFE_HW_NUM_MAX)) {
		CAM_ERR(CAM_ISP, "invalid parameter = %d", max_idx);
		return rc;
	}

	for (i = 0; i < max_idx; i++) {
		if (affected_core[ife_hwr_mgr_ctx->base[i].idx])
			rc = 0;
		else {
			ctx_affected_core_idx[j] = ife_hwr_mgr_ctx->base[i].idx;
			j = j + 1;
		}
	}

	if (rc == 0) {
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
 *  Loop through each context
 *  a. match core_idx
 *  b. For each context from ctx_list Stop the acquired resources
 *  c. Notify CRM with fatal error for the affected isp context
 *  d. For any dual VFE context, if copanion VFE is also serving
 *     other context it should also notify the CRM with fatal error
 */
static int  cam_ife_hw_mgr_process_overflow(
	struct cam_ife_hw_mgr_ctx   *curr_ife_hwr_mgr_ctx,
	struct cam_isp_hw_error_event_data *error_event_data,
	uint32_t curr_core_idx,
	struct cam_hw_event_recovery_data  *recovery_data)
{
	uint32_t affected_core[CAM_IFE_HW_NUM_MAX] = {0};
	struct cam_ife_hw_mgr_ctx   *ife_hwr_mgr_ctx = NULL;
	cam_hw_event_cb_func	         ife_hwr_irq_err_cb;
	struct cam_ife_hw_mgr		*ife_hwr_mgr = NULL;
	struct cam_hw_stop_args          stop_args;
	uint32_t i = 0;

	CAM_DBG(CAM_ISP, "Enter");

	if (!recovery_data) {
		CAM_ERR(CAM_ISP, "recovery_data parameter is NULL");
		return -EINVAL;
	}
	recovery_data->no_of_context = 0;
	/* affected_core is indexed by core_idx*/
	affected_core[curr_core_idx] = 1;

	ife_hwr_mgr = curr_ife_hwr_mgr_ctx->hw_mgr;

	list_for_each_entry(ife_hwr_mgr_ctx,
		&ife_hwr_mgr->used_ctx_list, list) {

		/*
		 * Check if current core_idx matches the HW associated
		 * with this context
		 */
		CAM_DBG(CAM_ISP, "Calling match Hw idx");
		if (cam_ife_hw_mgr_is_ctx_affected(ife_hwr_mgr_ctx,
			affected_core, CAM_IFE_HW_NUM_MAX))
			continue;

		atomic_set(&ife_hwr_mgr_ctx->overflow_pending, 1);

		ife_hwr_irq_err_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_ERROR];

		stop_args.ctxt_to_hw_map = ife_hwr_mgr_ctx;

		/* Add affected_context in list of recovery data*/
		CAM_DBG(CAM_ISP, "Add new entry in affected_ctx_list");
		if (recovery_data->no_of_context < CAM_CTX_MAX)
			recovery_data->affected_ctx[
				recovery_data->no_of_context++] =
				ife_hwr_mgr_ctx;

		/*
		 * In the call back function corresponding ISP context
		 * will update CRM about fatal Error
		 */

		ife_hwr_irq_err_cb(ife_hwr_mgr_ctx->common.cb_priv,
			CAM_ISP_HW_EVENT_ERROR, error_event_data);

	}
	/* fill the affected_core in recovery data */
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		recovery_data->affected_core[i] = affected_core[i];
		CAM_DBG(CAM_ISP, "Vfe core %d is affected (%d)",
			 i, recovery_data->affected_core[i]);
	}
	CAM_DBG(CAM_ISP, "Exit");
	return 0;
}

static int cam_ife_hw_mgr_get_err_type(
	void                              *handler_priv,
	void                              *payload)
{
	struct cam_isp_resource_node         *hw_res_l = NULL;
	struct cam_isp_resource_node         *hw_res_r = NULL;
	struct cam_ife_hw_mgr_ctx            *ife_hwr_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload   *evt_payload;
	struct cam_ife_hw_mgr_res            *isp_ife_camif_res = NULL;
	uint32_t  status = 0;
	uint32_t  core_idx;

	ife_hwr_mgr_ctx = handler_priv;
	evt_payload = payload;

	if (!evt_payload) {
		CAM_ERR(CAM_ISP, "No payload");
		return IRQ_HANDLED;
	}

	core_idx = evt_payload->core_index;
	evt_payload->evt_id = CAM_ISP_HW_EVENT_ERROR;

	list_for_each_entry(isp_ife_camif_res,
		&ife_hwr_mgr_ctx->res_list_ife_src, list) {

		if ((isp_ife_camif_res->res_type ==
			CAM_IFE_HW_MGR_RES_UNINIT) ||
			(isp_ife_camif_res->res_id != CAM_ISP_HW_VFE_IN_CAMIF))
			continue;

		hw_res_l = isp_ife_camif_res->hw_res[CAM_ISP_HW_SPLIT_LEFT];
		hw_res_r = isp_ife_camif_res->hw_res[CAM_ISP_HW_SPLIT_RIGHT];

		CAM_DBG(CAM_ISP, "is_dual_vfe ? = %d\n",
			isp_ife_camif_res->is_dual_vfe);

		/* ERROR check for Left VFE */
		if (!hw_res_l) {
			CAM_DBG(CAM_ISP, "VFE(L) Device is NULL");
			break;
		}

		CAM_DBG(CAM_ISP, "core id= %d, HW id %d", core_idx,
			hw_res_l->hw_intf->hw_idx);

		if (core_idx == hw_res_l->hw_intf->hw_idx) {
			status = hw_res_l->bottom_half_handler(
				hw_res_l, evt_payload);
		}

		if (status)
			break;

		/* ERROR check for Right  VFE */
		if (!hw_res_r) {
			CAM_DBG(CAM_ISP, "VFE(R) Device is NULL");
			continue;
		}
		CAM_DBG(CAM_ISP, "core id= %d, HW id %d", core_idx,
			hw_res_r->hw_intf->hw_idx);

		if (core_idx == hw_res_r->hw_intf->hw_idx) {
			status = hw_res_r->bottom_half_handler(
				hw_res_r, evt_payload);
		}

		if (status)
			break;
	}
	CAM_DBG(CAM_ISP, "Exit (status = %d)!", status);
	return status;
}

static int  cam_ife_hw_mgr_handle_camif_error(
	void                              *handler_priv,
	void                              *payload)
{
	int32_t  error_status = CAM_ISP_HW_ERROR_NONE;
	uint32_t core_idx;
	struct cam_ife_hw_mgr_ctx               *ife_hwr_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload      *evt_payload;
	struct cam_isp_hw_error_event_data       error_event_data = {0};
	struct cam_hw_event_recovery_data        recovery_data = {0};

	ife_hwr_mgr_ctx = handler_priv;
	evt_payload = payload;
	core_idx = evt_payload->core_index;

	error_status = cam_ife_hw_mgr_get_err_type(ife_hwr_mgr_ctx,
		evt_payload);

	if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending))
		return error_status;

	switch (error_status) {
	case CAM_ISP_HW_ERROR_OVERFLOW:
	case CAM_ISP_HW_ERROR_P2I_ERROR:
	case CAM_ISP_HW_ERROR_VIOLATION:
		CAM_DBG(CAM_ISP, "Enter: error_type (%d)", error_status);

		error_event_data.error_type =
				CAM_ISP_HW_ERROR_OVERFLOW;

		cam_ife_hw_mgr_process_overflow(ife_hwr_mgr_ctx,
				&error_event_data,
				core_idx,
				&recovery_data);

		/* Trigger for recovery */
		recovery_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;
		cam_ife_hw_mgr_do_error_recovery(&recovery_data);
		break;
	default:
		CAM_DBG(CAM_ISP, "None error (%d)", error_status);
	}

	return error_status;
}

/*
 * DUAL VFE is valid for PIX processing path
 * This function assumes hw_res[0] is master in case
 * of dual VFE.
 * RDI path does not support DUAl VFE
 */
static int cam_ife_hw_mgr_handle_reg_update(
	void                              *handler_priv,
	void                              *payload)
{
	struct cam_isp_resource_node            *hw_res;
	struct cam_ife_hw_mgr_ctx               *ife_hwr_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload      *evt_payload;
	struct cam_ife_hw_mgr_res               *ife_src_res = NULL;
	cam_hw_event_cb_func                     ife_hwr_irq_rup_cb;
	struct cam_isp_hw_reg_update_event_data  rup_event_data;
	uint32_t  core_idx;
	uint32_t  rup_status = -EINVAL;

	CAM_DBG(CAM_ISP, "Enter");

	ife_hwr_mgr_ctx = handler_priv;
	evt_payload = payload;

	if (!handler_priv || !payload) {
		CAM_ERR(CAM_ISP, "Invalid Parameter");
		return -EPERM;
	}

	core_idx = evt_payload->core_index;
	ife_hwr_irq_rup_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_REG_UPDATE];

	evt_payload->evt_id = CAM_ISP_HW_EVENT_REG_UPDATE;
	list_for_each_entry(ife_src_res,
			&ife_hwr_mgr_ctx->res_list_ife_src, list) {

		if (ife_src_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT)
			continue;

		CAM_DBG(CAM_ISP, "resource id = %d, curr_core_idx = %d",
			 ife_src_res->res_id, core_idx);
		switch (ife_src_res->res_id) {
		case CAM_ISP_HW_VFE_IN_CAMIF:
			if (ife_src_res->is_dual_vfe)
				/* It checks for slave core RUP ACK*/
				hw_res = ife_src_res->hw_res[1];
			else
				hw_res = ife_src_res->hw_res[0];

			if (!hw_res) {
				CAM_ERR(CAM_ISP, "CAMIF device is NULL");
				break;
			}
			CAM_DBG(CAM_ISP,
				"current_core_id = %d , core_idx res = %d",
				 core_idx, hw_res->hw_intf->hw_idx);

			if (core_idx == hw_res->hw_intf->hw_idx) {
				rup_status = hw_res->bottom_half_handler(
					hw_res, evt_payload);
			}
			if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending))
				break;

			if (!rup_status) {
				ife_hwr_irq_rup_cb(
					ife_hwr_mgr_ctx->common.cb_priv,
					CAM_ISP_HW_EVENT_REG_UPDATE,
					&rup_event_data);
			}
			break;

		case CAM_ISP_HW_VFE_IN_RDI0:
		case CAM_ISP_HW_VFE_IN_RDI1:
		case CAM_ISP_HW_VFE_IN_RDI2:
		case CAM_ISP_HW_VFE_IN_RDI3:
			if (!ife_hwr_mgr_ctx->is_rdi_only_context)
				continue;

			/*
			 * This is RDI only context, send Reg update and epoch
			 * HW event to cam context
			 */
			hw_res = ife_src_res->hw_res[0];

			if (!hw_res) {
				CAM_ERR(CAM_ISP, "RDI Device is NULL");
				break;
			}

			if (core_idx == hw_res->hw_intf->hw_idx)
				rup_status = hw_res->bottom_half_handler(
					hw_res, evt_payload);

			if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending))
				break;
			if (!rup_status) {
				/* Send the Reg update hw event */
				ife_hwr_irq_rup_cb(
					ife_hwr_mgr_ctx->common.cb_priv,
					CAM_ISP_HW_EVENT_REG_UPDATE,
					&rup_event_data);
			}
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid resource id (%d)",
				ife_src_res->res_id);
		}

	}

	if (!rup_status)
		CAM_DBG(CAM_ISP, "Exit rup_status = %d", rup_status);

	return 0;
}

static int cam_ife_hw_mgr_check_irq_for_dual_vfe(
	struct cam_ife_hw_mgr_ctx   *ife_hw_mgr_ctx,
	uint32_t                     core_idx0,
	uint32_t                     core_idx1,
	uint32_t                     hw_event_type)
{
	int32_t rc = -1;
	uint32_t *event_cnt = NULL;

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

	if (event_cnt[core_idx0] ==
			event_cnt[core_idx1]) {

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
			"One of the VFE cound not generate hw event %d",
			hw_event_type);
		rc = -1;
		return rc;
	}

	CAM_DBG(CAM_ISP, "Only one core_index has given hw event %d",
			hw_event_type);

	return rc;
}

static int cam_ife_hw_mgr_handle_epoch_for_camif_hw_res(
	void                              *handler_priv,
	void                              *payload)
{
	int32_t rc = -EINVAL;
	struct cam_isp_resource_node         *hw_res_l;
	struct cam_isp_resource_node         *hw_res_r;
	struct cam_ife_hw_mgr_ctx            *ife_hwr_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload   *evt_payload;
	struct cam_ife_hw_mgr_res            *isp_ife_camif_res = NULL;
	cam_hw_event_cb_func                  ife_hwr_irq_epoch_cb;
	struct cam_isp_hw_epoch_event_data    epoch_done_event_data;
	uint32_t  core_idx;
	uint32_t  epoch_status = -EINVAL;
	uint32_t  core_index0;
	uint32_t  core_index1;

	CAM_DBG(CAM_ISP, "Enter");

	ife_hwr_mgr_ctx = handler_priv;
	evt_payload = payload;
	ife_hwr_irq_epoch_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_EPOCH];
	core_idx = evt_payload->core_index;

	evt_payload->evt_id = CAM_ISP_HW_EVENT_EPOCH;

	list_for_each_entry(isp_ife_camif_res,
		&ife_hwr_mgr_ctx->res_list_ife_src, list) {
		if ((isp_ife_camif_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT)
			|| (isp_ife_camif_res->res_id !=
			CAM_ISP_HW_VFE_IN_CAMIF))
			continue;

		hw_res_l = isp_ife_camif_res->hw_res[0];
		hw_res_r = isp_ife_camif_res->hw_res[1];

		switch (isp_ife_camif_res->is_dual_vfe) {
		/* Handling Single VFE Scenario */
		case 0:
			/* EPOCH check for Left side VFE */
			if (!hw_res_l) {
				CAM_ERR(CAM_ISP, "Left Device is NULL");
				break;
			}

			if (core_idx == hw_res_l->hw_intf->hw_idx) {
				epoch_status = hw_res_l->bottom_half_handler(
					hw_res_l, evt_payload);
				if (atomic_read(
					&ife_hwr_mgr_ctx->overflow_pending))
					break;
				if (!epoch_status)
					ife_hwr_irq_epoch_cb(
						ife_hwr_mgr_ctx->common.cb_priv,
						CAM_ISP_HW_EVENT_EPOCH,
						&epoch_done_event_data);
			}

			break;

		/* Handling Dual VFE Scenario */
		case 1:
			/* SOF check for Left side VFE (Master)*/

			if ((!hw_res_l) || (!hw_res_r)) {
				CAM_ERR(CAM_ISP, "Dual VFE Device is NULL");
				break;
			}
			if (core_idx == hw_res_l->hw_intf->hw_idx) {
				epoch_status = hw_res_l->bottom_half_handler(
					hw_res_l, evt_payload);

				if (!epoch_status)
					ife_hwr_mgr_ctx->epoch_cnt[core_idx]++;
				else
					break;
			}

			/* SOF check for Right side VFE */
			if (core_idx == hw_res_r->hw_intf->hw_idx) {
				epoch_status = hw_res_r->bottom_half_handler(
					hw_res_r, evt_payload);

				if (!epoch_status)
					ife_hwr_mgr_ctx->epoch_cnt[core_idx]++;
				else
					break;
			}

			core_index0 = hw_res_l->hw_intf->hw_idx;
			core_index1 = hw_res_r->hw_intf->hw_idx;

			rc = cam_ife_hw_mgr_check_irq_for_dual_vfe(
					ife_hwr_mgr_ctx,
					core_index0,
					core_index1,
					evt_payload->evt_id);

			if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending))
				break;
			if (!rc)
				ife_hwr_irq_epoch_cb(
					ife_hwr_mgr_ctx->common.cb_priv,
					CAM_ISP_HW_EVENT_EPOCH,
					&epoch_done_event_data);

			break;

		/* Error */
		default:
			CAM_ERR(CAM_ISP, "error with hw_res");

		}
	}

	if (!epoch_status)
		CAM_DBG(CAM_ISP, "Exit epoch_status = %d", epoch_status);

	return 0;
}

static int cam_ife_hw_mgr_process_camif_sof(
	struct cam_ife_hw_mgr_res            *isp_ife_camif_res,
	struct cam_ife_hw_mgr_ctx            *ife_hwr_mgr_ctx,
	struct cam_vfe_top_irq_evt_payload   *evt_payload)
{
	struct cam_isp_hw_sof_event_data      sof_done_event_data;
	cam_hw_event_cb_func                  ife_hwr_irq_sof_cb;
	struct cam_isp_resource_node         *hw_res_l = NULL;
	struct cam_isp_resource_node         *hw_res_r = NULL;
	int32_t rc = -EINVAL;
	uint32_t  core_idx;
	uint32_t  sof_status = 0;
	uint32_t  core_index0;
	uint32_t  core_index1;

	CAM_DBG(CAM_ISP, "Enter");
	core_idx = evt_payload->core_index;
	hw_res_l = isp_ife_camif_res->hw_res[0];
	hw_res_r = isp_ife_camif_res->hw_res[1];
	CAM_DBG(CAM_ISP, "is_dual_vfe ? = %d",
		isp_ife_camif_res->is_dual_vfe);

	ife_hwr_irq_sof_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_SOF];

	switch (isp_ife_camif_res->is_dual_vfe) {
	/* Handling Single VFE Scenario */
	case 0:
		/* SOF check for Left side VFE */
		if (!hw_res_l) {
			CAM_ERR(CAM_ISP, "VFE Device is NULL");
			break;
		}
		CAM_DBG(CAM_ISP, "curr_core_idx = %d,core idx hw = %d",
			core_idx, hw_res_l->hw_intf->hw_idx);

		if (core_idx == hw_res_l->hw_intf->hw_idx) {
			sof_status = hw_res_l->bottom_half_handler(hw_res_l,
				evt_payload);
			if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending))
				break;
			if (!sof_status) {
				cam_ife_mgr_cmd_get_sof_timestamp(
					ife_hwr_mgr_ctx,
					&sof_done_event_data.timestamp);

				ife_hwr_irq_sof_cb(
					ife_hwr_mgr_ctx->common.cb_priv,
					CAM_ISP_HW_EVENT_SOF,
					&sof_done_event_data);
			}
		}

		break;

	/* Handling Dual VFE Scenario */
	case 1:
		/* SOF check for Left side VFE */

		if (!hw_res_l) {
			CAM_ERR(CAM_ISP, "VFE Device is NULL");
			break;
		}
		CAM_DBG(CAM_ISP, "curr_core_idx = %d, res hw idx= %d",
				 core_idx,
				hw_res_l->hw_intf->hw_idx);

		if (core_idx == hw_res_l->hw_intf->hw_idx) {
			sof_status = hw_res_l->bottom_half_handler(
				hw_res_l, evt_payload);
			if (!sof_status)
				ife_hwr_mgr_ctx->sof_cnt[core_idx]++;
			else
				break;
		}

		/* SOF check for Right side VFE */
		if (!hw_res_r) {
			CAM_ERR(CAM_ISP, "VFE Device is NULL");
			break;
		}
		CAM_DBG(CAM_ISP, "curr_core_idx = %d, ews hw idx= %d",
				 core_idx,
				hw_res_r->hw_intf->hw_idx);
		if (core_idx == hw_res_r->hw_intf->hw_idx) {
			sof_status = hw_res_r->bottom_half_handler(hw_res_r,
				evt_payload);
			if (!sof_status)
				ife_hwr_mgr_ctx->sof_cnt[core_idx]++;
			else
				break;
		}

		core_index0 = hw_res_l->hw_intf->hw_idx;
		core_index1 = hw_res_r->hw_intf->hw_idx;

		if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending))
			break;

		rc = cam_ife_hw_mgr_check_irq_for_dual_vfe(ife_hwr_mgr_ctx,
			core_index0, core_index1, evt_payload->evt_id);

		if (!rc) {
			cam_ife_mgr_cmd_get_sof_timestamp(
					ife_hwr_mgr_ctx,
					&sof_done_event_data.timestamp);

			ife_hwr_irq_sof_cb(ife_hwr_mgr_ctx->common.cb_priv,
				CAM_ISP_HW_EVENT_SOF, &sof_done_event_data);
		}

		break;

	default:
		CAM_ERR(CAM_ISP, "error with hw_res");
		break;
	}

	CAM_DBG(CAM_ISP, "Exit (sof_status = %d)", sof_status);

	return 0;
}

static int cam_ife_hw_mgr_handle_sof(
	void                              *handler_priv,
	void                              *payload)
{
	int32_t rc = -EINVAL;
	struct cam_isp_resource_node         *hw_res = NULL;
	struct cam_ife_hw_mgr_ctx            *ife_hw_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload   *evt_payload;
	struct cam_ife_hw_mgr_res            *ife_src_res = NULL;
	cam_hw_event_cb_func                  ife_hw_irq_sof_cb;
	struct cam_isp_hw_sof_event_data      sof_done_event_data;
	uint32_t  sof_status = 0;

	CAM_DBG(CAM_ISP, "Enter");

	ife_hw_mgr_ctx = handler_priv;
	evt_payload = payload;
	if (!evt_payload) {
		CAM_ERR(CAM_ISP, "no payload");
		return IRQ_HANDLED;
	}
	ife_hw_irq_sof_cb =
		ife_hw_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_SOF];

	evt_payload->evt_id = CAM_ISP_HW_EVENT_SOF;

	list_for_each_entry(ife_src_res,
		&ife_hw_mgr_ctx->res_list_ife_src, list) {

		if (ife_src_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT)
			continue;

		switch (ife_src_res->res_id) {
		case CAM_ISP_HW_VFE_IN_RDI0:
		case CAM_ISP_HW_VFE_IN_RDI1:
		case CAM_ISP_HW_VFE_IN_RDI2:
		case CAM_ISP_HW_VFE_IN_RDI3:
			/* check if it is rdi only context */
			if (ife_hw_mgr_ctx->is_rdi_only_context) {
				hw_res = ife_src_res->hw_res[0];
				sof_status = hw_res->bottom_half_handler(
					hw_res, evt_payload);

				if (!sof_status) {
					cam_ife_mgr_cmd_get_sof_timestamp(
						ife_hw_mgr_ctx,
						&sof_done_event_data.timestamp);

					ife_hw_irq_sof_cb(
						ife_hw_mgr_ctx->common.cb_priv,
						CAM_ISP_HW_EVENT_SOF,
						&sof_done_event_data);
					CAM_DBG(CAM_ISP, "sof_status = %d",
						sof_status);
				}

				/* this is RDI only context so exit from here */
				return 0;
			}
			break;

		case CAM_ISP_HW_VFE_IN_CAMIF:
			rc = cam_ife_hw_mgr_process_camif_sof(ife_src_res,
				ife_hw_mgr_ctx, evt_payload);
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid resource id :%d",
				ife_src_res->res_id);
			break;
		}
	}

	return 0;
}

static int cam_ife_hw_mgr_handle_eof_for_camif_hw_res(
	void                              *handler_priv,
	void                              *payload)
{
	int32_t rc = -EINVAL;
	struct cam_isp_resource_node         *hw_res_l = NULL;
	struct cam_isp_resource_node         *hw_res_r = NULL;
	struct cam_ife_hw_mgr_ctx            *ife_hwr_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload   *evt_payload;
	struct cam_ife_hw_mgr_res            *isp_ife_camif_res = NULL;
	cam_hw_event_cb_func                  ife_hwr_irq_eof_cb;
	struct cam_isp_hw_eof_event_data      eof_done_event_data;
	uint32_t  core_idx;
	uint32_t  eof_status = 0;
	uint32_t  core_index0;
	uint32_t  core_index1;

	CAM_DBG(CAM_ISP, "Enter");

	ife_hwr_mgr_ctx = handler_priv;
	evt_payload = payload;
	if (!evt_payload) {
		pr_err("%s: no payload\n", __func__);
		return IRQ_HANDLED;
	}
	core_idx = evt_payload->core_index;
	ife_hwr_irq_eof_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_EOF];

	evt_payload->evt_id = CAM_ISP_HW_EVENT_EOF;

	list_for_each_entry(isp_ife_camif_res,
		&ife_hwr_mgr_ctx->res_list_ife_src, list) {

		if ((isp_ife_camif_res->res_type ==
			CAM_IFE_HW_MGR_RES_UNINIT) ||
			(isp_ife_camif_res->res_id != CAM_ISP_HW_VFE_IN_CAMIF))
			continue;

		hw_res_l = isp_ife_camif_res->hw_res[0];
		hw_res_r = isp_ife_camif_res->hw_res[1];

		CAM_DBG(CAM_ISP, "is_dual_vfe ? = %d",
				isp_ife_camif_res->is_dual_vfe);
		switch (isp_ife_camif_res->is_dual_vfe) {
		/* Handling Single VFE Scenario */
		case 0:
			/* EOF check for Left side VFE */
			if (!hw_res_l) {
				pr_err("%s: VFE Device is NULL\n",
					__func__);
				break;
			}
			CAM_DBG(CAM_ISP, "curr_core_idx = %d, core idx hw = %d",
					core_idx, hw_res_l->hw_intf->hw_idx);

			if (core_idx == hw_res_l->hw_intf->hw_idx) {
				eof_status = hw_res_l->bottom_half_handler(
					hw_res_l, evt_payload);
				if (atomic_read(
					&ife_hwr_mgr_ctx->overflow_pending))
					break;
				if (!eof_status)
					ife_hwr_irq_eof_cb(
						ife_hwr_mgr_ctx->common.cb_priv,
						CAM_ISP_HW_EVENT_EOF,
						&eof_done_event_data);
			}

			break;
		/* Handling dual VFE Scenario */
		case 1:
			if ((!hw_res_l) || (!hw_res_r)) {
				CAM_ERR(CAM_ISP, "Dual VFE Device is NULL");
				break;
			}
			if (core_idx == hw_res_l->hw_intf->hw_idx) {
				eof_status = hw_res_l->bottom_half_handler(
					hw_res_l, evt_payload);

				if (!eof_status)
					ife_hwr_mgr_ctx->eof_cnt[core_idx]++;
				else
					break;
			}

			/* EOF check for Right side VFE */
			if (core_idx == hw_res_r->hw_intf->hw_idx) {
				eof_status = hw_res_r->bottom_half_handler(
					hw_res_r, evt_payload);

				if (!eof_status)
					ife_hwr_mgr_ctx->eof_cnt[core_idx]++;
				else
					break;
			}

			core_index0 = hw_res_l->hw_intf->hw_idx;
			core_index1 = hw_res_r->hw_intf->hw_idx;

			rc = cam_ife_hw_mgr_check_irq_for_dual_vfe(
					ife_hwr_mgr_ctx,
					core_index0,
					core_index1,
					evt_payload->evt_id);

			if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending))
				break;

			if (!rc)
				ife_hwr_irq_eof_cb(
					ife_hwr_mgr_ctx->common.cb_priv,
					CAM_ISP_HW_EVENT_EOF,
					&eof_done_event_data);

			break;

		default:
			CAM_ERR(CAM_ISP, "error with hw_res");
		}
	}

	CAM_DBG(CAM_ISP, "Exit (eof_status = %d)", eof_status);

	return 0;
}


static int cam_ife_hw_mgr_handle_buf_done_for_hw_res(
	void                              *handler_priv,
	void                              *payload)

{
	int32_t                              buf_done_status = 0;
	int32_t                              i;
	int32_t                              rc = 0;
	cam_hw_event_cb_func                 ife_hwr_irq_wm_done_cb;
	struct cam_isp_resource_node        *hw_res_l = NULL;
	struct cam_ife_hw_mgr_ctx           *ife_hwr_mgr_ctx = NULL;
	struct cam_vfe_bus_irq_evt_payload  *evt_payload = payload;
	struct cam_ife_hw_mgr_res           *isp_ife_out_res = NULL;
	struct cam_hw_event_recovery_data    recovery_data;
	struct cam_isp_hw_done_event_data    buf_done_event_data = {0};
	struct cam_isp_hw_error_event_data   error_event_data = {0};
	uint32_t  error_resc_handle[CAM_IFE_HW_OUT_RES_MAX];
	uint32_t  num_of_error_handles = 0;

	CAM_DBG(CAM_ISP, "Enter");

	ife_hwr_mgr_ctx = evt_payload->ctx;
	ife_hwr_irq_wm_done_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_DONE];

	evt_payload->evt_id = CAM_ISP_HW_EVENT_DONE;

	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		isp_ife_out_res = &ife_hwr_mgr_ctx->res_list_ife_out[i];

		if (isp_ife_out_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT)
			continue;

		hw_res_l = isp_ife_out_res->hw_res[0];

		/*
		 * DUAL VFE: Index 0 is always a master. In case of composite
		 * Error, if the error is not in master, it needs to be checked
		 * in slave (for debuging purpose only) For other cases:
		 * Index zero is valid
		 */

		if (hw_res_l && (evt_payload->core_index ==
			hw_res_l->hw_intf->hw_idx))
			buf_done_status = hw_res_l->bottom_half_handler(
				hw_res_l, evt_payload);
		else
			continue;

		switch (buf_done_status) {
		case CAM_VFE_IRQ_STATUS_ERR_COMP:
			/*
			 * Write interface can pipeline upto 2 buffer done
			 * strobes from each write client. If any of the client
			 * triggers a third buffer done strobe before a
			 * composite interrupt based on the first buffer doneis
			 * triggered an error irq is set. This scenario can
			 * only happen if a client is 3 frames ahead of the
			 * other clients enabled in the same composite mask.
			 */
		case CAM_VFE_IRQ_STATUS_COMP_OWRT:
			/*
			 * It is an indication that bandwidth is not sufficient
			 * to generate composite done irq within the VBI time.
			 */

			error_resc_handle[num_of_error_handles++] =
					isp_ife_out_res->res_id;

			if (num_of_error_handles > 0) {
				error_event_data.error_type =
					CAM_ISP_HW_ERROR_BUSIF_OVERFLOW;
				goto err;
			}

			break;
		case CAM_VFE_IRQ_STATUS_ERR:
			break;
		case CAM_VFE_IRQ_STATUS_SUCCESS:
			buf_done_event_data.num_handles = 1;
			buf_done_event_data.resource_handle[0] =
				isp_ife_out_res->res_id;

			if (atomic_read(&ife_hwr_mgr_ctx->overflow_pending))
				break;
			/* Report for Successful buf_done event if any */
			if (buf_done_event_data.num_handles > 0 &&
				ife_hwr_irq_wm_done_cb) {
				CAM_DBG(CAM_ISP, "notify isp context");
				ife_hwr_irq_wm_done_cb(
					ife_hwr_mgr_ctx->common.cb_priv,
					CAM_ISP_HW_EVENT_DONE,
					&buf_done_event_data);
			}

			break;
		default:
			/* Do NOTHING */
			error_resc_handle[num_of_error_handles++] =
				isp_ife_out_res->res_id;
			if (num_of_error_handles > 0) {
				error_event_data.error_type =
					CAM_ISP_HW_ERROR_BUSIF_OVERFLOW;
				goto err;
			}
			break;
		}
		if (!buf_done_status)
			CAM_DBG(CAM_ISP,
				"buf_done status:(%d),out_res->res_id: 0x%x",
				buf_done_status, isp_ife_out_res->res_id);
	}

	return rc;

err:
	/*
	 * Report for error if any.
	 * For the first phase, Error is reported as overflow, for all
	 * the affected context and any successful buf_done event is not
	 * reported.
	 */
	rc = cam_ife_hw_mgr_process_overflow(ife_hwr_mgr_ctx,
		&error_event_data, evt_payload->core_index,
		&recovery_data);

	/*
	 * We can temporarily return from here as
	 * for the first phase, we are going to reset entire HW.
	 */

	CAM_DBG(CAM_ISP, "Exit buf_done_status Error = %d",
		buf_done_status);
	return rc;
}

int cam_ife_mgr_do_tasklet_buf_done(void *handler_priv,
	void *evt_payload_priv)
{
	struct cam_ife_hw_mgr_ctx               *ife_hwr_mgr_ctx = handler_priv;
	struct cam_vfe_bus_irq_evt_payload      *evt_payload;
	int rc = -EINVAL;

	if (!handler_priv)
		return rc;

	evt_payload = evt_payload_priv;
	ife_hwr_mgr_ctx = (struct cam_ife_hw_mgr_ctx *)evt_payload->ctx;

	CAM_DBG(CAM_ISP, "addr of evt_payload = %llx core index:0x%x",
		(uint64_t)evt_payload, evt_payload->core_index);
	CAM_DBG(CAM_ISP, "bus_irq_status_0: = %x", evt_payload->irq_reg_val[0]);
	CAM_DBG(CAM_ISP, "bus_irq_status_1: = %x", evt_payload->irq_reg_val[1]);
	CAM_DBG(CAM_ISP, "bus_irq_status_2: = %x", evt_payload->irq_reg_val[2]);
	CAM_DBG(CAM_ISP, "bus_irq_comp_err: = %x", evt_payload->irq_reg_val[3]);
	CAM_DBG(CAM_ISP, "bus_irq_comp_owrt: = %x",
		evt_payload->irq_reg_val[4]);
	CAM_DBG(CAM_ISP, "bus_irq_dual_comp_err: = %x",
		evt_payload->irq_reg_val[5]);
	CAM_DBG(CAM_ISP, "bus_irq_dual_comp_owrt: = %x",
		evt_payload->irq_reg_val[6]);
	/* WM Done */
	return cam_ife_hw_mgr_handle_buf_done_for_hw_res(ife_hwr_mgr_ctx,
		evt_payload_priv);
}

int cam_ife_mgr_do_tasklet(void *handler_priv, void *evt_payload_priv)
{
	struct cam_ife_hw_mgr_ctx            *ife_hwr_mgr_ctx = handler_priv;
	struct cam_vfe_top_irq_evt_payload   *evt_payload;
	int rc = -EINVAL;

	if (!evt_payload_priv)
		return rc;

	evt_payload = evt_payload_priv;
	if (!handler_priv)
		goto put_payload;

	ife_hwr_mgr_ctx = (struct cam_ife_hw_mgr_ctx *)handler_priv;

	CAM_DBG(CAM_ISP, "addr of evt_payload = %pK core_index:%d",
		(void *)evt_payload,
		evt_payload->core_index);
	CAM_DBG(CAM_ISP, "irq_status_0: = %x", evt_payload->irq_reg_val[0]);
	CAM_DBG(CAM_ISP, "irq_status_1: = %x", evt_payload->irq_reg_val[1]);
	CAM_DBG(CAM_ISP, "Violation register: = %x",
		evt_payload->irq_reg_val[2]);

	/*
	 * If overflow/overwrite/error/violation are pending
	 * for this context it needs to be handled remaining
	 * interrupts are ignored.
	 */
	if (g_ife_hw_mgr.debug_cfg.enable_recovery) {
		CAM_DBG(CAM_ISP, "IFE Mgr recovery is enabled");
	       rc = cam_ife_hw_mgr_handle_camif_error(ife_hwr_mgr_ctx,
		    evt_payload_priv);
	} else {
		CAM_DBG(CAM_ISP, "recovery is not enabled");
		rc = 0;
	}

	if (rc) {
		CAM_ERR(CAM_ISP, "Encountered Error (%d), ignoring other irqs",
			 rc);
		goto put_payload;
	}

	CAM_DBG(CAM_ISP, "Calling EOF");
	cam_ife_hw_mgr_handle_eof_for_camif_hw_res(ife_hwr_mgr_ctx,
		evt_payload_priv);

	CAM_DBG(CAM_ISP, "Calling SOF");
	/* SOF IRQ */
	cam_ife_hw_mgr_handle_sof(ife_hwr_mgr_ctx,
		evt_payload_priv);

	CAM_DBG(CAM_ISP, "Calling RUP");
	/* REG UPDATE */
	cam_ife_hw_mgr_handle_reg_update(ife_hwr_mgr_ctx,
		evt_payload_priv);

	CAM_DBG(CAM_ISP, "Calling EPOCH");
	/* EPOCH IRQ */
	cam_ife_hw_mgr_handle_epoch_for_camif_hw_res(ife_hwr_mgr_ctx,
		evt_payload_priv);

put_payload:
	cam_vfe_put_evt_payload(evt_payload->core_info, &evt_payload);
	return IRQ_HANDLED;
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
	g_ife_hw_mgr.debug_cfg.enable_recovery = 0;

	return 0;

err:
	debugfs_remove_recursive(g_ife_hw_mgr.debug_cfg.dentry);
	return -ENOMEM;
}

int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf)
{
	int rc = -EFAULT;
	int i, j;
	struct cam_iommu_handle cdm_handles;

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

	if (cam_smmu_ops(g_ife_hw_mgr.mgr_common.img_iommu_hdl,
		CAM_SMMU_ATTACH)) {
		CAM_ERR(CAM_ISP, "Attach iommu handle failed.");
		goto attach_fail;
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
		for (j = 0; j < CAM_IFE_HW_OUT_RES_MAX; j++) {
			INIT_LIST_HEAD(&g_ife_hw_mgr.ctx_pool[i].
				res_list_ife_out[j].list);
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

		g_ife_hw_mgr.ctx_pool[i].ctx_state =
			CAM_IFE_HW_MGR_CTX_AVAILABLE;
		list_add_tail(&g_ife_hw_mgr.ctx_pool[i].list,
			&g_ife_hw_mgr.free_ctx_list);
	}

	/* Create Worker for ife_hw_mgr with 10 tasks */
	rc = cam_req_mgr_workq_create("cam_ife_worker", 10,
			&g_ife_hw_mgr.workq, CRM_WORKQ_USAGE_NON_IRQ);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "Unable to create worker");
		goto end;
	}

	/* fill return structure */
	hw_mgr_intf->hw_mgr_priv = &g_ife_hw_mgr;
	hw_mgr_intf->hw_get_caps = cam_ife_mgr_get_hw_caps;
	hw_mgr_intf->hw_acquire = cam_ife_mgr_acquire_hw;
	hw_mgr_intf->hw_start = cam_ife_mgr_start_hw;
	hw_mgr_intf->hw_stop = cam_ife_mgr_stop_hw;
	hw_mgr_intf->hw_read = cam_ife_mgr_read;
	hw_mgr_intf->hw_write = cam_ife_mgr_write;
	hw_mgr_intf->hw_release = cam_ife_mgr_release_hw;
	hw_mgr_intf->hw_prepare_update = cam_ife_mgr_prepare_hw_update;
	hw_mgr_intf->hw_config = cam_ife_mgr_config_hw;
	hw_mgr_intf->hw_cmd = cam_ife_mgr_cmd;

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
	cam_smmu_ops(g_ife_hw_mgr.mgr_common.img_iommu_hdl,
		CAM_SMMU_DETACH);
attach_fail:
	cam_smmu_destroy_handle(g_ife_hw_mgr.mgr_common.img_iommu_hdl);
	g_ife_hw_mgr.mgr_common.img_iommu_hdl = -1;
	return rc;
}
