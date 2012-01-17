/*
 * Copyright (c) 2011, 2012 Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifdef CONFIG_ARCH_MSM_KRAIT

#include <linux/irq.h>

#include <mach/msm-krait-l2-accessors.h>

#define MAX_L2_PERIOD	((1ULL << 32) - 1)
#define MAX_KRAIT_L2_CTRS 5

#define L2PMCCNTR 0x409
#define L2PMCCNTCR 0x408
#define L2PMCCNTSR 0x40A
#define L2CYCLE_CTR_BIT 31
#define L2CYCLE_CTR_EVENT_IDX 4
#define L2CYCLE_CTR_RAW_CODE 0xfe

#define L2PMOVSR	0x406

#define L2PMCR	0x400
#define L2PMCR_RESET_ALL	0x6
#define L2PMCR_GLOBAL_ENABLE	0x1
#define L2PMCR_GLOBAL_DISABLE	0x0

#define L2PMCNTENSET	0x403
#define L2PMCNTENCLR	0x402

#define L2PMINTENSET	0x405
#define L2PMINTENCLR	0x404

#define IA_L2PMXEVCNTCR_BASE	0x420
#define IA_L2PMXEVTYPER_BASE	0x424
#define IA_L2PMRESX_BASE	0x410
#define IA_L2PMXEVFILTER_BASE	0x423
#define IA_L2PMXEVCNTR_BASE	0x421

/* event format is -e rsRCCG See get_event_desc() */

#define EVENT_REG_MASK		0xf000
#define EVENT_GROUPSEL_MASK	0x000f
#define	EVENT_GROUPCODE_MASK	0x0ff0
#define EVENT_REG_SHIFT		12
#define EVENT_GROUPCODE_SHIFT	4

#define	RESRX_VALUE_EN	0x80000000

static struct platform_device *l2_pmu_device;

struct hw_krait_l2_pmu {
	struct perf_event *events[MAX_KRAIT_L2_CTRS];
	unsigned long active_mask[BITS_TO_LONGS(MAX_KRAIT_L2_CTRS)];
	raw_spinlock_t lock;
};

struct hw_krait_l2_pmu hw_krait_l2_pmu;

struct event_desc {
	int event_groupsel;
	int event_reg;
	int event_group_code;
};

void get_event_desc(u64 config, struct event_desc *evdesc)
{
	/* L2PMEVCNTRX */
	evdesc->event_reg = (config & EVENT_REG_MASK) >> EVENT_REG_SHIFT;
	/* Group code (row ) */
	evdesc->event_group_code =
	    (config & EVENT_GROUPCODE_MASK) >> EVENT_GROUPCODE_SHIFT;
	/* Group sel (col) */
	evdesc->event_groupsel = (config & EVENT_GROUPSEL_MASK);

	pr_debug("%s: reg: %x, group_code: %x, groupsel: %x\n", __func__,
		 evdesc->event_reg, evdesc->event_group_code,
		 evdesc->event_groupsel);
}

static void set_evcntcr(int ctr)
{
	u32 evtcr_reg = (ctr * 16) + IA_L2PMXEVCNTCR_BASE;

	set_l2_indirect_reg(evtcr_reg, 0x0);
}

static void set_evtyper(int event_groupsel, int event_reg, int ctr)
{
	u32 evtype_reg = (ctr * 16) + IA_L2PMXEVTYPER_BASE;
	u32 evtype_val = event_groupsel + (4 * event_reg);

	set_l2_indirect_reg(evtype_reg, evtype_val);
}

static void set_evres(int event_groupsel, int event_reg, int event_group_code)
{
	u32 group_reg = event_reg + IA_L2PMRESX_BASE;
	u32 group_val =
		RESRX_VALUE_EN | (event_group_code << (8 * event_groupsel));
	u32 resr_val;
	u32 group_byte = 0xff;
	u32 group_mask = ~(group_byte << (8 * event_groupsel));

	resr_val = get_l2_indirect_reg(group_reg);
	resr_val &= group_mask;
	resr_val |= group_val;

	set_l2_indirect_reg(group_reg, resr_val);
}

static void set_evfilter_task_mode(int ctr)
{
	u32 filter_reg = (ctr * 16) + IA_L2PMXEVFILTER_BASE;
	u32 filter_val = 0x000f0030 | 1 << smp_processor_id();

	set_l2_indirect_reg(filter_reg, filter_val);
}

static void set_evfilter_sys_mode(int ctr)
{
	u32 filter_reg = (ctr * 16) + IA_L2PMXEVFILTER_BASE;
	u32 filter_val = 0x000f003f;

	set_l2_indirect_reg(filter_reg, filter_val);
}

static void enable_intenset(u32 idx)
{
	if (idx == L2CYCLE_CTR_EVENT_IDX)
		set_l2_indirect_reg(L2PMINTENSET, 1 << L2CYCLE_CTR_BIT);
	else
		set_l2_indirect_reg(L2PMINTENSET, 1 << idx);
}

static void disable_intenclr(u32 idx)
{
	if (idx == L2CYCLE_CTR_EVENT_IDX)
		set_l2_indirect_reg(L2PMINTENCLR, 1 << L2CYCLE_CTR_BIT);
	else
		set_l2_indirect_reg(L2PMINTENCLR, 1 << idx);
}

static void enable_counter(u32 idx)
{
	if (idx == L2CYCLE_CTR_EVENT_IDX)
		set_l2_indirect_reg(L2PMCNTENSET, 1 << L2CYCLE_CTR_BIT);
	else
		set_l2_indirect_reg(L2PMCNTENSET, 1 << idx);
}

static void disable_counter(u32 idx)
{
	if (idx == L2CYCLE_CTR_EVENT_IDX)
		set_l2_indirect_reg(L2PMCNTENCLR, 1 << L2CYCLE_CTR_BIT);
	else
		set_l2_indirect_reg(L2PMCNTENCLR, 1 << idx);
}

static u64 read_counter(u32 idx)
{
	u32 val;
	u32 counter_reg = (idx * 16) + IA_L2PMXEVCNTR_BASE;

	if (idx == L2CYCLE_CTR_EVENT_IDX)
		val = get_l2_indirect_reg(L2PMCCNTR);
	else
		val = get_l2_indirect_reg(counter_reg);

	return val;
}

static void write_counter(u32 idx, u32 val)
{
	u32 counter_reg = (idx * 16) + IA_L2PMXEVCNTR_BASE;

	if (idx == L2CYCLE_CTR_EVENT_IDX)
		set_l2_indirect_reg(L2PMCCNTR, val);
	else
		set_l2_indirect_reg(counter_reg, val);
}

static int
pmu_event_set_period(struct perf_event *event,
		     struct hw_perf_event *hwc, int idx)
{
	s64 left = local64_read(&hwc->period_left);
	s64 period = hwc->sample_period;
	int ret = 0;

	if (unlikely(left <= -period)) {
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (unlikely(left <= 0)) {
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (left > (s64) MAX_L2_PERIOD)
		left = MAX_L2_PERIOD;

	local64_set(&hwc->prev_count, (u64)-left);

	write_counter(idx, (u64) (-left) & 0xffffffff);

	perf_event_update_userpage(event);

	return ret;
}

static u64
pmu_event_update(struct perf_event *event, struct hw_perf_event *hwc, int idx,
		 int overflow)
{
	u64 prev_raw_count, new_raw_count;
	u64 delta;

again:
	prev_raw_count = local64_read(&hwc->prev_count);
	new_raw_count = read_counter(idx);

	if (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
			    new_raw_count) != prev_raw_count)
		goto again;

	new_raw_count &= MAX_L2_PERIOD;
	prev_raw_count &= MAX_L2_PERIOD;

	if (overflow)
		delta = MAX_L2_PERIOD - prev_raw_count + new_raw_count;
	else
		delta = new_raw_count - prev_raw_count;

	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);

	pr_debug("%s: new: %lld, prev: %lld, event: %ld count: %lld\n",
		 __func__, new_raw_count, prev_raw_count,
		 hwc->config_base, local64_read(&event->count));

	return new_raw_count;
}

static void krait_l2_read(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	pmu_event_update(event, hwc, hwc->idx, 0);
}

static void krait_l2_stop_counter(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	if (!(hwc->state & PERF_HES_STOPPED)) {
		disable_intenclr(idx);
		disable_counter(idx);

		pmu_event_update(event, hwc, idx, 0);
		hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
	}

	pr_debug("%s: event: %ld ctr: %d stopped\n", __func__, hwc->config_base,
		 idx);
}

static void krait_l2_start_counter(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	struct event_desc evdesc;

	if (flags & PERF_EF_RELOAD)
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));

	hwc->state = 0;

	pmu_event_set_period(event, hwc, idx);

	if (hwc->config_base == L2CYCLE_CTR_RAW_CODE)
		goto out;

	set_evcntcr(idx);

	memset(&evdesc, 0, sizeof(evdesc));

	get_event_desc(hwc->config_base, &evdesc);

	set_evtyper(evdesc.event_groupsel, evdesc.event_reg, idx);

	set_evres(evdesc.event_groupsel, evdesc.event_reg,
		  evdesc.event_group_code);

	if (event->cpu < 0)
		set_evfilter_task_mode(idx);
	else
		set_evfilter_sys_mode(idx);

out:
	enable_intenset(idx);
	enable_counter(idx);

	pr_debug
	    ("%s: ctr: %d group: %ld group_code: %lld started from cpu:%d\n",
	     __func__, idx, hwc->config_base, hwc->config, smp_processor_id());
}

static void krait_l2_del_event(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	unsigned long iflags;

	raw_spin_lock_irqsave(&hw_krait_l2_pmu.lock, iflags);

	clear_bit(idx, (long unsigned int *)(&hw_krait_l2_pmu.active_mask));

	krait_l2_stop_counter(event, PERF_EF_UPDATE);
	hw_krait_l2_pmu.events[idx] = NULL;
	hwc->idx = -1;

	raw_spin_unlock_irqrestore(&hw_krait_l2_pmu.lock, iflags);

	pr_debug("%s: event: %ld deleted\n", __func__, hwc->config_base);

	perf_event_update_userpage(event);
}

static int krait_l2_add_event(struct perf_event *event, int flags)
{
	int ctr = 0;
	struct hw_perf_event *hwc = &event->hw;
	unsigned long iflags;
	int err = 0;

	perf_pmu_disable(event->pmu);

	raw_spin_lock_irqsave(&hw_krait_l2_pmu.lock, iflags);

	/* Cycle counter has a resrvd index */
	if (hwc->config_base == L2CYCLE_CTR_RAW_CODE) {
		if (hw_krait_l2_pmu.events[L2CYCLE_CTR_EVENT_IDX]) {
			pr_err("%s: Stale cycle ctr event ptr !\n", __func__);
			err = -EINVAL;
			goto out;
		}
		hwc->idx = L2CYCLE_CTR_EVENT_IDX;
		hw_krait_l2_pmu.events[L2CYCLE_CTR_EVENT_IDX] = event;
		set_bit(L2CYCLE_CTR_EVENT_IDX,
			(long unsigned int *)&hw_krait_l2_pmu.active_mask);
		goto skip_ctr_loop;
	}

	for (ctr = 0; ctr < MAX_KRAIT_L2_CTRS - 1; ctr++) {
		if (!hw_krait_l2_pmu.events[ctr]) {
			hwc->idx = ctr;
			hw_krait_l2_pmu.events[ctr] = event;
			set_bit(ctr,
				(long unsigned int *)
				&hw_krait_l2_pmu.active_mask);
			break;
		}
	}

	if (hwc->idx < 0) {
		err = -ENOSPC;
		pr_err("%s: No space for event: %llx!!\n", __func__,
		       event->attr.config);
		goto out;
	}

skip_ctr_loop:

	disable_counter(hwc->idx);

	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;

	if (flags & PERF_EF_START)
		krait_l2_start_counter(event, PERF_EF_RELOAD);

	perf_event_update_userpage(event);

	pr_debug("%s: event: %ld, ctr: %d added from cpu:%d\n",
		 __func__, hwc->config_base, hwc->idx, smp_processor_id());
out:
	raw_spin_unlock_irqrestore(&hw_krait_l2_pmu.lock, iflags);

	/* Resume the PMU even if this event could not be added */
	perf_pmu_enable(event->pmu);

	return err;
}

static void krait_l2_pmu_enable(struct pmu *pmu)
{
	isb();
	set_l2_indirect_reg(L2PMCR, L2PMCR_GLOBAL_ENABLE);
}

static void krait_l2_pmu_disable(struct pmu *pmu)
{
	set_l2_indirect_reg(L2PMCR, L2PMCR_GLOBAL_DISABLE);
	isb();
}

u32 get_reset_pmovsr(void)
{
	int val;

	val = get_l2_indirect_reg(L2PMOVSR);
	/* reset it */
	val &= 0xffffffff;
	set_l2_indirect_reg(L2PMOVSR, val);

	return val;
}

static irqreturn_t krait_l2_handle_irq(int irq_num, void *dev)
{
	unsigned long pmovsr;
	struct perf_sample_data data;
	struct pt_regs *regs;
	struct perf_event *event;
	struct hw_perf_event *hwc;
	int bitp;
	int idx = 0;

	pmovsr = get_reset_pmovsr();

	if (!(pmovsr & 0xffffffff))
		return IRQ_NONE;

	regs = get_irq_regs();

	perf_sample_data_init(&data, 0);

	raw_spin_lock(&hw_krait_l2_pmu.lock);

	while (pmovsr) {
		bitp = __ffs(pmovsr);

		if (bitp == L2CYCLE_CTR_BIT)
			idx = L2CYCLE_CTR_EVENT_IDX;
		else
			idx = bitp;

		event = hw_krait_l2_pmu.events[idx];

		if (!event)
			goto next;

		if (!test_bit(idx, hw_krait_l2_pmu.active_mask))
			goto next;

		hwc = &event->hw;
		pmu_event_update(event, hwc, idx, 1);
		data.period = event->hw.last_period;

		if (!pmu_event_set_period(event, hwc, idx))
			goto next;

		if (perf_event_overflow(event, 0, &data, regs))
			disable_counter(hwc->idx);
next:
		pmovsr &= (pmovsr - 1);
	}

	raw_spin_unlock(&hw_krait_l2_pmu.lock);

	irq_work_run();

	return IRQ_HANDLED;
}

static atomic_t active_l2_events = ATOMIC_INIT(0);
static DEFINE_MUTEX(krait_pmu_reserve_mutex);

static int pmu_reserve_hardware(void)
{
	int i, err = -ENODEV, irq;

	l2_pmu_device = reserve_pmu(ARM_PMU_DEVICE_L2);

	if (IS_ERR(l2_pmu_device)) {
		pr_warning("unable to reserve pmu\n");
		return PTR_ERR(l2_pmu_device);
	}

	if (l2_pmu_device->num_resources < 1) {
		pr_err("no irqs for PMUs defined\n");
		return -ENODEV;
	}

	if (strncmp(l2_pmu_device->name, "l2-arm-pmu", 6)) {
		pr_err("Incorrect pdev reserved !\n");
		return -EINVAL;
	}

	for (i = 0; i < l2_pmu_device->num_resources; ++i) {
		irq = platform_get_irq(l2_pmu_device, i);
		if (irq < 0)
			continue;

		err = request_irq(irq, krait_l2_handle_irq,
				  IRQF_DISABLED | IRQF_NOBALANCING,
				  "krait-l2-pmu", NULL);
		if (err) {
			pr_warning("unable to request IRQ%d for Krait L2 perf "
				   "counters\n", irq);
			break;
		}

		irq_get_chip(irq)->irq_unmask(irq_get_irq_data(irq));
	}

	if (err) {
		for (i = i - 1; i >= 0; --i) {
			irq = platform_get_irq(l2_pmu_device, i);
			if (irq >= 0)
				free_irq(irq, NULL);
		}
		release_pmu(l2_pmu_device);
		l2_pmu_device = NULL;
	}

	return err;
}

static void pmu_release_hardware(void)
{
	int i, irq;

	for (i = l2_pmu_device->num_resources - 1; i >= 0; --i) {
		irq = platform_get_irq(l2_pmu_device, i);
		if (irq >= 0)
			free_irq(irq, NULL);
	}

	krait_l2_pmu_disable(NULL);

	release_pmu(l2_pmu_device);
	l2_pmu_device = NULL;
}

static void pmu_perf_event_destroy(struct perf_event *event)
{
	if (atomic_dec_and_mutex_lock
	    (&active_l2_events, &krait_pmu_reserve_mutex)) {
		pmu_release_hardware();
		mutex_unlock(&krait_pmu_reserve_mutex);
	}
}

static int krait_l2_event_init(struct perf_event *event)
{
	int err = 0;
	struct hw_perf_event *hwc = &event->hw;
	int status = 0;

	switch (event->attr.type) {
	case PERF_TYPE_SHARED:
		break;

	default:
		return -ENOENT;
	}

	hwc->idx = -1;

	event->destroy = pmu_perf_event_destroy;

	if (!atomic_inc_not_zero(&active_l2_events)) {
		/* 0 active events */
		mutex_lock(&krait_pmu_reserve_mutex);
		err = pmu_reserve_hardware();
		mutex_unlock(&krait_pmu_reserve_mutex);
		if (!err)
			atomic_inc(&active_l2_events);
		else
			return err;
	}

	hwc->config = 0;
	hwc->event_base = 0;

	/* Check if we came via perf default syms */
	if (event->attr.config == PERF_COUNT_HW_L2_CYCLES)
		hwc->config_base = L2CYCLE_CTR_RAW_CODE;
	else
		hwc->config_base = event->attr.config;

	/* Only one CPU can control the cycle counter */
	if (hwc->config_base == L2CYCLE_CTR_RAW_CODE) {
		/* Check if its already running */
		status = get_l2_indirect_reg(L2PMCCNTSR);
		if (status == 0x2) {
			err = -ENOSPC;
			goto out;
		}
	}

	if (!hwc->sample_period) {
		hwc->sample_period = MAX_L2_PERIOD;
		hwc->last_period = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	pr_debug("%s: event: %lld init'd\n", __func__, event->attr.config);

out:
	if (err < 0)
		pmu_perf_event_destroy(event);

	return err;
}

static struct pmu krait_l2_pmu = {
	.pmu_enable = krait_l2_pmu_enable,
	.pmu_disable = krait_l2_pmu_disable,
	.event_init = krait_l2_event_init,
	.add = krait_l2_add_event,
	.del = krait_l2_del_event,
	.start = krait_l2_start_counter,
	.stop = krait_l2_stop_counter,
	.read = krait_l2_read,
};

static const struct arm_pmu *__init krait_l2_pmu_init(void)
{
	/* Register our own PMU here */
	perf_pmu_register(&krait_l2_pmu, "Krait L2", PERF_TYPE_SHARED);

	memset(&hw_krait_l2_pmu, 0, sizeof(hw_krait_l2_pmu));

	/* Reset all ctrs */
	set_l2_indirect_reg(L2PMCR, L2PMCR_RESET_ALL);

	/* Avoid spurious interrupt if any */
	get_reset_pmovsr();

	raw_spin_lock_init(&hw_krait_l2_pmu.lock);

	/* Don't return an arm_pmu here */
	return NULL;
}
#else

static const struct arm_pmu *__init krait_l2_pmu_init(void)
{
	return NULL;
}
#endif
