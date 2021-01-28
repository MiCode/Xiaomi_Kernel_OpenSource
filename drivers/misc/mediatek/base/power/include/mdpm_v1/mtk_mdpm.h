/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MDPM_H__
#define __MTK_MDPM_H__

#if defined(CONFIG_MACH_MT6765)
#include "mt6765/mtk_mdpm_platform.h"
#elif defined(CONFIG_MACH_MT6761)
#include "mt6761/mtk_mdpm_platform.h"
#elif defined(CONFIG_MACH_MT3967)
#include "mt3967/mtk_mdpm_platform.h"
#endif

#endif /* __MTK_MDPM_H__ */
