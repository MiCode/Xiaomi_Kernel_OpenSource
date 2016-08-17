/*
 * arch/arm/mach-tegra/tegra11x_la_priv.h
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

#ifndef _MACH_TEGRA_TEGRA11x_LA_PRIV_H_
#define _MACH_TEGRA_TEGRA11x_LA_PRIV_H_

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)

#define MC_LA_AVPC_ARM7_0	0x2e4
#define MC_LA_DC_0		0x2e8
#define MC_LA_DC_1		0x2ec
#define MC_LA_DC_2		0x2f0
#define MC_LA_DCB_0		0x2f4
#define MC_LA_DCB_1		0x2f8
#define MC_LA_DCB_2		0x2fc
#define MC_LA_EPP_0		0x300
#define MC_LA_EPP_1		0x304
#define MC_LA_G2_0		0x308
#define MC_LA_G2_1		0x30c
#define MC_LA_HC_0		0x310
#define MC_LA_HC_1		0x314
#define MC_LA_HDA_0		0x318
#define MC_LA_ISP_0		0x31C
#define MC_LA_MPCORE_0		0x320
#define MC_LA_MPCORELP_0	0x324
#define MC_LA_MSENC_0		0x328
#define MC_LA_NV_0		0x334
#define MC_LA_NV_1		0x338
#define MC_LA_NV2_0		0x33c
#define MC_LA_NV2_1		0x340
#define MC_LA_PPCS_0		0x344
#define MC_LA_PPCS_1		0x348
#define MC_LA_PTC_0		0x34c

#define MC_LA_VDE_0		0x354
#define MC_LA_VDE_1		0x358
#define MC_LA_VDE_2		0x35c
#define MC_LA_VDE_3		0x360
#define MC_LA_VI_0		0x364
#define MC_LA_VI_1		0x368
#define MC_LA_VI_2		0x36c

#define MC_LA_XUSB_0		0x37c /* T11x specific*/
#define MC_LA_XUSB_1		0x380 /* T11x specific*/
#define MC_LA_NV_2		0x384 /* T11x specific*/
#define MC_LA_NV_3		0x388 /* T11x specific*/

#define MC_LA_EMUCIF_0		0x38c
#define MC_LA_TSEC_0		0x390

#define MC_DIS_PTSA_RATE_0		0x41c
#define MC_DIS_PTSA_MIN_0		0x420
#define MC_DIS_PTSA_MAX_0		0x424
#define MC_DISB_PTSA_RATE_0		0x428
#define MC_DISB_PTSA_MIN_0		0x42c
#define MC_DISB_PTSA_MAX_0		0x430
#define MC_VE_PTSA_RATE_0		0x434
#define MC_VE_PTSA_MIN_0		0x438
#define MC_VE_PTSA_MAX_0		0x43c
#define MC_RING2_PTSA_RATE_0		0x440
#define MC_RING2_PTSA_MIN_0		0x444
#define MC_RING2_PTSA_MAX_0		0x448
#define MC_MLL_MPCORER_PTSA_RATE_0	0x44c
#define MC_MLL_MPCORER_PTSA_MIN_0	0x450
#define MC_MLL_MPCORER_PTSA_MAX_0	0x454
#define MC_SMMU_SMMU_PTSA_RATE_0	0x458
#define MC_SMMU_SMMU_PTSA_MIN_0		0x45c
#define MC_SMMU_SMMU_PTSA_MAX_0		0x460
#define MC_R0_DIS_PTSA_RATE_0		0x464
#define MC_R0_DIS_PTSA_MIN_0		0x468
#define MC_R0_DIS_PTSA_MAX_0		0x46c
#define MC_R0_DISB_PTSA_RATE_0		0x470
#define MC_R0_DISB_PTSA_MIN_0		0x474
#define MC_R0_DISB_PTSA_MAX_0		0x478
#define MC_RING1_PTSA_RATE_0		0x47c
#define MC_RING1_PTSA_MIN_0		0x480
#define MC_RING1_PTSA_MAX_0		0x484

#define MC_DIS_EXTRA_SNAP_LEVELS_0	0x2ac
#define MC_HEG_EXTRA_SNAP_LEVELS_0	0x2b0
#define MC_EMEM_ARB_MISC0_0		0x0d8
#define MC_PTSA_GRANT_DECREMENT_0	0x960

#define BASE_EMC_FREQ_MHZ		500
#define MAX_CAMERA_BW_MHZ		528


/*
 * The rule for getting the fifo_size_in_atoms is:
 * 1.If REORDER_DEPTH exists, use it(default is overridden).
 * 2.Else if (write_client) use RFIFO_DEPTH.
 * 3.Else (read client) use RDFIFO_DEPTH.
 * Multiply the value by 2 for dual channel.
 * Multiply the value by 2 for wide clients.
 * A client is wide, if CMW is larger than MW.
 * Refer to project.h file.
 */
struct la_client_info la_info_array[] = {
	LA_INFO(3,	150,	AVPC_ARM7_0, 7 : 0,	AVPC_ARM7R,	false,	0),
	LA_INFO(3,	150,	AVPC_ARM7_0, 23 : 16,	AVPC_ARM7W,	false,	0),
	LA_INFO(256,	1050,	DC_0,	7 : 0,		DISPLAY_0A,	true,	0),
	LA_INFO(256,	1050,	DC_0,	23 : 16,	DISPLAY_0B,	true,	0),
	LA_INFO(256,	1050,	DC_1,	7 : 0,		DISPLAY_0C,	true,	0),
	LA_INFO(96,	1050,	DC_2,	7 : 0,		DISPLAY_HC,	false,	0),
	LA_INFO(256,	1050,	DCB_0,	7 : 0,		DISPLAY_0AB,	true,	0),
	LA_INFO(256,	1050,	DCB_0,	23 : 16,	DISPLAY_0BB,	true,	0),
	LA_INFO(256,	1050,	DCB_1,	7 : 0,		DISPLAY_0CB,	true,	0),
	LA_INFO(96,	1050,	DCB_2,	7 : 0,		DISPLAY_HCB,	false,	0),
	LA_INFO(16,	150,	EPP_0,	7 : 0,		EPPUP,		false,	0),
	LA_INFO(64,	150,	EPP_0,	23 : 16,	EPPU,		false,	0),
	LA_INFO(64,	150,	EPP_1,	7 : 0,		EPPV,		false,	0),
	LA_INFO(64,	150,	EPP_1,	23 : 16,	EPPY,		false,	0),
	LA_INFO(128,	150,	G2_0,	7 : 0,		G2PR,		false,	0),
	LA_INFO(128,	150,	G2_0,	23 : 16,	G2SR,		false,	0),
	LA_INFO(96,	150,	G2_1,	7 : 0,		G2DR,		false,	0),
	LA_INFO(256,	150,	G2_1,	23 : 16,	G2DW,		false,	0),
	LA_INFO(32,	150,	HC_0,	7 : 0,		HOST1X_DMAR,	false,	0),
	LA_INFO(16,	150,	HC_0,	23 : 16,	HOST1XR,	false,	0),
	LA_INFO(64,	150,	HC_1,	7 : 0,		HOST1XW,	false,	0),
	LA_INFO(32,	150,	HDA_0,	7 : 0,		HDAR,		false,	0),
	LA_INFO(32,	150,	HDA_0,	23 : 16,	HDAW,		false,	0),
	LA_INFO(128,	150,	ISP_0,	7 : 0,		ISPW,		false,	0),
	LA_INFO(96,	150,	MPCORE_0,   7 : 0,	MPCORER,	false,	0),
	LA_INFO(128,	150,	MPCORE_0,   23 : 16,	MPCOREW,	false,	0),
	LA_INFO(96,	150,	MPCORELP_0, 7 : 0,	MPCORE_LPR,	false,	0),
	LA_INFO(128,	150,	MPCORELP_0, 23 : 16,	MPCORE_LPW,	false,	0),
	LA_INFO(128,	150,	NV_0,	7 : 0,		FDCDRD,		false,	0),
	LA_INFO(256,	150,	NV_0,	23 : 16,	IDXSRD,		false,	0),
	LA_INFO(432,	150,	NV_1,	7 : 0,		TEXL2SRD,	false,	0),
	LA_INFO(128,	150,	NV_1,	23 : 16,	FDCDWR,		false,	0),
	LA_INFO(128,	150,	NV2_0,	7 : 0,		FDCDRD2,	false,	0),
	LA_INFO(128,	150,	NV2_1,	23 : 16,	FDCDWR2,	false,	0),
	LA_INFO(8,	150,	PPCS_0,	7 : 0,		PPCS_AHBDMAR,	false,	0),
	LA_INFO(80,	150,	PPCS_0,	23 : 16,	PPCS_AHBSLVR,	false,	0),
	LA_INFO(16,	150,	PPCS_1,	7 : 0,		PPCS_AHBDMAW,	false,	0),
	LA_INFO(80,	150,	PPCS_1,	23 : 16,	PPCS_AHBSLVW,	false,	0),
	LA_INFO(40,	150,	PTC_0,	7 : 0,		PTCR,		false,	0),
	LA_INFO(16,	150,	VDE_0,	7 : 0,		VDE_BSEVR,	false,	131),
	LA_INFO(8,	150,	VDE_0,	23 : 16,	VDE_MBER,	false,	131),
	LA_INFO(64,	150,	VDE_1,	7 : 0,		VDE_MCER,	false,	50),
	LA_INFO(32,	150,	VDE_1,	23 : 16,	VDE_TPER,	false,	123),
	LA_INFO(8,	150,	VDE_2,	7 : 0,		VDE_BSEVW,	false,	131),
	LA_INFO(32,	150,	VDE_2,	23 : 16,	VDE_DBGW,	false,	131),
	LA_INFO(16,	150,	VDE_3,	7 : 0,		VDE_MBEW,	false,	70),
	LA_INFO(32,	150,	VDE_3,	23 : 16,	VDE_TPMW,	false,	76),
	LA_INFO(128,	1050,	VI_0,	7 : 0,		VI_WSB,		true,	0),
	LA_INFO(128,	1050,	VI_1,	7 : 0,		VI_WU,		true,	0),
	LA_INFO(128,	1050,	VI_1,	23 : 16,	VI_WV,		true,	0),
	LA_INFO(128,	1050,	VI_2,	7 : 0,		VI_WY,		true,	0),

	LA_INFO(128,	150,	MSENC_0,    7 : 0,	MSENCSRD,	false,	128),
	LA_INFO(32,	150,	MSENC_0,    23 : 16,	MSENCSWR,	false,	41),
	LA_INFO(160,	150,	XUSB_0,	    7 : 0,	XUSB_HOSTR,	false,	0),
	LA_INFO(160,	150,	XUSB_0,	    23 : 16,	XUSB_HOSTW,	false,	0),
	LA_INFO(160,	150,	XUSB_1,	    7 : 0,	XUSB_DEVR,	false,	0),
	LA_INFO(160,	150,	XUSB_1,	    23 : 16,	XUSB_DEVW,	false,	0),
	LA_INFO(128,	150,	NV_2,	    7 : 0,	FDCDRD3,	false,	0),
	LA_INFO(128,	150,	NV_2,	    23 : 16,	FDCDRD4,	false,	0),
	LA_INFO(128,	150,	NV_3,	    7 : 0,	FDCDWR3,	false,	0),
	LA_INFO(128,	150,	NV_3,	    23 : 16,	FDCDWR4,	false,	0),
	LA_INFO(28,	150,	EMUCIF_0,   7 : 0,	EMUCIFR,	false,	0),
	LA_INFO(48,	150,	EMUCIF_0,   23 : 16,	EMUCIFW,	false,	0),
	LA_INFO(32,	150,	TSEC_0,	    7 : 0,	TSECSRD,	false,	0),
	LA_INFO(32,	150,	TSEC_0,	    23 : 16,	TSECSWR,	false,	0),

/* end of list. */
	LA_INFO(0,	0,	TSEC_0,	    0 : 0,	MAX_ID,		false,	0)
};

static int ns_per_tick = 30;
/* MC atom size in bytes */
static const int normal_atom_size = 16;
#endif

#endif /* _MACH_TEGRA_TEGRA11x_LA_PRIV_H_ */
