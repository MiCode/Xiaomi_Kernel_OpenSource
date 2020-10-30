/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_TRACE_H__
#define __LPM_TRACE_H__

#include "lpm_trace_event.h"

#define LPM_TRACE_EVENT_SUPPORT

enum {
	LPM_TRACE_EVENT_LEVEL_DEBUG,
};

#ifdef LPM_TRACE_EVENT_SUPPORT
#define LPM_TRACE_IMPL(_level, fmt, args...)	({\
	int t_len = 0;\
	struct lpm_trace_debug_t _buf;\
	(void)_level;\
	t_len = snprintf(_buf._datas\
			, LPM_TRACE_EVENT_MESG_MAX-1, fmt, ##args);\
	_buf._datas[t_len] = '\0';\
	trace_lpm_trace_rcuidle(&_buf);\
	t_len; })

#define LPM_TRACE(fmts, args...)\
	LPM_TRACE_IMPL(LPM_TRACE_EVENT_LEVEL_DEBUG, fmts, ##args)
#else
#define LPM_TRACE(fmts, args...)
#endif

#endif
