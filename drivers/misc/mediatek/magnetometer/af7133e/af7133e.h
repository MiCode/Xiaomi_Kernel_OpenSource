/* include/linux/af7133e_af8133i.h - AF7133E_AF8133I compass driver
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
 * Definitions for af7133e_af8133i compass chip.
 */
#ifndef AF7133E_AF8133I_H
#define AF7133E_AF8133I_H

#include <linux/ioctl.h>

//#define AF7133E_AF8133I_I2C_ADDRESS 	0x1E  //7-bit address 
#define AF7133E_AF8133I_I2C_ADDRESS 	0x1c  //7-bit address 
#define AF7133E_AF8133I_BUFSIZE      6


/*AF7133 register address*/
#define AF7133E_AF8133I_REG_PCODE  	0x00
#define AF7133E_AF8133I_REG_STATUS	0x02
#define AF7133E_AF8133I_REG_DATA  	0x03
#define AF7133E_AF8133I_REG_STATE	0x0A
#define AF7133E_AF8133I_REG_RANGE	0x0B
#define AF7133E_AF8133I_REG_SWR		0x11

#define AF7133E_AF8133I_REG_POR		0x18
#define AF7133E_AF8133I_REG_RATE		0x19
#define AF7133E_AF8133I_REG_CHIPID 0x1C


/*AF7133 parameters*/
#define AF7133E_AF8133I_MODE_IDLE  	0x00
#define AF7133E_AF8133I_MODE_WAKE   	0x01
#define AF7133E_AF8133I_MODE_SINGLE  0x02

#define AF7133E_AF8133I_HF_ENABLE  	0x80
#define AF7133E_AF8133I_HF_DISABLE  	0x00

#define AF7133E_AF8133I_PCODE_VALUE  0x50

#define AF7133E_AF8133I_POR_RESET    0x01

#define AF7133E_AF8133I_RATE_10HZ  0x0C
#define AF7133E_AF8133I_RATE_25HZ	0x05
#define AF7133E_AF8133I_RATE_30HZ	0x04

// conversion of magnetic data (for AF7133) to uT units
#define CONVERT_M			5
#define CONVERT_M_DIV		100		

// conversion of orientation data to degree units
#define CONVERT_O			1
#define CONVERT_O_DIV		200



#endif

