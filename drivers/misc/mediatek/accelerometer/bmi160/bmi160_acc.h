/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 * VERSION: V2.0
 */

#ifndef BMI160_ACC_H
#define BMI160_ACC_H

#include <linux/ioctl.h>

#define COMPATIABLE_NAME		"mediatek,bmi160_acc"
#define BMI160_DEV_NAME        "bmi160_acc"
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               pr_info(GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    pr_err(GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    pr_debug(GSE_TAG fmt, ##args)

#define BMI160_GET_BITSLICE(regvar, bitname)\
		((regvar & bitname##__MSK) >> bitname##__POS)
#define BMI160_SET_BITSLICE(regvar, bitname, val)\
		((regvar & ~bitname##__MSK) | \
		((val<<bitname##__POS)&bitname##__MSK))

#define BMI160_BUS_WRITE_FUNC(dev_addr, reg_addr, reg_data, wr_len)\
				bus_write(dev_addr, reg_addr, reg_data, wr_len)
#define BMI160_BUS_READ_FUNC(dev_addr, reg_addr, reg_data, r_len)\
				bus_read(dev_addr, reg_addr, reg_data, r_len)
#define BMI160_WR_FUNC_PTR s8 (*bus_write)(u8, u8,\
u8 *, u8)
#define BMI160_RD_FUNC_PTR s8 (*bus_read)(u8,\
				u8, u8 *, u8)
#define BMI160_BRD_FUNC_PTR s8 \
	(*burst_read)(u8, u8, u8 *, u32)

#define INPUT_EVENT_SGM     REL_DIAL/*7*/



#define WORK_DELAY_DEFAULT	(200)
#define ABSMIN				(-32768)
#define ABSMAX				(32767)

#define BMI160_GEN_READ_WRITE_DELAY     (1)
#define BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY    (5)

#define  BMI160_MAX_UNDER_SIG_MOTION    (3)
#define	BMI160_MAX_VALUE_SIGNIFICANT_MOTION     (1)
#define BMI160_MDELAY_DATA_TYPE               u32
#define BMI160_ACC_SUCCESS	0
#define BMI160_ACC_ERR_I2C	-1
#define BMI_INT0      0
#define BMI_INT1      1
#define DISABLE       0
#define ENABLE       1
#define BMI_DELAY_MIN		(1)
#define FIFO_FRAME_CNT		(20)
#define FIFO_DATA_BUFSIZE   1024
#define BMI160_BUFSIZE		256
#define CONVERT_M			1
#define CONVERT_M_DIV		4
#define CONVERT_O			1
/* (C_PI_F32X * AXIS_RESOLUTION_FACTOR / 180) */
#define CONVERT_O_DIV		71
#define CONVERT_G			1
#define CONVERT_G_DIV		938
#define CONVERT_VRV			1
#define CONVERT_VRV_DIV		(0x40000000)
#define CONVERT_VLA_DIV		16384
#define CONVERT_VG_DIV		16384
#define CMD_RESET_INT_ENGINE 0xB1
#define	RESET_STEP_COUNTER	(0xB2)
#define BMI160_RETURN_FUNCTION_TYPE			s8
#define BMI160_NULL							(0)
#define E_BMI160_NULL_PTR					((s8)-127)
#define E_BMI160_COMM_RES					((s8)-1)
#define E_BMI160_OUT_OF_RANGE				((s8)-2)
#define E_BMI160_BUSY						((s8)-3)
#define	SUCCESS								((u8)0)
#define	ERROR								((s8)-1)
#define BMI160_INIT_VALUE					(0)
#define BMI160_GEN_READ_WRITE_DATA_LENGTH	(1)
#define BMI160_USER_PMU_TRIGGER_ADDR		(0X6C)

#define BMI160_INTR1_MAP_FIFO_WM		(0)
#define BMI160_INTR2_MAP_FIFO_WM		(1)

#define BMI160_INTR1_LEVEL				(0)
#define BMI160_INTR2_LEVEL				(1)

#define BMI160_INTR1_OUTPUT_ENABLE		(0)
#define BMI160_INTR2_OUTPUT_ENABLE		(1)

#define BMI160_I2C_ADDR					0x68

#define	C_BMI160_ZERO_U8X				((u8)0)
#define C_BMI160_ONE_U8X                ((u8)1)
#define C_BMI160_TWO_U8X                ((u8)2)
#define C_BMI160_THREE_U8X              ((u8)3)
#define C_BMI160_FOUR_U8X               ((u8)4)
#define C_BMI160_FIVE_U8X               ((u8)5)
#define C_BMI160_SIX_U8X                ((u8)6)
#define C_BMI160_SEVEN_U8X              ((u8)7)
#define C_BMI160_EIGHT_U8X              ((u8)8)
#define C_BMI160_NINE_U8X               ((u8)9)
#define C_BMI160_ELEVEN_U8X             ((u8)11)
#define C_BMI160_TWELVE_U8X             ((u8)12)
#define C_BMI160_FOURTEEN_U8X           ((u8)14)
#define C_BMI160_FIFTEEN_U8X            ((u8)15)
#define C_BMI160_SIXTEEN_U8X            ((u8)16)
#define C_BMI160_THIRTYONE_U8X          ((u8)31)
#define C_BMI160_THIRTYTWO_U8X          ((u8)32)

#define C_MAX_FIR_LENGTH			(32)
#define REG_MAX0					0x24
#define REG_MAX1					0x56
#define BMI160_ACC_AXIS_X          0
#define BMI160_ACC_AXIS_Y          1
#define BMI160_ACC_AXIS_Z         2
#define BMI160_ACC_AXES_NUM        3
#define BMI160_ACC_DATA_LEN        6

#define BMI160_ACC_MODE_NORMAL      0
#define BMI160_ACC_MODE_LOWPOWER    1
#define BMI160_ACC_MODE_SUSPEND     2

#define BMI160_USER_CHIP_ID_ADDR		0x00
/* Chip ID Description - Reg Addr --> 0x00, Bit --> 0...7 */
#define BMI160_USER_CHIP_ID__POS        0
#define BMI160_USER_CHIP_ID__MSK        0xFF
#define BMI160_USER_CHIP_ID__LEN       8
#define BMI160_USER_CHIP_ID__REG        BMI160_USER_CHIP_ID_ADDR

#define SENSOR_CHIP_ID_BMI				(0xD0)
#define SENSOR_CHIP_ID_BMI_C2			(0xD1)
#define SENSOR_CHIP_ID_BMI_C3			(0xD3)

#define BMI160_USER_INTR_STAT_0_ADDR	(0X1C)
#define BMI160_USER_INTR_STAT_1_ADDR	(0X1D)
#define BMI160_USER_INTR_STAT_2_ADDR	(0X1E)
#define BMI160_USER_INTR_STAT_3_ADDR	(0X1F)
#define BMI160_USER_INTR_MOTION_1_ADDR	(0X60)
#define BMI160_USER_INTR_MOTION_3_ADDR	(0X62)
#define	BMI160_ENABLE_ANY_MOTION_INTR1	(0x04)
#define	BMI160_ENABLE_ANY_MOTION_INTR2	(0x04)
#define	BMI160_ENABLE_ANY_MOTION_AXIS	(0x07)
#define	BMI160_SIG_MOTION_INTR_ENABLE   (0x01)
#define BMI160_ANY_MOTION_X_ENABLE      (0)
/* INT ENABLE 1 */
#define BMI160_ANY_MOTION_X_ENABLE       (0)
#define BMI160_ANY_MOTION_Y_ENABLE       (1)
#define BMI160_ANY_MOTION_Z_ENABLE       (2)
#define BMI160_DOUBLE_TAP_ENABLE         (4)
#define BMI160_SINGLE_TAP_ENABLE         (5)
#define BMI160_ORIENT_ENABLE             (6)
#define BMI160_FLAT_ENABLE               (7)
#define BMI160_USER_OFFSET_0_ADDR				(0X71)

/***************************************************/
/**\name STEP COUNTER CONFIGURATION REGISTERS*/
/******************************************************/
#define BMI160_USER_STEP_CONFIG_0_ADDR	(0X7A)
#define BMI160_USER_STEP_CONFIG_1_ADDR	(0X7B)
/***************************************************/
/**\name INTERRUPT ENABLE REGISTERS*/
/******************************************************/
#define BMI160_USER_INTR_ENABLE_0_ADDR	(0X50)
#define BMI160_USER_INTR_ENABLE_1_ADDR	(0X51)
#define BMI160_USER_INTR_ENABLE_2_ADDR	(0X52)
#define BMI160_USER_INTR_OUT_CTRL_ADDR	(0X53)

/* Int_Status_1 Description - Reg Addr --> 0x1D, Bit --> 6 */
#define BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__POS	(6)
#define BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__LEN	(1)
#define BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__MSK	(0x40)
#define BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__REG	\
		(BMI160_USER_INTR_STAT_1_ADDR)

#define BMI160_USER_ACC_CONF_ADDR		0X40
/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 0...3 */
#define BMI160_USER_ACC_CONF_ODR__POS   0
#define BMI160_USER_ACC_CONF_ODR__LEN   4
#define BMI160_USER_ACC_CONF_ODR__MSK   0x0F
#define BMI160_USER_ACC_CONF_ODR__REG	BMI160_USER_ACC_CONF_ADDR

#define BMI160_ACCEL_OSR4_AVG1          0x00
#define BMI160_ACCEL_OSR2_AVG2          0x01

/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 4...6 */
#define BMI160_USER_ACC_CONF_ACC_BWP__POS	4
#define BMI160_USER_ACC_CONF_ACC_BWP__LEN	3
#define BMI160_USER_ACC_CONF_ACC_BWP__MSK	0x70
#define BMI160_USER_ACC_CONF_ACC_BWP__REG	BMI160_USER_ACC_CONF_ADDR

/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 7 */
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__POS		7
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__LEN		1
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__MSK		0x80
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__REG   BMI160_USER_ACC_CONF_ADDR

#define BMI160_USER_ACC_RANGE_ADDR          0X41
/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACC_RANGE__POS          0
#define BMI160_USER_ACC_RANGE__LEN          4
#define BMI160_USER_ACC_RANGE__MSK          0x0F
#define BMI160_USER_ACC_RANGE__REG          BMI160_USER_ACC_RANGE_ADDR

#define BMI160_USER_FIFO_CONFIG_0_ADDR      (0X46)
#define BMI160_USER_FIFO_CONFIG_1_ADDR      (0X47)

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 1 */
#define BMI160_USER_FIFO_TIME_ENABLE__POS   (1)
#define BMI160_USER_FIFO_TIME_ENABLE__LEN   (1)
#define BMI160_USER_FIFO_TIME_ENABLE__MSK   (0x02)
#define BMI160_USER_FIFO_TIME_ENABLE__REG      (BMI160_USER_FIFO_CONFIG_1_ADDR)

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 4 */
#define BMI160_USER_FIFO_HEADER_EN__POS		4
#define BMI160_USER_FIFO_HEADER_EN__LEN		1
#define BMI160_USER_FIFO_HEADER_EN__MSK		0x10
#define BMI160_USER_FIFO_HEADER_EN__REG		BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 5 */
#define BMI160_USER_FIFO_MAG_EN__POS        5
#define BMI160_USER_FIFO_MAG_EN__LEN        1
#define BMI160_USER_FIFO_MAG_EN__MSK        0x20
#define BMI160_USER_FIFO_MAG_EN__REG        BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 6 */
#define BMI160_USER_FIFO_ACC_EN__POS        6
#define BMI160_USER_FIFO_ACC_EN__LEN        1
#define BMI160_USER_FIFO_ACC_EN__MSK        0x40
#define BMI160_USER_FIFO_ACC_EN__REG        BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 7 */
#define BMI160_USER_FIFO_GYRO_EN__POS       7
#define BMI160_USER_FIFO_GYRO_EN__LEN       1
#define BMI160_USER_FIFO_GYRO_EN__MSK       0x80
#define BMI160_USER_FIFO_GYRO_EN__REG       BMI160_USER_FIFO_CONFIG_1_ADDR
#define BMI160_USER_FIFO_LENGTH_0_ADDR       0X22
#define BMI160_USER_FIFO_LENGTH_1_ADDR       0X23
#define BMI160_USER_FIFO_DATA_ADDR           0X24
#define BMI160_USER_ACCEL_CONFIG_ADDR        (0X40)
#define BMI160_USER_SELF_TEST_ADDR				(0X6D)

#define BMI160_USER_INT_EN_0_ADDR			0X50
#define BMI160_USER_INT_EN_1_ADDR           0X51
#define BMI160_USER_INT_EN_2_ADDR           0X52

#define BMI160_USER_INTR_LATCH_ADDR	           (0X54)
#define BMI160_USER_INTR_MAP_0_ADDR           (0X55)
#define BMI160_USER_INTR_MAP_1_ADDR           (0X56)
#define BMI160_USER_INTR_MAP_2_ADDR           (0X57)

#define CMD_PMU_ACC_SUSPEND           0x10
#define CMD_PMU_ACC_NORMAL            0x11
#define CMD_PMU_ACC_LP1               0x12
#define CMD_PMU_ACC_LP2               0x13

#define CMD_PMU_GYRO_SUSPEND          0x14
#define CMD_PMU_GYRO_NORMAL           0x15
#define CMD_PMU_GYRO_FASTSTART        0x17
#define BMI160_MAX_UNDER_SAMPLING           (1)

#define BMI160_CMD_COMMANDS_ADDR       0X7E
/* Command description address - Reg Addr --> 0x7E, Bit -->  0....7 */
#define BMI160_CMD_COMMANDS__POS            0
#define BMI160_CMD_COMMANDS__LEN            8
#define BMI160_CMD_COMMANDS__MSK            0xFF
#define BMI160_CMD_COMMANDS__REG            BMI160_CMD_COMMANDS_ADDR
/*! BMI Self test */
#define BMI_SELFTEST_AMP_HIGH       1

/**************************************************/
/**\name	ACCEL ODR          */
/*************************************************/
#define BMI160_ACCEL_OUTPUT_DATA_RATE_RESERVED       (0x00)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_0_78HZ         (0x01)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_1_56HZ         (0x02)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_3_12HZ         (0x03)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_6_25HZ         (0x04)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_12_5HZ         (0x05)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_25HZ           (0x06)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_50HZ           (0x07)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_100HZ          (0x08)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_200HZ          (0x09)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_400HZ          (0x0A)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_800HZ          (0x0B)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_1600HZ         (0x0C)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_RESERVED0      (0x0D)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_RESERVED1      (0x0E)
#define BMI160_ACCEL_OUTPUT_DATA_RATE_RESERVED2      (0x0F)
#define BMI160_MAX_ACCEL_OUTPUT_DATA_RATE   (12)

#define BMI160_SHIFT_1_POSITION             1
#define BMI160_SHIFT_2_POSITION             2
#define BMI160_SHIFT_3_POSITION             3
#define BMI160_SHIFT_4_POSITION             4
#define BMI160_SHIFT_5_POSITION             5
#define BMI160_SHIFT_6_POSITION             6
#define BMI160_SHIFT_7_POSITION             7
#define BMI160_SHIFT_8_POSITION             8
#define BMI160_SHIFT_12_POSITION            12
#define BMI160_SHIFT_16_POSITION            16
#define BMI160_USER_OFFSET_0_ADDR				(0X71)
#define BMI160_USER_OFFSET_1_ADDR				(0X72)
#define BMI160_USER_OFFSET_2_ADDR				(0X73)
#define BMI160_USER_OFFSET_3_ADDR				(0X74)
#define BMI160_USER_OFFSET_4_ADDR				(0X75)
#define BMI160_USER_OFFSET_5_ADDR				(0X76)
#define BMI160_USER_OFFSET_6_ADDR				(0X77)
#define	BMI160_MAX_VALUE_SELFTEST_SIGN  (1)
#define	BMI160_MAX_VALUE_SELFTEST_AMP   (1)
#define	BMI160_MAX_VALUE_SELFTEST_START (1)
#define BMI160_MAX_ACCEL_SELFTEST_AXIS	    (3)
#define	BMI160_ACCEL_X_DATA_SIZE   (2)
#define	BMI160_ACCEL_Y_DATA_SIZE   (2)
#define	BMI160_ACCEL_Z_DATA_SIZE   (2)
#define	BMI160_ACCEL_XYZ_DATA_SIZE (6)
#define BMI160_GYRO_DATA_LENGTH		(2)
#define BMI160_GYRO_XYZ_DATA_LENGTH	(6)
#define BMI160_ACCEL_DATA_LENGTH	(2)
#define BMI160_ACCEL_XYZ_DATA_LENGTH (6)

#define	BMI160_GYRO_X_DATA_SIZE    (2)
#define	BMI160_GYRO_Y_DATA_SIZE    (2)
#define	BMI160_GYRO_Z_DATA_SIZE    (2)
#define	BMI160_GYRO_XYZ_DATA_SIZE  (6)
#define	BMI160_ACCEL_X_LSB_BYTE              (0)
#define	BMI160_ACCEL_X_MSB_BYTE              (1)
#define	BMI160_ACCEL_Y_LSB_BYTE              (0)
#define	BMI160_ACCEL_Y_MSB_BYTE              (1)
#define	BMI160_ACCEL_Z_LSB_BYTE              (0)
#define	BMI160_ACCEL_Z_MSB_BYTE              (1)

#define BMI160_USER_DATA_8_ADDR				(0X0C)
#define BMI160_USER_DATA_9_ADDR				(0X0D)
#define BMI160_USER_DATA_10_ADDR			(0X0E)
#define BMI160_USER_DATA_11_ADDR			(0X0F)
#define BMI160_USER_DATA_12_ADDR			(0X10)
#define BMI160_USER_DATA_13_ADDR			(0X11)
#define BMI160_USER_DATA_14_ADDR			(0X12)
#define BMI160_USER_DATA_15_ADDR			(0X13)
#define BMI160_USER_DATA_16_ADDR			(0X14)
#define BMI160_USER_DATA_17_ADDR			(0X15)
#define BMI160_USER_DATA_18_ADDR			(0X16)
#define BMI160_USER_DATA_19_ADDR			(0X17)
#define BMI160_USER_STAT_ADDR					(0X1B)
#define BMI160_USER_ERROR_ADDR					(0X02)

/* ACC_X (LSB) Description - Reg Addr --> 0x12, Bit --> 0...7 */
#define BMI160_USER_DATA_14_ACC_X_LSB__POS	0
#define BMI160_USER_DATA_14_ACC_X_LSB__LEN	8
#define BMI160_USER_DATA_14_ACC_X_LSB__MSK	0xFF
#define BMI160_USER_DATA_14_ACC_X_LSB__REG	BMI160_USER_DATA_14_ADDR
/* GYR_X (LSB) Description - Reg Addr --> (0x0C), Bit --> 0...7 */
#define BMI160_USER_DATA_8_GYRO_X_LSB__POS  (0)
#define BMI160_USER_DATA_8_GYRO_X_LSB__LEN  (8)
#define BMI160_USER_DATA_8_GYRO_X_LSB__MSK  (0xFF)
#define BMI160_USER_DATA_8_GYRO_X_LSB__REG  (BMI160_USER_DATA_8_ADDR)

#define BMI160_USER_PMU_STATUS_ADDR			0X03
/* PMU_Status Description of ACCEL - Reg Addr --> 0x03, Bit --> 5...4 */
#define BMI160_USER_ACC_PMU_STATUS__POS     4
#define BMI160_USER_ACC_PMU_STATUS__LEN     2
#define BMI160_USER_ACC_PMU_STATUS__MSK     0x30
#define BMI160_USER_ACC_PMU_STATUS__REG		BMI160_USER_PMU_STATUS_ADDR

#define BMI160_USER_ACC_RANGE_ADDR          0X41
/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACC_RANGE__POS          0
#define BMI160_USER_ACC_RANGE__LEN          4
#define BMI160_USER_ACC_RANGE__MSK          0x0F
#define BMI160_USER_ACC_RANGE__REG          BMI160_USER_ACC_RANGE_ADDR
/* Acc_Conf Description - Reg Addr --> (0x40), Bit --> 7 */
#define BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__POS           (7)
#define BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__LEN           (1)
#define BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__MSK           (0x80)
#define BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__REG	\
(BMI160_USER_ACCEL_CONFIG_ADDR)
/* Acc_Conf Description - Reg Addr --> (0x40), Bit --> 0...3 */
#define BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__POS               (0)
#define BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__LEN               (4)
#define BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__MSK               (0x0F)
#define BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__REG		       \
(BMI160_USER_ACCEL_CONFIG_ADDR)
/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACCEL_RANGE__POS               (0)
#define BMI160_USER_ACCEL_RANGE__LEN               (4)
#define BMI160_USER_ACCEL_RANGE__MSK               (0x0F)
#define BMI160_USER_ACCEL_RANGE__REG              \
(BMI160_USER_ACC_RANGE_ADDR)
/* GYR_X (LSB) Description - Reg Addr --> (0x0C), Bit --> 0...7 */
#define BMI160_USER_DATA_8_GYRO_X_LSB__POS           (0)
#define BMI160_USER_DATA_8_GYRO_X_LSB__LEN           (8)
#define BMI160_USER_DATA_8_GYRO_X_LSB__MSK          (0xFF)
#define BMI160_USER_DATA_8_GYRO_X_LSB__REG          (BMI160_USER_DATA_8_ADDR)
/* GYR_X (MSB) Description - Reg Addr --> (0x0D), Bit --> 0...7 */
#define BMI160_USER_DATA_9_GYRO_X_MSB__POS           (0)
#define BMI160_USER_DATA_9_GYRO_X_MSB__LEN           (8)
#define BMI160_USER_DATA_9_GYRO_X_MSB__MSK          (0xFF)
#define BMI160_USER_DATA_9_GYRO_X_MSB__REG          (BMI160_USER_DATA_9_ADDR)
/* GYR_Y (LSB) Description - Reg Addr --> 0x0E, Bit --> 0...7 */
#define BMI160_USER_DATA_10_GYRO_Y_LSB__POS           (0)
#define BMI160_USER_DATA_10_GYRO_Y_LSB__LEN           (8)
#define BMI160_USER_DATA_10_GYRO_Y_LSB__MSK          (0xFF)
#define BMI160_USER_DATA_10_GYRO_Y_LSB__REG          (BMI160_USER_DATA_10_ADDR)
/* GYR_Y (MSB) Description - Reg Addr --> (0x0F), Bit --> 0...7 */
#define BMI160_USER_DATA_11_GYRO_Y_MSB__POS           (0)
#define BMI160_USER_DATA_11_GYRO_Y_MSB__LEN           (8)
#define BMI160_USER_DATA_11_GYRO_Y_MSB__MSK          (0xFF)
#define BMI160_USER_DATA_11_GYRO_Y_MSB__REG          (BMI160_USER_DATA_11_ADDR)
/* GYR_Z (LSB) Description - Reg Addr --> (0x10), Bit --> 0...7 */
#define BMI160_USER_DATA_12_GYRO_Z_LSB__POS           (0)
#define BMI160_USER_DATA_12_GYRO_Z_LSB__LEN           (8)
#define BMI160_USER_DATA_12_GYRO_Z_LSB__MSK          (0xFF)
#define BMI160_USER_DATA_12_GYRO_Z_LSB__REG          (BMI160_USER_DATA_12_ADDR)
/* GYR_Z (MSB) Description - Reg Addr --> (0x11), Bit --> 0...7 */
#define BMI160_USER_DATA_13_GYRO_Z_MSB__POS           (0)
#define BMI160_USER_DATA_13_GYRO_Z_MSB__LEN           (8)
#define BMI160_USER_DATA_13_GYRO_Z_MSB__MSK          (0xFF)
#define BMI160_USER_DATA_13_GYRO_Z_MSB__REG          (BMI160_USER_DATA_13_ADDR)
/**************************************************************/
/**\name	ACCEL DATA XYZ LENGTH, POSITION AND MASK	*/
/**************************************************************/
/* ACC_X (LSB) Description - Reg Addr --> (0x12), Bit --> 0...7 */
#define BMI160_USER_DATA_14_ACCEL_X_LSB__POS           (0)
#define BMI160_USER_DATA_14_ACCEL_X_LSB__LEN           (8)
#define BMI160_USER_DATA_14_ACCEL_X_LSB__MSK          (0xFF)
#define BMI160_USER_DATA_14_ACCEL_X_LSB__REG          (BMI160_USER_DATA_14_ADDR)
/* ACC_X (MSB) Description - Reg Addr --> 0x13, Bit --> 0...7 */
#define BMI160_USER_DATA_15_ACCEL_X_MSB__POS           (0)
#define BMI160_USER_DATA_15_ACCEL_X_MSB__LEN           (8)
#define BMI160_USER_DATA_15_ACCEL_X_MSB__MSK          (0xFF)
#define BMI160_USER_DATA_15_ACCEL_X_MSB__REG          (BMI160_USER_DATA_15_ADDR)
/* ACC_Y (LSB) Description - Reg Addr --> (0x14), Bit --> 0...7 */
#define BMI160_USER_DATA_16_ACCEL_Y_LSB__POS           (0)
#define BMI160_USER_DATA_16_ACCEL_Y_LSB__LEN           (8)
#define BMI160_USER_DATA_16_ACCEL_Y_LSB__MSK          (0xFF)
#define BMI160_USER_DATA_16_ACCEL_Y_LSB__REG          (BMI160_USER_DATA_16_ADDR)
/* ACC_Y (MSB) Description - Reg Addr --> (0x15), Bit --> 0...7 */
#define BMI160_USER_DATA_17_ACCEL_Y_MSB__POS           (0)
#define BMI160_USER_DATA_17_ACCEL_Y_MSB__LEN           (8)
#define BMI160_USER_DATA_17_ACCEL_Y_MSB__MSK          (0xFF)
#define BMI160_USER_DATA_17_ACCEL_Y_MSB__REG          (BMI160_USER_DATA_17_ADDR)
/* ACC_Z (LSB) Description - Reg Addr --> 0x16, Bit --> 0...7 */
#define BMI160_USER_DATA_18_ACCEL_Z_LSB__POS           (0)
#define BMI160_USER_DATA_18_ACCEL_Z_LSB__LEN           (8)
#define BMI160_USER_DATA_18_ACCEL_Z_LSB__MSK          (0xFF)
#define BMI160_USER_DATA_18_ACCEL_Z_LSB__REG          (BMI160_USER_DATA_18_ADDR)
/* ACC_Z (MSB) Description - Reg Addr --> (0x17), Bit --> 0...7 */
#define BMI160_USER_DATA_19_ACCEL_Z_MSB__POS           (0)
#define BMI160_USER_DATA_19_ACCEL_Z_MSB__LEN           (8)
#define BMI160_USER_DATA_19_ACCEL_Z_MSB__MSK          (0xFF)
#define BMI160_USER_DATA_19_ACCEL_Z_MSB__REG          (BMI160_USER_DATA_19_ADDR)

/**************************************************************/
/**\name	ACCEL SELF TEST LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Self_Test Description - Reg Addr --> 0x6d, Bit --> 0...1 */
#define BMI160_USER_ACCEL_SELFTEST_AXIS__POS               (0)
#define BMI160_USER_ACCEL_SELFTEST_AXIS__LEN               (2)
#define BMI160_USER_ACCEL_SELFTEST_AXIS__MSK               (0x03)
#define BMI160_USER_ACCEL_SELFTEST_AXIS__REG	(BMI160_USER_SELF_TEST_ADDR)

/* Self_Test Description - Reg Addr --> 0x6d, Bit --> 2 */
#define BMI160_USER_ACCEL_SELFTEST_SIGN__POS               (2)
#define BMI160_USER_ACCEL_SELFTEST_SIGN__LEN               (1)
#define BMI160_USER_ACCEL_SELFTEST_SIGN__MSK               (0x04)
#define BMI160_USER_ACCEL_SELFTEST_SIGN__REG	(BMI160_USER_SELF_TEST_ADDR)

/* Self_Test Description - Reg Addr --> 0x6d, Bit --> 3 */
#define BMI160_USER_SELFTEST_AMP__POS               (3)
#define BMI160_USER_SELFTEST_AMP__LEN               (1)
#define BMI160_USER_SELFTEST_AMP__MSK               (0x08)
#define BMI160_USER_SELFTEST_AMP__REG		(BMI160_USER_SELF_TEST_ADDR)
/**************************************************************/
/**\name	GYRO SELF TEST LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Self_Test Description - Reg Addr --> 0x6d, Bit --> 4 */
#define BMI160_USER_GYRO_SELFTEST_START__POS               (4)
#define BMI160_USER_GYRO_SELFTEST_START__LEN               (1)
#define BMI160_USER_GYRO_SELFTEST_START__MSK               (0x10)
#define BMI160_USER_GYRO_SELFTEST_START__REG		    \
(BMI160_USER_SELF_TEST_ADDR)
/**************************************************************/
/**\name	GYRO SELF TEST LENGTH, POSITION AND MASK	*/
/**************************************************************/
/* Status Description - Reg Addr --> 0x1B, Bit --> 1 */
#define BMI160_USER_STAT_GYRO_SELFTEST_OK__POS          (1)
#define BMI160_USER_STAT_GYRO_SELFTEST_OK__LEN          (1)
#define BMI160_USER_STAT_GYRO_SELFTEST_OK__MSK          (0x02)
#define BMI160_USER_STAT_GYRO_SELFTEST_OK__REG         \
(BMI160_USER_STAT_ADDR)

/* Fifo_Config_0 Description - Reg Addr --> 0x46, Bit --> 0...7 */
#define BMI160_USER_FIFO_WM__POS            (0)
#define BMI160_USER_FIFO_WM__LEN            (8)
#define BMI160_USER_FIFO_WM__MSK            (0xFF)
#define BMI160_USER_FIFO_WM__REG         (BMI160_USER_FIFO_CONFIG_0_ADDR)

/* Int_En_1 Description - Reg Addr --> (0x51), Bit -->6 */
#define BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__POS	(6)
#define BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__LEN	(1)
#define BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__MSK   (0x40)
#define BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__REG	\
(BMI160_USER_INTR_ENABLE_1_ADDR)

/* Fifo_Length0 Description - Reg Addr --> 0x22, Bit --> 0...7 */
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__POS          0
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__LEN          8
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__MSK          0xFF
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__REG          \
(BMI160_USER_FIFO_LENGTH_0_ADDR)

/*Fifo_Length1 Description - Reg Addr --> 0x23, Bit --> 0...2 */
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__POS          0
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__LEN          3
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__MSK          0x07
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__REG          \
(BMI160_USER_FIFO_LENGTH_1_ADDR)

/* Fifo_Data Description - Reg Addr --> 0x24, Bit --> 0...7 */
#define BMI160_USER_FIFO_DATA__POS          0
#define BMI160_USER_FIFO_DATA__LEN          8
#define BMI160_USER_FIFO_DATA__MSK          0xFF
#define BMI160_USER_FIFO_DATA__REG          BMI160_USER_FIFO_DATA_ADDR
/**************************************************************/
/**\name	ERROR STATUS LENGTH, POSITION AND MASK    */
/**************************************************************/
/* Error Description - Reg Addr --> (0x02), Bit --> 0 */
#define BMI160_USER_ERR_STAT__POS               (0)
#define BMI160_USER_ERR_STAT__LEN               (8)
#define BMI160_USER_ERR_STAT__MSK               (0xFF)
#define BMI160_USER_ERR_STAT__REG               (BMI160_USER_ERROR_ADDR)

#define BMI160_USER_FATAL_ERR__POS               (0)
#define BMI160_USER_FATAL_ERR__LEN               (1)
#define BMI160_USER_FATAL_ERR__MSK               (0x01)
#define BMI160_USER_FATAL_ERR__REG               (BMI160_USER_ERROR_ADDR)

/* Error Description - Reg Addr --> (0x02), Bit --> 1...4 */
#define BMI160_USER_ERR_CODE__POS               (1)
#define BMI160_USER_ERR_CODE__LEN               (4)
#define BMI160_USER_ERR_CODE__MSK               (0x1E)
#define BMI160_USER_ERR_CODE__REG               (BMI160_USER_ERROR_ADDR)

/* Error Description - Reg Addr --> (0x02), Bit --> 5 */
#define BMI160_USER_I2C_FAIL_ERR__POS               (5)
#define BMI160_USER_I2C_FAIL_ERR__LEN               (1)
#define BMI160_USER_I2C_FAIL_ERR__MSK               (0x20)
#define BMI160_USER_I2C_FAIL_ERR__REG               (BMI160_USER_ERROR_ADDR)

/* Error Description - Reg Addr --> (0x02), Bit --> 6 */
#define BMI160_USER_DROP_CMD_ERR__POS              (6)
#define BMI160_USER_DROP_CMD_ERR__LEN              (1)
#define BMI160_USER_DROP_CMD_ERR__MSK              (0x40)
#define BMI160_USER_DROP_CMD_ERR__REG              (BMI160_USER_ERROR_ADDR)
/**************************************************************/
/**\name	MAG DATA READY LENGTH, POSITION AND MASK    */
/**************************************************************/
/* Error Description - Reg Addr --> (0x02), Bit --> 7 */
#define BMI160_USER_MAG_DADA_RDY_ERR__POS               (7)
#define BMI160_USER_MAG_DADA_RDY_ERR__LEN               (1)
#define BMI160_USER_MAG_DADA_RDY_ERR__MSK               (0x80)
#define BMI160_USER_MAG_DADA_RDY_ERR__REG               (BMI160_USER_ERROR_ADDR)


/**************************************************************/
/**\name	SIGNIFICANT MOTION SELECT LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Motion_3 Description - Reg Addr --> (0x62), Bit --> 1 */
#define BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__POS		(1)
#define BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__LEN		(1)
#define BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__MSK		(0x02)
#define BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__REG		\
		(BMI160_USER_INTR_MOTION_3_ADDR)
/**************************************************************/
/**\name	ANY MOTION THRESHOLD LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Motion_1 Description - Reg Addr --> (0x60), Bit --> 0...7 */
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__POS    (0)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__LEN    (8)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__MSK    (0xFF)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__REG	\
		(BMI160_USER_INTR_MOTION_1_ADDR)
/**************************************************************/
/**\name	ANY MOTION THRESHOLD LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Motion_1 Description - Reg Addr --> (0x60), Bit --> 0...7 */
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__POS     (0)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__LEN     (8)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__MSK     (0xFF)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__REG     \
		(BMI160_USER_INTR_MOTION_1_ADDR)
/* Int_Motion_3 Description - Reg Addr --> (0x62), Bit --> 3..2 */
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__POS		(2)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__LEN		(2)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__MSK		(0x0C)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__REG		\
		(BMI160_USER_INTR_MOTION_3_ADDR)
/* Int_Motion_3 Description - Reg Addr --> (0x62), Bit --> 5..4 */
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__POS		(4)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__LEN		(2)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__MSK		(0x30)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__REG		\
		(BMI160_USER_INTR_MOTION_3_ADDR)
/* Int_En_2 Description - Reg Addr --> (0x52), Bit -->3 */
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__POS		(3)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__LEN		(1)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__MSK		(0x08)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__REG		\
	(BMI160_USER_INTR_ENABLE_2_ADDR)

/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 1 */
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__POS	(1)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__LEN	(1)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__MSK	(0x02)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__REG   \
		(BMI160_USER_INTR_STAT_0_ADDR)
/* STEP_CONFIG_1  Description - Reg Addr --> 0x7B, Bit -->	0 to 2 */
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__POS	(3)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__LEN	(1)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__MSK	(0x08)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG	\
	(BMI160_USER_STEP_CONFIG_1_ADDR)

/**************************************************************/
/**\name	INTERRUPT STATUS LENGTH, POSITION AND MASK    */
/**************************************************************/
/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 0 */
#define BMI160_USER_INTR_STAT_0_STEP_INTR__POS           (0)
#define BMI160_USER_INTR_STAT_0_STEP_INTR__LEN           (1)
#define BMI160_USER_INTR_STAT_0_STEP_INTR__MSK          (0x01)
#define BMI160_USER_INTR_STAT_0_STEP_INTR__REG          \
		(BMI160_USER_INTR_STAT_0_ADDR)

#define BMI160_ACCEL_RANGE_2G		0x03
#define BMI160_ACCEL_RANGE_4G		0x05
#define BMI160_ACCEL_RANGE_8G		0x08
#define BMI160_ACCEL_RANGE_16G		0x0c
/* accel range check*/
#define BMI160_ACCEL_RANGE0  (3)
#define BMI160_ACCEL_RANGE1  (5)
#define BMI160_ACCEL_RANGE3  (8)
#define BMI160_ACCEL_RANGE4  (12)

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

#define FIFO_HEAD_SENSOR_TIME       0x44
#define FIFO_HEAD_SKIP_FRAME        0x40
#define FIFO_HEAD_OVER_READ_LSB     0x80
#define FIFO_HEAD_OVER_READ_MSB     0x00

#define CMD_CLR_FIFO_DATA           0xB0
#define CMD_RESET_USER_REG            0xB6

/****************************************************/
/**\name	SHIFT VALUE DEFINITION       */
/***************************************************/
#define BMI160_SHIFT_BIT_POSITION_BY_01_BIT      (1)
#define BMI160_SHIFT_BIT_POSITION_BY_02_BITS     (2)
#define BMI160_SHIFT_BIT_POSITION_BY_03_BITS     (3)
#define BMI160_SHIFT_BIT_POSITION_BY_04_BITS     (4)
#define BMI160_SHIFT_BIT_POSITION_BY_05_BITS     (5)
#define BMI160_SHIFT_BIT_POSITION_BY_06_BITS     (6)
#define BMI160_SHIFT_BIT_POSITION_BY_07_BITS     (7)
#define BMI160_SHIFT_BIT_POSITION_BY_08_BITS     (8)
#define BMI160_SHIFT_BIT_POSITION_BY_09_BITS     (9)
#define BMI160_SHIFT_BIT_POSITION_BY_12_BITS     (12)
#define BMI160_SHIFT_BIT_POSITION_BY_13_BITS     (13)
#define BMI160_SHIFT_BIT_POSITION_BY_14_BITS     (14)
#define BMI160_SHIFT_BIT_POSITION_BY_15_BITS     (15)
#define BMI160_SHIFT_BIT_POSITION_BY_16_BITS     (16)

/**\name	STEP COUNTER LENGTH, POSITION AND MASK*/
/**************************************************************/
/* STEP_CNT_0  Description - Reg Addr --> 0x78, Bit -->  0 to 7 */
#define BMI160_USER_STEP_COUNT_LSB__POS               (0)
#define BMI160_USER_STEP_COUNT_LSB__LEN               (7)
#define BMI160_USER_STEP_COUNT_LSB__MSK               (0xFF)
#define BMI160_USER_STEP_COUNT_LSB__REG	 (BMI160_USER_STEP_COUNT_0_ADDR)

/* STEP_CNT_1  Description - Reg Addr --> 0x79, Bit -->  0 to 7 */
#define BMI160_USER_STEP_COUNT_MSB__POS               (0)
#define BMI160_USER_STEP_COUNT_MSB__LEN               (7)
#define BMI160_USER_STEP_COUNT_MSB__MSK               (0xFF)
#define BMI160_USER_STEP_COUNT_MSB__REG	 (BMI160_USER_STEP_COUNT_1_ADDR)
/**************************************************************/
/**\name	STEP COUNTER CONFIGURATION LENGTH, POSITION AND MASK*/
/**************************************************************/
/* STEP_CONFIG_0  Description - Reg Addr --> 0x7A, Bit -->  0 to 7 */
#define BMI160_USER_STEP_CONFIG_ZERO__POS               (0)
#define BMI160_USER_STEP_CONFIG_ZERO__LEN               (7)
#define BMI160_USER_STEP_CONFIG_ZERO__MSK               (0xFF)
#define BMI160_USER_STEP_CONFIG_ZERO__REG	 \
(BMI160_USER_STEP_CONFIG_0_ADDR)


/* STEP_CONFIG_1  Description - Reg Addr --> 0x7B, Bit -->  0 to 2 and
4 to 7 */
#define BMI160_USER_STEP_CONFIG_ONE_CNF1__POS               (0)
#define BMI160_USER_STEP_CONFIG_ONE_CNF1__LEN               (3)
#define BMI160_USER_STEP_CONFIG_ONE_CNF1__MSK               (0x07)
#define BMI160_USER_STEP_CONFIG_ONE_CNF1__REG	 \
(BMI160_USER_STEP_CONFIG_1_ADDR)

#define BMI160_USER_STEP_CONFIG_ONE_CNF2__POS               (4)
#define BMI160_USER_STEP_CONFIG_ONE_CNF2__LEN               (4)
#define BMI160_USER_STEP_CONFIG_ONE_CNF2__MSK               (0xF0)
#define BMI160_USER_STEP_CONFIG_ONE_CNF2__REG	 \
(BMI160_USER_STEP_CONFIG_1_ADDR)
/**************************************************************/
/**\name	STEP COUNTER ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* STEP_CONFIG_1  Description - Reg Addr --> 0x7B, Bit -->  0 to 2 */
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__POS		(3)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__LEN		(1)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__MSK		(0x08)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG	\
(BMI160_USER_STEP_CONFIG_1_ADDR)

/**\name	STEP DETECTION SELECTION MODES      */
/**\name	STEP CONFIGURATION MASK DEFINITION   */
/*************************************************/
#define BMI160_STEP_CONFIG_0_7		(0x00FF)
#define BMI160_STEP_CONFIG_8_10		(0x0700)
#define BMI160_STEP_CONFIG_11_14	(0xF000)

/*************************************************/
#define	BMI160_STEP_NORMAL_MODE			(0)
#define	BMI160_STEP_SENSITIVE_MODE		(1)
#define	BMI160_STEP_ROBUST_MODE			(2)
/**************************************************/
/**\name	STEP CONFIGURATION SELECT MODE    */
/*************************************************/
#define	STEP_CONFIG_NORMAL		(0X315)
#define	STEP_CONFIG_SENSITIVE	(0X2D)
#define	STEP_CONFIG_ROBUST		(0X71D)
#define	STEP_CONFIG_HIGHBUFFER_SENSITIVE	(0X0F2D)

/*! FIFO Head definition*/
#define FIFO_HEAD_A        0x84
#define FIFO_HEAD_G        0x88
#define FIFO_HEAD_M        0x90

#define FIFO_HEAD_G_A       (FIFO_HEAD_G | FIFO_HEAD_A)
#define FIFO_HEAD_M_A       (FIFO_HEAD_M | FIFO_HEAD_A)
#define FIFO_HEAD_M_G       (FIFO_HEAD_M | FIFO_HEAD_G)
#define FIFO_HEAD_M_G_A       (FIFO_HEAD_M | FIFO_HEAD_G | FIFO_HEAD_A)

/*! FIFO head mode Frame bytes number definition */
#define A_BYTES_FRM      6
#define G_BYTES_FRM      6
#define M_BYTES_FRM      8
#define GA_BYTES_FRM     12
#define MG_BYTES_FRM     14
#define MA_BYTES_FRM     14
#define MGA_BYTES_FRM    20

#define ACC_FIFO_HEAD	"acc"
#define GYRO_FIFO_HEAD	"gyro"
#define MAG_FIFO_HEAD	"mag"
#define LMADA     (1)

struct bmi160_t {
	u8 chip_id;/**< chip id of BMI160 */
	u8 dev_addr;/**< device address of BMI160 */
	s8 mag_manual_enable;/**< used for check the mag manual/auto mode status */
	BMI160_WR_FUNC_PTR;/**< bus write function pointer */
	BMI160_RD_FUNC_PTR;/**< bus read function pointer */
	BMI160_BRD_FUNC_PTR;/**< burst write function pointer */
	void (*delay_msec)(BMI160_MDELAY_DATA_TYPE);/**< delay function pointer */
};

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

/*! bmi sensor support type*/
enum BMI_SENSOR_TYPE {
	BMI_ACC_SENSOR,
	BMI_GYRO_SENSOR,
	BMI_MAG_SENSOR,
	BMI_SENSOR_TYPE_MAX
};
#endif
