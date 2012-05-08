/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include "kgsl_device.h"

struct kgsl_device;
struct kgsl_ringbuffer_issueibcmds;
struct kgsl_device_waittimestamp;

/*
 * Tracepoint for kgsl issue ib commands
 */
TRACE_EVENT(kgsl_issueibcmds,

	TP_PROTO(struct kgsl_device *device,
			struct kgsl_ringbuffer_issueibcmds *cmd,
			struct kgsl_ibdesc *ibdesc,
			int result),

	TP_ARGS(device, cmd, ibdesc, result),

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
		__entry->ibdesc_addr = ibdesc[0].gpuaddr;
		__entry->numibs = cmd->numibs;
		__entry->timestamp = cmd->timestamp;
		__entry->flags = cmd->flags;
		__entry->result = result;
	),

	TP_printk(
		"d_name=%s ctx=%u ib=0x%u numibs=%u timestamp=0x%x "
		"flags=0x%x(%s) result=%d",
		__get_str(device_name),
		__entry->drawctxt_id,
		__entry->ibdesc_addr,
		__entry->numibs,
		__entry->timestamp,
		__entry->flags,
		__entry->flags ? __print_flags(__entry->flags, "|",
			{ KGSL_CONTEXT_SAVE_GMEM, "SAVE_GMEM" },
			{ KGSL_CONTEXT_SUBMIT_IB_LIST, "IB_LIST" },
			{ KGSL_CONTEXT_CTX_SWITCH, "CTX_SWITCH" })
			: "None",
		__entry->result
	)
);

/*
 * Tracepoint for kgsl readtimestamp
 */
TRACE_EVENT(kgsl_readtimestamp,

	TP_PROTO(struct kgsl_device *device,
			unsigned int context_id,
			unsigned int type,
			unsigned int timestamp),

	TP_ARGS(device, context_id, type, timestamp),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, context_id)
		__field(unsigned int, type)
		__field(unsigned int, timestamp)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->context_id = context_id;
		__entry->type = type;
		__entry->timestamp = timestamp;
	),

	TP_printk(
		"d_name=%s context_id=%u type=%u timestamp=0x%x",
		__get_str(device_name),
		__entry->context_id,
		__entry->type,
		__entry->timestamp
	)
);

/*
 * Tracepoint for kgsl waittimestamp entry
 */
TRACE_EVENT(kgsl_waittimestamp_entry,

	TP_PROTO(struct kgsl_device *device,
			unsigned int context_id,
			unsigned int curr_ts,
			unsigned int wait_ts,
			unsigned int timeout),

	TP_ARGS(device, context_id, curr_ts, wait_ts, timeout),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, context_id)
		__field(unsigned int, curr_ts)
		__field(unsigned int, wait_ts)
		__field(unsigned int, timeout)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->context_id = context_id;
		__entry->curr_ts = curr_ts;
		__entry->wait_ts = wait_ts;
		__entry->timeout = timeout;
	),

	TP_printk(
		"d_name=%s context_id=%u curr_ts=%u timestamp=0x%x timeout=%u",
		__get_str(device_name),
		__entry->context_id,
		__entry->curr_ts,
		__entry->wait_ts,
		__entry->timeout
	)
);

/*
 * Tracepoint for kgsl waittimestamp exit
 */
TRACE_EVENT(kgsl_waittimestamp_exit,

	TP_PROTO(struct kgsl_device *device, unsigned int curr_ts,
		 int result),

	TP_ARGS(device, curr_ts, result),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, curr_ts)
		__field(int, result)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->curr_ts = curr_ts;
		__entry->result = result;
	),

	TP_printk(
		"d_name=%s curr_ts=%u result=%d",
		__get_str(device_name),
		__entry->curr_ts,
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

TRACE_EVENT(kgsl_mem_alloc,

	TP_PROTO(struct kgsl_mem_entry *mem_entry),

	TP_ARGS(mem_entry),

	TP_STRUCT__entry(
		__field(unsigned int, gpuaddr)
		__field(unsigned int, size)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
	),

	TP_printk(
		"gpuaddr=0x%08x size=%d",
		__entry->gpuaddr, __entry->size
	)
);

TRACE_EVENT(kgsl_mem_map,

	TP_PROTO(struct kgsl_mem_entry *mem_entry, int fd),

	TP_ARGS(mem_entry, fd),

	TP_STRUCT__entry(
		__field(unsigned int, gpuaddr)
		__field(unsigned int, size)
		__field(int, fd)
		__field(int, type)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->fd = fd;
		__entry->type = mem_entry->memtype;
	),

	TP_printk(
		"gpuaddr=0x%08x size=%d type=%d fd=%d",
		__entry->gpuaddr, __entry->size,
		__entry->type, __entry->fd
	)
);

TRACE_EVENT(kgsl_mem_free,

	TP_PROTO(struct kgsl_mem_entry *mem_entry),

	TP_ARGS(mem_entry),

	TP_STRUCT__entry(
		__field(unsigned int, gpuaddr)
		__field(unsigned int, size)
		__field(int, type)
		__field(int, fd)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->type = mem_entry->memtype;
	),

	TP_printk(
		"gpuaddr=0x%08x size=%d type=%d",
		__entry->gpuaddr, __entry->size, __entry->type
	)
);

DECLARE_EVENT_CLASS(kgsl_mem_timestamp_template,

	TP_PROTO(struct kgsl_device *device, struct kgsl_mem_entry *mem_entry,
		unsigned int id, unsigned int curr_ts, unsigned int free_ts),

	TP_ARGS(device, mem_entry, id, curr_ts, free_ts),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, gpuaddr)
		__field(unsigned int, size)
		__field(int, type)
		__field(unsigned int, drawctxt_id)
		__field(unsigned int, curr_ts)
		__field(unsigned int, free_ts)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->drawctxt_id = id;
		__entry->type = mem_entry->memtype;
		__entry->curr_ts = curr_ts;
		__entry->free_ts = free_ts;
	),

	TP_printk(
		"d_name=%s gpuaddr=0x%08x size=%d type=%d ctx=%u"
		" curr_ts=0x%08x free_ts=0x%08x",
		__get_str(device_name),
		__entry->gpuaddr,
		__entry->size,
		__entry->type,
		__entry->drawctxt_id,
		__entry->curr_ts,
		__entry->free_ts
	)
);

DEFINE_EVENT(kgsl_mem_timestamp_template, kgsl_mem_timestamp_queue,
	TP_PROTO(struct kgsl_device *device, struct kgsl_mem_entry *mem_entry,
		unsigned int id, unsigned int curr_ts, unsigned int free_ts),
	TP_ARGS(device, mem_entry, id, curr_ts, free_ts)
);

DEFINE_EVENT(kgsl_mem_timestamp_template, kgsl_mem_timestamp_free,
	TP_PROTO(struct kgsl_device *device, struct kgsl_mem_entry *mem_entry,
		unsigned int id, unsigned int curr_ts, unsigned int free_ts),
	TP_ARGS(device, mem_entry, id, curr_ts, free_ts)
);

TRACE_EVENT(kgsl_context_create,

	TP_PROTO(struct kgsl_device *device, struct kgsl_context *context,
		 unsigned int flags),

	TP_ARGS(device, context, flags),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, id)
		__field(unsigned int, flags)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->id = context->id;
		__entry->flags = flags;
	),

	TP_printk(
		"d_name=%s ctx=%u flags=0x%x %s",
		__get_str(device_name), __entry->id, __entry->flags,
		__entry->flags ? __print_flags(__entry->flags, "|",
			{ KGSL_CONTEXT_NO_GMEM_ALLOC , "NO_GMEM_ALLOC" },
			{ KGSL_CONTEXT_PREAMBLE, "PREAMBLE" },
			{ KGSL_CONTEXT_TRASH_STATE, "TRASH_STATE" },
			{ KGSL_CONTEXT_PER_CONTEXT_TS, "PER_CONTEXT_TS" })
			: "None"
	)
);

TRACE_EVENT(kgsl_context_detach,

	TP_PROTO(struct kgsl_device *device, struct kgsl_context *context),

	TP_ARGS(device, context),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, id)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->id = context->id;
	),

	TP_printk(
		"d_name=%s ctx=%u",
		__get_str(device_name), __entry->id
	)
);

TRACE_EVENT(kgsl_mmu_pagefault,

	TP_PROTO(struct kgsl_device *device, unsigned int page,
		 unsigned int pt, const char *op),

	TP_ARGS(device, page, pt, op),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, page)
		__field(unsigned int, pt)
		__string(op, op)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->page = page;
		__entry->pt = pt;
		__assign_str(op, op);
	),

	TP_printk(
		"d_name=%s page=0x%08x pt=%d op=%s\n",
		__get_str(device_name), __entry->page, __entry->pt,
		__get_str(op)
	)
);

#endif /* _KGSL_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
