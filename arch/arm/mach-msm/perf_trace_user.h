/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#if !defined(_PERF_TRACE_USER_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _PERF_TRACE_USER_H_

#include <linux/cpumask.h>
#include <linux/tracepoint.h>
#include <mach/msm-krait-l2-accessors.h>

#define CNTENSET_CC    0x80000000
#define NUM_L1_CTRS             4
#define NUM_L2_PERCPU           2

extern u32 perf_num_l2ctrs;

TRACE_EVENT(perf_trace_user,
	TP_PROTO(char *string, u32 cnten_val, unsigned long l2_enmask),
	TP_ARGS(string, cnten_val, l2_enmask),

	TP_STRUCT__entry(
		__field(u32, cctr)
		__field(u32, ctr0)
		__field(u32, ctr1)
		__field(u32, ctr2)
		__field(u32, ctr3)
		__field(u32, lctr0)
		__field(u32, lctr1)
		__string(user_string, string)
		),

	TP_fast_assign(
		u32 cnt;
		u32 l1_cnts[NUM_L1_CTRS];
		u32 l2_cnts[NUM_L2_PERCPU];
		unsigned long idx;
		int i;

		if (cnten_val & CNTENSET_CC) {
			/* Read value */
			asm volatile("mrc p15, 0, %0, c9, c13, 0"
				     : "=r"(cnt));
			__entry->cctr = cnt;
		} else
			__entry->cctr = 0;
		for (i = 0; i < NUM_L1_CTRS; i++) {
			if (cnten_val & (1 << i)) {
				/* Select */
				asm volatile("mcr p15, 0, %0, c9, c12, 5"
					     : : "r"(i));
				/* Read value */
				asm volatile("mrc p15, 0, %0, c9, c13, 2"
					     : "=r"(cnt));
				l1_cnts[i] = cnt;
			} else {
				l1_cnts[i] = 0;
			}
		}

		/* L2 counters */
		i = 0;
		l2_cnts[0] = 0;
		l2_cnts[1] = 0;
		for_each_set_bit(idx, &l2_enmask, perf_num_l2ctrs) {
			/*
			 * L2PMXEVCNTR address offsets increment by 16,
			 * so multiply idx by 16 to get the next reg
			 */
			l2_cnts[i++] = get_l2_indirect_reg(
				(idx * 16) + IA_L2PMXEVCNTR_BASE);
		}
		__entry->ctr0 = l1_cnts[0];
		__entry->ctr1 = l1_cnts[1];
		__entry->ctr2 = l1_cnts[2];
		__entry->ctr3 = l1_cnts[3];
		__entry->lctr0 = l2_cnts[0];
		__entry->lctr1 = l2_cnts[1];
		__assign_str(user_string, string);
		),

	    TP_printk("CCNTR: %u, CTR0: %u, CTR1: %u, CTR2: %u, CTR3: %u, L2CTR0: %u, L2CTR1: %u, MSG=%s",
		      __entry->cctr, __entry->ctr0, __entry->ctr1,
		      __entry->ctr2, __entry->ctr3,
		      __entry->lctr0, __entry->lctr1,
		      __get_str(user_string)
		    )
	);

#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../arch/arm/mach-msm
#define TRACE_INCLUDE_FILE perf_trace_user
#include <trace/define_trace.h>
