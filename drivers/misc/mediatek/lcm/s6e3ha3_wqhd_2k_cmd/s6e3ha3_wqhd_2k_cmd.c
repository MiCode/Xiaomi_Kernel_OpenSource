/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#endif
#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
#include <cust_i2c.h>
#endif
#endif


#ifdef BUILD_LK
 #define LCD_DEBUG(fmt)  dprintf(CRITICAL, fmt)
#else
 #define LCD_DEBUG(fmt)  pr_debug(fmt)
#endif
/**
 *  Local Constants
 */
#define LCM_DSI_CMD_MODE	1
#define FRAME_WIDTH		(1440)
#define FRAME_HEIGHT		(2560)

#ifndef TRUE
 #define TRUE 1
#endif

#ifndef FALSE
 #define FALSE 0
#endif

/**
 *  Local Variables
 */
static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/**
 * Local Functions
 */
#define dsi_set_cmd_by_cmdq_dual(handle, cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V23(handle, cmd, (unsigned char)(count),\
					  (unsigned char *)(ppara), (unsigned char)(force_update))
#define dsi_set_cmdq_V3(para_tbl, size, force_update) \
			lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
			lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)


#define REGFLAG_DELAY		0xFE
#define REGFLAG_END_OF_TABLE	0xFF /* END OF REGISTERS MARKER */

struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
#ifdef BUILD_LK
			dprintf(0, "[LK]REGFLAG_DELAY\n");
#endif
			MDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
#ifdef BUILD_LK
			dprintf(0, "[LK]push_table end\n");
#endif
			break;
		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}

static struct LCM_setting_table lcm_initialization_setting[] = {
	/* Sleep Out */
	{0x11, 0, {} },
	{REGFLAG_DELAY, 20, {} }, /* 20ms */

	/* Interface Setting */
	{0xF0, 2, {0x5A, 0x5A} }, /* TEST KEY Enable */
	{REGFLAG_DELAY, 20, {} }, /* 20ms */

#if (!LCM_DSI_CMD_MODE)
	{0xB0, 1, {0x01} }, /* global para */
	{0xF2, 1, {0x4E} }, /* Need to transfer from Command mode to video mode
			       if The module's default  is commmand mode. */
#endif

	{0xC4, 1, {0x00} }, /* DUAL Control */
	{0xF9, 1, {0x00} }, /* DUAL Control */
	{0xC4, 1, {0x03} }, /* DUAL Control */
	{0xF9, 1, {0x03} }, /* DUAL Control */
	/* MBGR */
	{0x36, 1, {0x08} },
	{0xC2, 19, {0x00, 0x00, 0xD8, 0xD8, 0x00, 0x80, 0x2B, 0x05,
			   0x08, 0x0E, 0x07, 0x0B, 0x05, 0x0D, 0x0A, 0x15,
			   0x13, 0x20, 0x1E} }, /* PENTILE Setting */
	{0xF0,	2, {0xA5, 0xA5} }, /* TEST KEY Disable */
	{REGFLAG_DELAY, 120, {} }, /* 120ms */

	/* Common Setting */
	{0x35, 1, {0x00} }, /* TE ON */

	/* Brightness Control */
	{0x53, 1, {0x20} }, /* Dimming Control */
	{0x51, 1, {0xFF} }, /* Luminance Setting */

	/* ACL ON/OFF */
	{0x55, 1, {0x00} },

	/* memory access */
	{0x2C, 0, {} },
	{0x3C, 0, {} },

	/* display on */
	{0x29, 0, {} },
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x00, 0, {} },  /* NOP */
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 150, {} }
};


/**
 *  LCM Driver Implementations
 */

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->lcm_if = LCM_INTERFACE_DSI_DUAL;
	params->lcm_cmd_if = LCM_INTERFACE_DSI0;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = BURST_VDO_MODE;
#endif

	params->dsi.dual_dsi_type = DUAL_DSI_CMD;
	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM		= LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	/* Video mode setting */
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.packet_size = 256;
	params->dsi.ssc_disable = 1;
	/* params->dsi.ssc_range = 3; */

	params->dsi.vertical_sync_active	= 2;
	params->dsi.vertical_backporch		= 4;
	params->dsi.vertical_frontporch		= 20;
	params->dsi.vertical_active_line	= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active	= 30;
	params->dsi.horizontal_backporch	= 40;
	params->dsi.horizontal_frontporch	= 90;
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;

	/* Bit rate calculation */
	params->dsi.PLL_CLOCK = 450;

	params->dsi.ufoe_enable  = 1;
	params->dsi.ufoe_params.lr_mode_en = 1;
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(20);

	SET_RESET_PIN(0);
	MDELAY(20);

	SET_RESET_PIN(1);
	MDELAY(150);

	push_table(lcm_initialization_setting, ARRAY_SIZE(lcm_initialization_setting), 1);

#ifdef BUILD_LK
	dprintf(0, "[LK]push_table end\n");
#endif

}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting, ARRAY_SIZE(lcm_suspend_setting), 1);
	SET_RESET_PIN(0);
}

static void lcm_resume(void)
{
	lcm_init();
}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
		       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif

#define LCM_ID_S6E3FA2_ID (0x030000) /* ID1:0x03 ID2: 0x00 ID3:0x00 */

static unsigned int lcm_compare_id(void)
{
	unsigned char buffer[3] = {0};
	unsigned int array[16];
	unsigned int lcd_id = 0;

	SET_RESET_PIN(1);
	MDELAY(20);
	SET_RESET_PIN(0);
	MDELAY(20);
	SET_RESET_PIN(1);
	MDELAY(150);

	array[0] = 0x00033700; /*read id return two byte, version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x04, buffer, 3);
	MDELAY(20);
	lcd_id = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];

#ifdef BUILD_LK
	dprintf(0, "%s, LK s6e3ha3 debug: s6e3ha3 id = 0x%08x\n", __func__, lcd_id);
#else
	pr_debug("%s, kernel s6e3ha3 horse debug: s6e3ha3 id = 0x%08x\n", __func__, lcd_id);
#endif

	if (lcd_id == LCM_ID_S6E3FA2_ID)
		return 1;
	else
		return 0;

}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	/* Refresh value of backlight level. */
	unsigned int cmd_1 = 0x51;
	unsigned int count_1 = 1;
	unsigned int value_1 = level;
#if (LCM_DSI_CMD_MODE) /* NOP */
	unsigned int cmd = 0x00;
	unsigned int count = 0;
	unsigned int value = 0x00;

	dsi_set_cmd_by_cmdq_dual(handle, cmd, count, &value, 1);
#endif

#ifdef BUILD_LK
	dprintf(0, "%s,lk s6e3ha3 brightness: level = %d\n", __func__, level);
#else
	pr_debug("%s, kernel s6e3ha3 brightness: level = %d\n", __func__, level);
#endif

	dsi_set_cmd_by_cmdq_dual(handle, cmd_1, count_1, &value_1, 1);
}

LCM_DRIVER s6e3ha3_wqhd_2k_cmd_lcm_drv = {
	.name				= "s6e3ha3_wqhd_2k_cmd",
	.set_util_funcs			= lcm_set_util_funcs,
	.get_params			= lcm_get_params,
	.init				= lcm_init,
	.suspend			= lcm_suspend,
	.resume				= lcm_resume,
	.set_backlight_cmdq		= lcm_setbacklight_cmdq,
	.compare_id			= lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
	.update				= lcm_update,
#endif
};
