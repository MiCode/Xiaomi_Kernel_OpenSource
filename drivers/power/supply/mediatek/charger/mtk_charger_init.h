/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_CHARGER_INIT_H__
#define __MTK_CHARGER_INIT_H__

#define BATTERY_CV 4420000
#define V_CHARGER_MAX 12800000 /*12.8 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3250000
#define NON_STD_AC_CHARGER_CURRENT		1000000
#define CHARGING_HOST_CHARGER_CURRENT		650000
#define APPLE_1_0A_CHARGER_CURRENT		650000
#define APPLE_2_1A_CHARGER_CURRENT		800000
#define TA_AC_CHARGING_CURRENT	3000000

/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1400000 /* 1.4 A */

/* sw jeita */
#define JEITA_TEMP_ABOVE_T6_CV	4100000
#define JEITA_TEMP_T5_TO_T6_CV	4100000
#define JEITA_TEMP_T4_TO_T5_CV	4480000
#define JEITA_TEMP_T3_TO_T4_CV	4480000
#define JEITA_TEMP_T2_TO_T3_CV	4450000
#define JEITA_TEMP_T1_TO_T2_CV	4450000
#define JEITA_TEMP_T0_TO_T1_CV	4450000
#define JEITA_TEMP_BELOW_T0_CV	4450000

/* 60 */
#define TEMP_T6_THRES  60
#define TEMP_T6_THRES_MINUS_X_DEGREE 59
/* 48 */
#define TEMP_T5_THRES  48
#define TEMP_T5_THRES_MINUS_X_DEGREE 47
/* 35 */
#define TEMP_T4_THRES  35
#define TEMP_T4_THRES_MINUS_X_DEGREE 34
/* 15 */
#define TEMP_T3_THRES  15
#define TEMP_T3_THRES_MINUS_X_DEGREE 14
/* 10 */
#define TEMP_T2_THRES  10
#define TEMP_T2_THRES_PLUS_X_DEGREE 9
/* 5 */
#define TEMP_T1_THRES  5
#define TEMP_T1_THRES_PLUS_X_DEGREE 4
/* 0 */
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  -1
/* -10 */
#define TEMP_NEG_10_THRES  -10
/* 67W charger*/
/* above 60  stop charging*/
#define JEITA_TEMP_ABOVE_T6_CC	0
/* 48 ~ 60  main charger*/
#define JEITA_TEMP_T5_TO_T6_CC	2350000
/* 35 ~ 48  double charger pump*/
#define JEITA_TEMP_T4_TO_T5_CC	3250000
/* 15 ~ 35  double charger pump*/
#define JEITA_TEMP_T3_TO_T4_CC	3250000
/* 10 ~ 15 single charger pump*/
#define JEITA_TEMP_T2_TO_T3_CC	5880000
/* 5 ~ 10 main charger */
#define JEITA_TEMP_T1_TO_T2_CC	3430000
/* 0 ~ 5 main charger */
#define JEITA_TEMP_T0_TO_T1_CC	2400000
/* -10 ~ 0 main charger */
#define JEITA_TEMP_BELOW_T0_CC	735000

#define TEMP_LCD_OFF_T7  60
#define TEMP_LCD_OFF_T6  49
#define TEMP_LCD_OFF_T5  47
#define TEMP_LCD_OFF_T4  45
#define TEMP_LCD_OFF_T3  43
#define TEMP_LCD_OFF_T2  10
#define TEMP_LCD_OFF_T1  5
#define TEMP_LCD_OFF_T0  0
#define TEMP_LCD_OFF_NEG_10  -10

#define TEMP_LCD_ON_T9  60
#define TEMP_LCD_ON_T8  47
#define TEMP_LCD_ON_T7  45
#define TEMP_LCD_ON_T6  44
#define TEMP_LCD_ON_T5  41
#define TEMP_LCD_ON_T4  39
#define TEMP_LCD_ON_T3  37
#define TEMP_LCD_ON_T2  10
#define TEMP_LCD_ON_T1  5
#define TEMP_LCD_ON_T0  0
#define TEMP_LCD_ON_NEG_10  -10
#define OFFSET 1

#define CURR_LCD_OFF_T6_TO_T7  1000000
#define CURR_LCD_OFF_T5_TO_T6  1500000
#define CURR_LCD_OFF_T4_TO_T5  2000000
#define CURR_LCD_OFF_T3_TO_T4  2500000
#define CURR_LCD_OFF_T2_TO_T3  3000000
#define CURR_LCD_OFF_T1_TO_T2  2450000
#define CURR_LCD_OFF_T0_TO_T1  890000
#define CURR_LCD_OFF_NEG_10_TO_T0  490000

#define CURR_LCD_ON_T8_TO_T9  700000
#define CURR_LCD_ON_T7_TO_T8  1000000
#define CURR_LCD_ON_T6_TO_T7  1500000
#define CURR_LCD_ON_T5_TO_T6  2000000
#define CURR_LCD_ON_T4_TO_T5  2500000
#define CURR_LCD_ON_T3_TO_T4  2800000
#define CURR_LCD_ON_T2_TO_T3  3000000
#define CURR_LCD_ON_T1_TO_T2  2450000
#define CURR_LCD_ON_T0_TO_T1  890000
#define CURR_LCD_ON_NEG_10_TO_T0  490000
/* -Extb HONGMI-85045,ADD,wangbin.wt.20210623.add sw jeita*/

/* Battery Temperature Protection */
#define MIN_CHARGE_TEMP  0
#define MIN_CHARGE_TEMP_PLUS_X_DEGREE	6
#define MAX_CHARGE_TEMP  50
#define MAX_CHARGE_TEMP_MINUS_X_DEGREE	47

/* pe */
#define PE_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define TA_AC_12V_INPUT_CURRENT 3200000
#define TA_AC_9V_INPUT_CURRENT	3200000
#define TA_AC_7V_INPUT_CURRENT	3200000
#define TA_9V_SUPPORT
#define TA_12V_SUPPORT

/* pe2.0 */
#define PE20_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define TA_START_BATTERY_SOC	0
#define TA_STOP_BATTERY_SOC	85

/* dual charger */
#define TA_AC_MASTER_CHARGING_CURRENT 1500000
#define TA_AC_SLAVE_CHARGING_CURRENT 1500000
#define SLAVE_MIVR_DIFF 100000

/* slave charger */
#define CHG2_EFF 90

/* cable measurement impedance */
#define CABLE_IMP_THRESHOLD 699
#define VBAT_CABLE_IMP_THRESHOLD 3900000 /* uV */

/* bif */
#define BIF_THRESHOLD1 4250000	/* UV */
#define BIF_THRESHOLD2 4300000	/* UV */
#define BIF_CV_UNDER_THRESHOLD2 4450000	/* UV */
#define BIF_CV BATTERY_CV /* UV */

#define R_SENSE 56 /* mohm */

#define MAX_CHARGING_TIME (12 * 60 * 60) /* 12 hours */

#define DEFAULT_BC12_CHARGER 0 /* MAIN_CHARGER */

/* battery warning */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP

/* pe4 */
#define PE40_MAX_VBUS 11000
#define PE40_MAX_IBUS 3000
#define HIGH_TEMP_TO_LEAVE_PE40 46
#define HIGH_TEMP_TO_ENTER_PE40 39
#define LOW_TEMP_TO_LEAVE_PE40 10
#define LOW_TEMP_TO_ENTER_PE40 16

/* pd */
#define PD_VBUS_UPPER_BOUND 10000000	/* uv */
#define PD_VBUS_LOW_BOUND 5000000	/* uv */
#define PD_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define PD_STOP_BATTERY_SOC 80

#define VSYS_WATT 5000000
#define IBUS_ERR 14

#endif /*__MTK_CHARGER_INIT_H__*/
