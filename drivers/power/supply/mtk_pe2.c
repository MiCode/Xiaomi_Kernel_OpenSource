// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_pe2.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
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

#include "mtk_pe2.h"
#include "mtk_charger_algorithm_class.h"


static int pe2_dbg_level = PE2_DEBUG_LEVEL;

int pe2_get_debug_level(void)
{
	return pe2_dbg_level;
}

int pe2_plugout_reset(struct chg_alg_device *alg)
{
	int ret = 0, cnt = 0, ret_value = 0;
	struct mtk_pe20 *pe2;

	pe2_dbg("%s\n", __func__);
	pe2 = dev_get_drvdata(&alg->dev);

	/* pe2 is not running */
	if (pe2->state != PE2_RUN &&
		pe2->state != PE2_DONE) {
		pe2->state = PE2_HW_READY;
		pe2_err("%s:not running,state:%d\n",
			__func__, pe2->state);
		return ret_value;
	}

	/* set flag to end PE2 thread asap */
	mutex_lock(&pe2->cable_out_lock);
	pe2->is_cable_out_occur = true;
	mutex_unlock(&pe2->cable_out_lock);

	while (mutex_trylock(&pe2->access_lock) == 0) {
		pe2_err("%s:pe2 is running state:%d cnt:%d\n",
			__func__, pe2->state,
			cnt);
		cnt++;
		msleep(100);
	}

	mutex_lock(&pe2->cable_out_lock);
	pe2->is_cable_out_occur = false;
	mutex_unlock(&pe2->cable_out_lock);

	pe2->idx = -1;
	pe2->vbus = 5000000; /* mV */
	if (pe2->state != PE2_HW_FAIL)
		pe2->state = PE2_HW_READY;

	/* Enable OVP */
	ret = pe2_hal_enable_vbus_ovp(alg, true);
	if (ret < 0) {
		pe2_err("%s:enable vbus ovp fail, ret:%d\n",
			__func__, ret);
		ret_value = -EHAL;
	}

	/* Set MIVR for vbus 5V */
	ret = pe2_hal_set_mivr(alg, CHG1, pe2->min_charger_voltage); /* uV */
	if (ret < 0) {
		pe2_err("%s:set mivr fail, ret:%d\n",
			__func__, ret);
		ret_value = -EHAL;
	}

	pe2_dbg("%s: OK\n", __func__);
	mutex_unlock(&pe2->access_lock);
	return ret_value;

}

int pe2_reset_ta_vchr(struct chg_alg_device *alg)
{
	int ret, chr_volt = 0, ret_value = -1;
	u32 retry_cnt = 0;
	struct mtk_pe20 *pe2;
	int chg_cnt, i, is_chip_enabled;

	pe2_dbg("%s: starts\n", __func__);
	pe2 = dev_get_drvdata(&alg->dev);

	ret = pe2_hal_set_mivr(alg, 0, pe2->min_charger_voltage);
	if (ret < 0)
		pe2_err("%s:set mivr fail, ret:%d\n",
			__func__, ret);

	/* Reset TA's charging voltage */
	do {
		chg_cnt = pe2_hal_get_charger_cnt(alg);
		if (chg_cnt > 1) {
			for (i = CHG2; i < CHG_MAX; i++) {
				is_chip_enabled =
						pe2_hal_is_chip_enable(alg, i);
				if (is_chip_enabled)
					pe2_hal_enable_chip(alg, i, false);
					if (ret < 0)
						pe2_err("%s:enable chip fail,idx:%d ret:%d\n",
						__func__, i, ret);
			}
		}

		pe2_hal_reset_ta(alg, CHG1);
		if (ret < 0)
			pe2_err("%s:reset TA fail, ret:%d\n",
				__func__, ret);
		msleep(250);

		/* Check charger's voltage */
		chr_volt = pe2_hal_get_vbus(alg);
		pe2_dbg("%s: ori_vbus:%d vbus:%d\n", __func__,
			pe2->ta_vchr_org, chr_volt);
		if (abs(chr_volt - pe2->ta_vchr_org) <= 1000000) {
			pe2->vbus = chr_volt;
			pe2->idx = -1;
			ret_value = 0;
			break;
		}
		retry_cnt++;
	} while (retry_cnt < 3);

	if (ret_value != 0) {
		pe2_err("%s: failed, ret = %d\n", __func__, ret);
		ret = pe2_hal_set_mivr(alg, CHG1, pe2->vbus - 500000);
		if (ret < 0)
			pe2_err("%s:set mivr fail, ret:%d\n",
			__func__, ret);
		return -EHAL;
	}
	/* Measure VBAT */
	pe2->vbat_orig = pe2_hal_get_vbus(alg);
	pe2_dbg("%s: OK\n", __func__);

	return ret;
}

static void pe2_check_cable_impedance(struct chg_alg_device *alg)
{
	int ret = 0;
	int vchr1, vchr2, cable_imp;
	unsigned int aicr_value;
	bool mivr_state = false;
	struct timespec ptime[2], diff;
	struct mtk_pe20 *pe2;

	pe2_dbg("%s: starts\n", __func__);
	pe2 = dev_get_drvdata(&alg->dev);

	if (pe2->vbat_orig > pe2->vbat_cable_imp_threshold) {
		pe2_info("VBAT > %dmV, directly set aicr to %dmA\n",
			pe2->vbat_cable_imp_threshold / 1000,
			pe2->pe2_input_current / 1000);
		pe2->aicr_cable_imp = pe2->pe2_input_current;
		goto end;
	}

	/* Disable cable drop compensation */
	pe2_hal_enable_cable_drop_comp(alg, false);

	get_monotonic_boottime(&ptime[0]);

	/* Set ichg = 2500mA, set MIVR */
	pe2_hal_set_charging_current(alg, CHG1, 2500000);
	mdelay(240);
	pe2_hal_set_mivr(alg, CHG1, pe2->min_charger_voltage);

	get_monotonic_boottime(&ptime[1]);
	diff = timespec_sub(ptime[1], ptime[0]);

	aicr_value = 800000;
	pe2_hal_set_input_current(alg, CHG1, aicr_value);

	/* To wait for soft-start */
	msleep(150);

	ret = pe2_hal_get_mivr_state(alg, CHG1, &mivr_state);
	if (ret != -ENOTSUPP && mivr_state) {
		pe2->aicr_cable_imp = 1000000;
		goto end;
	}

	vchr1 = pe2_hal_get_vbus(alg);

	aicr_value = 500000;
	pe2_hal_set_input_current(alg, CHG1, aicr_value);
	msleep(20);

	vchr2 = pe2_hal_get_vbus(alg);

	/*
	 * Calculate cable impedance (|V1 - V2|) / (|I2 - I1|)
	 * m_ohm = (mv * 10 * 1000) / (mA * 10)
	 * m_ohm = (uV * 10) / (mA * 10)
	 */
	cable_imp = (abs(vchr1 - vchr2) * 10) / (7400 - 4625);

	pe2_info("%s: cable_imp:%d mohm, vchr1:%d, vchr2:%d, time:%ld\n",
		__func__, cable_imp, vchr1 / 1000, vchr2 / 1000,
		diff.tv_nsec);

	/* Recover cable drop compensation */
	aicr_value = 100000;
	pe2_hal_set_input_current(alg, CHG1, aicr_value);
	msleep(250);

	if (cable_imp < pe2->cable_imp_threshold) {
		pe2->aicr_cable_imp = pe2->pe2_input_current;
		pe2_info("Normal cable\n");
	} else {
		pe2->aicr_cable_imp = 1000000; /* uA */
		pe2_info("Bad cable\n");
	}
	pe2_hal_set_input_current(alg, CHG1, pe2->aicr_cable_imp);

	pe2_info("%s: set aicr:%dmA, vbat:%dmV, mivr_state:%d\n",
		__func__, pe2->aicr_cable_imp / 1000,
		pe2->vbat_orig / 1000, mivr_state);
	return;

end:
	pe2_info("%s not started: set aicr:%dmA, vbat:%dmV, mivr_state:%d\n",
		__func__, pe2->aicr_cable_imp / 1000,
		pe2->vbat_orig / 1000, mivr_state);
}

static int _pe20_set_ta_vchr(struct chg_alg_device *alg, u32 chr_volt)
{
	int ret = 0, ret_value = 0;
	struct mtk_pe20 *pe2;
	int chg_cnt, i;
	bool is_chip_enabled;

	pe2_dbg("%s: starts\n", __func__);
	pe2 = dev_get_drvdata(&alg->dev);

	/* Not to set chr volt if cable is plugged out */
	if (pe2->is_cable_out_occur) {
		pe2_err("%s: failed, cable out\n", __func__);
		return -ECABLEOUT;
	}

	chg_cnt = pe2_hal_get_charger_cnt(alg);
	if (chg_cnt > 1) {
		for (i = CHG2; i < CHG_MAX; i++) {
			is_chip_enabled = pe2_hal_is_chip_enable(alg, i);
			if (is_chip_enabled) {
				ret = pe2_hal_enable_chip(alg, i, false);
				if (ret < 0)
					pe2_err("%s:enable chip fail,idx:%d ret:%d\n",
					__func__, i, ret);
					ret_value = -EHAL;
				}
		}
	}
	ret = pe2_hal_send_ta20_current_pattern(alg, chr_volt);
	if (ret < 0) {
		pe2_err("%s: pattern failed, ret = %d\n", __func__, ret);
		ret_value = -EHAL;
	}

	return ret_value;
}

static int pe20_set_ta_vchr(struct chg_alg_device *alg, u32 chr_volt)
{
	int ret = 0, ret_value = 0;
	int vchr_before, vchr_after, vchr_delta;
	const u32 sw_retry_cnt_max = 3;
	const u32 retry_cnt_max = 5;
	u32 sw_retry_cnt = 0, retry_cnt = 0;
	struct mtk_pe20 *pe2;

	pe2_dbg("%s: starts\n", __func__);
	pe2 = dev_get_drvdata(&alg->dev);

	do {
		vchr_before = pe2_hal_get_vbus(alg);
		ret = _pe20_set_ta_vchr(alg, chr_volt);
		vchr_after = pe2_hal_get_vbus(alg);

		vchr_delta = abs(vchr_after - chr_volt);

		/*
		 * It is successful if VBUS difference to target is
		 * less than 500mV.
		 */
		if (vchr_delta < 500000 && ret == 0) {
			pe2_dbg("%s: OK, vchr = (%d, %d), vchr_target = %dmV\n",
				__func__, vchr_before / 1000, vchr_after / 1000,
				chr_volt / 1000);
			return ret_value;
		}

		if (ret == 0 || sw_retry_cnt >= sw_retry_cnt_max)
			retry_cnt++;
		else
			sw_retry_cnt++;

		ret = pe2_hal_set_mivr(alg, CHG1, pe2->min_charger_voltage);
		if (ret < 0)
			pe2_err("%s:set mivr fail, ret:%d\n",
			__func__, ret);

		pe2_dbg("%s: retry_cnt = (%d, %d), vchr = (%d, %d), vchr_target = %dmV\n",
			__func__, sw_retry_cnt, retry_cnt, vchr_before / 1000,
			vchr_after / 1000, chr_volt / 1000);
		pe2_dbg("%s: %d %d %d %d\n", pe2->is_cable_out_occur,
			pe2_hal_get_charger_type(alg),
			retry_cnt,
			retry_cnt_max);

	} while (!pe2->is_cable_out_occur &&
		 pe2_hal_get_charger_type(alg) !=
		 POWER_SUPPLY_USB_TYPE_DCP &&
		 retry_cnt < retry_cnt_max);

	if (pe2->is_cable_out_occur)
		ret_value = -ECABLEOUT;
	else
		ret_value = -EHAL;

	pe2_dbg("%s: failed, vchr_org = %dmV, vchr_after = %dmV, target_vchr = %dmV\n",
		__func__, pe2->ta_vchr_org / 1000, vchr_after / 1000,
		chr_volt / 1000);

	return ret_value;
}


static int pe20_detect_ta(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe20 *pe2;

	pe2_dbg("%s: starts\n", __func__);

	pe2 = dev_get_drvdata(&alg->dev);
	pe2->ta_vchr_org = pe2_hal_get_vbus(alg);

	/* Disable OVP */
	ret = pe2_hal_enable_vbus_ovp(alg, false);
	if (ret < 0)
		goto err;
	if (abs(pe2->ta_vchr_org - 8500000) > 500000)
		ret = pe20_set_ta_vchr(alg, 8500000);
	else
		ret = pe20_set_ta_vchr(alg, 6500000);

	pe2_dbg("%s: ret:%d\n", __func__, ret);

	return ret;
err:
	pe2_hal_enable_vbus_ovp(alg, true);
	pe2_err("%s: failed, ret = %d\n", __func__, ret);
	return ret;
}

static int __pe2_check_charger(struct chg_alg_device *alg)
{
	int ret = 0, ret_value = 0;
	struct mtk_pe20 *pe2;
	int uisoc;

	pe2 = dev_get_drvdata(&alg->dev);
	uisoc = pe2_hal_get_uisoc(alg);

	pe2_dbg("%s uisoc:%d s:%d end:%d type:%d", __func__,
		uisoc,
		pe2->ta_start_battery_soc,
		pe2->ta_stop_battery_soc,
		pe2_hal_get_charger_type(alg));

	if (pe2_hal_get_charger_type(alg) !=
		POWER_SUPPLY_USB_TYPE_DCP) {
		ret_value = ALG_TA_NOT_SUPPORT;
		goto out;
	}

	if (pe2->is_cable_out_occur)
		goto out;

	if (uisoc < pe2->ta_start_battery_soc ||
		uisoc >= pe2->ta_stop_battery_soc) {
		ret_value = ALG_TA_CHECKING;
		goto out;
	}

	ret = pe2_reset_ta_vchr(alg);
	if (ret != 0)
		goto out;

	if (pe2->is_cable_out_occur)
		goto out;

	pe2_check_cable_impedance(alg);

	if (pe2->is_cable_out_occur)
		goto out;

	ret = pe20_detect_ta(alg);
	if (ret < 0)
		goto out;

	pe2_dbg("%s: OK, state = %d\n",
		__func__, pe2->state);

	return ret;
out:

	if (ret_value == 0)
		ret_value = ALG_TA_NOT_SUPPORT;
	pe2_dbg("%s:SOC:(%d,%d,%d),state:%d,chr_type:%d,ret:%d,plugout:%d\n",
		__func__,
		pe2_hal_get_uisoc(alg),
		pe2->ta_start_battery_soc,
		pe2->ta_stop_battery_soc,
		pe2->state,
		pe2_hal_get_charger_type(alg),
		ret,
		pe2->is_cable_out_occur);
	return ret_value;
}

static int pe2_leave(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe20 *pe2;

	pe2_dbg("%s: starts\n", __func__);
	pe2 = dev_get_drvdata(&alg->dev);

	ret = pe2_reset_ta_vchr(alg);
	if (ret != 0) {
		pe2_err("%s: failed, state = %d, ret = %d\n",
			__func__, pe2->state, ret);
	}

	ret = pe2_hal_enable_vbus_ovp(alg, true);
	if (ret != 0) {
		pe2_err("%s: enable vbus ovp fai,ret:%d\n",
			__func__, ret);
	}

	pe2_hal_set_mivr(alg, CHG1, pe2->min_charger_voltage);
	if (ret != 0) {
		pe2_err("%s:set mivr fail,ret:%d\n",
			__func__, ret);
	}

	pe2_dbg("%s: OK\n", __func__);
	return ret;
}

static int _pe2_init_algo(struct chg_alg_device *alg)
{
	struct mtk_pe20 *pe2;
	int ret;

	pe2 = dev_get_drvdata(&alg->dev);
	pe2_dbg("%s\n", __func__);

	mutex_lock(&pe2->access_lock);
	if (pe2_hal_init_hardware(alg) != 0) {
		pe2->state = PE2_HW_FAIL;
		pe2_err("%s:init hw fail\n", __func__);
	} else
		pe2->state = PE2_HW_READY;
	ret = pe2_hal_set_efficiency_table(pe2->alg);
	if (ret != 0)
		pe2_err("%s: use default table, %d\n", __func__, ret);
	mutex_unlock(&pe2->access_lock);
	return 0;
}

static char *pe2_state_to_str(int state)
{
	switch (state) {
	case PE2_HW_UNINIT:
		return "PE2_HW_UNINIT";
	case PE2_HW_FAIL:
		return "PE2_HW_FAIL";
	case PE2_HW_READY:
		return "PE2_HW_READY";
	case PE2_TA_NOT_SUPPORT:
		return "PE2_TA_NOT_SUPPORT";
	case PE2_RUN:
		return "PE2_RUN";
	case PE2_DONE:
		return "PE2_DONE";
	default:
		break;
	}
	pe2_err("%s unknown state:%d\n", __func__
		, state);
	return "PE2_UNKNOWN";
}

static int _pe2_is_algo_ready(struct chg_alg_device *alg)
{
	struct mtk_pe20 *pe2;
	int ret_value, uisoc;

	pe2 = dev_get_drvdata(&alg->dev);

	mutex_lock(&pe2->access_lock);
	__pm_stay_awake(pe2->suspend_lock);
	pe2_dbg("%s state:%s\n", __func__,
		pe2_state_to_str(pe2->state));

	switch (pe2->state) {
	case PE2_HW_UNINIT:
	case PE2_HW_FAIL:
		ret_value = ALG_INIT_FAIL;
		break;
	case PE2_HW_READY:

		uisoc = pe2_hal_get_uisoc(alg);

		if (pe2_hal_get_charger_type(alg) !=
			POWER_SUPPLY_USB_TYPE_DCP) {
			ret_value = ALG_TA_NOT_SUPPORT;
		} else if (uisoc < pe2->ta_start_battery_soc ||
			uisoc >= pe2->ta_stop_battery_soc) {
			ret_value = ALG_NOT_READY;
		} else {
			ret_value = ALG_READY;
		}
		break;
	case PE2_TA_NOT_SUPPORT:
		ret_value = ALG_TA_NOT_SUPPORT;
		break;
	case PE2_RUN:
		ret_value = ALG_RUNNING;
		break;
	case PE2_DONE:
		ret_value = ALG_DONE;
		break;
	default:
		ret_value = ALG_INIT_FAIL;
		break;
	}
	__pm_relax(pe2->suspend_lock);
	mutex_unlock(&pe2->access_lock);

	return ret_value;
}
static int __pe2_run(struct chg_alg_device *alg)
{
	struct mtk_pe20 *pe2;
	int i;
	int vbat, vbus, ichg;
	int pre_vbus, pre_idx;
	int tune = 0, pes = 0; /* For log, to know the state of PE+20 */
	u32 size;
	int ret = 0, ret_value = 0, vchr, uisoc;

	pe2 = dev_get_drvdata(&alg->dev);

	if (pe2->is_cable_out_occur) {
		ret_value = ALG_TA_NOT_SUPPORT;
		goto out;
	}

	vbat = pe2_hal_get_vbat(alg);
	vbus = pe2_hal_get_vbus(alg);
	ichg = pe2_hal_get_ibat(alg);

	pre_vbus = pe2->vbus;
	pre_idx = pe2->idx;


	/* PE+ leaves unexpectedly */
	vchr = pe2_hal_get_vbus(alg);
	if (abs(vchr - pe2->ta_vchr_org) < 1000000) {
		pe2_err("%s: PE+20 leave unexpectedly, recheck TA %d %d\n",
			__func__,
			vchr,
			pe2->ta_vchr_org);
		ret = pe2_leave(alg);
		pe2->ta_vchr_org = vchr;
		ret_value = ALG_TA_CHECKING;
		goto out;
	}

	ichg = pe2_hal_get_ibat(alg);
	/* Check SOC & Ichg */
	uisoc = pe2_hal_get_uisoc(alg);
	if (uisoc > pe2->ta_stop_battery_soc &&
		ichg > 0 && ichg < pe2->pe20_ichg_level_threshold) {
		ret = pe2_leave(alg);
		pe2_err("%s: OK, SOC = (%d,%d), stop PE+20\n", __func__,
			uisoc, pe2->ta_stop_battery_soc);
		ret_value = ALG_DONE;
		goto out;
	}

	if (pe2->charging_current_limit != -1 &&
		pe2->charging_current_limit <
		pe2->pe2_charger_current)
		pe2->charging_current = pe2->charging_current_limit;
	else
		pe2->charging_current = pe2->pe2_charger_current;

	if (pe2->input_current_limit != -1 &&
		pe2->input_current_limit <
		pe2->pe2_input_current)
		pe2->input_current = pe2->input_current_limit;
	else
		pe2->input_current = pe2->pe2_input_current;

	pe2_hal_set_charging_current(alg,
		CHG1, pe2->input_current);
	pe2_hal_set_input_current(alg,
		CHG1, pe2->charging_current);
	pe2_hal_set_cv(alg,
		CHG1, pe2->cv);

	pe2_dbg("%s cv:%d icl:%d cc:%d\n", __func__,
		pe2->cv,
		pe2->input_current,
		pe2->charging_current);


	size = ARRAY_SIZE(pe2->profile);
	for (i = 0; i < size; i++) {
		tune = 0;

		/* Exceed this level, check next level */
		if (vbat > (pe2->profile[i].vbat + 100000))
			continue;

		/* If vbat is still 30mV larger than the lower level
		 * Do not down grade
		 */
		if (i < pe2->idx && vbat > (pe2->profile[i].vbat + 30000))
			continue;

		if (pe2->vbus != pe2->profile[i].vchr)
			tune = 1;

		pe2->vbus = pe2->profile[i].vchr;
		pe2->idx = i;

		if (abs(vbus - pe2->vbus) >= 1000000)
			tune = 2;

		if (tune != 0) {
			ret = pe20_set_ta_vchr(alg, pe2->vbus);
			if (ret == 0)
				pe2_hal_set_mivr(alg, CHG1, pe2->vbus - 500000);
			else {
				pe2_leave(alg);
				ret_value = ALG_TA_NOT_SUPPORT;
			}
		} else
			pe2_hal_set_mivr(alg, CHG1, pe2->vbus - 500000);
		break;
	}
	pes = 2;

out:

	pe2_dbg("%s: vbus = (%d, %d), idx = (%d, %d), I = %d plugout=%d\n",
		__func__, pre_vbus / 1000, pe2->vbus / 1000, pre_idx,
		pe2->idx, ichg / 1000, pe2->is_cable_out_occur);

	pe2_dbg("%s: SOC = %d, state = %s, tune = %d, pes = %d, vbat = %d, ret = %d:%d\n",
		__func__, pe2_hal_get_uisoc(alg),
		pe2_state_to_str(pe2->state),
		tune, pes,
		vbat / 1000, ret, ret_value);
	return ret_value;
}

static int _pe2_start_algo(struct chg_alg_device *alg)
{
	int ret = 0, ret_value = 0;
	struct mtk_pe20 *pe2;
	bool again;

	pe2 = dev_get_drvdata(&alg->dev);
	mutex_lock(&pe2->access_lock);
	__pm_stay_awake(pe2->suspend_lock);

	do {
		pe2_info("%s state:%d %s %d\n", __func__,
			pe2->state,
			pe2_state_to_str(pe2->state),
			again);

		again = false;
		switch (pe2->state) {
		case PE2_HW_UNINIT:
		case PE2_HW_FAIL:
			ret_value = ALG_INIT_FAIL;
			break;
		case PE2_HW_READY:
			ret = __pe2_check_charger(alg);
			if (ret == 0) {
				pe2->state = PE2_RUN;
				ret_value = ALG_READY;
				again = true;
			} else if (ret == ALG_TA_CHECKING)
				ret_value = ALG_TA_CHECKING;
			else {
				pe2->state = PE2_TA_NOT_SUPPORT;
				ret_value = ALG_TA_NOT_SUPPORT;
			}
			break;
		case PE2_TA_NOT_SUPPORT:
			ret_value = ALG_TA_NOT_SUPPORT;
			break;
		case PE2_RUN:
			ret = __pe2_run(alg);
			if (ret == ALG_TA_NOT_SUPPORT)
				pe2->state = PE2_TA_NOT_SUPPORT;
			else if (ret == ALG_TA_CHECKING) {
				pe2->state = PE2_RUN;
				again = true;
			} else if (ret == ALG_DONE)
				pe2->state = PE2_DONE;
			ret_value = ret;
			break;
		case PE2_DONE:
			ret_value = ALG_DONE;
			break;
		default:
			pe2_err("PE2 unknown state:%d\n", pe2->state);
			ret_value = ALG_INIT_FAIL;
			break;
		}
	} while (again == true);
	__pm_relax(pe2->suspend_lock);
	mutex_unlock(&pe2->access_lock);

	return ret_value;
}


static bool _pe2_is_algo_running(struct chg_alg_device *alg)
{
	struct mtk_pe20 *pe2;

	pe2_dbg("%s\n", __func__);
	pe2 = dev_get_drvdata(&alg->dev);

	if (pe2->state == PE2_RUN)
		return true;

	return false;
}

static int _pe2_stop_algo(struct chg_alg_device *alg)
{
	struct mtk_pe20 *pe2;

	pe2 = dev_get_drvdata(&alg->dev);

	pe2_dbg("%s %d\n", __func__, pe2->state);
	if (pe2->state == PE2_RUN) {
		pe2_reset_ta_vchr(alg);
		pe2->state = PE2_HW_READY;
	}

	return 0;
}

static int _pe2_notifier_call(struct chg_alg_device *alg,
			 struct chg_alg_notify *notify)
{
	struct mtk_pe20 *pe2;
	int ret = 0;

	pe2 = dev_get_drvdata(&alg->dev);
	pe2_err("%s evt:%d\n", __func__, notify->evt);

	switch (notify->evt) {
	case EVT_PLUG_OUT:
		pe2_plugout_reset(alg);
		break;
	case EVT_FULL:
		if (pe2->state == PE2_RUN) {
			pe2_err("%s evt full\n",  __func__);
			pe2_leave(alg);
			pe2->state = PE2_DONE;
		}
		break;
	case EVT_RECHARGE:
		if (pe2->state == PE2_DONE) {
			pe2_err("%s evt recharge\n",  __func__);
			pe2->state = PE2_HW_READY;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void mtk_pe2_parse_dt(struct mtk_pe20 *pe2,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;

	/* PE 2.0 */
	if (of_property_read_u32(np, "pe20_ichg_level_threshold", &val) >= 0)
		pe2->pe20_ichg_level_threshold = val;
	else {
		pr_notice("use default PE20_ICHG_LEAVE_THRESHOLD:%d\n",
			PE20_ICHG_LEAVE_THRESHOLD);
		pe2->pe20_ichg_level_threshold =
						PE20_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "ta_start_battery_soc", &val) >= 0)
		pe2->ta_start_battery_soc = val;
	else {
		pr_notice("use default TA_START_BATTERY_SOC:%d\n",
			TA_START_BATTERY_SOC);
		pe2->ta_start_battery_soc = TA_START_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "ta_stop_battery_soc", &val) >= 0)
		pe2->ta_stop_battery_soc = val;
	else {
		pr_notice("use default TA_STOP_BATTERY_SOC:%d\n",
			TA_STOP_BATTERY_SOC);
		pe2->ta_stop_battery_soc = TA_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		pe2->min_charger_voltage = val;
	else {
		pr_notice("use default V_CHARGER_MIN:%d\n", PE20_V_CHARGER_MIN);
		pe2->min_charger_voltage = PE20_V_CHARGER_MIN;
	}

	/* cable measurement impedance */
	if (of_property_read_u32(np, "cable_imp_threshold", &val) >= 0)
		pe2->cable_imp_threshold = val;
	else {
		pr_notice("use default CABLE_IMP_THRESHOLD:%d\n",
			PE2_CABLE_IMP_THRESHOLD);
		pe2->cable_imp_threshold = PE2_CABLE_IMP_THRESHOLD;
	}

	if (of_property_read_u32(np, "vbat_cable_imp_threshold", &val) >= 0)
		pe2->vbat_cable_imp_threshold = val;
	else {
		pr_notice("use default VBAT_CABLE_IMP_THRESHOLD:%d\n",
			PE2_VBAT_CABLE_IMP_THRESHOLD);
		pe2->vbat_cable_imp_threshold = PE2_VBAT_CABLE_IMP_THRESHOLD;
	}

	if (of_property_read_u32(np, "pe2_input_current", &val) >= 0)
		pe2->pe2_input_current = val;
	else {
		pr_notice("use default PE2_INPUT_CURRENT:%d\n",
			PE2_INPUT_CURRENT);
		pe2->pe2_input_current = PE2_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "pe2_charger_current", &val) >= 0)
		pe2->pe2_charger_current = val;
	else {
		pr_notice("use default pe2_charger_current:%d\n",
			PE2_CHARGING_CURRENT);
		pe2->pe2_charger_current = PE2_CHARGING_CURRENT;
	}

	if (of_property_read_u32(np, "pe2_slave_mivr_diff", &val) >= 0)
		pe2->pe2_slave_mivr_diff = val;
	else {
		pr_notice("use default PE2_SLAVE_MIVR_DIFF:%d\n",
			PE2_SLAVE_MIVR_DIFF);
		pe2->pe2_slave_mivr_diff = PE2_SLAVE_MIVR_DIFF;
	}


}

int _pe2_get_status(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value)
{

	pr_notice("%s\n", __func__);
	if (s == ALG_MAX_VBUS)
		*value = 10000;
	else
		pr_notice("%s does not support prop:%d\n", __func__, s);
	return 0;
}

int _pe2_set_setting(struct chg_alg_device *alg_dev,
	struct chg_alg_setting *setting)
{
	struct mtk_pe20 *pe2;

	pe2_dbg("%s cv:%d icl:%d cc:%d\n",
		__func__,
		setting->cv,
		setting->input_current_limit,
		setting->charging_current_limit);
	pe2 = dev_get_drvdata(&alg_dev->dev);

	mutex_lock(&pe2->access_lock);
	__pm_stay_awake(pe2->suspend_lock);
	pe2->cv = setting->cv;
	pe2->input_current_limit = setting->input_current_limit;
	pe2->charging_current_limit = setting->charging_current_limit;
	__pm_relax(pe2->suspend_lock);
	mutex_unlock(&pe2->access_lock);

	return 0;
}

static struct chg_alg_ops pe2_alg_ops = {
	.init_algo = _pe2_init_algo,
	.is_algo_ready = _pe2_is_algo_ready,
	.start_algo = _pe2_start_algo,
	.is_algo_running = _pe2_is_algo_running,
	.stop_algo = _pe2_stop_algo,
	.notifier_call = _pe2_notifier_call,
	.get_status = _pe2_get_status,
	.set_setting = _pe2_set_setting,
};

static int mtk_pe2_probe(struct platform_device *pdev)
{
	struct mtk_pe20 *pe2 = NULL;

	pr_notice("%s: starts\n", __func__);

	pe2 = devm_kzalloc(&pdev->dev, sizeof(*pe2), GFP_KERNEL);
	if (!pe2)
		return -ENOMEM;
	platform_set_drvdata(pdev, pe2);
	pe2->pdev = pdev;

	pe2->suspend_lock =
		wakeup_source_register(NULL, "PE+20 suspend wakelock");

	mutex_init(&pe2->access_lock);
	mutex_init(&pe2->cable_out_lock);

	pe2->ta_vchr_org = 5000000;
	pe2->idx = -1;
	pe2->vbus = 5000000;
	pe2->state = PE2_HW_UNINIT;
	mtk_pe2_parse_dt(pe2, &pdev->dev);

	pe2->profile[0].vbat = 3400000;
	pe2->profile[1].vbat = 3500000;
	pe2->profile[2].vbat = 3600000;
	pe2->profile[3].vbat = 3700000;
	pe2->profile[4].vbat = 3800000;
	pe2->profile[5].vbat = 3900000;
	pe2->profile[6].vbat = 4000000;
	pe2->profile[7].vbat = 4100000;
	pe2->profile[8].vbat = 4200000;
	pe2->profile[9].vbat = 4300000;

	/*
	pe2->profile[0].vchr = 8000000;
	pe2->profile[1].vchr = 8500000;
	pe2->profile[2].vchr = 8500000;
	pe2->profile[3].vchr = 9000000;
	pe2->profile[4].vchr = 9000000;
	pe2->profile[5].vchr = 9000000;
	pe2->profile[6].vchr = 9500000;
	pe2->profile[7].vchr = 9500000;
	pe2->profile[8].vchr = 10000000;
	pe2->profile[9].vchr = 10000000;
	*/
	pe2->profile[0].vchr = 8000000;
	pe2->profile[1].vchr = 8000000;
	pe2->profile[2].vchr = 8000000;
	pe2->profile[3].vchr = 8500000;
	pe2->profile[4].vchr = 8500000;
	pe2->profile[5].vchr = 8500000;
	pe2->profile[6].vchr = 9000000;
	pe2->profile[7].vchr = 9000000;
	pe2->profile[8].vchr = 9500000;
	pe2->profile[9].vchr = 9500000;

	pe2->alg = chg_alg_device_register("pe2", &pdev->dev,
					pe2, &pe2_alg_ops, NULL);

	return 0;
}

static int mtk_pe2_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_pe2_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id mtk_pe2_of_match[] = {
	{.compatible = "mediatek,charger,pe2",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pe2_of_match);

struct platform_device pe2_device = {
	.name = "pe2",
	.id = -1,
};

static struct platform_driver pe2_driver = {
	.probe = mtk_pe2_probe,
	.remove = mtk_pe2_remove,
	.shutdown = mtk_pe2_shutdown,
	.driver = {
		   .name = "pe2",
		   .of_match_table = mtk_pe2_of_match,
	},
};

static int __init mtk_pe2_init(void)
{
	return platform_driver_register(&pe2_driver);
}
late_initcall(mtk_pe2_init);

static void __exit mtk_pe2_exit(void)
{
	platform_driver_unregister(&pe2_driver);
}
module_exit(mtk_pe2_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Pump Express 2 algorithm Driver");
MODULE_LICENSE("GPL");

