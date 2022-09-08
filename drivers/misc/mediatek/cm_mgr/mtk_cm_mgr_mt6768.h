/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#include "mtk_cm_mgr_reg_mt6768.h"

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_events_mt6768.h"

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#define CM_MGR_CPU_CLUSTER 2

enum {
	CM_MGR_LP4 = 0,
	CM_MGR_MAX,
};

#endif	/* __MTK_CM_MGR_PLATFORM_H__ */
