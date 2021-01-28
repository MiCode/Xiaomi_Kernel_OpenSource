// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include "mtk_battery.h"

#define CAR_MIN_GAP 15

int set_kernel_soc(struct mtk_battery *gm, int _soc)
{
	gm->soc = (_soc + 50) / 100;
	return 0;
}

void set_fg_bat_tmp_c_gap(int tmp)
{
	battery_set_property(BAT_PROP_UISOC, tmp);
}

void set_fg_time(struct mtk_battery *gm, int _time)
{
	struct timespec time, time_now, end_time;
	ktime_t ktime;

	get_monotonic_boottime(&time_now);
	time.tv_sec = _time;
	time.tv_nsec = 0;
	end_time = timespec_add(time_now, time);
	ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);
	alarm_start(&gm->tracking_timer, ktime);
}

int get_d0_c_soc_cust(struct mtk_battery *gm, int value)
{
	//implemented by the customer
	return value;
}

int get_uisoc_cust(struct mtk_battery *gm, int value)
{
	//implemented by the customer
	return value;
}

int get_ptimrac(void)
{
	return gauge_get_int_property(
				GAUGE_PROP_PTIM_RESIST);
}

int get_ptim_vbat(void)
{
	return gauge_get_int_property(GAUGE_PROP_PTIM_BATTERY_VOLTAGE) * 10;
}

int get_ptim_i(struct mtk_battery *gm)
{
	struct power_supply *psy;
	union power_supply_propval val;

	psy = gm->gauge->psy;
	power_supply_get_property(psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);

	return val.intval;
}

void get_hw_info(void)
{
	gauge_set_property(GAUGE_PROP_HW_INFO, 0);
}

int get_charger_exist(void)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name("ac");
	if (psy != NULL) {
		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_ONLINE, &val);
		if (val.intval == true)
			return true;
	}

	psy = power_supply_get_by_name("usb");
	if (psy != NULL) {
		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_ONLINE, &val);
		if (val.intval == true)
			return true;
	}

	return false;
}

int get_charger_status(struct mtk_battery *gm)
{
	int charger_status = 0;

	if (gm->bs_data.bat_status ==
				POWER_SUPPLY_STATUS_NOT_CHARGING)
		charger_status = -1;
	else
		charger_status = 0;

	return charger_status;
}

int get_imix_r(void)
{
	/*todo in alps*/
	return 0;
}

int fg_adc_reset(struct mtk_battery *gm)
{
	battery_set_property(BAT_PROP_FG_RESET, 0);
	return 0;
}

static int interpolation(int i1, int b1, int i2, int b2, int i)
{
	int ret;

	ret = (b2 - b1) * (i - i1) / (i2 - i1) + b1;
	return ret;
}

int fg_get_saddles(struct mtk_battery *gm)
{
	return gm->fg_table_cust_data.fg_profile[0].size;
}

struct fuelgauge_profile_struct *fg_get_profile(
	struct mtk_battery *gm, int temperature)
{
	int i;
	struct fuel_gauge_table_custom_data *ptable;

	ptable = &gm->fg_table_cust_data;
	for (i = 0; i < ptable->active_table_number; i++)
		if (ptable->fg_profile[i].temperature == temperature)
			return &ptable->fg_profile[i].fg_profile[0];

	if (ptable->temperature_tb0 == temperature)
		return &ptable->fg_profile_temperature_0[0];

	if (ptable->temperature_tb1 == temperature)
		return &ptable->fg_profile_temperature_1[0];

	bm_debug("[%s]: no table for %d\n",
		__func__,
		temperature);

	return NULL;
}

int fg_check_temperature_order(struct mtk_battery *gm,
	int *is_ascending, int *is_descending)
{
	int i;
	struct fuel_gauge_table_custom_data *ptable;

	ptable = &gm->fg_table_cust_data;
	*is_ascending = 0;
	*is_descending = 0;
	/* is ascending*/

	bm_debug("act:%d table: %d %d %d %d %d %d %d %d %d %d\n",
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

	bm_debug("active_table_no is %d, %d %d\n",
		ptable->active_table_number,
		*is_ascending,
		*is_descending);
	for (i = 0; i < ptable->active_table_number; i++) {
		bm_debug("table[%d]:%d\n",
			i,
			ptable->fg_profile[i].temperature);
	}

	if (*is_ascending == 0 && *is_descending == 0)
		return -1;

	return 0;
}

void fgr_construct_battery_profile(struct mtk_battery *gm, int table_idx)
{
	struct fuelgauge_profile_struct *low_profile_p = NULL;
	struct fuelgauge_profile_struct *high_profile_p = NULL;
	struct fuelgauge_profile_struct *temp_profile_p = NULL;
	int low_temp = 0, high_temp = 0, temperature = 0;
	int i, saddles;
	int low_pseudo1 = 0, high_pseudo1 = 0;
	int low_pseudo100 = 0, high_pseudo100 = 0;
	int low_qmax = 0, high_qmax = 0, low_qmax_h = 0, high_qmax_h = 0;
	int low_shutdown_zcv = 0, high_shutdown_zcv = 0;
	int is_ascending, is_descending;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	temperature = algo->last_temp;
	temp_profile_p = fg_get_profile(gm, table_idx);

	if (temp_profile_p == NULL) {
		bm_debug("[FGADC] fg_get_profile : create table fail !\n");
		return;
	}

	if (fg_check_temperature_order(gm, &is_ascending, &is_descending)) {
		bm_err("[FGADC] fg_check_temperature_order : t0~t3 setting error !\n");
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
			fg_get_profile(gm,
			ptable->fg_profile[i - 1].temperature);
		high_profile_p =
			fg_get_profile(gm,
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
			fg_get_profile(gm, ptable->fg_profile[i].temperature);
		high_profile_p =
			fg_get_profile(gm,
				ptable->fg_profile[i - 1].temperature);
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
		algo->T_table = temperature;
	if (table_idx == 254)
		algo->T_table_c = temperature;

	saddles = fg_get_saddles(gm);
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
		temp_profile_p[i].resistance2 =
		interpolation(low_temp, low_profile_p[i].resistance2,
		high_temp, high_profile_p[i].resistance2, temperature);

	}

	if (table_idx == ptable->temperature_tb0) {
		if (pdata->pseudo1_en == true)
			algo->batterypseudo1_h = interpolation(
			low_temp,
			low_pseudo1,
			high_temp,
			high_pseudo1,
			temperature);

		if (pdata->pseudo100_en == true)
			algo->batterypseudo100 = interpolation(
			low_temp,
			low_pseudo100,
			high_temp,
			high_pseudo100,
			temperature);

		bm_debug("[Profile_Table]pseudo1_en:[%d] lowT %d %d %d lowPs1 %d highPs1 %d batterypseudo1_h [%d]\n",
			pdata->pseudo1_en, low_temp,
			high_temp, temperature,
			low_pseudo1, high_pseudo1,
			algo->batterypseudo1_h);
		bm_debug("[Profile_Table]pseudo100_en:[%d] %d lowT %d %d %d low100 %d %d [%d]\n",
			pdata->pseudo100_en, pdata->pseudo100_en_dis,
			low_temp, high_temp, temperature,
			low_pseudo100, high_pseudo100,
			algo->batterypseudo100);

/*
 *	low_qmax and High_qmax need to do
 *	UNIT_TRANS_10 from "1 mAHR" to "0.1 mAHR"
 */
		algo->qmax_t_0ma_h = interpolation(
			low_temp, UNIT_TRANS_10 * low_qmax,
			high_temp, UNIT_TRANS_10 * high_qmax,
			temperature);
		algo->qmax_t_Nma_h = interpolation(
			low_temp, UNIT_TRANS_10 * low_qmax_h,
			high_temp, UNIT_TRANS_10 * high_qmax_h,
			temperature);

		bm_debug("[Profile_Table]lowT %d %d %d lowQ %d %d qmax_t_0ma_h [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_qmax,
			UNIT_TRANS_10 * high_qmax,
			algo->qmax_t_0ma_h);
		bm_debug("[Profile_Table]lowT %d %d %d lowQh %d %d qmax_t_Nma_h [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_qmax_h,
			UNIT_TRANS_10 * high_qmax_h,
			algo->qmax_t_Nma_h);

		algo->shutdown_hl_zcv = interpolation(
			low_temp, UNIT_TRANS_10 * low_shutdown_zcv,
			high_temp, UNIT_TRANS_10 * high_shutdown_zcv,
			temperature);

		bm_debug("[Profile_Table]lowT %d %d %d LowShutZCV %d HighShutZCV %d shutdown_hl_zcv [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_shutdown_zcv,
			UNIT_TRANS_10 * high_shutdown_zcv,
			algo->shutdown_hl_zcv);

	} else if (table_idx == ptable->temperature_tb1) {
/*
 *	low_qmax and High_qmax need to do
 *	UNIT_TRANS_10 from "1 mAHR" to "0.1 mAHR"
 */
		algo->qmax_t_0ma_h_tb1 = interpolation(
		low_temp, UNIT_TRANS_10 * low_qmax,
			high_temp, UNIT_TRANS_10 * high_qmax,
			temperature);
		algo->qmax_t_Nma_h_tb1 = interpolation(
			low_temp, UNIT_TRANS_10 * low_qmax_h,
			high_temp, UNIT_TRANS_10 * high_qmax_h,
			temperature);

		bm_debug("[Profile_Table]lowT %d %d %d lowQ %d %d qmax_t_0ma_h [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_qmax,
			UNIT_TRANS_10 * high_qmax,
			algo->qmax_t_0ma_h_tb1);
		bm_debug("[Profile_Table]lowT %d %d %d lowQh %d %d qmax_t_Nma_h [%d]\n",
			low_temp, high_temp, temperature,
			UNIT_TRANS_10 * low_qmax_h,
			UNIT_TRANS_10 * high_qmax_h,
			algo->qmax_t_Nma_h_tb1);
	}

	bm_debug("[Profile_Table]T_table %d T_table_c %d %d %d is_ascend %d %d\n",
		algo->T_table, algo->T_table_c, pdata->pseudo1_en,
		pdata->pseudo100_en, is_ascending, is_descending);
	bm_debug("[Profile_Table]Pseudo1_h %d %d, Qmax_T_0mA_H %d,%d qmax_t_0ma_h_tb1 %d %d\n",
		algo->batterypseudo1_h,
		algo->batterypseudo100,
		algo->qmax_t_0ma_h,
		algo->qmax_t_Nma_h,
		algo->qmax_t_0ma_h_tb1,
		algo->qmax_t_Nma_h_tb1);
}

void fgr_construct_table_by_temp(
	struct mtk_battery *gm, bool update, int table_idx)
{
	int fg_temp;
	struct mtk_battery_algo *algo;

	algo = &gm->algo;
	fg_temp = force_get_tbat(gm, true);
	if (fg_temp != algo->last_temp || update == true) {
		bm_err("[%s] tempture from(%d)to(%d) Tb:%d",
			__func__,
			algo->last_temp, fg_temp, table_idx);
		algo->last_temp = fg_temp;
		fgr_construct_battery_profile(gm, table_idx);
	}
}

void fg_construct_battery_profile_by_qmax(struct mtk_battery *gm,
	int qmax, int table_index)
{
	int i;
	struct fuelgauge_profile_struct *profile_p;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	profile_p = fg_get_profile(gm, table_index);

	if (table_index == ptable->temperature_tb0) {
		algo->qmax_t_0ma = qmax;

		for (i = 0; i < 100; i++)
			profile_p[i].percentage =
			profile_p[i].mah * 10000 / algo->qmax_t_0ma;
	} else if (table_index == ptable->temperature_tb1) {
		algo->qmax_t_0ma_tb1 = qmax;
		for (i = 0; i < 100; i++)
			profile_p[i].percentage =
			profile_p[i].mah * 10000 / algo->qmax_t_0ma_tb1;
	}

	bm_debug("[%s] qmax:%d qmax_t_0ma:%d\n",
		__func__,
		qmax, algo->qmax_t_0ma);
}

void fg_construct_battery_profile_by_vboot(struct mtk_battery *gm,
	int _vboot, int table_index)
{
	int i, j;
	struct fuelgauge_profile_struct *profile_p;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	profile_p = fg_get_profile(gm, table_index);

	for (j = 0; j < 100; j++)
		if (profile_p[j].voltage < _vboot)
			break;

	if (table_index == ptable->temperature_tb0) {
		if (j == 0) {
			algo->qmax_t_0ma = profile_p[0].mah;
		} else if (j >= 100) {
			algo->qmax_t_0ma = profile_p[99].mah;
		} else {
			/*qmax_t_0ma = profile_p[j].mah;*/
			algo->qmax_t_0ma = interpolation(
				profile_p[j].voltage,
				profile_p[j].mah,
				profile_p[j-1].voltage,
				profile_p[j-1].mah,
				_vboot);
		}

		if (algo->qmax_t_0ma < 3000) {
			bm_err("[ERR][%s]index %d idx:%d _vboot:%d %d qmax_t_0ma:[%d => 3000]\n",
				__func__,
				table_index, j,
				_vboot, profile_p[j].voltage, algo->qmax_t_0ma);
		}

		if (algo->qmax_t_0ma > 50000) {
			bm_err("[ERR][%s]index %d idx:%d _vboot:%d %d qmax_t_0ma:[%d => 50000]\n",
				__func__,
				table_index, j,
				_vboot, profile_p[j].voltage, algo->qmax_t_0ma);
		}

		for (i = 0; i < 100; i++)
			profile_p[i].percentage =
			profile_p[i].mah * 10000 / algo->qmax_t_0ma;

	} else if (table_index == ptable->temperature_tb1) {
		if (j == 0) {
			algo->qmax_t_0ma_tb1 = profile_p[0].mah;
		} else if (j >= 100) {
			algo->qmax_t_0ma_tb1 = profile_p[99].mah;
		} else {
			/*qmax_t_0ma = profile_p[j].mah;*/
			algo->qmax_t_0ma_tb1 =
			interpolation(
			profile_p[j].voltage,
			profile_p[j].mah,
			profile_p[j-1].voltage,
			profile_p[j-1].mah, _vboot);
		}

		if (algo->qmax_t_0ma_tb1 < 3000) {
			bm_err("[ERR][%s]index %d idx:%d _vboot:%d %d qmax_t_0ma_tb1:[%d => 3000]\n",
				__func__,
				table_index, j,
				_vboot, profile_p[j].voltage,
				algo->qmax_t_0ma_tb1);
		}

		if (algo->qmax_t_0ma_tb1 > 50000) {
			bm_err("[ERR][%s]index %d idx:%d _vboot:%d %d qmax_t_0ma_tb1:[%d => 50000]\n",
				__func__,
				table_index, j,
				_vboot, profile_p[j].voltage,
				algo->qmax_t_0ma_tb1);
		}

		for (i = 0; i < 100; i++)
			profile_p[i].percentage =
			profile_p[i].mah * 10000 / algo->qmax_t_0ma_tb1;
	}

	if (table_index == ptable->temperature_tb1) {
		bm_debug("[%s]index %d idx:%d _vboot:%d %d qmax_t_0ma_tb1:%d\n",
			__func__,
			table_index, j, _vboot,
			profile_p[j].voltage, algo->qmax_t_0ma_tb1);
	} else {
		bm_debug("[%s]index %d idx:%d _vboot:%d %d qmax_t_0ma:%d\n",
			__func__,
			table_index, j, _vboot,
			profile_p[j].voltage, algo->qmax_t_0ma);
	}
}

static int fg_compensate_battery_voltage_from_low(
	struct mtk_battery *gm,
	int oriv, int curr, int tablei)
{
	int fg_volt, fg_volt_withIR, ret_compensate_value = 0;
	int hit_h_percent = 0, hit_l_percent = 0;
	struct fuelgauge_profile_struct *profile_p;
	int i = 0, size, high = 0;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	profile_p = fg_get_profile(gm, tablei);
	if (profile_p == NULL) {
		bm_err("[ERR][%s] fail ,profile_p=null!\n",
			__func__);
		return 0;
	}
	size = fg_get_saddles(gm);

	bm_debug("[%s]size:%d oriv=%d I:%d\n",
		__func__,
		size, oriv, curr);

	for (; size > 0; size--) {
		high = size-1;
		if (high >= 1) {
			if (profile_p[high-1].percentage < 10000) {
				bm_debug("[%s]find high=%d,[%d][%d]\n",
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
			algo->fg_resistance_bat =  profile_p[high-1].resistance;
			ret_compensate_value =
				(curr * (algo->fg_resistance_bat *
				algo->DC_ratio / 100 + pdata->r_fg_value +
				pdata->fg_meter_resistance)) / 1000;
			ret_compensate_value = (ret_compensate_value + 5) / 10;
			fg_volt_withIR = fg_volt + ret_compensate_value;
			if (fg_volt_withIR > oriv) {
				hit_h_percent = profile_p[high].percentage;
				hit_l_percent = profile_p[high-1].percentage;
				bm_err("[%s]h_percent=[%d,%d],high=%d,fg_volt_withIR=%d > oriv=%d\n",
					__func__,
					hit_h_percent, hit_l_percent,
					high, fg_volt_withIR, oriv);
				break;
			}
		} else {
			bm_err("[ERR][%s] can't find available voltage!!!\n",
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

		algo->fg_resistance_bat = interpolation(
			profile_p[high-1].percentage,
			profile_p[high-1].resistance,
			profile_p[high].percentage,
			profile_p[high].resistance, i);

		ret_compensate_value =
			(curr * (algo->fg_resistance_bat * algo->DC_ratio
			/ 100 + pdata->r_fg_value +
			pdata->fg_meter_resistance)) / 1000;
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

	bm_err("[ERR][%s] should not reach here!!!!!!\n",
		__func__);
	return fg_volt;
}

void fgr_construct_vboot(struct mtk_battery *gm, int table_idx)
{
	int iboot = 0;
	int rac = get_ptimrac();
	int ptim_vbat;
	int ptim_i;
	int vboot_t = 0;
	int curr_temp = force_get_tbat(gm, true);
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	ptim_i = get_ptim_i(gm);
	ptim_vbat = gauge_get_int_property(GAUGE_PROP_PTIM_BATTERY_VOLTAGE)
		* 10;

	bm_debug("[%s] idx %d T_NEW %d T_table %d T_table_c %d qmax_sel %d\n",
		__func__,
		table_idx, curr_temp,
		algo->T_table, algo->T_table_c,
		pdata->qmax_sel);

	if (pdata->iboot_sel == 0)
		iboot = ptable->fg_profile[0].pon_iboot;
	else
		iboot = pdata->shutdown_system_iboot;

	if (pdata->qmax_sel == 0) {
		algo->vboot =
			ptable->fg_profile[0].pmic_min_vol
			+ iboot * rac / 10000;
		if (table_idx == ptable->temperature_tb0)
			fg_construct_battery_profile_by_qmax(gm,
			algo->qmax_t_0ma_h, table_idx);
		if (table_idx == ptable->temperature_tb1)
			fg_construct_battery_profile_by_qmax(gm,
			algo->qmax_t_0ma_h_tb1, table_idx);
	} else if (pdata->qmax_sel == 1) {
		vboot_t =
			ptable->fg_profile[0].pmic_min_vol
			+ iboot * rac / 10000;

		fg_construct_battery_profile_by_vboot(gm, vboot_t, table_idx);
		if (table_idx == 255) {
			algo->vboot =
				fg_compensate_battery_voltage_from_low(gm,
				ptable->fg_profile[0].pmic_min_vol,
				(0 - iboot), table_idx);
			fg_construct_battery_profile_by_vboot(gm,
				algo->vboot, table_idx);
		} else if (table_idx == 254) {
			algo->vboot_c =
				fg_compensate_battery_voltage_from_low(gm,
				ptable->fg_profile[0].pmic_min_vol,
				(0 - iboot), table_idx);
			fg_construct_battery_profile_by_vboot(gm,
				algo->vboot_c, table_idx);
		}
		bm_debug("[%s]idx %d T_NEW %d T_table %d T_table_c %d qmax_sel %d vboot_t=[%d:%d:%d] %d %d rac %d\n",
			__func__,
			table_idx, curr_temp,
			algo->T_table, algo->T_table_c,
			pdata->qmax_sel, vboot_t,
			algo->vboot, algo->vboot_c,
			ptable->fg_profile[0].pmic_min_vol,
			iboot, rac);
	}

/* batterypseudo1_auto = get_batterypseudo1_auto(vboot, shutdown_hl_zcv); */

	if (algo->qmax_t_aging == 9999999 || algo->aging_factor > 10000)
		algo->aging_factor = 10000;

	bm_debug("[%s] qmax_sel=%d iboot_sel=%d iboot:%d vbat:%d i:%d vboot:%d %d %d\n",
		__func__,
		pdata->qmax_sel, pdata->iboot_sel, iboot,
		ptim_vbat, ptim_i, algo->vboot, algo->vboot_c, vboot_t);

	if (pdata->qmax_sel == 0) {
		bm_debug("[%s][by_qmax]qmax_sel %d qmax %d vboot %d %d pmic_min_vol %d iboot %d r %d\n",
			__func__,
			pdata->qmax_sel, algo->qmax_t_0ma_h,
			algo->vboot, algo->vboot_c,
			ptable->fg_profile[0].pmic_min_vol,
			iboot, rac);
	}
	if (pdata->qmax_sel == 1) {
		bm_debug("[%s][by_vboot]qmax_sel %d vboot_t %d vboot %d %d pmic_min_vol %d iboot %d rac %d\n",
			__func__,
			pdata->qmax_sel, vboot_t, algo->vboot,
			algo->vboot_c,
			ptable->fg_profile[0].pmic_min_vol,
			iboot, rac);
	}
}

void fgr_dump_table(struct mtk_battery *gm, int idx)
{
	struct fuelgauge_profile_struct *profile_p;
	int i;

	profile_p = fg_get_profile(gm, idx);

	bm_err("[%s]table idx:%d (i,mah,voltage,resistance,percentage)\n",
		__func__,
		idx);
	for (i = 0; i < fg_get_saddles(gm); i = i + 5) {
		bm_err("(%2d,%5d,%5d,%5d,%3d)(%2d,%5d,%5d,%5d,%3d)(%2d,%5d,%5d,%5d,%3d)(%2d,%5d,%5d,%5d,%3d)(%2d,%5d,%5d,%5d,%3d)\n",
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

void fgr_update_quse(struct mtk_battery *gm, int caller)
{
	int aging_factor_cust = 0;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	/* caller = 1 means update c table */
	/* caller = 2 means update v table */

	if (caller == 1) {
		if (pdata->aging_sel == 1)
			algo->quse_tb1 =
			algo->qmax_t_0ma_tb1 * aging_factor_cust / 10000;
		else
			algo->quse_tb1 =
			algo->qmax_t_0ma_tb1 * algo->aging_factor / 10000;
	} else {
		if (pdata->aging_sel == 1)
			algo->quse_tb0 =
			algo->qmax_t_0ma * aging_factor_cust / 10000;
		else
			algo->quse_tb0 =
			algo->qmax_t_0ma * algo->aging_factor / 10000;
	}

	if (caller == 1) {
		bm_debug("[%s]aging_sel %d qmax_t_0ma_tb1 %d quse_tb1 [%d] aging[%d]\n",
			__func__,
			pdata->aging_sel, algo->qmax_t_0ma_tb1,
			algo->quse_tb1, algo->aging_factor);
	}
}

/* update uisoc ht/lt gap */
void fgr_update_uisoc_threshold(struct mtk_battery *gm)
{
	int D_Remain = 0;
	struct mtk_battery_algo *algo;

	algo = &gm->algo;

	algo->car = gauge_get_int_property(GAUGE_PROP_COULOMB);
	fgr_update_quse(gm, 1);

	/* calculate ui ht gap */
	algo->ht_gap = algo->quse_tb1 / 100;

	if (algo->ht_gap < (algo->quse_tb1 / 1000))
		algo->ht_gap = algo->quse_tb1 / 1000;

	if (algo->ui_soc <= 100)
		algo->ht_gap = algo->quse_tb1 / 200;

	if (algo->ht_gap < CAR_MIN_GAP)
		algo->ht_gap = CAR_MIN_GAP;

	/* calculate ui lt_gap */
	D_Remain = algo->soc * algo->quse_tb1 / 10000;
	algo->lt_gap =
		D_Remain * gm->fg_cust_data.diff_soc_setting
		/ algo->ui_soc;

	if (algo->lt_gap < (algo->quse_tb1 / 1000))
		algo->lt_gap = algo->quse_tb1 / 1000;

	if (algo->ui_soc <= 100)
		algo->lt_gap = algo->quse_tb1 / 200;

	if (algo->lt_gap < CAR_MIN_GAP)
		algo->lt_gap = CAR_MIN_GAP;

	bm_debug("[%s]car:%d quse_tb1[%d %d] gap[%d %d][%d]\n",
		__func__,
		algo->car, algo->quse_tb1,
		algo->soc, algo->ht_gap,
		algo->lt_gap, D_Remain);
}

void fgr_update_uisoc_ht(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;

	algo = &gm->algo;
	fgr_update_uisoc_threshold(gm);
	battery_set_property(BAT_PROP_UISOC_HT_INT_GAP, algo->ht_gap);
	bm_debug("[%s] update ht_en:%d ht_gap:%d\n",
		__func__,
		algo->uisoc_ht_en, algo->ht_gap);

}

void fgr_update_uisoc_lt(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;

	algo = &gm->algo;
	fgr_update_uisoc_threshold(gm);
	battery_set_property(BAT_PROP_UISOC_LT_INT_GAP, algo->lt_gap);
	bm_debug("[%s] update lt_en:%d lt_gap:%d\n",
		__func__,
		algo->uisoc_lt_en, algo->lt_gap);
}

void fg_enable_uisoc_ht(struct mtk_battery *gm, int en)
{
	struct mtk_battery_algo *algo;

	algo = &gm->algo;
	algo->uisoc_ht_en = en;
	battery_set_property(BAT_PROP_ENABLE_UISOC_HT_INT, en);
	bm_debug("[%s] ht_en:%d ht_gap:%d\n",
		__func__,
		algo->uisoc_ht_en, algo->ht_gap);
}

void fg_enable_uisoc_lt(struct mtk_battery *gm, int en)
{
	struct mtk_battery_algo *algo;

	algo = &gm->algo;
	algo->uisoc_lt_en = en;
	battery_set_property(BAT_PROP_ENABLE_UISOC_LT_INT, en);
	bm_debug("[%s] lt_en:%d lt_gap:%d\n",
		__func__, algo->uisoc_lt_en, algo->lt_gap);
}

int SOC_to_OCV_c(struct mtk_battery *gm, int _soc)
{
	struct fuelgauge_profile_struct *profile_p;
	int ret_vol = 0;
	int i = 0, size, high;
	int _dod = 10000 - _soc;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	profile_p = fg_get_profile(gm, ptable->temperature_tb1);
	if (profile_p == NULL) {
		bm_err("[%s] fgauge get c table: fail !\n",
			__func__);
		return 0;
	}

	size = fg_get_saddles(gm);

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
	bm_debug("[%s]soc:%d dod:%d! voltage:%d highidx:%d\n",
		__func__,
		_soc, _dod, ret_vol, high);

	return ret_vol;
}

int DOD_to_OCV_c(struct mtk_battery *gm, int _dod)
{
	struct fuelgauge_profile_struct *profile_p;
	int ret_vol = 0;
	int i = 0, size, high;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	profile_p = fg_get_profile(gm, ptable->temperature_tb1);
	if (profile_p == NULL) {
		bm_err("[%s] fgauge get c table fail !\n",
			__func__);
		return 0;
	}

	size = fg_get_saddles(gm);

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
	bm_debug("[%s]DOD_to_OCV: dod:%d vol:%d highidx:%d\n",
		__func__,
		_dod, ret_vol, high);

	return ret_vol;
}

int OCV_to_SOC_c(struct mtk_battery *gm, int _ocv)
{
	struct fuelgauge_profile_struct *profile_p;
	int ret_vol = 0;
	int i = 0, size, high;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;


	profile_p = fg_get_profile(gm, ptable->temperature_tb1);
	if (profile_p == NULL) {
		bm_err("[%s]can't get c table: fail\n",
			__func__);
		return 0;
	}

	size = fg_get_saddles(gm);

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
	bm_debug("[%s]voltage:%d dod:%d highidx:%d\n",
		__func__,
		_ocv, ret_vol, high);

	return ret_vol;
}

int OCV_to_DOD_c(struct mtk_battery *gm, int _ocv)
{
	struct fuelgauge_profile_struct *profile_p;
	int ret_vol = 0;
	int i = 0, size, high;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	profile_p = fg_get_profile(gm, ptable->temperature_tb1);
	if (profile_p == NULL) {
		bm_err("[%s] fgauge can't get c table: fail\n",
			__func__);
		return 0;
	}

	size = fg_get_saddles(gm);

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

	bm_debug("[%s]voltage:%d dod:%d highidx:%d\n",
		__func__,
		_ocv, ret_vol, high);

	return ret_vol;
}

void Set_fg_c_d0_by_ocv(struct mtk_battery *gm, int _ocv)
{
	struct mtk_battery_algo *algo;

	algo = &gm->algo;
	algo->fg_c_d0_ocv = _ocv;
	algo->fg_c_d0_dod = OCV_to_DOD_c(gm, _ocv);
	algo->fg_c_d0_soc = 10000 - algo->fg_c_d0_dod;
}

void fgr_set_soc_by_vc_mode(struct mtk_battery *gm)
{
	gm->algo.soc = gm->algo.fg_c_soc;
}

void fgr_update_fg_bat_tmp_threshold_c(struct mtk_battery *gm)
{
	gm->algo.fg_bat_tmp_c_gap = 1;
}

void fgr_update_c_dod(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	algo->car = gauge_get_int_property(GAUGE_PROP_COULOMB);
	fgr_update_quse(gm, 1);
	Set_fg_c_d0_by_ocv(gm, algo->fg_c_d0_ocv);
	algo->fg_c_dod = algo->fg_c_d0_dod - algo->car * 10000 / algo->quse_tb1;
	algo->fg_c_soc = 10000 - algo->fg_c_dod;

	bm_debug("[%s] fg_c_dod %d fg_c_d0_dod %d car %d quse_tb1 %d fg_c_soc %d\n",
		__func__,
		algo->fg_c_dod, algo->fg_c_d0_dod,
		algo->car, algo->quse_tb1,
		algo->fg_c_soc);
}

void fgr_dod_init(struct mtk_battery *gm)
{
	int init_swocv = get_ptim_vbat();
	int con0_soc = gauge_get_int_property(GAUGE_PROP_CON0_SOC);
	int con0_uisoc = gauge_get_int_property(GAUGE_PROP_RTC_UI_SOC);

	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	algo->rtc_ui_soc = UNIT_TRANS_100 * con0_uisoc;
	init_swocv = gauge_get_int_property(GAUGE_PROP_PTIM_BATTERY_VOLTAGE)
		* 10;

	if (algo->rtc_ui_soc == 0 || con0_soc == 0) {
		algo->rtc_ui_soc = OCV_to_SOC_c(gm, init_swocv);
		algo->fg_c_d0_soc = algo->rtc_ui_soc;

		if (algo->rtc_ui_soc < 0) {
			bm_err("[%s]rtcui<0,set to 0,rtc_ui_soc:%d fg_c_d0_soc:%d\n",
				__func__,
				algo->rtc_ui_soc, algo->fg_c_d0_soc);
			algo->rtc_ui_soc = 0;
		}

		algo->ui_d0_soc = algo->rtc_ui_soc;
		bm_err("[%s]rtcui=0 case,init_swocv=%d,OCV_to_SOC_c=%d ui:[%d %d] con0_soc=[%d %d]\n",
			__func__,
			init_swocv, algo->fg_c_d0_soc,
			algo->ui_d0_soc, algo->rtc_ui_soc,
			con0_soc, con0_uisoc);
	} else {
		algo->ui_d0_soc = algo->rtc_ui_soc;
		algo->fg_c_d0_soc = UNIT_TRANS_100 * con0_soc;
	}

	algo->fg_c_d0_ocv = SOC_to_OCV_c(gm, algo->fg_c_d0_soc);
	Set_fg_c_d0_by_ocv(gm, algo->fg_c_d0_ocv);
	fg_adc_reset(gm);

	if (pdata->d0_sel == 1) {
		/* reserve for custom c_d0 / custom ui_soc */
		algo->fg_c_d0_soc = get_d0_c_soc_cust(gm, algo->fg_c_d0_soc);
		algo->ui_d0_soc = get_uisoc_cust(gm, algo->ui_d0_soc);

		algo->fg_c_d0_ocv = SOC_to_OCV_c(gm, algo->fg_c_d0_soc);
		Set_fg_c_d0_by_ocv(gm, algo->fg_c_d0_ocv);
	}
	fgr_update_c_dod(gm);
	algo->ui_soc = algo->ui_d0_soc;
	fgr_set_soc_by_vc_mode(gm);

	bm_err("[%s]fg_c_d0[%d %d %d] d0_sel[%d] c_soc[%d %d] ui[%d %d] soc[%d] con0[ui %d %d]\n",
		__func__,
		algo->fg_c_d0_soc, algo->fg_c_d0_ocv,
		algo->fg_c_d0_dod, pdata->d0_sel,
		algo->fg_c_dod, algo->fg_c_soc,
		algo->rtc_ui_soc, algo->ui_d0_soc,
		algo->soc, con0_uisoc, con0_soc);
}

void fgr_imix_error_calibration(struct mtk_battery *gm)
{
	int imix = 0;
	int iboot = 0;

	imix = get_imix_r();
	iboot = gm->fg_cust_data.shutdown_system_iboot;

	if ((imix < iboot) && (imix > 0))
		fg_adc_reset(gm);
}

void fgr_dlpt_sd_handler(struct mtk_battery *gm)
{
	gm->ui_soc = 0;
	gm->algo.low_tracking_enable = 0;
	set_fg_time(gm, 0);
	battery_set_property(BAT_PROP_ENABLE_UISOC_HT_INT, 0);
	battery_set_property(BAT_PROP_ENABLE_UISOC_LT_INT, 0);
	battery_set_property(BAT_PROP_UISOC, gm->algo.ui_soc);
	fgr_imix_error_calibration(gm);
}

void fgr_shutdown_int_handler(struct mtk_battery *gm)
{
	gm->algo.low_tracking_enable = 1;
	set_fg_time(gm, gm->fg_cust_data.discharge_tracking_time);
	fgr_imix_error_calibration(gm);
}

void fgr_error_calibration2(struct mtk_battery *gm, int intr_no)
{
	int shutdown_cond = get_shutdown_cond(gm);

	if (shutdown_cond != 1)
		gm->algo.low_tracking_enable = false;
}

void fgr_int_end_flow(struct mtk_battery *gm, unsigned int intr_no)
{
	int curr_temp, vbat;
	char intr_name[32];
	struct mtk_battery_algo *algo;

	algo = &gm->algo;
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
		bm_err("[Intr_Number_to_Name] unknown intr %d\n",
			intr_no);
		break;
	}

	algo->car = gauge_get_int_property(GAUGE_PROP_COULOMB);
	get_hw_info();
	vbat = gauge_get_int_property(GAUGE_PROP_BATTERY_VOLTAGE);
	curr_temp = force_get_tbat(gm, true);

	set_kernel_soc(gm, algo->soc);
	battery_set_property(BAT_PROP_UISOC, algo->ui_soc);
	gauge_set_property(GAUGE_PROP_RTC_UI_SOC,
		(algo->ui_soc + 50) / 100);

	if (algo->soc <= 100)
		gauge_set_property(GAUGE_PROP_CON0_SOC, 100);
	else if (algo->soc >= 10000)
		gauge_set_property(GAUGE_PROP_CON0_SOC, 10000);
	else
		gauge_set_property(GAUGE_PROP_CON0_SOC, algo->soc);

	fgr_error_calibration2(gm, intr_no);
	bm_debug("[%s][%s]soc:%d, c_soc:%d ui_soc:%d VBAT %d T:[%d C:%d] car:%d\n",
		__func__,
		intr_name, algo->soc, algo->fg_c_soc,
		algo->ui_soc, vbat, curr_temp,
		algo->T_table_c, algo->car);
}

void fgr_temp_c_int_handler(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;

	/* int curr_temp; */
	fgr_construct_table_by_temp(gm, true, ptable->temperature_tb1);
	fgr_construct_vboot(gm, ptable->temperature_tb1);
	/* fg_debug_dump(ptable->temperature_tb1);*/
	fgr_update_c_dod(gm);

	fgr_set_soc_by_vc_mode(gm);
	algo->fg_bat_tmp_c_gap = 1;
	set_fg_bat_tmp_c_gap(algo->fg_bat_tmp_c_gap);
}

void fgr_update_fg_bat_int1_threshold(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	pdata = &gm->fg_cust_data;

	algo->fg_bat_int1_gap = algo->quse_tb1
		* pdata->diff_soc_setting / 10000;

	if (algo->fg_bat_int1_gap < CAR_MIN_GAP)
		algo->fg_bat_int1_gap = CAR_MIN_GAP;

	bm_debug("[%s] quse_tb1:%d gap:%d diff_soc_setting:%d MIN:%d\n",
		__func__,
		algo->quse_tb1, algo->fg_bat_int1_gap,
		pdata->diff_soc_setting, CAR_MIN_GAP);
}

void fgr_bat_int1_handler(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;

	algo = &gm->algo;
	fgr_update_c_dod(gm);
	fgr_update_fg_bat_int1_threshold(gm);
	battery_set_property(BAT_PROP_COULOMB_INT_GAP,
		algo->fg_bat_int1_gap);
	fgr_set_soc_by_vc_mode(gm);

	bm_debug("[%s]soc %d\n",
		__func__, algo->soc);
}

void fgr_bat_int2_h_handler(struct mtk_battery *gm)
{
	int ui_gap_ht = 0;
	/* int is_charger_exist = get_charger_exist(); */
	int _car;
	int delta_car_bat0;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	_car = gauge_get_int_property(GAUGE_PROP_COULOMB);
	delta_car_bat0 = abs(algo->prev_car_bat0 - _car);

	fgr_update_uisoc_threshold(gm);
	ui_gap_ht = delta_car_bat0 * UNIT_TRANS_100 / algo->ht_gap;

	bm_debug("[%s][IN]ui_soc %d, ht_gap:[%d %d], _car[%d %d %d]\n",
		__func__,
		algo->ui_soc, algo->ht_gap, ui_gap_ht,
		_car, algo->prev_car_bat0, delta_car_bat0);

	if (ui_gap_ht > 100)
		ui_gap_ht = 100;

	if (ui_gap_ht > 0)
		algo->prev_car_bat0 = _car;

	if (algo->ui_soc >= 10000)
		algo->ui_soc = 10000;
	else {
		if ((algo->ui_soc + ui_gap_ht) >= 10000)
			algo->ui_soc = 10000;
		else
			algo->ui_soc = algo->ui_soc + ui_gap_ht;
	}

	if (algo->ui_soc >= 10000)
		algo->ui_soc = 10000;

	fgr_update_uisoc_ht(gm);
	fgr_update_uisoc_lt(gm);

	bm_debug("[%s][OUT]ui_soc %d, ui_gap_ht:%d, _car[%d %d %d]\n",
		__func__, algo->ui_soc, ui_gap_ht,
		_car, algo->prev_car_bat0, delta_car_bat0);
}

void fgr_bat_int2_l_handler(struct mtk_battery *gm)
{
	int ui_gap_lt = 0;
	int is_charger_exist = get_charger_exist();
	int _car;
	int delta_car_bat0;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	_car = gauge_get_int_property(GAUGE_PROP_COULOMB);
	delta_car_bat0 = abs(algo->prev_car_bat0 - _car);
	fgr_update_uisoc_threshold(gm);

	if (algo->ui_soc > algo->soc && algo->soc >= 100) {
		ui_gap_lt = delta_car_bat0 * UNIT_TRANS_100 / algo->lt_gap;
		ui_gap_lt = ui_gap_lt * algo->ui_soc / algo->soc;
	} else
		ui_gap_lt = delta_car_bat0 * UNIT_TRANS_100 / algo->lt_gap;

	bm_debug("[%s][IN]ui_soc %d, lt_gap[%d %d] _car[%d %d %d]\n",
		__func__,
		algo->ui_soc, algo->lt_gap,
		ui_gap_lt, _car,
		algo->prev_car_bat0, delta_car_bat0);

	if (ui_gap_lt > 100)
		ui_gap_lt = 100;

	if (ui_gap_lt < 0) {
		bm_debug("[FG_ERR][%s] ui_gap_lt %d should not less than 0\n",
			__func__,
			ui_gap_lt);
		ui_gap_lt = 0;
	}

	if (ui_gap_lt > 0)
		algo->prev_car_bat0 = _car;

	if (is_charger_exist == true) {
		algo->ui_soc = algo->ui_soc - ui_gap_lt;
	} else {
		if (algo->ui_soc <= 100) {
			if (algo->ui_soc == 0)
				algo->ui_soc = 0;
			else
				algo->ui_soc = 100;
		} else {
			if ((algo->ui_soc - ui_gap_lt) < 100)
				algo->ui_soc = 100;
			else
				algo->ui_soc = algo->ui_soc - ui_gap_lt;

			if (algo->ui_soc < 100)
				algo->ui_soc = 100;
		}
	}
	fgr_update_uisoc_ht(gm);
	fgr_update_uisoc_lt(gm);

	bm_debug("[%s][OUT]ui_soc %d, ui_gap_lt:%d, _car[%d %d %d]\n",
		__func__, algo->ui_soc, ui_gap_lt,
		_car, algo->prev_car_bat0, delta_car_bat0);
}

void fgr_bat_int2_handler(struct mtk_battery *gm, int source)
{
	int _car = gauge_get_int_property(GAUGE_PROP_COULOMB);
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	bm_debug("[%s]car:%d pre_car:%d ht:%d lt:%d u_type:%d source:%d\n",
		__func__, _car, algo->prev_car_bat0,
		algo->ht_gap, algo->lt_gap, pdata->uisoc_update_type, source);
	if (_car > algo->prev_car_bat0)
		fgr_bat_int2_h_handler(gm);
	else if (_car < algo->prev_car_bat0)
		fgr_bat_int2_l_handler(gm);

}

void fgr_time_handler(struct mtk_battery *gm)
{
	int is_charger_exist = get_charger_exist();
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	bm_debug("[%s][IN] chr:%d, low_tracking:%d ui_soc:%d\n",
		__func__, is_charger_exist,
		algo->low_tracking_enable, algo->ui_soc);

	if (algo->low_tracking_enable) {
		if (is_charger_exist)
			return;

		if (is_charger_exist == false) {
			algo->ui_soc = algo->ui_soc - 100;
			if (algo->ui_soc <= 0) {
				algo->ui_soc = 0;
				algo->low_tracking_enable = 0;
			}
		}
	} else
		set_fg_time(gm, 0);
}

void fgr_vbat2_h_int_handler(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	pdata = &gm->fg_cust_data;
	gauge_set_property(GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT, false);
	algo->fg_vbat2_lt = pdata->vbat2_det_voltage1;
	gauge_set_property(GAUGE_PROP_VBAT_LT_INTR_THRESHOLD,
		algo->fg_vbat2_lt);
	gauge_set_property(GAUGE_PROP_EN_LOW_VBAT_INTERRUPT, true);
	bm_debug("[%s]fg_vbat2_lt=%d %d\n",
		__func__,
		algo->fg_vbat2_lt, algo->fg_vbat2_ht);
}

void fgr_vbat2_l_int_handler(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;

	if (algo->fg_vbat2_lt == pdata->vbat2_det_voltage1) {
		set_shutdown_cond(gm, LOW_BAT_VOLT);
		algo->fg_vbat2_lt = pdata->vbat2_det_voltage2;
		algo->fg_vbat2_ht = pdata->vbat2_det_voltage3;
		gauge_set_property(GAUGE_PROP_VBAT_LT_INTR_THRESHOLD,
			algo->fg_vbat2_lt);
		gauge_set_property(GAUGE_PROP_VBAT_HT_INTR_THRESHOLD,
			algo->fg_vbat2_ht);
		gauge_set_property(GAUGE_PROP_EN_LOW_VBAT_INTERRUPT, true);
		gauge_set_property(GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT, true);
	}
	bm_debug("[%s]fg_vbat2_lt=%d %d,[%d %d %d]\n",
		__func__,
		algo->fg_vbat2_lt, algo->fg_vbat2_ht,
		pdata->vbat2_det_voltage1,
		pdata->vbat2_det_voltage2,
		pdata->vbat2_det_voltage3);
}

void do_fg_algo(struct mtk_battery *gm, unsigned int intr_num)
{
	switch (intr_num) {
	case FG_INTR_BAT_TMP_C_HT:
		fgr_temp_c_int_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_BAT_TMP_HT);
		break;
	case FG_INTR_BAT_TMP_C_LT:
		fgr_temp_c_int_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_BAT_TMP_LT);
		break;
	case FG_INTR_BAT_INT1_HT:
		fgr_bat_int1_handler(gm);
		fgr_bat_int2_handler(gm, 0);
		fgr_int_end_flow(gm, FG_INTR_BAT_INT1_HT);
		break;
	case FG_INTR_BAT_INT1_LT:
		fgr_bat_int1_handler(gm);
		fgr_bat_int2_handler(gm, 0);
		fgr_int_end_flow(gm, FG_INTR_BAT_INT1_LT);
		break;
	case FG_INTR_BAT_INT2_HT:
		fgr_bat_int2_h_handler(gm);
		fgr_bat_int1_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_BAT_INT2_HT);
		break;
	case FG_INTR_BAT_INT2_LT:
		fgr_bat_int2_l_handler(gm);
		fgr_bat_int1_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_BAT_INT2_LT);
		break;
	case FG_INTR_FG_TIME:
		fgr_time_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_FG_TIME);
		break;
	case FG_INTR_SHUTDOWN:
		fgr_shutdown_int_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_SHUTDOWN);
		break;
	case FG_INTR_DLPT_SD:
		fgr_dlpt_sd_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_DLPT_SD);
		break;
	case FG_INTR_VBAT2_H:
		fgr_vbat2_h_int_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_VBAT2_H);
		break;
	case FG_INTR_VBAT2_L:
		fgr_vbat2_l_int_handler(gm);
		fgr_int_end_flow(gm, FG_INTR_VBAT2_L);
		break;
	}
	bm_debug("[%s] intr_num=0x%x\n", __func__, intr_num);
}

void fgr_set_int1(struct mtk_battery *gm)
{
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;
	int car_now = gauge_get_int_property(GAUGE_PROP_COULOMB);

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	fgr_update_quse(gm, 1);
	/* set c gap */
	fgr_update_fg_bat_int1_threshold(gm);
	battery_set_property(BAT_PROP_COULOMB_INT_GAP,
		algo->fg_bat_int1_gap);
	bm_debug("[%s]set cgap :fg_bat_int1_gap %d to kernel done\n",
		__func__,
		algo->fg_bat_int1_gap);

	/* set ui_soc gap*/
	algo->prev_car_bat0 = car_now;
	fgr_update_uisoc_ht(gm);
	fgr_update_uisoc_lt(gm);

	battery_set_property(BAT_PROP_ENABLE_UISOC_HT_INT, 1);
	battery_set_property(BAT_PROP_ENABLE_UISOC_LT_INT, 1);

	/*set bat tempture  */
	algo->fg_bat_tmp_c_gap = 1;
	set_fg_bat_tmp_c_gap(algo->fg_bat_tmp_c_gap);
	bm_debug("[%s]fg_bat_tmp_c_gap %d\n",
		__func__, algo->fg_bat_tmp_c_gap);

	algo->fg_vbat2_lt = pdata->vbat2_det_voltage1;
	gauge_set_property(GAUGE_PROP_VBAT_LT_INTR_THRESHOLD,
		algo->fg_vbat2_lt);
	gauge_set_property(GAUGE_PROP_EN_LOW_VBAT_INTERRUPT, true);

	set_kernel_soc(gm, algo->soc);
	battery_set_property(BAT_PROP_UISOC, algo->ui_soc);
	gauge_set_property(GAUGE_PROP_RTC_UI_SOC,
		(algo->ui_soc + 50) / 100);

	if (algo->soc <= 100)
		gauge_set_property(GAUGE_PROP_CON0_SOC, 100);
	else if (algo->soc >= 12000)
		gauge_set_property(GAUGE_PROP_CON0_SOC, 10000);
	else
		gauge_set_property(GAUGE_PROP_CON0_SOC, algo->soc);

	bm_debug("[%s] done\n", __func__);
}

void battery_algo_init(struct mtk_battery *gm)
{
	int is_bat_exist;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;

	ptable = &gm->fg_table_cust_data;
	algo = &gm->algo;
	algo->fg_bat_tmp_c_gap = 1;
	algo->aging_factor = 10000;
	algo->DC_ratio = 100;
	gauge_get_property(GAUGE_PROP_BATTERY_EXIST, &is_bat_exist);
	bm_err("MTK Battery algo init bat_exist:%d\n",
		is_bat_exist);

	if (is_bat_exist) {
		fgr_construct_table_by_temp(gm, true,
			ptable->temperature_tb1);
		fgr_construct_vboot(gm,
			ptable->temperature_tb1);
		fgr_dump_table(gm, ptable->temperature_tb1);
		fgr_dod_init(gm);
		fgr_set_int1(gm);
		battery_set_property(BAT_PROP_INIT_DONE, 1);
		gauge_set_property(GAUGE_PROP_IS_NVRAM_FAIL_MODE, 1);
	}
	bm_err("[battery_recovery] is_evb:%d is_bat_exist %d\n",
		 gm->disableGM30, is_bat_exist);
}

