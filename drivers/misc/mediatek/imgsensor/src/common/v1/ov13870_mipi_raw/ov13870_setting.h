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

#ifndef _OV13870MIPI_SETTING_H_
#define _OV13870MIPI_SETTING_H_

/*******************************************************************************
 * Log
 *******************************************************************************/
#define PFX "OV13870"
#define LOG_INF_NEW(format, args...)                                           \
	pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF LOG_INF_NEW
#define LOG_1 LOG_INF("OV13870,MIPI 4LANE\n")
#define SENSORDB LOG_INF

/*******************************************************************************
 * Proifling
 *******************************************************************************/
#define PROFILE 1
#if PROFILE
static struct timeval tv1, tv2;
static DEFINE_SPINLOCK(kdsensor_drv_lock);
/*******************************************************************************
 *
 *******************************************************************************/
static void KD_SENSOR_PROFILE_INIT(void) { do_gettimeofday(&tv1); }

/*******************************************************************************
 *
 *******************************************************************************/
static void KD_SENSOR_PROFILE(char *tag)
{
	unsigned long TimeIntervalUS;

	spin_lock(&kdsensor_drv_lock);

	do_gettimeofday(&tv2);
	TimeIntervalUS =
	    (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
	tv1 = tv2;

	spin_unlock(&kdsensor_drv_lock);
	LOG_INF("[%s]Profile = %lu us\n", tag, TimeIntervalUS);
}
#else
static void KD_SENSOR_PROFILE_INIT(void) {}

static void KD_SENSOR_PROFILE(char *tag) {}
#endif

/* Sensor defines */

#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN 765 /* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3
#endif

/* Sensor module version setting */
#include "ov13870_setting_v1.h"

enum { OV13870_VERSION_ID_V1 = 0, OV13870_VERSION_NUM };

enum { PDAF_NO_PDAF = 1, PDAF_VC_TYPE, PDAF_RAW_TYPE };

#define DEFAULT_PDAF_TYPE PDAF_VC_TYPE

UINT16 pdaf_sensor_type = DEFAULT_PDAF_TYPE;
UINT16 proc_pdaf_sensor_type = DEFAULT_PDAF_TYPE;

static void sensor_init_v1(void);
static void preview_setting_v1(void);
static void capture_setting_v1(void);
static void capture_setting_pdaf_raw_v1(void);
static void capture_setting_pdaf_vc_v1(void);
static void normal_video_setting_v1(void);
static void hs_video_setting_v1(void);
static void slim_video_setting_v1(void);
static void custom1_setting_v1(void);
static void custom2_setting_v1(void);
static void custom3_setting_v1(void);
static void custom4_setting_v1(void);
static void custom5_setting_v1(void);

#endif
