/*
 * arch/arm/mach-tegra/tegra3_la_priv.h
 *
 * Copyright (C) 2012, NVIDIA CORPORATION. All rights reserved.
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

#ifndef _MACH_TEGRA_TEGRA3_LA_PRIV_H_
#define _MACH_TEGRA_TEGRA3_LA_PRIV_H_

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)

#define MC_LA_AFI_0		0x2e0
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
#define MC_LA_MPE_0		0x328
#define MC_LA_MPE_1		0x32c
#define MC_LA_MPE_2		0x330
#define MC_LA_NV_0		0x334
#define MC_LA_NV_1		0x338
#define MC_LA_NV2_0		0x33c
#define MC_LA_NV2_1		0x340
#define MC_LA_PPCS_0		0x344
#define MC_LA_PPCS_1		0x348
#define MC_LA_PTC_0		0x34c
#define MC_LA_SATA_0		0x350
#define MC_LA_VDE_0		0x354
#define MC_LA_VDE_1		0x358
#define MC_LA_VDE_2		0x35c
#define MC_LA_VDE_3		0x360
#define MC_LA_VI_0		0x364
#define MC_LA_VI_1		0x368
#define MC_LA_VI_2		0x36c

#define MC_ARB_OVERRIDE		0xe8
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

/*
 * The rule for getting the fifo_size_in_atoms is:
 * 1.If REORDER_DEPTH exists, use it(default is overridden).
 * 2.Else if (write_client) use RFIFO_DEPTH.
 * 3.Else (read client) use RDFIFO_DEPTH.
 * Multiply the value by 2 for wide clients.
 * A client is wide, if CMW is larger than MW.
 * Refer to project.h file.
 */
struct la_client_info la_info_array[] = {
	LA_INFO(32,	150,	AFI_0,	7 : 0,		AFIR,		false,	0),
	LA_INFO(32,	150,	AFI_0,	23 : 16,	AFIW,		false,	0),
	LA_INFO(2,	150,	AVPC_ARM7_0, 7 : 0,	AVPC_ARM7R,	false,	0),
	LA_INFO(2,	150,	AVPC_ARM7_0, 23 : 16,	AVPC_ARM7W,	false,	0),
	LA_INFO(128,	1050,	DC_0,	7 : 0,		DISPLAY_0A,	true,	0),
	LA_INFO(64,	1050,	DC_0,	23 : 16,	DISPLAY_0B,	true,	0),
	LA_INFO(128,	1050,	DC_1,	7 : 0,		DISPLAY_0C,	true,	0),
	LA_INFO(64,	1050,	DC_1,	23 : 16,	DISPLAY_1B,	true,	0),
	LA_INFO(2,	1050,	DC_2,	7 : 0,		DISPLAY_HC,	false,	0),
	LA_INFO(128,	1050,	DCB_0,	7 : 0,		DISPLAY_0AB,	true,	0),
	LA_INFO(64,	1050,	DCB_0,	23 : 16,	DISPLAY_0BB,	true,	0),
	LA_INFO(128,	1050,	DCB_1,	7 : 0,		DISPLAY_0CB,	true,	0),
	LA_INFO(64,	1050,	DCB_1,	23 : 16,	DISPLAY_1BB,	true,	0),
	LA_INFO(2,	1050,	DCB_2,	7 : 0,		DISPLAY_HCB,	false,	0),
	LA_INFO(8,	150,	EPP_0,	7 : 0,		EPPUP,		false,	0),
	LA_INFO(64,	150,	EPP_0,	23 : 16,	EPPU,		false,	0),
	LA_INFO(64,	150,	EPP_1,	7 : 0,		EPPV,		false,	0),
	LA_INFO(64,	150,	EPP_1,	23 : 16,	EPPY,		false,	0),
	LA_INFO(64,	150,	G2_0,	7 : 0,		G2PR,		false,	0),
	LA_INFO(64,	150,	G2_0,	23 : 16,	G2SR,		false,	0),
	LA_INFO(48,	150,	G2_1,	7 : 0,		G2DR,		false,	0),
	LA_INFO(128,	150,	G2_1,	23 : 16,	G2DW,		false,	0),
	LA_INFO(16,	150,	HC_0,	7 : 0,		HOST1X_DMAR,	false,	0),
	LA_INFO(8,	150,	HC_0,	23 : 16,	HOST1XR,	false,	0),
	LA_INFO(32,	150,	HC_1,	7 : 0,		HOST1XW,	false,	0),
	LA_INFO(16,	150,	HDA_0,	7 : 0,		HDAR,		false,	0),
	LA_INFO(16,	150,	HDA_0,	23 : 16,	HDAW,		false,	0),
	LA_INFO(64,	150,	ISP_0,	7 : 0,		ISPW,		false,	0),
	LA_INFO(14,	150,	MPCORE_0, 7 : 0,	MPCORER,	false,	0),
	LA_INFO(24,	150,	MPCORE_0, 23 : 16,	MPCOREW,	false,	0),
	LA_INFO(14,	150,	MPCORELP_0, 7 : 0,	MPCORE_LPR,	false,	0),
	LA_INFO(24,	150,	MPCORELP_0, 23 : 16,	MPCORE_LPW,	false,	0),
	LA_INFO(8,	150,	MPE_0,	7 : 0,		MPE_UNIFBR,	false,	0),
	LA_INFO(2,	150,	MPE_0,	23 : 16,	MPE_IPRED,	false,	0),
	LA_INFO(64,	150,	MPE_1,	7 : 0,		MPE_AMEMRD,	false,	0),
	LA_INFO(8,	150,	MPE_1,	23 : 16,	MPE_CSRD,	false,	0),
	LA_INFO(8,	150,	MPE_2,	7 : 0,		MPE_UNIFBW,	false,	0),
	LA_INFO(8,	150,	MPE_2,	23 : 16,	MPE_CSWR,	false,	0),
	LA_INFO(96,	150,	NV_0,	7 : 0,		FDCDRD,		false,	0),
	LA_INFO(64,	150,	NV_0,	23 : 16,	IDXSRD,		false,	0),
	LA_INFO(64,	150,	NV_1,	7 : 0,		TEXSRD,		false,	0),
	LA_INFO(96,	150,	NV_1,	23 : 16,	FDCDWR,		false,	0),
	LA_INFO(96,	150,	NV2_0,	7 : 0,		FDCDRD2,	false,	0),
	LA_INFO(64,	150,	NV2_0,	23 : 16,	IDXSRD2,	false,	0),
	LA_INFO(64,	150,	NV2_1,	7 : 0,		TEXSRD2,	false,	0),
	LA_INFO(96,	150,	NV2_1,	23 : 16,	FDCDWR2,	false,	0),
	LA_INFO(2,	150,	PPCS_0,	7 : 0,		PPCS_AHBDMAR,	false,	0),
	LA_INFO(8,	150,	PPCS_0,	23 : 16,	PPCS_AHBSLVR,	false,	0),
	LA_INFO(2,	150,	PPCS_1,	7 : 0,		PPCS_AHBDMAW,	false,	0),
	LA_INFO(4,	150,	PPCS_1,	23 : 16,	PPCS_AHBSLVW,	false,	0),
	LA_INFO(2,	150,	PTC_0,	7 : 0,		PTCR,		false,	0),
	LA_INFO(32,	150,	SATA_0,	7 : 0,		SATAR,		false,	0),
	LA_INFO(32,	150,	SATA_0,	23 : 16,	SATAW,		false,	0),
	LA_INFO(8,	150,	VDE_0,	7 : 0,		VDE_BSEVR,	false,	0),
	LA_INFO(4,	150,	VDE_0,	23 : 16,	VDE_MBER,	false,	0),
	LA_INFO(16,	150,	VDE_1,	7 : 0,		VDE_MCER,	false,	0),
	LA_INFO(16,	150,	VDE_1,	23 : 16,	VDE_TPER,	false,	0),
	LA_INFO(4,	150,	VDE_2,	7 : 0,		VDE_BSEVW,	false,	0),
	LA_INFO(16,	150,	VDE_2,	23 : 16,	VDE_DBGW,	false,	0),
	LA_INFO(2,	150,	VDE_3,	7 : 0,		VDE_MBEW,	false,	0),
	LA_INFO(16,	150,	VDE_3,	23 : 16,	VDE_TPMW,	false,	0),
	LA_INFO(8,	1050,	VI_0,	7 : 0,		VI_RUV,		false,	0),
	LA_INFO(64,	1050,	VI_0,	23 : 16,	VI_WSB,		true,	0),
	LA_INFO(64,	1050,	VI_1,	7 : 0,		VI_WU,		true,	0),
	LA_INFO(64,	1050,	VI_1,	23 : 16,	VI_WV,		true,	0),
	LA_INFO(64,	1050,	VI_2,	7 : 0,		VI_WY,		true,	0),

/* end of list. */
	LA_INFO(0,	0,	AFI_0,	0 : 0,		MAX_ID,		false,	0)
};

#define DISP1_RA(r) \
	((u32)IO_ADDRESS(TEGRA_DISPLAY_BASE) + DS_DISP_MCCIF_##r##_HYST)
#define DISP2_RA(r) \
	((u32)IO_ADDRESS(TEGRA_DISPLAY2_BASE) + DS_DISP_MCCIF_##r##_HYST)

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
	((u32)IO_ADDRESS(TEGRA_VI_BASE) + VI_MCCIF_##r##_HYST)
#define VI_TM_RA(r) \
	((u32)IO_ADDRESS(TEGRA_VI_BASE) + VI_TIMEOUT_WOCAL_VI)
#define VI_TL_RA(r) \
	((u32)IO_ADDRESS(TEGRA_VI_BASE) + VI_RESERVE_##r)

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

static int ns_per_tick = 30;
/* Tegra3 MC atom size in bytes */
static const int normal_atom_size = 16;
#endif

#endif /* _MACH_TEGRA_TEGRA3_LA_PRIV_H_ */
