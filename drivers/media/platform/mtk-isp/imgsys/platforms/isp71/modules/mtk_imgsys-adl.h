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
#define ADL_A_REG_BASE  (0x15005000)
#define ADL_B_REG_BASE  (0x15007000)

#define ADL_A_DUMP_SIZE (0x1000)
#define ADL_B_DUMP_SIZE (0x1000)

// Debug port offset
#define ADL_REG_RESET   (0x300)
#define ADL_REG_DMA_DBG (0x350)
#define ADL_REG_DBG_SEL (0x354)
#define ADL_REG_CQ_DBG  (0x358)
#define ADL_REG_CQ_DMA  (0x35C)

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_adl_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);

void imgsys_adl_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
	uint32_t engine);

void imgsys_adl_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_IMGSYS_ADL_H_ */
