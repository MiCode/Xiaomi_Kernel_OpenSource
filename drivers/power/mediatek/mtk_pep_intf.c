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

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include "mtk_pep20_intf.h"
#include "mtk_pep_intf.h"

#if !defined(TA_AC_CHARGING_CURRENT)
#include <mach/mt_pe.h>
#endif


#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT
static struct mutex pep_access_lock;
static struct mutex pep_pmic_sync_lock;
static struct wake_lock pep_suspend_lock;
static int pep_ta_vchr_org = 5000; /* mA */
static bool pep_to_check_chr_type = true;
static bool pep_to_tune_ta_vchr = true;
static bool pep_is_cable_out_occur; /* Plug out happend while detect PE+ */
static bool pep_is_connect;
static bool pep_is_enabled = true;


static int pep_enable_hw_vbus_ovp(bool enable)
{
	int ret = 0;
	u32 data;

	data = (enable ? 1 : 0); /* Compatible with charging_hw_bq25896.c */
	ret = battery_charging_control(CHARGING_CMD_SET_VBUS_OVP_EN, &data);
	if (ret < 0)
		battery_log(BAT_LOG_CRTI, "%s: failed, ret = %d\n",
			__func__, ret);
	return ret;
}

/* Enable/Disable HW & SW VBUS OVP */
static int pep_enable_vbus_ovp(bool enable)
{
	int ret = 0;
	u32 sw_ovp = (enable ? V_CHARGER_MAX : 15000);

	/* Enable/Disable HW(PMIC) OVP */
	ret = pep_enable_hw_vbus_ovp(enable);
	if (ret < 0) {
		battery_log(BAT_LOG_CRTI, "%s: failed, ret = %d\n",
			__func__, ret);
		return ret;
	}

	/* Enable/Disable SW OVP status */
	batt_cust_data.v_charger_max = sw_ovp;

	return ret;
}

static int pep_enable_charging(bool enable)
{
	int ret = 0;
	u32 data;

	data = (enable ? 1 : 0); /* Compatible with charging_hw_bq25896.c */
	ret = battery_charging_control(CHARGING_CMD_ENABLE, &enable);
	if (ret < 0)
		battery_log(BAT_LOG_CRTI, "%s: failed, ret = %d\n",
			__func__, ret);
	return ret;
}

static int pep_set_mivr(u32 mivr)
{
	int ret = 0;

	ret = battery_charging_control(CHARGING_CMD_SET_VINDPM, &mivr);
	if (ret < 0)
		battery_log(BAT_LOG_CRTI, "%s: failed, ret = %d\n",
			__func__, ret);
	return ret;
}


static int pep_leave(bool disable_charging)
{
	int ret = 0;

	battery_log(BAT_LOG_FULL, "%s: starts\n", __func__);

	/* CV point reached, disable charger */
	ret = pep_enable_charging(disable_charging);
	if (ret < 0)
		goto _err;

	/* Decrease TA voltage to 5V */
	ret = mtk_pep_reset_ta_vchr();
	if (ret < 0 || pep_is_connect)
		goto _err;

	battery_log(BAT_LOG_CRTI, "%s: OK\n", __func__);
	return ret;

_err:
	battery_log(BAT_LOG_CRTI, "%s: failed, is_connect = %d, ret = %d\n",
		__func__, pep_is_connect, ret);
	return ret;
}

static int pep_check_leave_status(void)
{
	int ret = 0;
	u32 ichg = 0, vchr = 0;
	kal_bool current_sign;

	battery_log(BAT_LOG_FULL, "%s: starts\n", __func__);

	/* PE+ leaves unexpectedly */
	vchr = battery_meter_get_charger_voltage();
	if (abs(vchr - pep_ta_vchr_org) < 1000) {
		battery_log(BAT_LOG_CRTI,
			"%s: PE+ leave unexpectedly, recheck TA\n", __func__);
		pep_to_check_chr_type = true;
		ret = pep_leave(true);
		if (ret < 0 || pep_is_connect)
			goto _err;

		return ret;
	}

	ichg = battery_meter_get_battery_current(); /* 0.1 mA */
	ichg /= 10; /* mA */
	current_sign = battery_meter_get_battery_current_sign();

	/* Check SOC & Ichg */
	if (BMT_status.SOC > batt_cust_data.ta_stop_battery_soc &&
	    current_sign && ichg < 1000) {
		ret = pep_leave(true);
		if (ret < 0 || pep_is_connect)
			goto _err;
		battery_log(BAT_LOG_CRTI,
			"%s: OK, SOC = (%d,%d), Ichg = %dmA, stop PE+\n",
			__func__, BMT_status.SOC, batt_cust_data.ta_stop_battery_soc,
			ichg);
	}

	return ret;

_err:
	battery_log(BAT_LOG_CRTI, "%s: failed, is_connect = %d, ret = %d\n",
		__func__, pep_is_connect, ret);
	return ret;
}

static int __pep_increase_ta_vchr(void)
{
	int ret = 0;
	bool increase = true; /* Increase */
	kal_bool data; /* Use kal_bool, compatible with charging_hw_bq25896.c */

	data = (increase ? KAL_TRUE : KAL_FALSE);
	if (BMT_status.charger_exist) {
		ret = battery_charging_control(
			CHARGING_CMD_SET_TA_CURRENT_PATTERN, &data);
		if (ret < 0)
			battery_log(BAT_LOG_CRTI, "%s: failed, ret = %d\n",
				__func__, ret);
		else
			battery_log(BAT_LOG_CRTI, "%s: OK\n", __func__);
		return ret;
	}

	/* TA is not exist */
	ret = -EIO;
	battery_log(BAT_LOG_CRTI, "%s: failed, cable out\n", __func__);
	return ret;
}

static int pep_increase_ta_vchr(u32 vchr_target)
{
	int ret = 0;
	int vchr_before, vchr_after;
	u32 retry_cnt = 0;

	do {
		vchr_before = battery_meter_get_charger_voltage();
		__pep_increase_ta_vchr();
		vchr_after = battery_meter_get_charger_voltage();

		if (abs(vchr_after - vchr_target) <= 1000) {
			battery_log(BAT_LOG_CRTI, "%s: OK\n", __func__);
			return ret;
		}
		battery_log(BAT_LOG_CRTI,
			"%s: retry, cnt = %d, vchr = (%d, %d), vchr_target = %d\n",
			__func__, retry_cnt, vchr_before, vchr_after, vchr_target);

		retry_cnt++;
	} while (BMT_status.charger_exist && retry_cnt < 3);

	ret = -EIO;
	battery_log(BAT_LOG_CRTI,
		"%s: failed, vchr = (%d, %d), vchr_target = %d\n",
		__func__, vchr_before, vchr_after, vchr_target);

	return ret;
}

static int pep_detect_ta(void)
{
	int ret = 0;

	battery_log(BAT_LOG_FULL, "%s: starts\n", __func__);

	/* Disable OVP */
	ret = pep_enable_vbus_ovp(false);
	if (ret < 0)
		goto _err;

	pep_ta_vchr_org = battery_meter_get_charger_voltage();
	ret = pep_increase_ta_vchr(7000); /* mA */

	if (ret == 0) {
		pep_is_connect = true;
		battery_log(BAT_LOG_CRTI, "%s: OK, is_connect = %d\n",
			__func__, pep_is_connect);
		return ret;
	}

	/* Detect PE+ TA failed */
	pep_is_connect = false;
	pep_to_check_chr_type = false;

	/* Enable OVP */
	pep_enable_vbus_ovp(true);

	/* Set MIVR to 4.5V for vbus 5V */
	pep_set_mivr(4500);

_err:
	battery_log(BAT_LOG_CRTI, "%s: failed, is_connect = %d\n",
		__func__, pep_is_connect);

	return ret;
}

static int pep_init_ta(void)
{
	int ret = 0;

	battery_log(BAT_LOG_FULL, "%s: starts\n", __func__);
	if (batt_cust_data.ta_9v_support || batt_cust_data.ta_12v_support)
		pep_to_tune_ta_vchr = true;

	ret = battery_charging_control(CHARGING_CMD_INIT, NULL);
	if (ret < 0)
		battery_log(BAT_LOG_CRTI, "%s failed, ret = %d\n",
			__func__, ret);
	else
		battery_log(BAT_LOG_CRTI, "%s OK\n", __func__);

	return ret;
}

static int pep_plugout_reset(void)
{
	int ret = 0;

	battery_log(BAT_LOG_FULL, "%s: starts\n", __func__);

	pep_to_check_chr_type = true;

	ret = mtk_pep_reset_ta_vchr();
	if (ret < 0)
		goto _err;

	/* Set cable out occur to false */
	mtk_pep_set_is_cable_out_occur(false);
	battery_log(BAT_LOG_CRTI, "%s: OK\n", __func__);
	return ret;

_err:
	battery_log(BAT_LOG_CRTI, "%s: failed, ret = %d\n", __func__, ret);

	return ret;
}

int mtk_pep_init(void)
{
	wake_lock_init(&pep_suspend_lock, WAKE_LOCK_SUSPEND,
		"PE+ TA charger suspend wakelock");
	mutex_init(&pep_access_lock);
	mutex_init(&pep_pmic_sync_lock);
	return 0;
}

int mtk_pep_reset_ta_vchr(void)
{
	int ret = 0, chr_volt = 0;
	u32 retry_cnt = 0;
	CHR_CURRENT_ENUM aicr;

	battery_log(BAT_LOG_FULL, "%s: starts\n", __func__);

	/* Set aicr to 70 mA */
	aicr = CHARGE_CURRENT_70_00_MA;

	do {
		ret = battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
			&aicr);

		msleep(500);
		/* Check charger's voltage */
		chr_volt = battery_meter_get_charger_voltage();
		if (abs(chr_volt - pep_ta_vchr_org) <= 1000) {
			pep_is_connect = false;
			break;
		}

		retry_cnt++;
	} while (retry_cnt < 3);

	if (pep_is_connect) {
		battery_log(BAT_LOG_CRTI, "%s: failed, ret = %d\n",
			__func__, ret);
		/*
		 * SET_INPUT_CURRENT success but chr_volt does not reset to 5V
		 * set ret = -EIO to represent the case
		 */
		ret = -EIO;
		return ret;
	}

	/* Enable OVP */
	ret = pep_enable_vbus_ovp(true);
	pep_set_mivr(4500);
	battery_log(BAT_LOG_CRTI, "%s: OK\n", __func__);

	return ret;
}


int mtk_pep_check_charger(void)
{
	int ret = 0;

	if (mtk_pep20_get_is_connect()) {
		battery_log(BAT_LOG_CRTI, "%s: stop, PE+20 is connected\n",
			__func__);
		return ret;
	}

	if (!pep_is_enabled) {
		battery_log(BAT_LOG_CRTI, "%s: stop, PE+ is disabled\n",
			__func__);
		return ret;
	}

	/* Lock */
	mutex_lock(&pep_access_lock);
	wake_lock(&pep_suspend_lock);

	battery_log(BAT_LOG_FULL, "%s: starts\n", __func__);

	if (!BMT_status.charger_exist || pep_is_cable_out_occur)
		pep_plugout_reset();

	/* Not to check charger type or
	 * Not standard charger or
	 * SOC is not in range
	 */
	if (!pep_to_check_chr_type ||
	    BMT_status.charger_type != STANDARD_CHARGER ||
	    BMT_status.SOC < batt_cust_data.ta_start_battery_soc ||
	    BMT_status.SOC >= batt_cust_data.ta_stop_battery_soc)
		goto _err;

	/* Reset/Init/Detect TA */
	ret = mtk_pep_reset_ta_vchr();
	if (ret < 0)
		goto _err;

	ret = pep_init_ta();
	if (ret < 0)
		goto _err;

	ret = pep_detect_ta();
	if (ret < 0)
		goto _err;

	pep_to_check_chr_type = false;

	/* Unlock */
	wake_unlock(&pep_suspend_lock);
	mutex_unlock(&pep_access_lock);
	battery_log(BAT_LOG_CRTI, "%s: OK, to_check_chr_type = %d\n",
			__func__, pep_to_check_chr_type);
	return ret;

_err:
	/* Unlock */
	wake_unlock(&pep_suspend_lock);
	mutex_unlock(&pep_access_lock);
	battery_log(BAT_LOG_CRTI,
		"%s: stop, SOC = %d, to_check_chr_type = %d, chr_type = %d, ret = %d\n",
		__func__, BMT_status.SOC, pep_to_check_chr_type, BMT_status.charger_type, ret);
	return ret;
}

int mtk_pep_start_algorithm(void)
{
	int ret = 0, chr_volt;

	if (mtk_pep20_get_is_connect()) {
		battery_log(BAT_LOG_CRTI, "%s: stop, PE+20 is connected\n",
			__func__);
		return ret;
	}

	if (!pep_is_enabled) {
		battery_log(BAT_LOG_CRTI, "%s: stop, PE+ is disabled\n",
			__func__);
		return ret;
	}

	/* Lock */
	mutex_lock(&pep_access_lock);
	wake_lock(&pep_suspend_lock);

	battery_log(BAT_LOG_FULL, "%s: starts\n", __func__);

	if (!BMT_status.charger_exist || pep_is_cable_out_occur)
		pep_plugout_reset();

	/* TA is not connected */
	if (!pep_is_connect) {
		ret = -EIO;
		battery_log(BAT_LOG_CRTI, "%s: stop, PE+ is not connected\n",
			__func__);
		goto _out;
	}

	/* No need to tune TA */
	if (!pep_to_tune_ta_vchr) {
		ret = pep_check_leave_status();
		battery_log(BAT_LOG_CRTI, "%s: stop, not to tune TA vchr\n",
			__func__);
		goto _out;
	}

	pep_to_tune_ta_vchr = false;

	/* Increase TA voltage to 9V */
	if (batt_cust_data.ta_9v_support || batt_cust_data.ta_12v_support) {
		ret = pep_increase_ta_vchr(9000); /* mA */
		if (ret < 0) {
			battery_log(BAT_LOG_CRTI,
				"%s: failed, cannot increase to 9V\n",
				__func__);
			goto _err;
		}

		/* Successfully, increase to 9V */
		battery_log(BAT_LOG_CRTI, "%s: output 9V ok\n", __func__);
	}

	/* Increase TA voltage to 12V */
	if (batt_cust_data.ta_12v_support) {
		ret = pep_increase_ta_vchr(12000); /* mA */
		if (ret < 0) {
			battery_log(BAT_LOG_CRTI,
				"%s: failed, cannot increase to 12V\n",
				__func__);
			goto _err;
		}

		/* Successfully, increase to 12V */
		battery_log(BAT_LOG_CRTI, "%s: output 12V ok\n", __func__);
	}

	chr_volt = battery_meter_get_charger_voltage();
	ret = pep_set_mivr(chr_volt - 1000);
	if (ret < 0)
		goto _err;

	battery_log(BAT_LOG_CRTI, "%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
		__func__, pep_ta_vchr_org, chr_volt, chr_volt - pep_ta_vchr_org);
	battery_log(BAT_LOG_CRTI, "%s: OK\n", __func__);

	wake_unlock(&pep_suspend_lock);
	mutex_unlock(&pep_access_lock);
	return ret;

_err:
	pep_leave(false);
_out:
	chr_volt = battery_meter_get_charger_voltage();
	battery_log(BAT_LOG_CRTI, "%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
		__func__, pep_ta_vchr_org, chr_volt, chr_volt - pep_ta_vchr_org);

	wake_unlock(&pep_suspend_lock);
	mutex_unlock(&pep_access_lock);

	return ret;
}

int mtk_pep_set_charging_current(CHR_CURRENT_ENUM *ichg, CHR_CURRENT_ENUM *aicr)
{
	int ret = 0, chr_volt = 0;

	if (!pep_is_connect)
		return -ENOTSUPP;

	chr_volt = battery_meter_get_charger_voltage();
	if ((chr_volt - pep_ta_vchr_org) > 6000) { /* TA = 12V */
		*aicr = batt_cust_data.ta_ac_12v_input_current;
		*ichg = batt_cust_data.ta_ac_charging_current;
	} else if ((chr_volt - pep_ta_vchr_org) > 3000) { /* TA = 9V */
		*aicr = batt_cust_data.ta_ac_9v_input_current;
		*ichg = batt_cust_data.ta_ac_charging_current;
	} else if ((chr_volt - pep_ta_vchr_org) > 1000) { /* TA = 7V */
		*aicr = batt_cust_data.ta_ac_7v_input_current;
		*ichg = batt_cust_data.ta_ac_charging_current;
	}

	battery_log(BAT_LOG_CRTI,
		"%s: Ichg= %dmA, AICR = %dmA, chr_org = %d, chr_after = %d\n",
		__func__, *ichg / 100, *aicr / 100, pep_ta_vchr_org, chr_volt);
	return ret;
}

/* PE+ set functions */

void mtk_pep_set_to_check_chr_type(bool check)
{
	mutex_lock(&pep_access_lock);
	wake_lock(&pep_suspend_lock);

	battery_log(BAT_LOG_CRTI, "%s: check = %d\n", __func__, check);
	pep_to_check_chr_type = check;

	wake_unlock(&pep_suspend_lock);
	mutex_unlock(&pep_access_lock);
}


void mtk_pep_set_is_enable(bool enable)
{
	mutex_lock(&pep_access_lock);
	wake_lock(&pep_suspend_lock);

	battery_log(BAT_LOG_CRTI, "%s: enable = %d\n", __func__, enable);
	pep_is_enabled = enable;

	wake_unlock(&pep_suspend_lock);
	mutex_unlock(&pep_access_lock);
}

void mtk_pep_set_is_cable_out_occur(bool out)
{
	battery_log(BAT_LOG_CRTI, "%s: out = %d\n", __func__, out);
	mutex_lock(&pep_pmic_sync_lock);
	pep_is_cable_out_occur = out;
	mutex_unlock(&pep_pmic_sync_lock);
}

/* PE+ get functions */

bool mtk_pep_get_to_check_chr_type(void)
{
	return pep_to_check_chr_type;
}

bool mtk_pep_get_is_connect(void)
{
	/*
	 * Cable out is occurred,
	 * but not execute plugout_reset yet
	 */
	if (pep_is_cable_out_occur)
		return false;

	return pep_is_connect;
}


bool mtk_pep_get_is_enable(void)
{
	return pep_is_enabled;
}
#endif
