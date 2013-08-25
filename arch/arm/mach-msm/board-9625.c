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
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/restart.h>
#include <mach/gpio.h>
#include <mach/clk-provider.h>
#include <mach/qpnp-int.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include "board-dt.h"
#include <mach/msm_bus_board.h>
#include <mach/msm_smem.h>
#include "clock.h"
#include "modem_notifier.h"
#include "spm.h"

#define MSM_KERNEL_EBI_SIZE	0x51000

static struct memtype_reserve msm9625_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm9625_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static void __init msm9625_calculate_reserve_sizes(void)
{
	msm9625_reserve_table[MEMTYPE_EBI1].size += MSM_KERNEL_EBI_SIZE;
}

static struct reserve_info msm9625_reserve_info __initdata = {
	.memtype_reserve_table = msm9625_reserve_table,
	.calculate_reserve_sizes = msm9625_calculate_reserve_sizes,
	.paddr_to_memtype = msm9625_paddr_to_memtype,
};

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("phy_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("core_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("iface_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("xo", NULL, "msm_otg", OFF),
	CLK_DUMMY("dfab_clk",	DFAB_CLK,	NULL, 0),
	CLK_DUMMY("dma_bam_pclk",	DMA_BAM_P_CLK,	NULL, 0),
	CLK_DUMMY("mem_clk",	NULL,	NULL, 0),
	CLK_DUMMY("core_clk",	SPI_CLK,	"spi_qsd.1",	OFF),
	CLK_DUMMY("iface_clk",	SPI_P_CLK,	"spi_qsd.1",	OFF),
	CLK_DUMMY("core_clk",	NULL,	"f9966000.i2c", 0),
	CLK_DUMMY("iface_clk",	NULL,	"f9966000.i2c", 0),
	CLK_DUMMY("core_clk",	NULL,	"fe12f000.slim",	OFF),
};

struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

static const char *msm9625_dt_match[] __initconst = {
	"qcom,msm9625",
	NULL
};

static struct of_dev_auxdata msm9625_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-lsuart-v14", 0xF991F000, \
			"msm_serial_hsl.0", NULL),
	OF_DEV_AUXDATA("qcom,spmi-pmic-arb", 0xFC4C0000, \
			"spmi-pmic-arb.0", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,msm-tsens", 0xFC4A8000, \
			"msm-tsens", NULL),
	OF_DEV_AUXDATA("qcom,usb-bam-msm", 0xF9A44000, \
			"usb_bam", NULL),
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A15000, \
			"msm_hsic_host", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
		       "msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
		       "msm_sdcc.3", NULL),
	{}
};

static void __init msm9625_early_memory(void)
{
	reserve_info = &msm9625_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm9625_reserve_table);
}

static void __init msm9625_reserve(void)
{
	reserve_info = &msm9625_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm9625_reserve_table);
	msm_reserve();
}

#define BIMC_BASE	0xfc380000
#define BIMC_SIZE	0x0006A000
#define SYS_NOC_BASE	0xfc460000
#define PERIPH_NOC_BASE 0xFC468000
#define CONFIG_NOC_BASE	0xfc480000
#define NOC_SIZE	0x00004000

static struct resource bimc_res[] = {
	{
		.start = BIMC_BASE,
		.end = BIMC_BASE + BIMC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "bimc_mem",
	},
};

static struct resource sys_noc_res[] = {
	{
		.start = SYS_NOC_BASE,
		.end = SYS_NOC_BASE + NOC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "sys_noc_mem",
	},
};

static struct resource config_noc_res[] = {
	{
		.start = CONFIG_NOC_BASE,
		.end = CONFIG_NOC_BASE + NOC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "config_noc_mem",
	},
};

static struct resource periph_noc_res[] = {
	{
		.start = PERIPH_NOC_BASE,
		.end = PERIPH_NOC_BASE + NOC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "periph_noc_mem",
	},
};

static struct platform_device msm_bus_sys_noc = {
	.name  = "msm_bus_fabric",
	.id    =  MSM_BUS_FAB_SYS_NOC,
	.num_resources = ARRAY_SIZE(sys_noc_res),
	.resource = sys_noc_res,
};

static struct platform_device msm_bus_bimc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_BIMC,
	.num_resources = ARRAY_SIZE(bimc_res),
	.resource = bimc_res,
};

static struct platform_device msm_bus_periph_noc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_PERIPH_NOC,
	.num_resources = ARRAY_SIZE(periph_noc_res),
	.resource = periph_noc_res,
};

static struct platform_device msm_bus_config_noc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_CONFIG_NOC,
	.num_resources = ARRAY_SIZE(config_noc_res),
	.resource = config_noc_res,
};

static struct platform_device *msm_bus_9625_devices[] = {
	&msm_bus_sys_noc,
	&msm_bus_bimc,
	&msm_bus_periph_noc,
	&msm_bus_config_noc,
};

static void __init msm9625_init_buses(void)
{
#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_sys_noc.dev.platform_data =
		&msm_bus_9625_sys_noc_pdata;
	msm_bus_bimc.dev.platform_data = &msm_bus_9625_bimc_pdata;
	msm_bus_periph_noc.dev.platform_data = &msm_bus_9625_periph_noc_pdata;
	msm_bus_config_noc.dev.platform_data = &msm_bus_9625_config_noc_pdata;
#endif
	platform_add_devices(msm_bus_9625_devices,
				ARRAY_SIZE(msm_bus_9625_devices));
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here.
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm9625_add_drivers(void)
{
	msm_smem_init();
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	rpm_regulator_smd_driver_init();
	msm_spm_device_init();
	msm_clock_init(&msm9625_clock_init_data);
	msm9625_init_buses();
	tsens_tm_init_driver();
	msm_thermal_device_init();
}

void __init msm9625_init(void)
{
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm9625_init_gpiomux();
	board_dt_populate(msm9625_auxdata_lookup);
	msm9625_add_drivers();
}

DT_MACHINE_START(MSM9625_DT, "Qualcomm MSM 9625 (Flattened Device Tree)")
	.map_io = msm_map_msm9625_io,
	.init_irq = msm_dt_init_irq_l2x0,
	.init_machine = msm9625_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm9625_dt_match,
	.reserve = msm9625_reserve,
	.init_very_early = msm9625_early_memory,
	.restart = msm_restart,
MACHINE_END
