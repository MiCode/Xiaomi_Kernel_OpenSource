/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __KD_SENSORLIST_H__
#define __KD_SENSORLIST_H__

#include "kd_camera_typedef.h"
#include "imgsensor_sensor.h"

struct IMGSENSOR_INIT_FUNC_LIST {
	MUINT32   id;
	MUINT8    name[32];
	MUINT32 (*init)(struct SENSOR_FUNCTION_STRUCT **pfFunc);
};

/*M505 imagesensor*/
UINT32 HYNIX_HI1337_I_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HYNIX_HI1337_II_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HYNIX_HI1337_III_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC_GC02M1_II_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 OV_OV02B_I_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 OV_OV02B_III_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HYNIX_HI259_I_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HYNIX_HI259_II_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HYNIX_HI259_III_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC_GC5035_I_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC_GC5035_II_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC_GC5035_III_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 GC02M1MACRO_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);

extern struct IMGSENSOR_INIT_FUNC_LIST kdSensorList[];

#endif

