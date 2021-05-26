/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MTK_EEM_CONFIG_H_
#define _MTK_EEM_CONFIG_H_


/* CONFIG (SW related) */
#ifdef DISABLE_EEM
#define EEM_NOT_READY		(1)
#endif

#define CONFIG_EEM_SHOWLOG	(0)
#define EN_ISR_LOG		(0)
#define FULL_REG_DUMP_SNDATA	(0)
#define EARLY_PORTING		(0)

#define EEM_ENABLE		(1) /* enable; after pass HPT mini-SQC */
#define SN_ENABLE			(1)
#define EEM_IPI_ENABLE		(1)
#define VMIN_PREDICT_ENABLE	(0)

/* FIX ME */
#define EEM_FAKE_EFUSE		(0)
#define UPDATE_TO_UPOWER	(1)
#define EEM_LOCKTIME_LIMIT	(3000)
#define SUPPORT_PICACHU		(1)
#define SUPPORT_PI_LOG_AREA		(0)

#define ENABLE_INIT_TIMER	(1)


#define SET_PMIC_VOLT_TO_DVFS	(1)
#define LOG_INTERVAL		(100LL * MSEC_PER_SEC)
#define DVT			(0)
#define SUPPORT_DCONFIG		(1)
#define ENABLE_HT_FT		(1)
//#define EARLY_PORTING_VPU
#define EN_TEST_EQUATION	(0)
#define FAKE_SN_DVT_EFUSE_FOR_DE	(0)
#define ENABLE_COUNT_SNTEMP		(1)

#if DVT
#define DUMP_LEN		410
#else
#define DUMP_LEN		105
#endif


/* Sensor network configuration */
#define SIZE_REG_DUMP_ADDR_OFF		(105)
#if FULL_REG_DUMP_SNDATA
#define SIZE_SN_MCUSYS_REG			(16)
#else
#define SIZE_SN_MCUSYS_REG			(10)
#endif



#define SIZE_REG_DUMP_COMPAREDVOP	(5)
#define SIZE_REG_DUMP_SENSORMINDATA	(64)
#define SIZE_SN_COEF				(53)
#define SIZE_SN_DUMP_SENSOR			(64)
#define SIZE_SN_DUMP_CPE			(19)
#define TOTEL_SN_COEF_VER			(2)
#define TOTEL_SN_DBG_NUM			(5)
#define MIN_SIZE_SN_DUMP_CPE			(7)



#define NUM_SN_CPU			(8)



enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_L,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

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

#define SPARE1_VAL	0x44535F53
#define SPARE2_VAL	0x00003839

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

#define SPARE1_VAL	0x44535F53
#define SPARE2_VAL	0x00003839

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

#endif

#define DEVINFO_21 0x65786E64
#define DEVINFO_22 0x0000796F
#define DEVINFO_23 0x013C6764
#define DEVINFO_24 0x38673966
#define DEVINFO_25 0x33683467
#define DEVINFO_27 0x756A7767



/*******************************************
 * eemsn sw setting
 ********************************************
 */
#define NR_HW_RES_FOR_BANK (24) /* real eemsn banks for efuse */
#define IDX_HW_RES_SN (18) /* start index of Sensor Network efuse */

#define NR_FREQ 16
#define NR_FREQ_CPU 16
#define NR_PI_VF 6


#define L_FREQ_BASE			2000000
#define B_FREQ_BASE			2210000
#define	CCI_FREQ_BASE		1400000
#define L_M_FREQ_BASE		1500000
#define B_M_FREQ_BASE		1650000
#define CCI_M_FREQ_BASE		1050000

#define BANK_L_TURN_PT		0
#define BANK_B_TURN_PT		6


#define SN_V_BASE		(50000)
#define SN_V_DENOM		(110000 - 50000)

/*
 * 100 us, This is the EEMSN Detector sampling time as represented in
 * cycles of bclk_ck during INIT. 52 MHz
 */
#define DETWINDOW_VAL		0xA28

/*
 * mili Volt to config value. voltage = 600mV + val * 6.25mV
 * val = (voltage - 600) / 6.25
 * @mV: mili volt
 */

/* 1mV=>10uV */
/* EEMSN */
#define EEMSN_V_BASE		(40000)
#define EEMSN_STEP		(625)

/* CPU */
#define CPU_PMIC_BASE	(40000)
#define CPU_PMIC_BASE2	(40000)

#define CPU_PMIC_STEP		(625) /* 1.231/1024=0.001202v=120(10uv)*/

#define LOW_TEMP_OFF		(8)
#define HIGH_TEMP_OFF			(3)
#define AGING_VAL_CPU		(0x6) /* CPU aging margin : 37mv*/
#define AGING_VAL_CPU_B		(0x6) /* CPU aging margin : 37mv*/


#endif
