/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RS_TRACE_H_
#define _RS_TRACE_H_

#define RS_SYSTRACE_LIST(macro) \
	macro(MANDATORY, 0), \
	macro(MAX, 1), \

#define RS_GENERATE_ENUM(name, shft) RS_DEBUG_##name = 1U << shft
enum {
	RS_SYSTRACE_LIST(RS_GENERATE_ENUM)
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

