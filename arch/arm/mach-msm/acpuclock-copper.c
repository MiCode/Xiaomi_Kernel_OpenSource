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

#include <linux/platform_device.h>
#include <linux/of.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include <mach/socinfo.h>

#include "acpuclock.h"
#include "acpuclock-krait.h"

/* Corner type vreg VDD values */
#define LVL_NONE	RPM_VREG_CORNER_NONE
#define LVL_LOW		RPM_VREG_CORNER_LOW
#define LVL_NOM		RPM_VREG_CORNER_NOMINAL
#define LVL_HIGH	RPM_VREG_CORNER_HIGH

static struct hfpll_data hfpll_data_cpu = {
	.mode_offset = 0x00,
	.l_offset = 0x04,
	.m_offset = 0x08,
	.n_offset = 0x0C,
	.config_offset = 0x14,
	/* TODO: Verify magic number for copper when available. */
	.config_val = 0x7845C665,
	.low_vdd_l_max = 52,
	.vdd[HFPLL_VDD_NONE] = 0,
	.vdd[HFPLL_VDD_LOW]  = 810000,
	.vdd[HFPLL_VDD_NOM]  = 900000,
};

static struct hfpll_data hfpll_data_l2 = {
	.mode_offset = 0x00,
	.l_offset = 0x04,
	.m_offset = 0x08,
	.n_offset = 0x0C,
	.config_offset = 0x14,
	/* TODO: Verify magic number for copper when available. */
	.config_val = 0x7845C665,
	.low_vdd_l_max = 52,
	.vdd[HFPLL_VDD_NONE] = LVL_NONE,
	.vdd[HFPLL_VDD_LOW]  = LVL_LOW,
	.vdd[HFPLL_VDD_NOM]  = LVL_NOM,
};

static struct scalable scalable[] = {
	[CPU0] = {
		.hfpll_phys_base = 0xF908A000,
		.hfpll_data = &hfpll_data_cpu,
		.l2cpmr_iaddr = 0x4501,
		.vreg[VREG_CORE] = { "krait0",     1050000, 3200000 },
		.vreg[VREG_MEM]  = { "krait0_mem", 1050000, 0,
				     RPM_VREG_VOTER1,
				     RPM_VREG_ID_PM8941_S1 },
		.vreg[VREG_DIG]  = { "krait0_dig", 1050000, 0,
				     RPM_VREG_VOTER1,
				     RPM_VREG_ID_PM8941_S2 },
		.vreg[VREG_HFPLL_A] = { "hfpll", 1800000, 0,
				     RPM_VREG_VOTER1,
				     RPM_VREG_ID_PM8941_L12 },
	},
	[CPU1] = {
		.hfpll_phys_base = 0xF909A000,
		.hfpll_data = &hfpll_data_cpu,
		.l2cpmr_iaddr = 0x5501,
		.vreg[VREG_CORE] = { "krait1",     1050000, 3200000 },
		.vreg[VREG_MEM]  = { "krait1_mem", 1050000, 0,
				     RPM_VREG_VOTER2,
				     RPM_VREG_ID_PM8941_S1 },
		.vreg[VREG_DIG]  = { "krait1_dig", 1050000, 0,
				     RPM_VREG_VOTER2,
				     RPM_VREG_ID_PM8941_S2 },
		.vreg[VREG_HFPLL_A] = { "hfpll", 1800000, 0,
				     RPM_VREG_VOTER2,
				     RPM_VREG_ID_PM8941_L12 },
	},
	[CPU2] = {
		.hfpll_phys_base = 0xF90AA000,
		.hfpll_data = &hfpll_data_cpu,
		.l2cpmr_iaddr = 0x6501,
		.vreg[VREG_CORE] = { "krait2",     1050000, 3200000 },
		.vreg[VREG_MEM]  = { "krait2_mem", 1050000, 0,
				     RPM_VREG_VOTER4,
				     RPM_VREG_ID_PM8921_S1 },
		.vreg[VREG_DIG]  = { "krait2_dig", 1050000, 0,
				     RPM_VREG_VOTER4,
				     RPM_VREG_ID_PM8921_S2 },
		.vreg[VREG_HFPLL_A] = { "hfpll", 1800000, 0,
				     RPM_VREG_VOTER4,
				     RPM_VREG_ID_PM8941_L12 },
	},
	[CPU3] = {
		.hfpll_phys_base = 0xF90BA000,
		.hfpll_data = &hfpll_data_cpu,
		.l2cpmr_iaddr = 0x7501,
		.vreg[VREG_CORE] = { "krait3",     1050000, 3200000 },
		.vreg[VREG_MEM]  = { "krait3_mem", 1050000, 0,
				     RPM_VREG_VOTER5,
				     RPM_VREG_ID_PM8941_S1 },
		.vreg[VREG_DIG]  = { "krait3_dig", 1050000, 0,
				     RPM_VREG_VOTER5,
				     RPM_VREG_ID_PM8941_S2 },
		.vreg[VREG_HFPLL_A] = { "hfpll", 1800000, 0,
				     RPM_VREG_VOTER5,
				     RPM_VREG_ID_PM8941_L12 },
	},
	[L2] = {
		.hfpll_phys_base = 0xF9016000,
		.hfpll_data = &hfpll_data_l2,
		.l2cpmr_iaddr = 0x0500,
		.vreg[VREG_HFPLL_A] = { "hfpll", 1800000, 0,
				     RPM_VREG_VOTER6,
				     RPM_VREG_ID_PM8941_L12 },
	},
};

static struct msm_bus_paths bw_level_tbl[] = {
	[0] =  BW_MBPS(400), /* At least  50 MHz on bus. */
	[1] =  BW_MBPS(800), /* At least 100 MHz on bus. */
	[2] = BW_MBPS(1334), /* At least 167 MHz on bus. */
	[3] = BW_MBPS(2666), /* At least 200 MHz on bus. */
	[4] = BW_MBPS(3200), /* At least 333 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_scale_data = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclk-copper",
};

#define L2(x) (&l2_freq_tbl[(x)])
static struct l2_level l2_freq_tbl[] = {
	[0]  = { {STBY_KHZ, QSB,   0, 0,   0 }, LVL_NOM, 1050000, 0 },
	[1]  = { {  300000, PLL_0, 0, 2,   0 }, LVL_NOM, 1050000, 2 },
	[2]  = { {  384000, HFPLL, 2, 0,  40 }, LVL_NOM, 1050000, 2 },
	[3]  = { {  460800, HFPLL, 2, 0,  48 }, LVL_NOM, 1050000, 2 },
	[4]  = { {  537600, HFPLL, 1, 0,  28 }, LVL_NOM, 1050000, 2 },
	[5]  = { {  576000, HFPLL, 1, 0,  30 }, LVL_NOM, 1050000, 3 },
	[6]  = { {  652800, HFPLL, 1, 0,  34 }, LVL_NOM, 1050000, 3 },
	[7]  = { {  729600, HFPLL, 1, 0,  38 }, LVL_NOM, 1050000, 3 },
	[8]  = { {  806400, HFPLL, 1, 0,  42 }, LVL_NOM, 1050000, 3 },
	[9]  = { {  883200, HFPLL, 1, 0,  46 }, LVL_NOM, 1050000, 4 },
	[10] = { {  960000, HFPLL, 1, 0,  50 }, LVL_NOM, 1050000, 4 },
	[11] = { { 1036800, HFPLL, 1, 0,  54 }, LVL_NOM, 1050000, 4 },
};

static struct acpu_level acpu_freq_tbl[] = {
	{ 0, {STBY_KHZ, QSB,   0, 0,   0 }, L2(0),  1050000 },
	{ 1, {  300000, PLL_0, 0, 2,   0 }, L2(1),  1050000 },
	{ 1, {  384000, HFPLL, 2, 0,  40 }, L2(2),  1050000 },
	{ 1, {  460800, HFPLL, 2, 0,  48 }, L2(3),  1050000 },
	{ 1, {  537600, HFPLL, 1, 0,  28 }, L2(4),  1050000 },
	{ 1, {  576000, HFPLL, 1, 0,  30 }, L2(5),  1050000 },
	{ 1, {  652800, HFPLL, 1, 0,  34 }, L2(6),  1050000 },
	{ 1, {  729600, HFPLL, 1, 0,  38 }, L2(7),  1050000 },
	{ 1, {  806400, HFPLL, 1, 0,  42 }, L2(8),  1050000 },
	{ 1, {  883200, HFPLL, 1, 0,  46 }, L2(9),  1050000 },
	{ 1, {  960000, HFPLL, 1, 0,  50 }, L2(10), 1050000 },
	{ 1, { 1036800, HFPLL, 1, 0,  54 }, L2(11), 1050000 },
	{ 0, { 0 } }
};

static struct acpuclk_krait_params acpuclk_copper_params = {
	.scalable = scalable,
	.pvs_acpu_freq_tbl[PVS_SLOW] = acpu_freq_tbl,
	.pvs_acpu_freq_tbl[PVS_NOMINAL] = acpu_freq_tbl,
	.pvs_acpu_freq_tbl[PVS_FAST] = acpu_freq_tbl,
	.l2_freq_tbl = l2_freq_tbl,
	.l2_freq_tbl_size = ARRAY_SIZE(l2_freq_tbl),
	.bus_scale_data = &bus_scale_data,
	.qfprom_phys_base = 0xFC4A8000,
};

static int __init acpuclk_copper_probe(struct platform_device *pdev)
{
	return acpuclk_krait_init(&pdev->dev, &acpuclk_copper_params);
}

static struct of_device_id acpuclk_copper_match_table[] = {
	{ .compatible = "qcom,acpuclk-copper" },
	{}
};

static struct platform_driver acpuclk_copper_driver = {
	.driver = {
		.name = "acpuclk-copper",
		.of_match_table = acpuclk_copper_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_8960_init(void)
{
	return platform_driver_probe(&acpuclk_copper_driver,
				     acpuclk_copper_probe);
}
device_initcall(acpuclk_8960_init);
