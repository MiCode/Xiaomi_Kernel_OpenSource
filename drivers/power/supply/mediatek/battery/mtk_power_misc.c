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

#ifndef _DEA_MODIFY_
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/alarmtimer.h>

#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_battery.h>
#include <mach/mtk_battery_property.h>
#else
#include <string.h>
#include "simulator_kernel.h"
#include "mtk_battery_property.h"
#endif
#include <mtk_gauge_time_service.h>
#include "mtk_battery_internal.h"


struct shutdown_condition {
	bool is_overheat;
	bool is_soc_zero_percent;
	bool is_uisoc_one_percent;
	bool is_under_shutdown_voltage;
	bool is_dlpt_shutdown;
};

struct shutdown_controller {
	struct alarm kthread_fgtimer;
	bool timeout;
	bool overheat;
	wait_queue_head_t  wait_que;
	struct shutdown_condition shutdown_status;
	struct timespec pre_time[SHUTDOWN_FACTOR_MAX];
	int avgvbat;
	bool lowbatteryshutdown;
	int batdata[AVGVBAT_ARRAY_SIZE];
	int batidx;
	struct mutex lock;
	struct notifier_block psy_nb;
};

static struct shutdown_controller sdc;

static int g_vbat_lt;
static int g_vbat_lt_lv1;
static int shutdown_cond_flag;
static int fix_coverity;

static void wake_up_power_misc(struct shutdown_controller *sdd)
{
	sdd->timeout = true;
	wake_up(&sdd->wait_que);
}

static void wake_up_overheat(struct shutdown_controller *sdd)
{
	sdd->overheat = true;
	wake_up(&sdd->wait_que);
}

void set_shutdown_vbat_lt(int vbat_lt, int vbat_lt_lv1)
{
	g_vbat_lt = vbat_lt;
	g_vbat_lt_lv1 = vbat_lt_lv1;
}

int get_shutdown_cond(void)
{
	int ret = 0;
	int vbat = battery_get_bat_voltage();

	if (sdc.shutdown_status.is_soc_zero_percent)
		ret |= 1;
	if (sdc.shutdown_status.is_uisoc_one_percent)
		ret |= 1;
	if (sdc.lowbatteryshutdown)
		ret |= 1;
	bm_err("%s ret:%d %d %d %d vbat:%d\n",
		__func__,
	ret, sdc.shutdown_status.is_soc_zero_percent,
	sdc.shutdown_status.is_uisoc_one_percent,
	sdc.lowbatteryshutdown, vbat);

	return ret;
}

void set_shutdown_cond_flag(int val)
{
	shutdown_cond_flag = val;
}

int get_shutdown_cond_flag(void)
{
	return shutdown_cond_flag;
}

int disable_shutdown_cond(int shutdown_cond)
{
	int now_current;
	int now_is_charging = 0;
	int now_is_kpoc;

	now_current = battery_get_bat_current();
	now_is_kpoc = is_kernel_power_off_charging();

	if (mt_get_charger_type() != CHARGER_UNKNOWN)
		now_is_charging = 1;

	bm_err("%s %d, is kpoc %d curr %d is_charging %d flag:%d lb:%d\n",
		__func__,
		shutdown_cond, now_is_kpoc, now_current, now_is_charging,
		shutdown_cond_flag, battery_get_bat_voltage());

	switch (shutdown_cond) {
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	case LOW_BAT_VOLT:
		sdc.shutdown_status.is_under_shutdown_voltage = false;
		sdc.lowbatteryshutdown = false;
		bm_err("disable LOW_BAT_VOLT avgvbat %d ,threshold:%d %d %d\n",
		sdc.avgvbat,
		BAT_VOLTAGE_HIGH_BOUND, g_vbat_lt, g_vbat_lt_lv1);
		break;
#endif
	default:
		break;
	}
	return 0;
}

int set_shutdown_cond(int shutdown_cond)
{
	int now_current;
	int now_is_charging = 0;
	int now_is_kpoc;
	int vbat;
	struct shutdown_condition *sds;
	int enable_lbat_shutdown;

#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	enable_lbat_shutdown = 1;
#else
	enable_lbat_shutdown = 0;
#endif

	now_current = battery_get_bat_current();
	now_is_kpoc = is_kernel_power_off_charging();
	vbat = battery_get_bat_voltage();
	sds = &sdc.shutdown_status;

	if (now_current >= 0)
		now_is_charging = 1;

	bm_err("%s %d %d kpoc %d curr %d is_charging %d flag:%d lb:%d\n",
		__func__,
		shutdown_cond, enable_lbat_shutdown,
		now_is_kpoc, now_current, now_is_charging,
		shutdown_cond_flag, vbat);

	if (shutdown_cond_flag == 1)
		return 0;

	if (shutdown_cond_flag == 2 && shutdown_cond != LOW_BAT_VOLT)
		return 0;

	if (shutdown_cond_flag == 3 && shutdown_cond != DLPT_SHUTDOWN)
		return 0;

	switch (shutdown_cond) {
	case OVERHEAT:
		mutex_lock(&sdc.lock);
		sdc.shutdown_status.is_overheat = true;
		mutex_unlock(&sdc.lock);
		bm_err("[%s]OVERHEAT shutdown!\n", __func__);
		kernel_power_off();
		break;
	case SOC_ZERO_PERCENT:
		if (sdc.shutdown_status.is_soc_zero_percent != true) {
			mutex_lock(&sdc.lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sds->is_soc_zero_percent =
						true;
					get_monotonic_boottime(
						&sdc.pre_time[
						SOC_ZERO_PERCENT]);
					bm_err("[%s]soc_zero_percent shutdown\n",
						__func__);
					notify_fg_shutdown();
				}
			}
			mutex_unlock(&sdc.lock);
		}
		break;
	case UISOC_ONE_PERCENT:
		if (sdc.shutdown_status.is_uisoc_one_percent != true) {
			mutex_lock(&sdc.lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sds->is_uisoc_one_percent =
						true;
					get_monotonic_boottime(
					&sdc.pre_time[UISOC_ONE_PERCENT]);
					bm_err("[%s]uisoc 1 percent shutdown\n",
						__func__);
					notify_fg_shutdown();
				}
			}
			mutex_unlock(&sdc.lock);
		}
		break;
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	case LOW_BAT_VOLT:
		if (sdc.shutdown_status.is_under_shutdown_voltage != true) {
			int i;

			mutex_lock(&sdc.lock);
			if (now_is_kpoc != 1) {
				sds->is_under_shutdown_voltage = true;
				for (i = 0; i < AVGVBAT_ARRAY_SIZE; i++)
					sdc.batdata[i] =
						VBAT2_DET_VOLTAGE1 / 10;
				sdc.batidx = 0;
			}
			bm_err("LOW_BAT_VOLT:vbat %d %d",
				vbat, VBAT2_DET_VOLTAGE1 / 10);
			mutex_unlock(&sdc.lock);
		}
		break;
#endif
	case DLPT_SHUTDOWN:
		if (sdc.shutdown_status.is_dlpt_shutdown != true) {
			mutex_lock(&sdc.lock);
			sdc.shutdown_status.is_dlpt_shutdown = true;
			get_monotonic_boottime(&sdc.pre_time[DLPT_SHUTDOWN]);
			notify_fg_dlpt_sd();
			mutex_unlock(&sdc.lock);
		}
		break;

	default:
		break;
	}

	wake_up_power_misc(&sdc);

	return 0;
}

int next_waketime(int polling)
{
	if (polling <= 0)
		return 0;
	else
		return 10;
}

static int shutdown_event_handler(struct shutdown_controller *sdd)
{
	struct timespec now, duraction;
	int polling = 0;
	static int ui_zero_time_flag;
	static int down_to_low_bat;
	int now_current = 0;
	int current_ui_soc = battery_get_uisoc();
	int current_soc = battery_get_soc();
	int vbat = battery_get_bat_voltage();
	int tmp = 25;

	now.tv_sec = 0;
	now.tv_nsec = 0;
	duraction.tv_sec = 0;
	duraction.tv_nsec = 0;

	get_monotonic_boottime(&now);

	bm_err("%s:soc_zero:%d,ui 1percent:%d,dlpt_shut:%d,under_shutdown_volt:%d\n",
		__func__,
		sdd->shutdown_status.is_soc_zero_percent,
		sdd->shutdown_status.is_uisoc_one_percent,
		sdd->shutdown_status.is_dlpt_shutdown,
		sdd->shutdown_status.is_under_shutdown_voltage);


	if (sdd->shutdown_status.is_soc_zero_percent) {
		if (current_ui_soc == 0) {
			duraction = timespec_sub(
				now, sdd->pre_time[SOC_ZERO_PERCENT]);
			polling++;
			if (duraction.tv_sec >= SHUTDOWN_TIME) {
				bm_err("soc zero shutdown\n");
				kernel_power_off();
				return next_waketime(polling);

			}
		} else if (current_soc > 0) {
			sdd->shutdown_status.is_soc_zero_percent = false;
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}
	}

	if (sdd->shutdown_status.is_uisoc_one_percent) {
		now_current = battery_get_bat_current();

		if (current_ui_soc == 0) {
			duraction =
				timespec_sub(
				now, sdd->pre_time[UISOC_ONE_PERCENT]);
			polling++;
			if (duraction.tv_sec >= SHUTDOWN_TIME) {
				bm_err("uisoc one percent shutdown\n");
				kernel_power_off();
				return next_waketime(polling);
			}
		} else if (now_current > 0 && current_soc > 0) {
			polling = 0;
			sdd->shutdown_status.is_uisoc_one_percent = 0;
			bm_err("disable uisoc_one_percent shutdown cur:%d soc:%d\n",
				now_current, current_soc);
			return next_waketime(polling);
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}
	}

	if (sdd->shutdown_status.is_dlpt_shutdown) {
		duraction = timespec_sub(now, sdd->pre_time[DLPT_SHUTDOWN]);
		polling++;
		if (duraction.tv_sec >= SHUTDOWN_TIME) {
			bm_err("dlpt shutdown\n");
			kernel_power_off();
			return next_waketime(polling);
		}
	}

	if (sdd->shutdown_status.is_under_shutdown_voltage) {

		int vbatcnt = 0, i;

		sdd->batdata[sdd->batidx] = vbat;

		for (i = 0; i < AVGVBAT_ARRAY_SIZE; i++)
			vbatcnt += sdd->batdata[i];
		sdd->avgvbat = vbatcnt / AVGVBAT_ARRAY_SIZE;

		tmp = battery_get_bat_temperature();

		bm_err("lbatcheck vbat:%d avgvbat:%d %d,%d tmp:%d,bound:%d,th:%d %d,en:%d\n",
			vbat,
			sdd->avgvbat,
			g_vbat_lt,
			g_vbat_lt_lv1,
			tmp,
			BAT_VOLTAGE_LOW_BOUND,
			LOW_TEMP_THRESHOLD,
			LOW_TMP_BAT_VOLTAGE_LOW_BOUND,
			LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN);

		if (sdd->avgvbat < BAT_VOLTAGE_LOW_BOUND) {
			/* avg vbat less than 3.4v */
			sdd->lowbatteryshutdown = true;
			polling++;

			if (down_to_low_bat == 0) {
				if (IS_ENABLED(
					LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN)) {
					if (tmp >= LOW_TEMP_THRESHOLD) {
						down_to_low_bat = 1;
						bm_err("normal tmp, battery voltage is low shutdown\n");
						notify_fg_shutdown();
					} else if (sdd->avgvbat <=
						LOW_TMP_BAT_VOLTAGE_LOW_BOUND) {
						down_to_low_bat = 1;
						bm_err("cold tmp, battery voltage is low shutdown\n");
						notify_fg_shutdown();
					} else
						bm_err("low temp disable low battery sd\n");
				} else {
					down_to_low_bat = 1;
					bm_err("[%s]avg vbat is low to shutdown\n",
						__func__);
					notify_fg_shutdown();
				}
			}

			if ((current_ui_soc == 0) && (ui_zero_time_flag == 0)) {
				get_monotonic_boottime(
					&sdc.pre_time[LOW_BAT_VOLT]);
				ui_zero_time_flag = 1;
			}

			if (current_ui_soc == 0) {
				duraction = timespec_sub(
					now, sdd->pre_time[LOW_BAT_VOLT]);
				if (duraction.tv_sec >= SHUTDOWN_TIME) {
					bm_err("low bat shutdown, over %d second\n",
						SHUTDOWN_TIME);
					kernel_power_off();
					return next_waketime(polling);
				}
			}
		} else {
			/* greater than 3.4v, clear status */
			down_to_low_bat = 0;
			ui_zero_time_flag = 0;
			sdd->pre_time[LOW_BAT_VOLT].tv_sec = 0;
			sdd->lowbatteryshutdown = false;
			polling++;
		}

		polling++;
			bm_err("[%s][UT] V %d ui_soc %d dur %d [%d:%d:%d:%d] batdata[%d] %d\n",
				__func__,
			sdd->avgvbat, current_ui_soc,
			(int)duraction.tv_sec,
			down_to_low_bat, ui_zero_time_flag,
			(int)sdd->pre_time[LOW_BAT_VOLT].tv_sec,
			sdd->lowbatteryshutdown,
			sdd->batidx, sdd->batdata[sdd->batidx]);

		sdd->batidx++;
		if (sdd->batidx >= AVGVBAT_ARRAY_SIZE)
			sdd->batidx = 0;
	}

	bm_err(
		"%s %d avgvbat:%d sec:%d lowst:%d\n",
		__func__,
		polling, sdd->avgvbat,
		(int)duraction.tv_sec, sdd->lowbatteryshutdown);

	return next_waketime(polling);

}

static enum alarmtimer_restart power_misc_kthread_fgtimer_func(
	struct alarm *alarm, ktime_t now)
{
	struct shutdown_controller *info =
		container_of(
			alarm, struct shutdown_controller, kthread_fgtimer);

	wake_up_power_misc(info);
	return ALARMTIMER_NORESTART;
}

void power_misc_handler(void *arg)
{
	struct shutdown_controller *sdd = arg;
	struct timespec time, time_now, end_time;
	ktime_t ktime;
	int secs = 0;

	secs = shutdown_event_handler(sdd);
	if (secs != 0 && is_fg_disabled() == false) {
		get_monotonic_boottime(&time_now);
		time.tv_sec = secs;
		time.tv_nsec = 0;
		end_time = timespec_add(time_now, time);
		ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);

		alarm_start(&sdd->kthread_fgtimer, ktime);
		bm_err("%s:set new alarm timer:%ds\n",
			__func__, secs);
	}

}

static int power_misc_routine_thread(void *arg)
{
	struct shutdown_controller *sdd = arg;

	while (1) {
		wait_event(sdd->wait_que, (sdd->timeout == true)
			|| (sdd->overheat == true));
		if (sdd->timeout == true) {
			sdd->timeout = false;
			power_misc_handler(arg);
		}
		if (sdd->overheat == true) {
			sdd->overheat = false;
			bm_err("%s battery overheat~ power off\n",
				__func__);
			kernel_power_off();
			fix_coverity = 1;
			return 1;
		}
		if (fix_coverity == 1)
			break;
	}

	return 0;
}

int mtk_power_misc_psy_event(
	struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;
	union power_supply_propval val;
	int ret;
	int tmp = 0;

	if (strcmp(psy->desc->name, "battery") == 0) {
		ret = psy->desc->get_property(
			psy, POWER_SUPPLY_PROP_TEMP, &val);
		if (!ret) {
			tmp = val.intval / 10;
			if (tmp >= BATTERY_SHUTDOWN_TEMPERATURE) {
				bm_err(
					"battery temperature >= %d,shutdown",
					tmp);

				wake_up_overheat(&sdc);
			}
		}
	}

	return NOTIFY_DONE;
}

void mtk_power_misc_init(struct platform_device *pdev)
{
	mutex_init(&sdc.lock);
	alarm_init(&sdc.kthread_fgtimer, ALARM_BOOTTIME,
		power_misc_kthread_fgtimer_func);
	init_waitqueue_head(&sdc.wait_que);

	kthread_run(power_misc_routine_thread, &sdc, "power_misc_thread");

	sdc.psy_nb.notifier_call = mtk_power_misc_psy_event;
	power_supply_reg_notifier(&sdc.psy_nb);
}

