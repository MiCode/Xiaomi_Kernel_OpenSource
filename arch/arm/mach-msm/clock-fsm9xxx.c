/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/clk.h>

#include <mach/clk.h>

#include "clock.h"

/*
 * Clocks
 */

struct clk_lookup msm_clocks_fsm9xxx[] = {
	CLK_DUMMY("adm_clk",	ADM0_CLK,	NULL, OFF),
	CLK_DUMMY("uart_clk",	UART1_CLK,	"msm_serial.0", OFF),
	CLK_DUMMY("ce_clk",	CE_CLK,		NULL, OFF),
};

unsigned msm_num_clocks_fsm9xxx = ARRAY_SIZE(msm_clocks_fsm9xxx);

