/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_H__
#define __MTK_CM_MGR_PLATFORM_H__

#include "mtk_cm_mgr_reg_mt6789.h"

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif /* CONFIG_MTK_DRAMC */

enum {
	CM_MGR_LP4 = 0,
	CM_MGR_MAX,
};

enum cm_mgr_cpu_cluster {
	CM_MGR_L = 0,
	CM_MGR_B,
	CM_MGR_CPU_CLUSTER,
};
/* #define CM_MGR_CPU_CLUSTER 2 */

#endif	/* __MTK_CM_MGR_PLATFORM_H__ */
