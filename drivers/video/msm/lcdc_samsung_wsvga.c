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
 */

#include <linux/delay.h>
#include <linux/pwm.h>
#ifdef CONFIG_PMIC8058_PWM
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-pwm.h>
#endif
#include <mach/gpio.h>
#include "msm_fb.h"



#ifdef CONFIG_PMIC8058_PWM
static struct pwm_device *bl_pwm0;
static struct pwm_device *bl_pwm1;

/* for samsung panel 300hz was the minimum freq where flickering wasnt
 * observed as the screen was dimmed
 */

#define PWM_FREQ_HZ 300
#define PWM_PERIOD_USEC (USEC_PER_SEC / PWM_FREQ_HZ)
#define PWM_LEVEL 100
#define PWM_DUTY_LEVEL (PWM_PERIOD_USEC / PWM_LEVEL)
#endif

struct lcdc_samsung_data {
	struct msm_panel_common_pdata *pdata;
#ifdef CONFIG_FB_MSM_LCDC_DSUB
	int vga_enabled;
#endif
	struct platform_device *fbpdev;
};

static struct lcdc_samsung_data *dd;


static void lcdc_samsung_panel_set_backlight(struct msm_fb_data_type *mfd)
{
	int bl_level;
	int ret;

	bl_level = mfd->bl_level;

#ifdef CONFIG_PMIC8058_PWM
	if (bl_pwm0) {
		ret = pwm_config(bl_pwm0, PWM_DUTY_LEVEL * bl_level,
			PWM_PERIOD_USEC);
		if (ret)
			printk(KERN_ERR "pwm_config on pwm 0 failed %d\n", ret);
	}

	if (bl_pwm1) {
		ret = pwm_config(bl_pwm1,
			PWM_PERIOD_USEC - (PWM_DUTY_LEVEL * bl_level),
			PWM_PERIOD_USEC);
		if (ret)
			printk(KERN_ERR "pwm_config on pwm 1 failed %d\n", ret);
	}

	if (bl_pwm0) {
		ret = pwm_enable(bl_pwm0);
		if (ret)
			printk(KERN_ERR "pwm_enable on pwm 0 failed %d\n", ret);
	}

	if (bl_pwm1) {
		ret = pwm_enable(bl_pwm1);
		if (ret)
			printk(KERN_ERR "pwm_enable on pwm 1 failed %d\n", ret);
	}
#endif

}

#ifdef CONFIG_FB_MSM_LCDC_DSUB
static ssize_t show_vga_enable(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", dd->vga_enabled);
}

static ssize_t store_vga_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long enable;
	int rc;

	rc = strict_strtoul(buf, 10, &enable);
	if (rc)
		return -EINVAL;

	if (dd->pdata && dd->pdata->vga_switch)
		rc = dd->pdata->vga_switch(enable);
	else
		rc = -ENODEV;
	if (!rc) {
		dd->vga_enabled = enable;
		rc = count;
	}
	return rc;
}

static DEVICE_ATTR(vga_enable, S_IRUGO|S_IWUSR, show_vga_enable,
		   store_vga_enable);
static struct attribute *attrs[] = {
	&dev_attr_vga_enable.attr,
	NULL,
};
static struct attribute_group attr_group = {
	.attrs = attrs,
};
#endif

static int __devinit samsung_probe(struct platform_device *pdev)
{
	int rc = 0;
#ifdef CONFIG_FB_MSM_LCDC_DSUB
	struct msm_fb_data_type *mfd;
#endif

	if (pdev->id == 0) {
		dd = kzalloc(sizeof *dd, GFP_KERNEL);
		if (!dd)
			return -ENOMEM;
#ifdef CONFIG_FB_MSM_LCDC_DSUB
		dd->vga_enabled = 0;
#endif
		dd->pdata = pdev->dev.platform_data;
		return 0;
	} else if (!dd)
		return -ENODEV;

#ifdef CONFIG_PMIC8058_PWM
	bl_pwm0 = pwm_request(dd->pdata->gpio_num[0], "backlight1");
	if (bl_pwm0 == NULL || IS_ERR(bl_pwm0)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_pwm0 = NULL;
	}

	bl_pwm1 = pwm_request(dd->pdata->gpio_num[1], "backlight2");
	if (bl_pwm1 == NULL || IS_ERR(bl_pwm1)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_pwm1 = NULL;
	}

	pr_debug("samsung_probe: bl_pwm0=%p LPG_chan0=%d "
			"bl_pwm1=%p LPG_chan1=%d\n",
			bl_pwm0, (int)dd->pdata->gpio_num[0],
			bl_pwm1, (int)dd->pdata->gpio_num[1]
			);
#endif


	dd->fbpdev = msm_fb_add_device(pdev);
	if (!dd->fbpdev) {
		dev_err(&pdev->dev, "failed to add msm_fb device\n");
		rc = -ENODEV;
		goto probe_exit;
	}

#ifdef CONFIG_FB_MSM_LCDC_DSUB
	mfd = platform_get_drvdata(dd->fbpdev);
	if (mfd && mfd->fbi && mfd->fbi->dev) {
		rc = sysfs_create_group(&mfd->fbi->dev->kobj, &attr_group);
		if (rc)
			dev_err(&pdev->dev, "failed to create sysfs group\n");
	} else {
		dev_err(&pdev->dev, "no dev to create sysfs group\n");
		rc = -ENODEV;
	}
#endif

probe_exit:
	return rc;
}

#ifdef CONFIG_FB_MSM_LCDC_DSUB
static int __devexit samsung_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&dd->fbpdev->dev.kobj, &attr_group);
	return 0;
}
#endif

static struct platform_driver this_driver = {
	.probe  = samsung_probe,
#ifdef CONFIG_FB_MSM_LCDC_DSUB
	.remove = samsung_remove,
#endif
	.driver = {
		.name   = "lcdc_samsung_wsvga",
	},
};

static struct msm_fb_panel_data samsung_panel_data = {
	.set_backlight = lcdc_samsung_panel_set_backlight,
};

static struct platform_device this_device = {
	.name   = "lcdc_samsung_wsvga",
	.id	= 1,
	.dev	= {
		.platform_data = &samsung_panel_data,
	}
};

static int __init lcdc_samsung_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_LCDC_AUTO_DETECT
	if (msm_fb_detect_client("lcdc_samsung_wsvga"))
		return 0;
#endif

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &samsung_panel_data.panel_info;
	pinfo->xres = 1024;
	pinfo->yres = 600;
#ifdef CONFIG_FB_MSM_LCDC_DSUB
	/* DSUB (VGA) is on the same bus, this allows us to allocate for the
	 * max resolution of the DSUB display */
	pinfo->mode2_xres = 1440;
	pinfo->mode2_yres = 900;
	pinfo->mode2_bpp = 16;
#else
	MSM_FB_SINGLE_MODE_PANEL(pinfo);
#endif
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 43192000;
	pinfo->bl_max = PWM_LEVEL;
	pinfo->bl_min = 1;

	pinfo->lcdc.h_back_porch = 80;
	pinfo->lcdc.h_front_porch = 48;
	pinfo->lcdc.h_pulse_width = 32;
	pinfo->lcdc.v_back_porch = 4;
	pinfo->lcdc.v_front_porch = 3;
	pinfo->lcdc.v_pulse_width = 1;
	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lcdc_samsung_panel_init);
