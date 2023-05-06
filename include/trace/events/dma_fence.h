/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM dma_fence

#if !defined(_TRACE_DMA_FENCE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DMA_FENCE_H

#include <linux/tracepoint.h>

struct dma_fence;

DECLARE_EVENT_CLASS(dma_fence,

	TP_PROTO(struct dma_fence *fence),

	TP_ARGS(fence),

	TP_STRUCT__entry(
		__string(driver, fence->ops->get_driver_name(fence))
		__string(timeline, fence->ops->get_timeline_name(fence))
		__field(unsigned int, context)
		__field(unsigned int, seqno)
	),

	TP_fast_assign(
		__assign_str(driver, fence->ops->get_driver_name(fence))
		__assign_str(timeline, fence->ops->get_timeline_name(fence))
		__entry->context = fence->context;
		__entry->seqno = fence->seqno;
	),

	TP_printk("driver=%s timeline=%s context=%u seqno=%u",
		  __get_str(driver), __get_str(timeline), __entry->context,
		  __entry->seqno)
);

DEFINE_EVENT(dma_fence, dma_fence_emit,

	TP_PROTO(struct dma_fence *fence),

	TP_ARGS(fence)
);

DEFINE_EVENT(dma_fence, dma_fence_init,

	TP_PROTO(struct dma_fence *fence),

	TP_ARGS(fence)
);

DEFINE_EVENT(dma_fence, dma_fence_destroy,

	TP_PROTO(struct dma_fence *fence),

	TP_ARGS(fence)
);

DEFINE_EVENT(dma_fence, dma_fence_enable_signal,

	TP_PROTO(struct dma_fence *fence),

	TP_ARGS(fence)
);

DEFINE_EVENT(dma_fence, dma_fence_signaled,

	TP_PROTO(struct dma_fence *fence),

	TP_ARGS(fence)
);

DEFINE_EVENT(dma_fence, dma_fence_wait_start,

	TP_PROTO(struct dma_fence *fence),

	TP_ARGS(fence)
);

DEFINE_EVENT(dma_fence, dma_fence_wait_end,

	TP_PROTO(struct dma_fence *fence),

	TP_ARGS(fence)
);


//Add By XiaoMi
DECLARE_EVENT_CLASS(dma_heap_class,
	TP_PROTO(size_t len, unsigned int fd_flags,
			  unsigned int heap_flags),
	TP_ARGS(len, fd_flags, heap_flags),
	TP_STRUCT__entry(
		__field(size_t, len)
		__field(unsigned int, fd_flags)
		__field(unsigned int, heap_flags)
	),
	TP_fast_assign(
		__entry->len = len;
		__entry->fd_flags = fd_flags;
		__entry->heap_flags = heap_flags;
	),

	TP_printk("len %zu fd_flags %u heap_flags %x\n",
		__entry->len,
		__entry->fd_flags,
		__entry->heap_flags)

);


DEFINE_EVENT(dma_heap_class, dma_heap_buffer_alloc,
	TP_PROTO(size_t len, unsigned int fd_flags,
			  unsigned int heap_flags),

	TP_ARGS(len, fd_flags, heap_flags)
);
//Add By XiaoMi End

#endif /*  _TRACE_DMA_FENCE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
