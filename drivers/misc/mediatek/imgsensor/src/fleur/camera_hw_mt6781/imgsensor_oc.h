/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __IMGSENSOR_OC_H__
#define __IMGSENSOR_OC_H__

#include "imgsensor.h"
#include "imgsensor_common.h"
#include "imgsensor_hw.h"

#define IMGSENSOR_OC_ENABLE

enum IMGSENSOR_RETURN imgsensor_oc_init(void);
enum IMGSENSOR_RETURN
	imgsensor_oc_interrupt(enum IMGSENSOR_HW_POWER_STATUS pwr_status);

extern struct IMGSENSOR gimgsensor;

#endif

