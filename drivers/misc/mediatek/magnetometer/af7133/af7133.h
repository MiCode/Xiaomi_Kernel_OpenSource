/* include/linux/af7133.h - AF7133 compass driver
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Definitions for af7133 compass chip.
 */
#ifndef AF7133_H
#define AF7133_H

#include <linux/ioctl.h>

#define AF7133_I2C_ADDRESS 	0x1E  //7-bit address 
#define AF7133_BUFSIZE      6


/*AF7133 register address*/
#define AF7133_REG_DATA  	0x00
#define AF7133_REG_STATUS	0x06
#define AF7133_REG_MODE		0x0B
#define AF7133_REG_HF		  0x10
#define AF7133_REG_PCODE  0x16
#define AF7133_REG_POR		0x18
#define AF7133_REG_RATE		0x19
#define AF7133_REG_CHIPID 0x1C


/*AF7133 parameters*/
#define AF7133_MODE_IDLE  	0x00
#define AF7133_MODE_WAKE   	0x01
#define AF7133_MODE_SINGLE  0x02

#define AF7133_HF_ENABLE  	0x80
#define AF7133_HF_DISABLE  	0x00

#define AF7133_PCODE_VALUE  0x48

#define AF7133_POR_RESET    0x01

#define AF7133_RATE_10HZ  0x0C
#define AF7133_RATE_25HZ	0x05
#define AF7133_RATE_30HZ	0x04

// conversion of magnetic data (for AF7133) to uT units
#define CONVERT_M			5
#define CONVERT_M_DIV		100		

// conversion of orientation data to degree units
#define CONVERT_O			1
#define CONVERT_O_DIV		1



#endif

