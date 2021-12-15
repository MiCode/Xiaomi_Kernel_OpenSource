/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <linux/platform_device.h>

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

struct auo {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;

	int error;
};

#define auo_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		auo_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define auo_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		auo_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct auo *panel_to_auo(struct drm_panel *panel)
{
	return container_of(panel, struct auo, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int auo_dcs_read(struct auo *ctx, u8 cmd, void *data, size_t len)
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

static void auo_panel_get_data(struct auo *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = auo_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;

static int auo_panel_bias_regulator_init(void)
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

static int auo_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	auo_panel_bias_regulator_init();

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

static int auo_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	auo_panel_bias_regulator_init();

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

static void auo_dcs_write(struct auo *ctx, const void *data, size_t len)
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

extern void lcm_set_reset_pin(unsigned int value);
static void auo_panel_init(struct auo *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(10 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	auo_dcs_write_seq_static(ctx, 0xFF, 0x24);
	auo_dcs_write_seq_static(ctx, 0xFB, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC5, 0x31);

	auo_dcs_write_seq_static(ctx, 0xFF, 0x20);
	auo_dcs_write_seq_static(ctx, 0x00, 0x01);
	auo_dcs_write_seq_static(ctx, 0x01, 0x55);
	auo_dcs_write_seq_static(ctx, 0x02, 0x45);
	auo_dcs_write_seq_static(ctx, 0x03, 0x55);
	auo_dcs_write_seq_static(ctx, 0x05, 0x40);
	auo_dcs_write_seq_static(ctx, 0x06, 0x99);
	auo_dcs_write_seq_static(ctx, 0x07, 0x9E);
	auo_dcs_write_seq_static(ctx, 0x08, 0x0C);
	auo_dcs_write_seq_static(ctx, 0x0B, 0x87);
	auo_dcs_write_seq_static(ctx, 0x0C, 0x87);
	auo_dcs_write_seq_static(ctx, 0x0E, 0xAB);
	auo_dcs_write_seq_static(ctx, 0x0F, 0xA9);
	auo_dcs_write_seq_static(ctx, 0x11, 0x0D);
	auo_dcs_write_seq_static(ctx, 0x12, 0x10);
	auo_dcs_write_seq_static(ctx, 0x13, 0x03);
	auo_dcs_write_seq_static(ctx, 0x14, 0x4A);
	auo_dcs_write_seq_static(ctx, 0x15, 0x12);
	auo_dcs_write_seq_static(ctx, 0x16, 0x12);
	auo_dcs_write_seq_static(ctx, 0x30, 0x01);
	auo_dcs_write_seq_static(ctx, 0x58, 0x00);
	auo_dcs_write_seq_static(ctx, 0x59, 0x00);
	auo_dcs_write_seq_static(ctx, 0x5A, 0x02);
	auo_dcs_write_seq_static(ctx, 0x5B, 0x00);
	auo_dcs_write_seq_static(ctx, 0x5C, 0x00);
	auo_dcs_write_seq_static(ctx, 0x5D, 0x00);
	auo_dcs_write_seq_static(ctx, 0x5E, 0x02);
	auo_dcs_write_seq_static(ctx, 0x5F, 0x02);
	auo_dcs_write_seq_static(ctx, 0x72, 0x31);
	auo_dcs_write_seq_static(ctx, 0xFB, 0x01);

	auo_dcs_write_seq_static(ctx, 0xFF, 0x24);
	auo_dcs_write_seq_static(ctx, 0x00, 0x01);
	auo_dcs_write_seq_static(ctx, 0x01, 0x0B);
	auo_dcs_write_seq_static(ctx, 0x02, 0x0C);
	auo_dcs_write_seq_static(ctx, 0x03, 0x03);
	auo_dcs_write_seq_static(ctx, 0x04, 0x05);
	auo_dcs_write_seq_static(ctx, 0x05, 0x1C);
	auo_dcs_write_seq_static(ctx, 0x06, 0x10);
	auo_dcs_write_seq_static(ctx, 0x07, 0x00);
	auo_dcs_write_seq_static(ctx, 0x08, 0x1C);
	auo_dcs_write_seq_static(ctx, 0x09, 0x00);
	auo_dcs_write_seq_static(ctx, 0x0A, 0x00);
	auo_dcs_write_seq_static(ctx, 0x0B, 0x00);
	auo_dcs_write_seq_static(ctx, 0x0C, 0x00);
	auo_dcs_write_seq_static(ctx, 0x0D, 0x13);
	auo_dcs_write_seq_static(ctx, 0x0E, 0x15);
	auo_dcs_write_seq_static(ctx, 0x0F, 0x17);
	auo_dcs_write_seq_static(ctx, 0x10, 0x01);
	auo_dcs_write_seq_static(ctx, 0x11, 0x0B);
	auo_dcs_write_seq_static(ctx, 0x12, 0x0C);
	auo_dcs_write_seq_static(ctx, 0x13, 0x04);
	auo_dcs_write_seq_static(ctx, 0x14, 0x06);
	auo_dcs_write_seq_static(ctx, 0x15, 0x1C);
	auo_dcs_write_seq_static(ctx, 0x16, 0x10);
	auo_dcs_write_seq_static(ctx, 0x17, 0x00);
	auo_dcs_write_seq_static(ctx, 0x18, 0x1C);
	auo_dcs_write_seq_static(ctx, 0x19, 0x00);
	auo_dcs_write_seq_static(ctx, 0x1A, 0x00);
	auo_dcs_write_seq_static(ctx, 0x1B, 0x00);
	auo_dcs_write_seq_static(ctx, 0x1C, 0x00);
	auo_dcs_write_seq_static(ctx, 0x1D, 0x13);
	auo_dcs_write_seq_static(ctx, 0x1E, 0x15);
	auo_dcs_write_seq_static(ctx, 0x1F, 0x17);
	auo_dcs_write_seq_static(ctx, 0x20, 0x00);
	auo_dcs_write_seq_static(ctx, 0x21, 0x03);
	auo_dcs_write_seq_static(ctx, 0x22, 0x01);
	auo_dcs_write_seq_static(ctx, 0x23, 0x4A);
	auo_dcs_write_seq_static(ctx, 0x24, 0x4A);
	auo_dcs_write_seq_static(ctx, 0x25, 0x6D);
	auo_dcs_write_seq_static(ctx, 0x26, 0x40);
	auo_dcs_write_seq_static(ctx, 0x27, 0x40);
	auo_dcs_write_seq_static(ctx, 0x32, 0x7B);
	auo_dcs_write_seq_static(ctx, 0x33, 0x00);
	auo_dcs_write_seq_static(ctx, 0x34, 0x01);
	auo_dcs_write_seq_static(ctx, 0x35, 0x8E);
	auo_dcs_write_seq_static(ctx, 0x39, 0x01);
	auo_dcs_write_seq_static(ctx, 0x3A, 0x8E);

	auo_dcs_write_seq_static(ctx, 0xBD, 0x20); /* VEND */
	auo_dcs_write_seq_static(ctx, 0xB6, 0x22);
	auo_dcs_write_seq_static(ctx, 0xB7, 0x24);
	auo_dcs_write_seq_static(ctx, 0xB8, 0x07);
	auo_dcs_write_seq_static(ctx, 0xB9, 0x07);
	auo_dcs_write_seq_static(ctx, 0xC1, 0x6D);
	auo_dcs_write_seq_static(ctx, 0xC2, 0x00);
	auo_dcs_write_seq_static(ctx, 0xC4, 0x24);

	auo_dcs_write_seq_static(ctx, 0xBE, 0x07);
	auo_dcs_write_seq_static(ctx, 0xBF, 0x07);
	auo_dcs_write_seq_static(ctx, 0x29, 0xD8);
	auo_dcs_write_seq_static(ctx, 0x2A, 0x2A);

	auo_dcs_write_seq_static(ctx, 0x5B, 0x43); /* CTRL */
	auo_dcs_write_seq_static(ctx, 0x5C, 0x00);
	auo_dcs_write_seq_static(ctx, 0x5F, 0x73);
	auo_dcs_write_seq_static(ctx, 0x60, 0x73);
	auo_dcs_write_seq_static(ctx, 0x63, 0x22);
	auo_dcs_write_seq_static(ctx, 0x64, 0x00);
	auo_dcs_write_seq_static(ctx, 0x67, 0x08);
	auo_dcs_write_seq_static(ctx, 0x68, 0x04);

	auo_dcs_write_seq_static(ctx, 0x7A, 0x80); /* MUX */
	auo_dcs_write_seq_static(ctx, 0x7B, 0x91);
	auo_dcs_write_seq_static(ctx, 0x7C, 0xD8);
	auo_dcs_write_seq_static(ctx, 0x7D, 0x60);
	auo_dcs_write_seq_static(ctx, 0x74, 0x09);
	auo_dcs_write_seq_static(ctx, 0x7E, 0x09);
	auo_dcs_write_seq_static(ctx, 0x75, 0x21);
	auo_dcs_write_seq_static(ctx, 0x7F, 0x21);
	auo_dcs_write_seq_static(ctx, 0x76, 0x05);
	auo_dcs_write_seq_static(ctx, 0x81, 0x05);
	auo_dcs_write_seq_static(ctx, 0x77, 0x04);
	auo_dcs_write_seq_static(ctx, 0x82, 0x04);

	auo_dcs_write_seq_static(ctx, 0x93, 0x06); /* FP,BP */
	auo_dcs_write_seq_static(ctx, 0x94, 0x06);
	auo_dcs_write_seq_static(ctx, 0xB3, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB4, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB5, 0x00);

	auo_dcs_write_seq_static(ctx, 0x78, 0x00); /* SOURCE EQ */
	auo_dcs_write_seq_static(ctx, 0x79, 0x00);
	auo_dcs_write_seq_static(ctx, 0x80, 0x00);
	auo_dcs_write_seq_static(ctx, 0x83, 0x00);
	auo_dcs_write_seq_static(ctx, 0x84, 0x04);

	auo_dcs_write_seq_static(ctx, 0x8A, 0x33); /* pixel column driving */
	auo_dcs_write_seq_static(ctx, 0x8B, 0xF0);
	auo_dcs_write_seq_static(ctx, 0x9B, 0x0F);
	auo_dcs_write_seq_static(ctx, 0xC6, 0x09);
	auo_dcs_write_seq_static(ctx, 0xFB, 0x01);
	auo_dcs_write_seq_static(ctx, 0xEC, 0x00);

	auo_dcs_write_seq_static(ctx, 0xFF, 0x20);
	auo_dcs_write_seq_static(ctx, 0xFB, 0x01);
	auo_dcs_write_seq_static(ctx, 0x75, 0x00);
	auo_dcs_write_seq_static(ctx, 0x76, 0x49);
	auo_dcs_write_seq_static(ctx, 0x77, 0x00);
	auo_dcs_write_seq_static(ctx, 0x78, 0x78);
	auo_dcs_write_seq_static(ctx, 0x79, 0x00);
	auo_dcs_write_seq_static(ctx, 0x7A, 0xA4);
	auo_dcs_write_seq_static(ctx, 0x7B, 0x00);
	auo_dcs_write_seq_static(ctx, 0x7C, 0xC2);
	auo_dcs_write_seq_static(ctx, 0x7D, 0x00);
	auo_dcs_write_seq_static(ctx, 0x7E, 0xDA);
	auo_dcs_write_seq_static(ctx, 0x7F, 0x00);
	auo_dcs_write_seq_static(ctx, 0x80, 0xED);
	auo_dcs_write_seq_static(ctx, 0x81, 0x00);
	auo_dcs_write_seq_static(ctx, 0x82, 0xFE);
	auo_dcs_write_seq_static(ctx, 0x83, 0x01);
	auo_dcs_write_seq_static(ctx, 0x84, 0x0E);
	auo_dcs_write_seq_static(ctx, 0x85, 0x01);
	auo_dcs_write_seq_static(ctx, 0x86, 0x1B);
	auo_dcs_write_seq_static(ctx, 0x87, 0x01);
	auo_dcs_write_seq_static(ctx, 0x88, 0x48);
	auo_dcs_write_seq_static(ctx, 0x89, 0x01);
	auo_dcs_write_seq_static(ctx, 0x8A, 0x6C);
	auo_dcs_write_seq_static(ctx, 0x8B, 0x01);
	auo_dcs_write_seq_static(ctx, 0x8C, 0xA2);
	auo_dcs_write_seq_static(ctx, 0x8D, 0x01);
	auo_dcs_write_seq_static(ctx, 0x8E, 0xCD);
	auo_dcs_write_seq_static(ctx, 0x8F, 0x02);
	auo_dcs_write_seq_static(ctx, 0x90, 0x0F);
	auo_dcs_write_seq_static(ctx, 0x91, 0x02);
	auo_dcs_write_seq_static(ctx, 0x92, 0x42);

	auo_dcs_write_seq_static(ctx, 0x93, 0x02);
	auo_dcs_write_seq_static(ctx, 0x94, 0x43);
	auo_dcs_write_seq_static(ctx, 0x95, 0x02);
	auo_dcs_write_seq_static(ctx, 0x96, 0x71);
	auo_dcs_write_seq_static(ctx, 0x97, 0x02);
	auo_dcs_write_seq_static(ctx, 0x98, 0xA3);
	auo_dcs_write_seq_static(ctx, 0x99, 0x02);
	auo_dcs_write_seq_static(ctx, 0x9A, 0xC5);
	auo_dcs_write_seq_static(ctx, 0x9B, 0x02);
	auo_dcs_write_seq_static(ctx, 0x9C, 0xF3);
	auo_dcs_write_seq_static(ctx, 0x9D, 0x03);
	auo_dcs_write_seq_static(ctx, 0x9E, 0x12);
	auo_dcs_write_seq_static(ctx, 0x9F, 0x03);
	auo_dcs_write_seq_static(ctx, 0xA0, 0x3A);
	auo_dcs_write_seq_static(ctx, 0xA2, 0x03);
	auo_dcs_write_seq_static(ctx, 0xA3, 0x46);
	auo_dcs_write_seq_static(ctx, 0xA4, 0x03);
	auo_dcs_write_seq_static(ctx, 0xA5, 0x52);
	auo_dcs_write_seq_static(ctx, 0xA6, 0x03);
	auo_dcs_write_seq_static(ctx, 0xA7, 0x60);
	auo_dcs_write_seq_static(ctx, 0xA9, 0x03);
	auo_dcs_write_seq_static(ctx, 0xAA, 0x6E);
	auo_dcs_write_seq_static(ctx, 0xAB, 0x03);
	auo_dcs_write_seq_static(ctx, 0xAC, 0x7D);
	auo_dcs_write_seq_static(ctx, 0xAD, 0x03);
	auo_dcs_write_seq_static(ctx, 0xAE, 0x8B);
	auo_dcs_write_seq_static(ctx, 0xAF, 0x03);
	auo_dcs_write_seq_static(ctx, 0xB0, 0x91);
	auo_dcs_write_seq_static(ctx, 0xB1, 0x03);
	auo_dcs_write_seq_static(ctx, 0xB2, 0xCF);
	auo_dcs_write_seq_static(ctx, 0xB3, 0x00); /* RN GAMMA SETTING */
	auo_dcs_write_seq_static(ctx, 0xB4, 0x49);
	auo_dcs_write_seq_static(ctx, 0xB5, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB6, 0x78);
	auo_dcs_write_seq_static(ctx, 0xB7, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB8, 0xA4);
	auo_dcs_write_seq_static(ctx, 0xB9, 0x00);
	auo_dcs_write_seq_static(ctx, 0xBA, 0xC2);
	auo_dcs_write_seq_static(ctx, 0xBB, 0x00);
	auo_dcs_write_seq_static(ctx, 0xBC, 0xDA);
	auo_dcs_write_seq_static(ctx, 0xBD, 0x00);
	auo_dcs_write_seq_static(ctx, 0xBE, 0xED);
	auo_dcs_write_seq_static(ctx, 0xBF, 0x00);
	auo_dcs_write_seq_static(ctx, 0xC0, 0xFE);

	auo_dcs_write_seq_static(ctx, 0xC1, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC2, 0x0E);
	auo_dcs_write_seq_static(ctx, 0xC3, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC4, 0x1B);
	auo_dcs_write_seq_static(ctx, 0xC5, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC6, 0x48);
	auo_dcs_write_seq_static(ctx, 0xC7, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC8, 0x6C);
	auo_dcs_write_seq_static(ctx, 0xC9, 0x01);
	auo_dcs_write_seq_static(ctx, 0xCA, 0xA2);
	auo_dcs_write_seq_static(ctx, 0xCB, 0x01);
	auo_dcs_write_seq_static(ctx, 0xCC, 0xCD);
	auo_dcs_write_seq_static(ctx, 0xCD, 0x02);
	auo_dcs_write_seq_static(ctx, 0xCE, 0x0F);
	auo_dcs_write_seq_static(ctx, 0xCF, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD0, 0x42);
	auo_dcs_write_seq_static(ctx, 0xD1, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD2, 0x43);
	auo_dcs_write_seq_static(ctx, 0xD3, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD4, 0x71);
	auo_dcs_write_seq_static(ctx, 0xD5, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD6, 0xA3);
	auo_dcs_write_seq_static(ctx, 0xD7, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD8, 0xC5);

	auo_dcs_write_seq_static(ctx, 0xD9, 0x02);
	auo_dcs_write_seq_static(ctx, 0xDA, 0xF3);
	auo_dcs_write_seq_static(ctx, 0xDB, 0x03);
	auo_dcs_write_seq_static(ctx, 0xDD, 0x03);
	auo_dcs_write_seq_static(ctx, 0xDE, 0x3A);
	auo_dcs_write_seq_static(ctx, 0xDF, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE0, 0x46);
	auo_dcs_write_seq_static(ctx, 0xE1, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE2, 0x52);
	auo_dcs_write_seq_static(ctx, 0xE3, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE4, 0x60);
	auo_dcs_write_seq_static(ctx, 0xE5, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE6, 0x6E);
	auo_dcs_write_seq_static(ctx, 0xE7, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE8, 0x7D);
	auo_dcs_write_seq_static(ctx, 0xE9, 0x03);
	auo_dcs_write_seq_static(ctx, 0xEA, 0x8B);
	auo_dcs_write_seq_static(ctx, 0xEB, 0x03);
	auo_dcs_write_seq_static(ctx, 0xEC, 0x91);
	auo_dcs_write_seq_static(ctx, 0xED, 0x03);
	auo_dcs_write_seq_static(ctx, 0xEE, 0xCF);

	auo_dcs_write_seq_static(ctx, 0xEF, 0x00); /* GP GAMMA SETTING */
	auo_dcs_write_seq_static(ctx, 0xF0, 0x49);
	auo_dcs_write_seq_static(ctx, 0xF1, 0x00);
	auo_dcs_write_seq_static(ctx, 0xF2, 0x78);
	auo_dcs_write_seq_static(ctx, 0xF3, 0x00);
	auo_dcs_write_seq_static(ctx, 0xF4, 0xA4);
	auo_dcs_write_seq_static(ctx, 0xF5, 0x00);
	auo_dcs_write_seq_static(ctx, 0xF6, 0xC2);
	auo_dcs_write_seq_static(ctx, 0xF7, 0x00);
	auo_dcs_write_seq_static(ctx, 0xF8, 0xDA);
	auo_dcs_write_seq_static(ctx, 0xF9, 0x00);
	auo_dcs_write_seq_static(ctx, 0xFA, 0xED);

	auo_dcs_write_seq_static(ctx, 0xFF, 0x21); /* CMD2 PAGE1 */
	auo_dcs_write_seq_static(ctx, 0xFB, 0x01);

	auo_dcs_write_seq_static(ctx, 0x00, 0x00);
	auo_dcs_write_seq_static(ctx, 0x01, 0xFE);
	auo_dcs_write_seq_static(ctx, 0x02, 0x01);
	auo_dcs_write_seq_static(ctx, 0x03, 0x0E);
	auo_dcs_write_seq_static(ctx, 0x04, 0x01);
	auo_dcs_write_seq_static(ctx, 0x05, 0x1B);
	auo_dcs_write_seq_static(ctx, 0x06, 0x01);
	auo_dcs_write_seq_static(ctx, 0x07, 0x48);
	auo_dcs_write_seq_static(ctx, 0x08, 0x01);
	auo_dcs_write_seq_static(ctx, 0x09, 0x6C);
	auo_dcs_write_seq_static(ctx, 0x0A, 0x01);
	auo_dcs_write_seq_static(ctx, 0x0B, 0xA2);
	auo_dcs_write_seq_static(ctx, 0x0C, 0x01);
	auo_dcs_write_seq_static(ctx, 0x0D, 0xCD);
	auo_dcs_write_seq_static(ctx, 0x0E, 0x02);
	auo_dcs_write_seq_static(ctx, 0x0F, 0x0F);
	auo_dcs_write_seq_static(ctx, 0x10, 0x02);
	auo_dcs_write_seq_static(ctx, 0x11, 0x42);
	auo_dcs_write_seq_static(ctx, 0x12, 0x02);
	auo_dcs_write_seq_static(ctx, 0x13, 0x43);
	auo_dcs_write_seq_static(ctx, 0x14, 0x02);
	auo_dcs_write_seq_static(ctx, 0x15, 0x71);

	auo_dcs_write_seq_static(ctx, 0x16, 0x02);
	auo_dcs_write_seq_static(ctx, 0x17, 0xA3);
	auo_dcs_write_seq_static(ctx, 0x18, 0x02);
	auo_dcs_write_seq_static(ctx, 0x19, 0xC5);
	auo_dcs_write_seq_static(ctx, 0x1A, 0x02);
	auo_dcs_write_seq_static(ctx, 0x1B, 0xF3);
	auo_dcs_write_seq_static(ctx, 0x1C, 0x03);
	auo_dcs_write_seq_static(ctx, 0x1D, 0x12);
	auo_dcs_write_seq_static(ctx, 0x1E, 0x03);
	auo_dcs_write_seq_static(ctx, 0x1F, 0x3A);
	auo_dcs_write_seq_static(ctx, 0x20, 0x03);
	auo_dcs_write_seq_static(ctx, 0x21, 0x46);
	auo_dcs_write_seq_static(ctx, 0x22, 0x03);
	auo_dcs_write_seq_static(ctx, 0x23, 0x52);
	auo_dcs_write_seq_static(ctx, 0x24, 0x03);
	auo_dcs_write_seq_static(ctx, 0x25, 0x60);
	auo_dcs_write_seq_static(ctx, 0x26, 0x03);
	auo_dcs_write_seq_static(ctx, 0x27, 0x6E);
	auo_dcs_write_seq_static(ctx, 0x28, 0x03);
	auo_dcs_write_seq_static(ctx, 0x29, 0x7D);
	auo_dcs_write_seq_static(ctx, 0x2A, 0x03);
	auo_dcs_write_seq_static(ctx, 0x2B, 0x8B);
	auo_dcs_write_seq_static(ctx, 0x2D, 0x03);
	auo_dcs_write_seq_static(ctx, 0x2F, 0x91);
	auo_dcs_write_seq_static(ctx, 0x30, 0x03);
	auo_dcs_write_seq_static(ctx, 0x31, 0xCF);
	auo_dcs_write_seq_static(ctx, 0x32, 0x00);
	auo_dcs_write_seq_static(ctx, 0x33, 0x49);
	auo_dcs_write_seq_static(ctx, 0x34, 0x00);
	auo_dcs_write_seq_static(ctx, 0x35, 0x78);
	auo_dcs_write_seq_static(ctx, 0x36, 0x00);
	auo_dcs_write_seq_static(ctx, 0x37, 0xA4);
	auo_dcs_write_seq_static(ctx, 0x38, 0x00);
	auo_dcs_write_seq_static(ctx, 0x39, 0xC2);
	auo_dcs_write_seq_static(ctx, 0x3A, 0x00);
	auo_dcs_write_seq_static(ctx, 0x3B, 0xDA);
	auo_dcs_write_seq_static(ctx, 0x3D, 0x00);
	auo_dcs_write_seq_static(ctx, 0x3F, 0xED);
	auo_dcs_write_seq_static(ctx, 0x40, 0x00);
	auo_dcs_write_seq_static(ctx, 0x41, 0xFE);
	auo_dcs_write_seq_static(ctx, 0x42, 0x01);
	auo_dcs_write_seq_static(ctx, 0x43, 0x0E);
	auo_dcs_write_seq_static(ctx, 0x44, 0x01);
	auo_dcs_write_seq_static(ctx, 0x45, 0x1B);
	auo_dcs_write_seq_static(ctx, 0x46, 0x01);
	auo_dcs_write_seq_static(ctx, 0x47, 0x48);
	auo_dcs_write_seq_static(ctx, 0x48, 0x01);
	auo_dcs_write_seq_static(ctx, 0x49, 0x6C);
	auo_dcs_write_seq_static(ctx, 0x4A, 0x01);
	auo_dcs_write_seq_static(ctx, 0x4B, 0xA2);
	auo_dcs_write_seq_static(ctx, 0x4C, 0x01);
	auo_dcs_write_seq_static(ctx, 0x4D, 0xCD);
	auo_dcs_write_seq_static(ctx, 0x4E, 0x02);
	auo_dcs_write_seq_static(ctx, 0x4F, 0x0F);
	auo_dcs_write_seq_static(ctx, 0x50, 0x02);
	auo_dcs_write_seq_static(ctx, 0x51, 0x42);
	auo_dcs_write_seq_static(ctx, 0x52, 0x02);
	auo_dcs_write_seq_static(ctx, 0x53, 0x43);
	auo_dcs_write_seq_static(ctx, 0x54, 0x02);
	auo_dcs_write_seq_static(ctx, 0x55, 0x71);
	auo_dcs_write_seq_static(ctx, 0x56, 0x02);
	auo_dcs_write_seq_static(ctx, 0x58, 0xA3);
	auo_dcs_write_seq_static(ctx, 0x59, 0x02);
	auo_dcs_write_seq_static(ctx, 0x5A, 0xC5);
	auo_dcs_write_seq_static(ctx, 0x5B, 0x02);
	auo_dcs_write_seq_static(ctx, 0x5C, 0xF3);
	auo_dcs_write_seq_static(ctx, 0x5D, 0x03);
	auo_dcs_write_seq_static(ctx, 0x5E, 0x12);
	auo_dcs_write_seq_static(ctx, 0x5F, 0x03);
	auo_dcs_write_seq_static(ctx, 0x60, 0x3A);
	auo_dcs_write_seq_static(ctx, 0x61, 0x03);
	auo_dcs_write_seq_static(ctx, 0x62, 0x46);
	auo_dcs_write_seq_static(ctx, 0x63, 0x03);
	auo_dcs_write_seq_static(ctx, 0x64, 0x52);
	auo_dcs_write_seq_static(ctx, 0x65, 0x03);
	auo_dcs_write_seq_static(ctx, 0x66, 0x60);

	auo_dcs_write_seq_static(ctx, 0x67, 0x03);
	auo_dcs_write_seq_static(ctx, 0x68, 0x6E);
	auo_dcs_write_seq_static(ctx, 0x69, 0x03);
	auo_dcs_write_seq_static(ctx, 0x6A, 0x7D);
	auo_dcs_write_seq_static(ctx, 0x6B, 0x03);
	auo_dcs_write_seq_static(ctx, 0x6C, 0x8B);
	auo_dcs_write_seq_static(ctx, 0x6D, 0x03);
	auo_dcs_write_seq_static(ctx, 0x6E, 0x91);
	auo_dcs_write_seq_static(ctx, 0x6F, 0x03);
	auo_dcs_write_seq_static(ctx, 0x70, 0xCF);
	auo_dcs_write_seq_static(ctx, 0x71, 0x00); /* BP GAMMA SETTING */
	auo_dcs_write_seq_static(ctx, 0x72, 0x49);
	auo_dcs_write_seq_static(ctx, 0x73, 0x00);
	auo_dcs_write_seq_static(ctx, 0x74, 0x78);
	auo_dcs_write_seq_static(ctx, 0x75, 0x00);
	auo_dcs_write_seq_static(ctx, 0x76, 0xA4);
	auo_dcs_write_seq_static(ctx, 0x77, 0x00);
	auo_dcs_write_seq_static(ctx, 0x78, 0xC2);
	auo_dcs_write_seq_static(ctx, 0x79, 0x00);
	auo_dcs_write_seq_static(ctx, 0x7A, 0xDA);
	auo_dcs_write_seq_static(ctx, 0x7B, 0x00);
	auo_dcs_write_seq_static(ctx, 0x7C, 0xED);
	auo_dcs_write_seq_static(ctx, 0x7D, 0x00);
	auo_dcs_write_seq_static(ctx, 0x7E, 0xFE);
	auo_dcs_write_seq_static(ctx, 0x7F, 0x01);
	auo_dcs_write_seq_static(ctx, 0x80, 0x0E);
	auo_dcs_write_seq_static(ctx, 0x81, 0x01);
	auo_dcs_write_seq_static(ctx, 0x82, 0x1B);
	auo_dcs_write_seq_static(ctx, 0x83, 0x01);
	auo_dcs_write_seq_static(ctx, 0x84, 0x48);
	auo_dcs_write_seq_static(ctx, 0x85, 0x01);
	auo_dcs_write_seq_static(ctx, 0x86, 0x6C);
	auo_dcs_write_seq_static(ctx, 0x87, 0x01);
	auo_dcs_write_seq_static(ctx, 0x88, 0xA2);
	auo_dcs_write_seq_static(ctx, 0x89, 0x01);
	auo_dcs_write_seq_static(ctx, 0x8A, 0xCD);
	auo_dcs_write_seq_static(ctx, 0x8B, 0x02);
	auo_dcs_write_seq_static(ctx, 0x8C, 0x0F);
	auo_dcs_write_seq_static(ctx, 0x8D, 0x02);
	auo_dcs_write_seq_static(ctx, 0x8E, 0x42);
	auo_dcs_write_seq_static(ctx, 0x8F, 0x02);
	auo_dcs_write_seq_static(ctx, 0x90, 0x43);
	auo_dcs_write_seq_static(ctx, 0x91, 0x02);
	auo_dcs_write_seq_static(ctx, 0x92, 0x71);
	auo_dcs_write_seq_static(ctx, 0x93, 0x02);
	auo_dcs_write_seq_static(ctx, 0x94, 0xA3);
	auo_dcs_write_seq_static(ctx, 0x95, 0x02);
	auo_dcs_write_seq_static(ctx, 0x96, 0xC5);
	auo_dcs_write_seq_static(ctx, 0x97, 0x02);
	auo_dcs_write_seq_static(ctx, 0x98, 0xF3);
	auo_dcs_write_seq_static(ctx, 0x99, 0x03);
	auo_dcs_write_seq_static(ctx, 0x9A, 0x12);
	auo_dcs_write_seq_static(ctx, 0x9B, 0x03);
	auo_dcs_write_seq_static(ctx, 0x9C, 0x3A);
	auo_dcs_write_seq_static(ctx, 0x9D, 0x03);
	auo_dcs_write_seq_static(ctx, 0x9E, 0x46);
	auo_dcs_write_seq_static(ctx, 0x9F, 0x03);
	auo_dcs_write_seq_static(ctx, 0xA0, 0x52);
	auo_dcs_write_seq_static(ctx, 0xA2, 0x03);
	auo_dcs_write_seq_static(ctx, 0xA3, 0x60);
	auo_dcs_write_seq_static(ctx, 0xA4, 0x03);
	auo_dcs_write_seq_static(ctx, 0xA5, 0x6E);
	auo_dcs_write_seq_static(ctx, 0xA6, 0x03);
	auo_dcs_write_seq_static(ctx, 0xA7, 0x7D);
	auo_dcs_write_seq_static(ctx, 0xA9, 0x03);
	auo_dcs_write_seq_static(ctx, 0xAA, 0x8B);
	auo_dcs_write_seq_static(ctx, 0xAB, 0x03);
	auo_dcs_write_seq_static(ctx, 0xAC, 0x91);
	auo_dcs_write_seq_static(ctx, 0xAD, 0x03);
	auo_dcs_write_seq_static(ctx, 0xAE, 0xCF);
	auo_dcs_write_seq_static(ctx, 0xAF, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB0, 0x49);
	auo_dcs_write_seq_static(ctx, 0xB1, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB2, 0x78);
	auo_dcs_write_seq_static(ctx, 0xB3, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB4, 0xA4);
	auo_dcs_write_seq_static(ctx, 0xB5, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB6, 0xC2);
	auo_dcs_write_seq_static(ctx, 0xB7, 0x00);
	auo_dcs_write_seq_static(ctx, 0xB8, 0xDA);
	auo_dcs_write_seq_static(ctx, 0xB9, 0x00);
	auo_dcs_write_seq_static(ctx, 0xBA, 0xED);
	auo_dcs_write_seq_static(ctx, 0xBB, 0x00);
	auo_dcs_write_seq_static(ctx, 0xBC, 0xFE);
	auo_dcs_write_seq_static(ctx, 0xBD, 0x01);
	auo_dcs_write_seq_static(ctx, 0xBE, 0x0E);
	auo_dcs_write_seq_static(ctx, 0xBF, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC0, 0x1B);
	auo_dcs_write_seq_static(ctx, 0xC1, 0x01);

	auo_dcs_write_seq_static(ctx, 0xC2, 0x48);
	auo_dcs_write_seq_static(ctx, 0xC3, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC4, 0x6C);
	auo_dcs_write_seq_static(ctx, 0xC5, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC6, 0xA2);
	auo_dcs_write_seq_static(ctx, 0xC7, 0x01);
	auo_dcs_write_seq_static(ctx, 0xC8, 0xCD);
	auo_dcs_write_seq_static(ctx, 0xC9, 0x02);
	auo_dcs_write_seq_static(ctx, 0xCA, 0x0F);
	auo_dcs_write_seq_static(ctx, 0xCB, 0x02);
	auo_dcs_write_seq_static(ctx, 0xCC, 0x42);
	auo_dcs_write_seq_static(ctx, 0xCD, 0x02);
	auo_dcs_write_seq_static(ctx, 0xCE, 0x43);
	auo_dcs_write_seq_static(ctx, 0xCF, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD0, 0x71);
	auo_dcs_write_seq_static(ctx, 0xD1, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD2, 0xA3);
	auo_dcs_write_seq_static(ctx, 0xD3, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD4, 0xC5);
	auo_dcs_write_seq_static(ctx, 0xD5, 0x02);
	auo_dcs_write_seq_static(ctx, 0xD6, 0xF3);
	auo_dcs_write_seq_static(ctx, 0xD7, 0x03);
	auo_dcs_write_seq_static(ctx, 0xD8, 0x12);

	auo_dcs_write_seq_static(ctx, 0xD9, 0x03);
	auo_dcs_write_seq_static(ctx, 0xDA, 0x3A);
	auo_dcs_write_seq_static(ctx, 0xDB, 0x03);
	auo_dcs_write_seq_static(ctx, 0xDC, 0x46);
	auo_dcs_write_seq_static(ctx, 0xDD, 0x03);
	auo_dcs_write_seq_static(ctx, 0xDE, 0x52);
	auo_dcs_write_seq_static(ctx, 0xDF, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE0, 0x60);
	auo_dcs_write_seq_static(ctx, 0xE1, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE2, 0x6E);
	auo_dcs_write_seq_static(ctx, 0xE3, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE4, 0x7D);
	auo_dcs_write_seq_static(ctx, 0xE5, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE6, 0x8B);
	auo_dcs_write_seq_static(ctx, 0xE7, 0x03);
	auo_dcs_write_seq_static(ctx, 0xE8, 0x91);
	auo_dcs_write_seq_static(ctx, 0xE9, 0x03);
	auo_dcs_write_seq_static(ctx, 0xEA, 0xCF);

	auo_dcs_write_seq_static(ctx, 0xFF, 0x10);
	usleep_range(1000, 1100);
	auo_dcs_write_seq_static(ctx, 0x3B, 0x03, 0x0A, 0x0A);

	auo_dcs_write_seq_static(ctx, 0x35, 0x00);
	auo_dcs_write_seq_static(ctx, 0x44, 0x07, 0x78);
	auo_dcs_write_seq_static(ctx, 0xFB, 0x01);

	auo_dcs_write_seq_static(ctx, 0xC9, 0x49, 0x02, 0x05, 0x00, 0x0F, 0x06,
				 0x67, 0x03, 0x2E, 0x10, 0xF0);
	auo_dcs_write_seq_static(ctx, 0xBB, 0x03);
	auo_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	auo_dcs_write_seq_static(ctx, 0x29);

	auo_dcs_write_seq_static(ctx, 0x51, 0x00);
	auo_dcs_write_seq_static(ctx, 0x5E, 0x00);
	auo_dcs_write_seq_static(ctx, 0x53, 0x24);
	auo_dcs_write_seq_static(ctx, 0x55, 0x00);
}

static int auo_disable(struct drm_panel *panel)
{
	struct auo *ctx = panel_to_auo(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int auo_unprepare(struct drm_panel *panel)
{
	struct auo *ctx = panel_to_auo(panel);

	if (!ctx->prepared)
		return 0;

	auo_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	auo_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(80);
	auo_dcs_write_seq_static(ctx, 0x4F);
	msleep(20);

	ctx->error = 0;
	ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	auo_panel_bias_disable();
#endif

	return 0;
}

static int auo_prepare(struct drm_panel *panel)
{
	struct auo *ctx = panel_to_auo(panel);
	int ret;

	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	auo_panel_bias_enable();
#endif

	auo_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		auo_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	auo_panel_get_data(ctx);
#endif

	return ret;
}

static int auo_enable(struct drm_panel *panel)
{
	struct auo *ctx = panel_to_auo(panel);

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
	.clock = 135930,
	.hdisplay = 1080,
	.hsync_start = 1080 + 40,
	.hsync_end = 1080 + 40 + 10,
	.htotal = 1080 + 40 + 10 + 20,
	.vdisplay = 1920,
	.vsync_start = 1920 + 40,
	.vsync_end = 1920 + 40 + 2,
	.vtotal = 1920 + 40 + 2 + 8,
	.vrefresh = 60,
};

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

static int auo_get_modes(struct drm_panel *panel)
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

	panel->connector->display_info.width_mm = 75;
	panel->connector->display_info.height_mm = 133;

	return 1;
}

static const struct drm_panel_funcs auo_drm_funcs = {
	.disable = auo_disable,
	.unprepare = auo_unprepare,
	.prepare = auo_prepare,
	.enable = auo_enable,
	.get_modes = auo_get_modes,
};

static int auo_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct auo *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct auo), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->prepared = false;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &auo_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
	pr_info("%s-\n", __func__);

	return ret;
}

static int auo_remove(struct mipi_dsi_device *dsi)
{
	struct auo *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id auo_of_match[] = {
	{
		.compatible = "auo,u40073",
	},
	{} };

MODULE_DEVICE_TABLE(of, auo_of_match);

static struct mipi_dsi_driver auo_driver = {
	.probe = auo_probe,
	.remove = auo_remove,
	.driver = {

			.name = "panel-auo-u40073",
			.owner = THIS_MODULE,
			.of_match_table = auo_of_match,
		},
};

module_mipi_dsi_driver(auo_driver);

MODULE_AUTHOR("Robin Chen <robin.chen@mediatek.com>");
MODULE_DESCRIPTION("AUO U40073 LCD Panel Driver");
MODULE_LICENSE("GPL v2");
