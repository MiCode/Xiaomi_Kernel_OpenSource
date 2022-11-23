/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __IMGSENSOR_COMMON_H__
#define __IMGSENSOR_COMMON_H__

#define PREFIX "[imgsensor]"

#define pr_fmt(fmt) PREFIX "[%s] " fmt, __func__

#include "kd_camera_feature.h"
#include "kd_imgsensor_define.h"

/************************************************************************
 * Debug configuration
 ************************************************************************/
#define PREFIX "[imgsensor]"
#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG(fmt, arg...)  pr_debug(PREFIX fmt, ##arg)
#define PK_INFO(fmt, arg...) pr_debug(PREFIX fmt, ##arg)
#else
#define PK_DBG(fmt, arg...)
#define PK_INFO(fmt, arg...) pr_debug(PREFIX fmt, ##arg)
#endif

#define PLATFORM_POWER_SEQ_NAME "platform_power_seq"
#define DEBUG_CAMERA_HW_K

#define IMGSENSOR_LEGACY_COMPAT
#define IMGSENSOR_TOSTRING(value)           #value
#define IMGSENSOR_STRINGIZE(stringizedName) IMGSENSOR_TOSTRING(stringizedName)

enum IMGSENSOR_ARCH {
	IMGSENSOR_ARCH_V1 = 0,
	IMGSENSOR_ARCH_V2,
	IMGSENSOR_ARCH_V3
};

enum IMGSENSOR_RETURN {
	IMGSENSOR_RETURN_SUCCESS = 0,
	IMGSENSOR_RETURN_ERROR   = -1,
};

#define LENGTH_FOR_SNPRINTF 256
#endif

