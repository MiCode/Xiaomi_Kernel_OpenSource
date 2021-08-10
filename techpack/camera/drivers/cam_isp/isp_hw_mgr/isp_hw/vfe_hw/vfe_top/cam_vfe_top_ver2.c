// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "cam_io_util.h"
#include "cam_cdm_util.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_top.h"
#include "cam_vfe_top_ver2.h"
#include "cam_debug_util.h"
#include "cam_vfe_soc.h"

#define CAM_VFE_HW_RESET_HW_AND_REG_VAL       0x00003F9F
#define CAM_VFE_HW_RESET_HW_VAL               0x00003F87

struct cam_vfe_top_ver2_common_data {
	struct cam_hw_soc_info                     *soc_info;
	struct cam_hw_intf                         *hw_intf;
	struct cam_vfe_top_ver2_reg_offset_common  *common_reg;
	struct cam_vfe_top_dump_data               *dump_data;
};

struct cam_vfe_top_ver2_priv {
	struct cam_vfe_top_ver2_common_data common_data;
	unsigned long                       hw_clk_rate;
	unsigned long                       req_clk_rate[
						CAM_VFE_TOP_MUX_MAX];
	struct cam_vfe_top_priv_common      top_common;
};

static int cam_vfe_top_mux_get_base(struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          mem_base = 0;
	struct cam_isp_hw_get_cmd_update *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Error! Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !top_priv ||
		!top_priv->common_data.soc_info) {
		CAM_ERR(CAM_ISP, "Error! Invalid args");
		return -EINVAL;
	}

	cdm_util_ops =
		(struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_changebase();
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	mem_base = CAM_SOC_GET_REG_MAP_CAM_BASE(
		top_priv->common_data.soc_info, VFE_CORE_BASE_IDX);
	CAM_DBG(CAM_ISP, "core %d mem_base 0x%x",
		top_priv->common_data.soc_info->index, mem_base);

	cdm_util_ops->cdm_write_changebase(
	cdm_args->cmd.cmd_buf_addr, mem_base);
	cdm_args->cmd.used_bytes = (size * 4);

	return 0;
}

static int cam_vfe_top_set_hw_clk_rate(
	struct cam_vfe_top_ver2_priv *top_priv)
{
	struct cam_hw_soc_info        *soc_info = NULL;
	int                            i, rc = 0;
	unsigned long                  max_clk_rate = 0;
	struct cam_vfe_soc_private    *soc_private = NULL;

	soc_info = top_priv->common_data.soc_info;
	soc_private =
		(struct cam_vfe_soc_private *)soc_info->soc_private;

	for (i = 0; i < top_priv->top_common.num_mux; i++) {
		if (top_priv->req_clk_rate[i] > max_clk_rate)
			max_clk_rate = top_priv->req_clk_rate[i];
	}
	if (max_clk_rate == top_priv->hw_clk_rate)
		return 0;

	CAM_DBG(CAM_PERF, "VFE: Clock name=%s idx=%d clk=%llu",
		soc_info->clk_name[soc_info->src_clk_idx],
		soc_info->src_clk_idx, max_clk_rate);
	soc_private->ife_clk_src = max_clk_rate;
	rc = cam_soc_util_set_src_clk_rate(soc_info, max_clk_rate);

	if (!rc)
		top_priv->hw_clk_rate = max_clk_rate;
	else
		CAM_ERR(CAM_PERF, "Set Clock rate failed, rc=%d", rc);

	return rc;
}

static int cam_vfe_top_fs_update(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_fe_update_args *cmd_update = cmd_args;

	if (cmd_update->node_res->process_cmd)
		return cmd_update->node_res->process_cmd(cmd_update->node_res,
			CAM_ISP_HW_CMD_FE_UPDATE_IN_RD, cmd_args, arg_size);

	return 0;
}

static int cam_vfe_top_clock_update(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_clock_update_args     *clk_update = NULL;
	struct cam_isp_resource_node         *res = NULL;
	struct cam_hw_info                   *hw_info = NULL;
	int                                   i, rc = 0;

	clk_update =
		(struct cam_vfe_clock_update_args *)cmd_args;
	res = clk_update->node_res;

	if (!res || !res->hw_intf->hw_priv) {
		CAM_ERR(CAM_PERF, "Invalid input res %pK", res);
		return -EINVAL;
	}

	hw_info = res->hw_intf->hw_priv;

	if (res->res_type != CAM_ISP_RESOURCE_VFE_IN ||
		res->res_id >= CAM_ISP_HW_VFE_IN_MAX) {
		CAM_ERR(CAM_PERF, "VFE:%d Invalid res_type:%d res id%d",
			res->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	for (i = 0; i < top_priv->top_common.num_mux; i++) {
		if (top_priv->top_common.mux_rsrc[i].res_id == res->res_id) {
			top_priv->req_clk_rate[i] = clk_update->clk_rate;
			break;
		}
	}

	if (hw_info->hw_state != CAM_HW_STATE_POWER_UP) {
		CAM_DBG(CAM_PERF,
			"VFE:%d Not ready to set clocks yet :%d",
			res->hw_intf->hw_idx,
			hw_info->hw_state);
	} else
		rc = cam_vfe_top_set_hw_clk_rate(top_priv);

	return rc;
}

static int cam_vfe_top_blanking_update(uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_isp_blanking_config       *blanking_config = NULL;
	struct cam_isp_resource_node         *node_res = NULL;

	blanking_config =
		(struct cam_isp_blanking_config *)cmd_args;
	node_res = blanking_config->node_res;

	if (!node_res) {
		CAM_ERR(CAM_PERF, "Invalid input res %pK", node_res);
		return -EINVAL;
	}

	if (!node_res->process_cmd) {
		CAM_ERR(CAM_PERF, "Invalid input res process_cmd %pK",
			node_res->process_cmd);
		return -EINVAL;
	}

	return node_res->process_cmd(node_res,
		cmd_type, cmd_args, arg_size);
}

static int cam_vfe_top_mux_get_reg_update(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_isp_hw_get_cmd_update  *cmd_update = cmd_args;

	if (cmd_update->res->process_cmd)
		return cmd_update->res->process_cmd(cmd_update->res,
			CAM_ISP_HW_CMD_GET_REG_UPDATE, cmd_args, arg_size);

	return -EINVAL;
}

static int cam_vfe_top_wait_comp_event(struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	struct cam_isp_hw_get_cmd_update *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Error! Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !top_priv ||
		!top_priv->common_data.soc_info) {
		CAM_ERR(CAM_ISP, "Error! Invalid args");
		return -EINVAL;
	}

	cdm_util_ops =
		(struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_comp_wait();
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	cdm_util_ops->cdm_write_wait_comp_event(cdm_args->cmd.cmd_buf_addr,
		0, 0x2);
	cdm_args->cmd.used_bytes = (size * 4);

	return 0;
}

static int cam_vfe_top_add_wait_trigger(struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	uint32_t                          size = 0;
	uint32_t                          reg_val_pair[2];
	struct cam_isp_hw_get_cmd_update *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;

	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Error! Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !top_priv ||
		!top_priv->common_data.soc_info) {
		CAM_ERR(CAM_ISP, "Error! Invalid args");
		return -EINVAL;
	}

	cdm_util_ops =
		(struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_reg_random(1);
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	reg_val_pair[0] = 0x90;
	reg_val_pair[1] = 0x1;

	cdm_util_ops->cdm_write_regrandom(cdm_args->cmd.cmd_buf_addr,
		1, reg_val_pair);
	cdm_args->cmd.used_bytes = (size * 4);

	return 0;
}

static int cam_vfe_top_get_data(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_isp_resource_node  *res = cmd_args;

	if (res->process_cmd)
		return res->process_cmd(res,
			CAM_ISP_HW_CMD_CAMIF_DATA, cmd_args, arg_size);

	return -EINVAL;
}

int cam_vfe_top_get_hw_caps(void *device_priv,
	void *args, uint32_t arg_size)
{
	struct cam_vfe_hw_get_hw_cap *vfe_cap_info = NULL;
	struct cam_vfe_top_ver2_priv *vfe_top_prv = NULL;
	struct cam_vfe_soc_private *vfe_soc_private = NULL;

	if (!device_priv || !args) {
		CAM_ERR(CAM_ISP,
			"Invalid arguments device_priv:%p, args:%p",
			device_priv, args);
		return -EINVAL;
	}

	vfe_cap_info = (struct cam_vfe_hw_get_hw_cap *)args;
	vfe_top_prv = (struct cam_vfe_top_ver2_priv *)device_priv;

	if (!vfe_top_prv->common_data.soc_info) {
		CAM_ERR(CAM_ISP, "soc_info is null");
		return -EFAULT;
	}

	vfe_soc_private = (struct cam_vfe_soc_private *)
		vfe_top_prv->common_data.soc_info->soc_private;

	vfe_cap_info->is_lite = (vfe_soc_private->is_ife_lite) ? true : false;
	vfe_cap_info->incr =
		(vfe_top_prv->top_common.hw_version) & 0x00ffff;
	vfe_cap_info->minor =
		((vfe_top_prv->top_common.hw_version) >> 16) & 0x0fff;
	vfe_cap_info->major =
		((vfe_top_prv->top_common.hw_version) >> 28) & 0x000f;

	return 0;
}

static int cam_vfe_hw_dump(
	struct cam_vfe_top_ver2_priv *top_priv,
	void *cmd_args,
	uint32_t arg_size)
{
	int                                i, j;
	uint8_t                           *dst;
	uint32_t                           reg_start_offset;
	uint32_t                           reg_dump_size = 0;
	uint32_t                           lut_dump_size = 0;
	uint32_t                           val;
	uint32_t                           num_reg;
	void __iomem                      *reg_base;
	uint32_t                          *addr, *start;
	size_t                             remain_len;
	uint32_t                           min_len;
	struct cam_hw_soc_info            *soc_info;
	struct cam_vfe_top_dump_data      *dump_data;
	struct cam_isp_hw_dump_header     *hdr;
	struct cam_isp_hw_dump_args       *dump_args =
		(struct cam_isp_hw_dump_args *)cmd_args;

	if (!dump_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}
	if (!dump_args->cpu_addr || !dump_args->buf_len) {
		CAM_ERR(CAM_ISP,
			"Invalid params %pK %zu",
			(void *)dump_args->cpu_addr,
			dump_args->buf_len);
		return -EINVAL;
	}
	if (dump_args->buf_len <= dump_args->offset) {
		CAM_WARN(CAM_ISP,
			"Dump offset overshoot offset %zu buf_len %zu",
			dump_args->offset, dump_args->buf_len);
		return -ENOSPC;
	}
	dump_data = top_priv->common_data.dump_data;
	soc_info = top_priv->common_data.soc_info;

	/*Dump registers */
	for (i = 0; i < dump_data->num_reg_dump_entries; i++)
		reg_dump_size += (dump_data->reg_entry[i].reg_dump_end -
			dump_data->reg_entry[i].reg_dump_start);
	/*
	 * We dump the offset as well, so the total size dumped becomes
	 * multiplied by 2
	 */
	reg_dump_size *= 2;
	for (i = 0; i < dump_data->num_lut_dump_entries; i++)
		lut_dump_size += ((dump_data->lut_entry[i].lut_addr_size) *
			(dump_data->lut_entry[i].lut_word_size/8));

	/*Minimum len comprises of:
	 * soc_index
	 * lut_dump_size + reg_dump_size + sizeof dump_header +
	 * (num_lut_dump_entries--> represents number of banks)
	 */
	min_len = sizeof(uint32_t) + lut_dump_size + reg_dump_size +
		sizeof(struct cam_isp_hw_dump_header) +
		(dump_data->num_lut_dump_entries * sizeof(uint32_t));
	remain_len = dump_args->buf_len - dump_args->offset;
	if (remain_len < min_len) {
		CAM_WARN(CAM_ISP, "Dump buffer exhaust remain %zu, min %u",
			remain_len, min_len);
		return -ENOSPC;
	}

	dst = (uint8_t *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct cam_isp_hw_dump_header *)dst;
	hdr->word_size = sizeof(uint32_t);
	scnprintf(hdr->tag, CAM_ISP_HW_DUMP_TAG_MAX_LEN, "VFE_REG:");
	addr = (uint32_t *)(dst + sizeof(struct cam_isp_hw_dump_header));
	start = addr;
	*addr++ = soc_info->index;
	for (i = 0; i < dump_data->num_reg_dump_entries; i++) {
		num_reg  = (dump_data->reg_entry[i].reg_dump_end -
			dump_data->reg_entry[i].reg_dump_start)/4;
		reg_start_offset = dump_data->reg_entry[i].reg_dump_start;
		reg_base = soc_info->reg_map[0].mem_base + reg_start_offset;
		for (j = 0; j < num_reg; j++) {
			addr[0] = soc_info->mem_block[0]->start +
				reg_start_offset + (j*4);
			addr[1] = cam_io_r(reg_base + (j*4));
			addr += 2;
		}
	}
	hdr->size = hdr->word_size * (addr - start);
	dump_args->offset +=  hdr->size +
		sizeof(struct cam_isp_hw_dump_header);

	/*dump LUT*/
	for (i = 0; i < dump_data->num_lut_dump_entries; i++) {

		dst = (char *)dump_args->cpu_addr + dump_args->offset;
		hdr = (struct cam_isp_hw_dump_header *)dst;
		scnprintf(hdr->tag, CAM_ISP_HW_DUMP_TAG_MAX_LEN, "LUT_REG:");
		hdr->word_size = dump_data->lut_entry[i].lut_word_size/8;
		addr = (uint32_t *)(dst +
			sizeof(struct cam_isp_hw_dump_header));
		start = addr;
		*addr++ = dump_data->lut_entry[i].lut_bank_sel;
		val = 0x100 |  dump_data->lut_entry[i].lut_bank_sel;
		cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			dump_data->dmi_cfg);
		cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			dump_data->dmi_addr);
		for (j = 0; j < dump_data->lut_entry[i].lut_addr_size;
			j++) {
			if (dump_data->lut_entry[i].lut_word_size == 64) {
				addr[0] = cam_io_r(
					soc_info->reg_map[0].mem_base +
					dump_data->dmi_data_path_lo);
				addr[1] = cam_io_r(
					soc_info->reg_map[0].mem_base +
					dump_data->dmi_data_path_hi);
				addr += 2;
			} else {
				*addr = cam_io_r(
					soc_info->reg_map[0].mem_base +
					dump_data->dmi_data_path_lo);
				addr++;
			}
		}
		hdr->size = hdr->word_size * (addr - start);
		dump_args->offset +=  hdr->size +
			sizeof(struct cam_isp_hw_dump_header);
	}
	CAM_DBG(CAM_ISP, "offset %zu", dump_args->offset);
	return 0;
}

int cam_vfe_top_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv   *top_priv = device_priv;
	struct cam_vfe_top_ver2_common_data common_data = top_priv->common_data;

	top_priv->hw_clk_rate = 0;

	top_priv->top_common.hw_version =
		cam_io_r_mb(common_data.soc_info->reg_map[0].mem_base +
		common_data.common_reg->hw_version);

	return 0;
}

int cam_vfe_top_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv   *top_priv = device_priv;
	struct cam_hw_soc_info         *soc_info = NULL;
	struct cam_vfe_top_ver2_reg_offset_common *reg_common = NULL;
	uint32_t *reset_reg_args = reset_core_args;
	uint32_t reset_reg_val;

	if (!top_priv || !reset_reg_args) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	switch (*reset_reg_args) {
	case CAM_VFE_HW_RESET_HW_AND_REG:
		reset_reg_val = CAM_VFE_HW_RESET_HW_AND_REG_VAL;
		break;
	default:
		reset_reg_val = CAM_VFE_HW_RESET_HW_VAL;
		break;
	}

	CAM_DBG(CAM_ISP, "reset reg value: %x", reset_reg_val);
	soc_info = top_priv->common_data.soc_info;
	reg_common = top_priv->common_data.common_reg;

	/* Mask All the IRQs except RESET */
	cam_io_w_mb((1 << 31),
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX) + 0x5C);

	/* Reset HW */
	cam_io_w_mb(reset_reg_val,
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX) +
		reg_common->global_reset_cmd);

	CAM_DBG(CAM_ISP, "Reset HW exit");
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
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	args = (struct cam_vfe_acquire_args *)reserve_args;
	acquire_args = &args->vfe_in;


	for (i = 0; i < top_priv->top_common.num_mux; i++) {
		if (top_priv->top_common.mux_rsrc[i].res_id ==
			acquire_args->res_id &&
			top_priv->top_common.mux_rsrc[i].res_state ==
			CAM_ISP_RESOURCE_STATE_AVAILABLE) {

			if (acquire_args->res_id == CAM_ISP_HW_VFE_IN_CAMIF) {
				rc = cam_vfe_camif_ver2_acquire_resource(
					&top_priv->top_common.mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			if (acquire_args->res_id ==
				CAM_ISP_HW_VFE_IN_PDLIB) {
				rc = cam_vfe_camif_lite_ver2_acquire_resource(
					&top_priv->top_common.mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			if ((acquire_args->res_id >=
				CAM_ISP_HW_VFE_IN_RDI0) &&
				(acquire_args->res_id <=
				CAM_ISP_HW_VFE_IN_RDI3)) {
				rc = cam_vfe_rdi_ver2_acquire_resource(
					&top_priv->top_common.mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			if (acquire_args->res_id == CAM_ISP_HW_VFE_IN_RD) {
				rc = cam_vfe_fe_ver1_acquire_resource(
					&top_priv->top_common.mux_rsrc[i],
					args);
				if (rc)
					break;
			}

			top_priv->top_common.mux_rsrc[i].cdm_ops =
				acquire_args->cdm_ops;
			top_priv->top_common.mux_rsrc[i].tasklet_info =
				args->tasklet;
			top_priv->top_common.mux_rsrc[i].res_state =
				CAM_ISP_RESOURCE_STATE_RESERVED;
			acquire_args->rsrc_node =
				&top_priv->top_common.mux_rsrc[i];

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
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)release_args;

	CAM_DBG(CAM_ISP, "Resource in state %d", mux_res->res_state);
	if (mux_res->res_state < CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Error! Resource in Invalid res_state :%d",
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
	struct cam_hw_info                      *hw_info = NULL;
	struct cam_hw_soc_info                  *soc_info = NULL;
	struct cam_vfe_soc_private              *soc_private = NULL;
	int rc = 0;

	if (!device_priv || !start_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv *)device_priv;
	soc_info = top_priv->common_data.soc_info;
	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error soc_private NULL");
		return -EINVAL;
	}

	mux_res = (struct cam_isp_resource_node *)start_args;
	hw_info = (struct cam_hw_info  *)mux_res->hw_intf->hw_priv;

	if (hw_info->hw_state == CAM_HW_STATE_POWER_UP) {
		rc = cam_vfe_top_set_hw_clk_rate(top_priv);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"set_hw_clk_rate failed, rc=%d", rc);
			return rc;
		}

		rc = cam_vfe_top_set_axi_bw_vote(soc_private,
			&top_priv->top_common, true);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"set_axi_bw_vote failed, rc=%d", rc);
			return rc;
		}

		if (mux_res->start) {
			rc = mux_res->start(mux_res);
		} else {
			CAM_ERR(CAM_ISP,
				"Invalid res id:%d", mux_res->res_id);
			rc = -EINVAL;
		}
	} else {
		CAM_ERR(CAM_ISP, "VFE HW not powered up");
		rc = -EPERM;
	}

	return rc;
}

int cam_vfe_top_stop(void *device_priv,
	void *stop_args, uint32_t arg_size)
{
	struct cam_vfe_top_ver2_priv            *top_priv;
	struct cam_isp_resource_node            *mux_res;
	struct cam_hw_info                      *hw_info = NULL;
	struct cam_hw_soc_info                  *soc_info = NULL;
	struct cam_vfe_soc_private              *soc_private = NULL;
	int i, rc = 0;

	if (!device_priv || !stop_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid input arguments");
		return -EINVAL;
	}

	top_priv = (struct cam_vfe_top_ver2_priv   *)device_priv;
	mux_res = (struct cam_isp_resource_node *)stop_args;
	hw_info = (struct cam_hw_info  *)mux_res->hw_intf->hw_priv;
	soc_info = top_priv->common_data.soc_info;
	soc_private = soc_info->soc_private;

	if ((mux_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF) ||
		(mux_res->res_id == CAM_ISP_HW_VFE_IN_PDLIB) ||
		(mux_res->res_id == CAM_ISP_HW_VFE_IN_RD) ||
		((mux_res->res_id >= CAM_ISP_HW_VFE_IN_RDI0) &&
		(mux_res->res_id <= CAM_ISP_HW_VFE_IN_RDI3))) {
		rc = mux_res->stop(mux_res);
	} else {
		CAM_ERR(CAM_ISP, "Invalid res id:%d", mux_res->res_id);
		return -EINVAL;
	}

	if (!rc) {
		for (i = 0; i < top_priv->top_common.num_mux; i++) {
			if (top_priv->top_common.mux_rsrc[i].res_id ==
				mux_res->res_id) {
				top_priv->req_clk_rate[i] = 0;
				top_priv->req_clk_rate[i] = 0;
				top_priv->top_common.req_axi_vote[i]
					.axi_path[0].camnoc_bw = 0;
				top_priv->top_common.req_axi_vote[i]
					.axi_path[0].mnoc_ab_bw = 0;
				top_priv->top_common.req_axi_vote[i]
					.axi_path[0].mnoc_ib_bw = 0;
				top_priv->top_common.axi_vote_control[i] =
					CAM_VFE_BW_CONTROL_EXCLUDE;
				break;
			}
		}
	}
	soc_private->ife_clk_src = 0;
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
	struct cam_hw_soc_info                  *soc_info = NULL;
	struct cam_vfe_soc_private              *soc_private = NULL;

	if (!device_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "Error! Invalid arguments");
		return -EINVAL;
	}
	top_priv = (struct cam_vfe_top_ver2_priv *)device_priv;
	soc_info = top_priv->common_data.soc_info;
	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Error soc_private NULL");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_CHANGE_BASE:
		rc = cam_vfe_top_mux_get_base(top_priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
		rc = cam_vfe_top_mux_get_reg_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_CAMIF_DATA:
		rc = cam_vfe_top_get_data(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_CLOCK_UPDATE:
		rc = cam_vfe_top_clock_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_FE_UPDATE_IN_RD:
		rc = cam_vfe_top_fs_update(top_priv, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_UPDATE:
		rc = cam_vfe_top_bw_update(soc_private, &top_priv->top_common,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_UPDATE_V2:
		rc = cam_vfe_top_bw_update_v2(soc_private,
			&top_priv->top_common, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_BW_CONTROL:
		rc = cam_vfe_top_bw_control(soc_private, &top_priv->top_common,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_DUMP_HW:
		rc = cam_vfe_hw_dump(top_priv,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_ADD_WAIT:
		rc = cam_vfe_top_wait_comp_event(top_priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_ADD_WAIT_TRIGGER:
		rc = cam_vfe_top_add_wait_trigger(top_priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_BLANKING_UPDATE:
		rc = cam_vfe_top_blanking_update(cmd_type, cmd_args, arg_size);
		break;
	default:
		rc = -EINVAL;
		CAM_ERR(CAM_ISP, "Error! Invalid cmd:%d", cmd_type);
		break;
	}

	return rc;
}

int cam_vfe_top_ver2_init(
	struct cam_hw_soc_info                 *soc_info,
	struct cam_hw_intf                     *hw_intf,
	void                                   *top_hw_info,
	void                                   *vfe_irq_controller,
	struct cam_vfe_top                    **vfe_top_ptr)
{
	int i, j, rc = 0;
	struct cam_vfe_top_ver2_priv           *top_priv = NULL;
	struct cam_vfe_top_ver2_hw_info        *ver2_hw_info = top_hw_info;
	struct cam_vfe_top                     *vfe_top;

	vfe_top = kzalloc(sizeof(struct cam_vfe_top), GFP_KERNEL);
	if (!vfe_top) {
		CAM_DBG(CAM_ISP, "Error! Failed to alloc for vfe_top");
		rc = -ENOMEM;
		goto end;
	}

	top_priv = kzalloc(sizeof(struct cam_vfe_top_ver2_priv),
		GFP_KERNEL);
	if (!top_priv) {
		CAM_DBG(CAM_ISP, "Error! Failed to alloc for vfe_top_priv");
		rc = -ENOMEM;
		goto free_vfe_top;
	}

	vfe_top->top_priv = top_priv;
	top_priv->hw_clk_rate = 0;
	if (ver2_hw_info->num_mux > CAM_VFE_TOP_MUX_MAX) {
		CAM_ERR(CAM_ISP, "Invalid number of input rsrc: %d, max: %d",
			ver2_hw_info->num_mux, CAM_VFE_TOP_MUX_MAX);
		rc = -EINVAL;
		goto free_top_priv;
	}

	top_priv->top_common.num_mux = ver2_hw_info->num_mux;

	for (i = 0, j = 0; i < top_priv->top_common.num_mux; i++) {
		top_priv->top_common.mux_rsrc[i].res_type =
			CAM_ISP_RESOURCE_VFE_IN;
		top_priv->top_common.mux_rsrc[i].hw_intf = hw_intf;
		top_priv->top_common.mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_AVAILABLE;
		top_priv->req_clk_rate[i] = 0;

		if (ver2_hw_info->mux_type[i] == CAM_VFE_CAMIF_VER_2_0) {
			top_priv->top_common.mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_CAMIF;

			rc = cam_vfe_camif_ver2_init(hw_intf, soc_info,
				&ver2_hw_info->camif_hw_info,
				&top_priv->top_common.mux_rsrc[i],
				vfe_irq_controller);
			if (rc)
				goto deinit_resources;
		} else if (ver2_hw_info->mux_type[i] ==
			CAM_VFE_CAMIF_LITE_VER_2_0) {
			top_priv->top_common.mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_PDLIB;

			rc = cam_vfe_camif_lite_ver2_init(hw_intf, soc_info,
				&ver2_hw_info->camif_lite_hw_info,
				&top_priv->top_common.mux_rsrc[i],
				vfe_irq_controller);

			if (rc)
				goto deinit_resources;
		} else if (ver2_hw_info->mux_type[i] ==
			CAM_VFE_RDI_VER_1_0) {
			/* set the RDI resource id */
			top_priv->top_common.mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_RDI0 + j++;

			rc = cam_vfe_rdi_ver2_init(hw_intf, soc_info,
				&ver2_hw_info->rdi_hw_info,
				&top_priv->top_common.mux_rsrc[i],
				vfe_irq_controller);
			if (rc)
				goto deinit_resources;
		} else if (ver2_hw_info->mux_type[i] ==
			CAM_VFE_IN_RD_VER_1_0) {
			/* set the RD resource id */
			top_priv->top_common.mux_rsrc[i].res_id =
				CAM_ISP_HW_VFE_IN_RD;

			rc = cam_vfe_fe_ver1_init(hw_intf, soc_info,
				&ver2_hw_info->fe_hw_info,
				&top_priv->top_common.mux_rsrc[i]);
			if (rc)
				goto deinit_resources;
		} else {
			CAM_WARN(CAM_ISP, "Invalid mux type: %u",
				ver2_hw_info->mux_type[i]);
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
	top_priv->top_common.hw_idx        = hw_intf->hw_idx;
	top_priv->common_data.common_reg   = ver2_hw_info->common_reg;
	top_priv->common_data.dump_data    = ver2_hw_info->dump_data;

	return rc;

deinit_resources:
	for (--i; i >= 0; i--) {
		if (ver2_hw_info->mux_type[i] == CAM_VFE_CAMIF_VER_2_0) {
			if (cam_vfe_camif_ver2_deinit(
				&top_priv->top_common.mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "Camif Deinit failed");
		} else if (ver2_hw_info->mux_type[i] ==
			CAM_VFE_CAMIF_LITE_VER_2_0) {
			if (cam_vfe_camif_lite_ver2_deinit(
				&top_priv->top_common.mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "Camif lite deinit failed");
		} else if (ver2_hw_info->mux_type[i] ==
			CAM_VFE_IN_RD_VER_1_0) {
			if (cam_vfe_fe_ver1_deinit(
				&top_priv->top_common.mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "VFE FE deinit failed");
		} else {
			if (cam_vfe_rdi_ver2_deinit(
				&top_priv->top_common.mux_rsrc[i]))
				CAM_ERR(CAM_ISP, "RDI Deinit failed");
		}
		top_priv->top_common.mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	}

free_top_priv:
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
		CAM_ERR(CAM_ISP, "Error! Invalid input");
		return -EINVAL;
	}

	vfe_top = *vfe_top_ptr;
	if (!vfe_top) {
		CAM_ERR(CAM_ISP, "Error! vfe_top NULL");
		return -ENODEV;
	}

	top_priv = vfe_top->top_priv;
	if (!top_priv) {
		CAM_ERR(CAM_ISP, "Error! vfe_top_priv NULL");
		rc = -ENODEV;
		goto free_vfe_top;
	}

	for (i = 0; i < top_priv->top_common.num_mux; i++) {
		top_priv->top_common.mux_rsrc[i].res_state =
			CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
		if (top_priv->top_common.mux_rsrc[i].res_type ==
			CAM_VFE_CAMIF_VER_2_0) {
			rc = cam_vfe_camif_ver2_deinit(
				&top_priv->top_common.mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP, "Camif deinit failed rc=%d",
					rc);
		} else if (top_priv->top_common.mux_rsrc[i].res_type ==
			CAM_VFE_CAMIF_LITE_VER_2_0) {
			rc = cam_vfe_camif_lite_ver2_deinit(
				&top_priv->top_common.mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP,
					"Camif lite deinit failed rc=%d", rc);
		} else if (top_priv->top_common.mux_rsrc[i].res_type ==
			CAM_VFE_RDI_VER_1_0) {
			rc = cam_vfe_rdi_ver2_deinit(
				&top_priv->top_common.mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP, "RDI deinit failed rc=%d", rc);
		} else if (top_priv->top_common.mux_rsrc[i].res_type ==
			CAM_VFE_IN_RD_VER_1_0) {
			rc = cam_vfe_fe_ver1_deinit(
				&top_priv->top_common.mux_rsrc[i]);
			if (rc)
				CAM_ERR(CAM_ISP, "Camif deinit failed rc=%d",
					rc);
		}
	}

	kfree(vfe_top->top_priv);

free_vfe_top:
	kfree(vfe_top);
	*vfe_top_ptr = NULL;

	return rc;
}
