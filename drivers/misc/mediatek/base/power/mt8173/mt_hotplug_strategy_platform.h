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

/**
* @file    mt_hotplug_strategy_platform.h
* @brief   hotplug strategy (hps) - header file for platform defines
*/

#ifndef __MT_HOTPLUG_STRATEGY_PLATFORM_H__
#define __MT_HOTPLUG_STRATEGY_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/sched/rt.h>		/* MAX_RT_PRIO */

/*
 * CONFIG - compile time
 */
#define HPS_TASK_PRIORITY		(MAX_RT_PRIO - 3)
#define HPS_TIMER_INTERVAL_MS		200

#define MAX_CPU_UP_TIMES		10
#define MAX_CPU_DOWN_TIMES		100
#define MAX_TLP_TIMES			10
/* cpu capability of big / little = 1.7, aka 170, 170 - 100 = 70 */
#define CPU_DMIPS_BIG_LITTLE_DIFF	70

/*
 * CONFIG - runtime
 */
#define DEF_CPU_UP_THRESHOLD		80
#define DEF_CPU_UP_TIMES		1
#define DEF_CPU_DOWN_THRESHOLD		70
#define DEF_CPU_DOWN_TIMES		10
#define DEF_TLP_TIMES			1

#define EN_CPU_INPUT_BOOST		1
#define DEF_CPU_INPUT_BOOST_CPU_NUM	2

#define EN_CPU_RUSH_BOOST		1
#define DEF_CPU_RUSH_BOOST_THRESHOLD	98
#define DEF_CPU_RUSH_BOOST_TIMES	1

#define EN_HPS				1
#define EN_LOG_NOTICE			1
#define EN_LOG_INFO			0
#define EN_LOG_DEBUG			0

#ifdef __cplusplus
}
#endif

#endif /* __MT_HOTPLUG_STRATEGY_PLATFORM_H__ */
