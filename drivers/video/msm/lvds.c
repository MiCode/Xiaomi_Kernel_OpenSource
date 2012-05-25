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
#include <mach/hardware.h>
#include <mach/msm_iomap.h>
#include <mach/clk.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "msm_fb.h"
#include "mdp4.h"
static int lvds_probe(struct platform_device *pdev);
static int lvds_remove(struct platform_device *pdev);

static int lvds_off(struct platform_device *pdev);
static int lvds_on(struct platform_device *pdev);

static struct platform_device *pdev_list[MSM_FB_MAX_DEV_LIST];
static int pdev_list_cnt;

static struct clk *lvds_clk;

static struct platform_driver lvds_driver = {
	.probe = lvds_probe,
	.remove = lvds_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "lvds",
		   },
};

static struct lcdc_platform_data *lvds_pdata;

static void lvds_init(struct msm_fb_data_type *mfd)
{
	unsigned int lvds_intf, lvds_phy_cfg0;

	MDP_OUTP(MDP_BASE + 0xc2034, 0x33);
	usleep(1000);

	/* LVDS PHY PLL configuration */
	MDP_OUTP(MDP_BASE + 0xc3004, 0x62);
	MDP_OUTP(MDP_BASE + 0xc3008, 0x30);
	MDP_OUTP(MDP_BASE + 0xc300c, 0xc4);
	MDP_OUTP(MDP_BASE + 0xc3014, 0x10);
	MDP_OUTP(MDP_BASE + 0xc3018, 0x05);
	MDP_OUTP(MDP_BASE + 0xc301c, 0x62);
	MDP_OUTP(MDP_BASE + 0xc3020, 0x41);
	MDP_OUTP(MDP_BASE + 0xc3024, 0x0d);

	MDP_OUTP(MDP_BASE + 0xc3000, 0x01);
	/* Wait until LVDS PLL is locked and ready */
	while (!readl_relaxed(MDP_BASE + 0xc3080))
		cpu_relax();

	writel_relaxed(0x00, mmss_cc_base + 0x0264);
	writel_relaxed(0x00, mmss_cc_base + 0x0094);

	writel_relaxed(0x02, mmss_cc_base + 0x00E4);

	writel_relaxed((0x80 | readl_relaxed(mmss_cc_base + 0x00E4)),
	       mmss_cc_base + 0x00E4);
	usleep(1000);
	writel_relaxed((~0x80 & readl_relaxed(mmss_cc_base + 0x00E4)),
	       mmss_cc_base + 0x00E4);

	writel_relaxed(0x05, mmss_cc_base + 0x0094);
	writel_relaxed(0x02, mmss_cc_base + 0x0264);
	/* Wait until LVDS pixel clock output is enabled */
	mb();

	if (mfd->panel_info.bpp == 24) {
		if (lvds_pdata &&
		    lvds_pdata->lvds_pixel_remap &&
		    lvds_pdata->lvds_pixel_remap()) {
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D0_3_TO_0 */
			MDP_OUTP(MDP_BASE +  0xc2014, 0x05080001);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D0_6_TO_4 */
			MDP_OUTP(MDP_BASE +  0xc2018, 0x00020304);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D1_3_TO_0 */
			MDP_OUTP(MDP_BASE +  0xc201c, 0x1011090a);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D1_6_TO_4 */
			MDP_OUTP(MDP_BASE +  0xc2020, 0x000b0c0d);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D2_3_TO_0 */
			MDP_OUTP(MDP_BASE +  0xc2024, 0x191a1213);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D2_6_TO_4 */
			MDP_OUTP(MDP_BASE +  0xc2028, 0x00141518);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D3_3_TO_0 */
			MDP_OUTP(MDP_BASE +  0xc202c, 0x171b0607);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D3_6_TO_4 */
			MDP_OUTP(MDP_BASE +  0xc2030, 0x000e0f16);
		} else {
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D0_3_TO_0 */
			MDP_OUTP(MDP_BASE +  0xc2014, 0x03040508);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D0_6_TO_4 */
			MDP_OUTP(MDP_BASE +  0xc2018, 0x00000102);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D1_3_TO_0 */
			MDP_OUTP(MDP_BASE +  0xc201c, 0x0c0d1011);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D1_6_TO_4 */
			MDP_OUTP(MDP_BASE +  0xc2020, 0x00090a0b);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D2_3_TO_0 */
			MDP_OUTP(MDP_BASE +  0xc2024, 0x151a191a);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D2_6_TO_4 */
			MDP_OUTP(MDP_BASE +  0xc2028, 0x00121314);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D3_3_TO_0 */
			MDP_OUTP(MDP_BASE +  0xc202c, 0x0f16171b);
			/* MDP_LCDC_LVDS_MUX_CTL_FOR_D3_6_TO_4 */
			MDP_OUTP(MDP_BASE +  0xc2030, 0x0006070e);
		}
		if (mfd->panel_info.lvds.channel_mode ==
			LVDS_DUAL_CHANNEL_MODE) {
			lvds_intf = 0x0001ff80;
			lvds_phy_cfg0 = BIT(6) | BIT(7);
			if (mfd->panel_info.lvds.channel_swap)
				lvds_intf |= BIT(4);
		} else {
			lvds_intf = 0x00010f84;
			lvds_phy_cfg0 = BIT(6);
		}
	} else if (mfd->panel_info.bpp == 18) {
		/* MDP_LCDC_LVDS_MUX_CTL_FOR_D0_3_TO_0 */
		MDP_OUTP(MDP_BASE +  0xc2014, 0x03040508);
		/* MDP_LCDC_LVDS_MUX_CTL_FOR_D0_6_TO_4 */
		MDP_OUTP(MDP_BASE +  0xc2018, 0x00000102);
		/* MDP_LCDC_LVDS_MUX_CTL_FOR_D1_3_TO_0 */
		MDP_OUTP(MDP_BASE +  0xc201c, 0x0c0d1011);
		/* MDP_LCDC_LVDS_MUX_CTL_FOR_D1_6_TO_4 */
		MDP_OUTP(MDP_BASE +  0xc2020, 0x00090a0b);
		/* MDP_LCDC_LVDS_MUX_CTL_FOR_D2_3_TO_0 */
		MDP_OUTP(MDP_BASE +  0xc2024, 0x1518191a);
		/* MDP_LCDC_LVDS_MUX_CTL_FOR_D2_6_TO_4 */
		MDP_OUTP(MDP_BASE +  0xc2028, 0x00121314);

		if (mfd->panel_info.lvds.channel_mode ==
			LVDS_DUAL_CHANNEL_MODE) {
			lvds_intf = 0x00017788;
			lvds_phy_cfg0 = BIT(6) | BIT(7);
			if (mfd->panel_info.lvds.channel_swap)
				lvds_intf |= BIT(4);
		} else {
			lvds_intf = 0x0001078c;
			lvds_phy_cfg0 = BIT(6);
		}
	} else {
		BUG();
	}

	/* MDP_LVDSPHY_CFG0 */
	MDP_OUTP(MDP_BASE +  0xc3100, lvds_phy_cfg0);
	/* MDP_LCDC_LVDS_INTF_CTL */
	MDP_OUTP(MDP_BASE +  0xc2000, lvds_intf);
	MDP_OUTP(MDP_BASE +  0xc3108, 0x30);
	lvds_phy_cfg0 |= BIT(4);

	/* Wait until LVDS PHY registers are configured */
	mb();
	usleep(1);
	/* MDP_LVDSPHY_CFG0, enable serialization */
	MDP_OUTP(MDP_BASE +  0xc3100, lvds_phy_cfg0);
}

static int lvds_off(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);
	ret = panel_next_off(pdev);

	if (lvds_clk)
		clk_disable_unprepare(lvds_clk);

	MDP_OUTP(MDP_BASE +  0xc3100, 0x0);
	MDP_OUTP(MDP_BASE + 0xc3000, 0x0);
	usleep(10);

	if (lvds_pdata && lvds_pdata->lcdc_power_save)
		lvds_pdata->lcdc_power_save(0);

	if (lvds_pdata && lvds_pdata->lcdc_gpio_config)
		ret = lvds_pdata->lcdc_gpio_config(0);

#ifdef CONFIG_MSM_BUS_SCALING
	mdp_bus_scale_update_request(0);
#endif

	return ret;
}

static int lvds_on(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_fb_data_type *mfd;
	unsigned long panel_pixclock_freq = 0;
	mfd = platform_get_drvdata(pdev);

	if (lvds_pdata && lvds_pdata->lcdc_get_clk)
		panel_pixclock_freq = lvds_pdata->lcdc_get_clk();

	if (!panel_pixclock_freq)
		panel_pixclock_freq = mfd->fbi->var.pixclock;
#ifdef CONFIG_MSM_BUS_SCALING
	mdp_bus_scale_update_request(2);
#endif
	mfd = platform_get_drvdata(pdev);

	if (lvds_clk) {
		mfd->fbi->var.pixclock = clk_round_rate(lvds_clk,
			mfd->fbi->var.pixclock);
		ret = clk_set_rate(lvds_clk, mfd->fbi->var.pixclock);
		if (ret) {
			pr_err("%s: Can't set lvds clock to rate %u\n",
				__func__, mfd->fbi->var.pixclock);
			goto out;
		}
		clk_prepare_enable(lvds_clk);
	}

	if (lvds_pdata && lvds_pdata->lcdc_power_save)
		lvds_pdata->lcdc_power_save(1);
	if (lvds_pdata && lvds_pdata->lcdc_gpio_config)
		ret = lvds_pdata->lcdc_gpio_config(1);

	lvds_init(mfd);
	ret = panel_next_on(pdev);

out:
	return ret;
}

static int lvds_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct fb_info *fbi;
	struct platform_device *mdp_dev = NULL;
	struct msm_fb_panel_data *pdata = NULL;
	int rc;

	if (pdev->id == 0) {
		lvds_pdata = pdev->dev.platform_data;

		lvds_clk = clk_get(&pdev->dev, "lvds_clk");
		if (IS_ERR_OR_NULL(lvds_clk)) {
			pr_err("Couldnt find lvds_clk\n");
			lvds_clk = NULL;
		}
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
		pr_err("lvds_probe: platform_device_add_data failed!\n");
		platform_device_put(mdp_dev);
		return -ENOMEM;
	}
	/*
	 * data chain
	 */
	pdata = (struct msm_fb_panel_data *)mdp_dev->dev.platform_data;
	pdata->on = lvds_on;
	pdata->off = lvds_off;
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
	if (lvds_clk) {
		fbi->var.pixclock = clk_round_rate(lvds_clk,
			mfd->panel_info.clk_rate);
	}

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
		goto lvds_probe_err;

	pdev_list[pdev_list_cnt++] = pdev;

	return 0;

lvds_probe_err:
	platform_device_put(mdp_dev);
	return rc;
}

static int lvds_remove(struct platform_device *pdev)
{
	return 0;
}

static int lvds_register_driver(void)
{
	return platform_driver_register(&lvds_driver);
}

static int __init lvds_driver_init(void)
{
	return lvds_register_driver();
}

module_init(lvds_driver_init);
