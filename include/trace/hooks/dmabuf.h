/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM dmabuf

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DMABUF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DMABUF_H

#include <trace/hooks/vendor_hooks.h>

#ifdef __GENKSYMS__
#include <linux/dma-buf.h>
#endif

struct dma_buf;

DECLARE_HOOK(android_vh_dma_buf_release,
		TP_PROTO(struct dma_buf *data),
		TP_ARGS(data));

struct dma_heap;
DECLARE_HOOK(android_vh_dmabuf_heap_flags_validation,
		TP_PROTO(struct dma_heap *heap, size_t len,
			unsigned int fd_flags, unsigned int heap_flags, bool *skip),
		TP_ARGS(heap, len, fd_flags, heap_flags, skip));

struct dma_buf_sysfs_entry;
DECLARE_RESTRICTED_HOOK(android_rvh_dma_buf_stats_teardown,
	TP_PROTO(struct dma_buf_sysfs_entry *sysfs_entry, bool *skip_sysfs_release),
	TP_ARGS(sysfs_entry, skip_sysfs_release), 1);
#endif /* _TRACE_HOOK_DMABUF_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

