/* Copyright (c) 2010, 2012 The Linux Foundation. All rights reserved.
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
#include "mddihost.h"
#include "mddihosti.h"

#define MDDI_ORISE_1_2 1
#define write_client_reg(__X, __Y, __Z) {\
	mddi_queue_register_write(__X, __Y, TRUE, 0);\
}

static int mddi_orise_lcd_on(struct platform_device *pdev);
static int mddi_orise_lcd_off(struct platform_device *pdev);
static int __init mddi_orise_probe(struct platform_device *pdev);
static int __init mddi_orise_init(void);

/* function used to turn on the display */
static void mddi_orise_prim_lcd_init(void)
{
	write_client_reg(0x00110000, 0, TRUE);
	mddi_wait(150);
	write_client_reg(0x00290000, 0, TRUE);
}

static struct platform_driver this_driver = {
	.driver = {
		.name   = "mddi_orise",
	},
};

static struct msm_fb_panel_data mddi_orise_panel_data = {
	.on = mddi_orise_lcd_on,
	.off = mddi_orise_lcd_off,
};

static struct platform_device this_device = {
	.name	= "mddi_orise",
	.id	= MDDI_ORISE_1_2,
	.dev	= {
		.platform_data = &mddi_orise_panel_data,
	}
};

static int mddi_orise_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mddi_orise_prim_lcd_init();

	return 0;
}

static int mddi_orise_lcd_off(struct platform_device *pdev)
{
	return 0;
}

static int __init mddi_orise_probe(struct platform_device *pdev)
{
	msm_fb_add_device(pdev);
	return 0;
}

static int __init mddi_orise_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
	u32 id;
	ret = msm_fb_detect_client("mddi_orise");
	if (ret == -ENODEV)
		return 0;

	if (ret) {
		id = mddi_get_client_id();
		if (((id >> 16) != 0xbe8d) || ((id & 0xffff) != 0x8031))
			return 0;
	}
#endif
	ret = platform_driver_probe(&this_driver, mddi_orise_probe);
	if (!ret) {
		pinfo = &mddi_orise_panel_data.panel_info;
		pinfo->xres = 480;
		pinfo->yres = 800;
		MSM_FB_SINGLE_MODE_PANEL(pinfo);
		pinfo->type = MDDI_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->mddi.is_type1 = TRUE;
		pinfo->mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
		pinfo->wait_cycle = 0;
		pinfo->bpp = 18;
		pinfo->fb_num = 2;
		pinfo->clk_rate = 192000000;
		pinfo->clk_min = 192000000;
		pinfo->clk_max = 192000000;
		pinfo->lcd.rev = 2;
		pinfo->lcd.vsync_enable = FALSE;
		pinfo->lcd.refx100 = 6050;
		pinfo->lcd.v_back_porch = 2;
		pinfo->lcd.v_front_porch = 2;
		pinfo->lcd.v_pulse_width = 105;
		pinfo->lcd.hw_vsync_mode = TRUE;
		pinfo->lcd.vsync_notifier_period = 0;

		ret = platform_device_register(&this_device);
		if (ret)
			platform_driver_unregister(&this_driver);
	}
	return ret;
}
module_init(mddi_orise_init);
