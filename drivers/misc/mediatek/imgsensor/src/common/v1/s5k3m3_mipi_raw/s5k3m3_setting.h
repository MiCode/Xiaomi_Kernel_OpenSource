/*
 * Copyright (C) 2016 MediaTek Inc.
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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k3m3_setting.h
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
#ifndef _s5k3m3MIPI_SETTING_H_
#define _s5k3m3MIPI_SETTING_H_

/*===FEATURE SWITCH===*/
/* #define FPTPDAFSUPPORT   //for pdaf switch */

/* #define NONCONTINUEMODE */
/*===FEATURE SWITCH===*/
#define VCPDAF
/* Open VCPDAF_PRE when preview mode need PDAF VC */
/* #define VCPDAF_PRE */

#include "kd_camera_typedef.h"
#include "s5k3m3_setting_v1.h"
#include "s5k3m3_setting_v2.h"


#define S5K3M3_MODULE_ID_V1 0xA001
#define S5K3M3_MODULE_ID_V2 0xA101

enum {
	S5K3M3_VERSION_ID_V1 = 0,
	S5K3M3_VERSION_ID_V2,
	S5K3M3_VERSION_NUM
};


static void sensor_init_v1(void);
static void preview_setting_v1(kal_uint16 fps);
static void capture_setting_v1(kal_uint16 currefps);
static void hs_video_setting_v1(kal_uint16 fps);
static void slim_video_setting_v1(kal_uint16 fps);
static void custom1_setting_v1(kal_uint16 fps);
static void custom2_setting_v1(kal_uint16 fps);


static void sensor_init_v2(void);
static void preview_setting_v2(kal_uint16 fps);
static void capture_setting_v2(kal_uint16 currefps);
static void hs_video_setting_v2(kal_uint16 fps);
static void slim_video_setting_v2(kal_uint16 fps);
static void custom1_setting_v2(kal_uint16 fps);
static void custom2_setting_v2(kal_uint16 fps);


#endif
