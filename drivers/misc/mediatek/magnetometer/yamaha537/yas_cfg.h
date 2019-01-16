/**
 * Configuration header file of the core driver API @file yas_cfg.h
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
#ifndef __YAS_CFG_H__
#define __YAS_CFG_H__

#define YAS_MAG_DRIVER_NONE			(0) /*!< No Magnetometer */
#define YAS_MAG_DRIVER_YAS529			(1) /*!< YAS 529 (MS-3C) */
#define YAS_MAG_DRIVER_YAS530			(2) /*!< YAS 530 (MS-3E) */
#define YAS_MAG_DRIVER_YAS532			(3) /*!< YAS 532 (MS-3R) */
#define YAS_MAG_DRIVER_YAS533			(4) /*!< YAS 533 (MS-3F) */
#define YAS_MAG_DRIVER_YAS535			(5) /*!< YAS 535 (MS-6C) */
#define YAS_MAG_DRIVER_YAS536			(6) /*!< YAS 536 (MS-3W) */
#define YAS_MAG_DRIVER_YAS537			(7) /*!< YAS 537 (MS-3T) */
#define YAS_MAG_DRIVER_YAS539			(8) /*!< YAS 539 (MS-3S) */
#define YAS_MAG_DRIVER_YAS53x			(0x7fff) /*!< YAS XXX */

#define YAS_ACC_DRIVER_NONE			(0) /*!< No Accelerometer */
#define YAS_ACC_DRIVER_ADXL345			(1) /*!< ADXL 345 */
#define YAS_ACC_DRIVER_ADXL346			(2) /*!< ADXL 346 */
#define YAS_ACC_DRIVER_BMA150			(3) /*!< BMA 150 */
#define YAS_ACC_DRIVER_BMA222			(4) /*!< BMA 222 */
#define YAS_ACC_DRIVER_BMA222E			(5) /*!< BMA 222E */
#define YAS_ACC_DRIVER_BMA250			(6) /*!< BMA 250 */
#define YAS_ACC_DRIVER_BMA250E			(7) /*!< BMA 250E */
#define YAS_ACC_DRIVER_BMA254			(8) /*!< BMA 254 */
#define YAS_ACC_DRIVER_BMA255			(9) /*!< BMA 255 */
#define YAS_ACC_DRIVER_BMI055			(10) /*!< BMI 055 */
#define YAS_ACC_DRIVER_BMI058			(11) /*!< BMI 058 */
#define YAS_ACC_DRIVER_DMARD08			(12) /*!< DMARD08 */
#define YAS_ACC_DRIVER_KXSD9			(13) /*!< KXSD9 */
#define YAS_ACC_DRIVER_KXTE9			(14) /*!< KXTE9 */
#define YAS_ACC_DRIVER_KXTF9			(15) /*!< KXTF9 */
#define YAS_ACC_DRIVER_KXTI9			(16) /*!< KXTI9 */
#define YAS_ACC_DRIVER_KXTJ2			(17) /*!< KXTJ2 */
#define YAS_ACC_DRIVER_KXUD9			(18) /*!< KXUD9 */
#define YAS_ACC_DRIVER_LIS331DL			(19) /*!< LIS331DL */
#define YAS_ACC_DRIVER_LIS331DLH		(20) /*!< LIS331DLH */
#define YAS_ACC_DRIVER_LIS331DLM		(21) /*!< LIS331DLM */
#define YAS_ACC_DRIVER_LIS3DH			(22) /*!< LIS3DH */
#define YAS_ACC_DRIVER_LSM330DLC		(23) /*!< LSM330DLC */
#define YAS_ACC_DRIVER_MMA8452Q			(24) /*!< MMA8452Q */
#define YAS_ACC_DRIVER_MMA8453Q			(25) /*!< MMA8453Q */
#define YAS_ACC_DRIVER_U2DH			(26) /*!< U2DH */
#define YAS_ACC_DRIVER_YAS535			(27) /*!< YAS 535 (MS-6C) */
#define YAS_ACC_DRIVER_YAS53x			(0x7fff) /*!< YAS XXX */

#define YAS_GYRO_DRIVER_NONE			(0) /*!< No Gyroscope */
#define YAS_GYRO_DRIVER_BMG160			(1) /*!< BMG160 */
#define YAS_GYRO_DRIVER_BMI055			(2) /*!< BMI055 */
#define YAS_GYRO_DRIVER_BMI058			(3) /*!< BMI058 */
#define YAS_GYRO_DRIVER_EWTZMU			(4) /*!< EWTZMU */
#define YAS_GYRO_DRIVER_ITG3200			(5) /*!< ITG3200 */
#define YAS_GYRO_DRIVER_ITG3500			(6) /*!< ITG3500 */
#define YAS_GYRO_DRIVER_L3G3200D		(7) /*!< L3G3200D */
#define YAS_GYRO_DRIVER_L3G4200D		(8) /*!< L3G4200D */
#define YAS_GYRO_DRIVER_LSM330DLC		(9) /*!< LSM330DLC */
#define YAS_GYRO_DRIVER_MPU3050			(10) /*!< MPU3050 */
#define YAS_GYRO_DRIVER_MPU6050			(11) /*!< MPU6050 */
#define YAS_GYRO_DRIVER_YAS53x			(0x7fff) /*!< YAS XXX */

/*----------------------------------------------------------------------------
 *                               Configuration
 *----------------------------------------------------------------------------*/

#define YAS_ACC_DRIVER				(YAS_ACC_DRIVER_BMI055)
#define YAS_MAG_DRIVER				(YAS_MAG_DRIVER_YAS537)
#define YAS_GYRO_DRIVER				(YAS_GYRO_DRIVER_BMI055)

/*! Magnetic driver interrupt enable (0:Disable, 1: Enable) */
#define YAS_MAG_DRIVER_INTERRUPT_ENABLE		(0)
/*! Magnetic driver interrupt active HIGH (0:active LOW, 1: active HIGH) */
#define YAS_MAG_DRIVER_ACTIVE_HIGH		(0)

/*! Magnetic minimum calibration enable (0:Disable, 1: Enable) */
#define YAS_MAG_CALIB_MINI_ENABLE		(0)
/*! Magnetic floating point calibration enable (0:Disable, 1: Enable) */
#define YAS_MAG_CALIB_FLOAT_ENABLE		(0)
/*! Magnetic sphere calibration enable (0:Disable, 1: Enable) */
#define YAS_MAG_CALIB_SPHERE_ENABLE		(1)
/*! Magnetic ellipsoid calibration enable (0:Disable, 1: Enable) */
#define YAS_MAG_CALIB_ELLIPSOID_ENABLE		(1)
/*! Magnetic calibration with gyroscope enable (0:Disable, 1: Enable) */
#define YAS_MAG_CALIB_WITH_GYRO_ENABLE		(1)

#if YAS_MAG_CALIB_MINI_ENABLE
#undef YAS_MAG_CALIB_FLOAT_ENABLE
#undef YAS_MAG_CALIB_SPHERE_ENABLE
#undef YAS_MAG_CALIB_ELLIPSOID_ENABLE
#undef YAS_MAG_CALIB_WITH_GYRO_ENABLE
#define YAS_MAG_CALIB_FLOAT_ENABLE		(0)
#define YAS_MAG_CALIB_SPHERE_ENABLE		(0)
#define YAS_MAG_CALIB_ELLIPSOID_ENABLE		(0)
#define YAS_MAG_CALIB_WITH_GYRO_ENABLE		(0)
#elif YAS_MAG_CALIB_FLOAT_ENABLE
#undef YAS_MAG_CALIB_WITH_GYRO_ENABLE
#define YAS_MAG_CALIB_WITH_GYRO_ENABLE		(0)
#endif
/*! Magnetic calibration enable (0:Disable, 1: Enable) */
#define YAS_MAG_CALIB_ENABLE	(YAS_MAG_CALIB_FLOAT_ENABLE | \
		YAS_MAG_CALIB_MINI_ENABLE | \
		YAS_MAG_CALIB_SPHERE_ENABLE | \
		YAS_MAG_CALIB_ELLIPSOID_ENABLE | \
		YAS_MAG_CALIB_WITH_GYRO_ENABLE)

/*! Gyroscope calibration enable (0:Disable, 1: Enable) */
#define YAS_GYRO_CALIB_ENABLE			(1)
/*! Magnetic filter enable (0:Disable, 1: Enable) */
#define YAS_MAG_FILTER_ENABLE			(1)
/*! Fusion with gyroscope enable (0:Disable, 1: Enable) */
#define YAS_FUSION_ENABLE			(1)
/*! Fusion with gyroscope enable (0:Disable, 1: Enable) */
#define YAS_FUSION_WITH_GYRO_ENABLE		(1)
/*! Quaternion (gyroscope) enable (0:Disable, 1: Enable) */
#define YAS_GAMEVEC_ENABLE			(1)
/*! Magnetic average filter enable (0:Disable, 1:Enable) */
#define YAS_MAG_AVERAGE_FILTER_ENABLE		(0)
/*! step counter enable (0:Disable, 1:Enable) */
#define YAS_STEPCOUNTER_ENABLE			(0)
/*! Significant motion enable (0:Disable, 1:Enable) */
#define YAS_SIGNIFICANT_MOTION_ENABLE		(0)
/*! Software gyroscope enable (0:Disable, 1:Enable) */
#define YAS_SOFTWARE_GYROSCOPE_ENABLE		(0)
/* Acc and Mag 6 Axis attitude filter enable (0: Disable, 1: Enable) */
#define YAS_ATTITUDE_FILTER_ENABLE		(0)
/*! Log enable (0:Disable, 1:Enable) */
#define YAS_LOG_ENABLE				(0)
/*! Orientation enable (0:Disable, 1:Enable) */
#define YAS_ORIENTATION_ENABLE			(1)

/*! Mangetic vdd in mV */
#define YAS_MAG_VCORE				(1800)

/*! No sleep version of YAS532 driver */
#define YAS532_DRIVER_NO_SLEEP			(0)

/* ----------------------------------------------------------------------------
 *                            Driver Configuration
 *--------------------------------------------------------------------------- */
/*! Default sensor delay in [msec] */
#define YAS_DEFAULT_SENSOR_DELAY		(50)

/* ----------------------------------------------------------------------------
 *                      Geomagnetic Filter Configuration
 *--------------------------------------------------------------------------- */

/*! Geomagnetic adaptive filter noise threshold (dispersion in [nT]) */
#define YAS_MAG_DEFAULT_FILTER_NOISE		(1800)
/*! Geomagnetic adaptive filter length */
#define YAS_MAG_DEFAULT_FILTER_LEN		(30)

/* ----------------------------------------------------------------------------
 *                           Other Configuration
 *--------------------------------------------------------------------------- */

#if YAS_ACC_DRIVER == YAS_ACC_DRIVER_NONE
#undef YAS_STEPCOUNTER_ENABLE
#define YAS_STEPCOUNTER_ENABLE			(0)
#undef YAS_SIGNIFICANT_MOTION_ENABLE
#define YAS_SIGNIFICANT_MOTION_ENABLE		(0)
#endif

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_NONE
#undef YAS_MAG_CALIB_ENABLE
#define YAS_MAG_CALIB_ENABLE			(0)
#undef YAS_MAG_FILTER_ENABLE
#define YAS_MAG_FILTER_ENABLE			(0)
#endif
#if YAS_MAG_DRIVER != YAS_MAG_DRIVER_YAS536
#undef YAS_MAG_AVERAGE_FILTER_ENABLE
#define YAS_MAG_AVERAGE_FILTER_ENABLE		(0)
#endif

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_NONE \
		    || YAS_ACC_DRIVER == YAS_ACC_DRIVER_NONE
#undef YAS_SOFTWARE_GYROSCOPE_ENABLE
#define YAS_SOFTWARE_GYROSCOPE_ENABLE		(0)
#undef YAS_FUSION_ENABLE
#define YAS_FUSION_ENABLE			(0)
#endif

#if YAS_ACC_DRIVER == YAS_ACC_DRIVER_NONE \
		    || YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_NONE
#undef YAS_GAMEVEC_ENABLE
#define YAS_GAMEVEC_ENABLE			(0)
#endif

#if YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_NONE \
		     || YAS_MAG_DRIVER == YAS_MAG_DRIVER_NONE
#undef YAS_GYRO_CALIB_ENABLE
#define YAS_GYRO_CALIB_ENABLE			(0)
#endif

#if YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_NONE
#undef YAS_FUSION_WITH_GYRO_ENABLE
#define YAS_FUSION_WITH_GYRO_ENABLE		(0)
#endif

#if !YAS_FUSION_ENABLE
#undef YAS_FUSION_WITH_GYRO_ENABLE
#define YAS_FUSION_WITH_GYRO_ENABLE		(0)
#endif

#if YAS_LOG_ENABLE
#ifdef __KERNEL__
#undef YAS_LOG_ENABLE
#define YAS_LOG_ENABLE				(0)
#else
#include <stdio.h>
#include <string.h>
#endif
#endif

/*! yas magnetometer name */
#define YAS_MAG_NAME		"yas_magnetometer"
/*! yas accelerometer name */
#define YAS_ACC_NAME		"yas_accelerometer"
/*! yas accelerometer and magnetometer 6axis sensor name */
#define YAS_ACC_MAG_NAME	"yas_acc_mag_6axis"
/*! yas accelerometer and gyroscope 6axis sensor name */
#define YAS_ACC_GYRO_NAME	"yas_acc_gyro_6axis"
/*! yas gyroscope name */
#define YAS_GYRO_NAME		"yas_gyroscope"

#endif
