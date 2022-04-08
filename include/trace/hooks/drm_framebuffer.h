/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM drm_framebuffer

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DRM_FRAMEBUFFER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DRM_FRAMEBUFFER_H

#include <trace/hooks/vendor_hooks.h>

struct drm_framebuffer;
DECLARE_HOOK(android_vh_atomic_remove_fb,
	TP_PROTO(struct drm_framebuffer *fb, bool *allow),
	TP_ARGS(fb, allow))

#endif /* _TRACE_HOOK_DRM_FRAMEBUFFER_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

