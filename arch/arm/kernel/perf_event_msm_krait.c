/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <asm/system.h>

#ifdef CONFIG_CPU_V7
#define KRAIT_EVT_PREFIX 1
#define KRAIT_MAX_L1_REG 2
/*
   event encoding:                prccg
   p  = prefix (1 for Krait L1)
   r  = register
   cc = code
   g  = group
*/
#define KRAIT_L1_ICACHE_MISS    0x10010
#define KRAIT_L1_ICACHE_ACCESS  0x10011
#define KRAIT_DTLB_ACCESS       0x121B2
#define KRAIT_ITLB_ACCESS       0x121C0

u32 evt_type_base[] = {0x4c, 0x50, 0x54};

static const unsigned armv7_krait_perf_map[PERF_COUNT_HW_MAX] = {
	[PERF_COUNT_HW_CPU_CYCLES]	    = ARMV7_PERFCTR_CPU_CYCLES,
	[PERF_COUNT_HW_INSTRUCTIONS]	    = ARMV7_PERFCTR_INSTR_EXECUTED,
	[PERF_COUNT_HW_CACHE_REFERENCES]    = HW_OP_UNSUPPORTED,
	[PERF_COUNT_HW_CACHE_MISSES]	    = HW_OP_UNSUPPORTED,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = ARMV7_PERFCTR_PC_WRITE,
	[PERF_COUNT_HW_BRANCH_MISSES]	    = ARMV7_PERFCTR_PC_BRANCH_MIS_PRED,
	[PERF_COUNT_HW_BUS_CYCLES]	    = ARMV7_PERFCTR_CLOCK_CYCLES,
};

static const unsigned armv7_krait_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					  [PERF_COUNT_HW_CACHE_OP_MAX]
					  [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	[C(L1D)] = {
		/*
		 * The performance counters don't differentiate between read
		 * and write accesses/misses so this isn't strictly correct,
		 * but it's the best we can do. Writes and reads get
		 * combined.
		 */
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= ARMV7_PERFCTR_DCACHE_ACCESS,
			[C(RESULT_MISS)]	= ARMV7_PERFCTR_DCACHE_REFILL,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= ARMV7_PERFCTR_DCACHE_ACCESS,
			[C(RESULT_MISS)]	= ARMV7_PERFCTR_DCACHE_REFILL,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(L1I)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= KRAIT_L1_ICACHE_ACCESS,
			[C(RESULT_MISS)]	= KRAIT_L1_ICACHE_MISS,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= KRAIT_L1_ICACHE_ACCESS,
			[C(RESULT_MISS)]	= KRAIT_L1_ICACHE_MISS,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(LL)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(DTLB)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= KRAIT_DTLB_ACCESS,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= KRAIT_DTLB_ACCESS,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(ITLB)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= KRAIT_ITLB_ACCESS,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= KRAIT_ITLB_ACCESS,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(BPU)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]
					= ARMV7_PERFCTR_PC_BRANCH_PRED,
			[C(RESULT_MISS)]
					= ARMV7_PERFCTR_PC_BRANCH_PRED,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]
					= ARMV7_PERFCTR_PC_BRANCH_PRED,
			[C(RESULT_MISS)]
					= ARMV7_PERFCTR_PC_BRANCH_PRED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
};

struct krait_evt {
	/*
	 * The group_setval field corresponds to the value that the group
	 * register needs to be set to. This value is calculated from the row
	 * and column that the event belongs to in the event table
	 */
	u32 group_setval;

	/*
	 * The groupcode corresponds to the group that the event belongs to.
	 * Krait has 3 groups of events PMRESR0, 1, 2
	 * going from 0 to 2 in terms of the codes used
	 */
	u8 groupcode;

	/*
	 * The armv7_evt_type field corresponds to the armv7 defined event
	 * code that the Krait events map to
	 */
	u32 armv7_evt_type;
};

static unsigned int get_krait_evtinfo(unsigned int krait_evt_type,
					struct krait_evt *evtinfo)
{
	u8 prefix;
	u8 reg;
	u8 code;
	u8 group;

	prefix = (krait_evt_type & 0xF0000) >> 16;
	reg    = (krait_evt_type & 0x0F000) >> 12;
	code   = (krait_evt_type & 0x00FF0) >> 4;
	group  =  krait_evt_type & 0x0000F;

	if ((prefix != KRAIT_EVT_PREFIX) || (group > 3) ||
	    (reg > KRAIT_MAX_L1_REG))
		return -EINVAL;

	evtinfo->group_setval = 0x80000000 | (code << (group * 8));
	evtinfo->groupcode = reg;
	evtinfo->armv7_evt_type = evt_type_base[reg] | group;

	return evtinfo->armv7_evt_type;
}

static u32 krait_read_pmresr0(void)
{
	u32 val;

	asm volatile("mrc p15, 1, %0, c9, c15, 0" : "=r" (val));
	return val;
}

static void krait_write_pmresr0(u32 val)
{
	asm volatile("mcr p15, 1, %0, c9, c15, 0" : : "r" (val));
}

static u32 krait_read_pmresr1(void)
{
	u32 val;

	asm volatile("mrc p15, 1, %0, c9, c15, 1" : "=r" (val));
	return val;
}

static void krait_write_pmresr1(u32 val)
{
	asm volatile("mcr p15, 1, %0, c9, c15, 1" : : "r" (val));
}

static u32 krait_read_pmresr2(void)
{
	u32 val;

	asm volatile("mrc p15, 1, %0, c9, c15, 2" : "=r" (val));
	return val;
}

static void krait_write_pmresr2(u32 val)
{
	asm volatile("mcr p15, 1, %0, c9, c15, 2" : : "r" (val));
}

struct krait_access_funcs {
	u32 (*read) (void);
	void (*write) (u32);
};

/*
 * The krait_functions array is used to set up the event register codes
 * based on the group to which an event belongs.
 * Having the following array modularizes the code for doing that.
 */
struct krait_access_funcs krait_functions[] = {
	{krait_read_pmresr0, krait_write_pmresr0},
	{krait_read_pmresr1, krait_write_pmresr1},
	{krait_read_pmresr2, krait_write_pmresr2},
};

static inline u32 krait_get_columnmask(u32 evt_code)
{
	const u32 columnmasks[] = {0xffffff00, 0xffff00ff, 0xff00ffff,
					0x80ffffff};

	return columnmasks[evt_code & 0x3];
}

static void krait_evt_setup(u32 gr, u32 setval, u32 evt_code)
{
	u32 val;

	val = krait_get_columnmask(evt_code) & krait_functions[gr].read();
	val = val | setval;
	krait_functions[gr].write(val);
}

static void krait_clear_pmuregs(void)
{
	krait_write_pmresr0(0);
	krait_write_pmresr1(0);
	krait_write_pmresr2(0);
}

static void krait_clearpmu(u32 grp, u32 val, u32 evt_code)
{
	u32 new_pmuval;

	new_pmuval = krait_functions[grp].read() &
		krait_get_columnmask(evt_code);
	krait_functions[grp].write(new_pmuval);
}

static void krait_pmu_disable_event(struct hw_perf_event *hwc, int idx)
{
	unsigned long flags;
	u32 val = 0;
	u32 gr;
	unsigned long event;
	struct krait_evt evtinfo;

	/* Disable counter and interrupt */
	raw_spin_lock_irqsave(&pmu_lock, flags);

	/* Disable counter */
	armv7_pmnc_disable_counter(idx);

	/*
	 * Clear pmresr code (if destined for PMNx counters)
	 * We don't need to set the event if it's a cycle count
	 */
	if (idx != ARMV7_CYCLE_COUNTER) {
		val = hwc->config_base;
		if (val > 0x40) {
			event = get_krait_evtinfo(val, &evtinfo);
			if (event == -EINVAL)
				goto krait_dis_out;
			val = evtinfo.group_setval;
			gr = evtinfo.groupcode;
			krait_clearpmu(gr, val, evtinfo.armv7_evt_type);
		}
	}
	/* Disable interrupt for this counter */
	armv7_pmnc_disable_intens(idx);

krait_dis_out:
	raw_spin_unlock_irqrestore(&pmu_lock, flags);
}

static void krait_pmu_enable_event(struct hw_perf_event *hwc, int idx)
{
	unsigned long flags;
	u32 val = 0;
	u32 gr;
	unsigned long event;
	struct krait_evt evtinfo;

	/*
	 * Enable counter and interrupt, and set the counter to count
	 * the event that we're interested in.
	 */
	raw_spin_lock_irqsave(&pmu_lock, flags);

	/* Disable counter */
	armv7_pmnc_disable_counter(idx);

	/*
	 * Set event (if destined for PMNx counters)
	 * We don't need to set the event if it's a cycle count
	 */
	if (idx != ARMV7_CYCLE_COUNTER) {
		val = hwc->config_base;
		if (val < 0x40) {
			armv7_pmnc_write_evtsel(idx, hwc->config_base);
		} else {
			event = get_krait_evtinfo(val, &evtinfo);

			if (event == -EINVAL)
				goto krait_out;
			/*
			 * Set event (if destined for PMNx counters)
			 * We don't need to set the event if it's a cycle count
			 */
			armv7_pmnc_write_evtsel(idx, event);
			val = 0x0;
			asm volatile("mcr p15, 0, %0, c9, c15, 0" : :
				"r" (val));
			val = evtinfo.group_setval;
			gr = evtinfo.groupcode;
			krait_evt_setup(gr, val, evtinfo.armv7_evt_type);
		}
	}

	/* Enable interrupt for this counter */
	armv7_pmnc_enable_intens(idx);

	/* Enable counter */
	armv7_pmnc_enable_counter(idx);

krait_out:
	raw_spin_unlock_irqrestore(&pmu_lock, flags);
}

static struct arm_pmu krait_pmu = {
	.handle_irq		= armv7pmu_handle_irq,
#ifdef CONFIG_SMP
	.secondary_enable       = scorpion_secondary_enable,
	.secondary_disable      = scorpion_secondary_disable,
#endif
	.enable			= krait_pmu_enable_event,
	.disable		= krait_pmu_disable_event,
	.read_counter		= armv7pmu_read_counter,
	.write_counter		= armv7pmu_write_counter,
	.raw_event_mask		= 0xFFFFF,
	.get_event_idx		= armv7pmu_get_event_idx,
	.start			= armv7pmu_start,
	.stop			= armv7pmu_stop,
	.max_period		= (1LLU << 32) - 1,
};

static const struct arm_pmu *__init armv7_krait_pmu_init(void)
{
	krait_pmu.id		= ARM_PERF_PMU_ID_KRAIT;
	krait_pmu.name	        = "ARMv7 Krait";
	krait_pmu.cache_map	= &armv7_krait_perf_cache_map;
	krait_pmu.event_map	= &armv7_krait_perf_map;
	krait_pmu.num_events	= armv7_read_num_pmnc_events();
	krait_clear_pmuregs();
	return &krait_pmu;
}

#else
static const struct arm_pmu *__init armv7_krait_pmu_init(void)
{
	return NULL;
}
#endif	/* CONFIG_CPU_V7 */
