// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017,2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *sdm450_dt_match[] __initconst = {
	"qcom,sdm450",
	NULL
};

static void __init sdm450_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDM450_DT,
	"Qualcomm Technologies, Inc. SDM450 (Flattened Device Tree)")
	.init_machine		= sdm450_init,
	.dt_compat		= sdm450_dt_match,
MACHINE_END
