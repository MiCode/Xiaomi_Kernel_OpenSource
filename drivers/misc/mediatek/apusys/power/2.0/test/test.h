// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _TEST_H_
#define _TEST_H_

#ifdef BUILD_POLICY_TEST

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define u32 unsigned int


extern int g_pwr_log_level;
extern int g_pm_procedure;

enum {
	APUSYS_PWR_LOG_WARN,
	APUSYS_PWR_LOG_INFO,
	APUSYS_PWR_LOG_DEBUG,
};


#define DVFS_TAG "[DVFS]"
#define PWR_LOG_INF(format, args...)	printf(format, ##args)
#define PWR_LOG_PM(format, args...)	printf(format, ##args)
#define PWR_LOG_ERR(format, args...)	printf(format, ##args)

#define VPU_TAG "[apu_power_2.0]"
#define LOG_DBG(format, args...)	printf(format, ##args)
#define LOG_INF(format, args...)	printf(format, ##args)
#define LOG_WRN(format, args...)	printf(format, ##args)


extern u32 get_devinfo_with_index(unsigned int index);

#endif
#endif
