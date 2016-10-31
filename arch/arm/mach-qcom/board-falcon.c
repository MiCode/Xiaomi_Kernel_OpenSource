/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

static const char *msmfalcon_dt_match[] __initconst = {
	"qcom,msmfalcon",
	"qcom,apqfalcon",
	NULL
};

static void __init msmfalcon_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MSMFALCON_DT,
	"Qualcomm Technologies, Inc. MSM FALCON (Flattened Device Tree)")
	.init_machine = msmfalcon_init,
	.dt_compat = msmfalcon_dt_match,
MACHINE_END

static const char *msmtriton_dt_match[] __initconst = {
	"qcom,msmtriton",
	"qcom,apqtriton",
	NULL
};

static void __init msmtriton_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MSMTRITON_DT,
	"Qualcomm Technologies, Inc. MSM TRITON (Flattened Device Tree)")
	.init_machine = msmtriton_init,
	.dt_compat = msmtriton_dt_match,
MACHINE_END
