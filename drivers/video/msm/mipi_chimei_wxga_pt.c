/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
 * Chimei WXGA LVDS Panel driver.
 * The panel model is N101BCG-L21.
 *
 * The panel interface includes:
 * 1. LVDS input for video (clock & data).
 * 2. few configuration	pins: Up/Down scan, Left/Right scan etc.
 * 3. Backlight LED control.
 * 4. I2C interface for EEPROM access.
 *
 * The Panel is *internally* controlled by Novatek NT51009 controller.
 * However, the "3-wire" SPI interface is not exposed on the panel interface.
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
static struct msm_panel_info chimei_wxga_pinfo;

/**
 * The mipi_dsi_phy_ctrl is calculated according to the
 * "DSI_panel_bring_up_guide_ver3.docm" using the excel sheet.
 * Output is based on: 1366x768, RGB888, 4 lanes , 60 frames per second.
 */
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* DSIPHY_REGULATOR_CTRL */
	.regulator = {0x03, 0x0a, 0x04, 0x00, 0x20}, /* common 8960 */
	/* DSIPHY_CTRL */
	.ctrl = {0x5f, 0x00, 0x00, 0x10}, /* common 8960 */
	/* DSIPHY_STRENGTH_CTRL */
	.strength = {0xff, 0x00, 0x06, 0x00}, /* common 8960 */
	/* DSIPHY_TIMING_CTRL */
	.timing = { 0xB6, 0x8D, 0x1E, /* panel specific */
	0, /* DSIPHY_TIMING_CTRL_3 = 0 */
	0x21, 0x95, 0x21, 0x8F, 0x21, 0x03, 0x04},  /* panel specific */

	/* DSIPHY_PLL_CTRL */
	.pll = { 0x00, /* common 8960 */
	/* VCO */
	0xC6, 0x01, 0x19, /* panel specific */
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
 * and other are *calculated* according to the
 * "DSI_panel_bring_up_guide_ver3.docm".
 *
 * @return int
 */
static int __init mipi_chimei_wxga_init(void)
{
	int ret;
	struct msm_panel_info *pinfo = &chimei_wxga_pinfo;

	if (msm_fb_detect_client("mipi_video_chimei_wxga"))
		return 0;

	pr_debug("mipi-dsi chimei wxga (1366x768) driver ver 1.0.\n");
	/* Landscape */
	pinfo->xres = 1366;
	pinfo->yres = 768;
	pinfo->type =  MIPI_VIDEO_PANEL;
	pinfo->pdest = DISPLAY_1; /* Primary Display */
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24; /* RGB888 = 24 bits-per-pixel */
	pinfo->fb_num = 2; /* using two frame buffers */

	/* bitclk */
	pinfo->clk_rate = 473400000; /* 473.4 MHZ Calculated */

	/*
	 * this panel is operated by DE,
	 * vsycn and hsync are ignored
	 */

	pinfo->lcdc.h_front_porch = 96+2;/* thfp */
	pinfo->lcdc.h_back_porch = 88;	/* thb */
	pinfo->lcdc.h_pulse_width = 40;	/* thpw */

	pinfo->lcdc.v_front_porch = 15;	/* tvfp */
	pinfo->lcdc.v_back_porch = 23;	/* tvb */
	pinfo->lcdc.v_pulse_width = 20;	/* tvpw */

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
	pinfo->mipi.t_clk_pre = 64;		/* Calculated */

	pinfo->mipi.dsi_phy_db = &dsi_video_mode_phy_db;

	/* Four lanes are recomended for 1366x768 at 60 frames per second */
	pinfo->mipi.frame_rate = 60; /* 60 frames per second */
	pinfo->mipi.data_lane0 = true;
	pinfo->mipi.data_lane1 = true;
	pinfo->mipi.data_lane2 = true;
	pinfo->mipi.data_lane3 = true;

	pinfo->mipi.mode = DSI_VIDEO_MODE;
	/*
	 * Note: The CMI panel input is RGB888,
	 * thus the DSI-to-LVDS bridge output is RGB888.
	 * This parameter selects the DSI-Core output to the bridge.
	 */
	pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;

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
	 * toshiba d2l chip does not need max_pkt_szie dcs cmd
	 * client reply len is directly configure through
	 * RDPKTLN register (0x0404)
	 */
	pinfo->mipi.no_max_pkt_size = 1;
	pinfo->mipi.force_clk_lane_hs = 1;

	ret = mipi_tc358764_dsi2lvds_register(pinfo, MIPI_DSI_PRIM,
					      MIPI_DSI_PANEL_WXGA);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_chimei_wxga_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Chimei WXGA LVDS Panel driver");
MODULE_AUTHOR("Amir Samuelov <amirs@codeaurora.org>");
