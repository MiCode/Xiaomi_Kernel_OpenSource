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
#ifndef _MTK_EEM_CONFIG_H_
#define _MTK_EEM_CONFIG_H_

#include "mtk_eem_prj_config.h"


/* CONFIG (SW related) */
#define EEM_NOT_READY		(0)
#define EARLY_PORTING		(0)
#define LOG_INTERVAL		(100LL * MSEC_PER_SEC)


/*
 * ##########################
 * eemsn config
 * ############################
 */
#define SUPPORT_DCONFIG		(1)
#define SUPPORT_PICACHU		(1)
#define EEM_IPI_ENABLE		(1)
#define ENABLE_INIT_TIMER	(1)

#define SET_PMIC_VOLT_TO_DVFS	(1)
#define UPDATE_TO_UPOWER	(1)

#define NR_HW_RES_FOR_BANK (22)	/* real eemsn banks for efuse */
#define IDX_HW_RES_SN (14)	/* start index of Sensor Network efuse */

#define NR_FREQ 16
#define NR_FREQ_CPU 16
#define NR_PI_VF 6

/* 1mV=>10uV */
/* EEMSN */
#define EEMSN_V_BASE	(625)
#define EEMSN_STEP		(625)

/* CPU */
#define CPU_PMIC_BASE	(0)
#define CPU_PMIC_STEP	(625)


/*
 * ##########################
 * SN config
 * ############################
 */
#define NUM_SN_CPU					(8)

/* SN dump data */
#if FULL_REG_DUMP_SNDATA
#define SIZE_SN_MCUSYS_REG			(17)
#else
#define SIZE_SN_MCUSYS_REG			(12)
#endif

#define SIZE_SN_DUMP_SENSOR			(64)
#define SIZE_SN_DUMP_CPE			(19)
#define MIN_SIZE_SN_DUMP_CPE		(7)


/*
 * ##########################
 * safe efuse
 * ############################
 */
#define DEVINFO_HRID_0 12
#define DEVINFO_SEG_IDX 30

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
#define DEVINFO_IDX_14 165
#define DEVINFO_IDX_15 166
#define DEVINFO_IDX_16 167
#define DEVINFO_IDX_17 168
#define DEVINFO_IDX_18 169
#define DEVINFO_IDX_19 170
#define DEVINFO_IDX_20 173
#define DEVINFO_IDX_21 174


#define DEVINFO_TIME_IDX 132

#if defined(MC50_LOAD)

#define DEVINFO_0 0x0
#define DEVINFO_1 0x6610243A
#define DEVINFO_2 0x98EB243A
#define DEVINFO_3 0x41122430
#define DEVINFO_4 0x70152420
#define DEVINFO_5 0x9AE52420
#define DEVINFO_6 0x26162438
#define DEVINFO_7 0x9AE52420
#define DEVINFO_8 0x27162438
#define DEVINFO_9 0x5DEF2459
#define DEVINFO_10 0x30162408
#define DEVINFO_11 0xB3E1243A
#define DEVINFO_12 0xD1F4243A
#define DEVINFO_13 0x1B031B03

#else

#define DEVINFO_0 0x0
#define DEVINFO_1 0x00000000
#define DEVINFO_2 0x00000000
#define DEVINFO_3 0x00000000
#define DEVINFO_4 0x00000000
#define DEVINFO_5 0x00000000
#define DEVINFO_6 0x00000000
#define DEVINFO_7 0x00000000
#define DEVINFO_8 0x00000000
#define DEVINFO_9 0x00000000
#define DEVINFO_10 0x00000000
#define DEVINFO_11 0x00000000
#define DEVINFO_12 0x00000000
#define DEVINFO_13 0x00000000
#endif

#define DEVINFO_14 0x1BC415BE
#define DEVINFO_15 0x15BE20F6
#define DEVINFO_16 0x20F61BC4
#define DEVINFO_17 0x396D396D
#define DEVINFO_18 0x3C6C3C6C
#define DEVINFO_19 0x00003210
#define DEVINFO_20 0x0000727C
#define DEVINFO_21 0x00000080

#endif
