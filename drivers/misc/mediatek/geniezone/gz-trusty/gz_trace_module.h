/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _GZ_TRACE_MODULE_H_
#define _GZ_TRACE_MODULE_H_

#if IS_ENABLED(CONFIG_FTRACE)

#include <linux/kallsyms.h>
#include <linux/trace_events.h>
static noinline void tracing_mark_write(char *fmt, ...)
{
	struct va_format vaf;
	va_list va;

	va_start(va, fmt);
	vaf.fmt = fmt;
	vaf.va = &va;
	trace_printk("%pV", &vaf);
	va_end(va);
}

#define GZ_TRUSTY_TRACE_BEGIN(fmt, args...) do { \
	preempt_disable(); \
	tracing_mark_write( \
		"B|%d|GZT_"fmt"\n", current->tgid, ##args); \
	preempt_enable();\
} while (0)

#define GZ_TRUSTY_TRACE_END() do { \
	preempt_disable(); \
	tracing_mark_write("E\n"); \
	preempt_enable(); \
} while (0)

#define GZ_TRUSTY_TRACE_CNT(cnt, fmt, args...) do { \
	preempt_disable(); \
	tracing_mark_write( \
		"C|%d|GZT_"fmt"|%d\n", current->tgid, ##args, cnt); \
	preempt_enable();\
} while (0)

#define GZ_TRUSTY_ASYNC_TRACE_BEGIN(cookie, fmt, args...) do { \
	preempt_disable(); \
	tracing_mark_write( \
		"S|%d|GZT_"fmt"|%d\n", current->tgid, ##args, cookie); \
	preempt_enable();\
} while (0)

#define GZ_TRUSTY_ASYNC_TRACE_END(cookie, fmt, args...) do { \
	preempt_disable(); \
	tracing_mark_write( \
		"F|%d|GZT_"fmt"|%d\n", current->tgid, ##args, cookie); \
	preempt_enable();\
} while (0)

#define GZ_TRUSTY_TRACE_INJECTION(fmt, args...) do { \
	preempt_disable(); \
	tracing_mark_write( \
		"GZT|"fmt"\n", ##args); \
	preempt_enable();\
} while (0)

/* How to,
 * use for in the same process
 * no need add "\n"
 * you can find time slot in systrace by keyword GZ_XXXX
 * GZ_TRUSTY_TRACE_BEGIN("%s", __func__);
 * GZ_TRUSTY_TRACE_END();

 * use for async case
 * no need add "\n"
 * you can find time slot in systrace by keyword GZ_XXXX
 * GZ_TRUSTY_ASYNC_TRACE_BEGIN(atomic_read(&tctx->kick_cookie), "ToKick");
 * GZ_TRUSTY_ASYNC_TRACE_END(atomic_read(&tctx->kick_cookie), "ToKick");
 * atomic_inc(&tctx->kick_cookie);
 *
 * use for add news
 * can't find info in systrace UI, only find in raw data
 * GZ_TRUSTY_TRACE_CNT(100, "my_std_call32");
 */
#else

#define GZ_TRUSTY_TRACE_BEGIN(fmt, args...) do { \
} while (0)

#define GZ_TRUSTY_TRACE_END() do { \
} while (0)

#define GZ_TRUSTY_TRACE_CNT(cnt, fmt, args...) do { \
} while (0)

#define GZ_TRUSTY_ASYNC_TRACE_BEGIN(cookie, fmt, args...) do { \
} while (0)

#define GZ_TRUSTY_ASYNC_TRACE_END(cookie, fmt, args...) do { \
} while (0)

#endif /* CONFIG_FTRACE */

#endif
