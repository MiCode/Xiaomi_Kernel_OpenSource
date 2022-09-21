/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TOP_TPG101_H_
#define _CAM_TOP_TPG101_H_

#include "cam_top_tpg_ver1.h"
#include "cam_top_tpg_core.h"

static struct cam_top_tpg_ver1_reg_offset cam_top_tpg101_reg = {
	.tpg_hw_version = 0x0,
	.tpg_hw_status = 0x4,
	.tpg_ctrl = 0x60,
	.tpg_vc_cfg0 = 0x64,
	.tpg_vc_cfg1 = 0x68,
	.tpg_lfsr_seed = 0x6c,
	.tpg_dt_0_cfg_0 = 0x70,
	.tpg_dt_1_cfg_0 = 0x74,
	.tpg_dt_2_cfg_0 = 0x78,
	.tpg_dt_3_cfg_0 = 0x7C,
	.tpg_dt_0_cfg_1 = 0x80,
	.tpg_dt_1_cfg_1 = 0x84,
	.tpg_dt_2_cfg_1 = 0x88,
	.tpg_dt_3_cfg_1 = 0x8C,
	.tpg_dt_0_cfg_2 = 0x90,
	.tpg_dt_1_cfg_2 = 0x94,
	.tpg_dt_2_cfg_2 = 0x98,
	.tpg_dt_3_cfg_2 = 0x9C,
	.tpg_color_bar_cfg = 0xA0,
	.tpg_common_gen_cfg = 0xA4,
	.tpg_vbi_cfg = 0xA8,
	.tpg_test_bus_crtl = 0xF8,
	.tpg_spare = 0xFC,
	/* configurations */
	.major_version = 1,
	.minor_version = 0,
	.version_incr = 0,
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
	.top_mux_reg_offset = 0x1C,
};

struct cam_top_tpg_hw_info cam_top_tpg101_hw_info = {
	.tpg_reg = &cam_top_tpg101_reg,
	.hw_dts_version = CAM_TOP_TPG_VERSION_1,
	.csid_max_clk = 426400000,
	.phy_max_clk = 384000000,
};

#endif /* _CAM_TOP_TPG101_H_ */
