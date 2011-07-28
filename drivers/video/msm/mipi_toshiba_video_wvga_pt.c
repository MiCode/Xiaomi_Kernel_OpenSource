/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_toshiba.h"

static struct msm_panel_info pinfo;

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* 480*854, RGB888, 2 Lane 60 fps video mode */
		{0x03, 0x01, 0x01, 0x00},	/* regulator */
		/* timing   */
		{0x6a, 0x22, 0x0f, 0x00, 0x30, 0x38, 0x13, 0x26,
		0x1b, 0x03, 0x04},
		{0x7f, 0x00, 0x00, 0x00},	/* phy ctrl */
		{0xee, 0x03, 0x86, 0x03},	/* strength */
		/* pll control */

#define DSI_BIT_CLK_380MHZ

#if defined(DSI_BIT_CLK_366MHZ)
		{0x41, 0xdb, 0xb2, 0xf5, 0x00, 0x50, 0x48, 0x63,
		0x31, 0x0f, 0x07,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
#elif defined(DSI_BIT_CLK_380MHZ)
		{0x41, 0xf7, 0xb2, 0xf5, 0x00, 0x50, 0x48, 0x63,
		0x31, 0x0f, 0x07,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
#elif defined(DSI_BIT_CLK_400MHZ)
		{0x41, 0x8f, 0xb1, 0xda, 0x00, 0x50, 0x48, 0x63,
		0x31, 0x0f, 0x07,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
#else		/* 200 mhz */
		{0x41, 0x8f, 0xb1, 0xda, 0x00, 0x50, 0x48, 0x63,
		0x33, 0x1f, 0x0f,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
#endif
};

static int __init mipi_video_toshiba_wvga_pt_init(void)
{
	int ret;

	if (msm_fb_detect_client("mipi_video_toshiba_wvga"))
		return 0;

	pinfo.xres = 480;
	pinfo.yres = 864; /* 856 for V1 surf */
	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.lcdc.h_back_porch = 64;
	pinfo.lcdc.h_front_porch = 64;
	pinfo.lcdc.h_pulse_width = 16;
	pinfo.lcdc.v_back_porch = 8;
	pinfo.lcdc.v_front_porch = 4;
	pinfo.lcdc.v_pulse_width = 1;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 15;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = TRUE;
	pinfo.mipi.hfp_power_stop = FALSE;
	pinfo.mipi.hbp_power_stop = FALSE;
	pinfo.mipi.hsa_power_stop = FALSE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_NON_BURST_SYNCH_PULSE;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_BGR;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.t_clk_post = 0x04;
	pinfo.mipi.t_clk_pre = 0x17;
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;

	ret = mipi_toshiba_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_WVGA_PT);
	if (ret)
		printk(KERN_ERR "%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_toshiba_wvga_pt_init);
