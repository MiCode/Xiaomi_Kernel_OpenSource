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
#include <linux/clk/msm-clk-provider.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/restart.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/smd.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

static struct of_dev_auxdata msmsamarium_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900,
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900,
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, "msm_otg", NULL),
	OF_DEV_AUXDATA("qcom,spi-qup-v2", 0xF9923000, \
			"spi_qsd.1", NULL),
	{},
};

void __init msmsamarium_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
}

static void __init msmsamarium_early_memory(void)
{
	of_scan_flat_dt(dt_scan_for_memory_hole, NULL);
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msmsamarium_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	msm_pm_sleep_status_init();
	rpm_smd_regulator_driver_init();
	msm_spm_device_init();
	if (of_board_is_rumi())
		msm_clock_init(&msmsamarium_rumi_clock_init_data);
	else
		msm_clock_init(&msmsamarium_clock_init_data);
}

static void __init msmsamarium_map_io(void)
{
	msm_map_msmsamarium_io();
}

void __init msmsamarium_init(void)
{
	struct of_dev_auxdata *adata = msmsamarium_auxdata_lookup;

	/*
	 * populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init.  socinfo_init needs smem support so call
	 * msm_smem_init before it.
	 */
	board_dt_populate(adata);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msmsamarium_init_gpiomux();
	msmsamarium_add_drivers();
}

void __init msmsamarium_init_very_early(void)
{
	msmsamarium_early_memory();
}

static const char *msmsamarium_dt_match[] __initconst = {
	"qcom,msmsamarium",
	"qcom,apqsamarium",
	NULL
};

DT_MACHINE_START(MSMSAMARIUM_DT,
	"Qualcomm Technologies, Inc. MSM Samarium(Flattened Device Tree)")
	.map_io			= msmsamarium_map_io,
	.init_machine		= msmsamarium_init,
	.dt_compat		= msmsamarium_dt_match,
	.reserve		= msmsamarium_reserve,
	.init_very_early	= msmsamarium_init_very_early,
	.restart		= msm_restart,
	.smp			= &msm8962_smp_ops,
MACHINE_END
