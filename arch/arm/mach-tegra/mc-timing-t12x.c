/*
 * arch/arm/mach-tegra/mc-timing-t12x.c
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/io.h>

#include <mach/mcerr.h>

#include "iomap.h"

#define MC_LA_REG(mod) MC_LATENCY_ALLOWANCE_ ## mod

static u32 mc_timing_reg_table[] = {
	MC_LA_REG(AFI_0),
	MC_LA_REG(AVPC_0),
	MC_LA_REG(DC_0),
	MC_LA_REG(DC_1),
	MC_LA_REG(DC_2),
	MC_LA_REG(DCB_0),
	MC_LA_REG(DCB_1),
	MC_LA_REG(DCB_2),
	MC_LA_REG(HC_0),
	MC_LA_REG(HC_1),
	MC_LA_REG(HDA_0),
	MC_LA_REG(MPCORE_0),
	MC_LA_REG(MPCORELP_0),
	MC_LA_REG(MSENC_0),
	MC_LA_REG(PPCS_0),
	MC_LA_REG(PPCS_1),
	MC_LA_REG(PTC_0),
	MC_LA_REG(SATA_0),
	MC_LA_REG(VDE_0),
	MC_LA_REG(VDE_1),
	MC_LA_REG(VDE_2),
	MC_LA_REG(VDE_3),
	MC_LA_REG(ISP2_0),
	MC_LA_REG(ISP2_1),
	MC_LA_REG(XUSB_0),
	MC_LA_REG(XUSB_1),
	MC_LA_REG(ISP2B_0),
	MC_LA_REG(ISP2B_1),
	MC_LA_REG(TSEC_0),
	MC_LA_REG(VIC_0),
	MC_LA_REG(VI2_0),
	MC_LA_REG(A9AVP_0),
	MC_LA_REG(GPU_0),
	MC_LA_REG(SDMMCA_0),
	MC_LA_REG(SDMMCAA_0),
	MC_LA_REG(SDMMC_0),
	MC_LA_REG(SDMMCAB_0),
	MC_LA_REG(DC_3)
};

#define NUM_LA_REGS ARRAY_SIZE(mc_timing_reg_table)

void tegra12_mc_latency_allowance_save(u32 **pctx)
{
	u32 *ctx = *pctx;
	u32 i;
	for (i = 0; i < NUM_LA_REGS; ++i)
		*ctx++ = readl(IOMEM(mc +
			mc_timing_reg_table[i]));
}

void tegra12_mc_latency_allowance_restore(u32 **pctx)
{
	u32 *ctx = *pctx;
	u32 i;
	for (i = 0; i < NUM_LA_REGS; ++i)
		__raw_writel(*ctx++, IOMEM(mc +
			mc_timing_reg_table[i]));
}
