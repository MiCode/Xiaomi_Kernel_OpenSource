/*
 * arch/arm/mach-tegra/tegra3_la.c
 *
 * Copyright (C) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/stringify.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <asm/io.h>
#include <mach/latency_allowance.h>

#include "iomap.h"
#include "la_priv.h"

#define T3_MC_LA_AFI_0		0x2e0
#define T3_MC_LA_AVPC_ARM7_0	0x2e4
#define T3_MC_LA_DC_0		0x2e8
#define T3_MC_LA_DC_1		0x2ec
#define T3_MC_LA_DC_2		0x2f0
#define T3_MC_LA_DCB_0		0x2f4
#define T3_MC_LA_DCB_1		0x2f8
#define T3_MC_LA_DCB_2		0x2fc
#define T3_MC_LA_EPP_0		0x300
#define T3_MC_LA_EPP_1		0x304
#define T3_MC_LA_G2_0		0x308
#define T3_MC_LA_G2_1		0x30c
#define T3_MC_LA_HC_0		0x310
#define T3_MC_LA_HC_1		0x314
#define T3_MC_LA_HDA_0		0x318
#define T3_MC_LA_ISP_0		0x31C
#define T3_MC_LA_MPCORE_0		0x320
#define T3_MC_LA_MPCORELP_0	0x324
#define T3_MC_LA_MPE_0		0x328
#define T3_MC_LA_MPE_1		0x32c
#define T3_MC_LA_MPE_2		0x330
#define T3_MC_LA_NV_0		0x334
#define T3_MC_LA_NV_1		0x338
#define T3_MC_LA_NV2_0		0x33c
#define T3_MC_LA_NV2_1		0x340
#define T3_MC_LA_PPCS_0		0x344
#define T3_MC_LA_PPCS_1		0x348
#define T3_MC_LA_PTC_0		0x34c
#define T3_MC_LA_SATA_0		0x350
#define T3_MC_LA_VDE_0		0x354
#define T3_MC_LA_VDE_1		0x358
#define T3_MC_LA_VDE_2		0x35c
#define T3_MC_LA_VDE_3		0x360
#define T3_MC_LA_VI_0		0x364
#define T3_MC_LA_VI_1		0x368
#define T3_MC_LA_VI_2		0x36c

#define T3_MC_ARB_OVERRIDE		0xe8
#define GLOBAL_LATENCY_SCALING_ENABLE_BIT 7

#define DS_DISP_MCCIF_DISPLAY0A_HYST (0x481 * 4)
#define DS_DISP_MCCIF_DISPLAY0B_HYST (0x482 * 4)
#define DS_DISP_MCCIF_DISPLAY0C_HYST (0x483 * 4)
#define DS_DISP_MCCIF_DISPLAY1B_HYST (0x484 * 4)

#define DS_DISP_MCCIF_DISPLAY0AB_HYST (0x481 * 4)
#define DS_DISP_MCCIF_DISPLAY0BB_HYST (0x482 * 4)
#define DS_DISP_MCCIF_DISPLAY0CB_HYST (0x483 * 4)
#define DS_DISP_MCCIF_DISPLAY1BB_HYST (0x484 * 4)

#define VI_MCCIF_VIWSB_HYST	(0x9a * 4)
#define VI_MCCIF_VIWU_HYST	(0x9b * 4)
#define VI_MCCIF_VIWV_HYST	(0x9c * 4)
#define VI_MCCIF_VIWY_HYST	(0x9d * 4)

#define VI_TIMEOUT_WOCAL_VI	(0x70 * 4)
#define VI_RESERVE_3		(0x97 * 4)
#define VI_RESERVE_4		(0x98 * 4)

/* maximum valid value for latency allowance */
#define T3_MC_LA_MAX_VALUE		255

#define T3_MC_RA(r) \
	(IO_ADDRESS(TEGRA_MC_BASE) + (T3_MC_##r))
#define T3_RA(r) \
	(IO_ADDRESS(TEGRA_MC_BASE) + (T3_MC_LA_##r))

#define T3_LA(f, e, a, r, i, ss, la) \
{ \
	.fifo_size_in_atoms = f, \
	.expiration_in_ns = e, \
	.reg_addr = T3_RA(a), \
	.mask = MASK(r), \
	.shift = SHIFT(r), \
	.id = ID(i), \
	.name = __stringify(i), \
	.scaling_supported = ss, \
	.init_la = la, \
}

/*
 * The consensus for getting the fifo_size_in_atoms is:
 * 1.If REORDER_DEPTH exists, use it(default is overridden).
 * 2.Else if (write_client) use RFIFO_DEPTH.
 * 3.Else (read client) use RDFIFO_DEPTH.
 * Multiply the value by 2 for wide clients.
 * A client is wide, if CMW is larger than MW.
 * Refer to project.h file.
 */
struct la_client_info t3_la_info_array[] = {
	T3_LA(32,	150,	AFI_0,	7 : 0,		AFIR,		false,	0),
	T3_LA(32,	150,	AFI_0,	23 : 16,	AFIW,		false,	0),
	T3_LA(2,	150,	AVPC_ARM7_0, 7 : 0,	AVPC_ARM7R,	false,	0),
	T3_LA(2,	150,	AVPC_ARM7_0, 23 : 16,	AVPC_ARM7W,	false,	0),
	T3_LA(128,	1050,	DC_0,	7 : 0,		DISPLAY_0A,	true,	0),
	T3_LA(64,	1050,	DC_0,	23 : 16,	DISPLAY_0B,	true,	0),
	T3_LA(128,	1050,	DC_1,	7 : 0,		DISPLAY_0C,	true,	0),
	T3_LA(64,	1050,	DC_1,	23 : 16,	DISPLAY_1B,	true,	0),
	T3_LA(2,	1050,	DC_2,	7 : 0,		DISPLAY_HC,	false,	0),
	T3_LA(128,	1050,	DCB_0,	7 : 0,		DISPLAY_0AB,	true,	0),
	T3_LA(64,	1050,	DCB_0,	23 : 16,	DISPLAY_0BB,	true,	0),
	T3_LA(128,	1050,	DCB_1,	7 : 0,		DISPLAY_0CB,	true,	0),
	T3_LA(64,	1050,	DCB_1,	23 : 16,	DISPLAY_1BB,	true,	0),
	T3_LA(2,	1050,	DCB_2,	7 : 0,		DISPLAY_HCB,	false,	0),
	T3_LA(8,	150,	EPP_0,	7 : 0,		EPPUP,		false,	0),
	T3_LA(64,	150,	EPP_0,	23 : 16,	EPPU,		false,	0),
	T3_LA(64,	150,	EPP_1,	7 : 0,		EPPV,		false,	0),
	T3_LA(64,	150,	EPP_1,	23 : 16,	EPPY,		false,	0),
	T3_LA(64,	150,	G2_0,	7 : 0,		G2PR,		false,	0),
	T3_LA(64,	150,	G2_0,	23 : 16,	G2SR,		false,	0),
	T3_LA(48,	150,	G2_1,	7 : 0,		G2DR,		false,	0),
	T3_LA(128,	150,	G2_1,	23 : 16,	G2DW,		false,	0),
	T3_LA(16,	150,	HC_0,	7 : 0,		HOST1X_DMAR,	false,	0),
	T3_LA(8,	150,	HC_0,	23 : 16,	HOST1XR,	false,	0),
	T3_LA(32,	150,	HC_1,	7 : 0,		HOST1XW,	false,	0),
	T3_LA(16,	150,	HDA_0,	7 : 0,		HDAR,		false,	0),
	T3_LA(16,	150,	HDA_0,	23 : 16,	HDAW,		false,	0),
	T3_LA(64,	150,	ISP_0,	7 : 0,		ISPW,		false,	0),
	T3_LA(14,	150,	MPCORE_0, 7 : 0,	MPCORER,	false,	0),
	T3_LA(24,	150,	MPCORE_0, 23 : 16,	MPCOREW,	false,	0),
	T3_LA(14,	150,	MPCORELP_0, 7 : 0,	MPCORE_LPR,	false,	0),
	T3_LA(24,	150,	MPCORELP_0, 23 : 16,	MPCORE_LPW,	false,	0),
	T3_LA(8,	150,	MPE_0,	7 : 0,		MPE_UNIFBR,	false,	0),
	T3_LA(2,	150,	MPE_0,	23 : 16,	MPE_IPRED,	false,	0),
	T3_LA(64,	150,	MPE_1,	7 : 0,		MPE_AMEMRD,	false,	0),
	T3_LA(8,	150,	MPE_1,	23 : 16,	MPE_CSRD,	false,	0),
	T3_LA(8,	150,	MPE_2,	7 : 0,		MPE_UNIFBW,	false,	0),
	T3_LA(8,	150,	MPE_2,	23 : 16,	MPE_CSWR,	false,	0),
	T3_LA(96,	150,	NV_0,	7 : 0,		FDCDRD,		false,	0),
	T3_LA(64,	150,	NV_0,	23 : 16,	IDXSRD,		false,	0),
	T3_LA(64,	150,	NV_1,	7 : 0,		TEXSRD,		false,	0),
	T3_LA(96,	150,	NV_1,	23 : 16,	FDCDWR,		false,	0),
	T3_LA(96,	150,	NV2_0,	7 : 0,		FDCDRD2,	false,	0),
	T3_LA(64,	150,	NV2_0,	23 : 16,	IDXSRD2,	false,	0),
	T3_LA(64,	150,	NV2_1,	7 : 0,		TEXSRD2,	false,	0),
	T3_LA(96,	150,	NV2_1,	23 : 16,	FDCDWR2,	false,	0),
	T3_LA(2,	150,	PPCS_0,	7 : 0,		PPCS_AHBDMAR,	false,	0),
	T3_LA(8,	150,	PPCS_0,	23 : 16,	PPCS_AHBSLVR,	false,	0),
	T3_LA(2,	150,	PPCS_1,	7 : 0,		PPCS_AHBDMAW,	false,	0),
	T3_LA(4,	150,	PPCS_1,	23 : 16,	PPCS_AHBSLVW,	false,	0),
	T3_LA(2,	150,	PTC_0,	7 : 0,		PTCR,		false,	0),
	T3_LA(32,	150,	SATA_0,	7 : 0,		SATAR,		false,	0),
	T3_LA(32,	150,	SATA_0,	23 : 16,	SATAW,		false,	0),
	T3_LA(8,	150,	VDE_0,	7 : 0,		VDE_BSEVR,	false,	0),
	T3_LA(4,	150,	VDE_0,	23 : 16,	VDE_MBER,	false,	0),
	T3_LA(16,	150,	VDE_1,	7 : 0,		VDE_MCER,	false,	0),
	T3_LA(16,	150,	VDE_1,	23 : 16,	VDE_TPER,	false,	0),
	T3_LA(4,	150,	VDE_2,	7 : 0,		VDE_BSEVW,	false,	0),
	T3_LA(16,	150,	VDE_2,	23 : 16,	VDE_DBGW,	false,	0),
	T3_LA(2,	150,	VDE_3,	7 : 0,		VDE_MBEW,	false,	0),
	T3_LA(16,	150,	VDE_3,	23 : 16,	VDE_TPMW,	false,	0),
	T3_LA(8,	1050,	VI_0,	7 : 0,		VI_RUV,		false,	0),
	T3_LA(64,	1050,	VI_0,	23 : 16,	VI_WSB,		true,	0),
	T3_LA(64,	1050,	VI_1,	7 : 0,		VI_WU,		true,	0),
	T3_LA(64,	1050,	VI_1,	23 : 16,	VI_WV,		true,	0),
	T3_LA(64,	1050,	VI_2,	7 : 0,		VI_WY,		true,	0),

/* end of list. */
	T3_LA(0,	0,	AFI_0,	0 : 0,		MAX_ID,		false,	0)
};

#define DISP1_RA(r) \
	(IO_ADDRESS(TEGRA_DISPLAY_BASE) + DS_DISP_MCCIF_##r##_HYST)
#define DISP2_RA(r) \
	(IO_ADDRESS(TEGRA_DISPLAY2_BASE) + DS_DISP_MCCIF_##r##_HYST)

#define DISP_SCALING_REG_INFO(id, r, ra) \
	{ \
		ID(id), \
		ra(r), MASK(15 : 8), SHIFT(15 : 8), \
		ra(r), MASK(23 : 16), SHIFT(15 : 8), \
		ra(r), MASK(7 : 0), SHIFT(15 : 8) \
	}

struct la_scaling_reg_info disp_info[] = {
	DISP_SCALING_REG_INFO(DISPLAY_0A, DISPLAY0A, DISP1_RA),
	DISP_SCALING_REG_INFO(DISPLAY_0B, DISPLAY0B, DISP1_RA),
	DISP_SCALING_REG_INFO(DISPLAY_0C, DISPLAY0C, DISP1_RA),
	DISP_SCALING_REG_INFO(DISPLAY_1B, DISPLAY1B, DISP1_RA),
	DISP_SCALING_REG_INFO(MAX_ID,     DISPLAY1B, DISP1_RA), /*dummy entry*/
	DISP_SCALING_REG_INFO(DISPLAY_0AB, DISPLAY0AB, DISP2_RA),
	DISP_SCALING_REG_INFO(DISPLAY_0BB, DISPLAY0BB, DISP2_RA),
	DISP_SCALING_REG_INFO(DISPLAY_0CB, DISPLAY0CB, DISP2_RA),
	DISP_SCALING_REG_INFO(DISPLAY_1BB, DISPLAY1BB, DISP2_RA),
};

#define VI_TH_RA(r) \
	(IO_ADDRESS(TEGRA_VI_BASE) + VI_MCCIF_##r##_HYST)
#define VI_TM_RA(r) \
	(IO_ADDRESS(TEGRA_VI_BASE) + VI_TIMEOUT_WOCAL_VI)
#define VI_TL_RA(r) \
	(IO_ADDRESS(TEGRA_VI_BASE) + VI_RESERVE_##r)

struct la_scaling_reg_info vi_info[] = {
	{
		ID(VI_WSB),
		VI_TL_RA(4), MASK(7 : 0), SHIFT(7 : 0),
		VI_TM_RA(0), MASK(7 : 0), SHIFT(7 : 0),
		VI_TH_RA(VIWSB), MASK(7 : 0), SHIFT(7 : 0)
	},
	{
		ID(VI_WU),
		VI_TL_RA(3), MASK(15 : 8), SHIFT(15 : 8),
		VI_TM_RA(0), MASK(15 : 8), SHIFT(15 : 8),
		VI_TH_RA(VIWU), MASK(7 : 0), SHIFT(7 : 0)
	},
	{
		ID(VI_WV),
		VI_TL_RA(3), MASK(7 : 0), SHIFT(7 : 0),
		VI_TM_RA(0), MASK(23 : 16), SHIFT(23 : 16),
		VI_TH_RA(VIWV), MASK(7 : 0), SHIFT(7 : 0)
	},
	{
		ID(VI_WY),
		VI_TL_RA(4), MASK(15 : 8), SHIFT(15 : 8),
		VI_TM_RA(0), MASK(31 : 24), SHIFT(31 : 24),
		VI_TH_RA(VIWY), MASK(7 : 0), SHIFT(7 : 0)
	}
};

static struct la_chip_specific *cs;

static void set_thresholds(struct la_scaling_reg_info *info,
			    enum tegra_la_id id)
{
	unsigned long reg_read;
	unsigned long reg_write;
	unsigned int thresh_low;
	unsigned int thresh_mid;
	unsigned int thresh_high;
	int la_set;
	int idx = cs->id_to_index[id];

	reg_read = readl(cs->la_info_array[idx].reg_addr);
	la_set = (reg_read & cs->la_info_array[idx].mask) >>
		 cs->la_info_array[idx].shift;
	/* la should be set before enabling scaling. */
	BUG_ON(la_set != cs->scaling_info[idx].la_set);

	thresh_low = (cs->scaling_info[idx].threshold_low * la_set) / 100;
	thresh_mid = (cs->scaling_info[idx].threshold_mid * la_set) / 100;
	thresh_high = (cs->scaling_info[idx].threshold_high * la_set) / 100;
	la_debug("%s: la_set=%d, thresh_low=%d(%d%%), thresh_mid=%d(%d%%),"
		" thresh_high=%d(%d%%) ", __func__, la_set,
		thresh_low, cs->scaling_info[idx].threshold_low,
		thresh_mid, cs->scaling_info[idx].threshold_mid,
		thresh_high, cs->scaling_info[idx].threshold_high);

	reg_read = readl(info->tl_reg_addr);
	reg_write = (reg_read & ~info->tl_mask) |
		(thresh_low << info->tl_shift);
	writel(reg_write, info->tl_reg_addr);
	la_debug("reg_addr=0x%x, read=0x%x, write=0x%x",
		(u32)info->tl_reg_addr, (u32)reg_read, (u32)reg_write);

	reg_read = readl(info->tm_reg_addr);
	reg_write = (reg_read & ~info->tm_mask) |
		(thresh_mid << info->tm_shift);
	writel(reg_write, info->tm_reg_addr);
	la_debug("reg_addr=0x%x, read=0x%x, write=0x%x",
		(u32)info->tm_reg_addr, (u32)reg_read, (u32)reg_write);

	reg_read = readl(info->th_reg_addr);
	reg_write = (reg_read & ~info->th_mask) |
		(thresh_high << info->th_shift);
	writel(reg_write, info->th_reg_addr);
	la_debug("reg_addr=0x%x, read=0x%x, write=0x%x",
		(u32)info->th_reg_addr, (u32)reg_read, (u32)reg_write);
}

static void set_disp_latency_thresholds(enum tegra_la_id id)
{
	set_thresholds(&disp_info[id - ID(DISPLAY_0A)], id);
}

static void set_vi_latency_thresholds(enum tegra_la_id id)
{
	set_thresholds(&vi_info[id - ID(VI_WSB)], id);
}

/* Thresholds for scaling are specified in % of fifo freeness.
 * If threshold_low is specified as 20%, it means when the fifo free
 * between 0 to 20%, use la as programmed_la.
 * If threshold_mid is specified as 50%, it means when the fifo free
 * between 20 to 50%, use la as programmed_la/2 .
 * If threshold_high is specified as 80%, it means when the fifo free
 * between 50 to 80%, use la as programmed_la/4.
 * When the fifo is free between 80 to 100%, use la as 0(highest priority).
 */
int t3_enable_la_scaling(enum tegra_la_id id,
				    unsigned int threshold_low,
				    unsigned int threshold_mid,
				    unsigned int threshold_high)
{
	unsigned long reg;
	void __iomem *scaling_enable_reg =
				(void __iomem *)(T3_MC_RA(ARB_OVERRIDE));
	int idx = cs->id_to_index[id];

	VALIDATE_ID(id, cs);
	VALIDATE_THRESHOLDS(threshold_low, threshold_mid, threshold_high);

	if (cs->la_info_array[idx].scaling_supported == false)
		goto exit;

	spin_lock(&cs->lock);

	la_debug("\n%s: id=%d, tl=%d, tm=%d, th=%d", __func__,
		id, threshold_low, threshold_mid, threshold_high);
	cs->scaling_info[idx].threshold_low = threshold_low;
	cs->scaling_info[idx].threshold_mid = threshold_mid;
	cs->scaling_info[idx].threshold_high = threshold_high;
	cs->scaling_info[idx].scaling_ref_count++;

	if (id >= ID(DISPLAY_0A) && id <= ID(DISPLAY_1BB))
		set_disp_latency_thresholds(id);
	else if (id >= ID(VI_WSB) && id <= ID(VI_WY))
		set_vi_latency_thresholds(id);
	if (!cs->la_scaling_enable_count++) {
		reg = readl(scaling_enable_reg);
		reg |= (1 << GLOBAL_LATENCY_SCALING_ENABLE_BIT);
		writel(reg,  scaling_enable_reg);
		la_debug("enabled scaling.");
	}
	spin_unlock(&cs->lock);
exit:
	return 0;
}

void t3_disable_la_scaling(enum tegra_la_id id)
{
	unsigned long reg;
	void __iomem *scaling_enable_reg =
				(void __iomem *)(T3_MC_RA(ARB_OVERRIDE));
	int idx;

	BUG_ON(id >= TEGRA_LA_MAX_ID);
	idx = cs->id_to_index[id];
	BUG_ON(cs->la_info_array[idx].id != id);

	if (cs->la_info_array[idx].scaling_supported == false)
		return;
	spin_lock(&cs->lock);
	la_debug("\n%s: id=%d", __func__, id);
	cs->scaling_info[idx].scaling_ref_count--;
	BUG_ON(cs->scaling_info[idx].scaling_ref_count < 0);

	if (!--cs->la_scaling_enable_count) {
		reg = readl(scaling_enable_reg);
		reg = reg & ~(1 << GLOBAL_LATENCY_SCALING_ENABLE_BIT);
		writel(reg, scaling_enable_reg);
		la_debug("disabled scaling.");
	}
	spin_unlock(&cs->lock);
}

void tegra_la_get_t3_specific(struct la_chip_specific *cs_la)
{
	cs_la->ns_per_tick = 30;
	cs_la->atom_size = 16;
	cs_la->la_max_value = T3_MC_LA_MAX_VALUE;
	cs_la->la_info_array = t3_la_info_array;
	cs_la->la_info_array_size = ARRAY_SIZE(t3_la_info_array);
	cs_la->enable_la_scaling = t3_enable_la_scaling;
	cs_la->disable_la_scaling = t3_disable_la_scaling;
	cs = cs_la;
}
