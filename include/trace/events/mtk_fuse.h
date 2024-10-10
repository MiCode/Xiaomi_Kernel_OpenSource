/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_fuse

#if !defined(_TRACE_MTK_FUSE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_FUSE_H

#include <linux/tracepoint.h>

TRACE_EVENT(mtk_fuse_queue_forget,
	TP_PROTO(const char *func, int line, u64 nodeid, u64 nlookup),

	TP_ARGS(func, line, nodeid, nlookup),

	TP_STRUCT__entry(
		__string(func, func)
		__field(int, line)
		__field(u64, nodeid)
		__field(u64, nlookup)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__entry->line = line;
		__entry->nodeid = nodeid;
		__entry->nlookup = nlookup;
	),

	TP_printk("%s:%d, nodeid = %llu, nlookup = %llu",
		__get_str(func),
		__entry->line,
		__entry->nodeid,
		__entry->nlookup)
);

TRACE_EVENT(mtk_fuse_nlookup,
	TP_PROTO(const char *func, int line, struct inode *inode, u64 nodeid, u64 nlookup, u64 nlookup_after),

	TP_ARGS(func, line, inode, nodeid, nlookup, nlookup_after),

	TP_STRUCT__entry(
		__string(func, func)
		__field(int, line)
		__field(struct inode *, inode)
		__field(u64, nodeid)
		__field(u64, nlookup)
		__field(u64, nlookup_after)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__entry->line = line;
		__entry->inode = inode;
		__entry->nodeid = nodeid;
		__entry->nlookup = nlookup;
		__entry->nlookup_after = nlookup_after;
	),

	TP_printk("%s:%d, inode = 0x%p, nodeid = %llu, nlookup = %llu, nlookup_after = %llu",
		__get_str(func),
		__entry->line,
		__entry->inode,
		__entry->nodeid,
		__entry->nlookup,
		__entry->nlookup_after)
);

TRACE_EVENT(mtk_fuse_force_forget,
	TP_PROTO(const char *func, int line, struct inode *inode, u64 nodeid, u64 nlookup),

	TP_ARGS(func, line, inode, nodeid, nlookup),

	TP_STRUCT__entry(
		__string(func, func)
		__field(int, line)
		__field(struct inode *, inode)
		__field(u64, nodeid)
		__field(u64, nlookup)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__entry->line = line;
		__entry->inode = inode;
		__entry->nodeid = nodeid;
		__entry->nlookup = nlookup;
	),

	TP_printk("%s:%d, inode = 0x%p, nodeid = %llu, nlookup = %llu",
		__get_str(func),
		__entry->line,
		__entry->inode,
		__entry->nodeid,
		__entry->nlookup)
);

TRACE_EVENT(mtk_fuse_iget_backing,
	TP_PROTO(const char *func, int line, struct inode *inode, u64 nodeid, struct inode *backing),

	TP_ARGS(func, line, inode, nodeid, backing),

	TP_STRUCT__entry(
		__string(func, func)
		__field(int, line)
		__field(struct inode *, inode)
		__field(u64, nodeid)
		__field(struct inode *, backing)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__entry->line = line;
		__entry->inode = inode;
		__entry->nodeid = nodeid;
		__entry->backing = backing;
	),

	TP_printk("%s:%d, inode = %p, nodeid = %llu, backing = %p",
		__get_str(func),
		__entry->line,
		__entry->inode,
		__entry->nodeid,
		__entry->backing)
);
#endif /*_TRACE_MTK_FUSE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
