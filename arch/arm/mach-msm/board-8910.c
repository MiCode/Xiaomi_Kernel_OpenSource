/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <asm/mach/map.h>
#include <asm/arch_timer.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/restart.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include "board-dt.h"
#include "clock.h"

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  HSUSB_IFACE_CLK, "f9a55000.usb", OFF),
	CLK_DUMMY("core_clk",	HSUSB_CORE_CLK, "f9a55000.usb", OFF),
	CLK_DUMMY("iface_clk",	NULL,		"msm_sdcc.1", OFF),
	CLK_DUMMY("core_clk",	NULL,		"msm_sdcc.1", OFF),
	CLK_DUMMY("bus_clk",	NULL,		"msm_sdcc.1", OFF),
	CLK_DUMMY("iface_clk",	NULL,		"msm_sdcc.2", OFF),
	CLK_DUMMY("core_clk",	NULL,		"msm_sdcc.2", OFF),
	CLK_DUMMY("bus_clk",	NULL,		"msm_sdcc.2", OFF),
};

static struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

static struct of_dev_auxdata msm8910_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	{}
};

void __init msm8910_init(void)
{
	struct of_dev_auxdata *adata = msm8910_auxdata_lookup;

	msm8910_init_gpiomux();
	msm_clock_init(&msm_dummy_clock_init_data);

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);
}

static const char *msm8910_dt_match[] __initconst = {
	"qcom,msm8910",
	NULL
};

DT_MACHINE_START(MSM8910_DT, "Qualcomm MSM 8910 (Flattened Device Tree)")
	.map_io = msm_map_msm8910_io,
	.init_irq = msm_dt_init_irq_nompm,
	.init_machine = msm8910_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8910_dt_match,
	.restart = msm_restart,
MACHINE_END
