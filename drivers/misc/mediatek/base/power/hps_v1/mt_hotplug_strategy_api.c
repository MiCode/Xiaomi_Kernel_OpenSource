// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/**
 * @file    mt_hotplug_strategy_api.c
 * @brief   hotplug strategy(hps) - api
 */

#include <linux/kernel.h>
#include <linux/module.h>	/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/sched.h>	/* sched_get_* */

#include "mt_hotplug_strategy_internal.h"
#include "mt_hotplug_strategy.h"

/*
 * hps cpu num base
 */
int hps_set_cpu_num_base(enum hps_base_type_e type,
		unsigned int little_cpu, unsigned int big_cpu)
{
	unsigned int nll, nlb;
	unsigned int lo, bo;

	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((type < 0) || (type >= BASE_COUNT))
		return -1;

	if ((little_cpu > num_possible_little_cpus()) || (little_cpu < 1))
		return -1;

	if (hps_ctxt.is_hmp && (big_cpu > num_possible_big_cpus()))
		return -1;

	/* XXX: check mutex lock or not? use hps_ctxt.lock! */
	mutex_lock(&hps_ctxt.lock);

	switch (type) {
	case BASE_PERF_SERV:
		hps_ctxt.little_num_base_perf_serv = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_base_perf_serv = big_cpu;
		break;
	case BASE_CUSTOM1:
		hps_ctxt.little_num_base_custom1 = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_base_custom1 = big_cpu;
		break;
	case BASE_CUSTOM2:
		hps_ctxt.little_num_base_custom2 = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_base_custom2 = big_cpu;
		break;
	default:
		break;
	}

	if (hps_ctxt.is_hmp) {
		nll = num_limit_little_cpus();
		nlb = num_limit_big_cpus();
		lo = num_online_little_cpus();
		bo = num_online_big_cpus();

		if ((bo < big_cpu && bo < nlb) || (lo < little_cpu && lo < nll))
			hps_task_wakeup_nolock();
	} else {
		nll = num_limit_little_cpus();
		lo = num_online_little_cpus();

		if (lo < little_cpu && lo < nll)
			hps_task_wakeup_nolock();
	}

	mutex_unlock(&hps_ctxt.lock);

	return 0;
}

int hps_get_cpu_num_base(enum hps_base_type_e type,
		unsigned int *little_cpu_ptr, unsigned int *big_cpu_ptr)
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
	case BASE_CUSTOM1:
		*little_cpu_ptr = hps_ctxt.little_num_base_custom1;
		*big_cpu_ptr = hps_ctxt.big_num_base_custom1;
		break;
	case BASE_CUSTOM2:
		*little_cpu_ptr = hps_ctxt.little_num_base_custom2;
		*big_cpu_ptr = hps_ctxt.big_num_base_custom2;
		break;
	default:
		break;
	}

	return 0;
}

/*
 * hps cpu num limit
 */
int hps_set_cpu_num_limit(enum hps_limit_type_e type,
		unsigned int little_cpu, unsigned int big_cpu)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((type < 0) || (type >= LIMIT_COUNT))
		return -1;

	if ((little_cpu > num_possible_little_cpus()) || (little_cpu < 1))
		return -1;

	if (hps_ctxt.is_hmp && (big_cpu > num_possible_big_cpus()))
		return -1;

	mutex_lock(&hps_ctxt.lock);

	switch (type) {
	case LIMIT_THERMAL:
		hps_ctxt.little_num_limit_thermal = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_limit_thermal = big_cpu;
		break;
	case LIMIT_LOW_BATTERY:
		hps_ctxt.little_num_limit_low_battery = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_limit_low_battery = big_cpu;
		break;
	case LIMIT_ULTRA_POWER_SAVING:
		hps_ctxt.little_num_limit_ultra_power_saving = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_limit_ultra_power_saving = big_cpu;
		break;
	case LIMIT_POWER_SERV:
		hps_ctxt.little_num_limit_power_serv = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_limit_power_serv = big_cpu;
		break;
	case LIMIT_CUSTOM1:
		hps_ctxt.little_num_limit_custom1 = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_limit_custom1 = big_cpu;
		break;
	case LIMIT_CUSTOM2:
		hps_ctxt.little_num_limit_custom2 = little_cpu;
		if (hps_ctxt.is_hmp)
			hps_ctxt.big_num_limit_custom2 = big_cpu;
		break;
	default:
		break;
	}

	if (hps_ctxt.is_hmp) {
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

int hps_get_cpu_num_limit(enum hps_limit_type_e type,
		unsigned int *little_cpu_ptr, unsigned int *big_cpu_ptr)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((little_cpu_ptr == NULL) || (big_cpu_ptr == NULL))
		return -1;

	if ((type < 0) || (type >= LIMIT_COUNT))
		return -1;

	switch (type) {
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
	case LIMIT_CUSTOM1:
		*little_cpu_ptr = hps_ctxt.little_num_limit_custom1;
		*big_cpu_ptr = hps_ctxt.big_num_limit_custom1;
		break;
	case LIMIT_CUSTOM2:
		*little_cpu_ptr = hps_ctxt.little_num_limit_custom2;
		*big_cpu_ptr = hps_ctxt.big_num_limit_custom2;
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
int hps_get_num_possible_cpus(
		unsigned int *little_cpu_ptr, unsigned int *big_cpu_ptr)
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
int hps_get_num_online_cpus(
		unsigned int *little_cpu_ptr, unsigned int *big_cpu_ptr)
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

	mutex_lock(&hps_ctxt.lock);

	if (!hps_ctxt.enabled && enabled)
		hps_ctxt_reset_stas_nolock();
	hps_ctxt.enabled = enabled;

	mutex_unlock(&hps_ctxt.lock);

	return 0;
}
