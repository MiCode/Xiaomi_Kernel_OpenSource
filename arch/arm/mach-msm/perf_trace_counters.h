/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#undef TRACE_SYSTEM
#define TRACE_SYSTEM perf_trace_counters

#if !defined(_PERF_TRACE_COUNTERS_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _PERF_TRACE_COUNTERS_H_

/* Ctr index for PMCNTENSET/CLR */
#define CC 0x80000000
#define C0 0x1
#define C1 0x10
#define C2 0x100
#define C3 0x1000


#include <linux/sched.h>
#include <linux/tracepoint.h>

TRACE_EVENT(sched_switch_with_ctrs,

		TP_PROTO(pid_t prev, pid_t next),

		TP_ARGS(prev, next),

		TP_STRUCT__entry(
			__field(pid_t,	old_pid)
			__field(pid_t,	new_pid)
			__field(u32, cctr)
			__field(u32, ctr0)
			__field(u32, ctr1)
			__field(u32, ctr2)
			__field(u32, ctr3)
		),

		TP_fast_assign(
			__entry->old_pid	= prev;
			__entry->new_pid	= next;

			/* cycle counter */
			/* Disable */
			asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(CC));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 0"
				: "=r"(__entry->cctr));
			/* Reset */
			asm volatile("mcr p15, 0, %0, c9, c13, 0" : : "r"(0));
			/* Enable */
			asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(CC));

			/* ctr 0 */
			/* Disable */
			asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(C0));
			/* Select */
			asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(0));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 2"
					: "=r"(__entry->ctr0));
			/* Reset */
			asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r"(0));
			/* Enable */
			asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(C0));

			/* ctr 1 */
			/* Disable */
			asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(C1));
			/* Select */
			asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(1));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 2"
					: "=r"(__entry->ctr1));
			/* Reset */
			asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r"(0));
			/* Enable */
			asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(C1));

			/* ctr 2 */
			/* Disable */
			asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(C2));
			/* Select */
			asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(2));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 2"
					: "=r"(__entry->ctr2));
			/* Reset */
			asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r"(0));
			/* Enable */
			asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(C2));

			/* ctr 3 */
			/* Disable */
			asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(C3));
			/* Select */
			asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(3));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 2"
					: "=r"(__entry->ctr3));
			/* Reset */
			asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r"(0));
			/* Enable */
			asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(C3));

		),

		TP_printk("prev_pid=%d, next_pid=%d, CCNTR: %u, CTR0: %u," \
				" CTR1: %u, CTR2: %u, CTR3: %u",
				__entry->old_pid, __entry->new_pid,
				__entry->cctr, __entry->ctr0, __entry->ctr1,
				__entry->ctr2, __entry->ctr3)
);

#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../arch/arm/mach-msm
#define TRACE_INCLUDE_FILE perf_trace_counters
#include <trace/define_trace.h>
