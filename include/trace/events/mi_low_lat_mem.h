#undef TRACE_SYSTEM
#define TRACE_SYSTEM mi_low_lat_mem

#if !defined(_TRACE_MI_LOW_LAT_MEM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MI_LOW_LAT_MEM_H

#include <linux/tracepoint.h>
#include <linux/page-flags.h>
#include <linux/page-flags-layout.h>
#include <trace/events/mmflags.h>

TRACE_EVENT(mi_smart_refill_pool_shrink,
	TP_PROTO(void *data, gfp_t flags, int count, int *nr_to_scan),

	TP_ARGS(data, flags, count, nr_to_scan),

	TP_STRUCT__entry(
		__field(void *, data)
		__field(gfp_t, flags)
		__field(int, count)
		__field(int *, nr_to_scan)
	),

	TP_fast_assign(
		__entry->data = data;
		__entry->flags = flags;
		__entry->count = count;
		__entry->nr_to_scan = nr_to_scan;
	),

	TP_printk("low_lat_mem: refill shrink, %p %d\n",
			__entry->data, __entry->count)
);

TRACE_EVENT(mi_smart_refill_deferred_free,
	TP_PROTO(void **data, int num_orders, struct page *page),

	TP_ARGS(data, num_orders, page),

	TP_STRUCT__entry(
		__field(void **, data)
		__field(int, num_orders)
		__field(struct page *, page)
	),

	TP_fast_assign(
		__entry->data = data;
		__entry->num_orders = num_orders;
		__entry->page = page;
	),

	TP_printk("low_lat_mem: refill deferred_free, %p\n",
			__entry->page)
);

TRACE_EVENT(mi_smart_refill_sg_free,
	TP_PROTO(void **data, int num_orders, struct page *page),

	TP_ARGS(data, num_orders, page),

	TP_STRUCT__entry(
		__field(void **, data)
		__field(int, num_orders)
		__field(struct page *, page)
	),

	TP_fast_assign(
		__entry->data = data;
		__entry->num_orders = num_orders;
		__entry->page = page;
	),

	TP_printk("low_lat_mem: refill sg_free, %p\n",
			__entry->page)
);

TRACE_EVENT(mi_smart_refill_alloc,
	TP_PROTO(void *data, struct page *page, int dmabuf_alloc_from_buddy),

	TP_ARGS(data, page, dmabuf_alloc_from_buddy),

	TP_STRUCT__entry(
		__field(void *, data)
		__field(struct page *, page)
		__field(int, dmabuf_alloc_from_buddy)
	),

	TP_fast_assign(
		__entry->data = data;
		__entry->page = page;
		__entry->dmabuf_alloc_from_buddy = dmabuf_alloc_from_buddy;
	),

	TP_printk("low_lat_mem: refill alloc, %p\n",
			__entry->data)
);

#if IS_ENABLED(CONFIG_SMART_REFILL_EXTEND_POOL_SUPPORT)
TRACE_EVENT(mi_smart_refill_extend_alloc,
	TP_PROTO(void *data, struct page *page, int dmabuf_alloc_from_buddy),

	TP_ARGS(data, page, dmabuf_alloc_from_buddy),

	TP_STRUCT__entry(
		__field(void *, data)
		__field(struct page *, page)
		__field(int, dmabuf_alloc_from_buddy)
	),

	TP_fast_assign(
		__entry->data = data;
		__entry->page = page;
		__entry->dmabuf_alloc_from_buddy = dmabuf_alloc_from_buddy;
	),

	TP_printk("low_lat_mem: refill extend_alloc, %p\n",
			__entry->data)
);
#endif

TRACE_EVENT(mi_alloc_pre_compact_page,
	TP_PROTO(gfp_t gfp, unsigned int order, struct page **page),

	TP_ARGS(gfp, order, page),

	TP_STRUCT__entry(
		__field(gfp_t, gfp)
		__field(unsigned int, order)
		__field(struct page **, page)
	),

	TP_fast_assign(
		__entry->gfp = gfp;
		__entry->order = order;
		__entry->page = page;
	),

	TP_printk("low_lat_mem: pre compact gfp=%s order=%d\n",
			show_gfp_flags(__entry->gfp), __entry->order)
);
#endif /* _TRACE_MI_LOW_LAT_MEM_H */

#include <trace/define_trace.h>
