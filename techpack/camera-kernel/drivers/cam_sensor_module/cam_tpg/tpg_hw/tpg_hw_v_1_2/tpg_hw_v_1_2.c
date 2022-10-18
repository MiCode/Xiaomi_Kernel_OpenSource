// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "tpg_hw_v_1_2.h"

/* TPG HW IDs */
enum cam_tpg_hw_id {
    CAM_TPG_0 = 13,
    CAM_TPG_1,
    CAM_TPG_MAX,
};

static struct cam_tpg_ver_1_2_reg_offset cam_tpg102_reg = {
	.tpg_hw_version = 0x0,
	.tpg_hw_status = 0x4,
	.tpg_module_cfg = 0x60,
	.tpg_cfg0 = 0x68,
	.tpg_cfg1 = 0x6C,
	.tpg_cfg2 = 0x70,
	.tpg_cfg3 = 0x74,
	.tpg_spare = 0x1FC,

	/* configurations */
	.major_version = 1,
	.minor_version = 0,
	.version_incr = 2,
	.tpg_en_shift = 0,
	.tpg_hbi_shift = 20,
	.tpg_dt_shift = 11,
	.tpg_rotate_period_shift = 5,
	.tpg_split_en_shift = 4,
	.top_mux_reg_offset = 0x90,
	.tpg_mux_sel_tpg_0_shift = 0,
	.tpg_mux_sel_tpg_1_shift = 8,
};

static int configure_global_configs(
	struct tpg_hw *hw,
	int num_vcs,
	struct tpg_global_config_t *configs)
{
	uint32_t val;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_tpg_ver_1_2_reg_offset *tpg_reg = &cam_tpg102_reg;

	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}
	soc_info = hw->soc_info;

	if (num_vcs <= 0) {
		CAM_ERR(CAM_TPG, "Invalid vc count");
		return -EINVAL;
	}

	/* Program number of frames */
	val = 0xFFFFF;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_cfg3);
	CAM_DBG(CAM_TPG, "TPG[%d] cfg3=0x%x",
			hw->hw_idx, val);

	val = cam_io_r_mb(soc_info->reg_map[1].mem_base +
			tpg_reg->top_mux_reg_offset);

	if (hw->hw_idx == CAM_TPG_0)
		val |= 1 << tpg_reg->tpg_mux_sel_tpg_0_shift;
	else if (hw->hw_idx == CAM_TPG_1)
		val |= 1 << tpg_reg->tpg_mux_sel_tpg_1_shift;

	cam_io_w_mb(val,
			soc_info->reg_map[1].mem_base + tpg_reg->top_mux_reg_offset);
	CAM_INFO(CAM_TPG, "TPG[%d] Set CPAS top mux: 0x%x",
			hw->hw_idx, val);

	val = (1 << tpg_reg->tpg_en_shift);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_module_cfg);
	CAM_DBG(CAM_TPG, "TPG[%d] tpg_module_cfg=0x%x", hw->hw_idx, val);

	return 0;
}

static int configure_dt(
	struct tpg_hw *hw,
	uint32_t       vc_slot,
	uint32_t       dt_slot,
	struct tpg_stream_config_t *stream)
{
	uint32_t val;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_tpg_ver_1_2_reg_offset *tpg_reg = &cam_tpg102_reg;
	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}

	soc_info = hw->soc_info;
	CAM_DBG(CAM_TPG, "TPG[%d] slot(%d,%d) <= dt:%d",
			hw->hw_idx,
			vc_slot,
			dt_slot,
			stream->dt);

	val = (((stream->stream_dimension.width & 0xFFFF) << 16) |
			(stream->stream_dimension.height & 0xFFFF));
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_cfg0);
	CAM_DBG(CAM_TPG, "TPG[%d] cfg0=0x%x",
			hw->hw_idx, val);

	val = stream->dt << tpg_reg->tpg_dt_shift;
	cam_io_w_mb(val,
			soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_cfg2);
	CAM_DBG(CAM_TPG, "TPG[%d] cfg2=0x%x",
			hw->hw_idx, val);

	return 0;
}

static int configure_vc(
	struct tpg_hw *hw,
	uint32_t       vc_slot,
	int            num_dts,
	struct tpg_stream_config_t *stream)
{
	uint32_t val = 0;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_tpg_ver_1_2_reg_offset *tpg_reg = &cam_tpg102_reg;
	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}

	soc_info = hw->soc_info;
	if (stream->cb_mode == TPG_COLOR_BAR_MODE_SPLIT)
		val |= (1 << tpg_reg->tpg_split_en_shift);

	CAM_DBG(CAM_TPG, "TPG[%d] period: %d", hw->hw_idx, stream->rotate_period);
	val |= ((stream->rotate_period & 0x3F) <<
			tpg_reg->tpg_rotate_period_shift);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_cfg2);
	CAM_DBG(CAM_TPG, "TPG[%d] cfg2=0x%x",
			hw->hw_idx, val);

	val = stream->hbi << tpg_reg->tpg_hbi_shift | stream->vbi;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_cfg1);
	CAM_DBG(CAM_TPG, "TPG[%d] cfg1=0x%x",
			hw->hw_idx, val);

	return 0;
}

static int tpg_hw_v_1_2_reset(
	struct tpg_hw *hw, void *data)
{
	struct cam_hw_soc_info *soc_info = NULL;
	uint32_t val;
	struct cam_tpg_ver_1_2_reg_offset *tpg_reg = &cam_tpg102_reg;
	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}

	soc_info = hw->soc_info;

	/* Clear out tpg_module_cfg before reset */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base + tpg_reg->tpg_module_cfg);

	/* Read the version */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_hw_version);
	CAM_INFO(CAM_TPG, "TPG[%d] TPG HW version: 0x%x started",
			hw->hw_idx, val);
	return 0;
}

int tpg_hw_v_1_2_process_cmd(
	struct tpg_hw *hw,
	uint32_t       cmd,
	void          *arg)
{
	int rc = 0;
	if (hw == NULL) {
		CAM_ERR(CAM_TPG, "invalid argument");
		return -EINVAL;
	}

	CAM_DBG(CAM_TPG, "TPG[%d] Cmd opcode:0x%x", hw->hw_idx, cmd);
	switch(cmd) {
	case TPG_CONFIG_VC:
	{
		struct vc_config_args *vc_config =
			(struct vc_config_args *)arg;

		if (vc_config == NULL) {
			CAM_ERR(CAM_TPG, "invalid argument");
			return -EINVAL;
		}
		rc = configure_vc(hw,
			vc_config->vc_slot,
			vc_config->num_dts,
			vc_config->stream);
	}
	break;
	case TPG_CONFIG_DT:
	{
		struct dt_config_args *dt_config =
			(struct dt_config_args *)arg;

		if (dt_config == NULL) {
			CAM_ERR(CAM_TPG, "invalid argument");
			return -EINVAL;
		}
		rc = configure_dt(hw,
			dt_config->vc_slot,
			dt_config->dt_slot,
			dt_config->stream);
	}
	break;
	case TPG_CONFIG_CTRL:
	{
		struct global_config_args *global_args =
			(struct global_config_args *)arg;
		rc = configure_global_configs(hw,
				global_args->num_vcs,
				global_args->globalconfig);
	}
	break;
	default:
		CAM_ERR(CAM_TPG, "invalid argument");
		break;
	}
	return rc;
}

int tpg_hw_v_1_2_start(struct tpg_hw *hw, void *data)
{
	CAM_DBG(CAM_TPG, "TPG V1.2 HWL start");
	return 0;
}

int tpg_hw_v_1_2_stop(struct tpg_hw *hw, void *data)
{
	CAM_DBG(CAM_TPG, "TPG V1.2 HWL stop");
	tpg_hw_v_1_2_reset(hw, data);
	return 0;
}

int tpg_hw_v_1_2_dump_status(struct tpg_hw *hw, void *data)
{
	struct cam_hw_soc_info *soc_info = NULL;
	uint32_t val;
	struct cam_tpg_ver_1_2_reg_offset *tpg_reg = &cam_tpg102_reg;

	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}

	soc_info = hw->soc_info;
	CAM_DBG(CAM_TPG, "TPG V1.2 HWL status dump");
	/* Read the version */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_hw_status);
	CAM_INFO(CAM_TPG, "TPG[%d] TPG HW status: 0x%x started",
			hw->hw_idx, val);

	return 0;
}

int tpg_hw_v_1_2_init(struct tpg_hw *hw, void *data)
{
	CAM_DBG(CAM_TPG, "TPG V1.2 HWL init");
	tpg_hw_v_1_2_reset(hw, data);
	return 0;
}
