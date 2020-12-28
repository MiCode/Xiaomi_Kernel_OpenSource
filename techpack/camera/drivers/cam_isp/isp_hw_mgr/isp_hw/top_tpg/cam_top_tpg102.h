/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TOP_TPG102_H_
#define _CAM_TOP_TPG102_H_

#include "cam_top_tpg_ver2.h"
#include "cam_top_tpg_core.h"

static struct cam_top_tpg_ver2_reg_offset cam_top_tpg102_reg = {
	.tpg_hw_version = 0x0,
	.tpg_hw_status = 0x4,
	.tpg_module_cfg = 0x60,
	.tpg_cfg_0 = 0x68,
	.tpg_cfg_1 = 0x6C,
	.tpg_cfg_2 = 0x70,
	.tpg_cfg_3 = 0x74,
	.tpg_spare = 0xFC,
	.top_mux_sel = 0x90,
	/* configurations */
	.major_version = 1,
	.minor_version = 0,
	.version_incr = 0,
	.tpg_module_en = 1,
	.tpg_mux_sel_en = 1,
	.tpg_mux_sel_tpg_0_shift = 0,
	.tpg_mux_sel_tpg_1_shift = 8,
	.tpg_en_shift_val = 0,
	.tpg_phy_sel_shift_val = 3,
	.tpg_num_active_lines_shift = 4,
	.tpg_fe_pkt_en_shift = 2,
	.tpg_fs_pkt_en_shift = 1,
	.tpg_line_interleaving_mode_shift = 10,
	.tpg_num_dts_shift_val = 8,
	.tpg_v_blank_cnt_shift = 12,
	.tpg_dt_encode_format_shift = 16,
	.tpg_payload_mode_color = 0x8,
	.tpg_split_en_shift = 5,
};

struct cam_top_tpg_hw_info cam_top_tpg102_hw_info = {
	.tpg_reg = &cam_top_tpg102_reg,
	.hw_dts_version = CAM_TOP_TPG_VERSION_2,
	.csid_max_clk = 400000000,
	.phy_max_clk = 400000000,
};

#endif /* _CAM_TOP_TPG102_H_ */
