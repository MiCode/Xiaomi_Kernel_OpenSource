/*
 * Copyright (C) 2015 MediaTek Inc.
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

#if defined(CONFIG_ARCH_MT6755)

#include "spm_v2/mt_spm_vcore_dvfs_mt6755.h"

#elif defined(CONFIG_ARCH_MT6757)

#include "spm_v2/mt_spm_vcore_dvfs_mt6757.h"

#elif defined(CONFIG_ARCH_MT6797)

#include "spm_v2/mt_spm_vcore_dvfs_mt6797.h"

#endif

#endif /* __MT_SPM_REG_H___ */

