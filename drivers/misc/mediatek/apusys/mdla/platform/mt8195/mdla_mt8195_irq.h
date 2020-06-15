/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_MT8195_IRQ_H__
#define __MDLA_MT8195_IRQ_H__

#include <linux/device.h>

int mdla_mt8195_get_irq_num(int core_id);
int mdla_mt8195_irq_request(struct device *dev, int irqdesc_num);
int mdla_mt8195_irq_release(struct device *dev);

#endif /* __MDLA_MT8195_IRQ_H__ */

