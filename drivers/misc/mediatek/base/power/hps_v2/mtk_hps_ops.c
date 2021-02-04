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
#include <linux/slab.h>

#include "mtk_hps.h"
#include "mtk_hps_internal.h"

static int cal_base_cores(void)
{
	int i, base_val;

	i = base_val = 0;
	mutex_lock(&hps_ctxt.para_lock);
	for (i = 0; i < hps_sys.cluster_num; i++)
		base_val += hps_sys.cluster_info[i].base_value;

	mutex_unlock(&hps_ctxt.para_lock);
	return base_val;
}

#if 0
static int hps_cal_cores(void)
{
	int rush_boost_cores, sys_cores;

	rush_boost_cores = 0;
	if ((hps_ctxt.cur_loads > hps_ctxt.rush_boost_threshold * hps_sys.total_online_cores) &&
		(hps_sys.total_online_cores * 100 < hps_ctxt.cur_tlp)) {
		rush_boost_cores = hps_ctxt.cur_tlp / 100 + (hps_ctxt.cur_tlp % 100 ? 1 : 0);
	}
	WARN_ON(hps_ctxt.up_threshold == 0);
	sys_cores = hps_ctxt.cur_loads / hps_ctxt.up_threshold + (hps_ctxt.cur_loads % hps_ctxt.up_threshold ? 1 : 0);
	return max(sys_cores, rush_boost_cores);
}
#endif

static int hps_algo_rush_boost(void)
{
	int val, base_val;

	if (!hps_ctxt.rush_boost_enabled)
		return 0;
	base_val = cal_base_cores();

	if (hps_ctxt.cur_loads > hps_ctxt.rush_boost_threshold * hps_sys.total_online_cores)
		++hps_ctxt.rush_count;
	else
		hps_ctxt.rush_count = 0;
	if (hps_ctxt.rush_boost_times == 1)
		hps_ctxt.tlp_avg = hps_ctxt.cur_tlp;

	if ((hps_ctxt.rush_count >= hps_ctxt.rush_boost_times) &&
	    (hps_sys.total_online_cores * 100 < hps_ctxt.tlp_avg)) {
		val = hps_ctxt.tlp_avg / 100 + (hps_ctxt.tlp_avg % 100 ? 1 : 0);
		WARN_ON(!(val > hps_sys.total_online_cores));
		if (val > num_possible_cpus())
			val = num_possible_cpus();
		if (val > base_val)
			val -= base_val;
		else
			val = 0;
		hps_sys.tlp_avg = hps_ctxt.tlp_avg;
		hps_sys.rush_cnt = hps_ctxt.rush_count;
		hps_cal_core_num(&hps_sys, val, base_val);

		return 1;
	} else
		return 0;
}

/*
 * update history - up
 */
static int hps_algo_up(void)
{
	int val, base_val;

	base_val = cal_base_cores();

	if (hps_sys.total_online_cores < num_possible_cpus()) {
		val = hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index];
		hps_ctxt.up_loads_history[hps_ctxt.up_loads_history_index] = hps_ctxt.cur_loads;
		hps_ctxt.up_loads_sum += hps_ctxt.cur_loads;
		hps_ctxt.up_loads_history_index =
		    (hps_ctxt.up_loads_history_index + 1 ==
		     hps_ctxt.up_times) ? 0 : hps_ctxt.up_loads_history_index + 1;
		++hps_ctxt.up_loads_count;
		/* XXX: use >= or >, which is benifit? use > */
		if (hps_ctxt.up_loads_count > hps_ctxt.up_times) {
			WARN_ON(hps_ctxt.up_loads_sum < val);
			hps_ctxt.up_loads_sum -= val;
		}

		if (hps_ctxt.up_times == 1)
			hps_ctxt.up_loads_sum = hps_ctxt.cur_loads;
		if (hps_ctxt.up_loads_count >= hps_ctxt.up_times) {
			if (hps_ctxt.up_loads_sum >
			    hps_ctxt.up_threshold * hps_ctxt.up_times *
			    hps_sys.total_online_cores) {
				val = hps_sys.total_online_cores + 1;
				if (val > base_val)
					val -= base_val;
				else
					val = 0;
				hps_sys.up_load_avg = hps_ctxt.up_loads_sum / hps_ctxt.up_times;
				hps_cal_core_num(&hps_sys, val, base_val);

				return 1;
			}
		}		/* if (hps_ctxt.up_loads_count >= hps_ctxt.up_times) */
	}
	return 0;
}

/*
 * algo - cpu down (inc. quick landing)
 */
static int hps_algo_down(void)
{
	int val, base_val;

	base_val = cal_base_cores();
	if (hps_sys.total_online_cores > 1) {
		val = hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index];
		hps_ctxt.down_loads_history[hps_ctxt.down_loads_history_index] = hps_ctxt.cur_loads;
		hps_ctxt.down_loads_sum += hps_ctxt.cur_loads;
		hps_ctxt.down_loads_history_index =
		    (hps_ctxt.down_loads_history_index + 1 ==
		     hps_ctxt.down_times) ? 0 : hps_ctxt.down_loads_history_index + 1;
		++hps_ctxt.down_loads_count;
		/* XXX: use >= or >, which is benifit? use > */
		if (hps_ctxt.down_times == 1)
			val = hps_ctxt.down_loads_sum = hps_ctxt.cur_loads;
		if (hps_ctxt.down_loads_count > hps_ctxt.down_times) {
			if (hps_ctxt.down_times > 1) {
				WARN_ON(hps_ctxt.down_loads_sum < val);
				hps_ctxt.down_loads_sum -= val;
			}
		}
		if (hps_ctxt.stats_dump_enabled)
			hps_ctxt_print_algo_stats_down(0);
		if (hps_ctxt.down_loads_count >= hps_ctxt.down_times) {
			unsigned int down_threshold = hps_ctxt.down_threshold * hps_ctxt.down_times;

			val = hps_sys.total_online_cores;

			while (hps_ctxt.down_loads_sum < down_threshold * (val - 1))
				--val;
			WARN_ON(val < 0);
			if (val > base_val)
				val -= base_val;
			else
				val = 0;
			hps_sys.down_load_avg = hps_ctxt.down_loads_sum / hps_ctxt.down_times;
			hps_cal_core_num(&hps_sys, val, base_val);

			return 1;
		}		/* if (hps_ctxt.down_loads_count >= hps_ctxt.down_times) */
	}
	return 0;
}

#if 0
int hps_algo_heavytsk_det(void)
{
	int i, j, ret, sys_cores, hvy_cores;

	i = j = ret = sys_cores = hvy_cores = 0;
	/*sys_cores = hps_cal_cores(); */
	/* Check heavy task value of last cluster if needed */
#if 0
	if (hps_sys.cluster_info[hps_sys.cluster_num - 1].hvyTsk_value)
		ret = 1;
#endif

	for (i = hps_sys.cluster_num - 1; i > 0; i--) {
		if (hps_sys.cluster_info[i - 1].hvyTsk_value)
			ret = 1;
		hvy_cores += hps_sys.cluster_info[i].hvyTsk_value;

		if (i == hps_sys.cluster_num - 1)
			hps_sys.cluster_info[i].target_core_num =
			    max3(hps_sys.cluster_info[i].online_core_num,
				 hps_sys.cluster_info[i - 1].hvyTsk_value,
				 hps_sys.cluster_info[i].hvyTsk_value);
		else
			hps_sys.cluster_info[i].target_core_num =
			    max(hps_sys.cluster_info[i].online_core_num,
				hps_sys.cluster_info[i - 1].hvyTsk_value);
		if (hps_sys.cluster_info[i].target_core_num > hps_sys.cluster_info[i].limit_value)
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
#endif
	return ret;
}
#endif
static int hps_algo_perf_indicator(void)
{
	unsigned int i;

	if (atomic_read(&hps_ctxt.is_ondemand) != 0) { /* for ondemand request */
		atomic_set(&hps_ctxt.is_ondemand, 0);

		mutex_lock(&hps_ctxt.para_lock);
		for (i = 0; i < hps_sys.cluster_num; i++)
			hps_sys.cluster_info[i].target_core_num =
				max(hps_sys.cluster_info[i].base_value, hps_sys.cluster_info[i].online_core_num);

		mutex_unlock(&hps_ctxt.para_lock);

		return 1;
	}
	return 0;
}

/* Notice : Sorting function pointer by priority */
static int (*hps_func[]) (void) = {
hps_algo_rush_boost, hps_algo_perf_indicator, hps_algo_up, hps_algo_down};

int hps_ops_init(void)
{
	int i;

	hps_sys.func_num = 0;
	hps_sys.func_num = ARRAY_SIZE(hps_func);
	hps_sys.hps_sys_ops =
	    kcalloc(hps_sys.func_num, sizeof(*hps_sys.hps_sys_ops), GFP_KERNEL);
	if (!hps_sys.hps_sys_ops) {
		tag_pr_notice("@%s: fail to allocate memory for hps_sys_ops!\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < hps_sys.func_num; i++) {
		hps_sys.hps_sys_ops[i].func_id = 0xF00 + i;
		hps_sys.hps_sys_ops[i].enabled = 1;
		hps_sys.hps_sys_ops[i].hps_sys_func_ptr = *hps_func[i];
		tag_pr_info("%d: func_id %d, enabled %d\n", i, hps_sys.hps_sys_ops[i].func_id,
			    hps_sys.hps_sys_ops[i].enabled);
	}

	return 0;
}
