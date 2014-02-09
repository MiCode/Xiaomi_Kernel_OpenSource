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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <soc/qcom/rpm-smd.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include "board-dt.h"
#include "clock.h"

static struct of_dev_auxdata mdm9630_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, "msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm_pcie", 0xFC520000, "msm_pcie", NULL),
	{}
};

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init mdm9630_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	rpm_smd_regulator_driver_init();
	msm_spm_device_init();
	msm_clock_init(&mdm9630_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
}

void __init mdm9630_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
}
static void __init mdm9630_early_memory(void)
{
	of_scan_flat_dt(dt_scan_for_memory_hole, NULL);
}
static void __init mdm9630_map_io(void)
{
	msm_map_mdm9630_io();
}

void __init mdm9630_init(void)
{
	/*
	 * populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init.  socinfo_init needs smem support so call
	 * msm_smem_init before it.
	 */
	board_dt_populate(mdm9630_auxdata_lookup);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	mdm9630_init_gpiomux();
	mdm9630_add_drivers();
}

static const char *mdm9630_dt_match[] __initconst = {
	"qcom,mdm9630",
	NULL
};

DT_MACHINE_START(MDM9630_DT, "Qualcomm MDM 9630 (Flattened Device Tree)")
	.map_io			= mdm9630_map_io,
	.init_machine		= mdm9630_init,
	.dt_compat		= mdm9630_dt_match,
	.reserve		= mdm9630_reserve,
	.init_very_early	= mdm9630_early_memory,
	.restart		= msm_restart,
MACHINE_END
