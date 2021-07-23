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

#ifndef __IMGSENSOR_HW_MT6306_h__
#define __IMGSENSOR_HW_MT6306_h__

/* #include "mtk_6306_gpio.h" */
#include "imgsensor_hw.h"
#include "imgsensor_common.h"

enum MT6306_PIN {
	MT6306_PIN_CAM_PDN0,
	MT6306_PIN_CAM_RST0,
	MT6306_PIN_CAM_PDN1,
	MT6306_PIN_CAM_RST1,
	MT6306_PIN_CAM_PDN2,
	MT6306_PIN_CAM_RST2,
	MT6306_PIN_CAM_EXT_PWR_EN,
	MT6306_PIN_MAX_NUM
};

struct MT6306 {
	atomic_t    enable_cnt[MT6306_PIN_MAX_NUM];
};

enum IMGSENSOR_RETURN imgsensor_hw_mt6306_open(
	struct IMGSENSOR_HW_DEVICE **pdevice);

#endif

