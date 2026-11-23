/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Oplus. All rights reserved.
 */

#ifndef __OPLUS_CPU_JANK_BASE_H__
#define __OPLUS_CPU_JANK_BASE_H__

#include <linux/cgroup.h>
#include <linux/string.h>
#include <linux/kernel_stat.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/cpufreq.h>
#include <linux/processor.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/cpumask.h>
#include <linux/kallsyms.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/trace_events.h>
#include <linux/compat.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/futex.h>

#if IS_ENABLED(CONFIG_SCHED_WALT)
////#include "../../../kernel/sched/walt/walt.h"
#else
#include <linux/sched/cputime.h>
#include <kernel/sched/sched.h>
#include <fs/proc/internal.h>
#endif
#include <linux/mi_sched.h>

#include <trace/hooks/sched.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/futex.h>
#include <trace/hooks/cpufreq.h>
#include <trace/events/task.h>
#include <trace/events/sched.h>

#define REGISTER_TRACE_VH(vender_hook, handler) { \
		ret = register_trace_##vender_hook(handler, NULL); \
		if (ret) { \
			pr_err("failed to register_trace_"#vender_hook", ret=%d\n", ret); \
			return ret; \
		} \
	}

#define UNREGISTER_TRACE_VH(vender_hook, handler) { \
		unregister_trace_##vender_hook(handler, NULL); \
	}

#define REGISTER_TRACE_RVH		REGISTER_TRACE_VH

#ifdef VENDOR_DEBUG
#define UNREGISTER_TRACE_RVH	UNREGISTER_TRACE_VH
#else
#define UNREGISTER_TRACE_RVH(vender_hook, handler)
#endif

#define osi_debug(fmt, args...)					\
do {								            \
	if (g_osi_debug >= 1) {	                    \
		 printk(KERN_INFO "[OSI_DEBUG]"fmt, ##args);			\
	}							                \
} while (0)

#define osi_err(fmt, args...)					\
do {								            \
	if (g_osi_debug >= 1) {	                    \
		 printk(KERN_ERR "[OSI_ERR]"fmt, ##args);			\
	}							                \
} while (0)

#define osi_debug_deferred(fmt, args...)					\
do {								            \
	if (g_osi_debug >= 1) {	                    \
		 printk_deferred(KERN_INFO "[OSI_DEBUG]"fmt, ##args);			\
	}							                \
} while (0)

#define osi_err_deferred(fmt, args...)					\
do {								            \
	if (g_osi_debug >= 1) {	                    \
		 printk_deferred(KERN_ERR "[OSI_ERR]"fmt, ##args);			\
	}							                \
} while (0)

extern int g_osi_debug;



#if IS_ENABLED(CONFIG_CGROUP_SCHED)
#define SA_CGROUP_SYS_BACKGROUND	(1)
#define SA_CGROUP_FOREGROUND		(2)
#define SA_CGROUP_BACKGROUND		(3)
#define SA_CGROUP_TOP_APP			(4)
#define SA_CGROUP_UX				(9)
#endif

#define OHM_LATENCY_DIST_MAX 13

enum {
	OHM_SCHED_IOWAIT = 0,
	OHM_SCHED_DSTATE,
	OHM_SCHED_TOTAL,
};

struct sched_stat_common {
	u64 max_ms;
	u64 high_cnt;
	u64 low_cnt;
	u64 min_cnt;
	u64 total_ms;
	u64 total_cnt;
	u64 latency_dist[OHM_LATENCY_DIST_MAX];
};

struct sched_stat_para {
	bool ctrl;
	////bool logon;
	////bool trig;
	int min_thresh_ms;
	int low_thresh_ms;
	int high_thresh_ms;
	u64 delta_ms;
	spinlock_t lock;
	struct sched_stat_common all;
	struct sched_stat_common fg;
	struct sched_stat_common ux;
	struct sched_stat_common rt;
	struct sched_stat_common top;
	struct sched_stat_common bg;
	struct sched_stat_common sysbg;
};

extern struct sched_stat_para sched_para[OHM_SCHED_TOTAL];

#endif  /* endif */
