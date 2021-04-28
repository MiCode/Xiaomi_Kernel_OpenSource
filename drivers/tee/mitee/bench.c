/*
 * Copyright (c) 2017, Linaro Limited
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/smp.h>
#include "optee_bench.h"

/*
 * Specific defines for ARM performance timers
 */
/* aarch32 */
#define OPTEE_BENCH_DEF_OPTS			(1 | 16)
#define OPTEE_BENCH_DEF_OVER			0x8000000f
/* enable 64 divider for CCNT */
#define OPTEE_BENCH_DIVIDER_OPTS		(OPTEE_BENCH_DEF_OPTS | 8)

/* aarch64 */
#define OPTEE_BENCH_ARMV8_PMCR_MASK	0x3f
#define OPTEE_BENCH_ARMV8_PMCR_E	(1 << 0) /* Enable all counters */
#define OPTEE_BENCH_ARMV8_PMCR_P	(1 << 1) /* Reset all counters */
#define OPTEE_BENCH_ARMV8_PMCR_C	(1 << 2) /* Cycle counter reset */
#define OPTEE_BENCH_ARMV8_PMCR_D    (1 << 3) /* 64 divider */

#define OPTEE_BENCH_ARMV8_PMUSERENR_EL0	(1 << 0) /* EL0 access enable */
#define OPTEE_BENCH_ARMV8_PMUSERENR_CR	(1 << 2) /* CCNT read enable */

struct optee_ts_global *optee_bench_ts_global;
struct rw_semaphore optee_bench_ts_rwsem;

#ifdef CONFIG_OPTEE_BENCHMARK
static inline u32 armv8pmu_pmcr_read(void)
{
		u32 val = 0;

		asm volatile("mrs %0, pmcr_el0" : "=r"(val));

		return (u32)val;
}

static inline void armv8pmu_pmcr_write(u32 val)
{
		val &= OPTEE_BENCH_ARMV8_PMCR_MASK;
		asm volatile("msr pmcr_el0, %0" :: "r"((u64)val));
}

static inline u64 read_ccounter(void)
{
	u64 ccounter;

#ifdef __aarch64__
	asm volatile("mrs %0, PMCCNTR_EL0" : "=r"(ccounter));
#else
	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(ccounter));
#endif

	return ccounter * OPTEE_BENCH_DIVIDER;
}

static void optee_pmu_setup(void *data)
{
#ifdef __aarch64__
	/* Enable EL0 access to PMU counters. */
	asm volatile("msr pmuserenr_el0, %0" :: "r"((u64)
			OPTEE_BENCH_ARMV8_PMUSERENR_EL0 |
			OPTEE_BENCH_ARMV8_PMUSERENR_CR));
	/* Enable PMU counters */
	armv8pmu_pmcr_write(OPTEE_BENCH_ARMV8_PMCR_P |
					OPTEE_BENCH_ARMV8_PMCR_C |
					OPTEE_BENCH_ARMV8_PMCR_D);
	asm volatile("msr pmcntenset_el0, %0" :: "r"((u64)(1 << 31)));
	armv8pmu_pmcr_write(armv8pmu_pmcr_read() |
					OPTEE_BENCH_ARMV8_PMCR_E);
#else
	/* Enable EL0 access to PMU counters */
	asm volatile("mcr p15, 0, %0, c9, c14, 0" :: "r"(1));
	/* Enable all PMU counters */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"
					(OPTEE_BENCH_DIVIDER_OPTS));
	/* Disable counter overflow interrupts */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(OPTEE_BENCH_DEF_OVER));
#endif
}

static void optee_pmu_disable(void *data)
{
#ifdef __aarch64__
	/* Disable EL0 access */
	asm volatile("msr pmuserenr_el0, %0" :: "r"((u64)0));
	/* Disable PMU counters */
	armv8pmu_pmcr_write(armv8pmu_pmcr_read() | ~OPTEE_BENCH_ARMV8_PMCR_E);
#else
	/* Disable all PMU counters */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(0));
	/* Enable counter overflow interrupts */
	asm volatile("mcr p15, 0, %0, c9, c12, 2" :: "r"(OPTEE_BENCH_DEF_OVER));
	/* Disable EL0 access to PMU counters. */
	asm volatile("mcr p15, 0, %0, c9, c14, 0" :: "r"(0));
#endif
}

void optee_bm_enable(void)
{
	on_each_cpu(optee_pmu_setup, NULL, 1);
}

void optee_bm_disable(void)
{
	on_each_cpu(optee_pmu_disable, NULL, 1);
}

void optee_bm_timestamp(void)
{
	struct optee_ts_cpu_buf *cpu_buf;
	struct optee_time_st ts_data;
	uint64_t ts_i;
	void *ret_addr;
	int cur_cpu = 0;
	int ret;

	down_read(&optee_bench_ts_rwsem);

	if (!optee_bench_ts_global) {
		up_read(&optee_bench_ts_rwsem);
		return;
	}

	cur_cpu = get_cpu();

	if (cur_cpu >= optee_bench_ts_global->cores) {
		put_cpu();
		up_read(&optee_bench_ts_rwsem);
		return;
	}

	ret_addr = __builtin_return_address(0);

	cpu_buf = &optee_bench_ts_global->cpu_buf[cur_cpu];
	ts_i = __sync_fetch_and_add(&cpu_buf->head, 1);
	ts_data.cnt = read_ccounter();
	ts_data.addr = (uintptr_t)ret_addr;
	ts_data.src = OPTEE_BENCH_KMOD;
	cpu_buf->stamps[ts_i & OPTEE_BENCH_MAX_MASK] = ts_data;

	up_read(&optee_bench_ts_rwsem);

	put_cpu();
}
#endif  /* CONFIG_OPTEE_BENCHMARK */
