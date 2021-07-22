// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/sort.h>
#include <linux/cpufreq.h>

#include "fpsgo_cpu_policy.h"

static int policy_num;
static int *opp_count;
static unsigned int **opp_table;

// --------------------------------------------------
static int cmp_uint(const void *a, const void *b)
{
	return *(unsigned int *)b - *(unsigned int *)a;
}

void fpsgo_cpu_policy_init(void)
{
	int cpu;
	int num = 0, count;
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos;

	/* query policy number */
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
				__func__, num, cpu, policy->min, policy->max);

			num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}

	policy_num = num;

	if (policy_num == 0) {
		pr_info("%s, no policy", __func__);
		return;
	}

	opp_count = kcalloc(policy_num, sizeof(int), GFP_KERNEL);
	opp_table = kcalloc(policy_num, sizeof(unsigned int *), GFP_KERNEL);

	num = 0;
	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		/* calc opp count */
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			count++;
		}
		opp_count[num] = count;
		opp_table[num] = kcalloc(count, sizeof(unsigned int), GFP_KERNEL);
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			opp_table[num][count] = pos->frequency;
			count++;
		}

		sort(opp_table[num], opp_count[num], sizeof(unsigned int), cmp_uint, NULL);

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

}

int fpsgo_get_cpu_policy_num(void)
{
	return policy_num;
}

int fpsgo_get_cpu_opp_info(int **opp_cnt, unsigned int ***opp_tbl)
{
	int i, j;

	if (policy_num <= 0)
		return -EFAULT;

	*opp_cnt = kcalloc(policy_num, sizeof(int), GFP_KERNEL);
	*opp_tbl = kcalloc(policy_num, sizeof(unsigned int *), GFP_KERNEL);

	if (*opp_cnt == NULL || *opp_tbl == NULL)
		return -1;

	for (i = 0; i < policy_num; i++) {

		(*opp_cnt)[i] = opp_count[i];
		(*opp_tbl)[i] = kcalloc(opp_count[i], sizeof(unsigned int), GFP_KERNEL);

		for (j = 0; j < opp_count[i]; j++)
			(*opp_tbl)[i][j] = opp_table[i][j];

	}

	return 0;
}


