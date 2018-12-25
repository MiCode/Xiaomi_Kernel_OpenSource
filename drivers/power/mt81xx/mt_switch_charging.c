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

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mutex.h>

#include "mt_battery_common.h"
#include "mt_battery_custom_data.h"
#include "mt_battery_meter.h"
#include "mt_charging.h"
#include <mtk_boot.h>

/* TODO: temp code for usb!!! */

enum usb_state_enum { USB_SUSPEND = 0, USB_UNCONFIGURED, USB_CONFIGURED };

/* ============================================================ // */
/* define */
/* ============================================================ // */
/* cut off to full */
#define POST_CHARGING_TIME (30 * 60) /* 30mins */
#define FULL_CHECK_TIMES 6

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
u32 g_bcct_flag;
u32 g_bcct_value;

int g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
int g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA;
u32 g_usb_state = USB_UNCONFIGURED;
static u32 full_check_count;

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
struct wakeup_source TA_charger_suspend_lock;
bool ta_check_chr_type = true;
bool ta_cable_out_occur;
bool is_ta_connect;
bool ta_vchr_tuning = true;
int ta_v_chr_org;
static DEFINE_MUTEX(ta_mutex);
#endif
/* ////////////////////////////////////////////////
 *     JEITA
 * ////////////////////////////////////////////////
 */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
int g_temp_status = TEMP_POS_10_TO_POS_45;
bool temp_error_recovery_chr_flag = true;
bool trickle_charge_stage;
#endif

/* ============================================================ // */
void BATTERY_SetUSBState(int usb_state_value)
{
#if defined(CONFIG_POWER_EXT)
	pr_debug("[BATTERY_SetUSBState] in FPGA/EVB, no service\r\n");
#else
	if ((usb_state_value < USB_SUSPEND) ||
	    ((usb_state_value > USB_CONFIGURED))) {
		pr_debug("[BATTERY] BAT_SetUSBState Fail! Restore to default value\r\n");
		usb_state_value = USB_UNCONFIGURED;
	} else {
		pr_debug("[BATTERY] BAT_SetUSBState Success! Set %d\r\n",
			    usb_state_value);
		g_usb_state = usb_state_value;
	}
#endif
}

void bat_charger_update_usb_state(int usb_state)
{
	BATTERY_SetUSBState(usb_state);
	wake_up_bat();
}
EXPORT_SYMBOL(bat_charger_update_usb_state);

u32 get_charging_setting_current(void)
{
	return g_temp_CC_value;
}

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
static void set_ta_charging_current(void)
{
	int real_v_chrA = 0;

	real_v_chrA = battery_meter_get_charger_voltage();
	pr_debug("set_ta_charging_current, chrA=%d, chrB=%d\n",
		    ta_v_chr_org, real_v_chrA);

	if ((real_v_chrA - ta_v_chr_org) > 3000) {
		g_temp_input_CC_value =
			p_bat_charging_data
				->ta_ac_9v_input_current; /* TA = 9V */
		g_temp_CC_value = p_bat_charging_data->ta_ac_charging_current;
	} else if ((real_v_chrA - ta_v_chr_org) > 1000) {
		g_temp_input_CC_value =
			p_bat_charging_data
				->ta_ac_7v_input_current; /* TA = 7V */
		g_temp_CC_value = p_bat_charging_data->ta_ac_charging_current;
	}
}

static void mtk_ta_reset_vchr(void)
{
	bat_charger_set_input_current(CHARGE_CURRENT_70_00_MA);
	msleep(250); /* reset Vchr to 5V */

	pr_debug("mtk_ta_reset_vchr(): reset Vchr to 5V\n");
}

static void mtk_ta_increase(void)
{
	if (ta_cable_out_occur == false) {
		bat_charger_set_ta_pattern(true);
	} else {
		ta_check_chr_type = true;
		pr_debug("mtk_ta_increase() Cable out\n");
	}
}

static bool mtk_ta_retry_increase(void)
{
	int real_v_chrA;
	int real_v_chrB;
	bool retransmit = true;
	u32 retransmit_count = 0;

	do {
		real_v_chrA = battery_meter_get_charger_voltage();
		mtk_ta_increase(); /* increase TA voltage to 7V */
		real_v_chrB = battery_meter_get_charger_voltage();

		if (real_v_chrB - real_v_chrA >= 1000) /* 1.0V */
			retransmit = false;
		else {
			retransmit_count++;
			pr_debug("mtk_ta_detector(): retransmit_count =%d, chrA=%d, chrB=%d\n",
				retransmit_count, real_v_chrA, real_v_chrB);
		}

		if ((retransmit_count == 3) ||
		    (BMT_status.charger_exist == false))
			retransmit = false;

	} while ((retransmit == true) && (ta_cable_out_occur == false));

	pr_debug("mtk_ta_retry_increase() real_v_chrA=%d, real_v_chrB=%d, retry=%d\n",
		real_v_chrA, real_v_chrB, retransmit_count);

	if (retransmit_count == 3)
		return false;
	else
		return true;
}

static void mtk_ta_detector(void)
{
	int real_v_chrB = 0;

	pr_debug("mtk_ta_detector() start\n");

	ta_v_chr_org = battery_meter_get_charger_voltage();
	mtk_ta_retry_increase();
	real_v_chrB = battery_meter_get_charger_voltage();

	if (real_v_chrB - ta_v_chr_org >= 1000)
		is_ta_connect = true;
	else
		is_ta_connect = false;

	pr_debug("mtk_ta_detector() end, is_ta_connect=%d\n",
		    is_ta_connect);
}

static void mtk_ta_init(void)
{
	is_ta_connect = false;
	ta_cable_out_occur = false;

	if (p_bat_charging_data->ta_9v_support) {
		ta_vchr_tuning = false;
		p_bat_charging_data->v_charger_max = 10500;
		bat_charger_set_hv_threshold(10500000);
	} else {
		p_bat_charging_data->v_charger_max = 7500;
		bat_charger_set_hv_threshold(7500000);
	}

	bat_charger_init(p_bat_charging_data);
}

static void battery_pump_express_charger_check(void)
{
	if (true == ta_check_chr_type &&
	    BMT_status.charger_type == STANDARD_CHARGER &&
	    BMT_status.SOC >= p_bat_charging_data->ta_start_battery_soc &&
	    BMT_status.SOC < p_bat_charging_data->ta_stop_battery_soc) {

		mutex_lock(&ta_mutex);
		__pm_stay_awake(&TA_charger_suspend_lock);

		mtk_ta_reset_vchr();

		mtk_ta_init();
		mtk_ta_detector();

		/* need to re-check if the charger plug out during ta detector
		 */
		if (true == ta_cable_out_occur)
			ta_check_chr_type = true;
		else
			ta_check_chr_type = false;

		__pm_relax(&TA_charger_suspend_lock);
		mutex_unlock(&ta_mutex);
	} else {
		pr_debug("Stop battery_pump_express_charger_check, SOC=%d, ta_check_chr_type = %d, charger_type = %d\n",
			BMT_status.SOC, ta_check_chr_type,
			BMT_status.charger_type);
	}
}

static void battery_pump_express_algorithm_start(void)
{
	int charger_vol;

	mutex_lock(&ta_mutex);
	__pm_stay_awake(&TA_charger_suspend_lock);

	if (true == is_ta_connect) {
		/* check cable impedance */
		charger_vol = battery_meter_get_charger_voltage();
		if (false == ta_vchr_tuning) {
			mtk_ta_retry_increase(); /* increase TA voltage to 9V */
			charger_vol = battery_meter_get_charger_voltage();
			ta_vchr_tuning = true;
		} else if (BMT_status.SOC >
			   p_bat_charging_data->ta_stop_battery_soc) {
			/* disable charging, avoid Iterm issue */
			bat_charger_enable(false);
			mtk_ta_reset_vchr(); /* decrease TA voltage to 5V */
			charger_vol = battery_meter_get_charger_voltage();
			if (abs(charger_vol - ta_v_chr_org) <= 1000) /* 1.0V */
				is_ta_connect = false;

			pr_debug("Stop battery_pump_express_algorithm, SOC=%d is_ta_connect =%d, TA_STOP_BATTERY_SOC: %d\n",
				BMT_status.SOC, is_ta_connect,
				p_bat_charging_data->ta_stop_battery_soc);
		}
		pr_debug("[BATTERY] check cable impedance, VA(%d) VB(%d) delta(%d).\n",
			ta_v_chr_org, charger_vol, charger_vol - ta_v_chr_org);

		pr_debug("mtk_ta_algorithm() end\n");
	} else
		pr_debug("It's not a TA charger, bypass TA algorithm\n");

	__pm_relax(&TA_charger_suspend_lock);
	mutex_unlock(&ta_mutex);
}
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

static int select_jeita_cv(void)
{
	int cv_voltage;

	if (g_temp_status == TEMP_ABOVE_POS_60)
		cv_voltage =
			p_bat_charging_data->jeita_temp_above_pos_60_cv_voltage;
	else if (g_temp_status == TEMP_POS_45_TO_POS_60)
		cv_voltage = p_bat_charging_data
				     ->jeita_temp_pos_45_to_pos_60_cv_voltage;
	else if (g_temp_status == TEMP_POS_10_TO_POS_45)
		cv_voltage = p_bat_charging_data
				     ->jeita_temp_pos_10_to_pos_45_cv_voltage;
	else if (g_temp_status == TEMP_POS_0_TO_POS_10)
		cv_voltage = p_bat_charging_data
				     ->jeita_temp_pos_0_to_pos_10_cv_voltage;
	else if (g_temp_status == TEMP_NEG_10_TO_POS_0)
		cv_voltage = p_bat_charging_data
				     ->jeita_temp_neg_10_to_pos_0_cv_voltage;
	else if (g_temp_status == TEMP_BELOW_NEG_10)
		cv_voltage =
			p_bat_charging_data->jeita_temp_below_neg_10_cv_voltage;
	else
		cv_voltage = BATTERY_VOLT_04_200000_V;

	if (g_temp_status == TEMP_POS_0_TO_POS_10 &&
	    trickle_charge_stage == true)
		cv_voltage = BATTERY_VOLT_04_200000_V;

	return cv_voltage;
}

int do_jeita_state_machine(void)
{
	int previous_g_temp_status;
	int cv_voltage;

	/* JEITA battery temp Standard */
	previous_g_temp_status = g_temp_status;

	if (BMT_status.temperature >=
	    p_bat_charging_data->temp_pos_60_threshold) {
		pr_debug("[BATTERY] Battery Over high Temperature(%d) !!\n\r",
			p_bat_charging_data->temp_pos_60_threshold);

		g_temp_status = TEMP_ABOVE_POS_60;

		return PMU_STATUS_FAIL;
	} else if (BMT_status.temperature >
		   p_bat_charging_data->temp_pos_45_threshold) {
		if ((g_temp_status == TEMP_ABOVE_POS_60) &&
		    (BMT_status.temperature >=
		     p_bat_charging_data->temp_pos_60_thres_minus_x_degree)) {
			pr_debug("[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				p_bat_charging_data
					->temp_pos_60_thres_minus_x_degree,
				p_bat_charging_data
					->temp_pos_60_threshold);

			return PMU_STATUS_FAIL;
		}
		pr_debug("[BATTERY] Battery Temperature between %d and %d !!\n\r",
			p_bat_charging_data->temp_pos_45_threshold,
			p_bat_charging_data->temp_pos_60_threshold);

		g_temp_status = TEMP_POS_45_TO_POS_60;

	} else if (BMT_status.temperature >=
		   p_bat_charging_data->temp_pos_10_threshold) {
		if (((g_temp_status == TEMP_POS_45_TO_POS_60) &&
		     (BMT_status.temperature >=
		      p_bat_charging_data->temp_pos_45_thres_minus_x_degree)) ||
		    ((g_temp_status == TEMP_POS_0_TO_POS_10) &&
		     (BMT_status.temperature <=
		      p_bat_charging_data->temp_pos_10_thres_plus_x_degree))) {
			pr_debug("[BATTERY] Battery Temperature not recovery to normal temperature charging mode yet!!\n\r");
		} else {
			pr_debug("[BATTERY] Battery Normal Temperature between %d and %d !!\n\r",
				p_bat_charging_data->temp_pos_10_threshold,
				p_bat_charging_data->temp_pos_45_threshold);
			g_temp_status = TEMP_POS_10_TO_POS_45;
		}
	} else if (BMT_status.temperature >=
		   p_bat_charging_data->temp_pos_0_threshold) {
		if ((g_temp_status == TEMP_NEG_10_TO_POS_0 ||
		     g_temp_status == TEMP_BELOW_NEG_10) &&
		    (BMT_status.temperature <=
		     p_bat_charging_data->temp_pos_0_thres_plus_x_degree)) {
			if (g_temp_status == TEMP_BELOW_NEG_10 ||
			    g_temp_status == TEMP_NEG_10_TO_POS_0) {
				pr_debug("[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
					p_bat_charging_data
					->temp_pos_0_threshold,
					p_bat_charging_data
					->temp_pos_0_thres_plus_x_degree);
				return PMU_STATUS_FAIL;
			}
		} else {
			pr_debug("[BATTERY] Battery Temperature between %d and %d !!\n\r",
				p_bat_charging_data->temp_pos_0_threshold,
				p_bat_charging_data->temp_pos_10_threshold);

			g_temp_status = TEMP_POS_0_TO_POS_10;
		}
	} else if (BMT_status.temperature >=
		   p_bat_charging_data->temp_neg_10_threshold) {
		if ((g_temp_status == TEMP_BELOW_NEG_10) &&
		    (BMT_status.temperature <=
		     p_bat_charging_data->temp_neg_10_thres_plus_x_degree)) {
			pr_debug("[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				p_bat_charging_data->temp_neg_10_threshold,
				p_bat_charging_data
					->temp_neg_10_thres_plus_x_degree);

			return PMU_STATUS_FAIL;
		}
		pr_debug("[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
			p_bat_charging_data->temp_neg_10_threshold,
			p_bat_charging_data->temp_pos_0_threshold);

		g_temp_status = TEMP_NEG_10_TO_POS_0;
		return PMU_STATUS_FAIL;

	} else {
		pr_debug("[BATTERY] Battery below low Temperature(%d) !!\n\r",
			p_bat_charging_data->temp_neg_10_threshold);
		g_temp_status = TEMP_BELOW_NEG_10;

		return PMU_STATUS_FAIL;
	}

	/* set CV after temperature changed */
	if (g_temp_status != previous_g_temp_status) {
		cv_voltage = select_jeita_cv();
		bat_charger_set_cv_voltage(cv_voltage);
	}

	return PMU_STATUS_OK;
}

static void set_jeita_charging_current(void)
{
#ifdef CONFIG_CONFIG_USB_IF
	if (BMT_status.charger_type == STANDARD_HOST)
		return;
#endif

	if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
		g_temp_CC_value = CHARGE_CURRENT_350_00_MA;
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		pr_debug("[BATTERY] JEITA set charging current : %d\r\n",
			g_temp_CC_value);
	} else if (g_temp_status == TEMP_POS_0_TO_POS_10) {
		g_temp_CC_value = 51200;
		if (trickle_charge_stage == true)
			g_temp_CC_value = 25600;
	} else if (g_temp_status == TEMP_POS_10_TO_POS_45) {
		if (BMT_status.temperature <= 23)
			g_temp_CC_value = 153600;
		else
			g_temp_CC_value = 211200;
	} else
		g_temp_CC_value = 153600;
}

#endif

void select_charging_curret_bcct(void)
{
	if ((BMT_status.charger_type == STANDARD_HOST) ||
	    (BMT_status.charger_type == NONSTANDARD_CHARGER)) {
		if (g_bcct_value < 100)
			g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA;
		else if (g_bcct_value < 500)
			g_temp_input_CC_value = CHARGE_CURRENT_100_00_MA;
		else if (g_bcct_value < 800)
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		else if (g_bcct_value == 800)
			g_temp_input_CC_value = CHARGE_CURRENT_800_00_MA;
		else
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
	} else if ((BMT_status.charger_type == STANDARD_CHARGER) ||
		   (BMT_status.charger_type == APPLE_1_0A_CHARGER) ||
		   (BMT_status.charger_type == APPLE_2_1A_CHARGER) ||
		   (BMT_status.charger_type == CHARGING_HOST)) {
		g_temp_input_CC_value = CHARGE_CURRENT_MAX;

		/* --------------------------------------------------- */
		/* set IOCHARGE */
		if (g_bcct_value < 550)
			g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
		else if (g_bcct_value < 650)
			g_temp_CC_value = CHARGE_CURRENT_550_00_MA;
		else if (g_bcct_value < 750)
			g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
		else if (g_bcct_value < 850)
			g_temp_CC_value = CHARGE_CURRENT_750_00_MA;
		else if (g_bcct_value < 950)
			g_temp_CC_value = CHARGE_CURRENT_850_00_MA;
		else if (g_bcct_value < 1050)
			g_temp_CC_value = CHARGE_CURRENT_950_00_MA;
		else if (g_bcct_value < 1150)
			g_temp_CC_value = CHARGE_CURRENT_1050_00_MA;
		else if (g_bcct_value < 1250)
			g_temp_CC_value = CHARGE_CURRENT_1150_00_MA;
		else if (g_bcct_value == 1250)
			g_temp_CC_value = CHARGE_CURRENT_1250_00_MA;
		else
			g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
		/* --------------------------------------------------- */

	} else {
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
	}
}

u32 set_bat_charging_current_limit(int current_limit)
{

	if (current_limit != -1) {
		g_bcct_flag = 1;
		g_bcct_value = current_limit * 100;
	} else {
		/* change to default current setting */
		g_bcct_flag = 0;
	}

	return g_bcct_flag;
}

__attribute__((weak)) int mt_usb_pd_get_current(void)
{
	return 0;
}

void select_charging_curret(void)
{
	if (g_ftm_battery_flag) {
		pr_debug("[BATTERY] FTM charging : %d\r\n",
			    charging_level_data[0]);
		g_temp_CC_value = charging_level_data[0];

		if (g_temp_CC_value == CHARGE_CURRENT_450_00_MA) {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_MAX;
			g_temp_CC_value =
				p_bat_charging_data->ac_charger_current;

			pr_debug("[BATTERY] set_ac_current \r\n");
		}
	} else if (g_custom_charging_current != -1) {
		pr_debug("[BATTERY] custom charging : %d\r\n",
			    g_custom_charging_current);
		g_temp_CC_value = g_custom_charging_current;

		if (g_temp_CC_value <= CHARGE_CURRENT_500_00_MA)
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		else
			g_temp_input_CC_value = CHARGE_CURRENT_MAX;

	} else {
		if (BMT_status.charger_type == STANDARD_HOST) {
#ifdef CONFIG_CONFIG_USB_IF
			{
				g_temp_input_CC_value = CHARGE_CURRENT_MAX;
				if (g_usb_state == USB_SUSPEND)
					g_temp_CC_value =
					p_bat_charging_data
					->usb_charger_current_suspend;
				else if (g_usb_state == USB_UNCONFIGURED)
					g_temp_CC_value =
					p_bat_charging_data
					->usb_charger_current_unconfigured;
				else if (g_usb_state == USB_CONFIGURED)
					g_temp_CC_value =
					p_bat_charging_data
					->usb_charger_current_configured;
				else
					g_temp_CC_value =
					p_bat_charging_data
					->usb_charger_current_unconfigured;

				pr_debug("[BATTERY] STANDARD_HOST CC mode charging : %d on %d state\r\n",
					g_temp_CC_value, g_usb_state);
			}
#else
			{
				g_temp_input_CC_value =
					p_bat_charging_data
						->usb_charger_current;
				g_temp_CC_value = p_bat_charging_data
							  ->usb_charger_current;
			}
#endif
		} else if (BMT_status.charger_type == NONSTANDARD_CHARGER) {
			g_temp_input_CC_value =
				p_bat_charging_data->non_std_ac_charger_current;
			g_temp_CC_value =
				p_bat_charging_data->non_std_ac_charger_current;

		} else if (BMT_status.charger_type == STANDARD_CHARGER) {
			g_temp_input_CC_value =
				p_bat_charging_data->ac_charger_input_current;
			g_temp_CC_value =
				p_bat_charging_data->ac_charger_current;
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
			if (is_ta_connect == true)
				set_ta_charging_current();
#endif
		} else if (BMT_status.charger_type == CHARGING_HOST) {
			g_temp_input_CC_value =
				p_bat_charging_data
					->charging_host_charger_current;
			g_temp_CC_value =
				p_bat_charging_data
					->charging_host_charger_current;
		} else if (BMT_status.charger_type == APPLE_2_1A_CHARGER) {
			g_temp_input_CC_value =
				p_bat_charging_data->apple_2_1a_charger_current;
			g_temp_CC_value =
				p_bat_charging_data->apple_2_1a_charger_current;
		} else if (BMT_status.charger_type == APPLE_1_0A_CHARGER) {
			g_temp_input_CC_value =
				p_bat_charging_data->apple_1_0a_charger_current;
			g_temp_CC_value =
				p_bat_charging_data->apple_1_0a_charger_current;
		} else if (BMT_status.charger_type == APPLE_0_5A_CHARGER) {
			g_temp_input_CC_value =
				p_bat_charging_data->apple_0_5a_charger_current;
			g_temp_CC_value =
				p_bat_charging_data->apple_0_5a_charger_current;
		} else if (BMT_status.charger_type == TYPEC_1_5A_CHARGER) {
			g_temp_input_CC_value = 150000;
			g_temp_CC_value =
				p_bat_charging_data->ac_charger_current;
		} else if (BMT_status.charger_type == TYPEC_3A_CHARGER) {
			g_temp_input_CC_value = 280000;
			g_temp_CC_value =
				p_bat_charging_data->ac_charger_current;
		} else if (BMT_status.charger_type == TYPEC_PD_5V_CHARGER) {
			if (mt_usb_pd_get_current())
				g_temp_input_CC_value =
					mt_usb_pd_get_current() * 100;
			else
				g_temp_input_CC_value = 150000;
			g_temp_CC_value =
				p_bat_charging_data->ac_charger_current;
		} else if (BMT_status.charger_type == TYPEC_PD_12V_CHARGER) {
			if (mt_usb_pd_get_current())
				g_temp_input_CC_value =
					mt_usb_pd_get_current() * 100;
			else
				g_temp_input_CC_value = 150000;
			g_temp_CC_value =
				p_bat_charging_data->ac_charger_current;
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
			g_temp_CC_value = CHARGE_CURRENT_500_00_MA;
		}

		pr_debug("[BATTERY] Default CC mode charging : %d, input current = %d\r\n",
			g_temp_CC_value, g_temp_input_CC_value);

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		set_jeita_charging_current();
#endif
	}
}

static bool charging_full_check(void)
{
	bool status = bat_charger_get_charging_status();

	if (status) {
		full_check_count++;
		if (full_check_count >= FULL_CHECK_TIMES)
			return true;
		else
			return false;
	} else
		full_check_count = 0;

	return status;
}

static void pchr_turn_on_charging(void)
{
	int cv_voltage;
	bool charging_enable = true;

	if (BMT_status.bat_charging_state == CHR_ERROR) {
		pr_debug("[BATTERY] Charger Error, turn OFF charging !\n");
		charging_enable = false;
	} else if ((g_platform_boot_mode == META_BOOT) ||
		   (g_platform_boot_mode == ADVMETA_BOOT)) {
		pr_debug("[BATTERY] In meta or advanced meta mode, disable charging.\n");
		charging_enable = false;
	} else {
		/*HW initialization */

		bat_charger_init(p_bat_charging_data);

		pr_debug("charging_hw_init\n");

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
		battery_pump_express_algorithm_start();
#endif
		/* Set Charging Current */
		select_charging_curret();
		pr_debug("[BATTERY] select_charging_curret !\n");

		if (g_bcct_flag == 1) {
			if (g_bcct_value < g_temp_CC_value)
				g_temp_CC_value = g_bcct_value;

			pr_debug("[BATTERY] select_charging_curret_bcct !\n");
		}

		if (g_temp_CC_value == CHARGE_CURRENT_0_00_MA ||
		    g_temp_input_CC_value == CHARGE_CURRENT_0_00_MA) {

			charging_enable = false;

			pr_debug("[BATTERY] charging current is set 0mA, turn off charging !\r\n");
		} else {
			bat_charger_set_input_current(g_temp_input_CC_value);
			bat_charger_set_current(g_temp_CC_value);

/*Set CV Voltage */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
			cv_voltage = p_bat_charging_data->battery_cv_voltage;
#else
			cv_voltage = select_jeita_cv();
#endif
			bat_charger_set_cv_voltage(cv_voltage);
		}
	}

	/* enable/disable charging */
	bat_charger_enable(charging_enable);

	pr_debug("[BATTERY] pchr_turn_on_charging(), enable =%d !\r\n",
		charging_enable);
}

int BAT_PreChargeModeAction(void)
{
	pr_debug("[BATTERY] Pre-CC mode charge, timer=%d on %d !!\n\r",
		BMT_status.PRE_charging_time,
		BMT_status.total_charging_time);

	BMT_status.PRE_charging_time += BAT_TASK_PERIOD;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	trickle_charge_stage = false;
#endif

	/*  Enable charger */
	pchr_turn_on_charging();

	if (BMT_status.UI_SOC == 100) {
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
	} else if (BMT_status.bat_vol > p_bat_charging_data->v_pre2cc_thres) {
		BMT_status.bat_charging_state = CHR_CC;
	}

	return PMU_STATUS_OK;
}

int BAT_ConstantCurrentModeAction(void)
{
	pr_debug("[BATTERY] CC mode charge, timer=%d on %d !!\n\r",
		BMT_status.CC_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time += BAT_TASK_PERIOD;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	/*  Enable charger */
	pchr_turn_on_charging();

	if (charging_full_check() == true) {

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		if (g_temp_status == TEMP_POS_0_TO_POS_10) {
			if (trickle_charge_stage == false) {
				trickle_charge_stage = true;
				full_check_count = 0;
				return PMU_STATUS_OK;
			}
		} else
			trickle_charge_stage = false;
#endif
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;
	}

	return PMU_STATUS_OK;
}

int BAT_BatteryFullAction(void)
{
	pr_debug("[BATTERY] Battery full !!\n\r");

	BMT_status.bat_full = true;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.bat_in_recharging_state = false;

	/* still config charger at full charge status */
	pchr_turn_on_charging();

	if (charging_full_check() == false) {
		pr_debug("[BATTERY] Battery Re-charging !!\n\r");

		if (BMT_status.SOC < 100) {
			pr_debug("[BATTERY] Leave Re-charging !!\n\r");
			BMT_status.bat_in_recharging_state = false;
			BMT_status.bat_charging_state = CHR_CC;
			BMT_status.bat_full = false;
			return PMU_STATUS_OK;
		}

		BMT_status.bat_in_recharging_state = true;
		BMT_status.bat_charging_state = CHR_BATFULL;
	}

	return PMU_STATUS_OK;
}

int BAT_BatteryHoldAction(void)
{
	pr_debug("[BATTERY] Hold mode !!\n\r");

	if (BMT_status.bat_vol <
		p_bat_charging_data->talking_recharge_voltage ||
	    g_call_state == CALL_IDLE) {
		BMT_status.bat_charging_state = CHR_CC;
		pr_debug("[BATTERY] Exit Hold mode and Enter CC mode !!\n\r");
	}

	bat_charger_enable(false);

	return PMU_STATUS_OK;
}

int BAT_BatteryCmdHoldAction(void)
{
	pr_debug("[BATTERY] Cmd Hold mode !!\n\r");

	BMT_status.bat_full = false;
	BMT_status.bat_in_recharging_state = false;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;

	/*  Disable charger */
	bat_charger_enable(false);
	bat_charger_enable_power_path(false);

	return PMU_STATUS_OK;
}

int BAT_BatteryStatusFailAction(void)
{
	pr_debug("[BATTERY] BAD Battery status... Charging Stop !!\n\r");

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if ((g_temp_status == TEMP_ABOVE_POS_60) ||
	    (g_temp_status == TEMP_BELOW_NEG_10) ||
	    (g_temp_status == TEMP_NEG_10_TO_POS_0))
		temp_error_recovery_chr_flag = false;

	if ((temp_error_recovery_chr_flag == false) &&
	    (g_temp_status != TEMP_ABOVE_POS_60) &&
	    (g_temp_status != TEMP_NEG_10_TO_POS_0) &&
	    (g_temp_status != TEMP_BELOW_NEG_10)) {

		temp_error_recovery_chr_flag = true;
		BMT_status.bat_charging_state = CHR_PRE;
	}
#endif

	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;

	/*  Disable charger */
	bat_charger_enable(false);

	return PMU_STATUS_OK;
}

void mt_battery_charging_algorithm(void)
{
	bat_charger_reset_watchdog_timer();

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	battery_pump_express_charger_check();
#endif
	switch (BMT_status.bat_charging_state) {
	case CHR_PRE:
		BAT_PreChargeModeAction();
		break;

	case CHR_CC:
		BAT_ConstantCurrentModeAction();
		break;

	case CHR_BATFULL:
		BAT_BatteryFullAction();
		break;

	case CHR_HOLD:
		BAT_BatteryHoldAction();
		break;

	case CHR_CMD_HOLD:
		BAT_BatteryCmdHoldAction();
		break;

	case CHR_ERROR:
		BAT_BatteryStatusFailAction();
		break;
	}

	bat_charger_dump_register();
}
