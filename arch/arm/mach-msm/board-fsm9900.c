/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/clk-provider.h>
#include "board-dt.h"
#include "clock.h"
#include "devices.h"
#include "platsmp.h"

static struct of_dev_auxdata fsm9900_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	{}
};

void __init fsm9900_reserve(void)
{
}

static void __init fsm9900_early_memory(void)
{
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init fsm9900_add_drivers(void)
{
	msm_smd_init();
	if (of_board_is_rumi())
		msm_clock_init(&fsm9900_dummy_clock_init_data);
	else
		msm_clock_init(&fsm9900_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
}

static void __init fsm9900_map_io(void)
{
	msm_map_fsm9900_io();
}

void __init fsm9900_init(void)
{
	struct of_dev_auxdata *adata = fsm9900_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	fsm9900_init_gpiomux();
	board_dt_populate(adata);
	fsm9900_add_drivers();
}

void __init fsm9900_init_very_early(void)
{
	fsm9900_early_memory();
}

static const char *fsm9900_dt_match[] __initconst = {
	"qcom,fsm9900",
	NULL
};

DT_MACHINE_START(FSM9900_DT, "Qualcomm FSM 9900 (Flattened Device Tree)")
	.map_io = fsm9900_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = fsm9900_init,
	.dt_compat = fsm9900_dt_match,
	.reserve = fsm9900_reserve,
	.init_very_early = fsm9900_init_very_early,
	.restart = msm_restart,
	.smp = &msm8974_smp_ops,
MACHINE_END
