/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/pwm.h>
#ifdef CONFIG_PMIC8058_PWM
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-pwm.h>
#endif
#include <mach/gpio.h>
#include "msm_fb.h"




static struct pwm_device *bl_pwm;

#define PWM_FREQ_HZ 210
#define PWM_PERIOD_USEC (USEC_PER_SEC / PWM_FREQ_HZ)
#define PWM_DUTY_LEVEL (PWM_PERIOD_USEC / PWM_LEVEL)
#define PWM_LEVEL 15

static struct msm_panel_common_pdata *cm_pdata;
static struct platform_device *cm_fbpdev;
static int led_pwm;		/* pm8058 gpio 24, channel 0 */
static int led_en;		/* pm8058 gpio 1 */
static int lvds_pwr_down;	/* msm gpio 30 */
static int chimei_bl_level = 1;


static void lcdc_chimei_set_backlight(int level)
{
	int ret;

	if (bl_pwm) {
		ret = pwm_config(bl_pwm, PWM_DUTY_LEVEL * level,
			PWM_PERIOD_USEC);
		if (ret) {
			pr_err("%s: pwm_config on pwm failed %d\n",
					__func__, ret);
			return;
		}

		ret = pwm_enable(bl_pwm);
		if (ret) {
			pr_err("%s: pwm_enable on pwm failed %d\n",
					__func__, ret);
			return;
		}
	}

	chimei_bl_level = level;
}

static int lcdc_chimei_panel_on(struct platform_device *pdev)
{
	int ret;

	/* panel powered on here */

	ret = gpio_request(lvds_pwr_down, "lvds_pwr_down");
	if (ret == 0) {
		/* output, pull high to enable */
		gpio_direction_output(lvds_pwr_down, 1);
	} else {
		pr_err("%s: lvds_pwr_down=%d, gpio_request failed\n",
			__func__, lvds_pwr_down);
	}

	msleep(200);
	/* power on led pwm power >= 200 ms */

	if (chimei_bl_level == 0)
		chimei_bl_level = 1;
	lcdc_chimei_set_backlight(chimei_bl_level);

	msleep(10);

	ret = gpio_request(led_en, "led_en");
	if (ret == 0) {
		/* output, pull high */
		gpio_direction_output(led_en, 1);
	} else {
		pr_err("%s: led_en=%d, gpio_request failed\n",
			__func__, led_en);
	}
	return ret;
}

static int lcdc_chimei_panel_off(struct platform_device *pdev)
{
	/* pull low to disable */
	gpio_set_value_cansleep(led_en, 0);
	gpio_free(led_en);

	msleep(10);

	lcdc_chimei_set_backlight(0);

	msleep(200);
	/* power off led pwm power >= 200 ms */

	/* pull low to shut down lvds */
	gpio_set_value_cansleep(lvds_pwr_down, 0);
	gpio_free(lvds_pwr_down);

	/* panel power off here */

	return 0;
}

static void lcdc_chimei_panel_backlight(struct msm_fb_data_type *mfd)
{
	lcdc_chimei_set_backlight(mfd->bl_level);
}

static int __devinit chimei_probe(struct platform_device *pdev)
{
	int rc = 0;

	if (pdev->id == 0) {
		cm_pdata = pdev->dev.platform_data;
		if (cm_pdata == NULL) {
			pr_err("%s: no PWM gpio specified\n", __func__);
			return 0;
		}
		led_pwm = cm_pdata->gpio_num[0];
		led_en = cm_pdata->gpio_num[1];
		lvds_pwr_down = cm_pdata->gpio_num[2];
		pr_info("%s: led_pwm=%d led_en=%d lvds_pwr_down=%d\n",
			__func__, led_pwm, led_en, lvds_pwr_down);
		return 0;
	}

	if (cm_pdata == NULL)
		return -ENODEV;

	bl_pwm = pwm_request(led_pwm, "backlight");
	if (bl_pwm == NULL || IS_ERR(bl_pwm)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_pwm = NULL;
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
	.probe  = chimei_probe,
	.driver = {
		.name   = "lcdc_chimei_lvds_wxga",
	},
};

static struct msm_fb_panel_data chimei_panel_data = {
	.on = lcdc_chimei_panel_on,
	.off = lcdc_chimei_panel_off,
	.set_backlight = lcdc_chimei_panel_backlight,
};

static struct platform_device this_device = {
	.name   = "lcdc_chimei_lvds_wxga",
	.id	= 1,
	.dev	= {
		.platform_data = &chimei_panel_data,
	}
};

static int __init lcdc_chimei_lvds_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	if (msm_fb_detect_client("lcdc_chimei_wxga"))
		return 0;

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &chimei_panel_data.panel_info;
	pinfo->xres = 1366;
	pinfo->yres = 768;
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 69300000;
	pinfo->bl_max = PWM_LEVEL;
	pinfo->bl_min = 1;

	/*
	 * this panel is operated by de,
	 * vsycn and hsync are ignored
	 */
	pinfo->lcdc.h_back_porch = 108;
	pinfo->lcdc.h_front_porch = 0;
	pinfo->lcdc.h_pulse_width = 1;
	pinfo->lcdc.v_back_porch = 0;
	pinfo->lcdc.v_front_porch = 16;
	pinfo->lcdc.v_pulse_width = 1;
	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lcdc_chimei_lvds_panel_init);
