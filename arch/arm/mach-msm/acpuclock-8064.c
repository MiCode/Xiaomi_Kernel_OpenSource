/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include "mach/socinfo.h"
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
		.vreg[VREG_HFPLL_A] = { "krait0_hfpll", 1800000 },
	},
	[CPU1] = {
		.hfpll_phys_base = 0x00903240,
		.aux_clk_sel_phys = 0x02098014,
		.aux_clk_sel = 3,
		.sec_clk_sel = 2,
		.l2cpmr_iaddr = 0x5501,
		.vreg[VREG_CORE] = { "krait1", 1300000 },
		.vreg[VREG_MEM]  = { "krait1_mem", 1150000 },
		.vreg[VREG_DIG]  = { "krait1_dig", 1150000 },
		.vreg[VREG_HFPLL_A] = { "krait1_hfpll", 1800000 },
	},
	[CPU2] = {
		.hfpll_phys_base = 0x00903280,
		.aux_clk_sel_phys = 0x020A8014,
		.aux_clk_sel = 3,
		.sec_clk_sel = 2,
		.l2cpmr_iaddr = 0x6501,
		.vreg[VREG_CORE] = { "krait2", 1300000 },
		.vreg[VREG_MEM]  = { "krait2_mem", 1150000 },
		.vreg[VREG_DIG]  = { "krait2_dig", 1150000 },
		.vreg[VREG_HFPLL_A] = { "krait2_hfpll", 1800000 },
	},
	[CPU3] = {
		.hfpll_phys_base = 0x009032C0,
		.aux_clk_sel_phys = 0x020B8014,
		.aux_clk_sel = 3,
		.sec_clk_sel = 2,
		.l2cpmr_iaddr = 0x7501,
		.vreg[VREG_CORE] = { "krait3", 1300000 },
		.vreg[VREG_MEM]  = { "krait3_mem", 1150000 },
		.vreg[VREG_DIG]  = { "krait3_dig", 1150000 },
		.vreg[VREG_HFPLL_A] = { "krait3_hfpll", 1800000 },
	},
	[L2] = {
		.hfpll_phys_base = 0x00903300,
		.aux_clk_sel_phys = 0x02011028,
		.aux_clk_sel = 3,
		.sec_clk_sel = 2,
		.l2cpmr_iaddr = 0x0500,
		.vreg[VREG_HFPLL_A] = { "l2_hfpll", 1800000 },
	},
};

/*
 * The correct maximum rate for 8064ab in 600 MHZ.
 * We rely on the RPM rounding requests up here.
*/
static struct msm_bus_paths bw_level_tbl[] __initdata = {
	[0] =  BW_MBPS(640), /* At least  80 MHz on bus. */
	[1] = BW_MBPS(1064), /* At least 133 MHz on bus. */
	[2] = BW_MBPS(1600), /* At least 200 MHz on bus. */
	[3] = BW_MBPS(2128), /* At least 266 MHz on bus. */
	[4] = BW_MBPS(3200), /* At least 400 MHz on bus. */
	[5] = BW_MBPS(4264), /* At least 533 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_scale_data __initdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclk-8064",
};

static struct l2_level l2_freq_tbl[] __initdata = {
	[0]  = { {  384000, PLL_8, 0, 0x00 },  950000, 1050000, 1 },
	[1]  = { {  432000, HFPLL, 2, 0x20 }, 1050000, 1050000, 2 },
	[2]  = { {  486000, HFPLL, 2, 0x24 }, 1050000, 1050000, 2 },
	[3]  = { {  540000, HFPLL, 2, 0x28 }, 1050000, 1050000, 2 },
	[4]  = { {  594000, HFPLL, 1, 0x16 }, 1050000, 1050000, 2 },
	[5]  = { {  648000, HFPLL, 1, 0x18 }, 1050000, 1050000, 4 },
	[6]  = { {  702000, HFPLL, 1, 0x1A }, 1150000, 1150000, 4 },
	[7]  = { {  756000, HFPLL, 1, 0x1C }, 1150000, 1150000, 4 },
	[8]  = { {  810000, HFPLL, 1, 0x1E }, 1150000, 1150000, 4 },
	[9]  = { {  864000, HFPLL, 1, 0x20 }, 1150000, 1150000, 4 },
	[10] = { {  918000, HFPLL, 1, 0x22 }, 1150000, 1150000, 5 },
	[11] = { {  972000, HFPLL, 1, 0x24 }, 1150000, 1150000, 5 },
	[12] = { { 1026000, HFPLL, 1, 0x26 }, 1150000, 1150000, 5 },
	[13] = { { 1080000, HFPLL, 1, 0x28 }, 1150000, 1150000, 5 },
	[14] = { { 1134000, HFPLL, 1, 0x2A }, 1150000, 1150000, 5 },
	{ }
};

static struct acpu_level tbl_slow[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   950000 },
	{ 0, {   432000, HFPLL, 2, 0x20 }, L2(5),   975000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   975000 },
	{ 0, {   540000, HFPLL, 2, 0x28 }, L2(5),  1000000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),  1000000 },
	{ 0, {   648000, HFPLL, 1, 0x18 }, L2(5),  1025000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),  1025000 },
	{ 0, {   756000, HFPLL, 1, 0x1C }, L2(5),  1075000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),  1075000 },
	{ 0, {   864000, HFPLL, 1, 0x20 }, L2(5),  1100000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),  1100000 },
	{ 0, {   972000, HFPLL, 1, 0x24 }, L2(5),  1125000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1125000 },
	{ 0, {  1080000, HFPLL, 1, 0x28 }, L2(14), 1175000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1175000 },
	{ 0, {  1188000, HFPLL, 1, 0x2C }, L2(14), 1200000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1200000 },
	{ 0, {  1296000, HFPLL, 1, 0x30 }, L2(14), 1225000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1225000 },
	{ 0, {  1404000, HFPLL, 1, 0x34 }, L2(14), 1237500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1237500 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1250000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_nom[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   900000 },
	{ 0, {   432000, HFPLL, 2, 0x20 }, L2(5),   925000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   925000 },
	{ 0, {   540000, HFPLL, 2, 0x28 }, L2(5),   950000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   950000 },
	{ 0, {   648000, HFPLL, 1, 0x18 }, L2(5),   975000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   975000 },
	{ 0, {   756000, HFPLL, 1, 0x1C }, L2(5),  1025000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),  1025000 },
	{ 0, {   864000, HFPLL, 1, 0x20 }, L2(5),  1050000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),  1050000 },
	{ 0, {   972000, HFPLL, 1, 0x24 }, L2(5),  1075000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1075000 },
	{ 0, {  1080000, HFPLL, 1, 0x28 }, L2(14), 1125000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1125000 },
	{ 0, {  1188000, HFPLL, 1, 0x2C }, L2(14), 1150000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1150000 },
	{ 0, {  1296000, HFPLL, 1, 0x30 }, L2(14), 1175000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1175000 },
	{ 0, {  1404000, HFPLL, 1, 0x34 }, L2(14), 1187500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1187500 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1200000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_fast[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   850000 },
	{ 0, {   432000, HFPLL, 2, 0x20 }, L2(5),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 0, {   540000, HFPLL, 2, 0x28 }, L2(5),   900000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   900000 },
	{ 0, {   648000, HFPLL, 1, 0x18 }, L2(5),   925000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   925000 },
	{ 0, {   756000, HFPLL, 1, 0x1C }, L2(5),   975000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   975000 },
	{ 0, {   864000, HFPLL, 1, 0x20 }, L2(5),  1000000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),  1000000 },
	{ 0, {   972000, HFPLL, 1, 0x24 }, L2(5),  1025000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1025000 },
	{ 0, {  1080000, HFPLL, 1, 0x28 }, L2(14), 1075000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1075000 },
	{ 0, {  1188000, HFPLL, 1, 0x2C }, L2(14), 1100000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1100000 },
	{ 0, {  1296000, HFPLL, 1, 0x30 }, L2(14), 1125000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1125000 },
	{ 0, {  1404000, HFPLL, 1, 0x34 }, L2(14), 1137500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1137500 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1150000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_faster[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   850000 },
	{ 0, {   432000, HFPLL, 2, 0x20 }, L2(5),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 0, {   540000, HFPLL, 2, 0x28 }, L2(5),   900000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   900000 },
	{ 0, {   648000, HFPLL, 1, 0x18 }, L2(5),   925000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   925000 },
	{ 0, {   756000, HFPLL, 1, 0x1C }, L2(5),   962500 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   962500 },
	{ 0, {   864000, HFPLL, 1, 0x20 }, L2(5),   975000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   975000 },
	{ 0, {   972000, HFPLL, 1, 0x24 }, L2(5),  1000000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1000000 },
	{ 0, {  1080000, HFPLL, 1, 0x28 }, L2(14), 1050000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1050000 },
	{ 0, {  1188000, HFPLL, 1, 0x2C }, L2(14), 1075000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1075000 },
	{ 0, {  1296000, HFPLL, 1, 0x30 }, L2(14), 1100000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1100000 },
	{ 0, {  1404000, HFPLL, 1, 0x34 }, L2(14), 1112500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1112500 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1125000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS0_1512MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   950000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   950000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   950000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   962500 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),  1000000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),  1025000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1037500 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1075000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1087500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1125000 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1150000 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1162500 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS1_1512MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   950000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   950000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   950000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   962500 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   975000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),  1000000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1012500 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1037500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1050000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1087500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1112500 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1125000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS2_1512MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   925000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   925000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   925000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   925000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   937500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   950000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   975000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1000000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1012500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1037500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1075000 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1087500 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS3_1512MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   900000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   900000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   900000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   900000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   900000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   925000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   950000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  975000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  987500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1000000 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1037500 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1050000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS4_1512MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  950000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  962500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  975000 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1000000 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1012500 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS5_1512MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  937500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  950000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  962500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14),  987500 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14), 1000000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS6_1512MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  937500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  950000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  962500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14),  975000 },
	{ 1, {  1512000, HFPLL, 1, 0x38 }, L2(14),  987500 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS0_1700MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   950000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   950000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   950000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   962500 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),  1000000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),  1025000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1037500 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1075000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1087500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1125000 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1150000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1175000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1225000 },
	{ 1, {  1728000, HFPLL, 1, 0x40 }, L2(14), 1250000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS1_1700MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   950000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   950000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   950000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   962500 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   975000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),  1000000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1012500 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1037500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1050000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1087500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1112500 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1150000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1187500 },
	{ 1, {  1728000, HFPLL, 1, 0x40 }, L2(14), 1200000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS2_1700MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   925000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   925000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   925000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   925000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   937500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   950000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   975000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1000000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1012500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1037500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1075000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1100000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1137500 },
	{ 1, {  1728000, HFPLL, 1, 0x40 }, L2(14), 1162500 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS3_1700MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   900000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   900000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   900000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   900000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   900000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   925000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   950000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  975000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  987500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1000000 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1037500 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1062500 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1100000 },
	{ 1, {  1728000, HFPLL, 1, 0x40 }, L2(14), 1125000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS4_1700MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  950000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  962500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  975000 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1000000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1037500 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1075000 },
	{ 1, {  1728000, HFPLL, 1, 0x40 }, L2(14), 1100000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS5_1700MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  937500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  950000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  962500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14),  987500 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1012500 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1050000 },
	{ 1, {  1728000, HFPLL, 1, 0x40 }, L2(14), 1075000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS6_1700MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  937500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  950000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  962500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14),  975000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1000000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1025000 },
	{ 1, {  1728000, HFPLL, 1, 0x40 }, L2(14), 1050000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS0_2000MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   950000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   950000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   950000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   950000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   962500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   975000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),  1000000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1025000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1037500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1062500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1100000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1125000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1175000 },
	{ 1, {  1782000, HFPLL, 1, 0x42 }, L2(14), 1225000 },
	{ 1, {  1890000, HFPLL, 1, 0x46 }, L2(14), 1287500 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS1_2000MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   925000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   925000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   925000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   925000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   937500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   950000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   975000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14), 1000000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14), 1012500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1037500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1075000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1100000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1137500 },
	{ 1, {  1782000, HFPLL, 1, 0x42 }, L2(14), 1187500 },
	{ 1, {  1890000, HFPLL, 1, 0x46 }, L2(14), 1250000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS2_2000MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   900000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   900000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   900000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   900000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   912500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   925000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   950000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  975000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  987500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1012500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1050000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1075000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1112500 },
	{ 1, {  1782000, HFPLL, 1, 0x42 }, L2(14), 1162500 },
	{ 1, {  1890000, HFPLL, 1, 0x46 }, L2(14), 1212500 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS3_2000MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   900000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   900000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   900000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   900000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   900000 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   912500 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   937500 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  962500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  975000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14), 1000000 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1025000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1050000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1087500 },
	{ 1, {  1782000, HFPLL, 1, 0x42 }, L2(14), 1137500 },
	{ 1, {  1890000, HFPLL, 1, 0x46 }, L2(14), 1175000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS4_2000MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  950000 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  962500 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  975000 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14), 1000000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1037500 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1075000 },
	{ 1, {  1782000, HFPLL, 1, 0x42 }, L2(14), 1112500 },
	{ 1, {  1890000, HFPLL, 1, 0x46 }, L2(14), 1150000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS5_2000MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  937500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  950000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  962500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14),  987500 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1012500 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1050000 },
	{ 1, {  1782000, HFPLL, 1, 0x42 }, L2(14), 1087500 },
	{ 1, {  1890000, HFPLL, 1, 0x46 }, L2(14), 1125000 },
	{ 0, { 0 } }
};

static struct acpu_level tbl_PVS6_2000MHz[] __initdata = {
	{ 1, {   384000, PLL_8, 0, 0x00 }, L2(0),   875000 },
	{ 1, {   486000, HFPLL, 2, 0x24 }, L2(5),   875000 },
	{ 1, {   594000, HFPLL, 1, 0x16 }, L2(5),   875000 },
	{ 1, {   702000, HFPLL, 1, 0x1A }, L2(5),   875000 },
	{ 1, {   810000, HFPLL, 1, 0x1E }, L2(5),   887500 },
	{ 1, {   918000, HFPLL, 1, 0x22 }, L2(5),   900000 },
	{ 1, {  1026000, HFPLL, 1, 0x26 }, L2(5),   925000 },
	{ 1, {  1134000, HFPLL, 1, 0x2A }, L2(14),  937500 },
	{ 1, {  1242000, HFPLL, 1, 0x2E }, L2(14),  950000 },
	{ 1, {  1350000, HFPLL, 1, 0x32 }, L2(14),  962500 },
	{ 1, {  1458000, HFPLL, 1, 0x36 }, L2(14),  975000 },
	{ 1, {  1566000, HFPLL, 1, 0x3A }, L2(14), 1000000 },
	{ 1, {  1674000, HFPLL, 1, 0x3E }, L2(14), 1025000 },
	{ 1, {  1782000, HFPLL, 1, 0x42 }, L2(14), 1062500 },
	{ 1, {  1890000, HFPLL, 1, 0x46 }, L2(14), 1100000 },
	{ 0, { 0 } }
};

static struct pvs_table pvs_tables[NUM_SPEED_BINS][NUM_PVS] __initdata = {
	[0][PVS_SLOW]    = {tbl_slow, sizeof(tbl_slow),     0 },
	[0][PVS_NOMINAL] = {tbl_nom,  sizeof(tbl_nom),  25000 },
	[0][PVS_FAST]    = {tbl_fast, sizeof(tbl_fast), 25000 },
	[0][PVS_FASTER]  = {tbl_faster, sizeof(tbl_faster), 25000 },

	[1][0] = { tbl_PVS0_1700MHz, sizeof(tbl_PVS0_1700MHz),     0 },
	[1][1] = { tbl_PVS1_1700MHz, sizeof(tbl_PVS1_1700MHz),     25000 },
	[1][2] = { tbl_PVS2_1700MHz, sizeof(tbl_PVS2_1700MHz),     25000 },
	[1][3] = { tbl_PVS3_1700MHz, sizeof(tbl_PVS3_1700MHz),     25000 },
	[1][4] = { tbl_PVS4_1700MHz, sizeof(tbl_PVS4_1700MHz),     25000 },
	[1][5] = { tbl_PVS5_1700MHz, sizeof(tbl_PVS5_1700MHz),     25000 },
	[1][6] = { tbl_PVS6_1700MHz, sizeof(tbl_PVS6_1700MHz),     25000 },

	[2][0] = { tbl_PVS0_2000MHz, sizeof(tbl_PVS0_2000MHz),     0 },
	[2][1] = { tbl_PVS1_2000MHz, sizeof(tbl_PVS1_2000MHz),     25000 },
	[2][2] = { tbl_PVS2_2000MHz, sizeof(tbl_PVS2_2000MHz),     25000 },
	[2][3] = { tbl_PVS3_2000MHz, sizeof(tbl_PVS3_2000MHz),     25000 },
	[2][4] = { tbl_PVS4_2000MHz, sizeof(tbl_PVS4_2000MHz),     25000 },
	[2][5] = { tbl_PVS5_2000MHz, sizeof(tbl_PVS5_2000MHz),     25000 },
	[2][6] = { tbl_PVS6_2000MHz, sizeof(tbl_PVS6_2000MHz),     25000 },

	[14][0] = { tbl_PVS0_1512MHz, sizeof(tbl_PVS0_1512MHz),     0 },
	[14][1] = { tbl_PVS1_1512MHz, sizeof(tbl_PVS1_1512MHz),     25000 },
	[14][2] = { tbl_PVS2_1512MHz, sizeof(tbl_PVS2_1512MHz),     25000 },
	[14][3] = { tbl_PVS3_1512MHz, sizeof(tbl_PVS3_1512MHz),     25000 },
	[14][4] = { tbl_PVS4_1512MHz, sizeof(tbl_PVS4_1512MHz),     25000 },
	[14][5] = { tbl_PVS5_1512MHz, sizeof(tbl_PVS5_1512MHz),     25000 },
	[14][6] = { tbl_PVS6_1512MHz, sizeof(tbl_PVS6_1512MHz),     25000 },
};

static struct acpuclk_krait_params acpuclk_8064_params __initdata = {
	.scalable = scalable,
	.scalable_size = sizeof(scalable),
	.hfpll_data = &hfpll_data,
	.pvs_tables = pvs_tables,
	.l2_freq_tbl = l2_freq_tbl,
	.l2_freq_tbl_size = sizeof(l2_freq_tbl),
	.bus_scale = &bus_scale_data,
	.pte_efuse_phys = 0x007000C0,
	.get_bin_info = get_krait_bin_format_a,
	.stby_khz = 384000,
};

static int __init acpuclk_8064_probe(struct platform_device *pdev)
{
	if (cpu_is_apq8064ab() ||
		SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2) {
		acpuclk_8064_params.hfpll_data->low_vdd_l_max = 37;
		acpuclk_8064_params.hfpll_data->nom_vdd_l_max = 74;
	}

	return acpuclk_krait_init(&pdev->dev, &acpuclk_8064_params);
}

static struct platform_driver acpuclk_8064_driver = {
	.driver = {
		.name = "acpuclk-8064",
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_8064_init(void)
{
	return platform_driver_probe(&acpuclk_8064_driver,
				     acpuclk_8064_probe);
}
device_initcall(acpuclk_8064_init);
