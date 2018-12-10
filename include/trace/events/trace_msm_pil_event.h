/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM msm_pil_event

#if !defined(_TRACE_MSM_PIL_EVENT_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_PIL_EVENT_H_

#include <linux/tracepoint.h>
#include <../drivers/soc/qcom/peripheral-loader.h>

TRACE_EVENT(pil_event,

	TP_PROTO(const char *event_name, struct pil_desc *desc),

	TP_ARGS(event_name, desc),

	TP_STRUCT__entry(
		__string(event_name, event_name)
		__string(fw_name, desc->fw_name)
	),

	TP_fast_assign(
		__assign_str(event_name, event_name);
		__assign_str(fw_name, desc->fw_name);
	),

	TP_printk("event_name=%s fw_name=%s",
		__get_str(event_name),
		__get_str(fw_name))
);

TRACE_EVENT(pil_notif,

	TP_PROTO(const char *event_name, unsigned long code,
	const char *fw_name),

	TP_ARGS(event_name, code, fw_name),

	TP_STRUCT__entry(
		__string(event_name, event_name)
		__field(unsigned long, code)
		__string(fw_name, fw_name)
	),

	TP_fast_assign(
		__assign_str(event_name, event_name);
		__entry->code = code;
		__assign_str(fw_name, fw_name);
	),

	TP_printk("event_name=%s code=%lu fw=%s",
		__get_str(event_name),
		__entry->code,
		__get_str(fw_name))
);

TRACE_EVENT(pil_func,

	TP_PROTO(const char *func_name),

	TP_ARGS(func_name),

	TP_STRUCT__entry(
		__string(func_name, func_name)
	),

	TP_fast_assign(
		__assign_str(func_name, func_name);
	),

	TP_printk("func_name=%s",
		__get_str(func_name))
);

#endif
#define TRACE_INCLUDE_FILE trace_msm_pil_event
#include <trace/define_trace.h>
