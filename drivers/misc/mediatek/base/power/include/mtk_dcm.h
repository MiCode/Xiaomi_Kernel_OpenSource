/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef _MTK_DCM_H_
#define _MTK_DCM_H_

#if defined(CONFIG_MACH_MT6735)		\
	|| defined(CONFIG_MACH_MT6735M) \
	|| defined(CONFIG_MACH_MT6753)

#include "../mt6735/mtk_dcm.h"

#elif defined(CONFIG_MACH_MT6755)

#include "../mt6755/mtk_dcm.h"

#elif defined(CONFIG_MACH_MT6757)

#include "../mt6757/mtk_dcm.h"

#elif defined(CONFIG_MACH_MT6580)

#include "../mt6580/mtk_dcm.h"

#elif defined(CONFIG_MACH_MT6797)

#include "../mt6797/mtk_dcm.h"

#elif defined(CONFIG_MT2701_DCM)

#include "../mt2701_dcm/mtk_dcm.h"

#elif defined(CONFIG_MACH_MT8160)

#include "../mt8160/mtk_dcm.h"

#elif defined(CONFIG_MACH_MT8163)

#include "../mt8163/mtk_dcm.h"

#elif defined(CONFIG_MACH_MT8173)

#include "../mt8173/mtk_dcm.h"

#elif defined(CONFIG_MACH_MT8127)

#include "../../../include/mt-plat/mt8127/include/mach/mtk_dcm.h"

#elif defined(CONFIG_MACH_ELBRUS)	\
	|| defined(CONFIG_MACH_MT6799)	\
	|| defined(CONFIG_MACH_MT6759)	\
	|| defined(CONFIG_MACH_MT6761)	\
	|| defined(CONFIG_MACH_MT6763)	\
	|| defined(CONFIG_MACH_MT6765)	\
	|| defined(CONFIG_MACH_MT6739)

#include "dcm_v1/mtk_dcm.h"

#else

#error NO corresponding project of mtk_dcm.h header file can be found!!!

#endif

#endif /* _MTK_DCM_H_ */

