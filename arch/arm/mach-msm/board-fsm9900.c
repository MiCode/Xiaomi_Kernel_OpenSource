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
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/clk-provider.h>
#include "board-dt.h"
#include "clock.h"
#include "devices.h"
#include "platsmp.h"

void __init fsm9900_reserve(void)
{
}

static void __init fsm9900_early_memory(void)
{
}

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP2_UART_CLK, "f9960000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP2_UART_CLK, "f9960000.serial", OFF),
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("core_clk",   BLSP2_I2C_CLK,  "f9966000.i2c",    OFF),
	CLK_DUMMY("iface_clk",  BLSP2_I2C_CLK,  "f9966000.i2c",    OFF),
	CLK_DUMMY("core_clk",   BLSP1_I2C_CLK,  "f9924000.i2c",    OFF),
	CLK_DUMMY("iface_clk",  BLSP1_I2C_CLK,  "f9924000.i2c",    OFF),
	CLK_DUMMY("core_clk",   NULL,           "f9a55000.usb",    OFF),
	CLK_DUMMY("iface_clk",  NULL,           "f9a55000.usb",    OFF),
	CLK_DUMMY("phy_clk",    NULL,           "f9a55000.usb",    OFF),
	CLK_DUMMY("xo",         NULL,           "f9a55000.usb",    OFF),
	CLK_DUMMY("core_clk",   NULL,           "msm_ehci_host",   OFF),
	CLK_DUMMY("iface_clk",  NULL,           "msm_ehci_host",   OFF),
	CLK_DUMMY("sleep_clk",  NULL,           "msm_ehci_host",   OFF),
	CLK_DUMMY("xo",         NULL,           "msm_ehci_host",   OFF),
	CLK_DUMMY("core_clk",   NULL,           "f9824900.sdhci_msm", OFF),
	CLK_DUMMY("iface_clk",  NULL,           "f9824900.sdhci_msm", OFF),
	CLK_DUMMY("core_clk",   NULL,           "f98a4900.sdhci_msm", OFF),
	CLK_DUMMY("iface_clk",  NULL,           "f98a4900.sdhci_msm", OFF),
};

static struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init fsm9900_add_drivers(void)
{
	msm_smd_init();
	msm_clock_init(&msm_dummy_clock_init_data);
}

static void __init fsm9900_map_io(void)
{
	msm_map_fsm9900_io();
}

void __init fsm9900_init(void)
{
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	fsm9900_init_gpiomux();
	board_dt_populate(NULL);
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
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = fsm9900_dt_match,
	.reserve = fsm9900_reserve,
	.init_very_early = fsm9900_init_very_early,
	.restart = msm_restart,
	.smp = &msm8974_smp_ops,
MACHINE_END
