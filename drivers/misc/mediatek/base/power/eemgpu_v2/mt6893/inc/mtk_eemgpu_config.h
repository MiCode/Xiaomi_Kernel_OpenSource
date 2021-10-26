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
/* #define EEMG_NOT_READY		(1) */
#define CONFIG_EEMG_SHOWLOG	(0)
#define EN_ISR_LOG		(0)
#define EEMG_BANK_SOC		(0) /* use voltage bin, so disable it */
#define EARLY_PORTING		(0)
#define DUMP_DATA_TO_DE		(1)
#define EEMG_ENABLE		(1) /* enable; after pass HPT mini-SQC */
#define EEMG_FAKE_EFUSE		(1)
//#define MT6885
//#define MT6889
//#define MC50_LOAD

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



#define DEVINFO_IDX_0 50
#define DEVINFO_IDX_1 63
#define DEVINFO_IDX_2 64
#define DEVINFO_IDX_3 65
#define DEVINFO_IDX_4 66



#define DEVINFO_IDX_FAB4 134


#if EEMG_FAKE_EFUSE		/* select secure mode based on efuse config */
#define SEC_MOD_SEL			0x00		/* non secure  mode */
#else
#define SEC_MOD_SEL			0x00		/* Secure Mode 0 */
/* #define SEC_MOD_SEL			0x10	*/	/* Secure Mode 1 */
/* #define SEC_MOD_SEL			0x20	*/	/* Secure Mode 2 */
/* #define SEC_MOD_SEL			0x30	*/	/* Secure Mode 3 */
/* #define SEC_MOD_SEL			0x40	*/	/* Secure Mode 4 */
#endif


#if defined(CMD_LOAD)
#define DEVINFO_0 0x0 /* MC50 Safe EFUSE */
#define DEVINFO_1 0xA8FB1B03
#define DEVINFO_2 0x2D15247F
#define DEVINFO_3 0x7AE3247F
#define DEVINFO_4 0x0F192439

#elif defined(MC50_LOAD)

#if defined(MT6885)
#define DEVINFO_0 0x0 /* MC50 Safe EFUSE */
#define DEVINFO_1 0xA8FB1B03
#define DEVINFO_2 0x2D15247F
#define DEVINFO_3 0x7AE3247F
#define DEVINFO_4 0x0F192439

#elif defined(MT6889)
#define DEVINFO_0 0x0 /* MC50 Safe EFUSE */
#define DEVINFO_1 0xA8FB1B03
#define DEVINFO_2 0x2D15247F
#define DEVINFO_3 0x7AE3247F
#define DEVINFO_4 0x0F192439

#endif

#else

#define DEVINFO_0 0x0 /* MC50 Safe EFUSE */
#define DEVINFO_1 0xA8FB1B03
#define DEVINFO_2 0x2D15247F
#define DEVINFO_3 0x7AE3247F
#define DEVINFO_4 0x0F192439

#endif


/*****************************************
 * eem sw setting
 ******************************************
 */
#define NR_HW_RES_FOR_BANK	(5) /* real eem banks for efuse */
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


#define GPU_FREQ_BASE		836000
#define GPU_M_FREQ_BASE		675000

#define BANK_GPU_TURN_PT	6


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


#define DTHI_VAL		(0x01) /* positive */
#define DTLO_VAL		(0xfe) /* negative (2's compliment) */
/* This timeout value is in cycles of bclk_ck. */
#define DETMAX_VAL		(0xffff)
#define AGECONFIG_VAL	(0x555555)
#define AGEM_VAL		(0x0)
#define DCCONFIG_VAL	(0x1)




/* different for GPU */
#define VMAX_VAL_GPU                    (0x60) /* eem domain: 1v*/
#define VMIN_VAL_GPU                    (0x18) /* eem domain: 0.55v*/
#define VMIN_VAL_GPU_SEG2               (0x20) /* volt domain: 0.6v*/
#define VCO_VAL_GPU                     (0x18) /* eem domain: 0.55v*/


/* different for GPU_L */
#define VMAX_VAL_GL                     (0x60)
#define VMIN_VAL_GL                     (0x1B) /* eem domain: 0.56875v*/
#define VCO_VAL_GL                      (0x18)
#define DVTFIXED_VAL_GL					(0x01)
#define DVTFIXED_VAL_GPU				(0x06)

/* different for GPU_H */
#define VMAX_VAL_GH                     (0x60) /* volt domain: 1.11875v*/
#define VMIN_VAL_GH                     (0x1B) /* eem domain: 0.56875v*/
#define VCO_VAL_GH                      (0x18)


/* use in base_ops_mon_mode */
#define MTS_VAL			(0x1fb)
#define BTS_VAL			(0x6d1)

#define CORESEL_VAL			(0x8fff0000)
#define CORESEL_INIT2_VAL		(0x0fff0000)


#define LOW_TEMP_VAL		(25000)
#define EXTRA_LOW_TEMP_VAL	(10000)
#define HIGH_TEMP_VAL		(85000)

#define LOW_TEMP_OFF_DEFAULT	(0)
#define LOW_TEMP_OFF_GPU		(4)
#define HIGH_TEMP_OFF_GPU		(0)
#define EXTRA_LOW_TEMP_OFF_GPU	(7)
#define MARGIN_ADD_OFF			(5)
#define MARGIN_CLAMP_OFF		(8)



/* for EEMCTL0's setting */
#define EEMG_CTL0_GPU		(0x00540003)
#define EEMG_CTL0_VPU		(0x00210003)


#define AGING_VAL_GPU		(0x0) /* GPU aging margin : 43.75mv*/


#endif
