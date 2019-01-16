/* alps
 *
 * (C) Copyright 2009
 * MediaTek <www.MediaTek.com>
 *
 * MT6516 Sensor IOCTL & data structure
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __HWMSENSOR_H__
#define __HWMSENSOR_H__

#include <linux/ioctl.h>

#define SENSOR_TYPE_ACCELEROMETER       1
#define SENSOR_TYPE_MAGNETIC_FIELD      2
#define SENSOR_TYPE_ORIENTATION         3
#define SENSOR_TYPE_GYROSCOPE           4
#define SENSOR_TYPE_LIGHT               5
#define SENSOR_TYPE_PRESSURE            6
#define SENSOR_TYPE_TEMPERATURE         7
#define SENSOR_TYPE_PROXIMITY           8
#define SENSOR_TYPE_GRAVITY             9
#define SENSOR_TYPE_LINEAR_ACCELERATION 10
#define SENSOR_TYPE_ROTATION_VECTOR     11
#define SENSOR_TYPE_SIGNIFICANT_MOTION  17
#define SENSOR_TYPE_STEP_DETECTOR       18
#define SENSOR_TYPE_STEP_COUNTER        19

#define SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR     20

#define SENSOR_TYPE_HEART_RATE          21
#define SENSOR_TYPE_TILT_DETECTOR       22
#define SENSOR_TYPE_WAKE_GESTURE        23
#define SENSOR_TYPE_GLANCE_GESTURE      24
#define SENSOR_TYPE_PICK_UP_GESTURE     25

#define SENSOR_TYPE_PEDOMETER           26
#define SENSOR_TYPE_IN_POCKET           27
#define SENSOR_TYPE_ACTIVITY            28
#define SENSOR_TYPE_FACE_DOWN           29
#define SENSOR_TYPE_SHAKE               30

/*---------------------------------------------------------------------------*/
#define ID_BASE							0
#define ID_ORIENTATION					(ID_BASE+SENSOR_TYPE_ORIENTATION-1)
#define ID_MAGNETIC						(ID_BASE+SENSOR_TYPE_MAGNETIC_FIELD-1)
#define ID_ACCELEROMETER				(ID_BASE+SENSOR_TYPE_ACCELEROMETER-1)
#define ID_LINEAR_ACCELERATION			(ID_BASE+SENSOR_TYPE_LINEAR_ACCELERATION-1)
#define ID_ROTATION_VECTOR				(ID_BASE+SENSOR_TYPE_ROTATION_VECTOR-1)
#define ID_GRAVITY						(ID_BASE+SENSOR_TYPE_GRAVITY-1)
#define ID_GYROSCOPE					(ID_BASE+SENSOR_TYPE_GYROSCOPE-1)
#define ID_PROXIMITY					(ID_BASE+SENSOR_TYPE_PROXIMITY-1)
#define ID_LIGHT						(ID_BASE+SENSOR_TYPE_LIGHT-1)
#define ID_PRESSURE						(ID_BASE+SENSOR_TYPE_PRESSURE-1)
#define ID_TEMPRERATURE					(ID_BASE+SENSOR_TYPE_TEMPERATURE-1)
#define ID_SIGNIFICANT_MOTION			(ID_BASE+SENSOR_TYPE_SIGNIFICANT_MOTION-1)  
#define ID_STEP_DETECTOR				(ID_BASE+SENSOR_TYPE_STEP_DETECTOR-1)  
#define ID_STEP_COUNTER					(ID_BASE+SENSOR_TYPE_STEP_COUNTER-1)                
#define ID_GEOMAGNETIC_ROTATION_VECTOR                  (ID_BASE+SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR-1)
#define ID_HEART_RATE           (ID_BASE+SENSOR_TYPE_HEART_RATE-1)
#define ID_TILT_DETECTOR        (ID_BASE+SENSOR_TYPE_TILT_DETECTOR-1)
#define ID_WAKE_GESTURE         (ID_BASE+SENSOR_TYPE_WAKE_GESTURE-1)
#define ID_GLANCE_GESTURE       (ID_BASE+SENSOR_TYPE_GLANCE_GESTURE-1)
#define ID_PICK_UP_GESTURE      (ID_BASE+SENSOR_TYPE_PICK_UP_GESTURE-1)
#define ID_PEDOMETER                                    (ID_BASE+SENSOR_TYPE_PEDOMETER-1)
#define ID_ACTIVITY                                     (ID_BASE+SENSOR_TYPE_ACTIVITY-1)
#define ID_IN_POCKET                                     (ID_BASE+SENSOR_TYPE_IN_POCKET-1)
#define ID_FACE_DOWN                                    (ID_BASE+SENSOR_TYPE_FACE_DOWN-1)
#define ID_SHAKE                                        (ID_BASE+SENSOR_TYPE_SHAKE-1)
#define ID_SENSOR_MAX_HANDLE	  (ID_BASE+30)
#define ID_NONE							    (ID_BASE+31)

#define ID_OFFSET                           (1)

//#define MAX_ANDROID_SENSOR_NUM	(ID_SENSOR_MAX_HANDLE + 1)
//alps\kernel-3.10\drivers\misc\mediatek\hwmon\hwmsen\hwmsen_dev.c
//hwmsen_unlocked_ioctl copy from user only limit 1400 bytes
#define MAX_ANDROID_SENSOR_NUM	(ID_TILT_DETECTOR +1) //not support MTK virtual sensor (all of them are one shot), otherwise fail at copy_form_user, size too large


/*---------------------------------------------------------------------------*/
#define SENSOR_ORIENTATION				(1 << ID_ORIENTATION)
#define SENSOR_MAGNETIC					(1 << ID_MAGNETIC)
#define SENSOR_ACCELEROMETER			(1 << ID_ACCELEROMETER)
#define SENSOR_GYROSCOPE				(1 << ID_GYROSCOPE)
#define SENSOR_PROXIMITY				(1 << ID_PROXIMITY)
#define SENSOR_LIGHT					(1 << ID_LIGHT)
#define SENSOR_PRESSURE					(1 << ID_PRESSURE)
#define SENSOR_TEMPRERATURE				(1 << ID_TEMPRERATURE)
#define SENSOR_GRAVITY					(1 << ID_GRAVITY)
#define SENSOR_LINEAR_ACCELERATION		(1 << ID_LINEAR_ACCELERATION)
#define SENSOR_ROTATION_VECTOR			(1 << ID_ROTATION_VECTOR)

#define SENSOR_SIGNIFICANT_MOTION           (1 << ID_SIGNIFICANT_MOTION)
#define SENSOR_STEP_DETECTOR                (1 << ID_STEP_DETECTOR)
#define SENSOR_STEP_COUNTER                 (1 << ID_STEP_COUNTER)
#define SENSOR_GEOMAGNETIC_ROTATION_VECTOR  (1 << ID_GEOMAGNETIC_ROTATION_VECTOR)

#define SENSOR_HEART_RATE           (1 << ID_HEART_RATE)
#define SENSOR_TILT_DETECTOR        (1 << ID_TILT_DETECTOR)
#define SENSOR_WAKE_GESTURE         (1 << ID_WAKE_GESTURE)
#define SENSOR_GLANCE_GESTURE       (1 << ID_GLANCE_GESTURE)
#define SENSOR_PICK_UP_GESTURE      (1 << ID_PICK_UP_GESTURE)

#define SENSOR_PEDOMETER                    (1 << ID_PEDOMETER) 
#define SENSOR_IN_POCKET                    (1 << ID_IN_POCKET) 
#define SENSOR_ACTIVITY                     (1 << ID_ACTIVITY) 
#define SENSOR_FACE_DOWN                    (1 << ID_FACE_DOWN) 
#define SENSOR_SHAKE                        (1 << ID_SHAKE) 

/*----------------------------------------------------------------------------*/
#define HWM_INPUTDEV_NAME               "hwmdata"
#define HWM_SENSOR_DEV_NAME             "hwmsensor"
#define HWM_SENSOR_DEV                  "/dev/hwmsensor"
#define C_MAX_HWMSEN_EVENT_NUM          4
/*----------------------------------------------------------------------------*/
#define ACC_PL_DEV_NAME                 "m_acc_pl"
#define ACC_INPUTDEV_NAME               "m_acc_input"
#define ACC_MISC_DEV_NAME               "m_acc_misc"
#define MAG_PL_DEV_NAME                 "m_mag_pl"
#define MAG_INPUTDEV_NAME               "m_mag_input"
#define MAG_MISC_DEV_NAME               "m_mag_misc"
#define GYRO_PL_DEV_NAME                	"m_gyro_pl"
#define GYRO_INPUTDEV_NAME              	"m_gyro_input"
#define GYRO_MISC_DEV_NAME              	"m_gyro_misc"
#define ALSPS_PL_DEV_NAME                	"m_alsps_pl"
#define ALSPS_INPUTDEV_NAME              "m_alsps_input"
#define ALSPS_MISC_DEV_NAME              "m_alsps_misc"
#define BARO_PL_DEV_NAME                	"m_baro_pl"
#define BARO_INPUTDEV_NAME              "m_baro_input"
#define BARO_MISC_DEV_NAME              "m_baro_misc"

#define STEP_C_PL_DEV_NAME                "m_step_c_pl"
#define STEP_C_INPUTDEV_NAME              "m_step_c_input"
#define STEP_C_MISC_DEV_NAME              "m_step_c_misc"

#define INPK_PL_DEV_NAME                "m_inpk_pl"
#define INPK_INPUTDEV_NAME              "m_inpk_input"
#define INPK_MISC_DEV_NAME              "m_inpk_misc"

#define SHK_PL_DEV_NAME                "m_shk_pl"
#define SHK_INPUTDEV_NAME              "m_shk_input"
#define SHK_MISC_DEV_NAME              "m_shk_misc"

#define FDN_PL_DEV_NAME                "m_fdn_pl"
#define FDN_INPUTDEV_NAME              "m_fdn_input"
#define FDN_MISC_DEV_NAME              "m_fdn_misc"

#define PKUP_PL_DEV_NAME                "m_pkup_pl"
#define PKUP_INPUTDEV_NAME              "m_pkup_input"
#define PKUP_MISC_DEV_NAME              "m_pkup_misc"

#define ACT_PL_DEV_NAME                "m_act_pl"
#define ACT_INPUTDEV_NAME              "m_act_input"
#define ACT_MISC_DEV_NAME              "m_act_misc"

#define PDR_PL_DEV_NAME                "m_pdr_pl"
#define PDR_INPUTDEV_NAME              "m_pdr_input"
#define PDR_MISC_DEV_NAME              "m_pdr_misc"

#define HRM_PL_DEV_NAME                "m_hrm_pl"
#define HRM_INPUTDEV_NAME              "m_hrm_input"
#define HRM_MISC_DEV_NAME              "m_hrm_misc"

#define TILT_PL_DEV_NAME               "m_tilt_pl"
#define TILT_INPUTDEV_NAME             "m_tilt_input"
#define TILT_MISC_DEV_NAME             "m_tilt_misc"

#define WAG_PL_DEV_NAME                "m_wag_pl"
#define WAG_INPUTDEV_NAME              "m_wag_input"
#define WAG_MISC_DEV_NAME              "m_wag_misc"

#define GLG_PL_DEV_NAME                "m_glg_pl"
#define GLG_INPUTDEV_NAME              "m_glg_input"
#define GLG_MISC_DEV_NAME              "m_glg_misc"

#define TEMP_PL_DEV_NAME                	"m_temp_pl"
#define TEMP_INPUTDEV_NAME              	"m_temp_input"
#define TEMP_MISC_DEV_NAME              	"m_temp_misc"

#define BATCH_PL_DEV_NAME               	"m_batch_pl"
#define BATCH_INPUTDEV_NAME             	"m_batch_input"
#define BATCH_MISC_DEV_NAME             	"m_batch_misc"

#define EVENT_TYPE_SENSOR				0x01
#define EVENT_SENSOR_ACCELERATION		SENSOR_ACCELEROMETER
#define EVENT_SENSOR_MAGNETIC			SENSOR_MAGNETIC
#define EVENT_SENSOR_ORIENTATION		SENSOR_ORIENTATION
#define EVENT_SENSOR_GYROSCOPE			SENSOR_GYROSCOPE
#define EVENT_SENSOR_LIGHT				SENSOR_LIGHT
#define EVENT_SENSOR_PRESSURE			SENSOR_PRESSURE
#define EVENT_SENSOR_TEMPERATURE		SENSOR_TEMPRERATURE
#define EVENT_SENSOR_PROXIMITY			SENSOR_PROXIMITY
#define EVENT_SENSOR_GRAVITY			SENSOR_PRESSURE
#define EVENT_SENSOR_LINEAR_ACCELERATION		SENSOR_TEMPRERATURE
#define EVENT_SENSOR_ROTATION_VECTOR	SENSOR_PROXIMITY
/*-----------------------------------------------------------------------------*/

enum {
	HWM_MODE_DISABLE = 0,
	HWM_MODE_ENABLE = 1,
};

/*------------sensors data----------------------------------------------------*/
typedef struct {
	/* sensor identifier */
	int sensor;
	/* sensor values */
	int	values[6];
	/* sensor values divide */
	uint32_t value_divide;
	/* sensor accuracy */
	int8_t status;
	/* whether updata? */
	int update;
	/* time is in nanosecond */
	int64_t time;

	uint32_t reserved;
} hwm_sensor_data;

typedef struct {
	hwm_sensor_data data[MAX_ANDROID_SENSOR_NUM];
	int date_type;
} hwm_trans_data;


#define MAX_BATCH_DATA_PER_QUREY    18
typedef struct {
	int numOfDataReturn;
	int numOfDataLeft;
	hwm_sensor_data data[MAX_BATCH_DATA_PER_QUREY];
}batch_trans_data;

/*----------------------------------------------------------------------------*/
#define HWM_IOC_MAGIC           0x91

/* set delay */
#define HWM_IO_SET_DELAY		_IOW(HWM_IOC_MAGIC, 0x01, uint32_t)

/* wake up */
#define HWM_IO_SET_WAKE			_IO(HWM_IOC_MAGIC, 0x02)

/* Enable/Disable  sensor */
#define HWM_IO_ENABLE_SENSOR	_IOW(HWM_IOC_MAGIC, 0x03, uint32_t)
#define HWM_IO_DISABLE_SENSOR	_IOW(HWM_IOC_MAGIC, 0x04, uint32_t)

/* Enable/Disable sensor */
#define HWM_IO_ENABLE_SENSOR_NODATA		_IOW(HWM_IOC_MAGIC, 0x05, uint32_t)
#define HWM_IO_DISABLE_SENSOR_NODATA	_IOW(HWM_IOC_MAGIC, 0x06, uint32_t)
/* Get sensors data */
#define HWM_IO_GET_SENSORS_DATA			_IOWR(HWM_IOC_MAGIC, 0x07, hwm_trans_data)

/*----------------------------------------------------------------------------*/
#define BATCH_IOC_MAGIC           0x92

/* Get sensor data */
#define BATCH_IO_GET_SENSORS_DATA			_IOWR(BATCH_IOC_MAGIC, 0x01, batch_trans_data)

#endif				/* __HWMSENSOR_H__ */
