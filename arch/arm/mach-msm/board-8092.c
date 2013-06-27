/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/msm_memtypes.h>
#include <mach/qpnp-int.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <mach/clk-provider.h>

#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

static struct memtype_reserve mpq8092_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags  =       MEMTYPE_FLAGS_1M_ALIGN,
		},
	[MEMTYPE_EBI1] = {
		.flags  =       MEMTYPE_FLAGS_1M_ALIGN,
		},
};

static int mpq8092_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct reserve_info mpq8092_reserve_info __initdata = {
	.memtype_reserve_table = mpq8092_reserve_table,
	.paddr_to_memtype = mpq8092_paddr_to_memtype,
};

static void __init mpq8092_early_memory(void)
{
	reserve_info = &mpq8092_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, mpq8092_reserve_table);
}

static void __init mpq8092_dt_reserve(void)
{
	msm_reserve();
}

static void __init mpq8092_map_io(void)
{
	msm_map_mpq8092_io();
}

static struct of_dev_auxdata mpq8092_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, \
			"msm_otg", NULL),
	OF_DEV_AUXDATA("qcom,msm-lsuart-v14", 0xF991F000, \
			"msm_serial_hsl.0", NULL),
	OF_DEV_AUXDATA("qcom,msm-lsuart-v14", 0xF9922000, \
			"msm_serial_hsl.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	{}
};

static void __init mpq8092_init(void)
{
	struct of_dev_auxdata *adata = mpq8092_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	mpq8092_init_gpiomux();
	msm_clock_init(&mpq8092_clock_init_data);
	board_dt_populate(adata);
}

static const char *mpq8092_dt_match[] __initconst = {
	"qcom,mpq8092-sim",
	"qcom,mpq8092-rumi",
	"qcom,mpq8092",
	NULL
};

DT_MACHINE_START(MSM8092_DT, "Qualcomm MSM 8092 (Flattened Device Tree)")
	.map_io = mpq8092_map_io,
	.init_irq = msm_dt_init_irq_nompm,
	.init_machine = mpq8092_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = mpq8092_dt_match,
	.reserve = mpq8092_dt_reserve,
	.init_very_early = mpq8092_early_memory,
	.smp = &msm8974_smp_ops,
MACHINE_END
