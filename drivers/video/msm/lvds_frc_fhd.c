/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <mach/gpio.h>
#include "msm_fb.h"

static struct lvds_panel_platform_data *frc_pdata;
static struct platform_device *frc_fbpdev;
static int gpio_update;		/* 268 */
static int gpio_reset;	/* 269 */
static int gpio_pwr;		/* 270 */

static int lvds_frc_panel_on(struct platform_device *pdev)
{
	int ret;

	ret = gpio_request(gpio_pwr, "frc_pwr");
	if (ret) {
		pr_err("%s: gpio_pwr=%d, gpio_request failed\n",
			__func__, gpio_pwr);
		goto panel_on_exit;
	}
	ret = gpio_request(gpio_update, "frc_update");
	if (ret) {
		pr_err("%s: gpio_update=%d, gpio_request failed\n",
			__func__, gpio_update);
		goto panel_on_exit1;
	}
	ret = gpio_request(gpio_reset, "frc_reset");
	if (ret) {
		pr_err("%s: gpio_reset=%d, gpio_request failed\n",
			__func__, gpio_reset);
		goto panel_on_exit2;
	}

	gpio_direction_output(gpio_reset, 1);
	gpio_direction_output(gpio_pwr, 0);
	gpio_direction_output(gpio_update, 0);
	usleep(1000);
	gpio_direction_output(gpio_reset, 0);
	usleep(1000);
	gpio_direction_output(gpio_pwr, 1);
	usleep(1000);
	gpio_direction_output(gpio_update, 1);
	usleep(1000);
	gpio_direction_output(gpio_reset, 1);
	usleep(1000);
	gpio_free(gpio_reset);
panel_on_exit2:
	gpio_free(gpio_update);
panel_on_exit1:
	gpio_free(gpio_pwr);
panel_on_exit:
	return ret;
}

static int lvds_frc_panel_off(struct platform_device *pdev)
{
	int ret;

	ret = gpio_request(gpio_pwr, "frc_pwr");
	if (ret) {
		pr_err("%s: gpio_pwr=%d, gpio_request failed\n",
			__func__, gpio_pwr);
		goto panel_off_exit;
	}
	ret = gpio_request(gpio_update, "frc_update");
	if (ret) {
		pr_err("%s: gpio_update=%d, gpio_request failed\n",
			__func__, gpio_update);
		goto panel_off_exit1;
	}
	ret = gpio_request(gpio_reset, "frc_reset");
	if (ret) {
		pr_err("%s: gpio_reset=%d, gpio_request failed\n",
			__func__, gpio_reset);
		goto panel_off_exit2;
	}
	gpio_direction_output(gpio_reset, 0);
	usleep(1000);
	gpio_direction_output(gpio_update, 0);
	usleep(1000);
	gpio_direction_output(gpio_pwr, 0);
	usleep(1000);
	gpio_free(gpio_reset);
panel_off_exit2:
	gpio_free(gpio_update);
panel_off_exit1:
	gpio_free(gpio_pwr);
panel_off_exit:
	return ret;
}

static int __devinit lvds_frc_probe(struct platform_device *pdev)
{
	int rc = 0;

	if (pdev->id == 0) {
		frc_pdata = pdev->dev.platform_data;
		if (frc_pdata != NULL) {
			gpio_update = frc_pdata->gpio[0];
			gpio_reset = frc_pdata->gpio[1];
			gpio_pwr = frc_pdata->gpio[2];
			pr_info("%s: power=%d update=%d reset=%d\n",
				__func__, gpio_pwr, gpio_update, gpio_reset);
		}
		return 0;
	}

	frc_fbpdev = msm_fb_add_device(pdev);
	if (!frc_fbpdev) {
		dev_err(&pdev->dev, "failed to add msm_fb device\n");
		rc = -ENODEV;
		goto probe_exit;
	}

probe_exit:
	return rc;
}

static struct platform_driver this_driver = {
	.probe  = lvds_frc_probe,
	.driver = {
		.name   = "lvds_frc_fhd",
	},
};

static struct msm_fb_panel_data lvds_frc_panel_data = {
	.on = lvds_frc_panel_on,
	.off = lvds_frc_panel_off,
};

static struct platform_device this_device = {
	.name   = "lvds_frc_fhd",
	.id	= 1,
	.dev	= {
		.platform_data = &lvds_frc_panel_data,
	}
};

static int __init lvds_frc_fhd_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	if (msm_fb_detect_client("lvds_frc_fhd"))
		return 0;

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &lvds_frc_panel_data.panel_info;
	pinfo->xres = 1920;
	pinfo->yres = 1080;
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
	pinfo->type = LVDS_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 74250000;
	pinfo->bl_max = 255;
	pinfo->bl_min = 1;

	/*
	 * use hdmi 1080p60 setting, for dual channel mode,
	 * horizontal length is half.
	 */
	pinfo->lcdc.h_back_porch = 148/2;
	pinfo->lcdc.h_front_porch = 88/2;
	pinfo->lcdc.h_pulse_width = 44/2;
	pinfo->lcdc.v_back_porch = 36;
	pinfo->lcdc.v_front_porch = 4;
	pinfo->lcdc.v_pulse_width = 5;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;
	pinfo->lvds.channel_mode = LVDS_DUAL_CHANNEL_MODE;
	pinfo->lcdc.is_sync_active_high = TRUE;

	/* Set border color, padding only for reducing active display region */
	pinfo->lcdc.border_clr = 0x0;
	pinfo->lcdc.xres_pad = 0;
	pinfo->lcdc.yres_pad = 0;

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lvds_frc_fhd_init);
