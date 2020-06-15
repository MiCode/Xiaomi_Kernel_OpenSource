/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_DRIVER_H__
#define __MDLA_DRIVER_H__
#include <linux/types.h>

#include <apusys_device.h>

#define DRIVER_NAME "mtk_mdla"

#define ONLY_MDLA_MODULE 0

#ifdef CONFIG_MTK_APUSYS_RT_SUPPORT
#define DEVICE_MDLA     APUSYS_DEVICE_MDLA
#define DEVICE_MDLA_RT  APUSYS_DEVICE_MDLA_RT
#else
#define DEVICE_MDLA     APUSYS_DEVICE_MDLA
#define DEVICE_MDLA_RT  APUSYS_DEVICE_MDLA
#endif

int mdla_drv_init(void);
void mdla_drv_exit(void);

#endif /* __MDLA_DRIVER_H__ */

