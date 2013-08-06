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

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <asm/mach/arch.h>
#include <soc/qcom/socinfo.h>
#include <mach/board.h>
#include <mach/msm_memtypes.h>

#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

static struct memtype_reserve msm8916_reserve_table[] __initdata = {
	[MEMTYPE_EBI0] = {
		.flags  =       MEMTYPE_FLAGS_1M_ALIGN,
		},
	[MEMTYPE_EBI1] = {
		.flags  =       MEMTYPE_FLAGS_1M_ALIGN,
		},
};

static int msm8916_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct reserve_info msm8916_reserve_info __initdata = {
	.memtype_reserve_table = msm8916_reserve_table,
	.paddr_to_memtype = msm8916_paddr_to_memtype,
};

static void __init msm8916_early_memory(void)
{
	reserve_info = &msm8916_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8916_reserve_table);
}

static void __init msm8916_dt_reserve(void)
{
	reserve_info = &msm8916_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8916_reserve_table);
	msm_reserve();
}

static void __init msm8916_map_io(void)
{
	msm_map_msm8916_io();
}

static struct of_dev_auxdata msm8916_auxdata_lookup[] __initdata = {
	{}
};

static void __init msm8916_init(void)
{
	struct of_dev_auxdata *adata = msm8916_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm_clock_init(&msm8916_clock_init_data);
	board_dt_populate(adata);
}

static const char *msm8916_dt_match[] __initconst = {
	"qcom,msm8916",
	NULL
};

DT_MACHINE_START(MSM8916_DT, "Qualcomm MSM 8916 (Flattened Device Tree)")
	.map_io = msm8916_map_io,
	.init_machine = msm8916_init,
	.dt_compat = msm8916_dt_match,
	.reserve = msm8916_dt_reserve,
	.init_very_early = msm8916_early_memory,
	.smp = &msm8916_smp_ops,
MACHINE_END
