/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/of.h>

#include <asm/uaccess.h>
#include <mt_boot.h>

#include "mt_battery_custom_data.h"
#include "mt_battery_common.h"
#include "mt_battery_meter.h"
#include "mt_battery_meter_hal.h"
/* ============================================================ // */
/* define */
/* ============================================================ // */
static DEFINE_MUTEX(FGADC_mutex);
static DEFINE_MUTEX(qmax_mutex);

/*static s32 g_currentfactor = 100;*/


#define BM_LOG_ERROR 0
#define BM_LOG_CRTI 1
#define BM_LOG_FULL 2

int Enable_FGADC_LOG = BM_LOG_CRTI;

#define bm_print(num, fmt, args...)   \
do {									\
	if (Enable_FGADC_LOG >= (int)num) {				\
		pr_warn(fmt, ##args);\
		} \
	} while (0)

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
#ifdef CONFIG_OF
static const struct of_device_id mt_battery_meter_id[] = {
	{.compatible = "mediatek,battery_meter"},
	{},
};

MODULE_DEVICE_TABLE(of, mt_battery_meter_id);
#endif

static BATTERY_METER_CONTROL battery_meter_ctrl;

/* static struct proc_dir_entry *proc_entry_fgadc; */
static char proc_fgadc_data[32];

static bool gFG_Is_Charging;
static bool gFG_Is_Init;
static struct mt_battery_meter_custom_data *p_bat_meter_data;

/* Disable Battery check for HQA */
#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
#define FIXED_TBAT_25
#endif

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // PMIC AUXADC Related Variable */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
int g_R_BAT_SENSE = 0;
int g_R_I_SENSE = 0;
int g_R_CHARGER_1 = 0;
int g_R_CHARGER_2 = 0;
int g_R_FG_offset = 0;

/*int fg_qmax_update_for_aging_flag = 1;*/

/* debug purpose */
s32 g_hw_ocv_debug = 0;
s32 g_hw_soc_debug = 0;
s32 g_sw_soc_debug = 0;
s32 g_rtc_soc_debug = 0;

/* HW FG */
s32 gFG_DOD0 = 0;
s32 gFG_DOD1 = 0;
s32 gFG_columb = 0;
s32 gFG_voltage = 0;
s32 gFG_voltage_OCV = 0;
s32 gFG_current = 0;
s32 gFG_capacity = 0;
s32 gFG_capacity_by_c = 0;
s32 gFG_capacity_by_c_init = 0;
s32 gFG_capacity_by_v = 0;
s32 gFG_capacity_by_v_init = 0;
s32 gFG_temp = 100;
s32 gFG_resistance_bat = 0;
s32 gFG_compensate_value = 0;
s32 gFG_ori_voltage = 0;
s32 gFG_BATT_CAPACITY = 0;
s32 gFG_voltage_init = 0;
s32 gFG_current_auto_detect_R_fg_total = 0;
s32 gFG_current_auto_detect_R_fg_count = 0;
s32 gFG_current_auto_detect_R_fg_result = 0;
s32 gFG_15_vlot = 3700;
s32 gFG_BATT_CAPACITY_init_high_current = 1200;
s32 gFG_BATT_CAPACITY_aging = 1200;

s32 g_sw_vbat_temp = 0;
s32 g_hw_ocv_before_sleep = 0;

struct timespec time_before_sleep;

/* voltage mode */
s32 gfg_percent_check_point = 50;
s32 volt_mode_update_timer = 0;
s32 volt_mode_update_time_out = 6;	/* 1mins */

/* EM */
s32 g_fg_dbg_bat_hwocv = 0;
s32 g_fg_dbg_bat_volt = 0;
s32 g_fg_dbg_bat_current = 0;
s32 g_fg_dbg_bat_zcv = 0;
s32 g_fg_dbg_bat_temp = 0;
s32 g_fg_dbg_bat_r = 0;
s32 g_fg_dbg_bat_car = 0;
s32 g_fg_dbg_bat_qmax = 0;
s32 g_fg_dbg_d0 = 0;
s32 g_fg_dbg_d1 = 0;
s32 g_fg_dbg_percentage = 0;
s32 g_fg_dbg_percentage_fg = 0;
s32 g_fg_dbg_percentage_voltmode = 0;

s32 g_update_qmax_flag = 1;
/*s32 FGbatteryIndex = 0;*/
/*s32 FGbatteryVoltageSum = 0;*/
/*s32 gFG_voltage_AVG = 0;*/
s32 gFG_vbat_offset = 0;
#if 0
s32 FGvbatVoltageBuffer[FG_VBAT_AVERAGE_SIZE];
s32 g_tracking_point = CUST_TRACKING_POINT;
#else
s32 g_tracking_point = 0;
/*s32 FGvbatVoltageBuffer[36];*/	/* tmp fix */
#endif

s32 g_rtc_fg_soc = 0;
s32 g_I_SENSE_offset = 0;

/* aging mechanism */
#if defined(CONFIG_MTK_ENABLE_AGING_ALGORITHM) && !defined(CONFIG_POWER_EXT)

static s32 aging_ocv_1;
static s32 aging_ocv_2;
static s32 aging_car_1;
static s32 aging_car_2;
static s32 aging_dod_1;
static s32 aging_dod_2;
static time_t aging_resume_time_1;
static time_t aging_resume_time_2;

#ifndef SELF_DISCHARGE_CHECK_THRESHOLD
#define SELF_DISCHARGE_CHECK_THRESHOLD 3
#endif

#ifndef OCV_RECOVER_TIME
#define OCV_RECOVER_TIME 1800
#endif

#ifndef DOD1_ABOVE_THRESHOLD
#define DOD1_ABOVE_THRESHOLD 30
#endif

#ifndef DOD2_BELOW_THRESHOLD
#define DOD2_BELOW_THRESHOLD 70
#endif

#ifndef MIN_DOD_DIFF_THRESHOLD
#define MIN_DOD_DIFF_THRESHOLD 60
#endif

#ifndef MIN_AGING_FACTOR
#define MIN_AGING_FACTOR 90
#endif

#endif				/* aging mechanism */

/* battery info */
#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT

s32 gFG_battery_cycle = 0;
s32 gFG_aging_factor = 100;
s32 gFG_columb_sum = 0;
s32 gFG_pre_columb_count = 0;

s32 gFG_max_voltage = 0;
s32 gFG_min_voltage = 10000;
s32 gFG_max_current = 0;
s32 gFG_min_current = 0;
s32 gFG_max_temperature = -20;
s32 gFG_min_temperature = 100;

#endif				/* battery info */

/* Temperature window size */
#define TEMP_AVERAGE_SIZE	5

__attribute__ ((weak))
int set_rtc_spare_fg_value(int val)
{
	pr_debug("need rtc porting!\n");
	return 0;
}

__attribute__ ((weak))
int get_rtc_spare_fg_value(void)
{
	pr_debug("need rtc porting!\n");
	return 0;
}

__attribute__ ((weak))
bool mt_usb_is_device(void)
{
	pr_debug("need usb porting!\n");
	return true;
}

/* ============================================================ // */
int get_r_fg_value(void)
{
	return p_bat_meter_data->r_fg_value + g_R_FG_offset;
}

int BattThermistorConverTemp(int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;
	int saddles = p_bat_meter_data->battery_ntc_table_saddles;
	struct BATT_TEMPERATURE *batt_temp_table =
	    (struct BATT_TEMPERATURE *)p_bat_meter_data->p_batt_temperature_table;


	if (Res >= batt_temp_table[0].TemperatureR) {
		TBatt_Value = batt_temp_table[0].BatteryTemp;
	} else if (Res <= batt_temp_table[saddles - 1].TemperatureR) {
		TBatt_Value = batt_temp_table[saddles - 1].BatteryTemp;
	} else {
		RES1 = batt_temp_table[0].TemperatureR;
		TMP1 = batt_temp_table[0].BatteryTemp;

		for (i = 0; i <= saddles - 1; i++) {
			if (Res >= batt_temp_table[i].TemperatureR) {
				RES2 = batt_temp_table[i].TemperatureR;
				TMP2 = batt_temp_table[i].BatteryTemp;
				break;
			}

			RES1 = batt_temp_table[i].TemperatureR;
			TMP1 = batt_temp_table[i].BatteryTemp;
		}

		TBatt_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) / (RES1 - RES2);
	}

	return TBatt_Value;
}

s32 fgauge_get_Q_max(s16 temperature)
{
	s32 ret_Q_max = 0;
	s32 low_temperature = 0, high_temperature = 0;
	s32 low_Q_max = 0, high_Q_max = 0;

	if (temperature <= p_bat_meter_data->tempearture_t1) {
		low_temperature = (-10);
		low_Q_max = p_bat_meter_data->q_max_neg_10;
		high_temperature = p_bat_meter_data->tempearture_t1;
		high_Q_max = p_bat_meter_data->q_max_pos_0;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (p_bat_meter_data->p_battery_profile_t1_5 &&
			temperature <= p_bat_meter_data->temperature_t1_5) {
			low_temperature = p_bat_meter_data->tempearture_t1;
			low_Q_max = p_bat_meter_data->q_max_pos_0;
			high_temperature = p_bat_meter_data->temperature_t1_5;
			high_Q_max = p_bat_meter_data->q_max_pos_10;

			if (temperature < low_temperature)
				temperature = low_temperature;

	} else if (temperature <= p_bat_meter_data->temperature_t2) {

		if (p_bat_meter_data->p_battery_profile_t1_5) {
			low_temperature = p_bat_meter_data->temperature_t1_5;
			low_Q_max = p_bat_meter_data->q_max_pos_10;
		} else {
			low_temperature = p_bat_meter_data->tempearture_t1;
			low_Q_max = p_bat_meter_data->q_max_pos_0;
		}
		high_temperature = p_bat_meter_data->temperature_t2;
		high_Q_max = p_bat_meter_data->q_max_pos_25;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_temperature = p_bat_meter_data->temperature_t2;
		low_Q_max = p_bat_meter_data->q_max_pos_25;
		high_temperature = p_bat_meter_data->temperature_t3;
		high_Q_max = p_bat_meter_data->q_max_pos_50;

		if (temperature > high_temperature)
			temperature = high_temperature;

	}

	ret_Q_max = low_Q_max + (((temperature - low_temperature) * (high_Q_max - low_Q_max)
				 ) / (high_temperature - low_temperature)
	    );

	bm_print(BM_LOG_FULL, "[fgauge_get_Q_max] Q_max = %d\r\n", ret_Q_max);

	return ret_Q_max;
}

/*added for battery ocv*/
s32 battery_meter_get_battery_voltage_OCV(void)
{
	int ret = 0;
	int val = 0;

	if (battery_meter_ctrl == NULL || gFG_Is_Init == false)
		return 0;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &val);
	return val;
}

s32 fgauge_get_Q_max_high_current(s16 temperature)
{
	s32 ret_Q_max = 0;
	s32 low_temperature = 0, high_temperature = 0;
	s32 low_Q_max = 0, high_Q_max = 0;

	if (temperature <= p_bat_meter_data->tempearture_t1) {
		low_temperature = (-10);
		low_Q_max = p_bat_meter_data->q_max_neg_10_h_current;
		high_temperature = p_bat_meter_data->tempearture_t1;
		high_Q_max = p_bat_meter_data->q_max_pos_0_h_current;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (p_bat_meter_data->p_battery_profile_t1_5 &&
		temperature <= p_bat_meter_data->temperature_t1_5) {
		low_temperature = p_bat_meter_data->tempearture_t1;
		low_Q_max = p_bat_meter_data->q_max_pos_0_h_current;
		high_temperature = p_bat_meter_data->temperature_t1_5;
		high_Q_max = p_bat_meter_data->q_max_pos_10_h_current;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= p_bat_meter_data->temperature_t2) {

		if (p_bat_meter_data->p_battery_profile_t1_5) {
			low_temperature = p_bat_meter_data->temperature_t1_5;
			low_Q_max = p_bat_meter_data->q_max_pos_10_h_current;
		} else {
			low_temperature = p_bat_meter_data->tempearture_t1;
			low_Q_max = p_bat_meter_data->q_max_pos_0_h_current;
		}
		high_temperature = p_bat_meter_data->temperature_t2;
		high_Q_max = p_bat_meter_data->q_max_pos_25_h_current;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_temperature = p_bat_meter_data->temperature_t2;
		low_Q_max = p_bat_meter_data->q_max_pos_25_h_current;
		high_temperature = p_bat_meter_data->temperature_t3;
		high_Q_max = p_bat_meter_data->q_max_pos_50_h_current;

		if (temperature > high_temperature)
			temperature = high_temperature;

	}

	ret_Q_max = low_Q_max + (((temperature - low_temperature) * (high_Q_max - low_Q_max)
				 ) / (high_temperature - low_temperature)
	    );

	bm_print(BM_LOG_FULL, "[fgauge_get_Q_max_high_current] Q_max = %d\r\n", ret_Q_max);

	return ret_Q_max;
}

int BattVoltToTemp(int dwVolt)
{
	s64 TRes_temp;
	s64 TRes;
	int sBaTTMP = -100;
	s64 critical_low_v;
	struct BATT_TEMPERATURE *batt_temp_table =
	    (struct BATT_TEMPERATURE *)p_bat_meter_data->p_batt_temperature_table;

	critical_low_v =
	    (batt_temp_table[0].TemperatureR * (s64) p_bat_meter_data->rbat_pull_up_volt);
	do_div(critical_low_v,
	       batt_temp_table[0].TemperatureR + p_bat_meter_data->rbat_pull_up_volt);

	if (dwVolt >= critical_low_v)
		TRes_temp = batt_temp_table[0].TemperatureR;
	else {
		TRes_temp = (p_bat_meter_data->rbat_pull_up_r * (s64) dwVolt);
		do_div(TRes_temp, (p_bat_meter_data->rbat_pull_up_volt - dwVolt));
	}

	TRes = (TRes_temp * p_bat_meter_data->rbat_pull_down_r);
	do_div(TRes, abs(p_bat_meter_data->rbat_pull_down_r - TRes_temp));

	/* convert register to temperature */
	sBaTTMP = BattThermistorConverTemp((int)TRes);

	return sBaTTMP;
}

int force_get_tbat(void)
{
#if defined(CONFIG_POWER_EXT) || defined(FIXED_TBAT_25)
	bm_print(BM_LOG_FULL, "[force_get_tbat] fixed TBAT=25 t\n");
	return 25;
#else
	int bat_temperature_volt = 0;
	int bat_temperature_val = 0;
	int fg_r_value = 0;
	s32 fg_current_temp = 0;
	bool fg_current_state = false;
	int bat_temperature_volt_temp = 0;
	int ret = 0;

#if defined(CONFIG_SOC_BY_HW_FG)
	static int vbat_on_discharge = 0, current_discharge;
	static int vbat_on_charge = 0, current_charge;
	int resident_R = 0;
#endif
	static bool battery_check_done;
	static bool battery_exist;
	u32 baton_count = 0;
	u32 i;

	if (gFG_Is_Init == false)
		return 0;

	if (battery_check_done == false) {
		for (i = 0; i < 3; i++)
			baton_count += bat_charger_get_battery_status();

		if (baton_count >= 3) {
			pr_warn("[BATTERY] No battery detected.\n");
			battery_exist = false;
		} else {
			pr_debug("[BATTERY] Battery detected!\n");
			battery_exist = true;
		}
		battery_check_done = true;
	}
#if defined(CONFIG_FIXED_T25_FOR_NO_BATTERY)
	if (battery_exist == false)
		return 25;
#endif

	/* Get V_BAT_Temperature */
	bat_temperature_volt = 2;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &bat_temperature_volt);

	if (bat_temperature_volt != 0) {
#if defined(CONFIG_SOC_BY_HW_FG)
		fg_r_value = get_r_fg_value();

		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &fg_current_temp);
		ret =
		    battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &fg_current_state);
		fg_current_temp = fg_current_temp / 10;

		if (fg_current_state == true) {
			if (vbat_on_charge == 0) {
				vbat_on_charge = bat_temperature_volt;
				current_charge = fg_current_temp;
			}
			bat_temperature_volt_temp = bat_temperature_volt;
			bat_temperature_volt =
			    bat_temperature_volt - ((fg_current_temp * fg_r_value) / 1000);
		} else {
			if (vbat_on_discharge == 0) {
				vbat_on_discharge = bat_temperature_volt;
				current_discharge = fg_current_temp;
			}
			bat_temperature_volt_temp = bat_temperature_volt;
			bat_temperature_volt =
			    bat_temperature_volt + ((fg_current_temp * fg_r_value) / 1000);
		}

		if (vbat_on_charge != 0 && vbat_on_discharge != 0) {
			if (current_charge + current_discharge != 0) {
				resident_R =
				    (1000 * (vbat_on_charge - vbat_on_discharge)) /
				    (current_charge + current_discharge) -
				    p_bat_meter_data->r_fg_value;
				bm_print(BM_LOG_FULL, "[auto K for resident R] %d\n", resident_R);
			}
			vbat_on_charge = 0;
			vbat_on_discharge = 0;
		}
#endif

		bat_temperature_val = BattVoltToTemp(bat_temperature_volt);
	}

	bm_print(BM_LOG_FULL, "[force_get_tbat] %d,%d,%d,%d,%d,%d\n",
		 bat_temperature_volt_temp, bat_temperature_volt, fg_current_state, fg_current_temp,
		 fg_r_value, bat_temperature_val);

	return bat_temperature_val;
#endif
}
EXPORT_SYMBOL(force_get_tbat);

int fgauge_get_saddles(void)
{
	if (p_bat_meter_data)
		return p_bat_meter_data->battery_profile_saddles;
	else
		return 0;
}

int fgauge_get_saddles_r_table(void)
{
	if (p_bat_meter_data)
		return p_bat_meter_data->battery_r_profile_saddles;
	else
		return 0;
}

struct BATTERY_PROFILE_STRUCT *fgauge_get_profile(u32 temperature)
{
	if (temperature == p_bat_meter_data->temperature_t0)
		return p_bat_meter_data->p_battery_profile_t0;
	else if (temperature == p_bat_meter_data->tempearture_t1)
		return p_bat_meter_data->p_battery_profile_t1;
	else if (temperature == p_bat_meter_data->temperature_t1_5)
		return p_bat_meter_data->p_battery_profile_t1_5;
	else if (temperature == p_bat_meter_data->temperature_t2)
		return p_bat_meter_data->p_battery_profile_t2;
	else if (temperature == p_bat_meter_data->temperature_t3)
		return p_bat_meter_data->p_battery_profile_t3;
	else if (temperature == p_bat_meter_data->temperature_t)
		return p_bat_meter_data->p_battery_profile_temperature;
	else
		return NULL;
}

struct R_PROFILE_STRUCT *fgauge_get_profile_r_table(u32 temperature)
{
	if (temperature == p_bat_meter_data->temperature_t0)
		return p_bat_meter_data->p_r_profile_t0;
	else if (temperature == p_bat_meter_data->tempearture_t1)
		return p_bat_meter_data->p_r_profile_t1;
	else if (temperature == p_bat_meter_data->temperature_t1_5)
		return p_bat_meter_data->p_r_profile_t1_5;
	else if (temperature == p_bat_meter_data->temperature_t2)
		return p_bat_meter_data->p_r_profile_t2;
	else if (temperature == p_bat_meter_data->temperature_t3)
		return p_bat_meter_data->p_r_profile_t3;
	else if (temperature == p_bat_meter_data->temperature_t)
		return p_bat_meter_data->p_r_profile_temperature;
	else
		return NULL;
}

s32 fgauge_read_capacity_by_v(s32 voltage)
{
	int i = 0, saddles = 0;
	struct BATTERY_PROFILE_STRUCT *profile_p;
	s32 ret_percent = 0;

	profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI, "[FGADC] fgauge get ZCV profile : fail !\r\n");
		return 100;
	}

	saddles = fgauge_get_saddles();

	if (voltage > (profile_p + 0)->voltage)
		return 100;	/* battery capacity, not dod */

	if (voltage < (profile_p + saddles - 1)->voltage)
		return 0;	/* battery capacity, not dod */

	for (i = 0; i < saddles - 1; i++) {
		if ((voltage <= (profile_p + i)->voltage)
		    && (voltage >= (profile_p + i + 1)->voltage)) {
			ret_percent =
			    (profile_p + i)->percentage +
			    (((((profile_p + i)->voltage) -
			       voltage) * (((profile_p + i + 1)->percentage) -
					   ((profile_p + i)->percentage))
			     ) / (((profile_p + i)->voltage) - ((profile_p + i + 1)->voltage))
			    );

			break;
		}

	}
	ret_percent = 100 - ret_percent;

	return ret_percent;
}

s32 fgauge_read_v_by_capacity(int bat_capacity)
{
	int i = 0, saddles = 0;
	struct BATTERY_PROFILE_STRUCT *profile_p;
	s32 ret_volt = 0;

	profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI,
			 "[fgauge_read_v_by_capacity] fgauge get ZCV profile : fail !\r\n");
		return 3700;
	}

	saddles = fgauge_get_saddles();

	if (bat_capacity < (profile_p + 0)->percentage)
		return 3700;

	if (bat_capacity > (profile_p + saddles - 1)->percentage)
		return 3700;

	for (i = 0; i < saddles - 1; i++) {
		if ((bat_capacity >= (profile_p + i)->percentage)
		    && (bat_capacity <= (profile_p + i + 1)->percentage)) {
			ret_volt =
			    (profile_p + i)->voltage -
			    (((bat_capacity -
			       ((profile_p + i)->percentage)) * (((profile_p + i)->voltage) -
								 ((profile_p + i + 1)->voltage))
			     ) / (((profile_p + i + 1)->percentage) - ((profile_p + i)->percentage))
			    );

			break;
		}
	}

	return ret_volt;
}

s32 fgauge_read_d_by_v(s32 volt_bat)
{
	int i = 0, saddles = 0;
	struct BATTERY_PROFILE_STRUCT *profile_p;
	s32 ret_d = 0;

	profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI, "[FGADC] fgauge get ZCV profile : fail !\r\n");
		return 100;
	}

	saddles = fgauge_get_saddles();

	if (volt_bat > (profile_p + 0)->voltage)
		return 0;

	if (volt_bat < (profile_p + saddles - 1)->voltage)
		return 100;

	for (i = 0; i < saddles - 1; i++) {
		if ((volt_bat <= (profile_p + i)->voltage)
		    && (volt_bat >= (profile_p + i + 1)->voltage)) {
			ret_d =
			    (profile_p + i)->percentage +
			    (((((profile_p + i)->voltage) -
			       volt_bat) * (((profile_p + i + 1)->percentage) -
					    ((profile_p + i)->percentage))
			     ) / (((profile_p + i)->voltage) - ((profile_p + i + 1)->voltage))
			    );

			break;
		}

	}

	return ret_d;
}

s32 fgauge_read_v_by_d(int d_val)
{
	int i = 0, saddles = 0;
	struct BATTERY_PROFILE_STRUCT *profile_p;
	s32 ret_volt = 0;

	profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI,
			 "[fgauge_read_v_by_capacity] fgauge get ZCV profile : fail !\r\n");
		return 3700;
	}

	saddles = fgauge_get_saddles();

	if (d_val < (profile_p + 0)->percentage)
		return 3700;

	if (d_val > (profile_p + saddles - 1)->percentage)
		return 3700;

	for (i = 0; i < saddles - 1; i++) {
		if ((d_val >= (profile_p + i)->percentage)
		    && (d_val <= (profile_p + i + 1)->percentage)) {
			ret_volt =
			    (profile_p + i)->voltage -
			    (((d_val -
			       ((profile_p + i)->percentage)) * (((profile_p + i)->voltage) -
								 ((profile_p + i + 1)->voltage))
			     ) / (((profile_p + i + 1)->percentage) - ((profile_p + i)->percentage))
			    );

			break;
		}
	}

	return ret_volt;
}

s32 fgauge_read_r_bat_by_v(s32 voltage)
{
	int i = 0, saddles = 0;
	struct R_PROFILE_STRUCT *profile_p;
	s32 ret_r = 0;

	profile_p = fgauge_get_profile_r_table(p_bat_meter_data->temperature_t);
	if (profile_p == NULL) {
		bm_print(BM_LOG_ERROR, "[FGADC] fgauge get R-Table profile : fail !\r\n");
		return 0;
	}

	saddles = fgauge_get_saddles_r_table();

	if (voltage > (profile_p + 0)->voltage)
		return (profile_p + 0)->resistance;

	if (voltage < (profile_p + saddles - 1)->voltage)
		return (profile_p + saddles - 1)->resistance;

	for (i = 0; i < saddles - 1; i++) {
		if ((voltage <= (profile_p + i)->voltage)
		    && (voltage >= (profile_p + i + 1)->voltage)) {
			ret_r =
			    (profile_p + i)->resistance +
			    (((((profile_p + i)->voltage) -
			       voltage) * (((profile_p + i + 1)->resistance) -
					   ((profile_p + i)->resistance))
			     ) / (((profile_p + i)->voltage) - ((profile_p + i + 1)->voltage))
			    );
			break;
		}
	}

	return ret_r;
}


void fgauge_construct_battery_profile(s32 temperature,
				      struct BATTERY_PROFILE_STRUCT *temp_profile_p)
{
	struct BATTERY_PROFILE_STRUCT *low_profile_p, *high_profile_p;
	s32 low_temperature, high_temperature;
	int i, saddles;
	s32 temp_v_1 = 0, temp_v_2 = 0;

	if (temperature <= p_bat_meter_data->tempearture_t1) {
		low_profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t0);
		high_profile_p = fgauge_get_profile(p_bat_meter_data->tempearture_t1);
		low_temperature = (-10);
		high_temperature = p_bat_meter_data->tempearture_t1;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (p_bat_meter_data->p_battery_profile_t1_5 &&
		temperature <= p_bat_meter_data->temperature_t1_5) {
		low_profile_p = fgauge_get_profile(p_bat_meter_data->tempearture_t1);
		high_profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t1_5);
		low_temperature = p_bat_meter_data->tempearture_t1;
		high_temperature = p_bat_meter_data->temperature_t1_5;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= p_bat_meter_data->temperature_t2) {

		if (p_bat_meter_data->p_battery_profile_t1_5) {
			low_profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t1_5);
			low_temperature = p_bat_meter_data->temperature_t1_5;
		} else {
			low_profile_p = fgauge_get_profile(p_bat_meter_data->tempearture_t1);
			low_temperature = p_bat_meter_data->tempearture_t1;
		}

		high_profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t2);
		high_temperature = p_bat_meter_data->temperature_t2;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t2);
		high_profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t3);
		low_temperature = p_bat_meter_data->temperature_t2;
		high_temperature = p_bat_meter_data->temperature_t3;

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	saddles = fgauge_get_saddles();

	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->voltage) > ((low_profile_p + i)->voltage)) {
			temp_v_1 = (high_profile_p + i)->voltage;
			temp_v_2 = (low_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			    (((temperature - low_temperature) * (temp_v_1 - temp_v_2)
			     ) / (high_temperature - low_temperature)
			    );
		} else {
			temp_v_1 = (low_profile_p + i)->voltage;
			temp_v_2 = (high_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			    (((high_temperature - temperature) * (temp_v_1 - temp_v_2)
			     ) / (high_temperature - low_temperature)
			    );
		}

		(temp_profile_p + i)->percentage = (high_profile_p + i)->percentage;
#if 0
		(temp_profile_p + i)->voltage = temp_v_2 +
		    (((temperature - low_temperature) * (temp_v_1 - temp_v_2)
		     ) / (high_temperature - low_temperature)
		    );
#endif
	}


	/* Dumpt new battery profile */
	for (i = 0; i < saddles; i++) {
		bm_print(BM_LOG_CRTI, "<DOD,Voltage> at %d = <%d,%d>\r\n",
			 temperature, (temp_profile_p + i)->percentage,
			 (temp_profile_p + i)->voltage);
	}

}

void fgauge_construct_r_table_profile(s32 temperature, struct R_PROFILE_STRUCT *temp_profile_p)
{
	struct R_PROFILE_STRUCT *low_profile_p, *high_profile_p;
	s32 low_temperature, high_temperature;
	int i, saddles;
	s32 temp_v_1 = 0, temp_v_2 = 0;
	s32 temp_r_1 = 0, temp_r_2 = 0;

	if (temperature <= p_bat_meter_data->tempearture_t1) {
		low_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->temperature_t0);
		high_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->tempearture_t1);
		low_temperature = (-10);
		high_temperature = p_bat_meter_data->tempearture_t1;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (p_bat_meter_data->p_battery_profile_t1_5 &&
		temperature <= p_bat_meter_data->temperature_t1_5) {
		low_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->tempearture_t1);
		high_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->temperature_t1_5);
		low_temperature = p_bat_meter_data->tempearture_t1;
		high_temperature = p_bat_meter_data->temperature_t1_5;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= p_bat_meter_data->temperature_t2) {

		if (p_bat_meter_data->p_battery_profile_t1_5) {
			low_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->temperature_t1_5);
			low_temperature = p_bat_meter_data->temperature_t1_5;
		} else {
			low_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->tempearture_t1);
			low_temperature = p_bat_meter_data->tempearture_t1;
		}

		high_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->temperature_t2);
		high_temperature = p_bat_meter_data->temperature_t2;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->temperature_t2);
		high_profile_p = fgauge_get_profile_r_table(p_bat_meter_data->temperature_t3);
		low_temperature = p_bat_meter_data->temperature_t2;
		high_temperature = p_bat_meter_data->temperature_t3;

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	saddles = fgauge_get_saddles_r_table();

	/* Interpolation for V_BAT */
	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->voltage) > ((low_profile_p + i)->voltage)) {
			temp_v_1 = (high_profile_p + i)->voltage;
			temp_v_2 = (low_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			    (((temperature - low_temperature) * (temp_v_1 - temp_v_2)
			     ) / (high_temperature - low_temperature)
			    );
		} else {
			temp_v_1 = (low_profile_p + i)->voltage;
			temp_v_2 = (high_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			    (((high_temperature - temperature) * (temp_v_1 - temp_v_2)
			     ) / (high_temperature - low_temperature)
			    );
		}

#if 0
		/* (temp_profile_p + i)->resistance = (high_profile_p + i)->resistance; */

		(temp_profile_p + i)->voltage = temp_v_2 +
		    (((temperature - low_temperature) * (temp_v_1 - temp_v_2)
		     ) / (high_temperature - low_temperature)
		    );
#endif
	}

	/* Interpolation for R_BAT */
	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->resistance) > ((low_profile_p + i)->resistance)) {
			temp_r_1 = (high_profile_p + i)->resistance;
			temp_r_2 = (low_profile_p + i)->resistance;

			(temp_profile_p + i)->resistance = temp_r_2 +
			    (((temperature - low_temperature) * (temp_r_1 - temp_r_2)
			     ) / (high_temperature - low_temperature)
			    );
		} else {
			temp_r_1 = (low_profile_p + i)->resistance;
			temp_r_2 = (high_profile_p + i)->resistance;

			(temp_profile_p + i)->resistance = temp_r_2 +
			    (((high_temperature - temperature) * (temp_r_1 - temp_r_2)
			     ) / (high_temperature - low_temperature)
			    );
		}

#if 0
		/* (temp_profile_p + i)->voltage = (high_profile_p + i)->voltage; */

		(temp_profile_p + i)->resistance = temp_r_2 +
		    (((temperature - low_temperature) * (temp_r_1 - temp_r_2)
		     ) / (high_temperature - low_temperature)
		    );
#endif
	}

	/* Dumpt new r-table profile */
	for (i = 0; i < saddles; i++) {
		bm_print(BM_LOG_CRTI, "<Rbat,VBAT> at %d = <%d,%d>\r\n",
			 temperature, (temp_profile_p + i)->resistance,
			 (temp_profile_p + i)->voltage);
	}

}

#ifdef CONFIG_CUSTOM_BATTERY_CYCLE_AGING_DATA

s32 get_battery_aging_factor(s32 cycle)
{
	s32 i, f1, f2, c1, c2;
	s32 saddles;
	s32 factor;
	struct BATTERY_CYCLE_STRUCT *battery_aging_table;

	if (p_bat_meter_data && p_bat_meter_data->p_battery_aging_table)
		saddles = p_bat_meter_data->battery_aging_table_saddles;
	else
		return 100;

	battery_aging_table = p_bat_meter_data->p_battery_aging_table;

	for (i = 0; i < saddles; i++) {
		if (battery_aging_table[i].cycle == cycle)
			return battery_aging_table[i].aging_factor;

		if (battery_aging_table[i].cycle > cycle) {
			if (i == 0)
				return 100;

			if (battery_aging_table[i].aging_factor >
			    battery_aging_table[i - 1].aging_factor) {
				f1 = battery_aging_table[i].aging_factor;
				f2 = battery_aging_table[i - 1].aging_factor;
				c1 = battery_aging_table[i].cycle;
				c2 = battery_aging_table[i - 1].cycle;
				factor = (f2 + ((cycle - c2) * (f1 - f2)) / (c1 - c2));
				return factor;
			}
			f1 = battery_aging_table[i - 1].aging_factor;
			f2 = battery_aging_table[i].aging_factor;
			c1 = battery_aging_table[i].cycle;
			c2 = battery_aging_table[i - 1].cycle;
			factor = (f2 + ((cycle - c2) * (f1 - f2)) / (c1 - c2));
			return factor;
		}
	}

	return battery_aging_table[saddles - 1].aging_factor;
}

#endif

void update_qmax_by_cycle(void)
{
#ifdef CONFIG_CUSTOM_BATTERY_CYCLE_AGING_DATA
	s32 factor = 0;
	s32 aging_capacity;

	factor = get_battery_aging_factor(gFG_battery_cycle);

	mutex_lock(&qmax_mutex);
	if (factor > 0 && factor < 100) {
		bm_print(BM_LOG_CRTI, "[FG] cycle count to aging factor %d\n", factor);
		aging_capacity = gFG_BATT_CAPACITY * factor / 100;
		if (aging_capacity < gFG_BATT_CAPACITY_aging) {
			bm_print(BM_LOG_CRTI, "[FG] update gFG_BATT_CAPACITY_aging to %d\n",
				 aging_capacity);
			gFG_BATT_CAPACITY_aging = aging_capacity;
		}
	}
	mutex_unlock(&qmax_mutex);
#endif
}

void update_qmax_by_aging_factor(void)
{
#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	s32 aging_capacity;

	mutex_lock(&qmax_mutex);
	if (gFG_aging_factor < 100 && gFG_aging_factor > 0) {
		aging_capacity = gFG_BATT_CAPACITY * gFG_aging_factor / 100;
		if (aging_capacity < gFG_BATT_CAPACITY_aging) {
			bm_print(BM_LOG_CRTI,
				 "[FG] update gFG_BATT_CAPACITY_aging to %d\n", aging_capacity);
			gFG_BATT_CAPACITY_aging = aging_capacity;
		}
	}
	mutex_unlock(&qmax_mutex);
#endif
}

void update_qmax_by_temp(void)
{
	mutex_lock(&qmax_mutex);
	gFG_BATT_CAPACITY = fgauge_get_Q_max(gFG_temp);
	gFG_BATT_CAPACITY_init_high_current = fgauge_get_Q_max_high_current(gFG_temp);
	gFG_BATT_CAPACITY_aging = gFG_BATT_CAPACITY;
	mutex_unlock(&qmax_mutex);

	update_qmax_by_cycle();
	update_qmax_by_aging_factor();

	bm_print(BM_LOG_CRTI,
		 "[fgauge_update_dod] gFG_BATT_CAPACITY=%d, gFG_BATT_CAPACITY_aging=%d, gFG_BATT_CAPACITY_init_high_current=%d\r\n",
		 gFG_BATT_CAPACITY, gFG_BATT_CAPACITY_aging, gFG_BATT_CAPACITY_init_high_current);
}

void dod_init(void)
{
#if defined(CONFIG_SOC_BY_HW_FG)
	int ret = 0;
	/* use get_hw_ocv----------------------------------------------------------------- */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &gFG_voltage_OCV);
	gFG_capacity_by_v = fgauge_read_capacity_by_v(gFG_voltage_OCV);

	g_hw_ocv_debug = gFG_voltage_OCV;
	g_hw_soc_debug = gFG_capacity_by_v;
	g_sw_soc_debug = gFG_capacity_by_v_init;
	g_fg_dbg_bat_hwocv = gFG_voltage_OCV;

	bm_print(BM_LOG_FULL, "[FGADC] get_hw_ocv=%d, HW_SOC=%d, SW_SOC = %d\n",
		 gFG_voltage_OCV, gFG_capacity_by_v, gFG_capacity_by_v_init);

	/* compare with hw_ocv & sw_ocv, check if less than or equal to 5% tolerance */
	if (abs(gFG_capacity_by_v_init - gFG_capacity_by_v) > 5)
		gFG_capacity_by_v = gFG_capacity_by_v_init;
	/* ------------------------------------------------------------------------------- */
#endif

#if defined(CONFIG_POWER_EXT)
	g_rtc_fg_soc = gFG_capacity_by_v;
#else
	g_rtc_fg_soc = get_rtc_spare_fg_value();
	g_rtc_soc_debug = g_rtc_fg_soc;
#endif

	pr_warn("%s: %d, %d, %d, %d\n", __func__, g_hw_ocv_debug, g_hw_soc_debug, g_sw_soc_debug,
		  g_rtc_soc_debug);

#if defined(CONFIG_SOC_BY_HW_FG)
	/* decrease rtc soc by 1 if swocv is less by threshold 15% */
	if (g_rtc_fg_soc > 1 && g_rtc_fg_soc >= gFG_capacity_by_v_init + 15) {
		g_rtc_fg_soc -= 1;
		set_rtc_spare_fg_value(g_rtc_fg_soc);
	}

	/* increase rtc soc by 1 if swocv is more by threshold 10% */
	if (bat_is_charger_exist() == true &&
	    g_rtc_fg_soc > 1 && gFG_capacity_by_v_init - g_rtc_fg_soc > 10) {
		g_rtc_fg_soc += 1;
		set_rtc_spare_fg_value(g_rtc_fg_soc);
	}
#endif

	if (((g_rtc_fg_soc != 0)
	     &&
	     ((abs(g_rtc_fg_soc - gFG_capacity_by_v) <
	       p_bat_meter_data->poweron_delta_capacity_tolerance)
	      || (abs(g_rtc_fg_soc - gFG_capacity_by_v_init) <
		  p_bat_meter_data->poweron_delta_capacity_tolerance))
	     &&
	     ((gFG_capacity_by_v > p_bat_meter_data->poweron_low_capacity_tolerance
	       || bat_is_charger_exist() == true)))
	    || ((g_rtc_fg_soc != 0)
		&& (get_boot_reason() == BR_WDT_BY_PASS_PWK || get_boot_reason() == BR_WDT
		    || get_boot_mode() == RECOVERY_BOOT))) {

		gFG_capacity_by_v = g_rtc_fg_soc;
	}
	bm_print(BM_LOG_FULL, "[FGADC] g_rtc_fg_soc=%d, gFG_capacity_by_v=%d\n",
		 g_rtc_fg_soc, gFG_capacity_by_v);

	if (gFG_capacity_by_v == 0 && bat_is_charger_exist() == true) {
		gFG_capacity_by_v = 1;

		bm_print(BM_LOG_FULL, "[FGADC] gFG_capacity_by_v=%d\n", gFG_capacity_by_v);
	}
	gFG_capacity = gFG_capacity_by_v;
	gFG_capacity_by_c_init = gFG_capacity;
	gFG_capacity_by_c = gFG_capacity;

	gFG_DOD0 = 100 - gFG_capacity;
	gFG_DOD1 = gFG_DOD0;

	gfg_percent_check_point = gFG_capacity;

#if 1				/* defined(CHANGE_TRACKING_POINT) */
	gFG_15_vlot = fgauge_read_v_by_capacity((100 - g_tracking_point));
	bm_print(BM_LOG_FULL, "[FGADC] gFG_15_vlot = %dmV\n", gFG_15_vlot);
#else
	/* gFG_15_vlot = fgauge_read_v_by_capacity(86); //14% */
	gFG_15_vlot = fgauge_read_v_by_capacity((100 - g_tracking_point));
	bm_print(BM_LOG_FULL, "[FGADC] gFG_15_vlot = %dmV\n", gFG_15_vlot);
	if ((gFG_15_vlot > 3800) || (gFG_15_vlot < 3600)) {
		bm_print(BM_LOG_CRTI, "[FGADC] gFG_15_vlot(%d) over range, reset to 3700\n",
			 gFG_15_vlot);
		gFG_15_vlot = 3700;
	}
#endif
}


#if defined(CONFIG_SOC_BY_HW_FG)
void update_fg_dbg_tool_value(void)
{
	g_fg_dbg_bat_volt = gFG_voltage_init;

	if (gFG_Is_Charging == true)
		g_fg_dbg_bat_current = gFG_current;
	else
		g_fg_dbg_bat_current = -gFG_current;

	g_fg_dbg_bat_zcv = gFG_voltage;

	g_fg_dbg_bat_temp = gFG_temp;

	g_fg_dbg_bat_r = gFG_resistance_bat;

	g_fg_dbg_bat_car = gFG_columb;

	g_fg_dbg_bat_qmax = gFG_BATT_CAPACITY_aging;

	g_fg_dbg_d0 = gFG_DOD0;

	g_fg_dbg_d1 = gFG_DOD1;

	g_fg_dbg_percentage = bat_get_ui_percentage();

	g_fg_dbg_percentage_fg = gFG_capacity_by_c;

	g_fg_dbg_percentage_voltmode = gfg_percent_check_point;
}

#if 0
s32 fgauge_compensate_battery_voltage_recursion(s32 ori_voltage, s32 recursion_time)
{
	s32 ret_compensate_value = 0;
	s32 temp_voltage_1 = ori_voltage;
	s32 temp_voltage_2 = temp_voltage_1;
	int i = 0;

	for (i = 0; i < recursion_time; i++) {
		gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2);	/* Ohm */
		ret_compensate_value =
		    (gFG_current * (gFG_resistance_bat + p_bat_meter_data->r_fg_value)) / 1000;
		ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;

		if (gFG_Is_Charging == true)
			ret_compensate_value = ret_compensate_value - (ret_compensate_value * 2);

		temp_voltage_2 = temp_voltage_1 + ret_compensate_value;

		bm_print(BM_LOG_FULL,
			 "[fgauge_compensate_battery_voltage_recursion] %d,%d,%d,%d\r\n",
			 temp_voltage_1, temp_voltage_2, gFG_resistance_bat, ret_compensate_value);
	}

	gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2);	/* Ohm */
	ret_compensate_value =
	    (gFG_current *
	     (gFG_resistance_bat + p_bat_meter_data->r_fg_value +
	      p_bat_meter_data->fg_meter_resistance)) / 1000;
	ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;

	if (gFG_Is_Charging == true)
		ret_compensate_value = ret_compensate_value - (ret_compensate_value * 2);

	gFG_compensate_value = ret_compensate_value;

	bm_print(BM_LOG_FULL, "[fgauge_compensate_battery_voltage_recursion] %d,%d,%d,%d\r\n",
		 temp_voltage_1, temp_voltage_2, gFG_resistance_bat, ret_compensate_value);

	return ret_compensate_value;
}

s32 fgauge_get_dod0(s32 voltage, s32 temperature, bool bOcv)
{
	s32 dod0 = 0;
	int i = 0, saddles = 0, jj = 0;
	struct BATTERY_PROFILE_STRUCT *profile_p;
	struct R_PROFILE_STRUCT *profile_p_r_table;
	int ret = 0;

/* R-Table (First Time) */
	/* Re-constructure r-table profile according to current temperature */
	profile_p_r_table = fgauge_get_profile_r_table(p_bat_meter_data->temperature_t);
	if (profile_p_r_table == NULL) {
		bm_print(BM_LOG_ERROR,
			 "[FGADC] fgauge_get_profile_r_table : create table fail !\r\n");
		return 0;
	}
	fgauge_construct_r_table_profile(temperature, profile_p_r_table);

	/* Re-constructure battery profile according to current temperature */
	profile_p = fgauge_get_profile(p_bat_meter_data->temperature_t);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI, "[FGADC] fgauge_get_profile : create table fail !\r\n");
		return 100;
	}
	fgauge_construct_battery_profile(temperature, profile_p);

	/* Get total saddle points from the battery profile */
	saddles = fgauge_get_saddles();

	/* If the input voltage is not OCV, compensate to ZCV due to battery loading */
	/* Compasate battery voltage from current battery voltage */
	jj = 0;
	if (bOcv == false) {
		while (gFG_current == 0) {
			ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
			if (jj > 10)
				break;
			jj++;
		}

		/*voltage = voltage + fgauge_compensate_battery_voltage_recursion(voltage, 5);*/	/* mV */
		voltage = battery_meter_get_battery_voltage_OCV();
		bm_print(BM_LOG_FULL, "[FGADC]battery ocv voltage=%d\r\n", voltage);
	}
	/* If battery voltage is less then mimimum profile voltage, then return 100 */
	/* If battery voltage is greater then maximum profile voltage, then return 0 */
	if (voltage > (profile_p + 0)->voltage)
		return 0;

	if (voltage < (profile_p + saddles - 1)->voltage)
		return 100;

	/* get DOD0 according to current temperature */
	for (i = 0; i < saddles - 1; i++) {
		if ((voltage <= (profile_p + i)->voltage)
		    && (voltage >= (profile_p + i + 1)->voltage)) {
			dod0 =
			    (profile_p + i)->percentage +
			    (((((profile_p + i)->voltage) -
			       voltage) * (((profile_p + i + 1)->percentage) -
					   ((profile_p + i)->percentage))
			     ) / (((profile_p + i)->voltage) - ((profile_p + i + 1)->voltage))
			    );

			break;
		}
	}

	return dod0;
}


s32 fgauge_update_dod(void)
{
	s32 FG_dod_1 = 0;
	int adjust_coulomb_counter = p_bat_meter_data->car_tune_value;

	if (gFG_DOD0 > 100) {
		gFG_DOD0 = 100;
		bm_print(BM_LOG_FULL, "[fgauge_update_dod] gFG_DOD0 set to 100, gFG_columb=%d\r\n",
			 gFG_columb);
	} else if (gFG_DOD0 < 0) {
		gFG_DOD0 = 0;
		bm_print(BM_LOG_FULL, "[fgauge_update_dod] gFG_DOD0 set to 0, gFG_columb=%d\r\n",
			 gFG_columb);
	} else {
	}

	FG_dod_1 = gFG_DOD0 - ((gFG_columb * 100) / gFG_BATT_CAPACITY_aging);

	bm_print(BM_LOG_FULL,
		 "[fgauge_update_dod] FG_dod_1=%d, adjust_coulomb_counter=%d, gFG_columb=%d, gFG_DOD0=%d, gFG_temp=%d, gFG_BATT_CAPACITY=%d\r\n",
		 FG_dod_1, adjust_coulomb_counter, gFG_columb, gFG_DOD0, gFG_temp,
		 gFG_BATT_CAPACITY);

	if (FG_dod_1 > 100) {
		FG_dod_1 = 100;
		bm_print(BM_LOG_FULL, "[fgauge_update_dod] FG_dod_1 set to 100, gFG_columb=%d\r\n",
			 gFG_columb);
	} else if (FG_dod_1 < 0) {
		FG_dod_1 = 0;
		bm_print(BM_LOG_FULL, "[fgauge_update_dod] FG_dod_1 set to 0, gFG_columb=%d\r\n",
			 gFG_columb);
	} else {
	}

	return FG_dod_1;
}


s32 fgauge_read_capacity(s32 type)
{
	s32 voltage;
	s32 temperature;
	s32 dvalue = 0;
	s32 temp_val = 0;

	if (type == 0) {	/* for initialization */
		/* Use voltage to calculate capacity */
		voltage = battery_meter_get_battery_voltage();	/* in unit of mV */
		temperature = battery_meter_get_battery_temperature();
		dvalue = fgauge_get_dod0(voltage, temperature, false);	/* need compensate vbat */
	} else {
		/* Use DOD0 and columb counter to calculate capacity */
		dvalue = fgauge_update_dod();	/* DOD1 = DOD0 + (-CAR)/Qmax */
	}

	gFG_DOD1 = dvalue;

#if 0
	/* Battery Aging update ---------------------------------------------------------- */
	dvalue_new = dvalue;
	dvalue =
	    ((dvalue_new * gFG_BATT_CAPACITY_init_high_current * 100) / gFG_BATT_CAPACITY_aging) /
	    100;
	bm_print(BM_LOG_FULL,
		 "[fgauge_read_capacity] dvalue=%d, dvalue_new=%d, gFG_BATT_CAPACITY_init_high_current=%d, gFG_BATT_CAPACITY_aging=%d\r\n",
		 dvalue, dvalue_new, gFG_BATT_CAPACITY_init_high_current, gFG_BATT_CAPACITY_aging);
	/* ---------------------------------------------------------------------------- */
#endif

	temp_val = dvalue;
	dvalue = 100 - temp_val;

	if (dvalue <= 1) {
		dvalue = 1;
		bm_print(BM_LOG_FULL, "[fgauge_read_capacity] dvalue<=1 and set dvalue=1 !!\r\n");
	}

	return dvalue;
}
#endif

void fg_voltage_mode(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	if (bat_is_charger_exist() == true) {
		/* SOC only UP when charging */
		if (gFG_capacity_by_v > gfg_percent_check_point)
			gfg_percent_check_point++;

	} else {
		/* SOC only Done when dis-charging */
		if (gFG_capacity_by_v < gfg_percent_check_point)
			gfg_percent_check_point--;

	}

	bm_print(BM_LOG_FULL,
		 "[FGADC_VoltageMothod] gFG_capacity_by_v=%d,gfg_percent_check_point=%d\r\n",
		 gFG_capacity_by_v, gfg_percent_check_point);
#endif
}


void fgauge_algo_run(void)
{
	int ret = 0;
#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	int columb_delta = 0;
	int charge_current = 0;
#endif

/* 1. Get Raw Data */
	gFG_voltage = battery_meter_get_battery_voltage();
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &gFG_Is_Charging);

	gFG_voltage_init = gFG_voltage;
	/*gFG_voltage = gFG_voltage + fgauge_compensate_battery_voltage_recursion(gFG_voltage, 5);*/	/* mV */
	gFG_voltage_OCV = battery_meter_get_battery_voltage_OCV();
	/*gFG_voltage = gFG_voltage + p_bat_meter_data->ocv_board_compesate;*/

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	if (gFG_Is_Charging) {
		charge_current -= gFG_current;
		if (charge_current < gFG_min_current)
			gFG_min_current = charge_current;
	} else {
		if (gFG_current > gFG_max_current)
			gFG_max_current = gFG_current;
	}

	columb_delta = gFG_pre_columb_count - gFG_columb;

	if (columb_delta < 0)
		columb_delta = columb_delta - 2 * columb_delta;	/* absolute value */

	gFG_pre_columb_count = gFG_columb;
	gFG_columb_sum += columb_delta;

	/* should we use gFG_BATT_CAPACITY or gFG_BATT_CAPACITY_aging ?? */
	if (gFG_columb_sum >= 2 * gFG_BATT_CAPACITY_aging) {
		gFG_battery_cycle++;
		gFG_columb_sum -= 2 * gFG_BATT_CAPACITY_aging;
		bm_print(BM_LOG_CRTI, "Update battery cycle count to %d. \r\n", gFG_battery_cycle);
	}
	bm_print(BM_LOG_FULL, "@@@ bat cycle count %d, columb sum %d. \r\n", gFG_battery_cycle,
		 gFG_columb_sum);
#endif

/* 2. Calculate battery capacity by VBAT */
	gFG_capacity_by_v = fgauge_read_capacity_by_v(gFG_voltage_OCV);

/* 3. Calculate battery capacity by Coulomb Counter */
	gFG_capacity_by_c = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);

/* 4. voltage mode */
/*
	if (volt_mode_update_timer >= volt_mode_update_time_out) {
		volt_mode_update_timer = 0;

		fg_voltage_mode();
	} else {
		volt_mode_update_timer++;
	}
*/

/* 5. Logging */
	bm_print(BM_LOG_FULL, "[FGADC] GG init cond. hw_ocv=%d, HW_SOC=%d, SW_SOC=%d, RTC_SOC=%d\n",
		 g_hw_ocv_debug, g_hw_soc_debug, g_sw_soc_debug, g_rtc_soc_debug);

	bm_print(BM_LOG_FULL,
		 "[FGADC] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
		 gFG_Is_Charging, gFG_current, gFG_columb, gFG_voltage, gFG_voltage_OCV, gFG_capacity_by_v,
		 gFG_capacity_by_c, gFG_capacity_by_c_init, gFG_BATT_CAPACITY, gFG_DOD0, gFG_DOD1);
	update_fg_dbg_tool_value();
}

void fgauge_algo_run_init(void)
{
	int ret = 0;

	/* disable power path to avoid elevated voltage by charger */
	bat_charger_enable(false);
	bat_charger_enable_power_path(false);
	msleep(100);

/* 1. Get Raw Data */
	gFG_voltage = battery_meter_get_battery_voltage();
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &gFG_Is_Charging);

	bat_charger_enable(true);
	bat_charger_enable_power_path(true);

	gFG_voltage_init = gFG_voltage;
#if 0
	gFG_voltage = gFG_voltage + fgauge_compensate_battery_voltage_recursion(gFG_voltage, 5);	/* mV */
	gFG_voltage = gFG_voltage + p_bat_meter_data->ocv_board_compesate;

	pr_warn("cv:%d ocv:%d i:%d r:%d\n", gFG_voltage_init, gFG_voltage, gFG_current,
		  gFG_resistance_bat);
#endif

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);
#if 0
/* 1.1 Average FG_voltage */
	for (i = 0; i < p_bat_meter_data->fg_vbat_average_size; i++)
		FGvbatVoltageBuffer[i] = gFG_voltage;

	FGbatteryVoltageSum = gFG_voltage * p_bat_meter_data->fg_vbat_average_size;
	gFG_voltage_AVG = gFG_voltage;
#endif
/* 2. Calculate battery capacity by VBAT */
	gFG_capacity_by_v = fgauge_read_capacity_by_v(gFG_voltage);
	gFG_capacity_by_v_init = gFG_capacity_by_v;

/* 3. Calculate battery capacity by Coulomb Counter */
	gFG_capacity_by_c = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);

/* 4. update DOD0 */

	dod_init();

/* 5. Logging */
	bm_print(BM_LOG_FULL,
		 "[FGADC] %d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
		 gFG_Is_Charging, gFG_current, gFG_columb, gFG_voltage, gFG_capacity_by_v,
		 gFG_capacity_by_c, gFG_capacity_by_c_init, gFG_BATT_CAPACITY, gFG_voltage_init);
	update_fg_dbg_tool_value();
}

void fgauge_initialization(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	u32 ret = 0;

	/* gFG_BATT_CAPACITY_init_high_current = fgauge_get_Q_max_high_current(25); */
	/* gFG_BATT_CAPACITY_aging = fgauge_get_Q_max(25); */

	/* 1. HW initialization */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_HW_FG_INIT, p_bat_meter_data);

	gFG_Is_Init = true;

	/* 2. SW algorithm initialization */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &gFG_voltage);

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
	/*
	i = 0;
	while (gFG_current == 0) {
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
		if (i > 10) {
			bm_print(BM_LOG_CRTI, "[fgauge_initialization] gFG_current == 0\n");
			break;
		}
		i++;
	}
	*/
	gFG_capacity = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);
	gFG_temp = battery_meter_get_battery_temperature();
	/*gFG_capacity = fgauge_read_capacity(0);*/

	gFG_capacity_by_c_init = gFG_capacity;
	gFG_capacity_by_c = gFG_capacity;
	gFG_capacity_by_v = gFG_capacity;

	gFG_DOD0 = 100 - gFG_capacity;

	gFG_BATT_CAPACITY = fgauge_get_Q_max(gFG_temp);

	gFG_BATT_CAPACITY_init_high_current = fgauge_get_Q_max_high_current(gFG_temp);
	gFG_BATT_CAPACITY_aging = fgauge_get_Q_max(gFG_temp);

	ret = battery_meter_ctrl(BATTERY_METER_CMD_DUMP_REGISTER, NULL);

	bm_print(BM_LOG_CRTI, "[fgauge_initialization] Done\n");
#endif
}
#endif

s32 get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level)
{
#if defined(CONFIG_POWER_EXT)

	return first_wakeup_time;

#elif defined(CONFIG_SOC_BY_AUXADC) ||  defined(CONFIG_SOC_BY_SW_FG) || defined(CONFIG_SOC_BY_HW_FG)

	s32 vbat_val = 0;
	s32 ret_time = 600;

	vbat_val = g_sw_vbat_temp;

	/* change wake up period when system suspend. */
	if (vbat_val > p_bat_meter_data->vbat_normal_wakeup)	/* 3.6v */
		ret_time = p_bat_meter_data->normal_wakeup_period;	/* 90 min */
	else if (vbat_val > p_bat_meter_data->vbat_low_power_wakeup)	/* 3.5v */
		ret_time = p_bat_meter_data->low_power_wakeup_period;	/* 5 min */
	else
		ret_time = p_bat_meter_data->close_poweroff_wakeup_period;	/* 0.5 min */

	bm_print(BM_LOG_CRTI, "vbat_val=%d, ret_time=%d\n", vbat_val, ret_time);

	return ret_time;
#else

	s32 car_instant = 0;
	s32 current_instant = 0;
	static s32 car_sleep;
	s32 car_wakeup = 0;
	static s32 last_time;

	s32 ret_val = -1;
	int check_fglog = 0;
	s32 I_sleep = 0;
	s32 new_time = 0;

	int ret = 0;

	check_fglog = Enable_FGADC_LOG;
	/*
	   if (check_fglog == 0)
	   Enable_FGADC_LOG=1;
	 */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &current_instant);

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &car_instant);

	/*
	   if (check_fglog == 0)
	   Enable_FGADC_LOG=0;
	 */
	if (car_instant < 0)
		car_instant = car_instant - (car_instant * 2);

	if (first_use == 1) {
		/* ret_val = 30*60; *//* 30 mins */
		ret_val = first_wakeup_time;
		last_time = ret_val;
		car_sleep = car_instant;
	} else {
		car_wakeup = car_instant;

		if (last_time == 0)
			last_time = 1;

		if (car_sleep > car_wakeup) {
			car_sleep = car_wakeup;
			bm_print(BM_LOG_FULL, "[get_dynamic_period] reset car_sleep\n");
		}

		I_sleep = ((car_wakeup - car_sleep) * 3600) / last_time;	/* unit: second */

		if (I_sleep == 0) {
			/*
			   if (check_fglog == 0)
			   Enable_FGADC_LOG=1;
			 */

			ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &I_sleep);

			I_sleep = I_sleep / 10;
			/*
			   if (check_fglog == 0)
			   Enable_FGADC_LOG=0;
			 */
		}

		if (I_sleep == 0) {
			new_time = first_wakeup_time;
		} else {
			new_time =
			    ((gFG_BATT_CAPACITY * battery_capacity_level * 3600) / 100) / I_sleep;
		}
		ret_val = new_time;

		if (ret_val == 0)
			ret_val = first_wakeup_time;

		bm_print(BM_LOG_CRTI,
			 "[get_dynamic_period] car_instant=%d, car_wakeup=%d, car_sleep=%d, I_sleep=%d, gFG_BATT_CAPACITY=%d, last_time=%d, new_time=%d\r\n",
			 car_instant, car_wakeup, car_sleep, I_sleep, gFG_BATT_CAPACITY, last_time,
			 new_time);

		/* update parameter */
		car_sleep = car_wakeup;
		last_time = ret_val;
	}
	return ret_val;

#endif
}

/* battery ZCV value */
s32 battery_meter_get_battery_voltage(void)
{
	int ret = 0;
	int val = 5;

	if (battery_meter_ctrl == NULL || gFG_Is_Init == false)
		return 0;

	val = 5;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &val);

	g_sw_vbat_temp = val;

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	if (g_sw_vbat_temp > gFG_max_voltage)
		gFG_max_voltage = g_sw_vbat_temp;

	if (g_sw_vbat_temp < gFG_min_voltage)
		gFG_min_voltage = g_sw_vbat_temp;

#endif

	return val;
}

s32 battery_meter_get_battery_voltage_cached(void)
{
	return gFG_voltage_init;
}

s32 battery_meter_get_average_battery_voltage(void)
{
	return get_bat_average_voltage();
}

s32 battery_meter_get_charging_current(void)
{
#if defined(CONFIG_SWCHR_POWER_PATH)
	return 0;
#else
	s32 ADC_BAT_SENSE_tmp[20] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	s32 ADC_BAT_SENSE_sum = 0;
	s32 ADC_BAT_SENSE = 0;
	s32 ADC_I_SENSE_tmp[20] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	s32 ADC_I_SENSE_sum = 0;
	s32 ADC_I_SENSE = 0;
	int repeat = 20;
	int i = 0;
	int j = 0;
	s32 temp = 0;
	int ICharging = 0;
	int ret = 0;
	int val = 1;

	for (i = 0; i < repeat; i++) {
		val = 1;	/* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &val);
		ADC_BAT_SENSE_tmp[i] = val;

		val = 1;	/* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
		ADC_I_SENSE_tmp[i] = val;

		ADC_BAT_SENSE_sum += ADC_BAT_SENSE_tmp[i];
		ADC_I_SENSE_sum += ADC_I_SENSE_tmp[i];
	}

	/* sorting    BAT_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_BAT_SENSE_tmp[j] < ADC_BAT_SENSE_tmp[i]) {
				temp = ADC_BAT_SENSE_tmp[j];
				ADC_BAT_SENSE_tmp[j] = ADC_BAT_SENSE_tmp[i];
				ADC_BAT_SENSE_tmp[i] = temp;
			}
		}
	}

	bm_print(BM_LOG_FULL, "[g_Get_I_Charging:BAT_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		bm_print(BM_LOG_FULL, "%d,", ADC_BAT_SENSE_tmp[i]);

	bm_print(BM_LOG_FULL, "\r\n");

	/* sorting    I_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_I_SENSE_tmp[j] < ADC_I_SENSE_tmp[i]) {
				temp = ADC_I_SENSE_tmp[j];
				ADC_I_SENSE_tmp[j] = ADC_I_SENSE_tmp[i];
				ADC_I_SENSE_tmp[i] = temp;
			}
		}
	}

	bm_print(BM_LOG_FULL, "[g_Get_I_Charging:I_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		bm_print(BM_LOG_FULL, "%d,", ADC_I_SENSE_tmp[i]);

	bm_print(BM_LOG_FULL, "\r\n");

	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[0];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[1];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[18];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[19];
	ADC_BAT_SENSE = ADC_BAT_SENSE_sum / (repeat - 4);

	bm_print(BM_LOG_FULL, "[g_Get_I_Charging] ADC_BAT_SENSE=%d\r\n", ADC_BAT_SENSE);

	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[0];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[1];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[18];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[19];
	ADC_I_SENSE = ADC_I_SENSE_sum / (repeat - 4);

	bm_print(BM_LOG_FULL, "[g_Get_I_Charging] ADC_I_SENSE(Before)=%d\r\n", ADC_I_SENSE);


	bm_print(BM_LOG_FULL, "[g_Get_I_Charging] ADC_I_SENSE(After)=%d\r\n", ADC_I_SENSE);

	if (ADC_I_SENSE > ADC_BAT_SENSE)
		ICharging =
		    (ADC_I_SENSE - ADC_BAT_SENSE +
		     g_I_SENSE_offset) * 1000 / p_bat_meter_data->cust_r_sense;
	else
		ICharging = 0;

	return ICharging;
#endif
}

s32 battery_meter_get_battery_current(void)
{
	int ret = 0;
	s32 val = 0;

	if (battery_meter_ctrl == NULL || gFG_Is_Init == false)
		return 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &val);

	return val;
}

bool battery_meter_get_battery_current_sign(void)
{
	int ret = 0;
	bool val = 0;

	if (battery_meter_ctrl == NULL || gFG_Is_Init == false)
		return 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &val);

	return val;
}

s32 battery_meter_get_car(void)
{
	int ret = 0;
	s32 val = 0;

	if (battery_meter_ctrl == NULL || gFG_Is_Init == false)
		return 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &val);

	return val;
}

s32 battery_meter_get_battery_temperature(void)
{
	int ret = 0;
	int val = 0;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &val);
	return val;
/*
#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	s32 batt_temp = force_get_tbat();

	if (batt_temp > gFG_max_temperature)
		gFG_max_temperature = batt_temp;
	if (batt_temp < gFG_min_temperature)
		gFG_min_temperature = batt_temp;

	return batt_temp;
#else
	return force_get_tbat();
#endif
*/
}

s32 battery_meter_get_charger_voltage(void)
{
	int ret = 0;
	int val = 0;

	if (battery_meter_ctrl == NULL || gFG_Is_Init == false)
		return 0;

	val = 5;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_CHARGER, &val);

	/* val = (((R_CHARGER_1+R_CHARGER_2)*100*val)/R_CHARGER_2)/100; */
	return val;
}

bool battery_meter_ocv2cv_trans_support(void)
{
	if (p_bat_meter_data)
		return p_bat_meter_data->enable_ocv2cv_trans ? true : false;
	else
		return false;
}

s32 battery_meter_get_battery_soc(void)
{
#if defined(CONFIG_SOC_BY_HW_FG)
	return gFG_capacity_by_c;
#else
	return 50;
#endif
}

/* Here we compensate D1 by a factor from Qmax with loading. */
#if 0
s32 battery_meter_trans_battery_percentage(s32 d_val)
{
	s32 d_val_before = 0;
	s32 temp_val = 0;
	s32 C_0mA = 0;
	s32 C_600mA = 0;
	s32 C_current = 0;
	s32 i_avg_current = 0;

	d_val_before = d_val;
	temp_val = battery_meter_get_battery_temperature();
	C_0mA = fgauge_get_Q_max(temp_val);

	/* discharging and current > 600ma */
	i_avg_current = g_currentfactor * (p_bat_meter_data->cv_current / 100);
	if (false == gFG_Is_Charging && g_currentfactor > 100) {
		C_600mA = fgauge_get_Q_max_high_current(temp_val);
		C_current = fgauge_get_Q_max_high_current_by_current(i_avg_current, temp_val);
		if (C_current < C_600mA)
			C_600mA = C_current;
	} else
		C_600mA = fgauge_get_Q_max_high_current(temp_val);

	if (C_0mA > C_600mA)
		d_val = d_val + (((C_0mA - C_600mA) * (d_val)) / C_600mA);

	if (d_val > 100)
		d_val = 100;

	bm_print(BM_LOG_CRTI, "[battery_meter_trans_battery_percentage] %d,%d,%d,%d,%d,%d\r\n",
		 temp_val, C_0mA, C_600mA, d_val_before, d_val, g_currentfactor);

	return d_val;
}
#endif

s32 battery_meter_get_battery_percentage(void)
{
#if defined(CONFIG_POWER_EXT)
	return 50;
#else
/*
	if (bat_is_charger_exist() == false)
		fg_qmax_update_for_aging_flag = 1;
*/
#if defined(CONFIG_SOC_BY_HW_FG)

	fgauge_algo_run();

	/* We keep gFG_capacity_by_c as capacity before compensation */
	/* Compensated capacity is returned for UI SOC tracking */
/*
	if (p_bat_meter_data->enable_ocv2cv_trans)
		return 100 - battery_meter_trans_battery_percentage(100 - gFG_capacity_by_c);
	else
*/
		return gFG_capacity_by_c;
#endif
#endif
}


s32 battery_meter_initial(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else

#if defined(CONFIG_SOC_BY_HW_FG)
	fgauge_initialization();
	fgauge_algo_run_init();
	bm_print(BM_LOG_CRTI, "[battery_meter_initial] CONFIG_SOC_BY_HW_FG done\n");
#endif

	gFG_Is_Init = true;
	return 0;
#endif
}

s32 battery_meter_reset(bool bUI_SOC)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
/*
	u32 ui_percentage = bat_get_ui_percentage();

	if (p_bat_meter_data->enable_ocv2cv_trans) {
		if (false == bUI_SOC) {
			ui_percentage = battery_meter_get_battery_soc();
				bm_print(BM_LOG_FULL, "[battery_meter_reset] use meter soc: %d\n",
				 ui_percentage);
		}
	}

#if defined(CONFIG_QMAX_UPDATE_BY_CHARGED_CAPACITY)
	if (bat_is_charging_full() == true) {
		if (fg_qmax_update_for_aging_flag == 1) {
			fg_qmax_update_for_aging();
			fg_qmax_update_for_aging_flag = 0;
		}
	}
#endif

	reset_parameter_car();
	reset_parameter_dod_full(ui_percentage);
*/
	return 0;
#endif
}

s32 battery_meter_sync(s32 bat_i_sense_offset)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	g_I_SENSE_offset = bat_i_sense_offset;
	return 0;
#endif
}

s32 battery_meter_get_battery_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3987;
#else
	return gFG_voltage;
#endif
}

s32 battery_meter_get_battery_nPercent_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3700;
#else
	return gFG_15_vlot;	/* 15% zcv,  15% can be customized by 100-g_tracking_point */
#endif
}

s32 battery_meter_get_battery_nPercent_UI_SOC(void)
{
#if defined(CONFIG_POWER_EXT)
	return 15;
#else
	return g_tracking_point;	/* tracking point */
#endif
}

s32 battery_meter_get_tempR(s32 dwVolt)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int TRes;

	if (gFG_Is_Init == false)
		return 0;

	TRes =
	    (p_bat_meter_data->rbat_pull_up_r * dwVolt) / (p_bat_meter_data->rbat_pull_up_volt -
							   dwVolt);

	return TRes;
#endif
}

s32 battery_meter_get_batteryR(void)
{
	int vol = 0;
	int ret = 0;
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &vol);
	gFG_resistance_bat  =  fgauge_read_r_bat_by_v(vol);
	return gFG_resistance_bat + p_bat_meter_data->r_fg_value +
	p_bat_meter_data->fg_meter_resistance;
#endif
}

s32 battery_meter_get_tempV(void)   /*battery temp*/
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret = 0;
	int val = 0;

	if (battery_meter_ctrl == NULL || gFG_Is_Init == false)
		return 0;

	val = 1;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &val);
	return val;
#endif
}

s32 battery_meter_get_VSense(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret = 0;
	int val = 0;

	if (battery_meter_ctrl == NULL || gFG_Is_Init == false)
		return 0;

	val = 1;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
	return val;
#endif
}

/* ============================================================ // */
static ssize_t fgadc_log_write(struct file *filp, const char __user *buff,
			       size_t len, loff_t *data)
{
	if (copy_from_user(&proc_fgadc_data, buff, len)) {
		bm_print(BM_LOG_CRTI, "fgadc_log_write error.\n");
		return -EFAULT;
	}

	if (proc_fgadc_data[0] == '1') {
		bm_print(BM_LOG_CRTI, "enable FGADC driver log system\n");
		Enable_FGADC_LOG = 1;
	} else if (proc_fgadc_data[0] == '2') {
		bm_print(BM_LOG_CRTI, "enable FGADC driver log system:2\n");
		Enable_FGADC_LOG = 2;
	} else {
		bm_print(BM_LOG_CRTI, "Disable FGADC driver log system\n");
		Enable_FGADC_LOG = 0;
	}

	return len;
}

static const struct file_operations fgadc_proc_fops = {
	.write = fgadc_log_write,
};

int init_proc_log_fg(void)
{
	int ret = 0;

#if 1
	proc_create("fgadc_log", 0644, NULL, &fgadc_proc_fops);
	bm_print(BM_LOG_CRTI, "proc_create fgadc_proc_fops\n");
#else
	proc_entry_fgadc = create_proc_entry("fgadc_log", 0644, NULL);

	if (proc_entry_fgadc == NULL) {
		ret = -ENOMEM;
		bm_print(BM_LOG_CRTI, "init_proc_log_fg: Couldn't create proc entry\n");
	} else {
		proc_entry_fgadc->write_proc = fgadc_log_write;
		bm_print(BM_LOG_CRTI, "init_proc_log_fg loaded.\n");
	}
#endif

	return ret;
}

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT

/* ============================================================ // */

static ssize_t show_FG_Battery_Cycle(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_battery_cycle  : %d\n", gFG_battery_cycle);
	return sprintf(buf, "%d\n", gFG_battery_cycle);
}

static ssize_t store_FG_Battery_Cycle(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int cycle, ret;

	ret = kstrtoint(buf, 0, &cycle);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	bm_print(BM_LOG_CRTI, "[FG] update battery cycle count: %d\n", cycle);
	gFG_battery_cycle = cycle;
	update_qmax_by_cycle();

	return size;
}

static DEVICE_ATTR(FG_Battery_Cycle, 0664, show_FG_Battery_Cycle, store_FG_Battery_Cycle);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Max_Battery_Voltage(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_max_voltage  : %d\n", gFG_max_voltage);
	return sprintf(buf, "%d\n", gFG_max_voltage);
}

static ssize_t store_FG_Max_Battery_Voltage(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int voltage, ret;

	ret = kstrtoint(buf, 0, &voltage);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (voltage > gFG_max_voltage) {
		bm_print(BM_LOG_CRTI, "[FG] update battery max voltage: %d\n", voltage);
		gFG_max_voltage = voltage;
	}

	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Voltage, 0664, show_FG_Max_Battery_Voltage,
		   store_FG_Max_Battery_Voltage);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Min_Battery_Voltage(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_min_voltage  : %d\n", gFG_min_voltage);
	return sprintf(buf, "%d\n", gFG_min_voltage);
}

static ssize_t store_FG_Min_Battery_Voltage(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int voltage, ret;

	ret = kstrtoint(buf, 0, &voltage);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (voltage < gFG_min_voltage) {
		bm_print(BM_LOG_CRTI, "[FG] update battery min voltage: %d\n", voltage);
		gFG_min_voltage = voltage;
	}

	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Voltage, 0664, show_FG_Min_Battery_Voltage,
		   store_FG_Min_Battery_Voltage);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Max_Battery_Current(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_max_current  : %d\n", gFG_max_current);
	return sprintf(buf, "%d\n", gFG_max_current);
}

static ssize_t store_FG_Max_Battery_Current(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int bat_current, ret;

	ret = kstrtoint(buf, 0, &bat_current);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (bat_current > gFG_max_current) {
		bm_print(BM_LOG_CRTI, "[FG] update battery max current: %d\n", bat_current);
		gFG_max_current = bat_current;
	}

	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Current, 0664, show_FG_Max_Battery_Current,
		   store_FG_Max_Battery_Current);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Min_Battery_Current(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_min_current  : %d\n", gFG_min_current);
	return sprintf(buf, "%d\n", gFG_min_current);
}

static ssize_t store_FG_Min_Battery_Current(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int bat_current, ret;

	ret = kstrtoint(buf, 0, &bat_current);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (bat_current < gFG_min_current) {
		bm_print(BM_LOG_CRTI, "[FG] update battery min current: %d\n", bat_current);
		gFG_min_current = bat_current;
	}

	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Current, 0664, show_FG_Min_Battery_Current,
		   store_FG_Min_Battery_Current);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Max_Battery_Temperature(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_max_temperature  : %d\n", gFG_max_temperature);
	return sprintf(buf, "%d\n", gFG_max_temperature);
}

static ssize_t store_FG_Max_Battery_Temperature(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	int temp, ret;

	ret = kstrtoint(buf, 0, &temp);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (temp > gFG_max_temperature) {
		bm_print(BM_LOG_CRTI, "[FG] update battery max temp: %d\n", temp);
		gFG_max_temperature = temp;
	}

	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Temperature, 0664, show_FG_Max_Battery_Temperature,
		   store_FG_Max_Battery_Temperature);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Min_Battery_Temperature(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_min_temperature  : %d\n", gFG_min_temperature);
	return sprintf(buf, "%d\n", gFG_min_temperature);
}

static ssize_t store_FG_Min_Battery_Temperature(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	int temp, ret;

	ret = kstrtoint(buf, 0, &temp);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}
	if (temp < gFG_min_temperature) {
		bm_print(BM_LOG_CRTI, "[FG] update battery min temp: %d\n", temp);
		gFG_min_temperature = temp;
	}

	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Temperature, 0664, show_FG_Min_Battery_Temperature,
		   store_FG_Min_Battery_Temperature);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Aging_Factor(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_aging_factor  : %d\n", gFG_aging_factor);
	return sprintf(buf, "%d\n", gFG_aging_factor);
}

static ssize_t store_FG_Aging_Factor(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	int factor, ret;

	ret = kstrtoint(buf, 0, &factor);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (factor <= 100 && factor > 0) {
		pr_debug("[FG] update battery aging factor: old(%d), new(%d)\n",
			gFG_aging_factor, factor);

		gFG_aging_factor = factor;
		update_qmax_by_aging_factor();
	} else {
		pr_warn("[FG] try to set aging factor (%d) out of range!\n", factor);
	}

	return size;
}

static DEVICE_ATTR(FG_Aging_Factor, 0664, show_FG_Aging_Factor, store_FG_Aging_Factor);

/* ------------------------------------------------------------------------------------------- */

#endif

#if 0
/* ============================================================ // */
static ssize_t show_FG_R_Offset(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_R_Offset : %d\n", g_R_FG_offset);
	return sprintf(buf, "%d\n", g_R_FG_offset);
}

static ssize_t store_FG_R_Offset(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int offset, ret;

	ret = kstrtoint(buf, 0, &offset);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	bm_print(BM_LOG_CRTI, "[FG] update g_R_FG_offset to %d\n", offset);
	g_R_FG_offset = offset;

	return size;
}

static DEVICE_ATTR(FG_R_Offset, 0664, show_FG_R_Offset, store_FG_R_Offset);
#endif

/* ============================================================ // */
static ssize_t show_FG_Current(struct device *dev, struct device_attribute *attr, char *buf)
{
	s32 ret = 0;
	s32 fg_current_inout_battery = 0;
	s32 val = 0;
	bool is_charging = 0;

	if (battery_meter_ctrl) {
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &val);
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &is_charging);
	}

	if (is_charging == true)
		fg_current_inout_battery = val;
	else
		fg_current_inout_battery = -val;

	bm_print(BM_LOG_FULL, "[FG] gFG_current_inout_battery : %d\n", fg_current_inout_battery);
	return sprintf(buf, "%d\n", fg_current_inout_battery);
}

static ssize_t store_FG_Current(struct device *dev, struct device_attribute *attr, const char *buf,
				size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_Current, 0664, show_FG_Current, store_FG_Current);

/* ============================================================ // */
static ssize_t show_FG_g_fg_dbg_bat_volt(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_volt : %d\n", g_fg_dbg_bat_volt);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_volt);
}

static ssize_t store_FG_g_fg_dbg_bat_volt(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_volt, 0664, show_FG_g_fg_dbg_bat_volt,
		   store_FG_g_fg_dbg_bat_volt);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_hwocv(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_hwocv : %d\n", g_fg_dbg_bat_hwocv);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_hwocv);
}

static ssize_t store_FG_g_fg_dbg_bat_hwocv(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_hwocv, 0664, show_FG_g_fg_dbg_bat_hwocv,
		   store_FG_g_fg_dbg_bat_hwocv);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_current(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_current : %d\n", g_fg_dbg_bat_current);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_current);
}

static ssize_t store_FG_g_fg_dbg_bat_current(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_current, 0664, show_FG_g_fg_dbg_bat_current,
		   store_FG_g_fg_dbg_bat_current);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_zcv(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_zcv : %d\n", g_fg_dbg_bat_zcv);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_zcv);
}

static ssize_t store_FG_g_fg_dbg_bat_zcv(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_zcv, 0664, show_FG_g_fg_dbg_bat_zcv, store_FG_g_fg_dbg_bat_zcv);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_temp(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_temp : %d\n", g_fg_dbg_bat_temp);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_temp);
}

static ssize_t store_FG_g_fg_dbg_bat_temp(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_temp, 0664, show_FG_g_fg_dbg_bat_temp,
		   store_FG_g_fg_dbg_bat_temp);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_r(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_r : %d\n", g_fg_dbg_bat_r);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_r);
}

static ssize_t store_FG_g_fg_dbg_bat_r(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_r, 0664, show_FG_g_fg_dbg_bat_r, store_FG_g_fg_dbg_bat_r);

#if 0
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_car(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_car : %d\n", g_fg_dbg_bat_car);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_car);
}

static ssize_t store_FG_g_fg_dbg_bat_car(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_car, 0664, show_FG_g_fg_dbg_bat_car, store_FG_g_fg_dbg_bat_car);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_qmax(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_qmax : %d\n", g_fg_dbg_bat_qmax);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_qmax);
}

static ssize_t store_FG_g_fg_dbg_bat_qmax(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_qmax, 0664, show_FG_g_fg_dbg_bat_qmax,
		   store_FG_g_fg_dbg_bat_qmax);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_d0(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_d0 : %d\n", g_fg_dbg_d0);
	return sprintf(buf, "%d\n", g_fg_dbg_d0);
}

static ssize_t store_FG_g_fg_dbg_d0(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_d0, 0664, show_FG_g_fg_dbg_d0, store_FG_g_fg_dbg_d0);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_d1(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_d1 : %d\n", g_fg_dbg_d1);
	return sprintf(buf, "%d\n", g_fg_dbg_d1);
}

static ssize_t store_FG_g_fg_dbg_d1(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_d1, 0664, show_FG_g_fg_dbg_d1, store_FG_g_fg_dbg_d1);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_percentage(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_percentage : %d\n", g_fg_dbg_percentage);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage);
}

static ssize_t store_FG_g_fg_dbg_percentage(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage, 0664, show_FG_g_fg_dbg_percentage,
		   store_FG_g_fg_dbg_percentage);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_percentage_fg(struct device *dev, struct device_attribute *attr,
					      char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_percentage_fg : %d\n", g_fg_dbg_percentage_fg);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage_fg);
}

static ssize_t store_FG_g_fg_dbg_percentage_fg(struct device *dev, struct device_attribute *attr,
					       const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_fg, 0664, show_FG_g_fg_dbg_percentage_fg,
		   store_FG_g_fg_dbg_percentage_fg);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_percentage_voltmode(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_percentage_voltmode : %d\n",
		 g_fg_dbg_percentage_voltmode);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage_voltmode);
}

static ssize_t store_FG_g_fg_dbg_percentage_voltmode(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_voltmode, 0664, show_FG_g_fg_dbg_percentage_voltmode,
		   store_FG_g_fg_dbg_percentage_voltmode);

/* ============================================================ // */

static ssize_t show_car_tune_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "car_tune_value: %d\n", p_bat_meter_data->car_tune_value);
	return sprintf(buf, "%d\n", p_bat_meter_data->car_tune_value);
}

static ssize_t store_car_tune_value(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int car_tune_value, ret;

	ret = kstrtoint(buf, 0, &car_tune_value);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (p_bat_meter_data)
		p_bat_meter_data->car_tune_value = car_tune_value;

	return size;
}

static DEVICE_ATTR(car_tune_value, S_IRUSR | S_IWUSR, show_car_tune_value, store_car_tune_value);
#endif

static ssize_t show_charging_current_limit(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "%u mA\n", bat_charger_get_charging_current());
}

static ssize_t store_charging_current_limit(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int charging_current_limit, ret;

	ret = kstrtoint(buf, 0, &charging_current_limit);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	set_bat_charging_current_limit(charging_current_limit);

	return size;
}

static DEVICE_ATTR(charging_current_limit, S_IRUSR | S_IWUSR,
		   show_charging_current_limit, store_charging_current_limit);

static void init_meter_global_data(struct platform_device *dev)
{
	g_R_BAT_SENSE = p_bat_meter_data->r_bat_sense;
	g_R_I_SENSE = p_bat_meter_data->r_i_sense;
	g_R_CHARGER_1 = p_bat_meter_data->r_charger_1;
	g_R_CHARGER_2 = p_bat_meter_data->r_charger_2;
	g_R_FG_offset = p_bat_meter_data->cust_r_fg_offset;
	g_tracking_point = p_bat_meter_data->cust_tracking_point;
}

static int battery_meter_probe(struct platform_device *dev)
{
	int ret_device_file = 0;
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	char *temp_strptr;
	int cmd_len;
	char chr_mode_str[] = " androidboot.mode=charger";
#endif

	p_bat_meter_data = (struct mt_battery_meter_custom_data *)dev->dev.platform_data;

	if (!p_bat_meter_data) {

#ifdef CONFIG_OF
		mt_bm_of_probe(&dev->dev, &p_bat_meter_data);
#endif
	}

	init_meter_global_data(dev);

	bm_print(BM_LOG_CRTI, "[battery_meter_probe] probe\n");

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT
	    || get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
		cmd_len = strlen(saved_command_line) + strlen(chr_mode_str) + 1;
		temp_strptr = kzalloc(cmd_len, GFP_KERNEL);
		strncpy(temp_strptr, saved_command_line, cmd_len);
		strncat(temp_strptr, chr_mode_str, strlen(chr_mode_str));
		saved_command_line = temp_strptr;
	}
#endif

	/* select battery meter control method */
	battery_meter_ctrl = bm_ctrl_cmd;
	/* LOG System Set */
	init_proc_log_fg();

	/* Create File For FG UI DEBUG */
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Current);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_volt);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_hwocv);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_current);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_zcv);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_temp);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_r);
	/*
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_car);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_qmax);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d0);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d1);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_fg);
	ret_device_file =
	device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_voltmode);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_R_Offset);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_car_tune_value);
	*/
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_charging_current_limit);
#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Battery_Cycle);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Aging_Factor);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Max_Battery_Voltage);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Min_Battery_Voltage);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Max_Battery_Current);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Min_Battery_Current);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Max_Battery_Temperature);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Min_Battery_Temperature);
#endif

	return 0;
}

static int battery_meter_remove(struct platform_device *dev)
{
	bm_print(BM_LOG_CRTI, "[battery_meter_remove]\n");
	return 0;
}

static void battery_meter_shutdown(struct platform_device *dev)
{
	bm_print(BM_LOG_CRTI, "[battery_meter_shutdown]\n");
}

static int battery_meter_suspend(struct platform_device *dev, pm_message_t state)
{
	/* -- hibernation path */
	if (state.event == PM_EVENT_FREEZE) {
		pr_warn("[%s] %p:%p\n", __func__, battery_meter_ctrl, &bm_ctrl_cmd);
		battery_meter_ctrl = bm_ctrl_cmd;
	}
	/* -- end of hibernation path */

#if defined(CONFIG_POWER_EXT)

#elif defined(CONFIG_SOC_BY_SW_FG) || defined(CONFIG_SOC_BY_HW_FG)
	get_monotonic_boottime(&time_before_sleep);
	battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &g_hw_ocv_before_sleep);
#endif

	bm_print(BM_LOG_CRTI, "[battery_meter_suspend]\n");
	return 0;
}

#if defined(CONFIG_MTK_ENABLE_AGING_ALGORITHM) && !defined(CONFIG_POWER_EXT)
static void battery_aging_check(void)
{
	s32 hw_ocv_after_sleep;
	s32 vbat;
	s32 DOD_hwocv;
	s32 DOD_now;
	s32 qmax_aging = 0;
	s32 dod_gap = 10;
	struct timespec time_after_sleep;

	battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &hw_ocv_after_sleep);
	g_fg_dbg_bat_hwocv = hw_ocv_after_sleep;

	vbat = battery_meter_get_battery_voltage();

	bm_print(BM_LOG_CRTI, "@@@ HW_OCV=%d, VBAT=%d\n", hw_ocv_after_sleep, vbat);

	get_monotonic_boottime(&time_after_sleep);

	bm_print(BM_LOG_CRTI, "@@@ suspend_time %lu resume time %lu\n", time_before_sleep.tv_sec,
		 time_after_sleep.tv_sec);

	/* aging */
	if (time_after_sleep.tv_sec - time_before_sleep.tv_sec > OCV_RECOVER_TIME) {
		if (aging_ocv_1 == 0) {
			aging_ocv_1 = hw_ocv_after_sleep;
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &aging_car_1);
			aging_resume_time_1 = time_after_sleep.tv_sec;

			if (fgauge_read_d_by_v(aging_ocv_1) > DOD1_ABOVE_THRESHOLD) {
				aging_ocv_1 = 0;
				bm_print(BM_LOG_CRTI,
					 "[aging check] reset and find next aging_ocv1 for better precision\n");
			}
		} else if (aging_ocv_2 == 0) {
			aging_ocv_2 = hw_ocv_after_sleep;
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &aging_car_2);
			aging_resume_time_2 = time_after_sleep.tv_sec;

			if (fgauge_read_d_by_v(aging_ocv_2) < DOD2_BELOW_THRESHOLD) {
				aging_ocv_2 = 0;
				bm_print(BM_LOG_CRTI,
					 "[aging check] reset and find next aging_ocv2 for better precision\n");
			}
		} else {
			aging_ocv_1 = aging_ocv_2;
			aging_car_1 = aging_car_2;
			aging_resume_time_1 = aging_resume_time_2;

			aging_ocv_2 = hw_ocv_after_sleep;
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &aging_car_2);
			aging_resume_time_2 = time_after_sleep.tv_sec;
		}

		if (aging_ocv_2 > 0) {
			aging_dod_1 = fgauge_read_d_by_v(aging_ocv_1);
			aging_dod_2 = fgauge_read_d_by_v(aging_ocv_2);

			/* TODO: check if current too small to get accurate car */

			/* check dod region to avoid hwocv error margin */
			dod_gap = MIN_DOD_DIFF_THRESHOLD;

			/* check if DOD gap bigger than setting */
			if (aging_dod_2 > aging_dod_1 && (aging_dod_2 - aging_dod_1) >= dod_gap) {
				/* do aging calculation */
				qmax_aging =
				    (100 * (aging_car_1 - aging_car_2)) / (aging_dod_2 -
									   aging_dod_1);

				/* update if aging over 10%. */
				if (gFG_BATT_CAPACITY > qmax_aging
				    && ((gFG_BATT_CAPACITY - qmax_aging) >
					(gFG_BATT_CAPACITY / (100 - MIN_AGING_FACTOR)))) {
					bm_print(BM_LOG_CRTI,
						 "[aging check] before apply aging, qmax_aging(%d) qmax_now(%d) ocv1(%d) dod1(%d) car1(%d) ocv2(%d) dod2(%d) car2(%d)\n",
						 qmax_aging, gFG_BATT_CAPACITY, aging_ocv_1,
						 aging_dod_1, aging_car_1, aging_ocv_2, aging_dod_2,
						 aging_car_2);

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
					gFG_aging_factor = 100 -
					    ((gFG_BATT_CAPACITY -
					      qmax_aging) * 100) / gFG_BATT_CAPACITY;
#endif

					if (gFG_BATT_CAPACITY_aging > qmax_aging) {
						bm_print(BM_LOG_CRTI,
							 "[aging check] new qmax_aging %d old qmax_aging %d\n",
							 qmax_aging, gFG_BATT_CAPACITY_aging);
						gFG_BATT_CAPACITY_aging = qmax_aging;
						gFG_DOD0 = aging_dod_2;
						gFG_DOD1 = gFG_DOD0;
						reset_parameter_car();
					} else {
						bm_print(BM_LOG_CRTI,
							 "[aging check] current qmax_aging %d is smaller than calculated qmax_aging %d\n",
							 gFG_BATT_CAPACITY_aging, qmax_aging);
					}
					aging_ocv_1 = 0;
					aging_ocv_2 = 0;
				} else {
					aging_ocv_2 = 0;
					bm_print(BM_LOG_CRTI,
						 "[aging check] show no degrade, qmax_aging(%d) qmax_now(%d) ocv1(%d) dod1(%d) car1(%d) ocv2(%d) dod2(%d) car2(%d)\n",
						 qmax_aging, gFG_BATT_CAPACITY, aging_ocv_1,
						 aging_dod_1, aging_car_1, aging_ocv_2, aging_dod_2,
						 aging_car_2);
					bm_print(BM_LOG_CRTI,
						 "[aging check] reset and find next aging_ocv2\n");
				}
			} else {
				aging_ocv_2 = 0;
				bm_print(BM_LOG_CRTI,
					 "[aging check] reset and find next aging_ocv2\n");
			}
			bm_print(BM_LOG_CRTI,
				 "[aging check] qmax_aging(%d) qmax_now(%d) ocv1(%d) dod1(%d) car1(%d) ocv2(%d) dod2(%d) car2(%d)\n",
				 qmax_aging, gFG_BATT_CAPACITY, aging_ocv_1, aging_dod_1,
				 aging_car_1, aging_ocv_2, aging_dod_2, aging_car_2);
		}
	}
	/* self-discharging */
	if (time_after_sleep.tv_sec - time_before_sleep.tv_sec > OCV_RECOVER_TIME) {	/* 30 mins */

		DOD_hwocv = fgauge_read_d_by_v(hw_ocv_after_sleep);

		if (DOD_hwocv < DOD1_ABOVE_THRESHOLD || DOD_hwocv > DOD2_BELOW_THRESHOLD) {

			/* update columb counter to get DOD_now. */
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);
			/*DOD_now = 100 - fgauge_read_capacity(1);*/
			DOD_now = 100 - gFG_columb;

			if (DOD_hwocv > DOD_now
			    && (DOD_hwocv - DOD_now > SELF_DISCHARGE_CHECK_THRESHOLD)) {
				gFG_DOD0 = DOD_hwocv;
				gFG_DOD1 = gFG_DOD0;
				reset_parameter_car();
				pr_warn
				    ("[self-discharge check] reset to HWOCV. dod_ocv(%d) dod_now(%d)\n",
				     DOD_hwocv, DOD_now);
			}
			pr_warn("[self-discharge check] dod_ocv(%d) dod_now(%d)\n", DOD_hwocv,
				  DOD_now);
		}
	}
	bm_print(BM_LOG_CRTI, "sleeptime=(%lu)s, be_ocv=(%d), af_ocv=(%d), D0=(%d), car=(%d)\n",
		 time_after_sleep.tv_sec - time_before_sleep.tv_sec, g_hw_ocv_before_sleep,
		 hw_ocv_after_sleep, gFG_DOD0, gFG_columb);
}
#endif

static int battery_meter_resume(struct platform_device *dev)
{
#if defined(CONFIG_POWER_EXT)

#elif defined(CONFIG_SOC_BY_HW_FG)

#ifdef CONFIG_MTK_ENABLE_AGING_ALGORITHM
	if (bat_is_charger_exist() == false)
		battery_aging_check();

#endif

#endif
	bm_print(BM_LOG_CRTI, "[battery_meter_resume]\n");
	return 0;
}

#if 0				/* move to board-common-battery.c */
struct platform_device battery_meter_device = {
	.name = "battery_meter",
	.id = -1,
};
#endif

static struct platform_driver battery_meter_driver = {
	.probe = battery_meter_probe,
	.remove = battery_meter_remove,
	.shutdown = battery_meter_shutdown,
	.suspend = battery_meter_suspend,
	.resume = battery_meter_resume,
	.driver = {
		   .name = "battery_meter",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(mt_battery_meter_id),
#endif
		   },
};

static int __init battery_meter_init(void)
{
	int ret;

#if 0				/* move to board-common-battery.c */
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		bm_print(BM_LOG_CRTI, "[battery_meter_driver] Unable to device register(%d)\n",
			 ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&battery_meter_driver);
	if (ret) {
		bm_print(BM_LOG_CRTI, "[battery_meter_driver] Unable to register driver (%d)\n",
			 ret);
		return ret;
	}

	bm_print(BM_LOG_CRTI, "[battery_meter_driver] Initialization : DONE\n");

	return 0;

}

static void __exit battery_meter_exit(void)
{
}
module_init(battery_meter_init);
module_exit(battery_meter_exit);

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("Battery Meter Device Driver");
MODULE_LICENSE("GPL");
