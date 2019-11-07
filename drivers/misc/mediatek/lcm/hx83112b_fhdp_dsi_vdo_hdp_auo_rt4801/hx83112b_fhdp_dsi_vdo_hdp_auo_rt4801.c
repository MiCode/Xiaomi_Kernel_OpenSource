/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#  include <linux/string.h>
#  include <linux/kernel.h>
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "data_hw_roundedpattern.h"
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#  include <platform/upmu_common.h>
#  include <platform/mt_gpio.h>
#  include <platform/mt_i2c.h>
#  include <platform/mt_pmic.h>
#  include <string.h>
#elif defined(BUILD_UBOOT)
#  include <asm/arch/mt_gpio.h>
#endif

#ifdef BUILD_LK
#  define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#  define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#  define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#  define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_HX83112B 0x83
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#  include <linux/kernel.h>
#  include <linux/module.h>
#  include <linux/fs.h>
#  include <linux/slab.h>
#  include <linux/init.h>
#  include <linux/list.h>
#  include <linux/i2c.h>
#  include <linux/irq.h>
#  include <linux/uaccess.h>
#  include <linux/interrupt.h>
#  include <linux/io.h>
#  include <linux/platform_device.h>
#endif

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "lcm_i2c.h"

#define FRAME_WIDTH			(720)
#define FRAME_HEIGHT			(1440)
#define VIRTUAL_WIDTH		(1080)
#define VIRTUAL_HEIGHT	(2160)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(64500)
#define LCM_PHYSICAL_HEIGHT		(129000)
#define LCM_DENSITY			(320)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE		0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


/* i2c control start */

#define LCM_I2C_ADDR 0x3E
#define LCM_I2C_BUSNUM  1	/* for I2C channel 0 */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"


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
	{ .compatible = "mediatek,I2C_LCD_BIAS", },
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = {
	{LCM_I2C_ID_NAME, 0},
	{}
};

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect               = _lcm_i2c_detect, */
	.driver = {
		   .owner = THIS_MODULE,
		   .name = LCM_I2C_ID_NAME,
		   .of_match_table = _lcm_i2c_of_match,
		   },

};

/*****************************************************************************
 * Function
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	pr_debug("[LCM][I2C] NT: info==>name=%s addr=0x%x\n",
		client->name, client->addr);
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
/* i2c control end */

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xB9, 0x03, {0x83, 0x11, 0x2B} },
	{0xB1, 0x0A, {0xF8, 0x29, 0x29, 0x00, 0x00, 0x0F, 0x14, 0x0F,
		      0x14, 0x33} },
	{0xD2, 0x02, {0x2C, 0x2C} },
	{0xB2, 0x0B, {0x80, 0x02, 0x00, 0x80, 0x70, 0x00, 0x08, 0x1C,
		      0x09, 0x01, 0x04} },
	{0xE9, 0x01, {0xD1} },
	{0xB2, 0x02, {0x00, 0x08} },
	{0xE9, 0x01, {0x00} },
	{0xE9, 0x01, {0xCE} },
	{0xB2, 0x01, {0xA3} },
	{0xE9, 0x01, {0x00} },
	{0xBD, 0x01, {0x02} },
	{0xB2, 0x02, {0xB5, 0x0A} },
	{0xBD, 0x01, {0x00} },
	{0xDD, 0x08, {0x00, 0x00, 0x08, 0x1C, 0x09, 0x34, 0x34, 0x8B} },
	{0xB4, 0x18, {0x01, 0xD3, 0x00, 0x00, 0x00, 0x00, 0x03, 0xD0,
		      0x00, 0x00, 0x0F, 0xCB, 0x01, 0x00, 0x00, 0x13,
		      0x00, 0x2E, 0x08, 0x01, 0x12, 0x00, 0x00, 0x2E} },
	{0xBD, 0x01, {0x02} },
	{0xB4, 0x01, {0x92} },
	{0xBD, 0x01, {0x00} },
	{0xB6, 0x03, {0x81, 0x81, 0xE3} },
	{0xC0, 0x01, {0x44} },
	{0xC2, 0x01, {0x83} },
	{0xCC, 0x01, {0x08} },
	{0xBD, 0x01, {0x03} },
	{0xC1, 0x39, {0xFF, 0xFA, 0xF6, 0xF3, 0xEF, 0xEB, 0xE7, 0xE0,
		      0xDC, 0xD9, 0xD6, 0xD2, 0xCF, 0xCB, 0xC7, 0xC3,
		      0xBF, 0xBB, 0xB7, 0xB0, 0xA8, 0xA1, 0x9A, 0x92,
		      0x89, 0x81, 0x7A, 0x73, 0x6B, 0x63, 0x5A, 0x51,
		      0x48, 0x40, 0x38, 0x31, 0x29, 0x20, 0x16, 0x0D,
		      0x09, 0x07, 0x05, 0x02, 0x00, 0x08, 0x2E, 0xF6,
		      0x20, 0x18, 0x94, 0xF8, 0x6F, 0x59, 0x18, 0xFC,
		      0x00} },
	{0xBD, 0x01, {0x02} },
	{0xC1, 0x39, {0xFF, 0xFA, 0xF6, 0xF3, 0xEF, 0xEB, 0xE7, 0xE0,
		      0xDC, 0xD9, 0xD6, 0xD2, 0xCF, 0xCB, 0xC7, 0xC3,
		      0xBF, 0xBB, 0xB7, 0xB0, 0xA8, 0xA1, 0x9A, 0x92,
		      0x89, 0x81, 0x7A, 0x73, 0x6B, 0x63, 0x5A, 0x51,
		      0x48, 0x40, 0x38, 0x31, 0x29, 0x20, 0x16, 0x0D,
		      0x09, 0x07, 0x05, 0x02, 0x00, 0x08, 0x2E, 0xF6,
		      0x20, 0x18, 0x94, 0xF8, 0x6F, 0x59, 0x18, 0xFC,
		      0x00} },
	{0xBD, 0x01, {0x01} },
	{0xC1, 0x39, {0xFF, 0xFA, 0xF6, 0xF3, 0xEF, 0xEB, 0xE7, 0xE0,
		      0xDC, 0xD9, 0xD6, 0xD2, 0xCF, 0xCB, 0xC7, 0xC3,
		      0xBF, 0xBB, 0xB7, 0xB0, 0xA8, 0xA1, 0x9A, 0x92,
		      0x89, 0x81, 0x7A, 0x73, 0x6B, 0x63, 0x5A, 0x51,
		      0x48, 0x40, 0x38, 0x31, 0x29, 0x20, 0x16, 0x0D,
		      0x09, 0x07, 0x05, 0x02, 0x00, 0x08, 0x2E, 0xF6,
		      0x20, 0x18, 0x94, 0xF8, 0x6F, 0x59, 0x18, 0xFC,
		      0x00} },
	{0xBD, 0x01, {0x00} },
	{0xC1, 0x01, {0x01} },
	{0xD3, 0x16, {0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x0A,
		      0x0A, 0x07, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
		      0x08, 0x32, 0x10, 0x08, 0x00, 0x08} },
	{0xE9, 0x01, {0xE3} },
	{0xD3, 0x03, {0x05, 0x08, 0x86} },
	{0xE9, 0x01, {0x00} },
	{0xBD, 0x01, {0x01} },
	{0xE9, 0x01, {0xC8} },
	{0xD3, 0x01, {0x81} },
	{0xE9, 0x01, {0x00} },
	{0xBD, 0x01, {0x00} },
	{0xD5, 0x30, {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		      0x31, 0x31, 0x30, 0x30, 0x2F, 0x2F, 0x31, 0x31,
		      0x30, 0x30, 0x2F, 0x2F, 0xC0, 0x18, 0x40, 0x40,
		      0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
		      0x21, 0x20, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18,
		      0x03, 0x03, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18} },
	{0xD6, 0x30, {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		      0x31, 0x31, 0x30, 0x30, 0x2F, 0x2F, 0x31, 0x31,
		      0x30, 0x30, 0x2F, 0x2F, 0xC0, 0x18, 0x40, 0x40,
		      0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01,
		      0x20, 0x21, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19,
		      0x20, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18} },
	{0xD8, 0x18, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },
	{0xBD, 0x01, {0x01} },
	{0xD8, 0x18, {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
		      0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
		      0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA} },
	{0xBD, 0x01, {0x02} },
	{0xD8, 0x0C, {0xAF, 0xFF, 0xFA, 0xAA, 0xBA, 0xAA, 0xAA, 0xFF,
		      0xFA, 0xAA, 0xBA, 0xAA} },
	{0xBD, 0x01, {0x03} },
	{0xD8, 0x18, {0xAA, 0xAA, 0xAB, 0xAA, 0xAE, 0xAA, 0xAA, 0xAA,
		      0xAB, 0xAA, 0xAE, 0xAA, 0xAA, 0xFF, 0xFA, 0xAA,
		      0xBA, 0xAA, 0xAA, 0xFF, 0xFA, 0xAA, 0xBA, 0xAA} },
	{0xBD, 0x01, {0x00} },
	{0xE7, 0x19, {0x09, 0x09, 0x00, 0x07, 0xE6, 0x00, 0x27, 0x00,
		      0x07, 0x00, 0x00, 0xE6, 0x2A, 0x00, 0xE6, 0x00,
		      0x0A, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x12,
		      0x04} },
	{0xE9, 0x01, {0xE4} },
	{0xE7, 0x02, {0x17, 0x69} },
	{0xE9, 0x01, {0x00} },
	{0xBD, 0x01, {0x01} },
	{0xE7, 0x09, {0x00, 0x00, 0x01, 0x20, 0x01, 0x0E, 0x08, 0xEE,
		      0x09} },
	{0xBD, 0x01, {0x02} },
	{0xE7, 0x03, {0x20, 0x20, 0x00} },
	{0xBD, 0x01, {0x03} },
	{0xE7, 0x06, {0x00, 0x08, 0x01, 0x00, 0x00, 0x20} },
	{0xE9, 0x01, {0xC9} },
	{0xE7, 0x02, {0x2E, 0xCB} },
	{0xE9, 0x01, {0x00} },
	{0xBD, 0x01, {0x00} },
	{0xD1, 0x01, {0x27} },
	{0xBD, 0x01, {0x01} },
	{0xE9, 0x01, {0xC2} },
	{0xCB, 0x01, {0x27} },
	{0xE9, 0x01, {0x00} },
	{0xBD, 0x01, {0x00} },
	{0x51, 0x02, {0x0F, 0xFF} },
	{0x53, 0x01, {0x24} },
	{0x55, 0x01, {0x00} },
	{0x35, 0x01, {0x00} },
	{0x44, 2, {0x08, 0x66} }, /* set TE event @ line 0x866(2150) */

	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },

	{0xE9, 0x01, {0xC2} },
	{0xB0, 0x01, {0x01} },
	{0xE9, 0x01, {0x00} },

	{0x29, 0, {} },
	{REGFLAG_DELAY, 20, {} },
};

static struct LCM_setting_table
__maybe_unused lcm_deep_sleep_mode_in_setting[] = {
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 50, {} },
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 150, {} },
};

static struct LCM_setting_table __maybe_unused lcm_sleep_out_setting[] = {
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 50, {} },
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;
		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
			break;
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->virtual_width = VIRTUAL_WIDTH;
	params->virtual_height = VIRTUAL_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	LCM_LOGI("%s: lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 20;
	params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = VIRTUAL_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = VIRTUAL_WIDTH;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 488;
	params->dsi.PLL_CK_CMD = 480;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9d;

	/* for ARR 2.0 */
	params->max_refresh_rate = 60;
	params->min_refresh_rate = 45;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->corner_pattern_height = ROUND_CORNER_H_TOP;
	params->corner_pattern_height_bot = ROUND_CORNER_H_BOT;
	params->corner_pattern_tp_size = sizeof(top_rc_pattern);
	params->corner_pattern_lt_addr = (void *)top_rc_pattern;
#endif
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	if (lcm_util.set_gpio_lcd_enp_bias) {
		lcm_util.set_gpio_lcd_enp_bias(1);

		_lcm_i2c_write_bytes(0x0, 0xf);
		_lcm_i2c_write_bytes(0x1, 0xf);
	} else
		LCM_LOGI("set_gpio_lcd_enp_bias not defined...\n");
}

static void lcm_suspend_power(void)
{
	SET_RESET_PIN(0);
	if (lcm_util.set_gpio_lcd_enp_bias)
		lcm_util.set_gpio_lcd_enp_bias(0);
	else
		LCM_LOGI("set_gpio_lcd_enp_bias not defined...\n");
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	SET_RESET_PIN(0);
	lcm_init_power();
}

static void lcm_init(void)
{
	SET_RESET_PIN(0);
	MDELAY(15);
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);

	push_table(NULL, init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
	LCM_LOGI("hx83112b_fhdp----tps6132----lcm mode = vdo mode :%d----\n",
		 lcm_dsi_mode);
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting,
		   ARRAY_SIZE(lcm_suspend_setting), 1);
}

static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int id[3] = {0x83, 0x11, 0x2B};
	unsigned int data_array[3];
	unsigned char read_buf[3];

	data_array[0] = 0x00033700; /* set max return size = 3 */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x04, read_buf, 3); /* read lcm id */

	LCM_LOGI("ATA read = 0x%x, 0x%x, 0x%x\n",
		 read_buf[0], read_buf[1], read_buf[2]);

	if ((read_buf[0] == id[0]) &&
	    (read_buf[1] == id[1]) &&
	    (read_buf[2] == id[2]))
		ret = 1;
	else
		ret = 0;

	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	LCM_LOGI("%s,hx83112b backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
	unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

#ifdef LCM_SET_DISPLAY_ON_DELAY
	lcm_set_display_on();
#endif

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}
static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[1];
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00013700;  /* read id return 1byte */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xDA, buffer, 1);
	id = buffer[0];     /* we only need ID */

	LCM_LOGI("%s,hx83112b id = 0x%08x\n", __func__, id);

	if (id == LCM_ID_HX83112B)
		return 1;
	else
		return 0;

}

struct LCM_DRIVER hx83112b_fhdp_dsi_vdo_hdp_auo_rt4801_lcm_drv = {
	.name = "hx83112b_fhdp_dsi_vdo_hdp_auo_rt4801_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.compare_id = lcm_compare_id,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
};

