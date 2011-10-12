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
 */

#include "msm_fb.h"

static struct msm_panel_common_pdata *cm_pdata;
static struct platform_device *cm_fbpdev;

static int lvds_chimei_panel_on(struct platform_device *pdev)
{
	return 0;
}

static int lvds_chimei_panel_off(struct platform_device *pdev)
{
	return 0;
}

static void lvds_chimei_set_backlight(struct msm_fb_data_type *mfd)
{
}

static int __devinit lvds_chimei_probe(struct platform_device *pdev)
{
	int rc = 0;

	if (pdev->id == 0) {
		cm_pdata = pdev->dev.platform_data;
		if (cm_pdata == NULL)
			pr_err("%s: no PWM gpio specified\n", __func__);
		return 0;
	}

	cm_fbpdev = msm_fb_add_device(pdev);
	if (!cm_fbpdev) {
		dev_err(&pdev->dev, "failed to add msm_fb device\n");
		rc = -ENODEV;
		goto probe_exit;
	}

probe_exit:
	return rc;
}

static struct platform_driver this_driver = {
	.probe  = lvds_chimei_probe,
	.driver = {
		.name   = "lvds_chimei_wxga",
	},
};

static struct msm_fb_panel_data lvds_chimei_panel_data = {
	.on = lvds_chimei_panel_on,
	.off = lvds_chimei_panel_off,
	.set_backlight = lvds_chimei_set_backlight,
};

static struct platform_device this_device = {
	.name   = "lvds_chimei_wxga",
	.id	= 1,
	.dev	= {
		.platform_data = &lvds_chimei_panel_data,
	}
};

static int __init lvds_chimei_wxga_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	if (msm_fb_detect_client("lvds_chimei_wxga"))
		return 0;

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &lvds_chimei_panel_data.panel_info;
	pinfo->xres = 320;
	pinfo->yres = 240;
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
	pinfo->type = LVDS_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 75000000;
	pinfo->bl_max = 15;
	pinfo->bl_min = 1;

	/*
	 * this panel is operated by de,
	 * vsycn and hsync are ignored
	 */
	pinfo->lcdc.h_back_porch = 0;
	pinfo->lcdc.h_front_porch = 194;
	pinfo->lcdc.h_pulse_width = 40;
	pinfo->lcdc.v_back_porch = 0;
	pinfo->lcdc.v_front_porch = 38;
	pinfo->lcdc.v_pulse_width = 20;
	pinfo->lcdc.border_clr = 0xffff00;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;
	pinfo->lvds.channel_mode = LVDS_SINGLE_CHANNEL_MODE;
	pinfo->lcdc.xres_pad = 1046;
	pinfo->lcdc.yres_pad = 528;

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lvds_chimei_wxga_init);
