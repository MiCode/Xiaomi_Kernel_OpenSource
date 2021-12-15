/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef _MTK_MDPM_H_
#define _MTK_MDPM_H_

#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6768)
#include "mdpm_v1/mtk_mdpm.h"
#elif defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#include "mdpm_v2/mtk_mdpm.h"
#else
#error NO corresponding project of mtk_mdpm.h header file can be found!!!
#endif

#endif /* _MTK_MDPM_H_ */

