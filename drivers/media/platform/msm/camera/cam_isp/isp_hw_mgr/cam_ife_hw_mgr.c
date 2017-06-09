/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

#define CAM_IFE_HW_ENTRIES_MAX  20

static struct cam_ife_hw_mgr g_ife_hw_mgr;

static int cam_ife_mgr_get_hw_caps(void *hw_mgr_priv,
	void *hw_caps_args)
{
	int rc = 0;
	int i;
	struct cam_ife_hw_mgr             *hw_mgr = hw_mgr_priv;
	struct cam_query_cap_cmd          *query = hw_caps_args;
	struct cam_isp_query_cap_cmd       query_isp;

	CDBG("%s: enter\n", __func__);

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

	CDBG("%s: exit rc :%d !\n", __func__, rc);

	return rc;
}

static int cam_ife_hw_mgr_is_rdi_res(uint32_t format)
{
	int rc = 0;

	switch (format) {
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
		CDBG("%s: enabled vfe hardware %d\n", __func__,
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
	pr_err("%s: INIT HW res failed! (type:%d, id:%d)", __func__,
		isp_hw_res->res_type, isp_hw_res->res_id);
	return rc;
}

static int cam_ife_hw_mgr_start_hw_res(
	struct cam_ife_hw_mgr_res   *isp_hw_res)
{
	int i;
	int rc = -1;
	struct cam_hw_intf      *hw_intf;

	for (i = 0; i < CAM_ISP_HW_SPLIT_MAX; i++) {
		if (!isp_hw_res->hw_res[i])
			continue;
		hw_intf = isp_hw_res->hw_res[i]->hw_intf;
		if (hw_intf->hw_ops.start) {
			rc = hw_intf->hw_ops.start(hw_intf->hw_priv,
				isp_hw_res->hw_res[i],
				sizeof(struct cam_isp_resource_node));
			if (rc) {
				pr_err("%s: Can not start HW resources!\n",
					__func__);
				goto err;
			}
		} else {
			pr_err("%s:function null\n", __func__);
			goto err;
		}
	}

	return 0;
err:
	pr_err("%s: Start hw res failed! (type:%d, id:%d)", __func__,
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
			pr_err("%s:stop null\n", __func__);
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
		pr_err("No more free ife hw mgr ctx!\n");
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
				pr_err("%s:Release hw resrouce id %d failed!\n",
					__func__, isp_hw_res->res_id);
			isp_hw_res->hw_res[i] = NULL;
		} else
			pr_err("%s:Release null\n", __func__);
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

	CDBG("%s:%d: release context completed ctx id:%d\n",
		__func__, __LINE__, ife_ctx->ctx_index);

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
		pr_err("No more free ife hw mgr ctx!\n");
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
		CDBG("%s: Add split id = %d for base idx = %d\n", __func__,
			split_id, base_idx);
		ctx->base[0].split_id = split_id;
		ctx->base[0].idx      = base_idx;
		ctx->num_base++;
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
			CDBG("%s: Add split id = %d for base idx = %d\n",
				__func__, split_id, base_idx);
			ctx->base[ctx->num_base].split_id = split_id;
			ctx->base[ctx->num_base].idx      = base_idx;
			ctx->num_base++;
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
		pr_err("%s: Error! Mux List empty\n", __func__);
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
			if (res->res_id == CAM_ISP_HW_VFE_IN_CAMIF)
				cam_ife_mgr_add_base_info(ctx, i,
					res->hw_intf->hw_idx);

			else
				cam_ife_mgr_add_base_info(ctx,
						CAM_ISP_HW_SPLIT_MAX,
						res->hw_intf->hw_idx);
		}
	}
	CDBG("%s: ctx base num = %d\n", __func__, ctx->num_base);

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
		pr_err("%s: invalid resource type\n", __func__);
		goto err;
	}

	vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_OUT;
	vfe_acquire.tasklet = ife_ctx->common.tasklet_info;

	ife_out_res = &ife_ctx->res_list_ife_out[vfe_out_res_id & 0xFF];
	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];

		if (vfe_out_res_id != out_port->res_type)
			continue;

		vfe_acquire.vfe_out.cdm_ops = ife_ctx->cdm_ops;
		vfe_acquire.vfe_out.out_port_info = out_port;
		vfe_acquire.vfe_out.split_id = CAM_ISP_HW_SPLIT_LEFT;
		vfe_acquire.vfe_out.unique_id = ife_ctx->ctx_index;
		hw_intf = ife_src_res->hw_res[0]->hw_intf;
		rc = hw_intf->hw_ops.reserve(hw_intf->hw_priv,
			&vfe_acquire,
			sizeof(struct cam_vfe_acquire_args));
		if (rc) {
			pr_err("%s: Can not acquire out resource 0x%x\n",
				__func__, out_port->res_type);
			goto err;
		}
		break;
	}

	if (i == in_port->num_out_res) {
		pr_err("%s: Can not acquire out resource\n", __func__);
		goto err;
	}

	ife_out_res->hw_res[0] = vfe_acquire.vfe_out.rsrc_node;
	ife_out_res->is_dual_vfe = 0;
	ife_out_res->res_id = vfe_out_res_id;
	ife_out_res->res_type = CAM_ISP_RESOURCE_VFE_OUT;

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
			pr_err("%s: invalid output resource type 0x%x\n",
				__func__, out_port->res_type);
			continue;
		}

		if (cam_ife_hw_mgr_is_rdi_res(out_port->res_type))
			continue;

		CDBG("%s: res_type 0x%x\n",
			__func__, out_port->res_type);

		ife_out_res = &ife_ctx->res_list_ife_out[k];
		ife_out_res->is_dual_vfe = in_port->usage_type;

		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_OUT;
		vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		vfe_acquire.vfe_out.cdm_ops = ife_ctx->cdm_ops;
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
				pr_err("%s:Can not acquire out resource 0x%x\n",
					__func__, out_port->res_type);
				goto err;
			}

			ife_out_res->hw_res[j] =
				vfe_acquire.vfe_out.rsrc_node;
			CDBG("%s: resource type :0x%x res id:0x%x\n",
				__func__, ife_out_res->hw_res[j]->res_type,
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
			pr_err("%s: Fatal: Unknown IFE SRC resource!\n",
				__func__);
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
			pr_err("%s: No more free hw mgr resource!\n", __func__);
			goto err;
		}
		cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_src,
			&ife_src_res);

		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_IN;
		vfe_acquire.tasklet = ife_ctx->common.tasklet_info;
		vfe_acquire.rsrc_type = CAM_ISP_RESOURCE_VFE_IN;
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
			pr_err("%s: Wrong IFE CSID Resource Node!\n",
				__func__);
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
				pr_err("%s:Can not acquire IFE HW res %d!\n",
					__func__, csid_res->res_id);
				goto err;
			}
			ife_src_res->hw_res[i] = vfe_acquire.vfe_in.rsrc_node;
			CDBG("%s:acquire success res type :0x%x res id:0x%x\n",
				__func__, ife_src_res->hw_res[i]->res_type,
				ife_src_res->hw_res[i]->res_id);

		}

		/* It should be one to one mapping between
		 * csid resource and ife source resource
		 */
		csid_res->child[0] = ife_src_res;
		csid_res->num_children = 1;
		ife_src_res->parent = csid_res;
		csid_res->child[csid_res->num_children++] = ife_src_res;
	}

	return 0;
err:
	/* release resource at the entry function */
	return rc;
}

static int cam_ife_hw_mgr_acquire_res_ife_csid_ipp(
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_isp_in_port_info        *in_port,
	uint32_t                            cid_res_id)
{
	int rc = -1;
	int i, j;

	struct cam_ife_hw_mgr               *ife_hw_mgr;
	struct cam_ife_hw_mgr_res           *csid_res;
	struct cam_hw_intf                   *hw_intf;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;

	ife_hw_mgr = ife_ctx->hw_mgr;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &csid_res);
	if (rc) {
		pr_err("%s: No more free hw mgr resource!\n", __func__);
		goto err;
	}
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_csid, &csid_res);

	csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
	csid_acquire.res_id = CAM_IFE_PIX_PATH_RES_IPP;
	csid_acquire.cid = cid_res_id;
	csid_acquire.in_port = in_port;

	if (in_port->usage_type)
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_MASTER;
	else
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;



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

	if (i == CAM_IFE_CSID_HW_NUM_MAX) {
		pr_err("%s: Can not acquire ife csid ipp resrouce!\n",
			__func__);
		goto err;
	}

	CDBG("%s: acquired csid(%d) left ipp resrouce successfully!\n",
		__func__, i);

	csid_res->res_type = CAM_ISP_RESOURCE_PIX_PATH;
	csid_res->res_id = CAM_IFE_PIX_PATH_RES_IPP;
	csid_res->is_dual_vfe = in_port->usage_type;
	csid_res->hw_res[0] = csid_acquire.node_res;
	csid_res->hw_res[1] = NULL;

	if (csid_res->is_dual_vfe) {
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_SLAVE;

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
			pr_err("%s: Can not acquire ife csid rdi resrouce!\n",
				__func__);
			goto err;
		}
		csid_res->hw_res[1] = csid_acquire.node_res;

		CDBG("%s:acquired csid(%d)right ipp resrouce successfully!\n",
			__func__, j);

	}
	csid_res->parent = &ife_ctx->res_list_ife_in;
	ife_ctx->res_list_ife_in.child[
		ife_ctx->res_list_ife_in.num_children++] = csid_res;

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
		CDBG("%s: maximum rdi output type exceeded\n", __func__);
		break;
	}

	return path_id;
}

static int cam_ife_hw_mgr_acquire_res_ife_csid_rdi(
	struct cam_ife_hw_mgr_ctx     *ife_ctx,
	struct cam_isp_in_port_info   *in_port,
	uint32_t                       cid_res_id)
{
	int rc = -1;
	int i, j;

	struct cam_ife_hw_mgr               *ife_hw_mgr;
	struct cam_ife_hw_mgr_res           *csid_res;
	struct cam_hw_intf                   *hw_intf;
	struct cam_isp_out_port_info        *out_port;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;

	ife_hw_mgr = ife_ctx->hw_mgr;

	for (i = 0; i < in_port->num_out_res; i++) {
		out_port = &in_port->data[i];
		if (!cam_ife_hw_mgr_is_rdi_res(out_port->res_type))
			continue;

		rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list,
			&csid_res);
		if (rc) {
			pr_err("%s: No more free hw mgr resource!\n",
				__func__);
			goto err;
		}
		cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_csid, &csid_res);

		/*
		 * no need to check since we are doing one to one mapping
		 * between the csid rdi type and out port rdi type
		 */

		csid_acquire.res_id =
			cam_ife_hw_mgr_get_ife_csid_rdi_res_type(
				out_port->res_type);

		csid_acquire.res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_acquire.cid = cid_res_id;
		csid_acquire.in_port = in_port;
		csid_acquire.sync_mode = CAM_ISP_HW_SYNC_NONE;

		for (j = 0; j < CAM_IFE_CSID_HW_NUM_MAX; j++) {
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
			pr_err("%s: Can not acquire ife csid rdi resrouce!\n",
				__func__);
			goto err;
		}

		csid_res->res_type = CAM_ISP_RESOURCE_PIX_PATH;
		csid_res->res_id = csid_acquire.res_id;
		csid_res->is_dual_vfe = 0;
		csid_res->hw_res[0] = csid_acquire.node_res;
		csid_res->hw_res[1] = NULL;

		csid_res->parent = &ife_ctx->res_list_ife_in;
		ife_ctx->res_list_ife_in.child[
			ife_ctx->res_list_ife_in.num_children++] = csid_res;
	}

	return 0;
err:
	/* resource resources at entry funciton */
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
		pr_err("%s: No Free resource for this context!\n", __func__);
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

static int cam_ife_mgr_acquire_cid_res(
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_isp_in_port_info        *in_port,
	uint32_t                           *cid_res_id)
{
	int rc = -1;
	int i, j;
	struct cam_ife_hw_mgr               *ife_hw_mgr;
	struct cam_ife_hw_mgr_res           *cid_res;
	struct cam_hw_intf                  *hw_intf;
	struct cam_csid_hw_reserve_resource_args  csid_acquire;

	/* no dual vfe for TPG */
	if ((in_port->res_type == CAM_ISP_IFE_IN_RES_TPG) &&
		(in_port->usage_type != 0)) {
		pr_err("%s: No Dual VFE on TPG input!\n", __func__);
		goto err;
	}

	ife_hw_mgr = ife_ctx->hw_mgr;

	rc = cam_ife_hw_mgr_get_res(&ife_ctx->free_res_list, &cid_res);
	if (rc) {
		pr_err("%s: No more free hw mgr resource!\n", __func__);
		goto err;
	}
	cam_ife_hw_mgr_put_res(&ife_ctx->res_list_ife_cid, &cid_res);

	csid_acquire.res_type = CAM_ISP_RESOURCE_CID;
	csid_acquire.in_port = in_port;

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
		pr_err("%s: Can not acquire ife csid rdi resrouce!\n",
			__func__);
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
			pr_err("%s: Can not acquire ife csid rdi resrouce!\n",
				__func__);
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
static int cam_ife_mgr_acquire_hw_for_ctx(
	struct cam_ife_hw_mgr_ctx          *ife_ctx,
	struct cam_isp_in_port_info        *in_port)
{
	int rc                                    = -1;
	int is_dual_vfe                           = 0;
	int pixel_count                           = 0;
	int rdi_count                             = 0;
	uint32_t                                cid_res_id = 0;

	is_dual_vfe = in_port->usage_type;

	/* get root node resource */
	rc = cam_ife_hw_mgr_acquire_res_root(ife_ctx, in_port);
	if (rc) {
		pr_err("%s:%d: Can not acquire csid rx resource!\n",
			__func__, __LINE__);
		goto err;
	}

	/* get cid resource */
	rc = cam_ife_mgr_acquire_cid_res(ife_ctx, in_port, &cid_res_id);
	if (rc) {
		pr_err("%s%d: Acquire IFE CID resource Failed!\n",
			__func__, __LINE__);
		goto err;
	}

	cam_ife_hw_mgr_preprocess_out_port(ife_ctx, in_port,
		&pixel_count, &rdi_count);

	if (!pixel_count && !rdi_count) {
		pr_err("%s: Error! no PIX or RDI resource\n", __func__);
		return -EINVAL;
	}

	if (pixel_count) {
		/* get ife csid IPP resrouce */
		rc = cam_ife_hw_mgr_acquire_res_ife_csid_ipp(ife_ctx, in_port,
				cid_res_id);
		if (rc) {
			pr_err("%s%d: Acquire IFE CSID IPP resource Failed!\n",
				__func__, __LINE__);
			goto err;
		}
	}

	if (rdi_count) {
		/* get ife csid rdi resource */
		rc = cam_ife_hw_mgr_acquire_res_ife_csid_rdi(ife_ctx, in_port,
			cid_res_id);
		if (rc) {
			pr_err("%s%d: Acquire IFE CSID RDI resource Failed!\n",
				__func__, __LINE__);
			goto err;
		}
	}

	/* get ife src resource */
	rc = cam_ife_hw_mgr_acquire_res_ife_src(ife_ctx, in_port);
	if (rc) {
		pr_err("%s%d: Acquire IFE SRC resource Failed!\n",
			__func__, __LINE__);
		goto err;
	}

	rc = cam_ife_hw_mgr_acquire_res_ife_out(ife_ctx, in_port);
	if (rc) {
		pr_err("%s%d: Acquire IFE OUT resource Failed!\n",
			__func__, __LINE__);
		goto err;
	}

	return 0;
err:
	/* release resource at the acquire entry funciton */
	return rc;
}

void cam_ife_cam_cdm_callback(uint32_t handle, void *userdata,
	enum cam_cdm_cb_status status, uint32_t cookie)
{
	CDBG("%s: Called by CDM hdl=%x, udata=%pK, status=%d, cookie=%d\n",
		__func__, handle, userdata, status, cookie);
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
	struct cam_cdm_acquire_data cdm_acquire;

	CDBG("%s: Enter...\n", __func__);

	if (!acquire_args || acquire_args->num_acq <= 0) {
		pr_err("%s: Nothing to acquire. Seems like error\n", __func__);
		return -EINVAL;
	}

	/* get the ife ctx */
	rc = cam_ife_hw_mgr_get_ctx(&ife_hw_mgr->free_ctx_list, &ife_ctx);
	if (rc || !ife_ctx) {
		pr_err("Get ife hw context failed!\n");
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
	if (!cam_cdm_acquire(&cdm_acquire)) {
		CDBG("Successfully acquired the CDM HW hdl=%x\n",
			cdm_acquire.handle);
		ife_ctx->cdm_handle = cdm_acquire.handle;
		ife_ctx->cdm_ops = cdm_acquire.ops;
	} else {
		pr_err("Failed to acquire the CDM HW\n");
		goto err;
	}

	isp_resource = (struct cam_isp_resource *)acquire_args->acquire_info;

	/* acquire HW resources */
	for (i = 0; i < acquire_args->num_acq; i++) {
		if (isp_resource[i].resource_id != CAM_ISP_RES_ID_PORT)
			continue;

		CDBG("%s: start copy from user handle %lld with len = %d\n",
			__func__, isp_resource[i].res_hdl,
			isp_resource[i].length);

		in_port = memdup_user((void __user *)isp_resource[i].res_hdl,
			isp_resource[i].length);
		if (in_port > 0) {
			rc = cam_ife_mgr_acquire_hw_for_ctx(ife_ctx, in_port);
			kfree(in_port);
			if (rc) {
				pr_err("%s: can not acquire resource!\n",
					__func__);
				goto free_res;
			}
		} else {
			pr_err("%s: copy from user failed with in_port = %pK",
				__func__, in_port);
			rc = -EFAULT;
			goto free_res;
		}
	}
	/* Process base info */
	rc = cam_ife_mgr_process_base_info(ife_ctx);
	if (rc) {
		pr_err("%s: Error process) base info!\n",
			__func__);
		return -EINVAL;
	}

	acquire_args->ctxt_to_hw_map = ife_ctx;
	ife_ctx->ctx_in_use = 1;

	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->used_ctx_list, &ife_ctx);

	CDBG("%s: Exit...(success)!\n", __func__);

	return 0;
free_res:
	cam_ife_hw_mgr_release_hw_for_ctx(ife_ctx);
	cam_ife_hw_mgr_put_ctx(&ife_hw_mgr->free_ctx_list, &ife_ctx);
err:
	CDBG("%s: Exit...(rc=%d)!\n", __func__, rc);
	return rc;
}

/* entry function: config_hw */
static int cam_ife_mgr_config_hw(void *hw_mgr_priv,
					void *config_hw_args)
{
	int rc = -1, i;
	struct cam_hw_start_args *cfg;
	struct cam_hw_update_entry *cmd;
	struct cam_cdm_bl_request *cdm_cmd;
	struct cam_ife_hw_mgr_ctx *ctx;

	CDBG("%s: Enter\n", __func__);
	if (!hw_mgr_priv || !config_hw_args) {
		pr_err("%s%d: Invalid arguments\n", __func__, __LINE__);
		return -EINVAL;
	}

	cfg = config_hw_args;
	ctx = (struct cam_ife_hw_mgr_ctx *)cfg->ctxt_to_hw_map;
	if (!ctx) {
		pr_err("%s: Fatal: Invalid context is used!\n", __func__);
		return -EPERM;
	}

	if (!ctx->ctx_in_use || !ctx->cdm_cmd) {
		pr_err("%s: Invalid context parameters !\n", __func__);
		return -EPERM;
	}

	CDBG("%s%d: Enter...ctx id:%d\n", __func__, __LINE__, ctx->ctx_index);

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

		rc = cam_cdm_submit_bls(ctx->cdm_handle, cdm_cmd);
		if (rc)
			pr_err("Failed to apply the configs\n");
	} else {
		pr_err("No commands to config\n");
	}
	CDBG("%s: Exit\n", __func__);

	return rc;
}

static int cam_ife_mgr_stop_hw_in_overflow(void *hw_mgr_priv,
		void *stop_hw_args)
{
	int                               rc        = 0;
	struct cam_hw_stop_args          *stop_args = stop_hw_args;
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	struct cam_ife_hw_mgr_ctx        *ctx;
	uint32_t                          i, master_base_idx = 0;

	if (!hw_mgr_priv || !stop_hw_args) {
		pr_err("%s%d: Invalid arguments\n", __func__, __LINE__);
		return -EINVAL;
	}
	ctx = (struct cam_ife_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		pr_err("%s: Fatal: Invalid context is used!\n", __func__);
		return -EPERM;
	}

	CDBG("%s%d: Enter...ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);

	/* stop resource will remove the irq mask from the hardware */
	if (!ctx->num_base) {
		pr_err("%s%d: error number of bases are zero\n",
			__func__, __LINE__);
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
	 * if Context does not have PIX resources and has only RDI resource
	 * then take the first base index.
	 */

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

	/* update vote bandwidth should be done at the HW layer */

	CDBG("%s%d Exit...ctx id:%d rc :%d\n", __func__, __LINE__,
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
	uint32_t                          i, master_base_idx = 0;

	if (!hw_mgr_priv || !stop_hw_args) {
		pr_err("%s%d: Invalid arguments\n", __func__, __LINE__);
		return -EINVAL;
	}
	ctx = (struct cam_ife_hw_mgr_ctx *)stop_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		pr_err("%s: Fatal: Invalid context is used!\n", __func__);
		return -EPERM;
	}

	CDBG("%s%d: Enter...ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);

	/* Note:stop resource will remove the irq mask from the hardware */

	if (!ctx->num_base) {
		pr_err("%s%d: error number of bases are zero\n",
			__func__, __LINE__);
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
			master_base_idx, CAM_CSID_HALT_AT_FRAME_BOUNDARY);

	/* stop rest of the CSID paths  */
	for (i = 0; i < ctx->num_base; i++) {
		if (i == master_base_idx)
			continue;

		cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_csid,
			ctx->base[i].idx, CAM_CSID_HALT_AT_FRAME_BOUNDARY);
	}

	/* Stop the master CIDs first */
	cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_cid,
			master_base_idx, CAM_CSID_HALT_AT_FRAME_BOUNDARY);

	/* stop rest of the CIDs  */
	for (i = 0; i < ctx->num_base; i++) {
		if (i == master_base_idx)
			continue;
		cam_ife_mgr_csid_stop_hw(ctx, &ctx->res_list_ife_cid,
			ctx->base[i].idx, CAM_CSID_HALT_AT_FRAME_BOUNDARY);
	}

	if (cam_cdm_stream_off(ctx->cdm_handle))
		pr_err("%s%d: CDM stream off failed %d\n",
			__func__, __LINE__, ctx->cdm_handle);

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

	CDBG("%s%d Exit...ctx id:%d rc :%d\n", __func__, __LINE__,
		ctx->ctx_index, rc);

	return rc;
}

static int cam_ife_mgr_reset_hw(struct cam_ife_hw_mgr *hw_mgr,
			uint32_t hw_idx)
{
	uint32_t i = 0;
	struct cam_hw_intf             *csid_hw_intf;
	struct cam_hw_intf             *vfe_hw_intf;
	struct cam_csid_reset_cfg_args  csid_reset_args;

	if (!hw_mgr) {
		CDBG("%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	/* Reset IFE CSID HW */
	csid_reset_args.reset_type = CAM_IFE_CSID_RESET_GLOBAL;

	for (i = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		if (hw_idx != hw_mgr->csid_devices[i]->hw_idx)
			continue;

		csid_hw_intf = hw_mgr->csid_devices[i];
		csid_hw_intf->hw_ops.reset(csid_hw_intf->hw_priv,
			&csid_reset_args,
			sizeof(struct cam_csid_reset_cfg_args));
		break;
	}

	/* Reset VFE HW*/
	for (i = 0; i < CAM_VFE_HW_NUM_MAX; i++) {
		if (hw_idx != hw_mgr->ife_devices[i]->hw_idx)
			continue;
		CDBG("%d:VFE (id = %d) reset\n", __LINE__, hw_idx);
		vfe_hw_intf = hw_mgr->ife_devices[i];
		vfe_hw_intf->hw_ops.reset(vfe_hw_intf->hw_priv, NULL, 0);
		break;
	}

	CDBG("%d: Exit Successfully\n", __LINE__);
	return 0;
}

static int cam_ife_mgr_restart_hw(void *hw_mgr_priv,
		void *start_hw_args)
{
	int                               rc = -1;
	struct cam_hw_start_args         *start_args = start_hw_args;
	struct cam_ife_hw_mgr_ctx        *ctx;
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	uint32_t                          i;

	if (!hw_mgr_priv || !start_hw_args) {
		pr_err("%s%d: Invalid arguments\n", __func__, __LINE__);
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)start_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		pr_err("%s: Invalid context is used!\n", __func__);
		return -EPERM;
	}

	CDBG("%s%d Enter... ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);

	CDBG("%s%d START IFE OUT ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* start the IFE out devices */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		rc = cam_ife_hw_mgr_start_hw_res(&ctx->res_list_ife_out[i]);
		if (rc) {
			pr_err("%s: Can not start IFE OUT (%d)!\n",
				__func__, i);
			goto err;
		}
	}

	CDBG("%s%d START IFE SRC ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* Start the IFE mux in devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not start IFE MUX (%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}

	CDBG("%s:%d: START CSID HW ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* Start the IFE CSID HW devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not start IFE CSID (%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}

	CDBG("%s%d START CID SRC ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* Start the IFE CID HW devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_cid, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not start IFE CSID (%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}

	/* Start IFE root node: do nothing */
	CDBG("%s: Exit...(success)\n", __func__);
	return 0;

err:
	cam_ife_mgr_stop_hw(hw_mgr_priv, start_hw_args);
	CDBG("%s: Exit...(rc=%d)\n", __func__, rc);
	return rc;
}

static int cam_ife_mgr_start_hw(void *hw_mgr_priv, void *start_hw_args)
{
	int                               rc = -1;
	struct cam_hw_start_args         *start_args = start_hw_args;
	struct cam_ife_hw_mgr_ctx        *ctx;
	struct cam_ife_hw_mgr_res        *hw_mgr_res;
	uint32_t                          i;

	if (!hw_mgr_priv || !start_hw_args) {
		pr_err("%s%d: Invalid arguments\n", __func__, __LINE__);
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)start_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		pr_err("%s: Invalid context is used!\n", __func__);
		return -EPERM;
	}

	CDBG("%s%d Enter... ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);

	/* update Bandwidth should be done at the hw layer */

	cam_tasklet_start(ctx->common.tasklet_info);

	/* INIT IFE Root: do nothing */

	CDBG("%s%d INIT IFE CID ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* INIT IFE CID */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_cid, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not INIT IFE CID.(id :%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}


	CDBG("%s%d INIT IFE csid ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);

	/* INIT IFE csid */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not INIT IFE CSID.(id :%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}

	/* INIT IFE SRC */
	CDBG("%s%d INIT IFE SRC in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		rc = cam_ife_hw_mgr_init_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not INIT IFE SRC (%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}

	/* INIT IFE OUT */
	CDBG("%s%d INIT IFE OUT RESOURCES in ctx id:%d\n", __func__,
		__LINE__, ctx->ctx_index);

	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		rc = cam_ife_hw_mgr_init_hw_res(&ctx->res_list_ife_out[i]);
		if (rc) {
			pr_err("%s: Can not INIT IFE OUT (%d)!\n",
				__func__, ctx->res_list_ife_out[i].res_id);
			goto err;
		}
	}

	CDBG("%s: start cdm interface\n", __func__);
	rc = cam_cdm_stream_on(ctx->cdm_handle);
	if (rc) {
		pr_err("%s: Can not start cdm (%d)!\n",
			__func__, ctx->cdm_handle);
		goto err;
	}

	/* Apply initial configuration */
	CDBG("%s: Config HW\n", __func__);
	rc = cam_ife_mgr_config_hw(hw_mgr_priv, start_hw_args);
	if (rc) {
		pr_err("%s: Config HW failed\n", __func__);
		goto err;
	}

	CDBG("%s%d START IFE OUT ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* start the IFE out devices */
	for (i = 0; i < CAM_IFE_HW_OUT_RES_MAX; i++) {
		rc = cam_ife_hw_mgr_start_hw_res(&ctx->res_list_ife_out[i]);
		if (rc) {
			pr_err("%s: Can not start IFE OUT (%d)!\n",
				__func__, i);
			goto err;
		}
	}

	CDBG("%s%d START IFE SRC ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* Start the IFE mux in devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_src, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not start IFE MUX (%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}

	CDBG("%s:%d: START CSID HW ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* Start the IFE CSID HW devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_csid, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not start IFE CSID (%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}

	CDBG("%s%d START CID SRC ... in ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	/* Start the IFE CID HW devices */
	list_for_each_entry(hw_mgr_res, &ctx->res_list_ife_cid, list) {
		rc = cam_ife_hw_mgr_start_hw_res(hw_mgr_res);
		if (rc) {
			pr_err("%s: Can not start IFE CSID (%d)!\n",
				__func__, hw_mgr_res->res_id);
			goto err;
		}
	}

	/* Start IFE root node: do nothing */
	CDBG("%s: Exit...(success)\n", __func__);
	return 0;
err:
	cam_ife_mgr_stop_hw(hw_mgr_priv, start_hw_args);
	CDBG("%s: Exit...(rc=%d)\n", __func__, rc);
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

	if (!hw_mgr_priv || !release_hw_args) {
		pr_err("%s%d: Invalid arguments\n", __func__, __LINE__);
		return -EINVAL;
	}

	ctx = (struct cam_ife_hw_mgr_ctx *)release_args->ctxt_to_hw_map;
	if (!ctx || !ctx->ctx_in_use) {
		pr_err("%s: Fatal: Invalid context is used!\n", __func__);
		return -EPERM;
	}

	CDBG("%s%d Enter...ctx id:%d\n", __func__, __LINE__,
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
	ctx->ctx_in_use = 0;
	CDBG("%s%d Exit...ctx id:%d\n", __func__, __LINE__,
		ctx->ctx_index);
	cam_ife_hw_mgr_put_ctx(&hw_mgr->free_ctx_list, &ctx);
	return rc;
}

static int cam_ife_mgr_prepare_hw_update(void *hw_mgr_priv,
	void *prepare_hw_update_args)
{
	int rc = 0;
	struct cam_hw_prepare_update_args *prepare =
		(struct cam_hw_prepare_update_args *) prepare_hw_update_args;
	struct cam_ife_hw_mgr_ctx        *ctx;
	struct cam_ife_hw_mgr            *hw_mgr;
	struct cam_isp_kmd_buf_info       kmd_buf;
	uint32_t                          i;
	bool                              fill_fence = true;

	if (!hw_mgr_priv || !prepare_hw_update_args) {
		pr_err("%s: Invalid args\n", __func__);
		return -EINVAL;
	}

	CDBG("%s:%d enter\n", __func__, __LINE__);

	ctx = (struct cam_ife_hw_mgr_ctx *) prepare->ctxt_to_hw_map;
	hw_mgr = (struct cam_ife_hw_mgr *)hw_mgr_priv;

	rc = cam_isp_validate_packet(prepare->packet);
	if (rc)
		return rc;

	CDBG("%s:%d enter\n", __func__, __LINE__);
	/* Pre parse the packet*/
	rc = cam_isp_get_kmd_buffer(prepare->packet, &kmd_buf);
	if (rc)
		return rc;

	rc = cam_packet_util_process_patches(prepare->packet,
		hw_mgr->mgr_common.cmd_iommu_hdl);
	if (rc) {
		pr_err("%s: Patch ISP packet failed.\n", __func__);
		return rc;
	}

	prepare->num_hw_update_entries = 0;
	prepare->num_in_map_entries = 0;
	prepare->num_out_map_entries = 0;

	for (i = 0; i < ctx->num_base; i++) {
		CDBG("%s: process cmd buffer for device %d\n", __func__, i);

		/* Add change base */
		rc = cam_isp_add_change_base(prepare, &ctx->res_list_ife_src,
			ctx->base[i].idx, &kmd_buf);
		if (rc)
			return rc;

		/* get command buffers */
		if (ctx->base[i].split_id != CAM_ISP_HW_SPLIT_MAX) {
			rc = cam_isp_add_command_buffers(prepare,
				ctx->base[i].split_id);
			if (rc)
				return rc;
		}

		/* get IO buffers */
		rc = cam_isp_add_io_buffers(hw_mgr->mgr_common.img_iommu_hdl,
				prepare, ctx->base[i].idx,
			&kmd_buf, ctx->res_list_ife_out,
			CAM_IFE_HW_OUT_RES_MAX, fill_fence);

		if (rc)
			return rc;

		/* fence map table entries need to fill only once in the loop */
		if (fill_fence)
			fill_fence = false;
	}

	/* add reg update commands */
	for (i = 0; i < ctx->num_base; i++) {
		/* Add change base */
		rc = cam_isp_add_change_base(prepare, &ctx->res_list_ife_src,
			ctx->base[i].idx, &kmd_buf);
		if (rc)
			return rc;

		/*Add reg update */
		rc = cam_isp_add_reg_update(prepare, &ctx->res_list_ife_src,
			ctx->base[i].idx, &kmd_buf);
		if (rc)
			return rc;
	}

	return rc;
}

static int cam_ife_mgr_process_recovery_cb(void *priv, void *data)
{
	int32_t rc = 0;
	struct cam_hw_event_recovery_data   *recovery_data = priv;
	struct cam_hw_start_args     start_args;
	struct cam_ife_hw_mgr   *ife_hw_mgr = NULL;
	uint32_t   hw_mgr_priv;
	uint32_t i = 0;

	uint32_t error_type = recovery_data->error_type;
	struct cam_ife_hw_mgr_ctx        *ctx = NULL;

	/* Here recovery is performed */
	CDBG("%s:Enter: ErrorType = %d\n", __func__, error_type);

	switch (error_type) {
	case CAM_ISP_HW_ERROR_OVERFLOW:
	case CAM_ISP_HW_ERROR_BUSIF_OVERFLOW:
		if (!recovery_data->affected_ctx[0]) {
			pr_err("No context is affected but recovery called\n");
			kfree(recovery_data);
			return 0;
		}

		ctx = recovery_data->affected_ctx[0];
		ife_hw_mgr = ctx->hw_mgr;

		for (i = 0; i < CAM_VFE_HW_NUM_MAX; i++) {
			if (recovery_data->affected_core[i])
				rc = cam_ife_mgr_reset_hw(ife_hw_mgr, i);
		}

		for (i = 0; i < recovery_data->no_of_context; i++) {
			start_args.ctxt_to_hw_map =
				recovery_data->affected_ctx[i];
			rc = cam_ife_mgr_restart_hw(&hw_mgr_priv, &start_args);
		}

		break;

	case CAM_ISP_HW_ERROR_P2I_ERROR:
		break;

	case CAM_ISP_HW_ERROR_VIOLATION:
		break;

	default:
		pr_err("%s: Invalid Error\n", __func__);
	}
	CDBG("%s:Exit: ErrorType = %d\n", __func__, error_type);

	kfree(recovery_data);
	return rc;
}

static int cam_ife_hw_mgr_do_error_recovery(
		struct cam_hw_event_recovery_data  *ife_mgr_recovery_data)
{
	int32_t rc = 0;
	struct crm_workq_task        *task = NULL;
	struct cam_hw_event_recovery_data  *recovery_data = NULL;

	return 0;

	recovery_data = kzalloc(sizeof(struct cam_hw_event_recovery_data),
		GFP_ATOMIC);
	if (!recovery_data)
		return -ENOMEM;

	memcpy(recovery_data, ife_mgr_recovery_data,
			sizeof(struct cam_hw_event_recovery_data));

	CDBG("%s: Enter: error_type (%d)\n", __func__,
		recovery_data->error_type);

	task = cam_req_mgr_workq_get_task(g_ife_hw_mgr.workq);
	if (!task) {
		pr_err("%s: No empty task frame\n", __func__);
		kfree(recovery_data);
		return -ENOMEM;
	}

	task->process_cb = &cam_ife_mgr_process_recovery_cb;
	rc = cam_req_mgr_workq_enqueue_task(task, recovery_data,
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
static int cam_ife_hw_mgr_match_hw_idx(
	struct cam_ife_hw_mgr_ctx   *ife_hwr_mgr_ctx,
	uint32_t *affected_core)
{

	int32_t rc = -EPERM;
	uint32_t i = 0, j = 0;
	uint32_t max_idx =  ife_hwr_mgr_ctx->num_base;
	uint32_t ctx_affected_core_idx[CAM_IFE_HW_NUM_MAX] = {0};

	CDBG("%s:Enter:max_idx = %d\n", __func__, max_idx);

	while (i < max_idx) {
		if (affected_core[ife_hwr_mgr_ctx->base[i].idx])
			rc = 0;
		else {
			ctx_affected_core_idx[j] = ife_hwr_mgr_ctx->base[i].idx;
			j = j + 1;
		}

		i = i + 1;
	}

	if (rc == 0) {
		while (j) {
			if (affected_core[ctx_affected_core_idx[j-1]] != 1)
				affected_core[ctx_affected_core_idx[j-1]] = 1;

			j = j - 1;
		}
	}
	CDBG("%s:Exit\n", __func__);
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
static int  cam_ife_hw_mgr_handle_overflow(
	struct cam_ife_hw_mgr_ctx   *curr_ife_hwr_mgr_ctx,
	struct cam_isp_hw_error_event_data *error_event_data,
	uint32_t curr_core_idx,
	struct cam_hw_event_recovery_data  *recovery_data)
{
	uint32_t affected_core[CAM_IFE_HW_NUM_MAX] = {0};
	struct cam_ife_hw_mgr_ctx   *ife_hwr_mgr_ctx = NULL;
	cam_hw_event_cb_func	         ife_hwr_irq_err_cb;
	struct cam_ife_hw_mgr		*ife_hwr_mgr = NULL;
	uint32_t                            hw_mgr_priv = 1;
	struct cam_hw_stop_args          stop_args;
	uint32_t i = 0;

	CDBG("%s:Enter\n", __func__);
	return 0;

	if (!recovery_data) {
		pr_err("%s: recovery_data parameter is NULL\n",
			__func__);
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
		CDBG("%s:Calling match Hw idx\n", __func__);
		if (cam_ife_hw_mgr_match_hw_idx(ife_hwr_mgr_ctx, affected_core))
			continue;

		ife_hwr_irq_err_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_ERROR];

		stop_args.ctxt_to_hw_map = ife_hwr_mgr_ctx;

		/* Add affected_context in list of recovery data*/
		CDBG("%s:Add new entry in affected_ctx_list\n", __func__);
		if (recovery_data->no_of_context < CAM_CTX_MAX)
			recovery_data->affected_ctx[
				recovery_data->no_of_context++] =
				ife_hwr_mgr_ctx;

		/*
		 * Stop the hw resources associated with this context
		 * and call the error callback. In the call back function
		 * corresponding ISP context will update CRM about fatal Error
		 */
		if (!cam_ife_mgr_stop_hw_in_overflow(&hw_mgr_priv,
			&stop_args)) {
			CDBG("%s:Calling Error handler CB\n", __func__);
			ife_hwr_irq_err_cb(ife_hwr_mgr_ctx->common.cb_priv,
				CAM_ISP_HW_EVENT_ERROR, error_event_data);
		}
	}
	/* fill the affected_core in recovery data */
	for (i = 0; i < CAM_IFE_HW_NUM_MAX; i++) {
		recovery_data->affected_core[i] = affected_core[i];
		CDBG("%s: Vfe core %d is affected (%d)\n",
			__func__, i, recovery_data->affected_core[i]);
	}
	CDBG("%s:Exit\n", __func__);
	return 0;
}

static int  cam_ife_hw_mgr_handle_camif_error(
	void                              *handler_priv,
	void                              *payload)
{
	int32_t  rc = 0;
	uint32_t core_idx;
	struct cam_ife_hw_mgr_ctx               *ife_hwr_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload      *evt_payload;
	struct cam_isp_hw_error_event_data       error_event_data = {0};
	struct cam_hw_event_recovery_data        recovery_data = {0};

	ife_hwr_mgr_ctx = handler_priv;
	evt_payload = payload;
	core_idx = evt_payload->core_index;

	rc = evt_payload->error_type;
	CDBG("%s: Enter: error_type (%d)\n", __func__, evt_payload->error_type);
	switch (evt_payload->error_type) {
	case CAM_ISP_HW_ERROR_OVERFLOW:
	case CAM_ISP_HW_ERROR_P2I_ERROR:
	case CAM_ISP_HW_ERROR_VIOLATION:

		error_event_data.error_type =
				CAM_ISP_HW_ERROR_OVERFLOW;

		cam_ife_hw_mgr_handle_overflow(ife_hwr_mgr_ctx,
				&error_event_data,
				core_idx,
				&recovery_data);

		/* Trigger for recovery */
		recovery_data.error_type = CAM_ISP_HW_ERROR_OVERFLOW;
		cam_ife_hw_mgr_do_error_recovery(&recovery_data);
		break;
	default:
		CDBG("%s: None error. Error type (%d)\n", __func__,
			evt_payload->error_type);
	}

	CDBG("%s: Exit (%d)\n", __func__, rc);
	return rc;
}

/*
 * DUAL VFE is valid for PIX processing path
 * This function assumes hw_res[0] is master in case
 * of dual VFE.
 * RDI path does not support DUAl VFE
 */
static int cam_ife_hw_mgr_handle_rup_for_camif_hw_res(
	void                              *handler_priv,
	void                              *payload)
{
	struct cam_isp_resource_node            *hw_res;
	struct cam_ife_hw_mgr_ctx               *ife_hwr_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload      *evt_payload;
	struct cam_ife_hw_mgr_res               *isp_ife_camif_res = NULL;
	cam_hw_event_cb_func                     ife_hwr_irq_rup_cb;
	struct cam_isp_hw_reg_update_event_data  rup_event_data;
	uint32_t  core_idx;
	uint32_t  rup_status = -EINVAL;

	CDBG("%s: Enter\n", __func__);

	ife_hwr_mgr_ctx = handler_priv;
	evt_payload = payload;

	if (!handler_priv || !payload) {
		pr_err("%s: Invalid Parameter\n", __func__);
		return -EPERM;
	}

	core_idx = evt_payload->core_index;
	ife_hwr_irq_rup_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_REG_UPDATE];

	evt_payload->evt_id = CAM_ISP_HW_EVENT_REG_UPDATE;
	list_for_each_entry(isp_ife_camif_res,
			&ife_hwr_mgr_ctx->res_list_ife_src, list) {

		if (isp_ife_camif_res->res_type == CAM_IFE_HW_MGR_RES_UNINIT)
			continue;

		CDBG("%s: camif resource id = %d, curr_core_idx = %d\n",
			__func__, isp_ife_camif_res->res_id, core_idx);
		switch (isp_ife_camif_res->res_id) {
		case CAM_ISP_HW_VFE_IN_CAMIF:
			if (isp_ife_camif_res->is_dual_vfe)
				/* It checks for slave core RUP ACK*/
				hw_res = isp_ife_camif_res->hw_res[1];
			else
				hw_res = isp_ife_camif_res->hw_res[0];

			if (!hw_res) {
				pr_err("%s: CAMIF device is NULL\n", __func__);
				break;
			}
			CDBG("%s: current_core_id = %d , core_idx res = %d\n",
					__func__, core_idx,
					hw_res->hw_intf->hw_idx);

			if (core_idx == hw_res->hw_intf->hw_idx) {
				rup_status = hw_res->bottom_half_handler(
					hw_res, evt_payload);
			}
			break;

		case CAM_ISP_HW_VFE_IN_RDI0:
		case CAM_ISP_HW_VFE_IN_RDI1:
		case CAM_ISP_HW_VFE_IN_RDI2:
			hw_res = isp_ife_camif_res->hw_res[0];

			if (!hw_res) {
				pr_err("%s: RDI Device is NULL\n", __func__);
				break;
			}
			if (core_idx == hw_res->hw_intf->hw_idx)
				/* Need to process rdi reg update */
				rup_status = -EINVAL;
			break;
		default:
			pr_err("%s: invalid resource id (%d)", __func__,
				isp_ife_camif_res->res_id);
		}

		/* only do callback for pixel reg update for now */
		if (!rup_status && (isp_ife_camif_res->res_id ==
			CAM_ISP_HW_VFE_IN_CAMIF)) {
			ife_hwr_irq_rup_cb(ife_hwr_mgr_ctx->common.cb_priv,
				CAM_ISP_HW_EVENT_REG_UPDATE, &rup_event_data);
		}

	}

	CDBG("%s: Exit (rup_status = %d)!\n", __func__, rup_status);
	return 0;
}

static int cam_ife_hw_mgr_check_epoch_for_dual_vfe(
	struct cam_ife_hw_mgr_ctx   *ife_hw_mgr_ctx,
	uint32_t                     core_idx0,
	uint32_t                     core_idx1)
{
	int32_t rc = -1;
	uint32_t *epoch_cnt = ife_hw_mgr_ctx->epoch_cnt;

	if (epoch_cnt[core_idx0] ==
			epoch_cnt[core_idx1]) {

		epoch_cnt[core_idx0] = 0;
		epoch_cnt[core_idx1] = 0;

		rc = 0;
		return rc;
	}

	if ((epoch_cnt[core_idx0] - epoch_cnt[core_idx1] > 1) ||
		(epoch_cnt[core_idx1] - epoch_cnt[core_idx0] > 1)) {

		pr_warn("%s:One of the VFE of dual VFE cound not generate error\n",
			__func__);
		rc = -1;
		return rc;
	}

	CDBG("Only one core_index has given EPOCH\n");

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

	CDBG("%s:Enter\n", __func__);

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
				pr_err("%s: Left Device is NULL\n",
					__func__);
				break;
			}

			if (core_idx == hw_res_l->hw_intf->hw_idx) {
				epoch_status = hw_res_l->bottom_half_handler(
					hw_res_l, evt_payload);
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
				pr_err("%s: Dual VFE Device is NULL\n",
					__func__);
				break;
			}
			if (core_idx == hw_res_l->hw_intf->hw_idx) {
				epoch_status = hw_res_l->bottom_half_handler(
					hw_res_l, evt_payload);

				if (!epoch_status)
					ife_hwr_mgr_ctx->epoch_cnt[core_idx]++;
			}

			/* SOF check for Right side VFE */
			if (core_idx == hw_res_r->hw_intf->hw_idx) {
				epoch_status = hw_res_r->bottom_half_handler(
					hw_res_r, evt_payload);

				if (!epoch_status)
					ife_hwr_mgr_ctx->epoch_cnt[core_idx]++;
			}

			core_index0 = hw_res_l->hw_intf->hw_idx;
			core_index1 = hw_res_r->hw_intf->hw_idx;

			rc = cam_ife_hw_mgr_check_epoch_for_dual_vfe(
					ife_hwr_mgr_ctx,
					core_index0,
					core_index1);

			if (!rc)
				ife_hwr_irq_epoch_cb(
					ife_hwr_mgr_ctx->common.cb_priv,
					CAM_ISP_HW_EVENT_EPOCH,
					&epoch_done_event_data);

			break;

		/* Error */
		default:
			pr_err("%s: error with hw_res\n", __func__);

		}
	}

	CDBG("%s: Exit (epoch_status = %d)!\n", __func__, epoch_status);
	return 0;
}

static int cam_ife_hw_mgr_check_sof_for_dual_vfe(
	struct cam_ife_hw_mgr_ctx   *ife_hwr_mgr_ctx,
	uint32_t                     core_idx0,
	uint32_t                     core_idx1)
{
	uint32_t *sof_cnt = ife_hwr_mgr_ctx->sof_cnt;
	int32_t rc = -1;

	if (sof_cnt[core_idx0] ==
			sof_cnt[core_idx1]) {

		sof_cnt[core_idx0] = 0;
		sof_cnt[core_idx1] = 0;

		rc = 0;
		return rc;
	}

	if ((sof_cnt[core_idx0] - sof_cnt[core_idx1] > 1) ||
		(sof_cnt[core_idx1] - sof_cnt[core_idx0] > 1)) {

		pr_err("%s: One VFE of dual VFE cound not generate SOF\n",
					__func__);
		rc = -1;
		return rc;
	}

	pr_info("Only one core_index has given SOF\n");

	return rc;
}

static int cam_ife_hw_mgr_handle_sof_for_camif_hw_res(
	void                              *handler_priv,
	void                              *payload)
{
	int32_t rc = -1;
	struct cam_isp_resource_node         *hw_res_l = NULL;
	struct cam_isp_resource_node         *hw_res_r = NULL;
	struct cam_ife_hw_mgr_ctx            *ife_hwr_mgr_ctx;
	struct cam_vfe_top_irq_evt_payload   *evt_payload;
	struct cam_ife_hw_mgr_res            *isp_ife_camif_res = NULL;
	cam_hw_event_cb_func                  ife_hwr_irq_sof_cb;
	struct cam_isp_hw_sof_event_data      sof_done_event_data;
	uint32_t  core_idx;
	uint32_t  sof_status = 0;
	uint32_t  core_index0;
	uint32_t  core_index1;

	CDBG("%s:Enter\n", __func__);

	ife_hwr_mgr_ctx = handler_priv;
	evt_payload = payload;
	if (!evt_payload) {
		pr_err("%s: no payload\n", __func__);
		return IRQ_HANDLED;
	}
	core_idx = evt_payload->core_index;
	ife_hwr_irq_sof_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_SOF];

	evt_payload->evt_id = CAM_ISP_HW_EVENT_SOF;

	list_for_each_entry(isp_ife_camif_res,
		&ife_hwr_mgr_ctx->res_list_ife_src, list) {

		if ((isp_ife_camif_res->res_type ==
			CAM_IFE_HW_MGR_RES_UNINIT) ||
			(isp_ife_camif_res->res_id != CAM_ISP_HW_VFE_IN_CAMIF))
			continue;

		hw_res_l = isp_ife_camif_res->hw_res[0];
		hw_res_r = isp_ife_camif_res->hw_res[1];

		CDBG("%s:is_dual_vfe ? = %d\n", __func__,
			isp_ife_camif_res->is_dual_vfe);
		switch (isp_ife_camif_res->is_dual_vfe) {
		/* Handling Single VFE Scenario */
		case 0:
			/* SOF check for Left side VFE */
			if (!hw_res_l) {
				pr_err("%s: VFE Device is NULL\n",
					__func__);
				break;
			}
			CDBG("%s: curr_core_idx = %d, core idx hw = %d\n",
					__func__, core_idx,
					hw_res_l->hw_intf->hw_idx);

			if (core_idx == hw_res_l->hw_intf->hw_idx) {
				sof_status = hw_res_l->bottom_half_handler(
					hw_res_l, evt_payload);
				if (!sof_status)
					ife_hwr_irq_sof_cb(
						ife_hwr_mgr_ctx->common.cb_priv,
						CAM_ISP_HW_EVENT_SOF,
						&sof_done_event_data);
			}

			break;

		/* Handling Dual VFE Scenario */
		case 1:
			/* SOF check for Left side VFE */

			if (!hw_res_l) {
				pr_err("%s: VFE Device is NULL\n",
					__func__);
				break;
			}
			CDBG("%s: curr_core_idx = %d, idx associated hw = %d\n",
					__func__, core_idx,
					hw_res_l->hw_intf->hw_idx);

			if (core_idx == hw_res_l->hw_intf->hw_idx) {
				sof_status = hw_res_l->bottom_half_handler(
					hw_res_l, evt_payload);
				if (!sof_status)
					ife_hwr_mgr_ctx->sof_cnt[core_idx]++;
			}

			/* SOF check for Right side VFE */
			if (!hw_res_r) {
				pr_err("%s: VFE Device is NULL\n",
					__func__);
				break;
			}
			CDBG("%s: curr_core_idx = %d, idx associated hw = %d\n",
					__func__, core_idx,
					hw_res_r->hw_intf->hw_idx);
			if (core_idx == hw_res_r->hw_intf->hw_idx) {
				sof_status = hw_res_r->bottom_half_handler(
					hw_res_r, evt_payload);
				if (!sof_status)
					ife_hwr_mgr_ctx->sof_cnt[core_idx]++;
			}

			core_index0 = hw_res_l->hw_intf->hw_idx;
			core_index1 = hw_res_r->hw_intf->hw_idx;

			rc = cam_ife_hw_mgr_check_sof_for_dual_vfe(
				ife_hwr_mgr_ctx, core_index0, core_index1);

			if (!rc)
				ife_hwr_irq_sof_cb(
					ife_hwr_mgr_ctx->common.cb_priv,
					CAM_ISP_HW_EVENT_SOF,
					&sof_done_event_data);

			break;

		default:
			pr_err("%s: error with hw_res\n", __func__);
		}
	}

	CDBG("%s: Exit (sof_status = %d)!\n", __func__, sof_status);
	return 0;
}

static int cam_ife_hw_mgr_handle_buf_done_for_hw_res(
	void                              *handler_priv,
	void                              *payload)

{
	int32_t                              buf_done_status = 0;
	int32_t                              i = 0;
	int32_t                              rc = 0;
	cam_hw_event_cb_func                 ife_hwr_irq_wm_done_cb;
	struct cam_isp_resource_node        *hw_res_l = NULL;
	struct cam_ife_hw_mgr_ctx           *ife_hwr_mgr_ctx = handler_priv;
	struct cam_vfe_bus_irq_evt_payload  *evt_payload = payload;
	struct cam_ife_hw_mgr_res           *isp_ife_out_res = NULL;
	struct cam_hw_event_recovery_data    recovery_data;
	struct cam_isp_hw_done_event_data    buf_done_event_data = {0};
	struct cam_isp_hw_error_event_data   error_event_data = {0};
	uint32_t  error_resc_handle[CAM_IFE_HW_OUT_RES_MAX];
	uint32_t  num_of_error_handles = 0;

	CDBG("%s:Enter\n", __func__);

	ife_hwr_irq_wm_done_cb =
		ife_hwr_mgr_ctx->common.event_cb[CAM_ISP_HW_EVENT_DONE];

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

			/* Report for Successful buf_done event if any */
			if (buf_done_event_data.num_handles > 0 &&
				ife_hwr_irq_wm_done_cb) {
				CDBG("%s: notify isp context\n", __func__);
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
		CDBG("%s:buf_done status:(%d),isp_ife_out_res->res_id : 0x%x\n",
			__func__, buf_done_status, isp_ife_out_res->res_id);
	}


	CDBG("%s: Exit (buf_done_status (Success) = %d)!\n", __func__,
			buf_done_status);
	return rc;

err:
	/*
	 * Report for error if any.
	 * For the first phase, Error is reported as overflow, for all
	 * the affected context and any successful buf_done event is not
	 * reported.
	 */
	rc = cam_ife_hw_mgr_handle_overflow(ife_hwr_mgr_ctx,
		&error_event_data, evt_payload->core_index,
		&recovery_data);

	/*
	 * We can temporarily return from here as
	 * for the first phase, we are going to reset entire HW.
	 */

	CDBG("%s: Exit (buf_done_status (Error) = %d)!\n", __func__,
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
	ife_hwr_mgr_ctx = (struct cam_ife_hw_mgr_ctx *)handler_priv;

	CDBG("addr of evt_payload = %llx\n", (uint64_t)evt_payload);
	CDBG("bus_irq_status_0: = %x\n", evt_payload->irq_reg_val[0]);
	CDBG("bus_irq_status_1: = %x\n", evt_payload->irq_reg_val[1]);
	CDBG("bus_irq_status_2: = %x\n", evt_payload->irq_reg_val[2]);
	CDBG("bus_irq_comp_err: = %x\n", evt_payload->irq_reg_val[3]);
	CDBG("bus_irq_comp_owrt: = %x\n", evt_payload->irq_reg_val[4]);
	CDBG("bus_irq_dual_comp_err: = %x\n", evt_payload->irq_reg_val[5]);
	CDBG("bus_irq_dual_comp_owrt: = %x\n", evt_payload->irq_reg_val[6]);

	/*
	 * If overflow/overwrite/error/violation are pending
	 * for this context it needs to be handled remaining
	 * interrupts are ignored.
	 */
	rc = cam_ife_hw_mgr_handle_camif_error(ife_hwr_mgr_ctx,
		evt_payload_priv);
	if (rc) {
		pr_err("%s: Encountered Error (%d), ignoring other irqs\n",
			__func__, rc);
		return IRQ_HANDLED;
	}

	CDBG("%s: Calling Buf_done\n", __func__);
	/* WM Done */
	return cam_ife_hw_mgr_handle_buf_done_for_hw_res(ife_hwr_mgr_ctx,
		evt_payload_priv);
}

int cam_ife_mgr_do_tasklet(void *handler_priv, void *evt_payload_priv)
{
	struct cam_ife_hw_mgr_ctx            *ife_hwr_mgr_ctx = handler_priv;
	struct cam_vfe_top_irq_evt_payload   *evt_payload;
	int rc = -EINVAL;

	if (!handler_priv)
		return rc;

	evt_payload = evt_payload_priv;
	ife_hwr_mgr_ctx = (struct cam_ife_hw_mgr_ctx *)handler_priv;

	CDBG("addr of evt_payload = %llx\n", (uint64_t)evt_payload);
	CDBG("irq_status_0: = %x\n", evt_payload->irq_reg_val[0]);
	CDBG("irq_status_1: = %x\n", evt_payload->irq_reg_val[1]);
	CDBG("Violation register: = %x\n", evt_payload->irq_reg_val[2]);

	/*
	 * If overflow/overwrite/error/violation are pending
	 * for this context it needs to be handled remaining
	 * interrupts are ignored.
	 */
	rc = cam_ife_hw_mgr_handle_camif_error(ife_hwr_mgr_ctx,
		evt_payload_priv);
	if (rc) {
		pr_err("%s: Encountered Error (%d), ignoring other irqs\n",
			__func__, rc);
		return IRQ_HANDLED;
	}

	CDBG("%s: Calling SOF\n", __func__);
	/* SOF IRQ */
	cam_ife_hw_mgr_handle_sof_for_camif_hw_res(ife_hwr_mgr_ctx,
		evt_payload_priv);

	CDBG("%s: Calling RUP\n", __func__);
	/* REG UPDATE */
	cam_ife_hw_mgr_handle_rup_for_camif_hw_res(ife_hwr_mgr_ctx,
		evt_payload_priv);

	CDBG("%s: Calling EPOCH\n", __func__);
	/* EPOCH IRQ */
	cam_ife_hw_mgr_handle_epoch_for_camif_hw_res(ife_hwr_mgr_ctx,
		evt_payload_priv);

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

int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf)
{
	int rc = -EFAULT;
	int i, j;
	struct cam_iommu_handle cdm_handles;

	CDBG("%s: Enter\n", __func__);

	memset(&g_ife_hw_mgr, 0, sizeof(g_ife_hw_mgr));

	mutex_init(&g_ife_hw_mgr.ctx_mutex);

	if (CAM_IFE_HW_NUM_MAX != CAM_IFE_CSID_HW_NUM_MAX) {
		pr_err("%s: Fatal, CSID num is different then IFE num!\n",
			__func__);
		goto end;
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
			CDBG("reg_map: mem base = 0x%llx, cam_base = 0x%llx\n",
				(uint64_t) soc_info->reg_map[0].mem_base,
				(uint64_t) soc_info->reg_map[0].mem_cam_base);
		} else {
			g_ife_hw_mgr.cdm_reg_map[i] = NULL;
		}
	}
	if (j == 0) {
		pr_err("%s: no valid IFE HW!\n", __func__);
		goto end;
	}

	/* fill csid hw intf information */
	for (i = 0, j = 0; i < CAM_IFE_CSID_HW_NUM_MAX; i++) {
		rc = cam_ife_csid_hw_init(&g_ife_hw_mgr.csid_devices[i], i);
		if (!rc)
			j++;
	}
	if (!j) {
		pr_err("%s: no valid IFE CSID HW!\n", __func__);
		goto end;
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
		pr_err("%s: Can not get iommu handle.\n", __func__);
		goto end;
	}

	if (cam_smmu_ops(g_ife_hw_mgr.mgr_common.img_iommu_hdl,
		CAM_SMMU_ATTACH)) {
		pr_err("%s: Attach iommu handle failed.\n", __func__);
		goto end;
	}

	CDBG("got iommu_handle=%d\n", g_ife_hw_mgr.mgr_common.img_iommu_hdl);
	g_ife_hw_mgr.mgr_common.img_iommu_hdl_secure = -1;

	if (!cam_cdm_get_iommu_handle("ife", &cdm_handles)) {
		CDBG("Successfully acquired the CDM iommu handles\n");
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl = cdm_handles.non_secure;
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl_secure =
			cdm_handles.secure;
	} else {
		CDBG("Failed to acquire the CDM iommu handles\n");
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl = -1;
		g_ife_hw_mgr.mgr_common.cmd_iommu_hdl_secure = -1;
	}

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
			pr_err("Allocation Failed for cdm command\n");
			goto end;
		}

		g_ife_hw_mgr.ctx_pool[i].ctx_index = i;
		g_ife_hw_mgr.ctx_pool[i].hw_mgr = &g_ife_hw_mgr;

		cam_tasklet_init(&g_ife_hw_mgr.mgr_common.tasklet_pool[i],
			&g_ife_hw_mgr.ctx_pool[i], i);
		g_ife_hw_mgr.ctx_pool[i].common.tasklet_info =
			g_ife_hw_mgr.mgr_common.tasklet_pool[i];

		list_add_tail(&g_ife_hw_mgr.ctx_pool[i].list,
			&g_ife_hw_mgr.free_ctx_list);
	}

	/* Create Worker for ife_hw_mgr with 10 tasks */
	rc = cam_req_mgr_workq_create("cam_ife_worker", 10,
			&g_ife_hw_mgr.workq, CRM_WORKQ_USAGE_NON_IRQ);

	if (rc < 0) {
		pr_err("%s: Unable to create worker\n", __func__);
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

	CDBG("%s: Exit\n", __func__);
	return 0;
end:
	if (rc) {
		for (i = 0; i < CAM_CTX_MAX; i++)
			kfree(g_ife_hw_mgr.ctx_pool[i].cdm_cmd);
	}
	return rc;
}

