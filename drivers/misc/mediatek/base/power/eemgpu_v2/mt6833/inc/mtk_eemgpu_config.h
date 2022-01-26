/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _MTK_EEMG_CONFIG_H_
#define _MTK_EEMG_CONFIG_H_

#include "mtk_eemgpu_prj_config.h"

#define EEMG_NOT_READY		(0)
#define LOG_INTERVAL		(2LL * NSEC_PER_SEC)
#define SUPPORT_DCONFIG		(1)
#define ENABLE_HT_FT		(1)
#define ENABLE_MINIHQA		(0)

#define NR_HW_RES_FOR_BANK	(18) /* real eem banks for efuse */
#define EEMG_INIT01_FLAG	(0x01) /* 0x01=> [0]:GPU */
#define EEMG_CORNER_FLAG	(0x30) /* 0x30=> [5]:VPU, [4]:MDLA */


#define CORESEL_VAL			(0x8fff0000)
#define CORESEL_INIT2_VAL	(0x0fff0000)

/* for EEMCTL0's setting */
#define EEMG_CTL0_GPU		(0x00100003)


/*
 * ##########################
 * eemsng config
 * ##########################
 */
#define BANK_GPU_TURN_PT	9

/* eemsng parameter */
#define DETWINDOW_VAL	0xA28
#define DTHI_VAL		(0x01) /* positive */
#define DTLO_VAL		(0xfe) /* negative (2's compliment) */
#define DETMAX_VAL		(0xffff)
#define AGECONFIG_VAL	(0x555555)
#define AGEM_VAL		(0x0)
#define DCCONFIG_VAL	(0x1)
#define MTS_VAL			(0x1fb)
#define BTS_VAL			(0x6d1)


#define VBOOT_VAL		(0x38) /* volt domain: 0.75v */

#define VMAX_VAL_GPU	(0x48) /* eem domain: 0.85v*/
#define VMIN_VAL_GPU    (0x18) /* eem domain: 0.55v*/
#define VCO_VAL_GPU     (0x10) /* eem domain: 0.5v*/

/* different for GPU_L */
#define VMAX_VAL_GL     (0x48) /* eem domain: 0.85v*/
#define VMIN_VAL_GL     (0x18) /* eem domain: 0.55v*/
#define VCO_VAL_GL      (0x10) /* eem domain: 0.5v*/
#define DVTFIXED_VAL_GL		(0x01)
#define DVTFIXED_VAL_GPU	(0x05)

/* different for GPU_H */
#define VMAX_VAL_GH     (0x48) /* eem domain: 0.85v*/
#define VMIN_VAL_GH     (0x18) /* eem domain: 0.55v*/
#define VCO_VAL_GH      (0x10) /* eem domain: 0.5v*/

/* check temperature upper/lower bound */
#define LOW_TEMP_OFF_DEFAULT	(0)
#define LOW_TEMP_OFF_GPU		(4)
#define HIGH_TEMP_OFF_GPU		(3)
#define EXTRA_LOW_TEMP_OFF_GPU	(7)


/* pmic setting */
/* 1mV=>10uV */
/* EEM */
#define EEMG_V_BASE		(40000)
#define EEMG_STEP		(625)

/* GPU */
#define GPU_PMIC_BASE	(40000)
#define GPU_PMIC_STEP	(625)


#if DVT
#define DUMP_LEN	417
#else
#define DUMP_LEN	109
#endif


/*
 * ##########################
 * safe efuse
 * ##########################
 */
#define DEVINFO_IDX_0 50
#define DEVINFO_IDX_1 51
#define DEVINFO_IDX_2 52
#define DEVINFO_IDX_3 53
#define DEVINFO_IDX_4 54
#define DEVINFO_IDX_5 55
#define DEVINFO_IDX_6 56
#define DEVINFO_IDX_7 57
#define DEVINFO_IDX_8 58
#define DEVINFO_IDX_9 59
#define DEVINFO_IDX_10 60
#define DEVINFO_IDX_11 61
#define DEVINFO_IDX_12 62
#define DEVINFO_IDX_13 63
#define DEVINFO_IDX_14 64
#define DEVINFO_IDX_15 65
#define DEVINFO_IDX_16 66
#define DEVINFO_IDX_17 67


#if defined(CMD_LOAD)
/* Safe EFUSE */
#define DEVINFO_0 0x00060006
/* L_LO */
#define DEVINFO_1 0x0
/* B_LO + L_LO */
#define DEVINFO_2 0x000000AF
/* B_LO */
#define DEVINFO_3 0x9B0B0363
/* CCI */
#define DEVINFO_4 0x9B0B0769
/* GPU_LO + CCI */
#define DEVINFO_5 0x00A100A6
/* GPU_LO */
#define DEVINFO_6 0x9B0BBF96
/* APU */
#define DEVINFO_7 0x10bd3c1b
/* L_HI + APU */
#define DEVINFO_8 0x550055
/* L_HI */
#define DEVINFO_9 0x10bd3c1b
/* B_HI */
#define DEVINFO_10 0x9B0B2263
/* MODEM + B_HI */
#define DEVINFO_11 0x00B900AC
/* MODEM */
#define DEVINFO_12 0x570B166E
/* L */
#define DEVINFO_16 0x9B0B0866
/* B + L */
#define DEVINFO_17 0x00A100BB
/* B */
#define DEVINFO_18 0x9B0B2263
/* MDLA */
#define DEVINFO_19 0x9B0BD594
/* GPU + MDLA */
#define DEVINFO_23 0x00B100B1
/* GPU */
#define DEVINFO_24 0x9B0BC56E

#elif defined(MC50_LOAD)
/* MC50 Safe EFUSE */
#define DEVINFO_0 0x00020002
#define DEVINFO_1 0x00000000
#define DEVINFO_2 0x00000000
#define DEVINFO_3 0x36173453
#define DEVINFO_4 0x41123450
#define DEVINFO_5 0x6AE33450
#define DEVINFO_6 0x3B17342C
#define DEVINFO_7 0x4BEE3450
#define DEVINFO_8 0x3A14342C
#define DEVINFO_9 0xFC542CE8
#define DEVINFO_10 0xABAF2C87
#define DEVINFO_11 0x00000000
#define DEVINFO_12 0x65E93428
#define DEVINFO_13 0x00000000
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0xA8FBA8FB

#else

/* MC99 Safe EFUSE */
#define DEVINFO_0 0x0
#define DEVINFO_1 0x5C1A2C25
#define DEVINFO_2 0x22082C3F
#define DEVINFO_3 0x46172C0A
#define DEVINFO_4 0x411A2C20
#define DEVINFO_5 0x6AEB2C20
#define DEVINFO_6 0x3B1F2C36
#define DEVINFO_7 0x4B162C20
#define DEVINFO_8 0x3A1C2C36
#define DEVINFO_9 0xC15B249E	/* GPU efuse */
#define DEVINFO_10 0x82AA24BA	/* GPU efuse */
#define DEVINFO_11 0x00000000
#define DEVINFO_12 0x5F052C39
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0xA8FBA8FB	/* GPU efuse */

#endif

#endif
