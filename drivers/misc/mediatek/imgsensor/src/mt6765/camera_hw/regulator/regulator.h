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

#ifndef __IMGSENSOR_HW_REGULATOR_H__
#define __IMGSENSOR_HW_REGULATOR_H__
#include "imgsensor_common.h"

#include <linux/of.h>
#include <linux/device.h>
#include <linux/regulator/consumer.h>

#include "imgsensor_hw.h"
#include "imgsensor.h"


extern struct device *gimgsensor_device;
extern struct IMGSENSOR gimgsensor;

enum REGULATOR_VOLTAGE {
	REGULATOR_VOLTAGE_0    = 0,
	REGULATOR_VOLTAGE_1000 = 1000000,
	REGULATOR_VOLTAGE_1100 = 1100000,
	REGULATOR_VOLTAGE_1200 = 1200000,
	REGULATOR_VOLTAGE_1210 = 1210000,
	REGULATOR_VOLTAGE_1220 = 1220000,
	REGULATOR_VOLTAGE_1500 = 1500000,
	REGULATOR_VOLTAGE_1800 = 1800000,
	REGULATOR_VOLTAGE_2500 = 2500000,
	REGULATOR_VOLTAGE_2800 = 2800000,
	REGULATOR_VOLTAGE_2900 = 2900000,
};

enum REGULATOR_TYPE {
	REGULATOR_TYPE_MAIN_VCAMA,
	REGULATOR_TYPE_MAIN_VCAMD,
	REGULATOR_TYPE_MAIN_VCAMIO,
	REGULATOR_TYPE_MAIN_VCAMAF,
	REGULATOR_TYPE_SUB_VCAMA,
	REGULATOR_TYPE_SUB_VCAMD,
	REGULATOR_TYPE_SUB_VCAMIO,
	REGULATOR_TYPE_MAIN2_VCAMA,
	REGULATOR_TYPE_MAIN2_VCAMD,
	REGULATOR_TYPE_MAIN2_VCAMIO,
	REGULATOR_TYPE_SUB2_VCAMA,
	REGULATOR_TYPE_SUB2_VCAMD,
	REGULATOR_TYPE_SUB2_VCAMIO,
	REGULATOR_TYPE_MAIN3_VCAMA,
	REGULATOR_TYPE_MAIN3_VCAMD,
	REGULATOR_TYPE_MAIN3_VCAMIO,
	REGULATOR_TYPE_MAX_NUM
};

struct REGULATOR_CTRL {
	char *pregulator_type;
};

struct REGULATOR {
	struct regulator *pregulator[REGULATOR_TYPE_MAX_NUM];
	atomic_t    enable_cnt[REGULATOR_TYPE_MAX_NUM];
	pid_t pid;
};

enum IMGSENSOR_RETURN imgsensor_hw_regulator_open(
	struct IMGSENSOR_HW_DEVICE **pdevice);

#endif

