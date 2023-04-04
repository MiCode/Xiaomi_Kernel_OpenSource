/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MT_COMMON_PPM_PLATFORM_H__
#define __MT_COMMON_PPM_PLATFORM_H__

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6765)
#include <mtk_ppm_platform_6765.h>
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6833)
#include <mtk_ppm_platform_6833.h>
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6768)
#include <mtk_ppm_platform_6768.h>
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6739)
#include <mtk_ppm_platform_6739.h>
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
#include <mtk_ppm_platform_6761.h>
#endif

#endif
