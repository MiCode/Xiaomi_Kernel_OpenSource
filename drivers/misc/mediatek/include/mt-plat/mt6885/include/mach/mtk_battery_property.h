/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MTK_BATTERY_PROPERTY_H
#define _MTK_BATTERY_PROPERTY_H

/* customize */
#define DIFFERENCE_FULLOCV_ITH	250	/* mA */
#define MTK_CHR_EXIST 1
#define KEEP_100_PERCENT 1
#define R_FG_VALUE	5				/* mOhm */
#define EMBEDDED_SEL 1
#define PMIC_SHUTDOWN_CURRENT 20	/* 0.01 mA */
#define FG_METER_RESISTANCE	100
#define CAR_TUNE_VALUE	100 /*1.00 */
#define NO_BAT_TEMP_COMPENSATE 0
/* NO_BAT_TEMP_COMPENSATE 1 = don't need bat_temper compensate, */
/* but fg_meter_resistance still use for SWOCV */

/* enable that soc = 0 , shutdown */
#define SHUTDOWN_GAUGE0 1

/* enable that uisoc = 1 and wait xmins then shutdown */
#define SHUTDOWN_GAUGE1_XMINS 1
/* define Xmins to shutdown*/
#define SHUTDOWN_1_TIME	5

#define SHUTDOWN_GAUGE1_VBAT_EN 0
#define SHUTDOWN_GAUGE1_VBAT 34000

#define SHUTDOWN_GAUGE0_VOLTAGE 34000

#define POWERON_SYSTEM_IBOOT 500	/* mA */

/*
 * LOW_TEMP_MODE = 0
 *	disable LOW_TEMP_MODE
 * LOW_TEMP_MODE = 1
 *	if battery temperautre < LOW_TEMP_MODE_TEMP
 *	when bootup , force C mode
 * LOW_TEMP_MODE = 2
 *	if battery temperautre < LOW_TEMP_MODE_TEMP
 *	force C mode
 */
#define LOW_TEMP_MODE 0
#define LOW_TEMP_MODE_TEMP 0


#define D0_SEL 0	/* not implement */
#define AGING_SEL 0	/* not implement */
#define DLPT_UI_REMAP_EN 0

/* ADC resistor  */
#define R_BAT_SENSE	4
#define R_I_SENSE	4
#define R_CHARGER_1	330
#define R_CHARGER_2	39


#define QMAX_SEL 1
#define IBOOT_SEL 0
#define SHUTDOWN_SYSTEM_IBOOT 15000	/* 0.1mA */
#define PMIC_MIN_VOL 33500

/*ui_soc related */
#define DIFFERENCE_FULL_CV 1000 /*0.01%*/
#define PSEUDO1_EN 1
#define PSEUDO100_EN 1
#define PSEUDO100_EN_DIS 0

#define DIFF_SOC_SETTING 50	/* 0.01% */
#define DIFF_BAT_TEMP_SETTING 1
#define DIFF_BAT_TEMP_SETTING_C 10
#define DISCHARGE_TRACKING_TIME 10
#define CHARGE_TRACKING_TIME 60
#define DIFFERENCE_FULLOCV_VTH	1000	/* 0.1mV */
#define CHARGE_PSEUDO_FULL_LEVEL 8000
#define FULL_TRACKING_BAT_INT2_MULTIPLY 6

/* pre tracking */
#define FG_PRE_TRACKING_EN 1
#define VBAT2_DET_TIME 5
#define VBAT2_DET_COUNTER 6
#define VBAT2_DET_VOLTAGE1	34000
#define VBAT2_DET_VOLTAGE2	33500
#define VBAT2_DET_VOLTAGE3	34500

/* PCB setting */
#define CALIBRATE_CAR_TUNE_VALUE_BY_META_TOOL
#define CALI_CAR_TUNE_AVG_NUM	60

/* Aging Compensation 1*/
#define AGING_FACTOR_MIN 90
#define AGING_FACTOR_DIFF 10
#define DIFFERENCE_VOLTAGE_UPDATE 50
#define AGING_ONE_EN 1
#define AGING1_UPDATE_SOC 30
#define AGING1_LOAD_SOC 70
#define AGING_TEMP_DIFF 10
#define AGING_TEMP_LOW_LIMIT 15
#define AGING_TEMP_HIGH_LIMIT 50
#define AGING_100_EN 1

/* Aging Compensation 2*/
#define AGING_TWO_EN 1

/* Aging Compensation 3*/
#define AGING_THIRD_EN 1
#define AGING_4_EN 1
#define AGING_5_EN 1
#define AGING_6_EN 1

#define AGING4_UPDATE_SOC 40
#define AGING4_LOAD_SOC 70

#define AGING5_UPDATE_SOC 30
#define AGING5_LOAD_SOC 70

#define AGING6_UPDATE_SOC 30
#define AGING6_LOAD_SOC 70

/* threshold */
#define HWOCV_SWOCV_DIFF	300
#define HWOCV_SWOCV_DIFF_LT	1500
#define HWOCV_SWOCV_DIFF_LT_TEMP	5
#define HWOCV_OLDOCV_DIFF	300
#define HWOCV_OLDOCV_DIFF_CHR	800
#define SWOCV_OLDOCV_DIFF	300
#define SWOCV_OLDOCV_DIFF_CHR	800
#define VBAT_OLDOCV_DIFF	1000
#define SWOCV_OLDOCV_DIFF_EMB	1000	/* 100mV */

#define VIR_OLDOCV_DIFF_EMB	10000	/* 1000mV */
#define VIR_OLDOCV_DIFF_EMB_LT	10000	/* 1000mV */
#define VIR_OLDOCV_DIFF_EMB_TMP	5

#define TNEW_TOLD_PON_DIFF	5
#define TNEW_TOLD_PON_DIFF2	15
#define PMIC_SHUTDOWN_TIME	30
#define BAT_PLUG_OUT_TIME	32
#define EXT_HWOCV_SWOCV		300
#define EXT_HWOCV_SWOCV_LT		1500
#define EXT_HWOCV_SWOCV_LT_TEMP		5

/* fgc & fgv threshold */
#define DIFFERENCE_FGC_FGV_TH1 300
#define DIFFERENCE_FGC_FGV_TH2 500
#define DIFFERENCE_FGC_FGV_TH3 300
#define DIFFERENCE_FGC_FGV_TH_SOC1 7000
#define DIFFERENCE_FGC_FGV_TH_SOC2 3000
#define NAFG_TIME_SETTING 10
#define NAFG_RATIO 100
#define NAFG_RATIO_EN 0
#define NAFG_RATIO_TMP_THR 1
#define NAFG_RESISTANCE 1500

#define PMIC_SHUTDOWN_SW_EN 1
#define FORCE_VC_MODE 0	/* 0: mix, 1:Coulomb, 2:voltage */

#define LOADING_1_EN 0
#define LOADING_2_EN 2
#define DIFF_IAVG_TH 3000

/* ZCV INTR */
#define ZCV_SUSPEND_TIME 3
#define SLEEP_CURRENT_AVG 200 /*0.1mA*/
#define ZCV_CAR_GAP_PERCENTAGE 1

/* Additional battery table */
#define ADDITIONAL_BATTERY_TABLE_EN 1

#define DC_RATIO_SEL	5
#define DC_R_CNT	1000	/* if set 0, dcr_start will not be 1*/

#define BAT_PAR_I 4000	/* not implement */

#define PSEUDO1_SEL	2

#define FG_TRACKING_CURRENT	30000	/* not implement */
#define FG_TRACKING_CURRENT_IBOOT_EN	0	/* not implement */
#define UI_FAST_TRACKING_EN 0
#define UI_FAST_TRACKING_GAP 300
#define KEEP_100_PERCENT_MINSOC 9000


#define SHUTDOWN_CONDITION_LOW_BAT_VOLT
#define LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN 1
#define LOW_TEMP_THRESHOLD 5

#define BATTERY_TMP_TO_DISABLE_GM30 -50
#define BATTERY_TMP_TO_DISABLE_NAFG -35
#define DEFAULT_BATTERY_TMP_WHEN_DISABLE_NAFG 25
#define BATTERY_TMP_TO_ENABLE_NAFG -20
/* #define GM30_DISABLE_NAFG */

#define POWER_ON_CAR_CHR	5
#define POWER_ON_CAR_NOCHR	-35

#define SHUTDOWN_CAR_RATIO	1


#define MULTI_TEMP_GAUGE0 1	/* different temp using different gauge 0% */

#define OVER_DISCHARGE_LEVEL -1500

#define UISOC_UPDATE_TYPE 0
/*
 *	uisoc_update_type:
 *	0: only ui_soc interrupt update ui_soc
 *	1: coulomb/nafg will update ui_soc if delta car > ht/lt_gap /2
 *	2: coulomb/nafg will update ui_soc
 */

/* using current to limit uisoc in 100% case*/
/* UI_FULL_LIMIT_ITH0 3000 means 300ma */
#define UI_FULL_LIMIT_EN 0
#define UI_FULL_LIMIT_SOC0 9900
#define UI_FULL_LIMIT_ITH0 2200

#define UI_FULL_LIMIT_SOC1 9900
#define UI_FULL_LIMIT_ITH1 2200

#define UI_FULL_LIMIT_SOC2 9900
#define UI_FULL_LIMIT_ITH2 2200

#define UI_FULL_LIMIT_SOC3 9900
#define UI_FULL_LIMIT_ITH3 2200

#define UI_FULL_LIMIT_SOC4 9900
#define UI_FULL_LIMIT_ITH4 2200

#define UI_FULL_LIMIT_TIME 99999


/* using voltage to limit uisoc in 1% case */
/* UI_LOW_LIMIT_VTH0=36000 means 3.6v */
#define UI_LOW_LIMIT_EN 1

#define UI_LOW_LIMIT_SOC0 200
#define UI_LOW_LIMIT_VTH0 34500

#define UI_LOW_LIMIT_SOC1 200
#define UI_LOW_LIMIT_VTH1 34500

#define UI_LOW_LIMIT_SOC2 200
#define UI_LOW_LIMIT_VTH2 34500

#define UI_LOW_LIMIT_SOC3 200
#define UI_LOW_LIMIT_VTH3 34500

#define UI_LOW_LIMIT_SOC4 200
#define UI_LOW_LIMIT_VTH4 34500

#define UI_LOW_LIMIT_TIME 99999

#define MOVING_BATTEMP_EN 1
#define MOVING_BATTEMP_THR 20


#endif
