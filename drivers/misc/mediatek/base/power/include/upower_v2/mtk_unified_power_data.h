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

#if defined(CONFIG_MACH_MT6761)
#include "mtk_unified_power_data_mt6761.h"
#endif

#if defined(CONFIG_MACH_MT3967)
#include "mtk_unified_power_data_mt3967.h"
#endif

#if defined(CONFIG_MACH_MT6779)
#include "mtk_unified_power_data_mt6779.h"
#endif

#endif /* UNIFIED_POWER_DATA_H */
