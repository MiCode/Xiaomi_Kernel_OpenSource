/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include "board-dt.h"

static void __init msmvpipa_map_io(void)
{
	msm_map_msmvpipa_io();
}

static const char *msmvpipa_dt_match[] __initconst = {
	"qcom,msmvpipa",
	NULL
};

static void __init msmvpipa_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MSMVPIPA_DT,
		"Qualcomm Technologies, Inc. MSM VPIPA (Flattened Device Tree)")
	.init_machine		= msmvpipa_init,
	.dt_compat		= msmvpipa_dt_match,
	.map_io			= msmvpipa_map_io,
MACHINE_END
