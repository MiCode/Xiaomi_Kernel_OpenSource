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

#ifndef BMI160_GYRO_H
#define BMI160_GYRO_H

#include <linux/ioctl.h>
/*************************************************************
| sensor | chip id | bit number |     7-bit i2c address      |
-------------------------------------------------------------|
| bmg160 |  0x0F   |     16     |0x68(SDO:low),0x69(SDO:high)|
*************************************************************/
/*
 * configuration
*/
#define BMG_DRIVER_VERSION "V1.3"

/* apply low pass filter on output */
/* #define CONFIG_BMG_LOWPASS */

#define BMG_AXIS_X				0
#define BMG_AXIS_Y				1
#define BMG_AXIS_Z				2
#define BMG_AXES_NUM				3
#define BMG_DATA_LEN				6

#define C_MAX_FIR_LENGTH			(32)
#define MAX_SENSOR_NAME				(32)

/* common definition */
#define BMG_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMG_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define BMG_BUFSIZE				128

/*
*1 rad = PI*degree/180, about 3.1416*degree/180
*1 degree = rad*180/PI, about rad*180/3.1416
*/
#define DEGREE_TO_RAD				938

/* -----------------  BMI160 gyro macros  ---------------- */
#define SW_CALIBRATION

#define BMG_DEV_NAME				"bmi160_gyro"
#define BMI160_GYRO_I2C_ADDRESS			0x66

#define BMI160_USER_CHIP_ID_ADDR				0x00
/* USER DATA REGISTERS DEFINITION START */
/* Chip ID Description - Reg Addr --> 0x00, Bit --> 0...7 */
#define BMI160_USER_CHIP_ID__POS             0
#define BMI160_USER_CHIP_ID__MSK            0xFF
#define BMI160_USER_CHIP_ID__LEN             8
#define BMI160_USER_CHIP_ID__REG             BMI160_USER_CHIP_ID_ADDR


#define SENSOR_CHIP_ID_BMI (0xD0)
#define SENSOR_CHIP_ID_BMI_C2 (0xD1)
#define SENSOR_CHIP_ID_BMI_C3 (0xD3)

#define SENSOR_CHIP_REV_ID_BMI (0x00)

/* BMI160 Gyro ODR */
#define BMI160_GYRO_ODR_RESERVED		0x00
#define BMI160_GYRO_ODR_25HZ			0x06
#define BMI160_GYRO_ODR_50HZ			0x07
#define BMI160_GYRO_ODR_100HZ			0x08
#define BMI160_GYRO_ODR_200HZ			0x09
#define BMI160_GYRO_ODR_400HZ			0x0A
#define BMI160_GYRO_ODR_800HZ			0x0B
#define BMI160_GYRO_ODR_1600HZ			0x0C
#define BMI160_GYRO_ODR_3200HZ			0x0D

#define         C_BMI160_FOURTEEN_U8X                   ((u8)14)

#define BMI160_USER_GYR_CONF_ADDR               0X42
/* Gyro_Conf Description - Reg Addr --> 0x42, Bit --> 0...3 */
#define BMI160_USER_GYR_CONF_ODR__POS               0
#define BMI160_USER_GYR_CONF_ODR__LEN               4
#define BMI160_USER_GYR_CONF_ODR__MSK               0x0F
#define BMI160_USER_GYR_CONF_ODR__REG               BMI160_USER_GYR_CONF_ADDR

/* range */
#define BMI160_RANGE_2000			0
#define BMI160_RANGE_1000			1
#define BMI160_RANGE_500			2
#define BMI160_RANGE_250			3
#define BMI160_RANGE_125			4

#define BMI160_USER_GYR_RANGE_ADDR              0X43
/* Gyr_Range Description - Reg Addr --> 0x43, Bit --> 0...2 */
#define BMI160_USER_GYR_RANGE__POS               0
#define BMI160_USER_GYR_RANGE__LEN               3
#define BMI160_USER_GYR_RANGE__MSK               0x07
#define BMI160_USER_GYR_RANGE__REG               BMI160_USER_GYR_RANGE_ADDR

#define BMI160_CMD_COMMANDS_ADDR				0X7E
/* Command description address - Reg Addr --> 0x7E, Bit -->  0....7 */
#define BMI160_CMD_COMMANDS__POS              0
#define BMI160_CMD_COMMANDS__LEN              8
#define BMI160_CMD_COMMANDS__MSK              0xFF
#define BMI160_CMD_COMMANDS__REG	 BMI160_CMD_COMMANDS_ADDR

#define CMD_PMU_GYRO_SUSPEND          0x14
#define CMD_PMU_GYRO_NORMAL           0x15
#define CMD_PMU_GYRO_FASTSTART        0x17

#define BMI160_SHIFT_1_POSITION                 1
#define BMI160_SHIFT_2_POSITION                 2
#define BMI160_SHIFT_3_POSITION                 3
#define BMI160_SHIFT_4_POSITION                 4
#define BMI160_SHIFT_5_POSITION                 5
#define BMI160_SHIFT_6_POSITION                 6
#define BMI160_SHIFT_7_POSITION                 7
#define BMI160_SHIFT_8_POSITION                 8
#define BMI160_SHIFT_12_POSITION                12
#define BMI160_SHIFT_16_POSITION                16

#define BMI160_USER_DATA_8_ADDR					0X0C
/* GYR_X (LSB) Description - Reg Addr --> 0x0C, Bit --> 0...7 */
#define BMI160_USER_DATA_8_GYR_X_LSB__POS           0
#define BMI160_USER_DATA_8_GYR_X_LSB__LEN           8
#define BMI160_USER_DATA_8_GYR_X_LSB__MSK          0xFF
#define BMI160_USER_DATA_8_GYR_X_LSB__REG          BMI160_USER_DATA_8_ADDR

#endif/* BMI160_GYRO_H */
