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


#define DATA_RATE		1360
#define DYN_DATA_RATE		650
#define FHDP_120HZ_DYN_RATE	750
#define FHDP_60HZ_DYN_RATE	510
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

static atomic_t current_backlight;

#define SUPPORT_RES_SWITCH 1
#define PANEL_2K 1


struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;
	bool wqhd_en;
	unsigned int dynamic_fps;
	unsigned int gate_ic;
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


static struct LCM_setting_table mode_120hz_setting[] = {
	/* frequency select 120hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* TE fixed */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x01, 0xBD}},
	{0xBD, 01, {0x83}},
	{0xB0, 03, {0x00, 0x2D, 0xBD}},
	{0xBD, 01, {0x04}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting[] = {
	/* frequency select 60hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x08}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* TE fixed */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x01, 0xBD}},
	{0xBD, 01, {0x83}},
	{0xB0, 03, {0x00, 0x2D, 0xBD}},
	{0xBD, 01, {0x04}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_wqhd_setting[] = {
	/* WQHD DSC setting */
	{0x9D, 01, {0x01}},
	{0x9E, 120, {0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
			0x05, 0xA0, 0x00, 0x19, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x02, 0xBE,
			0x00, 0x0A, 0x00, 0x0C, 0x04, 0x00, 0x03, 0x0D,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x05, 0x9F}},
	{0x2B, 04, {0x00, 0x00, 0x0c, 0x7F}},
	/* scaler setting WQHD */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x4C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* VLIN1 set 7.6V */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0A, 0xB1}},
	{0xB1, 01, {0x38}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* dimming setting */
	{0xF0, 02, {0x5A, 0x5A}},
	/* dimming speed setting */
	{0xB0, 01, {0x0E}},
	{0x94, 01, {0x28}},
	/* elvss dim setting */
	{0xB0, 01, {0x0D}},
	{0x94, 01, {0x60}},
	{0x53, 01, {0x28}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* FFC setting */
	{0xFC, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x2A, 0xC5}},
	{0xC5, 28, {0x11, 0x10, 0x50, 0x05, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C}},
	{0xFC, 02, {0xA5, 0xA5}},

	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x4C, 0xF4}},
	{0xF4, 01, {0xF0}},
	{0xE5, 01, {0x1D}},
	{0xED, 03, {0x45, 0x4C, 0x20}},
	{0xF0, 02, {0xA5, 0xA5}},

	{0x29, 01, {0x00}},

	/* TE fixed */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x01, 0xBD}},
	{0xBD, 01, {0x83}},
	{0xB0, 03, {0x00, 0x2D, 0xBD}},
	{0xBD, 01, {0x04}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_fhdp_setting[] = {
	/* FHDP DSC setting */
	{0x9D, 01, {0x01}},
	{0x9E, 120, {0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
			0x04, 0x38, 0x00, 0x20, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0x15,
			0x00, 0x07, 0x00, 0x0C, 0x03, 0x19, 0x03, 0x2E,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x05, 0x9F}},
	{0x2B, 04, {0x00, 0x00, 0x0c, 0x7F}},
	/* scaler setting FHDP */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x4D}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* VLIN1 set 7.6V */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0A, 0xB1}},
	{0xB1, 01, {0x38}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* dimming setting */
	{0xF0, 02, {0x5A, 0x5A}},
	/* dimming speed setting */
	{0xB0, 01, {0x0E}},
	{0x94, 01, {0x28}},
	/* elvss dim setting */
	{0xB0, 01, {0x0D}},
	{0x94, 01, {0x60}},
	{0x53, 01, {0x28}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* FFC setting */
	{0xFC, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x2A, 0xC5}},
	{0xC5, 28, {0x11, 0x10, 0x50, 0x05, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C}},
	{0xFC, 02, {0xA5, 0xA5}},

	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x4C, 0xF4}},
	{0xF4, 01, {0xF0}},
	{0xE5, 01, {0x1D}},
	{0xED, 03, {0x45, 0x4C, 0x20}},
	{0xF0, 02, {0xA5, 0xA5}},

	{0x29, 01, {0x00}},

	/* TE fixed */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x01, 0xBD}},
	{0xBD, 01, {0x83}},
	{0xB0, 03, {0x00, 0x2D, 0xBD}},
	{0xBD, 01, {0x04}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i, j;
	unsigned char temp[255] = {0};
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		memset(temp, 0, sizeof(temp));
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			temp[0] = cmd;
			for (j = 0; j < table[i].count; j++)
				temp[j+1] = table[i].para_list[j];

			lcm_dcs_write(ctx, temp, table[i].count+1);
		}
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	unsigned int level = 0;

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
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

	if (ctx->wqhd_en)
		push_table(ctx, mode_wqhd_setting,
			sizeof(mode_wqhd_setting) / sizeof(struct LCM_setting_table));
	else
		push_table(ctx, mode_fhdp_setting,
			sizeof(mode_fhdp_setting) / sizeof(struct LCM_setting_table));

	level = atomic_read(&current_backlight);
	bl_tb[1] = (level >> 8) & 0x7;
	bl_tb[2] = level & 0xFF;
	lcm_dcs_write(ctx, bl_tb, ARRAY_SIZE(bl_tb));

	if (ctx->dynamic_fps == 60) {
		push_table(ctx, mode_60hz_setting,
			sizeof(mode_60hz_setting) / sizeof(struct LCM_setting_table));
	} else if (ctx->dynamic_fps == 120) {
		push_table(ctx, mode_120hz_setting,
			sizeof(mode_120hz_setting) / sizeof(struct LCM_setting_table));
	}

	pr_info("%s- wqhd:%d,mode:%u,bl:%u\n",
		__func__, ctx->wqhd_en, ctx->dynamic_fps, level);
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
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
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

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

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

#if (SUPPORT_RES_SWITCH)
static const struct drm_display_mode wqhd_120hz_mode = {
	.clock = 582000,
	.hdisplay = HACT_WQHD,
	.hsync_start = HACT_WQHD + MODE1_HFP,
	.hsync_end = HACT_WQHD + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_WQHD + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_WQHD,
	.vsync_start = VACT_WQHD + MODE1_VFP,
	.vsync_end = VACT_WQHD + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_WQHD + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

static const struct drm_display_mode wqhd_60hz_mode = {
	.clock = 288750,
	.hdisplay = HACT_WQHD,
	.hsync_start = HACT_WQHD + MODE1_HFP,
	.hsync_end = HACT_WQHD + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_WQHD + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_WQHD,
	.vsync_start = VACT_WQHD + MODE1_VFP,
	.vsync_end = VACT_WQHD + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_WQHD + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};
#endif

static const struct drm_display_mode default_mode_fhdp = {
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

#if (SUPPORT_RES_SWITCH)
static const struct drm_display_mode performence_mode_fhdp = {
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
#endif


#if defined(CONFIG_MTK_PANEL_EXT)

#if (SUPPORT_RES_SWITCH)
static struct mtk_panel_params ext_params = {
	.pll_clk = 680,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9f,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69552,
	.physical_height_um = 154560,
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
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 702,
		.decrement_interval = 10,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1024,
		.slice_bpg_offset = 781,
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
	.dyn_fps = {
		.data_rate = DYN_DATA_RATE,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DYN_DATA_RATE + 10,
	},
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 680,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9f,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69552,
	.physical_height_um = 154560,
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
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 702,
		.decrement_interval = 10,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1024,
		.slice_bpg_offset = 781,
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
	.dyn_fps = {
		.data_rate = DATA_RATE,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE + 10,
	},
};
#endif

static struct mtk_panel_params ext_params_fhdp = {
	.pll_clk = 680,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9f,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69552,
	.physical_height_um = 154560,
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
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 789,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 793,
		.slice_bpg_offset = 814,
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
	.dyn_fps = {
		.data_rate = FHDP_60HZ_DYN_RATE,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = FHDP_60HZ_DYN_RATE + 10,
	},
};

#if (SUPPORT_RES_SWITCH)
static struct mtk_panel_params ext_params_120hz_fhdp = {
	.pll_clk = 680,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9f,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69552,
	.physical_height_um = 154560,
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
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 789,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 793,
		.slice_bpg_offset = 814,
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
	.dyn_fps = {
		.data_rate = FHDP_120HZ_DYN_RATE,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = FHDP_120HZ_DYN_RATE + 10,
	},
};
#endif

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
	if (!m) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}

	pr_info("%s thh drm_mode_vrefresh = %d, m->hdisplay = %d\n",
		__func__, drm_mode_vrefresh(m), m->hdisplay);

#if (SUPPORT_RES_SWITCH)
	if (drm_mode_vrefresh(m) == MODE0_FPS && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE1_FPS && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE2_FPS && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_fhdp;
	else if (drm_mode_vrefresh(m) == MODE3_FPS && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_120hz_fhdp;
	else
		ret = 1;
#else
	ext->params = &ext_params_fhdp;
#endif

	return ret;
}

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

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};

	bl_tb[1] = (level >> 8) & 0x7;
	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));

	atomic_set(&current_backlight, level);

	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
	return 0;
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		push_table(ctx, mode_120hz_setting,
			sizeof(mode_120hz_setting) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 120;
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		push_table(ctx, mode_60hz_setting,
			sizeof(mode_60hz_setting) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 60;
	}
}

static void mode_switch_to_wqhd(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		push_table(ctx, mode_wqhd_setting,
			sizeof(mode_wqhd_setting) / sizeof(struct LCM_setting_table));
	}
	ctx->wqhd_en = true;
}

static void mode_switch_to_fhdp(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		push_table(ctx, mode_fhdp_setting,
			sizeof(mode_fhdp_setting) / sizeof(struct LCM_setting_table));
	}
	ctx->wqhd_en = false;
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	bool isFpsChange = false;
	bool isResChange = false;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);

	if (cur_mode == dst_mode)
		return ret;
	if (!m_dst || !m_cur) {
		pr_info("%s m_dst or m_cur is NULL\n", __func__);
		return ret;
	}
	isFpsChange = drm_mode_vrefresh(m_dst) == drm_mode_vrefresh(m_cur) ? false : true;
	isResChange = m_dst->vdisplay == m_cur->vdisplay ? false : true;

	pr_info("%s isFpsChange = %d, isResChange = %d\n", __func__, isFpsChange, isResChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_dst), m_dst->vdisplay, m_dst->hdisplay);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_cur), m_cur->vdisplay, m_cur->hdisplay);

	if (isResChange) {
		if (m_dst->hdisplay == HACT_WQHD && m_dst->vdisplay == VACT_WQHD)
			mode_switch_to_wqhd(panel, stage);
		else if (m_dst->hdisplay == HACT_FHDP && m_dst->vdisplay == VACT_FHDP)
			mode_switch_to_fhdp(panel, stage);
		else
			ret |= 1;

		isFpsChange = 1;//if change res, need config fps

		pr_info("%s: if change res, need config fps\n", __func__);
	}

	if (isFpsChange) {
		if (drm_mode_vrefresh(m_dst) == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE1_FPS)
			mode_switch_to_120(panel, stage);
		else
			ret |= 1;
	}
	return ret;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x01, 0x25, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_info("%s error\n", __func__);
		return 0;
	}

	pr_notice("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] && data[1] == id[1] && data[2] == id[2])
		return 1;

	pr_notice("ATA expect read data is %x %x %x\n", id[0], id[1], id[2]);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.mode_switch = mode_switch,
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


static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
#if (SUPPORT_RES_SWITCH)
	struct drm_display_mode *mode0, *mode1, *mode2, *mode3;
#else
	struct drm_display_mode *mode2;
#endif

#if (SUPPORT_RES_SWITCH)
	mode0 = drm_mode_duplicate(connector->dev, &wqhd_120hz_mode);
	if (!mode0) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			wqhd_120hz_mode.hdisplay, wqhd_120hz_mode.vdisplay,
			drm_mode_vrefresh(&wqhd_120hz_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode0);
	mode0->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode0);

	mode1 = drm_mode_duplicate(connector->dev, &wqhd_60hz_mode);
	if (!mode1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			wqhd_60hz_mode.hdisplay, wqhd_60hz_mode.vdisplay,
			drm_mode_vrefresh(&wqhd_60hz_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);
#endif

	mode2 = drm_mode_duplicate(connector->dev, &default_mode_fhdp);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode_fhdp.hdisplay, default_mode_fhdp.vdisplay,
			drm_mode_vrefresh(&default_mode_fhdp));
		return -ENOMEM;
	}
	drm_mode_set_name(mode2);
#if (SUPPORT_RES_SWITCH)
	mode2->type = DRM_MODE_TYPE_DRIVER;
#else
	mode2->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
#endif
	drm_mode_probed_add(connector, mode2);

#if (SUPPORT_RES_SWITCH)
	mode3 = drm_mode_duplicate(connector->dev, &performence_mode_fhdp);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode_fhdp.hdisplay, performence_mode_fhdp.vdisplay,
			drm_mode_vrefresh(&performence_mode_fhdp));
		return -ENOMEM;
	}
	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);
#endif

	connector->display_info.width_mm = 71;
	connector->display_info.height_mm = 153;

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
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+ samsung,s6e3hae,cmd,120hz\n", __func__);

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
		dev_info(dev, "%s: cannot get reset-gpios %ld\n",
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

	atomic_set(&current_backlight, 4095);
	ctx->prepared = true;
	ctx->enabled = true;
	ctx->dynamic_fps = 120;
	ctx->wqhd_en = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_handle_reg(&ctx->panel);
#if (PANEL_2K)
	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
#else
	ret = mtk_panel_ext_create(dev, &ext_params_fhdp, &ext_funcs, &ctx->panel);
#endif
	if (ret < 0)
		return ret;
#endif

	pr_info(" %s-\n", __func__);

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
	{ .compatible = "samsung,s6e3hae_wqhd_dphy_120hz_cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "s6e3hae_wqhd_dphy_120hz_cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("samsung, s6e3hae wqhd dphy oled panel driver");
MODULE_LICENSE("GPL v2");

