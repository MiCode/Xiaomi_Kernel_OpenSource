/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>
#include "mtk_intf.h"

#define PD_MIN_WATT 5000000
#define PD_VBUS_IR_DROP_THRESHOLD 1200

static struct pdc *pd;

bool pdc_is_ready(void)
{
	return adapter_is_support_pd();
}

void pdc_init_table(void)
{
	pd->cap.nr = 0;
	pd->cap.selected_cap_idx = -1;

	if (pdc_is_ready())
		adapter_get_cap(&pd->cap);
	else
		chr_err("mtk_is_pdc_ready is fail\n");

	chr_err("[%s] nr:%d default:%d\n", __func__, pd->cap.nr,
	pd->cap.selected_cap_idx);
}

void pdc_get_reset_idx(void)
{
	struct pd_cap *cap;
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
		chr_err("[%s]reset idx:%d vbus:%d %d\n", __func__,
			idx, cap->min_mv[idx], cap->max_mv[idx]);
	}
}

int pdc_set_mivr(int uV)
{
	int ret = 0;

	ret = charger_set_mivr(uV);
	if (ret < 0)
		chr_err("%s: failed, ret = %d\n", __func__, ret);

	return ret;
}


int pdc_get_idx(int selected_idx,
	int *boost_idx, int *buck_idx)
{
	struct pd_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;
	idx = selected_idx;

	if (idx < 0) {
		chr_err("[%s] invalid idx:%d\n", __func__, idx);
		*boost_idx = 0;
		*buck_idx = 0;
		return -1;
	}

	/* get boost_idx */
	for (i = 0; i < cap->nr; i++) {

		if (cap->min_mv[i] < pd->vbus_l ||
			cap->max_mv[i] < pd->vbus_l) {
			chr_err("min_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_l);
			continue;
		}

		if (cap->min_mv[i] > pd->vbus_h ||
			cap->max_mv[i] > pd->vbus_h) {
			chr_err("max_mv error:%d %d %d\n",
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
			chr_err("min_mv error:%d %d %d\n",
					cap->min_mv[i],
					cap->max_mv[i],
					pd->vbus_l);
			continue;
		}

		if (cap->min_mv[i] > pd->vbus_h ||
			cap->max_mv[i] > pd->vbus_h) {
			chr_err("max_mv error:%d %d %d\n",
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

int pdc_setup(int idx)
{
	int ret = -100;
	unsigned int mivr;
	unsigned int oldmivr = 4600000;
	unsigned int oldmA = 3000000;
	bool force_update = false;

	if (pd->pd_idx == idx) {
		charger_get_mivr(&oldmivr);

		if (pd->cap.max_mv[idx] - oldmivr / 1000 >
			PD_VBUS_IR_DROP_THRESHOLD)
			force_update = true;
	}

	if (pd->pd_idx != idx || force_update) {
		if (pd->cap.max_mv[idx] > 5000)
			enable_vbus_ovp(false);
		else
			enable_vbus_ovp(true);

		charger_get_mivr(&oldmivr);
		mivr = pd->data.min_charger_voltage / 1000;
		pdc_set_mivr(pd->data.min_charger_voltage);

		charger_get_input_current(&oldmA);
		oldmA = oldmA / 1000;

		if (oldmA > pd->cap.ma[idx])
			charger_set_input_current(pd->cap.ma[idx] * 1000);

		ret = adapter_set_cap(pd->cap.max_mv[idx], pd->cap.ma[idx]);

		if (ret == ADAPTER_OK) {
			if (oldmA < pd->cap.ma[idx])
				charger_set_input_current(pd->cap.ma[idx]
								* 1000);

			if ((pd->cap.max_mv[idx] - PD_VBUS_IR_DROP_THRESHOLD)
				> mivr)
				mivr = pd->cap.max_mv[idx] -
					PD_VBUS_IR_DROP_THRESHOLD;

			pdc_set_mivr(mivr * 1000);
		} else {
			if (oldmA > pd->cap.ma[idx])
				charger_set_input_current(oldmA * 1000);

			pdc_set_mivr(oldmivr);
		}

		pdc_get_idx(idx, &pd->pd_boost_idx, &pd->pd_buck_idx);
	}

	chr_err("[%s]idx:%d:%d:%d:%d vbus:%d cur:%d ret:%d\n", __func__,
		pd->pd_idx, idx, pd->pd_boost_idx, pd->pd_buck_idx,
		pd->cap.max_mv[idx], pd->cap.ma[idx], ret);

	pd->pd_idx = idx;

	return ret;
}

void pdc_get_cap_max_watt(void)
{
	struct pd_cap *cap;
	int i = 0;
	int idx = 0;

	cap = &pd->cap;

	if (pd->pd_cap_max_watt == -1) {
		for (i = 0; i < cap->nr; i++) {
			if (cap->min_mv[i] <= pd->vbus_h ||
				cap->max_mv[i] <= pd->vbus_h) {

				if (cap->maxwatt[i] > pd->pd_cap_max_watt) {
					pd->pd_cap_max_watt = cap->maxwatt[i];
					idx = i;
				}
				continue;
			}
		}
		chr_err("[%s]idx:%d vbus:%d %d maxwatt:%d\n", __func__,
			idx, cap->min_mv[idx], cap->max_mv[idx],
			pd->pd_cap_max_watt);
	}
}

int pdc_reset(void)
{
	if (pd == NULL || !pdc_is_ready())
		return -1;

	chr_err("%s: reset to default profile\n", __func__);
	pdc_init_table();
	pdc_get_reset_idx();
	pdc_setup(pd->pd_reset_idx);

	return 0;
}

int pdc_stop(void)
{
	pdc_reset();

	return 0;
}

int pdc_get_setting(int *newvbus, int *newcur,
			int *newidx)
{
	int ret = 0;
	int idx, selected_idx;
	unsigned int pd_max_watt, pd_min_watt, now_max_watt;
	int ibus = 0, vbus;
	bool boost = false, buck = false;
	struct pd_cap *cap = NULL;
	unsigned int mivr1 = 0;
	bool chg1_mivr = false;

	pdc_init_table();
	pdc_get_reset_idx();
	pdc_get_cap_max_watt();

	cap = &pd->cap;

	if (cap->nr == 0)
		return -1;

	ret = charger_get_ibus(&ibus);
	if (ret < 0) {
		chr_err("[%s] get ibus fail, keep default voltage\n", __func__);
		return -1;
	}

	charger_get_mivr_state(&chg1_mivr);
	charger_get_mivr(&mivr1);

	vbus = battery_get_vbus();
	ibus = ibus / 1000;

	if ((chg1_mivr && (vbus < mivr1 / 1000 - 500)))
		goto reset;

	selected_idx = cap->selected_cap_idx;
	idx = selected_idx;

	if (idx < 0 || idx >= ADAPTER_CAP_MAX_NR)
		idx = selected_idx = 0;

	pd_max_watt = cap->max_mv[idx] * (cap->ma[idx]
			/ 100 * (100 - pd->data.ibus_err) - 100);
	now_max_watt = cap->max_mv[idx] * ibus;
	pd_min_watt = cap->max_mv[pd->pd_buck_idx] * cap->ma[pd->pd_buck_idx]
			/ 100 * (100 - pd->data.ibus_err)
			- pd->data.vsys_watt;

	if (pd_min_watt <= 5000000)
		pd_min_watt = 5000000;

	if ((now_max_watt >= pd_max_watt) || chg1_mivr) {
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

	chr_err("[%s]watt:%d,%d,%d up:%d,%d vbus:%d ibus:%d, mivr:%d\n",
		__func__,
		pd_max_watt, now_max_watt, pd_min_watt,
		boost, buck,
		vbus, ibus, chg1_mivr);

	chr_err("[%s]vbus:%d:%d:%d current:%d idx:%d default_idx:%d\n",
		__func__, pd->vbus_h, pd->vbus_l, *newvbus,
		*newcur, *newidx, selected_idx);

	return 0;

reset:
	pdc_reset();
	*newidx = pd->pd_reset_idx;
	*newvbus = cap->max_mv[*newidx];
	*newcur = cap->ma[*newidx];

	return 0;
}

int pdc_check_leave(void)
{
	struct pd_cap *cap;
	int ibus = 0, vbus = 0;
	unsigned int mivr1 = 0;
	bool mivr_state = false;
	int max_mv = 0;

	cap = &pd->cap;
	max_mv = cap->max_mv[pd->pd_idx];

	charger_get_ibus(&ibus);
	ibus = ibus / 1000;
	vbus = battery_get_vbus();
	charger_get_mivr_state(&mivr_state);
	charger_get_mivr(&mivr1);

	chr_err("[%s]mv:%d vbus:%d ibus:%d idx:%d min_watt:%d mivr:%d mivr_state:%d\n",
		__func__, max_mv, vbus, ibus, pd->pd_idx,
		PD_MIN_WATT, mivr1 / 1000, mivr_state);

	if (max_mv * ibus <= PD_MIN_WATT) {
		if (mivr_state)
			chr_err("[%s] MIVR occurred, ibus can't draw much higher current",
				__func__);
		goto leave;
	}

	return 0;

leave:
	pdc_stop();
	return 2;
}

int pdc_init(void)
{
	struct pdc *pdc = NULL;

	if (pd == NULL) {
		pdc = kzalloc(sizeof(struct pdc), GFP_KERNEL);
		if (pdc == NULL)
			return -ENOMEM;

		pd = pdc;

		pd->data.input_current_limit = 3000000;
		pd->data.charging_current_limit = 3000000;
		pd->data.battery_cv = 4350000;

		pd->data.min_charger_voltage = 4600000;
		pd->data.pd_vbus_low_bound = 5000000;
		pd->data.pd_vbus_upper_bound = 5000000;
		pd->data.ibus_err = 14;
		pd->data.vsys_watt = 5000000;

		pd->pdc_input_current_limit_setting = -1;
		pd->pdc_max_watt_setting = -1;
		pd->pd_cap_max_watt = -1;
		pd->pd_idx = -1;
		pd->pd_reset_idx = -1;
		pd->pd_boost_idx = 0;
		pd->pd_buck_idx = 0;
		pd->vbus_l = 5000;
		pd->vbus_h = 5000;

		return 0;
	}

	return 1;
}

struct pdc_data *pdc_get_data(void)
{
	return &pd->data;
}

int pdc_set_data(struct pdc_data data)
{
	pd->data.input_current_limit = data.input_current_limit;
	pd->data.charging_current_limit = data.charging_current_limit;
	pd->data.battery_cv = data.battery_cv;
	pd->data.min_charger_voltage = data.min_charger_voltage;
	pd->data.pd_vbus_low_bound = data.pd_vbus_low_bound;
	pd->data.pd_vbus_upper_bound = data.pd_vbus_upper_bound;
	pd->data.ibus_err = data.ibus_err;
	pd->data.vsys_watt = data.vsys_watt;

	chr_err("[%s]%d %d %d %d %d %d %d %d\n", __func__,
		pd->data.input_current_limit,
		pd->data.charging_current_limit,
		pd->data.battery_cv,
		pd->data.min_charger_voltage,
		pd->data.pd_vbus_low_bound,
		pd->data.pd_vbus_upper_bound,
		pd->data.ibus_err,
		pd->data.vsys_watt);

	pd->vbus_l = pd->data.pd_vbus_low_bound / 1000;
	pd->vbus_h = pd->data.pd_vbus_upper_bound / 1000;

	return 0;
}

int pdc_set_current(void)
{
	if (pd->pdc_input_current_limit_setting != -1 &&
	    pd->pdc_input_current_limit_setting <
	    pd->data.input_current_limit)
		pd->data.input_current_limit =
			pd->pdc_input_current_limit_setting;

	charger_set_input_current(pd->data.input_current_limit);
	charger_set_charging_current(pd->data.charging_current_limit);

	return 0;
}

int pdc_set_cv(void)
{
	charger_set_constant_voltage(pd->data.battery_cv);

	return 0;
}

int pdc_run(void)
{
	int ret = 0;
	int vbus = 0, cur = 0, idx = 0;

	pd->vbus_l = pd->data.pd_vbus_low_bound / 1000;
	pd->vbus_h = pd->data.pd_vbus_upper_bound / 1000;

	pdc_set_cv();

	ret = pdc_get_setting(&vbus, &cur, &idx);

	if (ret != -1 && idx != -1) {
		pd->pdc_input_current_limit_setting =  cur * 1000;
		pdc_set_current();
		pdc_setup(idx);
	}

	ret = pdc_check_leave();

	chr_err("[%s]vbus:%d input_cur:%d idx:%d current:%d ret:%d\n",
			__func__, vbus, cur, idx,
			pd->data.input_current_limit, ret);

	return ret;
}
