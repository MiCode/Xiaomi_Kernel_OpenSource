/*
 * power_hal_sysfs.h: Early suspend sysfs header file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Sathyanarayanan KN(sathyanarayanan.kuppuswamy@intel.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef _LINUX_POWER_HAL_SUSPEND_SYSFS_H
#define _LINUX_POWER_HAL_SUSPEND_SYSFS_H

#define POWER_HAL_SUSPEND_STATUS_LEN 1
#define POWER_HAL_SUSPEND_ON  "1"
#define POWER_HAL_SUSPEND_OFF "0"

int register_power_hal_suspend_device(struct device *dev);
void unregister_power_hal_suspend_device(struct device *dev);
#endif

