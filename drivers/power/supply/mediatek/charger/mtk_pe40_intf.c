// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#define PE40_VBUS_IR_DROP_THRESHOLD 1200

int mtk_pe40_set_mivr(struct charger_manager *pinfo, int uV)
{
	int ret = 0;
	bool chg2_chip_enabled = false;

	ret = charger_dev_set_mivr(pinfo->chg1_dev, uV);
	if (ret < 0)
		chr_err("%s: failed, ret = %d\n", __func__, ret);

	if (pinfo->chg2_dev) {
		charger_dev_is_chip_enabled(pinfo->chg2_dev,
			&chg2_chip_enabled);
		if (chg2_chip_enabled) {
			ret = charger_dev_set_mivr(pinfo->chg2_dev,
				uV + pinfo->data.slave_mivr_diff);
			if (ret < 0)
				pr_info("%s: chg2 failed, ret = %d\n", __func__,
					ret);
		}
	}

	return ret;
}

int mtk_pe40_pd_1st_request(struct charger_manager *pinfo,
	int adapter_mv, int adapter_ma, int ma)
{
	unsigned int oldmA = 3000000;
	int ret;
	int mivr;
	bool chg2_enable = false;

	if (is_dual_charger_supported(pinfo))
		charger_dev_is_enabled(pinfo->chg2_dev, &chg2_enable);

	mivr = pinfo->data.min_charger_voltage / 1000;
	mtk_pe40_set_mivr(pinfo, pinfo->data.min_charger_voltage);

	charger_dev_get_input_current(pinfo->chg1_dev, &oldmA);
	oldmA = oldmA / 1000;

	chr_err("pe40_pd_req:vbus:%d ibus:%d input_current:%d %d\n",
		adapter_mv, adapter_ma, ma, oldmA);

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

	ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO_START,
		adapter_mv, adapter_ma);

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

	if ((adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD;

	mtk_pe40_set_mivr(pinfo, mivr * 1000);

	pinfo->pe4.pe4_input_current_limit_setting = ma * 1000;
	return ret;
}

int mtk_pe40_pd_request(struct charger_manager *pinfo,
	int *adapter_vbus, int *adapter_ibus, int ma)
{
	unsigned int oldmA = 3000000;
	unsigned int oldmivr = 4600;
	int ret;
	int mivr;
	int adapter_mv, adapter_ma;
	struct mtk_pe40 *pe40;
	bool chg2_enable = false;

	if (is_dual_charger_supported(pinfo))
		charger_dev_is_enabled(pinfo->chg2_dev, &chg2_enable);

	pe40 = &pinfo->pe4;
	adapter_mv = *adapter_vbus;
	adapter_ma = *adapter_ibus;

	charger_dev_get_mivr(pinfo->chg1_dev, &oldmivr);

	mivr = pinfo->data.min_charger_voltage / 1000;
	mtk_pe40_set_mivr(pinfo, pinfo->data.min_charger_voltage);

	charger_dev_get_input_current(pinfo->chg1_dev, &oldmA);
	oldmA = oldmA / 1000;

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

	ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO,
		adapter_mv, adapter_ma);

	chr_err("pe40_pd_req:vbus:%d ibus:%d input_current:%d ret:%d\n",
		adapter_mv, adapter_ma, ma, ret);

	if (ret == MTK_ADAPTER_REJECT) {
		chr_err("pe40_pd_req: reject\n");

		if (pe40->cap.pdp > 0 &&
			adapter_mv * adapter_ma > pe40->cap.pdp * 1000000) {
			*adapter_ibus = pe40->cap.pdp * 1000000
					/ adapter_mv;
			ret = adapter_dev_set_cap(pinfo->pd_adapter,
				MTK_PD_APDO, adapter_mv, *adapter_ibus);

			chr_err("pe40_pd_req:vbus:%d new_ibus:%d pdp:%d ret:%d\n",
				adapter_mv, *adapter_ibus,
				pe40->cap.pdp, ret);

			if (ret == MTK_ADAPTER_OK)
				ret = MTK_ADAPTER_ADJUST;

			if (ret == MTK_ADAPTER_REJECT)
				goto err;
		} else
			goto err;
	}

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

	if ((adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD) > mivr)
		mivr = adapter_mv - PE40_VBUS_IR_DROP_THRESHOLD;

	mtk_pe40_set_mivr(pinfo, mivr * 1000);

	pinfo->pe4.pe4_input_current_limit_setting = ma * 1000;
	return ret;

err:
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

	mtk_pe40_set_mivr(pinfo, oldmivr);
	return ret;
}

void mtk_pe40_reset(struct charger_manager *pinfo, bool enable)
{
	struct switch_charging_alg_data *swchgalg = pinfo->algorithm_data;
	struct mtk_pe40 *pe40;

	pe40 = &pinfo->pe4;

	if (pe40->is_connect == true) {
		adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO_END,
			5000, 2000);

		mtk_pe40_set_mivr(pinfo, pinfo->data.min_charger_voltage);
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
	pe40->max_vbus = pinfo->data.pe40_max_vbus;
	pe40->max_ibus = pinfo->data.pe40_max_ibus;
	pe40->max_charger_ibus = pinfo->data.pe40_max_ibus *
				(100 - pinfo->data.ibus_err) / 100;
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
		chr_err("%s:%d retry:%d\n", __func__, type, retry);
	}
}

bool mtk_is_TA_support_pd_pps(struct charger_manager *pinfo)
{
	if (pinfo->enable_pe_4 == false)
		return false;

	if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
		return true;
	return false;
}

void mtk_pe40_init_cap(struct charger_manager *info)
{
	adapter_dev_get_cap(info->pd_adapter, MTK_PD_APDO, &info->pe4.cap);
}

int mtk_pe40_get_setting_by_watt(struct charger_manager *pinfo, int *voltage,
	int *adapter_ibus, int *actual_current, int watt,
	int *ibus_current_setting)
{
	int i;
	struct mtk_pe40 *pe40;
	struct adapter_power_cap *pe40_cap;
	int vbus = 0, ibus = 0, ibus_setting = 0;
	int idx = 0, ta_ibus = 0;

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

		pe40->max_charger_ibus = max_ibus *
					(100 - pinfo->data.ibus_err) / 100;

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

	chr_err("%s:[%d,%d]%d vbus:%d ibus:%d aicl:%d current:%d %d\n",
		__func__,
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

int mtk_pe40_get_ibus(struct charger_manager *pinfo, u32 *ibus)
{
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

	if (pinfo->data.parallel_vbus) {
		ret = charger_dev_get_ibus(pinfo->chg1_dev, &chg1_ibus);

		if (is_enable) {
			ret = charger_dev_get_ibat(pinfo->chg1_dev, &chg1_ibat);
			if (ret < 0)
				chr_err("[%s] get ibat fail\n", __func__);

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

		chr_err("[%s] chg2_watt:%d ibat2:%d ibat1:%d ibat:%d ibus1:%d ibus2:%d ibus:%d\n",
			__func__, chg2_watt, chg2_ibat, chg1_ibat, ibat * 100,
			chg1_ibus, chg2_ibus, *ibus);
	} else {
		ret = charger_dev_get_ibus(pinfo->chg1_dev, ibus);
	}

	return 0;
}

bool mtk_pe40_is_ready(struct charger_manager *pinfo)
{
	struct charger_data *pdata;
	int tmp;
	int ibus;
	int ret;

	tmp = battery_get_bat_temperature();
	pdata = &pinfo->chg1_data;

	ret = mtk_pe40_get_ibus(pinfo, &ibus);

	chr_err("pe40_ready:%d hv:%d thermal:%d,%d tmp:%d,%d,%d pps:%d en:%d ibus:%d %d\n",
		pinfo->enable_pe_4,
		pinfo->enable_hv_charging,
		pdata->thermal_charging_current_limit,
		pdata->thermal_input_current_limit,
		tmp,
		pinfo->data.high_temp_to_enter_pe40,
		pinfo->data.low_temp_to_enter_pe40,
		mtk_is_TA_support_pd_pps(pinfo),
		mtk_pe40_get_is_enable(pinfo),
		ret,
		pinfo->data.pe40_stop_battery_soc);

	if (pinfo->enable_pe_4 == false ||
		pinfo->enable_hv_charging == false ||
		pdata->thermal_charging_current_limit != -1 ||
		pdata->thermal_input_current_limit != -1 ||
		tmp > pinfo->data.high_temp_to_enter_pe40 ||
		tmp < pinfo->data.low_temp_to_enter_pe40 ||
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

int pe40_get_output(struct charger_manager *pinfo,
	struct pe4_pps_status *pe4_status)
{
	int ret;

	ret = adapter_dev_get_output(pinfo->pd_adapter,
		&pe4_status->output_mv,
		&pe4_status->output_ma);

	if (ret != 0)
		return ret;

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

	if (pinfo->enable_hv_charging == false)
		return -1;

	pdata = &pinfo->chg1_data;
	pe40 = &pinfo->pe4;
	voltage = 0;
	mtk_pe40_get_setting_by_watt(pinfo, &voltage, &adapter_ibus,
		&actual_current, 27000000, &input_current);
	ret = mtk_pe40_pd_request(pinfo, &voltage, &adapter_ibus,
				input_current);

	if (ret != 0 && ret != MTK_ADAPTER_REJECT &&
			ret != MTK_ADAPTER_ADJUST) {
		chr_err("[pe40_i1] err:1 %d\n", ret);
		return -1;
	}

	for (i = 0; i < 3 ; i++) {
		charger_dev_dump_registers(pinfo->chg1_dev);
		msleep(100);
	}

	mtk_pe40_get_ibus(pinfo, &ibus1);
	vbus1 = battery_get_vbus();
	ibus1 = ibus1 / 1000;
	vbat1 = battery_get_bat_voltage();
	voltage1 = voltage;

	voltage = 0;
	mtk_pe40_get_setting_by_watt(pinfo, &voltage, &adapter_ibus,
		&actual_current, 15000000, &input_current);


	for (i = 0; i < 6 ; i++) {
		ret = mtk_pe40_pd_request(pinfo, &voltage, &adapter_ibus,
			input_current);

		if (ret != 0 && ret != MTK_ADAPTER_ADJUST) {
			chr_err("[pe40_i1] err:2 %d\n", ret);
			return -1;
		}

		msleep(100);
		mtk_pe40_get_ibus(pinfo, &ibus2);
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
		actual_current);

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

	ret = pe40_get_output(pinfo, &cap);

	pe40->can_query = true;
	if (ret == 0 && (cap.output_ma == -1 || cap.output_mv == -1))
		pe40->can_query = false;
	else if (ret == MTK_ADAPTER_NOT_SUPPORT)
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
		ret = mtk_pe40_pd_request(pinfo, &voltage, &actual_current,
					actual_current);

		if (ret != 0 && ret != MTK_ADAPTER_ADJUST) {
			chr_err("[pe40_i0] err:3 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus1 = battery_get_vbus();
			vbat1 = battery_get_bat_voltage();
			mtk_pe40_get_ibus(pinfo, &ibus1);
			ibus1 = ibus1 / 1000;
			ret = pe40_get_output(pinfo, &cap1);
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
		ret = mtk_pe40_pd_request(pinfo, &voltage, &actual_current,
					actual_current);

		if (ret != 0 && ret != MTK_ADAPTER_ADJUST) {
			chr_err("[pe40_i0] err:5 %d\n", ret);
			goto err;
		}

		for (i = 0; i < 4; i++) {
			msleep(250);
			vbus2 = battery_get_vbus();
			vbat2 = battery_get_bat_voltage();
			mtk_pe40_get_ibus(pinfo, &ibus2);
			ibus2 = ibus2 / 1000;
			ret = pe40_get_output(pinfo, &cap2);
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
	ret = mtk_pe40_pd_request(pinfo, &pe40->avbus, &adapter_ibus,
				input_current);

	if (ret != 0 && ret != MTK_ADAPTER_REJECT &&
			ret != MTK_ADAPTER_ADJUST) {
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
	struct adapter_status TAstatus;
	int ret;
	int tmp;
	int i;
	int high_tmp_cnt = 0;

	pe40 = &pinfo->pe4;

	TAstatus.ocp = 0;
	TAstatus.otp = 0;
	TAstatus.ovp = 0;
	TAstatus.temperature = 0;

	/* vbus ov */
	vbus = battery_get_vbus();
	if (vbus - pe40->avbus >= 2000) {
		chr_err("[pe40_err]vbus ov :vbus:%d avbus:%d\n",
			vbus, pe40->avbus);
		goto err;
	}

	/* cable voltage drop check */
	if (pe40->can_query == true) {
		ret = pe40_get_output(pinfo, &cap);
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
		ret = adapter_dev_get_status(pinfo->pd_adapter, &TAstatus);
		if (TAstatus.temperature >= 100 &&
			TAstatus.temperature != 0 &&
			ret != MTK_ADAPTER_NOT_SUPPORT &&
			ret != MTK_ADAPTER_TIMEOUT) {
			high_tmp_cnt++;
			chr_err("[pe40]TA Thermal:%d cnt:%d\n",
				TAstatus.temperature, high_tmp_cnt);
		} else if (ret == MTK_ADAPTER_TIMEOUT) {
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

	if (ret == MTK_ADAPTER_NOT_SUPPORT)
		chr_err("[pe40]TA adapter_dev_get_status not support\n");
	else {
		if (TAstatus.ocp || TAstatus.otp || TAstatus.ovp) {

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

	if (tmp > pinfo->data.high_temp_to_leave_pe40 ||
		tmp < pinfo->data.low_temp_to_leave_pe40) {

		chr_err("[pe40_err]tmp:%d threshold:%d %d\n",
			tmp, pinfo->data.high_temp_to_leave_pe40,
			pinfo->data.low_temp_to_leave_pe40);
		return 1;
	}

	return 0;
err:
	return -1;
}

int mtk_pe40_cc_state(struct charger_manager *pinfo)
{
	int ibus = 0, vbat, ibat, vbus, compare_ibus = 0;
	int icl, ccl, ccl2, cv, max_icl;
	struct mtk_pe40 *pe40;
	int ret;
	int oldavbus = 0;
	int oldibus = 0;
	int watt;
	int max_watt;
	struct charger_data *pdata;
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

	if (pinfo->enable_hv_charging == false)
		goto disable_hv;

	pdata = &pinfo->chg1_data;
	pe40 = &pinfo->pe4;

	vbat = battery_get_bat_voltage();
	ibat = battery_get_bat_current_mA();

	mtk_pe40_get_ibus(pinfo, &ibus);
	ibus = ibus / 1000;
	oldibus = ibus;
	charger_dev_get_mivr_state(pinfo->chg1_dev, &chg1_mivr);
	charger_dev_get_mivr(pinfo->chg1_dev, &mivr1);

	if (is_dual_charger_supported(pinfo)) {
		charger_dev_is_enabled(pinfo->chg2_dev, &chg2_enable);
		if (chg2_enable) {
			charger_dev_get_mivr_state(pinfo->chg2_dev, &chg2_mivr);
			charger_dev_get_mivr(pinfo->chg2_dev, &mivr2);
		}
	}

	vbus = battery_get_vbus();
	ccl = pinfo->chg1_data.charging_current_limit / 1000;
	ccl2 = pinfo->chg2_data.charging_current_limit / 1000;
	cv = pinfo->data.battery_cv / 1000;
	watt = pe40->avbus * ibus;

	icl = pinfo->chg1_data.input_current_limit / 1000 *
		(100 - pinfo->data.ibus_err) / 100;

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

	icl_threshold = 100;
	max_watt = pe40->avbus * max_icl;

	chr_err("[pe40_cc]vbus:%d:%d,ibus:%d,cibus:%d,ibat:%d icl:%d:%d,ccl:%d,%d,vbat:%d,maxIbus:%d,mivr:%d,%d\n",
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
		mtk_pe40_end(pinfo, 1, true);
		return 0;
	}

	if (pinfo->data.parallel_vbus) {
		if (pinfo->chg1_data.thermal_input_current_limit != -1 ||
		    pinfo->chg2_data.thermal_input_current_limit != -1)
			thermal_skip = true;
	}

	if (((chg1_mivr || chg2_mivr) && !thermal_skip) ||
	    ((compare_ibus >= (max_icl - icl_threshold)) && !thermal_skip) ||
	    (compare_ibus <= (max_icl - icl_threshold * 2))) {

		oldavbus = pe40->avbus;

		if (chg1_mivr || chg2_mivr) {
			pe40->avbus = pe40->avbus + 50;
			if (pinfo->data.parallel_vbus)
				new_watt = (pe40->avbus + 50) * icl * 2;
			else
				new_watt = (pe40->avbus + 50) * icl;
		} else if (compare_ibus >= (max_icl - icl_threshold)) {
			pe40->avbus = pe40->avbus + 50;
			new_watt = (pe40->avbus + 50) * ibus;
		} else if (compare_ibus <= (max_icl - icl_threshold * 2)) {
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
			ret = mtk_pe40_pd_request(pinfo, &pe40->avbus,
					&adapter_ibus, input_current);
			if (ret != 0 && ret != MTK_ADAPTER_REJECT &&
					ret != MTK_ADAPTER_ADJUST)
				goto err;
		}
		msleep(100);

		vbat = battery_get_bat_voltage();
		ibat = battery_get_bat_current_mA();
		mtk_pe40_get_ibus(pinfo, &ibus);
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

	if (pe40->avbus * oldibus <= PE40_MIN_WATT) {
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



