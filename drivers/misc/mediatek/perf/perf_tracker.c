/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <trace/events/sched.h>
#include <trace/events/mtk_events.h>

#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif

#include <linux/mm.h>
#include <linux/swap.h>
#include <mt-plat/mtk_blocktag.h>
#include <helio-dvfsrc.h>

#ifdef CONFIG_MTK_QOS_FRAMEWORK
#include <mtk_qos_sram.h>
#endif

#include "perf_tracker.h"

static int perf_tracker_on, perf_tracker_init;
static u64 checked_timestamp;
static u64 ms_interval = 2 * NSEC_PER_MSEC;
static DEFINE_SPINLOCK(check_lock);
static DEFINE_MUTEX(perf_ctl_mutex);

#if !defined(CONFIG_MTK_BLOCK_TAG) || !defined(MTK_BTAG_FEATURE_MICTX_IOSTAT)
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

#ifdef CONFIG_MTK_BLOCK_TAG
static struct mtk_btag_mictx_iostat_struct iostatptr;

void  __attribute__((weak)) mtk_btag_mictx_enable(int enable) {}

int __attribute__((weak)) mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_iostat_struct *io)
{
	return -1;
}
#endif

u32 __attribute__((weak)) dvfsrc_sram_read(u32 offset)
{
	return 0;
}

unsigned int __attribute__((weak)) get_dram_data_rate(void)
{
	return 0;
}

u32 __attribute__((weak)) qos_sram_read(u32 offset)
{
	return 0;
}

static int check_cnt;
static inline bool do_check(u64 wallclock)
{
	bool do_check = false;
	unsigned long flags;

	/* check interval */
	spin_lock_irqsave(&check_lock, flags);
	if ((s64)(wallclock - checked_timestamp)
			>= (s64)ms_interval) {
		checked_timestamp = wallclock;
		check_cnt++;
		do_check = true;
	}
	spin_unlock_irqrestore(&check_lock, flags);

	return do_check;
}
static inline bool hit_long_check(void)
{
	bool do_check = false;
	unsigned long flags;

	spin_lock_irqsave(&check_lock, flags);
	if (check_cnt >= 2) {
		check_cnt = 0;
		do_check = true;
	}
	spin_unlock_irqrestore(&check_lock, flags);

	return do_check;
}

static inline u32 cpu_stall_ratio(int cpu)
{
#ifdef CM_STALL_RATIO_OFFSET
	return qos_sram_read(CM_STALL_RATIO_OFFSET + cpu * 4);
#else
	return 0;
#endif
}

#define K(x) ((x) << (PAGE_SHIFT - 10))
#define max_cpus 8

void perf_tracker(u64 wallclock)
{
	int dram_rate = 0;
	long mm_free_pages = 0;
#ifdef CONFIG_MTK_BLOCK_TAG
	struct mtk_btag_mictx_iostat_struct *iostat = &iostatptr;
#endif
	int bw_c, bw_g, bw_mm, bw_total;
	int i;
	int stall[max_cpus] = {0};
	long mm_available;
	unsigned int sched_freq[3];

	if (!perf_tracker_on || !perf_tracker_init)
		return;

	if (!do_check(wallclock))
		return;

	/* dram freq */
	dram_rate = get_dram_data_rate();

	/* emi */
	bw_c  = qos_sram_read(QOS_DEBUG_1);
	bw_g  = qos_sram_read(QOS_DEBUG_2);
	bw_mm = qos_sram_read(QOS_DEBUG_3);
	bw_total = qos_sram_read(QOS_DEBUG_0);

	/* sched: cpu freq */
	sched_freq[0] = get_sched_cur_freq(0);
	sched_freq[1] = get_sched_cur_freq(1);
	sched_freq[2] = get_sched_cur_freq(2);

	/* trace for short msg */
	trace_perf_index_s(
			sched_freq[0], sched_freq[1], sched_freq[2],
			dram_rate, bw_c, bw_g, bw_mm, bw_total
			);

	if (!hit_long_check())
		return;

	/* free mem */
	mm_free_pages = global_page_state(NR_FREE_PAGES);
	mm_available = si_mem_available();

#ifdef CONFIG_MTK_BLOCK_TAG
	/* IO stat */
	if (mtk_btag_mictx_get_data(iostat))
		memset(iostat, 0, sizeof(struct mtk_btag_mictx_iostat_struct));
#endif

	/* cpu stall ratio */
	for (i = 0; i < nr_cpu_ids || i < max_cpus; i++)
		stall[i] = cpu_stall_ratio(i);

	/* trace for long msg */
	trace_perf_index_l(
			K(mm_free_pages),
			K(mm_available),
			iostat->wl,
			iostat->tp_req_r, iostat->tp_all_r,
			iostat->reqsize_r, iostat->reqcnt_r,
			iostat->tp_req_w, iostat->tp_all_w,
			iostat->reqsize_w, iostat->reqcnt_w,
			iostat->duration, iostat->q_depth,
			stall
			);
}

/*
 * make perf tracker on
 * /sys/devices/system/cpu/perf/enable
 * 1: on
 * 0: off
 */
static ssize_t show_perf_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "enable = %d\n",
			perf_tracker_on);
	return len;
}

static ssize_t store_perf_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	mutex_lock(&perf_ctl_mutex);

	if (sscanf(buf, "%iu", &val) != 0) {
		val = (val > 0) ? 1 : 0;

		perf_tracker_on = val;
#ifdef CONFIG_MTK_BLOCK_TAG
		mtk_btag_mictx_enable(val);
#endif
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static struct kobj_attribute perf_enable_attr =
__ATTR(enable, 0600, show_perf_enable, store_perf_enable);

static struct attribute *perf_attrs[] = {
	&perf_enable_attr.attr,
	NULL,
};

static struct attribute_group perf_attr_group = {
	.attrs = perf_attrs,
};

static int init_perf_tracker(void)
{
	int ret = 0;
	struct kobject *kobj = NULL;

	perf_tracker_init = 1;

	kobj = kobject_create_and_add("perf", &cpu_subsys.dev_root->kobj);

	if (kobj) {
		ret = sysfs_create_group(kobj, &perf_attr_group);
		if (ret)
			kobject_put(kobj);
		else
			kobject_uevent(kobj, KOBJ_ADD);
	}

	return 0;
}
late_initcall_sync(init_perf_tracker);
