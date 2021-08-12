// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include "mtk_pe.h"
#include "mtk_charger_algorithm_class.h"

#define VBUS_MAX_DROP 1500000

static int pe_dbg_level = PE_DEBUG_LEVEL;

int pe_get_debug_level(void)
{
	return pe_dbg_level;
}

int mtk_pe_reset_ta_vchr(struct chg_alg_device *alg)
{
	int ret = -1, chr_volt = 0;
	u32 retry_cnt = 0;
	struct mtk_pe *pe;
	bool is_chip_enabled = false;
	int chg_cnt, i;
	int mivr;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s: starts\n", __func__);

	pe_hal_set_mivr(alg, CHG1, pe->min_charger_voltage);

	/* Reset TA's charging voltage */
	do {

		chg_cnt = pe_hal_get_charger_cnt(alg);
		if (chg_cnt > 1) {
			for (i = CHG2; i < CHG_MAX; i++) {
				is_chip_enabled = pe_hal_is_chip_enable(alg, i);
				if (is_chip_enabled)
					pe_hal_enable_chip(alg, i, false);
			}
		}

		ret = pe_hal_reset_ta(alg, CHG1);
		if (ret < 0)
			pe_err("%s reset TA fail\n", __func__);
		msleep(250);

		/* Check charger's voltage */
		chr_volt = pe_hal_get_vbus(alg);
		if (abs(chr_volt - pe->ta_vchr_org) <= 1000000)
			break;

		retry_cnt++;
	} while (retry_cnt < 3);

	if (retry_cnt >= 3) {
		pe_err("%s: failed, ret = %d\n", __func__, ret);
		mivr = chr_volt - 1000000;
		if (mivr < pe->min_charger_voltage)
			mivr = pe->min_charger_voltage;
		pe_hal_set_mivr(alg, CHG1, mivr);
		/*
		 * SET_INPUT_CURRENT success but chr_volt does not reset to 5V
		 * set ret = -EIO to represent the case
		 */
		return -EIO;
	}

	/* Enable OVP */
	ret = pe_hal_enable_vbus_ovp(alg, true);
	if (ret < 0)
		pe_err("%s enable vbus ovp fail\n", __func__);

	pe_dbg("%s: OK\n", __func__);

	return 0;
}

static int pe_leave(struct chg_alg_device *alg, bool disable_charging)
{
	int ret = 0, ret_value;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s: starts\n", __func__);

	/* CV point reached, disable charger */
	ret = pe_hal_enable_charging(alg, disable_charging);
	if (ret < 0) {
		pe_err("%s enable charging fail:%d\n",
			__func__, ret);
		ret_value = -EHAL;
	}

	/* Decrease TA voltage to 5V */
	ret = mtk_pe_reset_ta_vchr(alg);
	if (ret < 0) {
		pe_err("%s reset TA fail:%d\n",
			__func__, ret);
		ret_value = -EHAL;
	}

	pe_dbg("%s: OK\n", __func__);
	return ret;
}

int __pe_increase_ta_vchr(struct chg_alg_device *alg)
{
	int ret = 0, ret_value = 0;
	bool increase = true; /* Increase */

	if (pe_hal_get_charger_type(alg) !=
		POWER_SUPPLY_TYPE_UNKNOWN) {
		ret = pe_hal_send_ta_current_pattern(alg,
							increase);

		if (ret < 0) {
			pe_err("%s: failed, ret = %d\n", __func__, ret);
			ret_value = -EHAL;
		} else
			pe_dbg("%s: OK\n", __func__);
		return ret_value;
	}

	/* TA is not exist */
	ret_value = -ECABLEOUT;
	pe_err("%s: failed, cable out\n", __func__);
	return ret_value;
}

static int pe_increase_ta_vchr(struct chg_alg_device *alg, u32 vchr_target)
{
	int ret = 0, ret_value = 0;
	int vchr_before, vchr_after;
	u32 retry_cnt = 0;
	bool is_chip_enabled = false;
	int chg_cnt, i;

	do {

		chg_cnt = pe_hal_get_charger_cnt(alg);
		if (chg_cnt > 1) {
			for (i = CHG2; i < CHG_MAX; i++) {
				is_chip_enabled = pe_hal_is_chip_enable(alg, i);
				if (is_chip_enabled)
					ret = pe_hal_enable_chip(alg, i, false);
				if (ret < 0)
					pe_err("enable chip fail %d\n", i);
			}
		}

		vchr_before = pe_hal_get_vbus(alg);
		__pe_increase_ta_vchr(alg);
		vchr_after = pe_hal_get_vbus(alg);

		if (abs(vchr_after - vchr_target) <= VBUS_MAX_DROP) {
			pe_dbg("%s: OK\n", __func__);
			return ret_value;
		}
		pe_dbg("%s: retry, cnt = %d, vchr = (%d, %d), vchr_target = %d\n",
			__func__, retry_cnt, vchr_before / 1000,
			vchr_after / 1000, vchr_target / 1000);

		retry_cnt++;
	} while (pe_hal_get_charger_type(alg) !=
		POWER_SUPPLY_TYPE_UNKNOWN && retry_cnt < 3);

	ret = -EHAL;
	pe_dbg("%s: failed, vchr = (%d, %d), vchr_target = %d\n",
		__func__, vchr_before / 1000, vchr_after / 1000,
		vchr_target / 1000);

	return ret;
}

static int pe_detect_ta(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s: starts\n", __func__);

	/* Disable OVP */
	ret = pe_hal_enable_vbus_ovp(alg, false);
	if (ret < 0)
		goto _err;

	pe->ta_vchr_org = pe_hal_get_vbus(alg);
	ret = pe_increase_ta_vchr(alg, 7000000); /* uv */

	if (ret == 0) {
		pe_dbg("%s: OK\n", __func__);
		return ret;
	}

	/* Detect PE+ TA failed */
	ret = -1;

	/* Enable OVP */
	pe_hal_enable_vbus_ovp(alg, true);

	/* Set MIVR for vbus 5V */
	pe_hal_set_mivr(alg, CHG1, pe->min_charger_voltage);

_err:
	pe_err("%s: failed, ret:%d\n",
		__func__, ret);

	return ret;
}

static int pe_init_ta(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s: starts\n", __func__);

	return ret;
}

static int pe_plugout_reset(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s: starts\n", __func__);

	/* pe is not running */
	if (pe->state != PE_RUN &&
		pe->state != PE_DONE) {
		pe->state = PE_HW_READY;
		pe_err("%s:not running,state:%d\n",
			__func__, pe->state);
		return ret;
	}

	if (pe->state != PE_HW_FAIL)
		pe->state = PE_HW_READY;

	ret = mtk_pe_reset_ta_vchr(alg);
	if (ret < 0)
		goto _err;

	pe_dbg("%s: OK\n", __func__);
	return ret;

_err:
	pe_err("%s: failed, ret = %d\n", __func__, ret);

	return ret;
}

int __pe_check_charger(struct chg_alg_device *alg)
{
	int ret = 0, uisoc, ret_value = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s: starts\n", __func__);
	uisoc = pe_hal_get_uisoc(alg);
	pe_dbg("%s uisoc:%d s:%d end:%d type:%d", __func__,
		uisoc,
		pe->ta_start_battery_soc,
		pe->ta_stop_battery_soc,
		pe_hal_get_charger_type(alg));

	if (pe_hal_get_charger_type(alg) != POWER_SUPPLY_TYPE_USB_DCP) {
		ret_value = ALG_TA_NOT_SUPPORT;
		goto out;
	}

	if ((uisoc < pe->ta_start_battery_soc &&
	    pe->ref_vbat > pe->vbat_threshold) ||
		uisoc >= pe->ta_stop_battery_soc) {
		ret_value = ALG_TA_CHECKING;
		goto out;
	}

	if (pe->is_cable_out_occur)
		goto out;

	/* Reset/Init/Detect TA */
	ret = mtk_pe_reset_ta_vchr(alg);
	if (ret < 0)
		goto out;

	if (pe->is_cable_out_occur)
		goto out;

	ret = pe_init_ta(alg);
	if (ret < 0)
		goto out;

	if (pe->is_cable_out_occur)
		goto out;

	ret = pe_detect_ta(alg);
	if (ret < 0)
		goto out;

	pe_dbg("%s: OK\n",
		__func__);
	return ret_value;
out:
	if (ret_value == 0)
		ret_value = ALG_TA_NOT_SUPPORT;

	pe_dbg("%s: stop, SOC:%d, chr_type:%d, ret:%d:%d ref_vbat:%d\n",
		__func__, pe_hal_get_uisoc(alg),
		pe_hal_get_charger_type(alg), ret,
		ret_value, pe->ref_vbat);

	return ret_value;
}

int mtk_pe_set_charging_current(struct chg_alg_device *alg)
{
	int ret = 0, chr_volt = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);

	chr_volt = pe_hal_get_vbus(alg);
	if ((chr_volt - pe->ta_vchr_org) > 6000000) { /* TA = 12V */
		pe->input_current = pe->ta_ac_12v_input_current;
		pe->charging_current = pe->ta_ac_charger_current;
	} else if ((chr_volt - pe->ta_vchr_org) > 3000000) { /* TA = 9V */
		pe->input_current  = pe->ta_ac_9v_input_current;
		pe->charging_current = pe->ta_ac_charger_current;
	} else if ((chr_volt - pe->ta_vchr_org) > 1000000) { /* TA = 7V */
		pe->input_current  = pe->ta_ac_7v_input_current;
		pe->charging_current = pe->ta_ac_charger_current;
	}

	if (pe->charging_current_limit != -1 &&
		pe->charging_current_limit <
		pe->charging_current)
		pe->charging_current = pe->charging_current_limit;

	if (pe->input_current_limit != -1 &&
		pe->input_current_limit <
		pe->input_current)
		pe->input_current = pe->input_current_limit;

	pe_hal_set_charging_current(alg,
		CHG1, pe->input_current);
	pe_hal_set_input_current(alg,
		CHG1, pe->charging_current);
	pe_hal_set_cv(alg,
		CHG1, pe->cv);

	pe_dbg("%s cv:%d icl:%d cc:%d chr_org:%d chr_after:%d\n",
		__func__,
		pe->cv,
		pe->input_current,
		pe->charging_current,
		pe->ta_vchr_org / 1000,
		chr_volt / 1000);

	return ret;
}

static char *pe_state_to_str(int state)
{
	switch (state) {
	case PE_HW_UNINIT:
		return "PE_HW_UNINIT";
		break;
	case PE_HW_FAIL:
		return "PE_HW_FAIL";
		break;
	case PE_HW_READY:
		return "PE_HW_READY";
		break;
	case PE_TA_NOT_SUPPORT:
		return "PE_TA_NOT_SUPPORT";
		break;
	case PE_RUN:
		return "PE_RUN";
		break;
	case PE_DONE:
		return "PE_DONE";
		break;
	default:
		break;
	}
	pe_err("%s unknown state:%d\n", __func__
		, state);
	return "PE_UNKNOWN";
}

static int _pe_is_algo_ready(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;
	int ret_value = 0, uisoc;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s state:%s\n", __func__,
		pe_state_to_str(pe->state));

	switch (pe->state) {
	case PE_HW_UNINIT:
	case PE_HW_FAIL:
		ret_value = ALG_INIT_FAIL;
		break;
	case PE_HW_READY:
		uisoc = pe_hal_get_uisoc(alg);
		if (pe_hal_get_charger_type(alg) !=
			POWER_SUPPLY_TYPE_USB_DCP) {
			ret_value = ALG_TA_NOT_SUPPORT;
		} else if ((uisoc < pe->ta_start_battery_soc &&
			    pe->ref_vbat > pe->vbat_threshold) ||
			uisoc >= pe->ta_stop_battery_soc) {
			ret_value = ALG_NOT_READY;
		} else {
			ret_value = ALG_READY;
		}
		break;
	case PE_TA_NOT_SUPPORT:
		ret_value = ALG_TA_NOT_SUPPORT;
		break;
	case PE_RUN:
		ret_value = ALG_RUNNING;
		break;
	case PE_DONE:
		ret_value = ALG_DONE;
		break;
	default:
		break;
	}

	return ret_value;
}

static int _pe_init_algo(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;
	int log_level;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s\n", __func__);

	if (pe_hal_init_hardware(alg) != 0)
		pe->state = PE_HW_FAIL;
	else
		pe->state = PE_HW_READY;

	log_level = pe_hal_get_log_level(alg);
	pr_notice("%s: log_level=%d", __func__, log_level);
	if (log_level > 0)
		pe_dbg_level = log_level;

	return 0;
}

static bool _pe_is_algo_running(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;

	pe_dbg("%s\n", __func__);
	pe = dev_get_drvdata(&alg->dev);

	if (pe->state == PE_RUN)
		return true;
	return false;
}

static int _pe_stop_algo(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);

	pe_dbg("%s %d\n", __func__, pe->state);
	if (pe->state == PE_RUN) {
		mtk_pe_reset_ta_vchr(alg);
		pe->state = PE_HW_READY;
	}

	return 0;
}

static int _pe_notifier_call(struct chg_alg_device *alg,
			 struct chg_alg_notify *notify)
{
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s evt:%d\n", __func__, notify->evt);

	pe_dbg("%s state:%d %s\n", __func__,
		pe->state,
		pe_state_to_str(pe->state));

	switch (notify->evt) {
	case EVT_PLUG_OUT:
		pe_plugout_reset(alg);
		break;
	case EVT_FULL:
		if (pe->state == PE_RUN) {
			pe_err("%s evt full\n",  __func__);
			pe_leave(alg, true);
			pe->state = PE_DONE;
		}
		break;
	case EVT_RECHARGE:
		if (pe->state == PE_DONE) {
			pe_err("%s evt recharge\n",  __func__);
			pe->state = PE_HW_READY;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int pe_get_conditional_vbus(struct chg_alg_device *alg, u32 uA)
{
	int chr_volt = 0, orig_chr_current = 0;

	pe_hal_get_charging_current(alg, CHG1, &orig_chr_current);
	pe_hal_set_charging_current(alg, CHG1, uA);
	msleep(20);
	chr_volt = pe_hal_get_vbus(alg);
	pe_hal_set_charging_current(alg, CHG1, orig_chr_current);

	return chr_volt;
}

int _pe_get_status(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value)
{
	pe_dbg("%s\n", __func__);
	if (s == ALG_MAX_VBUS)
		*value = 12000;
	else
		pe_dbg("%s does not support prop:%d\n", __func__, s);
	return 0;
}

int _pe_set_prop(struct chg_alg_device *alg,
		enum chg_alg_props s, int value)
{
	struct mtk_pe *pe;

	pr_notice("%s %d %d\n", __func__, s, value);

	pe = dev_get_drvdata(&alg->dev);

	switch (s) {
	case ALG_LOG_LEVEL:
		pe_dbg_level = value;
		break;
	case ALG_REF_VBAT:
		pe->ref_vbat = value;
		break;
	default:
		break;
	}

	return 0;
}

int _pe_set_setting(struct chg_alg_device *alg_dev,
	struct chg_limit_setting *setting)
{
	struct mtk_pe *pe;

	pe_dbg("%s cv:%d icl:%d cc:%d\n",
		__func__,
		setting->cv,
		setting->input_current_limit1,
		setting->charging_current_limit1);
	pe = dev_get_drvdata(&alg_dev->dev);
	pe->cv = setting->cv;
	pe->input_current_limit = setting->input_current_limit1;
	pe->charging_current_limit = setting->charging_current_limit1;

	return 0;
}

static int __pe_run(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;
	int ret = 0, chr_volt, chr_volt2, ret_value = 0, ichg;
	bool tune = false;
	int mivr;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s: starts\n", __func__);

	if (pe->is_cable_out_occur)
		goto _out;

	chr_volt = pe_hal_get_vbus(alg);
	chr_volt2 = pe_get_conditional_vbus(alg, 500000);

	if (pe->ta_9v_support && pe->ta_12v_support) {
		if (abs(chr_volt - 12000000) > VBUS_MAX_DROP) {
			if (abs(chr_volt2 - 12000000) > VBUS_MAX_DROP)
				tune = true;
			else {
				pe_err("%s: V drop out of range, skip pe", __func__);
				ret_value = ALG_TA_NOT_SUPPORT;
				goto _err;
			}
		}
		pe_dbg("%s: vbus:%d target:%d 9v:%d 12v:%d tune:%d\n",
			__func__, chr_volt,
			12000000, pe->ta_9v_support,
			pe->ta_12v_support, tune);
	} else if (pe->ta_9v_support && !pe->ta_12v_support) {
		if (abs(chr_volt - 9000000) > VBUS_MAX_DROP) {
			if (abs(chr_volt2 - 9000000) > VBUS_MAX_DROP)
				tune = true;
			else {
				pe_err("%s: V drop out of range, skip pe", __func__);
				ret_value = ALG_TA_NOT_SUPPORT;
				goto _err;
			}
		}
		pe_dbg("%s: vbus:%d target:%d 9v:%d 12v:%d tune:%d\n",
			__func__, chr_volt,
			9000000, pe->ta_9v_support,
			pe->ta_12v_support, tune);
	} else {
		tune = false;
		pe_dbg("%s:error setting vbus:%d 9v:%d 12v:%d tune:%d\n",
			__func__, chr_volt,
			pe->ta_9v_support, pe->ta_12v_support,
			tune);
		ret_value = ALG_TA_NOT_SUPPORT;
		goto _err;
	}

	if (tune == true) {
		/* Increase TA voltage to 9V */
		if (pe->ta_9v_support || pe->ta_12v_support) {
			ret = pe_increase_ta_vchr(alg, 9000000); /* uv */
			if (ret < 0) {
				pe_err("%s: failed, cannot increase to 9V\n",
					__func__);
				ret_value = ALG_TA_NOT_SUPPORT;
				goto _err;
			}

			/* Successfully, increase to 9V */
			pe_dbg("%s: output 9V ok,vbus:%d\n",
				__func__,
				pe_hal_get_vbus(alg));
		}

		/* Increase TA voltage to 12V */
		if (pe->ta_12v_support) {
			ret = pe_increase_ta_vchr(alg, 12000000); /* uv */
			if (ret < 0) {
				pe_err("%s: failed, cannot increase to 12V\n",
					__func__);
				ret_value = ALG_TA_NOT_SUPPORT;
				goto _err;
			}

			/* Successfully, increase to 12V */
			pe_dbg("%s: output 12V ok,vbus:%d\n",
				__func__,
				pe_hal_get_vbus(alg));
		}
	} else {
		ichg = pe_hal_get_ibat(alg);

		/* Check SOC & Ichg */
		if (pe->ta_stop_battery_soc < pe_hal_get_uisoc(alg) &&
		    ichg > 0 && ichg < pe->pe_ichg_level_threshold) {
			ret_value = ALG_DONE;
			pe_dbg("%s: OK, SOC = (%d,%d), Ichg:%dmA,ret:%d stop PE+\n",
				__func__, pe_hal_get_uisoc(alg),
				pe->ta_stop_battery_soc, ichg / 1000,
				ret);
			pe_leave(alg, true);
			goto _out;
		}
	}

	mtk_pe_set_charging_current(alg);

	chr_volt = pe_hal_get_vbus(alg);
	mivr = chr_volt - 1000000;
	if (mivr < pe->min_charger_voltage)
		mivr = pe->min_charger_voltage;
	ret = pe_hal_set_mivr(alg, CHG1, mivr);
	if (ret < 0) {
		pe_err("%s: set mivr fail\n",
			__func__);
		ret_value = ALG_TA_NOT_SUPPORT;
		goto _err;
	}

	pe_dbg("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
		__func__, pe->ta_vchr_org / 1000, chr_volt / 1000,
		(chr_volt - pe->ta_vchr_org) / 1000);
	pe_dbg("%s: OK\n", __func__);

	return ret_value;

_err:
	pe_leave(alg, false);
_out:

	if (ret_value != 0)
		ret_value = ALG_TA_NOT_SUPPORT;
	chr_volt = pe_hal_get_vbus(alg);
	pr_debug("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
		__func__, pe->ta_vchr_org / 1000, chr_volt / 1000,
		(chr_volt - pe->ta_vchr_org) / 1000);

	return ret_value;
}


int _pe_start_algo(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;
	bool again;
	int ret, ret_value;

	pe = dev_get_drvdata(&alg->dev);
	pe_dbg("%s state:%d %s\n", __func__,
		pe->state,
		pe_state_to_str(pe->state));

	/* Lock */
	mutex_lock(&pe->access_lock);
	__pm_stay_awake(pe->suspend_lock);

	do {

		pe_info("%s state:%d %s %d\n", __func__,
			pe->state,
			pe_state_to_str(pe->state),
			again);
		again = false;

		switch (pe->state) {
		case PE_HW_UNINIT:
		case PE_HW_FAIL:
			ret_value = ALG_INIT_FAIL;
			break;
		case PE_HW_READY:
			ret = __pe_check_charger(alg);
			if (ret == 0) {
				pe->state = PE_RUN;
				ret_value = ALG_READY;
				again = true;
			} else if (ret == ALG_TA_CHECKING)
				ret_value = ALG_TA_CHECKING;
			else {
				pe->state = PE_TA_NOT_SUPPORT;
				ret_value = ALG_TA_NOT_SUPPORT;
			}
			break;

		case PE_TA_NOT_SUPPORT:
			ret_value = ALG_TA_NOT_SUPPORT;
			break;
		case PE_RUN:
			ret = __pe_run(alg);
			if (ret == ALG_TA_NOT_SUPPORT)
				pe->state = PE_TA_NOT_SUPPORT;
			else if (ret == ALG_TA_CHECKING) {
				pe->state = PE_RUN;
				again = true;
			} else if (ret == ALG_DONE)
				pe->state = PE_DONE;
			ret_value = ret;
			break;
		case PE_DONE:
			ret_value = ALG_DONE;
			break;
		default:
			pe_err("PE unknown state:%d\n", pe->state);
			ret_value = ALG_INIT_FAIL;
			break;
		}
	} while (again == true);

	__pm_relax(pe->suspend_lock);
	mutex_unlock(&pe->access_lock);

	return ret;
}


static struct chg_alg_ops pe_alg_ops = {
	.init_algo = _pe_init_algo,
	.is_algo_ready = _pe_is_algo_ready,
	.start_algo = _pe_start_algo,
	.is_algo_running = _pe_is_algo_running,
	.stop_algo = _pe_stop_algo,
	.notifier_call = _pe_notifier_call,
	.get_prop = _pe_get_status,
	.set_prop = _pe_set_prop,
	.set_current_limit = _pe_set_setting,
};

static void mtk_pe_parse_dt(struct mtk_pe *pe,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;

	/* PE */

	pe->ta_12v_support = of_property_read_bool(np, "ta_12v_support");
	pe->ta_9v_support = of_property_read_bool(np, "ta_9v_support");

	if (of_property_read_u32(np, "pe_ichg_level_threshold", &val) >= 0)
		pe->pe_ichg_level_threshold = val;
	else {
		pr_notice("use default PE_ICHG_LEAVE_THRESHOLD:%d\n",
			PE_ICHG_LEAVE_THRESHOLD);
		pe->pe_ichg_level_threshold =
						PE_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "ta_start_battery_soc", &val) >= 0)
		pe->ta_start_battery_soc = val;
	else {
		pr_notice("use default TA_START_BATTERY_SOC:%d\n",
			TA_START_BATTERY_SOC);
		pe->ta_start_battery_soc = TA_START_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "ta_stop_battery_soc", &val) >= 0)
		pe->ta_stop_battery_soc = val;
	else {
		pr_notice("use default TA_STOP_BATTERY_SOC:%d\n",
			TA_STOP_BATTERY_SOC);
		pe->ta_stop_battery_soc = TA_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		pe->min_charger_voltage = val;
	else {
		pr_notice("use default V_CHARGER_MIN:%d\n", PE_V_CHARGER_MIN);
		pe->min_charger_voltage = PE_V_CHARGER_MIN;
	}

	if (of_property_read_u32(np, "ta_ac_12v_input_current", &val) >= 0)
		pe->ta_ac_12v_input_current = val;
	else {
		pr_notice("use default TA_AC_12V_INPUT_CURRENT:%d\n",
			TA_AC_12V_INPUT_CURRENT);
		pe->ta_ac_12v_input_current = TA_AC_12V_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "ta_ac_9v_input_current", &val) >= 0)
		pe->ta_ac_9v_input_current = val;
	else {
		pr_notice("use default TA_AC_9V_INPUT_CURRENT:%d\n",
			TA_AC_9V_INPUT_CURRENT);
		pe->ta_ac_9v_input_current = TA_AC_9V_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "ta_ac_7v_input_current", &val) >= 0)
		pe->ta_ac_7v_input_current = val;
	else {
		pr_notice("use default TA_AC_7V_INPUT_CURRENT:%d\n",
			TA_AC_7V_INPUT_CURRENT);
		pe->ta_ac_7v_input_current = TA_AC_7V_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "pe_charger_current", &val) >= 0)
		pe->ta_ac_charger_current = val;
	else {
		pr_notice("use default pe_charger_current:%d\n",
			PE_CHARGING_CURRENT);
		pe->ta_ac_charger_current = PE_CHARGING_CURRENT;
	}

	if (of_property_read_u32(np, "vbat_threshold", &val) >= 0)
		pe->vbat_threshold = val;
	else {
		pr_notice("turn off vbat_threshold checking:%d\n",
			DISABLE_VBAT_THRESHOLD);
		pe->vbat_threshold = DISABLE_VBAT_THRESHOLD;
	}

}

static int mtk_pe_probe(struct platform_device *pdev)
{
	struct mtk_pe *pe = NULL;

	pr_notice("%s: starts\n", __func__);

	pe = devm_kzalloc(&pdev->dev, sizeof(*pe), GFP_KERNEL);
	if (!pe)
		return -ENOMEM;
	platform_set_drvdata(pdev, pe);
	pe->pdev = pdev;

	pe->suspend_lock =
		wakeup_source_register(NULL, "PE+ suspend wakelock");
	mutex_init(&pe->access_lock);
	mutex_init(&pe->cable_out_lock);

	pe->ta_vchr_org = 5000000;

	mtk_pe_parse_dt(pe, &pdev->dev);
	pe->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev, "gauge");
	if (IS_ERR_OR_NULL(pe->bat_psy))
		pe_err("%s: devm power fail to get bat_psy\n", __func__);

	pe->alg = chg_alg_device_register("pe", &pdev->dev,
					pe, &pe_alg_ops, NULL);

	return 0;

}

static int mtk_pe_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_pe_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id mtk_pe_of_match[] = {
	{.compatible = "mediatek,charger,pe",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pe_of_match);

struct platform_device pe_device = {
	.name = "pe",
	.id = -1,
};

static struct platform_driver pe_driver = {
	.probe = mtk_pe_probe,
	.remove = mtk_pe_remove,
	.shutdown = mtk_pe_shutdown,
	.driver = {
		   .name = "pe",
		   .of_match_table = mtk_pe_of_match,
	},
};

static int __init mtk_pe_init(void)
{
	return platform_driver_register(&pe_driver);
}
module_init(mtk_pe_init);

static void __exit mtk_pe_exit(void)
{
	platform_driver_unregister(&pe_driver);
}
module_exit(mtk_pe_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Pump Express algorithm Driver");
MODULE_LICENSE("GPL");
