// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "tpg_hw_v_1_3.h"

enum tpg_hw_v_1_3_encode_fomat_t {
	RAW_8_BIT = 1,
	RAW_10_BIT,
	RAW_12_BIT,
	RAW_14_BIT,
	RAW_16_BIT
};

static struct cam_tpg_ver_1_3_reg_offset cam_tpg103_reg = {
	.tpg_hw_version        = 0x0,
	.tpg_hw_status         = 0x4,
	.tpg_ctrl              = 0x64,
	.tpg_vc0_cfg0          = 0x68,
	.tpg_vc0_lfsr_seed     = 0x6C,
	.tpg_vc0_hbi_cfg       = 0x70,
	.tpg_vc0_vbi_cfg       = 0x74,
	.tpg_vc0_color_bar_cfg = 0x78,
	.tpg_vc0_dt_0_cfg_0   = 0x7C,
	.tpg_vc0_dt_0_cfg_1   = 0x80,
	.tpg_vc0_dt_0_cfg_2   = 0x84,
	.tpg_vc0_dt_1_cfg_0   = 0x88,
	.tpg_vc0_dt_1_cfg_1   = 0x8C,
	.tpg_vc0_dt_1_cfg_2   = 0x90,
	.tpg_vc0_dt_2_cfg_0   = 0x94,
	.tpg_vc0_dt_2_cfg_1   = 0x98,
	.tpg_vc0_dt_2_cfg_2   = 0x9C,
	.tpg_vc0_dt_3_cfg_0   = 0xA0,
	.tpg_vc0_dt_3_cfg_1   = 0xA4,
	.tpg_vc0_dt_3_cfg_2   = 0xA8,

	.tpg_vc1_cfg0          = 0xC8,
	.tpg_vc1_lfsr_seed     = 0xCC,
	.tpg_vc1_hbi_cfg       = 0xD0,
	.tpg_vc1_vbi_cfg       = 0xD4,
	.tpg_vc1_color_bar_cfg = 0xD8,
	.tpg_vc1_dt_0_cfg_0   = 0xDC,
	.tpg_vc1_dt_0_cfg_1   = 0xE0,
	.tpg_vc1_dt_0_cfg_2   = 0xE4,
	.tpg_vc1_dt_1_cfg_0   = 0xE8,
	.tpg_vc1_dt_1_cfg_1   = 0xEC,
	.tpg_vc1_dt_1_cfg_2   = 0xF0,
	.tpg_vc1_dt_2_cfg_0   = 0xF4,
	.tpg_vc1_dt_2_cfg_1   = 0xF8,
	.tpg_vc1_dt_2_cfg_2   = 0xFC,
	.tpg_vc1_dt_3_cfg_0   = 0x100,
	.tpg_vc1_dt_3_cfg_1   = 0x104,
	.tpg_vc1_dt_3_cfg_2   = 0x108,

	.tpg_vc2_cfg0          = 0x128,
	.tpg_vc2_lfsr_seed     = 0x12C,
	.tpg_vc2_hbi_cfg       = 0x130,
	.tpg_vc2_vbi_cfg       = 0x134,
	.tpg_vc2_color_bar_cfg = 0x138,
	.tpg_vc2_dt_0_cfg_0   = 0x13C,
	.tpg_vc2_dt_0_cfg_1   = 0x140,
	.tpg_vc2_dt_0_cfg_2   = 0x144,
	.tpg_vc2_dt_1_cfg_0   = 0x148,
	.tpg_vc2_dt_1_cfg_1   = 0x14C,
	.tpg_vc2_dt_1_cfg_2   = 0x150,
	.tpg_vc2_dt_2_cfg_0   = 0x154,
	.tpg_vc2_dt_2_cfg_1   = 0x158,
	.tpg_vc2_dt_2_cfg_2   = 0x15C,
	.tpg_vc2_dt_3_cfg_0   = 0x160,
	.tpg_vc2_dt_3_cfg_1   = 0x164,
	.tpg_vc2_dt_3_cfg_2   = 0x168,

	.tpg_vc3_cfg0          = 0x188,
	.tpg_vc3_lfsr_seed     = 0x18C,
	.tpg_vc3_hbi_cfg       = 0x190,
	.tpg_vc3_vbi_cfg       = 0x194,
	.tpg_vc3_color_bar_cfg = 0x198,
	.tpg_vc3_dt_0_cfg_0   = 0x19C,
	.tpg_vc3_dt_0_cfg_1   = 0x1A0,
	.tpg_vc3_dt_0_cfg_2   = 0x1A4,
	.tpg_vc3_dt_1_cfg_0   = 0x1A8,
	.tpg_vc3_dt_1_cfg_1   = 0x1AC,
	.tpg_vc3_dt_1_cfg_2   = 0x1B0,
	.tpg_vc3_dt_2_cfg_0   = 0x1B4,
	.tpg_vc3_dt_2_cfg_1   = 0x1B8,
	.tpg_vc3_dt_2_cfg_2   = 0x1BC,
	.tpg_vc3_dt_3_cfg_0   = 0x1C0,
	.tpg_vc3_dt_3_cfg_1   = 0x1C4,
	.tpg_vc3_dt_3_cfg_2   = 0x1C8,
	.tpg_throttle          = 0x1CC,
	.tpg_top_irq_status    = 0x1E0,
	.tpg_top_irq_mask      = 0x1E4,
	.tpg_top_irq_clear     = 0x1E8,
	.tpg_top_irq_set       = 0x1EC,
	.tpg_top_irq_cmd       = 0x1F0,
	.tpg_top_clear         = 0x1F4,
	.tpg_test_bus_crtl     = 0x1F8,
	.tpg_spare             = 0x1FC,

	/* configurations */
	.major_version = 2,
	.minor_version = 0,
	.version_incr = 0,
	.tpg_en_shift_val = 0,
	.tpg_cphy_dphy_sel_shift_val = 3,
	.tpg_num_active_lanes_shift = 4,
	.tpg_fe_pkt_en_shift = 2,
	.tpg_fs_pkt_en_shift = 1,
	.tpg_line_interleaving_mode_shift = 10,
	.tpg_num_frames_shift_val = 16,
	.tpg_num_dts_shift_val = 8,
	.tpg_v_blank_cnt_shift = 12,
	.tpg_dt_encode_format_shift = 20,
	.tpg_payload_mode_color = 0x8,
	.tpg_split_en_shift = 4,
	.top_mux_reg_offset = 0x1C,
	.tpg_vc_dt_pattern_id_shift = 6,
	.tpg_num_active_vcs_shift = 30,
	.tpg_color_bar_qcfa_en_shift = 3,
	.tpg_color_bar_qcfa_rotate_period_shift = 8,
};

#define  FRAME_INTERLEAVE  0x0
#define  LINE_INTERLEAVE   0x1
#define  SHDR_INTERLEAVE   0x2
#define  SPARSE_PD_INTERLEAVE 0x3
static int get_tpg_vc_dt_pattern_id(
		enum tpg_interleaving_format_t vc_dt_pattern)
{
	switch (vc_dt_pattern) {
	case TPG_INTERLEAVING_FORMAT_INVALID:
	case TPG_INTERLEAVING_FORMAT_MAX:
	case TPG_INTERLEAVING_FORMAT_FRAME:
		return FRAME_INTERLEAVE;
	case TPG_INTERLEAVING_FORMAT_LINE:
		return LINE_INTERLEAVE;
	case TPG_INTERLEAVING_FORMAT_SHDR:
		return SHDR_INTERLEAVE;
	case TPG_INTERLEAVING_FORMAT_SPARSE_PD:
		return SPARSE_PD_INTERLEAVE;

	}
	return FRAME_INTERLEAVE;
}

static int configure_global_configs(
	struct tpg_hw *hw,
	int num_vcs,
	struct tpg_global_config_t *configs)
{
	uint32_t val, phy_type = 0;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_tpg_ver_1_3_reg_offset *tpg_reg = &cam_tpg103_reg;

	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}
	soc_info = hw->soc_info;

	if (configs->phy_type == TPG_PHY_TYPE_CPHY)
		phy_type = 1;

	if (num_vcs <= 0) {
		CAM_ERR(CAM_TPG, "Invalid vc count");
		return -EINVAL;
	}

	val = configs->skip_pattern;
	cam_io_w_mb(val,
		soc_info->reg_map[0].mem_base + tpg_reg->tpg_throttle);
	CAM_DBG(CAM_TPG, "tpg[%d] throttle=0x%x", hw->hw_idx, val);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_top_irq_mask);


	val = ((num_vcs - 1) <<
			(tpg_reg->tpg_num_active_vcs_shift) |
			(configs->lane_count - 1) <<
			tpg_reg->tpg_num_active_lanes_shift) |
		get_tpg_vc_dt_pattern_id(configs->interleaving_format) <<
		(tpg_reg->tpg_vc_dt_pattern_id_shift) |
		(phy_type << tpg_reg->tpg_cphy_dphy_sel_shift_val) |
		(1 << tpg_reg->tpg_en_shift_val);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base + tpg_reg->tpg_ctrl);
	CAM_DBG(CAM_TPG, "tpg[%d] tpg_ctrl=0x%x", hw->hw_idx, val);

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

#define  INCREMENTING       0x0
#define  ALTERNATING_55_AA  0x1
#define  RANDOM             0x4
#define  USER_SPECIFIED     0x5
#define  COLOR_BARS         0x8

static int get_tpg_payload_mode(enum tpg_pattern_t pattern)
{
	switch (pattern) {
	case TPG_PATTERN_INVALID:
	case TPG_PATTERN_REAL_IMAGE:
	case TPG_PATTERN_COLOR_BAR:
		return COLOR_BARS;
	case TPG_PATTERN_RANDOM_PIXL:
	case TPG_PATTERN_RANDOM_INCREMENTING_PIXEL:
		return RANDOM;
	case TPG_PATTERN_ALTERNATING_55_AA:
		return ALTERNATING_55_AA;
	case TPG_PATTERN_ALTERNATING_USER_DEFINED:
		return USER_SPECIFIED;
	default:
		return COLOR_BARS;
	}
	return COLOR_BARS;
}

static int configure_dt(
	struct tpg_hw *hw,
	uint32_t       vc_slot,
	uint32_t       dt_slot,
	struct tpg_stream_config_t *stream)
{
	uint32_t val;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_tpg_ver_1_3_reg_offset *tpg_reg = &cam_tpg103_reg;
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
			tpg_reg->tpg_vc0_dt_0_cfg_0 +
			(0x60 * vc_slot) + (dt_slot * 0x0c));
	CAM_DBG(CAM_TPG, "TPG[%d] vc%d_dt%d_cfg_0=0x%x",
			hw->hw_idx,
			vc_slot, dt_slot, val);

	cam_io_w_mb(stream->dt,
			soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_dt_0_cfg_1 +
			(0x60 * vc_slot) + (dt_slot * 0x0c));
	CAM_DBG(CAM_TPG, "TPG[%d] vc%d_dt%d_cfg_1=0x%x",
			hw->hw_idx,
			vc_slot, dt_slot, stream->dt);

	val = ((get_tpg_encode_format(stream->pixel_depth) & 0xF) <<
			tpg_reg->tpg_dt_encode_format_shift) |
		get_tpg_payload_mode(stream->pattern_type);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_dt_0_cfg_2 +
			(0x60 * vc_slot) + (dt_slot * 0x0c));
	CAM_DBG(CAM_TPG, "TPG[%d] vc%d_dt%d_cfg_2=0x%x",
			hw->hw_idx,
			vc_slot, dt_slot, val);

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
	struct cam_tpg_ver_1_3_reg_offset *tpg_reg = &cam_tpg103_reg;
	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}

	soc_info = hw->soc_info;
	/* Use CFA pattern here */
	if (stream->output_format == TPG_IMAGE_FORMAT_QCFA)
		val |= (1 << tpg_reg->tpg_color_bar_qcfa_en_shift);

	if (stream->cb_mode == TPG_COLOR_BAR_MODE_SPLIT)
		val |= (1 << tpg_reg->tpg_split_en_shift);

	CAM_DBG(CAM_TPG, "TPG[%d] period: %d", hw->hw_idx, stream->rotate_period);
	val |= ((stream->rotate_period & 0x3F) <<
			tpg_reg->tpg_color_bar_qcfa_rotate_period_shift);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_color_bar_cfg + (0x60 * vc_slot));
	CAM_DBG(CAM_TPG, "TPG[%d] vc%d_color_bar_cfg=0x%x",
			hw->hw_idx,
			vc_slot, val);

	val = stream->hbi;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_hbi_cfg + (0x60 * vc_slot));
	CAM_DBG(CAM_TPG, "TPG[%d] vc%d_hbi_cfg=0x%x",
			hw->hw_idx,
			vc_slot, val);

	val = stream->vbi;
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_vbi_cfg + (0x60 * vc_slot));
	CAM_DBG(CAM_TPG, "TPG[%d] vc%d_vbi_cgf=0x%x",
			hw->hw_idx,
			vc_slot, val);

	cam_io_w_mb(0x12345678,
		soc_info->reg_map[0].mem_base +
		tpg_reg->tpg_vc0_lfsr_seed + (0x60 * vc_slot));

	val = ((0 << tpg_reg->tpg_num_frames_shift_val) |
		((num_dts-1) <<	 tpg_reg->tpg_num_dts_shift_val) |
		stream->vc);
	cam_io_w_mb(val, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_vc0_cfg0 + (0x60 * vc_slot));
	CAM_DBG(CAM_TPG, "TPG[%d] vc%d_cfg0=0x%x",
			hw->hw_idx,
			vc_slot, val);

	return 0;
}

static int tpg_hw_v_1_3_reset(
	struct tpg_hw *hw, void *data)
{
	struct cam_hw_soc_info *soc_info = NULL;
	uint32_t val;
	struct cam_tpg_ver_1_3_reg_offset *tpg_reg = &cam_tpg103_reg;
	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}

	soc_info = hw->soc_info;

	/* Clear out tpg_ctrl and irqs before reset */
	cam_io_w_mb(0, soc_info->reg_map[0].mem_base + tpg_reg->tpg_ctrl);

	cam_io_w_mb(0, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_top_irq_mask);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_top_irq_clear);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_top_irq_cmd);

	cam_io_w_mb(1, soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_top_clear);

	/* Read the version */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_hw_version);
	CAM_INFO(CAM_TPG, "TPG[%d] TPG HW version: 0x%x started",
			hw->hw_idx, val);
	return 0;
}

int tpg_hw_v_1_3_process_cmd(
	struct tpg_hw *hw,
	uint32_t       cmd,
	void          *arg)
{
	int rc = 0;
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

int tpg_hw_v_1_3_start(struct tpg_hw *hw, void *data)
{
	CAM_DBG(CAM_TPG, "TPG V1.3 HWL start");
	return 0;
}

int tpg_hw_v_1_3_stop(struct tpg_hw *hw, void *data)
{
	CAM_DBG(CAM_TPG, "TPG V1.3 HWL stop");
	tpg_hw_v_1_3_reset(hw, data);
	return 0;
}

int tpg_hw_v_1_3_dump_status(struct tpg_hw *hw, void *data)
{
	struct cam_hw_soc_info *soc_info = NULL;
	uint32_t val;
	struct cam_tpg_ver_1_3_reg_offset *tpg_reg = &cam_tpg103_reg;

	if (!hw) {
		CAM_ERR(CAM_TPG, "invalid params");
		return -EINVAL;
	}

	soc_info = hw->soc_info;
	CAM_DBG(CAM_TPG, "TPG V1.3 HWL status dump");
	/* Read the version */
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_hw_status);
	CAM_INFO(CAM_TPG, "TPG[%d] TPG HW status: 0x%x started",
			hw->hw_idx, val);
	val = cam_io_r_mb(soc_info->reg_map[0].mem_base +
			tpg_reg->tpg_top_irq_status);
	CAM_INFO(CAM_TPG, "TPG[%d] TPG HW irq status: 0x%x started",
			hw->hw_idx, val);

	return 0;
}

int tpg_hw_v_1_3_init(struct tpg_hw *hw, void *data)
{
	CAM_DBG(CAM_TPG, "TPG V1.3 HWL init");
	tpg_hw_v_1_3_reset(hw, data);
	return 0;
}
