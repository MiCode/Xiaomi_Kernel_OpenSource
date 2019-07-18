/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_HAL_H__
#define __GED_HAL_H__

#include "ged_type.h"

#define	GED_GPU_INFO_CAPABILITY 20
#define GED_GPU_INFO_RUNTIME 21


GED_ERROR ged_hal_init(void);

void ged_hal_exit(void);

#endif
