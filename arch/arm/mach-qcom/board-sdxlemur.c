// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *sdxlemur_dt_match[] __initconst = {
	"qcom,sdxlemur",
	NULL
};

static void __init sdxlemur_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDXLEMUR_DT,
	"Qualcomm Technologies, Inc. SDXLEMUR (Flattened Device Tree)")
	.init_machine		= sdxlemur_init,
	.dt_compat		= sdxlemur_dt_match,
MACHINE_END
