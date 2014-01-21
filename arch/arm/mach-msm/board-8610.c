/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/msm_tsens.h>
#include <linux/clk/msm-clk-provider.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/restart.h>
#include <linux/regulator/qpnp-regulator.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <mach/msm_memtypes.h>
#include <soc/qcom/socinfo.h>
#include <mach/board.h>
#include <mach/msm_smd.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include <linux/msm_thermal.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

static struct of_dev_auxdata msm8610_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	{}
};

static void __init msm8610_early_memory(void)
{
	of_scan_flat_dt(dt_scan_for_memory_hole, NULL);
}

static void __init msm8610_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
}

void __init msm8610_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	rpm_smd_regulator_driver_init();
	qpnp_regulator_init();
	tsens_tm_init_driver();
	msm_thermal_device_init();

	if (of_board_is_rumi())
		msm_clock_init(&msm8610_rumi_clock_init_data);
	else
		msm_clock_init(&msm8610_clock_init_data);
}

void __init msm8610_init(void)
{
	struct of_dev_auxdata *adata = msm8610_auxdata_lookup;

	/*
	 * populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init.  socinfo_init needs smem support so call
	 * msm_smem_init before it.  msm8610_init_gpiomux needs socinfo so
	 * call socinfo_init before it.
	 */
	board_dt_populate(adata);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm8610_init_gpiomux();
	msm8610_add_drivers();
}

static const char *msm8610_dt_match[] __initconst = {
	"qcom,msm8610",
	NULL
};

DT_MACHINE_START(MSM8610_DT, "Qualcomm MSM 8610 (Flattened Device Tree)")
	.map_io			= msm_map_msm8610_io,
	.init_machine		= msm8610_init,
	.dt_compat		= msm8610_dt_match,
	.restart		= msm_restart,
	.reserve		= msm8610_reserve,
	.init_very_early	= msm8610_early_memory,
	.smp			= &arm_smp_ops,
MACHINE_END
