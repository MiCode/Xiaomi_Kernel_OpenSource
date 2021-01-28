/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMGSENSOR_PWR_CTRL_H__
#define __IMGSENSOR_PWR_CTRL_H__

#include "imgsensor_sensor.h"
#include "imgsensor_cfg_table.h"
#include "imgsensor_common.h"

enum IMGSENSOR_HW_POWER_STATUS {
	IMGSENSOR_HW_POWER_STATUS_OFF,
	IMGSENSOR_HW_POWER_STATUS_ON
};

struct IMGSENSOR_HW_SENSOR_POWER {
	struct IMGSENSOR_HW_POWER_INFO *ppwr_info;
	enum   IMGSENSOR_HW_ID          id[IMGSENSOR_HW_PIN_MAX_NUM];
};

struct IMGSENSOR_HW {
	struct IMGSENSOR_HW_DEVICE      *pdev[IMGSENSOR_HW_ID_MAX_NUM];
	struct IMGSENSOR_HW_SENSOR_POWER
	    sensor_pwr[IMGSENSOR_SENSOR_IDX_MAX_NUM];
	const char *enable_sensor_by_index[IMGSENSOR_SENSOR_IDX_MAX_NUM];
};

enum IMGSENSOR_RETURN imgsensor_hw_init(struct IMGSENSOR_HW *phw);
enum IMGSENSOR_RETURN imgsensor_hw_release_all(struct IMGSENSOR_HW *phw);
enum IMGSENSOR_RETURN imgsensor_hw_power(
	struct IMGSENSOR_HW *phw,
	struct IMGSENSOR_SENSOR *psensor,
	char *curr_sensor_name,
	enum IMGSENSOR_HW_POWER_STATUS pwr_status);

#endif

