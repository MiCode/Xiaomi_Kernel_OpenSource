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

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

static char bl_tb0[] = { 0x51, 0xff };

//TO DO: You have to do that remove macro BYPASSI2C and solve build error
//otherwise voltage will be unstable
#define BYPASSI2C

#ifndef BYPASSI2C
/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;

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
#endif

struct jdi {
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
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X10);
	//REGR 0XFE,0X10
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XB0, 0X00);
	//DSC OFF
	jdi_dcs_write_seq_static(ctx, 0XC0, 0X00);
	jdi_dcs_write_seq_static(ctx, 0XC2, 0X1B, 0XA0);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X20);
	//REGR 0XFE,0X20
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X01, 0X66);
	jdi_dcs_write_seq_static(ctx, 0X06, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X07, 0X38);
	jdi_dcs_write_seq_static(ctx, 0X18, 0X66);
	jdi_dcs_write_seq_static(ctx, 0X1B, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X2F, 0X83);
	jdi_dcs_write_seq_static(ctx, 0X5C, 0X90);
	jdi_dcs_write_seq_static(ctx, 0X5E, 0XAA);
	jdi_dcs_write_seq_static(ctx, 0X69, 0X91);
	jdi_dcs_write_seq_static(ctx, 0X95, 0XD1);
	jdi_dcs_write_seq_static(ctx, 0X96, 0XD1);
	jdi_dcs_write_seq_static(ctx, 0XF2, 0X65);
	jdi_dcs_write_seq_static(ctx, 0XF3, 0X64);
	jdi_dcs_write_seq_static(ctx, 0XF4, 0X65);
	jdi_dcs_write_seq_static(ctx, 0XF5, 0X64);
	jdi_dcs_write_seq_static(ctx, 0XF6, 0X65);
	jdi_dcs_write_seq_static(ctx, 0XF7, 0X64);
	jdi_dcs_write_seq_static(ctx, 0XF8, 0X65);
	jdi_dcs_write_seq_static(ctx, 0XF9, 0X64);
	jdi_dcs_write_seq_static(ctx, 0x89, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x8A, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x8D, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x8E, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x8F, 0x15);//VCOM
	jdi_dcs_write_seq_static(ctx, 0x91, 0x15);//VCOM

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X24);
	//REGR 0XFE,0X24
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X01, 0X0F);
	jdi_dcs_write_seq_static(ctx, 0X03, 0X0C);
	jdi_dcs_write_seq_static(ctx, 0X05, 0X1D);
	jdi_dcs_write_seq_static(ctx, 0X08, 0X2F);
	jdi_dcs_write_seq_static(ctx, 0X09, 0X2E);
	jdi_dcs_write_seq_static(ctx, 0X0A, 0X2D);
	jdi_dcs_write_seq_static(ctx, 0X0B, 0X2C);
	jdi_dcs_write_seq_static(ctx, 0X11, 0X17);
	jdi_dcs_write_seq_static(ctx, 0X12, 0X13);
	jdi_dcs_write_seq_static(ctx, 0X13, 0X15);
	jdi_dcs_write_seq_static(ctx, 0X15, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X16, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X17, 0X18);
	jdi_dcs_write_seq_static(ctx, 0X1B, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X1D, 0X1D);
	jdi_dcs_write_seq_static(ctx, 0X20, 0X2F);
	jdi_dcs_write_seq_static(ctx, 0X21, 0X2E);
	jdi_dcs_write_seq_static(ctx, 0X22, 0X2D);
	jdi_dcs_write_seq_static(ctx, 0X23, 0X2C);
	jdi_dcs_write_seq_static(ctx, 0X29, 0X17);
	jdi_dcs_write_seq_static(ctx, 0X2A, 0X13);
	jdi_dcs_write_seq_static(ctx, 0X2B, 0X15);
	jdi_dcs_write_seq_static(ctx, 0X2F, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X30, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X31, 0X18);
	jdi_dcs_write_seq_static(ctx, 0X32, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X34, 0X10);
	jdi_dcs_write_seq_static(ctx, 0X35, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X36, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X37, 0X20);
	jdi_dcs_write_seq_static(ctx, 0X4D, 0X1B);
	jdi_dcs_write_seq_static(ctx, 0X4E, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X4F, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X71, 0X30);
	jdi_dcs_write_seq_static(ctx, 0X79, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X7A, 0X82);
	jdi_dcs_write_seq_static(ctx, 0X7B, 0X96);
	jdi_dcs_write_seq_static(ctx, 0X7D, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X80, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X81, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X82, 0X13);
	jdi_dcs_write_seq_static(ctx, 0X84, 0X31);
	jdi_dcs_write_seq_static(ctx, 0X85, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X86, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X87, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X90, 0X13);
	jdi_dcs_write_seq_static(ctx, 0X92, 0X31);
	jdi_dcs_write_seq_static(ctx, 0X93, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X94, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X95, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X9C, 0XF4);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XA0, 0X16);
	jdi_dcs_write_seq_static(ctx, 0XA2, 0X16);
	jdi_dcs_write_seq_static(ctx, 0XA3, 0X02);
	jdi_dcs_write_seq_static(ctx, 0XA4, 0X04);
	jdi_dcs_write_seq_static(ctx, 0XA5, 0X04);
	jdi_dcs_write_seq_static(ctx, 0XC6, 0XC0);
	jdi_dcs_write_seq_static(ctx, 0XC9, 0X00);
	jdi_dcs_write_seq_static(ctx, 0XD9, 0X80);
	jdi_dcs_write_seq_static(ctx, 0XE9, 0X02);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X25);
	//REGR 0XFE,0X25
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X0F, 0X1B);
	jdi_dcs_write_seq_static(ctx, 0X18, 0X21);
	jdi_dcs_write_seq_static(ctx, 0X19, 0XE4);
	jdi_dcs_write_seq_static(ctx, 0X21, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X66, 0XD8);
	jdi_dcs_write_seq_static(ctx, 0X68, 0X50);
	jdi_dcs_write_seq_static(ctx, 0X69, 0X10);
	jdi_dcs_write_seq_static(ctx, 0X6B, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X6D, 0X0D);
	jdi_dcs_write_seq_static(ctx, 0X6E, 0X48);
	jdi_dcs_write_seq_static(ctx, 0X72, 0X41);
	jdi_dcs_write_seq_static(ctx, 0X73, 0X4A);
	jdi_dcs_write_seq_static(ctx, 0X74, 0XD0);
	jdi_dcs_write_seq_static(ctx, 0X77, 0X62);
	jdi_dcs_write_seq_static(ctx, 0X79, 0X77);
	jdi_dcs_write_seq_static(ctx, 0X7D, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X7E, 0X1D);
	jdi_dcs_write_seq_static(ctx, 0X7F, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X80, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X84, 0X0D);
	jdi_dcs_write_seq_static(ctx, 0XCF, 0X80);
	jdi_dcs_write_seq_static(ctx, 0XD6, 0X80);
	jdi_dcs_write_seq_static(ctx, 0XD7, 0X80);
	jdi_dcs_write_seq_static(ctx, 0XEF, 0X20);
	jdi_dcs_write_seq_static(ctx, 0XF0, 0X84);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X26);
	//REGR 0XFE,0X26
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X15, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X81, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X83, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X84, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X85, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X86, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X87, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X88, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X8A, 0X1A);
	jdi_dcs_write_seq_static(ctx, 0X8B, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X8C, 0X24);
	jdi_dcs_write_seq_static(ctx, 0X8E, 0X42);
	jdi_dcs_write_seq_static(ctx, 0X8F, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X90, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X91, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X9A, 0X80);
	jdi_dcs_write_seq_static(ctx, 0X9B, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X9C, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X9E, 0X00);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X27);
	//REGR 0XFE,0X27
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X01, 0X60);
	jdi_dcs_write_seq_static(ctx, 0X20, 0X81);
	jdi_dcs_write_seq_static(ctx, 0X21, 0XE7);
	jdi_dcs_write_seq_static(ctx, 0X25, 0X82);
	jdi_dcs_write_seq_static(ctx, 0X26, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X6E, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X6F, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X70, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X71, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X72, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X75, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X76, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X77, 0X00);
	jdi_dcs_write_seq_static(ctx, 0X7D, 0X09);
	jdi_dcs_write_seq_static(ctx, 0X7E, 0X5F);
	jdi_dcs_write_seq_static(ctx, 0X80, 0X23);
	jdi_dcs_write_seq_static(ctx, 0X82, 0X09);
	jdi_dcs_write_seq_static(ctx, 0X83, 0X5F);
	jdi_dcs_write_seq_static(ctx, 0X88, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X89, 0X10);
	jdi_dcs_write_seq_static(ctx, 0XA5, 0X10);
	jdi_dcs_write_seq_static(ctx, 0XA6, 0X23);
	jdi_dcs_write_seq_static(ctx, 0XA7, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XB6, 0X40);
	jdi_dcs_write_seq_static(ctx, 0XE3, 0X02);
	jdi_dcs_write_seq_static(ctx, 0XE4, 0XDA);
	jdi_dcs_write_seq_static(ctx, 0XE5, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XE6, 0X6D);
	jdi_dcs_write_seq_static(ctx, 0XE9, 0X03);
	jdi_dcs_write_seq_static(ctx, 0XEA, 0X2F);
	jdi_dcs_write_seq_static(ctx, 0XEB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XEC, 0X98);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X2A);
	//REGR 0XFE,0X2A
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X00, 0X91);
	jdi_dcs_write_seq_static(ctx, 0X03, 0X20);
	jdi_dcs_write_seq_static(ctx, 0X07, 0X52);
	jdi_dcs_write_seq_static(ctx, 0X0A, 0X70);
	jdi_dcs_write_seq_static(ctx, 0X0D, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X0E, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X11, 0XF0);
	jdi_dcs_write_seq_static(ctx, 0X15, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X16, 0XB6);
	jdi_dcs_write_seq_static(ctx, 0X19, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X1A, 0X8A);
	jdi_dcs_write_seq_static(ctx, 0X1B, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X1D, 0X36);
	jdi_dcs_write_seq_static(ctx, 0X1E, 0X4F);
	jdi_dcs_write_seq_static(ctx, 0X1F, 0X51);
	jdi_dcs_write_seq_static(ctx, 0X20, 0X4F);
	jdi_dcs_write_seq_static(ctx, 0X28, 0XEC);
	jdi_dcs_write_seq_static(ctx, 0X29, 0X0C);
	jdi_dcs_write_seq_static(ctx, 0X2A, 0X05);
	jdi_dcs_write_seq_static(ctx, 0X2D, 0X06);
	jdi_dcs_write_seq_static(ctx, 0X2F, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X30, 0X4A);
	jdi_dcs_write_seq_static(ctx, 0X33, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X34, 0XEE);
	jdi_dcs_write_seq_static(ctx, 0X35, 0X30);
	jdi_dcs_write_seq_static(ctx, 0X36, 0X06);
	jdi_dcs_write_seq_static(ctx, 0X37, 0XE9);
	jdi_dcs_write_seq_static(ctx, 0X38, 0X34);
	jdi_dcs_write_seq_static(ctx, 0X39, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X3A, 0X4A);
	jdi_dcs_write_seq_static(ctx, 0X45, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X46, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X47, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X4A, 0XA0);
	jdi_dcs_write_seq_static(ctx, 0X4E, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X4F, 0XB6);
	jdi_dcs_write_seq_static(ctx, 0X52, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X8A);
	jdi_dcs_write_seq_static(ctx, 0X54, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X56, 0X36);
	jdi_dcs_write_seq_static(ctx, 0X57, 0X76);
	jdi_dcs_write_seq_static(ctx, 0X58, 0X76);
	jdi_dcs_write_seq_static(ctx, 0X59, 0X76);
	jdi_dcs_write_seq_static(ctx, 0X60, 0X80);
	jdi_dcs_write_seq_static(ctx, 0X61, 0XA0);
	jdi_dcs_write_seq_static(ctx, 0X62, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X63, 0X33);
	jdi_dcs_write_seq_static(ctx, 0X65, 0X04);
	jdi_dcs_write_seq_static(ctx, 0X66, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X67, 0X05);
	jdi_dcs_write_seq_static(ctx, 0X68, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X6A, 0X4C);
	jdi_dcs_write_seq_static(ctx, 0X6B, 0XA2);
	jdi_dcs_write_seq_static(ctx, 0X6C, 0X21);
	jdi_dcs_write_seq_static(ctx, 0X6D, 0XBB);
	jdi_dcs_write_seq_static(ctx, 0X6E, 0X9F);
	jdi_dcs_write_seq_static(ctx, 0X6F, 0X23);
	jdi_dcs_write_seq_static(ctx, 0X70, 0XB9);
	jdi_dcs_write_seq_static(ctx, 0X71, 0X05);
	jdi_dcs_write_seq_static(ctx, 0X7A, 0X07);
	jdi_dcs_write_seq_static(ctx, 0X7B, 0X40);
	jdi_dcs_write_seq_static(ctx, 0X7D, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X7F, 0X2C);
	jdi_dcs_write_seq_static(ctx, 0X83, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X84, 0XB6);
	jdi_dcs_write_seq_static(ctx, 0X87, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X88, 0X8A);
	jdi_dcs_write_seq_static(ctx, 0X89, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X8B, 0X36);
	jdi_dcs_write_seq_static(ctx, 0X8C, 0X3A);
	jdi_dcs_write_seq_static(ctx, 0X8D, 0X3A);
	jdi_dcs_write_seq_static(ctx, 0X8E, 0X3A);
	jdi_dcs_write_seq_static(ctx, 0X95, 0X80);
	jdi_dcs_write_seq_static(ctx, 0X96, 0XFD);
	jdi_dcs_write_seq_static(ctx, 0X97, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X98, 0X17);
	jdi_dcs_write_seq_static(ctx, 0X99, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X9A, 0X08);
	jdi_dcs_write_seq_static(ctx, 0X9B, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X9C, 0X4C);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0XAF);
	jdi_dcs_write_seq_static(ctx, 0X9F, 0X6B);
	jdi_dcs_write_seq_static(ctx, 0XA0, 0XFF);
	jdi_dcs_write_seq_static(ctx, 0XA2, 0X40);
	jdi_dcs_write_seq_static(ctx, 0XA3, 0X6F);
	jdi_dcs_write_seq_static(ctx, 0XA4, 0XF9);
	jdi_dcs_write_seq_static(ctx, 0XA5, 0X45);
	jdi_dcs_write_seq_static(ctx, 0XA6, 0X6A);
	jdi_dcs_write_seq_static(ctx, 0XA7, 0X4C);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X2C);
	//REGR 0XFE,0X2C
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X00, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X01, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X02, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X03, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X04, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X05, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X0D, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X0E, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X16, 0X1B);
	jdi_dcs_write_seq_static(ctx, 0X17, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X18, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X19, 0X4B);
	jdi_dcs_write_seq_static(ctx, 0X2A, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X4D, 0X16);
	jdi_dcs_write_seq_static(ctx, 0X4E, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X4F, 0X27);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X54, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X55, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X56, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X58, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X59, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X61, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X62, 0X1F);
	jdi_dcs_write_seq_static(ctx, 0X6A, 0X14);
	jdi_dcs_write_seq_static(ctx, 0X6B, 0X34);
	jdi_dcs_write_seq_static(ctx, 0X6C, 0X34);
	jdi_dcs_write_seq_static(ctx, 0X6D, 0X34);
	jdi_dcs_write_seq_static(ctx, 0X7E, 0X03);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X0E);
	jdi_dcs_write_seq_static(ctx, 0X9E, 0X02);
	jdi_dcs_write_seq_static(ctx, 0X9F, 0X03);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X20);
	jdi_dcs_write_seq_static(ctx, 0xfb, 0x01);
	jdi_dcs_write_seq_static(ctx, 0xb0, 0x00, 0X00, 0X00, 0X15, 0X00, 0X3F, 0X00, 0X5F, 0X00,
		0X7E, 0X00, 0X97, 0X00, 0XAF, 0X00, 0Xc3);
	jdi_dcs_write_seq_static(ctx, 0xb1, 0x00, 0Xd7, 0X01, 0X0A, 0X01, 0X32, 0X01, 0X6F, 0X01,
		0X9E, 0X01, 0XE5, 0X02, 0X1d, 0X02, 0X1e);
	jdi_dcs_write_seq_static(ctx, 0xb2, 0x02, 0X56, 0X02, 0X94, 0X02, 0XBC, 0X02, 0XF1, 0X03,
		0X13, 0X03, 0X41, 0X03, 0X4F, 0X03, 0X5F);
	jdi_dcs_write_seq_static(ctx, 0xb3, 0x03, 0X71, 0X03, 0X84, 0X03, 0X99, 0X03, 0XB0, 0X03,
		0XCA, 0X03, 0XD7, 0X00, 0X00);
	jdi_dcs_write_seq_static(ctx, 0xb4, 0x00, 0X00, 0X00, 0X17, 0X00, 0X46, 0X00, 0X69, 0X00,
		0X8C, 0X00, 0Xa5, 0X00, 0XbE, 0X00, 0Xd1);
	jdi_dcs_write_seq_static(ctx, 0xb5, 0x00, 0Xe4, 0X01, 0X18, 0X01, 0X40, 0X01, 0X7C, 0X01,
		0XAA, 0X01, 0Xf0, 0X02, 0X27, 0X02, 0X28);
	jdi_dcs_write_seq_static(ctx, 0xb6, 0x02, 0X5E, 0X02, 0X9B, 0X02, 0XC3, 0X02, 0XF6, 0X03,
		0X18, 0X03, 0X45, 0X03, 0X54, 0X03, 0X63);
	jdi_dcs_write_seq_static(ctx, 0xb7, 0x03, 0X75, 0X03, 0X87, 0X03, 0X9C, 0X03, 0XB2, 0X03,
		0XCA, 0X03, 0XD7, 0X00, 0X00);
	jdi_dcs_write_seq_static(ctx, 0xb8, 0x00, 0X00, 0X00, 0X18, 0X00, 0X49, 0X00, 0X6B, 0X00,
		0X8E, 0X00, 0Xa8, 0X00, 0XC1, 0X00, 0Xd3);
	jdi_dcs_write_seq_static(ctx, 0xb9, 0x00, 0Xe5, 0X01, 0X18, 0X01, 0X3F, 0X01, 0X7B, 0X01,
		0Xa8, 0X01, 0XEC, 0X02, 0X24, 0X02, 0X26);
	jdi_dcs_write_seq_static(ctx, 0xba, 0x02, 0X5A, 0X02, 0X97, 0X02, 0Xc0, 0X02, 0XF4, 0X03,
		0X15, 0X03, 0X43, 0X03, 0X51, 0X03, 0X61);
	jdi_dcs_write_seq_static(ctx, 0xbb, 0x03, 0X72, 0X03, 0X85, 0X03, 0X9A, 0X03, 0XB1, 0X03,
		0XCA, 0X03, 0XD7, 0X00, 0X00);

	jdi_dcs_write_seq_static(ctx, 0xC6, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xC7, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xC8, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xC9, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xCA, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xCB, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xCC, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xCD, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xCE, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xCF, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xD0, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD1, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD2, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD3, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD4, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xD5, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD6, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD7, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD8, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xD9, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xDA, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xDB, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xDC, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xDD, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xDE, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xDF, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xE0, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xE1, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xE2, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xE3, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xE4, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xE5, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xE6, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xE7, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xE8, 0x00);

	jdi_dcs_write_seq_static(ctx, 0xE9, 0x00);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X21);
	jdi_dcs_write_seq_static(ctx, 0xfb, 0x01);
	jdi_dcs_write_seq_static(ctx, 0xb0, 0x00, 0X00, 0X00, 0X15, 0X00, 0X3F, 0X00, 0X5F, 0X00,
		0X7E, 0X00, 0X97, 0X00, 0XAF, 0X00, 0Xc3);
	jdi_dcs_write_seq_static(ctx, 0xb1, 0x00, 0Xd7, 0X01, 0X0A, 0X01, 0X32, 0X01, 0X6F, 0X01,
		0X9E, 0X01, 0XE5, 0X02, 0X1d, 0X02, 0X1e);
	jdi_dcs_write_seq_static(ctx, 0xb2, 0x02, 0X56, 0X02, 0X94, 0X02, 0XBC, 0X02, 0XF1, 0X03,
		0X13, 0X03, 0X41, 0X03, 0X4F, 0X03, 0X5F);
	jdi_dcs_write_seq_static(ctx, 0xb3, 0x03, 0X71, 0X03, 0X84, 0X03, 0X99, 0X03, 0XB0, 0X03,
		0XCA, 0X03, 0XD7, 0X00, 0X00);
	jdi_dcs_write_seq_static(ctx, 0xb4, 0x00, 0X00, 0X00, 0X17, 0X00, 0X46, 0X00, 0X69, 0X00,
		0X8C, 0X00, 0Xa5, 0X00, 0XbE, 0X00, 0Xd1);
	jdi_dcs_write_seq_static(ctx, 0xb5, 0x00, 0Xe4, 0X01, 0X18, 0X01, 0X40, 0X01, 0X7C, 0X01,
		0XAA, 0X01, 0Xf0, 0X02, 0X27, 0X02, 0X28);
	jdi_dcs_write_seq_static(ctx, 0xb6, 0x02, 0X5E, 0X02, 0X9B, 0X02, 0XC3, 0X02, 0XF6, 0X03,
		0X18, 0X03, 0X45, 0X03, 0X54, 0X03, 0X63);
	jdi_dcs_write_seq_static(ctx, 0xb7, 0x03, 0X75, 0X03, 0X87, 0X03, 0X9C, 0X03, 0XB2, 0X03,
		0XCA, 0X03, 0XD7, 0X00, 0X00);
	jdi_dcs_write_seq_static(ctx, 0xb8, 0x00, 0X00, 0X00, 0X18, 0X00, 0X49, 0X00, 0X6B, 0X00,
		0X8E, 0X00, 0Xa8, 0X00, 0XC1, 0X00, 0Xd3);
	jdi_dcs_write_seq_static(ctx, 0xb9, 0x00, 0Xe5, 0X01, 0X18, 0X01, 0X3F, 0X01, 0X7B, 0X01,
		0Xa8, 0X01, 0XEC, 0X02, 0X24, 0X02, 0X26);
	jdi_dcs_write_seq_static(ctx, 0xba, 0x02, 0X5A, 0X02, 0X97, 0X02, 0Xc0, 0X02, 0XF4, 0X03,
		0X15, 0X03, 0X43, 0X03, 0X51, 0X03, 0X61);
	jdi_dcs_write_seq_static(ctx, 0xbb, 0x03, 0X72, 0X03, 0X85, 0X03, 0X9A, 0X03, 0XB1, 0X03,
		0XCA, 0X03, 0XD7, 0X00, 0X00);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0XE0);
	//REGR 0XFE,0XE0
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X35, 0X82);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0XF0);
	//REGR 0XFE,0XF0
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X1C, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X33, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X5A, 0X00);
	jdi_dcs_write_seq_static(ctx, 0XD2, 0X52);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0XD0);
	//REGR 0XFE,0XD0
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X53, 0X22);
	jdi_dcs_write_seq_static(ctx, 0X54, 0X02);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0XC0);
	//CCMON
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X9C, 0X11);
	jdi_dcs_write_seq_static(ctx, 0X9D, 0X11);

	jdi_dcs_write_seq_static(ctx, 0XFF, 0X2B);
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0XB7, 0X08);
	jdi_dcs_write_seq_static(ctx, 0XB8, 0X0B);
	//CCMOFF
	//CCMRUN
	jdi_dcs_write_seq_static(ctx, 0XFF, 0X10);
	jdi_dcs_write_seq_static(ctx, 0XFB, 0X01);
	jdi_dcs_write_seq_static(ctx, 0X35, 0X01);//TE Enable
	jdi_dcs_write_seq_static(ctx, 0X51, 0XFF);//Write_Display_Brightness
	jdi_dcs_write_seq_static(ctx, 0X53, 0X0C);//Write_CTRL_Display
	jdi_dcs_write_seq_static(ctx, 0X55, 0X00);//Write CABC

	jdi_dcs_write_seq_static(ctx, 0x11);
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

	if (!ctx->prepared)
		return 0;

	jdi_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	jdi_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(150);

	/*
	 * ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	 * gpiod_set_value(ctx->reset_gpio, 0);
	 * devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	 */
	if (ctx->gate_ic == 0) {
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		usleep_range(2000, 2001);

		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	} else if (ctx->gate_ic == 4831) {
		_gate_ic_i2c_panel_bias_enable(0);
		_gate_ic_Power_off();
	}
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
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end
	if (ctx->gate_ic == 0) {
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		usleep_range(2000, 2001);
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
	} else if (ctx->gate_ic == 4831) {
		_gate_ic_Power_on();
		_gate_ic_i2c_panel_bias_enable(1);
	}
#ifndef BYPASSI2C
	_lcm_i2c_write_bytes(0x0, 0xf);
	_lcm_i2c_write_bytes(0x1, 0xf);
#endif
	jdi_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		jdi_unprepare(panel);

	ctx->prepared = true;
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
	.clock = 182284,
	.hdisplay = 1080,
	.hsync_start = 1080 + 76,//HFP
	.hsync_end = 1080 + 76 + 12,//HSA
	.htotal = 1080 + 76 + 12 + 60,//HBP
	.vdisplay = 2400,
	.vsync_start = 2400 + 54,//VFP
	.vsync_end = 2400 + 54 + 10,//VSA
	.vtotal = 2400 + 54 + 10 + 10,//VBP
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 595,
	.vfp_low_power = 840,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lane_swap_en = 1,
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
	.data_rate = 1190,
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

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct jdi *ctx = panel_to_jdi(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = jdi_setbacklight_cmdq,
	.ata_check = panel_ata_check,
};
#endif

static int jdi_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;

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

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 152;

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
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct jdi *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+\n", __func__);

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

	ctx = devm_kzalloc(dev, sizeof(struct jdi), GFP_KERNEL);
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
	drm_panel_init(&ctx->panel, dev, &jdi_drm_funcs, DRM_MODE_CONNECTOR_DSI);

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

	pr_info("%s- jdi,nt36672e,vdo,60hz\n", __func__);

	return ret;
}

static int jdi_remove(struct mipi_dsi_device *dsi)
{
	struct jdi *ctx = mipi_dsi_get_drvdata(dsi);
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

static const struct of_device_id jdi_of_match[] = {
	{
	    .compatible = "jdi,nt36672e,vdo,60hz",
	},
	{}
};

MODULE_DEVICE_TABLE(of, jdi_of_match);

static struct mipi_dsi_driver jdi_driver = {
	.probe = jdi_probe,
	.remove = jdi_remove,
	.driver = {
		.name = "panel-jdi-nt36672e-vdo-60hz",
		.owner = THIS_MODULE,
		.of_match_table = jdi_of_match,
	},
};

module_mipi_dsi_driver(jdi_driver);

MODULE_AUTHOR("shaohua deng <shaohua.deng@mediatek.com>");
MODULE_DESCRIPTION("JDI NT36672E VDO 60HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
