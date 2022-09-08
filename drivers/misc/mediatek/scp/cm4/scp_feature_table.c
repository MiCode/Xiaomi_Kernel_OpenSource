// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "scp_feature_define.h"
#include "scp_ipi.h"
/*scp feature list*/
struct scp_feature_tb feature_table[NUM_FEATURE_ID] = {
	{
		.feature     = VOW_FEATURE_ID,
		.freq        = 47,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = OPEN_DSP_FEATURE_ID,
		.freq        = 356,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = SENS_FEATURE_ID,
		.freq        = 62,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = MP3_FEATURE_ID,
		.freq        = 47,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = FLP_FEATURE_ID,
		.freq        = 26,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = RTOS_FEATURE_ID,
		.freq        = 0,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = SPEAKER_PROTECT_FEATURE_ID,
		.freq        = 200,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = VCORE_TEST_FEATURE_ID,
		.freq        = 0,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = VOW_BARGEIN_FEATURE_ID,
		.freq        = 100,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = VOW_DUMP_FEATURE_ID,
		.freq        = 10,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = VOW_VENDOR_M_FEATURE_ID,
		.freq        = 43,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = VOW_VENDOR_A_FEATURE_ID,
		.freq        = 43,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = VOW_VENDOR_G_FEATURE_ID,
		.freq        = 22,
		.enable      = 0,
		.sub_feature = 0,
	},
	{
		.feature     = ULTRA_FEATURE_ID,
		.freq        = 200,
		.enable      = 0,
		.sub_feature = 0,
	},
};


/*scp sensor type list*/
struct scp_sub_feature_tb sensor_type_table[NUM_SENSOR_TYPE] = {
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

MODULE_LICENSE("GPL");

