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

#ifndef _CPT_clap070wp03xg_sn65dsi83_
#define _CPT_clap070wp03xg_sn65dsi83_

#include "ddp_hal.h"

typedef unsigned char kal_uint8;
typedef struct {
	unsigned char cmd;
	unsigned char data;
} sn65dsi8x_setting_table;

extern unsigned int GPIO_LCD_PWR_EN;
extern unsigned int GPIO_LCD_PWR2_EN;
extern unsigned int GPIO_LCD_RST_EN;
extern unsigned int GPIO_LCD_STB_EN;
extern unsigned int GPIO_LCD_BRIDGE_EN;

int lcm_vgp_supply_disable(void);
int lcm_vgp_supply_enable(void);

extern void DSI_clk_HS_mode(DISP_MODULE_ENUM module, void *cmdq, bool enter);

#endif
