/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_RPM_STATS_LOG_H__
#define __QCOM_RPM_STATS_LOG_H__

struct msm_rpmh_master_stats {
	uint32_t version_id;
	uint32_t counts;
	uint64_t last_entered;
	uint64_t last_exited;
	uint64_t accumulated_duration;
};
#if IS_ENABLED(CONFIG_QTI_RPM_STATS_LOG)

void msm_rpmh_master_stats_update(void);
struct msm_rpmh_master_stats *msm_rpmh_get_apss_data(void);
#else

static inline void msm_rpmh_master_stats_update(void) {}
static inline struct msm_rpmh_master_stats *msm_rpmh_get_apss_data(void)
{
	return NULL;
}

#endif

#endif /* __QCOM_RPM_STATS_LOG_H__ */
