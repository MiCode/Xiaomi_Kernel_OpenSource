// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <trace/events/sched.h>

#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif

#include <linux/mm.h>
#include <linux/swap.h>

#define CREATE_TRACE_POINTS
#include <perf_tracker_trace.h>

#ifdef CONFIG_MTK_QOS_FRAMEWORK
#include <mtk_qos_sram.h>
#endif

static int perf_tracker_on;
static DEFINE_MUTEX(perf_ctl_mutex);

static struct mtk_btag_mictx_iostat_struct iostat;
#ifdef CONFIG_MTK_BLOCK_TAG
void  __attribute__((weak)) mtk_btag_mictx_enable(int enable) {}

int __attribute__((weak)) mtk_btag_mictx_get_data(
		struct mtk_btag_mictx_iostat_struct *io)
{
	return -1;
}
#endif

int perf_tracker_enable(int on)
{
	perf_tracker_on = on;

	return (perf_tracker_on == on) ? 0 : -1;
}
EXPORT_SYMBOL(perf_tracker_enable);

unsigned int __attribute__((weak)) get_dram_data_rate(void)
{
	return 0;
}

u32 __attribute__((weak)) qos_sram_read(u32 offset)
{
	return 0;
}

static inline u32 cpu_stall_ratio(int cpu)
{
#ifdef CONFIG_MTK_QOS_FRAMEWORK
	return qos_sram_read(CM_STALL_RATIO_ID_0 + cpu);
#else
	return 0;
#endif
}

#define K(x) ((x) << (PAGE_SHIFT - 10))
#define max_cpus 8

void perf_tracker(u64 wallclock,
		    long mm_available,
		    long mm_free)
{
	int dram_rate = 0;
	struct mtk_btag_mictx_iostat_struct *iostat_ptr = &iostat;
	int bw_c = 0, bw_g = 0, bw_mm = 0, bw_total = 0;
	int i;
	int stall[max_cpus] = {0};
	unsigned int sched_freq[3] = {0};
	int cid;

	if (!perf_tracker_on)
		return;

	/* dram freq */
	dram_rate = get_dram_data_rate();

#ifdef CONFIG_MTK_QOS_FRAMEWORK
	/* emi */
	bw_c  = qos_sram_read(QOS_DEBUG_1);
	bw_g  = qos_sram_read(QOS_DEBUG_2);
	bw_mm = qos_sram_read(QOS_DEBUG_3);
	bw_total = qos_sram_read(QOS_DEBUG_0);
#endif
	/* sched: cpu freq */
	for (cid = 0; cid < cluster_nr; cid++)
		sched_freq[cid] = mt_cpufreq_get_cur_freq(cid);

	/* trace for short msg */
	trace_perf_index_s(
			sched_freq[0], sched_freq[1], sched_freq[2],
			dram_rate, bw_c, bw_g, bw_mm, bw_total
			);

	if (!hit_long_check())
		return;

	/* free mem */
	if (mm_free == -1) {
		mm_free = global_zone_page_state(NR_FREE_PAGES);
		mm_available = si_mem_available();
	}

#ifdef CONFIG_MTK_BLOCK_TAG
	/* If getting I/O stat fail, fallback to zero value. */
	if (mtk_btag_mictx_get_data(iostat_ptr))
		memset(iostat_ptr, 0,
			sizeof(struct mtk_btag_mictx_iostat_struct));
#endif
	/* cpu stall ratio */
	for (i = 0; i < nr_cpu_ids || i < max_cpus; i++)
		stall[i] = cpu_stall_ratio(i);

	/* trace for long msg */
	trace_perf_index_l(
			K(mm_free),
			K(mm_available),
			iostat_ptr,
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
				struct kobj_attribute *attr,
				const char *buf,
				size_t count)
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

struct kobj_attribute perf_tracker_enable_attr =
__ATTR(enable, 0600, show_perf_enable, store_perf_enable);
