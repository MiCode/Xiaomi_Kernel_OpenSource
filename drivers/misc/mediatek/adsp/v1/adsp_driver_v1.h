/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef ADSP_MT6893_H
#define ADSP_MT6893_H

#include <linux/soc/mediatek/mtk-mbox.h>
#include "adsp_platform_driver.h"

/* symbol need from adsp.ko */
extern int adsp_system_bootup(void);
extern void register_adspsys(struct adspsys_priv *mt_adspsys);
extern void register_adsp_core(struct adsp_priv *pdata);

extern int adsp_mbox_probe(struct platform_device *pdev);
extern struct mtk_mbox_pin_send *get_adsp_mbox_pin_send(int index);
extern struct mtk_mbox_pin_recv *get_adsp_mbox_pin_recv(int index);

extern int adsp_mem_device_probe(struct platform_device *pdev);
extern int adsp_mbox_probe(struct platform_device *pdev);

extern int adsp_after_bootup(struct adsp_priv *pdata);
extern int adsp_core0_init(struct adsp_priv *pdata);
extern int adsp_core1_init(struct adsp_priv *pdata);

#endif /* ADSP_CLK_MT6893_H */
