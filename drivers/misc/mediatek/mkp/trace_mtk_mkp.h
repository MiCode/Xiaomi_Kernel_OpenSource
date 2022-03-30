/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_mkp

#if !defined(_TRACE_MTK_MKP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_MKP_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/rbtree.h>
#include <linux/types.h> // for phys_addr_t
#include <linux/random.h>

#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/sizes.h>

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <uapi/linux/sched/types.h>
#include <linux/futex.h>
#include <linux/plist.h>
#include <linux/percpu-defs.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <linux/vmalloc.h>
#include <linux/cma.h>
#include <linux/of_reserved_mem.h>
#include <linux/timer.h>

#include <asm/memory.h> // for MODULE_VADDR
#include <linux/types.h> // for phys_addr_t

#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>

const char *mkp_trace_print_array(void);
#define __print_mkp_array() \
({ \
	mkp_trace_print_array(); \
})

/*
 * Tracepoint for mkp test:
 */
TRACE_EVENT(mkp_trace_event_test,

	TP_PROTO(char *test),

	TP_ARGS(test),

	TP_STRUCT__entry(
		__string(test, test)
	),

	TP_fast_assign(
		__assign_str(test, test);
	),

	TP_printk("mkp_test: %s\n", __print_mkp_array())
);

#endif /* _TRACE_MTK_MKP_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_mtk_mkp
/* This part must be outside protection */
#include <trace/define_trace.h>
