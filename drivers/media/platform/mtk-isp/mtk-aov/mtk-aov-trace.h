/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <Johnson-CH.chiu@mediatek.com>
 *
 */
#ifndef __MTK_AOV_TRACE_H__
#define __MTK_AOV_TRACE_H__

#include <linux/kernel.h>
#include <linux/trace_events.h>

#include "mtk-aov-config.h"

#if AOV_SUPPORT_TRACE

#define AVO_MAX_TRACE_SIZE    (1024)

#define AOV_TRACE_FORCE_BEGIN(fmt, args...)                   \
	__aov_trace_write("B|%d|" fmt "\n", current->tgid, ##args)

#define AOV_TRACE_FORCE_END()                                 \
	__aov_trace_write("E\n")

#define AOV_TRACE_BEGIN(fmt, args...)                         \
	do {                                                        \
		if (is_aov_trace_enable()) {                              \
			AOV_TRACE_FORCE_BEGIN(fmt, ##args);                     \
		}                                                         \
	} while (0)

#define AOV_TRACE_END()                                       \
	do {                                                        \
		if (is_aov_trace_enable()) {                              \
			AOV_TRACE_FORCE_END();                                  \
		}                                                         \
	} while (0)

bool is_aov_trace_enable(void);

void __aov_trace_write(const char *fmt, ...);

#else

#define AOV_TRACE_FORCE_BEGIN(fmt, args...)
#define AOV_TRACE_FORCE_END()
#define AOV_TRACE_BEGIN(fmt, args...)
#define AOV_TRACE_END()

#endif

#endif  // __MTK_AOV_TRACE_H__
