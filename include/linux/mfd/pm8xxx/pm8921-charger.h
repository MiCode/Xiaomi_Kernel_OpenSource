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

#ifndef __PM8XXX_CHARGER_H
#define __PM8XXX_CHARGER_H

#include <linux/errno.h>

#define PM8921_CHARGER_DEV_NAME	"pm8921-charger"

struct pm8xxx_charger_core_data {
	unsigned int	vbat_channel;
};

/**
 * struct pm8921_charger_platform_data -
 * @safety_time:	max charging time in minutes
 * @update_time:	how often the userland be updated of the charging
 * @max_voltage:	the max voltage the battery should be charged up to
 * @min_voltage:	the voltage where charging method switches from trickle
 *			to fast. This is also the minimum voltage the system
 *			operates at
 * @resume_voltage:	the voltage to wait for before resume charging after the
 *			battery has been fully charged
 * @term_current:	the charger current at which EOC happens
 * @get_batt_capacity_percent:
 *			a board specific function to return battery
 *			capacity. If null - a default one will be used
 *
 */
struct pm8921_charger_platform_data {
	struct pm8xxx_charger_core_data	charger_cdata;
	unsigned int			safety_time;
	unsigned int			update_time;
	unsigned int			max_voltage;
	unsigned int			min_voltage;
	unsigned int			resume_voltage;
	unsigned int			term_current;
	unsigned int			(*get_batt_capacity_percent) (void);
};

#if defined(CONFIG_PM8921_CHARGER) || defined(CONFIG_PM8921_CHARGER_MODULE)
void pm8921_charger_vbus_draw(unsigned int mA);
int pm8921_charger_register_vbus_sn(void (*callback)(int));
void pm8921_charger_unregister_vbus_sn(void (*callback)(int));
#else
static inline void pm8921_charger_vbus_draw(unsigned int mA)
{
}
static inline int pm8921_charger_register_vbus_sn(void (*callback)(int))
{
	return -ENXIO;
}
static inline void pm8921_charger_unregister_vbus_sn(void (*callback)(int))
{
}
#endif

#endif
