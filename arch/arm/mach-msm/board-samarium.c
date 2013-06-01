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
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/clk-provider.h>
#include "board-dt.h"
#include "clock.h"
#include "devices.h"
#include "platsmp.h"

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("core_clk",	SDC1_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("iface_clk",	SDC1_P_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("core_clk",	SDC2_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("iface_clk",	SDC2_P_CLK,	"msm_sdcc.2", OFF),
};

static struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

static struct of_dev_auxdata msmsamarium_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	{},
};

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msmsamarium_add_drivers(void)
{
	msm_clock_init(&msm_dummy_clock_init_data);
}

static void __init msmsamarium_map_io(void)
{
	msm_map_msmsamarium_io();
}

void __init msmsamarium_init(void)
{
	struct of_dev_auxdata *adata = msmsamarium_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msmsamarium_init_gpiomux();
	board_dt_populate(adata);
	msmsamarium_add_drivers();
}

static const char *msmsamarium_dt_match[] __initconst = {
	"qcom,msmsamarium",
	NULL
};

DT_MACHINE_START(MSMSAMARIUM_DT, "Qualcomm MSM Samarium(Flattened Device Tree)")
	.map_io = msmsamarium_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msmsamarium_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msmsamarium_dt_match,
	.restart = msm_restart,
	.smp = &msm8974_smp_ops,
MACHINE_END
