/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_LOG_H__
#define __APU_LOG_H__

#include <linux/ratelimit.h>

/* allow in 5ms burst 50 times message print */
#define apu_info_ratelimited(dev, fmt, ...)  \
{                                                \
	static DEFINE_RATELIMIT_STATE(_rs,           \
				      HZ * 5,                    \
				      50);                       \
	if (__ratelimit(&_rs))                       \
		dev_info(dev, fmt, ##__VA_ARGS__);       \
}

enum {
	APUSYS_PWR_LOG_OFF = 0, // disable
	APUSYS_PWR_LOG_ERR = 1, // error
	APUSYS_PWR_LOG_WRN = 2, // warning
	APUSYS_PWR_LOG_PRO = 3, // profiling
	APUSYS_PWR_LOG_INF = 4, // information
	APUSYS_PWR_LOG_DBG = 5, // debug
	APUSYS_PWR_LOG_ALL = 6, // verbose
	APUSYS_PWR_LOG_CUS = 7, // customer view
};

extern uint32_t log_lvl;

#endif
