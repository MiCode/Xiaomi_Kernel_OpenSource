/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __IMGSENSOR_HW_MT6306_h__
#define __IMGSENSOR_HW_MT6306_h__

#include "mtk_6306_gpio.h"
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
	atomic_t enable_cnt[MT6306_PIN_MAX_NUM];
};

enum IMGSENSOR_RETURN imgsensor_hw_mt6306_open(
	struct IMGSENSOR_HW_DEVICE **pdevice);

#endif

