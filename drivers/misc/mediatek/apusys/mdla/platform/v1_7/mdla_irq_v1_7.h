/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_V1_7_IRQ_H__
#define __MDLA_V1_7_IRQ_H__

#include <linux/device.h>

int mdla_v1_7_get_irq_num(int core_id);
int mdla_v1_7_irq_request(struct device *dev, int irqdesc_num);
int mdla_v1_7_irq_release(struct device *dev);

#endif /* __MDLA_V1_7_IRQ_H__ */

