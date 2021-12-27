// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/iopoll.h>

#include "dsi_catalog.h"
#include "dsi_ctrl_hw.h"
#include "dsi_ctrl_reg.h"
#include "dsi_hw.h"
#include "dsi_panel.h"
#include "dsi_catalog.h"
#include "sde_dbg.h"

#define MMSS_MISC_CLAMP_REG_OFF           0x0014
#define DSI_CTRL_DYNAMIC_FORCE_ON         (0x23F|BIT(8)|BIT(9)|BIT(11)|BIT(21))
#define DSI_CTRL_CMD_MISR_ENABLE          BIT(28)
#define DSI_CTRL_VIDEO_MISR_ENABLE        BIT(16)
#define DSI_CTRL_DMA_LINK_SEL             (BIT(12)|BIT(13))
#define DSI_CTRL_MDP0_LINK_SEL            (BIT(20)|BIT(22))

/* Unsupported formats default to RGB888 */
static const u8 cmd_mode_format_map[DSI_PIXEL_FORMAT_MAX] = {
	0x6, 0x7, 0x8, 0x8, 0x0, 0x3, 0x4 };
static const u8 video_mode_format_map[DSI_PIXEL_FORMAT_MAX] = {
	0x0, 0x1, 0x2, 0x3, 0x3, 0x3, 0x3 };

/**
 * dsi_split_link_setup() - setup dsi split link configurations
 * @ctrl:             Pointer to the controller host hardware.
 * @cfg:              DSI host configuration that is common to both video and
 *                    command modes.
 */
static void dsi_split_link_setup(struct dsi_ctrl_hw *ctrl,
				struct dsi_host_common_cfg *cfg)
{
	u32 reg;

	if (!cfg->split_link.split_link_enabled)
		return;

	reg = DSI_R32(ctrl, DSI_SPLIT_LINK);

	/* DMA_LINK_SEL */
	reg &= ~(0x7 << 12);
	reg |= DSI_CTRL_DMA_LINK_SEL;

	/* MDP0_LINK_SEL */
	reg &= ~(0x7 << 20);
	reg |= DSI_CTRL_MDP0_LINK_SEL;

	/* EN */
	reg |= 0x1;

	/* DSI_SPLIT_LINK */
	DSI_W32(ctrl, DSI_SPLIT_LINK, reg);
	wmb(); /* make sure split link is asserted */
}

/**
 * dsi_setup_trigger_controls() - setup dsi trigger configurations
 * @ctrl:             Pointer to the controller host hardware.
 * @cfg:              DSI host configuration that is common to both video and
 *                    command modes.
 */
static void dsi_setup_trigger_controls(struct dsi_ctrl_hw *ctrl,
				       struct dsi_host_common_cfg *cfg)
{
	u32 reg = 0;
	const u8 trigger_map[DSI_TRIGGER_MAX] = {
		0x0, 0x2, 0x1, 0x4, 0x5, 0x6 };

	reg |= (cfg->te_mode == DSI_TE_ON_EXT_PIN) ? BIT(31) : 0;
	reg |= (trigger_map[cfg->dma_cmd_trigger] & 0x7);
	reg |= (trigger_map[cfg->mdp_cmd_trigger] & 0x7) << 4;
	DSI_W32(ctrl, DSI_TRIG_CTRL, reg);
}

/**
 * dsi_ctrl_hw_cmn_host_setup() - setup dsi host configuration
 * @ctrl:             Pointer to the controller host hardware.
 * @cfg:              DSI host configuration that is common to both video and
 *                    command modes.
 */
void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,
			       struct dsi_host_common_cfg *cfg)
{
	u32 reg_value = 0;

	dsi_setup_trigger_controls(ctrl, cfg);
	dsi_split_link_setup(ctrl, cfg);

	/* Setup T_CLK_PRE extend register */
	reg_value = DSI_R32(ctrl, DSI_TEST_PATTERN_GEN_VIDEO_ENABLE);
	if (cfg->t_clk_pre_extend)
		reg_value |= BIT(0);
	else
		reg_value &= ~BIT(0);
	DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_VIDEO_ENABLE, reg_value);

	/* Setup clocking timing controls */
	reg_value = ((cfg->t_clk_post & 0x3F) << 8);
	reg_value |= (cfg->t_clk_pre & 0x3F);
	DSI_W32(ctrl, DSI_CLKOUT_TIMING_CTRL, reg_value);

	/* EOT packet control */
	reg_value = cfg->append_tx_eot ? 1 : 0;
	reg_value |= (cfg->ignore_rx_eot ? (1 << 4) : 0);
	DSI_W32(ctrl, DSI_EOT_PACKET_CTRL, reg_value);

	/* Turn on dsi clocks */
	DSI_W32(ctrl, DSI_CLK_CTRL, 0x23F);

	/* Setup DSI control register */
	reg_value = DSI_R32(ctrl, DSI_CTRL);
	reg_value |= (cfg->en_crc_check ? BIT(24) : 0);
	reg_value |= (cfg->en_ecc_check ? BIT(20) : 0);
	reg_value |= BIT(8); /* Clock lane */
	reg_value |= ((cfg->data_lanes & DSI_DATA_LANE_3) ? BIT(7) : 0);
	reg_value |= ((cfg->data_lanes & DSI_DATA_LANE_2) ? BIT(6) : 0);
	reg_value |= ((cfg->data_lanes & DSI_DATA_LANE_1) ? BIT(5) : 0);
	reg_value |= ((cfg->data_lanes & DSI_DATA_LANE_0) ? BIT(4) : 0);

	DSI_W32(ctrl, DSI_CTRL, reg_value);

	if (cfg->phy_type == DSI_PHY_TYPE_CPHY)
		DSI_W32(ctrl, DSI_CPHY_MODE_CTRL, BIT(0));

	if (ctrl->phy_isolation_enabled)
		DSI_W32(ctrl, DSI_DEBUG_CTRL, BIT(28));
	DSI_CTRL_HW_DBG(ctrl, "Host configuration complete\n");
}

/**
 * phy_sw_reset() - perform a soft reset on the PHY.
 * @ctrl:        Pointer to the controller host hardware.
 */
void dsi_ctrl_hw_cmn_phy_sw_reset(struct dsi_ctrl_hw *ctrl)
{
	DSI_W32(ctrl, DSI_PHY_SW_RESET, BIT(24)|BIT(0));
	wmb(); /* make sure reset is asserted */
	udelay(1000);
	DSI_W32(ctrl, DSI_PHY_SW_RESET, 0x0);
	wmb(); /* ensure reset is cleared before waiting */
	udelay(100);

	DSI_CTRL_HW_DBG(ctrl, "phy sw reset done\n");
}

/**
 * soft_reset() - perform a soft reset on DSI controller
 * @ctrl:          Pointer to the controller host hardware.
 *
 * The video, command and controller engines will be disabled before the
 * reset is triggered and re-enabled after the reset is complete.
 *
 * If the reset is done while MDP timing engine is turned on, the video
 * enigne should be re-enabled only during the vertical blanking time.
 */
void dsi_ctrl_hw_cmn_soft_reset(struct dsi_ctrl_hw *ctrl)
{
	u32 reg = 0;
	u32 reg_ctrl = 0;

	/* Clear DSI_EN, VIDEO_MODE_EN, CMD_MODE_EN */
	reg_ctrl = DSI_R32(ctrl, DSI_CTRL);
	DSI_W32(ctrl, DSI_CTRL, reg_ctrl & ~0x7);
	wmb(); /* wait controller to be disabled before reset */

	/* Force enable PCLK, BYTECLK, AHBM_HCLK */
	reg = DSI_R32(ctrl, DSI_CLK_CTRL);
	DSI_W32(ctrl, DSI_CLK_CTRL, reg | DSI_CTRL_DYNAMIC_FORCE_ON);
	wmb(); /* wait for clocks to be enabled */

	/* Trigger soft reset */
	DSI_W32(ctrl, DSI_SOFT_RESET, 0x1);
	wmb(); /* wait for reset to assert before waiting */
	udelay(1);
	DSI_W32(ctrl, DSI_SOFT_RESET, 0x0);
	wmb(); /* ensure reset is cleared */

	/* Disable force clock on */
	DSI_W32(ctrl, DSI_CLK_CTRL, reg);
	wmb(); /* make sure clocks are restored */

	/* Re-enable DSI controller */
	DSI_W32(ctrl, DSI_CTRL, reg_ctrl);
	wmb(); /* make sure DSI controller is enabled again */
	DSI_CTRL_HW_DBG(ctrl, "ctrl soft reset done\n");
	SDE_EVT32(ctrl->index);
}

/**
 * setup_misr() - Setup frame MISR
 * @ctrl:	  Pointer to the controller host hardware.
 * @panel_mode:   CMD or VIDEO mode indicator
 * @enable:	  Enable/disable MISR.
 * @frame_count:  Number of frames to accumulate MISR.
 */
void dsi_ctrl_hw_cmn_setup_misr(struct dsi_ctrl_hw *ctrl,
			enum dsi_op_mode panel_mode,
			bool enable,
			u32 frame_count)
{
	u32 addr;
	u32 config = 0;

	if (panel_mode == DSI_OP_CMD_MODE) {
		addr = DSI_MISR_CMD_CTRL;
		if (enable)
			config = DSI_CTRL_CMD_MISR_ENABLE;
	} else {
		addr = DSI_MISR_VIDEO_CTRL;
		if (enable)
			config = DSI_CTRL_VIDEO_MISR_ENABLE;
		if (frame_count > 255)
			frame_count = 255;
		config |= frame_count << 8;
	}

	DSI_CTRL_HW_DBG(ctrl, "MISR ctrl: 0x%x\n", config);
	DSI_W32(ctrl, addr, config);
	wmb(); /* make sure MISR is configured */
}

/**
 * collect_misr() - Read frame MISR
 * @ctrl:	  Pointer to the controller host hardware.
 * @panel_mode:   CMD or VIDEO mode indicator
 */
u32 dsi_ctrl_hw_cmn_collect_misr(struct dsi_ctrl_hw *ctrl,
			enum dsi_op_mode panel_mode)
{
	u32 addr;
	u32 enabled;
	u32 misr = 0;

	if (panel_mode == DSI_OP_CMD_MODE) {
		addr = DSI_MISR_CMD_MDP0_32BIT;
		enabled = DSI_R32(ctrl, DSI_MISR_CMD_CTRL) &
				DSI_CTRL_CMD_MISR_ENABLE;
	} else {
		addr = DSI_MISR_VIDEO_32BIT;
		enabled = DSI_R32(ctrl, DSI_MISR_VIDEO_CTRL) &
				DSI_CTRL_VIDEO_MISR_ENABLE;
	}

	if (enabled)
		misr = DSI_R32(ctrl, addr);

	DSI_CTRL_HW_DBG(ctrl, "MISR enabled %x value: 0x%x\n", enabled, misr);
	return misr;
}

/**
 * set_timing_db() - enable/disable Timing DB register
 * @ctrl:          Pointer to controller host hardware.
 * @enable:        Enable/Disable flag.
 *
 * Enable or Disabe the Timing DB register.
 */
void dsi_ctrl_hw_cmn_set_timing_db(struct dsi_ctrl_hw *ctrl,
				     bool enable)
{
	if (enable)
		DSI_W32(ctrl, DSI_DSI_TIMING_DB_MODE, 0x1);
	else
		DSI_W32(ctrl, DSI_DSI_TIMING_DB_MODE, 0x0);

	wmb(); /* make sure timing db registers are set */
	DSI_CTRL_HW_DBG(ctrl, "ctrl timing DB set:%d\n", enable);
	SDE_EVT32(ctrl->index, enable);
}

/**
 * set_video_timing() - set up the timing for video frame
 * @ctrl:          Pointer to controller host hardware.
 * @mode:          Video mode information.
 *
 * Set up the video timing parameters for the DSI video mode operation.
 */
void dsi_ctrl_hw_cmn_set_video_timing(struct dsi_ctrl_hw *ctrl,
				     struct dsi_mode_info *mode)
{
	u32 reg = 0;
	u32 hs_start = 0;
	u32 hs_end, active_h_start, active_h_end, h_total, width = 0;
	u32 vs_start = 0, vs_end = 0;
	u32 vpos_start = 0, vpos_end, active_v_start, active_v_end, v_total;

	if (mode->dsc_enabled && mode->dsc) {
		width = mode->dsc->pclk_per_line;
		reg = mode->dsc->bytes_per_pkt << 16;
		reg |= (0x0b << 8);    /* dtype of compressed image */
		/*
		 * pkt_per_line:
		 * 0 == 1 pkt
		 * 1 == 2 pkt
		 * 2 == 4 pkt
		 * 3 pkt is not support
		 */
		if (mode->dsc->pkt_per_line == 4)
			reg |= (mode->dsc->pkt_per_line - 2) << 6;
		else
			reg |= (mode->dsc->pkt_per_line - 1) << 6;
		reg |= mode->dsc->eol_byte_num << 4;
		reg |= 1;
		DSI_W32(ctrl, DSI_VIDEO_COMPRESSION_MODE_CTRL, reg);
	} else {
		width = mode->h_active;
	}

	hs_end = mode->h_sync_width;
	active_h_start = mode->h_sync_width + mode->h_back_porch;
	active_h_end = active_h_start + width;
	h_total = (mode->h_sync_width + mode->h_back_porch + width +
		   mode->h_front_porch) - 1;

	vpos_end = mode->v_sync_width;
	active_v_start = mode->v_sync_width + mode->v_back_porch;
	active_v_end = active_v_start + mode->v_active;
	v_total = (mode->v_sync_width + mode->v_back_porch + mode->v_active +
		   mode->v_front_porch) - 1;

	reg = ((active_h_end & 0xFFFF) << 16) | (active_h_start & 0xFFFF);
	DSI_W32(ctrl, DSI_VIDEO_MODE_ACTIVE_H, reg);

	reg = ((active_v_end & 0xFFFF) << 16) | (active_v_start & 0xFFFF);
	DSI_W32(ctrl, DSI_VIDEO_MODE_ACTIVE_V, reg);

	reg = ((v_total & 0xFFFF) << 16) | (h_total & 0xFFFF);
	DSI_W32(ctrl, DSI_VIDEO_MODE_TOTAL, reg);

	reg = ((hs_end & 0xFFFF) << 16) | (hs_start & 0xFFFF);
	DSI_W32(ctrl, DSI_VIDEO_MODE_HSYNC, reg);

	reg = ((vs_end & 0xFFFF) << 16) | (vs_start & 0xFFFF);
	DSI_W32(ctrl, DSI_VIDEO_MODE_VSYNC, reg);

	reg = ((vpos_end & 0xFFFF) << 16) | (vpos_start & 0xFFFF);
	DSI_W32(ctrl, DSI_VIDEO_MODE_VSYNC_VPOS, reg);

	/* TODO: HS TIMER value? */
	DSI_W32(ctrl, DSI_HS_TIMER_CTRL, 0x3FD08);
	DSI_W32(ctrl, DSI_MISR_VIDEO_CTRL, 0x10100);
	DSI_W32(ctrl, DSI_DSI_TIMING_FLUSH, 0x1);
	DSI_CTRL_HW_DBG(ctrl, "ctrl video parameters updated\n");
	SDE_EVT32(v_total, h_total);
}

/**
 * setup_cmd_stream() - set up parameters for command pixel streams
 * @ctrl:              Pointer to controller host hardware.
 * @mode:              Pointer to mode information.
 * @h_stride:          Horizontal stride in bytes.
 * @vc_id:             stream_id
 *
 * Setup parameters for command mode pixel stream size.
 */
void dsi_ctrl_hw_cmn_setup_cmd_stream(struct dsi_ctrl_hw *ctrl,
				     struct dsi_mode_info *mode,
				     u32 h_stride,
				     u32 vc_id,
				     struct dsi_rect *roi)
{
	u32 width_final, stride_final;
	u32 height_final;
	u32 stream_total = 0, stream_ctrl = 0;
	u32 reg_ctrl = 0, reg_ctrl2 = 0, data = 0;

	if (roi && (!roi->w || !roi->h))
		return;

	if (mode->dsc_enabled && mode->dsc) {
		u32 reg = 0;
		u32 offset = 0;
		int pic_width, this_frame_slices, intf_ip_w;
		struct msm_display_dsc_info dsc;

		memcpy(&dsc, mode->dsc, sizeof(dsc));
		pic_width = roi ? roi->w : mode->h_active;
		this_frame_slices = pic_width / dsc.slice_width;
		intf_ip_w = this_frame_slices * dsc.slice_width;
		dsi_dsc_pclk_param_calc(&dsc, intf_ip_w);

		if (vc_id != 0)
			offset = 16;
		reg_ctrl = DSI_R32(ctrl, DSI_COMMAND_COMPRESSION_MODE_CTRL);
		reg_ctrl2 = DSI_R32(ctrl, DSI_COMMAND_COMPRESSION_MODE_CTRL2);
		width_final = dsc.pclk_per_line;
		stride_final = dsc.bytes_per_pkt;
		height_final = roi ? roi->h : mode->v_active;

		reg = 0x39 << 8;
		/*
		 * pkt_per_line:
		 * 0 == 1 pkt
		 * 1 == 2 pkt
		 * 2 == 4 pkt
		 * 3 pkt is not support
		 */
		if (dsc.pkt_per_line == 4)
			reg |= (dsc.pkt_per_line - 2) << 6;
		else
			reg |= (dsc.pkt_per_line - 1) << 6;
		reg |= dsc.eol_byte_num << 4;
		reg |= 1;

		reg_ctrl &= ~(0xFFFF << offset);
		reg_ctrl |= (reg << offset);
		reg_ctrl2 &= ~(0xFFFF << offset);
		reg_ctrl2 |= (dsc.bytes_in_slice << offset);

		DSI_CTRL_HW_DBG(ctrl, "reg_ctrl 0x%x reg_ctrl2 0x%x\n",
				reg_ctrl, reg_ctrl2);
	} else if (roi) {
		width_final = roi->w;
		stride_final = roi->w * 3;
		height_final = roi->h;
	} else {
		width_final = mode->h_active;
		stride_final = h_stride;
		height_final = mode->v_active;
	}

	/* HS Timer value */
	DSI_W32(ctrl, DSI_HS_TIMER_CTRL, 0x3FD08);

	stream_ctrl = (stride_final + 1) << 16;
	stream_ctrl |= (vc_id & 0x3) << 8;
	stream_ctrl |= 0x39; /* packet data type */

	DSI_W32(ctrl, DSI_COMMAND_COMPRESSION_MODE_CTRL, reg_ctrl);
	DSI_W32(ctrl, DSI_COMMAND_COMPRESSION_MODE_CTRL2, reg_ctrl2);

	DSI_W32(ctrl, DSI_COMMAND_MODE_MDP_STREAM0_CTRL, stream_ctrl);
	DSI_W32(ctrl, DSI_COMMAND_MODE_MDP_STREAM1_CTRL, stream_ctrl);

	stream_total = (height_final << 16) | width_final;
	DSI_W32(ctrl, DSI_COMMAND_MODE_MDP_STREAM0_TOTAL, stream_total);
	DSI_W32(ctrl, DSI_COMMAND_MODE_MDP_STREAM1_TOTAL, stream_total);

	if (ctrl->null_insertion_enabled) {
		/* enable null packet insertion */
		data = (vc_id << 1);
		data |= 0 << 16;
		data |= 0x1;
		DSI_W32(ctrl, DSI_COMMAND_MODE_NULL_INSERTION_CTRL, data);
	}

	DSI_CTRL_HW_DBG(ctrl, "stream_ctrl 0x%x stream_total 0x%x\n",
			stream_ctrl, stream_total);
}

/**
 * setup_avr() - set the AVR_SUPPORT_ENABLE bit in DSI_VIDEO_MODE_CTRL
 * @ctrl:          Pointer to controller host hardware.
 * @enable:        Controls whether this bit is set or cleared
 *
 * Set or clear the AVR_SUPPORT_ENABLE bit in DSI_VIDEO_MODE_CTRL.
 */
void dsi_ctrl_hw_cmn_setup_avr(struct dsi_ctrl_hw *ctrl, bool enable)
{
	u32 reg = DSI_R32(ctrl, DSI_VIDEO_MODE_CTRL);

	if (enable)
		reg |= BIT(29);
	else
		reg &= ~BIT(29);

	DSI_W32(ctrl, DSI_VIDEO_MODE_CTRL, reg);
	DSI_CTRL_HW_DBG(ctrl, "AVR %s\n", enable ? "enabled" : "disabled");
}

/**
 * video_engine_setup() - Setup dsi host controller for video mode
 * @ctrl:          Pointer to controller host hardware.
 * @common_cfg:    Common configuration parameters.
 * @cfg:           Video mode configuration.
 *
 * Set up DSI video engine with a specific configuration. Controller and
 * video engine are not enabled as part of this function.
 */
void dsi_ctrl_hw_cmn_video_engine_setup(struct dsi_ctrl_hw *ctrl,
				       struct dsi_host_common_cfg *common_cfg,
				       struct dsi_video_engine_cfg *cfg)
{
	u32 reg = 0;

	reg |= (cfg->last_line_interleave_en ? BIT(31) : 0);
	reg |= (cfg->pulse_mode_hsa_he ? BIT(28) : 0);
	reg |= (cfg->hfp_lp11_en ? BIT(24) : 0);
	reg |= (cfg->hbp_lp11_en ? BIT(20) : 0);
	reg |= (cfg->hsa_lp11_en ? BIT(16) : 0);
	reg |= (cfg->eof_bllp_lp11_en ? BIT(15) : 0);
	reg |= (cfg->bllp_lp11_en ? BIT(12) : 0);
	reg |= (cfg->traffic_mode & 0x3) << 8;
	reg |= (cfg->vc_id & 0x3);
	reg |= (video_mode_format_map[common_cfg->dst_format] & 0x3) << 4;
	DSI_W32(ctrl, DSI_VIDEO_MODE_CTRL, reg);

	reg = (common_cfg->swap_mode & 0x7) << 12;
	reg |= (common_cfg->bit_swap_red ? BIT(0) : 0);
	reg |= (common_cfg->bit_swap_green ? BIT(4) : 0);
	reg |= (common_cfg->bit_swap_blue ? BIT(8) : 0);
	DSI_W32(ctrl, DSI_VIDEO_MODE_DATA_CTRL, reg);
	/* Disable Timing double buffering */
	DSI_W32(ctrl, DSI_DSI_TIMING_DB_MODE, 0x0);

	DSI_CTRL_HW_DBG(ctrl, "Video engine setup done\n");
}

void dsi_ctrl_hw_cmn_debug_bus(struct dsi_ctrl_hw *ctrl, u32 *entries, u32 size)
{
	u32 reg = 0, i = 0;

	for (i = 0; i < size; i++) {
		DSI_W32(ctrl, DSI_DEBUG_BUS_CTL, entries[i]);
		/* make sure that debug test point is enabled */
		wmb();
		reg = DSI_R32(ctrl, DSI_DEBUG_BUS_STATUS);
		DSI_CTRL_HW_INFO(ctrl, "debug bus ctrl: 0x%x status:0x%x\n",
				entries[i], reg);
	}
}

/**
 * cmd_engine_setup() - setup dsi host controller for command mode
 * @ctrl:          Pointer to the controller host hardware.
 * @common_cfg:    Common configuration parameters.
 * @cfg:           Command mode configuration.
 *
 * Setup DSI CMD engine with a specific configuration. Controller and
 * command engine are not enabled as part of this function.
 */
void dsi_ctrl_hw_cmn_cmd_engine_setup(struct dsi_ctrl_hw *ctrl,
				     struct dsi_host_common_cfg *common_cfg,
				     struct dsi_cmd_engine_cfg *cfg)
{
	u32 reg = 0;

	reg = (cfg->max_cmd_packets_interleave & 0xF) << 20;
	reg |= (common_cfg->bit_swap_red ? BIT(4) : 0);
	reg |= (common_cfg->bit_swap_green ? BIT(8) : 0);
	reg |= (common_cfg->bit_swap_blue ? BIT(12) : 0);
	reg |= cmd_mode_format_map[common_cfg->dst_format];
	DSI_W32(ctrl, DSI_COMMAND_MODE_MDP_CTRL, reg);

	reg = DSI_R32(ctrl, DSI_COMMAND_MODE_MDP_CTRL2);
	reg |= BIT(16);
	DSI_W32(ctrl, DSI_COMMAND_MODE_MDP_CTRL2, reg);

	reg = cfg->wr_mem_start & 0xFF;
	reg |= (cfg->wr_mem_continue & 0xFF) << 8;
	reg |= (cfg->insert_dcs_command ? BIT(16) : 0);
	DSI_W32(ctrl, DSI_COMMAND_MODE_MDP_DCS_CMD_CTRL, reg);

	DSI_CTRL_HW_DBG(ctrl, "Cmd engine setup done\n");
}

/**
 * video_engine_en() - enable DSI video engine
 * @ctrl:          Pointer to controller host hardware.
 * @on:            Enable/disabel video engine.
 */
void dsi_ctrl_hw_cmn_video_engine_en(struct dsi_ctrl_hw *ctrl, bool on)
{
	u32 reg = 0;

	/* Set/Clear VIDEO_MODE_EN bit */
	reg = DSI_R32(ctrl, DSI_CTRL);
	if (on)
		reg |= BIT(1);
	else
		reg &= ~BIT(1);

	DSI_W32(ctrl, DSI_CTRL, reg);

	DSI_CTRL_HW_DBG(ctrl, "Video engine = %d\n", on);
}

/**
 * ctrl_en() - enable DSI controller engine
 * @ctrl:          Pointer to the controller host hardware.
 * @on:            turn on/off the DSI controller engine.
 */
void dsi_ctrl_hw_cmn_ctrl_en(struct dsi_ctrl_hw *ctrl, bool on)
{
	u32 reg = 0;
	u32 clk_ctrl;

	clk_ctrl = DSI_R32(ctrl, DSI_CLK_CTRL);
	DSI_W32(ctrl, DSI_CLK_CTRL, clk_ctrl | DSI_CTRL_DYNAMIC_FORCE_ON);
	wmb(); /* wait for clocks to enable */

	/* Set/Clear DSI_EN bit */
	reg = DSI_R32(ctrl, DSI_CTRL);
	if (on)
		reg |= BIT(0);
	else
		reg &= ~BIT(0);

	DSI_W32(ctrl, DSI_CTRL, reg);
	wmb(); /* wait for DSI_EN update before disabling clocks */

	DSI_W32(ctrl, DSI_CLK_CTRL, clk_ctrl);
	wmb(); /* make sure clocks are restored */

	DSI_CTRL_HW_DBG(ctrl, "Controller engine = %d\n", on);
}

/**
 * cmd_engine_en() - enable DSI controller command engine
 * @ctrl:          Pointer to the controller host hardware.
 * @on:            Turn on/off the DSI command engine.
 */
void dsi_ctrl_hw_cmn_cmd_engine_en(struct dsi_ctrl_hw *ctrl, bool on)
{
	u32 reg = 0;

	/* Set/Clear CMD_MODE_EN bit */
	reg = DSI_R32(ctrl, DSI_CTRL);
	if (on)
		reg |= BIT(2);
	else
		reg &= ~BIT(2);

	DSI_W32(ctrl, DSI_CTRL, reg);

	DSI_CTRL_HW_DBG(ctrl, "command engine = %d\n", on);
}

/**
 * kickoff_command() - transmits commands stored in memory
 * @ctrl:          Pointer to the controller host hardware.
 * @cmd:           Command information.
 * @flags:         Modifiers for command transmission.
 *
 * The controller hardware is programmed with address and size of the
 * command buffer. The transmission is kicked off if
 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag is not set. If this flag is
 * set, caller should make a separate call to trigger_command_dma() to
 * transmit the command.
 */
void dsi_ctrl_hw_cmn_kickoff_command(struct dsi_ctrl_hw *ctrl,
				    struct dsi_ctrl_cmd_dma_info *cmd,
				    u32 flags)
{
	u32 reg = 0;

	/*Set BROADCAST_EN and EMBEDDED_MODE */
	reg = DSI_R32(ctrl, DSI_COMMAND_MODE_DMA_CTRL);
	if (cmd->en_broadcast)
		reg |= BIT(31);
	else
		reg &= ~BIT(31);

	if (cmd->is_master)
		reg |= BIT(30);
	else
		reg &= ~BIT(30);

	if (cmd->use_lpm)
		reg |= BIT(26);
	else
		reg &= ~BIT(26);

	reg |= BIT(28);/* Select embedded mode */
	reg &= ~BIT(24);/* packet type */
	reg &= ~BIT(29);/* WC_SEL to 0 */
	DSI_W32(ctrl, DSI_COMMAND_MODE_DMA_CTRL, reg);

	reg = DSI_R32(ctrl, DSI_DMA_FIFO_CTRL);
	reg |= BIT(20);/* Disable write watermark*/
	reg |= BIT(16);/* Disable read watermark */

	DSI_W32(ctrl, DSI_DMA_FIFO_CTRL, reg);
	DSI_W32(ctrl, DSI_DMA_CMD_OFFSET, cmd->offset);
	DSI_W32(ctrl, DSI_DMA_CMD_LENGTH, (cmd->length & 0xFFFFFF));

	/* wait for writes to complete before kick off */
	wmb();

	if (!(flags & DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER))
		DSI_W32(ctrl, DSI_CMD_MODE_DMA_SW_TRIGGER, 0x1);
}

/**
 * kickoff_fifo_command() - transmits a command using FIFO in dsi
 *                          hardware.
 * @ctrl:          Pointer to the controller host hardware.
 * @cmd:           Command information.
 * @flags:         Modifiers for command transmission.
 *
 * The controller hardware FIFO is programmed with command header and
 * payload. The transmission is kicked off if
 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag is not set. If this flag is
 * set, caller should make a separate call to trigger_command_dma() to
 * transmit the command.
 */
void dsi_ctrl_hw_cmn_kickoff_fifo_command(struct dsi_ctrl_hw *ctrl,
					 struct dsi_ctrl_cmd_dma_fifo_info *cmd,
					 u32 flags)
{
	u32 reg = 0, i = 0;
	u32 *ptr = cmd->command;
	/*
	 * Set CMD_DMA_TPG_EN, TPG_DMA_FIFO_MODE and
	 * CMD_DMA_PATTERN_SEL = custom pattern stored in TPG DMA FIFO
	 */
	reg = (BIT(1) | BIT(2) | (0x3 << 16));
	DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CTRL, reg);

	/*
	 * Program the FIFO with command buffer. Hardware requires an extra
	 * DWORD (set to zero) if the length of command buffer is odd DWORDS.
	 */
	for (i = 0; i < cmd->size; i += 4) {
		DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CMD_DMA_INIT_VAL, *ptr);
		ptr++;
	}

	if ((cmd->size / 4) & 0x1)
		DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CMD_DMA_INIT_VAL, 0);

	/*Set BROADCAST_EN and EMBEDDED_MODE */
	reg = DSI_R32(ctrl, DSI_COMMAND_MODE_DMA_CTRL);
	if (cmd->en_broadcast)
		reg |= BIT(31);
	else
		reg &= ~BIT(31);

	if (cmd->is_master)
		reg |= BIT(30);
	else
		reg &= ~BIT(30);

	if (cmd->use_lpm)
		reg |= BIT(26);
	else
		reg &= ~BIT(26);

	reg |= BIT(28);

	DSI_W32(ctrl, DSI_COMMAND_MODE_DMA_CTRL, reg);

	DSI_W32(ctrl, DSI_DMA_CMD_LENGTH, (cmd->size & 0xFFFFFFFF));
	/* Finish writes before command trigger */
	wmb();

	if (!(flags & DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER))
		DSI_W32(ctrl, DSI_CMD_MODE_DMA_SW_TRIGGER, 0x1);

	DSI_CTRL_HW_DBG(ctrl, "size=%d, trigger = %d\n", cmd->size,
		 (flags & DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER) ? false : true);
}

void dsi_ctrl_hw_cmn_reset_cmd_fifo(struct dsi_ctrl_hw *ctrl)
{
	/* disable cmd dma tpg */
	DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CTRL, 0x0);

	DSI_W32(ctrl, DSI_TPG_DMA_FIFO_RESET, 0x1);
	udelay(1);
	DSI_W32(ctrl, DSI_TPG_DMA_FIFO_RESET, 0x0);
}

/**
 * trigger_command_dma() - trigger transmission of command buffer.
 * @ctrl:          Pointer to the controller host hardware.
 *
 * This trigger can be only used if there was a prior call to
 * kickoff_command() of kickoff_fifo_command() with
 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag.
 */
void dsi_ctrl_hw_cmn_trigger_command_dma(struct dsi_ctrl_hw *ctrl)
{
	DSI_W32(ctrl, DSI_CMD_MODE_DMA_SW_TRIGGER, 0x1);
}

/**
 * clear_rdbk_reg() - clear previously read panel data.
 * @ctrl:          Pointer to the controller host hardware.
 *
 * This function is called before sending DSI Rx command to
 * panel in order to clear if any stale data remaining from
 * previous read operation.
 */
void dsi_ctrl_hw_cmn_clear_rdbk_reg(struct dsi_ctrl_hw *ctrl)
{
	DSI_W32(ctrl, DSI_RDBK_DATA_CTRL, 0x1);
	wmb(); /* ensure read back register is reset */
	DSI_W32(ctrl, DSI_RDBK_DATA_CTRL, 0x0);
	wmb(); /* ensure read back register is cleared */
}

/**
 * get_cmd_read_data() - get data read from the peripheral
 * @ctrl:           Pointer to the controller host hardware.
 * @rd_buf:         Buffer where data will be read into.
 * @total_read_len: Number of bytes to read.
 *
 * return: number of bytes read.
 */
u32 dsi_ctrl_hw_cmn_get_cmd_read_data(struct dsi_ctrl_hw *ctrl,
				     u8 *rd_buf,
				     u32 read_offset,
				     u32 rx_byte,
				     u32 pkt_size,
				     u32 *hw_read_cnt)
{
	u32 *lp, *temp, data;
	int i, j = 0, cnt, off;
	u32 read_cnt;
	u32 repeated_bytes = 0;
	u8 reg[16] = {0};
	bool ack_err = false;

	lp = (u32 *)rd_buf;
	temp = (u32 *)reg;
	cnt = (rx_byte + 3) >> 2;

	if (cnt > 4)
		cnt = 4;

	read_cnt = (DSI_R32(ctrl, DSI_RDBK_DATA_CTRL) >> 16);
	ack_err = (rx_byte == 4) ? (read_cnt == 8) :
			((read_cnt - 4) == (pkt_size + 6));

	if (ack_err)
		read_cnt -= 4;
	if (!read_cnt) {
		DSI_CTRL_HW_ERR(ctrl, "Panel detected error, no data read\n");
		return 0;
	}

	if (read_cnt > 16) {
		int bytes_shifted, data_lost = 0, rem_header = 0;

		bytes_shifted = read_cnt - rx_byte;
		if (bytes_shifted >= 4)
			data_lost = bytes_shifted - 4; /* remove DCS header */
		else
			rem_header = 4 - bytes_shifted; /* remaining header */

		repeated_bytes = (read_offset - 4) - data_lost + rem_header;
	}

	off = DSI_RDBK_DATA0;
	off += ((cnt - 1) * 4);

	for (i = 0; i < cnt; i++) {
		data = DSI_R32(ctrl, off);
		if (!repeated_bytes)
			*lp++ = ntohl(data);
		else
			*temp++ = ntohl(data);
		off -= 4;
	}

	if (repeated_bytes) {
		for (i = repeated_bytes; i < 16; i++)
			rd_buf[j++] = reg[i];
	}

	*hw_read_cnt = read_cnt;
	DSI_CTRL_HW_DBG(ctrl, "Read %d bytes\n", rx_byte);
	return rx_byte;
}

/**
 * get_interrupt_status() - returns the interrupt status
 * @ctrl:          Pointer to the controller host hardware.
 *
 * Returns the ORed list of interrupts(enum dsi_status_int_type) that
 * are active. This list does not include any error interrupts. Caller
 * should call get_error_status for error interrupts.
 *
 * Return: List of active interrupts.
 */
u32 dsi_ctrl_hw_cmn_get_interrupt_status(struct dsi_ctrl_hw *ctrl)
{
	u32 reg = 0;
	u32 ints = 0;

	reg = DSI_R32(ctrl, DSI_INT_CTRL);

	if (reg & BIT(0))
		ints |= DSI_CMD_MODE_DMA_DONE;
	if (reg & BIT(8))
		ints |= DSI_CMD_FRAME_DONE;
	if (reg & BIT(10))
		ints |= DSI_CMD_STREAM0_FRAME_DONE;
	if (reg & BIT(12))
		ints |= DSI_CMD_STREAM1_FRAME_DONE;
	if (reg & BIT(14))
		ints |= DSI_CMD_STREAM2_FRAME_DONE;
	if (reg & BIT(16))
		ints |= DSI_VIDEO_MODE_FRAME_DONE;
	if (reg & BIT(20))
		ints |= DSI_BTA_DONE;
	if (reg & BIT(28))
		ints |= DSI_DYN_REFRESH_DONE;
	if (reg & BIT(30))
		ints |= DSI_DESKEW_DONE;
	if (reg & BIT(24))
		ints |= DSI_ERROR;

	DSI_CTRL_HW_DBG(ctrl, "Interrupt status = 0x%x, INT_CTRL=0x%x\n",
		 ints, reg);
	return ints;
}

/**
 * clear_interrupt_status() - clears the specified interrupts
 * @ctrl:          Pointer to the controller host hardware.
 * @ints:          List of interrupts to be cleared.
 */
void dsi_ctrl_hw_cmn_clear_interrupt_status(struct dsi_ctrl_hw *ctrl, u32 ints)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, DSI_INT_CTRL);

	if (ints & DSI_CMD_MODE_DMA_DONE)
		reg |= BIT(0);
	if (ints & DSI_CMD_FRAME_DONE)
		reg |= BIT(8);
	if (ints & DSI_CMD_STREAM0_FRAME_DONE)
		reg |= BIT(10);
	if (ints & DSI_CMD_STREAM1_FRAME_DONE)
		reg |= BIT(12);
	if (ints & DSI_CMD_STREAM2_FRAME_DONE)
		reg |= BIT(14);
	if (ints & DSI_VIDEO_MODE_FRAME_DONE)
		reg |= BIT(16);
	if (ints & DSI_BTA_DONE)
		reg |= BIT(20);
	if (ints & DSI_DYN_REFRESH_DONE)
		reg |= BIT(28);
	if (ints & DSI_DESKEW_DONE)
		reg |= BIT(30);

	/*
	 * Do not clear error status.
	 * It will be cleared as part of
	 * error handler function.
	 */
	reg &= ~BIT(24);
	DSI_W32(ctrl, DSI_INT_CTRL, reg);

	DSI_CTRL_HW_DBG(ctrl, "Clear interrupts, ints = 0x%x, INT_CTRL=0x%x\n",
		 ints, reg);
}

/**
 * enable_status_interrupts() - enable the specified interrupts
 * @ctrl:          Pointer to the controller host hardware.
 * @ints:          List of interrupts to be enabled.
 *
 * Enables the specified interrupts. This list will override the
 * previous interrupts enabled through this function. Caller has to
 * maintain the state of the interrupts enabled. To disable all
 * interrupts, set ints to 0.
 */
void dsi_ctrl_hw_cmn_enable_status_interrupts(
		struct dsi_ctrl_hw *ctrl, u32 ints)
{
	u32 reg = 0;

	/* Do not change value of DSI_ERROR_MASK bit */
	reg |= (DSI_R32(ctrl, DSI_INT_CTRL) & BIT(25));
	if (ints & DSI_CMD_MODE_DMA_DONE)
		reg |= BIT(1);
	if (ints & DSI_CMD_FRAME_DONE)
		reg |= BIT(9);
	if (ints & DSI_CMD_STREAM0_FRAME_DONE)
		reg |= BIT(11);
	if (ints & DSI_CMD_STREAM1_FRAME_DONE)
		reg |= BIT(13);
	if (ints & DSI_CMD_STREAM2_FRAME_DONE)
		reg |= BIT(15);
	if (ints & DSI_VIDEO_MODE_FRAME_DONE)
		reg |= BIT(17);
	if (ints & DSI_BTA_DONE)
		reg |= BIT(21);
	if (ints & DSI_DYN_REFRESH_DONE)
		reg |= BIT(29);
	if (ints & DSI_DESKEW_DONE)
		reg |= BIT(31);

	DSI_W32(ctrl, DSI_INT_CTRL, reg);

	DSI_CTRL_HW_DBG(ctrl, "Enable interrupts 0x%x, INT_CTRL=0x%x\n", ints,
			reg);
}

/**
 * get_error_status() - returns the error status
 * @ctrl:          Pointer to the controller host hardware.
 *
 * Returns the ORed list of errors(enum dsi_error_int_type) that are
 * active. This list does not include any status interrupts. Caller
 * should call get_interrupt_status for status interrupts.
 *
 * Return: List of active error interrupts.
 */
u64 dsi_ctrl_hw_cmn_get_error_status(struct dsi_ctrl_hw *ctrl)
{
	u32 dln0_phy_err;
	u32 fifo_status;
	u32 ack_error;
	u32 timeout_errors;
	u32 clk_error;
	u32 dsi_status;
	u64 errors = 0, shift = 0x1;

	dln0_phy_err = DSI_R32(ctrl, DSI_DLN0_PHY_ERR);
	if (dln0_phy_err & BIT(0))
		errors |= DSI_DLN0_ESC_ENTRY_ERR;
	if (dln0_phy_err & BIT(4))
		errors |= DSI_DLN0_ESC_SYNC_ERR;
	if (dln0_phy_err & BIT(8))
		errors |= DSI_DLN0_LP_CONTROL_ERR;
	if (dln0_phy_err & BIT(12))
		errors |= DSI_DLN0_LP0_CONTENTION;
	if (dln0_phy_err & BIT(16))
		errors |= DSI_DLN0_LP1_CONTENTION;

	fifo_status = DSI_R32(ctrl, DSI_FIFO_STATUS);
	if (fifo_status & BIT(7))
		errors |= DSI_CMD_MDP_FIFO_UNDERFLOW;
	if (fifo_status & BIT(10))
		errors |= DSI_CMD_DMA_FIFO_UNDERFLOW;
	if (fifo_status & BIT(18))
		errors |= DSI_DLN0_HS_FIFO_OVERFLOW;
	if (fifo_status & BIT(19))
		errors |= DSI_DLN0_HS_FIFO_UNDERFLOW;
	if (fifo_status & BIT(22))
		errors |= DSI_DLN1_HS_FIFO_OVERFLOW;
	if (fifo_status & BIT(23))
		errors |= DSI_DLN1_HS_FIFO_UNDERFLOW;
	if (fifo_status & BIT(26))
		errors |= DSI_DLN2_HS_FIFO_OVERFLOW;
	if (fifo_status & BIT(27))
		errors |= DSI_DLN2_HS_FIFO_UNDERFLOW;
	if (fifo_status & BIT(30))
		errors |= DSI_DLN3_HS_FIFO_OVERFLOW;
	if (fifo_status & BIT(31))
		errors |= DSI_DLN3_HS_FIFO_UNDERFLOW;

	ack_error = DSI_R32(ctrl, DSI_ACK_ERR_STATUS);
	if (ack_error & BIT(16))
		errors |= DSI_RDBK_SINGLE_ECC_ERR;
	if (ack_error & BIT(17))
		errors |= DSI_RDBK_MULTI_ECC_ERR;
	if (ack_error & BIT(20))
		errors |= DSI_RDBK_CRC_ERR;
	if (ack_error & BIT(23))
		errors |= DSI_RDBK_INCOMPLETE_PKT;
	if (ack_error & BIT(24))
		errors |= DSI_PERIPH_ERROR_PKT;
	if (ack_error & BIT(15))
		errors |= (shift << DSI_EINT_PANEL_SPECIFIC_ERR);

	timeout_errors = DSI_R32(ctrl, DSI_TIMEOUT_STATUS);
	if (timeout_errors & BIT(0))
		errors |= DSI_HS_TX_TIMEOUT;
	if (timeout_errors & BIT(4))
		errors |= DSI_LP_RX_TIMEOUT;
	if (timeout_errors & BIT(8))
		errors |= DSI_BTA_TIMEOUT;

	clk_error = DSI_R32(ctrl, DSI_CLK_STATUS);
	if (clk_error & BIT(16))
		errors |= DSI_PLL_UNLOCK;

	dsi_status = DSI_R32(ctrl, DSI_STATUS);
	if (dsi_status & BIT(31))
		errors |= DSI_INTERLEAVE_OP_CONTENTION;

	DSI_CTRL_HW_DBG(ctrl, "Error status = 0x%llx, phy=0x%x, fifo=0x%x\n",
		 errors, dln0_phy_err, fifo_status);
	DSI_CTRL_HW_DBG(ctrl, "ack=0x%x, timeout=0x%x, clk=0x%x, dsi=0x%x\n",
		 ack_error, timeout_errors, clk_error, dsi_status);
	return errors;
}

/**
 * clear_error_status() - clears the specified errors
 * @ctrl:          Pointer to the controller host hardware.
 * @errors:          List of errors to be cleared.
 */
void dsi_ctrl_hw_cmn_clear_error_status(struct dsi_ctrl_hw *ctrl, u64 errors)
{
	u32 dln0_phy_err = 0;
	u32 fifo_status = 0;
	u32 ack_error = 0;
	u32 timeout_error = 0;
	u32 clk_error = 0;
	u32 dsi_status = 0;

	if (errors & DSI_RDBK_SINGLE_ECC_ERR)
		ack_error |= BIT(16);
	if (errors & DSI_RDBK_MULTI_ECC_ERR)
		ack_error |= BIT(17);
	if (errors & DSI_RDBK_CRC_ERR)
		ack_error |= BIT(20);
	if (errors & DSI_RDBK_INCOMPLETE_PKT)
		ack_error |= BIT(23);
	if (errors & DSI_PERIPH_ERROR_PKT)
		ack_error |= BIT(24);
	if (errors & DSI_PANEL_SPECIFIC_ERR)
		ack_error |= BIT(15);

	if (errors & DSI_LP_RX_TIMEOUT)
		timeout_error |= BIT(4);
	if (errors & DSI_HS_TX_TIMEOUT)
		timeout_error |= BIT(0);
	if (errors & DSI_BTA_TIMEOUT)
		timeout_error |= BIT(8);

	if (errors & DSI_PLL_UNLOCK)
		clk_error |= BIT(16);

	if (errors & DSI_DLN0_LP0_CONTENTION)
		dln0_phy_err |= BIT(12);
	if (errors & DSI_DLN0_LP1_CONTENTION)
		dln0_phy_err |= BIT(16);
	if (errors & DSI_DLN0_ESC_ENTRY_ERR)
		dln0_phy_err |= BIT(0);
	if (errors & DSI_DLN0_ESC_SYNC_ERR)
		dln0_phy_err |= BIT(4);
	if (errors & DSI_DLN0_LP_CONTROL_ERR)
		dln0_phy_err |= BIT(8);

	if (errors & DSI_CMD_DMA_FIFO_UNDERFLOW)
		fifo_status |= BIT(10);
	if (errors & DSI_CMD_MDP_FIFO_UNDERFLOW)
		fifo_status |= BIT(7);
	if (errors & DSI_DLN0_HS_FIFO_OVERFLOW)
		fifo_status |= BIT(18);
	if (errors & DSI_DLN1_HS_FIFO_OVERFLOW)
		fifo_status |= BIT(22);
	if (errors & DSI_DLN2_HS_FIFO_OVERFLOW)
		fifo_status |= BIT(26);
	if (errors & DSI_DLN3_HS_FIFO_OVERFLOW)
		fifo_status |= BIT(30);
	if (errors & DSI_DLN0_HS_FIFO_UNDERFLOW)
		fifo_status |= BIT(19);
	if (errors & DSI_DLN1_HS_FIFO_UNDERFLOW)
		fifo_status |= BIT(23);
	if (errors & DSI_DLN2_HS_FIFO_UNDERFLOW)
		fifo_status |= BIT(27);
	if (errors & DSI_DLN3_HS_FIFO_UNDERFLOW)
		fifo_status |= BIT(31);

	if (errors & DSI_INTERLEAVE_OP_CONTENTION)
		dsi_status |= BIT(31);

	DSI_W32(ctrl, DSI_DLN0_PHY_ERR, dln0_phy_err);
	DSI_W32(ctrl, DSI_FIFO_STATUS, fifo_status);
	/* Writing of an extra 0 is needed to clear ack error bits */
	DSI_W32(ctrl, DSI_ACK_ERR_STATUS, ack_error);
	wmb(); /* make sure register is committed */
	DSI_W32(ctrl, DSI_ACK_ERR_STATUS, 0x0);
	DSI_W32(ctrl, DSI_TIMEOUT_STATUS, timeout_error);
	DSI_W32(ctrl, DSI_CLK_STATUS, clk_error);
	DSI_W32(ctrl, DSI_STATUS, dsi_status);

	DSI_CTRL_HW_DBG(ctrl, "clear errors = 0x%llx, phy=0x%x, fifo=0x%x\n",
		 errors, dln0_phy_err, fifo_status);
	DSI_CTRL_HW_DBG(ctrl, "ack=0x%x, timeout=0x%x, clk=0x%x, dsi=0x%x\n",
		 ack_error, timeout_error, clk_error, dsi_status);
}

/**
 * enable_error_interrupts() - enable the specified interrupts
 * @ctrl:          Pointer to the controller host hardware.
 * @errors:        List of errors to be enabled.
 *
 * Enables the specified interrupts. This list will override the
 * previous interrupts enabled through this function. Caller has to
 * maintain the state of the interrupts enabled. To disable all
 * interrupts, set errors to 0.
 */
void dsi_ctrl_hw_cmn_enable_error_interrupts(struct dsi_ctrl_hw *ctrl,
					    u64 errors)
{
	u32 int_ctrl = 0;
	u32 int_mask0 = 0x7FFF3BFF;

	int_ctrl = DSI_R32(ctrl, DSI_INT_CTRL);
	if (errors)
		int_ctrl |= BIT(25);
	else
		int_ctrl &= ~BIT(25);

	if (errors & DSI_RDBK_SINGLE_ECC_ERR)
		int_mask0 &= ~BIT(0);
	if (errors & DSI_RDBK_MULTI_ECC_ERR)
		int_mask0 &= ~BIT(1);
	if (errors & DSI_RDBK_CRC_ERR)
		int_mask0 &= ~BIT(2);
	if (errors & DSI_RDBK_INCOMPLETE_PKT)
		int_mask0 &= ~BIT(3);
	if (errors & DSI_PERIPH_ERROR_PKT)
		int_mask0 &= ~BIT(4);

	if (errors & DSI_LP_RX_TIMEOUT)
		int_mask0 &= ~BIT(5);
	if (errors & DSI_HS_TX_TIMEOUT)
		int_mask0 &= ~BIT(6);
	if (errors & DSI_BTA_TIMEOUT)
		int_mask0 &= ~BIT(7);

	if (errors & DSI_PLL_UNLOCK)
		int_mask0 &= ~BIT(28);

	if (errors & DSI_DLN0_LP0_CONTENTION)
		int_mask0 &= ~BIT(24);
	if (errors & DSI_DLN0_LP1_CONTENTION)
		int_mask0 &= ~BIT(25);
	if (errors & DSI_DLN0_ESC_ENTRY_ERR)
		int_mask0 &= ~BIT(21);
	if (errors & DSI_DLN0_ESC_SYNC_ERR)
		int_mask0 &= ~BIT(22);
	if (errors & DSI_DLN0_LP_CONTROL_ERR)
		int_mask0 &= ~BIT(23);

	if (errors & DSI_CMD_DMA_FIFO_UNDERFLOW)
		int_mask0 &= ~BIT(9);
	if (errors & DSI_CMD_MDP_FIFO_UNDERFLOW)
		int_mask0 &= ~BIT(11);
	if (errors & DSI_DLN0_HS_FIFO_OVERFLOW)
		int_mask0 &= ~BIT(16);
	if (errors & DSI_DLN1_HS_FIFO_OVERFLOW)
		int_mask0 &= ~BIT(17);
	if (errors & DSI_DLN2_HS_FIFO_OVERFLOW)
		int_mask0 &= ~BIT(18);
	if (errors & DSI_DLN3_HS_FIFO_OVERFLOW)
		int_mask0 &= ~BIT(19);
	if (errors & DSI_DLN0_HS_FIFO_UNDERFLOW)
		int_mask0 &= ~BIT(26);
	if (errors & DSI_DLN1_HS_FIFO_UNDERFLOW)
		int_mask0 &= ~BIT(27);
	if (errors & DSI_DLN2_HS_FIFO_UNDERFLOW)
		int_mask0 &= ~BIT(29);
	if (errors & DSI_DLN3_HS_FIFO_UNDERFLOW)
		int_mask0 &= ~BIT(30);

	if (errors & DSI_INTERLEAVE_OP_CONTENTION)
		int_mask0 &= ~BIT(8);

	DSI_W32(ctrl, DSI_INT_CTRL, int_ctrl);
	DSI_W32(ctrl, DSI_ERR_INT_MASK0, int_mask0);

	DSI_CTRL_HW_DBG(ctrl, "[DSI_%d] enable errors = 0x%llx, int_mask0=0x%x\n",
		 errors, int_mask0);
}

/**
 * video_test_pattern_setup() - setup test pattern engine for video mode
 * @ctrl:          Pointer to the controller host hardware.
 * @type:          Type of test pattern.
 * @init_val:      Initial value to use for generating test pattern.
 */
void dsi_ctrl_hw_cmn_video_test_pattern_setup(struct dsi_ctrl_hw *ctrl,
					     enum dsi_test_pattern type,
					     u32 init_val)
{
	u32 reg = 0;

	DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_VIDEO_INIT_VAL, init_val);

	switch (type) {
	case DSI_TEST_PATTERN_FIXED:
		reg |= (0x2 << 4);
		break;
	case DSI_TEST_PATTERN_INC:
		reg |= (0x1 << 4);
		break;
	case DSI_TEST_PATTERN_POLY:
		DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_VIDEO_POLY, 0xF0F0F);
		break;
	default:
		break;
	}

	DSI_W32(ctrl, DSI_TPG_MAIN_CONTROL, 0x100);
	DSI_W32(ctrl, DSI_TPG_VIDEO_CONFIG, 0x5);
	DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CTRL, reg);

	DSI_CTRL_HW_DBG(ctrl, "Video test pattern setup done\n");
}

/**
 * cmd_test_pattern_setup() - setup test patttern engine for cmd mode
 * @ctrl:          Pointer to the controller host hardware.
 * @type:          Type of test pattern.
 * @init_val:      Initial value to use for generating test pattern.
 * @stream_id:     Stream Id on which packets are generated.
 */
void dsi_ctrl_hw_cmn_cmd_test_pattern_setup(struct dsi_ctrl_hw *ctrl,
					   enum dsi_test_pattern type,
					   u32 init_val,
					   u32 stream_id)
{
	u32 reg = 0;
	u32 init_offset;
	u32 poly_offset;
	u32 pattern_sel_shift;

	switch (stream_id) {
	case 0:
		init_offset = DSI_TEST_PATTERN_GEN_CMD_MDP_INIT_VAL0;
		poly_offset = DSI_TEST_PATTERN_GEN_CMD_MDP_STREAM0_POLY;
		pattern_sel_shift = 8;
		break;
	case 1:
		init_offset = DSI_TEST_PATTERN_GEN_CMD_MDP_INIT_VAL1;
		poly_offset = DSI_TEST_PATTERN_GEN_CMD_MDP_STREAM1_POLY;
		pattern_sel_shift = 12;
		break;
	case 2:
		init_offset = DSI_TEST_PATTERN_GEN_CMD_MDP_INIT_VAL2;
		poly_offset = DSI_TEST_PATTERN_GEN_CMD_MDP_STREAM2_POLY;
		pattern_sel_shift = 20;
		break;
	default:
		return;
	}

	DSI_W32(ctrl, init_offset, init_val);

	switch (type) {
	case DSI_TEST_PATTERN_FIXED:
		reg |= (0x2 << pattern_sel_shift);
		break;
	case DSI_TEST_PATTERN_INC:
		reg |= (0x1 << pattern_sel_shift);
		break;
	case DSI_TEST_PATTERN_POLY:
		DSI_W32(ctrl, poly_offset, 0xF0F0F);
		break;
	default:
		break;
	}

	DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CTRL, reg);
	DSI_CTRL_HW_DBG(ctrl, "Cmd test pattern setup done\n");
}

/**
 * test_pattern_enable() - enable test pattern engine
 * @ctrl:          Pointer to the controller host hardware.
 * @enable:        Enable/Disable test pattern engine.
 */
void dsi_ctrl_hw_cmn_test_pattern_enable(struct dsi_ctrl_hw *ctrl,
					bool enable)
{
	u32 reg = DSI_R32(ctrl, DSI_TEST_PATTERN_GEN_CTRL);

	if (enable)
		reg |= BIT(0);
	else
		reg &= ~BIT(0);

	DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CTRL, reg);

	DSI_CTRL_HW_DBG(ctrl, "Test pattern enable=%d\n", enable);
}

/**
 * trigger_cmd_test_pattern() - trigger a command mode frame update with
 *                              test pattern
 * @ctrl:          Pointer to the controller host hardware.
 * @stream_id:     Stream on which frame update is sent.
 */
void dsi_ctrl_hw_cmn_trigger_cmd_test_pattern(struct dsi_ctrl_hw *ctrl,
					     u32 stream_id)
{
	switch (stream_id) {
	case 0:
		DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CMD_STREAM0_TRIGGER, 0x1);
		break;
	case 1:
		DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CMD_STREAM1_TRIGGER, 0x1);
		break;
	case 2:
		DSI_W32(ctrl, DSI_TEST_PATTERN_GEN_CMD_STREAM2_TRIGGER, 0x1);
		break;
	default:
		break;
	}

	DSI_CTRL_HW_DBG(ctrl, "Cmd Test pattern trigger\n");
}

void dsi_ctrl_hw_dln0_phy_err(struct dsi_ctrl_hw *ctrl)
{
	u32 status = 0;
	/*
	 * Clear out any phy errors prior to exiting ULPS
	 * This fixes certain instances where phy does not exit
	 * ULPS cleanly. Also, do not print error during such cases.
	 */
	status = DSI_R32(ctrl, DSI_DLN0_PHY_ERR);
	if (status & 0x011111) {
		DSI_W32(ctrl, DSI_DLN0_PHY_ERR, status);
		DSI_CTRL_HW_ERR(ctrl, "phy_err_status = %x\n", status);
	}
}

void dsi_ctrl_hw_cmn_phy_reset_config(struct dsi_ctrl_hw *ctrl,
		bool enable)
{
	u32 reg = 0;

	reg = DSI_MMSS_MISC_R32(ctrl, MMSS_MISC_CLAMP_REG_OFF);

	/* Mask/unmask disable PHY reset bit */
	if (enable)
		reg |= BIT(30);
	else
		reg &= ~BIT(30);

	DSI_MMSS_MISC_W32(ctrl, MMSS_MISC_CLAMP_REG_OFF, reg);
}

int dsi_ctrl_hw_cmn_ctrl_reset(struct dsi_ctrl_hw *ctrl,
		int mask)
{
	int rc = 0;
	u32 data;

	DSI_CTRL_HW_DBG(ctrl, "DSI CTRL and PHY reset, mask=%d\n", mask);

	data = DSI_R32(ctrl, 0x0004);
	/* Disable DSI video mode */
	DSI_W32(ctrl, 0x004, (data & ~BIT(1)));
	wmb(); /* ensure register committed */
	/* Disable DSI controller */
	DSI_W32(ctrl, 0x004, (data & ~(BIT(0) | BIT(1))));
	wmb(); /* ensure register committed */
	/* "Force On" all dynamic clocks */
	DSI_W32(ctrl, 0x11c, 0x100a00);

	/* DSI_SW_RESET */
	DSI_W32(ctrl, 0x118, 0x1);
	wmb(); /* ensure register is committed */
	DSI_W32(ctrl, 0x118, 0x0);
	wmb(); /* ensure register is committed */

	/* Remove "Force On" all dynamic clocks */
	DSI_W32(ctrl, 0x11c, 0x00);
	/* Enable DSI controller */
	DSI_W32(ctrl, 0x004, (data & ~BIT(1)));
	wmb(); /* ensure register committed */

	return rc;
}

void dsi_ctrl_hw_cmn_mask_error_intr(struct dsi_ctrl_hw *ctrl, u32 idx, bool en)
{
	u32 reg = 0;
	u32 fifo_status = 0, timeout_status = 0;
	u32 overflow_clear = BIT(10) | BIT(18) | BIT(22) | BIT(26) | BIT(30);
	u32 underflow_clear = BIT(19) | BIT(23) | BIT(27) | BIT(31);
	u32 lp_rx_clear = BIT(4);

	reg = DSI_R32(ctrl, 0x10c);

	/*
	 * Before unmasking we should clear the corresponding error status bits
	 * that might have been set while we masked these errors. Since these
	 * are sticky bits, these errors will trigger the moment we unmask
	 * the error bits.
	 */
	if (idx & BIT(DSI_FIFO_OVERFLOW)) {
		if (en) {
			reg |= (0x1f << 16);
			reg |= BIT(9);
		} else {
			reg &= ~(0x1f << 16);
			reg &= ~BIT(9);
			fifo_status = DSI_R32(ctrl, 0x00c);
			DSI_W32(ctrl, 0x00c, fifo_status | overflow_clear);
		}
	}

	if (idx & BIT(DSI_FIFO_UNDERFLOW)) {
		if (en)
			reg |= (0x1b << 26);
		else {
			reg &= ~(0x1b << 26);
			fifo_status = DSI_R32(ctrl, 0x00c);
			DSI_W32(ctrl, 0x00c, fifo_status | underflow_clear);
		}
	}

	if (idx & BIT(DSI_LP_Rx_TIMEOUT)) {
		if (en)
			reg |= (0x7 << 23);
		else {
			reg &= ~(0x7 << 23);
			timeout_status = DSI_R32(ctrl, 0x0c0);
			DSI_W32(ctrl, 0x0c0, timeout_status | lp_rx_clear);
		}
	}

	if (idx & BIT(DSI_PLL_UNLOCK_ERR)) {
		if (en)
			reg |= BIT(28);
		else
			reg &= ~BIT(28);
	}

	DSI_W32(ctrl, 0x10c, reg);
	wmb(); /* ensure error is masked */
}

void dsi_ctrl_hw_cmn_error_intr_ctrl(struct dsi_ctrl_hw *ctrl, bool en)
{
	u32 reg = 0;
	u32 dsi_total_mask = 0x2222AA02;

	reg = DSI_R32(ctrl, 0x110);
	reg &= dsi_total_mask;

	if (en)
		reg |= (BIT(24) | BIT(25));
	else
		reg &= ~BIT(25);

	DSI_W32(ctrl, 0x110, reg);
	wmb(); /* ensure error is masked */
}

u32 dsi_ctrl_hw_cmn_get_error_mask(struct dsi_ctrl_hw *ctrl)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, 0x10c);

	return reg;
}

u32 dsi_ctrl_hw_cmn_get_hw_version(struct dsi_ctrl_hw *ctrl)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, 0x0);

	return reg;
}

int dsi_ctrl_hw_cmn_wait_for_cmd_mode_mdp_idle(struct dsi_ctrl_hw *ctrl)
{
	int rc = 0, val = 0;
	u32 cmd_mode_mdp_busy_mask = BIT(2);
	u32 const sleep_us = 2 * 1000;
	u32 const timeout_us = 200 * 1000;

	rc = readl_poll_timeout(ctrl->base + DSI_STATUS, val,
			!(val & cmd_mode_mdp_busy_mask), sleep_us, timeout_us);
	if (rc)
		DSI_CTRL_HW_ERR(ctrl, "timed out waiting for idle\n");

	return rc;
}

void dsi_ctrl_hw_cmn_hs_req_sel(struct dsi_ctrl_hw *ctrl, bool sel_phy)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, DSI_LANE_CTRL);
	if (sel_phy)
		reg &= ~BIT(24);
	else
		reg |= BIT(24);
	DSI_W32(ctrl, DSI_LANE_CTRL, reg);
	wmb(); /* make sure request is set */
}

void dsi_ctrl_hw_cmn_set_continuous_clk(struct dsi_ctrl_hw *ctrl, bool enable)
{
	u32 reg = 0;

	reg = DSI_R32(ctrl, DSI_LANE_CTRL);
	if (enable)
		reg |= BIT(28);
	else
		reg &= ~BIT(28);
	DSI_W32(ctrl, DSI_LANE_CTRL, reg);
	wmb(); /* make sure request is set */
}

int dsi_ctrl_hw_cmn_wait4dynamic_refresh_done(struct dsi_ctrl_hw *ctrl)
{
	int rc;
	u32 const sleep_us = 1000;
	u32 const timeout_us = 84000; /* approximately 5 vsyncs */
	u32 reg = 0, dyn_refresh_done = BIT(28);

	rc = readl_poll_timeout(ctrl->base + DSI_INT_CTRL, reg,
				(reg & dyn_refresh_done), sleep_us, timeout_us);
	if (rc) {
		DSI_CTRL_HW_ERR(ctrl, "wait4dynamic refresh timedout %d\n", rc);
		return rc;
	}

	/* ack dynamic refresh done status */
	reg = DSI_R32(ctrl, DSI_INT_CTRL);
	reg |= dyn_refresh_done;
	DSI_W32(ctrl, DSI_INT_CTRL, reg);

	return 0;
}
