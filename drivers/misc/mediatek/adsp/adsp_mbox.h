/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_MBOX_H__
#define __ADSP_MBOX_H__

#include <linux/soc/mediatek/mtk-mbox.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#define ADSP_MBOX_TOTAL (5)

#define ADSP_MBOX0_CH_ID        0
#define ADSP_MBOX1_CH_ID        1
#define ADSP_MBOX2_CH_ID        2
#define ADSP_MBOX3_CH_ID        3
#define ADSP_MBOX4_CH_ID        4

#define ADSP_MBOX_SEND_IDX      0
#define ADSP_MBOX_RECV_IDX      1
#define ADSP_TOTAL_SEND_PIN     2
#define ADSP_TOTAL_RECV_PIN     2
#define ADSP_IPI_CH_CNT         (ADSP_TOTAL_SEND_PIN + ADSP_TOTAL_RECV_PIN)
#define ADSP_MBOX_SLOT_COUNT    64

#define ADSP_MBOX_SEND_SLOT_OFFSET  0
#define ADSP_MBOX_RECV_SLOT_OFFSET  0

#if IS_ENABLED(CONFIG_MTK_IRQ_DBG)
extern void mt_irq_dump_status(int irq);
#endif

void adsp_mbox_dump(void);
int adsp_mbox_send(struct mtk_mbox_pin_send *pin_send, void *msg,
		   unsigned int wait);
int adsp_mbox_probe(struct platform_device *pdev);
struct mtk_mbox_pin_send *get_adsp_mbox_pin_send(int index);
struct mtk_mbox_pin_recv *get_adsp_mbox_pin_recv(int index);
#endif  /* __ADSP_MBOX_H__ */
