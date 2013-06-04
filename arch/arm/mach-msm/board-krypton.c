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
#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/clk-provider.h>
#include "board-dt.h"
#include "clock.h"
#include "devices.h"

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
};

static struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

static struct of_dev_auxdata msmkrypton_auxdata_lookup[] __initdata = {
	{}
};

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msmkrypton_add_drivers(void)
{
	msm_smd_init();
	msm_clock_init(&msm_dummy_clock_init_data);
}

static void __init msmkrypton_map_io(void)
{
	msm_map_msmkrypton_io();
}

void __init msmkrypton_init(void)
{
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msmkrypton_init_gpiomux();
	board_dt_populate(msmkrypton_auxdata_lookup);
	msmkrypton_add_drivers();
}

static const char *msmkrypton_dt_match[] __initconst = {
	"qcom,msmkrypton",
	NULL
};

DT_MACHINE_START(MSMKRYPTON_DT, "Qualcomm MSM Krypton (Flattened Device Tree)")
	.map_io = msmkrypton_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msmkrypton_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msmkrypton_dt_match,
	.restart = msm_restart,
MACHINE_END
