/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef __MTK_SPM_VCORE_DVFS_IPI_H__
#define __MTK_SPM_VCORE_DVFS_IPI_H__

#if defined(CONFIG_MACH_MT6775)

#include "spm_v3/mtk_spm_vcore_dvfs_ipi_mt6775.h"

#elif defined(CONFIG_MACH_MT6771)

#include "spm_v4/mtk_spm_vcore_dvfs_ipi_mt6771.h"

#endif

#endif
