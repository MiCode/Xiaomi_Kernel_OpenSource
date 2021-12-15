/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#undef TRACE_SYSTEM
#define TRACE_SYSTEM thermal

#if !defined(_MTK_THERMAL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_THERMAL_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(cooling_device_state,
		TP_PROTO(int device, unsigned long state),
		TP_ARGS(device, state), TP_STRUCT__entry(__field(int, device)
			__field(unsigned long, state)
			),
		TP_fast_assign(__entry->device = device;
			__entry->state = state;),
		TP_printk("cooling_device=%d, state=%lu\n",
					__entry->device, __entry->state)
	   );

TRACE_EVENT(thermal_zone_state,
		TP_PROTO(int device, int state),
		TP_ARGS(device, state), TP_STRUCT__entry(__field(int, device)
			__field(int, state)
			),
		TP_fast_assign(__entry->device = device;
			__entry->state = state;),
		TP_printk("thermal_zone=%d, state=%d\n",
					__entry->device, __entry->state)
	   );
#endif				/* _MTK_THERMAL_TRACE_H */

	/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mach/mtk_thermal_trace
#include <trace/define_trace.h>
