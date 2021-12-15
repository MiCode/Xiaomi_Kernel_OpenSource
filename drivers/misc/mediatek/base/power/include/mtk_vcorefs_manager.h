/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _COMMON_MTK_VCOREFS_MANAGER_H
#define _COMMON_MTK_VCOREFS_MANAGER_H

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)

#include "vcorefs_v1/mtk_vcorefs_manager_mt6757.h"

#elif defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6763) \
	|| defined(CONFIG_MACH_MT6771) || defined(CONFIG_MACH_MT6759) \
	|| defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT6739) \
	|| defined(CONFIG_MACH_MT6775)

#include "vcorefs_v3/mtk_vcorefs_manager.h"

#endif

#endif /* _COMMON_MTK_VCOREFS_MANAGER_H */

