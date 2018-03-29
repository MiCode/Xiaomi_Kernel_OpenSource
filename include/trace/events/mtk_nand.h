#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_nand

#if !defined(_TRACE_MTK_NAND_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_NAND_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(nand_io,

	TP_PROTO(long long len),

	TP_ARGS(len),

	TP_STRUCT__entry(
		__field(long long, len)
	),

	TP_fast_assign(
		__entry->len = len;
	),

	TP_printk("len=%lld", __entry->len)
);

DEFINE_EVENT(nand_io, ubifs_read_begin,
	TP_PROTO(long long len),
	TP_ARGS(len)
	);

DEFINE_EVENT(nand_io, ubifs_read_end,
	TP_PROTO(long long len),
	TP_ARGS(len)
	);

DEFINE_EVENT(nand_io, ubifs_read_len,
	TP_PROTO(long long len),
	TP_ARGS(len)
	);

DEFINE_EVENT(nand_io, ubifs_write_begin,
	TP_PROTO(long long len),
	TP_ARGS(len)
	);

DEFINE_EVENT(nand_io, ubifs_write_end,
	TP_PROTO(long long len),
	TP_ARGS(len)
	);

DEFINE_EVENT(nand_io, ubifs_write_len,
	TP_PROTO(long long len),
	TP_ARGS(len)
	);


DECLARE_EVENT_CLASS(nand_drv,

	TP_PROTO(unsigned long long size, unsigned long long addr),

	TP_ARGS(size, addr),

	TP_STRUCT__entry(
		__field(unsigned long long, size)
		__field(unsigned long long, addr)
	),

	TP_fast_assign(
		__entry->size = size;
		__entry->addr = addr;
	),

	TP_printk("size=0x%llx, addr=0x%llx",
			__entry->size, __entry->addr)
);

DEFINE_EVENT(nand_drv, nand_read_begin,
	TP_PROTO(unsigned long long size, unsigned long long addr),
	TP_ARGS(size, addr)
	);

DEFINE_EVENT(nand_drv, nand_read_end,
	TP_PROTO(unsigned long long size, unsigned long long addr),
	TP_ARGS(size, addr)
	);

DEFINE_EVENT(nand_drv, nand_write_begin,
	TP_PROTO(unsigned long long size, unsigned long long addr),
	TP_ARGS(size, addr)
	);

DEFINE_EVENT(nand_drv, nand_write_end,
	TP_PROTO(unsigned long long size, unsigned long long addr),
	TP_ARGS(size, addr)
	);

DEFINE_EVENT(nand_drv, nand_erase_begin,
	TP_PROTO(unsigned long long size, unsigned long long addr),
	TP_ARGS(size, addr)
	);

DEFINE_EVENT(nand_drv, nand_erase_end,
	TP_PROTO(unsigned long long size, unsigned long long addr),
	TP_ARGS(size, addr)
	);

#endif /* _TRACE_MTK_NAND_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

