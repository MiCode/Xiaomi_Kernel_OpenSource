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
#ifndef __MCUPM_IPI_ID_H__
#define __MCUPM_IPI_ID_H__

#include <mt-plat/mtk_tinysys_ipi.h>

/* define module id here ... */
#define CH_S_PLATFORM	0
#define CH_S_CPU_DVFS	1
#define CH_S_FHCTL	2
#define CH_S_MCDI	3
#define CH_S_SUSPEND	4
#define CH_IPIR_C_MET   5
#define CH_IPIS_C_MET   6
#define CH_S_EEMSN      7

#define MCUPM_IPI_COUNT	    8


extern struct mtk_mbox_device mcupm_mboxdev;
extern struct mtk_ipi_device mcupm_ipidev;

#endif /* __MCUPM_IPI_ID_H__ */
