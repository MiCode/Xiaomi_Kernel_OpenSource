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

#include <mt-plat/mtk_boot.h>
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_battery.h>
#include <upmu_common.h>
#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"
#include "mtk_switch_charging.h"

#define PE40_VBUS_STEP 50
#define PE40_MIN_WATT 5000000

#define IBUS_ERR 14

#define PE40_MAX_VBUS 11000
#define PE40_MAX_IBUS 3000

/* pe4.0 */
#ifndef HIGH_TEMP_TO_LEAVE_PE40
#define HIGH_TEMP_TO_LEAVE_PE40 50
#endif
#ifndef HIGH_TEMP_TO_ENTER_PE40
#define HIGH_TEMP_TO_ENTER_PE40 TEMP_T3_THRES_MINUS_X_DEGREE
#endif
#ifndef LOW_TEMP_TO_LEAVE_PE40
#define LOW_TEMP_TO_LEAVE_PE40 TEMP_T2_THRES
#endif
#ifndef LOW_TEMP_TO_ENTER_PE40
#define LOW_TEMP_TO_ENTER_PE40 TEMP_T2_THRES_PLUS_X_DEGREE
#endif
#ifndef PE40_VBUS_IR_DROP_THRESHOLD
#define PE40_VBUS_IR_DROP_THRESHOLD 1200
#endif


int mtk_pe40_pd_1st_request(struct charger_manager *pinfo,
	int adapter_mv, int adapter_ma, int ma,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	unsigned int oldmA;
	int ret;
	int mivr;

	chr_err("pe40_pd_req:vbus:%d ibus:%d input_current:%d\n",
		adapter_mv, adapter_ma, ma);

	mivr = pinfo->data.min_charger_voltage / 1000;
	charger_dev_set_mivr(pinfo->chg1_dev, pinfo->data.min_charger_voltage);

	charger_dev_get_input_current(pinfo->chg1_dev, &oldmA);
	oldmA = oldmA / 1000;
	if (oldmA > ma)
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);

	ret = tcpm_set_apdo_charging_policy(pinfo->tcpc,
			DPM_CHARGING_POLICY_PPS, adapter_mv, adapter_ma, NULL);

	if (oldmA < ma)
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);

	if ((adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD;

	charger_dev_set_mivr(pinfo->chg1_dev, mivr * 1000);

	pinfo->pe4.pe4_input_current_limit_setting = ma * 1000;
	return ret;
}

int mtk_pe40_pd_request(struct charger_manager *pinfo,
	int adapter_mv, int adapter_ma, int ma,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	unsigned int oldmA;
	unsigned int oldmivr;
	int ret;
	int mivr;

	charger_dev_get_mivr(pinfo->chg1_dev, &oldmivr);

	mivr = pinfo->data.min_charger_voltage / 1000;
	charger_dev_set_mivr(pinfo->chg1_dev, pinfo->data.min_charger_voltage);

	charger_dev_get_input_current(pinfo->chg1_dev, &oldmA);
	oldmA = oldmA / 1000;
	if (oldmA > ma)
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);
	ret = tcpm_dpm_pd_request(pinfo->tcpc, adapter_mv, adapter_ma, cb_data);

	chr_err("pe40_pd_req:vbus:%d ibus:%d input_current:%d ret:%d\n",
		adapter_mv, adapter_ma, ma, ret);

	if (ret == TCP_DPM_RET_REJECT) {
		chr_err("pe40_pd_req: reject\n");
		goto err;
	}

	if (oldmA < ma)
		charger_dev_set_input_current(pinfo->chg1_dev, ma * 1000);

	if ((adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD;

	charger_dev_set_mivr(pinfo->chg1_dev, mivr * 1000);

	pinfo->pe4.pe4_input_current_limit_setting = ma * 1000;
	return ret;

err:
	if (oldmA > ma)
		charger_dev_set_input_current(pinfo->chg1_dev, oldmA * 1000);
	charger_dev_set_mivr(pinfo->chg1_dev, oldmivr);
	return ret;
}

void mtk_pe40_reset(struct charger_manager *pinfo, bool enable)
{
	struct switch_charging_alg_data *swchgalg = pinfo->algorithm_data;
	struct mtk_pe40 *pe40;

	pe40 = &pinfo->pe4;

	if (pe40->is_connect == true) {
		tcpm_set_pd_charging_policy(pinfo->tcpc,
			DPM_CHARGING_POLICY_VSAFE5V, NULL);
		charger_dev_set_mivr(pinfo->chg1_dev,
					pinfo->data.min_charger_voltage);
		pmic_enable_hw_vbus_ovp(true);
		charger_enable_vbus_ovp(pinfo, true);
		pinfo->polling_interval = 10;
		swchgalg->state = CHR_CC;
		chr_err("set TD true\n");
		charger_dev_enable_termination(pinfo->chg1_dev, true);
	}

	pe40->cap.nr = 0;
	pe40->is_enabled = enable;
	pe40->is_connect = false;
	pe40->pe4_input_current_limit = -1;
	pe40->pe4_input_current_limit_setting = -1;
	pe40->max_vbus = PE40_MAX_VBUS;
	pe40->max_ibus = PE40_MAX_IBUS;
	pe40->max_charger_ibus = PE40_MAX_IBUS * (100 - IBUS_ERR) / 100;
}

void mtk_pe40_plugout_reset(struct charger_manager *pinfo)
{
	if (pinfo->enable_pe_4)
		mtk_pe40_reset(pinfo, true);
}

void mtk_pe40_end(struct charger_manager *pinfo, int type, bool retry)
{
	if (pinfo->enable_pe_4) {
		mtk_pe40_reset(pinfo, retry);
		chr_err("mtk_pe40_end:%d retry:%d\n", type, retry);
	}
}


bool mtk_is_TA_support_pd_pps(struct charger_manager *pinfo)
{
	if (pinfo->tcpc == NULL)
		return false;

	if (pinfo->enable_pe_4 == false)
		return false;

	if (pinfo->pd_type == PD_CONNECT_PE_READY_SNK_APDO)
		return true;
	return false;
}

void mtk_pe40_init_cap(struct charger_manager *info)
{
	struct tcpm_power_cap_val cap;
	uint8_t cap_i = 0;
	int ret;
	int idx = 0;
	int i;
	struct pe40_power_cap *pe40_cap;
	struct mtk_pe40 *pe40;

	if (info->tcpc == NULL)
		return;

	pe40 = &info->pe4;
	pe40->max_vbus = PE40_MAX_VBUS;

	pe40_cap = &info->pe4.cap;
	while (1) {
		ret = tcpm_inquire_pd_source_apdo(info->tcpc,
				TCPM_POWER_CAP_APDO_TYPE_PPS, &cap_i, &cap);
		if (ret == TCPM_ERROR_NOT_FOUND) {
			break;
		} else if (ret != TCPM_SUCCESS) {
			chr_err("[%s] tcpm_inquire_pd_source_apdo failed(%d)\n",
				__func__, ret);
			break;
		}

		pe40_cap->pwr_limit[idx] = cap.pwr_limit;
		pe40_cap->ma[idx] = cap.ma;
		pe40_cap->max_mv[idx] = cap.max_mv;
		pe40_cap->min_mv[idx] = cap.min_mv;
		pe40_cap->maxwatt[idx] = cap.max_mv * cap.ma;
		pe40_cap->minwatt[idx] = cap.min_mv * cap.ma;
		if (cap.pwr_limit == 1)
			pe40->max_vbus = 9000;
		idx++;
		chr_err("pps_boundary[%d], %d mv ~ %d mv, %d ma pl:%d\n", cap_i,
			cap.min_mv, cap.max_mv, cap.ma, cap.pwr_limit);
	}

	pe40_cap->nr = idx;

	for (i = 0; i < pe40_cap->nr; i++) {
		chr_err("pps_cap[%d:%d], %d mv ~ %d mv, %d ma pl:%d\n", i,
			(int)pe40_cap->nr, pe40_cap->min_mv[i],
			pe40_cap->max_mv[i], pe40_cap->ma[i], cap.pwr_limit);
	}

	if (cap_i == 0)
		chr_err("no APDO for pps\n");
}

int mtk_pe40_get_setting_by_watt(struct charger_manager *pinfo, int *voltage,
	int *adapter_ibus, int *actual_current, int watt,
	int *ibus_current_setting)
{
	int i;
	struct mtk_pe40 *pe40;
	struct pe40_power_cap *pe40_cap;
	int vbus = 0, ibus = 0, ibus_setting = 0;
	int idx = 0, ta_ibus = 0;

	if (pinfo->tcpc == NULL)
		return -1;

	pe40 = &pinfo->pe4;

	pe40_cap = &pinfo->pe4.cap;
	for (i = 0; i < pe40_cap->nr; i++) {
		int max_ibus = 0;
		int max_vbus = 0;

		/* update upper bound */
		if (pe40_cap->ma[i] > pe40->max_ibus)
			max_ibus = pe40->max_ibus;
		else
			max_ibus = pe40_cap->ma[i];

		if (max_ibus > pinfo->pe4.input_current_limit / 1000)
			max_ibus = pinfo->pe4.input_current_limit / 1000;

		if (pe40->pe4_input_current_limit != -1 &&
			max_ibus > (pe40->pe4_input_current_limit / 1000))
			max_ibus = pe40->pe4_input_current_limit / 1000;

		pe40->max_charger_ibus = max_ibus * (100 - IBUS_ERR) / 100;

		chr_err("pe4: %d %d %d %d %d %d\n",
			pe40_cap->ma[i], pe40->max_ibus,
			pinfo->pe4.input_current_limit / 1000,
			pe40->pe4_input_current_limit / 1000,
			max_ibus, pe40->max_charger_ibus);


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

		/* is max watt ok */
		if (max_vbus * (pe40->max_charger_ibus - 200) >= watt) {
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
		idx = 4;

	}

	*voltage = vbus;
	*ibus_current_setting = ibus_setting;
	*actual_current = ibus;
	*adapter_ibus = ta_ibus;

	chr_err("mtk_pe40_get_setting_by_watt:[%d,%d]%d vbus:%d ibus:%d aicl:%d current:%d %d\n",
		idx, i,
		watt, *voltage,
		*adapter_ibus,
		*ibus_current_setting,
		ibus, pe40->max_charger_ibus);

	return idx;
}

bool mtk_pe40_get_is_connect(struct charger_manager *pinfo)
{
	if (mtk_is_TA_support_pd_pps(pinfo) == false)
		return false;

	if (pinfo->enable_pe_4 == false)
		return false;

	if ((get_boot_mode() == META_BOOT) && (get_boot_mode() == ADVMETA_BOOT))
		return false;

	return pinfo->pe4.is_connect;
}

void mtk_pe40_set_is_enable(struct charger_manager *pinfo, bool enable)
{
	if (pinfo->enable_pe_4 == false)
		return;

	pinfo->pe4.is_enabled = enable;
}

bool mtk_pe40_get_is_enable(struct charger_manager *pinfo)
{
	if (pinfo->enable_pe_4 == false)
		return false;

	return pinfo->pe4.is_enabled;
}

bool mtk_pe40_is_ready(struct charger_manager *pinfo)
{
	struct charger_data *pdata;
	int tmp;
	int ibus;
	int ret;

	tmp = battery_get_bat_temperature();
	pdata = &pinfo->chg1_data;

	ret = charger_dev_get_ibus(pinfo->chg1_dev, &ibus);
	chr_err("pe40_ready:%d hv:%d thermal:%d,%d tmp:%d,%d,%d pps:%d en:%d ibus:%d %d\n",
		pinfo->enable_pe_4,
		pinfo->enable_hv_charging,
		pdata->thermal_charging_current_limit,
		pdata->thermal_input_current_limit,
		tmp,
		HIGH_TEMP_TO_ENTER_PE40,
		LOW_TEMP_TO_ENTER_PE40,
		mtk_is_TA_support_pd_pps(pinfo),
		mtk_pe40_get_is_enable(pinfo),
		ret,
		pinfo->data.pe40_stop_battery_soc);

	if (pinfo->enable_pe_4 == false ||
		pinfo->enable_hv_charging == false ||
		pdata->thermal_charging_current_limit != -1 ||
		pdata->thermal_input_current_limit != -1 ||
		tmp > HIGH_TEMP_TO_ENTER_PE40 ||
		tmp < LOW_TEMP_TO_ENTER_PE40 ||
		ret == -ENOTSUPP)
		return false;

	if (is_dual_charger_supported(pinfo) == true &&
		(battery_get_soc() >= pinfo->data.pe40_stop_battery_soc ||
		battery_get_uisoc() == -1))
		return false;

	if (mtk_is_TA_support_pd_pps(pinfo) == true)
		return mtk_pe40_get_is_enable(pinfo);

	return false;
}

int pe40_tcpm_dpm_pd_get_pps_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pe4_pps_status *pe4_status)
{
	int ret;
	struct pd_pps_status pps_status;

	ret = tcpm_dpm_pd_get_pps_status(tcpc, NULL, &pps_status);
	if (ret != 0)
		return ret;

	pe4_status->output_mv = pps_status.output_mv;
	pe4_status->output_ma = pps_status.output_ma;

	return ret;
}

int mtk_pe40_get_init_watt(struct charger_manager *pinfo)
{
	int ret;
	struct mtk_pe40 *pe40;
	struct charger_data *pdata;
	int vbus1, ibus1;
	int vbus2, ibus2;
	int vbat1, vbat2;
	int voltage = 0, input_current = 1000, actual_current = 0;
	int voltage1 = 0, adapter_ibus;
	bool is_enable = false, is_chip_enable = false;
	int i;

	if (pinfo->tcpc == NULL)
		return -1;

	if (pinfo->enable_hv_charging == false)
		return -1;

	pdata = &pinfo->chg1_data;
	pe40 = &pinfo->pe4;
	voltage = 0;
	mtk_pe40_get_setting_by_watt(pinfo, &voltage, &adapter_ibus,
		&actual_current, 27000000, &input_current);
	ret = mtk_pe40_pd_request(pinfo, voltage, adapter_ibus,
				input_current, NULL);

	if (ret != 0 && ret != TCP_DPM_RET_REJECT) {
		chr_err("[pe40_i1] err:1 %d\n", ret);
		return -1;
	}

	for (i = 0; i < 3 ; i++) {
		charger_dev_dump_registers(pinfo->chg1_dev);
		msleep(100);
	}

	charger_dev_get_ibus(pinfo->chg1_dev, &ibus1);
	vbus1 = battery_get_vbus();
	ibus1 = ibus1 / 1000;
	vbat1 = battery_get_bat_voltage();
	voltage1 = voltage;

	voltage = 0;
	mtk_pe40_get_setting_by_watt(pinfo, &voltage, &adapter_ibus,
		&actual_current, 15000000, &input_current);


	for (i = 0; i < 6 ; i++) {
		ret = mtk_pe40_pd_request(pinfo, voltage, adapter_ibus,
			input_current, NULL);
		if (ret != 0) {
			chr_err("[pe40_i1] err:2 %d\n", ret);
			return -1;
		}

		msleep(100);
		charger_dev_get_ibus(pinfo->chg1_dev, &ibus2);
		vbus2 = battery_get_vbus();
		ibus2 = ibus2 / 1000;
		vbat2 = battery_get_bat_voltage();

		if (is_dual_charger_supported(pinfo) == true) {
			charger_dev_is_enabled(pinfo->chg2_dev, &is_enable);
			charger_dev_is_chip_enabled(pinfo->chg2_dev,
						&is_chip_enable);
		}

		chr_err("[pe40_vbus] vbus1:%d ibus1:%d vbus2:%d ibus2:%d watt:%d en:%d %d vbat:%d %d\n",
			vbus1, ibus1, vbus2, ibus2, voltage1 * ibus1, is_enable,
			is_chip_enable, vbat1, vbat2);
	}

	return voltage1 * ibus1;
}


int mtk_pe40_init_state(struct charger_manager *pinfo)
{
	struct switch_charging_alg_data *swchgalg = pinfo->algorithm_data;
	int ret = 0;
	struct mtk_pe40 *pe40;
	int vbus1, vbat1, ibus1;
	int vbus2, vbat2, ibus2;
	struct pe4_pps_status cap, cap1, cap2;
	int voltage, adapter_ibus = 1000, actual_current;
	int watt = 0;
	int i;
	int input_current = 0;
	bool chg2_chip_enabled = false;

	struct charger_data *pdata2;

	if (pinfo->tcpc == NULL)
		goto err;

	if (pinfo->enable_hv_charging == false)
		goto retry;

	chr_err("set TD false\n");
	charger_dev_enable_termination(pinfo->chg1_dev, false);

	pmic_enable_hw_vbus_ovp(false);
	charger_enable_vbus_ovp(pinfo, false);

	pdata2 = &pinfo->chg2_data;

	pe40 = &pinfo->pe4;
	mtk_pe40_init_cap(pinfo);
	pinfo->pe4.is_connect = true;
	voltage = 0;
	mtk_pe40_get_setting_by_watt(pinfo, &voltage, &adapter_ibus,
		&actual_current, 5000000, &input_current);

	ret = mtk_pe40_pd_1st_request(pinfo, voltage, actual_current,
		actual_current, NULL);

	if (ret != 0) {
		chr_err("[pe40_i0] err:1 %d\n", ret);
		goto retry;
	}

	/* disable charger */
	charger_dev_enable_powerpath(pinfo->chg1_dev, false);
	if (is_dual_charger_supported(pinfo) == true) {
		charger_dev_is_chip_enabled(pinfo->chg2_dev,
					&chg2_chip_enabled);
		if (chg2_chip_enabled) {
			charger_dev_enable(pinfo->chg2_dev, false);
			charger_dev_enable_chip(pinfo->chg2_dev, false);
		}
	}
	msleep(500);

	cap.output_ma = 0;
	cap.output_mv = 0;
	ret = pe40_tcpm_dpm_pd_get_pps_status(pinfo->tcpc, NULL, &cap);

	pe40->can_query = true;
	if (ret == 0 && (cap.output_ma == -1 || cap.output_mv == -1))
		pe40->can_query = false;
	else if (ret == TCP_DPM_RET_NOT_SUPPORT)
		pe40->can_query = false;
	else if (ret != 0) {
		chr_err("[pe40_i0] err:2 %d\n", ret);
		goto err;
	}

	chr_err("[pe40_i0] can_query:%d ret:%d\n",
		pe40->can_query,
		ret);

	pe40->pmic_vbus = battery_get_vbus();
	pe40->TA_vbus = cap.output_mv;
	pe40->vbus_cali = pe40->TA_vbus - pe40->pmic_vbus;

	chr_err("[pe40_i0]pmic_vbus:%d TA_vbus:%d cali:%d ibus:%d chip2:%d\n",
		pe40->pmic_vbus, pe40->TA_vbus, pe40->vbus_cali,
		cap.output_ma, chg2_chip_enabled);

	/*enable charger*/
	charger_dev_enable_powerpath(pinfo->chg1_dev, true);
	if (is_dual_charger_supported(pinfo) == true) {
		charger_dev_is_chip_enabled(pinfo->chg2_dev,
					&chg2_chip_enabled);
		if (chg2_chip_enabled == false)
			charger_dev_enable_chip(pinfo->chg2_dev, true);
		charger_dev_enable(pinfo->chg2_dev, true);

	charger_dev_set_input_current(pinfo->chg2_dev,
		pdata2->input_current_limit);
	charger_dev_set_charging_current(pinfo->chg2_dev,
		pdata2->charging_current_limit);
	}
	msleep(100);

	if (cap.output_ma > 100) {
		chr_err("[pe40_i0] FOD fail :%d\n", cap.output_ma);
		goto err;
	}

	if (pe40->can_query == true) {
		/* measure 1 */
		voltage = 0;
		mtk_pe40_get_setting_by_watt(pinfo, &voltage, &adapter_ibus,
			&actual_current, 5000000, &input_current);
		ret = mtk_pe40_pd_request(pinfo, voltage, actual_current,
					actual_current, NULL);
		if (ret != 0) {
			chr_err("[pe40_i0] err:3 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus1 = battery_get_vbus();
			vbat1 = battery_get_bat_voltage();
			charger_dev_get_ibus(pinfo->chg1_dev, &ibus1);
			ibus1 = ibus1 / 1000;
			ret = pe40_tcpm_dpm_pd_get_pps_status(pinfo->tcpc,
								NULL, &cap1);
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
		mtk_pe40_get_setting_by_watt(pinfo, &voltage, &adapter_ibus,
			&actual_current, 7500000, &input_current);
		ret = mtk_pe40_pd_request(pinfo, voltage, actual_current,
					actual_current, NULL);
		if (ret != 0) {
			chr_err("[pe40_i0] err:5 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus2 = battery_get_vbus();
			vbat2 = battery_get_bat_voltage();
			charger_dev_get_ibus(pinfo->chg1_dev, &ibus2);
			ibus2 = ibus2 / 1000;
			ret = pe40_tcpm_dpm_pd_get_pps_status(pinfo->tcpc,
								NULL, &cap2);
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

		pe40->r_sw = abs((vbus2 - vbus1) - (vbat2 - vbat1)) * 1000 /
				abs(cap2.output_ma - cap1.output_ma);
		pe40->r_cable = abs((cap2.output_mv - cap1.output_mv) -
				    (vbus2 - vbus1)) * 1000 /
				abs(cap2.output_ma - cap1.output_ma);
		pe40->r_cable_2 = abs(cap2.output_mv - pe40->vbus_cali - vbus2)
				* 1000 / abs(cap2.output_ma);
		pe40->r_cable_1 = abs(cap1.output_mv - pe40->vbus_cali - vbus1)
				* 1000 / abs(cap1.output_ma);

		if (pe40->r_cable_1 < pinfo->data.pe40_r_cable_3a_lower)
			pe40->pe4_input_current_limit = 5000000;
		else if (pe40->r_cable_1 >= pinfo->data.pe40_r_cable_3a_lower &&
			pe40->r_cable_1 < pinfo->data.pe40_r_cable_2a_lower)
			pe40->pe4_input_current_limit = 3000000;
		else if (pe40->r_cable_1 >= pinfo->data.pe40_r_cable_2a_lower &&
			pe40->r_cable_1 < pinfo->data.pe40_r_cable_1a_lower)
			pe40->pe4_input_current_limit = 2000000;
		else if (pe40->r_cable_1 >= pinfo->data.pe40_r_cable_1a_lower)
			pe40->pe4_input_current_limit = 1000000;

		chr_err("[pe40_i2]r_sw:%d r_cable:%d r_cable_1:%d r_cable_2:%d pe4_icl:%d\n",
			pe40->r_sw, pe40->r_cable, pe40->r_cable_1,
			pe40->r_cable_2, pe40->pe4_input_current_limit);
		} else
			chr_err("TA does not support query\n");

	watt = mtk_pe40_get_init_watt(pinfo);
	voltage = 0;
	mtk_pe40_get_setting_by_watt(pinfo, &voltage, &adapter_ibus,
				&actual_current, watt, &input_current);
	pe40->avbus = voltage / 10 * 10;
	ret = mtk_pe40_pd_request(pinfo, pe40->avbus, adapter_ibus,
				input_current, NULL);
	if (ret != 0 && ret != TCP_DPM_RET_REJECT) {
		chr_err("[pe40_i0] err:6 %d\n", ret);
		goto err;
	}

	pe40->avbus = voltage;
	pe40->ibus = watt / voltage;
	pe40->watt = watt;

	swchgalg->state = CHR_PE40_CC;
	pinfo->polling_interval = 10;

	return 0;

retry:
	mtk_pe40_end(pinfo, 0, true);
	return 0;
err:
	mtk_pe40_end(pinfo, 2, false);
	return 0;
}

int mtk_pe40_safety_check(struct charger_manager *pinfo)
{
	int vbus;
	struct mtk_pe40 *pe40;
	struct pe4_pps_status cap;
	struct pd_status TAstatus = {0,};
	int ret;
	int tmp;
	int i;
	int high_tmp_cnt = 0;

	if (pinfo->tcpc == NULL)
		goto err;

	pe40 = &pinfo->pe4;

	/* vbus ov */
	vbus = battery_get_vbus();
	if (vbus - pe40->avbus >= 2000) {
		chr_err("[pe40_err]vbus ov :vbus:%d avbus:%d\n",
			vbus, pe40->avbus);
		goto err;
	}

	/* cable voltage drop check */
	if (pe40->can_query == true) {
		ret = pe40_tcpm_dpm_pd_get_pps_status(pinfo->tcpc, NULL, &cap);
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
		if (cap.output_mv >= pe40->avbus * 12 / 10) {
			chr_err("[pe40_err]TA vbus ovp :vbus:%d avbus:%d\n",
				cap.output_mv, pe40->avbus);
			goto err;
		}
	}

	/* TA Thermal */
	for (i = 0; i < 3; i++) {
		ret = tcpm_dpm_pd_get_status(pinfo->tcpc, NULL, &TAstatus);
		if (TAstatus.internal_temp >= 100 &&
			TAstatus.internal_temp != 0 &&
			ret != TCP_DPM_RET_NOT_SUPPORT &&
			ret != TCP_DPM_RET_TIMEOUT) {
			high_tmp_cnt++;
			chr_err("[pe40]TA Thermal:%d cnt:%d\n",
				TAstatus.internal_temp, high_tmp_cnt);
		} else if (ret == TCP_DPM_RET_TIMEOUT) {
			chr_err("[pe40]TA tcpm_dpm_pd_get_status timeout\n");
			goto err;
		} else
			break;

		if (high_tmp_cnt >= 3) {
			chr_err("[pe40_err]TA Thermal: %d thd:%d cnt:%d\n",
				TAstatus.internal_temp, 100, high_tmp_cnt);
			goto err;
		}
	}

	if (ret == TCP_DPM_RET_NOT_SUPPORT)
		chr_err("[pe40]TA tcpm_dpm_pd_get_status not support\n");
	else {
		if (TAstatus.event_flags & PD_STASUS_EVENT_OCP ||
			TAstatus.event_flags & PD_STATUS_EVENT_OTP ||
			TAstatus.event_flags & PD_STATUS_EVENT_OVP) {

			chr_err("[pe40_err]TA protect: ocp:%d otp:%d ovp:%d\n",
				TAstatus.event_flags & PD_STASUS_EVENT_OCP,
				TAstatus.event_flags & PD_STATUS_EVENT_OTP,
				TAstatus.event_flags & PD_STATUS_EVENT_OVP);
			goto err;
		}

		chr_err("PD_TA:TA protect: ocp:%d otp:%d ovp:%d tmp:%d\n",
			TAstatus.event_flags & PD_STASUS_EVENT_OCP,
			TAstatus.event_flags & PD_STATUS_EVENT_OTP,
			TAstatus.event_flags & PD_STATUS_EVENT_OVP,
			TAstatus.internal_temp);
	}

	tmp = battery_get_bat_temperature();

	if (tmp > HIGH_TEMP_TO_LEAVE_PE40 || tmp < LOW_TEMP_TO_LEAVE_PE40) {

		chr_err("[pe40_err]tmp:%d threshold:%d %d\n",
			tmp, HIGH_TEMP_TO_LEAVE_PE40, LOW_TEMP_TO_LEAVE_PE40);
		return 1;
	}

	return 0;
err:
	return -1;
}

int mtk_pe40_cc_state(struct charger_manager *pinfo)
{
	int ibus = 0, vbat, ibat, vbus;
	int icl, ccl, ccl2, cv, max_icl;
	struct mtk_pe40 *pe40;
	int ret;
	int oldavbus = 0;
	int watt;
	int max_watt;
	struct charger_data *pdata;
	int actual_current;
	int new_watt = 0;
	int adapter_ibus = 0;
	int input_current = 0;
	int icl_threshold;
	bool mivr_loop;

	if (pinfo->tcpc == NULL)
		goto err;

	if (pinfo->enable_hv_charging == false)
		goto disable_hv;

	pdata = &pinfo->chg1_data;
	pe40 = &pinfo->pe4;

	vbat = battery_get_bat_voltage();
	ibat = battery_get_bat_current_mA();

	charger_dev_get_mivr_state(pinfo->chg1_dev, &mivr_loop);
	charger_dev_get_ibus(pinfo->chg1_dev, &ibus);
	vbus = battery_get_vbus();
	ibus = ibus / 1000;
	icl = pinfo->chg1_data.input_current_limit / 1000 *
		(100 - IBUS_ERR) / 100;
	ccl = pinfo->chg1_data.charging_current_limit / 1000;
	ccl2 = pinfo->chg2_data.charging_current_limit / 1000;
	cv = pinfo->data.battery_cv / 1000;
	watt = pe40->avbus * ibus;

	if (icl > pe40->max_charger_ibus)
		max_icl = pe40->max_charger_ibus;
	else
		max_icl = icl;

	icl_threshold = 100;

	max_watt = pe40->avbus * max_icl;

	chr_err("[pe40_cc]vbus:%d:%d,ibus:%d,ibat:%d icl:%d:%d,ccl:%d,%d,vbat:%d,maxIbus:%d\n",
		pe40->avbus, vbus,
		ibus,
		ibat,
		icl, max_icl,
		ccl, ccl2,
		vbat, pe40->max_charger_ibus);

	if ((mivr_loop && vbus <= 5000) ||
	    (ibus >= (max_icl - icl_threshold)) ||
	    (ibus <= (max_icl - icl_threshold * 2))) {

		oldavbus = pe40->avbus;

		if (mivr_loop && vbus <= 5000) {
			pe40->avbus = pe40->avbus + 50;
			new_watt = (pe40->avbus + 50) * icl;
		} else if (ibus >= (max_icl - icl_threshold)) {
			pe40->avbus = pe40->avbus + 50;
			new_watt = (pe40->avbus + 50) * ibus;
		} else if (ibus <= (max_icl - icl_threshold * 2)) {
			new_watt = pe40->avbus * pe40->ibus - 500000;
			pe40->avbus = pe40->avbus - 50;
		}

		ret = mtk_pe40_get_setting_by_watt(pinfo, &pe40->avbus,
				&adapter_ibus, &actual_current, new_watt,
			&input_current);

		if (ibus >= (max_icl - icl_threshold) && ret != 4)
			pinfo->polling_interval = 3;

		if (pe40->avbus <= 5000)
			pe40->avbus = 5000;

		if (abs(pe40->avbus - oldavbus) >= 50) {
			ret = mtk_pe40_pd_request(pinfo, pe40->avbus,
					adapter_ibus, input_current, NULL);
			if (ret != 0 && ret != TCP_DPM_RET_REJECT)
				goto err;
		}
		msleep(100);

		vbat = battery_get_bat_voltage();
		ibat = battery_get_bat_current_mA();
		charger_dev_get_ibus(pinfo->chg1_dev, &ibus);
		vbus = battery_get_vbus();
		ibus = ibus / 1000;
		icl = pinfo->chg1_data.input_current_limit / 1000;
		ccl = pinfo->chg1_data.charging_current_limit / 1000;

		pe40->watt = pe40->avbus * ibus;
		pe40->vbus = vbus;
		pe40->ibus = ibus;
	} else
		pinfo->polling_interval = 10;

	ret = mtk_pe40_safety_check(pinfo);
	if (ret == -1)
		goto err;
	if (ret == 1)
		goto disable_hv;

	if (pe40->avbus * ibus <= PE40_MIN_WATT) {
		if (pinfo->enable_hv_charging == false ||
			pdata->thermal_charging_current_limit != -1 ||
			pdata->thermal_input_current_limit != -1)
			mtk_pe40_end(pinfo, 1, true);

		else
			mtk_pe40_end(pinfo, 1, false);
	}

	return 0;

disable_hv:
	mtk_pe40_end(pinfo, 0, true);
	return 0;
err:
	mtk_pe40_end(pinfo, 2, false);
	return 0;
}


bool mtk_pe40_init(struct charger_manager *pinfo)
{
	mtk_pe40_reset(pinfo, true);

	return true;
}



