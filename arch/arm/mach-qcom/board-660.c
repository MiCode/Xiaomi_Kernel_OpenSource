/*
 * Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved.
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

static const char *sdm660_dt_match[] __initconst = {
	"qcom,sdm660",
	"qcom,sda660",
	NULL
};

static void __init sdm660_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDM660_DT,
	"Qualcomm Technologies, Inc. SDM 660 (Flattened Device Tree)")
	.init_machine = sdm660_init,
	.dt_compat = sdm660_dt_match,
MACHINE_END

static const char *sdm630_dt_match[] __initconst = {
	"qcom,sdm630",
	"qcom,sda630",
	NULL
};

static void __init sdm630_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDM630_DT,
	"Qualcomm Technologies, Inc. SDM 630 (Flattened Device Tree)")
	.init_machine = sdm630_init,
	.dt_compat = sdm630_dt_match,
MACHINE_END

static const char *sdm658_dt_match[] __initconst = {
	"qcom,sdm658",
	"qcom,sda658",
	NULL
};

static void __init sdm658_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDM658_DT,
	"Qualcomm Technologies, Inc. SDM 658 (Flattened Device Tree)")
	.init_machine = sdm658_init,
	.dt_compat = sdm658_dt_match,
MACHINE_END
