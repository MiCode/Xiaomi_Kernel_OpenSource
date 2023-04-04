/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MDPM_H__
#define __MTK_MDPM_H__

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
#include "mtk_mdpm_platform_6761.h"
#endif
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6765)
#include "mtk_mdpm_platform_6765.h"
#endif
#if IS_ENABLED(CONFIG_MTK_MDPM_LEGACY_V1)
#include "mtk_mdpm_platform_v1.h"
#endif

#endif /* __MTK_MDPM_H__ */
