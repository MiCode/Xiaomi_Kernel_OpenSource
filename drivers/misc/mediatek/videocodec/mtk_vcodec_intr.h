/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_INTR_H_
#define _MTK_VCODEC_INTR_H_

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include "mtk_vcodec_pm.h"
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"
#include "drv_api.h"
#define MTK_INST_IRQ_RECEIVED           0x1

#define MTK_VDEC_HW_ACTIVE      0x10
#define MTK_VDEC_IRQ_CFG        0x11
#define MTK_VDEC_IRQ_CLR        0x10
#define MTK_VDEC_IRQ_CFG_REG    0xA4
#define MTK_VDEC_IRQ_STATUS_DEC_SUCCESS        0x10000

#define VENC_IRQ_STATUS_SPS         0x1
#define VENC_IRQ_STATUS_PPS         0x2
#define VENC_IRQ_STATUS_FRM         0x4
#define VENC_IRQ_STATUS_DRAM        0x8
#define VENC_IRQ_STATUS_PAUSE       0x10
#define VENC_IRQ_STATUS_SWITCH      0x20

#define MTK_VENC_IRQ_STATUS_OFFSET      0x05C
#define MTK_VENC_IRQ_ACK_OFFSET 0x060

#define VCODEC_DEVNAME     "Vcodec"
int mtk_vcodec_irq_setup(struct platform_device *pdev, struct mtk_vcodec_dev *dev);

#endif /* _MTK_VCODEC_INTR_H_ */
