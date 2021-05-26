/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __IMGSENSOR_HW_SMARTLOD_H__
#define __IMGSENSOR_HW_SMARTLOD_H__

#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "imgsensor_hw.h"
#include "imgsensor_common.h"

enum SMARTLDO_VOLTAGE {
	SMARTLDO_VOLTAGE_0    = 0,
	SMARTLDO_VCAMD_DVDD_1150 = 1150000,
	SMARTLDO_VCAMA_MAIN_2800 = 2800000,
	SMARTLDO_VCAMD_DVDD_1200 = 1200000,
	SMARTLDO_VCAMA_AVDD_2800 = 2800000,
};

enum SMARTLDO_TYPE {
	SMARTLDO_TYPE_VCAMA,
	SMARTLDO_TYPE_VCAMD,
	SMARTLDO_TYPE_VCAMIO,
	SMARTLDO_TYPE_MAX_NUM
};

struct SMARTLDO_CTRL {
	char *psmartldo_type;
};


struct smartldo {

};

struct SMARTLDO {
	struct smartlod *psmartldo[
		IMGSENSOR_SENSOR_IDX_MAX_NUM][SMARTLDO_TYPE_MAX_NUM];
	atomic_t          enable_cnt[
		IMGSENSOR_SENSOR_IDX_MAX_NUM][SMARTLDO_TYPE_MAX_NUM];
};

enum IMGSENSOR_RETURN imgsensor_hw_smartldo_open(
	struct IMGSENSOR_HW_DEVICE **pdevice);

#endif

