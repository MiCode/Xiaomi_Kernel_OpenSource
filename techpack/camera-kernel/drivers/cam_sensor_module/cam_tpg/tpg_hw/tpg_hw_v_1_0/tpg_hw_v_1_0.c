// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "tpg_hw_v_1_0.h"

enum tpg_hw_encode_format_t {
	RAW_8_BIT = 1,
	RAW_10_BIT,
	RAW_12_BIT,
	RAW_14_BIT,
	RAW_16_BIT
};

static struct cam_tpg_ver1_reg_offset cam_tpg101_reg = {
	.tpg_hw_version     = 0x0,
	.tpg_hw_status      = 0x4,
	.tpg_ctrl           = 0x60,
	.tpg_vc_cfg0        = 0x64,
	.tpg_vc_cfg1        = 0x68,
	.tpg_lfsr_seed      = 0x6c,
	.tpg_dt_0_cfg_0     = 0x70,
	.tpg_dt_1_cfg_0     = 0x74,
	.tpg_dt_2_cfg_0     = 0x78,
	.tpg_dt_3_cfg_0     = 0x7C,
	.tpg_dt_0_cfg_1     = 0x80,
	.tpg_dt_1_cfg_1     = 0x84,
	.tpg_dt_2_cfg_1     = 0x88,
	.tpg_dt_3_cfg_1     = 0x8C,
	.tpg_dt_0_cfg_2     = 0x90,
	.tpg_dt_1_cfg_2     = 0x94,
	.tpg_dt_2_cfg_2     = 0x98,
	.tpg_dt_3_cfg_2     = 0x9C,
	.tpg_color_bar_cfg  = 0xA0,
	.tpg_common_gen_cfg = 0xA4,
	.tpg_vbi_cfg        = 0xA8,
	.tpg_test_bus_crtl  = 0xF8,
	.tpg_spare          = 0xFC,

	/* configurations */
	.major_version     = 1,
	.minor_version     = 0,
	.version_incr      = 0,
	.tpg_en_shift_val  = 0,
	.tpg_phy_sel_shift_val      = 3,
	.tpg_num_active_lines_shift = 4,
	.tpg_fe_pkt_en_shift        = 2,
	.tpg_fs_pkt_en_shift        = 1,
	.tpg_line_interleaving_mode_shift = 10,
	.tpg_num_dts_shift_val            = 8,
	.tpg_v_blank_cnt_shift            = 12,
	.tpg_dt_encode_format_shift       = 16,
	.tpg_payload_mode_color = 0x8,
	.tpg_split_en_shift     = 5,
	.top_mux_reg_offset     = 0x1C,
};

static int configure_global_configs(struct tpg_hw *hw,
		struct tpg_global_config_t *configs)
{
	uint32_t val;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_tpg_ver1_reg_offset *tpg_reg = &cam_tpg101_reg;

	if (hw == NULL) {
		CAM_ERR(CAM_TPG, "invalid argument");
		return -EINVAL;
	}
	soc_info = hw->soc_info;

	val = cam_io_r_mb(soc_info->reg_map[1].mem_base +
			tpg_reg->top_mux_reg_offset);
	val |= (1 << hw->hw_idx);

	cam_io_w_mb(val,
			soc_info->reg_map[1].mem_base + tpg_reg->top_mux_reg_offset);
	CAM_INFO(CAM_ISP, "TPG:%d Set top Mux: 0x%x",
			hw->hw_idx, val);

	val = ((4 - 1) <<
			tpg_reg->tpg_num_active_lines_shift) |
		(1 << tpg_reg->tpg_fe_pkt_en_shift) |
		(1 << tpg_reg->tpg_fs_pkt_en_shift) |
		(0 << tpg_reg->tpg_phy_sel_shift_val) |
		(1 << tpg_reg->tpg_en_shift_val);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_ctrl);

	return 0;
}

static int get_tpg_encode_format(int sw_encode_format)
{
	switch (sw_encode_format) {
	case PACK_8_BIT:
		return RAW_8_BIT;
	case PACK_10_BIT:
		return RAW_10_BIT;
	case PACK_12_BIT:
		return RAW_12_BIT;
	case PACK_14_BIT:
		return RAW_14_BIT;
	case PACK_16_BIT:
		return RAW_16_BIT;
	}
	return RAW_8_BIT;
}

static int configure_dt(
	struct tpg_hw *hw,
	uint32_t       vc_slot,
	uint32_t       dt_slot,
	struct tpg_stream_config_t *stream)
{
	uint32_t val;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_tpg_ver1_reg_offset *tpg_reg = &cam_tpg101_reg;

	if (hw == NULL) {
		CAM_ERR(CAM_TPG, "invalid argument");
		return -EINVAL;
	}
	soc_info = hw->soc_info;

	CAM_DBG(CAM_TPG, "TPG[%d] slot(%d,%d) <= dt:%d",
			hw->hw_idx,
			vc_slot,
			dt_slot,
			stream->dt);
	/* configure width and height */
	val = (((stream->stream_dimension.width & 0xFFFF) << 16) |
			(stream->stream_dimension.height & 0x3FFF));
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_dt_0_cfg_0 + 0x10 * dt_slot);

	/* configure data type */
	cam_io_w_mb(stream->dt,
			soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_dt_0_cfg_1 + 0x10 * dt_slot);

	/* configure bpp */
	val = ((get_tpg_encode_format(stream->pixel_depth) & 0xF) <<
			tpg_reg->tpg_dt_encode_format_shift) |
		tpg_reg->tpg_payload_mode_color;

	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_dt_0_cfg_2 + 0x10 * dt_slot);

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
	struct cam_tpg_ver1_reg_offset *tpg_reg = &cam_tpg101_reg;

	if (hw == NULL) {
		CAM_ERR(CAM_TPG, "invalid argument");
		return -EINVAL;
	}
	soc_info = hw->soc_info;

	CAM_DBG(CAM_TPG, "Configureing vc : %d at the slot : %d num_dts=%d",
			stream->vc, vc_slot, num_dts);
	val = ((num_dts - 1) <<
			tpg_reg->tpg_num_dts_shift_val) | stream->vc;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_vc_cfg0);

	cam_io_w_mb(stream->hbi,
			soc_info->reg_map[0].mem_base + tpg_reg->tpg_vc_cfg1);

	val = (1 << tpg_reg->tpg_split_en_shift);
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_common_gen_cfg);

	cam_io_w_mb(stream->vbi,
			soc_info->reg_map[0].mem_base + tpg_reg->tpg_vbi_cfg);

	return 0;
}

static int tpg_hw_v_1_0_reset(struct tpg_hw *hw, void *data)
{
	struct cam_hw_soc_info *soc_info = NULL;
	uint32_t val;
	struct cam_tpg_ver1_reg_offset *tpg_reg = &cam_tpg101_reg;
	if (hw == NULL) {
		CAM_ERR(CAM_TPG, "invalid argument");
		return -EINVAL;
	}
	soc_info = hw->soc_info;

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_ctrl);

	/* Reset the TOP tpg mux sel*/
	val = cam_io_r_mb(soc_info->reg_map[1].mem_base +
			tpg_reg->top_mux_reg_offset);
	val &= ~(1 << hw->hw_idx);

	cam_io_w_mb(val,
			soc_info->reg_map[1].mem_base + tpg_reg->top_mux_reg_offset);
	CAM_INFO(CAM_TPG, "TPG:%d Reset Top Mux: 0x%x",
			hw->hw_idx, val);

	return 0;
}

int tpg_hw_v_1_0_process_cmd(
	struct tpg_hw *hw,
	uint32_t       cmd,
	void          *arg)
{

	if (hw == NULL) {
		CAM_ERR(CAM_TPG, "invalid argument");
		return -EINVAL;
	}
	switch(cmd) {
	case TPG_CONFIG_VC:
	{
		struct vc_config_args *vc_config =
			(struct vc_config_args *)arg;

		if (vc_config == NULL) {
			CAM_ERR(CAM_TPG, "invalid argument");
			return -EINVAL;
		}
		configure_vc(hw,
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
		configure_dt(hw,
			dt_config->vc_slot,
			dt_config->dt_slot,
			dt_config->stream);
	}
	break;
	case TPG_CONFIG_CTRL:
		configure_global_configs(hw, arg);
	break;
	default:
		CAM_ERR(CAM_TPG, "invalid argument");
		break;
	}
	return 0;
}

int tpg_hw_v_1_0_stop(struct tpg_hw *hw, void *data)
{
	CAM_INFO(CAM_TPG, "TPG V1.0 HWL stop");
	tpg_hw_v_1_0_reset(hw, data);
	return 0;
}

int tpg_hw_v_1_0_start(struct tpg_hw *hw, void *data)
{
	CAM_DBG(CAM_TPG, "TPG V1.3 HWL start");
	return 0;
}

int tpg_hw_v_1_0_init(struct tpg_hw *hw, void *data)
{
	CAM_INFO(CAM_TPG, "TPG V1.0 HWL init");
	tpg_hw_v_1_0_reset(hw, data);
	return 0;
}
