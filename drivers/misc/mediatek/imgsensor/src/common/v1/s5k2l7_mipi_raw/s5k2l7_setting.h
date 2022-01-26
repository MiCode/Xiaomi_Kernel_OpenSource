/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k2l7_setting.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor setting file
 *
 ****************************************************************************/
#ifndef _s5k2l7MIPI_SETTING_H_
#define _s5k2l7MIPI_SETTING_H_

#include "kd_camera_typedef.h"
#include "s5k2l7_setting_mode1.h"
#include "s5k2l7_setting_mode2.h"
#include "s5k2l7_setting_mode3.h"
#include "s5k2l7_setting_mode1_v2.h"
#include "s5k2l7_setting_mode2_v2.h"
#include "s5k2l7_setting_mode3_v2.h"

#define HV_MIRROR_FLIP

#define S5K2L7_SENSOR_MODE 3	/* default m3 */

UINT16 pdaf_sensor_mode = S5K2L7_SENSOR_MODE;
UINT16 proc_pdaf_sensor_mode = S5K2L7_SENSOR_MODE;


#endif
