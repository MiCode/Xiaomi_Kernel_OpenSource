/*
 * Copyright (c) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

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
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

/* BSP.lcm - 2020.11.19  */
extern int lcm_name[10];

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
/*zhangyundan - 2020.11.04 - Add for adjust the TP on screen timing*/
extern int32_t nvt_ts_tp_resume(void);
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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
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
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
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

static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(10 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0X01);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x03, 0x0A, 0x0A, 0x04, 0x04);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0X20);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0X01);

	lcm_dcs_write_seq_static(ctx, 0x0E, 0XAB);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0XA4);
	lcm_dcs_write_seq_static(ctx, 0x06, 0xAE);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x9E);
	lcm_dcs_write_seq_static(ctx, 0x95, 0xAF);
	lcm_dcs_write_seq_static(ctx, 0x96, 0xAF);
	lcm_dcs_write_seq_static(ctx, 0x88, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x0C);

	lcm_dcs_write_seq_static(ctx, 0x69, 0x99);
	lcm_dcs_write_seq_static(ctx, 0x6D, 0x44);
	lcm_dcs_write_seq_static(ctx, 0x62, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0x78, 0x01);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x23);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0x07, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x09, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0x00, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x02, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x12);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x09, 0x11);

	lcm_dcs_write_seq_static(ctx, 0x0A, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x0B, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x13, 0x05);


	lcm_dcs_write_seq_static(ctx, 0x14, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x10);

	lcm_dcs_write_seq_static(ctx, 0x1E, 0x12);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x23, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x24, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x27, 0x0E);

	lcm_dcs_write_seq_static(ctx, 0x28, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x2D, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0x31, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x32, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x34, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x49);

	lcm_dcs_write_seq_static(ctx, 0x37, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x38, 0x6C);
	lcm_dcs_write_seq_static(ctx, 0x39, 0x6C);
	lcm_dcs_write_seq_static(ctx, 0x3F, 0x6C);
	lcm_dcs_write_seq_static(ctx, 0x41, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x42, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x4C, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x27);
	lcm_dcs_write_seq_static(ctx, 0x72, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x73, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x74, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x75, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x7C, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x7A, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x1A);

	lcm_dcs_write_seq_static(ctx, 0x7D, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x80, 0x47);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x07);
	lcm_dcs_write_seq_static(ctx, 0x82, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x31);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x13);

	lcm_dcs_write_seq_static(ctx, 0x89, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0X31);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0x8F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x92, 0x76);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x02);

	lcm_dcs_write_seq_static(ctx, 0xB4, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xDC, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xDD, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xDE, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x4C);
	lcm_dcs_write_seq_static(ctx, 0xE0, 0x2A);
	lcm_dcs_write_seq_static(ctx, 0xEB, 0x03);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0x05, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x24, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x38, 0x30);
	lcm_dcs_write_seq_static(ctx, 0x4B, 0x21);
	lcm_dcs_write_seq_static(ctx, 0x4C, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x66, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x50);

	lcm_dcs_write_seq_static(ctx, 0x69, 0x60);
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x65);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x62);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0x25);
	lcm_dcs_write_seq_static(ctx, 0x7F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x34);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x04);

	lcm_dcs_write_seq_static(ctx, 0x8E, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xBF, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xD9, 0x68);
	lcm_dcs_write_seq_static(ctx, 0xDB, 0x32);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0x06, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x82);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0xC5);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0xAF);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0x48);
	lcm_dcs_write_seq_static(ctx, 0x98, 0xF1);
	lcm_dcs_write_seq_static(ctx, 0xA9, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xAA, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xAE, 0x8A);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x27);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0x13, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0x14);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x08);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0xA2, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x1D, 0x00, 0x35, 0x00, 0x5C, 0x00, 0x7C, 0x00, 0x97, 0x00, 0xAE, 0x00, 0xC3, 0x00, 0xD5);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x00, 0xE6, 0x01, 0x1E, 0x01, 0x46, 0x01, 0x86, 0x01, 0xB4, 0x01, 0xFD, 0x02, 0x34, 0x02, 0x35);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x6C, 0x02, 0xA9, 0x02, 0xD3, 0x03, 0x03, 0x03, 0x27, 0x03, 0x4E, 0x03, 0x5E, 0x03, 0x6C);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x7E, 0x03, 0x94, 0x03, 0xAF, 0x03, 0xC8, 0x03, 0xD2, 0x03, 0xD4, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x77, 0x00, 0x86, 0x00, 0xA0, 0x00, 0xB7, 0x00, 0xCB, 0x00, 0xDC, 0x00, 0xED, 0x00, 0xFB);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x01, 0x09, 0x01, 0x37, 0x01, 0x5A, 0x01, 0x94, 0x01, 0xBE, 0x02, 0x03, 0x02, 0x39, 0x02, 0x3A);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x70, 0x02, 0xAC, 0x02, 0xD6, 0x03, 0x06, 0x03, 0x2A, 0x03, 0x51, 0x03, 0x61, 0x03, 0x6F);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x80, 0x03, 0x96, 0x03, 0x9F, 0x03, 0xC7, 0x03, 0xD2, 0x03, 0xD4, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x28, 0x00, 0x57, 0x00, 0x7B, 0x00, 0x98, 0x00, 0xB1, 0x00, 0xC7, 0x00, 0xDA);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0xEB, 0x01, 0x24, 0x01, 0x4C, 0x01, 0x8C, 0x01, 0xB9, 0x02, 0x01, 0x02, 0x38, 0x02, 0x39);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x6F, 0x02, 0xAB, 0x02, 0xD5, 0x03, 0x06, 0x03, 0x2A, 0x03, 0x50, 0x03, 0x60, 0x03, 0x70);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x83, 0x03, 0x9B, 0x03, 0xB6, 0x03, 0xCA, 0x03, 0xD2, 0x03, 0xD4, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x15, 0x00, 0x2D, 0x00, 0x54, 0x00, 0x74, 0x00, 0x8F, 0x00, 0xA6, 0x00, 0xBB, 0x00, 0xCD);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x00, 0xDE, 0x01, 0x16, 0x01, 0x3E, 0x01, 0x7E, 0x01, 0xAC, 0x01, 0xF5, 0x02, 0x2C, 0x02, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x64, 0x02, 0xA1, 0x02, 0xCB, 0x02, 0xFB, 0x03, 0x1F, 0x03, 0x46, 0x03, 0x56, 0x03, 0x64);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x76, 0x03, 0x8C, 0x03, 0xA7, 0x03, 0xC0, 0x03, 0xD2, 0x03, 0xD4, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x6F, 0x00, 0x7E, 0x00, 0x98, 0x00, 0xAF, 0x00, 0xC3, 0x00, 0xD4, 0x00, 0xE5, 0x00, 0xF3);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x01, 0x01, 0x01, 0x2F, 0x01, 0x52, 0x01, 0x8C, 0x01, 0xB6, 0x01, 0xFB, 0x02, 0x31, 0x02, 0x32);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x68, 0x02, 0xA4, 0x02, 0xCE, 0x02, 0xFE, 0x03, 0x22, 0x03, 0x49, 0x03, 0x59, 0x03, 0x67);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x78, 0x03, 0x8E, 0x03, 0x97, 0x03, 0xBF, 0x03, 0xD2, 0x03, 0xD4, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x20, 0x00, 0x4F, 0x00, 0x73, 0x00, 0x90, 0x00, 0xA9, 0x00, 0xBF, 0x00, 0xD2);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0xE3, 0x01, 0x1C, 0x01, 0x44, 0x01, 0x84, 0x01, 0xB1, 0x01, 0xF9, 0x02, 0x30, 0x02, 0x31);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x67, 0x02, 0xA3, 0x02, 0xCD, 0x02, 0xFE, 0x03, 0x22, 0x03, 0x48, 0x03, 0x58, 0x03, 0x68);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x7B, 0x03, 0x93, 0x03, 0xAE, 0x03, 0xC2, 0x03, 0xD2, 0x03, 0xD4, 0x00, 0x00);

	/******gamma******/
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x51, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	msleep(20);
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
	lcm_dcs_write_seq_static(ctx, 0x4F, 0x01);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
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

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	pr_err("panel-nt35695-vdo prepare\n");
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
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
/*zhangyundan - 2020.11.04 - Add for adjust the TP on screen timing*/
	nvt_ts_tp_resume();
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

#define HFP (24)
#define HSA (8)
#define HBP (24)
#define VFP (10)
#define VSA (2)
#define VBP (8)
#define VAC (2340)
#define HAC (1080)
static u32 fake_heigh = 2340;
static u32 fake_width = 1080;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	.clock = 160857,			// htotal*vtotal*fresh/1000
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,	//1136
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,	//2360
	.vrefresh = 60,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
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
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x0, 0x81, 0x0};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 443,
	.vfp_low_power = 0,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x53,
		.count = 1,
		.para_list[0] = 0x24,
	},
	.wait_sof_before_dec_vfp = 1,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
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

static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vdisplay = fake_heigh;
		mode->vsync_start = fake_heigh + VFP;
		mode->vsync_end = fake_heigh + VFP + VSA;
		mode->vtotal = fake_heigh + VFP + VSA + VBP;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hdisplay = fake_width;
		mode->hsync_start = fake_width + HFP;
		mode->hsync_end = fake_width + HFP + HSA;
		mode->htotal = fake_width + HFP + HSA + HBP;
	}
}

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	if (need_fake_resolution)
		change_drm_disp_mode_params(&default_mode);
	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = 64;
	panel->connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	/* BSP.lcm - 2020.11.19 - start */
	pr_info("[LCM][KERNEL]nt36672A probe!!!\n");
	if (strstr(saved_command_line, "nt36672A_fhdp_dsi_vdo_tianma_j19_lcm_drv")) {
		pr_err("nt36672a probe match");
	} else {
		pr_err("not match nt36672a !!!\n");
		return -ENODEV;
	}
	/* BSP.lcm - 2020.11.19 - end */

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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
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
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

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

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

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
	check_is_need_fake_resolution(dev);
	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

/* BSP.lcm - 2020.11.19 - start */
static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "nt36672a,tianma0,vdo", },
	{ }
};
/* BSP.lcm - 2020.11.19 - end */

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-nt35695b-auo-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Yi-Lun Wang <Yi-Lun.Wang@mediatek.com>");
MODULE_DESCRIPTION("nt35695b VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");

