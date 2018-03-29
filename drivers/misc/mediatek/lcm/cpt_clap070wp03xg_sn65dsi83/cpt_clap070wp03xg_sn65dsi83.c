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
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#else
#include <string.h>
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include "ddp_hal.h"
#else
#include "sn65dsi83_i2c.h"
#endif

#include "lcm_drv.h"
#include "cpt_clap070wp03xg_sn65dsi83.h"

#define LVDS_PANEL_8BITS_SUPPORT

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)

#define GPIO_OUT_ONE  1
#define GPIO_OUT_ZERO 0

#define SN65DSI_DEBUG		/* for check system(bb dsi and ti chip) status */
/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util = {
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};

unsigned int GPIO_LCD_PWR_EN;
unsigned int GPIO_LCD_PWR2_EN;
unsigned int GPIO_LCD_RST_EN;
unsigned int GPIO_LCD_STB_EN;
unsigned int GPIO_LCD_BRIDGE_EN;

#define SET_RESET_PIN(v)    (mt_set_reset_pin((v)))
#define UDELAY(n)					(lcm_util.udelay(n))
#define MDELAY(n)					(lcm_util.mdelay(n))
#define REGFLAG_DELAY			0xAB

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

/* sn65dis83 chip init table */
static sn65dsi8x_setting_table sn65dis83_init_table[] = {
#if defined(LVDS_PANEL_8BITS_SUPPORT)
	{0x09, 0x00},
	{0x0A, 0x05},
	{0x0B, 0x10},
	{0x0D, 0x00},
	{0x10, 0x26},
	{0x11, 0x00},
	{0x12, 0x2c},
	{0x13, 0x00},
	{0x18, 0x78},
	{0x19, 0x00},
	{0x1A, 0x03},
	{0x1B, 0x00},
	{0x20, 0x20},
	{0x21, 0x03},
	{0x22, 0x00},
	{0x23, 0x00},
	{0x24, 0x00},
	{0x25, 0x00},		/* 0x00 */
	{0x26, 0x00},
	{0x27, 0x00},
	{0x28, 0x21},
	{0x29, 0x00},
	{0x2A, 0x00},
	{0x2B, 0x00},
	{0x2C, 0x0a},
	{0x2D, 0x00},
	{0x2E, 0x00},
	{0x2F, 0x00},
	{0x30, 0x02},
	{0x31, 0x00},
	{0x32, 0x00},
	{0x33, 0x00},
	{0x34, 0x18},
	{0x35, 0x00},
	{0x36, 0x00},		/* 0x00 */
	{0x37, 0x00},
	{0x38, 0x00},		/* 0x00 */
	{0x39, 0x00},
	{0x3A, 0x00},		/* 0x00 */
	{0x3B, 0x00},
	{0x3C, 0x00},		/* 0x00 */
	{0x3D, 0x00},
	{0x3E, 0x00},
	{0x0D, 0x01},
	{REGFLAG_DELAY, 0x0a},
	{0x09, 0x01},
	{0xFF, 0x00},
#endif
};

static void push_table(sn65dsi8x_setting_table *table, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].data);
			break;

		case 0xFF:
			break;

		default:
#ifdef BUILD_LK
			sn65dsi83_write_byte(cmd, table[i].data);
#else
			sn65dsi83_write_byte(cmd, table[i].data);
#endif
		}
	}
}

#ifdef SN65DSI_DEBUG
static void dump_reg_table(sn65dsi8x_setting_table *table, unsigned int count)
{
	unsigned int i;
	unsigned char data;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].data);
			break;

		case 0xFF:
			break;

		default:
#ifdef BUILD_LK
			sn65dsi83_read_byte(cmd, &data);
			printf("dump cmd=0x%x  data=0x%x\n", cmd, data);
#else
			sn65dsi83_read_byte(cmd, &data);
			pr_debug("dump cmd=0x%x  data=0x%x\n", cmd, data);
#endif
		}
	}
}
#endif

void init_sn65dsi8x(void)
{
#ifdef SN65DSI_DEBUG
	unsigned char data;
#endif

	push_table(sn65dis83_init_table,
		   sizeof(sn65dis83_init_table) / sizeof(sn65dsi8x_setting_table));

#ifdef SN65DSI_DEBUG
	sn65dsi83_write_byte(0xe0, 1);
	sn65dsi83_write_byte(0xe1, 0xff);
	MDELAY(5);

	sn65dsi83_read_byte(0xe5, &data);
#ifdef BUILD_LK
	printf("dump cmd=0xe5  data=0x%x\n", data);
#else
	pr_debug("dump cmd=0xe5  data=0x%x\n", data);
#endif
	dump_reg_table(sn65dis83_init_table,
		       sizeof(sn65dis83_init_table) / sizeof(sn65dsi8x_setting_table));
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

	params->dsi.mode = SYNC_EVENT_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;

	/* The following defined the fomat for data coming from LCD engine. */
#if defined(LVDS_PANEL_8BITS_SUPPORT)
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
#else
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB666;
#endif

	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 2;
	params->dsi.vertical_frontporch = 4;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 24;
	params->dsi.horizontal_frontporch = 30;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

#if defined(LVDS_PANEL_8BITS_SUPPORT)
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
#else
	params->dsi.PS = LCM_PACKED_PS_18BIT_RGB666;
#endif

	params->dsi.pll_select = 0;	/* 0: MIPI_PLL; 1: LVDS_PLL */
	params->dsi.PLL_CLOCK = 224;
	params->dsi.cont_clock = 1;
}

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	if (GPIO == 0xFFFFFFFF) {
#ifdef BUILD_LK
		printf("[LK/LCM] GPIO_LCD_PWR_EN =   0x%x\n", GPIO_LCD_PWR_EN);
		printf("[LK/LCM] GPIO_LCD_PWR2_EN =  0x%x\n", GPIO_LCD_PWR2_EN);
		printf("[LK/LCM] GPIO_LCD_RST_EN =  0x%x\n", GPIO_LCD_RST_EN);
		printf("[LK/LCM] GPIO_LCD_STB_EN =   0x%x\n", GPIO_LCD_STB_EN);
		printf("[LK/LCM] GPIO_LCD_BRIDGE_EN =  0x%x\n", GPIO_LCD_BRIDGE_EN);
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

static void lcm_init(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM]lcm_init\n");

	/* step1: sn65dsi8x enbable and init */
	/* GPIO42       VDD18_LVDS for SN65DSI83 power */
	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ONE);
	MDELAY(20);

	/* GPIO127       LVDS_EN_R for SN65DSI83 */
	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ONE);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ONE);
	MDELAY(50);

	DSI_clk_HS_mode(0, NULL, 1);
	MDELAY(5);

	init_sn65dsi8x();
	MDELAY(10);

	/* step 2 :lvds lcd init */
	/* GPIO131        VDD33_LCD */
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(50);

	/* VGP6_PMU       AVDDVGH/VGL/VCOM for panel power */
	upmu_set_rg_vgp6_vosel(0x7);
	upmu_set_rg_vgp6_sw_en(0x1);

	/* GPIO103       LCM_RST for panel */
	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	MDELAY(50);

	/* GPIO102       LCM_STBY_2V8 for panel */
	lcm_set_gpio_output(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
#elif (defined BUILD_UBOOT)
#else
	pr_debug("[Kernel/LCM]lcm_init\n");

	DSI_clk_HS_mode(0, NULL, 1);
#endif
}

static void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM]lcm_suspend enter\n");
#else
	unsigned char temp;

	pr_debug("[Kernel/LCM]lcm_suspend enter\n");

	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ZERO);
	MDELAY(30);		/* avoid LCD resume transint */

	/* step 1 power down lvds lcd */
	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ZERO);
	MDELAY(10);

	/* LCMBIASON :VGH VHL */
	lcm_vgp_supply_enable();
	MDELAY(20);

	lcm_vgp_supply_disable();
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);	/* LCM VCC :enable LCD VCC */
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_STB_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);	/* LCM_STBY */
	MDELAY(50);

	/* step 2 suspend sn65dsi8x */
	sn65dsi83_read_byte(0x0a, &temp);	/* for test wether ti lock the pll clok */
	pr_debug("lcm_suspend  0x0a  value=0x%x\n", temp);

	sn65dsi83_read_byte(0x0d, &temp);
	pr_debug("lcm_suspend  0x0d  value=0x%x\n", temp);

	sn65dsi83_write_byte(0x0d, (temp & 0xfe));	/* set bit0: 0 */

	/* step 3 set dsi LP mode */
	DSI_clk_HS_mode(0, NULL, 0);
#endif
}


static void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM]lcm_resume enter\n");

#else
#ifdef SN65DSI_DEBUG
	unsigned char temp;
#endif

	pr_debug("[Kernel/LCM]lcm_resume enter\n");

	DSI_clk_HS_mode(0, NULL, 1);
	MDELAY(50);

	/* step 1 resume sn65dsi8x */
	lcm_set_gpio_output(GPIO_LCD_PWR2_EN, GPIO_OUT_ONE);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ONE);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ZERO);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCD_BRIDGE_EN, GPIO_OUT_ONE);
	MDELAY(10);

	init_sn65dsi8x();
	MDELAY(10);

#ifdef SN65DSI_DEBUG
	sn65dsi83_read_byte(0x0a, &temp);
	pr_debug("lcm_resume cmd-- 0x0a=0x%x\n", temp);

	sn65dsi83_read_byte(0x0d, &temp);
	pr_debug("lcm_resume cmd-- 0x0d=0x%x\n", temp);

	sn65dsi83_read_byte(0x09, &temp);
	pr_debug("lcm_resume cmd-- 0x09=0x%x\n", temp);
#endif

	/* step 2 resume lvds */
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(50);

	/* hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM"); */
	lcm_vgp_supply_enable();
	MDELAY(30);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_STB_EN, GPIO_OUT_ONE);
	MDELAY(5);
#endif
}

static unsigned int lcm_compare_id(void)
{
#if defined(BUILD_LK)
	printf("Sn65dsi83 lcm_compare_id enter\n");
#endif

	return 1;
}

LCM_DRIVER cpt_clap070wp03xg_sn65dsi83_lcm_drv = {
	.name = "cpt_clap070wp03xg_sn65dsi83",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
};
