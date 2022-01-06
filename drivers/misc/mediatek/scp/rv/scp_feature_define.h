/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SCP_FEATURE_DEFINE_H__
#define __SCP_FEATURE_DEFINE_H__

#include "scp.h"

/* scp platform configs*/
#define SCP_BOOT_TIME_OUT_MONITOR        (1)
#define SCP_RESERVED_MEM                 (1)
#ifdef CONFIG_MTK_TINYSYS_SCP_LOGGER_SUPPORT
#define SCP_LOGGER_ENABLE                (1)
#else
#define SCP_LOGGER_ENABLE                (0)
#endif
#define SCP_DVFS_INIT_ENABLE             (1)
#define SCP_VOW_LOW_POWER_MODE           (1)

/* scp rescovery feature option*/
#define SCP_RECOVERY_SUPPORT             (1)
/* scp recovery timeout value (ms)*/
#define SCP_SYS_RESET_TIMEOUT            1000

#define SCP_PARAMS_TO_SCP_SUPPORT

/* scp aed definition*/
#define SCP_AED_STR_LEN                  (512)
#define SCP_CHECK_AED_STR_LEN(func, offset) ({\
	int ret; ret = func; ((ret > 0) && ((ret + offset) < (SCP_AED_STR_LEN - 1))) ? ret : 0; })

/* scp sub feature register API marco*/
#define SCP_REGISTER_SUB_SENSOR          (1)

/* emi mpu define*/
#define ENABLE_SCP_EMI_PROTECTION        (1)

#define MPU_REGION_ID_SCP_SMEM           7
#define MPU_DOMAIN_D0                    0
#define MPU_DOMAIN_D3                    3


#define SCPSYS_CORE0                     0
#define SCPSYS_CORE1                     1

/* scp sensor type ID list */
enum scp_sensor_id {
	INVALID_FEATURE_ID = 0,
	/* follow google default sensor */
	ACCELEROMETER_FEATURE_ID = 1,
	MAGNETIC_FIELD_FEATURE_ID,
	ORIENTATION_FEATURE_ID,
	GYROSCOPE_FEATURE_ID,
	LIGHT_FEATURE_ID,
	PRESSURE_FEATURE_ID,
	TEMPERATURE_FEATURE_ID,
	PROXIMITY_FEATURE_ID,
	GRAVITY_FEATURE_ID,
	LINEAR_ACCELERATION_FEATURE_ID,
	ROTATION_VECTOR_FEATURE_ID,
	RELATIVE_HUMIDITY_FEATURE_ID,
	AMBIENT_TEMPERATURE_FEATURE_ID,
	MAGNETIC_FIELD_UNCALIBRATED_FEATURE_ID,
	GAME_ROTATION_VECTOR_FEATURE_ID,
	GYROSCOPE_UNCALIBRATED_FEATURE_ID,
	SIGNIFICANT_MOTION_FEATURE_ID,
	STEP_DETECTOR_FEATURE_ID,
	STEP_COUNTER_FEATURE_ID,
	GEOMAGNETIC_ROTATION_VECTOR_FEATURE_ID,
	HEART_RATE_FEATURE_ID,
	TILT_DETECTOR_FEATURE_ID,
	WAKE_GESTURE_FEATURE_ID,
	GLANCE_GESTURE_FEATURE_ID,
	PICK_UP_GESTURE_FEATURE_ID,
	WRIST_TILT_GESTURE_FEATURE_ID,
	DEVICE_ORIENTATION_FEATURE_ID,
	POSE_6DOF_FEATURE_ID,
	STATIONARY_DETECT_FEATURE_ID,
	MOTION_DETECT_FEATURE_ID,
	HEART_BEAT_FEATURE_ID,
	DYNAMIC_SENSOR_META_FEATURE_ID,
	ADDITIONAL_INFO_FEATURE_ID,
	LOW_LATENCY_OFFBODY_DETECT_FEATURE_ID,
	ACCELEROMETER_UNCALIBRATED_FEATURE_ID,/* 35 */

	/* follow mtk add sensor type */
	PEDOMETER_FEATURE_ID = 55,
	IN_POCKET_FEATURE_ID,
	ACTIVITY_FEATURE_ID,
	PDR_FEATURE_ID,
	FREEFALL_FEATURE_ID,
	FLAT_FEATURE_ID,
	FACE_DOWN_FEATURE_ID,
	SHAKE_FEATURE_ID,
	BRINGTOSEE_FEATURE_ID,
	ANSWER_CALL_FEATURE_ID,
	GEOFENCE_FEATURE_ID,
	FLOOR_COUNTER_FEATURE_ID,
	EKG_FEATURE_ID,
	PPG1_FEATURE_ID,
	PPG2_FEATURE_ID,
	RGBW_FEATURE_ID,
	GYRO_TEMPERATURE_FEATURE_ID,
	SAR_FEATURE_ID,
	OIS_FEATURE_ID,
	FLICKER_FEATURE_ID,
	GYRO_SECONDARY_FEATURE_ID,
	FLICKER_REAR_FEATURE_ID,
	RGBW_REAR_FEATURE_ID,
	NUM_SENSOR_TYPE,
};

struct scp_feature_tb {
	uint32_t feature:5,	/* max = 31 */
		 freq:10,	/* max = 1023 */
		 enable:1,	/* max = 1 */
		 sys_id:1;	/* max = 1, run at which subsys? */
};

struct scp_sub_feature_tb {
	uint32_t feature:10,	/* max = 1023 */
		 freq:9,	/* max = 511 */
		 enable:1;	/* max = 1 */
};


extern struct scp_feature_tb feature_table[NUM_FEATURE_ID];
extern struct scp_sub_feature_tb sensor_type_table[NUM_SENSOR_TYPE];

#endif


