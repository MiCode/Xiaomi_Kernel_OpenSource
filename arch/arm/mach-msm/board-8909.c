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
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include "board-dt.h"
#include "platsmp.h"

static const char *msm8909_dt_match[] __initconst = {
	"qcom,msm8909",
	"qcom,apq8009",
	NULL
};

static void __init msm8909_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MSM8909_DT,
	"Qualcomm Technologies, Inc. MSM 8909 (Flattened Device Tree)")
	.init_machine	= msm8909_init,
	.dt_compat	= msm8909_dt_match,
	.smp	= &msm8909_smp_ops,
MACHINE_END
