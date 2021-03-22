// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017,2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *msm8953_dt_match[] __initconst = {
	"qcom,msm8953",
	NULL
};

static void __init msm8953_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MSM8953_DT,
	"Qualcomm Technologies, Inc. MSM8953 (Flattened Device Tree)")
	.init_machine		= msm8953_init,
	.dt_compat		= msm8953_dt_match,
MACHINE_END
