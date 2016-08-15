#ifndef LINUX_BMP180_MODULE_H
#define LINUX_BMP180_MODULE_H
/*
 * Copyright (c) 2011 Sony Ericsson Mobile Communications AB.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Optional platform configuration of the bmp180 digital barometric pressure
 * and temperature sensor from Bosch Sensortec.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

struct bmp180_platform_data {
	int (*gpio_setup) (bool request);
};

#define BMP180_CLIENT_NAME		"bmp180"
#define BMP180_ULTRA_HIGH_RESOLUTION	(3)
#define BMP180_HIGH_RESOLUTION		(2)
#define BMP180_STANDARD			(1)
#define BMP180_ULTRA_LOW_POWER		(0)

#endif /* LINUX_BMP180_MODULE_H */
