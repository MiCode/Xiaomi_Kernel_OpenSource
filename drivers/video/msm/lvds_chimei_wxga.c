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
#include <linux/pwm.h>
#include <linux/mfd/pm8xxx/pm8921.h>

#define LVDS_CHIMEI_PWM_FREQ_HZ 300
#define LVDS_CHIMEI_PWM_PERIOD_USEC (USEC_PER_SEC / LVDS_CHIMEI_PWM_FREQ_HZ)
#define LVDS_CHIMEI_PWM_LEVEL 255
#define LVDS_CHIMEI_PWM_DUTY_LEVEL \
	(LVDS_CHIMEI_PWM_PERIOD_USEC / LVDS_CHIMEI_PWM_LEVEL)


static struct lvds_panel_platform_data *cm_pdata;
static struct platform_device *cm_fbpdev;
static struct pwm_device *bl_lpm;

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
	int ret;

	pr_debug("%s: back light level %d\n", __func__, mfd->bl_level);

	if (bl_lpm) {
		ret = pwm_config(bl_lpm, LVDS_CHIMEI_PWM_DUTY_LEVEL *
			mfd->bl_level, LVDS_CHIMEI_PWM_PERIOD_USEC);
		if (ret) {
			pr_err("pwm_config on lpm failed %d\n", ret);
			return;
		}
		if (mfd->bl_level) {
			ret = pwm_enable(bl_lpm);
			if (ret)
				pr_err("pwm enable/disable on lpm failed"
					"for bl %d\n",	mfd->bl_level);
		} else {
			pwm_disable(bl_lpm);
		}
	}
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

	if (cm_pdata != NULL)
		bl_lpm = pwm_request(cm_pdata->gpio[0],
			"backlight");

	if (bl_lpm == NULL || IS_ERR(bl_lpm)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_lpm = NULL;
	}
	pr_debug("bl_lpm = %p lpm = %d\n", bl_lpm,
		cm_pdata->gpio[0]);

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
	pinfo->xres = 1366;
	pinfo->yres = 768;
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
	pinfo->type = LVDS_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 79400000;
	pinfo->bl_max = 255;
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
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;
	pinfo->lvds.channel_mode = LVDS_SINGLE_CHANNEL_MODE;

	/* Set border color, padding only for reducing active display region */
	pinfo->lcdc.border_clr = 0x0;
	pinfo->lcdc.xres_pad = 0;
	pinfo->lcdc.yres_pad = 0;

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lvds_chimei_wxga_init);
