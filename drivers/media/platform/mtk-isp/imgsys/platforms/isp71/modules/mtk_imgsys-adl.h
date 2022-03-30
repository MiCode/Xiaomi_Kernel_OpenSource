/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: ChenHung Yang <chenhung.yang@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_ADL_H_
#define _MTK_IMGSYS_ADL_H_

// Standard C header file

// kernel header file

// mtk imgsys local header file
#include <mtk_imgsys-dev.h>

// Local header file
#include "mtk_imgsys-engine.h"

// ADL A & B register base
#define IMGADL_A_REG_BASE           (0x15005000)
#define IMGADL_B_REG_BASE           (0x15007000)

#define IMGADL_A_DUMP_SIZE          (0x1500)
#define IMGADL_B_DUMP_SIZE          (0x1500)

// Debug port offset
#define IPUDMATOP_DMA_DBG_SEL       (0x0070)
#define IMGADL_ADL_RESET            (0x0300)
#define IMGADL_ADL_DMA_0_DEBUG      (0x0350)
#define IMGADL_ADL_DMA_0_DEBUG_SEL  (0x0354)
#define IMGADL_ADL_CQ_DEBUG         (0x0358)
#define IMGADL_ADL_CQ_DMA_DEBUG     (0x035C)
#define IMGADLCQ_CQ_EN              (0x1000)
#define CQP2ENGDMATOP_DMA_DBG_SEL   (0x1270)

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_adl_init(struct mtk_imgsys_dev *imgsys_dev);

void imgsys_adl_set(struct mtk_imgsys_dev *imgsys_dev);

void imgsys_adl_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t engine);

void imgsys_adl_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_IMGSYS_ADL_H_ */
