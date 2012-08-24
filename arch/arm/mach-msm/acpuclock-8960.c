/*
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>

#include "acpuclock.h"
#include "acpuclock-krait.h"

static struct hfpll_data hfpll_data __initdata = {
	.mode_offset = 0x00,
	.l_offset = 0x08,
	.m_offset = 0x0C,
	.n_offset = 0x10,
	.config_offset = 0x04,
	.config_val = 0x7845C665,
	.has_droop_ctl = true,
	.droop_offset = 0x14,
	.droop_val = 0x0108C000,
	.low_vdd_l_max = 22,
	.nom_vdd_l_max = 42,
	.vdd[HFPLL_VDD_NONE] =       0,
	.vdd[HFPLL_VDD_LOW]  =  945000,
	.vdd[HFPLL_VDD_NOM]  = 1050000,
	.vdd[HFPLL_VDD_HIGH] = 1150000,
};

static struct scalable scalable[] __initdata = {
	[CPU0] = {
		.hfpll_phys_base = 0x00903200,
		.aux_clk_sel_phys = 0x02088014,
		.aux_clk_sel = 3,
		.sec_clk_sel = 2,
		.l2cpmr_iaddr = 0x4501,
		.vreg[VREG_CORE] = { "krait0", 1300000 },
		.vreg[VREG_MEM]  = { "krait0_mem", 1150000 },
		.vreg[VREG_DIG]  = { "krait0_dig", 1150000 },
		.vreg[VREG_HFPLL_A] = { "krait0_s8", 2050000 },
		.vreg[VREG_HFPLL_B] = { "krait0_l23", 1800000 },
	},
	[CPU1] = {
		.hfpll_phys_base = 0x00903300,
		.aux_clk_sel_phys = 0x02098014,
		.aux_clk_sel = 3,
		.sec_clk_sel = 2,
		.l2cpmr_iaddr = 0x5501,
		.vreg[VREG_CORE] = { "krait1", 1300000 },
		.vreg[VREG_MEM]  = { "krait1_mem", 1150000 },
		.vreg[VREG_DIG]  = { "krait1_dig", 1150000 },
		.vreg[VREG_HFPLL_A] = { "krait1_s8", 2050000 },
		.vreg[VREG_HFPLL_B] = { "krait1_l23", 1800000 },
	},
	[L2] = {
		.hfpll_phys_base = 0x00903400,
		.aux_clk_sel_phys = 0x02011028,
		.aux_clk_sel = 3,
		.sec_clk_sel = 2,
		.l2cpmr_iaddr = 0x0500,
		.vreg[VREG_HFPLL_A] = { "l2_s8", 2050000 },
		.vreg[VREG_HFPLL_B] = { "l2_l23", 1800000 },
	},
};

static struct msm_bus_paths bw_level_tbl[] __initdata = {
	[0] =  BW_MBPS(640), /* At least  80 MHz on bus. */
	[1] = BW_MBPS(1064), /* At least 133 MHz on bus. */
	[2] = BW_MBPS(1600), /* At least 200 MHz on bus. */
	[3] = BW_MBPS(2128), /* At least 266 MHz on bus. */
	[4] = BW_MBPS(3200), /* At least 400 MHz on bus. */
	[5] = BW_MBPS(3600), /* At least 450 MHz on bus. */
	[6] = BW_MBPS(3936), /* At least 492 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_scale_data __initdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclk-8960",
};

static struct l2_level l2_freq_tbl[] __initdata = {
	[0]  = { {  384000, PLL_8, 0, 0x00 }, 1050000, 1050000, 1 },
	[1]  = { {  432000, HFPLL, 2, 0x20 }, 1050000, 1050000, 2 },
	[2]  = { {  486000, HFPLL, 2, 0x24 }, 1050000, 1050000, 2 },
	[3]  = { {  540000, HFPLL, 2, 0x28 }, 1050000, 1050000, 2 },
	[4]  = { {  594000, HFPLL, 1, 0x16 }, 1050000, 1050000, 2 },
	[5]  = { {  648000, HFPLL, 1, 0x18 }, 1050000, 1050000, 4 },
	[6]  = { {  702000, HFPLL, 1, 0x1A }, 1050000, 1050000, 4 },
	[7]  = { {  756000, HFPLL, 1, 0x1C }, 1150000, 1150000, 4 },
	[8]  = { {  810000, HFPLL, 1, 0x1E }, 1150000, 1150000, 4 },
	[9]  = { {  864000, HFPLL, 1, 0x20 }, 1150000, 1150000, 4 },
	[10] = { {  918000, HFPLL, 1, 0x22 }, 1150000, 1150000, 6 },
	[11] = { {  972000, HFPLL, 1, 0x24 }, 1150000, 1150000, 6 },
	[12] = { { 1026000, HFPLL, 1, 0x26 }, 1150000, 1150000, 6 },
	[13] = { { 1080000, HFPLL, 1, 0x28 }, 1150000, 1150000, 6 },
	[14] = { { 1134000, HFPLL, 1, 0x2A }, 1150000, 1150000, 6 },
	[15] = { { 1188000, HFPLL, 1, 0x2C }, 1150000, 1150000, 6 },
	[16] = { { 1242000, HFPLL, 1, 0x2E }, 1150000, 1150000, 6 },
	[17] = { { 1296000, HFPLL, 1, 0x30 }, 1150000, 1150000, 6 },
	[18] = { { 1350000, HFPLL, 1, 0x32 }, 1150000, 1150000, 6 },
	{ }
};

#define AVS(x) .avsdscr_setting = (x)

static struct acpu_level acpu_freq_tbl_slow[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   950000, AVS(0x40001F) },
	{ 0, {   432000, HFPLL, 2, 0x20 }, L2(6),   975000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(6),   975000 },
	{ 0, {   540000, HFPLL, 2, 0x28 }, L2(6),  1000000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(6),  1000000 },
	{ 0, {   648000, HFPLL, 1, 0x18 }, L2(6),  1025000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(6),  1025000 },
	{ 0, {   756000, HFPLL, 1, 0x1C }, L2(6),  1075000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(6),  1075000 },
	{ 0, {   864000, HFPLL, 1, 0x20 }, L2(6),  1100000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(6),  1100000 },
	{ 0, {   972000, HFPLL, 1, 0x24 }, L2(6),  1125000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(6),  1125000 },
	{ 0, {  1080000, HFPLL, 1, 0x28 }, L2(18), 1175000, AVS(0x400015) },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(18), 1175000, AVS(0x400015) },
	{ 0, {  1188000, HFPLL, 1, 0x2C }, L2(18), 1200000, AVS(0x400015) },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(18), 1200000, AVS(0x400015) },
	{ 0, {  1296000, HFPLL, 1, 0x30 }, L2(18), 1225000, AVS(0x400015) },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(18), 1225000, AVS(0x400015) },
	{ 0, {  1404000, HFPLL, 1, 0x34 }, L2(18), 1237500, AVS(0x400015) },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(18), 1237500, AVS(0x100018) },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(18), 1250000, AVS(0x400012) },
	{ 0, { 0 } }
};

static struct acpu_level acpu_freq_tbl_nom[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   900000, AVS(0x40007F) },
	{ 0, {   432000, HFPLL, 2, 0x20 }, L2(6),   925000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(6),   925000 },
	{ 0, {   540000, HFPLL, 2, 0x28 }, L2(6),   950000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(6),   950000 },
	{ 0, {   648000, HFPLL, 1, 0x18 }, L2(6),   975000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(6),   975000 },
	{ 0, {   756000, HFPLL, 1, 0x1C }, L2(6),  1025000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(6),  1025000 },
	{ 0, {   864000, HFPLL, 1, 0x20 }, L2(6),  1050000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(6),  1050000 },
	{ 0, {   972000, HFPLL, 1, 0x24 }, L2(6),  1075000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(6),  1075000 },
	{ 0, {  1080000, HFPLL, 1, 0x28 }, L2(18), 1125000, AVS(0x400015) },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(18), 1125000, AVS(0x400015) },
	{ 0, {  1188000, HFPLL, 1, 0x2C }, L2(18), 1150000, AVS(0x400015) },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(18), 1150000, AVS(0x400015) },
	{ 0, {  1296000, HFPLL, 1, 0x30 }, L2(18), 1175000, AVS(0x400015) },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(18), 1175000, AVS(0x400015) },
	{ 0, {  1404000, HFPLL, 1, 0x34 }, L2(18), 1187500, AVS(0x400015) },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(18), 1187500, AVS(0x100018) },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(18), 1200000, AVS(0x400012) },
	{ 0, { 0 } }
};

static struct acpu_level acpu_freq_tbl_fast[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   850000, AVS(0x4000FF) },
	{ 0, {   432000, HFPLL, 2, 0x20 }, L2(6),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(6),   875000 },
	{ 0, {   540000, HFPLL, 2, 0x28 }, L2(6),   900000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(6),   900000 },
	{ 0, {   648000, HFPLL, 1, 0x18 }, L2(6),   925000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(6),   925000 },
	{ 0, {   756000, HFPLL, 1, 0x1C }, L2(6),   975000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(6),   975000 },
	{ 0, {   864000, HFPLL, 1, 0x20 }, L2(6),  1000000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(6),  1000000 },
	{ 0, {   972000, HFPLL, 1, 0x24 }, L2(6),  1025000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(6),  1025000 },
	{ 0, {  1080000, HFPLL, 1, 0x28 }, L2(18), 1075000, AVS(0x10001B) },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(18), 1075000, AVS(0x10001B) },
	{ 0, {  1188000, HFPLL, 1, 0x2C }, L2(18), 1100000, AVS(0x10001B) },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(18), 1100000, AVS(0x10001B) },
	{ 0, {  1296000, HFPLL, 1, 0x30 }, L2(18), 1125000, AVS(0x10001B) },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(18), 1125000, AVS(0x400012) },
	{ 0, {  1404000, HFPLL, 1, 0x34 }, L2(18), 1137500, AVS(0x400012) },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(18), 1137500, AVS(0x400012) },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(18), 1150000, AVS(0x400012) },
	{ 0, { 0 } }
};

static struct pvs_table pvs_tables[NUM_SPEED_BINS][NUM_PVS] __initdata = {
[0][PVS_SLOW]    = { acpu_freq_tbl_slow, sizeof(acpu_freq_tbl_slow),     0 },
[0][PVS_NOMINAL] = { acpu_freq_tbl_nom,  sizeof(acpu_freq_tbl_nom),  25000 },
[0][PVS_FAST]    = { acpu_freq_tbl_fast, sizeof(acpu_freq_tbl_fast), 25000 },
};

static struct acpuclk_krait_params acpuclk_8960_params __initdata = {
	.scalable = scalable,
	.scalable_size = sizeof(scalable),
	.hfpll_data = &hfpll_data,
	.pvs_tables = pvs_tables,
	.l2_freq_tbl = l2_freq_tbl,
	.l2_freq_tbl_size = sizeof(l2_freq_tbl),
	.bus_scale = &bus_scale_data,
	.pte_efuse_phys = 0x007000C0,
	.stby_khz = 384000,
};

static int __init acpuclk_8960_probe(struct platform_device *pdev)
{
	return acpuclk_krait_init(&pdev->dev, &acpuclk_8960_params);
}

static struct platform_driver acpuclk_8960_driver = {
	.driver = {
		.name = "acpuclk-8960",
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_8960_init(void)
{
	return platform_driver_probe(&acpuclk_8960_driver, acpuclk_8960_probe);
}
device_initcall(acpuclk_8960_init);
