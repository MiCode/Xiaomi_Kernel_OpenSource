/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_V2_0_IRQ_H__
#define __MDLA_V2_0_IRQ_H__

#include <linux/types.h>
#include <linux/device.h>

int mdla_v2_0_get_irq_num(u32 core_id);
int mdla_v2_0_irq_request(struct device *dev, int irqdesc_num);
int mdla_v2_0_irq_release(struct device *dev);

#endif /* __MDLA_V2_0_IRQ_H__ */

