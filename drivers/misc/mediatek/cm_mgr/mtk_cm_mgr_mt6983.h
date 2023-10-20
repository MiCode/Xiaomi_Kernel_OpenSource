/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#include "mtk_cm_mgr_reg_mt6983.h"

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_events_mt6983.h"

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#define OFFS_CM_HINT (0x12D4)
#define OFFS_CM_THRESH (0x12D8)
#define OFFS_DSU_PERF (0x12DC)

enum {
	CM_MGR_LP4 = 0,
	CM_MGR_MAX,
};

enum cm_mgr_cpu_cluster {
	CM_MGR_L = 0,
	CM_MGR_B,
	CM_MGR_BB,
	CM_MGR_CPU_CLUSTER,
};
/* #define CM_MGR_CPU_CLUSTER 3 */

#endif	/* __MTK_CM_MGR_PLATFORM_H__ */
