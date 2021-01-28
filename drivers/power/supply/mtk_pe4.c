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

#include "mtk_pe4.h"
#include "mtk_charger_algorithm_class.h"

#define PE40_VBUS_STEP 50
#define PE40_MIN_WATT 5000000
#define PE40_VBUS_IR_DROP_THRESHOLD 1200

static int pe4_dbg_level = PE4_DEBUG_LEVEL;

int pe4_get_debug_level(void)
{
	return pe4_dbg_level;
}

void mtk_pe40_reset(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe40;

	pe40 = dev_get_drvdata(&alg->dev);

	if (pe40->state == PE4_RUN) {
		pe4_hal_set_adapter_cap_end(alg, 5000, 2000);

		pe4_hal_set_mivr(alg, CHG1, pe40->min_charger_voltage);
		pe4_hal_enable_vbus_ovp(alg, true);
		pe40->polling_interval = 10;
		pe40->state = PE4_STOP;
		pe4_err("set TD true\n");
		pe4_hal_enable_termination(alg, CHG1, true);
	}

	pe40->cap.nr = 0;
	pe40->pe4_input_current_limit = -1;
	pe40->pe4_input_current_limit_setting = -1;
	pe40->max_vbus = pe40->pe40_max_vbus;
	pe40->max_ibus = pe40->pe40_max_ibus;
	pe40->max_charger_ibus = pe40->pe40_max_ibus *
				(100 - pe40->ibus_err) / 100;
}


static int _pe4_init_algo(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;
	int cnt;

	pe4 = dev_get_drvdata(&alg->dev);
	pe4_dbg("%s\n", __func__);

	mutex_lock(&pe4->access_lock);
	if (pe4_hal_init_hardware(alg) != 0) {
		pe4->state = PE4_HW_FAIL;
		pe4_err("%s:init hw fail\n", __func__);
	} else
		pe4->state = PE4_HW_READY;
	mtk_pe40_reset(alg);

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
	case PE4_STOP:
		return "PE4_STOP";
	default:
		break;
	}
	pe4_err("%s unknown state:%d\n", __func__
		, state);
	return "PE4_UNKNOWN";
}

static int _pe4_is_algo_ready(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;
	int ret_value, uisoc;
	int ret, tmp;
	int oldstate;

	pe4 = dev_get_drvdata(&alg->dev);

	mutex_lock(&pe4->access_lock);
	__pm_stay_awake(pe4->suspend_lock);
	oldstate = pe4->state;

	switch (pe4->state) {
	case PE4_HW_UNINIT:
	case PE4_HW_FAIL:
		ret_value = ALG_INIT_FAIL;
		break;
	case PE4_STOP:
	case PE4_HW_READY:
		uisoc = pe4_hal_get_uisoc(alg);
		ret = pe4_hal_is_pd_adapter_ready(alg);
		ret_value = ret;
		if (ret == ALG_READY) {
			uisoc = pe4_hal_get_uisoc(alg);
			tmp = pe4_hal_get_battery_temperature(alg);
			pe4_err("c:%d,%d uisoc:%d,%d tmp:%d,%d,%d\n",
				pe4->input_current_limit1,
				pe4->charging_current_limit1,
				uisoc,
				pe4->pe40_stop_battery_soc,
				tmp,
				pe4->high_temp_to_enter_pe40,
				pe4->low_temp_to_enter_pe40);
			if (pe4->input_current_limit1 != -1 ||
				pe4->charging_current_limit1 != -1 ||
				uisoc > pe4->pe40_stop_battery_soc ||
				uisoc == -1 ||
				tmp > pe4->high_temp_to_enter_pe40 ||
				tmp < pe4->low_temp_to_enter_pe40)
				ret_value = ALG_NOT_READY;
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

	pe4_dbg("%s state:%s=>%s ret:%d\n", __func__,
		pe4_state_to_str(oldstate),
		pe4_state_to_str(pe4->state),
		ret_value);


	__pm_relax(pe4->suspend_lock);
	mutex_unlock(&pe4->access_lock);

	return ret_value;
}

void mtk_pe40_init_cap(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);
	pe4_hal_get_adapter_cap(alg, &pe4->cap);
}

int mtk_pe40_get_setting_by_watt(struct chg_alg_device *alg, int *voltage,
	int *adapter_ibus, int *actual_current, int watt,
	int *ibus_current_setting)
{
	int i;
	struct mtk_pe40 *pe40;
	struct pe4_power_cap *pe40_cap;
	int vbus = 0, ibus = 0, ibus_setting = 0;
	int idx = 0, ta_ibus = 0;

	pe40 = dev_get_drvdata(&alg->dev);

	pe40_cap = &pe40->cap;

	pe4_dbg("%s cv:%d icl:%d:%d:%d:%d, watt:%d pdp:%d,%d\n",
		__func__,
		pe40->cv,
		pe40->input_current_limit1,
		pe40->input_current_limit2,
		pe40->pe4_input_current_limit,
		pe40->pe4_input_current_limit_setting,
		watt,
		pe40_cap->pwr_limit[i],
		pe40_cap->pdp);


	for (i = 0; i < pe40_cap->nr; i++) {
		int max_ibus = 0;
		int max_vbus = 0;

		/* update upper bound */
		if (pe40_cap->ma[i] > pe40->max_ibus)
			max_ibus = pe40->max_ibus;
		else
			max_ibus = pe40_cap->ma[i];
		pe4_err("1.%d %d %d %d\n",
			pe40_cap->ma[i],
			pe40->max_ibus,
			max_ibus,
			pe40->pe40_max_ibus);

		if (pe40->input_current_limit1 != -1 &&
			max_ibus > pe40->input_current_limit1 / 1000)
			max_ibus = pe40->input_current_limit1 / 1000;

		pe4_err("2.%d %d\n",
			pe40->input_current_limit1,
			max_ibus);

		if (pe40->pe4_input_current_limit != -1 &&
			max_ibus > (pe40->pe4_input_current_limit / 1000))
			max_ibus = pe40->pe4_input_current_limit / 1000;

		pe4_err("3.%d %d\n",
			pe40->pe4_input_current_limit,
			max_ibus);


		pe40->max_charger_ibus = max_ibus *
					(100 - pe40->ibus_err) / 100;

		pe4_err("idx:%d nr:%d mV:%d:%d mA:%d\n",
			i,
			pe40_cap->nr,
			pe40_cap->max_mv[i],
			pe40_cap->min_mv[i],
			pe40_cap->ma[i]);

		pe4_err("ibus:%d %d err:%d\n",
			pe40->max_charger_ibus,
			max_ibus,
			pe40->ibus_err);


		if (pe40_cap->max_mv[i] > pe40->max_vbus)
			max_vbus = pe40->max_vbus;
		else
			max_vbus = pe40_cap->max_mv[i];

		if (*voltage != 0 && *voltage <= max_vbus &&
			*voltage >= pe40_cap->min_mv[i]) {
			ibus = watt / *voltage;
			vbus = *voltage;
			ibus_setting = max_ibus;
			ta_ibus = pe40_cap->ma[i];
			if (ibus <= max_ibus) {
				idx = 1;
				break;
			}
		}

		/* is 5v ok ? */
		if (max_vbus >= 5000 &&
			pe40_cap->min_mv[i] <= 5000 &&
			5000 * pe40->max_charger_ibus >= watt) {
			vbus = 5000;
			ibus = watt / 5000;
			ibus_setting = max_ibus;
			ta_ibus = pe40_cap->ma[i];
			idx = 2;
			break;
		}

		/* is power limit set */
		if (pe40_cap->pwr_limit[i] && pe40_cap->pdp > 0) {
			if (watt > pe40_cap->pdp * 1000000)
				watt = pe40_cap->pdp * 1000000;

			if (max_vbus * (pe40->max_charger_ibus - 200) >= watt) {
				ibus = pe40->max_charger_ibus - 200;
				vbus = watt / ibus;
				ibus_setting = max_ibus;
				ta_ibus = pe40_cap->ma[i];
				if (vbus > max_vbus)
					vbus = max_vbus;
				if (vbus < pe40_cap->min_mv[i])
					vbus = pe40_cap->min_mv[i];

				idx = 4;
				break;
			}
		}

		/* is max watt ok */
		if (max_vbus * (pe40->max_charger_ibus - 200) >= watt &&
			!pe40_cap->pwr_limit[i]) {
			ibus = pe40->max_charger_ibus - 200;
			vbus = watt / ibus;
			ibus_setting = max_ibus;
			ta_ibus = pe40_cap->ma[i];
			if (vbus < pe40_cap->min_mv[i])
				vbus = pe40_cap->min_mv[i];

			idx = 3;
			break;
		}

		vbus = max_vbus;
		ibus = pe40->max_charger_ibus;
		ibus_setting = max_ibus;
		ta_ibus = pe40_cap->ma[i];
		idx = 5;

	}

	*voltage = vbus;
	*ibus_current_setting = ibus_setting;
	*actual_current = ibus;
	*adapter_ibus = ta_ibus;

	pe4_err("%s:[%d,%d]%d vbus:%d ibus:%d aicl:%d current:%d %d\n",
		__func__,
		idx, i,
		watt, *voltage,
		*adapter_ibus,
		*ibus_current_setting,
		ibus, pe40->max_charger_ibus);

	return idx;
}

int mtk_pe40_pd_1st_request(struct chg_alg_device *alg,
	int adapter_mv, int adapter_ma, int ma)
{
	unsigned int oldmA = 3000000;
	int ret;
	int mivr;
//	bool chg2_enable = false;
	struct mtk_pe40 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (is_dual_charger_supported(pinfo))
		charger_dev_is_enabled(pinfo->chg2_dev, &chg2_enable);
#endif

	mivr = pe4->min_charger_voltage / 1000;
	pe4_hal_set_mivr(alg, CHG1, pe4->min_charger_voltage);
	pe4_hal_get_input_current(alg, CHG1, &oldmA);
	oldmA = oldmA / 1000;

	pe4_err("pe40_pd_req:vbus:%d ibus:%d input_current:%d %d\n",
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
	if (oldmA > ma)
		pe4_hal_set_input_current(alg, CHG1, ma * 1000);
#endif

	if ((adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD;

	pe4_hal_set_mivr(alg, CHG1, mivr * 1000);
	pe4->pe4_input_current_limit_setting = ma * 1000;
	return ret;
}

int mtk_pe40_pd_request(struct chg_alg_device *alg,
	int *adapter_vbus, int *adapter_ibus, int ma)
{
	unsigned int oldmA = 3000000;
	unsigned int oldmivr = 4600;
	int ret;
	int mivr;
	int adapter_mv, adapter_ma;
	struct mtk_pe40 *pe40;

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	bool chg2_enable = false;

	if (is_dual_charger_supported(pinfo))
		charger_dev_is_enabled(pinfo->chg2_dev, &chg2_enable);
#endif

	pe40 = dev_get_drvdata(&alg->dev);
	adapter_mv = *adapter_vbus;
	adapter_ma = *adapter_ibus;

	pe4_hal_get_mivr(alg, CHG1, &oldmivr);

	mivr = pe40->min_charger_voltage / 1000;
	pe4_hal_set_mivr(alg, CHG1, pe40->min_charger_voltage);

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
	pe4_hal_set_input_current(alg, CHG1, ma * 1000);
#endif

	ret = pe4_hal_set_adapter_cap(alg, adapter_mv, adapter_ma);

	pe4_err("pe40_pd_req:vbus:%d ibus:%d input_current:%d ret:%d\n",
		adapter_mv, adapter_ma, ma, ret);

	if (ret == MTK_ADAPTER_PE4_REJECT) {
		pe4_err("pe40_pd_req: reject\n");

		if (pe40->cap.pdp > 0 &&
			adapter_mv * adapter_ma > pe40->cap.pdp * 1000000) {
			*adapter_ibus = pe40->cap.pdp * 1000000
					/ adapter_mv;
			ret = pe4_hal_set_adapter_cap(alg,
				adapter_mv, adapter_ma);

			pe4_err("pe40_pd_req:vbus:%d new_ibus:%d pdp:%d ret:%d\n",
				adapter_mv, *adapter_ibus,
				pe40->cap.pdp, ret);

			if (ret == MTK_ADAPTER_PE4_OK)
				ret = MTK_ADAPTER_PE4_ADJUST;

			if (ret == MTK_ADAPTER_PE4_REJECT)
				goto err;
		} else
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
	pe4_hal_set_input_current(alg, CHG1, ma * 1000);
#endif


	if ((adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD;

	pe4_hal_set_mivr(alg, CHG1, mivr * 1000);

	pe40->pe4_input_current_limit_setting = ma * 1000;
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

int mtk_pe40_get_ibus(struct chg_alg_device *alg, u32 *ibus)
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

		pe4_err("[%s] chg2_watt:%d ibat2:%d ibat1:%d ibat:%d ibus1:%d ibus2:%d ibus:%d\n",
			__func__, chg2_watt, chg2_ibat, chg1_ibat, ibat * 100,
			chg1_ibus, chg2_ibus, *ibus);
	} else {
		ret = charger_dev_get_ibus(pinfo->chg1_dev, ibus);
	}
#endif
	pe4_hal_get_ibus(alg, ibus);

	return 0;
}

int mtk_pe40_get_init_watt(struct chg_alg_device *alg)
{
	int ret;
	struct mtk_pe40 *pe40;
	int vbus1, ibus1;
	int vbus2, ibus2;
	int vbat1, vbat2;
	int voltage = 0, input_current = 1000, actual_current = 0;
	int voltage1 = 0, adapter_ibus;
	bool is_enable = false, is_chip_enable = false;
	int i;

	pe40 = dev_get_drvdata(&alg->dev);
	voltage = 0;
	mtk_pe40_get_setting_by_watt(alg, &voltage, &adapter_ibus,
		&actual_current, 27000000, &input_current);
	ret = mtk_pe40_pd_request(alg, &voltage, &adapter_ibus,
				input_current);

	if (ret != 0 && ret != MTK_ADAPTER_PE4_REJECT &&
			ret != MTK_ADAPTER_PE4_ADJUST) {
		pe4_err("[pe40_i1] err:1 %d\n", ret);
		return -1;
	}

	for (i = 0; i < 3 ; i++) {
		pe4_hal_dump_registers(alg);
		msleep(100);
	}

	mtk_pe40_get_ibus(alg, &ibus1);
	vbus1 = pe4_hal_get_vbus(alg);
	ibus1 = ibus1 / 1000;
	vbat1 = pe4_hal_get_vbat(alg);
	voltage1 = voltage;

	voltage = 0;
	mtk_pe40_get_setting_by_watt(alg, &voltage, &adapter_ibus,
		&actual_current, 15000000, &input_current);


	for (i = 0; i < 6 ; i++) {
		ret = mtk_pe40_pd_request(alg, &voltage, &adapter_ibus,
			input_current);

		if (ret != 0 && ret != MTK_ADAPTER_PE4_ADJUST) {
			pe4_err("[pe40_i1] err:2 %d\n", ret);
			return -1;
		}

		msleep(100);
		mtk_pe40_get_ibus(alg, &ibus2);
		vbus2 = pe4_hal_get_vbus(alg);
		ibus2 = ibus2 / 1000;
		vbat2 = pe4_hal_get_vbat(alg);

		if (alg->config == DUAL_CHARGERS_IN_SERIES) {
			pe4_hal_is_charger_enable(alg, CHG2, &is_enable);
			is_chip_enable = pe4_hal_is_chip_enable(alg, CHG2);
		}

		pe4_err("[pe40_vbus] vbus1:%d ibus1:%d vbus2:%d ibus2:%d watt:%d en:%d %d vbat:%d %d\n",
			vbus1, ibus1, vbus2, ibus2, voltage1 * ibus1, is_enable,
			is_chip_enable, vbat1, vbat2);
	}

	return voltage1 * ibus1;
}

void mtk_pe40_end(struct chg_alg_device *alg, int type)
{
	mtk_pe40_reset(alg);
	pe4_err("%s: retry:%d\n", __func__, type);
}

int mtk_pe40_init_state(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;
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

	pe4 = dev_get_drvdata(&alg->dev);

	pe4_err("set TD false\n");
	pe4_hal_enable_termination(alg, CHG1, false);
	pe4_hal_enable_vbus_ovp(alg, false);

	mtk_pe40_init_cap(alg);
	voltage = 0;
	mtk_pe40_get_setting_by_watt(alg, &voltage, &adapter_ibus,
		&actual_current, 5000000, &input_current);

	ret = mtk_pe40_pd_1st_request(alg, voltage, actual_current,
		actual_current);

	if (ret != 0) {
		pe4_err("[pe40_i0] err:1 %d\n", ret);
		goto retry;
	}

	/* disable charger */
	pe4_hal_enable_powerpath(alg, CHG1, false);
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

	ret = pe40_hal_get_adapter_output(alg, &cap);

	pe4->can_query = true;
	if (ret == 0 && (cap.output_ma == -1 || cap.output_mv == -1))
		pe4->can_query = false;
	else if (ret == 1)
		pe4->can_query = false;
	else if (ret != 0) {
		pe4_err("[pe40_i0] err:2 %d\n", ret);
		goto err;
	}

	pe4_err("[pe40_i0] can_query:%d ret:%d\n",
		pe4->can_query,
		ret);

	pe4->pmic_vbus = pe4_hal_get_vbus(alg);
	pe4->TA_vbus = cap.output_mv;
	pe4->vbus_cali = pe4->TA_vbus - pe4->pmic_vbus;

	pe4_err("[pe40_i0]pmic_vbus:%d TA_vbus:%d cali:%d ibus:%d chip2:%d\n",
		pe4->pmic_vbus, pe4->TA_vbus, pe4->vbus_cali,
		cap.output_ma, chg2_chip_enabled);

	/*enable charger*/
	pe4_hal_enable_powerpath(alg, CHG1, true);
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
		pe4_err("[pe40_i0] FOD fail :%d\n", cap.output_ma);
		goto err;
	}

	if (pe4->can_query == true) {
		/* measure 1 */
		voltage = 0;
		mtk_pe40_get_setting_by_watt(alg, &voltage, &adapter_ibus,
			&actual_current, 5000000, &input_current);
		ret = mtk_pe40_pd_request(alg, &voltage, &actual_current,
					actual_current);

		if (ret != 0 && ret != MTK_ADAPTER_PE4_ADJUST) {
			pe4_err("[pe40_i0] err:3 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus1 = pe4_hal_get_vbus(alg);
			vbat1 = pe4_hal_get_vbat(alg);
			mtk_pe40_get_ibus(alg, &ibus1);
			ibus1 = ibus1 / 1000;
			ret = pe40_hal_get_adapter_output(alg, &cap1);
			if (ret != 0) {
				pe4_err("[pe40_i0] err:4 %d\n", ret);
				goto err;
			}

			pe4_err("[pe40_i11]vbus:%d ibus:%d vbat:%d TA_vbus:%d TA_ibus:%d setting:%d %d\n",
				vbus1, ibus1, vbat1,
				cap1.output_mv, cap1.output_ma,
				voltage, actual_current);

			if (abs(cap1.output_ma - actual_current) < 200)
				break;
		}


		/* measure 2 */
		voltage = 0;
		mtk_pe40_get_setting_by_watt(alg, &voltage, &adapter_ibus,
			&actual_current, 7500000, &input_current);
		ret = mtk_pe40_pd_request(alg, &voltage, &actual_current,
					actual_current);

		if (ret != 0 && ret != MTK_ADAPTER_PE4_ADJUST) {
			pe4_err("[pe40_i0] err:5 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus2 = pe4_hal_get_vbus(alg);
			vbat2 = pe4_hal_get_vbat(alg);
			mtk_pe40_get_ibus(alg, &ibus2);
			ibus2 = ibus2 / 1000;
			ret = pe40_hal_get_adapter_output(alg, &cap2);
			if (ret != 0)
				goto err;

			pe4_err("[pe40_i12]vbus:%d ibus:%d vbat:%d TA_vbus:%d TA_ibus:%d setting:%d %d\n",
				vbus2, ibus2, vbat2,
				cap2.output_mv, cap2.output_ma,
				voltage, actual_current);
			if (abs(cap2.output_ma - actual_current) < 200)
				break;
		}

		pe4_err("[pe40_i1]vbus:%d,%d,%d,%d ibus:%d,%d,%d,%d vbat:%d,%d\n",
			vbus1, vbus2, cap1.output_mv, cap2.output_mv,
			ibus1, ibus2, cap1.output_ma, cap2.output_ma,
			vbat1, vbat2);

		pe4->r_sw = abs((vbus2 - vbus1) - (vbat2 - vbat1)) * 1000 /
				abs(cap2.output_ma - cap1.output_ma);
		pe4->r_cable = abs((cap2.output_mv - cap1.output_mv) -
				    (vbus2 - vbus1)) * 1000 /
				abs(cap2.output_ma - cap1.output_ma);
		pe4->r_cable_2 = abs(cap2.output_mv - pe4->vbus_cali - vbus2)
				* 1000 / abs(cap2.output_ma);
		pe4->r_cable_1 = abs(cap1.output_mv - pe4->vbus_cali - vbus1)
				* 1000 / abs(cap1.output_ma);

		if (pe4->r_cable_1 < pe4->pe40_r_cable_3a_lower)
			pe4->pe4_input_current_limit = 5000000;
		else if (pe4->r_cable_1 >= pe4->pe40_r_cable_3a_lower &&
			pe4->r_cable_1 < pe4->pe40_r_cable_2a_lower)
			pe4->pe4_input_current_limit = 3000000;
		else if (pe4->r_cable_1 >= pe4->pe40_r_cable_2a_lower &&
			pe4->r_cable_1 < pe4->pe40_r_cable_1a_lower)
			pe4->pe4_input_current_limit = 2000000;
		else if (pe4->r_cable_1 >= pe4->pe40_r_cable_1a_lower)
			pe4->pe4_input_current_limit = 1000000;

		pe4_err("[pe40_i2]r_sw:%d r_cable:%d r_cable_1:%d r_cable_2:%d pe4_icl:%d\n",
			pe4->r_sw, pe4->r_cable, pe4->r_cable_1,
			pe4->r_cable_2, pe4->pe4_input_current_limit);
		} else
			pe4_err("TA does not support query\n");

	watt = mtk_pe40_get_init_watt(alg);
	voltage = 0;
	mtk_pe40_get_setting_by_watt(alg, &voltage, &adapter_ibus,
				&actual_current, watt, &input_current);
	pe4->avbus = voltage / 10 * 10;
	ret = mtk_pe40_pd_request(alg, &pe4->avbus, &adapter_ibus,
				input_current);

	if (ret != 0 && ret != MTK_ADAPTER_PE4_REJECT &&
			ret != MTK_ADAPTER_PE4_ADJUST) {
		pe4_err("[pe40_i0] err:6 %d\n", ret);
		goto err;
	}

	pe4->avbus = voltage;
	pe4->ibus = watt / voltage;
	pe4->watt = watt;

	pe4->state = PE4_RUN;
	pe4->polling_interval = 10;

	return 0;

retry:
	mtk_pe40_end(alg, 0);
	return 0;
err:
	mtk_pe40_end(alg, 2);
	return 0;
}

int mtk_pe40_safety_check(struct chg_alg_device *alg)
{
	int vbus;
	struct mtk_pe40 *pe40;
	struct pe4_pps_status cap;
	//struct pd_status TAstatus = {0,};
	struct pe4_adapter_status TAstatus;
	int ret;
	int tmp;
	int i;
	int high_tmp_cnt = 0;

	pe40 = dev_get_drvdata(&alg->dev);

	TAstatus.ocp = 0;
	TAstatus.otp = 0;
	TAstatus.ovp = 0;
	TAstatus.temperature = 0;

	/* vbus ov */
	vbus = pe4_hal_get_vbus(alg);
	if (vbus - pe40->avbus >= 2000) {
		pe4_err("[pe40_err]vbus ov :vbus:%d avbus:%d\n",
			vbus, pe40->avbus);
		goto err;
	}

	/* cable voltage drop check */
	if (pe40->can_query == true) {
		ret = pe40_hal_get_adapter_output(alg, &cap);
		if (ret != 0) {
			pe4_err("[pe40_err] err:1 %d\n", ret);
			goto err;
		}

		if (cap.output_mv != -1 &&
			(cap.output_mv - vbus) > PE40_VBUS_IR_DROP_THRESHOLD) {
			pe4_err("[pe40_err]vbus ov2 vbus:%d TAvbus:%d %d %d\n",
				vbus, cap.output_mv,
				PE40_VBUS_IR_DROP_THRESHOLD,
				(cap.output_mv - vbus) >
				PE40_VBUS_IR_DROP_THRESHOLD);
			goto err;
		}

		/* TA V_BUS OVP */
		if (cap.output_mv >= pe40->avbus * 12 / 10) {
			pe4_err("[pe40_err]TA vbus ovp :vbus:%d avbus:%d\n",
				cap.output_mv, pe40->avbus);
			goto err;
		}
	}

	/* TA Thermal */
	for (i = 0; i < 3; i++) {
		ret = pe40_hal_get_adapter_status(alg, &TAstatus);
		//ret = tcpm_dpm_pd_get_status(pinfo->tcpc, NULL, &TAstatus);
		if (TAstatus.temperature >= 100 &&
			TAstatus.temperature != 0 &&
			ret != MTK_ADAPTER_PE4_NOT_SUPPORT &&
			ret != MTK_ADAPTER_PE4_TIMEOUT) {
			high_tmp_cnt++;
			pe4_err("[pe40]TA Thermal:%d cnt:%d\n",
				TAstatus.temperature, high_tmp_cnt);
		} else if (ret == MTK_ADAPTER_PE4_TIMEOUT) {
			pe4_err("[pe40]TA adapter_dev_get_status timeout\n");
			goto err;
		} else
			break;

		if (high_tmp_cnt >= 3) {
			pe4_err("[pe40_err]TA Thermal: %d thd:%d cnt:%d\n",
				TAstatus.temperature, 100, high_tmp_cnt);
			goto err;
		}
	}

	if (ret == MTK_ADAPTER_PE4_NOT_SUPPORT)
		pe4_err("[pe40]TA adapter_dev_get_status not support\n");
	else {
		if (TAstatus.ocp || TAstatus.otp || TAstatus.ovp) {

			pe4_err("[pe40_err]TA protect: ocp:%d otp:%d ovp:%d\n",
				TAstatus.ocp,
				TAstatus.otp,
				TAstatus.ovp);
			goto err;
		}

		pe4_err("PD_TA:TA protect: ocp:%d otp:%d ovp:%d tmp:%d\n",
			TAstatus.ocp,
			TAstatus.otp,
			TAstatus.ovp,
			TAstatus.temperature);
	}

	tmp = pe4_hal_get_battery_temperature(alg);

	if (tmp > pe40->high_temp_to_leave_pe40 ||
		tmp < pe40->low_temp_to_leave_pe40) {

		pe4_err("[pe40_err]tmp:%d threshold:%d %d\n",
			tmp, pe40->high_temp_to_leave_pe40,
			pe40->low_temp_to_leave_pe40);
		return 1;
	}

	return 0;
err:
	return -1;
}


int mtk_pe40_cc_state(struct chg_alg_device *alg)
{
	int ibus = 0, vbat, ibat, vbus, compare_ibus = 0;
	int icl, ccl, ccl2, cv, max_icl;
	struct mtk_pe40 *pe40;
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
	unsigned int mivr2 = 0;
	bool chg1_mivr = false;
	bool chg2_mivr = false;
	bool chg2_enable = false;
	bool thermal_skip = false;

	pe40 = dev_get_drvdata(&alg->dev);

	vbat = pe4_hal_get_vbat(alg);
	ibat = pe4_hal_get_ibat(alg);

	mtk_pe40_get_ibus(alg, &ibus);
	ibus = ibus / 1000;
	oldibus = ibus;
	pe4_hal_get_mivr_state(alg, CHG1, &chg1_mivr);
	pe4_hal_get_mivr(alg, CHG1, &mivr1);

	if (alg->config == DUAL_CHARGERS_IN_SERIES) {
		chg2_enable = pe4_hal_is_chip_enable(alg, CHG2);
		if (chg2_enable) {
			pe4_hal_get_mivr_state(alg, CHG2, &chg2_mivr);
			pe4_hal_get_mivr(alg, CHG2, &mivr2);
		}
	}


	vbus = pe4_hal_get_vbus(alg);
	ccl = pe40->charger_current1 / 1000;
	ccl2 = pe40->charger_current1 / 1000;
	cv = pe40->cv / 1000;
	watt = pe40->avbus * ibus;

	icl = pe40->input_current1 / 1000 *
		(100 - pe40->ibus_err) / 100;

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (pinfo->data.parallel_vbus) {
		charger_dev_get_ibus(pinfo->chg1_dev, &compare_ibus);
		compare_ibus = compare_ibus / 1000;


		if (icl > pe40->max_charger_ibus / 2)
			max_icl = pe40->max_charger_ibus / 2;
		else
			max_icl = icl;
	} else {
		compare_ibus = ibus;

		if (icl > pe40->max_charger_ibus)
			max_icl = pe40->max_charger_ibus;
		else
			max_icl = icl;
	}
#else
	compare_ibus = ibus;

	if (icl > pe40->max_charger_ibus)
		max_icl = pe40->max_charger_ibus;
	else
		max_icl = icl;
#endif

	icl_threshold = 100;
	max_watt = pe40->avbus * max_icl;

	pe4_err("[pe40_cc]vbus:%d:%d,ibus:%d,cibus:%d,ibat:%d icl:%d:%d,ccl:%d,%d,vbat:%d,maxIbus:%d,mivr:%d,%d\n",
		pe40->avbus, vbus,
		ibus,
		compare_ibus,
		ibat,
		icl, max_icl,
		ccl, ccl2,
		vbat, pe40->max_charger_ibus,
		chg1_mivr, chg2_mivr);

	if ((chg1_mivr && (vbus < mivr1 / 1000 - 500)) ||
	    (chg2_mivr && (vbus < mivr2 / 1000 - 500))) {
		mtk_pe40_end(alg, 1);
		return 0;
	}

#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
	if (pinfo->data.parallel_vbus) {
		if (pinfo->chg1_data.thermal_input_current_limit != -1 ||
		    pinfo->chg2_data.thermal_input_current_limit != -1)
			thermal_skip = true;
	}
#endif

	if (((chg1_mivr || chg2_mivr) && !thermal_skip) ||
	    ((compare_ibus >= (max_icl - icl_threshold)) && !thermal_skip) ||
	    (compare_ibus <= (max_icl - icl_threshold * 2))) {

		oldavbus = pe40->avbus;

		if (chg1_mivr || chg2_mivr) {
			pe40->avbus = pe40->avbus + 50;
#ifdef PE4_DUAL_CHARGER_IN_PARALLEL
			if (pinfo->data.parallel_vbus)
				new_watt = (pe40->avbus + 50) * icl * 2;
			else
				new_watt = (pe40->avbus + 50) * icl;
#else
		new_watt = (pe40->avbus + 50) * icl;
#endif
		} else if (compare_ibus >= (max_icl - icl_threshold)) {
			pe40->avbus = pe40->avbus + 50;
			new_watt = (pe40->avbus + 50) * ibus;
		} else if (compare_ibus <= (max_icl - icl_threshold * 2)) {
			new_watt = pe40->avbus * pe40->ibus - 500000;
			pe40->avbus = pe40->avbus - 50;
		}

		ret = mtk_pe40_get_setting_by_watt(alg, &pe40->avbus,
				&adapter_ibus, &actual_current, new_watt,
			&input_current);

		if (ibus >= (max_icl - icl_threshold) && ret != 4)
			pe40->polling_interval = 3;


		if (pe40->avbus <= 5000)
			pe40->avbus = 5000;

		if (abs(pe40->avbus - oldavbus) >= 50) {
			ret = mtk_pe40_pd_request(alg, &pe40->avbus,
					&adapter_ibus, input_current);
			if (ret != 0 && ret != MTK_ADAPTER_PE4_REJECT &&
					ret != MTK_ADAPTER_PE4_ADJUST) {
				pe4_err("pe4 end2 error1\n");
				goto err;
			}
		}
		msleep(100);

		vbat = pe4_hal_get_vbat(alg);
		ibat = pe4_hal_get_ibat(alg);
		mtk_pe40_get_ibus(alg, &ibus);
		vbus = pe4_hal_get_vbat(alg);
		ibus = ibus / 1000;
		icl = pe40->input_current_limit1 / 1000;
		ccl = pe40->charger_current1 / 1000;

		pe40->watt = pe40->avbus * ibus;
		pe40->vbus = vbus;
		pe40->ibus = ibus;
	} else
		pe40->polling_interval = 10;

	ret = mtk_pe40_safety_check(alg);
	if (ret == -1) {
		pe4_err("pe4 end2 error2\n");
		goto err;
	}
	if (ret == 1)
		goto disable_hv;

	if (pe40->avbus * oldibus <= PE40_MIN_WATT) {
		if (pe40->charging_current_limit1 != -1 ||
			pe40->input_current_limit1 != -1)
			mtk_pe40_end(alg, 1);

		else
			mtk_pe40_end(alg, 1);
	}

	return 0;

disable_hv:
	mtk_pe40_end(alg, 0);
	return 0;
err:
	mtk_pe40_end(alg, 2);
	return 0;
}

static int pe4_sc_set_charger(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;
	int ichg1_min = -1, aicr1_min = -1;
	int ret;

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
		if (ret != -ENOTSUPP &&
			pe4->charging_current_limit1 < ichg1_min)
			pe4->charger_current1 = 0;
	} else
		pe4->charger_current1 = pe4->sc_charger_current;

	if (pe4->input_current_limit1 != -1 &&
		pe4->input_current_limit1 <
		pe4->sc_input_current) {
		pe4->input_current1 = pe4->input_current_limit1;
		ret = pe4_hal_get_min_input_current(alg, CHG1, &aicr1_min);
		if (ret != -ENOTSUPP &&
			pe4->input_current_limit1 < aicr1_min)
			pe4->input_current1 = 0;
	} else
		pe4->input_current1 = pe4->sc_input_current;
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
	pe4_hal_set_cv(alg,
		CHG1, pe4->cv);

	pe4_dbg("%s m:%d s:%d cv:%d chg1:%d,%d min:%d:%d\n", __func__,
		alg->config,
		pe4->state,
		pe4->cv,
		pe4->input_current1,
		pe4->charger_current1,
		ichg1_min,
		aicr1_min);

	return 0;
}

static int pe4_dcs_set_charger(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;
	bool chg2_enable = true;
	bool chg2_chip_enabled = false;
	int ret;
	int ichg1_min = -1, ichg2_min = -1;
	int aicr1_min = -1;

	pe4 = dev_get_drvdata(&alg->dev);

	if (pe4->input_current_limit1 == 0 ||
		pe4->charging_current_limit1 == 0 ||
		pe4->charging_current_limit2 == 0) {
		pr_notice("input/charging current is 0, end PD\n");
		return -1;
	}

	mutex_lock(&pe4->data_lock);
	if (pe4->input_current_limit1 != -1 &&
		pe4->input_current_limit1 <
		pe4->dcs_input_current) {
		pe4->input_current1 = pe4->input_current_limit1;
		ret = pe4_hal_get_min_input_current(alg, CHG1, &aicr1_min);
		if (ret != -ENOTSUPP &&
			pe4->input_current_limit1 < aicr1_min)
			pe4->input_current1 = 0;
	} else
		pe4->input_current1 = pe4->dcs_input_current;

	if (pe4->charging_current_limit1 != -1 &&
		pe4->charging_current_limit1 <
		pe4->dcs_chg1_charger_current) {
		pe4->charger_current1 = pe4->charging_current_limit1;
		ret = pe4_hal_get_min_charging_current(alg, CHG1, &ichg1_min);
		if (ret != -ENOTSUPP &&
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
		if (ret != -ENOTSUPP &&
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
	pe4_err("chg2_en:%d %d %d\n",
		chg2_enable, chg2_chip_enabled, pe4->state);
	if (pe4->state == PE4_RUN) {
		if (!chg2_chip_enabled)
			pe4_hal_charger_enable_chip(alg, CHG2, true);
		pe4_hal_enable_charger(alg, CHG2, true);
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
		chg2_enable,
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
	struct mtk_pe40 *pe4;
	bool again;
	int uisoc, tmp;

	pe4 = dev_get_drvdata(&alg->dev);
	mutex_lock(&pe4->access_lock);
	__pm_stay_awake(pe4->suspend_lock);

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
				uisoc = pe4_hal_get_uisoc(alg);
				tmp = pe4_hal_get_battery_temperature(alg);
				if (pe4->input_current_limit1 != -1 ||
					pe4->charging_current_limit1 != -1 ||
					uisoc > pe4->pe40_stop_battery_soc ||
					uisoc == -1 ||
					tmp > pe4->high_temp_to_enter_pe40 ||
					tmp < pe4->low_temp_to_enter_pe40) {
					ret_value = ALG_NOT_READY;
					pe4_info("%d %d %d %d %d\n",
						pe4->input_current_limit1,
						pe4->charging_current_limit1,
						pe4->pe40_stop_battery_soc,
						pe4->high_temp_to_enter_pe40,
						pe4->low_temp_to_enter_pe40);
				} else {
					again = true;
					pe4->state = PE4_STOP;
				}
			} else if (ret == ALG_TA_NOT_SUPPORT)
				pe4->state = PE4_TA_NOT_SUPPORT;
			break;
		case PE4_TA_NOT_SUPPORT:
			ret_value = ALG_TA_NOT_SUPPORT;
			break;
		case PE4_STOP:
			pe4_hal_set_charging_current(alg,
				CHG1, pe4->charger_current1);
			mtk_pe40_init_state(alg);
			again = true;
			break;
		case PE4_RUN:
		case PE4_TUNING:
		case PE4_POSTCC:
			pe4_hal_set_charging_current(alg,
				CHG1, pe4->charger_current1);
			_pe4_set_current(alg);
			mtk_pe40_cc_state(alg);
			break;
		default:
			pe4_err("PE4 unknown state:%d\n", pe4->state);
			ret_value = ALG_INIT_FAIL;
			break;
		}
	} while (again == true);
	__pm_relax(pe4->suspend_lock);
	mutex_unlock(&pe4->access_lock);

	return ret_value;
}


static bool _pe4_is_algo_running(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;

	pe4_dbg("%s\n", __func__);
	pe4 = dev_get_drvdata(&alg->dev);

	if (pe4->state == PE4_RUN || PE4_STOP || PE4_TUNING || PE4_POSTCC)
		return true;

	return false;
}

static int _pe4_stop_algo(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);

	pe4_dbg("%s %d\n", __func__, pe4->state);
	if (pe4->state == PE4_RUN || PE4_STOP || PE4_TUNING || PE4_POSTCC)
		pe4->state = PE4_HW_READY;

	return 0;
}

static int pe4_plugout_reset(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;

	pe4 = dev_get_drvdata(&alg->dev);
	switch (pe4->state) {
	case PE4_HW_UNINIT:
	case PE4_HW_FAIL:
	case PE4_HW_READY:
		break;
	case PE4_TA_NOT_SUPPORT:
		pe4->state = PE4_HW_READY;
		break;
	case PE4_STOP:
	case PE4_RUN:
	case PE4_TUNING:
	case PE4_POSTCC:
		if (alg->config == DUAL_CHARGERS_IN_SERIES) {
			pe4_hal_enable_charger(alg, CHG2, false);
			pe4_hal_charger_enable_chip(alg,
			CHG2, false);
		}
		pe4->state = PE4_HW_READY;
		break;
	default:
		break;
	}
	return 0;
}

static int pe4_full_evt(struct chg_alg_device *alg)
{
	struct mtk_pe40 *pe4;
	int ret = 0;
	bool chg_en, chg2_enabled = false;
	int ichg2, ichg2_min;
	int ret_value;

	pe4 = dev_get_drvdata(&alg->dev);
	switch (pe4->state) {
	case PE4_HW_UNINIT:
	case PE4_HW_FAIL:
	case PE4_HW_READY:
	case PE4_TA_NOT_SUPPORT:
	case PE4_STOP:
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
				if (ret == -ENOTSUPP)
					ichg2_min = 100000;

				pe4_err("ichg2:%d, ichg2_min:%d state:%d\n",
					ichg2, ichg2_min, pe4->state);
				if (ichg2 - 500000 <= ichg2_min) {
					pe4->state = PE4_POSTCC;
					pe4_hal_enable_charger(alg,
						CHG2, false);
					pe4_hal_set_eoc_current(alg,
						CHG1, 150000);
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
	struct mtk_pe40 *pe4;
	int ret_value;

	pe4 = dev_get_drvdata(&alg->dev);
	pe4_err("%s evt:%d\n", __func__, notify->evt);

	switch (notify->evt) {
	case EVT_PLUG_OUT:
		ret_value = pe4_plugout_reset(alg);
		break;
	case EVT_FULL:
		ret_value = pe4_full_evt(alg);
		break;
	default:
		ret_value = -EINVAL;
	}

	return ret_value;
}

static void mtk_pe4_parse_dt(struct mtk_pe40 *pe4,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;

	if (of_property_read_u32(np, "pe40_max_vbus", &val) >= 0)
		pe4->pe40_max_vbus = val;
	else {
		pe4_err("use default pe40_max_vbus:%d\n", PE40_MAX_VBUS);
		pe4->pe40_max_vbus = PE40_MAX_VBUS;
	}

	if (of_property_read_u32(np, "pe40_max_ibus", &val) >= 0)
		pe4->pe40_max_ibus = val;
	else {
		pe4_err("use default pe40_max_ibus:%d\n", PE40_MAX_IBUS);
		pe4->pe40_max_ibus = PE40_MAX_IBUS;
	}

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		pe4->min_charger_voltage = val;
	else {
		pe4_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		pe4->min_charger_voltage = V_CHARGER_MIN;
	}

	if (of_property_read_u32(np, "pe40_stop_battery_soc", &val) >= 0)
		pe4->pe40_stop_battery_soc = val;
	else {
		pe4_err("use default pe40_stop_battery_soc:%d\n", 80);
		pe4->pe40_stop_battery_soc = 80;
	}

	if (of_property_read_u32(np, "high_temp_to_leave_pe40", &val) >= 0) {
		pe4->high_temp_to_leave_pe40 = val;
	} else {
		pe4_err("use default high_temp_to_leave_pe40:%d\n",
			HIGH_TEMP_TO_LEAVE_PE40);
		pe4->high_temp_to_leave_pe40 = HIGH_TEMP_TO_LEAVE_PE40;
	}

	if (of_property_read_u32(np, "high_temp_to_enter_pe40", &val) >= 0) {
		pe4->high_temp_to_enter_pe40 = val;
	} else {
		pe4_err("use default high_temp_to_enter_pe40:%d\n",
			HIGH_TEMP_TO_ENTER_PE40);
		pe4->high_temp_to_enter_pe40 = HIGH_TEMP_TO_ENTER_PE40;
	}

	if (of_property_read_u32(np, "low_temp_to_leave_pe40", &val) >= 0) {
		pe4->low_temp_to_leave_pe40 = val;
	} else {
		pe4_err("use default low_temp_to_leave_pe40:%d\n",
			LOW_TEMP_TO_LEAVE_PE40);
		pe4->low_temp_to_leave_pe40 = LOW_TEMP_TO_LEAVE_PE40;
	}

	if (of_property_read_u32(np, "low_temp_to_enter_pe40", &val) >= 0) {
		pe4->low_temp_to_enter_pe40 = val;
	} else {
		pe4_err("use default low_temp_to_enter_pe40:%d\n",
			LOW_TEMP_TO_ENTER_PE40);
		pe4->low_temp_to_enter_pe40 = LOW_TEMP_TO_ENTER_PE40;
	}

	if (of_property_read_u32(np, "ibus_err", &val) >= 0) {
		pe4->ibus_err = val;
	} else {
		pe4_err("use default ibus_err:%d\n",
			IBUS_ERR);
		pe4->ibus_err = IBUS_ERR;
	}

	if (of_property_read_u32(np, "pe40_r_cable_1a_lower", &val) >= 0)
		pe4->pe40_r_cable_1a_lower = val;
	else {
		pe4_err("use default pe40_r_cable_1a_lower:%d\n", 530);
		pe4->pe40_r_cable_1a_lower = 530;
	}

	if (of_property_read_u32(np, "pe40_r_cable_2a_lower", &val) >= 0)
		pe4->pe40_r_cable_2a_lower = val;
	else {
		pe4_err("use default pe40_r_cable_2a_lower:%d\n", 390);
		pe4->pe40_r_cable_2a_lower = 390;
	}

	if (of_property_read_u32(np, "pe40_r_cable_3a_lower", &val) >= 0)
		pe4->pe40_r_cable_3a_lower = val;
	else {
		pe4_err("use default pe40_r_cable_3a_lower:%d\n", 252);
		pe4->pe40_r_cable_3a_lower = 252;
	}

	/* single charger */
	if (of_property_read_u32(np, "sc_input_current", &val)
		>= 0) {
		pe4->sc_input_current = val;
	} else {
		pe4_err("use default sc_input_current:%d\n", 3000000);
		pe4->sc_input_current = 3000000;
	}

	if (of_property_read_u32(np, "sc_charger_current", &val)
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
	} else {
		pe4_err("use default dcs_input_current:%d\n", 3000000);
		pe4->dcs_input_current = 3000000;
	}

	if (of_property_read_u32(np, "dcs_chg1_charger_current", &val)
		>= 0) {
		pe4->dcs_chg1_charger_current = val;
	} else {
		pe4_err("use default dcs_chg1_charger_current:%d\n", 1500000);
		pe4->dcs_chg1_charger_current = 1500000;
	}

	if (of_property_read_u32(np, "dcs_chg2_charger_current", &val)
		>= 0) {
		pe4->dcs_chg2_charger_current = val;
	} else {
		pe4_err("use default dcs_chg2_charger_current:%d\n", 1500000);
		pe4->dcs_chg2_charger_current = 1500000;
	}

	if (of_property_read_u32(np, "dual_polling_ieoc", &val) >= 0)
		pe4->dual_polling_ieoc = val;
	else {
		pr_notice("use default dual_polling_ieoc :%d\n", 750000);
		pe4->dual_polling_ieoc = 750000;
	}

	if (of_property_read_u32(np, "slave_mivr_diff", &val) >= 0)
		pe4->slave_mivr_diff = val;
	else {
		pr_notice("use default slave_mivr_diff:%d\n",
			PE4_SLAVE_MIVR_DIFF);
		pe4->slave_mivr_diff = PE4_SLAVE_MIVR_DIFF;
	}

}

int _pe4_get_status(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value)
{

	pr_notice("%s\n", __func__);
	if (s == ALG_MAX_VBUS)
		*value = 10000;
	else
		pr_notice("%s does not support prop:%d\n", __func__, s);
	return 0;
}

int _pe4_set_setting(struct chg_alg_device *alg_dev,
	struct chg_limit_setting *setting)
{
	struct mtk_pe40 *pe4;

	pe4 = dev_get_drvdata(&alg_dev->dev);

	pe4_dbg("%s cv:%d icl:%d,%d cc:%d,%d\n",
		__func__,
		setting->cv,
		setting->input_current_limit1,
		setting->input_current_limit2,
		setting->charging_current_limit1,
		setting->charging_current_limit2);

	mutex_lock(&pe4->access_lock);
	__pm_stay_awake(pe4->suspend_lock);
	pe4->cv = setting->cv;
	pe4->input_current_limit1 = setting->input_current_limit1;
	pe4->input_current_limit2 = setting->input_current_limit2;
	pe4->charging_current_limit1 = setting->charging_current_limit1;
	pe4->charging_current_limit2 = setting->charging_current_limit2;

	pe4_dbg("%s cv:%d icl1:%d:%d icl2:%d:%d icl:%d:%d cc:%d:%d\n",
		__func__,
		setting->cv,
		pe4->input_current1,
		pe4->input_current_limit2,
		pe4->input_current2,
		pe4->input_current_limit2,
		pe4->pe4_input_current_limit,
		pe4->pe4_input_current_limit_setting,
		pe4->charger_current1,
		pe4->charger_current2);

	__pm_relax(pe4->suspend_lock);
	mutex_unlock(&pe4->access_lock);

	return 0;
}

int _pe4_set_prop(struct chg_alg_device *alg,
		enum chg_alg_props s, int value)
{
	pr_notice("%s %d %d\n", __func__, s, value);
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

static int mtk_pe4_probe(struct platform_device *pdev)
{
	struct mtk_pe40 *pe4 = NULL;

	pr_notice("%s: starts\n", __func__);

	pe4 = devm_kzalloc(&pdev->dev, sizeof(*pe4), GFP_KERNEL);
	if (!pe4)
		return -ENOMEM;
	platform_set_drvdata(pdev, pe4);
	pe4->pdev = pdev;
	mutex_init(&pe4->access_lock);
	mutex_init(&pe4->data_lock);
	pe4->suspend_lock =
		wakeup_source_register(NULL, "PE4.0 suspend wakelock");

	mtk_pe4_parse_dt(pe4, &pdev->dev);

	pe4->alg = chg_alg_device_register("pe4", &pdev->dev,
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
	{.compatible = "mediatek,charger,pe4",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pe4_of_match);

struct platform_device pe4_device = {
	.name = "pe4",
	.id = -1,
};

static struct platform_driver pe4_driver = {
	.probe = mtk_pe4_probe,
	.remove = mtk_pe4_remove,
	.shutdown = mtk_pe4_shutdown,
	.driver = {
		   .name = "pe4",
		   .of_match_table = mtk_pe4_of_match,
	},
};

static int __init mtk_pe4_init(void)
{
	return platform_driver_register(&pe4_driver);
}
late_initcall(mtk_pe4_init);

static void __exit mtk_pe4_exit(void)
{
	platform_driver_unregister(&pe4_driver);
}
module_exit(mtk_pe4_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Pump Express 4 algorithm Driver");
MODULE_LICENSE("GPL");

