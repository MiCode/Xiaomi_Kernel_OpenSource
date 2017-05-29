/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DSI_DEFS_H_
#define _DSI_DEFS_H_

#include <linux/types.h>

#define DSI_H_TOTAL(t) (((t)->h_active) + ((t)->h_back_porch) + \
			((t)->h_sync_width) + ((t)->h_front_porch))

#define DSI_V_TOTAL(t) (((t)->v_active) + ((t)->v_back_porch) + \
			((t)->v_sync_width) + ((t)->v_front_porch))

/**
 * enum dsi_pixel_format - DSI pixel formats
 * @DSI_PIXEL_FORMAT_RGB565:
 * @DSI_PIXEL_FORMAT_RGB666:
 * @DSI_PIXEL_FORMAT_RGB666_LOOSE:
 * @DSI_PIXEL_FORMAT_RGB888:
 * @DSI_PIXEL_FORMAT_RGB111:
 * @DSI_PIXEL_FORMAT_RGB332:
 * @DSI_PIXEL_FORMAT_RGB444:
 * @DSI_PIXEL_FORMAT_MAX:
 */
enum dsi_pixel_format {
	DSI_PIXEL_FORMAT_RGB565 = 0,
	DSI_PIXEL_FORMAT_RGB666,
	DSI_PIXEL_FORMAT_RGB666_LOOSE,
	DSI_PIXEL_FORMAT_RGB888,
	DSI_PIXEL_FORMAT_RGB111,
	DSI_PIXEL_FORMAT_RGB332,
	DSI_PIXEL_FORMAT_RGB444,
	DSI_PIXEL_FORMAT_MAX
};

/**
 * enum dsi_op_mode - dsi operation mode
 * @DSI_OP_VIDEO_MODE: DSI video mode operation
 * @DSI_OP_CMD_MODE:   DSI Command mode operation
 * @DSI_OP_MODE_MAX:
 */
enum dsi_op_mode {
	DSI_OP_VIDEO_MODE = 0,
	DSI_OP_CMD_MODE,
	DSI_OP_MODE_MAX
};

/**
 * enum dsi_mode_flags - flags to signal other drm components via private flags
 * @DSI_MODE_FLAG_SEAMLESS:	Seamless transition requested by user
 * @DSI_MODE_FLAG_DFPS:		Seamless transition is DynamicFPS
 * @DSI_MODE_FLAG_VBLANK_PRE_MODESET:	Transition needs VBLANK before Modeset
 */
enum dsi_mode_flags {
	DSI_MODE_FLAG_SEAMLESS			= BIT(0),
	DSI_MODE_FLAG_DFPS			= BIT(1),
	DSI_MODE_FLAG_VBLANK_PRE_MODESET	= BIT(2)
};

/**
 * enum dsi_data_lanes - dsi physical lanes
 * @DSI_DATA_LANE_0: Physical lane 0
 * @DSI_DATA_LANE_1: Physical lane 1
 * @DSI_DATA_LANE_2: Physical lane 2
 * @DSI_DATA_LANE_3: Physical lane 3
 * @DSI_CLOCK_LANE:  Physical clock lane
 */
enum dsi_data_lanes {
	DSI_DATA_LANE_0 = BIT(0),
	DSI_DATA_LANE_1 = BIT(1),
	DSI_DATA_LANE_2 = BIT(2),
	DSI_DATA_LANE_3 = BIT(3),
	DSI_CLOCK_LANE  = BIT(4)
};

/**
 * enum dsi_logical_lane - dsi logical lanes
 * @DSI_LOGICAL_LANE_0:     Logical lane 0
 * @DSI_LOGICAL_LANE_1:     Logical lane 1
 * @DSI_LOGICAL_LANE_2:     Logical lane 2
 * @DSI_LOGICAL_LANE_3:     Logical lane 3
 * @DSI_LOGICAL_CLOCK_LANE: Clock lane
 * @DSI_LANE_MAX:           Maximum lanes supported
 */
enum dsi_logical_lane {
	DSI_LOGICAL_LANE_0 = 0,
	DSI_LOGICAL_LANE_1,
	DSI_LOGICAL_LANE_2,
	DSI_LOGICAL_LANE_3,
	DSI_LOGICAL_CLOCK_LANE,
	DSI_LANE_MAX
};

/**
 * enum dsi_trigger_type - dsi trigger type
 * @DSI_TRIGGER_NONE:     No trigger.
 * @DSI_TRIGGER_TE:       TE trigger.
 * @DSI_TRIGGER_SEOF:     Start or End of frame.
 * @DSI_TRIGGER_SW:       Software trigger.
 * @DSI_TRIGGER_SW_SEOF:  Software trigger and start/end of frame.
 * @DSI_TRIGGER_SW_TE:    Software and TE triggers.
 * @DSI_TRIGGER_MAX:      Max trigger values.
 */
enum dsi_trigger_type {
	DSI_TRIGGER_NONE = 0,
	DSI_TRIGGER_TE,
	DSI_TRIGGER_SEOF,
	DSI_TRIGGER_SW,
	DSI_TRIGGER_SW_SEOF,
	DSI_TRIGGER_SW_TE,
	DSI_TRIGGER_MAX
};

/**
 * enum dsi_color_swap_mode - color swap mode
 * @DSI_COLOR_SWAP_RGB:
 * @DSI_COLOR_SWAP_RBG:
 * @DSI_COLOR_SWAP_BGR:
 * @DSI_COLOR_SWAP_BRG:
 * @DSI_COLOR_SWAP_GRB:
 * @DSI_COLOR_SWAP_GBR:
 */
enum dsi_color_swap_mode {
	DSI_COLOR_SWAP_RGB = 0,
	DSI_COLOR_SWAP_RBG,
	DSI_COLOR_SWAP_BGR,
	DSI_COLOR_SWAP_BRG,
	DSI_COLOR_SWAP_GRB,
	DSI_COLOR_SWAP_GBR
};

/**
 * enum dsi_dfps_type - Dynamic FPS support type
 * @DSI_DFPS_NONE:           Dynamic FPS is not supported.
 * @DSI_DFPS_SUSPEND_RESUME:
 * @DSI_DFPS_IMMEDIATE_CLK:
 * @DSI_DFPS_IMMEDIATE_HFP:
 * @DSI_DFPS_IMMEDIATE_VFP:
 * @DSI_DPFS_MAX:
 */
enum dsi_dfps_type {
	DSI_DFPS_NONE = 0,
	DSI_DFPS_SUSPEND_RESUME,
	DSI_DFPS_IMMEDIATE_CLK,
	DSI_DFPS_IMMEDIATE_HFP,
	DSI_DFPS_IMMEDIATE_VFP,
	DSI_DFPS_MAX
};

/**
 * enum dsi_phy_type - DSI phy types
 * @DSI_PHY_TYPE_DPHY:
 * @DSI_PHY_TYPE_CPHY:
 */
enum dsi_phy_type {
	DSI_PHY_TYPE_DPHY,
	DSI_PHY_TYPE_CPHY
};

/**
 * enum dsi_te_mode - dsi te source
 * @DSI_TE_ON_DATA_LINK:    TE read from DSI link
 * @DSI_TE_ON_EXT_PIN:      TE signal on an external GPIO
 */
enum dsi_te_mode {
	DSI_TE_ON_DATA_LINK = 0,
	DSI_TE_ON_EXT_PIN,
};

/**
 * enum dsi_video_traffic_mode - video mode pixel transmission type
 * @DSI_VIDEO_TRAFFIC_SYNC_PULSES:       Non-burst mode with sync pulses.
 * @DSI_VIDEO_TRAFFIC_SYNC_START_EVENTS: Non-burst mode with sync start events.
 * @DSI_VIDEO_TRAFFIC_BURST_MODE:        Burst mode using sync start events.
 */
enum dsi_video_traffic_mode {
	DSI_VIDEO_TRAFFIC_SYNC_PULSES = 0,
	DSI_VIDEO_TRAFFIC_SYNC_START_EVENTS,
	DSI_VIDEO_TRAFFIC_BURST_MODE,
};

/**
 * struct dsi_mode_info - video mode information dsi frame
 * @h_active:         Active width of one frame in pixels.
 * @h_back_porch:     Horizontal back porch in pixels.
 * @h_sync_width:     HSYNC width in pixels.
 * @h_front_porch:    Horizontal fron porch in pixels.
 * @h_skew:
 * @h_sync_polarity:  Polarity of HSYNC (false is active high).
 * @v_active:         Active height of one frame in lines.
 * @v_back_porch:     Vertical back porch in lines.
 * @v_sync_width:     VSYNC width in lines.
 * @v_front_porch:    Vertical front porch in lines.
 * @v_sync_polarity:  Polarity of VSYNC (false is active high).
 * @refresh_rate:     Refresh rate in Hz.
 */
struct dsi_mode_info {
	u32 h_active;
	u32 h_back_porch;
	u32 h_sync_width;
	u32 h_front_porch;
	u32 h_skew;
	bool h_sync_polarity;

	u32 v_active;
	u32 v_back_porch;
	u32 v_sync_width;
	u32 v_front_porch;
	bool v_sync_polarity;

	u32 refresh_rate;
};

/**
 * struct dsi_lane_mapping - Mapping between DSI logical and physical lanes
 * @physical_lane0:   Logical lane to which physical lane 0 is mapped.
 * @physical_lane1:   Logical lane to which physical lane 1 is mapped.
 * @physical_lane2:   Logical lane to which physical lane 2 is mapped.
 * @physical_lane3:   Logical lane to which physical lane 3 is mapped.
 */
struct dsi_lane_mapping {
	enum dsi_logical_lane physical_lane0;
	enum dsi_logical_lane physical_lane1;
	enum dsi_logical_lane physical_lane2;
	enum dsi_logical_lane physical_lane3;
};

/**
 * struct dsi_host_common_cfg - Host configuration common to video and cmd mode
 * @dst_format:          Destination pixel format.
 * @data_lanes:          Physical data lanes to be enabled.
 * @en_crc_check:        Enable CRC checks.
 * @en_ecc_check:        Enable ECC checks.
 * @te_mode:             Source for TE signalling.
 * @mdp_cmd_trigger:     MDP frame update trigger for command mode.
 * @dma_cmd_trigger:     Command DMA trigger.
 * @cmd_trigger_stream:  Command mode stream to trigger.
 * @bit_swap_read:       Is red color bit swapped.
 * @bit_swap_green:      Is green color bit swapped.
 * @bit_swap_blue:       Is blue color bit swapped.
 * @t_clk_post:          Number of byte clock cycles that the transmitter shall
 *                       continue sending after last data lane has transitioned
 *                       to LP mode.
 * @t_clk_pre:           Number of byte clock cycles that the high spped clock
 *                       shall be driven prior to data lane transitions from LP
 *                       to HS mode.
 * @ignore_rx_eot:       Ignore Rx EOT packets if set to true.
 * @append_tx_eot:       Append EOT packets for forward transmissions if set to
 *                       true.
 * @force_clk_lane_hs:   Force clock lane in high speed mode.
 */
struct dsi_host_common_cfg {
	enum dsi_pixel_format dst_format;
	enum dsi_data_lanes data_lanes;
	bool en_crc_check;
	bool en_ecc_check;
	enum dsi_te_mode te_mode;
	enum dsi_trigger_type mdp_cmd_trigger;
	enum dsi_trigger_type dma_cmd_trigger;
	u32 cmd_trigger_stream;
	enum dsi_color_swap_mode swap_mode;
	bool bit_swap_red;
	bool bit_swap_green;
	bool bit_swap_blue;
	u32 t_clk_post;
	u32 t_clk_pre;
	bool ignore_rx_eot;
	bool append_tx_eot;
	bool force_clk_lane_hs;
};

/**
 * struct dsi_video_engine_cfg - DSI video engine configuration
 * @host_cfg:                  Pointer to host common configuration.
 * @last_line_interleave_en:   Allow command mode op interleaved on last line of
 *                             video stream.
 * @pulse_mode_hsa_he:         Send HSA and HE following VS/VE packet if set to
 *                             true.
 * @hfp_lp11_en:               Enter low power stop mode (LP-11) during HFP.
 * @hbp_lp11_en:               Enter low power stop mode (LP-11) during HBP.
 * @hsa_lp11_en:               Enter low power stop mode (LP-11) during HSA.
 * @eof_bllp_lp11_en:          Enter low power stop mode (LP-11) during BLLP of
 *                             last line of a frame.
 * @bllp_lp11_en:              Enter low power stop mode (LP-11) during BLLP.
 * @traffic_mode:              Traffic mode for video stream.
 * @vc_id:                     Virtual channel identifier.
 */
struct dsi_video_engine_cfg {
	bool last_line_interleave_en;
	bool pulse_mode_hsa_he;
	bool hfp_lp11_en;
	bool hbp_lp11_en;
	bool hsa_lp11_en;
	bool eof_bllp_lp11_en;
	bool bllp_lp11_en;
	enum dsi_video_traffic_mode traffic_mode;
	u32 vc_id;
};

/**
 * struct dsi_cmd_engine_cfg - DSI command engine configuration
 * @host_cfg:                  Pointer to host common configuration.
 * @host_cfg:                      Common host configuration
 * @max_cmd_packets_interleave     Maximum number of command mode RGB packets to
 *                                 send with in one horizontal blanking period
 *                                 of the video mode frame.
 * @wr_mem_start:                  DCS command for write_memory_start.
 * @wr_mem_continue:               DCS command for write_memory_continue.
 * @insert_dcs_command:            Insert DCS command as first byte of payload
 *                                 of the pixel data.
 * @mdp_transfer_time_us   Specifies the mdp transfer time for command mode
 *                         panels in microseconds
 */
struct dsi_cmd_engine_cfg {
	u32 max_cmd_packets_interleave;
	u32 wr_mem_start;
	u32 wr_mem_continue;
	bool insert_dcs_command;
	u32 mdp_transfer_time_us;
};

/**
 * struct dsi_host_config - DSI host configuration parameters.
 * @panel_mode:            Operation mode for panel (video or cmd mode).
 * @common_config:         Host configuration common to both Video and Cmd mode.
 * @video_engine:          Video engine configuration if panel is in video mode.
 * @cmd_engine:            Cmd engine configuration if panel is in cmd mode.
 * @esc_clk_rate_khz:      Esc clock frequency in Hz.
 * @bit_clk_rate_hz:       Bit clock frequency in Hz.
 * @video_timing:          Video timing information of a frame.
 * @lane_map:              Mapping between logical and physical lanes.
 * @phy_type:              PHY type to be used.
 */
struct dsi_host_config {
	enum dsi_op_mode panel_mode;
	struct dsi_host_common_cfg common_config;
	union {
		struct dsi_video_engine_cfg video_engine;
		struct dsi_cmd_engine_cfg cmd_engine;
	} u;
	u64 esc_clk_rate_hz;
	u64 bit_clk_rate_hz;
	struct dsi_mode_info video_timing;
	struct dsi_lane_mapping lane_map;
};

/**
 * struct dsi_display_mode - specifies mode for dsi display
 * @timing:         Timing parameters for the panel.
 * @pixel_clk_khz:  Pixel clock in Khz.
 * @panel_mode:     Panel operation mode.
 * @flags:          Additional flags.
 */
struct dsi_display_mode {
	struct dsi_mode_info timing;
	u32 pixel_clk_khz;
	enum dsi_op_mode panel_mode;

	u32 flags;
};

#endif /* _DSI_DEFS_H_ */
