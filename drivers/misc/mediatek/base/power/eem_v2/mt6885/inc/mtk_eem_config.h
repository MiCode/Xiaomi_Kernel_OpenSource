/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef _MTK_EEM_CONFIG_H_
#define _MTK_EEM_CONFIG_H_

/* CONFIG (SW related) */
/* #define EEM_NOT_READY		(1) */
#define CONFIG_EEM_SHOWLOG	(0)
#define EN_ISR_LOG		(0)
#define EEM_BANK_SOC		(0) /* use voltage bin, so disable it */
#define EARLY_PORTING		(0) /* for detecting real vboot in eem_init01 */
#define DUMP_DATA_TO_DE		(1)
#define EEM_ENABLE		(1) /* enable; after pass HPT mini-SQC */
#define EEM_FAKE_EFUSE		(0)
//#define MT6885
//#define MT6889
//#define MC50_LOAD

/* FIX ME */
#define UPDATE_TO_UPOWER	(1)
#define EEM_LOCKTIME_LIMIT	(3000)
#define ENABLE_LOO		(1)
#define ENABLE_LOO_B		(1)
#define ENABLE_LOO_G		(0)
#define ENABLE_CPU			(1)
#define ENABLE_GPU			(0)
#define EN_EEM_THERM_CLK	(0)
#define SUPPORT_PICACHU		(1)


#ifdef CORN_LOAD
#define ENABLE_VPU              (1)
#define ENABLE_MDLA             (1)
#else
#define ENABLE_VPU              (0)
#define ENABLE_MDLA             (0)
#endif

#define ENABLE_INIT1_STRESS	(1)

#define EEM_OFFSET
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
#define DEVINFO_0 0xFF00
/* L_LO */
#define DEVINFO_1 0x10bd3c1b
/* B_LO + L_LO */
#define DEVINFO_2 0x550055
/* B_LO */
#define DEVINFO_3 0x10bd3c1b
/* CCI */
#define DEVINFO_4 0x10bd3c1b
/* GPU_LO + CCI */
#define DEVINFO_5 0x550055
/* GPU_LO */
#define DEVINFO_6 0x10bd3c1b
/* APU */
#define DEVINFO_7 0x10bd3c1b
/* L_HI + APU */
#define DEVINFO_8 0x550055
/* L_HI */
#define DEVINFO_9 0x10bd3c1b
/* B_HI */
#define DEVINFO_10 0x10bd3c1b
/* MODEM + B_HI */
#define DEVINFO_11 0x550055
/* MODEM */
#define DEVINFO_12 0x10bd3c1b
/* L */
#define DEVINFO_16 0x10bd3c1b
/* B + L */
#define DEVINFO_17 0x550055
/* B */
#define DEVINFO_18 0x10bd3c1b
/* MDLA */
#define DEVINFO_19 0x10bd3c1b
/* GPU + MDLA */
#define DEVINFO_23 0x550055
/* GPU */
#define DEVINFO_24 0x10bd3c1b

#else

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

#if defined(MT6885)
#define DEVINFO_0 0x0 /* MC50 Safe EFUSE */
#define DEVINFO_1 0x67130025 /* L_LO */
#define DEVINFO_2 0x7D10002F /* B_LO + L_LO */
#define DEVINFO_3 0x2716009E /* B_LO */
#define DEVINFO_4 0x4B140057 /* CCI */
#define DEVINFO_5 0x0 /* GPU_LO + CCI */
#define DEVINFO_6 0x0 /* GPU_LO */
#define DEVINFO_7 0x74100057 /* APU */
#define DEVINFO_8 0x56100076 /* L_HI + APU */
#define DEVINFO_9 0x2DEF00A6 /* L_HI */
#define DEVINFO_10 0x0919002C /* B_HI */
#define DEVINFO_11 0xCB6F0083 /* MODEM + B_HI */
#define DEVINFO_12 0x186A00A5 /* MODEM */
#define DEVINFO_13 0x1B031B03 /* MODEM */
#define DEVINFO_14 0x1B031B03 /* MODEM */
#define DEVINFO_15 0x0 /* MODEM */
#define DEVINFO_16 0x1B031B03 /* L */
#define DEVINFO_17 0x1B031B03 /* B + L */

#elif defined(MT6889)
#define DEVINFO_0 0x0 /* MC50 Safe EFUSE */
#define DEVINFO_1 0x67130025 /* L_LO */
#define DEVINFO_2 0x67E80059 /* B_LO + L_L0 */
#define DEVINFO_3 0x2716009E /* B_L0 */
#define DEVINFO_4 0x4B140057 /* CCI */
#define DEVINFO_5 0x0 /* GPU_LO + CCI */
#define DEVINFO_6 0x0 /* GPU_L0 */
#define DEVINFO_7 0x74100057 /* APU */
#define DEVINFO_8 0x56100076 /* L_HI + APU */
#define DEVINFO_9 0x2DEE00B5 /* L_HI */
#define DEVINFO_10 0x0919002C /* B_HI */
#define DEVINFO_11 0xCB6F0083 /* MODEM + B_HI */
#define DEVINFO_12 0x186A00A5 /* MODEM */
#define DEVINFO_13 0x1B031B03 /* MODEM */
#define DEVINFO_14 0x1B031B03 /* MODEM */
#define DEVINFO_15 0x0 /* MODEM */
#define DEVINFO_16 0x1B031B03 /* L */
#define DEVINFO_17 0x1B031B03 /* B + L */

#endif

#else

#define DEVINFO_0 0x0 /* MC50 Safe EFUSE */
#define DEVINFO_1 0x67130025 /* L_LO */
#define DEVINFO_2 0x9DEB0025 /* B_LO + L_L0 */
#define DEVINFO_3 0x43150046 /* B_L0 */
#define DEVINFO_4 0x61120027 /* CCI */
#define DEVINFO_5 0x0 /* GPU_LO + CCI */
#define DEVINFO_6 0x0 /* GPU_L0 */
#define DEVINFO_7 0x91EC0027 /* APU */
#define DEVINFO_8 0x56100076 /* L_HI + APU */
#define DEVINFO_9 0x7AE20076 /* L_HI */
#define DEVINFO_10 0x3A150032 /* B_HI */
#define DEVINFO_11 0xCB6F0083 /* MODEM + B_HI */
#define DEVINFO_12 0x186A00A5 /* MODEM */
#define DEVINFO_13 0x1B031B03 /* MODEM */
#define DEVINFO_14 0x1B031B03 /* MODEM */
#define DEVINFO_15 0x0 /* MODEM */
#define DEVINFO_16 0x1B031B03 /* L */
#define DEVINFO_17 0x1B031B03 /* B + L */

#endif
#endif


/*****************************************
 * eem sw setting
 ******************************************
 */
#define NR_HW_RES_FOR_BANK	(18) /* real eem banks for efuse */
#if ENABLE_GPU
#define EEM_INIT01_FLAG (0x0f) /* 0x0f=> [3]:GPU, [2]:CCI, [1]:B, [0]:L */
#else
#define EEM_INIT01_FLAG (0x07) /* 0x0f=> [3]:GPU, [2]:CCI, [1]:B, [0]:L */
#endif
#define EEM_CORNER_FLAG (0x30) /* 0x30=> [5]:VPU, [4]:MDLA */
#if 0
#if ENABLE_LOO
#if DVT
#define EEM_GPU_INIT02_FLAG (0x48) /* should be 0x048=>[6]:GPU_HI,[3]:GPU_LO */
#else
#define EEM_GPU_INIT02_FLAG (0x18) /* should be 0x018=>[4]:GPU_HI,[3]:GPU_LO */
#endif
#else
#define EEM_GPU_INIT02_FLAG (0x8) /* should be 0x08=>[3]:GPU */
#endif
#endif

#define NR_FREQ 16
#define NR_FREQ_GPU 16
#define NR_FREQ_VPU 16
#define NR_FREQ_CPU 16

#define L_FREQ_BASE			2000000
#define B_FREQ_BASE			2300000
#define B_FREQ26_BASE		2600000
#define	CCI_FREQ_BASE		1540000
#define GPU_FREQ_BASE		806000
#define B_M_FREQ_BASE		1750000
#define GPU_M_FREQ_BASE		620000

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
#define CPU_PMIC_BASE_6359	(0)
#define CPU_PMIC_STEP		(625) /* 1.231/1024=0.001202v=120(10uv)*/
#define CPU_PMIC_VMAX_CLAMP	(0xA0) /* volt domain: 1v*/

/* GPU */
#define GPU_PMIC_BASE		(0)
#define GPU_PMIC_STEP		(625) /* 1.231/1024=0.001202v=120(10uv)*/

/* common part: for cci, LL, L, GPU */
/* common part: for  LL, L */
#define VBOOT_PMIC_VAL	(75000)
#define VBOOT_PMIC_CLR	(0)
#define VBOOT_VAL		(0x38) /* volt domain: 0.75v */
#define VMAX_VAL		(0x60) /* volt domain: 1v*/
#define VMIN_VAL		(0x20) /* volt domain: 0.6v*/
#define VMIN_VAL_D4		(0x28) /* volt domain: 0.65v*/
#define VMIN_VAL_D5		(0x24) /* volt domain: 0.625v*/
#define VCO_VAL			(0x18)
#define DVTFIXED_VAL	(0x6)

#define DVTFIXED_M_VAL	(0x6)

#define CLAMP_VMAX_VAL_B_T			(0x60) /* volt domain: 1v */
#define CLAMP_VMAX_VAL_B_T_AGING		(0x5C) /* volt domain: 0.975v */
#define CLAMP_VMAX_PMIC_VAL_B_T			(0xA0) /* pmic domain: 1v */
#define CLAMP_VMAX_PMIC_VAL_B_T_AGING	(0x9C) /* pmic domain: 0.975v */

#define VMAX_VAL_B_T		(0x73) /* volt domain: 1.11875v */

#define VMAX_VAL_B		(0x60) /* volt domain: 1v */
#define VMIN_VAL_B		(0x20) /* volt domain: 0.6v*/
#define VMIN_VAL_B_D4	(0x28) /* volt domain: 0.65v*/
#define VMIN_VAL_B_D5	(0x24) /* volt domain: 0.625v*/
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
#define VMIN_VAL_GPU                    (0x18) /* eem domain: 0.55v*/
#define VCO_VAL_GPU                     (0x18) /* eem domain: 0.55v*/

/* different for GPU_L */
#define VMAX_VAL_GL                     (0x60)
#define VMIN_VAL_GL                     (0x18)
#define VCO_VAL_GL                      (0x18)
#define DVTFIXED_VAL_GL					(0x04)
#define DVTFIXED_VAL_GPU				(0x04)

/* different for GPU_H */
#define VMAX_VAL_GH                     (0x60) /* volt domain: 1.11875v*/
#define VMIN_VAL_GH                     (0x18)
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


#define LOW_TEMP_VAL		(18000)
#define EXTRA_LOW_TEMP_VAL	(10000)
#define HIGH_TEMP_VAL		(85000)

#define LOW_TEMP_OFF_DEFAULT	(0)
#define LOW_TEMP_OFF_L		(8)
#define HIGH_TEMP_OFF_L		(3)
#define LOW_TEMP_OFF_B		(8)
#define HIGH_TEMP_OFF_B		(3)
#define LOW_TEMP_OFF_GPU		(4)
#define HIGH_TEMP_OFF_GPU		(3)
#define EXTRA_LOW_TEMP_OFF_GPU	(7)
#define MARGIN_ADD_OFF			(5)
#define MARGIN_CLAMP_OFF		(8)



/* for EEMCTL0's setting */
#define EEM_CTL0_L			(0xBA98000F)
#define EEM_CTL0_B			(0x00540003)
#define EEM_CTL0_CCI		(0xBA98000F)
#define EEM_CTL0_GPU		(0x00540003)
#define EEM_CTL0_VPU		(0x00210003)


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
