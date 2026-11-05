/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM fsnotify

#ifdef CREATE_TRACE_POINTS
#define TRACE_INCLUDE_PATH trace/hooks
#define UNDEF_TRACE_INCLUDE_PATH
#endif

#if !defined(_TRACE_HOOK_FSNOTIFY_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_FSNOTIFY_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_fsnotify_open,
		TP_PROTO(struct file *file, __u32 *mask),
		TP_ARGS(file, mask));

#endif /* _TRACE_HOOK_FSNOTIFY_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
