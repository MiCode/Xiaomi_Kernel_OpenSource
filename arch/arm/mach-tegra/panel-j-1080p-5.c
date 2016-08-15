/*
 * arch/arm/mach-tegra/panel-j-1080p-5.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mach/dc.h>
#include <mach/iomap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/ioport.h>

#include "gpio-names.h"
#include "board-panel.h"
#include "board-pisces.h"

#define DSI_PANEL_RESET         1
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH5

#define LCD_VSN_EN TEGRA_GPIO_PS0
#define LCD_VSP_EN TEGRA_GPIO_PI4

#define DC_CTRL_MODE            TEGRA_DC_OUT_CONTINUOUS_MODE

static struct regulator *vdd_lcd_s_1v8;

static bool dsi_j_1080p_5_reg_requested;
static bool dsi_j_1080p_5_gpio_requested;

struct tegra_dc_mode dsi_j_1080p_5_modes[] = {
	/* 1080x1920@60Hz */
	{
		.pclk = 36000000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 10,
		.v_sync_width = 1,
		.h_back_porch = 82,
		.v_back_porch = 4,
		.h_active = 1080,
		.v_active = 1920,
		.h_front_porch = 100,
		.v_front_porch = 3,
	},
};

static int dsi_j_1080p_5_reg_get(void)
{
	int err = 0;

	if (dsi_j_1080p_5_reg_requested)
		return 0;

	vdd_lcd_s_1v8 = regulator_get(NULL, "vdd_lcd_1v8_s");
	if (IS_ERR_OR_NULL(vdd_lcd_s_1v8)) {
		pr_err("vdd_lcd_1v8_s regulator get failed\n");
		err = PTR_ERR(vdd_lcd_s_1v8);
		vdd_lcd_s_1v8 = NULL;
		goto fail;
	}

	dsi_j_1080p_5_reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_j_1080p_5_gpio_get(void)
{
	int err = 0;

	if (dsi_j_1080p_5_gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	err = gpio_request(LCD_VSN_EN, "lcd vsn en");
	if (err < 0) {
		pr_err("lcd vsn en gpio request failed, err=%d\n", err);
		goto fail;
	}

	err = gpio_request(LCD_VSP_EN, "lcd vsp en");
	if (err < 0) {
		pr_err("lcd vsp en gpio request failed, err=%d\n", err);
		goto fail;
	}

	dsi_j_1080p_5_gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_j_1080p_5_enable(struct device *dev)
{
	int err = 0;

	err = dsi_j_1080p_5_reg_get();
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = dsi_j_1080p_5_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}
	gpio_direction_output(DSI_PANEL_RST_GPIO, 0);
	usleep_range(3000, 5000);

	if (vdd_lcd_s_1v8) {
		err = regulator_enable(vdd_lcd_s_1v8);
		if (err < 0) {
			pr_err("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);

	gpio_direction_output(LCD_VSP_EN, 0);
	usleep_range(3000, 5000);
	gpio_set_value(LCD_VSP_EN, 1);
	msleep(1);
	gpio_direction_output(LCD_VSN_EN, 0);
	usleep_range(3000, 5000);
	gpio_set_value(LCD_VSN_EN, 1);
	msleep(1);

#if DSI_PANEL_RESET
	gpio_set_value(DSI_PANEL_RST_GPIO, 1);
	msleep(20);
#endif

	return 0;
fail:
	return err;
}

static u8 MCAP_off[] = {0xb0, 0x04};
static u8 hsync_output[] = {0xc3, 0x01, 0x00, 0x10};
static u8 pwm_freq_chg[] = {0xce, 0x00, 0x01, 0x88, 0xc1, 0x00, 0x1e, 0x04};
static u8 pwm_freq_chg_done[] = {0xd6, 0x01};
static u8 MCAP_on[] = {0xb0, 0x03};
static u8 brightness_settting[] = {0x51, 0x0f, 0xff};

static struct tegra_dsi_cmd dsi_j_1080p_5_init_cmd[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, 0x0, 0x0),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, 0x0, 0x0),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, hsync_output),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, pwm_freq_chg),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, pwm_freq_chg_done),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, 0x0, 0x0),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, 0x0, 0x0),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x55, 0x00),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x53, 0x2c),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x36, 0x00),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x35, 0x00),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, 0x29, 0x0),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, 0x11, 0x0),
	DSI_DLY_MS(120),
	DSI_CMD_LONG(DSI_DCS_LWRITE, brightness_settting),
};

static struct tegra_dsi_out dsi_j_1080p_5_pdata = {
	.n_data_lanes = 4,

	.dsi_instance = DSI_INSTANCE_0,

	.refresh_rate = 60,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,
	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.dsi_init_cmd = dsi_j_1080p_5_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_j_1080p_5_init_cmd),
};

static int dsi_j_1080p_5_disable(void)
{
	int err = 0;

	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	msleep(2);
	gpio_set_value(LCD_VSN_EN, 0);
	msleep(2);
	gpio_set_value(LCD_VSP_EN, 0);
	msleep(2);
	if (vdd_lcd_s_1v8) {
		err = regulator_disable(vdd_lcd_s_1v8);
		if (err < 0) {
			pr_err("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	}
	return 0;
fail:
	return err;
}

static void dsi_j_1080p_5_resources_init(struct resource *resources,
			int n_resources)
{
	int i;
	for (i = 0; i < n_resources; i++) {
		struct resource *r = &resources[i];
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "dsi_regs")) {
			r->start = TEGRA_DSI_BASE;
			r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
		}
	}
}

static void dsi_j_1080p_5_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_j_1080p_5_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_j_1080p_5_modes;
	dc->n_modes = ARRAY_SIZE(dsi_j_1080p_5_modes);
	dc->enable = dsi_j_1080p_5_enable;
	dc->disable = dsi_j_1080p_5_disable;
	dc->width = 62;
	dc->height = 110;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_j_1080p_5_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_j_1080p_5_modes[0].h_active;
	fb->yres = dsi_j_1080p_5_modes[0].v_active;
}

struct tegra_panel __initdata dsi_j_1080p_5 = {
	.init_dc_out = dsi_j_1080p_5_dc_out_init,
	.init_fb_data = dsi_j_1080p_5_fb_data_init,
	.init_resources = dsi_j_1080p_5_resources_init,
};
EXPORT_SYMBOL(dsi_j_1080p_5);
