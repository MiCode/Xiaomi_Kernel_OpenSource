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

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <platform/mt_i2c.h>
#include <platform/upmu_common.h>
#include "ddp_hal.h"
#else
#endif

#include "lcm_drv.h"
#include "hx8394c_wxga_dsi_vdo.h"

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)

#define LCM_ID       (0x8394)

#define REGFLAG_DELAY								0xFE
#define REGFLAG_END_OF_TABLE							0xFF
/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util = { 0 };

unsigned int GPIO_LCD_PWR;
unsigned int GPIO_LCD_RST;

#define SET_RESET_PIN(v) lcm_set_gpio_output(GPIO_LCD_RST, v)	/* (lcm_util.set_reset_pin((v))) */

#define UDELAY(n)		(lcm_util.udelay(n))
#define MDELAY(n)		(lcm_util.mdelay(n))

#define GPIO_OUT_ONE 1
#define GPIO_OUT_ZERO 0

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)	 lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)							         lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)				 lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg							             lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)           lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#define dsi_set_cmdq_V3(para_tbl, size, force_update)   lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

#ifdef PUSH_TABLET_USING
#if 0
static struct LCM_setting_table lcm_initialization_setting[] = {
/*
Note :

Data ID will depends on the following rule.

count of parameters > 1	=> Data ID = 0x39
count of parameters = 1	=> Data ID = 0x15
count of parameters = 0	=> Data ID = 0x05

Structure Format :

{DCS command, count of parameters, {parameter list}}
{REGFLAG_DELAY, milliseconds of time, {}},

...

Setting ending by predefined flag

{REGFLAG_END_OF_TABLE, 0x00, {}}
*/
	{0xB9, 3, {0xFF, 0x83, 0x94} },
	{REGFLAG_DELAY, 5, {} },
	{0xBA, 1, {0x22} },
	{REGFLAG_DELAY, 5, {} },
	{0xB1, 15,
	 {0x64, 0x10, 0x30, 0x44, 0x34, 0x11, 0xf1, 0x81, 0x70, 0xD9, 0x34, 0x80, 0xC0, 0xD2,
	  0x01} },
	{REGFLAG_DELAY, 5, {} },
	{0xB2, 12, {0x45, 0x64, 0x04, 0x08, 0x40, 0x1C, 0x08, 0x08, 0x1C, 0x4D, 0x00, 0x00} },
	{REGFLAG_DELAY, 5, {} },
	{0xB4, 22,
	 {0x00, 0xFF, 0x18, 0x60, 0x60, 0x60, 0x00, 0x00, 0x01, 0x30, 0x04, 0x68, 0x18, 0x60, 0x60,
	  0x60, 0x00, 0x00, 0x01, 0x30, 0x04, 0x68} },
	{REGFLAG_DELAY, 5, {} },
	{0xB6, 2, {0x5C, 0x5C} },
	{REGFLAG_DELAY, 5, {} },
	{0xCC, 1, {0x09} },
	{REGFLAG_DELAY, 5, {} },
	{0xD3, 32,
	 {0x00, 0x00, 0x00, 0x01, 0x07, 0x00, 0x08, 0x32, 0x10, 0x0A, 0x00, 0x05, 0x00, 0x20, 0x0A,
	  0x05, 0x09, 0x00, 0x32, 0x10, 0x08, 0x00, 0x11, 0x11, 0x0D, 0x07, 0x23, 0x0D, 0x07, 0x47,
	  0x0D, 0x08} },
	{REGFLAG_DELAY, 5, {} },
	{0xD5, 44,
	 {0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02,
	  0x02, 0x20, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	  0x21, 0x21, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18} },
	{REGFLAG_DELAY, 5, {} },
	{0xE0, 42,
	 {0x02, 0x0A, 0x10, 0x28, 0x30, 0x3e, 0x1D, 0x3A, 0x05, 0x0B, 0x0D, 0x17, 0x0E, 0x12, 0x15,
	  0x12, 0x13, 0x06, 0x12, 0x14, 0x18, 0x02, 0x0A, 0x10, 0x28, 0x30, 0x3E, 0x1D, 0x3A, 0x05,
	  0x0B, 0x0D, 0x17, 0x0E, 0x12, 0x15, 0x12, 0x13, 0x06, 0x12, 0x14, 0x18} },
	{REGFLAG_DELAY, 200, {} },
	{0xC9, 5, {0x1F, 0x2E, 0x1E, 0x1E, 0x10} },
	{REGFLAG_DELAY, 200, {} },
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 200, {} },
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 250, {} },
	/* Setting ending by predefined flag */
	/* {REGFLAG_END_OF_TABLE, 0x00, {}} */
};
#else
static struct LCM_setting_table lcm_initialization_setting[] = {
	{0xB9, 3, {0xFF, 0x83, 0x94} },
	{0xBA, 1, {0x22} },
	{0xB1, 15,
	 {0x64, 0x10, 0x30, 0x44, 0x34, 0x11, 0xf1, 0x81, 0x70, 0xD9, 0x34, 0x80, 0xC0, 0xD2,
	  0x01} },
	{0xB2, 12, {0x45, 0x64, 0x04, 0x08, 0x40, 0x1C, 0x08, 0x08, 0x1C, 0x4D, 0x00, 0x00} },
	{0xB4, 22,
	 {0x00, 0xFF, 0x18, 0x60, 0x60, 0x60, 0x00, 0x00, 0x01, 0x30, 0x04, 0x68, 0x18, 0x60, 0x60,
	  0x60, 0x00, 0x00, 0x01, 0x30, 0x04, 0x68} },
	{0xB6, 2, {0x5C, 0x5C} },
	{0xCC, 1, {0x09} },
	{0xD3, 32,
	 {0x00, 0x00, 0x00, 0x01, 0x07, 0x00, 0x08, 0x32, 0x10, 0x0A, 0x00, 0x05, 0x00, 0x20, 0x0A,
	  0x05, 0x09, 0x00, 0x32, 0x10, 0x08, 0x00, 0x11, 0x11, 0x0D, 0x07, 0x23, 0x0D, 0x07, 0x47,
	  0x0D, 0x08} },
	{0xD5, 44,
	 {0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02,
	  0x02, 0x20, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	  0x21, 0x21, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18} },
	{0xE0, 42,
	 {0x02, 0x0A, 0x10, 0x28, 0x30, 0x3E, 0x1D, 0x3A, 0x05, 0x0B, 0x0D, 0x17, 0x0E, 0x12, 0x15,
	  0x12, 0x13, 0x06, 0x12, 0x14, 0x18, 0x02, 0x0A, 0x10, 0x28, 0x30, 0x3E, 0x1D, 0x3A, 0x05,
	  0x0B, 0x0D, 0x17, 0x0E, 0x12, 0x15, 0x12, 0x13, 0x06, 0x12, 0x14, 0x18} },
	{REGFLAG_DELAY, 200, {} },
	{0xC9, 5, {0x1F, 0x2E, 0x1E, 0x1E, 0x10} },
	{REGFLAG_DELAY, 200, {} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 200, {} },
	{0x29, 0, {} },
	{REGFLAG_DELAY, 250, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
#endif

static void init_lcm_registers(void)
{
	unsigned int data_array[16];

#ifdef BUILD_LK
	printf("%s, LK\n", __func__);
#else
	pr_debug("%s, KE\n", __func__);
#endif

#ifdef PUSH_TABLET_USING
	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
#else
	data_array[0] = 0x00043902;
	data_array[1] = 0x9483ffb9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x22ba1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00103902;
	data_array[1] = 0x301064b1;
	data_array[2] = 0xf1113444;
	data_array[3] = 0x34D97081;
	data_array[4] = 0x01D2C080;
	dsi_set_cmdq(data_array, 5, 1);

	data_array[0] = 0x000d3902;
	data_array[1] = 0x046445b2;
	data_array[2] = 0x081C4008;
	data_array[3] = 0x004D1C08;
	data_array[4] = 0x00;
	dsi_set_cmdq(data_array, 5, 1);

	data_array[0] = 0x00173902;
	data_array[1] = 0x18ff00b4;
	data_array[2] = 0x00606060;
	data_array[3] = 0x04300100;
	data_array[4] = 0x60601868;
	data_array[5] = 0x01000060;
	data_array[6] = 0x00680430;
	dsi_set_cmdq(data_array, 7, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x005c5cb6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x09cc1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00213902;
	data_array[1] = 0x000000d3;
	data_array[2] = 0x08000701;
	data_array[3] = 0x000A1032;
	data_array[4] = 0x0A200005;
	data_array[5] = 0x32000905;
	data_array[6] = 0x11000810;
	data_array[7] = 0x23070D11;
	data_array[8] = 0x0D47070D;
	data_array[9] = 0x08;
	dsi_set_cmdq(data_array, 10, 1);

	data_array[0] = 0x002d3902;
	data_array[1] = 0x010101d5;
	data_array[2] = 0x00000001;
	data_array[3] = 0x03030300;
	data_array[4] = 0x02020203;
	data_array[5] = 0x18202002;
	data_array[6] = 0x18181818;
	data_array[7] = 0x18181818;
	data_array[8] = 0x21181818;
	data_array[9] = 0x18181821;
	data_array[10] = 0x18181818;
	data_array[11] = 0x18181818;
	data_array[12] = 0x18;
	dsi_set_cmdq(data_array, 13, 1);

	data_array[0] = 0x002b3902;
	data_array[1] = 0x100a02e0;
	data_array[2] = 0x1D3E3028;
	data_array[3] = 0x0D0B053A;
	data_array[4] = 0x15120E17;
	data_array[5] = 0x12061312;
	data_array[6] = 0x0A021814;
	data_array[7] = 0x3E302810;
	data_array[8] = 0x0B053A1D;
	data_array[9] = 0x120E170D;
	data_array[10] = 0x13061215;
	data_array[11] = 0x00181412;
	dsi_set_cmdq(data_array, 12, 1);
	/*MDELAY(200); */

	data_array[0] = 0x00063902;
	data_array[1] = 0x1e2e1fc9;
	data_array[2] = 0x101e;
	dsi_set_cmdq(data_array, 3, 1);
	/*MDELAY(200); */

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);
#endif
}

#ifdef PUSH_TABLET_USING
static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;
	unsigned cmd;

	for (i = 0; i < count; i++) {

		cmd = table[i].cmd;
		switch (cmd) {

		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}
#endif

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	if (GPIO == 0xFFFFFFFF) {
#ifdef BUILD_LK
		printf("[LK/LCM] GPIO_LCD_PWR =   0x%x\n", GPIO_LCD_PWR);
		printf("[LK/LCM] GPIO_LCD_RST =   0x%x\n", GPIO_LCD_RST);
#elif (defined BUILD_UBOOT)
#else
#endif

		return;
	}
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#else
	gpio_set_value(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#endif
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_THREE_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Video mode setting */
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 3;
	params->dsi.vertical_backporch = 8;	/* 3;//4 */
	params->dsi.vertical_frontporch = 60;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 48;	/* 12;//20; */
	params->dsi.horizontal_backporch = 80;	/* 32;//48; //10 OK */
	params->dsi.horizontal_frontporch = 52;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 345;

	params->dsi.cont_clock = 1;
}

static void lcm_power_init(void)
{
#ifdef BUILD_LK
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ONE);

	SET_RESET_PIN(1);
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	/*MDELAY(50); */
	MDELAY(10);
#else
#endif
}

static void lcm_init(void)
{
	DSI_clk_HS_mode(DISP_MODULE_DSI0, NULL, 1);

#ifdef BUILD_LK
	init_lcm_registers();
#else
#endif
}

static void lcm_suspend(void)
{
#ifndef BUILD_LK
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ZERO);
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);
#endif
}

static void lcm_resume(void)
{
#ifndef BUILD_LK
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ONE);

	SET_RESET_PIN(1);
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	/*MDELAY(50); */
	MDELAY(10);

	DSI_clk_HS_mode(DISP_MODULE_DSI0, NULL, 1);
	init_lcm_registers();
#endif
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[3];

	unsigned int data_array[16];

	SET_RESET_PIN(1);	/* NOTE:should reset LCM firstly */
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
	MDELAY(20);

	data_array[0] = 0x00043902;
	data_array[1] = 0x9483FFB9;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0] = 0x00033700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x04, buffer, 3);
	id = (buffer[0] << 8) | buffer[1];	/* we only need ID */

	pr_debug("read id, buf:0x%02x ,0x%02x,0x%02x, id=0X%X", buffer[0], buffer[1], buffer[2],
		  id);

	return (LCM_ID == id) ? 1 : 0;
}

LCM_DRIVER hx8394c_wxga_dsi_vdo_lcm_drv = {
	.name = "hx8394c_wxga_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init_power = lcm_power_init,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
};
