/*
 * ARM DynamIQ Shared Unit (DSU) PMU Low level register access routines.
 *
 * Copyright (C) ARM Limited, 2017.
 *
 * Author: Suzuki K Poulose <suzuki.poulose@arm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#include <asm/cp15.h>


#define CLUSTERPMCR			__ACCESS_CP15(c15, 0, c5, 0)
#define CLUSTERPMCNTENSET		__ACCESS_CP15(c15, 0, c5, 1)
#define CLUSTERPMCNTENCLR		__ACCESS_CP15(c15, 0, c5, 2)
#define CLUSTERPMOVSSET			__ACCESS_CP15(c15, 0, c5, 3)
#define CLUSTERPMOVSCLR			__ACCESS_CP15(c15, 0, c5, 4)
#define CLUSTERPMSELR			__ACCESS_CP15(c15, 0, c5, 5)
#define CLUSTERPMINTENSET		__ACCESS_CP15(c15, 0, c5, 6)
#define CLUSTERPMINTENCLR		__ACCESS_CP15(c15, 0, c5, 7)
#define CLUSTERPMCCNTR			__ACCESS_CP15(c15, 0, c6, 0)
#define CLUSTERPMXEVTYPER		__ACCESS_CP15(c15, 0, c6, 1)
#define CLUSTERPMXEVCNTR		__ACCESS_CP15(c15, 0, c6, 2)
#define CLUSTERPMMDCR			__ACCESS_CP15(c15, 0, c6, 3)
#define CLUSTERPMCEID0			__ACCESS_CP15(c15, 0, c6, 4)
#define CLUSTERPMCEID1			__ACCESS_CP15(c15, 0, c6, 5)

static inline u32 __dsu_pmu_read_pmcr(void)
{
	return read_sysreg(CLUSTERPMCR);
}

static inline void __dsu_pmu_write_pmcr(u32 val)
{
	write_sysreg(val, CLUSTERPMCR);
	isb();
}

static inline u32 __dsu_pmu_get_pmovsclr(void)
{
	u32 val = read_sysreg(CLUSTERPMOVSCLR);
	/* Clear the bit */
	write_sysreg(val, CLUSTERPMOVSCLR);
	isb();
	return val;
}

static inline void __dsu_pmu_select_counter(int counter)
{
	write_sysreg(counter, CLUSTERPMSELR);
	isb();
}

static inline u64 __dsu_pmu_read_counter(int counter)
{
	__dsu_pmu_select_counter(counter);
	return read_sysreg(CLUSTERPMXEVCNTR);
}

static inline void __dsu_pmu_write_counter(int counter, u64 val)
{
	__dsu_pmu_select_counter(counter);
	write_sysreg(val, CLUSTERPMXEVCNTR);
	isb();
}

static inline void __dsu_pmu_set_event(int counter, u32 event)
{
	__dsu_pmu_select_counter(counter);
	write_sysreg(event, CLUSTERPMXEVTYPER);
	isb();
}

static inline u64 __dsu_pmu_read_pmccntr(void)
{
	return read_sysreg(CLUSTERPMCCNTR);
}

static inline void __dsu_pmu_write_pmccntr(u64 val)
{
	write_sysreg(val, CLUSTERPMCCNTR);
	isb();
}

static inline void __dsu_pmu_disable_counter(int counter)
{
	write_sysreg(BIT(counter), CLUSTERPMCNTENCLR);
	isb();
}

static inline void __dsu_pmu_enable_counter(int counter)
{
	write_sysreg(BIT(counter), CLUSTERPMCNTENSET);
	isb();
}

static inline void __dsu_pmu_counter_interrupt_enable(int counter)
{
	write_sysreg(BIT(counter), CLUSTERPMINTENSET);
	isb();
}

static inline void __dsu_pmu_counter_interrupt_disable(int counter)
{
	write_sysreg(BIT(counter), CLUSTERPMINTENCLR);
	isb();
}


static inline u32 __dsu_pmu_read_pmceid(int n)
{
	switch (n) {
	case 0:
		return read_sysreg(CLUSTERPMCEID0);
	case 1:
		return read_sysreg(CLUSTERPMCEID1);
	default:
		BUILD_BUG();
		return 0;
	}
}
