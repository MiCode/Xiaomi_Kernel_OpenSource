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

#ifndef __GED_HAL_H__
#define __GED_HAL_H__

#include "ged_type.h"

#define	GED_GPU_INFO_CAPABILITY 20
#define GED_GPU_INFO_RUNTIME 21


GED_ERROR ged_hal_init(void);

void ged_hal_exit(void);

#endif
