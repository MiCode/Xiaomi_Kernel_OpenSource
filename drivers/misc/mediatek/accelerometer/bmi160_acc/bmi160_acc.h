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
#define CMD_RESET_INT_ENGINE 0xB1
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
#define BMI_INT_LEVEL      	0
#define BMI_INT_EDGE        1
/* Interrupt mapping*/
#define	BMI160_MAP_INTR1	(0)
#define	BMI160_MAP_INTR2	(1)

#define WORK_DELAY_DEFAULT	(200)
#define ABSMIN				(-32768)
#define ABSMAX				(32767)

#define BMI160_GEN_READ_WRITE_DELAY     (1)
#define BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY    (5)
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
#define  BMI160_MAX_UNDER_SIG_MOTION    (3)
#define	BMI160_MAX_VALUE_SIGNIFICANT_MOTION     (1)
/* ----------------- bmi160_acc macros --------------------*/
#define BMI160_I2C_ADDR				0x68

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
#define BMI_INT0       		0
#define BMI_INT1      		1
#define DISABLE      		0
#define ENABLE       		1

/***************************************************/
/**\name INTERRUPT ENABLE REGISTERS*/
/******************************************************/
#define BMI160_USER_INTR_ENABLE_0_ADDR	(0X50)
#define BMI160_USER_INTR_ENABLE_1_ADDR	(0X51)
#define BMI160_USER_INTR_ENABLE_2_ADDR	(0X52)
#define BMI160_USER_INTR_OUT_CTRL_ADDR	(0X53)
#define BMI160_USER_INTR_LATCH_ADDR		(0X54)
#define BMI160_USER_INTR_MAP_0_ADDR		(0X55)
#define BMI160_USER_INTR_MAP_1_ADDR		(0X56)
#define BMI160_USER_INTR_MAP_2_ADDR		(0X57)
#define BMI160_INTR1_MAP_FIFO_WM		(0)
#define BMI160_INTR2_MAP_FIFO_WM		(1)

#define BMI160_INTR1_LEVEL				(0)
#define BMI160_INTR2_LEVEL				(1)

#define BMI160_INTR1_OUTPUT_ENABLE		(0)
#define BMI160_INTR2_OUTPUT_ENABLE		(1)

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

/**************************************************************/
/**\name	INTERRUPT2 MAPPIONG OF ANY MOTION LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_2 Description - Reg Addr --> 0x57, Bit -->2 */
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__POS     (2)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__LEN     (1)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__MSK     (0x04)
#define BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__REG     \
		(BMI160_USER_INTR_MAP_2_ADDR)
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
/**************************************************************/
/**\name	INTERRUPT MAPPIONG OF ANY MOTION_G LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->2 */
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__POS	(2)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__LEN	(1)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__MSK    (0x04)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG    \
		(BMI160_USER_INTR_MAP_0_ADDR)
/**************************************************************/
/**\name	INTERRUPT MAPPIONG OF ANY MOTION_G LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Map_0 Description - Reg Addr --> 0x55, Bit -->2 */
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__POS	(2)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__LEN	(1)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__MSK    (0x04)
#define BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG    \
		(BMI160_USER_INTR_MAP_0_ADDR)
/**************************************************************/
/**\name	ANY MOTION THRESHOLD LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Motion_1 Description - Reg Addr --> (0x60), Bit --> 0...7 */
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__POS     (0)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__LEN     (8)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__MSK     (0xFF)
#define BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__REG     \
		(BMI160_USER_INTR_MOTION_1_ADDR)

/**************************************************************/
/**\name	EDGE CONTROL ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->0 */
#define BMI160_USER_INTR1_EDGE_CTRL__POS		(0)
#define BMI160_USER_INTR1_EDGE_CTRL__LEN 		(1)
#define BMI160_USER_INTR1_EDGE_CTRL__MSK        (0x01)
#define BMI160_USER_INTR1_EDGE_CTRL__REG		\
(BMI160_USER_INTR_OUT_CTRL_ADDR)
/**************************************************************/
/**\name	EDGE CONTROL ENABLE LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->0 */
#define BMI160_USER_INTR1_EDGE_CTRL__POS		(0)
#define BMI160_USER_INTR1_EDGE_CTRL__LEN 		(1)
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

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->3 */
#define BMI160_USER_INTR1_OUTPUT_ENABLE__POS	(3)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__LEN    (1)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__MSK    (0x08)
#define BMI160_USER_INTR1_OUTPUT_ENABLE__REG	\
(BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->5 */
#define BMI160_USER_INTR2_LEVEL__POS		(5)
#define BMI160_USER_INTR2_LEVEL__LEN		(1)
#define BMI160_USER_INTR2_LEVEL__MSK 		(0x20)
#define BMI160_USER_INTR2_LEVEL__REG         \
(BMI160_USER_INTR_OUT_CTRL_ADDR)

/* Int_Out_Ctrl Description - Reg Addr --> 0x53, Bit -->7 */
#define BMI160_USER_INTR2_OUTPUT_EN__POS       (7)
#define BMI160_USER_INTR2_OUTPUT_EN__LEN       (1)
#define BMI160_USER_INTR2_OUTPUT_EN__MSK       (0x80)
#define BMI160_USER_INTR2_OUTPUT_EN__REG		\
(BMI160_USER_INTR_OUT_CTRL_ADDR)
/**************************************************************/
/**\name	SIGNIFICANT INTERRUPT STATUS LENGTH, POSITION AND MASK*/
/**************************************************************/
/* Int_Status_0 Description - Reg Addr --> 0x1C, Bit --> 1 */
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__POS	(1)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__LEN	(1)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__MSK	(0x02)
#define BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__REG   \
		(BMI160_USER_INTR_STAT_0_ADDR)
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

struct bmi160_t {
u8 chip_id;/**< chip id of BMI160 */
u8 dev_addr;/**< device address of BMI160 */
s8 mag_manual_enable;/**< used for check the mag manual/auto mode status */
BMI160_WR_FUNC_PTR;/**< bus write function pointer */
BMI160_RD_FUNC_PTR;/**< bus read function pointer */
BMI160_BRD_FUNC_PTR;/**< burst write function pointer */
void (*delay_msec)(BMI160_MDELAY_DATA_TYPE);/**< delay function pointer */
};

typedef enum {
	TYPE_STEP_NON = 0,
	TYPE_STEP_DETECTOR = 1,
	TYPE_SIGNIFICANT = 2
} STEP_NOTIFY_TYPE;

#endif
