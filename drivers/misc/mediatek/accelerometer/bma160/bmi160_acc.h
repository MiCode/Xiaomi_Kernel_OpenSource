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

#define BMI160_ACCEL_OSR4_AVG1                  0x00
#define BMI160_ACCEL_OSR2_AVG2                  0x01

/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 4...6 */
#define BMI160_USER_ACC_CONF_ACC_BWP__POS	4
#define BMI160_USER_ACC_CONF_ACC_BWP__LEN	3
#define BMI160_USER_ACC_CONF_ACC_BWP__MSK	0x70
#define BMI160_USER_ACC_CONF_ACC_BWP__REG	BMI160_USER_ACC_CONF_ADDR

/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 7 */
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__POS		7
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__LEN		1
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__MSK		0x80
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__REG 		BMI160_USER_ACC_CONF_ADDR

#define BMI160_USER_ACC_RANGE_ADDR              0X41
/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACC_RANGE__POS               0
#define BMI160_USER_ACC_RANGE__LEN               4
#define BMI160_USER_ACC_RANGE__MSK               0x0F
#define BMI160_USER_ACC_RANGE__REG               BMI160_USER_ACC_RANGE_ADDR

#define BMI160_USER_FIFO_CONFIG_1_ADDR          0X47

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 4 */
#define BMI160_USER_FIFO_HEADER_EN__POS               4
#define BMI160_USER_FIFO_HEADER_EN__LEN               1
#define BMI160_USER_FIFO_HEADER_EN__MSK               0x10
#define BMI160_USER_FIFO_HEADER_EN__REG         BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 5 */
#define BMI160_USER_FIFO_MAG_EN__POS               5
#define BMI160_USER_FIFO_MAG_EN__LEN               1
#define BMI160_USER_FIFO_MAG_EN__MSK               0x20
#define BMI160_USER_FIFO_MAG_EN__REG            BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 6 */
#define BMI160_USER_FIFO_ACC_EN__POS               6
#define BMI160_USER_FIFO_ACC_EN__LEN               1
#define BMI160_USER_FIFO_ACC_EN__MSK               0x40
#define BMI160_USER_FIFO_ACC_EN__REG            BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 7 */
#define BMI160_USER_FIFO_GYRO_EN__POS               7
#define BMI160_USER_FIFO_GYRO_EN__LEN               1
#define BMI160_USER_FIFO_GYRO_EN__MSK               0x80
#define BMI160_USER_FIFO_GYRO_EN__REG           BMI160_USER_FIFO_CONFIG_1_ADDR
#define BMI160_USER_FIFO_LENGTH_0_ADDR			0X22
#define BMI160_USER_FIFO_LENGTH_1_ADDR			0X23
#define BMI160_USER_FIFO_DATA_ADDR				0X24

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

/* Fifo_Length0 Description - Reg Addr --> 0x22, Bit --> 0...7 */
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__POS           0
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__LEN           8
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__MSK          0xFF
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__REG          \
		BMI160_USER_FIFO_LENGTH_0_ADDR

/*Fifo_Length1 Description - Reg Addr --> 0x23, Bit --> 0...2 */
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__POS           0
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__LEN           3
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__MSK          0x07
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__REG          \
		BMI160_USER_FIFO_LENGTH_1_ADDR

/* Fifo_Data Description - Reg Addr --> 0x24, Bit --> 0...7 */
#define BMI160_USER_FIFO_DATA__POS           0
#define BMI160_USER_FIFO_DATA__LEN           8
#define BMI160_USER_FIFO_DATA__MSK          0xFF
#define BMI160_USER_FIFO_DATA__REG          BMI160_USER_FIFO_DATA_ADDR

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

#define FIFO_FRAME_CNT (20)
#define FIFO_HEAD_SENSOR_TIME        0x44
#define FIFO_HEAD_SKIP_FRAME        0x40
#define FIFO_HEAD_OVER_READ_LSB       0x80
#define FIFO_HEAD_OVER_READ_MSB       0x00

#define CMD_CLR_FIFO_DATA             0xB0

/*! FIFO Head definition*/
#define FIFO_HEAD_A        0x84
#define FIFO_HEAD_G        0x88
#define FIFO_HEAD_M        0x90

#define FIFO_HEAD_G_A        (FIFO_HEAD_G | FIFO_HEAD_A)
#define FIFO_HEAD_M_A        (FIFO_HEAD_M | FIFO_HEAD_A)
#define FIFO_HEAD_M_G        (FIFO_HEAD_M | FIFO_HEAD_G)

#define FIFO_HEAD_M_G_A         (FIFO_HEAD_M | FIFO_HEAD_G | FIFO_HEAD_A)

/*! FIFO head mode Frame bytes number definition */
#define A_BYTES_FRM      6
#define G_BYTES_FRM      6
#define M_BYTES_FRM      8
#define GA_BYTES_FRM     12
#define MG_BYTES_FRM     14
#define MA_BYTES_FRM     14
#define MGA_BYTES_FRM    20

#define ACC_FIFO_HEAD       "acc"
#define GYRO_FIFO_HEAD     "gyro"
#define MAG_FIFO_HEAD         "mag"

#define LMADA     (1)

struct odr_t {
	u8 acc_odr;
	u8 gyro_odr;
	u8 mag_odr;
};

/*bmi fifo sensor type combination*/
enum BMI_FIFO_DATA_SELECT_T {
	BMI_FIFO_A_SEL = 1,
	BMI_FIFO_G_SEL,
	BMI_FIFO_G_A_SEL,
	BMI_FIFO_M_SEL,
	BMI_FIFO_M_A_SEL,
	BMI_FIFO_M_G_SEL,
	BMI_FIFO_M_G_A_SEL,
	BMI_FIFO_DATA_SEL_MAX
};

/*bmi fifo analyse return err status*/
enum BMI_FIFO_ANALYSE_RETURN_T {
        FIFO_OVER_READ_RETURN = -10,
        FIFO_SENSORTIME_RETURN = -9,
        FIFO_SKIP_OVER_LEN = -8,
        FIFO_M_G_A_OVER_LEN = -7,
        FIFO_M_G_OVER_LEN = -6,
        FIFO_M_A_OVER_LEN = -5,
        FIFO_G_A_OVER_LEN = -4,
        FIFO_M_OVER_LEN = -3,
        FIFO_G_OVER_LEN = -2,
        FIFO_A_OVER_LEN = -1
};

struct bmi160acc_t {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

struct bmi160gyro_t {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

/*! bmi sensor time resolution definition*/
enum BMI_SENSOR_TIME_RS_TYPE {
        TS_0_78_HZ = 1,/*0.78HZ*/
        TS_1_56_HZ,/*1.56HZ*/
        TS_3_125_HZ,/*3.125HZ*/
        TS_6_25_HZ,/*6.25HZ*/
        TS_12_5_HZ,/*12.5HZ*/
        TS_25_HZ,/*25HZ, odr=6*/
        TS_50_HZ,/*50HZ*/
        TS_100_HZ,/*100HZ*/
        TS_200_HZ,/*200HZ*/
        TS_400_HZ,/*400HZ*/
        TS_800_HZ,/*800HZ*/
        TS_1600_HZ,/*1600HZ*/
        TS_MAX_HZ
};

/*! bmi sensor time depend on ODR*/
struct bmi_sensor_time_odr_tbl {
        u32 ts_duration_lsb;
        u32 ts_duration_us;
        u32 ts_delat;/*sub current delat fifo_time*/
};

/*! bmi sensor support type*/
enum BMI_SENSOR_TYPE {
	BMI_ACC_SENSOR,
	BMI_GYRO_SENSOR,
	BMI_MAG_SENSOR,
	BMI_SENSOR_TYPE_MAX
};
#endif
