/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM dm

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DM_H

#include <trace/hooks/vendor_hooks.h>
#include <linux/blk_types.h>

struct bio;
DECLARE_HOOK(android_vh_dm_update_clone_bio,
	TP_PROTO(struct bio *clone, struct bio *bio),
	TP_ARGS(clone, bio));

#endif /* _TRACE_HOOK_DM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
