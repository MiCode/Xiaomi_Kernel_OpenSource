/* linux/drivers/hwmon/LSM303C.c
 *
 * (C) Copyright 2008
 * MediaTek <www.mediatek.com>
 *
 * LSM303C driver for MT6516
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef LSM303C
#define LSM303C
#include <linux/ioctl.h>

/* Magnetometer Sensor Full Scale */
// conversion of magnetic data to uT units
#define CONVERT_M                   (1.0f/10.0f)
#define CONVERT_M_X                 (CONVERT_M)
#define CONVERT_M_Y                 (CONVERT_M)
#define CONVERT_M_Z                 (CONVERT_M)

#define	I2C_AUTO_INCREMENT	(0x80)


#define	LSM303C_REG_CTL0			0x1F
#define LSM303C_REG_DEVID			0x0F
#define	LSM303C_REG_BW_RATE			0x20
#define LSM303C_REG_DATA_FORMAT		0x21
#define LSM303C_REG_POWER_CTL  		0x22
#define LSM303C_REG_DATAX0		    0x28

#define LSM303C_REG_OFSX            0XFF




#define LSM303C_FIXED_DEVID			0x3d


#define LSM303C_MAG_FS_MASK	(0x60)
#define LSM303C_MAG_FS_16G	(0x60)	/* Full scale 16 gauss */


#define SENSITIVITY_MAG_16G	480	/**	ugauss/LSB	*/

#define LSM303C_MAG_POWER_ON	  (0x00)  /* POWER ON */
#define LSM303C_MAG_POWER_OFF	  (0x02)  /* POWER ON */


#define ODR_MAG_MASK		  (0X1C)	/* Mask for odr change on mag */
//#define LSM303C_MAG_ODR3_125  (0x00)  /* 3.25Hz output data rate */
#define LSM303C_MAG_ODR_625	  (0x00)  /* 0.625Hz output data rate */
#define LSM303C_MAG_ODR1_25	  (0x04)  /* 1.25Hz output data rate */
#define LSM303C_MAG_ODR2_5	  (0x08)  /* 2.5Hz output data rate */
#define LSM303C_MAG_ODR5	  (0x0c)  /* 5Hz output data rate */
#define LSM303C_MAG_ODR10	  (0x10)  /* 10Hz output data rate */
#define LSM303C_MAG_ODR20	  (0x14)  /* 20Hz output data rate */
#define LSM303C_MAG_ODR40	  (0x18)  /* 20Hz output data rate */
#define LSM303C_MAG_ODR80	  (0x1c)  /* 20Hz output data rate */


#define LSM303C_SUCCESS						0
#define LSM303C_ERR_I2C						-1
#define LSM303C_ERR_STATUS					-3
#define LSM303C_ERR_SETUP_FAILURE			-4
#define LSM303C_ERR_GETGSENSORDATA			-5
#define LSM303C_ERR_IDENTIFICATION			-6



#define LSM303C_BUFSIZE				256


#endif
