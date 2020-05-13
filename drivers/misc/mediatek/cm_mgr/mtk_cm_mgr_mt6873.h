/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#define PERF_TIME 3000

#include "mtk_cm_mgr_reg_mt6873.h"

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_events_mt6873.h"

/* #define USE_CPU_TO_DRAM_MAP */

#define USE_CM_MGR_AT_SSPM
#define CM_MGR_CPU_OPP_SIZE 16
#define CM_MGR_VCORE_OPP_COUNT 21

enum {
	CM_MGR_LP4 = 0,
	CM_MGR_MAX,
};

#endif	/* __MTK_CM_MGR_PLATFORM_H__ */
