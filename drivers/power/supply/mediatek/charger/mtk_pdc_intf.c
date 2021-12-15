/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include "mtk_charger_intf.h"

#define PD_VBUS_IR_DROP_THRESHOLD 1200

void mtk_pdc_plugout(struct charger_manager *info)
{
	info->pdc.check_impedance = true;
	info->pdc.pd_cap_max_watt = -1;
	info->pdc.pd_idx = -1;
	info->pdc.pd_reset_idx = -1;
	info->pdc.pd_boost_idx = 0;
	info->pdc.pd_buck_idx = 0;
}

int mtk_pdc_set_mivr(struct charger_manager *info, int uV)
{
	int ret = 0;
	bool chg2_chip_enabled = false;

	ret = charger_dev_set_mivr(info->chg1_dev, uV);
	if (ret < 0)
		chr_err("%s: failed, ret = %d\n", __func__, ret);

	if (info->chg2_dev) {
		charger_dev_is_chip_enabled(info->chg2_dev,
			&chg2_chip_enabled);
		if (chg2_chip_enabled) {
			ret = charger_dev_set_mivr(info->chg2_dev,
				uV + info->data.slave_mivr_diff);
			if (ret < 0)
				pr_info("%s: chg2 failed, ret = %d\n", __func__,
					ret);
		}
	}

	return ret;
}

void mtk_pdc_check_cable_impedance(struct charger_manager *pinfo)
{
	int ret = 0;
	int vchr1, vchr2, cable_imp;
	unsigned int aicr_value;
	bool mivr_state = false;
	struct timespec ptime[2], diff;

	if (pinfo->pdc.check_impedance == false)
		return;

	pinfo->pdc.check_impedance = false;
	pr_debug("%s: starts\n", __func__);

	get_monotonic_boottime(&ptime[0]);

	/* Set ichg = 2500mA, set MIVR */
	charger_dev_set_charging_current(pinfo->chg1_dev, 2500000);
	mdelay(240);
	ret = mtk_pdc_set_mivr(pinfo, pinfo->data.min_charger_voltage);
	if (ret < 0)
		chr_err("%s: failed, ret = %d\n", __func__, ret);

	get_monotonic_boottime(&ptime[1]);
	diff = timespec_sub(ptime[1], ptime[0]);

	aicr_value = 800000;
	charger_dev_set_input_current(pinfo->chg1_dev, aicr_value);

	/* To wait for soft-start */
	msleep(150);

	ret = charger_dev_get_mivr_state(pinfo->chg1_dev, &mivr_state);
	if (ret != -ENOTSUPP && mivr_state) {
		pr_debug("%s: fail ret:%d mivr_state:%d\n", __func__,
			ret, mivr_state);
		goto end;
	}

	vchr1 = battery_get_vbus() * 1000;

	aicr_value = 500000;
	charger_dev_set_input_current(pinfo->chg1_dev, aicr_value);
	msleep(20);

	vchr2 = battery_get_vbus() * 1000;

	/*
	 * Calculate cable impedance (|V1 - V2|) / (|I2 - I1|)
	 * m_ohm = (mv * 10 * 1000) / (mA * 10)
	 * m_ohm = (uV * 10) / (mA * 10)
	 */
	cable_imp = (abs(vchr1 - vchr2) * 10) / (7400 - 4625);

	chr_err("%s: cable_imp:%d mohm, vchr1:%d, vchr2:%d, time:%ld\n",
		 __func__, cable_imp, vchr1 / 1000, vchr2 / 1000, diff.tv_nsec);

	chr_err("cable_imp:%d threshold:%d s:%d\n", cable_imp,
		pinfo->data.cable_imp_threshold, mivr_state);

	return;

end:
	chr_err("%s fail\n",
		__func__);
}


static bool mtk_is_pdc_ready(struct charger_manager *info)
{
	if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK ||
		info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30)
		return true;

	if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO &&
		info->enable_pe_4 == false &&
		info->enable_pe_5 == false)
		return true;

	return false;
}

bool mtk_pdc_check_charger(struct charger_manager *info)
{
	if (mtk_is_pdc_ready(info) == false)
		return false;

	return true;
}

void mtk_pdc_plugout_reset(struct charger_manager *info)
{
	info->pdc.cap.nr = 0;
}

void mtk_pdc_set_max_watt(struct charger_manager *info, int watt)
{
	info->pdc.vbus_h = 10000;
	info->pdc.pdc_max_watt_setting = watt;
}

int mtk_pdc_get_max_watt(struct charger_manager *info)
{
	int charging_current = info->data.pd_charger_current / 1000;
	int vbat = pmic_get_battery_voltage();

	if (info->pdc.pdc_max_watt_setting != -1)
		info->pdc.pdc_max_watt = info->pdc.pdc_max_watt_setting;
	else {
		if (info->chg1_data.thermal_charging_current_limit != -1)
			charging_current =
			info->chg1_data.thermal_charging_current_limit / 1000;

		info->pdc.pdc_max_watt = vbat * charging_current;
	}
	chr_err("[%s]watt:%d:%d vbat:%d c:%d=>\n", __func__,
		info->pdc.pdc_max_watt_setting,
		info->pdc.pdc_max_watt, vbat, charging_current);

	return info->pdc.pdc_max_watt;
}

int mtk_pdc_get_idx(struct charger_manager *info, int selected_idx,
	int *boost_idx, int *buck_idx)
{
	struct mtk_pdc *pd = &info->pdc;
	struct adapter_power_cap *cap;
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

int mtk_pdc_setup(struct charger_manager *info, int idx)
{
	int ret = -100;
	unsigned int mivr;
	unsigned int oldmivr = 4600000;
	unsigned int oldmA = 3000000;
	bool chg2_chip_enabled = false;
	bool force_update = false;

	struct mtk_pdc *pd = &info->pdc;

	if (pd->pd_idx == idx) {
		charger_dev_get_mivr(info->chg1_dev, &oldmivr);

		if (pd->cap.max_mv[idx] - oldmivr / 1000 >
			PD_VBUS_IR_DROP_THRESHOLD)
			force_update = true;

		if (info->chg2_dev) {
			charger_dev_is_chip_enabled(info->chg2_dev,
				&chg2_chip_enabled);
			if (chg2_chip_enabled) {
				charger_dev_get_mivr(info->chg2_dev, &oldmivr);

				if (pd->cap.max_mv[idx] - oldmivr / 1000  >
					PD_VBUS_IR_DROP_THRESHOLD -
					info->data.slave_mivr_diff / 1000)
					force_update = true;
			}
		}
	}

	if (pd->pd_idx != idx || force_update) {
		if (pd->cap.max_mv[idx] > 5000)
			charger_enable_vbus_ovp(info, false);
		else
			charger_enable_vbus_ovp(info, true);

		charger_dev_get_mivr(info->chg1_dev, &oldmivr);
		mivr = info->data.min_charger_voltage / 1000;
		mtk_pdc_set_mivr(info, info->data.min_charger_voltage);

		charger_dev_get_input_current(info->chg1_dev, &oldmA);
		oldmA = oldmA / 1000;

		if (info->data.parallel_vbus && (oldmA * 2 > pd->cap.ma[idx])) {
			charger_dev_set_input_current(info->chg1_dev,
					pd->cap.ma[idx] * 1000 / 2);
			charger_dev_set_input_current(info->chg2_dev,
					pd->cap.ma[idx] * 1000 / 2);
		} else if (info->data.parallel_vbus == false &&
			(oldmA > pd->cap.ma[idx]))
			charger_dev_set_input_current(info->chg1_dev,
						pd->cap.ma[idx] * 1000);

		ret = adapter_dev_set_cap(info->pd_adapter, MTK_PD,
			pd->cap.max_mv[idx], pd->cap.ma[idx]);

		if (ret == MTK_ADAPTER_OK) {
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

			if ((pd->cap.max_mv[idx] - PD_VBUS_IR_DROP_THRESHOLD)
				> mivr)
				mivr = pd->cap.max_mv[idx] -
					PD_VBUS_IR_DROP_THRESHOLD;

			mtk_pdc_set_mivr(info, mivr * 1000);
		} else {
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

			mtk_pdc_set_mivr(info, oldmivr);
		}

		mtk_pdc_get_idx(info, idx, &pd->pd_boost_idx, &pd->pd_buck_idx);
	}

	chr_err("[%s]idx:%d:%d:%d:%d vbus:%d cur:%d ret:%d\n", __func__,
		pd->pd_idx, idx, pd->pd_boost_idx, pd->pd_buck_idx,
		pd->cap.max_mv[idx], pd->cap.ma[idx], ret);

	pd->pd_idx = idx;

	return ret;
}

void mtk_pdc_get_cap_max_watt(struct charger_manager *info)
{
	struct mtk_pdc *pd = &info->pdc;
	struct adapter_power_cap *cap;
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

void mtk_pdc_get_reset_idx(struct charger_manager *info)
{
	struct mtk_pdc *pd = &info->pdc;
	struct adapter_power_cap *cap;
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

void mtk_pdc_reset(struct charger_manager *info)
{
	struct mtk_pdc *pd = &info->pdc;

	chr_err("%s: reset to default profile\n", __func__);
	mtk_pdc_init_table(info);
	mtk_pdc_get_reset_idx(info);
	mtk_pdc_setup(info, pd->pd_reset_idx);
}

int mtk_pdc_get_setting(struct charger_manager *info, int *newvbus, int *newcur,
			int *newidx)
{
	int ret = 0;
	int idx, selected_idx;
	unsigned int pd_max_watt, pd_min_watt, now_max_watt;
	struct mtk_pdc *pd = &info->pdc;
	int ibus = 0, vbus;
	int ibat = 0, chg1_ibat = 0, chg2_ibat = 0;
	int chg2_watt = 0;
	bool boost = false, buck = false;
	struct adapter_power_cap *cap = NULL;
	unsigned int mivr1 = 0;
	unsigned int mivr2 = 0;
	bool chg1_mivr = false;
	bool chg2_mivr = false;
	bool chg2_enable = false;

	mtk_pdc_init_table(info);
	mtk_pdc_get_reset_idx(info);
	mtk_pdc_get_cap_max_watt(info);

	cap = &pd->cap;

	if (cap->nr == 0)
		return -1;

	if (info->enable_hv_charging == false)
		goto reset;

	ret = charger_dev_get_ibus(info->chg1_dev, &ibus);
	if (ret < 0) {
		chr_err("[%s] get ibus fail, keep default voltage\n", __func__);
		return -1;
	}

	if (info->data.parallel_vbus) {
		ret = charger_dev_get_ibat(info->chg1_dev, &chg1_ibat);
		if (ret < 0)
			chr_err("[%s] get ibat fail\n", __func__);

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

		chr_err("[%s] chg2_watt:%d ibat2:%d ibat1:%d ibat:%d\n",
			__func__, chg2_watt, chg2_ibat, chg1_ibat, ibat * 100);
	}

	charger_dev_get_mivr_state(info->chg1_dev, &chg1_mivr);
	charger_dev_get_mivr(info->chg1_dev, &mivr1);

	if (is_dual_charger_supported(info)) {
		charger_dev_is_enabled(info->chg2_dev, &chg2_enable);
		if (chg2_enable) {
			charger_dev_get_mivr_state(info->chg2_dev, &chg2_mivr);
			charger_dev_get_mivr(info->chg2_dev, &mivr2);
		}
	}

	vbus = battery_get_vbus();
	ibus = ibus / 1000;

	if ((chg1_mivr && (vbus < mivr1 / 1000 - 500)) ||
	    (chg2_mivr && (vbus < mivr2 / 1000 - 500)))
		goto reset;

	selected_idx = cap->selected_cap_idx;
	idx = selected_idx;

	if (idx < 0 || idx >= ADAPTER_CAP_MAX_NR)
		idx = selected_idx = 0;

	pd_max_watt = cap->max_mv[idx] * (cap->ma[idx]
			/ 100 * (100 - info->data.ibus_err) - 100);
	now_max_watt = cap->max_mv[idx] * ibus + chg2_watt;
	pd_min_watt = cap->max_mv[pd->pd_buck_idx] * cap->ma[pd->pd_buck_idx]
			/ 100 * (100 - info->data.ibus_err)
			- info->data.vsys_watt;

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

	chr_err("[%s]watt:%d,%d,%d up:%d,%d vbus:%d ibus:%d, mivr:%d,%d\n",
		__func__,
		pd_max_watt, now_max_watt, pd_min_watt,
		boost, buck,
		vbus, ibus, chg1_mivr, chg2_mivr);

	chr_err("[%s]vbus:%d:%d:%d current:%d idx:%d default_idx:%d\n",
		__func__, pd->vbus_h, pd->vbus_l, *newvbus,
		*newcur, *newidx, selected_idx);

	return 0;

reset:
	mtk_pdc_reset(info);
	*newidx = pd->pd_reset_idx;
	*newvbus = cap->max_mv[*newidx];
	*newcur = cap->ma[*newidx];

	return 0;
}

void mtk_pdc_init_table(struct charger_manager *info)
{
	struct mtk_pdc *pd = &info->pdc;

	pd->cap.nr = 0;
	pd->cap.selected_cap_idx = -1;

	if (mtk_is_pdc_ready(info))
		adapter_dev_get_cap(info->pd_adapter, MTK_PD, &pd->cap);
	else
		chr_err("mtk_is_pdc_ready is fail\n");

	chr_err("[%s] nr:%d default:%d\n", __func__, pd->cap.nr,
	pd->cap.selected_cap_idx);
}

bool mtk_pdc_init(struct charger_manager *info)
{
	info->pdc.pdc_max_watt_setting = -1;

	info->pdc.check_impedance = true;
	info->pdc.pd_cap_max_watt = -1;
	info->pdc.pd_idx = -1;
	info->pdc.pd_reset_idx = -1;
	info->pdc.pd_boost_idx = 0;
	info->pdc.pd_buck_idx = 0;
	info->pdc.vbus_l = info->data.pd_vbus_low_bound / 1000;
	info->pdc.vbus_h = info->data.pd_vbus_upper_bound / 1000;

	return true;
}

