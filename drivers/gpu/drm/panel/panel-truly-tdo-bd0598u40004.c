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

#include <linux/backlight.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

/* #define PANEL_BIST_PATTERN */	/* enable this to check panel self -bist pattern */
#define PANEL_SUPPORT_READBACK	/* option function to read data from some panel address*/

struct truly {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct regulator_bulk_data supplies[3];
	struct gpio_desc *reset_gpio;
	u32 power_on_delay;
	u32 reset_delay;
	u32 init_delay;

	u8 version;
	u8 id;
	bool prepared;
	bool enabled;

	int error;
};

#define truly_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	truly_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define truly_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	truly_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct truly *panel_to_truly(struct drm_panel *panel)
{
	return container_of(panel, struct truly, panel);
}

static int truly_clear_error(struct truly *ctx)
{
	int ret = ctx->error;

	ctx->error = 0;
	return ret;
}

#ifdef PANEL_SUPPORT_READBACK
static int truly_dcs_read(struct truly *ctx, u8 cmd, void *data, size_t len)
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

static void truly_panel_get_data(struct truly *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (0 == ret) {
		ret = truly_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void truly_dcs_write(struct truly *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing dcs seq: %*ph\n", ret, len,
			data);
		ctx->error = ret;
	}
}

static void truly_panel_init(struct truly *ctx)
{
	truly_dcs_write_seq_static(ctx, 0xFF, 0x10);
	mdelay(2);

	truly_dcs_write_seq_static(ctx, 0xBB, 0x03);
	truly_dcs_write_seq_static(ctx, 0x3B, 0x03, 0x0A, 0x0A,
				   0x0A, 0x0A);
	truly_dcs_write_seq_static(ctx, 0x53, 0x24);
	truly_dcs_write_seq_static(ctx, 0x55, 0x00);
	truly_dcs_write_seq_static(ctx, 0x5E, 0x00);

#ifndef PANEL_BIST_PATTERN
	truly_dcs_write_seq_static(ctx, 0x11);
	mdelay(150);
#endif

	truly_dcs_write_seq_static(ctx, 0xFF, 0x24);
	mdelay(2);

	truly_dcs_write_seq_static(ctx, 0xFB, 0x01);
	truly_dcs_write_seq_static(ctx, 0x9D, 0xB0);
	truly_dcs_write_seq_static(ctx, 0x72, 0x00);
	truly_dcs_write_seq_static(ctx, 0x93, 0x04);
	truly_dcs_write_seq_static(ctx, 0x94, 0x04);
	truly_dcs_write_seq_static(ctx, 0x9B, 0x0F);
	truly_dcs_write_seq_static(ctx, 0x8A, 0x33);
	truly_dcs_write_seq_static(ctx, 0x86, 0x1B);
	truly_dcs_write_seq_static(ctx, 0x87, 0x39);
	truly_dcs_write_seq_static(ctx, 0x88, 0x1B);
	truly_dcs_write_seq_static(ctx, 0x89, 0x39);
	truly_dcs_write_seq_static(ctx, 0x8B, 0xF4);
	truly_dcs_write_seq_static(ctx, 0x8C, 0x01);
	truly_dcs_write_seq_static(ctx, 0x90, 0x79);
	truly_dcs_write_seq_static(ctx, 0x91, 0x4C);
	truly_dcs_write_seq_static(ctx, 0x92, 0x77);
	truly_dcs_write_seq_static(ctx, 0x95, 0xE4);
	truly_dcs_write_seq_static(ctx, 0xDE, 0xFF);
	truly_dcs_write_seq_static(ctx, 0xDF, 0x82);
	truly_dcs_write_seq_static(ctx, 0x00, 0x0F);
	truly_dcs_write_seq_static(ctx, 0x01, 0x00);
	truly_dcs_write_seq_static(ctx, 0x02, 0x00);
	truly_dcs_write_seq_static(ctx, 0x03, 0x00);
	truly_dcs_write_seq_static(ctx, 0x04, 0x0B);
	truly_dcs_write_seq_static(ctx, 0x05, 0x0C);
	truly_dcs_write_seq_static(ctx, 0x06, 0x00);
	truly_dcs_write_seq_static(ctx, 0x07, 0x00);
	truly_dcs_write_seq_static(ctx, 0x08, 0x00);
	truly_dcs_write_seq_static(ctx, 0x09, 0x00);
	truly_dcs_write_seq_static(ctx, 0x0A, 0x03);
	truly_dcs_write_seq_static(ctx, 0x0B, 0x04);
	truly_dcs_write_seq_static(ctx, 0x0C, 0x01);
	truly_dcs_write_seq_static(ctx, 0x0D, 0x13);
	truly_dcs_write_seq_static(ctx, 0x0E, 0x15);
	truly_dcs_write_seq_static(ctx, 0x0F, 0x17);
	truly_dcs_write_seq_static(ctx, 0x10, 0x0F);
	truly_dcs_write_seq_static(ctx, 0x11, 0x00);
	truly_dcs_write_seq_static(ctx, 0x12, 0x00);
	truly_dcs_write_seq_static(ctx, 0x13, 0x00);
	truly_dcs_write_seq_static(ctx, 0x14, 0x0B);
	truly_dcs_write_seq_static(ctx, 0x15, 0x0C);
	truly_dcs_write_seq_static(ctx, 0x16, 0x00);
	truly_dcs_write_seq_static(ctx, 0x17, 0x00);
	truly_dcs_write_seq_static(ctx, 0x18, 0x00);
	truly_dcs_write_seq_static(ctx, 0x19, 0x00);
	truly_dcs_write_seq_static(ctx, 0x1A, 0x03);
	truly_dcs_write_seq_static(ctx, 0x1B, 0x04);
	truly_dcs_write_seq_static(ctx, 0x1C, 0x01);
	truly_dcs_write_seq_static(ctx, 0x1D, 0x13);
	truly_dcs_write_seq_static(ctx, 0x1E, 0x15);
	truly_dcs_write_seq_static(ctx, 0x1F, 0x17);
	truly_dcs_write_seq_static(ctx, 0x20, 0x09);
	truly_dcs_write_seq_static(ctx, 0x21, 0x01);
	truly_dcs_write_seq_static(ctx, 0x22, 0x00);
	truly_dcs_write_seq_static(ctx, 0x23, 0x00);
	truly_dcs_write_seq_static(ctx, 0x24, 0x00);
	truly_dcs_write_seq_static(ctx, 0x25, 0x6D);
	truly_dcs_write_seq_static(ctx, 0x26, 0x00);
	truly_dcs_write_seq_static(ctx, 0x27, 0x00);
	truly_dcs_write_seq_static(ctx, 0x2F, 0x02);
	truly_dcs_write_seq_static(ctx, 0x30, 0x04);
	truly_dcs_write_seq_static(ctx, 0x31, 0x49);
	truly_dcs_write_seq_static(ctx, 0x32, 0x23);
	truly_dcs_write_seq_static(ctx, 0x33, 0x01);
	truly_dcs_write_seq_static(ctx, 0x34, 0x00);
	truly_dcs_write_seq_static(ctx, 0x35, 0x69);
	truly_dcs_write_seq_static(ctx, 0x36, 0x00);
	truly_dcs_write_seq_static(ctx, 0x37, 0x2D);
	truly_dcs_write_seq_static(ctx, 0x38, 0x08);
	truly_dcs_write_seq_static(ctx, 0x39, 0x00);
	truly_dcs_write_seq_static(ctx, 0x3A, 0x69);
	truly_dcs_write_seq_static(ctx, 0x29, 0x58);
	truly_dcs_write_seq_static(ctx, 0x2A, 0x16);
	truly_dcs_write_seq_static(ctx, 0x5B, 0x00);
	truly_dcs_write_seq_static(ctx, 0x5F, 0x75);
	truly_dcs_write_seq_static(ctx, 0x63, 0x00);
	truly_dcs_write_seq_static(ctx, 0x67, 0x04);
	truly_dcs_write_seq_static(ctx, 0x7B, 0x80);
	truly_dcs_write_seq_static(ctx, 0x7C, 0xD8);
	truly_dcs_write_seq_static(ctx, 0x7D, 0x60);
	truly_dcs_write_seq_static(ctx, 0x7E, 0x10);
	truly_dcs_write_seq_static(ctx, 0x7F, 0x19);
	truly_dcs_write_seq_static(ctx, 0x80, 0x00);
	truly_dcs_write_seq_static(ctx, 0x81, 0x06);
	truly_dcs_write_seq_static(ctx, 0x82, 0x03);
	truly_dcs_write_seq_static(ctx, 0x83, 0x00);
	truly_dcs_write_seq_static(ctx, 0x84, 0x03);
	truly_dcs_write_seq_static(ctx, 0x85, 0x07);
	truly_dcs_write_seq_static(ctx, 0x74, 0x10);
	truly_dcs_write_seq_static(ctx, 0x75, 0x19);
	truly_dcs_write_seq_static(ctx, 0x76, 0x06);
	truly_dcs_write_seq_static(ctx, 0x77, 0x03);
	truly_dcs_write_seq_static(ctx, 0x78, 0x00);
	truly_dcs_write_seq_static(ctx, 0x79, 0x00);
	truly_dcs_write_seq_static(ctx, 0x99, 0x33);
	truly_dcs_write_seq_static(ctx, 0x98, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB3, 0x28);
	truly_dcs_write_seq_static(ctx, 0xB4, 0x05);
	truly_dcs_write_seq_static(ctx, 0xB5, 0x10);
	truly_dcs_write_seq_static(ctx, 0xFF, 0x20);
	mdelay(2);

	truly_dcs_write_seq_static(ctx, 0x00, 0x01);
	truly_dcs_write_seq_static(ctx, 0x01, 0x55);
	truly_dcs_write_seq_static(ctx, 0x02, 0x45);
	truly_dcs_write_seq_static(ctx, 0x03, 0x55);
	truly_dcs_write_seq_static(ctx, 0x05, 0x50);
	truly_dcs_write_seq_static(ctx, 0x06, 0x9E);
	truly_dcs_write_seq_static(ctx, 0x07, 0xA8);
	truly_dcs_write_seq_static(ctx, 0x08, 0x0C);
	truly_dcs_write_seq_static(ctx, 0x0B, 0x96);
	truly_dcs_write_seq_static(ctx, 0x0C, 0x96);
	truly_dcs_write_seq_static(ctx, 0x0E, 0x00);
	truly_dcs_write_seq_static(ctx, 0x0F, 0x00);
	truly_dcs_write_seq_static(ctx, 0x11, 0x29);
	truly_dcs_write_seq_static(ctx, 0x12, 0x29);
	truly_dcs_write_seq_static(ctx, 0x13, 0x03);
	truly_dcs_write_seq_static(ctx, 0x14, 0x0A);
	truly_dcs_write_seq_static(ctx, 0x15, 0x99);
	truly_dcs_write_seq_static(ctx, 0x16, 0x99);
	truly_dcs_write_seq_static(ctx, 0x6D, 0x44);
	truly_dcs_write_seq_static(ctx, 0x58, 0x05);
	truly_dcs_write_seq_static(ctx, 0x59, 0x05);
	truly_dcs_write_seq_static(ctx, 0x5A, 0x05);
	truly_dcs_write_seq_static(ctx, 0x5B, 0x05);
	truly_dcs_write_seq_static(ctx, 0x5C, 0x00);
	truly_dcs_write_seq_static(ctx, 0x5D, 0x00);
	truly_dcs_write_seq_static(ctx, 0x5E, 0x00);
	truly_dcs_write_seq_static(ctx, 0x5F, 0x00);
	truly_dcs_write_seq_static(ctx, 0x1B, 0x39);
	truly_dcs_write_seq_static(ctx, 0x1C, 0x39);
	truly_dcs_write_seq_static(ctx, 0x1D, 0x47);
	truly_dcs_write_seq_static(ctx, 0xFF, 0x20);
	mdelay(20);
		/*	R+ */
	truly_dcs_write_seq_static(ctx, 0x75, 0x00);
	truly_dcs_write_seq_static(ctx, 0x76, 0x00);
	truly_dcs_write_seq_static(ctx, 0x77, 0x00);
	truly_dcs_write_seq_static(ctx, 0x78, 0x22);
	truly_dcs_write_seq_static(ctx, 0x79, 0x00);
	truly_dcs_write_seq_static(ctx, 0x7A, 0x46);
	truly_dcs_write_seq_static(ctx, 0x7B, 0x00);
	truly_dcs_write_seq_static(ctx, 0x7C, 0x5C);
	truly_dcs_write_seq_static(ctx, 0x7D, 0x00);
	truly_dcs_write_seq_static(ctx, 0x7E, 0x76);
	truly_dcs_write_seq_static(ctx, 0x7F, 0x00);
	truly_dcs_write_seq_static(ctx, 0x80, 0x8D);
	truly_dcs_write_seq_static(ctx, 0x81, 0x00);
	truly_dcs_write_seq_static(ctx, 0x82, 0xA6);
	truly_dcs_write_seq_static(ctx, 0x83, 0x00);
	truly_dcs_write_seq_static(ctx, 0x84, 0xB8);
	truly_dcs_write_seq_static(ctx, 0x85, 0x00);
	truly_dcs_write_seq_static(ctx, 0x86, 0xC7);
	truly_dcs_write_seq_static(ctx, 0x87, 0x00);
	truly_dcs_write_seq_static(ctx, 0x88, 0xF6);
	truly_dcs_write_seq_static(ctx, 0x89, 0x01);
	truly_dcs_write_seq_static(ctx, 0x8A, 0x1D);
	truly_dcs_write_seq_static(ctx, 0x8B, 0x01);
	truly_dcs_write_seq_static(ctx, 0x8C, 0x54);
	truly_dcs_write_seq_static(ctx, 0x8D, 0x01);
	truly_dcs_write_seq_static(ctx, 0x8E, 0x81);
	truly_dcs_write_seq_static(ctx, 0x8F, 0x01);
	truly_dcs_write_seq_static(ctx, 0x90, 0xCB);
	truly_dcs_write_seq_static(ctx, 0x91, 0x02);
	truly_dcs_write_seq_static(ctx, 0x92, 0x05);
	truly_dcs_write_seq_static(ctx, 0x93, 0x02);
	truly_dcs_write_seq_static(ctx, 0x94, 0x07);
	truly_dcs_write_seq_static(ctx, 0x95, 0x02);
	truly_dcs_write_seq_static(ctx, 0x96, 0x47);
	truly_dcs_write_seq_static(ctx, 0x97, 0x02);
	truly_dcs_write_seq_static(ctx, 0x98, 0x82);
	truly_dcs_write_seq_static(ctx, 0x99, 0x02);
	truly_dcs_write_seq_static(ctx, 0x9A, 0xAB);
	truly_dcs_write_seq_static(ctx, 0x9B, 0x02);
	truly_dcs_write_seq_static(ctx, 0x9C, 0xDC);
	truly_dcs_write_seq_static(ctx, 0x9D, 0x03);
	truly_dcs_write_seq_static(ctx, 0x9E, 0x01);
	truly_dcs_write_seq_static(ctx, 0x9F, 0x03);
	truly_dcs_write_seq_static(ctx, 0xA0, 0x3A);
	truly_dcs_write_seq_static(ctx, 0xA2, 0x03);
	truly_dcs_write_seq_static(ctx, 0xA3, 0x56);
	truly_dcs_write_seq_static(ctx, 0xA4, 0x03);
	truly_dcs_write_seq_static(ctx, 0xA5, 0x6D);
	truly_dcs_write_seq_static(ctx, 0xA6, 0x03);
	truly_dcs_write_seq_static(ctx, 0xA7, 0x89);
	truly_dcs_write_seq_static(ctx, 0xA9, 0x03);
	truly_dcs_write_seq_static(ctx, 0xAA, 0xA3);
	truly_dcs_write_seq_static(ctx, 0xAB, 0x03);
	truly_dcs_write_seq_static(ctx, 0xAC, 0xC9);
	truly_dcs_write_seq_static(ctx, 0xAD, 0x03);
	truly_dcs_write_seq_static(ctx, 0xAE, 0xDD);
	truly_dcs_write_seq_static(ctx, 0xAF, 0x03);
	truly_dcs_write_seq_static(ctx, 0xB0, 0xF5);
	truly_dcs_write_seq_static(ctx, 0xB1, 0x03);
	truly_dcs_write_seq_static(ctx, 0xB2, 0xFF);
		/*	R- */
	truly_dcs_write_seq_static(ctx, 0xB3, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB4, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB5, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB6, 0x22);
	truly_dcs_write_seq_static(ctx, 0xB7, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB8, 0x46);
	truly_dcs_write_seq_static(ctx, 0xB9, 0x00);
	truly_dcs_write_seq_static(ctx, 0xBA, 0x5C);
	truly_dcs_write_seq_static(ctx, 0xBB, 0x00);
	truly_dcs_write_seq_static(ctx, 0xBC, 0x76);
	truly_dcs_write_seq_static(ctx, 0xBD, 0x00);
	truly_dcs_write_seq_static(ctx, 0xBE, 0x8D);
	truly_dcs_write_seq_static(ctx, 0xBF, 0x00);
	truly_dcs_write_seq_static(ctx, 0xC0, 0xA6);
	truly_dcs_write_seq_static(ctx, 0xC1, 0x00);
	truly_dcs_write_seq_static(ctx, 0xC2, 0xB8);
	truly_dcs_write_seq_static(ctx, 0xC3, 0x00);
	truly_dcs_write_seq_static(ctx, 0xC4, 0xC7);
	truly_dcs_write_seq_static(ctx, 0xC5, 0x00);
	truly_dcs_write_seq_static(ctx, 0xC6, 0xF6);
	truly_dcs_write_seq_static(ctx, 0xC7, 0x01);
	truly_dcs_write_seq_static(ctx, 0xC8, 0x1D);
	truly_dcs_write_seq_static(ctx, 0xC9, 0x01);
	truly_dcs_write_seq_static(ctx, 0xCA, 0x54);
	truly_dcs_write_seq_static(ctx, 0xCB, 0x01);
	truly_dcs_write_seq_static(ctx, 0xCC, 0x81);
	truly_dcs_write_seq_static(ctx, 0xCD, 0x01);
	truly_dcs_write_seq_static(ctx, 0xCE, 0xCB);
	truly_dcs_write_seq_static(ctx, 0xCF, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD0, 0x05);
	truly_dcs_write_seq_static(ctx, 0xD1, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD2, 0x07);
	truly_dcs_write_seq_static(ctx, 0xD3, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD4, 0x47);
	truly_dcs_write_seq_static(ctx, 0xD5, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD6, 0x82);
	truly_dcs_write_seq_static(ctx, 0xD7, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD8, 0xAB);
	truly_dcs_write_seq_static(ctx, 0xD9, 0x02);
	truly_dcs_write_seq_static(ctx, 0xDA, 0xDC);
	truly_dcs_write_seq_static(ctx, 0xDB, 0x03);
	truly_dcs_write_seq_static(ctx, 0xDC, 0x01);
	truly_dcs_write_seq_static(ctx, 0xDD, 0x03);
	truly_dcs_write_seq_static(ctx, 0xDE, 0x3A);
	truly_dcs_write_seq_static(ctx, 0xDF, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE0, 0x56);
	truly_dcs_write_seq_static(ctx, 0xE1, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE2, 0x6D);
	truly_dcs_write_seq_static(ctx, 0xE3, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE4, 0x89);
	truly_dcs_write_seq_static(ctx, 0xE5, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE6, 0xA3);
	truly_dcs_write_seq_static(ctx, 0xE7, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE8, 0xC9);
	truly_dcs_write_seq_static(ctx, 0xE9, 0x03);
	truly_dcs_write_seq_static(ctx, 0xEA, 0xDD);
	truly_dcs_write_seq_static(ctx, 0xEB, 0x03);
	truly_dcs_write_seq_static(ctx, 0xEC, 0xF5);
	truly_dcs_write_seq_static(ctx, 0xED, 0x03);
	truly_dcs_write_seq_static(ctx, 0xEE, 0xFF);
		/*	G+ */
	truly_dcs_write_seq_static(ctx, 0xEF, 0x00);
	truly_dcs_write_seq_static(ctx, 0xF0, 0x00);
	truly_dcs_write_seq_static(ctx, 0xF1, 0x00);
	truly_dcs_write_seq_static(ctx, 0xF2, 0x22);
	truly_dcs_write_seq_static(ctx, 0xF3, 0x00);
	truly_dcs_write_seq_static(ctx, 0xF4, 0x46);
	truly_dcs_write_seq_static(ctx, 0xF5, 0x00);
	truly_dcs_write_seq_static(ctx, 0xF6, 0x5C);
	truly_dcs_write_seq_static(ctx, 0xF7, 0x00);
	truly_dcs_write_seq_static(ctx, 0xF8, 0x76);
	truly_dcs_write_seq_static(ctx, 0xF9, 0x00);
	truly_dcs_write_seq_static(ctx, 0xFA, 0x8D);

	truly_dcs_write_seq_static(ctx, 0xFF, 0x21);
	mdelay(20);

	truly_dcs_write_seq_static(ctx, 0x00, 0x00);
	truly_dcs_write_seq_static(ctx, 0x01, 0xA6);
	truly_dcs_write_seq_static(ctx, 0x02, 0x00);
	truly_dcs_write_seq_static(ctx, 0x03, 0xB8);
	truly_dcs_write_seq_static(ctx, 0x04, 0x00);
	truly_dcs_write_seq_static(ctx, 0x05, 0xC7);
	truly_dcs_write_seq_static(ctx, 0x06, 0x00);
	truly_dcs_write_seq_static(ctx, 0x07, 0xF6);
	truly_dcs_write_seq_static(ctx, 0x08, 0x01);
	truly_dcs_write_seq_static(ctx, 0x09, 0x1D);
	truly_dcs_write_seq_static(ctx, 0x0A, 0x01);
	truly_dcs_write_seq_static(ctx, 0x0B, 0x54);
	truly_dcs_write_seq_static(ctx, 0x0C, 0x01);
	truly_dcs_write_seq_static(ctx, 0x0D, 0x81);
	truly_dcs_write_seq_static(ctx, 0x0E, 0x01);
	truly_dcs_write_seq_static(ctx, 0x0F, 0xCB);
	truly_dcs_write_seq_static(ctx, 0x10, 0x02);
	truly_dcs_write_seq_static(ctx, 0x11, 0x05);
	truly_dcs_write_seq_static(ctx, 0x12, 0x02);
	truly_dcs_write_seq_static(ctx, 0x13, 0x07);
	truly_dcs_write_seq_static(ctx, 0x14, 0x02);
	truly_dcs_write_seq_static(ctx, 0x15, 0x47);
	truly_dcs_write_seq_static(ctx, 0x16, 0x02);
	truly_dcs_write_seq_static(ctx, 0x17, 0x82);
	truly_dcs_write_seq_static(ctx, 0x18, 0x02);
	truly_dcs_write_seq_static(ctx, 0x19, 0xAB);
	truly_dcs_write_seq_static(ctx, 0x1A, 0x02);
	truly_dcs_write_seq_static(ctx, 0x1B, 0xDC);
	truly_dcs_write_seq_static(ctx, 0x1C, 0x03);
	truly_dcs_write_seq_static(ctx, 0x1D, 0x01);
	truly_dcs_write_seq_static(ctx, 0x1E, 0x03);
	truly_dcs_write_seq_static(ctx, 0x1F, 0x3A);
	truly_dcs_write_seq_static(ctx, 0x20, 0x03);
	truly_dcs_write_seq_static(ctx, 0x21, 0x56);
	truly_dcs_write_seq_static(ctx, 0x22, 0x03);
	truly_dcs_write_seq_static(ctx, 0x23, 0x6D);
	truly_dcs_write_seq_static(ctx, 0x24, 0x03);
	truly_dcs_write_seq_static(ctx, 0x25, 0x89);
	truly_dcs_write_seq_static(ctx, 0x26, 0x03);
	truly_dcs_write_seq_static(ctx, 0x27, 0xA3);
	truly_dcs_write_seq_static(ctx, 0x28, 0x03);
	truly_dcs_write_seq_static(ctx, 0x29, 0xC9);
	truly_dcs_write_seq_static(ctx, 0x2A, 0x03);
	truly_dcs_write_seq_static(ctx, 0x2B, 0xDD);
	truly_dcs_write_seq_static(ctx, 0x2D, 0x03);
	truly_dcs_write_seq_static(ctx, 0x2F, 0xF5);
	truly_dcs_write_seq_static(ctx, 0x30, 0x03);
	truly_dcs_write_seq_static(ctx, 0x31, 0xFF);
		/*	G- */
	truly_dcs_write_seq_static(ctx, 0x32, 0x00);
	truly_dcs_write_seq_static(ctx, 0x33, 0x00);
	truly_dcs_write_seq_static(ctx, 0x34, 0x00);
	truly_dcs_write_seq_static(ctx, 0x35, 0x22);
	truly_dcs_write_seq_static(ctx, 0x36, 0x00);
	truly_dcs_write_seq_static(ctx, 0x37, 0x46);
	truly_dcs_write_seq_static(ctx, 0x38, 0x00);
	truly_dcs_write_seq_static(ctx, 0x39, 0x5C);
	truly_dcs_write_seq_static(ctx, 0x3A, 0x00);
	truly_dcs_write_seq_static(ctx, 0x3B, 0x76);
	truly_dcs_write_seq_static(ctx, 0x3D, 0x00);
	truly_dcs_write_seq_static(ctx, 0x3F, 0x8D);
	truly_dcs_write_seq_static(ctx, 0x40, 0x00);
	truly_dcs_write_seq_static(ctx, 0x41, 0xA6);
	truly_dcs_write_seq_static(ctx, 0x42, 0x00);
	truly_dcs_write_seq_static(ctx, 0x43, 0xB8);
	truly_dcs_write_seq_static(ctx, 0x44, 0x00);
	truly_dcs_write_seq_static(ctx, 0x45, 0xC7);
	truly_dcs_write_seq_static(ctx, 0x46, 0x00);
	truly_dcs_write_seq_static(ctx, 0x47, 0xF6);
	truly_dcs_write_seq_static(ctx, 0x48, 0x01);
	truly_dcs_write_seq_static(ctx, 0x49, 0x1D);
	truly_dcs_write_seq_static(ctx, 0x4A, 0x01);
	truly_dcs_write_seq_static(ctx, 0x4B, 0x54);
	truly_dcs_write_seq_static(ctx, 0x4C, 0x01);
	truly_dcs_write_seq_static(ctx, 0x4D, 0x81);
	truly_dcs_write_seq_static(ctx, 0x4E, 0x01);
	truly_dcs_write_seq_static(ctx, 0x4F, 0xCB);
	truly_dcs_write_seq_static(ctx, 0x50, 0x02);
	truly_dcs_write_seq_static(ctx, 0x51, 0x05);
	truly_dcs_write_seq_static(ctx, 0x52, 0x02);
	truly_dcs_write_seq_static(ctx, 0x53, 0x07);
	truly_dcs_write_seq_static(ctx, 0x54, 0x02);
	truly_dcs_write_seq_static(ctx, 0x55, 0x47);
	truly_dcs_write_seq_static(ctx, 0x56, 0x02);
	truly_dcs_write_seq_static(ctx, 0x58, 0x82);
	truly_dcs_write_seq_static(ctx, 0x59, 0x02);
	truly_dcs_write_seq_static(ctx, 0x5A, 0xAB);
	truly_dcs_write_seq_static(ctx, 0x5B, 0x02);
	truly_dcs_write_seq_static(ctx, 0x5C, 0xDC);
	truly_dcs_write_seq_static(ctx, 0x5D, 0x03);
	truly_dcs_write_seq_static(ctx, 0x5E, 0x01);
	truly_dcs_write_seq_static(ctx, 0x5F, 0x03);
	truly_dcs_write_seq_static(ctx, 0x60, 0x3A);
	truly_dcs_write_seq_static(ctx, 0x61, 0x03);
	truly_dcs_write_seq_static(ctx, 0x62, 0x56);
	truly_dcs_write_seq_static(ctx, 0x63, 0x03);
	truly_dcs_write_seq_static(ctx, 0x64, 0x6D);
	truly_dcs_write_seq_static(ctx, 0x65, 0x03);
	truly_dcs_write_seq_static(ctx, 0x66, 0x89);
	truly_dcs_write_seq_static(ctx, 0x67, 0x03);
	truly_dcs_write_seq_static(ctx, 0x68, 0xA3);
	truly_dcs_write_seq_static(ctx, 0x69, 0x03);
	truly_dcs_write_seq_static(ctx, 0x6A, 0xC9);
	truly_dcs_write_seq_static(ctx, 0x6B, 0x03);
	truly_dcs_write_seq_static(ctx, 0x6C, 0xDD);
	truly_dcs_write_seq_static(ctx, 0x6D, 0x03);
	truly_dcs_write_seq_static(ctx, 0x6E, 0xF5);
	truly_dcs_write_seq_static(ctx, 0x6F, 0x03);
	truly_dcs_write_seq_static(ctx, 0x70, 0xFF);
		/*	B+ */
	truly_dcs_write_seq_static(ctx, 0x71, 0x00);
	truly_dcs_write_seq_static(ctx, 0x72, 0x00);
	truly_dcs_write_seq_static(ctx, 0x73, 0x00);
	truly_dcs_write_seq_static(ctx, 0x74, 0x22);
	truly_dcs_write_seq_static(ctx, 0x75, 0x00);
	truly_dcs_write_seq_static(ctx, 0x76, 0x46);
	truly_dcs_write_seq_static(ctx, 0x77, 0x00);
	truly_dcs_write_seq_static(ctx, 0x78, 0x5C);
	truly_dcs_write_seq_static(ctx, 0x79, 0x00);
	truly_dcs_write_seq_static(ctx, 0x7A, 0x76);
	truly_dcs_write_seq_static(ctx, 0x7B, 0x00);
	truly_dcs_write_seq_static(ctx, 0x7C, 0x8D);
	truly_dcs_write_seq_static(ctx, 0x7D, 0x00);
	truly_dcs_write_seq_static(ctx, 0x7E, 0xA6);
	truly_dcs_write_seq_static(ctx, 0x7F, 0x00);
	truly_dcs_write_seq_static(ctx, 0x80, 0xB8);
	truly_dcs_write_seq_static(ctx, 0x81, 0x00);
	truly_dcs_write_seq_static(ctx, 0x82, 0xC7);
	truly_dcs_write_seq_static(ctx, 0x83, 0x00);
	truly_dcs_write_seq_static(ctx, 0x84, 0xF6);
	truly_dcs_write_seq_static(ctx, 0x85, 0x01);
	truly_dcs_write_seq_static(ctx, 0x86, 0x1D);
	truly_dcs_write_seq_static(ctx, 0x87, 0x01);
	truly_dcs_write_seq_static(ctx, 0x88, 0x54);
	truly_dcs_write_seq_static(ctx, 0x89, 0x01);
	truly_dcs_write_seq_static(ctx, 0x8A, 0x81);
	truly_dcs_write_seq_static(ctx, 0x8B, 0x01);
	truly_dcs_write_seq_static(ctx, 0x8C, 0xCB);
	truly_dcs_write_seq_static(ctx, 0x8D, 0x02);
	truly_dcs_write_seq_static(ctx, 0x8E, 0x05);
	truly_dcs_write_seq_static(ctx, 0x8F, 0x02);
	truly_dcs_write_seq_static(ctx, 0x90, 0x07);
	truly_dcs_write_seq_static(ctx, 0x91, 0x02);
	truly_dcs_write_seq_static(ctx, 0x92, 0x47);
	truly_dcs_write_seq_static(ctx, 0x93, 0x02);
	truly_dcs_write_seq_static(ctx, 0x94, 0x82);
	truly_dcs_write_seq_static(ctx, 0x95, 0x02);
	truly_dcs_write_seq_static(ctx, 0x96, 0xAB);
	truly_dcs_write_seq_static(ctx, 0x97, 0x02);
	truly_dcs_write_seq_static(ctx, 0x98, 0xDC);
	truly_dcs_write_seq_static(ctx, 0x99, 0x03);
	truly_dcs_write_seq_static(ctx, 0x9A, 0x01);
	truly_dcs_write_seq_static(ctx, 0x9B, 0x03);
	truly_dcs_write_seq_static(ctx, 0x9C, 0x3A);
	truly_dcs_write_seq_static(ctx, 0x9D, 0x03);
	truly_dcs_write_seq_static(ctx, 0x9E, 0x56);
	truly_dcs_write_seq_static(ctx, 0x9F, 0x03);
	truly_dcs_write_seq_static(ctx, 0xA0, 0x6D);
	truly_dcs_write_seq_static(ctx, 0xA2, 0x03);
	truly_dcs_write_seq_static(ctx, 0xA3, 0x89);
	truly_dcs_write_seq_static(ctx, 0xA4, 0x03);
	truly_dcs_write_seq_static(ctx, 0xA5, 0xA3);
	truly_dcs_write_seq_static(ctx, 0xA6, 0x03);
	truly_dcs_write_seq_static(ctx, 0xA7, 0xC9);
	truly_dcs_write_seq_static(ctx, 0xA9, 0x03);
	truly_dcs_write_seq_static(ctx, 0xAA, 0xDD);
	truly_dcs_write_seq_static(ctx, 0xAB, 0x03);
	truly_dcs_write_seq_static(ctx, 0xAC, 0xF5);
	truly_dcs_write_seq_static(ctx, 0xAD, 0x03);
	truly_dcs_write_seq_static(ctx, 0xAE, 0xFF);
		/*	B- */
	truly_dcs_write_seq_static(ctx, 0xAF, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB0, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB1, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB2, 0x22);
	truly_dcs_write_seq_static(ctx, 0xB3, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB4, 0x46);
	truly_dcs_write_seq_static(ctx, 0xB5, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB6, 0x5C);
	truly_dcs_write_seq_static(ctx, 0xB7, 0x00);
	truly_dcs_write_seq_static(ctx, 0xB8, 0x76);
	truly_dcs_write_seq_static(ctx, 0xB9, 0x00);
	truly_dcs_write_seq_static(ctx, 0xBA, 0x8D);
	truly_dcs_write_seq_static(ctx, 0xBB, 0x00);
	truly_dcs_write_seq_static(ctx, 0xBC, 0xA6);
	truly_dcs_write_seq_static(ctx, 0xBD, 0x00);
	truly_dcs_write_seq_static(ctx, 0xBE, 0xB8);
	truly_dcs_write_seq_static(ctx, 0xBF, 0x00);
	truly_dcs_write_seq_static(ctx, 0xC0, 0xC7);
	truly_dcs_write_seq_static(ctx, 0xC1, 0x00);
	truly_dcs_write_seq_static(ctx, 0xC2, 0xF6);
	truly_dcs_write_seq_static(ctx, 0xC3, 0x01);
	truly_dcs_write_seq_static(ctx, 0xC4, 0x1D);
	truly_dcs_write_seq_static(ctx, 0xC5, 0x01);
	truly_dcs_write_seq_static(ctx, 0xC6, 0x54);
	truly_dcs_write_seq_static(ctx, 0xC7, 0x01);
	truly_dcs_write_seq_static(ctx, 0xC8, 0x81);
	truly_dcs_write_seq_static(ctx, 0xC9, 0x01);
	truly_dcs_write_seq_static(ctx, 0xCA, 0xCB);
	truly_dcs_write_seq_static(ctx, 0xCB, 0x02);
	truly_dcs_write_seq_static(ctx, 0xCC, 0x05);
	truly_dcs_write_seq_static(ctx, 0xCD, 0x02);
	truly_dcs_write_seq_static(ctx, 0xCE, 0x07);
	truly_dcs_write_seq_static(ctx, 0xCF, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD0, 0x47);
	truly_dcs_write_seq_static(ctx, 0xD1, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD2, 0x82);
	truly_dcs_write_seq_static(ctx, 0xD3, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD4, 0xAB);
	truly_dcs_write_seq_static(ctx, 0xD5, 0x02);
	truly_dcs_write_seq_static(ctx, 0xD6, 0xDC);
	truly_dcs_write_seq_static(ctx, 0xD7, 0x03);
	truly_dcs_write_seq_static(ctx, 0xD8, 0x01);
	truly_dcs_write_seq_static(ctx, 0xD9, 0x03);
	truly_dcs_write_seq_static(ctx, 0xDA, 0x3A);
	truly_dcs_write_seq_static(ctx, 0xDB, 0x03);
	truly_dcs_write_seq_static(ctx, 0xDC, 0x56);
	truly_dcs_write_seq_static(ctx, 0xDD, 0x03);
	truly_dcs_write_seq_static(ctx, 0xDE, 0x6D);
	truly_dcs_write_seq_static(ctx, 0xDF, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE0, 0x89);
	truly_dcs_write_seq_static(ctx, 0xE1, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE2, 0xA3);
	truly_dcs_write_seq_static(ctx, 0xE3, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE4, 0xC9);
	truly_dcs_write_seq_static(ctx, 0xE5, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE6, 0xDD);
	truly_dcs_write_seq_static(ctx, 0xE7, 0x03);
	truly_dcs_write_seq_static(ctx, 0xE8, 0xF5);
	truly_dcs_write_seq_static(ctx, 0xE9, 0x03);
	truly_dcs_write_seq_static(ctx, 0xEA, 0xFF);

	truly_dcs_write_seq_static(ctx, 0xFF, 0x21);
	mdelay(2);

	truly_dcs_write_seq_static(ctx, 0xEB, 0x30);
	truly_dcs_write_seq_static(ctx, 0xEC, 0x17);
	truly_dcs_write_seq_static(ctx, 0xED, 0x20);
	truly_dcs_write_seq_static(ctx, 0xEE, 0x0F);
	truly_dcs_write_seq_static(ctx, 0xEF, 0x1F);
	truly_dcs_write_seq_static(ctx, 0xF0, 0x0F);
	truly_dcs_write_seq_static(ctx, 0xF1, 0x0F);
	truly_dcs_write_seq_static(ctx, 0xF2, 0x07);
	truly_dcs_write_seq_static(ctx, 0xFF, 0x23);
	mdelay(2);

	truly_dcs_write_seq_static(ctx, 0x08, 0x04);
	truly_dcs_write_seq_static(ctx, 0xFF, 0x10);
	mdelay(2);

#ifdef PANEL_BIST_PATTERN
	truly_dcs_write_seq_static(ctx, 0xFF, 0x24);
	mdelay(1);

	truly_dcs_write_seq_static(ctx, 0xEC, 0x01);
#else
	truly_dcs_write_seq_static(ctx, 0x35, 0x00);
	truly_dcs_write_seq_static(ctx, 0x29);
	mdelay(20);
#endif
}

static void truly_set_sequence(struct truly *ctx)
{
	truly_panel_init(ctx);
}

static int truly_power_on(struct truly *ctx)
{
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(100);

	return 0;
}

static int truly_power_off(struct truly *ctx)
{
	gpiod_set_value(ctx->reset_gpio, 0);
	return 0;
}

static int truly_disable(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);
	int ret = 0;

	if (!ctx->enabled)
		return 0;

	ret = truly_power_off(ctx);

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return ret;
}

static int truly_unprepare(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);

	if (!ctx->prepared)
		return 0;

	truly_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	truly_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	truly_clear_error(ctx);

	ctx->prepared = false;

	return 0;
}

static int truly_prepare(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);
	int ret = 0;

	if (ctx->prepared)
		return ret;

	if (ret < 0)
		return ret;

	truly_set_sequence(ctx);

	ret = ctx->error;
	if (ret < 0)
		truly_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	truly_panel_get_data(ctx);
#endif

	return ret;
}

static int truly_enable(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);
	int ret = 0;

	if (ctx->enabled)
		return 0;

	ret = truly_power_on(ctx);

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return ret;
}

static struct drm_display_mode default_mode = {
	.clock = 142858,
	.hdisplay = 1080,
	.hsync_start = 1080 + 40,
	.hsync_end = 1080 + 10 + 40,
	.htotal = 1080 + 10 + 20 + 40,
	.vdisplay = 1920,
	.vsync_start = 1920 + 10,
	.vsync_end = 1920 + 2 + 10,
	.vtotal = 1920 + 2 + 8 + 10,
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

static const struct panel_desc truly_panel = {
	.modes = &default_mode,
	.num_modes = 1,
	.bpc = 6,
	.size = {
		.width = 1080,
		.height = 1920,
	},
};

static int truly_get_modes(struct drm_panel *panel)
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
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = 75;
	panel->connector->display_info.height_mm = 133;

	return 1;
}

static const struct drm_panel_funcs truly_drm_funcs = {
	.disable = truly_disable,
	.unprepare = truly_unprepare,
	.prepare = truly_prepare,
	.enable = truly_enable,
	.get_modes = truly_get_modes,
};

static int truly_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct truly *ctx;
	struct device_node *backlight;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(struct truly), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			  | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			  | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->supplies[0].supply = "avdd";
	ctx->supplies[1].supply = "avee";
	ctx->supplies[1].supply = "iovcc";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		dev_err(dev, "failed to get regulators: %d\n", ret);

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset");
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	ret = gpiod_direction_output(ctx->reset_gpio, 1);
	if (ret < 0) {
		dev_err(dev, "cannot configure reset-gpios %d\n", ret);
		return ret;
	}

	ctx->prepared = false;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &truly_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	return ret;
}

static int truly_remove(struct mipi_dsi_device *dsi)
{
	struct truly *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id truly_of_match[] = {
	{ .compatible = "truly,bd0598u4004", },
	{ }
};

MODULE_DEVICE_TABLE(of, truly_of_match);

static struct mipi_dsi_driver truly_driver = {
	.probe = truly_probe,
	.remove = truly_remove,
	.driver = {
		.name = "panel-bd0598u4004",
		.owner = THIS_MODULE,
		.of_match_table = truly_of_match,
	},
};

module_mipi_dsi_driver(truly_driver);

MODULE_AUTHOR("Jitao Shi <jitao.shi@mediatek.com>");
MODULE_AUTHOR("Shaoming Chen <shaoming.chen@mediatek.com>");
MODULE_DESCRIPTION("TRULY TDO-BD0598U40004 LCD Panel Driver");
MODULE_LICENSE("GPL v2");
