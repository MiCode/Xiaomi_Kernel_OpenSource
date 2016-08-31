/*
 * arch/arm/mach-tegra/panel-a-wqxga-7-9-x6.c
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
static u8 __maybe_unused test_11[] = {0xFF, 0xAA, 0x55, 0xA5, 0x80};
static u8 __maybe_unused test_12[] = {0xF7, 0x10, 0xD8, 0x6B};
static u8 __maybe_unused test_13[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
static u8 __maybe_unused test_14[] = {0xC5, 0x11, 0x11};
static u8 __maybe_unused test_15[] = {0x6f, 0x0c};
static u8 __maybe_unused test_16[] = {0xf9, 0x0A};
static u8 __maybe_unused test_17[] = {0x6f, 0x05};
static u8 __maybe_unused test_18[] = {0xf9, 0x00};
static u8 __maybe_unused test_19[] = {0x6f, 0x11};
static u8 __maybe_unused test_20[] = {0xf9, 0x0B};

static u8 __maybe_unused gamma_f0[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02};
static u8 __maybe_unused gamma_b0[] = {0xB0, 0x41};
static u8 __maybe_unused gamma_d1[] = {
0xD1, 0x00, 0x00, 0x00, 0x1D, 0x00, 0x46, 0x00, 0x63, 0x00,
0x7D, 0x00, 0xA6, 0x00, 0xC8, 0x00, 0xFC
};
static u8 __maybe_unused gamma_d2[] = {
0xD2, 0x01, 0x25, 0x01, 0x65, 0x01, 0x99, 0x01, 0xEA, 0x02,
0x29, 0x02, 0x2B, 0x02, 0x61, 0x02, 0x9C
};
static u8 __maybe_unused gamma_d3[] = {
0xD3, 0x02, 0xC0, 0x02, 0xF2, 0x03, 0x14, 0x03, 0x40, 0x03,
0x5D, 0x03, 0x7F, 0x03, 0x94, 0x03, 0xB0
};
static u8 __maybe_unused gamma_d4[] = {
0xD4, 0x03, 0xDC, 0x03, 0xFF
};
static struct tegra_dsi_cmd __maybe_unused a_gamma_cmds[] = {
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, gamma_f0),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_1_PARAM, 0xEE, 0x01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, gamma_b0),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, gamma_d1),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, gamma_d2),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, gamma_d3),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, gamma_d4),
};

static struct tegra_dsi_cmd __maybe_unused bist_cmd[] = {



	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, test_01),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, test_02),
	DSI_CMD_LONG_BOTH(DSI_GENERIC_LONG_WRITE, test_03),
};

static struct tegra_dsi_cmd dsi_a_wqxga_7_9_init_cmd[] = {
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, DSI_DCS_NO_OP),
	DSI_DLY_MS(120),

	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, DSI_DCS_NO_OP),
};

static struct tegra_dsi_cmd dsi_a_wqxga_7_9_suspend_cmd[] = {
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_OFF, DSI_DCS_NO_OP),
	DSI_DLY_MS(100),
	DSI_CMD_SHORT_BOTH(DSI_DCS_WRITE_0_PARAM, DSI_DCS_ENTER_SLEEP_MODE, DSI_DCS_NO_OP),
	DSI_DLY_MS(150)
};

static struct tegra_dsi_out dsi_a_wqxga_7_9_pdata = {
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
	.dsi_init_cmd = dsi_a_wqxga_7_9_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_a_wqxga_7_9_init_cmd),
	.dsi_suspend_cmd = dsi_a_wqxga_7_9_suspend_cmd,
	.n_suspend_cmd = ARRAY_SIZE(dsi_a_wqxga_7_9_suspend_cmd),
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

	err = gpio_request(dsi_a_wqxga_7_9_pdata.dsi_panel_rst_gpio,
		"panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	/* free pwm GPIO */
	err = gpio_request(dsi_a_wqxga_7_9_pdata.dsi_panel_bl_pwm_gpio,
		"panel pwm");
	if (err < 0) {
		pr_err("panel pwm gpio request failed\n");
		goto fail;
	}
	gpio_free(dsi_a_wqxga_7_9_pdata.dsi_panel_bl_pwm_gpio);
	gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_a_wqxga_7_9_postpoweron(struct device *dev)
{
	return 0;
}

static int dsi_a_wqxga_7_9_enable(struct device *dev)
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
	gpio_direction_output(dsi_a_wqxga_7_9_pdata.dsi_panel_rst_gpio, 1);
	usleep_range(1000, 3000);
	gpio_set_value(dsi_a_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	usleep_range(1000, 3000);
	gpio_set_value(dsi_a_wqxga_7_9_pdata.dsi_panel_rst_gpio, 1);
	msleep(32);
#endif

	return 0;
fail:
	return err;

}

static int dsi_a_wqxga_7_9_disable(void)
{
	gpio_set_value(dsi_a_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	msleep(10);
	if (avdd_lcd_vsn_5v5)
		regulator_disable(avdd_lcd_vsn_5v5);
	msleep(10);
	if (avdd_lcd_vsp_5v5)
		regulator_disable(avdd_lcd_vsp_5v5);
	msleep(10);
	if (dvdd_lcd_1v8)
		regulator_disable(dvdd_lcd_1v8);
fail:
	return 0;
}

static int dsi_a_wqxga_7_9_postsuspend(void)
{
	pr_info("%s\n", __func__);
	gpio_set_value(dsi_a_wqxga_7_9_pdata.dsi_panel_rst_gpio, 0);
	return 0;
}

static struct tegra_dc_mode dsi_a_wqxga_7_9_modes[] = {
	{
		.pclk = 216500000, /* @60Hz */
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 4,
		.v_sync_width = 1,
		.h_back_porch = 100,
		.v_back_porch = 10,
		.h_active = 1536,
		.v_active = 2048,
		.h_front_porch = 104,
		.v_front_porch = 10,
	},
};

static void dsi_a_wqxga_7_9_set_disp_device(
	struct platform_device *board_display_device)
{
	disp_device = board_display_device;
}

static void dsi_a_wqxga_7_9_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_a_wqxga_7_9_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_a_wqxga_7_9_modes;
	dc->n_modes = ARRAY_SIZE(dsi_a_wqxga_7_9_modes);
	dc->enable = dsi_a_wqxga_7_9_enable;
	dc->postpoweron = dsi_a_wqxga_7_9_postpoweron;
	dc->disable = dsi_a_wqxga_7_9_disable;
	dc->postsuspend	= dsi_a_wqxga_7_9_postsuspend,
	dc->width = 120;
	dc->height = 160;
	dc->flags = DC_CTRL_MODE;
}

struct tegra_dsi_cmd *p_gamma_cmds = NULL;
u32 n_gamma_cmds = 0;
struct tegra_dsi_cmd *p_ce_cmds = NULL;
u32 n_ce_cmds = 0;
EXPORT_SYMBOL(p_gamma_cmds);
EXPORT_SYMBOL(n_gamma_cmds);
EXPORT_SYMBOL(p_ce_cmds);
EXPORT_SYMBOL(n_ce_cmds);

struct tegra_dsi_cmd *p_bist_cmd = NULL;
u16 n_bist_cmd = 0;
EXPORT_SYMBOL(p_bist_cmd);
EXPORT_SYMBOL(n_bist_cmd);

extern void tegra_dsi_set_dispparam(struct tegra_dsi_cmd *cmds, int cmds_cnt);

static void dsi_a_wqxga_7_9_set_dispparam(unsigned int param)
{
	unsigned int temp;

	temp = param & 0x0000000F;
	switch (temp) {		/* Gamma */
	case 0x1:	/* 22*/
		tegra_dsi_set_dispparam(a_gamma_cmds, ARRAY_SIZE(a_gamma_cmds));
		p_gamma_cmds = a_gamma_cmds;
		n_gamma_cmds = ARRAY_SIZE(a_gamma_cmds);
		pr_info("panel: %s() - set a_gamma_cmds\n", __func__);
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
	switch (temp) {		/* Other Func */
	case 0xA000:
		pr_info("panel: enable panel BIST mode \n");
		p_bist_cmd = bist_cmd;
		n_bist_cmd = ARRAY_SIZE(bist_cmd);
		break;
	case 0xB000:
		pr_info("panel: return to normal mode from BIST mode \n");
		p_bist_cmd = dsi_a_wqxga_7_9_init_cmd;
		n_bist_cmd = ARRAY_SIZE(dsi_a_wqxga_7_9_init_cmd);
		break;
	default:
		break;
	}
}

static void dsi_a_wqxga_7_9_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_a_wqxga_7_9_modes[0].h_active;
	fb->yres = dsi_a_wqxga_7_9_modes[0].v_active;
}

struct tegra_panel dsi_a_wqxga_7_9_x6 = {
	.init_dc_out = dsi_a_wqxga_7_9_dc_out_init,
	.init_fb_data = dsi_a_wqxga_7_9_fb_data_init,
	.set_disp_device = dsi_a_wqxga_7_9_set_disp_device,
	.set_dispparam = dsi_a_wqxga_7_9_set_dispparam,
};
EXPORT_SYMBOL(dsi_a_wqxga_7_9_x6);

