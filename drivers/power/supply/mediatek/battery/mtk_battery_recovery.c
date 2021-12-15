/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mtk_battery_recovery.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of the Anroid Battery service
 *   in recovery mode for updating the battery status.
 *
 * Author:
 * -------
 * Timo Liao
 *
 ****************************************************************************/
#ifndef _DEA_MODIFY_
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/v1/mtk_battery.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_rtc.h>
#else
#include <string.h>
#endif

#include <mtk_gauge_class.h>
#include "mtk_battery_internal.h"
#include "mtk_battery_recovery.h"

#define UNIT_TRANS_10   10
#define UNIT_TRANS_100  100
#define CAR_MIN_GAP 15

struct fuel_gauge_custom_data fgr_data, *pdata;
struct fuel_gauge_table_custom_data fgr_table, *ptable;
struct fgd_nl_msg_t st_nl_data;

int last_temp;
int T_table;
int T_table_c;

/* in recovery mode, soc only follows c_soc */
int soc;

/* tempeture related*/
int fg_bat_tmp_c_gap = 1;

/* CSOC related */
int fg_c_d0_ocv;
int fg_c_d0_dod;
int fg_c_d0_soc;
int fg_c_dod;
int fg_c_soc;
int fg_bat_int1_gap;
int prev_car_bat0;

/* UI related */
int rtc_ui_soc;
int ui_soc;
int ui_d0_soc;
int vboot;
int vboot_c;
int qmax_t_0ma; /* 0.1mA */
int qmax_t_0ma_tb1; /* 0.1mA */
int qmax_t_0ma_h;
int qmax_t_Nma_h;
int quse_tb0;
int quse_tb1;
int car;
int batterypseudo1_h;
int batterypseudo100;
int shutdown_hl_zcv;
int qmax_t_0ma_h_tb1;
int qmax_t_Nma_h_tb1;
int qmax_t_aging;
int aging_factor = 10000;
int fg_resistance_bat;
int DC_ratio = 100;
int ht_gap;
int lt_gap;
int low_tracking_enable;
int fg_vbat2_lt;
int fg_vbat2_ht;

/* Interrupt control */
int fg_bat_int2_ht_en;
int fg_bat_int2_lt_en;

/* receive interrupt from mtk_battery.c */
void wakeup_fg_algo_recovery(unsigned int intr_num)
{
	if (fg_interrupt_check() == false)
		return;

	switch (intr_num) {
	case FG_INTR_BAT_TMP_C_HT:
		fg_temp_c_int_handler();
		fg_int_end_flow(FG_INTR_BAT_TMP_HT);
		break;
	case FG_INTR_BAT_TMP_C_LT:
		fg_temp_c_int_handler();
		fg_int_end_flow(FG_INTR_BAT_TMP_LT);
		break;
	case FG_INTR_BAT_INT1_HT:
		fg_bat_int1_handler();
		fg_bat_int2_handler(0);
		fg_int_end_flow(FG_INTR_BAT_INT1_HT);
		break;
	case FG_INTR_BAT_INT1_LT:
		fg_bat_int1_handler();
		fg_bat_int2_handler(0);
		fg_int_end_flow(FG_INTR_BAT_INT1_LT);
		break;
	case FG_INTR_BAT_INT2_HT:
		fgr_bat_int2_h_handler();
		fg_bat_int1_handler();
		fg_int_end_flow(FG_INTR_BAT_INT2_HT);
		break;
	case FG_INTR_BAT_INT2_LT:
		fgr_bat_int2_l_handler();
		fg_bat_int1_handler();
		fg_int_end_flow(FG_INTR_BAT_INT2_LT);
		break;
	case FG_INTR_FG_TIME:
		fg_time_handler();
		fg_int_end_flow(FG_INTR_FG_TIME);
		break;
	case FG_INTR_SHUTDOWN:
		fgr_shutdown_int_handler();
		fg_int_end_flow(FG_INTR_SHUTDOWN);
		break;
	case FG_INTR_DLPT_SD:
		dlpt_sd_handler();
		fg_int_end_flow(FG_INTR_DLPT_SD);
		break;
	case FG_INTR_VBAT2_H:
		fgr_vbat2_h_int_handler();
		fg_int_end_flow(FG_INTR_VBAT2_H);
		break;
	case FG_INTR_VBAT2_L:
		fgr_vbat2_l_int_handler();
		fg_int_end_flow(FG_INTR_VBAT2_L);
		break;
	}

	bm_err("[%s] intr_num=0x%x\n",
		__func__,
		intr_num);
}

void fgr_SEND_to_kernel(int cmd, int *send_data, int *recive_data)
{
	memset(&st_nl_data, 0, sizeof(struct fgd_nl_msg_t));

/* bmd_ctrl_cmd_from_user(void *nl_data, struct fgd_nl_msg_t *ret_msg) */

	st_nl_data.fgd_cmd = cmd;

	memcpy(&st_nl_data.fgd_data[0], send_data, 4);
	bmd_ctrl_cmd_from_user((void *)&st_nl_data, &st_nl_data);
	memcpy(recive_data, &st_nl_data.fgd_data[0], 4);
}

void fg_update_quse(int caller)
{
	int aging_factor_cust = 0;

	/* caller = 1 means update c table */
	/* caller = 2 means update v table */

	if (caller == 1) {
		if (pdata->aging_sel == 1)
			quse_tb1 = qmax_t_0ma_tb1 * aging_factor_cust / 10000;
		else
			quse_tb1 = qmax_t_0ma_tb1 * aging_factor / 10000;
	} else {
		if (pdata->aging_sel == 1)
			quse_tb0 = qmax_t_0ma * aging_factor_cust / 10000;
		else
			quse_tb0 = qmax_t_0ma * aging_factor / 10000;
	}

	if (caller == 1) {
		bm_err("[%s]aging_sel %d qmax_t_0ma_tb1 %d quse_tb1 [%d] aging[%d]\n",
			__func__,
			pdata->aging_sel, qmax_t_0ma_tb1,
			quse_tb1, aging_factor);
	}
}

void fg_enable_fg_bat_int2_ht(int en)
{
	fg_bat_int2_ht_en = en;
	enable_fg_bat_int2_ht(en);
	bm_err("[%s] ht_en:%d ht_gap:%d\n",
		__func__,
		fg_bat_int2_ht_en, ht_gap);
}


void fg_enable_fg_bat_int2_lt(int en)
{
	fg_bat_int2_lt_en = en;
	enable_fg_bat_int2_lt(en);
	bm_err("[%s] lt_en:%d lt_gap:%d\n",
		__func__,
		fg_bat_int2_lt_en, lt_gap);
}

void fg_set_soc_by_vc_mode(void)
{
	soc = fg_c_soc;
}

/* update csoc ht/lt gap */
void fg_update_fg_bat_int1_threshold(void)
{
	fg_bat_int1_gap = quse_tb1 * pdata->diff_soc_setting / 10000;

	if (fg_bat_int1_gap < CAR_MIN_GAP)
		fg_bat_int1_gap = CAR_MIN_GAP;

	bm_err("[%s] quse_tb1:%d gap:%d diff_soc_setting:%d MIN:%d\n",
		__func__,
		quse_tb1, fg_bat_int1_gap,
		pdata->diff_soc_setting, CAR_MIN_GAP);
}

/* update uisoc ht/lt gap */
void fg_update_fg_bat_int2_threshold(void)
{
	int D_Remain = 0;

	car = get_fg_hw_car();
	fg_update_quse(1);

	/* calculate ui ht gap */
	ht_gap = quse_tb1 / 100;

	if (ht_gap < (quse_tb1 / 1000))
		ht_gap = quse_tb1 / 1000;

	if (ui_soc <= 100)
		ht_gap = quse_tb1 / 200;

	if (ht_gap < CAR_MIN_GAP)
		ht_gap = CAR_MIN_GAP;

	/* calculate ui lt_gap */
	D_Remain = soc * quse_tb1 / 10000;
	lt_gap = D_Remain * pdata->diff_soc_setting / ui_soc;

	if (lt_gap < (quse_tb1 / 1000))
		lt_gap = quse_tb1 / 1000;

	if (ui_soc <= 100)
		lt_gap = quse_tb1 / 200;

	if (lt_gap < CAR_MIN_GAP)
		lt_gap = CAR_MIN_GAP;

	bm_err(
		"[%s]car:%d quse_tb1[%d %d] gap[%d %d][%d]\n",
		__func__,
		car, quse_tb1, soc, ht_gap, lt_gap, D_Remain);
}


void fg_update_fg_bat_int2_ht(void)
{
	fg_update_fg_bat_int2_threshold();
	set_fg_bat_int2_ht_gap(ht_gap);
	bm_err("[%s] update ht_en:%d ht_gap:%d\n",
		__func__,
		fg_bat_int2_ht_en, ht_gap);

}

void fg_update_fg_bat_int2_lt(void)
{
	fg_update_fg_bat_int2_threshold();
	set_fg_bat_int2_lt_gap(lt_gap);
	bm_err("[%s] update lt_en:%d lt_gap:%d\n",
		__func__,
		fg_bat_int2_lt_en, lt_gap);
}

void fg_update_fg_bat_tmp_threshold_c(void)
{
	fg_bat_tmp_c_gap = 1;
}

/* Initial setting  for interrupt and gauge states*/
void fg_set_int1(void)
{
	int car_now = get_fg_hw_car();

	fg_update_quse(1);

	/* set c gap */
	fg_update_fg_bat_int1_threshold();
	set_fg_bat_int1_gap(fg_bat_int1_gap);
	bm_err(
		"[%s]set cgap :fg_bat_int1_gap %d to kernel done\n",
		__func__,
		fg_bat_int1_gap);

	/* set ui_soc gap*/
	prev_car_bat0 = car_now;
	fg_update_fg_bat_int2_ht();/* update ui_ht gap and setting gap */
	fg_update_fg_bat_int2_lt();

	fg_enable_fg_bat_int2_ht(1);
	fg_enable_fg_bat_int2_lt(1);

	/*set bat tempture  */
	fg_update_fg_bat_tmp_threshold_c();
	set_fg_bat_tmp_c_gap(fg_bat_tmp_c_gap);
	bm_err("[%s]fg_bat_tmp_c_gap %d\n",
		__func__,
		fg_bat_tmp_c_gap);

	fg_vbat2_lt = pdata->vbat2_det_voltage1;
	set_fg_vbat2_l_th(fg_vbat2_lt);
	enable_fg_vbat2_l_int(true);

	set_kernel_soc(soc);
	set_kernel_uisoc(ui_soc);
	set_kernel_init_vbat(get_ptim_vbat());

	set_rtc_ui_soc((ui_soc+50) / 100);

	if (soc <= 100)
		set_con0_soc(100);
	else if (soc >= 12000)
		set_con0_soc(10000);
	else
		set_con0_soc(soc);

	bm_err("[FGADC_intr_end][INTR_Initialize]%s done\n",
		__func__);

	/* fg_dump_int_threshold(); */
}

void fg_bat_int1_handler(void)
{
	fg_update_c_dod();
	fg_update_fg_bat_int1_threshold();
	set_fg_bat_int1_gap(fg_bat_int1_gap);
	soc = fg_c_soc;

	bm_err("[%s]soc %d\n",
		__func__,
		soc);

}

void fg_bat_int2_handler(int source)
{
	int _car = get_fg_hw_car();

	bm_err("[%s]car:%d pre_car:%d ht:%d lt:%d u_type:%d source:%d\n",
		__func__,
	_car, prev_car_bat0, ht_gap, lt_gap, pdata->uisoc_update_type, source);

	/* recovery mode dont care u_type */

	if (_car > prev_car_bat0)
		fgr_bat_int2_h_handler();
	else if (_car < prev_car_bat0)
		fgr_bat_int2_l_handler();

}

void fgr_bat_int2_h_handler(void)
{
	int ui_gap_ht = 0;
	/* int is_charger_exist = get_charger_exist(); */
	int _car = get_fg_hw_car();
	int delta_car_bat0 = abs(prev_car_bat0 - _car);

	fg_update_fg_bat_int2_threshold();
	ui_gap_ht = delta_car_bat0 * UNIT_TRANS_100 / ht_gap;

	bm_err("[fg_bat_int2_h_handler][IN]ui_soc %d, ht_gap:[%d %d], _car[%d %d %d]\n",
		ui_soc, ht_gap, ui_gap_ht, _car, prev_car_bat0, delta_car_bat0);

	if (ui_gap_ht > 100)
		ui_gap_ht = 100;

	if (ui_gap_ht > 0)
		prev_car_bat0 = _car;

	if (ui_soc >= 10000)
		ui_soc = 10000;
	else {
		if ((ui_soc + ui_gap_ht) >= 10000)
			ui_soc = 10000;
		else
			ui_soc = ui_soc + ui_gap_ht;
	}

	if (ui_soc >= 10000)
		ui_soc = 10000;

	fg_update_fg_bat_int2_ht();
	fg_update_fg_bat_int2_lt();

	bm_err("[fg_bat_int2_h_handler][OUT]ui_soc %d, ui_gap_ht:%d, _car[%d %d %d]\n",
		ui_soc, ui_gap_ht, _car, prev_car_bat0, delta_car_bat0);
}

void fg_time_handler(void)
{
	int is_charger_exist = get_charger_exist();

	bm_err("[%s][IN] chr:%d, low_tracking:%d ui_soc:%d\n",
		__func__,
		is_charger_exist, low_tracking_enable, ui_soc);

	if (low_tracking_enable) {
		if (is_charger_exist)
			return;

		if (is_charger_exist == false) {
			ui_soc = ui_soc - 100;
			if (ui_soc <= 0) {
				ui_soc = 0;
				low_tracking_enable = 0;
			}
		}
	} else
		set_fg_time(0);
}

void imix_error_calibration(void)
{
	int imix = 0;
	int iboot = 0;

	imix = get_imix_r();
	iboot = pdata->shutdown_system_iboot;

	if ((imix < iboot) && (imix > 0))
		fg_adc_reset();
}

void fgr_shutdown_int_handler(void)
{
	low_tracking_enable = 1;
	set_fg_time(pdata->discharge_tracking_time);
	imix_error_calibration();
}

void dlpt_sd_handler(void)
{
	ui_soc = 0;
	low_tracking_enable = 0;
	set_fg_time(0);
	fg_enable_fg_bat_int2_ht(false);
	fg_enable_fg_bat_int2_lt(false);
	set_kernel_uisoc(ui_soc);
	imix_error_calibration();
}

void fgr_vbat2_h_int_handler(void)
{
	enable_fg_vbat2_h_int(false);
	fg_vbat2_lt = pdata->vbat2_det_voltage1;
	set_fg_vbat2_l_th(fg_vbat2_lt);
	enable_fg_vbat2_l_int(true);
	bm_err("[%s]fg_vbat2_lt=%d %d\n",
		__func__,
		fg_vbat2_lt, fg_vbat2_ht);
}


void fgr_vbat2_l_int_handler(void)
{
	if (fg_vbat2_lt == pdata->vbat2_det_voltage1) {
		set_shutdown_cond(LOW_BAT_VOLT);
		fg_vbat2_lt = pdata->vbat2_det_voltage2;
		fg_vbat2_ht = pdata->vbat2_det_voltage3;
		set_fg_vbat2_l_th(fg_vbat2_lt);
		set_fg_vbat2_h_th(fg_vbat2_ht);
		enable_fg_vbat2_l_int(true);
		enable_fg_vbat2_h_int(true);
	}
	bm_err("[%s]fg_vbat2_lt=%d %d,[%d %d %d]\n",
		__func__,
		fg_vbat2_lt, fg_vbat2_ht,
		pdata->vbat2_det_voltage1,
		pdata->vbat2_det_voltage2,
		pdata->vbat2_det_voltage3);
}

void fgr_bat_int2_l_handler(void)
{
	int ui_gap_lt = 0;
	int is_charger_exist = get_charger_exist();
	int _car = get_fg_hw_car();
	int delta_car_bat0 = abs(prev_car_bat0 - _car);

	fg_update_fg_bat_int2_threshold();

	if (ui_soc > soc && soc >= 100) {
		ui_gap_lt = delta_car_bat0 * UNIT_TRANS_100 / lt_gap;
		ui_gap_lt = ui_gap_lt * ui_soc / soc;
	} else
		ui_gap_lt = delta_car_bat0 * UNIT_TRANS_100 / lt_gap;

	bm_err("[%s][IN]ui_soc %d, lt_gap[%d %d] _car[%d %d %d]\n",
		__func__,
		ui_soc, lt_gap, ui_gap_lt, _car, prev_car_bat0, delta_car_bat0);

	if (ui_gap_lt > 100)
		ui_gap_lt = 100;

	if (ui_gap_lt < 0) {
		bm_err(
			"[FG_ERR][%s] ui_gap_lt %d should not less than 0\n",
			__func__,
			ui_gap_lt);
		ui_gap_lt = 0;
	}

	if (ui_gap_lt > 0)
		prev_car_bat0 = _car;

	if (is_charger_exist == true) {
		ui_soc = ui_soc - ui_gap_lt;
	} else {
		if (ui_soc <= 100) {
			if (ui_soc == 0)
				ui_soc = 0;
			else
				ui_soc = 100;
		} else {
			if ((ui_soc - ui_gap_lt) < 100)
				ui_soc = 100;
			else
				ui_soc = ui_soc - ui_gap_lt;

			if (ui_soc < 100)
				ui_soc = 100;
		}
	}

	fg_update_fg_bat_int2_ht();
	fg_update_fg_bat_int2_lt();

	bm_err("[%s][OUT]ui_soc %d, ui_gap_lt:%d, _car[%d %d %d]\n",
		__func__,
		ui_soc, ui_gap_lt, _car, prev_car_bat0, delta_car_bat0);
}

void fg_error_calibration2(int intr_no)
{
	int shutdown_cond = get_shutdown_cond();

	if (shutdown_cond != 1)
		low_tracking_enable = false;
}


void fg_int_end_flow(unsigned int intr_no)
{
	int curr_temp, vbat;
	char intr_name[32];

	switch (intr_no) {
	case FG_INTR_0:
		sprintf(intr_name, "FG_INTR_INIT");
		break;

	case FG_INTR_TIMER_UPDATE:
		sprintf(intr_name, "FG_INTR_TIMER_UPDATE");
		break;

	case FG_INTR_BAT_CYCLE:
		sprintf(intr_name, "FG_INTR_BAT_CYCLE");
		break;

	case FG_INTR_CHARGER_OUT:
		sprintf(intr_name, "FG_INTR_CHARGER_OUT");
		break;

	case FG_INTR_CHARGER_IN:
		sprintf(intr_name, "FG_INTR_CHARGER_IN");
		break;

	case FG_INTR_FG_TIME:
		sprintf(intr_name, "FG_INTR_FG_TIME");
		break;

	case FG_INTR_BAT_INT1_HT:
		sprintf(intr_name, "FG_INTR_COULOMB_HT");
		break;

	case FG_INTR_BAT_INT1_LT:
		sprintf(intr_name, "FG_INTR_COULOMB_LT");
		break;

	case FG_INTR_BAT_INT2_HT:
		sprintf(intr_name, "FG_INTR_UISOC_HT");
		break;

	case FG_INTR_BAT_INT2_LT:
		sprintf(intr_name, "FG_INTR_UISOC_LT");
		break;

	case FG_INTR_BAT_TMP_HT:
		sprintf(intr_name, "FG_INTR_BAT_TEMP_HT");
		break;

	case FG_INTR_BAT_TMP_LT:
		sprintf(intr_name, "FG_INTR_BAT_TEMP_LT");
		break;

	case FG_INTR_BAT_TIME_INT:
		sprintf(intr_name, "FG_INTR_BAT_TIME_INT");
		break;

	case FG_INTR_NAG_C_DLTV:
		sprintf(intr_name, "FG_INTR_NAFG_VOLTAGE");
		break;

	case FG_INTR_FG_ZCV:
		sprintf(intr_name, "FG_INTR_FG_ZCV");
		break;

	case FG_INTR_SHUTDOWN:
		sprintf(intr_name, "FG_INTR_SHUTDOWN");
		break;

	case FG_INTR_RESET_NVRAM:
		sprintf(intr_name, "FG_INTR_RESET_NVRAM");
		break;

	case FG_INTR_BAT_PLUGOUT:
		sprintf(intr_name, "FG_INTR_BAT_PLUGOUT");
		break;

	case FG_INTR_IAVG:
		sprintf(intr_name, "FG_INTR_IAVG");
		break;

	case FG_INTR_VBAT2_L:
		sprintf(intr_name, "FG_INTR_VBAT2_L");
		break;

	case FG_INTR_VBAT2_H:
		sprintf(intr_name, "FG_INTR_VBAT2_H");
		break;

	case FG_INTR_CHR_FULL:
		sprintf(intr_name, "FG_INTR_CHR_FULL");
		break;

	case FG_INTR_DLPT_SD:
		sprintf(intr_name, "FG_INTR_DLPT_SD");
		break;

	case FG_INTR_BAT_TMP_C_HT:
		sprintf(intr_name, "FG_INTR_BAT_TMP_C_HT");
		break;

	case FG_INTR_BAT_TMP_C_LT:
		sprintf(intr_name, "FG_INTR_BAT_TMP_C_LT");
		break;

	case FG_INTR_BAT_INT1_CHECK:
		sprintf(intr_name, "FG_INTR_COULOMB_C");
		break;

	default:
		sprintf(intr_name, "FG_INTR_UNKNOWN");
		bm_err("[Intr_Number_to_Name] unknown intr %d\n", intr_no);
		break;
	}

	car = get_fg_hw_car();
	get_hw_info(intr_no);
	vbat = get_vbat();
	curr_temp = force_get_tbat(true);

	set_kernel_soc(soc);
	set_kernel_uisoc(ui_soc);
	set_rtc_ui_soc((ui_soc+50) / 100);

	if (soc <= 100)
		set_con0_soc(100);
	else if (soc >= 10000)
		set_con0_soc(10000);
	else
		set_con0_soc(soc);

	fg_error_calibration2(intr_no);

	bm_err("[FGADC_intr_end][%s]soc:%d, c_soc:%d ui_soc:%d VBAT %d T:[%d C:%d] car:%d\n",
		intr_name, soc, fg_c_soc,
		ui_soc, vbat, curr_temp,
		T_table_c, car);
}

void fg_temp_c_int_handler(void)
{
	/* int curr_temp; */
	fg_construct_table_by_temp(true, ptable->temperature_tb1);
	fg_construct_vboot(ptable->temperature_tb1);
	/* fg_debug_dump(ptable->temperature_tb1);*/
	fg_update_c_dod();

	fg_set_soc_by_vc_mode();
	fg_update_fg_bat_tmp_threshold_c();
	set_fg_bat_tmp_c_gap(fg_bat_tmp_c_gap);
}

void fgr_set_cust_data(void)
{
	pdata = &fg_cust_data;
	ptable = &fg_table_cust_data;
}

int fg_get_saddles(void)
{
	return ptable->fg_profile[0].size;
}

struct FUELGAUGE_PROFILE_STRUCT *fg_get_profile(int temperature)
{
	int i;

	for (i = 0; i < ptable->active_table_number; i++)
		if (ptable->fg_profile[i].temperature == temperature)
			return &ptable->fg_profile[i].fg_profile[0];

	if (ptable->temperature_tb0 == temperature)
		return &ptable->fg_profile_temperature_0[0];

	if (ptable->temperature_tb1 == temperature)
		return &ptable->fg_profile_temperature_1[0];

	bm_err(
		"%s: no table for %d\n",
		__func__,
		temperature);

	return NULL;
}


int fg_check_temperature_order(int *is_ascending, int *is_descending)
{
	int i;

	*is_ascending = 0;
	*is_descending = 0;
	/* is ascending*/

	bm_err("act:%d table: %d %d %d %d %d %d %d %d %d %d\n",
		ptable->active_table_number,
		ptable->fg_profile[0].temperature,
		ptable->fg_profile[1].temperature,
		ptable->fg_profile[2].temperature,
		ptable->fg_profile[3].temperature,
		ptable->fg_profile[4].temperature,
		ptable->fg_profile[5].temperature,
		ptable->fg_profile[6].temperature,
		ptable->fg_profile[7].temperature,
		ptable->fg_profile[8].temperature,
		ptable->fg_profile[9].temperature);

	for (i = 0; i < ptable->active_table_number - 1; i++) {
		if (ptable->fg_profile[i].temperature >
			ptable->fg_profile[i + 1].temperature)
			break;
		*is_ascending = 1;
		*is_descending = 0;
	}

	/* is descending*/
	for (i = 0; i < ptable->active_table_number - 1; i++) {
		if (ptable->fg_profile[i].temperature <
			ptable->fg_profile[i + 1].temperature)
			break;
		*is_ascending = 0;
		*is_descending = 1;

	}

	bm_err("active_table_no is %d, %d %d\n",
		ptable->active_table_number,
		*is_ascending,
		*is_descending);
	for (i = 0; i < ptable->active_table_number; i++) {
		bm_err("table[%d]:%d\n",
			i,
			ptable->fg_profile[i].temperature);
	}

	if (*is_ascending == 0 && *is_descending == 0)
		return -1;

	return 0;
}


void fgr_construct_battery_profile(int table_idx)
{
	struct FUELGAUGE_PROFILE_STRUCT *low_profile_p = NULL;
	struct FUELGAUGE_PROFILE_STRUCT *high_profile_p = NULL;
	struct FUELGAUGE_PROFILE_STRUCT *temp_profile_p = NULL;
	int low_temp = 0, high_temp = 0, temperature = 0;
	int i, saddles;
	int low_pseudo1 = 0, high_pseudo1 = 0;
	int low_pseudo100 = 0, high_pseudo100 = 0;
	int low_qmax = 0, high_qmax = 0, low_qmax_h = 0, high_qmax_h = 0;
	int low_shutdown_zcv = 0, high_shutdown_zcv = 0;
	int is_ascending, is_descending;

	temperature = last_temp;
	temp_profile_p = fg_get_profile(table_idx);

	if (temp_profile_p == NULL) {
		bm_err(
			"[FGADC] fg_get_profile : create table fail !\n");
		return;
	}

	if (fg_check_temperature_order(&is_ascending, &is_descending)) {
		bm_err(
			"[FGADC] fg_check_temperature_order : t0~t3 setting error !\n");
		return;
	}

	for (i = 1; i < ptable->active_table_number; i++) {
		if (is_ascending) {
			if (temperature <= ptable->fg_profile[i].temperature)
				break;
		} else {
			if (temperature >= ptable->fg_profile[i].temperature)
				break;
		}
	}

	if (i > (ptable->active_table_number - 1))
		i = ptable->active_table_number - 1;

	if (is_ascending) {
		low_profile_p =
			fg_get_profile(
			ptable->fg_profile[i - 1].temperature);
		high_profile_p =
			fg_get_profile(
			ptable->fg_profile[i].temperature);
		low_temp =
			ptable->fg_profile[i - 1].temperature;
		high_temp =
			ptable->fg_profile[i].temperature;
		low_pseudo1 =
			ptable->fg_profile[i - 1].pseudo1;
		high_pseudo1 =
			ptable->fg_profile[i].pseudo1;
		low_pseudo100 =
			ptable->fg_profile[i - 1].pseudo100;
		high_pseudo100 =
			ptable->fg_profile[i].pseudo100;
		low_qmax = ptable->fg_profile[i - 1].q_max;
		high_qmax = ptable->fg_profile[i].q_max;
		low_qmax_h =
			ptable->fg_profile[i - 1].q_max_h_current;
		high_qmax_h =
			ptable->fg_profile[i].q_max_h_current;
		low_shutdown_zcv =
			ptable->fg_profile[i - 1].shutdown_hl_zcv;
		high_shutdown_zcv =
			ptable->fg_profile[i].shutdown_hl_zcv;
	} else {
		low_profile_p =
			fg_get_profile(ptable->fg_profile[i].temperature);
		high_profile_p =
			fg_get_profile(ptable->fg_profile[i - 1].temperature);
		low_temp = ptable->fg_profile[i].temperature;
		high_temp = ptable->fg_profile[i - 1].temperature;
		low_pseudo1 = ptable->fg_profile[i].pseudo1;
		high_pseudo1 = ptable->fg_profile[i - 1].pseudo1;
		low_pseudo100 = ptable->fg_profile[i].pseudo100;
		high_pseudo100 = ptable->fg_profile[i - 1].pseudo100;
		low_qmax = ptable->fg_profile[i].q_max;
		high_qmax = ptable->fg_profile[i - 1].q_max;
		low_qmax_h = ptable->fg_profile[i].q_max_h_current;
		high_qmax_h = ptable->fg_profile[i - 1].q_max_h_current;
		low_shutdown_zcv = ptable->fg_profile[i].shutdown_hl_zcv;
		high_shutdown_zcv = ptable->fg_profile[i - 1].shutdown_hl_zcv;
	}
	if (temperature < low_temp)
		temperature = low_temp;
	else if (temperature > high_temp)
		temperature = high_temp;


	if (table_idx == 255)
		T_table = temperature;
	if (table_idx == 254)
		T_table_c = temperature;

	saddles = fg_get_saddles();

	for (i = 0; i < saddles; i++) {
		temp_profile_p[i].mah =
		interpolation(low_temp, low_profile_p[i].mah,
		high_temp, high_profile_p[i].mah, temperature);
		temp_profile_p[i].voltage =
		interpolation(low_temp, low_profile_p[i].voltage,
		high_temp, high_profile_p[i].voltage, temperature);
		temp_profile_p[i].resistance =
		interpolation(low_temp, low_profile_p[i].resistance,
		high_temp, high_profile_p[i].resistance, temperature);
		temp_profile_p[i].charge_r.rdc[0] =
		interpolation(low_temp, low_profile_p[i].charge_r.rdc[0],
		high_temp, high_profile_p[i].charge_r.rdc[0], temperature);

	}

	if (table_idx == ptable->temperature_tb0) {
		if (pdata->pseudo1_en == true)
			batterypseudo1_h = interpolation(
			low_temp,
			low_pseudo1,
			high_temp,
			high_pseudo1,
			temperature);

		if (pdata->pseudo100_en == true)
			batterypseudo100 = interpolation(
			low_temp,
			low_pseudo100,
			high_temp,
			high_pseudo100,
			temperature);

		bm_trace(
			"[Profile_Table]pseudo1_en:[%d] lowT %d %d %d lowPs1 %d highPs1 %d batterypseudo1_h [%d]\n",
			pdata->pseudo1_en, low_temp,
			high_temp, temperature,
			low_pseudo1, high_pseudo1,
			batterypseudo1_h);
		bm_trace(
			"[Profile_Table]pseudo100_en:[%d] %d lowT %d %d %d low100 %d %d [%d]\n",
			pdata->pseudo100_en, pdata->pseudo100_en_dis,
			low_temp, high_temp, temperature,
			low_pseudo100, high_pseudo100,
			batterypseudo100);

/*
 *	low_qmax and High_qmax need to do
 *	UNIT_TRANS_10 from "1 mAHR" to "0.1 mAHR"
 */
		qmax_t_0ma_h = interpolation(
			low_temp, UNIT_TRANS_10 * low_qmax,
			high_temp, UNIT_TRANS_10 * high_qmax,
			temperature);
		qmax_t_Nma_h = interpolation(
			low_temp, UNIT_TRANS_10 * low_qmax_h,
			high_temp, UNIT_TRANS_10 * high_qmax_h,
			temperature);

		bm_trace(
			"[Profile_Table]lowT %d %d %d lowQ %d %d qmax_t_0ma_h [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_qmax,
			UNIT_TRANS_10 * high_qmax,
			qmax_t_0ma_h);
		bm_trace(
			"[Profile_Table]lowT %d %d %d lowQh %d %d qmax_t_Nma_h [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_qmax_h,
			UNIT_TRANS_10 * high_qmax_h,
			qmax_t_Nma_h);

		shutdown_hl_zcv = interpolation(
			low_temp, UNIT_TRANS_10 * low_shutdown_zcv,
			high_temp, UNIT_TRANS_10 * high_shutdown_zcv,
			temperature);

		bm_trace(
			"[Profile_Table]lowT %d %d %d LowShutZCV %d HighShutZCV %d shutdown_hl_zcv [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_shutdown_zcv,
			UNIT_TRANS_10 * high_shutdown_zcv,
			shutdown_hl_zcv);

	} else if (table_idx == ptable->temperature_tb1) {
/*
 *	low_qmax and High_qmax need to do
 *	UNIT_TRANS_10 from "1 mAHR" to "0.1 mAHR"
 */
		qmax_t_0ma_h_tb1 = interpolation(
		low_temp, UNIT_TRANS_10 * low_qmax,
			high_temp, UNIT_TRANS_10 * high_qmax,
			temperature);
		qmax_t_Nma_h_tb1 = interpolation(
			low_temp, UNIT_TRANS_10 * low_qmax_h,
			high_temp, UNIT_TRANS_10 * high_qmax_h,
			temperature);

		bm_trace(
			"[Profile_Table]lowT %d %d %d lowQ %d %d qmax_t_0ma_h [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_qmax,
			UNIT_TRANS_10 * high_qmax,
			qmax_t_0ma_h_tb1);
		bm_trace(
			"[Profile_Table]lowT %d %d %d lowQh %d %d qmax_t_Nma_h [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_qmax_h,
			UNIT_TRANS_10 * high_qmax_h,
			qmax_t_Nma_h_tb1);
	}

	bm_trace(
		"[Profile_Table]T_table %d T_table_c %d %d %d is_ascend %d %d\n",
		T_table, T_table_c, pdata->pseudo1_en,
		pdata->pseudo100_en, is_ascending, is_descending);
	bm_trace(
		"[Profile_Table]Pseudo1_h %d %d, Qmax_T_0mA_H %d,%d qmax_t_0ma_h_tb1 %d %d\n",
		batterypseudo1_h, batterypseudo100, qmax_t_0ma_h,
		qmax_t_Nma_h, qmax_t_0ma_h_tb1, qmax_t_Nma_h_tb1);
}

void fg_construct_table_by_temp(bool update, int table_idx)
{
	int fg_temp;

	fg_temp = force_get_tbat(true);
	if (fg_temp != last_temp || update == true) {
		bm_trace(
			"[construct_table_by_temp] tempture from(%d)to(%d) Tb:%d",
			last_temp, fg_temp, table_idx);
		last_temp = fg_temp;
		fgr_construct_battery_profile(table_idx);
	}
}


void set_fg_bat_int1_gap(int gap)
{
	int sends = gap;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_FG_BAT_INT1_GAP,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_FG_BAT_INT1_GAP %d %d\n",
		sends, receive);
}

void set_fg_bat_int2_ht_gap(int gap)
{
	int sends = gap;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP %d %d\n",
		sends, receive);
}


void set_fg_bat_int2_lt_gap(int gap)
{
	int sends = gap;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_FG_BAT_INT2_LT_GAP,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_FG_BAT_INT2_LT_GAP %d %d\n",
		sends, receive);
}

void enable_fg_bat_int2_ht(int en)
{
	int sends = en;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT %d %d\n",
		sends, receive);
}

void enable_fg_bat_int2_lt(int en)
{
	int sends = en;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_LT,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_LT %d %d\n",
		sends, receive);
}

int set_kernel_soc(int _soc)
{
	int sends = _soc;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_KERNEL_SOC, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_KERNEL_SOC %d %d\n",
		sends, receive);
	return receive;
}

int set_kernel_uisoc(int _uisoc)
{
	int sends = _uisoc;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_KERNEL_UISOC, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_KERNEL_UISOC %d %d\n",
		sends, receive);
	return receive;
}


int set_kernel_init_vbat(int _vbat)
{
	int sends = _vbat;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_KERNEL_INIT_VBAT,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_KERNEL_INIT_VBAT %d %d\n",
		sends, receive);
	return receive;
}

void set_fg_bat_tmp_c_gap(int tmp)
{
	int sends = tmp;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_FG_BAT_TMP_C_GAP,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_FG_BAT_TMP_C_GAP %d %d\n",
		sends, receive);
}

void set_init_flow_done(int flag)
{
	int sends = flag;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_INIT_FLAG, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_INIT_FLAG %d %d\n",
		sends, receive);
}

void set_rtc_ui_soc(int rtc_ui_soc)
{
	int sends = rtc_ui_soc;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_RTC_UI_SOC, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_RTC_UI_SOC %d %d\n",
		sends, receive);
}

void set_fg_time(int _time)
{
	int sends = _time;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_FG_TIME, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_FG_TIME %d %d\n",
		sends, receive);
}

void set_con0_soc(int _soc)
{
	int sends = _soc;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_CON0_SOC, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_CON0_SOC %d %d\n",
		sends, receive);
}

int get_con0_soc(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_CON0_SOC, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_CON0_SOC:%d %d\n",
		sends, receive);
	return receive;
}

void set_nvram_fail_status(int flag)
{
	int sends = flag;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_NVRAM_FAIL_STATUS,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_NVRAM_FAIL_STATUS %d %d\n",
		sends, receive);
}

void enable_fg_vbat2_h_int(int en)
{
	int sends = en;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_ENABLE_FG_VBAT_H_INT,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_ENABLE_FG_VBAT_H_INT %d %d\n",
		sends, receive);
}

void enable_fg_vbat2_l_int(int en)
{
	int sends = en;
	int receive = 0;

	fgr_SEND_to_kernel(
		FG_DAEMON_CMD_ENABLE_FG_VBAT_L_INT,
		&sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_ENABLE_FG_VBAT_L_INT %d %d\n",
		sends, receive);
}

void set_fg_vbat2_h_th(int thr)
{
	int sends = thr;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_FG_VBAT_H_TH, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_FG_VBAT_H_TH %d %d\n",
		sends, receive);
}

void set_fg_vbat2_l_th(int thr)
{
	int sends = thr;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_SET_FG_VBAT_L_TH, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_SET_FG_VBAT_L_TH %d %d\n",
		sends, receive);
}

int get_d0_c_soc_cust(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_D0_C_SOC_CUST, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_D0_C_SOC_CUST:%d %d\n",
		sends, receive);
	return receive;
}

int get_uisoc_cust(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_UISOC_CUST, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_UISOC_CUST:%d %d\n",
		sends, receive);
	return receive;
}

int get_fg_hw_car(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_FG_HW_CAR, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_FG_HW_CAR:%d %d\n",
		sends, receive);
	return receive;
}

int get_rtc_ui_soc(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_RTC_UI_SOC, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_RTC_UI_SOC:%d %d\n",
		sends, receive);
	return receive;
}

int get_ptimrac(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_RAC, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_RAC:%d %d\n", sends, receive);
	return receive;
}

int get_ptim_vbat(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_PTIM_VBAT, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_PTIM_VBAT:%d %d\n",
		sends, receive);
	return receive;
}

int get_ptim_i(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_PTIM_I, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_PTIM_I:%d %d\n",
		sends, receive);
	return receive;
}

int get_hw_info(int intr_no)
{
	int sends = intr_no;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_HW_INFO, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_HW_INFO:%d %d\n",
		sends, receive);
	return receive;
}

unsigned int get_vbat(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_VBAT, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_VBAT:%d %d\n",
		sends, receive);
	return receive;
}

int get_charger_exist(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_IS_CHARGER_EXIST, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_IS_CHARGER_EXIST:%d %d\n",
		sends, receive);
	return receive;
}

int get_charger_status(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_CHARGER_STATUS, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_CHARGER_STATUS:%d %d\n",
		sends, receive);
	return receive;
}

int get_imix_r(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_GET_IMIX, &sends, &receive);
	bm_err("send_to_kernel=FG_DAEMON_CMD_GET_IMIX:%d %d\n", sends, receive);
	return receive;
}

void fg_construct_battery_profile_by_qmax(int qmax, int table_index)
{
	int i;
	struct FUELGAUGE_PROFILE_STRUCT *profile_p;

	profile_p = fg_get_profile(table_index);

	if (table_index == ptable->temperature_tb0) {
		qmax_t_0ma = qmax;

		for (i = 0; i < 100; i++)
			profile_p[i].percentage =
			profile_p[i].mah * 10000 / qmax_t_0ma;
	} else if (table_index == ptable->temperature_tb1) {
		qmax_t_0ma_tb1 = qmax;
		for (i = 0; i < 100; i++)
			profile_p[i].percentage =
			profile_p[i].mah * 10000 / qmax_t_0ma_tb1;
	}

	bm_err("[%s] qmax:%d qmax_t_0ma:%d\n",
		__func__,
		qmax, qmax_t_0ma);
}

void fg_construct_battery_profile_by_vboot(int _vboot, int table_index)
{
	int i, j;
	struct FUELGAUGE_PROFILE_STRUCT *profile_p;

	profile_p = fg_get_profile(table_index);

	for (j = 0; j < 100; j++)
		if (profile_p[j].voltage < _vboot)
			break;

	if (table_index == ptable->temperature_tb0) {
		if (j == 0) {
			qmax_t_0ma = profile_p[0].mah;
		} else if (j >= 100) {
			qmax_t_0ma = profile_p[99].mah;
		} else {
			/*qmax_t_0ma = profile_p[j].mah;*/
			qmax_t_0ma = interpolation(
				profile_p[j].voltage,
				profile_p[j].mah,
				profile_p[j-1].voltage,
				profile_p[j-1].mah,
				_vboot);
		}

		if (qmax_t_0ma < 3000) {
			bm_err("[FG_ERR][%s]index %d idx:%d _vboot:%d %d qmax_t_0ma:[%d => 3000]\n",
				__func__,
				table_index, j,
				_vboot, profile_p[j].voltage, qmax_t_0ma);
		}

		if (qmax_t_0ma > 50000) {
			bm_err("[FG_ERR][%s]index %d idx:%d _vboot:%d %d qmax_t_0ma:[%d => 50000]\n",
				__func__,
				table_index, j,
				_vboot, profile_p[j].voltage, qmax_t_0ma);
		}

		for (i = 0; i < 100; i++)
			profile_p[i].percentage =
			profile_p[i].mah * 10000 / qmax_t_0ma;

	} else if (table_index == ptable->temperature_tb1) {
		if (j == 0) {
			qmax_t_0ma_tb1 = profile_p[0].mah;
		} else if (j >= 100) {
			qmax_t_0ma_tb1 = profile_p[99].mah;
		} else {
			/*qmax_t_0ma = profile_p[j].mah;*/
			qmax_t_0ma_tb1 =
			interpolation(
			profile_p[j].voltage,
			profile_p[j].mah,
			profile_p[j-1].voltage,
			profile_p[j-1].mah, _vboot);
		}

		if (qmax_t_0ma_tb1 < 3000) {
			bm_err("[FG_ERR][%s]index %d idx:%d _vboot:%d %d qmax_t_0ma_tb1:[%d => 3000]\n",
				__func__,
				table_index, j,
				_vboot, profile_p[j].voltage,
				qmax_t_0ma_tb1);
		}

		if (qmax_t_0ma_tb1 > 50000) {
			bm_err("[FG_ERR][%s]index %d idx:%d _vboot:%d %d qmax_t_0ma_tb1:[%d => 50000]\n",
				__func__,
				table_index, j,
				_vboot, profile_p[j].voltage,
				qmax_t_0ma_tb1);
		}

		for (i = 0; i < 100; i++)
			profile_p[i].percentage =
			profile_p[i].mah * 10000 / qmax_t_0ma_tb1;
	}

	if (table_index == ptable->temperature_tb1) {
		bm_err("[%s]index %d idx:%d _vboot:%d %d qmax_t_0ma_tb1:%d\n",
			__func__,
			table_index, j, _vboot,
			profile_p[j].voltage, qmax_t_0ma_tb1);
	} else {
		bm_err("[%s]index %d idx:%d _vboot:%d %d qmax_t_0ma:%d\n",
			__func__,
			table_index, j, _vboot,
			profile_p[j].voltage, qmax_t_0ma);
	}
}

static int fg_compensate_battery_voltage_from_low(
	int oriv, int curr, int tablei)
{
	int fg_volt, fg_volt_withIR, ret_compensate_value = 0;
	int hit_h_percent = 0, hit_l_percent = 0;
	struct FUELGAUGE_PROFILE_STRUCT *profile_p;
	int i = 0, size, high = 0;

	profile_p = fg_get_profile(tablei);
	if (profile_p == NULL) {
		bm_err("[FG_ERR][%s] fail ,profile_p=null!\n",
			__func__);
		return 0;
	}
	size = fg_get_saddles();

	bm_err("[%s]size:%d oriv=%d I:%d\n",
		__func__,
		size, oriv, curr);

	for (; size > 0; size--) {
		high = size-1;
		if (high >= 1) {
			if (profile_p[high-1].percentage < 10000) {
				bm_err("[%s]find high=%d,[%d][%d]\n",
					__func__,
					high, profile_p[high].percentage,
					profile_p[high-1].percentage);
				break;
			}
		}
	}

	for (; high > 0; high--) {
		if (high >= 1) {
			fg_volt = profile_p[high-1].voltage;
			fg_resistance_bat =  profile_p[high-1].resistance;
			ret_compensate_value =
				(curr * (fg_resistance_bat * DC_ratio / 100 +
				pdata->r_fg_value + pdata->fg_meter_resistance))
				/ 1000;
			ret_compensate_value = (ret_compensate_value + 5) / 10;
			fg_volt_withIR = fg_volt + ret_compensate_value;
			if (fg_volt_withIR > oriv) {
				hit_h_percent = profile_p[high].percentage;
				hit_l_percent = profile_p[high-1].percentage;
				bm_err(
					"[%s] h_percent=[%d,%d],high=%d,fg_volt_withIR=%d > oriv=%d\n",
					__func__,
					hit_h_percent, hit_l_percent,
					high, fg_volt_withIR, oriv);
				break;
			}
		} else {
			bm_err("[FG_ERR][%s] can't find available voltage!!!\n",
				__func__);
			fg_volt = profile_p[0].voltage;
		}
	}

	/* check V+IR > orig_v  every 0.1% */
	for (i = hit_h_percent; i >= hit_l_percent; i = i-10) {
		fg_volt = interpolation(
			profile_p[high-1].percentage,
			profile_p[high-1].voltage,
			profile_p[high].percentage,
			profile_p[high].voltage, i);

		fg_resistance_bat = interpolation(
			profile_p[high-1].percentage,
			profile_p[high-1].resistance,
			profile_p[high].percentage,
			profile_p[high].resistance, i);

		ret_compensate_value =
			(curr * (fg_resistance_bat * DC_ratio / 100 +
			pdata->r_fg_value + pdata->fg_meter_resistance))
			/ 1000;
		ret_compensate_value =
			(ret_compensate_value + 5) / 10;
		fg_volt_withIR = fg_volt + ret_compensate_value;

		if (fg_volt_withIR > oriv) {
			bm_err("[%s]fg_volt=%d,%d,IR=%d,orig_v:%d,+IR=%d,percent=%d,\n",
				__func__,
				fg_volt, high,
				ret_compensate_value, oriv,
				fg_volt_withIR, i);
			return fg_volt;
		}
	}

	bm_err("[FG_ERR][%s] should not reach here!!!!!!\n",
		__func__);
	return fg_volt;
}


void fg_construct_vboot(int table_idx)
{
	int iboot = 0;
	int rac = get_ptimrac();
	int ptim_vbat = get_ptim_vbat();
	int ptim_i = get_ptim_i();
	int vboot_t = 0;
	int curr_temp = force_get_tbat(1);

	bm_err("[%s] idx %d T_NEW %d T_table %d T_table_c %d qmax_sel %d\n",
		__func__,
		table_idx, curr_temp, T_table, T_table_c, pdata->qmax_sel);

	if (pdata->iboot_sel == 0)
		iboot = ptable->fg_profile[0].pon_iboot;
	else
		iboot = pdata->shutdown_system_iboot;

	if (pdata->qmax_sel == 0) {
		vboot =
			ptable->fg_profile[0].pmic_min_vol
			+ iboot * rac / 10000;
		if (table_idx == ptable->temperature_tb0)
			fg_construct_battery_profile_by_qmax(
			qmax_t_0ma_h, table_idx);
		if (table_idx == ptable->temperature_tb1)
			fg_construct_battery_profile_by_qmax(
			qmax_t_0ma_h_tb1, table_idx);
	} else if (pdata->qmax_sel == 1) {
		vboot_t =
			ptable->fg_profile[0].pmic_min_vol
			+ iboot * rac / 10000;

		fg_construct_battery_profile_by_vboot(vboot_t, table_idx);
		if (table_idx == 255) {
			vboot =
				fg_compensate_battery_voltage_from_low(
				ptable->fg_profile[0].pmic_min_vol,
				(0 - iboot), table_idx);
			fg_construct_battery_profile_by_vboot(
				vboot, table_idx);
		} else if (table_idx == 254) {
			vboot_c =
				fg_compensate_battery_voltage_from_low(
				ptable->fg_profile[0].pmic_min_vol,
				(0 - iboot), table_idx);
			fg_construct_battery_profile_by_vboot(
				vboot_c, table_idx);
		}
		bm_err("[%s]idx %d T_NEW %d T_table %d T_table_c %d qmax_sel %d vboot_t=[%d:%d:%d] %d %d rac %d\n",
			__func__,
			table_idx, curr_temp,
			T_table, T_table_c,
			pdata->qmax_sel, vboot_t,
			vboot, vboot_c,
			ptable->fg_profile[0].pmic_min_vol,
			iboot, rac);
	}

/* batterypseudo1_auto = get_batterypseudo1_auto(vboot, shutdown_hl_zcv); */

	if (qmax_t_aging == 9999999 || aging_factor > 10000)
		aging_factor = 10000;

	bm_err(
		"[%s] qmax_sel=%d iboot_sel=%d iboot:%d vbat:%d i:%d vboot:%d %d %d\n",
		__func__,
		pdata->qmax_sel, pdata->iboot_sel, iboot,
		ptim_vbat, ptim_i, vboot, vboot_c, vboot_t);

	if (pdata->qmax_sel == 0) {
		bm_err(
			"[%s][by_qmax]qmax_sel %d qmax %d vboot %d %d pmic_min_vol %d iboot %d r %d\n",
			__func__,
			pdata->qmax_sel, qmax_t_0ma_h,
			vboot, vboot_c,
			ptable->fg_profile[0].pmic_min_vol,
			iboot, rac);
	}
	if (pdata->qmax_sel == 1) {
		bm_err(
			"[%s][by_vboot]qmax_sel %d vboot_t %d vboot %d %d pmic_min_vol %d iboot %d rac %d\n",
			__func__,
			pdata->qmax_sel, vboot_t, vboot,
			vboot_c,
			ptable->fg_profile[0].pmic_min_vol,
			iboot, rac);
	}
}



void fgr_dump_table(int idx)
{
	struct FUELGAUGE_PROFILE_STRUCT *profile_p;
	int i;

	profile_p = fg_get_profile(idx);

	bm_err(
		"[fg_dump_table]table idx:%d (i,mah,voltage,resistance,percentage)\n",
		idx);
	for (i = 0; i < fg_get_saddles(); i = i + 5) {
		bm_err(
		"(%2d,%5d,%5d,%5d,%3d)(%2d,%5d,%5d,%5d,%3d)(%2d,%5d,%5d,%5d,%3d)(%2d,%5d,%5d,%5d,%3d)(%2d,%5d,%5d,%5d,%3d)\n",
		i, profile_p[i].mah, profile_p[i].voltage,
		profile_p[i].resistance, profile_p[i].percentage,
		i+1, profile_p[i+1].mah, profile_p[i+1].voltage,
		profile_p[i+1].resistance, profile_p[i+1].percentage,
		i+2, profile_p[i+2].mah, profile_p[i+2].voltage,
		profile_p[i+2].resistance, profile_p[i+2].percentage,
		i+3, profile_p[i+3].mah, profile_p[i+3].voltage,
		profile_p[i+3].resistance, profile_p[i+3].percentage,
		i+4, profile_p[i+4].mah, profile_p[i+4].voltage,
		profile_p[i+4].resistance, profile_p[i+4].percentage
		);
	}
}

int fg_adc_reset(void)
{
	int sends = 0;
	int receive = 0;

	fgr_SEND_to_kernel(FG_DAEMON_CMD_FGADC_RESET, &sends, &receive);
	bm_err(
		"send_to_kernel=FG_DAEMON_CMD_FGADC_RESET:%d %d, prev_car:%d\n",
		sends, receive, prev_car_bat0);

	prev_car_bat0 = 0;
	return receive;
}

int SOC_to_OCV_c(int _soc)
{
	struct FUELGAUGE_PROFILE_STRUCT *profile_p;
	int ret_vol = 0;
	int i = 0, size, high;
	int _dod = 10000 - _soc;

	profile_p = fg_get_profile(ptable->temperature_tb1);
	if (profile_p == NULL) {
		bm_err("[FGADC] fgauge get c table: fail !\n");
		return 0;
	}

	size = fg_get_saddles();

	for (i = 0; i < size; i++) {
		if (profile_p[i].percentage >= _dod)
			break;
	}

	if (i == 0) {
		high = 1;
		ret_vol = profile_p[0].voltage;
	} else if (i >= size) {
		high = size-1;
		ret_vol = profile_p[high].voltage;
	} else {
		high = i;

		ret_vol = interpolation(
			profile_p[high-1].percentage,
			profile_p[high-1].voltage,
			profile_p[high].percentage,
			profile_p[high].voltage,
			_dod);
	}
	bm_err("[FGADC] %s: soc:%d dod:%d! voltage:%d highidx:%d\n",
		__func__,
		_soc, _dod, ret_vol, high);

	return ret_vol;
}

int DOD_to_OCV_c(int _dod)
{
	struct FUELGAUGE_PROFILE_STRUCT *profile_p;
	int ret_vol = 0;
	int i = 0, size, high;

	profile_p = fg_get_profile(ptable->temperature_tb1);
	if (profile_p == NULL) {
		bm_err("[FGADC] fgauge get c table fail !\n");
		return 0;
	}

	size = fg_get_saddles();

	for (i = 0; i < size; i++) {
		if (profile_p[i].percentage >= _dod)
			break;
	}

	if (i == 0) {
		high = 1;
		ret_vol = profile_p[0].voltage;
	} else if (i >= size) {
		high = size-1;
		ret_vol = profile_p[high].voltage;
	} else {
		high = i;

		ret_vol = interpolation(
			profile_p[high-1].percentage,
			profile_p[high-1].voltage,
			profile_p[high].percentage,
			profile_p[high].voltage,
			_dod);
	}
	bm_err("[FGADC] DOD_to_OCV: dod:%d vol:%d highidx:%d\n",
		_dod, ret_vol, high);

	return ret_vol;
}

int OCV_to_SOC_c(int _ocv)
{
	struct FUELGAUGE_PROFILE_STRUCT *profile_p;
	int ret_vol = 0;
	int i = 0, size, high;

	profile_p = fg_get_profile(ptable->temperature_tb1);
	if (profile_p == NULL) {
		bm_err("[FGADC]OCV_to_SOC_cfgauge can't get c table: fail !\n");
		return 0;
	}

	size = fg_get_saddles();

	for (i = 0; i < size; i++) {
		if (profile_p[i].voltage <= _ocv)
			break;
	}

	if (i == 0) {
		high = 1;
		ret_vol = profile_p[0].percentage;
		ret_vol = 10000 - ret_vol;
	} else if (i >= size) {
		high = size-1;
		ret_vol = profile_p[high].percentage;
		ret_vol = 10000 - ret_vol;
	} else {
		high = i;

		ret_vol = interpolation(
			profile_p[high-1].voltage,
			profile_p[high-1].percentage,
			profile_p[high].voltage,
			profile_p[high].percentage,
			_ocv);

		ret_vol = 10000 - ret_vol;
	}
	bm_err("[FGADC] OCV_to_DOD: voltage:%d dod:%d highidx:%d\n",
		_ocv, ret_vol, high);

	return ret_vol;

}


int OCV_to_DOD_c(int _ocv)
{
	struct FUELGAUGE_PROFILE_STRUCT *profile_p;
	int ret_vol = 0;
	int i = 0, size, high;

	profile_p = fg_get_profile(ptable->temperature_tb1);
	if (profile_p == NULL) {
		bm_err("[FGADC] fgauge can't get c table: fail !\n");
		return 0;
	}

	size = fg_get_saddles();

	for (i = 0; i < size; i++) {
		if (profile_p[i].voltage <= _ocv)
			break;
	}

	if (i == 0) {
		high = 1;
		ret_vol = profile_p[0].percentage;
	} else if (i >= size) {
		high = size-1;
		ret_vol = profile_p[high].percentage;
	} else {
		high = i;

		ret_vol = interpolation(
			profile_p[high-1].voltage,
			profile_p[high-1].percentage,
			profile_p[high].voltage,
			profile_p[high].percentage,
			_ocv);
	}

	bm_err("[FGADC] OCV_to_DOD: voltage:%d dod:%d highidx:%d\n",
		_ocv, ret_vol, high);

	return ret_vol;
}

void Set_fg_c_d0_by_ocv(int _ocv)
{
	fg_c_d0_ocv = _ocv;
	fg_c_d0_dod = OCV_to_DOD_c(_ocv);
	fg_c_d0_soc = 10000 - fg_c_d0_dod;
}

void fg_update_c_dod(void)
{
	bm_err("[%s]\n",
		__func__);

	car = get_fg_hw_car();
	fg_update_quse(1);
	Set_fg_c_d0_by_ocv(fg_c_d0_ocv);
	/* using c_d0_ocv to update c_d0_dod and c_d0_soc */
	fg_c_dod = fg_c_d0_dod - car * 10000 / quse_tb1;
	fg_c_soc = 10000 - fg_c_dod;

	bm_err("[%s] fg_c_dod %d fg_c_d0_dod %d car %d quse_tb1 %d fg_c_soc %d\n",
		__func__,
		fg_c_dod, fg_c_d0_dod, car, quse_tb1, fg_c_soc);
}

void fgr_dod_init(void)
{
	int init_swocv = get_ptim_vbat();
	int con0_soc = get_con0_soc();
	int con0_uisoc = get_rtc_ui_soc();

	rtc_ui_soc = UNIT_TRANS_100 * con0_uisoc;

	if (rtc_ui_soc == 0 || con0_soc == 0) {
		rtc_ui_soc = OCV_to_SOC_c(init_swocv);
		fg_c_d0_soc = rtc_ui_soc;

		if (rtc_ui_soc < 0) {
			bm_err("[dod_init_recovery]rtcui<0,set to 0,rtc_ui_soc:%d fg_c_d0_soc:%d\n",
				rtc_ui_soc, fg_c_d0_soc);

			rtc_ui_soc = 0;
		}

		ui_d0_soc = rtc_ui_soc;
		bm_err("[dod_init_recovery]rtcui=0 case,init_swocv=%d,OCV_to_SOC_c=%d ui:[%d %d] con0_soc=[%d %d]\n",
			init_swocv, fg_c_d0_soc,
			ui_d0_soc, rtc_ui_soc,
			con0_soc, con0_uisoc);

	} else {
		ui_d0_soc = rtc_ui_soc;
		fg_c_d0_soc = UNIT_TRANS_100 * con0_soc;
	}


	fg_c_d0_ocv = SOC_to_OCV_c(fg_c_d0_soc);
	Set_fg_c_d0_by_ocv(fg_c_d0_ocv);

	fg_adc_reset();

	if (pdata->d0_sel == 1) {
		/* reserve for custom c_d0 / custom ui_soc */
		fg_c_d0_soc = get_d0_c_soc_cust();
		ui_d0_soc = get_uisoc_cust();

		fg_c_d0_ocv = SOC_to_OCV_c(fg_c_d0_soc);
		Set_fg_c_d0_by_ocv(fg_c_d0_ocv);
	}

	fg_update_c_dod();

	ui_soc = ui_d0_soc;
	soc = fg_c_soc;

	bm_err("[dod_init]fg_c_d0[%d %d %d] d0_sel[%d] c_soc[%d %d] ui[%d %d] soc[%d] con0[ui %d %d]\n",
		fg_c_d0_soc, fg_c_d0_ocv,
		fg_c_d0_dod, pdata->d0_sel,
		fg_c_dod, fg_c_soc,
		rtc_ui_soc, ui_d0_soc,
		soc, con0_uisoc, con0_soc);
}

void battery_recovery_init(void)
{
	bool is_bat_exist = 0;

	bm_err("enter MTK_BAT_RECOVERY init\n");
	is_bat_exist = pmic_is_battery_exist();
	bm_err("is_bat_exist = %d\n", is_bat_exist);

	if (is_bat_exist) {
		bm_err("battery_recovery: enter fgr_set_cust_data\n");
		fgr_set_cust_data();
		bm_err("battery_recovery: leave fgr_set_cust_data\n");
		fg_construct_table_by_temp(true, ptable->temperature_tb1);
		bm_err("battery_recovery: leave fg_construct_table_by_temp\n");
		fg_construct_vboot(ptable->temperature_tb1);
		bm_err("battery_recovery: leave fg_construct_vboot\n");
		fgr_dump_table(ptable->temperature_tb1);
		bm_err("battery_recovery: leave fgr_dump_table\n");
		fgr_dod_init();
		bm_err("battery_recovery: leave fgr_dod_init\n");
		fg_set_int1();
		bm_err("battery_recovery: leave fg_set_int1\n");
		set_init_flow_done(1);
		bm_err("battery_recovery: set_init_flow_done\n");
		set_nvram_fail_status(1);
		bm_err("battery_recovery: set_enter_recovery(set_nvram_fail_status)done\n");
		bm_err("battery_recovery %d %d\n",
			fg_table_cust_data.fg_profile[0].pseudo100,
			fg_table_cust_data.fg_profile[0].size);
	}
	bm_err("[battery_recovery] is_evb:%d,%d is_bat_exist %d\n",
		is_fg_disabled(), fg_interrupt_check(), is_bat_exist);
}
