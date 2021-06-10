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
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "data_hw_roundedpattern.h"
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
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_NT36672A (0x6A)

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
#endif

#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (2160)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH (68040)
#define LCM_PHYSICAL_HEIGHT (136080)
#define LCM_DENSITY (480)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x4F, 1, {0x01} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting[] = {
	{0xFF, 1, {0x20} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x06, 1, {0xAE} },
	{0x07, 1, {0xB6} },
	{0x61, 1, {0x81} },
	{0x62, 1, {0xD6} },
	{0x69, 1, {0x99} },
	{0x6D, 1, {0x44} },
	{0x89, 1, {0x40} },

	{0xFF, 1, {0x21} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },

	{0xFF, 1, {0x24} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0xFE, 1, {0x24} },
	{0x00, 1, {0x1C} },
	{0x01, 1, {0x1C} },
	{0x02, 1, {0x1C} },
	{0x03, 1, {0x1C} },
	{0x04, 1, {0x14} },
	{0x05, 1, {0x12} },
	{0x06, 1, {0x10} },
	{0x07, 1, {0x14} },
	{0x08, 1, {0x12} },
	{0x09, 1, {0x10} },
	{0x0A, 1, {0x20} },
	{0x0B, 1, {0x1E} },
	{0x0C, 1, {0x06} },
	{0x0D, 1, {0x05} },
	{0x0E, 1, {0x04} },
	{0x0F, 1, {0x03} },
	{0x10, 1, {0x24} },
	{0x11, 1, {0x0D} },
	{0x12, 1, {0x09} },
	{0x13, 1, {0x0A} },
	{0x14, 1, {0x01} },
	{0x15, 1, {0x1C} },
	{0x16, 1, {0x1C} },
	{0x17, 1, {0x1C} },
	{0x18, 1, {0x1C} },
	{0x19, 1, {0x1C} },
	{0x1A, 1, {0x1C} },
	{0x1B, 1, {0x14} },
	{0x1C, 1, {0x12} },
	{0x1D, 1, {0x10} },
	{0x1E, 1, {0x14} },
	{0x1F, 1, {0x12} },
	{0x20, 1, {0x10} },
	{0x21, 1, {0x00} },
	{0x22, 1, {0x1E} },
	{0x23, 1, {0x06} },
	{0x24, 1, {0x05} },
	{0x25, 1, {0x04} },
	{0x26, 1, {0x03} },
	{0x27, 1, {0x24} },
	{0x28, 1, {0x0D} },
	{0x29, 1, {0x09} },
	{0x2A, 1, {0x0A} },
	{0x2B, 1, {0x01} },
	{0x2D, 1, {0x1C} },
	{0x2F, 1, {0x1C} },
	{0x35, 1, {0x49} },
	{0x37, 1, {0x00} },
	{0x38, 1, {0x7A} },
	{0x39, 1, {0x7A} },
	{0x3F, 1, {0x7A} },
	{0x4C, 1, {0x1C} },
	{0x4D, 1, {0x1C} },
	{0x5B, 1, {0x04} },
	{0x64, 1, {0x01} },
	{0x68, 1, {0x82} },
	{0x72, 1, {0x00} },
	{0x73, 1, {0x00} },
	{0x74, 1, {0x00} },
	{0x75, 1, {0x00} },
	{0x7A, 1, {0x04} },
	{0x7B, 1, {0xA0} },
	{0x7D, 1, {0x05} },
	{0x80, 1, {0x42} },
	{0x82, 1, {0x11} },
	{0x83, 1, {0x22} },
	{0x84, 1, {0x33} },
	{0x85, 1, {0x00} },
	{0x86, 1, {0x00} },
	{0x87, 1, {0x00} },
	{0x88, 1, {0x11} },
	{0x89, 1, {0x22} },
	{0x8A, 1, {0x33} },
	{0x8B, 1, {0x00} },
	{0x8C, 1, {0x00} },
	{0x8D, 1, {0x00} },
	{0x92, 1, {0x7C} },
	{0xC4, 1, {0xA9} },
	{0xDC, 1, {0x09} },
	{0xDD, 1, {0x10} },
	{0xDE, 1, {0x11} },
	{0xDF, 1, {0x00} },
	{0xE0, 1, {0x00} },
	{0xE1, 1, {0x01} },
	{0xE2, 1, {0x00} },
	{0xE4, 1, {0x00} },
	{0xE5, 1, {0x09} },
	{0xEB, 1, {0x04} },

	{0xFF, 1, {0x25} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x21, 1, {0x20} },
	{0x22, 1, {0x20} },
	{0x24, 1, {0x7C} },
	{0x25, 1, {0x7C} },
	{0x30, 1, {0x26} },
	{0x31, 1, {0x4B} },
	{0x38, 1, {0x26} },
	{0x3F, 1, {0x21} },
	{0x40, 1, {0x88} },
	{0x41, 1, {0xDB} },
	{0x4B, 1, {0x21} },
	{0x4C, 1, {0x88} },
	{0x4D, 1, {0xDB} },
	{0x84, 1, {0x75} },

	{0xFF, 1, {0x26} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x1A, 1, {0x9F} },
	{0x1D, 1, {0x0B} },
	{0x1E, 1, {0x22} },
	{0x99, 1, {0x10} },
	{0x9A, 1, {0x94} },
	{0x9E, 1, {0x00} },
	{0xAE, 1, {0x4A} },

	{0xFF, 1, {0x27} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x13, 1, {0x00} },

	{0xFF, 1, {0xD0} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0x1C, 1, {0x66} },
	{0x1D, 1, {0x06} },

	{0xFF, 1, {0xF0} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0xA2, 1, {0x00} },

	{0xFF, 1, {0x10} },
	{REGFLAG_DELAY, 100, {} },
	{0xFB, 1, {0x01} },
	{0xBA, 1, {0x02} },
#if (LCM_DSI_CMD_MODE)
	{0xBB, 1, {0x10} },
#else
	{0xBB, 1, {0x03} },
#endif
	{0x53, 1, {0x24} },/*need change*/

	{0x11, 0, {} },

#ifndef LCM_SET_DISPLAY_ON_DELAY
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
#endif

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
	params->dsi.IsCphy = 1;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	LCM_LOGI("%s lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_THREE_LANE;

	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	/* video mode timing */
	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 20;
	//params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;

#if (LCM_DSI_CMD_MODE)
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 480;
#else
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 300;
#endif

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

	params->dsi.lane_swap_en = 0;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->corner_pattern_height = ROUND_CORNER_H_TOP;
	params->corner_pattern_height_bot = ROUND_CORNER_H_BOT;
	params->corner_pattern_tp_size = sizeof(top_rc_pattern);
	params->corner_pattern_lt_addr = (void *)top_rc_pattern;
#endif
}

static void lcm_init_power(void)
{
	display_bias_enable();
}

static void lcm_suspend_power(void)
{
	display_bias_disable();
}

static void lcm_resume_power(void)
{
	SET_RESET_PIN(0);
	display_bias_enable();
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
	push_table(NULL, init_setting,
		sizeof(init_setting)/sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting,
		sizeof(lcm_suspend_setting)/
		sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
	/* SET_RESET_PIN(0); */
}

static void lcm_resume(void)
{
	lcm_init();
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
	unsigned int id = 0, version_id = 0;
	unsigned char buffer[2];
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00013700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	array[0] = 0x20FF2300;
	dsi_set_cmdq(array, 1, 1);
	array[0] = 0x01FB2300;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x3B, buffer, 1);
	id = buffer[0];		/* we only need ID */

	read_reg_v2(0x3A, buffer, 1);
	version_id = buffer[0];

	array[0] = 0x10FF2300;
	dsi_set_cmdq(array, 1, 1);
	array[0] = 0x01FB2300;
	dsi_set_cmdq(array, 1, 1);

	LCM_LOGI("%s,nt36672a_id=0x%08x,version_id=0x%x\n",
		 __func__, id, version_id);

	if (id == LCM_ID_NT36672A && version_id == 0x00)
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
		LCM_LOGI("[LCM ERROR] [0x0A]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x0A]=0x%02x\n", buffer[0]);
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

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n",
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

	LCM_LOGI("%s,nt35695 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
		   sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
	return NULL;
}

#if (LCM_DSI_CMD_MODE)
/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty.
	 * In this case we need to use minimu roi
	 */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		pr_debug("%s calc error,assign full roi:y=%d,h=%d\n",
			 __func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*pr_debug("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n", */
	/*      *x, *y, *width, *height, x1, y1, w, h); */

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif

#if (LCM_DSI_CMD_MODE)
struct LCM_DRIVER nt36672a_fhdp_dsi_cmd_auo_cphy_lcm_drv = {
	.name = "nt36672a_fhdp_dsi_cmd_auo_cphy_lcm_drv",
#else
struct LCM_DRIVER nt36672a_fhdp_dsi_vdo_auo_cphy_lcm_drv = {
	.name = "nt36672a_fhdp_dsi_vdo_auo_cphy_lcm_drv",
#endif
	.set_util_funcs = lcm_set_util_funcs,
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
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
