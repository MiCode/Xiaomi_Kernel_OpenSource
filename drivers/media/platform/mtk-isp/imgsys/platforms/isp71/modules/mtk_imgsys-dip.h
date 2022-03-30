/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DIP_DIP_H_
#define _MTK_DIP_DIP_H_

#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-engine.h"
/* DIP */
#define TOP_CTL_OFFSET	0x0000
#define TOP_CTL_RANGE	0x0334
#define DMATOP_OFFSET	0x1000
#define DMATOP_RANGE	0x0CC
#define RDMA_OFFSET	0x1200
#define RDMA_RANGE	0x0AAC
#define WDMA_OFFSET	0x22C0
#define WDMA_RANGE	0x0C08
#define NR3D_CTL_OFFSET	0x5000
#define NR3D_CTL_RANGE	0x484
#define TNR_CTL_OFFSET	0x6000
#define TNR_CTL_RANGE	0x0234
#define DIPCTL_DBG_SEL 0x1A8
#define DIPCTL_DBG_OUT 0x1AC
#define DIPCTL_DMA_DBG_SEL 0x1070
#define DIPCTL_DMA_DBG_OUT 0x1074
#define NR3D_DBG_SEL 0x500C

/* DIP NR */
#define MCRP_OFFSET	0xB240
#define MCRP_RANGE	0x0004
#define N_DMATOP_OFFSET	0x1000
#define N_DMATOP_RANGE	0x0CC
#define N_RDMA_OFFSET	0x1A40
#define N_RDMA_RANGE	0x04EC
#define N_WDMA_OFFSET	0x2000
#define N_WDMA_RANGE	0x0DEC
#define TNC_CTL_OFFSET  0xD4AC
#define TNC_CTL_RANGE	0x20
#define TNC_DEBUG_SET   0xD4CC

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev);
#endif /* _MTK_DIP_DIP_H_ */

