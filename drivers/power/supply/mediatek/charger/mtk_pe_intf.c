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


#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#include <upmu_common.h>
#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"

/* Unit of the following functions are uV, uA */
static inline u32 pe_get_vbus(void)
{
	return battery_get_vbus() * 1000;
}

static inline u32 pe_get_ibat(void)
{
	return battery_get_bat_current() * 100;
}

static bool cancel_pe(struct charger_manager *pinfo)
{
	if (mtk_pdc_check_charger(pinfo) || mtk_is_TA_support_pd_pps(pinfo))
		return true;
	return false;
}

/* Enable/Disable HW & SW VBUS OVP */
static int pe_enable_vbus_ovp(struct charger_manager *pinfo, bool enable)
{
	int ret = 0;

	/* Enable/Disable HW(PMIC) OVP */
	ret = pmic_enable_hw_vbus_ovp(enable);
	if (ret < 0) {
		chr_err("%s: failed, ret = %d\n", __func__, ret);
		return ret;
	}

	charger_enable_vbus_ovp(pinfo, enable);

	return ret;
}

static int pe_enable_charging(struct charger_manager *pinfo, bool enable)
{
	int ret = 0;

	ret = charger_dev_enable(pinfo->chg1_dev, enable);
	if (ret < 0)
		chr_err("%s: failed, ret = %d\n", __func__, ret);
	return ret;
}

static int pe_set_mivr(struct charger_manager *pinfo, int uV)
{
	int ret = 0;
	bool chg2_chip_enabled = false;

	ret = charger_dev_set_mivr(pinfo->chg1_dev, uV);
	if (ret < 0)
		chr_err("%s: failed, ret = %d\n", __func__, ret);

	if (pinfo->chg2_dev) {
		charger_dev_is_chip_enabled(pinfo->chg2_dev,
			&chg2_chip_enabled);
		if (chg2_chip_enabled) {
			ret = charger_dev_set_mivr(pinfo->chg2_dev,
				uV + pinfo->data.slave_mivr_diff);
			if (ret < 0)
				pr_info("%s: chg2 failed, ret = %d\n", __func__,
					ret);
		}
	}

	return ret;
}

static int pe_leave(struct charger_manager *pinfo, bool disable_charging)
{
	int ret = 0;
	struct mtk_pe *pe = &pinfo->pe;

	chr_debug("%s: starts\n", __func__);

	/* CV point reached, disable charger */
	ret = pe_enable_charging(pinfo, disable_charging);
	if (ret < 0)
		goto _err;

	/* Decrease TA voltage to 5V */
	ret = mtk_pe_reset_ta_vchr(pinfo);
	if (ret < 0 || pe->is_connect)
		goto _err;

	chr_info("%s: OK\n", __func__);
	return ret;

_err:
	chr_err("%s: failed, is_connect = %d, ret = %d\n",
		__func__, pe->is_connect, ret);
	return ret;
}

static int pe_check_leave_status(struct charger_manager *pinfo)
{
	int ret = 0;
	u32 ichg = 0, vchr = 0;
	struct mtk_pe *pe = &pinfo->pe;

	chr_debug("%s: starts\n", __func__);

	/* PE+ leaves unexpectedly */
	vchr = pe_get_vbus();
	if (abs(vchr - pe->ta_vchr_org) < 1000000) {
		chr_err("%s: PE+ leave unexpectedly, recheck TA vbus:%d vchg_org:%d !\n",
			__func__, vchr, pe->ta_vchr_org);
		pe->to_check_chr_type = true;
		ret = pe_leave(pinfo, true);
		if (ret < 0 || pe->is_connect)
			goto _err;

		return ret;
	}

	ichg = pe_get_ibat();

	/* Check SOC & Ichg */
	if (pinfo->data.ta_stop_battery_soc < battery_get_soc() &&
	    ichg > 0 && ichg < pinfo->data.pe_ichg_level_threshold) {
		ret = pe_leave(pinfo, true);
		if (ret < 0 || pe->is_connect)
			goto _err;
		chr_info("%s: OK, SOC = (%d,%d), Ichg = %dmA, stop PE+\n",
			__func__, battery_get_soc(),
			pinfo->data.ta_stop_battery_soc, ichg / 1000);
	}

	return ret;

_err:
	chr_err("%s: failed, is_connect = %d, ret = %d\n",
		__func__, pe->is_connect, ret);
	return ret;
}

static int __pe_increase_ta_vchr(struct charger_manager *pinfo)
{
	int ret = 0;
	bool increase = true; /* Increase */

	if (mt_get_charger_type() != CHARGER_UNKNOWN) {
		ret = charger_dev_send_ta_current_pattern(pinfo->chg1_dev,
							increase);

		if (ret < 0)
			chr_err("%s: failed, ret = %d\n", __func__, ret);
		else
			chr_info("%s: OK\n", __func__);
		return ret;
	}

	/* TA is not exist */
	ret = -EIO;
	chr_err("%s: failed, cable out\n", __func__);
	return ret;
}

static int pe_increase_ta_vchr(struct charger_manager *pinfo, u32 vchr_target)
{
	int ret = 0;
	int vchr_before, vchr_after;
	u32 retry_cnt = 0;
	bool chg2_chip_enabled = false;

	do {
		if (pinfo->chg2_dev) {
			charger_dev_is_chip_enabled(pinfo->chg2_dev,
				&chg2_chip_enabled);
			if (chg2_chip_enabled)
				charger_dev_enable(pinfo->chg2_dev, false);
		}

		vchr_before = pe_get_vbus();
		__pe_increase_ta_vchr(pinfo);
		vchr_after = pe_get_vbus();

		if (abs(vchr_after - vchr_target) <= 1000000) {
			chr_info("%s: OK\n", __func__);
			return ret;
		}
		chr_err("%s: retry, cnt = %d, vchr = (%d, %d), vchr_target = %d\n",
			__func__, retry_cnt, vchr_before / 1000,
			vchr_after / 1000, vchr_target / 1000);

		retry_cnt++;
	} while (mt_get_charger_type() != CHARGER_UNKNOWN && retry_cnt < 3 &&
		pinfo->enable_hv_charging && cancel_pe(pinfo) != true);

	ret = -EIO;
	chr_err("%s: failed, vchr = (%d, %d), vchr_target = %d\n",
		__func__, vchr_before / 1000, vchr_after / 1000,
		vchr_target / 1000);

	return ret;
}

static int pe_detect_ta(struct charger_manager *pinfo)
{
	int ret = 0;
	struct mtk_pe *pe = &pinfo->pe;

	chr_debug("%s: starts\n", __func__);

	/* Disable OVP */
	ret = pe_enable_vbus_ovp(pinfo, false);
	if (ret < 0)
		goto _err;

	pe->ta_vchr_org = pe_get_vbus();
	ret = pe_increase_ta_vchr(pinfo, 7000000); /* uv */

	if (ret == 0) {
		pe->is_connect = true;
		chr_info("%s: OK, is_connect = %d\n", __func__,
			pe->is_connect);
		return ret;
	}

	/* Detect PE+ TA failed */
	pe->is_connect = false;
	pe->to_check_chr_type = false;

	/* Enable OVP */
	pe_enable_vbus_ovp(pinfo, true);

	/* Set MIVR for vbus 5V */
	pe_set_mivr(pinfo, pinfo->data.min_charger_voltage);

_err:
	chr_err("%s: failed, is_connect = %d\n",
		__func__, pe->is_connect);

	return ret;
}

static int pe_init_ta(struct charger_manager *pinfo)
{
	int ret = 0;

	chr_debug("%s: starts\n", __func__);
	if (pinfo->data.ta_9v_support || pinfo->data.ta_12v_support)
		pinfo->pe.to_tune_ta_vchr = true;

	return ret;
}

static int mtk_pe_plugout_reset(struct charger_manager *pinfo)
{
	int ret = 0;

	chr_debug("%s: starts\n", __func__);

	pinfo->pe.to_check_chr_type = true;

	ret = mtk_pe_reset_ta_vchr(pinfo);
	if (ret < 0)
		goto _err;

	/* Set cable out occur to false */
	mtk_pe_set_is_cable_out_occur(pinfo, false);
	chr_info("%s: OK\n", __func__);
	return ret;

_err:
	chr_err("%s: failed, ret = %d\n", __func__, ret);

	return ret;
}

int mtk_pe_init(struct charger_manager *pinfo)
{
	wakeup_source_init(&pinfo->pe.suspend_lock, "PE+ suspend wakelock");
	mutex_init(&pinfo->pe.access_lock);
	mutex_init(&pinfo->pe.pmic_sync_lock);

	pinfo->pe.ta_vchr_org = 5000000;
	pinfo->pe.to_check_chr_type = true;
	pinfo->pe.to_tune_ta_vchr = true;
	pinfo->pe.is_enabled = true;

	return 0;
}

int mtk_pe_reset_ta_vchr(struct charger_manager *pinfo)
{
	int ret = 0, chr_volt = 0;
	u32 retry_cnt = 0;
	struct mtk_pe *pe = &pinfo->pe;
	bool chg2_chip_enabled = false;

	chr_debug("%s: starts\n", __func__);

	pe_set_mivr(pinfo, pinfo->data.min_charger_voltage);

	/* Reset TA's charging voltage */
	do {
		if (pinfo->chg2_dev) {
			charger_dev_is_chip_enabled(pinfo->chg2_dev,
				&chg2_chip_enabled);
			if (chg2_chip_enabled)
				charger_dev_enable(pinfo->chg2_dev, false);
		}

		ret = charger_dev_reset_ta(pinfo->chg1_dev);
		msleep(250);

		/* Check charger's voltage */
		chr_volt = pe_get_vbus();
		if (abs(chr_volt - pe->ta_vchr_org) <= 1000000) {
			pe->is_connect = false;
			break;
		}

		retry_cnt++;
	} while (retry_cnt < 3);

	if (pe->is_connect) {
		chr_err("%s: failed, ret = %d\n", __func__, ret);
		pe_set_mivr(pinfo, chr_volt - 1000000);
		/*
		 * SET_INPUT_CURRENT success but chr_volt does not reset to 5V
		 * set ret = -EIO to represent the case
		 */
		ret = -EIO;
		return ret;
	}

	/* Enable OVP */
	ret = pe_enable_vbus_ovp(pinfo, true);
	chr_info("%s: OK\n", __func__);

	return ret;
}

int mtk_pe_check_charger(struct charger_manager *pinfo)
{
	int ret = 0;
	struct mtk_pe *pe = &pinfo->pe;

	if (!pinfo->enable_hv_charging) {
		pr_info("%s: hv charging is disabled\n", __func__);
		if (pe->is_connect) {
			pe_leave(pinfo, true);
			pe->to_check_chr_type = true;
		}
		return ret;
	}

	if (mtk_pe20_get_is_connect(pinfo)) {
		chr_info("%s: stop, PE+20 is connected\n", __func__);
		return ret;
	}

	if (!pinfo->enable_pe_plus)
		return -ENOTSUPP;

	if (!pe->is_enabled) {
		chr_info("%s: stop, PE+ is disabled\n", __func__);
		return ret;
	}

	/* Lock */
	mutex_lock(&pe->access_lock);
	__pm_stay_awake(&pe->suspend_lock);

	chr_debug("%s: starts\n", __func__);

	if (mt_get_charger_type() == CHARGER_UNKNOWN || pe->is_cable_out_occur)
		mtk_pe_plugout_reset(pinfo);

	/* Not to check charger type or
	 * Not standard charger or
	 * SOC is not in range
	 */
	if (!pe->to_check_chr_type ||
	    mt_get_charger_type() != STANDARD_CHARGER ||
	    pinfo->data.ta_start_battery_soc > battery_get_soc() ||
	    pinfo->data.ta_stop_battery_soc <= battery_get_soc())
		goto _err;

	if (cancel_pe(pinfo))
		goto _err;

	/* Reset/Init/Detect TA */
	ret = mtk_pe_reset_ta_vchr(pinfo);
	if (ret < 0)
		goto _err;

	ret = pe_init_ta(pinfo);
	if (ret < 0)
		goto _err;

	if (cancel_pe(pinfo))
		goto _err;

	ret = pe_detect_ta(pinfo);
	if (ret < 0)
		goto _err;

	pe->to_check_chr_type = false;

	/* Unlock */
	__pm_relax(&pe->suspend_lock);
	mutex_unlock(&pe->access_lock);
	chr_info("%s: OK, to_check_chr_type = %d\n",
		__func__, pe->to_check_chr_type);

	return ret;

_err:
	/* Unlock */
	__pm_relax(&pe->suspend_lock);
	mutex_unlock(&pe->access_lock);
	chr_info("%s: stop, SOC = %d, to_check_chr_type = %d, chr_type = %d, ret = %d\n",
		__func__, battery_get_soc(), pe->to_check_chr_type,
		mt_get_charger_type(), ret);

	return ret;
}

int mtk_pe_start_algorithm(struct charger_manager *pinfo)
{
	int ret = 0, chr_volt;
	struct mtk_pe *pe = &pinfo->pe;

	if (!pinfo->enable_hv_charging) {
		chr_info("%s: hv charging is disabled\n", __func__);
		if (pe->is_connect) {
			pe_leave(pinfo, true);
			pe->to_check_chr_type = true;
		}
		return ret;
	}

	if (mtk_pe20_get_is_connect(pinfo)) {
		chr_info("%s: stop, PE+20 is connected\n", __func__);
		return ret;
	}

	if (!pe->is_enabled) {
		chr_info("%s: stop, PE+ is disabled\n", __func__);
		return ret;
	}

	/* Lock */
	mutex_lock(&pe->access_lock);
	__pm_stay_awake(&pe->suspend_lock);

	chr_debug("%s: starts\n", __func__);

	if (mt_get_charger_type() == CHARGER_UNKNOWN || pe->is_cable_out_occur)
		mtk_pe_plugout_reset(pinfo);

	/* TA is not connected */
	if (!pe->is_connect) {
		ret = -EIO;
		chr_info("%s: stop, PE+ is not connected\n", __func__);
		goto _out;
	}

	/* No need to tune TA */
	if (!pe->to_tune_ta_vchr) {
		ret = pe_check_leave_status(pinfo);
		chr_info("%s: stop, not to tune TA vchr\n",
			__func__);
		goto _out;
	}

	pe->to_tune_ta_vchr = false;

	/* Increase TA voltage to 9V */
	if (pinfo->data.ta_9v_support || pinfo->data.ta_12v_support) {
		ret = pe_increase_ta_vchr(pinfo, 9000000); /* uv */
		if (ret < 0) {
			chr_err("%s: failed, cannot increase to 9V\n",
				__func__);
			goto _err;
		}

		/* Successfully, increase to 9V */
		chr_info("%s: output 9V ok\n", __func__);

	}

	/* Increase TA voltage to 12V */
	if (pinfo->data.ta_12v_support) {
		ret = pe_increase_ta_vchr(pinfo, 12000000); /* uv */
		if (ret < 0) {
			chr_err("%s: failed, cannot increase to 12V\n",
				__func__);
			goto _err;
		}

		/* Successfully, increase to 12V */
		chr_info("%s: output 12V ok\n", __func__);
	}

	chr_volt = pe_get_vbus();
	ret = pe_set_mivr(pinfo, chr_volt - 1000000);
	if (ret < 0)
		goto _err;

	chr_info("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
		__func__, pe->ta_vchr_org / 1000, chr_volt / 1000,
		(chr_volt - pe->ta_vchr_org) / 1000);
	chr_info("%s: OK\n", __func__);

	__pm_relax(&pe->suspend_lock);
	mutex_unlock(&pe->access_lock);
	return ret;

_err:
	pe_leave(pinfo, false);
_out:
	chr_volt = pe_get_vbus();
	chr_info("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
		__func__, pe->ta_vchr_org / 1000, chr_volt / 1000,
		(chr_volt - pe->ta_vchr_org) / 1000);

	__pm_relax(&pinfo->pe.suspend_lock);
	mutex_unlock(&pinfo->pe.access_lock);

	return ret;
}

int mtk_pe_set_charging_current(struct charger_manager *pinfo,
	unsigned int *ichg, unsigned int *aicr)
{
	int ret = 0, chr_volt = 0;
	struct mtk_pe *pe = &pinfo->pe;

	if (!pe->is_connect)
		return -ENOTSUPP;

	chr_volt = pe_get_vbus();
	if ((chr_volt - pe->ta_vchr_org) > 6000000) { /* TA = 12V */
		*aicr = pinfo->data.ta_ac_12v_input_current;
		*ichg = pinfo->data.ta_ac_charger_current;
	} else if ((chr_volt - pe->ta_vchr_org) > 3000000) { /* TA = 9V */
		*aicr = pinfo->data.ta_ac_9v_input_current;
		*ichg = pinfo->data.ta_ac_charger_current;
	} else if ((chr_volt - pe->ta_vchr_org) > 1000000) { /* TA = 7V */
		*aicr = pinfo->data.ta_ac_7v_input_current;
		*ichg = pinfo->data.ta_ac_charger_current;
	}

	chr_info("%s: Ichg= %dmA, AICR = %dmA, chr_org = %d, chr_after = %d\n",
		__func__, *ichg / 1000, *aicr / 1000, pe->ta_vchr_org / 1000,
		chr_volt / 1000);
	return ret;
}

/* PE+ set functions */

void mtk_pe_set_to_check_chr_type(struct charger_manager *pinfo, bool check)
{
	mutex_lock(&pinfo->pe.access_lock);
	__pm_stay_awake(&pinfo->pe.suspend_lock);

	chr_info("%s: check = %d\n", __func__, check);
	pinfo->pe.to_check_chr_type = check;

	__pm_relax(&pinfo->pe.suspend_lock);
	mutex_unlock(&pinfo->pe.access_lock);
}

void mtk_pe_set_is_enable(struct charger_manager *pinfo, bool enable)
{
	mutex_lock(&pinfo->pe.access_lock);
	__pm_stay_awake(&pinfo->pe.suspend_lock);

	chr_info("%s: enable = %d\n", __func__, enable);
	pinfo->pe.is_enabled = enable;

	__pm_relax(&pinfo->pe.suspend_lock);
	mutex_unlock(&pinfo->pe.access_lock);
}

void mtk_pe_set_is_cable_out_occur(struct charger_manager *pinfo, bool out)
{
	chr_info("%s: out = %d\n", __func__, out);
	mutex_lock(&pinfo->pe.pmic_sync_lock);
	pinfo->pe.is_cable_out_occur = out;
	mutex_unlock(&pinfo->pe.pmic_sync_lock);
}

/* PE+ get functions */
bool mtk_pe_get_to_check_chr_type(struct charger_manager *pinfo)
{
	return pinfo->pe.to_check_chr_type;
}

bool mtk_pe_get_is_connect(struct charger_manager *pinfo)
{
	/*
	 * Cable out is occurred,
	 * but not execute plugout_reset yet
	 */
	if (pinfo->pe.is_cable_out_occur)
		return false;

	return pinfo->pe.is_connect;
}

bool mtk_pe_get_is_enable(struct charger_manager *pinfo)
{
	return pinfo->pe.is_enabled;
}
