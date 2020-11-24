// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *monaco_dt_match[] __initconst = {
	"qcom,monaco",
	NULL
};

static void __init monaco_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MONACO_DT,
	"Qualcomm Technologies, Inc. MONACO (Flattened Device Tree)")
	.init_machine		= monaco_init,
	.dt_compat		= monaco_dt_match,
MACHINE_END
