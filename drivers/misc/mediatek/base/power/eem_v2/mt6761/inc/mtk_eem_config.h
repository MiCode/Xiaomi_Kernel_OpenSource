/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */
#ifndef _MTK_EEM_CONFIG_H_
#define _MTK_EEM_CONFIG_H_

/* CONFIG (SW related) */
/* #define EEM_NOT_READY (1) *//* for bring up, remove for MP */
#define CONFIG_EEM_SHOWLOG (0)
#define EN_ISR_LOG (0)
#define EEM_BANK_SOC (0) /* use voltage bin, so disable it */
#define EARLY_PORTING (0) /* for detecting real vboot in eem_init01 */
#define DUMP_DATA_TO_DE (1)
#ifdef MC50_LOAD
    #define EEM_FAKE_EFUSE (1)
#else
    #define EEM_FAKE_EFUSE (0)
#endif
/* FIX ME */
#define UPDATE_TO_UPOWER (1)
#define EEM_LOCKTIME_LIMIT (3000)
#define ENABLE_EEMCTL0 (1)
#define ENABLE_LOO			(0)
#define ENABLE_INIT1_STRESS (1)

#define EEM_OFFSET
#define SET_PMIC_VOLT (1)
#define SET_PMIC_VOLT_TO_DVFS (1)
#define LOG_INTERVAL	(2LL * NSEC_PER_SEC)

enum mt_cpu_dvfs_id {
MT_CPU_DVFS_LL,
//MT_CPU_DVFS_L,
//MT_CPU_DVFS_CCI,
NR_MT_CPU_DVFS,
};


#define DEVINFO_IDX_0 50	/* 580 */
#define DEVINFO_IDX_1 51	/* 584 */
#define DEVINFO_IDX_2 52	/* 588 */
#define DEVINFO_IDX_3 53	/* 58C */
#define DEVINFO_IDX_4 54	/* 590 */
#define DEVINFO_IDX_5 55	/* 594 */
#define DEVINFO_IDX_6 56	/* 598 */
#define DEVINFO_IDX_7 57	/* 59C */
#define DEVINFO_IDX_8 58	/* 5A0 */
#define DEVINFO_IDX_9 61	/* 5A4 */
#define DEVINFO_IDX_10 62	/* 5A8 */
#define DEVINFO_IDX_11 63	/* 5AC */

#define DEVINFO_OFF_0 0xc8	/* 580 */
#define DEVINFO_OFF_1 0xcc	/* 584 */
#define DEVINFO_OFF_2 0xd0	/* 588 */
#define DEVINFO_OFF_3 0xd4	/* 58C */
#define DEVINFO_OFF_4 0xd8	/* 590 */
#define DEVINFO_OFF_5 0xdc	/* 594 */
#define DEVINFO_OFF_6 0xe0	/* 598 */
#define DEVINFO_OFF_7 0xe4	/* 59C */
#define DEVINFO_OFF_8 0xe8	/* 5A0 */
#define DEVINFO_OFF_9 0xf4	/* 5A4 */
#define DEVINFO_OFF_10 0xf8	/* 5A8 */
#define DEVINFO_OFF_11 0xfc	/* 5AC */

/*****************************************
 * eem sw setting
 ******************************************
 */
#define NR_HW_RES_FOR_BANK	(6) /* real eem banks for efuse */
#define EEM_INIT01_FLAG (0x1) /* [0]:LL */
#if ENABLE_LOO
#define EEM_L_INIT02_FLAG (0x9) /* should be 0x0F=> [3]:L_HI, [0]:L */
#define EEM_2L_INIT02_FLAG (0x3) /* should be 0x0F=> [4]:2L_HI, [1]:2L */
#endif

#define NR_FREQ 16
#define NR_FREQ_CPU 16
#define NR_FREQ_GPU 16

/*
 * 100 us, This is the EEM Detector sampling time as represented in
 * cycles of bclk_ck during INIT. 52 MHz
 */
#define DETWINDOW_VAL		0xA28

/*
 * mili Volt to config value. voltage = 600mV + val * 6.25mV
 * val = (voltage - 600) / 6.25
 * @mV:	mili volt
 */

/* 1mV=>10uV */
/* EEM */
#define EEM_V_BASE		(51875)
#define EEM_STEP		(625)

/* CPU */
#define CPU_PMIC_BASE_6357	(51875)
#define CPU_PMIC_STEP		(625)

/* common part: for cci, LL, L */
#define VBOOT_VAL		(0x2D)
#define VMAX_VAL		(0x51)
#define VMIN_VAL		(0x15)
#define VCO_VAL			(0x15)
#define DVTFIXED_VAL		(0x8)
#define DVTFIXED_M_VAL		(0x4)



#define DTHI_VAL		(0x01) /* positive */
#define DTLO_VAL		(0xfe) /* negative (2's compliment) */
#define DETMAX_VAL (0xffff) /*This timeout value is in cycles of bclk_ck.*/
#define AGECONFIG_VAL		(0x555555)
#define AGEM_VAL		(0x0)
#define DCCONFIG_VAL		(0x555555)

/* use in base_ops_mon_mode */
#define MTS_VAL			(0x1fb)
#define BTS_VAL			(0x6d1)

#define CORESEL_VAL			(0x8fff0000)
#define CORESEL_INIT2_VAL		(0x0fff0000)


#define INVERT_TEMP_VAL (25000)
#define OVER_INV_TEM_VAL (27000)

#define LOW_TEMP_OFF_DEFAULT (0)

#if ENABLE_EEMCTL0
#define EEM_CTL0_L (0x00000001)
#define EEM_CTL0_2L (0x00000001)
#define EEM_CTL0_GPU (0x00010001)
#endif

#if EEM_FAKE_EFUSE	/* select PTP secure mode based on efuse config. */
#define SEC_MOD_SEL			0xF0		/* non secure  mode */
#else
#define SEC_MOD_SEL			0x00		/* Secure Mode 0 */
/* #define SEC_MOD_SEL			0x10	*/	/* Secure Mode 1 */
/* #define SEC_MOD_SEL			0x20	*/	/* Secure Mode 2 */
/* #define SEC_MOD_SEL			0x30	*/	/* Secure Mode 3 */
/* #define SEC_MOD_SEL			0x40	*/	/* Secure Mode 4 */
#endif

#if SEC_MOD_SEL == 0xF0
/* Safe EFUSE */
#define DEVINFO_0 0x00000100

/*2-line*/
/* L_LOW */
#define DEVINFO_1 0x0
/* L_LOW + LL_LOW */
#define DEVINFO_2 0x0
/* LL_LOW */
#define DEVINFO_3 0x0
/* L_HIGH */
#define DEVINFO_4 0x0
/* L_HIGH + LL_HIGH */
#define DEVINFO_5 0x0
/* LL_HIGH */
#define DEVINFO_6 0x0
/* CCI */
#define DEVINFO_7 0x04045BF7
#define DEVINFO_8 0x27000000

/*1-line*/
/* L */
#define DEVINFO_9 0x0
/* L + LL */
#define DEVINFO_10 0x0
/* LL */
#define DEVINFO_11 0x0

#else
/* Safe EFUSE */
#define DEVINFO_0 0x0000FF00

/*2-line*/
/* L_LOW */
//#define DEVINFO_1 0x5DA40CB9
#define DEVINFO_1 0xFFFFFFFF
/* L_LOW + LL_LOW */
//#define DEVINFO_2 0x001E001E
#define DEVINFO_2 0xFFFFFFFF
/* LL_LOW */
//#define DEVINFO_3 0x5DA47B4D
#define DEVINFO_3 0xFFFFFFFF
/* L_HIGH */
#define DEVINFO_4 0x5DA4B4D8
/* L_HIGH + LL_HIGH */
#define DEVINFO_5 0x00750074
/* LL_HIGH */
#define DEVINFO_6 0x5DA4DFA6
/* CCI */
//#define DEVINFO_7 0x5DA4CABD
#define DEVINFO_7 0x21F152C9
//#define DEVINFO_8 0x00740000
#define DEVINFO_8 0x00720000

/*1-line*/
/* L */
#define DEVINFO_9 0x5DA4D7AB
/* L + LL */
#define DEVINFO_10 0x00750074
/* LL */
#define DEVINFO_11 0x5DA439B1

#endif

#endif
