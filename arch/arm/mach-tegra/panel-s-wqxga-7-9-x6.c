/*
 * arch/arm/mach-tegra/panel-s-wqxga-7-9-x6.c
 *
 * Copyright (c) 2014, XIAOMI CORPORATION. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http:
 */

#include <mach/dc.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/max8831_backlight.h>
#include <linux/platform_data/lp855x.h>
#include <linux/leds.h>
#include <linux/ioport.h>
#include <generated/mach-types.h>
#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra11_host1x_devices.h"

#define TEGRA_DSI_GANGED_MODE	1

#define DSI_PANEL_RESET		1

#define DC_CTRL_MODE	(TEGRA_DC_OUT_CONTINUOUS_MODE | \
						TEGRA_DC_OUT_INITIALIZED_MODE)

static bool reg_requested;
static bool gpio_requested;
static struct platform_device *disp_device;
static struct regulator *avdd_lcd_vsp_5v5;
static struct regulator *avdd_lcd_vsn_5v5;
static struct regulator *dvdd_lcd_1v8;

static u8 __maybe_unused test_01[] = {0x6A, 0x00};
static u8 __maybe_unused test_02[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
static u8 __maybe_unused test_03[] = {0xEE, 0x87, 0x78, 0x02, 0x40};

static struct tegra_dsi_cmd __maybe_unused bist_cmd[] = {
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_1_PARAM, 0x51, 0xC8),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_1_PARAM, 0x55, 0x01),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_1_PARAM, 0x53, 0x2C),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, test_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, test_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, test_03),
};

static struct tegra_dsi_cmd dsi_s_wqxga_7_9_init_cmd[] = {
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, DSI_DCS_NO_OP),
	DSI_DLY_MS(120),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_1_PARAM, 0x51, 0xFF),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_1_PARAM, 0x55, 0x01),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_1_PARAM, 0x53, 0x2C),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, DSI_DCS_NO_OP)
};

static struct tegra_dsi_cmd dsi_s_wqxga_7_9_suspend_cmd[] = {
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_OFF, DSI_DCS_NO_OP),
	DSI_DLY_MS(100),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_ENTER_SLEEP_MODE, DSI_DCS_NO_OP),
	DSI_DLY_MS(150)
};

static struct tegra_dsi_out dsi_s_wqxga_7_9_pdata = {
	.controller_vs = DSI_VS_1,

	.n_data_lanes = 8,
	.ganged_type = TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE,
	.refresh_rate = 60,

	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,
	.dsi_init_cmd = dsi_s_wqxga_7_9_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_s_wqxga_7_9_init_cmd),
	.dsi_suspend_cmd = dsi_s_wqxga_7_9_suspend_cmd,
	.n_suspend_cmd = ARRAY_SIZE(dsi_s_wqxga_7_9_suspend_cmd),
	.bl_name = "pwm-backlight",
	.lp00_pre_panel_wakeup = true,
	.ulpm_not_supported = true,
	.no_pkt_seq_hbp = false,
};

static int ardbeg_dsi_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;
	dvdd_lcd_1v8 = regulator_get(dev, "dvdd_lcdio");
	if (IS_ERR_OR_NULL(dvdd_lcd_1v8)) {
		err = PTR_ERR(dvdd_lcd_1v8);
		dvdd_lcd_1v8 = NULL;
		goto fail;
	}
	avdd_lcd_vsp_5v5 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_vsp_5v5)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_vsp_5v5);
		avdd_lcd_vsp_5v5 = NULL;
		goto fail;
	}
	avdd_lcd_vsn_5v5 = regulator_get(dev, "bvdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_vsn_5v5)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_vsn_5v5);
		avdd_lcd_vsn_5v5 = NULL;
		goto fail;
	}

	reg_requested = true;
	return 0;
fail:
	return err;
}

static int ardbeg_dsi_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	err = gpio_request(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio,
		"panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	err = gpio_request(dsi_s_wqxga_7_9_pdata.dsi_panel_bl_pwm_gpio,
		"panel pwm");
	if (err < 0) {
		pr_err("panel pwm gpio request failed\n");
		goto fail;
	}
	gpio_free(dsi_s_wqxga_7_9_pdata.dsi_panel_bl_pwm_gpio);
	gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_s_wqxga_7_9_postpoweron(struct device *dev)
{
	return 0;
}

static int dsi_s_wqxga_7_9_enable(struct device *dev)
{
	int err = 0;

	struct tegra_dc_out *mocha_disp_out =
		((struct tegra_dc_platform_data *)
			(disp_device->dev.platform_data))->default_out;

	err = ardbeg_dsi_regulator_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = ardbeg_dsi_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}

	if (dvdd_lcd_1v8) {
		err = regulator_enable(dvdd_lcd_1v8);
		if (err < 0) {
			pr_err("dvdd_lcd regulator enable failed\n");
			goto fail;
		}
		msleep(12);
	}

	if (avdd_lcd_vsp_5v5) {
		err = regulator_enable(avdd_lcd_vsp_5v5);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
		msleep(12);
	}

	if (avdd_lcd_vsn_5v5) {
		err = regulator_enable(avdd_lcd_vsn_5v5);
		if (err < 0) {
			pr_err("bvdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	if ((mocha_disp_out->flags & TEGRA_DC_OUT_INITIALIZED_MODE)) {
		pr_info("panel: %s - TEGRA_DC_OUT_INITIALIZED_MODE flag marked\n", __func__);
		goto fail;
	}

	if (avdd_lcd_vsn_5v5)
		msleep(24);
#if DSI_PANEL_RESET
	pr_info("panel: %s\n", __func__);
	gpio_direction_output(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 1);
	usleep_range(1000, 3000);
	gpio_set_value(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	usleep_range(1000, 3000);
	gpio_set_value(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 1);
	msleep(32);
#endif

	return 0;
fail:
	return err;

}

static int dsi_s_wqxga_7_9_disable(void)
{
	pr_info("panel: %s\n", __func__);
	gpio_set_value(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	msleep(10);
	if (avdd_lcd_vsn_5v5)
		regulator_disable(avdd_lcd_vsn_5v5);
	msleep(10);
	if (avdd_lcd_vsp_5v5)
		regulator_disable(avdd_lcd_vsp_5v5);
	msleep(10);
	if (dvdd_lcd_1v8)
		regulator_disable(dvdd_lcd_1v8);

	return 0;
}

static int dsi_s_wqxga_7_9_postsuspend(void)
{
	pr_info("%s\n", __func__);
	gpio_set_value(dsi_s_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	return 0;
}

static struct tegra_dc_mode dsi_s_wqxga_7_9_modes[] = {
	{
		.pclk = 214824960, /* @60Hz */
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 28,
		.v_sync_width = 2,
		.h_back_porch = 28,
		.v_back_porch = 8,
		.h_active = 1536,
		.v_active = 2048,
		.h_front_porch = 136,
		.v_front_porch = 14,
	},
};

static void dsi_s_wqxga_7_9_set_disp_device(
	struct platform_device *board_display_device)
{
	disp_device = board_display_device;
}

static void dsi_s_wqxga_7_9_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_s_wqxga_7_9_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_s_wqxga_7_9_modes;
	dc->n_modes = ARRAY_SIZE(dsi_s_wqxga_7_9_modes);
	dc->enable = dsi_s_wqxga_7_9_enable;
	dc->postpoweron = dsi_s_wqxga_7_9_postpoweron;
	dc->disable = dsi_s_wqxga_7_9_disable;
	dc->postsuspend = dsi_s_wqxga_7_9_postsuspend,
	dc->width = 120;
	dc->height = 160;
	dc->flags = DC_CTRL_MODE;
}

extern struct tegra_dsi_cmd *p_bist_cmd;
extern u16 n_bist_cmd;
static void dsi_s_wqxga_7_9_set_dispparam(unsigned int param)
{
	unsigned int temp;

	temp = param & 0x0000000F;
	switch (temp) {		/* Gamma */
	case 0x1:	/* 22*/
		break;
	case 0x2:	/* 24 */
		break;
	case 0x3:	/* 25 */
		break;
	case 0xF:
		break;
	default:
		break;
	}


	temp = param & 0x0000F000;
	switch (temp) {
	case 0xA000:
		pr_info("panel: enable panel BIST mode \n");
		p_bist_cmd = bist_cmd;
		n_bist_cmd = ARRAY_SIZE(bist_cmd);
		break;
	case 0xB000:
		pr_info("panel: return to normal mode from BIST mode \n");
		p_bist_cmd = dsi_s_wqxga_7_9_init_cmd;
		n_bist_cmd = ARRAY_SIZE(dsi_s_wqxga_7_9_init_cmd);
		break;
	default:
		break;
	}
}

static void dsi_s_wqxga_7_9_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_s_wqxga_7_9_modes[0].h_active;
	fb->yres = dsi_s_wqxga_7_9_modes[0].v_active;
}

struct tegra_panel dsi_s_wqxga_7_9_x6 = {
	.init_dc_out = dsi_s_wqxga_7_9_dc_out_init,
	.init_fb_data = dsi_s_wqxga_7_9_fb_data_init,
	.set_disp_device = dsi_s_wqxga_7_9_set_disp_device,
	.set_dispparam = dsi_s_wqxga_7_9_set_dispparam,
};
EXPORT_SYMBOL(dsi_s_wqxga_7_9_x6);

