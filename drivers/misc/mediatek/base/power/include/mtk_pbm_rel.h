/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef _MTK_PBM_REL_H_
#define _MTK_PBM_REL_H_

#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) \
	|| defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#include "pbm_v4/mtk_pbm_rel.h"
#else
#error NO corresponding project of mtk_pbm_rel.h header file can be found!!!
#endif

#endif /* _MTK_PBM_REL_H_ */

