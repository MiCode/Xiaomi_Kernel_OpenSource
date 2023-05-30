// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/sched/clock.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include "qti_bcl_stats.h"

static struct dentry	*bcl_stats_parent;
static struct dentry	*bcl_dev_parent;

void bcl_update_clear_stats(struct bcl_lvl_stats *bcl_stat)
{
	uint32_t iter = 0;
	unsigned long long last_duration;
	struct bcl_data_history *hist_data;

	if (!bcl_stat->trigger_state)
		return;

	iter = (bcl_stat->counter % BCL_HISTORY_COUNT);
	hist_data = &bcl_stat->bcl_history[iter];
	hist_data->clear_ts = sched_clock();
	last_duration = DIV_ROUND_UP(
			hist_data->clear_ts - hist_data->trigger_ts,
			NSEC_PER_USEC);
	bcl_stat->total_duration += last_duration;
	if (last_duration > bcl_stat->max_duration)
		bcl_stat->max_duration = last_duration;

	bcl_stat->counter++;
	bcl_stat->trigger_state = false;
}

void bcl_update_trigger_stats(struct bcl_lvl_stats *bcl_stat, int ibat, int vbat)
{
	uint32_t iter = 0;

	iter = (bcl_stat->counter % BCL_HISTORY_COUNT);
	bcl_stat->bcl_history[iter].clear_ts = 0x0;
	bcl_stat->bcl_history[iter].trigger_ts = sched_clock();
	bcl_stat->bcl_history[iter].ibat = ibat;
	bcl_stat->bcl_history[iter].vbat = vbat;
	bcl_stat->trigger_state = true;
}

static int bcl_lvl_show(struct seq_file *s, void *data)
{
	struct bcl_lvl_stats *bcl_stat = s->private;
	unsigned long long last_duration = 0;
	int idx = 0, cur_counter = 0, loop_till = 0;

	seq_printf(s, "%-30s: %d\n",
					"Counter", bcl_stat->counter);
	seq_printf(s, "%-30s: %d\n",
					"Irq self cleared counter",
					bcl_stat->self_cleared_counter);
	seq_printf(s, "%-30s: %lu\n",
					"Max Mitigation at", bcl_stat->max_mitig_ts);
	seq_printf(s, "%-30s: %lu usec\n",
					"Max Mitigation latency",
					DIV_ROUND_UP(bcl_stat->max_mitig_latency,
						NSEC_PER_USEC));
	seq_printf(s, "%-30s: %lu usec\n",
					"Max duration", bcl_stat->max_duration);
	seq_printf(s, "%-30s: %lu usec\n",
					"Total Duration",	bcl_stat->total_duration);
	seq_printf(s, "Last %d iterations	:\n", BCL_HISTORY_COUNT);
	seq_printf(s, "%s%10s%10s%15s%15s%16s\n", "idx", "ibat", "vbat",
				"trigger_ts", "clear_ts", "duration(usec)");

	cur_counter = (bcl_stat->counter % BCL_HISTORY_COUNT);
	idx = cur_counter - 1;
	loop_till = -1;
	/* print history data as stack. latest entry first */
	do {
		last_duration = 0;
		if (idx < 0) {
			idx = BCL_HISTORY_COUNT - 1;
			loop_till = cur_counter;
			continue;
		}
		if (bcl_stat->bcl_history[idx].clear_ts)
			last_duration = DIV_ROUND_UP(
					bcl_stat->bcl_history[idx].clear_ts -
					bcl_stat->bcl_history[idx].trigger_ts,
					NSEC_PER_USEC);
		seq_printf(s, "[%d]%10d%10d%15lu%15lu%16lu\n", idx,
				bcl_stat->bcl_history[idx].ibat,
				bcl_stat->bcl_history[idx].vbat,
				bcl_stat->bcl_history[idx].trigger_ts,
				bcl_stat->bcl_history[idx].clear_ts,
				last_duration);
		--idx;
	} while (idx > loop_till);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(bcl_lvl);

void bcl_stats_init(char *bcl_name, struct bcl_lvl_stats *bcl_stats, uint32_t stats_len)
{
	int idx = 0;
	char stats_name[BCL_STATS_NAME_LENGTH];

	bcl_stats_parent = debugfs_lookup("bcl_stats", NULL);
	if (bcl_stats_parent == NULL)
		bcl_stats_parent = debugfs_create_dir("bcl_stats", NULL);

	bcl_dev_parent = debugfs_create_dir(bcl_name, bcl_stats_parent);
	for (idx = 0; idx < stats_len; idx++) {
		snprintf(stats_name, BCL_STATS_NAME_LENGTH, "lvl%d_stats", idx);
		debugfs_create_file(stats_name, 0444, bcl_dev_parent,
				&bcl_stats[idx], &bcl_lvl_fops);
	}
}
