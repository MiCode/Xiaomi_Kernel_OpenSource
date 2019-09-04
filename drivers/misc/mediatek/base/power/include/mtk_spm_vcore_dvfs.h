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

#ifndef _COMMON_MTK_SPM_VCORE_DVFS_H
#define _COMMON_MTK_SPM_VCORE_DVFS_H

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)

#include "spm_v2/mtk_spm_vcore_dvfs_mt6757.h"

#elif defined(CONFIG_MACH_MT6797)

#include "spm_v2/mt_spm_vcore_dvfs_mt6797.h"

#elif defined(CONFIG_MACH_MT6799)

#include "spm_v3/mtk_spm_vcore_dvfs.h"

#elif defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739)

#include "spm_v4/mtk_spm_vcore_dvfs.h"

#elif defined(CONFIG_MACH_MT6771)

#include "spm_v4/mtk_spm_vcore_dvfs_mt6771.h"

#elif defined(CONFIG_MACH_MT6759)

#include "spm_v3/mtk_spm_vcore_dvfs_mt6759.h"

#elif defined(CONFIG_MACH_MT6758)

#include "spm_v3/mtk_spm_vcore_dvfs_mt6758.h"

#elif defined(CONFIG_MACH_MT6775)

#include "spm_v3/mtk_spm_vcore_dvfs_mt6775.h"

#endif

#endif /* _COMMON_MTK_SPM_VCORE_DVFS_H */

