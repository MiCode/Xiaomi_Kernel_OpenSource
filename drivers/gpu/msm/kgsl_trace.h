/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#if !defined(_KGSL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _KGSL_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM kgsl
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE kgsl_trace

#include <linux/tracepoint.h>

struct kgsl_device;
struct kgsl_ringbuffer_issueibcmds;
struct kgsl_device_waittimestamp;

/*
 * Tracepoint for kgsl issue ib commands
 */
TRACE_EVENT(kgsl_issueibcmds,

	TP_PROTO(struct kgsl_device *device,
			struct kgsl_ringbuffer_issueibcmds *cmd, int result),

	TP_ARGS(device, cmd, result),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, drawctxt_id)
		__field(unsigned int, ibdesc_addr)
		__field(unsigned int, numibs)
		__field(unsigned int, timestamp)
		__field(unsigned int, flags)
		__field(int, result)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->drawctxt_id = cmd->drawctxt_id;
		__entry->ibdesc_addr = cmd->ibdesc_addr;
		__entry->numibs = cmd->numibs;
		__entry->timestamp = cmd->timestamp;
		__entry->flags = cmd->flags;
		__entry->result = result;
	),

	TP_printk(
		"d_name=%s ctx=%u ib=%u numibs=%u timestamp=%u "
		"flags=%u result=%d",
		__get_str(device_name),
		__entry->drawctxt_id,
		__entry->ibdesc_addr,
		__entry->numibs,
		__entry->timestamp,
		__entry->flags,
		__entry->result
	)
);

/*
 * Tracepoint for kgsl readtimestamp
 */
TRACE_EVENT(kgsl_readtimestamp,

	TP_PROTO(struct kgsl_device *device,
			struct kgsl_cmdstream_readtimestamp *cmd),

	TP_ARGS(device, cmd),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, type)
		__field(unsigned int, timestamp)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->type = cmd->type;
		__entry->timestamp = cmd->timestamp;
	),

	TP_printk(
		"d_name=%s type=%u timestamp=%u",
		__get_str(device_name),
		__entry->type,
		__entry->timestamp
	)
);

/*
 * Tracepoint for kgsl waittimestamp entry
 */
TRACE_EVENT(kgsl_waittimestamp_entry,

	TP_PROTO(struct kgsl_device *device,
			struct kgsl_device_waittimestamp *cmd),

	TP_ARGS(device, cmd),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, timestamp)
		__field(unsigned int, timeout)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->timestamp = cmd->timestamp;
		__entry->timeout = cmd->timeout;
	),

	TP_printk(
		"d_name=%s timestamp=%u timeout=%u",
		__get_str(device_name),
		__entry->timestamp,
		__entry->timeout
	)
);

/*
 * Tracepoint for kgsl waittimestamp exit
 */
TRACE_EVENT(kgsl_waittimestamp_exit,

	TP_PROTO(struct kgsl_device *device, int result),

	TP_ARGS(device, result),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(int, result)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->result = result;
	),

	TP_printk(
		"d_name=%s result=%d",
		__get_str(device_name),
		__entry->result
	)
);

DECLARE_EVENT_CLASS(kgsl_pwr_template,
	TP_PROTO(struct kgsl_device *device, int on),

	TP_ARGS(device, on),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(int, on)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->on = on;
	),

	TP_printk(
		"d_name=%s %s",
		__get_str(device_name),
		__entry->on ? "on" : "off"
	)
);

DEFINE_EVENT(kgsl_pwr_template, kgsl_clk,
	TP_PROTO(struct kgsl_device *device, int on),
	TP_ARGS(device, on)
);

DEFINE_EVENT(kgsl_pwr_template, kgsl_irq,
	TP_PROTO(struct kgsl_device *device, int on),
	TP_ARGS(device, on)
);

DEFINE_EVENT(kgsl_pwr_template, kgsl_bus,
	TP_PROTO(struct kgsl_device *device, int on),
	TP_ARGS(device, on)
);

DEFINE_EVENT(kgsl_pwr_template, kgsl_rail,
	TP_PROTO(struct kgsl_device *device, int on),
	TP_ARGS(device, on)
);

TRACE_EVENT(kgsl_pwrlevel,

	TP_PROTO(struct kgsl_device *device, unsigned int pwrlevel,
		 unsigned int freq),

	TP_ARGS(device, pwrlevel, freq),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, pwrlevel)
		__field(unsigned int, freq)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->pwrlevel = pwrlevel;
		__entry->freq = freq;
	),

	TP_printk(
		"d_name=%s pwrlevel=%d freq=%d",
		__get_str(device_name),
		__entry->pwrlevel,
		__entry->freq
	)
);

DECLARE_EVENT_CLASS(kgsl_pwrstate_template,
	TP_PROTO(struct kgsl_device *device, unsigned int state),

	TP_ARGS(device, state),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, state)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->state = state;
	),

	TP_printk(
		"d_name=%s %s",
		__get_str(device_name),
		kgsl_pwrstate_to_str(__entry->state)
	)
);

DEFINE_EVENT(kgsl_pwrstate_template, kgsl_pwr_set_state,
	TP_PROTO(struct kgsl_device *device, unsigned int state),
	TP_ARGS(device, state)
);

DEFINE_EVENT(kgsl_pwrstate_template, kgsl_pwr_request_state,
	TP_PROTO(struct kgsl_device *device, unsigned int state),
	TP_ARGS(device, state)
);

#endif /* _KGSL_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
