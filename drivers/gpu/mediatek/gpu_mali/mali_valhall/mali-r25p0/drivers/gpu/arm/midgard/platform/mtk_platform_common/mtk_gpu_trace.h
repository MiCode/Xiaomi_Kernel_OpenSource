/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM perf_tracker

#if !defined(_MTK_GPU_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_GPU_TRACE_H

#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>


TRACE_EVENT(perf_index_gpu,
	TP_PROTO(u32 *gpu_data, u32 lens),
	TP_ARGS(gpu_data, lens),
	TP_STRUCT__entry(
		__dynamic_array(u32, gpu_data, lens)
		__field(u32, lens)
	),
	TP_fast_assign(
		memcpy(__get_dynamic_array(gpu_data), gpu_data,
			lens * sizeof(u32));
		__entry->lens = lens;
	),
	TP_printk("data=%s", __print_array(__get_dynamic_array(gpu_data),
		__entry->lens, sizeof(u32)))
);

#endif /*_MTK_GPU_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_gpu_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
