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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/slab.h>
#include "cam_io_util.h"
#include "cam_cdm_util.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver2.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

struct cam_vfe_top_ver2_common_data {
	struct cam_hw_soc_info                     *soc_info;
	struct cam_hw_intf                         *hw_intf;
	struct cam_vfe_top_ver2_reg_offset_common  *common_reg;
};

struct cam_vfe_top_ver2_priv {
	struct cam_vfe_top_ver2_common_data common_data;
	struct cam_vfe_camif               *camif;
	struct cam_isp_resource_node        mux_rsrc[CAM_VFE_TOP_VER2_MUX_MAX];
};

static int cam_vfe_top_mux_get_base(struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          mem_base = 0;
	struct cam_isp_hw_get_cdm_args   *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cdm_args)) {
		pr_err("Error! Invalid cmd size\n");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !top_priv ||
		!top_priv->common_data.soc_info) {
		pr_err("Error! Invalid args\n");
		return -EINVAL;
	}

	cdm_util_ops =
		(struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		pr_err("Invalid CDM ops\n");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_changebase();
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->size) {
		pr_err("buf size:%d is not sufficient, expected: %d\n",
			cdm_args->size, size);
		return -EINVAL;
	}

	mem_base = CAM_SOC_GET_REG_MAP_CAM_BASE(
		top_priv->common_data.soc_info, VFE_CORE_BASE_IDX);
	CDBG("core %d mem_base 0x%x\n", top_priv->common_data.soc_info->index,
		mem_base);

	cdm_util_ops->cdm_write_changebase(cdm_args->cmd_buf_addr, mem_base);
	cdm_args->used_bytes = (size * 4);

	return 0;
}

static int cam_vfe_top_mux_get_reg_update(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          reg_val_pair[2];
	struct cam_isp_hw_get_cdm_args   *cdm_args = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cdm_args)) {
		pr_err("Error! Invalid cmd size\n");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res) {
		pr_err("Error! Invalid args\n");
		return -EINVAL;
	}

	cdm_util_ops = (struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		pr_err("Error! Invalid CDM ops\n");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_reg_random(1);
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->size) {
		pr_err("Error! buf size:%d is not sufficient, expected: %d\n",
			cdm_args->size, size);
		return -EINVAL;
	}

	reg_val_pair[0] = top_priv->common_data.common_reg->reg_update_cmd;

	if (cdm_args->res->res_id == CAM_ISP_HW_VFE_IN_CAMIF)
		reg_val_pair[1] = BIT(0);
	else {
		uint32_t rdi_num = cdm_args->res->res_id -
			CAM_ISP_HW_VFE_IN_RDI0;
		/* RDI reg_update starts at BIT 1, so add 1 */
		reg_val_pair[1] = BIT(rdi_num + 1);
	}

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd_buf_addr,
		1, reg_val_pair);

	cdm_args->used_bytes = size * 4;

	return 0;
}

int cam_vfe_top_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv   *top_priv = device_priv;
	struct cam_hw_soc_info         *soc_info = NULL;
	struct cam_vfe_top_ver2_reg_offset_common *reg_common = NULL;

	if (!top_priv) {
		pr_err("Invalid arguments\n");
		return -EINVAL;
	}

	soc_info = top_priv->common_data.soc_info;
	reg_common = top_priv->common_data.common_reg;

	/* Mask All the IRQs except RESET */
	cam_io_w_mb((1 << 31),
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX) + 0x5C);

	/* Reset HW */
	cam_io_w_mb(0x00003F9F,
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX) +
		reg_common->global_reset_cmd);

	CDBG("Reset HW exit\n");
	return 0;
}

int cam_vfe_top_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_vfe_acquire_args             *args;
	struct cam_vfe_hw_vfe_in_acquire_args   *acquire_args;
	uint32_t i;
	int rc = -EINVAL;

	if (!device_priv || !reserve_args) {
		pr_err("Error! Invalid input arguments\n");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	args = (struct cam_vfe_acquire_args *)reserve_args;
	acquire_args = &args->vfe_in;


	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		if (top_priv->mux_rsrc[i].res_id ==  acquire_args->res_id &&
			top_priv->mux_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {

			if (acquire_args->res_id == CAM_ISP_HW_VFE_IN_CAMIF) {
				rc = cam_vfe_camif_ver2_acquire_resource(
					&top_priv->mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			top_priv->mux_rsrc[i].cdm_ops = acquire_args->cdm_ops;
			top_priv->mux_rsrc[i].tasklet_info = args->tasklet;
			top_priv->mux_rsrc[i].res_state =
				CAM_ISP_RESOURCE_STATE_RESERVED;
			acquire_args->rsrc_node =
				&top_priv->mux_rsrc[i];

			rc = 0;
			break;
		}
	}

	return rc;

}

int cam_vfe_top_release(void *device_priv,
	void *release_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;

	if (!device_priv || !release_args) {
		pr_err("Error! Invalid input arguments\n");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)release_args;

	CDBG("%s: Resource in state %d\n", __func__, mux_res->res_state);
	if (mux_res->res_state < CAM_ISP_RESOURCE_STATE_RESERVED) {
		pr_err("Error! Resource in Invalid res_state :%d\n",
			mux_res->res_state);
		return -EINVAL;
	}
	mux_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

int cam_vfe_top_start(void *device_priv,
	void *start_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;
	int rc = 0;

	if (!device_priv || !start_args) {
		pr_err("Error! Invalid input arguments\n");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)start_args;

	if (mux_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF) {
		rc = mux_res->start(mux_res);
	} else if (mux_res->res_id >= CAM_ISP_HW_VFE_IN_RDI0 &&
		mux_res->res_id <= CAM_ISP_HW_VFE_IN_RDI3) {
		mux_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
		rc = 0;
	} else {
		pr_err("Invalid res id:%d\n", mux_res->res_id);
		rc = -EINVAL;
	}

	return rc;
}

int cam_vfe_top_stop(void *device_priv,
	void *stop_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;
	int rc = 0;

	if (!device_priv || !stop_args) {
		pr_err("Error! Invalid input arguments\n");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)stop_args;

	if (mux_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF ||
		(mux_res->res_id >= CAM_ISP_HW_VFE_IN_RDI0 &&
		mux_res->res_id <= CAM_ISP_HW_VFE_IN_RDI3)) {
		rc = mux_res->stop(mux_res);
	} else {
		pr_err("Invalid res id:%d\n", mux_res->res_id);
		rc = -EINVAL;
	}

	return rc;

}

int cam_vfe_top_read(void *device_priv,
	void *read_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_write(void *device_priv,
	void *write_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_top_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct cam_vfe_top_ver2_priv            *top_priv;

	if (!device_priv || !cmd_args) {
		pr_err("Error! Invalid arguments\n");
		return -EINVAL;
	}
	top_priv = (struct cam_vfe_top_ver2_priv *)device_priv;

	switch (cmd_type) {
	case CAM_VFE_HW_CMD_GET_CHANGE_BASE:
		rc = cam_vfe_top_mux_get_base(top_priv, cmd_args, arg_size);
		break;
	case CAM_VFE_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_top_mux_get_reg_update(top_priv, cmd_args,
			arg_size);
		break;
	default:
		rc = -EINVAL;
		pr_err("Error! Invalid cmd:%d\n", cmd_type);
		break;
	}

	return rc;
}

int cam_vfe_top_ver2_init(
	struct cam_hw_soc_info                 *soc_info,
	struct cam_hw_intf                     *hw_intf,
	void                                   *top_hw_info,
	struct cam_vfe_top                    **vfe_top_ptr)
{
	int i, j, rc = 0;
	struct cam_vfe_top_ver2_priv           *top_priv = NULL;
	struct cam_vfe_top_ver2_hw_info        *ver2_hw_info = top_hw_info;
	struct cam_vfe_top                     *vfe_top;

	vfe_top = kzalloc(sizeof(struct cam_vfe_top), GFP_KERNEL);
	if (!vfe_top) {
		CDBG("Error! Failed to alloc for vfe_top\n");
		rc = -ENOMEM;
		goto end;
	}

	top_priv = kzalloc(sizeof(struct cam_vfe_top_ver2_priv),
		GFP_KERNEL);
	if (!top_priv) {
		CDBG("Error! Failed to alloc for vfe_top_priv\n");
		rc = -ENOMEM;
		goto free_vfe_top;
	}
	vfe_top->top_priv = top_priv;

	for (i = 0, j = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		top_priv->mux_rsrc[i].res_type = CAM_ISP_RESOURCE_VFE_IN;
		top_priv->mux_rsrc[i].hw_intf = hw_intf;
		top_priv->mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		if (ver2_hw_info->mux_type[i] == CAM_VFE_CAMIF_VER_2_0) {
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_CAMIF;

			rc = cam_vfe_camif_ver2_init(hw_intf, soc_info,
				&ver2_hw_info->camif_hw_info,
				&top_priv->mux_rsrc[i]);
			if (rc)
				goto deinit_resources;
		} else {
			/* set the RDI resource id */
			top_priv->mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_RDI0 + j++;

			rc = cam_vfe_rdi_ver2_init(hw_intf, soc_info,
				&ver2_hw_info->rdi_hw_info,
				&top_priv->mux_rsrc[i]);
			if (rc)
				goto deinit_resources;
		}
	}

	vfe_top->hw_ops.get_hw_caps = cam_vfe_top_get_hw_caps;
	vfe_top->hw_ops.init        = cam_vfe_top_init_hw;
	vfe_top->hw_ops.reset       = cam_vfe_top_reset;
	vfe_top->hw_ops.reserve     = cam_vfe_top_reserve;
	vfe_top->hw_ops.release     = cam_vfe_top_release;
	vfe_top->hw_ops.start       = cam_vfe_top_start;
	vfe_top->hw_ops.stop        = cam_vfe_top_stop;
	vfe_top->hw_ops.read        = cam_vfe_top_read;
	vfe_top->hw_ops.write       = cam_vfe_top_write;
	vfe_top->hw_ops.process_cmd = cam_vfe_top_process_cmd;
	*vfe_top_ptr = vfe_top;

	top_priv->common_data.soc_info     = soc_info;
	top_priv->common_data.hw_intf      = hw_intf;
	top_priv->common_data.common_reg   = ver2_hw_info->common_reg;

	return rc;

deinit_resources:
	for (--i; i >= 0; i--) {
		if (ver2_hw_info->mux_type[i] == CAM_VFE_CAMIF_VER_2_0) {
			if (cam_vfe_camif_ver2_deinit(&top_priv->mux_rsrc[i]))
				pr_err("Camif Deinit failed\n");
		} else {
			if (cam_vfe_rdi_ver2_deinit(&top_priv->mux_rsrc[i]))
				pr_err("RDI Deinit failed\n");
		}
		top_priv->mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	}

	kfree(vfe_top->top_priv);
free_vfe_top:
	kfree(vfe_top);
end:
	return rc;
}

int cam_vfe_top_ver2_deinit(struct cam_vfe_top  **vfe_top_ptr)
{
	int i, rc = 0;
	struct cam_vfe_top_ver2_priv           *top_priv = NULL;
	struct cam_vfe_top                     *vfe_top;

	if (!vfe_top_ptr) {
		pr_err("Error! Invalid input\n");
		return -EINVAL;
	}

	vfe_top = *vfe_top_ptr;
	if (!vfe_top) {
		pr_err("Error! vfe_top NULL\n");
		return -ENODEV;
	}

	top_priv = vfe_top->top_priv;
	if (!top_priv) {
		pr_err("Error! vfe_top_priv NULL\n");
		rc = -ENODEV;
		goto free_vfe_top;
	}

	for (i = 0; i < CAM_VFE_TOP_VER2_MUX_MAX; i++) {
		top_priv->mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
		if (top_priv->mux_rsrc[i].res_id ==
			CAM_ISP_HW_VFE_IN_CAMIF) {
			rc = cam_vfe_camif_ver2_deinit(&top_priv->mux_rsrc[i]);
			if (rc)
				pr_err("Error! Camif deinit failed rc=%d\n",
					rc);
		} else {
			rc = cam_vfe_rdi_ver2_deinit(&top_priv->mux_rsrc[i]);
			if (rc)
				pr_err("Error! RDI deinit failed rc=%d\n", rc);
		}
	}

	kfree(vfe_top->top_priv);

free_vfe_top:
	kfree(vfe_top);
	*vfe_top_ptr = NULL;

	return rc;
}

