/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef BMI160_ACC_H
#define BMI160_ACC_H

#include <linux/ioctl.h>

#define COMPATIABLE_NAME "mediatek,bmi160_acc"
#define BMI160_DEV_NAME "bmi160_acc"

#define BMI160_GET_BITSLICE(regvar, bitname)                                   \
	((regvar & bitname##__MSK) >> bitname##__POS)
#define BMI160_SET_BITSLICE(regvar, bitname, val)                              \
	((regvar & ~bitname##__MSK) |                                          \
	 ((val << bitname##__POS) & bitname##__MSK))

#define BMI160_BUS_WRITE_FUNC(dev_addr, reg_addr, reg_data, wr_len)            \
	bus_write(dev_addr, reg_addr, reg_data, wr_len)
#define BMI160_BUS_READ_FUNC(dev_addr, reg_addr, reg_data, r_len)              \
	bus_read(dev_addr, reg_addr, reg_data, r_len)
#define BMI160_WR_FUNC_PTR s8 (*bus_write)(u8, u8, u8 *, u8)
#define BMI160_RD_FUNC_PTR s8 (*bus_read)(u8, u8, u8 *, u8)
#define BMI160_BRD_FUNC_PTR s8 (*burst_read)(u8, u8, u8 *, u32)

#define BMI_INT_LEVEL 0
#define BMI_INT_EDGE 1
/* Interrupt mapping*/
#define BMI160_MAP_INTR1 (0)
#define BMI160_MAP_INTR2 (1)

#define WORK_DELAY_DEFAULT (200)
#define ABSMIN (-32768)
#define ABSMAX (32767)

#define BMI160_GEN_READ_WRITE_DELAY (1)
#define BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY (5)
/* IN OUT CONTROL */
#define BMI160_INTR1_EDGE_CTRL (0)
#define BMI160_INTR2_EDGE_CTRL (1)
#define BMI160_INTR1_LEVEL (0)
#define BMI160_INTR2_LEVEL (1)
#define BMI160_INTR1_OUTPUT_TYPE (0)
#define BMI160_INTR2_OUTPUT_TYPE (1)
#define BMI160_INTR1_OUTPUT_ENABLE (0)
#define BMI160_INTR2_OUTPUT_ENABLE (1)
#define BMI160_INTR1_INPUT_ENABLE (0)
#define BMI160_INTR2_INPUT_ENABLE (1)
#define BMI160_SIG_MOTION_STAT_HIGH (1)
#define BMI160_MAX_UNDER_SIG_MOTION (3)
#define BMI160_MAX_VALUE_SIGNIFICANT_MOTION (1)
#define BMI160_MDELAY_DATA_TYPE u32
#define BMI160_ACC_SUCCESS 0
#define BMI160_ACC_ERR_I2C -1
#define BMI_INT0 0
#define BMI_INT1 1
#define DISABLE 0
#define ENABLE 1
#define BMI_DELAY_MIN (1)
#define FIFO_FRAME_CNT (20)
#define FIFO_DATA_BUFSIZE 1024
#define BMI160_BUFSIZE 256
#define CONVERT_M 1
#define CONVERT_M_DIV 4
#define CONVERT_O 1
/* (C_PI_F32X * AXIS_RESOLUTION_FACTOR / 180) */
#define CONVERT_O_DIV 71
#define CONVERT_G 1
#define CONVERT_G_DIV 938
#define CONVERT_VRV 1
#define CONVERT_VRV_DIV (0x40000000)
#define CONVERT_VLA_DIV 16384
#define CONVERT_VG_DIV 16384
#define RESET_STEP_COUNTER (0xB2)
#define BMI160_RETURN_FUNCTION_TYPE s8
#define BMI160_NULL (0)
#define E_BMI160_NULL_PTR ((s8)-127)
#define E_BMI160_COMM_RES ((s8)-1)
#define E_BMI160_OUT_OF_RANGE ((s8)-2)
#define E_BMI160_BUSY ((s8)-3)
#define SUCCESS ((u8)0)
#define ERROR ((s8)-1)
#define BMI160_INIT_VALUE (0)
#define BMI160_GEN_READ_WRITE_DATA_LENGTH (1)
#define BMI160_USER_PMU_TRIGGER_ADDR (0X6C)

#define BMI160_INTR1_MAP_FIFO_WM (0)
#define BMI160_INTR2_MAP_FIFO_WM (1)

#define BMI160_INTR1_LEVEL (0)
#define BMI160_INTR2_LEVEL (1)

#define BMI160_INTR1_OUTPUT_ENABLE (0)
#define BMI160_INTR2_OUTPUT_ENABLE (1)

#define BMI160_I2C_ADDR 0x68

#define C_BMI160_ZERO_U8X ((u8)0)
#define C_BMI160_ONE_U8X ((u8)1)
#define C_BMI160_TWO_U8X ((u8)2)
#define C_BMI160_THREE_U8X ((u8)3)
#define C_BMI160_FOUR_U8X ((u8)4)
#define C_BMI160_FIVE_U8X ((u8)5)
#define C_BMI160_SIX_U8X ((u8)6)
#define C_BMI160_SEVEN_U8X ((u8)7)
#define C_BMI160_EIGHT_U8X ((u8)8)
#define C_BMI160_NINE_U8X ((u8)9)
#define C_BMI160_ELEVEN_U8X ((u8)11)
#define C_BMI160_TWELVE_U8X ((u8)12)
#define C_BMI160_FOURTEEN_U8X ((u8)14)
#define C_BMI160_FIFTEEN_U8X ((u8)15)
#define C_BMI160_SIXTEEN_U8X ((u8)16)
#define C_BMI160_THIRTYONE_U8X ((u8)31)
#define C_BMI160_THIRTYTWO_U8X ((u8)32)

#define C_MAX_FIR_LENGTH (32)
#define REG_MAX0 0x24
#define REG_MAX1 0x56
#define BMI160_ACC_AXIS_X 0
#define BMI160_ACC_AXIS_Y 1
#define BMI160_ACC_AXIS_Z 2
#define BMI160_ACC_AXES_NUM 3
#define BMI160_ACC_DATA_LEN 6

#define BMI160_ACC_MODE_NORMAL 0
#define BMI160_ACC_MODE_LOWPOWER 1
#define BMI160_ACC_MODE_SUSPEND 2

#define BMI160_USER_CHIP_ID_ADDR 0x00
/* Chip ID Description - Reg Addr --> 0x00, Bit --> 0...7 */
#define BMI160_USER_CHIP_ID__POS 0
#define BMI160_USER_CHIP_ID__MSK 0xFF
#define BMI160_USER_CHIP_ID__LEN 8
#define BMI160_USER_CHIP_ID__REG BMI160_USER_CHIP_ID_ADDR

#define SENSOR_CHIP_ID_BMI (0xD0)
#define SENSOR_CHIP_ID_BMI_C2 (0xD1)
#define SENSOR_CHIP_ID_BMI_C3 (0xD3)

#define BMI160_USER_INTR_STAT_0_ADDR (0X1C)
#define BMI160_USER_INTR_STAT_1_ADDR (0X1D)
#define BMI160_USER_INTR_STAT_2_ADDR (0X1E)
#define BMI160_USER_INTR_STAT_3_ADDR (0X1F)

#define BMI160_LOW_G_INTR_STAT (0x01)
#define BMI160_ENABLE_LOW_G (0x08)
#define BMI160_STEP_DET_STAT_HIGH (1)
#define BMI160_STEP_DETECT_INTR_ENABLE (0x01)

#define BMI160_USER_INTR_MOTION_1_ADDR (0X60)
#define BMI160_USER_INTR_MOTION_3_ADDR (0X62)
#define BMI160_ENABLE_ANY_MOTION_INTR1 (0x04)
#define BMI160_ENABLE_ANY_MOTION_INTR2 (0x04)
#define BMI160_ENABLE_ANY_MOTION_AXIS (0x07)
#define BMI160_SIG_MOTION_INTR_ENABLE (0x01)
#define BMI160_ANY_MOTION_X_ENABLE (0)
/* INT ENABLE 1 */
#define BMI160_ANY_MOTION_X_ENABLE (0)
#define BMI160_ANY_MOTION_Y_ENABLE (1)
#define BMI160_ANY_MOTION_Z_ENABLE (2)
#define BMI160_DOUBLE_TAP_ENABLE (4)
#define BMI160_SINGLE_TAP_ENABLE (5)
#define BMI160_ORIENT_ENABLE (6)
#define BMI160_FLAT_ENABLE (7)
/***************************************************/
/**\name STEP COUNTER CONFIGURATION REGISTERS*/
/******************************************************/
#define BMI160_USER_STEP_CONFIG_0_ADDR (0X7A)
#define BMI160_USER_STEP_CONFIG_1_ADDR (0X7B)
/***************************************************/
/**\name INTERRUPT ENABLE REGISTERS*/
/******************************************************/
#define BMI160_USER_INTR_ENABLE_0_ADDR (0X50)
#define BMI160_USER_INTR_ENABLE_1_ADDR (0X51)
#define BMI160_USER_INTR_ENABLE_2_ADDR (0X52)
#define BMI160_USER_INTR_OUT_CTRL_ADDR (0X53)

/* Int_Status_1 Description - Reg Addr --> 0x1D, Bit --> 6 */
#define BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__POS (6)
#define BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__LEN (1)
#define BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__MSK (0x40)
#define BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__REG (BMI160_USER_INTR_STAT_1_ADDR)

#define BMI160_USER_ACC_CONF_ADDR 0X40
/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 0...3 */
#define BMI160_USER_ACC_CONF_ODR__POS 0
#define BMI160_USER_ACC_CONF_ODR__LEN 4
#define BMI160_USER_ACC_CONF_ODR__MSK 0x0F
#define BMI160_USER_ACC_CONF_ODR__REG BMI160_USER_ACC_CONF_ADDR

#define BMI160_ACCEL_OSR4_AVG1 0x00
#define BMI160_ACCEL_OSR2_AVG2 0x01

/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 4...6 */
#define BMI160_USER_ACC_CONF_ACC_BWP__POS 4
#define BMI160_USER_ACC_CONF_ACC_BWP__LEN 3
#define BMI160_USER_ACC_CONF_ACC_BWP__MSK 0x70
#define BMI160_USER_ACC_CONF_ACC_BWP__REG BMI160_USER_ACC_CONF_ADDR

/* Acc_Conf Description - Reg Addr --> 0x40, Bit --> 7 */
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__POS 7
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__LEN 1
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__MSK 0x80
#define BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING__REG BMI160_USER_ACC_CONF_ADDR

#define BMI160_USER_ACC_RANGE_ADDR 0X41
/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACC_RANGE__POS 0
#define BMI160_USER_ACC_RANGE__LEN 4
#define BMI160_USER_ACC_RANGE__MSK 0x0F
#define BMI160_USER_ACC_RANGE__REG BMI160_USER_ACC_RANGE_ADDR

#define BMI160_USER_FIFO_CONFIG_0_ADDR (0X46)
#define BMI160_USER_FIFO_CONFIG_1_ADDR (0X47)

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 1 */
#define BMI160_USER_FIFO_TIME_ENABLE__POS (1)
#define BMI160_USER_FIFO_TIME_ENABLE__LEN (1)
#define BMI160_USER_FIFO_TIME_ENABLE__MSK (0x02)
#define BMI160_USER_FIFO_TIME_ENABLE__REG (BMI160_USER_FIFO_CONFIG_1_ADDR)

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 4 */
#define BMI160_USER_FIFO_HEADER_EN__POS 4
#define BMI160_USER_FIFO_HEADER_EN__LEN 1
#define BMI160_USER_FIFO_HEADER_EN__MSK 0x10
#define BMI160_USER_FIFO_HEADER_EN__REG BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 5 */
#define BMI160_USER_FIFO_MAG_EN__POS 5
#define BMI160_USER_FIFO_MAG_EN__LEN 1
#define BMI160_USER_FIFO_MAG_EN__MSK 0x20
#define BMI160_USER_FIFO_MAG_EN__REG BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 6 */
#define BMI160_USER_FIFO_ACC_EN__POS 6
#define BMI160_USER_FIFO_ACC_EN__LEN 1
#define BMI160_USER_FIFO_ACC_EN__MSK 0x40
#define BMI160_USER_FIFO_ACC_EN__REG BMI160_USER_FIFO_CONFIG_1_ADDR

/* Fifo_Config_1 Description - Reg Addr --> 0x47, Bit --> 7 */
#define BMI160_USER_FIFO_GYRO_EN__POS 7
#define BMI160_USER_FIFO_GYRO_EN__LEN 1
#define BMI160_USER_FIFO_GYRO_EN__MSK 0x80
#define BMI160_USER_FIFO_GYRO_EN__REG BMI160_USER_FIFO_CONFIG_1_ADDR
#define BMI160_USER_FIFO_LENGTH_0_ADDR 0X22
#define BMI160_USER_FIFO_LENGTH_1_ADDR 0X23
#define BMI160_USER_FIFO_DATA_ADDR 0X24
#define BMI160_USER_ACCEL_CONFIG_ADDR (0X40)

#define BMI160_USER_INT_EN_0_ADDR 0X50
#define BMI160_USER_INT_EN_1_ADDR 0X51
#define BMI160_USER_INT_EN_2_ADDR 0X52

#define BMI160_USER_INTR_LATCH_ADDR (0X54)
#define BMI160_USER_INTR_MAP_0_ADDR (0X55)
#define BMI160_USER_INTR_MAP_1_ADDR (0X56)
#define BMI160_USER_INTR_MAP_2_ADDR (0X57)

#define CMD_PMU_ACC_SUSPEND 0x10
#define CMD_PMU_ACC_NORMAL 0x11
#define CMD_PMU_ACC_LP1 0x12
#define CMD_PMU_ACC_LP2 0x13

#define BMI160_CMD_COMMANDS_ADDR 0X7E
/* Command description address - Reg Addr --> 0x7E, Bit -->  0....7 */
#define BMI160_CMD_COMMANDS__POS 0
#define BMI160_CMD_COMMANDS__LEN 8
#define BMI160_CMD_COMMANDS__MSK 0xFF
#define BMI160_CMD_COMMANDS__REG BMI160_CMD_COMMANDS_ADDR

#define BMI160_SHIFT_1_POSITION 1
#define BMI160_SHIFT_2_POSITION 2
#define BMI160_SHIFT_3_POSITION 3
#define BMI160_SHIFT_4_POSITION 4
#define BMI160_SHIFT_5_POSITION 5
#define BMI160_SHIFT_6_POSITION 6
#define BMI160_SHIFT_7_POSITION 7
#define BMI160_SHIFT_8_POSITION 8
#define BMI160_SHIFT_12_POSITION 12
#define BMI160_SHIFT_16_POSITION 16

#define BMI160_USER_DATA_8_ADDR (0X0C)
#define BMI160_USER_DATA_9_ADDR (0X0D)
#define BMI160_USER_DATA_10_ADDR (0X0E)
#define BMI160_USER_DATA_11_ADDR (0X0F)
#define BMI160_USER_DATA_12_ADDR (0X10)
#define BMI160_USER_DATA_13_ADDR (0X11)
#define BMI160_USER_DATA_14_ADDR (0X12)
#define BMI160_USER_DATA_15_ADDR (0X13)
/* ACC_X (LSB) Description - Reg Addr --> 0x12, Bit --> 0...7 */
#define BMI160_USER_DATA_14_ACC_X_LSB__POS 0
#define BMI160_USER_DATA_14_ACC_X_LSB__LEN 8
#define BMI160_USER_DATA_14_ACC_X_LSB__MSK 0xFF
#define BMI160_USER_DATA_14_ACC_X_LSB__REG BMI160_USER_DATA_14_ADDR
/* GYR_X (LSB) Description - Reg Addr --> (0x0C), Bit --> 0...7 */
#define BMI160_USER_DATA_8_GYRO_X_LSB__POS (0)
#define BMI160_USER_DATA_8_GYRO_X_LSB__LEN (8)
#define BMI160_USER_DATA_8_GYRO_X_LSB__MSK (0xFF)
#define BMI160_USER_DATA_8_GYRO_X_LSB__REG (BMI160_USER_DATA_8_ADDR)

#define BMI160_USER_PMU_STATUS_ADDR 0X03
/* PMU_Status Description of ACCEL - Reg Addr --> 0x03, Bit --> 5...4 */
#define BMI160_USER_ACC_PMU_STATUS__POS 4
#define BMI160_USER_ACC_PMU_STATUS__LEN 2
#define BMI160_USER_ACC_PMU_STATUS__MSK 0x30
#define BMI160_USER_ACC_PMU_STATUS__REG BMI160_USER_PMU_STATUS_ADDR

#define BMI160_USER_ACC_RANGE_ADDR 0X41
/* Acc_Range Description - Reg Addr --> 0x41, Bit --> 0...3 */
#define BMI160_USER_ACC_RANGE__POS 0
#define BMI160_USER_ACC_RANGE__LEN 4
#define BMI160_USER_ACC_RANGE__MSK 0x0F
#define BMI160_USER_ACC_RANGE__REG BMI160_USER_ACC_RANGE_ADDR

/* Fifo_Config_0 Description - Reg Addr --> 0x46, Bit --> 0...7 */
#define BMI160_USER_FIFO_WM__POS (0)
#define BMI160_USER_FIFO_WM__LEN (8)
#define BMI160_USER_FIFO_WM__MSK (0xFF)
#define BMI160_USER_FIFO_WM__REG (BMI160_USER_FIFO_CONFIG_0_ADDR)

/* Int_En_1 Description - Reg Addr --> (0x51), Bit -->6 */
#define BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__POS (6)
#define BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__MSK (0x40)
#define BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__REG                          \
	(BMI160_USER_INTR_ENABLE_1_ADDR)

/* Fifo_Length0 Description - Reg Addr --> 0x22, Bit --> 0...7 */
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__POS 0
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__LEN 8
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__MSK 0xFF
#define BMI160_USER_FIFO_BYTE_COUNTER_LSB__REG BMI160_USER_FIFO_LENGTH_0_ADDR

/*Fifo_Length1 Description - Reg Addr --> 0x23, Bit --> 0...2 */
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__POS 0
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__LEN 3
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__MSK 0x07
#define BMI160_USER_FIFO_BYTE_COUNTER_MSB__REG BMI160_USER_FIFO_LENGTH_1_ADDR

/* Fifo_Data Description - Reg Addr --> 0x24, Bit --> 0...7 */
#define BMI160_USER_FIFO_DATA__POS 0
#define BMI160_USER_FIFO_DATA__LEN 8
#define BMI160_USER_FIFO_DATA__MSK 0xFF
#define BMI160_USER_FIFO_DATA__REG BMI160_USER_FIFO_DATA_ADDR

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->1 */
#define BMI160_USER_INTR1_LEVEL__POS (1)
#define BMI160_USER_INTR1_LEVEL__LEN (1)
#define BMI160_USER_INTR1_LEVEL__MSK (0x02)
#define BMI160_USER_INTR1_LEVEL__REG (BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->3 */
#define BMI160_USER_INTR1_OUTPUT_ENABLE__POS (3)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__LEN (1)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__MSK (0x08)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__REG (BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->5 */
#define BMI160_USER_INTR2_LEVEL__POS (5)
#define BMI160_USER_INTR2_LEVEL__LEN (1)
#define BMI160_USER_INTR2_LEVEL__MSK (0x20)
#define BMI160_USER_INTR2_LEVEL__REG (BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->7 */
#define BMI160_USER_INTR2_OUTPUT_EN__POS (7)
#define BMI160_USER_INTR2_OUTPUT_EN__LEN (1)
#define BMI160_USER_INTR2_OUTPUT_EN__MSK (0x80)
#define BMI160_USER_INTR2_OUTPUT_EN__REG (BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->6 */
#define BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__POS (6)
#define BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__LEN (1)
#define BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__MSK (0x40)
#define BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__REG (BMI160_USER_INTR_MAP_1_ADDR)

/* Int_Map_1 Description - Reg Addr --> 0x56, Bit -->2 */
#define BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__POS (2)
#define BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__LEN (1)
#define BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__MSK (0x04)
#define BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__REG (BMI160_USER_INTR_MAP_1_ADDR)

#define BMI160_USER_ERROR_ADDR (0X02)
/* Error Description - Reg Addr --> (0x02), Bit --> 6 */
#define BMI160_USER_DROP_CMD_ERR__POS (6)
#define BMI160_USER_DROP_CMD_ERR__LEN (1)
#define BMI160_USER_DROP_CMD_ERR__MSK (0x40)
#define BMI160_USER_DROP_CMD_ERR__REG (BMI160_USER_ERROR_ADDR)
/**************************************************************/
/**\name	EDGE CONTROL ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->0 */
#define BMI160_USER_INTR1_EDGE_CTRL__POS (0)
#define BMI160_USER_INTR1_EDGE_CTRL__LEN (1)
#define BMI160_USER_INTR1_EDGE_CTRL__MSK (0x01)
#define BMI160_USER_INTR1_EDGE_CTRL__REG (BMI160_USER_INTR_OUT_CTRL_ADDR)
/**************************************************************/
/**\name	EDGE CONTROL ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->4 */
#define BMI160_USER_INTR2_EDGE_CTRL__POS (4)
#define BMI160_USER_INTR2_EDGE_CTRL__LEN (1)
#define BMI160_USER_INTR2_EDGE_CTRL__MSK (0x10)
#define BMI160_USER_INTR2_EDGE_CTRL__REG (BMI160_USER_INTR_OUT_CTRL_ADDR)
/**************************************************************/
/**\name	STEP DETECTOR ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_2 Description - Reg Addr --> (0x52), Bit -->3 */
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__POS (3)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__MSK (0x08)
#define BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__REG                    \
	(BMI160_USER_INTR_ENABLE_2_ADDR)
/**************************************************************/
/**\name	STEP COUNTER ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* STEP_CONFIG_1  Description - Reg Addr --> 0x7B, Bit -->  0 to 2 */
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__POS (3)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__LEN (1)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__MSK (0x08)
#define BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG                       \
	(BMI160_USER_STEP_CONFIG_1_ADDR)
/**************************************************************/
/**\name	INTERRUPT1 MAPPIONG OF LOW_G LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->0 */
#define BMI160_USER_INTR_MAP_0_INTR1_LOW_G__POS (0)
#define BMI160_USER_INTR_MAP_0_INTR1_LOW_G__LEN (1)
#define BMI160_USER_INTR_MAP_0_INTR1_LOW_G__MSK (0x01)
#define BMI160_USER_INTR_MAP_0_INTR1_LOW_G__REG (BMI160_USER_INTR_MAP_0_ADDR)
/**************************************************************/
/**\name	INTERRUPT2 MAPPIONG OF LOW_G LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->0 */
#define BMI160_USER_INTR_MAP_2_INTR2_LOW_G__POS (0)
#define BMI160_USER_INTR_MAP_2_INTR2_LOW_G__LEN (1)
#define BMI160_USER_INTR_MAP_2_INTR2_LOW_G__MSK (0x01)
#define BMI160_USER_INTR_MAP_2_INTR2_LOW_G__REG (BMI160_USER_INTR_MAP_2_ADDR)
/**************************************************************/
/**\name	LOW_G ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_1 Description - Reg Addr --> (0x51), Bit -->3 */
#define BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__POS (3)
#define BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__MSK (0x08)
#define BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG                            \
	(BMI160_USER_INTR_ENABLE_1_ADDR)
/**************************************************************/
/**\name	SIGNIFICANT MOTION SELECT LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Motion_3 Description - Reg Addr --> (0x62), Bit --> 1 */
#define BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__POS (1)
#define BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__LEN (1)
#define BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__MSK (0x02)
#define BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__REG                      \
	(BMI160_USER_INTR_MOTION_3_ADDR)
/**************************************************************/
/**\name	ANY MOTION THRESHOLD LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Motion_1 Description - Reg Addr --> (0x60), Bit --> 0...7 */
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__POS (0)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__LEN (8)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__MSK (0xFF)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__REG                   \
	(BMI160_USER_INTR_MOTION_1_ADDR)
/**************************************************************/
/**\name	ANY MOTION THRESHOLD LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Motion_1 Description - Reg Addr --> (0x60), Bit --> 0...7 */
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__POS (0)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__LEN (8)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__MSK (0xFF)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__REG                   \
	(BMI160_USER_INTR_MOTION_1_ADDR)
/* Int_Motion_3 Description - Reg Addr --> (0x62), Bit --> 3..2 */
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__POS (2)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__LEN (2)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__MSK (0x0C)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__REG                          \
	(BMI160_USER_INTR_MOTION_3_ADDR)
/* Int_Motion_3 Description - Reg Addr --> (0x62), Bit --> 5..4 */
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__POS (4)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__LEN (2)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__MSK (0x30)
#define BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__REG                         \
	(BMI160_USER_INTR_MOTION_3_ADDR)
/**************************************************************/
/**\name	INTERRUPT MAPPIONG OF ANY MOTION_G LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->2 */
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__POS (2)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__LEN (1)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__MSK (0x04)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG                           \
	(BMI160_USER_INTR_MAP_0_ADDR)
/**************************************************************/
/**\name	INTERRUPT2 MAPPIONG OF ANY MOTION LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->2 */
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__POS (2)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__LEN (1)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__MSK (0x04)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__REG                           \
	(BMI160_USER_INTR_MAP_2_ADDR)
/**************************************************************/
/**\name	ANY MOTION XYZ AXIS ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->0 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__POS (0)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__MSK (0x01)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__REG                     \
	(BMI160_USER_INTR_ENABLE_0_ADDR)
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->1 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__POS (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__MSK (0x02)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__REG                     \
	(BMI160_USER_INTR_ENABLE_0_ADDR)
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->2 */
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__POS (2)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__MSK (0x04)
#define BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__REG                     \
	(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	DOUBLE TAP ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->4 */
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__POS (4)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__MSK (0x10)
#define BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__REG                       \
	(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	SINGLE TAP ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->5 */
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__POS (5)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__MSK (0x20)
#define BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__REG                       \
	(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	ORIENT ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->6 */
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__POS (6)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__MSK (0x40)
#define BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__REG                           \
	(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	FLAT ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_En_0 Description - Reg Addr --> 0x50, Bit -->7 */
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__POS (7)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__LEN (1)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__MSK (0x80)
#define BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__REG                             \
	(BMI160_USER_INTR_ENABLE_0_ADDR)
/**************************************************************/
/**\name	SIGNIFICANT INTERRUPT STATUS LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 1 */
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__POS (1)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__LEN (1)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__MSK (0x02)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__REG                          \
	(BMI160_USER_INTR_STAT_0_ADDR)
/**************************************************************/
/**\name	INTERRUPT STATUS LENGTH, POSITION AND MASK    */
/**************************************************************/
/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 0 */
#define BMI160_USER_INTR_STAT_0_STEP_INTR__POS (0)
#define BMI160_USER_INTR_STAT_0_STEP_INTR__LEN (1)
#define BMI160_USER_INTR_STAT_0_STEP_INTR__MSK (0x01)
#define BMI160_USER_INTR_STAT_0_STEP_INTR__REG (BMI160_USER_INTR_STAT_0_ADDR)

#define BMI160_ACCEL_RANGE_2G 0x03
#define BMI160_ACCEL_RANGE_4G 0x05
#define BMI160_ACCEL_RANGE_8G 0x08
#define BMI160_ACCEL_RANGE_16G 0x0c

/* BMI160 Accel ODR */
#define BMI160_ACCEL_ODR_RESERVED 0x00
#define BMI160_ACCEL_ODR_0_78HZ 0x01
#define BMI160_ACCEL_ODR_1_56HZ 0x02
#define BMI160_ACCEL_ODR_3_12HZ 0x03
#define BMI160_ACCEL_ODR_6_25HZ 0x04
#define BMI160_ACCEL_ODR_12_5HZ 0x05
#define BMI160_ACCEL_ODR_25HZ 0x06
#define BMI160_ACCEL_ODR_50HZ 0x07
#define BMI160_ACCEL_ODR_100HZ 0x08
#define BMI160_ACCEL_ODR_200HZ 0x09
#define BMI160_ACCEL_ODR_400HZ 0x0A
#define BMI160_ACCEL_ODR_800HZ 0x0B
#define BMI160_ACCEL_ODR_1600HZ 0x0C
#define BMI160_ACCEL_ODR_RESERVED0 0x0D
#define BMI160_ACCEL_ODR_RESERVED1 0x0E
#define BMI160_ACCEL_ODR_RESERVED2 0x0F

#define FIFO_HEAD_SENSOR_TIME 0x44
#define FIFO_HEAD_SKIP_FRAME 0x40
#define FIFO_HEAD_OVER_READ_LSB 0x80
#define FIFO_HEAD_OVER_READ_MSB 0x00

#define CMD_CLR_FIFO_DATA 0xB0

/*! FIFO Head definition*/
#define FIFO_HEAD_A 0x84
#define FIFO_HEAD_G 0x88
#define FIFO_HEAD_M 0x90

#define FIFO_HEAD_G_A (FIFO_HEAD_G | FIFO_HEAD_A)
#define FIFO_HEAD_M_A (FIFO_HEAD_M | FIFO_HEAD_A)
#define FIFO_HEAD_M_G (FIFO_HEAD_M | FIFO_HEAD_G)
#define FIFO_HEAD_M_G_A (FIFO_HEAD_M | FIFO_HEAD_G | FIFO_HEAD_A)

/*! FIFO head mode Frame bytes number definition */
#define A_BYTES_FRM 6
#define G_BYTES_FRM 6
#define M_BYTES_FRM 8
#define GA_BYTES_FRM 12
#define MG_BYTES_FRM 14
#define MA_BYTES_FRM 14
#define MGA_BYTES_FRM 20

#define ACC_FIFO_HEAD "acc"
#define GYRO_FIFO_HEAD "gyro"
#define MAG_FIFO_HEAD "mag"
#define LMADA (1)

struct bmi160_t {
	u8 chip_id;
	/**< chip id of BMI160 */
	u8 dev_addr;
	/**< device address of BMI160 */
	s8 mag_manual_enable;
	/**< used for check the mag manual/auto mode status */
	BMI160_WR_FUNC_PTR;
	/**< bus write function pointer */
	BMI160_RD_FUNC_PTR;
	/**< bus read function pointer */
	BMI160_BRD_FUNC_PTR;
	/**< burst write function pointer */
	void (*delay_msec)(u32 BMI160_MDELAY_DATA_TYPE);
	/**< delay function pointer */
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

/*! bmi sensor time resolution definition*/
enum BMI_SENSOR_TIME_RS_TYPE {
	TS_0_78_HZ = 1, /*0.78HZ */
	TS_1_56_HZ,     /*1.56HZ */
	TS_3_125_HZ,    /*3.125HZ */
	TS_6_25_HZ,     /*6.25HZ */
	TS_12_5_HZ,     /*12.5HZ */
	TS_25_HZ,       /*25HZ, odr=6 */
	TS_50_HZ,       /*50HZ */
	TS_100_HZ,      /*100HZ */
	TS_200_HZ,      /*200HZ */
	TS_400_HZ,      /*400HZ */
	TS_800_HZ,      /*800HZ */
	TS_1600_HZ,     /*1600HZ */
	TS_MAX_HZ
};

/*! bmi sensor time depend on ODR*/
struct bmi_sensor_time_odr_tbl {
	u32 ts_duration_lsb;
	u32 ts_duration_us;
	u32 ts_delat; /*sub current delat fifo_time */
};

/*! bmi sensor support type*/
enum BMI_SENSOR_TYPE {
	BMI_ACC_SENSOR,
	BMI_GYRO_SENSOR,
	BMI_MAG_SENSOR,
	BMI_SENSOR_TYPE_MAX
};

enum STEP_NOTIFY_TYPE {
	TYPE_STEP_NON = 0,
	TYPE_STEP_DETECTOR = 1,
	TYPE_SIGNIFICANT = 2
};

int bmi160_acc_get_mode(struct i2c_client *client, unsigned char *mode);
int bmi160_acc_get_drop_cmd_err(struct i2c_client *client,
				unsigned char *drop_cmd_err);
#ifdef CUSTOM_KERNEL_SIGNIFICANT_MOTION_SENSOR
extern int bmi160_step_notify(enum STEP_NOTIFY_TYPE);
#endif
#endif
