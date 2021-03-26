// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *qm215_dt_match[] __initconst = {
	"qcom,qm215",
	NULL
};

static void __init qm215_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(QM215_DT,
	"Qualcomm Technologies, Inc. QM215")
	.init_machine		= qm215_init,
	.dt_compat		= qm215_dt_match,
MACHINE_END
