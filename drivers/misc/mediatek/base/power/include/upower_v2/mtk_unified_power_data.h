/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */
#ifndef MTK_UNIFIED_POWER_DATA_H
#define MTK_UNIFIED_POWER_DATA_H

#if defined(CONFIG_MACH_MT6763)
#include "mtk_unified_power_data_mt6763.h"
#endif
#if defined(CONFIG_MACH_MT6758)
#include "mtk_unified_power_data_mt6758.h"
#endif

#if defined(CONFIG_MACH_MT6765)
#include "mtk_unified_power_data_mt6765.h"
#endif

#if defined(CONFIG_MACH_MT6877)
#include "mtk_unified_power_data_mt6877.h"
#endif

#if defined(CONFIG_MACH_MT6739)
#include "mtk_unified_power_data_mt6739.h"
#endif

#if defined(CONFIG_MACH_MT6761)
#include "mtk_unified_power_data_mt6761.h"
#endif

#if defined(CONFIG_MACH_MT6771)
#include "mtk_unified_power_data_mt6771.h"
#endif

#if defined(CONFIG_MACH_MT3967)
#include "mtk_unified_power_data_mt3967.h"
#endif

#if defined(CONFIG_MACH_MT6779)
#include "mtk_unified_power_data_mt6779.h"
#endif

#if defined(CONFIG_MACH_MT6781)
#include "mtk_unified_power_data_mt6781.h"
#endif

#if defined(CONFIG_MACH_MT6768)
#include "mtk_unified_power_data_mt6768.h"
#endif

#if defined(CONFIG_MACH_MT6785)
#include "mtk_unified_power_data_mt6785.h"
#endif

#if defined(CONFIG_MACH_MT6833)
#include "mtk_unified_power_data_mt6833.h"
#endif

#if defined(CONFIG_MACH_MT6885)
#if !defined(TRIGEAR_UPOWER)
#include "mtk_unified_power_data_mt6885.h"
#else
#include "mtk_unified_power_data_mt6893.h"
#endif
#endif

#if defined(CONFIG_MACH_MT6893)
#include "mtk_unified_power_data_mt6893.h"
#endif
#if defined(CONFIG_MACH_MT6853)
#include "mtk_unified_power_data_mt6853.h"
#endif
#if defined(CONFIG_MACH_MT6873)
#include "mtk_unified_power_data_mt6873.h"
#endif


#endif /* UNIFIED_POWER_DATA_H */
