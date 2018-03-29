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
#include "b080uan01_2_wuxga_dsi_vdo.h"

#define FRAME_WIDTH	(1200)
#define FRAME_HEIGHT	(1920)

#define REGFLAG_DELAY	0xFE
#define REGFLAG_END_OF_TABLE	0xFF

#define LCM_DSI_CMD_MODE	0

unsigned int GPIO_LCD_PWR_EN;
unsigned int GPIO_LCD_RST_EN;

/* ---------------------------------------------------------------------------
 *  Local Variables
   --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util = {0};

#define GPIO_OUT_ONE  1
#define GPIO_OUT_ZERO 0

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))
/* ---------------------------------------
   Local Functions
 ---------------------------------------*/
#define dsi_set_cmdq(pdata, queue_size, force_update)	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)

static void lcm_init_register(void)
{
	unsigned int data_array[16];

	pr_debug("%s, kernel", __func__);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);

    /* 0x29. Display on */
	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);
	/* Backlight turn on, no need to control it */
}

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
}

static void lcd_poweron(unsigned char enabled)
{
	if (enabled > 0) {
		pr_debug("[Kernel/LCM] lcm_resume_power() enter\n");
		lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	} else {
		pr_debug("[Kernel/LCM] lcm_suspend_power() enter\n");
		lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);
	}
}

static void lcd_reset(unsigned char enabled)
{
	if (enabled > 0)
		lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	else
		lcm_set_gpio_output(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
}

/* ---------------------------------------------------------------------------
 *  LCM Driver Implementations
 --------------------------------------------------------------------------- */
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
	params->dsi.mode   = SYNC_EVENT_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/*The following defined the fomat for data coming from LCD engine.*/
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability.*/
	params->dsi.word_count = FRAME_WIDTH*3;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 4;
	params->dsi.vertical_frontporch = 12;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 8;
	params->dsi.horizontal_backporch = 60;
	params->dsi.horizontal_frontporch = 60;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.ufoe_enable = 0;
	params->dsi.edp_panel = 1;

	params->dsi.PLL_CLOCK = 480;
	params->dsi.ssc_disable = 1;
}

static void lcm_init_power(void)
{
	lcd_reset(0);
	MDELAY(5);

	lcd_poweron(0);
	MDELAY(5);

	lcd_poweron(1);
	MDELAY(10);

	lcd_reset(1);
	MDELAY(5);

	lcd_reset(0);
	MDELAY(5);

	lcd_reset(1);
	MDELAY(10);
}


static void lcm_suspend_power(void)
{
	lcd_reset(0);
	MDELAY(150);

	lcd_poweron(0);
	MDELAY(550);
}


static void lcm_resume_power(void)
{
	lcm_init_power();
}

static void lcm_init(void)
{
	pr_debug("%s, Kernel\n", __func__);
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];

	data_array[0] = 0x00280500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00100500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);
}

static void lcm_resume(void)
{
	lcm_init_register();
}

LCM_DRIVER b080uan01_2_wuxga_dsi_vdo_lcm_drv = {
	.name			= "b080uan01_2_wuxga_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.init_power     = lcm_init_power,
	.suspend        = lcm_suspend,
	.suspend_power  = lcm_suspend_power,
	.resume         = lcm_resume,
	.resume_power	= lcm_resume_power,
};
