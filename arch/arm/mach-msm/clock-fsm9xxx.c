/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

static struct clk_lookup msm_clocks_fsm9xxx[] = {
	CLK_DUMMY("core_clk",	ADM0_CLK,	"msm_dmov", OFF),
	CLK_DUMMY("core_clk",	UART1_CLK,	"msm_serial.0", OFF),
	CLK_DUMMY("core_clk",	CE_CLK,		"qce.0", OFF),
	CLK_DUMMY("core_clk",	CE_CLK,		"qcota.0", OFF),
	CLK_DUMMY("core_clk",	CE_CLK,		"qcrypto.0", OFF),
};

struct clock_init_data fsm9xxx_clock_init_data __initdata = {
	.table = msm_clocks_fsm9xxx,
	.size = ARRAY_SIZE(msm_clocks_fsm9xxx),
};
