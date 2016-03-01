/* Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
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
#include "adreno_drawctxt.h"

struct kgsl_device;
struct kgsl_ringbuffer_issueibcmds;
struct kgsl_device_waittimestamp;

/*
 * Tracepoint for kgsl issue ib commands
 */
TRACE_EVENT(kgsl_issueibcmds,

	TP_PROTO(struct kgsl_device *device,
			int drawctxt_id,
			struct kgsl_cmdbatch *cmdbatch,
			unsigned int numibs,
			int timestamp,
			int flags,
			int result,
			unsigned int type),

	TP_ARGS(device, drawctxt_id, cmdbatch, numibs, timestamp,
		flags, result, type),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, drawctxt_id)
		__field(unsigned int, numibs)
		__field(unsigned int, timestamp)
		__field(unsigned int, flags)
		__field(int, result)
		__field(unsigned int, drawctxt_type)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->drawctxt_id = drawctxt_id;
		__entry->numibs = numibs;
		__entry->timestamp = timestamp;
		__entry->flags = flags;
		__entry->result = result;
		__entry->drawctxt_type = type;
	),

	TP_printk(
		"d_name=%s ctx=%u ib=0x0 numibs=%u ts=%u "
		"flags=%s result=%d type=%s",
		__get_str(device_name),
		__entry->drawctxt_id,
		__entry->numibs,
		__entry->timestamp,
		__entry->flags ? __print_flags(__entry->flags, "|",
						KGSL_CMDBATCH_FLAGS) : "None",
		__entry->result,
		__print_symbolic(__entry->drawctxt_type, KGSL_CONTEXT_TYPES)
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
		"d_name=%s context_id=%u type=%u ts=%u",
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
		"d_name=%s ctx=%u curr_ts=%u ts=%u timeout=%u",
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
		"d_name=%s flag=%s",
		__get_str(device_name),
		__entry->on ? "on" : "off"
	)
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

DEFINE_EVENT(kgsl_pwr_template, kgsl_retention_clk,
	TP_PROTO(struct kgsl_device *device, int on),
	TP_ARGS(device, on)
);

TRACE_EVENT(kgsl_clk,

	TP_PROTO(struct kgsl_device *device, unsigned int on,
		unsigned int freq),

	TP_ARGS(device, on, freq),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(int, on)
		__field(unsigned int, freq)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->on = on;
		__entry->freq = freq;
	),

	TP_printk(
		"d_name=%s flag=%s active_freq=%d",
		__get_str(device_name),
		__entry->on ? "on" : "off",
		__entry->freq
	)
);

TRACE_EVENT(kgsl_pwrlevel,

	TP_PROTO(struct kgsl_device *device,
		unsigned int pwrlevel,
		unsigned int freq,
		unsigned int prev_pwrlevel,
		unsigned int prev_freq),

	TP_ARGS(device, pwrlevel, freq, prev_pwrlevel, prev_freq),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, pwrlevel)
		__field(unsigned int, freq)
		__field(unsigned int, prev_pwrlevel)
		__field(unsigned int, prev_freq)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->pwrlevel = pwrlevel;
		__entry->freq = freq;
		__entry->prev_pwrlevel = prev_pwrlevel;
		__entry->prev_freq = prev_freq;
	),

	TP_printk(
		"d_name=%s pwrlevel=%d freq=%d prev_pwrlevel=%d prev_freq=%d",
		__get_str(device_name),
		__entry->pwrlevel,
		__entry->freq,
		__entry->prev_pwrlevel,
		__entry->prev_freq
	)
);

TRACE_EVENT(kgsl_buslevel,

	TP_PROTO(struct kgsl_device *device, unsigned int pwrlevel,
		 unsigned int bus),

	TP_ARGS(device, pwrlevel, bus),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, pwrlevel)
		__field(unsigned int, bus)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->pwrlevel = pwrlevel;
		__entry->bus = bus;
	),

	TP_printk(
		"d_name=%s pwrlevel=%d bus=%d",
		__get_str(device_name),
		__entry->pwrlevel,
		__entry->bus
	)
);

TRACE_EVENT(kgsl_gpubusy,
	TP_PROTO(struct kgsl_device *device, unsigned int busy,
		unsigned int elapsed),

	TP_ARGS(device, busy, elapsed),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, busy)
		__field(unsigned int, elapsed)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->busy = busy;
		__entry->elapsed = elapsed;
	),

	TP_printk(
		"d_name=%s busy=%u elapsed=%d",
		__get_str(device_name),
		__entry->busy,
		__entry->elapsed
	)
);

TRACE_EVENT(kgsl_pwrstats,
	TP_PROTO(struct kgsl_device *device, s64 time,
		struct kgsl_power_stats *pstats, u32 ctxt_count),

	TP_ARGS(device, time, pstats, ctxt_count),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(s64, total_time)
		__field(u64, busy_time)
		__field(u64, ram_time)
		__field(u64, ram_wait)
		__field(u32, context_count)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->total_time = time;
		__entry->busy_time = pstats->busy_time;
		__entry->ram_time = pstats->ram_time;
		__entry->ram_wait = pstats->ram_wait;
		__entry->context_count = ctxt_count;
	),

	TP_printk(
		"d_name=%s total=%lld busy=%lld ram_time=%lld ram_wait=%lld context_count=%u",
		__get_str(device_name), __entry->total_time, __entry->busy_time,
		__entry->ram_time, __entry->ram_wait, __entry->context_count
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
		"d_name=%s state=%s",
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
		__field(uint64_t, gpuaddr)
		__field(uint64_t, size)
		__field(unsigned int, tgid)
		__array(char, usage, 16)
		__field(unsigned int, id)
		__field(uint64_t, flags)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->tgid = mem_entry->priv->pid;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->id = mem_entry->id;
		__entry->flags = mem_entry->memdesc.flags;
	),

	TP_printk(
		"gpuaddr=0x%llx size=%llu tgid=%u usage=%s id=%u flags=0x%llx",
		__entry->gpuaddr, __entry->size, __entry->tgid,
		__entry->usage, __entry->id, __entry->flags
	)
);

TRACE_EVENT(kgsl_mem_mmap,

	TP_PROTO(struct kgsl_mem_entry *mem_entry),

	TP_ARGS(mem_entry),

	TP_STRUCT__entry(
		__field(unsigned long, useraddr)
		__field(uint64_t, gpuaddr)
		__field(uint64_t, size)
		__array(char, usage, 16)
		__field(unsigned int, id)
		__field(uint64_t, flags)
	),

	TP_fast_assign(
		__entry->useraddr = mem_entry->memdesc.useraddr;
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->id = mem_entry->id;
		__entry->flags = mem_entry->memdesc.flags;
	),

	TP_printk(
	 "useraddr=0x%lx gpuaddr=0x%llx size=%llu usage=%s id=%u flags=0x%llx",
		__entry->useraddr, __entry->gpuaddr, __entry->size,
		__entry->usage, __entry->id, __entry->flags
	)
);

TRACE_EVENT(kgsl_mem_unmapped_area_collision,

	TP_PROTO(struct kgsl_mem_entry *mem_entry,
		 unsigned long addr,
		 unsigned long len),

	TP_ARGS(mem_entry, addr, len),

	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned long, addr)
		__field(unsigned long, len)
	),

	TP_fast_assign(
		__entry->id = mem_entry->id;
		__entry->len = len;
		__entry->addr = addr;
	),

	TP_printk(
		"id=%u len=%lu addr=0x%lx",
		__entry->id, __entry->len, __entry->addr
	)
);

TRACE_EVENT(kgsl_mem_map,

	TP_PROTO(struct kgsl_mem_entry *mem_entry, int fd),

	TP_ARGS(mem_entry, fd),

	TP_STRUCT__entry(
		__field(uint64_t, gpuaddr)
		__field(uint64_t, size)
		__field(int, fd)
		__field(int, type)
		__field(unsigned int, tgid)
		__array(char, usage, 16)
		__field(unsigned int, id)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->fd = fd;
		__entry->type = kgsl_memdesc_usermem_type(&mem_entry->memdesc);
		__entry->tgid = mem_entry->priv->pid;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->id = mem_entry->id;
	),

	TP_printk(
		"gpuaddr=0x%llx size=%llu type=%s fd=%d tgid=%u usage=%s id=%u",
		__entry->gpuaddr, __entry->size,
		__print_symbolic(__entry->type, KGSL_MEM_TYPES),
		__entry->fd, __entry->tgid,
		__entry->usage, __entry->id
	)
);

TRACE_EVENT(kgsl_mem_free,

	TP_PROTO(struct kgsl_mem_entry *mem_entry),

	TP_ARGS(mem_entry),

	TP_STRUCT__entry(
		__field(uint64_t, gpuaddr)
		__field(uint64_t, size)
		__field(int, type)
		__field(int, fd)
		__field(unsigned int, tgid)
		__array(char, usage, 16)
		__field(unsigned int, id)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->type = kgsl_memdesc_usermem_type(&mem_entry->memdesc);
		__entry->tgid = mem_entry->priv->pid;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->id = mem_entry->id;
	),

	TP_printk(
		"gpuaddr=0x%llx size=%llu type=%s tgid=%u usage=%s id=%u",
		__entry->gpuaddr, __entry->size,
		__print_symbolic(__entry->type, KGSL_MEM_TYPES),
		__entry->tgid, __entry->usage, __entry->id
	)
);

TRACE_EVENT(kgsl_mem_sync_cache,

	TP_PROTO(struct kgsl_mem_entry *mem_entry, uint64_t offset,
		uint64_t length, unsigned int op),

	TP_ARGS(mem_entry, offset, length, op),

	TP_STRUCT__entry(
		__field(uint64_t, gpuaddr)
		__array(char, usage, 16)
		__field(unsigned int, tgid)
		__field(unsigned int, id)
		__field(unsigned int, op)
		__field(uint64_t, offset)
		__field(uint64_t, length)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->tgid = mem_entry->priv->pid;
		__entry->id = mem_entry->id;
		__entry->op = op;
		__entry->offset = offset;
		__entry->length = (length == 0) ?
				mem_entry->memdesc.size : length;
	),

	TP_printk(
	 "gpuaddr=0x%llx size=%llu tgid=%u  usage=%s id=%u op=%c%c offset=%llu",
		__entry->gpuaddr,  __entry->length,
		__entry->tgid, __entry->usage, __entry->id,
		(__entry->op & KGSL_GPUMEM_CACHE_CLEAN) ? 'c' : '.',
		(__entry->op & KGSL_GPUMEM_CACHE_INV) ? 'i' : '.',
		__entry->offset
	)
);

TRACE_EVENT(kgsl_mem_sync_full_cache,

	TP_PROTO(unsigned int num_bufs, uint64_t bulk_size),
	TP_ARGS(num_bufs, bulk_size),

	TP_STRUCT__entry(
		__field(unsigned int, num_bufs)
		__field(uint64_t, bulk_size)
	),

	TP_fast_assign(
		__entry->num_bufs = num_bufs;
		__entry->bulk_size = bulk_size;
	),

	TP_printk(
		"num_bufs=%u bulk_size=%llu op=ci",
		__entry->num_bufs, __entry->bulk_size
	)
);

DECLARE_EVENT_CLASS(kgsl_mem_timestamp_template,

	TP_PROTO(struct kgsl_device *device, struct kgsl_mem_entry *mem_entry,
		unsigned int id, unsigned int curr_ts, unsigned int free_ts),

	TP_ARGS(device, mem_entry, id, curr_ts, free_ts),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(uint64_t, gpuaddr)
		__field(uint64_t, size)
		__field(int, type)
		__array(char, usage, 16)
		__field(unsigned int, id)
		__field(unsigned int, drawctxt_id)
		__field(unsigned int, curr_ts)
		__field(unsigned int, free_ts)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->id = mem_entry->id;
		__entry->drawctxt_id = id;
		__entry->type = kgsl_memdesc_usermem_type(&mem_entry->memdesc);
		__entry->curr_ts = curr_ts;
		__entry->free_ts = free_ts;
	),

	TP_printk(
		"d_name=%s gpuaddr=0x%llx size=%llu type=%s usage=%s id=%u ctx=%u"
		" curr_ts=%u free_ts=%u",
		__get_str(device_name),
		__entry->gpuaddr,
		__entry->size,
		__print_symbolic(__entry->type, KGSL_MEM_TYPES),
		__entry->usage,
		__entry->id,
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
		__field(unsigned int, priority)
		__field(unsigned int, type)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->id = context->id;
		__entry->flags = flags & ~(KGSL_CONTEXT_PRIORITY_MASK |
						KGSL_CONTEXT_TYPE_MASK);
		__entry->priority =
			(flags & KGSL_CONTEXT_PRIORITY_MASK)
				>> KGSL_CONTEXT_PRIORITY_SHIFT;
		__entry->type =
			(flags & KGSL_CONTEXT_TYPE_MASK)
				>> KGSL_CONTEXT_TYPE_SHIFT;
	),

	TP_printk(
		"d_name=%s ctx=%u flags=%s priority=%u type=%s",
		__get_str(device_name), __entry->id,
		__entry->flags ? __print_flags(__entry->flags, "|",
						KGSL_CONTEXT_FLAGS) : "None",
		__entry->priority,
		__print_symbolic(__entry->type, KGSL_CONTEXT_TYPES)
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

TRACE_EVENT(kgsl_context_destroy,

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

TRACE_EVENT(kgsl_user_pwrlevel_constraint,

	TP_PROTO(struct kgsl_device *device, unsigned int id, unsigned int type,
		unsigned int sub_type),

	TP_ARGS(device, id, type, sub_type),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, id)
		__field(unsigned int, type)
		__field(unsigned int, sub_type)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->id = id;
		__entry->type = type;
		__entry->sub_type = sub_type;
	),

	TP_printk(
		"d_name=%s ctx=%u constraint_type=%s constraint_subtype=%s",
		__get_str(device_name), __entry->id,
		__print_symbolic(__entry->type, KGSL_CONSTRAINT_TYPES),
		__print_symbolic(__entry->sub_type,
		KGSL_CONSTRAINT_PWRLEVEL_SUBTYPES)
	)
);

TRACE_EVENT(kgsl_constraint,

	TP_PROTO(struct kgsl_device *device, unsigned int type,
		unsigned int value, unsigned int on),

	TP_ARGS(device, type, value, on),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, type)
		__field(unsigned int, value)
		__field(unsigned int, on)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->type = type;
		__entry->value = value;
		__entry->on = on;
	),

	TP_printk(
		"d_name=%s constraint_type=%s constraint_value=%u status=%s",
		__get_str(device_name),
		__print_symbolic(__entry->type, KGSL_CONSTRAINT_TYPES),
		__entry->value,
		__entry->on ? "ON" : "OFF"
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
		"d_name=%s page=0x%08x pt=%u op=%s",
		__get_str(device_name), __entry->page, __entry->pt,
		__get_str(op)
	)
);

TRACE_EVENT(kgsl_regwrite,

	TP_PROTO(struct kgsl_device *device, unsigned int offset,
		unsigned int value),

	TP_ARGS(device, offset, value),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, offset)
		__field(unsigned int, value)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->offset = offset;
		__entry->value = value;
	),

	TP_printk(
		"d_name=%s reg=0x%x value=0x%x",
		__get_str(device_name), __entry->offset, __entry->value
	)
);

TRACE_EVENT(kgsl_popp_level,

	TP_PROTO(struct kgsl_device *device, int level1, int level2),

	TP_ARGS(device, level1, level2),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(int, level1)
		__field(int, level2)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->level1 = level1;
		__entry->level2 = level2;
	),

	TP_printk(
		"d_name=%s old level=%d new level=%d",
		__get_str(device_name), __entry->level1, __entry->level2)
);

TRACE_EVENT(kgsl_popp_mod,

	TP_PROTO(struct kgsl_device *device, int x, int y),

	TP_ARGS(device, x, y),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(int, x)
		__field(int, y)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->x = x;
		__entry->y = y;
	),

	TP_printk(
		"d_name=%s GPU busy mod=%d bus busy mod=%d",
		__get_str(device_name), __entry->x, __entry->y)
);

TRACE_EVENT(kgsl_popp_nap,

	TP_PROTO(struct kgsl_device *device, int t, int nap, int percent),

	TP_ARGS(device, t, nap, percent),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(int, t)
		__field(int, nap)
		__field(int, percent)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->t = t;
		__entry->nap = nap;
		__entry->percent = percent;
	),

	TP_printk(
		"d_name=%s nap time=%d number of naps=%d percentage=%d",
		__get_str(device_name), __entry->t, __entry->nap,
			__entry->percent)
);

TRACE_EVENT(kgsl_register_event,
		TP_PROTO(unsigned int id, unsigned int timestamp, void *func),
		TP_ARGS(id, timestamp, func),
		TP_STRUCT__entry(
			__field(unsigned int, id)
			__field(unsigned int, timestamp)
			__field(void *, func)
		),
		TP_fast_assign(
			__entry->id = id;
			__entry->timestamp = timestamp;
			__entry->func = func;
		),
		TP_printk(
			"ctx=%u ts=%u cb=%pF",
			__entry->id, __entry->timestamp, __entry->func)
);

TRACE_EVENT(kgsl_fire_event,
		TP_PROTO(unsigned int id, unsigned int ts,
			unsigned int type, unsigned int age, void *func),
		TP_ARGS(id, ts, type, age, func),
		TP_STRUCT__entry(
			__field(unsigned int, id)
			__field(unsigned int, ts)
			__field(unsigned int, type)
			__field(unsigned int, age)
			__field(void *, func)
		),
		TP_fast_assign(
			__entry->id = id;
			__entry->ts = ts;
			__entry->type = type;
			__entry->age = age;
			__entry->func = func;
		),
		TP_printk(
			"ctx=%u ts=%u type=%s age=%u cb=%pF",
			__entry->id, __entry->ts,
			__print_symbolic(__entry->type, KGSL_EVENT_TYPES),
			__entry->age, __entry->func)
);

TRACE_EVENT(kgsl_active_count,

	TP_PROTO(struct kgsl_device *device, unsigned long ip),

	TP_ARGS(device, ip),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, count)
		__field(unsigned long, ip)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->count = atomic_read(&device->active_cnt);
		__entry->ip = ip;
	),

	TP_printk(
		"d_name=%s active_cnt=%u func=%pf",
		__get_str(device_name), __entry->count, (void *) __entry->ip
	)
);

TRACE_EVENT(kgsl_pagetable_destroy,
	TP_PROTO(u64 ptbase, unsigned int name),
	TP_ARGS(ptbase, name),
	TP_STRUCT__entry(
		__field(u64, ptbase)
		__field(unsigned int, name)
	),
	TP_fast_assign(
		__entry->ptbase = ptbase;
		__entry->name = name;
	),
	TP_printk("ptbase=%llx name=%u", __entry->ptbase, __entry->name)
);

DECLARE_EVENT_CLASS(syncpoint_timestamp_template,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, struct kgsl_context *context,
		unsigned int timestamp),
	TP_ARGS(cmdbatch, context, timestamp),
	TP_STRUCT__entry(
		__field(unsigned int, cmdbatch_context_id)
		__field(unsigned int, context_id)
		__field(unsigned int, timestamp)
	),
	TP_fast_assign(
		__entry->cmdbatch_context_id = cmdbatch->context->id;
		__entry->context_id = context->id;
		__entry->timestamp = timestamp;
	),
	TP_printk("ctx=%d sync ctx=%d ts=%d",
		__entry->cmdbatch_context_id, __entry->context_id,
		__entry->timestamp)
);

DEFINE_EVENT(syncpoint_timestamp_template, syncpoint_timestamp,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, struct kgsl_context *context,
		unsigned int timestamp),
	TP_ARGS(cmdbatch, context, timestamp)
);

DEFINE_EVENT(syncpoint_timestamp_template, syncpoint_timestamp_expire,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, struct kgsl_context *context,
		unsigned int timestamp),
	TP_ARGS(cmdbatch, context, timestamp)
);

DECLARE_EVENT_CLASS(syncpoint_fence_template,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, char *name),
	TP_ARGS(cmdbatch, name),
	TP_STRUCT__entry(
		__string(fence_name, name)
		__field(unsigned int, cmdbatch_context_id)
	),
	TP_fast_assign(
		__entry->cmdbatch_context_id = cmdbatch->context->id;
		__assign_str(fence_name, name);
	),
	TP_printk("ctx=%d fence=%s",
		__entry->cmdbatch_context_id, __get_str(fence_name))
);

DEFINE_EVENT(syncpoint_fence_template, syncpoint_fence,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, char *name),
	TP_ARGS(cmdbatch, name)
);

DEFINE_EVENT(syncpoint_fence_template, syncpoint_fence_expire,
	TP_PROTO(struct kgsl_cmdbatch *cmdbatch, char *name),
	TP_ARGS(cmdbatch, name)
);

TRACE_EVENT(kgsl_msg,
	TP_PROTO(const char *msg),
	TP_ARGS(msg),
	TP_STRUCT__entry(
		__string(msg, msg)
	),
	TP_fast_assign(
		__assign_str(msg, msg);
	),
	TP_printk(
		"%s", __get_str(msg)
	)
);


#endif /* _KGSL_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
