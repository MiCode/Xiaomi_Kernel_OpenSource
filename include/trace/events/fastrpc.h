/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM fastrpc

#if !defined(_TRACE_FASTRPC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_FASTRPC_H
#include <linux/tracepoint.h>

TRACE_EVENT(fastrpc_rpmsg_send,

	TP_PROTO(int cid, uint64_t smq_ctx,
		uint64_t ctx, uint32_t handle,
		uint32_t sc, uint64_t addr, uint64_t size),

	TP_ARGS(cid, smq_ctx, ctx, handle, sc, addr, size),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(u64, smq_ctx)
		__field(u64, ctx)
		__field(u32, handle)
		__field(u32, sc)
		__field(u64, addr)
		__field(u64, size)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->smq_ctx = smq_ctx;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
		__entry->addr = addr;
		__entry->size = size;
	),

	TP_printk("to cid %d: smq_ctx 0x%llx, ctx 0x%llx, handle 0x%x, sc 0x%x, addr 0x%llx, size %llu",
		__entry->cid, __entry->smq_ctx, __entry->ctx, __entry->handle,
		__entry->sc, __entry->addr, __entry->size)
);

TRACE_EVENT(fastrpc_rpmsg_response,

	TP_PROTO(int cid, uint64_t ctx, int retval,
		uint32_t rsp_flags, uint32_t early_wake_time),

	TP_ARGS(cid, ctx, retval, rsp_flags, early_wake_time),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(u64, ctx)
		__field(int, retval)
		__field(u32, rsp_flags)
		__field(u32, early_wake_time)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->ctx = ctx;
		__entry->retval = retval;
		__entry->rsp_flags = rsp_flags;
		__entry->early_wake_time = early_wake_time;
	),

	TP_printk("from cid %d: ctx 0x%llx, retval 0x%x, rsp_flags %u, early_wake_time %u",
		__entry->cid, __entry->ctx, __entry->retval,
		__entry->rsp_flags, __entry->early_wake_time)
);

TRACE_EVENT(fastrpc_context_interrupt,

	TP_PROTO(int cid, uint64_t smq_ctx, uint64_t ctx,
		uint32_t handle, uint32_t sc),

	TP_ARGS(cid, smq_ctx, ctx, handle, sc),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(u64, smq_ctx)
		__field(u64, ctx)
		__field(u32, handle)
		__field(u32, sc)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->smq_ctx = smq_ctx;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
	),

	TP_printk("to cid %d: smq_ctx 0x%llx, ctx 0x%llx, handle 0x%x, sc 0x%x",
		__entry->cid, __entry->smq_ctx,
		__entry->ctx, __entry->handle, __entry->sc)
);

TRACE_EVENT(fastrpc_context_restore,

	TP_PROTO(int cid, uint64_t smq_ctx, uint64_t ctx,
		uint32_t handle, uint32_t sc),

	TP_ARGS(cid, smq_ctx, ctx, handle, sc),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(u64, smq_ctx)
		__field(u64, ctx)
		__field(u32, handle)
		__field(u32, sc)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->smq_ctx = smq_ctx;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
	),

	TP_printk("for cid %d: smq_ctx 0x%llx, ctx 0x%llx, handle 0x%x, sc 0x%x",
		__entry->cid, __entry->smq_ctx,
		__entry->ctx, __entry->handle, __entry->sc)
);

TRACE_EVENT(fastrpc_dma_map,

	TP_PROTO(int cid, int fd, uint64_t phys, size_t size,
		size_t len, unsigned int attr, int mflags),

	TP_ARGS(cid, fd, phys, size, len, attr, mflags),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(int, fd)
		__field(u64, phys)
		__field(size_t, size)
		__field(size_t, len)
		__field(unsigned int, attr)
		__field(int, mflags)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->fd = fd;
		__entry->phys = phys;
		__entry->size = size;
		__entry->len = len;
		__entry->attr = attr;
		__entry->mflags = mflags;
	),

	TP_printk("cid %d, fd %d, phys 0x%llx, size %zu (len %zu), attr 0x%x, flags 0x%x",
		__entry->cid, __entry->fd, __entry->phys, __entry->size,
		__entry->len, __entry->attr, __entry->mflags)
);

TRACE_EVENT(fastrpc_dma_unmap,

	TP_PROTO(int cid, uint64_t phys, size_t size),

	TP_ARGS(cid, phys, size),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(u64, phys)
		__field(size_t, size)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->phys = phys;
		__entry->size = size;
	),

	TP_printk("cid %d, phys 0x%llx, size %zu",
		__entry->cid, __entry->phys, __entry->size)
);

TRACE_EVENT(fastrpc_dma_alloc,

	TP_PROTO(int cid, uint64_t phys, size_t size,
		unsigned long attr, int mflags),

	TP_ARGS(cid, phys, size, attr, mflags),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(u64, phys)
		__field(size_t, size)
		__field(unsigned long, attr)
		__field(int, mflags)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->phys = phys;
		__entry->size = size;
		__entry->attr = attr;
		__entry->mflags = mflags;
	),

	TP_printk("cid %d, phys 0x%llx, size %zu, attr 0x%lx, flags 0x%x",
		__entry->cid, __entry->phys, __entry->size,
		__entry->attr, __entry->mflags)
);

TRACE_EVENT(fastrpc_dma_free,

	TP_PROTO(int cid, uint64_t phys, size_t size),

	TP_ARGS(cid, phys, size),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(u64, phys)
		__field(size_t, size)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->phys = phys;
		__entry->size = size;
	),

	TP_printk("cid %d, phys 0x%llx, size %zu",
		__entry->cid, __entry->phys, __entry->size)
);

TRACE_EVENT(fastrpc_context_complete,

	TP_PROTO(int cid, uint64_t smq_ctx, int retval,
		uint64_t ctx, uint32_t handle, uint32_t sc),

	TP_ARGS(cid, smq_ctx, retval, ctx, handle, sc),

	TP_STRUCT__entry(
		__field(int, cid)
		__field(u64, smq_ctx)
		__field(int, retval)
		__field(u64, ctx)
		__field(u32, handle)
		__field(u32, sc)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->smq_ctx = smq_ctx;
		__entry->retval = retval;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
	),

	TP_printk("from cid %d: smq_ctx 0x%llx, retval 0x%x, ctx 0x%llx, handle 0x%x, sc 0x%x",
		__entry->cid, __entry->smq_ctx, __entry->retval,
		__entry->ctx, __entry->handle, __entry->sc)
);

TRACE_EVENT(fastrpc_context_alloc,

	TP_PROTO(uint64_t smq_ctx, uint64_t ctx,
		uint32_t handle, uint32_t sc),

	TP_ARGS(smq_ctx, ctx, handle, sc),

	TP_STRUCT__entry(
		__field(u64, smq_ctx)
		__field(u64, ctx)
		__field(u32, handle)
		__field(u32, sc)
	),

	TP_fast_assign(
		__entry->smq_ctx = smq_ctx;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
	),

	TP_printk("for: smq_ctx 0x%llx, ctx 0x%llx, handle 0x%x, sc 0x%x",
		__entry->smq_ctx, __entry->ctx, __entry->handle, __entry->sc)
);

TRACE_EVENT(fastrpc_context_free,

	TP_PROTO(uint64_t smq_ctx, uint64_t ctx,
		uint32_t handle, uint32_t sc),

	TP_ARGS(smq_ctx, ctx, handle, sc),

	TP_STRUCT__entry(
		__field(u64, smq_ctx)
		__field(u64, ctx)
		__field(u32, handle)
		__field(u32, sc)
	),

	TP_fast_assign(
		__entry->smq_ctx = smq_ctx;
		__entry->ctx = ctx;
		__entry->handle = handle;
		__entry->sc = sc;
	),

	TP_printk("for: smq_ctx 0x%llx, ctx 0x%llx, handle 0x%x, sc 0x%x",
		__entry->smq_ctx, __entry->ctx, __entry->handle, __entry->sc)
);

TRACE_EVENT(fastrpc_perf_counters,

	TP_PROTO(uint32_t handle, uint32_t sc,
		uint64_t count, uint64_t flush, uint64_t map,
		uint64_t copy, uint64_t link, uint64_t getargs,
		uint64_t putargs, uint64_t invargs, uint64_t invoke,
		uint64_t tid),

	TP_ARGS(handle, sc, count, flush, map, copy, link, getargs,
		putargs, invargs, invoke, tid),

	TP_STRUCT__entry(
		__field(u32, handle)
		__field(u32, sc)
		__field(u64, count)
		__field(u64, flush)
		__field(u64, map)
		__field(u64, copy)
		__field(u64, link)
		__field(u64, getargs)
		__field(u64, putargs)
		__field(u64, invargs)
		__field(u64, invoke)
		__field(u64, tid)
	),

	TP_fast_assign(
		__entry->handle = handle;
		__entry->sc = sc;
		__entry->count = count;
		__entry->flush = flush;
		__entry->map = map;
		__entry->copy = copy;
		__entry->link = link;
		__entry->getargs = getargs;
		__entry->putargs = putargs;
		__entry->invargs = invargs;
		__entry->invoke = invoke;
		__entry->tid = tid;
	),

	TP_printk("for: handle 0x%x, sc 0x%x, count %lld, flush %lld ns, map %lld ns, copy %lld ns, link %lld ns, getargs %lld ns, putargs %lld ns, invargs %lld ns, invoke %lld ns, tid %lld",
		__entry->handle, __entry->sc, __entry->count,
		__entry->flush, __entry->map, __entry->copy, __entry->link,
		__entry->getargs, __entry->putargs, __entry->invargs,
		__entry->invoke, __entry->tid)
);

TRACE_EVENT(fastrpc_msg,
	TP_PROTO(const char *message),
	TP_ARGS(message),
	TP_STRUCT__entry(__string(buf, message)),
	TP_fast_assign(
		__assign_str(buf, message);
	),
	TP_printk(" %s", __get_str(buf))
);

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
