/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include <mach/socinfo.h>

#include "acpuclock.h"
#include "acpuclock-krait.h"

/* Corner type vreg VDD values */
#define LVL_NONE	RPM_REGULATOR_CORNER_NONE
#define LVL_LOW		RPM_REGULATOR_CORNER_SVS_SOC
#define LVL_NOM		RPM_REGULATOR_CORNER_NORMAL
#define LVL_HIGH	RPM_REGULATOR_CORNER_SUPER_TURBO

static struct hfpll_data hfpll_data __initdata = {
	.mode_offset = 0x00,
	.l_offset = 0x04,
	.m_offset = 0x08,
	.n_offset = 0x0C,
	.has_user_reg = true,
	.user_offset = 0x10,
	.config_offset = 0x14,
	/* TODO: Verify magic numbers when final values are available. */
	.user_val = 0x8,
	.config_val = 0x04D0405D,
	.low_vco_l_max = 65,
	.low_vdd_l_max = 52,
	.nom_vdd_l_max = 104,
	.vdd[HFPLL_VDD_NONE] = LVL_NONE,
	.vdd[HFPLL_VDD_LOW]  = LVL_LOW,
	.vdd[HFPLL_VDD_NOM]  = LVL_NOM,
	.vdd[HFPLL_VDD_HIGH] = LVL_HIGH,
};

static struct scalable scalable[] __initdata = {
	[CPU0] = {
		.hfpll_phys_base = 0xF908A000,
		.l2cpmr_iaddr = 0x4501,
		.vreg[VREG_CORE] = { "krait0",     1050000 },
		.vreg[VREG_MEM]  = { "krait0_mem", 1050000 },
		.vreg[VREG_DIG]  = { "krait0_dig", LVL_HIGH },
		.vreg[VREG_HFPLL_A] = { "krait0_hfpll_a", 2150000 },
		.vreg[VREG_HFPLL_B] = { "krait0_hfpll_b", 1800000 },
	},
	[CPU1] = {
		.hfpll_phys_base = 0xF909A000,
		.l2cpmr_iaddr = 0x5501,
		.vreg[VREG_CORE] = { "krait1",     1050000 },
		.vreg[VREG_MEM]  = { "krait1_mem", 1050000 },
		.vreg[VREG_DIG]  = { "krait1_dig", LVL_HIGH },
		.vreg[VREG_HFPLL_A] = { "krait1_hfpll_a", 2150000 },
		.vreg[VREG_HFPLL_B] = { "krait1_hfpll_b", 1800000 },
	},
	[CPU2] = {
		.hfpll_phys_base = 0xF90AA000,
		.l2cpmr_iaddr = 0x6501,
		.vreg[VREG_CORE] = { "krait2",     1050000 },
		.vreg[VREG_MEM]  = { "krait2_mem", 1050000 },
		.vreg[VREG_DIG]  = { "krait2_dig", LVL_HIGH },
		.vreg[VREG_HFPLL_A] = { "krait2_hfpll_a", 2150000 },
		.vreg[VREG_HFPLL_B] = { "krait2_hfpll_b", 1800000 },
	},
	[CPU3] = {
		.hfpll_phys_base = 0xF90BA000,
		.l2cpmr_iaddr = 0x7501,
		.vreg[VREG_CORE] = { "krait3",     1050000 },
		.vreg[VREG_MEM]  = { "krait3_mem", 1050000 },
		.vreg[VREG_DIG]  = { "krait3_dig", LVL_HIGH },
		.vreg[VREG_HFPLL_A] = { "krait3_hfpll_a", 2150000 },
		.vreg[VREG_HFPLL_B] = { "krait3_hfpll_b", 1800000 },
	},
	[L2] = {
		.hfpll_phys_base = 0xF9016000,
		.l2cpmr_iaddr = 0x0500,
		.vreg[VREG_HFPLL_A] = { "l2_hfpll_a", 2150000 },
		.vreg[VREG_HFPLL_B] = { "l2_hfpll_b", 1800000 },
	},
};

static struct msm_bus_paths bw_level_tbl[] __initdata = {
	[0] =  BW_MBPS(552), /* At least  69 MHz on bus. */
	[1] = BW_MBPS(1112), /* At least 139 MHz on bus. */
	[2] = BW_MBPS(2224), /* At least 278 MHz on bus. */
	[3] = BW_MBPS(4448), /* At least 556 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_scale_data __initdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclk-8974",
};

static struct l2_level l2_freq_tbl[] __initdata = {
	[0]  = { {  300000, PLL_0, 0, 2,   0 }, LVL_LOW,   950000, 0 },
	[1]  = { {  384000, HFPLL, 2, 0,  40 }, LVL_NOM,   950000, 1 },
	[2]  = { {  460800, HFPLL, 2, 0,  48 }, LVL_NOM,   950000, 1 },
	[3]  = { {  537600, HFPLL, 1, 0,  28 }, LVL_NOM,   950000, 2 },
	[4]  = { {  576000, HFPLL, 1, 0,  30 }, LVL_NOM,   950000, 2 },
	[5]  = { {  652800, HFPLL, 1, 0,  34 }, LVL_NOM,   950000, 2 },
	[6]  = { {  729600, HFPLL, 1, 0,  38 }, LVL_NOM,   950000, 2 },
	[7]  = { {  806400, HFPLL, 1, 0,  42 }, LVL_NOM,   950000, 2 },
	[8]  = { {  883200, HFPLL, 1, 0,  46 }, LVL_HIGH, 1050000, 2 },
	[9]  = { {  960000, HFPLL, 1, 0,  50 }, LVL_HIGH, 1050000, 2 },
	[10] = { { 1036800, HFPLL, 1, 0,  54 }, LVL_HIGH, 1050000, 3 },
	[11] = { { 1113600, HFPLL, 1, 0,  58 }, LVL_HIGH, 1050000, 3 },
	[12] = { { 1190400, HFPLL, 1, 0,  62 }, LVL_HIGH, 1050000, 3 },
	[13] = { { 1267200, HFPLL, 1, 0,  66 }, LVL_HIGH, 1050000, 3 },
	[14] = { { 1344000, HFPLL, 1, 0,  70 }, LVL_HIGH, 1050000, 3 },
	[15] = { { 1420800, HFPLL, 1, 0,  74 }, LVL_HIGH, 1050000, 3 },
	[16] = { { 1497600, HFPLL, 1, 0,  78 }, LVL_HIGH, 1050000, 3 },
	[17] = { { 1574400, HFPLL, 1, 0,  82 }, LVL_HIGH, 1050000, 3 },
	[18] = { { 1651200, HFPLL, 1, 0,  86 }, LVL_HIGH, 1050000, 3 },
	[19] = { { 1728000, HFPLL, 1, 0,  90 }, LVL_HIGH, 1050000, 3 },
	[20] = { { 1804800, HFPLL, 1, 0,  94 }, LVL_HIGH, 1050000, 3 },
	[21] = { { 1881600, HFPLL, 1, 0,  98 }, LVL_HIGH, 1050000, 3 },
	[22] = { { 1958400, HFPLL, 1, 0, 102 }, LVL_HIGH, 1050000, 3 },
	[23] = { { 2035200, HFPLL, 1, 0, 106 }, LVL_HIGH, 1050000, 3 },
	[24] = { { 2112000, HFPLL, 1, 0, 110 }, LVL_HIGH, 1050000, 3 },
	[25] = { { 2188800, HFPLL, 1, 0, 114 }, LVL_HIGH, 1050000, 3 },
};

static struct acpu_level acpu_freq_tbl[] __initdata = {
	{ 1, {  300000, PLL_0, 0, 2,   0 }, L2(0),   950000, 3200000 },
	{ 1, {  384000, HFPLL, 2, 0,  40 }, L2(3),   950000, 3200000 },
	{ 1, {  460800, HFPLL, 2, 0,  48 }, L2(3),   950000, 3200000 },
	{ 1, {  537600, HFPLL, 1, 0,  28 }, L2(5),   950000, 3200000 },
	{ 1, {  576000, HFPLL, 1, 0,  30 }, L2(5),   950000, 3200000 },
	{ 1, {  652800, HFPLL, 1, 0,  34 }, L2(5),   950000, 3200000 },
	{ 1, {  729600, HFPLL, 1, 0,  38 }, L2(5),   950000, 3200000 },
	{ 1, {  806400, HFPLL, 1, 0,  42 }, L2(7),   950000, 3200000 },
	{ 1, {  883200, HFPLL, 1, 0,  46 }, L2(7),   950000, 3200000 },
	{ 1, {  960000, HFPLL, 1, 0,  50 }, L2(7),   950000, 3200000 },
	{ 1, { 1036800, HFPLL, 1, 0,  54 }, L2(7),   950000, 3200000 },
	{ 0, { 1113600, HFPLL, 1, 0,  58 }, L2(12), 1050000, 3200000 },
	{ 0, { 1190400, HFPLL, 1, 0,  62 }, L2(12), 1050000, 3200000 },
	{ 0, { 1267200, HFPLL, 1, 0,  66 }, L2(12), 1050000, 3200000 },
	{ 0, { 1344000, HFPLL, 1, 0,  70 }, L2(15), 1050000, 3200000 },
	{ 0, { 1420800, HFPLL, 1, 0,  74 }, L2(15), 1050000, 3200000 },
	{ 0, { 1497600, HFPLL, 1, 0,  78 }, L2(15), 1050000, 3200000 },
	{ 0, { 1574400, HFPLL, 1, 0,  82 }, L2(20), 1050000, 3200000 },
	{ 0, { 1651200, HFPLL, 1, 0,  86 }, L2(20), 1050000, 3200000 },
	{ 0, { 1728000, HFPLL, 1, 0,  90 }, L2(20), 1050000, 3200000 },
	{ 0, { 1804800, HFPLL, 1, 0,  94 }, L2(25), 1050000, 3200000 },
	{ 0, { 1881600, HFPLL, 1, 0,  98 }, L2(25), 1050000, 3200000 },
	{ 0, { 1958400, HFPLL, 1, 0, 102 }, L2(25), 1050000, 3200000 },
	{ 0, { 1996800, HFPLL, 1, 0, 104 }, L2(25), 1050000, 3200000 },
	{ 0, { 0 } }
};

static struct pvs_table pvs_tables[NUM_PVS]  __initdata = {
	[PVS_SLOW]    = { acpu_freq_tbl, sizeof(acpu_freq_tbl) },
	[PVS_NOMINAL] = { acpu_freq_tbl, sizeof(acpu_freq_tbl)  },
	[PVS_FAST]    = { acpu_freq_tbl, sizeof(acpu_freq_tbl) },
};

static struct acpuclk_krait_params acpuclk_8974_params __initdata = {
	.scalable = scalable,
	.scalable_size = sizeof(scalable),
	.hfpll_data = &hfpll_data,
	.pvs_tables = pvs_tables,
	.l2_freq_tbl = l2_freq_tbl,
	.l2_freq_tbl_size = sizeof(l2_freq_tbl),
	.bus_scale = &bus_scale_data,
	.qfprom_phys_base = 0xFC4A8000,
	.stby_khz = 300000,
};

static int __init acpuclk_8974_probe(struct platform_device *pdev)
{
	return acpuclk_krait_init(&pdev->dev, &acpuclk_8974_params);
}

static struct of_device_id acpuclk_8974_match_table[] = {
	{ .compatible = "qcom,acpuclk-8974" },
	{}
};

static struct platform_driver acpuclk_8974_driver = {
	.driver = {
		.name = "acpuclk-8974",
		.of_match_table = acpuclk_8974_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_8974_init(void)
{
	return platform_driver_probe(&acpuclk_8974_driver,
				     acpuclk_8974_probe);
}
device_initcall(acpuclk_8974_init);
