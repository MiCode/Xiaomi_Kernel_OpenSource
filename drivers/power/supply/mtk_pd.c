// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_pd.c
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

#include "mtk_pd.h"
#include "mtk_charger_algorithm_class.h"

static int pd_dbg_level = PD_DEBUG_LEVEL;
#define PD_VBUS_IR_DROP_THRESHOLD 1200


int pd_get_debug_level(void)
{
	return pd_dbg_level;
}

static char *pd_state_to_str(int state)
{
	switch (state) {
	case PD_HW_UNINIT:
		return "PD_HW_UNINIT";
	case PD_HW_FAIL:
		return "PD_HW_FAIL";
	case PD_HW_READY:
		return "PD_HW_READY";
	case PD_TA_NOT_SUPPORT:
		return "PD_TA_NOT_SUPPORT";
	case PD_STOP:
		return "PD_STOP";
	case PD_RUN:
		return "PD_RUN";
	case PD_DONE:
		return "PD_DONE";
	default:
		break;
	}
	pd_err("%s unknown state:%d\n", __func__
		, state);
	return "PD_UNKNOWN";
}

static int _pd_init_algo(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;

	pd = dev_get_drvdata(&alg->dev);
	pd_dbg("%s\n", __func__);

	mutex_lock(&pd->access_lock);
	if (pd_hal_init_hardware(alg) != 0) {
		pd->state = PD_HW_FAIL;
		pd_err("%s:init hw fail\n", __func__);
	} else
		pd->state = PD_HW_READY;

	pd->pdc_max_watt_setting = -1;

	pd->check_impedance = true;
	pd->pd_cap_max_watt = -1;
	pd->pd_idx = -1;
	pd->pd_reset_idx = -1;
	pd->pd_boost_idx = 0;
	pd->pd_buck_idx = 0;

	mutex_unlock(&pd->access_lock);
	return 0;
}

static int _pd_is_algo_ready(struct chg_alg_device *alg)
{
	return pd_hal_is_pd_adapter_ready(alg);
}

void __mtk_pdc_init_table(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);

	pd->cap.nr = 0;
	pd->cap.selected_cap_idx = -1;

	if (pd_hal_is_pd_adapter_ready(alg) == ALG_READY)
		pd_hal_get_adapter_cap(alg, &pd->cap);
	else
		pd_err("mtk_is_pdc_ready is fail\n");

	pd_err("[%s] nr:%d default:%d\n", __func__, pd->cap.nr,
	pd->cap.selected_cap_idx);
}

void __mtk_pdc_get_reset_idx(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	struct pd_power_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;

	if (pd->pd_reset_idx == -1) {
		for (i = 0; i < cap->nr; i++) {

			if (cap->min_mv[i] < pd->vbus_l ||
				cap->max_mv[i] < pd->vbus_l ||
				cap->min_mv[i] > pd->vbus_l ||
				cap->max_mv[i] > pd->vbus_l) {
				continue;
			}
			idx = i;
		}
		pd->pd_reset_idx = idx;
		pd_err("[%s]reset idx:%d vbus:%d %d\n", __func__,
			idx, cap->min_mv[idx], cap->max_mv[idx]);
	}
}

void __mtk_pdc_get_cap_max_watt(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	struct pd_power_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;

	if (pd->pd_cap_max_watt == -1) {
		for (i = 0; i < cap->nr; i++) {
			if (cap->min_mv[i] <= pd->vbus_h &&
				cap->min_mv[i] >= pd->vbus_l &&
				cap->max_mv[i] <= pd->vbus_h &&
				cap->max_mv[i] >= pd->vbus_l) {

				if (cap->maxwatt[i] > pd->pd_cap_max_watt) {
					pd->pd_cap_max_watt = cap->maxwatt[i];
					idx = i;
				}
				pd_err("%d %d %d %d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_h,
					pd->vbus_l,
					cap->maxwatt[i],
					pd->pd_cap_max_watt);
				continue;
			}
		}
		pd_err("[%s]idx:%d vbus:%d %d maxwatt:%d\n", __func__,
			idx, cap->min_mv[idx], cap->max_mv[idx],
			pd->pd_cap_max_watt);
	}
}

int __mtk_pdc_get_idx(struct chg_alg_device *alg, int selected_idx,
	int *boost_idx, int *buck_idx)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	struct pd_power_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;
	idx = selected_idx;

	if (idx < 0) {
		pd_err("[%s] invalid idx:%d\n", __func__, idx);
		*boost_idx = 0;
		*buck_idx = 0;
		return -1;
	}

	/* get boost_idx */
	for (i = 0; i < cap->nr; i++) {

		if (cap->min_mv[i] < pd->vbus_l ||
			cap->max_mv[i] < pd->vbus_l) {
			pd_err("min_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_l);
			continue;
		}

		if (cap->min_mv[i] > pd->vbus_h ||
			cap->max_mv[i] > pd->vbus_h) {
			pd_err("max_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_h);
			continue;
		}

		if (idx == selected_idx) {
			if (cap->maxwatt[i] > cap->maxwatt[idx])
				idx = i;
		} else {
			if (cap->maxwatt[i] < cap->maxwatt[idx] &&
				cap->maxwatt[i] > cap->maxwatt[selected_idx])
				idx = i;
		}
	}
	*boost_idx = idx;
	idx = selected_idx;

	/* get buck_idx */
	for (i = 0; i < cap->nr; i++) {

		if (cap->min_mv[i] < pd->vbus_l ||
			cap->max_mv[i] < pd->vbus_l) {
			pd_err("min_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_l);
			continue;
		}

		if (cap->min_mv[i] > pd->vbus_h ||
			cap->max_mv[i] > pd->vbus_h) {
			pd_err("max_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_h);
			continue;
		}

		if (idx == selected_idx) {
			if (cap->maxwatt[i] < cap->maxwatt[idx])
				idx = i;
		} else {
			if (cap->maxwatt[i] > cap->maxwatt[idx] &&
				cap->maxwatt[i] < cap->maxwatt[selected_idx])
				idx = i;
		}
	}
	*buck_idx = idx;

	return 0;
}

int __mtk_pdc_setup(struct chg_alg_device *alg, int idx)
{
	int ret = -100;
	unsigned int mivr;
	unsigned int oldmivr = 4600000;
	unsigned int oldmA = 3000000;
	bool force_update = false;
	int chg_cnt, is_chip_enabled, i;

	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);

	if (pd->pd_idx == idx) {
		pd_hal_get_mivr(alg, CHG1, &oldmivr);

		if (pd->cap.max_mv[idx] - oldmivr / 1000 >
			PD_VBUS_IR_DROP_THRESHOLD)
			force_update = true;

		chg_cnt = pd_hal_get_charger_cnt(alg);
		if (chg_cnt > 1) {
			for (i = CHG2; i < CHG_MAX; i++) {
				is_chip_enabled =
						pd_hal_is_chip_enable(alg, i);
				if (is_chip_enabled) {
					pd_hal_get_mivr(alg, CHG2, &oldmivr);

					if (pd->cap.max_mv[idx] - oldmivr / 1000
						> PD_VBUS_IR_DROP_THRESHOLD -
						pd->slave_mivr_diff / 1000)
						force_update = true;
				}
			}
		}
	}

	if (pd->pd_idx != idx || force_update) {
		if (pd->cap.max_mv[idx] > 5000)
			pd_hal_enable_vbus_ovp(alg, false);
		else
			pd_hal_enable_vbus_ovp(alg, true);

		pd_hal_get_mivr(alg, CHG1, &oldmivr);
		mivr = pd->min_charger_voltage / 1000;
		pd_hal_set_mivr(alg, CHG1, pd->min_charger_voltage);

		pd_hal_get_input_current(alg, CHG1, &oldmA);
		oldmA = oldmA / 1000;

#ifdef FIXME
		if (info->data.parallel_vbus && (oldmA * 2 > pd->cap.ma[idx])) {
			charger_dev_set_input_current(info->chg1_dev,
					pd->cap.ma[idx] * 1000 / 2);
			charger_dev_set_input_current(info->chg2_dev,
					pd->cap.ma[idx] * 1000 / 2);
		} else if (info->data.parallel_vbus == false &&
			(oldmA > pd->cap.ma[idx]))
			charger_dev_set_input_current(info->chg1_dev,
						pd->cap.ma[idx] * 1000);
#endif
		if (oldmA > pd->cap.ma[idx])
			pd_hal_set_input_current(alg, CHG1,
				pd->cap.ma[idx] * 1000);

		ret = pd_hal_set_adapter_cap(alg, pd->cap.max_mv[idx],
			pd->cap.ma[idx]);

		if (ret == 0) {
#ifdef FIXME
			if (info->data.parallel_vbus &&
				(oldmA * 2 < pd->cap.ma[idx])) {
				charger_dev_set_input_current(info->chg1_dev,
						pd->cap.ma[idx] * 1000 / 2);
				charger_dev_set_input_current(info->chg2_dev,
						pd->cap.ma[idx] * 1000 / 2);
			} else if (info->data.parallel_vbus == false &&
				(oldmA < pd->cap.ma[idx]))
				charger_dev_set_input_current(info->chg1_dev,
						pd->cap.ma[idx] * 1000);
#endif

			if (oldmA < pd->cap.ma[idx])
				pd_hal_set_input_current(alg, CHG1,
					pd->cap.ma[idx] * 1000);

			if ((pd->cap.max_mv[idx] - PD_VBUS_IR_DROP_THRESHOLD)
				> mivr)
				mivr = pd->cap.max_mv[idx] -
					PD_VBUS_IR_DROP_THRESHOLD;

			pd_hal_set_mivr(alg, CHG1, mivr * 1000);
		} else {
#ifdef FIXME
			if (info->data.parallel_vbus &&
				(oldmA * 2 > pd->cap.ma[idx])) {
				charger_dev_set_input_current(info->chg1_dev,
						oldmA * 1000 / 2);
				charger_dev_set_input_current(info->chg2_dev,
						oldmA * 1000 / 2);
			} else if (info->data.parallel_vbus == false &&
				(oldmA > pd->cap.ma[idx]))
				charger_dev_set_input_current(info->chg1_dev,
					oldmA * 1000);
#endif
			if (oldmA > pd->cap.ma[idx])
				pd_hal_set_input_current(alg, CHG1,
					oldmA * 1000);

			pd_hal_set_mivr(alg, CHG1, oldmivr);
		}

		__mtk_pdc_get_idx(alg, idx,
			&pd->pd_boost_idx, &pd->pd_buck_idx);
	}

	pd_err("[%s]idx:%d:%d:%d:%d vbus:%d cur:%d ret:%d\n", __func__,
		pd->pd_idx, idx, pd->pd_boost_idx, pd->pd_buck_idx,
		pd->cap.max_mv[idx], pd->cap.ma[idx], ret);

	pd->pd_idx = idx;

	return ret;
}


void mtk_pdc_reset(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);

	pd_err("%s: reset to default profile\n", __func__);
	__mtk_pdc_init_table(alg);
	__mtk_pdc_get_reset_idx(alg);
	__mtk_pdc_setup(alg, pd->pd_reset_idx);
}


int __mtk_pdc_get_setting(struct chg_alg_device *alg, int *newvbus, int *newcur,
			int *newidx)
{
	int ret = 0;
	int idx, selected_idx;
	unsigned int pd_max_watt, pd_min_watt, now_max_watt;
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	int ibus = 0, vbus;
	int chg2_watt = 0;
	bool boost = false, buck = false;
	struct pd_power_cap *cap;
	unsigned int mivr1 = 0;
	unsigned int mivr2 = 0;
	bool chg1_mivr = false;
	bool chg2_mivr = false;
	int chg_cnt, i, is_chip_enabled;


	__mtk_pdc_init_table(alg);
	__mtk_pdc_get_reset_idx(alg);
	__mtk_pdc_get_cap_max_watt(alg);

	cap = &pd->cap;

	if (cap->nr == 0)
		return -1;

	ret = pd_hal_get_ibus(alg, &ibus);
	if (ret < 0) {
		pd_err("[%s] get ibus fail, keep default voltage\n", __func__);
		return -1;
	}

#ifdef FIXME
	if (info->data.parallel_vbus) {
		ret = charger_dev_get_ibat(info->chg1_dev, &chg1_ibat);
		if (ret < 0)
			pd_err("[%s] get ibat fail\n", __func__);

		ret = charger_dev_get_ibat(info->chg2_dev, &chg2_ibat);
		if (ret < 0) {
			ibat = battery_get_bat_current();
			chg2_ibat = ibat * 100 - chg1_ibat;
		}

		if (ibat < 0 || chg2_ibat < 0)
			chg2_watt = 0;
		else
			chg2_watt = chg2_ibat / 1000 * battery_get_bat_voltage()
					/ info->data.chg2_eff * 100;

		pd_err("[%s] chg2_watt:%d ibat2:%d ibat1:%d ibat:%d\n",
			__func__, chg2_watt, chg2_ibat, chg1_ibat, ibat * 100);
	}
#endif

	pd_hal_get_mivr_state(alg, CHG1, &chg1_mivr);
	pd_hal_get_mivr(alg, CHG1, &mivr1);

	chg_cnt = pd_hal_get_charger_cnt(alg);
	if (chg_cnt > 1) {
		for (i = CHG2; i < CHG_MAX; i++) {
			is_chip_enabled =
					pd_hal_is_chip_enable(alg, i);
			if (is_chip_enabled) {
				pd_hal_get_mivr_state(alg, CHG2, &chg2_mivr);
				pd_hal_get_mivr(alg, CHG2, &mivr2);
			}

		}
	}

	vbus = pd_hal_get_vbus(alg);
	ibus = ibus / 1000;
	if (ibus == 0)
		ibus = 1000;

	if ((chg1_mivr && (vbus < mivr1 / 1000 - 500)) ||
	    (chg2_mivr && (vbus < mivr2 / 1000 - 500)))
		goto reset;

	selected_idx = cap->selected_cap_idx;
	idx = selected_idx;

	if (idx < 0 || idx >= PD_CAP_MAX_NR)
		idx = selected_idx = 0;

	pd_max_watt = cap->max_mv[idx] * (cap->ma[idx]
			/ 100 * (100 - pd->ibus_err) - 100);
	now_max_watt = cap->max_mv[idx] * ibus + chg2_watt;
	pd_min_watt = cap->max_mv[pd->pd_buck_idx] * cap->ma[pd->pd_buck_idx]
			/ 100 * (100 - pd->ibus_err)
			- pd->vsys_watt;

	if (pd_min_watt <= 5000000)
		pd_min_watt = 5000000;

	if ((now_max_watt >= pd_max_watt) || chg1_mivr || chg2_mivr) {
		*newidx = pd->pd_boost_idx;
		boost = true;
	} else if (now_max_watt <= pd_min_watt) {
		*newidx = pd->pd_buck_idx;
		buck = true;
	} else {
		*newidx = selected_idx;
		boost = false;
		buck = false;
	}

	*newvbus = cap->max_mv[*newidx];
	*newcur = cap->ma[*newidx];

	pd_err("[%s]watt:%d,%d,%d up:%d,%d vbus:%d ibus:%d, mivr:%d,%d\n",
		__func__,
		pd_max_watt, now_max_watt, pd_min_watt,
		boost, buck,
		vbus, ibus, chg1_mivr, chg2_mivr);

	pd_err("[%s]vbus:%d:%d:%d current:%d idx:%d default_idx:%d\n",
		__func__, pd->vbus_h, pd->vbus_l, *newvbus,
		*newcur, *newidx, selected_idx);

	return 0;

reset:
	mtk_pdc_reset(alg);
	*newidx = pd->pd_reset_idx;
	*newvbus = cap->max_mv[*newidx];
	*newcur = cap->ma[*newidx];

	return 0;
}


static int __pd_run(struct chg_alg_device *alg)
{
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	int vbus, cur, idx, ret;

	ret = __mtk_pdc_get_setting(alg, &vbus, &cur, &idx);
	if (ret != -1 && idx != -1) {
		pd->input_current_limit = cur * 1000;
		pd->charging_current_limit =
			pd->charging_current;
		__mtk_pdc_setup(alg, idx);
	} else {
		pd->input_current_limit =
			PD_FAIL_CURRENT;
		pd->charging_current_limit =
			PD_FAIL_CURRENT;
	}
	pd_err("[%s]vbus:%d input_cur:%d idx:%d current:%d\n",
		__func__, vbus, cur, idx,
		pd->charging_current);

	if (pd->charging_current_limit != -1 &&
		pd->charging_current_limit <
		pd->pd_charging_current)
		pd->charging_current = pd->charging_current_limit;
	else
		pd->charging_current = pd->pd_charging_current;

	if (pd->input_current_limit != -1 &&
		pd->input_current_limit <
		pd->pd_input_current)
		pd->input_current = pd->input_current_limit;
	else
		pd->input_current = pd->pd_input_current;

	pd_hal_set_charging_current(alg,
		CHG1, pd->input_current);
	pd_hal_set_input_current(alg,
		CHG1, pd->charging_current);
	pd_hal_set_cv(alg,
		CHG1, pd->cv);
	return ALG_RUNNING;
}

static int _pd_start_algo(struct chg_alg_device *alg)
{
	int ret_value = 0;
	struct mtk_pd *pd = dev_get_drvdata(&alg->dev);
	bool again;

	mutex_lock(&pd->access_lock);

	do {
		pd_info("%s state:%d %s %d\n", __func__,
			pd->state,
			pd_state_to_str(pd->state),
			again);
		again = false;

		switch (pd->state) {
		case PD_HW_UNINIT:
		case PD_HW_FAIL:
			ret_value = ALG_INIT_FAIL;
			break;
		case PD_HW_READY:
			ret_value = pd_hal_is_pd_adapter_ready(alg);
			if (ret_value == ALG_TA_NOT_SUPPORT)
				pd->state = PD_TA_NOT_SUPPORT;
			else if (ret_value == ALG_READY)
				pd->state = PD_RUN;
				again = true;
			break;
		case PD_TA_NOT_SUPPORT:
			ret_value = ALG_TA_NOT_SUPPORT;
			break;
		case PD_RUN:
		case PD_STOP:
			ret_value = __pd_run(alg);
			break;
		default:
			pd_err("PD unknown state:%d\n", pd->state);
			ret_value = ALG_INIT_FAIL;
			break;
		}
	} while (again == true);

	mutex_unlock(&pd->access_lock);

	return ret_value;
}


static bool _pd_is_algo_running(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;

	pd_dbg("%s\n", __func__);
	pd = dev_get_drvdata(&alg->dev);

	if (pd->state == PD_RUN)
		return true;

	return false;
}

static int _pd_stop_algo(struct chg_alg_device *alg)
{
	pd_dbg("%s\n", __func__);

	return 0;
}

static int _pd_notifier_call(struct chg_alg_device *alg,
			 struct chg_alg_notify *notify)
{
	struct mtk_pd *pd;
	int ret = 0;

	pd = dev_get_drvdata(&alg->dev);
	pd_err("%s evt:%d\n", __func__, notify->evt);

	switch (notify->evt) {
	case EVT_PLUG_OUT:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void mtk_pd_parse_dt(struct mtk_pd *pd,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		pd->min_charger_voltage = val;
	else {
		pd_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		pd->min_charger_voltage = V_CHARGER_MIN;
	}

	/* PD */
	if (of_property_read_u32(np, "pd_vbus_upper_bound", &val) >= 0) {
		pd->vbus_h = val / 1000;
	} else {
		pd_err("use default pd_vbus_upper_bound:%d\n",
			PD_VBUS_UPPER_BOUND);
		pd->vbus_h = PD_VBUS_UPPER_BOUND / 1000;
	}

	if (of_property_read_u32(np, "pd_vbus_low_bound", &val) >= 0) {
		pd->vbus_l = val / 1000;
	} else {
		pd_err("use default pd_vbus_low_bound:%d\n",
			PD_VBUS_LOW_BOUND);
		pd->vbus_l = PD_VBUS_LOW_BOUND / 1000;
	}

	if (of_property_read_u32(np, "vsys_watt", &val) >= 0) {
		pd->vsys_watt = val;
	} else {
		pd_err("use default vsys_watt:%d\n",
			VSYS_WATT);
		pd->vsys_watt = VSYS_WATT;
	}

	if (of_property_read_u32(np, "ibus_err", &val) >= 0) {
		pd->ibus_err = val;
	} else {
		pd_err("use default ibus_err:%d\n",
			IBUS_ERR);
		pd->ibus_err = IBUS_ERR;
	}

	/* dual charger */
	if (of_property_read_u32(np, "slave_mivr_diff", &val) >= 0)
		pd->slave_mivr_diff = val;
	else {
		pd_err("use default SLAVE_MIVR_DIFF:%d\n", SLAVE_MIVR_DIFF);
		pd->slave_mivr_diff = SLAVE_MIVR_DIFF;
	}


}

int _pd_get_status(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value)
{

	pr_notice("%s\n", __func__);
	if (s == ALG_MAX_VBUS)
		*value = 10000;
	else
		pr_notice("%s does not support prop:%d\n", __func__, s);
	return 0;
}

int _pd_set_setting(struct chg_alg_device *alg_dev,
	struct chg_alg_setting *setting)
{
	struct mtk_pd *pd;

	pd_dbg("%s cv:%d icl:%d cc:%d\n",
		__func__,
		setting->cv,
		setting->input_current_limit,
		setting->charging_current_limit);
	pd = dev_get_drvdata(&alg_dev->dev);

	mutex_lock(&pd->access_lock);
	pd->cv = setting->cv;
	pd->input_current_limit = setting->input_current_limit;
	pd->charging_current_limit = setting->charging_current_limit;
	mutex_unlock(&pd->access_lock);

	return 0;
}

static struct chg_alg_ops pd_alg_ops = {
	.init_algo = _pd_init_algo,
	.is_algo_ready = _pd_is_algo_ready,
	.start_algo = _pd_start_algo,
	.is_algo_running = _pd_is_algo_running,
	.stop_algo = _pd_stop_algo,
	.notifier_call = _pd_notifier_call,
	.get_status = _pd_get_status,
	.set_setting = _pd_set_setting,
};

static int mtk_pd_probe(struct platform_device *pdev)
{
	struct mtk_pd *pd = NULL;

	pr_notice("%s: starts\n", __func__);

	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;
	platform_set_drvdata(pdev, pd);
	pd->pdev = pdev;
	mutex_init(&pd->access_lock);
	mtk_pd_parse_dt(pd, &pdev->dev);

	pd->alg = chg_alg_device_register("pd", &pdev->dev,
					pd, &pd_alg_ops, NULL);

	return 0;
}

static int mtk_pd_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_pd_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id mtk_pd_of_match[] = {
	{.compatible = "mediatek,charger,pd",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pd_of_match);

struct platform_device pd_device = {
	.name = "pd",
	.id = -1,
};

static struct platform_driver pd_driver = {
	.probe = mtk_pd_probe,
	.remove = mtk_pd_remove,
	.shutdown = mtk_pd_shutdown,
	.driver = {
		   .name = "pd",
		   .of_match_table = mtk_pd_of_match,
	},
};

static int __init mtk_pd_init(void)
{
	return platform_driver_register(&pd_driver);
}
late_initcall(mtk_pd_init);

static void __exit mtk_pd_exit(void)
{
	platform_driver_unregister(&pd_driver);
}
module_exit(mtk_pd_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK PD algorithm Driver");
MODULE_LICENSE("GPL");

