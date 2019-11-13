// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/percpu.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include <asm/local64.h>

#define L2_COUNTERS_BUG		 "[l2 counters error]: "
/*
 * Events id
 * 0xXXX here,
 *
 * 1 bit(lsb) for group (group is either txn/tenure counter).
 * 4 bits for serial number for counter starting from 0 to 8.
 * 5 bits for bit position of counter enable bit in a register.
 */
#define L2_EVENT_CYCLE_CNTR			0x000
#define L2_EVENT_DDR_WR_CNTR			0x022
#define L2_EVENT_DDR_RD_CNTR			0x044
#define L2_EVENT_SNP_RD_CNTR			0x066
#define L2_EVENT_ACP_WR_CNTR			0x088

#define L2_EVENT_TENURE_CNTR			0x26b
#define L2_EVENT_LOW_RANGE_OCCUR_CNTR		0x04d
#define L2_EVENT_MID_RANGE_OCCUR_CNTR		0x0cf
#define L2_EVENT_HIGH_RANGE_OCCUR_CNTR		0x151

#define EVENT_GROUP_MASK			0x1
#define REGBIT_MASK				0x3e0
#define ID_MASK					0x1e

#define TRANSACTION_CNTRS_GROUP_ID		0x0
#define TENURE_CNTRS_GROUP_ID			0x1
#define ID_SHIFT				0x1
#define REGBIT_SHIFT				0x5

#define TXN_CONFIG_REG_OFFSET			0x54c
#define OVERFLOW_REG_OFFSET			0x560
#define CNTR_SET_VAL_REG_OFFSET			0x55c
#define TXN_CYCLE_CNTR_DATA			0x634
#define TXN_DDR_WR_CNTR_DATA			0x638
#define TXN_DDR_RD_CNTR_DATA			0x63c
#define TXN_SNP_RD_CNTR_DATA			0x640
#define TXN_ACP_WR_CNTR_DATA			0x644

#define TENURE_CONFIG_REG_OFFSET		0x52c
#define LOW_RANGE_OCCURRENCE_CNTR_DATA		0x53c
#define MID_RANGE_OCCURRENCE_CNTR_DATA		0x540
#define HIGH_RANGE_OCCURRENCE_CNTR_DATA		0x544
#define LPM_TENURE_CNTR_DATA			0x548
#define LOW_RANGE_TENURE_VAL			0x534
#define MID_RANGE_TENURE_VAL			0x538

#define TENURE_ENABLE_ALL			0x880444
#define TENURE_CNTR_ENABLE			19
#define LOW_RANGE_OCCURRENCE_CNTR_ENABLE	2
#define MID_RANGE_OCCURRENCE_CNTR_ENABLE	6
#define HIGH_RANGE_OCCURRENCE_CNTR_ENABLE	10
#define OCCURRENCE_CNTR_ENABLE_MASK		(BIT(2) | BIT(6) | BIT(10))

#define LPM_MODE_TENURE_CNTR_RESET		12
#define LOW_RANGE_OCCURRENCE_CNTR_RESET		 0
#define MID_RANGE_OCCURRENCE_CNTR_RESET		 4
#define HIGH_RANGE_OCCURRENCE_CNTR_RESET	 8

/* Txn reset/set/overflow bit offsets */
#define TXN_RESET_BIT				 5
#define TXN_RESET_ALL_CNTR			0x000003e0
#define TXN_RESET_ALL_CNTR_OVSR_BIT		0x007c0000
#define TENURE_RESET_ALL_CNTR			0x00001111
#define TENURE_RESET_OVERFLOW_ALL_CNTR		0x00002888

#define TXN_SET_BIT				13
#define TXN_OVERFLOW_RESET_BIT			18

#define LOW_RANGE_OCCURRENCE_CNTR_OVERFLOW_RESET	 3
#define MID_RANGE_OCCURRENCE_CNTR_OVERFLOW_RESET	 7
#define HIGH_RANGE_OCCURRENCE_CNTR_OVERFLOW_RESET	11
#define LPM_MODE_TENURE_CNTR_OVERFLOW_RESET		13

enum counter_index {
	CLUSTER_CYCLE_COUNTER,
	DDR_WR_CNTR,
	DDR_RD_CNTR,
	SNP_RD_CNTR,
	ACP_WR_CNTR,
	LPM_TENURE_CNTR,
	LOW_OCCURRENCE_CNTR,
	MID_OCCURRENCE_CNTR,
	HIGH_OCCURRENCE_CNTR,
	MAX_L2_CNTRS
};

/*
 * Each cluster has its own PMU(counters) and associated with one or more CPUs.
 * This structure represents one of the hardware PMUs.
 */
struct cluster_pmu {
	struct device dev;
	struct list_head next;
	struct perf_event *events[MAX_L2_CNTRS];
	void __iomem *reg_addr;
	struct l2cache_pmu *l2cache_pmu;
	DECLARE_BITMAP(used_counters, MAX_L2_CNTRS);
	int irq;
	int cluster_id;
	/* The CPU that is used for collecting events on this cluster */
	int on_cpu;
	/* All the CPUs associated with this cluster */
	cpumask_t cluster_cpus;
	spinlock_t pmu_lock;
};

/*
 * Aggregate PMU. Implements the core pmu functions and manages
 * the hardware PMUs.
 */
struct l2cache_pmu {
	struct hlist_node node;
	u32 num_pmus;
	struct pmu pmu;
	int num_counters;
	cpumask_t cpumask;
	struct platform_device *pdev;
	struct cluster_pmu * __percpu *pmu_cluster;
	struct list_head clusters;
};


static unsigned int which_cluster_tenure = 1;
static u32 l2_counter_present_mask;

#define to_l2cache_pmu(p)	(container_of(p, struct l2cache_pmu, pmu))
#define to_cluster_device(d)	container_of(d, struct cluster_pmu, dev)


static inline struct cluster_pmu *get_cluster_pmu(
	struct l2cache_pmu *l2cache_pmu, int cpu)
{
	return *per_cpu_ptr(l2cache_pmu->pmu_cluster, cpu);
}

static inline u32 cluster_tenure_counter_read(struct cluster_pmu *cluster,
		u32 idx)
{
	u32 val = 0;

	switch (idx) {
	case LOW_RANGE_OCCURRENCE_CNTR_ENABLE:
		val = readl_relaxed(cluster->reg_addr +
					LOW_RANGE_OCCURRENCE_CNTR_DATA);
		break;

	case MID_RANGE_OCCURRENCE_CNTR_ENABLE:
		val = readl_relaxed(cluster->reg_addr +
					MID_RANGE_OCCURRENCE_CNTR_DATA);
		break;

	case HIGH_RANGE_OCCURRENCE_CNTR_ENABLE:
		val = readl_relaxed(cluster->reg_addr +
					HIGH_RANGE_OCCURRENCE_CNTR_DATA);
		break;

	default:
		pr_crit(L2_COUNTERS_BUG
			"Invalid index, during %s\n", __func__);
	}

	return val;
}

static inline u32 cluster_pmu_counter_get_value(struct cluster_pmu *cluster,
		u32 idx, u32 event_grp)
{
	if (event_grp == TENURE_CNTRS_GROUP_ID)
		return cluster_tenure_counter_read(cluster, idx);

	return readl_relaxed(cluster->reg_addr +
					TXN_CYCLE_CNTR_DATA + (4 * idx));
}

static inline u32 cluster_txn_config_read(struct cluster_pmu *cluster)
{
	return readl_relaxed(cluster->reg_addr + TXN_CONFIG_REG_OFFSET);
}

static inline void cluster_txn_config_write(struct cluster_pmu *cluster,
		u32 val)
{
	writel_relaxed(val, cluster->reg_addr + TXN_CONFIG_REG_OFFSET);
}

static inline u32 cluster_tenure_config_read(struct cluster_pmu *cluster)
{
	return readl_relaxed(cluster->reg_addr + TENURE_CONFIG_REG_OFFSET);
}

static inline void cluster_tenure_config_write(struct cluster_pmu *cluster,
		u32 val)
{
	writel_relaxed(val, cluster->reg_addr + TENURE_CONFIG_REG_OFFSET);
}

static void cluster_txn_cntr_reset(struct cluster_pmu *cluster, u32 idx)
{
	cluster_txn_config_write(cluster, cluster_txn_config_read(cluster)
				| BIT(idx));
	cluster_txn_config_write(cluster, cluster_txn_config_read(cluster)
				& ~BIT(idx));
}

static void cluster_pmu_reset(struct cluster_pmu *cluster)
{
	cluster_txn_config_write(cluster, cluster_txn_config_read(cluster)
				| TXN_RESET_ALL_CNTR);
	cluster_txn_config_write(cluster, cluster_txn_config_read(cluster)
				& ~TXN_RESET_ALL_CNTR);
	cluster_txn_config_write(cluster, cluster_txn_config_read(cluster)
				| TXN_RESET_ALL_CNTR_OVSR_BIT);
	cluster_txn_config_write(cluster, cluster_txn_config_read(cluster)
				& ~TXN_RESET_ALL_CNTR_OVSR_BIT);
	cluster_tenure_config_write(cluster, cluster_tenure_config_read(cluster)
				| TENURE_RESET_ALL_CNTR);
	cluster_tenure_config_write(cluster, cluster_tenure_config_read(cluster)
				& ~TENURE_RESET_ALL_CNTR);
	cluster_tenure_config_write(cluster, cluster_tenure_config_read(cluster)
				| TENURE_RESET_OVERFLOW_ALL_CNTR);
	cluster_tenure_config_write(cluster, cluster_tenure_config_read(cluster)
				& ~TENURE_RESET_OVERFLOW_ALL_CNTR);
}

static void cluster_tenure_counter_reset(struct cluster_pmu *cluster, u32 idx)
{
	cluster_tenure_config_write(cluster, cluster_tenure_config_read(cluster)
				| BIT(idx));
	cluster_tenure_config_write(cluster, cluster_tenure_config_read(cluster)
				& ~BIT(idx));
}

static inline void cluster_tenure_counter_enable(struct cluster_pmu *cluster,
						u32 idx)
{
	u32 val;

	val = cluster_tenure_config_read(cluster);
	/* Already enabled */
	if (val & BIT(idx))
		return;

	switch (idx) {
	case LOW_RANGE_OCCURRENCE_CNTR_ENABLE:
		cluster_tenure_counter_reset(cluster,
					LOW_RANGE_OCCURRENCE_CNTR_RESET);
		break;

	case MID_RANGE_OCCURRENCE_CNTR_ENABLE:
		cluster_tenure_counter_reset(cluster,
					MID_RANGE_OCCURRENCE_CNTR_RESET);
		break;

	case HIGH_RANGE_OCCURRENCE_CNTR_ENABLE:
		cluster_tenure_counter_reset(cluster,
					HIGH_RANGE_OCCURRENCE_CNTR_RESET);
		break;

	default:
		pr_crit(L2_COUNTERS_BUG
			"Invalid index, during %s\n", __func__);
		return;
	}

	if (!(val & BIT(TENURE_CNTR_ENABLE))) {
		cluster_tenure_counter_reset(cluster,
					LPM_MODE_TENURE_CNTR_RESET);
		/*
		 * Enable tenure counter as a part of enablement of any
		 * occurrences counter, as occurrence counters would not
		 * increment unless tenure counter is enabled.
		 */
		cluster_tenure_config_write(cluster,
			cluster_tenure_config_read(cluster)
			| BIT(TENURE_CNTR_ENABLE));
	}

	cluster_tenure_config_write(cluster,
			cluster_tenure_config_read(cluster) | BIT(idx));
}

static inline void cluster_txn_counter_enable(struct cluster_pmu *cluster,
						u32 idx)
{
	u32 val;

	val = cluster_txn_config_read(cluster);
	if (val & BIT(idx))
		return;

	cluster_txn_cntr_reset(cluster, TXN_RESET_BIT + idx);
	cluster_txn_config_write(cluster, cluster_txn_config_read(cluster)
				| BIT(idx));
}

static inline void cluster_tenure_counter_disable(struct cluster_pmu *cluster,
						u32 idx)
{
	u32 val;

	cluster_tenure_config_write(cluster, cluster_tenure_config_read(cluster)
					& ~BIT(idx));
	val = cluster_tenure_config_read(cluster);
	if (!(val & OCCURRENCE_CNTR_ENABLE_MASK))
		cluster_tenure_config_write(cluster, val &
					~BIT(TENURE_CNTR_ENABLE));
}

static inline void cluster_txn_counter_disable(struct cluster_pmu *cluster,
						u32 idx)
{
	cluster_txn_config_write(cluster,
		cluster_txn_config_read(cluster) & ~BIT(idx));
}

static inline u32 cluster_reg_read(struct cluster_pmu *cluster, u32 offset)
{
	return readl_relaxed(cluster->reg_addr + offset);
}

static inline void cluster_tenure_cntr_reset_ovsr(struct cluster_pmu *cluster,
		u32 event_idx)
{
	switch (event_idx) {
	case LPM_TENURE_CNTR:
		cluster_tenure_counter_reset(cluster,
				LPM_MODE_TENURE_CNTR_OVERFLOW_RESET);
		break;

	case LOW_RANGE_OCCURRENCE_CNTR_ENABLE:
		cluster_tenure_counter_reset(cluster,
				LOW_RANGE_OCCURRENCE_CNTR_OVERFLOW_RESET);
		break;

	case MID_RANGE_OCCURRENCE_CNTR_ENABLE:
		cluster_tenure_counter_reset(cluster,
				MID_RANGE_OCCURRENCE_CNTR_OVERFLOW_RESET);
		break;

	case HIGH_RANGE_OCCURRENCE_CNTR_ENABLE:
		cluster_tenure_counter_reset(cluster,
				HIGH_RANGE_OCCURRENCE_CNTR_OVERFLOW_RESET);
		break;

	default:
		pr_crit(L2_COUNTERS_BUG
			"Invalid index, during %s\n", __func__);
	}
}

static inline void cluster_pmu_reset_ovsr(struct cluster_pmu *cluster,
		u32 config_base)
{
	u32 event_idx;
	u32 event_grp;

	event_idx = (config_base & REGBIT_MASK) >> REGBIT_SHIFT;
	event_grp = config_base & EVENT_GROUP_MASK;

	if (event_grp == TENURE_CNTRS_GROUP_ID)
		cluster_tenure_cntr_reset_ovsr(cluster, event_idx);
	else
		cluster_txn_cntr_reset(cluster,
			TXN_OVERFLOW_RESET_BIT + event_idx);
}

static inline bool cluster_pmu_has_overflowed(u32 ovsr)
{
	return !!(ovsr & l2_counter_present_mask);
}

static inline bool cluster_pmu_counter_has_overflowed(u32 ovsr, u32 idx)
{
	return !!(ovsr & BIT(idx));
}

static void l2_cache_event_update(struct perf_event *event, u32 ovsr)
{
	struct hw_perf_event *hwc = &event->hw;
	u64 delta, prev, now;
	u32 event_idx = hwc->config_base;
	u32 event_grp;
	struct cluster_pmu *cluster;

	prev = local64_read(&hwc->prev_count);
	if (ovsr) {
		now = 0xffffffff;
		goto out;
	}

	cluster = get_cluster_pmu(to_l2cache_pmu(event->pmu), event->cpu);
	event_idx = (hwc->config_base & REGBIT_MASK) >> REGBIT_SHIFT;
	event_grp = hwc->config_base & EVENT_GROUP_MASK;
	do {
		prev = local64_read(&hwc->prev_count);
		now = cluster_pmu_counter_get_value(cluster, event_idx,
				event_grp);
	} while (local64_cmpxchg(&hwc->prev_count, prev, now) != prev);

	/* All are 32-bit counters */
out:
	delta = now - prev;
	delta &= 0xffffffff;

	local64_add(delta, &event->count);
	if (ovsr)
		local64_set(&hwc->prev_count, 0);
}

static int l2_cache_get_event_idx(struct cluster_pmu *cluster,
				   struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx;
	int num_ctrs = cluster->l2cache_pmu->num_counters;

	idx = (hwc->config_base & ID_MASK) >> ID_SHIFT;
	if (idx >= num_ctrs)
		return -EINVAL;

	if (test_bit(idx, cluster->used_counters))
		return -EAGAIN;

	set_bit(idx, cluster->used_counters);
	return idx;
}

static void l2_cache_clear_event_idx(struct cluster_pmu *cluster,
				      struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	clear_bit(idx, cluster->used_counters);
}

static irqreturn_t l2_cache_handle_irq(int irq_num, void *data)
{
	struct cluster_pmu *cluster = data;
	int num_counters = cluster->l2cache_pmu->num_counters;
	u32 ovsr;
	int idx;
	u32 config_base;

	ovsr = cluster_reg_read(cluster, OVERFLOW_REG_OFFSET);
	if (!cluster_pmu_has_overflowed(ovsr))
		return IRQ_NONE;

	/*
	 * LPM tenure counter overflow would be a special case, although
	 * it would never happen, but for a ideal case we would reset
	 * it's overflow bit. I hope hardware takes care the overflow
	 * of tenure counter and its classifying category but even if
	 * it does not, we would get a extra count gets added
	 * erroneously to one of low/mid/high occurrence counter, but
	 * that is very rare and we can ignore it too.
	 */
	if (ovsr & BIT(LPM_TENURE_CNTR))
		cluster_tenure_cntr_reset_ovsr(cluster, LPM_TENURE_CNTR);

	spin_lock(&cluster->pmu_lock);
	for_each_set_bit(idx, cluster->used_counters, num_counters) {
		struct perf_event *event = cluster->events[idx];
		struct hw_perf_event *hwc;

		if (WARN_ON_ONCE(!event))
			continue;

		if (!cluster_pmu_counter_has_overflowed(ovsr, idx))
			continue;

		l2_cache_event_update(event, 1);
		hwc = &event->hw;
		config_base = hwc->config_base;
		cluster_pmu_reset_ovsr(cluster, config_base);
	}
	spin_unlock(&cluster->pmu_lock);
	return IRQ_HANDLED;
}

static int l2_cache_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct cluster_pmu *cluster;
	struct l2cache_pmu *l2cache_pmu;

	if (event->attr.type != event->pmu->type)
		return -ENOENT;

	l2cache_pmu = to_l2cache_pmu(event->pmu);

	if (hwc->sample_period) {
		dev_dbg_ratelimited(&l2cache_pmu->pdev->dev,
				    "Sampling not supported\n");
		return -EOPNOTSUPP;
	}

	if (event->cpu < 0) {
		dev_dbg_ratelimited(&l2cache_pmu->pdev->dev,
				    "Per-task mode not supported\n");
		return -EOPNOTSUPP;
	}

	/* We can not filter accurately so we just don't allow it. */
	if (event->attr.exclude_user || event->attr.exclude_kernel ||
	    event->attr.exclude_hv || event->attr.exclude_idle) {
		dev_dbg_ratelimited(&l2cache_pmu->pdev->dev,
				    "Can't exclude execution levels\n");
		return -EOPNOTSUPP;
	}

	cluster = get_cluster_pmu(l2cache_pmu, event->cpu);
	if (!cluster) {
		/* CPU has not been initialised */
		dev_dbg_ratelimited(&l2cache_pmu->pdev->dev,
			"CPU%d not associated with L2 cluster\n", event->cpu);
		return -EINVAL;
	}

	/* Ensure all events in a group are on the same cpu */
	if ((event->group_leader != event) &&
	    (cluster->on_cpu != event->group_leader->cpu)) {
		dev_dbg_ratelimited(&l2cache_pmu->pdev->dev,
			 "Can't create group on CPUs %d and %d",
			 event->cpu, event->group_leader->cpu);
		return -EINVAL;
	}

	hwc->idx = -1;
	hwc->config_base = event->attr.config;
	event->readable_on_cpus = CPU_MASK_ALL;

	/*
	 * We are overiding event->cpu, as it is possible to enable events,
	 * even if the event->cpu is offline.
	 */
	event->cpu = cluster->on_cpu;
	return 0;
}

static void l2_cache_event_start(struct perf_event *event, int flags)
{
	struct cluster_pmu *cluster;
	struct hw_perf_event *hwc = &event->hw;
	int event_idx;

	hwc->state = 0;
	cluster = get_cluster_pmu(to_l2cache_pmu(event->pmu), event->cpu);
	event_idx = (hwc->config_base & REGBIT_MASK) >> REGBIT_SHIFT;
	if ((hwc->config_base & EVENT_GROUP_MASK) == TENURE_CNTRS_GROUP_ID) {
		cluster_tenure_counter_enable(cluster, event_idx);
		return;
	}

	cluster_txn_counter_enable(cluster, event_idx);
}

static void l2_cache_event_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct cluster_pmu *cluster;
	int event_idx;
	u32 ovsr;

	cluster = get_cluster_pmu(to_l2cache_pmu(event->pmu), event->cpu);
	if (hwc->state & PERF_HES_STOPPED)
		return;

	event_idx = (hwc->config_base & REGBIT_MASK) >> REGBIT_SHIFT;
	if ((hwc->config_base & EVENT_GROUP_MASK) == TENURE_CNTRS_GROUP_ID)
		cluster_tenure_counter_disable(cluster, event_idx);
	else
		cluster_txn_counter_disable(cluster, event_idx);

	ovsr = cluster_reg_read(cluster, OVERFLOW_REG_OFFSET);
	if (cluster_pmu_counter_has_overflowed(ovsr, event_idx)) {
		l2_cache_event_update(event, 1);
		cluster_pmu_reset_ovsr(cluster, hwc->config_base);
	}

	if (flags & PERF_EF_UPDATE)
		l2_cache_event_update(event, 0);

	hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

static int l2_cache_event_add(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx;
	struct cluster_pmu *cluster;

	cluster = get_cluster_pmu(to_l2cache_pmu(event->pmu), event->cpu);
	idx = l2_cache_get_event_idx(cluster, event);
	if (idx < 0)
		return idx;

	hwc->idx = idx;
	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	cluster->events[idx] = event;
	local64_set(&hwc->prev_count, 0);

	if (flags & PERF_EF_START)
		l2_cache_event_start(event, flags);

	/* Propagate changes to the userspace mapping. */
	perf_event_update_userpage(event);

	return 0;
}

static void l2_cache_event_del(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct cluster_pmu *cluster;
	int idx = hwc->idx;
	unsigned long intr_flag;

	cluster = get_cluster_pmu(to_l2cache_pmu(event->pmu), event->cpu);

	/*
	 * We could race here with overflow interrupt of this event.
	 * So, let's be safe here.
	 */
	spin_lock_irqsave(&cluster->pmu_lock, intr_flag);
	l2_cache_event_stop(event, flags | PERF_EF_UPDATE);
	l2_cache_clear_event_idx(cluster, event);
	cluster->events[idx] = NULL;
	hwc->idx = -1;
	spin_unlock_irqrestore(&cluster->pmu_lock, intr_flag);

	perf_event_update_userpage(event);
}

static void l2_cache_event_read(struct perf_event *event)
{
	l2_cache_event_update(event, 0);
}

static ssize_t low_tenure_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct l2cache_pmu *l2cache_pmu = to_l2cache_pmu(dev_get_drvdata(dev));
	struct cluster_pmu *cluster = NULL;
	u32 val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val == 0 || val > INT_MAX)
		return -EINVAL;

	list_for_each_entry(cluster, &l2cache_pmu->clusters, next) {
		if (cluster->cluster_id == which_cluster_tenure)
			writel_relaxed(val,
				cluster->reg_addr + LOW_RANGE_TENURE_VAL);
	}

	return count;
}

static ssize_t low_tenure_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct l2cache_pmu *l2cache_pmu = to_l2cache_pmu(dev_get_drvdata(dev));
	struct cluster_pmu *cluster = NULL;
	u32 val = 0;

	list_for_each_entry(cluster, &l2cache_pmu->clusters, next) {
		if (cluster->cluster_id == which_cluster_tenure)
			val = cluster_reg_read(cluster, LOW_RANGE_TENURE_VAL);
	}

	return snprintf(buf, PAGE_SIZE, "0x%x\n", val);
}

static ssize_t mid_tenure_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct l2cache_pmu *l2cache_pmu = to_l2cache_pmu(dev_get_drvdata(dev));
	struct cluster_pmu *cluster = NULL;
	u32 val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val == 0 || val > INT_MAX)
		return -EINVAL;

	list_for_each_entry(cluster, &l2cache_pmu->clusters, next) {
		if (cluster->cluster_id == which_cluster_tenure)
			writel_relaxed(val,
				cluster->reg_addr + MID_RANGE_TENURE_VAL);
	}

	return count;
}

static ssize_t mid_tenure_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct l2cache_pmu *l2cache_pmu = to_l2cache_pmu(dev_get_drvdata(dev));
	struct cluster_pmu *cluster = NULL;
	u32 val = 0;

	list_for_each_entry(cluster, &l2cache_pmu->clusters, next) {
		if (cluster->cluster_id == which_cluster_tenure)
			val = cluster_reg_read(cluster, MID_RANGE_TENURE_VAL);
	}

	return snprintf(buf, PAGE_SIZE, "0x%x\n", val);
}

static ssize_t which_cluster_tenure_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", which_cluster_tenure);
}

static ssize_t which_cluster_tenure_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &which_cluster_tenure);
	if (ret < 0)
		return ret;

	if (which_cluster_tenure > 1)
		return -EINVAL;

	return count;
}

static struct device_attribute mid_tenure_threshold_attr =
	__ATTR(mid_tenure_threshold, 0644,
			mid_tenure_threshold_show,
			mid_tenure_threshold_store);

static struct attribute *mid_tenure_threshold_attrs[] = {
	&mid_tenure_threshold_attr.attr,
	NULL,
};

static struct attribute_group mid_tenure_threshold_group = {
	.attrs = mid_tenure_threshold_attrs,
};

static struct device_attribute low_tenure_threshold_attr =
	__ATTR(low_tenure_threshold, 0644,
			low_tenure_threshold_show,
			low_tenure_threshold_store);

static struct attribute *low_tenure_threshold_attrs[] = {
	&low_tenure_threshold_attr.attr,
	NULL,
};

static struct attribute_group low_tenure_threshold_group = {
	.attrs = low_tenure_threshold_attrs,
};

static struct device_attribute which_cluster_tenure_attr =
	__ATTR(which_cluster_tenure, 0644,
			which_cluster_tenure_show,
			which_cluster_tenure_store);

static struct attribute *which_cluster_tenure_attrs[] = {
	&which_cluster_tenure_attr.attr,
	NULL,
};

static struct attribute_group which_cluster_tenure_group = {
	.attrs = which_cluster_tenure_attrs,
};

static ssize_t l2_cache_pmu_cpumask_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct l2cache_pmu *l2cache_pmu = to_l2cache_pmu(dev_get_drvdata(dev));

	return cpumap_print_to_pagebuf(true, buf, &l2cache_pmu->cpumask);
}

static struct device_attribute l2_cache_pmu_cpumask_attr =
	__ATTR(cpumask, 0444, l2_cache_pmu_cpumask_show, NULL);


static struct attribute *l2_cache_pmu_cpumask_attrs[] = {
	&l2_cache_pmu_cpumask_attr.attr,
	NULL,
};

static struct attribute_group l2_cache_pmu_cpumask_group = {
	.attrs = l2_cache_pmu_cpumask_attrs,
};

PMU_FORMAT_ATTR(event,     "config:0-9");
static struct attribute *l2_cache_pmu_formats[] = {
	&format_attr_event.attr,
	NULL,
};

static struct attribute_group l2_cache_pmu_format_group = {
	.name = "format",
	.attrs = l2_cache_pmu_formats,
};

static ssize_t l2cache_pmu_event_show(struct device *dev,
				      struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);
	return snprintf(page, PAGE_SIZE, "event=0x%02llx\n", pmu_attr->id);
}

#define L2CACHE_EVENT_ATTR(_name, _id)					     \
	(&((struct perf_pmu_events_attr[]) {				     \
		{ .attr = __ATTR(_name, 0444, l2cache_pmu_event_show, NULL), \
		  .id = _id, }						     \
	})[0].attr.attr)

static struct attribute *l2_cache_pmu_events[] = {
	L2CACHE_EVENT_ATTR(cycles, L2_EVENT_CYCLE_CNTR),
	L2CACHE_EVENT_ATTR(ddr_write, L2_EVENT_DDR_WR_CNTR),
	L2CACHE_EVENT_ATTR(ddr_read, L2_EVENT_DDR_RD_CNTR),
	L2CACHE_EVENT_ATTR(snoop_read, L2_EVENT_SNP_RD_CNTR),
	L2CACHE_EVENT_ATTR(acp_write, L2_EVENT_ACP_WR_CNTR),
	L2CACHE_EVENT_ATTR(low_range_occur, L2_EVENT_LOW_RANGE_OCCUR_CNTR),
	L2CACHE_EVENT_ATTR(mid_range_occur, L2_EVENT_MID_RANGE_OCCUR_CNTR),
	L2CACHE_EVENT_ATTR(high_range_occur, L2_EVENT_HIGH_RANGE_OCCUR_CNTR),
	NULL
};

static struct attribute_group l2_cache_pmu_events_group = {
	.name = "events",
	.attrs = l2_cache_pmu_events,
};

static const struct attribute_group *l2_cache_pmu_attr_grps[] = {
	&l2_cache_pmu_format_group,
	&l2_cache_pmu_cpumask_group,
	&l2_cache_pmu_events_group,
	&mid_tenure_threshold_group,
	&low_tenure_threshold_group,
	&which_cluster_tenure_group,
	NULL,
};

static struct cluster_pmu *l2_cache_associate_cpu_with_cluster(
	struct l2cache_pmu *l2cache_pmu, int cpu)
{
	u64 mpidr;
	int cpu_cluster_id;
	struct cluster_pmu *cluster = NULL;

	/*
	 * This assumes that the cluster_id is in MPIDR[aff1] for
	 * single-threaded cores, and MPIDR[aff2] for multi-threaded
	 * cores. This logic will have to be updated if this changes.
	 */
	mpidr = read_cpuid_mpidr();
	if (mpidr & MPIDR_MT_BITMASK)
		cpu_cluster_id = MPIDR_AFFINITY_LEVEL(mpidr, 2);
	else
		cpu_cluster_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	list_for_each_entry(cluster, &l2cache_pmu->clusters, next) {
		if (cluster->cluster_id != cpu_cluster_id)
			continue;

		dev_info(&l2cache_pmu->pdev->dev,
			 "CPU%d associated with cluster %d\n", cpu,
			 cluster->cluster_id);
		cpumask_set_cpu(cpu, &cluster->cluster_cpus);
		*per_cpu_ptr(l2cache_pmu->pmu_cluster, cpu) = cluster;
		break;
	}

	return cluster;
}

static int l2cache_pmu_online_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct cluster_pmu *cluster;
	struct l2cache_pmu *l2cache_pmu;

	if (!node)
		return 0;

	l2cache_pmu = hlist_entry_safe(node, struct l2cache_pmu, node);
	cluster = get_cluster_pmu(l2cache_pmu, cpu);
	if (!cluster) {
		/* First time this CPU has come online */
		cluster = l2_cache_associate_cpu_with_cluster(l2cache_pmu, cpu);
		if (!cluster) {
			/* Only if broken firmware doesn't list every cluster */
			WARN_ONCE(1, "No L2 cache cluster for CPU%d\n", cpu);
			return 0;
		}
	}

	/* If another CPU is managing this cluster, we're done */
	if (cluster->on_cpu != -1)
		return 0;

	/*
	 * All CPUs on this cluster were down, use this one.
	 * Reset to put it into sane state.
	 */
	cluster->on_cpu = cpu;
	cpumask_set_cpu(cpu, &l2cache_pmu->cpumask);
	cluster_pmu_reset(cluster);

	enable_irq(cluster->irq);

	return 0;
}

static int l2cache_pmu_offline_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct cluster_pmu *cluster;
	struct l2cache_pmu *l2cache_pmu;
	cpumask_t cluster_online_cpus;
	unsigned int target;

	if (!node)
		return 0;

	l2cache_pmu = hlist_entry_safe(node, struct l2cache_pmu, node);
	cluster = get_cluster_pmu(l2cache_pmu, cpu);
	if (!cluster)
		return 0;

	/* If this CPU is not managing the cluster, we're done */
	if (cluster->on_cpu != cpu)
		return 0;

	/* Give up ownership of cluster */
	cpumask_clear_cpu(cpu, &l2cache_pmu->cpumask);
	cluster->on_cpu = -1;

	/* Any other CPU for this cluster which is still online */
	cpumask_and(&cluster_online_cpus, &cluster->cluster_cpus,
		    cpu_online_mask);
	target = cpumask_any_but(&cluster_online_cpus, cpu);
	if (target >= nr_cpu_ids) {
		disable_irq(cluster->irq);
		return 0;
	}

	perf_pmu_migrate_context(&l2cache_pmu->pmu, cpu, target);
	cluster->on_cpu = target;
	cpumask_set_cpu(target, &l2cache_pmu->cpumask);

	return 0;
}

static void l2_cache_pmu_dev_release(struct device *dev)
{
	struct cluster_pmu *cluster = to_cluster_device(dev);

	kfree(cluster);
}

static int l2_cache_pmu_probe_cluster(struct device *parent,
					struct device_node *cn, void *data)
{
	struct l2cache_pmu *l2cache_pmu = data;
	struct cluster_pmu *cluster;
	u32 fw_cluster_id;
	struct resource res;
	int ret;
	int irq;

	cluster = kzalloc(sizeof(*cluster), GFP_KERNEL);
	if (!cluster) {
		ret = -ENOMEM;
		return ret;
	}

	cluster->dev.parent = parent;
	cluster->dev.of_node = cn;
	cluster->dev.release = l2_cache_pmu_dev_release;
	dev_set_name(&cluster->dev, "%s:%s", dev_name(parent), cn->name);

	ret = device_register(&cluster->dev);
	if (ret) {
		pr_err(L2_COUNTERS_BUG
			"failed to register l2 cache pmu device\n");
		goto err_put_dev;
	}

	ret = of_property_read_u32(cn, "cluster-id", &fw_cluster_id);
	if (ret) {
		pr_err(L2_COUNTERS_BUG "Missing cluster-id.\n");
		goto err_put_dev;
	}

	ret = of_address_to_resource(cn, 0, &res);
	if (ret) {
		pr_err(L2_COUNTERS_BUG "not able to find the resource\n");
		goto err_put_dev;
	}

	cluster->reg_addr = devm_ioremap_resource(&cluster->dev, &res);
	if (IS_ERR(cluster->reg_addr)) {
		ret = PTR_ERR(cluster->reg_addr);
		pr_err(L2_COUNTERS_BUG "not able to remap the resource\n");
		goto err_put_dev;
	}

	INIT_LIST_HEAD(&cluster->next);
	cluster->cluster_id = fw_cluster_id;
	cluster->l2cache_pmu = l2cache_pmu;

	irq = of_irq_get(cn, 0);
	if (irq < 0) {
		pr_err(L2_COUNTERS_BUG
			"Failed to get valid irq for cluster %ld\n",
			fw_cluster_id);
		goto err_put_dev;
	}

	irq_set_status_flags(irq, IRQ_NOAUTOEN);
	cluster->irq = irq;
	cluster->on_cpu = -1;

	ret = devm_request_irq(&cluster->dev, irq, l2_cache_handle_irq,
			       IRQF_NOBALANCING | IRQF_NO_THREAD,
			       "l2-cache-pmu", cluster);
	if (ret) {
		pr_err(L2_COUNTERS_BUG
			"Unable to request IRQ%d for L2 PMU counters\n", irq);
		goto err_put_dev;
	}

	pr_info(L2_COUNTERS_BUG
		"Registered L2 cache PMU cluster %ld\n", fw_cluster_id);

	spin_lock_init(&cluster->pmu_lock);
	list_add(&cluster->next, &l2cache_pmu->clusters);
	l2cache_pmu->num_pmus++;

	return 0;

err_put_dev:
	put_device(&cluster->dev);
	return ret;
}

static int l2_cache_pmu_probe(struct platform_device *pdev)
{
	int err;
	struct l2cache_pmu *l2cache_pmu;
	struct device_node *pn = pdev->dev.of_node;
	struct device_node *cn;

	l2cache_pmu =
		devm_kzalloc(&pdev->dev, sizeof(*l2cache_pmu), GFP_KERNEL);
	if (!l2cache_pmu)
		return -ENOMEM;

	INIT_LIST_HEAD(&l2cache_pmu->clusters);
	platform_set_drvdata(pdev, l2cache_pmu);
	l2cache_pmu->pmu = (struct pmu) {
		.name		= "l2cache_counters",
		.task_ctx_nr    = perf_invalid_context,
		.event_init	= l2_cache_event_init,
		.add		= l2_cache_event_add,
		.del		= l2_cache_event_del,
		.start		= l2_cache_event_start,
		.stop		= l2_cache_event_stop,
		.read		= l2_cache_event_read,
		.attr_groups	= l2_cache_pmu_attr_grps,
	};

	l2cache_pmu->num_counters = MAX_L2_CNTRS;
	l2cache_pmu->pdev = pdev;
	l2cache_pmu->pmu_cluster = devm_alloc_percpu(&pdev->dev,
						     struct cluster_pmu *);
	if (!l2cache_pmu->pmu_cluster)
		return -ENOMEM;

	l2_counter_present_mask = GENMASK(l2cache_pmu->num_counters - 1, 0);
	cpumask_clear(&l2cache_pmu->cpumask);

	for_each_available_child_of_node(pn, cn) {
		err = l2_cache_pmu_probe_cluster(&pdev->dev, cn, l2cache_pmu);
		if (err < 0) {
			of_node_put(cn);
			dev_err(&pdev->dev,
				"No hardware L2 cache PMUs found\n");
			return err;
		}
	}

	if (l2cache_pmu->num_pmus == 0) {
		dev_err(&pdev->dev, "No hardware L2 cache PMUs found\n");
		return -ENODEV;
	}

	err = cpuhp_state_add_instance(CPUHP_AP_PERF_ARM_QCOM_L2_ONLINE,
				       &l2cache_pmu->node);
	if (err) {
		dev_err(&pdev->dev, "Error %d registering hotplug\n", err);
		return err;
	}

	err = perf_pmu_register(&l2cache_pmu->pmu, l2cache_pmu->pmu.name, -1);
	if (err) {
		dev_err(&pdev->dev, "Error %d registering L2 cache PMU\n", err);
		goto out_unregister;
	}

	dev_info(&pdev->dev, "Registered L2 cache PMU using %d HW PMUs\n",
		 l2cache_pmu->num_pmus);

	return 0;

out_unregister:
	cpuhp_state_remove_instance(CPUHP_AP_PERF_ARM_QCOM_L2_ONLINE,
				    &l2cache_pmu->node);
	return err;
}

static int l2cache_pmu_unregister_device(struct device *dev, void *data)
{
	device_unregister(dev);
	return 0;
}

static int l2_cache_pmu_remove(struct platform_device *pdev)
{
	struct l2cache_pmu *l2cache_pmu = platform_get_drvdata(pdev);
	int ret;

	perf_pmu_unregister(&l2cache_pmu->pmu);
	cpuhp_state_remove_instance(CPUHP_AP_PERF_ARM_QCOM_L2_ONLINE,
			&l2cache_pmu->node);

	ret = device_for_each_child(&pdev->dev, NULL,
			l2cache_pmu_unregister_device);
	if (ret)
		dev_warn(&pdev->dev,
			"can't remove cluster pmu device: %d\n", ret);
	return ret;
}

static const struct of_device_id l2_cache_pmu_of_match[] = {
	{ .compatible = "qcom,l2cache-pmu" },
	{},
};

static struct platform_driver l2_cache_pmu_driver = {
	.driver = {
		.name = "l2cache-pmu",
		.of_match_table = l2_cache_pmu_of_match,
	},
	.probe = l2_cache_pmu_probe,
	.remove = l2_cache_pmu_remove,
};

static int __init register_l2_cache_pmu_driver(void)
{
	int err;

	err = cpuhp_setup_state_multi(CPUHP_AP_PERF_ARM_QCOM_L2_ONLINE,
				      "AP_PERF_ARM_QCOM_L2_ONLINE",
				      l2cache_pmu_online_cpu,
				      l2cache_pmu_offline_cpu);
	if (err)
		return err;

	return platform_driver_register(&l2_cache_pmu_driver);
}
device_initcall(register_l2_cache_pmu_driver);
