// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_IPI_H__
#define __GPUEB_IPI_H__

// Common implementation
#define PLT_INIT           0x504C5401
#define PLT_LOG_ENABLE     0x504C5402
#define IPI_TIMEOUT_MS     3000U
#define IPI_SUPPORT        0

extern struct mtk_mbox_device   gpueb_mboxdev;
extern struct mtk_ipi_device    gpueb_ipidev;
extern struct mtk_mbox_info     *gpueb_mbox_info;
extern struct mtk_mbox_pin_send *gpueb_mbox_pin_send;
extern struct mtk_mbox_pin_recv *gpueb_mbox_pin_recv;
extern const char *gpueb_mbox_pin_send_name[20];
extern const char *gpueb_mbox_pin_recv_name[20];

int gpueb_ipi_init(struct platform_device *pdev);
int gpueb_get_send_PIN_ID_by_name(char *send_PIN_name);
int gpueb_get_recv_PIN_ID_by_name(char *recv_PIN_name);
void *get_gpueb_ipidev(void);

#endif /* __GPUEB_IPI_H__ */