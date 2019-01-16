/* BMA250 motion sensor driver
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */

#ifndef BMA250_H
#define BMA250_H
	 
#include <linux/ioctl.h>
	 
	#define BMA250_I2C_SLAVE_WRITE_ADDR		0x30
	#define BMA250_FIXED_DEVID			0x03
	
	 /* BMA250 Register Map  (Please refer to BMA250 Specifications) */
	#define BMA250_REG_DEVID				0x00
	#define BMA250_REG_OFSX				0x16
	#define BMA250_REG_OFSX_HIGH			0x1A
	#define BMA250_REG_BW_RATE			0x10
	#define BMA250_BW_MASK				0x1f
	#define BMA250_BW_200HZ				0x0d
	#define BMA250_BW_100HZ				0x0c
	#define BMA250_BW_50HZ				0x0b
	#define BMA250_BW_25HZ				0x0a
	#define BMA250_REG_POWER_CTL		0x11		
	#define BMA250_REG_DATA_FORMAT		0x0f
	#define BMA250_RANGE_MASK			0x0f
	#define BMA250_RANGE_2G				0x03
	#define BMA250_RANGE_4G				0x05
	#define BMA250_RANGE_8G				0x08
	#define BMA250_REG_DATAXLOW			0x02
	#define BMA250_REG_DATA_RESOLUTION	0x14
	#define BMA250_MEASURE_MODE			0x80	
	#define BMA250_SELF_TEST           			0x32
	#define BMA250_SELF_TEST_AXIS_X		0x01
	#define BMA250_SELF_TEST_AXIS_Y		0x02
	#define BMA250_SELF_TEST_AXIS_Z		0x03
	#define BMA250_SELF_TEST_POSITIVE	0x00
	#define BMA250_SELF_TEST_NEGATIVE	0x04
	#define BMA250_INT_REG_1           			0x16
	#define BMA250_INT_REG_2          		 	0x17

	
#define BMA250_SUCCESS						0
#define BMA250_ERR_I2C						-1
#define BMA250_ERR_STATUS					-3
#define BMA250_ERR_SETUP_FAILURE			-4
#define BMA250_ERR_GETGSENSORDATA			-5
#define BMA250_ERR_IDENTIFICATION			-6
	 
	 
	 
#define BMA250_BUFSIZE				256
	 
#endif

