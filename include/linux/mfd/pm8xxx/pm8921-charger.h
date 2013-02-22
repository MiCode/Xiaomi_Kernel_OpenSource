/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/power_supply.h>

#define PM8921_CHARGER_DEV_NAME	"pm8921-charger"

struct pm8xxx_charger_core_data {
	unsigned int	vbat_channel;
	unsigned int	batt_temp_channel;
	unsigned int	batt_id_channel;
};

enum pm8921_chg_cold_thr {
	PM_SMBC_BATT_TEMP_COLD_THR__LOW,
	PM_SMBC_BATT_TEMP_COLD_THR__HIGH
};

enum pm8921_chg_hot_thr	{
	PM_SMBC_BATT_TEMP_HOT_THR__LOW,
	PM_SMBC_BATT_TEMP_HOT_THR__HIGH
};

enum pm8921_usb_ov_threshold {
	PM_USB_OV_5P5V,
	PM_USB_OV_6V,
	PM_USB_OV_6P5V,
	PM_USB_OV_7V,
};

enum pm8921_usb_debounce_time {
	PM_USB_BYPASS_DEBOUNCER,
	PM_USB_DEBOUNCE_20P5MS,
	PM_USB_DEBOUNCE_40P5MS,
	PM_USB_DEBOUNCE_80P5MS,
};

enum pm8921_chg_led_src_config {
	LED_SRC_GND,
	LED_SRC_VPH_PWR,
	LED_SRC_5V,
	LED_SRC_MIN_VPH_5V,
	LED_SRC_BYPASS,
};

/**
 * struct pm8921_charger_platform_data -
 *			valid range 4 to 512 min. PON default 120 min
 * @ttrkl_time:		max trckl charging time in minutes
 *			valid range 1 to 64 mins. PON default 15 min
 * @update_time:	how often the userland be updated of the charging (msec)
 * @alarm_low_mv:	the voltage (mV) when low battery alarm is triggered
 * @alarm_high_mv:	the voltage (mV) when high battery alarm is triggered
 * @max_voltage:	the max voltage (mV) the battery should be charged up to
 * @min_voltage:	the voltage (mV) where charging method switches from
 *			trickle to fast. This is also the minimum voltage the
 *			system operates at
 * @uvd_thresh_voltage:	the USB falling UVD threshold (mV) (PM8917 only)
 * @safe_current_ma:	The upper limit of current allowed to be pushed in
 *			battery. This ends up writing in a one time
 *			programmable register.
 * @resume_voltage_delta:	the (mV) drop to wait for before resume charging
 *				after the battery has been fully charged
 * @resume_charge_percent:	the % SOC the charger will drop to after the
 *				battery is fully charged before resuming
 *				charging.
 * @term_current:	the charger current (mA) at which EOC happens
 * @cool_temp:		the temperature (degC) at which the battery is
 *			considered cool charging current and voltage is reduced.
 *			Use INT_MIN to indicate not valid.
 * @warm_temp:		the temperature (degC) at which the battery is
 *			considered warm charging current and voltage is reduced
 *			Use INT_MIN to indicate not valid.
 * @temp_check_period:	The polling interval in seconds to check battery
 *			temeperature if it has gone to cool or warm temperature
 *			area
 * @max_bat_chg_current:	Max charge current of the battery in mA
 *				Usually 70% of full charge capacity
 * @usb_max_current:		Maximum USB current in mA
 * @cool_bat_chg_current:	chg current (mA) when the battery is cool
 * @warm_bat_chg_current:	chg current (mA)  when the battery is warm
 * @cool_bat_voltage:		chg voltage (mV) when the battery is cool
 * @warm_bat_voltage:		chg voltage (mV) when the battery is warm
 * @get_batt_capacity_percent:
 *			a board specific function to return battery
 *			capacity. If null - a default one will be used
 * @has_dc_supply:	report DC online if this bit is set in board file
 * @trkl_voltage:	the trkl voltage in (mV) below which hw controlled
 *			 trkl charging happens with linear charger
 * @weak_voltage:	the weak voltage (mV) below which hw controlled
 *			trkl charging happens with switching mode charger
 * @trkl_current:	the trkl current in (mA) to use for trkl charging phase
 * @weak_current:	the weak current in (mA) to use for weak charging phase
 * @vin_min:		the input voltage regulation point (mV) - if the
 *			voltage falls below this, the charger reduces charge
 *			current or stop charging temporarily
 * @thermal_mitigation: the array of charge currents to use as temperature
 *			increases
 * @thermal_levels:	the number of thermal mitigation levels supported
 * @cold_thr:		if high battery will be cold when VBAT_THERM goes above
 *			80% of VREF_THERM (typically 1.8volts), if low the
 *			battery will be considered cold if VBAT_THERM goes above
 *			70% of VREF_THERM. Hardware defaults to low.
 * @hot_thr:		if high the battery will be considered hot when the
 *			VBAT_THERM goes below 35% of VREF_THERM, if low the
 *			battery will be considered hot when VBAT_THERM goes
 *			below 25% of VREF_THERM. Hardware defaults to low.
 * @rconn_mohm:		resistance in milliOhm from the vbat sense to ground
 *			with the battery terminals shorted. This indicates
 *			resistance of the pads, connectors, battery terminals
 *			and rsense.
 * @led_src_config:	Power source for anode of charger indicator LED.
 * @btc_override:	disable the comparators for conifugrations where a
 *			suitable voltages don't appear on vbatt therm line
 *			for the charger to detect battery is either cold / hot.
 * @btc_override_cold_degc:	Temperature in degCelcius when the battery is
 *				deemed cold and charging never happens. Used
 *				only if btc_override = 1
 * @btc_override_hot_degc:	Temperature in degCelcius when the battery is
 *				deemed hot and charging never happens. Used
 *				only if btc_override = 1
 * @btc_delay_ms:	Delay in milliseconds to monitor the battery temperature
 *			while charging when btc_override = 1
 * @btc_panic_if_cant_stop_chg:	flag to instruct the driver to panic if the
 *				driver couldn't stop charging when battery
 *				temperature is out of bounds. Used only if
 *				btc_override = 1
 * stop_chg_upon_expiry:	flag to indicate that the charger driver should
 *				stop charging the battery when the safety timer
 *				expires. If not set the charger driver will
 *				restart charging upon expiry.
 */
struct pm8921_charger_platform_data {
	struct pm8xxx_charger_core_data	charger_cdata;
	unsigned int			ttrkl_time;
	unsigned int			update_time;
	unsigned int			max_voltage;
	unsigned int			min_voltage;
	unsigned int			uvd_thresh_voltage;
	unsigned int			safe_current_ma;
	unsigned int			alarm_low_mv;
	unsigned int			alarm_high_mv;
	unsigned int			resume_voltage_delta;
	int				resume_charge_percent;
	unsigned int			term_current;
	int				cool_temp;
	int				warm_temp;
	unsigned int			temp_check_period;
	unsigned int			max_bat_chg_current;
	unsigned int			usb_max_current;
	unsigned int			cool_bat_chg_current;
	unsigned int			warm_bat_chg_current;
	unsigned int			cool_bat_voltage;
	unsigned int			warm_bat_voltage;
	unsigned int			(*get_batt_capacity_percent) (void);
	int64_t				batt_id_min;
	int64_t				batt_id_max;
	bool				keep_btm_on_suspend;
	bool				has_dc_supply;
	int				trkl_voltage;
	int				weak_voltage;
	int				trkl_current;
	int				weak_current;
	int				vin_min;
	int				*thermal_mitigation;
	int				thermal_levels;
	enum pm8921_chg_cold_thr	cold_thr;
	enum pm8921_chg_hot_thr		hot_thr;
	int				rconn_mohm;
	enum pm8921_chg_led_src_config	led_src_config;
	int				battery_less_hardware;
	int				btc_override;
	int				btc_override_cold_degc;
	int				btc_override_hot_degc;
	int				btc_delay_ms;
	int				btc_panic_if_cant_stop_chg;
	int				stop_chg_upon_expiry;
	bool				disable_chg_rmvl_wrkarnd;
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
 * pm8921_disable_input_current_limt - disable input current limit
 *
 * @disable: disable input curren_limit limit
 *
 * Disabling the charge current limit causes current
 * current limits to have no monitoring. An adequate charger
 * capable of supplying high current while sustaining VIN_MIN
 * is required if input current limiting is disabled.
 */
int pm8921_disable_input_current_limit(bool disable);

/**
 * pm8921_set_usb_power_supply_type - set USB supply type
 *
 * @type: power_supply_type enum
 *
 * This api lets one set a specific usb power_supply_type.
 * USB drivers can distinguish between types of USB connections
 * and set the appropriate type for the USB supply.
 */

int pm8921_set_usb_power_supply_type(enum power_supply_type type);

/**
 * pm8921_disable_source_current - disable drawing current from source
 * @disable: true to disable current drawing from source false otherwise
 *
 * This function will stop all charging activities and disable any current
 * drawn from the charger. The battery provides the system current.
 */
int pm8921_disable_source_current(bool disable);

/**
 * pm8921_regulate_input_voltage -
 * @voltage: voltage in millivolts to regulate
 *		allowable values are from 4300mV to 6500mV
 */
int pm8921_regulate_input_voltage(int voltage);
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
/**
 * pm8921_usb_ovp_set_threshold -
 * Set the usb threshold as defined in by
 * enum usb_ov_threshold
 */
int pm8921_usb_ovp_set_threshold(enum pm8921_usb_ov_threshold ov);

/**
 * pm8921_usb_ovp_set_hystersis -
 * @ms: the debounce time enum
 *
 * Sets the debounce time for usb insertion/removal detection
 *
 */
int pm8921_usb_ovp_set_hystersis(enum pm8921_usb_debounce_time ms);

/**
 * pm8921_usb_ovp_disable -
 *
 * when disabled there is no over voltage protection. The usb voltage is
 * fed to the pmic as is. This should be disabled only when there is
 * over voltage protection circuitry present outside the pmic chip.
 *
 */
int pm8921_usb_ovp_disable(int disable);
/**
 * pm8921_is_batfet_closed - battery fet status
 *
 * Returns 1 if batfet is closed 0 if open. On configurations without
 * batfet this will return 0.
 */
int pm8921_is_batfet_closed(void);
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
static inline int pm8917_set_under_voltage_detection_threshold(int mv)
{
	return -ENXIO;
}
static inline int pm8921_disable_input_current_limit(bool disable)
{
	return -ENXIO;
}
static inline int pm8921_set_usb_power_supply_type(enum power_supply_type type)
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
static inline int pm8921_regulate_input_voltage(int voltage)
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
static inline int pm8921_usb_ovp_set_threshold(enum pm8921_usb_ov_threshold ov)
{
	return -ENXIO;
}
static inline int pm8921_usb_ovp_set_hystersis(enum pm8921_usb_debounce_time ms)
{
	return -ENXIO;
}
static inline int pm8921_usb_ovp_disable(int disable)
{
	return -ENXIO;
}
static inline int pm8921_is_batfet_closed(void)
{
	return 1;
}
#endif

#endif
