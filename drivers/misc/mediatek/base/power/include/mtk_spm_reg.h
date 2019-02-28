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

#ifndef __MT_SPM_REG_H___
#define __MT_SPM_REG_H___

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)

#include "spm_v2/mtk_spm_reg.h"

#elif defined(CONFIG_MACH_MT6799)

#include "spm_v3/mtk_spm_reg.h"

#elif defined(CONFIG_MACH_MT6775)

#include "spm_v3/mtk_spm_reg_mt6775.h"

#elif defined(CONFIG_MACH_MT6759)

#include "spm_v3/mtk_spm_reg_mt6759.h"

#elif defined(CONFIG_MACH_MT6758)

#include "spm_v3/mtk_spm_reg_mt6758.h"

#elif defined(CONFIG_MACH_MT6763)

#include "spm_v4/mtk_spm_reg_mt6763.h"

#elif defined(CONFIG_MACH_MT6739)

#include "spm_v4/mtk_spm_reg_mt6739.h"

#elif defined(CONFIG_MACH_MT6771)

#include "spm_v4/mtk_spm_reg_mt6771.h"
#endif

#endif /* __MT_SPM_REG_H___ */

