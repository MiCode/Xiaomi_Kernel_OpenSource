/*
 * include/linux/ina3221.h
 *
 * Copyright (c) 2012, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _INA3221_H
#define _INA3221_H

#include <linux/types.h>

#define INA3221_CONFIG			0x00
#define INA3221_SHUNT_VOL_CHAN1		0x01
#define INA3221_BUS_VOL_CHAN1		0x02
#define INA3221_SHUNT_VOL_CHAN2		0x03
#define INA3221_BUS_VOL_CHAN2		0x04
#define INA3221_SHUNT_VOL_CHAN3		0x05
#define INA3221_BUS_VOL_CHAN3		0x06
#define INA3221_MASK_ENABLE		0x0F

#define INA3221_RESET			0x8000
#define INA3221_POWER_DOWN		0
#define INA3221_ENABLE_CHAN		(7 << 12) /* enable all 3 channels */
#define INA3221_AVG			(3 << 9) /* 64 averages */
#define INA3221_VBUS_CT			(4 << 6) /* Vbus 1.1 mS conv time */
#define INA3221_VSHUNT_CT		(4 << 3) /* Vshunt 1.1 mS conv time */
#define INA3221_CONT_MODE		7 /* continuous bus n shunt V measure */
#define INA3221_TRIG_MODE		3 /* triggered bus n shunt V measure */

#define INA3221_CONT_CONFIG_DATA	(INA3221_ENABLE_CHAN | INA3221_AVG | \
					INA3221_VBUS_CT | INA3221_VSHUNT_CT | \
					INA3221_CONT_MODE) /* 0x7727 */

#define INA3221_TRIG_CONFIG_DATA	(INA3221_ENABLE_CHAN | \
					INA3221_TRIG_MODE) /* 0x7723 */

#define INA3221_NUMBER_OF_RAILS		3
#define INA3221_RAIL_NAME_SIZE		32

struct ina3221_platform_data {
	char rail_name[INA3221_NUMBER_OF_RAILS][INA3221_RAIL_NAME_SIZE];
	u32 shunt_resistor[INA3221_NUMBER_OF_RAILS]; /* specify in mOhms */
	u16 cont_conf_data; /* config data for continuous mode */
	u16 trig_conf_data; /* config data for triggered mode */
};

#endif /* _LINUX_INA3221_H */
