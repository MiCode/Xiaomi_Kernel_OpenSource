// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "qcom-memlat: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/spinlock.h>
#include <trace/hooks/sched.h>
#include <soc/qcom/dcvs.h>
#include <soc/qcom/pmu_lib.h>
#include "trace-dcvs.h"

#define MAX_MEMLAT_GRPS	NUM_DCVS_HW_TYPES
#define FP_NAME		"memlat_fp"

enum common_ev_idx {
	INST_IDX,
	CYC_IDX,
	STALL_IDX,
	NUM_COMMON_EVS
};

enum grp_ev_idx {
	MISS_IDX,
	WB_IDX,
	ACC_IDX,
	NUM_GRP_EVS
};

enum mon_type {
	SAMPLING_MON	= BIT(0),
	THREADLAT_MON	= BIT(1),
	CPUCP_MON	= BIT(2),
	NUM_MON_TYPES
};

#define SAMPLING_VOTER	(num_possible_cpus())
#define NUM_FP_VOTERS	(SAMPLING_VOTER + 1)

enum memlat_type {
	MEMLAT_DEV,
	MEMLAT_GRP,
	MEMLAT_MON,
	NUM_MEMLAT_TYPES
};

struct memlat_spec {
	enum memlat_type type;
};

struct cpu_ctrs {
	u64				common_ctrs[NUM_COMMON_EVS];
	u64				grp_ctrs[MAX_MEMLAT_GRPS][NUM_GRP_EVS];
};

struct cpu_stats {
	struct cpu_ctrs			prev;
	struct cpu_ctrs			curr;
	struct cpu_ctrs			delta;
	struct qcom_pmu_data		raw_ctrs;
	ktime_t				last_sample_ts;
	spinlock_t			ctrs_lock;
	u32				freq_mhz;
	u32				stall_pct;
	u32				ipm[MAX_MEMLAT_GRPS];
	u32				wb_pct[MAX_MEMLAT_GRPS];
};

struct cpufreq_memfreq_map {
	unsigned int			cpufreq_mhz;
	unsigned int			memfreq_khz;
};

/* cur_freq is maintained by sampling algorithm only */
struct memlat_mon {
	struct device			*dev;
	struct memlat_group		*memlat_grp;
	enum mon_type			type;
	cpumask_t			cpus;
	struct cpufreq_memfreq_map	*freq_map;
	u32				ipm_ceil;
	u32				stall_floor;
	u32				wb_pct_thres;
	u32				wb_filter_ipm;
	u32				min_freq;
	u32				max_freq;
	u32				mon_min_freq;
	u32				mon_max_freq;
	u32				cur_freq;
	struct kobject			kobj;
	bool				is_compute;
};

struct memlat_group {
	struct device			*dev;
	struct kobject			*dcvs_kobj;
	enum dcvs_hw_type		hw_type;
	enum dcvs_path_type		sampling_path_type;
	enum dcvs_path_type		threadlat_path_type;
	u32				sampling_cur_freq;
	bool				fp_voting_enabled;
	u32				fp_freq;
	u32				*fp_votes;
	u32				grp_ev_ids[NUM_GRP_EVS];
	struct memlat_mon		*mons;
	u32				num_mons;
	u32				num_inited_mons;
	struct mutex			mons_lock;
};

struct memlat_dev_data {
	struct device			*dev;
	struct kobject			kobj;
	u32				common_ev_ids[NUM_COMMON_EVS];
	struct work_struct		work;
	struct workqueue_struct		*memlat_wq;
	u32				sample_ms;
	u32				cpucp_sample_ms;
	struct hrtimer			timer;
	ktime_t				last_update_ts;
	ktime_t				last_jiffy_ts;
	struct memlat_group		*groups[MAX_MEMLAT_GRPS];
	int				num_grps;
	int				num_inited_grps;
	spinlock_t			fp_agg_lock;
	spinlock_t			fp_commit_lock;
	bool				fp_enabled;
	bool				sampling_enabled;
	bool				inited;
};

static struct memlat_dev_data		*memlat_data;
static DEFINE_PER_CPU(struct cpu_stats *, sampling_stats);
static DEFINE_MUTEX(memlat_lock);

struct qcom_memlat_attr {
	struct attribute		attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};

#define to_memlat_attr(_attr) \
	container_of(_attr, struct qcom_memlat_attr, attr)
#define to_memlat_mon(k) container_of(k, struct memlat_mon, kobj)

#define MEMLAT_ATTR_RW(_name)						\
static struct qcom_memlat_attr _name =					\
__ATTR(_name, 0644, show_##_name, store_##_name)			\

#define MEMLAT_ATTR_RO(_name)						\
static struct qcom_memlat_attr _name =					\
__ATTR(_name, 0444, show_##_name, NULL)					\

#define show_attr(name)							\
static ssize_t show_##name(struct kobject *kobj,			\
			struct attribute *attr, char *buf)		\
{									\
	struct memlat_mon *mon = to_memlat_mon(kobj);			\
	return scnprintf(buf, PAGE_SIZE, "%u\n", mon->name);		\
}									\

#define store_attr(name, _min, _max) \
static ssize_t store_##name(struct kobject *kobj,			\
			struct attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	int ret;							\
	unsigned int val;						\
	struct memlat_mon *mon = to_memlat_mon(kobj);			\
	ret = kstrtouint(buf, 10, &val);				\
	if (ret < 0)							\
		return ret;						\
	val = max(val, _min);						\
	val = min(val, _max);						\
	mon->name = val;						\
	return count;							\
}									\

static ssize_t store_min_freq(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	unsigned int freq;
	struct memlat_mon *mon = to_memlat_mon(kobj);

	ret = kstrtouint(buf, 10, &freq);
	if (ret < 0)
		return ret;
	freq = max(freq, mon->mon_min_freq);
	freq = min(freq, mon->max_freq);

	mon->min_freq = freq;

	return count;
}

static ssize_t store_max_freq(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	unsigned int freq;
	struct memlat_mon *mon = to_memlat_mon(kobj);

	ret = kstrtouint(buf, 10, &freq);
	if (ret < 0)
		return ret;
	freq = max(freq, mon->min_freq);
	freq = min(freq, mon->mon_max_freq);

	mon->max_freq = freq;

	return count;
}

static ssize_t show_freq_map(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct memlat_mon *mon = to_memlat_mon(kobj);
	struct cpufreq_memfreq_map *map = mon->freq_map;
	unsigned int cnt = 0;

	cnt += scnprintf(buf, PAGE_SIZE, "CPU freq (MHz)\tMem freq (kHz)\n");

	while (map->cpufreq_mhz && cnt < PAGE_SIZE) {
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "%14u\t%14u\n",
				map->cpufreq_mhz, map->memfreq_khz);
		map++;
	}
	if (cnt < PAGE_SIZE)
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "\n");

	return cnt;
}

#define MIN_SAMPLE_MS	4U
#define MAX_SAMPLE_MS	1000U
static ssize_t store_sample_ms(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;
	val = max(val, MIN_SAMPLE_MS);
	val = min(val, MAX_SAMPLE_MS);

	memlat_data->sample_ms = val;

	return count;
}

static ssize_t show_sample_ms(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", memlat_data->sample_ms);
}

show_attr(min_freq);
show_attr(max_freq);
show_attr(cur_freq);
show_attr(ipm_ceil);
store_attr(ipm_ceil, 1U, 50000U);
show_attr(stall_floor);
store_attr(stall_floor, 0U, 100U);
show_attr(wb_pct_thres);
store_attr(wb_pct_thres, 0U, 100U);
show_attr(wb_filter_ipm);
store_attr(wb_filter_ipm, 0U, 50000U);

MEMLAT_ATTR_RW(sample_ms);

MEMLAT_ATTR_RW(min_freq);
MEMLAT_ATTR_RW(max_freq);
MEMLAT_ATTR_RO(freq_map);
MEMLAT_ATTR_RO(cur_freq);
MEMLAT_ATTR_RW(ipm_ceil);
MEMLAT_ATTR_RW(stall_floor);
MEMLAT_ATTR_RW(wb_pct_thres);
MEMLAT_ATTR_RW(wb_filter_ipm);

static struct attribute *memlat_settings_attr[] = {
	&sample_ms.attr,
	NULL,
};

static struct attribute *memlat_mon_attr[] = {
	&min_freq.attr,
	&max_freq.attr,
	&freq_map.attr,
	&cur_freq.attr,
	&ipm_ceil.attr,
	&stall_floor.attr,
	&wb_pct_thres.attr,
	&wb_filter_ipm.attr,
	NULL,
};

static struct attribute *compute_mon_attr[] = {
	&min_freq.attr,
	&max_freq.attr,
	&freq_map.attr,
	&cur_freq.attr,
	NULL,
};

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct qcom_memlat_attr *memlat_attr = to_memlat_attr(attr);
	ssize_t ret = -EIO;

	if (memlat_attr->show)
		ret = memlat_attr->show(kobj, attr, buf);

	return ret;
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct qcom_memlat_attr *memlat_attr = to_memlat_attr(attr);
	ssize_t ret = -EIO;

	if (memlat_attr->store)
		ret = memlat_attr->store(kobj, attr, buf, count);

	return ret;
}

static const struct sysfs_ops memlat_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static struct kobj_type memlat_settings_ktype = {
	.sysfs_ops	= &memlat_sysfs_ops,
	.default_attrs	= memlat_settings_attr,

};

static struct kobj_type memlat_mon_ktype = {
	.sysfs_ops	= &memlat_sysfs_ops,
	.default_attrs	= memlat_mon_attr,

};

static struct kobj_type compute_mon_ktype = {
	.sysfs_ops	= &memlat_sysfs_ops,
	.default_attrs	= compute_mon_attr,

};

static u32 cpufreq_to_memfreq(struct memlat_mon *mon, u32 cpu_mhz)
{
	struct cpufreq_memfreq_map *map = mon->freq_map;
	u32 mem_khz = 0;

	if (!map)
		goto out;

	while (map->cpufreq_mhz && map->cpufreq_mhz < cpu_mhz)
		map++;
	if (!map->cpufreq_mhz)
		map--;
	mem_khz = map->memfreq_khz;

out:
	return mem_khz;
}

static void calculate_sampling_stats(void)
{
	int i, grp, cpu, level = 0;
	unsigned long flags;
	struct cpu_stats *stats;
	struct cpu_ctrs *delta;
	struct memlat_group *memlat_grp;
	ktime_t now = ktime_get();
	s64 delta_us = ktime_us_delta(now, memlat_data->last_update_ts);

	memlat_data->last_update_ts = now;

	local_irq_save(flags);
	for_each_possible_cpu(cpu) {
		stats = per_cpu(sampling_stats, cpu);
		if (level == 0)
			spin_lock(&stats->ctrs_lock);
		else
			spin_lock_nested(&stats->ctrs_lock, level);
		level++;
	}

	for_each_possible_cpu(cpu) {
		stats = per_cpu(sampling_stats, cpu);
		/* update last sample ts to synchronize idle cpus */
		stats->last_sample_ts = now;
		delta = &stats->delta;
		for (i = 0; i < NUM_COMMON_EVS; i++) {
			if (!memlat_data->common_ev_ids[i])
				continue;
			delta->common_ctrs[i] = stats->curr.common_ctrs[i] -
						stats->prev.common_ctrs[i];
		}

		for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
			memlat_grp = memlat_data->groups[grp];
			if (!memlat_grp)
				continue;
			for (i = 0; i < NUM_GRP_EVS; i++) {
				if (!memlat_grp->grp_ev_ids[i])
					continue;
				delta->grp_ctrs[grp][i] =
					    stats->curr.grp_ctrs[grp][i] -
					    stats->prev.grp_ctrs[grp][i];
			}
		}

		stats->freq_mhz = delta->common_ctrs[CYC_IDX] / delta_us;
		if (!memlat_data->common_ev_ids[STALL_IDX])
			stats->stall_pct = 100;
		else
			stats->stall_pct = mult_frac(100,
						delta->common_ctrs[STALL_IDX],
						delta->common_ctrs[CYC_IDX]);
		for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
			memlat_grp = memlat_data->groups[grp];
			if (!memlat_grp) {
				stats->ipm[grp] = 0;
				stats->wb_pct[grp] = 0;
				continue;
			}
			stats->ipm[grp] = delta->common_ctrs[INST_IDX];
			if (delta->grp_ctrs[grp][MISS_IDX])
				stats->ipm[grp] /=
					delta->grp_ctrs[grp][MISS_IDX];

			if (!memlat_grp->grp_ev_ids[WB_IDX]
					|| !memlat_grp->grp_ev_ids[ACC_IDX])
				stats->wb_pct[grp] = 0;
			else
				stats->wb_pct[grp] = mult_frac(100,
						delta->grp_ctrs[grp][WB_IDX],
						delta->grp_ctrs[grp][ACC_IDX]);

			/* one meas event per memlat_group with group name */
			trace_memlat_dev_meas(dev_name(memlat_grp->dev), cpu,
					delta->common_ctrs[INST_IDX],
					delta->grp_ctrs[grp][MISS_IDX],
					stats->freq_mhz, stats->stall_pct,
					stats->wb_pct[grp], stats->ipm[grp]);

		}
		memcpy(&stats->prev, &stats->curr, sizeof(stats->curr));
	}

	for_each_possible_cpu(cpu) {
		stats = per_cpu(sampling_stats, cpu);
		spin_unlock(&stats->ctrs_lock);
	}
	local_irq_restore(flags);
}

static void calculate_mon_sampling_freq(struct memlat_mon *mon)
{
	struct cpu_stats *stats;
	int cpu, max_cpu = 0;
	u32 max_memfreq, max_cpufreq = 0;
	u32 hw = mon->memlat_grp->hw_type;

	for_each_cpu(cpu, &mon->cpus) {
		stats = per_cpu(sampling_stats, cpu);

		if ((mon->is_compute || (stats->ipm[hw] <= mon->ipm_ceil &&
				stats->stall_pct > mon->stall_floor) ||
				(stats->wb_pct[hw] >= mon->wb_pct_thres &&
				stats->ipm[hw] <= mon->wb_filter_ipm)) &&
				(stats->freq_mhz > max_cpufreq)) {
			max_cpu = cpu;
			max_cpufreq = stats->freq_mhz;
		}
	}

	max_memfreq = cpufreq_to_memfreq(mon, max_cpufreq);
	max_memfreq = max(max_memfreq, mon->min_freq);
	max_memfreq = min(max_memfreq, mon->max_freq);

	if (max_cpufreq) {
		stats = per_cpu(sampling_stats, max_cpu);
		trace_memlat_dev_update(dev_name(mon->dev), max_cpu,
				stats->prev.common_ctrs[INST_IDX],
				stats->prev.grp_ctrs[hw][MISS_IDX],
				max_cpufreq, max_memfreq);
	}

	mon->cur_freq = max_memfreq;
}

/*
 * updates fast path votes for "cpu" (i.e. fp_votes index)
 * fp_freqs array length MAX_MEMLAT_GRPS (i.e. one freq per grp)
 */
static void update_memlat_fp_vote(int cpu, u32 *fp_freqs)
{
	struct dcvs_freq voted_freqs[MAX_MEMLAT_GRPS];
	u32 max_freqs[MAX_MEMLAT_GRPS] = { 0 };
	int grp, i, ret;
	unsigned long flags_agg, flags_com;
	struct memlat_group *memlat_grp;
	u32 commit_mask = 0;

	if (!memlat_data->fp_enabled || cpu > NUM_FP_VOTERS)
		return;

	spin_lock_irqsave(&memlat_data->fp_agg_lock, flags_agg);
	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (!memlat_grp || !memlat_grp->fp_voting_enabled)
			continue;
		memlat_grp->fp_votes[cpu] = fp_freqs[grp];

		/* aggregate across all "cpus" */
		for (i = 0; i < NUM_FP_VOTERS; i++)
			max_freqs[grp] = max(memlat_grp->fp_votes[i],
							max_freqs[grp]);
		if (max_freqs[grp] != memlat_grp->fp_freq)
			commit_mask |= BIT(grp);
	}

	if (!commit_mask) {
		spin_unlock_irqrestore(&memlat_data->fp_agg_lock, flags_agg);
		return;
	}

	spin_lock_irqsave(&memlat_data->fp_commit_lock, flags_com);
	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		if (!(commit_mask & BIT(grp)))
			continue;
		memlat_grp = memlat_data->groups[grp];
		memlat_grp->fp_freq = max_freqs[grp];
	}
	spin_unlock_irqrestore(&memlat_data->fp_agg_lock, flags_agg);

	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		if (!(commit_mask & BIT(grp)))
			continue;
		voted_freqs[grp].ib = max_freqs[grp];
		voted_freqs[grp].hw_type = grp;
	}
	ret = qcom_dcvs_update_votes(FP_NAME, voted_freqs, commit_mask,
							DCVS_FAST_PATH);
	if (ret < 0)
		pr_err("error updating qcom dcvs fp: %d\n", ret);
	spin_unlock_irqrestore(&memlat_data->fp_commit_lock, flags_com);
}

/* sampling path update work */
static void memlat_update_work(struct work_struct *work)
{
	int i, grp, ret;
	struct memlat_group *memlat_grp;
	struct memlat_mon *mon;
	struct dcvs_freq new_freq;
	u32 max_freqs[MAX_MEMLAT_GRPS] = { 0 };

	calculate_sampling_stats();

	/* aggregate mons to calculate max freq per memlat_group */
	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (!memlat_grp)
			continue;
		for (i = 0; i < memlat_grp->num_inited_mons; i++) {
			mon = &memlat_grp->mons[i];
			if (!mon || !(mon->type & SAMPLING_MON))
				continue;
			calculate_mon_sampling_freq(mon);
			max_freqs[grp] = max(mon->cur_freq, max_freqs[grp]);
		}
	}

	update_memlat_fp_vote(SAMPLING_VOTER, max_freqs);

	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (!memlat_grp || memlat_grp->fp_voting_enabled ||
				memlat_grp->sampling_cur_freq == max_freqs[grp])
			continue;
		new_freq.ib = max_freqs[grp];
		new_freq.ab = 0;
		new_freq.hw_type = grp;
		ret = qcom_dcvs_update_votes(dev_name(memlat_grp->dev),
				&new_freq, 1, memlat_grp->sampling_path_type);
		if (ret < 0)
			dev_err(memlat_grp->dev, "qcom dcvs err: %d\n", ret);
		memlat_grp->sampling_cur_freq = max_freqs[grp];
	}
}

static enum hrtimer_restart memlat_hrtimer_handler(struct hrtimer *timer)
{
	queue_work(memlat_data->memlat_wq, &memlat_data->work);

	return HRTIMER_NORESTART;
}

static const u64 HALF_TICK_NS = (NSEC_PER_SEC / HZ) >> 1;
#define MEMLAT_UPDATE_DELAY (100 * NSEC_PER_USEC)
static void memlat_jiffies_update_cb(void *unused, void *extra)
{
	ktime_t now = ktime_get();
	s64 delta_ns = now - memlat_data->last_jiffy_ts + HALF_TICK_NS;

	if (unlikely(!memlat_data->inited))
		return;

	if (delta_ns > ms_to_ktime(memlat_data->sample_ms)) {
		hrtimer_start(&memlat_data->timer, MEMLAT_UPDATE_DELAY,
						HRTIMER_MODE_REL_PINNED);
		memlat_data->last_jiffy_ts = now;
	}
}

/*
 * Note: must hold stats->ctrs_lock and populate stats->raw_ctrs
 * before calling this API.
 */
static void process_raw_ctrs(struct cpu_stats *stats)
{
	int i, grp, idx;
	struct cpu_ctrs *curr_ctrs = &stats->curr;
	struct qcom_pmu_data *raw_ctrs = &stats->raw_ctrs;
	struct memlat_group *memlat_grp;
	u32 event_id;
	u64 ev_data;

	for (i = 0; i < raw_ctrs->num_evs; i++) {
		event_id = raw_ctrs->event_ids[i];
		ev_data = raw_ctrs->ev_data[i];
		if (!event_id)
			break;

		for (idx = 0; idx < NUM_COMMON_EVS; idx++) {
			if (event_id != memlat_data->common_ev_ids[idx])
				continue;
			curr_ctrs->common_ctrs[idx] = ev_data;
			break;
		}
		if (idx < NUM_COMMON_EVS)
			continue;

		for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
			memlat_grp = memlat_data->groups[grp];
			if (!memlat_grp)
				continue;
			for (idx = 0; idx < NUM_GRP_EVS; idx++) {
				if (event_id != memlat_grp->grp_ev_ids[idx])
					continue;
				curr_ctrs->grp_ctrs[grp][idx] = ev_data;
				break;
			}
			if (idx < NUM_GRP_EVS)
				break;
		}
	}
}

static void memlat_pmu_idle_cb(struct qcom_pmu_data *data, int cpu, int state)
{
	struct cpu_stats *stats = per_cpu(sampling_stats, cpu);
	unsigned long flags;

	if (unlikely(!memlat_data->inited))
		return;

	spin_lock_irqsave(&stats->ctrs_lock, flags);
	memcpy(&stats->raw_ctrs, data, sizeof(*data));
	process_raw_ctrs(stats);
	spin_unlock_irqrestore(&stats->ctrs_lock, flags);
}

static struct qcom_pmu_notif_node memlat_idle_notif = {
	.idle_cb = memlat_pmu_idle_cb,
};

static void memlat_sched_tick_cb(void *unused, struct rq *rq)
{
	int ret, cpu = smp_processor_id();
	struct cpu_stats *stats = per_cpu(sampling_stats, cpu);
	ktime_t now = ktime_get();
	s64 delta_ns;
	unsigned long flags;

	if (unlikely(!memlat_data->inited))
		return;

	spin_lock_irqsave(&stats->ctrs_lock, flags);
	delta_ns = now - stats->last_sample_ts + HALF_TICK_NS;
	if (delta_ns < ms_to_ktime(memlat_data->sample_ms))
		goto out;
	stats->last_sample_ts = now;
	stats->raw_ctrs.num_evs = 0;
	ret = qcom_pmu_read_all_local(&stats->raw_ctrs);
	if (ret < 0 || stats->raw_ctrs.num_evs == 0) {
		pr_err("error reading pmu counters on cpu%d: %d\n", cpu, ret);
		goto out;
	}
	process_raw_ctrs(stats);

out:
	spin_unlock_irqrestore(&stats->ctrs_lock, flags);
}

static int get_mask_from_dev_handle(struct platform_device *pdev,
					cpumask_t *mask)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_phandle;
	struct device *cpu_dev;
	int cpu, i = 0;
	int ret = -ENOENT;

	dev_phandle = of_parse_phandle(dev->of_node, "qcom,cpulist", i++);
	while (dev_phandle) {
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev && cpu_dev->of_node == dev_phandle) {
				cpumask_set_cpu(cpu, mask);
				ret = 0;
				break;
			}
		}
		dev_phandle = of_parse_phandle(dev->of_node,
						"qcom,cpulist", i++);
	}

	return ret;
}

#define COREDEV_TBL_PROP	"qcom,cpufreq-memfreq-tbl"
#define NUM_COLS		2
static struct cpufreq_memfreq_map *init_cpufreq_memfreq_map(struct device *dev,
					struct device_node *of_node)
{
	int len, nf, i, j;
	u32 data;
	struct cpufreq_memfreq_map *tbl;
	int ret;

	if (!of_find_property(of_node, COREDEV_TBL_PROP, &len))
		return NULL;
	len /= sizeof(data);

	if (len % NUM_COLS || len == 0)
		return NULL;
	nf = len / NUM_COLS;

	tbl = devm_kzalloc(dev, (nf + 1) * sizeof(struct cpufreq_memfreq_map),
			GFP_KERNEL);
	if (!tbl)
		return NULL;

	for (i = 0, j = 0; i < nf; i++, j += 2) {
		ret = of_property_read_u32_index(of_node, COREDEV_TBL_PROP,
							j, &data);
		if (ret < 0)
			return NULL;
		tbl[i].cpufreq_mhz = data / 1000;

		ret = of_property_read_u32_index(of_node, COREDEV_TBL_PROP,
							j + 1, &data);
		if (ret < 0)
			return NULL;
		tbl[i].memfreq_khz = data;
		pr_debug("Entry%d CPU:%u, Mem:%u\n", i, tbl[i].cpufreq_mhz,
				tbl[i].memfreq_khz);
	}
	tbl[i].cpufreq_mhz = 0;

	return tbl;
}

static bool memlat_grps_and_mons_inited(void)
{
	struct memlat_group *memlat_grp;
	int grp;

	if (memlat_data->num_inited_grps < memlat_data->num_grps)
		return false;

	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (!memlat_grp)
			continue;
		if (memlat_grp->num_inited_mons < memlat_grp->num_mons)
			return false;
	}

	return true;
}

static int memlat_sampling_init(void)
{
	int cpu;
	struct device *dev = memlat_data->dev;
	struct cpu_stats *stats;

	for_each_possible_cpu(cpu) {
		stats = devm_kzalloc(dev, sizeof(*stats), GFP_KERNEL);
		if (!stats)
			return -ENOMEM;
		per_cpu(sampling_stats, cpu) = stats;
		spin_lock_init(&stats->ctrs_lock);
	}

	memlat_data->memlat_wq = create_freezable_workqueue("memlat_wq");
	if (!memlat_data->memlat_wq) {
		dev_err(dev, "Couldn't create memlat workqueue.\n");
		return -ENOMEM;
	}
	INIT_WORK(&memlat_data->work, &memlat_update_work);

	hrtimer_init(&memlat_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	memlat_data->timer.function = memlat_hrtimer_handler;

	register_trace_android_vh_scheduler_tick(memlat_sched_tick_cb, NULL);
	register_trace_android_vh_jiffies_update(memlat_jiffies_update_cb, NULL);
	qcom_pmu_idle_register(&memlat_idle_notif);

	return 0;
}

static inline bool should_enable_memlat_fp(void)
{
	int grp;
	struct memlat_group *memlat_grp;

	/* wait until all groups have inited before enabling */
	if (memlat_data->num_inited_grps < memlat_data->num_grps)
		return false;

	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (memlat_grp && memlat_grp->fp_voting_enabled)
			return true;
	}

	return false;
}

#define INST_EV		0x08
#define CYC_EV		0x11
static int memlat_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct kobject *dcvs_kobj;
	struct memlat_dev_data *dev_data;
	int i, cpu, last_ev, last_cpu, max, ret;
	u32 event_id;

	dev_data = devm_kzalloc(dev, sizeof(*dev_data), GFP_KERNEL);
	if (!dev_data)
		return -ENOMEM;

	dev_data->dev = dev;
	dev_data->sample_ms = 8;

	dev_data->num_grps = of_get_available_child_count(dev->of_node);
	if (!dev_data->num_grps) {
		dev_err(dev, "No memlat grps provided!\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,inst-ev", &event_id);
	if (ret < 0) {
		dev_dbg(dev, "Inst event not specified. Using def:0x%x\n",
			INST_EV);
		event_id = INST_EV;
	}
	dev_data->common_ev_ids[INST_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,cyc-ev", &event_id);
	if (ret < 0) {
		dev_dbg(dev, "Cyc event not specified. Using def:0x%x\n",
			CYC_EV);
		event_id = CYC_EV;
	}
	dev_data->common_ev_ids[CYC_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,stall-ev", &event_id);
	if (ret < 0)
		dev_dbg(dev, "Stall event not specified. Skipping.\n");
	else
		dev_data->common_ev_ids[STALL_IDX] = event_id;

	for_each_possible_cpu(cpu) {
		last_cpu = cpu;
		for (i = 0; i < NUM_COMMON_EVS; i++) {
			last_ev = i;
			event_id = dev_data->common_ev_ids[i];
			if (!event_id)
				continue;
			ret = qcom_pmu_create(event_id, cpu);
			if (ret < 0) {
				dev_err(dev, "err creating cpu%d ev=%lu: %d\n",
							cpu, event_id, ret);
				goto err_out;
			}
		}
	}

	dcvs_kobj = qcom_dcvs_kobject_get(NUM_DCVS_HW_TYPES);
	if (IS_ERR(dcvs_kobj)) {
		ret = PTR_ERR(dcvs_kobj);
		dev_err(dev, "error getting kobj from qcom_dcvs: %d\n", ret);
		goto err_out;
	}
	ret = kobject_init_and_add(&dev_data->kobj, &memlat_settings_ktype,
					dcvs_kobj, "memlat_settings");
	if (ret < 0) {
		dev_err(dev, "failed to init memlat settings kobj: %d\n", ret);
		kobject_put(&dev_data->kobj);
		goto err_out;
	}

	memlat_data = dev_data;

	return 0;

err_out:
	for (cpu = 0; cpu <= last_cpu; cpu++) {
		max = (cpu == last_cpu) ? last_ev : NUM_COMMON_EVS;
		for (i = 0; i < max; i++) {
			event_id = dev_data->common_ev_ids[i];
			if (!event_id)
				continue;
			qcom_pmu_delete(event_id, cpu);
		}
	}

	return ret;
}

static int memlat_grp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct memlat_group *memlat_grp;
	int i, cpu, last_ev, last_cpu, max, ret;
	u32 event_id, num_mons;
	u32 hw_type = NUM_DCVS_PATHS, path_type = NUM_DCVS_PATHS;
	struct device_node *of_node;

	of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!of_node) {
		dev_err(dev, "Unable to find target-dev for grp\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(of_node, "qcom,dcvs-hw-type", &hw_type);
	if (ret < 0 || hw_type >= NUM_DCVS_HW_TYPES) {
		dev_err(dev, "invalid dcvs hw_type=%d, ret=%d\n", hw_type, ret);
		return -EINVAL;
	}

	memlat_grp = devm_kzalloc(dev, sizeof(*memlat_grp), GFP_KERNEL);
	memlat_grp->hw_type = hw_type;
	memlat_grp->dev = dev;

	memlat_grp->dcvs_kobj = qcom_dcvs_kobject_get(hw_type);
	if (IS_ERR(memlat_grp->dcvs_kobj)) {
		ret = PTR_ERR(memlat_grp->dcvs_kobj);
		dev_err(dev, "error getting kobj from qcom_dcvs: %d\n", ret);
		return ret;
	}

	of_node = of_parse_phandle(dev->of_node, "qcom,sampling-path", 0);
	if (of_node) {
		ret = of_property_read_u32(of_node, "qcom,dcvs-path-type",
								&path_type);
		if (ret < 0 || path_type >= NUM_DCVS_PATHS) {
			dev_err(dev, "invalid dcvs path: %d, ret=%d\n",
							path_type, ret);
			return -EINVAL;
		}
		if (path_type == DCVS_FAST_PATH)
			ret = qcom_dcvs_register_voter(FP_NAME, hw_type,
								path_type);
		else
			ret = qcom_dcvs_register_voter(dev_name(dev), hw_type,
								path_type);
		if (ret < 0) {
			dev_err(dev, "qcom dcvs registration error: %d\n", ret);
			return ret;
		}
		memlat_grp->sampling_path_type = path_type;
	} else
		memlat_grp->sampling_path_type = NUM_DCVS_PATHS;

	of_node = of_parse_phandle(dev->of_node, "qcom,threadlat-path", 0);
	if (of_node) {
		ret = of_property_read_u32(of_node, "qcom,dcvs-path-type",
								&path_type);
		if (ret < 0 || path_type >= NUM_DCVS_PATHS) {
			dev_err(dev, "invalid dcvs path: %d, ret=%d\n",
							path_type, ret);
			return -EINVAL;
		}
		memlat_grp->threadlat_path_type = path_type;
	} else
		memlat_grp->threadlat_path_type = NUM_DCVS_PATHS;

	if (path_type >= NUM_DCVS_PATHS) {
		dev_err(dev, "error: no dcvs voting paths\n");
		return -ENODEV;
	}
	if (memlat_grp->sampling_path_type == DCVS_FAST_PATH ||
			memlat_grp->threadlat_path_type == DCVS_FAST_PATH) {
		memlat_grp->fp_voting_enabled = true;
		memlat_grp->fp_votes = devm_kzalloc(dev, NUM_FP_VOTERS *
						sizeof(*memlat_grp->fp_votes),
						GFP_KERNEL);
	}

	num_mons = of_get_available_child_count(dev->of_node);
	if (!num_mons) {
		dev_err(dev, "No mons provided!\n");
		return -ENODEV;
	}
	memlat_grp->mons =
		devm_kzalloc(dev, num_mons * sizeof(*memlat_grp->mons),
				GFP_KERNEL);
	if (!memlat_grp->mons)
		return -ENOMEM;
	memlat_grp->num_mons = num_mons;
	memlat_grp->num_inited_mons = 0;
	mutex_init(&memlat_grp->mons_lock);

	ret = of_property_read_u32(dev->of_node, "qcom,miss-ev", &event_id);
	if (ret < 0) {
		dev_err(dev, "Cache miss event missing for grp: %d\n", ret);
		return -EINVAL;
	}
	memlat_grp->grp_ev_ids[MISS_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,access-ev",
				   &event_id);
	if (ret < 0)
		dev_dbg(dev, "Access event not specified. Skipping.\n");
	else
		memlat_grp->grp_ev_ids[ACC_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,wb-ev", &event_id);
	if (ret < 0)
		dev_dbg(dev, "WB event not specified. Skipping.\n");
	else
		memlat_grp->grp_ev_ids[WB_IDX] = event_id;

	for_each_possible_cpu(cpu) {
		last_cpu = cpu;
		for (i = 0; i < NUM_GRP_EVS; i++) {
			last_ev = i;
			event_id = memlat_grp->grp_ev_ids[i];
			if (!event_id)
				continue;
			ret = qcom_pmu_create(event_id, cpu);
			if (ret < 0) {
				dev_err(dev, "err creating cpu%d ev=%lu: %d\n",
							cpu, event_id, ret);
				goto err_out;
			}
		}
	}

	mutex_lock(&memlat_lock);
	memlat_data->num_inited_grps++;
	memlat_data->groups[hw_type] = memlat_grp;
	if (!memlat_data->fp_enabled && should_enable_memlat_fp()) {
		spin_lock_init(&memlat_data->fp_agg_lock);
		spin_lock_init(&memlat_data->fp_commit_lock);
		memlat_data->fp_enabled = true;
	}
	mutex_unlock(&memlat_lock);

	dev_set_drvdata(dev, memlat_grp);

	return 0;

err_out:
	for (cpu = 0; cpu <= last_cpu; cpu++) {
		max = (cpu == last_cpu) ? last_ev : NUM_GRP_EVS;
		for (i = 0; i < max; i++) {
			event_id = memlat_grp->grp_ev_ids[i];
			if (!event_id)
				continue;
			qcom_pmu_delete(event_id, cpu);
		}
	}

	return ret;
}

static int memlat_mon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct memlat_group *memlat_grp;
	struct memlat_mon *mon;
	struct device_node *of_node = dev->of_node;
	u32 num_cpus;

	memlat_grp = dev_get_drvdata(dev->parent);
	if (!memlat_grp) {
		dev_err(dev, "Mon probe called without memlat_grp inited.\n");
		return -ENODEV;
	}

	mutex_lock(&memlat_grp->mons_lock);
	mon = &memlat_grp->mons[memlat_grp->num_inited_mons];
	mon->memlat_grp = memlat_grp;
	mon->dev = dev;

	if (get_mask_from_dev_handle(pdev, &mon->cpus)) {
		dev_err(dev, "Mon missing cpulist\n");
		ret = -ENODEV;
		goto unlock_out;
	}

	num_cpus = cpumask_weight(&mon->cpus);

	if (of_property_read_bool(dev->of_node, "qcom,sampling-enabled")) {
		mutex_lock(&memlat_lock);
		if (!memlat_data->sampling_enabled) {
			ret = memlat_sampling_init();
			memlat_data->sampling_enabled = true;
		}
		mutex_unlock(&memlat_lock);
		mon->type |= SAMPLING_MON;
	}

	if (of_property_read_bool(dev->of_node, "qcom,threadlat-enabled"))
		mon->type |= THREADLAT_MON;

	if (of_property_read_bool(dev->of_node, "qcom,cpucp-enabled"))
		mon->type |= CPUCP_MON;

	if (!mon->type) {
		dev_err(dev, "No types configured for mon!\n");
		ret = -ENODEV;
		goto unlock_out;
	}

	if (of_property_read_bool(dev->of_node, "qcom,compute-mon"))
		mon->is_compute = true;

	mon->ipm_ceil = 400;
	mon->stall_floor = 0;
	mon->wb_pct_thres = 100;
	mon->wb_filter_ipm = 25000;

	if (of_parse_phandle(dev->of_node, COREDEV_TBL_PROP, 0))
		of_node = of_parse_phandle(dev->of_node, COREDEV_TBL_PROP, 0);
	mon->freq_map = init_cpufreq_memfreq_map(dev, of_node);
	if (!mon->freq_map) {
		dev_err(dev, "error importing cpufreq-memfreq table!\n");
		ret = -EINVAL;
		goto unlock_out;
	}

	mon->mon_min_freq = mon->min_freq = cpufreq_to_memfreq(mon, 0);
	mon->mon_max_freq = mon->max_freq = cpufreq_to_memfreq(mon, U32_MAX);
	mon->cur_freq = mon->min_freq;

	if (mon->is_compute)
		ret = kobject_init_and_add(&mon->kobj, &compute_mon_ktype,
					memlat_grp->dcvs_kobj, dev_name(dev));
	else
		ret = kobject_init_and_add(&mon->kobj, &memlat_mon_ktype,
					memlat_grp->dcvs_kobj, dev_name(dev));
	if (ret < 0) {
		dev_err(dev, "failed to init memlat mon kobj: %d\n", ret);
		kobject_put(&mon->kobj);
		goto unlock_out;
	}

	memlat_grp->num_inited_mons++;
	if (memlat_grps_and_mons_inited())
		memlat_data->inited = true;

unlock_out:
	mutex_unlock(&memlat_grp->mons_lock);
	return ret;
}

static int qcom_memlat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	const struct memlat_spec *spec = of_device_get_match_data(dev);
	enum memlat_type type = NUM_MEMLAT_TYPES;

	if (spec)
		type = spec->type;

	switch (type) {
	case MEMLAT_DEV:
		if (memlat_data) {
			dev_err(dev, "only one memlat device allowed\n");
			ret = -ENODEV;
		}
		ret = memlat_dev_probe(pdev);
		if (!ret && of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case MEMLAT_GRP:
		ret = memlat_grp_probe(pdev);
		if (!ret && of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case MEMLAT_MON:
		ret = memlat_mon_probe(pdev);
		break;
	default:
		/*
		 * This should never happen.
		 */
		dev_err(dev, "Invalid memlat mon type specified: %u\n", type);
		return -EINVAL;
	}

	if (ret < 0) {
		dev_err(dev, "Failure to probe memlat device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct memlat_spec spec[] = {
	[0] = { MEMLAT_DEV },
	[1] = { MEMLAT_GRP },
	[2] = { MEMLAT_MON },
};

static const struct of_device_id memlat_match_table[] = {
	{ .compatible = "qcom,memlat", .data = &spec[0] },
	{ .compatible = "qcom,memlat-grp", .data = &spec[1] },
	{ .compatible = "qcom,memlat-mon", .data = &spec[2] },
	{}
};

static struct platform_driver qcom_memlat_driver = {
	.probe = qcom_memlat_probe,
	.driver = {
		.name = "qcom-memlat",
		.of_match_table = memlat_match_table,
		.suppress_bind_attrs = true,
	},
};

static int __init qcom_memlat_init(void)
{
	return platform_driver_register(&qcom_memlat_driver);
}

#if IS_MODULE(CONFIG_QCOM_MEMLAT)
module_init(qcom_memlat_init);
#else
arch_initcall(qcom_memlat_init);
#endif
static __exit void qcom_memlat_exit(void)
{
	platform_driver_unregister(&qcom_memlat_driver);
}
module_exit(qcom_memlat_exit);

MODULE_DESCRIPTION("QCOM MEMLAT Driver");
MODULE_LICENSE("GPL v2");
