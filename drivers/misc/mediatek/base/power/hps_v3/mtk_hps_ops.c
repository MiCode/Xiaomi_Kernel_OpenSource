// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include "mtk_hps.h"
#include "mtk_hps_internal.h"

/*
 *#include <mt-plat/met_drv.h>
 */

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
static int hps_algo_rush_boost(void)
{
	int val, base_val, ret;
	unsigned int idx, total_rel_load;

	idx = total_rel_load = 0;
	for (idx = 0 ; idx < hps_sys.cluster_num ; idx++)
		total_rel_load += hps_sys.cluster_info[idx].rel_load;

	if (!hps_ctxt.rush_boost_enabled)
		return 0;
	base_val = cal_base_cores();

	if (total_rel_load > hps_ctxt.rush_boost_threshold *
	hps_sys.total_online_cores)
		++hps_ctxt.rush_count;
	else
		hps_ctxt.rush_count = 0;
	if (hps_ctxt.rush_boost_times == 1)
		hps_ctxt.tlp_avg = hps_ctxt.cur_tlp;

	if ((hps_ctxt.rush_count >= hps_ctxt.rush_boost_times) &&
	    (hps_sys.total_online_cores * 100 < hps_ctxt.tlp_avg)) {
		val = hps_ctxt.tlp_avg / 100 +
			(hps_ctxt.tlp_avg % 100 ? 1 : 0);
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


		/* [MET] debug for geekbench */
		met_tag_oneshot(0, "sched_rush_boost", 1);

		ret = 1;
	} else {
		/* [MET] debug for geekbench */
		met_tag_oneshot(0, "sched_rush_boost", 0);
		ret = 0;
	}
	return ret;
}
static int hps_algo_eas(void)
{
	int val, ret, i;

	ret = 0;
	for (i = 0 ; i < hps_sys.cluster_num ; i++) {
		hps_sys.cluster_info[i].target_core_num =
			hps_sys.cluster_info[i].online_core_num;

		/*if up_threshold > loading > down_threshold ==> No action*/
		if ((hps_sys.cluster_info[i].loading <
		(hps_sys.cluster_info[i].up_threshold *
		hps_sys.cluster_info[i].online_core_num)) &&
		(hps_sys.cluster_info[i].loading >
		(hps_sys.cluster_info[i].down_threshold *
		hps_sys.cluster_info[i].online_core_num)))
			continue;

		/*if loading > up_threshod ==> power on cores*/
		if ((hps_sys.cluster_info[i].loading >
			(hps_sys.cluster_info[i].up_threshold *
			hps_sys.cluster_info[i].online_core_num))) {
			val = hps_sys.cluster_info[i].loading /
				hps_sys.cluster_info[i].up_threshold;
			if (hps_sys.cluster_info[i].loading %
			hps_sys.cluster_info[i].up_threshold)
				val++;
			if (val <= hps_sys.cluster_info[i].limit_value)
				hps_sys.cluster_info[i].target_core_num = val;
			else
				hps_sys.cluster_info[i].target_core_num =
					hps_sys.cluster_info[i].limit_value;
			ret = 1;
		} else if ((hps_sys.cluster_info[i].loading <
			(hps_sys.cluster_info[i].down_threshold *
			hps_sys.cluster_info[i].online_core_num))) {
		/*if loading < down_threshod ==> power off cores*/
			if (!hps_sys.cluster_info[i].loading) {
				hps_sys.cluster_info[i].target_core_num = 0;
				continue;
			}
			val = hps_sys.cluster_info[i].loading /
				hps_sys.cluster_info[i].down_threshold;
			if (hps_sys.cluster_info[i].loading %
				hps_sys.cluster_info[i].down_threshold)
				val++;
			if (val >= hps_sys.cluster_info[i].base_value)
				hps_sys.cluster_info[i].target_core_num = val;
			else
				hps_sys.cluster_info[i].target_core_num =
					hps_sys.cluster_info[i].base_value;
			ret = 1;
		}
	}

	return ret;
}
static int hps_algo_perf_indicator(void)
{
	unsigned int i;
	/* for ondemand request */
	if (atomic_read(&hps_ctxt.is_ondemand) != 0) {
		atomic_set(&hps_ctxt.is_ondemand, 0);

		mutex_lock(&hps_ctxt.para_lock);
		for (i = 0; i < hps_sys.cluster_num; i++)
			hps_sys.cluster_info[i].target_core_num =
				max(hps_sys.cluster_info[i].base_value,
				hps_sys.cluster_info[i].online_core_num);

		mutex_unlock(&hps_ctxt.para_lock);

		return 1;
	}
	return 0;
}

/* Notice : Sorting function pointer by priority */
static int (*hps_func[]) (void) = {
/*hps_algo_perf_indicator, hps_algo_rush_boost, */
/*hps_algo_eas, hps_algo_up, hps_algo_down};*/
hps_algo_perf_indicator, hps_algo_rush_boost, hps_algo_eas};
int hps_ops_init(void)
{
	int i;

	hps_sys.func_num = 0;
	hps_sys.func_num = ARRAY_SIZE(hps_func);
	hps_sys.hps_sys_ops = kcalloc(hps_sys.func_num,
		sizeof(*hps_sys.hps_sys_ops), GFP_KERNEL);
	if (!hps_sys.hps_sys_ops)
		return -ENOMEM;

	for (i = 0; i < hps_sys.func_num; i++) {
		hps_sys.hps_sys_ops[i].func_id = 0xF00 + i;
		hps_sys.hps_sys_ops[i].enabled = 1;
		hps_sys.hps_sys_ops[i].hps_sys_func_ptr = *hps_func[i];
		hps_warn("%d: func_id %d, enabled %d\n", i,
			hps_sys.hps_sys_ops[i].func_id,
			hps_sys.hps_sys_ops[i].enabled);
	}

	return 0;
}
