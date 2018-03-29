/* BOSCH Pressure Sensor Driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _HTS221_H_
#define _HTS221_H_

#include <linux/ioctl.h>
/*****************************************************
|  sensor  |   chip id  |     7-bit i2c address      |
-----------------------------------------------------|
|  bmp180  |    0x55    |           0x77             |
*****************************************************/

/*#define DEBUF*/
/*#define CONFIG_ID_TEMPERATURE*/
/*#define CONFIG_I2C_BASIC_FUNCTION*/

#define HTS_DRIVER_VERSION "V1.5"

#define HTS_DEV_NAME        "hts221"

#define C_MAX_FIR_LENGTH (32)
#define MAX_SENSOR_NAME  (32)
#define HTS221_DATA_NUM		1
#define HTS221_PRESSURE		0
#define HTS221_BUFSIZE			128

/*********************************[HTS221]*************************************/
/* i2c address */
#define HTS221_I2C_ADDRESS		0x5F

/* Enable/Disable Sensor */
#define MASK_ENABLE             (0x80)
#define ENABLE_SENSOR           (0x80)
#define DISABLE_SENSOR          (0x00)
#define START_NEW_CONVERT		(1)
/* OUT PUT RATE */
#define HTS221_ODR_ONE_SHOT		(0)
#define HTS221_ODR_1HZ			(1)
#define HTS221_ODR_7HZ			(2)
#define HTS221_ODR_125HZ		(3)
#define HTS221_ODR_MASK			(0XFC)

/* block data updata */
#define HTS221_BDU				(1<<2)
#define HTS221_BDU_MASK			(0XFB)

/* POWER MODE MASK */
#define HTS221_POWER_MASK		(0X7F)

/* data register */
#define REG_WHOAMI_ADDR         (0x0f)	/** Who am i address register */
#define REG_AVCONFIG_ADDR       (0x10)	/** Humidity res conf register */
#define REG_CNTRL1_ADDR         (0x20)	/** CNTRL1 address register */
#define REG_CNTRL2_ADDR         (0x21)	/** CNTRL2 address register */
#define REG_H_OUT_L             (0x28)	/** OUT humidity address register */
#define REG_H_OUT_H             (0x29)	/** OUT humidity address register */

#define REG_T_OUT_L             (0x2A)	/** OUT temperature address register */
#define REG_T_OUT_H             (0x2B)	/** OUT temperature address register */

/* calibration data */
#define REG_0RH_CAL_X_H         (0X36)	/** Calibration H 0 address register */
#define REG_1RH_CAL_X_H         (0X3a)	/** Calibration H 1 address register */
#define REG_0RH_CAL_Y_H         (0x30)	/** Calibration H 0 RH address register */
#define REG_1RH_CAL_Y_H         (0x31)	/** Calibration H 1 RH address register */
#define REG_0T_CAL_X_L          (0x3c)	/** Calibration T 0 address register */
#define REG_1T_CAL_X_L          (0x3e)	/** Calibration T 1 address register */
#define REG_0T_CAL_Y_H          (0x32)	/** Calibration T 0 C address register */
#define REG_1T_CAL_Y_H          (0x33)	/** Calibration T 1 C address register */
#define REG_STATUS              (0x27)	/** Status address register */

#define REG_T1_T0_CAL_Y_H       (0x35)	/** Calibration T0 and T! Address register  **/

/* Humidity Sensor Resolution */
#define HTS221_H_RESOLUTION_4    (0x00)	/* Resolution set to 0.4 %RH */
#define HTS221_H_RESOLUTION_8    (0x01)	/* Resolution set to 0.3 %RH */
#define HTS221_H_RESOLUTION_16   (0x02)	/* Resolution set to 0.2 %RH */
#define HTS221_H_RESOLUTION_32   (0x03)	/* Resolution set to 0.15 %RH */
#define HTS221_H_RESOLUTION_64   (0x04)	/* Resolution set to 0.1 %RH */
#define HTS221_H_RESOLUTION_128  (0x05)	/* Resolution set to 0.07 %RH */
#define HTS221_H_RESOLUTION_256  (0x06)	/* Resolution set to 0.05 %RH */
#define HTS221_H_RESOLUTION_512  (0x07)	/* Resolution set to 0.03 %RH */

/* Temperature Sensor Resolution */
#define HTS221_T_RESOLUTION_2    (0x00)	/* Resolution set to 0.08 DegC */
#define HTS221_T_RESOLUTION_4    (0x08)	/* Resolution set to 0.05 DegC */
#define HTS221_T_RESOLUTION_8    (0x10)	/* Resolution set to 0.04 DegC */
#define HTS221_T_RESOLUTION_16   (0x18)	/* Resolution set to 0.03 DegC */
#define HTS221_T_RESOLUTION_32   (0x20)	/* Resolution set to 0.02 DegC */
#define HTS221_T_RESOLUTION_64   (0x28)	/* Resolution set to 0.015 DegC */
#define HTS221_T_RESOLUTION_128  (0x30)	/* Resolution set to 0.01 DegC */
#define HTS221_T_RESOLUTION_256  (0x38)	/* Resolution set to 0.007 DegC */

/* Sensor Resolution */
#define HTS221_H_RESOLUTION_MASK (0xf8)
#define HTS221_T_RESOLUTION_MASK (0xC7)

#endif
