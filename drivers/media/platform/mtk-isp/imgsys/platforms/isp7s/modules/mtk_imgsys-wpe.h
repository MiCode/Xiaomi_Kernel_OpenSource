/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Floria Huang <floria.huang@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_WPE_H_
#define _MTK_IMGSYS_WPE_H_

#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-engine.h"

void imgsys_wpe_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_wpe_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_wpe_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);
void imgsys_wpe_uninit(struct mtk_imgsys_dev *imgsys_dev);
#endif /* _MTK_IMGSYS_WPE_H_ */
