/* LSM6DS0_H
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * mpu300 head file for MT65xx
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
#ifndef LSM6DS0_H
#define LSM6DS0_H
	 
	 
#define LSM6DS0_I2C_SLAVE_ADDR		0xD4     //SD0 high--->D6    SD0 low ----->D4

#define LSM6DS0_FIXED_DEVID			0x68


/* LSM6DS0 Register Map  (Please refer to LSM6DS0 Specifications) */
#define LSM6DS0_REG_DEVID			         0x0F


#define LSM6DS0_REG_GYRO_XH			0x19
#define LSM6DS0_REG_GYRO_XL			0x18

#define LSM6DS0_CTL_REG1			0x10
#define LSM6DS0_CTL_REG8			0x22 

#define LSM6DS0_FIFO_CTL			0x2E 

#define LSM6DS0_CTL_REG4			0x23 
#define LSM6DS0_FIFO_SRC_REG		0x2F 
#define LSM6DS0_STATUS_REG		    0x17 


/*LSM6DS0 Register Bit definitions*/ 

#define LSM6DS0_FS_245_LSB			133	// LSB/(o/s)
#define LSM6DS0_FS_500_LSB			66 
#define LSM6DS0_FS_2000_LSB		16  
#define LSM6DS0_OUT_MAGNIFY		131

#define LSM6DS0_RANGE_245		    0x00
#define LSM6DS0_RANGE_500		    0x08
#define LSM6DS0_RANGE_2000		    0x10

#define LSM6DS0_FIFO_MODE_BYPASS   0x00
#define LSM6DS0_FIFO_MODE_FIFO     0x20
#define LSM6DS0_FIFO_MODE_STREAM   0x40

#define AUTO_INCREMENT 0x80


#define LSM6DS0_SAM_RATE_MASK		    0x07	//set sample rate and low padd filter configuration
#define LSM6DS0_RATE_8K_LPFB_256HZ 	0x00
#define LSM6DS0_RATE_1K_LPFB_188HZ	0x01
#define LSM6DS0_RATE_1K_LPFB_98HZ 	0x02
#define LSM6DS0_RATE_1K_LPFB_42HZ 	0x03
#define LSM6DS0_RATE_1K_LPFB_20HZ 	0x04
#define LSM6DS0_RATE_1K_LPFB_10HZ 	0x05
#define LSM6DS0_RATE_1K_LPFB_5HZ 	0x06


#define LSM6DS0_POWER_ON			0x60	
#define LSM6DS0_100HZ 0x00 
#define LSM6DS0_200HZ 0x40 
#define LSM6DS0_400HZ 0x80
#define LSM6DS0_800HZ 0xC0 


	 
#define LSM6DS0_SUCCESS		       0
#define LSM6DS0_ERR_I2C		      -1
#define LSM6DS0_ERR_STATUS			  -3
#define LSM6DS0_ERR_SETUP_FAILURE	  -4
#define LSM6DS0_ERR_GETGSENSORDATA  -5
#define LSM6DS0_ERR_IDENTIFICATION	  -6

#define LSM6DS0_BUFSIZE 60

// 1 rad = 180/PI degree, LSM6DS0_OUT_MAGNIFY = 131,
// 180*131/PI = 7506
#define DEGREE_TO_RAD	7506
	 
#endif //LSM6DS0_H

