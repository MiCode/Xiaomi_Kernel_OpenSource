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
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>

#include <asm/system.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <mach/dma.h>

#include "mdss_edp.h"

#define RGB_COMPONENTS		3
#define VDDA_MIN_UV			1800000	/* uV units */
#define VDDA_MAX_UV			1800000	/* uV units */
#define VDDA_UA_ON_LOAD		100000	/* uA units */
#define VDDA_UA_OFF_LOAD	100		/* uA units */


static int mdss_edp_get_base_address(struct mdss_edp_drv_pdata *edp_drv);
static int mdss_edp_get_mmss_cc_base_address(struct mdss_edp_drv_pdata
		*edp_drv);
static int mdss_edp_regulator_init(struct mdss_edp_drv_pdata *edp_drv);
static int mdss_edp_regulator_on(struct mdss_edp_drv_pdata *edp_drv);
static int mdss_edp_regulator_off(struct mdss_edp_drv_pdata *edp_drv);

static int mdss_edp_gpio_panel_en(struct mdss_edp_drv_pdata *edp_drv);

static void mdss_edp_edid2pinfo(struct mdss_edp_drv_pdata *edp_drv);
static void mdss_edp_fill_edid_data(struct mdss_edp_drv_pdata *edp_drv);
static void mdss_edp_fill_dpcd_data(struct mdss_edp_drv_pdata *edp_drv);

static int mdss_edp_device_register(struct mdss_edp_drv_pdata *edp_drv);

static void mdss_edp_config_sync(unsigned char *edp_base);
static void mdss_edp_config_sw_div(unsigned char *edp_base);
static void mdss_edp_config_static_mdiv(unsigned char *edp_base);
static void mdss_edp_enable(unsigned char *edp_base, int enable);

/*
 * Init regulator needed for edp, 8974_l12
 */
static int mdss_edp_regulator_init(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	edp_drv->vdda_vreg = devm_regulator_get(&(edp_drv->pdev->dev), "vdda");
	if (IS_ERR(edp_drv->vdda_vreg)) {
		pr_err("%s: Could not get 8941_l12, ret = %ld\n", __func__,
				PTR_ERR(edp_drv->vdda_vreg));
		return -ENODEV;
	}

	ret = regulator_set_voltage(edp_drv->vdda_vreg,
			VDDA_MIN_UV, VDDA_MAX_UV);
	if (ret) {
		pr_err("%s: vdda_vreg set_voltage failed, ret=%d\n", __func__,
				ret);
		return -EINVAL;
	}

	ret = mdss_edp_regulator_on(edp_drv);
	if (ret)
		return ret;

	return 0;
}

/*
 * Set uA and enable vdda
 */
static int mdss_edp_regulator_on(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	ret = regulator_set_optimum_mode(edp_drv->vdda_vreg, VDDA_UA_ON_LOAD);
	if (ret < 0) {
		pr_err("%s: vdda_vreg set regulator mode failed.\n", __func__);
		return ret;
	}

	ret = regulator_enable(edp_drv->vdda_vreg);
	if (ret) {
		pr_err("%s: Failed to enable vdda_vreg regulator.\n", __func__);
		return ret;
	}

	return 0;
}

/*
 * Disable vdda and set uA
 */
static int mdss_edp_regulator_off(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	ret = regulator_disable(edp_drv->vdda_vreg);
	if (ret) {
		pr_err("%s: Failed to disable vdda_vreg regulator.\n",
				__func__);
		return ret;
	}

	ret = regulator_set_optimum_mode(edp_drv->vdda_vreg, VDDA_UA_OFF_LOAD);
	if (ret < 0) {
		pr_err("%s: vdda_vreg set regulator mode failed.\n",
				__func__);
		return ret;
	}

	return 0;
}

/*
 * Enables the gpio that supply power to the panel
 */
static int mdss_edp_gpio_panel_en(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret = 0;

	edp_drv->gpio_panel_en = of_get_named_gpio(edp_drv->pdev->dev.of_node,
			"gpio-panel-en", 0);
	if (!gpio_is_valid(edp_drv->gpio_panel_en)) {
		pr_err("%s: gpio_panel_en not specified\n", __func__);
		goto gpio_err;
	}

	ret = gpio_request(edp_drv->gpio_panel_en, "disp_enable");
	if (ret) {
		pr_err("%s: Request reset gpio_panel_en failed, ret=%d\n",
				__func__, ret);
		goto gpio_free;
	}

	ret = gpio_direction_output(edp_drv->gpio_panel_en, 1);
	if (ret) {
		pr_err("%s: Set direction for gpio_panel_en failed, ret=%d\n",
				__func__, ret);
		goto gpio_free;
	}

	gpio_set_value(edp_drv->gpio_panel_en, 1);

	return 0;

gpio_free:
	gpio_free(edp_drv->gpio_panel_en);
gpio_err:
	return -ENODEV;
}

static void mdss_edp_config_sync(unsigned char *edp_base)
{
	int ret = 0;

	ret = edp_read(edp_base + 0xc); /* EDP_CONFIGURATION_CTRL */
	ret &= ~0x733;
	ret |= (0x55 & 0x733);
	edp_write(edp_base + 0xc, ret);
	edp_write(edp_base + 0xc, 0x55); /* EDP_CONFIGURATION_CTRL */
}

static void mdss_edp_config_sw_div(unsigned char *edp_base)
{
	edp_write(edp_base + 0x14, 0x13b); /* EDP_SOFTWARE_MVID */
	edp_write(edp_base + 0x18, 0x266); /* EDP_SOFTWARE_NVID */
}

static void mdss_edp_config_static_mdiv(unsigned char *edp_base)
{
	int ret = 0;

	ret = edp_read(edp_base + 0xc); /* EDP_CONFIGURATION_CTRL */
	edp_write(edp_base + 0xc, ret | 0x2); /* EDP_CONFIGURATION_CTRL */
	edp_write(edp_base + 0xc, 0x57); /* EDP_CONFIGURATION_CTRL */
}

static void mdss_edp_enable(unsigned char *edp_base, int enable)
{
	edp_write(edp_base + 0x8, 0x0); /* EDP_STATE_CTRL */
	edp_write(edp_base + 0x8, 0x40); /* EDP_STATE_CTRL */
	edp_write(edp_base + 0x94, enable); /* EDP_TIMING_ENGINE_EN */
	edp_write(edp_base + 0x4, enable); /* EDP_MAINLINK_CTRL */
}

int mdss_edp_on(struct mdss_panel_data *pdata)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;

	edp_drv = container_of(pdata, struct mdss_edp_drv_pdata,
			panel_data);
	if (!edp_drv) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	mdss_edp_config_sync(edp_drv->edp_base);
	mdss_edp_config_sw_div(edp_drv->edp_base);
	mdss_edp_config_static_mdiv(edp_drv->edp_base);
	mdss_edp_enable(edp_drv->edp_base, 1);

	return 0;
}

int mdss_edp_off(struct mdss_panel_data *pdata)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;
	int ret = 0;

	edp_drv = container_of(pdata, struct mdss_edp_drv_pdata,
				panel_data);
	if (!edp_drv) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	mdss_edp_enable(edp_drv->edp_base, 0);
	gpio_set_value(edp_drv->gpio_panel_en, 0);

	return ret;
}

/*
 * Converts from EDID struct to mdss_panel_info
 */
static void mdss_edp_edid2pinfo(struct mdss_edp_drv_pdata *edp_drv)
{
	struct display_timing_desc *dp;
	struct mdss_panel_info *pinfo;

	dp = &edp_drv->edid.timing[0];
	pinfo = &edp_drv->panel_data.panel_info;

	pinfo->clk_rate = dp->pclk;

	pinfo->xres = dp->h_addressable + dp->h_border * 2;
	pinfo->yres = dp->v_addressable + dp->v_border * 2;

	pinfo->lcdc.h_back_porch = dp->h_blank - dp->h_fporch \
		- dp->h_sync_pulse;
	pinfo->lcdc.h_front_porch = dp->h_fporch;
	pinfo->lcdc.h_pulse_width = dp->h_sync_pulse;

	pinfo->lcdc.v_back_porch = dp->v_blank - dp->v_fporch \
		- dp->v_sync_pulse;
	pinfo->lcdc.v_front_porch = dp->v_fporch;
	pinfo->lcdc.v_pulse_width = dp->v_sync_pulse;

	pinfo->type = EDP_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = edp_drv->edid.color_depth * RGB_COMPONENTS;
	pinfo->fb_num = 2;

	pinfo->lcdc.border_clr = 0;	 /* black */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;
}

static int mdss_edp_remove(struct platform_device *pdev)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;

	edp_drv = platform_get_drvdata(pdev);

	gpio_free(edp_drv->gpio_panel_en);
	mdss_edp_regulator_off(edp_drv);
	iounmap(edp_drv->edp_base);
	iounmap(edp_drv->mmss_cc_base);
	edp_drv->edp_base = NULL;

	return 0;
}

static int mdss_edp_device_register(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	mdss_edp_edid2pinfo(edp_drv);

	edp_drv->panel_data.on = mdss_edp_on;
	edp_drv->panel_data.off = mdss_edp_off;

	ret = mdss_register_panel(&edp_drv->panel_data);
	if (ret) {
		dev_err(&(edp_drv->pdev->dev), "unable to register eDP\n");
		return ret;
	}

	pr_debug("%s: eDP initialized\n", __func__);

	return 0;
}

/*
 * Retrieve edp base address
 */
static int mdss_edp_get_base_address(struct mdss_edp_drv_pdata *edp_drv)
{
	struct resource *res;

	res = platform_get_resource_byname(edp_drv->pdev, IORESOURCE_MEM,
			"edp_base");
	if (!res) {
		pr_err("%s: Unable to get the MDSS EDP resources", __func__);
		return -ENOMEM;
	}

	edp_drv->edp_base = ioremap(res->start, resource_size(res));
	if (!edp_drv->edp_base) {
		pr_err("%s: Unable to remap EDP resources",  __func__);
		return -ENOMEM;
	}

	return 0;
}

static int mdss_edp_get_mmss_cc_base_address(struct mdss_edp_drv_pdata
		*edp_drv)
{
	struct resource *res;

	res = platform_get_resource_byname(edp_drv->pdev, IORESOURCE_MEM,
			"mmss_cc_base");
	if (!res) {
		pr_err("%s: Unable to get the MMSS_CC resources", __func__);
		return -ENOMEM;
	}

	edp_drv->mmss_cc_base = ioremap(res->start, resource_size(res));
	if (!edp_drv->mmss_cc_base) {
		pr_err("%s: Unable to remap MMSS_CC resources",  __func__);
		return -ENOMEM;
	}

	return 0;
}

static void mdss_edp_fill_edid_data(struct mdss_edp_drv_pdata *edp_drv)
{
	struct edp_edid *edid = &edp_drv->edid;

	edid->id_name[0] = 'A';
	edid->id_name[0] = 'U';
	edid->id_name[0] = 'O';
	edid->id_name[0] = 0;
	edid->id_product = 0x305D;
	edid->version = 1;
	edid->revision = 4;
	edid->ext_block_cnt = 0;
	edid->video_digital = 0x5;
	edid->color_depth = 6;
	edid->dpm = 0;
	edid->color_format = 0;
	edid->timing[0].pclk = 138500000;
	edid->timing[0].h_addressable = 1920;
	edid->timing[0].h_blank = 160;
	edid->timing[0].v_addressable = 1080;
	edid->timing[0].v_blank = 30;
	edid->timing[0].h_fporch = 48;
	edid->timing[0].h_sync_pulse = 32;
	edid->timing[0].v_sync_pulse = 14;
	edid->timing[0].v_fporch = 8;
	edid->timing[0].width_mm =  256;
	edid->timing[0].height_mm = 144;
	edid->timing[0].h_border = 0;
	edid->timing[0].v_border = 0;
	edid->timing[0].interlaced = 0;
	edid->timing[0].stereo = 0;
	edid->timing[0].sync_type = 1;
	edid->timing[0].sync_separate = 1;
	edid->timing[0].vsync_pol = 0;
	edid->timing[0].hsync_pol = 0;

}

static void mdss_edp_fill_dpcd_data(struct mdss_edp_drv_pdata *edp_drv)
{
	struct dpcd_cap *cap = &edp_drv->dpcd;

	cap->max_lane_count = 2;
	cap->max_link_clk = 270;
}


static int mdss_edp_probe(struct platform_device *pdev)
{
	int ret;
	struct mdss_edp_drv_pdata *edp_drv;

	if (!pdev->dev.of_node) {
		pr_err("%s: Failed\n", __func__);
		return -EPERM;
	}

	edp_drv = devm_kzalloc(&pdev->dev, sizeof(*edp_drv), GFP_KERNEL);
	if (edp_drv == NULL) {
		pr_err("%s: Failed, could not allocate edp_drv", __func__);
		return -ENOMEM;
	}

	edp_drv->pdev = pdev;
	edp_drv->pdev->id = 1;
	edp_drv->clk_on = 0;

	ret = mdss_edp_get_base_address(edp_drv);
	if (ret)
		goto probe_err;

	ret = mdss_edp_get_mmss_cc_base_address(edp_drv);
	if (ret)
		goto edp_base_unmap;

	ret = mdss_edp_regulator_init(edp_drv);
	if (ret)
		goto mmss_cc_base_unmap;

	ret = mdss_edp_gpio_panel_en(edp_drv);
	if (ret)
		goto edp_regulator_off;

	mdss_edp_fill_edid_data(edp_drv);
	mdss_edp_fill_dpcd_data(edp_drv);
	mdss_edp_device_register(edp_drv);

	return 0;

edp_regulator_off:
	mdss_edp_regulator_off(edp_drv);
mmss_cc_base_unmap:
	iounmap(edp_drv->mmss_cc_base);
edp_base_unmap:
	iounmap(edp_drv->edp_base);
probe_err:
	return ret;

}

static const struct of_device_id msm_mdss_edp_dt_match[] = {
	{.compatible = "qcom,mdss-edp"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_mdss_edp_dt_match);

static struct platform_driver mdss_edp_driver = {
	.probe = mdss_edp_probe,
	.remove = mdss_edp_remove,
	.shutdown = NULL,
	.driver = {
		.name = "mdss_edp",
		.of_match_table = msm_mdss_edp_dt_match,
	},
};

static int __init mdss_edp_init(void)
{
	int ret;

	ret = platform_driver_register(&mdss_edp_driver);
	if (ret) {
		pr_err("%s driver register failed", __func__);
		return ret;
	}

	return ret;
}
module_init(mdss_edp_init);

static void __exit mdss_edp_driver_cleanup(void)
{
	platform_driver_unregister(&mdss_edp_driver);
}
module_exit(mdss_edp_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("eDP controller driver");
