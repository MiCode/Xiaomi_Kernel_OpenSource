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
#ifndef _MTK_EEMG_CONFIG_H_
#define _MTK_EEMG_CONFIG_H_

/* CONFIG (SW related) */
//#define EEMG_NOT_READY		(1)
#define CONFIG_EEMG_SHOWLOG	(0)
#define EN_ISR_LOG		(0)
#define EEMG_BANK_SOC		(0) /* use voltage bin, so disable it */
#define EARLY_PORTING		(0)
#define DUMP_DATA_TO_DE		(1)
#define EEMG_ENABLE		(1) /* enable; after pass HPT mini-SQC */
#define EEMG_FAKE_EFUSE		(0)

/* FIX ME */
#define EEMG_LOCKTIME_LIMIT	(3000)
#define ENABLE_LOO		(1)
#define ENABLE_LOO_B		(0)
#define ENABLE_LOO_G		(1)
#define ENABLE_GPU		(1)

#ifdef CORN_LOAD
#define ENABLE_VPU              (1)
#define ENABLE_MDLA             (1)
#else
#define ENABLE_VPU              (0)
#define ENABLE_MDLA             (0)
#endif



#define EEMG_OFFSET
#define SET_PMIC_VOLT		(1)
#define SET_PMIC_VOLT_TO_DVFS	(1)
#define LOG_INTERVAL		(2LL * NSEC_PER_SEC)
#define DVT			(0)
#define SUPPORT_DCONFIG		(1)
#define ENABLE_HT_FT		(1)
//#define EARLY_PORTING_VPU
#define ENABLE_MINIHQA		(0)
#define ENABLE_REMOVE_AGING	(0)

#if DVT
#define DUMP_LEN		410
#else
#define DUMP_LEN		105
#endif


enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_L,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

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
#define DEVINFO_IDX_19 69

#if EEMG_FAKE_EFUSE		/* select secure mode based on efuse config */
#define SEC_MOD_SEL			0x00		/* non secure  mode */
#else
#define SEC_MOD_SEL			0x00		/* Secure Mode 0 */
/* #define SEC_MOD_SEL			0x10	*/	/* Secure Mode 1 */
/* #define SEC_MOD_SEL			0x20	*/	/* Secure Mode 2 */
/* #define SEC_MOD_SEL			0x30	*/	/* Secure Mode 3 */
/* #define SEC_MOD_SEL			0x40	*/	/* Secure Mode 4 */
#endif

#if SEC_MOD_SEL == 0xF0

#define DEVINFO_0 0xFF00
#define DEVINFO_1 0x10bd3c1b
#define DEVINFO_2 0x550055
#define DEVINFO_3 0x10bd3c1b
#define DEVINFO_4 0x10bd3c1b
#define DEVINFO_5 0x550055
#define DEVINFO_6 0x10bd3c1b
#define DEVINFO_7 0x10bd3c1b
#define DEVINFO_8 0x550055
#define DEVINFO_9 0x10bd3c1b
#define DEVINFO_10 0x10bd3c1b
#define DEVINFO_11 0x550055
#define DEVINFO_12 0x10bd3c1b
#define DEVINFO_16 0x10bd3c1b
#define DEVINFO_17 0x550055

#else

#if defined(CMD_LOAD)

#define DEVINFO_0 0x00060006
#define DEVINFO_1 0x0
#define DEVINFO_2 0x000000AF
#define DEVINFO_3 0x9B0B0363
#define DEVINFO_4 0x9B0B0769
#define DEVINFO_5 0x00A100A6
#define DEVINFO_6 0x9B0BBF96
#define DEVINFO_7 0x10bd3c1b
#define DEVINFO_8 0x550055
#define DEVINFO_9 0x10bd3c1b
#define DEVINFO_10 0x9B0B2263
#define DEVINFO_11 0x00B900AC
#define DEVINFO_12 0x570B166E
#define DEVINFO_16 0x9B0B0866
#define DEVINFO_17 0x00A100BB

#elif defined(MC50_LOAD)

#define DEVINFO_0 0x00000001
#define DEVINFO_1 0x0
#define DEVINFO_2 0x5A1E242A
#define DEVINFO_3 0x57142476
#define DEVINFO_4 0x49172454
#define DEVINFO_5 0x0
#define DEVINFO_6 0x0
#define DEVINFO_7 0x4D152457
#define DEVINFO_8 0x00000000
#define DEVINFO_9 0x56EC00DC
#define DEVINFO_10 0x081800A6
#define DEVINFO_11 0x6F572444
#define DEVINFO_12 0x725C244D
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x00000000
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0x1B031B03
//#define DEVINFO_18 0x5D025D02

#else

/* MC99 Safe EFUSE */
#define DEVINFO_0 0x0
#define DEVINFO_1 0x94132424
#define DEVINFO_2 0xB1E92424
#define DEVINFO_3 0x42122446
#define DEVINFO_4 0x63122424
#define DEVINFO_5 0x0
#define DEVINFO_6 0x0
#define DEVINFO_7 0x62122424
#define DEVINFO_8 0x51152498
#define DEVINFO_9 0x46EA0098
#define DEVINFO_10 0x37170054
#define DEVINFO_11 0xC3990089
#define DEVINFO_12 0xDC910089
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x0
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0x1B031B03

#endif
#endif


/*****************************************
 * eem sw setting
 ******************************************
 */
#define NR_HW_RES_FOR_BANK	(18) /* real eem banks for efuse */
#define EEMG_INIT01_FLAG (0x01) /* 0x01=> [0]:GPU */
#define EEMG_CORNER_FLAG (0x30) /* 0x30=> [5]:VPU, [4]:MDLA */
#if 0
#if ENABLE_LOO
#if DVT
#define EEMG_GPU_INIT02_FLAG (0x48) /* should be 0x048=>[6]:GPU_HI,[3]:GPU_LO */
#else
#define EEMG_GPU_INIT02_FLAG (0x18) /* should be 0x018=>[4]:GPU_HI,[3]:GPU_LO */
#endif
#else
#define EEMG_GPU_INIT02_FLAG (0x8) /* should be 0x08=>[3]:GPU */
#endif
#endif

#define NR_FREQ 16
#define NR_FREQ_GPU 16
#define NR_FREQ_VPU 16
#define NR_FREQ_CPU 16

#define L_FREQ_BASE			2000000
#define B_FREQ_BASE			2300000
#define	CCI_FREQ_BASE		1540000
#define GPU_FREQ_BASE		902000
#define B_M_FREQ_BASE		1750000
#define GPU_M_FREQ_BASE		688000

#define BANK_L_TURN_PT		0
#define BANK_GPU_TURN_PT	6
#if ENABLE_LOO_B
#define BANK_B_TURN_PT		6
#if 0
#define EEMG_B_INIT02_FLAG (0x22) /* should be 0x022=> [5]:B_HI, [1]:B */
#endif
#endif

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
#define EEMG_V_BASE		(40000)
#define EEMG_STEP		(625)

/* CPU */
#define CPU_PMIC_BASE_6359	(0)
#define CPU_PMIC_STEP		(625)

/* GPU */
#define GPU_PMIC_BASE		(0)
#define GPU_PMIC_STEP		(625)

/* common part: for cci, LL, L, GPU */
/* common part: for  LL, L */
#define VBOOT_PMIC_VAL	(75000)
#define VBOOT_PMIC_CLR	(0)
#define VBOOT_VAL		(0x38) /* volt domain: 0.75v */
#define VMAX_VAL		(0x60) /* volt domain: 1v*/
#define VMIN_VAL		(0x20) /* volt domain: 0.55v*/
#define VCO_VAL			(0x18)
#define DVTFIXED_VAL	(0x6)

#define DVTFIXED_M_VAL	(0x6)


#define VMAX_VAL_B		(0x60) /* volt domain: 1v*/
#define VMIN_VAL_B		(0x20) /* volt domain: 0.6v*/
#define VCO_VAL_B		(0x18) /* volt domain: 0.55v*/
#define DVTFIXED_VAL_B	(0x6)

#define DTHI_VAL		(0x01) /* positive */
#define DTLO_VAL		(0xfe) /* negative (2's compliment) */
/* This timeout value is in cycles of bclk_ck. */
#define DETMAX_VAL		(0xffff)
#define AGECONFIG_VAL	(0x555555)
#define AGEM_VAL		(0x0)
#define DCCONFIG_VAL	(0x1)

/* different for CCI */
#define VMAX_VAL_CCI		(0x60) /* volt domain: 1v*/
#define VMIN_VAL_CCI		(0x20)
#define VCO_VAL_CCI			(0x18)
#define DVTFIXED_VAL_CCI	(0x6)


/* different for GPU */
#define VMAX_VAL_GPU                    (0x60) /* eem domain: 1v*/
#define VMIN_VAL_GPU			(0x1A) /* 0.5625v */
#define VMIN_VAL_GPU_01			(0x1E) /* 0.5875v */
#define VCO_VAL_GPU                     (0x18) /* eem domain: 0.55v*/

/* different for GPU_L */
#define VMAX_VAL_GL                     (0x60)
#define VMIN_VAL_GL                     (0x18)
#define VCO_VAL_GL                      (0x18)
#define DVTFIXED_VAL_GL			(0x01)
#define DVTFIXED_VAL_GPU		(0x06)

/* different for GPU_H */
#define VMAX_VAL_GH                     (0x60) /* volt domain: 1.11875v*/
#define VMIN_VAL_GH			(0x1A) /* 0.5625v */
#define VMIN_VAL_GH_01                  (0x1E) /* 0.5875v */
#define VCO_VAL_GH                      (0x18)

/* different for L_L */
#define VMAX_VAL_LL                     (0x37)
#define VMIN_VAL_LL                     (0x15)
#define VCO_VAL_LL                      (0x15)

/* different for B_L */
#define VMAX_VAL_BL                     (0x60) /* volt domain: 1.11875v*/
#define VMIN_VAL_BL                     (0x20)
#define VCO_VAL_BL                      (0x18)
#define DVTFIXED_VAL_BL					(0x6)

/* different for L_H */
#define VMAX_VAL_H			(0x50)
#define VMIN_VAL_H			(0x30)
#define VCO_VAL_H			(0x30)
#define DVTFIXED_VAL_H			(0x03)

/* different for B_H */
#define VMAX_VAL_BH			(0x73) /* volt domain: 1.11875v*/
#define VMIN_VAL_BH			(0x20)
#define VCO_VAL_BH			(0x18)
#define DVTFIXED_VAL_BH		(0x6)

/* different for APU */
#define VBOOT_VAL_VPU		(0x40) /* eem domain: 0x40, volt domain: 0.8v */
#define VMAX_VAL_VPU		(0xFF) /* eem domain: 0x60, volt domain: 1.0v */
#define VMIN_VAL_VPU		(0x00) /* eem domain: 0x60, volt domain: 1.0v */
#define VCO_VAL_VPU             (0x10)
#define DVTFIXED_VAL_VPU	(0x06)


/* use in base_ops_mon_mode */
#define MTS_VAL			(0x1fb)
#define BTS_VAL			(0x6d1)

#define CORESEL_VAL			(0x8fff0000)
#define CORESEL_INIT2_VAL		(0x0fff0000)


#define LOW_TEMP_VAL		(25000)
#define EXTRA_LOW_TEMP_VAL	(10000)
#define HIGH_TEMP_VAL		(85000)

#define LOW_TEMP_OFF_DEFAULT	(0)
#define LOW_TEMP_OFF_L		(8)
#define HIGH_TEMP_OFF_L		(3)
#define LOW_TEMP_OFF_B		(8)
#define HIGH_TEMP_OFF_B		(3)
#define LOW_TEMP_OFF_GPU		(4)
#define HIGH_TEMP_OFF_GPU		(0)
#define EXTRA_LOW_TEMP_OFF_GPU	(7)
#define MARGIN_ADD_OFF			(5)
#define MARGIN_CLAMP_OFF		(8)



/* for EEMCTL0's setting */
#define EEMG_CTL0_L			(0xBA98000F)
#define EEMG_CTL0_B			(0x00540003)
#define EEMG_CTL0_CCI		(0xBA98000F)
#define EEMG_CTL0_GPU		(0x00540003)
#define EEMG_CTL0_VPU		(0x00210003)


#define AGING_VAL_CPU		(0x0) /* CPU aging margin : 31mv*/
#define AGING_VAL_CPU_B		(0x0) /* CPU aging margin : 37mv*/
#define AGING_VAL_GPU		(0x0) /* GPU aging margin : 43.75mv*/


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
