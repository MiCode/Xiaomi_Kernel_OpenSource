/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _RS_TRACE_H_
#define _RS_TRACE_H_

enum {
	RS_DEBUG_MANDATORY = 1U << 0,
	RS_DEBUG_MAX       = 1U << 1,
};

extern uint32_t rs_systrace_mask;

int rs_trace_init(void);

void __rs_systrace_c(pid_t pid, int val, const char *fmt, ...);
void __rs_systrace_b(pid_t pid, const char *fmt, ...);
void __rs_systrace_e(void);

void __rs_systrace_c_uint64(pid_t pid, uint64_t val, const char *fmt, ...);

#define rs_systrace_c(mask, pid, val, fmt...) \
	do { \
		if (rs_systrace_mask & mask) \
			__rs_systrace_c(pid, val, fmt); \
	} while (0)

#define rs_systrace_c_uint64(mask, pid, val, fmt...) \
	do { \
		if (rs_systrace_mask & mask) \
			__rs_systrace_c_uint64(pid, val, fmt); \
	} while (0)

#define rs_systrace_b(mask, tgid, fmt, ...) \
	do { \
		if (rs_systrace_mask & mask) \
			__rs_systrace_b(tgid, fmt); \
	} while (0)

#define rs_systrace_e(mask) \
	do { \
		if (rs_systrace_mask & mask) \
			__rs_systrace_e(); \
	} while (0)

#define rs_systrace_c_log(pid, val, fmt...) \
	rs_systrace_c(RS_DEBUG_MANDATORY, pid, val, fmt)

#define rs_systrace_c_uint64_log(pid, val, fmt...) \
	rs_systrace_c_uint64(RS_DEBUG_MANDATORY, pid, val, fmt)

#endif

