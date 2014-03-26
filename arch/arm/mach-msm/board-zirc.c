/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

static void __init msmzirc_map_io(void)
{
	msm_map_msmzirc_io();
}

static const char *msmzirc_dt_match[] __initconst = {
	"qcom,msmzirc",
	NULL
};

static void __init msmzirc_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(MSMZIRC_DT,
		 "Qualcomm Technologies, Inc. MSM ZIRC (Flattened Device Tree)")
	.init_machine		= msmzirc_init,
	.dt_compat		= msmzirc_dt_match,
	.map_io			= msmzirc_map_io,
MACHINE_END
