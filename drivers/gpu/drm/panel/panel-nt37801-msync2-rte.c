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
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

/* enable this to check panel self -bist pattern
 * #define PANEL_BIST_PATTERN
 ****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

static char bl_tb0[] = {0x51, 0x3, 0xff};
#define MODE_0_FPS (60)
#define MODE_1_FPS (80)
#define MODE_2_FPS (90)
#define MODE_3_FPS (120)
#define MTE_OFF (0xFFFF)
static int current_fps = 120;


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

//#define S6E3HAE_SUPPORT_WQHD

#define DATA_RATE		1100
#define MODE0_FPS		60
#define MODE1_FPS		120
#define MODE2_FPS		60
#define MODE3_FPS		120
#define MODE1_HFP		20
#define MODE1_HSA		2
#define MODE1_HBP		16
#define MODE1_VFP		54
#define MODE1_VSA		10
#define MODE1_VBP		10
#define HACT_WQHD		1440
#define VACT_WQHD		3200
#define HACT_FHDP		1080
#define VACT_FHDP		2400

struct nt37801_lcm {
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

#define nt37801_lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define nt37801_lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct nt37801_lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct nt37801_lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct nt37801_lcm *ctx, u8 cmd, void *data, size_t len)
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

static void lcm_panel_get_data(struct nt37801_lcm *ctx)
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

static void lcm_dcs_write(struct nt37801_lcm *ctx, const void *data, size_t len)
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


#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF


struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[130];
};

//RTE60
static struct LCM_setting_table fhdplus60_init_setting[] = {
	/* ESD */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x05}},
	{0xBE, 01, {0x08}},
	{0x6F, 01, {0x01}},
	{0xBE, 01, {0x45}},
	/* DC init Code */
	/* 3.1 DVDD Strong */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x01}},
	{0xC5, 04, {0x00, 0x0B, 0x0B, 0x0B}},
	/* DSC setting */
	{0x90, 02, {0x13, 0x03}},
	{0x91, 18, {0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00,
		0x03, 0x1C, 0x01, 0x7E, 0x00, 0x0F, 0x08, 0xBB,
		0x04, 0x3D, 0x10, 0xF0}},
	{0x1F, 01, {0x70}},
	/* 3.2 TE ON */
	{0x35, 01, {0x00}},
	{0x5A, 01, {0x01}},
	/* 3.3 CASET/RASET Setting*/
	{0x2A, 04, {0x00, 0x00, 0x04, 0x37}},
	{0x2B, 04, {0x00, 0x00, 0x09, 0x5F}},
	/* 3.4 Dimming ON Setting */
	{0x53, 01, {0x28}},
	/* 3.4.1 Adjusting dimming speed */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x05}},
	{0xB2, 02, {0x08, 0x08}},
	{0x6F, 01, {0x09}},
	{0xB2, 02, {0x5F, 0x5F}},


	/*RTE*/
	/* EM cycle 612 */
	{0x6F, 01, {0x13}},
	{0xC0, 02, {0x02, 0x64}},
	/* XXXX */
	{0x6F, 01, {0x1F}},
	{0xC0, 01, {0x38}},
	/* AFP */
	{0x6F, 01, {0x26}},
	{0xC0, 02, {0x0A, 0x00}},
	/* protection period */
	{0x6F, 01, {0x20}},
	{0xC0, 02, {0x02, 0x00}},
	/* Valid TE period */
	{0x40, 01, {0x07}},
	{0x6F, 01, {0x01}},
	{0x40, 02, {0x12, 0xBC}},
	{0x6F, 01, {0x03}},
	{0x40, 02, {0x1C, 0xB0}},


	/* 3.5 Frame Rate 120Hz GIR OFF */
	{0x2F, 01, {0x02}},//02 60fps
	/*{0x26, 01, {0x00}},*/
	/* 3.6 GIR OFF */
	{0x5F, 01, {0x01}},
	/* 3.7 DBV */
	{0x51, 02, {0x07, 0xFF}},

	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x0E}},
	{0xBA, 07, {0x00, 0x6B, 0x00, 0x14, 0x09, 0xAC, 0x10}},
	{0x6F, 01, {0x13}},
	{0xB2, 01, {0x47}},

	/* 4. User Command Set */
	/* Sleep Out */
	{0x11, 00, {}},
	{REGFLAG_DELAY, 120, {}},
	/* Display On */
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

//RTE80
static struct LCM_setting_table fhdplus80_init_setting[] = {
	/* ESD */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x05}},
	{0xBE, 01, {0x08}},
	{0x6F, 01, {0x01}},
	{0xBE, 01, {0x45}},
	/* DC init Code */
	/* 3.1 DVDD Strong */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x01}},
	{0xC5, 04, {0x00, 0x0B, 0x0B, 0x0B}},
	/* DSC setting */
	{0x90, 02, {0x13, 0x03}},
	{0x91, 18, {0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00,
		0x03, 0x1C, 0x01, 0x7E, 0x00, 0x0F, 0x08, 0xBB,
		0x04, 0x3D, 0x10, 0xF0}},
	{0x1F, 01, {0x70}},
	/* 3.2 TE ON */
	{0x35, 01, {0x00}},
	{0x5A, 01, {0x01}},
	/* 3.3 CASET/RASET Setting*/
	{0x2A, 04, {0x00, 0x00, 0x04, 0x37}},
	{0x2B, 04, {0x00, 0x00, 0x09, 0x5F}},
	/* 3.4 Dimming ON Setting */
	{0x53, 01, {0x28}},
	/* 3.4.1 Adjusting dimming speed */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	/*{0xB2, 01, {0x91}},//DC on*/
	/*{0xBA, 01, {0x00}},*/
	/*{0xBE, 02, {0x47, 0x47}},*/
	{0x6F, 01, {0x05}},
	{0xB2, 02, {0x08, 0x08}},
	{0x6F, 01, {0x09}},
	{0xB2, 02, {0x5F, 0x5F}},


	/*RTE*/
	/* EM cycle 612 */
	{0x6F, 01, {0x13}},
	{0xC0, 02, {0x02, 0x64}},
	/* XXXX */
	{0x6F, 01, {0x1F}},
	{0xC0, 01, {0x38}},
	/* AFP */
	{0x6F, 01, {0x26}},
	{0xC0, 02, {0x0A, 0x00}},
	/* protection period */
	{0x6F, 01, {0x20}},
	{0xC0, 02, {0x02, 0x00}},
	/* Valid TE period */
	{0x40, 01, {0x07}},
	{0x6F, 01, {0x01}},
	{0x40, 02, {0x0D, 0xF8}},
	{0x6F, 01, {0x03}},
	{0x40, 02, {0x17, 0xEC}},


	/* 3.5 Frame Rate 120Hz GIR OFF */
	{0x2F, 01, {0x02}},//02 60fps
	/*{0x26, 01, {0x00}},*/
	/* 3.6 GIR OFF */
	{0x5F, 01, {0x01}},
	/* 3.7 DBV */
	{0x51, 02, {0x07, 0xFF}},

	/* VFP & EM pluse */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x0E}},
	{0xBA, 07, {0x00, 0x6B, 0x00, 0x14, 0x04, 0xE4, 0x10}},
	{0x6F, 01, {0x13}},
	{0xB2, 01, {0x45}},

	/* 4. User Command Set */
	/* Sleep Out */
	{0x11, 00, {}},
	{REGFLAG_DELAY, 120, {}},
	/* Display On */
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table fhdplus90_init_setting[] = {
	/* ESD */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x05}},
	{0xBE, 01, {0x08}},
	{0x6F, 01, {0x01}},
	{0xBE, 01, {0x45}},
	/* DC init Code */
	/* 3.1 DVDD Strong */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x01}},
	{0xC5, 04, {0x00, 0x0B, 0x0B, 0x0B}},
	/* DSC setting */
	{0x90, 02, {0x13, 0x03}},
	{0x91, 18, {0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00,
		0x03, 0x1C, 0x01, 0x7E, 0x00, 0x0F, 0x08, 0xBB,
		0x04, 0x3D, 0x10, 0xF0}},
	{0x1F, 01, {0x70}},
	/* 3.2 TE ON */
	{0x35, 01, {0x00}},
	{0x5A, 01, {0x01}},
	/* 3.3 CASET/RASET Setting*/
	{0x2A, 04, {0x00, 0x00, 0x04, 0x37}},
	{0x2B, 04, {0x00, 0x00, 0x09, 0x5F}},
	/* 3.4 Dimming ON Setting */
	{0x53, 01, {0x28}},
	/* 3.4.1 Adjusting dimming speed */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	/*{0xB2, 01, {0x91}},//DC on*/
	/*{0xBA, 01, {0x00}},*/
	/*{0xBE, 02, {0x47, 0x47}},*/
	{0x6F, 01, {0x05}},
	{0xB2, 02, {0x08, 0x08}},
	{0x6F, 01, {0x09}},
	{0xB2, 02, {0x5F, 0x5F}},


	/*RTE*/
	/* EM cycle 612 */
	{0x6F, 01, {0x13}},
	{0xC0, 02, {0x02, 0x64}},
	/* XXXX */
	{0x6F, 01, {0x1F}},
	{0xC0, 01, {0x38}},
	/* AFP */
	{0x6F, 01, {0x26}},
	{0xC0, 02, {0x07, 0xD0}},
	/* protection period */
	{0x6F, 01, {0x20}},
	{0xC0, 02, {0x02, 0x00}},
	/* Valid TE period */
	{0x40, 01, {0x0F}},
	{0x6F, 01, {0x01}},
	/*{0x40, 02, {0x13, 0x0D}},*/
	{0x40, 02, {0x09, 0x61}},
	{0x6F, 01, {0x03}},
	/*{0x40, 02, {0x17, 0x08}},*/
	{0x40, 02, {0x09, 0xC4}},


	/* 3.5 Frame Rate 120Hz GIR OFF */
	{0x2F, 01, {0x02}},//02 60fps
	/*{0x26, 01, {0x00}},*/
	/* 3.6 GIR OFF */
	{0x5F, 01, {0x01}},
	/* 3.7 DBV */
	{0x51, 02, {0x07, 0xFF}},

	/* 4. User Command Set */
	/* Sleep Out */
	{0x11, 00, {}},
	{REGFLAG_DELAY, 120, {}},
	/* Display On */
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

//RTE120
static struct LCM_setting_table fhdplus120_init_setting[] = {
	/* ESD */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x05}},
	{0xBE, 01, {0x08}},
	{0x6F, 01, {0x01}},
	{0xBE, 01, {0x45}},
	/* DC init Code */
	/* 3.1 DVDD Strong */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x01}},
	{0xC5, 04, {0x00, 0x0B, 0x0B, 0x0B}},
	/* DSC setting */
	{0x90, 02, {0x13, 0x03}},
	{0x91, 18, {0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00,
		0x03, 0x1C, 0x01, 0x7E, 0x00, 0x0F, 0x08, 0xBB,
		0x04, 0x3D, 0x10, 0xF0}},
	{0x1F, 01, {0x70}},
	/* 3.2 TE ON */
	{0x35, 01, {0x00}},
	{0x5A, 01, {0x01}},
	/* 3.3 CASET/RASET Setting*/
	{0x2A, 04, {0x00, 0x00, 0x04, 0x37}},
	{0x2B, 04, {0x00, 0x00, 0x09, 0x5F}},
	/* 3.4 Dimming ON Setting */
	{0x53, 01, {0x28}},
	/* 3.4.1 Adjusting dimming speed */
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 01, {0x05}},
	{0xB2, 02, {0x08, 0x08}},
	{0x6F, 01, {0x09}},
	{0xB2, 02, {0x5F, 0x5F}},

	/*RTE*/
	{0x6F, 01, {0x13}},
	{0xC0, 02, {0x02, 0x64}},

	{0x6F, 01, {0x1F}},
	{0xC0, 01, {0x38}},
	/* AFP */
	{0x6F, 01, {0x22}},
	{0xC0, 02, {0x0A, 0x00}},

	{0x6F, 01, {0x20}},
	{0xC0, 02, {0x02, 0x00}},

	{0x40, 01, {0x07}},
	{0x6F, 01, {0x01}},
	{0x40, 02, {0x09, 0x2C}},
	{0x6F, 01, {0x03}},
	{0x40, 02, {0x13, 0x20}},

	/* 3.5 Frame Rate 120Hz GIR OFF */
	{0x2F, 01, {0x00}},
	{0x26, 01, {0x00}},
	/* 3.6 GIR OFF */
	{0x5F, 01, {0x01}},
	/* 3.7 DBV */
	{0x51, 02, {0x07, 0xFF}},
	/* 4. User Command Set */
	/* Sleep Out */
	{0x11, 00, {}},
	{REGFLAG_DELAY, 120, {}},
	/* Display On */
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct nt37801_lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i, j;
	unsigned char temp[255] = {0};

	for (i = 0; i < count; i++) {
		unsigned int cmd = table[i].cmd;

		memset(temp, 0, sizeof(temp));
		switch (cmd) {
		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				msleep(table[i].count);
			else
				msleep(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			temp[0] = cmd;
			for (j = 0; j < table[i].count; j++)
				temp[j+1] = table[i].para_list[j];

			lcm_dcs_write(ctx, temp, table[i].count + 1);
			break;
		}
	}
}

static void lcm_panel_init(struct nt37801_lcm *ctx)
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

	switch (current_fps) {
	case 120:
		push_table(ctx, fhdplus120_init_setting, sizeof(fhdplus120_init_setting) /
				sizeof(struct LCM_setting_table));
		break;
	case 90:
		push_table(ctx, fhdplus90_init_setting, sizeof(fhdplus90_init_setting) /
				sizeof(struct LCM_setting_table));
		break;
	case 80:
		push_table(ctx, fhdplus80_init_setting, sizeof(fhdplus80_init_setting) /
				sizeof(struct LCM_setting_table));
		break;
	case 60:
		push_table(ctx, fhdplus60_init_setting, sizeof(fhdplus60_init_setting) /
				sizeof(struct LCM_setting_table));
		break;
	default:
		break;
	}

	pr_info("%s-\n", __func__);
}

static int nt37801_lcm_disable(struct drm_panel *panel)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int nt37801_lcm_unprepare(struct drm_panel *panel)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	nt37801_lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	nt37801_lcm_dcs_write_seq_static(ctx, 0x10);
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

static int nt37801_lcm_prepare(struct drm_panel *panel)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);
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
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		nt37801_lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int nt37801_lcm_enable(struct drm_panel *panel)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

//RTE60
static const struct drm_display_mode default_mode = {
	.clock = 166300,
	.hdisplay = HACT_FHDP,
	.hsync_start = HACT_FHDP + MODE1_HFP,
	.hsync_end = HACT_FHDP + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_FHDP + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_FHDP,
	.vsync_start = VACT_FHDP + MODE1_VFP,
	.vsync_end = VACT_FHDP + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_FHDP + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

//RTE80
static const struct drm_display_mode performance_mode = {
	.clock = 221300,
	.hdisplay = HACT_FHDP,
	.hsync_start = HACT_FHDP + MODE1_HFP,
	.hsync_end = HACT_FHDP + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_FHDP + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_FHDP,
	.vsync_start = VACT_FHDP + MODE1_VFP,
	.vsync_end = VACT_FHDP + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_FHDP + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

//RTE120
static const struct drm_display_mode performance_mode1 = {
	.clock = 332600,
	.hdisplay = HACT_FHDP,
	.hsync_start = HACT_FHDP + MODE1_HFP,
	.hsync_end = HACT_FHDP + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_FHDP + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_FHDP,
	.vsync_start = VACT_FHDP + MODE1_VFP,
	.vsync_end = VACT_FHDP + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_FHDP + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
//RTE60
static struct mtk_panel_params ext_params = {
	.pll_clk = 550,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70200,
	.physical_height_um = 152100,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 1080,
		.chunk_size = 1080,
		.xmit_delay = 512,
		.dec_delay = 796,
		.scale_value = 32,
		.increment_interval = 382,
		.decrement_interval = 15,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 1085,
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
	.data_rate = DATA_RATE,

	/*Msync 2.0*/
	.msync2_enable = 1,
	.msync_cmd_table = {
		.te_type = REQUEST_TE,
		.msync_max_fps = 120,
		.msync_min_fps = 37,
		.msync_level_num = 20,
		.delay_frame_num = 2,
		.request_te_tb = {

			/* Request-TE level */
			.rte_te_level[0] = {
				.level_id = 0,
				.level_fps = 60,
				.max_fps = 60,
				.min_fps = 43,
			},
			.rte_te_level[1] = {
				.level_id = 1,
				.level_fps = 80,
				.max_fps = 80,
				.min_fps = 53,
			},
			.rte_te_level[2] = {
				.level_id = 3,
				.level_fps = 120,
				.max_fps = 120,
				.min_fps = 68,
			},
		},
	},

};

//RTE80
//RTE120
static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 680,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70200,
	.physical_height_um = 152100,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 1080,
		.chunk_size = 1080,
		.xmit_delay = 512,
		.dec_delay = 796,
		.scale_value = 32,
		.increment_interval = 382,
		.decrement_interval = 15,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 1085,
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
	.data_rate = DATA_RATE,

	/*Msync 2.0*/
	.msync2_enable = 1,
	.msync_cmd_table = {
		.te_type = REQUEST_TE,
		.msync_max_fps = 120,
		.msync_min_fps = 37,
		.msync_level_num = 20,
		.delay_frame_num = 2,
		.request_te_tb = {

			/* Request-TE level */
			.rte_te_level[0] = {
				.level_id = 0,
				.level_fps = 60,
				.max_fps = 60,
				.min_fps = 43,
			},
			.rte_te_level[1] = {
				.level_id = 1,
				.level_fps = 80,
				.max_fps = 80,
				.min_fps = 53,
			},
			.rte_te_level[2] = {
				.level_id = 3,
				.level_fps = 120,
				.max_fps = 120,
				.min_fps = 68,
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
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE_2_FPS)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE_3_FPS)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	if (!ret)
		current_fps = drm_mode_vrefresh(m);

	return ret;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode)
{
	int ret = 0;

	if (mode == 0)
		*ext_param = &ext_params;
	else if (mode == 1)
		*ext_param = &ext_params;
	else if (mode == 2)
		*ext_param = &ext_params;
	else if (mode == 3)
		*ext_param = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);

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

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	unsigned int dbv = level << 1;

	if (level) {
		bl_tb0[1] = (dbv >> 8) & 0xFF;
		bl_tb0[2] = dbv & 0xFF;
	}
	bl_tb[1] = (dbv >> 8) & 0xFF;
	bl_tb[2] = dbv & 0xFF;

	if (!cb)
		return -1;
	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	return 0;
}

//RTE60
static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
	} else if (stage == AFTER_DSI_POWERON) {
		DDPINFO("%s:%d switch to 60fps\n", __func__, __LINE__);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBE, 0x08);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBE, 0x45);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC5, 0x00, 0x0B, 0x0B, 0x0B);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x90, 0x13, 0x03);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x91, 0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00,
				0x03, 0x1C, 0x01, 0x7E, 0x00, 0x0F, 0x08, 0xBB,
				0x04, 0x3D, 0x10, 0xF0);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x1F, 0x70);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x5A, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x09, 0x5F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x08, 0x08);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x09);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x5F, 0x5F);

		/*RTE*/
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x13);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x02, 0x64);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x1F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x38);
		/* AFP */
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x26);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x0A, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x20);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x02, 0x00);
		/* Valid TE period */
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x07);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x12, 0xBC);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x1C, 0xB0);

		nt37801_lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);

		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x0E);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x6B, 0x00, 0x14,
				0x09, 0xAC, 0x10);
		/* EM pluse = 8*/
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x13);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x47);
	}
}

//RTE80
static void mode_switch_to_80(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
	} else if (stage == AFTER_DSI_POWERON) {
		DDPINFO("%s:%d switch to 80fps\n", __func__, __LINE__);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBE, 0x08);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBE, 0x45);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC5, 0x00, 0x0B, 0x0B, 0x0B);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x90, 0x13, 0x03);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x91, 0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00,
				0x03, 0x1C, 0x01, 0x7E, 0x00, 0x0F, 0x08, 0xBB,
				0x04, 0x3D, 0x10, 0xF0);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x1F, 0x70);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x5A, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x09, 0x5F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x08, 0x08);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x09);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x5F, 0x5F);


		/*RTE*/
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x13);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x02, 0x64);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x1F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x38);
		/* AFP */
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x26);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x0A, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x20);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x02, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x07);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x0D, 0xF8);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x17, 0xEC);

		nt37801_lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);

		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x0E);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x6B, 0x00,
				0x14, 0x04, 0xE4, 0x10);
		/* EM pluse = 6*/
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x13);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x45);
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {

	} else if (stage == AFTER_DSI_POWERON) {
		DDPINFO("%s:%d switch to 90fps\n", __func__, __LINE__);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBE, 0x08);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBE, 0x45);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC5, 0x00, 0x0B, 0x0B, 0x0B);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x90, 0x13, 0x03);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x91, 0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00,
				0x03, 0x1C, 0x01, 0x7E, 0x00, 0x0F, 0x08, 0xBB,
				0x04, 0x3D, 0x10, 0xF0);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x1F, 0x70);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x5A, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x09, 0x5F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x08, 0x08);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x09);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x5F, 0x5F);

		/*RTE*/
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x13);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x02, 0x64);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x1F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x38);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x26);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x07, 0xD0);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x20);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x02, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x0F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x09, 0x61);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x09, 0xC4);

		nt37801_lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x5F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);
	}
}

//RTE120
static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
	} else if (stage == AFTER_DSI_POWERON) {
		DDPINFO("%s:%d switch to 120fps\n", __func__, __LINE__);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBE, 0x08);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xBE, 0x45);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC5, 0x00, 0x0B, 0x0B, 0x0B);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x90, 0x13, 0x03);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x91, 0xAB, 0x28, 0x00, 0x0C, 0xC2, 0x00,
				0x03, 0x1C, 0x01, 0x7E, 0x00, 0x0F, 0x08, 0xBB,
				0x04, 0x3D, 0x10, 0xF0);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x1F, 0x70);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x5A, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x09, 0x5F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x08, 0x08);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x09);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xB2, 0x5F, 0x5F);

		/*RTE*/
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x13);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x02, 0x64);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x1F);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x38);
		/* AFP */
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x26);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x0A, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x20);
		nt37801_lcm_dcs_write_seq_static(ctx, 0xC0, 0x02, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x07);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x09, 0x2C);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x40, 0x13, 0x20);

		nt37801_lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
		nt37801_lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
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
		DDPMSG("%s:%d switch to 60fps\n", __func__, __LINE__);
	} else if (drm_mode_vrefresh(m) == MODE_1_FPS) { /*switch to 90 */
		mode_switch_to_80(panel, stage);
		DDPMSG("%s:%d switch to 80fps\n", __func__, __LINE__);
	} else if (drm_mode_vrefresh(m) == MODE_2_FPS) { /*switch to 90 */
		mode_switch_to_90(panel, stage);
		DDPMSG("%s:%d switch to 90fps\n", __func__, __LINE__);
	} else if (drm_mode_vrefresh(m) == MODE_3_FPS) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
		DDPMSG("%s:%d switch to 120fps\n", __func__, __LINE__);
	} else
		ret = 1;

	return ret;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct nt37801_lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x01, 0x25, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_notice("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] && data[1] == id[1] && data[2] == id[2])
		return 1;

	pr_notice("ATA expect read data is %x %x %x\n", id[0], id[1], id[2]);

	return 0;
}

struct mtk_panel_para_table request_te_cmd[] = {
	{2, {0x3F, 0x00}},
};

//RTE60
struct mtk_panel_para_table msync_level_60[] = {
	{6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},

	/*RTE*/
	/* EM cycle 612 */
	{2, {0x6F, 0x13}},
	{3, {0xC0, 0x02, 0x64}},
	/* XXXX */
	{2, {0x6F, 0x1F}},
	{2, {0xC0, 0x38}},
	/* AFP */
	{2, {0x6F, 0x26}},
	{3, {0xC0, 0x0A, 0x00}},
	/* protection period */
	{2, {0x6F, 0x20}},
	{3, {0xC0, 0x02, 0x00}},
	/* Valid TE period */
	{2, {0x40, 0x07}},
	{2, {0x6F, 0x01}},
	{3, {0x40, 0x12, 0xBC}},
	{2, {0x6F, 0x03}},
	{3, {0x40, 0x1C, 0xB0}},

	/* 3.5 Frame Rate 120Hz GIR OFF */
	{2, {0x2F, 0x12}},//02 60fps

	{6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{2, {0x6F, 0x0E}},
	{8, {0xBA, 0x00, 0x6B, 0x00, 0x14, 0x09, 0xAC, 0x10}},
	/* EM pluse = 8*/
	{2, {0x6F, 0x13}},
	{2, {0xB2, 0x47}},
};

//RTE80
struct mtk_panel_para_table msync_level_80[] = {
	{6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},

	/*RTE*/
	/* EM cycle 612 */
	{2, {0x6F, 0x13}},
	{3, {0xC0, 0x02, 0x64}},
	/* XXXX */
	{2, {0x6F, 0x1F}},
	{2, {0xC0, 0x38}},
	/* AFP */
	{2, {0x6F, 0x26}},
	{3, {0xC0, 0x0A, 0x00}},
	/* protection period */
	{2, {0x6F, 0x20}},
	{3, {0xC0, 0x02, 0x00}},
	/* Valid TE period */
	{2, {0x40, 0x07}},
	{2, {0x6F, 0x01}},
	{3, {0x40, 0x0D, 0xF8}},
	{2, {0x6F, 0x03}},
	{3, {0x40, 0x17, 0xEC}},

	/* 3.5 Frame Rate 120Hz GIR OFF */
	{2, {0x2F, 0x02}},//02 60fps
	/*{0x26, 01, {0x00}},*/

	/* VFP & EM pluse */
	{6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{2, {0x6F, 0x0E}},
	{8, {0xBA, 0x00, 0x6B, 0x00, 0x14, 0x04, 0xE4, 0x10}},
	{2, {0x6F, 0x13}},
	{2, {0xB2, 0x45}},

	/* Enable RTE */
	{2, {0x2F, 0x12}},
};

struct mtk_panel_para_table msync_level_90[] = {
	/*RTE*/
	/* EM cycle 612 */
	{2, {0x6F, 0x13}},
	{3, {0xC0, 0x02, 0x64}},
	/* XXXX */
	{2, {0x6F, 0x1F}},
	{2, {0xC0, 0x38}},
	/* AFP */
	{2, {0x6F, 0x26}},
	{3, {0xC0, 0x07, 0xD0}},
	/* protection period */
	{2, {0x6F, 0x20}},
	{3, {0xC0, 0x02, 0x00}},
	/* Valid TE period */
	{2, {0x40, 0x0F}},
	{2, {0x6F, 0x01}},
	/*{0x40, 02, {0x13, 0x0D}},*/
	{3, {0x40, 0x09, 0x61}},
	{2, {0x6F, 0x03}},
	/*{0x40, 02, {0x17, 0x08}},*/
	{3, {0x40, 0x09, 0xC4}},

	{2, {0x2F, 0x12}},
};

//RTE120
struct mtk_panel_para_table msync_level_120[] = {
	{6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},

	/*RTE*/
	/* EM cycle 612 */
	{2, {0x6F, 0x13}},
	{3, {0xC0, 0x02, 0x64}},
	/* XXXX */
	{2, {0x6F, 0x1F}},
	{2, {0xC0, 0x38}},
	/* AFP */
	{2, {0x6F, 0x26}},
	{3, {0xC0, 0x0A, 0x00}},
	/* protection period */
	{2, {0x6F, 0x20}},
	{3, {0xC0, 0x02, 0x00}},
	/* Valid TE period */
	{2, {0x40, 0x07}},
	{2, {0x6F, 0x01}},
	{3, {0x40, 0x09, 0x2C}},
	{2, {0x6F, 0x03}},
	{3, {0x40, 0x13, 0x20}},

	/* 3.5 Frame Rate 120Hz GIR OFF */
	{2, {0x2F, 0x00}},//02 60fps
	{2, {0x26, 0x00}},

	{2, {0x2F, 0x10}},
};

struct mtk_panel_para_table msync_close_rte_60fps[] = {
	{2, {0x2F, 0x02}},
};

struct mtk_panel_para_table msync_close_rte_80fps[] = {
	{2, {0x2F, 0x02}},
};

struct mtk_panel_para_table msync_close_rte_90fps[] = {
	{2, {0x2F, 0x02}},
};

struct mtk_panel_para_table msync_close_rte_120fps[] = {
	{2, {0x2F, 0x00}},
};

static int msync_te_level_switch_grp(void *dsi, dcs_grp_write_gce cb,
		void *handle, struct drm_panel *panel, unsigned int fps_level)
{
	int ret = 0;
	struct mtk_panel_ext *ext = find_panel_ext(panel);

	DDPMSG("[Msync2.0]%s:%d fps_level:%d\n", __func__, __LINE__, fps_level);
	if (fps_level <= 60) { /*switch to 60 */
		DDPMSG("%s:%d switch to 60fps\n", __func__, __LINE__);
		cb(dsi, handle, msync_level_60, ARRAY_SIZE(msync_level_60));
		ext->params->pll_clk = ext_params.pll_clk;
		ext->params->data_rate = ext_params.data_rate;

	} else if (fps_level <= 80) { /*switch to 80 */
		DDPMSG("[Msync2.0]%s:%d switch to 80fps\n", __func__, __LINE__);
		cb(dsi, handle, msync_level_80, ARRAY_SIZE(msync_level_80));
		ext->params->pll_clk = ext_params.pll_clk;
		ext->params->data_rate = ext_params.data_rate;

	} else if (fps_level <= 90) { /*switch to 90 */
		DDPMSG("[Msync2.0]%s:%d switch to 90fps\n", __func__, __LINE__);
		cb(dsi, handle, msync_level_90, ARRAY_SIZE(msync_level_90));
		ext->params->pll_clk = ext_params.pll_clk;
		ext->params->data_rate = ext_params.data_rate;

	} else if (fps_level <= 120) { /*switch to 120 */
		DDPMSG("[Msync2.0]%s:%d switch to 120fps\n", __func__, __LINE__);
		cb(dsi, handle, msync_level_120, ARRAY_SIZE(msync_level_120));
		ext->params->pll_clk = ext_params_120hz.pll_clk;
		ext->params->data_rate = ext_params_120hz.data_rate;

	} else if (fps_level == 0xEEEE) { /*send request TE*/
		DDPMSG("[Msync2.0]%s:%d send request TE\n", __func__, __LINE__);
		cb(dsi, handle, request_te_cmd, ARRAY_SIZE(request_te_cmd));

	} else if (fps_level == 0xFFFF) { /*close request te */
		DDPMSG("[Msync2.0]%s:%d Close MTE done, current fps:%d\n",
			__func__, __LINE__, current_fps);
		if (current_fps == 60) {
			cb(dsi, handle, msync_close_rte_60fps, ARRAY_SIZE(msync_close_rte_60fps));
			ext->params->pll_clk = ext_params.pll_clk;
			ext->params->data_rate = ext_params.data_rate;
		} else if (current_fps == 80) {
			cb(dsi, handle, msync_close_rte_80fps, ARRAY_SIZE(msync_close_rte_80fps));
			ext->params->pll_clk = ext_params.pll_clk;
			ext->params->data_rate = ext_params.data_rate;
		} else if (current_fps == 90) {
			cb(dsi, handle, msync_close_rte_90fps, ARRAY_SIZE(msync_close_rte_90fps));
			ext->params->pll_clk = ext_params.pll_clk;
			ext->params->data_rate = ext_params.data_rate;
		} else {
			cb(dsi, handle, msync_close_rte_120fps, ARRAY_SIZE(msync_close_rte_120fps));
			ext->params->pll_clk = ext_params_120hz.pll_clk;
			ext->params->data_rate = ext_params_120hz.data_rate;
		}
		/*cb(dsi, handle, msync_default, ARRAY_SIZE(msync_default));*/
	} else
		ret = 1;

	DDPMSG("[Msync2.0]%s:%d fps_level:%d\n", __func__, __LINE__, fps_level);
	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.mode_switch = mode_switch,
	.ata_check = panel_ata_check,
	.msync_te_level_switch_grp = msync_te_level_switch_grp,
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

static int nt37801_lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode1;
	struct drm_display_mode *mode2;

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

	mode1 = drm_mode_duplicate(connector->dev, &performance_mode);
	if (!mode1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay,
			performance_mode.vdisplay,
			drm_mode_vrefresh(&performance_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode1);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode1.hdisplay,
			performance_mode1.vdisplay,
			drm_mode_vrefresh(&performance_mode1));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = nt37801_lcm_disable,
	.unprepare = nt37801_lcm_unprepare,
	.prepare = nt37801_lcm_prepare,
	.enable = nt37801_lcm_enable,
	.get_modes = nt37801_lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct nt37801_lcm *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+ samsung,nt37801,cmd,120hz\n", __func__);

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

	ctx = devm_kzalloc(dev, sizeof(struct nt37801_lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

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
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
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

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s-samsung,nt37801 cmd 120hz\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct nt37801_lcm *ctx = mipi_dsi_get_drvdata(dsi);
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
	{ .compatible = "nt37801,msync2_rte", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "nt37801_msync2_rte",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Yonggang Yu <yonggang.yu@mediatek.com>");
MODULE_DESCRIPTION("nt37801 fqhdplus dphy oled panel driver for msync2.0 RTE");
MODULE_LICENSE("GPL v2");
