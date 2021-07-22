// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "vcp_feature_define.h"
#include "vcp_ipi_pin.h"
#include "vcp.h"

/*vcp feature list*/
struct vcp_feature_tb feature_table[NUM_FEATURE_ID] = {
/* VFFP:20 + default:5 */
	{
		.feature	= SENS_FEATURE_ID,
		.freq		= 29,
		.enable		= 0,
		.sys_id		= VCPSYS_CORE0,
	},
	{
		.feature	= RTOS_FEATURE_ID,
		.freq		= 0,
		.enable		= 0,
		.sys_id		= VCPSYS_CORE0,
	},
	{
		.feature	= VCORE_TEST_FEATURE_ID,
		.freq		= 77,
		.enable		= 0,
		.sys_id		= VCPSYS_CORE0,
	},
};

/*vcp sensor type list*/
struct vcp_sub_feature_tb sensor_type_table[NUM_SENSOR_TYPE] = {
	{
		.feature = ACCELEROMETER_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = MAGNETIC_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = ORIENTATION_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = GYROSCOPE_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = LIGHT_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = PROXIMITY_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = PRESSURE_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = STEP_COUNTER_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = SIGNIFICANT_MOTION_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = STEP_DETECTOR_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = GLANCE_GESTURE_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = ANSWER_CALL_FEATURE_ID,
		.freq    = 3,
		.enable  = 0,
	},
	{
		.feature = SHAKE_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = STATIONARY_DETECT_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = MOTION_DETECT_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = IN_POCKET_FEATURE_ID,
		.freq    = 3,
		.enable  = 0,
	},
	{
		.feature = SHAKE_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = DEVICE_ORIENTATION_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
	{
		.feature = ACTIVITY_FEATURE_ID,
		.freq    = 3,
		.enable  = 0,
	},
};

