/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef _COMMON_MTK_SPM_VCORE_DVFS_H
#define _COMMON_MTK_SPM_VCORE_DVFS_H
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)

#include "spm_v2/mtk_spm_vcore_dvfs_mt6757.h"

#elif defined(CONFIG_MACH_MT6763)

#include "spm_v4/mtk_spm_vcore_dvfs.h"

#endif

#endif /* _COMMON_MTK_SPM_VCORE_DVFS_H */

