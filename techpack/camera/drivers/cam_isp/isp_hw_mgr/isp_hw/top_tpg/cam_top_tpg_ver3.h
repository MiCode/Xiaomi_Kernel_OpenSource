/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TOP_TPG_VER3_H_
#define _CAM_TOP_TPG_VER3_H_

#include "cam_top_tpg_core.h"

struct cam_top_tpg_ver3_reg_offset {
	uint32_t tpg_hw_version;
	uint32_t tpg_hw_status;
	uint32_t tpg_ctrl;
	uint32_t tpg_vc0_cfg0;
	uint32_t tpg_vc0_lfsr_seed;
	uint32_t tpg_vc0_hbi_cfg;
	uint32_t tpg_vc0_vbi_cfg;
	uint32_t tpg_vc0_color_bar_cfg;
	uint32_t tpg_vc0_dt_0_cfg_0;
	uint32_t tpg_vc0_dt_0_cfg_1;
	uint32_t tpg_vc0_dt_0_cfg_2;
	uint32_t tpg_vc0_dt_1_cfg_0;
	uint32_t tpg_vc0_dt_1_cfg_1;
	uint32_t tpg_vc0_dt_1_cfg_2;
	uint32_t tpg_vc0_dt_2_cfg_0;
	uint32_t tpg_vc0_dt_2_cfg_1;
	uint32_t tpg_vc0_dt_2_cfg_2;
	uint32_t tpg_vc0_dt_3_cfg_0;
	uint32_t tpg_vc0_dt_3_cfg_1;
	uint32_t tpg_vc0_dt_3_cfg_2;

	uint32_t tpg_vc1_cfg0;
	uint32_t tpg_vc1_lfsr_seed;
	uint32_t tpg_vc1_hbi_cfg;
	uint32_t tpg_vc1_vbi_cfg;
	uint32_t tpg_vc1_color_bar_cfg;
	uint32_t tpg_vc1_dt_0_cfg_0;
	uint32_t tpg_vc1_dt_0_cfg_1;
	uint32_t tpg_vc1_dt_0_cfg_2;
	uint32_t tpg_vc1_dt_1_cfg_0;
	uint32_t tpg_vc1_dt_1_cfg_1;
	uint32_t tpg_vc1_dt_1_cfg_2;
	uint32_t tpg_vc1_dt_2_cfg_0;
	uint32_t tpg_vc1_dt_2_cfg_1;
	uint32_t tpg_vc1_dt_2_cfg_2;
	uint32_t tpg_vc1_dt_3_cfg_0;
	uint32_t tpg_vc1_dt_3_cfg_1;
	uint32_t tpg_vc1_dt_3_cfg_2;

	uint32_t tpg_vc2_cfg0;
	uint32_t tpg_vc2_lfsr_seed;
	uint32_t tpg_vc2_hbi_cfg;
	uint32_t tpg_vc2_vbi_cfg;
	uint32_t tpg_vc2_color_bar_cfg;
	uint32_t tpg_vc2_dt_0_cfg_0;
	uint32_t tpg_vc2_dt_0_cfg_1;
	uint32_t tpg_vc2_dt_0_cfg_2;
	uint32_t tpg_vc2_dt_1_cfg_0;
	uint32_t tpg_vc2_dt_1_cfg_1;
	uint32_t tpg_vc2_dt_1_cfg_2;
	uint32_t tpg_vc2_dt_2_cfg_0;
	uint32_t tpg_vc2_dt_2_cfg_1;
	uint32_t tpg_vc2_dt_2_cfg_2;
	uint32_t tpg_vc2_dt_3_cfg_0;
	uint32_t tpg_vc2_dt_3_cfg_1;
	uint32_t tpg_vc2_dt_3_cfg_2;

	uint32_t tpg_vc3_cfg0;
	uint32_t tpg_vc3_lfsr_seed;
	uint32_t tpg_vc3_hbi_cfg;
	uint32_t tpg_vc3_vbi_cfg;
	uint32_t tpg_vc3_color_bar_cfg;
	uint32_t tpg_vc3_dt_0_cfg_0;
	uint32_t tpg_vc3_dt_0_cfg_1;
	uint32_t tpg_vc3_dt_0_cfg_2;
	uint32_t tpg_vc3_dt_1_cfg_0;
	uint32_t tpg_vc3_dt_1_cfg_1;
	uint32_t tpg_vc3_dt_1_cfg_2;
	uint32_t tpg_vc3_dt_2_cfg_0;
	uint32_t tpg_vc3_dt_2_cfg_1;
	uint32_t tpg_vc3_dt_2_cfg_2;
	uint32_t tpg_vc3_dt_3_cfg_0;
	uint32_t tpg_vc3_dt_3_cfg_1;
	uint32_t tpg_vc3_dt_3_cfg_2;
	uint32_t tpg_throttle;
	uint32_t tpg_top_irq_status;
	uint32_t tpg_top_irq_mask;
	uint32_t tpg_top_irq_clear;
	uint32_t tpg_top_irq_set;
	uint32_t tpg_top_irq_cmd;
	uint32_t tpg_top_clear;
	uint32_t tpg_test_bus_crtl;
	uint32_t tpg_spare;

	/* configurations */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t version_incr;
	uint32_t tpg_en_shift_val;
	uint32_t tpg_cphy_dphy_sel_shift_val;
	uint32_t tpg_num_active_lanes_shift;
	uint32_t tpg_fe_pkt_en_shift;
	uint32_t tpg_fs_pkt_en_shift;
	uint32_t tpg_line_interleaving_mode_shift;
	uint32_t tpg_num_dts_shift_val;
	uint32_t tpg_v_blank_cnt_shift;
	uint32_t tpg_dt_encode_format_shift;
	uint32_t tpg_payload_mode_color;
	uint32_t tpg_split_en_shift;
	uint32_t top_mux_reg_offset;
	uint32_t tpg_vc_dt_pattern_id_shift;
	uint32_t tpg_num_active_vcs_shift;
	uint32_t tpg_color_bar_qcfa_en_shift;
};

int cam_top_tpg_ver3_init(struct cam_top_tpg_hw *tpg_hw);

#endif /* _CAM_TOP_TPG_VER3_H_ */
