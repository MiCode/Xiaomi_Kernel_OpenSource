/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/thermal.h>
#include <linux/sched.h>

#ifndef __QCOM_BCL_STATS_H__
#define __QCOM_BCL_STATS_H__

#define BCL_HISTORY_COUNT     10
#define BCL_STATS_NAME_LENGTH 30

struct bcl_data_history {
	uint32_t vbat;
	uint32_t ibat;
	unsigned long long trigger_ts;
	unsigned long long clear_ts;
};

struct bcl_lvl_stats {
	uint32_t counter;
	uint32_t self_cleared_counter;
	bool trigger_state;
	unsigned long long max_mitig_ts;
	unsigned long long max_mitig_latency;
	unsigned long long max_duration;
	unsigned long long total_duration;
	struct bcl_data_history bcl_history[BCL_HISTORY_COUNT];
};

void bcl_stats_init(char *bcl_name, struct bcl_lvl_stats *bcl_stats,
		    uint32_t stats_len);

void bcl_update_clear_stats(struct bcl_lvl_stats *bcl_stat);

void bcl_update_trigger_stats(struct bcl_lvl_stats *bcl_stat,
			      int ibat, int vbat);

#endif /* __QCOM_BCL_STATS_H__ */
