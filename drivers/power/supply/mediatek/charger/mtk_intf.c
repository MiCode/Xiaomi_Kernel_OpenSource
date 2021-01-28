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

#include "mtk_intf.h"

static struct charger_manager *pinfo;

int charger_is_chip_enabled(bool *en)
{
	return charger_dev_is_chip_enabled(pinfo->chg1_dev, en);
}

int charger_enable_chip(bool en)
{
	return charger_dev_enable_chip(pinfo->chg1_dev, en);
}

int charger_is_enabled(bool *en)
{
	return charger_dev_is_enabled(pinfo->chg1_dev, en);
}

int charger_get_mivr_state(bool *in_loop)
{
	return charger_dev_get_mivr_state(pinfo->chg1_dev, in_loop);
}

int charger_get_mivr(u32 *uV)
{
	return charger_dev_get_mivr(pinfo->chg1_dev, uV);
}

int charger_set_mivr(u32 uV)
{
	return charger_dev_set_mivr(pinfo->chg1_dev, uV);
}

int charger_get_input_current(u32 *uA)
{
	return charger_dev_get_input_current(pinfo->chg1_dev, uA);
}

int charger_set_input_current(u32 uA)
{
	return charger_dev_set_input_current(pinfo->chg1_dev, uA);
}

int charger_set_charging_current(u32 uA)
{
	return charger_dev_set_charging_current(pinfo->chg1_dev, uA);
}

int charger_get_ibus(u32 *ibus)
{
	return charger_dev_get_ibus(pinfo->chg1_dev, ibus);
}

int charger_set_constant_voltage(u32 uV)
{
	return charger_dev_set_constant_voltage(pinfo->chg1_dev, uV);
}

int charger_enable_termination(bool en)
{
	return charger_dev_enable_termination(pinfo->chg1_dev, en);
}

int charger_enable_powerpath(bool en)
{
	return charger_dev_enable_powerpath(pinfo->chg1_dev, en);
}

int charger_dump_registers(void)
{
	return charger_dev_dump_registers(pinfo->chg1_dev);
}

int adapter_set_1st_cap(int mV, int mA)
{
	return adapter_dev_set_cap(pinfo->pd_adapter,
		MTK_PD_APDO_START, mV, mA);
}

int adapter_set_cap(int mV, int mA)
{
	int ret;

	ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO, mV, mA);
	if (ret == MTK_ADAPTER_REJECT)
		return ADAPTER_REJECT;
	else if (ret != 0)
		return ADAPTER_ERROR;

	return ADAPTER_OK;
}

int adapter_set_cap_start(int mV, int mA)
{
	int ret;

	ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO_START, mV, mA);
	if (ret == MTK_ADAPTER_REJECT)
		return ADAPTER_REJECT;
	else if (ret != 0)
		return ADAPTER_ERROR;

	return ADAPTER_OK;
}

int adapter_set_cap_end(int mV, int mA)
{
	int ret;

	ret = adapter_dev_set_cap(pinfo->pd_adapter, MTK_PD_APDO_END, mV, mA);
	if (ret == MTK_ADAPTER_REJECT)
		return ADAPTER_REJECT;
	else if (ret != 0)
		return ADAPTER_ERROR;

	return ADAPTER_OK;
}

int adapter_get_output(int *mV, int *mA)
{
	return adapter_dev_get_output(pinfo->pd_adapter, mV, mA);
}

int adapter_get_pps_cap(struct pps_cap *cap)
{
	struct adapter_power_cap tacap = {0};
	int i;

	adapter_dev_get_cap(pinfo->pd_adapter, MTK_PD_APDO, &tacap);

	cap->selected_cap_idx = tacap.selected_cap_idx;
	cap->nr = tacap.nr;
	cap->pdp = tacap.pdp;

	for (i = 0; i < tacap.nr; i++) {
		cap->pwr_limit[i] = tacap.pwr_limit[i];
		cap->max_mv[i] = tacap.max_mv[i];
		cap->min_mv[i] = tacap.min_mv[i];
		cap->ma[i] = tacap.ma[i];
		cap->maxwatt[i] = tacap.maxwatt[i];
		cap->minwatt[i] = tacap.minwatt[i];
		cap->type[i] = tacap.type[i];
	}

	return 0;
}

int adapter_get_status(struct ta_status *sta)
{
	struct adapter_status tasta = {0};
	int ret = 0;

	ret = adapter_dev_get_status(pinfo->pd_adapter, &tasta);

	sta->temperature = tasta.temperature;
	sta->ocp = tasta.ocp;
	sta->otp = tasta.otp;
	sta->ovp = tasta.ovp;

	return ret;
}

int adapter_is_support_pd_pps(void)
{
	if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
		return true;

	return false;
}

int adapter_get_cap(struct pd_cap *cap)
{
	struct adapter_power_cap tacap = {0};
	int i;

	adapter_dev_get_cap(pinfo->pd_adapter, MTK_PD, &tacap);

	cap->selected_cap_idx = tacap.selected_cap_idx;
	cap->nr = tacap.nr;

	for (i = 0; i < tacap.nr; i++) {
		cap->max_mv[i] = tacap.max_mv[i];
		cap->min_mv[i] = tacap.min_mv[i];
		cap->ma[i] = tacap.ma[i];
		cap->maxwatt[i] = tacap.maxwatt[i];
		cap->minwatt[i] = tacap.minwatt[i];
		cap->type[i] = tacap.type[i];
	}

	return 0;
}

int adapter_is_support_pd(void)
{
	if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK ||
		pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30)
		return true;

	if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO &&
		pinfo->enable_pe_4 == false &&
		pinfo->enable_pe_5 == false)
		return true;

	return false;
}

int set_charger_manager(struct charger_manager *info)
{
	if (pinfo == NULL)
		pinfo = info;

	return 0;
}

int enable_vbus_ovp(bool en)
{
	charger_enable_vbus_ovp(pinfo, en);

	return 0;
}

int wake_up_charger(void)
{
	if (pinfo != NULL)
		_wake_up_charger(pinfo);

	return 0;
}
