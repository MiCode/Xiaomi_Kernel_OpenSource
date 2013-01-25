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
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <asm/mach/map.h>
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
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <linux/msm_thermal.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"
#include "spm.h"
#include "lpm_resources.h"

static struct memtype_reserve msm8226_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm8226_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}
static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   NULL,           "f9926000.i2c", OFF),
	CLK_DUMMY("iface_clk",  NULL,           "f9926000.i2c", OFF),
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  HSUSB_IFACE_CLK, "f9a55000.usb", OFF),
	CLK_DUMMY("core_clk",	HSUSB_CORE_CLK, "f9a55000.usb", OFF),
	CLK_DUMMY("dfab_clk",	DFAB_CLK, "msm_sps", OFF),
	CLK_DUMMY("dma_bam_pclk",	DMA_BAM_P_CLK, "msm_sps", OFF),
	CLK_DUMMY("iface_clk",	NULL,		"msm_sdcc.1", OFF),
	CLK_DUMMY("core_clk",	NULL,		"msm_sdcc.1", OFF),
	CLK_DUMMY("bus_clk",	NULL,		"msm_sdcc.1", OFF),
	CLK_DUMMY("iface_clk",	NULL,		"msm_sdcc.2", OFF),
	CLK_DUMMY("core_clk",	NULL,		"msm_sdcc.2", OFF),
	CLK_DUMMY("bus_clk",	NULL,		"msm_sdcc.2", OFF),
	CLK_DUMMY("core_clk",	NULL,		"f9928000.spi", OFF),
	CLK_DUMMY("iface_clk",	NULL,		"f9928000.spi", OFF),
	CLK_DUMMY("iface_clk",		NULL, "fda64000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk",		NULL, "fda64000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk",		NULL, "fda44000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk",		NULL, "fda44000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk",		NULL, "fd928000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk",		NULL, "fd928000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk",		NULL, "fdb10000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk",		NULL, "fdb10000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk",	NULL, "fdb10000.qcom,iommu", OFF),
	CLK_DUMMY("iface_clk",		NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("alt_core_clk",	NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk",		NULL, "fdc84000.qcom,iommu", OFF),
	CLK_DUMMY("core_clk",		NULL, "fdd00000.qcom,ocmem", OFF),
	CLK_DUMMY("iface_clk",		NULL, "fdd00000.qcom,ocmem", OFF),
	CLK_DUMMY("br_clk",		NULL, "fdd00000.qcom,ocmem", OFF),
};

static struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

static struct of_dev_auxdata msm8226_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	{}
};

static struct reserve_info msm8226_reserve_info __initdata = {
	.memtype_reserve_table = msm8226_reserve_table,
	.paddr_to_memtype = msm8226_paddr_to_memtype,
};

static void __init msm8226_early_memory(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8226_reserve_table);
}

static void __init msm8226_reserve(void)
{
	msm_reserve();
}


void __init msm8226_add_drivers(void)
{
	msm_rpm_driver_init();
	msm_lpmrs_module_init();
	msm_spm_device_init();
	msm_thermal_device_init();
}

void __init msm8226_init(void)
{
	struct of_dev_auxdata *adata = msm8226_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm8226_init_gpiomux();
	msm8226_add_drivers();
	msm_clock_init(&msm_dummy_clock_init_data);
	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);

}

static const char *msm8226_dt_match[] __initconst = {
	"qcom,msm8226",
	NULL
};

DT_MACHINE_START(MSM8226_DT, "Qualcomm MSM 8226 (Flattened Device Tree)")
	.map_io = msm_map_msm8226_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8226_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8226_dt_match,
	.reserve = msm8226_reserve,
	.init_very_early = msm8226_early_memory,
	.restart = msm_restart,
	.smp = &arm_smp_ops,
MACHINE_END
