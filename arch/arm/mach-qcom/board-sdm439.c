// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *sdm439_dt_match[] __initconst = {
	"qcom,sdm439",
	NULL
};

static void __init sdm439_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDM439_DT,
	"Qualcomm Technologies, Inc. SDM439 (Flattened Device Tree)")
	.init_machine		= sdm439_init,
	.dt_compat		= sdm439_dt_match,
MACHINE_END
