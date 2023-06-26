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
#include <linux/gpio.h>
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "data_hw_roundedpattern.h"
#endif

#include "lcm_drv.h"
/* Huaqin add for K19A-315 by feiwen at 2021/06/16 start */
#include "../../../../input/touchscreen/mediatek/NT36672C/nt36xxx.h"
/* Huaqin add for K19A-315 by feiwen at 2021/06/16 end */
/* Huaqin add for HQ-142518 by feiwen at 2021/06/24 start */
#include "mtk_boot_common.h"
/* Huaqin add for HQ-142518 by feiwen at 2021/06/24 end */
#include <linux/hqsysfs.h>
#ifdef BUILD_LK
#  include <platform/upmu_common.h>
#  include <platform/mt_gpio.h>
#  include <platform/mt_i2c.h>
#  include <platform/mt_pmic.h>
#  include <string.h>
#elif defined(BUILD_UBOOT)
#  include <asm/arch/mt_gpio.h>
#endif

/* Huaqin modify for HQ-140017 by caogaojie at 2021/07/14 start */
extern int hq_selene_pcba_config;
/* Huaqin modify for HQ-140017 by caogaojie at 2021/07/14 end */

/* Huaqin add for K19A-315 by feiwen at 2021/06/16 start */
#ifdef mdelay
#undef mdelay
#endif

#ifdef udelay
#undef udelay
#endif
/* Huaqin add for K19A-315 by feiwen at 2021/06/16 end */

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

/* Huaqin modify for HQ-132693 by liunianliang at 2021/04/27 start */
/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(67716)
#define LCM_PHYSICAL_HEIGHT		(150480)
#define LCM_DENSITY			(405)
/* Huaqin modify for HQ-132693 by liunianliang at 2021/04/27 end */

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

#ifdef CONFIG_MTK_MT6382_BDG
#define DSC_ENABLE
#endif

//#define DSC_ENABLE
//#define DSC_ENABLE

/* i2c control start */

#define LCM_I2C_ADDR 0x3E
#define LCM_I2C_BUSNUM  1	/* for I2C channel 0 */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"

/* Huaqin add for HQ-123498 by caogaojie at 2021/05/08 start */
static unsigned ENP = 490; //gpio165
static unsigned ENN = 494; //gpio169
/* Huaqin add for HQ-123498 by caogaojie at 2021/05/08 end */
#define GPIO_LCD_BIAS_ENP   ENP
#define GPIO_LCD_BIAS_ENN   ENN

/* Huaqin add for HQ-132637 by liunianliang at 2021/05/04 start */
extern bool nvt_gesture_flag;
/* Huaqin add for HQ-132637 by liunianliang at 2021/05/04 end */

/* Huaqin add for HQ-124138 by liunianliang at 2021/04/29 start */
#ifdef CONFIG_MI_ERRFLAG_ESD_CHECK_ENABLE
/* Huaqin add for K19A-315 by feiwen at 2021/06/16 start */
extern struct nvt_ts_data *ts;
/* Huaqin add for K19A-315 by feiwen at 2021/06/16 end */
extern int32_t nvt_update_firmware(char *firmware_name);
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 start */
static bool esd_flag = false;
extern bool g_trigger_disp_esd_recovery;
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 end */
#endif
/* Huaqin add for HQ-124138 by liunianliang at 2021/04/29 end */
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
/* Huaqin modify for HQ-124216 by caogaojie at 2021/05/08 start */
static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};
/* Huaqin modify for HQ-124216 by caogaojie at 2021/05/08 end */

/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 start */
static struct LCM_setting_table tp_reset_cmd1[] = {
	{0xFF, 1, {0xC0} },
	{0x4B, 1, {0x00} },
};

static struct LCM_setting_table tp_reset_cmd2[] = {
	{0xFF, 1, {0xC0} },
	{0x4B, 1, {0x0E} },
};
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 end */
static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF, 1, {0x10} },
        {0xFB, 1, {0x01} },
	{0XB0, 1, {0x00} },
	{0XC0, 1, {0x03} },
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 start */
        {0xC1, 16, {0x89,0x28,0x00,0x08,0x00,0xAA,0x02,0x0E,0x00,0x2B,0x00,0x07,0x0D,0xB7,0x0C,0xB7} },
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 end */
        {0XC2, 2, {0X1B,0XA0} },
//FR
	/*{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },
	{0x40, 1, {0x22} },*/

        {0xFF, 1, {0XE0} },
        {0XFB, 1, {0X01} },
        {0X35, 1, {0X82} },
        {0X85, 1, {0X32} },

        {0xFF, 1, {0XF0} },
        {0xFB, 1, {0x01} },
        {0X1C, 1, {0X01} },
        {0X33, 1, {0X01} },
        {0X5A, 1, {0X00} },
	{0X9C, 1, {0X00} },

        {0xFF, 1, {0XD0} },
        {0xFB, 1, {0x01} },
        {0X53, 1, {0X22} },
	{0X54, 1, {0X02} },

        {0XFF, 1, {0XC0} },
        {0XFB, 1, {0X01} },
	{0X9C, 1, {0X11} },
	{0X9D, 1, {0X11} },

        {0xFF, 1, {0X27} },
        {0XFB, 1, {0X01} },
	/* Huaqin modify for K19A-271 by caogaojie at 2021/05/13 start */
        {0X3F, 1, {0X09} },
	/* Huaqin modify for K19A-271 by caogaojie at 2021/05/13 end */
        {0X40, 1, {0X25} },
        {0X43, 1, {0X08} },

        {0XFF, 1, {0X10} },
        {0XFB, 1, {0X01} },
	{0X35, 1, {0X00} },

        {0x11, 0, {} },
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 start */
	{0x11, 0, {} },
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 end */
	/* Huaqin modify for HQ-132702 by liunianliang at 2021/05/20 start */
        {REGFLAG_DELAY, 70, {} },
        {0x29, 0, {} },
	/* Huaqin modify for HQ-132702 by liunianliang at 2021/05/20 end */
};

/* Huaqin modify for HQ-140017 by caogaojie at 2021/07/14 start */
static struct LCM_setting_table init_setting_vdo_K19L[] = {
	{0xFF, 1, {0x10} },
        {0xFB, 1, {0x01} },
	{0XB0, 1, {0x00} },
	{0XC0, 1, {0x03} },
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 start */
        {0xC1, 16, {0x89,0x28,0x00,0x08,0x00,0xAA,0x02,0x0E,0x00,0x2B,0x00,0x07,0x0D,0xB7,0x0C,0xB7} },
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 end */
        {0XC2, 2, {0X1B,0XA0} },
//FR
	/*{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },
	{0x40, 1, {0x22} },*/

        {0xFF, 1, {0XE0} },
        {0XFB, 1, {0X01} },
        {0X35, 1, {0X82} },
        {0X85, 1, {0X32} },

        {0xFF, 1, {0XF0} },
        {0xFB, 1, {0x01} },
        {0X1C, 1, {0X01} },
        {0X33, 1, {0X01} },
        {0X5A, 1, {0X00} },
	{0X9C, 1, {0X00} },

        {0xFF, 1, {0XD0} },
        {0xFB, 1, {0x01} },
        {0X53, 1, {0X22} },
  	{0X54, 1, {0X02} },

        {0XFF, 1, {0XC0} },
        {0XFB, 1, {0X01} },
  	{0X9C, 1, {0X11} },
  	{0X9D, 1, {0X11} },

        {0xFF, 1, {0X27} },
        {0XFB, 1, {0X01} },
	/* Huaqin modify for K19A-271 by caogaojie at 2021/05/13 start */
        {0X3F, 1, {0X09} },
	/* Huaqin modify for K19A-271 by caogaojie at 2021/05/13 end */
        {0X40, 1, {0X25} },
        {0X43, 1, {0X08} },

        {0XFF, 1, {0X20} },
        {0XFB, 1, {0X01} },
        {0X1F, 1, {0XF0} },
        {0X20, 1, {0XF0} },
        {0X21, 1, {0X01} },//MUX_driving

        {0XFF, 1, {0X10} },
        {0XFB, 1, {0X01} },
  	{0X35, 1, {0X00} },

        {0x11, 0, {} },
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 start */
	{0x11, 0, {} },
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 end */
	/* Huaqin modify for HQ-132702 by liunianliang at 2021/05/20 start */
        {REGFLAG_DELAY, 70, {} },
        {0x29, 0, {} },
	/* Huaqin modify for HQ-132702 by liunianliang at 2021/05/20 end */
};
/* Huaqin modify for HQ-140017 by caogaojie at 2021/07/14 end */
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
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 start */
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
			if (table[i].count > 1)
				MDELAY(1);
	/* Huaqin modify for HQ-126356 by caogaojie at 2021/05/07 end */
			break;
		}
	}
}

static void lcm_set_gpio_output(unsigned GPIO, unsigned int output)
{
	int ret;

	ret = gpio_request(GPIO, "GPIO");
	if (ret < 0) {
		pr_err("[%s]: GPIO requset fail!\n", __func__);
	}

	if (gpio_is_valid(GPIO)) {
		ret = gpio_direction_output(GPIO, output);
			if (ret < 0) {
				pr_err("[%s]: failed to set output", __func__);
			}
	}

	gpio_free(GPIO);
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
/* Huaqin modify for HQHW-802 by caogaojie at 2021/05/10 start */
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
	struct dfps_info *dfps_params = dsi->dfps_params;

	dsi->dfps_enable = 1;
	dsi->dfps_default_fps = 6000;/*real fps * 100, to support float*/
	dsi->dfps_def_vact_tim_fps = 9000;/*real vact timing fps * 100*/
	/* traversing array must less than DFPS_LEVELS */
	/* DPFS_LEVEL0 */
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	/* dfps_params[0].PLL_CLOCK = 574; */
	/* dfps_params[0].data_rate = xx; */
	dfps_params[0].vertical_frontporch = 1290;
	/* Huaqin modify for HQ-124150 by caogaojie at 2021/06/07 start */
	dfps_params[0].vertical_frontporch_for_low_power = 2524;

	/* DPFS_LEVEL1 */
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	/* dfps_params[1].PLL_CLOCK = 380; */
	/* dfps_params[1].data_rate = xx; */
	/* Huaqin modify for HQ-179522 by jiangyue at 2022/01/24 start */
	dfps_params[1].vertical_frontporch = 46;
	/* Huaqin modify for HQ-179522 by jiangyue at 2022/01/24 end */
	dfps_params[1].vertical_frontporch_for_low_power = 2524;
	/* Huaqin modify for HQ-124150 by caogaojie at 2021/06/07 end */
	dsi->dfps_num = 2;
}
/* Huaqin modify for HQHW-802 by caogaojie at 2021/05/12 end */
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

/* Huaqin modify for K19S-31 by jiangyue at 2022/01/14 start */
	params->dsi.vertical_sync_active = 10;
	params->dsi.vertical_backporch = 10;
	params->dsi.vertical_frontporch = 46;
	//params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 16;
	params->dsi.horizontal_backporch = 16;
	params->dsi.horizontal_frontporch = 148;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	/* Huaqin modify for K19S-31 by jiangyue at 2022/01/14 end */
#ifdef CONFIG_MTK_MT6382_BDG
	params->dsi.bdg_ssc_disable = 1;
	params->dsi.dsc_params.ver = 17;
	params->dsi.dsc_params.slice_mode = 1;
	params->dsi.dsc_params.rgb_swap = 0;
	params->dsi.dsc_params.dsc_cfg = 34;
	params->dsi.dsc_params.rct_on = 1;
	params->dsi.dsc_params.bit_per_channel = 8;
	params->dsi.dsc_params.dsc_line_buf_depth = 9;
	params->dsi.dsc_params.bp_enable = 1;
	params->dsi.dsc_params.bit_per_pixel = 128;
	params->dsi.dsc_params.pic_height = 2400;
	params->dsi.dsc_params.pic_width = 1080;
	params->dsi.dsc_params.slice_height = 8;
	params->dsi.dsc_params.slice_width = 540;
	params->dsi.dsc_params.chunk_size = 540;
	params->dsi.dsc_params.xmit_delay = 170;
	params->dsi.dsc_params.dec_delay = 526;
	params->dsi.dsc_params.scale_value = 32;
	params->dsi.dsc_params.increment_interval = 43;
	params->dsi.dsc_params.decrement_interval = 7;
	params->dsi.dsc_params.line_bpg_offset = 12;
	params->dsi.dsc_params.nfl_bpg_offset = 3511;
	params->dsi.dsc_params.slice_bpg_offset = 3255;
	params->dsi.dsc_params.initial_offset = 6144;
	params->dsi.dsc_params.final_offset = 7072;
	params->dsi.dsc_params.flatness_minqp = 3;
	params->dsi.dsc_params.flatness_maxqp = 12;
	params->dsi.dsc_params.rc_model_size = 8192;
	params->dsi.dsc_params.rc_edge_factor = 6;
	params->dsi.dsc_params.rc_quant_incr_limit0 = 11;
	params->dsi.dsc_params.rc_quant_incr_limit1 = 11;
	params->dsi.dsc_params.rc_tgt_offset_hi = 3;
	params->dsi.dsc_params.rc_tgt_offset_lo = 3;
#endif
	params->dsi.dsc_enable = 0;
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
#ifdef DSC_ENABLE
	/* Huaqin modify for K19S-31 by jiangyue at 2022/01/14 start */
    params->dsi.bdg_dsc_enable = 1;
    params->dsi.PLL_CLOCK = 360; //with dsc
/* Huaqin modify for K19S-31 by jiangyue at 2022/01/14 end */
#else
	params->dsi.bdg_dsc_enable = 0;
	params->dsi.PLL_CLOCK = 574; //without dsc
#endif
	params->dsi.PLL_CK_CMD = 480;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
/* Huaqin add for HQ-124138 by liunianliang at 2021/04/29 start */
#ifdef CONFIG_MI_ERRFLAG_ESD_CHECK_ENABLE
	params->dsi.esd_check_enable = 1;
#endif
/* Huaqin add for HQ-124138 by liunianliang at 2021/04/29 end */
	//params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	/*params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;*/

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
		//params->dsi.dynamic_fps_table[i].idle_check_interval =
		//lcm_dynamic_fps_setting[i].idle_check_interval;
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
	//display_bias_enable();
	LCM_LOGI("[nt36672D] %s enter\n", __func__);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 1);
	MDELAY(3);

	lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 1);
	/* Huaqin modify for HQ-132702 by liunianliang at 2021/05/20 start */
	MDELAY(10);
	/* Huaqin modify for HQ-132702 by liunianliang at 2021/05/20 end */

	LCM_LOGI("[nt36672D] %s exit\n", __func__);
}

static void lcm_suspend_power(void)
{
/* Huaqin add for HQ-123199 by shiwenlong at 2021/4/4 start */
/* Huaqin modify for HQ-123498 by caogaojie at 2021/05/08 start */
	if (!nvt_gesture_flag)
	{
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENN, 0);
		MDELAY(3);
		lcm_set_gpio_output(GPIO_LCD_BIAS_ENP, 0);
		MDELAY(5);
	}
/* Huaqin modify for HQ-123498 by caogaojie at 2021/05/08 end */
/* Huaqin add for HQ-123199 by shiwenlong at 2021/4/4 end */
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
/* Huaqin modify for HQ-123199 by shiwenlong at 2021/4/4 start */
	SET_RESET_PIN(1);
/* Huaqin modify for HQ-123199 by shiwenlong at 2021/4/4 end */
	LCM_LOGI("[DENNIS][%s][%d]\n", __func__, __LINE__);
	lcm_init_power();
}

static void lcm_init(void)
{
	/* Huaqin modify for HQ-132702 by liunianliang at 2021/05/20 start */
      	SET_RESET_PIN(0);
      	MDELAY(5);
      	SET_RESET_PIN(1);
      	MDELAY(10);
      	SET_RESET_PIN(0);
      	MDELAY(5);
      	SET_RESET_PIN(1);
	/* Huaqin modify for HQ-132702 by caogaojie at 2021/06/15 start */
	MDELAY(10);
	/* Huaqin modify for HQ-132702 by caogaojie at 2021/06/15 end */
	/* Huaqin modify for HQ-132702 by liunianliang at 2021/05/20 end */
	/* Huaqin modify for HQ-161950 by jiangyue at 2021/11/05 start */
	if(esd_flag){
		nvt_bootloader_reset_locked();
		push_table(NULL, tp_reset_cmd1, ARRAY_SIZE(tp_reset_cmd1), 1);
		MDELAY(10);
		nvt_esd_vdd_tp_recovery();
		MDELAY(20);
		push_table(NULL, tp_reset_cmd2, ARRAY_SIZE(tp_reset_cmd2), 1);
		MDELAY(10);
		esd_flag = false;
		g_trigger_disp_esd_recovery = false;
	}
	/* Huaqin modify for HQ-161950 by jiangyue at 2021/11/05 end */

	/* Huaqin modify for HQ-140017 by caogaojie at 2021/07/14 start */
	/* Huaqin modify for HQ-155949 by caogaojie at 2021/09/18 start */
	//2:PCBA_K19A_LA 8:PCBA_K19L_LA 13:PCBA_K19V_LA
	if (hq_selene_pcba_config == 2 || hq_selene_pcba_config == 8 || hq_selene_pcba_config == 13){
		push_table(NULL, init_setting_vdo_K19L, ARRAY_SIZE(init_setting_vdo_K19L), 1);
		LCM_LOGI("%s this is K19L %d\n",__func__,hq_selene_pcba_config);
	} else {
		push_table(NULL, init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
		LCM_LOGI("%s this is the other config %d\n",__func__,hq_selene_pcba_config);
	}
	/* Huaqin modify for HQ-140017 by caogaojie at 2021/07/14 end */
	/* Huaqin modify for HQ-155949 by caogaojie at 2021/09/18 end */
}

static void lcm_suspend(void)
{
	LCM_LOGI("[DENNIS][%s][%d]\n", __func__, __LINE__);
	push_table(NULL, lcm_suspend_setting,
		   ARRAY_SIZE(lcm_suspend_setting), 1);
}

static void lcm_resume(void)
{
	LCM_LOGI("[DENNIS][%s][%d]\n", __func__, __LINE__);
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
	pr_err("%s,nt36672c backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

static void lcm_set_hw_info(void)
{
	hq_regiser_hw_info(HWID_LCM, "incell,vendor:Truly,IC:nt36672C(novatek)");
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

//	SET_RESET_PIN(1);
//	SET_RESET_PIN(0);
	MDELAY(1);

//	SET_RESET_PIN(1);
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

/* Huaqin add for HQ-124138 by liunianliang at 2021/04/29 start */
#ifdef CONFIG_MI_ERRFLAG_ESD_CHECK_ENABLE
static unsigned int lcd_esd_recover(void)
{
	LCM_LOGI("%s, int and update tp fw..\n", __func__);
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 start */
	esd_flag = true;
/* Huaqin modify for HQ-144782 by caogaojie at 2021/07/05 end */
	lcm_init_power();
	lcm_init();

/* Huaqin add for HQ-142518 by feiwen at 2021/06/24 start */
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
		||get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT)
		{
			LCM_LOGI("%s, enter power off charging mode.\n", __func__);
		} else {
/* Huaqin add for K19A-315 by feiwen at 2021/06/16 start */
			mutex_lock(&ts->lock);
			nvt_update_firmware("nt36672c_tr_02_ts_fw.bin");
			mutex_unlock(&ts->lock);
/* Huaqin add for K19A-315 by feiwen at 2021/06/16 end */
		}
#endif
/* Huaqin add for HQ-142518 by feiwen at 2021/06/24 end */
	return 0;
}
#endif
/* Huaqin add for HQ-124138 by liunianliang at 2021/04/29 end */

struct LCM_DRIVER dsi_panel_k19a_43_02_0b_dsc_vdo_lcm_drv = {
	.name = "dsi_panel_k19a_43_02_0b_dsc_vdo_lcm_drv",
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
	.set_hw_info = lcm_set_hw_info,
/* Huaqin add for HQ-124138 by liunianliang at 2021/04/29 start */
#ifdef CONFIG_MI_ERRFLAG_ESD_CHECK_ENABLE
	.esd_recover = lcd_esd_recover,
#endif
/* Huaqin add for HQ-124138 by liunianliang at 2021/04/29 end */
};

