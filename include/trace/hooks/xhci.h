/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM xhci
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_XHCI_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_XHCI_H

#include <trace/hooks/vendor_hooks.h>
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */

DECLARE_HOOK(android_vh_xhci_suspend,
	TP_PROTO(struct device *dev, int *bypass),
	TP_ARGS(dev, bypass));

DECLARE_HOOK(android_vh_xhci_resume,
	TP_PROTO(struct device *dev, int *bypass),
	TP_ARGS(dev, bypass));

#endif /* _TRACE_HOOK_XHCI_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

