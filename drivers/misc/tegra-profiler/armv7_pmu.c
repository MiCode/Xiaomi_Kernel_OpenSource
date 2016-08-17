/*
 * drivers/misc/tegra-profiler/armv7_pmu.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <asm/cputype.h>
#include <asm/pmu.h>

#include <linux/tegra_profiler.h>

#include "armv7_pmu.h"
#include "quadd.h"
#include "debug.h"

static struct armv7_pmu_ctx pmu_ctx;

DEFINE_PER_CPU(u32[QUADD_MAX_PMU_COUNTERS], pmu_prev_val);

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

static u32 armv7_pmu_pmnc_read(void)
{
	u32 val;
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
	return val;
}

static void armv7_pmu_pmnc_write(u32 val)
{
	val &= QUADD_ARMV7_PMNC_MASK;
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(val));
}

static void armv7_pmu_pmnc_enable_counter(int index)
{
	u32 val;

	if (index == QUADD_ARMV7_CYCLE_COUNTER)
		val = QUADD_ARMV7_CCNT;
	else
		val = 1 << index;

	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (val));
}

static void armv7_pmu_select_counter(unsigned int idx)
{
	u32 val;

	val = idx & QUADD_ARMV7_SELECT_MASK;
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (val));
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

static u32 armv7_pmu_read_counter(int idx)
{
	u32 val = 0;

	if (idx == QUADD_ARMV7_CYCLE_COUNTER) {
		/* Cycle count register (PMCCNTR) reading */
		asm volatile ("MRC p15, 0, %0, c9, c13, 0" : "=r"(val));
	} else {
		/* counter selection*/
		armv7_pmu_select_counter(idx);
		/* event count register reading */
		asm volatile ("MRC p15, 0, %0, c9, c13, 2" : "=r"(val));
	}

	return val;
}

static __attribute__((unused)) void armv7_pmu_write_counter(int idx, u32 value)
{
	if (idx == QUADD_ARMV7_CYCLE_COUNTER) {
		/* Cycle count register (PMCCNTR) writing */
		asm volatile("mcr p15, 0, %0, c9, c13, 0" : : "r" (value));
	} else {
		/* counter selection*/
		armv7_pmu_select_counter(idx);
		/* event count register writing */
		asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r" (value));
	}
}

static void armv7_pmu_event_select(u32 event)
{
	event &= QUADD_ARMV7_EVTSEL_MASK;
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r" (event));
}

static __attribute__((unused)) void armv7_pmnc_enable_interrupt(int idx)
{
	u32 val;

	if (idx == QUADD_ARMV7_CYCLE_COUNTER)
		val = QUADD_ARMV7_CCNT;
	else
		val = 1 << idx;

	asm volatile("mcr p15, 0, %0, c9, c14, 1" : : "r" (val));
}

static __attribute__((unused)) void armv7_pmnc_disable_interrupt(int idx)
{
	u32 val;

	if (idx == QUADD_ARMV7_CYCLE_COUNTER)
		val = QUADD_ARMV7_CCNT;
	else
		val = 1 << idx;

	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r" (val));
}

static void armv7_pmnc_disable_all_interrupts(void)
{
	u32 val = QUADD_ARMV7_CCNT | pmu_ctx.counters_mask;

	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r" (val));
}

static void armv7_pmnc_reset_overflow_flags(void)
{
	u32 val = QUADD_ARMV7_CCNT | pmu_ctx.counters_mask;

	asm volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (val));
}

static inline void select_event(unsigned int idx, unsigned int event)
{
	/* counter selection */
	armv7_pmu_select_counter(idx);
	armv7_pmu_event_select(event);
}

static inline void disable_all_counters(void)
{
	u32 val;

	/* Disable all counters */
	val = armv7_pmu_pmnc_read();
	if (val & QUADD_ARMV7_PMNC_E)
		armv7_pmu_pmnc_write(val & ~QUADD_ARMV7_PMNC_E);
}

static inline void enable_all_counters(void)
{
	u32 val;

	/* Enable all counters */
	val = armv7_pmu_pmnc_read();
	val |= QUADD_ARMV7_PMNC_E | QUADD_ARMV7_PMNC_X;
	armv7_pmu_pmnc_write(val);
}

static inline void quadd_init_pmu(void)
{
	armv7_pmnc_reset_overflow_flags();
	armv7_pmnc_disable_all_interrupts();
}

static inline void reset_all_counters(void)
{
	u32 val;

	val = armv7_pmu_pmnc_read();
	val |= QUADD_ARMV7_PMNC_P | QUADD_ARMV7_PMNC_C;
	armv7_pmu_pmnc_write(val);
}

static int pmu_enable(void)
{
	int err;

	err = reserve_pmu(ARM_PMU_DEVICE_CPU);
	if (err) {
		pr_err("error: pmu was not reserved\n");
		return err;
	}
	pr_info("pmu was reserved\n");
	return 0;
}

static void pmu_disable(void)
{
	release_pmu(ARM_PMU_DEVICE_CPU);
	pr_info("pmu was released\n");
}

static void pmu_start(void)
{
	int i, idx;
	u32 event;
	u32 *prevp = __get_cpu_var(pmu_prev_val);

	disable_all_counters();
	quadd_init_pmu();

	for (i = 0; i < pmu_ctx.nr_used_counters; i++) {
		struct quadd_pmu_event_info *pmu_event = &pmu_ctx.pmu_events[i];

		prevp[i] = 0;

		event = pmu_event->hw_value;
		idx = pmu_event->counter_idx;

		if (idx != QUADD_ARMV7_CYCLE_COUNTER)
			select_event(idx, event);

		armv7_pmu_pmnc_enable_counter(idx);
	}

	reset_all_counters();
	enable_all_counters();

	qm_debug_start_source(QUADD_EVENT_SOURCE_PMU);
}

static void pmu_stop(void)
{
	reset_all_counters();
	disable_all_counters();

	qm_debug_stop_source(QUADD_EVENT_SOURCE_PMU);
}

static int __maybe_unused pmu_read(struct event_data *events)
{
	int idx, i;
	u32 val;
	u32 *prevp = __get_cpu_var(pmu_prev_val);

	if (pmu_ctx.nr_used_counters == 0) {
		pr_warn_once("error: counters were not initialized\n");
		return 0;
	}

	for (i = 0; i < pmu_ctx.nr_used_counters; i++) {
		struct quadd_pmu_event_info *pmu_event = &pmu_ctx.pmu_events[i];

		idx = pmu_event->counter_idx;

		val = armv7_pmu_read_counter(idx);
		val = armv7_pmu_adjust_value(val, pmu_event->quadd_event_id);

		events[i].event_source = QUADD_EVENT_SOURCE_PMU;
		events[i].event_id = pmu_event->quadd_event_id;

		events[i].val = val;
		events[i].prev_val = prevp[i];

		prevp[i] = val;

		qm_debug_read_counter(events[i].event_id, events[i].prev_val,
				      events[i].val);
	}

	return pmu_ctx.nr_used_counters;
}

static int __maybe_unused pmu_read_emulate(struct event_data *events)
{
	int i;
	static u32 val = 100;
	u32 *prevp = __get_cpu_var(pmu_prev_val);

	for (i = 0; i < pmu_ctx.nr_used_counters; i++) {
		if (val > 200)
			val = 100;

		events[i].event_id = prevp[i];
		events[i].val = val;

		val += 5;
	}

	return pmu_ctx.nr_used_counters;
}

static int set_events(int *events, int size)
{
	int i, nr_l1_r = 0, nr_l1_w = 0, curr_idx = 0;

	pmu_ctx.l1_cache_rw = 0;
	pmu_ctx.nr_used_counters = 0;

	if (!events || size == 0)
		return 0;

	if (size > QUADD_MAX_PMU_COUNTERS) {
		pr_err("Too many events (> %d)\n", QUADD_MAX_PMU_COUNTERS);
		return -ENOSPC;
	}

	if (!pmu_ctx.current_map) {
		pr_err("Invalid current_map\n");
		return -ENODEV;
	}

	for (i = 0; i < size; i++) {
		struct quadd_pmu_event_info *pmu_event = &pmu_ctx.pmu_events[i];

		if (events[i] > QUADD_EVENT_TYPE_MAX) {
			pr_err("Error event: %d\n", events[i]);
			return -EINVAL;
		}

		if (curr_idx >= pmu_ctx.nr_counters) {
			pr_err("Too many events (> %d)\n",
			       pmu_ctx.nr_counters);
			return -ENOSPC;
		}

		if (events[i] == QUADD_EVENT_TYPE_CPU_CYCLES) {
			pmu_event->hw_value = QUADD_ARMV7_CPU_CYCLE_EVENT;
			pmu_event->counter_idx = QUADD_ARMV7_CYCLE_COUNTER;
		} else {
			pmu_event->hw_value = pmu_ctx.current_map[events[i]];
			pmu_event->counter_idx = curr_idx++;
		}
		pmu_event->quadd_event_id = events[i];

		if (events[i] == QUADD_EVENT_TYPE_L1_DCACHE_READ_MISSES)
			nr_l1_r++;
		else if (events[i] == QUADD_EVENT_TYPE_L1_DCACHE_WRITE_MISSES)
			nr_l1_w++;

		pr_info("Event has been added: id/pmu value: %s/%#x\n",
			quadd_get_event_str(events[i]),
			pmu_event->hw_value);
	}
	pmu_ctx.nr_used_counters = size;

	if (nr_l1_r > 0 && nr_l1_w > 0)
		pmu_ctx.l1_cache_rw = 1;

	return 0;
}

static int get_supported_events(int *events)
{
	int i, nr_events = 0;

	for (i = 0; i < QUADD_EVENT_TYPE_MAX; i++) {
		if (pmu_ctx.current_map[i] != QUADD_ARMV7_UNSUPPORTED_EVENT)
			events[nr_events++] = i;
	}
	return nr_events;
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
};

struct quadd_event_source_interface *quadd_armv7_pmu_init(void)
{
	struct quadd_event_source_interface *pmu = NULL;
	unsigned long cpu_id, cpu_implementer, part_number;

	cpu_id = read_cpuid_id();
	cpu_implementer = cpu_id >> 24;
	part_number = cpu_id & 0xFFF0;

	if (cpu_implementer == QUADD_ARM_CPU_IMPLEMENTER) {
		switch (part_number) {
		case QUADD_ARM_CPU_PART_NUMBER_CORTEX_A9:
			pmu_ctx.arch = QUADD_ARM_CPU_TYPE_CORTEX_A9;
			strcpy(pmu_ctx.arch_name, "Cortex A9");
			pmu_ctx.nr_counters = 6;
			pmu_ctx.counters_mask =
				QUADD_ARMV7_COUNTERS_MASK_CORTEX_A9;
			pmu_ctx.current_map = quadd_armv7_a9_events_map;
			pmu = &pmu_armv7_int;
			break;

		case QUADD_ARM_CPU_PART_NUMBER_CORTEX_A15:
			pmu_ctx.arch = QUADD_ARM_CPU_TYPE_CORTEX_A15;
			strcpy(pmu_ctx.arch_name, "Cortex A15");
			pmu_ctx.nr_counters = 6;
			pmu_ctx.counters_mask =
				QUADD_ARMV7_COUNTERS_MASK_CORTEX_A15;
			pmu_ctx.current_map = quadd_armv7_a15_events_map;
			pmu = &pmu_armv7_int;
			break;

		default:
			pmu_ctx.arch = QUADD_ARM_CPU_TYPE_UNKNOWN;
			strcpy(pmu_ctx.arch_name, "Unknown");
			pmu_ctx.nr_counters = 0;
			pmu_ctx.current_map = NULL;
			break;
		}
	}

	pr_info("arch: %s, number of counters: %d\n",
		pmu_ctx.arch_name, pmu_ctx.nr_counters);
	return pmu;
}
