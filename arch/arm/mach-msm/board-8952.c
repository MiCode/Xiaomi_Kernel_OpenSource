/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

static const char *msm8952_dt_match[] __initconst = {
	"qcom,msm8952",
	"qcom,apq8052",
	NULL
};

static void __init msm8952_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MSM8952_DT,
	"Qualcomm Technologies, Inc. MSM 8952 (Flattened Device Tree)")
	.init_machine = msm8952_init,
	.dt_compat = msm8952_dt_match,
MACHINE_END
