/*****************************************************************************
 *
 * Filename:
 * ---------
 *    pmic6326_sw.h
 *
 * Project:
 * --------
 *   Maui_Software
 *
 * Description:
 * ------------
 *   This file is intended for PMIC 6326 driver.
 *
 * Author:
 * -------
 *  mtk01845
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision$
 * $Modtime:   20 Jun 2005 20:03:54  $
 * $Log:   //mtkvs01/vmdata/Maui_sw/archives/mcu/drv/src/gpt.c-arc  $
 *
 * 08 09 2010 chipeng.chang
 * [ALPS00003297] [Need Patch] [Volunteer Patch] android 2.2 migration
 * remove .o and add header file.
 *
 * Feb 5 2009 mtk01845
 * [MAUI_01624867] [Drv] PMIC6326 ECO version compatible support
 *
 *
 * Jan 19 2009 mtk01845
 * [MAUI_01314361] [Drv]MT6268 check to MAUI
 *
 *
 * Sep 12 2008 mtk01845
 * [MAUI_00803843] [Drv][PMIC6326] Check-in to MAUI
 *
 *
 * Sep 2 2008 mtk01845
 * [MAUI_01231139] [Drv][PMIC6326] Redundent custom APIs cause MoDIS link error
 *
 *
 * Aug 29 2008 mtk01845
 * [MAUI_00803843] [Drv][PMIC6326] Check-in to MAUI
 *
 *
 * Aug 6 2008 mtk01845
 * [MAUI_00803843] [Drv][PMIC6326] Check-in to MAUI
 *
 *
 * Aug 5 2008 mtk01845
 * [MAUI_00803843] [Drv][PMIC6326] Check-in to MAUI
 *
 *
 * Jul 13 2008 mtk01845
 * [MAUI_00803843] [Drv][PMIC6326] Check-in to MAUI
 *
 *
 * Jun 23 2008 mtk01845
 * [MAUI_00791553] [Drv] MT6268A merge DVT code back to MAUI
 *
 *
 * Jun 23 2008 mtk01845
 * [MAUI_00791553] [Drv] MT6268A merge DVT code back to MAUI
 *
 *
 * Jun 23 2008 mtk01845
 * [MAUI_00791553] [Drv] MT6268A merge DVT code back to MAUI
 *
 *
 * Jun 20 2008 mtk01845
 * [MAUI_00791553] [Drv] MT6268A merge DVT code back to MAUI
 *
*------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#ifndef __PMIC6326_SW_H__
#define __PMIC6326_SW_H__
/* #include "pmic_features.h" */


#ifndef __PRODUCTION_RELEASE__
/* Define to keep chaging when assert */
/* This flag is only for debug purpose */
/* #define DRV_MISC_PMIC_ASSERT_KEEP_CHARGING */
#endif				/* #ifndef __PRODUCTION_RELEASE__ */


#ifdef __PRODUCTION_RELEASE__
/* Define to enable PMIC6326 charger watch dog timer kick */
/* When enable charger, PMIC6326 will enable a watch dog timer */
/* We need to kick the timer periodically, to ontify PMIC6326 that BB is alive */
/* If timeout, PMIC6326 will disable charge automatically */
/* #### If this is NOT enabled, we will disable the watch dog timer function at boot time */
#define ENABLE_KICK_PMIC6326_CHARGER_WATCHDOG_TIMER
#endif				/* #ifdef __PRODUCTION_RELEASE__ */



#define PMIC6326_ECO_1_VERSION		0x01
#define PMIC6326_ECO_2_VERSION		0x02
#define PMIC6326_ECO_3_VERSION		0x03
#define PMIC6326_ECO_4_VERSION		0x04


typedef struct {
	void (*pmic_ac_det) (void);
	void (*pmic_usb_det) (void);
} pmic6326_chrdect_callbac_struct;



/* TTTTTTTTTTTTTTTTT */
/* Implemented functions */



/* (0x0D) INT STATUS 3 (RO) */
typedef enum {
	VSDIO_OC_STAT = 0x01,	/* BIT0 */
	VGP_OC_STAT = 0x02,	/* BIT1 */
	VUSB_OC_STAT = 0x04,	/* BIT2 */
	OVP_INT_STAT = 0x08,	/* BIT3 */
	CHRDET_INT_STAT = 0x10,	/* BIT4 */
	PWRKEY_INT_STAT = 0x20	/* BIT5 */
} int_state_3_enum;


/* (0x1B) LDO CTRL 2 VRF */
typedef enum {
	VRF_BIAS_CURRENT_TIMES_1_0 = 0,
	VRF_BIAS_CURRENT_TIMES_0_5,
	VRF_BIAS_CURRENT_TIMES_2_0,
	VRF_BIAS_CURRENT_TIMES_3_0
} vrf_ical_en_enum;

/* (0x1C) LDO CTRL 3 VRF */
typedef enum {
	VRF_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VRF_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VRF_MAX_SLEW_RATE,
	VRF_MAX_SLEW_RATE_TIMES_1_OVER_5
} vrf_calst_enum;
typedef enum {
	VRF_OC_THRESHOLD_685MA = 0,
	VRF_OC_THRESHOLD_635MA,
	VRF_OC_THRESHOLD_785MA,
	VRF_OC_THRESHOLD_735MA
} vrf_caloc_enum;

typedef enum {
	VRF_DEFAULT_MILLER_CAPACITOR = 0,
	VRF_INCREASE_MILLER_CAPACITOR
} vrf_cm_enum;

typedef enum {
	VRF_ENABLE_WITH_SRCLKEN = 0,
	VRF_ENABLE_WITH_VRF_EN
} vrf_on_sel_enum;


/* (0x1E) LDO CTRL 5 VTCXO */
typedef enum {
	VTCXO_BIAS_CURRENT_TIMES_1_0 = 0,
	VTCXO_BIAS_CURRENT_TIMES_0_5,
	VTCXO_BIAS_CURRENT_TIMES_2_0,
	VTCXO_BIAS_CURRENT_TIMES_3_0
} vtcxo_ical_en_enum;


/* (0x1F) LDO CTRL 6 VTCXO */
typedef enum {
	VTCXO_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VTCXO_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VTCXO_MAX_SLEW_RATE,
	VTCXO_MAX_SLEW_RATE_TIMES_1_OVER_5
} vtcxo_calst_enum;
typedef enum {
	VTCXO_OC_THRESHOLD_100MA = 0,
	VTCXO_OC_THRESHOLD_109MA,
	VTCXO_OC_THRESHOLD_82MA,
	VTCXO_OC_THRESHOLD_91MA
} vtcxo_caloc_enum;

typedef enum {
	VTCXO_ENABLE_WITH_SRCLKEN = 0,
	VTCXO_ENABLE_WITH_VTCXO_EN
} vtcxo_on_sel_enum;

typedef enum {
	VTCXO_DEFAULT_MILLER_CAPACITOR = 0,
	VTCXO_INCREASE_MILLER_CAPACITOR
} vtcxo_cm_enum;

/* (0x21) LDO CTRL 8 V3GTX */
typedef enum {
	V3GTX_BIAS_CURRENT_TIMES_1_0 = 0,
	V3GTX_BIAS_CURRENT_TIMES_0_5,
	V3GTX_BIAS_CURRENT_TIMES_2_0,
	V3GTX_BIAS_CURRENT_TIMES_3_0
} v3gtx_ical_en_enum;

typedef enum {
	V3GTX_2_8 = 0,
	V3GTX_3_0,
	V3GTX_3_3,
	V3GTX_2_5
} v3gtx_vol;


/* (0x22) LDO CTRL 9 V3GTX */
typedef enum {
	V3GTX_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	V3GTX_MAX_SLEW_RATE_TIMES_1_OVER_21,
	V3GTX_MAX_SLEW_RATE,
	V3GTX_MAX_SLEW_RATE_TIMES_1_OVER_5
} v3gtx_calst_enum;

typedef enum {
	V3GTX_OC_THRESHOLD_400MA = 0,
	V3GTX_OC_THRESHOLD_438MA,
	V3GTX_OC_THRESHOLD_324MA,
	V3GTX_OC_THRESHOLD_362MA
} v3gtx_caloc_enum;

typedef enum {
	V3GTX_ENABLE_WITH_SRCLKEN = 0,
	V3GTX_ENABLE_WITH_V3GTX_EN
} v3gtx_on_sel_enum;


/* (0x24) LDO CTRL 11 V3GRX */
typedef enum {
	V3GRX_2_8 = 0,
	V3GRX_3_0,
	V3GRX_3_3,
	V3GRX_2_5
} v3grx_vol;

typedef enum {
	V3GRX_BIAS_CURRENT_TIMES_1_0 = 0,
	V3GRX_BIAS_CURRENT_TIMES_0_5,
	V3GRX_BIAS_CURRENT_TIMES_2_0,
	V3GRX_BIAS_CURRENT_TIMES_3_0
} v3grx_ical_en_enum;


/* (0x25) LDO CTRL 12 V3GRX */
typedef enum {
	V3GRX_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	V3GRX_MAX_SLEW_RATE_TIMES_1_OVER_21,
	V3GRX_MAX_SLEW_RATE,
	V3GRX_MAX_SLEW_RATE_TIMES_1_OVER_5
} v3grx_calst_enum;

typedef enum {
	V3GRX_OC_THRESHOLD_200MA = 0,
	V3GRX_OC_THRESHOLD_219MA,
	V3GRX_OC_THRESHOLD_162MA,
	V3GRX_OC_THRESHOLD_181MA
} v3grx_caloc_enum;


typedef enum {
	V3GRX_ENABLE_WITH_SRCLKEN = 0,
	V3GRX_ENABLE_WITH_V3GRX_EN
} v3grx_on_sel_enum;


/* (0x2E) LDO CTRL 21 VCAMA */
typedef enum {
	VCAMA_2_8 = 0,
	VCAMA_2_5,
	VCAMA_1_8,
	VCAMA_1_5
} vcama_sel_enum;
typedef enum {
	VCAMA_BIAS_CURRENT_X_1_0 = 0,
	VCAMA_BIAS_CURRENT_X_0_5,
	VCAMA_BIAS_CURRENT_X_2_0,
	VCAMA_BIAS_CURRENT_X_3_0
} vcama_ical_en_enum;

/* (0x2F) LDO CTRL 22 VCAMA */
typedef enum {
	VCAMA_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VCAMA_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VCAMA_MAX_SLEW_RATE,
	VCAMA_MAX_SLEW_RATE_TIMES_1_OVER_5
} vcama_calst_enum;
typedef enum {
	VCAMA_OC_THRESHOLD_500MA = 0,
	VCAMA_OC_THRESHOLD_548MA,
	VCAMA_OC_THRESHOLD_405MA,
	VCAMA_OC_THRESHOLD_452MA
} vcama_caloc_enum;

typedef enum {
	VCAMA_DEFAULT_MILLER_CAPACITOR = 0,
	VCAMA_INCREASE_MILLER_CAPACITOR
} vcama_cm_enum;

/* (0x31) LDO CTRL 24 VWIFI3V3 */
typedef enum {
	VWIFI3V3_2_8 = 0,
	VWIFI3V3_3_0,
	VWIFI3V3_3_3,
	VWIFI3V3_2_5
} vwifi3v3_sel_enum;
typedef enum {
	VWIFI3V3_BIAS_CURRENT_X_1_0 = 0,
	VWIFI3V3_BIAS_CURRENT_X_0_5,
	VWIFI3V3_BIAS_CURRENT_X_2_0,
	VWIFI3V3_BIAS_CURRENT_X_3_0
} vwifi3v3_ical_en_enum;

/* (0x32) LDO CTRL 25 VWIFI3V3 */
typedef enum {
	VWIFI3V3_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VWIFI3V3_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VWIFI3V3_MAX_SLEW_RATE,
	VWIFI3V3_MAX_SLEW_RATE_TIMES_1_OVER_5
} vwifi3v3_calst_enum;
typedef enum {
	VWIFI3V3_OC_THRESHOLD_600MA = 0,
	VWIFI3V3_OC_THRESHOLD_657MA,
	VWIFI3V3_OC_THRESHOLD_486MA,
	VWIFI3V3_OC_THRESHOLD_543MA
} vwifi3v3_caloc_enum;

typedef enum {
	VWIFI3V3_DEFAULT_MILLER_CAPACITOR = 0,
	VWIFI3V3_INCREASE_MILLER_CAPACITOR
} vwifi3v3_cm_enum;

/* (0x34) LDO CTRL 27 VWIFI2V8 */
typedef enum {
	VWIFI2V8_2_8 = 0,
	VWIFI2V8_3_0,
	VWIFI2V8_3_3,
	VWIFI2V8_2_5
} vwifi2v8_sel_enum;
typedef enum {
	VWIFI2V8_BIAS_CURRENT_X_1_0 = 0,
	VWIFI2V8_BIAS_CURRENT_X_0_5,
	VWIFI2V8_BIAS_CURRENT_X_2_0,
	VWIFI2V8_BIAS_CURRENT_X_3_0
} vwifi2v8_ical_en_enum;

/* (0x35) LDO CTRL 28 VWIFI2V8 */
typedef enum {
	VWIFI2V8_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VWIFI2V8_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VWIFI2V8_MAX_SLEW_RATE,
	VWIFI2V8_MAX_SLEW_RATE_TIMES_1_OVER_5
} vwifi2v8_calst_enum;
typedef enum {
	VWIFI2V8_OC_THRESHOLD_300MA = 0,
	VWIFI2V8_OC_THRESHOLD_329MA,
	VWIFI2V8_OC_THRESHOLD_243MA,
	VWIFI2V8_OC_THRESHOLD_271MA
} vwifi2v8_caloc_enum;

typedef enum {
	VWIFI2V8_DEFAULT_MILLER_CAPACITOR = 0,
	VWIFI2V8_INCREASE_MILLER_CAPACITOR
} vwifi2v8_cm_enum;

/* (0x37) LDO CTRL 30 VSIM */
typedef enum {
	VSIM_1_3V = 0,
	VSIM_1_5V,
	VSIM_1_8V,
	VSIM_2_5V,
	VSIM_2_8V,
	VSIM_3_0V,
	VSIM_3_3V,
	VSIM_1_2V
} vsim_sel_enum;
typedef enum {
	VSIM_BIAS_CURRENT_X_1_0 = 0,
	VSIM_BIAS_CURRENT_X_0_5,
	VSIM_BIAS_CURRENT_X_2_0,
	VSIM_BIAS_CURRENT_X_3_0
} vsim_ical_en_enum;

/* (0x3A) LDO CTRL 33 VUSB */
typedef enum {
	VUSB_1_3 = 0,
	VUSB_1_5,
	VUSB_1_8,
	VUSB_2_5,
	VUSB_2_8,
	VUSB_3_0,
	VUSB_3_3,
	VUSB_1_2		/* TTTTTTTTTTTTTT */
} vusb_sel_enum;
typedef enum {
	VUSB_BIAS_CURRENT_X_1_0 = 0,
	VUSB_BIAS_CURRENT_X_0_5,
	VUSB_BIAS_CURRENT_X_2_0,
	VUSB_BIAS_CURRENT_X_3_0
} vusb_ical_en_enum;

/* (0x3B) LDO CTRL 34 VUSB */
typedef enum {
	VUSB_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VUSB_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VUSB_MAX_SLEW_RATE,
	VUSB_MAX_SLEW_RATE_TIMES_1_OVER_5
} vusb_calst_enum;
typedef enum {
	VUSB_OC_THRESHOLD_200MA = 0,
	VUSB_OC_THRESHOLD_218MA,
	VUSB_OC_THRESHOLD_164MA,
	VUSB_OC_THRESHOLD_182MA
} vusb_caloc_enum;

/* (0x3D) LDO CTRL 36 VBT */
typedef enum {
	VBT_1_3 = 0,
	VBT_1_5,
	VBT_1_8,
	VBT_2_5,
	VBT_2_8,
	VBT_3_0,
	VBT_3_3,
	VBT_1_2			/* TTTTTTTTTTTTTT */
} vbt_sel_enum;

typedef enum {
	VBT_E3_1_5 = 0,
	VBT_E3_1_3,
	VBT_E3_2_5,
	VBT_E3_1_8,
	VBT_E3_3_0,
	VBT_E3_2_8,
	VBT_E3_3_3
	    /* VBT_E3_3_3 */
} vbt_e3_sel_enum;

typedef enum {
	VBT_BIAS_CURRENT_X_1_0 = 0,
	VBT_BIAS_CURRENT_X_0_5,
	VBT_BIAS_CURRENT_X_2_0,
	VBT_BIAS_CURRENT_X_3_0
} vbt_ical_en_enum;

/* (0x3E) LDO CTRL 37 VBT */
typedef enum {
	VBT_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VBT_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VBT_MAX_SLEW_RATE,
	VBT_MAX_SLEW_RATE_TIMES_1_OVER_5
} vbt_calst_enum;
typedef enum {
	VBT_OC_THRESHOLD_200MA = 0,
	VBT_OC_THRESHOLD_218MA,
	VBT_OC_THRESHOLD_164MA,
	VBT_OC_THRESHOLD_182MA
} vbt_caloc_enum;

/* (0x40) LDO CTRL 39 VCAMD */
typedef enum {
	VCAMD_1_3 = 0,
	VCAMD_1_5,
	VCAMD_1_8,
	VCAMD_2_5,
	VCAMD_2_8,
	VCAMD_3_0,
	VCAMD_3_3,
	VCAMD_1_2		/* TTTTTTTTTTTTTT */
} vcamd_sel_enum;
typedef enum {
	VCAMD_BIAS_CURRENT_X_1_0 = 0,
	VCAMD_BIAS_CURRENT_X_0_5,
	VCAMD_BIAS_CURRENT_X_2_0,
	VCAMD_BIAS_CURRENT_X_3_0
} vcamd_ical_en_enum;

/* (0x41) LDO CTRL 40 VCAMD */
typedef enum {
	VCAMD_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VCAMD_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VCAMD_MAX_SLEW_RATE,
	VCAMD_MAX_SLEW_RATE_TIMES_1_OVER_5
} vcamd_calst_enum;
typedef enum {
	VCAMD_OC_THRESHOLD_200MA = 0,
	VCAMD_OC_THRESHOLD_218MA,
	VCAMD_OC_THRESHOLD_164MA,
	VCAMD_OC_THRESHOLD_182MA
} vcamd_caloc_enum;

/* (0x43) LDO CTRL 42 VGP */
typedef enum {
	VGP_1_3 = 0,
	VGP_1_5,
	VGP_1_8,
	VGP_2_5,
	VGP_2_8,
	VGP_3_0,
	VGP_3_3
} vgp_sel_enum;

/* (0x46) LDO CTRL 45 VSDIO */
typedef enum {
	VSDIO_BIAS_CURRENT_X_1_0 = 0,
	VSDIO_BIAS_CURRENT_X_0_5,
	VSDIO_BIAS_CURRENT_X_2_0,
	VSDIO_BIAS_CURRENT_X_3_0
} vsdio_ical_en_enum;

/* (0x47) LDO CTRL 46 VSDIO */
typedef enum {
	VSDIO_MAX_SLEW_RATE_TIMES_1_OVER_17 = 0,
	VSDIO_MAX_SLEW_RATE_TIMES_1_OVER_21,
	VSDIO_MAX_SLEW_RATE,
	VSDIO_MAX_SLEW_RATE_TIMES_1_OVER_5
} vsdio_calst_enum;
typedef enum {
	VSDIO_OC_THRESHOLD_700MA = 0,
	VSDIO_OC_THRESHOLD_767MA,
	VSDIO_OC_THRESHOLD_567MA,
	VSDIO_OC_THRESHOLD_633MA
} vsdio_caloc_enum;
typedef enum {
	VSDIO_2_8 = 0,
	VSDIO_3_0
} vsdio_sel_enum;
typedef enum {
	VSDIO_DEFAULT_MILLER_CAPACITOR = 0,
	VSDIO_INCREASE_MILLER_CAPACITOR
} vsdio_cm_enum;


/* (0x53) BUCK CTRL 11 VCORE2 */
typedef enum {
	VCORE2_ENABLE_WITH_EN_PASS = 0,
	VCORE2_ENABLE_WITH_VCORE2_EN
} vcore2_on_sel_enum;

/* (0x5C) BOOST CTRL 1 BOOST1 */
typedef enum {
	VBOOST1_VOL_3_20_V = 0,
	VBOOST1_VOL_3_35_V,
	VBOOST1_VOL_3_50_V,
	VBOOST1_VOL_3_65_V,
	VBOOST1_VOL_3_80_V,
	VBOOST1_VOL_3_95_V,
	VBOOST1_VOL_4_10_V,
	VBOOST1_VOL_4_25_V,
	VBOOST1_VOL_4_40_V,
	VBOOST1_VOL_4_55_V,
	VBOOST1_VOL_4_70_V,
	VBOOST1_VOL_4_85_V,
	VBOOST1_VOL_5_00_V,
	VBOOST1_VOL_5_15_V,
	VBOOST1_VOL_5_30_V,
	VBOOST1_VOL_5_45_V
} vboost1_tune_enum;

/* (0x5D) BOOST CTRL 2 BOOST1 */
typedef enum {
	BOOST1_SOFT_START_SPEED = 0,
	BOOST1_SOFT_START_SPEED_TIMES_2_OVER_3
} boost1_soft_st_speed_enum;

/* (0x5F) BOOST CTRL 4 BOOST2 */
typedef enum {
	VBOOST2_VOL_6_00_V = 0,
	VBOOST2_VOL_6_75_V,
	VBOOST2_VOL_7_50_V,
	VBOOST2_VOL_8_25_V,
	VBOOST2_VOL_9_00_V,
	VBOOST2_VOL_9_75_V,
	VBOOST2_VOL_10_05_V,
	VBOOST2_VOL_11_25_V,
	VBOOST2_VOL_12_00_V,
	VBOOST2_VOL_12_75_V,
	VBOOST2_VOL_13_50_V,
	VBOOST2_VOL_14_25_V,
	VBOOST2_VOL_15_00_V,
	VBOOST2_VOL_15_75_V,
	VBOOST2_VOL_16_50_V,
	VBOOST2_VOL_17_25_V
} vboost2_tune_enum;

typedef enum {
	BOOST2_OC_THRESHOLD_5UA = 0,
	BOOST2_OC_THRESHOLD_2UA,
	BOOST2_OC_THRESHOLD_10UA,
	BOOST2_OC_THRESHOLD_7UA
} boost2_oc_th_enum;

typedef enum {
	BOOST2_DIGITAL_DIMING = 0,
	BOOST2_ANALOG_DIMING
} boost2_dim_source_enum;

/* (0x61) BOOST CTRL 6 BOOST2 and BOOST */
typedef enum {
	BOOST_MODE_TYPE_I = 0,
	BOOST_MODE_TYPE_II,
	BOOST_MODE_TYPE_III,
	BOOST_MODE_TYPE_IV
} boost_mode_sel_enum;

/* (0x65) DRIVER CTRL 4 FLASH */
/* typedef enum */
/* { */
/* FLASH_CURRENT_0MA=0, */
/* FLASH_CURRENT_50MA, */
/* FLASH_CURRENT_100MA, */
/* FLASH_CURRENT_150MA, */
/* FLASH_CURRENT_200MA, */
/* FLASH_CURRENT_250MA, */
/* FLASH_CURRENT_300MA, */
/* FLASH_CURRENT_350MA, */
/* FLASH_CURRENT_400MA, */
/* FLASH_CURRENT_450MA, */
/* FLASH_CURRENT_500MA, */
/* FLASH_CURRENT_550MA */
/* }flash_i_tune_enum; */

/* (0x68) DRIVER CTRL 7 BL */
typedef enum {
	BL_I_CORSE_TUNE_4MA = 0,
	BL_I_CORSE_TUNE_8MA,
	BL_I_CORSE_TUNE_12MA,
	BL_I_CORSE_TUNE_16MA,
	BL_I_CORSE_TUNE_20MA,
	BL_I_CORSE_TUNE_24MA,
	BL_I_CORSE_TUNE_28MA,
	BL_I_CORSE_TUNE_32MA
} bl_i_corse_tune_enum;

typedef enum {
	BL_I_FINE_TUNE_0MA = 0,
	BL_I_FINE_TUNE_MINUS_1MA,
	BL_I_FINE_TUNE_MINUS_2MA,
	BL_I_FINE_TUNE_MINUS_3MA,
	BL_I_FINE_TUNE_PLUS_4MA,
	BL_I_FINE_TUNE_PLUS_3MA,
	BL_I_FINE_TUNE_PLUS_2MA,
	BL_I_FINE_TUNE_PLUS_1MA
} bl_i_fine_tune_enum;

/* (0x6D) DRIVER CTRL 12 BL */
typedef enum {
	BL_NUM_1 = 0,
	BL_NUM_2,
	BL_NUM_3,
	BL_NUM_4,
	BL_NUM_5,
	BL_NUM_6,
	BL_NUM_7,
	BL_NUM_8
} bl_number_enum;

/* (0x74) CLASS_D CTRL 4 SPKL */
typedef enum {
	SPKL_FB_FORCED_DTIN_DTIP = 0,
	SPKL_FF_FORCED_DTIN_DTIP,
	SPKL_FB_AUTO_CAL_DTCN_DTCP,
	SPKL_FF_AUTO_CAL_DTCN_DTCP
} spkl_dmode_enum;

typedef enum {
	SPKL_DTCAL_ENABLE_CLASS_D_R_READ_TIME_CAL = 0,
	SPKL_DTCAL_DISABLE_CLASS_D_R_READ_TIME_CAL
} spkl_dtcal_enum;

/* (0x79) CLASS_D CTRL 9 SPKR */
typedef enum {
	SPKR_FB_FORCED_DTIN_DTIP = 0,
	SPKR_FF_FORCED_DTIN_DTIP,
	SPKR_FB_AUTO_CAL_DTCN_DTCP,
	SPKR_FF_AUTO_CAL_DTCN_DTCP
} spkr_dmode_enum;

typedef enum {
	SPKR_DTCAL_ENABLE_CLASS_D_R_READ_TIME_CAL = 0,
	SPKR_DTCAL_DISABLE_CLASS_D_R_READ_TIME_CAL
} spkr_dtcal_enum;

/* (0x81) CHARGER CTRL 1 */
typedef enum {
	CHR_CURRENT_OFFSET_NO = 0,
	CHR_CURRENT_OFFSET_PLUS_1_STEP = 1,
	CHR_CURRENT_OFFSET_PLUS_2_STEP = 2,
	CHR_CURRENT_OFFSET_MINUS_2_STEP = 6,
	CHR_CURRENT_OFFSET_MINUS_1_STEP = 7
} cht_chr_offset_enum;

typedef enum {
	CHR_CURRENT_50MA = 0,
	CHR_CURRENT_90MA,
	CHR_CURRENT_150MA,
	CHR_CURRENT_225MA,
	CHR_CURRENT_300MA,
	CHR_CURRENT_450MA,
	CHR_CURRENT_650MA,
	CHR_CURRENT_800MA
} chr_chr_current_enum;

/* (0x83) TESTMODE CTRL 3 Analog Switch */
/* typedef enum */
/* { */
/* ASW_ASEL_ASW_2_SETS = 0, */
/* ASW_ASEL_SIMLS, */
/* ASW_ASEL_ASW_1_SET, */
/* ASW_ASEL_BL_CURRENT_SOURCE */
/* }asw_asel_enum; */
typedef enum {
	ASW_ASEL_ISINK_6_8_AS = 0,	/* ISINK 6~8 used as Analog Switch, others for BL */
	ASW_ASEL_ALL_ISINK_BL = 3	/* All ISINK used for BL */
} asw_asel_enum;


typedef enum {
	HI_Z = 0,
	RECEIVER,
	TWO_OF_RGB_DRIVER
	    /* TODO, the last item value */
} asw_bsel_enum;

/* (0x84) TESTMODE CTRL 4 Testmode */
typedef enum {
	VGP2_ENABLE_WITH_SRCLKEN = 0,
	VGP2_ENABLE_WITH_VGP2_EN
} vgp2_on_sel_enum;


/* (0x89) INT CTRL 1 */
typedef enum {
	INT_EN_VCORE2_OC = 0x01,
	INT_EN_VPA_OC = 0x02,
	INT_EN_BOOST1_OC = 0x04,
	INT_EN_BOOST2_OC = 0x08,
	INT_EN_SPKL_OC = 0x10,
	INT_EN_SPKR_OC = 0x20,
	INT_EN_V3GTX_OC = 0x40,
	INT_EN_V3GRX_OC = 0x80,
	INT_EN_0X89_ALL = 0xFF,
	INT1_EN_ALL = 0xFF
} int_ctrl_1_enum;

/* (0x8A) INT CTRL 2 */
typedef enum {
	INT_EN_RF_OC = 0x01,
	INT_EN_VTCXO_OC = 0x02,
	INT_EN_VCAMA_OC = 0x04,
	INT_EN_VWIFI3V3_OC = 0x08,
	INT_EN_VWIFI2V8_OC = 0x10,
	INT_EN_VSIM_OC = 0x20,
	INT_EN_VBT_OC = 0x40,
	INT_EN_VCAMD_OC = 0x80,
	INT_EN_0X8A_ALL = 0xFF,
	INT2_EN_ALL = 0xFF
} int_ctrl_2_enum;

/* (0x8B) INT CTRL 3 */
typedef enum {
	INT_EN_VSDIO_OC = 0x01,
	INT_EN_VGP_OC = 0x02,
	INT_EN_VUSB_OC = 0x04,
	INT_EN_CHRDET = 0x08,
	INT_EN_OVP = 0x10,
	INT_EN_WATCHDOG = 0x20,
	INT_EN_PWRKEY = 0x40,
	INT_EN_0X8B_ALL = 0x7F,
	INT3_EN_ALL = 0x7F
} int_ctrl_3_enum;


/* (0x96) WATCHDOG CTRL and INT CTRL 4 */
typedef enum {
	WDT_TIMEOUT_4_SEC = 0,
	WDT_TIMEOUT_8_SEC,
	WDT_TIMEOUT_16_SEC,
	WDT_TIMEOUT_32_SEC
} wdt_timout_enum;


/* Combinational functions structures */
typedef enum {
	VGP2_1_3 = 0,
	VGP2_1_5,
	VGP2_1_8,
	VGP2_2_5,
	VGP2_2_8,
	VGP2_3_0,
	VGP2_3_3
} vgp2_sel_enum;


#if 0
void pmic_init(void);


/* (0x09) STATUS 6 (RO) */
extern kal_bool pmic_boost2_oc_status(void);
extern kal_bool pmic_spkr_oc_det_status(void);
extern kal_bool pmic_spkl_oc_det_status(void);
extern kal_bool pmic_pwrkey_deb_status(void);
extern kal_bool pmic_ovp_status(void);
extern kal_bool pmic_chrdet_status(void);
extern kal_bool pmic_bat_on_status(void);
extern kal_bool pmic_cv_status(void);

/* (0x0D) INT STATUS 3 (RO) */
extern kal_uint8 pmic_int_status_3(void);
extern kal_bool pmic_vsdio_oc_int_status(void);
extern kal_bool pmic_vgp_oc_int_status(void);
extern kal_bool pmic_vusb_oc_int_status(void);
extern kal_bool pmic_ovp_int_status(void);
extern kal_bool pmic_chrdet_int_status(void);


/* (0x0E) INT STATUS 4 (RO) */
extern kal_uint8 pmic_int_status_4(void);
extern kal_bool pmic_watchdog_int_status(void);
extern void pmic_watchdog_clear(void);

/* (0x1B) LDO CTRL 2 VRF */
extern void pmic_vrf_ical_en(vrf_ical_en_enum sel);
extern void pmic_vrf_oc_auto_off(kal_bool auto_off);
extern void pmic_vrf_enable(kal_bool enable);
extern void pmic_vrf_cal(kal_uint8 val);

/* (0x1C) LDO CTRL 3 VRF */
extern void pmic_vrf_calst(vrf_calst_enum sel);
extern void pmic_vrf_caloc(vrf_caloc_enum sel);
extern void pmic_vrf_on_sel(vrf_on_sel_enum sel);
extern void pmic_vrf_en_force(kal_bool enable);
extern void pmic_vrf_plnmos_dis(kal_bool disable);
extern void pmic_vrf_cm(vrf_cm_enum sel);

/* (0x1E) LDO CTRL 5 VTCXO */
extern void pmic_vtcxo_ical_en(vtcxo_ical_en_enum sel);
extern void pmic_vtcxo_oc_auto_off(kal_bool auto_off);
extern void pmic_vtcxo_enable(kal_bool enable);
extern void pmic_vtcxo_cal(kal_uint8 val);

/* (0x1F) LDO CTRL 6 VTCXO */
extern void pmic_vtcxo_calst(vtcxo_calst_enum sel);
extern void pmic_vtcxo_caloc(vtcxo_caloc_enum sel);
extern void pmic_vtcxo_on_sel(vtcxo_on_sel_enum sel);
extern void pmic_vtcxo_en_force(kal_bool enable);
extern void pmic_vtcxo_plnmos_dis(kal_bool disable);
extern void pmic_vtcxo_cm(vtcxo_cm_enum sel);

/* (0x21) LDO CTRL 8 V3GTX */
extern void pmic_v3gtx_sel(v3gtx_vol vol);
extern void pmic_v3gtx_ical_en(v3gtx_ical_en_enum sel);
extern void pmic_v3gtx_cal(kal_uint8 val);

/* (0x22) LDO CTRL 9 V3GTX */
extern void pmic_v3gtx_calst(v3gtx_calst_enum sel);
extern void pmic_v3gtx_caloc(v3gtx_caloc_enum sel);
extern void pmic_v3gtx_oc_auto_off(kal_bool auto_off);
extern void pmic_v3gtx_enable(kal_bool enable);
extern void pmic_v3gtx_on_sel(v3gtx_on_sel_enum sel);
extern void pmic_v3gtx_en_force(kal_bool enable);

/* (0x24) LDO CTRL 11 V3GRX */
extern void pmic_v3grx_sel(v3grx_vol vol);
extern void pmic_3grx_ical_en(v3grx_ical_en_enum sel);
extern void pmic_v3grx_cal(kal_uint8 val);

/* (0x25) LDO CTRL 12 V3GRX */
extern void pmic_v3grx_calst(v3grx_calst_enum sel);
extern void pmic_v3grx_caloc(v3grx_caloc_enum sel);
extern void pmic_v3grx_oc_auto_off(kal_bool auto_off);
extern void pmic_v3grx_enable(kal_bool enable);
extern void pmic_v3grx_on_sel(v3grx_on_sel_enum sel);
extern void pmic_v3grx_en_force(kal_bool enable);

/* (0x2E) LDO CTRL 21 VCAMA */
extern void pmic_vcama_sel(vcama_sel_enum sel);
extern void pmic_vcama_ical_en(vcama_ical_en_enum sel);
extern void pmic_vcama_cal(kal_uint8 val);

/* (0x2F) LDO CTRL 22 VCAMA */
extern void pmic_vcama_calst(vcama_calst_enum sel);
extern void pmic_vcama_caloc(vcama_caloc_enum sel);
extern void pmic_vcama_enable(kal_bool enable);
extern void pmic_vcama_en_force(kal_bool enable);
extern void pmic_vcama_plnmos_dis(kal_bool disable);
extern void pmic_vcama_cm(vcama_cm_enum sel);

/* (0x31) LDO CTRL 24 VWIFI3V3 */
extern void pmic_vwifi3v3_sel(vwifi3v3_sel_enum sel);
extern void pmic_vwifi3v3_ical_en(vwifi3v3_ical_en_enum sel);
extern void pmic_vwifi3v3_cal(kal_uint8 val);

/* (0x32) LDO CTRL 25 VWIFI3V3 */
extern void pmic_vwifi3v3_calst(vwifi3v3_calst_enum sel);
extern void pmic_vwifi3v3_caloc(vwifi3v3_caloc_enum sel);
extern void pmic_vwifi3v3_enable(kal_bool enable);
extern void pmic_vwifi3v3_en_force(kal_bool enable);
extern void pmic_vwifi3v3_plnmos_dis(kal_bool disable);
extern void pmic_vwifi3v3_cm(vwifi3v3_cm_enum sel);

/* (0x34) LDO CTRL 27 VWIFI2V8 */
extern void pmic_vwifi2v8_sel(vwifi2v8_sel_enum sel);
extern void pmic_vwifi2v8_ical_en(vwifi2v8_ical_en_enum sel);
extern void pmic_vwifi2v8_cal(kal_uint8 val);

/* (0x35) LDO CTRL 28 VWIFI2V8 */
extern void pmic_vwifi2v8_calst(vwifi2v8_calst_enum sel);
extern void pmic_vwifi2v8_caloc(vwifi2v8_caloc_enum sel);
extern void pmic_vwifi2v8_enable(kal_bool enable);
extern void pmic_vwifi2v8_en_force(kal_bool enable);
extern void pmic_vwifi2v8_plnmos_dis(kal_bool disable);
extern void pmic_vwifi2v8_cm(vwifi2v8_cm_enum sel);

/* (0x37) LDO CTRL 30 VSIM */
/* void pmic_vsim_sel(vsim_sel_enum sel); */
extern void pmic6326_vsim_sel(vsim_sel_enum sel);
extern void pmic6326_vsim_sel(vsim_sel_enum sel);
extern void pmic_vsim_enable(kal_bool enable);
extern void pmic_vsim_ical_en(vsim_ical_en_enum sel);
extern void pmic_vsim_en_force(kal_bool enable);
extern void pmic_vsim_plnmos_dis(kal_bool disable);

/* (0x38) LDO CTRL 31 VSIM */
extern void pmic_vsim_cal(kal_uint8 val);

/* (0x3A) LDO CTRL 33 VUSB */
/* USB voltage is NOT opened for change */
/* extern void pmic_vusb_sel(vusb_sel_enum sel); */
extern void pmic_vusb_enable(kal_bool enable);
extern void pmic_vusb_ical_en(vusb_ical_en_enum sel);
extern void pmic_vusb_en_force(kal_bool enable);
extern void pmic_vusb_plnmos_dis(kal_bool disable);

/* (0x3B) LDO CTRL 34 VUSB */
extern void pmic_vusb_cal(kal_uint8 val);
extern void pmic_vusb_calst(vusb_calst_enum sel);
extern void pmic_vusb_caloc(vusb_caloc_enum sel);

/* (0x3D) LDO CTRL 36 VBT */
extern void pmic_vbt_sel(vbt_sel_enum sel);
extern void pmic_vbt_enable(kal_bool enable);
extern void pmic_vbt_ical_en(vbt_ical_en_enum sel);
extern void pmic_vbt_en_force(kal_bool enable);
extern void pmic_vbt_plnmos_dis(kal_bool disable);

/* (0x3E) LDO CTRL 37 VBT */
extern void pmic_vbt_cal(kal_uint8 val);
extern void pmic_vbt_calst(vbt_calst_enum sel);
extern void pmic_vbt_caloc(vbt_caloc_enum sel);

/* (0x40) LDO CTRL 39 VCAMD */
extern void pmic_vcamd_sel(vcamd_sel_enum sel);
extern void pmic_vcamd_enable(kal_bool enable);
extern void pmic_vcamd_ical_en(vcamd_ical_en_enum sel);
extern void pmic_vcamd_en_force(kal_bool enable);
extern void pmic_vcamd_plnmos_dis(kal_bool disable);

/* (0x41) LDO CTRL 40 VCAMD */
extern void pmic_vcamd_cal(kal_uint8 val);
extern void pmic_vcamd_calst(vcamd_calst_enum sel);
extern void pmic_vcamd_caloc(vcamd_caloc_enum sel);

/* (0x43) LDO CTRL 42 VGP */
extern void pmic_vgp_sel(vgp_sel_enum sel);
extern void pmic_vgp_enable(kal_bool enable);

/* (0x44) LDO CTRL 43 VGP */
extern void pmic_vgp_cal(kal_uint8 val);

/* (0x46) LDO CTRL 45 VSDIO */
extern void pmic_vsdio_ical_en(vsdio_ical_en_enum sel);
extern void pmic_vsdio_enable(kal_bool enable);
extern void pmic_vsdio_en_force(kal_bool enable);
extern void pmic_vsdio_cal(kal_uint8 val);

/* (0x47) LDO CTRL 46 VSDIO */
extern void pmic_vsdio_calst(vsdio_calst_enum sel);
extern void pmic_vsdio_caloc(vsdio_caloc_enum sel);
extern void pmic_vsdio_plnmos_dis(kal_bool disable);
extern void pmic_vsdio_sel(vsdio_sel_enum sel);
extern void pmic_vsdio_cm(vsdio_cm_enum sel);

/* (0x48) LDO CTRL 47 VSDIO */
extern void pmic_vcore1_dvfs_step_inc(kal_uint8 val);

/* (0x4E) BUCK CTRL 6 VCORE1 */
extern void pmic_vcore1_dvfs_0_eco3(kal_uint8 val);

/* (0x4F) BUCK CTRL 7 VCORE1 */
extern void pmic_vcore1_sleep_0_eco3(kal_uint8 val);
extern void pmic_vcore1_dvfs_ramp_enable(kal_bool enable);
extern void pmic_vcore1_dvfs_target_update(kal_bool update);

/* (0x51) BUCK CTRL 9 VCORE2 */
extern void pmic_vcore2_dvfs_0_eco3(kal_uint8 val);


/* (0x52) BUCK CTRL 10 VCORE2 */
extern void pmic_vcore2_enable(kal_bool enable);
extern void pmic_vcore2_sleep_0_eco3(kal_uint8 val);

/* (0x53) BUCK CTRL 11 VCORE2 */
extern void pmic_vcore2_on_sel(vcore2_on_sel_enum sel);

/* (0x57) BUCK CTRL 15 VMEM */
extern void pmic_vcore1_sleep_1_eco3(kal_uint8 val);
extern void pmic_vcore1_dvfs_1_eco3(kal_uint8 val);

/* (0x58) BUCK CTRL 16 VPA */
extern void pmic_vpa_tuneh(kal_uint8 value);
extern void pmic_vpa_en_force(kal_bool enable);
extern void pmic_vpa_plnmos_dis(kal_bool disable);
extern void pmic_vpa_enable(kal_bool enable);

/* (0x59) BUCK CTRL 17 VPA */
extern void pmic_vpa_tunel(kal_uint8 value);

/* (0x5A) BUCK CTRL 18 VPA */
extern void pmic_vpa_oc_tune(kal_uint8 val);

/* (0x5C) BOOST CTRL 1 BOOST1 */
extern void pmic_vboost1_tune(vboost1_tune_enum sel);
extern void pmic_vboost1_tatt(kal_uint8 val);

/* (0x5D) BOOST CTRL 2 BOOST1 */
extern void pmic_boost1_oc_th(kal_uint8 val);
extern void pmic_boost1_enable(kal_bool enable);
extern void pmic_boost1_pre_sr_con(kal_uint8 val);
extern void pmic_boost1_soft_st_speed(boost1_soft_st_speed_enum sel);

/* (0x5E) BOOST CTRL 3 BOOST1 */
extern void pmic_boost1_dio_sr_con(kal_uint8 val);
extern void pmic_boost1_sync_enable(kal_bool enable);


/* (0x5F) BOOST CTRL 4 BOOST2 */
extern void pmic_boost2_tune(vboost2_tune_enum sel);
extern void pmic_boots2_oc_th(boost2_oc_th_enum sel);
extern void pmic_boost2_dim_source(boost2_dim_source_enum sel);

/* (0x60) BOOST CTRL 5 BOOST2 */
extern void pmic_boost2_pre_sr_con(kal_uint8 val);
extern void pmic_boost2_enable(kal_bool enable);

/* (0x61) BOOST CTRL 6 BOOST2 and BOOST */
extern void pmic_boost_mode(boost_mode_sel_enum sel);

/* (0x64) DRIVER CTRL 3 GEN */
extern void pmic_igen_drv_isel(kal_uint8 sel);
extern void pmic_igen_drv_force(kal_bool force);
extern void pmic_vgen_drv_bgsel(kal_uint8 sel);

/* (0x65) DRIVER CTRL 4 FLASH */
extern void pmic_flash_i_tune(kal_uint8 val);
extern void pmic_flash_dim_div(kal_uint8 val);

/* (0x66) DRIVER CTRL 5 FLASH */
extern void pmic_flash_dim_duty(kal_uint8 duty);
extern void pmic_flash_enable(kal_bool enable);
extern void pmic_flash_bypass(kal_bool bypass);

/* (0x67) DRIVER CTRL 6 BL */
extern void pmic_bl_dim_duty(kal_uint8 duty);
extern void pmic_bl_enable(kal_bool enable);
extern void pmic_bl_i_cal_enable(kal_bool enable);
extern void pmic_bl_bypass(kal_bool bypass);

/* (0x68) DRIVER CTRL 7 BL */
extern void pmic_bl_i_corse_tune(bl_i_corse_tune_enum sel);
extern void pmic_bl_i_fine_tune(bl_i_fine_tune_enum sel);

/* (0x6D) DRIVER CTRL 12 BL */
extern void pmic_bl_dim_div(kal_uint8 val);
extern void pmic_bl_number(bl_number_enum num);

/* (0x6E) DRIVER CTRL 13 KP */
extern void pmic_kp_dim_div(kal_uint8 val);
extern void pmic_kp_enable(kal_bool enable);

/* (0x6F) DRIVER CTRL 14 KP */
extern void pmic_kp_dim_duty(kal_uint8 duty);

/* (0x70) DRIVER CTRL 15 VIBR */
extern void pmic_vibr_dim_div(kal_uint8 val);
extern void pmic_vibr_enable(kal_bool enable);

/* (0x71) DRIVER CTRL 16 VIBR */
extern void pmic_vibr_dim_duty(kal_uint8 duty);

/* (0x73) CLASS_D CTRL 3 SPKL */
extern void pmic_spkl_dtin(kal_uint8 val);
extern void pmic_spkl_dtip(kal_uint8 val);

/* (0x74) CLASS_D CTRL 4 SPKL */
extern void pmic_spkl_dmode(spkl_dmode_enum sel);
extern void pmic_spkl_enable(kal_bool enable);
extern void pmic_spkl_dtcal(spkl_dtcal_enum sel);

/* (0x78) CLASS_D CTRL 8 SPKR */
extern void pmic_spkr_dtin(kal_uint8 val);
extern void pmic_spkr_dtip(kal_uint8 val);

/* (0x79) CLASS_D CTRL 9 SPKR */
extern void pmic_spkr_dmode(spkr_dmode_enum sel);
extern void pmic_spkr_enable(kal_bool enable);
extern void pmic_spkr_dtcal(spkr_dtcal_enum sel);


/* (0x81) CHARGER CTRL 1 */
extern void pmic_chr_offset(cht_chr_offset_enum sel);
extern void pmic_chr_ov_th_high(void);
extern void pmic_chr_current(chr_chr_current_enum current);

/* (0x82) CHARGER CTRL 2 */
extern void pmic_chr_cv_rt(void);
extern void pmic_chr_force(kal_bool force);
extern void pmic_chr_chr_enable(kal_bool enable);
extern void pmic_chr_cv_tune(void);
/* (0x83) TESTMODE CTRL 3 Analog Switch */
extern void pmic_asw_asel(asw_asel_enum sel);
extern void pmic_asw_bsel(asw_bsel_enum sel);
extern void pmic_asw_a1sel(kal_uint8 sel);
extern void pmic_asw_a2sel(kal_uint8 sel);
/* (0x86) TESTMODE CTRL 6 BB AUXADC Related */
extern void pmic_adc_isense_enable(kal_bool enable);
extern void pmic_adc_vbat_enable(kal_bool enable);
extern void pmic6326_adc_meas_on(kal_bool on);	/* exported for controling vbat, isense adc measure at same time */

/* (0x89) INT CTRL 1 */
extern void pmic_int_ctrl_1_enable(int_ctrl_1_enum sel, kal_bool enable);
/* (0x8A) INT CTRL 2 */
extern void pmic_int_ctrl_2_enable(int_ctrl_2_enum sel, kal_bool enable);
/* (0x8B) INT CTRL 2 */
extern void pmic_int_ctrl_3_enable(int_ctrl_3_enum sel, kal_bool enable);

/* (0x96) WATCHDOG CTRL and INT CTRL 4 */
extern void pmic_wdt_timeout(wdt_timout_enum sel);
extern void pmic_intr_polarity(kal_bool assert);
extern void pmic_wdt_enable(kal_bool enable);


/* Combinational functions */
extern void pmic_vgp2_enable(kal_bool enable);
extern void pmic_vgp2_sel(vgp2_sel_enum sel);
extern void pmic_vgp2_on_sel(vgp2_on_sel_enum sel);
extern void pmic_vgp2_sell(kal_uint8 value);
extern void pmic_vgp2_selh(kal_uint8 value);
extern void pmic_vsim2_enable(kal_bool enable);
extern void pmic_vsim2_sel(vsim_sel_enum sel);
extern void pmic_spk_enable(kal_bool enable);

extern void pmic6326_EM_reg_write(kal_uint8 reg, kal_uint8 val);
extern kal_uint8 pmic6326_EM_reg_read(kal_uint8 reg);

#if defined(DRV_MISC_PMIC_ASSERT_KEEP_CHARGING)
extern void pmic6326_assert_chaging_kick(void);
#endif				/* #if defined(DRV_MISC_PMIC_ASSERT_KEEP_CHARGING) */

/* The following are implemented in custom files */
/* MoDIS parser skip start */
extern void pmic6326_customization_init(void);
extern void pmic6326_cust_vspk_enable(kal_bool enable);
extern void pmic6326_csut_vsim_enable(kal_bool enable);
extern void pmic6326_csut_vsim_sel(pmic_adpt_vsim_volt volt);
extern void pmic6326_csut_vsim2_enable(kal_bool enable);
extern void pmic6326_csut_vsim2_sel(pmic_adpt_vsim_volt sel);
extern void pmic6326_csut_vusb_enable(kal_bool enable);
extern void pmic6326_csut_vcama_enable(kal_bool enable);
extern void pmic6326_csut_vcama_sel(pmic_adpt_vcama_volt vol);
extern void pmic6326_csut_vcamd_enable(kal_bool enable);
extern void pmic6326_csut_vcamd_sel(pmic_adpt_vcamd_volt volt);
/* MoDIS parser skip end */

#endif

/* ======================================================================================= */

#endif				/* #ifndef __PMIC6326_SW_H__ */
