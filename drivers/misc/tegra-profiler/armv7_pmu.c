/*
 * drivers/misc/tegra-profiler/armv7_pmu.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/err.h>
#include <linux/bitmap.h>
#include <linux/slab.h>

#include <asm/cputype.h>
#include <asm/pmu.h>

#include <linux/tegra_profiler.h>

#include "armv7_pmu.h"
#include "quadd.h"
#include "debug.h"

static struct armv7_pmu_ctx pmu_ctx;

struct quadd_pmu_info {
	DECLARE_BITMAP(used_cntrs, QUADD_MAX_PMU_COUNTERS);
	u32 prev_vals[QUADD_MAX_PMU_COUNTERS];
	int is_already_active;
};

struct quadd_cntrs_info {
	int pcntrs;
	int ccntr;

	spinlock_t lock;
};

static DEFINE_PER_CPU(struct quadd_pmu_info, cpu_pmu_info);

static unsigned quadd_armv7_a9_events_map[QUADD_EVENT_TYPE_MAX] = {
	[QUADD_EVENT_TYPE_INSTRUCTIONS] =
		QUADD_ARMV7_A9_HW_EVENT_INST_OUT_OF_RENAME_STAGE,
	[QUADD_EVENT_TYPE_BRANCH_INSTRUCTIONS] =
		QUADD_ARMV7_HW_EVENT_PC_WRITE,
	[QUADD_EVENT_TYPE_BRANCH_MISSES] =
		QUADD_ARMV7_HW_EVENT_PC_BRANCH_MIS_PRED,
	[QUADD_EVENT_TYPE_BUS_CYCLES] =
		QUADD_ARMV7_HW_EVENT_CLOCK_CYCLES,

	[QUADD_EVENT_TYPE_L1_DCACHE_READ_MISSES] =
		QUADD_ARMV7_HW_EVENT_DCACHE_REFILL,
	[QUADD_EVENT_TYPE_L1_DCACHE_WRITE_MISSES] =
		QUADD_ARMV7_HW_EVENT_DCACHE_REFILL,
	[QUADD_EVENT_TYPE_L1_ICACHE_MISSES] =
		QUADD_ARMV7_HW_EVENT_IFETCH_MISS,

	[QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES] =
		QUADD_ARMV7_UNSUPPORTED_EVENT,
	[QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES] =
		QUADD_ARMV7_UNSUPPORTED_EVENT,
	[QUADD_EVENT_TYPE_L2_ICACHE_MISSES] =
		QUADD_ARMV7_UNSUPPORTED_EVENT,
};

static unsigned quadd_armv7_a15_events_map[QUADD_EVENT_TYPE_MAX] = {
	[QUADD_EVENT_TYPE_INSTRUCTIONS] =
				QUADD_ARMV7_HW_EVENT_INSTR_EXECUTED,
	[QUADD_EVENT_TYPE_BRANCH_INSTRUCTIONS] =
				QUADD_ARMV7_A15_HW_EVENT_SPEC_PC_WRITE,
	[QUADD_EVENT_TYPE_BRANCH_MISSES] =
				QUADD_ARMV7_HW_EVENT_PC_BRANCH_MIS_PRED,
	[QUADD_EVENT_TYPE_BUS_CYCLES] = QUADD_ARMV7_HW_EVENT_BUS_CYCLES,

	[QUADD_EVENT_TYPE_L1_DCACHE_READ_MISSES] =
				QUADD_ARMV7_A15_HW_EVENT_L1_DCACHE_READ_REFILL,
	[QUADD_EVENT_TYPE_L1_DCACHE_WRITE_MISSES] =
				QUADD_ARMV7_A15_HW_EVENT_L1_DCACHE_WRITE_REFILL,
	[QUADD_EVENT_TYPE_L1_ICACHE_MISSES] =
				QUADD_ARMV7_HW_EVENT_IFETCH_MISS,

	[QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES] =
				QUADD_ARMV7_A15_HW_EVENT_L2_DCACHE_READ_REFILL,
	[QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES] =
				QUADD_ARMV7_A15_HW_EVENT_L2_DCACHE_WRITE_REFILL,
	[QUADD_EVENT_TYPE_L2_ICACHE_MISSES] =
				QUADD_ARMV7_UNSUPPORTED_EVENT,
};

static inline u32
armv7_pmu_pmnc_read(void)
{
	u32 val;
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
	return val;
}

static inline void
armv7_pmu_pmnc_write(u32 val)
{
	/* Read Performance MoNitor Control (PMNC) register */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : :
		     "r"(val & QUADD_ARMV7_PMNC_MASK));
}

static inline u32
armv7_pmu_cntens_read(void)
{
	u32 val;

	/* Read CouNT ENable Set (CNTENS) register */
	asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(val));
	return val;
}

static inline void
armv7_pmu_cntens_write(u32 val)
{
	/* Write CouNT ENable Set (CNTENS) register */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (val));
}

static inline void
armv7_pmu_cntenc_write(u32 val)
{
	/* Write CouNT ENable Clear (CNTENC) register */
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (val));
}

static inline void
armv7_pmu_pmnxsel_write(u32 val)
{
	/* Read Performance Counter SELection (PMNXSEL) register */
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : :
		     "r" (val & QUADD_ARMV7_SELECT_MASK));
}

static inline u32
armv7_pmu_ccnt_read(void)
{
	u32 val;

	/* Read Cycle CouNT (CCNT) register */
	asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r"(val));
	return val;
}

static inline void
armv7_pmu_ccnt_write(u32 val)
{
	/* Write Cycle CouNT (CCNT) register */
	asm volatile ("mcr p15, 0, %0, c9, c13, 0" : : "r"(val));
}

static inline u32
armv7_pmu_pmcnt_read(void)
{
	u32 val;

	/* Read Performance Monitor CouNT (PMCNTx) registers */
	asm volatile ("mrc p15, 0, %0, c9, c13, 2" : "=r"(val));
	return val;
}

static inline void
armv7_pmu_pmcnt_write(u32 val)
{
	/* Write Performance Monitor CouNT (PMCNTx) registers */
	asm volatile ("mcr p15, 0, %0, c9, c13, 2" : : "r"(val));
}

static inline void
armv7_pmu_evtsel_write(u32 event)
{
	/* Write Event SELection (EVTSEL) register */
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : :
		     "r" (event & QUADD_ARMV7_EVTSEL_MASK));
}

static inline u32
armv7_pmu_intens_read(void)
{
	u32 val;

	/* Read INTerrupt ENable Set (INTENS) register */
	asm volatile ("mrc p15, 0, %0, c9, c14, 1" : "=r"(val));
	return val;
}

static inline void
armv7_pmu_intens_write(u32 val)
{
	/* Write INTerrupt ENable Set (INTENS) register */
	asm volatile ("mcr p15, 0, %0, c9, c14, 1" : : "r"(val));
}

static inline void
armv7_pmu_intenc_write(u32 val)
{
	/* Write INTerrupt ENable Clear (INTENC) register */
	asm volatile ("mcr p15, 0, %0, c9, c14, 2" : : "r"(val));
}

static void enable_counter(int idx)
{
	armv7_pmu_cntens_write(1UL << idx);
}

static void disable_counter(int idx)
{
	armv7_pmu_cntenc_write(1UL << idx);
}

static void select_counter(unsigned int counter)
{
	armv7_pmu_pmnxsel_write(counter);
}

static int is_pmu_enabled(void)
{
	u32 pmnc = armv7_pmu_pmnc_read();

	if (pmnc & QUADD_ARMV7_PMNC_E) {
		u32 cnten = armv7_pmu_cntens_read();
		cnten &= pmu_ctx.counters_mask | QUADD_ARMV7_CCNT;
		return cnten ? 1 : 0;
	}

	return 0;
}

static u32 read_counter(int idx)
{
	u32 val;

	if (idx == QUADD_ARMV7_CCNT_BIT) {
		val = armv7_pmu_ccnt_read();
	} else {
		select_counter(idx);
		val = armv7_pmu_pmcnt_read();
	}

	return val;
}

static void write_counter(int idx, u32 value)
{
	if (idx == QUADD_ARMV7_CCNT_BIT) {
		armv7_pmu_ccnt_write(value);
	} else {
		select_counter(idx);
		armv7_pmu_pmcnt_write(value);
	}
}

static int
get_free_counters(unsigned long *bitmap, int nbits, int *ccntr)
{
	int cc;
	u32 cntens;

	cntens = armv7_pmu_cntens_read();
	cntens = ~cntens & (pmu_ctx.counters_mask | QUADD_ARMV7_CCNT);

	bitmap_zero(bitmap, nbits);
	bitmap_copy(bitmap, (unsigned long *)&cntens,
		    BITS_PER_BYTE * sizeof(u32));

	cc = (cntens & QUADD_ARMV7_CCNT) ? 1 : 0;

	if (ccntr)
		*ccntr = cc;

	return bitmap_weight(bitmap, BITS_PER_BYTE * sizeof(u32)) - cc;
}

static u32 armv7_pmu_adjust_value(u32 value, int event_id)
{
	/*
	* Cortex A8/A9: l1 cache performance counters
	* don't differentiate between read and write data accesses/misses,
	* so currently we are devided by two
	*/
	if (pmu_ctx.l1_cache_rw &&
	    (pmu_ctx.arch == QUADD_ARM_CPU_TYPE_CORTEX_A8 ||
	    pmu_ctx.arch == QUADD_ARM_CPU_TYPE_CORTEX_A9) &&
	    (event_id == QUADD_EVENT_TYPE_L1_DCACHE_READ_MISSES ||
	    event_id == QUADD_EVENT_TYPE_L1_DCACHE_WRITE_MISSES)) {
		return value / 2;
	}
	return value;
}

static void __maybe_unused
disable_interrupt(int idx)
{
	armv7_pmu_intenc_write(1UL << idx);
}

static void
disable_all_interrupts(void)
{
	u32 val = QUADD_ARMV7_CCNT | pmu_ctx.counters_mask;
	armv7_pmu_intenc_write(val);
}

static void
armv7_pmnc_reset_overflow_flags(void)
{
	u32 val = QUADD_ARMV7_CCNT | pmu_ctx.counters_mask;
	asm volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (val));
}

static void
select_event(unsigned int idx, unsigned int event)
{
	select_counter(idx);
	armv7_pmu_evtsel_write(event);
}

static void disable_all_counters(void)
{
	u32 val;

	/* Disable all counters */
	val = armv7_pmu_pmnc_read();
	if (val & QUADD_ARMV7_PMNC_E)
		armv7_pmu_pmnc_write(val & ~QUADD_ARMV7_PMNC_E);

	armv7_pmu_cntenc_write(QUADD_ARMV7_CCNT | pmu_ctx.counters_mask);
}

static void enable_all_counters(void)
{
	u32 val;

	/* Enable all counters */
	val = armv7_pmu_pmnc_read();
	val |= QUADD_ARMV7_PMNC_E | QUADD_ARMV7_PMNC_X;
	armv7_pmu_pmnc_write(val);
}

static void reset_all_counters(void)
{
	u32 val;

	val = armv7_pmu_pmnc_read();
	val |= QUADD_ARMV7_PMNC_P | QUADD_ARMV7_PMNC_C;
	armv7_pmu_pmnc_write(val);
}

static void quadd_init_pmu(void)
{
	armv7_pmnc_reset_overflow_flags();
	disable_all_interrupts();
}

static int pmu_enable(void)
{
	return 0;
}

static void __pmu_disable(void *arg)
{
	struct quadd_pmu_info *pi = &__get_cpu_var(cpu_pmu_info);

	if (!pi->is_already_active) {
		pr_info("[%d] reset all counters\n",
			smp_processor_id());

		disable_all_counters();
		reset_all_counters();
	} else {
		int idx;

		for_each_set_bit(idx, pi->used_cntrs, QUADD_MAX_PMU_COUNTERS) {
			pr_info("[%d] reset counter: %d\n",
				smp_processor_id(), idx);

			disable_counter(idx);
			write_counter(idx, 0);
		}
	}
}

static void pmu_disable(void)
{
	on_each_cpu(__pmu_disable, NULL, 1);
}

static void pmu_start(void)
{
	int idx = 0, pcntrs, ccntr;
	u32 event;
	DECLARE_BITMAP(free_bitmap, QUADD_MAX_PMU_COUNTERS);
	struct quadd_pmu_info *pi = &__get_cpu_var(cpu_pmu_info);
	u32 *prevp = pi->prev_vals;
	struct quadd_pmu_event_info *ei;

	bitmap_zero(pi->used_cntrs, QUADD_MAX_PMU_COUNTERS);

	if (is_pmu_enabled()) {
		pi->is_already_active = 1;
	} else {
		disable_all_counters();
		quadd_init_pmu();

		pi->is_already_active = 0;
	}

	pcntrs = get_free_counters(free_bitmap, QUADD_MAX_PMU_COUNTERS, &ccntr);

	list_for_each_entry(ei, &pmu_ctx.used_events, list) {
		int index;

		*prevp++ = 0;

		event = ei->hw_value;

		if (ei->quadd_event_id == QUADD_EVENT_TYPE_CPU_CYCLES) {
			if (!ccntr) {
				pr_err_once("Error: cpu cycles counter is already occupied\n");
				return;
			}
			index = QUADD_ARMV7_CCNT_BIT;
		} else {
			if (!pcntrs--) {
				pr_err_once("Error: too many performance events\n");
				return;
			}

			index = find_next_bit(free_bitmap,
					      QUADD_MAX_PMU_COUNTERS, idx);
			if (index >= QUADD_MAX_PMU_COUNTERS) {
				pr_err_once("Error: too many events\n");
				return;
			}
			idx = index + 1;
			select_event(index, event);
		}
		set_bit(index, pi->used_cntrs);

		write_counter(index, 0);
		enable_counter(index);
	}

	if (!pi->is_already_active) {
		reset_all_counters();
		enable_all_counters();
	}

	qm_debug_start_source(QUADD_EVENT_SOURCE_PMU);
}

static void pmu_stop(void)
{
	int idx;
	struct quadd_pmu_info *pi = &__get_cpu_var(cpu_pmu_info);

	if (!pi->is_already_active) {
		disable_all_counters();
		reset_all_counters();
	} else {
		for_each_set_bit(idx, pi->used_cntrs, QUADD_MAX_PMU_COUNTERS) {
			disable_counter(idx);
			write_counter(idx, 0);
		}
	}

	qm_debug_stop_source(QUADD_EVENT_SOURCE_PMU);
}

static int __maybe_unused
pmu_read(struct event_data *events, int max_events)
{
	u32 val;
	int idx = 0, i = 0;
	struct quadd_pmu_info *pi = &__get_cpu_var(cpu_pmu_info);
	u32 *prevp = pi->prev_vals;
	struct quadd_pmu_event_info *ei;

	if (bitmap_empty(pi->used_cntrs, QUADD_MAX_PMU_COUNTERS)) {
		pr_err_once("Error: counters were not initialized\n");
		return 0;
	}

	list_for_each_entry(ei, &pmu_ctx.used_events, list) {
		int index;

		if (ei->quadd_event_id == QUADD_EVENT_TYPE_CPU_CYCLES) {
			if (!test_bit(QUADD_ARMV7_CCNT_BIT, pi->used_cntrs)) {
				pr_err_once("Error: ccntr is not used\n");
				return 0;
			}
			index = QUADD_ARMV7_CCNT_BIT;
		} else {
			index = find_next_bit(pi->used_cntrs,
					      QUADD_MAX_PMU_COUNTERS, idx);
			idx = index + 1;

			if (index >= QUADD_MAX_PMU_COUNTERS) {
				pr_err_once("Error: perf counter is not used\n");
				return 0;
			}
		}

		val = read_counter(index);
		val = armv7_pmu_adjust_value(val, ei->quadd_event_id);

		events->event_source = QUADD_EVENT_SOURCE_PMU;
		events->event_id = ei->quadd_event_id;

		events->val = val;
		events->prev_val = *prevp;

		*prevp = val;

		qm_debug_read_counter(events->event_id, events->prev_val,
				      events->val);

		if (++i >= max_events)
			break;

		events++;
		prevp++;
	}

	return i;
}

static int __maybe_unused
pmu_read_emulate(struct event_data *events, int max_events)
{
	int i = 0;
	static u32 val = 100;
	struct quadd_pmu_info *pi = &__get_cpu_var(cpu_pmu_info);
	u32 *prevp = pi->prev_vals;
	struct quadd_pmu_event_info *ei;

	list_for_each_entry(ei, &pmu_ctx.used_events, list) {
		if (val > 200)
			val = 100;

		events->event_id = *prevp;
		events->val = val;

		*prevp = val;
		val += 5;

		if (++i >= max_events)
			break;

		events++;
		prevp++;
	}

	return i;
}

static void __get_free_counters(void *arg)
{
	int pcntrs, ccntr;
	DECLARE_BITMAP(free_bitmap, QUADD_MAX_PMU_COUNTERS);
	struct quadd_cntrs_info *ci = arg;

	pcntrs = get_free_counters(free_bitmap, QUADD_MAX_PMU_COUNTERS, &ccntr);

	spin_lock(&ci->lock);

	ci->pcntrs = min_t(int, pcntrs, ci->pcntrs);

	if (!ccntr)
		ci->ccntr = 0;

	pr_info("[%d] pcntrs/ccntr: %d/%d, free_bitmap: %#lx\n",
		smp_processor_id(), pcntrs, ccntr, free_bitmap[0]);

	spin_unlock(&ci->lock);
}

static void free_events(struct list_head *head)
{
	struct quadd_pmu_event_info *entry, *next;

	list_for_each_entry_safe(entry, next, head, list) {
		list_del(&entry->list);
		kfree(entry);
	}
}

static int set_events(int *events, int size)
{
	int free_pcntrs, err;
	int i, nr_l1_r = 0, nr_l1_w = 0;
	struct quadd_cntrs_info free_ci;

	pmu_ctx.l1_cache_rw = 0;

	free_events(&pmu_ctx.used_events);

	if (!events || !size)
		return 0;

	if (!pmu_ctx.current_map) {
		pr_err("Invalid current_map\n");
		return -ENODEV;
	}

	spin_lock_init(&free_ci.lock);
	free_ci.pcntrs = QUADD_MAX_PMU_COUNTERS;
	free_ci.ccntr = 1;

	on_each_cpu(__get_free_counters, &free_ci, 1);

	free_pcntrs = free_ci.pcntrs;
	pr_info("free counters: pcntrs/ccntr: %d/%d\n",
		free_pcntrs, free_ci.ccntr);

	for (i = 0; i < size; i++) {
		struct quadd_pmu_event_info *ei;

		if (events[i] > QUADD_EVENT_TYPE_MAX) {
			pr_err("Error event: %d\n", events[i]);
			err = -EINVAL;
			goto out_free;
		}

		ei = kzalloc(sizeof(*ei), GFP_KERNEL);
		if (!ei) {
			err = -ENOMEM;
			goto out_free;
		}

		INIT_LIST_HEAD(&ei->list);
		list_add_tail(&ei->list, &pmu_ctx.used_events);

		if (events[i] == QUADD_EVENT_TYPE_CPU_CYCLES) {
			ei->hw_value = QUADD_ARMV7_CPU_CYCLE_EVENT;
			if (!free_ci.ccntr) {
				pr_err("Error: cpu cycles counter is already occupied\n");
				err = -EBUSY;
				goto out_free;
			}
		} else {
			if (!free_pcntrs--) {
				pr_err("Error: too many performance events\n");
				err = -ENOSPC;
				goto out_free;
			}

			ei->hw_value = pmu_ctx.current_map[events[i]];
		}

		ei->quadd_event_id = events[i];

		if (events[i] == QUADD_EVENT_TYPE_L1_DCACHE_READ_MISSES)
			nr_l1_r++;
		else if (events[i] == QUADD_EVENT_TYPE_L1_DCACHE_WRITE_MISSES)
			nr_l1_w++;

		pr_info("Event has been added: id/pmu value: %s/%#x\n",
			quadd_get_event_str(events[i]),
			ei->hw_value);
	}

	if (nr_l1_r > 0 && nr_l1_w > 0)
		pmu_ctx.l1_cache_rw = 1;

	return 0;

out_free:
	free_events(&pmu_ctx.used_events);
	return err;
}

static int get_supported_events(int *events, int max_events)
{
	int i, nr_events = 0;

	max_events = min_t(int, QUADD_EVENT_TYPE_MAX, max_events);

	for (i = 0; i < max_events; i++) {
		if (pmu_ctx.current_map[i] != QUADD_ARMV7_UNSUPPORTED_EVENT)
			events[nr_events++] = i;
	}
	return nr_events;
}

static int get_current_events(int *events, int max_events)
{
	int i = 0;
	struct quadd_pmu_event_info *ei;

	list_for_each_entry(ei, &pmu_ctx.used_events, list) {
		events[i++] = ei->quadd_event_id;

		if (i >= max_events)
			break;
	}

	return i;
}

static struct quadd_event_source_interface pmu_armv7_int = {
	.enable			= pmu_enable,
	.disable		= pmu_disable,

	.start			= pmu_start,
	.stop			= pmu_stop,

#ifndef QUADD_USE_EMULATE_COUNTERS
	.read			= pmu_read,
#else
	.read			= pmu_read_emulate,
#endif
	.set_events		= set_events,
	.get_supported_events	= get_supported_events,
	.get_current_events	= get_current_events,
};

struct quadd_event_source_interface *quadd_armv7_pmu_init(void)
{
	struct quadd_event_source_interface *pmu = NULL;
	unsigned long cpu_id, cpu_implementer, part_number;

	cpu_id = read_cpuid_id();
	cpu_implementer = cpu_id >> 24;
	part_number = cpu_id & 0xFFF0;

	if (cpu_implementer == ARM_CPU_IMP_ARM) {
		switch (part_number) {
		case ARM_CPU_PART_CORTEX_A9:
			pmu_ctx.arch = QUADD_ARM_CPU_TYPE_CORTEX_A9;
			strcpy(pmu_ctx.arch_name, "Cortex A9");
			pmu_ctx.counters_mask =
				QUADD_ARMV7_COUNTERS_MASK_CORTEX_A9;
			pmu_ctx.current_map = quadd_armv7_a9_events_map;
			pmu = &pmu_armv7_int;
			break;

		case ARM_CPU_PART_CORTEX_A15:
			pmu_ctx.arch = QUADD_ARM_CPU_TYPE_CORTEX_A15;
			strcpy(pmu_ctx.arch_name, "Cortex A15");
			pmu_ctx.counters_mask =
				QUADD_ARMV7_COUNTERS_MASK_CORTEX_A15;
			pmu_ctx.current_map = quadd_armv7_a15_events_map;
			pmu = &pmu_armv7_int;
			break;

		default:
			pmu_ctx.arch = QUADD_ARM_CPU_TYPE_UNKNOWN;
			strcpy(pmu_ctx.arch_name, "Unknown");
			pmu_ctx.current_map = NULL;
			break;
		}
	}

	INIT_LIST_HEAD(&pmu_ctx.used_events);

	pr_info("arch: %s\n", pmu_ctx.arch_name);

	return pmu;
}

void quadd_armv7_pmu_deinit(void)
{
	free_events(&pmu_ctx.used_events);
}
