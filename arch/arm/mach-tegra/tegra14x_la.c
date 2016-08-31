/*
 * arch/arm/mach-tegra/tegra14x_la.c
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

#define T14X_MC_LA_AVPC_ARM7_0	0x2e4
#define T14X_MC_LA_DC_0		0x2e8
#define T14X_MC_LA_DC_1		0x2ec
#define T14X_MC_LA_DC_2		0x2f0
#define T14X_MC_LA_DCB_0		0x2f4
#define T14X_MC_LA_DCB_1		0x2f8
#define T14X_MC_LA_DCB_2		0x2fc
#define T14X_MC_LA_EPP_0		0x300
#define T14X_MC_LA_EPP_1		0x304
#define T14X_MC_LA_G2_0		0x308
#define T14X_MC_LA_G2_1		0x30c
#define T14X_MC_LA_HC_0		0x310
#define T14X_MC_LA_HC_1		0x314
#define T14X_MC_LA_HDA_0		0x318
#define T14X_MC_LA_ISP_0		0x31C
#define T14X_MC_LA_MPCORE_0		0x320
#define T14X_MC_LA_MPCORELP_0	0x324
#define T14X_MC_LA_MSENC_0		0x328
#define T14X_MC_LA_NV_0		0x334
#define T14X_MC_LA_NV_1		0x338
#define T14X_MC_LA_NV2_0		0x33c
#define T14X_MC_LA_NV2_1		0x340
#define T14X_MC_LA_PPCS_0		0x344
#define T14X_MC_LA_PPCS_1		0x348
#define T14X_MC_LA_PTC_0		0x34c

#define T14X_MC_LA_VDE_0		0x354
#define T14X_MC_LA_VDE_1		0x358
#define T14X_MC_LA_VDE_2		0x35c
#define T14X_MC_LA_VDE_3		0x360
#define T14X_MC_LA_VI_0		0x364
#define T14X_MC_LA_VI_1		0x368
#define T14X_MC_LA_VI_2		0x36c
#define T14X_MC_LA_ISP2_0		0x370 /* T14x specific*/
#define T14X_MC_LA_ISP2_1		0x374 /* T14x specific*/

#define T14X_MC_LA_EMUCIF_0		0x38c
#define T14X_MC_LA_TSEC_0		0x390

#define T14X_MC_LA_BBMCI_0		0x394 /* T14x specific*/
#define T14X_MC_LA_BBMCILL_0		0x398 /* T14x specific*/
#define T14X_MC_LA_DC_3		0x39c /* T14x specific*/

#define T14X_MC_DIS_PTSA_RATE_0			0x41c
#define T14X_MC_DIS_PTSA_MIN_0			0x420
#define T14X_MC_DIS_PTSA_MAX_0			0x424
#define T14X_MC_DISB_PTSA_RATE_0		0x428
#define T14X_MC_DISB_PTSA_MIN_0			0x42c
#define T14X_MC_DISB_PTSA_MAX_0			0x430
#define T14X_MC_VE_PTSA_RATE_0			0x434
#define T14X_MC_VE_PTSA_MIN_0			0x438
#define T14X_MC_VE_PTSA_MAX_0			0x43c
#define T14X_MC_RING2_PTSA_RATE_0		0x440
#define T14X_MC_RING2_PTSA_MIN_0		0x444
#define T14X_MC_RING2_PTSA_MAX_0		0x448
#define T14X_MC_MLL_MPCORER_PTSA_RATE_0		0x44c
#define T14X_MC_MLL_MPCORER_PTSA_MIN_0		0x450
#define T14X_MC_MLL_MPCORER_PTSA_MAX_0		0x454
#define T14X_MC_SMMU_SMMU_PTSA_RATE_0		0x458
#define T14X_MC_SMMU_SMMU_PTSA_MIN_0		0x45c
#define T14X_MC_SMMU_SMMU_PTSA_MAX_0		0x460
#define T14X_MC_R0_DIS_PTSA_RATE_0		0x464
#define T14X_MC_R0_DIS_PTSA_MIN_0		0x468
#define T14X_MC_R0_DIS_PTSA_MAX_0		0x46c
#define T14X_MC_R0_DISB_PTSA_RATE_0		0x470
#define T14X_MC_R0_DISB_PTSA_MIN_0		0x474
#define T14X_MC_R0_DISB_PTSA_MAX_0		0x478
#define T14X_MC_RING1_PTSA_RATE_0		0x47c
#define T14X_MC_RING1_PTSA_MIN_0		0x480
#define T14X_MC_RING1_PTSA_MAX_0		0x484

#define T14X_MC_BBC_PTSA_RATE_0			0x4d0
#define T14X_MC_BBC_PTSA_MIN_0			0x4d4
#define T14X_MC_BBC_PTSA_MAX_0			0x4d8

#define T14X_MC_SCALED_LA_DISPLAY0A_0		0x690
#define T14X_MC_SCALED_LA_DISPLAY0B_0		0x698
#define T14X_MC_SCALED_LA_DISPLAY0BB_0		0x69c
#define T14X_MC_SCALED_LA_DISPLAY0C_0		0x6a0


#define T14X_MC_DIS_EXTRA_SNAP_LEVELS_0		0x2ac
#define T14X_MC_HEG_EXTRA_SNAP_LEVELS_0		0x2b0
#define T14X_MC_EMEM_ARB_MISC0_0		0x0d8
#define T14X_MC_TIMING_CONTROL_0		0xfc
#define T14X_MC_PTSA_GRANT_DECREMENT_0		0x960

#define T14X_MC_BBCLL_EARB_CFG_0		0x080

#define T14X_BASE_EMC_FREQ_MHZ		800
#define T14X_MAX_CAMERA_BW_MHZ		528
#define T14X_MAX_BBCDMA_BW_MHZ		1200
#define T14X_MAX_BBCLL_BW_MHZ		640

/* maximum valid value for latency allowance */
#define T14X_MC_LA_MAX_VALUE		255

#define T14X_MC_RA(r) \
	(IO_ADDRESS(TEGRA_MC_BASE) + (T14X_MC_##r))
#define T14X_RA(r) \
	(IO_ADDRESS(TEGRA_MC_BASE) + (T14X_MC_LA_##r))

#define T14X_LA(f, e, a, r, i, ss, la) \
{ \
	.fifo_size_in_atoms = f, \
	.expiration_in_ns = e, \
	.reg_addr = T14X_RA(a), \
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
 * Multiply the value by 2 for dual channel.
 * Multiply the value by 2 for wide clients.
 * A client is wide, if CMW is larger than MW.
 * Refer to project.h file.
 */
struct la_client_info t14x_la_info_array[] = {
	T14X_LA(4,	150,	AVPC_ARM7_0, 7 : 0,	AVPC_ARM7R,	false,	0),
	T14X_LA(8,	150,	AVPC_ARM7_0, 23 : 16,	AVPC_ARM7W,	false,	0),
	T14X_LA(128,	1050,	DC_0,	7 : 0,		DISPLAY_0A,	true,	0),
	T14X_LA(128,	1050,	DC_0,	23 : 16,	DISPLAY_0B,	true,	0),
	T14X_LA(128,	1050,	DC_1,	7 : 0,		DISPLAY_0C,	true,	0),
	T14X_LA(192,	1050,	DC_2,	7 : 0,		DISPLAY_HC,	false,	0),
	T14X_LA(128,	1920,	DCB_0,	7 : 0,		DISPLAY_0AB,	true,	0),
	T14X_LA(128,	1050,	DCB_0,	23 : 16,	DISPLAY_0BB,	true,	0),
	T14X_LA(128,	1920,	DCB_1,	7 : 0,		DISPLAY_0CB,	true,	0),
	T14X_LA(192,	1050,	DCB_2,	7 : 0,		DISPLAY_HCB,	false,	0),
	T14X_LA(192,	1920,	DC_2,	23 : 16,	DISPLAY_T,	false,	0),
	T14X_LA(192,	1920,	DC_3,	7 : 0,		DISPLAYD,	false,	0),
	T14X_LA(32,	150,	EPP_0,	7 : 0,		EPPUP,		false,	0),
	T14X_LA(32,	150,	EPP_0,	23 : 16,	EPPU,		false,	0),
	T14X_LA(32,	150,	EPP_1,	7 : 0,		EPPV,		false,	0),
	T14X_LA(32,	150,	EPP_1,	23 : 16,	EPPY,		false,	0),
	T14X_LA(128,	150,	G2_0,	7 : 0,		G2PR,		false,	35),
	T14X_LA(128,	150,	G2_0,	23 : 16,	G2SR,		false,	21),
	T14X_LA(72,	150,	G2_1,	7 : 0,		G2DR,		false,	24),
	T14X_LA(128,	150,	G2_1,	23 : 16,	G2DW,		false,	9),
	T14X_LA(16,	150,	HC_0,	7 : 0,		HOST1X_DMAR,	false,	0),
	T14X_LA(8,	150,	HC_0,	23 : 16,	HOST1XR,	false,	0),
	T14X_LA(32,	150,	HC_1,	7 : 0,		HOST1XW,	false,	0),
	T14X_LA(64,	150,	HDA_0,	7 : 0,		HDAR,		false,	0),
	T14X_LA(64,	150,	HDA_0,	23 : 16,	HDAW,		false,	0),
	T14X_LA(64,	150,	ISP_0,	7 : 0,		ISPW,		false,	0),
	T14X_LA(40,	150,	MPCORE_0,   7 : 0,	MPCORER,	false,	0),
	T14X_LA(42,	150,	MPCORE_0,   23 : 16,	MPCOREW,	false,	0),
	T14X_LA(24,	150,	MPCORELP_0, 7 : 0,	MPCORE_LPR,	false,	0),
	T14X_LA(24,	150,	MPCORELP_0, 23 : 16,	MPCORE_LPW,	false,	0),
	T14X_LA(160,	150,	NV_0,	7 : 0,		FDCDRD,		false,	30),
	T14X_LA(256,	150,	NV_0,	23 : 16,	IDXSRD,		false,	23),
	T14X_LA(256,	150,	NV_1,	7 : 0,		TEXL2SRD,	false,	23),
	T14X_LA(128,	150,	NV_1,	23 : 16,	FDCDWR,		false,	23),
	T14X_LA(160,	150,	NV2_0,	7 : 0,		FDCDRD2,	false,	30),
	T14X_LA(128,	150,	NV2_1,	7 : 0,		FDCDWR2,	false,	23),
	T14X_LA(4,	150,	PPCS_0,	7 : 0,		PPCS_AHBDMAR,	false,	0),
	T14X_LA(29,	150,	PPCS_0,	23 : 16,	PPCS_AHBSLVR,	false,	0),
	T14X_LA(8,	150,	PPCS_1,	7 : 0,		PPCS_AHBDMAW,	false,	0),
	T14X_LA(8,	150,	PPCS_1,	23 : 16,	PPCS_AHBSLVW,	false,	0),
	T14X_LA(20,	150,	PTC_0,	7 : 0,		PTCR,		false,	0),
	T14X_LA(8,	150,	VDE_0,	7 : 0,		VDE_BSEVR,	false,	28),
	T14X_LA(9,	150,	VDE_0,	23 : 16,	VDE_MBER,	false,	59),
	T14X_LA(64,	150,	VDE_1,	7 : 0,		VDE_MCER,	false,	27),
	T14X_LA(32,	150,	VDE_1,	23 : 16,	VDE_TPER,	false,	110),
	T14X_LA(4,	150,	VDE_2,	7 : 0,		VDE_BSEVW,	false,	255),
	T14X_LA(4,	150,	VDE_2,	23 : 16,	VDE_DBGW,	false,	255),
	T14X_LA(8,	150,	VDE_3,	7 : 0,		VDE_MBEW,	false,	132),
	T14X_LA(32,	150,	VDE_3,	23 : 16,	VDE_TPMW,	false,	38),
	T14X_LA(200,	1050,	VI_0,	7 : 0,		VI_WSB,		true,	0),
	T14X_LA(200,	1050,	VI_1,	7 : 0,		VI_WU,		true,	0),
	T14X_LA(200,	1050,	VI_1,	23 : 16,	VI_WV,		true,	0),
	T14X_LA(200,	1050,	VI_2,	7 : 0,		VI_WY,		true,	0),

	T14X_LA(64,	150,	MSENC_0,7 : 0,		MSENCSRD,	false,	50),
	T14X_LA(16,	150,	MSENC_0,23 : 16,	MSENCSWR,	false,	37),
	T14X_LA(14,	150,	EMUCIF_0,7 : 0,		EMUCIFR,	false,	0),
	T14X_LA(24,	150,	EMUCIF_0,23 : 16,	EMUCIFW,	false,	0),
	T14X_LA(32,	150,	TSEC_0,	7 : 0,		TSECSRD,	false,	0),
	T14X_LA(32,	150,	TSEC_0,	23 : 16,	TSECSWR,	false,	0),

	T14X_LA(125,	150,	VI_0,	23 : 16,	VI_W,		false,	0),
	T14X_LA(75,	150,	ISP2_0,	7 : 0,		ISP_RA,		false,	0),
	T14X_LA(150,	150,	ISP2_1,	7 : 0,		ISP_WA,		false,	0),
	T14X_LA(64,	150,	ISP2_1,	23 : 16,	ISP_WB,		false,	0),
	T14X_LA(32,	150,	BBMCI_0,7 : 0,		BBCR,		false,	8),
	T14X_LA(32,	150,	BBMCI_0,23 : 16,	BBCW,		false,	8),
	T14X_LA(16,	150,	BBMCILL_0,7 : 0,	BBCLLR,		false,	1),

/* end of list. */
	T14X_LA(0,	0,	DC_3,	0 : 0,		MAX_ID,		false,	0)
};

static struct la_chip_specific *cs;

static unsigned int t14x_get_ptsa_rate(unsigned int bw)
{
	// 8 = (1 channels) * (2 ddr) * (4 bytes)
	// T148DIFF - T114 code was wrong - hopefully this is right for T148
	unsigned base_memory_bw = 8 * T14X_BASE_EMC_FREQ_MHZ;
	// 281 = 256 * 1.1 (1.1 is the extra margin for ISO clients)
	unsigned rate = 281 * bw / base_memory_bw;
	if (rate > 255)
		rate = 255;
	return rate;
}

static void program_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;

	writel(p->ptsa_grant_dec, T14X_MC_RA(PTSA_GRANT_DECREMENT_0));
	writel(1, T14X_MC_RA(TIMING_CONTROL_0));

	writel(p->dis_ptsa_rate, T14X_MC_RA(DIS_PTSA_RATE_0));
	writel(p->dis_ptsa_min, T14X_MC_RA(DIS_PTSA_MIN_0));
	writel(p->dis_ptsa_max, T14X_MC_RA(DIS_PTSA_MAX_0));

	writel(p->disb_ptsa_rate, T14X_MC_RA(DISB_PTSA_RATE_0));
	writel(p->disb_ptsa_min, T14X_MC_RA(DISB_PTSA_MIN_0));
	writel(p->disb_ptsa_max, T14X_MC_RA(DISB_PTSA_MAX_0));

	writel(p->ve_ptsa_rate, T14X_MC_RA(VE_PTSA_RATE_0));
	writel(p->ve_ptsa_min, T14X_MC_RA(VE_PTSA_MIN_0));
	writel(p->ve_ptsa_max, T14X_MC_RA(VE_PTSA_MAX_0));

	writel(p->ring2_ptsa_rate, T14X_MC_RA(RING2_PTSA_RATE_0));
	writel(p->ring2_ptsa_min, T14X_MC_RA(RING2_PTSA_MIN_0));
	writel(p->ring2_ptsa_max, T14X_MC_RA(RING2_PTSA_MAX_0));

	writel(p->bbc_ptsa_rate, T14X_MC_RA(BBC_PTSA_RATE_0));
	writel(p->bbc_ptsa_min, T14X_MC_RA(BBC_PTSA_MIN_0));
	writel(p->bbc_ptsa_max, T14X_MC_RA(BBC_PTSA_MAX_0));

	writel(p->bbcll_earb_cfg, T14X_MC_RA(BBCLL_EARB_CFG_0));

	writel(p->mpcorer_ptsa_rate, T14X_MC_RA(MLL_MPCORER_PTSA_RATE_0));
	writel(p->mpcorer_ptsa_min, T14X_MC_RA(MLL_MPCORER_PTSA_MIN_0));
	writel(p->mpcorer_ptsa_max, T14X_MC_RA(MLL_MPCORER_PTSA_MAX_0));

	writel(p->smmu_ptsa_rate, T14X_MC_RA(SMMU_SMMU_PTSA_RATE_0));
	writel(p->smmu_ptsa_min, T14X_MC_RA(SMMU_SMMU_PTSA_MIN_0));
	writel(p->smmu_ptsa_max, T14X_MC_RA(SMMU_SMMU_PTSA_MAX_0));

	writel(p->ring1_ptsa_rate, T14X_MC_RA(RING1_PTSA_RATE_0));
	writel(p->ring1_ptsa_min, T14X_MC_RA(RING1_PTSA_MIN_0));
	writel(p->ring1_ptsa_max, T14X_MC_RA(RING1_PTSA_MAX_0));

	writel(p->dis_extra_snap_level, T14X_MC_RA(DIS_EXTRA_SNAP_LEVELS_0));
	writel(p->heg_extra_snap_level, T14X_MC_RA(HEG_EXTRA_SNAP_LEVELS_0));
}

static void save_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;

	p->ptsa_grant_dec = readl(T14X_MC_RA(PTSA_GRANT_DECREMENT_0));

	p->dis_ptsa_rate = readl(T14X_MC_RA(DIS_PTSA_RATE_0));
	p->dis_ptsa_min = readl(T14X_MC_RA(DIS_PTSA_MIN_0));
	p->dis_ptsa_max = readl(T14X_MC_RA(DIS_PTSA_MAX_0));

	p->disb_ptsa_rate = readl(T14X_MC_RA(DISB_PTSA_RATE_0));
	p->disb_ptsa_min = readl(T14X_MC_RA(DISB_PTSA_MIN_0));
	p->disb_ptsa_max = readl(T14X_MC_RA(DISB_PTSA_MAX_0));

	p->ve_ptsa_rate = readl(T14X_MC_RA(VE_PTSA_RATE_0));
	p->ve_ptsa_min = readl(T14X_MC_RA(VE_PTSA_MIN_0));
	p->ve_ptsa_max = readl(T14X_MC_RA(VE_PTSA_MAX_0));

	p->ring2_ptsa_rate = readl(T14X_MC_RA(RING2_PTSA_RATE_0));
	p->ring2_ptsa_min = readl(T14X_MC_RA(RING2_PTSA_MIN_0));
	p->ring2_ptsa_max = readl(T14X_MC_RA(RING2_PTSA_MAX_0));

	p->bbc_ptsa_rate = readl(T14X_MC_RA(BBC_PTSA_RATE_0));
	p->bbc_ptsa_min = readl(T14X_MC_RA(BBC_PTSA_MIN_0));
	p->bbc_ptsa_max = readl(T14X_MC_RA(BBC_PTSA_MAX_0));

	p->bbcll_earb_cfg = readl(T14X_MC_RA(BBCLL_EARB_CFG_0));

	p->mpcorer_ptsa_rate = readl(T14X_MC_RA(MLL_MPCORER_PTSA_RATE_0));
	p->mpcorer_ptsa_min = readl(T14X_MC_RA(MLL_MPCORER_PTSA_MIN_0));
	p->mpcorer_ptsa_max = readl(T14X_MC_RA(MLL_MPCORER_PTSA_MAX_0));

	p->smmu_ptsa_rate = readl(T14X_MC_RA(SMMU_SMMU_PTSA_RATE_0));
	p->smmu_ptsa_min = readl(T14X_MC_RA(SMMU_SMMU_PTSA_MIN_0));
	p->smmu_ptsa_max = readl(T14X_MC_RA(SMMU_SMMU_PTSA_MAX_0));

	p->ring1_ptsa_rate = readl(T14X_MC_RA(RING1_PTSA_RATE_0));
	p->ring1_ptsa_min = readl(T14X_MC_RA(RING1_PTSA_MIN_0));
	p->ring1_ptsa_max = readl(T14X_MC_RA(RING1_PTSA_MAX_0));

	p->dis_extra_snap_level = readl(T14X_MC_RA(DIS_EXTRA_SNAP_LEVELS_0));
	p->heg_extra_snap_level = readl(T14X_MC_RA(HEG_EXTRA_SNAP_LEVELS_0));
}

static void program_ring1_ptsa(struct ptsa_info *p)
{
	p->ring1_ptsa_rate = p->dis_ptsa_rate +
			     p->bbc_ptsa_rate;
#if defined(CONFIG_TEGRA_ERRATA_977223)
	p->ring1_ptsa_rate /= 2;
#endif
	p->ring1_ptsa_rate += p->disb_ptsa_rate +
			      p->ve_ptsa_rate +
			      p->ring2_ptsa_rate;
	writel(p->ring1_ptsa_rate, T14X_MC_RA(RING1_PTSA_RATE_0));
}

static void t14x_init_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;
	struct clk *emc_clk __attribute__((unused));
	unsigned long emc_freq __attribute__((unused));
	unsigned long same_freq __attribute__((unused));
	unsigned long grant_dec __attribute__((unused));

	emc_clk = clk_get(NULL, "emc");
	la_debug("**** emc clk_rate=%luMHz", clk_get_rate(emc_clk) / 1000000);

	emc_freq = clk_get_rate(emc_clk);
	emc_freq /= 1000000;
	/* Compute initial value for grant dec */
	same_freq = readl(T14X_MC_RA(EMEM_ARB_MISC0_0));
	same_freq = same_freq >> 27 & 1;
	grant_dec = 256 * (same_freq ? 2 : 1) * emc_freq /
		    T14X_BASE_EMC_FREQ_MHZ;
	if (grant_dec > 511)
		grant_dec = 511;

	p->dis_ptsa_min = 0x36;
	p->dis_ptsa_max = 0x1e;
	p->dis_ptsa_rate = readl(T14X_MC_RA(DIS_PTSA_RATE_0));

	p->disb_ptsa_min = 0x36;
	p->disb_ptsa_max = 0x1e;
	p->disb_ptsa_rate = readl(T14X_MC_RA(DISB_PTSA_RATE_0));

	p->ve_ptsa_rate = t14x_get_ptsa_rate(T14X_MAX_CAMERA_BW_MHZ);
	p->ve_ptsa_min = 0x3d;
	p->ve_ptsa_max = 0x14;

	p->ring2_ptsa_rate = 0x01;
	p->ring2_ptsa_min = 0x3f;
	p->ring2_ptsa_max = 0x01;

	p->bbc_ptsa_min = 0x3e;
	p->bbc_ptsa_max = 0x18;

	p->mpcorer_ptsa_rate = 23 * emc_freq / T14X_BASE_EMC_FREQ_MHZ;
	p->mpcorer_ptsa_min = 0x3f;
	p->mpcorer_ptsa_max = 0x0b;

	p->smmu_ptsa_rate = 0x1;
	p->smmu_ptsa_min = 0x1;
	p->smmu_ptsa_max = 0x1;

	p->ring1_ptsa_min = 0x36;
	p->ring1_ptsa_max = 0x1f;

	p->dis_extra_snap_level = 0x0;
	p->heg_extra_snap_level = 0x2;
	p->ptsa_grant_dec = grant_dec;

	p->bbc_ptsa_rate = t14x_get_ptsa_rate(T14X_MAX_BBCDMA_BW_MHZ);

	/* BBC ring0 ptsa max/min/rate/limit */
	p->bbcll_earb_cfg = 0xd << 24 | 0x3f << 16 |
		t14x_get_ptsa_rate(T14X_MAX_BBCLL_BW_MHZ) << 8 | 8 << 0;

	program_ring1_ptsa(p);
	program_ptsa();
}

#define ID_IDX(x) (ID(x) - ID(DISPLAY_0A))
static void t14x_update_display_ptsa_rate(unsigned int *disp_bw_array)
{
	unsigned int total_dis_bw;
	unsigned int total_disb_bw;
	struct ptsa_info *p = &cs->ptsa_info;

	if (cs->disable_ptsa || cs->disable_disp_ptsa)
		return;
	total_dis_bw = disp_bw_array[ID_IDX(DISPLAY_0A)] +
			disp_bw_array[ID_IDX(DISPLAY_0B)] +
			disp_bw_array[ID_IDX(DISPLAY_0C)] +
			disp_bw_array[ID_IDX(DISPLAY_T)] +
			disp_bw_array[ID_IDX(DISPLAYD)];
	total_disb_bw = disp_bw_array[ID_IDX(DISPLAY_0AB)] +
			disp_bw_array[ID_IDX(DISPLAY_0BB)] +
			disp_bw_array[ID_IDX(DISPLAY_0CB)];

	p->dis_ptsa_rate = t14x_get_ptsa_rate(total_dis_bw);
	p->disb_ptsa_rate = t14x_get_ptsa_rate(total_disb_bw);

	writel(p->dis_ptsa_rate, T14X_MC_RA(DIS_PTSA_RATE_0));
	writel(p->disb_ptsa_rate, T14X_MC_RA(DISB_PTSA_RATE_0));

	program_ring1_ptsa(p);
}

#define BBC_ID_IDX(x) (ID(x) - ID(BBCR))
static void t14x_update_bbc_ptsa_rate(uint *bbc_bw_array)
{
	uint total_bbc_bw;
	struct ptsa_info *p = &cs->ptsa_info;

	if (cs->disable_ptsa || cs->disable_bbc_ptsa)
		return;

	total_bbc_bw = bbc_bw_array[BBC_ID_IDX(BBCR)] +
		       bbc_bw_array[BBC_ID_IDX(BBCW)];
	p->bbc_ptsa_rate = t14x_get_ptsa_rate(total_bbc_bw);
	writel(p->bbc_ptsa_rate, T14X_MC_RA(BBC_PTSA_RATE_0));
	program_ring1_ptsa(p);
}

static void program_la(struct la_client_info *ci, int la)
{
	unsigned long reg_read;
	unsigned long reg_write;

	spin_lock(&cs->lock);
	reg_read = readl(ci->reg_addr);
	reg_write = (reg_read & ~ci->mask) |
			(la << ci->shift);
	writel(reg_write, ci->reg_addr);
	ci->la_set = la;
	la_debug("reg_addr=0x%x, read=0x%x, write=0x%x",
		(u32)ci->reg_addr, (u32)reg_read, (u32)reg_write);

	BUG_ON(la > 255);
	/* la scaling for display is on in MC always.
	 * set lo and hi la values to same as normal la.
	 */
	switch (ci->id) {
		case ID(DISPLAY_0A):
			writel(la << 16 | la, T14X_MC_RA(SCALED_LA_DISPLAY0A_0));
			break;
		case ID(DISPLAY_0B):
			writel(la << 16 | la, T14X_MC_RA(SCALED_LA_DISPLAY0B_0));
			break;
		case ID(DISPLAY_0C):
			writel(la << 16 | la, T14X_MC_RA(SCALED_LA_DISPLAY0C_0));
			break;
		case ID(DISPLAY_0BB):
			writel(la << 16 | la, T14X_MC_RA(SCALED_LA_DISPLAY0BB_0));
			break;
		default:
			break;
	}
	spin_unlock(&cs->lock);
}

#define DISPLAY_MARGIN 100 /* 100 -> 1.0, 110 -> 1.1 */
static int t14x_set_la(enum tegra_la_id id, unsigned int bw_mbps)
{
	int ideal_la;
	int la_to_set;
	unsigned int fifo_size_in_atoms;
	int bytes_per_atom = cs->atom_size;
	struct la_client_info *ci;
	int idx = cs->id_to_index[id];

	VALIDATE_ID(id, cs);
	VALIDATE_BW(bw_mbps);

	ci = &cs->la_info_array[idx];
	fifo_size_in_atoms = ci->fifo_size_in_atoms;

	if (id == TEGRA_LA_BBCR || id == TEGRA_LA_BBCW) {
		cs->bbc_bw_array[id - TEGRA_LA_BBCR] = bw_mbps;
		t14x_update_bbc_ptsa_rate(cs->bbc_bw_array);
#ifdef CONFIG_TEGRA_DISABLE_BBC_LATENCY_ALLOWANCE
		return 0;
#endif
	}

	if (id >= TEGRA_LA_DISPLAY_0A && id <= TEGRA_LA_DISPLAYD) {
		cs->disp_bw_array[id - TEGRA_LA_DISPLAY_0A] = bw_mbps;
		t14x_update_display_ptsa_rate(cs->disp_bw_array);
	}

	if (bw_mbps == 0) {
		la_to_set = cs->la_max_value;
	} else {
		if (id >= TEGRA_LA_DISPLAY_0A && id <= TEGRA_LA_DISPLAYD) {
			/* display la margin shold be 1.1 */
			ideal_la = (100 * fifo_size_in_atoms * bytes_per_atom * 1000) /
				   (DISPLAY_MARGIN * bw_mbps * cs->ns_per_tick);
		} else {
			ideal_la = (fifo_size_in_atoms * bytes_per_atom * 1000) /
				   (bw_mbps * cs->ns_per_tick);
		}
		la_to_set = ideal_la -
			    (ci->expiration_in_ns / cs->ns_per_tick) - 1;
	}

	la_debug("\n%s:id=%d,idx=%d, bw=%dmbps, la_to_set=%d",
		__func__, id, idx, bw_mbps, la_to_set);
	la_to_set = (la_to_set < 0) ? 0 : la_to_set;
	la_to_set = (la_to_set > cs->la_max_value) ? cs->la_max_value : la_to_set;

	if (cs->disable_la)
		return 0;
	program_la(ci, la_to_set);
	return 0;
}

static int t14x_la_suspend(void)
{
	int i = 0;
	struct la_client_info *ci;

	/* stashing LA and PTSA from registers is necessary
	 * in order to get latest values programmed by DVFS.
	 */
	for (i = 0; i < cs->la_info_array_size; i++) {
		ci = &cs->la_info_array[i];
		ci->la_set = (readl(ci->reg_addr) & ci->mask) >>
			     ci->shift;
	}
	save_ptsa();
	return 0;
}

static void t14x_la_resume(void)
{
	int i;

	for (i = 0; i < cs->la_info_array_size; i++) {
		if (cs->la_info_array[i].la_set)
			program_la(&cs->la_info_array[i],
				cs->la_info_array[i].la_set);
	}
	program_ptsa();
}

void tegra_la_get_t14x_specific(struct la_chip_specific *cs_la)
{
	cs_la->ns_per_tick = 30;
	cs_la->atom_size = 16;
	cs_la->la_max_value = T14X_MC_LA_MAX_VALUE;
	cs_la->la_info_array = t14x_la_info_array;
	cs_la->la_info_array_size = ARRAY_SIZE(t14x_la_info_array);
	cs_la->init_ptsa = t14x_init_ptsa;
	cs_la->update_display_ptsa_rate = t14x_update_display_ptsa_rate;
	cs_la->set_la = t14x_set_la;
	cs_la->suspend = t14x_la_suspend;
	cs_la->resume = t14x_la_resume;
	cs = cs_la;
}
