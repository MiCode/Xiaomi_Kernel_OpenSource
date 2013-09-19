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
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/clk-provider.h>
#include <mach/msm_smem.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include "spm.h"
#include "board-dt.h"
#include "clock.h"
#include "devices.h"
#include "platsmp.h"
#include "modem_notifier.h"
#include "pm.h"

static struct of_dev_auxdata msmsamarium_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900,
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900,
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, \
			"msm_otg", NULL),
	OF_DEV_AUXDATA("qcom,spi-qup-v2", 0xF9923000, \
			"spi_qsd.1", NULL),
	{},
};

static struct memtype_reserve msmsamarium_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msmsamarium_paddr_to_memtype(phys_addr_t paddr)
{
	return MEMTYPE_EBI1;
}

static struct reserve_info msmsamarium_reserve_info __initdata = {
	.memtype_reserve_table = msmsamarium_reserve_table,
	.paddr_to_memtype = msmsamarium_paddr_to_memtype,
};

void __init msmsamarium_reserve(void)
{
	reserve_info = &msmsamarium_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msmsamarium_reserve_table);
	msm_reserve();
}

static void __init msmsamarium_early_memory(void)
{
	reserve_info = &msmsamarium_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msmsamarium_reserve_table);
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msmsamarium_add_drivers(void)
{
	msm_smem_init();
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_pm_sleep_status_init();
	rpm_regulator_smd_driver_init();
	msm_spm_device_init();
	if (of_board_is_rumi())
		msm_clock_init(&msmsamarium_rumi_clock_init_data);
	else
		msm_clock_init(&msmsamarium_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
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

void __init msmsamarium_init_very_early(void)
{
	msmsamarium_early_memory();
}

static const char *msmsamarium_dt_match[] __initconst = {
	"qcom,msmsamarium",
	NULL
};

DT_MACHINE_START(MSMSAMARIUM_DT, "Qualcomm MSM Samarium(Flattened Device Tree)")
	.map_io = msmsamarium_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msmsamarium_init,
	.dt_compat = msmsamarium_dt_match,
	.reserve = msmsamarium_reserve,
	.init_very_early = msmsamarium_init_very_early,
	.restart = msm_restart,
	.smp = &msm8974_smp_ops,
MACHINE_END
