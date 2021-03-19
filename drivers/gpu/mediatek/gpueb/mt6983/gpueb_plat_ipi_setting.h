// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

#ifndef __GPUEB_PLAT_IPI_SETTING_H__
#define __GPUEB_PLAT_IPI_SETTING_H__

#define PIN_R_SIZE 20
#define PIN_S_SIZE 20
#define GPUEB_MBOX_TOTAL 4
#define GPUEB_SLOT_NUM_PER_MBOX (PIN_R_SIZE + PIN_S_SIZE)

#define CH_PLATFORM	0
#define CH_DVFS	    1
#define CH_SLEEP	2
#define CH_TIMER	3
#define GPUEB_IPI_COUNT	4
#define GPUEB_TOTAL_SEND_PIN     GPUEB_IPI_COUNT
#define GPUEB_TOTAL_RECV_PIN     GPUEB_IPI_COUNT

#define PIN_R_SIZE_PLATFORM     PIN_R_SIZE
#define PIN_R_SIZE_DVFS         PIN_R_SIZE
#define PIN_R_SIZE_SLEEP        PIN_R_SIZE
#define PIN_R_SIZE_TIMER        PIN_R_SIZE

#define PIN_S_SIZE_PLATFORM     PIN_S_SIZE
#define PIN_S_SIZE_DVFS         PIN_S_SIZE
#define PIN_S_SIZE_SLEEP        PIN_S_SIZE
#define PIN_S_SIZE_TIMER        PIN_S_SIZE

#define PIN_S_OFFSET_PLATFORM   0
#define PIN_S_OFFSET_DVFS       0
#define PIN_S_OFFSET_SLEEP      0
#define PIN_S_OFFSET_TIMER      0

#define PIN_R_OFFSET_PLATFORM   (PIN_S_OFFSET_PLATFORM  + PIN_S_SIZE_PLATFORM)
#define PIN_R_OFFSET_DVFS       (PIN_S_OFFSET_DVFS      + PIN_S_SIZE_DVFS)
#define PIN_R_OFFSET_SLEEP      (PIN_S_OFFSET_SLEEP     + PIN_S_SIZE_SLEEP)
#define PIN_R_OFFSET_TIMER      (PIN_S_OFFSET_TIMER     + PIN_S_SIZE_TIMER)

#define PIN_R_MSG_SIZE_PLATFORM 1 // 1 slot,    4 bytes
#define PIN_R_MSG_SIZE_DVFS     4 // 4 slots,   16 bytes
#define PIN_R_MSG_SIZE_SLEEP    1 // 1 slot,    4 bytes
#define PIN_R_MSG_SIZE_TIMER    1 // 1 slot,    4 bytes

#define PIN_S_MSG_SIZE_PLATFORM 4 // 4 slots,   16 bytes
#define PIN_S_MSG_SIZE_DVFS     4 // 4 slots,   16 bytes
#define PIN_S_MSG_SIZE_SLEEP    3 // 3 slots,   12 bytes
#define PIN_S_MSG_SIZE_TIMER    3 // 3 slots,   12 bytes

extern struct mtk_mbox_info gpueb_plat_mbox_table[GPUEB_MBOX_TOTAL];
extern struct mtk_mbox_pin_send gpueb_plat_mbox_pin_send[GPUEB_IPI_COUNT];
extern struct mtk_mbox_pin_recv gpueb_plat_mbox_pin_recv[GPUEB_IPI_COUNT];
extern struct mtk_mbox_device gpueb_plat_mboxdev;
extern struct mtk_ipi_device gpueb_plat_ipidev;

#endif /* __GPUEB_PLAT_IPI_SETTING_H__ */