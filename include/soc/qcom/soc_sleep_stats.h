/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __SOC_SLEEP_STATS_H__
#define __SOC_SLEEP_STATS_H__

struct ddr_stats_ss_vote_info {
	u32 ab; /* vote_x */
	u32 ib; /* vote_y */
};
#if IS_ENABLED(CONFIG_QCOM_SOC_SLEEP_STATS)

int ddr_stats_get_ss_count(void);
int ddr_stats_get_ss_vote_info(int ss_count,
			       struct ddr_stats_ss_vote_info *vote_info);
#else

int ddr_stats_get_ss_count(void)
{return -ENODEV; }
int ddr_stats_get_ss_vote_info(int ss_count,
			       struct ddr_stats_ss_vote_info *vote_info)
{ return -ENODEV; }

#endif
#endif /*__SOC_SLEEP_STATS_H__ */
