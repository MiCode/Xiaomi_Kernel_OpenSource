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

#define LCM_ID_nt36672c 0x83
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

#define FRAME_WIDTH			(1080)
#define FRAME_HEIGHT			(2400)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(64500)
#define LCM_PHYSICAL_HEIGHT		(129000)
#define LCM_DENSITY			(480)

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

// static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
// {
	// int ret = 0;
	// struct i2c_client *client = _lcm_i2c_client;
	// char write_data[2] = { 0 };

	// if (client == NULL) {
		// pr_debug("ERROR!! _lcm_i2c_client is null\n");
		// return 0;
	// }

	// write_data[0] = addr;
	// write_data[1] = value;
	// ret = i2c_master_send(client, write_data, 2);
	// if (ret < 0)
		// pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	// return ret;
// }

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
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	//DSC on
	{0xC0, 1, {0x03} },
	{0xC1, 16,
		{0x89, 0x28, 0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E,
		 0x00, 0x2B, 0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7} },
	{0xC2, 2, {0x1B, 0xA0} },

	{0xFF, 1, {0x20} },
	{0xFB, 1, {0x01} },
	{0x01, 1, {0x66} },
	{0x32, 1, {0x4D} },
	{0x69, 1, {0xD1} },
	{0xF2, 1, {0x64} },
	{0xF4, 1, {0x64} },
	{0xF6, 1, {0x64} },
	{0xF9, 1, {0x64} },

	{0xFF, 1, {0x26} },
	{0xFB, 1, {0x01} },
	{0x81, 1, {0x0E} },
	{0x84, 1, {0x03} },
	{0x86, 1, {0x03} },
	{0x88, 1, {0x07} },

	{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },
	{0xE3, 1, {0x01} },
	{0xE4, 1, {0xEC} },
	{0xE5, 1, {0x02} },
	{0xE6, 1, {0xE3} },
	{0xE7, 1, {0x01} },
	{0xE8, 1, {0xEC} },
	{0xE9, 1, {0x02} },
	{0xEA, 1, {0x22} },
	{0xEB, 1, {0x03} },
	{0xEC, 1, {0x32} },
	{0xED, 1, {0x02} },
	{0xEE, 1, {0x22} },

	{0xFF, 1, {0x2A} },
	{0xFB, 1, {0x01} },
	{0x0C, 1, {0x04} },
	{0x0F, 1, {0x01} },
	{0x11, 1, {0xE0} },
	{0x15, 1, {0x0E} },
	{0x16, 1, {0x78} },
	{0x19, 1, {0x0D} },
	{0x1A, 1, {0xF4} },
	{0x37, 1, {0x6E} },
	{0x88, 1, {0x76} },

	{0xFF, 1, {0x2C} },
	{0xFB, 1, {0x01} },
	{0x4D, 1, {0x1E} },
	{0x4E, 1, {0x04} },
	{0x4F, 1, {0x00} },
	{0x9D, 1, {0x1E} },
	{0x9E, 1, {0x04} },

	{0xFF, 1, {0xF0} },
	{0xFB, 1, {0x01} },
	{0x5A, 1, {0x00} },

	{0xFF, 1, {0xE0} },
	{0xFB, 1, {0x01} },
	{0x25, 1, {0x02} },
	{0x4E, 1, {0x02} },
	{0x85, 1, {0x02} },

	{0xFF, 1, {0xD0} },
	{0xFB, 1, {0x01} },
	{0X09, 1, {0xAD} },

	{0xFF, 1, {0X20} },
	{0xFB, 1, {0x01} },
	{0XF8, 1, {0x64} },

	{0xFF, 1, {0x2A} },
	{0xFB, 1, {0x01} },
	{0X1A, 1, {0xF0} },
	{0x30, 1, {0x5E} },
	{0x31, 1, {0xCA} },
	{0x34, 1, {0xFE} },
	{0x35, 1, {0x35} },
	{0x36, 1, {0xA2} },

	{0x36, 1, {0xA2} },
	{0x37, 1, {0xF8} },
	{0x38, 1, {0x37} },
	{0x39, 1, {0xA0} },
	{0x3A, 1, {0x5E} },
	{0x53, 1, {0xD7} },
	{0x88, 1, {0x72} },
	{0x88, 1, {0x72} },

	{0xFF, 1, {0x24} },
	{0xFB, 1, {0x01} },
	{0xC6, 1, {0xC0} },

	{0xFF, 1, {0xE0} },
	{0xFB, 1, {0x01} },
	{0x25, 1, {0x00} },
	{0x4E, 1, {0x02} },
	{0x35, 1, {0x82} },
	{0xFF, 1, {0xC0} },

	{0xFF, 1, {0xC0} },
	{0xFB, 1, {0x01} },
	{0x9C, 1, {0x11} },
	{0x9D, 1, {0x11} },
	//60HZ VESA DSC
	{0xFF, 1, {0x25} },
	{0xFB, 1, {0x01} },
	{0x18, 1, {0x22} },

	//CCMRUN
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	{0xC0, 1, {0x03} },
	{0x51, 1, {0x00} },
	{0x35, 1, {0x00} },
	{0x53, 1, {0x24} },
#ifdef DSC_ENABLE
	{0xC0, 1, {0x03} },
#else
	{0xC0, 1, {0x00} },
#endif
	{0x53, 1, {0x24} },
	{0x55, 1, {0x00} },
	{0xFF, 1, {0x10} },
	{0x11, 0, {} },
#ifndef LCM_SET_DISPLAY_ON_DELAY
	{REGFLAG_DELAY, 120, {} },
	/* Display On*/
	{0x29, 0, {} },
#endif
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

// static struct dynamic_fps_info lcm_dynamic_fps_setting[] = {
	// {60, 20},
	// {50, 458},
	// {40, 1115},
	// {30, 2210},
// #if 0
	// {60, 20, 50},
	// {50, 458, 60},
	// {40, 115, 75},
	// {30, 2210, 100},
// #endif
// };
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
			if (table[i].count > 1)
				MDELAY(1);
			break;
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
	struct dfps_info *dfps_params = dsi->dfps_params;
	dsi->dfps_enable = 1;
	dsi->dfps_default_fps = 9000;/*real fps * 100, to support float*/
	dsi->dfps_def_vact_tim_fps = 9000;/*real vact timing fps * 100*/
	/* traversing array must less than DFPS_LEVELS */
	/* DPFS_LEVEL0 */
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	dfps_params[0].PLL_CLOCK = 500;
	dfps_params[0].vertical_frontporch = 2480;
	/* dfps_params[0].data_rate = xx; */
	/* DPFS_LEVEL1 */
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	dfps_params[1].PLL_CLOCK = 500;
	dfps_params[1].vertical_frontporch = 800;
	/* dfps_params[1].data_rate = xx; */
	dsi->dfps_num = 2;
}
#endif

static void lcm_get_params(struct LCM_PARAMS *params)
{
	// unsigned int i = 0;
	// unsigned int dynamic_fps_levels = 0;

	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
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

	params->dsi.vertical_sync_active = 10;
	params->dsi.vertical_backporch = 22;
	params->dsi.vertical_frontporch = 800;
	//params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 22;
	params->dsi.horizontal_backporch = 22;
	params->dsi.horizontal_frontporch = 165;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.bdg_ssc_disable = 1;
	params->dsi.dsc_enable = 0;
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
#ifdef DSC_ENABLE
	params->dsi.bdg_dsc_enable = 1;
	params->dsi.PLL_CLOCK = 220; //with dsc
//	params->dsi.PLL_CLOCK = 300; //with dsc
#else
	params->dsi.bdg_dsc_enable = 0;
	params->dsi.PLL_CLOCK = 500; //without dsc
#endif
	params->dsi.PLL_CK_CMD = 480;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9d;

	/* for ARR 2.0 */
	// params->max_refresh_rate = 60;
	// params->min_refresh_rate = 45;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_height = ROUND_CORNER_H_TOP;
	params->corner_pattern_height_bot = ROUND_CORNER_H_BOT;
	params->corner_pattern_tp_size = sizeof(top_rc_pattern);
	params->corner_pattern_lt_addr = (void *)top_rc_pattern;
#endif
	// /*ARR setting*/
	// params->dsi.dynamic_fps_levels = 4;
	// params->max_refresh_rate = 60;
	// params->min_refresh_rate = 30;
#if 0
	/*vertical_frontporch should be related to the max fps*/
	//params->dsi.vertical_frontporch = 20;
	/*vertical_frontporch_for_low_power
	 *should be related to the min fps
	 */
	params->dsi.vertical_frontporch_for_low_power = 750;
#endif

	// dynamic_fps_levels =
		// sizeof(lcm_dynamic_fps_setting)/sizeof(struct dynamic_fps_info);

	// dynamic_fps_levels =
		// params->dsi.dynamic_fps_levels <
		// dynamic_fps_levels
		// ? params->dsi.dynamic_fps_levels
		// : dynamic_fps_levels;

	// for (i = 0; i < dynamic_fps_levels; i++) {
		// params->dsi.dynamic_fps_table[i].fps =
			// lcm_dynamic_fps_setting[i].fps;
		// params->dsi.dynamic_fps_table[i].vfp =
			// lcm_dynamic_fps_setting[i].vfp;
		// params->dsi.dynamic_fps_table[i].idle_check_interval =
		 // lcm_dynamic_fps_setting[i].idle_check_interval;
	// }
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/****DynFPS start****/
	lcm_dfps_int(&(params->dsi));
	/****DynFPS end****/
#endif
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	display_bias_enable();
/*
	if (lcm_util.set_gpio_lcd_enp_bias) {
		lcm_util.set_gpio_lcd_enp_bias(1);

		_lcm_i2c_write_bytes(0x0, 0xf);
		_lcm_i2c_write_bytes(0x1, 0xf);
	} else
		LCM_LOGI("set_gpio_lcd_enp_bias not defined...\n");
*/
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
	LCM_LOGI("nt36672c_fhdp----tps6132----lcm mode = vdo mode :%d----\n",
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
	LCM_LOGI("%s,nt36672c backlight: level = %d\n", __func__, level);

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

	LCM_LOGI("%s,nt36672c id = 0x%08x\n", __func__, id);

	if (id == LCM_ID_nt36672c)
		return 1;
	else
		return 0;

}

struct LCM_DRIVER nt36672c_fhdp_dsi_vdo_60hz_wo_dsc_shenchao_lcm_drv = {
	.name = "nt36672c_fhdp_dsi_vdo_60hz_wo_dsc_shenchao_lcm_drv",
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

