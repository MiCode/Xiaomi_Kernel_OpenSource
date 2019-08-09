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
#include <linux/delay.h>
#include "mtk_charger_intf.h"

static bool check_impedance = true;
static int pd_idx = -1;
static int vbus_l = PD_VBUS_LOW_BOUND / 1000;
static int vbus_h = PD_VBUS_UPPER_BOUND / 1000;


void mtk_pdc_plugout(struct charger_manager *info)
{
	check_impedance = true;
	pd_idx = -1;
}

void mtk_pdc_check_cable_impedance(struct charger_manager *pinfo)
{
	int ret = 0;
	int vchr1, vchr2, cable_imp;
	unsigned int aicr_value;
	bool mivr_state = false;
	struct timespec ptime[2], diff;

	if (check_impedance == false)
		return;

	check_impedance = false;
	pr_debug("%s: starts\n", __func__);

	get_monotonic_boottime(&ptime[0]);

	/* Set ichg = 2500mA, set MIVR */
	charger_dev_set_charging_current(pinfo->chg1_dev, 2500000);
	mdelay(240);
	ret = charger_dev_set_mivr(pinfo->chg1_dev,
				pinfo->data.min_charger_voltage);
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
	if (info->tcpc == NULL)
		return false;

	if (info->pd_type == PD_CONNECT_PE_READY_SNK ||
		info->pd_type == PD_CONNECT_PE_READY_SNK_PD30 ||
		info->pd_type == PD_CONNECT_PE_READY_SNK_APDO)
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
	vbus_h = 10000;
	info->pdc.pdc_max_watt_setting = watt;
}

int mtk_pdc_get_max_watt(struct charger_manager *info)
{
	int charging_current = info->data.pd_charger_current / 1000;
	int vbat = pmic_get_battery_voltage();

	if (info->tcpc == NULL)
		return 0;

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

int mtk_pdc_setup(struct charger_manager *info, int idx)
{
	int ret = -100;

	struct mtk_pdc *pd = &info->pdc;

	if (info->tcpc == NULL)
		return -1;

	if (pd_idx != idx)
	ret = tcpm_dpm_pd_request(info->tcpc, pd->cap.max_mv[idx],
					pd->cap.ma[idx], NULL);

	chr_err("[%s]idx:%d:%d vbus:%d cur:%d ret:%d\n", __func__,
		pd_idx, idx, pd->cap.max_mv[idx], pd->cap.ma[idx], ret);
	pd_idx = idx;

	return ret;
}

int mtk_pdc_get_setting(struct charger_manager *info, int *vbus, int *cur,
			int *idx)
{
	int i = 0;
	unsigned int max_watt;
	struct mtk_pdc *pd = &info->pdc;
	int min_vbus_idx = -1;

	if (info->tcpc == NULL)
		return -1;

	mtk_pdc_init_table(info);

	if (pd->cap.nr == 0)
		return -1;

	if (info->enable_hv_charging == false)
		vbus_h = 5000;

	max_watt = mtk_pdc_get_max_watt(info);
	*idx = -1;

	for (i = 0; i < pd->cap.nr; i++) {

		if (pd->cap.min_mv[i] < vbus_l || pd->cap.max_mv[i] < vbus_l)
			continue;

		if (pd->cap.min_mv[i] > vbus_h || pd->cap.max_mv[i] > vbus_h)
			continue;

		if (min_vbus_idx == -1) {
			*vbus = pd->cap.max_mv[i];
			*cur = pd->cap.ma[i];
			*idx = i;
			min_vbus_idx = i;
			chr_err("[%s]0:%d:watt:%d vbus:%d current:%d idx:%d default_idx:%d\n",
				__func__, i, info->pdc.pdc_max_watt, *vbus,
				*cur, *idx, pd->cap.selected_cap_idx);
			continue;
		}

		if (pd->cap.maxwatt[min_vbus_idx] < max_watt) {
			if (pd->cap.maxwatt[i] >
			    pd->cap.maxwatt[min_vbus_idx]) {
				*vbus = pd->cap.max_mv[i];
				*cur = pd->cap.ma[i];
				*idx = i;
				min_vbus_idx = i;
			}
			chr_err("[%s]1:%d:watt:%d vbus:%d current:%d idx:%d default_idx:%d\n",
				__func__, i, info->pdc.pdc_max_watt, *vbus,
				*cur, *idx, pd->cap.selected_cap_idx);

		} else {
			if (pd->cap.min_mv[i] < pd->cap.min_mv[min_vbus_idx] &&
				pd->cap.maxwatt[i] >= max_watt) {
				*vbus = pd->cap.max_mv[i];
				*cur = pd->cap.ma[i];
				*idx = i;
				min_vbus_idx = i;
			}
			chr_err("[%s]2:%d:watt:%d vbus:%d current:%d idx:%d default_idx:%d\n",
				__func__, i, info->pdc.pdc_max_watt, *vbus,
				*cur, *idx, pd->cap.selected_cap_idx);
		}

	}

	chr_err("[%s]watt:%d vbus:%d:%d:%d current:%d idx:%d default_idx:%d\n",
		__func__, info->pdc.pdc_max_watt, *vbus, vbus_h, vbus_l,
		*cur, *idx, pd->cap.selected_cap_idx);

	return 0;
}

void mtk_pdc_init_table(struct charger_manager *info)
{
	struct tcpm_remote_power_cap cap;
	int i;
	struct mtk_pdc *pd = &info->pdc;

	if (info->tcpc == NULL)
		return;
	cap.nr = 0;
	cap.selected_cap_idx = -1;
	if (mtk_is_pdc_ready(info)) {
		tcpm_get_remote_power_cap(info->tcpc, &cap);

		if (cap.nr != 0) {
			pd->cap.nr = cap.nr;
			pd->cap.selected_cap_idx = cap.selected_cap_idx;
			for (i = 0; i < cap.nr; i++) {
				pd->cap.ma[i] = cap.ma[i];
				pd->cap.max_mv[i] = cap.max_mv[i];
				pd->cap.min_mv[i] = cap.min_mv[i];
				if (cap.max_mv[i] != cap.min_mv[i]) {
					pd->cap.maxwatt[i] = pd->cap.ma[i] *
							     pd->cap.min_mv[i];
					pd->cap.minwatt[i] = 100 *
							     pd->cap.min_mv[i];
				} else {
					pd->cap.maxwatt[i] = pd->cap.ma[i] *
							    pd->cap.max_mv[i];
					pd->cap.minwatt[i] = 100 *
							    pd->cap.max_mv[i];
				}
				chr_err("[%s]:%d mv:[%d,%d] %d max:%d min:%d\n",
					__func__, i, pd->cap.min_mv[i],
					pd->cap.max_mv[i], pd->cap.ma[i],
					pd->cap.maxwatt[i], pd->cap.minwatt[i]);
			}
		}
	} else
		chr_err("mtk_is_pdc_ready is fail\n");

#ifdef PDC_TEST
	for (i = 0; i < 35000000; i = i + 2000000) {
		int vbus, cur, idx;

		mtk_pdc_set_max_watt(i);
		mtk_pdc_get_setting(&vbus, &cur, &idx);
		chr_err("[%s]watt:%d,vbus:%d,cur:%d,idx:%d\n",
			__func__, pdc_max_watt, vbus, cur, idx);
	}
#endif

	chr_err("[%s] nr:%d default:%d\n", __func__, cap.nr,
	cap.selected_cap_idx);
}

bool mtk_pdc_init(struct charger_manager *info)
{
	info->pdc.pdc_max_watt_setting = -1;
	return true;
}

