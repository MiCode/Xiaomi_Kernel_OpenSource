/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_MDPM_H__
#define __MTK_MDPM_H__

#if defined(CONFIG_MACH_MT6765)
#include "mt6765/mtk_mdpm_platform.h"
#elif defined(CONFIG_MACH_MT6761)
#include "mt6761/mtk_mdpm_platform.h"
#elif defined(CONFIG_MACH_MT3967)
#include "mt3967/mtk_mdpm_platform.h"
#elif defined(CONFIG_MACH_MT6768)
#include "mt6768/mtk_mdpm_platform.h"
#endif

#endif /* __MTK_MDPM_H__ */
