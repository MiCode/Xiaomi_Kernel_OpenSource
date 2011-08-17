/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PMIC8058_CHARGER_H__
#define __PMIC8058_CHARGER_H__

#if defined(CONFIG_BATTERY_MSM8X60) || defined(CONFIG_BATTERY_MSM8X60_MODULE)
int pmic8058_get_charge_batt(void);
int pmic8058_set_charge_batt(int);
#else
int pmic8058_get_charge_batt(void)
{
	return -ENXIO;
}
int pmic8058_set_charge_batt(int)
{
	return -ENXIO;
}
#endif
#endif /* __PMIC8058_CHARGER_H__ */
