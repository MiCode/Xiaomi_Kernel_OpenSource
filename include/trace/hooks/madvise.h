/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM madvise
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_MADVISE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_MADVISE_H
#include <trace/hooks/vendor_hooks.h>

DECLARE_RESTRICTED_HOOK(android_rvh_process_madvise_bypass,
	TP_PROTO(int pidfd, const struct iovec __user *vec, size_t vlen,
		int behavior, unsigned int flags, ssize_t *ret, bool *bypass),
	TP_ARGS(pidfd, vec, vlen, behavior, flags, ret, bypass), 1);
struct vm_area_struct;
DECLARE_HOOK(android_vh_update_vma_flags,
	TP_PROTO(struct vm_area_struct *vma),
	TP_ARGS(vma));
DECLARE_HOOK(android_vh_madvise_pageout_return_error,
	TP_PROTO(int ret, bool *return_error),
	TP_ARGS(ret, return_error));
DECLARE_HOOK(android_vh_process_madvise_return_error,
	TP_PROTO(int behavior, int ret,  bool *return_error),
	TP_ARGS(behavior, ret, return_error));
DECLARE_HOOK(android_vh_madvise_pageout_bypass,
	TP_PROTO(struct mm_struct *mm, bool pageout, int *ret),
	TP_ARGS(mm, pageout, ret));

#endif

#include <trace/define_trace.h>