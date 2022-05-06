/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM drm_atomic

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DRM_ATOMIC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DRM_ATOMIC_H

#include <trace/hooks/vendor_hooks.h>

struct drm_atomic_state;
struct drm_crtc;
DECLARE_HOOK(android_vh_drm_atomic_check_modeset,
	TP_PROTO(struct drm_atomic_state *state, struct drm_crtc *crtc, bool *allow),
	TP_ARGS(state, crtc, allow))

#endif /* _TRACE_HOOK_DRM_ATOMIC_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

