/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Marvin Lin <Marvin.Lin@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_ME_H_
#define _MTK_IMGSYS_ME_H_

#include "mtk_imgsys-dev.h"

#define ME_CTL_OFFSET      0x0000
#define ME_CTL_RANGE       0xA10
#define ME_CTL_RANGE_TF    0x120

#define MMG_CTL_OFFSET      0x0000
#define MMG_CTL_RANGE       0xA40
#define MMG_CTL_RANGE_TF    0x30

void imgsys_me_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_me_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);
void imgsys_me_uninit(struct mtk_imgsys_dev *imgsys_dev);


#endif /* _MTK_IMGSYS_ME_H_ */
