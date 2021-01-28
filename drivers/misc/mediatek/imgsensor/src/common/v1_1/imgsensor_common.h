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

#ifndef __IMGSENSOR_COMMON_H__
#define __IMGSENSOR_COMMON_H__

#include "kd_camera_feature.h"
#include "kd_imgsensor_define.h"

/******************************************************************************
 * Debug configuration
 ******************************************************************************/
#define PREFIX "[imgsensor]"
#define PLATFORM_POWER_SEQ_NAME "platform_power_seq"

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG(fmt, arg...)  pr_debug(PREFIX fmt, ##arg)
#define PK_PR_ERR(fmt, arg...)  pr_err(fmt, ##arg)
#define PK_INFO(fmt, arg...) pr_debug(PREFIX fmt, ##arg)
#else
#define PK_DBG(fmt, arg...)
#define PK_PR_ERR(fmt, arg...)  pr_err(fmt, ##arg)
#define PK_INFO(fmt, arg...) pr_debug(PREFIX fmt, ##arg)
#endif

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

