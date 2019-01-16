/*
 * Copyright (c) 2010 Yamaha Corporation
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

#define YAS_MAG_DRIVER_NONE			(-1)
#define YAS_MAG_DRIVER_YAS529               (1)
#define YAS_MAG_DRIVER_YAS530               (2)
#define YAS_MAG_DRIVER_YAS532			(3)

#define YAS_ACC_DRIVER_NONE			(-1)
#define YAS_ACC_DRIVER_ADXL345              (0)
#define YAS_ACC_DRIVER_ADXL346              (1)
#define YAS_ACC_DRIVER_BMA150               (2)
#define YAS_ACC_DRIVER_BMA222               (3)
#define YAS_ACC_DRIVER_BMA250               (4)
#define YAS_ACC_DRIVER_BMA254			(5)
#define YAS_ACC_DRIVER_KXSD9			(6)
#define YAS_ACC_DRIVER_KXTE9			(7)
#define YAS_ACC_DRIVER_KXTF9			(8)
#define YAS_ACC_DRIVER_KXUD9			(9)
#define YAS_ACC_DRIVER_LIS331DL			(10)
#define YAS_ACC_DRIVER_LIS331DLH		(11)
#define YAS_ACC_DRIVER_LIS331DLM		(12)
#define YAS_ACC_DRIVER_LIS3DH			(13)
#define YAS_ACC_DRIVER_MMA8452Q			(14)
#define YAS_ACC_DRIVER_MMA8453Q			(15)

#define YAS_GYRO_DRIVER_NONE			(-1)
#define YAS_GYRO_DRIVER_ITG3200			(0)
#define YAS_GYRO_DRIVER_L3G4200D		(1)
#define YAS_GYRO_DRIVER_EWTZMU			(2)

/* ----------------------------------------------------------------------------
 *                               Configuration
 *--------------------------------------------------------------------------- */

#define YAS_ACC_DRIVER				(YAS_ACC_DRIVER_LIS3DH)
#define YAS_MAG_DRIVER				(YAS_MAG_DRIVER_YAS532)
#define YAS_GYRO_DRIVER				(YAS_GYRO_DRIVER_NONE)

/* ----------------------------------------------------------------------------
 *                   Acceleration Calibration Configuration
 *--------------------------------------------------------------------------- */

#define YAS_DEFAULT_ACCCALIB_LENGTH         (20)

#if YAS_ACC_DRIVER == YAS_ACC_DRIVER_ADXL345
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (8000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_ADXL346
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (4000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA150
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (4000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA222
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (25000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA250
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (20000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA254
#define YAS_DEFAULT_ACCCALIB_DISTORTION		(4000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXSD9
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (80000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTE9
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (400000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTF9
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (2000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXUD9
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (20000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DL
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (17000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DLH
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (6000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DLM
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (28000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (18000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_MMA8452Q
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (1000)
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_MMA8453Q
#define YAS_DEFAULT_ACCCALIB_DISTORTION     (1000)
#else
#define YAS_DEFAULT_ACCCALIB_DISTORTION		(0)
#endif

/* ----------------------------------------------------------------------------
 *                     Acceleration Filter Configuration
 *--------------------------------------------------------------------------- */

/*----------------------------------------------------------------------------*/
/*                     Accelerometer Filter Configuration                     */
/*----------------------------------------------------------------------------*/
#if YAS_ACC_DRIVER == YAS_ACC_DRIVER_ADXL345
#define YAS_ACC_DEFAULT_FILTER_THRESH       (76612)  /* ((38,306 um/s^2)/count) * 2  */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_ADXL346
#define YAS_ACC_DEFAULT_FILTER_THRESH       (76612)  /* ((38,306 um/s^2)/count) * 2  */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA150
#define YAS_ACC_DEFAULT_FILTER_THRESH       (76612)  /* ((38,306 um/s^2)/count) * 2  */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA222
#define YAS_ACC_DEFAULT_FILTER_THRESH       (153227) /* ((153,227 um/s^2)/count) * 1 */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA250
#define YAS_ACC_DEFAULT_FILTER_THRESH       (76612)  /* ((38,306 um/s^2)/count) * 2  */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_BMA254
#define YAS_ACC_DEFAULT_FILTER_THRESH (19152)  /* ((9,576 um/s^2)/count)*2  */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXSD9
#define YAS_ACC_DEFAULT_FILTER_THRESH       (239460) /* ((11,973 um/s^2)/count) * 20 */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTE9
#define YAS_ACC_DEFAULT_FILTER_THRESH       (612909) /* ((612,909 um/s^2)/count) * 1 */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXTF9
#define YAS_ACC_DEFAULT_FILTER_THRESH       (19152)  /* ((9,576 um/s^2)/count) * 2   */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_KXUD9
#define YAS_ACC_DEFAULT_FILTER_THRESH       (215514) /* ((11.973 um/s^2)/count * 18  */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DL
#define YAS_ACC_DEFAULT_FILTER_THRESH       (176518) /* ((176.518 um/s^2)/count * 1  */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DLH
#define YAS_ACC_DEFAULT_FILTER_THRESH       (95760)  /* ((9.576 um/s^2)/count * 10   */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS331DLM
#define YAS_ACC_DEFAULT_FILTER_THRESH       (306454) /* ((153,227 um/s^2)/count * 2  */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_LIS3DH
#define YAS_ACC_DEFAULT_FILTER_THRESH       (76608)  /* ((9,576 um/s^2)/count * 8    */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_MMA8452Q
#define YAS_ACC_DEFAULT_FILTER_THRESH       (19152)  /* ((9.576 um/s^2)/count * 2    */
#elif YAS_ACC_DRIVER == YAS_ACC_DRIVER_MMA8453Q
#define YAS_ACC_DEFAULT_FILTER_THRESH       (38306)  /* ((38,306 um/s^2)/count * 1   */
#else
#define YAS_ACC_DEFAULT_FILTER_THRESH (0)
#endif

/* ----------------------------------------------------------------------------
 *                   Geomagnetic Calibration Configuration
 *--------------------------------------------------------------------------- */

#undef YAS_MAGCALIB_INSTANTIATE
#define YAS_DEFAULT_MAGCALIB_THRESHOLD      (1)
#define YAS_DEFAULT_MAGCALIB_DISTORTION     (15)
#define YAS_DEFAULT_MAGCALIB_SHAPE          (0)
#define YAS_MAGCALIB_SHAPE_NUM              (2)
#undef YAS_MAG_MANUAL_OFFSET

/* ----------------------------------------------------------------------------
 *                      Geomagnetic Filter Configuration
 *--------------------------------------------------------------------------- */

#define YAS_MAG_MAX_FILTER_LEN              (30)
#define YAS_MAG_I2C_SLAVEADDR               (0x2e)
#define YAS_MAG_DEFAULT_FILTER_NOISE_X      (144) /* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_NOISE_Y      (144) /* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_NOISE_Z      (144) /* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_LEN          (20)

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS529
#define YAS_MAG_DEFAULT_FILTER_THRESH       (300)
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS530
#define YAS_MAG_DEFAULT_FILTER_THRESH       (100)
#elif YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS532
#define YAS_MAG_DEFAULT_FILTER_THRESH		(300)
#else
#define YAS_MAG_DEFAULT_FILTER_THRESH		(0)
#endif

/* ----------------------------------------------------------------------------
 *                    Gyroscope Calibration Configuration
 *--------------------------------------------------------------------------- */

#define YAS_DEFAULT_GYROCALIB_LENGTH		(20)

#if YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_ITG3200
#define YAS_DEFAULT_GYROCALIB_DISTORTION	(0)
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_L3G4200D
#define YAS_DEFAULT_GYROCALIB_DISTORTION	(2500000)
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_EWTZMU
#define YAS_DEFAULT_GYROCALIB_DISTORTION	(400000)
#else
#define YAS_DEFAULT_GYROCALIB_DISTORTION	(0)
#endif

/* ----------------------------------------------------------------------------
 *                       Gyroscope Filter Configuration
 *--------------------------------------------------------------------------- */
#if YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_ITG3200
#define YAS_GYRO_DEFAULT_FILTER_THRESH		(0)
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_L3G4200D
#define YAS_GYRO_DEFAULT_FILTER_THRESH		(280) /* 70mdps x 4 */
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_EWTZMU
#define YAS_GYRO_DEFAULT_FILTER_THRESH		(320) /* 80mdps x 4 */
#else
#define YAS_GYRO_DEFAULT_FILTER_THRESH		(0)
#endif

/* ----------------------------------------------------------------------------
 *                          Gyroscope Configuration
 *--------------------------------------------------------------------------- */

#if YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_ITG3200
#define YAS_GYRO_FIFO_MAX			(1)
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_L3G4200D
#define YAS_GYRO_FIFO_MAX			(32)
#elif YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_EWTZMU
#define YAS_GYRO_FIFO_MAX			(32)
#else
#define YAS_GYRO_FIFO_MAX			(1)
#endif

/* ----------------------------------------------------------------------------
 *                            Other Configuration
 *--------------------------------------------------------------------------- */
#undef YAS_SENSOR_KERNEL_DEVFILE_INTERFACE
#define YAS_SUPPORT_FUSION_DRIVER		(1)
#define YAS_SUPPORT_SOFTWARE_GYROSCOPE		(0)

#if YAS_ACC_DRIVER == YAS_ACC_DRIVER_NONE \
	|| YAS_MAG_DRIVER == YAS_MAG_DRIVER_NONE \
	|| YAS_GYRO_DRIVER == YAS_GYRO_DRIVER_NONE
#undef YAS_SUPPORT_FUSION_DRIVER
#define YAS_SUPPORT_FUSION_DRIVER		(0)
#endif

#if YAS_SUPPORT_SOFTWARE_GYROSCOPE && YAS_GYRO_DRIVER != YAS_GYRO_DRIVER_NONE
#undef YAS_SUPPORT_SOFTWARE_GYROSCOPE
#define YAS_SUPPORT_SOFTWARE_GYROSCOPE		(0)
#endif

#endif
