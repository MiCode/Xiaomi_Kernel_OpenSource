/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */
#ifndef MTK_UNIFIED_POWER_DATA_H
#define MTK_UNIFIED_POWER_DATA_H

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6765)
#include "mtk_unified_power_data_6765.h"
#else
#include "mtk_unified_power_data_plat.h"
#endif

#endif /* UNIFIED_POWER_DATA_H */
