/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_ISP_SWPM_PLAT_H__
#define __MTK_ISP_SWPM_PLAT_H__

#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
#include "subsys/mtk_isp_swpm_mt6885.h"
#endif

#if defined(CONFIG_MACH_MT6873)
#include "subsys/mtk_isp_swpm_mt6873.h"
#endif

#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
#include "subsys/mtk_isp_swpm_mt6853.h"
#endif

#if defined(CONFIG_MACH_MT6877)
#include "subsys/mtk_isp_swpm_mt6877.h"
#endif

#endif

