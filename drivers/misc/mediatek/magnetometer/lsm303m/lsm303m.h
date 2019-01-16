/* linux/drivers/hwmon/LSM303D.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * LSM303D driver for MT6516
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


#ifndef LSM303M
#define LSM303M
#include <linux/ioctl.h>

/* Magnetometer Sensor Full Scale */
// conversion of magnetic data to uT units
#define CONVERT_M                   (1.0f/10.0f)
#define CONVERT_M_X                 (CONVERT_M)
#define CONVERT_M_Y                 (CONVERT_M)
#define CONVERT_M_Z                 (CONVERT_M)

#define	I2C_AUTO_INCREMENT	(0x80)


#define	LSM303M_REG_CTL0			0x1F
#define LSM303M_REG_DEVID			0x0F
#define	LSM303M_REG_BW_RATE			0x24
#define LSM303M_REG_DATA_FORMAT		0x25
#define LSM303M_REG_POWER_CTL  		0x26
#define LSM303M_REG_DATAX0		    0x08	

#define LSM303M_REG_OFSX            0XFF




#define LSM303M_FIXED_DEVID			0x49


#define LSM303M_MAG_FS_MASK	(0x60)
#define LSM303M_MAG_FS_2G	(0x00)	/* Full scale 2 gauss */
#define LSM303M_MAG_FS_4G	(0x20)	/* Full scale 4 gauss */
#define LSM303M_MAG_FS_8G	(0x40)	/* Full scale 8 gauss */
#define LSM303M_MAG_FS_12G	(0x60)	/* Full scale 12 gauss */


#define SENSITIVITY_MAG_2G	80	/**	ugauss/LSB	*/
#define SENSITIVITY_MAG_4G	160	/**	ugauss/LSB	*/
#define SENSITIVITY_MAG_8G	320	/**	ugauss/LSB	*/
#define SENSITIVITY_MAG_12G	480	/**	ugauss/LSB	*/

#define LSM303M_MAG_POWER_ON	  (0x00)  /* POWER ON */
#define LSM303M_MAG_POWER_OFF	  (0x02)  /* POWER ON */


#define ODR_MAG_MASK		  (0X1C)	/* Mask for odr change on mag */
#define LSM303M_MAG_ODR3_125  (0x00)  /* 3.25Hz output data rate */
#define LSM303M_MAG_ODR6_25	  (0x04)  /* 6.25Hz output data rate */
#define LSM303M_MAG_ODR12_5	  (0x08)  /* 12.5Hz output data rate */
#define LSM303M_MAG_ODR25	  (0x0C)  /* 25Hz output data rate */
#define LSM303M_MAG_ODR50	  (0x10)  /* 50Hz output data rate */
#define LSM303M_MAG_ODR100	  (0x14)  /* 100Hz output data rate */


#define LSM303M_SUCCESS						0
#define LSM303M_ERR_I2C						-1
#define LSM303M_ERR_STATUS					-3
#define LSM303M_ERR_SETUP_FAILURE			-4
#define LSM303M_ERR_GETGSENSORDATA			-5
#define LSM303M_ERR_IDENTIFICATION			-6
	  
	  
	  
#define LSM303M_BUFSIZE				256


#endif


