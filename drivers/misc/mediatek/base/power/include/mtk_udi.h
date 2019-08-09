/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MTK_UDI_H_
#define _MTK_UDI_H_

#if defined(CONFIG_MACH_MT6779)

#include "udi_v2/mtk_udi_mt6779.h"

#elif defined(CONFIG_MACH_MT3967)

#include "udi_v1/mtk_udi_mt3967.h"

#elif defined(CONFIG_MACH_MT6761)

#include "udi_v1/mtk_udi_mt6761.h"

#elif defined(CONFIG_MACH_MT6765)

#include "udi_v1/mtk_udi_mt6765.h"

#elif defined(CONFIG_MACH_MT6775)

#include "udi_v1/mtk_udi_mt6775.h"

#elif defined(CONFIG_MACH_MT6771)

#include "udi_v1/mtk_udi_mt6771.h"

#elif defined(CONFIG_MACH_MT6758)

#include "udi_v1/mtk_udi_mt6758.h"

#elif defined(CONFIG_MACH_MT6763)

#include "udi_v1/mtk_udi_mt6763.h"

#elif defined(CONFIG_MACH_MT6759)

#include "udi_v1/mtk_udi_mt6759.h"

#elif defined(CONFIG_MACH_MT6799)

#include "../mt6799/mtk_udi.h"

#else

#error NO corresponding project of mtk_udi.h header file can be found!!!

#endif

#endif /* _MTK_UDI_H_ */

