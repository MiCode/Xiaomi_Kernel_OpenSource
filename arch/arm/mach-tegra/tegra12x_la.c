/*
 * arch/arm/mach-tegra/tegra12x_la.c
 *
 * Copyright (C) 2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/clk.h>
#include <asm/io.h>
#include <mach/latency_allowance.h>
#include "la_priv.h"
#include "clock.h"
#include "iomap.h"


/*
 * Note about fixed point arithmetic:
 * ----------------------------------
 * This file contains fixed point values and arithmetic due to the need to use
 * floating point values. All fixed point values have the "_fp" or "_FP" suffix
 * in their name. Macros used to convert between real and fixed point values are
 * listed below:
 *    - T12X_LA_FP_FACTOR
 *    - T12X_LA_REAL_TO_FP(val)
 *    - T12X_LA_FP_TO_REAL(val)
 *
 * Some scenarios require additional accuracy than what can be provided with
 * T12X_LA_FP_FACTOR. For these special cases we use the following additional
 * fixed point factor:- T12X_LA_ADDITIONAL_FP_FACTOR. Fixed point values which
 * use the addtional fixed point factor have a suffix of "_fpa" or "_FPA" in
 * their name. Macros used to convert between fpa values and other forms (i.e.
 * fp and real) are as follows:
 *    - T12X_LA_FP_TO_FPA(val)
 *    - T12X_LA_FPA_TO_FP(val)
 *    - T12X_LA_FPA_TO_REAL(val)
 *    - T12X_LA_REAL_TO_FPA(val)
 */


/* LA registers */
#define T12X_MC_LA_AFI_0				0x2e0
#define T12X_MC_LA_AVPC_ARM7_0				0x2e4
#define T12X_MC_LA_DC_0					0x2e8
#define T12X_MC_LA_DC_1					0x2ec
#define T12X_MC_LA_DC_2					0x2f0
#define T12X_MC_LA_DCB_0				0x2f4
#define T12X_MC_LA_DCB_1				0x2f8
#define T12X_MC_LA_DCB_2				0x2fc
#define T12X_MC_LA_HC_0					0x310
#define T12X_MC_LA_HC_1					0x314
#define T12X_MC_LA_HDA_0				0x318
#define T12X_MC_LA_MPCORE_0				0x320
#define T12X_MC_LA_MPCORELP_0				0x324
#define T12X_MC_LA_MSENC_0				0x328
#define T12X_MC_LA_PPCS_0				0x344
#define T12X_MC_LA_PPCS_1				0x348
#define T12X_MC_LA_PTC_0				0x34c
#define T12X_MC_LA_SATA_0				0x350
#define T12X_MC_LA_VDE_0				0x354
#define T12X_MC_LA_VDE_1				0x358
#define T12X_MC_LA_VDE_2				0x35c
#define T12X_MC_LA_VDE_3				0x360
#define T12X_MC_LA_ISP2_0				0x370
#define T12X_MC_LA_ISP2_1				0x374
#define T12X_MC_LA_XUSB_0				0x37c
#define T12X_MC_LA_XUSB_1				0x380
#define T12X_MC_LA_ISP2B_0				0x384
#define T12X_MC_LA_ISP2B_1				0x388
#define T12X_MC_LA_TSEC_0				0x390
#define T12X_MC_LA_VIC_0				0x394
#define T12X_MC_LA_VI2_0				0x398
#define T12X_MC_LA_GPU_0				0x3ac
#define T12X_MC_LA_SDMMCA_0				0x3b8
#define T12X_MC_LA_SDMMCAA_0				0x3bc
#define T12X_MC_LA_SDMMC_0				0x3c0
#define T12X_MC_LA_SDMMCAB_0				0x3c4
#define T12X_MC_LA_DC_3					0x3c8
#define T12X_MC_SCALED_LA_DISPLAY0A_0	(IO_ADDRESS(TEGRA_MC_BASE) + 0x690)
#define T12X_MC_SCALED_LA_DISPLAY0A_0_LOW_SHIFT		0
#define T12X_MC_SCALED_LA_DISPLAY0A_0_LOW_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0A_0_LOW_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0A_0_HIGH_SHIFT	16
#define T12X_MC_SCALED_LA_DISPLAY0A_0_HIGH_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0A_0_HIGH_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0AB_0	(IO_ADDRESS(TEGRA_MC_BASE) + 0x694)
#define T12X_MC_SCALED_LA_DISPLAY0AB_0_LOW_SHIFT	 0
#define T12X_MC_SCALED_LA_DISPLAY0AB_0_LOW_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0AB_0_LOW_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0AB_0_HIGH_SHIFT	16
#define T12X_MC_SCALED_LA_DISPLAY0AB_0_HIGH_MASK	(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0AB_0_HIGH_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0B_0	(IO_ADDRESS(TEGRA_MC_BASE) + 0x698)
#define T12X_MC_SCALED_LA_DISPLAY0B_0_LOW_SHIFT		0
#define T12X_MC_SCALED_LA_DISPLAY0B_0_LOW_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0B_0_LOW_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0B_0_HIGH_SHIFT	16
#define T12X_MC_SCALED_LA_DISPLAY0B_0_HIGH_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0B_0_HIGH_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0BB_0	(IO_ADDRESS(TEGRA_MC_BASE) + 0x69c)
#define T12X_MC_SCALED_LA_DISPLAY0BB_0_LOW_SHIFT	 0
#define T12X_MC_SCALED_LA_DISPLAY0BB_0_LOW_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0BB_0_LOW_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0BB_0_HIGH_SHIFT	16
#define T12X_MC_SCALED_LA_DISPLAY0BB_0_HIGH_MASK	(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0BB_0_HIGH_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0C_0	(IO_ADDRESS(TEGRA_MC_BASE) + 0x6a0)
#define T12X_MC_SCALED_LA_DISPLAY0C_0_LOW_SHIFT		0
#define T12X_MC_SCALED_LA_DISPLAY0C_0_LOW_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0C_0_LOW_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0C_0_HIGH_SHIFT	16
#define T12X_MC_SCALED_LA_DISPLAY0C_0_HIGH_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0C_0_HIGH_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0CB_0	(IO_ADDRESS(TEGRA_MC_BASE) + 0x6a4)
#define T12X_MC_SCALED_LA_DISPLAY0CB_0_LOW_SHIFT	 0
#define T12X_MC_SCALED_LA_DISPLAY0CB_0_LOW_MASK		(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0CB_0_LOW_SHIFT)
#define T12X_MC_SCALED_LA_DISPLAY0CB_0_HIGH_SHIFT	16
#define T12X_MC_SCALED_LA_DISPLAY0CB_0_HIGH_MASK	(0xff << \
				T12X_MC_SCALED_LA_DISPLAY0CB_0_HIGH_SHIFT)

/* PTSA registers */
#define T12X_MC_DIS_PTSA_RATE_0		0x41c
#define T12X_MC_DIS_PTSA_MIN_0		0x420
#define T12X_MC_DIS_PTSA_MAX_0		0x424
#define T12X_MC_DISB_PTSA_RATE_0	0x428
#define T12X_MC_DISB_PTSA_MIN_0		0x42c
#define T12X_MC_DISB_PTSA_MAX_0		0x430
#define T12X_MC_VE_PTSA_RATE_0		0x434
#define T12X_MC_VE_PTSA_MIN_0		0x438
#define T12X_MC_VE_PTSA_MAX_0		0x43c
#define T12X_MC_RING2_PTSA_RATE_0	0x440
#define T12X_MC_RING2_PTSA_MIN_0	0x444
#define T12X_MC_RING2_PTSA_MAX_0	0x448
#define T12X_MC_MLL_MPCORER_PTSA_RATE_0	0x44c
#define T12X_MC_MLL_MPCORER_PTSA_MIN_0	0x450
#define T12X_MC_MLL_MPCORER_PTSA_MAX_0	0x454
#define	T12X_MC_SMMU_SMMU_PTSA_RATE_0	0x458
#define T12X_MC_SMMU_SMMU_PTSA_MIN_0	0x45c
#define T12X_MC_SMMU_SMMU_PTSA_MAX_0	0x460
#define T12X_MC_R0_DIS_PTSA_MIN_0	0x468
#define T12X_MC_R0_DIS_PTSA_MAX_0	0x46c
#define T12X_MC_R0_DISB_PTSA_MIN_0	0x474
#define T12X_MC_R0_DISB_PTSA_MAX_0	0x478
#define T12X_MC_RING1_PTSA_RATE_0	0x47c
#define T12X_MC_RING1_PTSA_MIN_0	0x480
#define T12X_MC_RING1_PTSA_MAX_0	0x484
#define T12X_MC_A9AVPPC_PTSA_MIN_0	0x48c
#define T12X_MC_A9AVPPC_PTSA_MAX_0	0x490
#define T12X_MC_VE2_PTSA_RATE_0		0x494
#define T12X_MC_VE2_PTSA_MIN_0		0x498
#define T12X_MC_VE2_PTSA_MAX_0		0x49c
#define T12X_MC_ISP_PTSA_RATE_0		0x4a0
#define T12X_MC_ISP_PTSA_MIN_0		0x4a4
#define T12X_MC_ISP_PTSA_MAX_0		0x4a8
#define T12X_MC_PCX_PTSA_MIN_0		0x4b0
#define T12X_MC_PCX_PTSA_MAX_0		0x4b4
#define T12X_MC_SAX_PTSA_MIN_0		0x4bc
#define T12X_MC_SAX_PTSA_MAX_0		0x4c0
#define T12X_MC_MSE_PTSA_MIN_0		0x4c8
#define T12X_MC_MSE_PTSA_MAX_0		0x4cc
#define T12X_MC_SD_PTSA_MIN_0		0x4d4
#define T12X_MC_SD_PTSA_MAX_0		0x4d8
#define T12X_MC_AHB_PTSA_MIN_0		0x4e0
#define T12X_MC_AHB_PTSA_MAX_0		0x4e4
#define T12X_MC_APB_PTSA_MIN_0		0x4ec
#define T12X_MC_APB_PTSA_MAX_0		0x4f0
#define T12X_MC_AVP_PTSA_MIN_0		0x4f8
#define T12X_MC_AVP_PTSA_MAX_0		0x4fc
#define T12X_MC_VD_PTSA_MIN_0		0x504
#define T12X_MC_VD_PTSA_MAX_0		0x508
#define T12X_MC_FTOP_PTSA_MIN_0		0x510
#define T12X_MC_FTOP_PTSA_MAX_0		0x514
#define T12X_MC_HOST_PTSA_MIN_0		0x51c
#define T12X_MC_HOST_PTSA_MAX_0		0x520
#define T12X_MC_USBX_PTSA_MIN_0		0x528
#define T12X_MC_USBX_PTSA_MAX_0		0x52c
#define T12X_MC_USBD_PTSA_MIN_0		0x534
#define T12X_MC_USBD_PTSA_MAX_0		0x538
#define T12X_MC_GK_PTSA_MIN_0		0x540
#define T12X_MC_GK_PTSA_MAX_0		0x544
#define T12X_MC_AUD_PTSA_MIN_0		0x54c
#define T12X_MC_AUD_PTSA_MAX_0		0x550
#define T12X_MC_VICPC_PTSA_MIN_0	0x558
#define T12X_MC_VICPC_PTSA_MAX_0	0x55c

/* Misc registers */
#define T12X_MC_EMEM_ARB_MISC0_0	0xd8
#define     T12X_MC_EMC_SAME_FREQ_BIT	27
#define T12X_MC_TIMING_CONTROL_0	0xfc
#define T12X_MC_DIS_EXTRA_SNAP_LEVELS_0	0x2ac
#define T12X_MC_PTSA_GRANT_DECREMENT_0	0x960

/* Register naming macros */
#define T12X_MC_RA(r) \
	(IO_ADDRESS(TEGRA_MC_BASE) + (T12X_MC_##r))
#define T12X_RA(r) \
	(IO_ADDRESS(TEGRA_MC_BASE) + (T12X_MC_LA_##r))

/* Misc macros */
#define T12X_LA_FP_FACTOR					1000
#define T12X_LA_REAL_TO_FP(val)			((val) * T12X_LA_FP_FACTOR)
#define T12X_LA_FP_TO_REAL(val)			((val) / T12X_LA_FP_FACTOR)
#define T12X_LA_ADDITIONAL_FP_FACTOR				10
#define T12X_LA_FP_TO_FPA(val)		((val) * T12X_LA_ADDITIONAL_FP_FACTOR)
#define T12X_LA_FPA_TO_FP(val)		((val) / T12X_LA_ADDITIONAL_FP_FACTOR)
#define T12X_LA_FPA_TO_REAL(val)		((val) / \
						T12X_LA_FP_FACTOR / \
						T12X_LA_ADDITIONAL_FP_FACTOR)
#define T12X_LA_REAL_TO_FPA(val)		((val) * \
						T12X_LA_FP_FACTOR * \
						T12X_LA_ADDITIONAL_FP_FACTOR)
#define T12X_LA_STATIC_LA_MINUS_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP	70000
#define T12X_LA_DRAM_WIDTH_BITS					64
#define T12X_LA_DISP_CATCHUP_FACTOR_FP				1100
#define T12X_MC_PTSA_MIN_DEFAULT_MASK				0x3f
#define T12X_MC_PTSA_MAX_DEFAULT_MASK				0x3f
#define T12X_MC_PTSA_RATE_DEFAULT_MASK				0xff
#define T12X_EMC_MIN_FREQ_MHZ_FP				12500
#define T12X_EMC_MAX_FREQ_MHZ					1067
#define T12X_MC_MAX_FREQ_MHZ					533
#define T12X_MAX_GRANT_DEC					511
#define T12X_MAX_GD_FP						1996
#define T12X_LOW_GD_FP				(T12X_MAX_GD_FP * \
						T12X_EMC_MIN_FREQ_MHZ_FP * \
						2 / \
						T12X_EMC_MAX_FREQ_MHZ/ \
						T12X_LA_FP_FACTOR)
#define T12X_MAX_GD_FPA						19961
#define T12X_LOW_GD_FPA				(T12X_MAX_GD_FPA * \
						T12X_EMC_MIN_FREQ_MHZ_FP * \
						2 / \
						T12X_EMC_MAX_FREQ_MHZ/ \
						T12X_LA_FP_FACTOR)
#define T12X_MAX_DDA_RATE					255
#define T12X_EXP_TIME_EMCCLKS_FP				88000
#define T12X_MAX_LA_NSEC					7650
/* Note:- T12X_DDA_BW_MARGIN_FP is supposed to be 1100. But
	  T12X_DDA_BW_MARGIN_FP has been increased to 1150 to compensate for
	  fixed point arithmetic errors. */
#define T12X_DDA_BW_MARGIN_FP					1100
#define T12X_1_DDA_FRAC_FP					10
#define T12X_MPCORER_CPU_RD_MARGIN_FP				100
#define T12X_MIN_CYCLES_PER_GRANT				2
#define T12X_EMEM_PTSA_MINMAX_WIDTH				5
#define T12X_EMEM_PTSA_RATE_WIDTH				8
#define T12X_RING1_FEEDER_SISO_ALLOC_DIV			2
#define T12X_LA_USEC_TO_NSEC_FACTOR				1000
#define T12X_LA_HZ_TO_MHZ_FACTOR				1000000
#define T12X_LA(f, e, a, r, i, ss, la, clk) \
{ \
	.fifo_size_in_atoms = f, \
	.expiration_in_ns = e, \
	.reg_addr = T12X_RA(a), \
	.mask = MASK(r), \
	.shift = SHIFT(r), \
	.id = ID(i), \
	.name = __stringify(i), \
	.scaling_supported = ss, \
	.init_la = la, \
	.la_ref_clk_mhz = clk \
}


struct la_client_info t12x_la_info_array[] = {
	T12X_LA(0, 0, AFI_0, 7 : 0, AFIR, false, 28, 800),
	T12X_LA(0, 0, AFI_0, 23 : 16, AFIW, false, 128, 800),
	T12X_LA(0, 0, AVPC_ARM7_0, 7 : 0, AVPC_ARM7R, false, 4, 0),
	T12X_LA(0, 0, AVPC_ARM7_0, 23 : 16, AVPC_ARM7W, false, 128, 800),
	T12X_LA(0, 0, DC_0, 7 : 0, DISPLAY_0A, true, 80, 0),
	T12X_LA(0, 0, DCB_0, 7 : 0, DISPLAY_0AB, true, 80, 0),
	T12X_LA(0, 0, DC_0, 23 : 16, DISPLAY_0B, true, 80, 0),
	T12X_LA(0, 0, DCB_0, 23 : 16, DISPLAY_0BB, true, 80, 0),
	T12X_LA(0, 0, DC_1, 7 : 0, DISPLAY_0C, true, 80, 0),
	T12X_LA(0, 0, DCB_1, 7 : 0, DISPLAY_0CB, true, 80, 0),
	T12X_LA(0, 0, DC_3, 7 : 0, DISPLAYD, false, 80, 0),
	T12X_LA(0, 0, DC_2, 7 : 0, DISPLAY_HC, false, 80, 0),
	T12X_LA(0, 0, DCB_2, 7 : 0, DISPLAY_HCB, false, 80, 0),
	T12X_LA(0, 0, DC_2, 23 : 16, DISPLAY_T, false, 80, 0),
	T12X_LA(0, 0, GPU_0, 7 : 0, GPUSRD, false, 25, 800),
	T12X_LA(0, 0, GPU_0, 23 : 16, GPUSWR, false, 128, 800),
	T12X_LA(0, 0, HDA_0, 7 : 0, HDAR, false, 36, 0),
	T12X_LA(0, 0, HDA_0, 23 : 16, HDAW, false, 128, 800),
	T12X_LA(0, 0, TSEC_0, 7 : 0, TSECSRD, false, 60, 200),
	T12X_LA(0, 0, TSEC_0, 23 : 16, TSECSWR, false, 128, 800),
	T12X_LA(0, 0, HC_0, 7 : 0, HOST1X_DMAR, false, 22, 800),
	T12X_LA(0, 0, HC_0, 23 : 16, HOST1XR, false, 80, 0),
	T12X_LA(0, 0, HC_1, 7 : 0, HOST1XW, false, 128, 800),
	T12X_LA(0, 0, ISP2_0, 7 : 0, ISP_RA, false, 54, 300),
	T12X_LA(0, 0, ISP2_1, 7 : 0, ISP_WA, false, 128, 800),
	T12X_LA(0, 0, ISP2_1, 23 : 16, ISP_WB, false, 128, 800),
	T12X_LA(0, 0, ISP2B_0, 7 : 0, ISP_RAB, false, 54, 300),
	T12X_LA(0, 0, ISP2B_1, 7 : 0, ISP_WAB, false, 128, 800),
	T12X_LA(0, 0, ISP2B_1, 23 : 16, ISP_WBB, false, 128, 800),
	T12X_LA(0, 0, MPCORELP_0, 7 : 0, MPCORE_LPR, false, 4, 0),
	T12X_LA(0, 0, MPCORELP_0, 23 : 16, MPCORE_LPW, false, 128, 800),
	T12X_LA(0, 0, MPCORE_0, 7 : 0, MPCORER, false, 4, 0),
	T12X_LA(0, 0, MPCORE_0, 23 : 16, MPCOREW, false, 128, 800),
	T12X_LA(0, 0, MSENC_0, 7 : 0, MSENCSRD, false, 24, 0),
	T12X_LA(0, 0, MSENC_0, 23 : 16, MSENCSWR, false, 128, 800),
	T12X_LA(0, 0, PPCS_1, 7 : 0, PPCS_AHBDMAW, true, 128, 800),
	T12X_LA(0, 0, PPCS_0, 23 : 16, PPCS_AHBSLVR, false, 39, 408),
	T12X_LA(0, 0, PPCS_1, 23 : 16, PPCS_AHBSLVW, false, 128, 800),
	T12X_LA(0, 0, PTC_0, 7 : 0, PTCR, false, 0, 0),
	T12X_LA(0, 0, SATA_0, 7 : 0, SATAR, false, 101, 400),
	T12X_LA(0, 0, SATA_0, 23 : 16, SATAW, false, 128, 800),
	T12X_LA(0, 0, SDMMC_0, 7 : 0, SDMMCR, false, 144, 248),
	T12X_LA(0, 0, SDMMCA_0, 7 : 0, SDMMCRA, false, 144, 248),
	T12X_LA(0, 0, SDMMCAA_0, 7 : 0, SDMMCRAA, false, 65, 248),
	T12X_LA(0, 0, SDMMCAB_0, 7 : 0, SDMMCRAB, false, 65, 248),
	T12X_LA(0, 0, SDMMC_0, 23 : 16, SDMMCW, false, 128, 800),
	T12X_LA(0, 0, SDMMCA_0, 23 : 16, SDMMCWA, false, 128, 800),
	T12X_LA(0, 0, SDMMCAA_0, 23 : 16, SDMMCWAA, false, 128, 800),
	T12X_LA(0, 0, SDMMCAB_0, 23 : 16, SDMMCWAB, false, 128, 800),
	T12X_LA(0, 0, VDE_0, 7 : 0, VDE_BSEVR, false, 255, 0),
	T12X_LA(0, 0, VDE_2, 7 : 0, VDE_BSEVW, false, 128, 800),
	T12X_LA(0, 0, VDE_2, 23 : 16, VDE_DBGW, false, 255, 0),
	T12X_LA(0, 0, VDE_0, 23 : 16, VDE_MBER, false, 212, 200),
	T12X_LA(0, 0, VDE_3, 7 : 0, VDE_MBEW, false, 128, 800),
	T12X_LA(0, 0, VDE_1, 7 : 0, VDE_MCER, false, 41, 400),
	T12X_LA(0, 0, VDE_1, 23 : 16, VDE_TPER, false, 81, 200),
	T12X_LA(0, 0, VDE_3, 23 : 16, VDE_TPMW, false, 128, 800),
	T12X_LA(0, 0, VIC_0, 7 : 0, VICSRD, false, 27, 800),
	T12X_LA(0, 0, VIC_0, 23 : 16, VICSWR, false, 128, 800),
	T12X_LA(0, 0, VI2_0, 7 : 0, VI_W, false, 128, 800),
	T12X_LA(0, 0, XUSB_1, 7 : 0, XUSB_DEVR, false, 56, 400),
	T12X_LA(0, 0, XUSB_1, 23 : 16, XUSB_DEVW, false, 128, 800),
	T12X_LA(0, 0, XUSB_0, 7 : 0, XUSB_HOSTR, false, 56, 400),
	T12X_LA(0, 0, XUSB_0, 23 : 16, XUSB_HOSTW, false, 128, 800),

	/* end of list */
	T12X_LA(0, 0, DC_3, 0 : 0, MAX_ID, false, 0, 0)
};

static struct la_chip_specific *cs;
const struct disp_client *tegra_la_disp_clients_info;
static unsigned int total_dc0_bw;
static unsigned int total_dc1_bw;
DEFINE_MUTEX(disp_and_camera_ptsa_lock);


unsigned int tegra12x_la_real_to_fp(unsigned int val)
{
	return val * T12X_LA_FP_FACTOR;
}

unsigned int tegra12x_la_fp_to_real(unsigned int val)
{
	return val / T12X_LA_FP_FACTOR;
}

static inline bool is_display_client(enum tegra_la_id id)
{
	return ((id >= FIRST_DISP_CLIENT_ID) && (id <= LAST_DISP_CLIENT_ID));
}

static inline bool is_camera_client(enum tegra_la_id id)
{
	return ((id >= FIRST_CAMERA_CLIENT_ID) &&
		(id <= LAST_CAMERA_CLIENT_ID));
}

unsigned int fraction2dda_fp(unsigned int fraction_fp,
				unsigned int div,
				unsigned int mask)
{
	unsigned int dda = 0;
	unsigned int f_fpa = T12X_LA_FP_TO_FPA(fraction_fp) / div;
	int i = 0;
	unsigned int r = 0;

	for (i = 0; i < T12X_EMEM_PTSA_RATE_WIDTH; i++) {
		f_fpa *= 2;
		r = T12X_LA_FPA_TO_REAL(f_fpa);
		dda = (dda << 1) | (unsigned int)(r);
		f_fpa -= T12X_LA_REAL_TO_FPA(r);
	}
	if (f_fpa > 0) {
		/* Do not round up if the calculated dda is at the mask value
		   already, it will overflow */
		if (dda != mask)
			dda++;		/* to round up dda value */
	}

	return min(dda, (unsigned int)T12X_MAX_DDA_RATE);
}

static void program_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;

	writel(p->ptsa_grant_dec, T12X_MC_RA(PTSA_GRANT_DECREMENT_0));
	writel(1, T12X_MC_RA(TIMING_CONTROL_0));

	writel(p->dis_ptsa_rate, T12X_MC_RA(DIS_PTSA_RATE_0));
	writel(p->dis_ptsa_min, T12X_MC_RA(DIS_PTSA_MIN_0));
	writel(p->dis_ptsa_max, T12X_MC_RA(DIS_PTSA_MAX_0));

	writel(p->disb_ptsa_rate, T12X_MC_RA(DISB_PTSA_RATE_0));
	writel(p->disb_ptsa_min, T12X_MC_RA(DISB_PTSA_MIN_0));
	writel(p->disb_ptsa_max, T12X_MC_RA(DISB_PTSA_MAX_0));

	writel(p->ve_ptsa_rate, T12X_MC_RA(VE_PTSA_RATE_0));
	writel(p->ve_ptsa_min, T12X_MC_RA(VE_PTSA_MIN_0));
	writel(p->ve_ptsa_max, T12X_MC_RA(VE_PTSA_MAX_0));

	writel(p->ve2_ptsa_rate, T12X_MC_RA(VE2_PTSA_RATE_0));
	writel(p->ve2_ptsa_min, T12X_MC_RA(VE2_PTSA_MIN_0));
	writel(p->ve2_ptsa_max, T12X_MC_RA(VE2_PTSA_MAX_0));

	writel(p->ring2_ptsa_rate, T12X_MC_RA(RING2_PTSA_RATE_0));
	writel(p->ring2_ptsa_min, T12X_MC_RA(RING2_PTSA_MIN_0));
	writel(p->ring2_ptsa_max, T12X_MC_RA(RING2_PTSA_MAX_0));

	/* FIXME:- Is bbc and bbcll_earb_cfg required for T124?
	writel(p->bbc_ptsa_rate, T12X_MC_RA(BBC_PTSA_RATE_0));
	writel(p->bbc_ptsa_min, T12X_MC_RA(BBC_PTSA_MIN_0));
	writel(p->bbc_ptsa_max, T12X_MC_RA(BBC_PTSA_MAX_0));

	writel(p->bbcll_earb_cfg, T12X_MC_RA(BBCLL_EARB_CFG_0));*/

	writel(p->mpcorer_ptsa_rate, T12X_MC_RA(MLL_MPCORER_PTSA_RATE_0));
	writel(p->mpcorer_ptsa_min, T12X_MC_RA(MLL_MPCORER_PTSA_MIN_0));
	writel(p->mpcorer_ptsa_max, T12X_MC_RA(MLL_MPCORER_PTSA_MAX_0));

	writel(p->smmu_ptsa_rate, T12X_MC_RA(SMMU_SMMU_PTSA_RATE_0));
	writel(p->smmu_ptsa_min, T12X_MC_RA(SMMU_SMMU_PTSA_MIN_0));
	writel(p->smmu_ptsa_max, T12X_MC_RA(SMMU_SMMU_PTSA_MAX_0));

	writel(p->ring1_ptsa_rate, T12X_MC_RA(RING1_PTSA_RATE_0));
	writel(p->ring1_ptsa_min, T12X_MC_RA(RING1_PTSA_MIN_0));
	writel(p->ring1_ptsa_max, T12X_MC_RA(RING1_PTSA_MAX_0));

	writel(p->dis_extra_snap_level, T12X_MC_RA(DIS_EXTRA_SNAP_LEVELS_0));
	/* FIXME:- Is heg_extra_snap_level required for T124?
	writel(p->heg_extra_snap_level, T12X_MC_RA(HEG_EXTRA_SNAP_LEVELS_0)); */

	writel(p->isp_ptsa_rate, T12X_MC_RA(ISP_PTSA_RATE_0));
	writel(p->isp_ptsa_min, T12X_MC_RA(ISP_PTSA_MIN_0));
	writel(p->isp_ptsa_max, T12X_MC_RA(ISP_PTSA_MAX_0));

	writel(p->a9avppc_ptsa_min, T12X_MC_RA(A9AVPPC_PTSA_MIN_0));
	writel(p->a9avppc_ptsa_max, T12X_MC_RA(A9AVPPC_PTSA_MAX_0));

	writel(p->avp_ptsa_min, T12X_MC_RA(AVP_PTSA_MIN_0));
	writel(p->avp_ptsa_max, T12X_MC_RA(AVP_PTSA_MAX_0));

	writel(p->r0_dis_ptsa_min, T12X_MC_RA(R0_DIS_PTSA_MIN_0));
	writel(p->r0_dis_ptsa_max, T12X_MC_RA(R0_DIS_PTSA_MAX_0));

	writel(p->r0_disb_ptsa_min, T12X_MC_RA(R0_DISB_PTSA_MIN_0));
	writel(p->r0_disb_ptsa_max, T12X_MC_RA(R0_DISB_PTSA_MAX_0));

	writel(p->vd_ptsa_min, T12X_MC_RA(VD_PTSA_MIN_0));
	writel(p->vd_ptsa_max, T12X_MC_RA(VD_PTSA_MAX_0));

	writel(p->mse_ptsa_min, T12X_MC_RA(MSE_PTSA_MIN_0));
	writel(p->mse_ptsa_max, T12X_MC_RA(MSE_PTSA_MAX_0));

	writel(p->gk_ptsa_min, T12X_MC_RA(GK_PTSA_MIN_0));
	writel(p->gk_ptsa_max, T12X_MC_RA(GK_PTSA_MAX_0));

	writel(p->vicpc_ptsa_min, T12X_MC_RA(VICPC_PTSA_MIN_0));
	writel(p->vicpc_ptsa_max, T12X_MC_RA(VICPC_PTSA_MAX_0));

	writel(p->apb_ptsa_min, T12X_MC_RA(APB_PTSA_MIN_0));
	writel(p->apb_ptsa_max, T12X_MC_RA(APB_PTSA_MAX_0));

	writel(p->pcx_ptsa_min, T12X_MC_RA(PCX_PTSA_MIN_0));
	writel(p->pcx_ptsa_max, T12X_MC_RA(PCX_PTSA_MAX_0));

	writel(p->host_ptsa_min, T12X_MC_RA(HOST_PTSA_MIN_0));
	writel(p->host_ptsa_max, T12X_MC_RA(HOST_PTSA_MAX_0));

	writel(p->ahb_ptsa_min, T12X_MC_RA(AHB_PTSA_MIN_0));
	writel(p->ahb_ptsa_max, T12X_MC_RA(AHB_PTSA_MAX_0));

	writel(p->sax_ptsa_min, T12X_MC_RA(SAX_PTSA_MIN_0));
	writel(p->sax_ptsa_max, T12X_MC_RA(SAX_PTSA_MAX_0));

	writel(p->aud_ptsa_min, T12X_MC_RA(AUD_PTSA_MIN_0));
	writel(p->aud_ptsa_max, T12X_MC_RA(AUD_PTSA_MAX_0));

	writel(p->sd_ptsa_min, T12X_MC_RA(SD_PTSA_MIN_0));
	writel(p->sd_ptsa_max, T12X_MC_RA(SD_PTSA_MAX_0));

	writel(p->usbx_ptsa_min, T12X_MC_RA(USBX_PTSA_MIN_0));
	writel(p->usbx_ptsa_max, T12X_MC_RA(USBX_PTSA_MAX_0));

	writel(p->usbd_ptsa_min, T12X_MC_RA(USBD_PTSA_MIN_0));
	writel(p->usbd_ptsa_min, T12X_MC_RA(USBD_PTSA_MAX_0));

	writel(p->ftop_ptsa_min, T12X_MC_RA(FTOP_PTSA_MIN_0));
	writel(p->ftop_ptsa_max, T12X_MC_RA(FTOP_PTSA_MAX_0));
}

static void save_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;

	p->ptsa_grant_dec = readl(T12X_MC_RA(PTSA_GRANT_DECREMENT_0));

	p->dis_ptsa_rate = readl(T12X_MC_RA(DIS_PTSA_RATE_0));
	p->dis_ptsa_min = readl(T12X_MC_RA(DIS_PTSA_MIN_0));
	p->dis_ptsa_max = readl(T12X_MC_RA(DIS_PTSA_MAX_0));

	p->disb_ptsa_rate = readl(T12X_MC_RA(DISB_PTSA_RATE_0));
	p->disb_ptsa_min = readl(T12X_MC_RA(DISB_PTSA_MIN_0));
	p->disb_ptsa_max = readl(T12X_MC_RA(DISB_PTSA_MAX_0));

	p->ve_ptsa_rate = readl(T12X_MC_RA(VE_PTSA_RATE_0));
	p->ve_ptsa_min = readl(T12X_MC_RA(VE_PTSA_MIN_0));
	p->ve_ptsa_max = readl(T12X_MC_RA(VE_PTSA_MAX_0));

	p->ve2_ptsa_rate = readl(T12X_MC_RA(VE2_PTSA_RATE_0));
	p->ve2_ptsa_min = readl(T12X_MC_RA(VE2_PTSA_MIN_0));
	p->ve2_ptsa_max = readl(T12X_MC_RA(VE2_PTSA_MAX_0));

	p->ring2_ptsa_rate = readl(T12X_MC_RA(RING2_PTSA_RATE_0));
	p->ring2_ptsa_min = readl(T12X_MC_RA(RING2_PTSA_MIN_0));
	p->ring2_ptsa_max = readl(T12X_MC_RA(RING2_PTSA_MAX_0));

	/* FIXME:- Is bbc and bbcll_earb_cfg required for T124?
	p->bbc_ptsa_rate = readl(T12X_MC_RA(BBC_PTSA_RATE_0));
	p->bbc_ptsa_min = readl(T12X_MC_RA(BBC_PTSA_MIN_0));
	p->bbc_ptsa_max = readl(T12X_MC_RA(BBC_PTSA_MAX_0));

	p->bbcll_earb_cfg = readl(T12X_MC_RA(BBCLL_EARB_CFG_0)); */

	p->mpcorer_ptsa_rate = readl(T12X_MC_RA(MLL_MPCORER_PTSA_RATE_0));
	p->mpcorer_ptsa_min = readl(T12X_MC_RA(MLL_MPCORER_PTSA_MIN_0));
	p->mpcorer_ptsa_max = readl(T12X_MC_RA(MLL_MPCORER_PTSA_MAX_0));

	p->smmu_ptsa_rate = readl(T12X_MC_RA(SMMU_SMMU_PTSA_RATE_0));
	p->smmu_ptsa_min = readl(T12X_MC_RA(SMMU_SMMU_PTSA_MIN_0));
	p->smmu_ptsa_max = readl(T12X_MC_RA(SMMU_SMMU_PTSA_MAX_0));

	p->ring1_ptsa_rate = readl(T12X_MC_RA(RING1_PTSA_RATE_0));
	p->ring1_ptsa_min = readl(T12X_MC_RA(RING1_PTSA_MIN_0));
	p->ring1_ptsa_max = readl(T12X_MC_RA(RING1_PTSA_MAX_0));

	p->dis_extra_snap_level = readl(T12X_MC_RA(DIS_EXTRA_SNAP_LEVELS_0));
	/* FIXME:- Is heg_extra_snap_level required for T124?
	p->heg_extra_snap_level = readl(T12X_MC_RA(HEG_EXTRA_SNAP_LEVELS_0)); */

	p->isp_ptsa_rate = readl(T12X_MC_RA(ISP_PTSA_RATE_0));
	p->isp_ptsa_min = readl(T12X_MC_RA(ISP_PTSA_MIN_0));
	p->isp_ptsa_max = readl(T12X_MC_RA(ISP_PTSA_MAX_0));

	p->a9avppc_ptsa_min = readl(T12X_MC_RA(A9AVPPC_PTSA_MIN_0));
	p->a9avppc_ptsa_max = readl(T12X_MC_RA(A9AVPPC_PTSA_MAX_0));

	p->avp_ptsa_min = readl(T12X_MC_RA(AVP_PTSA_MIN_0));
	p->avp_ptsa_max = readl(T12X_MC_RA(AVP_PTSA_MAX_0));

	p->r0_dis_ptsa_min = readl(T12X_MC_RA(R0_DIS_PTSA_MIN_0));
	p->r0_dis_ptsa_max = readl(T12X_MC_RA(R0_DIS_PTSA_MAX_0));

	p->r0_disb_ptsa_min = readl(T12X_MC_RA(R0_DISB_PTSA_MIN_0));
	p->r0_disb_ptsa_min = readl(T12X_MC_RA(R0_DISB_PTSA_MAX_0));

	p->vd_ptsa_min = readl(T12X_MC_RA(VD_PTSA_MIN_0));
	p->vd_ptsa_max = readl(T12X_MC_RA(VD_PTSA_MAX_0));

	p->mse_ptsa_min = readl(T12X_MC_RA(MSE_PTSA_MIN_0));
	p->mse_ptsa_max = readl(T12X_MC_RA(MSE_PTSA_MAX_0));

	p->gk_ptsa_min = readl(T12X_MC_RA(GK_PTSA_MIN_0));
	p->gk_ptsa_max = readl(T12X_MC_RA(GK_PTSA_MAX_0));

	p->vicpc_ptsa_min = readl(T12X_MC_RA(VICPC_PTSA_MIN_0));
	p->vicpc_ptsa_min = readl(T12X_MC_RA(VICPC_PTSA_MAX_0));

	p->apb_ptsa_min = readl(T12X_MC_RA(APB_PTSA_MIN_0));
	p->apb_ptsa_max = readl(T12X_MC_RA(APB_PTSA_MAX_0));

	p->pcx_ptsa_min = readl(T12X_MC_RA(PCX_PTSA_MIN_0));
	p->pcx_ptsa_max = readl(T12X_MC_RA(PCX_PTSA_MAX_0));

	p->host_ptsa_min = readl(T12X_MC_RA(HOST_PTSA_MIN_0));
	p->host_ptsa_max = readl(T12X_MC_RA(HOST_PTSA_MAX_0));

	p->ahb_ptsa_min = readl(T12X_MC_RA(AHB_PTSA_MIN_0));
	p->ahb_ptsa_max = readl(T12X_MC_RA(AHB_PTSA_MAX_0));

	p->sax_ptsa_min = readl(T12X_MC_RA(SAX_PTSA_MIN_0));
	p->sax_ptsa_max = readl(T12X_MC_RA(SAX_PTSA_MAX_0));

	p->aud_ptsa_min = readl(T12X_MC_RA(AUD_PTSA_MIN_0));
	p->aud_ptsa_max = readl(T12X_MC_RA(AUD_PTSA_MAX_0));

	p->sd_ptsa_min = readl(T12X_MC_RA(SD_PTSA_MIN_0));
	p->sd_ptsa_max = readl(T12X_MC_RA(SD_PTSA_MAX_0));

	p->usbx_ptsa_min = readl(T12X_MC_RA(USBX_PTSA_MIN_0));
	p->usbx_ptsa_max = readl(T12X_MC_RA(USBX_PTSA_MAX_0));

	p->usbd_ptsa_min = readl(T12X_MC_RA(USBD_PTSA_MIN_0));
	p->usbd_ptsa_max = readl(T12X_MC_RA(USBD_PTSA_MAX_0));

	p->ftop_ptsa_min = readl(T12X_MC_RA(FTOP_PTSA_MIN_0));
	p->ftop_ptsa_max = readl(T12X_MC_RA(FTOP_PTSA_MAX_0));
}

static void t12x_init_ptsa(void)
{
	struct clk *emc_clk __attribute__((unused));
	unsigned long emc_freq_mhz __attribute__((unused));
	unsigned int mc_freq_mhz;
	unsigned long same_freq __attribute__((unused));
	struct ptsa_info *p = &cs->ptsa_info;
	unsigned int mpcorer_ptsa_rate_fp = 0;

	/* get emc frequency */
	emc_clk = clk_get(NULL, "emc");
	emc_freq_mhz = clk_get_rate(emc_clk) /
			T12X_LA_HZ_TO_MHZ_FACTOR;
	la_debug("**** emc clk_rate=%luMHz", emc_freq_mhz);

	/* get mc frequency */
	same_freq = (readl(T12X_MC_RA(EMEM_ARB_MISC0_0)) >>
			T12X_MC_EMC_SAME_FREQ_BIT) & 0x1;
	mc_freq_mhz = same_freq ? emc_freq_mhz : emc_freq_mhz / 2;
	la_debug("**** mc clk_rate=%uMHz", mc_freq_mhz);

	/* compute initial value for grant dec */
	p->ptsa_grant_dec = min(mc_freq_mhz *
				T12X_MAX_GRANT_DEC /
				T12X_MC_MAX_FREQ_MHZ,
				(unsigned int)T12X_MAX_GRANT_DEC);

	/* initialize PTSA reg values */
	/* FIXME:- Program inital ptsa rates */
	p->ve_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ve_ptsa_max = (unsigned int)(31) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->isp_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->isp_ptsa_max = (unsigned int)(31) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->ve2_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ve2_ptsa_max = (unsigned int)(31) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->a9avppc_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->a9avppc_ptsa_max = (unsigned int)(16) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->ring2_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ring2_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;
	p->ring2_ptsa_rate = 1 & T12X_MC_PTSA_RATE_DEFAULT_MASK;

	p->dis_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->dis_ptsa_max = (unsigned int)(31) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->disb_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->disb_ptsa_max = (unsigned int)(31) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->ring1_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ring1_ptsa_max = (unsigned int)(31) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->mpcorer_ptsa_min = (unsigned int)(-4) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->mpcorer_ptsa_max = (unsigned int)(4) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;
	/* allocate 10% for CPU read */
	mpcorer_ptsa_rate_fp = T12X_MPCORER_CPU_RD_MARGIN_FP *
				T12X_MAX_GRANT_DEC *
				emc_freq_mhz /
				T12X_EMC_MAX_FREQ_MHZ /
				T12X_MIN_CYCLES_PER_GRANT;
	/* between 200 and 400 MHz EMC freq, scale from 0 to 10% */
	if (emc_freq_mhz < 400)
		mpcorer_ptsa_rate_fp *= (emc_freq_mhz - 200) / (400 - 200);
	/* make sure mpcorer_ptsa_rate is at least 1 */
	if (emc_freq_mhz < 200 || mpcorer_ptsa_rate_fp < T12X_LA_FP_FACTOR)
		mpcorer_ptsa_rate_fp = T12X_LA_FP_FACTOR;
	/* round up */
	if (mpcorer_ptsa_rate_fp % T12X_LA_FP_FACTOR != 0)
		mpcorer_ptsa_rate_fp += T12X_LA_FP_FACTOR;
	p->mpcorer_ptsa_rate = T12X_LA_FP_TO_REAL(mpcorer_ptsa_rate_fp) &
				T12X_MC_PTSA_RATE_DEFAULT_MASK;

	p->smmu_ptsa_min = (unsigned int)(1) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->smmu_ptsa_max = (unsigned int)(1) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->r0_dis_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->r0_dis_ptsa_max = (unsigned int)(31) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->r0_disb_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->r0_disb_ptsa_max = (unsigned int)(31) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->vd_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->vd_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->mse_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->mse_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->gk_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->gk_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->vicpc_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->vicpc_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->apb_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->apb_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->pcx_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->pcx_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->host_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->host_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->ahb_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ahb_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->sax_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->sax_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->aud_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->aud_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->sd_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->sd_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->usbx_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->usbx_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->usbd_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->usbd_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->ftop_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ftop_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	p->avp_ptsa_min = (unsigned int)(-2) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->avp_ptsa_max = (unsigned int)(0) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;

	program_ptsa();
}

static void t12x_calc_disp_and_camera_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;
	unsigned int ve_bw_fp = cs->camera_bw_array[CAMERA_IDX(VI_W)] *
				T12X_DDA_BW_MARGIN_FP;
	unsigned int ve2_bw_fp = 0;
	unsigned int isp_bw_fp = 0;
	unsigned int total_dc0_bw_fp = total_dc0_bw *
					T12X_DDA_BW_MARGIN_FP;
	unsigned int total_dc1_bw_fp = total_dc1_bw *
					T12X_DDA_BW_MARGIN_FP;
	unsigned int low_freq_bw_fp = T12X_EMC_MIN_FREQ_MHZ_FP *
					2 *
					T12X_LA_DRAM_WIDTH_BITS /
					8;
	unsigned int dis_frac_fp = T12X_LA_FPA_TO_FP(
						T12X_LOW_GD_FPA *
						total_dc0_bw_fp /
						low_freq_bw_fp);
	unsigned int disb_frac_fp = T12X_LA_FPA_TO_FP(
						T12X_LOW_GD_FPA *
						total_dc1_bw_fp /
						low_freq_bw_fp);
	unsigned int ring1_bw_fp = total_dc0_bw_fp +
					total_dc1_bw_fp +
					ve_bw_fp;
	unsigned int total_iso_bw_fp = total_dc0_bw_fp + total_dc1_bw_fp;
	unsigned int siso_bw_fp = 0;
	int max_max = (1 << T12X_EMEM_PTSA_MINMAX_WIDTH) - 1;
	int i = 0;


	if (cs->agg_camera_array[AGG_CAMERA_ID(VE2)].is_hiso) {
		ve2_bw_fp = (cs->camera_bw_array[CAMERA_IDX(ISP_RAB)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WAB)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WBB)]) *
				T12X_DDA_BW_MARGIN_FP;
	} else {
		ve2_bw_fp = T12X_LA_REAL_TO_FP(
				cs->camera_bw_array[CAMERA_IDX(ISP_RAB)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WAB)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WBB)]);
	}

	if (cs->agg_camera_array[AGG_CAMERA_ID(ISP)].is_hiso) {
		isp_bw_fp = (cs->camera_bw_array[CAMERA_IDX(ISP_RA)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WA)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WB)]) *
				T12X_DDA_BW_MARGIN_FP;
	} else {
		isp_bw_fp = T12X_LA_REAL_TO_FP(
				cs->camera_bw_array[CAMERA_IDX(ISP_RA)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WA)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WB)]);
	}

	cs->agg_camera_array[AGG_CAMERA_ID(VE)].bw_fp = ve_bw_fp;
	cs->agg_camera_array[AGG_CAMERA_ID(VE2)].bw_fp = ve2_bw_fp;
	cs->agg_camera_array[AGG_CAMERA_ID(ISP)].bw_fp = isp_bw_fp;

	ring1_bw_fp += ve2_bw_fp + isp_bw_fp;


	for (i = 0; i < TEGRA_LA_AGG_CAMERA_NUM_CLIENTS; i++) {
		struct agg_camera_client_info *agg_client =
						&cs->agg_camera_array[i];

		if (agg_client->is_hiso) {
			agg_client->frac_fp = T12X_LA_FPA_TO_FP(
						T12X_LOW_GD_FPA *
						agg_client->bw_fp /
						low_freq_bw_fp);
			agg_client->ptsa_min = (unsigned int)(-5) &
						T12X_MC_PTSA_MIN_DEFAULT_MASK;
			agg_client->ptsa_max = (unsigned int)(max_max) &
						T12X_MC_PTSA_MAX_DEFAULT_MASK;

			total_iso_bw_fp += agg_client->bw_fp;
		} else {
			agg_client->frac_fp = T12X_1_DDA_FRAC_FP;
			agg_client->ptsa_min = (unsigned int)(-2) &
						T12X_MC_PTSA_MIN_DEFAULT_MASK;
			agg_client->ptsa_max = (unsigned int)(0) &
						T12X_MC_PTSA_MAX_DEFAULT_MASK;
		}
	}


	p->ring1_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ring1_ptsa_max = (unsigned int)(max_max) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;
	siso_bw_fp = total_iso_bw_fp / T12X_RING1_FEEDER_SISO_ALLOC_DIV;
	ring1_bw_fp += siso_bw_fp;
	p->ring1_ptsa_rate =
			(fraction2dda_fp(T12X_LOW_GD_FP *
						ring1_bw_fp /
						low_freq_bw_fp,
					4,
					T12X_MC_PTSA_RATE_DEFAULT_MASK) +
					1) &
			T12X_MC_PTSA_RATE_DEFAULT_MASK;
	if (p->ring1_ptsa_rate == 0)
		p->ring1_ptsa_rate = 0x1;

	p->dis_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->dis_ptsa_max = (unsigned int)(max_max) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;
	p->dis_ptsa_rate = fraction2dda_fp(dis_frac_fp,
					4,
					T12X_MC_PTSA_RATE_DEFAULT_MASK) &
					T12X_MC_PTSA_RATE_DEFAULT_MASK;

	p->disb_ptsa_min = (unsigned int)(-5) &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->disb_ptsa_max = (unsigned int)(max_max) &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;
	p->disb_ptsa_rate = fraction2dda_fp(disb_frac_fp,
					4,
					T12X_MC_PTSA_RATE_DEFAULT_MASK) &
					T12X_MC_PTSA_RATE_DEFAULT_MASK;

	p->ve_ptsa_min = cs->agg_camera_array[AGG_CAMERA_ID(VE)].ptsa_min &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ve_ptsa_max = cs->agg_camera_array[AGG_CAMERA_ID(VE)].ptsa_max &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;
	p->ve_ptsa_rate = fraction2dda_fp(
				cs->agg_camera_array[AGG_CAMERA_ID(VE)].frac_fp,
				4,
				T12X_MC_PTSA_RATE_DEFAULT_MASK) &
				T12X_MC_PTSA_RATE_DEFAULT_MASK;

	p->ve2_ptsa_min = cs->agg_camera_array[AGG_CAMERA_ID(VE2)].ptsa_min &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->ve2_ptsa_max = cs->agg_camera_array[AGG_CAMERA_ID(VE2)].ptsa_max &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;
	p->ve2_ptsa_rate = fraction2dda_fp(
			cs->agg_camera_array[AGG_CAMERA_ID(VE2)].frac_fp,
			4,
			T12X_MC_PTSA_RATE_DEFAULT_MASK) &
			T12X_MC_PTSA_RATE_DEFAULT_MASK;

	p->isp_ptsa_min = cs->agg_camera_array[AGG_CAMERA_ID(ISP)].ptsa_min &
					T12X_MC_PTSA_MIN_DEFAULT_MASK;
	p->isp_ptsa_max = cs->agg_camera_array[AGG_CAMERA_ID(ISP)].ptsa_max &
					T12X_MC_PTSA_MAX_DEFAULT_MASK;
	p->isp_ptsa_rate = fraction2dda_fp(
			cs->agg_camera_array[AGG_CAMERA_ID(ISP)].frac_fp,
			4,
			T12X_MC_PTSA_RATE_DEFAULT_MASK) &
			T12X_MC_PTSA_RATE_DEFAULT_MASK;
}

static void t12x_update_display_ptsa_rate(unsigned int *disp_bw_array)
{
	struct ptsa_info *p = &cs->ptsa_info;

	t12x_calc_disp_and_camera_ptsa();

	writel(p->ring1_ptsa_min, T12X_MC_RA(RING1_PTSA_MIN_0));
	writel(p->ring1_ptsa_max, T12X_MC_RA(RING1_PTSA_MAX_0));
	writel(p->ring1_ptsa_rate, T12X_MC_RA(RING1_PTSA_RATE_0));

	writel(p->dis_ptsa_min, T12X_MC_RA(DIS_PTSA_MIN_0));
	writel(p->dis_ptsa_max, T12X_MC_RA(DIS_PTSA_MAX_0));
	writel(p->dis_ptsa_rate, T12X_MC_RA(DIS_PTSA_RATE_0));

	writel(p->disb_ptsa_min, T12X_MC_RA(DISB_PTSA_MIN_0));
	writel(p->disb_ptsa_max, T12X_MC_RA(DISB_PTSA_MAX_0));
	writel(p->disb_ptsa_rate, T12X_MC_RA(DISB_PTSA_RATE_0));
}

static int t12x_update_camera_ptsa_rate(enum tegra_la_id id,
					unsigned int bw_mbps,
					int is_hiso)
{
	struct ptsa_info *p = NULL;
	int ret_code = 0;


	mutex_lock(&disp_and_camera_ptsa_lock);


	if (!is_camera_client(id)) {
		/* Non-camera clients should be handled by t12x_set_la(...) or
		   t12x_set_disp_la(...). */
		pr_err("%s: Ignoring request from a non-camera client.\n",
			__func__);
		pr_err("%s: Non-camera clients should be handled by "
			"t12x_set_la(...) or t12x_set_disp_la(...).\n",
			__func__);
		ret_code = -1;
		goto exit;
	}

	if ((id == ID(VI_W)) &&
		(!is_hiso)) {
		pr_err("%s: VI is stating that its not HISO.\n", __func__);
		pr_err("%s: Ignoring and assuming that VI is HISO because VI "
			"is always supposed to be HISO.\n",
			__func__);
		is_hiso = 1;
	}


	p = &cs->ptsa_info;

	if (id == ID(VI_W)) {
		cs->agg_camera_array[AGG_CAMERA_ID(VE)].is_hiso = is_hiso;
	} else if ((id == ID(ISP_RAB)) ||
			(id == ID(ISP_WAB)) ||
			(id == ID(ISP_WBB))) {
		cs->agg_camera_array[AGG_CAMERA_ID(VE2)].is_hiso = is_hiso;
	} else {
		cs->agg_camera_array[AGG_CAMERA_ID(ISP)].is_hiso = is_hiso;
	}

	cs->camera_bw_array[CAMERA_LA_IDX(id)] = bw_mbps;


	t12x_calc_disp_and_camera_ptsa();


	writel(p->ring1_ptsa_min, T12X_MC_RA(RING1_PTSA_MIN_0));
	writel(p->ring1_ptsa_max, T12X_MC_RA(RING1_PTSA_MAX_0));
	writel(p->ring1_ptsa_rate, T12X_MC_RA(RING1_PTSA_RATE_0));

	writel(p->ve_ptsa_min, T12X_MC_RA(VE_PTSA_MIN_0));
	writel(p->ve_ptsa_max, T12X_MC_RA(VE_PTSA_MAX_0));
	writel(p->ve_ptsa_rate, T12X_MC_RA(VE_PTSA_RATE_0));

	writel(p->ve2_ptsa_min, T12X_MC_RA(VE2_PTSA_MIN_0));
	writel(p->ve2_ptsa_max, T12X_MC_RA(VE2_PTSA_MAX_0));
	writel(p->ve2_ptsa_rate, T12X_MC_RA(VE2_PTSA_RATE_0));

	writel(p->isp_ptsa_min, T12X_MC_RA(ISP_PTSA_MIN_0));
	writel(p->isp_ptsa_max, T12X_MC_RA(ISP_PTSA_MAX_0));
	writel(p->isp_ptsa_rate, T12X_MC_RA(ISP_PTSA_RATE_0));


exit:
	mutex_unlock(&disp_and_camera_ptsa_lock);

	return ret_code;
}


static void program_scaled_la(struct la_client_info *ci, int la)
{
	unsigned long reg_write;

	if (ci->id == ID(DISPLAY_0A)) {
		reg_write = ((la << T12X_MC_SCALED_LA_DISPLAY0A_0_LOW_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0A_0_LOW_MASK) |
			((la << T12X_MC_SCALED_LA_DISPLAY0A_0_HIGH_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0A_0_HIGH_MASK);
		writel(reg_write, T12X_MC_SCALED_LA_DISPLAY0A_0);
		la_debug("reg_addr=0x%x, write=0x%x",
		(u32)T12X_MC_SCALED_LA_DISPLAY0A_0, (u32)reg_write);
	} else if (ci->id == ID(DISPLAY_0AB)) {
		reg_write = ((la << T12X_MC_SCALED_LA_DISPLAY0AB_0_LOW_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0AB_0_LOW_MASK) |
			((la << T12X_MC_SCALED_LA_DISPLAY0AB_0_HIGH_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0AB_0_HIGH_MASK);
		writel(reg_write, T12X_MC_SCALED_LA_DISPLAY0AB_0);
		la_debug("reg_addr=0x%x, write=0x%x",
		(u32)T12X_MC_SCALED_LA_DISPLAY0AB_0, (u32)reg_write);
	} else if (ci->id == ID(DISPLAY_0B)) {
		reg_write = ((la << T12X_MC_SCALED_LA_DISPLAY0B_0_LOW_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0B_0_LOW_MASK) |
			((la << T12X_MC_SCALED_LA_DISPLAY0B_0_HIGH_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0B_0_HIGH_MASK);
		writel(reg_write, T12X_MC_SCALED_LA_DISPLAY0B_0);
		la_debug("reg_addr=0x%x, write=0x%x",
		(u32)T12X_MC_SCALED_LA_DISPLAY0B_0, (u32)reg_write);
	} else if (ci->id == ID(DISPLAY_0BB)) {
		reg_write = ((la << T12X_MC_SCALED_LA_DISPLAY0BB_0_LOW_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0BB_0_LOW_MASK) |
			((la << T12X_MC_SCALED_LA_DISPLAY0BB_0_HIGH_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0BB_0_HIGH_MASK);
		writel(reg_write, T12X_MC_SCALED_LA_DISPLAY0BB_0);
		la_debug("reg_addr=0x%x, write=0x%x",
		(u32)T12X_MC_SCALED_LA_DISPLAY0BB_0, (u32)reg_write);
	} else if (ci->id == ID(DISPLAY_0C)) {
		reg_write = ((la << T12X_MC_SCALED_LA_DISPLAY0C_0_LOW_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0C_0_LOW_MASK) |
			((la << T12X_MC_SCALED_LA_DISPLAY0C_0_HIGH_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0C_0_HIGH_MASK);
		writel(reg_write, T12X_MC_SCALED_LA_DISPLAY0C_0);
		la_debug("reg_addr=0x%x, write=0x%x",
		(u32)T12X_MC_SCALED_LA_DISPLAY0C_0, (u32)reg_write);
	} else if (ci->id == ID(DISPLAY_0CB)) {
		reg_write = ((la << T12X_MC_SCALED_LA_DISPLAY0CB_0_LOW_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0CB_0_LOW_MASK) |
			((la << T12X_MC_SCALED_LA_DISPLAY0CB_0_HIGH_SHIFT) &
			T12X_MC_SCALED_LA_DISPLAY0CB_0_HIGH_MASK);
		writel(reg_write, T12X_MC_SCALED_LA_DISPLAY0CB_0);
		la_debug("reg_addr=0x%x, write=0x%x",
		(u32)T12X_MC_SCALED_LA_DISPLAY0CB_0, (u32)reg_write);
	}
}
static void program_la(struct la_client_info *ci, int la)
{
	unsigned long reg_read;
	unsigned long reg_write;

	BUG_ON(la > T12X_MC_LA_MAX_VALUE);

	spin_lock(&cs->lock);
	reg_read = readl(ci->reg_addr);
	reg_write = (reg_read & ~ci->mask) |
			(la << ci->shift);
	writel(reg_write, ci->reg_addr);
	ci->la_set = la;
	la_debug("reg_addr=0x%x, read=0x%x, write=0x%x",
		(u32)ci->reg_addr, (u32)reg_read, (u32)reg_write);

	program_scaled_la(ci, la);

	spin_unlock(&cs->lock);
}

static int t12x_set_la(enum tegra_la_id id,
			unsigned int bw_mbps)
{
	int idx = cs->id_to_index[id];
	struct la_client_info *ci = &cs->la_info_array[idx];
	unsigned int la_to_set = 0;

	if (is_display_client(id)) {
		/* Display clients should be handled by
		   t12x_set_disp_la(...). */
		return -1;
	} else if (id == ID(MSENCSRD)) {
		/* This is a special case. */
		struct clk *emc_clk = clk_get(NULL, "emc");
		unsigned int emc_freq_mhz = clk_get_rate(emc_clk) /
						T12X_LA_HZ_TO_MHZ_FACTOR;
		unsigned int val_1 = 53;
		unsigned int val_2 = 24;

		if (210 > emc_freq_mhz)
			val_1 = val_1 * 210 / emc_freq_mhz;

		if (574 > emc_freq_mhz)
			val_2 = val_2 * 574 / emc_freq_mhz;

		la_to_set = min3((unsigned int)T12X_MC_LA_MAX_VALUE,
				val_1,
				val_2);
	} else if (ci->la_ref_clk_mhz != 0) {
		/* In this case we need to scale LA with emc frequency. */
		struct clk *emc_clk = clk_get(NULL, "emc");
		unsigned long emc_freq_mhz = clk_get_rate(emc_clk) /
					(unsigned long)T12X_LA_HZ_TO_MHZ_FACTOR;

		if (ci->la_ref_clk_mhz <= emc_freq_mhz) {
			la_to_set = min(ci->init_la,
				(unsigned int)T12X_MC_LA_MAX_VALUE);
		} else {
			la_to_set = min((unsigned int)(ci->init_la *
					 ci->la_ref_clk_mhz / emc_freq_mhz),
				(unsigned int)T12X_MC_LA_MAX_VALUE);
		}
	} else {
		/* In this case we have a client with a static LA value. */
		la_to_set = ci->init_la;
	}

	program_la(ci, la_to_set);
	return 0;
}

static int t12x_set_disp_la(enum tegra_la_id id,
				unsigned int bw_mbps,
				struct dc_to_la_params disp_params)
{
	int idx = 0;
	struct la_client_info *ci = NULL;
	unsigned int la_to_set = 0;
	struct clk *emc_clk = NULL;
	unsigned long emc_freq_mhz = 0;
	unsigned int dvfs_time_nsec = 0;
	unsigned int dvfs_buffering_reqd_bytes = 0;
	unsigned int thresh_dvfs_bytes = 0;
	unsigned int total_buf_sz_bytes = 0;
	unsigned int effective_mccif_buf_sz = 0;
	unsigned int la_bw_upper_bound_nsec_fp = 0;
	unsigned int la_bw_upper_bound_nsec = 0;
	unsigned int la_nsec = 0;

	if (!is_display_client(id)) {
		/* Non-display clients should be handled by t12x_set_la(...). */
		return -1;
	}

	mutex_lock(&disp_and_camera_ptsa_lock);
	total_dc0_bw = disp_params.total_dc0_bw;
	total_dc1_bw = disp_params.total_dc1_bw;
	cs->update_display_ptsa_rate(cs->disp_bw_array);
	mutex_unlock(&disp_and_camera_ptsa_lock);

	idx = cs->id_to_index[id];
	ci = &cs->la_info_array[idx];
	la_to_set = 0;
	emc_clk = clk_get(NULL, "emc");
	emc_freq_mhz = clk_get_rate(emc_clk) /
			T12X_LA_HZ_TO_MHZ_FACTOR;
	dvfs_time_nsec = tegra_get_dvfs_time_nsec(emc_freq_mhz);
	dvfs_buffering_reqd_bytes = bw_mbps *
					dvfs_time_nsec /
					T12X_LA_USEC_TO_NSEC_FACTOR;
	thresh_dvfs_bytes =
			disp_params.thresh_lwm_bytes +
			dvfs_buffering_reqd_bytes +
			disp_params.spool_up_buffering_adj_bytes;
	total_buf_sz_bytes =
		cs->disp_clients[DISP_CLIENT_LA_ID(id)].line_buf_sz_bytes +
		cs->disp_clients[DISP_CLIENT_LA_ID(id)].mccif_size_bytes;
	effective_mccif_buf_sz =
		(cs->disp_clients[DISP_CLIENT_LA_ID(id)].line_buf_sz_bytes >
		thresh_dvfs_bytes) ?
		cs->disp_clients[DISP_CLIENT_LA_ID(id)].mccif_size_bytes :
		total_buf_sz_bytes - thresh_dvfs_bytes;

	la_bw_upper_bound_nsec_fp = effective_mccif_buf_sz *
					T12X_LA_FP_FACTOR /
					bw_mbps;
	la_bw_upper_bound_nsec_fp = la_bw_upper_bound_nsec_fp *
					T12X_LA_FP_FACTOR /
					T12X_LA_DISP_CATCHUP_FACTOR_FP;
	la_bw_upper_bound_nsec_fp =
		la_bw_upper_bound_nsec_fp -
		(T12X_LA_STATIC_LA_MINUS_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP +
		T12X_EXP_TIME_EMCCLKS_FP) /
		emc_freq_mhz;
	la_bw_upper_bound_nsec_fp *= T12X_LA_USEC_TO_NSEC_FACTOR;
	la_bw_upper_bound_nsec = T12X_LA_FP_TO_REAL(
						la_bw_upper_bound_nsec_fp);


	la_nsec = min(la_bw_upper_bound_nsec,
			(unsigned int)T12X_MAX_LA_NSEC);

	la_to_set = min(la_nsec / cs->ns_per_tick,
			(unsigned int)T12X_MC_LA_MAX_VALUE);

	program_la(ci, la_to_set);
	return 0;
}

static int t12x_la_suspend(void)
{
	int i = 0;
	struct la_client_info *ci = NULL;

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

static void t12x_la_resume(void)
{
	int i;

	for (i = 0; i < cs->la_info_array_size; i++) {
		if (cs->la_info_array[i].la_set)
			program_la(&cs->la_info_array[i],
					cs->la_info_array[i].la_set);
	}
	program_ptsa();
}

void tegra_la_get_t12x_specific(struct la_chip_specific *cs_la)
{
	int i = 0;

	cs_la->ns_per_tick = 30;
	cs_la->atom_size = 64;
	cs_la->la_max_value = T12X_MC_LA_MAX_VALUE;
	cs_la->la_info_array = t12x_la_info_array;
	cs_la->la_info_array_size = ARRAY_SIZE(t12x_la_info_array);

	cs_la->la_params.fp_factor = T12X_LA_FP_FACTOR;
	cs_la->la_params.la_real_to_fp = tegra12x_la_real_to_fp;
	cs_la->la_params.la_fp_to_real = tegra12x_la_fp_to_real;
	cs_la->la_params.static_la_minus_snap_arb_to_row_srt_emcclks_fp =
			T12X_LA_STATIC_LA_MINUS_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP;
	cs_la->la_params.dram_width_bits = T12X_LA_DRAM_WIDTH_BITS;
	cs_la->la_params.disp_catchup_factor_fp =
						T12X_LA_DISP_CATCHUP_FACTOR_FP;

	cs_la->init_ptsa = t12x_init_ptsa;
	cs_la->update_display_ptsa_rate = t12x_update_display_ptsa_rate;
	cs_la->update_camera_ptsa_rate = t12x_update_camera_ptsa_rate;
	cs_la->set_la = t12x_set_la;
	cs_la->set_disp_la = t12x_set_disp_la;
	cs_la->suspend = t12x_la_suspend;
	cs_la->resume = t12x_la_resume;
	cs = cs_la;

	tegra_la_disp_clients_info = cs_la->disp_clients;

	/* set some entries to zero */
	for (i = 0; i < NUM_CAMERA_CLIENTS; i++)
		cs_la->camera_bw_array[i] = 0;
	for (i = 0; i < TEGRA_LA_AGG_CAMERA_NUM_CLIENTS; i++) {
		cs_la->agg_camera_array[i].bw_fp = 0;
		cs_la->agg_camera_array[i].frac_fp = 0;
		cs_la->agg_camera_array[i].ptsa_min = 0;
		cs_la->agg_camera_array[i].ptsa_max = 0;
		cs_la->agg_camera_array[i].is_hiso = false;
	}

	/* set mccif_size_bytes values */
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0A)].mccif_size_bytes = 4096;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0B)].mccif_size_bytes = 5760;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0C)].mccif_size_bytes = 4096;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HC)].mccif_size_bytes = 2048;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0AB)].mccif_size_bytes =
									4096;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0BB)].mccif_size_bytes =
									4096;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0CB)].mccif_size_bytes =
									4096;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HCB)].mccif_size_bytes =
									2048;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_T)].mccif_size_bytes = 4096;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAYD)].mccif_size_bytes = 4096;

	/* set line_buf_sz_bytes values */
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0A)].line_buf_sz_bytes =
									133632;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0B)].line_buf_sz_bytes =
									53248;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0C)].line_buf_sz_bytes =
									53248;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HC)].line_buf_sz_bytes = 320;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0AB)].line_buf_sz_bytes =
									49152;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0BB)].line_buf_sz_bytes =
									49152;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0CB)].line_buf_sz_bytes =
									49152;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HCB)].line_buf_sz_bytes =
									320;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_T)].line_buf_sz_bytes = 6144;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAYD)].line_buf_sz_bytes = 6144;

	/* set win_type values */
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0A)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULL;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0B)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLA;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0C)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLA;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HC)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_CURSOR;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0AB)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLB;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0BB)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLB;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0CB)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLB;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HCB)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_CURSOR;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_T)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_SIMPLE;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAYD)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_SIMPLE;
}
