/* linux/drivers/hwmon/adxl345.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * BMA150 driver for MT6516
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA  BMA150
 */
#ifndef BMA150_H
#define BMA150_H
	 
#include <linux/ioctl.h>
	 
	#define BMA222_I2C_SLAVE_WRITE_ADDR		0x10
	 
	 /* BMA222 Register Map  (Please refer to BMA150 Specifications) */
	#define BMA222_REG_DEVID				0x00
	#define BMA222_FIXED_DEVID			0x03
	#define BMA222_REG_OFSX				0x16
	#define BMA222_REG_OFSX_HIGH			0x1A
	#define BMA222_REG_BW_RATE			0x10
	#define BMA222_BW_MASK				0x1f
	#define BMA222_BW_200HZ				0x0d
	#define BMA222_BW_100HZ				0x0c
	#define BMA222_BW_50HZ				0x0b
	#define BMA222_BW_25HZ				0x0a
	#define BMA222_REG_POWER_CTL		0x11		
	#define BMA222_REG_DATA_FORMAT		0x0f
	#define BMA222_RANGE_MASK			0x0f
	#define BMA222_RANGE_2G				0x03
	#define BMA222_RANGE_4G				0x05
	#define BMA222_RANGE_8G				0x08
	#define BMA222_REG_DATAXLOW			0x03
	#define BMA222_REG_DATA_RESOLUTION	0x14
	#define BMA222_MEASURE_MODE			0x80	
	#define BMA222_SELF_TEST           			0x32
	#define BMA222_SELF_TEST_AXIS_X		0x01
	#define BMA222_SELF_TEST_AXIS_Y		0x02
	#define BMA222_SELF_TEST_AXIS_Z		0x03
	#define BMA222_SELF_TEST_POSITIVE	0x00
	#define BMA222_SELF_TEST_NEGATIVE	0x04
	#define BMA222_INT_REG_1           			0x16
	#define BMA222_INT_REG_2          		 	0x17

	
#define BMA222_SUCCESS						0
#define BMA222_ERR_I2C						-1
#define BMA222_ERR_STATUS					-3
#define BMA222_ERR_SETUP_FAILURE			-4
#define BMA222_ERR_GETGSENSORDATA			-5
#define BMA222_ERR_IDENTIFICATION			-6
	 
	 
	 
#define BMA222_BUFSIZE				256

#define BMA222_AXES_NUM        3

/*----------------------------------------------------------------------------*/
typedef enum{
    BMA222_CUST_ACTION_SET_CUST = 1,
    BMA222_CUST_ACTION_SET_CALI,
    BMA222_CUST_ACTION_RESET_CALI
}CUST_ACTION;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
}BMA222_CUST;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    part;
    int32_t     data[0];
}BMA222_SET_CUST;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    int32_t     data[BMA222_AXES_NUM];
}BMA222_SET_CALI;
/*----------------------------------------------------------------------------*/
typedef BMA222_CUST BMA222_RESET_CALI;
/*----------------------------------------------------------------------------*/
typedef union
{
    uint32_t                data[10];
    BMA222_CUST         cust;
    BMA222_SET_CUST     setCust;
    BMA222_SET_CALI     setCali;
    BMA222_RESET_CALI   resetCali;
}BMA222_CUST_DATA;
/*----------------------------------------------------------------------------*/
	 
#endif

