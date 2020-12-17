/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#if !defined(_TRACE_DCVS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DCVS_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM dcvs

#include <linux/tracepoint.h>
#include <soc/qcom/dcvs.h>

TRACE_EVENT(memlat_dev_meas,

	TP_PROTO(const char *name, unsigned int dev_id, unsigned long inst,
		 unsigned long mem, unsigned long freq, unsigned int stall,
		 unsigned int wb, unsigned int ratio),

	TP_ARGS(name, dev_id, inst, mem, freq, stall, wb, ratio),

	TP_STRUCT__entry(
		__string(name, name)
		__field(unsigned int, dev_id)
		__field(unsigned long, inst)
		__field(unsigned long, mem)
		__field(unsigned long, freq)
		__field(unsigned int, stall)
		__field(unsigned int, wb)
		__field(unsigned int, ratio)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->dev_id = dev_id;
		__entry->inst = inst;
		__entry->mem = mem;
		__entry->freq = freq;
		__entry->stall = stall;
		__entry->wb = wb;
		__entry->ratio = ratio;
	),

	TP_printk("dev: %s, id=%u, inst=%lu, mem=%lu, freq=%lu, stall=%u, wb=%u, ratio=%u",
		__get_str(name),
		__entry->dev_id,
		__entry->inst,
		__entry->mem,
		__entry->freq,
		__entry->stall,
		__entry->wb,
		__entry->ratio)
);

TRACE_EVENT(memlat_dev_update,

	TP_PROTO(const char *name, unsigned int dev_id, unsigned long inst,
		 unsigned long mem, unsigned long freq, unsigned long vote),

	TP_ARGS(name, dev_id, inst, mem, freq, vote),

	TP_STRUCT__entry(
		__string(name, name)
		__field(unsigned int, dev_id)
		__field(unsigned long, inst)
		__field(unsigned long, mem)
		__field(unsigned long, freq)
		__field(unsigned long, vote)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->dev_id = dev_id;
		__entry->inst = inst;
		__entry->mem = mem;
		__entry->freq = freq;
		__entry->vote = vote;
	),

	TP_printk("dev: %s, id=%u, inst=%lu, mem=%lu, freq=%lu, vote=%lu",
		__get_str(name),
		__entry->dev_id,
		__entry->inst,
		__entry->mem,
		__entry->freq,
		__entry->vote)
);

#endif /* _TRACE_DCVS_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace-dcvs

#include <trace/define_trace.h>
