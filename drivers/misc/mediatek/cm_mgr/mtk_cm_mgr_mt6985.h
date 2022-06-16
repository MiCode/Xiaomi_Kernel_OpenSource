/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#include "mtk_cm_mgr_reg_mt6985.h"

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_events_mt6985.h"

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#define OFFS_CM_HINT (0x12D4)
#define OFFS_CM_THRESH (0x12D8)

#define CM_CPU0_AVG_OUTER_STALL_RATIO 0x1304
#define CM_CPU1_AVG_OUTER_STALL_RATIO 0x1308
#define CM_CPU2_AVG_OUTER_STALL_RATIO 0x130C
#define CM_CPU3_AVG_OUTER_STALL_RATIO 0x1310
#define CM_CPU4_AVG_OUTER_STALL_RATIO 0x1314
#define CM_CPU5_AVG_OUTER_STALL_RATIO 0x1318
#define CM_CPU6_AVG_OUTER_STALL_RATIO 0x131C
#define CM_CPU7_AVG_OUTER_STALL_RATIO 0x1320
#define CM_DRAM_AVG_FREQ 0x1324
#define CM_LATENCY_AWARENESS_STATUS 0x1328

enum {
	CM_MGR_LP5 = 0,
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
