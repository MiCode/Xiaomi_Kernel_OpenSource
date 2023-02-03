/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM direct_io
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DIRECT_IO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DIRECT_IO_H

#include <trace/hooks/vendor_hooks.h>

struct kiocb;
struct bio;
DECLARE_HOOK(android_vh_direct_io_update_bio,
	TP_PROTO(struct kiocb *iocb, struct bio *bio),
	TP_ARGS(iocb, bio));

#endif /* _TRACE_HOOK_DIRECT_IO_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
