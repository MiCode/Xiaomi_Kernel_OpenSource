/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/pwm.h>
#include <linux/clk.h>
#include <linux/spinlock_types.h>
#include <linux/kthread.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/dma.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_mdp.h"
#include "mdss_edp.h"
#include "mdss_debug.h"

#define RGB_COMPONENTS		3
#define VDDA_MIN_UV			1800000	/* uV units */
#define VDDA_MAX_UV			1800000	/* uV units */
#define VDDA_UA_ON_LOAD		100000	/* uA units */
#define VDDA_UA_OFF_LOAD	100		/* uA units */

static int mdss_edp_regulator_on(struct mdss_edp_drv_pdata *edp_drv);
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
 * Enables the gpio that supply power to the panel and enable the backlight
 */
static int mdss_edp_gpio_panel_en(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret = 0;

	edp_drv->gpio_panel_en = of_get_named_gpio(edp_drv->pdev->dev.of_node,
			"gpio-panel-en", 0);
	if (!gpio_is_valid(edp_drv->gpio_panel_en)) {
		pr_err("%s: gpio_panel_en=%d not specified\n", __func__,
				edp_drv->gpio_panel_en);
		goto gpio_err;
	}

	ret = gpio_request(edp_drv->gpio_panel_en, "disp_enable");
	if (ret) {
		pr_err("%s: Request reset gpio_panel_en failed, ret=%d\n",
				__func__, ret);
		return ret;
	}

	ret = gpio_direction_output(edp_drv->gpio_panel_en, 1);
	if (ret) {
		pr_err("%s: Set direction for gpio_panel_en failed, ret=%d\n",
				__func__, ret);
		goto gpio_free;
	}

	return 0;

gpio_free:
	gpio_free(edp_drv->gpio_panel_en);
gpio_err:
	return -ENODEV;
}

static int mdss_edp_pwm_config(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret = 0;

	ret = of_property_read_u32(edp_drv->pdev->dev.of_node,
			"qcom,panel-pwm-period", &edp_drv->pwm_period);
	if (ret) {
		pr_err("%s: panel pwm period is not specified, %d", __func__,
				edp_drv->pwm_period);
		return -EINVAL;
	}

	ret = of_property_read_u32(edp_drv->pdev->dev.of_node,
			"qcom,panel-lpg-channel", &edp_drv->lpg_channel);
	if (ret) {
		pr_err("%s: panel lpg channel is not specified, %d", __func__,
				edp_drv->lpg_channel);
		return -EINVAL;
	}

	edp_drv->bl_pwm = pwm_request(edp_drv->lpg_channel, "lcd-backlight");
	if (edp_drv->bl_pwm == NULL || IS_ERR(edp_drv->bl_pwm)) {
		pr_err("%s: pwm request failed", __func__);
		edp_drv->bl_pwm = NULL;
		return -EIO;
	}

	edp_drv->gpio_panel_pwm = of_get_named_gpio(edp_drv->pdev->dev.of_node,
			"gpio-panel-pwm", 0);
	if (!gpio_is_valid(edp_drv->gpio_panel_pwm)) {
		pr_err("%s: gpio_panel_pwm=%d not specified\n", __func__,
				edp_drv->gpio_panel_pwm);
		goto edp_free_pwm;
	}

	ret = gpio_request(edp_drv->gpio_panel_pwm, "disp_pwm");
	if (ret) {
		pr_err("%s: Request reset gpio_panel_pwm failed, ret=%d\n",
				__func__, ret);
		goto edp_free_pwm;
	}

	return 0;

edp_free_pwm:
	pwm_free(edp_drv->bl_pwm);
	return -ENODEV;
}

void mdss_edp_set_backlight(struct mdss_panel_data *pdata, u32 bl_level)
{
	int ret = 0;
	struct mdss_edp_drv_pdata *edp_drv = NULL;
	int bl_max;

	edp_drv = container_of(pdata, struct mdss_edp_drv_pdata, panel_data);
	if (!edp_drv) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	bl_max = edp_drv->panel_data.panel_info.bl_max;
	if (bl_level > bl_max)
		bl_level = bl_max;

	if (edp_drv->bl_pwm == NULL) {
		pr_err("%s: edp_drv->bl_pwm=NULL.\n", __func__);
		return;
	}

	ret = pwm_config(edp_drv->bl_pwm,
			bl_level * edp_drv->pwm_period / bl_max,
			edp_drv->pwm_period);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return;
	}

	ret = pwm_enable(edp_drv->bl_pwm);
	if (ret) {
		pr_err("%s: pwm_enable() failed err=%d\n", __func__, ret);
		return;
	}
}

void mdss_edp_config_sync(unsigned char *base)
{
	int ret = 0;

	ret = edp_read(base + 0xc); /* EDP_CONFIGURATION_CTRL */
	ret &= ~0x733;
	ret |= (0x55 & 0x733);
	edp_write(base + 0xc, ret);
	edp_write(base + 0xc, 0x55); /* EDP_CONFIGURATION_CTRL */
}

static void mdss_edp_config_sw_div(unsigned char *base)
{
	edp_write(base + 0x14, 0x13b); /* EDP_SOFTWARE_MVID */
	edp_write(base + 0x18, 0x266); /* EDP_SOFTWARE_NVID */
}

static void mdss_edp_config_static_mdiv(unsigned char *base)
{
	int ret = 0;

	ret = edp_read(base + 0xc); /* EDP_CONFIGURATION_CTRL */
	edp_write(base + 0xc, ret | 0x2); /* EDP_CONFIGURATION_CTRL */
	edp_write(base + 0xc, 0x57); /* EDP_CONFIGURATION_CTRL */
}

static void mdss_edp_enable(unsigned char *base, int enable)
{
	edp_write(base + 0x8, 0x0); /* EDP_STATE_CTRL */
	edp_write(base + 0x8, 0x40); /* EDP_STATE_CTRL */
	edp_write(base + 0x94, enable); /* EDP_TIMING_ENGINE_EN */
	edp_write(base + 0x4, enable); /* EDP_MAINLINK_CTRL */
}

static void mdss_edp_irq_enable(struct mdss_edp_drv_pdata *edp_drv);
static void mdss_edp_irq_disable(struct mdss_edp_drv_pdata *edp_drv);

int mdss_edp_on(struct mdss_panel_data *pdata)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;
	int ret = 0;

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	edp_drv = container_of(pdata, struct mdss_edp_drv_pdata,
			panel_data);

	pr_debug("%s:+\n", __func__);
	if (edp_drv->train_start == 0)
		edp_drv->train_start++;

	mdss_edp_phy_pll_reset(edp_drv->base);
	mdss_edp_aux_reset(edp_drv->base);
	mdss_edp_mainlink_reset(edp_drv->base);

	ret = mdss_edp_prepare_clocks(edp_drv);
	if (ret)
		return ret;
	mdss_edp_phy_powerup(edp_drv->base, 1);

	mdss_edp_pll_configure(edp_drv->base, edp_drv->edid.timing[0].pclk);
	mdss_edp_phy_pll_ready(edp_drv->base);

	ret = mdss_edp_clk_enable(edp_drv);
	if (ret) {
		mdss_edp_unprepare_clocks(edp_drv);
		return ret;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	mdss_edp_aux_ctrl(edp_drv->base, 1);

	mdss_edp_lane_power_ctrl(edp_drv->base,
				edp_drv->dpcd.max_lane_count, 1);
	mdss_edp_enable_mainlink(edp_drv->base, 1);
	mdss_edp_config_clk(edp_drv->base, edp_drv->mmss_cc_base);

	mdss_edp_clock_synchrous(edp_drv->base, 1);
	mdss_edp_phy_vm_pe_init(edp_drv->base);
	mdss_edp_config_sync(edp_drv->base);
	mdss_edp_config_sw_div(edp_drv->base);
	mdss_edp_config_static_mdiv(edp_drv->base);
	gpio_set_value(edp_drv->gpio_panel_en, 1);
	mdss_edp_irq_enable(edp_drv);

	pr_debug("%s:-\n", __func__);
	return 0;
}

int mdss_edp_wait4train(struct mdss_panel_data *pdata)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;
	int ret = 0;

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	edp_drv = container_of(pdata, struct mdss_edp_drv_pdata,
			panel_data);

	ret = wait_for_completion_timeout(&edp_drv->train_comp, 100);
	if (ret <= 0) {
		pr_err("%s: Link Train timedout\n", __func__);
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	mdss_edp_enable(edp_drv->base, 1);

	pr_debug("%s:\n", __func__);

	return ret;
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
	pr_debug("%s:+\n", __func__);

	mdss_edp_irq_disable(edp_drv);

	gpio_set_value(edp_drv->gpio_panel_en, 0);
	pwm_disable(edp_drv->bl_pwm);
	mdss_edp_enable(edp_drv->base, 0);
	mdss_edp_unconfig_clk(edp_drv->base, edp_drv->mmss_cc_base);
	mdss_edp_enable_mainlink(edp_drv->base, 0);

	mdss_edp_lane_power_ctrl(edp_drv->base,
				edp_drv->dpcd.max_lane_count, 0);
	mdss_edp_clk_disable(edp_drv);
	mdss_edp_phy_powerup(edp_drv->base, 0);
	mdss_edp_unprepare_clocks(edp_drv);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mdss_edp_aux_ctrl(edp_drv->base, 0);

	pr_debug("%s:-\n", __func__);
	return ret;
}

static int mdss_edp_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;

	pr_debug("%s: event=%d\n", __func__, event);
	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = mdss_edp_on(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		rc = mdss_edp_wait4train(pdata);
		break;
	case MDSS_EVENT_PANEL_OFF:
		rc = mdss_edp_off(pdata);
		break;
	}
	return rc;
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
	pr_debug("%s: pclk=%d\n", __func__, pinfo->clk_rate);

	pinfo->xres = dp->h_addressable + dp->h_border * 2;
	pinfo->yres = dp->v_addressable + dp->v_border * 2;

	pr_debug("%s: x=%d y=%d\n", __func__, pinfo->xres, pinfo->yres);

	pinfo->lcdc.h_back_porch = dp->h_blank - dp->h_fporch \
		- dp->h_sync_pulse;
	pinfo->lcdc.h_front_porch = dp->h_fporch;
	pinfo->lcdc.h_pulse_width = dp->h_sync_pulse;

	pr_debug("%s: hporch= %d %d %d\n", __func__,
		pinfo->lcdc.h_back_porch, pinfo->lcdc.h_front_porch,
		pinfo->lcdc.h_pulse_width);

	pinfo->lcdc.v_back_porch = dp->v_blank - dp->v_fporch \
		- dp->v_sync_pulse;
	pinfo->lcdc.v_front_porch = dp->v_fporch;
	pinfo->lcdc.v_pulse_width = dp->v_sync_pulse;

	pr_debug("%s: vporch= %d %d %d\n", __func__,
		pinfo->lcdc.v_back_porch, pinfo->lcdc.v_front_porch,
		pinfo->lcdc.v_pulse_width);

	pinfo->type = EDP_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = edp_drv->edid.color_depth * RGB_COMPONENTS;
	pinfo->fb_num = 2;

	pinfo->lcdc.border_clr = 0;	 /* black */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;
}

static int __devexit mdss_edp_remove(struct platform_device *pdev)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;

	edp_drv = platform_get_drvdata(pdev);

	gpio_free(edp_drv->gpio_panel_en);
	mdss_edp_regulator_off(edp_drv);
	iounmap(edp_drv->base);
	iounmap(edp_drv->mmss_cc_base);
	edp_drv->base = NULL;

	return 0;
}

static int mdss_edp_device_register(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	mdss_edp_edid2pinfo(edp_drv);
	edp_drv->panel_data.panel_info.bl_min = 1;
	edp_drv->panel_data.panel_info.bl_max = 255;

	edp_drv->panel_data.event_handler = mdss_edp_event_handler;
	edp_drv->panel_data.set_backlight = mdss_edp_set_backlight;

	ret = mdss_register_panel(edp_drv->pdev, &edp_drv->panel_data);
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

	edp_drv->base_size = resource_size(res);
	edp_drv->base = ioremap(res->start, resource_size(res));
	if (!edp_drv->base) {
		pr_err("%s: Unable to remap EDP resources",  __func__);
		return -ENOMEM;
	}

	pr_debug("%s: drv=%x base=%x size=%x\n", __func__,
		(int)edp_drv, (int)edp_drv->base, edp_drv->base_size);

	mdss_debug_register_base("edp",
			edp_drv->base, edp_drv->base_size);

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

static void mdss_edp_video_ready(struct mdss_edp_drv_pdata *edp_drv)
{
	pr_debug("%s: edp_video_ready\n", __func__);

}

static int edp_event_thread(void *data)
{
	struct mdss_edp_drv_pdata *ep;
	unsigned long flag;
	u32 todo = 0;

	ep = (struct mdss_edp_drv_pdata *)data;

	while (1) {
		wait_event(ep->event_q, (ep->event_pndx != ep->event_gndx));
		spin_lock_irqsave(&ep->event_lock, flag);
		if (ep->event_pndx == ep->event_gndx) {
			spin_unlock_irqrestore(&ep->event_lock, flag);
			break;
		}
		todo = ep->event_todo_list[ep->event_gndx];
		ep->event_todo_list[ep->event_gndx++] = 0;
		ep->event_gndx %= HPD_EVENT_MAX;
		spin_unlock_irqrestore(&ep->event_lock, flag);

		pr_debug("%s: todo=%x\n", __func__, todo);

		if (todo == 0)
			continue;

		if (todo & EV_EDID_READ)
			mdss_edp_edid_read(ep, 0);

		if (todo & EV_DPCD_CAP_READ)
			mdss_edp_dpcd_cap_read(ep);

		if (todo & EV_DPCD_STATUS_READ)
			mdss_edp_dpcd_status_read(ep);

		if (todo & EV_LINK_TRAIN) {
			INIT_COMPLETION(ep->train_comp);
			mdss_edp_link_train(ep);
		}

		if (todo & EV_VIDEO_READY)
			mdss_edp_video_ready(ep);
	}

	return 0;
}

static void edp_send_events(struct mdss_edp_drv_pdata *ep, u32 events)
{
	spin_lock(&ep->event_lock);
	ep->event_todo_list[ep->event_pndx++] = events;
	ep->event_pndx %= HPD_EVENT_MAX;
	wake_up(&ep->event_q);
	spin_unlock(&ep->event_lock);
}

irqreturn_t edp_isr(int irq, void *ptr)
{
	struct mdss_edp_drv_pdata *ep = (struct mdss_edp_drv_pdata *)ptr;
	unsigned char *base = ep->base;
	u32 isr1, isr2, mask1, mask2;
	u32 ack;

	isr1 = edp_read(base + 0x308);
	isr2 = edp_read(base + 0x30c);

	mask1 = isr1 & EDP_INTR_MASK1;
	mask2 = isr2 & EDP_INTR_MASK2;

	isr1 &= ~mask1;	/* remove masks bit */
	isr2 &= ~mask2;

	pr_debug("%s: isr=%x mask=%x isr2=%x mask2=%x\n",
			__func__, isr1, mask1, isr2, mask2);

	ack = isr1 & EDP_INTR_STATUS1;
	ack <<= 1;	/* ack bits */
	ack |= mask1;
	edp_write(base + 0x308, ack);

	ack = isr2 & EDP_INTR_STATUS2;
	ack <<= 1;	/* ack bits */
	ack |= mask2;
	edp_write(base + 0x30c, ack);

	if (isr1 & EDP_INTR_HPD) {
		isr1 &= ~EDP_INTR_HPD;	/* clear */
		if (ep->train_start)
			edp_send_events(ep, EV_LINK_TRAIN);
	}

	if (isr2 & EDP_INTR_READY_FOR_VIDEO)
		edp_send_events(ep, EV_VIDEO_READY);

	if (isr1 && ep->aux_cmd_busy) {
		/* clear EDP_AUX_TRANS_CTRL */
		edp_write(base + 0x318, 0);
		/* read EDP_INTERRUPT_TRANS_NUM */
		ep->aux_trans_num = edp_read(base + 0x310);

		if (ep->aux_cmd_i2c)
			edp_aux_i2c_handler(ep, isr1);
		else
			edp_aux_native_handler(ep, isr1);
	}

	return IRQ_HANDLED;
}

struct mdss_hw mdss_edp_hw = {
	.hw_ndx = MDSS_HW_EDP,
	.ptr = NULL,
	.irq_handler = edp_isr,
};

static void mdss_edp_irq_enable(struct mdss_edp_drv_pdata *edp_drv)
{
	edp_write(edp_drv->base + 0x308, EDP_INTR_MASK1);
	edp_write(edp_drv->base + 0x30c, EDP_INTR_MASK2);

	mdss_enable_irq(&mdss_edp_hw);
}

static void mdss_edp_irq_disable(struct mdss_edp_drv_pdata *edp_drv)
{
	edp_write(edp_drv->base + 0x308, 0x0);
	edp_write(edp_drv->base + 0x30c, 0x0);

	mdss_disable_irq(&mdss_edp_hw);
}

static int mdss_edp_irq_setup(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret = 0;


	edp_drv->gpio_panel_hpd = of_get_named_gpio_flags(
			edp_drv->pdev->dev.of_node, "gpio-panel-hpd", 0,
			&edp_drv->hpd_flags);

	if (!gpio_is_valid(edp_drv->gpio_panel_hpd)) {
		pr_err("%s gpio_panel_hpd %d is not valid ", __func__,
				edp_drv->gpio_panel_hpd);
		return -ENODEV;
	}

	ret = gpio_request(edp_drv->gpio_panel_hpd, "edp_hpd_irq_gpio");
	if (ret) {
		pr_err("%s unable to request gpio_panel_hpd %d", __func__,
				edp_drv->gpio_panel_hpd);
		return -ENODEV;
	}

	ret = gpio_tlmm_config(GPIO_CFG(
					edp_drv->gpio_panel_hpd,
					1,
					GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL,
					GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	if (ret) {
		pr_err("%s: unable to config tlmm = %d\n", __func__,
				edp_drv->gpio_panel_hpd);
		gpio_free(edp_drv->gpio_panel_hpd);
		return -ENODEV;
	}

	ret = gpio_direction_input(edp_drv->gpio_panel_hpd);
	if (ret) {
		pr_err("%s unable to set direction for gpio_panel_hpd %d",
				__func__, edp_drv->gpio_panel_hpd);
		return -ENODEV;
	}

	mdss_edp_hw.ptr = (void *)(edp_drv);

	if (mdss_register_irq(&mdss_edp_hw))
		pr_err("%s: mdss_register_irq failed.\n", __func__);


	return 0;
}


static void mdss_edp_event_setup(struct mdss_edp_drv_pdata *ep)
{
	init_waitqueue_head(&ep->event_q);
	spin_lock_init(&ep->event_lock);

	kthread_run(edp_event_thread, (void *)ep, "mdss_edp_hpd");
}

static int __devinit mdss_edp_probe(struct platform_device *pdev)
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
	edp_drv->train_start = 0; /* no link train yet */

	ret = mdss_edp_get_base_address(edp_drv);
	if (ret)
		goto probe_err;

	ret = mdss_edp_get_mmss_cc_base_address(edp_drv);
	if (ret)
		goto edp_base_unmap;

	ret = mdss_edp_regulator_init(edp_drv);
	if (ret)
		goto mmss_cc_base_unmap;

	ret = mdss_edp_clk_init(edp_drv);
	if (ret)
		goto edp_clk_deinit;

	ret = mdss_edp_gpio_panel_en(edp_drv);
	if (ret)
		goto edp_clk_deinit;

	ret = mdss_edp_pwm_config(edp_drv);
	if (ret)
		goto edp_free_gpio_panel_en;

	mdss_edp_irq_setup(edp_drv);

	mdss_edp_aux_init(edp_drv);

	mdss_edp_event_setup(edp_drv);

	/* need mdss clock to receive irq */
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	/* only need aux and ahb clock for aux channel */
	mdss_edp_prepare_aux_clocks(edp_drv);
	mdss_edp_aux_clk_enable(edp_drv);
	mdss_edp_phy_pll_reset(edp_drv->base);
	mdss_edp_aux_reset(edp_drv->base);
	mdss_edp_mainlink_reset(edp_drv->base);
	mdss_edp_phy_powerup(edp_drv->base, 1);
	mdss_edp_aux_ctrl(edp_drv->base, 1);

	mdss_edp_irq_enable(edp_drv);

	mdss_edp_edid_read(edp_drv, 0);
	mdss_edp_dpcd_cap_read(edp_drv);

	mdss_edp_irq_disable(edp_drv);

	mdss_edp_aux_ctrl(edp_drv->base, 0);
	mdss_edp_aux_clk_disable(edp_drv);
	mdss_edp_phy_powerup(edp_drv->base, 0);
	mdss_edp_unprepare_aux_clocks(edp_drv);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mdss_edp_device_register(edp_drv);

	return 0;


edp_free_gpio_panel_en:
	gpio_free(edp_drv->gpio_panel_en);
edp_clk_deinit:
	mdss_edp_clk_deinit(edp_drv);
	mdss_edp_regulator_off(edp_drv);
mmss_cc_base_unmap:
	iounmap(edp_drv->mmss_cc_base);
edp_base_unmap:
	iounmap(edp_drv->base);
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
	.remove = __devexit_p(mdss_edp_remove),
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
