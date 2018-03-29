/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DISP_LOG_H__
#define __DISP_LOG_H__
#include <mt-plat/aee.h>
#include <linux/printk.h>

#include "disp_recorder.h"
#include "disp_debug.h"
#include "disp_drv_platform.h"

#if defined(COMMON_DISP_LOG)
#include "mtkfb_debug.h"
#endif

/*
 * Display has 7 log level:
 *    0 - ERROR_LEVEL, only error log use this level
 *    1 - Reserved For Future
 *    2 - WARN_LEVEL, for new feature implementing or single feature debug
 *    3 - DEFAULT_LEVEL, normal display log
 *    4 - Reserved For Future
 *    5 - DEBUG_LEVEL, will print display path config or HWC calling log
 *    6 - IRQ_LEVEL, print display IRQ log
 */
#define ERROR_LEVEL   0
#define WARN_LEVEL    1
#define DEFAULT_LEVEL 3
#define DEBUG_LEVEL   5
#define DISP_IRQ_LEVEL     6

/*
 * Display has 7 log level:
 *    0 - ERROR_LEVEL, only error log use this level
 *    1 - Reserved For Future
 *    2 - WARN_LEVEL, for new feature implementing or single feature debug
 *    3 - DEFAULT_LEVEL, normal display log
 *    4 - Reserved For Future
 *    5 - DEBUG_LEVEL, will print display path config or HWC calling log
 *    6 - DISP_IRQ_LEVEL, print display IRQ log
 */

/*
 * Log function: ERROR_LEVEL
 *   To print error log.
 *   And this log will be recorded in display log buffer.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPERR(fmt, args...)                                          \
	do {                                                           \
		dprec_logger_pr(DPREC_LOGGER_ERROR, fmt, ##args);      \
		if ((g_mobilelog > 0) && (g_loglevel >= ERROR_LEVEL))    \
			pr_err("[DISP]"fmt, ##args);                   \
	} while (0)

/*
 * Log function: WARN_LEVEL
 *   To print usage message.
 *   And this log will be recorded in display log buffer.
 *   This level log is reserved for feature debug,
 *   or only little meesages are needed in debugging stage.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPWRN(fmt, args...)                                          \
	do {                                                           \
		dprec_logger_pr(DPREC_LOGGER_DEBUG, fmt, ##args);      \
		if ((g_mobilelog > 0) && (g_loglevel >= WARN_LEVEL))     \
			pr_debug("[DISP]"fmt, ##args);                 \
	} while (0)

/*
 * Log function: DEFALUT_LEVEL
 *   To print usage message.
 *   And this log will be recorded in display log buffer.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPMSG(fmt, args...)                                          \
	do {                                                           \
		dprec_logger_pr(DPREC_LOGGER_DEBUG, fmt, ##args);      \
		if ((g_mobilelog > 0) && (g_loglevel >= DEFAULT_LEVEL))  \
			pr_debug("[DISP]"fmt, ##args);                 \
	} while (0)

/*
 * Log function: DEBUG_LEVEL
 *   To print which function has been in. Only print function name.
 *   And this log will be recorded in display log buffer.
 * Input:
 *   Null
 */
#define DISPFUNC()                                                     \
	do {                                                           \
		dprec_logger_pr(DPREC_LOGGER_DEBUG,                    \
			"func|%s\n", __func__);                        \
		if ((g_mobilelog > 0) && (g_loglevel >= DEBUG_LEVEL))  \
			pr_debug("[DISP]func|%s\n", __func__);         \
	} while (0)


/*
 * Log function: DEBUG_LEVEL
 *   To print usage message for debugging only
 *   and this log will be recorded in display log buffer when g_loglevel
 *   is not lower than DEBUG_LEVEL.
 *   suggest to use this function in debug stage.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPDBG(fmt, args...)                                          \
	do {                                                           \
		if (g_loglevel >= DEBUG_LEVEL)                          \
			dprec_logger_pr(DPREC_LOGGER_DEBUG,            \
					fmt, ##args);                  \
		if ((g_mobilelog > 0) && (g_loglevel >= DEBUG_LEVEL))    \
			pr_debug("[DISP]func|%s\n", __func__);         \
	} while (0)

/*
 * Log function: DISP_IRQ_LEVEL
 *   To print display IRQ message.
 *   And this log will be recorded in display log buffer when g_loglevel
 *   is not lower than DISP_IRQ_LEVEL.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPIRQ(fmt, args...)                                          \
	do {                                                           \
		if (g_loglevel >= DISP_IRQ_LEVEL)                            \
			dprec_logger_pr(DPREC_LOGGER_DEBUG,            \
					fmt, ##args);                  \
		if ((g_mobilelog > 0) && (g_loglevel >= DISP_IRQ_LEVEL))      \
			pr_debug("[DISP]IRQ: "fmt, ##args);            \
	} while (0)

/*
 * Log function: No LOG_LEVEL
 *   This log will be recorded in display log buffer only.
 *   And it won't printed in kernel log.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPRCD(fmt, args...)                                          \
	do {                                                           \
		if (g_rcdlevel > 0)                                     \
			dprec_logger_pr(DPREC_LOGGER_DEBUG,            \
				fmt, ##args);                          \
	} while (0)


/*
 * Log function: No LOG_LEVEL
 *   This log will be printed in kernel log only.
 *   And it won't be recorded in display log buffer.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPINFO(fmt, args...)                                         \
	do {                                                           \
		if (g_mobilelog)                                        \
			pr_debug("[DISP]"fmt, ##args);                 \
	} while (0)

/*
 * Log function: DUMP register Log
 *   This log will be used to dump display register log only.
 *   And it will be recorded in display log buffer.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPDMP(fmt, ...)                                              \
	do {                                                           \
		if (ddp_debug_analysis_to_buffer()) {                  \
			char log[512] = {'\0'};                        \
			scnprintf(log, 511, fmt, ##__VA_ARGS__);       \
			dprec_logger_dump(log);                        \
		} else {                                               \
			dprec_logger_pr(DPREC_LOGGER_DUMP,             \
					fmt, ##__VA_ARGS__);           \
			if (g_mobilelog)                                \
				pr_debug("[DISP]"fmt,                  \
					 ##__VA_ARGS__);               \
		}                                                      \
	} while (0)

/*
 * Log function: ASSERT
 *   This log will be used to make a system ASSERT.
 * Input:
 *   Null
 */
#ifndef ASSERT
#define ASSERT(expr)					               \
	do {						               \
		if (expr)				               \
			break;				               \
		pr_err("[DISP]ASSERT FAILED %s, %d\n",	               \
			__FILE__, __LINE__); BUG();	               \
	} while (0)
#endif

/*
 * Log function: trigger to dump DB log
 *   This function can trigger AEE to dump DB log.
 *   It can be used when a fatal Error occurred.
 * Input:
 *   Null
 */
#define DISPAEE(fmt, args...)                                          \
	do {                                                           \
		char str[200];                                         \
		snprintf(str, 199, "DISP:"fmt, ##args);                \
		DISPERR(fmt, ##args);                                  \
		aee_kernel_warning_api(__FILE__, __LINE__,             \
			DB_OPT_DEFAULT | DB_OPT_MMPROFILE_BUFFER,      \
			str, fmt, ##args);                             \
		pr_err("[DISP]"fmt, ##args);                           \
	} while (0)

/*
 * Log function: legacy log function for Fence ERROR
 *   This log will be used to print display fence log only.
 *   And it will be recorded in display ERROR log buffer.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPPR_ERROR(fmt, args...)                                     \
	do {                                                           \
		dprec_logger_pr(DPREC_LOGGER_ERROR,                    \
				fmt,                                   \
				##args);                               \
		pr_err("[DISP]ERROR:"fmt, ##args);	               \
	} while (0)

/*
 * Log function: legacy log function for Fence Message
 *   This log will be used to print fence log only.
 *   And it will be recorded in display FENCE log buffer.
 * Input:
 *   fmt: which format string will be printed.
 *   args: variable will be printed.
 */
#define DISPPR_FENCE(fmt, args...)                                     \
	do {                                                           \
		dprec_logger_pr(DPREC_LOGGER_FENCE,                    \
				fmt,                                   \
				##args);                               \
		if (g_fencelog)                                         \
			pr_debug("[DISP]fence/"fmt, ##args);           \
	} while (0)

#endif /* __DISP_LOG_H__ */
