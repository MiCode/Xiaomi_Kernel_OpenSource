/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/pm_runtime.h>

#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/platform_device.h>

#define TVENC_C
#include "tvenc.h"
#include "msm_fb.h"
#include "mdp4.h"
/* AXI rate in KHz */
#define MSM_SYSTEM_BUS_RATE	128000000

static int tvenc_probe(struct platform_device *pdev);
static int tvenc_remove(struct platform_device *pdev);

static int tvenc_off(struct platform_device *pdev);
static int tvenc_on(struct platform_device *pdev);

static struct platform_device *pdev_list[MSM_FB_MAX_DEV_LIST];
static int pdev_list_cnt;

static struct clk *tvenc_clk;
static struct clk *tvdac_clk;
static struct clk *tvenc_pclk;
static struct clk *mdp_tv_clk;
#ifdef CONFIG_FB_MSM_MDP40
static struct clk *tv_src_clk;
#endif

#ifdef CONFIG_MSM_BUS_SCALING
static uint32_t tvenc_bus_scale_handle;
#endif

static int tvenc_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int tvenc_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static struct dev_pm_ops tvenc_dev_pm_ops = {
	.runtime_suspend = tvenc_runtime_suspend,
	.runtime_resume = tvenc_runtime_resume,
};

static struct platform_driver tvenc_driver = {
	.probe = tvenc_probe,
	.remove = tvenc_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "tvenc",
		   .pm = &tvenc_dev_pm_ops
		   },
};

int tvenc_set_encoder_clock(boolean clock_on)
{
	int ret = 0;
	if (clock_on) {
#ifdef CONFIG_FB_MSM_MDP40
		/* Consolidated clock used by both HDMI & TV encoder.
		Clock exists only in MDP4 and not in older versions */
		ret = clk_set_rate(tv_src_clk, 27000000);
		if (ret) {
			pr_err("%s: tvsrc_clk set rate failed! %d\n",
				__func__, ret);
			goto tvsrc_err;
		}
#endif
		ret = clk_prepare_enable(tvenc_clk);
		if (ret) {
			pr_err("%s: tvenc_clk enable failed! %d\n",
				__func__, ret);
			goto tvsrc_err;
		}

		if (!IS_ERR(tvenc_pclk)) {
			ret = clk_prepare_enable(tvenc_pclk);
			if (ret) {
				pr_err("%s: tvenc_pclk enable failed! %d\n",
					__func__, ret);
				goto tvencp_err;
			}
		}
		return ret;
	} else {
		if (!IS_ERR(tvenc_pclk))
			clk_disable_unprepare(tvenc_pclk);
		clk_disable_unprepare(tvenc_clk);
		return ret;
	}
tvencp_err:
	clk_disable_unprepare(tvenc_clk);
tvsrc_err:
	return ret;
}

int tvenc_set_clock(boolean clock_on)
{
	int ret = 0;
	if (clock_on) {
		if (tvenc_pdata->poll) {
			ret = tvenc_set_encoder_clock(CLOCK_ON);
			if (ret) {
				pr_err("%s: TVenc clock(s) enable failed! %d\n",
					__func__, ret);
				goto tvenc_err;
			}
		}
		ret = clk_prepare_enable(tvdac_clk);
		if (ret) {
			pr_err("%s: tvdac_clk enable failed! %d\n",
				__func__, ret);
			goto tvdac_err;
		}
		if (!IS_ERR(mdp_tv_clk)) {
			ret = clk_prepare_enable(mdp_tv_clk);
			if (ret) {
				pr_err("%s: mdp_tv_clk enable failed! %d\n",
					__func__, ret);
				goto mdptv_err;
			}
		}
		return ret;
	} else {
		if (!IS_ERR(mdp_tv_clk))
			clk_disable_unprepare(mdp_tv_clk);
		clk_disable_unprepare(tvdac_clk);
		if (tvenc_pdata->poll)
			tvenc_set_encoder_clock(CLOCK_OFF);
		return ret;
	}

mdptv_err:
	clk_disable_unprepare(tvdac_clk);
tvdac_err:
	tvenc_set_encoder_clock(CLOCK_OFF);
tvenc_err:
	return ret;
}

static int tvenc_off(struct platform_device *pdev)
{
	int ret = 0;

	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	ret = panel_next_off(pdev);
	if (ret)
		pr_err("%s: tvout_off failed! %d\n",
		__func__, ret);

	tvenc_set_clock(CLOCK_OFF);

	if (tvenc_pdata && tvenc_pdata->pm_vid_en)
		ret = tvenc_pdata->pm_vid_en(0);
#ifdef CONFIG_MSM_BUS_SCALING
	if (tvenc_bus_scale_handle > 0)
		msm_bus_scale_client_update_request(tvenc_bus_scale_handle,
							0);
#else
	if (mfd->ebi1_clk)
		clk_disable_unprepare(mfd->ebi1_clk);
#endif

	if (ret)
		pr_err("%s: pm_vid_en(off) failed! %d\n",
		__func__, ret);
	mdp4_extn_disp = 0;
	return ret;
}

static int tvenc_on(struct platform_device *pdev)
{
	int ret = 0;

#ifndef CONFIG_MSM_BUS_SCALING
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);
#endif

#ifdef CONFIG_MSM_BUS_SCALING
	if (tvenc_bus_scale_handle > 0)
		msm_bus_scale_client_update_request(tvenc_bus_scale_handle,
							1);
#else
	if (mfd->ebi1_clk)
		clk_prepare_enable(mfd->ebi1_clk);
#endif
	mdp4_extn_disp = 1;
	if (tvenc_pdata && tvenc_pdata->pm_vid_en)
		ret = tvenc_pdata->pm_vid_en(1);
	if (ret) {
		pr_err("%s: pm_vid_en(on) failed! %d\n",
		__func__, ret);
		return ret;
	}

	ret = tvenc_set_clock(CLOCK_ON);
	if (ret) {
		pr_err("%s: tvenc_set_clock(CLOCK_ON) failed! %d\n",
		__func__, ret);
		tvenc_pdata->pm_vid_en(0);
		goto error;
	}

	ret = panel_next_on(pdev);
	if (ret) {
		pr_err("%s: tvout_on failed! %d\n",
		__func__, ret);
		tvenc_set_clock(CLOCK_OFF);
		tvenc_pdata->pm_vid_en(0);
	}

error:
	return ret;

}

void tvenc_gen_test_pattern(struct msm_fb_data_type *mfd)
{
	uint32 reg = 0, i;

	reg = readl(MSM_TV_ENC_CTL);
	reg |= TVENC_CTL_TEST_PATT_EN;

	for (i = 0; i < 3; i++) {
		TV_OUT(TV_ENC_CTL, 0);	/* disable TV encoder */

		switch (i) {
			/*
			 * TV Encoder - Color Bar Test Pattern
			 */
		case 0:
			reg |= TVENC_CTL_TPG_CLRBAR;
			break;
			/*
			 * TV Encoder - Red Frame Test Pattern
			 */
		case 1:
			reg |= TVENC_CTL_TPG_REDCLR;
			break;
			/*
			 * TV Encoder - Modulated Ramp Test Pattern
			 */
		default:
			reg |= TVENC_CTL_TPG_MODRAMP;
			break;
		}

		TV_OUT(TV_ENC_CTL, reg);
		mdelay(5000);

		switch (i) {
			/*
			 * TV Encoder - Color Bar Test Pattern
			 */
		case 0:
			reg &= ~TVENC_CTL_TPG_CLRBAR;
			break;
			/*
			 * TV Encoder - Red Frame Test Pattern
			 */
		case 1:
			reg &= ~TVENC_CTL_TPG_REDCLR;
			break;
			/*
			 * TV Encoder - Modulated Ramp Test Pattern
			 */
		default:
			reg &= ~TVENC_CTL_TPG_MODRAMP;
			break;
		}
	}
}

static int tvenc_resource_initialized;

static int tvenc_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct platform_device *mdp_dev = NULL;
	struct msm_fb_panel_data *pdata = NULL;
	int rc, ret;
	struct clk *ebi1_clk = NULL;

	if (pdev->id == 0) {
		tvenc_base = ioremap(pdev->resource[0].start,
					pdev->resource[0].end -
					pdev->resource[0].start + 1);
		if (!tvenc_base) {
			pr_err("tvenc_base ioremap failed!\n");
			return -ENOMEM;
		}

		tvenc_clk = clk_get(&pdev->dev, "enc_clk");
		tvdac_clk = clk_get(&pdev->dev, "dac_clk");
		tvenc_pclk = clk_get(&pdev->dev, "iface_clk");
		mdp_tv_clk = clk_get(&pdev->dev, "mdp_clk");

#ifndef CONFIG_MSM_BUS_SCALING
		ebi1_clk = clk_get(&pdev->dev, "mem_clk");
		if (IS_ERR(ebi1_clk)) {
			rc = PTR_ERR(ebi1_clk);
			goto tvenc_probe_err;
		}
		clk_set_rate(ebi1_clk, MSM_SYSTEM_BUS_RATE);
#endif

#ifdef CONFIG_FB_MSM_MDP40
		tv_src_clk = clk_get(&pdev->dev, "src_clk");
		if (IS_ERR(tv_src_clk))
			tv_src_clk = tvenc_clk; /* Fallback to slave */
#endif

		if (IS_ERR(tvenc_clk)) {
			pr_err("%s: error: can't get tvenc_clk!\n", __func__);
			return PTR_ERR(tvenc_clk);
		}

		if (IS_ERR(tvdac_clk)) {
			pr_err("%s: error: can't get tvdac_clk!\n", __func__);
			return PTR_ERR(tvdac_clk);
		}

		if (IS_ERR(tvenc_pclk)) {
			ret = PTR_ERR(tvenc_pclk);
			if (-ENOENT == ret)
				pr_info("%s: tvenc_pclk does not exist!\n",
								__func__);
			else {
				pr_err("%s: error: can't get tvenc_pclk!\n",
								__func__);
				return ret;
			}
		}

		if (IS_ERR(mdp_tv_clk)) {
			ret = PTR_ERR(mdp_tv_clk);
			if (-ENOENT == ret)
				pr_info("%s: mdp_tv_clk does not exist!\n",
								__func__);
			else {
				pr_err("%s: error: can't get mdp_tv_clk!\n",
								__func__);
				return ret;
			}
		}

		tvenc_pdata = pdev->dev.platform_data;
		tvenc_resource_initialized = 1;
		return 0;
	}

	if (!tvenc_resource_initialized)
		return -EPERM;

	mfd = platform_get_drvdata(pdev);
	mfd->ebi1_clk = ebi1_clk;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (pdev_list_cnt >= MSM_FB_MAX_DEV_LIST)
		return -ENOMEM;

	if (tvenc_base == NULL)
		return -ENOMEM;

	mdp_dev = platform_device_alloc("mdp", pdev->id);
	if (!mdp_dev)
		return -ENOMEM;

	/*
	 * link to the latest pdev
	 */
	mfd->pdev = mdp_dev;
	mfd->dest = DISPLAY_TV;

	/*
	 * alloc panel device data
	 */
	if (platform_device_add_data
	    (mdp_dev, pdev->dev.platform_data,
	     sizeof(struct msm_fb_panel_data))) {
		pr_err("tvenc_probe: platform_device_add_data failed!\n");
		platform_device_put(mdp_dev);
		return -ENOMEM;
	}
	/*
	 * data chain
	 */
	pdata = mdp_dev->dev.platform_data;
	pdata->on = tvenc_on;
	pdata->off = tvenc_off;
	pdata->next = pdev;

	/*
	 * get/set panel specific fb info
	 */
	mfd->panel_info = pdata->panel_info;
#ifdef CONFIG_FB_MSM_MDP40
	mfd->fb_imgType = MDP_RGB_565;  /* base layer */
#else
	mfd->fb_imgType = MDP_YCRYCB_H2V1;
#endif

#ifdef CONFIG_MSM_BUS_SCALING
	if (!tvenc_bus_scale_handle && tvenc_pdata &&
		tvenc_pdata->bus_scale_table) {
		tvenc_bus_scale_handle =
			msm_bus_scale_register_client(
				tvenc_pdata->bus_scale_table);
		if (!tvenc_bus_scale_handle) {
			printk(KERN_ERR "%s not able to get bus scale\n",
				__func__);
		}
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
		goto tvenc_probe_err;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);



	pdev_list[pdev_list_cnt++] = pdev;

	return 0;

tvenc_probe_err:
#ifdef CONFIG_MSM_BUS_SCALING
	if (tvenc_pdata && tvenc_pdata->bus_scale_table &&
		tvenc_bus_scale_handle > 0) {
		msm_bus_scale_unregister_client(tvenc_bus_scale_handle);
		tvenc_bus_scale_handle = 0;
	}
#endif
	platform_device_put(mdp_dev);
	return rc;
}

static int tvenc_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

#ifdef CONFIG_MSM_BUS_SCALING
	if (tvenc_pdata && tvenc_pdata->bus_scale_table &&
		tvenc_bus_scale_handle > 0) {
		msm_bus_scale_unregister_client(tvenc_bus_scale_handle);
		tvenc_bus_scale_handle = 0;
	}
#else
	clk_put(mfd->ebi1_clk);
#endif

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static int tvenc_register_driver(void)
{
	return platform_driver_register(&tvenc_driver);
}

static int __init tvenc_driver_init(void)
{
	return tvenc_register_driver();
}

module_init(tvenc_driver_init);
