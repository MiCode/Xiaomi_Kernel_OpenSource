/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 * VERSION: V2.0
 * Date: 2016/12/08
 */

#ifndef BMI160_STC_H
#define BMI160_STC_H

#include <linux/ioctl.h>

extern struct i2c_client *bmi160_acc_i2c_client;

#define STC_DRIVER_VERSION	"V1.0"
#define SENSOR_NAME		"step_counter"
#define UNKNOWN_SENSOR		"unknown sensor"
#define COMPATIABLE_NAME	"mediatek,m_step_c_pl"
#define STC_DEV_NAME		"step_counter"
#define BMI160_ENABLE_INT1	1
#define BMI160_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMI160_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/*according i2c_mt6516.c */
#define C_I2C_FIFO_SIZE         8
#define C_MAX_FIR_LENGTH		(32)
#define MAX_SENSOR_NAME			(32)
#define BMG_BUFSIZE				128
#define E_BMI160_OUT_OF_RANGE	((s8)-2)
#define BMI160_ACC_ERR_I2C		-1
#define	C_BMI160_ZERO_U8X		((u8)0)
#define SENSOR_CHIP_ID_BMI		(0xD0)
#define SENSOR_CHIP_ID_BMI_C2 (0xD1)
#define SENSOR_CHIP_ID_BMI_C3 (0xD3)
#define SENSOR_CHIP_REV_ID_BMI (0x00)
#define BMI160_GYRO_I2C_ADDRESS	0x66
#define BMI160_I2C_ADDR			0x68
#define	ENABLE					(1)
#define	DISABLE					(0)

#define BMI160_ACC_MODE_NORMAL      0
#define BMI160_ACC_MODE_LOWPOWER    1
#define BMI160_ACC_MODE_SUSPEND     2

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
#define	BMI160_MAX_VALUE_SIGNIFICANT_MOTION	(1)
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
#define C_BMI160_FOURTEEN_U8X           ((u8)14)
#define BMI160_USER_GYR_RANGE_ADDR      0X43
#define BMI160_CMD_COMMANDS_ADDR		0X7E

#define CMD_PMU_ACC_SUSPEND           0x10
#define CMD_PMU_ACC_NORMAL            0x11
#define CMD_PMU_ACC_LP1               0x12
#define CMD_PMU_ACC_LP2               0x13

#define CMD_PMU_GYRO_SUSPEND          0x14
#define CMD_PMU_GYRO_NORMAL           0x15
#define CMD_PMU_GYRO_FASTSTART        0x17
#define BMI160_SHIFT_1_POSITION       1
#define BMI160_SHIFT_2_POSITION       2
#define BMI160_SHIFT_3_POSITION       3
#define BMI160_SHIFT_4_POSITION       4
#define BMI160_SHIFT_5_POSITION       5
#define BMI160_SHIFT_6_POSITION       6
#define BMI160_SHIFT_7_POSITION       7
#define BMI160_SHIFT_8_POSITION       8
#define BMI160_SHIFT_12_POSITION      12
#define BMI160_SHIFT_16_POSITION      16
#define BMI160_USER_CHIP_ID_ADDR		0x00
#define BMI160_USER_STEP_COUNT_0_ADDR	(0X78)
#define	BMI160_STEP_COUNT_LSB_BYTE    (0)
#define	BMI160_STEP_COUNT_MSB_BYTE    (1)
#define	BMI160_STEP_COUNT_LSB_BYTE    (0)
#define BMI160_STEP_COUNTER_LENGTH	(2)
#define BMI160_GEN_READ_WRITE_DATA_LENGTH	(1)
#define BMI160_SHIFT_BIT_POSITION_BY_08_BITS	(8)

#define BMI160_USER_INTR_MOTION_1_ADDR	(0X60)
#define BMI160_USER_INTR_MOTION_3_ADDR	(0X62)
#define	BMI160_ENABLE_ANY_MOTION_INTR1	(0x04)
#define	BMI160_ENABLE_ANY_MOTION_INTR2	(0x04)
#define	BMI160_ENABLE_ANY_MOTION_AXIS	(0x07)
#define	BMI160_SIG_MOTION_INTR_ENABLE   (0x01)
/* INT ENABLE 1 */
#define BMI160_ANY_MOTION_X_ENABLE       (0)
#define BMI160_ANY_MOTION_Y_ENABLE       (1)
#define BMI160_ANY_MOTION_Z_ENABLE       (2)
#define BMI160_DOUBLE_TAP_ENABLE         (4)
#define BMI160_SINGLE_TAP_ENABLE         (5)
#define BMI160_ORIENT_ENABLE             (6)
#define BMI160_FLAT_ENABLE               (7)
/***************************************************/
/**\name INTERRUPT ENABLE REGISTERS*/
/******************************************************/
#define BMI160_USER_INTR_ENABLE_0_ADDR	(0X50)
#define BMI160_USER_INTR_ENABLE_1_ADDR	(0X51)
#define BMI160_USER_INTR_ENABLE_2_ADDR	(0X52)
#define BMI160_USER_INTR_OUT_CTRL_ADDR	(0X53)
/***************************************************/
/**\name STEP COUNTER CONFIGURATION REGISTERS*/
/******************************************************/
#define BMI160_USER_STEP_CONFIG_0_ADDR		(0X7A)
#define BMI160_USER_STEP_CONFIG_1_ADDR		(0X7B)
#define BMI160_USER_INTR_ENABLE_2_ADDR		(0X52)
/**************************************************************/
/**\name	STEP COUNTER ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* STEP_CONFIG_1  Description - Reg Addr --> 0x7B, Bit -->  0 to 2 */
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__POS	(3)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__LEN	(1)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__MSK	(0x08)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG	\
(BMI160_USER_STEP_CONFIG_1_ADDR)
/**************************************************************/
/**\name	STEP COUNTER LENGTH, POSITION AND MASK*/
/**************************************************************/
/* STEP_CNT_0  Description - Reg Addr --> 0x78, Bit -->  0 to 7 */
#define BMI160_USER_STEP_COUNT_LSB__POS		(0)
#define BMI160_USER_STEP_COUNT_LSB__LEN		(7)
#define BMI160_USER_STEP_COUNT_LSB__MSK		(0xFF)
#define BMI160_USER_STEP_COUNT_LSB__REG	(BMI160_USER_STEP_COUNT_0_ADDR)
/**************************************************************/
/**\name	STEP DETECTOR ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_2 Description - Reg Addr --> (0x52), Bit -->3 */
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__POS		(3)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__LEN		(1)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__MSK		(0x08)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__REG		\
(BMI160_USER_INTR_ENABLE_2_ADDR)
/* USER DATA REGISTERS DEFINITION START */
/* Chip ID Description - Reg Addr --> 0x00, Bit --> 0...7 */
#define BMI160_USER_CHIP_ID__POS		0
#define BMI160_USER_CHIP_ID__MSK        0xFF
#define BMI160_USER_CHIP_ID__LEN        8
#define BMI160_USER_CHIP_ID__REG        BMI160_USER_CHIP_ID_ADDR

#define BMI160_USER_ACC_CONF_ADDR		0X40
/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 0...3 */
#define BMI160_USER_ACC_CONF_ODR__POS   0
#define BMI160_USER_ACC_CONF_ODR__LEN   4
#define BMI160_USER_ACC_CONF_ODR__MSK   0x0F
#define BMI160_USER_ACC_CONF_ODR__REG	BMI160_USER_ACC_CONF_ADDR

/* Command description address - Reg Addr --> 0x7E, Bit -->  0....7 */
#define BMI160_CMD_COMMANDS__POS        0
#define BMI160_CMD_COMMANDS__LEN        8
#define BMI160_CMD_COMMANDS__MSK        0xFF
#define BMI160_CMD_COMMANDS__REG	BMI160_CMD_COMMANDS_ADDR

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
/**\name	ANY MOTION XYZ AXIS ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->0 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__POS		(0)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__LEN      (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__MSK      (0x01)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__REG	    \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->1 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__POS      (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__LEN      (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__MSK      (0x02)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__REG	    \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->2 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__POS       (2)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__LEN       (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__MSK       (0x04)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__REG	     \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	DOUBLE TAP ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->4 */
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__POS          (4)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__LEN          (1)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__MSK          (0x10)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__REG	       \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	SINGLE TAP ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->5 */
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__POS           (5)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__LEN           (1)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__MSK           (0x20)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__REG	       \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	ORIENT ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->6 */
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__POS	(6)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__LEN    (1)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__MSK    (0x40)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__REG	\
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	FLAT ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->7 */
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__POS		(7)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__LEN      (1)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__MSK      (0x80)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__REG	    \
(BMI160_USER_INTR_ENABLE_0_ADDR)
#define BMI160_USER_PMU_STATUS_ADDR			0X03
/* PMU_Status Description of ACCEL - Reg Addr --> 0x03, Bit --> 5...4 */
#define BMI160_USER_ACC_PMU_STATUS__POS     4
#define BMI160_USER_ACC_PMU_STATUS__LEN     2
#define BMI160_USER_ACC_PMU_STATUS__MSK     0x30
#define BMI160_USER_ACC_PMU_STATUS__REG		BMI160_USER_PMU_STATUS_ADDR
/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 7 */
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__POS		7
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__LEN		1
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__MSK		0x80
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__REG	\
	BMI160_USER_ACC_CONF_ADDR
/* IN OUT CONTROL */
#define BMI160_INTR1_EDGE_CTRL			(0)
#define BMI160_INTR2_EDGE_CTRL			(1)
#define BMI160_INTR1_LEVEL				(0)
#define BMI160_INTR2_LEVEL				(1)
#define BMI160_INTR1_OUTPUT_TYPE		(0)
#define BMI160_INTR2_OUTPUT_TYPE		(1)
#define BMI160_INTR1_OUTPUT_ENABLE		(0)
#define BMI160_INTR2_OUTPUT_ENABLE		(1)
#define BMI160_INTR1_INPUT_ENABLE		(0)
#define BMI160_INTR2_INPUT_ENABLE		(1)
#define BMI160_SIG_MOTION_STAT_HIGH		(1)
/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->2 */
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__POS     (2)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__LEN     (1)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__MSK     (0x04)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__REG     \
		(BMI160_USER_INTR_MAP_2_ADDR)
/**************************************************************/
/**\name	ANY MOTION XYZ AXIS ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->0 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__POS		(0)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__LEN      (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__MSK      (0x01)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__REG	    \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->1 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__POS      (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__LEN      (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__MSK      (0x02)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__REG	    \
(BMI160_USER_INTR_ENABLE_0_ADDR)
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->2 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__POS       (2)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__LEN       (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__MSK       (0x04)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__REG	     \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	DOUBLE TAP ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->4 */
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__POS          (4)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__LEN          (1)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__MSK          (0x10)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__REG	       \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	SINGLE TAP ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->5 */
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__POS           (5)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__LEN           (1)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__MSK           (0x20)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__REG	       \
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	ORIENT ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->6 */
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__POS	(6)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__LEN    (1)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__MSK    (0x40)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__REG	\
		(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	FLAT ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->7 */
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__POS		(7)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__LEN      (1)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__MSK      (0x80)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__REG	    \
(BMI160_USER_INTR_ENABLE_0_ADDR)
	/**************************************************************/
	/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->0 */
#define BMI160_USER_INTR1_EDGE_CTRL__POS		(0)
#define BMI160_USER_INTR1_EDGE_CTRL__LEN		(1)
#define BMI160_USER_INTR1_EDGE_CTRL__MSK        (0x01)
#define BMI160_USER_INTR1_EDGE_CTRL__REG		\
	(BMI160_USER_INTR_OUT_CTRL_ADDR)
	/**************************************************************/
	/**\name	EDGE CONTROL ENABLE LENGTH, POSITION AND MASK*/
	/**************************************************************/
	/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->4 */
#define BMI160_USER_INTR2_EDGE_CTRL__POS		(4)
#define BMI160_USER_INTR2_EDGE_CTRL__LEN        (1)
#define BMI160_USER_INTR2_EDGE_CTRL__MSK        (0x10)
#define BMI160_USER_INTR2_EDGE_CTRL__REG		\
	(BMI160_USER_INTR_OUT_CTRL_ADDR)
/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->1 */
#define BMI160_USER_INTR1_LEVEL__POS        (1)
#define BMI160_USER_INTR1_LEVEL__LEN        (1)
#define BMI160_USER_INTR1_LEVEL__MSK        (0x02)
#define BMI160_USER_INTR1_LEVEL__REG        \
(BMI160_USER_INTR_OUT_CTRL_ADDR)
/**************************************************************/
/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->3 */
#define BMI160_USER_INTR1_OUTPUT_ENABLE__POS	(3)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__LEN    (1)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__MSK    (0x08)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__REG	\
(BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->5 */
#define BMI160_USER_INTR2_LEVEL__POS		(5)
#define BMI160_USER_INTR2_LEVEL__LEN		(1)
#define BMI160_USER_INTR2_LEVEL__MSK		(0x20)
#define BMI160_USER_INTR2_LEVEL__REG         \
(BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->7 */
#define BMI160_USER_INTR2_OUTPUT_EN__POS       (7)
#define BMI160_USER_INTR2_OUTPUT_EN__LEN       (1)
#define BMI160_USER_INTR2_OUTPUT_EN__MSK       (0x80)
#define BMI160_USER_INTR2_OUTPUT_EN__REG		\
(BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->6 */
#define BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__POS  (6)
#define BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__LEN  (1)
#define BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__MSK   (0x40)
#define BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__REG	\
(BMI160_USER_INTR_MAP_1_ADDR)

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->2 */
#define BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__POS   (2)
#define BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__LEN   (1)
#define BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__MSK   (0x04)
#define BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__REG	\
(BMI160_USER_INTR_MAP_1_ADDR)

/**************************************************************/
/**\name	EDGE CONTROL ENABLE LENGTH, POSITION AND MASK*/

/**\name	STEP DETECTOR ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_2 Description - Reg Addr --> (0x52), Bit -->3 */
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__POS	(3)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__MSK (0x08)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__REG	  \
(BMI160_USER_INTR_ENABLE_2_ADDR)
/**************************************************************/
/**\name	STEP COUNTER ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* STEP_CONFIG_1  Description - Reg Addr --> 0x7B, Bit -->  0 to 2 */
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__POS	(3)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__LEN	(1)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__MSK	(0x08)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG	\
(BMI160_USER_STEP_CONFIG_1_ADDR)
/**************************************************************/
/**\name	INTERRUPT1 MAPPIONG OF LOW_G LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->0 */
#define BMI160_USER_INTR_MAP_0_INTR1_LOW_G__POS		(0)
#define BMI160_USER_INTR_MAP_0_INTR1_LOW_G__LEN     (1)
#define BMI160_USER_INTR_MAP_0_INTR1_LOW_G__MSK     (0x01)
#define BMI160_USER_INTR_MAP_0_INTR1_LOW_G__REG \
	(BMI160_USER_INTR_MAP_0_ADDR)
/**************************************************************/
/**\name	INTERRUPT2 MAPPIONG OF LOW_G LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->0 */
#define BMI160_USER_INTR_MAP_2_INTR2_LOW_G__POS     (0)
#define BMI160_USER_INTR_MAP_2_INTR2_LOW_G__LEN     (1)
#define BMI160_USER_INTR_MAP_2_INTR2_LOW_G__MSK     (0x01)
#define BMI160_USER_INTR_MAP_2_INTR2_LOW_G__REG \
	(BMI160_USER_INTR_MAP_2_ADDR)
/**************************************************************/
/**\name	LOW_G ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_1 Description - Reg Addr --> (0x51), Bit -->3 */
#define BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__POS		(3)
#define BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__LEN	(1)
#define BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__MSK	(0x08)
#define BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG		\
(BMI160_USER_INTR_ENABLE_1_ADDR)
/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->2 */
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__POS	(2)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__LEN	(1)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__MSK    (0x04)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG    \
(BMI160_USER_INTR_MAP_0_ADDR)
/**\name	LATCH INTERRUPT LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Latch Description - Reg Addr --> 0x54, Bit -->0...3 */
#define BMI160_USER_INTR_LATCH__POS               (0)
#define BMI160_USER_INTR_LATCH__LEN               (4)
#define BMI160_USER_INTR_LATCH__MSK               (0x0F)
#define BMI160_USER_INTR_LATCH__REG               (BMI160_USER_INTR_LATCH_ADDR)
/**************************************************************/
/**\name	INPUT ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Latch Description - Reg Addr --> 0x54, Bit -->4 */
#define BMI160_USER_INTR1_INPUT_ENABLE__POS               (4)
#define BMI160_USER_INTR1_INPUT_ENABLE__LEN               (1)
#define BMI160_USER_INTR1_INPUT_ENABLE__MSK               (0x10)
#define BMI160_USER_INTR1_INPUT_ENABLE__REG               \
	(BMI160_USER_INTR_LATCH_ADDR)

/* Int_Latch Description - Reg Addr --> 0x54, Bit -->5*/
#define BMI160_USER_INTR2_INPUT_ENABLE__POS               (5)
#define BMI160_USER_INTR2_INPUT_ENABLE__LEN               (1)
#define BMI160_USER_INTR2_INPUT_ENABLE__MSK               (0x20)
#define BMI160_USER_INTR2_INPUT_ENABLE__REG              \
	(BMI160_USER_INTR_LATCH_ADDR)

#define	BMI160_LOW_G_INTR_STAT          (0x01)
#define	BMI160_ENABLE_LOW_G             (0x08)
#define BMI160_STEP_DET_STAT_HIGH		(1)
#define	BMI160_STEP_DETECT_INTR_ENABLE  (0x01)
/* Interrupt mapping*/
#define	BMI160_MAP_INTR1		(0)
#define	BMI160_MAP_INTR2		(1)
#define BMI160_USER_INTR_LATCH_ADDR			(0X54)
#define BMI160_USER_INTR_MAP_0_ADDR			(0X55)
#define BMI160_USER_INTR_MAP_1_ADDR			(0X56)
#define BMI160_USER_INTR_MAP_2_ADDR			(0X57)
#define BMI160_GEN_READ_WRITE_DELAY     (1)
#define BMI_INT_LEVEL      0
#define BMI_INT_EDGE        1
#define BMI160_MAX_LATCH_INTR               (15)


/* sensor type */
enum SENSOR_TYPE_ENUM {
	BMI160_STC_TYPE = 0x0,
	INVALID_TYPE = 0xff
};

/* power mode */
enum STC_POWERMODE_ENUM {
	STC_SUSPEND_MODE = 0x0,
	STC_NORMAL_MODE,
	STC_UNDEFINED_POWERMODE = 0xff
};
#endif/* BMI160_STC_H */
