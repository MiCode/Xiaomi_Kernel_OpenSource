// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *sdxnightjar_dt_match[] __initconst = {
	"qcom,sdxnightjar",
	NULL
};

static void __init sdxnightjar_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(SDXNIGHTJAR_DT,
	"Qualcomm Technologies, Inc. SDXNIGHTJAR (Flattened Device Tree)")
	.init_machine		= sdxnightjar_init,
	.dt_compat		= sdxnightjar_dt_match,
MACHINE_END
