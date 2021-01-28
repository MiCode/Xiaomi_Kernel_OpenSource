/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_L,
	MT_CPU_DVFS_B,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

enum mt_dvfs_debug_id {
	DEBUG_FREQ_CLUSTER0,
	DEBUG_FREQ_CLUSTER1,
	DEBUG_FREQ_CLUSTER2,
	DEBUG_FREQ_ALL,
	DEBUG_FREQ_DISABLED = 100,
};

enum throttle_type {
	DVFS_THROTTLE_UP,
	DVFS_THROTTLE_DOWN,
};

#ifdef CONFIG_MTK_CPU_FREQ
#include "../../drivers/misc/mediatek/include/mt-plat/mtk_cpufreq_common_api.h"
#else
static inline int  mt_cpufreq_set_by_schedule_load_cluster(
			int cid, unsigned int freq) { return 0; }
static inline int  mt_cpufreq_set_by_wfi_load_cluster(
			int cid, unsigned int freq) { return 0; }
static inline unsigned int mt_cpufreq_find_close_freq(
		unsigned int cluster_id, unsigned int freq) { return 0; }
static inline unsigned int mt_cpufreq_get_freq_by_idx(
			int cid, int idx) { return 0; }
static inline int mt_cpufreq_get_sched_enable(void) { return 0; }
static inline  int mt_cpufreq_get_cur_freq(
			enum mt_cpu_dvfs_id id) { return 0; };
#endif
