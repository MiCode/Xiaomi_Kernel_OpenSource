/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <linux/time.h>
#include <linux/slab.h>
#include "mtk_intf.h"

#define PE40_VBUS_STEP 50
#define PE40_MIN_WATT 5000000
#define PE40_VBUS_IR_DROP_THRESHOLD 1200

static struct pe40 *pe4;

int pe40_set_mivr(int uV)
{
	int ret = 0;

	ret = charger_set_mivr(uV);
	if (ret < 0)
		chr_err("%s: failed, ret = %d\n", __func__, ret);

	return ret;
}

int pe40_pd_1st_request(int adapter_mv, int adapter_ma, int ma)
{
	unsigned int oldmA = 3000000;
	int ret;
	int mivr;

	chr_err("pe40_pd_req:vbus:%d ibus:%d input_current:%d\n",
		adapter_mv, adapter_ma, ma);

	mivr = pe4->data.min_charger_voltage / 1000;
	pe40_set_mivr(pe4->data.min_charger_voltage);

	charger_get_input_current(&oldmA);
	oldmA = oldmA / 1000;
	if (oldmA > ma)
		charger_set_input_current(ma * 1000);

	ret = adapter_set_cap_start(adapter_mv, adapter_ma);

	if (oldmA < ma)
		charger_set_input_current(ma * 1000);

	if ((adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD;

	pe40_set_mivr(mivr * 1000);

	pe4->pe4_input_current_limit_setting = ma * 1000;
	return ret;
}

int pe40_pd_request(int *adapter_vbus, int *adapter_ibus, int ma)
{
	unsigned int oldmA = 3000000;
	unsigned int oldmivr = 4600;
	int ret;
	int mivr;
	int adapter_mv, adapter_ma;

	adapter_mv = *adapter_vbus;
	adapter_ma = *adapter_ibus;

	charger_get_mivr(&oldmivr);

	mivr = pe4->data.min_charger_voltage / 1000;
	pe40_set_mivr(pe4->data.min_charger_voltage);

	charger_get_input_current(&oldmA);
	oldmA = oldmA / 1000;
	if (oldmA > ma)
		charger_set_input_current(ma * 1000);

	ret = adapter_set_cap(adapter_mv, adapter_ma);

	chr_err("pe40_pd_req:vbus:%d ibus:%d input_current:%d ret:%d\n",
		adapter_mv, adapter_ma, ma, ret);

	if (ret == ADAPTER_REJECT) {
		chr_err("pe40_pd_req: reject\n");

		if (pe4->cap.pdp > 0 &&
			adapter_mv * adapter_ma > pe4->cap.pdp * 1000000) {
			*adapter_ibus = pe4->cap.pdp * 1000000
					/ adapter_mv;
			ret = adapter_set_cap(adapter_mv, *adapter_ibus);

			chr_err("pe40_pd_req:vbus:%d new_ibus:%d pdp:%d ret:%d\n",
				adapter_mv, *adapter_ibus,
				pe4->cap.pdp, ret);

			if (ret == ADAPTER_OK)
				ret = ADAPTER_ADJUST;

			if (ret == ADAPTER_REJECT)
				goto err;
		} else
			goto err;
	}

	if (oldmA < ma)
		charger_set_input_current(ma * 1000);

	if ((adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD;

	pe40_set_mivr(mivr * 1000);

	pe4->pe4_input_current_limit_setting = ma * 1000;
	return ret;

err:
	if (oldmA > ma)
		charger_set_input_current(oldmA * 1000);
	pe40_set_mivr(oldmivr);
	return ret;
}

int pe40_stop(void)
{
	if (pe4 == NULL)
		return -1;

	if (pe4->is_connect == true) {
		adapter_set_cap_end(5000, 2000);

		pe40_set_mivr(pe4->data.min_charger_voltage);
		pmic_enable_hw_vbus_ovp(true);
		enable_vbus_ovp(true);
		chr_err("set TD true\n");
		charger_enable_termination(true);

		pe4->state = PE40_INIT;
		pe4->is_connect = false;
		pe4->cap.nr = 0;
		pe4->pe4_input_current_limit = -1;
		pe4->pe4_input_current_limit_setting = -1;
		pe4->max_vbus = pe4->data.pe40_max_vbus;
		pe4->max_ibus = pe4->data.pe40_max_ibus;
		pe4->max_charger_ibus = pe4->data.pe40_max_ibus *
					(100 - pe4->data.ibus_err) / 100;
	}

	return 0;
}

int pe40_get_setting_by_watt(int *voltage,
	int *adapter_ibus, int *actual_current, int watt,
	int *ibus_current_setting)
{
	int i;
	struct pps_cap *pe40_cap;
	int vbus = 0, ibus = 0, ibus_setting = 0;
	int idx = 0, ta_ibus = 0;

	pe40_cap = &pe4->cap;
	for (i = 0; i < pe40_cap->nr; i++) {
		int max_ibus = 0;
		int max_vbus = 0;

		chr_err("%s: %d %d %d %d\n", __func__,
			pe40_cap->ma[i],
			pe4->max_ibus,
			pe40_cap->max_mv[i],
			pe40_cap->nr);

		/* update upper bound */
		if (pe40_cap->ma[i] > pe4->max_ibus)
			max_ibus = pe4->max_ibus;
		else
			max_ibus = pe40_cap->ma[i];

		if (max_ibus > pe4->data.input_current_limit / 1000)
			max_ibus = pe4->data.input_current_limit / 1000;

		if (pe4->pe4_input_current_limit != -1 &&
			max_ibus > (pe4->pe4_input_current_limit / 1000))
			max_ibus = pe4->pe4_input_current_limit / 1000;

		pe4->max_charger_ibus = max_ibus *
					(100 - pe4->data.ibus_err) / 100;

		chr_err("pe4: %d %d %d %d %d %d\n",
			pe40_cap->ma[i], pe4->max_ibus,
			pe4->data.input_current_limit / 1000,
			pe4->pe4_input_current_limit / 1000,
			max_ibus, pe4->max_charger_ibus);


		if (pe40_cap->max_mv[i] > pe4->max_vbus)
			max_vbus = pe4->max_vbus;
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
			5000 * pe4->max_charger_ibus >= watt) {
			vbus = 5000;
			ibus = watt / 5000;
			ibus_setting = max_ibus;
			ta_ibus = pe40_cap->ma[i];
			idx = 2;
			break;
		}

		/* is max watt ok */
		if (max_vbus * (pe4->max_charger_ibus - 200) >= watt &&
			!pe40_cap->pwr_limit[i]) {
			ibus = pe4->max_charger_ibus - 200;
			vbus = watt / ibus;
			ibus_setting = max_ibus;
			ta_ibus = pe40_cap->ma[i];
			if (vbus < pe40_cap->min_mv[i])
				vbus = pe40_cap->min_mv[i];

			idx = 3;
			break;
		}

		/* is power limit set */
		if (pe40_cap->pwr_limit[i] && pe40_cap->pdp > 0) {
			if (watt > pe40_cap->pdp * 1000000)
				watt = pe40_cap->pdp * 1000000;

			if (max_vbus * (pe4->max_charger_ibus - 200) >= watt) {
				ibus = pe4->max_charger_ibus - 200;
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
		vbus = max_vbus;
		ibus = pe4->max_charger_ibus;
		ibus_setting = max_ibus;
		ta_ibus = pe40_cap->ma[i];
		idx = 5;

	}

	*voltage = vbus;
	*ibus_current_setting = ibus_setting;
	*actual_current = ibus;
	*adapter_ibus = ta_ibus;

	chr_err("%s:[%d,%d]%d vbus:%d ibus:%d aicl:%d current:%d %d\n",
		__func__,
		idx, i,
		watt, *voltage,
		*adapter_ibus,
		*ibus_current_setting,
		ibus, pe4->max_charger_ibus);

	return idx;
}

bool pe40_is_ready(void)
{
	int tmp;

	tmp = battery_get_bat_temperature();

	if (!adapter_is_support_pd_pps() ||
		tmp > pe4->data.high_temp_to_enter_pe40 ||
		tmp < pe4->data.low_temp_to_enter_pe40)
		return false;

	return true;
}

int pe40_get_init_watt(void)
{
	int ret;
	int vbus1, ibus1;
	int vbus2, ibus2;
	int vbat1, vbat2;
	int voltage = 0, input_current = 1000, actual_current = 0;
	int voltage1 = 0, adapter_ibus;
	bool is_enable = false, is_chip_enable = false;
	int i;

	voltage = 0;
	pe40_get_setting_by_watt(&voltage, &adapter_ibus,
		&actual_current, 27000000, &input_current);
	ret = pe40_pd_request(&voltage, &adapter_ibus, input_current);

	if (ret != 0 && ret != ADAPTER_REJECT &&
			ret != ADAPTER_ADJUST) {
		chr_err("[pe40_i1] err:1 %d\n", ret);
		return -1;
	}

	for (i = 0; i < 3 ; i++) {
		charger_dump_registers();
		msleep(100);
	}

	charger_get_ibus(&ibus1);
	vbus1 = battery_get_vbus();
	ibus1 = ibus1 / 1000;
	vbat1 = battery_get_bat_voltage();
	voltage1 = voltage;

	voltage = 0;
	pe40_get_setting_by_watt(&voltage, &adapter_ibus,
		&actual_current, 15000000, &input_current);

	for (i = 0; i < 6 ; i++) {
		ret = pe40_pd_request(&voltage, &adapter_ibus,
			input_current);

		if (ret != 0 && ret != ADAPTER_ADJUST) {
			chr_err("[pe40_i1] err:2 %d\n", ret);
			return -1;
		}

		msleep(100);
		charger_get_ibus(&ibus2);
		vbus2 = battery_get_vbus();
		ibus2 = ibus2 / 1000;
		vbat2 = battery_get_bat_voltage();

		chr_err("[pe40_vbus] vbus1:%d ibus1:%d vbus2:%d ibus2:%d watt:%d en:%d %d vbat:%d %d\n",
			vbus1, ibus1, vbus2, ibus2, voltage1 * ibus1, is_enable,
			is_chip_enable, vbat1, vbat2);
	}

	return voltage1 * ibus1;
}

int pe40_init_state(void)
{
	int ret = 0;
	int vbus1, vbat1, ibus1;
	int vbus2, vbat2, ibus2;
	struct pps_status cap, cap1, cap2;
	int voltage, adapter_ibus = 1000, actual_current;
	int watt = 0;
	int i;
	int input_current = 0;

	chr_err("set TD false\n");
	charger_enable_termination(false);

	pmic_enable_hw_vbus_ovp(false);
	enable_vbus_ovp(false);

	adapter_get_pps_cap(&pe4->cap);
	pe4->max_vbus = pe4->data.pe40_max_vbus;
	pe4->is_connect = true;

	voltage = 0;
	pe40_get_setting_by_watt(&voltage, &adapter_ibus,
		&actual_current, 5000000, &input_current);

	ret = pe40_pd_1st_request(voltage, actual_current,
		actual_current);

	if (ret != 0) {
		chr_err("[pe40_i0] err:1 %d\n", ret);
		goto retry;
	}

	/* disable charger */
	charger_enable_powerpath(false);

	msleep(500);

	cap.output_ma = 0;
	cap.output_mv = 0;

	ret = adapter_get_output(&cap.output_mv, &cap.output_ma);

	pe4->can_query = true;
	if (ret == 0 && (cap.output_ma == -1 || cap.output_mv == -1))
		pe4->can_query = false;
	else if (ret == ADAPTER_NOT_SUPPORT)
		pe4->can_query = false;
	else if (ret != 0) {
		chr_err("[pe40_i0] err:2 %d\n", ret);
		goto err;
	}

	chr_err("[pe40_i0] can_query:%d ret:%d\n",
		pe4->can_query,
		ret);

	pe4->pmic_vbus = battery_get_vbus();
	pe4->TA_vbus = cap.output_mv;
	pe4->vbus_cali = pe4->TA_vbus - pe4->pmic_vbus;

	chr_err("[pe40_i0]pmic_vbus:%d TA_vbus:%d cali:%d ibus:%d\n",
		pe4->pmic_vbus, pe4->TA_vbus, pe4->vbus_cali,
		cap.output_ma);

	/*enable charger*/
	charger_enable_powerpath(true);

	msleep(100);

	if (cap.output_ma > 100) {
		chr_err("[pe40_i0] FOD fail :%d\n", cap.output_ma);
		goto err;
	}

	if (pe4->can_query == true) {
		/* measure 1 */
		voltage = 0;
		pe40_get_setting_by_watt(&voltage, &adapter_ibus,
			&actual_current, 5000000, &input_current);
		ret = pe40_pd_request(&voltage, &actual_current,
					actual_current);

		if (ret != 0 && ret != ADAPTER_ADJUST) {
			chr_err("[pe40_i0] err:3 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus1 = battery_get_vbus();
			vbat1 = battery_get_bat_voltage();
			charger_get_ibus(&ibus1);
			ibus1 = ibus1 / 1000;
			ret = adapter_get_output(&cap1.output_mv,
				&cap1.output_ma);

			if (ret != 0) {
				chr_err("[pe40_i0] err:4 %d\n", ret);
				goto err;
			}

			chr_err("[pe40_i11]vbus:%d ibus:%d vbat:%d TA_vbus:%d TA_ibus:%d setting:%d %d\n",
				vbus1, ibus1, vbat1,
				cap1.output_mv, cap1.output_ma,
				voltage, actual_current);

			if (abs(cap1.output_ma - actual_current) < 200)
				break;
		}


		/* measure 2 */
		voltage = 0;
		pe40_get_setting_by_watt(&voltage, &adapter_ibus,
			&actual_current, 7500000, &input_current);
		ret = pe40_pd_request(&voltage, &actual_current,
					actual_current);

		if (ret != 0 && ret != ADAPTER_ADJUST) {
			chr_err("[pe40_i0] err:5 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus2 = battery_get_vbus();
			vbat2 = battery_get_bat_voltage();
			charger_get_ibus(&ibus2);
			ibus2 = ibus2 / 1000;
			ret = adapter_get_output(&cap2.output_mv,
				&cap2.output_ma);

			if (ret != 0)
				goto err;

			chr_err("[pe40_i12]vbus:%d ibus:%d vbat:%d TA_vbus:%d TA_ibus:%d setting:%d %d\n",
				vbus2, ibus2, vbat2,
				cap2.output_mv, cap2.output_ma,
				voltage, actual_current);
			if (abs(cap2.output_ma - actual_current) < 200)
				break;
		}

		chr_err("[pe40_i1]vbus:%d,%d,%d,%d ibus:%d,%d,%d,%d vbat:%d,%d\n",
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

		if (pe4->r_cable_1 < pe4->data.pe40_r_cable_3a_lower)
			pe4->pe4_input_current_limit = 5000000;
		else if (pe4->r_cable_1 >= pe4->data.pe40_r_cable_3a_lower &&
			pe4->r_cable_1 < pe4->data.pe40_r_cable_2a_lower)
			pe4->pe4_input_current_limit = 3000000;
		else if (pe4->r_cable_1 >= pe4->data.pe40_r_cable_2a_lower &&
			pe4->r_cable_1 < pe4->data.pe40_r_cable_1a_lower)
			pe4->pe4_input_current_limit = 2000000;
		else if (pe4->r_cable_1 >= pe4->data.pe40_r_cable_1a_lower)
			pe4->pe4_input_current_limit = 1000000;

		chr_err("[pe40_i2]r_sw:%d r_cable:%d r_cable_1:%d r_cable_2:%d pe4_icl:%d\n",
			pe4->r_sw, pe4->r_cable, pe4->r_cable_1,
			pe4->r_cable_2, pe4->pe4_input_current_limit);
	} else
		chr_err("TA does not support query\n");

	watt = pe40_get_init_watt();
	voltage = 0;
	pe40_get_setting_by_watt(&voltage, &adapter_ibus,
				&actual_current, watt, &input_current);
	if (voltage <= 0)
		chr_err("abnormal voltage: %d\n", voltage);
	pe4->avbus = voltage / 10 * 10;
	ret = pe40_pd_request(&pe4->avbus, &adapter_ibus,
				input_current);

	if (ret != 0 && ret != ADAPTER_REJECT &&
			ret != ADAPTER_ADJUST) {
		chr_err("[pe40_i0] err:6 %d\n", ret);
		goto err;
	}

	pe4->avbus = voltage;
	if (voltage > 0)
		pe4->ibus = watt / voltage;
	else
		pe4->ibus = 0;
	pe4->watt = watt;
	pe4->state = PE40_CC;

	return 0;

retry:
	pe40_stop();
	return 1;
err:
	pe40_stop();
	return 2;
}

int pe40_safety_check(void)
{
	int vbus;
	struct pps_status cap;
	struct ta_status TAstatus;
	int ret;
	int tmp;
	int i;
	int high_tmp_cnt = 0;

	TAstatus.ocp = 0;
	TAstatus.otp = 0;
	TAstatus.ovp = 0;
	TAstatus.temperature = 0;

	/* vbus ov */
	vbus = battery_get_vbus();
	if (vbus - pe4->avbus >= 2000) {
		chr_err("[pe40_err]vbus ov :vbus:%d avbus:%d\n",
			vbus, pe4->avbus);
		goto err;
	}

	/* cable voltage drop check */
	if (pe4->can_query == true) {
		ret = adapter_get_output(&cap.output_mv, &cap.output_ma);
		if (ret != 0) {
			chr_err("[pe40_err] err:1 %d\n", ret);
			goto err;
		}

		if (cap.output_mv != -1 &&
			(cap.output_mv - vbus) > PE40_VBUS_IR_DROP_THRESHOLD) {
			chr_err("[pe40_err]vbus ov2 vbus:%d TAvbus:%d %d %d\n",
				vbus, cap.output_mv,
				PE40_VBUS_IR_DROP_THRESHOLD,
				(cap.output_mv - vbus) >
				PE40_VBUS_IR_DROP_THRESHOLD);
			goto err;
		}

		/* TA V_BUS OVP */
		if (cap.output_mv >= pe4->avbus * 12 / 10) {
			chr_err("[pe40_err]TA vbus ovp :vbus:%d avbus:%d\n",
				cap.output_mv, pe4->avbus);
			goto err;
		}
	}

	/* TA Thermal */
	for (i = 0; i < 3; i++) {
		ret = adapter_get_status(&TAstatus);
		if (TAstatus.temperature >= 100 &&
			TAstatus.temperature != 0 &&
			ret != ADAPTER_NOT_SUPPORT &&
			ret != ADAPTER_TIMEOUT) {
			high_tmp_cnt++;
			chr_err("[pe40]TA Thermal:%d cnt:%d\n",
				TAstatus.temperature, high_tmp_cnt);
		} else if (ret == ADAPTER_TIMEOUT) {
			chr_err("[pe40]TA adapter_dev_get_status timeout\n");
			goto err;
		} else
			break;

		if (high_tmp_cnt >= 3) {
			chr_err("[pe40_err]TA Thermal: %d thd:%d cnt:%d\n",
				TAstatus.temperature, 100, high_tmp_cnt);
			goto err;
		}
	}

	if (ret == ADAPTER_NOT_SUPPORT)
		chr_err("[pe40]TA adapter_dev_get_status not support\n");
	else {
		if (TAstatus.ocp ||
			TAstatus.otp ||
			TAstatus.ovp) {

			chr_err("[pe40_err]TA protect: ocp:%d otp:%d ovp:%d\n",
				TAstatus.ocp,
				TAstatus.otp,
				TAstatus.ovp);
			goto err;
		}

		chr_err("PD_TA:TA protect: ocp:%d otp:%d ovp:%d tmp:%d\n",
			TAstatus.ocp,
			TAstatus.otp,
			TAstatus.ovp,
			TAstatus.temperature);
	}

	tmp = battery_get_bat_temperature();

	if (tmp > pe4->data.high_temp_to_leave_pe40 ||
		tmp < pe4->data.low_temp_to_leave_pe40) {

		chr_err("[pe40_err]tmp:%d threshold:%d %d\n",
			tmp, pe4->data.high_temp_to_leave_pe40,
			pe4->data.low_temp_to_leave_pe40);
		return 1;
	}

	return 0;
err:
	return -1;
}

int pe40_cc_state(void)
{
	int ibus = 0, vbat, ibat, vbus;
	int icl, ccl, cv, max_icl;
	int ret;
	int oldavbus = 0;
	int watt;
	int max_watt;
	int actual_current;
	int new_watt = 0;
	int adapter_ibus = 0;
	int input_current = 0;
	int icl_threshold;
	bool mivr_loop = false;

	vbat = battery_get_bat_voltage();
	ibat = battery_get_bat_current_mA();

	charger_get_mivr_state(&mivr_loop);
	charger_get_ibus(&ibus);
	vbus = battery_get_vbus();
	ibus = ibus / 1000;
	icl = pe4->data.input_current_limit / 1000 *
		(100 - pe4->data.ibus_err) / 100;
	ccl = pe4->data.charging_current_limit / 1000;
	cv = pe4->data.battery_cv / 1000;
	watt = pe4->avbus * ibus;

	if (icl > pe4->max_charger_ibus)
		max_icl = pe4->max_charger_ibus;
	else
		max_icl = icl;

	icl_threshold = 100;

	max_watt = pe4->avbus * max_icl;

	chr_err("[pe40_cc]vbus:%d:%d,ibus:%d,ibat:%d icl:%d:%d,ccl:%d,vbat:%d,maxIbus:%d\n",
		pe4->avbus, vbus,
		ibus,
		ibat,
		icl, max_icl,
		ccl,
		vbat, pe4->max_charger_ibus);

	if ((mivr_loop && vbus <= 5000) ||
	    (ibus >= (max_icl - icl_threshold)) ||
	    (ibus <= (max_icl - icl_threshold * 2))) {

		oldavbus = pe4->avbus;

		if (mivr_loop && vbus <= 5000) {
			pe4->avbus = pe4->avbus + 50;
			new_watt = (pe4->avbus + 50) * icl;
		} else if (ibus >= (max_icl - icl_threshold)) {
			pe4->avbus = pe4->avbus + 50;
			new_watt = (pe4->avbus + 50) * ibus;
		} else if (ibus <= (max_icl - icl_threshold * 2)) {
			new_watt = pe4->avbus * pe4->ibus - 500000;
			pe4->avbus = pe4->avbus - 50;
		}

		ret = pe40_get_setting_by_watt(&pe4->avbus,
				&adapter_ibus, &actual_current, new_watt,
			&input_current);

		if (pe4->avbus <= 5000)
			pe4->avbus = 5000;

		if (abs(pe4->avbus - oldavbus) >= 50) {
			ret = pe40_pd_request(&pe4->avbus,
					&adapter_ibus, input_current);
			if (ret != 0 && ret != ADAPTER_REJECT &&
					ret != ADAPTER_ADJUST)
				goto err;
		}
		msleep(100);

		vbat = battery_get_bat_voltage();
		ibat = battery_get_bat_current_mA();
		charger_get_ibus(&ibus);
		vbus = battery_get_vbus();
		ibus = ibus / 1000;
		icl = pe4->data.input_current_limit / 1000;
		ccl = pe4->data.charging_current_limit / 1000;

		pe4->watt = pe4->avbus * ibus;
		pe4->vbus = vbus;
		pe4->ibus = ibus;
	}

	ret = pe40_safety_check();
	if (ret == 1)
		goto retry;

	if (ret == -1)
		goto err;

	if (pe4->avbus * ibus <= PE40_MIN_WATT)
		goto leave;

	return 0;

retry:
	pe40_stop();
	return 1;

leave:
err:
	pe40_stop();
	return 2;

}

int pe40_init(void)
{
	struct pe40 *pe40 = NULL;

	if (pe4 == NULL) {
		pe40 = kzalloc(sizeof(struct pe40), GFP_KERNEL);
		if (pe40 == NULL)
			return -ENOMEM;

		pe4 = pe40;
		pe4->state = PE40_INIT;
		pe4->is_connect = false;

		pe4->data.input_current_limit = 3000000;
		pe4->data.charging_current_limit = 3000000;
		pe4->data.battery_cv = 4350000;

		pe4->data.min_charger_voltage = 4600000;
		pe4->data.pe40_max_vbus = 11000;
		pe4->data.pe40_max_ibus = 3000;
		pe4->data.ibus_err = 14;
		pe4->data.high_temp_to_leave_pe40 = 46;
		pe4->data.high_temp_to_enter_pe40 = 39;
		pe4->data.low_temp_to_leave_pe40 = 10;
		pe4->data.low_temp_to_enter_pe40 = 16;
		pe4->data.pe40_r_cable_1a_lower = 576;
		pe4->data.pe40_r_cable_2a_lower = 435;
		pe4->data.pe40_r_cable_3a_lower = 293;

		pe4->pe4_input_current_limit = -1;
		pe4->pe4_input_current_limit_setting = -1;
		pe4->max_vbus = pe4->data.pe40_max_vbus;
		pe4->max_ibus = pe4->data.pe40_max_ibus;
		pe4->max_charger_ibus = pe4->data.pe40_max_ibus *
				(100 - pe4->data.ibus_err) / 100;

		chr_err("%s: done\n", __func__);

		return 0;
	}

	return 1;
}

struct pe40_data *pe40_get_data(void)
{
	return &pe4->data;
}

int pe40_set_data(struct pe40_data data)
{
	pe4->data.input_current_limit = data.input_current_limit;
	pe4->data.charging_current_limit = data.charging_current_limit;
	pe4->data.battery_cv = data.battery_cv;
	pe4->data.min_charger_voltage = data.min_charger_voltage;
	pe4->data.pe40_max_vbus = data.pe40_max_vbus;
	pe4->data.pe40_max_ibus = data.pe40_max_ibus;
	pe4->data.ibus_err = data.ibus_err;
	pe4->data.high_temp_to_enter_pe40 = data.high_temp_to_enter_pe40;
	pe4->data.low_temp_to_enter_pe40 = data.low_temp_to_enter_pe40;
	pe4->data.high_temp_to_leave_pe40 = data.high_temp_to_leave_pe40;
	pe4->data.low_temp_to_leave_pe40 = data.low_temp_to_leave_pe40;
	pe4->data.pe40_r_cable_3a_lower = data.pe40_r_cable_3a_lower;
	pe4->data.pe40_r_cable_2a_lower = data.pe40_r_cable_2a_lower;
	pe4->data.pe40_r_cable_1a_lower = data.pe40_r_cable_1a_lower;

	chr_err("[pe4_set_data]%d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		pe4->data.input_current_limit,
		pe4->data.charging_current_limit,
		pe4->data.battery_cv,
		pe4->data.min_charger_voltage,
		pe4->data.pe40_max_vbus,
		pe4->data.pe40_max_ibus,
		pe4->data.ibus_err,
		pe4->data.high_temp_to_enter_pe40,
		pe4->data.low_temp_to_enter_pe40,
		pe4->data.high_temp_to_leave_pe40,
		pe4->data.low_temp_to_leave_pe40,
		pe4->data.pe40_r_cable_3a_lower,
		pe4->data.pe40_r_cable_2a_lower,
		pe4->data.pe40_r_cable_1a_lower);

	return 0;
}

int pe40_set_current(void)
{
	if (pe4->pe4_input_current_limit != -1 &&
	    pe4->pe4_input_current_limit <
	    pe4->data.input_current_limit)
		pe4->data.input_current_limit =
			pe4->pe4_input_current_limit;

	if (pe4->pe4_input_current_limit_setting != -1 &&
	    pe4->pe4_input_current_limit_setting <
	    pe4->data.input_current_limit)
		pe4->data.input_current_limit =
			pe4->pe4_input_current_limit_setting;

	charger_set_input_current(pe4->data.input_current_limit);
	charger_set_charging_current(pe4->data.charging_current_limit);

	return 0;
}

int pe40_set_cv(void)
{
	charger_set_constant_voltage(pe4->data.battery_cv);

	return 0;
}

int pe40_run(void)
{
	int ret = 0;

	pe40_set_current();
	pe40_set_cv();

	switch (pe4->state) {
	case PE40_INIT:
		ret = pe40_init_state();
		break;

	case PE40_CC:
		ret = pe40_cc_state();
		break;
	}

	return ret;
}
