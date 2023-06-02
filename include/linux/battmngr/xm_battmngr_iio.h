
#ifndef __XM_BATTMNGR_IIO_H
#define __XM_BATTMNGR_IIO_H

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/power_supply.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct xm_battmngr_iio {
	struct device *dev;

	struct iio_channel	**iio_chan_list_cp;
	struct iio_channel	**iio_chan_list_cp_sec;
	struct iio_channel	**iio_chan_list_main_chg;
	struct iio_channel	**iio_chan_list_batt_fg;
	struct iio_channel	**iio_chan_list_pd;

	//struct iio_channel	*chg_pump_therm;
	struct iio_channel	*typec_conn_therm;
};

enum iio_type {
	CP_MASTER,
	CP_SLAVE,
	MAIN_CHG,
	BATT_FG,
	PD_PHY,
};

enum cp_iio_channels {
	CHARGE_PUMP_SC_PRESENT,
	CHARGE_PUMP_SC_CHARGING_ENABLED,
	CHARGE_PUMP_SC_STATUS,
	CHARGE_PUMP_SC_BATTERY_PRESENT,
	CHARGE_PUMP_SC_VBUS_PRESENT,
	CHARGE_PUMP_SC_BATTERY_VOLTAGE,
	CHARGE_PUMP_SC_BATTERY_CURRENT,
	CHARGE_PUMP_SC_BATTERY_TEMPERATURE,
	CHARGE_PUMP_SC_BUS_VOLTAGE,
	CHARGE_PUMP_SC_BUS_CURRENT,
	CHARGE_PUMP_SC_BUS_TEMPERATURE,
	CHARGE_PUMP_SC_DIE_TEMPERATURE,
	CHARGE_PUMP_SC_ALARM_STATUS,
	CHARGE_PUMP_SC_FAULT_STATUS,
	CHARGE_PUMP_SC_VBUS_ERROR_STATUS,
	CHARGE_PUMP_LN_PRESENT,
	CHARGE_PUMP_LN_CHARGING_ENABLED,
	CHARGE_PUMP_LN_STATUS,
	CHARGE_PUMP_LN_BATTERY_PRESENT,
	CHARGE_PUMP_LN_VBUS_PRESENT,
	CHARGE_PUMP_LN_BATTERY_VOLTAGE,
	CHARGE_PUMP_LN_BATTERY_CURRENT,
	CHARGE_PUMP_LN_BATTERY_TEMPERATURE,
	CHARGE_PUMP_LN_BUS_VOLTAGE,
	CHARGE_PUMP_LN_BUS_CURRENT,
	CHARGE_PUMP_LN_BUS_TEMPERATURE,
	CHARGE_PUMP_LN_DIE_TEMPERATURE,
	CHARGE_PUMP_LN_ALARM_STATUS,
	CHARGE_PUMP_LN_FAULT_STATUS,
	CHARGE_PUMP_LN_VBUS_ERROR_STATUS,
	CHARGE_PUMP_LN_REG_STATUS,
};

static const char * const cp_iio_chan[] = {
	[CHARGE_PUMP_SC_PRESENT] = "sc_present",
	[CHARGE_PUMP_SC_CHARGING_ENABLED] = "sc_charging_enabled",
	[CHARGE_PUMP_SC_STATUS] = "sc_status",
	[CHARGE_PUMP_SC_BATTERY_PRESENT] = "sc_battery_present",
	[CHARGE_PUMP_SC_VBUS_PRESENT] = "sc_vbus_present",
	[CHARGE_PUMP_SC_BATTERY_VOLTAGE] = "sc_battery_voltage",
	[CHARGE_PUMP_SC_BATTERY_CURRENT] = "sc_battery_current",
	[CHARGE_PUMP_SC_BATTERY_TEMPERATURE] = "sc_battery_temperature",
	[CHARGE_PUMP_SC_BUS_VOLTAGE] = "sc_bus_voltage",
	[CHARGE_PUMP_SC_BUS_CURRENT] = "sc_bus_current",
	[CHARGE_PUMP_SC_BUS_TEMPERATURE] = "sc_bus_temperature",
	[CHARGE_PUMP_SC_DIE_TEMPERATURE] = "sc_die_temperature",
	[CHARGE_PUMP_SC_ALARM_STATUS] = "sc_alarm_status",
	[CHARGE_PUMP_SC_FAULT_STATUS] = "sc_fault_status",
	[CHARGE_PUMP_SC_VBUS_ERROR_STATUS] = "sc_vbus_error_status",
	[CHARGE_PUMP_LN_PRESENT] = "ln_present",
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "ln_charging_enabled",
	[CHARGE_PUMP_LN_STATUS] = "ln_status",
	[CHARGE_PUMP_LN_BATTERY_PRESENT] = "ln_battery_present",
	[CHARGE_PUMP_LN_VBUS_PRESENT] = "ln_vbus_present",
	[CHARGE_PUMP_LN_BATTERY_VOLTAGE] = "ln_battery_voltage",
	[CHARGE_PUMP_LN_BATTERY_CURRENT] = "ln_battery_current",
	[CHARGE_PUMP_LN_BATTERY_TEMPERATURE] = "ln_battery_temperature",
	[CHARGE_PUMP_LN_BUS_VOLTAGE] = "ln_bus_voltage",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "ln_bus_current",
	[CHARGE_PUMP_LN_BUS_TEMPERATURE] = "ln_bus_temperature",
	[CHARGE_PUMP_LN_DIE_TEMPERATURE] = "ln_die_temperature",
	[CHARGE_PUMP_LN_ALARM_STATUS] = "ln_alarm_status",
	[CHARGE_PUMP_LN_FAULT_STATUS] = "ln_fault_status",
	[CHARGE_PUMP_LN_VBUS_ERROR_STATUS] = "ln_vbus_error_status",
	[CHARGE_PUMP_LN_REG_STATUS] = "ln_reg_status",
};

static const char * const cp_sec_iio_chan[] = {
	[CHARGE_PUMP_SC_PRESENT] = "sc_present_slave",
	[CHARGE_PUMP_SC_CHARGING_ENABLED] = "sc_charging_enabled_slave",
	[CHARGE_PUMP_SC_STATUS] = "sc_status_slave",
	[CHARGE_PUMP_SC_BATTERY_PRESENT] = "sc_battery_present_slave",
	[CHARGE_PUMP_SC_VBUS_PRESENT] = "sc_vbus_present_slave",
	[CHARGE_PUMP_SC_BATTERY_VOLTAGE] = "sc_battery_voltage_slave",
	[CHARGE_PUMP_SC_BATTERY_CURRENT] = "sc_battery_current_slave",
	[CHARGE_PUMP_SC_BATTERY_TEMPERATURE] = "sc_battery_temperature_slave",
	[CHARGE_PUMP_SC_BUS_VOLTAGE] = "sc_bus_voltage_slave",
	[CHARGE_PUMP_SC_BUS_CURRENT] = "sc_bus_current_slave",
	[CHARGE_PUMP_SC_BUS_TEMPERATURE] = "sc_bus_temperature_slave",
	[CHARGE_PUMP_SC_DIE_TEMPERATURE] = "sc_die_temperature_slave",
	[CHARGE_PUMP_SC_ALARM_STATUS] = "sc_alarm_status_slave",
    [CHARGE_PUMP_SC_FAULT_STATUS] = "sc_fault_status_slave",
	[CHARGE_PUMP_SC_VBUS_ERROR_STATUS] = "sc_vbus_error_status_slave",
	[CHARGE_PUMP_LN_PRESENT] = "ln_present_slave",
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "ln_charging_enabled_slave",
	[CHARGE_PUMP_LN_STATUS] = "ln_status_slave",
	[CHARGE_PUMP_LN_BATTERY_PRESENT] = "ln_battery_present_slave",
	[CHARGE_PUMP_LN_VBUS_PRESENT] = "ln_vbus_present_slave",
	[CHARGE_PUMP_LN_BATTERY_VOLTAGE] = "ln_battery_voltage_slave",
	[CHARGE_PUMP_LN_BATTERY_CURRENT] = "ln_battery_current_slave",
	[CHARGE_PUMP_LN_BATTERY_TEMPERATURE] = "ln_battery_temperature_slave",
	[CHARGE_PUMP_LN_BUS_VOLTAGE] = "ln_bus_voltage_slave",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "ln_bus_current_slave",
	[CHARGE_PUMP_LN_BUS_TEMPERATURE] = "ln_bus_temperature_slave",
	[CHARGE_PUMP_LN_DIE_TEMPERATURE] = "ln_die_temperature_slave",
	[CHARGE_PUMP_LN_ALARM_STATUS] = "ln_alarm_status_slave",
    [CHARGE_PUMP_LN_FAULT_STATUS] = "ln_fault_status_slave",
	[CHARGE_PUMP_LN_VBUS_ERROR_STATUS] = "ln_vbus_error_status_slave",
	[CHARGE_PUMP_LN_REG_STATUS] = "ln_reg_status",
};

enum main_chg_iio_channels {
	MAIN_CHARGER_PRESENT,
	MAIN_CHARGER_ONLINE,
	MAIN_CHARGER_DONE,
	MAIN_CHARGER_HZ,
	MAIN_CHARGER_INPUT_CURRENT_SETTLED,
	MAIN_CHARGER_INPUT_VOLTAGE_SETTLED,
	MAIN_CHARGER_CURRENT,
	MAIN_CHARGER_ENABLED,
	MAIN_CHARGER_OTG_ENABLE,
	MAIN_CHARGER_TERM,
	MAIN_CHARGER_VOLTAGE_TERM,
	MAIN_CHARGER_STATUS,
	MAIN_CHARGER_TYPE,
	MAIN_CHARGER_USB_TYPE,
	MAIN_CHARGER_BUS_VOLTAGE,
	MAIN_CHARGER_VBAT_VOLTAGE,
	MAIN_CHARGER_ENABLE_CHARGER_TERM,
};

static const char * const main_chg_iio_chan[] = {
	[MAIN_CHARGER_PRESENT] = "syv_charge_present",
	[MAIN_CHARGER_ONLINE] = "syv_charge_online",
	[MAIN_CHARGER_DONE] = "syv_charge_done",
	[MAIN_CHARGER_HZ] = "syv_chager_hz",
	[MAIN_CHARGER_INPUT_CURRENT_SETTLED] = "syv_input_current_settled",
	[MAIN_CHARGER_INPUT_VOLTAGE_SETTLED] = "syv_input_voltage_settled",
	[MAIN_CHARGER_CURRENT] = "syv_charge_current",
	[MAIN_CHARGER_ENABLED] = "syv_charger_enable",
	[MAIN_CHARGER_OTG_ENABLE] = "syv_otg_enable",
	[MAIN_CHARGER_TERM] = "syv_charger_term",
	[MAIN_CHARGER_VOLTAGE_TERM] = "syv_batt_voltage_term",
	[MAIN_CHARGER_STATUS] = "syv_charger_status",
	[MAIN_CHARGER_TYPE] = "syv_charger_type",
	[MAIN_CHARGER_USB_TYPE] = "syv_charger_usb_type",
	[MAIN_CHARGER_BUS_VOLTAGE] = "syv_vbus_voltage",
	[MAIN_CHARGER_VBAT_VOLTAGE] = "syv_vbat_voltage",
	[MAIN_CHARGER_ENABLE_CHARGER_TERM] = "syv_enable_charger_term",
};

enum batt_fg_iio_channels {
	BATT_FG_PRESENT,
	BATT_FG_STATUS,
	BATT_FG_VOLTAGE_NOW,
	BATT_FG_VOLTAGE_MAX,
	BATT_FG_CURRENT_NOW,
	BATT_FG_CAPACITY,
	BATT_FG_CAPACITY_LEVEL,
	BATT_FG_TEMP,
	BATT_FG_CHARGE_FULL,
	BATT_FG_CHARGE_FULL_DESIGN,
	BATT_FG_CYCLE_COUNT,
	BATT_FG_TIME_TO_EMPTY_NOW,
	BATT_FG_TIME_TO_FULL_NOW,
	BATT_FG_UPDATE_NOW,
	BATT_FG_THERM_CURR,
	BATT_FG_CHIP_OK,
	BATT_FG_BATTERY_AUTH,
	BATT_FG_SOC_DECIMAL,
	BATT_FG_SOC_DECIMAL_RATE,
	BATT_FG_SOH,
	BATT_FG_RSOC,
	BATT_FG_BATTERY_ID,
	BATT_FG_RESISTANCE_ID,
	BATT_FG_SHUTDOWN_DELAY,
	BATT_FG_FASTCHARGE_MODE,
	BATT_FG_TEMP_MAX,
	BATT_FG_TIME_OT,
	BATT_FG_REG_ROC,
	BATT_FG_RM,
};

static const char * const batt_fg_iio_chan[] = {
	[BATT_FG_PRESENT] = "bqfg_present",
	[BATT_FG_STATUS] = "bqfg_status",
	[BATT_FG_VOLTAGE_NOW] = "bqfg_voltage_now",
	[BATT_FG_VOLTAGE_MAX] = "bqfg_voltage_max",
	[BATT_FG_CURRENT_NOW] = "bqfg_current_now",
	[BATT_FG_CAPACITY] = "bqfg_capacity",
	[BATT_FG_CAPACITY_LEVEL] = "bqfg_capacity_level",
	[BATT_FG_TEMP] = "bqfg_temp",
	[BATT_FG_CHARGE_FULL] = "bqfg_charge_full",
	[BATT_FG_CHARGE_FULL_DESIGN] = "bqfg_charge_full_design",
	[BATT_FG_CYCLE_COUNT] = "bqfg_cycle_count",
	[BATT_FG_TIME_TO_EMPTY_NOW] = "bqfg_time_to_empty_now",
	[BATT_FG_TIME_TO_FULL_NOW] = "bqfg_time_to_full_now",
	[BATT_FG_UPDATE_NOW] = "bqfg_update_now",
	[BATT_FG_THERM_CURR] = "bqfg_therm_curr",
	[BATT_FG_CHIP_OK] = "bqfg_chip_ok",
	[BATT_FG_BATTERY_AUTH] = "bqfg_battery_auth",
	[BATT_FG_SOC_DECIMAL] = "bqfg_soc_decimal",
	[BATT_FG_SOC_DECIMAL_RATE] = "bqfg_soc_decimal_rate",
	[BATT_FG_SOH] = "bqfg_soh",
	[BATT_FG_RSOC] = "bqfg_rsoc",
	[BATT_FG_BATTERY_ID] = "bqfg_battery_id",
	[BATT_FG_RESISTANCE_ID] = "bqfg_resistance_id",
	[BATT_FG_SHUTDOWN_DELAY] = "bqfg_shutdown_delay",
	[BATT_FG_FASTCHARGE_MODE] = "bqfg_fastcharge_mode",
	[BATT_FG_TEMP_MAX] = "bqfg_temp_max",
	[BATT_FG_TIME_OT] = "bqfg_time_ot",
	[BATT_FG_REG_ROC] = "bqfg_reg_rsoc",
	[BATT_FG_RM] = "bqfg_rm",
};

enum pd_iio_channels {
	PD_ACTIVE,
	PD_CURRENT_MAX,
	PD_VOLTAGE_MIN,
	PD_VOLTAGE_MAX,
	PD_IN_HARD_RESET,
	PD_TYPEC_CC_ORIENTATION,
	PD_TYPEC_MODE,
	PD_USB_SUSPEND_SUPPORTED,
	PD_APDO_VOLT_MAX,
	PD_APDO_CURR_MAX,
	PD_USB_REAL_TYPE,
	PD_TYPEC_ACCESSORY_MODE,
	PD_TYPEC_ADAPTER_ID,
};

static const char * const pd_iio_chan[] = {
	[PD_ACTIVE] = "rt_pd_active",
	[PD_CURRENT_MAX] = "rt_pd_current_max",
	[PD_VOLTAGE_MIN] = "rt_pd_voltage_min",
	[PD_VOLTAGE_MAX] = "rt_pd_voltage_max",
	[PD_IN_HARD_RESET] = "rt_pd_in_hard_reset",
	[PD_TYPEC_CC_ORIENTATION] = "rt_typec_cc_orientation",
	[PD_TYPEC_MODE] = "rt_typec_mode",
	[PD_USB_SUSPEND_SUPPORTED] = "rt_pd_usb_suspend_supported",
	[PD_APDO_VOLT_MAX] = "rt_pd_apdo_volt_max",
	[PD_APDO_CURR_MAX] = "rt_pd_apdo_curr_max",
	[PD_USB_REAL_TYPE] = "rt_pd_usb_real_type",
	[PD_TYPEC_ACCESSORY_MODE] = "rt_typec_accessory_mode",
	[PD_TYPEC_ADAPTER_ID] = "rt_typec_adapter_id",
};

extern struct xm_battmngr_iio *g_battmngr_iio;
extern int xm_get_iio_channel(struct xm_battmngr_iio *battmngr_iio, const char *propname,
					struct iio_channel **chan);
extern int xm_battmngr_read_iio_prop(struct xm_battmngr_iio *battmngr_iio,
		enum iio_type type, int iio_chan, int *val);
extern int xm_battmngr_write_iio_prop(struct xm_battmngr_iio *battmngr_iio,
		enum iio_type type, int iio_chan, int val);
extern bool is_cp_master_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum cp_iio_channels chan);
extern bool is_cp_slave_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum cp_iio_channels chan);
extern bool is_main_chg_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum main_chg_iio_channels chan);
extern bool is_batt_fg_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum batt_fg_iio_channels chan);
extern bool is_pd_chan_valid(struct xm_battmngr_iio *battmngr_iio,
		enum batt_fg_iio_channels chan);
extern int xm_battmngr_iio_init(struct xm_battmngr_iio *battmngr_iio);

#endif /* __XM_BATTMNGR_IIO_H */

