/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#define PERF_TIME 3000

#include "mtk_cm_mgr_reg_mt6761.h"

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_events_mt6761.h"

#ifdef CONFIG_MTK_DRAMC_LEGACY
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC_LEGACY */

#define PER_CPU_STALL_RATIO
#define LIGHT_LOAD
/* #define USE_AVG_PMU */
/* #define DEBUG_CM_MGR */
#define USE_TIMER_CHECK
#define USE_NEW_CPU_OPP
#define USE_SINGLE_CLUSTER

#define CM_MGR_EMI_OPP	2
#define CM_MGR_LOWER_OPP 8
#define CM_MGR_CPU_CLUSTER 1
#define CM_MGR_CPU_COUNT 4
#define CM_MGR_CPU_LIMIT 4

#define CLUSTER0_MASK   0x0f

#define VCORE_ARRAY_SIZE CM_MGR_EMI_OPP
#define CM_MGR_CPU_ARRAY_SIZE (CM_MGR_CPU_CLUSTER * CM_MGR_EMI_OPP)
#define RATIO_COUNT (100 / 5 - 1)
#define IS_UP 1
#define IS_DOWN 0
#define USE_TIMER_CHECK_TIME msecs_to_jiffies(50)
#define CM_MGR_INIT_DELAY_MS 1
#define CM_MGR_BW_VALUE 0

enum {
	CM_MGR_LP4X_2CH_3200 = 0,
	CM_MGR_LP3_1CH_1866,
	CM_MGR_MAX,
};

#endif	/* __MTK_CM_MGR_PLATFORM_H__ */
