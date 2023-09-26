/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/module.h>       /* needed by all modules */
#include "scp_feature_define.h"
#include "scp_ipi_pin.h"


/*scp feature list*/
struct scp_feature_tb feature_table[NUM_FEATURE_ID] = {
/* VFFP:20 + default:5 */
	{
		.feature	= VOW_FEATURE_ID,
		.freq		= 5,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE1,
	},
	{
		.feature	= SENS_FEATURE_ID,
		.freq		= 29,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE0,
	},
	{
		.feature	= FLP_FEATURE_ID,
		.freq		= 26,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE0,
	},
	{
		.feature	= RTOS_FEATURE_ID,
		.freq		= 0,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE0,
	},
	{
		.feature	= SPEAKER_PROTECT_FEATURE_ID,
		.freq		= 200,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE1,
	},
	{
		.feature	= VCORE_TEST_FEATURE_ID,
		.freq		= 0,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE0,
	},
	{
		.feature        = VOW_BARGEIN_FEATURE_ID,
		.freq           = 120,
		.enable         = 0,
		.sys_id         = SCPSYS_CORE1,
	},
	{
		.feature	= VOW_DUMP_FEATURE_ID,
		.freq		= 10,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE1,
	},
	{
		.feature        = VOW_VENDOR_M_FEATURE_ID,
		.freq           = 80,
		.enable         = 0,
		.sys_id         = SCPSYS_CORE1,
	},
	{
		.feature        = VOW_VENDOR_A_FEATURE_ID,
		.freq           = 43,
		.enable         = 0,
		.sys_id         = SCPSYS_CORE1,
	},
	{
		.feature        = VOW_VENDOR_G_FEATURE_ID,
		.freq           = 22,
		.enable         = 0,
		.sys_id         = SCPSYS_CORE1,
	},
	{
		.feature        = VOW_DUAL_MIC_FEATURE_ID,
		.freq           = 20,
		.enable         = 0,
		.sys_id         = SCPSYS_CORE1,
	},
	{
		.feature        = VOW_DUAL_MIC_BARGE_IN_FEATURE_ID,
		.freq           = 135,
		.enable         = 0,
		.sys_id         = SCPSYS_CORE1,
	},
#ifdef CONFIG_MTK_ULTRASND_PROXIMITY
	{
		.feature        = ULTRA_FEATURE_ID,
		.freq           = 200,
		.enable         = 0,
		.sys_id         = SCPSYS_CORE0,
	},
#endif
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

