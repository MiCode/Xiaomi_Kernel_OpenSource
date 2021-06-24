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

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>

#include <linux/gpio/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/of_graph.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_panel_ext.h"
#endif
/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

#define AVDD_REG 0x00
#define AVDD_REG 0x01

/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;
static char bl_tb0[] = { 0x51, 0xff };

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	    .compatible = "mediatek,I2C_LCD_BIAS",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect		   = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

/*****************************************************************************
 * Function
 *****************************************************************************/

#ifdef VENDOR_EDIT
// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
extern void lcd_queue_load_tp_fw(void);
#endif /*VENDOR_EDIT*/

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	pr_debug("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,
		 client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);
/***********************************/

struct jdi {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

#define jdi_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		jdi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define jdi_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		jdi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct jdi *panel_to_jdi(struct drm_panel *panel)
{
	return container_of(panel, struct jdi, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int jdi_dcs_read(struct jdi *ctx, u8 cmd, void *data, size_t len)
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

static void jdi_panel_get_data(struct jdi *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = jdi_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void jdi_dcs_write(struct jdi *ctx, const void *data, size_t len)
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

static void jdi_panel_init(struct jdi *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	//set 120hz
	jdi_dcs_write_seq_static(ctx, 0xFF, 0x25);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x18, 0x22);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x10);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0xB0, 0x00);
	//DSC ON && set PPS
	jdi_dcs_write_seq_static(ctx, 0xC0, 0x03);
	jdi_dcs_write_seq_static(ctx, 0xC1, 0x89, 0x28, 0x00, 0x08, 0x00, 0xAA,
				0x02, 0x0E, 0x00, 0x2B, 0x00, 0x07, 0x0D, 0xB7,
				0x0C, 0xB7);
	jdi_dcs_write_seq_static(ctx, 0xC2, 0x1B, 0XA0);

	// CCMON
	jdi_dcs_write_seq_static(ctx, 0xFF, 0x20);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x01, 0x66);
	jdi_dcs_write_seq_static(ctx, 0x06, 0x40);
	jdi_dcs_write_seq_static(ctx, 0x07, 0x38);
	jdi_dcs_write_seq_static(ctx, 0x69, 0x91);
	//jdi_dcs_write_seq_static(ctx, 0x89, 0x17);
	jdi_dcs_write_seq_static(ctx, 0x95, 0xD1);
	jdi_dcs_write_seq_static(ctx, 0x96, 0xD1);
	jdi_dcs_write_seq_static(ctx, 0xF2, 0x64);
	jdi_dcs_write_seq_static(ctx, 0xF4, 0x64);
	jdi_dcs_write_seq_static(ctx, 0xF6, 0x64);
	jdi_dcs_write_seq_static(ctx, 0xF8, 0x64);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x24);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x01, 0x0F);
	jdi_dcs_write_seq_static(ctx, 0x03, 0x0C);
	jdi_dcs_write_seq_static(ctx, 0x05, 0x1D);
	jdi_dcs_write_seq_static(ctx, 0x08, 0x2F);
	jdi_dcs_write_seq_static(ctx, 0x09, 0x2E);
	jdi_dcs_write_seq_static(ctx, 0x0A, 0x2D);
	jdi_dcs_write_seq_static(ctx, 0x0B, 0x2C);
	jdi_dcs_write_seq_static(ctx, 0x11, 0x17);
	jdi_dcs_write_seq_static(ctx, 0x12, 0x13);
	jdi_dcs_write_seq_static(ctx, 0x13, 0x15);
	jdi_dcs_write_seq_static(ctx, 0x15, 0x14);
	jdi_dcs_write_seq_static(ctx, 0x16, 0x16);
	jdi_dcs_write_seq_static(ctx, 0x17, 0x18);
	jdi_dcs_write_seq_static(ctx, 0x1B, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x1D, 0x1D);
	jdi_dcs_write_seq_static(ctx, 0x20, 0x2F);
	jdi_dcs_write_seq_static(ctx, 0x21, 0x2E);
	jdi_dcs_write_seq_static(ctx, 0x22, 0x2D);
	jdi_dcs_write_seq_static(ctx, 0x23, 0x2C);
	jdi_dcs_write_seq_static(ctx, 0x29, 0x17);
	jdi_dcs_write_seq_static(ctx, 0x2A, 0x13);
	jdi_dcs_write_seq_static(ctx, 0x2B, 0x15);
	jdi_dcs_write_seq_static(ctx, 0x2F, 0x14);
	jdi_dcs_write_seq_static(ctx, 0x30, 0x16);
	jdi_dcs_write_seq_static(ctx, 0x31, 0x18);
	jdi_dcs_write_seq_static(ctx, 0x32, 0x04);
	jdi_dcs_write_seq_static(ctx, 0x34, 0x10);
	jdi_dcs_write_seq_static(ctx, 0x35, 0x1F);
	jdi_dcs_write_seq_static(ctx, 0x36, 0x1F);
	jdi_dcs_write_seq_static(ctx, 0x37, 0x20);
	jdi_dcs_write_seq_static(ctx, 0x4D, 0x1B);
	jdi_dcs_write_seq_static(ctx, 0x4E, 0x4B);
	jdi_dcs_write_seq_static(ctx, 0x4F, 0x4B);
	jdi_dcs_write_seq_static(ctx, 0x53, 0x4B);
	jdi_dcs_write_seq_static(ctx, 0x71, 0x30);
	jdi_dcs_write_seq_static(ctx, 0x79, 0x11);
	jdi_dcs_write_seq_static(ctx, 0x7A, 0x82);
	jdi_dcs_write_seq_static(ctx, 0x7B, 0x96);
	jdi_dcs_write_seq_static(ctx, 0x7D, 0x04);
	jdi_dcs_write_seq_static(ctx, 0x80, 0x04);
	jdi_dcs_write_seq_static(ctx, 0x81, 0x04);
	jdi_dcs_write_seq_static(ctx, 0x82, 0x13);
	jdi_dcs_write_seq_static(ctx, 0x84, 0x31);
	jdi_dcs_write_seq_static(ctx, 0x85, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x86, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x87, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x90, 0x13);
	jdi_dcs_write_seq_static(ctx, 0x92, 0x31);
	jdi_dcs_write_seq_static(ctx, 0x93, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x94, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x95, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x9C, 0xF4);
	jdi_dcs_write_seq_static(ctx, 0x9D, 0x01);
	jdi_dcs_write_seq_static(ctx, 0xA0, 0x16);
	jdi_dcs_write_seq_static(ctx, 0xA2, 0x16);
	jdi_dcs_write_seq_static(ctx, 0xA3, 0x02);
	jdi_dcs_write_seq_static(ctx, 0xA4, 0x04);
	jdi_dcs_write_seq_static(ctx, 0xA5, 0x04);
	jdi_dcs_write_seq_static(ctx, 0xC9, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD9, 0x80);
	jdi_dcs_write_seq_static(ctx, 0xE9, 0x02);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x25);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x19, 0xE4);
	jdi_dcs_write_seq_static(ctx, 0x21, 0x40);
	jdi_dcs_write_seq_static(ctx, 0x66, 0xD8);
	jdi_dcs_write_seq_static(ctx, 0x68, 0x50);
	jdi_dcs_write_seq_static(ctx, 0x69, 0x10);
	jdi_dcs_write_seq_static(ctx, 0x6B, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x6D, 0x0D);
	jdi_dcs_write_seq_static(ctx, 0x6E, 0x48);
	jdi_dcs_write_seq_static(ctx, 0x72, 0x41);
	jdi_dcs_write_seq_static(ctx, 0x73, 0x4A);
	jdi_dcs_write_seq_static(ctx, 0x74, 0xD0);
	//jdi_dcs_write_seq_static(ctx, 0x76, 0x83);
	jdi_dcs_write_seq_static(ctx, 0x77, 0x62);
	jdi_dcs_write_seq_static(ctx, 0x79, 0x81);//add
	jdi_dcs_write_seq_static(ctx, 0x7D, 0x03);
	jdi_dcs_write_seq_static(ctx, 0x7E, 0x15);
	jdi_dcs_write_seq_static(ctx, 0x7F, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x84, 0x4D);
	jdi_dcs_write_seq_static(ctx, 0xCF, 0x80);
	jdi_dcs_write_seq_static(ctx, 0xD6, 0x80);
	jdi_dcs_write_seq_static(ctx, 0xD7, 0x80);
	jdi_dcs_write_seq_static(ctx, 0xEF, 0x20);
	jdi_dcs_write_seq_static(ctx, 0xF0, 0x84);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x26);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x80, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x81, 0x16);
	jdi_dcs_write_seq_static(ctx, 0x83, 0x03);
	jdi_dcs_write_seq_static(ctx, 0x84, 0x03);
	jdi_dcs_write_seq_static(ctx, 0x85, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x86, 0x03);
	jdi_dcs_write_seq_static(ctx, 0x87, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x8A, 0x1A);
	jdi_dcs_write_seq_static(ctx, 0x8B, 0x11);
	jdi_dcs_write_seq_static(ctx, 0x8C, 0x24);
	jdi_dcs_write_seq_static(ctx, 0x8E, 0x42);
	jdi_dcs_write_seq_static(ctx, 0x8F, 0x11);
	jdi_dcs_write_seq_static(ctx, 0x90, 0x11);
	jdi_dcs_write_seq_static(ctx, 0x91, 0x11);
	jdi_dcs_write_seq_static(ctx, 0x9A, 0x81);
	jdi_dcs_write_seq_static(ctx, 0x9B, 0x03);
	jdi_dcs_write_seq_static(ctx, 0x9C, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x9D, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x9E, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x27);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x01, 0x60);
	jdi_dcs_write_seq_static(ctx, 0x20, 0x81);
	jdi_dcs_write_seq_static(ctx, 0x21, 0xEA);
	jdi_dcs_write_seq_static(ctx, 0x25, 0x82);
	jdi_dcs_write_seq_static(ctx, 0x26, 0x1F);
	jdi_dcs_write_seq_static(ctx, 0x6E, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x6F, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x70, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x71, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x72, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x75, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x76, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x77, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x7D, 0x09);
	jdi_dcs_write_seq_static(ctx, 0x7E, 0x5F);
	jdi_dcs_write_seq_static(ctx, 0x80, 0x23);
	jdi_dcs_write_seq_static(ctx, 0x82, 0x09);
	jdi_dcs_write_seq_static(ctx, 0x83, 0x5F);
	jdi_dcs_write_seq_static(ctx, 0x88, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x89, 0x10);
	jdi_dcs_write_seq_static(ctx, 0xA5, 0x10);
	jdi_dcs_write_seq_static(ctx, 0xA6, 0x23);
	jdi_dcs_write_seq_static(ctx, 0xA7, 0x01);
	jdi_dcs_write_seq_static(ctx, 0xB6, 0x40);
	jdi_dcs_write_seq_static(ctx, 0xE3, 0x02);
	jdi_dcs_write_seq_static(ctx, 0xE4, 0xE0);
	jdi_dcs_write_seq_static(ctx, 0xE5, 0x01);//add
	jdi_dcs_write_seq_static(ctx, 0xE6, 0x70);//add
	jdi_dcs_write_seq_static(ctx, 0xE9, 0x03);
	jdi_dcs_write_seq_static(ctx, 0xEA, 0x2F);
	jdi_dcs_write_seq_static(ctx, 0xEB, 0x01);//add
	jdi_dcs_write_seq_static(ctx, 0xEC, 0x98);//add

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x2A);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x00, 0x91);
	jdi_dcs_write_seq_static(ctx, 0x03, 0x20);
	jdi_dcs_write_seq_static(ctx, 0x07, 0x52);// modify
	jdi_dcs_write_seq_static(ctx, 0x0A, 0x60);
	jdi_dcs_write_seq_static(ctx, 0x0C, 0x06);
	jdi_dcs_write_seq_static(ctx, 0x0D, 0x40);
	jdi_dcs_write_seq_static(ctx, 0x0E, 0x02);
	jdi_dcs_write_seq_static(ctx, 0x0F, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x11, 0x58);
	jdi_dcs_write_seq_static(ctx, 0x15, 0x0E);
	jdi_dcs_write_seq_static(ctx, 0x16, 0x79);
	jdi_dcs_write_seq_static(ctx, 0x19, 0x0D);
	jdi_dcs_write_seq_static(ctx, 0x1A, 0xF2);
	jdi_dcs_write_seq_static(ctx, 0x1B, 0x14);
	jdi_dcs_write_seq_static(ctx, 0x1D, 0x36);
	jdi_dcs_write_seq_static(ctx, 0x1E, 0x55);
	jdi_dcs_write_seq_static(ctx, 0x1F, 0x55);
	jdi_dcs_write_seq_static(ctx, 0x20, 0x55);
	//jdi_dcs_write_seq_static(ctx, 0x27, 0x80);
	jdi_dcs_write_seq_static(ctx, 0x28, 0x0A);
	jdi_dcs_write_seq_static(ctx, 0x29, 0x0B);
	jdi_dcs_write_seq_static(ctx, 0x2A, 0x4B);//modify
	jdi_dcs_write_seq_static(ctx, 0x2B, 0x05);//modify
	jdi_dcs_write_seq_static(ctx, 0x2D, 0x08);//add
	jdi_dcs_write_seq_static(ctx, 0x2F, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x30, 0x47);
	jdi_dcs_write_seq_static(ctx, 0x31, 0x23);//modify
	jdi_dcs_write_seq_static(ctx, 0x33, 0x25);//modify
	jdi_dcs_write_seq_static(ctx, 0x34, 0xFF);
	jdi_dcs_write_seq_static(ctx, 0x35, 0x2C);//modify
	jdi_dcs_write_seq_static(ctx, 0x36, 0x75);//modify
	jdi_dcs_write_seq_static(ctx, 0x37, 0xFB);//modify
	jdi_dcs_write_seq_static(ctx, 0x38, 0x2E);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x39, 0x73);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x3A, 0x47);
	//jdi_dcs_write_seq_static(ctx, 0x44, 0x4C);
	//jdi_dcs_write_seq_static(ctx, 0x45, 0x09);
	jdi_dcs_write_seq_static(ctx, 0x46, 0x40);
	jdi_dcs_write_seq_static(ctx, 0x47, 0x02);
	//jdi_dcs_write_seq_static(ctx, 0x48, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x4A, 0xF0);
	jdi_dcs_write_seq_static(ctx, 0x4E, 0x0E);
	jdi_dcs_write_seq_static(ctx, 0x4F, 0x8B);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x52, 0x0E);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x53, 0x04);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x54, 0x14);
	jdi_dcs_write_seq_static(ctx, 0x56, 0x36);
	jdi_dcs_write_seq_static(ctx, 0x57, 0x80);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x58, 0x80);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x59, 0x80);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x60, 0x80);
	jdi_dcs_write_seq_static(ctx, 0x61, 0x0A);
	jdi_dcs_write_seq_static(ctx, 0x62, 0x03);
	jdi_dcs_write_seq_static(ctx, 0x63, 0xED);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x65, 0x05);//ADD
	jdi_dcs_write_seq_static(ctx, 0x66, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x67, 0x04);
	jdi_dcs_write_seq_static(ctx, 0x68, 0x4D);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x6A, 0x0A);//ADD
	jdi_dcs_write_seq_static(ctx, 0x6B, 0xC9);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x6C, 0x1F);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x6D, 0xE3);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x6E, 0xC6);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x6F, 0x20);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x70, 0xE2);//MODIFY
	jdi_dcs_write_seq_static(ctx, 0x71, 0x04);
	jdi_dcs_write_seq_static(ctx, 0x7A, 0X04);//ADD
	jdi_dcs_write_seq_static(ctx, 0X7B, 0X04);//ADD
	jdi_dcs_write_seq_static(ctx, 0X7C, 0x01);//ADD
	jdi_dcs_write_seq_static(ctx, 0X7D, 0x01);//ADD
	jdi_dcs_write_seq_static(ctx, 0X7F, 0XE0);//ADD
	jdi_dcs_write_seq_static(ctx, 0X83, 0X0E);//ADD
	jdi_dcs_write_seq_static(ctx, 0X84, 0X8B);//ADD
	jdi_dcs_write_seq_static(ctx, 0X87, 0X0E);//ADD
	jdi_dcs_write_seq_static(ctx, 0X88, 0X04);//ADD
	jdi_dcs_write_seq_static(ctx, 0X89, 0X14);//ADD
	jdi_dcs_write_seq_static(ctx, 0X8B, 0X36);//ADD
	jdi_dcs_write_seq_static(ctx, 0X8C, 0X40);//ADD
	jdi_dcs_write_seq_static(ctx, 0X8D, 0X40);//ADD
	jdi_dcs_write_seq_static(ctx, 0X8E, 0X40);//ADD
	jdi_dcs_write_seq_static(ctx, 0X95, 0X80);//ADD
	jdi_dcs_write_seq_static(ctx, 0X96, 0X0A);//ADD
	jdi_dcs_write_seq_static(ctx, 0X97, 0X12);//ADD
	jdi_dcs_write_seq_static(ctx, 0X98, 0X92);//ADD
	jdi_dcs_write_seq_static(ctx, 0X9A, 0X0A);//ADD
	jdi_dcs_write_seq_static(ctx, 0X9B, 0X02);//ADD
	jdi_dcs_write_seq_static(ctx, 0X9C, 0X49);//ADD
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X98);//ADD
	jdi_dcs_write_seq_static(ctx, 0X9F, 0X5F);//ADD
	jdi_dcs_write_seq_static(ctx, 0XA0, 0XFF);//ADD
	jdi_dcs_write_seq_static(ctx, 0XA2, 0X3A);//ADD
	jdi_dcs_write_seq_static(ctx, 0XA3, 0XD9);//ADD
	jdi_dcs_write_seq_static(ctx, 0XA4, 0XFA);//ADD
	jdi_dcs_write_seq_static(ctx, 0XA5, 0X3C);//ADD
	jdi_dcs_write_seq_static(ctx, 0XA6, 0XD7);//ADD
	jdi_dcs_write_seq_static(ctx, 0XA7, 0X49);//ADD

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x2C);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x00, 0x02);
	jdi_dcs_write_seq_static(ctx, 0x01, 0x02);
	jdi_dcs_write_seq_static(ctx, 0x02, 0x02);
	jdi_dcs_write_seq_static(ctx, 0x03, 0x16);
	jdi_dcs_write_seq_static(ctx, 0x04, 0x16);
	jdi_dcs_write_seq_static(ctx, 0x05, 0x16);
	jdi_dcs_write_seq_static(ctx, 0x0D, 0x1F);
	jdi_dcs_write_seq_static(ctx, 0x0E, 0x1F);
	jdi_dcs_write_seq_static(ctx, 0x16, 0x1B);
	jdi_dcs_write_seq_static(ctx, 0x17, 0x4B);
	jdi_dcs_write_seq_static(ctx, 0x18, 0x4B);
	jdi_dcs_write_seq_static(ctx, 0x19, 0x4B);
	jdi_dcs_write_seq_static(ctx, 0x2A, 0x03);
	//jdi_dcs_write_seq_static(ctx, 0x3B, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x4D, 0x16);
	jdi_dcs_write_seq_static(ctx, 0x4E, 0x03);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X02);//ADD
	jdi_dcs_write_seq_static(ctx, 0X54, 0X02);//ADD
	jdi_dcs_write_seq_static(ctx, 0X55, 0X02);//ADD
	jdi_dcs_write_seq_static(ctx, 0X56, 0X0F);//ADD
	jdi_dcs_write_seq_static(ctx, 0X58, 0X0F);//ADD
	jdi_dcs_write_seq_static(ctx, 0X59, 0X0F);//ADD
	jdi_dcs_write_seq_static(ctx, 0X61, 0X1F);//ADD
	jdi_dcs_write_seq_static(ctx, 0X62, 0X1F);//ADD
	jdi_dcs_write_seq_static(ctx, 0X6A, 0X15);//ADD
	jdi_dcs_write_seq_static(ctx, 0X6B, 0X37);//ADD
	jdi_dcs_write_seq_static(ctx, 0X6C, 0X37);//ADD
	jdi_dcs_write_seq_static(ctx, 0X6D, 0X37);//ADD
	jdi_dcs_write_seq_static(ctx, 0X7E, 0X03);//ADD
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X10);//ADD
	jdi_dcs_write_seq_static(ctx, 0X9E, 0X03);//ADD

	jdi_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x5A, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0xD0);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);

	// CCMOFF
	// CCMRUN
	jdi_dcs_write_seq_static(ctx, 0xFF, 0x10);
	// add for pwm,by zsq
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x35, 0x00);
	jdi_dcs_write_seq_static(ctx, 0x53, 0x24);
	jdi_dcs_write_seq_static(ctx, 0x55, 0x00);
	// end by zsq
	jdi_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	/* Display On*/
	jdi_dcs_write_seq_static(ctx, 0x29);
	jdi_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1]);

	pr_info("%s-\n", __func__);
}

static int jdi_disable(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int jdi_unprepare(struct drm_panel *panel)
{

	struct jdi *ctx = panel_to_jdi(panel);

	pr_info("%s\n", __func__);

	if (!ctx->prepared)
		return 0;

	jdi_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	jdi_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(200);
	/*
	 * ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	 * if (IS_ERR(ctx->reset_gpio)) {
	 *	dev_err(ctx->dev, "%s: cannot get reset-gpios %ld\n",
	 *		__func__, PTR_ERR(ctx->reset_gpio));
	 *	return PTR_ERR(ctx->reset_gpio);
	 * }
	 * gpiod_set_value(ctx->reset_gpio, 0);
	 * devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	 */
	ctx->bias_neg =
	    devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	usleep_range(2000, 2001);

	ctx->bias_pos =
	    devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int jdi_prepare(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end
	ctx->bias_pos =
	    devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	usleep_range(2000, 2001);
	ctx->bias_neg =
	    devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(0x0, 0xf);
	_lcm_i2c_write_bytes(0x1, 0xf);
	jdi_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		jdi_unprepare(panel);

	ctx->prepared = true;
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	jdi_panel_get_data(ctx);
#endif

#ifdef VENDOR_EDIT
	// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
	lcd_queue_load_tp_fw();
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int jdi_enable(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

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
	.clock = 422163,
	.hdisplay = 1080,
	.hsync_start = 1080 + 274,//HFP
	.hsync_end = 1080 + 274 + 12,//HSA
	.htotal = 1080 + 274 + 12 + 56,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 2528,//VFP
	.vsync_end = 2400 + 2528 + 10,//VSA
	.vtotal = 2400 + 2528 + 10 + 10,//VBP
	.vrefresh = 60,
};

static const struct drm_display_mode performance_mode = {
	.clock = 422206,
	.hdisplay = 1080,
	.hsync_start = 1080 + 274,//HFP
	.hsync_end = 1080 + 274 + 12,//HSA
	.htotal = 1080 + 274 + 12 + 56,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 879,//VFP
	.vsync_end = 2400 + 879 + 10,//VSA
	.vtotal = 2400 + 879 + 10 + 10,//VBP
	.vrefresh = 90,
};

static const struct drm_display_mode performance_mode1 = {
	.clock = 422163,
	.hdisplay = 1080,
	.hsync_start = 1080 + 274,//HFP
	.hsync_end = 1080 + 274 + 12,//HSA
	.htotal = 1080 + 274 + 12 + 56,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 54,//VFP
	.vsync_end = 2400 + 54 + 10,//VSA
	.vtotal = 2400 + 54 + 10 + 10,//VBP
	.vrefresh = 120,
};


#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 369,
	.vfp_low_power = 4178,//45hz
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
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
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 738,
	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 120,
	},
	.dyn = {
		.switch_en = 1,
		.pll_clk = 428,
		.vfp_lp_dyn = 4178,
		.hfp = 396,
		.vfp = 2528,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 369,
	.vfp_low_power = 2528,//60hz
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
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
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 738,
	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 120,
	},
	.dyn = {
		.switch_en = 1,
		.pll_clk = 428,
		.vfp_lp_dyn = 2528,
		.hfp = 396,
		.vfp = 879,
	},
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 369,
	.vfp_low_power = 2528,//idle 60hz
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
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
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 170,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 43,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 7072,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 738,
	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 120,
	},
	.dyn = {
		.switch_en = 1,
		.pll_clk = 428,
		.vfp_lp_dyn = 2528,
		.hfp = 396,
		.vfp = 54,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int jdi_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{

	if (level > 255)
		level = 255;
	pr_info("%s backlight = -%d\n", __func__, level);
	bl_tb0[1] = (u8)level;
#if 0
	char bl_tb0[] = {0x51, 0xf, 0xff};

	if (level > 255)
		level = 255;

	level = level * 4095 / 255;
	bl_tb0[1] = ((level >> 8) & 0xf);
	bl_tb0[2] = (level & 0xff);
#endif
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
static int mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);

	if (m->vrefresh == 60)
		ext->params = &ext_params;
	else if (m->vrefresh == 90)
		ext->params = &ext_params_90hz;
	else if (m->vrefresh == 120)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct jdi *ctx = panel_to_jdi(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = jdi_setbacklight_cmdq,
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

static int jdi_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;

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

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode.hdisplay, performance_mode.vdisplay,
			 performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	mode3 = drm_mode_duplicate(panel->drm, &performance_mode1);
	if (!mode3) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode1.hdisplay, performance_mode1.vdisplay,
			 performance_mode1.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode3);

	panel->connector->display_info.width_mm = 70;
	panel->connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs jdi_drm_funcs = {
	.disable = jdi_disable,
	.unprepare = jdi_unprepare,
	.prepare = jdi_prepare,
	.enable = jdi_enable,
	.get_modes = jdi_get_modes,
};

static int jdi_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct jdi *ctx;
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
	ctx = devm_kzalloc(dev, sizeof(struct jdi), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 3;
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
		dev_err(dev, "%s:cannot get reset-gpios %ld\n",
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
	ctx->panel.funcs = &jdi_drm_funcs;

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

	pr_info("%s- jdi,nt36672c,cphy,vdo\n", __func__);

	return ret;
}

static int jdi_remove(struct mipi_dsi_device *dsi)
{
	struct jdi *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id jdi_of_match[] = {
	{
	    .compatible = "jdi,nt36672c,cphy,vdo",
	},
	{}
};

MODULE_DEVICE_TABLE(of, jdi_of_match);

static struct mipi_dsi_driver jdi_driver = {
	.probe = jdi_probe,
	.remove = jdi_remove,
	.driver = {
		.name = "panel-jdi-nt36672c-cphy-vdo",
		.owner = THIS_MODULE,
		.of_match_table = jdi_of_match,
	},
};


module_mipi_dsi_driver(jdi_driver);

MODULE_AUTHOR("Elon Hsu <elon.hsu@mediatek.com>");
MODULE_DESCRIPTION("jdi r66451 CMD AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
