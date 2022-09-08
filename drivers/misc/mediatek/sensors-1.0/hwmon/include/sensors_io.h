/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef SENSORS_IO_H
#define SENSORS_IO_H

#include <linux/ioctl.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif

struct GSENSOR_VECTOR3D {
	unsigned short x; /**< X axis */
	unsigned short y; /**< Y axis */
	unsigned short z; /**< Z axis */
};

struct SENSOR_DATA {
	int x;
	int y;
	int z;
};

struct biometric_cali {
	unsigned int pga6;
	unsigned int ambdac5_5;
};

struct biometric_test_data {
	int ppg_ir;
	int ppg_r;
	int ekg;
};

struct biometric_threshold {
	int ppg_ir_threshold;
	int ppg_r_threshold;
	int ekg_threshold;
};

#if IS_ENABLED(CONFIG_COMPAT)

struct compat_biometric_cali {
	compat_uint_t pga6;
	compat_uint_t ambdac5_5;
};

struct compat_biometric_test_data {
	compat_int_t ppg_ir;
	compat_int_t ppg_r;
	compat_int_t ekg;
};

struct compat_biometric_threshold {
	compat_int_t ppg_ir_threshold;
	compat_int_t ppg_r__threshold;
	compat_int_t ekg_threshold;
};

#endif

#define GSENSOR 0x85
#define GSENSOR_IOCTL_INIT _IO(GSENSOR, 0x01)
#define GSENSOR_IOCTL_READ_CHIPINFO _IOR(GSENSOR, 0x02, int)
#define GSENSOR_IOCTL_READ_SENSORDATA _IOR(GSENSOR, 0x03, int)
#define GSENSOR_IOCTL_READ_RAW_DATA _IOR(GSENSOR, 0x06, int)
#define GSENSOR_IOCTL_SET_CALI _IOW(GSENSOR, 0x06, struct SENSOR_DATA)
#define GSENSOR_IOCTL_GET_CALI _IOW(GSENSOR, 0x07, struct SENSOR_DATA)
#define GSENSOR_IOCTL_CLR_CALI _IO(GSENSOR, 0x08)
#define GSENSOR_IOCTL_ENABLE_CALI _IO(GSENSOR, 0x09)
#define GSENSOR_IOCTL_SELF_TEST _IO(GSENSOR, 0x0A)

#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_GSENSOR_IOCTL_INIT _IO(GSENSOR, 0x01)
#define COMPAT_GSENSOR_IOCTL_READ_CHIPINFO _IOR(GSENSOR, 0x02, compat_int_t)
#define COMPAT_GSENSOR_IOCTL_READ_SENSORDATA _IOR(GSENSOR, 0x03, compat_int_t)
#define COMPAT_GSENSOR_IOCTL_READ_RAW_DATA _IOR(GSENSOR, 0x06, compat_int_t)
#define COMPAT_GSENSOR_IOCTL_SET_CALI _IOW(GSENSOR, 0x06, struct SENSOR_DATA)
#define COMPAT_GSENSOR_IOCTL_GET_CALI _IOW(GSENSOR, 0x07, struct SENSOR_DATA)
#define COMPAT_GSENSOR_IOCTL_CLR_CALI _IO(GSENSOR, 0x08)
#define COMPAT_GSENSOR_IOCTL_ENABLE_CALI _IO(GSENSOR, 0x09)
#define COMPAT_GSENSOR_IOCTL_SELF_TEST _IO(GSENSOR, 0x0A)
#endif

/* IOCTLs for Msensor misc. device library */
#define MSENSOR 0x83
#define MSENSOR_IOCTL_READ_SENSORDATA _IOR(MSENSOR, 0x03, int)
#define MSENSOR_IOCTL_SELF_TEST _IOW(MSENSOR, 0x0B, int)
#define MSENSOR_IOCTL_SENSOR_ENABLE _IOW(MSENSOR, 0x51, int)

#if IS_ENABLED(CONFIG_COMPAT)
/*COMPACT IOCTL for 64bit kernel running 32bit daemon*/
#define COMPAT_MSENSOR_IOCTL_READ_SENSORDATA _IOR(MSENSOR, 0x03, compat_int_t)
#define COMPAT_MSENSOR_IOCTL_SELF_TEST _IOW(MSENSOR, 0x0B, compat_int_t)
#define COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE _IOW(MSENSOR, 0x51, compat_int_t)
#endif

#define ALSPS 0x84
#define ALSPS_SET_PS_MODE _IOW(ALSPS, 0x01, int)
#define ALSPS_GET_PS_RAW_DATA _IOR(ALSPS, 0x04, int)
#define ALSPS_SET_ALS_MODE _IOW(ALSPS, 0x05, int)
#define ALSPS_GET_ALS_RAW_DATA _IOR(ALSPS, 0x08, int)

/*-------------------MTK add-------------------------------------------*/
#define ALSPS_GET_PS_TEST_RESULT _IOR(ALSPS, 0x09, int)
#define ALSPS_GET_PS_THRESHOLD_HIGH _IOR(ALSPS, 0x0B, int)
#define ALSPS_GET_PS_THRESHOLD_LOW _IOR(ALSPS, 0x0C, int)
#define ALSPS_IOCTL_CLR_CALI _IOW(ALSPS, 0x0F, int)
#define ALSPS_IOCTL_GET_CALI _IOR(ALSPS, 0x10, int)
#define ALSPS_IOCTL_SET_CALI _IOW(ALSPS, 0x11, int)
#define ALSPS_SET_PS_THRESHOLD _IOW(ALSPS, 0x12, int)
#define AAL_SET_ALS_MODE _IOW(ALSPS, 0x14, int)
#define AAL_GET_ALS_MODE _IOR(ALSPS, 0x15, int)
#define AAL_GET_ALS_DATA _IOR(ALSPS, 0x16, int)
#define ALSPS_ALS_ENABLE_CALI _IO(ALSPS, 0x17)
#define ALSPS_PS_ENABLE_CALI _IO(ALSPS, 0x18)
#define ALSPS_IOCTL_ALS_GET_CALI _IOW(ALSPS, 0x19, int)
#define ALSPS_ALS_SET_CALI _IOW(ALSPS, 0x20, int)

#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_ALSPS_SET_PS_MODE _IOW(ALSPS, 0x01, compat_int_t)
#define COMPAT_ALSPS_GET_PS_RAW_DATA _IOR(ALSPS, 0x04, compat_int_t)
#define COMPAT_ALSPS_SET_ALS_MODE _IOW(ALSPS, 0x05, compat_int_t)
#define COMPAT_ALSPS_GET_ALS_RAW_DATA _IOR(ALSPS, 0x08, compat_int_t)

/*-------------------MTK add-------------------------------------------*/
#define COMPAT_ALSPS_GET_PS_TEST_RESULT _IOR(ALSPS, 0x09, compat_int_t)
#define COMPAT_ALSPS_GET_PS_THRESHOLD_HIGH _IOR(ALSPS, 0x0B, compat_int_t)
#define COMPAT_ALSPS_GET_PS_THRESHOLD_LOW _IOR(ALSPS, 0x0C, compat_int_t)
#define COMPAT_ALSPS_IOCTL_CLR_CALI _IOW(ALSPS, 0x0F, compat_int_t)
#define COMPAT_ALSPS_IOCTL_GET_CALI _IOR(ALSPS, 0x10, compat_int_t)
#define COMPAT_ALSPS_IOCTL_SET_CALI _IOW(ALSPS, 0x11, compat_int_t)
#define COMPAT_ALSPS_SET_PS_THRESHOLD _IOW(ALSPS, 0x12, compat_int_t)
#define COMPAT_AAL_SET_ALS_MODE _IOW(ALSPS, 0x14, compat_int_t)
#define COMPAT_AAL_GET_ALS_MODE _IOR(ALSPS, 0x15, compat_int_t)
#define COMPAT_AAL_GET_ALS_DATA _IOR(ALSPS, 0x16, compat_int_t)
#define COMPAT_ALSPS_ALS_ENABLE_CALI _IO(ALSPS, 0x17)
#define COMPAT_ALSPS_PS_ENABLE_CALI _IO(ALSPS, 0x18)
#define COMPAT_ALSPS_IOCTL_ALS_GET_CALI _IOR(ALSPS, 0x19, compat_int_t)
#define COMPAT_ALSPS_IOCTL_ALS_SET_CALI _IOR(ALSPS, 0x20, compat_int_t)

#endif

#define GYROSCOPE 0x86
#define GYROSCOPE_IOCTL_INIT _IO(GYROSCOPE, 0x01)
#define GYROSCOPE_IOCTL_SMT_DATA _IOR(GYROSCOPE, 0x02, int)
#define GYROSCOPE_IOCTL_READ_SENSORDATA _IOR(GYROSCOPE, 0x03, int)
#define GYROSCOPE_IOCTL_SET_CALI _IOW(GYROSCOPE, 0x04, struct SENSOR_DATA)
#define GYROSCOPE_IOCTL_GET_CALI _IOW(GYROSCOPE, 0x05, struct SENSOR_DATA)
#define GYROSCOPE_IOCTL_CLR_CALI _IO(GYROSCOPE, 0x06)
#define GYROSCOPE_IOCTL_READ_SENSORDATA_RAW _IOR(GYROSCOPE, 0x07, int)
#define GYROSCOPE_IOCTL_ENABLE_CALI _IO(GYROSCOPE, 0x0A)
#define GYROSCOPE_IOCTL_SELF_TEST _IO(GYROSCOPE, 0x0B)
#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_GYROSCOPE_IOCTL_INIT _IO(GYROSCOPE, 0x01)
#define COMPAT_GYROSCOPE_IOCTL_SMT_DATA _IOR(GYROSCOPE, 0x02, compat_int_t)
#define COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA                                 \
	_IOR(GYROSCOPE, 0x03, compat_int_t)
#define COMPAT_GYROSCOPE_IOCTL_SET_CALI                                        \
	_IOW(GYROSCOPE, 0x04, struct SENSOR_DATA)
#define COMPAT_GYROSCOPE_IOCTL_GET_CALI                                        \
	_IOW(GYROSCOPE, 0x05, struct SENSOR_DATA)
#define COMPAT_GYROSCOPE_IOCTL_CLR_CALI _IO(GYROSCOPE, 0x06)
#define COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA_RAW                             \
	_IOR(GYROSCOPE, 0x07, compat_int_t)
#define COMPAT_GYROSCOPE_IOCTL_ENABLE_CALI _IO(GYROSCOPE, 0x0A)
#define COMPAT_GYROSCOPE_IOCTL_SELF_TEST _IO(GYROSCOPE, 0x0B)
#endif
#define BROMETER 0x87
#define BAROMETER_IOCTL_INIT _IO(BROMETER, 0x01)
#define BAROMETER_GET_PRESS_DATA _IOR(BROMETER, 0x02, int)
#define BAROMETER_GET_TEMP_DATA _IOR(BROMETER, 0x03, int)
#define BAROMETER_IOCTL_ENABLE_CALI _IO(BROMETER, 0x05)
#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_BAROMETER_IOCTL_INIT _IO(BROMETER, 0x01)
#define COMPAT_BAROMETER_GET_PRESS_DATA _IOR(BROMETER, 0x02, compat_int_t)
#define COMPAT_BAROMETER_GET_TEMP_DATA _IOR(BROMETER, 0x03, compat_int_t)
#define COMPAT_BAROMETER_IOCTL_ENABLE_CALI _IO(BROMETER, 0x05)
#endif

#define BIOMETRIC 0x90
#define BIOMETRIC_IOCTL_INIT _IO(BIOMETRIC, 0x01)
#define BIOMETRIC_IOCTL_DO_CALI _IOW(BIOMETRIC, 0x02, struct biometric_cali)
#define BIOMETRIC_IOCTL_SET_CALI _IOW(BIOMETRIC, 0x03, struct biometric_cali)
#define BIOMETRIC_IOCTL_GET_CALI _IOW(BIOMETRIC, 0x04, struct biometric_cali)
#define BIOMETRIC_IOCTL_CLR_CALI _IO(BIOMETRIC, 0x05)
#define BIOMETRIC_IOCTL_FTM_START _IO(BIOMETRIC, 0x06)
#define BIOMETRIC_IOCTL_FTM_END _IO(BIOMETRIC, 0x07)
#define BIOMETRIC_IOCTL_FTM_GET_DATA                                           \
	_IOW(BIOMETRIC, 0x08, struct biometric_test_data)
#define BIOMETRIC_IOCTL_FTM_GET_THRESHOLD                                      \
	_IOW(BIOMETRIC, 0x09, struct biometric_threshold)
#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_BIOMETRIC_IOCTL_INIT _IO(BIOMETRIC, 0x01)
#define COMPAT_BIOMETRIC_IOCTL_DO_CALI                                         \
	_IOW(BIOMETRIC, 0x02, struct compat_biometric_cali)
#define COMPAT_BIOMETRIC_IOCTL_SET_CALI                                        \
	_IOW(BIOMETRIC, 0x03, struct compat_biometric_cali)
#define COMPAT_BIOMETRIC_IOCTL_GET_CALI                                        \
	_IOW(BIOMETRIC, 0x04, struct compat_biometric_cali)
#define COMPAT_BIOMETRIC_IOCTL_CLR_CALI _IO(BIOMETRIC, 0x05)
#define COMPAT_BIOMETRIC_IOCTL_FTM_START _IO(BIOMETRIC, 0x06)
#define COMPAT_BIOMETRIC_IOCTL_FTM_END _IO(BIOMETRIC, 0x07)
#define COMPAT_BIOMETRIC_IOCTL_FTM_GET_DATA                                    \
	_IOW(BIOMETRIC, 0x08, struct compat_biometric_test_data)
#define COMPAT_BIOMETRIC_IOCTL_FTM_GET_THRESHOLD                               \
	_IOW(BIOMETRIC, 0x09, struct compat_biometric_threshold)
#endif

#define SAR 0x91
#define SAR_IOCTL_INIT _IOW(SAR, 0x01, int)
#define SAR_IOCTL_READ_SENSORDATA _IOR(SAR, 0x02, struct SENSOR_DATA)
#define SAR_IOCTL_GET_CALI  _IOR(SAR, 0x03, struct SENSOR_DATA)
#define SAR_IOCTL_ENABLE_CALI _IO(SAR, 0x04)
#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_SAR_IOCTL_INIT _IOW(SAR, 0x01, compat_int_t)
#define COMPAT_SAR_IOCTL_READ_SENSORDATA _IOR(SAR, 0x02, struct SENSOR_DATA)
#define COMPAT_SAR_IOCTL_GET_CALI _IOR(SAR, 0x03, struct SENSOR_DATA)
#define COMPAT_SAR_IOCTL_ENABLE_CALI _IO(SAR, 0x04)
#endif

#endif
