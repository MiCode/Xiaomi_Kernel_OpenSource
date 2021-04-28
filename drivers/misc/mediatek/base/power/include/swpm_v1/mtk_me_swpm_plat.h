/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __MTK_ME_SWPM_PLAT_H__
#define __MTK_ME_SWPM_PLAT_H__

#if defined(CONFIG_MACH_MT6893)
#include "subsys/mtk_me_swpm_mt6893.h"
#elif defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
#include "subsys/mtk_me_swpm_mt6853.h"
#else
/* Use a default header for other projects */
/* Todo: Should refine in the future */
#include "subsys/mtk_me_swpm_default.h"
#endif

#endif

