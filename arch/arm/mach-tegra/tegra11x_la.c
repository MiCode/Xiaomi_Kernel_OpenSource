/*
 * arch/arm/mach-tegra/tegra11x_la.c
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

#define T11X_MC_LA_AVPC_ARM7_0		0x2e4
#define T11X_MC_LA_DC_0			0x2e8
#define T11X_MC_LA_DC_1			0x2ec
#define T11X_MC_LA_DC_2			0x2f0
#define T11X_MC_LA_DCB_0		0x2f4
#define T11X_MC_LA_DCB_1		0x2f8
#define T11X_MC_LA_DCB_2		0x2fc
#define T11X_MC_LA_EPP_0		0x300
#define T11X_MC_LA_EPP_1		0x304
#define T11X_MC_LA_G2_0			0x308
#define T11X_MC_LA_G2_1			0x30c
#define T11X_MC_LA_HC_0			0x310
#define T11X_MC_LA_HC_1			0x314
#define T11X_MC_LA_HDA_0		0x318
#define T11X_MC_LA_ISP_0		0x31C
#define T11X_MC_LA_MPCORE_0		0x320
#define T11X_MC_LA_MPCORELP_0		0x324
#define T11X_MC_LA_MSENC_0		0x328
#define T11X_MC_LA_NV_0			0x334
#define T11X_MC_LA_NV_1			0x338
#define T11X_MC_LA_NV2_0		0x33c
#define T11X_MC_LA_NV2_1		0x340
#define T11X_MC_LA_PPCS_0		0x344
#define T11X_MC_LA_PPCS_1		0x348
#define T11X_MC_LA_PTC_0		0x34c

#define T11X_MC_LA_VDE_0		0x354
#define T11X_MC_LA_VDE_1		0x358
#define T11X_MC_LA_VDE_2		0x35c
#define T11X_MC_LA_VDE_3		0x360
#define T11X_MC_LA_VI_0			0x364
#define T11X_MC_LA_VI_1			0x368
#define T11X_MC_LA_VI_2			0x36c

#define T11X_MC_LA_XUSB_0		0x37c /* T11x specific*/
#define T11X_MC_LA_XUSB_1		0x380 /* T11x specific*/
#define T11X_MC_LA_NV_2			0x384 /* T11x specific*/
#define T11X_MC_LA_NV_3			0x388 /* T11x specific*/

#define T11X_MC_LA_EMUCIF_0		0x38c
#define T11X_MC_LA_TSEC_0		0x390

#define T11X_MC_DIS_PTSA_RATE_0			0x41c
#define T11X_MC_DIS_PTSA_MIN_0			0x420
#define T11X_MC_DIS_PTSA_MAX_0			0x424
#define T11X_MC_DISB_PTSA_RATE_0		0x428
#define T11X_MC_DISB_PTSA_MIN_0			0x42c
#define T11X_MC_DISB_PTSA_MAX_0			0x430
#define T11X_MC_VE_PTSA_RATE_0			0x434
#define T11X_MC_VE_PTSA_MIN_0			0x438
#define T11X_MC_VE_PTSA_MAX_0			0x43c
#define T11X_MC_RING2_PTSA_RATE_0		0x440
#define T11X_MC_RING2_PTSA_MIN_0		0x444
#define T11X_MC_RING2_PTSA_MAX_0		0x448
#define T11X_MC_MLL_MPCORER_PTSA_RATE_0		0x44c
#define T11X_MC_MLL_MPCORER_PTSA_MIN_0		0x450
#define T11X_MC_MLL_MPCORER_PTSA_MAX_0		0x454
#define T11X_MC_SMMU_SMMU_PTSA_RATE_0		0x458
#define T11X_MC_SMMU_SMMU_PTSA_MIN_0		0x45c
#define T11X_MC_SMMU_SMMU_PTSA_MAX_0		0x460
#define T11X_MC_R0_DIS_PTSA_RATE_0		0x464
#define T11X_MC_R0_DIS_PTSA_MIN_0		0x468
#define T11X_MC_R0_DIS_PTSA_MAX_0		0x46c
#define T11X_MC_R0_DISB_PTSA_RATE_0		0x470
#define T11X_MC_R0_DISB_PTSA_MIN_0		0x474
#define T11X_MC_R0_DISB_PTSA_MAX_0		0x478
#define T11X_MC_RING1_PTSA_RATE_0		0x47c
#define T11X_MC_RING1_PTSA_MIN_0		0x480
#define T11X_MC_RING1_PTSA_MAX_0		0x484

#define T11X_MC_DIS_EXTRA_SNAP_LEVELS_0		0x2ac
#define T11X_MC_HEG_EXTRA_SNAP_LEVELS_0		0x2b0
#define T11X_MC_EMEM_ARB_MISC0_0		0x0d8
#define T11X_MC_PTSA_GRANT_DECREMENT_0		0x960

#define T11X_BASE_EMC_FREQ_MHZ		500
#define T11X_MAX_CAMERA_BW_MHZ		528

/* maximum valid value for latency allowance */
#define T11X_MC_LA_MAX_VALUE		255

#define T11X_MC_RA(r) \
	(IO_ADDRESS(TEGRA_MC_BASE) + (T11X_MC_##r))
#define T11X_RA(r) \
	(IO_ADDRESS(TEGRA_MC_BASE) + (T11X_MC_LA_##r))

#define T11X_LA(f, e, a, r, i, ss, la) \
{ \
	.fifo_size_in_atoms = f, \
	.expiration_in_ns = e, \
	.reg_addr = T11X_RA(a), \
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
struct la_client_info t11x_la_info_array[] = {
	T11X_LA(3,	150,	AVPC_ARM7_0, 7 : 0,	AVPC_ARM7R,	false,	0),
	T11X_LA(3,	150,	AVPC_ARM7_0, 23 : 16,	AVPC_ARM7W,	false,	0),
	T11X_LA(256,	1050,	DC_0,	7 : 0,		DISPLAY_0A,	true,	0),
	T11X_LA(256,	1050,	DC_0,	23 : 16,	DISPLAY_0B,	true,	0),
	T11X_LA(256,	1050,	DC_1,	7 : 0,		DISPLAY_0C,	true,	0),
	T11X_LA(96,	1050,	DC_2,	7 : 0,		DISPLAY_HC,	false,	0),
	T11X_LA(256,	1050,	DCB_0,	7 : 0,		DISPLAY_0AB,	true,	0),
	T11X_LA(256,	1050,	DCB_0,	23 : 16,	DISPLAY_0BB,	true,	0),
	T11X_LA(256,	1050,	DCB_1,	7 : 0,		DISPLAY_0CB,	true,	0),
	T11X_LA(96,	1050,	DCB_2,	7 : 0,		DISPLAY_HCB,	false,	0),
	T11X_LA(16,	150,	EPP_0,	7 : 0,		EPPUP,		false,	0),
	T11X_LA(64,	150,	EPP_0,	23 : 16,	EPPU,		false,	0),
	T11X_LA(64,	150,	EPP_1,	7 : 0,		EPPV,		false,	0),
	T11X_LA(64,	150,	EPP_1,	23 : 16,	EPPY,		false,	0),
	T11X_LA(128,	150,	G2_0,	7 : 0,		G2PR,		false,	0),
	T11X_LA(128,	150,	G2_0,	23 : 16,	G2SR,		false,	0),
	T11X_LA(96,	150,	G2_1,	7 : 0,		G2DR,		false,	0),
	T11X_LA(256,	150,	G2_1,	23 : 16,	G2DW,		false,	0),
	T11X_LA(32,	150,	HC_0,	7 : 0,		HOST1X_DMAR,	false,	0),
	T11X_LA(16,	150,	HC_0,	23 : 16,	HOST1XR,	false,	0),
	T11X_LA(64,	150,	HC_1,	7 : 0,		HOST1XW,	false,	0),
	T11X_LA(32,	150,	HDA_0,	7 : 0,		HDAR,		false,	0),
	T11X_LA(32,	150,	HDA_0,	23 : 16,	HDAW,		false,	0),
	T11X_LA(128,	150,	ISP_0,	7 : 0,		ISPW,		false,	0),
	T11X_LA(96,	150,	MPCORE_0,   7 : 0,	MPCORER,	false,	0),
	T11X_LA(128,	150,	MPCORE_0,   23 : 16,	MPCOREW,	false,	0),
	T11X_LA(96,	150,	MPCORELP_0, 7 : 0,	MPCORE_LPR,	false,	0),
	T11X_LA(128,	150,	MPCORELP_0, 23 : 16,	MPCORE_LPW,	false,	0),
	T11X_LA(128,	150,	NV_0,	7 : 0,		FDCDRD,		false,	0),
	T11X_LA(256,	150,	NV_0,	23 : 16,	IDXSRD,		false,	0),
	T11X_LA(432,	150,	NV_1,	7 : 0,		TEXL2SRD,	false,	0),
	T11X_LA(128,	150,	NV_1,	23 : 16,	FDCDWR,		false,	0),
	T11X_LA(128,	150,	NV2_0,	7 : 0,		FDCDRD2,	false,	0),
	T11X_LA(128,	150,	NV2_1,	7 : 0,		FDCDWR2,	false,	0),
	T11X_LA(8,	150,	PPCS_0,	7 : 0,		PPCS_AHBDMAR,	false,	0),
	T11X_LA(80,	150,	PPCS_0,	23 : 16,	PPCS_AHBSLVR,	false,	0),
	T11X_LA(16,	150,	PPCS_1,	7 : 0,		PPCS_AHBDMAW,	false,	0),
	T11X_LA(80,	150,	PPCS_1,	23 : 16,	PPCS_AHBSLVW,	false,	0),
	T11X_LA(40,	150,	PTC_0,	7 : 0,		PTCR,		false,	0),
	T11X_LA(16,	150,	VDE_0,	7 : 0,		VDE_BSEVR,	false,	131),
	T11X_LA(8,	150,	VDE_0,	23 : 16,	VDE_MBER,	false,	131),
	T11X_LA(64,	150,	VDE_1,	7 : 0,		VDE_MCER,	false,	50),
	T11X_LA(32,	150,	VDE_1,	23 : 16,	VDE_TPER,	false,	123),
	T11X_LA(8,	150,	VDE_2,	7 : 0,		VDE_BSEVW,	false,	131),
	T11X_LA(32,	150,	VDE_2,	23 : 16,	VDE_DBGW,	false,	131),
	T11X_LA(16,	150,	VDE_3,	7 : 0,		VDE_MBEW,	false,	70),
	T11X_LA(32,	150,	VDE_3,	23 : 16,	VDE_TPMW,	false,	76),
	T11X_LA(128,	1050,	VI_0,	7 : 0,		VI_WSB,		true,	0),
	T11X_LA(128,	1050,	VI_1,	7 : 0,		VI_WU,		true,	0),
	T11X_LA(128,	1050,	VI_1,	23 : 16,	VI_WV,		true,	0),
	T11X_LA(128,	1050,	VI_2,	7 : 0,		VI_WY,		true,	0),

	T11X_LA(128,	150,	MSENC_0,    7 : 0,	MSENCSRD,	false,	128),
	T11X_LA(32,	150,	MSENC_0,    23 : 16,	MSENCSWR,	false,	41),
	T11X_LA(160,	150,	XUSB_0,	    7 : 0,	XUSB_HOSTR,	false,	0),
	T11X_LA(160,	150,	XUSB_0,	    23 : 16,	XUSB_HOSTW,	false,	0),
	T11X_LA(160,	150,	XUSB_1,	    7 : 0,	XUSB_DEVR,	false,	0),
	T11X_LA(160,	150,	XUSB_1,	    23 : 16,	XUSB_DEVW,	false,	0),
	T11X_LA(128,	150,	NV_2,	    7 : 0,	FDCDRD3,	false,	0),
	T11X_LA(128,	150,	NV_2,	    23 : 16,	FDCDRD4,	false,	0),
	T11X_LA(128,	150,	NV_3,	    7 : 0,	FDCDWR3,	false,	0),
	T11X_LA(128,	150,	NV_3,	    23 : 16,	FDCDWR4,	false,	0),
	T11X_LA(28,	150,	EMUCIF_0,   7 : 0,	EMUCIFR,	false,	0),
	T11X_LA(48,	150,	EMUCIF_0,   23 : 16,	EMUCIFW,	false,	0),
	T11X_LA(32,	150,	TSEC_0,	    7 : 0,	TSECSRD,	false,	0),
	T11X_LA(32,	150,	TSEC_0,	    23 : 16,	TSECSWR,	false,	0),

/* end of list. */
	T11X_LA(0,	0,	TSEC_0,	    0 : 0,	MAX_ID,		false,	0)
};

static unsigned int t11x_get_ptsa_rate(unsigned int bw)
{
	/* 16 = 2 channels * 2 ddr * 4 bytes */
	unsigned int base_memory_bw = 16 * T11X_BASE_EMC_FREQ_MHZ;
	unsigned int rate = 281 * bw / base_memory_bw;
	if (rate > 255)
		rate = 255;
	return rate;
}

static void t11x_init_ptsa(void)
{
	struct clk *emc_clk __attribute__((unused));
	unsigned long emc_freq __attribute__((unused));
	unsigned long same_freq __attribute__((unused));
	unsigned long grant_dec __attribute__((unused));
	unsigned long ring1_rate __attribute__((unused));

	emc_clk = clk_get(NULL, "emc");
	la_debug("**** emc clk_rate=%luMHz", clk_get_rate(emc_clk)/1000000);

	emc_freq = clk_get_rate(emc_clk);
	emc_freq /= 1000000;
	/* Compute initial value for grant dec */
	same_freq = readl(T11X_MC_RA(EMEM_ARB_MISC0_0));
	same_freq = same_freq >> 27 & 1;
	grant_dec = 256 * (same_freq ? 2 : 1) * emc_freq;
	if (grant_dec > 511)
		grant_dec = 511;
	writel(grant_dec, T11X_MC_RA(PTSA_GRANT_DECREMENT_0));

	writel(0x3d, T11X_MC_RA(DIS_PTSA_MIN_0));
	writel(0x14, T11X_MC_RA(DIS_PTSA_MAX_0));

	writel(0x3d, T11X_MC_RA(DISB_PTSA_MIN_0));
	writel(0x14, T11X_MC_RA(DISB_PTSA_MAX_0));

	writel(t11x_get_ptsa_rate(T11X_MAX_CAMERA_BW_MHZ),
		T11X_MC_RA(VE_PTSA_RATE_0));
	writel(0x3d, T11X_MC_RA(VE_PTSA_MIN_0));
	writel(0x14, T11X_MC_RA(VE_PTSA_MAX_0));

	writel(0x01, T11X_MC_RA(RING2_PTSA_RATE_0));
	writel(0x3f, T11X_MC_RA(RING2_PTSA_MIN_0));
	writel(0x05, T11X_MC_RA(RING2_PTSA_MAX_0));

	writel(38 * emc_freq / T11X_BASE_EMC_FREQ_MHZ,
		T11X_MC_RA(MLL_MPCORER_PTSA_RATE_0));
	writel(0x3f, T11X_MC_RA(MLL_MPCORER_PTSA_MIN_0));
	writel(0x05, T11X_MC_RA(MLL_MPCORER_PTSA_MAX_0));

	writel(0x01, T11X_MC_RA(SMMU_SMMU_PTSA_RATE_0));
	writel(0x01, T11X_MC_RA(SMMU_SMMU_PTSA_MIN_0));
	writel(0x01, T11X_MC_RA(SMMU_SMMU_PTSA_MAX_0));

	ring1_rate = readl(T11X_MC_RA(DIS_PTSA_RATE_0)) +
		     readl(T11X_MC_RA(DISB_PTSA_RATE_0));
#if defined(CONFIG_TEGRA_ERRATA_977223)
	ring1_rate /= 2;
#endif
	ring1_rate += readl(T11X_MC_RA(VE_PTSA_RATE_0)) +
		      readl(T11X_MC_RA(RING2_PTSA_RATE_0));
	writel(ring1_rate, T11X_MC_RA(RING1_PTSA_RATE_0));
	writel(0x36, T11X_MC_RA(RING1_PTSA_MIN_0));
	writel(0x1f, T11X_MC_RA(RING1_PTSA_MAX_0));

	writel(0x00, T11X_MC_RA(DIS_EXTRA_SNAP_LEVELS_0));
	writel(0x03, T11X_MC_RA(HEG_EXTRA_SNAP_LEVELS_0));
}

static void t11x_update_display_ptsa_rate(unsigned int *disp_bw_array)
{
	unsigned int num_active = (disp_bw_array[0] != 0) +
				  (disp_bw_array[1] != 0) +
				  (disp_bw_array[2] != 0);
	unsigned int num_activeb = (disp_bw_array[5] != 0) +
				   (disp_bw_array[6] != 0) +
				   (disp_bw_array[7] != 0);
	unsigned int max_bw = disp_bw_array[0];
	unsigned int max_bwb = disp_bw_array[5];
	unsigned int rate_dis;
	unsigned int rate_disb;
	unsigned long ring1_rate;

	max_bw = max(disp_bw_array[0], disp_bw_array[1]);
	max_bw = max(max_bw, disp_bw_array[2]);

	max_bwb = max(disp_bw_array[5], disp_bw_array[6]);
	max_bwb = max(max_bwb, disp_bw_array[7]);

	rate_dis = t11x_get_ptsa_rate(num_active * max_bw);
	rate_disb = t11x_get_ptsa_rate(num_activeb * max_bwb);

	writel(rate_dis, T11X_MC_RA(DIS_PTSA_RATE_0));
	writel(rate_disb, T11X_MC_RA(DISB_PTSA_RATE_0));

	ring1_rate = rate_dis + rate_disb;
#if defined(CONFIG_TEGRA_ERRATA_977223)
	ring1_rate /= 2;
#endif
	ring1_rate += readl(T11X_MC_RA(VE_PTSA_RATE_0)) +
		      readl(T11X_MC_RA(RING2_PTSA_RATE_0));
	writel(ring1_rate, T11X_MC_RA(RING1_PTSA_RATE_0));
}

void tegra_la_get_t11x_specific(struct la_chip_specific *cs)
{
	cs->ns_per_tick = 30;
	cs->atom_size = 16;
	cs->la_max_value = T11X_MC_LA_MAX_VALUE;
	cs->la_info_array = t11x_la_info_array;
	cs->la_info_array_size = ARRAY_SIZE(t11x_la_info_array);
	cs->init_ptsa = t11x_init_ptsa;
	cs->update_display_ptsa_rate = t11x_update_display_ptsa_rate;
}
