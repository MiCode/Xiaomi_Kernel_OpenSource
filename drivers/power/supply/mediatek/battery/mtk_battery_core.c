/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mtk_battery.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 * This Module defines functions of the Anroid Battery service for
 * updating the battery status
 *
 * Author:
 * -------
 * Weiching Lin
 *
 ****************************************************************************/
#ifndef _DEA_MODIFY_
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/wait.h>		/* For wait queue*/
#include <linux/sched.h>	/* For wait queue*/
#include <linux/kthread.h>	/* For Kthread_run */
#include <linux/platform_device.h>	/* platform device */
#include <linux/time.h>

#include <linux/netlink.h>	/* netlink */
#include <linux/kernel.h>
#include <linux/socket.h>	/* netlink */
#include <linux/skbuff.h>	/* netlink */
#include <net/sock.h>		/* netlink */
#include <linux/cdev.h>		/* cdev */

#include <linux/err.h>	/* IS_ERR, PTR_ERR */
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/proc_fs.h>
#include <linux/of_fdt.h>	/*of_dt API*/
#include <linux/of.h>
#include <linux/vmalloc.h>
#include <linux/math64.h>
#include <linux/alarmtimer.h>

#include <mt-plat/aee.h>
#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/mtk_charger.h>
#include <mt-plat/v1/mtk_battery.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
#include <pmic_lbat_service.h>

#include <mtk_gauge_class.h>
#include "mtk_battery_internal.h"
#include <mtk_gauge_time_service.h>

#include <mach/mtk_battery_property.h>
#include <mach/mtk_battery_table.h>
#else
#include <string.h>

#include <mtk_gauge_class.h>
#include "mtk_battery_internal.h"
#include <mtk_gauge_time_service.h>
#include <mtk_battery_property.h>
#include <mtk_battery_table.h>
#include "simulator_kernel.h"
#endif



/* ============================================================ */
/* global variable */
/* ============================================================ */
struct mtk_battery gm;

/* ============================================================ */
/* gauge hal interface */
/* ============================================================ */

void __attribute__ ((weak)) 
		pmic_register_interrupt_callback(unsigned int intNo,
  		void (EINT_FUNC_PTR) (void))
{
	/*work around for mt6768*/
}

bool gauge_get_current(int *bat_current)
{
	bool is_charging = false;

	if (is_fg_disabled()) {
		*bat_current = 0;
		return is_charging;
	}

	if (get_ec()->debug_fg_curr_en == 1) {
		*bat_current = get_ec()->debug_fg_curr_value;
		return false;
	}

	if (is_battery_init_done() == false) {
		*bat_current = 0;
		return false;
	}

	gauge_dev_get_current(gm.gdev, &is_charging, bat_current);
	return is_charging;
}

int gauge_get_average_current(bool *valid)
{
	int iavg = 0;
	int ver = gauge_get_hw_version();

	if (is_fg_disabled())
		iavg = 0;
	else {
		if (ver >= GAUGE_HW_V1000 &&
			ver < GAUGE_HW_V2000)
			iavg = gm.sw_iavg;
		else
			gauge_dev_get_average_current(gm.gdev, &iavg, valid);
	}

	return iavg;
}

int gauge_get_coulomb(void)
{
	int columb = 0;

	if (is_fg_disabled())
		return columb;

	gauge_dev_get_coulomb(gm.gdev, &columb);
	return columb;
}

int gauge_reset_hw(void)
{
	if (is_fg_disabled())
		return 0;

	/* must handle sw_ncar before reset car */
	fg_sw_bat_cycle_accu();
	gm.bat_cycle_car = 0;
	gm.log.car_diff += gauge_get_coulomb();

	bm_err("%s car:%d\n",
		__func__,
		gauge_get_coulomb());


	gauge_coulomb_before_reset();
	gauge_dev_reset_hw(gm.gdev);
	gauge_coulomb_after_reset();
	get_monotonic_boottime(&gm.sw_iavg_time);
	gm.sw_iavg_car = gauge_get_coulomb();

	gm.bat_cycle_car = 0;
	zcv_filter_init(&gm.zcvf);
	return 0;
}

int gauge_reset_ncar(void)
{
	gauge_dev_reset_ncar(gm.gdev);
	gm.bat_cycle_ncar = 0;

	bm_err("%s done, %d, version:%d\n",
		__func__,
		gm.bat_cycle_ncar, gauge_get_hw_version());

	return 0;
}

int gauge_get_hwocv(void)
{
	int hwocv = 37000;

	if (is_fg_disabled())
		hwocv = 37000;
	else
		gauge_dev_get_hwocv(gm.gdev, &hwocv);

	return hwocv;
}

int gauge_set_coulomb_interrupt1_ht(int car)
{
	bm_debug("%s:%d\n",
		__func__,
		car);
	return gauge_dev_set_coulomb_interrupt1_ht(gm.gdev, car);
}

int gauge_set_coulomb_interrupt1_lt(int car)
{
	bm_debug("%s:%d\n",
		__func__,
		car);
	return gauge_dev_set_coulomb_interrupt1_lt(gm.gdev, car);
}

int gauge_get_ptim_current(int *ptim_current, bool *is_charging)
{
	gauge_dev_get_ptim_current(gm.gdev, ptim_current, is_charging);
	return 0;
}

int gauge_get_zcv_current(int *zcv_current)
{
	gauge_dev_get_zcv_current(gm.gdev, zcv_current);
	return 0;
}

int gauge_get_zcv(int *zcv)
{
	gauge_dev_get_zcv(gm.gdev, zcv);
	return 0;
}

int gauge_set_nag_en(int nafg_zcv_en)
{
	if (is_fg_disabled())
		return 0;

#if defined(CONFIG_MTK_DISABLE_GAUGE)
#else
	if (gm.disable_nafg_int == false)
		gauge_dev_enable_nag_interrupt(gm.gdev, nafg_zcv_en);
#endif
	bm_debug(
		"%s = %d\n",
		__func__,
		nafg_zcv_en);

	return 0;
}

int gauge_set_zcv_interrupt_en(int zcv_intr_en)
{
	gauge_dev_enable_zcv_interrupt(gm.gdev, zcv_intr_en);
	return 0;
}

int gauge_get_hw_version(void)
{
	return gauge_dev_get_hw_version(gm.gdev);
}

int gauge_enable_vbat_low_interrupt(int en)
{
	if (gauge_get_hw_version() >= GAUGE_HW_V2000) {
		gauge_dev_enable_vbat_low_interrupt(gm.gdev, en);
	} else {
		mutex_lock(&gm.sw_low_battery_mutex);
		gm.sw_low_battery_lt_en = en;
		mutex_unlock(&gm.sw_low_battery_mutex);
	}
	return 0;
}

int gauge_enable_vbat_high_interrupt(int en)
{
	if (gauge_get_hw_version() >= GAUGE_HW_V2000) {
		gauge_dev_enable_vbat_high_interrupt(gm.gdev, en);
	} else {
		mutex_lock(&gm.sw_low_battery_mutex);
		gm.sw_low_battery_ht_en = en;
		mutex_unlock(&gm.sw_low_battery_mutex);
	}
	return 0;
}

int gauge_set_vbat_low_threshold(int threshold)
{
	if (gauge_get_hw_version() >= GAUGE_HW_V2000) {
		gauge_dev_set_vbat_low_threshold(gm.gdev, threshold);
	} else {
		mutex_lock(&gm.sw_low_battery_mutex);
		gm.sw_low_battery_lt_threshold = threshold;
		mutex_unlock(&gm.sw_low_battery_mutex);
	}
	return 0;
}

int gauge_set_vbat_high_threshold(int threshold)
{
	if (gauge_get_hw_version() >= GAUGE_HW_V2000) {
		gauge_dev_set_vbat_high_threshold(gm.gdev, threshold);
	} else {
		mutex_lock(&gm.sw_low_battery_mutex);
		gm.sw_low_battery_ht_threshold = threshold;
		mutex_unlock(&gm.sw_low_battery_mutex);
	}
	return 0;
}

int gauge_enable_iavg_interrupt(bool ht_en, int ht_th,
	bool lt_en, int lt_th)
{
	return gauge_dev_enable_iavg_interrupt(
		gm.gdev, ht_en, ht_th, lt_en, lt_th);
}

int gauge_get_nag_vbat(void)
{
	int nafg_vbat = 0;

	gauge_dev_get_nag_vbat(gm.gdev, &nafg_vbat);
	return nafg_vbat;
}

int gauge_get_nag_cnt(void)
{
	int nafg_cnt = 0;

	gauge_dev_get_nag_cnt(gm.gdev, &nafg_cnt);
	return nafg_cnt;
}

int gauge_get_nag_c_dltv(void)
{
	int nafg_c_dltv = 0;

	gauge_dev_get_nag_c_dltv(gm.gdev, &nafg_c_dltv);
	return nafg_c_dltv;
}

int gauge_get_nag_dltv(void)
{
	int nafg_dltv = 0;

	gauge_dev_get_nag_dltv(gm.gdev, &nafg_dltv);
	return nafg_dltv;
}

/* ============================================================ */
/* battery health methods */
/* ============================================================ */
int mtk_get_bat_health(void)
{
	if (gm.bat_health != 0)
		return gm.bat_health;

	return 10000;
}

int mtk_get_bat_show_ag(void)
{
	if (gm.show_ag != 0)
		return gm.show_ag;

	return 10000;
}

/* ============================================================ */
/* zcv filter methods */
/* ============================================================ */
void zcv_filter_init(struct zcv_filter *zf)
{
	memset(zf->log, 0, sizeof(struct zcv_log) * ZCV_LOG_LEN);
	zf->fidx = -1;
	zf->lidx = -1;
	zf->size = 0;
	zf->zcvtime = 16 * 60;
	zf->zcvcurrent = 20;
}

int zcv_add(struct zcv_filter *zf, int car, struct timespec *t)
{
	int nidx;
	int ret = 0;

	nidx = (zf->lidx+1) % ZCV_LOG_LEN;

	bm_debug("badd fidx:%d lidx:%d nidx:%d\n",
			zf->fidx,
			zf->lidx,
			nidx);

	if (zf->fidx == -1) {
		/* array empty */
		zf->fidx = 0;
		zf->lidx = 0;
		zf->size++;
		zf->log[0].car = car;
		zf->log[0].time = *t;
	} else {
		if (nidx == zf->fidx) /* array full */
			ret = -1;
		else {
		/* array not full */
			zf->lidx = nidx;
			zf->log[nidx].car = car;
			zf->log[nidx].time = *t;
			zf->log[nidx].dtime = 0;
			zf->log[nidx].dcar = 0;
			zf->log[nidx].avgcurrent = 0;
			zf->size++;
		}
	}

	bm_debug("aadd fidx:%d lidx:%d nidx:%d\n",
			zf->fidx,
			zf->lidx,
			nidx);
	return ret;
}

void zcv_remove(struct zcv_filter *zf)
{
	int nidx;

	nidx = (zf->fidx+1)%ZCV_LOG_LEN;
	if (zf->fidx == -1)
		return;

	zf->log[zf->fidx].car = 0;
	zf->log[zf->fidx].time.tv_sec = 0;
	zf->log[zf->fidx].time.tv_nsec = 0;
	zf->log[zf->fidx].dtime = 0;
	zf->log[zf->fidx].dcar = 0;
	zf->log[zf->fidx].avgcurrent = 0;
	if (zf->fidx == zf->lidx) {
		zf->fidx = -1;
		zf->lidx = -1;
		zf->size = 0;
	} else {
		zf->fidx = nidx;
		zf->size--;
	}
}

void zcv_filter_remove(struct zcv_filter *zf)
{
	zcv_remove(zf);
}

int zcv_filter_add(struct zcv_filter *zf)
{
	int ret = 0;
	struct zcv_log *old;
	struct timespec dtime, now;
	int dcar = 0;
	int avgc = 0;
	int time_thread = 0;
	int avgc_thread = 0;
	int car = 0;

	car = gauge_get_coulomb();
	get_monotonic_boottime(&now);

	dtime.tv_sec = 0;
	dtime.tv_nsec = 0;

	time_thread = (fg_cust_data.zcv_suspend_time + 1) * 4 * 60 / 20;
	avgc_thread = fg_cust_data.sleep_current_avg / 10;

	if (zf->lidx != -1) {
		old = &zf->log[zf->lidx];
		dtime = timespec_sub(now, old->time);
		dcar = abs(car - old->car);
		if (dtime.tv_sec != 0)
			avgc = dcar * 360 / dtime.tv_sec;
		else
			avgc = 0;

		if (dtime.tv_sec < time_thread) {
			bm_err("zcvf, no update time:%ld %d avgc:%d avgc_thread\n",
				(long)dtime.tv_sec, time_thread,
				avgc, avgc_thread);
			return 0;
		}
	}

	ret = zcv_add(zf, car, &now);
	if (ret == -1) {
		zcv_remove(zf);
		ret = zcv_add(zf, car, &now);
	}

	zf->log[zf->lidx].dcar = dcar;
	zf->log[zf->lidx].dtime = dtime.tv_sec;
	zf->log[zf->lidx].avgcurrent = avgc;

	return 0;
}

void zcv_filter_dump(struct zcv_filter *zf)
{
	int i;
	struct timespec dtime;
	struct timespec now_time;

	get_monotonic_boottime(&now_time);
	dtime.tv_sec = 0;
	dtime.tv_nsec = 0;
	bm_debug("zcvf dump:fidx:%d lidx:%d size:%d now:%ld zcvt:%d avgc:%d\n",
			zf->fidx,
			zf->lidx,
			zf->size,
			now_time.tv_sec,
			(fg_cust_data.zcv_suspend_time + 1) * 4 * 60,
			fg_cust_data.sleep_current_avg / 10);
	for (i = 0; i < ZCV_LOG_LEN; i++) {
		dtime = timespec_sub(now_time, zf->log[i].time);
		bm_debug("zcvf idx:%d car:%d avgc:%d dcar:%d time:%ld dtime_p:%d dtime_n:%ld\n",
			i,
			zf->log[i].car,
			zf->log[i].avgcurrent,
			zf->log[i].dcar,
			zf->log[i].time.tv_sec,
			zf->log[i].dtime,
			dtime.tv_sec);
	}
}

bool zcv_check(struct zcv_filter *zf)
{
	struct timespec now_time, dtime;
	int idx = 0;
	int time_thread = 0;
	int avgc_thread = 0;
	int i = 0;
	int dcar = 0;
	int avgc = 0;
	struct zcv_log *log;
	int fg_coulomb = gauge_get_coulomb();
	bool oc = false;
	int i_intime = -1;

	get_monotonic_boottime(&now_time);
	time_thread = (fg_cust_data.zcv_suspend_time + 1) * 4 * 60;
	avgc_thread = fg_cust_data.sleep_current_avg / 10 * 3 / 2;


	for (i = 0; i < zf->size; i++) {
		idx = zf->fidx;
		idx = (idx + i) % ZCV_LOG_LEN;
		log = &zf->log[idx];
		dtime = timespec_sub(now_time, log->time);

		bm_debug("zcvf i:%d i_intime:%d idx:%d dtime_now:%ld avgc_prev:%d car:%d %d ot:%d\n",
			i, i_intime, idx, dtime.tv_sec, log->avgcurrent,
			log->car, fg_coulomb,
			dtime.tv_sec > time_thread);
		if (dtime.tv_sec <= time_thread) {
			if (i_intime == -1)
				i_intime = i;

			if (i_intime != i && log->avgcurrent > avgc_thread) {
				oc = true;
				bm_debug("zcvf (1) idx:%d,%d avgc_prev:%d avgc_t:%d\n",
					idx, i, log->avgcurrent, avgc_thread);
			} else {
				dcar = abs(fg_coulomb - log->car);
				if (dtime.tv_sec != 0)
					avgc = dcar * 360 / dtime.tv_sec;
				else
					avgc = 0;
				if (avgc > avgc_thread) {
					oc = true;
					bm_debug("zcvf (2) idx:%d:%d avgc_pre:%d avgc_now:%d avgc_t:%d cou:%d %d\n",
						idx, i, log->avgcurrent, avgc, avgc_thread,
						fg_coulomb, log->car);
				}
			}
		}
	}

	bm_debug("zcvf check:%d\n", oc);

	return oc;
}


/* ============================================================ */
/* weak function for other module */
/* ============================================================ */
bool __attribute__ ((weak)) mt_usb_is_device(void)
{
	pr_notice_once("%s: usb is not ready\n", __func__);
	return false;
}

/* ============================================================ */
/* custom setting */
/* ============================================================ */
#ifdef MTK_GET_BATTERY_ID_BY_AUXADC
void fgauge_get_profile_id(void)
{
	int id_volt = 0;
	int id = 0;
	int ret = 0;
	int auxadc_voltage = 0;
	struct iio_channel *channel;
	struct device_node *batterty_node;
	struct platform_device *battery_dev;

	batterty_node = of_find_node_by_name(NULL, "battery");
	if (!batterty_node) {
		bm_err("[%s] of_find_node_by_name fail\n", __func__);
		return;
	}

	battery_dev = of_find_device_by_node(batterty_node);
	if (!battery_dev) {
		bm_err("[%s] of_find_device_by_node fail\n", __func__);
		return;
	}

	channel = iio_channel_get(&(battery_dev->dev), "batteryID-channel");
	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		bm_err("[%s] iio channel not found %d\n",
		__func__, ret);
		return;
	}

	if (channel)
		ret = iio_read_channel_processed(channel, &auxadc_voltage);


	if (ret <= 0) {
		bm_err("[%s] iio_read_channel_processed failed\n", __func__);
		return;
	}

	bm_err("[%s]auxadc_voltage is %d\n", __func__, auxadc_voltage);
	id_volt = auxadc_voltage * 1500 / 4096;
	bm_err("[%s]battery_id_voltage is %d\n", __func__, id_volt);

	if ((sizeof(g_battery_id_voltage) /
		sizeof(int)) != TOTAL_BATTERY_NUMBER) {
		bm_debug("[%s]error! voltage range incorrect!\n",
			__func__);
		return;
	}

	for (id = 0; id < TOTAL_BATTERY_NUMBER; id++) {
		if (id_volt < g_battery_id_voltage[id]) {
			gm.battery_id = id;
			break;
		} else if (g_battery_id_voltage[id] == -1) {
			gm.battery_id = TOTAL_BATTERY_NUMBER - 1;
		}
	}

	bm_debug("[%s]Battery id (%d)\n",
		__func__,
		gm.battery_id);
}
#elif defined(MTK_GET_BATTERY_ID_BY_GPIO)
void fgauge_get_profile_id(void)
{
	gm.battery_id = 0;
}
#else
void fgauge_get_profile_id(void)
{
	if (get_ec()->debug_bat_id_en == 1)
		gm.battery_id = get_ec()->debug_bat_id_value;
	else
		gm.battery_id = BATTERY_PROFILE_ID;

	bm_err("[%s]Battery id=(%d) en:%d,%d\n",
		__func__,
		gm.battery_id, get_ec()->debug_bat_id_en,
		get_ec()->debug_bat_id_value);
}
#endif

void fg_custom_init_from_header(void)
{
	int i, j;

	fgauge_get_profile_id();

	fg_cust_data.versionID1 = FG_DAEMON_CMD_FROM_USER_NUMBER;
	fg_cust_data.versionID2 = sizeof(fg_cust_data);
	fg_cust_data.versionID3 = FG_KERNEL_CMD_FROM_USER_NUMBER;
	fg_cust_data.fg_get_max = FG_GET_MAX;
	fg_cust_data.fg_set_max = FG_SET_DATA_MAX;

	if (gm.gdev != NULL) {
		fg_cust_data.hardwareVersion = gauge_get_hw_version();
		fg_cust_data.pl_charger_status =
			gm.hw_status.pl_charger_status;
	}

	fg_cust_data.daemon_log_level = BM_DAEMON_DEFAULT_LOG_LEVEL;
	fg_cust_data.q_max_L_current = Q_MAX_L_CURRENT;
	fg_cust_data.q_max_H_current = Q_MAX_H_CURRENT;
	fg_cust_data.q_max_sys_voltage =
		UNIT_TRANS_10 * g_Q_MAX_SYS_VOLTAGE[gm.battery_id];

	fg_cust_data.pseudo1_en = PSEUDO1_EN;
	fg_cust_data.pseudo100_en = PSEUDO100_EN;
	fg_cust_data.pseudo100_en_dis = PSEUDO100_EN_DIS;
	fg_cust_data.pseudo1_iq_offset = UNIT_TRANS_100 *
		g_FG_PSEUDO1_OFFSET[gm.battery_id];

	/* iboot related */
	fg_cust_data.qmax_sel = QMAX_SEL;
	fg_cust_data.iboot_sel = IBOOT_SEL;
	fg_cust_data.shutdown_system_iboot = SHUTDOWN_SYSTEM_IBOOT;

	/* multi-temp gague 0% related */
	fg_cust_data.multi_temp_gauge0 = MULTI_TEMP_GAUGE0;

	/*hw related */
	fg_cust_data.car_tune_value = UNIT_TRANS_10 * CAR_TUNE_VALUE;
	fg_cust_data.fg_meter_resistance = FG_METER_RESISTANCE;
	fg_cust_data.com_fg_meter_resistance = FG_METER_RESISTANCE;
	fg_cust_data.r_fg_value = UNIT_TRANS_10 * R_FG_VALUE;
	fg_cust_data.com_r_fg_value = UNIT_TRANS_10 * R_FG_VALUE;

	/* Aging Compensation */
	fg_cust_data.aging_one_en = AGING_ONE_EN;
	fg_cust_data.aging1_update_soc = UNIT_TRANS_100 * AGING1_UPDATE_SOC;
	fg_cust_data.aging1_load_soc = UNIT_TRANS_100 * AGING1_LOAD_SOC;
	fg_cust_data.aging4_update_soc = UNIT_TRANS_100 * AGING4_UPDATE_SOC;
	fg_cust_data.aging4_load_soc = UNIT_TRANS_100 * AGING4_LOAD_SOC;
	fg_cust_data.aging5_update_soc = UNIT_TRANS_100 * AGING5_UPDATE_SOC;
	fg_cust_data.aging5_load_soc = UNIT_TRANS_100 * AGING5_LOAD_SOC;
	fg_cust_data.aging6_update_soc = UNIT_TRANS_100 * AGING6_UPDATE_SOC;
	fg_cust_data.aging6_load_soc = UNIT_TRANS_100 * AGING6_LOAD_SOC;
	fg_cust_data.aging_temp_diff = AGING_TEMP_DIFF;
	fg_cust_data.aging_temp_low_limit = AGING_TEMP_LOW_LIMIT;
	fg_cust_data.aging_temp_high_limit = AGING_TEMP_HIGH_LIMIT;
	fg_cust_data.aging_100_en = AGING_100_EN;
	fg_cust_data.difference_voltage_update = DIFFERENCE_VOLTAGE_UPDATE;
	fg_cust_data.aging_factor_min = UNIT_TRANS_100 * AGING_FACTOR_MIN;
	fg_cust_data.aging_factor_diff = UNIT_TRANS_100 * AGING_FACTOR_DIFF;
	/* Aging Compensation 2*/
	fg_cust_data.aging_two_en = AGING_TWO_EN;
	/* Aging Compensation 3*/
	fg_cust_data.aging_third_en = AGING_THIRD_EN;
	fg_cust_data.aging_4_en = AGING_4_EN;
	fg_cust_data.aging_5_en = AGING_5_EN;
	fg_cust_data.aging_6_en = AGING_6_EN;


	/* ui_soc related */
	fg_cust_data.diff_soc_setting = DIFF_SOC_SETTING;
	fg_cust_data.keep_100_percent = UNIT_TRANS_100 * KEEP_100_PERCENT;
	fg_cust_data.difference_full_cv = DIFFERENCE_FULL_CV;
	fg_cust_data.diff_bat_temp_setting = DIFF_BAT_TEMP_SETTING;
	fg_cust_data.diff_bat_temp_setting_c = DIFF_BAT_TEMP_SETTING_C;
	fg_cust_data.discharge_tracking_time = DISCHARGE_TRACKING_TIME;
	fg_cust_data.charge_tracking_time = CHARGE_TRACKING_TIME;
	fg_cust_data.difference_fullocv_vth = DIFFERENCE_FULLOCV_VTH;
	fg_cust_data.difference_fullocv_ith =
		UNIT_TRANS_10 * DIFFERENCE_FULLOCV_ITH;
	fg_cust_data.charge_pseudo_full_level = CHARGE_PSEUDO_FULL_LEVEL;
	fg_cust_data.over_discharge_level = OVER_DISCHARGE_LEVEL;
	fg_cust_data.full_tracking_bat_int2_multiply =
		FULL_TRACKING_BAT_INT2_MULTIPLY;

	/* pre tracking */
	fg_cust_data.fg_pre_tracking_en = FG_PRE_TRACKING_EN;
	fg_cust_data.vbat2_det_time = VBAT2_DET_TIME;
	fg_cust_data.vbat2_det_counter = VBAT2_DET_COUNTER;
	fg_cust_data.vbat2_det_voltage1 = VBAT2_DET_VOLTAGE1;
	fg_cust_data.vbat2_det_voltage2 = VBAT2_DET_VOLTAGE2;
	fg_cust_data.vbat2_det_voltage3 = VBAT2_DET_VOLTAGE3;

	/* sw fg */
	fg_cust_data.difference_fgc_fgv_th1 = DIFFERENCE_FGC_FGV_TH1;
	fg_cust_data.difference_fgc_fgv_th2 = DIFFERENCE_FGC_FGV_TH2;
	fg_cust_data.difference_fgc_fgv_th3 = DIFFERENCE_FGC_FGV_TH3;
	fg_cust_data.difference_fgc_fgv_th_soc1 = DIFFERENCE_FGC_FGV_TH_SOC1;
	fg_cust_data.difference_fgc_fgv_th_soc2 = DIFFERENCE_FGC_FGV_TH_SOC2;
	fg_cust_data.nafg_time_setting = NAFG_TIME_SETTING;
	fg_cust_data.nafg_ratio = NAFG_RATIO;
	fg_cust_data.nafg_ratio_en = NAFG_RATIO_EN;
	fg_cust_data.nafg_ratio_tmp_thr = NAFG_RATIO_TMP_THR;
	fg_cust_data.nafg_resistance = NAFG_RESISTANCE;

	/* ADC resistor  */
	fg_cust_data.r_charger_1 = R_CHARGER_1;
	fg_cust_data.r_charger_2 = R_CHARGER_2;

	/* mode select */
	fg_cust_data.pmic_shutdown_current = PMIC_SHUTDOWN_CURRENT;
	fg_cust_data.pmic_shutdown_sw_en = PMIC_SHUTDOWN_SW_EN;
	fg_cust_data.force_vc_mode = FORCE_VC_MODE;
	fg_cust_data.embedded_sel = EMBEDDED_SEL;
	fg_cust_data.loading_1_en = LOADING_1_EN;
	fg_cust_data.loading_2_en = LOADING_2_EN;
	fg_cust_data.diff_iavg_th = DIFF_IAVG_TH;

	fg_cust_data.shutdown_gauge0 = SHUTDOWN_GAUGE0;
	fg_cust_data.shutdown_1_time = SHUTDOWN_1_TIME;
	fg_cust_data.shutdown_gauge1_xmins = SHUTDOWN_GAUGE1_XMINS;
	fg_cust_data.shutdown_gauge0_voltage = SHUTDOWN_GAUGE0_VOLTAGE;
	fg_cust_data.shutdown_gauge1_vbat_en = SHUTDOWN_GAUGE1_VBAT_EN;
	fg_cust_data.shutdown_gauge1_vbat = SHUTDOWN_GAUGE1_VBAT;
	fg_cust_data.power_on_car_chr = POWER_ON_CAR_CHR;
	fg_cust_data.power_on_car_nochr = POWER_ON_CAR_NOCHR;
	fg_cust_data.shutdown_car_ratio = SHUTDOWN_CAR_RATIO;

	/* ZCV update */
	fg_cust_data.zcv_suspend_time = ZCV_SUSPEND_TIME;
	fg_cust_data.sleep_current_avg = SLEEP_CURRENT_AVG;
	fg_cust_data.zcv_com_vol_limit = ZCV_COM_VOL_LIMIT;
	fg_cust_data.zcv_car_gap_percentage = ZCV_CAR_GAP_PERCENTAGE;

	/* dod_init */
	fg_cust_data.hwocv_oldocv_diff = HWOCV_OLDOCV_DIFF;
	fg_cust_data.hwocv_oldocv_diff_chr = HWOCV_OLDOCV_DIFF_CHR;
	fg_cust_data.hwocv_swocv_diff = HWOCV_SWOCV_DIFF;
	fg_cust_data.hwocv_swocv_diff_lt = HWOCV_SWOCV_DIFF_LT;
	fg_cust_data.hwocv_swocv_diff_lt_temp = HWOCV_SWOCV_DIFF_LT_TEMP;
	fg_cust_data.swocv_oldocv_diff = SWOCV_OLDOCV_DIFF;
	fg_cust_data.swocv_oldocv_diff_chr = SWOCV_OLDOCV_DIFF_CHR;
	fg_cust_data.vbat_oldocv_diff = VBAT_OLDOCV_DIFF;
	fg_cust_data.swocv_oldocv_diff_emb = SWOCV_OLDOCV_DIFF_EMB;
	fg_cust_data.vir_oldocv_diff_emb = VIR_OLDOCV_DIFF_EMB;
	fg_cust_data.vir_oldocv_diff_emb_lt = VIR_OLDOCV_DIFF_EMB_LT;
	fg_cust_data.vir_oldocv_diff_emb_tmp = VIR_OLDOCV_DIFF_EMB_TMP;

	fg_cust_data.pmic_shutdown_time = UNIT_TRANS_60 * PMIC_SHUTDOWN_TIME;
	fg_cust_data.tnew_told_pon_diff = TNEW_TOLD_PON_DIFF;
	fg_cust_data.tnew_told_pon_diff2 = TNEW_TOLD_PON_DIFF2;
	gm.ext_hwocv_swocv = EXT_HWOCV_SWOCV;
	gm.ext_hwocv_swocv_lt = EXT_HWOCV_SWOCV_LT;
	gm.ext_hwocv_swocv_lt_temp = EXT_HWOCV_SWOCV_LT_TEMP;

	fg_cust_data.dc_ratio_sel = DC_RATIO_SEL;
	fg_cust_data.dc_r_cnt = DC_R_CNT;

	fg_cust_data.pseudo1_sel = PSEUDO1_SEL;

	fg_cust_data.d0_sel = D0_SEL;
	fg_cust_data.dlpt_ui_remap_en = DLPT_UI_REMAP_EN;

	fg_cust_data.aging_sel = AGING_SEL;
	fg_cust_data.bat_par_i = BAT_PAR_I;

	fg_cust_data.fg_tracking_current = FG_TRACKING_CURRENT;
	fg_cust_data.fg_tracking_current_iboot_en =
		FG_TRACKING_CURRENT_IBOOT_EN;
	fg_cust_data.ui_fast_tracking_en = UI_FAST_TRACKING_EN;
	fg_cust_data.ui_fast_tracking_gap = UI_FAST_TRACKING_GAP;

	fg_cust_data.bat_plug_out_time = BAT_PLUG_OUT_TIME;
	fg_cust_data.keep_100_percent_minsoc = KEEP_100_PERCENT_MINSOC;

	fg_cust_data.uisoc_update_type = UISOC_UPDATE_TYPE;

	fg_cust_data.battery_tmp_to_disable_gm30 = BATTERY_TMP_TO_DISABLE_GM30;
	fg_cust_data.battery_tmp_to_disable_nafg = BATTERY_TMP_TO_DISABLE_NAFG;
	fg_cust_data.battery_tmp_to_enable_nafg = BATTERY_TMP_TO_ENABLE_NAFG;

	fg_cust_data.low_temp_mode = LOW_TEMP_MODE;
	fg_cust_data.low_temp_mode_temp = LOW_TEMP_MODE_TEMP;

	/* current limit for uisoc 100% */
	fg_cust_data.ui_full_limit_en = UI_FULL_LIMIT_EN;
	fg_cust_data.ui_full_limit_soc0 = UI_FULL_LIMIT_SOC0;
	fg_cust_data.ui_full_limit_ith0 = UI_FULL_LIMIT_ITH0;
	fg_cust_data.ui_full_limit_soc1 = UI_FULL_LIMIT_SOC1;
	fg_cust_data.ui_full_limit_ith1 = UI_FULL_LIMIT_ITH1;
	fg_cust_data.ui_full_limit_soc2 = UI_FULL_LIMIT_SOC2;
	fg_cust_data.ui_full_limit_ith2 = UI_FULL_LIMIT_ITH2;
	fg_cust_data.ui_full_limit_soc3 = UI_FULL_LIMIT_SOC3;
	fg_cust_data.ui_full_limit_ith3 = UI_FULL_LIMIT_ITH3;
	fg_cust_data.ui_full_limit_soc4 = UI_FULL_LIMIT_SOC4;
	fg_cust_data.ui_full_limit_ith4 = UI_FULL_LIMIT_ITH4;
	fg_cust_data.ui_full_limit_time = UI_FULL_LIMIT_TIME;

	fg_cust_data.ui_full_limit_fc_soc0 = UI_FULL_LIMIT_FC_SOC0;
	fg_cust_data.ui_full_limit_fc_ith0 = UI_FULL_LIMIT_FC_ITH0;
	fg_cust_data.ui_full_limit_fc_soc1 = UI_FULL_LIMIT_FC_SOC1;
	fg_cust_data.ui_full_limit_fc_ith1 = UI_FULL_LIMIT_FC_ITH1;
	fg_cust_data.ui_full_limit_fc_soc2 = UI_FULL_LIMIT_FC_SOC2;
	fg_cust_data.ui_full_limit_fc_ith2 = UI_FULL_LIMIT_FC_ITH2;
	fg_cust_data.ui_full_limit_fc_soc3 = UI_FULL_LIMIT_FC_SOC3;
	fg_cust_data.ui_full_limit_fc_ith3 = UI_FULL_LIMIT_FC_ITH3;
	fg_cust_data.ui_full_limit_fc_soc4 = UI_FULL_LIMIT_FC_SOC4;
	fg_cust_data.ui_full_limit_fc_ith4 = UI_FULL_LIMIT_FC_ITH4;

	/* voltage limit for uisoc 1% */
	fg_cust_data.ui_low_limit_en = UI_LOW_LIMIT_EN;
	fg_cust_data.ui_low_limit_soc0 = UI_LOW_LIMIT_SOC0;
	fg_cust_data.ui_low_limit_vth0 = UI_LOW_LIMIT_VTH0;
	fg_cust_data.ui_low_limit_soc1 = UI_LOW_LIMIT_SOC1;
	fg_cust_data.ui_low_limit_vth1 = UI_LOW_LIMIT_VTH1;
	fg_cust_data.ui_low_limit_soc2 = UI_LOW_LIMIT_SOC2;
	fg_cust_data.ui_low_limit_vth2 = UI_LOW_LIMIT_VTH2;
	fg_cust_data.ui_low_limit_soc3 = UI_LOW_LIMIT_SOC3;
	fg_cust_data.ui_low_limit_vth3 = UI_LOW_LIMIT_VTH3;
	fg_cust_data.ui_low_limit_soc4 = UI_LOW_LIMIT_SOC4;
	fg_cust_data.ui_low_limit_vth4 = UI_LOW_LIMIT_VTH4;
	fg_cust_data.ui_low_limit_time = UI_LOW_LIMIT_TIME;

	fg_cust_data.moving_battemp_en = MOVING_BATTEMP_EN;
	fg_cust_data.moving_battemp_thr = MOVING_BATTEMP_THR;

#if defined(GM30_DISABLE_NAFG)
		fg_cust_data.disable_nafg = 1;
#else
		fg_cust_data.disable_nafg = 0;
#endif

	if (gauge_get_hw_version() == GAUGE_HW_V2001) {
		bm_err("GAUGE_HW_V2001 disable nafg\n");
		fg_cust_data.disable_nafg = 1;
	}

	fg_table_cust_data.active_table_number = ACTIVE_TABLE;

#if defined(CONFIG_MTK_ADDITIONAL_BATTERY_TABLE)
	if (fg_table_cust_data.active_table_number == 0)
		fg_table_cust_data.active_table_number = 5;
#else
	if (fg_table_cust_data.active_table_number == 0)
		fg_table_cust_data.active_table_number = 4;
#endif

	bm_err("fg active table:%d\n",
		fg_table_cust_data.active_table_number);


	fg_table_cust_data.temperature_tb0 = TEMPERATURE_TB0;
	fg_table_cust_data.temperature_tb1 = TEMPERATURE_TB1;

	fg_table_cust_data.fg_profile[0].size =
		sizeof(fg_profile_t0[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[0].fg_profile,
			&fg_profile_t0[gm.battery_id],
			sizeof(fg_profile_t0[gm.battery_id]));

	fg_table_cust_data.fg_profile[1].size =
		sizeof(fg_profile_t1[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[1].fg_profile,
			&fg_profile_t1[gm.battery_id],
			sizeof(fg_profile_t1[gm.battery_id]));

	fg_table_cust_data.fg_profile[2].size =
		sizeof(fg_profile_t2[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[2].fg_profile,
			&fg_profile_t2[gm.battery_id],
			sizeof(fg_profile_t2[gm.battery_id]));

	fg_table_cust_data.fg_profile[3].size =
		sizeof(fg_profile_t3[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[3].fg_profile,
			&fg_profile_t3[gm.battery_id],
			sizeof(fg_profile_t3[gm.battery_id]));

	fg_table_cust_data.fg_profile[4].size =
		sizeof(fg_profile_t4[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[4].fg_profile,
			&fg_profile_t4[gm.battery_id],
			sizeof(fg_profile_t4[gm.battery_id]));

	fg_table_cust_data.fg_profile[5].size =
		sizeof(fg_profile_t5[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[5].fg_profile,
			&fg_profile_t5[gm.battery_id],
			sizeof(fg_profile_t5[gm.battery_id]));

	fg_table_cust_data.fg_profile[6].size =
		sizeof(fg_profile_t6[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[6].fg_profile,
			&fg_profile_t6[gm.battery_id],
			sizeof(fg_profile_t6[gm.battery_id]));

	fg_table_cust_data.fg_profile[7].size =
		sizeof(fg_profile_t7[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[7].fg_profile,
			&fg_profile_t7[gm.battery_id],
			sizeof(fg_profile_t7[gm.battery_id]));

	fg_table_cust_data.fg_profile[8].size =
		sizeof(fg_profile_t8[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[8].fg_profile,
			&fg_profile_t8[gm.battery_id],
			sizeof(fg_profile_t8[gm.battery_id]));

	fg_table_cust_data.fg_profile[9].size =
		sizeof(fg_profile_t9[gm.battery_id]) /
		sizeof(struct FUELGAUGE_PROFILE_STRUCT);

	memcpy(&fg_table_cust_data.fg_profile[9].fg_profile,
			&fg_profile_t9[gm.battery_id],
			sizeof(fg_profile_t9[gm.battery_id]));

	for (i = 0; i < MAX_TABLE; i++) {
		struct FUELGAUGE_PROFILE_STRUCT *p;

		p = &fg_table_cust_data.fg_profile[i].fg_profile[0];

		fg_table_cust_data.fg_profile[i].temperature =
			g_temperature[i];
		fg_table_cust_data.fg_profile[i].q_max =
			g_Q_MAX[i][gm.battery_id];
		fg_table_cust_data.fg_profile[i].q_max_h_current =
			g_Q_MAX_H_CURRENT[i][gm.battery_id];
		fg_table_cust_data.fg_profile[i].pseudo1 =
			UNIT_TRANS_100 * g_FG_PSEUDO1[i][gm.battery_id];
		fg_table_cust_data.fg_profile[i].pseudo100 =
			UNIT_TRANS_100 * g_FG_PSEUDO100[i][gm.battery_id];
		fg_table_cust_data.fg_profile[i].pmic_min_vol =
			g_PMIC_MIN_VOL[i][gm.battery_id];
		fg_table_cust_data.fg_profile[i].pon_iboot =
			g_PON_SYS_IBOOT[i][gm.battery_id];
		fg_table_cust_data.fg_profile[i].qmax_sys_vol =
			g_QMAX_SYS_VOL[i][gm.battery_id];
		/* shutdown_hl_zcv */
		fg_table_cust_data.fg_profile[i].shutdown_hl_zcv =
			g_SHUTDOWN_HL_ZCV[i][gm.battery_id];

		for (j = 0; j < 100; j++)
			if (p[j].charge_r.rdc[0] == 0)
				p[j].charge_r.rdc[0] = p[j].resistance;

	}


	/* fg_custom_init_dump(); */

	/* init battery temperature table */
	gm.rbat.type = 10;
	gm.rbat.rbat_pull_up_r = RBAT_PULL_UP_R;
	gm.rbat.rbat_pull_up_volt = RBAT_PULL_UP_VOLT;
	gm.rbat.bif_ntc_r = BIF_NTC_R;

	if (IS_ENABLED(BAT_NTC_47)) {
		gm.rbat.type = 47;
		gm.rbat.rbat_pull_up_r = RBAT_PULL_UP_R;
	}

}


#ifdef CONFIG_OF
static int fg_read_dts_val(const struct device_node *np,
		const char *node_srting,
		int *param, int unit)
{
	static unsigned int val;

	if (!of_property_read_u32(np, node_srting, &val)) {
		*param = (int)val * unit;
		bm_debug("Get %s: %d\n",
			 node_srting, *param);
	} else {
		bm_debug("Get %s failed\n", node_srting);
		return -1;
	}
	return 0;
}

static int fg_read_dts_val_by_idx(const struct device_node *np,
		const char *node_srting,
		int idx, int *param, int unit)
{
	unsigned int val;

	if (!of_property_read_u32_index(np, node_srting, idx, &val)) {
		*param = (int)val * unit;
		bm_debug("Get %s %d: %d\n",
			 node_srting, idx, *param);
	} else {
		bm_debug("Get %s failed, idx %d\n", node_srting, idx);
		return -1;
	}
	return 0;
}

static void fg_custom_parse_table(const struct device_node *np,
		const char *node_srting,
		struct FUELGAUGE_PROFILE_STRUCT *profile_struct, int column)
{
	int mah, voltage, resistance, idx, saddles;
	int i = 0, charge_rdc[MAX_CHARGE_RDC];

	struct FUELGAUGE_PROFILE_STRUCT *profile_p;

	profile_p = profile_struct;

	saddles = fg_table_cust_data.fg_profile[0].size;
	idx = 0;

	bm_err("%s: %s, %d, column:%d\n",
		__func__,
		node_srting, saddles, column);

	while (!of_property_read_u32_index(np, node_srting, idx, &mah)) {
		idx++;
		if (!of_property_read_u32_index(
			np, node_srting, idx, &voltage)) {
		}
		idx++;
		if (!of_property_read_u32_index(
				np, node_srting, idx, &resistance)) {
		}
		idx++;


		if (column == 3) {
			for (i = 0; i < MAX_CHARGE_RDC; i++)
				charge_rdc[i] = resistance;
		} else if (column >= 4) {
			if (!of_property_read_u32_index(
				np, node_srting, idx, &charge_rdc[0]))
				idx++;
		}

		/* read more for column >4 case */
		if (column > 4) {
			for (i = 1; i <= column - 4; i++) {
				if (!of_property_read_u32_index(
					np, node_srting, idx, &charge_rdc[i]))
					idx++;
			}
		}

		bm_debug("%s: mah: %d, voltage: %d, resistance: %d, rdc0:%d rdc:%d %d %d %d\n",
			__func__, mah, voltage, resistance, charge_rdc[0],
			charge_rdc[1], charge_rdc[2], charge_rdc[3], charge_rdc[4]);

		profile_p->mah = mah;
		profile_p->voltage = voltage;
		profile_p->resistance = resistance;

		for (i = 0; i < MAX_CHARGE_RDC; i++)
			profile_p->charge_r.rdc[i] = charge_rdc[i];


		profile_p++;

		if (idx >= (saddles * column))
			break;
	}

	if (idx == 0) {
		bm_err("[%s] cannot find %s in dts\n", __func__, node_srting);
		return;
	}

	profile_p--;

	while (idx < (100 * column)) {
		profile_p++;
		profile_p->mah = mah;
		profile_p->voltage = voltage;
		profile_p->resistance = resistance;
		for (i = 0; i < MAX_CHARGE_RDC; i++)
			profile_p->charge_r.rdc[i] = charge_rdc[i];

		idx = idx + column;
	}
}

/* struct FUELGAUGE_TEMPERATURE Fg_Temperature_Table[21]; */
static void fg_custom_part_ntc_table(const struct device_node *np,
		struct FUELGAUGE_TEMPERATURE *profile_struct)
{
	struct FUELGAUGE_TEMPERATURE *p_fg_temp_table;
	int bat_temp = 0, temperature_r = 0;
	int saddles = 0, idx = 0, ret = 0, ret_a = 0;
#if 0
	int i;
#endif
	p_fg_temp_table = profile_struct;

#if 0 /* dump */
	bm_err("[before]Fg_Temperature_Table - bat_temp : temperature_r\n");
	for (i = 0; i < 21; i++) {
		bm_err("%d : %d %d\n", i, Fg_Temperature_Table[i].BatteryTemp,
			Fg_Temperature_Table[i].TemperatureR);
	}
#endif

	ret = fg_read_dts_val(np, "RBAT_TYPE", &(gm.rbat.type), 1);
	ret_a = fg_read_dts_val(np, "RBAT_PULL_UP_R",
			&(gm.rbat.rbat_pull_up_r), 1);
	if ((ret == -1) || (ret_a == -1)) {
		bm_err("Fail to get ntc type from dts.Keep default value\t");
		bm_err("RBAT_TYPE=%d, RBAT_PULL_UP_R=%d\n",
			gm.rbat.type, gm.rbat.rbat_pull_up_r);
		return;
	}
	bm_err("From DTS. RBAT_TYPE = %d, RBAT_PULL_UP_R=%d\n",
		gm.rbat.type, gm.rbat.rbat_pull_up_r);

	fg_read_dts_val(np, "rbat_temperature_table_num", &saddles, 1);
	bm_err("%s : rbat_temperature_table_num(%d)\n", __func__, saddles);

	idx = 0;

	while (1) {
		if (idx >= (saddles * 2))
			break;
		ret = of_property_read_u32_index(np, "rbat_battery_temperature",
							idx, &bat_temp);

		idx++;
		if (!of_property_read_u32_index(
			np, "rbat_battery_temperature", idx, &temperature_r))
			bm_debug("bat_temp = %d, temperature_r=%d\n",
					bat_temp, temperature_r);

		p_fg_temp_table->BatteryTemp = bat_temp;
		p_fg_temp_table->TemperatureR = temperature_r;

		idx++;
		p_fg_temp_table++;
	}

#if 0 /* dump */
	bm_err("[after]Fg_Temperature_Table - bat_temp : temperature_r\n");
	for (i = 0; i < saddles; i++) {
		bm_err("%d : %d %d\n", i, Fg_Temperature_Table[i].BatteryTemp,
			Fg_Temperature_Table[i].TemperatureR);
	}
#endif
}

void fg_custom_init_from_dts(struct platform_device *dev)
{
	struct device_node *np = dev->dev.of_node;
	unsigned int val;
	int bat_id, multi_battery, active_table, i, j, ret, column;
	int r_pseudo100_raw = 0, r_pseudo100_col = 0;
	char node_name[128];

	fgauge_get_profile_id();
	bat_id = gm.battery_id;

	bm_err("%s\n", __func__);

	fg_read_dts_val(np, "MULTI_BATTERY", &(multi_battery), 1);
	fg_read_dts_val(np, "ACTIVE_TABLE", &(active_table), 1);

	fg_read_dts_val(np, "Q_MAX_L_CURRENT", &(fg_cust_data.q_max_L_current),
		1);
	fg_read_dts_val(np, "Q_MAX_H_CURRENT", &(fg_cust_data.q_max_H_current),
		1);
	fg_read_dts_val_by_idx(np, "g_Q_MAX_SYS_VOLTAGE", gm.battery_id,
		&(fg_cust_data.q_max_sys_voltage), UNIT_TRANS_10);

	fg_read_dts_val(np, "PSEUDO1_EN", &(fg_cust_data.pseudo1_en), 1);
	fg_read_dts_val(np, "PSEUDO100_EN", &(fg_cust_data.pseudo100_en), 1);
	fg_read_dts_val(np, "PSEUDO100_EN_DIS",
		&(fg_cust_data.pseudo100_en_dis), 1);
	fg_read_dts_val_by_idx(np, "g_FG_PSEUDO1_OFFSET", gm.battery_id,
		&(fg_cust_data.pseudo1_iq_offset), UNIT_TRANS_100);

	/* iboot related */
	fg_read_dts_val(np, "QMAX_SEL", &(fg_cust_data.qmax_sel), 1);
	fg_read_dts_val(np, "IBOOT_SEL", &(fg_cust_data.iboot_sel), 1);
	fg_read_dts_val(np, "SHUTDOWN_SYSTEM_IBOOT",
		&(fg_cust_data.shutdown_system_iboot), 1);

	/*hw related */
	fg_read_dts_val(np, "CAR_TUNE_VALUE", &(fg_cust_data.car_tune_value),
		UNIT_TRANS_10);
	fg_read_dts_val(np, "FG_METER_RESISTANCE",
		&(fg_cust_data.fg_meter_resistance), 1);
	ret = fg_read_dts_val(np, "COM_FG_METER_RESISTANCE",
		&(fg_cust_data.com_fg_meter_resistance), 1);
	if (ret == -1)
		fg_cust_data.com_fg_meter_resistance =
			fg_cust_data.fg_meter_resistance;

	fg_read_dts_val(np, "NO_BAT_TEMP_COMPENSATE",
		&(gm.no_bat_temp_compensate), 1);
	fg_read_dts_val(np, "R_FG_VALUE", &(fg_cust_data.r_fg_value),
		UNIT_TRANS_10);
	ret = fg_read_dts_val(np, "COM_R_FG_VALUE",
		&(fg_cust_data.com_r_fg_value), UNIT_TRANS_10);
	if (ret == -1)
		fg_cust_data.com_r_fg_value = fg_cust_data.r_fg_value;

	fg_custom_part_ntc_table(np, Fg_Temperature_Table);

	fg_read_dts_val(np, "FULL_TRACKING_BAT_INT2_MULTIPLY",
		&(fg_cust_data.full_tracking_bat_int2_multiply), 1);
	fg_read_dts_val(np, "enable_tmp_intr_suspend",
		&(gm.enable_tmp_intr_suspend), 1);

	/* Aging Compensation */
	fg_read_dts_val(np, "AGING_ONE_EN", &(fg_cust_data.aging_one_en), 1);
	fg_read_dts_val(np, "AGING1_UPDATE_SOC",
		&(fg_cust_data.aging1_update_soc), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING1_LOAD_SOC",
		&(fg_cust_data.aging1_load_soc), UNIT_TRANS_100);

	fg_read_dts_val(np, "AGING4_UPDATE_SOC",
		&(fg_cust_data.aging4_update_soc), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING4_LOAD_SOC",
		&(fg_cust_data.aging4_load_soc), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING5_UPDATE_SOC",
		&(fg_cust_data.aging5_update_soc), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING5_LOAD_SOC",
		&(fg_cust_data.aging5_load_soc), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING6_UPDATE_SOC",
		&(fg_cust_data.aging6_update_soc), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING6_LOAD_SOC",
		&(fg_cust_data.aging6_load_soc), UNIT_TRANS_100);



	fg_read_dts_val(np, "AGING_TEMP_DIFF",
		&(fg_cust_data.aging_temp_diff), 1);
	fg_read_dts_val(np, "AGING_TEMP_LOW_LIMIT",
		&(fg_cust_data.aging_temp_low_limit), 1);
	fg_read_dts_val(np, "AGING_TEMP_HIGH_LIMIT",
		&(fg_cust_data.aging_temp_high_limit), 1);
	fg_read_dts_val(np, "AGING_100_EN", &(fg_cust_data.aging_100_en), 1);
	fg_read_dts_val(np, "DIFFERENCE_VOLTAGE_UPDATE",
		&(fg_cust_data.difference_voltage_update), 1);
	fg_read_dts_val(np, "AGING_FACTOR_MIN",
		&(fg_cust_data.aging_factor_min), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING_FACTOR_DIFF",
		&(fg_cust_data.aging_factor_diff), UNIT_TRANS_100);
	/* Aging Compensation 2*/
	fg_read_dts_val(np, "AGING_TWO_EN", &(fg_cust_data.aging_two_en), 1);
	/* Aging Compensation 3*/
	fg_read_dts_val(np, "AGING_THIRD_EN", &(fg_cust_data.aging_third_en),
		1);

	/* ui_soc related */
	fg_read_dts_val(np, "DIFF_SOC_SETTING",
		&(fg_cust_data.diff_soc_setting), 1);
	fg_read_dts_val(np, "KEEP_100_PERCENT",
		&(fg_cust_data.keep_100_percent), UNIT_TRANS_100);
	fg_read_dts_val(np, "DIFFERENCE_FULL_CV",
		&(fg_cust_data.difference_full_cv), 1);
	fg_read_dts_val(np, "DIFF_BAT_TEMP_SETTING",
		&(fg_cust_data.diff_bat_temp_setting), 1);
	fg_read_dts_val(np, "DIFF_BAT_TEMP_SETTING_C",
		&(fg_cust_data.diff_bat_temp_setting_c), 1);
	fg_read_dts_val(np, "DISCHARGE_TRACKING_TIME",
		&(fg_cust_data.discharge_tracking_time), 1);
	fg_read_dts_val(np, "CHARGE_TRACKING_TIME",
		&(fg_cust_data.charge_tracking_time), 1);
	fg_read_dts_val(np, "DIFFERENCE_FULLOCV_VTH",
		&(fg_cust_data.difference_fullocv_vth), 1);
	fg_read_dts_val(np, "DIFFERENCE_FULLOCV_ITH",
		&(fg_cust_data.difference_fullocv_ith), UNIT_TRANS_10);
	fg_read_dts_val(np, "CHARGE_PSEUDO_FULL_LEVEL",
		&(fg_cust_data.charge_pseudo_full_level), 1);
	fg_read_dts_val(np, "OVER_DISCHARGE_LEVEL",
		&(fg_cust_data.over_discharge_level), 1);

	/* pre tracking */
	fg_read_dts_val(np, "FG_PRE_TRACKING_EN",
		&(fg_cust_data.fg_pre_tracking_en), 1);
	fg_read_dts_val(np, "VBAT2_DET_TIME",
		&(fg_cust_data.vbat2_det_time), 1);
	fg_read_dts_val(np, "VBAT2_DET_COUNTER",
		&(fg_cust_data.vbat2_det_counter), 1);
	fg_read_dts_val(np, "VBAT2_DET_VOLTAGE1",
		&(fg_cust_data.vbat2_det_voltage1), 1);
	fg_read_dts_val(np, "VBAT2_DET_VOLTAGE2",
		&(fg_cust_data.vbat2_det_voltage2), 1);
	fg_read_dts_val(np, "VBAT2_DET_VOLTAGE3",
		&(fg_cust_data.vbat2_det_voltage3), 1);

	/* sw fg */
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH1",
		&(fg_cust_data.difference_fgc_fgv_th1), 1);
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH2",
		&(fg_cust_data.difference_fgc_fgv_th2), 1);
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH3",
		&(fg_cust_data.difference_fgc_fgv_th3), 1);
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH_SOC1",
		&(fg_cust_data.difference_fgc_fgv_th_soc1), 1);
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH_SOC2",
		&(fg_cust_data.difference_fgc_fgv_th_soc2), 1);
	fg_read_dts_val(np, "NAFG_TIME_SETTING",
		&(fg_cust_data.nafg_time_setting), 1);
	fg_read_dts_val(np, "NAFG_RATIO", &(fg_cust_data.nafg_ratio), 1);
	fg_read_dts_val(np, "NAFG_RATIO_EN", &(fg_cust_data.nafg_ratio_en), 1);
	fg_read_dts_val(np, "NAFG_RATIO_TMP_THR",
		&(fg_cust_data.nafg_ratio_tmp_thr), 1);
	fg_read_dts_val(np, "NAFG_RESISTANCE", &(fg_cust_data.nafg_resistance),
		1);

	/* mode select */
	fg_read_dts_val(np, "PMIC_SHUTDOWN_CURRENT",
		&(fg_cust_data.pmic_shutdown_current), 1);
	fg_read_dts_val(np, "PMIC_SHUTDOWN_SW_EN",
		&(fg_cust_data.pmic_shutdown_sw_en), 1);
	fg_read_dts_val(np, "FORCE_VC_MODE", &(fg_cust_data.force_vc_mode), 1);
	fg_read_dts_val(np, "EMBEDDED_SEL", &(fg_cust_data.embedded_sel), 1);
	fg_read_dts_val(np, "LOADING_1_EN", &(fg_cust_data.loading_1_en), 1);
	fg_read_dts_val(np, "LOADING_2_EN", &(fg_cust_data.loading_2_en), 1);
	fg_read_dts_val(np, "DIFF_IAVG_TH", &(fg_cust_data.diff_iavg_th), 1);

	fg_read_dts_val(np, "SHUTDOWN_GAUGE0", &(fg_cust_data.shutdown_gauge0),
		1);
	fg_read_dts_val(np, "SHUTDOWN_1_TIME", &(fg_cust_data.shutdown_1_time),
		1);
	fg_read_dts_val(np, "SHUTDOWN_GAUGE1_XMINS",
		&(fg_cust_data.shutdown_gauge1_xmins), 1);
	fg_read_dts_val(np, "SHUTDOWN_GAUGE0_VOLTAGE",
		&(fg_cust_data.shutdown_gauge0_voltage), 1);
	fg_read_dts_val(np, "SHUTDOWN_GAUGE1_VBAT_EN",
		&(fg_cust_data.shutdown_gauge1_vbat_en), 1);
	fg_read_dts_val(np, "SHUTDOWN_GAUGE1_VBAT",
		&(fg_cust_data.shutdown_gauge1_vbat), 1);

	/* ZCV update */
	fg_read_dts_val(np, "ZCV_SUSPEND_TIME",
		&(fg_cust_data.zcv_suspend_time), 1);
	fg_read_dts_val(np, "SLEEP_CURRENT_AVG",
		&(fg_cust_data.sleep_current_avg), 1);
	fg_read_dts_val(np, "ZCV_COM_VOL_LIMIT",
		&(fg_cust_data.zcv_com_vol_limit), 1);
	fg_read_dts_val(np, "ZCV_CAR_GAP_PERCENTAGE",
		&(fg_cust_data.zcv_car_gap_percentage), 1);

	/* dod_init */
	fg_read_dts_val(np, "HWOCV_OLDOCV_DIFF",
		&(fg_cust_data.hwocv_oldocv_diff), 1);
	fg_read_dts_val(np, "HWOCV_OLDOCV_DIFF_CHR",
		&(fg_cust_data.hwocv_oldocv_diff_chr), 1);
	fg_read_dts_val(np, "HWOCV_SWOCV_DIFF",
		&(fg_cust_data.hwocv_swocv_diff), 1);
	fg_read_dts_val(np, "HWOCV_SWOCV_DIFF_LT",
		&(fg_cust_data.hwocv_swocv_diff_lt), 1);
	fg_read_dts_val(np, "HWOCV_SWOCV_DIFF_LT_TEMP",
		&(fg_cust_data.hwocv_swocv_diff_lt_temp), 1);
	fg_read_dts_val(np, "SWOCV_OLDOCV_DIFF",
		&(fg_cust_data.swocv_oldocv_diff), 1);
	fg_read_dts_val(np, "SWOCV_OLDOCV_DIFF_CHR",
		&(fg_cust_data.swocv_oldocv_diff_chr), 1);
	fg_read_dts_val(np, "VBAT_OLDOCV_DIFF",
		&(fg_cust_data.vbat_oldocv_diff), 1);
	fg_read_dts_val(np, "SWOCV_OLDOCV_DIFF_EMB",
		&(fg_cust_data.swocv_oldocv_diff_emb), 1);

	fg_read_dts_val(np, "PMIC_SHUTDOWN_TIME",
		&(fg_cust_data.pmic_shutdown_time), UNIT_TRANS_60);
	fg_read_dts_val(np, "TNEW_TOLD_PON_DIFF",
		&(fg_cust_data.tnew_told_pon_diff), 1);
	fg_read_dts_val(np, "TNEW_TOLD_PON_DIFF2",
		&(fg_cust_data.tnew_told_pon_diff2), 1);
	fg_read_dts_val(np, "EXT_HWOCV_SWOCV",
		&(gm.ext_hwocv_swocv), 1);
	fg_read_dts_val(np, "EXT_HWOCV_SWOCV_LT",
		&(gm.ext_hwocv_swocv_lt), 1);
	fg_read_dts_val(np, "EXT_HWOCV_SWOCV_LT_TEMP",
		&(gm.ext_hwocv_swocv_lt_temp), 1);

	fg_read_dts_val(np, "DC_RATIO_SEL", &(fg_cust_data.dc_ratio_sel), 1);
	fg_read_dts_val(np, "DC_R_CNT", &(fg_cust_data.dc_r_cnt), 1);

	fg_read_dts_val(np, "PSEUDO1_SEL", &(fg_cust_data.pseudo1_sel), 1);

	fg_read_dts_val(np, "D0_SEL", &(fg_cust_data.d0_sel), 1);
	fg_read_dts_val(np, "AGING_SEL", &(fg_cust_data.aging_sel), 1);
	fg_read_dts_val(np, "BAT_PAR_I", &(fg_cust_data.bat_par_i), 1);
	fg_read_dts_val(np, "RECORD_LOG", &(fg_cust_data.record_log), 1);


	fg_read_dts_val(np, "FG_TRACKING_CURRENT",
		&(fg_cust_data.fg_tracking_current), 1);
	fg_read_dts_val(np, "FG_TRACKING_CURRENT_IBOOT_EN",
		&(fg_cust_data.fg_tracking_current_iboot_en), 1);
	fg_read_dts_val(np, "UI_FAST_TRACKING_EN",
		&(fg_cust_data.ui_fast_tracking_en), 1);
	fg_read_dts_val(np, "UI_FAST_TRACKING_GAP",
		&(fg_cust_data.ui_fast_tracking_gap), 1);

	fg_read_dts_val(np, "BAT_PLUG_OUT_TIME",
		&(fg_cust_data.bat_plug_out_time), 1);
	fg_read_dts_val(np, "KEEP_100_PERCENT_MINSOC",
		&(fg_cust_data.keep_100_percent_minsoc), 1);

	fg_read_dts_val(np, "UISOC_UPDATE_TYPE",
		&(fg_cust_data.uisoc_update_type), 1);

	fg_read_dts_val(np, "BATTERY_TMP_TO_DISABLE_GM30",
		&(fg_cust_data.battery_tmp_to_disable_gm30), 1);
	fg_read_dts_val(np, "BATTERY_TMP_TO_DISABLE_NAFG",
		&(fg_cust_data.battery_tmp_to_disable_nafg), 1);
	fg_read_dts_val(np, "BATTERY_TMP_TO_ENABLE_NAFG",
		&(fg_cust_data.battery_tmp_to_enable_nafg), 1);

	fg_read_dts_val(np, "LOW_TEMP_MODE", &(fg_cust_data.low_temp_mode), 1);
	fg_read_dts_val(np, "LOW_TEMP_MODE_TEMP",
		&(fg_cust_data.low_temp_mode_temp), 1);

	/* current limit for uisoc 100% */
	fg_read_dts_val(np, "UI_FULL_LIMIT_EN",
		&(fg_cust_data.ui_full_limit_en), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC0",
		&(fg_cust_data.ui_full_limit_soc0), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH0",
		&(fg_cust_data.ui_full_limit_ith0), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC1",
		&(fg_cust_data.ui_full_limit_soc1), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH1",
		&(fg_cust_data.ui_full_limit_ith1), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC2",
		&(fg_cust_data.ui_full_limit_soc2), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH2",
		&(fg_cust_data.ui_full_limit_ith2), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC3",
		&(fg_cust_data.ui_full_limit_soc3), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH3",
		&(fg_cust_data.ui_full_limit_ith3), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC4",
		&(fg_cust_data.ui_full_limit_soc4), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH4",
		&(fg_cust_data.ui_full_limit_ith4), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_TIME",
		&(fg_cust_data.ui_full_limit_time), 1);


	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC0",
		&(fg_cust_data.ui_full_limit_fc_soc0), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH0",
		&(fg_cust_data.ui_full_limit_fc_ith0), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC1",
		&(fg_cust_data.ui_full_limit_fc_soc1), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH1",
		&(fg_cust_data.ui_full_limit_fc_ith1), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC2",
		&(fg_cust_data.ui_full_limit_fc_soc2), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH2",
		&(fg_cust_data.ui_full_limit_fc_ith2), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC3",
		&(fg_cust_data.ui_full_limit_fc_soc3), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH3",
		&(fg_cust_data.ui_full_limit_fc_ith3), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC4",
		&(fg_cust_data.ui_full_limit_fc_soc4), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH4",
		&(fg_cust_data.ui_full_limit_fc_ith4), 1);

	/* voltage limit for uisoc 1% */
	fg_read_dts_val(np, "UI_LOW_LIMIT_EN", &(fg_cust_data.ui_low_limit_en),
		1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC0",
		&(fg_cust_data.ui_low_limit_soc0), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH0",
		&(fg_cust_data.ui_low_limit_vth0), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC1",
		&(fg_cust_data.ui_low_limit_soc1), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH1",
		&(fg_cust_data.ui_low_limit_vth1), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC2",
		&(fg_cust_data.ui_low_limit_soc2), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH2",
		&(fg_cust_data.ui_low_limit_vth2), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC3",
		&(fg_cust_data.ui_low_limit_soc3), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH3",
		&(fg_cust_data.ui_low_limit_vth3), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC4",
		&(fg_cust_data.ui_low_limit_soc4), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH4",
		&(fg_cust_data.ui_low_limit_vth4), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_TIME",
		&(fg_cust_data.ui_low_limit_time), 1);

	/* average battemp */
	fg_read_dts_val(np, "MOVING_BATTEMP_EN",
		&(fg_cust_data.moving_battemp_en), 1);
	fg_read_dts_val(np, "MOVING_BATTEMP_THR",
		&(fg_cust_data.moving_battemp_thr), 1);

	/* battery health */
	fg_read_dts_val(np, "AGING_DIFF_MAX_THRESHOLD",
		&(fg_cust_data.aging_diff_max_threshold), 1);
	fg_read_dts_val(np, "AGING_DIFF_MAX_LEVEL",
		&(fg_cust_data.aging_diff_max_level), 1);
	fg_read_dts_val(np, "AGING_FACTOR_T_MIN",
		&(fg_cust_data.aging_factor_t_min), 1);
	fg_read_dts_val(np, "CYCLE_DIFF",
		&(fg_cust_data.cycle_diff), 1);
	fg_read_dts_val(np, "AGING_COUNT_MIN",
		&(fg_cust_data.aging_count_min), 1);
	fg_read_dts_val(np, "DEFAULT_SCORE",
		&(fg_cust_data.default_score), 1);
	fg_read_dts_val(np, "DEFAULT_SCORE_QUANTITY",
		&(fg_cust_data.default_score_quantity), 1);
	fg_read_dts_val(np, "FAST_CYCLE_SET",
		&(fg_cust_data.fast_cycle_set), 1);
	fg_read_dts_val(np, "LEVEL_MAX_CHANGE_BAT",
		&(fg_cust_data.level_max_change_bat), 1);
	fg_read_dts_val(np, "DIFF_MAX_CHANGE_BAT",
		&(fg_cust_data.diff_max_change_bat), 1);
	fg_read_dts_val(np, "AGING_TRACKING_START",
		&(fg_cust_data.aging_tracking_start), 1);
	fg_read_dts_val(np, "MAX_AGING_DATA",
		&(fg_cust_data.max_aging_data), 1);
	fg_read_dts_val(np, "MAX_FAST_DATA",
		&(fg_cust_data.max_fast_data), 1);
	fg_read_dts_val(np, "FAST_DATA_THRESHOLD_SCORE",
		&(fg_cust_data.fast_data_threshold_score), 1);

	fg_read_dts_val(np, "DISABLE_MTKBATTERY",
		(int *)&(gm.disable_mtkbattery), 1);
	fg_read_dts_val(np, "MULTI_TEMP_GAUGE0",
		&(fg_cust_data.multi_temp_gauge0), 1);
	fg_read_dts_val(np, "FGC_FGV_TH1",
		&(fg_cust_data.difference_fgc_fgv_th1), 1);
	fg_read_dts_val(np, "FGC_FGV_TH2",
		&(fg_cust_data.difference_fgc_fgv_th2), 1);
	fg_read_dts_val(np, "FGC_FGV_TH3",
		&(fg_cust_data.difference_fgc_fgv_th3), 1);
	fg_read_dts_val(np, "UISOC_UPDATE_T",
		&(fg_cust_data.uisoc_update_type), 1);
	fg_read_dts_val(np, "UIFULLLIMIT_EN",
		&(fg_cust_data.ui_full_limit_en), 1);
	fg_read_dts_val(np, "MTK_CHR_EXIST", &(fg_cust_data.mtk_chr_exist), 1);

	fg_read_dts_val(np, "GM30_DISABLE_NAFG", &(fg_cust_data.disable_nafg),
		1);

	fg_read_dts_val(np, "ACTIVE_TABLE",
		&(fg_table_cust_data.active_table_number), 1);

#if defined(CONFIG_MTK_ADDITIONAL_BATTERY_TABLE)
	if (fg_table_cust_data.active_table_number == 0)
		fg_table_cust_data.active_table_number = 5;
#else
	if (fg_table_cust_data.active_table_number == 0)
		fg_table_cust_data.active_table_number = 4;
#endif

	bm_err("fg active table:%d\n",
		fg_table_cust_data.active_table_number);


	/* battery temperature, TEMPERATURE_T0 ~ T9 */
	for (i = 0; i < fg_table_cust_data.active_table_number; i++) {
		sprintf(node_name, "TEMPERATURE_T%d", i);
		fg_read_dts_val(np, node_name,
			&(fg_table_cust_data.fg_profile[i].temperature), 1);
		}


	fg_read_dts_val(np, "TEMPERATURE_TB0",
		&(fg_table_cust_data.temperature_tb0), 1);
	fg_read_dts_val(np, "TEMPERATURE_TB1",
		&(fg_table_cust_data.temperature_tb1), 1);

	for (i = 0; i < MAX_TABLE; i++) {
		struct FUELGAUGE_PROFILE_STRUCT *p;

		p = &fg_table_cust_data.fg_profile[i].fg_profile[0];
		fg_read_dts_val_by_idx(np, "g_temperature", i,
			&(fg_table_cust_data.fg_profile[i].temperature), 1);
		fg_read_dts_val_by_idx(np, "g_Q_MAX",
			i*TOTAL_BATTERY_NUMBER+gm.battery_id,
			&(fg_table_cust_data.fg_profile[i].q_max), 1);
		fg_read_dts_val_by_idx(np, "g_Q_MAX_H_CURRENT",
			i*TOTAL_BATTERY_NUMBER+gm.battery_id,
			&(fg_table_cust_data.fg_profile[i].q_max_h_current), 1);
		fg_read_dts_val_by_idx(np, "g_FG_PSEUDO1",
			i*TOTAL_BATTERY_NUMBER+gm.battery_id,
			&(fg_table_cust_data.fg_profile[i].pseudo1),
			UNIT_TRANS_100);
		fg_read_dts_val_by_idx(np, "g_FG_PSEUDO100",
			i*TOTAL_BATTERY_NUMBER+gm.battery_id,
			&(fg_table_cust_data.fg_profile[i].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val_by_idx(np, "g_PMIC_MIN_VOL",
			i*TOTAL_BATTERY_NUMBER+gm.battery_id,
			&(fg_table_cust_data.fg_profile[i].pmic_min_vol), 1);
		fg_read_dts_val_by_idx(np, "g_PON_SYS_IBOOT",
			i*TOTAL_BATTERY_NUMBER+gm.battery_id,
			&(fg_table_cust_data.fg_profile[i].pon_iboot), 1);
		fg_read_dts_val_by_idx(np, "g_QMAX_SYS_VOL",
			i*TOTAL_BATTERY_NUMBER+gm.battery_id,
			&(fg_table_cust_data.fg_profile[i].qmax_sys_vol), 1);
		fg_read_dts_val_by_idx(np, "g_SHUTDOWN_HL_ZCV",
			i*TOTAL_BATTERY_NUMBER+gm.battery_id,
			&(fg_table_cust_data.fg_profile[i].shutdown_hl_zcv), 1);
		for (j = 0; j < 100; j++) {
			if (p[j].charge_r.rdc[0] == 0)
				p[j].charge_r.rdc[0] = p[j].resistance;
	}
	}

	if (bat_id >= 0 && bat_id < TOTAL_BATTERY_NUMBER) {
		sprintf(node_name, "Q_MAX_SYS_VOLTAGE_BAT%d", bat_id);
		fg_read_dts_val(np, node_name,
			&(fg_cust_data.q_max_sys_voltage), UNIT_TRANS_10);
		sprintf(node_name, "PSEUDO1_IQ_OFFSET_BAT%d", bat_id);
		fg_read_dts_val(np, node_name,
			&(fg_cust_data.pseudo1_iq_offset), UNIT_TRANS_100);
	} else
		bm_err(
		"get Q_MAX_SYS_VOLTAGE_BAT, PSEUDO1_IQ_OFFSET_BAT %d failed\n",
		bat_id);

	if (fg_cust_data.multi_temp_gauge0 == 0) {
		int i = 0;

		if (!of_property_read_u32(np, "PMIC_MIN_VOL", &val)) {
			for (i = 0; i < MAX_TABLE; i++)
				fg_table_cust_data.fg_profile[i].pmic_min_vol =
				(int)val;
			bm_debug("Get PMIC_MIN_VOL: %d\n",
				 fg_table_cust_data.fg_profile[0].pmic_min_vol);
		} else {
			bm_err("Get PMIC_MIN_VOL failed\n");
		}

		if (!of_property_read_u32(np, "POWERON_SYSTEM_IBOOT", &val)) {
			for (i = 0; i < MAX_TABLE; i++)
				fg_table_cust_data.fg_profile[i].pon_iboot =
				(int)val * UNIT_TRANS_10;

			bm_debug("Get POWERON_SYSTEM_IBOOT: %d\n",
				fg_table_cust_data.fg_profile[0].pon_iboot);
		} else {
			bm_err("Get POWERON_SYSTEM_IBOOT failed\n");
		}
	}

	if (active_table == 0 && multi_battery == 0) {
		fg_read_dts_val(np, "g_FG_PSEUDO100_T0",
			&(fg_table_cust_data.fg_profile[0].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(np, "g_FG_PSEUDO100_T1",
			&(fg_table_cust_data.fg_profile[1].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(np, "g_FG_PSEUDO100_T2",
			&(fg_table_cust_data.fg_profile[2].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(np, "g_FG_PSEUDO100_T3",
			&(fg_table_cust_data.fg_profile[3].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(np, "g_FG_PSEUDO100_T4",
			&(fg_table_cust_data.fg_profile[4].pseudo100),
			UNIT_TRANS_100);
	}

	/* compatiable with old dtsi*/
	if (active_table == 0) {
		fg_read_dts_val(np, "TEMPERATURE_T0",
			&(fg_table_cust_data.fg_profile[0].temperature), 1);
		fg_read_dts_val(np, "TEMPERATURE_T1",
			&(fg_table_cust_data.fg_profile[1].temperature), 1);
		fg_read_dts_val(np, "TEMPERATURE_T2",
			&(fg_table_cust_data.fg_profile[2].temperature), 1);
		fg_read_dts_val(np, "TEMPERATURE_T3",
			&(fg_table_cust_data.fg_profile[3].temperature), 1);
		fg_read_dts_val(np, "TEMPERATURE_T4",
			&(fg_table_cust_data.fg_profile[4].temperature), 1);
	}

	fg_read_dts_val(np, "g_FG_charge_PSEUDO100_row",
		&(r_pseudo100_raw), 1);
	fg_read_dts_val(np, "g_FG_charge_PSEUDO100_col",
		&(r_pseudo100_col), 1);

	/* init for pseudo100 */
	for (i = 0; i < MAX_TABLE; i++) {
		for (j = 0; j < MAX_CHARGE_RDC; j++)
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[j]
				= fg_table_cust_data.fg_profile[i].pseudo100;
	}

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err("%6d %6d %6d %6d %6d\n",
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[0],
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[1],
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[2],
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[3],
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[4]
			);
	}
	/* read dtsi from pseudo100 */
	for (i = 0; i < MAX_TABLE; i++) {
		for (j = 0; j < r_pseudo100_raw; j++) {
			fg_read_dts_val_by_idx(np, "g_FG_charge_PSEUDO100",
				i*r_pseudo100_raw+j,
				&(fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[j+1]),
					UNIT_TRANS_100);
		}
	}


	bm_err("g_FG_charge_PSEUDO100_row:%d g_FG_charge_PSEUDO100_col:%d\n",
		r_pseudo100_raw, r_pseudo100_col);

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err("%6d %6d %6d %6d %6d\n",
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[0],
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[1],
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[2],
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[3],
			fg_table_cust_data.fg_profile[i].r_pseudo100.pseudo[4]
			);
	}

	/* END of pseudo100 */

	for (i = 0; i < fg_table_cust_data.active_table_number; i++) {
		sprintf(node_name, "battery%d_profile_t%d_num", bat_id, i);
		fg_read_dts_val(np, node_name,
			&(fg_table_cust_data.fg_profile[i].size), 1);

		/* compatiable with old dtsi table*/
		sprintf(node_name, "battery%d_profile_t%d_col", bat_id, i);
		ret = fg_read_dts_val(np, node_name, &(column), 1);
		if (ret == -1)
			column = 3;

		if (column < 3 || column > 8) {
			bm_err("%s, %s,column:%d ERROR!",
				__func__, node_name, column);
			/* correction */
			column = 3;
	}

		sprintf(node_name, "battery%d_profile_t%d", bat_id, i);
		fg_custom_parse_table(np, node_name,
			fg_table_cust_data.fg_profile[i].fg_profile, column);
	}
		}

#endif	/* end of CONFIG_OF */



/* ============================================================ */
/* Customized function */
/* ============================================================ */
int get_customized_aging_factor(int orig_af)
{
	return (orig_af + 100);
}

int get_customized_d0_c_soc(int origin_d0_c_soc)
{
	int val;

	if (get_ec()->debug_d0_c_en == 1)
		val = get_ec()->debug_d0_c_value;
	else
		val = (origin_d0_c_soc + 0);

	bm_err("[%s] EC_en %d EC_value %d original %d val %d\n",
		__func__,
		get_ec()->debug_d0_c_en, get_ec()->debug_d0_c_value,
		origin_d0_c_soc, val);

	return val;
}

int get_customized_d0_v_soc(int origin_d0_v_soc)
{
	int val;

	if (get_ec()->debug_d0_v_en == 1)
		val = get_ec()->debug_d0_v_value;
	else
		val = (origin_d0_v_soc + 0);

	bm_err("[%s] EC_en %d EC_value %d original %d val %d\n",
		__func__,
		get_ec()->debug_d0_v_en, get_ec()->debug_d0_v_value,
		origin_d0_v_soc, val);

	return val;
}

int get_customized_uisoc(int origin_uisoc)
{
	int val;

	if (get_ec()->debug_uisoc_en == 1)
		val = get_ec()->debug_uisoc_value;
	else
		val = (origin_uisoc + 0);

	bm_err("[get_customized_d0_c_soc] EC_en %d EC_value %d original %d val %d\n",
		get_ec()->debug_uisoc_en, get_ec()->debug_uisoc_value,
		origin_uisoc, val);

	return val;
}


/* ============================================================ */
/* query function */
/* ============================================================ */

struct mtk_battery *get_mtk_battery(void)
{
	return &gm;
}

struct BAT_EC_Struct *get_ec(void)
{
	return &gm.Bat_EC_ctrl;
}


/* ============================================================ */
/* interface with other module */
/* ============================================================ */
static void _do_ptim(void)
{
	bool is_charging = false;

	do_ptim_gauge(false, &gm.ptim_vol, &gm.ptim_curr, &is_charging);

	gm.log.ptim_bat = gm.ptim_vol;
	gm.log.ptim_cur = gm.ptim_curr;
	gm.log.ptim_is_charging = is_charging;

	if ((is_charging == false) && (gm.ptim_curr >= 0))
		gm.ptim_curr = 0 - gm.ptim_curr;
}

static int _get_ptim_bat_vol(void)
{
	int vbat;

	if (get_ec()->debug_ptim_v_en == 1)
		vbat = get_ec()->debug_ptim_v_value;
	else
		vbat = gm.ptim_vol;

	return vbat;
}

static int _get_ptim_R_curr(void)
{
	int cur;

	if (get_ec()->debug_ptim_r_en == 1)
		cur = get_ec()->debug_ptim_r_value;
	else
		cur = gm.ptim_curr;

	return cur;
}

static int _get_ptim_rac_val(void)
{
	int rac;

	if (get_ec()->debug_rac_en == 1)
		rac = get_ec()->debug_rac_value;
	else
		rac = get_rac();

	return rac;
}

int fg_get_system_sec(void)
{
	struct timespec time;

	time.tv_sec = 0;
	time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	return (int)time.tv_sec;
}

unsigned long long fg_get_log_sec(void)
{
	unsigned long long logtime;

#if defined(__LP64__) || defined(_LP64)
	logtime = sched_clock() / 1000000000;
#else
	logtime = div_u64(sched_clock(), 1000000000);
#endif

	return logtime;
}

void notify_fg_dlpt_sd(void)
{
	bm_err("[%s]\n", __func__);
	wakeup_fg_algo(FG_INTR_DLPT_SD);
}

void notify_fg_shutdown(void)
{
	bm_err("[%s]\n", __func__);
	wakeup_fg_algo(FG_INTR_SHUTDOWN);
}

void notify_fg_chr_full(void)
{
	struct timespec now_time, difftime;

	get_monotonic_boottime(&now_time);
	difftime = timespec_sub(now_time, gm.chr_full_handler_time);
	if (now_time.tv_sec <= 10 || difftime.tv_sec >= 10) {
		gm.chr_full_handler_time = now_time;
		bm_err("[fg_chr_full_int_handler]\n");
		wakeup_fg_algo(FG_INTR_CHR_FULL);
		fg_int_event(gm.gdev, EVT_INT_CHR_FULL);
	}
}

/* ============================================================ */
/* check bat plug out  */
/* ============================================================ */
void sw_check_bat_plugout(void)
{
	int is_bat_exist;

	if (gm.disable_plug_int && gm.disableGM30 != true) {
		is_bat_exist = pmic_is_battery_exist();
		/* fg_bat_plugout_int_handler(); */
		if (is_bat_exist == 0) {
			bm_err(
				"[swcheck_bat_plugout]g_disable_plug_int=%d, is_bat_exist %d, is_fg_disable %d\n",
				gm.disable_plug_int,
				is_bat_exist,
				is_fg_disabled());

			battery_notifier(EVENT_BATTERY_PLUG_OUT);
			battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
			wakeup_fg_algo(FG_INTR_BAT_PLUGOUT);
			battery_update(&battery_main);
			kernel_power_off();
		}
	}
}

/* ============================================================ */
/* nafg monitor */
/* ============================================================ */

void fg_nafg_monitor(void)
{
	int nafg_cnt = 0;
	struct timespec now_time, dtime;

	if (gm.disableGM30 || gm.cmd_disable_nafg || gm.ntc_disable_nafg)
		return;

	now_time.tv_sec = 0;
	now_time.tv_nsec = 0;
	dtime.tv_sec = 0;
	dtime.tv_nsec = 0;

	gauge_dev_get_nag_cnt(gm.gdev, &nafg_cnt);

	if (gm.last_nafg_cnt != nafg_cnt) {
		gm.last_nafg_cnt = nafg_cnt;
		get_monotonic_boottime(&gm.last_nafg_update_time);
	} else {
		get_monotonic_boottime(&now_time);
		dtime = timespec_sub(now_time, gm.last_nafg_update_time);
		if (dtime.tv_sec >= 600) {
			gm.is_nafg_broken = true;
			wakeup_fg_algo_cmd(
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG,
				true);
		}
	}
	bm_debug("[%s]time:%d nafg_cnt:%d, now:%d, last_t:%d\n",
		__func__,
		(int)dtime.tv_sec,
		gm.last_nafg_cnt,
		(int)now_time.tv_sec,
		(int)gm.last_nafg_update_time.tv_sec);

}

/* ============================================================ */
/* sw iavg */
/* ============================================================ */
static void sw_iavg_init(void)
{
	int is_bat_charging = 0;
	int bat_current = 0;

	get_monotonic_boottime(&gm.sw_iavg_time);
	gm.sw_iavg_car = gauge_get_coulomb();

	/* BAT_DISCHARGING = 0 */
	/* BAT_CHARGING = 1 */
	is_bat_charging = gauge_get_current(&bat_current);

	if (is_bat_charging == 1)
		gm.sw_iavg = bat_current;
	else
		gm.sw_iavg = -bat_current;
	gm.sw_iavg_ht = gm.sw_iavg + gm.sw_iavg_gap;
	gm.sw_iavg_lt = gm.sw_iavg - gm.sw_iavg_gap;

	bm_debug("[%s] iavg:%d car:%d\n", __func__,
		gm.sw_iavg,
		gm.sw_iavg_car);
}

void fg_update_sw_iavg(void)
{
	struct timespec now_time, diff;
	int fg_coulomb;

	get_monotonic_boottime(&now_time);

	diff = timespec_sub(now_time, gm.sw_iavg_time);
	bm_debug("[%s]diff time:%ld iavg:%d\n",
		__func__,
		diff.tv_sec,
		gm.sw_iavg);
	if (diff.tv_sec >= 60) {
		fg_coulomb = gauge_get_coulomb();
		gm.sw_iavg = (fg_coulomb - gm.sw_iavg_car) * 3600 / diff.tv_sec;
		gm.sw_iavg_time = now_time;
		gm.sw_iavg_car = fg_coulomb;
		if (gm.sw_iavg >= gm.sw_iavg_ht
			|| gm.sw_iavg <= gm.sw_iavg_lt) {
			gm.sw_iavg_ht = gm.sw_iavg + gm.sw_iavg_gap;
			gm.sw_iavg_lt = gm.sw_iavg - gm.sw_iavg_gap;
			if (gauge_get_hw_version() < GAUGE_HW_V2000)
				wakeup_fg_algo(FG_INTR_IAVG);
		}
		bm_debug("[%s]time:%ld car:%d %d iavg:%d ht:%d lt:%d gap:%d\n",
			__func__,
			diff.tv_sec, fg_coulomb, gm.sw_iavg_car, gm.sw_iavg,
			gm.sw_iavg_ht, gm.sw_iavg_lt, gm.sw_iavg_gap);
	}

}


/* ============================================================ */
/* interrupt handler */
/* ============================================================ */
/* ============================================================ */
/* sw battery temperature interrupt handler */
/* ============================================================ */

void fg_bat_sw_temp_int_l_handler(void)
{
	bm_debug("[%s]\n", __func__);
	fg_bat_temp_int_internal();
}

void fg_bat_sw_temp_int_h_handler(void)
{
	bm_debug("[%s]\n", __func__);
	fg_bat_temp_int_internal();
}

void fg_bat_temp_int_sw_check(void)
{
	int tmp = force_get_tbat(true);

	if (gm.disableGM30)
		return;

	bm_err(
		"[%s] tmp %d lt %d ht %d\n",
		__func__,
		tmp, gm.fg_bat_tmp_lt,
		gm.fg_bat_tmp_ht);

	if (tmp >= gm.fg_bat_tmp_ht)
		fg_bat_sw_temp_int_h_handler();
	else if (tmp <= gm.fg_bat_tmp_lt)
		fg_bat_sw_temp_int_l_handler();
}

void fg_int_event(struct gauge_device *gauge_dev, enum gauge_event evt)
{
	if (evt != EVT_INT_NAFG_CHECK)
		fg_bat_temp_int_sw_check();

	gauge_dev_notify_event(gauge_dev, evt, 0);
}

void fg_update_sw_low_battery_check(unsigned int thd)
{
	int vbat;
	int thd1, thd2, thd3;

	thd1 = fg_cust_data.vbat2_det_voltage1 / 10;
	thd2 = fg_cust_data.vbat2_det_voltage2 / 10;
	thd3 = fg_cust_data.vbat2_det_voltage3 / 10;


	if (gauge_get_hw_version() >= GAUGE_HW_V2000)
		return;

	vbat = pmic_get_battery_voltage() * 10;
	bm_err("[%s]vbat:%d %d ht:%d %d lt:%d %d\n",
		__func__,
		thd, vbat,
		gm.sw_low_battery_ht_en,
		gm.sw_low_battery_ht_threshold,
		gm.sw_low_battery_lt_en,
		gm.sw_low_battery_lt_threshold);

	if (gm.sw_low_battery_ht_en == 1 && thd == thd3) {
		mutex_lock(&gm.sw_low_battery_mutex);
		gm.sw_low_battery_ht_en = 0;
		gm.sw_low_battery_lt_en = 0;
		mutex_unlock(&gm.sw_low_battery_mutex);
		disable_shutdown_cond(LOW_BAT_VOLT);
		wakeup_fg_algo(FG_INTR_VBAT2_H);
	}
	if (gm.sw_low_battery_lt_en == 1 && (thd == thd1 || thd == thd2)) {
		mutex_lock(&gm.sw_low_battery_mutex);
		gm.sw_low_battery_ht_en = 0;
		gm.sw_low_battery_lt_en = 0;
		mutex_unlock(&gm.sw_low_battery_mutex);
		wakeup_fg_algo(FG_INTR_VBAT2_L);
	}

}

/* ============================================================ */
/* alarm timer handler */
/* ============================================================ */
static enum alarmtimer_restart tracking_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	gm.tracking_cb_flag = 1;
	wake_up(&gm.wait_que);
	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart one_percent_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	gm.onepercent_cb_flag = 1;
	wake_up(&gm.wait_que);
	return ALARMTIMER_NORESTART;
}

/* ============================================================ */
/* zcv interrupt handler */
/* ============================================================ */
void fg_zcv_int_handler(void)
{
	int fg_coulomb = 0;
	int zcv_intr_en = 0;
	int zcv_intr_curr = 0;
	int zcv = 0;

	if (fg_interrupt_check() == false)
		return;

	fg_coulomb = gauge_get_coulomb();
	gauge_get_zcv_current(&zcv_intr_curr);
	gauge_get_zcv(&zcv);
	bm_err("[%s] car:%d zcv_curr:%d zcv:%d, slp_cur_avg:%d zcv_check:%d\n",
		__func__,
		fg_coulomb, zcv_intr_curr, zcv,
		fg_cust_data.sleep_current_avg,
		zcv_check(&gm.zcvf));

	if (abs(zcv_intr_curr) < fg_cust_data.sleep_current_avg) {
		wakeup_fg_algo(FG_INTR_FG_ZCV);
		zcv_intr_en = 0;
		gauge_set_zcv_interrupt_en(zcv_intr_en);
	}

	fg_int_event(gm.gdev, EVT_INT_ZCV);
	sw_check_bat_plugout();
}


/* ============================================================ */
/* battery cycle interrupt handler */
/* ============================================================ */
void fg_sw_bat_cycle_accu(void)
{
	int diff_car = 0, tmp_car = 0, tmp_ncar = 0, tmp_thr = 0;
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_coulomb();
	tmp_car = gm.bat_cycle_car;
	tmp_ncar = gm.bat_cycle_ncar;

	diff_car = fg_coulomb - gm.bat_cycle_car;

	if (diff_car > 0) {
		bm_err("[%s]ERROR!drop diff_car\n", __func__);
		gm.bat_cycle_car = fg_coulomb;
	} else {
		gm.bat_cycle_ncar = gm.bat_cycle_ncar + abs(diff_car);
		gm.bat_cycle_car = fg_coulomb;
	}

	gauge_dev_get_hw_status(gm.gdev, &gm.hw_status, 0);
	bm_err("[%s]car[o:%d n:%d],diff_car:%d,ncar[o:%d n:%d hw:%d] thr %d\n",
		__func__,
		tmp_car, fg_coulomb, diff_car,
		tmp_ncar, gm.bat_cycle_ncar, gm.gdev->fg_hw_info.ncar,
		gm.bat_cycle_thr);

	if (gauge_get_hw_version() >= GAUGE_HW_V1000 &&
		gauge_get_hw_version() < GAUGE_HW_V2000) {
		if (gm.bat_cycle_thr > 0 &&
			gm.bat_cycle_ncar >= gm.bat_cycle_thr) {
			tmp_ncar = gm.bat_cycle_ncar;
			tmp_thr = gm.bat_cycle_thr;

			gm.bat_cycle_ncar = 0;
			wakeup_fg_algo(FG_INTR_BAT_CYCLE);
			bm_err("[fg_cycle_int_handler] ncar:%d thr:%d\n",
				tmp_ncar, tmp_thr);
		}
	}
}

void fg_cycle_int_handler(void)
{
	if (fg_interrupt_check() == false)
		return;
	gauge_enable_interrupt(FG_N_CHARGE_L_NO, 0);
	wakeup_fg_algo(FG_INTR_BAT_CYCLE);
	fg_int_event(gm.gdev, EVT_INT_BAT_CYCLE);
	sw_check_bat_plugout();
}

/* ============================================================ */
/* hw iavg interrupt handler */
/* ============================================================ */
void fg_iavg_int_ht_handler(void)
{
	if (fg_interrupt_check() == false)
		return;

	gm.hw_status.iavg_intr_flag = 0;
	gauge_enable_iavg_interrupt(false, 0, false, 0);
	gauge_enable_interrupt(FG_IAVG_H_NO, 0);
	gauge_enable_interrupt(FG_IAVG_L_NO, 0);
	bm_err("[FGADC_intr_end][%s] iavg_intr_flag %d\n",
		__func__,
		gm.hw_status.iavg_intr_flag);

	wakeup_fg_algo(FG_INTR_IAVG);

	fg_int_event(gm.gdev, EVT_INT_IAVG);

	sw_check_bat_plugout();
}

void fg_iavg_int_lt_handler(void)
{
	if (fg_interrupt_check() == false)
		return;

	gm.hw_status.iavg_intr_flag = 0;
	gauge_enable_iavg_interrupt(false, 0, false, 0);
	gauge_enable_interrupt(FG_IAVG_H_NO, 0);
	gauge_enable_interrupt(FG_IAVG_L_NO, 0);
	bm_err("[FGADC_intr_end][%s] iavg_intr_flag %d\n",
		__func__,
		gm.hw_status.iavg_intr_flag);

	wakeup_fg_algo(FG_INTR_IAVG);

	fg_int_event(gm.gdev, EVT_INT_IAVG);
	sw_check_bat_plugout();
}

/* ============================================================ */
/* charger in interrupt handler */
/* ============================================================ */
void fg_charger_in_handler(void)
{
	static enum charger_type chr_type;
	enum charger_type current_chr_type;

	current_chr_type = mt_get_charger_type();

	bm_debug("[%s] notify daemon %d %d\n",
		__func__,
		chr_type, current_chr_type);

	if (current_chr_type != CHARGER_UNKNOWN) {
		if (chr_type == CHARGER_UNKNOWN)
			wakeup_fg_algo_atomic(FG_INTR_CHARGER_IN);
	}

	if (current_chr_type == CHARGER_UNKNOWN) {
		if (chr_type != CHARGER_UNKNOWN)
			wakeup_fg_algo_atomic(FG_INTR_CHARGER_OUT);
	}

	chr_type = current_chr_type;
}

/* ============================================================ */
/* hw battery temperature interrupt handler */
/* ============================================================ */

void fg_bat_temp_int_init(void)
{
	int tmp = 0;
	int fg_bat_new_ht, fg_bat_new_lt;

	if (fg_interrupt_check() == false)
		return;
#if defined(CONFIG_MTK_DISABLE_GAUGE) || defined(FIXED_TBAT_25)
	tmp = 1;
	fg_bat_new_ht = 1;
	fg_bat_new_lt = 1;
	return;
#else
	tmp = force_get_tbat(true);

	fg_bat_new_ht = TempToBattVolt(tmp + 1, 1);
	fg_bat_new_lt = TempToBattVolt(tmp - 1, 0);

	gauge_dev_enable_battery_tmp_lt_interrupt(gm.gdev, false, 0);
	gauge_dev_enable_battery_tmp_ht_interrupt(gm.gdev, false, 0);
	gauge_dev_enable_battery_tmp_lt_interrupt(
		gm.gdev, true, fg_bat_new_lt);
	gauge_dev_enable_battery_tmp_ht_interrupt(
		gm.gdev, true, fg_bat_new_ht);
#endif
}

void fg_bat_temp_int_internal(void)
{
	int tmp = 0;
	int fg_bat_new_ht, fg_bat_new_lt;

	if (is_fg_disabled()) {
		battery_main.BAT_batt_temp = 25;
		battery_update(&battery_main);
		return;
	}

#if defined(CONFIG_MTK_DISABLE_GAUGE) || defined(FIXED_TBAT_25)
	battery_main.BAT_batt_temp = 25;
	battery_update(&battery_main);
	tmp = 1;
	fg_bat_new_ht = 1;
	fg_bat_new_lt = 1;
	return;
#else
	tmp = force_get_tbat(true);

	gauge_dev_enable_battery_tmp_lt_interrupt(gm.gdev, false, 0);
	gauge_dev_enable_battery_tmp_ht_interrupt(gm.gdev, false, 0);

	if (get_ec()->fixed_temp_en == 1)
		tmp = get_ec()->fixed_temp_value;

	if (tmp >= gm.fg_bat_tmp_c_ht)
		wakeup_fg_algo(FG_INTR_BAT_TMP_C_HT);
	else if (tmp <= gm.fg_bat_tmp_c_lt)
		wakeup_fg_algo(FG_INTR_BAT_TMP_C_LT);

	if (tmp >= gm.fg_bat_tmp_ht)
		wakeup_fg_algo(FG_INTR_BAT_TMP_HT);
	else if (tmp <= gm.fg_bat_tmp_lt)
		wakeup_fg_algo(FG_INTR_BAT_TMP_LT);

	fg_bat_new_ht = TempToBattVolt(tmp + 1, 1);
	fg_bat_new_lt = TempToBattVolt(tmp - 1, 0);

	if (gm.fixed_bat_tmp == 0xffff) {
		gauge_dev_enable_battery_tmp_lt_interrupt(
			gm.gdev, true, fg_bat_new_lt);
		gauge_dev_enable_battery_tmp_ht_interrupt(
			gm.gdev, true, fg_bat_new_ht);
	}
	bm_err("[%s][FG_TEMP_INT] T[%d] V[%d %d] C[%d %d] h[%d %d]\n",
		__func__,
		tmp, gm.fg_bat_tmp_ht,
		gm.fg_bat_tmp_lt, gm.fg_bat_tmp_c_ht,
		gm.fg_bat_tmp_c_lt,
		fg_bat_new_lt, fg_bat_new_ht);

	battery_main.BAT_batt_temp = tmp;
	battery_update(&battery_main);
#endif
}

void fg_bat_temp_int_l_handler(void)
{
	if (fg_interrupt_check() == false)
		return;

	bm_err("[%s]\n", __func__);
	fg_bat_temp_int_internal();
}

void fg_bat_temp_int_h_handler(void)
{
	if (fg_interrupt_check() == false)
		return;

	bm_err("[%s]\n", __func__);
	fg_bat_temp_int_internal();
}

/* ============================================================ */
/* battery plug out interrupt handler */
/* ============================================================ */
void fg_bat_plugout_int_handler(void)
{
	int is_bat_exist;
	int i;

	is_bat_exist = pmic_is_battery_exist();

	bm_err("[%s]is_bat %d miss:%d\n",
		__func__,
		is_bat_exist, gm.plug_miss_count);

	if (fg_interrupt_check() == false)
		return;

	gauge_dev_dump(gm.gdev, NULL, 0);

	/* avoid battery plug status mismatch case*/
	if (is_bat_exist == 1) {
		fg_int_event(gm.gdev, EVT_INT_BAT_PLUGOUT);
		gm.plug_miss_count++;

		bm_err("[%s]is_bat %d miss:%d\n",
			__func__,
			is_bat_exist, gm.plug_miss_count);

		for (i = 0 ; i < 20 ; i++)
			gauge_dev_dump(gm.gdev, NULL, 0);

		if (gm.plug_miss_count >= 3) {
			gauge_enable_interrupt(FG_BAT_PLUGOUT_NO, 0);
			bm_err("[%s]disable FG_BAT_PLUGOUT\n",
				__func__);
			gm.disable_plug_int = 1;
		}
	}

	if (is_bat_exist == 0) {
		battery_notifier(EVENT_BATTERY_PLUG_OUT);
		battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
		wakeup_fg_algo(FG_INTR_BAT_PLUGOUT);
		battery_update(&battery_main);
		fg_int_event(gm.gdev, EVT_INT_BAT_PLUGOUT);
		kernel_power_off();
	}
}

void fg_bat_plugout_int_handler_gm25(void)
{
	bool is_bat_exist;

	is_bat_exist = pmic_is_battery_exist();
	pr_info("%s: bat_exist: %d\n", __func__, is_bat_exist);

	if (fg_interrupt_check() == false)
		return;

	if (is_bat_exist == false) {
		gauge_dev_set_info(gm.gdev, GAUGE_BAT_PLUG_STATUS, 0);
		en_intr_VBATON_UNDET(0);
		battery_notifier(EVENT_BATTERY_PLUG_OUT);
		kernel_power_off();
	}
}


/* ============================================================ */
/* nafg interrupt handler */
/* ============================================================ */
void fg_nafg_int_handler(void)
{
	int nafg_en = 0;
	signed int nafg_cnt = 0;
	signed int nafg_dltv = 0;
	signed int nafg_c_dltv = 0;

	if (fg_interrupt_check() == false)
		return;

	/* 1. Get SW Car value */
	fg_int_event(gm.gdev, EVT_INT_NAFG_CHECK);

	gauge_dev_get_nag_cnt(gm.gdev, &nafg_cnt);
	gauge_dev_get_nag_dltv(gm.gdev, &nafg_dltv);
	gauge_dev_get_nag_c_dltv(gm.gdev, &nafg_c_dltv);

	gm.hw_status.sw_car_nafg_cnt = nafg_cnt;
	gm.hw_status.sw_car_nafg_dltv = nafg_dltv;
	gm.hw_status.sw_car_nafg_c_dltv = nafg_c_dltv;

	gm3_log_dump_nafg(0);
	bm_err(
		"[%s][fg_bat_nafg] [%d:%d:%d]\n",
		__func__,
		nafg_cnt, nafg_dltv, nafg_c_dltv);
	/* battery_dump_nag(); */

	/* 2. Stop HW interrupt*/
	gauge_set_nag_en(nafg_en);

	fg_int_event(gm.gdev, EVT_INT_NAFG);

	/* 3. Notify fg daemon */
	wakeup_fg_algo(FG_INTR_NAG_C_DLTV);

	get_monotonic_boottime(&gm.last_nafg_update_time);
}

/* ============================================================ */
/* coulomb interrupt handler */
/* ============================================================ */
int fg_bat_int1_h_handler(struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_coulomb();

	gm.fg_bat_int1_ht = fg_coulomb + gm.fg_bat_int1_gap;
	gm.fg_bat_int1_lt = fg_coulomb - gm.fg_bat_int1_gap;

	gauge_coulomb_start(&gm.coulomb_plus, gm.fg_bat_int1_gap);
	gauge_coulomb_start(&gm.coulomb_minus, -gm.fg_bat_int1_gap);

	bm_err("[%s] car:%d ht:%d lt:%d gap:%d\n",
		__func__,
		fg_coulomb, gm.fg_bat_int1_ht,
		gm.fg_bat_int1_lt, gm.fg_bat_int1_gap);

	fg_int_event(gm.gdev, EVT_INT_BAT_INT1_HT);
	wakeup_fg_algo(FG_INTR_BAT_INT1_HT);
	sw_check_bat_plugout();
	return 0;
}

int fg_bat_int1_l_handler(struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_coulomb();

	fg_sw_bat_cycle_accu();

	gm.fg_bat_int1_ht = fg_coulomb + gm.fg_bat_int1_gap;
	gm.fg_bat_int1_lt = fg_coulomb - gm.fg_bat_int1_gap;

	gauge_coulomb_start(&gm.coulomb_plus, gm.fg_bat_int1_gap);
	gauge_coulomb_start(&gm.coulomb_minus, -gm.fg_bat_int1_gap);

	bm_err("[%s] car:%d ht:%d lt:%d gap:%d\n",
		__func__,
		fg_coulomb, gm.fg_bat_int1_ht,
		gm.fg_bat_int1_lt, gm.fg_bat_int1_gap);

	fg_int_event(gm.gdev, EVT_INT_BAT_INT1_LT);
	wakeup_fg_algo(FG_INTR_BAT_INT1_LT);
	sw_check_bat_plugout();

	return 0;
}

int fg_bat_int2_h_handler(struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_coulomb();
	bm_err("[%s] car:%d ht:%d\n",
		__func__,
		fg_coulomb, gm.fg_bat_int2_ht);


	fg_sw_bat_cycle_accu();
	fg_int_event(gm.gdev, EVT_INT_BAT_INT2_HT);
	wakeup_fg_algo(FG_INTR_BAT_INT2_HT);
	sw_check_bat_plugout();

	return 0;
}

int fg_bat_int2_l_handler(struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_coulomb();
	bm_err("[%s] car:%d lt:%d\n",
		__func__,
		fg_coulomb, gm.fg_bat_int2_lt);

	fg_sw_bat_cycle_accu();

	fg_int_event(gm.gdev, EVT_INT_BAT_INT2_LT);
	wakeup_fg_algo(FG_INTR_BAT_INT2_LT);
	sw_check_bat_plugout();

	return 0;
}

/* ============================================================ */
/* hw low battery interrupt handler */
/* ============================================================ */
void fg_vbat2_l_int_handler(void)
{
	int lt_ht_en = 0;

	if (fg_interrupt_check() == false)
		return;
	bm_err("[%s]\n", __func__);
	gauge_enable_vbat_high_interrupt(lt_ht_en);
	gauge_enable_vbat_low_interrupt(lt_ht_en);
	wakeup_fg_algo(FG_INTR_VBAT2_L);

	fg_int_event(gm.gdev, EVT_INT_VBAT_L);
	sw_check_bat_plugout();
}

void fg_vbat2_h_int_handler(void)
{
	int lt_ht_en = 0;

	if (fg_interrupt_check() == false)
		return;
	bm_err("[%s]\n", __func__);
	gauge_enable_vbat_high_interrupt(lt_ht_en);
	gauge_enable_vbat_low_interrupt(lt_ht_en);
	disable_shutdown_cond(LOW_BAT_VOLT);
	wakeup_fg_algo(FG_INTR_VBAT2_H);

	fg_int_event(gm.gdev, EVT_INT_VBAT_H);
	sw_check_bat_plugout();
}

/* ============================================================ */
/* periodic */
/* ============================================================ */
void fg_drv_update_hw_status(void)
{
	static bool fg_current_state;
	signed int chr_vol;
	int fg_current, fg_coulomb, bat_vol;
	int plugout_status, tmp, bat_plugout_time;
	int fg_current_iavg;
	bool valid = false;
	static unsigned int cnt;
	ktime_t ktime = ktime_set(60, 0);

	bm_debug("[%s]=>\n", __func__);


	if (gauge_get_hw_version() >= GAUGE_HW_V1000 &&
	gauge_get_hw_version() < GAUGE_HW_V2000)
		fg_int_event(gm.gdev, EVB_PERIODIC_CHECK);

	fg_update_sw_iavg();

	gauge_dev_get_boot_battery_plug_out_status(
		gm.gdev, &plugout_status, &bat_plugout_time);

	fg_current_state = gauge_get_current(&fg_current);
	fg_coulomb = gauge_get_coulomb();
	bat_vol = pmic_get_battery_voltage();
	chr_vol = battery_get_vbus();
	tmp = force_get_tbat(true);

	bm_err("lbat %d %d %d %d\n",
		gm.sw_low_battery_ht_en,
		gm.sw_low_battery_ht_threshold,
		gm.sw_low_battery_lt_en,
		gm.sw_low_battery_lt_threshold);

	bm_err("car[%d,%ld,%ld,%ld,%ld, cycle_car:%d,ncar:%d] c:%d %d vbat:%d vbus:%d soc:%d %d gm3:%d %d %d %d\n",
		fg_coulomb, gm.coulomb_plus.end,
		gm.coulomb_minus.end, gm.soc_plus.end,
		gm.soc_minus.end,
		gm.bat_cycle_car, gm.bat_cycle_ncar,
		fg_current_state, fg_current, bat_vol, chr_vol,
		battery_get_soc(), battery_get_uisoc(),
		gm.disableGM30, fg_cust_data.disable_nafg,
		gm.ntc_disable_nafg, gm.cmd_disable_nafg);

	if (bat_get_debug_level() >= 7)
		gauge_coulomb_dump_list();

	fg_current_iavg = gauge_get_average_current(&valid);
	fg_nafg_monitor();

	bm_err("tmp:%d %d %d hcar2:%d lcar2:%d time:%d sw_iavg:%d %d %d nafg_m:%d %d %d\n",
		tmp, gm.fg_bat_tmp_int_ht, gm.fg_bat_tmp_int_lt,
		gm.fg_bat_int2_ht, gm.fg_bat_int2_lt,
		fg_get_system_sec(), gm.sw_iavg, fg_current_iavg, valid,
		gm.last_nafg_cnt, gm.is_nafg_broken, gm.disable_nafg_int);

	if (cnt % 10 == 0)
		gauge_dev_dump(gm.gdev, NULL, 0);
	cnt++;

	gm3_log_dump(false);
	gm3_log_dump_nafg(1);

	wakeup_fg_algo_cmd(
		FG_INTR_KERNEL_CMD,
		FG_KERNEL_CMD_DUMP_REGULAR_LOG, 0);

	if (bat_get_debug_level() >= BMLOG_DEBUG_LEVEL)
		ktime = ktime_set(10, 0);
	else
		ktime = ktime_set(60, 0);

	hrtimer_start(&gm.fg_hrtimer, ktime, HRTIMER_MODE_REL);

}


int battery_update_routine(void *x)
{
	int ret = 0;
	battery_update_psd(&battery_main);
	while (1) {
		ret = wait_event_interruptible(gm.wait_que,
			(gm.fg_update_flag > 0)
			|| (gm.tracking_cb_flag > 0)
			|| (gm.onepercent_cb_flag > 0));
		if (gm.fg_update_flag > 0) {
			gm.fg_update_flag = 0;
			fg_drv_update_hw_status();
		}
		if (gm.tracking_cb_flag > 0) {
			bm_err("%s wake by tracking_cb_flag:%d\n",
				__func__, gm.tracking_cb_flag);
			gm.tracking_cb_flag = 0;
			wakeup_fg_algo(FG_INTR_FG_TIME);
		}
		if (gm.onepercent_cb_flag > 0) {
			bm_err("%s wake by onepercent_cb_flag:%d\n",
				__func__, gm.onepercent_cb_flag);
			gm.onepercent_cb_flag = 0;
			wakeup_fg_algo_cmd(FG_INTR_FG_TIME, 0, 1);
		}
		if (gm.fix_coverity == 1)
			return 0;
	}
}

void fg_update_routine_wakeup(void)
{
	gm.fg_update_flag = 1;
	wake_up(&gm.wait_que);
}

enum hrtimer_restart fg_drv_thread_hrtimer_func(struct hrtimer *timer)
{
	fg_update_routine_wakeup();

	return HRTIMER_NORESTART;
}

void fg_drv_thread_hrtimer_init(void)
{
	ktime_t ktime = ktime_set(10, 0);

	hrtimer_init(&gm.fg_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gm.fg_hrtimer.function = fg_drv_thread_hrtimer_func;
	hrtimer_start(&gm.fg_hrtimer, ktime, HRTIMER_MODE_REL);
}

void fg_daemon_send_data(
	char *rcv,
	char *ret)
{
	struct fgd_cmd_param_t_6 *prcv;
	struct fgd_cmd_param_t_6 *pret;

	prcv = (struct fgd_cmd_param_t_6 *)rcv;
	pret = (struct fgd_cmd_param_t_6 *)ret;

	bm_trace("%s type:%d, tsize:%d size:%d idx:%d\n",
		__func__,
		prcv->type,
		prcv->total_size,
		prcv->size,
		prcv->idx);

		pret->type = prcv->type;
		pret->total_size = prcv->total_size;
		pret->size = prcv->size;
		pret->idx = prcv->idx;


	switch (prcv->type) {
	case FGD_CMD_PARAM_T_CUSTOM:
		{
			char *ptr;

			if (sizeof(struct fgd_cmd_param_t_custom)
				!= prcv->total_size) {
				bm_err("size is different %d %d\n",
				(int)sizeof(
				struct fgd_cmd_param_t_custom),
				prcv->total_size);
			}

			ptr = (char *)&gm.fg_data;
			memcpy(&ptr[prcv->idx],
				prcv->input,
				prcv->size);

			bm_debug(
				"FG_DATA_TYPE_TABLE type:%d size:%d %d idx:%d\n",
				prcv->type,
				prcv->total_size,
				prcv->size,
				prcv->idx);
		}
		break;
	default:
		bm_err("bad %s 0x%x\n",
			__func__, prcv->type);
		break;

	}

}


void fg_daemon_get_data(
	char *rcv,
	char *ret)
{
	struct fgd_cmd_param_t_6 *prcv;
	struct fgd_cmd_param_t_6 *pret;

	prcv = (struct fgd_cmd_param_t_6 *)rcv;
	pret = (struct fgd_cmd_param_t_6 *)ret;


	bm_err("%s type:%d, tsize:%d size:%d idx:%d\n",
		__func__,
		prcv->type,
		prcv->total_size,
		prcv->size,
		prcv->idx);

		pret->type = prcv->type;
		pret->total_size = prcv->total_size;
		pret->size = prcv->size;
		pret->idx = prcv->idx;


	switch (prcv->type) {
	case FUEL_GAUGE_TABLE_CUSTOM_DATA:
		{
			char *ptr;

			if (sizeof(struct fuel_gauge_table_custom_data)
				!= prcv->total_size) {
				bm_err("size is different %d %d\n",
				(int)sizeof(
				struct fuel_gauge_table_custom_data),
				prcv->total_size);
			}

			ptr = (char *)&fg_table_cust_data;
			memcpy(pret->input, &ptr[prcv->idx], pret->size);
			bm_debug(
				"FG_DATA_TYPE_TABLE type:%d size:%d %d idx:%d\n",
				prcv->type,
				prcv->total_size,
				prcv->size,
				prcv->idx);

		}
		break;
	default:
		bm_err("bad %s 0x%x\n",
			__func__, prcv->type);
		break;

	}

}

void fg_cmd_check(struct fgd_nl_msg_t *msg)
{
	while (msg->fgd_subcmd == 0 &&
		msg->fgd_subcmd_para1 != FGD_NL_MSG_T_HDR_LEN) {
		bm_err("fuel gauge version cmd:%d %d error %d != %d\n",
			msg->fgd_cmd,
			msg->fgd_subcmd,
			FGD_NL_MSG_T_HDR_LEN,
			msg->fgd_subcmd_para1);
		msleep(5000);
		if (gm.fix_coverity == 1)
			return;
	}
}

void fg_daemon_comm_INT_data(char *rcv, char *ret)
{
	struct fgd_cmd_param_t_7 *prcv;
	struct fgd_cmd_param_t_7 *pret;

	prcv = (struct fgd_cmd_param_t_7 *)rcv;
	pret = (struct fgd_cmd_param_t_7 *)ret;


	bm_debug("%s type:%d, in:%d out:%d statu:%d\n",
		__func__,
		prcv->type, prcv->input, prcv->output, prcv->status);

	pret->type = prcv->type;

	switch (prcv->type) {
	case FG_GET_SHUTDOWN_CAR:
		{
			int shutdown_car_diff = 0;

			gauge_dev_get_info(gm.gdev, GAUGE_SHUTDOWN_CAR,
				&shutdown_car_diff);

			memcpy(&pret->output, &shutdown_car_diff,
				sizeof(shutdown_car_diff));

			bm_debug("comm_INT:FG_GET_SHUTDOWN_CAR t:%d in:[%d %d] dif:%d out:%d s:%d\n",
				prcv->type, prcv->input, prcv->output,
				shutdown_car_diff,
				pret->output, pret->status);
		}
		break;

	case FG_GET_NCAR:
		{
			if (gauge_get_hw_version() >= GAUGE_HW_V1000 &&
				gauge_get_hw_version() < GAUGE_HW_V2000) {

				memcpy(&pret->output, &gm.bat_cycle_ncar,
					sizeof(gm.bat_cycle_ncar));

			} else {
				gauge_dev_get_hw_status(gm.gdev,
					&gm.hw_status, 0);

				memcpy(&pret->output,
					&gm.gdev->fg_hw_info.ncar,
					sizeof(gm.gdev->fg_hw_info.ncar));
			}
			bm_debug("comm_INT:FG_GET_NCAR t:%d in:[%d %d] ncar:%d out:%d s:%d\n",
				prcv->type, prcv->input, prcv->output,
				gm.bat_cycle_ncar, pret->output, pret->status);
		}
		break;
	case FG_GET_CURR_1:
		{
			gauge_dev_get_hw_status(gm.gdev,
				&gm.hw_status, 0);

			memcpy(&pret->output,
				&gm.gdev->fg_hw_info.current_1,
				sizeof(gm.gdev->fg_hw_info.current_1));

			bm_debug("comm_INT:FG_GET_CURR_1 t:%d in:[%d %d] cur1:%d out:%d s:%d\n",
				prcv->type, prcv->input, prcv->output,
				gm.gdev->fg_hw_info.current_1,
				pret->output, pret->status);
		}
		break;
	case FG_GET_CURR_2:
		{
			gauge_dev_get_hw_status(gm.gdev,
				&gm.hw_status, 0);

			memcpy(&pret->output,
				&gm.gdev->fg_hw_info.current_2,
				sizeof(gm.gdev->fg_hw_info.current_2));
			bm_debug("comm_INT:FG_GET_CURR_2 t:%d in:[%d %d] cur2:%d out:%d s:%d\n",
				prcv->type, prcv->input, prcv->output,
				gm.gdev->fg_hw_info.current_2,
				pret->output, pret->status);
		}
		break;
	case FG_GET_REFRESH:
		{
			gauge_dev_get_hw_status(gm.gdev, &gm.hw_status, 0);
		}
		break;
	case FG_GET_IS_AGING_RESET:
		{
			int reset = gm.is_reset_aging_factor;

			memcpy(&pret->output, &reset, sizeof(reset));
			gm.is_reset_aging_factor = 0;
		}
		break;
	case FG_GET_SOC_DECIMAL_RATE:
		{
			int decimal_rate = gm.soc_decimal_rate;

			memcpy(&pret->output,
				&decimal_rate, sizeof(decimal_rate));
			bm_debug("[FG_GET_SOC_DECIMAL_RATE]soc_decimal_rate:%d %d\n",
				decimal_rate, gm.soc_decimal_rate);
		}
		break;
	case FG_GET_DIFF_SOC_SET:
		{
			/* 1 = 0.01%, 50 = 0.5% */
			int soc_setting = 1;

			memcpy(&pret->output,
				&soc_setting, sizeof(soc_setting));
		}
		break;
	case FG_GET_IS_FORCE_FULL:
		{
			/* 1 = trust customer full condition */
			/* 0 = using gauge ori full flow */
			int force_full = gm.is_force_full;

			memcpy(&pret->output,
				&force_full, sizeof(force_full));
		}
		break;
	case FG_GET_ZCV_INTR_CURR:
		{
			int zcv_cur = 0;

			gauge_get_zcv_current(&zcv_cur);
			memcpy(&pret->output,
				&zcv_cur, sizeof(zcv_cur));

			bm_err("get FG_GET_ZCV_INTR_CURR %d\n", zcv_cur);
		}
		break;
	case FG_GET_CHARGE_POWER_SEL:
		{
			int charge_power_sel = gm.charge_power_sel;

			memcpy(&pret->output,
				&charge_power_sel, sizeof(charge_power_sel));
			bm_err("charge_power_sel = %d\n", charge_power_sel);
		}
		break;
	case FG_SET_SOC:
		{
			gm.soc = (prcv->input + 50) / 100;
		}
		break;
	case FG_SET_C_D0_SOC:
		{
			fg_cust_data.c_old_d0 = prcv->input;
		}
		break;
	case FG_SET_V_D0_SOC:
		{
			fg_cust_data.v_old_d0 = prcv->input;
		}
		break;
	case FG_SET_C_SOC:
		{
			fg_cust_data.c_soc = prcv->input;
		}
		break;
	case FG_SET_V_SOC:
		{
			fg_cust_data.v_soc = prcv->input;
		}
		break;
	case FG_SET_QMAX_T_AGING:
		break;
	case FG_SET_SAVED_CAR:
		{
			gm.d_saved_car = prcv->input;
		}
		break;
	case FG_SET_AGING_FACTOR:
		{
			gm.aging_factor = prcv->input;
		}
		break;
	case FG_SET_QMAX:
		{
			gm.algo_qmax = prcv->input;
		}
		break;
	case FG_SET_BAT_CYCLES:
		{
			gm.bat_cycle = prcv->input;
		}
		break;
	case FG_SET_NCAR:
		{
			gm.bat_cycle_ncar = prcv->input;
		}
		break;
	case FG_SET_OCV_mah:
		{
			gm.algo_ocv_to_mah = prcv->input;
		}
		break;
	case FG_SET_OCV_Vtemp:
		{
			gm.algo_vtemp = prcv->input;
		}
		break;
	case FG_SET_OCV_SOC:
		{
			gm.algo_ocv_to_soc = prcv->input;
		}
		break;
	case FG_SET_CON0_SOFF_VALID:
		{
			int ori_value = 0;

			gauge_dev_get_info(gm.gdev,
				GAUGE_MONITOR_SOFF_VALIDTIME, &ori_value);

			gauge_dev_set_info(gm.gdev,
				GAUGE_MONITOR_SOFF_VALIDTIME, prcv->input);

			bm_err("set GAUGE_MONITOR_SOFF_VALIDTIME ori:%d, new:%d\n",
				ori_value, prcv->input);
		}
		break;
	case FG_SET_ZCV_INTR_EN:
		{
			int zcv_intr_en = prcv->input;

			if (zcv_intr_en == 0 || zcv_intr_en == 1)
				gauge_set_zcv_interrupt_en(zcv_intr_en);

			bm_err("set zcv_interrupt_en %d\n", zcv_intr_en);
		}
		break;
	default:
		pret->status = -1;
		bm_err("%s type:%d in:%d out:%d,Retun t:%d,in:%d,o:%d,s:%d\n",
			__func__,
			prcv->type, prcv->input, prcv->output,
			pret->type, pret->input, pret->output, pret->status);
		break;
	}
#if 0
	bm_debug("%s type:%d in:%d out:%d,Retun t:%d,in:%d,o:%d,s:%d\n",
		__func__, prcv->type, prcv->input, prcv->output,
		pret->type, pret->input, pret->output, pret->status);
#endif


}

void bmd_ctrl_cmd_from_user(void *nl_data, struct fgd_nl_msg_t *ret_msg)
{
	struct fgd_nl_msg_t *msg;
	static int ptim_vbat, ptim_i;

	msg = nl_data;
	ret_msg->fgd_cmd = msg->fgd_cmd;

	switch (msg->fgd_cmd) {

	case FG_DAEMON_CMD_IS_BAT_PLUGOUT:
		{
			int is_bat_plugout = 0;
			int bat_plugout_time = 0;

			gauge_dev_get_boot_battery_plug_out_status(
				gm.gdev, &is_bat_plugout, &bat_plugout_time);
			ret_msg->fgd_data_len += sizeof(is_bat_plugout);
			memcpy(ret_msg->fgd_data,
				&is_bat_plugout, sizeof(is_bat_plugout));

			bm_debug(
				"[fr] BATTERY_METER_CMD_GET_BOOT_BATTERY_PLUG_STATUS = %d\n",
				is_bat_plugout);
		}
		break;
	case FG_DAEMON_CMD_IS_BAT_EXIST:
		{
			int is_bat_exist = 0;

			is_bat_exist = pmic_is_battery_exist();

			ret_msg->fgd_data_len += sizeof(is_bat_exist);
			memcpy(ret_msg->fgd_data,
				&is_bat_exist, sizeof(is_bat_exist));

			bm_debug(
				"[fr] FG_DAEMON_CMD_IS_BAT_EXIST = %d\n",
				is_bat_exist);
		}
		break;

	case FG_DAEMON_CMD_IS_BAT_CHARGING:
		{
			int is_bat_charging = 0;
			int bat_current = 0;

			/* BAT_DISCHARGING = 0 */
			/* BAT_CHARGING = 1 */
			is_bat_charging = gauge_get_current(&bat_current);

			ret_msg->fgd_data_len += sizeof(is_bat_charging);
			memcpy(ret_msg->fgd_data,
				&is_bat_charging, sizeof(is_bat_charging));

			bm_debug(
				"[fr] FG_DAEMON_CMD_IS_BAT_CHARGING = %d\n",
				is_bat_charging);
		}
		break;

	case FG_DAEMON_CMD_GET_CHARGER_STATUS:
		{
			int charger_status = 0;

			/* charger status need charger API */
			/* CHR_ERR = -1 */
			/* CHR_NORMAL = 0 */
			if (battery_main.BAT_STATUS ==
				POWER_SUPPLY_STATUS_NOT_CHARGING)
				charger_status = -1;
			else
				charger_status = 0;

			ret_msg->fgd_data_len += sizeof(charger_status);
			memcpy(ret_msg->fgd_data,
				&charger_status, sizeof(charger_status));

			bm_debug(
				"[fr] FG_DAEMON_CMD_GET_CHARGER_STATUS = %d\n",
				charger_status);
		}
		break;

	case FG_DAEMON_CMD_GET_FG_HW_CAR:
		{
			int fg_coulomb = 0;

			fg_coulomb = gauge_get_coulomb();
			ret_msg->fgd_data_len += sizeof(fg_coulomb);
			memcpy(ret_msg->fgd_data,
				&fg_coulomb, sizeof(fg_coulomb));

			bm_debug(
				"[fr] BATTERY_METER_CMD_GET_FG_HW_CAR = %d\n",
				fg_coulomb);
		}
		break;
	case FG_DAEMON_CMD_IS_CHARGER_EXIST:
		{
			int is_charger_exist = 0;

#if defined(CONFIG_MACH_MT6877)
			if (battery_main.BAT_STATUS == POWER_SUPPLY_STATUS_CHARGING)
				is_charger_exist = true;
			else
				is_charger_exist = false;
#else
			if (upmu_get_rgs_chrdet() == 0 ||
				mt_usb_is_device() == 0)
				is_charger_exist = false;
			else
				is_charger_exist = true;
#endif

			ret_msg->fgd_data_len += sizeof(is_charger_exist);
			memcpy(ret_msg->fgd_data,
				&is_charger_exist, sizeof(is_charger_exist));

			bm_debug(
				"[fr] FG_DAEMON_CMD_IS_CHARGER_EXIST = %d\n",
				is_charger_exist);
		}
		break;


	case FG_DAEMON_CMD_GET_INIT_FLAG:
		{
			ret_msg->fgd_data_len += sizeof(gm.init_flag);
			memcpy(ret_msg->fgd_data,
				&gm.init_flag, sizeof(gm.init_flag));
			bm_debug(
				"[fr] FG_DAEMON_CMD_GET_INIT_FLAG = %d\n",
				gm.init_flag);
		}
		break;

	case FG_DAEMON_CMD_SET_INIT_FLAG:
		{
			memcpy(&gm.init_flag,
				&msg->fgd_data[0], sizeof(gm.init_flag));

			if (gm.init_flag == 1)
				gauge_dev_set_info(gm.gdev,
					GAUGE_SHUTDOWN_CAR, -99999);

			bm_debug(
				"[fr] FG_DAEMON_CMD_SET_INIT_FLAG = %d\n",
				gm.init_flag);
		}
		break;

	case FG_DAEMON_CMD_GET_TEMPERTURE:	/* fix me */
		{
			bool update;
			int temperture = 0;

			memcpy(&update, &msg->fgd_data[0], sizeof(update));
			temperture = force_get_tbat(update);
			bm_debug("[fr] FG_DAEMON_CMD_GET_TEMPERTURE update = %d tmp:%d\n",
				update, temperture);
			ret_msg->fgd_data_len += sizeof(temperture);
			memcpy(ret_msg->fgd_data,
				&temperture, sizeof(temperture));
			/* gFG_temp = temperture; */
		}
	break;

	case FG_DAEMON_CMD_GET_RAC:
		{
			int rac;

			rac = _get_ptim_rac_val();
			ret_msg->fgd_data_len += sizeof(rac);
			memcpy(ret_msg->fgd_data, &rac, sizeof(rac));
			bm_debug("[fr] FG_DAEMON_CMD_GET_RAC = %d\n", rac);
		}
		break;

	case FG_DAEMON_CMD_GET_DISABLE_NAFG:
		{
			int ret = 0;

			if (gm.ntc_disable_nafg == true)
				ret = 1;
			else
				ret = 0;
			ret_msg->fgd_data_len += sizeof(ret);
			memcpy(ret_msg->fgd_data, &ret, sizeof(ret));
			bm_debug(
				"[fr] FG_DAEMON_CMD_GET_DISABLE_NAFG = %d\n",
				ret);
		}
		break;

	case FG_DAEMON_CMD_SET_DAEMON_PID:
		{

			fg_cmd_check(msg);

			/* check is daemon killed case*/
			if (gm.g_fgd_pid == 0) {
				memcpy(&gm.g_fgd_pid, &msg->fgd_data[0],
					sizeof(gm.g_fgd_pid));
				bm_err("[fr] FG_DAEMON_CMD_SET_DAEMON_PID = %d(first launch)\n",
					gm.g_fgd_pid);
			} else {
				memcpy(&gm.g_fgd_pid, &msg->fgd_data[0],
					sizeof(gm.g_fgd_pid));
				bm_err(
					"[fr]FG_DAEMON_CMD_SET_DAEMON_PID=%d,kill daemon case, %d init_flag:%d\n",
					gm.g_fgd_pid,
					get_ec()->debug_kill_daemontest,
					gm.init_flag);
				/* kill daemon dod_init 14*/
				if (get_ec()->debug_kill_daemontest != 1 &&
					gm.init_flag == 1)
					fg_cust_data.dod_init_sel = 14;
			}

		}
		break;

	case FG_DAEMON_CMD_CHECK_FG_DAEMON_VERSION:
		{
			unsigned int fgd_version;

			memcpy(
				&fgd_version, &msg->fgd_data[0],
				sizeof(fgd_version));
			bm_debug(
				"[fg_res] FG_DAEMON_CMD_CHECK_FG_DAEMON_VERSION = %d\n",
				fgd_version);
		}
		break;

	case FG_DAEMON_CMD_GET_FG_SHUTDOWN_COND:
		{
			unsigned int shutdown_cond = get_shutdown_cond();

			ret_msg->fgd_data_len += sizeof(shutdown_cond);
			memcpy(ret_msg->fgd_data,
				&shutdown_cond, sizeof(shutdown_cond));

			bm_debug("[fr] shutdown_cond = %d\n", shutdown_cond);
		}
		break;

	case FG_DAEMON_CMD_FGADC_RESET:
		{
			bm_err("[fr] fgadc_reset\n");
			gauge_reset_hw();
		}
		break;
	case FG_DAEMON_CMD_SEND_DATA:
		{
			fg_daemon_send_data(&msg->fgd_data[0],
				&ret_msg->fgd_data[0]);
		}
		break;

	case FG_DAEMON_CMD_GET_DATA:
		{
			fg_cmd_check(msg);
			fg_daemon_get_data(&msg->fgd_data[0],
				&ret_msg->fgd_data[0]);
			ret_msg->fgd_data_len =
				sizeof(struct fgd_cmd_param_t_6);
		}
		break;
	case FG_DAEMON_CMD_COMMUNICATION_INT:
		{
			fg_daemon_comm_INT_data(&msg->fgd_data[0],
				&ret_msg->fgd_data[0]);
			ret_msg->fgd_data_len =
				sizeof(struct fgd_cmd_param_t_7);
		}
		break;

	case FG_DAEMON_CMD_GET_CUSTOM_SETTING:
		{
			fg_cmd_check(msg);
			bm_err("[fr] data len:%d custom data length = %d\n",
				(int)sizeof(fg_cust_data),
				ret_msg->fgd_data_len);

			memcpy(ret_msg->fgd_data,
				&fg_cust_data, sizeof(fg_cust_data));
			ret_msg->fgd_data_len += sizeof(fg_cust_data);


			bm_debug("[fr] data len:%d custom data length = %d\n",
				(int)sizeof(fg_cust_data),
				ret_msg->fgd_data_len);
		}
		break;
	case FG_DAEMON_CMD_GET_BH_DATA:
		{
			ret_msg->fgd_data_len += sizeof(struct ag_center_data_st);
			memcpy(ret_msg->fgd_data,
				&gm.bh_data, sizeof(struct ag_center_data_st));
		}
		break;

	case FG_DAEMON_CMD_GET_PTIM_VBAT:
		{
			unsigned int ptim_bat_vol = 0;
			signed int ptim_R_curr = 0;
			int curr_bat_vol = 0;

			if (gm.init_flag == 1) {
				_do_ptim();
				ptim_bat_vol = _get_ptim_bat_vol();
				ptim_R_curr = _get_ptim_R_curr();
				bm_warn("[fr] PTIM V %d I %d\n",
					ptim_bat_vol, ptim_R_curr);
			} else {
				ptim_bat_vol = gm.ptim_lk_v;
				ptim_R_curr = gm.ptim_lk_i;

				curr_bat_vol =
					battery_get_bat_voltage() * 10;
				if (gm.ptim_lk_v == 0)
					ptim_bat_vol = curr_bat_vol;

				bm_err("[fr] PTIM_LK V %d I %d,curr_bat_vol=%d\n",
					ptim_bat_vol, ptim_R_curr,
					curr_bat_vol);
			}
			ptim_vbat = ptim_bat_vol;
			ptim_i = ptim_R_curr;
			ret_msg->fgd_data_len += sizeof(ptim_vbat);
			memcpy(ret_msg->fgd_data,
				&ptim_vbat, sizeof(ptim_vbat));
		}
		break;

	case FG_DAEMON_CMD_GET_PTIM_I:
		{
			ret_msg->fgd_data_len += sizeof(ptim_i);
			memcpy(ret_msg->fgd_data, &ptim_i, sizeof(ptim_i));
			bm_debug(
				"[fr] FG_DAEMON_CMD_GET_PTIM_I = %d\n", ptim_i);
		}
		break;

	case FG_DAEMON_CMD_GET_HW_OCV:
	{
		int voltage = 0;

		battery_main.BAT_batt_temp = force_get_tbat(true);
		voltage = gauge_get_hwocv();
		gm.hw_status.hw_ocv = voltage;

		ret_msg->fgd_data_len += sizeof(voltage);
		memcpy(ret_msg->fgd_data, &voltage, sizeof(voltage));
		bm_debug("[fr] FG_DAEMON_CMD_GET_HW_OCV = %d\n", voltage);

		gm.log.phone_state = 1;
		gm.log.ps_logtime = fg_get_log_sec();
		gm.log.ps_system_time = fg_get_system_sec();
		gm3_log_dump(true);

	}
	break;

	case FG_DAEMON_CMD_SET_SW_OCV:
	{
		int _sw_ocv;

		memcpy(&_sw_ocv, &msg->fgd_data[0], sizeof(_sw_ocv));
		gm.hw_status.sw_ocv = _sw_ocv;

		bm_debug("[fr] FG_DAEMON_CMD_SET_SW_OCV = %d\n", _sw_ocv);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_BAT_INT1_GAP:
	{
		int fg_coulomb = 0;

		fg_coulomb = gauge_get_coulomb();

		memcpy(&gm.fg_bat_int1_gap,
			&msg->fgd_data[0], sizeof(gm.fg_bat_int1_gap));

		gm.fg_bat_int1_ht = fg_coulomb + gm.fg_bat_int1_gap;
		gm.fg_bat_int1_lt = fg_coulomb - gm.fg_bat_int1_gap;
		gauge_coulomb_start(&gm.coulomb_plus, gm.fg_bat_int1_gap);
		gauge_coulomb_start(&gm.coulomb_minus, -gm.fg_bat_int1_gap);

		bm_err(
			"[fr] FG_DAEMON_CMD_SET_FG_BAT_INT1_GAP = %d car:%d\n",
			gm.fg_bat_int1_gap, fg_coulomb);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP:
	{
		memcpy(&gm.fg_bat_int2_ht,
			&msg->fgd_data[0], sizeof(gm.fg_bat_int2_ht));
		gauge_coulomb_start(&gm.soc_plus, gm.fg_bat_int2_ht);
		bm_err(
			"[fr][fg_bat_int2] FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP = %d\n",
			gm.fg_bat_int2_ht);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_BAT_INT2_LT_GAP:
	{
		memcpy(&gm.fg_bat_int2_lt,
			&msg->fgd_data[0], sizeof(gm.fg_bat_int2_lt));
		gauge_coulomb_start(&gm.soc_minus, -gm.fg_bat_int2_lt);
		bm_err(
			"[fr][fg_bat_int2] FG_DAEMON_CMD_SET_FG_BAT_INT2_LT_GAP = %d\n",
			gm.fg_bat_int2_lt);
	}
	break;

	case FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT:
	{
		memcpy(&gm.fg_bat_int2_ht_en,
			&msg->fgd_data[0], sizeof(gm.fg_bat_int2_ht_en));
		if (gm.fg_bat_int2_ht_en == 0)
			gauge_coulomb_stop(&gm.soc_plus);
		bm_debug(
			"[fr][fg_bat_int2] FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT = %d\n",
			gm.fg_bat_int2_ht_en);
	}
	break;

	case FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_LT:
	{
		memcpy(&gm.fg_bat_int2_lt_en,
			&msg->fgd_data[0], sizeof(gm.fg_bat_int2_lt_en));
		if (gm.fg_bat_int2_lt_en == 0)
			gauge_coulomb_stop(&gm.soc_minus);

		bm_debug(
			"[fr][fg_bat_int2] FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_LT = %d\n",
			gm.fg_bat_int2_lt_en);
	}

	break;


	case FG_DAEMON_CMD_SET_FG_BAT_TMP_C_GAP:
	{
		int tmp = force_get_tbat(true);

		memcpy(&gm.fg_bat_tmp_c_int_gap,
			&msg->fgd_data[0], sizeof(gm.fg_bat_tmp_c_int_gap));

		gm.fg_bat_tmp_c_ht = tmp + gm.fg_bat_tmp_c_int_gap;
		gm.fg_bat_tmp_c_lt = tmp - gm.fg_bat_tmp_c_int_gap;

		bm_warn(
			"[fr][FG_TEMP_INT] FG_DAEMON_CMD_SET_FG_BAT_TMP_C_GAP = %d ht:%d lt:%d\n",
			gm.fg_bat_tmp_c_int_gap,
			gm.fg_bat_tmp_c_ht,
			gm.fg_bat_tmp_c_lt);

	}
	break;

	case FG_DAEMON_CMD_SET_FG_BAT_TMP_GAP:
	{
		int tmp = force_get_tbat(true);

		memcpy(
			&gm.fg_bat_tmp_int_gap, &msg->fgd_data[0],
			sizeof(gm.fg_bat_tmp_int_gap));

		gm.fg_bat_tmp_ht = tmp + gm.fg_bat_tmp_int_gap;
		gm.fg_bat_tmp_lt = tmp - gm.fg_bat_tmp_int_gap;

		bm_warn(
			"[fr][FG_TEMP_INT] FG_DAEMON_CMD_SET_FG_BAT_TMP_GAP = %d ht:%d lt:%d\n",
			gm.fg_bat_tmp_int_gap,
			gm.fg_bat_tmp_ht, gm.fg_bat_tmp_lt);

	}
	break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_DURATION_TIME:
	{
		signed int time = 0;

		time = gm.pl_shutdown_time;

		ret_msg->fgd_data_len += sizeof(time);
		memcpy(ret_msg->fgd_data, &time, sizeof(time));
		bm_debug(
			"[fr] FG_DAEMON_CMD_GET_SHUTDOWN_DURATION_TIME = %d\n",
			time);
	}
	break;

	case FG_DAEMON_CMD_GET_BAT_PLUG_OUT_TIME:
	{
		int p1 = 0, p2 = 0;
		unsigned int time = 0;

		gauge_dev_get_boot_battery_plug_out_status(gm.gdev, &p1, &p2);
		time = p2;

		ret_msg->fgd_data_len += sizeof(time);
		memcpy(ret_msg->fgd_data, &time, sizeof(time));
		bm_debug(
			"[fr] FG_DAEMON_CMD_GET_BAT_PLUG_OUT_TIME = %d\n",
			time);
	}
	break;

	case FG_DAEMON_CMD_GET_VBAT:
	{
		unsigned int vbat = 0;

		vbat = battery_get_bat_voltage() * 10;
		ret_msg->fgd_data_len += sizeof(vbat);
		memcpy(ret_msg->fgd_data, &vbat, sizeof(vbat));
		bm_debug("[fr] FG_DAEMON_CMD_GET_VBAT = %d\n", vbat);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_RESET_RTC_STATUS:
	{
		int fg_reset_rtc;

		memcpy(&fg_reset_rtc, &msg->fgd_data[0], sizeof(fg_reset_rtc));

		gauge_dev_set_reset_status(gm.gdev, fg_reset_rtc);

		bm_debug(
			"[fr] BATTERY_METER_CMD_SET_FG_RESET_RTC_STATUS = %d\n",
			fg_reset_rtc);
	}
	break;

	case FG_DAEMON_CMD_SET_IS_FG_INITIALIZED:
	{
		int fg_reset;

		memcpy(&fg_reset, &msg->fgd_data[0], sizeof(fg_reset));
		gauge_dev_set_gauge_initialized(gm.gdev, fg_reset);

		bm_debug(
			"[fr] BATTERY_METER_CMD_SET_FG_RESET_STATUS = %d\n",
			fg_reset);
	}
	break;


	case FG_DAEMON_CMD_GET_IS_FG_INITIALIZED:
	{
		int fg_reset = 0;

		gauge_dev_is_gauge_initialized(gm.gdev, &fg_reset);
		ret_msg->fgd_data_len += sizeof(fg_reset);
		memcpy(ret_msg->fgd_data, &fg_reset, sizeof(fg_reset));
		bm_debug(
			"[fr] BATTERY_METER_CMD_GET_FG_RESET_STATUS = %d\n",
			fg_reset);
	}
	break;

	case FG_DAEMON_CMD_IS_HWOCV_UNRELIABLE:
	{
		int is_hwocv_unreliable;

		is_hwocv_unreliable = gm.hw_status.flag_hw_ocv_unreliable;
		ret_msg->fgd_data_len += sizeof(is_hwocv_unreliable);
		memcpy(ret_msg->fgd_data,
			&is_hwocv_unreliable, sizeof(is_hwocv_unreliable));
		bm_debug(
			"[fr] FG_DAEMON_CMD_IS_HWOCV_UNRELIABLE = %d\n",
			is_hwocv_unreliable);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_TIME:
	{
		int secs;
		struct timespec time, time_now, end_time;
		ktime_t ktime;

		memcpy(&secs, &msg->fgd_data[0], sizeof(secs));

		if (secs != 0 && secs > 0) {
			get_monotonic_boottime(&time_now);
			time.tv_sec = secs;
			time.tv_nsec = 0;

			end_time = timespec_add(time_now, time);
			ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);

			if (msg->fgd_subcmd_para1 == 0)
				alarm_start(&gm.tracking_timer, ktime);
			else
				alarm_start(&gm.one_percent_timer, ktime);
		} else {
			if (msg->fgd_subcmd_para1 == 0)
				alarm_cancel(&gm.tracking_timer);
			else
				alarm_cancel(&gm.one_percent_timer);
		}

		bm_err("[fr] FG_DAEMON_CMD_SET_FG_TIME = %d cmd:%d %d\n",
			secs,
			msg->fgd_subcmd, msg->fgd_subcmd_para1);
	}
	break;

	case FG_DAEMON_CMD_GET_FG_TIME:
	{
		int now_time_secs;

		now_time_secs = fg_get_system_sec();
		ret_msg->fgd_data_len += sizeof(now_time_secs);
		memcpy(ret_msg->fgd_data,
			&now_time_secs, sizeof(now_time_secs));
		bm_debug(
			"[fr] FG_DAEMON_CMD_GET_NOW_TIME = %d\n",
			now_time_secs);
	}
	break;

	case FG_DAEMON_CMD_GET_ZCV:
	{
		int zcv = 0;

		gauge_get_zcv(&zcv);
		ret_msg->fgd_data_len += sizeof(zcv);
		memcpy(ret_msg->fgd_data, &zcv, sizeof(zcv));
		bm_debug("[fr] FG_DAEMON_CMD_GET_ZCV = %d\n", zcv);
	}
	break;

	case FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_CNT:
	{
		int cnt = 0;
		int update;

		memcpy(&update, &msg->fgd_data[0], sizeof(update));

		if (update == 1)
			gauge_dev_get_nag_cnt(gm.gdev, &cnt);
		else
			cnt = gm.hw_status.sw_car_nafg_cnt;

		ret_msg->fgd_data_len += sizeof(cnt);
		memcpy(ret_msg->fgd_data, &cnt, sizeof(cnt));

		bm_debug(
			"[fr] BATTERY_METER_CMD_GET_SW_CAR_NAFG_CNT = %d\n",
			cnt);
	}
	break;

	case FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_DLTV:
	{
		int dltv = 0;
		int update;

		memcpy(&update, &msg->fgd_data[0], sizeof(update));

		if (update == 1)
			gauge_dev_get_nag_dltv(gm.gdev, &dltv);
		else
			dltv = gm.hw_status.sw_car_nafg_dltv;

		ret_msg->fgd_data_len += sizeof(dltv);
		memcpy(ret_msg->fgd_data, &dltv, sizeof(dltv));

		bm_debug(
			"[fr] BATTERY_METER_CMD_GET_SW_CAR_NAFG_DLTV = %d\n",
			dltv);
	}
	break;

	case FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_C_DLTV:
	{
		int c_dltv = 0;
		int update;

		memcpy(&update, &msg->fgd_data[0], sizeof(update));

		if (update == 1)
			gauge_dev_get_nag_c_dltv(gm.gdev, &c_dltv);
		else
			c_dltv = gm.hw_status.sw_car_nafg_c_dltv;

		ret_msg->fgd_data_len += sizeof(c_dltv);
		memcpy(ret_msg->fgd_data, &c_dltv, sizeof(c_dltv));

		bm_debug(
			"[fr] BATTERY_METER_CMD_GET_SW_CAR_NAFG_C_DLTV = %d\n",
			c_dltv);
	}
	break;

	case FG_DAEMON_CMD_SET_NAG_ZCV_EN:
	{
		int nafg_zcv_en;

		memcpy(&nafg_zcv_en, &msg->fgd_data[0], sizeof(nafg_zcv_en));

		gauge_set_nag_en(nafg_zcv_en);

		bm_trace(
			"[fr] FG_DAEMON_CMD_SET_NAG_ZCV_EN = %d\n",
			nafg_zcv_en);
	}
	break;

	case FG_DAEMON_CMD_SET_NAG_ZCV:
	{
		int nafg_zcv;

		memcpy(&nafg_zcv, &msg->fgd_data[0], sizeof(nafg_zcv));

		gauge_dev_set_nag_zcv(gm.gdev, nafg_zcv);
		gm.log.nafg_zcv = nafg_zcv;

		bm_debug("[fr] BATTERY_METER_CMD_SET_NAG_ZCV = %d\n", nafg_zcv);
	}
	break;

	case FG_DAEMON_CMD_SET_NAG_C_DLTV:
	{
		int nafg_c_dltv;

		memcpy(&nafg_c_dltv, &msg->fgd_data[0], sizeof(nafg_c_dltv));

		gauge_dev_set_nag_c_dltv(gm.gdev, nafg_c_dltv);
		gauge_set_nag_en(1);

		bm_debug(
			"[fr] BATTERY_METER_CMD_SET_NAG_C_DLTV = %d\n",
			nafg_c_dltv);
	}
	break;


	case FG_DAEMON_CMD_SET_ZCV_INTR:
	{
		int fg_zcv_current;

		memcpy(&fg_zcv_current,
			&msg->fgd_data[0], sizeof(fg_zcv_current));

		gauge_dev_set_zcv_interrupt_threshold(gm.gdev, fg_zcv_current);
		gauge_set_zcv_interrupt_en(1);

		bm_debug(
			"[fr] BATTERY_METER_CMD_SET_ZCV_INTR = %d\n",
			fg_zcv_current);
	}
	break;

	case FG_DAEMON_CMD_SET_BAT_PLUGOUT_INTR:
	{
		int fg_bat_plugout_en;

		memcpy(&fg_bat_plugout_en,
			&msg->fgd_data[0], sizeof(fg_bat_plugout_en));

		gauge_dev_enable_bat_plugout_interrupt(
			gm.gdev, fg_bat_plugout_en);

		bm_debug(
			"[fr] BATTERY_METER_CMD_SET_BAT_PLUGOUT_INTR_EN = %d\n",
			fg_bat_plugout_en);
	}
	break;

	case FG_DAEMON_CMD_SET_IAVG_INTR:
	{
		bm_debug("[fr] FG_DAEMON_CMD_SET_IAVG_INTR is removed\n");
	}
	break;

	case FG_DAEMON_CMD_SET_FG_QUSE:/*useless*/
	{
		int fg_quse;

		memcpy(&fg_quse, &msg->fgd_data[0], sizeof(fg_quse));

		bm_debug("[fr] FG_DAEMON_CMD_SET_FG_QUSE = %d\n", fg_quse);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_DC_RATIO:
	{
		int fg_dc_ratio;

		memcpy(&fg_dc_ratio, &msg->fgd_data[0], sizeof(fg_dc_ratio));

		bm_debug(
			"[fr] BATTERY_METER_CMD_SET_FG_DC_RATIO = %d\n",
			fg_dc_ratio);
	}
	break;

	case FG_DAEMON_CMD_SET_BATTERY_CYCLE_THRESHOLD:
	{
		memcpy(&gm.bat_cycle_thr,
			&msg->fgd_data[0], sizeof(gm.bat_cycle_thr));

		gauge_dev_set_battery_cycle_interrupt(
			gm.gdev, gm.bat_cycle_thr);

		bm_err(
			"[fr] FG_DAEMON_CMD_SET_BATTERY_CYCLE_THRESHOLD = %d\n",
			gm.bat_cycle_thr);

		fg_sw_bat_cycle_accu();
	}
	break;

	case FG_DAEMON_CMD_SOFF_RESET:
	{
		gauge_dev_reset_shutdown_time(gm.gdev);
		bm_debug("[fr] BATTERY_METER_CMD_SOFF_RESET\n");
	}
	break;

	case FG_DAEMON_CMD_NCAR_RESET:
	{
		gauge_reset_ncar();
		bm_debug("[fr] BATTERY_METER_CMD_NCAR_RESET\n");
	}
	break;

	case FG_DAEMON_CMD_GET_IMIX:
	{
		int imix = UNIT_TRANS_10 * get_imix();

		ret_msg->fgd_data_len += sizeof(imix);
		memcpy(ret_msg->fgd_data, &imix, sizeof(imix));
		bm_debug("[fr] FG_DAEMON_CMD_GET_IMIX = %d\n", imix);
	}
	break;

	case FG_DAEMON_CMD_IS_BATTERY_CYCLE_RESET:
	{
		int reset = gm.is_reset_battery_cycle;

		ret_msg->fgd_data_len += sizeof(reset);
		memcpy(ret_msg->fgd_data, &reset, sizeof(reset));
		bm_debug(
			"[fr] FG_DAEMON_CMD_IS_BATTERY_CYCLE_RESET = %d\n",
			reset);
		gm.is_reset_battery_cycle = false;
	}
	break;

	case FG_DAEMON_CMD_GET_AGING_FACTOR_CUST:
	{
		int aging_factor_cust = 0;
		int origin_aging_factor;

		memcpy(&origin_aging_factor,
			&msg->fgd_data[0], sizeof(origin_aging_factor));
		aging_factor_cust =
			get_customized_aging_factor(origin_aging_factor);

		ret_msg->fgd_data_len += sizeof(aging_factor_cust);
		memcpy(ret_msg->fgd_data,
			&aging_factor_cust, sizeof(aging_factor_cust));
		bm_debug(
			"[fr] FG_DAEMON_CMD_GET_AGING_FACTOR_CUST = %d\n",
			aging_factor_cust);
	}
	break;

	case FG_DAEMON_CMD_GET_D0_C_SOC_CUST:
	{
		int d0_c_cust = 0;
		int origin_d0_c;

		memcpy(&origin_d0_c, &msg->fgd_data[0], sizeof(origin_d0_c));
		d0_c_cust = get_customized_d0_c_soc(origin_d0_c);

		ret_msg->fgd_data_len += sizeof(d0_c_cust);
		memcpy(ret_msg->fgd_data, &d0_c_cust, sizeof(d0_c_cust));
		bm_err(
			"[fr] FG_DAEMON_CMD_GET_D0_C_CUST = %d\n",
			d0_c_cust);
	}
	break;

	case FG_DAEMON_CMD_GET_D0_V_SOC_CUST:
	{
		int d0_v_cust = 0;
		int origin_d0_v;

		memcpy(&origin_d0_v, &msg->fgd_data[0], sizeof(origin_d0_v));
		d0_v_cust = get_customized_d0_v_soc(origin_d0_v);

		ret_msg->fgd_data_len += sizeof(d0_v_cust);
		memcpy(ret_msg->fgd_data, &d0_v_cust, sizeof(d0_v_cust));
		bm_err(
			"[fr] FG_DAEMON_CMD_GET_D0_V_CUST = %d\n",
			d0_v_cust);
	}
	break;

	case FG_DAEMON_CMD_GET_UISOC_CUST:
	{
		int uisoc_cust = 0;
		int origin_uisoc;

		memcpy(&origin_uisoc, &msg->fgd_data[0], sizeof(origin_uisoc));
		uisoc_cust = get_customized_uisoc(origin_uisoc);

		ret_msg->fgd_data_len += sizeof(uisoc_cust);
		memcpy(ret_msg->fgd_data, &uisoc_cust, sizeof(uisoc_cust));
		bm_err(
			"[fr] FG_DAEMON_CMD_GET_UISOC_CUST = %d\n",
			uisoc_cust);
	}
	break;

	case FG_DAEMON_CMD_IS_KPOC:
	{
		int is_kpoc = is_kernel_power_off_charging();

		ret_msg->fgd_data_len += sizeof(is_kpoc);
		memcpy(ret_msg->fgd_data, &is_kpoc, sizeof(is_kpoc));
		bm_debug(
			"[fr] FG_DAEMON_CMD_IS_KPOC = %d\n", is_kpoc);
	}
	break;

	case FG_DAEMON_CMD_GET_NAFG_VBAT:
	{
		int nafg_vbat;

		nafg_vbat = gauge_get_nag_vbat();
		ret_msg->fgd_data_len += sizeof(nafg_vbat);
		memcpy(ret_msg->fgd_data, &nafg_vbat, sizeof(nafg_vbat));
		bm_debug(
			"[fr] FG_DAEMON_CMD_GET_NAFG_VBAT = %d\n",
			nafg_vbat);
		gauge_dev_dump(gm.gdev, NULL, 1);
	}
	break;

	case FG_DAEMON_CMD_GET_HW_INFO:
	{
		int intr_no = 0;
		int shutdown_car_diff = 0;
		int cmdtype = 0;

		cmdtype = msg->fgd_subcmd_para1;
		if (cmdtype == HW_INFO_SHUTDOWN_CAR) {
			gauge_dev_get_info(gm.gdev, GAUGE_SHUTDOWN_CAR,
				&shutdown_car_diff);

			ret_msg->fgd_data_len += sizeof(shutdown_car_diff);
			memcpy(ret_msg->fgd_data, &shutdown_car_diff,
				sizeof(shutdown_car_diff));

			bm_err("FG_DAEMON_CMD_GET_HW_INFO (GAUGE_SHUTDOWN_CAR): %d, cmdtype:%d\n",
				shutdown_car_diff, cmdtype);
		} else if (cmdtype == HW_INFO_NCAR) {
			if (gauge_get_hw_version() >= GAUGE_HW_V1000 &&
				gauge_get_hw_version() < GAUGE_HW_V2000) {

				ret_msg->fgd_data_len +=
					sizeof(gm.bat_cycle_ncar);

				memcpy(ret_msg->fgd_data, &gm.bat_cycle_ncar,
					sizeof(gm.bat_cycle_ncar));
			} else {
				/* TODO GM3 */
				gauge_dev_get_hw_status(gm.gdev,
					&gm.hw_status, intr_no);

				ret_msg->fgd_data_len +=
					sizeof(gm.gdev->fg_hw_info.ncar);

				memcpy(ret_msg->fgd_data,
					&gm.gdev->fg_hw_info.ncar,
					sizeof(gm.gdev->fg_hw_info.ncar));
			}

			bm_trace(
				"FG_DAEMON_CMD_GET_HW_INFO(NCAR):%d %d, cmdtype:%d\n",
				gm.bat_cycle_ncar,
				gm.gdev->fg_hw_info.ncar, cmdtype);

		} else {
			memcpy(&intr_no, &msg->fgd_data[0], sizeof(intr_no));
			intr_no = gauge_dev_get_hw_status(gm.gdev,
				&gm.hw_status, intr_no);
		}

		bm_trace(
			"[fr] FG_DAEMON_CMD_GET_HW_INFO = %d\n", intr_no);
	}
	break;

	case FG_DAEMON_CMD_GET_FG_CURRENT_AVG:
	{
		int fg_current_iavg = gm.sw_iavg;
		bool valid;

		if (gauge_get_hw_version() >= GAUGE_HW_V2000)
			fg_current_iavg =
				gauge_get_average_current(&valid);

		ret_msg->fgd_data_len += sizeof(fg_current_iavg);
		memcpy(ret_msg->fgd_data,
			&fg_current_iavg, sizeof(fg_current_iavg));
		bm_debug(
			"[fr] FG_DAEMON_CMD_GET_FG_CURRENT_AVG = %d %d v:%d\n",
			fg_current_iavg, gm.sw_iavg, gauge_get_hw_version());
	}
	break;

	case FG_DAEMON_CMD_GET_FG_CURRENT_IAVG_VALID:
	{
		bool valid = false;
		int iavg_valid = true;

		if (gauge_get_hw_version() >= GAUGE_HW_V2000) {
			gauge_get_average_current(&valid);
			iavg_valid = valid;
		}

		ret_msg->fgd_data_len += sizeof(iavg_valid);
		memcpy(ret_msg->fgd_data, &iavg_valid, sizeof(iavg_valid));
		bm_err(
			"[fr] FG_DAEMON_CMD_GET_FG_CURRENT_IAVG_VALID = %d\n",
			iavg_valid);
	}
	break;


	case FG_DAEMON_CMD_SET_KERNEL_SOC:
	{
		int daemon_soc;
		int soc_type;

		soc_type = msg->fgd_subcmd_para1;

		memcpy(&daemon_soc, &msg->fgd_data[0], sizeof(daemon_soc));
		if (soc_type == 0)
			gm.soc = (daemon_soc + 50) / 100;

		bm_err(
		"[fg_res]FG_DAEMON_CMD_SET_KERNEL_SOC = %d %d, type:%d\n",
		daemon_soc, gm.soc, soc_type);

	}
	break;

	case FG_DAEMON_CMD_SET_KERNEL_UISOC:
	{
		int daemon_ui_soc;
		int old_uisoc;
		struct timespec now_time, diff;

		memcpy(&daemon_ui_soc, &msg->fgd_data[0],
			sizeof(daemon_ui_soc));

		if (daemon_ui_soc < 0) {
			bm_err("FG_DAEMON_CMD_SET_KERNEL_UISOC error,daemon_ui_soc:%d\n",
				daemon_ui_soc);
			daemon_ui_soc = 0;
		}

		fg_cust_data.ui_old_soc = daemon_ui_soc;
		old_uisoc = gm.ui_soc;

		if (gm.disableGM30 == true)
			gm.ui_soc = 50;
		else
			gm.ui_soc = (daemon_ui_soc + 50) / 100;

		/* when UISOC changes, check the diff time for smooth */
		if (old_uisoc != gm.ui_soc) {
			get_monotonic_boottime(&now_time);
			diff = timespec_sub(now_time, gm.uisoc_oldtime);

			bm_debug("[fg_res] FG_DAEMON_CMD_SET_KERNEL_UISOC = %d %d GM3:%d old:%d diff=%ld\n",
				daemon_ui_soc, gm.ui_soc,
				gm.disableGM30, old_uisoc, diff.tv_sec);
			gm.uisoc_oldtime = now_time;

			battery_main.BAT_CAPACITY = gm.ui_soc;
			battery_update(&battery_main);
		} else {
			bm_debug("[fg_res] FG_DAEMON_CMD_SET_KERNEL_UISOC = %d %d GM3:%d\n",
				daemon_ui_soc, gm.ui_soc, gm.disableGM30);
			/* ac_update(&ac_main); */
			battery_main.BAT_CAPACITY = gm.ui_soc;
			battery_update(&battery_main);
		}
	}
	break;

	case FG_DAEMON_CMD_SET_KERNEL_INIT_VBAT:
	{
		int daemon_init_vbat;

		memcpy(&daemon_init_vbat, &msg->fgd_data[0],
			sizeof(daemon_init_vbat));
		bm_debug(
			"[fr] FG_DAEMON_CMD_SET_KERNEL_INIT_VBAT = %d\n",
			daemon_init_vbat);
	}
	break;


	case FG_DAEMON_CMD_SET_FG_SHUTDOWN_COND:
	{
		int shutdown_cond;

		memcpy(&shutdown_cond, &msg->fgd_data[0],
			sizeof(shutdown_cond));
		set_shutdown_cond(shutdown_cond);
		bm_debug(
			"[fr] FG_DAEMON_CMD_SET_FG_SHUTDOWN_COND = %d\n",
			shutdown_cond);

	}
	break;

	case FG_DAEMON_CMD_ENABLE_FG_VBAT_L_INT:
	{
		int fg_vbat_l_en;

		memcpy(&fg_vbat_l_en, &msg->fgd_data[0], sizeof(fg_vbat_l_en));
		gauge_enable_vbat_low_interrupt(fg_vbat_l_en);
		bm_debug(
			"[fr] FG_DAEMON_CMD_ENABLE_FG_VBAT_L_INT = %d\n",
			fg_vbat_l_en);
	}
	break;

	case FG_DAEMON_CMD_ENABLE_FG_VBAT_H_INT:
	{
		int fg_vbat_h_en;

		memcpy(&fg_vbat_h_en, &msg->fgd_data[0], sizeof(fg_vbat_h_en));
		gauge_enable_vbat_high_interrupt(fg_vbat_h_en);
		bm_debug(
			"[fr] FG_DAEMON_CMD_ENABLE_FG_VBAT_H_INT = %d\n",
			fg_vbat_h_en);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_VBAT_L_TH:
	{
		int fg_vbat_l_thr;

		memcpy(&fg_vbat_l_thr,
			&msg->fgd_data[0], sizeof(fg_vbat_l_thr));
		gauge_set_vbat_low_threshold(fg_vbat_l_thr);
		set_shutdown_vbat_lt(
			fg_vbat_l_thr, fg_cust_data.vbat2_det_voltage1);
		bm_debug(
			"[fr] FG_DAEMON_CMD_SET_FG_VBAT_L_TH = %d\n",
			fg_vbat_l_thr);
	}
	break;

	case FG_DAEMON_CMD_SET_FG_VBAT_H_TH:
	{
		int fg_vbat_h_thr;

		memcpy(&fg_vbat_h_thr, &msg->fgd_data[0],
			sizeof(fg_vbat_h_thr));
		gauge_set_vbat_high_threshold(fg_vbat_h_thr);
		bm_debug(
			"[fr] FG_DAEMON_CMD_SET_FG_VBAT_H_TH=%d\n",
			fg_vbat_h_thr);
	}
	break;

	case FG_DAEMON_CMD_SET_CAR_TUNE_VALUE:
	{
		signed int cali_car_tune;

		memcpy(
			&cali_car_tune,
			&msg->fgd_data[0],
			sizeof(cali_car_tune));
#ifdef CALIBRATE_CAR_TUNE_VALUE_BY_META_TOOL
		bm_err("[fr] cali_car_tune = %d, default = %d, Use [cali_car_tune]\n",
			cali_car_tune, fg_cust_data.car_tune_value);
		fg_cust_data.car_tune_value = cali_car_tune;
#else
		bm_err("[fr] cali_car_tune = %d, default = %d, Use [default]\n",
			cali_car_tune, fg_cust_data.car_tune_value);
#endif
	}
	break;

	case FG_DAEMON_CMD_PRINT_LOG:
	{
		fg_cmd_check(msg);
		bm_err("%s", &msg->fgd_data[0]);
	}
	break;

	case FG_DAEMON_CMD_DUMP_LOG:
	{
		gm.proc_subcmd = msg->fgd_subcmd;
		gm.proc_subcmd_para1 = msg->fgd_subcmd_para1;
		memset(gm.proc_log, 0, 4096);
		strncpy(gm.proc_log, &msg->fgd_data[0],
			strlen(&msg->fgd_data[0]));
		bm_err("[fr] FG_DAEMON_CMD_DUMP_LOG %d %d %d\n",
			msg->fgd_subcmd, msg->fgd_subcmd_para1,
			(int)strlen(&msg->fgd_data[0]));
	}
	break;

	case FG_DAEMON_CMD_GET_RTC_UI_SOC:
	{
		int rtc_ui_soc = 0;

		gauge_dev_get_rtc_ui_soc(gm.gdev, &rtc_ui_soc);

		ret_msg->fgd_data_len += sizeof(rtc_ui_soc);
		memcpy(ret_msg->fgd_data, &rtc_ui_soc,
			sizeof(rtc_ui_soc));
		bm_err("[fr] FG_DAEMON_CMD_GET_RTC_UI_SOC = %d\n",
			rtc_ui_soc);
	}
	break;

	case FG_DAEMON_CMD_SET_RTC_UI_SOC:
	{
		int rtc_ui_soc;

		memcpy(&rtc_ui_soc, &msg->fgd_data[0], sizeof(rtc_ui_soc));

		if (rtc_ui_soc < 0) {
			bm_err("[fr]FG_DAEMON_CMD_SET_RTC_UI_SOC error,rtc_ui_soc=%d\n",
				rtc_ui_soc);

			rtc_ui_soc = 0;
		}

		gauge_dev_set_rtc_ui_soc(gm.gdev, rtc_ui_soc);
		bm_debug(
			"[fr] BATTERY_METER_CMD_SET_RTC_UI_SOC=%d\n",
			rtc_ui_soc);
	}
	break;

	case FG_DAEMON_CMD_SET_CON0_SOC:
	{
		int _soc = 0;

		memcpy(&_soc, &msg->fgd_data[0], sizeof(_soc));
		gauge_dev_set_info(gm.gdev, GAUGE_CON0_SOC, _soc);
		bm_err("[fg_res] FG_DAEMON_CMD_SET_CON0_SOC = %d\n", _soc);
	}
	break;

	case FG_DAEMON_CMD_GET_CON0_SOC:
	{
		int _soc = 0;

		gauge_dev_get_info(gm.gdev, GAUGE_CON0_SOC, &_soc);
		ret_msg->fgd_data_len += sizeof(_soc);
		memcpy(ret_msg->fgd_data, &_soc, sizeof(_soc));

		bm_err("[fg_res] FG_DAEMON_CMD_GET_CON0_SOC = %d\n", _soc);

	}
	break;

	case FG_DAEMON_CMD_SET_NVRAM_FAIL_STATUS:
	{
		int flag = 0;

		memcpy(&flag, &msg->fgd_data[0], sizeof(flag));
		gauge_dev_set_info(gm.gdev, GAUGE_IS_NVRAM_FAIL_MODE, flag);
		bm_debug(
			"[fg_res] FG_DAEMON_CMD_SET_NVRAM_FAIL_STATUS = %d\n",
			flag);
	}
	break;

	case FG_DAEMON_CMD_GET_NVRAM_FAIL_STATUS:
	{
		int flag = 0;

		gauge_dev_get_info(gm.gdev, GAUGE_IS_NVRAM_FAIL_MODE, &flag);
		ret_msg->fgd_data_len += sizeof(flag);
		memcpy(ret_msg->fgd_data, &flag, sizeof(flag));
		bm_err(
			"[fg_res] FG_DAEMON_CMD_GET_NVRAM_FAIL_STATUS = %d\n",
			flag);
	}
	break;

	case FG_DAEMON_CMD_GET_RTC_TWO_SEC_REBOOT:
	{
		int two_sec_reboot_flag;

		two_sec_reboot_flag = gm.pl_two_sec_reboot;
		ret_msg->fgd_data_len += sizeof(two_sec_reboot_flag);
		memcpy(ret_msg->fgd_data,
			&two_sec_reboot_flag,
			sizeof(two_sec_reboot_flag));
		bm_debug(
			"[fr] FG_DAEMON_CMD_GET_RTC_TWO_SEC_REBOOT = %d\n",
			two_sec_reboot_flag);
	}
	break;

	case FG_DAEMON_CMD_GET_RTC_INVALID:
	{
		int rtc_invalid = 0;

		gauge_dev_is_rtc_invalid(gm.gdev, &rtc_invalid);

		ret_msg->fgd_data_len += sizeof(rtc_invalid);
		memcpy(ret_msg->fgd_data, &rtc_invalid, sizeof(rtc_invalid));
		bm_debug(
			"[fr] FG_DAEMON_CMD_GET_RTC_INVALID = %d\n",
			rtc_invalid);
	}
	break;

	case FG_DAEMON_CMD_SET_BATTERY_CAPACITY:
	{
		struct fgd_cmd_param_t_8 param;

		memcpy(&param, &msg->fgd_data[0],
			sizeof(struct fgd_cmd_param_t_8));

		if (param.data[10] != 0 && param.data[11] != 0) {
			gm.show_ag = param.data[10];
			gm.bat_health = param.data[11];
			bm_err("%s:SET_BATTERY_CAPACITY: show_ag:%d, bat_health:%d",
				__func__, gm.show_ag, gm.bat_health);
		}

		bm_debug(
			"[fr] FG_DAEMON_CMD_SET_BATTERY_CAPACITY = %d %d %d %d %d %d %d %d %d %d RM:%d\n",
			param.data[0],
			param.data[1],
			param.data[2],
			param.data[3],
			param.data[4],
			param.data[5],
			param.data[6],
			param.data[7],
			param.data[8],
			param.data[9],
			param.data[4] * param.data[6] / 10000);
	}
	break;

	default:
		bm_err("bad FG_DAEMON_CTRL_CMD_FROM_USER 0x%x\n", msg->fgd_cmd);
		break;
	}			/* switch() */

}

/* ============================================================ */
/* sub function/features */
/* ============================================================ */


/* ============================================================ */
/* init */
/* ============================================================ */

void mtk_battery_init(struct platform_device *dev)
{
	gm.ui_soc = -1;
	gm.soc = -1;
	gm.log_level = BM_DAEMON_DEFAULT_LOG_LEVEL;
	gm.d_log_level = BM_DAEMON_DEFAULT_LOG_LEVEL;

	gm.fixed_uisoc = 0xffff;

	gm.fg_bat_int1_ht = 0xffff;
	gm.fg_bat_int1_lt = 0xffff;

	gm.fg_bat_int2_ht = 0xffff;
	gm.fg_bat_int2_lt = 0xffff;
	gm.fg_bat_int2_ht_en = 0xffff;
	gm.fg_bat_int2_lt_en = 0xffff;

	gm.fg_bat_tmp_ht = 0xffff;
	gm.fg_bat_tmp_lt = 0xffff;
	gm.fg_bat_tmp_c_ht = 0xffff;
	gm.fg_bat_tmp_c_lt = 0xffff;
	gm.fg_bat_tmp_int_ht = 0xffff;
	gm.fg_bat_tmp_int_lt = 0xffff;

	gm.sw_iavg_gap = 3000;

	gm.fixed_bat_tmp = 0xffff;

	mutex_init(&gm.fg_mutex);
	mutex_init(&gm.sw_low_battery_mutex);
	init_waitqueue_head(&gm.wait_que);

	mutex_init(&gm.pmic_intr_mutex);
	mutex_init(&gm.notify_mutex);
	srcu_init_notifier_head(&gm.gm_notify);

	gm.gdev = get_gauge_by_name("gauge");
	if (gm.gdev != NULL) {
		gm.gdev->fg_cust_data = &fg_cust_data;
		gauge_dev_initial(gm.gdev);
	} else
		bm_err("gauge_dev is NULL\n");

	fg_custom_init_from_header();
#ifdef CONFIG_OF
	fg_custom_init_from_dts(dev);
#endif

	gauge_coulomb_service_init();
	gm.coulomb_plus.callback = fg_bat_int1_h_handler;
	gauge_coulomb_consumer_init(&gm.coulomb_plus, &dev->dev, "car+1%");
	gm.coulomb_minus.callback = fg_bat_int1_l_handler;
	gauge_coulomb_consumer_init(&gm.coulomb_minus, &dev->dev, "car-1%");

	gauge_coulomb_consumer_init(&gm.soc_plus, &dev->dev, "soc+1%");
	gm.soc_plus.callback = fg_bat_int2_h_handler;
	gauge_coulomb_consumer_init(&gm.soc_minus, &dev->dev, "soc-1%");
	gm.soc_minus.callback = fg_bat_int2_l_handler;

	kthread_run(battery_update_routine, NULL, "battery_thread");
	fg_drv_thread_hrtimer_init();

	alarm_init(&gm.tracking_timer, ALARM_BOOTTIME,
		tracking_timer_callback);

	alarm_init(&gm.one_percent_timer, ALARM_BOOTTIME,
		one_percent_timer_callback);

	fg_bat_temp_int_init();
	mtk_power_misc_init(dev);

	if (gauge_get_hw_version() >= GAUGE_HW_V1000) {
		/* SW FG nafg */
		pmic_register_interrupt_callback(FG_RG_INT_EN_NAG_C_DLTV,
		fg_nafg_int_handler);

		/* init ZCV INT */
		pmic_register_interrupt_callback(FG_ZCV_NO,
		fg_zcv_int_handler);

		if (gauge_get_hw_version() < GAUGE_HW_V2000) {
// workaround for mt6768
			/*lbat_user_register(&gm.lowbat_service, "fuel gauge",
			fg_cust_data.vbat2_det_voltage3 / 10,
			fg_cust_data.vbat2_det_voltage1 / 10,
			fg_cust_data.vbat2_det_voltage2 / 10,
			fg_update_sw_low_battery_check);

			lbat_user_set_debounce(&gm.lowbat_service,
			fg_cust_data.vbat2_det_time * 1000,
			fg_cust_data.vbat2_det_counter,
			fg_cust_data.vbat2_det_time * 1000,
			fg_cust_data.vbat2_det_counter);*/

			/* sw bat_cycle_car init, gm25 should start from 0 */
			gm.bat_cycle_car = gauge_get_coulomb();
			if (gm.bat_cycle_car < 0)
				gm.bat_cycle_car = 0;
		}
	}

	if (gauge_get_hw_version() >= GAUGE_HW_V2000) {

		/* sw bat_cycle_car init, gm3 may not start from 0 */
		gm.bat_cycle_car = gauge_get_coulomb();

		/* init  cycle int */
		pmic_register_interrupt_callback(FG_N_CHARGE_L_NO,
		fg_cycle_int_handler);

		/* init  IAVG int */
		pmic_register_interrupt_callback(FG_IAVG_H_NO,
		fg_iavg_int_ht_handler);
		pmic_register_interrupt_callback(FG_IAVG_L_NO,
			fg_iavg_int_lt_handler);

		/* init BAT PLUGOUT INT */
		pmic_register_interrupt_callback(FG_BAT_PLUGOUT_NO,
		fg_bat_plugout_int_handler);

		/* TEMPRATURE INT */
		pmic_register_interrupt_callback(FG_RG_INT_EN_BAT_TEMP_H,
		fg_bat_temp_int_h_handler);
		pmic_register_interrupt_callback(FG_RG_INT_EN_BAT_TEMP_L,
			fg_bat_temp_int_l_handler);

		/* VBAT2 L/H */
		pmic_register_interrupt_callback(FG_RG_INT_EN_BAT2_H,
		fg_vbat2_h_int_handler);
		pmic_register_interrupt_callback(FG_RG_INT_EN_BAT2_L,
			fg_vbat2_l_int_handler);

	}

	zcv_filter_init(&gm.zcvf);

	gm3_log_init();

	gauge_dev_get_info(gm.gdev, GAUGE_2SEC_REBOOT, &gm.pl_two_sec_reboot);
	gauge_dev_set_info(gm.gdev, GAUGE_2SEC_REBOOT, 0);

#ifdef _DEA_MODIFY_
	gm.fg_hrtimer.name = "gm.fg_hrtimer";
	gm.wait_que.function = fg_drv_update_hw_status;
	INIT_LIST_HEAD(&gm.wait_que.list);
	gm.wait_que.name = "fg_drv_update_hw_status thread";
#endif

}


void mtk_battery_last_init(struct platform_device *dev)
{
	int fgv;

	fgv = gauge_get_hw_version();
		if (fgv >= GAUGE_HW_V1000
		&& fgv < GAUGE_HW_V2000
		&& gm.disableGM30 == 0) {
			reg_VBATON_UNDET(fg_bat_plugout_int_handler_gm25);
			en_intr_VBATON_UNDET(1);
		}
	sw_iavg_init();
}


/* ============================================================ */
/* battery simulator log */
/* ============================================================ */

void gm3_log_init(void)
{
	gauge_dev_is_gauge_initialized(gm.gdev, &gm.log.is_gauge_initialized);
	gauge_dev_get_rtc_ui_soc(gm.gdev, &gm.log.rtc_ui_soc);
	gauge_dev_is_rtc_invalid(gm.gdev, &gm.log.is_rtc_invalid);
	gauge_dev_get_boot_battery_plug_out_status(
		gm.gdev, &gm.log.is_bat_plugout, &gm.log.bat_plugout_time);

	gauge_dev_get_info(gm.gdev, GAUGE_2SEC_REBOOT, &gm.log.twosec_reboot);
	gauge_dev_get_info(gm.gdev, GAUGE_PL_CHARGING_STATUS,
		&gm.log.pl_charging_status);
	gauge_dev_get_info(gm.gdev, GAUGE_MONITER_PLCHG_STATUS,
		&gm.log.moniter_plchg_status);
	gauge_dev_get_info(gm.gdev, GAUGE_BAT_PLUG_STATUS,
		&gm.log.bat_plug_status);
	gauge_dev_get_info(gm.gdev, GAUGE_IS_NVRAM_FAIL_MODE,
		&gm.log.is_nvram_fail_mode);
	gauge_dev_get_info(gm.gdev, GAUGE_CON0_SOC, &gm.log.con0_soc);
}

void gm3_log_notify(unsigned int interrupt)
{
	if (bat_get_debug_level() < 7)
		return;

	switch (interrupt) {
	case FG_INTR_CHR_FULL:
		{
			gm.log.bat_full_int = 1;
		}
		break;
	case FG_INTR_FG_ZCV:
		{
			gm.log.zcv_int = 1;
			gauge_get_zcv_current(&gm.log.zcv_current);
			gauge_get_zcv(&gm.log.zcv);
		}
		break;
	case FG_INTR_DLPT_SD:
		{
			gm.log.dlpt_sd_int = 1;
		}
		break;
	case FG_INTR_CHARGER_IN:
		{
			gm.log.chr_in_int = 1;
		}
		break;
	default:
		break;
	}

	if (interrupt != FG_INTR_KERNEL_CMD)
		gm3_log_dump(true);
}


void gm3_log_dump_nafg(int type)
{
	unsigned long long logtime;
	int system_time;
	char *title;

	if (type == 0)
		title = "GM3log-nafg";
	else
		title = "GM3log-nint-nafg";

#if defined(__LP64__) || defined(_LP64)
	logtime = sched_clock() / 1000000000;
#else
	logtime = div_u64(sched_clock(), 1000000000);
#endif
	system_time = fg_get_system_sec();

	bm_err("%s %d %llu %d %d %d %d %d\n",
		title,
		system_time,
		logtime,
		gm.log.nafg_zcv,
		gauge_get_nag_vbat(),
		gauge_get_nag_dltv(),
		gauge_get_nag_cnt(),
		gauge_get_nag_c_dltv()
		);
}

void gm3_log_dump(bool force)
{
	static struct timespec last_update_time;
	struct timespec now_time, diff;
	int system_time;
	int car;
	unsigned long long logtime;

	get_monotonic_boottime(&now_time);
	diff = timespec_sub(now_time, last_update_time);
	if (diff.tv_sec < 5)
		return;

	if (bat_get_debug_level() < 7)
		return;

	logtime = fg_get_log_sec();
	system_time = fg_get_system_sec();

	/* charger status need charger API */
	/* CHR_ERR = -1 */
	/* CHR_NORMAL = 0 */
	if (battery_main.BAT_STATUS ==
		POWER_SUPPLY_STATUS_NOT_CHARGING)
		gm.log.chr_status = -1;
	else
		gm.log.chr_status = 0;

	car = gauge_get_coulomb();

	bm_err("GM3log1 %d %llu %d %d %d %d %d %d %d %d %d %d %d\n",
		system_time,
		logtime,
		battery_get_bat_voltage(),
		battery_get_bat_current(),
		battery_get_bat_avg_current(),
		UNIT_TRANS_10 * get_imix(),
		car + gm.log.car_diff,
		force_get_tbat(true),
		upmu_get_rgs_chrdet(),
		pmic_is_battery_exist(),
		_get_ptim_rac_val(),
		gm.gdev->fg_hw_info.iavg_valid,
		gm.log.chr_status);

	bm_err("GM3log2 %llu %d %d %d %d %d %d %d %d\n",
		logtime,
		gm.log.zcv_int,
		gm.log.zcv,
		gm.log.zcv_current,
		is_kernel_power_off_charging(),
		gm.log.ptim_bat,
		gm.log.ptim_cur,
		gm.log.ptim_is_charging,
		battery_get_vbus());

	bm_err("GM3log3 %llu %d %d %d %d\n",
		logtime,
		gm.pl_shutdown_time,
		gm.ptim_lk_v,
		gm.ptim_lk_i,
		gm.log.nafg_zcv
		);

	bm_err("GM3log4 %d %d %d %d %d %d %d %d %d %d %d\n",
		gm.log.is_gauge_initialized,
		gm.log.rtc_ui_soc,
		gm.log.is_rtc_invalid,
		gm.log.is_bat_plugout,
		gm.log.bat_plugout_time,
		gm.log.twosec_reboot,
		gm.log.pl_charging_status,
		gm.log.moniter_plchg_status,
		gm.log.bat_plug_status,
		gm.log.is_nvram_fail_mode,
		gm.log.con0_soc
		);

	if (gm.gdev->fg_hw_info.hw_zcv != 0)
		bm_err("GM3log5 %d %d %d %d %d\n",
		gm.gdev->fg_hw_info.pmic_zcv,
		gm.gdev->fg_hw_info.pmic_zcv_rdy,
		gm.gdev->fg_hw_info.charger_zcv,
			gm.gdev->fg_hw_info.hw_zcv,
			gm.hw_status.flag_hw_ocv_unreliable
		);

	bm_err("GM3log int %llu %d %d %d %d %d\n",
		logtime,
		system_time,
		gm.log.phone_state,
		gm.log.bat_full_int,
		gm.log.dlpt_sd_int,
		gm.log.chr_in_int);

	bm_err("GM3log phone_state %llu %d\n",
		gm.log.ps_logtime,
		gm.log.ps_system_time);


	bm_err("GM3 car:%d car_diff:%d\n",
		car,
		gm.log.car_diff);


	/*reset*/
	gm.log.bat_full_int = 0;
	gm.log.zcv_int = 0;
	gm.log.dlpt_sd_int = 0;
	gm.log.chr_in_int = 0;
	gm.log.phone_state = 0;

}



