/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmstat

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
extern bool mtk_get_gpu_memory_usage(unsigned int *pMemUsage);
#endif

#if !defined(__TRACE_MMSTAT_H) || defined(TRACE_HEADER_MULTI_READ)
#define __TRACE_MMSTAT_H

#include <linux/tracepoint.h>
#include <linux/trace_seq.h>

const char *mmstat_trace_print_arrayset_seq(struct trace_seq*,
		const void*, int, int);
#define __print_mmstat_array(array, len, sets)				\
	({								\
		mmstat_trace_print_arrayset_seq(p, array, len, sets);	\
	})

/* num_entries should be a multiplie of set_size */
DECLARE_EVENT_CLASS(mmstat_trace_arrayset,

	TP_PROTO(unsigned long *arrayset, size_t num_entries, size_t set_size),

	TP_ARGS(arrayset, num_entries, set_size),

	TP_STRUCT__entry(
		__dynamic_array(unsigned long, arrayset, num_entries)
		__field(size_t, num_entries)
		__field(size_t, set_size)
	),

	TP_fast_assign(
		memcpy(__get_dynamic_array(arrayset), arrayset,
			num_entries * sizeof(unsigned long));
		__entry->num_entries = num_entries;
		__entry->set_size = set_size;
	),

	TP_printk("%s", __print_mmstat_array(__get_dynamic_array(arrayset),
			__entry->num_entries, __entry->set_size))
);

DEFINE_EVENT(mmstat_trace_arrayset, mmstat_trace_meminfo,

	TP_PROTO(unsigned long *meminfo, size_t num_entries, size_t set_size),

	TP_ARGS(meminfo, num_entries, set_size)
);

DEFINE_EVENT(mmstat_trace_arrayset, mmstat_trace_vmstat,

	TP_PROTO(unsigned long *vmstat, size_t num_entries, size_t set_size),

	TP_ARGS(vmstat, num_entries, set_size)
);

DEFINE_EVENT(mmstat_trace_arrayset, mmstat_trace_buddyinfo,

	TP_PROTO(unsigned long *buddyinfo, size_t num_entries, size_t set_size),

	TP_ARGS(buddyinfo, num_entries, set_size)
);

DEFINE_EVENT(mmstat_trace_arrayset, mmstat_trace_proc,

	TP_PROTO(unsigned long *proc_array, size_t num_entries,
		size_t set_size),

	TP_ARGS(proc_array, num_entries, set_size)
);
#endif /* __TRACE_MMSTAT_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace_mmstat

/* This part must be outside protection */
#include <trace/define_trace.h>
