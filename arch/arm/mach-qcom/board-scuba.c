// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include "board-dt.h"

static const char *scuba_dt_match[] __initconst = {
	"qcom,scuba",
	"qcom,scuba-iot",
	"qcom,scubap-iot",
	NULL
};

static void __init scuba_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(BENGAL,
	"Qualcomm Technologies, Inc. SCUBA (Flattened Device Tree)")
	.init_machine		= scuba_init,
	.dt_compat		= scuba_dt_match,
MACHINE_END
