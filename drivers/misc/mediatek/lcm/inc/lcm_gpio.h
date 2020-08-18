/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __LCM_GPIO_H__
#define __LCM_GPIO_H__

#include "lcm_drv.h"
#include "lcm_common.h"


#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
#define LCM_GPIO_DEVICE	"lcm_mode"


enum LCM_STATUS lcm_gpio_set_data(char type, const struct LCM_DATA_T1 *t1);
#endif

#endif

