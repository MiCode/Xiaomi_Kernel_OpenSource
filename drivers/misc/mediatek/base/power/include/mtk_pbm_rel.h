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

#ifndef _MTK_PBM_REL_H_
#define _MTK_PBM_REL_H_

#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) \
	|| defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#include "pbm_v4/mtk_pbm_rel.h"
#else
#error NO corresponding project of mtk_pbm_rel.h header file can be found!!!
#endif

#endif /* _MTK_PBM_REL_H_ */

