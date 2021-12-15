/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#include "include/panel-tianma-nt36672e-vdo-120hz-vfp.h"
#endif

#if defined(CONFIG_RT4831A_I2C)
#include "../../../misc/mediatek/gate_ic/gate_i2c.h"
#endif

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

struct tianma {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *pm_enable_gpio;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;

	int error;
};

static char bl_tb0[] = { 0x51, 0xff };
static int current_fps = 60;

#define tianma_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define tianma_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
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
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void tianma_panel_get_data(struct tianma *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = tianma_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
static struct LCD_setting_table lcm_suspend_setting[] = {
	{0xFF, 2, {0xFF, 0x10} },
	{0x28, 2, {0x28, 0x00} },

};

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
		pr_err("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_err("get dsv_neg fail, error: %d\n", ret);
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
		pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
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
		pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif


#define HFP_SUPPORT 1

static void tianma_panel_init(struct tianma *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(13 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x10);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x36, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x3B, 0x03, 0x23, 0x36, 0x04, 0x04);
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xC1, 0x89, 0x28,
		0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E, 0x00,
		0x2B, 0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7);
	tianma_dcs_write_seq_static(ctx, 0xC2, 0x1B, 0xA0);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0x01);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x20);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x01, 0x66);
	tianma_dcs_write_seq_static(ctx, 0x07, 0x28);
	tianma_dcs_write_seq_static(ctx, 0x17, 0x66);
	tianma_dcs_write_seq_static(ctx, 0x1B, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x2D, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x2F, 0x83);
	tianma_dcs_write_seq_static(ctx, 0x30, 0x1F);
	tianma_dcs_write_seq_static(ctx, 0x69, 0xD0);
	tianma_dcs_write_seq_static(ctx, 0x95, 0xA9);
	tianma_dcs_write_seq_static(ctx, 0x96, 0xA9);
	tianma_dcs_write_seq_static(ctx, 0xF2, 0x66);
	tianma_dcs_write_seq_static(ctx, 0xF3, 0x54);
	tianma_dcs_write_seq_static(ctx, 0xF4, 0x66);
	tianma_dcs_write_seq_static(ctx, 0xF5, 0x54);
	tianma_dcs_write_seq_static(ctx, 0xF6, 0x66);
	tianma_dcs_write_seq_static(ctx, 0xF7, 0x54);
	tianma_dcs_write_seq_static(ctx, 0xF8, 0x66);
	tianma_dcs_write_seq_static(ctx, 0xF9, 0x54);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x23);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x07, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x08, 0x07);
	tianma_dcs_write_seq_static(ctx, 0x09, 0x04);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x24);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x00, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x01, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x02, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x03, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x04, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x05, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x06, 0x13);
	tianma_dcs_write_seq_static(ctx, 0x07, 0x15);
	tianma_dcs_write_seq_static(ctx, 0x08, 0x17);
	tianma_dcs_write_seq_static(ctx, 0x09, 0x13);
	tianma_dcs_write_seq_static(ctx, 0x0A, 0x15);
	tianma_dcs_write_seq_static(ctx, 0x0B, 0x17);
	tianma_dcs_write_seq_static(ctx, 0x0C, 0x24);
	tianma_dcs_write_seq_static(ctx, 0x0D, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x0E, 0x2F);
	tianma_dcs_write_seq_static(ctx, 0x0F, 0x2D);
	tianma_dcs_write_seq_static(ctx, 0x10, 0x2E);
	tianma_dcs_write_seq_static(ctx, 0x11, 0x2C);
	tianma_dcs_write_seq_static(ctx, 0x12, 0x8B);
	tianma_dcs_write_seq_static(ctx, 0x13, 0x8C);
	tianma_dcs_write_seq_static(ctx, 0x14, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x15, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x16, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0x17, 0x22);
	tianma_dcs_write_seq_static(ctx, 0x18, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x19, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x1A, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x1B, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x1C, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x1D, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x1E, 0x13);
	tianma_dcs_write_seq_static(ctx, 0x1F, 0x15);
	tianma_dcs_write_seq_static(ctx, 0x20, 0x17);
	tianma_dcs_write_seq_static(ctx, 0x21, 0x13);
	tianma_dcs_write_seq_static(ctx, 0x22, 0x15);
	tianma_dcs_write_seq_static(ctx, 0x23, 0x17);
	tianma_dcs_write_seq_static(ctx, 0x24, 0x24);
	tianma_dcs_write_seq_static(ctx, 0x25, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x26, 0x2F);
	tianma_dcs_write_seq_static(ctx, 0x27, 0x2D);
	tianma_dcs_write_seq_static(ctx, 0x28, 0x2E);
	tianma_dcs_write_seq_static(ctx, 0x29, 0x2C);
	tianma_dcs_write_seq_static(ctx, 0x2A, 0x8B);
	tianma_dcs_write_seq_static(ctx, 0x2B, 0x8C);
	tianma_dcs_write_seq_static(ctx, 0x2D, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x2F, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x30, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0x31, 0x22);
	tianma_dcs_write_seq_static(ctx, 0x32, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x33, 0x03);
	tianma_dcs_write_seq_static(ctx, 0x35, 0x3C);
	tianma_dcs_write_seq_static(ctx, 0x36, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x4D, 0x04);
	tianma_dcs_write_seq_static(ctx, 0x4E, 0x38);
	tianma_dcs_write_seq_static(ctx, 0x4F, 0x38);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x38);
	tianma_dcs_write_seq_static(ctx, 0x79, 0x40);
	tianma_dcs_write_seq_static(ctx, 0x7A, 0x82);
	tianma_dcs_write_seq_static(ctx, 0x7B, 0x8F);
	tianma_dcs_write_seq_static(ctx, 0x82, 0x13);
	tianma_dcs_write_seq_static(ctx, 0x84, 0x31);
	tianma_dcs_write_seq_static(ctx, 0x85, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x86, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x87, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x90, 0x13);
	tianma_dcs_write_seq_static(ctx, 0x92, 0x31);
	tianma_dcs_write_seq_static(ctx, 0x93, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x94, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x95, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x9C, 0xF4);
	tianma_dcs_write_seq_static(ctx, 0x9D, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xA0, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0xA2, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0xA3, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xC4, 0x80);
	tianma_dcs_write_seq_static(ctx, 0xC6, 0xC0);
	tianma_dcs_write_seq_static(ctx, 0xC9, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xD9, 0x80);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0x02);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x25);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x19, 0xE4);
	tianma_dcs_write_seq_static(ctx, 0x21, 0xC0);
	tianma_dcs_write_seq_static(ctx, 0x66, 0x40);
	tianma_dcs_write_seq_static(ctx, 0x67, 0x29);
	tianma_dcs_write_seq_static(ctx, 0x68, 0x50);
	tianma_dcs_write_seq_static(ctx, 0x69, 0x60);
	tianma_dcs_write_seq_static(ctx, 0x6B, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x71, 0x6D);
	tianma_dcs_write_seq_static(ctx, 0x77, 0x60);
	tianma_dcs_write_seq_static(ctx, 0x79, 0x7E);
	tianma_dcs_write_seq_static(ctx, 0x7D, 0x40);
	tianma_dcs_write_seq_static(ctx, 0x7E, 0x2D);
	tianma_dcs_write_seq_static(ctx, 0xC0, 0x4D);
	tianma_dcs_write_seq_static(ctx, 0xC1, 0xA9);
	tianma_dcs_write_seq_static(ctx, 0xC2, 0xD2);
	tianma_dcs_write_seq_static(ctx, 0xC4, 0x11);
	tianma_dcs_write_seq_static(ctx, 0xD6, 0x80);
	tianma_dcs_write_seq_static(ctx, 0xD7, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xDA, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xDD, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xE0, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xF0, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xF1, 0x04);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x26);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x00, 0x10);
	tianma_dcs_write_seq_static(ctx, 0x01, 0xFB);
	tianma_dcs_write_seq_static(ctx, 0x03, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x04, 0xFB);
	tianma_dcs_write_seq_static(ctx, 0x05, 0x08);
	tianma_dcs_write_seq_static(ctx, 0x06, 0x10);
	tianma_dcs_write_seq_static(ctx, 0x08, 0x10);
	tianma_dcs_write_seq_static(ctx, 0x14, 0x06);
	tianma_dcs_write_seq_static(ctx, 0x15, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x74, 0xAF);
	tianma_dcs_write_seq_static(ctx, 0x81, 0x0E);
	tianma_dcs_write_seq_static(ctx, 0x83, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x85, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x87, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x88, 0x04);
	tianma_dcs_write_seq_static(ctx, 0x8A, 0x1A);
	tianma_dcs_write_seq_static(ctx, 0x8B, 0x11);
	tianma_dcs_write_seq_static(ctx, 0x8C, 0x24);
	tianma_dcs_write_seq_static(ctx, 0x8E, 0x42);
	tianma_dcs_write_seq_static(ctx, 0x8F, 0x11);
	tianma_dcs_write_seq_static(ctx, 0x90, 0x11);
	tianma_dcs_write_seq_static(ctx, 0x91, 0x11);
	tianma_dcs_write_seq_static(ctx, 0x9A, 0x80);
	tianma_dcs_write_seq_static(ctx, 0x9B, 0x04);
	tianma_dcs_write_seq_static(ctx, 0x9C, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x9D, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x9E, 0x00);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x27);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x01, 0x68);
	tianma_dcs_write_seq_static(ctx, 0x20, 0x81);
	tianma_dcs_write_seq_static(ctx, 0x21, 0x6A);
	tianma_dcs_write_seq_static(ctx, 0x25, 0x81);
	tianma_dcs_write_seq_static(ctx, 0x26, 0x94);
	tianma_dcs_write_seq_static(ctx, 0x6E, 0x23);
	tianma_dcs_write_seq_static(ctx, 0x6F, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x70, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x71, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x72, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x73, 0x21);
	tianma_dcs_write_seq_static(ctx, 0x74, 0x03);
	tianma_dcs_write_seq_static(ctx, 0x75, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x76, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x77, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x7D, 0x09);
	tianma_dcs_write_seq_static(ctx, 0x7E, 0x6B);
	tianma_dcs_write_seq_static(ctx, 0x80, 0x23);
	tianma_dcs_write_seq_static(ctx, 0x82, 0x09);
	tianma_dcs_write_seq_static(ctx, 0x83, 0x6B);
	tianma_dcs_write_seq_static(ctx, 0x88, 0x03);
	tianma_dcs_write_seq_static(ctx, 0x89, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xE3, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xE4, 0xE2);
	tianma_dcs_write_seq_static(ctx, 0xE5, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xE6, 0xD3);
	tianma_dcs_write_seq_static(ctx, 0xE9, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xEA, 0x1A);
	tianma_dcs_write_seq_static(ctx, 0xEB, 0x03);
	tianma_dcs_write_seq_static(ctx, 0xEC, 0x28);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x2A);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x00, 0x91);
	tianma_dcs_write_seq_static(ctx, 0x03, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x04, 0x4C);
	tianma_dcs_write_seq_static(ctx, 0x07, 0x4D);
	tianma_dcs_write_seq_static(ctx, 0x0A, 0x70);
	tianma_dcs_write_seq_static(ctx, 0x0C, 0x04);
	tianma_dcs_write_seq_static(ctx, 0x0D, 0x40);
	tianma_dcs_write_seq_static(ctx, 0x0F, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x11, 0xE1);
	tianma_dcs_write_seq_static(ctx, 0x15, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0x16, 0xAE);
	tianma_dcs_write_seq_static(ctx, 0x19, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0x1A, 0x82);
	tianma_dcs_write_seq_static(ctx, 0x1B, 0x23);
	tianma_dcs_write_seq_static(ctx, 0x1D, 0x36);
	tianma_dcs_write_seq_static(ctx, 0x1E, 0x3E);
	tianma_dcs_write_seq_static(ctx, 0x1F, 0x3E);
	tianma_dcs_write_seq_static(ctx, 0x20, 0x3E);
	tianma_dcs_write_seq_static(ctx, 0x28, 0xFD);
	tianma_dcs_write_seq_static(ctx, 0x29, 0x0B);
	tianma_dcs_write_seq_static(ctx, 0x2A, 0x6D);
	tianma_dcs_write_seq_static(ctx, 0x2D, 0x0B);
	tianma_dcs_write_seq_static(ctx, 0x2F, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x30, 0x85);
	tianma_dcs_write_seq_static(ctx, 0x33, 0x9A);
	tianma_dcs_write_seq_static(ctx, 0x34, 0xFF);
	tianma_dcs_write_seq_static(ctx, 0x35, 0x40);
	tianma_dcs_write_seq_static(ctx, 0x36, 0x12);
	tianma_dcs_write_seq_static(ctx, 0x37, 0xF9);
	tianma_dcs_write_seq_static(ctx, 0x38, 0x45);
	tianma_dcs_write_seq_static(ctx, 0x39, 0x0D);
	tianma_dcs_write_seq_static(ctx, 0x3A, 0x85);
	tianma_dcs_write_seq_static(ctx, 0x45, 0x06);
	tianma_dcs_write_seq_static(ctx, 0x46, 0x40);
	tianma_dcs_write_seq_static(ctx, 0x47, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x48, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x4A, 0x58);
	tianma_dcs_write_seq_static(ctx, 0x4E, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0x4F, 0xAE);
	tianma_dcs_write_seq_static(ctx, 0x52, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x82);
	tianma_dcs_write_seq_static(ctx, 0x54, 0x23);
	tianma_dcs_write_seq_static(ctx, 0x56, 0x36);
	tianma_dcs_write_seq_static(ctx, 0x57, 0x52);
	tianma_dcs_write_seq_static(ctx, 0x58, 0x52);
	tianma_dcs_write_seq_static(ctx, 0x59, 0x52);
	tianma_dcs_write_seq_static(ctx, 0x7A, 0x09);
	tianma_dcs_write_seq_static(ctx, 0x7B, 0x40);
	tianma_dcs_write_seq_static(ctx, 0x7F, 0xF0);
	tianma_dcs_write_seq_static(ctx, 0x83, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0x84, 0xAE);
	tianma_dcs_write_seq_static(ctx, 0x87, 0x0F);
	tianma_dcs_write_seq_static(ctx, 0x88, 0x82);
	tianma_dcs_write_seq_static(ctx, 0x89, 0x23);
	tianma_dcs_write_seq_static(ctx, 0x8B, 0x36);
	tianma_dcs_write_seq_static(ctx, 0x8C, 0x7D);
	tianma_dcs_write_seq_static(ctx, 0x8D, 0x7D);
	tianma_dcs_write_seq_static(ctx, 0x8E, 0x7D);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x2C);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x00, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x01, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x02, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x03, 0x14);
	tianma_dcs_write_seq_static(ctx, 0x04, 0x14);
	tianma_dcs_write_seq_static(ctx, 0x05, 0x14);
	tianma_dcs_write_seq_static(ctx, 0x0D, 0x50);
	tianma_dcs_write_seq_static(ctx, 0x0E, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x17, 0x48);
	tianma_dcs_write_seq_static(ctx, 0x18, 0x48);
	tianma_dcs_write_seq_static(ctx, 0x19, 0x48);
	tianma_dcs_write_seq_static(ctx, 0x2D, 0xAF);
	tianma_dcs_write_seq_static(ctx, 0x2F, 0x10);
	tianma_dcs_write_seq_static(ctx, 0x30, 0xFB);
	tianma_dcs_write_seq_static(ctx, 0x32, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x33, 0xFB);
	tianma_dcs_write_seq_static(ctx, 0x35, 0x15);
	tianma_dcs_write_seq_static(ctx, 0x37, 0x15);
	tianma_dcs_write_seq_static(ctx, 0x4D, 0x14);
	tianma_dcs_write_seq_static(ctx, 0x4E, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x4F, 0x06);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x54, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x55, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x61, 0x78);
	tianma_dcs_write_seq_static(ctx, 0x62, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x80, 0xAF);
	tianma_dcs_write_seq_static(ctx, 0x81, 0x10);
	tianma_dcs_write_seq_static(ctx, 0x82, 0xFB);
	tianma_dcs_write_seq_static(ctx, 0x84, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x85, 0xFB);
	tianma_dcs_write_seq_static(ctx, 0x87, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x89, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x9D, 0x1E);
	tianma_dcs_write_seq_static(ctx, 0x9E, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x9F, 0x13);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x5A, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x9F, 0x19);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0xE0);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x25, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x35, 0x82);
	tianma_dcs_write_seq_static(ctx, 0x4E, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x55, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x85, 0x32);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0xC0);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x9C, 0x11);
	tianma_dcs_write_seq_static(ctx, 0x9D, 0x11);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0xD0);
	tianma_dcs_write_seq_static(ctx, 0xFB, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x22);
	tianma_dcs_write_seq_static(ctx, 0x54, 0x02);
	pr_info("%s, fps:%d\n", __func__, current_fps);
	tianma_dcs_write_seq_static(ctx, 0XFF, 0X25);
	tianma_dcs_write_seq_static(ctx, 0XFB, 0X01);
	tianma_dcs_write_seq_static(ctx, 0X18, 0X20);
	tianma_dcs_write_seq_static(ctx, 0X79, 0X7F);

	tianma_dcs_write_seq_static(ctx, 0XFF, 0X2A);
	tianma_dcs_write_seq_static(ctx, 0XFB, 0X01);
	tianma_dcs_write_seq_static(ctx, 0X7A, 0X09);
	tianma_dcs_write_seq_static(ctx, 0X7C, 0X02);
	tianma_dcs_write_seq_static(ctx, 0X7F, 0XF0);
	tianma_dcs_write_seq_static(ctx, 0X8C, 0X7E);
	tianma_dcs_write_seq_static(ctx, 0X8D, 0X7E);
	tianma_dcs_write_seq_static(ctx, 0X8E, 0X7E);

	tianma_dcs_write_seq_static(ctx, 0XFF, 0X2C);
	tianma_dcs_write_seq_static(ctx, 0XFB, 0X01);
	tianma_dcs_write_seq_static(ctx, 0X87, 0X20);
	tianma_dcs_write_seq_static(ctx, 0X89, 0X20);
	tianma_dcs_write_seq_static(ctx, 0X9F, 0X0D);

	tianma_dcs_write_seq_static(ctx, 0xFF, 0x10);
	tianma_dcs_write_seq_static(ctx, 0x35, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x51, 0xFF);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x24);
	tianma_dcs_write_seq_static(ctx, 0x55, 0x00);

	tianma_dcs_write_seq_static(ctx, 0x11, 0x00);
	msleep(120);
	tianma_dcs_write_seq_static(ctx, 0x29, 0x00);
	tianma_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1]);
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
	pr_info("%s\n", __func__);

	tianma_dcs_write_seq_static(ctx, 0x28);
	tianma_dcs_write_seq_static(ctx, 0x10);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
#elif defined(CONFIG_RT4831A_I2C)
	/*this is rt4831a*/
	_gate_ic_i2c_panel_bias_enable(0);
	_gate_ic_Power_off();
#else
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);


	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	udelay(1000);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
#endif
	return 0;
}

static int tianma_prepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#elif defined(CONFIG_RT4831A_I2C)
	_gate_ic_Power_on();
	/*rt4831a co-work with leds_i2c*/
	_gate_ic_i2c_panel_bias_enable(1);
#else
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
#endif

	tianma_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		tianma_unprepare(panel);

	ctx->prepared = true;
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
	tianma_panel_get_data(ctx);
#endif

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
	.clock = 289452,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
	.vrefresh = MODE_0_FPS,
};

static const struct drm_display_mode performance_mode_1 = {
	.clock = 336420,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
	.vrefresh = MODE_1_FPS,


};

static const struct drm_display_mode performance_mode_2 = {
	.clock = 383239,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
	.vrefresh = MODE_2_FPS,

};

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
	.vfp_low_power = 4300,//45hz
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_0_DATA_RATE + 10,
		.hfp = 162,
		.vfp = 2672,
	},
	.data_rate = DATA_RATE,
	.lfr_enable = LFR_EN,
	.lfr_minimum_fps = MODE_0_FPS,
};


static struct mtk_panel_params ext_params_mode_1 = {
	.vfp_low_power = 2630,//60hz
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_1_DATA_RATE + 10,
		.hfp = 162,
		.vfp = 974,
	},
	.data_rate = DATA_RATE,
	.lfr_enable = LFR_EN,
	.lfr_minimum_fps = MODE_0_FPS,
};

static struct mtk_panel_params ext_params_mode_2 = {
	.vfp_low_power = 2630,//60hz
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_2_DATA_RATE + 10,
		.hfp = 162,
		.vfp = 122,
	},
	.data_rate = DATA_RATE,
	.lfr_enable = LFR_EN,
	.lfr_minimum_fps = MODE_0_FPS,
};

static int tianma_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	if (level > 255)
		level = 255;
	pr_info("%s backlight = -%d\n", __func__, level);
	bl_tb0[1] = (u8)level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}

struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);


	if (m->vrefresh == MODE_0_FPS)
		ext->params = &ext_params;
	else if (m->vrefresh == MODE_1_FPS)
		ext->params = &ext_params_mode_1;
	else if (m->vrefresh == MODE_2_FPS)
		ext->params = &ext_params_mode_2;
	else
		ret = 1;
	if (!ret)
		current_fps = m->vrefresh;
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

static int panel_ata_check(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x6e, 0x48, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_info("%s error\n", __func__);
		return 0;
	}

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}


static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = tianma_setbacklight_cmdq,
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
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
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int tianma_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode_1 = drm_mode_duplicate(panel->drm, &performance_mode_1);
	if (!mode_1) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_1.hdisplay,
			performance_mode_1.vdisplay,
			performance_mode_1.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode_1);


	mode_2 = drm_mode_duplicate(panel->drm, &performance_mode_2);
	if (!mode_2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_2.hdisplay,
			performance_mode_2.vdisplay,
			performance_mode_2.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode_2);

	panel->connector->display_info.width_mm = 64;
	panel->connector->display_info.height_mm = 129;

	return 3;
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
	struct tianma *ctx;
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
	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct tianma), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
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
		dev_info(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#elif !defined(CONFIG_RT4831A_I2C)
	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);
#endif

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &tianma_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

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

static int tianma_remove(struct mipi_dsi_device *dsi)
{
	struct tianma *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id tianma_of_match[] = {
	{
		.compatible = "tianma,nt36672e,vdo,120hz,vfp",
	},
	{} };

MODULE_DEVICE_TABLE(of, tianma_of_match);

static struct mipi_dsi_driver tianma_driver = {
	.probe = tianma_probe,
	.remove = tianma_remove,
	.driver = {
		.name = "panel-tianma-nt36672e-vdo-120hz-vfp",
		.owner = THIS_MODULE,
		.of_match_table = tianma_of_match,
	},
};

module_mipi_dsi_driver(tianma_driver);

MODULE_AUTHOR("Jingjing Liu <jingjing.liu@mediatek.com>");
MODULE_DESCRIPTION("tianma nt36672E vdo 120hz Panel Driver");
MODULE_LICENSE("GPL v2");

