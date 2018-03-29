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

#ifndef BOSCH_BARO_H
#define BOSCH_BARO_H

#include <linux/ioctl.h>
/*****************************************************
|  sensor  |   chip id  |     7-bit i2c address      |
-----------------------------------------------------|
|  bmp180  |    0x55    |           0x77             |
*****************************************************/

/* apply low pass filter on output */
/*#define CONFIG_BMP_LOWPASS*/
/*#define CONFIG_ID_TEMPERATURE*/
/*#define CONFIG_I2C_BASIC_FUNCTION*/

#define BMP_DRIVER_VERSION "V1.2"

#define BMP_DEV_NAME        "bmp180"

#define C_MAX_FIR_LENGTH (32)
#define MAX_SENSOR_NAME  (32)
#define BMP_DATA_NUM		1
#define BMP_PRESSURE		0
#define BMP_BUFSIZE			128

/* common definition */
#define BMP_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMP_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))


#define BMP_CHIP_ID_REG	0xD0

/*********************************[BMP180]*************************************/
/* chip id */
#define BMP180_CHIP_ID 0x55

/* i2c address */
#define BMP180_I2C_ADDRESS 0x77

/* calibration data */
#define BMP180_CALIBRATION_DATA_START 0xAA
#define BMP180_CALIBRATION_DATA_LENGTH	11	/* 16 bit values */

/* oversampling */
#define BMP180_OVERSAMPLING_1X               0x00
#define BMP180_OVERSAMPLING_2X               0x01
#define BMP180_OVERSAMPLING_4X               0x02
#define BMP180_OVERSAMPLING_8X               0x03

#define BMP180_CTRLMEAS_REG                  0xF4	/* Ctrl Measure Register */

#define BMP180_CTRLMEAS_REG_OSRSP__POS              6
#define BMP180_CTRLMEAS_REG_OSRSP__MSK              0xC0
#define BMP180_CTRLMEAS_REG_OSRSP__LEN              2
#define BMP180_CTRLMEAS_REG_OSRSP__REG              BMP180_CTRLMEAS_REG

/* data */
#define BMP180_TEMP_MEASUREMENT		0x2E
#define BMP180_PRESSURE_MEASUREMENT	0x34
#define BMP180_TEMP_CONVERSION_TIME	(5 + 5)
#define BMP180_CONVERSION_REGISTER_MSB	0xF6

#define BMP180_CTRLMEAS_REG_MC__POS              0
#define BMP180_CTRLMEAS_REG_MC__MSK              0x3F
#define BMP180_CTRLMEAS_REG_MC__LEN              6
#define BMP180_CTRLMEAS_REG_MC__REG              BMP180_CTRLMEAS_REG

#endif				/* BOSCH_BARO_H */
