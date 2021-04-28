/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_SPM_VCORE_DVFS_IPI_H__
#define __MTK_SPM_VCORE_DVFS_IPI_H__

#if defined(CONFIG_MACH_MT6775)

#include "spm_v3/mtk_spm_vcore_dvfs_ipi_mt6775.h"

#elif defined(CONFIG_MACH_MT6771)

#include "spm_v4/mtk_spm_vcore_dvfs_ipi_mt6771.h"

#endif

#endif
