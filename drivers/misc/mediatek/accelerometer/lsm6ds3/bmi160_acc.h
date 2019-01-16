/* BMI160_ACC motion sensor driver
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * Copyright (C) 2018 XiaoMi, Inc.
 * All Rights Reserved
 */

#ifndef BMI160_ACC_H
#define BMI160_ACC_H

#include <linux/ioctl.h>

/* BMI160_ACC Register Map  (Please refer to BMI160_ACC Specifications) */

#define BMI160_ACC_SUCCESS					0
#define BMI160_ACC_ERR_I2C					-1

#define BMI160_BUFSIZE					256
//tad3sgh add ++
#define CONVERT_M			1
#define CONVERT_M_DIV		4
#define CONVERT_O			1
#define CONVERT_O_DIV		71		//(C_PI_F32X * AXIS_RESOLUTION_FACTOR / 180)
#define CONVERT_G			1
#define CONVERT_G_DIV		938
#define CONVERT_VRV			1
#define CONVERT_VRV_DIV	(0x40000000)
#define CONVERT_VLA_DIV	16384
#define CONVERT_VG_DIV 16384
//tad3sgh add --

/* ----------------- bmi160_acc macros --------------------*/
#define BMI160_I2C_ADDR				0x68

#define         C_BMI160_ZERO_U8X                       ((u8)0)
#define         C_BMI160_ONE_U8X                        ((u8)1)
#define         C_BMI160_TWO_U8X                        ((u8)2)
#define         C_BMI160_THREE_U8X                      ((u8)3)
#define         C_BMI160_FOUR_U8X                       ((u8)4)
#define         C_BMI160_FIVE_U8X                       ((u8)5)
#define         C_BMI160_SIX_U8X                        ((u8)6)
#define         C_BMI160_SEVEN_U8X                      ((u8)7)
#define         C_BMI160_EIGHT_U8X                      ((u8)8)
#define         C_BMI160_NINE_U8X                       ((u8)9)
#define         C_BMI160_ELEVEN_U8X                     ((u8)11)
#define         C_BMI160_TWELVE_U8X                     ((u8)12)
#define         C_BMI160_FOURTEEN_U8X                   ((u8)14)
#define         C_BMI160_FIFTEEN_U8X                    ((u8)15)
#define         C_BMI160_SIXTEEN_U8X                    ((u8)16)
#define         C_BMI160_THIRTYONE_U8X                  ((u8)31)
#define         C_BMI160_THIRTYTWO_U8X                  ((u8)32)

#define BMI160_GET_BITSLICE(regvar, bitname)\
		((regvar & bitname##__MSK) >> bitname##__POS)
#define BMI160_SET_BITSLICE(regvar, bitname, val)\
		((regvar & ~bitname##__MSK) | \
		((val<<bitname##__POS)&bitname##__MSK))

#define BMI160_USER_CHIP_ID_ADDR				0x00
/* Chip ID Description - Reg Addr --> 0x00, Bit --> 0...7 */
#define BMI160_USER_CHIP_ID__POS             0
#define BMI160_USER_CHIP_ID__MSK            0xFF
#define BMI160_USER_CHIP_ID__LEN             8
#define BMI160_USER_CHIP_ID__REG             BMI160_USER_CHIP_ID_ADDR

#define SENSOR_CHIP_ID_BMI (0xD0)
#define SENSOR_CHIP_ID_BMI_C2 (0xD1)
#define SENSOR_CHIP_ID_BMI_C3 (0xD3)

#define BMI160_USER_ACC_CONF_ADDR				0X40
/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 0...3 */
#define BMI160_USER_ACC_CONF_ODR__POS               0
#define BMI160_USER_ACC_CONF_ODR__LEN               4
#define BMI160_USER_ACC_CONF_ODR__MSK               0x0F
#define BMI160_USER_ACC_CONF_ODR__REG		BMI160_USER_ACC_CONF_ADDR

#define BMI160_USER_ACC_RANGE_ADDR              0X41
/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACC_RANGE__POS               0
#define BMI160_USER_ACC_RANGE__LEN               4
#define BMI160_USER_ACC_RANGE__MSK               0x0F
#define BMI160_USER_ACC_RANGE__REG               BMI160_USER_ACC_RANGE_ADDR

#define BMI160_USER_INT_EN_0_ADDR				0X50
#define BMI160_USER_INT_EN_1_ADDR               0X51
#define BMI160_USER_INT_EN_2_ADDR               0X52

#define CMD_PMU_ACC_SUSPEND           0x10
#define CMD_PMU_ACC_NORMAL            0x11

#define BMI160_CMD_COMMANDS_ADDR				0X7E
/* Command description address - Reg Addr --> 0x7E, Bit -->  0....7 */
#define BMI160_CMD_COMMANDS__POS              0
#define BMI160_CMD_COMMANDS__LEN              8
#define BMI160_CMD_COMMANDS__MSK              0xFF
#define BMI160_CMD_COMMANDS__REG	 BMI160_CMD_COMMANDS_ADDR

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

#define BMI160_USER_DATA_14_ADDR				0X12
/* ACC_X (LSB) Description - Reg Addr --> 0x12, Bit --> 0...7 */
#define BMI160_USER_DATA_14_ACC_X_LSB__POS           0
#define BMI160_USER_DATA_14_ACC_X_LSB__LEN           8
#define BMI160_USER_DATA_14_ACC_X_LSB__MSK          0xFF
#define BMI160_USER_DATA_14_ACC_X_LSB__REG          BMI160_USER_DATA_14_ADDR

#define BMI160_USER_PMU_STATUS_ADDR				0X03
/* PMU_Status Description of ACCEL - Reg Addr --> 0x03, Bit --> 5...4 */
#define BMI160_USER_ACC_PMU_STATUS__POS               4
#define BMI160_USER_ACC_PMU_STATUS__LEN               2
#define BMI160_USER_ACC_PMU_STATUS__MSK               0x30
#define BMI160_USER_ACC_PMU_STATUS__REG		BMI160_USER_PMU_STATUS_ADDR

#define BMI160_USER_ACC_RANGE_ADDR              0X41
/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACC_RANGE__POS               0
#define BMI160_USER_ACC_RANGE__LEN               4
#define BMI160_USER_ACC_RANGE__MSK               0x0F
#define BMI160_USER_ACC_RANGE__REG               BMI160_USER_ACC_RANGE_ADDR

#define BMI160_ACCEL_RANGE_2G		0x03
#define BMI160_ACCEL_RANGE_4G		0x05
#define BMI160_ACCEL_RANGE_8G		0x08
#define BMI160_ACCEL_RANGE_16G		0x0c

/* BMI160 Accel ODR */
#define BMI160_ACCEL_ODR_RESERVED       0x00
#define BMI160_ACCEL_ODR_0_78HZ         0x01
#define BMI160_ACCEL_ODR_1_56HZ         0x02
#define BMI160_ACCEL_ODR_3_12HZ         0x03
#define BMI160_ACCEL_ODR_6_25HZ         0x04
#define BMI160_ACCEL_ODR_12_5HZ         0x05
#define BMI160_ACCEL_ODR_25HZ           0x06
#define BMI160_ACCEL_ODR_50HZ           0x07
#define BMI160_ACCEL_ODR_100HZ          0x08
#define BMI160_ACCEL_ODR_200HZ          0x09
#define BMI160_ACCEL_ODR_400HZ          0x0A
#define BMI160_ACCEL_ODR_800HZ          0x0B
#define BMI160_ACCEL_ODR_1600HZ         0x0C
#define BMI160_ACCEL_ODR_RESERVED0      0x0D
#define BMI160_ACCEL_ODR_RESERVED1      0x0E
#define BMI160_ACCEL_ODR_RESERVED2      0x0F

#endif
