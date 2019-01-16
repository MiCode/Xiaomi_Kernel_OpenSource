/**
 * Header file of the core driver API @file yas.h
 *
 * Copyright (c) 2013-2015 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef __YAS_H__
#define __YAS_H__

#include "yas_cfg.h"

#define YAS_VERSION	"10.2.5"	/*!< MS-x Driver Version */

/* ----------------------------------------------------------------------------
 *                             Typedef definition
 *--------------------------------------------------------------------------- */

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdint.h>
/*typedef signed char	int8_t;*/
/*typedef unsigned char	uint8_t;*/
/*typedef signed short	int16_t;*/
/*typedef unsigned short	uint16_t;*/
/*typedef signed int	int32_t;*/
/*typedef unsigned int	uint32_t;*/
#endif

/* ----------------------------------------------------------------------------
 *                              Macro definition
 *--------------------------------------------------------------------------- */

#define YAS_DEBUG			(0) /*!< Debug print (0:disabled,
					      1:enabled) */

#define YAS_NO_ERROR			(0) /*!< Succeed */
#define YAS_ERROR_ARG			(-1) /*!< Invalid argument */
#define YAS_ERROR_INITIALIZE		(-2) /*!< Invalid initialization status
					      */
#define YAS_ERROR_BUSY			(-3) /*!< Sensor is busy */
#define YAS_ERROR_DEVICE_COMMUNICATION	(-4) /*!< Device communication error */
#define YAS_ERROR_CHIP_ID		(-5) /*!< Invalid chip id */
#define YAS_ERROR_CALREG		(-6) /*!< Invalid CAL register */
#define YAS_ERROR_OVERFLOW		(-7) /*!< Overflow occured */
#define YAS_ERROR_UNDERFLOW		(-8) /*!< Underflow occured */
#define YAS_ERROR_DIRCALC		(-9) /*!< Direction calcuration error */
#define YAS_ERROR_ERROR			(-128) /*!< other error */

#ifndef NULL
#ifdef __cplusplus
#define NULL				(0) /*!< NULL */
#else
#define NULL				((void *)(0)) /*!< NULL */
#endif
#endif
#ifndef NELEMS
#define NELEMS(a)	((int)(sizeof(a)/sizeof(a[0]))) /*!< Number of array
							  elements */
#endif
#ifndef ABS
#define ABS(a)		((a) > 0 ? (a) : -(a)) /*!< Absolute value */
#endif
#ifndef M_PI
#define M_PI		(3.14159265358979323846) /*!< Math PI */
#endif

#define YAS_MATRIX_NORM		(10000) /*!< Matrix normalize unit */
/* YAS_MATRIX_NORM_RECIP_BIT = SFIXED_RECIP_Q31(YAS_MATRIX_NORM,
 * &YAS_MATRIX_NORM_RECIP) */
#define YAS_MATRIX_NORM_RECIP		(0x68DB8BAC)
#define YAS_MATRIX_NORM_RECIP_BIT	(18)
#define YAS_QUATERNION_NORM	(10000) /*!< Quaternion normalize unit */

#if YAS_DEBUG
#ifdef __KERNEL__
#include <linux/kernel.h>
#define YLOGD(args) (printk args)	/*!< Debug log (DEBUG) */
#define YLOGI(args) (printk args)	/*!< Debug log (INFO) */
#define YLOGE(args) (printk args)	/*!< Debug log (ERROR) */
#define YLOGW(args) (printk args)	/*!< Debug log (WARNING) */
#elif defined __ANDROID__
#include <cutils/log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "yas"
#define YLOGD(args) (ALOGD args)	/*!< Debug log (DEBUG) */
#define YLOGI(args) (ALOGI args)	/*!< Debug log (INFO) */
#define YLOGE(args) (ALOGE args)	/*!< Debug log (ERROR) */
#define YLOGW(args) (ALOGW args)	/*!< Debug log (WARNING) */
#else /* __ANDROID__ */
#include <stdio.h>
#define YLOGD(args) (printf args)	/*!< Debug log (DEBUG) */
#define YLOGI(args) (printf args)	/*!< Debug log (INFO) */
#define YLOGE(args) (printf args)	/*!< Debug log (ERROR) */
#define YLOGW(args) (printf args)	/*!< Debug log (WARNING) */
#endif /* __ANDROID__ */
#else /* DEBUG */
#define YLOGD(args)	/*!< Debug log (DEBUG) */
#define YLOGI(args)	/*!< Debug log (INFO) */
#define YLOGW(args)	/*!< Debug log (ERROR) */
#define YLOGE(args)	/*!< Debug log (WARNING) */
#endif /* DEBUG */

#define YAS_TYPE_ACC_NONE		(0x00000001) /*!< No Acceleration */
#define YAS_TYPE_MAG_NONE		(0x00000002) /*!< No Magnetometer */
#define YAS_TYPE_GYRO_NONE		(0x00000004) /*!< No Gyroscope */
#define YAS_TYPE_A_ACC			(0x00000008) /*!< 3-axis Acceleration */
#define YAS_TYPE_M_MAG			(0x00000010) /*!< 3-axis Magnetometer */
#define YAS_TYPE_G_GYRO			(0x00000020) /*!< 3-axis Gyroscope */
#define YAS_TYPE_AM_ACC			(0x00100000) /*!< 6-axis (Acc+Mag)
						       Acceleration */
#define YAS_TYPE_AM_MAG			(0x00200000) /*!< 6-axis (Acc+Mag)
						       Magnetometer */
#define YAS_TYPE_AG_ACC			(0x01000000) /*!< 6-axis (Acc+Gyro)
						       Acceleration */
#define YAS_TYPE_AG_GYRO		(0x02000000) /*!< 6-axis (Acc+Gyro)
						       Gyroscope */
#define YAS_TYPE_AMG_ACC		(0x10000000) /*!< 9-axis (Acc+Gyro+Mag)
						       Acceleration */
#define YAS_TYPE_AMG_MAG		(0x20000000) /*!< 9-axis (Acc+Gyro+Mag)
						       Magnetometer */
#define YAS_TYPE_AMG_GYRO		(0x40000000) /*!< 9-axis (Acc+Gyro+Mag)
						       Gyroscope */

#if YAS_ACC_DRIVER == YAS_ACC_DRIVER_NONE
#define YAS_TYPE_ACC YAS_TYPE_ACC_NONE
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_ADXL345
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_ADXL346
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA150
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA222
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA222E
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA250
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA250E
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA254
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA255
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMI055
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMI058
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_DMARD08
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXSD9
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTE9
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTF9
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTI9
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTJ2
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXUD9
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DL
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DLH
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DLM
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LSM330DLC
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_MMA8452Q
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_MMA8453Q
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_U2DH
#define YAS_TYPE_ACC YAS_TYPE_A_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_YAS535
#define YAS_TYPE_ACC YAS_TYPE_AM_ACC
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_YAS53x
#define YAS_TYPE_ACC YAS_TYPE_AMG_ACC
#endif

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_NONE
#define YAS_TYPE_MAG YAS_TYPE_MAG_NONE
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS529 /* MS-3C */
#define YAS_TYPE_MAG YAS_TYPE_M_MAG
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS530 /* MS-3E */
#define YAS_TYPE_MAG YAS_TYPE_M_MAG
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS532 /* MS-3R */
#define YAS_TYPE_MAG YAS_TYPE_M_MAG
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS533 /* MS-3F */
#define YAS_TYPE_MAG YAS_TYPE_M_MAG
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS535 /* MS-6C */
#define YAS_TYPE_MAG YAS_TYPE_AM_MAG
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS536 /* MS-3W */
#define YAS_TYPE_MAG YAS_TYPE_M_MAG
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS537 /* MS-3T */
#define YAS_TYPE_MAG YAS_TYPE_M_MAG
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS539 /* MS-3S */
#define YAS_TYPE_MAG YAS_TYPE_M_MAG
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS53x
#define YAS_TYPE_MAG YAS_TYPE_AMG_MAG
#endif

#if YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_NONE
#define YAS_TYPE_GYRO YAS_TYPE_GYRO_NONE
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_BMG160
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_BMI055
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_BMI058
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_EWTZMU
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_ITG3200
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_ITG3500
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_L3G3200D
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_L3G4200D
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_LSM330DLC
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_MPU3050
#define YAS_TYPE_GYRO YAS_TYPE_G_GYRO
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_YAS53x
#define YAS_TYPE_GYRO YAS_TYPE_AMG_GYRO
#endif

/* ----------------------------------------------------------------------------
 *                   Geomagnetic Calibration Configuration
 *--------------------------------------------------------------------------- */

/*! Geomagnetic calibration mode: spherical only */
#define YAS_MAG_CALIB_MODE_SPHERE		(0)
/*! Geomagnetic calibration mode: ellipsoidal only */
#define YAS_MAG_CALIB_MODE_ELLIPSOID		(1)
/*! Geomagnetic calibration mode: spherical with gyroscope */
#define YAS_MAG_CALIB_MODE_SPHERE_WITH_GYRO	(2)
/*! Geomagnetic calibration mode: ellisoldal with gyroscope */
#define YAS_MAG_CALIB_MODE_ELLIPSOID_WITH_GYRO	(3)
/*! Geomagnetic calibration mode: gyroscope only */
#define YAS_MAG_CALIB_MODE_WITH_GYRO		(4)

/* ----------------------------------------------------------------------------
 *                      Extension Command Definition
 *--------------------------------------------------------------------------- */
/*! YAS530 extension command: self test */
#define YAS530_SELF_TEST		(0x00000001)
/*! YAS530 extension command: self test noise */
#define YAS530_SELF_TEST_NOISE		(0x00000002)
/*! YAS530 extension command: obtains the hardware offset */
#define YAS530_GET_HW_OFFSET		(0x00000003)
/*! YAS530 extension command: sets the hardware offset */
#define YAS530_SET_HW_OFFSET		(0x00000004)
/*! YAS530 extension command: obtains last raw data (x, y1, y2, t) */
#define YAS530_GET_LAST_RAWDATA		(0x00000006)
/*! YAS530 extension command: obtains the ellipsoidal correction matrix */
#define YAS530_GET_STATIC_MATRIX	(0x00000007)
/*! YAS530 extension command: sets the ellipsoidal correction matrix */
#define YAS530_SET_STATIC_MATRIX	(0x00000008)

/*! YAS532 extension command: self test */
#define YAS532_SELF_TEST		(0x00000001)
/*! YAS532 extension command: self test noise */
#define YAS532_SELF_TEST_NOISE		(0x00000002)
/*! YAS532 extension command: obtains the hardware offset */
#define YAS532_GET_HW_OFFSET		(0x00000003)
/*! YAS532 extension command: sets the hardware offset */
#define YAS532_SET_HW_OFFSET		(0x00000004)
/*! YAS532 extension command: obtains last raw data (x, y1, y2, t) */
#define YAS532_GET_LAST_RAWDATA		(0x00000006)
/*! YAS532 extension command: sets the interrupt enable */
#define YAS532_SET_INTERRUPT_ENABLE	(0x00000007)
/*! YAS532 extension command: sets the interrupt active HIGH */
#define YAS532_SET_INTERRUPT_ACTIVE_HIGH (0x00000008)
/*! YAS532 extension command: obtains the ellipsoidal correction matrix */
#define YAS532_GET_STATIC_MATRIX	(0x00000009)
/*! YAS532 extension command: sets the ellipsoidal correction matrix */
#define YAS532_SET_STATIC_MATRIX	(0x0000000a)

/*! YAS535 extension command: obtains last raw data (xy1y2[3] t xyz[3]) */
#define YAS535_GET_LAST_RAWDATA		(0x00000001)
/*! YAS535 extension command: self test for magnetometer */
#define YAS535_MAG_SELF_TEST		(0x00000002)
/*! YAS535 extension command: self test noise for magnetometer */
#define YAS535_MAG_SELF_TEST_NOISE	(0x00000003)
/*! YAS535 extension command: obtains the hardware offset */
#define YAS535_MAG_GET_HW_OFFSET	(0x00000004)
/*! YAS535 extension command: sets the hardware offset */
#define YAS535_MAG_SET_HW_OFFSET	(0x00000005)
/*! YAS535 extension command: obtains the average samples for magnetometer */
#define YAS535_MAG_GET_AVERAGE_SAMPLE	(0x00000006)
/*! YAS535 extension command: sets the average samples for magnetometer */
#define YAS535_MAG_SET_AVERAGE_SAMPLE	(0x00000007)
/*! YAS535 extension command: self test for accelerometer */
#define YAS535_ACC_SELF_TEST		(0x00010000)
/*! YAS535 extension command: obtains the average samples for accelerometer */
#define YAS535_ACC_GET_AVERAGE_SAMPLE	(0x00020000)
/*! YAS535 extension command: sets the average samples for accelerometer */
#define YAS535_ACC_SET_AVERAGE_SAMPLE	(0x00040000)

/*! YAS536 extension command: self test */
#define YAS536_SELF_TEST		(0x00000001)
/*! YAS536 extension command: obtains the hardware offset */
#define YAS536_GET_HW_OFFSET		(0x00000002)
/*! YAS536 extension command: obtains the average filter length */
#define YAS536_GET_AVERAGE_LEN		(0x00000004)
/*! YAS536 extension command: sets the average filter length */
#define YAS536_SET_AVERAGE_LEN		(0x00000005)
/*! YAS536 extension command: obtains last raw data (x, y1, y2, t) */
#define YAS536_GET_LAST_RAWDATA		(0x00000006)

/*! YAS537 extension command: self test */
#define YAS537_SELF_TEST		(0x00000001)
/*! YAS537 extension command: self test noise */
#define YAS537_SELF_TEST_NOISE		(0x00000002)
/*! YAS537 extension command: obtains last raw data (x, y1, y2, t) */
#define YAS537_GET_LAST_RAWDATA		(0x00000003)
/*! YAS537 extension command: obtains the average samples */
#define YAS537_GET_AVERAGE_SAMPLE	(0x00000004)
/*! YAS537 extension command: sets the average samples */
#define YAS537_SET_AVERAGE_SAMPLE	(0x00000005)
/*! YAS537 extension command: obtains the hardware offset */
#define YAS537_GET_HW_OFFSET		(0x00000006)
/*! YAS537 extension command: obtains the ellipsoidal correction matrix */
#define YAS537_GET_STATIC_MATRIX	(0x00000007)
/*! YAS537 extension command: sets the ellipsoidal correction matrix */
#define YAS537_SET_STATIC_MATRIX	(0x00000008)
/*! YAS537 extension command: obtains the overflow and underflow threshold */
#define YAS537_GET_OUFLOW_THRESH	(0x00000009)


/*! YAS539 extension command: self test */
#define YAS539_SELF_TEST		(0x00000001)
/*! YAS539 extension command: self test noise */
#define YAS539_SELF_TEST_NOISE		(0x00000002)
/*! YAS539 extension command: obtains last raw data (x, y1, y2, t) */
#define YAS539_GET_LAST_RAWDATA		(0x00000003)
/*! YAS539 extension command: obtains the average samples */
#define YAS539_GET_AVERAGE_SAMPLE	(0x00000004)
/*! YAS539 extension command: sets the average samples */
#define YAS539_SET_AVERAGE_SAMPLE	(0x00000005)
/*! YAS539 extension command: obtains the hardware offset */
#define YAS539_GET_HW_OFFSET		(0x00000006)

/* ----------------------------------------------------------------------------
 *                            Structure definition
 *--------------------------------------------------------------------------- */

/**
 * @struct yas_vector
 * @brief Stores the sensor data
 */
struct yas_vector {
	int32_t v[3]; /*!< vector data */
};

/**
 * @struct yas_quaternion
 * @brief Stores the quaternion
 */
struct yas_quaternion {
	int32_t q[4]; /*!< quaternion */
	int32_t heading_error; /*!< heading error in mdegree,
				 -1 if unavailable */
};

/**
 * @struct yas_matrix
 * @brief Stores the matrix data
 */
struct yas_matrix {
	int16_t m[9]; /*!< matrix data */
};

/**
 * @struct yas_data
 * @brief Stores the sensor data
 */
struct yas_data {
	int32_t type; /*!< Sensor type */
	struct yas_vector xyz; /*!< X, Y, Z measurement data of the sensor */
	uint32_t timestamp; /*!< Measurement time */
	uint8_t accuracy; /*!< Measurement data accuracy */
};

/**
 * @struct yas_driver_callback
 * @brief User-written callback functions specific to each implementation, such
 * as communication controls with the device
 */
struct yas_driver_callback {
	/**
	 * Open the device
	 * @param[in] type Sensor type
	 * @retval 0 Success
	 * @retval Negative Failure
	 */
	int (*device_open)(int32_t type);
	/**
	 * Close the device
	 * @param[in] type Sensor type
	 * @retval 0 Success
	 * @retval Negative Failure
	 */
	int (*device_close)(int32_t type);
	/**
	 * Send data to the device
	 * @param[in] type Sensor type
	 * @param[in] addr Register address
	 * @param[in] buf The pointer to the data buffer to be sent
	 * @param[in] len The length of the data buffer
	 * @retval 0 Success
	 * @retval Negative Failure
	 */
	int (*device_write)(int32_t type, uint8_t addr, const uint8_t *buf,
			int len);
	/**
	 * Receive data from the device
	 * @param[in] type Sensor type
	 * @param[in] addr Register address
	 * @param[out] buf The pointer to the data buffer to be received
	 * @param[in] len The length of the data buffer
	 * @retval 0 Success
	 * @retval Negative Failure
	 */
	int (*device_read)(int32_t type, uint8_t addr, uint8_t *buf, int len);
	/**
	 * Sleep in micro-seconds
	 * @param[in] usec Sleep time in micro-seconds
	 */
	void (*usleep)(int usec);
	/**
	 * Obtains the current time in milli-seconds
	 * @return The current time in milli-seconds
	 */
	uint32_t (*current_time)(void);
};

/**
 * @struct yas_acc_driver
 * @brief Acceleration sensor driver
 */
struct yas_acc_driver {
	/**
	 * Initializes the sensor, starts communication with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the communications with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Obtains measurment period in milli-seconds
	 * @retval Non-Negatiev Measurement period in milli-seconds
	 * @retval Negative Failure
	 */
	int (*get_delay)(void);
	/**
	 * Sets measurment period in milli-seconds
	 * @param[in] delay Measurement period in milli-seconds
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_delay)(int delay);
	/**
	 * Reports the sensor status (Enabled or Disabled)
	 * @retval 0 Disabled
	 * @retval 1 Enabled
	 * @retval Negative Failure
	 */
	int (*get_enable)(void);
	/**
	 * Enables or disables sensors
	 * @param[in] enable The status of the sensor (0: Disable, 1: Enable)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_enable)(int enable);
	/**
	 * Obtains the sensor position
	 * @retval 0-7 The position of the sensor
	 * @retval Negative Failure
	 */
	int (*get_position)(void);
	/**
	 * Sets the sensor position
	 * @param[in] position The position of the sensor (0-7)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_position)(int position);
	/**
	 * Measures the sensor
	 * @param[out] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval Non-Negative The number of the measured sensor data
	 * @retval Negative Failure
	 */
	int (*measure)(struct yas_data *raw, int num);
	/**
	 * Extension command execution specific to the part number
	 * @param[in] cmd Extension command id
	 * @param[out] result Extension command result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*ext)(int32_t cmd, void *result);
	struct yas_driver_callback callback; /*!< Callback functions */
};

/**
 * @struct yas_mag_driver
 * @brief Magnetic sensor driver
 */
struct yas_mag_driver {
	/**
	 * Initializes the sensor, starts communication with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the communications with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Obtains measurment period in milli-seconds
	 * @retval Non-Negatiev Measurement period in milli-seconds
	 * @retval Negative Failure
	 */
	int (*get_delay)(void);
	/**
	 * Sets measurment period in milli-seconds
	 * @param[in] delay Measurement period in milli-seconds
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_delay)(int delay);
	/**
	 * Reports the sensor status (Enabled or Disabled)
	 * @retval 0 Disabled
	 * @retval 1 Enabled
	 * @retval Negative Failure
	 */
	int (*get_enable)(void);
	/**
	 * Enables or disables sensors
	 * @param[in] enable The status of the sensor (0: Disable, 1: Enable)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_enable)(int enable);
	/**
	 * Obtains the sensor position
	 * @retval 0-7 The position of the sensor
	 * @retval Negative Failure
	 */
	int (*get_position)(void);
	/**
	 * Sets the sensor position
	 * @param[in] position The position of the sensor (0-7)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_position)(int position);
	/**
	 * Measures the sensor
	 * @param[out] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval Non-Negative The number of the measured sensor data
	 * @retval Negative Failure
	 */
	int (*measure)(struct yas_data *raw, int num);
	/**
	 * Extension command execution specific to the part number
	 * @param[in] cmd Extension command id
	 * @param[out] result Extension command result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*ext)(int32_t cmd, void *result);
	struct yas_driver_callback callback; /*!< Callback functions */
};

/**
 * @struct yas_gyro_driver
 * @brief Gyroscope sensor driver
 */
struct yas_gyro_driver {
	/**
	 * Initializes the sensor, starts communication with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the communications with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Obtains measurment period in milli-seconds
	 * @retval Non-Negatiev Measurement period in milli-seconds
	 * @retval Negative Failure
	 */
	int (*get_delay)(void);
	/**
	 * Sets measurment period in milli-seconds
	 * @param[in] delay Measurement period in milli-seconds
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_delay)(int delay);
	/**
	 * Reports the sensor status (Enabled or Disabled)
	 * @retval 0 Disabled
	 * @retval 1 Enabled
	 * @retval Negative Failure
	 */
	int (*get_enable)(void);
	/**
	 * Enables or disables sensors
	 * @param[in] enable The status of the sensor (0: Disable, 1: Enable)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_enable)(int enable);
	/**
	 * Obtains the sensor position
	 * @retval 0-7 The position of the sensor
	 * @retval Negative Failure
	 */
	int (*get_position)(void);
	/**
	 * Sets the sensor position
	 * @param[in] position The position of the sensor (0-7)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_position)(int position);
	/**
	 * Measures the sensor
	 * @param[out] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval Non-Negative The number of the measured sensor data
	 * @retval Negative Failure
	 */
	int (*measure)(struct yas_data *raw, int num);
	/**
	 * Extension command execution specific to the part number
	 * @param[in] cmd Extension command id
	 * @param[out] result Extension command result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*ext)(int32_t cmd, void *result);
	struct yas_driver_callback callback; /*!< Callback functions */
};

/**
 * @struct yas_acc_mag_driver
 * @brief Acceleration and geomagnetix sensor driver (6-axis).
 */
struct yas_acc_mag_driver {
	/**
	 * Initializes the sensor, starts communication with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the communications with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Obtains measurment period in milli-seconds
	 * @param[in] type Sensor type
	 * @retval Non-Negatiev Measurement period in milli-seconds
	 * @retval Negative Failure
	 */
	int (*get_delay)(int32_t type);
	/**
	 * Sets measurment period in milli-seconds
	 * @param[in] type Sensor type
	 * @param[in] delay Measurement period in milli-seconds
	 * @retval Non-Negative The bit or of the sensor type successfully
	 * delay changed.
	 * @retval Negative Failure
	 */
	int (*set_delay)(int32_t type, int delay);
	/**
	 * Reports the sensor status (Enabled or Disabled)
	 * @param[in] type Sensor type
	 * @retval 0 Disabled
	 * @retval 1 Enabled
	 * @retval Negative Failure
	 */
	int (*get_enable)(int32_t type);
	/**
	 * Enables or disables sensors
	 * @param[in] type Sensor type
	 * @param[in] enable The status of the sensor (0: Disable, 1: Enable)
	 * @retval Non-Negative The bit or of the sensor type successfully
	 * enabled/disabled.
	 * @retval Negative Failure
	 */
	int (*set_enable)(int32_t type, int enable);
	/**
	 * Obtains the sensor position
	 * @retval 0-7 The position of the sensor
	 * @retval Negative Failure
	 */
	int (*get_position)(void);
	/**
	 * Sets the sensor position
	 * @param[in] position The position of the sensor (0-7)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_position)(int position);
	/**
	 * Measures the sensor
	 * @param[in] type Sensor type
	 * @param[out] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval Non-Negative The number of the measured sensor data
	 * @retval Negative Failure
	 */
	int (*measure)(int32_t type, struct yas_data *raw, int num);
	/**
	 * Extension command execution specific to the part number
	 * @param[in] cmd Extension command id
	 * @param[out] result Extension command result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*ext)(int32_t cmd, void *result);
	struct yas_driver_callback callback; /*!< Callback functions */
};

/**
 * @struct yas_acc_gyro_driver
 * @brief Acceleration and gyroscope sensor driver (6-axis).
 */
struct yas_acc_gyro_driver {
	/**
	 * Initializes the sensor, starts communication with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the communications with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Obtains measurment period in milli-seconds
	 * @param[in] type Sensor type
	 * @retval Non-Negatiev Measurement period in milli-seconds
	 * @retval Negative Failure
	 */
	int (*get_delay)(int32_t type);
	/**
	 * Sets measurment period in milli-seconds
	 * @param[in] type Sensor type
	 * @param[in] delay Measurement period in milli-seconds
	 * @retval Non-Negative The bit or of the sensor type successfully
	 * delay changed.
	 * @retval Negative Failure
	 */
	int (*set_delay)(int32_t type, int delay);
	/**
	 * Reports the sensor status (Enabled or Disabled)
	 * @param[in] type Sensor type
	 * @retval 0 Disabled
	 * @retval 1 Enabled
	 * @retval Negative Failure
	 */
	int (*get_enable)(int32_t type);
	/**
	 * Enables or disables sensors
	 * @param[in] type Sensor type
	 * @param[in] enable The status of the sensor (0: Disable, 1: Enable)
	 * @retval Non-Negative The bit or of the sensor type successfully
	 * enabled/disabled.
	 * @retval Negative Failure
	 */
	int (*set_enable)(int32_t type, int enable);
	/**
	 * Obtains the sensor position
	 * @retval 0-7 The position of the sensor
	 * @retval Negative Failure
	 */
	int (*get_position)(void);
	/**
	 * Sets the sensor position
	 * @param[in] position The position of the sensor (0-7)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_position)(int position);
	/**
	 * Measures the sensor
	 * @param[in] type Sensor type
	 * @param[out] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval Non-Negative The number of the measured sensor data
	 * @retval Negative Failure
	 */
	int (*measure)(int32_t type, struct yas_data *raw, int num);
	/**
	 * Extension command execution specific to the part number
	 * @param[in] cmd Extension command id
	 * @param[out] result Extension command result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*ext)(int32_t cmd, void *result);
	struct yas_driver_callback callback; /*!< Callback functions */
};

/**
 * @struct yas_acc_mag_gyro_driver
 * @brief Acceleration, geomagnetic and gyroscope sensor driver (9-axis).
 */
struct yas_acc_mag_gyro_driver {
	/**
	 * Initializes the sensor, starts communication with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the communications with the device
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Obtains measurment period in milli-seconds
	 * @param[in] type Sensor type
	 * @retval Non-Negatiev Measurement period in milli-seconds
	 * @retval Negative Failure
	 */
	int (*get_delay)(int32_t type);
	/**
	 * Sets measurment period in milli-seconds
	 * @param[in] type Sensor type
	 * @param[in] delay Measurement period in milli-seconds
	 * @retval Non-Negative The bit or of the sensor type successfully
	 * delay changed.
	 * @retval Negative Failure
	 */
	int (*set_delay)(int32_t type, int delay);
	/**
	 * Reports the sensor status (Enabled or Disabled)
	 * @param[in] type Sensor type
	 * @retval 0 Disabled
	 * @retval 1 Enabled
	 * @retval Negative Failure
	 */
	int (*get_enable)(int32_t type);
	/**
	 * Enables or disables sensors
	 * @param[in] type Sensor type
	 * @param[in] enable The status of the sensor (0: Disable, 1: Enable)
	 * @retval Non-Negative The bit or of the sensor type successfully
	 * enabled/disabled.
	 * @retval Negative Failure
	 */
	int (*set_enable)(int32_t type, int enable);
	/**
	 * Obtains the sensor position
	 * @retval 0-7 The position of the sensor
	 * @retval Negative Failure
	 */
	int (*get_position)(void);
	/**
	 * Sets the sensor position
	 * @param[in] position The position of the sensor (0-7)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_position)(int position);
	/**
	 * Measures the sensor
	 * @param[in] type Sensor type
	 * @param[out] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval Non-Negative The number of the measured sensor data
	 * @retval Negative Failure
	 */
	int (*measure)(int32_t type, struct yas_data *raw, int num);
	/**
	 * Extension command execution specific to the part number
	 * @param[in] cmd Extension command id
	 * @param[out] result Extension command result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*ext)(int32_t cmd, void *result);
	struct yas_driver_callback callback; /*!< Callback functions */
};

#if YAS_MAG_FILTER_ENABLE
/**
 * @struct yas_mag_filter_config
 * @brief Magnetic filter configuration
 */
struct yas_mag_filter_config {
	uint8_t len; /*!< Filter length */
	uint16_t noise; /*!< Filter noise X, Y, Z in [nT] */
};

/**
 * @struct yas_mag_filter
 * @brief Magnetic filter
 */
struct yas_mag_filter {
	/**
	 * Initializes the filter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the filter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the filter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Filters the sensor data
	 * @param[in] input Measured sensor data
	 * @param[out] output Filtered sensor data
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_vector *input, struct yas_vector *output);
	/**
	 * Obtains filter configuration
	 * @param[out] config Sensor filter configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_mag_filter_config *config);
	/**
	 * Sets filter configuration
	 * @param[in] config Sensor filter configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(struct yas_mag_filter_config *config);
};
#endif

#if YAS_MAG_CALIB_ENABLE
/**
 * @struct yas_mag_calib_config
 * @brief Magnetic calibration configuration
 */
struct yas_mag_calib_config {
	uint8_t mode; /*!< Calibration mode : #YAS_MAG_CALIB_MODE_SPHERE,
		    #YAS_MAG_CALIB_MODE_ELLIPSOID,
		    #YAS_MAG_CALIB_MODE_SPHERE_WITH_GYRO,
		    #YAS_MAG_CALIB_MODE_ELLIPSOID_WITH_GYRO */
	uint16_t spread[3]; /*!< Spread threshold for accuracy 1-3
			      (YAS_MAG_CALIB_MODE_SPHERE) */
	uint16_t variation[3]; /*!< Variation threshold for accuracy 1-3
				 (YAS_MAG_CALIB_MODE_SPHERE) */
#if YAS_MAG_CALIB_ELLIPSOID_ENABLE
	uint16_t el_spread[3]; /*!< Spread threshold for accuracy 1-3
				 (YAS_MAG_CALIB_MODE_ELLIPSOID) */
	uint16_t el_variation[3]; /*!< Variation threshold for accuracy 1-3
				    (YAS_MAG_CALIB_MODE_ELLIPSOID) */
#endif
#if !YAS_MAG_CALIB_MINI_ENABLE
	uint16_t trad_variation[3]; /*!< Traditional variation for accuracy 1-3
				     */
#endif
#if YAS_MAG_CALIB_WITH_GYRO_ENABLE
	uint16_t cwg_threshold[12]; /*!< Threshold for calibration with gyro.
				     Order is {eval_th_for_narrow,
				      diff_angle_th_for_narrow,
				      eval_th_for_wide, eval_th_for_wide }
				      for accuracy 1, 2, and 3. */
#endif
};

/**
 * @struct yas_mag_calib_result
 * @brief Magnetic calibration result
 */
struct yas_mag_calib_result {
	struct yas_vector offset; /*!< Calibration offset [nT] */
	uint16_t spread; /*!< Spread value */
	uint16_t variation; /*!< Variation value */
	uint32_t radius; /*!< Magnetic radius [nT] */
	uint8_t axis; /*!< Update axis */
	uint8_t accuracy; /*!< Accuracy [0-3] */
	uint8_t level; /*!< The number of sample */
#if !YAS_MAG_CALIB_MINI_ENABLE
	int success_mode; /*!< Success mode. 1:cwm, 2:cwg. */
	uint16_t trad_variation; /*!< Traditional variation value */
#endif
#if YAS_MAG_CALIB_WITH_GYRO_ENABLE
	uint16_t cwg_spread;
	uint16_t cwg_variation;
#endif
#if YAS_MAG_CALIB_ELLIPSOID_ENABLE
	struct yas_matrix dynamic_matrix;
#endif
};

/**
 * @struct yas_mag_calib
 * @brief Magnetic calibration
 */
struct yas_mag_calib {
	/**
	 * Initializes the calibration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the calibration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the calibration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Updates the calibration
	 * @param[in] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval 0 Calibration offset and accuracy are NOT changed
	 * @retval 1 Calibration offset or accuracy is changed
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_data *raw, int num);
	/**
	 * Obtains the calibration offset
	 * @param[in] type Sensor type
	 * @param[out] offset Calibration offset
	 * @param[out] accuracy Calibration offset accuracy
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_offset)(int type, struct yas_vector *offset,
			uint8_t *accuracy);
	/**
	 * Sets the calibration offset
	 * @param[in] type Sensor type
	 * @param[in] offset Calibration offset
	 * @param[in] accuracy Calibration offset accuracy
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_offset)(int type, struct yas_vector *offset,
			uint8_t accuracy);
	/**
	 * Obtains the calibration configuration
	 * @param[out] config Calibration configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_mag_calib_config *config);
	/**
	 * Sets the calibration configuration
	 * @param[in] config Calibration configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(struct yas_mag_calib_config *config);
#if YAS_MAG_CALIB_ELLIPSOID_ENABLE
	/**
	 * Obtains the dynamic ellipsoid correction matrix
	 * @param[out] m dynamic ellipsoid correction matrix
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_dynamic_matrix)(struct yas_matrix *m);
	/**
	 * Sets the dynamic ellipsoid correction matrix
	 * @param[in] m Dynamic ellipsoid correction matrix
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_dynamic_matrix)(struct yas_matrix *m);
#endif
	/**
	 * Obtains the detail of the last calibration result
	 * @param[out] r Last calibration result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_result)(struct yas_mag_calib_result *r);
};
#endif

#if YAS_GYRO_CALIB_ENABLE
/**
 * @struct yas_gyro_calib_config
 * @brief Gyroscope calibration configuration
 */
struct yas_gyro_calib_config {
	uint16_t mag_noise; /*!< Magnetic sensor noise in standard deviation
			     [nT] */
	uint16_t gyro_noise; /*!< Gyroscope sensor noise in [dps per root HZ] */
};

/**
 * @struct yas_gyro_calib_result
 * @brief Gyroscope calibration result
 */
struct yas_gyro_calib_result {
	struct yas_vector offset; /*!< Calibration offset [mdps] */
	uint8_t accuracy; /*!< Accuracy [0-3] */
};

/**
 * @struct yas_gyro_calib
 * @brief Gyroscope calibration
 */
struct yas_gyro_calib {
	/**
	 * Initializes the calibration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the calibration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the calibration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Updates the calibration
	 * @param[in] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval 0 Calibration offset or accuracy is NOT changed
	 * @retval 1 Calibration offset or accuracy is changed
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_data *raw, int num);
	/**
	 * Obtains the calibration offset
	 * @param[in] type Sensor type
	 * @param[out] offset Calibration offset
	 * @param[out] accuracy Calibration offset accuracy
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_offset)(int type, struct yas_vector *offset,
			uint8_t *accuracy);
	/**
	 * Sets the calibration offset
	 * @param[in] type Sensor type
	 * @param[in] offset Calibration offset
	 * @param[in] accuracy Calibration offset accuracy
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_offset)(int type, struct yas_vector *offset,
			uint8_t accuracy);
	/**
	 * Obtains the calibration configuration
	 * @param[out] config Calibration configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_gyro_calib_config *config);
	/**
	 * Sets the calibration configuration
	 * @param[in] config Calibration configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(struct yas_gyro_calib_config *config);
	/**
	 * Obtains the detail of the last calibration result
	 * @param[out] r Last calibration result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_result)(struct yas_gyro_calib_result *r);
};
#endif

#if YAS_MAG_AVERAGE_FILTER_ENABLE
/**
 * @struct yas_mag_avg_config
 * @brief Magnetic average filter configuration
 */
struct yas_mag_avg_config {
	int tap_min; /*!< Minimum average filter length (0:32, 1:64, 2:128,
			  3:256) */
	int tap_hard; /*!< Average filter length currently set to the sensor
			(0:32, 1:64, 2:128, 3:256) */
	int filter_len; /*!< Average filter length */
	uint32_t dfine; /*!< Measured standard deviation threshold [nT] */
	uint32_t dthresh; /*!< Median standard deviation threshold [nT] */
};

/**
 * @struct yas_mag_avg_result
 * @brief Magnetic average filter result
 */
struct yas_mag_avg_result {
	int32_t tap_new; /*!< New average filter taps
			    (0:32, 1:64, 2:128, 3:256) */
	int32_t dm;	/*!< Median standard deviation */
};

/**
 * @struct yas_mag_avg
 * @brief Magnetic average filter
 */
struct yas_mag_avg {
	/**
	 * Initializes the magnetic average filter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the magnetic average filter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the magnetic average filter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Updates the filter
	 * @param[in] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @param[out] r Average filter result
	 * @retval 0 Average filter taps is NOT changed
	 * @retval 1 Average filter taps is changed
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_data *raw, int num);
	/**
	 * Obtains the average filter tap
	 * @param[out] curtap Current average filter tap (0:32, 1:64, 2:128,
	 * 3:256)
	 * @param[out] newtap Average filter tap to be set (0:32, 1:64, 2:128,
	 * 3:256)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_tap)(int *curtap, int *newtap);
	/**
	 * Sets the average filter tap
	 * @param[in] tap The average filter tap (0:32, 1:64, 2:128, 3:256)
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_tap)(int tap);
	/**
	 * Obtains the average filter configuration
	 * @param[out] config Average filter configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_mag_avg_config *config);
	/**
	 * Sets the average filter configuration
	 * @param[in] config Average filter configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(struct yas_mag_avg_config *config);
	/**
	 * Obtains the detail of filter result
	 * @param[out] r Filter result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_result)(struct yas_mag_avg_result *r);
};
#endif

#if YAS_GAMEVEC_ENABLE
/**
 * @struct yas_gamevec_config
 * @brief Gamevec config
 */
struct yas_gamevec_config {
	int32_t weight;
	int32_t hpf_sq_out_threshold;
	int16_t sustain;
};
#endif

#if YAS_FUSION_ENABLE
/**
 * @struct yas_fusion_config
 * @brief Sensor fusion configuration
 */
struct yas_fusion_config {
	uint8_t mag_fusion_enable; /*!< 6 axis fusion enable or disable */
	uint8_t gyro_fusion_enable; /*!< 9 axis fusion enable or disable */
#if YAS_GAMEVEC_ENABLE
	struct yas_gamevec_config gamevec_config;
#endif
};

/**
 * @struct yas_fusion_result
 * @brief Sensor fusion result
 */
struct yas_fusion_result {
#if YAS_ORIENTATION_ENABLE
	struct yas_vector orientation_mag; /*!< orientation angle (acc and mag)
					  [mdegree].  Azimuth, Pitch, Roll */
#endif
	struct yas_quaternion quaternion_mag; /*!< quaternion (acc and mag)
						[normalized in
						YAS_QUATERNION_NORM] */
#if YAS_GAMEVEC_ENABLE
	struct yas_quaternion quaternion_gyro; /*!< quaternion (gyro)
						 [normalized in
						 YAS_QUATERNION_NORM] */
#endif
#if YAS_FUSION_WITH_GYRO_ENABLE
#if YAS_ORIENTATION_ENABLE
	struct yas_vector orientation_fusion; /*!< orientation angle (acc, mag
						and gyro) [mdegree].  Azimuth,
						Pitch, Roll */
#endif
	struct yas_quaternion quaternion_fusion; /*!< quaternion (acc, mag and
						   gyro) [normalized in
						   YAS_QUATERNION_NORM] */
	struct yas_vector gravity; /*!< Gravity [um/s^2] */
	struct yas_vector linear_acceleration; /*!< Linear acceleration
						 [um/s^2] */
#endif
};

/**
 * @struct yas_fusion
 * @brief Sensor fusion
 */
struct yas_fusion {
	/**
	 * Initializes the sensor fusion
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the sensor fusion
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the sensor fusion
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Notifies the square of offset change.
	 * @param[in] square of offset change in [nT].
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*notify_offset_change)(int32_t square_offset_change);
	/**
	 * Updates the sensor fusion
	 * @param[in] raw Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_data *raw, int num);
	/**
	 * Obtains the sensor fusion configuration
	 * @param[out] config Sensor fusion configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_fusion_config *config);
	/**
	 * Sets the sensor fusion configuration
	 * @param[in] config Sensor fusion configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(struct yas_fusion_config *config);
	/**
	 * Obtains the detail of the last fusion result
	 * @param[out] r Last fusion result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_result)(struct yas_fusion_result *r);
};
#endif

#if YAS_STEPCOUNTER_ENABLE
/**
 * @struct yas_stepcounter_config
 * @brief Stepcounter configuration
 */
struct yas_stepcounter_config {
	int8_t interval; /*!< Step counter calculate interval (ms)
					0 to 20 */
	int8_t noisecancel; /*!< Noise cancel setting
					0: off
					1: on */
};

/**
 * @struct yas_stepcounter_result
 * @brief Stepcounter result
 */
struct yas_stepcounter_result {
	int32_t walk_and_run_count[2];
	int32_t walk_and_run_time[2];
	int32_t totalcount; /*!< Total (walk and run) count */
	int32_t totaltime; /*!<  Total (walk and run) time */
};

/**
 * @struct yas_stepcounter
 * @brief Step counter function
 */
struct yas_stepcounter {
	/**
	 * Initializes the stepcounter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the stepcounter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the stepcounter
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Updates the stepcounter
	 * @param[in] Measured sensor data (accelerometer)
	 * @param[in] num The number of the measured sensor data
	 * @retval 0 Walk count and run count are NOT changed
	 * @retval 1 Walk count or run count is changed
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_data *ydata, int num);
	/**
	 * Obtains the stepcounter configuration
	 * @param[out] config stepcounter configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_stepcounter_config *config);
	/**
	 * Sets the stepcounter configuration
	 * @param[in] config stepcounter configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(struct yas_stepcounter_config *config);
	/**
	 * Obtains the detail of the last stepcounter result
	 * @param[out] Last stepcounter result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_result)(struct yas_stepcounter_result *r);
};
#endif

#if YAS_SIGNIFICANT_MOTION_ENABLE
/**
 * @struct yas_sfm_config
 * @brief Significant motion configuration
 */
struct yas_sfm_config {
	int dummy;
};

/**
 * @struct yas_sfm_result
 * @brief Significant Motion result
 */
struct yas_sfm_result {
	int edge_state;
	int edge_type;
	int acc_count;
	int var_count;
	int err_count;
};

/**
 * @struct yas_sfm
 * @brief Significant motion function
 */
struct yas_sfm {
	/**
	 * Initializes the significant motion
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the significant motion
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the significant motion
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Updates the significant motion
	 * @param[in] Measured sensor data (accelerometer)
	 * @param[in] num The number of the measured sensor data
	 * @retval 0 Walk count and run count are NOT changed
	 * @retval 1 Walk count or run count is changed
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_data *ydata, int num);
	/**
	 * Obtains the significant motion configuration
	 * @param[out] config significant motion configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_sfm_config *config);
	/**
	 * Sets the significant motion configuration
	 * @param[in] config significant motion configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(struct yas_sfm_config *config);
	/**
	 * Obtains the detail of the last significant motion result
	 * @param[out] Last significant motion result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_result)(struct yas_sfm_result *r);
};
#endif

#if YAS_SOFTWARE_GYROSCOPE_ENABLE
/**
 * @struct yas_swgyro_config
 * @brief Software gyroscope configuration
 */
struct yas_swgyro_config {
	int dummy; /*!< dummy */
};

/**
 * @struct yas_swgyro_result
 * @brief Software gyroscope result
 */
struct yas_swgyro_result {
	struct yas_vector swgyro; /*!< Software gyroscope value in mdps */
};

/**
 * @struct yas_swgyro
 * @brief Software gyroscope
 */
struct yas_swgyro {
	/**
	 * Initializes the software gyroscope
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the software gyroscope
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the software gyroscope
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Updates the software gyroscope
	 * @param[in] calibrated Measured sensor data
	 * @param[in] num The number of the measured sensor data
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_data *calibrated, int num);
	/**
	 * Obtains software gyroscope configuration
	 * @param[out] config Software gyroscope configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_swgyro_config *config);
	/**
	 * Sets software gyroscope configuration
	 * @param[in] config Software gyroscope configuration
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(struct yas_swgyro_config *config);
	/**
	 * Obtains the detail of the last software gyroscope result
	 * @param[out] r Last software gyroscope result
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_result)(struct yas_swgyro_result *r);
};
#endif

#if YAS_ATTITUDE_FILTER_ENABLE

struct yas_attitude_filter_result {
	struct yas_quaternion quaternion;
	struct yas_vector gravity; /*!< Gravity [um/s^2] */
	struct yas_vector linear_acceleration; /*!< Linear acceleration
						 [um/s^2] */
};

/**
 * @struct yas_attitude_filter_config
 * @brief Attitude-filter config
 */
struct yas_attitude_filter_config {
	int32_t dt;
	int32_t gamma;
};

/**
 * @struct yas_attitude_filter
 * @brief Attitude-filter.
 */
struct yas_attitude_filter {
	/**
	 * Initializes the attitude_filter.
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*init)(void);
	/**
	 * Terminates the attitude-filter.
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*term)(void);
	/**
	 * Resets the attitude-filter.
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*reset)(void);
	/**
	 * Updates the attitude-filter.
	 * @param[in] data Magnetometer and Accelerometer data.
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*update)(struct yas_data *data, int num);
	/**
	 * Gets a configuration parameter.
	 * @param[out] c A pointer to the configuration parameter.
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_config)(struct yas_attitude_filter_config *c);
	/**
	 * Sets a configuration parameter.
	 * @param[in] c A pointer to the configuration parameter.
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*set_config)(const struct yas_attitude_filter_config *c);
	/**
	 * Gets the last result of attitude-filter.
	 * @param[out] r Result.
	 * @retval #YAS_NO_ERROR Success
	 * @retval Negative Failure
	 */
	int (*get_result)(struct yas_attitude_filter_result *r);
};
#endif

#if YAS_LOG_ENABLE

/**
 * @struct yas_log
 * @brief User-written callback functions for log control
 */
struct yas_log {
	/**
	 * Open the log
	 * @retval 0 Success
	 * @retval Negative Failure
	 */
	int (*log_open)(void);
	/**
	 * Close the log
	 * @retval 0 Success
	 * @retval Negative Failure
	 */
	int (*log_close)(void);
	/**
	 * Write the log
	 * @param[in] buf Log string
	 * @param[in] len Log string length
	 * @retval 0 Success
	 * @retval Negative Failure
	 */
	int (*log_write)(const char *buf, int len);
};
#endif

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS530
struct yas530_self_test_result {
	int32_t id;
	int8_t xy1y2[3];
	int32_t dir;
	int32_t sx, sy;
	int32_t xyz[3];
};
#endif

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS532 \
	|| YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS533
struct yas532_self_test_result {
	int32_t id;
	int8_t xy1y2[3];
	int32_t dir;
	int32_t sx, sy;
	int32_t xyz[3];
};
#endif

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS537
struct yas537_self_test_result {
	int32_t id;
	int32_t dir;
	int32_t sx, sy;
	int32_t xyz[3];
};
#endif

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS539
struct yas539_self_test_result {
	int32_t id;
	int32_t dir;
	int32_t sx, sy;
	int32_t xyz[3];
};
#endif

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS535
struct yas535_acc_self_test_result {
	int dummy; /* TBD */
};
struct yas535_mag_self_test_result {
	int32_t id;
	int8_t xy1y2[3];
	int32_t dir;
	int32_t sx, sy;
	int32_t xyz[3];
};
#endif

/* ----------------------------------------------------------------------------
 *                         Global function definition
 *--------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the acceleration sensor driver module.  Call thie function by
 * specifying a callback function.
 * @param[in,out] f Pointer to yas_acc_driver struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_acc_driver_init(struct yas_acc_driver *f);

/**
 * Initializes the magnetic sensor driver module.  Call thie function by
 * specifying a callback function.
 * @param[in,out] f Pointer to yas_mag_driver struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_mag_driver_init(struct yas_mag_driver *f);

/**
 * Initializes the gyroscope sensor driver module.  Call thie function by
 * specifying a callback function.
 * @param[in,out] f Pointer to yas_gyro_driver struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_gyro_driver_init(struct yas_gyro_driver *f);

/**
 * Initializes the acceleration and gyroscope sensor (6-axis) driver module.
 * Call thie function by specifying a callback function.
 * @param[in,out] f Pointer to yas_acc_gyro_driver struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_acc_gyro_driver_init(struct yas_acc_gyro_driver *f);

/**
 * Initializes the acceleration and magnetic sensor (6-axis) driver module.
 * Call thie function by specifying a callback function.
 * @param[in,out] f Pointer to yas_acc_mag_driver struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_acc_mag_driver_init(struct yas_acc_mag_driver *f);

/**
 * Initializes the acceleration, magnetic and gyroscope sensor (9-axis) driver
 * module.  Call thie function by specifying a callback function.
 * @param[in,out] f Pointer to yas_acc_mag_gyro_driver struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_acc_mag_gyro_driver_init(struct yas_acc_mag_gyro_driver *f);

#if YAS_MAG_CALIB_ENABLE
/**
 * Initializes the magnetic calibration module.
 * @param[in,out] f Pointer to yas_mag_calib struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_mag_calib_init(struct yas_mag_calib *f);
#endif

#if YAS_GYRO_CALIB_ENABLE
/**
 * Initializes the gyroscope calibration module.
 * @param[in,out] f Pointer to yas_gyro_calib struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_gyro_calib_init(struct yas_gyro_calib *f);
#endif

#if YAS_MAG_FILTER_ENABLE
/**
 * Initializes the magnetic filter module.
 * @param[in,out] f Pointer to yas_mag_filter struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_mag_filter_init(struct yas_mag_filter *f);
#endif

#if YAS_MAG_AVERAGE_FILTER_ENABLE
/**
 * Initializes the magnetic average filter module.
 * @param[in,out] f Pointer to yas_mag_avg struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_mag_avg_init(struct yas_mag_avg *f);
#endif

#if YAS_FUSION_ENABLE
/**
 * Initializes the sensor fusion module.
 * @param[in,out] f Pointer to yas_fusion struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_fusion_init(struct yas_fusion *f);
#endif

#if YAS_STEPCOUNTER_ENABLE
/**
 * Initializes the stepcounter module.
 * @param[in,out] f Pointer to yas_stepcounter struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_stepcounter_init(struct yas_stepcounter *f);
#endif

#if YAS_SIGNIFICANT_MOTION_ENABLE
/**
 * Initializes the significant motion module.
 * @param[in,out] f Pointer to yas_sfm struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_sfm_init(struct yas_sfm *f);
#endif

#if YAS_SOFTWARE_GYROSCOPE_ENABLE
/**
 * Initializes the software gyroscope module
 * @param[in,out] f Pointer to yas_swgyro struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_swgyro_init(struct yas_swgyro *f);
#endif

#if YAS_ATTITUDE_FILTER_ENABLE
/**
 * Initializes the filtered-attutde module.
 * @param[in,out] f Pointer to yas_attitude_filter struct
 * @retval #YAS_NO_ERROR Success
 * @retval Negative Number Error
 */
int yas_attitude_filter_init(struct yas_attitude_filter *f);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __YAS_H__ */
