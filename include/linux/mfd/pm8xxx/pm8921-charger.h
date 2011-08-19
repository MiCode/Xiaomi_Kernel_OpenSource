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
	unsigned int	batt_temp_channel;
	unsigned int	batt_id_channel;
};

/**
 * struct pm8921_charger_platform_data -
 * @safety_time:	max charging time in minutes
 * @ttrkl_time:		max trckl charging time in minutes
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
	unsigned int			ttrkl_time;
	unsigned int			update_time;
	unsigned int			max_voltage;
	unsigned int			min_voltage;
	unsigned int			resume_voltage;
	unsigned int			term_current;
	unsigned int			(*get_batt_capacity_percent) (void);
	int64_t				batt_id_min;
	int64_t				batt_id_max;
};

enum pm8921_charger_source {
	PM8921_CHG_SRC_NONE,
	PM8921_CHG_SRC_USB,
	PM8921_CHG_SRC_DC,
};

#if defined(CONFIG_PM8921_CHARGER) || defined(CONFIG_PM8921_CHARGER_MODULE)
void pm8921_charger_vbus_draw(unsigned int mA);
int pm8921_charger_register_vbus_sn(void (*callback)(int));
void pm8921_charger_unregister_vbus_sn(void (*callback)(int));
/**
 * pm8921_charger_enable -
 *
 * @enable: 1 means enable charging, 0 means disable
 *
 * Enable/Disable battery charging current, the device will still draw current
 * from the charging source
 */
int pm8921_charger_enable(bool enable);

/**
 * pm8921_is_usb_chg_plugged_in - is usb plugged in
 *
 * if usb is under voltage or over voltage this will return false
 */
int pm8921_is_usb_chg_plugged_in(void);

/**
 * pm8921_is_dc_chg_plugged_in - is dc plugged in
 *
 * if dc is under voltage or over voltage this will return false
 */
int pm8921_is_dc_chg_plugged_in(void);

/**
 * pm8921_is_battery_present -
 *
 * returns if the pmic sees the battery present
 */
int pm8921_is_battery_present(void);

/**
 * pm8921_set_max_battery_charge_current - set max battery chg current
 *
 * @ma: max charge current in milliAmperes
 */
int pm8921_set_max_battery_charge_current(int ma);

/**
 * pm8921_disable_source_current - disable drawing current from source
 * @disable: true to disable current drawing from source false otherwise
 *
 * This function will stop all charging activities and disable any current
 * drawn from the charger. The battery provides the system current.
 */
int pm8921_disable_source_current(bool disable);

/**
 * pm8921_is_battery_charging -
 * @source: when the battery is charging the source is updated to reflect which
 *		charger, usb or dc, is charging the battery.
 *
 * RETURNS: bool, whether the battery is being charged or not
 */
bool pm8921_is_battery_charging(int *source);

/**
 * pm8921_batt_temperature - get battery temp in degC
 *
 */
int pm8921_batt_temperature(void);
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
static inline int pm8921_charger_enable(bool enable)
{
	return -ENXIO;
}
static inline int pm8921_is_usb_chg_plugged_in(void)
{
	return -ENXIO;
}
static inline int pm8921_is_dc_chg_plugged_in(void)
{
	return -ENXIO;
}
static inline int pm8921_is_battery_present(void)
{
	return -ENXIO;
}
static inline int pm8921_set_max_battery_charge_current(int ma)
{
	return -ENXIO;
}
static inline int pm8921_disable_source_current(bool disable)
{
	return -ENXIO;
}
static inline bool pm8921_is_battery_charging(int *source)
{
	*source = PM8921_CHG_SRC_NONE;
	return 0;
}
static inline int pm8921_batt_temperature(void)
{
	return -ENXIO;
}
#endif

#endif
