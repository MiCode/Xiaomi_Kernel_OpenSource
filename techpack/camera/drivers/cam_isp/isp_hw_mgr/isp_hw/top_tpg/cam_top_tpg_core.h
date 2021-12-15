/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_TOP_TPG_CORE_H_
#define _CAM_TOP_TPG_CORE_H_

#include "cam_hw.h"
#include "cam_top_tpg_hw_intf.h"
#include "cam_top_tpg_soc.h"

#define CAM_TOP_TPG_VERSION_1             0x10000001
#define CAM_TOP_TPG_VERSION_2             0x10000002
#define CAM_TOP_TPG_VERSION_3             0x20000000


enum cam_top_tpg_encode_format {
	CAM_TOP_TPG_ENCODE_FORMAT_RAW6,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW8,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW10,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW12,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW14,
	CAM_TOP_TPG_ENCODE_FORMAT_RAW16,
	CAM_TOP_TPG_ENCODE_FORMAT_MAX,
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
	void                                   *tpg_reg;
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
 * @bayer_pattern:   Bayer patter information
 * @rotate_period:   period value for repeating color, 0 for no rotate
 * @split_en:        enables split mode
 * @unicolor_en:     enables unicolor value
 * @unicolor_sel:    select color used in unicolor mode
 *
 */
struct cam_top_tpg_dt_cfg {
	uint32_t                               frame_width;
	uint32_t                               frame_height;
	uint32_t                               data_type;
	uint32_t                               encode_format;
	uint32_t                               payload_mode;
	uint32_t                               bayer_pattern;
	uint32_t                               rotate_period;
	uint32_t                               split_en;
	uint32_t                               unicolor_en;
	uint32_t                               unicolor_sel;
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
 * @num_frames:      number of output frames
 * @qcfa_en:         enable qcfa in color bar cfg
 * @dt_cfg:          dt configuration values
 *
 */
struct cam_top_tpg_cfg {
	uint32_t                        pix_pattern;
	uint32_t                        phy_sel;
	uint32_t                        num_active_lanes;
	uint32_t                        vc_num[4];
	uint32_t                        v_blank_count;
	uint32_t                        h_blank_count;
	uint32_t                        vbi_cnt;
	uint32_t                        num_active_dts;
	uint32_t                        num_frames;
	uint32_t                        vc_dt_pattern_id;
	uint32_t                        qcfa_en;
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

int cam_top_tpg_get_format(uint32_t    in_format, uint32_t *tpg_encode_format);

int cam_top_tpg_probe_init(struct cam_hw_intf *tpg_hw_intf,
	uint32_t tpg_idx);

int cam_top_tpg_deinit(struct cam_top_tpg_hw *top_tpg_hw);

#endif /* _CAM_TOP_TPG_CORE_H_ */
