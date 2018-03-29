/* BMA255 motion sensor driver
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */

#ifndef BMA255_H
#define BMA255_H

#include <linux/ioctl.h>

/* 7-bit addr: 0x10 (SDO connected to GND); 0x11 (SDO connected to VDDIO) */
#define BMA255_I2C_ADDR				0x10
/* chip ID */
#define BMA255_FIXED_DEVID			0xFA

 /* BMA255 Register Map  (Please refer to BMA255 Specifications) */
#define BMA255_REG_DEVID			0x00
#define BMA255_REG_OFSX				0x16
#define BMA255_REG_OFSX_HIGH		0x1A
#define BMA255_REG_BW_RATE			0x10
#define BMA255_BW_MASK				0x1f
#define BMA255_BW_200HZ				0x0d
#define BMA255_BW_100HZ				0x0c
#define BMA255_BW_50HZ				0x0b
#define BMA255_BW_25HZ				0x0a
#define BMA255_REG_POWER_CTL		0x11
#define BMA255_REG_DATA_FORMAT		0x0f
#define BMA255_RANGE_MASK			0x0f
#define BMA255_RANGE_2G				0x03
#define BMA255_RANGE_4G				0x05
#define BMA255_RANGE_8G				0x08
#define BMA255_REG_DATAXLOW			0x02
#define BMA255_REG_DATA_RESOLUTION	0x14
#define BMA255_MEASURE_MODE			0x80
#define BMA255_SELF_TEST         0x32
#define BMA255_SELF_TEST_AXIS_X		0x01
#define BMA255_SELF_TEST_AXIS_Y		0x02
#define BMA255_SELF_TEST_AXIS_Z		0x03
#define BMA255_SELF_TEST_POSITIVE	0x00
#define BMA255_SELF_TEST_NEGATIVE	0x04
#define BMA255_INT_REG_1          0x16
#define BMA255_INT_REG_2          0x17

#define BMA255_FIFO_MODE_REG                    0x3E
#define BMA255_FIFO_DATA_OUTPUT_REG             0x3F
#define BMA255_STATUS_FIFO_REG                  0x0E

#define BMA255_SUCCESS					0
#define BMA255_ERR_I2C					-1
#define BMA255_ERR_STATUS				-3
#define BMA255_ERR_SETUP_FAILURE		-4
#define BMA255_ERR_GETGSENSORDATA		-5
#define BMA255_ERR_IDENTIFICATION		-6

#define BMA255_BUFSIZE					256

/* power mode */
#define BMA255_MODE_CTRL_REG			0x11

#define BMA255_MODE_CTRL__POS			5
#define BMA255_MODE_CTRL__LEN			3
#define BMA255_MODE_CTRL__MSK			0xE0
#define BMA255_MODE_CTRL__REG			BMA255_MODE_CTRL_REG

#define BMA255_LOW_POWER_CTRL_REG		0x12

#define BMA255_LOW_POWER_MODE__POS		6
#define BMA255_LOW_POWER_MODE__LEN		1
#define BMA255_LOW_POWER_MODE__MSK		0x40
#define BMA255_LOW_POWER_MODE__REG		BMA255_LOW_POWER_CTRL_REG

/* range */
#define BMA255_RANGE_SEL_REG			0x0F

#define BMA255_RANGE_SEL__POS			0
#define BMA255_RANGE_SEL__LEN			4
#define BMA255_RANGE_SEL__MSK			0x0F
#define BMA255_RANGE_SEL__REG			BMA255_RANGE_SEL_REG

/* bandwidth */
#define BMA255_BW_7_81HZ			0x08
#define BMA255_BW_15_63HZ			0x09
#define BMA255_BW_31_25HZ			0x0A
#define BMA255_BW_62_50HZ			0x0B
#define BMA255_BW_125HZ				0x0C
#define BMA255_BW_250HZ				0x0D
#define BMA255_BW_500HZ				0x0E
#define BMA255_BW_1000HZ			0x0F

#define BMA255_BW_SEL_REG			0x10

#define BMA255_BANDWIDTH__POS			0
#define BMA255_BANDWIDTH__LEN			5
#define BMA255_BANDWIDTH__MSK			0x1F
#define BMA255_BANDWIDTH__REG			BMA255_BW_SEL_REG

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB

#define BMA255_AXES_NUM        3
/*----------------------------------------------------------------------------*/
enum CUST_ACTION {
	BMA255_CUST_ACTION_SET_CUST = 1,
	BMA255_CUST_ACTION_SET_CALI,
	BMA255_CUST_ACTION_RESET_CALI
};
/*----------------------------------------------------------------------------*/
struct BMA255_CUST {
	uint16_t action;
};
/*----------------------------------------------------------------------------*/
struct BMA255_SET_CUST {
	uint16_t action;
	uint16_t part;
	int32_t data[0];
};
/*----------------------------------------------------------------------------*/
struct BMA255_SET_CALI {
	uint16_t action;
	int32_t data[BMA255_AXES_NUM];
};
/*----------------------------------------------------------------------------*/
union BMA255_CUST_DATA {
	uint32_t data[10];
	struct BMA255_CUST cust;
	struct BMA255_SET_CUST setCust;
	struct BMA255_SET_CALI setCali;
	struct BMA255_CUST resetCali;
};
/*----------------------------------------------------------------------------*/
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */

#endif