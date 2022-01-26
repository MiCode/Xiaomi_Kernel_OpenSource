/*
 * Copyright (C) 2017 Hoperun Inc.
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

#ifndef __IMGSENSOR_CAMINFO_H__
#define __IMGSENSOR_CAMINFO_H__
#define IMGSENSOR_DEVICE_NNUMBER 255

#include "imgsensor_common.h"
#include "imgsensor_i2c.h"
#include "imgsensor_hw.h"

extern unsigned int s5kjn1sunny_get_otpdata(unsigned char *data, u16 i2cId);
extern unsigned int ov50c40ofilm_get_otpdata(unsigned char *data, u16 i2cId);
extern unsigned int ov16a1qofilm_get_otpdata(unsigned char *data, u16 i2cId);
extern unsigned int ov16a1qqtech_get_otpdata(unsigned char *data, u16 i2cId);
extern unsigned int imx355sunny_get_otpdata(unsigned char *data, u16 i2cId);
extern unsigned int imx355ofilm_get_otpdata(unsigned char *data, u16 i2cId);

typedef unsigned int (*imgsensor_hwinfo_get_func) (unsigned char *data, u16 i2cId);

MINT8 imgsensor_sensor_hw_register(struct IMGSENSOR_SENSOR *psensor, MUINT32 sensorID);

#endif
