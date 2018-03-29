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
#endif
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <debug.h>
#include <platform/upmu_common.h>

#elif (defined BUILD_UBOOT)
#include <asm/arch/mt6577_gpio.h>
#else
#endif
#include "lcm_drv.h"
#include <linux/of_gpio.h>

/* ---------------------------------------------------------------------------
 *  Local Constants
 * ---------------------------------------------------------------------------
 */

#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (1280)
#define HSYNC_PULSE_WIDTH 16
#define HSYNC_BACK_PORCH  16
#define HSYNC_FRONT_PORCH 32
#define VSYNC_PULSE_WIDTH 2
#define VSYNC_BACK_PORCH  2
#define VSYNC_FRONT_PORCH 4

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))
/* ---------------------------------------------------------------------------
   Local Variables
   ---------------------------------------------------------------------------
 */
static LCM_UTIL_FUNCS lcm_util = {0};
static unsigned int GPIO_LCD_PWR_EN;
static bool fgisfirst = true;

void lcm_get_gpio(void)
{
	static struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,lcm");

	GPIO_LCD_PWR_EN = of_get_named_gpio(node, "lcm_power_gpio", 0);
}

/* ---------------------------------------------------------------------------
    Local Functions
   ---------------------------------------------------------------------------
*/
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
}

/* ---------------------------------------------------------------------------
    LCM Driver Implementations
   ---------------------------------------------------------------------------
*/
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DPI;
	params->ctrl   = LCM_CTRL_SERIAL_DBI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->io_select_mode = 0;

	params->dpi.PLL_CLOCK = 67;

	params->dpi.clk_pol = LCM_POLARITY_FALLING;
	params->dpi.de_pol = LCM_POLARITY_RISING;
	params->dpi.vsync_pol = LCM_POLARITY_FALLING;
	params->dpi.hsync_pol = LCM_POLARITY_FALLING;

	params->dpi.hsync_pulse_width = HSYNC_PULSE_WIDTH;
	params->dpi.hsync_back_porch = HSYNC_BACK_PORCH;
	params->dpi.hsync_front_porch = HSYNC_FRONT_PORCH;
	params->dpi.vsync_pulse_width = VSYNC_PULSE_WIDTH;
	params->dpi.vsync_back_porch = VSYNC_BACK_PORCH;
	params->dpi.vsync_front_porch = VSYNC_FRONT_PORCH;

	params->dpi.lvds_tx_en = 1;
	params->dpi.ssc_disable = 1;
	params->dpi.format = LCM_DPI_FORMAT_RGB888;
	params->dpi.rgb_order = LCM_COLOR_ORDER_RGB;
	params->dpi.is_serial_output  = 0;
	params->dpi.intermediat_buffer_num = 0;
}

static void lcm_init(void)
{
}

static void lcm_suspend(void)
{
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
	if (fgisfirst) {
		fgisfirst = false;
		lcm_vgp_supply_enable();
	}

	MDELAY(20);
	lcm_vgp_supply_disable();
}

static void lcm_resume(void)
{
	lcm_vgp_supply_enable();
	MDELAY(30);
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
}

LCM_DRIVER cpt_clap070wp03xg_lvds_lcm_drv = {
	.name = "cpt_clap070wp03xg_lvds",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
};
