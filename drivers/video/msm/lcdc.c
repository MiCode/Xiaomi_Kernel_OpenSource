/* Copyright (c) 2008-2012, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "msm_fb.h"

static int lcdc_probe(struct platform_device *pdev);
static int lcdc_remove(struct platform_device *pdev);

static int lcdc_off(struct platform_device *pdev);
static int lcdc_on(struct platform_device *pdev);

static struct platform_device *pdev_list[MSM_FB_MAX_DEV_LIST];
static int pdev_list_cnt;

static struct clk *pixel_mdp_clk; /* drives the lcdc block in mdp */
static struct clk *pixel_lcdc_clk; /* drives the lcdc interface */

static struct platform_driver lcdc_driver = {
	.probe = lcdc_probe,
	.remove = lcdc_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "lcdc",
		   },
};

static struct lcdc_platform_data *lcdc_pdata;

static int lcdc_off(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);
	ret = panel_next_off(pdev);

	clk_disable_unprepare(pixel_mdp_clk);
	clk_disable_unprepare(pixel_lcdc_clk);

	if (lcdc_pdata && lcdc_pdata->lcdc_power_save)
		lcdc_pdata->lcdc_power_save(0);

	if (lcdc_pdata && lcdc_pdata->lcdc_gpio_config)
		ret = lcdc_pdata->lcdc_gpio_config(0);

#ifndef CONFIG_MSM_BUS_SCALING
	if (mfd->ebi1_clk) {
		if (mdp_rev == MDP_REV_303) {
			if (clk_set_rate(mfd->ebi1_clk, 0))
				pr_err("%s: ebi1_lcdc_clk set rate failed\n",
					__func__);
		}
		clk_disable_unprepare(mfd->ebi1_clk);
	}
#else
	mdp_bus_scale_update_request(0);
#endif

	return ret;
}

static int lcdc_on(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_fb_data_type *mfd;
	unsigned long panel_pixclock_freq = 0;
#ifndef CONFIG_MSM_BUS_SCALING
	unsigned long pm_qos_rate;
#endif
	mfd = platform_get_drvdata(pdev);

	if (lcdc_pdata && lcdc_pdata->lcdc_get_clk)
		panel_pixclock_freq = lcdc_pdata->lcdc_get_clk();

	if (!panel_pixclock_freq)
		panel_pixclock_freq = mfd->fbi->var.pixclock;
#ifdef CONFIG_MSM_BUS_SCALING
	mdp_bus_scale_update_request(2);
#else
	if (panel_pixclock_freq > 65000000)
		/* pm_qos_rate should be in Khz */
		pm_qos_rate = panel_pixclock_freq / 1000 ;
	else
		pm_qos_rate = 65000;

	if (mfd->ebi1_clk) {
		if (mdp_rev == MDP_REV_303) {
			if (clk_set_rate(mfd->ebi1_clk, 65000000))
				pr_err("%s: ebi1_lcdc_clk set rate failed\n",
					__func__);
		} else {
			clk_set_rate(mfd->ebi1_clk, pm_qos_rate * 1000);
		}
		clk_prepare_enable(mfd->ebi1_clk);
	}

#endif
	mfd = platform_get_drvdata(pdev);

	mfd->fbi->var.pixclock = clk_round_rate(pixel_mdp_clk,
					mfd->fbi->var.pixclock);
	ret = clk_set_rate(pixel_mdp_clk, mfd->fbi->var.pixclock);
	if (ret) {
		pr_err("%s: Can't set MDP LCDC pixel clock to rate %u\n",
			__func__, mfd->fbi->var.pixclock);
		goto out;
	}

	clk_prepare_enable(pixel_mdp_clk);
	clk_prepare_enable(pixel_lcdc_clk);

	if (lcdc_pdata && lcdc_pdata->lcdc_power_save)
		lcdc_pdata->lcdc_power_save(1);
	if (lcdc_pdata && lcdc_pdata->lcdc_gpio_config)
		ret = lcdc_pdata->lcdc_gpio_config(1);

	ret = panel_next_on(pdev);

out:
	return ret;
}

static int lcdc_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct fb_info *fbi;
	struct platform_device *mdp_dev = NULL;
	struct msm_fb_panel_data *pdata = NULL;
	int rc;
	struct clk *ebi1_clk = NULL;

	if (pdev->id == 0) {
		lcdc_pdata = pdev->dev.platform_data;
		pixel_mdp_clk = clk_get(&pdev->dev, "mdp_clk");
		if (IS_ERR(pixel_mdp_clk)) {
			pr_err("Couldnt find pixel_mdp_clk\n");
			return -EINVAL;
		}

		pixel_lcdc_clk = clk_get(&pdev->dev, "lcdc_clk");
		if (IS_ERR(pixel_lcdc_clk)) {
			pr_err("Couldnt find pixel_lcdc_clk\n");
			return -EINVAL;
		}

#ifndef CONFIG_MSM_BUS_SCALING
		ebi1_clk = clk_get(&pdev->dev, "mem_clk");
		if (IS_ERR(ebi1_clk))
			return PTR_ERR(ebi1_clk);
#endif

		return 0;
	}

	mfd = platform_get_drvdata(pdev);
	mfd->ebi1_clk = ebi1_clk;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (pdev_list_cnt >= MSM_FB_MAX_DEV_LIST)
		return -ENOMEM;

	mdp_dev = platform_device_alloc("mdp", pdev->id);
	if (!mdp_dev)
		return -ENOMEM;

	/*
	 * link to the latest pdev
	 */
	mfd->pdev = mdp_dev;
	mfd->dest = DISPLAY_LCDC;

	/*
	 * alloc panel device data
	 */
	if (platform_device_add_data
	    (mdp_dev, pdev->dev.platform_data,
	     sizeof(struct msm_fb_panel_data))) {
		pr_err("lcdc_probe: platform_device_add_data failed!\n");
		platform_device_put(mdp_dev);
		return -ENOMEM;
	}
	/*
	 * data chain
	 */
	pdata = (struct msm_fb_panel_data *)mdp_dev->dev.platform_data;
	pdata->on = lcdc_on;
	pdata->off = lcdc_off;
	pdata->next = pdev;

	/*
	 * get/set panel specific fb info
	 */
	mfd->panel_info = pdata->panel_info;

	if (mfd->index == 0)
		mfd->fb_imgType = MSMFB_DEFAULT_TYPE;
	else
		mfd->fb_imgType = MDP_RGB_565;

	fbi = mfd->fbi;
	fbi->var.pixclock = clk_round_rate(pixel_mdp_clk,
					mfd->panel_info.clk_rate);
	fbi->var.left_margin = mfd->panel_info.lcdc.h_back_porch;
	fbi->var.right_margin = mfd->panel_info.lcdc.h_front_porch;
	fbi->var.upper_margin = mfd->panel_info.lcdc.v_back_porch;
	fbi->var.lower_margin = mfd->panel_info.lcdc.v_front_porch;
	fbi->var.hsync_len = mfd->panel_info.lcdc.h_pulse_width;
	fbi->var.vsync_len = mfd->panel_info.lcdc.v_pulse_width;

	/*
	 * set driver data
	 */
	platform_set_drvdata(mdp_dev, mfd);
	/*
	 * register in mdp driver
	 */
	rc = platform_device_add(mdp_dev);
	if (rc)
		goto lcdc_probe_err;

	pdev_list[pdev_list_cnt++] = pdev;

	return 0;

lcdc_probe_err:
	platform_device_put(mdp_dev);
	return rc;
}

static int lcdc_remove(struct platform_device *pdev)
{
#ifndef CONFIG_MSM_BUS_SCALING
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	clk_put(mfd->ebi1_clk);
#endif
	return 0;
}

static int lcdc_register_driver(void)
{
	return platform_driver_register(&lcdc_driver);
}

static int __init lcdc_driver_init(void)
{

	return lcdc_register_driver();
}

module_init(lcdc_driver_init);
