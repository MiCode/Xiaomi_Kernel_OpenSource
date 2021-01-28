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
/* #define EEM_NOT_READY	(0) */
#define CONFIG_EEM_SHOWLOG	(0)
#define EN_ISR_LOG		(0)
#define EEM_BANK_SOC		(0) /* use voltage bin, so disable it */
#define EARLY_PORTING		(0) /* for detecting real vboot in eem_init01 */
#define DUMP_DATA_TO_DE		(1)
#define EEM_ENABLE		(1) /* enable; after pass HPT mini-SQC */
#define EEM_FAKE_EFUSE		(0)
/* FIX ME */
#define UPDATE_TO_UPOWER	(1)
#define PPM_READY (1)
#define EEM_LOCKTIME_LIMIT	(3000)
#define ENABLE_EEMCTL0		(1)
#define ENABLE_LOO			(0)
#define ENABLE_INIT1_STRESS	(1)

#define EEM_OFFSET
#define SET_PMIC_VOLT (1)
#define SET_PMIC_VOLT_TO_DVFS (1)
#define LOG_INTERVAL	(2LL * NSEC_PER_SEC)

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_L,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

#define DEVINFO_IDX_0 50	/* 10580 */
#define DEVINFO_IDX_1 51	/* 10584 */
#define DEVINFO_IDX_2 52	/* 10588 */
#define DEVINFO_IDX_3 53	/* 1058C */
#define DEVINFO_IDX_4 54	/* 10590 */
#define DEVINFO_IDX_5 55	/* 10594 */
#define DEVINFO_IDX_6 56	/* 10598 */
#define DEVINFO_IDX_7 57	/* 1059C */
#define DEVINFO_IDX_8 58	/* 105A0 */
#define DEVINFO_IDX_9 59	/* 105A4 */
#define DEVINFO_IDX_16 66	/* 105C0 */
#define DEVINFO_IDX_17 67	/* 105C4 */
#define DEVINFO_IDX_18 68	/* 105C8 */
#define CPUFREQ_SEG_CODE_IDX_0		7
#define TURBO_BIN_CODE_IDX_0		65

#if 0
/* Fake EFUSE */
#define DEVINFO_0 0xFF00
/* LL_LOW */
#define DEVINFO_1 0x10bd3c1b
/* L_LOW + LL_LOW */
#define DEVINFO_2 0x550055
/* L_LOW */
#define DEVINFO_3 0x10bd3c1b
/* CCI */
#define DEVINFO_4 0x10bd3c1b
/* GPU + CCI */
#define DEVINFO_5 0x550055
/* GPU */
#define DEVINFO_6 0x10bd3c1b
/* LL_HIGH */
#define DEVINFO_7 0x10bd3c1b
/* L_HIGH + LL_HIGH */
#define DEVINFO_8 0x550055
/* L_HIGH */
#define DEVINFO_9 0x10bd3c1b
/* LL */
#define DEVINFO_16 0x10bd3c1b
/* L + LL */
#define DEVINFO_17 0x550055
/* L */
#define DEVINFO_18 0x10bd3c1b
#else
/* Fake EFUSE */
#define DEVINFO_0 0x0000FF00
/* LL_LOW */
#define DEVINFO_1 0x09EA55F5
/* L_LOW + LL_LOW */
#define DEVINFO_2 0x00650065
/* L_LOW */
#define DEVINFO_3 0x09EA55F5
/* CCI */
#define DEVINFO_4 0x09EA5102
/* GPU + CCI */
#define DEVINFO_5 0x004E0024
/* GPU */
#define DEVINFO_6 0x09EA4F00
/* LL_HIGH */
#define DEVINFO_7 0x09EA90D1
/* L_HIGH + LL_HIGH */
#define DEVINFO_8 0x004E004E
/* L_HIGH */
#define DEVINFO_9 0x09EA90D1
/* LL */
#define DEVINFO_16 0x09EA68F0
/* L + LL */
#define DEVINFO_17 0x004E004E
/* L */
#define DEVINFO_18 0x09EA68F0
#endif
/*****************************************
 * eem sw setting
 ******************************************
 */
#define NR_HW_RES_FOR_BANK	(13) /* real eem banks for efuse */
#define EEM_INIT01_FLAG (0xF) /* should be 0x0F=>[3]:GPU,[2]:CCI,[1]:L,[0]:LL */
#if ENABLE_LOO
#define EEM_2L_INIT02_FLAG (0x11) /* should be 0x0F=> [4]:2L_HI, [0]:LL */
#define EEM_L_INIT02_FLAG (0x6) /* should be 0x0F=> [2]:L_HI, [1]:L */
#endif

#define NR_FREQ 16
#define NR_FREQ_GPU 16
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
#define CPU_PMIC_BASE_6358	(50000) /* (50000) */
#define CPU_PMIC_STEP		(625) /* 1.231/1024=0.001202v=120(10uv)*/

/* GPU */
#define GPU_PMIC_BASE		(50000)
#define GPU_PMIC_STEP		(625) /* 1.231/1024=0.001202v=120(10uv)*/

/* common part: for cci, LL, L, GPU */
#define VBOOT_VAL		(0x30) /* volt domain: 0.8v */
#define VMAX_VAL		(0x64) /* volt domain: 1.12v*/
#define VMIN_VAL		(0x10) /* volt domain: 0.6v*/
#define VMIN_GPU_VAL	(0x14) /* volt domain: 0.625v*/
#define VCO_VAL			(0x10)
#define DVTFIXED_VAL		(0x7)

#define DTHI_VAL		(0x01) /* positive */
#define DTLO_VAL		(0xfe) /* negative (2's compliment) */
/* This timeout value is in cycles of bclk_ck. */
#define DETMAX_VAL		(0xffff)
#define AGECONFIG_VAL		(0x555555)
#define AGEM_VAL		(0x0)
#define DCCONFIG_VAL		(0x555555)

/* different for GPU */
#define VBOOT_VAL_GPU		(0x30) /* eem domain: 0x40, volt domain: 0.8v */
#define VMAX_VAL_GPU		(0x40) /* eem domain: 0x60, volt domain: 1.0v */
#define DVTFIXED_VAL_GPU	(0x3)


/* use in base_ops_mon_mode */
#define MTS_VAL			(0x1fb)
#define BTS_VAL			(0x6d1)

#define CORESEL_VAL			(0x8fff0000)
#define CORESEL_INIT2_VAL		(0x0fff0000)


#define INVERT_TEMP_VAL (25000)
#define OVER_INV_TEM_VAL (27000)

#define LOW_TEMP_OFF_DEFAULT	(0)
#define LOW_TEMP_OFF_DEFAULT_GPU	(3)
#define MARGIN_2L_ADD_OFF_VER3			(8)	/* Add 50mv */
#define MARGIN_L_ADD_OFF_VER3			(13)	/* Add 81mv */
#define MARGIN_ADD_OFF_VER4				(10) /* Add 62.5mv */
#define LCPU_VMAX1050_PMIC_VAL		(0x58) /* volt domain: 1.05v */

#if ENABLE_EEMCTL0
#define EEM_CTL0_2L (0x00010001)
#define EEM_CTL0_L (0x00000001)
#define EEM_CTL0_CCI (0x00100003)
#define EEM_CTL0_GPU (0x00050001)
#endif

/* select PTP secure mode based on efuse config. */
#if EEM_FAKE_EFUSE
#define SEC_MOD_SEL			0xF0		/* non secure  mode */
#else
#define SEC_MOD_SEL			0x00		/* Secure Mode 0 */
/* #define SEC_MOD_SEL			0x10	*/	/* Secure Mode 1 */
/* #define SEC_MOD_SEL			0x20	*/	/* Secure Mode 2 */
/* #define SEC_MOD_SEL			0x30	*/	/* Secure Mode 3 */
/* #define SEC_MOD_SEL			0x40	*/	/* Secure Mode 4 */
#endif

#if SEC_MOD_SEL == 0x00
#define SEC_DCBDET 0xCC
#define SEC_DCMDET 0xE6
#define SEC_BDES 0xF5
#define SEC_MDES 0x97
#define SEC_MTDES 0xAC
#elif SEC_MOD_SEL == 0x10
#define SEC_DCBDET 0xE5
#define SEC_DCMDET 0xB
#define SEC_BDES 0x31
#define SEC_MDES 0x53
#define SEC_MTDES 0x68
#elif SEC_MOD_SEL == 0x20
#define SEC_DCBDET 0x39
#define SEC_DCMDET 0xFE
#define SEC_BDES 0x18
#define SEC_MDES 0x8F
#define SEC_MTDES 0xB4
#elif SEC_MOD_SEL == 0x30
#define SEC_DCBDET 0xDF
#define SEC_DCMDET 0x18
#define SEC_BDES 0x0B
#define SEC_MDES 0x7A
#define SEC_MTDES 0x52
#elif SEC_MOD_SEL == 0x40
#define SEC_DCBDET 0x36
#define SEC_DCMDET 0xF1
#define SEC_BDES 0xE2
#define SEC_MDES 0x80
#define SEC_MTDES 0x41
#endif

#endif
