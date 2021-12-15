/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Fred-YC Yu <fred-yc.yu@mediatek.com>
 */
#ifndef __MCUPM_IPI_ID_H__
#define __MCUPM_IPI_ID_H__

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

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
