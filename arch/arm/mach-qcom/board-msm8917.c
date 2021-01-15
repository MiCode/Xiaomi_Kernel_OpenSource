// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *msm8917_dt_match[] __initconst = {
	"qcom,msm8917",
	"qcom,apq8017",
	NULL
};

static void __init msm8917_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MSM8917_DT,
	"Qualcomm Technologies, Inc. MSM8917-PMI8950 MTP")
	.init_machine		= msm8917_init,
	.dt_compat		= msm8917_dt_match,
MACHINE_END
