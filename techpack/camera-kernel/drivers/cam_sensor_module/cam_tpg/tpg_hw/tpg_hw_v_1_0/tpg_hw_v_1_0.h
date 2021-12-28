/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __TPG_HW_V_1_0_H__
#define __TPG_HW_V_1_0_H__

#include "../tpg_hw.h"

struct cam_tpg_ver1_reg_offset {
	uint32_t tpg_hw_version;
	uint32_t tpg_hw_status;
	uint32_t tpg_ctrl;
	uint32_t tpg_vc_cfg0;
	uint32_t tpg_vc_cfg1;
	uint32_t tpg_lfsr_seed;
	uint32_t tpg_dt_0_cfg_0;
	uint32_t tpg_dt_1_cfg_0;
	uint32_t tpg_dt_2_cfg_0;
	uint32_t tpg_dt_3_cfg_0;
	uint32_t tpg_dt_0_cfg_1;
	uint32_t tpg_dt_1_cfg_1;
	uint32_t tpg_dt_2_cfg_1;
	uint32_t tpg_dt_3_cfg_1;
	uint32_t tpg_dt_0_cfg_2;
	uint32_t tpg_dt_1_cfg_2;
	uint32_t tpg_dt_2_cfg_2;
	uint32_t tpg_dt_3_cfg_2;
	uint32_t tpg_color_bar_cfg;
	uint32_t tpg_common_gen_cfg;
	uint32_t tpg_vbi_cfg;
	uint32_t tpg_test_bus_crtl;
	uint32_t tpg_spare;

	/* configurations */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t version_incr;
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
	uint32_t top_mux_reg_offset;
};

/**
 * @brief  : initialize the tpg hw v 1.0
 *
 * @param hw: tpg hw instance
 * @param data: initialize data
 *
 * @return : return 0 on success
 */
int tpg_hw_v_1_0_init(struct tpg_hw *hw, void *data);

/**
 * @brief : start tpg hw v 1.0
 *
 * @param hw: tpg hw instance
 * @param data: start argument
 *
 * @return : 0 on success
 */
int tpg_hw_v_1_0_start(struct tpg_hw *hw, void *data);

/**
 * @brief : stop tpg hw
 *
 * @param hw: tpg hw instance
 * @param data: arguments to stop tpg hw 1.0
 *
 * @return : 0 on success
 */
int tpg_hw_v_1_0_stop(struct tpg_hw *hw, void *data);


#endif
