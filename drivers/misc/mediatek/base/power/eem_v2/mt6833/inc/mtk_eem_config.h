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

#define NR_HW_RES_FOR_BANK (24)	/* real eemsn banks for efuse */
#define IDX_HW_RES_SN (18)	/* start index of Sensor Network efuse */

#define NR_FREQ 16
#define NR_FREQ_CPU 16
#define NR_PI_VF 6

/* 1mV=>10uV */
/* EEMSN */
#define EEMSN_V_BASE	(40000)
#define EEMSN_STEP		(625)

/* CPU */
#define CPU_PMIC_BASE	(40000)
#define CPU_PMIC_STEP	(625) /* 1.231/1024=0.001202v=120(10uv)*/


/*
 * ##########################
 * SN config
 * ############################
 */
#define NUM_SN_CPU					(8)

/* SN dump data */
#if FULL_REG_DUMP_SNDATA
#define SIZE_SN_MCUSYS_REG			(16)
#else
#define SIZE_SN_MCUSYS_REG			(10)
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
#define DEVINFO_IDX_14 64
#define DEVINFO_IDX_15 65
#define DEVINFO_IDX_16 66
#define DEVINFO_IDX_17 67
#define DEVINFO_IDX_18 68
#define DEVINFO_IDX_19 69
#define DEVINFO_IDX_20 70
#define DEVINFO_IDX_21 71
#define DEVINFO_IDX_22 72
#define DEVINFO_IDX_23 73
#define DEVINFO_IDX_24 74
#define DEVINFO_IDX_25 208
#define DEVINFO_IDX_27 210

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
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0x1B031B03


#else

#define DEVINFO_0 0x0
#define DEVINFO_1 0x5C1A2C25
#define DEVINFO_2 0x22082C3F
#define DEVINFO_3 0x46172C0A
#define DEVINFO_4 0x411A2C20
#define DEVINFO_5 0x6AEB2C20
#define DEVINFO_6 0x3B1F2C36
#define DEVINFO_7 0x4B162C20
#define DEVINFO_8 0x3A1C2C36
#define DEVINFO_9 0xC15B249E
#define DEVINFO_10 0x82AA24BA
#define DEVINFO_11 0x00000000
#define DEVINFO_12 0x5F052C39
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0xA8FBA8FB

#endif

#define DEVINFO_21 0x445E5245
#define DEVINFO_22 0x00005E50
#define DEVINFO_23 0x013C6764
#define DEVINFO_24 0x3B6B3C6C
#define DEVINFO_25 0x3A6D3B6E
#define DEVINFO_27 0x936F8268
#endif
