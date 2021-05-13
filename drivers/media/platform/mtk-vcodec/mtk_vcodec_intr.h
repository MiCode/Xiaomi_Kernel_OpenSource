/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_INTR_H_
#define _MTK_VCODEC_INTR_H_

#include <linux/interrupt.h>
#include <linux/irq.h>
#include "mtk_vcodec_drv.h"
#include <linux/platform_device.h>

#define MTK_INST_IRQ_RECEIVED           0x1

#define MTK_VDEC_HW_ACTIVE      0x10
#define MTK_VDEC_IRQ_CFG        0x11
#define MTK_VDEC_IRQ_CLR        0x10
#define MTK_VDEC_IRQ_CFG_REG    0xA4
#define MTK_VDEC_IRQ_STATUS_DEC_SUCCESS        0x10000

#define MTK_VENC_IRQ_STATUS_SPS 0x1
#define MTK_VENC_IRQ_STATUS_PPS 0x2
#define MTK_VENC_IRQ_STATUS_FRM 0x4
#define MTK_VENC_IRQ_STATUS_DRAM        0x8
#define MTK_VENC_IRQ_STATUS_PAUSE       0x10
#define MTK_VENC_IRQ_STATUS_SWITCH      0x20
#define MTK_VENC_IRQ_STATUS_VPS         0x80

#define MTK_VENC_IRQ_STATUS_OFFSET      0x05C
#define MTK_VENC_IRQ_ACK_OFFSET 0x060

struct mtk_vcodec_ctx;

/* timeout is ms */
int mtk_vcodec_wait_for_done_ctx(struct mtk_vcodec_ctx  *ctx,
	int core_id, int command, unsigned int timeout_ms);

irqreturn_t mtk_vcodec_dec_irq_handler(int irq, void *priv);
irqreturn_t mtk_vcodec_dec_lat_irq_handler(int irq, void *priv);
irqreturn_t mtk_vcodec_enc_irq_handler(int irq, void *priv);
int mtk_vcodec_dec_irq_setup(struct platform_device *pdev,
	struct mtk_vcodec_dev *dev);
int mtk_vcodec_enc_irq_setup(struct platform_device *pdev,
	struct mtk_vcodec_dev *dev);

void mtk_vcodec_gce_timeout_dump(void *ctx);
void mtk_vcodec_enc_timeout_dump(void *ctx);
void mtk_vcodec_dec_timeout_dump(void *ctx);

#endif /* _MTK_VCODEC_INTR_H_ */
