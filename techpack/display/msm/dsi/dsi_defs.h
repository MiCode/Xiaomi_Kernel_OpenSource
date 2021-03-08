/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_DEFS_H_
#define _DSI_DEFS_H_

#include <linux/types.h>
#include <drm/drm_mipi_dsi.h>
#include "msm_drv.h"

#define DSI_H_TOTAL(t) (((t)->h_active) + ((t)->h_back_porch) + \
			((t)->h_sync_width) + ((t)->h_front_porch))

#define DSI_V_TOTAL(t) (((t)->v_active) + ((t)->v_back_porch) + \
			((t)->v_sync_width) + ((t)->v_front_porch))

#define DSI_H_TOTAL_DSC(t) \
	({\
		u64 value;\
		if ((t)->dsc_enabled && (t)->dsc)\
			value = (t)->dsc->pclk_per_line;\
		else\
			value = (t)->h_active;\
		value = value + (t)->h_back_porch + (t)->h_sync_width +\
			(t)->h_front_porch;\
		value;\
	})

#define DSI_H_ACTIVE_DSC(t) \
	({\
		u64 value;\
		if ((t)->dsc_enabled && (t)->dsc)\
			value = (t)->dsc->pclk_per_line;\
		else\
			value = (t)->h_active;\
		value;\
	})


#define DSI_DEBUG_NAME_LEN		32
#define display_for_each_ctrl(index, display) \
	for (index = 0; (index < (display)->ctrl_count) &&\
			(index < MAX_DSI_CTRLS_PER_DISPLAY); index++)

#define DSI_WARN(fmt, ...)	DRM_WARN("[msm-dsi-warn]: "fmt, ##__VA_ARGS__)
#define DSI_ERR(fmt, ...)	DRM_DEV_ERROR(NULL, "[msm-dsi-error]: " fmt, \
								##__VA_ARGS__)
#define DSI_INFO(fmt, ...)	DRM_DEV_INFO(NULL, "[msm-dsi-info]: "fmt, \
								##__VA_ARGS__)
#define DSI_DEBUG(fmt, ...)	DRM_DEV_DEBUG(NULL, "[msm-dsi-debug]: "fmt, \
								##__VA_ARGS__)
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
 * @DSI_MODE_FLAG_DMS: Seamless transition is dynamic mode switch
 * @DSI_MODE_FLAG_VRR: Seamless transition is DynamicFPS.
 *                     New timing values are sent from DAL.
 * @DSI_MODE_FLAG_POMS:
 *         Seamless transition is dynamic panel operating mode switch
 * @DSI_MODE_FLAG_DYN_CLK: Seamless transition is dynamic clock change
 * @DSI_MODE_FLAG_DMS_FPS: Seamless fps only transition in Dynamic Mode Switch
 */
enum dsi_mode_flags {
	DSI_MODE_FLAG_SEAMLESS			= BIT(0),
	DSI_MODE_FLAG_DFPS			= BIT(1),
	DSI_MODE_FLAG_VBLANK_PRE_MODESET	= BIT(2),
	DSI_MODE_FLAG_DMS			= BIT(3),
	DSI_MODE_FLAG_VRR			= BIT(4),
	DSI_MODE_FLAG_POMS			= BIT(5),
	DSI_MODE_FLAG_DYN_CLK			= BIT(6),
	DSI_MODE_FLAG_DMS_FPS                   = BIT(7),
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
 * enum dsi_data_lanes - BIT map for DSI data lanes
 * This is used to identify the active DSI data lanes for
 * various operations like DSI data lane enable/ULPS/clamp
 * configurations.
 * @DSI_DATA_LANE_0: BIT(DSI_LOGICAL_LANE_0)
 * @DSI_DATA_LANE_1: BIT(DSI_LOGICAL_LANE_1)
 * @DSI_DATA_LANE_2: BIT(DSI_LOGICAL_LANE_2)
 * @DSI_DATA_LANE_3: BIT(DSI_LOGICAL_LANE_3)
 * @DSI_CLOCK_LANE:  BIT(DSI_LOGICAL_CLOCK_LANE)
 */
enum dsi_data_lanes {
	DSI_DATA_LANE_0 = BIT(DSI_LOGICAL_LANE_0),
	DSI_DATA_LANE_1 = BIT(DSI_LOGICAL_LANE_1),
	DSI_DATA_LANE_2 = BIT(DSI_LOGICAL_LANE_2),
	DSI_DATA_LANE_3 = BIT(DSI_LOGICAL_LANE_3),
	DSI_CLOCK_LANE  = BIT(DSI_LOGICAL_CLOCK_LANE)
};

/**
 * enum dsi_phy_data_lanes - dsi physical lanes
 * used for DSI logical to physical lane mapping
 * @DSI_PHYSICAL_LANE_INVALID: Physical lane valid/invalid
 * @DSI_PHYSICAL_LANE_0: Physical lane 0
 * @DSI_PHYSICAL_LANE_1: Physical lane 1
 * @DSI_PHYSICAL_LANE_2: Physical lane 2
 * @DSI_PHYSICAL_LANE_3: Physical lane 3
 */
enum dsi_phy_data_lanes {
	DSI_PHYSICAL_LANE_INVALID = 0,
	DSI_PHYSICAL_LANE_0 = BIT(0),
	DSI_PHYSICAL_LANE_1 = BIT(1),
	DSI_PHYSICAL_LANE_2 = BIT(2),
	DSI_PHYSICAL_LANE_3  = BIT(3)
};

enum dsi_lane_map_type_v1 {
	DSI_LANE_MAP_0123,
	DSI_LANE_MAP_3012,
	DSI_LANE_MAP_2301,
	DSI_LANE_MAP_1230,
	DSI_LANE_MAP_0321,
	DSI_LANE_MAP_1032,
	DSI_LANE_MAP_2103,
	DSI_LANE_MAP_3210,
};

/**
 * lane_map: DSI logical <-> physical lane mapping
 * lane_map_v1: Lane mapping for DSI controllers < v2.0
 * lane_map_v2: Lane mapping for DSI controllers >= 2.0
 */
struct dsi_lane_map {
	enum dsi_lane_map_type_v1 lane_map_v1;
	u8 lane_map_v2[DSI_LANE_MAX - 1];
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
 * enum dsi_dyn_clk_feature_type - Dynamic clock feature support type
 * @DSI_DYN_CLK_TYPE_LEGACY:			Constant FPS is not supported
 * @DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_HFP:	Constant FPS supported with
 *						change in hfp
 * @DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_VFP:	Constant FPS supported with
 *						change in vfp
 * @DSI_DYN_CLK_TYPE_MAX:
 */
enum dsi_dyn_clk_feature_type {
	DSI_DYN_CLK_TYPE_LEGACY = 0,
	DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_HFP,
	DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_VFP,
	DSI_DYN_CLK_TYPE_MAX
};

/**
 * enum dsi_cmd_set_type  - DSI command set type
 * @DSI_CMD_SET_PRE_ON:	                   Panel pre on
 * @DSI_CMD_SET_ON:                        Panel on
 * @DSI_CMD_SET_POST_ON:                   Panel post on
 * @DSI_CMD_SET_PRE_OFF:                   Panel pre off
 * @DSI_CMD_SET_OFF:                       Panel off
 * @DSI_CMD_SET_POST_OFF:                  Panel post off
 * @DSI_CMD_SET_PRE_RES_SWITCH:            Pre resolution switch
 * @DSI_CMD_SET_RES_SWITCH:                Resolution switch
 * @DSI_CMD_SET_POST_RES_SWITCH:           Post resolution switch
 * @DSI_CMD_SET_CMD_TO_VID_SWITCH:         Cmd to video mode switch
 * @DSI_CMD_SET_POST_CMD_TO_VID_SWITCH:    Post cmd to vid switch
 * @DSI_CMD_SET_VID_TO_CMD_SWITCH:         Video to cmd mode switch
 * @DSI_CMD_SET_POST_VID_TO_CMD_SWITCH:    Post vid to cmd switch
 * @DSI_CMD_SET_PANEL_STATUS:              Panel status
 * @DSI_CMD_SET_LP1:                       Low power mode 1
 * @DSI_CMD_SET_LP2:                       Low power mode 2
 * @DSI_CMD_SET_NOLP:                      Low power mode disable
 * @DSI_CMD_SET_PPS:                       DSC PPS command
 * @DSI_CMD_SET_ROI:			   Panel ROI update
 * @DSI_CMD_SET_TIMING_SWITCH:             Timing switch
 * @DSI_CMD_SET_POST_TIMING_SWITCH:        Post timing switch
 * @DSI_CMD_SET_QSYNC_ON                   Enable qsync mode
 * @DSI_CMD_SET_QSYNC_OFF                  Disable qsync mode
 * @DSI_CMD_SET_MAX
 */
enum dsi_cmd_set_type {
	DSI_CMD_SET_PRE_ON = 0,
	DSI_CMD_SET_ON,
	DSI_CMD_SET_POST_ON,
	DSI_CMD_SET_PRE_OFF,
	DSI_CMD_SET_OFF,
	DSI_CMD_SET_POST_OFF,
	DSI_CMD_SET_PRE_RES_SWITCH,
	DSI_CMD_SET_RES_SWITCH,
	DSI_CMD_SET_POST_RES_SWITCH,
	DSI_CMD_SET_CMD_TO_VID_SWITCH,
	DSI_CMD_SET_POST_CMD_TO_VID_SWITCH,
	DSI_CMD_SET_VID_TO_CMD_SWITCH,
	DSI_CMD_SET_POST_VID_TO_CMD_SWITCH,
	DSI_CMD_SET_PANEL_STATUS,
	DSI_CMD_SET_LP1,
	DSI_CMD_SET_LP2,
	DSI_CMD_SET_NOLP,
	DSI_CMD_SET_PPS,
	DSI_CMD_SET_ROI,
	DSI_CMD_SET_TIMING_SWITCH,
	DSI_CMD_SET_POST_TIMING_SWITCH,
	DSI_CMD_SET_QSYNC_ON,
	DSI_CMD_SET_QSYNC_OFF,
	/* xiaomi add start */
	DSI_CMD_SET_MI_CEON,
	DSI_CMD_SET_MI_CEOFF,
	DSI_CMD_SET_MI_CABCUION,
	DSI_CMD_SET_MI_CABCSTILLON,
	DSI_CMD_SET_MI_CABCMOVIEON,
	DSI_CMD_SET_MI_CABCOFF,
	DSI_CMD_SET_MI_SKINCE_CABCUION,
	DSI_CMD_SET_MI_SKINCE_CABCSTILLON,
	DSI_CMD_SET_MI_SKINCE_CABCMOVIEON,
	DSI_CMD_SET_MI_SKINCE_CABCOFF,
	DSI_CMD_SET_MI_DIMMINGON,
	DSI_CMD_SET_MI_DIMMINGOFF,
	DSI_CMD_SET_MI_ACL_OFF,
	DSI_CMD_SET_MI_ACL_L1,
	DSI_CMD_SET_MI_ACL_L2,
	DSI_CMD_SET_MI_ACL_L3,
	DSI_CMD_SET_DISP_LCD_HBM_L1_ON,
	DSI_CMD_SET_DISP_LCD_HBM_L2_ON,
	DSI_CMD_SET_DISP_LCD_HBM_L3_ON,
	DSI_CMD_SET_DISP_LCD_HBM_OFF,
	DSI_CMD_SET_MI_HBM_ON,
	DSI_CMD_SET_MI_HBM_OFF,
	DSI_CMD_SET_MI_HBM_FOD_ON,
	DSI_CMD_SET_MI_HBM_FOD_OFF,
	DSI_CMD_SET_MI_HBM_HDR_ON,
	DSI_CMD_SET_MI_HBM_HDR_OFF,
	DSI_CMD_SET_MI_HBM_FOD2NORM,
	DSI_CMD_SET_MI_NORMAL1,
	DSI_CMD_SET_MI_CRC_DCIP3,
	DSI_CMD_SET_MI_SRGB,
	DSI_CMD_SET_MI_DOZE_HBM,
	DSI_CMD_SET_MI_DOZE_LBM,
	DSI_CMD_SET_MI_CRC_OFF,
	DSI_CMD_SET_MI_ELVSS_DIMMING_OFF,
	DSI_CMD_SET_MI_FLAT_MODE_ON,
	DSI_CMD_SET_MI_FLAT_MODE_OFF,
	DSI_CMD_SET_MI_LEVEL2_KEY_ENABLE,
	DSI_CMD_SET_MI_GAMMA_OTP_READ_C8,
	DSI_CMD_SET_MI_GAMMA_OTP_READ_C9,
	DSI_CMD_SET_MI_GAMMA_OTP_READ_B3_PRE,
	DSI_CMD_SET_MI_GAMMA_OTP_READ_B3,
	DSI_CMD_SET_MI_LEVEL2_KEY_DISABLE,
	DSI_CMD_SET_MI_GAMMA_FLASH_READ_PRE,
	DSI_CMD_SET_MI_GAMMA_FLASH_READ_FB,
	DSI_CMD_SET_MI_WHITE_POINT_READ,
	DSI_CMD_SET_MI_ELVSS_DIMMING_OFFSET,
	DSI_CMD_SET_MI_ELVSS_DIMMING_READ,
	DSI_CMD_SET_MI_DC_ON,
	DSI_CMD_SET_MI_DC_OFF,
	DSI_CMD_SET_MI_VI_SETTING_LOW,
	DSI_CMD_SET_MI_VI_SETTING_HIGH,
	DSI_CMD_SET_MI_SWITCH_PAGE4,
	DSI_CMD_SET_MI_DC_READ,
	DSI_CMD_SET_MI_AOD_TO_DC_ON,
	DSI_CMD_SET_MI_DYNAMIC_ELVSS_ON,
	DSI_CMD_SET_MI_DYNAMIC_ELVSS_OFF,
	DSI_CMD_SET_MI_GAMMA_PRE_READ,
	DSI_CMD_SET_MI_GAMMA_READ_1ST_PRE,
	DSI_CMD_SET_MI_GAMMA_READ_2ND_PRE,
	DSI_CMD_SET_MI_GAMMA_READ_3RD_PRE,
	DSI_CMD_SET_MI_GAMMA_READ_4TH_PRE,
	DSI_CMD_SET_MI_GAMMA_READ_5TH_PRE,
	DSI_CMD_SET_MI_GAMMA_READ_6TH_PRE,
	DSI_CMD_SET_MI_GAMMA_READ_B7,
	DSI_CMD_SET_MI_GAMMA_B7,
	DSI_CMD_SET_MI_BLACK_SETTING,
	DSI_CMD_SET_MI_READ_LOCKDOWN_INFO,
	/* xiaomi add end */
	DSI_CMD_SET_MAX
};

/**
 * enum dsi_cmd_set_state - command set state
 * @DSI_CMD_SET_STATE_LP:   dsi low power mode
 * @DSI_CMD_SET_STATE_HS:   dsi high speed mode
 * @DSI_CMD_SET_STATE_MAX
 */
enum dsi_cmd_set_state {
	DSI_CMD_SET_STATE_LP = 0,
	DSI_CMD_SET_STATE_HS,
	DSI_CMD_SET_STATE_MAX
};

/**
 * enum dsi_clk_gate_type - Type of clock to be gated.
 * @PIXEL_CLK:  DSI pixel clock.
 * @BYTE_CLK:   DSI byte clock.
 * @DSI_PHY:    DSI PHY.
 * @DSI_CLK_ALL: All available DSI clocks
 * @DSI_CLK_NONE: None of the clocks should be gated
 */
enum dsi_clk_gate_type {
	PIXEL_CLK = 1,
	BYTE_CLK = 2,
	DSI_PHY = 4,
	DSI_CLK_ALL = (PIXEL_CLK | BYTE_CLK | DSI_PHY),
	DSI_CLK_NONE = 8,
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
 * struct dsi_cmd_desc - description of a dsi command
 * @msg:		dsi mipi msg packet
 * @last_command:   indicates whether the cmd is the last one to send
 * @post_wait_ms:   post wait duration
 */
struct dsi_cmd_desc {
	struct mipi_dsi_msg msg;
	bool last_command;
	u32  post_wait_ms;
};

/**
 * struct dsi_panel_cmd_set - command set of the panel
 * @type:      type of the command
 * @state:     state of the command
 * @count:     number of cmds
 * @ctrl_idx:  index of the dsi control
 * @cmds:      arry of cmds
 */
struct dsi_panel_cmd_set {
	enum dsi_cmd_set_type type;
	enum dsi_cmd_set_state state;
	u32 count;
	u32 ctrl_idx;
	struct dsi_cmd_desc *cmds;
};

/**
 * struct dsi_mode_info - video mode information dsi frame
 * @h_active:         Active width of one frame in pixels.
 * @h_back_porch:     Horizontal back porch in pixels.
 * @h_sync_width:     HSYNC width in pixels.
 * @h_front_porch:    Horizontal fron porch in pixels.
 * @h_skew:
 * @h_sync_polarity:  Polarity of HSYNC (false is active low).
 * @v_active:         Active height of one frame in lines.
 * @v_back_porch:     Vertical back porch in lines.
 * @v_sync_width:     VSYNC width in lines.
 * @v_front_porch:    Vertical front porch in lines.
 * @v_sync_polarity:  Polarity of VSYNC (false is active low).
 * @refresh_rate:     Refresh rate in Hz.
 * @clk_rate_hz:      DSI bit clock rate per lane in Hz.
 * @min_dsi_clk_hz:   Min DSI bit clock to transfer in vsync time.
 * @mdp_transfer_time_us:   Specifies the mdp transfer time for command mode
 *                    panels in microseconds.
 * @dsi_transfer_time_us:   Specifies dsi transfer time for command mode.
 * @dsc_enabled:      DSC compression enabled.
 * @dsc:              DSC compression configuration.
 * @roi_caps:         Panel ROI capabilities.
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
	u64 clk_rate_hz;
	u64 min_dsi_clk_hz;
	u32 mdp_transfer_time_us;
	u32 dsi_transfer_time_us;
	bool dsc_enabled;
	struct msm_display_dsc_info *dsc;
	struct msm_roi_caps roi_caps;
};

/**
 * struct dsi_split_link_config - Split Link Configuration
 * @split_link_enabled:  Split Link Enabled.
 * @num_sublinks:     Number of sublinks.
 * @lanes_per_sublink:   Number of lanes per sublink.
 */
struct dsi_split_link_config {
	bool split_link_enabled;
	u32 num_sublinks;
	u32 lanes_per_sublink;
};

/**
 * struct dsi_host_common_cfg - Host configuration common to video and cmd mode
 * @dst_format:          Destination pixel format.
 * @data_lanes:          Physical data lanes to be enabled.
 * @num_data_lanes:      Number of physical data lanes.
 * @bpp:                 Number of bits per pixel.
 * @en_crc_check:        Enable CRC checks.
 * @en_ecc_check:        Enable ECC checks.
 * @te_mode:             Source for TE signalling.
 * @mdp_cmd_trigger:     MDP frame update trigger for command mode.
 * @dma_cmd_trigger:     Command DMA trigger.
 * @cmd_trigger_stream:  Command mode stream to trigger.
 * @swap_mode:           DSI color swap mode.
 * @bit_swap_read:       Is red color bit swapped.
 * @bit_swap_green:      Is green color bit swapped.
 * @bit_swap_blue:       Is blue color bit swapped.
 * @t_clk_post:          Number of byte clock cycles that the transmitter shall
 *                       continue sending after last data lane has transitioned
 *                       to LP mode.
 * @t_clk_pre:           Number of byte clock cycles that the high spped clock
 *                       shall be driven prior to data lane transitions from LP
 *                       to HS mode.
 * @t_clk_pre_extend:    Increment t_clk_pre counter by 2 byteclk if set to
 *                       true, otherwise increment by 1 byteclk.
 * @ignore_rx_eot:       Ignore Rx EOT packets if set to true.
 * @append_tx_eot:       Append EOT packets for forward transmissions if set to
 *                       true.
 * @ext_bridge_mode:     External bridge is connected.
 * @force_hs_clk_lane:   Send continuous clock to the panel.
 * @phy_type:            DPHY/CPHY is enabled for this panel.
 * @dsi_split_link_config:  Split Link Configuration.
 * @byte_intf_clk_div:   Determines the factor for calculating byte intf clock.
 */
struct dsi_host_common_cfg {
	enum dsi_pixel_format dst_format;
	enum dsi_data_lanes data_lanes;
	u8 num_data_lanes;
	u8 bpp;
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
	bool t_clk_pre_extend;
	bool ignore_rx_eot;
	bool append_tx_eot;
	bool ext_bridge_mode;
	bool force_hs_clk_lane;
	enum dsi_phy_type phy_type;
	struct dsi_split_link_config split_link;
	u32 byte_intf_clk_div;
	u32 clk_strength;
};

/**
 * struct dsi_video_engine_cfg - DSI video engine configuration
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
 * @dma_sched_line:         Line number, after vactive end, at which command dma
 *			       needs to be triggered.
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
	u32 dma_sched_line;
};

/**
 * struct dsi_cmd_engine_cfg - DSI command engine configuration
 * @max_cmd_packets_interleave     Maximum number of command mode RGB packets to
 *                                 send with in one horizontal blanking period
 *                                 of the video mode frame.
 * @wr_mem_start:                  DCS command for write_memory_start.
 * @wr_mem_continue:               DCS command for write_memory_continue.
 * @insert_dcs_command:            Insert DCS command as first byte of payload
 *                                 of the pixel data.
 */
struct dsi_cmd_engine_cfg {
	u32 max_cmd_packets_interleave;
	u32 wr_mem_start;
	u32 wr_mem_continue;
	bool insert_dcs_command;
};

/**
 * struct dsi_host_config - DSI host configuration parameters.
 * @panel_mode:            Operation mode for panel (video or cmd mode).
 * @common_config:         Host configuration common to both Video and Cmd mode.
 * @video_engine:          Video engine configuration if panel is in video mode.
 * @cmd_engine:            Cmd engine configuration if panel is in cmd mode.
 * @esc_clk_rate_hz:      Esc clock frequency in Hz.
 * @bit_clk_rate_hz:       Bit clock frequency in Hz.
 * @bit_clk_rate_hz_override: DSI bit clk rate override from dt/sysfs.
 * @video_timing:          Video timing information of a frame.
 * @lane_map:              Mapping between logical and physical lanes.
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
	u64 bit_clk_rate_hz_override;
	struct dsi_mode_info video_timing;
	struct dsi_lane_map lane_map;
};

/**
 * struct dsi_display_mode_priv_info - private mode info that will be attached
 *                             with each drm mode
 * @cmd_sets:		  Command sets of the mode
 * @phy_timing_val:       Phy timing values
 * @phy_timing_len:       Phy timing array length
 * @panel_jitter:         Panel jitter for RSC backoff
 * @panel_prefill_lines:  Panel prefill lines for RSC
 * @mdp_transfer_time_us:   Specifies the mdp transfer time for command mode
 *                          panels in microseconds.
 * @dsi_transfer_time_us: Specifies the dsi transfer time for cmd panels.
 * @clk_rate_hz:          DSI bit clock per lane in hz.
 * @min_dsi_clk_hz:       Min dsi clk per lane to transfer frame in vsync time.
 * @topology:             Topology selected for the panel
 * @dsc:                  DSC compression info
 * @dsc_enabled:          DSC compression enabled
 * @roi_caps:		  Panel ROI capabilities
 */
struct dsi_display_mode_priv_info {
	struct dsi_panel_cmd_set cmd_sets[DSI_CMD_SET_MAX];

	u32 *phy_timing_val;
	u32 phy_timing_len;

	u32 panel_jitter_numer;
	u32 panel_jitter_denom;
	u32 panel_prefill_lines;
	u32 mdp_transfer_time_us;
	u32 dsi_transfer_time_us;
	u64 clk_rate_hz;
	u64 min_dsi_clk_hz;

	struct msm_display_topology topology;
	struct msm_display_dsc_info dsc;
	bool dsc_enabled;
	struct msm_roi_caps roi_caps;
};

/**
 * struct dsi_display_mode - specifies mode for dsi display
 * @timing:         Timing parameters for the panel.
 * @pixel_clk_khz:  Pixel clock in Khz.
 * @dsi_mode_flags: Flags to signal other drm components via private flags
 * @panel_mode:      Panel mode
 * @is_preferred:   Is mode preferred
 * @priv_info:      Mode private info
 */
struct dsi_display_mode {
	struct dsi_mode_info timing;
	u32 pixel_clk_khz;
	u32 dsi_mode_flags;
	enum dsi_op_mode panel_mode;
	bool is_preferred;
	struct dsi_display_mode_priv_info *priv_info;
};

/**
 * struct dsi_rect - dsi rectangle representation
 * Note: sde_rect is also using u16, this must be maintained for memcpy
 */
struct dsi_rect {
	u16 x;
	u16 y;
	u16 w;
	u16 h;
};

/**
 * dsi_rect_intersect - intersect two rectangles
 * @r1: first rectangle
 * @r2: scissor rectangle
 * @result: result rectangle, all 0's on no intersection found
 */
void dsi_rect_intersect(const struct dsi_rect *r1,
		const struct dsi_rect *r2,
		struct dsi_rect *result);

/**
 * dsi_rect_is_equal - compares two rects
 * @r1: rect value to compare
 * @r2: rect value to compare
 *
 * Returns true if the rects are same
 */
static inline bool dsi_rect_is_equal(struct dsi_rect *r1,
		struct dsi_rect *r2)
{
	return r1->x == r2->x && r1->y == r2->y && r1->w == r2->w &&
			r1->h == r2->h;
}

struct dsi_event_cb_info {
	uint32_t event_idx;
	void *event_usr_ptr;

	int (*event_cb)(void *event_usr_ptr,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3);
};

/**
 * enum dsi_error_status - various dsi errors
 * @DSI_FIFO_OVERFLOW:     DSI FIFO Overflow error
 * @DSI_FIFO_UNDERFLOW:    DSI FIFO Underflow error
 * @DSI_LP_Rx_TIMEOUT:     DSI LP/RX Timeout error
 * @DSI_PLL_UNLOCK_ERR:	   DSI PLL unlock error
 */
enum dsi_error_status {
	DSI_FIFO_OVERFLOW = 1,
	DSI_FIFO_UNDERFLOW,
	DSI_LP_Rx_TIMEOUT,
	DSI_PLL_UNLOCK_ERR,
	DSI_ERR_INTR_ALL,
};

/* structure containing the delays required for dynamic clk */
struct dsi_dyn_clk_delay {
	u32 pipe_delay;
	u32 pipe_delay2;
	u32 pll_delay;
};

/* dynamic refresh control bits */
enum dsi_dyn_clk_control_bits {
	DYN_REFRESH_INTF_SEL = 1,
	DYN_REFRESH_SYNC_MODE,
	DYN_REFRESH_SW_TRIGGER,
	DYN_REFRESH_SWI_CTRL,
};

/* convert dsi pixel format into bits per pixel */
static inline int dsi_pixel_format_to_bpp(enum dsi_pixel_format fmt)
{
	switch (fmt) {
	case DSI_PIXEL_FORMAT_RGB888:
	case DSI_PIXEL_FORMAT_MAX:
		return 24;
	case DSI_PIXEL_FORMAT_RGB666:
	case DSI_PIXEL_FORMAT_RGB666_LOOSE:
		return 18;
	case DSI_PIXEL_FORMAT_RGB565:
		return 16;
	case DSI_PIXEL_FORMAT_RGB111:
		return 3;
	case DSI_PIXEL_FORMAT_RGB332:
		return 8;
	case DSI_PIXEL_FORMAT_RGB444:
		return 12;
	}
	return 24;
}
#endif /* _DSI_DEFS_H_ */
