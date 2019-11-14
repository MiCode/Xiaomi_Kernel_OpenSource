/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_COMMON_H__
#define __APUSYS_COMMON_H__

#include <linux/printk.h>
#include <linux/seq_file.h>

extern u32 g_log_level;

enum {
	APUSYS_LOG_BITMAP_INFO,
	APUSYS_LOG_BITMAP_FLOW,
	APUSYS_LOG_BITMAP_CMD,
	APUSYS_LOG_BITMAP_MEM,
	APUSYS_LOG_BITMAP_PERF,
	APUSYS_LOG_BITMAP_LINE,

	APUSYS_LOG_BITMAP_MAX,
};

#define APUSYS_PREFIX "[apusys]"

#define LOG_ERR(x, args...) \
	pr_info(APUSYS_PREFIX "[error] %s " x, __func__, ##args)
#define LOG_WARN(x, args...) \
	pr_info(APUSYS_PREFIX "[warn] %s " x, __func__, ##args)
#define LOG_INFO(x, args...) \
	{ \
		if (g_log_level & (1 << APUSYS_LOG_BITMAP_INFO)) \
			pr_info(APUSYS_PREFIX "%s "\
			x, __func__, ##args);\
	}

#define LOG_DEBUG(x, args...) \
	{ \
		if (g_log_level & (1 << APUSYS_LOG_BITMAP_FLOW)) \
			pr_info(APUSYS_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define CLOG_DEBUG(x, args...) \
	{ \
		if (g_log_level & (1 << APUSYS_LOG_BITMAP_CMD)) \
			pr_info(APUSYS_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define MLOG_DEBUG(x, args...) \
	{ \
		if (g_log_level & (1 << APUSYS_LOG_BITMAP_MEM)) \
			pr_info(APUSYS_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define PLOG_DEBUG(x, args...) \
	{ \
		if (g_log_level & (1 << APUSYS_LOG_BITMAP_PERF)) \
			pr_info(APUSYS_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define LLOG_DEBUG(x, args...) \
	{ \
		if (g_log_level & (1 << APUSYS_LOG_BITMAP_LINE)) \
			pr_info(APUSYS_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}


/* print to console via seq file */
#define LOG_CON(s, x, args...) \
	{\
		if (s) \
			seq_printf(s, x, ##args); \
		else \
			LOG_INFO(x, ##args); \
	}

#define DEBUG_TAG LLOG_DEBUG("\n")

#endif
