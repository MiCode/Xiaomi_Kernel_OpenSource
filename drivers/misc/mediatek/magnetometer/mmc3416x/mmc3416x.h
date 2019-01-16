/*
 * Copyright (C) 2010 MEMSIC, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * Definitions for mmc3416x magnetic sensor chip.
 */
#ifndef __MMC3416X_H__
#define __MMC3416X_H__

#include <linux/ioctl.h>

#define MMC3416X_I2C_NAME		"mmc3416x"

#define MMC3416X_I2C_ADDR		0x60

#define CALIBRATION_DATA_SIZE	12


#define MMC3416X_REG_CTRL		0x07
#define MMC3416X_REG_BITS		0x08
#define MMC3416X_REG_DATA		0x00
#define MMC3416X_REG_DS			0x06
#define MMC3416X_REG_PRODUCTID_0		0x10
#define MMC3416X_REG_PRODUCTID_1		0x20

#define MMC3416X_CTRL_TM			0x01
#define MMC3416X_CTRL_CM			0x02
#define MMC3416X_CTRL_50HZ		0x00
#define MMC3416X_CTRL_25HZ		0x04
#define MMC3416X_CTRL_12HZ		0x08
#define MMC3416X_CTRL_NOBOOST            0x10
#define MMC3416X_CTRL_SET  	        0x20
#define MMC3416X_CTRL_RESET              0x40
#define MMC3416X_CTRL_REFILL             0x80

#define MMC3416X_BITS_SLOW_16            0x00
#define MMC3416X_BITS_FAST_16            0x01
#define MMC3416X_BITS_14                 0x02


//add from the sensors_mag_mmc3416x.c
// conversion of magnetic data (for mmc328x) to uT units
// conversion of magnetic data to uT units
// 32768 = 1Guass = 100 uT
// 100 / 32768 = 25 / 8096
// 65536 = 360Degree
// 360 / 65536 = 45 / 8192


#define CONVERT_M			25
#define CONVERT_M_DIV		8096			
#define CONVERT_O			45
#define CONVERT_O_DIV		8192

// sensitivity 2048 count = 1 Guass = 100uT


#define MMC3416X_OFFSET_X		32768
#define MMC3416X_OFFSET_Y		32768
#define MMC3416X_OFFSET_Z		32768
#define MMC3416X_SENSITIVITY_X		2048
#define MMC3416X_SENSITIVITY_Y		2048
#define MMC3416X_SENSITIVITY_Z		2048


#endif /* __MMC3416X_H__ */

