/*
 * Copyright (c) 2015 MediaTek Inc.
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

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#if defined(CONFIG_RT4831A_I2C)
#include "../../../misc/mediatek/gate_ic/gate_i2c.h"
#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *pm_enable_gpio;
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
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(10 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0XFF, 0X10);
	lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	lcm_dcs_write_seq_static(ctx, 0XBB, 0X13);

	lcm_dcs_write_seq_static(ctx, 0XFF, 0X20);
	lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X0E, 0XB0);
	lcm_dcs_write_seq_static(ctx, 0X0F, 0XAE);
	lcm_dcs_write_seq_static(ctx, 0X62, 0XAA);
	lcm_dcs_write_seq_static(ctx, 0X6D, 0X44);
	lcm_dcs_write_seq_static(ctx, 0X78, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X95, 0X87);
	lcm_dcs_write_seq_static(ctx, 0X96, 0X87);
	lcm_dcs_write_seq_static(ctx, 0X89, 0X17);
	lcm_dcs_write_seq_static(ctx, 0X8A, 0X17);
	lcm_dcs_write_seq_static(ctx, 0X8B, 0X17);
	lcm_dcs_write_seq_static(ctx, 0X8C, 0X17);
	lcm_dcs_write_seq_static(ctx, 0X8D, 0X17);
	lcm_dcs_write_seq_static(ctx, 0X8E, 0X17);

	lcm_dcs_write_seq_static(ctx, 0XFF, 0X24);
	lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X00, 0X1C);
	lcm_dcs_write_seq_static(ctx, 0X01, 0X1C);
	lcm_dcs_write_seq_static(ctx, 0X02, 0X1C);
	lcm_dcs_write_seq_static(ctx, 0X03, 0X1C);
	lcm_dcs_write_seq_static(ctx, 0X04, 0X20);
	lcm_dcs_write_seq_static(ctx, 0X05, 0X0D);
	lcm_dcs_write_seq_static(ctx, 0X06, 0X09);
	lcm_dcs_write_seq_static(ctx, 0X07, 0X0A);
	lcm_dcs_write_seq_static(ctx, 0X08, 0X1E);
	lcm_dcs_write_seq_static(ctx, 0X09, 0X0D);
	lcm_dcs_write_seq_static(ctx, 0X0A, 0X0D);
	lcm_dcs_write_seq_static(ctx, 0X0B, 0X25);
	lcm_dcs_write_seq_static(ctx, 0X0C, 0X24);
	lcm_dcs_write_seq_static(ctx, 0X0D, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X0E, 0X04);
	lcm_dcs_write_seq_static(ctx, 0X0F, 0X04);
	lcm_dcs_write_seq_static(ctx, 0X10, 0X03);
	lcm_dcs_write_seq_static(ctx, 0X11, 0X03);
	lcm_dcs_write_seq_static(ctx, 0X12, 0X14);
	lcm_dcs_write_seq_static(ctx, 0X13, 0X14);
	lcm_dcs_write_seq_static(ctx, 0X14, 0X12);
	lcm_dcs_write_seq_static(ctx, 0X15, 0X12);
	lcm_dcs_write_seq_static(ctx, 0X16, 0X10);
	lcm_dcs_write_seq_static(ctx, 0X17, 0X1C);
	lcm_dcs_write_seq_static(ctx, 0X18, 0X1C);
	lcm_dcs_write_seq_static(ctx, 0X19, 0X1C);
	lcm_dcs_write_seq_static(ctx, 0X1A, 0X1C);
	lcm_dcs_write_seq_static(ctx, 0X1B, 0X20);
	lcm_dcs_write_seq_static(ctx, 0X1C, 0X0D);
	lcm_dcs_write_seq_static(ctx, 0X1D, 0X09);
	lcm_dcs_write_seq_static(ctx, 0X1E, 0X0A);
	lcm_dcs_write_seq_static(ctx, 0X1F, 0X1E);
	lcm_dcs_write_seq_static(ctx, 0X20, 0X0D);
	lcm_dcs_write_seq_static(ctx, 0X21, 0X0D);
	lcm_dcs_write_seq_static(ctx, 0X22, 0X25);
	lcm_dcs_write_seq_static(ctx, 0X23, 0X24);
	lcm_dcs_write_seq_static(ctx, 0X24, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X25, 0X04);
	lcm_dcs_write_seq_static(ctx, 0X26, 0X04);
	lcm_dcs_write_seq_static(ctx, 0X27, 0X03);
	lcm_dcs_write_seq_static(ctx, 0X28, 0X03);
	lcm_dcs_write_seq_static(ctx, 0X29, 0X14);
	lcm_dcs_write_seq_static(ctx, 0X2A, 0X14);
	lcm_dcs_write_seq_static(ctx, 0X2B, 0X12);
	lcm_dcs_write_seq_static(ctx, 0X2D, 0X12);
	lcm_dcs_write_seq_static(ctx, 0X2F, 0X10);
	lcm_dcs_write_seq_static(ctx, 0X31, 0X02);
	lcm_dcs_write_seq_static(ctx, 0X32, 0X03);
	lcm_dcs_write_seq_static(ctx, 0X33, 0X04);
	lcm_dcs_write_seq_static(ctx, 0X34, 0X02);
	lcm_dcs_write_seq_static(ctx, 0X37, 0X09);
	lcm_dcs_write_seq_static(ctx, 0X38, 0X88);
	lcm_dcs_write_seq_static(ctx, 0X39, 0X88);
	lcm_dcs_write_seq_static(ctx, 0X3F, 0X88);
	lcm_dcs_write_seq_static(ctx, 0X41, 0X02);
	lcm_dcs_write_seq_static(ctx, 0X42, 0X03);
	lcm_dcs_write_seq_static(ctx, 0X4C, 0X10);
	lcm_dcs_write_seq_static(ctx, 0X4D, 0X10);
	lcm_dcs_write_seq_static(ctx, 0X61, 0XE8);
	lcm_dcs_write_seq_static(ctx, 0X72, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X73, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X74, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X75, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X79, 0X23);
	lcm_dcs_write_seq_static(ctx, 0X7A, 0X0B);
	lcm_dcs_write_seq_static(ctx, 0X7B, 0X99);
	lcm_dcs_write_seq_static(ctx, 0X7C, 0X80);
	lcm_dcs_write_seq_static(ctx, 0X7D, 0X09);
	lcm_dcs_write_seq_static(ctx, 0X80, 0X42);
	lcm_dcs_write_seq_static(ctx, 0X82, 0X13);
	lcm_dcs_write_seq_static(ctx, 0X83, 0X22);
	lcm_dcs_write_seq_static(ctx, 0X84, 0X31);
	lcm_dcs_write_seq_static(ctx, 0X85, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X86, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X87, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X88, 0X13);
	lcm_dcs_write_seq_static(ctx, 0X89, 0X22);
	lcm_dcs_write_seq_static(ctx, 0X8A, 0X31);
	lcm_dcs_write_seq_static(ctx, 0X8B, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X8C, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X8D, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X8E, 0XF4);
	lcm_dcs_write_seq_static(ctx, 0X8F, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X90, 0X80);
	lcm_dcs_write_seq_static(ctx, 0X92, 0X8D);
	lcm_dcs_write_seq_static(ctx, 0X93, 0X70);
	lcm_dcs_write_seq_static(ctx, 0X94, 0X0C);
	lcm_dcs_write_seq_static(ctx, 0X99, 0X30);
	lcm_dcs_write_seq_static(ctx, 0XA0, 0X30);

	lcm_dcs_write_seq_static(ctx, 0XB3, 0X02);
	lcm_dcs_write_seq_static(ctx, 0XB4, 0X00);
	lcm_dcs_write_seq_static(ctx, 0XDC, 0X64);
	lcm_dcs_write_seq_static(ctx, 0XDD, 0X03);
	lcm_dcs_write_seq_static(ctx, 0XDF, 0X46);
	lcm_dcs_write_seq_static(ctx, 0XE0, 0X46);
	lcm_dcs_write_seq_static(ctx, 0XE1, 0X22);
	lcm_dcs_write_seq_static(ctx, 0XE2, 0X24);
	lcm_dcs_write_seq_static(ctx, 0XE3, 0X09);
	lcm_dcs_write_seq_static(ctx, 0XE4, 0X09);
	lcm_dcs_write_seq_static(ctx, 0XEB, 0X0B);

	lcm_dcs_write_seq_static(ctx, 0XFF, 0X25);
	lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X21, 0X19);
	lcm_dcs_write_seq_static(ctx, 0X22, 0X19);
	lcm_dcs_write_seq_static(ctx, 0X24, 0X8D);
	lcm_dcs_write_seq_static(ctx, 0X25, 0X8D);
	lcm_dcs_write_seq_static(ctx, 0X30, 0X35);
	lcm_dcs_write_seq_static(ctx, 0X38, 0X35);
	lcm_dcs_write_seq_static(ctx, 0X3F, 0X20);
	lcm_dcs_write_seq_static(ctx, 0X40, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X4B, 0X20);
	lcm_dcs_write_seq_static(ctx, 0X4C, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X58, 0X22);
	lcm_dcs_write_seq_static(ctx, 0X59, 0X04);
	lcm_dcs_write_seq_static(ctx, 0X5A, 0X09);
	lcm_dcs_write_seq_static(ctx, 0X5B, 0X09);
	lcm_dcs_write_seq_static(ctx, 0X5C, 0X25);
	lcm_dcs_write_seq_static(ctx, 0X5E, 0XFF);
	lcm_dcs_write_seq_static(ctx, 0X5F, 0X28);
	lcm_dcs_write_seq_static(ctx, 0X66, 0XD8);
	lcm_dcs_write_seq_static(ctx, 0X67, 0X2B);
	lcm_dcs_write_seq_static(ctx, 0X68, 0X58);
	lcm_dcs_write_seq_static(ctx, 0X6B, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X6C, 0X6D);
	lcm_dcs_write_seq_static(ctx, 0X77, 0X72);
	lcm_dcs_write_seq_static(ctx, 0XBF, 0X00);
	lcm_dcs_write_seq_static(ctx, 0XC3, 0X01);

	lcm_dcs_write_seq_static(ctx, 0XFF, 0X26);
	lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X00, 0XA0);
	lcm_dcs_write_seq_static(ctx, 0X06, 0XFF);
	lcm_dcs_write_seq_static(ctx, 0X12, 0X72);
	lcm_dcs_write_seq_static(ctx, 0X19, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X1A, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X1C, 0XAF);
	lcm_dcs_write_seq_static(ctx, 0X1D, 0XFF);
	lcm_dcs_write_seq_static(ctx, 0X1E, 0X83);
	lcm_dcs_write_seq_static(ctx, 0X98, 0XF1);
	lcm_dcs_write_seq_static(ctx, 0XA9, 0X11);
	lcm_dcs_write_seq_static(ctx, 0XAA, 0X11);
	lcm_dcs_write_seq_static(ctx, 0XAE, 0X8A);

	lcm_dcs_write_seq_static(ctx, 0XFF, 0X27);
	lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	lcm_dcs_write_seq_static(ctx, 0X13, 0X00);
	lcm_dcs_write_seq_static(ctx, 0X1E, 0X14);
	lcm_dcs_write_seq_static(ctx, 0X22, 0X00);

	lcm_dcs_write_seq_static(ctx, 0XFF, 0XF0);
	lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	lcm_dcs_write_seq_static(ctx, 0XA2, 0X00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x08, 0x00, 0x09, 0x00, 0x2A,
		0x00, 0x3C, 0x00, 0x4D, 0x00, 0x5C, 0x00, 0x6A, 0x00, 0x78);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x00, 0x85, 0x00, 0xB5, 0x00, 0xDC,
		0x01, 0x1E, 0x01, 0x4F, 0x01, 0x9F, 0x01, 0xDE, 0x01, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x25, 0x02, 0x6F, 0x02, 0xA5,
		0x02, 0xE0, 0x03, 0x0A, 0x03, 0x39, 0x03, 0x49, 0x03, 0x58);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x6A, 0x03, 0x7E, 0x03, 0x95,
		0x03, 0xB3, 0x03, 0xD0, 0x03, 0xD4, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x08, 0x00, 0x09, 0x00, 0x2A,
		0x00, 0x3C, 0x00, 0x4D, 0x00, 0x5C, 0x00, 0x6A, 0x00, 0x78);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x00, 0x85, 0x00, 0xB5, 0x00, 0xDC,
		0x01, 0x1E, 0x01, 0x4F, 0x01, 0x9F, 0x01, 0xDE, 0x01, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x25, 0x02, 0x6F, 0x02, 0xA5,
		0x02, 0xE0, 0x03, 0x0A, 0x03, 0x39, 0x03, 0x49, 0x03, 0x58);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x6A, 0x03, 0x7E, 0x03, 0x95,
		0x03, 0xB3, 0x03, 0xD0, 0x03, 0xD4, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x08, 0x00, 0x09, 0x00, 0x2A,
		0x00, 0x3C, 0x00, 0x4D, 0x00, 0x5C, 0x00, 0x6A, 0x00, 0x78);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x85, 0x00, 0xB5, 0x00, 0xDC,
		0x01, 0x1E, 0x01, 0x4F, 0x01, 0x9F, 0x01, 0xDE, 0x01, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x25, 0x02, 0x6F, 0x02, 0xA5,
		0x02, 0xE0, 0x03, 0x0A, 0x03, 0x39, 0x03, 0x49, 0x03, 0x58);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x6A, 0x03, 0x7E, 0x03, 0x95,
		0x03, 0xB3, 0x03, 0xD0, 0x03, 0xD4, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);

	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x22,
		0x00, 0x34, 0x00, 0x45, 0x00, 0x54, 0x00, 0x62, 0x00, 0x70);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x00, 0x7D, 0x00, 0xAD, 0x00, 0xD4,
		0x01, 0x16, 0x01, 0x47, 0x01, 0x97, 0x01, 0xD6, 0x01, 0xD9);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x1D, 0x02, 0x67, 0x02, 0x9D,
		0x02, 0xD8, 0x03, 0x02, 0x03, 0x31, 0x03, 0x41, 0x03, 0x50);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x62, 0x03, 0x76, 0x03, 0x8D,
		0x03, 0xAB, 0x03, 0xC8, 0x03, 0xCC, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x00, 0x00, 0x01, 0x00, 0x22,
		0x00, 0x34, 0x00, 0x45, 0x00, 0x54, 0x00, 0x62, 0x00, 0x70);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x00, 0x7D, 0x00, 0xAD, 0x00, 0xD4,
		0x01, 0x16, 0x01, 0x47, 0x01, 0x97, 0x01, 0xD6, 0x01, 0xD9);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x1D, 0x02, 0x67, 0x02, 0x9D,
		0x02, 0xD8, 0x03, 0x02, 0x03, 0x31, 0x03, 0x41, 0x03, 0x50);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x62, 0x03, 0x76, 0x03, 0x8D,
		0x03, 0xAB, 0x03, 0xC8, 0x03, 0xCC, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x01, 0x00, 0x22,
		0x00, 0x34, 0x00, 0x45, 0x00, 0x54, 0x00, 0x62, 0x00, 0x70);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x7D, 0x00, 0xAD, 0x00, 0xD4,
		0x01, 0x16, 0x01, 0x47, 0x01, 0x97, 0x01, 0xD6, 0x01, 0xD9);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x1D, 0x02, 0x67, 0x02, 0x9D,
		0x02, 0xD8, 0x03, 0x02, 0x03, 0x31, 0x03, 0x41, 0x03, 0x50);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x62, 0x03, 0x76, 0x03, 0x8D,
		0x03, 0xAB, 0x03, 0xC8, 0x03, 0xCC, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0X20);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0X01);

	lcm_dcs_write_seq_static(ctx, 0XFF, 0x10);

	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);

	lcm_dcs_write_seq_static(ctx, 0x35);
	/*lcm_dcs_write_seq_static(ctx, 0XBB,0x03);*/

	lcm_dcs_write_seq_static(ctx, 0x11);

	msleep(120);

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

	pr_info("%s\n", __func__);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
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

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#elif defined(CONFIG_RT4831A_I2C)
	_gate_ic_Power_on();
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

#define HFP (24)
#define HSA (20)
#define HBP (20)
#define VFP (75)
#define VSA (2)
#define VBP (12)
#define VAC (2280)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 163406,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
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
	unsigned char id[3] = {0x00, 0x80, 0x00};
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

static struct mtk_panel_params ext_params = {
	.pll_clk = 522,
	.vfp_low_power = 112,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},

};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
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

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO
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

#ifndef CONFIG_RT4831A_I2C
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

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "nt36672a,rt4801,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-nt36672a-rt4801-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Ning Feng <Ning.Feng@mediatek.com>");
MODULE_DESCRIPTION("truly nt36672a VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");

