/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _CORE_CTL_H
#define _CORE_CTL_H

static noinline void tracing_mark_write(char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';
	trace_printk(log);
}

#define CORE_CTL_TRACE_BEGIN(fmt, args...) do { \
	preempt_disable(); \
	tracing_mark_write( \
		"B|%d|"fmt"\n", 7788, ##args); \
	preempt_enable();\
} while (0)

#define CORE_CTL_TRACE_END() do { \
	preempt_disable(); \
	tracing_mark_write("E|7788\n"); \
	preempt_enable(); \
} while (0)

#define CORE_CTL_TRACE_CNT(cnt, fmt, args...) do { \
	preempt_disable(); \
	tracing_mark_write( \
		"C|%d|"fmt"|%d\n", 7788, ##args, cnt); \
	preempt_enable();\
} while (0)

struct _CORE_CTL_PACKAGE {
	union {
		__u32 cid;
		__u32 cpu;
	};
	union {
		__u32 min;
		__u32 is_pause;
		__u32 throttle_ms;
		__u32 not_preferred_cpus;
		__u32 boost;
		__u32 thres;
		__u32 enable_policy;
	};
	__u32 max;
};

#define CORE_CTL_FORCE_RESUME_CPU               _IOW('g', 1,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_FORCE_PAUSE_CPU                _IOW('g', 2,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_OFFLINE_THROTTLE_MS        _IOW('g', 3,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_LIMIT_CPUS                 _IOW('g', 4,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_NOT_PREFERRED              _IOW('g', 5, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_BOOST                      _IOW('g', 6, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_UP_THRES                   _IOW('g', 7, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_ENABLE_POLICY                  _IOW('g', 8, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_CPU_BUSY_THRES             _IOW('g', 9, struct _CORE_CTL_PACKAGE)

#endif /* _CORE_CTL_H */
