/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <linux/pm_runtime.h>
#include <mach/clk.h>

#include "msm_fb.h"
#include "mdp4.h"

static int dtv_probe(struct platform_device *pdev);
static int dtv_remove(struct platform_device *pdev);

static int dtv_off(struct platform_device *pdev);
static int dtv_on(struct platform_device *pdev);

static struct platform_device *pdev_list[MSM_FB_MAX_DEV_LIST];
static int pdev_list_cnt;

static struct clk *tv_src_clk;
static struct clk *hdmi_clk;
static struct clk *mdp_tv_clk;


static int mdp4_dtv_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int mdp4_dtv_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops mdp4_dtv_dev_pm_ops = {
	.runtime_suspend = mdp4_dtv_runtime_suspend,
	.runtime_resume = mdp4_dtv_runtime_resume,
};

static struct platform_driver dtv_driver = {
	.probe = dtv_probe,
	.remove = dtv_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "dtv",
		   .pm = &mdp4_dtv_dev_pm_ops,
		   },
};

static struct lcdc_platform_data *dtv_pdata;
#ifdef CONFIG_MSM_BUS_SCALING
static uint32_t dtv_bus_scale_handle;
#else
static struct clk *ebi1_clk;
#endif

static int dtv_off(struct platform_device *pdev)
{
	int ret = 0;

	ret = panel_next_off(pdev);

	pr_info("%s\n", __func__);

	clk_disable(hdmi_clk);
	if (mdp_tv_clk)
		clk_disable(mdp_tv_clk);

	if (dtv_pdata && dtv_pdata->lcdc_power_save)
		dtv_pdata->lcdc_power_save(0);

	if (dtv_pdata && dtv_pdata->lcdc_gpio_config)
		ret = dtv_pdata->lcdc_gpio_config(0);
#ifdef CONFIG_MSM_BUS_SCALING
	if (dtv_bus_scale_handle > 0)
		msm_bus_scale_client_update_request(dtv_bus_scale_handle,
							0);
#else
	if (ebi1_clk)
		clk_disable(ebi1_clk);
#endif
	mdp4_extn_disp = 0;
	mdp_footswitch_ctrl(FALSE);
	return ret;
}

static int dtv_on(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_fb_data_type *mfd;
	unsigned long panel_pixclock_freq , pm_qos_rate;

	mfd = platform_get_drvdata(pdev);
	panel_pixclock_freq = mfd->fbi->var.pixclock;

	if (panel_pixclock_freq > 58000000)
		/* pm_qos_rate should be in Khz */
		pm_qos_rate = panel_pixclock_freq / 1000 ;
	else
		pm_qos_rate = 58000;
	mdp_footswitch_ctrl(TRUE);
	mdp4_extn_disp = 1;
#ifdef CONFIG_MSM_BUS_SCALING
	if (dtv_bus_scale_handle > 0)
		msm_bus_scale_client_update_request(dtv_bus_scale_handle,
							1);
#else
	if (ebi1_clk) {
		clk_set_rate(ebi1_clk, pm_qos_rate * 1000);
		clk_enable(ebi1_clk);
	}
#endif
	mfd = platform_get_drvdata(pdev);

	ret = clk_set_rate(tv_src_clk, mfd->fbi->var.pixclock);
	if (ret) {
		pr_info("%s: clk_set_rate(%d) failed\n", __func__,
			mfd->fbi->var.pixclock);
		if (mfd->fbi->var.pixclock == 27030000)
			mfd->fbi->var.pixclock = 27000000;
		ret = clk_set_rate(tv_src_clk, mfd->fbi->var.pixclock);
	}
	pr_info("%s: tv_src_clk=%dkHz, pm_qos_rate=%ldkHz, [%d]\n", __func__,
		mfd->fbi->var.pixclock/1000, pm_qos_rate, ret);

	clk_enable(hdmi_clk);
	clk_reset(hdmi_clk, CLK_RESET_ASSERT);
	udelay(20);
	clk_reset(hdmi_clk, CLK_RESET_DEASSERT);

	if (mdp_tv_clk)
		clk_enable(mdp_tv_clk);

	if (dtv_pdata && dtv_pdata->lcdc_power_save)
		dtv_pdata->lcdc_power_save(1);
	if (dtv_pdata && dtv_pdata->lcdc_gpio_config)
		ret = dtv_pdata->lcdc_gpio_config(1);

	ret = panel_next_on(pdev);
	return ret;
}

static int dtv_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct fb_info *fbi;
	struct platform_device *mdp_dev = NULL;
	struct msm_fb_panel_data *pdata = NULL;
	int rc;

	if (pdev->id == 0) {
		dtv_pdata = pdev->dev.platform_data;
		return 0;
	}

	mfd = platform_get_drvdata(pdev);

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
		pr_err("dtv_probe: platform_device_add_data failed!\n");
		platform_device_put(mdp_dev);
		return -ENOMEM;
	}
	/*
	 * data chain
	 */
	pdata = (struct msm_fb_panel_data *)mdp_dev->dev.platform_data;
	pdata->on = dtv_on;
	pdata->off = dtv_off;
	pdata->next = pdev;

	/*
	 * get/set panel specific fb info
	 */
	mfd->panel_info = pdata->panel_info;
	if (hdmi_prim_display)
		mfd->fb_imgType = MSMFB_DEFAULT_TYPE;
	else
		mfd->fb_imgType = MDP_RGB_565;

	fbi = mfd->fbi;
	fbi->var.pixclock = mfd->panel_info.clk_rate;
	fbi->var.left_margin = mfd->panel_info.lcdc.h_back_porch;
	fbi->var.right_margin = mfd->panel_info.lcdc.h_front_porch;
	fbi->var.upper_margin = mfd->panel_info.lcdc.v_back_porch;
	fbi->var.lower_margin = mfd->panel_info.lcdc.v_front_porch;
	fbi->var.hsync_len = mfd->panel_info.lcdc.h_pulse_width;
	fbi->var.vsync_len = mfd->panel_info.lcdc.v_pulse_width;

#ifdef CONFIG_MSM_BUS_SCALING
	if (!dtv_bus_scale_handle && dtv_pdata &&
		dtv_pdata->bus_scale_table) {
		dtv_bus_scale_handle =
			msm_bus_scale_register_client(
					dtv_pdata->bus_scale_table);
		if (!dtv_bus_scale_handle) {
			pr_err("%s not able to get bus scale\n",
				__func__);
		}
	}
#else
	ebi1_clk = clk_get(NULL, "ebi1_dtv_clk");
	if (IS_ERR(ebi1_clk)) {
		ebi1_clk = NULL;
		pr_warning("%s: Couldn't get ebi1 clock\n", __func__);
	}
#endif
	/*
	 * set driver data
	 */
	platform_set_drvdata(mdp_dev, mfd);

	/*
	 * register in mdp driver
	 */
	rc = platform_device_add(mdp_dev);
	if (rc)
		goto dtv_probe_err;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	pdev_list[pdev_list_cnt++] = pdev;
	return 0;

dtv_probe_err:
#ifdef CONFIG_MSM_BUS_SCALING
	if (dtv_pdata && dtv_pdata->bus_scale_table &&
		dtv_bus_scale_handle > 0)
		msm_bus_scale_unregister_client(dtv_bus_scale_handle);
#endif
	platform_device_put(mdp_dev);
	return rc;
}

static int dtv_remove(struct platform_device *pdev)
{
#ifdef CONFIG_MSM_BUS_SCALING
	if (dtv_pdata && dtv_pdata->bus_scale_table &&
		dtv_bus_scale_handle > 0)
		msm_bus_scale_unregister_client(dtv_bus_scale_handle);
#else
	if (ebi1_clk)
		clk_put(ebi1_clk);
#endif
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static int dtv_register_driver(void)
{
	return platform_driver_register(&dtv_driver);
}

static int __init dtv_driver_init(void)
{
	tv_src_clk = clk_get(NULL, "tv_src_clk");
	if (IS_ERR(tv_src_clk)) {
		pr_err("error: can't get tv_src_clk!\n");
		return IS_ERR(tv_src_clk);
	}

	hdmi_clk = clk_get(NULL, "hdmi_clk");
	if (IS_ERR(hdmi_clk)) {
		pr_err("error: can't get hdmi_clk!\n");
		return IS_ERR(hdmi_clk);
	}

	mdp_tv_clk = clk_get(NULL, "mdp_tv_clk");
	if (IS_ERR(mdp_tv_clk))
		mdp_tv_clk = NULL;

	return dtv_register_driver();
}

module_init(dtv_driver_init);
