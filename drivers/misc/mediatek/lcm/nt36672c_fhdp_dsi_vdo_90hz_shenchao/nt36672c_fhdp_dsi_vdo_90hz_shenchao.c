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

#ifdef CONFIG_MTK_MT6382_BDG
#define DSC_ENABLE
#endif

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;
static int regulator_inited;
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[500];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0XFF, 1, {0X10} },
	//REGR 0XFE, 1, {0X10} },
	{0XFB, 1, {0X01} },
	{0XB0, 1, {0X00} },
	{0XC0, 1, {0X03} },
	{0XC1, 16, {0X89, 0X28, 0X00, 0X08, 0X00, 0XAA, 0X02, 0X0E, 0X00,
		    0X2B, 0X00, 0X07, 0X0D, 0XB7, 0X0C, 0XB7} },
	{0XC2, 2, {0X1B, 0XA0} },

	{0XFF, 1, {0X20} },
	//REGR 0XFE, 1, {0X20} },
	{0XFB, 1, {0X01} },
	{0X01, 1, {0X66} },
	{0X06, 1, {0X50} },
	{0X07, 1, {0X28} },
	{0X0E, 1, {0X00} },
	{0X17, 1, {0X66} },
	{0X1B, 1, {0X01} },
	{0X5C, 1, {0X90} },
	{0X5E, 1, {0XA0} },
	{0X69, 1, {0XD0} },

	{0X19, 1, {0X55} },
	{0X32, 1, {0X3D} },
	{0X69, 1, {0XAA} },
	{0X95, 1, {0XD1} },
	{0X96, 1, {0XD1} },
	{0XF2, 1, {0X66} },
	{0XF3, 1, {0X44} },
	{0XF4, 1, {0X66} },
	{0XF3, 1, {0X44} },
	{0XF6, 1, {0X66} },
	{0XF3, 1, {0X44} },
	{0XF8, 1, {0X66} },
	{0XF3, 1, {0X44} },

	{0XFF, 1, {0X21} },
	//REGR 0XFE, 1, {0X10} },
	{0XFB, 1, {0X01} },

	{0XFF, 1, {0X24} },
	//REGR 0XFE, 1, {0X24} },
	{0XFB, 1, {0X01} },
	{0X00, 1, {0X1C} },
	{0X01, 1, {0X01} },
	{0X04, 1, {0X2C} },
	{0X05, 1, {0X2D} },
	{0X06, 1, {0X2E} },
	{0X07, 1, {0X2F} },
	{0X08, 1, {0X30} },
	{0X09, 1, {0X0F} },
	{0X0A, 1, {0X11} },
	{0X0C, 1, {0X14} },
	{0X0D, 1, {0X16} },
	{0X0E, 1, {0X18} },
	{0X0F, 1, {0X13} },
	{0X10, 1, {0X15} },
	{0X11, 1, {0X17} },
	{0X13, 1, {0X10} },
	{0X14, 1, {0X22} },
	{0X15, 1, {0X22} },
	{0X18, 1, {0X1C} },
	{0X19, 1, {0X01} },
	{0X1C, 1, {0X2C} },
	{0X1D, 1, {0X2D} },
	{0X1E, 1, {0X2E} },
	{0X1F, 1, {0X2F} },
	{0X20, 1, {0X30} },
	{0X21, 1, {0X0F} },
	{0X22, 1, {0X11} },
	{0X24, 1, {0X14} },
	{0X25, 1, {0X16} },
	{0X26, 1, {0X18} },
	{0X27, 1, {0X13} },
	{0X28, 1, {0X15} },
	{0X29, 1, {0X17} },
	{0X2B, 1, {0X10} },
	{0X2D, 1, {0X22} },
	{0X2F, 1, {0X22} },
	{0X32, 1, {0X44} },
	{0X33, 1, {0X00} },
	{0X34, 1, {0X00} },
	{0X35, 1, {0X01} },
	{0X36, 1, {0X3F} },
	{0X36, 1, {0X3F} },
	{0X37, 1, {0X00} },
	{0X38, 1, {0X00} },
	{0X4D, 1, {0X02} },
	{0X4E, 1, {0X3A} },
	{0X4F, 1, {0X3A} },
	{0X53, 1, {0X3A} },
	{0X7A, 1, {0X83} },
	{0X7B, 1, {0X90} },
	{0X7D, 1, {0X03} },
	{0X80, 1, {0X03} },
	{0X81, 1, {0X03} },
	{0X82, 1, {0X13} },
	{0X84, 1, {0X31} },
	{0X85, 1, {0X00} },
	{0X86, 1, {0X00} },
	{0X87, 1, {0X00} },
	{0X90, 1, {0X13} },
	{0X92, 1, {0X31} },
	{0X93, 1, {0X00} },
	{0X94, 1, {0X00} },
	{0X95, 1, {0X00} },
	{0X9C, 1, {0XF4} },
	{0X9D, 1, {0X01} },
	{0XA0, 1, {0X10} },
	{0XA2, 1, {0X10} },
	{0XA3, 1, {0X03} },
	{0XA4, 1, {0X03} },
	{0XA5, 1, {0X03} },
	{0XC4, 1, {0X80} },
	{0XC6, 1, {0XC0} },
	{0XC9, 1, {0X00} },
	{0XD1, 1, {0X34} },
	{0XD9, 1, {0X80} },
	{0XE9, 1, {0X03} },

	{0XFF, 1, {0X25} },
	//REGR 0XFE, 1, {0X25} },
	{0XFB, 1, {0X01} },
	{0X0F, 1, {0X1B} },
	{0X18, 1, {0X21} },
	{0X19, 1, {0XE4} },
	{0X21, 1, {0X40} },
	{0X68, 1, {0X58} },
	{0X69, 1, {0X10} },
	{0X6B, 1, {0X00} },
	{0X6C, 1, {0X1D} },
	{0X71, 1, {0X1D} },
	{0X77, 1, {0X72} },
	{0X7F, 1, {0X00} },
	{0X81, 1, {0X00} },
	{0X84, 1, {0X6D} },
	{0X86, 1, {0X2D} },
	{0X8D, 1, {0X00} },
	{0X8E, 1, {0X14} },
	{0X8F, 1, {0X04} },
	{0XC0, 1, {0X03} },
	{0XC1, 1, {0X19} },
	{0XC3, 1, {0X03} },
	{0XC4, 1, {0X11} },
	{0XC6, 1, {0X00} },
	{0XEF, 1, {0X00} },
	{0XF1, 1, {0X04} },

	{0XFF, 1, {0X26} },
	//REGR 0XFE, 1, {0X26} },
	{0XFB, 1, {0X01} },
	{0X00, 1, {0X10} },
	{0X01, 1, {0XEB} },
	{0X03, 1, {0X01} },
	{0X04, 1, {0X9A} },
	{0X06, 1, {0X11} },
	{0X08, 1, {0X96} },
	{0X14, 1, {0X02} },
	{0X15, 1, {0X01} },
	{0X74, 1, {0XAF} },
	{0X81, 1, {0X10} },
	{0X83, 1, {0X03} },
	{0X84, 1, {0X02} },
	{0X85, 1, {0X01} },
	{0X86, 1, {0X02} },
	{0X87, 1, {0X01} },
	{0X88, 1, {0X05} },
	{0X8A, 1, {0X1A} },
	{0X8B, 1, {0X11} },
	{0X8C, 1, {0X24} },
	{0X8E, 1, {0X42} },
	{0X8F, 1, {0X11} },
	{0X90, 1, {0X11} },
	{0X91, 1, {0X11} },
	{0X9A, 1, {0X80} },
	{0X9B, 1, {0X04} },
	{0X9C, 1, {0X00} },
	{0X9D, 1, {0X00} },
	{0X9E, 1, {0X00} },

	{0XFF, 1, {0X27} },
	//REGR 0XFE, 1, {0X27} },
	{0XFB, 1, {0X01} },
	{0X01, 1, {0X60} },
	{0X20, 1, {0X81} },
	{0X21, 1, {0X71} },
	{0X25, 1, {0X81} },
	{0X26, 1, {0X99} },
	{0X6E, 1, {0X00} },
	{0X6F, 1, {0X00} },
	{0X70, 1, {0X00} },
	{0X71, 1, {0X00} },
	{0X72, 1, {0X00} },
	{0X75, 1, {0X04} },
	{0X76, 1, {0X00} },
	{0X77, 1, {0X00} },
	{0X7D, 1, {0X09} },
	{0X7E, 1, {0X63} },
	{0X80, 1, {0X24} },
	{0X82, 1, {0X09} },
	{0X83, 1, {0X63} },
	{0XE3, 1, {0X01} },
	{0XE4, 1, {0XEC} },
	{0XE5, 1, {0X00} },
	{0XE6, 1, {0X7B} },
	{0XE9, 1, {0X02} },
	{0XEA, 1, {0X22} },
	{0XEB, 1, {0X00} },
	{0XEC, 1, {0X7B} },

	{0XFF, 1, {0X2A} },
	//REGR 0XFE, 1, {0X10} },
	{0XFB, 1, {0X01} },
	{0X00, 1, {0X91} },
	{0X03, 1, {0X20} },
	{0X07, 1, {0X64} },
	{0X0A, 1, {0X60} },
	{0X0C, 1, {0X04} },
	{0X0D, 1, {0X40} },
	{0X0F, 1, {0X01} },
	{0X11, 1, {0XE0} },
	{0X15, 1, {0X0E} },
	{0X16, 1, {0XB6} },
	{0X19, 1, {0X0E} },
	{0X1A, 1, {0X8A} },
	{0X1F, 1, {0X40} },
	{0X28, 1, {0XFD} },
	{0X29, 1, {0X1F} },
	{0X2A, 1, {0XFF} },
	{0X2D, 1, {0X0A} },
	{0X30, 1, {0X4F} },
	{0X31, 1, {0XE7} },
	{0X33, 1, {0X73} },
	{0X34, 1, {0XFF} },
	{0X35, 1, {0X3B} },
	{0X36, 1, {0XE6} },
	{0X36, 1, {0XE6} },
	{0X37, 1, {0XF9} },
	{0X38, 1, {0X40} },
	{0X39, 1, {0XE1} },
	{0X3A, 1, {0X4F} },
	{0X45, 1, {0X06} },
	{0X46, 1, {0X40} },
	{0X47, 1, {0X02} },
	{0X48, 1, {0X01} },
	{0X4A, 1, {0X56} },
	{0X4E, 1, {0X0E} },
	{0X4F, 1, {0XB6} },
	{0X52, 1, {0X0E} },
	{0X53, 1, {0X8A} },
	{0X57, 1, {0X55} },
	{0X58, 1, {0X55} },
	{0X59, 1, {0X55} },
	{0X60, 1, {0X80} },
	{0X61, 1, {0XFD} },
	{0X62, 1, {0X0D} },
	{0X63, 1, {0XC2} },
	{0X64, 1, {0X06} },
	{0X65, 1, {0X08} },
	{0X66, 1, {0X01} },
	{0X67, 1, {0X48} },
	{0X68, 1, {0X17} },
	{0X6A, 1, {0X8D} },
	{0X6B, 1, {0XFF} },
	{0X6C, 1, {0X2D} },
	{0X6D, 1, {0X8C} },
	{0X6E, 1, {0XFA} },
	{0X6F, 1, {0X31} },
	{0X70, 1, {0X88} },
	{0X71, 1, {0X48} },
	{0X7A, 1, {0X13} },
	{0X7B, 1, {0X90} },
	{0X7F, 1, {0X75} },
	{0X83, 1, {0X07} },
	{0X84, 1, {0XF9} },
	{0X87, 1, {0X07} },
	{0X88, 1, {0X77} },

	{0XFF, 1, {0X2C} },
	//REGR 0XFE, 1, {0X10} },
	{0XFB, 1, {0X01} },
	{0X03, 1, {0X17} },
	{0X04, 1, {0X17} },
	{0X05, 1, {0X17} },
	{0X0D, 1, {0X01} },
	{0X0E, 1, {0X54} },
	{0X17, 1, {0X4B} },
	{0X18, 1, {0X4B} },
	{0X19, 1, {0X4B} },
	{0X2D, 1, {0XAF} },

	{0X2F, 1, {0X10} },
	{0X30, 1, {0XEB} },
	{0X32, 1, {0X01} },
	{0X33, 1, {0X9A} },
	{0X35, 1, {0X16} },
	{0X37, 1, {0X96} },
	{0X37, 1, {0X96} },
	{0X4D, 1, {0X17} },
	{0X4E, 1, {0X04} },
	{0X4F, 1, {0X04} },
	{0X61, 1, {0X04} },
	{0X62, 1, {0X68} },
	{0X6B, 1, {0X71} },
	{0X6C, 1, {0X71} },
	{0X6D, 1, {0X71} },
	{0X81, 1, {0X11} },
	{0X82, 1, {0X84} },
	{0X84, 1, {0X01} },
	{0X85, 1, {0X9A} },
	{0X87, 1, {0X29} },
	{0X89, 1, {0X96} },
	{0X9D, 1, {0X1B} },
	{0X9E, 1, {0X08} },
	{0X9F, 1, {0X10} },

	{0XFF, 1, {0XE0} },
	//REGR 0XFE, 1, {0XE0} },
	{0XFB, 1, {0X01} },
	{0X35, 1, {0X82} },
	{0X25, 1, {0X00} },
	{0X25, 1, {0X00} },
	{0X4E, 1, {0X00} },

	{0XFF, 1, {0XF0} },
	//REGR 0XFE, 1, {0X10} },
	{0XFB, 1, {0X01} },
	{0X1C, 1, {0X01} },
	{0X33, 1, {0X01} },
	{0X5A, 1, {0X00} },

	{0XFF, 1, {0XD0} },
	//REGR 0XFE, 1, {0XD0} },
	{0XFB, 1, {0X01} },
	{0X53, 1, {0X22} },
	{0X54, 1, {0X02} },
	{0XFF, 1, {0XC0} },
	{0XFB, 1, {0X01} },
	{0X9C, 1, {0X11} },
	{0X9D, 1, {0X11} },

	{0XFF, 1, {0X2B} },
	{0XFB, 1, {0X01} },
	{0XB7, 1, {0X13} },
	{0XB8, 1, {0X0F} },
	{0XC0, 1, {0X03} },

	{0XFF, 1, {0X10} },
	{0X11, 0, {} },
	/* {REGFLAG_DELAY, 120, {} }, */
	/* DISPLAY ON */
	{0X29, 0, {} },
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

	/* DPFS_LEVEL0 */
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	/* dfps_params[0].PLL_CLOCK = 500; */
	/* dfps_params[0].data_rate = xx; */
	/* if vfp solution */
	dfps_params[0].vertical_frontporch = 1290;
	dfps_params[0].vertical_frontporch_for_low_power = 2466;

	/* DPFS_LEVEL1 */
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	/* dfps_params[1].PLL_CLOCK = 500 */
	/* dfps_params[1].data_rate = xx; */
	dfps_params[1].vertical_frontporch = 46;
	dfps_params[1].vertical_frontporch_for_low_power = 1290;
	dsi->dfps_num = 2;
}
#endif

static void lcm_get_params(struct LCM_PARAMS *params)
{
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
	params->dsi.vertical_backporch = 10;
	params->dsi.vertical_frontporch = 46;
	//params->dsi.vertical_frontporch_for_low_power = 750;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 22;
	params->dsi.horizontal_backporch = 22;
	params->dsi.horizontal_frontporch = 165;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
#ifdef CONFIG_MTK_MT6382_BDG
	params->dsi.bdg_ssc_disable = 1;
#endif
	params->dsi.dsc_enable = 0;
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
#ifdef DSC_ENABLE
	params->dsi.bdg_dsc_enable = 1;
	params->dsi.PLL_CLOCK = 380; //with dsc
#else
#ifdef CONFIG_MTK_MT6382_BDG
	params->dsi.bdg_dsc_enable = 0;
#endif
	//params->dsi.PLL_CLOCK = 500; //without dsc
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

	#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/****DynFPS start****/
	lcm_dfps_int(&(params->dsi));
	/****DynFPS end****/
	#endif
}

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
int lcm_bias_regulator_init(void)
{
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		pr_info("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_info("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}

int lcm_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 5500000, 5500000);
	if (ret < 0)
		pr_info("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5500000, 5500000);
	if (ret < 0)
		pr_info("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_info("enable regulator disp_bias_pos fail, ret = %d\n",
			ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_info("enable regulator disp_bias_neg fail, ret = %d\n",
			ret);
	retval |= ret;

	return retval;
}


int lcm_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		pr_info("disable regulator disp_bias_neg fail, ret = %d\n",
			ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_info("disable regulator disp_bias_pos fail, ret = %d\n",
			ret);
	retval |= ret;

	return retval;
}

#else
int lcm_bias_regulator_init(void)
{
	return 0;
}

int lcm_bias_enable(void)
{
	return 0;
}

int lcm_bias_disable(void)
{
	return 0;
}
#endif

/* turn on gate ic & control voltage to 5.5V */
/* equle display_bais_enable ,mt6768 need +/-5.5V */
static void lcm_init_power(void)
{
	lcm_bias_enable();
}

static void lcm_suspend_power(void)
{
	SET_RESET_PIN(0);
	lcm_bias_disable();
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
	LCM_LOGI("%s:%d\n", __func__, __LINE__);
	push_table(NULL, lcm_suspend_setting,
		   ARRAY_SIZE(lcm_suspend_setting), 1);
}

static void lcm_resume(void)
{
	LCM_LOGI("%s:%d\n", __func__, __LINE__);
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

	MDELAY(1);

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

struct LCM_DRIVER nt36672c_fhdp_dsi_vdo_90hz_shenchao_lcm_drv = {
	.name = "nt36672c_fhdp_dsi_vdo_90hz_shenchao_lcm_drv",
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

