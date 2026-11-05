/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM dmabuf

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DMABUF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DMABUF_H

#include <trace/hooks/vendor_hooks.h>

struct dma_buf;

DECLARE_HOOK(android_vh_dmabuf_page_pool_free_bypass,
		TP_PROTO(struct page *page, bool *bypass),
		TP_ARGS(page, bypass));
DECLARE_HOOK(android_vh_dma_heap_buffer_alloc_start,
		TP_PROTO(const char *name, size_t len,
			u32 fd_flags, u64 heap_flags),
		TP_ARGS(name, len, fd_flags, heap_flags));
DECLARE_HOOK(android_vh_dma_heap_buffer_alloc_end,
		TP_PROTO(const char *name, size_t len),
		TP_ARGS(name, len));
struct dma_buf_sysfs_entry;
DECLARE_RESTRICTED_HOOK(android_rvh_dma_buf_stats_teardown,
	TP_PROTO(struct dma_buf_sysfs_entry *sysfs_entry, bool *skip_sysfs_release),
	TP_ARGS(sysfs_entry, skip_sysfs_release), 1);
DECLARE_HOOK(android_vh_dma_heap_buffer_alloc_lat_start,
	TP_PROTO(unsigned long long *stime),
	TP_ARGS(stime));
DECLARE_HOOK(android_vh_dma_heap_buffer_alloc_lat_end,
	TP_PROTO(unsigned long long stime, size_t len, struct dma_buf *dma_buf),
	TP_ARGS(stime, len, dma_buf));
#endif /* _TRACE_HOOK_DMABUF_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

