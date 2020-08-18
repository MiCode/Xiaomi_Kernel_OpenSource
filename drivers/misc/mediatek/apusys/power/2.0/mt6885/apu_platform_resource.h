/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _APU_PLATFORM_RESOURCE_H_
#define _APU_PLATFORM_RESOURCE_H_

#include <linux/platform_device.h>
#include "hal_config_power.h"

extern int init_platform_resource(struct platform_device *pdev,
struct hal_param_init_power *init_power_data);

#endif // _APU_PLATFORM_RESOURCE_H_
