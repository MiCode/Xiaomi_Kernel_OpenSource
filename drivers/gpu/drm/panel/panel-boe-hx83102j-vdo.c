// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

#include "ktz8866.h"

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

static char bl_tb0[] = { 0x51, 0x0f, 0xff };
#ifdef HIMAX_WAKE_UP
extern uint8_t wake_flag_drm;
#endif
//TO DO: You have to do that remove macro BYPASSI2C and solve build error
//otherwise voltage will be unstable
struct boe {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	unsigned int gate_ic;

	int error;
};

#define boe_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		boe_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define boe_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		boe_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct boe *panel_to_boe(struct drm_panel *panel)
{
	return container_of(panel, struct boe, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int boe_dcs_read(struct boe *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void boe_panel_get_data(struct boe *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = boe_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void boe_dcs_write(struct boe *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
}

static void boe_panel_init(struct boe *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	boe_dcs_write_seq_static(ctx, 0xB9, 0x83, 0x10, 0x21, 0x55, 0x00);
	boe_dcs_write_seq_static(ctx, 0xB1, 0x2C, 0xB3, 0xB3, 0x2F, 0xEF, 0x43, 0xE1, 0x50,
	0x36, 0x36, 0x36, 0x36,
	0x1A, 0x8B, 0x11, 0x65, 0x00, 0x88, 0xFA, 0xFF, 0xFF, 0x8F, 0xFF, 0x08, 0x9A, 0x33);
	boe_dcs_write_seq_static(ctx, 0xB2, 0x00, 0x47, 0xB0, 0xD0, 0x00, 0x12, 0x5A, 0x3C,
	0x9D, 0x32, 0x30, 0x10, 0x00, 0x88, 0xF5);
	boe_dcs_write_seq_static(ctx, 0xB4, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x9A, 0x85,
	0x7C, 0x70, 0x01, 0x9E);
	boe_dcs_write_seq_static(ctx, 0xB6, 0x34, 0x34, 0x03);
	boe_dcs_write_seq_static(ctx, 0xE9, 0xCD);
	boe_dcs_write_seq_static(ctx, 0xBA, 0x84);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	boe_dcs_write_seq_static(ctx, 0xBC, 0x1B, 0x04);
	boe_dcs_write_seq_static(ctx, 0xBE, 0x20);
	boe_dcs_write_seq_static(ctx, 0xC0, 0x36, 0x36, 0x22, 0x11, 0x22, 0xA0, 0x61, 0x08,
	0xF5, 0x03);
	boe_dcs_write_seq_static(ctx, 0xE9, 0xCC);
	boe_dcs_write_seq_static(ctx, 0xC7, 0x80);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	boe_dcs_write_seq_static(ctx, 0xE9, 0xC6);
	boe_dcs_write_seq_static(ctx, 0xC8, 0x97);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	boe_dcs_write_seq_static(ctx, 0xC9, 0x00, 0x1E, 0x0F, 0xA0, 0x01);
	boe_dcs_write_seq_static(ctx, 0xCB, 0x08, 0x13, 0x07, 0x00, 0x0E, 0xCB);
	boe_dcs_write_seq_static(ctx, 0xCC, 0x02, 0x03, 0x44);
	boe_dcs_write_seq_static(ctx, 0xE9, 0xC4);
	boe_dcs_write_seq_static(ctx, 0xD0, 0x03);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	boe_dcs_write_seq_static(ctx, 0xD1, 0x37, 0x06, 0x00, 0x02, 0x04, 0x0C, 0xFF);
	boe_dcs_write_seq_static(ctx, 0xD3, 0x06, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08, 0x04,
	0x08, 0x37, 0x47, 0x64, 0x4B, 0x11, 0x11, 0x03, 0x03, 0x32, 0x10, 0x0C, 0x00, 0x0C,
	0x32, 0x10, 0x08, 0x00, 0x08, 0x32, 0x17, 0xE7, 0x07, 0xE7, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xD5, 0x24, 0x24, 0x24, 0x24, 0x07, 0x06, 0x07, 0x06,
	0x05, 0x04, 0x05, 0x04, 0x03, 0x02, 0x03, 0x02, 0x01, 0x00, 0x01, 0x00, 0x21, 0x20,
	0x21, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x1F, 0x1F, 0x1F,
	0x1E, 0x1E, 0x1E, 0x1E, 0x18, 0x18, 0x18, 0x18);
	boe_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAA, 0xAA, 0xFA, 0xA0, 0xAA, 0xAA,
	0xAA, 0xAA, 0xFA, 0xA0);
	boe_dcs_write_seq_static(ctx, 0xE0, 0x00, 0x04, 0x0C, 0x14, 0x1B, 0x2F, 0x4B, 0x54,
	0x5E, 0x5C, 0x7A, 0x82, 0x89, 0x99, 0x97, 0x9E, 0xA6, 0xB7, 0xB4, 0x57, 0x5E, 0x68,
	0x73, 0x00, 0x04, 0x0C, 0x14, 0x1B, 0x2F, 0x4B, 0x54, 0x5E, 0x5C, 0x7A, 0x82, 0x89,
	0x99, 0x97, 0x9E, 0xA6, 0xB7, 0xB4, 0x57, 0x5E, 0x68, 0x73);
	boe_dcs_write_seq_static(ctx, 0xE7, 0x0B, 0x0E, 0x0E, 0x1E, 0x29, 0x9E, 0x01, 0x62,
	0x47, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x12, 0x05, 0x02, 0x02, 0x0E);
	boe_dcs_write_seq_static(ctx, 0xE1, 0x12, 0x00, 0x00, 0x89, 0x30, 0x80, 0x07, 0xD0,
	0x02, 0x58, 0x00, 0x14, 0x02, 0x58, 0x02, 0x58, 0x02, 0x00, 0x02, 0x2C, 0x00, 0x20,
	0x02, 0x02, 0x00, 0x08, 0x00, 0x0C, 0x05, 0x0E, 0x04, 0x94, 0x18, 0x00, 0x10, 0xF0,
	0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E);
	boe_dcs_write_seq_static(ctx, 0xBD, 0x01);
	boe_dcs_write_seq_static(ctx, 0xB1, 0x01, 0xBF, 0x11);
	boe_dcs_write_seq_static(ctx, 0xCB, 0x86);
	boe_dcs_write_seq_static(ctx, 0xD2, 0xF0);
	boe_dcs_write_seq_static(ctx, 0xE9, 0xC9);
	boe_dcs_write_seq_static(ctx, 0xD3, 0x84);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	boe_dcs_write_seq_static(ctx, 0xD8, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0, 0xFF, 0xFF,
	0xFA, 0xAF, 0xFF, 0xA0, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0, 0xFF, 0xFF, 0xFA, 0xAF,
	0xFF, 0xA0, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0,
	0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0);
	boe_dcs_write_seq_static(ctx, 0xE7, 0x02, 0x00, 0x32, 0x01, 0x92, 0x0D, 0xD4, 0x0E,
	0xA0, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xBD, 0x02);
	boe_dcs_write_seq_static(ctx, 0xD8, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0, 0xFF, 0xFF,
	0xFA, 0xAF, 0xFF, 0xA0);
	boe_dcs_write_seq_static(ctx, 0xE7, 0xFE, 0x03, 0xFE, 0x03, 0xFE, 0x03, 0x02, 0x02,
	0x02, 0x25, 0x00, 0x25, 0x81, 0x02, 0x40, 0x00, 0x20, 0x49, 0x04, 0x03, 0x02, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00);
	boe_dcs_write_seq_static(ctx, 0xBD, 0x03);
	boe_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAA, 0xAA, 0xFA, 0xA0, 0xAA, 0xAA,
	0xAA, 0xAA, 0xFA, 0xA0, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0, 0xFF, 0xFF, 0xFA, 0xAF,
	0xFF, 0xA0, 0x55, 0x55, 0x55, 0x55, 0x55, 0x50, 0x55, 0x55, 0x55, 0x55, 0x55, 0x50);
	boe_dcs_write_seq_static(ctx, 0xE1, 0x00);
	boe_dcs_write_seq_static(ctx, 0xBD, 0x00);
	boe_dcs_write_seq_static(ctx, 0xE9, 0xC4);
	boe_dcs_write_seq_static(ctx, 0xBA, 0x96);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	boe_dcs_write_seq_static(ctx, 0xBD, 0x01);
	boe_dcs_write_seq_static(ctx, 0xE9, 0xC5);
	boe_dcs_write_seq_static(ctx, 0xBA, 0x4F);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	boe_dcs_write_seq_static(ctx, 0xBD, 0x00);
	boe_dcs_write_seq_static(ctx, 0x51, 0x0F, 0xFF);
	boe_dcs_write_seq_static(ctx, 0x53, 0x2C);
	boe_dcs_write_seq_static(ctx, 0xC9, 0x00, 0x1E, 0x0A, 0xB2, 0x01);
	boe_dcs_write_seq_static(ctx, 0x11, 0x00);
	msleep(120);
	boe_dcs_write_seq_static(ctx, 0x29, 0x00);
	msleep(20);

	//boe_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1]);

	pr_info("%s-\n", __func__);
}

static int boe_disable(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int boe_unprepare(struct drm_panel *panel)
{

	struct boe *ctx = panel_to_boe(panel);

	if (!ctx->prepared)
		return 0;

	boe_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	boe_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(150);

#ifdef HIMAX_WAKE_UP
	if (wake_flag_drm == 0) {
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	}
#else
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->bias_neg =
		devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	usleep_range(2000, 2001);

	ctx->bias_pos =
		devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

#ifdef HIMAX_WAKE_UP
	if (wake_flag_drm == 0)
		lcd_set_bias(0);
#else
	lcd_set_bias(0);
#endif

	ctx->error = 0;
	ctx->prepared = false;
#endif
	return 0;
}

static int boe_prepare(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// lcd reset H -> L -> L
	//ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	//gpiod_set_value(ctx->reset_gpio, 1);
	//usleep_range(10000, 10001);
	//gpiod_set_value(ctx->reset_gpio, 0);
	//msleep(20);
	//gpiod_set_value(ctx->reset_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end
	ctx->bias_pos =
		devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	usleep_range(2000, 2001);
	ctx->bias_neg =
		devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

#ifdef HIMAX_WAKE_UP
	if (wake_flag_drm == 0)
		lcd_set_bias(1);
#else
	lcd_set_bias(1);
#endif
#ifndef BYPASSI2C
//	_lcm_i2c_write_bytes(0x0, 0xf);
//	_lcm_i2c_write_bytes(0x1, 0xf);
#endif
	boe_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0) {
		boe_unprepare(panel);
		pr_info("%s11111-\n", __func__);
	}
	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	boe_panel_get_data(ctx);
#endif

#ifdef VENDOR_EDIT
	// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
	lcd_queue_load_tp_fw();
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int boe_enable(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 370506,
	.hdisplay = 1200,
	.hsync_start = 1200 + 60,//HFP
	.hsync_end = 1200 + 60 + 20,//HSA
	.htotal = 1200 + 60 + 20 + 40,//HBP
	.vdisplay = 2000,
	.vsync_start = 2000 + 92,//VFP
	.vsync_end = 2000 + 92 + 8,//VSA
	.vtotal = 2000 + 92 + 8 + 12,//VBP
};

static const struct drm_display_mode performance_mode_90hz = {
	.clock = 370431,
	.hdisplay = 1200,
	.hsync_start = 1200 + 60,//HFP
	.hsync_end = 1200 + 60 + 20,//HSA
	.htotal = 1200 + 60 + 20 + 40,//HBP
	.vdisplay = 2000,
	.vsync_start = 2400 + 92,//VFP
	.vsync_end = 2400 + 92 + 8,//VSA
	.vtotal = 2400 + 92 + 8 + 12,//VBP
};

static const struct drm_display_mode performance_mode_120hz = {
	.clock = 370506,
	.hdisplay = 1200,
	.hsync_start = 1200 + 60,//HFP
	.hsync_end = 1200 + 60 + 20,//HSA
	.htotal = 1200 + 60 + 20 + 40,//HBP
	.vdisplay = 2000,
	.vsync_start = 2000 + 92,//VFP
	.vsync_end = 2000 + 92 + 8,//VSA
	.vtotal = 2000 + 92 + 8 + 12,//VBP
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 502,
	.vfp_low_power = 4180,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 1,
	.lane_swap_en = 0,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 0,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2000,
		.pic_width = 1200,
		.slice_height = 20,
		.slice_width = 600,
		.chunk_size = 600,
		.xmit_delay = 512,
		.dec_delay = 556,
		.scale_value = 32,
		.increment_interval = 514,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1172,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1003,
	.lfr_enable = 0,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 60,
	//	.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
	//	.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
	//	.dfps_cmd_table[2] = {0, 2, {0x18, 0x21} },
		/*switch page for esd check*/
	//	.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
	//	.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 0,
		.pll_clk = 502,
	//	.vfp_lp_dyn = 4178,
	//	.hfp = 76,
	//	.vfp = 2575,
	},
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 510,
	.vfp_low_power = 2528,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 1,
	.lane_swap_en = 0,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 0,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2000,
		.pic_width = 1200,
		.slice_height = 20,
		.slice_width = 600,
		.chunk_size = 600,
		.xmit_delay = 512,
		.dec_delay = 556,
		.scale_value = 32,
		.increment_interval = 514,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1172,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1020,
	.lfr_enable = 0,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 120,
		//.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		//.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		//.dfps_cmd_table[2] = {0, 2, {0x18, 0x20} },
		/*switch page for esd check*/
		//.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		//.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 415,
	//	.vfp_lp_dyn = 2528,
	//	.hfp = 76,
	//	.vfp = 905,
	},
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 510,
	.vfp_low_power = 2528,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 1,
	.lane_swap_en = 0,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 0,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2000,
		.pic_width = 1200,
		.slice_height = 20,
		.slice_width = 600,
		.chunk_size = 600,
		.xmit_delay = 512,
		.dec_delay = 556,
		.scale_value = 32,
		.increment_interval = 514,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1172,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 1020,
	.lfr_enable = 0,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
	//	.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
	//	.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
	//	.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
		/*switch page for esd check*/
	//	.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
	//	.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.pll_clk = 415,
	//	.vfp_lp_dyn = 2528,
	//	.hfp = 76,
	//	.vfp = 82,
	},
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int boe_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{

	if (level > 255)
		level = 255;
	pr_info("%s backlight = %d\n", __func__, level);
	bl_tb0[1] = (u8)level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}

struct drm_display_mode *get_mode_by_id_hfp(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}
static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, mode);

	if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_to_60(struct drm_panel *panel)
{
	//struct boe *ctx = panel_to_boe(panel);
	// panel_to_boe(panel);

	pr_info("%s\n", __func__);

//	boe_dcs_write_seq_static(ctx, 0xFF, 0x25);
//	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
//	boe_dcs_write_seq_static(ctx, 0x18, 0x21);
//	boe_dcs_write_seq_static(ctx, 0xFF, 0x10);
//	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
}
static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	//struct drm_display_mode *m = get_mode_by_id_hfp(connector, dst_mode);

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

//	if (drm_mode_vrefresh(m) == 60) { /* 60 switch to 120 */
		mode_switch_to_60(panel);
//	} else if (drm_mode_vrefresh(m) == 90) { /* 1200 switch to 60 */
//		mode_switch_to_90(panel);
//	} else if (drm_mode_vrefresh(m) == 120) { /* 1200 switch to 60 */
//		mode_switch_to_120(panel);
//	else
//		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct boe *ctx = panel_to_boe(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = boe_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.ata_check = panel_ata_check,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int boe_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode_90hz);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_90hz.hdisplay, performance_mode_90hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_90hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &performance_mode_120hz);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_120hz.hdisplay, performance_mode_120hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_120hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs boe_drm_funcs = {
	.disable = boe_disable,
	.unprepare = boe_unprepare,
	.prepare = boe_prepare,
	.enable = boe_enable,
	.get_modes = boe_get_modes,
};

static int boe_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct boe *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+ boe,hx83102j,vdo,120hz\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct boe), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0)
		value = 0;
	else
		ctx->gate_ic = value;

	value = 0;
	ret = of_property_read_u32(dev->of_node, "rc-enable", &value);
	if (ret < 0)
		value = 0;
	else {
		ext_params.round_corner_en = value;
		ext_params_90hz.round_corner_en = value;
		ext_params_120hz.round_corner_en = value;
	}

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	if (ctx->gate_ic == 0) {
		ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_info(dev, "cannot get bias-gpios 0 %ld\n",
				 PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		devm_gpiod_put(dev, ctx->bias_pos);

		ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_info(dev, "cannot get bias-gpios 1 %ld\n",
				 PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		devm_gpiod_put(dev, ctx->bias_neg);
	}
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &boe_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	pr_info("%s- boe,hx83102j,vdo,120hz\n", __func__);

	return ret;
}

static int boe_remove(struct mipi_dsi_device *dsi)
{
	struct boe *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id boe_of_match[] = {
	{
	    .compatible = "boe,hx83102j,vdo,120hz",
	},
	{}
};

MODULE_DEVICE_TABLE(of, boe_of_match);

static struct mipi_dsi_driver boe_driver = {
	.probe = boe_probe,
	.remove = boe_remove,
	.driver = {
		.name = "panel-boe-hx83102j-vdo-120hz",
		.owner = THIS_MODULE,
		.of_match_table = boe_of_match,
	},
};

module_mipi_dsi_driver(boe_driver);

MODULE_AUTHOR("wulongchao <wulongchao@huaqin.com>");
MODULE_DESCRIPTION("BOE HX83102J VDO 120HZ LCD Panel Driver");
MODULE_LICENSE("GPL v2");

