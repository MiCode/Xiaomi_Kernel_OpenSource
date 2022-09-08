/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_ppm
#if !defined(_MTK_PPM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_PPM_TRACE_H
#include <linux/tracepoint.h>

TRACE_EVENT(ppm_user_setting,

		TP_PROTO(unsigned int policy_mask,
			int cid,
			unsigned int min_idx,
			unsigned int max_idx),

		TP_ARGS(policy_mask, cid, min_idx, max_idx),

		TP_STRUCT__entry(
			__field(unsigned int, mask)
			__field(int, cid)
			__field(unsigned int, min_idx)
			__field(unsigned int, max_idx)
			),

		TP_fast_assign(
			__entry->mask = policy_mask;
			__entry->cid = cid;
			__entry->min_idx = min_idx;
			__entry->max_idx = max_idx;
			),

		TP_printk("policy=%d cid=%d min=%d max=%d",
				__entry->mask, __entry->cid,
				__entry->min_idx, __entry->max_idx)
);

#endif /*_MTK_PPM_TRACE_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_ppm_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
