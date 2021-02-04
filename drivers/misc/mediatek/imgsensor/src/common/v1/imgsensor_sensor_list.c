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

#include "kd_imgsensor.h"
#include "imgsensor_sensor_list.h"

/* Add Sensor Init function here
 * Note:
 * 1. Add by the resolution from ""large to small"", due to large sensor
 *    will be possible to be main sensor.
 *    This can avoid I2C error during searching sensor.
 * 2. This should be the same as
 *     mediatek\custom\common\hal\imgsensor\src\sensorlist.cpp
 */
struct IMGSENSOR_INIT_FUNC_LIST kdSensorList[MAX_NUM_OF_SUPPORT_SENSOR] = {
 /*M510 imagesensor*/
#if defined(HYNIX_HI1337_I)
        {HYNIX_HI1337_I_SENSOR_ID,
        SENSOR_DRVNAME_HYNIX_HI1337_I,
        HYNIX_HI1337_I_SensorInit},
#endif
#if defined(HYNIX_HI1337_II)
        {HYNIX_HI1337_II_SENSOR_ID,
        SENSOR_DRVNAME_HYNIX_HI1337_II,
        HYNIX_HI1337_II_SensorInit},
#endif
#if defined(HYNIX_HI1337_III)
        {HYNIX_HI1337_III_SENSOR_ID,
        SENSOR_DRVNAME_HYNIX_HI1337_III,
        HYNIX_HI1337_III_SensorInit},
#endif
#if defined(GC_GC5035_I)
        {GC_GC5035_I_SENSOR_ID,
        SENSOR_DRVNAME_GC_GC5035_I,
        GC_GC5035_I_SensorInit},
#endif
#if defined(GC_GC5035_II)
        {GC_GC5035_II_SENSOR_ID,
        SENSOR_DRVNAME_GC_GC5035_II,
        GC_GC5035_II_SensorInit},
#endif
#if defined(GC_GC5035_III)
        {GC_GC5035_III_SENSOR_ID,
        SENSOR_DRVNAME_GC_GC5035_III,
        GC_GC5035_III_SensorInit},
#endif
#if defined(HYNIX_HI259_I)
        {HYNIX_HI259_I_SENSOR_ID,
        SENSOR_DRVNAME_HYNIX_HI259_I,
        HYNIX_HI259_I_SensorInit},
#endif
#if defined(HYNIX_HI259_II)
        {HYNIX_HI259_II_SENSOR_ID,
        SENSOR_DRVNAME_HYNIX_HI259_II,
        HYNIX_HI259_II_SensorInit},
#endif
#if defined(OV_OV02B_I)
        {OV02B_SENSOR_ID,
        SENSOR_DRVNAME_OV_OV02B_I,
        OV_OV02B_I_SensorInit},
#endif
#if defined(OV_OV02B_III)
        {OV02B_III_SENSOR_ID,
        SENSOR_DRVNAME_OV_OV02B_III,
        OV_OV02B_III_SensorInit},
#endif
#if defined(GC_GC02M1_II)
        {GC02M1_SENSOR_ID,
        SENSOR_DRVNAME_GC_GC02M1_II,
        GC_GC02M1_II_SensorInit},
#endif
#if defined(HYNIX_HI259_III)
        {HYNIX_HI259_III_SENSOR_ID,
        SENSOR_DRVNAME_HYNIX_HI259_III,
        HYNIX_HI259_III_SensorInit},
#endif

	/*  ADD sensor driver before this line */
	{0, {0}, NULL}, /* end of list */
};
/* e_add new sensor driver here */

