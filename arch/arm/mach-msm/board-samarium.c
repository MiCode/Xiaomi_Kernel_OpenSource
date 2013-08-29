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

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("xo",          cxo_pil_lpass_clk, "fe200000.qcom,lpass", OFF),
	CLK_DUMMY("core_clk",          q6ss_xo_clk, "fe200000.qcom,lpass", OFF),
	CLK_DUMMY("bus_clk",  gcc_lpass_q6_axi_clk, "fe200000.qcom,lpass", OFF),
	CLK_DUMMY("iface_clk", q6ss_ahb_lfabif_clk, "fe200000.qcom,lpass", OFF),
	CLK_DUMMY("reg_clk",         q6ss_ahbm_clk, "fe200000.qcom,lpass", OFF),

	CLK_DUMMY("core_clk",  venus_vcodec0_clk,  "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("iface_clk", venus_ahb_clk,      "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("bus_clk",   venus_axi_clk,      "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("mem_clk",   venus_ocmemnoc_clk, "fdce0000.qcom,venus", OFF),
	CLK_DUMMY("core_clk",  venus_vcodec0_clk,  "fd8c1024.qcom,gdsc",  OFF),

	CLK_DUMMY("xo",                CXO_CLK, "fc880000.qcom,mss", OFF),
	CLK_DUMMY("bus_clk",   MSS_BIMC_Q6_CLK, "fc880000.qcom,mss", OFF),
	CLK_DUMMY("iface_clk", MSS_CFG_AHB_CLK, "fc880000.qcom,mss", OFF),
	CLK_DUMMY("mem_clk",  BOOT_ROM_AHB_CLK, "fc880000.qcom,mss", OFF),
	CLK_DUMMY("xo",		XO_CLK,		"fb21b000.qcom,pronto", OFF),
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991e000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991e000.serial", OFF),
	CLK_DUMMY("core_clk",	SDC1_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("iface_clk",	SDC1_P_CLK,	"msm_sdcc.1", OFF),
	CLK_DUMMY("core_clk",	SDC2_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("iface_clk",	SDC2_P_CLK,	"msm_sdcc.2", OFF),
	CLK_DUMMY("core_clk",	USB_HS_SYSTEM_CLK, "msm_otg", OFF),
	CLK_DUMMY("iface_clk",	USB_HS_AHB_CLK,    "msm_otg", OFF),
	CLK_DUMMY("xo",         CXO_OTG_CLK,       "msm_otg", OFF),
	CLK_DUMMY("dfab_clk",	DFAB_CLK,	"msm_sps", OFF),
	CLK_DUMMY("dma_bam_pclk",	DMA_BAM_P_CLK,	"msm_sps", OFF),
	CLK_DUMMY("core_clk",   SPI_CLK,        "spi_qsd.1",  OFF),
	CLK_DUMMY("iface_clk",  SPI_P_CLK,      "spi_qsd.1",  OFF),
	CLK_DUMMY("core_clk", gcc_prng_ahb_clk.c, "f9bff000.qcom,msm-rng", OFF),
	CLK_DUMMY("core_clk",	I2C_CLK,	"f9924000.i2c", OFF),
	CLK_DUMMY("iface_clk",	I2C_P_CLK,	"f9924000.i2c", OFF),

	/* CoreSight clocks */
	CLK_DUMMY("core_clk", qdss_clk.c, "fc326000.tmc", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc320000.tpiu", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc324000.replicator", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc325000.tmc", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc323000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc321000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc322000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc355000.funnel", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc302000.stm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc34c000.etm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc34d000.etm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc34e000.etm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc34f000.etm", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc310000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc311000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc312000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc313000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc314000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc315000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc316000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc317000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc318000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc351000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc352000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc353000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc354000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc350000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc330000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc33c000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fc360000.cti", OFF),
	CLK_DUMMY("core_clk", qdss_clk.c, "fd828018.hwevent", OFF),

	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc326000.tmc", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc320000.tpiu", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc324000.replicator", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc325000.tmc", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc323000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc321000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc322000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc355000.funnel", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc302000.stm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc34c000.etm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc34d000.etm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc34e000.etm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc34f000.etm", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc310000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc311000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc312000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc313000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc314000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc315000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc316000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc317000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc318000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc351000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc352000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc353000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc354000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc350000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc330000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc33c000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fc360000.cti", OFF),
	CLK_DUMMY("core_a_clk", qdss_a_clk.c, "fd828018.hwevent", OFF),

	CLK_DUMMY("core_mmss_clk", mmss_misc_ahb_clk.c, "fd828018.hwevent",
		  OFF),
};

static struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

static struct of_dev_auxdata msmsamarium_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
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
	msm_clock_init(&msm_dummy_clock_init_data);
	tsens_tm_init_driver();
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
