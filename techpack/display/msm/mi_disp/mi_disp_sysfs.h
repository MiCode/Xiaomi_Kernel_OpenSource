/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DISP_SYSFS_H_
#define _MI_DISP_SYSFS_H_

#include <linux/device.h>

int mi_disp_create_device_attributes(struct device *dev);
void mi_disp_remove_device_attributes(struct device *dev);

#endif /* _MI_DISP_SYSFS_H_ */
