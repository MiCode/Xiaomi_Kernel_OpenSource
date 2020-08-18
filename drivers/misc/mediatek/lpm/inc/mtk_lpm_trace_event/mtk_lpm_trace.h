/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_TRACE_H__
#define __MTK_LPM_TRACE_H__

#include "mtk_lpm_trace_event.h"

#define MTK_LPM_TRACE_EVENT_SUPPORT

enum {
	MTK_LPM_TRACE_EVENT_LEVEL_DEBUG,
};

#ifdef MTK_LPM_TRACE_EVENT_SUPPORT
#define MTK_LPM_TRACE_IMPL(_level, fmt, args...)	({\
	int t_len = 0;\
	struct mtk_lpm_trace_debug_t _buf;\
	(void)_level;\
	t_len = snprintf(_buf._datas\
			, MTK_LPM_TRACE_EVENT_MESG_MAX-1, fmt, ##args);\
	_buf._datas[t_len] = '\0';\
	trace_mtk_lpm_trace_rcuidle(&_buf);\
	t_len; })

#define MTK_LPM_TRACE(fmts, args...)\
	MTK_LPM_TRACE_IMPL(MTK_LPM_TRACE_EVENT_LEVEL_DEBUG, fmts, ##args)
#else
#define MTK_LPM_TRACE(fmts, args...)
#endif

#endif
