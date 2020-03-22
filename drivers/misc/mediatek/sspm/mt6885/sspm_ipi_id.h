/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifndef __SSPM_IPI_ID_H__
#define __SSPM_IPI_ID_H__

#include <mt-plat/mtk_tinysys_ipi.h>

/* define module id here ... */
#define IPIS_C_PPM            0
#define IPIS_C_QOS            1
#define IPIS_C_PMIC           2
#define IPIS_C_MET            3
#define IPIS_C_THERMAL        4
#define IPIS_C_GPU_DVFS       5
#define IPIS_C_GPU_PM         6
#define IPIS_C_PLATFORM       7
#define IPIS_C_SMI            8
#define IPIS_C_CM             9
#define IPIS_C_SLBC           10
#define IPIS_C_SPM_SUSPEND    11
#define IPIR_I_QOS            12
#define IPIR_C_MET            13
#define IPIR_C_GPU_DVFS       14
#define IPIR_C_PLATFORM       15
#define IPIR_C_SLBC           16
#define SSPM_IPI_COUNT        17

extern struct mtk_mbox_device sspm_mboxdev;
extern struct mtk_ipi_device sspm_ipidev;

#endif /* __SSPM_IPI_ID_H__ */
