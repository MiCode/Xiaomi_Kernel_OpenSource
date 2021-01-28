/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#define PERF_TIME 3000

#include "mtk_cm_mgr_reg_mt6779.h"

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_events_mt6779.h"

#define USE_CM_MGR_AT_SSPM

enum {
	CM_MGR_LP4X_2CH = 0,
	CM_MGR_MAX,
};

#endif	/* __MTK_CM_MGR_PLATFORM_H__ */
