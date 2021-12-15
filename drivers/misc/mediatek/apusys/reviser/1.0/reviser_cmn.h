// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_REVISER_COMMON_H__
#define __APUSYS_REVISER_COMMON_H__

#include <linux/printk.h>
#include <linux/seq_file.h>

extern u8 g_reviser_log_level;

enum {
	REVISER_LOG_WARN,
	REVISER_LOG_INFO,
	REVISER_LOG_DEBUG,
};

#define REVISER_PREFIX "[reviser]"

#define LOG_ERR(x, args...) \
	pr_info(REVISER_PREFIX "[error] %s " x, __func__, ##args)
#define LOG_WARN(x, args...) \
	pr_info(REVISER_PREFIX "[warn] %s " x, __func__, ##args)
#define LOG_INFO(x, args...) \
	pr_info(REVISER_PREFIX "%s " x, __func__, ##args)
#define LOG_DEBUG(x, args...) \
	{ \
		if (g_reviser_log_level >= REVISER_LOG_DEBUG) \
			pr_info(REVISER_PREFIX "[debug] %s/%d "\
			x, __func__, __LINE__, ##args); \
	}


//#define DEBUG_TAG LOG_DEBUG("[%d]\n", __LINE__)
#define DEBUG_TAG

/* print to console via seq file */
#define LOG_CON(s, x, args...) \
	{\
		if (s) \
			seq_printf(s, x, ##args); \
		else \
			LOG_DEBUG(x, ##args); \
	}
#endif
