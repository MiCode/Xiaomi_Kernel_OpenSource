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
#include <linux/regulator/krait-regulator.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <linux/clk/msm-clk-provider.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

static struct of_dev_auxdata apq8084_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, "msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, "msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, "msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, "msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qca,qca1530", 0x00000000, "qca1530.1", NULL),
	OF_DEV_AUXDATA("qcom,ufshc", 0xFC594000, "msm_ufs.1", NULL),
	OF_DEV_AUXDATA("qcom,xhci-msm-hsic", 0xf9c00000, "msm_hsic_host", NULL),
	OF_DEV_AUXDATA("qcom,msm_pcie", 0xFC520000, "msm_pcie.1", NULL),
	OF_DEV_AUXDATA("qcom,msm_pcie", 0xFC528000, "msm_pcie.2", NULL),
	{}
};

void __init apq8084_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
}

static void __init apq8084_early_memory(void)
{
	of_scan_flat_dt(dt_scan_for_memory_hole, NULL);
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init apq8084_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	msm_pm_sleep_status_init();
	rpm_smd_regulator_driver_init();
	msm_spm_device_init();
	krait_power_init();
	if (of_board_is_rumi())
		msm_clock_init(&apq8084_rumi_clock_init_data);
	else
		msm_clock_init(&apq8084_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
}

static void __init apq8084_map_io(void)
{
	msm_map_8084_io();
}

void __init apq8084_init(void)
{
	struct of_dev_auxdata *adata = apq8084_auxdata_lookup;

	/*
	 * populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init.  socinfo_init needs smem support so call
	 * msm_smem_init before it.  apq8084_init_gpiomux needs socinfo so
	 * call socinfo_init before it.
	 */
	board_dt_populate(adata);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	apq8084_init_gpiomux();
	apq8084_add_drivers();
}

void __init apq8084_init_very_early(void)
{
	apq8084_early_memory();
}

static const char *apq8084_dt_match[] __initconst = {
	"qcom,apq8084",
	NULL
};

DT_MACHINE_START(APQ8084_DT, "Qualcomm APQ 8084 (Flattened Device Tree)")
	.map_io			= apq8084_map_io,
	.init_machine		= apq8084_init,
	.dt_compat		= apq8084_dt_match,
	.reserve		= apq8084_reserve,
	.init_very_early	= apq8084_init_very_early,
	.restart		= msm_restart,
	.smp			= &msm8974_smp_ops,
MACHINE_END
