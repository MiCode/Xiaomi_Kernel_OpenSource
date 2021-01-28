/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _PERF_TRACKER_H
#define _PERF_TRACKER_H

#include <linux/types.h>
#include <linux/module.h>

#ifdef CONFIG_MTK_PERF_TRACKER
/* cpufreq section */
extern int cluster_nr;
#ifdef CONFIG_MTK_CPU_FREQ
extern unsigned int mt_cpufreq_get_cur_freq(int id);
#else
static inline int mt_cpufreq_get_cur_freq(int id) { return 0; }
#endif
/* storage I/O section */
#ifdef CONFIG_MTK_BLOCK_TAG
#include <mt-plat/mtk_blocktag.h>
#else
struct mtk_btag_mictx_iostat_struct {
	__u64 duration;  /* duration time for below performance data (ns) */
	__u32 tp_req_r;  /* throughput (per-request): read  (KB/s) */
	__u32 tp_req_w;  /* throughput (per-request): write (KB/s) */
	__u32 tp_all_r;  /* throughput (overlapped) : read  (KB/s) */
	__u32 tp_all_w;  /* throughput (overlapped) : write (KB/s) */
	__u32 reqsize_r; /* request size : read  (Bytes) */
	__u32 reqsize_w; /* request size : write (Bytes) */
	__u32 reqcnt_r;  /* request count: read */
	__u32 reqcnt_w;  /* request count: write */
	__u16 wl;        /* storage device workload (%) */
	__u16 q_depth;   /* storage cmdq queue depth */
};
#endif
extern struct kobj_attribute perf_tracker_enable_attr;

extern void perf_tracker(u64 wallclock,
			long mm_available,
			long mm_free);
extern bool hit_long_check(void);
#else
static inline void perf_tracker(u64 wallclock,
				long mm_available,
				long mm_free) {}
#endif /* CONFIG_MTK_PERF_TRACKER */
#endif /* _PERF_TRACKER_H */
