/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
