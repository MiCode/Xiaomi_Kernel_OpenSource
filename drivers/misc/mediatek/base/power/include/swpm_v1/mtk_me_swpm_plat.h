/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_ME_SWPM_PLAT_H__
#define __MTK_ME_SWPM_PLAT_H__

#if defined(CONFIG_MACH_MT6893)
#include "subsys/mtk_me_swpm_mt6893.h"
#elif defined(CONFIG_MACH_MT6885)
#include "subsys/mtk_me_swpm_mt6885.h"
#elif defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
#include "subsys/mtk_me_swpm_mt6853.h"
#elif defined(CONFIG_MACH_MT6877)
#include "subsys/mtk_me_swpm_mt6877.h"
#else
/* Use a default header for other projects */
/* Todo: Should refine in the future */
#include "subsys/mtk_me_swpm_default.h"
#endif

#endif

