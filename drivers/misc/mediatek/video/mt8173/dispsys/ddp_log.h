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

#ifndef _H_DDP_LOG_
#define _H_DDP_LOG_
#include <mt-plat/aee.h>
#include "display_recorder.h"
#include "ddp_debug.h"
#ifndef LOG_TAG
#define LOG_TAG
#endif

#define DISP_LOG_D(fmt, args...)   pr_err("[DDP/"LOG_TAG"]"fmt, ##args)
#define DISP_LOG_I(fmt, args...)   pr_err("[DDP/"LOG_TAG"]"fmt, ##args)
#define DISP_LOG_W(fmt, args...)   pr_err("[DDP/"LOG_TAG"]"fmt, ##args)
#define DISP_LOG_E(fmt, args...)   \
	do { \
		pr_err("[DDP/"LOG_TAG"]error:"fmt, ##args); \
		dprec_logger_pr(DPREC_LOGGER_ERROR, fmt, ##args); \
	} while (0)


#define DISP_LOG_V(fmt, args...)               \
	do { \
		if (ddp_debug_dbg_log_level() >= 2) \
			DISP_LOG_I(fmt, ##args);            \
	} while (0)

#define DDPIRQ(fmt, args...)                   \
	do { \
		if (ddp_debug_irq_log_level()) \
			DISP_LOG_I(fmt, ##args);            \
	} while (0)

#define DDPDBG(fmt, args...)                   \
	do { \
		if (ddp_debug_dbg_log_level()) \
			DISP_LOG_I(fmt, ##args);            \
	} while (0)

#define DDPDEBUG_D(fmt, args...)               \
{                                       \
	DISP_LOG_D(fmt, ##args);                \
}

#define DDPMSG(fmt, args...)                   \
{                                       \
	DISP_LOG_I(fmt, ##args);                \
}

#define DDPERR(fmt, args...)                   \
{                                       \
	DISP_LOG_E(fmt, ##args);                \
}

#define DDPDUMP(fmt, ...)                        \
	do { \
		if (ddp_debug_analysis_to_buffer()) { \
			char log[512] = {'\0'};               \
			scnprintf(log, 511, fmt, ##__VA_ARGS__);    \
			dprec_logger_dump(log);             \
		}                                       \
		else                                    \
			pr_warn("[DDP/"LOG_TAG"]"fmt, ##__VA_ARGS__);\
	} while (0)

#ifndef ASSERT
#define ASSERT(expr)                             \
	do { \
		if (expr) \
			break;                          \
		pr_err("DDP ASSERT FAILED %s, %d\n",     \
	    __FILE__, __LINE__); BUG();          \
	} while (0)
#endif

#define DDPAEE(args...)                        \
				/*      do {                                       \
				   aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "DDP/"LOG_TAG, args); \
				   } while (0) */

#endif
