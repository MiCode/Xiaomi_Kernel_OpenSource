/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#ifndef __MCUPM_IPI_ID_H__
#define __MCUPM_IPI_ID_H__

//#include <mt-plat/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>


/* define module id here ... */
#define CH_S_PLATFORM	0
#define CH_S_CPU_DVFS	1
#define CH_S_FHCTL	2
#define CH_S_MCDI	3
#define CH_S_SUSPEND	4
#define MCUPM_IPI_COUNT	5


extern struct mtk_mbox_device mcupm_mboxdev;
extern struct mtk_ipi_device mcupm_ipidev;

#endif /* __MCUPM_IPI_ID_H__ */
