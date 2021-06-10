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
#include "mtk_imgsys-engine.h"

#define TOP_CTL_OFFSET	0x0000
#define TOP_CTL_RANGE	0x02F0
#define DMATOP_OFFSET	0x0800
#define DMATOP_RANGE	0x0080
#define RDMA_OFFSET	0x0A00
#define RDMA_RANGE	0x08B4
#define WDMA_OFFSET	0x1500
#define WDMA_RANGE	0x0760
#define NR3D_CTL_OFFSET	0x2000
#define NR3D_CTL_RANGE	0x2500
#define MCRP_OFFSET	0xA300
#define MCRP_RANGE	0x0004

#define DIPCTL_DBG_SEL 0x170
#define DIPCTL_DBG_OUT 0x174
#define NR3D_DBG_SEL 0x200C

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);

#endif /* _MTK_DIP_DIP_H_ */

