/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_FEATURE_DEFINE_H__
#define __VCP_FEATURE_DEFINE_H__

#include "vcp.h"

/* vcp platform configs*/
#define VCP_BOOT_TIME_OUT_MONITOR        (1)
#define VCP_RESERVED_MEM                 (1)
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_LOGGER_SUPPORT)
#define VCP_LOGGER_ENABLE                (1)
#else
#define VCP_LOGGER_ENABLE                (0)
#endif
#define VCP_DVFS_INIT_ENABLE             (0)
#define VCP_VOW_LOW_POWER_MODE           (1)
#define VCP_DEBUG_NODE_ENABLE            (0)

/* vcp rescovery feature option*/
#define VCP_RECOVERY_SUPPORT             (1)
/* vcp recovery timeout value (ms)*/
#define VCP_SYS_RESET_TIMEOUT            1000

#define VCP_PARAMS_TO_VCP_SUPPORT

/* vcp aed definition*/
#define VCP_AED_STR_LEN                  (512)

/* vcp sub feature register API marco*/
#define VCP_REGISTER_SUB_SENSOR          (1)

/* emi mpu define*/
#define ENABLE_VCP_EMI_PROTECTION        (0)

#define MPU_REGION_ID_VCP_SMEM           7
#define MPU_DOMAIN_D0                    0
#define MPU_DOMAIN_D3                    3


#define VCPSYS_CORE0                     0
#define VCPSYS_CORE1                     1

/* vcp sensor type ID list */
enum vcp_sensor_id {
	ACCELEROMETER_FEATURE_ID = 0,
	MAGNETIC_FEATURE_ID,
	ORIENTATION_FEATURE_ID,
	GYROSCOPE_FEATURE_ID,
	LIGHT_FEATURE_ID,
	PRESSURE_FEATURE_ID,
	TEMPRERATURE_FEATURE_ID,
	PROXIMITY_FEATURE_ID,
	GRAVITY_FEATURE_ID,
	LINEAR_ACCELERATION_FEATURE_ID,
	ROTATION_VECTOR_FEATURE_ID,
	RELATIVE_HUMIDITY_FEATURE_ID,
	AMBIENT_TEMPERATURE_FEATURE_ID,
	MAGNETIC_UNCALIBRATED_FEATURE_ID,
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
	WRIST_TITL_GESTURE_FEATURE_ID,
	DEVICE_ORIENTATION_FEATURE_ID,
	POSE_6DOF_FEATURE_ID,
	STATIONARY_DETECT_FEATURE_ID,
	MOTION_DETECT_FEATURE_ID,
	HEART_BEAT_FEATURE_ID,
	DYNAMIC_SENSOR_META_FEATURE_ID,
	ADDITIONAL_INFO_FEATURE_ID,
	PEDOMETER_FEATURE_ID = 34,
	IN_POCKET_FEATURE_ID,
	ACTIVITY_FEATURE_ID,
	PDR_FEATURE_ID,
	FREEFALL_FEATURE_ID,
	ACCELEROMETER_UNCALIBRATED_FEATURE_ID,
	FACE_DOWN_FEATURE_ID,
	SHAKE_FEATURE_ID,
	BRINGTOSEE_FEATURE_ID,
	ANSWER_CALL_FEATURE_ID,
	GEOFENCE_FEATURE_ID,
	FLOOR_COUNTER_FEATURE_ID,
	EKG_FEATURE_ID,
	PPG1_FEATURE_ID,
	PPG2_FEATURE_ID,
	NUM_SENSOR_TYPE,
};

struct vcp_feature_tb {
	uint32_t feature;
	uint32_t freq;
	uint32_t enable;
	uint32_t sys_id; /* run at which subsys? */
};

struct vcp_sub_feature_tb {
	uint32_t feature;
	uint32_t freq;
	uint32_t enable;
};


extern struct vcp_feature_tb feature_table[NUM_FEATURE_ID];
extern struct vcp_sub_feature_tb sensor_type_table[NUM_SENSOR_TYPE];
extern void vcp_register_sensor(enum feature_id id,
		enum vcp_sensor_id sensor_id);
extern void vcp_deregister_sensor(enum feature_id id,
		enum vcp_sensor_id sensor_id);

#endif


