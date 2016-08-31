/*
 * drivers/misc/tegra-profiler/armv8_pmu.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#include <linux/printk.h>
#include <linux/types.h>
#include <linux/string.h>

#include <linux/version.h>
#include <linux/err.h>
#include <linux/bitmap.h>
#include <linux/slab.h>

#include <asm/cputype.h>

#include "arm_pmu.h"
#include "armv8_pmu.h"
#include "armv8_events.h"
#include "quadd.h"
#include "debug.h"

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

static struct quadd_pmu_ctx pmu_ctx;

static unsigned quadd_armv8_pmuv3_events_map[QUADD_EVENT_TYPE_MAX] = {
	[QUADD_EVENT_TYPE_INSTRUCTIONS] =
		QUADD_ARMV8_HW_EVENT_INSTR_EXECUTED,
	[QUADD_EVENT_TYPE_BRANCH_INSTRUCTIONS] =
		QUADD_ARMV8_UNSUPPORTED_EVENT,
	[QUADD_EVENT_TYPE_BRANCH_MISSES] =
		QUADD_ARMV8_HW_EVENT_PC_BRANCH_MIS_PRED,
	[QUADD_EVENT_TYPE_BUS_CYCLES] =
		QUADD_ARMV8_UNSUPPORTED_EVENT,

	[QUADD_EVENT_TYPE_L1_DCACHE_READ_MISSES] =
		QUADD_ARMV8_HW_EVENT_L1_DCACHE_REFILL,
	[QUADD_EVENT_TYPE_L1_DCACHE_WRITE_MISSES] =
		QUADD_ARMV8_HW_EVENT_L1_DCACHE_REFILL,
	[QUADD_EVENT_TYPE_L1_ICACHE_MISSES] =
		QUADD_ARMV8_HW_EVENT_L1_ICACHE_REFILL,

	[QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES] =
		QUADD_ARMV8_UNSUPPORTED_EVENT,
	[QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES] =
		QUADD_ARMV8_UNSUPPORTED_EVENT,
	[QUADD_EVENT_TYPE_L2_ICACHE_MISSES] =
		QUADD_ARMV8_UNSUPPORTED_EVENT,
};

/*********************************************************************/

static inline u32
armv8_pmu_pmcr_read(void)
{
	u32 val;

	/* Read Performance Monitors Control Register */
	asm volatile("mrs %0, pmcr_el0" : "=r" (val));
	return val;
}

static inline void
armv8_pmu_pmcr_write(u32 val)
{
	asm volatile("msr pmcr_el0, %0" : :
		     "r" (val & QUADD_ARMV8_PMCR_WR_MASK));
}

static inline u32
armv8_pmu_pmceid_read(void)
{
	u32 val;

	/* Read Performance Monitors Common Event Identification Register */
	asm volatile("mrs %0, pmceid0_el0" : "=r" (val));
	return val;
}

static inline u32
armv8_pmu_pmcntenset_read(void)
{
	u32 val;

	/* Read Performance Monitors Count Enable Set Register */
	asm volatile("mrs %0, pmcntenset_el0" : "=r" (val));
	return val;
}

static inline void
armv8_pmu_pmcntenset_write(u32 val)
{
	/* Write Performance Monitors Count Enable Set Register */
	asm volatile("msr pmcntenset_el0, %0" : : "r" (val));
}

static inline void
armv8_pmu_pmcntenclr_write(u32 val)
{
	/* Write Performance Monitors Count Enable Clear Register */
	asm volatile("msr pmcntenclr_el0, %0" : : "r" (val));
}

static inline void
armv8_pmu_pmselr_write(u32 val)
{
	/* Write Performance Monitors Event Counter Selection Register */
	asm volatile("msr pmselr_el0, %0" : :
		     "r" (val & QUADD_ARMV8_SELECT_MASK));
}

static inline u64
armv8_pmu_pmccntr_read(void)
{
	u64 val;

	/* Read Performance Monitors Cycle Count Register */
	asm volatile("mrs %0, pmccntr_el0" : "=r" (val));
	return val;
}

static inline void
armv8_pmu_pmccntr_write(u64 val)
{
	/* Write Performance Monitors Selected Event Count Register */
	asm volatile("msr pmccntr_el0, %0" : : "r" (val));
}

static inline u32
armv8_pmu_pmxevcntr_read(void)
{
	u32 val;

	/* Read Performance Monitors Selected Event Count Register */
	asm volatile("mrs %0, pmxevcntr_el0" : "=r" (val));
	return val;
}

static inline void
armv8_pmu_pmxevcntr_write(u32 val)
{
	/* Write Performance Monitors Selected Event Count Register */
	asm volatile("msr pmxevcntr_el0, %0" : : "r" (val));
}

static inline void
armv8_pmu_pmxevtyper_write(u32 event)
{
	/* Write Performance Monitors Selected Event Type Register */
	asm volatile("msr pmxevtyper_el0, %0" : :
		     "r" (event & QUADD_ARMV8_EVTSEL_MASK));
}

static inline u32
armv8_pmu_pmintenset_read(void)
{
	u32 val;

	/* Read Performance Monitors Interrupt Enable Set Register */
	asm volatile("mrs %0, pmintenset_el1" : "=r" (val));
	return val;
}

static inline void
armv8_pmu_pmintenset_write(u32 val)
{
	/* Write Performance Monitors Interrupt Enable Set Register */
	asm volatile("msr pmintenset_el1, %0" : : "r" (val));
}

static inline void
armv8_pmu_pmintenclr_write(u32 val)
{
	/* Write Performance Monitors Interrupt Enable Clear Register */
	asm volatile("msr pmintenclr_el1, %0" : : "r" (val));
}

static inline u32
armv8_pmu_pmovsclr_read(void)
{
	u32 val;

	/* Read Performance Monitors Overflow Flag Status Register */
	asm volatile("mrs %0, pmovsclr_el0" : "=r" (val));
	return val;
}

static inline void
armv8_pmu_pmovsclr_write(int idx)
{
	/* Write Performance Monitors Overflow Flag Status Register */
	asm volatile("msr pmovsclr_el0, %0" : : "r" (BIT(idx)));
}

/*********************************************************************/



static void enable_counter(int idx)
{
	armv8_pmu_pmcntenset_write(BIT(idx));
}

static void disable_counter(int idx)
{
	armv8_pmu_pmcntenclr_write(BIT(idx));
}

static void select_counter(unsigned int counter)
{
	armv8_pmu_pmselr_write(counter);
}

static int is_pmu_enabled(void)
{
	u32 pmcr = armv8_pmu_pmcr_read();

	if (pmcr & QUADD_ARMV8_PMCR_E) {
		u32 pmcnten = armv8_pmu_pmcntenset_read();
		pmcnten &= pmu_ctx.counters_mask | QUADD_ARMV8_CCNT;
		return pmcnten ? 1 : 0;
	}

	return 0;
}

static u32 read_counter(int idx)
{
	u32 val;

	if (idx == QUADD_ARMV8_CCNT_BIT) {
		val = armv8_pmu_pmccntr_read();
	} else {
		select_counter(idx);
		val = armv8_pmu_pmxevcntr_read();
	}

	return val;
}

static void write_counter(int idx, u32 value)
{
	if (idx == QUADD_ARMV8_CCNT_BIT) {
		armv8_pmu_pmccntr_write(value);
	} else {
		select_counter(idx);
		armv8_pmu_pmxevcntr_write(value);
	}
}

static int
get_free_counters(unsigned long *bitmap, int nbits, int *ccntr)
{
	int cc;
	u32 cntens;

	cntens = armv8_pmu_pmcntenset_read();
	cntens = ~cntens & (pmu_ctx.counters_mask | QUADD_ARMV8_CCNT);

	bitmap_zero(bitmap, nbits);
	bitmap_copy(bitmap, (unsigned long *)&cntens,
		    BITS_PER_BYTE * sizeof(u32));

	cc = (cntens & QUADD_ARMV8_CCNT) ? 1 : 0;

	if (ccntr)
		*ccntr = cc;

	return bitmap_weight(bitmap, BITS_PER_BYTE * sizeof(u32)) - cc;
}

static void __maybe_unused
disable_interrupt(int idx)
{
	armv8_pmu_pmintenclr_write(BIT(idx));
}

static void
disable_all_interrupts(void)
{
	u32 val = QUADD_ARMV8_CCNT | pmu_ctx.counters_mask;
	armv8_pmu_pmintenclr_write(val);
}

static void
reset_overflow_flags(void)
{
	u32 val = QUADD_ARMV8_CCNT | pmu_ctx.counters_mask;
	armv8_pmu_pmovsclr_write(val);
}

static void
select_event(unsigned int idx, unsigned int event)
{
	select_counter(idx);
	armv8_pmu_pmxevtyper_write(event);
}

static void disable_all_counters(void)
{
	u32 val;

	/* Disable all counters */
	val = armv8_pmu_pmcr_read();
	if (val & QUADD_ARMV8_PMCR_E)
		armv8_pmu_pmcr_write(val & ~QUADD_ARMV8_PMCR_E);

	armv8_pmu_pmcntenclr_write(QUADD_ARMV8_CCNT | pmu_ctx.counters_mask);
}

static void enable_all_counters(void)
{
	u32 val;

	/* Enable all counters */
	val = armv8_pmu_pmcr_read();
	val |= QUADD_ARMV8_PMCR_E | QUADD_ARMV8_PMCR_X;
	armv8_pmu_pmcr_write(val);
}

static void reset_all_counters(void)
{
	u32 val;

	val = armv8_pmu_pmcr_read();
	val |= QUADD_ARMV8_PMCR_P | QUADD_ARMV8_PMCR_C;
	armv8_pmu_pmcr_write(val);
}

static void quadd_init_pmu(void)
{
	reset_overflow_flags();
	disable_all_interrupts();
}

static int pmu_enable(void)
{
	pr_info("pmu was reserved\n");
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
	pr_info("pmu was released\n");
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
			index = QUADD_ARMV8_CCNT_BIT;
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
			if (!test_bit(QUADD_ARMV8_CCNT_BIT, pi->used_cntrs)) {
				pr_err_once("Error: ccntr is not used\n");
				return 0;
			}
			index = QUADD_ARMV8_CCNT_BIT;
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

	pr_info("event identification register: %#x\n",
		armv8_pmu_pmceid_read());

	for (i = 0; i < size; i++) {
		struct quadd_pmu_event_info *ei;

		if (events[i] > QUADD_EVENT_TYPE_MAX) {
			pr_err("error event: %d\n", events[i]);
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
			ei->hw_value = QUADD_ARMV8_CPU_CYCLE_EVENT;
			if (!free_ci.ccntr) {
				pr_err("error: cpu cycles counter is already occupied\n");
				err = -EBUSY;
				goto out_free;
			}
		} else {
			if (!free_pcntrs--) {
				pr_err("error: too many performance events\n");
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
		if (pmu_ctx.current_map[i] != QUADD_ARMV8_UNSUPPORTED_EVENT)
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

/*********************************************************************/

static struct quadd_event_source_interface pmu_armv8_int = {
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

struct quadd_event_source_interface *quadd_armv8_pmu_init(void)
{
	u32 pmcr, imp, idcode;
	struct quadd_event_source_interface *pmu = NULL;

	u64 aa64_dfr = read_cpuid(ID_AA64DFR0_EL1);
	aa64_dfr = (aa64_dfr >> 8) & 0x0f;

	pmu_ctx.arch = QUADD_AA64_CPU_TYPE_UNKNOWN;

	switch (aa64_dfr) {
	case QUADD_AA64_PMUVER_PMUV3:
		strcpy(pmu_ctx.arch_name, "AA64 PmuV3");
		pmu_ctx.counters_mask =
			QUADD_ARMV8_COUNTERS_MASK_PMUV3;
		pmu_ctx.current_map = quadd_armv8_pmuv3_events_map;

		pmcr = armv8_pmu_pmcr_read();

		idcode = (pmcr >> QUADD_ARMV8_PMCR_IDCODE_SHIFT) &
			QUADD_ARMV8_PMCR_IDCODE_MASK;
		imp = pmcr >> QUADD_ARMV8_PMCR_IMP_SHIFT;

		pr_info("imp: %#x, idcode: %#x\n", imp, idcode);

		if (imp == ARM_CPU_IMP_ARM) {
			strcat(pmu_ctx.arch_name, " ARM");
			if (idcode == QUADD_AA64_CPU_IDCODE_CORTEX_A57) {
				pmu_ctx.arch = QUADD_AA64_CPU_TYPE_CORTEX_A57;
				strcat(pmu_ctx.arch_name, " CORTEX_A57");
			} else {
				pmu_ctx.arch = QUADD_AA64_CPU_TYPE_ARM;
			}
		} else if (imp == QUADD_AA64_CPU_IMP_NVIDIA) {
			strcat(pmu_ctx.arch_name, " Nvidia");
			pmu_ctx.arch = QUADD_AA64_CPU_TYPE_DENVER;
		} else {
			strcat(pmu_ctx.arch_name, " Unknown");
			pmu_ctx.arch = QUADD_AA64_CPU_TYPE_UNKNOWN_IMP;
		}

		pmu = &pmu_armv8_int;
		break;

	default:
		pr_err("error: incorrect PMUVer\n");
		break;
	}

	INIT_LIST_HEAD(&pmu_ctx.used_events);

	pr_info("arch: %s\n", pmu_ctx.arch_name);

	return pmu;
}

void quadd_armv8_pmu_deinit(void)
{
	free_events(&pmu_ctx.used_events);
}
