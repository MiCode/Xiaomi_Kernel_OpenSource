/*
 * Copyright (c) 2016, Linaro Limited
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

#ifndef _OPTEE_BENCH_H
#define _OPTEE_BENCH_H

#include <linux/rwsem.h>

/*
 * Cycle count divider is enabled (in PMCR),
 * CCNT value is incremented every 64th clock cycle
 */
#define OPTEE_BENCH_DIVIDER			64

/* max amount of timestamps */
#define OPTEE_BENCH_MAX_STAMPS		32
#define OPTEE_BENCH_MAX_MASK		(OPTEE_BENCH_MAX_STAMPS - 1)

/* OP-TEE susbsystems ids */
#define OPTEE_BENCH_KMOD	0x20000000

#define OPTEE_MSG_RPC_CMD_BENCH_REG_NEW		0
#define OPTEE_MSG_RPC_CMD_BENCH_REG_DEL		1

/* storing timestamp */
struct optee_time_st {
	uint64_t cnt;	/* stores value from CNTPCT register */
	uint64_t addr;	/* stores value from program counter register */
	uint64_t src;	/* OP-TEE subsystem id */
};

/* per-cpu circular buffer for timestamps */
struct optee_ts_cpu_buf {
	uint64_t head;
	uint64_t tail;
	struct optee_time_st stamps[OPTEE_BENCH_MAX_STAMPS];
};

/* memory layout for shared memory, where timestamps will be stored */
struct optee_ts_global {
	uint64_t cores;
	struct optee_ts_cpu_buf cpu_buf[];
};

extern struct optee_ts_global *optee_bench_ts_global;
extern struct rw_semaphore optee_bench_ts_rwsem;

#ifdef CONFIG_OPTEE_BENCHMARK
void optee_bm_enable(void);
void optee_bm_disable(void);
void optee_bm_timestamp(void);
#else
static inline void optee_bm_enable(void) {}
static inline void optee_bm_disable(void) {}
static inline void optee_bm_timestamp(void) {}
#endif  /* CONFIG_OPTEE_BENCHMARK */
#endif /* _OPTEE_BENCH_H */
