/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM evdev

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_EVDEV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_EVDEV_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_pass_input_event,
       TP_PROTO(int head, int tail, int bufsize, int type, int code, int value),
       TP_ARGS(head, tail, bufsize, type, code, value))

#endif /* _TRACE_HOOK_EVDEV_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

