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
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <asm-generic/bug.h>

#include "mtk_hps_internal.h"
#include <trace/events/mtk_events.h>

#ifdef CONFIG_MTK_ICCS_SUPPORT
#include <mtk_iccs.h>
#endif

/*
 * static
 */
#define STATIC
/* #define STATIC static */

/*
 * New hotpug strategy
 */
void hps_set_break_en(int hps_break_en)
{
	mutex_lock(&hps_ctxt.break_lock);
	atomic_set(&hps_ctxt.is_break, hps_break_en);
	mutex_unlock(&hps_ctxt.break_lock);
}

int hps_get_break_en(void)
{
	return atomic_read(&hps_ctxt.is_break);
}

int hps_current_core(void)
{
	int load_cores, tlp_cores, online_cores, i;

	load_cores = tlp_cores = online_cores = i = 0;
	for (i = hps_sys.cluster_num - 1; i >= 0; i--)
		online_cores += hps_sys.cluster_info[i].online_core_num;
	if (hps_ctxt.cur_loads > hps_ctxt.rush_boost_threshold * online_cores)
		tlp_cores = hps_ctxt.cur_tlp / 100 +
				(hps_ctxt.cur_tlp % 100 ? 1 : 0);
	else
		tlp_cores = 0;
	load_cores = hps_ctxt.cur_loads / hps_ctxt.up_threshold +
	    (hps_ctxt.cur_loads % hps_ctxt.up_threshold ? 1 : 0);
	return max(tlp_cores, load_cores);
}

#ifdef CONFIG_HPS
static int hps_algo_big_task_det(void)
{
	int i, j, ret;
	unsigned int idle_det_time;
	unsigned int window_length_ms = 0;
	hps_idle_ratio_t ratio;

	ret = 0;
	mtk_idle_recent_ratio_get(&window_length_ms, &ratio);
	idle_det_time = idle_get_current_time_ms() - ratio.last_end_ts +
			window_length_ms;
#if defined(__LP64__) || defined(_LP64)
	hps_ctxt.idle_ratio = (((ratio.value * window_length_ms) /
				idle_det_time) * 100) / idle_det_time;
#else
	hps_ctxt.idle_ratio = div_s64(div_s64(ratio.value * window_length_ms,
					idle_det_time) * 100, idle_det_time);
#endif

	if ((idle_det_time < window_length_ms) || (!ratio.value))
		goto BIG_TASK_DET;

	if (hps_ctxt.idle_det_enabled) {
		if (hps_ctxt.idle_ratio >= hps_ctxt.idle_threshold) {
			hps_ctxt.is_idle = 1;
			return ret;
		}
		hps_ctxt.is_idle = 0;
	}

BIG_TASK_DET:
	ret = 0;
	for (i = 1 ; i < hps_sys.cluster_num ; i++) {
		if (!hps_sys.cluster_info[i].bigTsk_value) {
			for (j = 1 ; j <= 4 ; j++) {/*Reset counter value*/
				if (j == 1)
					hps_sys.cluster_info[i].down_times[j] =
				hps_sys.cluster_info[i].down_time_val[j] =
					DEF_ROOT_CPU_DOWN_TIMES;
				else
					hps_sys.cluster_info[i].down_times[j] =
				hps_sys.cluster_info[i].down_time_val[j] =
					DEF_CPU_DOWN_TIMES;
			}
			continue;
		}

		j = hps_sys.cluster_info[i].online_core_num;

		if (hps_sys.cluster_info[i].bigTsk_value <
		hps_sys.cluster_info[i].online_core_num)
			hps_sys.cluster_info[i].down_times[j]--;
		else
			hps_sys.cluster_info[i].down_times[j] =
			hps_sys.cluster_info[i].down_time_val[j];

		if (hps_sys.cluster_info[i].down_times[j] <= 0) {
			hps_sys.cluster_info[i].target_core_num =
			hps_sys.cluster_info[i].bigTsk_value;
			hps_sys.cluster_info[i].down_times[j] =
			hps_sys.cluster_info[i].down_time_val[j];
			ret = 1;
		}

		if (hps_sys.cluster_info[i].bigTsk_value >
		hps_sys.cluster_info[i].target_core_num) {
			hps_sys.cluster_info[i].target_core_num =
			hps_sys.cluster_info[i].bigTsk_value;
			ret = 1;
		}
	}
	return ret;
}
#endif /* CONFIG_HPS */

#if 0
static int hps_algo_heavytsk_det(void)
{
	int i, hvy_tsk_remain;
	int ret = 0;
	int big_task  = 0;
	/*int big_thr, big_num;*/
	for (i = 0; i < hps_sys.cluster_num; i++) {
		if (hps_sys.cluster_info[i].hvyTsk_value) {
			big_task += hps_sys.cluster_info[i].hvyTsk_value;
			ret = 1;
		}

	}
	if (!ret)
		return 0;
#if 0
hps_warn("func ID 0x%x\n", hps_sys.action_id);
for (i = 0 ; i <= hps_sys.cluster_num - 1 ; i++)
	hps_warn("Org: Cluster[%d]==>%d\n",
	i, hps_sys.cluster_info[i].target_core_num);
#endif
	mutex_lock(&hps_ctxt.para_lock);
	hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num = 0;
	/*Process heavy task from Big cluster*/
	for (i = 0 ; i <= hps_sys.cluster_num - 1 ; i++) {
		hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num +=
		hps_sys.cluster_info[i].hvyTsk_value;
		if (i == (hps_sys.cluster_num - 1))
			break;
	}
	if (hps_sys.cluster_info[hps_sys.cluster_num - 1].loading >=
	(hps_sys.cluster_info[hps_sys.cluster_num - 1].up_threshold *
	hps_sys.cluster_info[hps_sys.cluster_num - 1].core_num)) {
		if (hps_sys.cluster_info[0].target_core_num <
		hps_sys.cluster_info[0].core_num)
			hps_sys.cluster_info[0].target_core_num++;
		else
	hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num++;
	}
	if (hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num <
	big_task)
		hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num =
		big_task;

	hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num =
	max(hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num,
		hps_sys.cluster_info[hps_sys.cluster_num - 1].base_value);

	hvy_tsk_remain =
	hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num -
		 hps_sys.cluster_info[hps_sys.cluster_num - 1].limit_value;
#if 0
hps_warn("hvy_tsk_remain %d\n", hvy_tsk_remain);
for (i = 0 ; i <= hps_sys.cluster_num - 1 ; i++)
	hps_warn("Step 00: Cluster[%d]==>%d\n",
		i, hps_sys.cluster_info[i].target_core_num);
#endif
	if (hvy_tsk_remain > 0) {
		hps_sys.cluster_info[hps_sys.cluster_num - 1].target_core_num -=
		hvy_tsk_remain;
		/*Process LL and L cluster*/
		for (i = hps_sys.cluster_num - 1 ; i >= 0 ; i--) {
			while (hvy_tsk_remain  > 0) {
				if (hps_sys.cluster_info[i].target_core_num <
				hps_sys.cluster_info[i].limit_value) {
				hps_sys.cluster_info[i].target_core_num++;
					hvy_tsk_remain--;
				} else
					break;
			}
#if 0
hps_warn("11hvy_tsk_remain %d\n", hvy_tsk_remain);
hps_warn("[%d]: target %d, base %d\n", i,
hps_sys.cluster_info[i].target_core_num,
hps_sys.cluster_info[i].base_value);
#endif
			hps_sys.cluster_info[i].target_core_num =
			max(hps_sys.cluster_info[i].target_core_num,
				hps_sys.cluster_info[i].base_value);
		}
	}
	mutex_unlock(&hps_ctxt.para_lock);
#if 0
for (i = 0 ; i <= hps_sys.cluster_num - 1 ; i++)
	hps_warn("Result: Cluster[%d]==>%d\n", i,
	hps_sys.cluster_info[i].target_core_num);
#endif
	return ret;
}
#endif
#if 1
#else
static int hps_algo_heavytsk_det(void)
{
	int i, j, ret, sys_cores, hvy_cores, target_cores_limit;
	unsigned int hvy_tmp;

	i = j = ret = sys_cores = hvy_cores = target_cores_limit = hvy_tmp = 0;
	for (i = 0; i < hps_sys.cluster_num; i++)
		if (hps_sys.cluster_info[i].hvyTsk_value)
			ret = 1;
	if (!ret)
		return 0;
	/*Calculate system cores */
	mutex_lock(&hps_ctxt.para_lock);
	target_cores_limit = hps_current_core();

	for (i = hps_sys.cluster_num - 1; i > 0; i--) {
		if (hps_sys.cluster_info[i - 1].hvyTsk_value)
			ret = 1;
		hvy_cores += hps_sys.cluster_info[i].hvyTsk_value;

		hvy_tmp = 0;
		for (j = i-1; j >= 0; j--) {
			if (hps_sys.cluster_info[j].limit_value) {
				hvy_tmp = hps_sys.cluster_info[j].hvyTsk_value;
				break;
			}
		}
		if (i == hps_sys.cluster_num - 1)
			hps_sys.cluster_info[i].target_core_num =
			    max(hps_sys.cluster_info[i].target_core_num,
				hvy_tmp) + hps_sys.cluster_info[i].hvyTsk_value;
		else
			hps_sys.cluster_info[i].target_core_num =
			    max(hps_sys.cluster_info[i].target_core_num,
				hvy_tmp);
		if (hps_sys.cluster_info[i].target_core_num >
		hps_sys.cluster_info[i].limit_value)
			hps_sys.cluster_info[i].target_core_num =
			    hps_sys.cluster_info[i].limit_value;
		sys_cores += hps_sys.cluster_info[i].target_core_num;
	}
#if 1
	hvy_cores += hps_sys.cluster_info[0].hvyTsk_value;
	sys_cores += hps_sys.cluster_info[0].target_core_num;
	if (sys_cores < hvy_cores) {
		for (i = hps_sys.cluster_num - 1; i >= 0; i--) {
			for (j = hps_sys.cluster_info[i].target_core_num;
			     j < hps_sys.cluster_info[i].limit_value; j++) {
				if (sys_cores >= hvy_cores)
					break;
				hps_sys.cluster_info[i].target_core_num++;
				hvy_cores--;
				ret = 1;
			}
		}
	}
	/*recalculate sys_core number */
	sys_cores = 0;
	for (i = 0; i < hps_sys.cluster_num; i++)
		sys_cores += hps_sys.cluster_info[i].target_core_num;
#endif
	if (sys_cores > target_cores_limit) {
		for (i = 0; i < hps_sys.cluster_num; i++) {
			for (j = hps_sys.cluster_info[i].base_value;
			     j <= hps_sys.cluster_info[i].limit_value; j++) {
				if (sys_cores <= target_cores_limit)
					break;
				if (hps_sys.cluster_info[i].target_core_num >
				0) {
				hps_sys.cluster_info[i].target_core_num--;
					sys_cores--;
				} else {
					ret = 1;
					continue;
				}
			}
		}
	} else {
		for (i = 0; i < hps_sys.cluster_num; i++) {
			for (j = hps_sys.cluster_info[i].base_value;
			     j <= hps_sys.cluster_info[i].limit_value; j++) {
				if (sys_cores >= target_cores_limit)
					break;
				if (hps_sys.cluster_info[i].target_core_num <
				    hps_sys.cluster_info[i].limit_value) {
				hps_sys.cluster_info[i].target_core_num++;
					sys_cores++;
					ret = 1;
				} else
					continue;
			}
		}
	}
	mutex_unlock(&hps_ctxt.para_lock);
	return ret;
}
#endif

#ifdef CONFIG_HPS
static int hps_algo_check_criteria(void)
{
	int ret, i;

	ret = 0;
	mutex_lock(&hps_ctxt.para_lock);
	for (i = 0; i < hps_sys.cluster_num; i++) {
		if (hps_ctxt.is_ppm_init == 0) {
			struct cpumask all_cpus;

			arch_get_cluster_cpus(&all_cpus,
			hps_sys.cluster_info[i].cluster_id);

			/* there is no input from PPM, so get default */
			hps_sys.cluster_info[i].ref_base_value = 0;
			hps_sys.cluster_info[i].ref_limit_value =
			cpumask_weight(&all_cpus);
		}

		if ((hps_sys.cluster_info[i].target_core_num >=
		     hps_sys.cluster_info[i].ref_base_value)
		    && (hps_sys.cluster_info[i].target_core_num <=
			hps_sys.cluster_info[i].ref_limit_value)) {
			continue;
		} else {
			/*
			 * hps_warn("... Cluster%d ... target = %d,
			 *	ref_base = %d, ref_limit = %d\n", i,
			 *	hps_sys.cluster_info[i].target_core_num,
			 *	hps_sys.cluster_info[i].ref_base_value,
			 *	hps_sys.cluster_info[i].ref_limit_value);
			 */
			ret = 1;
			break;
		}
	}
	mutex_unlock(&hps_ctxt.para_lock);
	if (ret)
		hps_warn("[Info]Condition break!!\n");
	return ret;
}
#endif /* CONFIG_HPS */

#ifdef CONFIG_HPS
static int hps_algo_do_cluster_action(unsigned int cluster_id)
{
	int cpu, target_cores, online_cores, cpu_id_min, cpu_id_max;

	target_cores = hps_sys.cluster_info[cluster_id].target_core_num;
	online_cores = hps_sys.cluster_info[cluster_id].online_core_num;
	cpu_id_min = hps_sys.cluster_info[cluster_id].cpu_id_min;
	cpu_id_max = hps_sys.cluster_info[cluster_id].cpu_id_max;

	if (target_cores > online_cores) {	/*Power up cpus */
		for (cpu = cpu_id_min; cpu <= cpu_id_max; ++cpu) {
			if (hps_get_break_en() != 0) {
				hps_warn
				("[CPUHP] up CPU%d: hps_get_break_en\n", cpu);
				return 1;
			}
			if (hps_algo_check_criteria() == 1) {
				hps_warn
				("[CPUHP] up CPU%d: hps_algo_check_criteria\n",
				cpu);
				return 1;
			}
			if (!cpu_online(cpu)) {
				cpu_up(cpu);
				++online_cores;
			}
			if (target_cores == online_cores)
				break;
		}

	} else {		/*Power down cpus */
		for (cpu = cpu_id_max; cpu >= cpu_id_min; --cpu) {
			if (hps_get_break_en() != 0) {
				hps_warn
				("[CPUHP] down CPU%d: hps_get_break_en\n",
				cpu);
				return 1;
			}
			if (hps_algo_check_criteria() == 1) {
				hps_warn
			("[CPUHP] down CPU%d: hps_algo_check_criteria\n",
				cpu);
				return 1;
			}
			if (cpu_online(cpu)) {
				cpu_down(cpu);
				--online_cores;
			}
			if (target_cores == online_cores)
				break;
		}
	}
	return 0;
}
#endif /* CONFIG_HPS */

unsigned int hps_get_cluster_cpus(unsigned int cluster_id)
{
	struct cpumask cls_cpus, cpus;

	arch_get_cluster_cpus(&cls_cpus, cluster_id);
	cpumask_and(&cpus, cpu_online_mask, &cls_cpus);
	return cpumask_weight(&cpus);
}

void hps_check_base_limit(struct hps_sys_struct *hps_sys)
{
	int i;

	mutex_lock(&hps_ctxt.para_lock);
	for (i = 0; i < hps_sys->cluster_num; i++) {
		if (hps_sys->cluster_info[i].target_core_num <
		hps_sys->cluster_info[i].base_value)
			hps_sys->cluster_info[i].target_core_num =
			    hps_sys->cluster_info[i].base_value;
		if (hps_sys->cluster_info[i].target_core_num >
		hps_sys->cluster_info[i].limit_value)
			hps_sys->cluster_info[i].target_core_num =
			    hps_sys->cluster_info[i].limit_value;
	}
	mutex_unlock(&hps_ctxt.para_lock);
}

int hps_cal_core_num(struct hps_sys_struct *hps_sys, int core_val,
	int base_val)
{
	/*int i, cpu, root_cluster;*/
	int i, j, cpu;

	/*initial target core nunber per cluster*/
	for (i = 0; i < hps_sys->cluster_num; i++)
		hps_sys->cluster_info[i].target_core_num = 0;

	mutex_lock(&hps_ctxt.para_lock);
	for (i = 0; i < hps_sys->cluster_num; i++) {
		for (j = 0; j < hps_sys->cluster_num; j++) {
			if (i == hps_sys->cluster_info[j].pwr_seq) {
				for (cpu = hps_sys->cluster_info[j].base_value;
				cpu < hps_sys->cluster_info[j].limit_value;
				cpu++) {
					if (core_val <= 0)
						goto out;
					else {
				hps_sys->cluster_info[j].target_core_num++;
						core_val--;
					}
				}
				break;
			}
				continue;
		}
	}
out:
	/* Add base value of per-cluster by default */
	for (i = 0; i < hps_sys->cluster_num; i++)
		hps_sys->cluster_info[i].target_core_num +=
		hps_sys->cluster_info[i].base_value;
	mutex_unlock(&hps_ctxt.para_lock);

	return 0;
}

void hps_define_root_cluster(struct hps_sys_struct *hps_sys)
{
	int i;

	mutex_lock(&hps_ctxt.para_lock);
#if 1
	hps_sys->root_cluster_id = hps_sys->ppm_root_cluster;
#else
	for (i = 0; i < hps_sys->cluster_num; i++) {
		if (hps_sys->cluster_info[i].pwr_seq == 0) {
			hps_sys->root_cluster_id = i;
			break;
		}
	}
#endif
#if 1
	/*Determine root cluster. */
	if (hps_sys->cluster_info[hps_sys->root_cluster_id].limit_value > 0) {
		mutex_unlock(&hps_ctxt.para_lock);
		return;
	}
	for (i = 0; i < hps_sys->cluster_num; i++) {
		if (hps_sys->cluster_info[i].limit_value > 0) {
			hps_sys->root_cluster_id = i;
			break;
		}
	}
#endif
	mutex_unlock(&hps_ctxt.para_lock);
}

void hps_set_funct_ctrl(void)
{
	if (!hps_ctxt.enabled)
		hps_ctxt.hps_func_control &= ~(1 << HPS_FUNC_CTRL_HPS);
	else
		hps_ctxt.hps_func_control |= (1 << HPS_FUNC_CTRL_HPS);
	if (!hps_ctxt.rush_boost_enabled)
		hps_ctxt.hps_func_control &= ~(1 << HPS_FUNC_CTRL_RUSH);
	else
		hps_ctxt.hps_func_control |= (1 << HPS_FUNC_CTRL_RUSH);
	if (!hps_ctxt.heavy_task_enabled)
		hps_ctxt.hps_func_control &= ~(1 << HPS_FUNC_CTRL_HVY_TSK);
	else
		hps_ctxt.hps_func_control |= (1 << HPS_FUNC_CTRL_HVY_TSK);
	if (!hps_ctxt.big_task_enabled)
		hps_ctxt.hps_func_control &= ~(1 << HPS_FUNC_CTRL_BIG_TSK);
	else
		hps_ctxt.hps_func_control |= (1 << HPS_FUNC_CTRL_BIG_TSK);
	if (!hps_ctxt.eas_enabled)
		hps_ctxt.hps_func_control &= ~(1 << HPS_FUNC_CTRL_EAS);
	else
		hps_ctxt.hps_func_control |= (1 << HPS_FUNC_CTRL_EAS);
	if (!hps_ctxt.idle_det_enabled)
		hps_ctxt.hps_func_control &= ~(1 << HPS_FUNC_CTRL_IDLE_DET);
	else
		hps_ctxt.hps_func_control |= (1 << HPS_FUNC_CTRL_IDLE_DET);
}

#ifdef CONFIG_HPS
void hps_algo_main(void)
{
	unsigned int i, val, base_val, action_print, origin_root, action_break;
	char str_online[64], str_ref_limit[64], str_ref_base[64],
	    str_criteria_limit[64], str_criteria_base[64], str_target[64],
	    str_hvytsk[64], str_pwrseq[64], str_bigtsk[64];
	char *online_ptr = str_online;
	char *criteria_limit_ptr = str_criteria_limit;
	char *criteria_base_ptr = str_criteria_base;
	char *ref_limit_ptr = str_ref_limit;
	char *ref_base_ptr = str_ref_base;
	char *hvytsk_ptr = str_hvytsk;
	char *target_ptr = str_target;
	char *pwrseq_ptr = str_pwrseq;
	char *bigtsk_ptr = str_bigtsk;
	static unsigned int hrtbt_dbg;
#ifdef CONFIG_MTK_ICCS_SUPPORT
	unsigned char real_online_power_state_bitmask = 0;
	unsigned char real_target_power_state_bitmask = 0;
	unsigned char iccs_online_power_state_bitmask = 0;
	unsigned char iccs_target_power_state_bitmask =
	iccs_get_target_power_state_bitmask();
	unsigned char target_cache_shared_state_bitmask = 0;
#endif

	/* Initial value */
	base_val = action_print = action_break =
	hps_sys.total_online_cores = 0;
	hps_sys.up_load_avg = hps_sys.down_load_avg = hps_sys.tlp_avg =
	hps_sys.rush_cnt = 0;
	hps_sys.action_id = origin_root = 0;
	/*
	 * run algo or not by hps_ctxt.enabled
	 */
	if ((u64) ktime_to_ms(ktime_sub(ktime_get(), hps_ctxt.hps_hrt_ktime))
	>= HPS_HRT_DBG_MS)
		action_print = hrtbt_dbg = 1;
	else
		hrtbt_dbg = 0;

	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.action = ACTION_NONE;
	atomic_set(&hps_ctxt.is_ondemand, 0);

	if (!hps_ctxt.enabled)
		goto HPS_END;
	if (hps_ctxt.eas_indicator) {
		/*Set cpu cores by scheduler*/
		goto HPS_ALGO_END;
	}
	/*
	 * algo - begin
	 */
	/*Back up limit and base value for check */

	mutex_lock(&hps_ctxt.para_lock);
	if ((hps_sys.cluster_info[0].base_value == 0) &&
		(hps_sys.cluster_info[1].base_value == 0) &&
		(hps_sys.cluster_info[2].base_value == 0) &&
		(hps_sys.cluster_info[0].limit_value == 0) &&
		(hps_sys.cluster_info[1].limit_value == 0) &&
		(hps_sys.cluster_info[2].limit_value == 0)) {
		hps_sys.cluster_info[0].base_value =
			hps_sys.cluster_info[0].ref_base_value = 0;
		hps_sys.cluster_info[1].base_value =
			hps_sys.cluster_info[1].ref_base_value = 0;
		hps_sys.cluster_info[2].base_value =
			hps_sys.cluster_info[2].ref_base_value = 0;
		hps_sys.cluster_info[0].limit_value =
			hps_sys.cluster_info[0].ref_limit_value = 4;
		hps_sys.cluster_info[1].limit_value =
			hps_sys.cluster_info[1].ref_limit_value = 4;
		hps_sys.cluster_info[2].limit_value =
			hps_sys.cluster_info[2].ref_limit_value = 0;
	}
	for (i = 0; i < hps_sys.cluster_num; i++) {
		hps_sys.cluster_info[i].base_value =
			hps_sys.cluster_info[i].ref_base_value;
		hps_sys.cluster_info[i].limit_value =
			hps_sys.cluster_info[i].ref_limit_value;
	}
	for (i = 0; i < hps_sys.cluster_num; i++) {
		base_val += hps_sys.cluster_info[i].base_value;
		hps_sys.cluster_info[i].target_core_num =
			hps_sys.cluster_info[i].online_core_num = 0;
		hps_sys.cluster_info[i].online_core_num =
		    hps_get_cluster_cpus(hps_sys.cluster_info[i].cluster_id);
		hps_sys.total_online_cores +=
			hps_sys.cluster_info[i].online_core_num;
	}


	mutex_unlock(&hps_ctxt.para_lock);
	/* Determine root cluster */
	origin_root = hps_sys.root_cluster_id;
	hps_define_root_cluster(&hps_sys);
	if (origin_root != hps_sys.root_cluster_id)
		hps_sys.action_id = HPS_SYS_CHANGE_ROOT;

	/*
	 * update history - tlp
	 */
	val = hps_ctxt.tlp_history[hps_ctxt.tlp_history_index];
	hps_ctxt.tlp_history[hps_ctxt.tlp_history_index] = hps_ctxt.cur_tlp;
	hps_ctxt.tlp_sum += hps_ctxt.cur_tlp;
	hps_ctxt.tlp_history_index =
	    (hps_ctxt.tlp_history_index + 1 ==
	     hps_ctxt.tlp_times) ? 0 : hps_ctxt.tlp_history_index + 1;
	++hps_ctxt.tlp_count;
	if (hps_ctxt.tlp_count > hps_ctxt.tlp_times) {
		WARN_ON(hps_ctxt.tlp_sum < val);
		hps_ctxt.tlp_sum -= val;
		hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_times;
	} else {
		hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_count;
	}
	if (hps_ctxt.stats_dump_enabled)
		hps_ctxt_print_algo_stats_tlp(0);

	/*Determine eas enabled or not*/
	if (!hps_ctxt.eas_enabled)
		hps_sys.hps_sys_ops[2].enabled = 0;

	for (i = 0 ; i < hps_sys.cluster_num ; i++)
		hps_sys.cluster_info[i].target_core_num =
			hps_sys.cluster_info[i].online_core_num;

	for (i = 0; i < hps_sys.func_num; i++) {
		if (hps_sys.hps_sys_ops[i].enabled == 1) {
			if (hps_sys.hps_sys_ops[i].hps_sys_func_ptr()) {
				hps_sys.action_id =
					hps_sys.hps_sys_ops[i].func_id;
				break;
			}
		}
	}
	/*
	 *	if (hps_ctxt.heavy_task_enabled)
	 *	if (hps_algo_heavytsk_det())
	 *	hps_sys.action_id = 0xE1;
	 */

	if (hps_ctxt.big_task_enabled)
		if (hps_algo_big_task_det())
			hps_sys.action_id = 0xE2;

	if (hps_sys.action_id == 0)
		goto HPS_END;

HPS_ALGO_END:
	/*
	 * algo - end
	 */

	/*Base and limit check */
	hps_check_base_limit(&hps_sys);

	/* Ensure that root cluster must one online cpu at less */
	if (hps_sys.cluster_info[hps_sys.root_cluster_id].target_core_num <= 0)
		hps_sys.cluster_info[hps_sys.root_cluster_id].target_core_num =
		1;

#ifdef CONFIG_MTK_ICCS_SUPPORT
	real_online_power_state_bitmask = 0;
	real_target_power_state_bitmask = 0;
	for (i = 0; i < hps_sys.cluster_num; i++) {
		real_online_power_state_bitmask |=
			((hps_sys.cluster_info[i].online_core_num > 0) << i);
		real_target_power_state_bitmask |=
			((hps_sys.cluster_info[i].target_core_num > 0) << i);
	}
	iccs_online_power_state_bitmask = iccs_target_power_state_bitmask;
	iccs_target_power_state_bitmask = real_target_power_state_bitmask;
	iccs_get_target_state(&iccs_target_power_state_bitmask,
		&target_cache_shared_state_bitmask);

	/*
	 * pr_notice("[%s] iccs_target_power_state_bitmask: 0x%x\n",
	 * __func__, iccs_target_power_state_bitmask);
	 */

	for (i = 0; i < hps_sys.cluster_num; i++) {
		hps_sys.cluster_info[i].iccs_state =
			(((real_online_power_state_bitmask >> i) & 1) << 3) |
			(((real_target_power_state_bitmask >> i) & 1) << 2) |
			(((iccs_online_power_state_bitmask >> i) & 1) << 1) |
			(((iccs_target_power_state_bitmask >> i) & 1) << 0);

		/*
		 * pr_notice("[%s] cluster: 0x%x iccs_state: 0x%x\n",
		 * __func__, i, hps_sys.cluster_info[i].iccs_state);
		 */

		if (hps_get_iccs_pwr_status(i) == 0x1)
			iccs_cluster_on_off(i, 1);
		else if (hps_get_iccs_pwr_status(i) == 0x2)
			iccs_cluster_on_off(i, 0);
	}
#endif

#if 1 /*Make sure that priority of power on action is higher than power down.*/
	for (i = 0; i < hps_sys.cluster_num; i++) {
		if (hps_sys.cluster_info[i].target_core_num >
		    hps_sys.cluster_info[i].online_core_num) {
			if (hps_algo_do_cluster_action(i) == 1) {
				action_print = action_break = 1;
				break;
			}
			action_print = 1;
		}
	}
	if (!action_break) {
		for (i = 0; i < hps_sys.cluster_num; i++) {
			if (hps_sys.cluster_info[i].target_core_num <
			    hps_sys.cluster_info[i].online_core_num) {
				if (hps_algo_do_cluster_action(i) == 1) {
					action_print = action_break = 1;
					break;
				}

				action_print = 1;
			}
		}
	}
#else
	/*Process root cluster first */
	if (hps_sys.cluster_info[hps_sys.root_cluster_id].target_core_num !=
	    hps_sys.cluster_info[hps_sys.root_cluster_id].online_core_num) {
		if (hps_algo_do_cluster_action(hps_sys.root_cluster_id) == 1)
			action_break = 1;
		else
			action_break = 0;
		action_print = 1;
	}

	for (i = 0; i < hps_sys.cluster_num; i++) {
		if (i == hps_sys.root_cluster_id)
			continue;
		if (hps_sys.cluster_info[i].target_core_num !=
		    hps_sys.cluster_info[i].online_core_num) {
			if (hps_algo_do_cluster_action(i) == 1)
				action_break = 1;
			else
				action_break = 0;
			action_print = 1;
		}
	}

#endif
#ifdef CONFIG_MTK_ICCS_SUPPORT
	for (i = 0; i < hps_sys.cluster_num; i++) {
		if (hps_get_cluster_cpus(hps_sys.cluster_info[i].cluster_id) !=
				hps_sys.cluster_info[i].target_core_num) {
			if (
		hps_get_cluster_cpus(hps_sys.cluster_info[i].cluster_id) == 0)
			/* cannot turn on */
				iccs_target_power_state_bitmask =
				(iccs_target_power_state_bitmask & ~(1 << i))
				| (iccs_online_power_state_bitmask & (1 << i));
			else if (hps_sys.cluster_info[i].target_core_num == 0)
			/* cannot turn off */
				iccs_target_power_state_bitmask |= (1 << i);
		}
	}
	/*
	 * pr_notice("[%s] iccs_target_power_state_bitmask: 0x%x\n",
	 * __func__, iccs_target_power_state_bitmask);
	 */
	iccs_set_target_power_state_bitmask(iccs_target_power_state_bitmask);
#endif
HPS_END:
	if (action_print || hrtbt_dbg) {
		int online, target, ref_limit, ref_base, criteria_limit,
			criteria_base, hvytsk, pwrseq, bigtsk;

		mutex_lock(&hps_ctxt.para_lock);

		online = target = criteria_limit = criteria_base = 0;
		for (i = 0; i < hps_sys.cluster_num; i++) {
			if (i == origin_root)
				online =
				    sprintf(online_ptr, "<%d>",
				    hps_sys.cluster_info[i].online_core_num);
			else
				online =
				    sprintf(online_ptr, "(%d)",
				    hps_sys.cluster_info[i].online_core_num);

			if (i == hps_sys.root_cluster_id)
				target =
				    sprintf(target_ptr, "<%d>",
				    hps_sys.cluster_info[i].target_core_num);
			else
				target =
				    sprintf(target_ptr, "(%d)",
				    hps_sys.cluster_info[i].target_core_num);

			criteria_limit =
			    sprintf(criteria_limit_ptr, "(%d)",
				    hps_sys.cluster_info[i].limit_value);
			criteria_base =
			    sprintf(criteria_base_ptr, "(%d)",
			    hps_sys.cluster_info[i].base_value);
			ref_limit =
			    sprintf(ref_limit_ptr, "(%d)",
			    hps_sys.cluster_info[i].ref_limit_value);
			ref_base =
			    sprintf(ref_base_ptr, "(%d)",
			    hps_sys.cluster_info[i].ref_base_value);
			hvytsk = sprintf(hvytsk_ptr, "(%d)",
			hps_sys.cluster_info[i].hvyTsk_value);
			bigtsk = sprintf(bigtsk_ptr, "(%d)",
			hps_sys.cluster_info[i].bigTsk_value);
			if (i == 0)
				pwrseq = sprintf(pwrseq_ptr,
				"(%d->", hps_sys.cluster_info[i].pwr_seq);
			else if ((i != 0) && (i != (hps_sys.cluster_num - 1)))
				pwrseq = sprintf(pwrseq_ptr, "%d->",
				hps_sys.cluster_info[i].pwr_seq);
			else if (i == (hps_sys.cluster_num - 1))
				pwrseq = sprintf(pwrseq_ptr, "%d) ",
				hps_sys.cluster_info[i].pwr_seq);

			online_ptr += online;
			target_ptr += target;
			criteria_limit_ptr += criteria_limit;
			criteria_base_ptr += criteria_base;
			ref_limit_ptr += ref_limit;
			ref_base_ptr += ref_base;
			hvytsk_ptr += hvytsk;
			bigtsk_ptr += bigtsk;
			pwrseq_ptr += pwrseq;
		}
		mutex_unlock(&hps_ctxt.para_lock);
		if (action_print) {
			hps_set_funct_ctrl();
			if (action_break)
				hps_warn
("(0x%X)%s action break!! (%u)(%u)(%u) %s %s%s-->%s%s (%u)(%u)(%u)(%u) %s\n",
((hps_ctxt.hps_func_control << 12) | hps_sys.action_id),
str_online, hps_ctxt.cur_loads,
hps_ctxt.cur_tlp, hps_ctxt.cur_iowait, str_hvytsk,
str_criteria_limit, str_criteria_base,
str_ref_limit, str_ref_base,
hps_sys.up_load_avg,
hps_sys.down_load_avg, hps_sys.tlp_avg, hps_sys.rush_cnt,
str_target);
			else {
				char str1[256];
				char str2[256];

				snprintf(str1, sizeof(str1),
"(0x%X)%s action end (%u)(%u)(%u) %s %s[%u](%u) %s %s%s (%u)(%u)(%u)(%u)",
((hps_ctxt.hps_func_control << 12) | hps_sys.action_id),
str_online, hps_ctxt.cur_loads,
hps_ctxt.cur_tlp, hps_ctxt.cur_iowait,
str_hvytsk, str_bigtsk, hps_ctxt.is_idle, hps_ctxt.idle_ratio,
str_pwrseq, str_criteria_limit, str_criteria_base,
hps_sys.up_load_avg,
hps_sys.down_load_avg,
hps_sys.tlp_avg, hps_sys.rush_cnt);

				snprintf(str2, sizeof(str2),
"[%u,%u|%u,%u|%u,%u][%u,%u,%u] [%u,%u,%u] [%u,%u,%u] [%u,%u,%u] %s",
hps_sys.cluster_info[0].up_threshold,
hps_sys.cluster_info[0].down_threshold,
hps_sys.cluster_info[1].up_threshold,
hps_sys.cluster_info[1].down_threshold,
hps_sys.cluster_info[2].up_threshold,
hps_sys.cluster_info[2].down_threshold,
hps_sys.cluster_info[0].loading,
hps_sys.cluster_info[1].loading,
hps_sys.cluster_info[2].loading,
hps_sys.cluster_info[0].rel_load,
hps_sys.cluster_info[1].rel_load,
hps_sys.cluster_info[2].rel_load,
hps_sys.cluster_info[0].abs_load,
hps_sys.cluster_info[1].abs_load,
hps_sys.cluster_info[2].abs_load,
/* sched-assist hotplug: for debug */
hps_sys.cluster_info[0].sched_load,
hps_sys.cluster_info[1].sched_load,
hps_sys.cluster_info[2].sched_load,
str_target);

				hps_warn("%s%s\n", str1, str2);
#ifdef _TRACE_
				trace_hps_update(
hps_sys.action_id, str_online, hps_ctxt.cur_loads,
hps_ctxt.cur_tlp, hps_ctxt.cur_iowait, str_hvytsk,
str_criteria_limit, str_criteria_base,
hps_sys.up_load_avg, hps_sys.down_load_avg,
hps_sys.tlp_avg,
hps_sys.rush_hps_sys.cluster_info[0].up_threshold,
hps_sys.cluster_info[0].down_threshold,
hps_sys.cluster_info[0].up_threshold,
hps_sys.cluster_info[0].down_threshold,
hps_sys.cluster_info[2].up_threshold,
hps_sys.cluster_info[2].down_threshold,
hps_sys.cluster_info[0].loading, hps_sys.cluster_info[1].loading,
hps_sys.cluster_info[2].loading,
hps_ctxt.up_times, hps_ctxt.down_times, str_target);
#endif
			}
			hps_ctxt_reset_stas_nolock();
		}
	}
#if HPS_HRT_BT_EN
	if (hrtbt_dbg) {
		hps_set_funct_ctrl();
		hps_warn(
"(0x%X)%s HRT_BT_DBG (%u)(%u)(%u) %s %s %s %s%s (%u)(%u)(%u)(%u) %s\n",
((hps_ctxt.hps_func_control << 12) | hps_sys.action_id),
str_online, hps_ctxt.cur_loads, hps_ctxt.cur_tlp,
hps_ctxt.cur_iowait, str_hvytsk, str_bigtsk, str_pwrseq, str_criteria_limit,
str_criteria_base, hps_sys.up_load_avg, hps_sys.down_load_avg,
hps_sys.tlp_avg, hps_sys.rush_cnt, str_target);
		hrtbt_dbg = 0;
		hps_ctxt.hps_hrt_ktime = ktime_get();
	}
#endif
	action_print = 0;
	action_break = 0;
	mutex_unlock(&hps_ctxt.lock);
}
#else /* CONFIG_HPS */
void hps_algo_main(void)
{

}
#endif /* !CONFIG_HPS */
