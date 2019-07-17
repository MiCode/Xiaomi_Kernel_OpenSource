/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#ifndef MTK_UNIFIED_POWER_DATA_H
#define MTK_UNIFIED_POWER_DATA_H

#if defined(CONFIG_MACH_MT6758)
#include "mtk_unified_power_data_mt6758.h"
#endif

#if defined(CONFIG_MACH_MT6765)
#include "mtk_unified_power_data_mt6765.h"
#endif

#if defined(CONFIG_MACH_MT6761)
#include "mtk_unified_power_data_mt6761.h"
#endif

#if defined(CONFIG_MACH_MT3967)
#include "mtk_unified_power_data_mt3967.h"
#endif

#if defined(CONFIG_MTK_UNIFY_POWER)
#include "mtk_unified_power_data_platform.h"
#endif

#if defined(CONFIG_MACH_MT6768)
#include "mtk_unified_power_data_mt6768.h"
#endif

#if defined(CONFIG_MACH_MT6785)
#include "mtk_unified_power_data_mt6785.h"
#endif

#endif /* UNIFIED_POWER_DATA_H */
