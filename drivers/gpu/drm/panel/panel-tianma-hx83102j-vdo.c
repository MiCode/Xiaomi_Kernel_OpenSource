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
#define CHANGE_FPS_EN 0

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

static char bl_tb0[] = { 0x51, 0x0f, 0xff };
//TO DO: You have to do that remove macro BYPASSI2C and solve build error
//otherwise voltage will be unstable
#ifdef HIMAX_WAKE_UP
extern uint8_t wake_flag_drm;
#endif

struct tianma {
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

#define tianma_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define tianma_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct tianma *panel_to_tianma(struct drm_panel *panel)
{
	return container_of(panel, struct tianma, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int tianma_dcs_read(struct tianma *ctx, u8 cmd, void *data, size_t len)
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

static void tianma_panel_get_data(struct tianma *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = tianma_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void tianma_dcs_write(struct tianma *ctx, const void *data, size_t len)
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
}

static void tianma_panel_init(struct tianma *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5 * 1000, 10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(55 * 1000, 65 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	tianma_dcs_write_seq_static(ctx, 0x35, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x11, 0x00);
	msleep(120);
	tianma_dcs_write_seq_static(ctx, 0xB9, 0x83, 0x10, 0x21, 0x55, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xB1, 0x2C, 0xAB, 0xAB, 0x27, 0xE7, 0x11, 0x9B, 0x11,
	0x36, 0x36, 0x36, 0x36, 0x2A, 0x8B, 0x11, 0x65, 0x00, 0x88, 0xFA, 0xFF, 0xFF, 0x8F,
	0xFF, 0x08, 0xFF, 0x33);
	tianma_dcs_write_seq_static(ctx, 0xB2, 0x00, 0x47, 0xB0, 0xD0, 0x00, 0x12, 0xCE, 0x3C,
	0x36, 0x01, 0x02, 0x00, 0x00, 0x88, 0x15, 0x25, 0x87, 0x00, 0x00, 0x00, 0x13, 0x50, 0xA0);
	tianma_dcs_write_seq_static(ctx, 0xB4, 0x1C, 0x2B, 0x1C, 0x2B, 0x1C, 0x2B, 0x03, 0x43,
	0x03, 0x43, 0x01, 0x4B);
	tianma_dcs_write_seq_static(ctx, 0xBC, 0x1B, 0x04);
	tianma_dcs_write_seq_static(ctx, 0xBE, 0x20);
	tianma_dcs_write_seq_static(ctx, 0xBF, 0xFC, 0x84, 0x80, 0x9C, 0x36, 0x00, 0x0D, 0x04);
	tianma_dcs_write_seq_static(ctx, 0xC0, 0x34, 0x34, 0x22, 0x11, 0x22, 0xA0, 0x61, 0x08,
	0xF5, 0x03);
	tianma_dcs_write_seq_static(ctx, 0xC2, 0x43, 0xFF, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0xCC);
	tianma_dcs_write_seq_static(ctx, 0xC7, 0x80, 0x03, 0xB2, 0x04, 0x60, 0x10, 0x80, 0x20,
	0x40);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0xC6);
	tianma_dcs_write_seq_static(ctx, 0xC8, 0x97);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0xD0);
	tianma_dcs_write_seq_static(ctx, 0xC8, 0x1D);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	tianma_dcs_write_seq_static(ctx, 0xCB, 0x08, 0x13, 0x07, 0x00, 0x07, 0x03);
	tianma_dcs_write_seq_static(ctx, 0xCC, 0x02, 0x03, 0x4C);
	tianma_dcs_write_seq_static(ctx, 0xD1, 0x07, 0x06, 0x00, 0x02, 0x04, 0x0C, 0xFF);
	tianma_dcs_write_seq_static(ctx, 0xD3, 0x06, 0x00, 0x00, 0x00, 0xC0, 0x07, 0x08, 0x07,
	0x08, 0x03, 0x03, 0x22, 0x23, 0x11, 0x11, 0x03, 0x03, 0x32, 0x10, 0x0F, 0x00, 0x0F,
	0x32, 0x10, 0x10, 0x00, 0x10, 0x32, 0x10, 0x10, 0x00, 0x10, 0x00, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xD5, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x40, 0x40, 0x41, 0x41,
	0x18, 0x18, 0x00, 0x00, 0x02, 0x02, 0x01, 0x01, 0x03, 0x03, 0x41, 0x41, 0x41, 0x41,
	0x20, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18);
	tianma_dcs_write_seq_static(ctx, 0xD6, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x40, 0x40, 0x18, 0x18,
	0x41, 0x41, 0x01, 0x01, 0x03, 0x03, 0x00, 0x00, 0x02, 0x02, 0x41, 0x41, 0x41, 0x41,
	0x20, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18);
	tianma_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xBA, 0xAA, 0xAA, 0xA0, 0xAA, 0xAA,
	0xBA, 0xAA, 0xAA, 0xA0, 0xAA, 0xAA, 0xAE, 0xFF, 0xAA, 0xA0, 0xAA, 0xAA, 0xAE, 0xFF,
	0xAA, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xE0, 0x00, 0x04, 0x0D, 0x13, 0x19, 0x28, 0x44, 0x4C,
	0x54, 0x50, 0x6C, 0x72, 0x79, 0x8A, 0x8A, 0x93, 0x9D, 0xB0, 0xB0, 0x56, 0x5D, 0x66,
	0x6C, 0x00, 0x04, 0x0D, 0x13, 0x1B, 0x2F, 0x47, 0x4C, 0x54, 0x50, 0x6C, 0x72, 0x79,
	0x8A, 0x8A, 0x93, 0x9D, 0xB0, 0xB0, 0x56, 0x5D, 0x66, 0x6C);
	tianma_dcs_write_seq_static(ctx, 0xE1, 0x12, 0x00, 0x00, 0x89, 0x30, 0x80, 0x07, 0xD0,
	0x02, 0x58, 0x00, 0x14, 0x02, 0x58, 0x02, 0x58, 0x02, 0x00, 0x02, 0x2C, 0x00, 0x20,
	0x02, 0x02, 0x00, 0x08, 0x00, 0x0C, 0x05, 0x0E, 0x04, 0x94, 0x18, 0x00, 0x10, 0xF0,
	0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54,
	0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09);
	tianma_dcs_write_seq_static(ctx, 0xE7, 0x2D, 0x08, 0x08, 0x40, 0x44, 0x4C, 0x01, 0xD2,
	0x4B, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x12, 0x05, 0x02, 0x02, 0x08, 0x33, 0x03,
	0x84, 0x18, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xBD, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xE1, 0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19,
	0xF8, 0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6, 0x2B, 0x34, 0x2B, 0x74, 0x3B,
	0x74, 0x6B, 0x74);
	tianma_dcs_write_seq_static(ctx, 0xE2, 0x1C, 0x2B, 0x1C, 0x2B, 0x1C, 0x2B, 0x03, 0x43,
	0x03, 0x43, 0x04, 0x68, 0x01, 0x4B, 0x03, 0x05, 0x20, 0x05, 0x07, 0x00, 0x00, 0x0F,
	0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x2B, 0x0E, 0x0E, 0x3E, 0x44, 0x4D, 0x01, 0xF8,
	0x4F, 0x14, 0x14, 0x00, 0x00, 0xD0, 0x01, 0x34, 0x47, 0x36, 0x48, 0x00, 0x20, 0x4B,
	0x00, 0x36, 0x00, 0x07, 0x03, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xB1, 0x01, 0xB6, 0x11);
	tianma_dcs_write_seq_static(ctx, 0xCB, 0x86);
	tianma_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAE, 0x00, 0xF0, 0xA0, 0xAA, 0xAA,
	0xAE, 0x00, 0xF0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xE7, 0x02, 0x00, 0xC0, 0x01, 0x15, 0x07, 0x9A, 0x08,
	0xA0, 0x04, 0x03, 0x77, 0x00, 0x00, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xBD, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xE2, 0x1C, 0x2B, 0x1C, 0x2B, 0x1C, 0x2B, 0x03, 0x43,
	0x03, 0x43, 0x04, 0x68, 0x01, 0x4B, 0x03, 0x05, 0x20, 0x05, 0x07, 0x00, 0x00, 0x0F,
	0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x0A, 0x1C, 0x1C, 0x1D, 0x50, 0x48, 0x01, 0xEF,
	0x86, 0x14, 0x14, 0x00, 0x00, 0x34, 0x01, 0x34, 0x47, 0x44, 0x48, 0x00, 0x20, 0x4B,
	0x00, 0x36, 0x00, 0x07, 0x03, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xBF, 0xF2);
	tianma_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xEA, 0xAA, 0xFA, 0xA0, 0xAA, 0xAA,
	0xEA, 0xAA, 0xFA, 0xA0);
	tianma_dcs_write_seq_static(ctx, 0xE7, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x03, 0x03,
	0x03, 0x03, 0x00, 0x03, 0x81, 0x02, 0x40, 0x00, 0x20, 0x4B, 0x02, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xBD, 0x03);
	tianma_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xBF, 0xAA, 0xAA, 0xA0, 0xAA, 0xAA,
	0xBF, 0xAA, 0xAA, 0xA0, 0xAA, 0xAA, 0xEA, 0xAA, 0xFA, 0xA0, 0xAA, 0xAA, 0xEA, 0xAA,
	0xFA, 0xA0, 0xAA, 0xAA, 0xEF, 0xAA, 0xFA, 0xA0, 0xAA, 0xAA, 0xEF, 0xAA, 0xFA, 0xA0);
	tianma_dcs_write_seq_static(ctx, 0xE1, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xBD, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0xCE);
	tianma_dcs_write_seq_static(ctx, 0xBA, 0x01, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	tianma_dcs_write_seq_static(ctx, 0xBD, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xBA, 0x2D, 0x2D, 0x20, 0x80, 0x4F);
	tianma_dcs_write_seq_static(ctx, 0xBD, 0x03);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0xC5);
	tianma_dcs_write_seq_static(ctx, 0xBA, 0x4F, 0x48);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	tianma_dcs_write_seq_static(ctx, 0xBD, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x2C);
	tianma_dcs_write_seq_static(ctx, 0xC9, 0x00, 0x1E, 0x13, 0x88, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x5E, 0x00, 0x30);
	tianma_dcs_write_seq_static(ctx, 0xBE, 0x11, 0x96);
	tianma_dcs_write_seq_static(ctx, 0xD9, 0x00, 0x3F, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x29, 0x00);
	msleep(20);
	//tianma_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1]);

	pr_info("%s-\n", __func__);
}

static int tianma_disable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int tianma_unprepare(struct drm_panel *panel)
{

	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->prepared)
		return 0;

	tianma_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	tianma_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
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
#endif

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
		lcd_set_bl_bias_reg(ctx->dev, 0);
#else
		lcd_set_bl_bias_reg(ctx->dev, 0);
#endif
	ctx->error = 0;
	ctx->prepared = false;
	return 0;
}

static int tianma_prepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);
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
		lcd_set_bl_bias_reg(ctx->dev, 1);
#else
		lcd_set_bl_bias_reg(ctx->dev, 1);
#endif
//	_lcm_i2c_write_bytes(0x0, 0xf);
//	_lcm_i2c_write_bytes(0x1, 0xf);
	tianma_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0) {
		tianma_unprepare(panel);
		pr_info("%s11111-\n", __func__);
	}
	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	tianma_panel_get_data(ctx);
#endif

#ifdef VENDOR_EDIT
	// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
	lcd_queue_load_tp_fw();
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int tianma_enable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

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
	.hsync_start = 1200 + 52,//HFP
	.hsync_end = 1200 + 52 + 20,//HSA
	.htotal = 1200 + 52 + 20 + 40,//HBP
	.vdisplay = 2000,
	.vsync_start = 2000 + 208,//VFP
	.vsync_end = 2000 + 208 + 8,//VSA
	.vtotal = 2000 + 208 + 8 + 12,//VBP
};

#if CHANGE_FPS_EN
static const struct drm_display_mode performance_mode_30hz = {
	.clock = 370431,
	.hdisplay = 1200,
	.hsync_start = 1200 + 52,//HFP
	.hsync_end = 1200 + 52 + 20,//HSA
	.htotal = 1200 + 52 + 20 + 40,//HBP
	.vdisplay = 2000,
	.vsync_start = 2000 + 6892,//VFP
	.vsync_end = 2000 + 6892 + 8,//VSA
	.vtotal = 2000 + 6892 + 8 + 12,//VBP
};

static const struct drm_display_mode performance_mode_60hz = {
	.clock = 370506,
	.hdisplay = 1200,
	.hsync_start = 1200 + 52,//HFP
	.hsync_end = 1200 + 52 + 20,//HSA
	.htotal = 1200 + 52 + 20 + 40,//HBP
	.vdisplay = 2000,
	.vsync_start = 2000 + 2436,//VFP
	.vsync_end = 2000 + 2436 + 8,//VSA
	.vtotal = 2000 + 2436 + 8 + 12,//VBP
};
#endif

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.vfp_low_power = 208,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
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
		.rc_buf_thresh[0] = 14,
		.rc_buf_thresh[1] = 28,
		.rc_buf_thresh[2] = 42,
		.rc_buf_thresh[3] = 56,
		.rc_buf_thresh[4] = 70,
		.rc_buf_thresh[5] = 84,
		.rc_buf_thresh[6] = 98,
		.rc_buf_thresh[7] = 105,
		.rc_buf_thresh[8] = 112,
		.rc_buf_thresh[9] = 119,
		.rc_buf_thresh[10] = 121,
		.rc_buf_thresh[11] = 123,
		.rc_buf_thresh[12] = 125,
		.rc_buf_thresh[13] = 126,
		.rc_range_parameters[0].range_min_qp = 0,
		.rc_range_parameters[0].range_max_qp = 4,
		.rc_range_parameters[0].range_bpg_offset = 2,
		.rc_range_parameters[1].range_min_qp = 0,
		.rc_range_parameters[1].range_max_qp = 4,
		.rc_range_parameters[1].range_bpg_offset = 0,
		.rc_range_parameters[2].range_min_qp = 1,
		.rc_range_parameters[2].range_max_qp = 5,
		.rc_range_parameters[2].range_bpg_offset = 0,
		.rc_range_parameters[3].range_min_qp = 1,
		.rc_range_parameters[3].range_max_qp = 6,
		.rc_range_parameters[3].range_bpg_offset = -2,
		.rc_range_parameters[4].range_min_qp = 3,
		.rc_range_parameters[4].range_max_qp = 7,
		.rc_range_parameters[4].range_bpg_offset = -4,
		.rc_range_parameters[5].range_min_qp = 3,
		.rc_range_parameters[5].range_max_qp = 7,
		.rc_range_parameters[5].range_bpg_offset = -6,
		.rc_range_parameters[6].range_min_qp = 3,
		.rc_range_parameters[6].range_max_qp = 7,
		.rc_range_parameters[6].range_bpg_offset = -8,
		.rc_range_parameters[7].range_min_qp = 3,
		.rc_range_parameters[7].range_max_qp = 8,
		.rc_range_parameters[7].range_bpg_offset = -8,
		.rc_range_parameters[8].range_min_qp = 3,
		.rc_range_parameters[8].range_max_qp = 9,
		.rc_range_parameters[8].range_bpg_offset = -8,
		.rc_range_parameters[9].range_min_qp = 3,
		.rc_range_parameters[9].range_max_qp = 10,
		.rc_range_parameters[9].range_bpg_offset = -10,
		.rc_range_parameters[10].range_min_qp = 5,
		.rc_range_parameters[10].range_max_qp = 11,
		.rc_range_parameters[10].range_bpg_offset = -10,
		.rc_range_parameters[11].range_min_qp = 5,
		.rc_range_parameters[11].range_max_qp = 12,
		.rc_range_parameters[11].range_bpg_offset = -12,
		.rc_range_parameters[12].range_min_qp = 5,
		.rc_range_parameters[12].range_max_qp = 13,
		.rc_range_parameters[12].range_bpg_offset = -12,
		.rc_range_parameters[13].range_min_qp = 7,
		.rc_range_parameters[13].range_max_qp = 13,
		.rc_range_parameters[13].range_bpg_offset = -12,
		.rc_range_parameters[14].range_min_qp = 13,
		.rc_range_parameters[14].range_max_qp = 13,
		.rc_range_parameters[14].range_bpg_offset = -12
		},
	.data_rate = 943,
	.data_rate_khz = 943307,
	.lfr_enable = 0,
	.lfr_minimum_fps = 120,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xb9, 0x83} },
		.dfps_cmd_table[1] = {0, 2, {0xe2, 0x10} },
		.dfps_cmd_table[2] = {0, 2, {0xb9, 0x00} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.vfp = 208,
	},
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
	.rotate = 1,
	.phy_timcon = {
		.hs_trail = 0x0B,
		.clk_hs_post = 0x0F,
	},
};

#if CHANGE_FPS_EN
static struct mtk_panel_params ext_params_30hz = {
	.vfp_low_power = 6892,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
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
		.rc_buf_thresh[0] = 14,
		.rc_buf_thresh[1] = 28,
		.rc_buf_thresh[2] = 42,
		.rc_buf_thresh[3] = 56,
		.rc_buf_thresh[4] = 70,
		.rc_buf_thresh[5] = 84,
		.rc_buf_thresh[6] = 98,
		.rc_buf_thresh[7] = 105,
		.rc_buf_thresh[8] = 112,
		.rc_buf_thresh[9] = 119,
		.rc_buf_thresh[10] = 121,
		.rc_buf_thresh[11] = 123,
		.rc_buf_thresh[12] = 125,
		.rc_buf_thresh[13] = 126,
		.rc_range_parameters[0].range_min_qp = 0,
		.rc_range_parameters[0].range_max_qp = 4,
		.rc_range_parameters[0].range_bpg_offset = 2,
		.rc_range_parameters[1].range_min_qp = 0,
		.rc_range_parameters[1].range_max_qp = 4,
		.rc_range_parameters[1].range_bpg_offset = 0,
		.rc_range_parameters[2].range_min_qp = 1,
		.rc_range_parameters[2].range_max_qp = 5,
		.rc_range_parameters[2].range_bpg_offset = 0,
		.rc_range_parameters[3].range_min_qp = 1,
		.rc_range_parameters[3].range_max_qp = 6,
		.rc_range_parameters[3].range_bpg_offset = -2,
		.rc_range_parameters[4].range_min_qp = 3,
		.rc_range_parameters[4].range_max_qp = 7,
		.rc_range_parameters[4].range_bpg_offset = -4,
		.rc_range_parameters[5].range_min_qp = 3,
		.rc_range_parameters[5].range_max_qp = 7,
		.rc_range_parameters[5].range_bpg_offset = -6,
		.rc_range_parameters[6].range_min_qp = 3,
		.rc_range_parameters[6].range_max_qp = 7,
		.rc_range_parameters[6].range_bpg_offset = -8,
		.rc_range_parameters[7].range_min_qp = 3,
		.rc_range_parameters[7].range_max_qp = 8,
		.rc_range_parameters[7].range_bpg_offset = -8,
		.rc_range_parameters[8].range_min_qp = 3,
		.rc_range_parameters[8].range_max_qp = 9,
		.rc_range_parameters[8].range_bpg_offset = -8,
		.rc_range_parameters[9].range_min_qp = 3,
		.rc_range_parameters[9].range_max_qp = 10,
		.rc_range_parameters[9].range_bpg_offset = -10,
		.rc_range_parameters[10].range_min_qp = 5,
		.rc_range_parameters[10].range_max_qp = 11,
		.rc_range_parameters[10].range_bpg_offset = -10,
		.rc_range_parameters[11].range_min_qp = 5,
		.rc_range_parameters[11].range_max_qp = 12,
		.rc_range_parameters[11].range_bpg_offset = -12,
		.rc_range_parameters[12].range_min_qp = 5,
		.rc_range_parameters[12].range_max_qp = 13,
		.rc_range_parameters[12].range_bpg_offset = -12,
		.rc_range_parameters[13].range_min_qp = 7,
		.rc_range_parameters[13].range_max_qp = 13,
		.rc_range_parameters[13].range_bpg_offset = -12,
		.rc_range_parameters[14].range_min_qp = 13,
		.rc_range_parameters[14].range_max_qp = 13,
		.rc_range_parameters[14].range_bpg_offset = -12
		},
	.data_rate = 943,
	.data_rate_khz = 943307,
	.lfr_enable = 0,
	.lfr_minimum_fps = 30,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 30,
		.dfps_cmd_table[0] = {0, 2, {0xb9, 0x83} },
		.dfps_cmd_table[1] = {0, 2, {0xe2, 0x10} },
		.dfps_cmd_table[2] = {0, 2, {0xb9, 0x00} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.vfp = 6892,
	},
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
	.rotate = 1,
	.phy_timcon = {
		.hs_trail = 0x0B,
		.clk_hs_post = 0x0F,
	},
};

static struct mtk_panel_params ext_params_60hz = {
	.vfp_low_power = 2528,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
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

		.rc_buf_thresh[0] = 14,
		.rc_buf_thresh[1] = 28,
		.rc_buf_thresh[2] = 42,
		.rc_buf_thresh[3] = 56,
		.rc_buf_thresh[4] = 70,
		.rc_buf_thresh[5] = 84,
		.rc_buf_thresh[6] = 98,
		.rc_buf_thresh[7] = 105,
		.rc_buf_thresh[8] = 112,
		.rc_buf_thresh[9] = 119,
		.rc_buf_thresh[10] = 121,
		.rc_buf_thresh[11] = 123,
		.rc_buf_thresh[12] = 125,
		.rc_buf_thresh[13] = 126,
		.rc_range_parameters[0].range_min_qp = 0,
		.rc_range_parameters[0].range_max_qp = 4,
		.rc_range_parameters[0].range_bpg_offset = 2,
		.rc_range_parameters[1].range_min_qp = 0,
		.rc_range_parameters[1].range_max_qp = 4,
		.rc_range_parameters[1].range_bpg_offset = 0,
		.rc_range_parameters[2].range_min_qp = 1,
		.rc_range_parameters[2].range_max_qp = 5,
		.rc_range_parameters[2].range_bpg_offset = 0,
		.rc_range_parameters[3].range_min_qp = 1,
		.rc_range_parameters[3].range_max_qp = 6,
		.rc_range_parameters[3].range_bpg_offset = -2,
		.rc_range_parameters[4].range_min_qp = 3,
		.rc_range_parameters[4].range_max_qp = 7,
		.rc_range_parameters[4].range_bpg_offset = -4,
		.rc_range_parameters[5].range_min_qp = 3,
		.rc_range_parameters[5].range_max_qp = 7,
		.rc_range_parameters[5].range_bpg_offset = -6,
		.rc_range_parameters[6].range_min_qp = 3,
		.rc_range_parameters[6].range_max_qp = 7,
		.rc_range_parameters[6].range_bpg_offset = -8,
		.rc_range_parameters[7].range_min_qp = 3,
		.rc_range_parameters[7].range_max_qp = 8,
		.rc_range_parameters[7].range_bpg_offset = -8,
		.rc_range_parameters[8].range_min_qp = 3,
		.rc_range_parameters[8].range_max_qp = 9,
		.rc_range_parameters[8].range_bpg_offset = -8,
		.rc_range_parameters[9].range_min_qp = 3,
		.rc_range_parameters[9].range_max_qp = 10,
		.rc_range_parameters[9].range_bpg_offset = -10,
		.rc_range_parameters[10].range_min_qp = 5,
		.rc_range_parameters[10].range_max_qp = 11,
		.rc_range_parameters[10].range_bpg_offset = -10,
		.rc_range_parameters[11].range_min_qp = 5,
		.rc_range_parameters[11].range_max_qp = 12,
		.rc_range_parameters[11].range_bpg_offset = -12,
		.rc_range_parameters[12].range_min_qp = 5,
		.rc_range_parameters[12].range_max_qp = 13,
		.rc_range_parameters[12].range_bpg_offset = -12,
		.rc_range_parameters[13].range_min_qp = 7,
		.rc_range_parameters[13].range_max_qp = 13,
		.rc_range_parameters[13].range_bpg_offset = -12,
		.rc_range_parameters[14].range_min_qp = 13,
		.rc_range_parameters[14].range_max_qp = 13,
		.rc_range_parameters[14].range_bpg_offset = -12,
		},
	.data_rate = 943,
	.data_rate_khz = 943307,
	.lfr_enable = 0,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		.dfps_cmd_table[0] = {0, 2, {0xb9, 0x83} },
		.dfps_cmd_table[1] = {0, 2, {0xe2, 0x10} },
		.dfps_cmd_table[2] = {0, 2, {0xb9, 0x00} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 1,
		.vfp = 2436,
	},
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
	.rotate = 1,
	.phy_timcon = {
		.hs_trail = 0x0B,
		.clk_hs_post = 0x0F,
	},
};
#endif

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int tianma_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	unsigned int bl_level = level  << 1;

	if (bl_level > 4095)
		bl_level = 4095;
	bl_tb0[1] = bl_level >> 8;
	bl_tb0[2] = bl_level & 0xFF;


	pr_info("%s level = %d,backlight = %d,bl_tb0[1] = 0x%x,bl_tb0[2] = 0x%x\n",
		__func__, level, bl_level, bl_tb0[1], bl_tb0[2]);

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

	pr_info("drm_mode_vrefresh(m) =%d", drm_mode_vrefresh(m));
	if (ext && m && drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params;
#if CHANGE_FPS_EN
	else if (ext && m && drm_mode_vrefresh(m) == 30)
		ext->params = &ext_params_30hz;
	else if (ext && m && drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params_60hz;
#endif
	else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct tianma *ctx = panel_to_tianma(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = tianma_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	//.mode_switch = mode_switch,
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

static int tianma_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
#if CHANGE_FPS_EN
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;
#endif

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
#if CHANGE_FPS_EN
	mode2 = drm_mode_duplicate(connector->dev, &performance_mode_30hz);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_30hz.hdisplay, performance_mode_30hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_30hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &performance_mode_60hz);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_60hz.hdisplay, performance_mode_60hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_60hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);
#endif
	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs tianma_drm_funcs = {
	.disable = tianma_disable,
	.unprepare = tianma_unprepare,
	.prepare = tianma_prepare,
	.enable = tianma_enable,
	.get_modes = tianma_get_modes,
};

static int tianma_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct tianma *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+ tianma,hx83102j,vdo,120hz\n", __func__);

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

	ctx = devm_kzalloc(dev, sizeof(struct tianma), GFP_KERNEL);
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
#if CHANGE_FPS_EN
		ext_params_30hz.round_corner_en = value;
		ext_params_60hz.round_corner_en = value;
#endif
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
	drm_panel_init(&ctx->panel, dev, &tianma_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	pr_info(" %d  %s\n", __LINE__, __func__);
	if (ret < 0)
		return ret;

#endif

	pr_info("%s- tianma,hx83102j,vdo,120hz\n", __func__);

	return ret;
}

static int tianma_remove(struct mipi_dsi_device *dsi)
{
	struct tianma *ctx = mipi_dsi_get_drvdata(dsi);
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

static const struct of_device_id tianma_of_match[] = {
	{
	    .compatible = "tianma,hx83102j,vdo,120hz",
	},
	{}
};

MODULE_DEVICE_TABLE(of, tianma_of_match);

static struct mipi_dsi_driver tianma_driver = {
	.probe = tianma_probe,
	.remove = tianma_remove,
	.driver = {
		.name = "panel-tianma-hx83102j-vdo-120hz",
		.owner = THIS_MODULE,
		.of_match_table = tianma_of_match,
	},
};

module_mipi_dsi_driver(tianma_driver);

MODULE_AUTHOR("wulongchao <wulongchao@huaqin.com>");
MODULE_DESCRIPTION("TIANMA HX83102J VDO 120HZ LCD Panel Driver");
MODULE_LICENSE("GPL v2");

