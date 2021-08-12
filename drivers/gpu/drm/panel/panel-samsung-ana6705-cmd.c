// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define MODE_0_FPS (60)
#define MODE_1_FPS (72)
#define MODE_2_FPS (90)
#define MODE_3_FPS (120)

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
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

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;


static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		dev_info("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		dev_info("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 5400000, 5400000);
	if (ret < 0)
		dev_info("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
	if (ret < 0)
		dev_info("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		dev_info("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		dev_info("enable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		dev_info("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		dev_info("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(2 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		/* DSC Setting */
		/* Dsc 8bit input */
		lcm_dcs_write_seq_static(ctx,
				0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x90, 0x05,
				0xA0, 0x00, 0x43, 0x02, 0xD0, 0x02, 0xD0, 0x02, 0x00,
				0x02, 0xC2, 0x00, 0x20, 0x06, 0x89, 0x00, 0x0A, 0x00,
				0x0F, 0x01, 0xD2, 0x01, 0x24, 0x18, 0x00, 0x10, 0xF0,
				0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E,
				0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69, 0x70, 0x77,
				0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09,
				0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
				0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xB6, 0x2A,
				0xF4, 0x2A, 0xF4, 0x4B, 0x34, 0x63, 0x74, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00);

		/* Sleep Out(11h) */
		lcm_dcs_write_seq_static(ctx, 0x11);
		msleep(30);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
		msleep(80);

		/* Common Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x00, 0x01);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TE(Vsync) ON */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);

		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Pre-charge time setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2B, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x60, 0x63, 0x69);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* HLPM Power Saving */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x46, 0xF4);
		lcm_dcs_write_seq_static(ctx, 0xF4, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB1, 0x05);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Brightness Control */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0x63);
		/* Dimming Setting */
		lcm_dcs_write_seq_static(ctx, 0x63, 0x01);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0x63);
		lcm_dcs_write_seq_static(ctx, 0x63, 0x20);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* ACL Mode */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPEC Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x56, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0x1F, 0x01, 0x17, 0x01, 0x18, 0x06,
				0x88, 0x06, 0x89, 0x0A, 0xE2, 0x0A,
				0xE3, 0x11, 0xF5, 0x1B, 0xC0);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6A, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0x1F, 0x01, 0x17, 0x03, 0x44, 0x04,
				0x5A, 0x05, 0x71, 0x06, 0x88, 0x07, 0x9E,
				0x08, 0xB5, 0x09, 0xCB, 0x0A, 0xE2, 0x0B,
				0xF9, 0x0D, 0x0F, 0x0E, 0x26, 0x0F, 0x3C,
				0x10, 0x53);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x52, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0x1F, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x54, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0x1F, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x03);

		lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);
		lcm_dcs_write_seq_static(ctx, 0x29);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x4f, 0x01);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
#else
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);


	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	udelay(1000);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#else
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
#endif

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (40)
#define HSA (10)
#define HBP (20)
#define HACT (1440)
#define VFP (8)
#define VSA (8)
#define VBP (8)
#define VACT (3216)
#define PLL_CLOCK (718)

static const struct drm_display_mode default_mode = {
	.clock		= 584642, //120Hz
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP, //1510
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP, //3240
};

static const struct drm_display_mode performance_mode = {
	.clock		= 437870, //90Hz
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP, //1510
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP, //3240
};

static const struct drm_display_mode performance_mode1 = {
	.clock		= 349807, //72Hz
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP, //1510
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP, //3240
};

static const struct drm_display_mode performance_mode2 = {
	.clock		= 291098, //60Hz
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP, //1510
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP, //3240
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3];
	unsigned char id[3] = {0x00, 0x80, 0x00};
	ssize_t ret;

	pr_info("%s success\n", __func__);

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0)
		pr_info("%s error\n", __func__);

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x03, 0xFF};

	bl_tb0[1] = ((level & 0xF00) >> 8) & 0xF;
	bl_tb0[2] = level & 0xFF;

	DDPINFO("lcm backlight level is %u\n", level);
	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static struct mtk_panel_params ext_params = {
	.data_rate = PLL_CLOCK * 2,
	.pll_clk = PLL_CLOCK,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3216,
		.pic_width = 1440,
		.slice_height = 67,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 706,
		.scale_value = 32,
		.increment_interval = 1673,
		.decrement_interval = 10,
		.line_bpg_offset = 15,
		.nfl_bpg_offset = 466,
		.slice_bpg_offset = 292,
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

	.msync2_enable = 1,
	.msync_cmd_table = {
		.te_type = MULTI_TE,
		.msync_max_fps = 120,
		.msync_min_fps = 20,
		.msync_level_num = 20,
		.multi_te_tb = {
			/* Multi-TE level */
			.multi_te_level[0] = {
				.level_id = 0,
				.level_fps = 60,
				.max_fps = 60,
				.min_fps = 40,
			},
			.multi_te_level[1] = {
				.level_id = 1,
				.level_fps = 72,
				.max_fps = 72,
				.min_fps = 45,
			},
			.multi_te_level[2] = {
				.level_id = 2,
				.level_fps = 90,
				.max_fps = 90,
				.min_fps = 51,
			},
			.multi_te_level[3] = {
				.level_id = 3,
				.level_fps = 120,
				.max_fps = 120,
				.min_fps = 60,
			},
		},
	},

};

static struct mtk_panel_params ext_params_120hz = {
	.data_rate = PLL_CLOCK * 2,
	.pll_clk = PLL_CLOCK,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3216,
		.pic_width = 1440,
		.slice_height = 67,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 706,
		.scale_value = 32,
		.increment_interval = 1673,
		.decrement_interval = 10,
		.line_bpg_offset = 15,
		.nfl_bpg_offset = 466,
		.slice_bpg_offset = 292,
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

	.msync2_enable = 1,
	.msync_cmd_table = {
		.te_type = MULTI_TE,
		.msync_max_fps = 120,
		.msync_min_fps = 20,
		.msync_level_num = 20,
		.multi_te_tb = {
			/* Multi-TE level */
			.multi_te_level[0] = {
				.level_id = 0,
				.level_fps = 60,
				.max_fps = 60,
				.min_fps = 40,
			},
			.multi_te_level[1] = {
				.level_id = 1,
				.level_fps = 72,
				.max_fps = 72,
				.min_fps = 45,
			},
			.multi_te_level[2] = {
				.level_id = 2,
				.level_fps = 90,
				.max_fps = 90,
				.min_fps = 51,
			},
			.multi_te_level[3] = {
				.level_id = 3,
				.level_fps = 120,
				.max_fps = 120,
				.min_fps = 60,
			},
		},
	},

};

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
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
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (drm_mode_vrefresh(m) == MODE_0_FPS)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE_1_FPS)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE_2_FPS)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE_3_FPS)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static int mtk_panel_ext_param_get(struct mtk_panel_params *ext_para,
			 unsigned int mode)
{
	int ret = 0;

	if (mode == 0)
		ext_para = &ext_params;
	else if (mode == 1)
		ext_para = &ext_params_120hz;
	else if (mode == 2)
		ext_para = &ext_params_120hz;
	else if (mode == 3)
		ext_para = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
	} else if (stage == AFTER_DSI_POWERON) {
		DDPINFO("%s:%d switch to 60fps\n", __func__, __LINE__);
		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);
		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x05);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x05);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* min fps 40fps */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x06);
	}
}

static void mode_switch_to_72(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
	} else if (stage == AFTER_DSI_POWERON) {
		DDPINFO("%s:%d switch to 72fps\n", __func__, __LINE__);
		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);
		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x04);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x04);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x05);
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {

	} else if (stage == AFTER_DSI_POWERON) {
		DDPINFO("%s:%d switch to 90fps\n", __func__, __LINE__);
		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);
		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x01);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x04);
	}
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
	} else if (stage == AFTER_DSI_POWERON) {
		DDPINFO("%s:%d switch to 120fps\n", __func__, __LINE__);
		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);
		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x03);
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	if (cur_mode == dst_mode)
		return ret;

	if (drm_mode_vrefresh(m) == MODE_0_FPS) { /*switch to 60 */
		mode_switch_to_60(panel, stage);
		DDPINFO("%s:%d switch to 60fps\n", __func__, __LINE__);
	} else if (drm_mode_vrefresh(m) == MODE_1_FPS) { /*switch to 90 */
		mode_switch_to_72(panel, stage);
		DDPINFO("%s:%d switch to 72fps\n", __func__, __LINE__);
	} else if (drm_mode_vrefresh(m) == MODE_2_FPS) { /*switch to 90 */
		mode_switch_to_90(panel, stage);
		DDPINFO("%s:%d switch to 90fps\n", __func__, __LINE__);
	} else if (drm_mode_vrefresh(m) == MODE_3_FPS) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
		DDPINFO("%s:%d switch to 120fps\n", __func__, __LINE__);
	} else
		ret = 1;

	return ret;
}


static int msync_te_level_switch(struct drm_panel *panel, unsigned int fps_level)
{
	int ret = 0;
	struct lcm *ctx = panel_to_lcm(panel);


	DDPINFO("%s:%d fps_level:%d\n", __func__, __LINE__, fps_level);
	if (fps_level <= MODE_0_FPS) { /*switch to 60 */
		DDPINFO("%s:%d switch to 60fps\n", __func__, __LINE__);
		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);

		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x05);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x05);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* min fps 40fps */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x06);

	} else if (fps_level <= MODE_1_FPS) { /*switch to 72 */
		DDPINFO("%s:%d switch to 72fps\n", __func__, __LINE__);
		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);

		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x04);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x04);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x05);

	} else if (fps_level <= MODE_2_FPS) { /*switch to 90 */
		DDPINFO("%s:%d switch to 90fps\n", __func__, __LINE__);
		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);

		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x01);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x04);

	} else if (fps_level <= MODE_3_FPS) { /*switch to 120 */
		DDPINFO("%s:%d switch to 120fps\n", __func__, __LINE__);
		/* CASET/PASET Setting */
		lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x05, 0x9F);
		lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0C, 0x8F);

		/* Scaler Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC1 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x22, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0xA1, 0xB1);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3A, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x26, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* TSP_SYNC3 Fixed Setting */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x38, 0xB9);
		/* TSP_VSYNC Fixed TE 02:120 03:90 05:60 */
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2A, 0xB9);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* OPLUS ADFR On */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x02);
		lcm_dcs_write_seq_static(ctx, 0xFD, 0x4A);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xF2);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x1B, 0x50);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x78);
		lcm_dcs_write_seq_static(ctx, 0xF2, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x08);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0xAA);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3F, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x45, 0xF6);
		lcm_dcs_write_seq_static(ctx, 0xF6, 0x1F);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6C, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x80);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xBD);
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x45, 0x65, 0x65, 0x65);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0E, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x13, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0xFF, 0x00, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		/* oplus adfr -> max:min */
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x04, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x02, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x1C);
		/* Frame Compensation 82:off 02:on */
		lcm_dcs_write_seq_static(ctx, 0xBD, 0x21, 0x82);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0F);
		lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Open Multi-TE */
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xB9, 0x0A);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

		/* Min FPS */
		lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x03);

	} else
		ret = 1;

	return ret;
}

int msync_cmd_set_min_fps(struct drm_panel *panel, unsigned int flag)
{
	int ret = 0;
	struct lcm *ctx = panel_to_lcm(panel);
	unsigned int fps_level = (flag & 0xFFFF0000) >> 16;
	unsigned int min_fps = flag & 0xFFFF;

	DDPINFO("%s:%d flag:0x%08x, fps_level:%u min_fps:%u\n",
			__func__, __LINE__, flag, fps_level, min_fps);
	if (fps_level <= MODE_0_FPS) { /*switch to 60 */
		if (min_fps >= 60) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x03);
		} else if (min_fps >= 51) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x04);
		} else if (min_fps >= 45) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x05);
		} else if (min_fps >= 40) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x06);
		} else if (min_fps >= 36) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x07);
		} else if (min_fps >= 33) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x08);
		} else if (min_fps >= 30) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x09);
		} else if (min_fps >= 28) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x0A);
		} else if (min_fps >= 26) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x0B);
		} else if (min_fps >= 24) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x0C);
		} else if (min_fps >= 23) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x0C, 0x0D);
		}

	} else if (fps_level <= MODE_1_FPS) { /*switch to 72 */
		if (min_fps >= 72) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x03);
		} else if (min_fps >= 60) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x03);
		} else if (min_fps >= 51) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x04);
		} else if (min_fps >= 45) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x05);
		} else if (min_fps >= 40) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x06);
		} else if (min_fps >= 36) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x07);
		} else if (min_fps >= 33) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x08);
		} else if (min_fps >= 30) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x09);
		} else if (min_fps >= 28) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x0A);
		} else if (min_fps >= 26) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x0B);
		} else if (min_fps >= 24) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x08, 0x0C);
		}

	} else if (fps_level <= MODE_2_FPS) { /*switch to 90 */
		if (min_fps >= 90) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x01);
		} else if (min_fps >= 72) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x02);
		} else if (min_fps >= 60) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x03);
		} else if (min_fps >= 51) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x04);
		} else if (min_fps >= 45) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x05);
		} else if (min_fps >= 40) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x06);
		} else if (min_fps >= 36) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x07);
		} else if (min_fps >= 33) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x08);
		} else if (min_fps >= 30) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x09);
		} else if (min_fps >= 28) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x0A);
		} else if (min_fps >= 24) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x04, 0x0C);
		}

	} else if (fps_level <= MODE_3_FPS) { /*switch to 120 */
		if (min_fps >= 120) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
		} else if (min_fps >= 90) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x01);
		} else if (min_fps >= 72) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x02);
		} else if (min_fps >= 60) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x03);
		} else if (min_fps >= 51) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x04);
		} else if (min_fps >= 45) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x05);
		} else if (min_fps >= 40) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x06);
		} else if (min_fps >= 36) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x07);
		} else if (min_fps >= 33) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x08);
		} else if (min_fps >= 30) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x09);
		} else if (min_fps >= 24) {
			lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0x60);
			lcm_dcs_write_seq_static(ctx, 0x60, 0x00, 0x0C);
		}

	} else
		ret = 1;

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.mode_switch = mode_switch,
	.msync_te_level_switch = msync_te_level_switch,
	.msync_cmd_set_min_fps = msync_cmd_set_min_fps,
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

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel,
						struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;
	struct drm_display_mode *mode4;

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

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay,
			performance_mode.vdisplay,
			drm_mode_vrefresh(&performance_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &performance_mode1);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode1.hdisplay,
			performance_mode1.vdisplay,
			drm_mode_vrefresh(&performance_mode1));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

	mode4 = drm_mode_duplicate(connector->dev, &performance_mode2);
	if (!mode4) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode2.hdisplay,
			performance_mode2.vdisplay,
			drm_mode_vrefresh(&performance_mode2));
		return -ENOMEM;
	}

	drm_mode_set_name(mode4);
	mode4->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode4);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

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

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

#ifndef CONFIG_MTK_DISP_NO_LK
	ctx->prepared = true;
	ctx->enabled = true;
#endif

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

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
	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
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

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "samsung,ana6705,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "samsung_ana6705_cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("Samsung ANA6705 AMOLED CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
