/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *sdm632_dt_match[] __initconst = {
	"qcom,sdm632",
	NULL
};

static void __init sdm632_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDM632_DT,
	"Qualcomm Technologies, Inc. SDM632 (Flattened Device Tree)")
	.init_machine		= sdm632_init,
	.dt_compat		= sdm632_dt_match,
MACHINE_END
