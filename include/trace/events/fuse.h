/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM fuse

#if !defined(_TRACE_FUSE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_FUSE_H

#include <linux/tracepoint.h>
#include <linux/fs.h>
#include <linux/fuse.h>

TRACE_EVENT(fuse_init_reply,

	TP_PROTO(struct fuse_mount *fm, struct fuse_init_in *in_arg,
				struct fuse_init_out *out_arg, int error),

	TP_ARGS(fm, in_arg, out_arg, error),

	TP_STRUCT__entry(
		__field(dev_t, dev)
		__field(struct fuse_conn *, fc)
		__field(uint32_t, flags)
		__field(int, error)
	),

	TP_fast_assign(
		__entry->dev = fm->fc->dev;
		__entry->fc = fm->fc;
		__entry->flags = out_arg->flags;
		__entry->error = error;
	),

	TP_printk("dev = (%d,%d) fc=%p flags=%x error=%d",
		MAJOR(__entry->dev), MINOR(__entry->dev),
		__entry->fc,
		__entry->flags,
		__entry->error)
);

TRACE_EVENT(fuse_simple_request,
	TP_PROTO(struct fuse_mount *fm, struct fuse_args *args),
	TP_ARGS(fm, args),
	TP_STRUCT__entry(
		__field(dev_t, dev)
		__field(struct fuse_conn *, fc)
		__field(uint32_t, opcode)
		__field(uint64_t, nodeid)
	),

	TP_fast_assign(
		__entry->dev = fm->fc->dev;
		__entry->fc = fm->fc;
		__entry->opcode = args->opcode;
		__entry->nodeid = args->nodeid;
	),

	TP_printk("dev = (%d,%d) fc=%p opcode=%u nodeid=%llu",
		MAJOR(__entry->dev), MINOR(__entry->dev),
		__entry->fc,
		__entry->opcode,
		__entry->nodeid)
);

TRACE_EVENT(fuse_simple_background,
	TP_PROTO(struct fuse_mount *fm, struct fuse_args *args),
	TP_ARGS(fm, args),
	TP_STRUCT__entry(
		__field(dev_t, dev)
		__field(struct fuse_conn *, fc)
		__field(uint32_t, opcode)
		__field(uint64_t, nodeid)
	),

	TP_fast_assign(
		__entry->dev = fm->fc->dev;
		__entry->fc = fm->fc;
		__entry->opcode = args->opcode;
		__entry->nodeid = args->nodeid;
	),

	TP_printk("dev = (%d,%d) fc=%p opcode=%u nodeid=%llu",
		MAJOR(__entry->dev), MINOR(__entry->dev),
		__entry->fc,
		__entry->opcode,
		__entry->nodeid)
);
#endif /* _TRACE_FUSE_H */

 /* This part must be outside protection */
#include <trace/define_trace.h>
