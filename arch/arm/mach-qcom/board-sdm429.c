// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *sdm429_dt_match[] __initconst = {
	"qcom,sdm429",
	NULL
};

static void __init sdm429_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDM429_DT,
	"Qualcomm Technologies, Inc. SDM429 (Flattened Device Tree)")
	.init_machine		= sdm429_init,
	.dt_compat		= sdm429_dt_match,
MACHINE_END
