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

#ifndef _APU_LOG_H_
#define _APU_LOG_H_

#ifdef BUILD_POLICY_TEST
#include "test.h"
#else
#include <aee.h>
#include "debug_driver.h"

extern int g_pwr_log_level;
extern int g_pm_procedure;

enum {
	APUSYS_PWR_LOG_ERR,
	APUSYS_PWR_LOG_WARN,
	APUSYS_PWR_LOG_INFO,
	APUSYS_PWR_LOG_DEBUG,
	APUSYS_PWR_LOG_VERBOSE,
};

#define DVFS_TAG "[DVFS]"
#define PWR_LOG_INF(format, args...) \
	do { \
		if (g_pwr_log_level >= APUSYS_PWR_LOG_INFO) \
			pr_info(DVFS_TAG " " format, ##args); \
	} while (0)

#define PWR_LOG_PM(format, args...) \
	do { \
		if (g_pwr_log_level >= APUSYS_PWR_LOG_WARN) \
			pr_info(DVFS_TAG "[pm] " format, ##args); \
	} while (0)

#define PWR_LOG_ERR(format, args...) \
		pr_info(DVFS_TAG "[error] " format, ##args)


#define PWR_TAG "[apu_power_2.0]"
#define LOG_PM(format, args...) \
	do { \
		if (g_pwr_log_level >= APUSYS_PWR_LOG_WARN) \
			pr_info(PWR_TAG "[pm] " format, ##args); \
	} while (0)

#define LOG_INF(format, args...) \
	do { \
		if (g_pwr_log_level >= APUSYS_PWR_LOG_INFO) \
			pr_info(PWR_TAG " " format, ##args); \
	} while (0)

#define LOG_WRN(format, args...) \
	do { \
		if (!g_pm_procedure) \
			if (g_pwr_log_level >= APUSYS_PWR_LOG_WARN) \
				pr_info(PWR_TAG "[warn] " format, ##args); \
	} while (0)

#define LOG_ERR(format, args...)    pr_info(PWR_TAG "[error] " format, ##args)
#define LOG_DBG(format, args...) \
	do { \
		if (g_pwr_log_level >= APUSYS_PWR_LOG_DEBUG) \
			pr_info(PWR_TAG " " format, ##args); \
	} while (0)

#define apu_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("APU PWR", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)

#define LOG_DUMP(format, args...) \
	apu_dbg_print(format, ##args)

#endif /* BUILD_POLICY_TEST */
#endif /* _APU_LOG_H_ */
