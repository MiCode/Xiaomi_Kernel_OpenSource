/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __MTK_SPM_MISC_H___
#define __MTK_SPM_MISC_H___

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)

#include "spm_v2/mtk_spm_misc.h"

#elif defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739)

#include "spm_v4/mtk_spm_misc.h"

#endif

#endif /* __MTK_SPM_MISC_H___ */
