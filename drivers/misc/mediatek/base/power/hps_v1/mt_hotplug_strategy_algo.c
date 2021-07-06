/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * @file    mt_hotplug_strategy_algo.c
 * @brief   hotplug strategy(hps) - algo
 */

#include <linux/kernel.h>
#include <linux/module.h>	/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/kthread.h>	/* kthread_create */
#include <linux/delay.h>	/* msleep */
#include <asm-generic/bug.h>	/* WARN_ON */

#include "mt_hotplug_strategy_internal.h"

/*
 * hps algo - hmp
 */

static void algo_hmp_limit(
		struct cpumask *little_online_cpumask,
		struct cpumask *big_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online,
		unsigned int big_num_base,
		unsigned int big_num_limit,
		unsigned int big_num_online)
{
	unsigned int cpu;
	unsigned int val;

	if (big_num_online > big_num_limit) {
		val = big_num_online - big_num_limit;
		for (cpu = hps_ctxt.big_cpu_id_max;
			cpu >= hps_ctxt.big_cpu_id_min; --cpu) {
			if (!cpumask_test_cpu(cpu, big_online_cpumask))
				continue;

			hps_cpu_down(cpu);
			cpumask_clear_cpu(cpu, big_online_cpumask);
			--big_num_online;
			if (--val == 0)
				break;
		}
		WARN_ON(val);
		hps_ctxt.action |= BIT(ACTION_LIMIT_BIG);
	}

	if (little_num_online > little_num_limit) {
		val = little_num_online - little_num_limit;
		for (cpu = hps_ctxt.little_cpu_id_max;
			cpu > hps_ctxt.little_cpu_id_min; --cpu) {
			if (!cpumask_test_cpu(cpu, little_online_cpumask))
				continue;

			hps_cpu_down(cpu);
			cpumask_clear_cpu(cpu, little_online_cpumask);
			--little_num_online;
			if (--val == 0)
				break;
		}
		WARN_ON(val);
		hps_ctxt.action |= BIT(ACTION_LIMIT_LITTLE);
	}
}

static void algo_hmp_base(
		struct cpumask *little_online_cpumask,
		struct cpumask *big_online_cpumask,
		unsigned int lb /* little_num_base */,
		unsigned int ll /* little_num_limit */,
		unsigned int lo /* little_num_online */,
		unsigned int bb /* big_num_base */,
		unsigned int bl /* big_num_limit */,
		unsigned int bo /* big_num_online */)
{
	unsigned int cpu;
	int val;
	unsigned int num_online = lo + bo;

	WARN_ON(bo > bl);
	WARN_ON(lo > ll);

	if (bo < bb && bo < bl) {
		val = min(bb, bl) - bo;
		for (cpu = hps_ctxt.big_cpu_id_min;
			cpu <= hps_ctxt.big_cpu_id_max; ++cpu) {
			if (cpumask_test_cpu(cpu, big_online_cpumask))
				continue;

			hps_cpu_up(cpu);
			cpumask_set_cpu(cpu, big_online_cpumask);
			++bo;
			if (--val == 0)
				break;
		}
		WARN_ON(val);
		hps_ctxt.action |= BIT(ACTION_BASE_BIG);
	}

	if (lo < lb && lo < ll && (num_online < lb + bb)) {
		val = min(lb, ll) - lo;
		if (bo > bb)
			val -= bo - bb;
		if (val > 0) {
			for (cpu = hps_ctxt.little_cpu_id_min;
				cpu <= hps_ctxt.little_cpu_id_max; ++cpu) {
				if (cpumask_test_cpu(cpu,
						little_online_cpumask))
					continue;

				hps_cpu_up(cpu);
				cpumask_set_cpu(cpu, little_online_cpumask);
				++lo;
				if (--val == 0)
					break;
			}
			WARN_ON(val);
			hps_ctxt.action |= BIT(ACTION_BASE_LITTLE);
		}
	}
}

static void algo_hmp_rush_boost(
		struct cpumask *little_online_cpumask,
		struct cpumask *big_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online,
		unsigned int big_num_base,
		unsigned int big_num_limit,
		unsigned int big_num_online)
{
	unsigned int cpu;
	unsigned int val;
	unsigned int num_online = little_num_online + big_num_online;

	if (!hps_ctxt.rush_boost_enabled)
		return;

	if (hps_ctxt.cur_loads > hps_ctxt.rush_boost_threshold * num_online)
		++hps_ctxt.rush_count;
	else
		hps_ctxt.rush_count = 0;

	if (hps_ctxt.rush_count < hps_ctxt.rush_boost_times ||
		num_online * 100 >= hps_ctxt.tlp_avg)
		return;

	val = hps_ctxt.tlp_avg / 100 + (hps_ctxt.tlp_avg % 100 ? 1 : 0);
	WARN_ON(!(val > num_online));
	if (val > num_possible_cpus())
		val = num_possible_cpus();

	val -= num_online;
	if (val && little_num_online < little_num_limit) {
		for (cpu = hps_ctxt.little_cpu_id_min;
			cpu <= hps_ctxt.little_cpu_id_max; ++cpu) {
			if (cpumask_test_cpu(cpu, little_online_cpumask))
				continue;

			hps_cpu_up(cpu);
			cpumask_set_cpu(cpu, little_online_cpumask);
			++little_num_online;
			--val;
			if (val == 0 || little_num_online == little_num_limit)
				break;
		}
		hps_ctxt.action |= BIT(ACTION_RUSH_BOOST_LITTLE);
	} else if (val && big_num_online < big_num_limit) {
		for (cpu = hps_ctxt.big_cpu_id_min;
			cpu <= hps_ctxt.big_cpu_id_max; ++cpu) {
			if (cpumask_test_cpu(cpu, big_online_cpumask))
				continue;

			hps_cpu_up(cpu);
			cpumask_set_cpu(cpu, big_online_cpumask);
			++big_num_online;
			--val;
			if (val == 0 || big_num_online == big_num_limit)
				break;
		}
		hps_ctxt.action |= BIT(ACTION_RUSH_BOOST_BIG);
	}
}

static void algo_hmp_up(
		struct cpumask *little_online_cpumask,
		struct cpumask *big_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online,
		unsigned int big_num_base,
		unsigned int big_num_limit,
		unsigned int big_num_online)
{
	unsigned int cpu;
	unsigned int val;
	unsigned int num_online = little_num_online + big_num_online;

	if (num_online >= num_possible_cpus())
		return;

	/*
	 * update history - up
	 */
	val = hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index];
	hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index] =
		hps_ctxt.cur_loads;
	hps_ctxt.up_loads_sum += hps_ctxt.cur_loads;
	hps_ctxt.up_loads_history_index =
		(hps_ctxt.up_loads_history_index + 1 == hps_ctxt.up_times) ?
			0 : hps_ctxt.up_loads_history_index + 1;
	++hps_ctxt.up_loads_count;

	if (hps_ctxt.up_loads_count > hps_ctxt.up_times) {
		WARN_ON(hps_ctxt.up_loads_sum < val);
		hps_ctxt.up_loads_sum -= val;
	}

	hps_ctxt_print_algo_stats_up(0);

	if (hps_ctxt.up_loads_count < hps_ctxt.up_times)
		return;

	if (hps_ctxt.up_loads_sum <=
		hps_ctxt.up_threshold * hps_ctxt.up_times * num_online)
		return;

	if (little_num_online < little_num_limit) {
		for (cpu = hps_ctxt.little_cpu_id_min;
			cpu <= hps_ctxt.little_cpu_id_max; ++cpu) {
			if (!cpumask_test_cpu(cpu, little_online_cpumask)) {
				hps_cpu_up(cpu);
				cpumask_set_cpu(cpu, little_online_cpumask);
				++little_num_online;
				break;
			}
		}
		hps_ctxt.action |= BIT(ACTION_UP_LITTLE);
	} else if (big_num_online < big_num_limit) {
		for (cpu = hps_ctxt.big_cpu_id_min;
			cpu <= hps_ctxt.big_cpu_id_max; ++cpu) {
			if (!cpumask_test_cpu(cpu, big_online_cpumask)) {
				hps_cpu_up(cpu);
				cpumask_set_cpu(cpu, big_online_cpumask);
				++big_num_online;
				break;
			}
		}
		hps_ctxt.action |= BIT(ACTION_UP_BIG);
	}
}

static void algo_hmp_down(
		struct cpumask *little_online_cpumask,
		struct cpumask *big_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online,
		unsigned int big_num_base,
		unsigned int big_num_limit,
		unsigned int big_num_online)
{
	unsigned int cpu;
	unsigned int val;
	unsigned int down_threshold;
	unsigned int num_online = little_num_online + big_num_online;

	if (num_online <= 1)
		return;

	/*
	 * update history - down
	 */
	val = hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index];
	hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index] =
		hps_ctxt.cur_loads;
	hps_ctxt.down_loads_sum += hps_ctxt.cur_loads;
	hps_ctxt.down_loads_history_index =
		(hps_ctxt.down_loads_history_index + 1 == hps_ctxt.down_times) ?
			0 : hps_ctxt.down_loads_history_index + 1;
	++hps_ctxt.down_loads_count;

	if (hps_ctxt.down_loads_count > hps_ctxt.down_times) {
		WARN_ON(hps_ctxt.down_loads_sum < val);
		hps_ctxt.down_loads_sum -= val;
	}

	hps_ctxt_print_algo_stats_down(0);

	if (hps_ctxt.down_loads_count < hps_ctxt.down_times)
		return;

	down_threshold = hps_ctxt.down_threshold * hps_ctxt.down_times;

	val = num_online;
	while (hps_ctxt.down_loads_sum < down_threshold * (val - 1))
		--val;
	val = num_online - val;

	if (val > 1 && !hps_ctxt.quick_landing_enabled)
		val = 1;

	if (val && big_num_online > big_num_base) {
		for (cpu = hps_ctxt.big_cpu_id_max;
			cpu >= hps_ctxt.big_cpu_id_min; --cpu) {
			if (!cpumask_test_cpu(cpu, big_online_cpumask))
				continue;

			hps_cpu_down(cpu);
			cpumask_clear_cpu(cpu, big_online_cpumask);
			--big_num_online;
			--val;
			if (val == 0 || big_num_online == big_num_base)
				break;

		}
		hps_ctxt.action |= BIT(ACTION_DOWN_BIG);
	} else if (val && little_num_online > little_num_base) {
		for (cpu = hps_ctxt.little_cpu_id_max;
			cpu > hps_ctxt.little_cpu_id_min; --cpu) {
			if (!cpumask_test_cpu(cpu, little_online_cpumask))
				continue;

			hps_cpu_down(cpu);
			cpumask_clear_cpu(cpu, little_online_cpumask);
			--little_num_online;
			--val;
			if (val == 0 || little_num_online == little_num_base)
				break;
		}
		hps_ctxt.action |= BIT(ACTION_DOWN_LITTLE);
	}
}

static void algo_hmp_big_to_little(
		struct cpumask *little_online_cpumask,
		struct cpumask *big_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online,
		unsigned int big_num_base,
		unsigned int big_num_limit,
		unsigned int big_num_online)
{
	unsigned int cpu;
	unsigned int val;
	unsigned int load;
	unsigned int num_online = little_num_online + big_num_online;

	if (hps_ctxt.down_loads_count < hps_ctxt.down_times)
		return;

	if (little_num_online >= little_num_limit ||
		big_num_online <= big_num_base)
		return;

	/* find last online big */
	for (val = hps_ctxt.big_cpu_id_max;
		val >= hps_ctxt.big_cpu_id_min; --val) {
		if (cpumask_test_cpu(val, big_online_cpumask))
			break;
	}
	WARN_ON(val < hps_ctxt.big_cpu_id_min);

	/* verify whether b2L will open 1 little */
	load = per_cpu(hps_percpu_ctxt, val).load * CPU_DMIPS_BIG_LITTLE_DIFF;
	load /= 100;
	load += hps_ctxt.up_loads_sum / hps_ctxt.up_times;
	if (load > hps_ctxt.up_threshold * num_online)
		return;

	/* up 1 little */
	for (cpu = hps_ctxt.little_cpu_id_min;
		cpu <= hps_ctxt.little_cpu_id_max; ++cpu) {
		if (cpumask_test_cpu(cpu, little_online_cpumask))
			continue;

		hps_cpu_up(cpu);
		cpumask_set_cpu(cpu, little_online_cpumask);
		++little_num_online;
		break;
	}

	/* down 1 big */
	hps_cpu_down(val);
	cpumask_clear_cpu(cpu, big_online_cpumask);
	--big_num_online;
	hps_ctxt.action |= BIT(ACTION_BIG_TO_LITTLE);
}

void hps_algo_hmp(void)
{
	unsigned int cpu;
	unsigned int val;
	struct cpumask little_online_cpumask;
	struct cpumask big_online_cpumask;
	unsigned int little_num_base, little_num_limit, little_num_online;
	unsigned int big_num_base, big_num_limit, big_num_online;
	/* log purpose */
	char str1[64];
	char str2[64];
	int i, j;
	char *str1_ptr = str1;
	char *str2_ptr = str2;

	/*
	 * run algo or not by hps_ctxt.enabled
	 */
	if (!hps_ctxt.enabled) {
		atomic_set(&hps_ctxt.is_ondemand, 0);
		return;
	}

	/*
	 * calculate cpu loading
	 */
	hps_ctxt.cur_loads = 0;
	str1_ptr = str1;
	str2_ptr = str2;

	for_each_possible_cpu(cpu) {
		per_cpu(hps_percpu_ctxt, cpu).load =
			hps_cpu_get_percpu_load(cpu);
		hps_ctxt.cur_loads += per_cpu(hps_percpu_ctxt, cpu).load;

		if (log_is_en(HPS_LOG_ALGO)) {
			if (cpu_online(cpu))
				i = sprintf(str1_ptr, "%4u", 1);
			else
				i = sprintf(str1_ptr, "%4u", 0);
			str1_ptr += i;
			j = sprintf(str2_ptr, "%4u",
				per_cpu(hps_percpu_ctxt, cpu).load);
			str2_ptr += j;
		}
	}
	hps_ctxt.cur_nr_heavy_task = hps_cpu_get_nr_heavy_task();
	hps_cpu_get_tlp(&hps_ctxt.cur_tlp, &hps_ctxt.cur_iowait);

	/*
	 * algo - begin
	 */
	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.action = ACTION_NONE;
	atomic_set(&hps_ctxt.is_ondemand, 0);

	/*
	 * algo - get boundary
	 */
	little_num_limit = num_limit_little_cpus();
	little_num_base = num_base_little_cpus();
	cpumask_and(&little_online_cpumask, &hps_ctxt.little_cpumask,
		cpu_online_mask);
	little_num_online = cpumask_weight(&little_online_cpumask);
	/* TODO: no need if is_hmp */
	big_num_limit = num_limit_big_cpus();
	big_num_base = num_base_big_cpus();
	cpumask_and(&big_online_cpumask, &hps_ctxt.big_cpumask,
			cpu_online_mask);
	big_num_online = cpumask_weight(&big_online_cpumask);

	log_alog(" CPU:%s\n", str1);
	log_alog("LOAD:%s\n", str2);
	log_alog(
		"loads(%u), hvy_tsk(%u), tlp(%u), iowait(%u), limit_t(%u)(%u), limit_lb(%u)(%u), limit_ups(%u)(%u), limit_pos(%u)(%u), limit_c1(%u)(%u), limit_c2(%u)(%u), base_pes(%u)(%u), base_c1(%u)(%u), base_c2(%u)(%u)\n",
		hps_ctxt.cur_loads, hps_ctxt.cur_nr_heavy_task,
		hps_ctxt.cur_tlp, hps_ctxt.cur_iowait,
		hps_ctxt.little_num_limit_thermal,
		hps_ctxt.big_num_limit_thermal,
		hps_ctxt.little_num_limit_low_battery,
		hps_ctxt.big_num_limit_low_battery,
		hps_ctxt.little_num_limit_ultra_power_saving,
		hps_ctxt.big_num_limit_ultra_power_saving,
		hps_ctxt.little_num_limit_power_serv,
		hps_ctxt.big_num_limit_power_serv,
		hps_ctxt.little_num_limit_custom1,
		hps_ctxt.big_num_limit_custom1,
		hps_ctxt.little_num_limit_custom2,
		hps_ctxt.big_num_limit_custom2,
		hps_ctxt.little_num_base_perf_serv,
		hps_ctxt.big_num_base_perf_serv,
		hps_ctxt.little_num_base_custom1,
		hps_ctxt.big_num_base_custom1,
		hps_ctxt.little_num_base_custom2,
		hps_ctxt.big_num_base_custom2);

	/*
	 * algo - thermal, low battery
	 */
	algo_hmp_limit(&little_online_cpumask, &big_online_cpumask,
		little_num_base, little_num_limit, little_num_online,
		big_num_base, big_num_limit, big_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * algo - PerfService, heavy task detect
	 */
	algo_hmp_base(&little_online_cpumask, &big_online_cpumask,
		little_num_base, little_num_limit, little_num_online,
		big_num_base, big_num_limit, big_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * update history - tlp
	 */
	val = hps_ctxt.tlp_history[hps_ctxt.tlp_history_index];
	hps_ctxt.tlp_history[hps_ctxt.tlp_history_index] = hps_ctxt.cur_tlp;
	hps_ctxt.tlp_sum += hps_ctxt.cur_tlp;
	hps_ctxt.tlp_history_index =
		(hps_ctxt.tlp_history_index + 1 == hps_ctxt.tlp_times) ?
			0 : hps_ctxt.tlp_history_index + 1;
	++hps_ctxt.tlp_count;
	if (hps_ctxt.tlp_count > hps_ctxt.tlp_times) {
		WARN_ON(hps_ctxt.tlp_sum < val);
		hps_ctxt.tlp_sum -= val;
		hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_times;
	} else
		hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_count;
	hps_ctxt_print_algo_stats_tlp(0);

	/*
	 * algo - rush boost
	 */
	algo_hmp_rush_boost(&little_online_cpumask, &big_online_cpumask,
		little_num_base, little_num_limit, little_num_online,
		big_num_base, big_num_limit, big_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * algo - cpu up
	 */
	algo_hmp_up(&little_online_cpumask, &big_online_cpumask,
		little_num_base, little_num_limit, little_num_online,
		big_num_base, big_num_limit, big_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * algo - cpu down (inc. quick landing)
	 */
	algo_hmp_down(&little_online_cpumask, &big_online_cpumask,
		little_num_base, little_num_limit, little_num_online,
		big_num_base, big_num_limit, big_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * algo - b2L
	 */
	algo_hmp_big_to_little(&little_online_cpumask, &big_online_cpumask,
		little_num_base, little_num_limit, little_num_online,
		big_num_base, big_num_limit, big_num_online);

	if (!hps_ctxt.action)
		goto ALGO_END_WO_ACTION;

	/*
	 * algo - end
	 */
ALGO_END_WITH_ACTION:
	log_act(
		"(%04x)(%u)(%u)action end(%u)(%u)(%u)(%u) (%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u) (%u)(%u)(%u) (%u)(%u)(%u) (%u)(%u)(%u)(%u)(%u)\n",
		hps_ctxt.action, little_num_online, big_num_online,
		hps_ctxt.cur_loads, hps_ctxt.cur_tlp,
		hps_ctxt.cur_iowait, hps_ctxt.cur_nr_heavy_task,
		hps_ctxt.little_num_limit_thermal,
		hps_ctxt.big_num_limit_thermal,
		hps_ctxt.little_num_limit_low_battery,
		hps_ctxt.big_num_limit_low_battery,
		hps_ctxt.little_num_limit_ultra_power_saving,
		hps_ctxt.big_num_limit_ultra_power_saving,
		hps_ctxt.little_num_limit_power_serv,
		hps_ctxt.big_num_limit_power_serv,
		hps_ctxt.little_num_limit_custom1,
		hps_ctxt.big_num_limit_custom1,
		hps_ctxt.little_num_limit_custom2,
		hps_ctxt.big_num_limit_custom2,
		hps_ctxt.little_num_base_perf_serv,
		hps_ctxt.big_num_base_perf_serv,
		hps_ctxt.little_num_base_custom1,
		hps_ctxt.big_num_base_custom1,
		hps_ctxt.little_num_base_custom2,
		hps_ctxt.big_num_base_custom2,
		hps_ctxt.up_loads_sum, hps_ctxt.up_loads_count,
		hps_ctxt.up_loads_history_index,
		hps_ctxt.down_loads_sum, hps_ctxt.down_loads_count,
		hps_ctxt.down_loads_history_index,
		hps_ctxt.rush_count, hps_ctxt.tlp_sum, hps_ctxt.tlp_count,
		hps_ctxt.tlp_history_index, hps_ctxt.tlp_avg);
	hps_ctxt_reset_stas_nolock();
ALGO_END_WO_ACTION:
	mutex_unlock(&hps_ctxt.lock);
}

/*
 * hps algo - smp
 */
static void algo_smp_limit(
		struct cpumask *little_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online)
{
	unsigned int cpu;
	unsigned int val;

	if (little_num_online <= little_num_limit)
		return;

	val = little_num_online - little_num_limit;

	for (cpu = hps_ctxt.little_cpu_id_max;
		cpu > hps_ctxt.little_cpu_id_min; --cpu) {
		if (!cpumask_test_cpu(cpu, little_online_cpumask))
			continue;

		hps_cpu_down(cpu);
		cpumask_clear_cpu(cpu, little_online_cpumask);
		--little_num_online;

		if (--val == 0)
			break;
	}

	WARN_ON(val);
	hps_ctxt.action |= BIT(ACTION_LIMIT_LITTLE);
}

static void algo_smp_base(
		struct cpumask *little_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online)
{
	unsigned int cpu;
	unsigned int val;

	WARN_ON(little_num_online > little_num_limit);
	if (little_num_online >= little_num_base ||
		little_num_online >= little_num_limit)
		return;

	val = min(little_num_base, little_num_limit) - little_num_online;

	for (cpu = hps_ctxt.little_cpu_id_min;
		cpu <= hps_ctxt.little_cpu_id_max; ++cpu) {
		if (cpumask_test_cpu(cpu, little_online_cpumask))
			continue;

		hps_cpu_up(cpu);
		cpumask_set_cpu(cpu, little_online_cpumask);
		++little_num_online;

		--val;
		if (val == 0 || little_num_online == little_num_limit)
			break;
	}

	WARN_ON(val);
	hps_ctxt.action |= BIT(ACTION_BASE_LITTLE);
}

static void algo_smp_rush_boost(
		struct cpumask *little_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online)
{
	unsigned int cpu;
	unsigned int val;

	if (!hps_ctxt.rush_boost_enabled)
		return;

	if (hps_ctxt.cur_loads >
		hps_ctxt.rush_boost_threshold * little_num_online)
		++hps_ctxt.rush_count;
	else
		hps_ctxt.rush_count = 0;

	if (hps_ctxt.rush_count < hps_ctxt.rush_boost_times ||
		little_num_online * 100 >= hps_ctxt.tlp_avg)
		return;

	val = hps_ctxt.tlp_avg / 100 + (hps_ctxt.tlp_avg % 100 ? 1 : 0);
	WARN_ON(!(val > little_num_online));
	if (val > num_possible_cpus())
		val = num_possible_cpus();

	val -= little_num_online;

	if (!val || little_num_online >= little_num_limit)
		return;

	for (cpu = hps_ctxt.little_cpu_id_min;
		cpu <= hps_ctxt.little_cpu_id_max; ++cpu) {
		if (cpumask_test_cpu(cpu, little_online_cpumask))
			continue;

		hps_cpu_up(cpu);
		cpumask_set_cpu(cpu, little_online_cpumask);
		++little_num_online;

		if (--val == 0)
			break;
	}

	hps_ctxt.action |= BIT(ACTION_RUSH_BOOST_LITTLE);
}

static void algo_smp_up(
		struct cpumask *little_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online)
{
	unsigned int cpu;
	unsigned int val;

	if (little_num_online >= num_possible_cpus())
		return;

	/*
	 * update history - up
	 */
	val = hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index];
	hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index] =
		hps_ctxt.cur_loads;
	hps_ctxt.up_loads_sum += hps_ctxt.cur_loads;
	hps_ctxt.up_loads_history_index =
		(hps_ctxt.up_loads_history_index + 1 == hps_ctxt.up_times) ?
				0 : hps_ctxt.up_loads_history_index + 1;
	++hps_ctxt.up_loads_count;

	if (hps_ctxt.up_loads_count > hps_ctxt.up_times) {
		WARN_ON(hps_ctxt.up_loads_sum < val);
		hps_ctxt.up_loads_sum -= val;
	}

	hps_ctxt_print_algo_stats_up(0);

	if (hps_ctxt.up_loads_count < hps_ctxt.up_times)
		return;

	if (hps_ctxt.up_loads_sum <=
		hps_ctxt.up_threshold * hps_ctxt.up_times * little_num_online)
		return;

	if (little_num_online >= little_num_limit)
		return;

	for (cpu = hps_ctxt.little_cpu_id_min;
		cpu <= hps_ctxt.little_cpu_id_max; ++cpu) {
		if (cpumask_test_cpu(cpu, little_online_cpumask))
			continue;

		hps_cpu_up(cpu);
		cpumask_set_cpu(cpu, little_online_cpumask);
		++little_num_online;
		break;
	}

	hps_ctxt.action |= BIT(ACTION_UP_LITTLE);
}

static void algo_smp_down(
		struct cpumask *little_online_cpumask,
		unsigned int little_num_base,
		unsigned int little_num_limit,
		unsigned int little_num_online)
{
	unsigned int cpu;
	unsigned int val;
	unsigned int down_threshold;

	if (little_num_online <= 1)
		return;

	/*
	 * update history - down
	 */
	val = hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index];
	hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index] =
		hps_ctxt.cur_loads;
	hps_ctxt.down_loads_sum += hps_ctxt.cur_loads;
	hps_ctxt.down_loads_history_index =
		(hps_ctxt.down_loads_history_index + 1 == hps_ctxt.down_times) ?
			0 : hps_ctxt.down_loads_history_index + 1;
	++hps_ctxt.down_loads_count;

	if (hps_ctxt.down_loads_count > hps_ctxt.down_times) {
		WARN_ON(hps_ctxt.down_loads_sum < val);
		hps_ctxt.down_loads_sum -= val;
	}

	hps_ctxt_print_algo_stats_down(0);

	if (hps_ctxt.down_loads_count < hps_ctxt.down_times)
		return;

	down_threshold = hps_ctxt.down_threshold * hps_ctxt.down_times;

	val = little_num_online;
	while (hps_ctxt.down_loads_sum < down_threshold * (val - 1))
		--val;
	val = little_num_online - val;

	if (val > 1 && !hps_ctxt.quick_landing_enabled)
		val = 1;

	if (!val || little_num_online <= little_num_base)
		return;

	for (cpu = hps_ctxt.little_cpu_id_max;
		cpu > hps_ctxt.little_cpu_id_min; --cpu) {
		if (!cpumask_test_cpu(cpu, little_online_cpumask))
			continue;

		hps_cpu_down(cpu);
		cpumask_clear_cpu(cpu, little_online_cpumask);
		--little_num_online;

		--val;
		if (val == 0 || little_num_online == little_num_base)
			break;
	}

	hps_ctxt.action |= BIT(ACTION_DOWN_LITTLE);
}

void hps_algo_smp(void)
{
	unsigned int cpu;
	unsigned int val;
	struct cpumask little_online_cpumask;
	unsigned int little_num_base, little_num_limit, little_num_online;
	/* log purpose */
	char str1[64];
	char str2[64];
	int i, j;
	char *str1_ptr = str1;
	char *str2_ptr = str2;

	/*
	 * run algo or not by hps_ctxt.enabled
	 */
	if (!hps_ctxt.enabled) {
		atomic_set(&hps_ctxt.is_ondemand, 0);
		return;
	}

	/*
	 * calculate cpu loading
	 */
	hps_ctxt.cur_loads = 0;
	str1_ptr = str1;
	str2_ptr = str2;

	for_each_possible_cpu(cpu) {
		per_cpu(hps_percpu_ctxt, cpu).load =
			hps_cpu_get_percpu_load(cpu);
		hps_ctxt.cur_loads += per_cpu(hps_percpu_ctxt, cpu).load;

		if (log_is_en(HPS_LOG_ALGO)) {
			if (cpu_online(cpu))
				i = sprintf(str1_ptr, "%4u", 1);
			else
				i = sprintf(str1_ptr, "%4u", 0);
			str1_ptr += i;
			j = sprintf(str2_ptr, "%4u",
				per_cpu(hps_percpu_ctxt, cpu).load);
			str2_ptr += j;
		}
	}
	hps_ctxt.cur_nr_heavy_task = hps_cpu_get_nr_heavy_task();
	hps_cpu_get_tlp(&hps_ctxt.cur_tlp, &hps_ctxt.cur_iowait);

	/*
	 * algo - begin
	 */
	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.action = ACTION_NONE;
	atomic_set(&hps_ctxt.is_ondemand, 0);

	/*
	 * algo - get boundary
	 */
	little_num_limit = num_limit_little_cpus();
	little_num_base = num_base_little_cpus();
	cpumask_and(&little_online_cpumask,
		&hps_ctxt.little_cpumask, cpu_online_mask);
	little_num_online = cpumask_weight(&little_online_cpumask);

	log_alog(" CPU:%s\n", str1);
	log_alog("LOAD:%s\n", str2);
	log_alog(
		"loads(%u), hvy_tsk(%u), tlp(%u), iowait(%u), limit_t(%u), limit_lb(%u), limit_ups(%u), limit_pos(%u), limit_c1(%u), limit_c2(%u), base_pes(%u), base_c1(%u), base_c2(%u)\n",
		hps_ctxt.cur_loads, hps_ctxt.cur_nr_heavy_task,
		hps_ctxt.cur_tlp, hps_ctxt.cur_iowait,
		hps_ctxt.little_num_limit_thermal,
		hps_ctxt.little_num_limit_low_battery,
		hps_ctxt.little_num_limit_ultra_power_saving,
		hps_ctxt.little_num_limit_power_serv,
		hps_ctxt.little_num_limit_custom1,
		hps_ctxt.little_num_limit_custom2,
		hps_ctxt.little_num_base_perf_serv,
		hps_ctxt.little_num_base_custom1,
		hps_ctxt.little_num_base_custom2);

	/*
	 * algo - thermal, low battery
	 */
	algo_smp_limit(&little_online_cpumask,
		little_num_base, little_num_limit, little_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * algo - PerfService, heavy task detect
	 */
	algo_smp_base(&little_online_cpumask,
		little_num_base, little_num_limit, little_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * update history - tlp
	 */
	val = hps_ctxt.tlp_history[hps_ctxt.tlp_history_index];
	hps_ctxt.tlp_history[hps_ctxt.tlp_history_index] = hps_ctxt.cur_tlp;
	hps_ctxt.tlp_sum += hps_ctxt.cur_tlp;
	hps_ctxt.tlp_history_index =
		(hps_ctxt.tlp_history_index + 1 == hps_ctxt.tlp_times) ?
			0 : hps_ctxt.tlp_history_index + 1;
	++hps_ctxt.tlp_count;
	if (hps_ctxt.tlp_count > hps_ctxt.tlp_times) {
		WARN_ON(hps_ctxt.tlp_sum < val);
		hps_ctxt.tlp_sum -= val;
		hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_times;
	} else
		hps_ctxt.tlp_avg = hps_ctxt.tlp_sum / hps_ctxt.tlp_count;
	hps_ctxt_print_algo_stats_tlp(0);

	/*
	 * algo - rush boost
	 */
	algo_smp_rush_boost(&little_online_cpumask,
		little_num_base, little_num_limit, little_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * algo - cpu up
	 */
	algo_smp_up(&little_online_cpumask,
		little_num_base, little_num_limit, little_num_online);

	if (hps_ctxt.action)
		goto ALGO_END_WITH_ACTION;

	/*
	 * algo - cpu down (inc. quick landing)
	 */
	algo_smp_down(&little_online_cpumask,
		little_num_base, little_num_limit, little_num_online);

	if (!hps_ctxt.action)
		goto ALGO_END_WO_ACTION;

	/*
	 * algo - end
	 */
ALGO_END_WITH_ACTION:
	log_act(
		"(%04x)(%u)action end(%u)(%u)(%u)(%u) (%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u)(%u) (%u)(%u)(%u) (%u)(%u)(%u) (%u)(%u)(%u)(%u)(%u)\n",
		hps_ctxt.action, little_num_online,
		hps_ctxt.cur_loads, hps_ctxt.cur_tlp, hps_ctxt.cur_iowait,
		hps_ctxt.cur_nr_heavy_task,
		hps_ctxt.little_num_limit_thermal,
		hps_ctxt.little_num_limit_low_battery,
		hps_ctxt.little_num_limit_ultra_power_saving,
		hps_ctxt.little_num_limit_power_serv,
		hps_ctxt.little_num_limit_custom1,
		hps_ctxt.little_num_limit_custom2,
		hps_ctxt.little_num_base_perf_serv,
		hps_ctxt.little_num_base_custom1,
		hps_ctxt.little_num_base_custom2,
		hps_ctxt.up_loads_sum, hps_ctxt.up_loads_count,
		hps_ctxt.up_loads_history_index,
		hps_ctxt.down_loads_sum, hps_ctxt.down_loads_count,
		hps_ctxt.down_loads_history_index,
		hps_ctxt.rush_count, hps_ctxt.tlp_sum, hps_ctxt.tlp_count,
		hps_ctxt.tlp_history_index, hps_ctxt.tlp_avg);
	hps_ctxt_reset_stas_nolock();
ALGO_END_WO_ACTION:
	mutex_unlock(&hps_ctxt.lock);
}
