/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _BATTERY_CUSTOM_DATA_H
#define _BATTERY_CUSTOM_DATA_H

#include <linux/device.h>

struct BATTERY_PROFILE_STRUCT {
	s32 percentage;
	s32 voltage;
};

struct R_PROFILE_STRUCT {
	s32 resistance; /* Ohm */
	s32 voltage;
};

struct BATTERY_CYCLE_STRUCT {
	s32 cycle;
	s32 aging_factor;
};

struct BATT_TEMPERATURE {
	s32 BatteryTemp;
	s32 TemperatureR;
};

struct mt_battery_meter_custom_data {
	/* ADC Channel Number */
	int cust_tabt_number;
	int vbat_channel_number;
	int isense_channel_number;
	int vcharger_channel_number;
	int vbattemp_channel_number;

	/*ADC resistor  */
	int r_bat_sense;
	int r_i_sense;
	int r_charger_1;
	int r_charger_2;

	int temperature_t0;
	int tempearture_t1;
	int temperature_t1_5;
	int temperature_t2;
	int temperature_t3;
	int temperature_t;

	int fg_meter_resistance;

	int q_max_pos_50;
	int q_max_pos_25;
	int q_max_pos_10;
	int q_max_pos_0;
	int q_max_neg_10;

	int q_max_pos_50_h_current;
	int q_max_pos_25_h_current;
	int q_max_pos_10_h_current;
	int q_max_pos_0_h_current;
	int q_max_neg_10_h_current;

	/* Discharge Percentage */
	int oam_d5;

	int cust_tracking_point;
	int cust_r_sense;
	int cust_hw_cc;
	int aging_tuning_value;
	int cust_r_fg_offset;

	int ocv_board_compesate; /* mV */
	int r_fg_board_base;
	int r_fg_board_slope;
	int car_tune_value;

	/* HW Fuel gague  */
	int current_detect_r_fg;
	int min_error_offset;
	int fg_vbat_average_size;
	int r_fg_value;

	int poweron_delta_capacity_tolerance;
	int poweron_low_capacity_tolerance;
	int poweron_max_vbat_tolerance;
	int poweron_delta_vbat_tolerance;

	int vbat_normal_wakeup;
	int vbat_low_power_wakeup;
	int normal_wakeup_period;
	int low_power_wakeup_period;
	int close_poweroff_wakeup_period;

	/* ocv2cv transform */
	int enable_ocv2cv_trans;
	int step_of_qmax; /*mAh*/
	int cv_current;   /*0.1mA*/

	/* meter table */
	int rbat_pull_up_r;
	int rbat_pull_down_r;
	int rbat_pull_up_volt;

	int battery_profile_saddles;
	int battery_r_profile_saddles;
	int battery_aging_table_saddles;
	int battery_ntc_table_saddles;
	void *p_batt_temperature_table;
	void *p_battery_profile_t0;
	void *p_battery_profile_t1;
	void *p_battery_profile_t1_5;
	void *p_battery_profile_t2;
	void *p_battery_profile_t3;
	void *p_r_profile_t0;
	void *p_r_profile_t1;
	void *p_r_profile_t1_5;
	void *p_r_profile_t2;
	void *p_r_profile_t3;
	void *p_battery_profile_temperature;
	void *p_r_profile_temperature;
	void *p_battery_aging_table;
};

struct mt_battery_charging_custom_data {
	int talking_recharge_voltage;
	int talking_sync_time;

	/* Battery Temperature Protection */
	int max_discharge_temperature;
	int min_discharge_temperature;
	int max_charge_temperature;
	int min_charge_temperature;
	int err_charge_temperature;
	int use_avg_temperature;

	/* Linear Charging Threshold */
	int v_pre2cc_thres;
	int v_cc2topoff_thres;
	int recharging_voltage;
	int charging_full_current;

	/* CONFIG_USB_IF */
	int usb_charger_current_suspend;
	int usb_charger_current_unconfigured;
	int usb_charger_current_configured;

	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int non_std_ac_charger_current;
	int charging_host_charger_current;
	int apple_0_5a_charger_current;
	int apple_1_0a_charger_current;
	int apple_2_1a_charger_current;

	/* Charger error check */
	/* BAT_LOW_TEMP_PROTECT_ENABLE */
	int v_charger_enable;
	int v_charger_max;
	int v_charger_min;

	/* Tracking time */
	int onehundred_percent_tracking_time;
	int npercent_tracking_time;
	int sync_to_real_tracking_time;

	/* Battery voltage */
	int battery_cv_voltage;

	/* JEITA parameter */
	int cust_soc_jeita_sync_time;
	int jeita_recharge_voltage;
	int jeita_temp_above_pos_60_cv_voltage;
	int jeita_temp_pos_45_to_pos_60_cv_voltage;
	int jeita_temp_pos_10_to_pos_45_cv_voltage;
	int jeita_temp_pos_0_to_pos_10_cv_voltage;
	int jeita_temp_neg_10_to_pos_0_cv_voltage;
	int jeita_temp_below_neg_10_cv_voltage;

	int temp_pos_60_threshold;
	int temp_pos_60_thres_minus_x_degree;
	int temp_pos_45_threshold;
	int temp_pos_45_thres_minus_x_degree;
	int temp_pos_10_threshold;
	int temp_pos_10_thres_plus_x_degree;
	int temp_pos_0_threshold;
	int temp_pos_0_thres_plus_x_degree;
	int temp_neg_10_threshold;
	int temp_neg_10_thres_plus_x_degree;

	/* For JEITA Linear Charging Only */
	int jeita_neg_10_to_pos_0_full_current;
	int jeita_temp_pos_45_to_pos_60_recharge_voltage;
	int jeita_temp_pos_10_to_pos_45_recharge_voltage;
	int jeita_temp_pos_0_to_pos_10_recharge_voltage;
	int jeita_temp_neg_10_to_pos_0_recharge_voltage;
	int jeita_temp_pos_45_to_pos_60_cc2topoff_threshold;
	int jeita_temp_pos_10_to_pos_45_cc2topoff_threshold;
	int jeita_temp_pos_0_to_pos_10_cc2topoff_threshold;
	int jeita_temp_neg_10_to_pos_0_cc2topoff_threshold;

	/* For charger IC GPIO config */
	int charger_enable_pin;
	int charger_otg_pin;

	/* for Pump Expresss Plus */
	int ta_start_battery_soc;
	int ta_stop_battery_soc;
	int ta_ac_9v_input_current;
	int ta_ac_7v_input_current;
	int ta_ac_charging_current;
	int ta_9v_support;
};

extern int mt_bm_of_probe(struct device *dev,
			  struct mt_battery_meter_custom_data **p_meter_data);

#endif /* #ifndef _BATTERY_CUSTOM_DATA_H */
