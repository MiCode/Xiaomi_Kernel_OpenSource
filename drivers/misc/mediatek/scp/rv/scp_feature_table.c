// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "scp_feature_define.h"
#include "scp_ipi_pin.h"
#include "scp.h"

/*scp feature list*/
struct scp_feature_tb feature_table[NUM_FEATURE_ID] = {
/* VFFP:20 + default:5 */
	{
		.feature	= VOW_FEATURE_ID,
	},
	{
		.feature	= SENS_FEATURE_ID,
	},
	{
		.feature	= FLP_FEATURE_ID,
	},
	{
		.feature	= RTOS_FEATURE_ID,
	},
	{
		.feature	= SPEAKER_PROTECT_FEATURE_ID,
	},
	{
		.feature	= VCORE_TEST_FEATURE_ID,
	},
	{
		.feature	= VOW_BARGEIN_FEATURE_ID,
	},
	{
		.feature	= VOW_DUMP_FEATURE_ID,
	},
	{
		.feature        = VOW_VENDOR_M_FEATURE_ID,
	},
	{
		.feature        = VOW_VENDOR_A_FEATURE_ID,
	},
	{
		.feature        = VOW_VENDOR_G_FEATURE_ID,
	},
	{
		.feature        = VOW_DUAL_MIC_FEATURE_ID,
	},
	{
		.feature        = VOW_DUAL_MIC_BARGE_IN_FEATURE_ID,
	},
	{
		.feature        = ULTRA_FEATURE_ID,
		.freq           = 200,
		.enable         = 0,
		.sys_id         = SCPSYS_CORE0,
	},
};

/*scp sensor type list*/
struct scp_sub_feature_tb sensor_type_table[NUM_SENSOR_TYPE] = {
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ACCELEROMETER_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = MAGNETIC_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ORIENTATION_FEATURE_ID,
		.freq    = 500,
		.enable  = 0,
	},
	{
		.feature = GYROSCOPE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = LIGHT_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = PRESSURE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = TEMPERATURE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = PROXIMITY_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = GRAVITY_FEATURE_ID,
		.freq    = 500,
		.enable  = 0,
	},
	{
		.feature = LINEAR_ACCELERATION_FEATURE_ID,
		.freq    = 500,
		.enable  = 0,
	},
	{
		.feature = ROTATION_VECTOR_FEATURE_ID,
		.freq    = 500,
		.enable  = 0,
	},
	{
		.feature = RELATIVE_HUMIDITY_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = AMBIENT_TEMPERATURE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = MAGNETIC_UNCALIBRATED_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = GAME_ROTATION_VECTOR_FEATURE_ID,
		.freq    = 500,
		.enable  = 0,
	},
	{
		.feature = GYROSCOPE_UNCALIBRATED_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = SIGNIFICANT_MOTION_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = STEP_DETECTOR_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = STEP_COUNTER_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = GEOMAGNETIC_ROTATION_VECTOR_FEATURE_ID,
		.freq    = 500,
		.enable  = 0,
	},
	{
		.feature = HEART_RATE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = TILT_DETECTOR_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = WAKE_GESTURE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = GLANCE_GESTURE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = PICK_UP_GESTURE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = WRIST_TILT_GESTURE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = DEVICE_ORIENTATION_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = POSE_6DOF_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = STATIONARY_DETECT_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = MOTION_DETECT_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = HEART_BEAT_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = DYNAMIC_SENSOR_META_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ADDITIONAL_INFO_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = LOW_LATENCY_OFFBODY_DETECT_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ACCELEROMETER_UNCALIBRATED_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	/* unused sensor type 36~54*/
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = 0,
		.freq    = 0,
		.enable  = 0,
	},
	/* follow mtk add sensor type */
	{
		.feature = PEDOMETER_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = IN_POCKET_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ACTIVITY_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = PDR_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FREEFALL_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FLAT_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FACE_DOWN_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = SHAKE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = BRINGTOSEE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ANSWER_CALL_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = GEOFENCE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FLOOR_COUNTER_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = EKG_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = PPG1_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = PPG2_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = RGBW_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = GYRO_TEMPERATURE_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = SAR_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = OIS_FEATURE_ID,
		.freq    = 361,
		.enable  = 0,
	},
	{
		.feature = FLICKER_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = GYRO_SECONDARY_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FLICKER_REAR_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = RGBW_REAR_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = PS_FACTORY_STRM_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ALS_FACTORY_STRM_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ELEVATOR_DETECT_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FOD_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = AOD_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = NONUI_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
		{
		.feature = LUX_B_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FREE_FALL_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = LIGHT_SMD_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = SAR_ALGO_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = SAR_ALGO_1_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = SAR_ALGO_2_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = DBTAP_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = SAR_SECONDARY_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = ELLIPTIC_FUSION_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FRONT_CCT_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
	{
		.feature = FRONT_CCT_FACTORY_STRM_FEATURE_ID,
		.freq    = 0,
		.enable  = 0,
	},
};

