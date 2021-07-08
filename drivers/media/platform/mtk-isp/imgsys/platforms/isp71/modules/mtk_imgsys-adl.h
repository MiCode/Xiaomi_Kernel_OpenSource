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

#define ADL_REG_DBG_SEL     (0x54)
#define ADL_REG_DBG_PORT    (0x50)

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_adl_init(struct mtk_imgsys_dev *imgsys_dev);

void imgsys_adl_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
	unsigned int engine);

void imgsys_adl_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_IMGSYS_TRAW_H_ */
