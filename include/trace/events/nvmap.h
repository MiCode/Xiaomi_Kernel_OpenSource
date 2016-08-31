/*
 * include/trace/events/nvmap.h
 *
 * NvMap event logging to ftrace.
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM nvmap

#if !defined(_TRACE_NVMAP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVMAP_H

#include <linux/nvmap.h>
#include <linux/dma-buf.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(nvmap,
	TP_PROTO(struct nvmap_client *client, const char *name),
	TP_ARGS(client, name),
	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__string(sname, name)
	),
	TP_fast_assign(
		__entry->client = client;
		__assign_str(sname, name)
	),
	TP_printk("client=%p, name=%s",
		__entry->client, __get_str(sname))
);

DEFINE_EVENT(nvmap, nvmap_open,
	TP_PROTO(struct nvmap_client *client, const char *name),
	TP_ARGS(client, name)
);

DEFINE_EVENT(nvmap, nvmap_release,
	TP_PROTO(struct nvmap_client *client, const char *name),
	TP_ARGS(client, name)
);

TRACE_EVENT(nvmap_create_handle,
	TP_PROTO(struct nvmap_client *client,
		 const char *name,
		 struct nvmap_handle *h,
		 u32 size,
		 struct nvmap_handle_ref *ref
	),

	TP_ARGS(client, name, h, size, ref),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__string(sname, name)
		__field(struct nvmap_handle *, h)
		__field(u32, size)
		__field(struct nvmap_handle_ref *, ref)
	),

	TP_fast_assign(
		__entry->client = client;
		__assign_str(sname, name)
		__entry->h = h;
		__entry->size = size;
		__entry->ref = ref;
	),

	TP_printk("client=%p, name=%s, handle=%p, size=%d, ref=%p",
		__entry->client, __get_str(sname),
		__entry->h, __entry->size, __entry->ref)
);

TRACE_EVENT(nvmap_alloc_handle_id,
	TP_PROTO(struct nvmap_client *client,
		 unsigned long handle_id,
		 u32 heap_mask,
		 u32 align,
		 u32 flags
	),

	TP_ARGS(client, handle_id, heap_mask, align, flags),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__field(unsigned long, handle_id)
		__field(u32, heap_mask)
		__field(u32, align)
		__field(u32, flags)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->handle_id = handle_id;
		__entry->heap_mask = heap_mask;
		__entry->align = align;
		__entry->flags = flags;
	),

	TP_printk("client=%p, id=0x%lx, heap_mask=0x%x, align=%d, flags=0x%x",
		__entry->client, __entry->handle_id, __entry->heap_mask,
		__entry->align, __entry->flags)
);

TRACE_EVENT(nvmap_free_handle_id,
	TP_PROTO(struct nvmap_client *client,
		 unsigned long handle_id
	),

	TP_ARGS(client, handle_id),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__field(unsigned long, handle_id)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->handle_id = handle_id;
	),

	TP_printk("client=%p, id=0x%lx",
		__entry->client, __entry->handle_id)
);

TRACE_EVENT(nvmap_duplicate_handle_id,
	TP_PROTO(struct nvmap_client *client,
		 unsigned long handle_id,
		 struct nvmap_handle_ref *ref
	),

	TP_ARGS(client, handle_id, ref),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__field(unsigned long, handle_id)
		__field(struct nvmap_handle_ref *, ref)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->handle_id = handle_id;
		__entry->ref = ref;
	),

	TP_printk("client=%p, id=0x%lx, ref=%p",
		__entry->client, __entry->handle_id, __entry->ref)
);

TRACE_EVENT(cache_maint,
	TP_PROTO(struct nvmap_client *client,
		 struct nvmap_handle *h,
		 unsigned long start,
		 unsigned long end,
		 u32 op
	),

	TP_ARGS(client, h, start, end, op),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__field(struct nvmap_handle *, h)
		__field(unsigned long, start)
		__field(unsigned long, end)
		__field(u32, op)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->h = h;
		__entry->start = start;
		__entry->end = end;
		__entry->op = op;
	),

	TP_printk("client=%p, h=%p, start=0x%lx, end=0x%lx, op=%d",
		__entry->client, __entry->h, __entry->start,
		__entry->end, __entry->op)
);

TRACE_EVENT(nvmap_map_into_caller_ptr,
	TP_PROTO(struct nvmap_client *client,
		 struct nvmap_handle *h,
		 u32 offset,
		 u32 length,
		 u32 flags
	),

	TP_ARGS(client, h, offset, length, flags),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__field(struct nvmap_handle *, h)
		__field(u32, offset)
		__field(u32, length)
		__field(u32, flags)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->h = h;
		__entry->offset = offset;
		__entry->length = length;
		__entry->flags = flags;
	),

	TP_printk("client=%p, h=%p, offset=%d, length=%d, flags=0x%x",
		__entry->client, __entry->h, __entry->offset,
		__entry->length, __entry->flags)
);

TRACE_EVENT(nvmap_ioctl_rw_handle,
	TP_PROTO(struct nvmap_client *client,
		 struct nvmap_handle *h,
		 u32 is_read,
		 u32 offset,
		 unsigned long addr,
		 u32 mem_stride,
		 u32 user_stride,
		 u32 elem_size,
		 u32 count
	),

	TP_ARGS(client, h, is_read, offset, addr, mem_stride,
		user_stride, elem_size, count),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__field(struct nvmap_handle *, h)
		__field(u32, is_read)
		__field(u32, offset)
		__field(unsigned long, addr)
		__field(u32, mem_stride)
		__field(u32, user_stride)
		__field(u32, elem_size)
		__field(u32, count)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->h = h;
		__entry->is_read = is_read;
		__entry->offset = offset;
		__entry->addr = addr;
		__entry->mem_stride = mem_stride;
		__entry->user_stride = user_stride;
		__entry->elem_size = elem_size;
		__entry->count = count;
	),

	TP_printk("client=%p, h=%p, is_read=%d, offset=%d, addr=0x%lx,"
		"mem_stride=%d, user_stride=%d, elem_size=%d, count=%d",
		__entry->client, __entry->h, __entry->is_read, __entry->offset,
		__entry->addr, __entry->mem_stride, __entry->user_stride,
		__entry->elem_size, __entry->count)
);

TRACE_EVENT(nvmap_ioctl_pinop,
	TP_PROTO(struct nvmap_client *client,
		 u32 is_pin,
		 u32 count,
		 unsigned long *ids
	),

	TP_ARGS(client, is_pin, count, ids),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__field(u32, is_pin)
		__field(u32, count)
		__field(unsigned long *, ids)
		__dynamic_array(unsigned long, ids, count)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->is_pin = is_pin;
		__entry->count = count;
		__entry->ids = ids;
		memcpy(__get_dynamic_array(ids), ids,
		    sizeof(unsigned long) * count);
	),

	TP_printk("client=%p, is_pin=%d, count=%d, ids=[%s]",
		__entry->client, __entry->is_pin, __entry->count,
		__print_hex(__get_dynamic_array(ids), __entry->ids ?
			    sizeof(unsigned long) * __entry->count : 0))
);

DECLARE_EVENT_CLASS(pin_unpin,
	TP_PROTO(struct nvmap_client *client,
		 const char *name,
		 struct nvmap_handle *h,
		 u32 pin_count
	),

	TP_ARGS(client, name, h, pin_count),

	TP_STRUCT__entry(
		__field(struct nvmap_client *, client)
		__string(sname, name)
		__field(struct nvmap_handle *, h)
		__field(u32, pin_count)
	),

	TP_fast_assign(
		__entry->client = client;
		__assign_str(sname, name)
		__entry->h = h;
		__entry->pin_count = pin_count;
	),

	TP_printk("client=%p, name=%s, h=%p, pin_count=%d",
		__entry->client, __get_str(sname),
		__entry->h, __entry->pin_count)
);

DEFINE_EVENT(pin_unpin, nvmap_pin,
	TP_PROTO(struct nvmap_client *client,
		 const char *name,
		 struct nvmap_handle *h,
		 u32 pin_count
	),
	TP_ARGS(client, name, h, pin_count)
);

DEFINE_EVENT(pin_unpin, nvmap_unpin,
	TP_PROTO(struct nvmap_client *client,
		 const char *name,
		 struct nvmap_handle *h,
		 u32 pin_count
	),
	TP_ARGS(client, name, h, pin_count)
);

DEFINE_EVENT(pin_unpin, handle_unpin_error,
	TP_PROTO(struct nvmap_client *client,
		 const char *name,
		 struct nvmap_handle *h,
		 u32 pin_count
	),
	TP_ARGS(client, name, h, pin_count)
);

/*
 * Nvmap dmabuf events
 */
DECLARE_EVENT_CLASS(nvmap_dmabuf_2,
	TP_PROTO(struct dma_buf *dbuf,
		 struct device *dev
	),

	TP_ARGS(dbuf, dev),

	TP_STRUCT__entry(
		__field(struct dma_buf *, dbuf)
		__field(struct device *, dev)
	),

	TP_fast_assign(
		__entry->dbuf = dbuf;
		__entry->dev = dev;
	),

	TP_printk("dmabuf=%p, device=%s",
		__entry->dbuf, dev_name(__entry->dev)
	)
);

DECLARE_EVENT_CLASS(nvmap_dmabuf_1,
	TP_PROTO(struct dma_buf *dbuf),

	TP_ARGS(dbuf),

	TP_STRUCT__entry(
		__field(struct dma_buf *, dbuf)
	),

	TP_fast_assign(
		__entry->dbuf = dbuf;
	),

	TP_printk("dmabuf=%p",
		__entry->dbuf
	)
);

DEFINE_EVENT(nvmap_dmabuf_2, nvmap_dmabuf_attach,
	TP_PROTO(struct dma_buf *dbuf,
		 struct device *dev
	),
	TP_ARGS(dbuf, dev)
);

DEFINE_EVENT(nvmap_dmabuf_2, nvmap_dmabuf_detach,
	TP_PROTO(struct dma_buf *dbuf,
		 struct device *dev
	),
	TP_ARGS(dbuf, dev)
);

DEFINE_EVENT(nvmap_dmabuf_2, nvmap_dmabuf_map_dma_buf,
	TP_PROTO(struct dma_buf *dbuf,
		 struct device *dev
	),
	TP_ARGS(dbuf, dev)
);

DEFINE_EVENT(nvmap_dmabuf_2, nvmap_dmabuf_unmap_dma_buf,
	TP_PROTO(struct dma_buf *dbuf,
		 struct device *dev
	),
	TP_ARGS(dbuf, dev)
);

DEFINE_EVENT(nvmap_dmabuf_1, nvmap_dmabuf_mmap,
	TP_PROTO(struct dma_buf *dbuf),
	TP_ARGS(dbuf)
);

DEFINE_EVENT(nvmap_dmabuf_1, nvmap_dmabuf_vmap,
	TP_PROTO(struct dma_buf *dbuf),
	TP_ARGS(dbuf)
);

DEFINE_EVENT(nvmap_dmabuf_1, nvmap_dmabuf_vunmap,
	TP_PROTO(struct dma_buf *dbuf),
	TP_ARGS(dbuf)
);

DEFINE_EVENT(nvmap_dmabuf_1, nvmap_dmabuf_kmap,
	TP_PROTO(struct dma_buf *dbuf),
	TP_ARGS(dbuf)
);

DEFINE_EVENT(nvmap_dmabuf_1, nvmap_dmabuf_kunmap,
	TP_PROTO(struct dma_buf *dbuf),
	TP_ARGS(dbuf)
);

DECLARE_EVENT_CLASS(nvmap_dmabuf_cpu_access,
	TP_PROTO(struct dma_buf *dbuf,
		 size_t start,
		 size_t len),

	TP_ARGS(dbuf, start, len),

	TP_STRUCT__entry(
		__field(struct dma_buf *, dbuf)
		__field(size_t, start)
		__field(size_t, len)
	),

	TP_fast_assign(
		__entry->dbuf = dbuf;
		__entry->start = start;
		__entry->len = len;
	),

	TP_printk("dmabuf=%p, start=%d len=%d",
		  __entry->dbuf, __entry->start, __entry->len
	)
);

DEFINE_EVENT(nvmap_dmabuf_cpu_access, nvmap_dmabuf_begin_cpu_access,
	TP_PROTO(struct dma_buf *dbuf,
		 u32 start,
		 u32 len),
	TP_ARGS(dbuf, start, len)
);

DEFINE_EVENT(nvmap_dmabuf_cpu_access, nvmap_dmabuf_end_cpu_access,
	TP_PROTO(struct dma_buf *dbuf,
		 u32 start,
		 u32 len),
	TP_ARGS(dbuf, start, len)
);

DECLARE_EVENT_CLASS(nvmap_dmabuf_make_release,
	TP_PROTO(const char *cli,
		 struct nvmap_handle *h,
		 struct dma_buf *dbuf
	),

	TP_ARGS(cli, h, dbuf),

	TP_STRUCT__entry(
		__field(const char *, cli)
		__field(struct nvmap_handle *, h)
		__field(struct dma_buf *, dbuf)
	),

	TP_fast_assign(
		__entry->cli = cli;
		__entry->h = h;
		__entry->dbuf = dbuf;
	),

	TP_printk("cli=%s handle=%p dmabuf=%p",
		  __entry->cli, __entry->h, __entry->dbuf
	)
);

DEFINE_EVENT(nvmap_dmabuf_make_release, nvmap_make_dmabuf,
	TP_PROTO(const char *cli,
		 struct nvmap_handle *h,
		 struct dma_buf *dbuf
	),
	TP_ARGS(cli, h, dbuf)
);

DEFINE_EVENT(nvmap_dmabuf_make_release, nvmap_dmabuf_release,
	TP_PROTO(const char *cli,
		 struct nvmap_handle *h,
		 struct dma_buf *dbuf
	),
	TP_ARGS(cli, h, dbuf)
);

#endif /* _TRACE_NVMAP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
