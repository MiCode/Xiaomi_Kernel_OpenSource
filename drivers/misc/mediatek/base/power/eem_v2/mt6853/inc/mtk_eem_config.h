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
#ifdef CONFIG_MT6360_PMIC
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
#define EEM_FAKE_EFUSE		(1)
#define UPDATE_TO_UPOWER	(1)
#define EEM_LOCKTIME_LIMIT	(3000)
#define SUPPORT_PICACHU		(1)
#define SUPPORT_PI_LOG_AREA		(0)

#define ENABLE_INIT_TIMER	(1)


#define EEM_OFFSET
#define SET_PMIC_VOLT		(0)
#define SET_PMIC_VOLT_TO_DVFS	(1)
#define LOG_INTERVAL		(100LL * MSEC_PER_SEC)
#define DVT			(0)
#define SUPPORT_DCONFIG		(1)
#define ENABLE_HT_FT		(1)
//#define EARLY_PORTING_VPU
#define ENABLE_MINIHQA		(0)
#define ENABLE_REMOVE_AGING	(0)
#define EN_TEST_EQUATION	(0)
#define FAKE_SN_DVT_EFUSE_FOR_DE	(0)
#define ENABLE_COUNT_SNTEMP		(0)

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
#define DEVINFO_IDX_27 209


#define DEVINFO_TIME_IDX 132



/* select secure mode based on efuse config */
#define SEC_MOD_SEL			0x00		/* non secure  mode */


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
#define DEVINFO_18 0x10bd3c1b
#define DEVINFO_19 0x10bd3c1b
#define DEVINFO_23 0x550055
#define DEVINFO_24 0x10bd3c1b

#else /* MC99 Safe EFUSE */

#if defined(MC50_LOAD)

#if defined(MT6873) //2.0GHz

#define SPARE1_VAL 0x404f5a50

#define DEVINFO_0 0x00000001
#define DEVINFO_1 0x0
#define DEVINFO_2 0x7517242B
#define DEVINFO_3 0x57142476
#define DEVINFO_4 0x49172454
#define DEVINFO_5 0x0
#define DEVINFO_6 0x0
#define DEVINFO_7 0x4D152457
#define DEVINFO_8 0x550055
#define DEVINFO_9 0x56EC00DC
#define DEVINFO_10 0x081800A6
#define DEVINFO_11 0x6F572444
#define DEVINFO_12 0x725C244D
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x0
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0x1B031B03
#define DEVINFO_18 0x5D025D02

#define DEVINFO_21 0x65786E64
#define DEVINFO_22 0x0000796F
#define DEVINFO_23 0x01196764
#define DEVINFO_24 0x38673966
#define DEVINFO_25 0x33683467
#define DEVINFO_26 0x756A7767


#elif defined(MT6875) //2.2GHz

#define SPARE1_VAL 0x404f6050

#define DEVINFO_0 0x00000001
#define DEVINFO_1 0x0
#define DEVINFO_2 0x97ED2425
#define DEVINFO_3 0x57142476
#define DEVINFO_4 0x49172454
#define DEVINFO_5 0x0
#define DEVINFO_6 0x0
#define DEVINFO_7 0x4D152457
#define DEVINFO_8 0x550055
#define DEVINFO_9 0x56EC00DC
#define DEVINFO_10 0x081800A6
#define DEVINFO_11 0x6F572444
#define DEVINFO_12 0x725C244D
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x0
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0x1B031B03
#define DEVINFO_18 0x5D025D02

#define DEVINFO_21 0x65786E64
#define DEVINFO_22 0x0000796F
#define DEVINFO_23 0x01196764
#define DEVINFO_24 0x38673966
#define DEVINFO_25 0x33683467
#define DEVINFO_26 0x756A7767


#elif defined(MT6875T) //#2.6GHz

#define SPARE1_VAL 0x374c554c

#define DEVINFO_0 0x00000001
#define DEVINFO_1 0x0
#define DEVINFO_2 0x5A1E242A
#define DEVINFO_3 0x57142876
#define DEVINFO_4 0x49172854
#define DEVINFO_5 0x0
#define DEVINFO_6 0x0
#define DEVINFO_7 0x4D152857
#define DEVINFO_8 0x550055
#define DEVINFO_9 0x56EC00DC
#define DEVINFO_10 0x081800A6
#define DEVINFO_11 0x6F572444
#define DEVINFO_12 0x725C244D
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x0
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0x1B031B03
#define DEVINFO_18 0x5D025D02

#define DEVINFO_21 0x65786E64
#define DEVINFO_22 0x0000796F
#define DEVINFO_23 0x01196764
#define DEVINFO_24 0x38673966
#define DEVINFO_25 0x33683467
#define DEVINFO_26 0x756A7767


#endif

#else

#define DEVINFO_0 0x0
#define DEVINFO_1 0x6610240A
#define DEVINFO_2 0x98EB2424
#define DEVINFO_3 0x4112243E
#define DEVINFO_4 0x70152430
#define DEVINFO_5 0x591F2450
#define DEVINFO_6 0x4513243C
#define DEVINFO_7 0x70152430
#define DEVINFO_8 0x2E152404
#define DEVINFO_9 0x56112477
#define DEVINFO_10 0x3914243F
#define DEVINFO_11 0xC3990089
#define DEVINFO_12 0xDC910089
#define DEVINFO_13 0x1B031B03
#define DEVINFO_14 0x1B031B03
#define DEVINFO_15 0x1B031B03
#define DEVINFO_16 0x1B031B03
#define DEVINFO_17 0x1B031B03

#define DEVINFO_21 0x65786E64
#define DEVINFO_22 0x0000796F
#define DEVINFO_23 0x013C6764
#define DEVINFO_24 0x38673966
#define DEVINFO_25 0x33683467
#define DEVINFO_26 0x756A7767

#define PICACHU_VMIN_HI	0x44535F53


#endif

#endif


/*******************************************
 * eemsn sw setting
 ********************************************
 */
#define NR_HW_RES_FOR_BANK (24) /* real eemsn banks for efuse */
#define IDX_HW_RES_SN (18) /* start index of Sensor Network efuse */

#define NR_FREQ 16
#define NR_FREQ_CPU 16


#define L_FREQ_BASE			2000000
#define B_FREQ_BASE			2210000
#define	CCI_FREQ_BASE		1400000
#define B_M_FREQ_BASE		1650000

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


/* common part: for cci, LL, L, GPU */
/* common part: for  LL, L */
#define VBOOT_PMIC_VAL	(80000)
#define VBOOT_PMIC_CLR	(0)
#define VBOOT_VAL		(0x40) /* volt domain: 0.75v */
#define VMAX_VAL		(0x60) /* volt domain: 1v*/
#define VMIN_VAL		(0x20) /* volt domain: 0.6v*/
#define VCO_VAL			(0x20)
#define DVTFIXED_VAL	(0x6)
#define DVTFIXED_VAL_V2	(10)

#define DVTFIXED_M_VAL	(0x07)


#define VMAX_VAL_B		(0x60) /* volt domain: 1v*/
#define VMIN_VAL_B		(0x20) /* volt domain: 0.6v*/
#define VCO_VAL_B		(0x20) /* volt domain: 0.6v*/
#define DVTFIXED_VAL_B	(0x6)
#define DVTFIXED_VAL_B_V2	(10)

#define DTHI_VAL		(0x01) /* positive */
#define DTLO_VAL		(0xfe) /* negative (2's compliment) */
/* This timeout value is in cycles of bclk_ck. */
#define DETMAX_VAL		(0xffff)
#define AGECONFIG_VAL	(0x555555)
#define AGEM_VAL		(0x0)
#define DCCONFIG_VAL	(0x555555)

/* different for CCI */
#define VMAX_VAL_CCI		(0x60) /* volt domain: 1v*/
#define VMIN_VAL_CCI		(0x20)
#define VCO_VAL_CCI			(0x20)
#define DVTFIXED_VAL_CCI	(0x6)
#define DVTFIXED_VAL_CCI_V2	(10)

/* different for L_L */
#define VMAX_VAL_LL                     (0x37)
#define VMIN_VAL_LL                     (0x15)
#define VCO_VAL_LL                      (0x15)

/* different for B_L */
#define VMAX_VAL_BL                     (0x60) /* volt domain: 1v*/
#define VMIN_VAL_BL                     (0x20)
#define VCO_VAL_BL                      (0x20)
#define DVTFIXED_VAL_BL					(0x6)

/* different for L_H */
#define VMAX_VAL_H			(0x50)
#define VMIN_VAL_H			(0x30)
#define VCO_VAL_H			(0x30)
#define DVTFIXED_VAL_H			(0x03)

/* different for B_H */
#define VMAX_VAL_BH			(0x60) /* volt domain: 1v*/
#define VMIN_VAL_BH			(0x20)
#define VCO_VAL_BH			(0x20)
#define DVTFIXED_VAL_BH		(0x6)


/* use in base_ops_mon_mode */
#define MTS_VAL			(0x1fb)
#define BTS_VAL			(0x6d1)

#define CORESEL_VAL			(0x8fff0000)
#define CORESEL_INIT2_VAL		(0x0fff0000)

#define SN_DELTA_VC_MAX			(6)
#define SN_DELTA_VC_MIN			(-6)
#define VOLT_T_EFUSE_HV			(100000)
#define VOLT_T_EFUSE_LV			(75000)

/* #define SN_TEMP_MARGIN			(5) */
#define SN_IR_GAIN				(3)


#define LOW_TEMP_VAL		(18)
#define HIGH_TEMP_VAL		(85)

#if 0
#define LOW_TEMP_OFF_DEFAULT	(0)
#define EXTRA_TEMP_OFF_L		(8)
#define EXTRA_TEMP_OFF_B		(8)
#define EXTRA_TEMP_OFF_GPU		(6)
#define EXTRA_TEMP_OFF_B_LO		(2)
#define MARGIN_ADD_OFF			(5)
#define MARGIN_CLAMP_OFF		(8)
#endif

#define LOW_TEMP_OFF		(8)
#define NORM_TEMP_OFF		(4)
#define NORM_TEMP_OFF_1850_L	(4)
#define NORM_TEMP_OFF_5085_L	(4)

#define NORM_TEMP_OFF_B			(8)
#define NORM_TEMP_OFF_1850_B	(8)
#define NORM_TEMP_OFF_5085_B	(8)
#define HIGH_TEMP_OFF			(3)

#define L_FREQ_LOW_M1	0
#define L_FREQ_LOW_M2	0
#define L_FREQ_LOW_M3	0
#define L_FREQ_LOW_M4	0
#define L_FREQ_LOW_D1	650
#define L_FREQ_LOW_D2	703
#define L_FREQ_LOW_D3	650
#define L_FREQ_LOW_D4	650
#define L_FREQ_LOW_NUM	5

#define B_FREQ_LOW_M1	0
#define B_FREQ_LOW_M2	0
#define B_FREQ_LOW_M3	0
#define B_FREQ_LOW_M4	0
#define B_FREQ_LOW_M5	0
#define B_FREQ_LOW_D1	725
#define B_FREQ_LOW_D2	898
#define B_FREQ_LOW_D3	729
#define B_FREQ_LOW_D4	725
#define B_FREQ_LOW_D5	725
#define B_FREQ_LOW_NUM	6


#define CCI_FREQ_LOW_M1	0
#define CCI_FREQ_LOW_M2	0
#define CCI_FREQ_LOW_M3	0
#define CCI_FREQ_LOW_M4	0
#define CCI_FREQ_LOW_D1	450
#define CCI_FREQ_LOW_D2	487
#define CCI_FREQ_LOW_D3	450
#define CCI_FREQ_LOW_D4	450
#define CCI_FREQ_LOW_NUM	5

#define AGING_VAL_CPU		(0x0) /* CPU aging margin : 31mv*/
#define AGING_VAL_CPU_B		(0x0) /* CPU aging margin : 37mv*/
#define AGING_VAL_GPU		(0x0) /* GPU aging margin : 43.75mv*/

#endif
