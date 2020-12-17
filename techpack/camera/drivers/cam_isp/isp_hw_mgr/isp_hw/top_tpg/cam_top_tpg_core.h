/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TOP_TPG_HW_H_
#define _CAM_TOP_TPG_HW_H_

#include "cam_hw.h"
#include "cam_top_tpg_hw_intf.h"
#include "cam_top_tpg_soc.h"

enum cam_top_tpg_encode_format {
	CAM_TOP_TPG_ENCODE_FORMAT_RAW6,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW8,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW10,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW12,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW14,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW16,
	CAM_TOP_TPG_ENCODE_FORMAT_MAX,
};

struct cam_top_tpg_reg_offset {
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
 * struct cam_top_tpg_hw_info- tpg hardware info
 *
 * @tpg_reg:         tpg register offsets
 * @hw_dts_version:  HW DTS version
 * @csid_max_clk:    maximum csid clock
 * @phy_max_clk      maximum phy clock
 *
 */
struct cam_top_tpg_hw_info {
	const struct cam_top_tpg_reg_offset    *tpg_reg;
	uint32_t                                hw_dts_version;
	uint32_t                                csid_max_clk;
	uint32_t                                phy_max_clk;
};

/**
 * struct cam_top_tpg_dt_cfg- tpg data type(dt) configuration
 *
 * @frame_width:     frame width in pixel
 * @frame_height:    frame height in pixel
 * @data_type:       data type(dt) value
 * @encode_format:   encode format for this data type
 * @payload_mode     payload data, such color bar, color box etc
 *
 */

struct cam_top_tpg_dt_cfg {
	uint32_t                               frame_width;
	uint32_t                               frame_height;
	uint32_t                               data_type;
	uint32_t                               encode_format;
	uint32_t                               payload_mode;
};

/**
 * struct cam_top_tpg_cfg- tpg congiguration
 * @pix_pattern :    pixel pattern output of the tpg
 * @phy_sel :        phy selection 0:dphy or 1:cphy
 * @num_active_lanes Number of active lines
 * @vc_num:          Virtual channel number
 * @h_blank_count:   horizontal blanking count value
 * @h_blank_count:   vertical blanking count value
 * @vbi_cnt:         vbi count
 * @num_active_dts:  number of active dts need to configure
 * @dt_cfg:          dt configuration values
 *
 */
struct cam_top_tpg_cfg {
	uint32_t                        pix_pattern;
	uint32_t                        phy_sel;
	uint32_t                        num_active_lanes;
	uint32_t                        vc_num;
	uint32_t                        v_blank_count;
	uint32_t                        h_blank_count;
	uint32_t                        vbi_cnt;
	uint32_t                        num_active_dts;
	struct cam_top_tpg_dt_cfg       dt_cfg[4];
};

/**
 * struct cam_top_tpg_hw- tpg hw device resources data
 *
 * @hw_intf:                  contain the tpg hw interface information
 * @hw_info:                  tpg hw device information
 * @tpg_info:                 tpg hw specific information
 * @tpg_res:                  tpg resource
 * @tpg_cfg:                  tpg configuration
 * @clk_rate                  clock rate
 * @lock_state                lock state
 * @tpg_complete              tpg completion
 *
 */
struct cam_top_tpg_hw {
	struct cam_hw_intf              *hw_intf;
	struct cam_hw_info              *hw_info;
	struct cam_top_tpg_hw_info      *tpg_info;
	struct cam_isp_resource_node     tpg_res;
	uint64_t                         clk_rate;
	spinlock_t                       lock_state;
	struct completion                tpg_complete;
};

int cam_top_tpg_hw_probe_init(struct cam_hw_intf  *tpg_hw_intf,
	uint32_t tpg_idx);

int cam_top_tpg_hw_deinit(struct cam_top_tpg_hw *top_tpg_hw);

#endif /* _CAM_TOP_TPG_HW_H_ */
