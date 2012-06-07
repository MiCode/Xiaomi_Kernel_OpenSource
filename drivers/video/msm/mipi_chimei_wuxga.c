/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

/*
 * Chimei WUXGA LVDS Panel driver.
 * The panel model is N101JSF-L21.
 *
 * The panel interface includes:
 * 1. LVDS input for video (clock & data).
 * 2. few configuration	pins to control 3D module: Enable, Mode (2D/3D).
 * 3. Backlight LED control (PWM 200 HZ).
 *
 * This panel is controled via the Toshiba DSI-to-LVDS bridge.
 *
 */

/* #define DEBUG 1 */

#include "msm_fb.h"
#include "msm_fb_panel.h"
#include "mipi_dsi.h"
#include "mipi_tc358764_dsi2lvds.h"

#define MHZ (1000*1000)

/**
 * Panel info parameters.
 * The panel info is passed to the mipi framebuffer driver.
 */
static struct msm_panel_info chimei_wuxga_pinfo;

/**
 * The mipi_dsi_phy_ctrl is calculated according to the
 * "dsi_timing_program.xlsm" excel sheet.
 * Output is based on: 1200x1920, RGB565, 4 lanes , 58 frames
 * per second.
 */
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* DSIPHY_REGULATOR_CTRL */
	.regulator = {0x03, 0x0a, 0x04, 0x00, 0x20}, /* common 8960 */
	/* DSIPHY_CTRL */
	.ctrl = {0x5f, 0x00, 0x00, 0x10}, /* common 8960 */
	/* DSIPHY_STRENGTH_CTRL */
	.strength = {0xff, 0x00, 0x06, 0x00}, /* common 8960 */
	/* DSIPHY_TIMING_CTRL */
	.timing = { 0xC9, 0x92, 0x29, /* panel specific */
	0, /* DSIPHY_TIMING_CTRL_3 = 0 */
	0x2D, 0x9B, 0x2B, 0x94, 0x2D, 0x03, 0x04},  /* panel specific */

	/* DSIPHY_PLL_CTRL */
	.pll = { 0x00, /* common 8960 */
	/* VCO */
	0x30, (0x01 | 0x30) , (0x19 | 0xC0), /* panel specific */
	0x00, 0x50, 0x48, 0x63,
	0x77, 0x88, 0x99, /* Auto update by dsi-mipi driver */
	0x00, 0x14, 0x03, 0x00, 0x02, /* common 8960 */
	0x00, 0x20, 0x00, 0x01 }, /* common 8960 */
};

/**
 * Module init.
 *
 * Register the panel-info.
 *
 * Some parameters are from the panel datasheet
 * and other are *calculated* by the "dsi_timing_program.xlsm"
 * excel file
 *
 * @return int
 */
static int __init mipi_chimei_wuxga_init(void)
{
	int ret;
	struct msm_panel_info *pinfo = &chimei_wuxga_pinfo;

	if (msm_fb_detect_client("mipi_video_chimei_wuxga"))
		return 0;

	pr_info("mipi-dsi chimei wuxga (1200x1920) driver ver 1.0.\n");

	/* Portrait */
	pinfo->xres = 1200;
	pinfo->yres = 1920;
	pinfo->type =  MIPI_VIDEO_PANEL;
	pinfo->pdest = DISPLAY_1; /* Primary Display */
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24; /* RGB565 requires 24 bits-per-pixel :-O */
	pinfo->fb_num = 2; /* using two frame buffers */

	/*
	 * The CMI panel requires 80 MHZ LVDS-CLK.
	 * The D2L bridge drives the LVDS-CLK from the DSI-CLK.
	 * The DSI-CLK = bitclk/2, 640 MHZ/2= 320 MHZ.
	 * LVDS-CLK = DSI-CLK/4 , 320 MHZ/4= 80 MHZ.
	 */

	pinfo->clk_rate = 635 * MHZ ; /* bitclk Calculated */

	/*
	 * this panel is operated by DE,
	 * vsycn and hsync are ignored
	 */

	pinfo->lcdc.h_front_porch = 160-48-32;	/* thfp */
	pinfo->lcdc.h_back_porch = 48;	/* thb */
	pinfo->lcdc.h_pulse_width = 32;	/* thpw */

	pinfo->lcdc.v_front_porch = 26-3-6;	/* tvfp */
	pinfo->lcdc.v_back_porch = 3;	/* tvb */
	pinfo->lcdc.v_pulse_width = 6;	/* tvpw */

	pinfo->lcdc.border_clr = 0;		/* black */
	pinfo->lcdc.underflow_clr = 0xff;	/* blue */

	pinfo->lcdc.hsync_skew = 0;

	/* Backlight levels - controled via PMIC pwm gpio */
	pinfo->bl_max = PWM_LEVEL;
	pinfo->bl_min = 1;

	/* mipi - general */
	pinfo->mipi.vc = 0; /* virtual channel */
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo->mipi.tx_eot_append = true;
	pinfo->mipi.t_clk_post = 34;		/* Calculated */
	pinfo->mipi.t_clk_pre = 69;		/* Calculated */

	pinfo->mipi.dsi_phy_db = &dsi_video_mode_phy_db;

	/* Four lanes are recomended for 1920x1200 at 60 frames per second */
	pinfo->mipi.frame_rate = 60;
	pinfo->mipi.data_lane0 = true;
	pinfo->mipi.data_lane1 = true;
	pinfo->mipi.data_lane2 = true;
	pinfo->mipi.data_lane3 = true;
	pinfo->mipi.esc_byte_ratio = 6;

	pinfo->mipi.mode = DSI_VIDEO_MODE;
	/*
	 * Note: The CMI panel input is RGB888,
	 * thus the DSI-to-LVDS bridge output is RGB888.
	 * This parameter selects the DSI-Core output to the bridge.
	 */
	pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB565;

	/* mipi - video mode */
	pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
	pinfo->mipi.pulse_mode_hsa_he = false; /* sync mode */

	pinfo->mipi.hfp_power_stop = false;
	pinfo->mipi.hbp_power_stop = false;
	pinfo->mipi.hsa_power_stop = false;
	pinfo->mipi.eof_bllp_power_stop = false;
	pinfo->mipi.bllp_power_stop = false;

	/* mipi - command mode */
	pinfo->mipi.te_sel = 1; /* TE from vsycn gpio */
	pinfo->mipi.interleave_max = 1;
	/* The bridge supports only Generic Read/Write commands */
	pinfo->mipi.insert_dcs_cmd = false;
	pinfo->mipi.wr_mem_continue = 0;
	pinfo->mipi.wr_mem_start = 0;
	pinfo->mipi.stream = false; /* dma_p */
	pinfo->mipi.mdp_trigger = DSI_CMD_TRIGGER_NONE;
	pinfo->mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	/*
	 * toshiba d2l chip does not need max_pkt_size dcs cmd
	 * client reply len is directly configure through
	 * RDPKTLN register (0x0404)
	 */
	pinfo->mipi.no_max_pkt_size = 1;
	pinfo->mipi.force_clk_lane_hs = 1;

	pinfo->is_3d_panel = FB_TYPE_3D_PANEL;

	ret = mipi_tc358764_dsi2lvds_register(pinfo, MIPI_DSI_PRIM, 1);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_chimei_wuxga_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Chimei WUXGA LVDS Panel driver");
MODULE_AUTHOR("Amir Samuelov <amirs@codeaurora.org>");
