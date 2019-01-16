/* BOSCH GYROSCOPE Sensor Driver Header File
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

#ifndef BMG160_H
#define BMG160_H

#include <linux/ioctl.h>
/*************************************************************
| sensor | chip id | bit number |     7-bit i2c address      |
-------------------------------------------------------------|
| bmg160 |  0x0F   |     16      |0x68(SDO:low),0x69(SDO:high)|
*************************************************************/
/*
 * configuration
*/
#define BMG_DRIVER_VERSION "V1.00"

/* apply low pass filter on output */
/* #define CONFIG_BMG_LOWPASS */
#define SW_CALIBRATION

#define BMG_AXIS_X				0
#define BMG_AXIS_Y				1
#define BMG_AXIS_Z				2
#define BMG_AXES_NUM				3
#define BMG_DATA_LEN				6
#define BMG_DEV_NAME				"bmg160"

#define C_MAX_FIR_LENGTH			(32)
#define MAX_SENSOR_NAME				(32)

/* common definition */
#define BMG_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMG_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define BMG_CHIP_ID_REG				0x00
#define BMG_BUFSIZE				128

/*
*1 rad = PI*degree/180, about 3.1416*degree/180
*1 degree = rad*180/PI, about rad*180/3.1416
*/
#define DEGREE_TO_RAD				57

/*********************************[BMG160]*************************************/
/* chip id */
#define BMG160_CHIP_ID				0x0F

/* i2c address */
/*
*7-bit addr: 0x68 (SDO connected to GND); 0x69 (SDO connected to VDDIO)
*/
#define BMG160_I2C_ADDRESS			0x68

/* bandwidth */
#define C_BMG160_No_Filter_U8X			0
#define	C_BMG160_BW_230Hz_U8X			1
#define	C_BMG160_BW_116Hz_U8X			2
#define	C_BMG160_BW_47Hz_U8X			3
#define	C_BMG160_BW_23Hz_U8X			4
#define	C_BMG160_BW_12Hz_U8X			5
#define	C_BMG160_BW_64Hz_U8X			6
#define	C_BMG160_BW_32Hz_U8X			7

#define BMG160_BW_ADDR				0x10

#define BMG160_BW_ADDR__POS			0
#define BMG160_BW_ADDR__LEN			3
#define BMG160_BW_ADDR__MSK			0x07
#define BMG160_BW_ADDR__REG			BMG160_BW_ADDR

/* range */
#define BMG160_RANGE_2000			0
#define BMG160_RANGE_1000			1
#define BMG160_RANGE_500			2
#define BMG160_RANGE_250			3
#define BMG160_RANGE_125			4

#define BMG160_RANGE_ADDR			0x0F

#define BMG160_RANGE_ADDR_RANGE__POS		0
#define BMG160_RANGE_ADDR_RANGE__LEN		3
#define BMG160_RANGE_ADDR_RANGE__MSK		0x07
#define BMG160_RANGE_ADDR_RANGE__REG		BMG160_RANGE_ADDR

/* power mode */
#define	BMG160_SUSPEND_MODE			4
#define BMG160_NORMAL_MODE			0

#define BMG160_MODE_LPM1_ADDR			0x11

#define BMG160_MODE_LPM1__POS			5
#define BMG160_MODE_LPM1__LEN			3
#define BMG160_MODE_LPM1__MSK			0xA0
#define BMG160_MODE_LPM1__REG			BMG160_MODE_LPM1_ADDR

/* data */
#define BMG160_RATE_X_LSB_ADDR			0x02

#endif/* BMG160_H */
