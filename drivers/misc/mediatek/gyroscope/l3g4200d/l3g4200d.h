/* l3g4200d.h
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
#ifndef L3G4200D_H
#define L3G4200D_H
	 
#include <linux/ioctl.h>
	 
#define L3G4200D_I2C_SLAVE_ADDR		0xD0
#define L3G4200D_FIXED_DEVID			0xD3


/* L3G4200D Register Map  (Please refer to L3G4200D Specifications) */
#define L3G4200D_REG_DEVID			         0x0F

/*
#define L3G4200D_REG_DEVID			         0x00
#define L3G4200D_REG_OFS_XH			0x0C
#define L3G4200D_REG_OFS_XL		         0x0D
#define L3G4200D_REG_OFS_YH			0x0E
#define L3G4200D_REG_OFS_YL			0x0F
#define L3G4200D_REG_OFS_ZH			0x10
#define L3G4200D_REG_OFS_ZL			0x11
#define L3G4200D_REG_FIFO_EN			0x12
#define L3G4200D_REG_AUX_VDD			0x13
#define L3G4200D_REG_AUX_ID		         0x14
#define L3G4200D_REG_SAMRT_DIV		0x15
#define L3G4200D_REG_DATA_FMT			0x16   //set external sync, full-scale range and sample rate, low pass filter bandwidth
#define L3G4200D_REG_INT_EN			0x17
#define L3G4200D_REG_AUX_ADDR   		0x18
#define L3G4200D_REG_INT_STA			0x1A

#define L3G4200D_REG_TEMP_OUTH		0x1B
#define L3G4200D_REG_TEMP_OUTL	         0x1C
*/
#define L3G4200D_REG_GYRO_XH			0x29
#define L3G4200D_REG_GYRO_XL			0x28
//#define L3G4200D_REG_GYRO_YH			0x1F
//#define L3G4200D_REG_GYRO_YL			0x20
//#define L3G4200D_REG_GYRO_ZH			0x21
//#define L3G4200D_REG_GYRO_ZL			0x22
/*
#define L3G4200D_REG_AUX_XH			0x23
#define L3G4200D_REG_AUX_XL			0x24
#define L3G4200D_REG_AUX_YH			0x25
#define L3G4200D_REG_AUX_YL			0x26
#define L3G4200D_REG_AUX_ZH			0x27
#define L3G4200D_REG_AUX_ZL			0x28
#define L3G4200D_REG_FIFO_CNTH		0x3A
#define L3G4200D_REG_FIFO_CNTL		0x3B
#define L3G4200D_REG_FIFO_DATA		0x3C
#define L3G4200D_REG_FIFO_CTL			0x3D
*/
#define L3G4200D_CTL_REG1			0x20 
#define L3G4200D_FIFO_CTL			0x2E 

#define L3G4200D_CTL_REG4			0x23 
#define L3G4200D_FIFO_SRC_REG		0x2F 
#define L3G4200D_STATUS_REG		    0x27 



	 

/*L3G4200D Register Bit definitions*/ 

#define L3G4200D_FS_250_LSB			131	// LSB/(o/s)
#define L3G4200D_FS_500_LSB			66 
#define L3G4200D_FS_2000_LSB		16  
#define L3G4200D_OUT_MAGNIFY		131
#define L3G4200D_RANGE_250		    0x00
#define L3G4200D_RANGE_500		    0x10
#define L3G4200D_RANGE_2000		    0x30

#define L3G4200D_FIFO_MODE_BYPASS   0x00
#define L3G4200D_FIFO_MODE_FIFO     0x20
#define L3G4200D_FIFO_MODE_STREAM   0x40

#define AUTO_INCREMENT 0x80










#define L3G4200D_SAM_RATE_MASK		    0x07	//set sample rate and low padd filter configuration
#define L3G4200D_RATE_8K_LPFB_256HZ 	0x00
#define L3G4200D_RATE_1K_LPFB_188HZ	0x01
#define L3G4200D_RATE_1K_LPFB_98HZ 	0x02
#define L3G4200D_RATE_1K_LPFB_42HZ 	0x03
#define L3G4200D_RATE_1K_LPFB_20HZ 	0x04
#define L3G4200D_RATE_1K_LPFB_10HZ 	0x05
#define L3G4200D_RATE_1K_LPFB_5HZ 	0x06


#define L3G4200D_POWER_ON			0x08	
#define L3G4200D_100HZ 0x00 
#define L3G4200D_200HZ 0x40 
#define L3G4200D_400HZ 0x80
#define L3G4200D_800HZ 0xC0 


	 
#define L3G4200D_SUCCESS		       0
#define L3G4200D_ERR_I2C		      -1
#define L3G4200D_ERR_STATUS			  -3
#define L3G4200D_ERR_SETUP_FAILURE	  -4
#define L3G4200D_ERR_GETGSENSORDATA  -5
#define L3G4200D_ERR_IDENTIFICATION	  -6

#define L3G4200D_BUFSIZE 60

// 1 rad = 180/PI degree, L3G4200D_OUT_MAGNIFY = 131,
// 180*131/PI = 7506
#define DEGREE_TO_RAD	7506
	 
#endif //L3G4200D_H

