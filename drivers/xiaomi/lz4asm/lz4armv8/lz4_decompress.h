/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM lz4_decompress

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_LZ4_DECOMPRESS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_LZ4_DECOMPRESS_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_lz4_decompress_bypass,
	TP_PROTO(const char *in, char *out,
			unsigned int inlen, unsigned int outlen,
			bool dip, int *ret, bool *bypass),
	TP_ARGS(in, out, inlen, outlen, dip, ret, bypass));


#endif /* _TRACE_HOOK_LZ4_DECOMPRESS_H */
/* This part must be outside protection */
#include <trace/define_trace.h>