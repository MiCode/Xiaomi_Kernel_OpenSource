/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM fuse
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_FUSE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_FUSE_H
#include <trace/hooks/vendor_hooks.h>
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */

struct wait_queue_head;
struct fuse_req;
DECLARE_HOOK(android_vh_queue_request_and_unlock,
	TP_PROTO(struct wait_queue_head *wq_head, bool sync),
	TP_ARGS(wq_head, sync));
DECLARE_HOOK(android_vh_fuse_request_send_ext,
	TP_PROTO(struct fuse_req *req, struct wait_queue_head *wq_head),
	TP_ARGS(req, wq_head));
DECLARE_HOOK(android_vh_fuse_request_end,
	TP_PROTO(struct task_struct *self),
	TP_ARGS(self));
DECLARE_HOOK(android_vh_fuse_request_end_ext,
	TP_PROTO(struct fuse_req *req, struct task_struct *self),
	TP_ARGS(req, self));
DECLARE_HOOK(android_vh_fuse_request_fetch,
	TP_PROTO(struct fuse_req *req, struct task_struct *self),
	TP_ARGS(req, self));

#endif /* _TRACE_HOOK_FUSE_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
