/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
			int timestamp,
			int flags,
			int result,
			unsigned int type),

	TP_ARGS(device, drawctxt_id, cmdbatch, timestamp, flags,
		result, type),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, drawctxt_id)
		__field(unsigned int, ibdesc_addr)
		__field(unsigned int, numibs)
		__field(unsigned int, timestamp)
		__field(unsigned int, flags)
		__field(int, result)
		__field(unsigned int, drawctxt_type)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->drawctxt_id = drawctxt_id;
		__entry->ibdesc_addr = cmdbatch->ibdesc[0].gpuaddr;
		__entry->numibs = cmdbatch->ibcount;
		__entry->timestamp = timestamp;
		__entry->flags = flags;
		__entry->result = result;
		__entry->drawctxt_type = type;
	),

	TP_printk(
		"d_name=%s ctx=%u ib=0x%u numibs=%u ts=%u "
		"flags=0x%x(%s) result=%d type=%s",
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
		__entry->result,
		__print_symbolic(__entry->drawctxt_type,
			ADRENO_DRAWCTXT_TYPES)
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
		__field(unsigned int, gpuaddr)
		__field(unsigned int, size)
		__field(unsigned int, tgid)
		__array(char, usage, 16)
		__field(unsigned int, id)
		__field(unsigned int, flags)
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
		"gpuaddr=0x%08x size=%u tgid=%u usage=%s id=%u flags=0x%08x",
		__entry->gpuaddr, __entry->size, __entry->tgid,
		__entry->usage, __entry->id, __entry->flags
	)
);

TRACE_EVENT(kgsl_mem_mmap,

	TP_PROTO(struct kgsl_mem_entry *mem_entry),

	TP_ARGS(mem_entry),

	TP_STRUCT__entry(
		__field(unsigned long, useraddr)
		__field(unsigned int, gpuaddr)
		__field(unsigned int, size)
		__array(char, usage, 16)
		__field(unsigned int, id)
		__field(unsigned int, flags)
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
		"useraddr=0x%lx gpuaddr=0x%08x size=%u usage=%s id=%u"
		" flags=0x%08x",
		__entry->useraddr, __entry->gpuaddr, __entry->size,
		__entry->usage, __entry->id, __entry->flags
	)
);

TRACE_EVENT(kgsl_mem_unmapped_area_collision,

	TP_PROTO(struct kgsl_mem_entry *mem_entry,
		 unsigned long hint,
		 unsigned long len,
		 unsigned long addr),

	TP_ARGS(mem_entry, hint, len, addr),

	TP_STRUCT__entry(
		__field(unsigned int, id)
		__field(unsigned long, hint)
		__field(unsigned long, len)
		__field(unsigned long, addr)
	),

	TP_fast_assign(
		__entry->id = mem_entry->id;
		__entry->hint  = hint;
		__entry->len = len;
		__entry->addr = addr;
	),

	TP_printk(
		"id=%u hint=0x%lx len=%lu addr=0x%lx",
		__entry->id, __entry->hint, __entry->len, __entry->addr
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
		__field(unsigned int, tgid)
		__array(char, usage, 16)
		__field(unsigned int, id)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->fd = fd;
		__entry->type = mem_entry->memtype;
		__entry->tgid = mem_entry->priv->pid;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->id = mem_entry->id;
	),

	TP_printk(
		"gpuaddr=0x%08x size=%u type=%d fd=%d tgid=%u usage=%s id=%u",
		__entry->gpuaddr, __entry->size,
		__entry->type, __entry->fd, __entry->tgid,
		__entry->usage, __entry->id
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
		__field(unsigned int, tgid)
		__array(char, usage, 16)
		__field(unsigned int, id)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->type = mem_entry->memtype;
		__entry->tgid = mem_entry->priv->pid;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->id = mem_entry->id;
	),

	TP_printk(
		"gpuaddr=0x%08x size=%u type=%d tgid=%u usage=%s id=%u",
		__entry->gpuaddr, __entry->size, __entry->type,
		__entry->tgid, __entry->usage, __entry->id
	)
);

TRACE_EVENT(kgsl_mem_sync_cache,

	TP_PROTO(struct kgsl_mem_entry *mem_entry, unsigned int op),

	TP_ARGS(mem_entry, op),

	TP_STRUCT__entry(
		__field(unsigned int, gpuaddr)
		__field(unsigned int, size)
		__array(char, usage, 16)
		__field(unsigned int, tgid)
		__field(unsigned int, id)
		__field(unsigned int, op)
	),

	TP_fast_assign(
		__entry->gpuaddr = mem_entry->memdesc.gpuaddr;
		__entry->size = mem_entry->memdesc.size;
		__entry->tgid = mem_entry->priv->pid;
		__entry->id = mem_entry->id;
		kgsl_get_memory_usage(__entry->usage, sizeof(__entry->usage),
				     mem_entry->memdesc.flags);
		__entry->op = op;
	),

	TP_printk(
		"gpuaddr=0x%08x size=%u tgid=%u usage=%s id=%u op=%c%c",
		__entry->gpuaddr, __entry->size, __entry->tgid, __entry->usage,
		__entry->id,
		(__entry->op & KGSL_GPUMEM_CACHE_CLEAN) ? 'c' : '.',
		(__entry->op & KGSL_GPUMEM_CACHE_INV) ? 'i' : '.'
	)
);

TRACE_EVENT(kgsl_mem_sync_full_cache,

	TP_PROTO(unsigned int num_bufs, unsigned int bulk_size,
		unsigned int op),

	TP_ARGS(num_bufs, bulk_size, op),

	TP_STRUCT__entry(
		__field(unsigned int, num_bufs)
		__field(unsigned int, bulk_size)
		__field(unsigned int, op)
	),

	TP_fast_assign(
		__entry->num_bufs = num_bufs;
		__entry->bulk_size = bulk_size;
		__entry->op = op;
	),

	TP_printk(
		"num_bufs=%d bulk_size=%d op=%c%c",
		__entry->num_bufs, __entry->bulk_size,
		(__entry->op & KGSL_GPUMEM_CACHE_CLEAN) ? 'c' : '.',
		(__entry->op & KGSL_GPUMEM_CACHE_INV) ? 'i' : '.'
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
		__entry->type = mem_entry->memtype;
		__entry->curr_ts = curr_ts;
		__entry->free_ts = free_ts;
	),

	TP_printk(
		"d_name=%s gpuaddr=0x%08x size=%u type=%d usage=%s id=%u ctx=%u"
		" curr_ts=%u free_ts=%u",
		__get_str(device_name),
		__entry->gpuaddr,
		__entry->size,
		__entry->type,
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


TRACE_EVENT(kgsl_pwrstats,
	TP_PROTO(struct kgsl_device *device, s64 time,
		struct kgsl_power_stats *pstats),

	TP_ARGS(device, time, pstats),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(s64, total_time)
		__field(u64, busy_time)
		__field(u64, ram_time)
		__field(u64, ram_wait)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->total_time = time;
		__entry->busy_time = pstats->busy_time;
		__entry->ram_time = pstats->ram_time;
		__entry->ram_wait = pstats->ram_wait;
	),

	TP_printk(
		"d_name=%s total=%lld busy=%lld ram_time=%lld ram_wait=%lld",
		__get_str(device_name), __entry->total_time, __entry->busy_time,
		__entry->ram_time, __entry->ram_wait
	)
);


#endif /* _KGSL_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
