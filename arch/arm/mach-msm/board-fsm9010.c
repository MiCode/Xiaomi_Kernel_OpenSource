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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <soc/qcom/restart.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/smd.h>
#include "board-dt.h"
#include "platsmp.h"


void __init fsm9010_reserve(void)
{
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init fsm9010_add_drivers(void)
{
	msm_smd_init();
}

static void __init fsm9010_map_io(void)
{
	msm_map_fsm9010_io();
}

void __init fsm9010_init(void)
{
	/*
	 * Populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init. socinfo_init needs smem support so call
	 * msm_smem_init before it.
	 */
	board_dt_populate(NULL);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	fsm9010_add_drivers();
}

static const char *fsm9010_dt_match[] __initconst = {
	"qcom,fsm9010",
	NULL
};

DT_MACHINE_START(FSM9010_DT,
		"Qualcomm Technologies, Inc. FSM 9010 (Flattened Device Tree)")
	.map_io			= fsm9010_map_io,
	.init_machine		= fsm9010_init,
	.dt_compat		= fsm9010_dt_match,
	.reserve		= fsm9010_reserve,
	.smp			= &arm_smp_ops,
MACHINE_END
