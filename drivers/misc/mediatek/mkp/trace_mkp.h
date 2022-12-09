/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef _TRACE_MKP_H
#define _TRACE_MKP_H

#if IS_ENABLED(CONFIG_TRACING) && IS_ENABLED(CONFIG_MTK_VM_DEBUG)

#include <linux/types.h>
#include <linux/kernel.h>

/*
 * Tracing of mkp vendor hooks
 */

#define TRACE_MESSAGE_LENGTH	256
#define BEGINED_PID		(current->pid)

static inline void tracing_mark_write(const char *buf)
{
	trace_puts(buf);
}

static inline void OUTPUT_MSG(const char *fmt, pid_t pid, const char *name)
{
	char buf[TRACE_MESSAGE_LENGTH];
	int len = snprintf(buf, sizeof(buf), fmt, pid, name);

	if (len < 0)
		return;

	if (len >= (int)TRACE_MESSAGE_LENGTH)
		return;

	tracing_mark_write(buf);
}

#define MKP_HOOK_BEGIN(name) \
	do { \
		if (mkp_hook_trace_enabled()) { \
			OUTPUT_MSG("B|%d|%s", BEGINED_PID, name); \
		} \
	} while (0)

#define MKP_HOOK_END(name) \
	do { \
		if (mkp_hook_trace_enabled()) { \
			OUTPUT_MSG("E|%d|%s", BEGINED_PID, name); \
		} \
	} while (0)

#else

#define MKP_HOOK_BEGIN(name) \
	do { \
	} while (0)
#define MKP_HOOK_END(name) \
	do { \
	} while (0)

#endif

#endif /* _TRACE_MKP_H */
