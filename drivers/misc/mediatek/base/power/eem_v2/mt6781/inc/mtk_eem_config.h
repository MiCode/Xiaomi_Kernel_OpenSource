/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef _MTK_EEM_CONFIG_H_
#define _MTK_EEM_CONFIG_H_

/* CONFIG (SW related) */
/* #define EEM_NOT_READY	(1) */
#define CONFIG_EEM_SHOWLOG	(0)
#define EN_ISR_LOG		(0)
#define EEM_BANK_SOC		(0) /* use voltage bin, so disable it */
#define EARLY_PORTING		(0) /* for detecting real vboot in eem_init01 */
#define DUMP_DATA_TO_DE		(1)
#define EEM_ENABLE		(1) /* enable; after pass HPT mini-SQC */
#define EEM_FAKE_EFUSE		(0)
/* FIX ME */
#define UPDATE_TO_UPOWER	(1)
#define EEM_LOCKTIME_LIMIT	(3000)
#define ENABLE_LOO			(1)
#define ENABLE_LOO_B			(1)
#define ENABLE_LOO_G			(1)
#define SUPPORT_GPU_VB	(1)	/* gpu voltage bin */
#define SUPPORT_PICACHU		(1)

#ifdef CORN_LOAD
#define ENABLE_VPU              (1)
#define ENABLE_MDLA             (1)
#else
#define ENABLE_VPU              (0)
#define ENABLE_MDLA             (0)
#endif
#define ENABLE_INIT1_STRESS	(0)

#define EEM_OFFSET
/* CCJ set to 0 for bring up */
/* #define EARLY_PORTING_GPU */
#define SET_PMIC_VOLT		(1)
#define SET_PMIC_VOLT_TO_DVFS (1)
#define LOG_INTERVAL	(2LL * NSEC_PER_SEC)
#define DVT					(0)
#define SUPPORT_DCONFIG		(1)
#define ENABLE_HT_FT		(1)
#define ENABLE_REMOVE_AGING		(0)

#if DVT
#define DUMP_LEN	410
#else
#define DUMP_LEN	105
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
#define DEVINFO_IDX_19 69


#define DEVINFO_TIME_IDX 132



#if EEM_FAKE_EFUSE		/* select secure mode based on efuse config */
#define SEC_MOD_SEL			0x00		/* non secure  mode */
#else
#define SEC_MOD_SEL			0x00		/* Secure Mode 0 */
/* #define SEC_MOD_SEL			0x10	*/	/* Secure Mode 1 */
/* #define SEC_MOD_SEL			0x20	*/	/* Secure Mode 2 */
/* #define SEC_MOD_SEL			0x30	*/	/* Secure Mode 3 */
/* #define SEC_MOD_SEL			0x40	*/	/* Secure Mode 4 */
#endif

#if SEC_MOD_SEL == 0xF0
/* Fake EFUSE */
#define DEVINFO_0 0x0
#define DEVINFO_1 0x9D112C35
#define DEVINFO_2 0x9A102C35
#define DEVINFO_3 0x62112C06
#define DEVINFO_4 0x91102C36
#define DEVINFO_5 0x92112C36
#define DEVINFO_6 0x56152C2D
#define DEVINFO_7 0x36192C02
#define DEVINFO_8 0x0
#define DEVINFO_9 0x0
#define DEVINFO_10 0x0
#define DEVINFO_11 0x0
#define DEVINFO_12 0x0
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B030000

#else

#if defined(CMD_LOAD)
/* Safe EFUSE */
#define DEVINFO_0 0x0
#define DEVINFO_1 0x9D112C35
#define DEVINFO_2 0x9A102C35
#define DEVINFO_3 0x62112C06
#define DEVINFO_4 0x91102C36
#define DEVINFO_5 0x92112C36
#define DEVINFO_6 0x56152C2D
#define DEVINFO_7 0x36192C02
#define DEVINFO_8 0x0
#define DEVINFO_9 0x0
#define DEVINFO_10 0x0
#define DEVINFO_11 0x0
#define DEVINFO_12 0x0
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B030000

#elif defined(MC50_LOAD)
/* MC50 Safe EFUSE */
#define DEVINFO_0 0x00000002
#define DEVINFO_1 0x7C152C3A
#define DEVINFO_2 0x4B1D2C3A
#define DEVINFO_3 0x49172C0A
#define DEVINFO_4 0x6BEF303C
#define DEVINFO_5 0x6CEC343D
#define DEVINFO_6 0x41112C2C
#define DEVINFO_7 0x0E1B2C04
#define DEVINFO_8 0x0
#define DEVINFO_9 0x0
#define DEVINFO_10 0x0
#define DEVINFO_11 0x0
#define DEVINFO_12 0x0
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B030000

#else
/* MC99 Safe EFUSE */
#define DEVINFO_0 0x0
#define DEVINFO_1 0x9D112C35
#define DEVINFO_2 0x9A102C35
#define DEVINFO_3 0x62112C06
#define DEVINFO_4 0x91102C36
#define DEVINFO_5 0x92112C36
#define DEVINFO_6 0x56152C2D
#define DEVINFO_7 0x36192C02
#define DEVINFO_8 0x0
#define DEVINFO_9 0x0
#define DEVINFO_10 0x0
#define DEVINFO_11 0x0
#define DEVINFO_12 0x0
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B030000

#endif
#endif


/*****************************************
 * eem sw setting
 ******************************************
 */
#define NR_HW_RES_FOR_BANK	(17) /* real eem banks for efuse */
#define EEM_CORNER_FLAG (0x30) /* 0x30=> [5]:VPU, [4]:MDLA */


#define NR_FREQ 16
#define NR_FREQ_GPU 16
#define NR_FREQ_VPU 16
#define NR_FREQ_CPU 16

#define L_FREQ_BASE			2000000
#define B_FREQ_BASE			2050000
#define	CCI_FREQ_BASE		1400000
#define GPU_FREQ_BASE		790000
#define B_M_FREQ_BASE		1670000
#define GPU_M_FREQ_BASE		560000

#define BANK_L_TURN_FREQ	2000000
#define BANK_B_TURN_FREQ	1670000
#define BANK_L_TURN_PT		0
#define BANK_GPU_TURN_PT	6
#if ENABLE_LOO_B
#define BANK_B_TURN_PT		6
#if 0
#define EEM_B_INIT02_FLAG (0x22) /* should be 0x022=> [5]:B_HI, [1]:B */
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
#define EEM_V_BASE		(40000)
#define EEM_STEP		(625)

/* CPU */
#define CPU_PMIC_BASE	(50000)
#define CPU_PMIC_STEP		(625) /* 1.231/1024=0.001202v=120(10uv)*/

/* GPU */
#define GPU_PMIC_BASE		(50000)
#define GPU_PMIC_STEP		(625) /* 1.231/1024=0.001202v=120(10uv)*/

/* common part: for cci, LL, L, GPU */
/* common part: for  LL, L */
#define VBOOT_PMIC_VAL	(80000)
#define VBOOT_PMIC_CLR	(0)
#define VBOOT_VAL		(0x40) /* volt domain: 0.8v */
#define VMAX_VAL		(0x73) /* volt domain: 1.11875v*/
#define VMIN_VAL		(0x20) /* volt domain: 0.6v*/
#define VCO_VAL			(0x10)
#define DVTFIXED_VAL	(0x6)
#define DVTFIXED_VAL_V2	(10)

#define DVTFIXED_M_VAL	(0x07)


#define VMAX_VAL_B		(0x73) /* volt domain: 1.11875v*/
#define VMIN_VAL_B		(0x20) /* volt domain: 0.6v*/
#define VCO_VAL_B		(0x10) /* volt domain: 0.5v*/
#define DVTFIXED_VAL_B	(0x6)


#define DTHI_VAL		(0x01) /* positive */
#define DTLO_VAL		(0xfe) /* negative (2's compliment) */
/* This timeout value is in cycles of bclk_ck. */
#define DETMAX_VAL		(0xffff)
#define AGECONFIG_VAL	(0x555555)
#define AGEM_VAL		(0x0)
#define DCCONFIG_VAL	(0x555555)

/* different for CCI */
#define VMAX_VAL_CCI		(0x73) /* volt domain: 1.11875v*/
#define VMIN_VAL_CCI		(0x20) /* volt domain: 0.6v*/
#define VCO_VAL_CCI			(0x10)
#define DVTFIXED_VAL_CCI	(0x6)



/* different for GPU */
#define VMAX_VAL_GPU                    (0x73) /* eem domain: 1.11875v*/
#define VMIN_VAL_GPU                    (0x20) /* volt domain: 0.6v*/
#define VCO_VAL_GPU                     (0x10) /* eem domain: 0.5v*/

/* different for GPU_L */
#define VMAX_VAL_GL                     (0x73)
#define VMIN_VAL_GL                     (0x20) /* volt domain: 0.6v*/
#define VCO_VAL_GL                      (0x10)
#define DVTFIXED_VAL_GL					(0x04)
#define DVTFIXED_VAL_GPU				(0x04)

/* different for GPU_H */
#define VMAX_VAL_GH                     (0x73) /* volt domain: 1.11875v*/
#define VMIN_VAL_GH                     (0x20) /* volt domain: 0.6v*/
#define VCO_VAL_GH                      (0x10)

/* different for B_H */
#define VMAX_VAL_BH			(0x73) /* volt domain: 1.11875v*/
#define VMIN_VAL_BH			(0x20) /* volt domain: 0.6v*/
#define VCO_VAL_BH			(0x10)
#define DVTFIXED_VAL_BH		(0x6)

/* different for B_H */
#define VMAX_VAL_BL			(0x73)/* volt domain: 1.11875v*/
#define VMIN_VAL_BL			(0x20) /* volt domain: 0.6v*/
#define VCO_VAL_BL			(0x10)
#define DVTFIXED_VAL_BL		(0x3)



/* use in base_ops_mon_mode */
#define MTS_VAL			(0x1fb)
#define BTS_VAL			(0x6d1)

#define CORESEL_VAL			(0x8fff0000)
#define CORESEL_INIT2_VAL		(0x0fff0000)


#define LOW_TEMP_VAL		(25000)
#define EXTRA_LOW_TEMP_VAL	(10000)
#define HIGH_TEMP_VAL		(85000)


#define LOW_TEMP_OFF_DEFAULT	(0)
#define EXTRA_TEMP_OFF_L		(8)
#define EXTRA_TEMP_OFF_B		(8)
#define EXTRA_TEMP_OFF_GPU		(4)
#define EXTRA_LOW_TEMP_OFF_GPU	(7)
#define EXTRA_TEMP_OFF_B_LO		(2)
#define MARGIN_ADD_OFF			(5)
#define MARGIN_CLAMP_OFF		(8)


#define LOW_TEMP_OFF_L (0x04)
#define LOW_TEMP_OFF_B (0x05)
#define LOW_TEMP_OFF_CCI (0x04)
#define LOW_TEMP_OFF_GPU (0x03)
#define LOW_TEMP_OFF_VPU (0x04)


/* for EEMCTL0's setting */
#define EEM_CTL0_L			(0x3210000F)
#define EEM_CTL0_B			(0x00540003)
#define EEM_CTL0_CCI		(0x3210000F)
#define EEM_CTL0_GPU		(0x00100003)
#define EEM_CTL0_VPU		(0x00010001)


#define AGING_VAL_CPU_L		(0x5) /* CPU aging margin : 31mv*/
#define AGING_VAL_CPU		(0x6) /* CPU aging margin : 37mv*/
#define AGING_VAL_GPU		(0x5) /* GPU aging margin : 43.75mv*/
#define BPCU_ADD_OFT_V5				(5) /* Add 31mv */


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
