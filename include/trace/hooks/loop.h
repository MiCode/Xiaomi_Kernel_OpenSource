/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM loop
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_LOOP__H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_LOOP_H

#include <trace/hooks/vendor_hooks.h>

struct bio;
struct kiocb;
DECLARE_HOOK(android_vh_loop_prepare_cmd,
	TP_PROTO(struct bio *bio, struct kiocb *iocb),
	TP_ARGS(bio, iocb));

#endif /* _TRACE_HOOK_LOOP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
