/* Copyright (c) 2014,2017 The Linux Foundation. All rights reserved.
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
#if !defined(_PERF_TRACE_USER_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _PERF_TRACE_USER_H_

#undef TRACE_SYSTEM
#define TRACE_SYSTEM perf_trace_counters

#include <linux/tracepoint.h>

#define CNTENSET_CC    0x80000000
#define NUM_L1_CTRS             6

TRACE_EVENT(perf_trace_user,
	TP_PROTO(char *string, u32 cnten_val),
	TP_ARGS(string, cnten_val),

	TP_STRUCT__entry(
		__field(u32, cctr)
		__field(u32, ctr0)
		__field(u32, ctr1)
		__field(u32, ctr2)
		__field(u32, ctr3)
		__field(u32, ctr4)
		__field(u32, ctr5)
		__string(user_string, string)
		),

	TP_fast_assign(
		u32 cnt;
		u32 l1_cnts[NUM_L1_CTRS];
		int i;

		if (cnten_val & CNTENSET_CC) {
			/* Read value */
			cnt = read_sysreg(pmccntr_el0);
			__entry->cctr = cnt;
		} else
			__entry->cctr = 0;
		for (i = 0; i < NUM_L1_CTRS; i++) {
			if (cnten_val & (1 << i)) {
				/* Select */
				write_sysreg(i, pmselr_el0);
				isb();
				/* Read value */
				cnt = read_sysreg(pmxevcntr_el0);
				l1_cnts[i] = cnt;
			} else {
				l1_cnts[i] = 0;
			}
		}

		__entry->ctr0 = l1_cnts[0];
		__entry->ctr1 = l1_cnts[1];
		__entry->ctr2 = l1_cnts[2];
		__entry->ctr3 = l1_cnts[3];
		__entry->ctr4 = l1_cnts[4];
		__entry->ctr5 = l1_cnts[5];
		__assign_str(user_string, string);
		),

		TP_printk("CCNTR: %u, CTR0: %u, CTR1: %u, CTR2: %u, CTR3: %u, CTR4: %u, CTR5: %u, MSG=%s",
				__entry->cctr,
				__entry->ctr0, __entry->ctr1,
				__entry->ctr2, __entry->ctr3,
				__entry->ctr4, __entry->ctr5,
				__get_str(user_string)
			)
	);

#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../arch/arm64/kernel
#define TRACE_INCLUDE_FILE perf_trace_user
#include <trace/define_trace.h>
