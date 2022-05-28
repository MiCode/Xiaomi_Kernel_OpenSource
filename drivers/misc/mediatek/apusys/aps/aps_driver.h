/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __APS_DRIVER_H__
#define __APS_DRIVER_H__
#include "apusys_device.h"
#include "apusys_core.h"

// static struct apusys_device* aps_adev;
// static struct aps_device* aps_dev;

int aps_init(struct apusys_core_info *info);
void aps_exit(void);

#endif /* !__APS_DRIVER_H__ */
