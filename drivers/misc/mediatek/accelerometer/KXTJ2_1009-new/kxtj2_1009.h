/* linux/drivers/hwmon/adxl345.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * KXTJ2_1009 driver for MT6575
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA  KXTIK
 */
#ifndef KXTJ2_1009_H
#define KXTJ2_1009_H
	 
#include <linux/ioctl.h>
	 
#define KXTJ2_1009_I2C_SLAVE_ADDR		0x1C
	 
 /* KXTJ2_1009 Register Map  (Please refer to KXTJ2_1009 Specifications) */
#define KXTJ2_1009_REG_DEVID			0x0F
#define	KXTJ2_1009_REG_BW_RATE			0x21
#define KXTJ2_1009_REG_POWER_CTL		0x1B
#define KXTJ2_1009_REG_CTL_REG3		0x1D
#define KXTJ2_1009_DCST_RESP			0x0C
#define KXTJ2_1009_REG_DATA_FORMAT		0x1B
#define KXTJ2_1009_REG_DATA_RESOLUTION		0x1B
#define KXTJ2_1009_RANGE_DATA_RESOLUTION_MASK	0x40
#define KXTJ2_1009_REG_DATAX0			0x06	 
#define KXTJ2_1009_FIXED_DEVID			0x09	 
#define KXTJ2_1009_BW_200HZ				0x05
#define KXTJ2_1009_BW_100HZ				0x04
#define KXTJ2_1009_BW_50HZ				0x03	 
#define KXTJ2_1009_MEASURE_MODE		0x80		 
#define KXTJ2_1009_RANGE_MASK		0x18
#define KXTJ2_1009_RANGE_2G			0x00
#define KXTJ2_1009_RANGE_4G			0x08
#define KXTJ2_1009_RANGE_8G			0x10
#define KXTJ2_1009_REG_INT_ENABLE	0x1E

#define KXTJ2_1009_SELF_TEST           0x10
	 	 
	 
#define KXTJ2_1009_SUCCESS						0
#define KXTJ2_1009_ERR_I2C						-1
#define KXTJ2_1009_ERR_STATUS					-3
#define KXTJ2_1009_ERR_SETUP_FAILURE				-4
#define KXTJ2_1009_ERR_GETGSENSORDATA			-5
#define KXTJ2_1009_ERR_IDENTIFICATION			-6
	 
	 
	 
#define KXTJ2_1009_BUFSIZE				256
	 
#define KXTJ2_1009_AXES_NUM        3

/*----------------------------------------------------------------------------*/
typedef enum{
    KXTJ2_1009_CUST_ACTION_SET_CUST = 1,
    KXTJ2_1009_CUST_ACTION_SET_CALI,
    KXTJ2_1009_CUST_ACTION_RESET_CALI
}CUST_ACTION;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
}KXTJ2_1009_CUST;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    part;
    int32_t     data[0];
}KXTJ2_1009_SET_CUST;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    int32_t     data[KXTJ2_1009_AXES_NUM];
}KXTJ2_1009_SET_CALI;
/*----------------------------------------------------------------------------*/
typedef KXTJ2_1009_CUST KXTJ2_1009_RESET_CALI;
/*----------------------------------------------------------------------------*/
typedef union
{
    uint32_t                data[10];
    KXTJ2_1009_CUST         cust;
    KXTJ2_1009_SET_CUST     setCust;
    KXTJ2_1009_SET_CALI     setCali;
    KXTJ2_1009_RESET_CALI   resetCali;
}KXTJ2_1009_CUST_DATA;
/*----------------------------------------------------------------------------*/

#endif

