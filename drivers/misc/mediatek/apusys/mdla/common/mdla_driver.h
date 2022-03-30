/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_DRIVER_H__
#define __MDLA_DRIVER_H__
#include <linux/types.h>

#include <apusys_device.h>

#define DRIVER_NAME "mtk_mdla"

#define DEVICE_MDLA     APUSYS_DEVICE_MDLA
#define DEVICE_MDLA_RT  APUSYS_DEVICE_MDLA_RT

struct device;

int mdla_drv_create_device_node(struct device *dev);
void mdla_drv_destroy_device_node(void);
int mdla_drv_init(void);
void mdla_drv_exit(void);

#endif /* __MDLA_DRIVER_H__ */

