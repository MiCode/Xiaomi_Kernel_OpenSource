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
#define C1 0x2
#define C2 0x4
#define C3 0x8
#define C_ALL (CC | C1 | C1 | C2 | C3)
#define RESET_ALL 6


#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/tracepoint.h>
#include <mach/msm-krait-l2-accessors.h>

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
			__field(u32, lctr0)
			__field(u32, lctr1)
		),

		TP_fast_assign(
			u32 cpu = smp_processor_id();
			u32 idx;
			u32 counter_reg;
			u32 val;
			u32 num_l2ctrs;
			u32 num_cores = nr_cpu_ids;
			__entry->old_pid	= prev;
			__entry->new_pid	= next;
			__entry->lctr0 = 0;
			__entry->lctr1 = 0;

			val = get_l2_indirect_reg(L2PMCR);
			num_l2ctrs = ((val >> 11) & 0x1f) + 1;
			/* Disable All*/
			asm volatile("mcr p15, 0, %0, c9, c12, 2"
					: : "r"(C_ALL));

			/* cycle counter */
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 0"
				: "=r"(__entry->cctr));

			/* ctr 0 */
			/* Select */
			asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(0));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 2"
					: "=r"(__entry->ctr0));

			/* ctr 1 */
			/* Select */
			asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(1));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 2"
					: "=r"(__entry->ctr1));

			/* ctr 2 */
			/* Select */
			asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(2));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 2"
					: "=r"(__entry->ctr2));

			/* ctr 3 */
			/* Select */
			asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(3));
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 2"
					: "=r"(__entry->ctr3));

			/* Read PMCR */
			asm volatile("mrc p15, 0, %0, c9, c12, 0"
					: "=r"(val));
			/* Reset all */
			asm volatile("mcr p15, 0, %0, c9, c12, 0"
					: : "r"(val | RESET_ALL));
			/* Enable All*/
			asm volatile("mcr p15, 0, %0, c9, c12, 1"
					: : "r"(C_ALL));

			/* L2 counters */
			/* Assign L2 counters to cores sequentially starting
			from zero. A core could have multiple L2 counters
			allocated if # L2 counters is more than the # cores */

			idx = cpu;
			/* Disable */
			set_l2_indirect_reg(L2PMCNTENCLR, 1 << idx);
			/* L2PMEVCNTR values go from 0x421, 0x431..
			So we multiply idx by 16 to get the counter reg
			value */
			counter_reg = (idx * 16) + IA_L2PMXEVCNTR_BASE;
			val = get_l2_indirect_reg(counter_reg);
			__entry->lctr0 = val;
			set_l2_indirect_reg(counter_reg, 0);
			/* Enable */
			set_l2_indirect_reg(L2PMCNTENSET, 1 << idx);

			idx = num_cores + cpu;
			if (idx < num_l2ctrs) {
				/* Disable */
				set_l2_indirect_reg(L2PMCNTENCLR, 1 << idx);
				counter_reg = (idx * 16) + IA_L2PMXEVCNTR_BASE;
				val = get_l2_indirect_reg(counter_reg);
				__entry->lctr1 = val;
				set_l2_indirect_reg(counter_reg, 0);
				/* Enable */
				set_l2_indirect_reg(L2PMCNTENSET, 1 << idx);
			}

		),

		TP_printk("prev_pid=%d, next_pid=%d, CCNTR: %u, CTR0: %u," \
				" CTR1: %u, CTR2: %u, CTR3: %u," \
				" L2CTR0,: %u, L2CTR1: %u",
				__entry->old_pid, __entry->new_pid,
				__entry->cctr, __entry->ctr0, __entry->ctr1,
				__entry->ctr2, __entry->ctr3,
				__entry->lctr0, __entry->lctr1)
);

#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../arch/arm/mach-msm
#define TRACE_INCLUDE_FILE perf_trace_counters
#include <trace/define_trace.h>
