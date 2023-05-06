// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

/*
 * This driver adds support for perf events to use the Performance
 * Monitor Counter Groups (PMCG) associated with an SMMU node
 * to monitor that node.
 *
 * Devices are named smmu_0_<phys_addr_page> where <phys_addr_page>
 * is the physical page address of the SMMU PMCG.
 * For example, the SMMU PMCG at 0xff88840000 is named smmu_0_ff88840
 *
 * Filtering by stream id is done by specifying filtering parameters
 * with the event. options are:
 *   filter_enable    - 0 = no filtering, 1 = filtering enabled
 *   filter_stream_id - stream id  to filter against
 * Further filtering information is available in the SMMU documentation.
 *
 * Example: perf stat -e smmu_0_ff88840/transaction,filter_enable=1,
 *                       filter_stream_id=0x42/ -a pwd
 * Applies filter pattern 0x42 to transaction events.
 *
 * SMMU events are not attributable to a CPU, so task mode and sampling
 * are not supported.
 */

#include <linux/bitops.h>
#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/msi.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/local64.h>

#define SMMU_PMCG_EVCNTR0               0x0
#define SMMU_PMCG_EVCNTR(n, stride)     (SMMU_PMCG_EVCNTR0 + (n) * (stride))
#define SMMU_PMCG_EVTYPER0              0x400
#define SMMU_PMCG_EVTYPER(n)            (SMMU_PMCG_EVTYPER0 + (n) * 4)
#define SMMU_PMCG_EVTYPER_EVENT_MASK          GENMASK(15, 0)
#define SMMU_PMCG_SMR0                  0xA00
#define SMMU_PMCG_SMR(n)		(SMMU_PMCG_SMR0 + (n) * 4)
#define SMMU_PMCG_CGCR0			0x800
#define SMMU_PMCG_CGCR(n)		(SMMU_PMCG_CGCR0 + (n) * 4)
#define SMMU_PMCG_CNTENSET0             0xC00
#define SMMU_PMCG_CNTENCLR0             0xC20
#define SMMU_PMCG_INTENSET0             0xC40
#define SMMU_PMCG_INTENCLR0             0xC60
#define SMMU_PMCG_OVSCLR0               0xC80
#define SMMU_PMCG_OVSSET0               0xCC0
#define SMMU_PMCG_CFGR                  0xE00
#define SMMU_PMCG_CFGR_SIZE_MASK              GENMASK(13, 8)
#define SMMU_PMCG_CFGR_SIZE_SHIFT             8
#define SMMU_PMCG_CFGR_COUNTER_SIZE_32        31
#define SMMU_PMCG_CFGR_NCTR_MASK              GENMASK(7, 0)
#define SMMU_PMCG_CFGR_NCTR_SHIFT             0
#define SMMU_PMCG_CFGR_NCTRGRP_MASK	      GENMASK(31, 24)
#define SMMU_PMCG_CFGR_NCTRGRP_SHIFT	      24
#define SMMU_PMCG_CR                    0xE04
#define SMMU_PMCG_CR_ENABLE                   BIT(0)
#define SMMU_PMCG_CEID0                 0xE20
#define SMMU_STATS_CFG                 0x84

#define SMMU_COUNTER_RELOAD             BIT(31)
#define SMMU_DEFAULT_FILTER_STREAM_ID   GENMASK(31, 0)

#define SMMU_MAX_COUNTERS               256
#define SMMU_MAX_EVENT_ID               32
#define SMMU_MAX_CEIDS			2

#define SMMU_PA_SHIFT                   12
#define SMMU_STREAM_ID_FILTER		8
#define SMMU_CGCR_ENABLE		11

/* Events */
#define SMMU_PMU_CYCLES                 0x0
#define SMMU_PMU_CYCLES_64		0x1
#define SMMU_PMU_TLB_ALLOCATE		0x8
#define SMMU_PMU_TLB_READ		0x9
#define SMMU_PMU_TLB_WRITE		0xA
#define SMMU_PMU_ACCESS			0x10
#define SMMU_PMU_READ_ACCESS		0x11
#define SMMU_PMU_WRITE_ACCESS		0x12

#define SMMU_STATS_START		0x80
#define SMMU_STATS_MTLB_LOOKUP_CNTR		0x88
#define SMMU_STATS_WC1_LOOKUP_CNTR		0x90
#define SMMU_STATS_WC2_LOOKUP_CNTR		0x94
#define SMMU_STATS_I0_LOOKUP_CNTR		0x98
#define SMMU_STATS_I1_LOOKUP_CNTR		0x9C
#define SMMU_STATS_I2_LOOKUP_CNTR		0xA0
#define SMMU_STATS_I3_LOOKUP_CNTR		0xA4
#define SMMU_STATS_I4_LOOKUP_CNTR		0xA8
#define SMMU_STATS_MTLB_HIT_CNTR		0xAC
#define SMMU_STATS_PFB_HIT_CNTR			0xB0
#define SMMU_STATS_WC1_HIT_CNTR			0xB4
#define SMMU_STATS_WC2_HIT_CNTR			0xB8
#define SMMU_STATS_I0_HIT_CNTR			0xBC
#define SMMU_STATS_I1_HIT_CNTR			0xC0
#define SMMU_STATS_I2_HIT_CNTR			0xC4
#define SMMU_STATS_I3_HIT_CNTR			0xC8
#define SMMU_STATS_I4_HIT_CNTR			0xCC
#define SMMU_STATS_PTW_CNTR				0xD0
#define SMMU_STATS_PTWQ_2BY4_1BY4_CNTR	0xD4
#define SMMU_STATS_PTWQ_4BY4_3BY4_CNTR	0xD8

static int cpuhp_state_num;

struct smmu_pmu {
	struct hlist_node node;
	struct perf_event *events[SMMU_MAX_COUNTERS];
	DECLARE_BITMAP(used_counters, SMMU_MAX_COUNTERS);
	DECLARE_BITMAP(supported_events, SMMU_MAX_EVENT_ID);
	unsigned int num_irqs;
	unsigned int *irqs;
	unsigned int on_cpu;
	struct pmu pmu;
	unsigned int num_counters;
	unsigned int num_countergroups;
	struct platform_device *pdev;
	void __iomem *reg_base;
	void __iomem *tcu_base;
	u64 counter_present_mask;
	u64 counter_mask;
	bool reg_size_32;
};

#define to_smmu_pmu(p) (container_of(p, struct smmu_pmu, pmu))

#define SMMU_PMU_EVENT_ATTR_EXTRACTOR(_name, _config, _size, _shift)    \
	static inline u32 get_##_name(struct perf_event *event)         \
	{                                                               \
		return (event->attr._config >> (_shift)) &              \
			GENMASK_ULL((_size) - 1, 0);                    \
	}

SMMU_PMU_EVENT_ATTR_EXTRACTOR(event, config, 16, 0);
SMMU_PMU_EVENT_ATTR_EXTRACTOR(filter_stream_id, config1, 32, 0);
SMMU_PMU_EVENT_ATTR_EXTRACTOR(tbu, config1, 3, 35);
SMMU_PMU_EVENT_ATTR_EXTRACTOR(filter_enable, config1, 1, 34);

static inline void smmu_pmu_enable(struct pmu *pmu)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(pmu);

	writel_relaxed(SMMU_PMCG_CR_ENABLE, smmu_pmu->reg_base + SMMU_PMCG_CR);
}

static inline void smmu_pmu_disable(struct pmu *pmu)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(pmu);

	writel_relaxed(0, smmu_pmu->reg_base + SMMU_PMCG_CR);
}

static inline void smmu_pmu_counter_set_value(struct smmu_pmu *smmu_pmu,
					      u32 idx, u64 value)
{
	if (smmu_pmu->reg_size_32)
		writel_relaxed(value, smmu_pmu->reg_base +
						SMMU_PMCG_EVCNTR(idx, 4));
	else
		writeq_relaxed(value, smmu_pmu->reg_base +
						SMMU_PMCG_EVCNTR(idx, 8));
}

static inline u64 smmu_pmu_counter_get_value(struct smmu_pmu *smmu_pmu, u32 idx)
{
	u64 value;

	if (smmu_pmu->reg_size_32)
		value = readl_relaxed(smmu_pmu->reg_base +
						SMMU_PMCG_EVCNTR(idx, 4));
	else
		value = readq_relaxed(smmu_pmu->reg_base +
						SMMU_PMCG_EVCNTR(idx, 8));

	return value;
}

static inline void smmu_pmu_counter_enable(struct smmu_pmu *smmu_pmu, u32 idx)
{
	writel_relaxed(BIT(idx), smmu_pmu->reg_base + SMMU_PMCG_CNTENSET0);
}

static inline void smmu_pmu_counter_disable(struct smmu_pmu *smmu_pmu, u32 idx)
{
	writel_relaxed(BIT(idx), smmu_pmu->reg_base + SMMU_PMCG_CNTENCLR0);
}

static inline void smmu_pmu_interrupt_enable(struct smmu_pmu *smmu_pmu, u32 idx)
{
	writel_relaxed(BIT(idx), smmu_pmu->reg_base + SMMU_PMCG_INTENSET0);
}

static inline void smmu_pmu_interrupt_disable(struct smmu_pmu *smmu_pmu,
					      u32 idx)
{
	writel_relaxed(BIT(idx), smmu_pmu->reg_base + SMMU_PMCG_INTENCLR0);
}

static void smmu_pmu_reset(struct smmu_pmu *smmu_pmu)
{
	unsigned int i;

	for (i = 0; i < smmu_pmu->num_counters; i++) {
		smmu_pmu_counter_disable(smmu_pmu, i);
		smmu_pmu_interrupt_disable(smmu_pmu, i);
	}
	smmu_pmu_disable(&smmu_pmu->pmu);
}

static inline void smmu_pmu_set_evtyper(struct smmu_pmu *smmu_pmu, u32 idx,
					u32 val)
{
	writel_relaxed(val, smmu_pmu->reg_base + SMMU_PMCG_EVTYPER(idx));
}

static inline void smmu_pmu_set_smr(struct smmu_pmu *smmu_pmu, u32 idx, u32 val)
{
	writel_relaxed(val, smmu_pmu->reg_base + SMMU_PMCG_SMR(idx));
}

static inline void smmu_pmu_enable_cgcr(struct smmu_pmu *smmu_pmu,
							u32 idx)
{
	writel_relaxed(BIT(SMMU_CGCR_ENABLE),
			smmu_pmu->reg_base + SMMU_PMCG_CGCR(idx));
}

static inline void smmu_pmu_disable_cgcr(struct smmu_pmu *smmu_pmu,
							u32 idx)
{
	writel_relaxed(0, smmu_pmu->reg_base + SMMU_PMCG_CGCR(idx));
}

static inline void smmu_pmu_set_cgcr_filtersid(struct smmu_pmu *smmu_pmu,
							u32 idx)
{
	writel_relaxed(BIT(SMMU_STREAM_ID_FILTER),
			smmu_pmu->reg_base + SMMU_PMCG_CGCR(idx));
}

static inline u64 smmu_pmu_getreset_ovsr(struct smmu_pmu *smmu_pmu)
{
	u64 result = readl_relaxed(smmu_pmu->reg_base + SMMU_PMCG_OVSSET0);

	writel_relaxed(result, smmu_pmu->reg_base + SMMU_PMCG_OVSCLR0);
	return result;
}

static inline bool smmu_pmu_has_overflowed(struct smmu_pmu *smmu_pmu, u64 ovsr)
{
	return !!(ovsr & smmu_pmu->counter_present_mask);
}

static void smmu_pmu_event_update(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	u64 delta, prev, now, event_id;
	u32 idx = hwc->idx;

	event_id = get_event(event);
	do {
		prev = local64_read(&hwc->prev_count);
		if (event_id >= SMMU_STATS_START)
			now = readl_relaxed((void *)(smmu_pmu->tcu_base + event_id));
		else
			now = smmu_pmu_counter_get_value(smmu_pmu, idx);
	} while (local64_cmpxchg(&hwc->prev_count, prev, now) != prev);

	/* handle overflow. */
	delta = now - prev;
	delta &= smmu_pmu->counter_mask;

	local64_add(delta, &event->count);
}

static void smmu_pmu_set_period(struct smmu_pmu *smmu_pmu,
				struct hw_perf_event *hwc)
{
	u32 idx = hwc->idx;
	u64 new;
	u32 event_id;
	struct perf_event *event = smmu_pmu->events[idx];

	/*
	 * We limit the max period to half the max counter value of the smallest
	 * counter size, so that even in the case of extreme interrupt latency
	 * the counter will (hopefully) not wrap past its initial value.
	 */
	event_id = get_event(event);
	new = SMMU_COUNTER_RELOAD;
	if (event_id >= SMMU_STATS_START) {
		new = readl_relaxed((void *)(smmu_pmu->tcu_base + event_id));
		local64_set(&hwc->prev_count, new);
		return;
	}
	local64_set(&hwc->prev_count, new);
	smmu_pmu_counter_set_value(smmu_pmu, idx, new);
	/* Ensure the SMMU_COUNTER_RELOAD value is written to the register */
	wmb();
}

static irqreturn_t smmu_pmu_handle_irq(int irq_num, void *data)
{
	struct smmu_pmu *smmu_pmu = data;
	u64 ovsr;
	unsigned int idx;

	ovsr = smmu_pmu_getreset_ovsr(smmu_pmu);
	if (!smmu_pmu_has_overflowed(smmu_pmu, ovsr))
		return IRQ_NONE;

	for_each_set_bit(idx, (unsigned long *)&ovsr, smmu_pmu->num_counters) {
		struct perf_event *event = smmu_pmu->events[idx];
		struct hw_perf_event *hwc;
		u32 evtyper;

		evtyper = get_event(event);
		if (!event)
			continue;

		smmu_pmu_event_update(event);
		hwc = &event->hw;
		smmu_pmu_set_period(smmu_pmu, hwc);
		dev_dbg(&smmu_pmu->pdev->dev, "Overflow on event : %d\n", evtyper);
	}
	return IRQ_HANDLED;
}

static unsigned int smmu_pmu_get_event_idx(struct smmu_pmu *smmu_pmu, int tbu)
{
	unsigned int idx, offset;
	unsigned int num_ctrs = smmu_pmu->num_counters;
	int counters_per_tbu = (int)(smmu_pmu->num_counters / smmu_pmu->num_countergroups);

	offset = tbu * counters_per_tbu;
	idx = find_next_zero_bit(smmu_pmu->used_counters, num_ctrs, offset);

	if (idx >= offset + counters_per_tbu) {
		dev_err(&smmu_pmu->pdev->dev, "All counters in use for tbu : %d\n", tbu);
		return -EAGAIN;
	}

	set_bit(idx, smmu_pmu->used_counters);

	return idx;
}

/*
 * Implementation of abstract pmu functionality required by
 * the core perf events code.
 */

static int smmu_pmu_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct perf_event *sibling;
	struct smmu_pmu *smmu_pmu;
	u32 event_id;

	if (event->attr.type != event->pmu->type)
		return -ENOENT;

	smmu_pmu = to_smmu_pmu(event->pmu);

	if (hwc->sample_period) {
		dev_dbg_ratelimited(&smmu_pmu->pdev->dev,
				    "Sampling not supported\n");
		return -EOPNOTSUPP;
	}

	if (event->cpu < 0) {
		dev_dbg_ratelimited(&smmu_pmu->pdev->dev,
				    "Per-task mode not supported\n");
		return -EOPNOTSUPP;
	}

	/* We cannot filter accurately so we just don't allow it. */
	if (event->attr.exclude_user || event->attr.exclude_kernel ||
	    event->attr.exclude_hv || event->attr.exclude_idle) {
		dev_dbg_ratelimited(&smmu_pmu->pdev->dev,
				    "Can't exclude execution levels\n");
		return -EOPNOTSUPP;
	}

	/* Verify specified event is supported on this PMU */
	event_id = get_event(event);

	/* Don't allow groups with mixed PMUs, except for s/w events */
	if (event->group_leader->pmu != event->pmu &&
	    !is_software_event(event->group_leader)) {
		dev_dbg_ratelimited(&smmu_pmu->pdev->dev,
			 "Can't create mixed PMU group\n");
		return -EINVAL;
	}

	list_for_each_entry(sibling, &event->group_leader->sibling_list,
			    sibling_list)
		if (sibling->pmu != event->pmu &&
		    !is_software_event(sibling)) {
			dev_dbg_ratelimited(&smmu_pmu->pdev->dev,
				 "Can't create mixed PMU group\n");
			return -EINVAL;
		}

	/* Ensure all events in a group are on the same cpu */
	if ((event->group_leader != event) &&
	    (event->cpu != event->group_leader->cpu)) {
		dev_dbg_ratelimited(&smmu_pmu->pdev->dev,
			 "Can't create group on CPUs %d and %d\n",
			 event->cpu, event->group_leader->cpu);
		return -EINVAL;
	}

	hwc->idx = -1;

	/*
	 * Ensure all events are on the same cpu so all events are in the
	 * same cpu context, to avoid races on pmu_enable etc.
	 */
	event->cpu = smmu_pmu->on_cpu;

	return 0;
}

static void smmu_pmu_event_start(struct perf_event *event, int flags)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	u32 event_id;
	u32 filter_stream_id;
	int counters_per_tbu = (int)(smmu_pmu->num_counters / smmu_pmu->num_countergroups);

	hwc->state = 0;

	event_id = get_event(event);

	if (event_id >= SMMU_STATS_START) {
		writel_relaxed(1, (void *)(smmu_pmu->tcu_base + SMMU_STATS_CFG));
		writel_relaxed(0, (void *)(smmu_pmu->tcu_base + SMMU_STATS_CFG));
		smmu_pmu_set_period(smmu_pmu, hwc);
		return;
	}
	smmu_pmu_set_period(smmu_pmu, hwc);
	smmu_pmu_enable_cgcr(smmu_pmu, (int)(idx / counters_per_tbu));

	if (get_filter_enable(event)) {
		smmu_pmu_set_cgcr_filtersid(smmu_pmu, (int)(idx / counters_per_tbu));
		filter_stream_id = get_filter_stream_id(event);
	} else {
		filter_stream_id = SMMU_DEFAULT_FILTER_STREAM_ID;
	}

	smmu_pmu_set_evtyper(smmu_pmu, idx, event_id);
	smmu_pmu_set_smr(smmu_pmu, (int)(idx / counters_per_tbu), filter_stream_id);
	smmu_pmu_interrupt_enable(smmu_pmu, idx);
	smmu_pmu_counter_enable(smmu_pmu, idx);
}

static void smmu_pmu_event_stop(struct perf_event *event, int flags)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	int counters_per_tbu = (int)(smmu_pmu->num_counters / smmu_pmu->num_countergroups);
	u32 event_id;

	if (hwc->state & PERF_HES_STOPPED)
		return;

	event_id = get_event(event);

	if (event_id < SMMU_STATS_START) {
		smmu_pmu_interrupt_disable(smmu_pmu, idx);
		smmu_pmu_counter_disable(smmu_pmu, idx);
		smmu_pmu_disable_cgcr(smmu_pmu, (int)(idx / counters_per_tbu));
	}

	if (flags & PERF_EF_UPDATE)
		smmu_pmu_event_update(event);
	hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
	if (event_id >= SMMU_STATS_START)
		writel_relaxed(2, (void *)(smmu_pmu->tcu_base + SMMU_STATS_CFG));
}

static int smmu_pmu_event_add(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx, tbu;
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	u32 event_id;

	event_id = get_event(event);
	if (event_id < SMMU_STATS_START) {
		tbu = get_tbu(event);
		idx = smmu_pmu_get_event_idx(smmu_pmu, tbu);
		if (idx < 0)
			return idx;
	} else
		idx = event_id;

	hwc->idx = idx;
	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	smmu_pmu->events[idx] = event;
	local64_set(&hwc->prev_count, 0);

	if (flags & PERF_EF_START)
		smmu_pmu_event_start(event, flags);

	/* Propagate changes to the userspace mapping. */
	perf_event_update_userpage(event);

	return 0;
}

static void smmu_pmu_event_del(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	int idx = hwc->idx;

	smmu_pmu_event_stop(event, flags | PERF_EF_UPDATE);
	smmu_pmu->events[idx] = NULL;
	clear_bit(idx, smmu_pmu->used_counters);

	perf_event_update_userpage(event);
}

static void smmu_pmu_event_read(struct perf_event *event)
{
	smmu_pmu_event_update(event);
}

/* cpumask */

static ssize_t smmu_pmu_cpumask_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(dev_get_drvdata(dev));

	return cpumap_print_to_pagebuf(true, buf, cpumask_of(smmu_pmu->on_cpu));
}

static struct device_attribute smmu_pmu_cpumask_attr =
		__ATTR(cpumask, 0444, smmu_pmu_cpumask_show, NULL);

static struct attribute *smmu_pmu_cpumask_attrs[] = {
	&smmu_pmu_cpumask_attr.attr,
	NULL,
};

static struct attribute_group smmu_pmu_cpumask_group = {
	.attrs = smmu_pmu_cpumask_attrs,
};

/* Events */

ssize_t smmu_pmu_event_show(struct device *dev,
			    struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);

	return scnprintf(page, 15, "event=0x%02llx\n", pmu_attr->id);
}

#define SMMU_EVENT_ATTR(_name, _id)					  \
	(&((struct perf_pmu_events_attr[]) {				  \
		{ .attr = __ATTR(_name, 0444, smmu_pmu_event_show, NULL), \
		  .id = _id, }						  \
	})[0].attr.attr)

static struct attribute *smmu_pmu_events[] = {
	SMMU_EVENT_ATTR(smmu_cycles, SMMU_PMU_CYCLES),
	SMMU_EVENT_ATTR(smmu_cycles_64, SMMU_PMU_CYCLES_64),
	SMMU_EVENT_ATTR(smmu_tlb_allocate, SMMU_PMU_TLB_ALLOCATE),
	SMMU_EVENT_ATTR(smmu_tlb_read, SMMU_PMU_TLB_READ),
	SMMU_EVENT_ATTR(smmu_tlb_write, SMMU_PMU_TLB_WRITE),
	SMMU_EVENT_ATTR(smmu_access, SMMU_PMU_ACCESS),
	SMMU_EVENT_ATTR(smmu_read_access, SMMU_PMU_READ_ACCESS),
	SMMU_EVENT_ATTR(smmu_write_access, SMMU_PMU_WRITE_ACCESS),
	SMMU_EVENT_ATTR(smmu_stats_mtlb_lookup_cntr, SMMU_STATS_MTLB_LOOKUP_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_wc1_lookup_cntr, SMMU_STATS_WC1_LOOKUP_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_wc2_lookup_cntr, SMMU_STATS_WC2_LOOKUP_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i0_lookup_cntr, SMMU_STATS_I0_LOOKUP_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i1_lookup_cntr, SMMU_STATS_I1_LOOKUP_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i2_lookup_cntr, SMMU_STATS_I2_LOOKUP_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i3_lookup_cntr, SMMU_STATS_I3_LOOKUP_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i4_lookup_cntr, SMMU_STATS_I4_LOOKUP_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_mtlb_hit_cntr, SMMU_STATS_MTLB_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_pfb_hit_cntr, SMMU_STATS_PFB_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_wc1_hit_cntr, SMMU_STATS_WC1_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_wc2_hit_cntr, SMMU_STATS_WC2_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i0_hit_cntr, SMMU_STATS_I0_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i1_hit_cntr, SMMU_STATS_I1_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i2_hit_cntr, SMMU_STATS_I2_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i3_hit_cntr, SMMU_STATS_I3_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_i4_hit_cntr, SMMU_STATS_I4_HIT_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_ptw_cntr, SMMU_STATS_PTW_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_ptwq_2by4_1by4_cntr, SMMU_STATS_PTWQ_2BY4_1BY4_CNTR),
	SMMU_EVENT_ATTR(smmu_stats_ptwq_4by4_3by4_cntr, SMMU_STATS_PTWQ_4BY4_3BY4_CNTR),
	NULL
};

static umode_t smmu_pmu_event_is_visible(struct kobject *kobj,
					 struct attribute *attr, int unused)
{
	struct device *dev = kobj_to_dev(kobj);
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(dev_get_drvdata(dev));
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr.attr);

	if (pmu_attr->id >= SMMU_STATS_START)
		return attr->mode;

	if (test_bit(pmu_attr->id, smmu_pmu->supported_events))
		return attr->mode;

	return 0;
}
static struct attribute_group smmu_pmu_events_group = {
	.name = "events",
	.attrs = smmu_pmu_events,
	.is_visible = smmu_pmu_event_is_visible,
};

/* Formats */
PMU_FORMAT_ATTR(event,		   "config:0-15");
PMU_FORMAT_ATTR(filter_stream_id,  "config1:0-31");
PMU_FORMAT_ATTR(tbu,  "config1:35-37");
PMU_FORMAT_ATTR(filter_enable,	   "config1:34");

static struct attribute *smmu_pmu_formats[] = {
	&format_attr_event.attr,
	&format_attr_filter_stream_id.attr,
	&format_attr_tbu.attr,
	&format_attr_filter_enable.attr,
	NULL
};

static struct attribute_group smmu_pmu_format_group = {
	.name = "format",
	.attrs = smmu_pmu_formats,
};

static const struct attribute_group *smmu_pmu_attr_grps[] = {
	&smmu_pmu_cpumask_group,
	&smmu_pmu_events_group,
	&smmu_pmu_format_group,
	NULL,
};

/*
 * Generic device handlers
 */

static unsigned int get_num_counters(struct smmu_pmu *smmu_pmu)
{
	u32 cfgr = readl_relaxed(smmu_pmu->reg_base + SMMU_PMCG_CFGR);

	return ((cfgr & SMMU_PMCG_CFGR_NCTR_MASK) >> SMMU_PMCG_CFGR_NCTR_SHIFT)
		+ 1;
}

static unsigned int get_num_countergroups(struct smmu_pmu *smmu_pmu)
{
	u32 cfgr = readl_relaxed(smmu_pmu->reg_base + SMMU_PMCG_CFGR);

	return ((cfgr & SMMU_PMCG_CFGR_NCTRGRP_MASK) >> SMMU_PMCG_CFGR_NCTRGRP_SHIFT)
		+ 1;
}

static int smmu_pmu_offline_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct smmu_pmu *smmu_pmu;
	unsigned int target;
	int i;

	smmu_pmu = hlist_entry_safe(node, struct smmu_pmu, node);
	if (cpu != smmu_pmu->on_cpu)
		return 0;

	target = cpumask_any_but(cpu_online_mask, cpu);
	if (target >= nr_cpu_ids)
		return 0;

	perf_pmu_migrate_context(&smmu_pmu->pmu, cpu, target);
	smmu_pmu->on_cpu = target;

	for (i = 0; i < smmu_pmu->num_irqs; ++i)
		WARN_ON(irq_set_affinity_hint(smmu_pmu->irqs[i],
						cpumask_of(target)));

	return 0;
}

static int smmu_pmu_probe(struct platform_device *pdev)
{
	struct smmu_pmu *smmu_pmu;
	struct resource *mem_resource_0, *mem_resource_1;
	void __iomem *mem_map_0, *mem_map_1;
	unsigned int reg_size;
	int err;
	int irq, i;
	u32 ceid[SMMU_MAX_CEIDS];
	u32 ceid_32;
	resource_size_t size;

	smmu_pmu = devm_kzalloc(&pdev->dev, sizeof(*smmu_pmu), GFP_KERNEL);
	if (!smmu_pmu)
		return -ENOMEM;

	platform_set_drvdata(pdev, smmu_pmu);
	smmu_pmu->pmu = (struct pmu) {
		.task_ctx_nr    = perf_invalid_context,
		.pmu_enable	= smmu_pmu_enable,
		.pmu_disable	= smmu_pmu_disable,
		.event_init	= smmu_pmu_event_init,
		.add		= smmu_pmu_event_add,
		.del		= smmu_pmu_event_del,
		.start		= smmu_pmu_event_start,
		.stop		= smmu_pmu_event_stop,
		.read		= smmu_pmu_event_read,
		.attr_groups	= smmu_pmu_attr_grps,
	};

	mem_resource_0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	size = resource_size(mem_resource_0);

	mem_map_0 = devm_ioremap(&pdev->dev, mem_resource_0->start, size);

	if (!mem_map_0) {
		dev_err(&pdev->dev, "Can't map SMMU PMU @%pa\n",
			&mem_resource_0->start);
		return PTR_ERR(mem_map_0);
	}

	mem_resource_1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	size = resource_size(mem_resource_1);

	mem_map_1 = devm_ioremap(&pdev->dev, mem_resource_1->start, size);
	if (!mem_map_1) {
		dev_err(&pdev->dev, "Can't map SMMU PMU TCU @%pa\n",
			&mem_resource_1->start);
		return PTR_ERR(mem_map_1);
	}

	dev_err(&pdev->dev, "SMMU PMU TCU @%pa\n", &mem_resource_1->start);
	smmu_pmu->reg_base = mem_map_0;
	smmu_pmu->tcu_base = mem_map_1;
	smmu_pmu->pmu.name =
		devm_kasprintf(&pdev->dev, GFP_KERNEL, "smmu_0_%llx",
			       (mem_resource_0->start) >> SMMU_PA_SHIFT);

	ceid_32 = readl_relaxed(smmu_pmu->reg_base + SMMU_PMCG_CEID0);
	ceid[0] = ceid_32;
	bitmap_from_arr32(smmu_pmu->supported_events, ceid, SMMU_MAX_EVENT_ID);

	smmu_pmu->num_irqs = platform_irq_count(pdev);
	smmu_pmu->irqs = devm_kzalloc(&pdev->dev,
			sizeof(*smmu_pmu->irqs) * smmu_pmu->num_irqs,
			GFP_KERNEL);

	for (i = 0; i < smmu_pmu->num_irqs; ++i) {
		irq = platform_get_irq(pdev, i);

		if (irq < 0) {
			dev_err(&pdev->dev, "failed to get irq index %d\n", i);
			return -ENODEV;
		}

		err = devm_request_irq(&pdev->dev, irq, smmu_pmu_handle_irq,
				IRQF_SHARED | IRQF_NO_THREAD, "smmu-pmu",
				smmu_pmu);
		if (err) {
			dev_err(&pdev->dev,
					"Unable to request IRQ %d\n", irq);
			return err;
		}
		smmu_pmu->irqs[i] = irq;
	}
	/* Pick one CPU to be the preferred one to use */
	smmu_pmu->on_cpu = smp_processor_id();

	for (i = 0; i < smmu_pmu->num_irqs; ++i) {
		WARN_ON(irq_set_affinity_hint(smmu_pmu->irqs[i],
						cpumask_of(smmu_pmu->on_cpu)));
	}

	smmu_pmu->num_counters = get_num_counters(smmu_pmu);
	smmu_pmu->num_countergroups = get_num_countergroups(smmu_pmu);
	smmu_pmu->pdev = pdev;
	smmu_pmu->counter_present_mask = GENMASK(smmu_pmu->num_counters - 1, 0);
	reg_size = (readl_relaxed(smmu_pmu->reg_base + SMMU_PMCG_CFGR) &
		    SMMU_PMCG_CFGR_SIZE_MASK) >> SMMU_PMCG_CFGR_SIZE_SHIFT;
	smmu_pmu->reg_size_32 = (reg_size == SMMU_PMCG_CFGR_COUNTER_SIZE_32);
	smmu_pmu->counter_mask = GENMASK_ULL(reg_size, 0);

	smmu_pmu_reset(smmu_pmu);

	err = cpuhp_state_add_instance_nocalls(cpuhp_state_num,
					       &smmu_pmu->node);
	if (err) {
		dev_err(&pdev->dev, "Error %d registering hotplug\n", err);
		return err;
	}

	err = perf_pmu_register(&smmu_pmu->pmu, smmu_pmu->pmu.name, -1);
	if (err) {
		dev_err(&pdev->dev, "Error %d registering SMMU PMU\n", err);
		goto out_unregister;
	}

	dev_info(&pdev->dev, "Registered SMMU PMU @ %pa using %d counters %d counter groups\n",
		 &mem_resource_0->start, smmu_pmu->num_counters,
		 smmu_pmu->num_countergroups);

	return err;

out_unregister:
	cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &smmu_pmu->node);
	return err;
}

static int smmu_pmu_remove(struct platform_device *pdev)
{
	struct smmu_pmu *smmu_pmu = platform_get_drvdata(pdev);

	perf_pmu_unregister(&smmu_pmu->pmu);
	cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &smmu_pmu->node);

	return 0;
}

static void smmu_pmu_shutdown(struct platform_device *pdev)
{
	struct smmu_pmu *smmu_pmu = platform_get_drvdata(pdev);

	smmu_pmu_disable(&smmu_pmu->pmu);
}

static const struct of_device_id arm_smmu_pmu_of_match[] = {
	{ .compatible = "arm,smmu-pmu", },
	{ },
};

MODULE_DEVICE_TABLE(of, arm_smmu_pmu_of_match);
static struct platform_driver smmu_pmu_driver = {
	.driver = {
		.name = "arm,smmu-pmu",
		.of_match_table	= of_match_ptr(arm_smmu_pmu_of_match),
	},
	.probe = smmu_pmu_probe,
	.remove = smmu_pmu_remove,
	.shutdown = smmu_pmu_shutdown,
};

static int __init arm_smmu_pmu_init(void)
{
	cpuhp_state_num = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
				      "perf/arm/smmupmu:online",
				      NULL,
				      smmu_pmu_offline_cpu);
	if (cpuhp_state_num < 0)
		return cpuhp_state_num;
	return platform_driver_register(&smmu_pmu_driver);
}
subsys_initcall(arm_smmu_pmu_init);

static void __exit arm_smmu_pmu_exit(void)
{
	platform_driver_unregister(&smmu_pmu_driver);
}

module_exit(arm_smmu_pmu_exit);
MODULE_LICENSE("GPL v2");
