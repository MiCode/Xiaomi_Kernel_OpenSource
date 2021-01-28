/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */
#ifndef _MTK_EEM_CONFIG_H_
#define _MTK_EEM_CONFIG_H_

/* CONFIG (SW related) */
/* #define EEM_NOT_READY (1) */ /* for bring up, remove for MP */
#define CONFIG_EEM_SHOWLOG (0)
#define EN_ISR_LOG (0)
#define EEM_BANK_SOC (0) /* use voltage bin, so disable it */
#define EARLY_PORTING (0) /* for detecting real vboot in eem_init01 */
#define DUMP_DATA_TO_DE (1)
#define EEM_FAKE_EFUSE (0)
/* FIX ME */
#define UPDATE_TO_UPOWER (1)
#define EEM_LOCKTIME_LIMIT (3000)
#define ENABLE_EEMCTL0 (1)
#define ENABLE_LOO			(1)
#define ENABLE_INIT1_STRESS (1)

#define EEM_OFFSET
#define SET_PMIC_VOLT (1)
#define SET_PMIC_VOLT_TO_DVFS (1)
#define LOG_INTERVAL	(2LL * NSEC_PER_SEC)

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

#define EX_DEV_IDX_0 59		/* 5A4 */
#define EX_DEV_IDX_1 64		/* 5B8 */
#define EX_DEV_IDX_2 65		/* 5BC */
#define EX_DEV_IDX_3 133	/* 7AC */

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

#define EX_DEV_OFF_0 0xec	/* 5A4 */
#define EX_DEV_OFF_1 0x100	/* 5B8 */
#define EX_DEV_OFF_2 0x104	/* 5BC */
#define EX_DEV_OFF_3 0x214	/* 7AC */

/*****************************************
 * eem sw setting
 ******************************************
 */
#define NR_HW_RES_FOR_BANK	(12) /* real eem banks for efuse */
#define EEM_INIT01_FLAG (0x3) /* [3]:GPU, [2]:CCI, [1]:LL, [0]:L */
#if ENABLE_LOO
#define EEM_L_INIT02_FLAG (0x9) /* should be 0x0F=> [3]:L_HI, [0]:L */
#define EEM_2L_INIT02_FLAG (0x12) /* should be 0x0F=> [4]:2L_HI, [1]:2L */
#endif

#define NR_FREQ 16
#define NR_FREQ_CPU 16

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
#define EEM_V_BASE		(50000)
#define EEM_STEP		(625)

/* CPU */
#define CPU_PMIC_BASE_6357	(51875)
#define CPU_PMIC_STEP		(625)

/* common part: for cci, LL, L */
#define VBOOT_VAL		(0x30)
#define VMAX_VAL		(0x64)
#define VMIN_VAL		(0x10)
#define VCO_VAL			(0x10)
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
#define EEM_CTL0_2L (0x00010001)
#define EEM_CTL0_CCI (0x00100003)
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
#define DEVINFO_0 0x0000FF00

/*2-line*/
/* L_LOW */
#define DEVINFO_1 0x12A446F6
/* L_LOW + LL_LOW */
#define DEVINFO_2 0x00510051
/* LL_LOW */
#define DEVINFO_3 0x12A43602
/* L_HIGH */
#define DEVINFO_4 0x12A4FB97
/* L_HIGH + LL_HIGH */
#define DEVINFO_5 0x003A003B
/* LL_HIGH */
#define DEVINFO_6 0x12A490EA
/* CCI */
#define DEVINFO_7 0x12A486F2
#define DEVINFO_8 0x003B0000

/*1-line*/
/* L */
#define DEVINFO_9 0x12A498E4
/* L + LL */
#define DEVINFO_10 0x003A003B
/* LL */
#define DEVINFO_11 0x12A476FE

#else
/* Safe EFUSE */
#define DEVINFO_0 0x0000FF00

/*2-line*/
/* L_LOW */
#define DEVINFO_1 0x5DA40CB9
/* L_LOW + LL_LOW */
#define DEVINFO_2 0x001E001E
/* LL_LOW */
#define DEVINFO_3 0x5DA47B4D
/* L_HIGH */
#define DEVINFO_4 0x5DA4B4D8
/* L_HIGH + LL_HIGH */
#define DEVINFO_5 0x00750074
/* LL_HIGH */
#define DEVINFO_6 0x5DA4DFA6
/* CCI */
#define DEVINFO_7 0x5DA4CABD
#define DEVINFO_8 0x00740000

/*1-line*/
/* L */
#define DEVINFO_9 0x5DA4D7AB
/* L + LL */
#define DEVINFO_10 0x00750074
/* LL */
#define DEVINFO_11 0x5DA439B1

#endif

#endif
