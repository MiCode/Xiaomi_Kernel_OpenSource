// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APU_PLATFORM_RESOURCE_H_
#define _APU_PLATFORM_RESOURCE_H_

#include <linux/platform_device.h>
#include "hal_config_power.h"

extern int init_platform_resource(struct platform_device *pdev,
struct hal_param_init_power *init_power_data);

#endif // _APU_PLATFORM_RESOURCE_H_
