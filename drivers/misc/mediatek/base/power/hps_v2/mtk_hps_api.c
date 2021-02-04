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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>

#include "mtk_hps_internal.h"
#include "mtk_hps.h"

/*
 * static
 */
#define STATIC
/* #define STATIC static */

/*
 * hps set PPM request
 */

int hps_set_PPM_request(unsigned int little_min, unsigned int little_max, unsigned int big_min,
			unsigned int big_max)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;
	if ((little_min > num_possible_little_cpus()) || (little_max > num_possible_little_cpus()))
		return -1;
	if ((big_min > num_possible_big_cpus()) || (big_max > num_possible_big_cpus()))
		return -1;

	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.little_num_base_perf_serv = little_min;
	hps_ctxt.little_num_limit_power_serv = little_max;
	hps_ctxt.big_num_base_perf_serv = big_min;
	hps_ctxt.big_num_limit_power_serv = big_max;

	/* hps_task_wakeup_nolock(); */
	mutex_unlock(&hps_ctxt.lock);
	return 0;
}

/*
 * hps cpu num base
 */
int hps_set_cpu_num_base(enum hps_base_type_e type, unsigned int little_cpu, unsigned int big_cpu)
{
	unsigned int num_online;

	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((type < 0) || (type >= BASE_COUNT))
		return -1;
	if (hps_ctxt.is_amp) {
		if (little_cpu > num_possible_little_cpus())
			return -1;
	} else {
		if ((little_cpu > num_possible_little_cpus()) || (little_cpu < 1))
			return -1;
	}
	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp) && (big_cpu > num_possible_big_cpus()))
		return -1;

	/* XXX: check mutex lock or not? use hps_ctxt.lock! */
	mutex_lock(&hps_ctxt.lock);

	switch (type) {
	case BASE_PERF_SERV:
		hps_ctxt.little_num_base_perf_serv = little_cpu;
		if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
			hps_ctxt.big_num_base_perf_serv = big_cpu;
		break;
	case BASE_PPM_SERV:
		hps_ctxt.little_num_base_perf_serv = little_cpu;
		hps_ctxt.big_num_base_perf_serv = big_cpu;
		break;
	default:
		break;
	}

	if (hps_ctxt.is_hmp || hps_ctxt.is_amp) {
		num_online = num_online_big_cpus();
		if ((num_online < big_cpu) &&
		    (num_online <
		     min(hps_ctxt.big_num_limit_thermal, hps_ctxt.big_num_limit_low_battery))
		    && (num_online <
			min(hps_ctxt.big_num_limit_ultra_power_saving,
			    hps_ctxt.big_num_limit_power_serv))) {
			hps_task_wakeup_nolock();
		} else {
			num_online = num_online_little_cpus();
			if ((num_online < little_cpu) &&
			    (num_online <
			     min(hps_ctxt.little_num_limit_thermal,
				 hps_ctxt.little_num_limit_low_battery))
			    && (num_online <
				min(hps_ctxt.little_num_limit_ultra_power_saving,
				    hps_ctxt.little_num_limit_power_serv))
			    && (num_online_cpus() < (little_cpu + big_cpu)))
				hps_task_wakeup_nolock();
		}
	} else {
		num_online = num_online_little_cpus();
		if ((num_online < little_cpu) &&
		    (num_online <
		     min(hps_ctxt.little_num_limit_thermal, hps_ctxt.little_num_limit_low_battery))
		    && (num_online <
			min(hps_ctxt.little_num_limit_ultra_power_saving,
			    hps_ctxt.little_num_limit_power_serv))) {
			hps_task_wakeup_nolock();
		}
	}

	mutex_unlock(&hps_ctxt.lock);

	return 0;
}

int hps_get_cpu_num_base(enum hps_base_type_e type, unsigned int *little_cpu_ptr,
			 unsigned int *big_cpu_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((little_cpu_ptr == NULL) || (big_cpu_ptr == NULL))
		return -1;

	if ((type < 0) || (type >= BASE_COUNT))
		return -1;

	switch (type) {
	case BASE_PERF_SERV:
		*little_cpu_ptr = hps_ctxt.little_num_base_perf_serv;
		*big_cpu_ptr = hps_ctxt.big_num_base_perf_serv;
		break;
	case BASE_PPM_SERV:
		*little_cpu_ptr = hps_ctxt.little_num_base_perf_serv;
		*big_cpu_ptr = hps_ctxt.big_num_base_perf_serv;
		break;
	default:
		break;
	}

	return 0;
}

/*
 * hps cpu num limit
 */
int hps_set_cpu_num_limit(enum hps_limit_type_e type, unsigned int little_cpu, unsigned int big_cpu)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((type < 0) || (type >= LIMIT_COUNT))
		return -1;
	if (hps_ctxt.is_amp) {
		if (little_cpu > num_possible_little_cpus())
			return -1;
	} else {
		if ((little_cpu > num_possible_little_cpus()) || (little_cpu < 1))
			return -1;
	}

	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp) && (big_cpu > num_possible_big_cpus()))
		return -1;

	mutex_lock(&hps_ctxt.lock);

	switch (type) {
	case LIMIT_PPM_SERV:
		hps_ctxt.little_num_limit_power_serv = little_cpu;
		if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
			hps_ctxt.big_num_limit_power_serv = big_cpu;
		break;
	case LIMIT_THERMAL:
		hps_ctxt.little_num_limit_thermal = little_cpu;
		if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
			hps_ctxt.big_num_limit_thermal = big_cpu;
		break;
	case LIMIT_LOW_BATTERY:
		hps_ctxt.little_num_limit_low_battery = little_cpu;
		if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
			hps_ctxt.big_num_limit_low_battery = big_cpu;
		break;
	case LIMIT_ULTRA_POWER_SAVING:
		hps_ctxt.little_num_limit_ultra_power_saving = little_cpu;
		if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
			hps_ctxt.big_num_limit_ultra_power_saving = big_cpu;
		break;
	case LIMIT_POWER_SERV:
		hps_ctxt.little_num_limit_power_serv = little_cpu;
		if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
			hps_ctxt.big_num_limit_power_serv = big_cpu;
		break;
	default:
		break;
	}

	if (hps_ctxt.is_hmp || hps_ctxt.is_amp) {
		if (num_online_big_cpus() > big_cpu)
			hps_task_wakeup_nolock();
		else if (num_online_little_cpus() > little_cpu)
			hps_task_wakeup_nolock();
	} else {
		if (num_online_little_cpus() > little_cpu)
			hps_task_wakeup_nolock();
	}

	mutex_unlock(&hps_ctxt.lock);

	return 0;
}

int hps_get_cpu_num_limit(enum hps_limit_type_e type, unsigned int *little_cpu_ptr,
			  unsigned int *big_cpu_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((little_cpu_ptr == NULL) || (big_cpu_ptr == NULL))
		return -1;

	if ((type < 0) || (type >= LIMIT_COUNT))
		return -1;

	switch (type) {
	case LIMIT_PPM_SERV:
		*little_cpu_ptr = hps_ctxt.little_num_limit_thermal;
		*big_cpu_ptr = hps_ctxt.big_num_limit_thermal;
		break;
	case LIMIT_THERMAL:
		*little_cpu_ptr = hps_ctxt.little_num_limit_thermal;
		*big_cpu_ptr = hps_ctxt.big_num_limit_thermal;
		break;
	case LIMIT_LOW_BATTERY:
		*little_cpu_ptr = hps_ctxt.little_num_limit_low_battery;
		*big_cpu_ptr = hps_ctxt.big_num_limit_low_battery;
		break;
	case LIMIT_ULTRA_POWER_SAVING:
		*little_cpu_ptr = hps_ctxt.little_num_limit_ultra_power_saving;
		*big_cpu_ptr = hps_ctxt.big_num_limit_ultra_power_saving;
		break;
	case LIMIT_POWER_SERV:
		*little_cpu_ptr = hps_ctxt.little_num_limit_power_serv;
		*big_cpu_ptr = hps_ctxt.big_num_limit_power_serv;
		break;
	default:
		break;
	}

	return 0;
}

/*
 * hps tlp
 */
int hps_get_tlp(unsigned int *tlp_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if (tlp_ptr == NULL)
		return -1;

	*tlp_ptr = hps_ctxt.tlp_avg;

	return 0;
}

/*
 * hps num_possible_cpus
 */
int hps_get_num_possible_cpus(unsigned int *little_cpu_ptr, unsigned int *big_cpu_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((little_cpu_ptr == NULL) || (big_cpu_ptr == NULL))
		return -1;

	*little_cpu_ptr = num_possible_little_cpus();
	*big_cpu_ptr = num_possible_big_cpus();

	return 0;
}

/*
 * hps num_online_cpus
 */
int hps_get_num_online_cpus(unsigned int *little_cpu_ptr, unsigned int *big_cpu_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((little_cpu_ptr == NULL) || (big_cpu_ptr == NULL))
		return -1;

	*little_cpu_ptr = num_online_little_cpus();
	*big_cpu_ptr = num_online_big_cpus();

	return 0;
}

/*
 * hps cpu num base
 */
int hps_get_enabled(unsigned int *enabled_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if (enabled_ptr == NULL)
		return -1;

	*enabled_ptr = hps_ctxt.enabled;

	return 0;
}

int hps_set_enabled(unsigned int enabled)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if (enabled > 1)
		return -1;

	/* XXX: check mutex lock or not? use hps_ctxt.lock! */
	mutex_lock(&hps_ctxt.lock);

	if (!hps_ctxt.enabled && enabled)
		hps_ctxt_reset_stas_nolock();
	hps_ctxt.enabled = enabled;

	mutex_unlock(&hps_ctxt.lock);

	return 0;
}

/*
 * hps get/set power mode
 */
int hps_get_powmode_status(unsigned int *pwrmode_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if (pwrmode_ptr == NULL)
		return -1;

	*pwrmode_ptr = hps_ctxt.power_mode;

	return 0;
}

int hps_set_powmode_status(unsigned int pwrmode)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	/* XXX: check mutex lock or not? use hps_ctxt.lock! */
	mutex_lock(&hps_ctxt.lock);
	hps_ctxt_reset_stas_nolock();
	hps_ctxt.power_mode = pwrmode;

	mutex_unlock(&hps_ctxt.lock);

	return 0;
}

/*
 * hps get/set PPM_power mode
 */
int hps_get_ppm_powmode_status(unsigned int *pwrmode_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if (pwrmode_ptr == NULL)
		return -1;

	*pwrmode_ptr = hps_ctxt.ppm_power_mode;

	return 0;
}

int hps_set_ppm_powmode_status(unsigned int pwrmode)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	/* XXX: check mutex lock or not? use hps_ctxt.lock! */
	mutex_lock(&hps_ctxt.lock);
	hps_ctxt_reset_stas_nolock();
	hps_ctxt.ppm_power_mode = pwrmode;

	mutex_unlock(&hps_ctxt.lock);

	return 0;
}
