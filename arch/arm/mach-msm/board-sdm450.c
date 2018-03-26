/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <asm/mach/arch.h>
#include "board-dt.h"

static const char *sdm450_dt_match[] __initconst = {
	"qcom,sdm450",
	"qcom,sda450",
	NULL
};

static void __init sdm450_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDM450_DT,
	"Qualcomm Technologies, Inc. SDM450 (Flattened Device Tree)")
	.init_machine = sdm450_init,
	.dt_compat = sdm450_dt_match,
MACHINE_END
