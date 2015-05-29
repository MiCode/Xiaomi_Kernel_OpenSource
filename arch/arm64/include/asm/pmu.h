/*
 * Based on arch/arm/include/asm/pmu.h
 *
 * Copyright (C) 2009 picoChip Designs Ltd, Jamie Iles
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_PMU_H
#define __ASM_PMU_H

#ifdef CONFIG_HW_PERF_EVENTS

enum arm_pmu_state {
	ARM_PMU_STATE_OFF       = 0,
	ARM_PMU_STATE_GOING_DOWN,
	ARM_PMU_STATE_RUNNING,
};

/* The events for a given PMU register set. */
struct pmu_hw_events {
	/*
	 * The events that are active on the PMU for the given index.
	 */
	struct perf_event	**events;

	/*
	 * A 1 bit for an index indicates that the counter is being used for
	 * an event. A 0 means that the counter can be used.
	 */
	unsigned long           *used_mask;

	u32			*from_idle;

	/*
	 * Hardware lock to serialize accesses to PMU registers. Needed for the
	 * read/modify/write sequences.
	 */
	raw_spinlock_t		pmu_lock;
};

struct arm_pmu {
	struct pmu		pmu;
	cpumask_t		active_irqs;
	const char		*name;
	irqreturn_t		(*handle_irq)(int irq_num, void *dev);
	void			(*enable)(struct hw_perf_event *evt, int idx);
	void			(*disable)(struct hw_perf_event *evt, int idx);
	int			(*get_event_idx)(struct pmu_hw_events *hw_events,
						 struct hw_perf_event *hwc);
	int			(*set_event_filter)(struct hw_perf_event *evt,
						    struct perf_event_attr *attr);
	u32			(*read_counter)(int idx);
	void			(*write_counter)(int idx, u32 val);
	void			(*start)(void);
	void			(*stop)(void);
	void			(*reset)(void *);
	int			(*request_irq)(struct arm_pmu *,
					       irq_handler_t handler);
	void			(*free_irq)(struct arm_pmu *);
	int			(*map_event)(struct perf_event *event);
	int			num_events;
	int			pmu_state;
	int			percpu_irq;
	atomic_t		active_events;
	struct mutex		reserve_mutex;
	u64			max_period;
	struct platform_device	*plat_device;
	struct pmu_hw_events	*(*get_hw_events)(void);
	void			(*save_pm_registers)(void *hcpu);
	void			(*restore_pm_registers)(void *hcpu);
	int			(*check_event)(
					 struct arm_pmu *armpmu,
					 struct hw_perf_event *hwc);
};

#define to_arm_pmu(p) (container_of(p, struct arm_pmu, pmu))

extern const unsigned armv8_pmuv3_perf_map[PERF_COUNT_HW_MAX];
extern const unsigned armv8_pmuv3_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					     [PERF_COUNT_HW_CACHE_OP_MAX]
					     [PERF_COUNT_HW_CACHE_RESULT_MAX];
int map_cpu_event(struct perf_event *event,
		  const unsigned (*event_map)[PERF_COUNT_HW_MAX],
		  const unsigned (*cache_map)[PERF_COUNT_HW_CACHE_MAX]
					     [PERF_COUNT_HW_CACHE_OP_MAX]
					     [PERF_COUNT_HW_CACHE_RESULT_MAX],
		  u32 raw_event_mask);

int __init armpmu_register(struct arm_pmu *armpmu, char *name, int type);

u64 armpmu_event_update(struct perf_event *event,
			struct hw_perf_event *hwc,
			int idx);

int armpmu_event_set_period(struct perf_event *event,
			    struct hw_perf_event *hwc,
			    int idx);

int armv8pmu_enable_intens(int idx);
int armv8pmu_disable_intens(int idx);
int armv8pmu_enable_counter(int idx);
int armv8pmu_disable_counter(int idx);
u32 armv8pmu_getreset_flags(void);
void armv8pmu_pmcr_write(u32 val);
void armv8pmu_write_evtype(int idx, u32 val);

int kryo_pmu_init(struct arm_pmu *cpu_pmu);

#endif /* CONFIG_HW_PERF_EVENTS */
#endif /* __ASM_PMU_H */
