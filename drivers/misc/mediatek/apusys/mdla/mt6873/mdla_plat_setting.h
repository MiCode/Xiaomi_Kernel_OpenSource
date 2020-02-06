/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MDLA_PLAT_SETTING_H__
#define __MDLA_PLAT_SETTING_H__

#define MTK_MDLA_MAX_NUM 1 // shift to dts later

#ifndef CONFIG_MTK_MDLA_DEBUG
#define CONFIG_MTK_MDLA_DEBUG
#endif

#ifndef CONFIG_MTK_MDLA_ION
#define CONFIG_MTK_MDLA_ION //move to dts latter
#endif

#define __APUSYS_MDLA_UT__ //TODO remove after UT issue fixed
//#define __APUSYS_MDLA_SW_PORTING_WORKAROUND__

#define __APUSYS_MDLA_PMU_SUPPORT__
//#ifdef CONFIG_MTK_APUSYS_RT_SUPPORT
#define __APUSYS_PREEMPTION__
//#endif//MTK_APUSYS_RT_SUPPORT
#ifdef __APUSYS_PREEMPTION__
#include "apusys_device.h"
#define PRIORITY_LEVEL 2

extern u32 mdla_batch_number;
extern u32 mdla_preemption_times;
extern u32 mdla_preemption_debug;
#else//__APUSYS_PREEMPTION__
#define PRIORITY_LEVEL 1
#endif//__APUSYS_PREEMPTION__

#endif //__MDLA_PLAT_SETTING_H__
