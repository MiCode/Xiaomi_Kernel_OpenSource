/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "kryo perfevents: " fmt

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/perf_event.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/perf/arm_pmu.h>

#include <soc/qcom/perf_event_kryo.h>

#define	ARMV8_IDX_CYCLE_COUNTER	0

#define COUNT_MASK	    0xffffffff

u32 evt_type_base[3] = {0xd8, 0xe0, 0xe8};

static struct arm_pmu *cpu_pmu;

struct kryo_evt {
	/*
	 * The group_setval field corresponds to the value that the pmresr
	 * register needs to be set to. This value is calculated from the row
	 * and column that the event belongs to in the event table
	 */
	u32 pmresr_setval;
	/*
	 * The PMRESR reg that the event belongs to.
	 * Kryo has 3 groups of events PMRESR0, 1, 2
	 */
	u8 reg;
	u8 group;
	/* The armv8 defined event code that the Kryo events map to */
	u32 armv8_evt_type;
	/* indicates whether the low (0) or high (1) RESR is used */
	int l_h;
};

static unsigned int get_kryo_evtinfo(unsigned int evt_type,
				     struct kryo_evt *evtinfo)
{
	u8 prefix = (evt_type & KRYO_EVT_PREFIX_MASK) >> KRYO_EVT_PREFIX_SHIFT;
	u8 reg =    (evt_type & KRYO_EVT_REG_MASK)    >> KRYO_EVT_REG_SHIFT;
	u8 code =   (evt_type & KRYO_EVT_CODE_MASK)   >> KRYO_EVT_CODE_SHIFT;
	u8 group =  (evt_type & KRYO_EVT_GROUP_MASK)  >> KRYO_EVT_GROUP_SHIFT;

	if ((group > KRYO_MAX_GROUP) || (reg > KRYO_MAX_L1_REG))
		return -EINVAL;

	if (prefix != KRYO_EVT_PREFIX)
		return -EINVAL;

	evtinfo->pmresr_setval = code << ((group & 0x3) * 8);
	if (group <= 3) {
		evtinfo->l_h = RESR_L;
	} else {
		evtinfo->l_h = RESR_H;
		evtinfo->pmresr_setval |= RESR_ENABLE;
	}
	evtinfo->reg = reg;
	evtinfo->group = group;
	evtinfo->armv8_evt_type = evt_type_base[reg] | group;

	return evtinfo->armv8_evt_type;
}

static void kryo_write_pmxevcntcr(u32 val)
{
	asm volatile("msr " pmxevcntcr_el0 ", %0" : : "r" (val));
}

static void kryo_write_pmresr(int reg, int l_h, u32 val)
{
	if (reg > KRYO_MAX_L1_REG) {
		pr_err("Invalid write to RESR reg %d\n", reg);
		return;
	}

	if (l_h == RESR_L) {
		switch (reg) {
		case 0:
			asm volatile("msr " pmresr0l_el0 ", %0" : : "r" (val));
			break;
		case 1:
			asm volatile("msr " pmresr1l_el0 ", %0" : : "r" (val));
			break;
		case 2:
			asm volatile("msr " pmresr2l_el0 ", %0" : : "r" (val));
			break;
		}
	} else {
		switch (reg) {
		case 0:
			asm volatile("msr " pmresr0h_el0 ", %0" : : "r" (val));
			break;
		case 1:
			asm volatile("msr " pmresr1h_el0 ", %0" : : "r" (val));
			break;
		case 2:
			asm volatile("msr " pmresr2h_el0 ", %0" : : "r" (val));
			break;
		}
	}
}

static u32 kryo_read_pmresr(int reg, int l_h)
{
	u32 val = 0;

	if (l_h == RESR_L) {
		switch (reg) {
		case 0:
			asm volatile("mrs %0, " pmresr0l_el0 : "=r" (val));
			break;
		case 1:
			asm volatile("mrs %0, " pmresr1l_el0 : "=r" (val));
			break;
		case 2:
			asm volatile("mrs %0, " pmresr2l_el0 : "=r" (val));
			break;
		default:
			WARN_ONCE(1, "Invalid read of RESR reg %d\n", reg);
			break;
		}
	} else {
		switch (reg) {
		case 0:
			asm volatile("mrs %0," pmresr0h_el0 : "=r" (val));
			break;
		case 1:
			asm volatile("mrs %0," pmresr1h_el0 : "=r" (val));
			break;
		case 2:
			asm volatile("mrs %0," pmresr2h_el0 : "=r" (val));
			break;
		default:
			WARN_ONCE(1, "Invalid read of RESR reg %d\n", reg);
			break;
		}
	}

	return val;
}

static inline u32 kryo_get_columnmask(u32 g)
{
	u32 mask;

	mask = ~(0xff << ((g & 0x3) * 8));
	if (g == KRYO_MAX_GROUP)
		mask |= RESR_ENABLE;

	return mask;
}

static void kryo_set_resr(struct kryo_evt *evtinfo)
{
	u32 val;

	val = kryo_read_pmresr(evtinfo->reg, evtinfo->l_h) &
		kryo_get_columnmask(evtinfo->group);
	val |= evtinfo->pmresr_setval;
	kryo_write_pmresr(evtinfo->reg, evtinfo->l_h, val);
	/*
	 * If we just wrote the RESR_L, we have to make sure the
	 * enable bit is set in RESR_H
	 */
	if (evtinfo->l_h == RESR_L) {
		val = kryo_read_pmresr(evtinfo->reg, RESR_H);
		if ((val & RESR_ENABLE) == 0) {
			val |= RESR_ENABLE;
			kryo_write_pmresr(evtinfo->reg, RESR_H, val);
		}
	}
}

static void kryo_clear_resrs(void)
{
	int i;

	for (i = 0; i <= KRYO_MAX_L1_REG; i++) {
		kryo_write_pmresr(i, RESR_L, 0);
		kryo_write_pmresr(i, RESR_H, 0);
	}
}

static void kryo_clear_resr(struct kryo_evt *evtinfo)
{
	u32 val;

	val = kryo_read_pmresr(evtinfo->reg, evtinfo->l_h) &
		kryo_get_columnmask(evtinfo->group);
	kryo_write_pmresr(evtinfo->reg, evtinfo->l_h, val);
}

static void kryo_pmu_disable_event(struct perf_event *event)
{
	unsigned long flags;
	u32 val = 0;
	unsigned long ev_num;
	struct kryo_evt evtinfo;
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);

	/* Disable counter and interrupt */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/* Disable counter */
	armv8pmu_disable_counter(idx);

	/*
	 * Clear pmresr code
	 * We don't need to set the event if it's a cycle count
	 */
	if (idx != ARMV8_IDX_CYCLE_COUNTER) {
		val = hwc->config_base & KRYO_EVT_MASK;

		if (val & KRYO_EVT_PREFIX_MASK) {
			ev_num = get_kryo_evtinfo(val, &evtinfo);
			if (ev_num == -EINVAL)
				goto kryo_dis_out;
			kryo_clear_resr(&evtinfo);
		}
	}
	/* Disable interrupt for this counter */
	armv8pmu_disable_intens(idx);

kryo_dis_out:
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void kryo_pmu_enable_event(struct perf_event *event)
{
	unsigned long flags;
	u32 val = 0;
	unsigned long ev_num;
	struct kryo_evt evtinfo;
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	unsigned long long prev_count = local64_read(&hwc->prev_count);
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);

	/*
	 * Enable counter and interrupt, and set the counter to count
	 * the event that we're interested in.
	 */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/* Disable counter */
	armv8pmu_disable_counter(idx);

	val = hwc->config_base & KRYO_EVT_MASK;

	/* set event for ARM-architected events, and filter for CC */
	if (!(val & KRYO_EVT_PREFIX_MASK) || (idx == ARMV8_IDX_CYCLE_COUNTER)) {
		armv8pmu_write_evtype(idx, hwc->config_base);
	} else {
		ev_num = get_kryo_evtinfo(val, &evtinfo);
		if (ev_num == -EINVAL)
			goto kryo_en_out;

		/* Restore Mode-exclusion bits */
		ev_num |= (hwc->config_base & KRYO_MODE_EXCL_MASK);

		armv8pmu_write_evtype(idx, ev_num);
		kryo_write_pmxevcntcr(0);
		kryo_set_resr(&evtinfo);
	}

	/* Enable interrupt for this counter */
	armv8pmu_enable_intens(idx);

	/* Restore prev val */
	cpu_pmu->write_counter(event, prev_count & COUNT_MASK);

	/* Enable counter */
	armv8pmu_enable_counter(idx);

kryo_en_out:
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

#ifdef CONFIG_PERF_EVENTS_USERMODE
static void kryo_init_usermode(void)
{
	u32 val;

	asm volatile("mrs %0, " pmactlr_el0 : "=r" (val));
	val |= PMACTLR_UEN;
	asm volatile("msr " pmactlr_el0 ", %0" : : "r" (val));
	asm volatile("mrs %0, pmuserenr_el0" : "=r" (val));
	val |= PMUSERENR_UEN;
	asm volatile("msr pmuserenr_el0, %0" : : "r" (val));
}
#else
static inline void kryo_init_usermode(void)
{
}
#endif

static int kryo_map_event(struct perf_event *event)
{
	return armpmu_map_event(event, &armv8_pmuv3_perf_map,
				&armv8_pmuv3_perf_cache_map,
				KRYO_EVT_MASK);
}

static void kryo_pmu_reset(void *info)
{
	u32 idx, nb_cnt = cpu_pmu->num_events;

	/* Stop all counters and their interrupts */
	for (idx = ARMV8_IDX_CYCLE_COUNTER; idx < nb_cnt; ++idx) {
		armv8pmu_disable_counter(idx);
		armv8pmu_disable_intens(idx);
	}

	/* Clear all pmresrs */
	kryo_clear_resrs();

	kryo_init_usermode();

	/* Reset irq status reg */
	armv8pmu_getreset_flags();

	/* Reset all counters */
	armv8pmu_pmcr_write(ARMV8_PMCR_P | ARMV8_PMCR_C);
}

/* NRCCG format for perf RAW codes. */
PMU_FORMAT_ATTR(prefix,	"config:16-19");
PMU_FORMAT_ATTR(reg,	"config:12-15");
PMU_FORMAT_ATTR(code,	"config:4-11");
PMU_FORMAT_ATTR(grp,	"config:0-3");

static struct attribute *kryo_ev_formats[] = {
	&format_attr_prefix.attr,
	&format_attr_reg.attr,
	&format_attr_code.attr,
	&format_attr_grp.attr,
	NULL,
};

/*
 * Format group is essential to access PMU from userspace
 * via its .name field.
 */
static struct attribute_group kryo_pmu_format_group = {
	.name = "format",
	.attrs = kryo_ev_formats,
};

static const struct attribute_group *kryo_pmu_attr_grps[] = {
	&kryo_pmu_format_group,
	NULL,
};

int kryo_pmu_init(struct arm_pmu *kryo_pmu)
{
	pr_info("CPU pmu for kryo-pmuv3 detected\n");

	armv8_pmu_init(kryo_pmu);
	cpu_pmu = kryo_pmu;

	cpu_pmu->enable			= kryo_pmu_enable_event;
	cpu_pmu->disable		= kryo_pmu_disable_event;
	cpu_pmu->reset			= kryo_pmu_reset;
	cpu_pmu->pmu.attr_groups	= kryo_pmu_attr_grps;
	cpu_pmu->map_event              = kryo_map_event;
	cpu_pmu->name			= "qcom,kryo-pmuv3";

	kryo_clear_resrs();

	return armv8pmu_probe_num_events(cpu_pmu);
}

