/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_DEBUG_DRV_H__
#define __APUSYS_DEBUG_DRV_H__

#define APUSYS_DEBUG_DEV_NAME "apusys_dump"

#define APUSYS_DEBUG_LOG_PREFIX "[apusys][dump]"
#define LOG_ERR(x, args...) \
pr_info(APUSYS_DEBUG_LOG_PREFIX "[error] %s " x, __func__, ##args)
#define LOG_WARN(x, args...) \
pr_info(APUSYS_DEBUG_LOG_PREFIX "[warn] %s " x, __func__, ##args)
#define LOG_INFO(x, args...) \
pr_info(APUSYS_DEBUG_LOG_PREFIX "[info] %s " x, __func__, ##args)
#define DEBUG_TAG LOG_DEBUG("\n")

#define LOG_DEBUG(x, args...) \
	{ \
		if (debug_log_level > 0) \
			pr_info(APUSYS_DEBUG_LOG_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define LOG_DETAIL(x, args...) \
	{ \
		if (debug_log_level > 1) \
			pr_info(APUSYS_DEBUG_LOG_PREFIX "[detail] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

#define INT_STA_PRINTF(m, x, args...)\
	{ \
		if (m != NULL) \
			seq_printf(m, x, ##args); \
		else \
			pr_info(APUSYS_DEBUG_LOG_PREFIX "[isr_work] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}

extern int debug_log_level;


#endif
