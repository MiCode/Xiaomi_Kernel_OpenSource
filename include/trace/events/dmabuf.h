/* SPDX-License-Identifier: GPL-2.0 */
/*
 * dma-buf trace points
 *
 * Copyright (C) 2020 panlinlin <panlinlin3@xiaomi.com>
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM dmabuf

#if !defined(_TRACE_DMABUF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DMABUF_H

#include <linux/tracepoint.h>
#include <linux/dma-buf.h>

#define EXP_NAME_MAX_LEN 128
#define DMABUF_NAME_MAX_LEN 64
#define GET_NAME_LEN(max, str)  max > strlen(str) + 1 ? strlen(str) + 1 : max
TRACE_EVENT(dma_buf_export,

	TP_PROTO(const struct dma_buf_export_info *exp_info, unsigned int pid),

	TP_ARGS(exp_info, pid),

	TP_STRUCT__entry(
		__array(char, expinfo_name, EXP_NAME_MAX_LEN)
		__field(size_t, size)
		__field(unsigned int, pid)
	),

	TP_fast_assign(
		memcpy(__entry->expinfo_name, exp_info->exp_name,
			GET_NAME_LEN(EXP_NAME_MAX_LEN, exp_info->exp_name));
		__entry->size = exp_info->size;
		__entry->pid = pid
	),

	TP_printk("expinfo_name=%s size=%08zu pid=%u",
		  __entry->expinfo_name, __entry->size, __entry->pid)
);

TRACE_EVENT(dma_buf_map_attachment,

	TP_PROTO(struct dma_buf_attachment *attach, enum dma_data_direction direction,
		 unsigned int pid),

	TP_ARGS(attach, direction, pid),

	TP_STRUCT__entry(
		__array(char, dmabuf_name, DMABUF_NAME_MAX_LEN)
		__field(size_t, size)
		__field(unsigned int, pid)
	),

	TP_fast_assign(
		memcpy(__entry->dmabuf_name, attach->dmabuf->name,
			GET_NAME_LEN(DMABUF_NAME_MAX_LEN, attach->dmabuf->name));
		__entry->size = attach->dmabuf->size;
		__entry->pid = pid
	),

	TP_printk("attach name=%s, size=%08zu, pid=%u",
		  __entry->dmabuf_name, __entry->size, __entry->pid)
);

TRACE_EVENT(dma_buf_unmap_attachment,

	TP_PROTO(struct dma_buf_attachment *attach, struct sg_table *sg_table,
				enum dma_data_direction direction, unsigned int pid),

	TP_ARGS(attach, sg_table, direction, pid),

	TP_STRUCT__entry(
		__array(char, dmabuf_name, DMABUF_NAME_MAX_LEN)
		__field(size_t, size)
		__field(unsigned int, pid)
	),

	TP_fast_assign(
		memcpy(__entry->dmabuf_name, attach->dmabuf->name,
			GET_NAME_LEN(DMABUF_NAME_MAX_LEN, attach->dmabuf->name));
		__entry->size = attach->dmabuf->size;
		__entry->pid = pid
	),

	TP_printk("attach name=%s, size=%08zu, pid=%u",
		  __entry->dmabuf_name, __entry->size, __entry->pid)
);

TRACE_EVENT(dma_buf_vmap,

	TP_PROTO(struct dma_buf *dmabuf, unsigned int pid),

	TP_ARGS(dmabuf, pid),

	TP_STRUCT__entry(
		__array(char, expinfo_name, EXP_NAME_MAX_LEN)
		__array(char, dmabuf_name, DMABUF_NAME_MAX_LEN)
		__field(size_t, size)
		__field(unsigned int, pid)
	),

	TP_fast_assign(
		memcpy(__entry->expinfo_name, dmabuf->exp_name,
			GET_NAME_LEN(EXP_NAME_MAX_LEN, dmabuf->exp_name));
		memcpy(__entry->dmabuf_name, dmabuf->name,
		        GET_NAME_LEN(DMABUF_NAME_MAX_LEN, dmabuf->name));
		__entry->size = dmabuf->size;
		__entry->pid = pid
	),

	TP_printk("dma_buf_vmap exporter name:%s , buffer name=%s, size=%08zu, pid=%u",
                  __entry->expinfo_name, __entry->dmabuf_name, __entry->size, __entry->pid)
);

TRACE_EVENT(dma_buf_vunmap,

	TP_PROTO(struct dma_buf *dmabuf, void *vaddr, unsigned int pid),

	TP_ARGS(dmabuf, vaddr, pid),

	TP_STRUCT__entry(
		__array(char, expinfo_name, EXP_NAME_MAX_LEN)
		__array(char, dmabuf_name, DMABUF_NAME_MAX_LEN)
		__field(size_t, size)
		__field(unsigned int, pid)
	),

	TP_fast_assign(
		memcpy(__entry->expinfo_name, dmabuf->exp_name,
			GET_NAME_LEN(EXP_NAME_MAX_LEN, dmabuf->exp_name));
		memcpy(__entry->dmabuf_name, dmabuf->name,
			GET_NAME_LEN(DMABUF_NAME_MAX_LEN, dmabuf->name));
		__entry->size = dmabuf->size;
		__entry->pid = pid
	),

	TP_printk("dma_buf_vunmap exporter name:%s , buffer name=%s, size=%08zu, pid=%u",
                  __entry->expinfo_name, __entry->dmabuf_name, __entry->size, __entry->pid)
);

TRACE_EVENT(dma_buf_put,

	TP_PROTO(struct dma_buf *dmabuf, unsigned int pid),

	TP_ARGS(dmabuf, pid),

	TP_STRUCT__entry(
		__array(char, expinfo_name, EXP_NAME_MAX_LEN)
		__array(char, dmabuf_name, DMABUF_NAME_MAX_LEN)
		__field(size_t, size)
		__field(unsigned int, pid)
	),

	TP_fast_assign(
		memcpy(__entry->expinfo_name, dmabuf->exp_name,
			GET_NAME_LEN(EXP_NAME_MAX_LEN, dmabuf->exp_name));
		memcpy(__entry->dmabuf_name, dmabuf->name,
			GET_NAME_LEN(DMABUF_NAME_MAX_LEN, dmabuf->name));
		__entry->size = dmabuf->size;
		__entry->pid = pid
	),

	TP_printk("dma_buf_put exporter name:%s , buffer name=%s, size=%08zu, pid=%u",
		  __entry->expinfo_name, __entry->dmabuf_name, __entry->size, __entry->pid)
);

TRACE_EVENT(dma_buf_mmap_internal,

	TP_PROTO(struct file *file, struct vm_area_struct *vma, unsigned int pid),

	TP_ARGS(file, vma, pid),

	TP_STRUCT__entry(
		__field(unsigned long, vm_start)
		__field(unsigned long, vm_end)
		__field(unsigned int, pid)
	),

	TP_fast_assign(
		__entry->vm_start = vma->vm_start;
		__entry->vm_end = vma->vm_end;
		__entry->pid = pid
	),

	TP_printk("dma_buf_mmap_internal: vma start:%lu, vma end:%lu, pid:%u",
			__entry->vm_start,
			__entry->vm_end,
			__entry->pid)
);

TRACE_EVENT(dma_buf_release,

	TP_PROTO(struct inode *inode, struct file *file, unsigned int pid),

	TP_ARGS(inode, file, pid),

	TP_STRUCT__entry(
		__field(dev_t,dev)
		__field(ino_t,ino)
		__field(uid_t,uid)
		__field(gid_t,gid)
		__field(__u64,blocks)
		__field(__u16,mode)
		__field(unsigned int, pid)
	),

	TP_fast_assign(
		__entry->dev	= inode->i_sb->s_dev;
		__entry->ino	= inode->i_ino;
		__entry->uid	= i_uid_read(inode);
		__entry->gid	= i_gid_read(inode);
		__entry->blocks	= inode->i_blocks;
		__entry->mode	= inode->i_mode;
		__entry->pid    = pid
	),

	TP_printk("dma_buf_release: dev %d,%d ino %lu mode 0%o uid %u gid %u blocks %llu pid %u",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  (unsigned long) __entry->ino, __entry->mode,
		  __entry->uid, __entry->gid, __entry->blocks, __entry->pid)
);

#endif /* _TRACE_DMABUF_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
