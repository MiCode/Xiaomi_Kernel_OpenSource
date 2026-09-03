/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo.Ham <myungjoo.ham@samsung.com>
 *
 * Charger Manager.
 * This framework enables to control and multiple chargers and to
 * monitor charging even in the context of suspend-to-RAM with
 * an interface combining the chargers.
 *
**/

//This file has been modified by Unisoc (Shanghai) Technologies Co., Ltd in 2023.

#ifndef _CHARGER_MANAGER_H
#define _CHARGER_MANAGER_H

#include <linux/alarmtimer.h>
#include <linux/extcon.h>
#include <linux/power_supply.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/power/sprd_vote.h>
#include <linux/power/sprd_vchg_detect.h>
#include <linux/power/sprd_fchg_extcon.h>
#include <linux/usb/sprd_commonphy.h>

enum cm_charge_info_cmd {
	CM_CHARGE_INFO_CHARGE_LIMIT = BIT(0),
	CM_CHARGE_INFO_INPUT_LIMIT = BIT(1),
	CM_CHARGE_INFO_THERMAL_LIMIT = BIT(2),
	CM_CHARGE_INFO_JEITA_LIMIT = BIT(3),
};

enum cm_charger_type_flag {
	CM_USB_TYPE = 0,
	CM_FCHG_TYPE,
	CM_WL_TYPE,
};

enum cm_charger_type {
	CM_CHARGER_TYPE_UNKNOWN = 0,
	CM_CHARGER_TYPE_SDP,
	CM_CHARGER_TYPE_DCP,
	CM_CHARGER_TYPE_CDP,
	CM_CHARGER_TYPE_FAST,
	CM_CHARGER_TYPE_ADAPTIVE,
	CM_WIRELESS_CHARGER_TYPE_BPP,
	CM_WIRELESS_CHARGER_TYPE_EPP,
};

enum power_supply_wireless_charger_type {
	POWER_SUPPLY_WIRELESS_CHARGER_TYPE_UNKNOWN = 0x20,
	POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP,		/* Standard wireless bpp mode */
	POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP,		/* Standard wireless epp mode */
};

enum data_source {
	CM_BATTERY_PRESENT,
	CM_NO_BATTERY,
	CM_FUEL_GAUGE,
	CM_CHARGER_STAT,
};

enum polling_modes {
	CM_POLL_DISABLE = 0,
	CM_POLL_ALWAYS,
	CM_POLL_EXTERNAL_POWER_ONLY,
	CM_POLL_CHARGING_ONLY,
};

enum cm_event_types {
	CM_EVENT_UNKNOWN = 0,
	CM_EVENT_BATT_FULL,
	CM_EVENT_BATT_IN,
	CM_EVENT_BATT_OUT,
	CM_EVENT_BATT_OVERHEAT,
	CM_EVENT_BATT_COLD,
	CM_EVENT_EXT_PWR_IN_OUT,
	CM_EVENT_CHG_START_STOP,
	CM_EVENT_UPDATE_USB_LIMINT,
	CM_EVENT_WL_CHG_START_STOP,
	CM_EVENT_FAST_CHARGE,
	CM_EVENT_INT,
	CM_EVENT_BATT_OVERVOLTAGE,
	CM_EVENT_IGNORE_HARD_RESET,
	CM_EVENT_BATT_AGING,
	CM_EVENT_OTHERS,
};

enum cm_capacity_cmd {
	CM_CAPACITY = 0,
	CM_BOOT_CAPACITY,
	CM_RAW_CAPACITY,
};

enum cm_ir_comp_state {
	CM_IR_COMP_STATE_UNKNOWN,
	CM_IR_COMP_STATE_NORMAL,
	CM_IR_COMP_STATE_CP,
};

enum cm_cp_state {
	CM_CP_STATE_UNKNOWN,
	CM_CP_STATE_RECOVERY,
	CM_CP_STATE_ENTRY,
	CM_CP_STATE_CHECK_VBUS,
	CM_CP_STATE_TUNE,
	CM_CP_STATE_EXIT,
};

enum cm_charge_status {
	CM_CHARGE_TEMP_OVERHEAT = BIT(0),
	CM_CHARGE_TEMP_COLD = BIT(1),
	CM_CHARGE_VOLTAGE_ABNORMAL = BIT(2),
	CM_CHARGE_HEALTH_ABNORMAL = BIT(3),
	CM_CHARGE_DURATION_ABNORMAL = BIT(4),
	CM_CHARGE_BATT_OVERVOLTAGE = BIT(5),
};

enum cm_charge_limit_status {
	CM_CHARGE_USB_LIMIT_CMD = BIT(0),
	CM_CHARGE_RP_LIMIT_CMD = BIT(1),
	CM_CHARGE_PD_LIMIT_CMD = BIT(2),
};

enum cm_fast_charge_command {
	CM_FAST_CHARGE_NORMAL_CMD = 1,
	CM_FAST_CHARGE_OVP_ENABLE_CMD,
	CM_FAST_CHARGE_MAX_OVP_ENABLE_CMD,
	CM_FAST_CHARGE_OVP_DISABLE_CMD,
	CM_PPS_CHARGE_ENABLE_CMD,
	CM_PPS_CHARGE_DISABLE_CMD,
	CM_POWER_PATH_ENABLE_CMD,
	CM_POWER_PATH_DISABLE_CMD,
	CM_BUCK_MAX_TERMINA_VOL,
};

enum cm_present_command {
	CM_USB_PRESENT_CMD,
	CM_BATTERY_PRESENT_CMD,
	CM_VBUS_PRESENT_CMD,
};

enum cm_temperature_command {
	CMD_BATT_TEMP_CMD,
	CM_BUS_TEMP_CMD,
	CM_DIE_TEMP_CMD,
};

enum cm_health_command {
	CM_FAULT_HEALTH_CMD,
	CM_ALARM_HEALTH_CMD,
	CM_SOFT_ALARM_HEALTH_CMD,
	CM_BUS_ERR_HEALTH_CMD,
	CM_GOOD_HEALTH_CMD = 0x7f7f7f7f,
};

enum cm_current_now_command {
	CM_IBAT_CURRENT_NOW_CMD,
	CM_IBUS_CURRENT_NOW_CMD,
};

enum power_supply_wireless_type {
	POWER_SUPPLY_WIRELESS_TYPE_UNKNOWN = 0x20,
	POWER_SUPPLY_WIRELESS_TYPE_BPP,		/* Standard wireless bpp mode */
	POWER_SUPPLY_WIRELESS_TYPE_EPP,		/* Standard wireless epp mode */
};

enum power_supply_charge_type {
	USB_CHARGE_TYPE_NORMAL = 0,		/* Charging Power <= 10W*/
	USB_CHARGE_TYPE_FAST,			/* 10W < Charging Power <= 20W */
	USB_CHARGE_TYPE_FLASH,			/* 20W < Charging Power <= 30W */
	USB_CHARGE_TYPE_TURBE,			/* 30W < Charging Power <= 50W */
	USB_CHARGE_TYPE_SUPER,			/* Charging Power > 50W */
	WIRELESS_CHARGE_TYPE_NORMAL,
	WIRELESS_CHARGE_TYPE_FAST,
	WIRELESS_CHARGE_TYPE_FLASH,
	CHARGE_MAX,
};

enum cm_charger_fault_status_mask {
	CM_CHARGER_BAT_OVP_FAULT_MASK = BIT(0),
	CM_CHARGER_BAT_OCP_FAULT_MASK = BIT(1),
	CM_CHARGER_BUS_OVP_FAULT_MASK = BIT(2),
	CM_CHARGER_BUS_OCP_FAULT_MASK = BIT(3),
	CM_CHARGER_BAT_THERM_FAULT_MASK = BIT(4),
	CM_CHARGER_BUS_THERM_FAULT_MASK = BIT(5),
	CM_CHARGER_DIE_THERM_FAULT_MASK = BIT(6),
	CM_CHARGER_BAT_OVP_ALARM_MASK = BIT(8),
	CM_CHARGER_BAT_OCP_ALARM_MASK = BIT(9),
	CM_CHARGER_BUS_OVP_ALARM_MASK = BIT(10),
	CM_CHARGER_BUS_OCP_ALARM_MASK = BIT(11),
	CM_CHARGER_BAT_THERM_ALARM_MASK = BIT(12),
	CM_CHARGER_BUS_THERM_ALARM_MASK = BIT(13),
	CM_CHARGER_DIE_THERM_ALARM_MASK = BIT(14),
	CM_CHARGER_BAT_UCP_ALARM_MASK = BIT(15),
	CM_CHARGER_BUS_ERR_LO_MASK = BIT(24),
	CM_CHARGER_BUS_ERR_HI_MASK = BIT(25),
};

enum cm_charger_fault_status_shift {
	CM_CHARGER_BAT_OVP_FAULT_SHIFT = 0,
	CM_CHARGER_BAT_OCP_FAULT_SHIFT = 1,
	CM_CHARGER_BUS_OVP_FAULT_SHIFT = 2,
	CM_CHARGER_BUS_OCP_FAULT_SHIFT = 3,
	CM_CHARGER_BAT_THERM_FAULT_SHIFT = 4,
	CM_CHARGER_BUS_THERM_FAULT_SHIFT = 5,
	CM_CHARGER_DIE_THERM_FAULT_SHIFT = 6,
	CM_CHARGER_BAT_OVP_ALARM_SHIFT = 8,
	CM_CHARGER_BAT_OCP_ALARM_SHIFT = 9,
	CM_CHARGER_BUS_OVP_ALARM_SHIFT = 10,
	CM_CHARGER_BUS_OCP_ALARM_SHIFT = 11,
	CM_CHARGER_BAT_THERM_ALARM_SHIFT = 12,
	CM_CHARGER_BUS_THERM_ALARM_SHIFT = 13,
	CM_CHARGER_DIE_THERM_ALARM_SHIFT = 14,
	CM_CHARGER_BAT_UCP_ALARM_SHIFT = 15,
	CM_CHARGER_BUS_ERR_LO_SHIFT = 24,
	CM_CHARGER_BUS_ERR_HI_SHIFT = 25,
};

/**
 * uvlo_shutdown_mode -
 * CM_SHUTDOWN_MODE_ORDERLY - if the file "/sbin/poweroff" exit, it will
 * shutdown from user layer to kernel layer depend on /sbin/poweroff.
 * you can use this mode if your system have the file /sbin/poweroff.
 *
 * CM_SHUTDOWN_MODE_KERNEL - emergency shutdown, data may not save.
 * system use this mode as default mode.
 *
 * CM_SHUTDOWN_MODE_ANDROID - set the UI cap to 0 and let android layer
 * to shutdown from android layer to kernel layer.
 * you can use this mode if you want to save data before shutdown.
 *
 */
enum uvlo_shutdown_modes {
	CM_SHUTDOWN_MODE_ORDERLY = 0,
	CM_SHUTDOWN_MODE_KERNEL,
	CM_SHUTDOWN_MODE_ANDROID,
};

#define CM_IBAT_BUFF_CNT 7

struct cm_power_supply_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	struct charger_manager *cm;
	int ONLINE;
};

struct charger_type {
	int psy_type;
	enum cm_charger_type adap_type;
};

/**
 * struct charger_sysfs_ctl_item
 * @externally_control:
 *	Set if the charger-manager cannot control charger,
 *	the charger will be maintained with disabled state.
 * @attr_g: Attribute group for the charger(regulator)
 * @attr_name: "name" sysfs entry
 * @attr_state: "state" sysfs entry
 * @attr_externally_control: "externally_control" sysfs entry
 * @attr_jeita_control: "jeita_control" sysfs entry
 * @attrs: Arrays pointing to attr_name/state/externally_control for attr_g
 */
struct charger_sysfs_ctl_item {
	/* charger never on when system is on */
	int externally_control;

	int cp_id;

	struct attribute_group attr_grp;
	struct device_attribute attr_stop_charge;
	struct device_attribute attr_externally_control;
	struct device_attribute attr_jeita_control;
	struct device_attribute attr_step_chg_control;
	struct device_attribute attr_cp_num;
	struct device_attribute attr_charge_pump_present;
	struct device_attribute attr_charge_pump_current;
	struct device_attribute attr_enable_power_path;
	struct device_attribute attr_keep_awake;
	struct device_attribute attr_support_fast_charge;
	struct device_attribute attr_support_step_chg;
        struct device_attribute attr_thermal_limit_current;
	struct device_attribute attr_unknow_type_cur_control;
	struct attribute *attrs[14];

	struct charger_manager *cm;
};

/*
 * struct cap_remap_table
 * @cnt: record the counts of battery capacity of this scope
 * @lcap: the lower boundary of the capacity scope before transfer
 * @hcap: the upper boundary of the capacity scope before transfer
 * @lb: the lower boundary of the capacity scope after transfer
 * @hb: the upper boundary of the capacity scope after transfer
*/
struct cap_remap_table {
	int cnt;
	int lcap;
	int hcap;
	int lb;
	int hb;
};

/*
 * struct cm_ir_compensation
 * @us: record the full charged battery voltage at normal condition.
 * @rc: compensation resistor value in mohm
 * @ibat_buf: record battery current
 * @us_upper_limit: limit the max battery voltage
 * @cp_upper_limit_offset: use for charge pump mode to adjust battery over
 *	voltage protection value.
 * @us_lower_limit: record the min battery voltage
 * @ir_compensation_en: enable/disable current and resistor compensation function.
 * ibat_index: record current battery current in the ibat_buf
 * @last_target_cccv: record last target cccv point;
 */
struct cm_ir_compensation {
	int us;
	int rc;
	int ibat_buf[CM_IBAT_BUFF_CNT];
	int us_upper_limit;
	int cp_upper_limit_offset;
	int us_lower_limit;
	bool ir_compensation_en;
	int ibat_index;
	int last_target_cccv;
	int ir_drop;
};

/*
 * struct cm_cp_fault_status
 * @bat_ovp_fault: record battery over voltage fault event
 * @bat_ocp_fault: record battery over current fault event
 * @bus_ovp_fault: record bus over voltage fault event
 * @bus_ocp_fault: record bus over current fault event
 * @bat_therm_fault: record battery over temperature fault event
 * @bus_therm_fault: record bus over temperature fault event
 * @die_therm_fault: record die over temperature fault event
 * @vbus_error_lo: record the bus voltage is low event
 * @vbus_error_hi: record the bus voltage is high event
 */
struct cm_cp_fault_status {
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;
	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;
	bool vbus_error_lo;
	bool vbus_error_hi;
};

/*
 * struct cm_cp_alarm_status
 * @bat_ovp_alarm: record battery over voltage alarm event
 * @bat_ocp_alarm: record battery over current alarm event
 * @bus_ovp_alarm: record bus over voltage alarm event
 * @bus_ocp_alarm: record bus over current alarm event
 * @bat_ucp_alarm: record battery under current alarm event
 * @bat_therm_alarm: record battery over temperature alarm event
 * @bus_therm_alarm: record bus over temperature alarm event
 * @die_therm_alarm: record die over temperature alarm event
 */
struct cm_cp_alarm_status {
	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;
	bool bat_ucp_alarm;
	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;
};

struct cm_charge_pump_info {
	int vbat_uV;
	int ibat_uA;
	int ibus_uA;
	int vbus_uV;

	int cp_est_ibat;

	int cp_ibus_limit;
	int cp_ibus_limit_max;
	int cp_ibus_limit_min;

	bool cp_soft_alarm_event;
	bool cp_fault_event;

	struct cm_cp_fault_status flt;
	struct cm_cp_alarm_status alm;
};

struct cm_buck_info {
	int buck_est_ibat;
	int buck_est_ibus;

	int buck_ibat_limit;
	int buck_ibus_limit;
	int buck_last_ibat_limit;
	int buck_ibat_limit_max;
	int buck_ibus_limit_max;

	int buck_cv_vol;

	int buck_start_work_temp_th;
	int buck_start_work_ibat_th;
	int buck_ibat_max_p;
	int buck_default_ibat_max;

	int buck_target_ibat;
	int buck_target_ibat_max;

	bool buck_bat_ovp_alarm;
	bool buck_bat_ovp;

	bool buck_is_inited;
	bool buck_is_running;
};

struct cm_adaptive_fchg_info {
	int adapter_max_ibus;
	int adapter_max_vbus;

	int request_vbus;
	int request_ibus;

	int last_request_vbus;

	int adjust_cnt;
};

/*
 * struct cm_charge_pump_status
 * @cp_running: record charge pumps running status
 * @check_cp_threshold: record the flag whether need to check charge pump
 *	start condition.
 * @cp_ocv_threshold: the ocv threshold of entry pps fast charge directly.
 * @recovery: record the flag whether need recover charge pump machine
 * @cp_state: record current charge pumps state
 * @cp_target_ibat: record target battery current
 * @cp_target_vbat: record target battery voltage
 * @cp_target_ibus: record target bus current
 * @cp_target_vbus: record target bus voltage
 * @cp_max_ibat: record the upper limit of  battery current
 * @cp_max_ibus: record the upper limit of  bus current
 * @adapter_max_ibus: record the max current of bus
 * @adapter_max_vbus: record the max voltage of bus
 * @vbat_uV: record the current battery voltage
 * @ibat_uA: record the current battery current
 * @ibus_uA: record the current bus current
 * @ibus_uV: record the current bus voltage
 * @tune_vbus_retry: record the retry time from vbus low to vbus high
 * @cp_taper_trigger_cnt: record the count of battery current reach taper current
 * @cp_ibat_ucp_cnt: record the count of battery current reach ucp current
 * @cp_taper_current: record the battery current threshold of exit charge pump
 * @cp_fault_event: record the fault event
 * @flt: record the all fault status
 * @alm: record the all alarm status
 */
struct cm_cp_state_machine {
	bool running;
	bool check_cp_threshold;
	bool recovery;
	int state;

	int target_ibat;
	int target_vbat;
	int target_ibus;
	int target_vbus;

	int default_max_ibat;
	int default_max_ibus;

	int vbat_uV;
	int vout_uV;
	int ibat_uA;
	int ibus_uA;
	int vbus_uV;

	int tune_vbus_retry;
	int taper_trigger_cnt;
	int ibat_ucp_cnt;
	int taper_current;
	bool state_tune_log;

	int jeita_status;
	int last_jeita_status;
	int jeita_ibat;
	int jeita_vbat;
	int step_chg_ibat;
	int step_chg_vbat;
	int cur_step_chg_ibat;
	int cur_step_chg_vbat;
	int step_down_trigger;
	bool disable_step_chg_log;

	int ir_vbat;

	int bat_temp;

	bool is_need_disable_buck;

	struct cm_charge_pump_info cp_info;
	struct cm_buck_info buck_info;
	struct cm_adaptive_fchg_info adaptive_fchg;
};

struct cm_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
	int flash_limit;
	int flash_cur;
	int wl_bpp_cur;
	int wl_bpp_limit;
	int wl_epp_cur;
	int wl_epp_limit;
};

/**
 * struct cm_charge_pump_status
 * @adapter_default_charge_vol: record default charge voltage for
 *   different charger type
 * @thm_pwr(in mw): record thermal power to limit charge current.
 * @thm_adjust_cur: record thermal current to limit charge current, which
 *   is converted from adapter_default_charge_vol and thm_adjust_cur
 * @need_calib_charge_lmt: if need to calib charge limit current by thermal.
 */
struct cm_thermal_info {
	u32 adapter_default_charge_vol;
	u32 thm_pwr;
	int thm_adjust_cur;
	bool need_calib_charge_lmt;
};

struct cm_jeita_info {
	bool jeita_changed;
	int jeita_status;
	int jeita_temperature;
	int temp_up_trigger;
	int temp_down_trigger;
};

/**
 * struct charger_desc
 * @psy_name: the name of power-supply-class for charger manager
 * @polling_mode:
 *	Determine which polling mode will be used
 * @fullbatt_vchkdrop_ms:
 * @fullbatt_vchkdrop_uV:
 *	Check voltage drop after the battery is fully charged.
 *	If it has dropped more than fullbatt_vchkdrop_uV after
 *	fullbatt_vchkdrop_ms, CM will restart charging.
 * @fullbatt_uV: voltage in microvolt
 *	If VBATT >= fullbatt_uV, it is assumed to be full.
 * @fullbatt_uA: battery current in microamp
 * @first_fullbatt_uA: battery current in microamp of first_full charged
 * @fullbatt_soc: state of Charge in %
 *	If state of Charge >= fullbatt_soc, it is assumed to be full.
 * @fullbatt_full_capacity: full capacity measure
 *	If full capacity of battery >= fullbatt_full_capacity,
 *	it is assumed to be full.
 * @constant_charge_voltage_max_uv: max battery voltage
 * @polling_interval_ms: interval in millisecond at which
 *	charger manager will monitor battery health
 * @battery_present:
 *	Specify where information for existence of battery can be obtained
 * @psy_charger_stat: the names of power-supply for chargers
 * @psy_fast_charger_stat: the names of power-supply for fast chargers
 * @psy_cp_stat: the names of power-supply for charge pumps
 * @psy_wl_charger_stat: the names of power-supply for wireless chargers
 * @psy_cp_converter_stat: the names of power-supply for charge pump converters
 * @num_charger_regulator: the number of entries in charger_regulators
 * @charger_regulators: array of charger regulators
 * @psy_fuel_gauge: the name of power-supply for fuel gauge
 * @thermal_zone : the name of thermal zone for battery
 * @temp_min : Minimum battery temperature for charging.
 * @temp_max : Maximum battery temperature for charging.
 * @temp_diff : Temperature difference to restart charging.
 * @cap : Battery capacity report to user space.
 * @measure_battery_temp:
 *	true: measure battery temperature
 *	false: measure ambient temperature
 * @charging_max_duration_ms: Maximum possible duration for charging
 *	If whole charging duration exceed 'charging_max_duration_ms',
 *	cm stop charging.
 * @discharging_max_duration_ms:
 *	Maximum possible duration for discharging with charger cable
 *	after full-batt. If discharging duration exceed 'discharging
 *	max_duration_ms', cm start charging.
 * @normal_charge_voltage_max:
 *	maximum normal charge voltage in microVolts
 * @normal_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart normal charging
 * @fast_charge_voltage_max:
 *	maximum fast charge voltage in microVolts
 * @fast_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart fast charging
 * @flash_charge_voltage_max:
 *	maximum flash charge voltage in microVolts
 * @flash_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart flash charging
 * @wireless_normal_charge_voltage_max:
 *	maximum wireless normal charge voltage in microVolts
 * @wireless_normal_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart wireless charging
 * @wireless_fast_charge_voltage_max:
 *	maximum wireless fast charge voltage in microVolts
 * @wireless_fast_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart wireless fast charging
 * @charger_status: Recording state of charge
 * @charger_type: Recording type of charge
 * @first_trigger_cnt: The number of times the battery is first_fully charged
 * @trigger_cnt: The number of times the battery is fully charged
 * @uvlo_trigger_cnt: The number of times the battery voltage is
 *	less than under voltage lock out
 * @uvlo_shutdown_mode:
 *	Determine which polling mode will be used
 * @cap_one_time: The percentage of electricity is not
 *	allowed to change by 1% in cm->desc->cap_one_time
 * @trickle_time_out: If 99% lasts longer than it , will force set full statu
 * @trickle_time: Record the charging time when battery
 *	capacity is larger than 99%.
 * @trickle_start_time: Record current time when battery capacity is 99%
 * @update_capacity_time: Record the battery capacity update time
 * @last_query_time: Record last time enter cm_batt_works
 * @force_set_full: The flag is indicate whether
 *	there is a mandatory setting of full status
 * @shutdown_voltage: If it has dropped more than shutdown_voltage,
 *	the phone will automatically shut down
 * @wdt_interval: Watch dog time pre-load value
 * @jeita_tab: Specify the jeita temperature table, which is used to
 *	adjust the charging current according to the battery temperature.
 * @jeita_tab_size: Specify the size of jeita temperature table.
 * @jeita_tab_array: Specify the jeita temperature table array, which is used to
 *	save the point of adjust the charging current according to the battery temperature.
 * @jeita_disabled: disable jeita function when needs
 * @force_jeita_status: force jeita to this status when disable jeita
 * @temperature: the battery temperature
 * @internal_resist: the battery internal resistance in mOhm
 * @cap_table_len: the length of ocv-capacity table
 * @cap_table: capacity table with corresponding ocv
 * @cap_remap_table: the table record the different scope of capacity
 *	information.
 * @cap_remap_table_len: the length of cap_remap_table
 * @cap_remap_total_cnt: the total count the whole battery capacity is divided
	into.
 * @is_fast_charge: if it is support fast charge or not
 * @enable_fast_charge: if is it start fast charge or not
 * @fast_charge_enable_count: to count the number that satisfy start
 *	fast charge condition.
 * @fast_charge_disable_count: to count the number that satisfy stop
 *	fast charge condition.
 * @double_IC_total_limit_current: if it use two charge IC to support
 *	fast charge, we use total limit current to campare with thermal_val,
 *	to limit the thermal_val under total limit current.
 * @cm_check_int: record the intterupt event
 * @cm_check_fault: record the flag whether need to check fault status
 * @fast_charger_type: record the charge type
 * @cp: record the charge pump status
 * @ir_comp: record the current and resistor compensation status
 * @wl_charge_en: if it is wireless charge enabled or not
 * @usb_charge_en: if it is usb charge enabled or not
 * @adapter_default_charge_vol(v): adapter charge voltage for thermal to calculate
 *     input limit current
 */
struct charger_desc {
	const char *psy_name;

	enum polling_modes polling_mode;
	unsigned int polling_interval_ms;

	unsigned int fullbatt_vchkdrop_ms;
	unsigned int fullbatt_vchkdrop_uV;
	unsigned int fullbatt_uV;
	unsigned int fullbatt_uA;
	unsigned int first_fullbatt_uA;
	unsigned int fullbatt_soc;
	unsigned int fullbatt_full_capacity;
	unsigned int constant_charge_voltage_max_uv;
	unsigned int fullbatt_voltage_offset_uv;
	unsigned int origin_fullbatt_uV;
	unsigned int origin_fullbatt_uA;
	unsigned int charge_term_ua;
	unsigned int jeita_charge_term_ma;

	enum data_source battery_present;

	const char **psy_alt_charger_adpt_stat;
	const char **psy_charger_stat;
	const char **psy_alt_cp_adpt_stat;
	const char **psy_cp_stat;
	const char **psy_wl_charger_stat;
	const char **psy_cp_converter_stat;

	int num_sysfs;
	struct charger_sysfs_ctl_item *sysfs;
	const struct attribute_group **sysfs_groups;

	const char *psy_fuel_gauge;

	const char *thermal_zone;

	int temp_min;
	int temp_max;
	int temp_diff;

	int cap;
	int raw_cap;
	bool measure_battery_temp;
	bool keep_awake;

	u32 charging_max_duration_ms;
	u32 discharging_max_duration_ms;

	u32 charge_voltage_max;
	u32 charge_voltage_drop;
	u32 normal_charge_voltage_max;
	u32 normal_charge_voltage_drop;
	u32 fast_charge_voltage_max;
	u32 fast_charge_voltage_drop;
	u32 flash_charge_voltage_max;
	u32 flash_charge_voltage_drop;
	u32 wireless_normal_charge_voltage_max;
	u32 wireless_normal_charge_voltage_drop;
	u32 wireless_fast_charge_voltage_max;
	u32 wireless_fast_charge_voltage_drop;

	int charger_status;
	u32 charger_type;
	int trigger_cnt;
	int first_trigger_cnt;
	int uvlo_trigger_cnt;
	enum uvlo_shutdown_modes uvlo_shutdown_mode;

	u32 cap_one_time;
	u32 default_cap_one_time;

	u32 trickle_time_out;
	u64 trickle_time;
	u64 trickle_start_time;

	u64 update_capacity_time;
	u64 last_query_time;

	bool force_set_full;
	u32 shutdown_voltage;

	u32 wdt_interval;

	int charge_limit_cur;
	int input_limit_cur;

	int usb_limit_cur;

	int thm_adjust_cur;

	struct sprd_battery_jeita_table *jeita_tab_array[SPRD_BATTERY_JEITA_MAX];
	u32 jeita_size[SPRD_BATTERY_JEITA_MAX];
	u32 max_current_jeita_index[SPRD_BATTERY_JEITA_MAX];
	struct sprd_battery_jeita_table *jeita_tab;
	u32 jeita_tab_size;
	int force_jeita_status;
	bool jeita_disabled;
	struct cm_jeita_info jeita_info;

	struct sprd_battery_step_chg_table *step_chg_table;
	u32 step_chg_table_size;
	bool support_step_chg;
	bool step_chg_disabled;

	int temperature;
	int internal_resist;
	int cap_table_len;
	struct power_supply_battery_ocv_table *cap_table;
	struct cap_remap_table *cap_remap_table;
	int cap_remap_table_len;
	int cap_remap_total_cnt;
	bool is_fast_charge;
	bool enable_fast_charge;
	bool fixed_fchg_running;
	bool wait_vbus_stable;
	bool check_fixed_fchg_threshold;
	bool force_pps_diasbled;
	bool support_fixed_fchg;
	bool support_adaptive_fchg;
	u32 fchg_voltage_check_count;
	u32 fast_charge_enable_count;
	u32 fast_charge_disable_count;
	u32 cp_nums;
	u32 alt_cp_nums;
	bool enable_alt_cp_adapt;

	u32 alt_charger_nums;
	bool enable_alt_charger_adapt;

	bool cm_check_int;
	bool cm_check_fault;
	u32 fast_charger_type;

	int fchg_ocv_threshold;

	struct cm_cp_state_machine cp_sm;
	struct cm_ir_compensation ir_comp;

	struct cm_charge_current cur;

	bool wl_charge_en;
	bool usb_charge_en;

	struct cm_thermal_info thm_info;

	struct mutex charger_type_mtx;
	struct mutex charge_info_mtx;
	struct mutex keep_awake_mtx;

	bool xts_limit_cur;
	int adapter_max_vbus;

	int thermal_limit_ibat;
	int thermal_limit_level;

	bool pd_enable_limit;
	bool pd_negotiated_limit_cur;
	int pd_req_vol_uv;
	int pd_req_cur_ua;
	int rp_limit_current;
	int limit_status;
	u32 pd_port_partner;
	u32 charge_type_poll_count;

	bool support_cp_buck_work_tgt;
  	u32 dcd_work_count;
};

#define PSY_NAME_MAX	30

/**
 * struct charger_manager
 * @entry: entry for list
 * @dev: device pointer
 * @desc: instance of charger_desc
 * @fuel_gauge: power_supply for fuel gauge
 * @charger_stat: array of power_supply for chargers
 * @tzd_batt : thermal zone device for battery
 * @charger_enabled: the state of charger
 * @fullbatt_vchk_jiffies_at:
 *	jiffies at the time full battery check will occur.
 * @fullbatt_vchk_work: work queue for full battery check
 * @uvlo_work: work queue to check uvlo state
 * @ir_compensation_work: work queue to check current and resistor
 *	compensation state
 * @emergency_stop:
 *	When setting true, stop charging
 * @psy_name_buf: the name of power-supply-class for charger manager
 * @charger_psy: power_supply for charger manager
 * @status_save_ext_pwr_inserted:
 *	saved status of external power before entering suspend-to-RAM
 * @status_save_batt:
 *	saved status of battery before entering suspend-to-RAM
 * @charging_start_time: saved start time of enabling charging
 * @charging_end_time: saved end time of disabling charging
 * @charging_status: saved charging status, 0 means charging normal
 * @battery_status: Current battery status
 * @charge_ws: wakeup source to prevent ap enter sleep mode in charge
 *	pump mode
 * @quick_charge_type_update_work: delay work for update the quick charge type
 * @quick_charge_type_poll_count: the cycle count of update quick charge type
 */
struct charger_manager {
	struct list_head entry;
	struct device *dev;
	struct charger_desc *desc;

#if IS_ENABLED(CONFIG_THERMAL)
	struct thermal_zone_device *tzd_batt;
#endif
	bool charger_enabled;
	bool shutdown_flag;
	bool shipmode_flag;
	bool cid_enable;
	bool screen_on;
	bool audio_on;

	unsigned long fullbatt_vchk_jiffies_at;
	struct delayed_work fullbatt_vchk_work;
	struct delayed_work cap_update_work;
	struct delayed_work uvlo_work;
	struct delayed_work ir_compensation_work;
	struct delayed_work fixed_fchg_work;
	struct delayed_work cp_work;
	struct delayed_work charger_type_update_work;
	struct delayed_work limit_current_work;
	struct delayed_work cid_detect_work;
	struct delayed_work shutdown_delay_work;
	struct delayed_work quick_charge_type_update_work;
	int emergency_stop;

	char psy_name_buf[PSY_NAME_MAX + 1];
	struct power_supply_desc charger_psy_desc;
	struct power_supply *charger_psy;
	struct power_supply *bms_psy;

	u32 quick_charge_type_poll_count;
	u64 charging_start_time;
	u64 charging_end_time;
	u32 charging_status;
	int battery_status;

	/* Charge power detect */
	struct delayed_work power_detect_work;

	int usb_present;

	/* Charge adjust dpm */
	struct delayed_work adjust_dpm_work;
	int vindpm_voltage_mv;

	bool usb_conndoned;
	struct delayed_work dcd_work;

	struct wakeup_source *charge_ws;
	struct wakeup_source *cp_ws;
	struct sprd_vote *cm_charge_vote;
	struct sprd_vchg_info *vchg_info;
	struct sprd_fchg_info *fchg_info;
	struct charger_dev *charger;

	struct iio_channel *battery_chan;
	int bat_id;
	int bat_id_adc_mv;
	int fake_capacity;
	int quick_charge_type;
	int otg_debug;

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	int low_fast_current;
	int nig_stop_flag;
	int nav_stop_flag;
	int pro_stop_flag;
	int outdoor_flag;
	int smart_fv;
	int cycle_fv;
#endif
	bool input_suspend;
	bool shutdown_delay;
	bool uisoc_100_adapter_plugin;
	int mtbf_current;
	int fake_charge_cycle;
	int erp_config;
	int erp_full_flag;
	int board_temp;
	bool enable_pfm;
};

extern int typec_get_cc_polarity(void);
extern bool typec_get_power_role(void);
extern int sc27xx_get_current_status_detach_or_attach(void);
extern int sc27xx_typec_set_mode(int mode);
extern int sc27xx_get_typec_mode(void);
extern bool sc27xx_get_try_sink_flag(void);

extern int sprd_hsphy_set_high_impedance_state(void);
extern int sprd_hsphy_cancel_high_impedance_state(void);

#if IS_ENABLED(CONFIG_CHARGER_MANAGER)
extern void cm_notify_event(struct power_supply *psy,
				enum cm_event_types type, char *msg);
#else
static inline void cm_notify_event(struct power_supply *psy,
				enum cm_event_types type, char *msg) { }
#endif
#endif /* _CHARGER_MANAGER_H */
