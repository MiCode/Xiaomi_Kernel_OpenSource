/*
 *  drivers/cpufreq/cpufreq_stats.c
 *
 *  Copyright (C) 2003-2004 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *  Copyright (C) 2019 XiaoMi, Inc.
 *  (C) 2004 Zou Nan hai <nanhai.zou@intel.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "cpufreq_stats.h"

static int cpufreq_stats_update(struct cpufreq_stats *stats)
{
	unsigned long long cur_time = get_jiffies_64();

	spin_lock(&stats->cpufreq_stats_lock);
	stats->time_in_state[stats->last_index] += cur_time - stats->last_time;
	stats->last_time = cur_time;
	spin_unlock(&stats->cpufreq_stats_lock);
	return 0;
}

static ssize_t show_total_trans(struct cpufreq_policy *policy, char *buf)
{
	return sprintf(buf, "%d\n", policy->stats->total_trans);
}

static ssize_t show_gov_total_trans(struct cpufreq_policy *policy, char *buf)
{
	return snprintf(buf, 20, "%d\n", policy->gov_stats->total_trans);
}

static ssize_t __show_time_in_state(struct cpufreq_policy *policy,
		   struct cpufreq_stats **pstats, char *buf)
{
	struct cpufreq_stats *stats = *pstats;
	ssize_t len = 0;
	int i;

	if (policy->fast_switch_enabled)
		return 0;

	cpufreq_stats_update(stats);
	for (i = 0; i < stats->state_num; i++) {
		len += sprintf(buf + len, "%u %llu\n", stats->freq_table[i],
			(unsigned long long)
			jiffies_64_to_clock_t(stats->time_in_state[i]));
	}
	return len;
}

static ssize_t show_time_in_state(struct cpufreq_policy *policy, char *buf)
{
	return __show_time_in_state(policy, &policy->stats, buf);
}

static ssize_t show_gov_time_in_state(struct cpufreq_policy *policy, char *buf)
{
	return __show_time_in_state(policy, &policy->gov_stats, buf);
}

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
static ssize_t __show_trans_table(struct cpufreq_policy *policy,
		   struct cpufreq_stats **pstats, char *buf)
{
	struct cpufreq_stats *stats = *pstats;
	ssize_t len = 0;
	int i, j;

	if (policy->fast_switch_enabled)
		return 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "   From  :    To\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "         : ");
	for (i = 0; i < stats->state_num; i++) {
		if (len >= PAGE_SIZE)
			break;
		len += snprintf(buf + len, PAGE_SIZE - len, "%9u ",
				stats->freq_table[i]);
	}
	if (len >= PAGE_SIZE)
		return PAGE_SIZE;

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	for (i = 0; i < stats->state_num; i++) {
		if (len >= PAGE_SIZE)
			break;

		len += snprintf(buf + len, PAGE_SIZE - len, "%9u: ",
				stats->freq_table[i]);

		for (j = 0; j < stats->state_num; j++) {
			if (len >= PAGE_SIZE)
				break;
			len += snprintf(buf + len, PAGE_SIZE - len, "%9u ",
					stats->trans_table[i*stats->max_state+j]);
		}
		if (len >= PAGE_SIZE)
			break;
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}
	if (len >= PAGE_SIZE)
		return PAGE_SIZE;
	return len;
}

static ssize_t show_trans_table(struct cpufreq_policy *policy, char *buf)
{
	return __show_trans_table(policy, &policy->stats, buf);
}

cpufreq_freq_attr_ro(trans_table);

static ssize_t show_gov_trans_table(struct cpufreq_policy *policy, char *buf)
{
	return __show_trans_table(policy, &policy->gov_stats, buf);
}

cpufreq_freq_attr_ro(gov_trans_table);
#endif

cpufreq_freq_attr_ro(total_trans);
cpufreq_freq_attr_ro(time_in_state);

static struct attribute *stats_attrs[] = {
	&total_trans.attr,
	&time_in_state.attr,
#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	&trans_table.attr,
#endif
	NULL
};

cpufreq_freq_attr_ro(gov_total_trans);
cpufreq_freq_attr_ro(gov_time_in_state);
static struct attribute *gov_stats_attrs[] = {
	&gov_total_trans.attr,
	&gov_time_in_state.attr,
#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	&gov_trans_table.attr,
#endif
	NULL
};

static struct attribute_group stats_attr_group[2] = {
	{
	.attrs = stats_attrs,
	.name = "stats"
	},
	{
	.attrs = gov_stats_attrs,
	.name = "gov_stats"
	},
};

static int freq_table_get_index(struct cpufreq_stats *stats, unsigned int freq)
{
	int index;
	for (index = 0; index < stats->max_state; index++)
		if (stats->freq_table[index] == freq)
			return index;
	return -1;
}

static int freq_table_match_index(struct cpufreq_policy *policy, unsigned int freq)
{
	freq = clamp_val(freq, policy->cpuinfo.min_freq, policy->cpuinfo.max_freq);
	if (policy->freq_table_sorted == CPUFREQ_TABLE_SORTED_ASCENDING)
		return cpufreq_table_find_index_al(policy, freq);
	else
		return cpufreq_table_find_index_dl(policy, freq);
}


static void __cpufreq_stats_free_table(struct cpufreq_policy *policy,
		   struct cpufreq_stats **pstats)
{
	struct cpufreq_stats *stats = *pstats;

	/* Already freed */
	if (!stats)
		return;

	pr_debug("%s: Free stats table\n", __func__);

	sysfs_remove_group(&policy->kobj, (*pstats)->stats_attr_group);
	kfree(stats->time_in_state);
	kfree(stats);
	*pstats = NULL;
}

void cpufreq_stats_free_table(struct cpufreq_policy *policy)
{
	__cpufreq_stats_free_table(policy, &policy->stats);
}

void cpufreq_gov_stats_free_table(struct cpufreq_policy *policy)
{
	__cpufreq_stats_free_table(policy, &policy->gov_stats);
}

static void __cpufreq_stats_create_table(struct cpufreq_policy *policy,
		   struct cpufreq_stats **pstats, unsigned int id)
{
	unsigned int i = 0, count = 0, ret = -ENOMEM;
	struct cpufreq_stats *stats;
	unsigned int alloc_size;
	struct cpufreq_frequency_table *pos, *table;

	/* We need cpufreq table for creating stats table */
	table = policy->freq_table;
	if (unlikely(!table))
		return;

	/* stats already initialized */
	if (*pstats)
		return;

	stats = kzalloc(sizeof(*stats), GFP_KERNEL);
	if (!stats)
		return;

	spin_lock_init(&stats->cpufreq_stats_lock);

	/* Find total allocation size */
	cpufreq_for_each_valid_entry(pos, table)
		count++;

	alloc_size = count * sizeof(int) + count * sizeof(u64);

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	alloc_size += count * count * sizeof(int);
#endif

	/* Allocate memory for time_in_state/freq_table/trans_table in one go */
	stats->time_in_state = kzalloc(alloc_size, GFP_KERNEL);
	if (!stats->time_in_state)
		goto free_stat;

	stats->freq_table = (unsigned int *)(stats->time_in_state + count);

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	stats->trans_table = stats->freq_table + count;
#endif

	stats->max_state = count;

	/* Find valid-unique entries */
	cpufreq_for_each_valid_entry(pos, table)
		if (freq_table_get_index(stats, pos->frequency) == -1)
			stats->freq_table[i++] = pos->frequency;

	stats->state_num = i;
	stats->last_time = get_jiffies_64();
	stats->last_index = freq_table_get_index(stats, policy->cur);

	*pstats = stats;

	if (id < 2) {
		ret = sysfs_create_group(&policy->kobj, &stats_attr_group[id]);
		if (!ret)
			return;
	}

	/* We failed, release resources */
	*pstats = NULL;
	kfree(stats->time_in_state);
free_stat:
	kfree(stats);
}

void cpufreq_stats_create_table(struct cpufreq_policy *policy)
{
	__cpufreq_stats_create_table(policy, &policy->stats, 0);

	if (policy->stats)
		policy->stats->stats_attr_group = &stats_attr_group[0];
}

void cpufreq_gov_stats_create_table(struct cpufreq_policy *policy)
{
	__cpufreq_stats_create_table(policy, &policy->gov_stats, 1);

	if (policy->gov_stats)
		policy->gov_stats->stats_attr_group = &stats_attr_group[1];
}


static void __cpufreq_stats_record_transition(struct cpufreq_stats *stats,
				     unsigned int new_freq)
{
	int old_index, new_index;

	if (!stats) {
		pr_debug("%s: No stats found\n", __func__);
		return;
	}

	old_index = stats->last_index;
	new_freq = min(new_freq, stats->freq_table[stats->state_num - 1]);
	new_freq = max(new_freq, stats->freq_table[0]);

	new_index = freq_table_get_index(stats, new_freq);
	/* We can't do stats->time_in_state[-1]= .. */
	if (old_index == -1 || new_index == -1 || old_index == new_index)
		return;

	cpufreq_stats_update(stats);

	stats->last_index = new_index;
#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	stats->trans_table[old_index * stats->max_state + new_index]++;
#endif
	stats->total_trans++;
}

void cpufreq_stats_record_transition(struct cpufreq_policy *policy,
				     unsigned int new_freq)
{
	__cpufreq_stats_record_transition(policy->stats, new_freq);
}

void cpufreq_gov_stats_record_transition(struct cpufreq_policy *policy,
				     unsigned int new_freq)
{
	int index = freq_table_match_index(policy, new_freq);

	new_freq = policy->freq_table[index].frequency;
	__cpufreq_stats_record_transition(policy->gov_stats, new_freq);
}
