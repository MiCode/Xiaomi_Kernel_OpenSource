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
#define ADL_A_REG_BASE  (0x15005300)
#define ADL_B_REG_BASE  (0x15007300)

// Debug port offset
#define ADL_REG_DMA_DBG (0x50)
#define ADL_REG_DBG_SEL (0x54)
#define ADL_REG_CQ_DBG  (0x58)
#define ADL_REG_CQ_DMA  (0x5C)

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_adl_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);

void imgsys_adl_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t engine);

void imgsys_adl_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_IMGSYS_ADL_H_ */
