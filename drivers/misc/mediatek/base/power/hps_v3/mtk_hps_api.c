// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>

#include "mtk_hps_internal.h"
#include "mtk_hps.h"

#ifdef CONFIG_MTK_ICCS_SUPPORT
#include <include/pmic_regulator.h>
#include <linux/delay.h>
#include <mach/mtk_cpufreq_api.h>
#include <mach/mtk_freqhopping.h>
#include <mt-plat/mtk_secure_api.h>
#include <mtk_etc.h>
#include <mtk_iccs.h>
#include <mtk_pmic_regulator.h>

static unsigned char iccs_target_power_state_bitmask;
#endif
/*
 * static
 */
#define STATIC
/* #define STATIC static */

/*
 * hps set PPM request
 */

int hps_set_PPM_request(unsigned int little_min, unsigned int little_max,
	unsigned int big_min, unsigned int big_max)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;
	if ((little_min > num_possible_little_cpus()) ||
	(little_max > num_possible_little_cpus()))
		return -1;
	if ((big_min > num_possible_big_cpus()) ||
	(big_max > num_possible_big_cpus()))
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
int hps_set_cpu_num_base(enum hps_base_type_e type, unsigned int little_cpu,
	unsigned int big_cpu)
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
		if ((little_cpu > num_possible_little_cpus()) ||
		(little_cpu < 1))
			return -1;
	}
	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp) &&
	(big_cpu > num_possible_big_cpus()))
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
		     min(hps_ctxt.big_num_limit_thermal,
		     hps_ctxt.big_num_limit_low_battery))
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
				min(
				hps_ctxt.little_num_limit_ultra_power_saving,
				hps_ctxt.little_num_limit_power_serv))
			    && (num_online_cpus() < (little_cpu + big_cpu)))
				hps_task_wakeup_nolock();
		}
	} else {
		num_online = num_online_little_cpus();
		if ((num_online < little_cpu) &&
		    (num_online <
		     min(hps_ctxt.little_num_limit_thermal,
		     hps_ctxt.little_num_limit_low_battery))
		    && (num_online <
			min(hps_ctxt.little_num_limit_ultra_power_saving,
			    hps_ctxt.little_num_limit_power_serv))) {
			hps_task_wakeup_nolock();
		}
	}

	mutex_unlock(&hps_ctxt.lock);

	return 0;
}

int hps_get_cpu_num_base(enum hps_base_type_e type,
			 unsigned int *little_cpu_ptr,
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
int hps_set_cpu_num_limit(enum hps_limit_type_e type, unsigned int little_cpu,
	unsigned int big_cpu)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	if ((type < 0) || (type >= LIMIT_COUNT))
		return -1;
	if (hps_ctxt.is_amp) {
		if (little_cpu > num_possible_little_cpus())
			return -1;
	} else {
		if ((little_cpu > num_possible_little_cpus()) ||
		(little_cpu < 1))
			return -1;
	}

	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp) &&
	(big_cpu > num_possible_big_cpus()))
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

int hps_get_cpu_num_limit(enum hps_limit_type_e type,
			  unsigned int *little_cpu_ptr,
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
int hps_get_num_possible_cpus(unsigned int *little_cpu_ptr,
	unsigned int *big_cpu_ptr)
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
int hps_get_num_online_cpus(unsigned int *little_cpu_ptr,
	unsigned int *big_cpu_ptr)
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

#ifdef CONFIG_MTK_ICCS_SUPPORT
int hps_get_iccs_pwr_status(int cluster)
{
	if (hps_ctxt.init_state != INIT_STATE_DONE)
		return -1;

	return hps_sys.cluster_info[cluster].iccs_state;
}

void iccs_cluster_on_off(int cluster, int state)
{
	if (state == 1) {
		mt_cpufreq_set_iccs_frequency_by_cluster(1, cluster,
		iccs_get_shared_cluster_freq());
		switch (cluster) {
		case 0:
			/*1. Turn on ARM PLL*/
			armpll_control(1, 1);
			/*2. Non-pause FQHP function*/
			mt_pause_armpll(FH_PLL0, 0);
			/*3. Switch to HW mode*/
			mp_enter_suspend(0, 1);

			mt_secure_call(MTK_SIP_POWER_UP_CLUSTER, 0, 0, 0);
			break;
		case 1:
			if (hps_ctxt.init_state == INIT_STATE_DONE) {
#if CPU_BUCK_CTRL
				/*1. Power ON VSram*/
				buck_enable(VSRAM_DVFS2, 1);
				/*2. Set the stttle time to 3000us*/
				dsb(sy);
				mdelay(3);
				dsb(sy);
				/*3. Power ON Vproc2*/
				hps_power_on_vproc2();
				dsb(sy);
				mdelay(1);
				dsb(sy);
#endif
			}
			/*4. Turn on ARM PLL*/
			armpll_control(2, 1);
			/*5. Non-pause FQHP function*/
			mt_pause_armpll(FH_PLL1, 0);
			/*6. Switch to HW mode*/
			mp_enter_suspend(1, 1);

			mt_secure_call(MTK_SIP_POWER_UP_CLUSTER, 1, 0, 0);
			break;
		case 2:
			/*1. Turn on ARM PLL*/
			armpll_control(3, 1);

			/*2. Non-pause FQHP function*/
			mt_pause_armpll(FH_PLL2, 0);
			/*3. Switch to HW mode*/
			mp_enter_suspend(2, 1);

			mt_secure_call(MTK_SIP_POWER_UP_CLUSTER, 2, 0, 0);

#ifdef CONFIG_MACH_MT6799
			mtk_etc_init();
#endif
			break;
		}
		iccs_set_cache_shared_state(cluster, 1);
	} else if (state == 0) {
		mt_cpufreq_set_iccs_frequency_by_cluster(0, cluster, 0);
		iccs_set_cache_shared_state(cluster, 0);

		mt_secure_call(MTK_SIP_POWER_DOWN_CLUSTER, cluster, 0, 1);

		switch (cluster) {
		case 0:
			/*1. Switch to SW mode*/
			mp_enter_suspend(0, 0);

			/*2. Pause FQHP function*/
			mt_pause_armpll(FH_PLL0, 0x01);

			/*3. Turn off ARM PLL*/
			armpll_control(1, 0);
			break;
		case 1:
			/*1. Switch to SW mode*/
			mp_enter_suspend(1, 0);
			/*2. Pause FQHP function*/
			mt_pause_armpll(FH_PLL1, 0x01);
			/*3. Turn off ARM PLL*/
			armpll_control(2, 0);
			if (hps_ctxt.init_state == INIT_STATE_DONE) {
#if CPU_BUCK_CTRL
				/*4. Power off Vproc2*/
				hps_power_off_vproc2();

				/*5. Turn off VSram*/
				buck_enable(VSRAM_DVFS2, 0);
#endif
			}
			break;
		case 2:
			/*1. Switch to SW mode*/
			mp_enter_suspend(2, 0);
			/*2. Pause FQHP function*/
			mt_pause_armpll(FH_PLL2, 0x01);

			/*3. Turn off ARM PLL*/
			armpll_control(3, 0);
			break;
		}
	}
}

unsigned char iccs_get_target_power_state_bitmask(void)
{
	return iccs_target_power_state_bitmask;
}

void iccs_set_target_power_state_bitmask(unsigned char value)
{
	iccs_target_power_state_bitmask = value;
}

void iccs_enter_low_power_state(void)
{
	int iccs_cluster;
	unsigned int iccs_cache_shared_state;

	iccs_cache_shared_state = iccs_get_curr_cache_shared_state();
	/*
	 * pr_notice("[%s] iccs_cache_shared_state: 0x%x\n",
	 * __func__, iccs_cache_shared_state);
	 */

	while (iccs_cache_shared_state) {
		iccs_cluster = __builtin_ctz(iccs_cache_shared_state);
		iccs_cluster_on_off(iccs_cluster, 0);
		iccs_cache_shared_state &= ~(1 << iccs_cluster);
	}

	iccs_set_target_power_state_bitmask(0);
}
#endif
