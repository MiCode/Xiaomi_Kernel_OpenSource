/*
 * Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#include <asm/cp15.h>
#include <asm/vfp.h>
#include "../vfp/vfpinstr.h"

#ifdef CONFIG_CPU_V7
#define KRAIT_EVT_PREFIX 1
#define KRAIT_VENUMEVT_PREFIX 2
/*
   event encoding:                nrccg
   n  = prefix (1 for Krait L1) (2 for Krait VeNum events)
   r  = register
   cc = code
   g  = group
*/

#define KRAIT_L1_ICACHE_ACCESS 0x10011
#define KRAIT_L1_ICACHE_MISS 0x10010

#define KRAIT_P2_L1_ITLB_ACCESS 0x12222
#define KRAIT_P2_L1_DTLB_ACCESS 0x12210

#define KRAIT_EVENT_MASK 0xfffff
#define KRAIT_MODE_EXCL_MASK 0xc0000000

#define COUNT_MASK	0xffffffff

u32 evt_type_base[4] = {0xcc, 0xd0, 0xd4, 0xd8};

/*
 * This offset is used to calculate the index
 * into evt_type_base[] and krait_functions[]
 */
#define VENUM_BASE_OFFSET 3

#define KRAIT_MAX_L1_REG 3
#define KRAIT_MAX_VENUM_REG 0

/*
 * Every 4 bytes represents a prefix.
 * Every nibble represents a register.
 * Every bit represents a group within a register.
 *
 * This supports up to 4 groups per register, upto 8
 * registers per prefix and upto 2 prefixes.
 */
static DEFINE_PER_CPU(u64, pmu_bitmap);

static const unsigned armv7_krait_perf_map[PERF_COUNT_HW_MAX] = {
	[PERF_COUNT_HW_CPU_CYCLES]	    = ARMV7_PERFCTR_CPU_CYCLES,
	[PERF_COUNT_HW_INSTRUCTIONS]	    = ARMV7_PERFCTR_INSTR_EXECUTED,
	[PERF_COUNT_HW_CACHE_REFERENCES]    = HW_OP_UNSUPPORTED,
	[PERF_COUNT_HW_CACHE_MISSES]	    = HW_OP_UNSUPPORTED,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = ARMV7_PERFCTR_PC_WRITE,
	[PERF_COUNT_HW_BRANCH_MISSES]	    = ARMV7_PERFCTR_PC_BRANCH_MIS_PRED,
	[PERF_COUNT_HW_BUS_CYCLES]	    = ARMV7_PERFCTR_CLOCK_CYCLES,
};

static unsigned armv7_krait_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
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
			[C(RESULT_ACCESS)]	= ARMV7_PERFCTR_L1_DCACHE_ACCESS,
			[C(RESULT_MISS)]	= ARMV7_PERFCTR_L1_DCACHE_REFILL,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= ARMV7_PERFCTR_L1_DCACHE_ACCESS,
			[C(RESULT_MISS)]	= ARMV7_PERFCTR_L1_DCACHE_REFILL,
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
	[C(ITLB)] = {
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

static int krait_8960_map_event(struct perf_event *event)
{
	return armpmu_map_event(event, &armv7_krait_perf_map,
			&armv7_krait_perf_cache_map, 0xfffff);
}

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
	reg = (krait_evt_type & 0x0F000) >> 12;
	code = (krait_evt_type & 0x00FF0) >> 4;
	group = krait_evt_type & 0x0000F;

	if ((group > 3) || (reg > KRAIT_MAX_L1_REG))
		return -EINVAL;

	if (prefix != KRAIT_EVT_PREFIX && prefix != KRAIT_VENUMEVT_PREFIX)
		return -EINVAL;

	if (prefix == KRAIT_VENUMEVT_PREFIX) {
		if ((code & 0xe0) || (reg > KRAIT_MAX_VENUM_REG))
			return -EINVAL;
		else
			reg += VENUM_BASE_OFFSET;
	}

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

static u32 krait_read_vmresr0(void)
{
	u32 val;

	asm volatile ("mrc p10, 7, %0, c11, c0, 0" : "=r" (val));
	return val;
}

static void krait_write_vmresr0(u32 val)
{
	asm volatile ("mcr p10, 7, %0, c11, c0, 0" : : "r" (val));
}

static DEFINE_PER_CPU(u32, venum_orig_val);
static DEFINE_PER_CPU(u32, fp_orig_val);

static void krait_pre_vmresr0(void)
{
	u32 venum_new_val;
	u32 fp_new_val;
	u32 v_orig_val;
	u32 f_orig_val;

	/* CPACR Enable CP10 and CP11 access */
	v_orig_val = get_copro_access();
	venum_new_val = v_orig_val | CPACC_SVC(10) | CPACC_SVC(11);
	set_copro_access(venum_new_val);
	/* Store orig venum val */
	__get_cpu_var(venum_orig_val) = v_orig_val;

	/* Enable FPEXC */
	f_orig_val = fmrx(FPEXC);
	fp_new_val = f_orig_val | FPEXC_EN;
	fmxr(FPEXC, fp_new_val);
	/* Store orig fp val */
	__get_cpu_var(fp_orig_val) = f_orig_val;

}

static void krait_post_vmresr0(void)
{
	/* Restore FPEXC */
	fmxr(FPEXC, __get_cpu_var(fp_orig_val));
	isb();
	/* Restore CPACR */
	set_copro_access(__get_cpu_var(venum_orig_val));
}

struct krait_access_funcs {
	u32 (*read) (void);
	void (*write) (u32);
	void (*pre) (void);
	void (*post) (void);
};

/*
 * The krait_functions array is used to set up the event register codes
 * based on the group to which an event belongs.
 * Having the following array modularizes the code for doing that.
 */
struct krait_access_funcs krait_functions[] = {
	{krait_read_pmresr0, krait_write_pmresr0, NULL, NULL},
	{krait_read_pmresr1, krait_write_pmresr1, NULL, NULL},
	{krait_read_pmresr2, krait_write_pmresr2, NULL, NULL},
	{krait_read_vmresr0, krait_write_vmresr0, krait_pre_vmresr0,
	 krait_post_vmresr0},
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

	if (krait_functions[gr].pre)
		krait_functions[gr].pre();

	val = krait_get_columnmask(evt_code) & krait_functions[gr].read();
	val = val | setval;
	krait_functions[gr].write(val);

	if (krait_functions[gr].post)
		krait_functions[gr].post();
}

static void krait_clear_pmuregs(void)
{
	krait_write_pmresr0(0);
	krait_write_pmresr1(0);
	krait_write_pmresr2(0);

	krait_pre_vmresr0();
	krait_write_vmresr0(0);
	krait_post_vmresr0();
}

static void krait_clearpmu(u32 grp, u32 val, u32 evt_code)
{
	u32 new_pmuval;

	if (krait_functions[grp].pre)
		krait_functions[grp].pre();

	new_pmuval = krait_functions[grp].read() &
		krait_get_columnmask(evt_code);
	krait_functions[grp].write(new_pmuval);

	if (krait_functions[grp].post)
		krait_functions[grp].post();
}

static void krait_pmu_disable_event(struct perf_event *event)
{
	unsigned long flags;
	u32 val = 0;
	u32 gr;
	unsigned long ev_num;
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	struct krait_evt evtinfo;
	struct pmu_hw_events *events = cpu_pmu->get_hw_events();


	/* Disable counter and interrupt */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/* Disable counter */
	armv7_pmnc_disable_counter(idx);

	/*
	 * Clear pmresr code (if destined for PMNx counters)
	 * We don't need to set the event if it's a cycle count
	 */
	if (idx != ARMV7_IDX_CYCLE_COUNTER) {
		val = hwc->config_base;
		val &= KRAIT_EVENT_MASK;

		if (val > 0x40) {
			ev_num = get_krait_evtinfo(val, &evtinfo);
			if (ev_num == -EINVAL)
				goto krait_dis_out;
			val = evtinfo.group_setval;
			gr = evtinfo.groupcode;
			krait_clearpmu(gr, val, evtinfo.armv7_evt_type);
		}
	}
	/* Disable interrupt for this counter */
	armv7_pmnc_disable_intens(idx);

krait_dis_out:
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void krait_pmu_enable_event(struct perf_event *event)
{
	unsigned long flags;
	u32 val = 0;
	u32 gr;
	unsigned long ev_num;
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	struct krait_evt evtinfo;
	unsigned long long prev_count = local64_read(&hwc->prev_count);
	struct pmu_hw_events *events = cpu_pmu->get_hw_events();

	/*
	 * Enable counter and interrupt, and set the counter to count
	 * the event that we're interested in.
	 */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/* Disable counter */
	armv7_pmnc_disable_counter(idx);

	val = hwc->config_base;
	val &= KRAIT_EVENT_MASK;

	/* set event for ARM-architected events, and filter for CC */
	if ((val < 0x40) || (idx == ARMV7_IDX_CYCLE_COUNTER)) {
		armv7_pmnc_write_evtsel(idx, hwc->config_base);
	} else {
		ev_num = get_krait_evtinfo(val, &evtinfo);

		if (ev_num == -EINVAL)
			goto krait_out;

		/* Restore Mode-exclusion bits */
		ev_num |= (hwc->config_base & KRAIT_MODE_EXCL_MASK);

		/* Set event (if destined for PMNx counters) */
		armv7_pmnc_write_evtsel(idx, ev_num);
		val = 0x0;
		asm volatile("mcr p15, 0, %0, c9, c15, 0" : :
			     "r" (val));
		val = evtinfo.group_setval;
		gr = evtinfo.groupcode;
		krait_evt_setup(gr, val, evtinfo.armv7_evt_type);
	}

	/* Enable interrupt for this counter */
	armv7_pmnc_enable_intens(idx);

	/* Restore prev val */
	armv7pmu_write_counter(event, prev_count & COUNT_MASK);

	/* Enable counter */
	armv7_pmnc_enable_counter(idx);

krait_out:
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

#ifdef CONFIG_PERF_EVENTS_USERMODE
static void krait_init_usermode(void)
{
	u32 val;

	/* Set PMACTLR[UEN] */
	asm volatile("mrc p15, 0, %0, c9, c15, 5" : "=r" (val));
	val |= 1;
	asm volatile("mcr p15, 0, %0, c9, c15, 5" : : "r" (val));
	/* Set PMUSERENR[UEN] */
	asm volatile("mrc p15, 0, %0, c9, c14, 0" : "=r" (val));
	val |= 1;
	asm volatile("mcr p15, 0, %0, c9, c14, 0" : : "r" (val));
}
#else
static inline void krait_init_usermode(void)
{
}
#endif

static void krait_pmu_reset(void *info)
{
	u32 idx, nb_cnt = cpu_pmu->num_events;

	/* Stop all counters and their interrupts */
	for (idx = ARMV7_IDX_CYCLE_COUNTER; idx < nb_cnt; ++idx) {
		armv7_pmnc_disable_counter(idx);
		armv7_pmnc_disable_intens(idx);
	}

	/* Clear all pmresrs */
	krait_clear_pmuregs();

	krait_init_usermode();

	/* Reset irq stat reg */
	armv7_pmnc_getreset_flags();

	/* Reset all ctrs to 0 */
	armv7_pmnc_write(ARMV7_PMNC_P | ARMV7_PMNC_C);
}

#ifdef CONFIG_PERF_EVENTS_RESET_PMU_DEBUGFS
static void (*l2_reset_pmu)(void);
void msm_perf_register_l2_reset_callback(void (*reset_l2_pmu))
{
	if (reset_l2_pmu != NULL)
		l2_reset_pmu = reset_l2_pmu;
}

static void krait_force_pmu_reset(void *info)
{
	/* krait specific reset */
	krait_pmu_reset(info);
	/* Reset column exclusion mask */
	__get_cpu_var(pmu_bitmap) = 0;
	/* Clear L2 PMU */
	if (msm_perf_clear_l2_pmu && l2_reset_pmu) {
		l2_reset_pmu();
		msm_perf_clear_l2_pmu = 0;
	}
}
#else
static inline void krait_force_pmu_reset(void *info)
{
}

inline void msm_perf_register_l2_reset_callback(void (*reset_l2_pmu))
{
}
#endif

/*
 * We check for column exclusion constraints here.
 * Two events cant have same reg and same group.
 */
static int msm_test_set_ev_constraint(struct perf_event *event)
{
	u32 evt_type = event->attr.config & KRAIT_EVENT_MASK;
	u8 prefix = (evt_type & 0xF0000) >> 16;
	u8 reg = (evt_type & 0x0F000) >> 12;
	u8 group = evt_type & 0x0000F;
	u64 cpu_pmu_bitmap = __get_cpu_var(pmu_bitmap);
	u64 bitmap_t;

	/* Return if non MSM event. */
	if (!prefix)
		return 0;

	bitmap_t = 1 << (((prefix - 1) * 32) + (reg * 4) + group);

	/* Set it if not already set. */
	if (!(cpu_pmu_bitmap & bitmap_t)) {
		cpu_pmu_bitmap |= bitmap_t;
		__get_cpu_var(pmu_bitmap) = cpu_pmu_bitmap;
		return 1;
	}
	/* Bit is already set. Constraint failed. */
	return -EPERM;
}

static int msm_clear_ev_constraint(struct perf_event *event)
{
	u32 evt_type = event->attr.config & KRAIT_EVENT_MASK;
	u8 prefix = (evt_type & 0xF0000) >> 16;
	u8 reg = (evt_type & 0x0F000) >> 12;
	u8 group = evt_type & 0x0000F;
	u64 cpu_pmu_bitmap = __get_cpu_var(pmu_bitmap);
	u64 bitmap_t;

	/* Return if non MSM event. */
	if (!prefix)
		return 0;

	bitmap_t = 1 << (((prefix - 1) * 32) + (reg * 4) + group);

	/* Clear constraint bit. */
	cpu_pmu_bitmap &= ~(bitmap_t);

	__get_cpu_var(pmu_bitmap) = cpu_pmu_bitmap;

	return 1;
}

static DEFINE_PER_CPU(u32, krait_pm_pmactlr);

static void krait_save_pm_registers(void *hcpu)
{
	u32 val;
	u32 cpu = (int)hcpu;

	/* Read PMACTLR */
	asm volatile("mrc p15, 0, %0, c9, c15, 5" : "=r" (val));
	per_cpu(krait_pm_pmactlr, cpu) = val;

	armv7pmu_save_pm_registers(hcpu);
}

static void krait_restore_pm_registers(void *hcpu)
{
	u32 val;
	u32 cpu = (int)hcpu;

	val = per_cpu(krait_pm_pmactlr, cpu);
	if (val != 0)
		/* Restore PMACTLR */
		asm volatile("mcr p15, 0, %0, c9, c15, 5" : : "r" (val));

	armv7pmu_restore_pm_registers(hcpu);
}

/* NRCCG format for perf RAW codes. */
PMU_FORMAT_ATTR(prefix,	"config:16-19");
PMU_FORMAT_ATTR(reg,	"config:12-15");
PMU_FORMAT_ATTR(code,	"config:4-11");
PMU_FORMAT_ATTR(grp,	"config:0-3");

static struct attribute *msm_l1_ev_formats[] = {
	&format_attr_prefix.attr,
	&format_attr_reg.attr,
	&format_attr_code.attr,
	&format_attr_grp.attr,
	NULL,
};

/*
 * Format group is essential to access PMU's from userspace
 * via their .name field.
 */
static struct attribute_group msm_pmu_format_group = {
	.name = "format",
	.attrs = msm_l1_ev_formats,
};

static const struct attribute_group *msm_l1_pmu_attr_grps[] = {
	&msm_pmu_format_group,
	NULL,
};

static int armv7_krait_pmu_init(struct arm_pmu *cpu_pmu)
{
	cpu_pmu->handle_irq		= armv7pmu_handle_irq;
	cpu_pmu->enable			= krait_pmu_enable_event;
	cpu_pmu->disable		= krait_pmu_disable_event;
	cpu_pmu->read_counter		= armv7pmu_read_counter;
	cpu_pmu->write_counter		= armv7pmu_write_counter;
	cpu_pmu->get_event_idx		= armv7pmu_get_event_idx;
	cpu_pmu->start			= armv7pmu_start;
	cpu_pmu->stop			= armv7pmu_stop;
	cpu_pmu->reset			= krait_pmu_reset;
	cpu_pmu->force_reset			= krait_force_pmu_reset;
	cpu_pmu->test_set_event_constraints	= msm_test_set_ev_constraint;
	cpu_pmu->clear_event_constraints	= msm_clear_ev_constraint;
	cpu_pmu->save_pm_registers	= krait_save_pm_registers;
	cpu_pmu->restore_pm_registers	= krait_restore_pm_registers;
	cpu_pmu->max_period		= (1LLU << 32) - 1;
	cpu_pmu->name			= "cpu";
	cpu_pmu->map_event		= krait_8960_map_event;
	cpu_pmu->num_events		= armv7_read_num_pmnc_events();
	cpu_pmu->pmu.attr_groups	= msm_l1_pmu_attr_grps;
	krait_clear_pmuregs();

	cpu_pmu->set_event_filter = armv7pmu_set_event_filter,

	armv7_krait_perf_cache_map[C(ITLB)]
		[C(OP_READ)]
		[C(RESULT_ACCESS)] = KRAIT_P2_L1_ITLB_ACCESS;
	armv7_krait_perf_cache_map[C(ITLB)]
		[C(OP_WRITE)]
		[C(RESULT_ACCESS)] = KRAIT_P2_L1_ITLB_ACCESS;
	armv7_krait_perf_cache_map[C(DTLB)]
		[C(OP_READ)]
		[C(RESULT_ACCESS)] = KRAIT_P2_L1_DTLB_ACCESS;
	armv7_krait_perf_cache_map[C(DTLB)]
		[C(OP_WRITE)]
		[C(RESULT_ACCESS)] = KRAIT_P2_L1_DTLB_ACCESS;

	return 0;
}

#else
static inline int armv7_krait_pmu_init(struct arm_pmu *cpu_pmu)
{
	return -ENODEV;
}
#endif	/* CONFIG_CPU_V7 */
