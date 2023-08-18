// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_pe4.c
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

#include "mtk_pe4_5.h"
#include "mtk_charger_algorithm_class.h"

#define PE45_VBUS_STEP 50
#define PE45_MIN_WATT 5000000
#define PE45_VBUS_IR_DROP_THRESHOLD 1200
#define PE45_MEASURE_R_AVG_TIMES	10

static int pe4_dbg_level = PE4_DEBUG_LEVEL;

static bool algo_waiver_test;
module_param(algo_waiver_test, bool, 0644);

int pe4_get_debug_level(void)
{
	return pe4_dbg_level;
}

void mtk_pe45_reset(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe45;

	pe45 = dev_get_drvdata(&alg->dev);

	if (pe45->state == PE4_RUN || pe45->state == PE4_INIT ||
	    pe45->state == PE4_TUNING || pe45->state == PE4_POSTCC) {
		pe4_hal_set_adapter_cap_end(alg, 5000, 2000);

		pe4_hal_set_mivr(alg, CHG1, pe45->min_charger_voltage);
		pe4_hal_enable_vbus_ovp(alg, true);
		pe45->polling_interval = 10;
		pe45->state = PE4_HW_READY;
		pe4_dbg("set TD true\n");
		pe4_hal_enable_termination(alg, CHG1, true);
		if (alg->config == DUAL_CHARGERS_IN_SERIES) {
			pe4_hal_enable_charger(alg, CHG2, false);
			pe4_hal_charger_enable_chip(alg, CHG2, false);
		}
	}


	pe4_hal_vbat_mon_en(alg, CHG1, false);
	pe45->old_cv = 0;
	pe45->stop_6pin_re_en = 0;
	pe45->cap.nr = 0;
	pe45->pe4_input_current_limit = -1;
	pe45->pe4_input_current_limit_setting = -1;
	pe45->max_vbus = pe45->pe45_max_vbus;
	pe45->max_ibus = pe45->pe45_max_ibus;
	pe45->max_charger_ibus = pe45->pe45_max_ibus *
				(100 - pe45->ibus_err) / 100;
}


static int _pe4_init_algo(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;
	int cnt, log_level;

	pe4 = dev_get_drvdata(&alg->dev);

	mutex_lock(&pe4->access_lock);
	if (pe4_hal_init_hardware(alg) != 0) {
		pe4->state = PE4_HW_FAIL;
		pe4_err("%s:init hw fail\n", __func__);
	} else
		pe4->state = PE4_HW_READY;
	mtk_pe45_reset(alg);

	if (alg->config == DUAL_CHARGERS_IN_PARALLEL) {
		pe4_err("%s does not support DUAL_CHARGERS_IN_PARALLEL\n",
			__func__);
		alg->config = SINGLE_CHARGER;
	} else if (alg->config == DUAL_CHARGERS_IN_SERIES) {
		cnt = pe4_hal_get_charger_cnt(alg);
		if (cnt == 2)
			alg->config = DUAL_CHARGERS_IN_SERIES;
		else
			alg->config = SINGLE_CHARGER;
	} else
		alg->config = SINGLE_CHARGER;

	log_level = pe4_hal_get_log_level(alg);
	pr_notice("%s: log_level=%d", __func__, log_level);
	if (log_level > 0)
		pe4_dbg_level = log_level;

	mutex_unlock(&pe4->access_lock);
	return 0;
}

static char *pe4_state_to_str(int state)
{
	switch (state) {
	case PE4_HW_UNINIT:
		return "PE4_HW_UNINIT";
	case PE4_HW_FAIL:
		return "PE4_HW_FAIL";
	case PE4_HW_READY:
		return "PE4_HW_READY";
	case PE4_TA_NOT_SUPPORT:
		return "PE4_TA_NOT_SUPPORT";
	case PE4_RUN:
		return "PE4_RUN";
	case PE4_TUNING:
		return "PE4_TUNING";
	case PE4_POSTCC:
		return "PE4_POSTCC";
	case PE4_INIT:
		return "PE4_INIT";
	default:
		break;
	}
	pe4_err("%s unknown state:%d\n", __func__
		, state);
	return "PE4_UNKNOWN";
}

static int _pe4_is_algo_ready(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;
	int ret_value, uisoc;
	int ret, tmp;
	int oldstate;

	pe4 = dev_get_drvdata(&alg->dev);

	mutex_lock(&pe4->access_lock);
	__pm_stay_awake(pe4->suspend_lock);
	oldstate = pe4->state;

	if (algo_waiver_test) {
		ret_value = ALG_WAIVER;
		goto skip;
	}

	switch (pe4->state) {
	case PE4_HW_UNINIT:
	case PE4_HW_FAIL:
		ret_value = ALG_INIT_FAIL;
		break;
	case PE4_INIT:
	case PE4_HW_READY:
		uisoc = pe4_hal_get_uisoc(alg);
		ret = pe4_hal_is_pd_adapter_ready(alg);
		ret_value = ret;
		if (ret == ALG_READY) {
			tmp = pe4_hal_get_battery_temperature(alg);
			pe4_dbg("c:%d,%d uisoc:%d,%d tmp:%d,%d,%d ref_vbat:%d\n",
				pe4->input_current_limit1,
				pe4->charging_current_limit1,
				uisoc,
				pe4->pe45_stop_battery_soc,
				tmp,
				pe4->high_temp_to_enter_pe45,
				pe4->low_temp_to_enter_pe45,
				pe4->ref_vbat);
			if (pe4->input_current_limit1 != -1 ||
				pe4->charging_current_limit1 != -1 ||
				pe4->input_current_limit2 != -1 ||
				pe4->charging_current_limit2 != -1 ||
				tmp > pe4->high_temp_to_enter_pe45 ||
				tmp < pe4->low_temp_to_enter_pe45) {
				ret_value = ALG_NOT_READY;
			} else if ((uisoc == -1 && pe4->ref_vbat > pe4->vbat_threshold) ||
					uisoc > pe4->pe45_stop_battery_soc) {
				ret_value = ALG_WAIVER;
			}
		} else if (ret == ALG_TA_NOT_SUPPORT)
			pe4->state = PE4_TA_NOT_SUPPORT;
		break;
	case PE4_TA_NOT_SUPPORT:
		ret_value = ALG_TA_NOT_SUPPORT;
		break;
	case PE4_RUN:
	case PE4_TUNING:
	case PE4_POSTCC:
		ret_value = ALG_RUNNING;
		break;
	default:
		ret_value = ALG_INIT_FAIL;
		break;
	}
skip:
	pe4_dbg("%s state:%s=>%s ret:%d\n", __func__,
		pe4_state_to_str(oldstate),
		pe4_state_to_str(pe4->state),
		ret_value);


	__pm_relax(pe4->suspend_lock);
	mutex_unlock(&pe4->access_lock);

	return ret_value;
}

void mtk_pe45_init_cap(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);
	pe4_hal_get_adapter_cap(alg, &pe4->cap);
}

int mtk_pe45_get_setting_by_watt(struct chg_alg_device *alg, int *voltage,
	int *adapter_ibus, int *actual_current, int watt,
	int *ibus_current_setting)
{
	unsigned int i = 0;
	struct mtk_pe45 *pe45;
	struct pe4_power_cap *pe45_cap;
	int vbus = 0, ibus = 0, ibus_setting = 0;
	int idx = 0, ta_ibus = 0;

	pe45 = dev_get_drvdata(&alg->dev);

	pe45_cap = &pe45->cap;

	pe4_dbg("%s cv:%d icl:%d:%d:%d:%d, watt:%d pdp:%d,%d\n",
		__func__,
		pe45->cv,
		pe45->input_current_limit1,
		pe45->input_current_limit2,
		pe45->pe4_input_current_limit,
		pe45->pe4_input_current_limit_setting,
		watt,
		pe45_cap->pwr_limit[i],
		pe45_cap->pdp);


	for (i = 0; i < pe45_cap->nr; i++) {
		int max_ibus = 0;
		int max_vbus = 0;

		/* update upper bound */
		if (pe45_cap->ma[i] > pe45->max_ibus)
			max_ibus = pe45->max_ibus;
		else
			max_ibus = pe45_cap->ma[i];
		pe4_dbg("1.%d %d %d %d\n",
			pe45_cap->ma[i],
			pe45->max_ibus,
			max_ibus,
			pe45->pe45_max_ibus);

		if (pe45->input_current_limit1 != -1 &&
			max_ibus > pe45->input_current_limit1 / 1000)
			max_ibus = pe45->input_current_limit1 / 1000;

		pe4_dbg("2.%d %d\n",
			pe45->input_current_limit1,
			max_ibus);

		if (pe45->pe4_input_current_limit != -1 &&
			max_ibus > (pe45->pe4_input_current_limit / 1000))
			max_ibus = pe45->pe4_input_current_limit / 1000;

		pe4_dbg("3.%d %d\n",
			pe45->pe4_input_current_limit,
			max_ibus);


		pe45->max_charger_ibus = max_ibus *
					(100 - pe45->ibus_err) / 100;

		pe4_dbg("idx:%d nr:%d mV:%d:%d mA:%d\n",
			i,
			pe45_cap->nr,
			pe45_cap->max_mv[i],
			pe45_cap->min_mv[i],
			pe45_cap->ma[i]);

		pe4_dbg("ibus:%d %d err:%d\n",
			pe45->max_charger_ibus,
			max_ibus,
			pe45->ibus_err);


		if (pe45_cap->max_mv[i] > pe45->max_vbus)
			max_vbus =  pe45->max_vbus;
		else
			max_vbus = pe45_cap->max_mv[i];

		if (*voltage != 0 && *voltage <= max_vbus &&
			*voltage >= pe45_cap->min_mv[i]) {
			ibus = watt / *voltage;
			vbus = *voltage;
			ibus_setting = max_ibus;
			ta_ibus = pe45_cap->ma[i];
			if (ibus <= max_ibus) {
				idx = 1;
				break;
			}
		}

		/* is 5v ok ? */
		if (max_vbus >= 5000 &&
			pe45_cap->min_mv[i] <= 5000 &&
			5000 * pe45->max_charger_ibus >= watt) {
			vbus = 5000;
			ibus = watt / 5000;
			ibus_setting = max_ibus;
			ta_ibus = pe45_cap->ma[i];
			idx = 2;
			break;
		}

		/* is power limit set */
		if (pe45_cap->pwr_limit[i] && pe45_cap->pdp > 0) {
			if (watt > pe45_cap->pdp * 1000000)
				watt = pe45_cap->pdp * 1000000;

			if (max_vbus * (pe45->max_charger_ibus - 200) >= watt) {
				ibus = pe45->max_charger_ibus - 200;
				vbus = watt / ibus;
				ibus_setting = max_ibus;
				ta_ibus = pe45_cap->ma[i];
				if (vbus > max_vbus)
					vbus = max_vbus;
				if (vbus < pe45_cap->min_mv[i])
					vbus = pe45_cap->min_mv[i];

				idx = 4;
				break;
			}
		}

		/* is max watt ok */
		if (max_vbus * (pe45->max_charger_ibus - 200) >= watt &&
			!pe45_cap->pwr_limit[i]) {
			ibus = pe45->max_charger_ibus - 200;
			vbus = watt / ibus;
			ibus_setting = max_ibus;
			ta_ibus = pe45_cap->ma[i];
			if (vbus < pe45_cap->min_mv[i])
				vbus = pe45_cap->min_mv[i];

			idx = 3;
			break;
		}

		vbus = max_vbus;
		ibus = pe45->max_charger_ibus;
		ibus_setting = max_ibus;
		ta_ibus = pe45_cap->ma[i];
		idx = 5;

	}

	*voltage = vbus;
	*ibus_current_setting = ibus_setting;
	*actual_current = ibus;
	*adapter_ibus = ta_ibus;

	pe4_dbg("%s:[%d,%d]%d vbus:%d ibus:%d aicl:%d current:%d %d\n",
		__func__,
		idx, i,
		watt, *voltage,
		*adapter_ibus,
		*ibus_current_setting,
		ibus, pe45->max_charger_ibus);

	return idx;
}

int mtk_pe45_pd_1st_request(struct chg_alg_device *alg,
	int adapter_mv, int adapter_ma, int ma)
{
	unsigned int oldmA = 3000000;
	int ret;
	int mivr;
//	bool chg2_enable = false;
	struct mtk_pe45 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (is_dual_charger_supported(pinfo))
		charger_dev_is_enabled(pinfo->chg2_dev, &chg2_enable);
#endif

	mivr = pe4->min_charger_voltage / 1000;
	pe4_hal_set_mivr(alg, CHG1, pe4->min_charger_voltage);
	pe4_hal_get_input_current(alg, CHG1, &oldmA);
	oldmA = oldmA / 1000;

	pe4_dbg("pe45_pd_req:vbus:%d ibus:%d input_current:%d %d\n",
		adapter_mv, adapter_ma, ma, oldmA);

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (pinfo->data.parallel_vbus && (oldmA * 2 > ma)) {
		if (chg2_enable) {
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000 / 2);
			charger_dev_set_input_current(pinfo->chg2_dev,
				ma * 1000 / 2);
		} else
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000);
	} else if (pinfo->data.parallel_vbus == false && (oldmA > ma))
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);
#else
	if (oldmA > ma)
		pe4_hal_set_input_current(alg, CHG1, ma * 1000);
#endif

	ret = pe4_hal_1st_set_adapter_cap(alg, adapter_mv, adapter_ma);

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (pinfo->data.parallel_vbus && (oldmA * 2 < ma)) {
		if (chg2_enable) {
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000 / 2);
			charger_dev_set_input_current(pinfo->chg2_dev,
				ma * 1000 / 2);
		} else
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000);
	} else if (pinfo->data.parallel_vbus == false && (oldmA < ma))
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);
#else
	if (oldmA < ma)
		pe4_hal_set_input_current(alg, CHG1, ma * 1000);
#endif

	if ((adapter_mv - PE45_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE45_VBUS_IR_DROP_THRESHOLD;

	pe4_hal_set_mivr(alg, CHG1, mivr * 1000);
	pe4->pe4_input_current_limit_setting = ma * 1000;
	return ret;
}

int mtk_pe45_pd_request(struct chg_alg_device *alg,
	int *adapter_vbus, int *adapter_ibus, int ma)
{
	unsigned int oldmA = 3000000;
	unsigned int oldmivr = 4600;
	int ret;
	int mivr;
	int adapter_mv, adapter_ma;
	struct mtk_pe45 *pe45;

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	bool chg2_enable = false;

	if (is_dual_charger_supported(pinfo))
		charger_dev_is_enabled(pinfo->chg2_dev, &chg2_enable);
#endif

	pe45 = dev_get_drvdata(&alg->dev);
	adapter_mv = *adapter_vbus;
	adapter_ma = *adapter_ibus;

	pe4_hal_get_mivr(alg, CHG1, &oldmivr);

	mivr = pe45->min_charger_voltage / 1000;
	pe4_hal_set_mivr(alg, CHG1, pe45->min_charger_voltage);

	pe4_hal_get_input_current(alg, CHG1, &oldmA);
	oldmA = oldmA / 1000;

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (pinfo->data.parallel_vbus && (oldmA * 2 > ma)) {
		if (chg2_enable) {
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000 / 2);
			charger_dev_set_input_current(pinfo->chg2_dev,
				ma * 1000 / 2);
		} else
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000);
	} else if (pinfo->data.parallel_vbus == false && (oldmA > ma))
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);
#else
	if (oldmA > ma)
		pe4_hal_set_input_current(alg, CHG1, ma * 1000);
#endif

	if (pe45->cap.pdp > 0 &&
		adapter_mv * adapter_ma > pe45->cap.pdp * 1000000) {
		*adapter_ibus = pe45->cap.pdp * 1000000 / adapter_mv;
		if (oldmA > *adapter_ibus)
			pe4_hal_set_input_current(alg, CHG1, *adapter_ibus * 1000);
	}

	ret = pe4_hal_set_adapter_cap(alg, adapter_mv, *adapter_ibus);

	pe4_dbg("%s: vbus:%d ibus:%d ibus2:%d input_current:%d pdp:%d ret:%d\n",
		__func__, adapter_mv, adapter_ma, *adapter_ibus, ma,
				pe45->cap.pdp, ret);

	if (ret == MTK_ADAPTER_PE4_REJECT) {
		pe4_err("pe45_pd_req: reject\n");
			goto err;
	}

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (pinfo->data.parallel_vbus && (oldmA * 2 < ma)) {
		if (chg2_enable) {
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000 / 2);
			charger_dev_set_input_current(pinfo->chg2_dev,
				ma * 1000 / 2);
		} else
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000);
	} else if (pinfo->data.parallel_vbus == false && (oldmA < ma))
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);
#else
	if (pe45->cap.pdp > 0 &&
		adapter_mv * adapter_ma > pe45->cap.pdp * 1000000)
		pe4_hal_set_input_current(alg, CHG1, *adapter_ibus * 1000);
	else if (oldmA < ma)
		pe4_hal_set_input_current(alg, CHG1, ma * 1000);
#endif

	if ((adapter_mv - PE45_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE45_VBUS_IR_DROP_THRESHOLD;

	pe4_hal_set_mivr(alg, CHG1, mivr * 1000);

	pe45->pe4_input_current_limit_setting = ma * 1000;
	return ret;

err:
#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (pinfo->data.parallel_vbus && (oldmA * 2 > ma)) {
		if (chg2_enable) {
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000 / 2);
			charger_dev_set_input_current(pinfo->chg2_dev,
				ma * 1000 / 2);
		} else
			charger_dev_set_input_current(pinfo->chg1_dev,
				ma * 1000);
	} else if (pinfo->data.parallel_vbus == false && (oldmA > ma))
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);
#else
	pe4_hal_set_input_current(alg, CHG1, ma * 1000);
#endif

	pe4_hal_set_mivr(alg, CHG1, oldmivr);
	return ret;
}

int mtk_pe45_get_ibus(struct chg_alg_device *alg, u32 *ibus)
{
#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	int ret = 0;
	unsigned int chg1_ibus = 0;
	unsigned int chg2_ibus = 0;
	int ibat = 0;
	int chg1_ibat = 0;
	int chg2_ibat = 0;
	int chg2_watt = 0;
	bool is_enable = false;

	if (is_dual_charger_supported(pinfo) == true)
		charger_dev_is_enabled(pinfo->chg2_dev, &is_enable);
#endif

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (pinfo->data.parallel_vbus) {
		ret = charger_dev_get_ibus(pinfo->chg1_dev, &chg1_ibus);

		if (is_enable) {
			ret = charger_dev_get_ibat(pinfo->chg1_dev, &chg1_ibat);
			if (ret < 0)
				pe4_err("[%s] get ibat fail\n", __func__);

			ret = charger_dev_get_ibat(pinfo->chg2_dev, &chg2_ibat);
			if (ret < 0) {
				ibat = battery_get_bat_current();
				chg2_ibat = ibat * 100 - chg1_ibat;
			}

			if (ibat < 0 || chg2_ibat < 0)
				chg2_watt = 0;
			else
				chg2_watt = chg2_ibat / 1000 *
					battery_get_bat_voltage();

			chg2_ibus = chg2_watt / battery_get_vbus() * 1000;
		}
		*ibus = chg1_ibus + chg2_ibus;

		pe4_dbg("[%s] chg2_watt:%d ibat2:%d ibat1:%d ibat:%d ibus1:%d ibus2:%d ibus:%d\n",
			__func__, chg2_watt, chg2_ibat, chg1_ibat, ibat * 100,
			chg1_ibus, chg2_ibus, *ibus);
	} else {
		ret = charger_dev_get_ibus(pinfo->chg1_dev, ibus);
	}
#endif
	pe4_hal_get_ibus(alg, ibus);

	return 0;
}

int mtk_pe45_rcable_control_chg_level(struct chg_alg_device *alg, int *vbus, int *ibus,
									int *adapter_ibus)
{
	int new_watt = 0;
	struct mtk_pe45 *pe45;
	struct pe4_power_cap *pe45_cap;
	bool chg1_mivr = false;

	pe45 = dev_get_drvdata(&alg->dev);
	pe45_cap = &pe45->cap;
	pe4_hal_get_mivr_state(alg, CHG1, &chg1_mivr);

	if (chg1_mivr) {
		if (pe45->rcable_index < MAX_RCABLE_INDEX - 1)
			pe45->rcable_index = pe45->rcable_index + 1;
		pe45->mivr_count = pe45->mivr_count + 1;
	}

	if (*vbus > pe45->r_cable_voltage[pe45->rcable_index] || chg1_mivr)
		*vbus = pe45->r_cable_voltage[pe45->rcable_index];
	if (pe45->max_vbus > pe45->r_cable_voltage[pe45->rcable_index] || chg1_mivr)
		pe45->max_vbus = pe45->r_cable_voltage[pe45->rcable_index];
	if (pe45->input_current_limit1 > pe45->r_cable_current_limit[pe45->rcable_index] * 1000)
		pe45->input_current_limit1 = pe45->r_cable_current_limit[pe45->rcable_index] * 1000;
	if (*ibus > pe45->r_cable_current_limit[pe45->rcable_index])
		*ibus = pe45->r_cable_current_limit[pe45->rcable_index];
	*adapter_ibus = *ibus;
	new_watt = (*vbus) * (*ibus);
	pe4_hal_set_input_current(alg, CHG1, pe45->input_current_limit1);
	pe4_dbg("%s: idx: %d, vbus: %d, ibus: %d, rcable: %d, watt: %d, mivr_count: %d\n",
		__func__, pe45->rcable_index, *vbus, *ibus,
		pe45->r_cable_1, new_watt, pe45->mivr_count);

	return new_watt;
}

int mtk_pe45_get_init_lower_watt(struct chg_alg_device *alg)
{
	int ret;
	struct mtk_pe45 *pe45;
	int vbus1, ibus1;
	int vbat1;
	int voltage = 0, input_current = 1000, actual_current = 0;
	int voltage1 = 0, adapter_ibus;
	unsigned int i;

	pe45 = dev_get_drvdata(&alg->dev);
	voltage = 0;
	mtk_pe45_get_setting_by_watt(alg, &voltage, &adapter_ibus,
		&actual_current, 15000000, &input_current);
	mtk_pe45_rcable_control_chg_level(alg, &voltage, &input_current, &adapter_ibus);
	ret = mtk_pe45_pd_request(alg, &voltage, &adapter_ibus,
				input_current);
	pe4_err("%s: voltage:%d, adapter_ibus: %d\n", __func__, voltage, adapter_ibus);
	if (ret != 0 && ret != MTK_ADAPTER_PE4_REJECT) {
		pe4_err("%s: err:1 %d\n", __func__, ret);
		return -1;
	}

	for (i = 0; i < 3 ; i++) {
		pe4_hal_dump_registers(alg);
		msleep(100);
	}

	mtk_pe45_get_ibus(alg, &ibus1);
	vbus1 = pe4_hal_get_vbus(alg);
	ibus1 = ibus1 / 1000;
	vbat1 = pe4_hal_get_vbat(alg);
	voltage1 = voltage;

	return voltage1 * ibus1;
}

int mtk_pe45_get_init_watt(struct chg_alg_device *alg)
{
	int ret;
	struct mtk_pe45 *pe45;
	int vbus1, ibus1;
	int vbus2, ibus2;
	int vbat1, vbat2;
	int voltage = 0, input_current = 1000, actual_current = 0;
	int voltage1 = 0, adapter_ibus;
	bool is_enable = false, is_chip_enable = false;
	unsigned int i;

	pe45 = dev_get_drvdata(&alg->dev);
	voltage = 0;
	mtk_pe45_get_setting_by_watt(alg, &voltage, &adapter_ibus,
		&actual_current, 27000000, &input_current);
	ret = mtk_pe45_pd_request(alg, &voltage, &adapter_ibus,
				input_current);

	if (ret != 0 && ret != MTK_ADAPTER_PE4_REJECT) {
		pe4_err("[pe45_i1] err:1 %d\n", ret);
		return -1;
	}

	for (i = 0; i < 3 ; i++) {
		pe4_hal_dump_registers(alg);
		msleep(100);
	}

	mtk_pe45_get_ibus(alg, &ibus1);
	vbus1 = pe4_hal_get_vbus(alg);
	ibus1 = ibus1 / 1000;
	vbat1 = pe4_hal_get_vbat(alg);
	voltage1 = voltage;

	voltage = 0;
	mtk_pe45_get_setting_by_watt(alg, &voltage, &adapter_ibus,
		&actual_current, 15000000, &input_current);


	for (i = 0; i < 6 ; i++) {
		ret = mtk_pe45_pd_request(alg, &voltage, &adapter_ibus,
			input_current);

		if (ret != 0) {
			pe4_err("[pe45_i1] err:2 %d\n", ret);
			return -1;
		}

		msleep(100);
		mtk_pe45_get_ibus(alg, &ibus2);
		vbus2 = pe4_hal_get_vbus(alg);
		ibus2 = ibus2 / 1000;
		vbat2 = pe4_hal_get_vbat(alg);

		if (alg->config == DUAL_CHARGERS_IN_SERIES) {
			pe4_hal_is_charger_enable(alg, CHG2, &is_enable);
			is_chip_enable = pe4_hal_is_chip_enable(alg, CHG2);
		}

		pe4_dbg("[pe45_vbus] vbus1:%d ibus1:%d vbus2:%d ibus2:%d watt:%d en:%d %d vbat:%d %d log_lv:%d\n",
			vbus1, ibus1, vbus2, ibus2, voltage1 * ibus1, is_enable,
			is_chip_enable, vbat1, vbat2, pe4_get_debug_level());
	}

	return voltage1 * ibus1;
}

void mtk_pe45_end(struct chg_alg_device *alg, int type)
{
	mtk_pe45_reset(alg);
	pe4_dbg("%s: retry:%d\n", __func__, type);
}

static int pe45_calculate_rcable_by_swchg(struct chg_alg_device *alg)
{
	int vbus1 = 0, vbus2 = 0, vbus_max = 0, vbus_min = 0;
	int ibus1 = 0, ibus2 = 0, ibus_max = 0, ibus_min = 0;
	int ret = 0, i = 0, val_vbus = 0, val_ibus = 0;
	int cal_r_cable = 0;

	ret = pe4_hal_set_input_current(alg, CHG1, 300000);
	if (ret < 0) {
		pe4_err("set aicr fail(%d)\n", ret);
		return ret;
	}

	ret = pe4_hal_set_charging_current(alg, CHG1, 3000000);
	if (ret < 0) {
		pe4_err("set ichg fail(%d)\n", ret);
		return ret;
	}

	pe4_hal_enable_charger(alg, CHG1, true);

	ret = pe4_hal_set_adapter_cap(alg, 8000, 1000);
	if (ret < 0) {
		pe4_err("set ta cap fail(%d)\n", ret);
		return ret;
	}

	for (i = 0; i < PE45_MEASURE_R_AVG_TIMES + 2; i++) {
		val_vbus = pe4_hal_get_vbus(alg);

		ret = pe4_hal_get_ibus(alg, &val_ibus);
		val_ibus = val_ibus / 1000;
		if (ret < 0) {
			pe4_err("get ibus fail(%d)\n", ret);
			return ret;
		}

		if (i == 0) {
			vbus_max = vbus_min = val_vbus;
			ibus_max = ibus_min = val_ibus;
		} else {
			vbus_max = max(vbus_max, val_vbus);
			ibus_max = max(ibus_max, val_ibus);
			vbus_min = min(vbus_min, val_vbus);
			ibus_min = min(ibus_min, val_ibus);
		}
		vbus1 += val_vbus;
		ibus1 += val_ibus;
		pe4_err("vbus=%d ibus=%d vbus(max,min)=(%d,%d) ibus(max,min)=(%d,%d) vbus1=%d ibus1=%d",
				val_vbus, val_ibus, vbus_max, vbus_min, ibus_max, ibus_min, vbus1, ibus1);
	}

	vbus1 -= (vbus_min + vbus_max);
	vbus1 = precise_div(vbus1, PE45_MEASURE_R_AVG_TIMES);

	ibus1 -= (ibus_min + ibus_max);
	ibus1 = precise_div(ibus1, PE45_MEASURE_R_AVG_TIMES);

	ret = pe4_hal_set_input_current(alg, CHG1, 500000);
	if (ret < 0) {
		pe4_err("set aicr fail(%d)\n", ret);
		return ret;
	}

	for (i = 0; i < PE45_MEASURE_R_AVG_TIMES + 2; i++) {
		val_vbus = pe4_hal_get_vbus(alg);

		ret = pe4_hal_get_ibus(alg, &val_ibus);
		val_ibus = val_ibus / 1000;
		if (ret < 0) {
			pe4_err("get ibus fail(%d)\n", ret);
			return ret;
		}

		if (i == 0) {
			vbus_max = vbus_min = val_vbus;
			ibus_max = ibus_min = val_ibus;
		} else {
			vbus_max = max(vbus_max, val_vbus);
			ibus_max = max(ibus_max, val_ibus);
			vbus_min = min(vbus_min, val_vbus);
			ibus_min = min(ibus_min, val_ibus);
		}
		vbus2 += val_vbus;
		ibus2 += val_ibus;
		pe4_err("vbus=%d ibus=%d vbus(max,min)=(%d,%d) ibus(max,min)=(%d,%d) vbus2=%d ibus2=%d",
				val_vbus, val_ibus, vbus_max, vbus_min, ibus_max, ibus_min, vbus2, ibus2);
	}

	vbus2 -= (vbus_min + vbus_max);
	vbus2 = precise_div(vbus2, PE45_MEASURE_R_AVG_TIMES);

	ibus2 -= (ibus_min + ibus_max);
	ibus2 = precise_div(ibus2, PE45_MEASURE_R_AVG_TIMES);

	cal_r_cable = precise_div(abs(vbus2 - vbus1) * 1000,
					     abs(ibus2 - ibus1)) * 1000;

	pe4_err("%s: cal_r_cable = %d, vbus = %d, %d, ibus = %d, %d\n",
				__func__ , cal_r_cable, vbus1, vbus2, ibus1, ibus2);
	return cal_r_cable / 1000;
}

void mtk_check_rcable_index_table(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe45;

	pe45 = dev_get_drvdata(&alg->dev);
	if (pe45->r_cable_1 <= pe45->r_cable_level[0])
		pe45->rcable_index = BELOW_200mohm;
	else if (pe45->r_cable_1 > pe45->r_cable_level[0] &&
		pe45->r_cable_1 <= pe45->r_cable_level[1])
		pe45->rcable_index = BETWEEN_200_300mohm;
	else if (pe45->r_cable_1 > pe45->r_cable_level[1] &&
		pe45->r_cable_1 <= pe45->r_cable_level[2])
		pe45->rcable_index = BETWEEN_300_400mohm;
	else if (pe45->r_cable_1 > pe45->r_cable_level[2] &&
		pe45->r_cable_1 <= pe45->r_cable_level[3])
		pe45->rcable_index = BETWEEN_400_500mohm;
	else if (pe45->r_cable_1 > pe45->r_cable_level[4])
		pe45->rcable_index = ABOVE_500mohm;
	pe4_dbg("%s: %d\n", __func__, pe45->rcable_index);
}

int mtk_pe45_init_state(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;
	int ret = 0;
	int vbus1, vbat1, ibus1;
	int vbus2, vbat2, ibus2;
	struct pe4_pps_status cap, cap1, cap2;
	int voltage, adapter_ibus = 1000, actual_current;
	int watt = 0;
	int i;
	int input_current = 0;
	bool chg2_chip_enabled = false;
	int chg_cnt, is_chip_enabled;


	pe4_hal_set_mivr(alg, CHG1, 4200000);
	pe4 = dev_get_drvdata(&alg->dev);

	pe4_dbg("set TD false\n");
	pe4_hal_enable_termination(alg, CHG1, false);
	pe4_hal_enable_vbus_ovp(alg, false);

	mtk_pe45_init_cap(alg);
	voltage = 0;
	mtk_pe45_get_setting_by_watt(alg, &voltage, &adapter_ibus,
		&actual_current, 5000000, &input_current);

	ret = mtk_pe45_pd_1st_request(alg, voltage, actual_current,
		actual_current);

	if (ret != 0) {
		pe4_err("[pe45_i0] err:1 %d\n", ret);
		goto retry;
	}

	/* disable charger */
	pe4_hal_force_disable_powerpath(alg, CHG1, true);
	chg_cnt = pe4_hal_get_charger_cnt(alg);
	if (chg_cnt > 1 && alg->config == DUAL_CHARGERS_IN_SERIES) {
		for (i = CHG2; i < CHG_MAX; i++) {
			is_chip_enabled = pe4_hal_is_chip_enable(alg, i);
			if (is_chip_enabled) {
				pe4_hal_enable_charger(alg, i, false);
				pe4_hal_charger_enable_chip(alg, i, false);
			}
		}
	}
	msleep(500);

	cap.output_ma = 0;
	cap.output_mv = 0;

	ret = pe45_hal_get_adapter_output(alg, &cap);

	pe4->can_query = true;
	if (ret == 0 && (cap.output_ma == -1 || cap.output_mv == -1))
		pe4->can_query = false;
	else if (ret == 1)
		pe4->can_query = false;
	else if (ret != 0) {
		pe4_err("[pe45_i0] err:2 %d\n", ret);
		/*enable charger*/
		pe4_hal_force_disable_powerpath(alg, CHG1, false);
		goto err;
	}

	pe4_dbg("[pe45_i0] can_query:%d ret:%d\n",
		pe4->can_query,
		ret);

	pe4->pmic_vbus = pe4_hal_get_vbus(alg);
	pe4->TA_vbus = cap.output_mv;
	pe4->vbus_cali = pe4->TA_vbus - pe4->pmic_vbus;

	pe4_dbg("[pe45_i0]pmic_vbus:%d TA_vbus:%d cali:%d ibus:%d chip2:%d\n",
		pe4->pmic_vbus, pe4->TA_vbus, pe4->vbus_cali,
		cap.output_ma, chg2_chip_enabled);

	/*enable charger*/
	pe4_hal_force_disable_powerpath(alg, CHG1, false);
	if (alg->config == SINGLE_CHARGER) {
		pe4_hal_set_charging_current(alg,
			CHG1, pe4->sc_charger_current);
		pe4_hal_set_input_current(alg,
			CHG1, pe4->sc_input_current);
	} else if (alg->config == DUAL_CHARGERS_IN_SERIES) {
		pe4_hal_set_charging_current(alg,
			CHG1, pe4->dcs_chg2_charger_current);
		pe4_hal_set_input_current(alg,
			CHG1, pe4->dcs_input_current);
		chg_cnt = pe4_hal_get_charger_cnt(alg);
		if (chg_cnt > 1) {
			for (i = CHG2; i < CHG_MAX; i++) {
				is_chip_enabled =
					pe4_hal_is_chip_enable(alg, i);
				if (is_chip_enabled == false) {
					pe4_hal_charger_enable_chip(
						alg, i, true);
					pe4_hal_enable_charger(alg, i, true);
					pe4_hal_set_charging_current(alg,
						CHG2,
						pe4->dcs_chg2_charger_current);
					pe4_hal_set_input_current(alg,
						CHG2,
						pe4->dcs_chg2_charger_current);
				}
			}
		}
	}
	pe4_hal_dump_registers(alg);
	msleep(100);

	if (cap.output_ma > 100) {
		pe4_err("[pe45_i0] FOD fail :%d\n", cap.output_ma);
		goto err;
	}

	if (pe4->can_query == true) {
		/* measure 1 */
		voltage = 0;
		mtk_pe45_get_setting_by_watt(alg, &voltage, &adapter_ibus,
			&actual_current, 5000000, &input_current);
		ret = mtk_pe45_pd_request(alg, &voltage, &actual_current,
					actual_current);

		if (ret != 0) {
			pe4_err("[pe45_i0] err:3 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus1 = pe4_hal_get_vbus(alg);
			vbat1 = pe4_hal_get_vbat(alg);
			mtk_pe45_get_ibus(alg, &ibus1);
			ibus1 = ibus1 / 1000;
			ret = pe45_hal_get_adapter_output(alg, &cap1);
			if (ret != 0) {
				pe4_err("[pe45_i0] err:4 %d\n", ret);
				goto err;
			}

			pe4_dbg("[pe45_i11]vbus:%d ibus:%d vbat:%d TA_vbus:%d TA_ibus:%d setting:%d %d\n",
				vbus1, ibus1, vbat1,
				cap1.output_mv, cap1.output_ma,
				voltage, actual_current);

			if (abs(cap1.output_ma - actual_current) < 200)
				break;
		}


		/* measure 2 */
		voltage = 0;
		mtk_pe45_get_setting_by_watt(alg, &voltage, &adapter_ibus,
			&actual_current, 7500000, &input_current);
		ret = mtk_pe45_pd_request(alg, &voltage, &actual_current,
					actual_current);

		if (ret != 0) {
			pe4_err("[pe45_i0] err:5 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus2 = pe4_hal_get_vbus(alg);
			vbat2 = pe4_hal_get_vbat(alg);
			mtk_pe45_get_ibus(alg, &ibus2);
			ibus2 = ibus2 / 1000;
			ret = pe45_hal_get_adapter_output(alg, &cap2);
			if (ret != 0)
				goto err;

			pe4_dbg("[pe45_i12]vbus:%d ibus:%d vbat:%d TA_vbus:%d TA_ibus:%d setting:%d %d\n",
				vbus2, ibus2, vbat2,
				cap2.output_mv, cap2.output_ma,
				voltage, actual_current);
			if (abs(cap2.output_ma - actual_current) < 200)
				break;
		}

		pe4_dbg("[pe45_i1]vbus:%d,%d,%d,%d ibus:%d,%d,%d,%d vbat:%d,%d\n",
			vbus1, vbus2, cap1.output_mv, cap2.output_mv,
			ibus1, ibus2, cap1.output_ma, cap2.output_ma,
			vbat1, vbat2);
		if (abs(cap2.output_ma - cap1.output_ma) != 0) {
			pe4->r_sw = abs((vbus2 - vbus1) - (vbat2 - vbat1)) * 1000 /
					abs(cap2.output_ma - cap1.output_ma);
			pe4->r_cable = abs((cap2.output_mv - cap1.output_mv) -
					    (vbus2 - vbus1)) * 1000 /
					abs(cap2.output_ma - cap1.output_ma);
		} else {
			pe4->r_sw = abs((vbus2 - vbus1) - (vbat2 - vbat1)) * 1000;
			pe4->r_cable = abs((cap2.output_mv - cap1.output_mv)
					- (vbus2 - vbus1)) * 1000;
		}
		pe4->r_cable_2 = abs(cap2.output_mv - pe4->vbus_cali - vbus2)
				* 1000 / abs(cap2.output_ma);
		pe4->r_cable_1 = abs(cap1.output_mv - pe4->vbus_cali - vbus1)
				* 1000 / abs(cap1.output_ma);

		pe4_dbg("[pe45_i2]cq:%d r_sw:%d r_cable:%d r_cable_1:%d r_cable_2:%d\n",
			pe4->can_query, pe4->r_sw, pe4->r_cable, pe4->r_cable_1,
			pe4->r_cable_2);
	} else {
		pe4_err("TA does not support query\n");
		pe4->r_cable_1 = pe45_calculate_rcable_by_swchg(alg);
		pe4_dbg("[pe45_i3]cq:%d r_cable_1:%d\n",
			pe4->can_query, pe4->r_cable_1);
	}

	mtk_check_rcable_index_table(alg);
	pe4->mivr_count = 0;
	watt = mtk_pe45_get_init_lower_watt(alg);

	mtk_pe45_get_setting_by_watt(alg, &voltage, &adapter_ibus,
				&actual_current, watt, &input_current);
	mtk_pe45_rcable_control_chg_level(alg, &voltage, &input_current, &adapter_ibus);
	pe4->avbus = voltage / 10 * 10;
	ret = mtk_pe45_pd_request(alg, &pe4->avbus, &adapter_ibus,
				input_current);

	if (ret != 0 && ret != MTK_ADAPTER_PE4_REJECT) {
		pe4_err("[pe45_i0] err:6 %d\n", ret);
		goto err;
	}

	pe4->avbus = voltage;
	pe4->ibus = watt / voltage;
	pe4->watt = watt;

	pe4->state = PE4_RUN;
	pe4->polling_interval = 10;
	pe4_hal_set_mivr(alg, CHG1, 4600000);
	return 0;

retry:
	mtk_pe45_end(alg, 0);
	return 0;
err:
	mtk_pe45_end(alg, 2);
	return 0;
}

int mtk_pe45_safety_check(struct chg_alg_device *alg)
{
	int vbus;
	struct mtk_pe45 *pe45;
	struct pe4_pps_status cap;
	//struct pd_status TAstatus = {0,};
	struct pe4_adapter_status TAstatus;
	int ret;
	int tmp;
	unsigned int i;
	int high_tmp_cnt = 0;

	pe45 = dev_get_drvdata(&alg->dev);

	TAstatus.ocp = 0;
	TAstatus.otp = 0;
	TAstatus.ovp = 0;
	TAstatus.temperature = 0;

	/* vbus ov */
	vbus = pe4_hal_get_vbus(alg);
	if (vbus - pe45->avbus >= 2000) {
		pe4_err("[pe45_err]vbus ov :vbus:%d avbus:%d\n",
			vbus, pe45->avbus);
		goto err;
	}

	/* cable voltage drop check */
	if (pe45->can_query == true) {
		ret = pe45_hal_get_adapter_output(alg, &cap);
		if (ret != 0) {
			pe4_err("[pe45_err] err:1 %d\n", ret);
			goto err;
		}

		if (cap.output_mv != -1 &&
			(cap.output_mv - vbus) > PE45_VBUS_IR_DROP_THRESHOLD) {
			pe4_err("[pe45_err]vbus ov2 vbus:%d TAvbus:%d %d %d\n",
				vbus, cap.output_mv,
				PE45_VBUS_IR_DROP_THRESHOLD,
				(cap.output_mv - vbus) >
				PE45_VBUS_IR_DROP_THRESHOLD);
			goto err;
		}

		/* TA V_BUS OVP */
		if (cap.output_mv >= pe45->avbus * 12 / 10) {
			pe4_err("[pe45_err]TA vbus ovp :vbus:%d avbus:%d\n",
				cap.output_mv, pe45->avbus);
			goto err;
		}
	}

	/* TA Thermal */
	for (i = 0; i < 3; i++) {
		ret = pe45_hal_get_adapter_status(alg, &TAstatus);
		if (TAstatus.temperature >= 100 &&
			TAstatus.temperature != 0 &&
			ret != MTK_ADAPTER_PE4_NOT_SUPPORT &&
			ret != MTK_ADAPTER_PE4_TIMEOUT) {
			high_tmp_cnt++;
			pe4_err("[pe45]TA Thermal:%d cnt:%d\n",
				TAstatus.temperature, high_tmp_cnt);
		} else if (ret == MTK_ADAPTER_PE4_TIMEOUT) {
			pe4_err("[pe45]TA adapter_dev_get_status timeout\n");
			goto err;
		} else
			break;

		if (high_tmp_cnt >= 3) {
			pe4_err("[pe45_err]TA Thermal: %d thd:%d cnt:%d\n",
				TAstatus.temperature, 100, high_tmp_cnt);
			goto err;
		}
	}

	if (ret == MTK_ADAPTER_PE4_NOT_SUPPORT)
		pe4_err("[pe45]TA adapter_dev_get_status not support\n");
	else {
		if (TAstatus.ocp || TAstatus.otp || TAstatus.ovp) {

			pe4_err("[pe45_err]TA protect: ocp:%d otp:%d ovp:%d\n",
				TAstatus.ocp,
				TAstatus.otp,
				TAstatus.ovp);
			goto err;
		}

		pe4_dbg("PD_TA:TA protect: ocp:%d otp:%d ovp:%d tmp:%d\n",
			TAstatus.ocp,
			TAstatus.otp,
			TAstatus.ovp,
			TAstatus.temperature);
	}

	tmp = pe4_hal_get_battery_temperature(alg);

	if (tmp > pe45->high_temp_to_leave_pe45 ||
		tmp < pe45->low_temp_to_leave_pe45) {

		pe4_err("[pe45_err]tmp:%d threshold:%d %d\n",
			tmp, pe45->high_temp_to_leave_pe45,
			pe45->low_temp_to_leave_pe45);
		return 1;
	}

	return 0;
err:
	return -1;
}

int mtk_pe45_cc_state(struct chg_alg_device *alg)
{
	int ibus = 0, vbat, ibat, vbus, compare_ibus = 0;
	int icl, ccl, ccl2, cv, max_icl;
	struct mtk_pe45 *pe45;
	int ret;
	int oldavbus = 0;
	int oldibus = 0;
	int watt;
	int max_watt;
//	struct charger_data *pdata;
	int actual_current;
	int new_watt = 0;
	int adapter_ibus = 0;
	int input_current = 0;
	int icl_threshold;
	unsigned int mivr1 = 0;
	bool chg1_mivr = false;

	bool thermal_skip = false;
	int uisoc = 0;

	pe45 = dev_get_drvdata(&alg->dev);

	vbat = pe4_hal_get_vbat(alg);
	ibat = pe4_hal_get_ibat(alg);

	mtk_pe45_get_ibus(alg, &ibus);
	ibus = ibus / 1000;
	oldibus = ibus;
	pe4_hal_get_mivr_state(alg, CHG1, &chg1_mivr);
	pe4_hal_get_mivr(alg, CHG1, &mivr1);

	vbus = pe4_hal_get_vbus(alg);
	ccl = pe45->charger_current1 / 1000;
	ccl2 = pe45->charger_current1 / 1000;
	cv = pe45->cv / 1000;
	watt = pe45->avbus * ibus;

	icl = pe45->input_current1 / 1000 *
		(100 - pe45->ibus_err) / 100;

	compare_ibus = ibus;

	if (icl > pe45->max_charger_ibus)
		max_icl = pe45->max_charger_ibus;
	else
		max_icl = icl;

	icl_threshold = 100;
	max_watt = pe45->avbus * max_icl;

	pe4_dbg("[pe45_cc]vbus:%d:%d,ibus:%d,cibus:%d,ibat:%d icl:%d:%d,ccl:%d,%d,vbat:%d,maxIbus:%d,mivr:%d\n",
		pe45->avbus, vbus,
		ibus,
		compare_ibus,
		ibat,
		icl, max_icl,
		ccl, ccl2,
		vbat, pe45->max_charger_ibus,
		chg1_mivr);

	if ((chg1_mivr && (vbus < mivr1 / 1000 - 500))) {
		mtk_pe45_end(alg, 1);
		return 0;
	}

	if (((chg1_mivr) && !thermal_skip) ||
	    ((compare_ibus >= (max_icl - icl_threshold)) && !thermal_skip) ||
	    (compare_ibus <= (max_icl - icl_threshold * 2))) {

		oldavbus = pe45->avbus;

		if (chg1_mivr) {
			pe45->avbus = pe45->avbus + 50;

		new_watt = (pe45->avbus + 50) * icl;

		} else if (compare_ibus >= (max_icl - icl_threshold)) {
			pe45->avbus = pe45->avbus + 50;
			new_watt = (pe45->avbus + 50) * ibus;
		} else if (compare_ibus <= (max_icl - icl_threshold * 2)) {
			new_watt = pe45->avbus * pe45->ibus - 500000;
			pe45->avbus = pe45->avbus - 50;
		}

		ret = mtk_pe45_get_setting_by_watt(alg, &pe45->avbus,
				&adapter_ibus, &actual_current, new_watt,
			&input_current);

		mtk_pe45_rcable_control_chg_level(alg, &pe45->avbus,
			    &input_current, &adapter_ibus);

		if (ibus >= (max_icl - icl_threshold) && ret != 4)
			pe45->polling_interval = 3;

		if (compare_ibus <= (max_icl - icl_threshold * 2)) {
			new_watt = pe45->avbus * pe45->ibus - 500000;
			pe45->avbus = pe45->avbus - 50;
		}

		if (pe45->avbus <= 5000)
			pe45->avbus = 5000;
		pe4_err("%s: avbus: %d, oldavbus: %d\n", __func__, pe45->avbus, oldavbus);
		if (abs(pe45->avbus - oldavbus) >= 50) {
			ret = mtk_pe45_pd_request(alg, &pe45->avbus,
					&adapter_ibus, input_current);
			if (ret != 0 && ret != MTK_ADAPTER_PE4_REJECT) {
				pe4_err("pe4 end2 error1\n");
				goto err;
			}
		}
		msleep(100);

		vbat = pe4_hal_get_vbat(alg);
		ibat = pe4_hal_get_ibat(alg);
		mtk_pe45_get_ibus(alg, &ibus);
		vbus = pe4_hal_get_vbat(alg);
		ibus = ibus / 1000;
		icl = pe45->input_current_limit1 / 1000;
		ccl = pe45->charger_current1 / 1000;

		pe45->watt = pe45->avbus * ibus;
		pe45->vbus = vbus;
		pe45->ibus = ibus;
	} else
		pe45->polling_interval = 10;

	ret = mtk_pe45_safety_check(alg);
	if (ret == -1) {
		pe4_err("pe4 end2 error2\n");
		goto err;
	}
	if (ret == 1)
		goto disable_hv;

	uisoc = pe4_hal_get_uisoc(alg);
	if (uisoc > 80 && pe45->avbus * oldibus <= PE45_MIN_WATT)
		mtk_pe45_end(alg, 1);


	return 0;

disable_hv:
	mtk_pe45_end(alg, 0);
	return 0;
err:
	mtk_pe45_end(alg, 2);
	return 0;
}

static int pe4_sc_set_charger(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;
	int ichg1_min = -1, aicr1_min = -1;
	int ret;
	int min_icl = 0;

	pe4 = dev_get_drvdata(&alg->dev);

	if (pe4->input_current_limit1 == 0 ||
		pe4->charging_current_limit1 == 0) {
		pe4_err("input/charging current is 0, end Pd\n");
		return -1;
	}

	mutex_lock(&pe4->data_lock);
	if (pe4->charging_current_limit1 != -1) {
		if (pe4->charging_current_limit1 <
			pe4->sc_charger_current)
			pe4->charger_current1 =
				pe4->charging_current_limit1;
		ret = pe4_hal_get_min_charging_current(alg, CHG1, &ichg1_min);
		if (ret != -EOPNOTSUPP &&
			pe4->charging_current_limit1 < ichg1_min)
			pe4->charger_current1 = 0;
	} else
		pe4->charger_current1 = pe4->sc_charger_current;

	min_icl = pe4->sc_input_current;
	if (pe4->pe4_input_current_limit != -1 &&
	    pe4->pe4_input_current_limit < min_icl)
		min_icl = pe4->pe4_input_current_limit;

	if (pe4->pe4_input_current_limit_setting != -1 &&
	    pe4->pe4_input_current_limit_setting < min_icl)
		min_icl = pe4->pe4_input_current_limit_setting;

	if (pe4->input_current_limit1 != -1 &&
	    pe4->input_current_limit1 < min_icl) {
		pe4->input_current1 = pe4->input_current_limit1;
		ret = pe4_hal_get_min_input_current(alg, CHG1, &aicr1_min);
		if (ret != -EOPNOTSUPP && pe4->input_current_limit1 < aicr1_min)
			pe4->input_current1 = 0;
	} else
		pe4->input_current1 = min_icl;
	mutex_unlock(&pe4->data_lock);

	if (pe4->input_current1 == 0 ||
		pe4->charger_current1 == 0) {
		pe4_err("current is zero %d %d\n",
			pe4->input_current1,
			pe4->charger_current1);
		return -1;
	}

	pe4_hal_set_charging_current(alg,
		CHG1, pe4->charger_current1);
	pe4_hal_set_input_current(alg,
		CHG1, pe4->input_current1);

	if (pe4->old_cv == 0 || (pe4->old_cv != pe4->cv) || pe4->pe4_6pin_en == 0) {
		pe4_hal_vbat_mon_en(alg, CHG1, false);
		pe4_hal_set_cv(alg, CHG1, pe4->cv);
		if (pe4->pe4_6pin_en && pe4->stop_6pin_re_en != 1)
			pe4_hal_vbat_mon_en(alg, CHG1, true);

		pe4_dbg("%s old_cv=%d, new cv=%d, pe4_6pin_en=%d\n", __func__,
			pe4->old_cv, pe4->cv, pe4->pe4_6pin_en);

		pe4->old_cv = pe4->cv;
	} else {
		if (pe4->pe4_6pin_en && pe4->stop_6pin_re_en != 1) {
			pe4->stop_6pin_re_en = 1;
			pe4_hal_vbat_mon_en(alg, CHG1, true);
		}
	}

	pe4_dbg("%s m:%d s:%d cv:%d chg1:%d,%d min:%d:%d,6pin_en:%d,6pin_re_en=%d\n",
		__func__,
		alg->config,
		pe4->state,
		pe4->cv,
		pe4->input_current1,
		pe4->charger_current1,
		ichg1_min,
		aicr1_min,
		pe4->pe4_6pin_en,
		pe4->stop_6pin_re_en);

	return 0;
}

static int pe4_dcs_set_charger(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;
	bool chg2_chip_enabled = false;
	int ret;
	int ichg1_min = -1, ichg2_min = -1;
	int aicr1_min = -1;
	int min_icl;

	pe4 = dev_get_drvdata(&alg->dev);

	if (pe4->input_current_limit1 == 0 ||
		pe4->charging_current_limit1 == 0 ||
		pe4->charging_current_limit2 == 0) {
		pr_notice("input/charging current is 0, end PD\n");
		return -1;
	}

	mutex_lock(&pe4->data_lock);
	min_icl = pe4->dcs_input_current;
	if (pe4->pe4_input_current_limit != -1 &&
	    pe4->pe4_input_current_limit < min_icl)
		min_icl = pe4->pe4_input_current_limit;

	if (pe4->pe4_input_current_limit_setting != -1 &&
	    pe4->pe4_input_current_limit_setting < min_icl)
		min_icl = pe4->pe4_input_current_limit_setting;

	if (pe4->input_current_limit1 != -1 &&
	    pe4->input_current_limit1 < min_icl) {
		pe4->input_current1 = pe4->input_current_limit1;
		ret = pe4_hal_get_min_input_current(alg, CHG1, &aicr1_min);
		if (ret != -EOPNOTSUPP && pe4->input_current_limit1 < aicr1_min)
			pe4->input_current1 = 0;
	} else
		pe4->input_current1 = min_icl;

	if (pe4->charging_current_limit1 != -1 &&
		pe4->charging_current_limit1 <
		pe4->dcs_chg1_charger_current) {
		pe4->charger_current1 = pe4->charging_current_limit1;
		ret = pe4_hal_get_min_charging_current(alg, CHG1, &ichg1_min);
		if (ret != -EOPNOTSUPP &&
			pe4->charging_current_limit1 < ichg1_min)
			pe4->charger_current1 = 0;
	} else
		pe4->charger_current1 = pe4->dcs_chg1_charger_current;

	if (pe4->state == PE4_RUN)
		pe4->charger_current2 = pe4->dcs_chg2_charger_current;

	if (pe4->charging_current_limit2 != -1 &&
		pe4->charging_current_limit2 <
		pe4->charger_current2) {
		pe4->charger_current2 = pe4->charging_current_limit2;
		ret = pe4_hal_get_min_charging_current(alg, CHG2, &ichg1_min);
		if (ret != -EOPNOTSUPP &&
			pe4->charging_current_limit2 < ichg1_min)
			pe4->charger_current2 = 0;
	}
	mutex_unlock(&pe4->data_lock);

	if (pe4->input_current1 == 0 ||
		pe4->charger_current1 == 0 ||
		pe4->charger_current2 == 0) {
		pe4_err("current is zero %d %d %d\n",
			pe4->input_current1,
			pe4->charger_current1,
			pe4->charger_current2);
		pe4_hal_enable_charger(alg, CHG2, false);
		pe4_hal_charger_enable_chip(alg, CHG2, false);
		return -1;
	}

	chg2_chip_enabled = pe4_hal_is_chip_enable(alg, CHG2);
	pe4_dbg("chg2_en:%d pe4_state:%d\n",
		chg2_chip_enabled, pe4->state);
	if (pe4->state == PE4_RUN) {
		if (!chg2_chip_enabled)
			pe4_hal_charger_enable_chip(alg, CHG2, true);
		pe4_hal_enable_charger(alg, CHG2, true);
		pe4_hal_set_cv(alg, CHG2, pe4->cv + 200000);
		pe4_hal_set_input_current(alg,
			CHG2, pe4->charger_current2);
		pe4_hal_set_charging_current(alg,
			CHG2, pe4->charger_current2);

		pe4_hal_set_eoc_current(alg, CHG1,
			pe4->dual_polling_ieoc);
		pe4_hal_enable_termination(alg, CHG1, false);
		pe4_hal_safety_check(alg, pe4->dual_polling_ieoc);
	} else if (pe4->state == PE4_TUNING) {
		if (!chg2_chip_enabled)
			pe4_hal_charger_enable_chip(alg, CHG2, true);
		pe4_hal_enable_charger(alg, CHG2, true);
		pe4_hal_set_eoc_current(alg, CHG1, pe4->dual_polling_ieoc);
		pe4_hal_enable_termination(alg, CHG1, false);
		pe4_hal_safety_check(alg, pe4->dual_polling_ieoc);
	} else if (pe4->state == PE4_POSTCC) {
		pe4_hal_set_eoc_current(alg, CHG1, 150000);
		pe4_hal_reset_eoc_state(alg);
		pe4_hal_enable_termination(alg, CHG1, true);
	} else {
		pe4_err("%s state error!", __func__);
		return -1;
	}

	pe4_hal_set_charging_current(alg,
		CHG1, pe4->charger_current1);
	pe4_hal_set_input_current(alg,
		CHG1, pe4->input_current1);
	pe4_hal_set_cv(alg,
		CHG1, pe4->cv);

	pe4_dbg("%s m:%d s:%d cv:%d chg1:%d,%d chg2:%d,%d chg2en:%d min:%d,%d,%d\n",
		__func__,
		alg->config,
		pe4->state,
		pe4->cv,
		pe4->input_current1,
		pe4->charger_current1,
		pe4->input_current2,
		pe4->charger_current2,
		chg2_chip_enabled,
		ichg1_min,
		ichg2_min,
		aicr1_min);

	return 0;
}

static void _pe4_set_current(struct chg_alg_device *alg)
{
	int ret_value;

	if (alg->config == DUAL_CHARGERS_IN_SERIES) {
		if (pe4_dcs_set_charger(alg) != 0) {
			ret_value = ALG_DONE;
			//goto out;
		}
	} else {
		if (pe4_sc_set_charger(alg) != 0) {
			ret_value = ALG_DONE;
			//goto out;
		}
	}
}

static int _pe4_start_algo(struct chg_alg_device *alg)
{
	int ret = 0, ret_value = 0;
	struct mtk_pe45 *pe4;
	bool again = false;
	int uisoc = 0, tmp = 0;

	pe4 = dev_get_drvdata(&alg->dev);
	mutex_lock(&pe4->access_lock);
	__pm_stay_awake(pe4->suspend_lock);

	if (algo_waiver_test) {
		ret_value = ALG_WAIVER;
		goto skip;
	}

	do {
		pe4_info("%s state:%d %s %d\n", __func__,
			pe4->state,
			pe4_state_to_str(pe4->state),
			again);

		again = false;
		switch (pe4->state) {
		case PE4_HW_UNINIT:
		case PE4_HW_FAIL:
			ret_value = ALG_INIT_FAIL;
			break;
		case PE4_HW_READY:
			uisoc = pe4_hal_get_uisoc(alg);
			ret = pe4_hal_is_pd_adapter_ready(alg);
			ret_value = ret;
			if (ret == ALG_READY) {
				tmp = pe4_hal_get_battery_temperature(alg);
				if (pe4->input_current_limit1 != -1 ||
					pe4->charging_current_limit1 != -1 ||
					pe4->input_current_limit2 != -1 ||
					pe4->charging_current_limit2 != -1 ||
					tmp > pe4->high_temp_to_enter_pe45 ||
					tmp < pe4->low_temp_to_enter_pe45) {
					ret_value = ALG_NOT_READY;
					pe4_info("%d %d %d %d %d\n",
						pe4->input_current_limit1,
						pe4->charging_current_limit1,
						pe4->pe45_stop_battery_soc,
						pe4->high_temp_to_enter_pe45,
						pe4->low_temp_to_enter_pe45);
				} else if ((uisoc == -1 && pe4->ref_vbat > pe4->vbat_threshold) ||
						uisoc > pe4->pe45_stop_battery_soc) {
					ret_value = ALG_WAIVER;
					pe4_info("%d\n", pe4->ref_vbat);
				} else {
					again = true;
					pe4->state = PE4_INIT;
				}
			} else if (ret == ALG_TA_NOT_SUPPORT)
				pe4->state = PE4_TA_NOT_SUPPORT;
			break;
		case PE4_TA_NOT_SUPPORT:
			ret_value = ALG_TA_NOT_SUPPORT;
			break;
		case PE4_INIT:
			pe4_hal_set_charging_current(alg,
				CHG1, pe4->charger_current1);
			mtk_pe45_init_state(alg);
			again = true;
			break;
		case PE4_RUN:
		case PE4_TUNING:
		case PE4_POSTCC:
			pe4_hal_set_charging_current(alg,
				CHG1, pe4->charger_current1);
			_pe4_set_current(alg);
			mtk_pe45_cc_state(alg);
			break;
		default:
			pe4_err("PE4 unknown state:%d\n", pe4->state);
			ret_value = ALG_INIT_FAIL;
			break;
		}
	} while (again == true);
skip:
	__pm_relax(pe4->suspend_lock);
	mutex_unlock(&pe4->access_lock);

	return ret_value;
}


static bool _pe4_is_algo_running(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);

	if (pe4->state == PE4_RUN || pe4->state == PE4_INIT ||
	    pe4->state == PE4_TUNING || pe4->state == PE4_POSTCC)
		return true;

	return false;
}

static int _pe4_stop_algo(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);

	pe4_dbg("%s %d\n", __func__, pe4->state);
	if (pe4->state == PE4_RUN || pe4->state == PE4_INIT ||
	    pe4->state == PE4_TUNING || pe4->state == PE4_POSTCC)
		mtk_pe45_end(alg, 0);

	return 0;
}

static int pe4_plugout_reset(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);
	switch (pe4->state) {
	case PE4_HW_UNINIT:
	case PE4_HW_FAIL:
	case PE4_HW_READY:
		break;
	case PE4_TA_NOT_SUPPORT:
		pe4->state = PE4_HW_READY;
		break;
	case PE4_INIT:
	case PE4_RUN:
	case PE4_TUNING:
	case PE4_POSTCC:
		mtk_pe45_end(alg, 3);
		break;
	default:
		break;
	}
	return 0;
}

static int pe4_full_evt(struct chg_alg_device *alg)
{
	struct mtk_pe45 *pe4;
	int ret = 0;
	bool chg_en, chg2_enabled = false;
	int ichg2, ichg2_min;
	int ret_value = 0;

	pe4 = dev_get_drvdata(&alg->dev);
	switch (pe4->state) {
	case PE4_HW_UNINIT:
	case PE4_HW_FAIL:
	case PE4_HW_READY:
	case PE4_TA_NOT_SUPPORT:
	case PE4_INIT:
		break;
	case PE4_RUN:
	case PE4_TUNING:
	case PE4_POSTCC:
		if (alg->config == DUAL_CHARGERS_IN_SERIES) {
			pe4_hal_is_charger_enable(
				alg, CHG2, &chg_en);
			chg2_enabled = pe4_hal_is_chip_enable(alg, CHG2);

			if (!chg_en || !chg2_enabled) {
				/* notify eoc , fix me */
				pe4->state = PE4_HW_READY;
				pe4_err("%s: charging done:%d %d\n",
					__func__, chg_en, chg2_enabled);
				if (alg->is_polling_mode == false)
					ret_value = 1;
			} else {
				pe4_hal_get_charging_current(alg, CHG2, &ichg2);
				ret = pe4_hal_get_min_charging_current(
					alg, CHG2, &ichg2_min);
				if (ret == -EOPNOTSUPP)
					ichg2_min = 100000;

				pe4_err("ichg2:%d, ichg2_min:%d state:%d\n",
					ichg2, ichg2_min, pe4->state);
				if (ichg2 - 500000 <= ichg2_min) {
					pe4->state = PE4_POSTCC;
					pe4_hal_enable_charger(alg,
						CHG2, false);
					pe4_hal_set_eoc_current(alg,
						CHG1, 150000);
					pe4_hal_reset_eoc_state(alg);
					pe4_hal_enable_termination(alg,
						CHG1, true);
				} else {
					pe4->state = PE4_TUNING;
					mutex_lock(&pe4->data_lock);
					if (pe4->charger_current2 >= 500000)
						pe4->charger_current2 =
							ichg2 - 500000;
					pe4_hal_set_charging_current(alg,
						CHG2, pe4->charger_current2);
					mutex_unlock(&pe4->data_lock);
				}
				ret_value = 1;
			}

		} else {
			if (pe4->state == PE4_RUN) {
				pe4_err("%s evt full\n",  __func__);
				pe4->state = PE4_HW_READY;
			}
		}

		break;
	default:
		ret_value = ALG_INIT_FAIL;
		break;
	}
	return ret_value;
}

static int _pe4_notifier_call(struct chg_alg_device *alg,
			 struct chg_alg_notify *notify)
{
	struct mtk_pe45 *pe4;
	int ret_value;

	pe4 = dev_get_drvdata(&alg->dev);
	pe4_dbg("%s evt:%d, state:%s\n", __func__, notify->evt,
		pe4_state_to_str(pe4->state));

	switch (notify->evt) {
	case EVT_PLUG_OUT:
		pe4->stop_6pin_re_en = 0;
		ret_value = pe4_plugout_reset(alg);
		break;
	case EVT_FULL:
		pe4->stop_6pin_re_en = 1;
		ret_value = pe4_full_evt(alg);
		break;
	case EVT_BATPRO_DONE:
		pe4->pe4_6pin_en = 0;
		ret_value = 0;
		break;
	default:
		ret_value = -EINVAL;
	}

	return ret_value;
}

static void mtk_pe4_parse_dt(struct mtk_pe45 *pe4,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;

	if (of_property_read_u32(np, "pe45_max_vbus", &val) >= 0)
		pe4->pe45_max_vbus = val;
	else if (of_property_read_u32(np, "pe45-max-vbus", &val) >= 0)
		pe4->pe45_max_vbus = val;
	else {
		pe4_err("use default pe45_max_vbus:%d\n", PE45_MAX_VBUS);
		pe4->pe45_max_vbus = PE45_MAX_VBUS;
	}

	if (of_property_read_u32(np, "pe45_max_ibus", &val) >= 0)
		pe4->pe45_max_ibus = val;
	else if (of_property_read_u32(np, "pe45-max-ibus", &val) >= 0)
		pe4->pe45_max_ibus = val;
	else {
		pe4_err("use default pe45_max_ibus:%d\n", PE45_MAX_IBUS);
		pe4->pe45_max_ibus = PE45_MAX_IBUS;
	}

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		pe4->min_charger_voltage = val;
	else if (of_property_read_u32(np, "min-charger-voltage", &val) >= 0)
		pe4->min_charger_voltage = val;
	else {
		pe4_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		pe4->min_charger_voltage = V_CHARGER_MIN;
	}

	if (of_property_read_u32(np, "pe45_stop_battery_soc", &val) >= 0)
		pe4->pe45_stop_battery_soc = val;
	else if (of_property_read_u32(np, "pe45-stop-battery-soc", &val) >= 0)
		pe4->pe45_stop_battery_soc = val;
	else {
		pe4_err("use default pe45_stop_battery_soc:%d\n", 80);
		pe4->pe45_stop_battery_soc = 80;
	}

	if (of_property_read_u32(np, "high_temp_to_leave_pe45", &val) >= 0)
		pe4->high_temp_to_leave_pe45 = val;
	else if (of_property_read_u32(np, "high-temp-to-leave-pe45", &val) >= 0)
		pe4->high_temp_to_leave_pe45 = val;
	else {
		pe4_err("use default high_temp_to_leave_pe45:%d\n",
			HIGH_TEMP_TO_LEAVE_PE45);
		pe4->high_temp_to_leave_pe45 = HIGH_TEMP_TO_LEAVE_PE45;
	}

	if (of_property_read_u32(np, "high_temp_to_enter_pe45", &val) >= 0)
		pe4->high_temp_to_enter_pe45 = val;
	else if (of_property_read_u32(np, "high-temp-to-enter-pe45", &val) >= 0)
		pe4->high_temp_to_enter_pe45 = val;
	else {
		pe4_err("use default high_temp_to_enter_pe45:%d\n",
			HIGH_TEMP_TO_ENTER_PE45);
		pe4->high_temp_to_enter_pe45 = HIGH_TEMP_TO_ENTER_PE45;
	}

	if (of_property_read_u32(np, "low_temp_to_leave_pe45", &val) >= 0)
		pe4->low_temp_to_leave_pe45 = val;
	else if (of_property_read_u32(np, "low-temp-to-leave-pe45", &val) >= 0)
		pe4->low_temp_to_leave_pe45 = val;
	else {
		pe4_err("use default low_temp_to_leave_pe45:%d\n",
			LOW_TEMP_TO_LEAVE_PE45);
		pe4->low_temp_to_leave_pe45 = LOW_TEMP_TO_LEAVE_PE45;
	}

	if (of_property_read_u32(np, "low_temp_to_enter_pe45", &val) >= 0)
		pe4->low_temp_to_enter_pe45 = val;
	else if (of_property_read_u32(np, "low-temp-to-enter-pe45", &val) >= 0)
		pe4->low_temp_to_enter_pe45 = val;
	else {
		pe4_err("use default low_temp_to_enter_pe45:%d\n",
			LOW_TEMP_TO_ENTER_PE45);
		pe4->low_temp_to_enter_pe45 = LOW_TEMP_TO_ENTER_PE45;
	}

	if (of_property_read_u32(np, "ibus_err", &val) >= 0)
		pe4->ibus_err = val;
	else if (of_property_read_u32(np, "ibus-err", &val) >= 0)
		pe4->ibus_err = val;
	else {
		pe4_err("use default ibus_err:%d\n",
			IBUS_ERR);
		pe4->ibus_err = IBUS_ERR;
	}

	if (of_property_read_u32(np, "pe45_r_cable_1a_lower", &val) >= 0)
		pe4->pe45_r_cable_1a_lower = val;
	else if (of_property_read_u32(np, "pe45-r-cable-1a-lower", &val) >= 0)
		pe4->pe45_r_cable_1a_lower = val;
	else {
		pe4_err("use default pe45_r_cable_1a_lower:%d\n", 530);
		pe4->pe45_r_cable_1a_lower = 530;
	}

	if (of_property_read_u32(np, "pe45_r_cable_2a_lower", &val) >= 0)
		pe4->pe45_r_cable_2a_lower = val;
	else if (of_property_read_u32(np, "pe45-r-cable-2a-lower", &val) >= 0)
		pe4->pe45_r_cable_2a_lower = val;
	else {
		pe4_err("use default pe45_r_cable_2a_lower:%d\n", 390);
		pe4->pe45_r_cable_2a_lower = 390;
	}

	if (of_property_read_u32(np, "pe45_r_cable_3a_lower", &val) >= 0)
		pe4->pe45_r_cable_3a_lower = val;
	else if (of_property_read_u32(np, "pe45-r-cable-3a-lower", &val) >= 0)
		pe4->pe45_r_cable_3a_lower = val;
	else {
		pe4_err("use default pe45_r_cable_3a_lower:%d\n", 252);
		pe4->pe45_r_cable_3a_lower = 252;
	}

	/* single charger */
	if (of_property_read_u32(np, "sc_input_current", &val)
		>= 0) {
		pe4->sc_input_current = val;
	} else if (of_property_read_u32(np, "sc-input-current", &val)
		>= 0) {
		pe4->sc_input_current = val;
	} else {
		pe4_err("use default sc_input_current:%d\n", 3000000);
		pe4->sc_input_current = 3000000;
	}

	if (of_property_read_u32(np, "sc_charger_current", &val)
		>= 0) {
		pe4->sc_charger_current = val;
	} else if (of_property_read_u32(np, "sc-charger-current", &val)
		>= 0) {
		pe4->sc_charger_current = val;
	} else {
		pe4_err("use default sc_charger_current:%d\n", 3000000);
		pe4->sc_charger_current = 3000000;
	}

	/* dual charger in series*/
	if (of_property_read_u32(np, "dcs_input_current", &val)
		>= 0) {
		pe4->dcs_input_current = val;
	} else if (of_property_read_u32(np, "dcs-input-current", &val)
		>= 0) {
		pe4->dcs_input_current = val;
	} else {
		pe4_err("use default dcs_input_current:%d\n", 3000000);
		pe4->dcs_input_current = 3000000;
	}

	if (of_property_read_u32(np, "dcs_chg1_charger_current", &val)
		>= 0) {
		pe4->dcs_chg1_charger_current = val;
	} else if (of_property_read_u32(np, "dcs-chg1-charger-current", &val)
		>= 0) {
		pe4->dcs_chg1_charger_current = val;
	} else {
		pe4_err("use default dcs_chg1_charger_current:%d\n", 1500000);
		pe4->dcs_chg1_charger_current = 1500000;
	}

	if (of_property_read_u32(np, "dcs_chg2_charger_current", &val)
		>= 0) {
		pe4->dcs_chg2_charger_current = val;
	} else if (of_property_read_u32(np, "dcs-chg2-charger-current", &val)
		>= 0) {
		pe4->dcs_chg2_charger_current = val;
	} else {
		pe4_err("use default dcs_chg2_charger_current:%d\n", 1500000);
		pe4->dcs_chg2_charger_current = 1500000;
	}

	if (of_property_read_u32(np, "dual_polling_ieoc", &val) >= 0)
		pe4->dual_polling_ieoc = val;
	else if (of_property_read_u32(np, "dual-polling-ieoc", &val) >= 0)
		pe4->dual_polling_ieoc = val;
	else {
		pr_notice("use default dual_polling_ieoc :%d\n", 750000);
		pe4->dual_polling_ieoc = 750000;
	}

	if (of_property_read_u32(np, "slave_mivr_diff", &val) >= 0)
		pe4->slave_mivr_diff = val;
	else if (of_property_read_u32(np, "slave-mivr-diff", &val) >= 0)
		pe4->slave_mivr_diff = val;
	else {
		pr_notice("use default slave_mivr_diff:%d\n",
			PE4_SLAVE_MIVR_DIFF);
		pe4->slave_mivr_diff = PE4_SLAVE_MIVR_DIFF;
	}

	if (of_property_read_u32(np, "vbat_threshold", &val) >= 0)
		pe4->vbat_threshold = val;
	else if (of_property_read_u32(np, "vbat-threshold", &val) >= 0)
		pe4->vbat_threshold = val;
	else {
		pr_notice("turn off vbat_threshold checking:%d\n",
			DISABLE_VBAT_THRESHOLD);
		pe4->vbat_threshold = DISABLE_VBAT_THRESHOLD;
	}
	pe4->enable_inductor_protect = false;
	if (of_property_read_u32_array(np, "pe45-r-cable-level", pe4->r_cable_level, 5) >= 0) {
		if (of_property_read_u32_array(np, "pe45-r-cable-voltage", pe4->r_cable_voltage, 5) >= 0) {
			if (of_property_read_u32_array(np, "pe45-r-cable-current-limit",
			pe4->r_cable_current_limit, 5) >= 0)
				pe4->enable_inductor_protect = true;
		}
	}

	if (!pe4->enable_inductor_protect)
		pr_notice("disable inductor protection\n");

}

int _pe4_get_status(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value)
{

	if (s == ALG_MAX_VBUS)
		*value = 10000;
	else
		pr_notice("%s does not support prop:%d\n", __func__, s);
	return 0;
}

int _pe4_set_setting(struct chg_alg_device *alg_dev,
	struct chg_limit_setting *setting)
{
	struct mtk_pe45 *pe4;

	pe4 = dev_get_drvdata(&alg_dev->dev);

	pe4_dbg("%s cv:%d icl:%d,%d cc:%d,%d, 6pin_en:%d\n",
		__func__,
		setting->cv,
		setting->input_current_limit1,
		setting->input_current_limit2,
		setting->charging_current_limit1,
		setting->charging_current_limit2,
		setting->vbat_mon_en);

	mutex_lock(&pe4->access_lock);
	__pm_stay_awake(pe4->suspend_lock);
	pe4->cv = setting->cv;
	pe4->pe4_6pin_en = setting->vbat_mon_en;
	pe4->input_current_limit1 = setting->input_current_limit1;
	pe4->input_current_limit2 = setting->input_current_limit2;
	pe4->charging_current_limit1 = setting->charging_current_limit1;
	pe4->charging_current_limit2 = setting->charging_current_limit2;

	pe4_dbg("%s cv:%d icl1:%d:%d icl2:%d:%d icl:%d:%d cc:%d:%d, pe4_6pin_en:%d\n",
		__func__,
		setting->cv,
		pe4->input_current1,
		pe4->input_current_limit2,
		pe4->input_current2,
		pe4->input_current_limit2,
		pe4->pe4_input_current_limit,
		pe4->pe4_input_current_limit_setting,
		pe4->charger_current1,
		pe4->charger_current2,
		pe4->pe4_6pin_en);

	__pm_relax(pe4->suspend_lock);
	mutex_unlock(&pe4->access_lock);

	return 0;
}

int _pe4_set_prop(struct chg_alg_device *alg,
		enum chg_alg_props s, int value)
{
	struct mtk_pe45 *pe45;

	pr_notice("%s %d %d\n", __func__, s, value);

	pe45 = dev_get_drvdata(&alg->dev);

	switch (s) {
	case ALG_LOG_LEVEL:
		pe4_dbg_level = value;
		break;
	case ALG_REF_VBAT:
		pe45->ref_vbat = value;
		break;
	default:
		break;
	}

	return 0;
}

static struct chg_alg_ops pe4_alg_ops = {
	.init_algo = _pe4_init_algo,
	.is_algo_ready = _pe4_is_algo_ready,
	.start_algo = _pe4_start_algo,
	.is_algo_running = _pe4_is_algo_running,
	.stop_algo = _pe4_stop_algo,
	.notifier_call = _pe4_notifier_call,
	.get_prop = _pe4_get_status,
	.set_prop = _pe4_set_prop,
	.set_current_limit = _pe4_set_setting,
};

static int mtk_pe45_probe(struct platform_device *pdev)
{
	struct mtk_pe45 *pe4 = NULL;

	pr_notice("%s: starts\n", __func__);

	pe4 = devm_kzalloc(&pdev->dev, sizeof(*pe4), GFP_KERNEL);
	if (!pe4)
		return -ENOMEM;
	platform_set_drvdata(pdev, pe4);
	pe4->pdev = pdev;
	mutex_init(&pe4->access_lock);
	mutex_init(&pe4->data_lock);
	pe4->suspend_lock =
		wakeup_source_register(NULL, "PE4.5 suspend wakelock");

	mtk_pe4_parse_dt(pe4, &pdev->dev);
	pe4->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev, "gauge");

	if (IS_ERR_OR_NULL(pe4->bat_psy))
		pe4_err("%s: devm power fail to get pe4->bat_psy\n", __func__);

	pe4->alg = chg_alg_device_register("pe45", &pdev->dev,
					pe4, &pe4_alg_ops, NULL);

	return 0;
}

static int mtk_pe4_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_pe4_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id mtk_pe4_of_match[] = {
	{.compatible = "mediatek,charger,pe45",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pe4_of_match);

struct platform_device pe4_device = {
	.name = "pe45",
	.id = -1,
};

static struct platform_driver pe4_driver = {
	.probe = mtk_pe45_probe,
	.remove = mtk_pe4_remove,
	.shutdown = mtk_pe4_shutdown,
	.driver = {
		   .name = "pe45",
		   .of_match_table = mtk_pe4_of_match,
	},
};

static int __init mtk_pe4_init(void)
{
	return platform_driver_register(&pe4_driver);
}
module_init(mtk_pe4_init);

static void __exit mtk_pe4_exit(void)
{
	platform_driver_unregister(&pe4_driver);
}
module_exit(mtk_pe4_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Pump Express 4 algorithm Driver");
MODULE_LICENSE("GPL");
