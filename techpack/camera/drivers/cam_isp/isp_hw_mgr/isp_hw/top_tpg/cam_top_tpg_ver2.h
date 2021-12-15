/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TOP_TPG_VER2_H_
#define _CAM_TOP_TPG_VER2_H_

#include "cam_top_tpg_core.h"

struct cam_top_tpg_ver2_reg_offset {
	uint32_t tpg_hw_version;
	uint32_t tpg_hw_status;
	uint32_t tpg_module_cfg;
	uint32_t tpg_cfg_0;
	uint32_t tpg_cfg_1;
	uint32_t tpg_cfg_2;
	uint32_t tpg_cfg_3;
	uint32_t tpg_spare;
	uint32_t top_mux_sel;
	/* configurations */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t version_incr;
	uint32_t tpg_module_en;
	uint32_t tpg_mux_sel_en;
	uint32_t tpg_mux_sel_tpg_0_shift;
	uint32_t tpg_mux_sel_tpg_1_shift;
	uint32_t tpg_en_shift_val;
	uint32_t tpg_phy_sel_shift_val;
	uint32_t tpg_num_active_lines_shift;
	uint32_t tpg_fe_pkt_en_shift;
	uint32_t tpg_fs_pkt_en_shift;
	uint32_t tpg_line_interleaving_mode_shift;
	uint32_t tpg_num_dts_shift_val;
	uint32_t tpg_v_blank_cnt_shift;
	uint32_t tpg_dt_encode_format_shift;
	uint32_t tpg_payload_mode_color;
	uint32_t tpg_split_en_shift;
};

int cam_top_tpg_ver2_init(struct cam_top_tpg_hw *tpg_hw);

#endif /* _CAM_TOP_TPG_VER2_H_ */
