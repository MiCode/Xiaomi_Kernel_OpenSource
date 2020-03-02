//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "../../drivers/misc/mediatek/base/power/include/mtk_upower.h"

static unsigned int get_next_freq(struct sugov_policy *sg_policy,
				  unsigned long util, unsigned long max)
{
	struct cpufreq_policy *policy = sg_policy->policy;
	int idx, target_idx = 0;
	int cap;
	int cpu = policy->cpu;
	struct upower_tbl *tbl;
	unsigned int freq = arch_scale_freq_invariant() ?
				policy->cpuinfo.max_freq : policy->cur;

	util = util + (util >> 2);

	tbl = upower_get_core_tbl(cpu);
	for (idx = 0; idx < tbl->row_num ; idx++) {
		cap = tbl->row[idx].cap;
		if (!cap)
			break;

		target_idx = idx;

		if (cap > util)
			break;
	}

	freq = mt_cpufreq_get_cpu_freq(cpu, target_idx);

	sg_policy->cached_raw_freq = freq;
	return cpufreq_driver_resolve_freq(policy, freq);
}
