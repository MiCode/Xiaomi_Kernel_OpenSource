/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __DDR_STATS_H__
#define __DDR_STATS_H__

struct ddr_freq_residency {
	uint32_t freq;
	uint64_t residency;
};

struct ddr_stats_ss_vote_info {
	u32 ab; /* vote_x */
	u32 ib; /* vote_y */
};

int ddr_stats_get_freq_count(void);
int ddr_stats_get_residency(int freq_count, struct ddr_freq_residency *data);
int ddr_stats_get_ss_count(void);
int ddr_stats_get_ss_vote_info(int ss_count,
			       struct ddr_stats_ss_vote_info *vote_info);
#endif /*__DDR_STATS_H_ */
