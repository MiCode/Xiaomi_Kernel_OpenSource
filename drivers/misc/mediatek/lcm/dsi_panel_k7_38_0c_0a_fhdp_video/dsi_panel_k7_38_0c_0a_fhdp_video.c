/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"
#include "lcm_define.h"
#include "disp_dts_gpio.h"
#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>

#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>

#ifndef CONFIG_FPGA_EARLY_PORTING
#include <cust_gpio_usage.h>
#include <cust_i2c.h>
#endif

#endif
#endif

static const unsigned int BL_MIN_LEVEL = 20;
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
#define read_reg(cmd) \
	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
extern int tps65132_write_bytes(unsigned char addr, unsigned char value);
//static int lcd_bl_en;
#endif
#include "../panel_set_disp_param.h"
/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE                                    0
#define FRAME_WIDTH                                     (1080)
#define FRAME_HEIGHT                                    (2400)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH (67000)
#define LCM_PHYSICAL_HEIGHT (149000)
#define LCM_DENSITY	(409)


#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF
#define REGFLAG_UNVALID		0x0

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define OLED_1P8_EN 14
#define AVDD_3V_EN 127
/*
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};
*/
static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 10, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 150, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table init_setting[] = {

	//{0x9F, 2, {0xA5,0xA5}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	//{0x9F, 2, {0x5A,0x5A}},
	/* TE vsync ON */
	//{0x9F, 2, {0xA5,0xA5}},
	{0x35, 1, {0x00}},
	//{0x9F, 2, {0x5A,0x5A}},
	/* FAIL SAFE Setting */
	{0xFC, 2, {0x5A,0x5A}},
	{0xB0, 2, {0x03,0xED}},
	{0xED, 9, {0x40,0x04,0x08,0xA8,0x84,0x4A,0x73,0x02,0x0A}},
	/* ESD Err Fg Setting */
	{0xFC, 2, {0x5A,0x5A}},
	{0xED, 3, {0x01, 0xCD, 0x00}},
	{0xE1, 1, {0x93}},
	{0xFC, 2, {0xA5,0xA5}},
	/* OFC Setting 91.8M*/
	{0xDF, 21, {0x09,0x30,0x95,0x43,0x34,0x05,0x00,0x27,0x00,
				0x2E,0x4F,0x7A,0x77,0x10,0x3D,0x73,0x00,0xFF,
				0x01,0x8B,0x08}},
	{0xFC, 2, {0xA5,0xA5}},
	/* 11bit Dimming Setting */
	{0xF0, 2, {0x5A,0x5A}},
	{0xB3, 2, {0x11,0x4B}},
	/* Fast Discharge Setting */
	{0xB0, 1, {0x03}},
	{0xCD, 1, {0x04}},
	/* Dynamic elvss setting */
	{0xB0, 2, {0x04,0xB3}},
	{0xB3, 1, {0xC0}},
	/* Dimming Speed Setting */
	{0xB0, 2, {0x06,0xB3}},
	{0xB3, 1, {0x10}},
	{0xB0, 2, {0x05,0xB3}},
	{0xB3, 1, {0x93}},
	/* CRC srgb p3 Setting */
	{0xF0, 2, {0x5A,0x5A}},
	{0x80, 1, {0x80}},
	{0xB1, 1, {0x01}},
	{0xB0, 2, {0x01, 0xB1}},
	{0xB1, 21, {0xA7,0x02,0x05,0x34,0xCF,0x14,0x09,0x04,0xAE,0x46,
				0xEB,0xD0,0xC7,0x06,0xC1,0xE6,0xE2,0x1D,0xFF,0xFA,0xE1}},
	{0xB0, 2, {0x16,0xB1}},
	{0xB1, 21, {0xD9,0x00,0x00,0x06,0xCF,0x02,0x09,0x03,0xC6,0x10,
				0xF4,0xD9,0xFD,0x00,0xE0,0xE8,0xE1,0x03,0xFF,0xFF,0xFF}},
	{0xB0, 2, {0x55, 0xB1}},
	{0xB1, 1, {0x80}},
	{0xF0, 2, {0xA5,0xA5}},
	{0x53, 1, {0x20}},
	{REGFLAG_DELAY, 100, {}},
	/* Display On*/
	//{0x9F, 2, {0xA5,0xA5}},
	{0x29, 0, {}},
	//{0x9F, 2, {0x5A,0x5A}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table bl_level[] = {
	{0x51, 2, {0x07, 0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(struct LCM_setting_table *table,
		unsigned int count,
		unsigned char force_update)
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
			dsi_set_cmdq_V2(cmd, table[i].count,
					table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}
#ifdef CONFIG_ADB_WRITE_PARAM_FEATURE
static void lcm_set_disp_param(unsigned int param)
{
	int ret;
	pr_info("lcm_set_disp_paramt: param = %d\n", param);
	ret = panel_disp_param_send_lock(PANEL_SUMSUNG_FT3418,param, push_table);
	return;
}
#endif
static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->lcm_if              = LCM_INTERFACE_DSI0;
	params->lcm_cmd_if          = LCM_INTERFACE_DSI0;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;

	params->density		   = LCM_DENSITY;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
#endif
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
	params->dsi.vertical_backporch = 9;
	params->dsi.vertical_frontporch = 21;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 14;
	params->dsi.horizontal_backporch = 22;
	params->dsi.horizontal_frontporch = 48;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	
	//params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
	params->dsi.PLL_CLOCK = 549;
	/* this value must be in MTK suggested table */
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0;
	params->dsi.lcm_esd_check_table[0].count = 0;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0;

}

static void lcm_init_power(void)
{

	//pr_info("kxx add for [LCM]%s\n",__func__);
	//lcd_bl_en = 1;
}


static void lcm_suspend_power(void)
{
	//pr_info("kxxadd for [LCM]%s\n",__func__);
	MDELAY(5);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_LDO3_OUT0);
	MDELAY(5);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_LDO18_OUT0);
	MDELAY(5);
}

static void lcm_resume_power(void)
{
	//pr_info("[LCM]%s\n",__func__);

}

static void lcm_init(void)
{

	//pr_info("kxxadd for [LCM]%s\n",__func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_LDO18_OUT1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_LDO3_OUT1);
	MDELAY(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
	MDELAY(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(10);
	push_table(init_setting,
		sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	//pr_info("kxx add for [LCM]%s\n",__func__);
	push_table(lcm_suspend_setting,sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
}

static void lcm_resume(void)
{
	//pr_info("kxx add for [LCM]%s\n",__func__);
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
		unsigned int height)
{
#if LCM_DSI_CMD_MODE
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
#endif
}

struct LCM_setting_table switch_table_page[] = {
	{ 0xF0, 2, {0x5A,0x59} }
};


static unsigned int lcm_compare_id(void)
{
	unsigned int id0 = 0;
	unsigned int id1 = 0;
	unsigned char buffer0[4];
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(switch_table_page, sizeof(switch_table_page) / sizeof(struct LCM_setting_table), 1);

	array[0] = 0x00013700;  // read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x82, buffer0, 4);
	id0 = buffer0[0];     // we only need ID
	//read_reg_v2(0x44, buffer1, 1);
	id1 = buffer0[3];     // we only need ID
	pr_info("%s,icnl9911c debug: icnl9911c id0 = 0x%x, id1 = 0x%x\n", __func__, id0,id1);
	pr_info("%s,icnl9911c debug: icnl9911c buff[1] = 0x%x, buff[2] = 0x%x\n", __func__, buffer0[1],buffer0[2]);
	if(id0 == 0x31 && id1 == 0x41)
		return 1;
	else
		return 0;
}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		pr_debug("[LCM][LCM ERROR] [0x0A]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	pr_debug("[LCM][LCM NORMAL] [0x0A]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	pr_debug("[LCM]ATA check size = 0x%x,0x%x,0x%x,0x%x\n",
			x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	/* read id return two byte,version and id */
	data_array[0] = 0x00043700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
			&& (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}


static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	pr_info("[LCM]%s,kxx add for nt3418 backlight: level = %d\n", __func__, level);
#ifdef CONFIG_BACKLIGHT_SUPPORT_2047_FEATURE
#else
	level = level << 3;
#endif
	bl_level[0].para_list[0] = level >> 8;
	bl_level[0].para_list[1] = level & 0xFF;

	push_table(bl_level,
			sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
	/* customization: 1. V2C config 2 values, */
	/* C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		/* mode control addr */
		lcm_switch_mode_cmd.addr = 0xBB;
		/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[0] = 0x13;
		/* disable video mode secondly */
		lcm_switch_mode_cmd.val[1] = 0x10;
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		/* disable GRAM and enable video mode */
		lcm_switch_mode_cmd.val[0] = 0x03;
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}


struct LCM_DRIVER dsi_panel_k7_38_0c_0a_fhdp_video = {
	.name = "dsi_panel_k7_38_0c_0a_fhdp_video",
	.set_util_funcs = lcm_set_util_funcs,
#ifdef CONFIG_ADB_WRITE_PARAM_FEATURE
	.set_disp_param = lcm_set_disp_param,
#endif
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
};
