/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef BATTERY_COMMON_H
#define BATTERY_COMMON_H

#include "mt_charging.h"
#include <linux/ioctl.h>
#include <linux/types.h>

/*****************************************************************************
 *  BATTERY VOLTAGE
 ****************************************************************************/
#define PRE_CHARGE_VOLTAGE 3200
#define SYSTEM_OFF_VOLTAGE 3400
#define CONSTANT_CURRENT_CHARGE_VOLTAGE 4100
#define CONSTANT_VOLTAGE_CHARGE_VOLTAGE 4200
#define CV_DROPDOWN_VOLTAGE 4000
#define CHARGER_THRESH_HOLD 4300
#define BATTERY_UVLO_VOLTAGE 2700

/*****************************************************************************
 *  BATTERY TIMER
 ****************************************************************************/
#define MAX_CHARGING_TIME (12 * 60 * 60)       /* 12hr */
#define MAX_POSTFULL_SAFETY_TIME (1 * 30 * 60) /* 30mins */
#define MAX_PreCC_CHARGING_TIME (1 * 30 * 60)  /* 0.5hr */
#define MAX_CV_CHARGING_TIME (3 * 60 * 60)     /* 3hr */
#define MUTEX_TIMEOUT 5000
#define BAT_TASK_PERIOD 10   /* 10sec */
#define g_free_bat_temp 1000 /* 1 s */

/*****************************************************************************
 *  BATTERY Protection
 ****************************************************************************/
#define Battery_Percent_100 100
#define charger_OVER_VOL 1
#define BATTERY_UNDER_VOL 2
#define BATTERY_OVER_TEMP 3
#define ADC_SAMPLE_TIMES 5

/*****************************************************************************
 *  Pulse Charging State
 ****************************************************************************/
#define CHR_PRE 0x1000
#define CHR_CC 0x1001
#define CHR_TOP_OFF 0x1002
#define CHR_POST_FULL 0x1003
#define CHR_BATFULL 0x1004
#define CHR_ERROR 0x1005
#define CHR_HOLD 0x1006
#define CHR_CMD_HOLD 0x1007

/*****************************************************************************
 *  Call State
 ****************************************************************************/
#define CALL_IDLE 0
#define CALL_ACTIVE 1

/*****************************************************************************
 *  Enum
 ****************************************************************************/

enum PMU_STATUS {
	PMU_STATUS_OK = 0,
	PMU_STATUS_FAIL = 1,
};

enum temp_state_enum {
	TEMP_BELOW_NEG_10 = 0,
	TEMP_NEG_10_TO_POS_0,
	TEMP_POS_0_TO_POS_10,
	TEMP_POS_10_TO_POS_45,
	TEMP_POS_45_TO_POS_60,
	TEMP_ABOVE_POS_60
};

/*****************************************************************************
 *  structure
 ****************************************************************************/
struct PMU_ChargerStruct {
	bool bat_exist;
	bool bat_full;
	s32 bat_charging_state;
	u32 bat_vol;
	bool bat_in_recharging_state;
	u32 Vsense;
	bool charger_exist;
	u32 charger_vol;
	s32 charger_protect_status;
	s32 ICharging;
	s32 IBattery;
	s32 temperature;
	s32 temperatureR;
	s32 temperatureV;
	u32 total_charging_time;
	u32 PRE_charging_time;
	u32 CC_charging_time;
	u32 TOPOFF_charging_time;
	u32 POSTFULL_charging_time;
	u32 charger_type;
	s32 SOC;
	s32 UI_SOC;
	u32 nPercent_ZCV;
	u32 nPrecent_UI_SOC_check_point;
	u32 ZCV;
};

struct mt_battery_charging_custom_data;

struct battery_common_data {
	int irq;
	bool common_init_done : 1;
	bool init_done : 1;
	bool down : 1;
	bool usb_host_mode : 1;
	bool usb_connect_ready : 1;
	CHARGING_CONTROL charger;
};

/*****************************************************************************
 *  Extern Variable
 ****************************************************************************/

extern struct battery_common_data g_bat;

extern struct PMU_ChargerStruct BMT_status;
extern bool g_ftm_battery_flag;
extern int charging_level_data[1];
extern bool g_call_state;
extern bool g_cmd_hold_charging;
extern bool g_charging_full_reset_bat_meter;
extern struct mt_battery_charging_custom_data *p_bat_charging_data;
extern int g_platform_boot_mode;
extern s32 g_custom_charging_current;
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
extern int g_temp_status;
#endif
extern bool g_bat_init_flag;

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
extern struct wake_lock TA_charger_suspend_lock;
extern bool ta_check_chr_type;
extern bool ta_cable_out_occur;
extern bool is_ta_connect;
extern bool ta_vchr_tuning;
extern int ta_v_chr_org;
#endif

static inline int get_bat_average_voltage(void)
{
	return BMT_status.bat_vol;
}

static inline void
bat_charger_init(struct mt_battery_charging_custom_data *pdata)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_INIT, pdata);
}

static inline void bat_charger_set_cv_voltage(int cv_voltage)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);
}

static inline void bat_charger_enable(bool enable)
{
	u32 charging_enable = enable;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_ENABLE, &charging_enable);
}

static inline void bat_charger_enable_power_path(bool enable)
{
	u32 charging_enable = enable;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_ENABLE_POWERPATH, &charging_enable);
}

static inline bool bat_charger_is_pcm_timer_trigger(void)
{
	bool is_pcm_timer_trigger = false;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER,
			      &is_pcm_timer_trigger);

	return is_pcm_timer_trigger != 0;
}

static inline void bat_charger_reset_watchdog_timer(void)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);
}

static inline void bat_charger_dump_register(void)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_DUMP_REGISTER, NULL);
}

static inline int bat_charger_get_platform_boot_mode(void)
{
	int val = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_PLATFORM_BOOT_MODE, &val);

	return val;
}

static inline int bat_charger_get_charger_type(void)
{
	int chr_type = CHARGER_UNKNOWN;

	if (g_bat.usb_connect_ready && g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_CHARGER_TYPE, &chr_type);
	else {
#if defined(CONFIG_POWER_EXT)
		chr_type = STANDARD_HOST;
#endif
	}
	return chr_type;
}

static inline void bat_charger_set_platform_reset(void)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_PLATFORM_RESET, NULL);
}

static inline int bat_charger_get_battery_status(void)
{
	int battery_status = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_BATTERY_STATUS, &battery_status);

	return battery_status;
}

static inline void bat_charger_set_hv_threshold(u32 hv_voltage)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_HV_THRESHOLD, &hv_voltage);
}

static inline bool bat_charger_get_hv_status(void)
{
	bool hv_status = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_HV_STATUS, &hv_status);

	return hv_status != 0;
}

static inline int bat_charger_get_charging_current(void)
{
	int charging_current = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_CURRENT, &charging_current);

	return charging_current;
}

static inline int bat_charger_get_input_current(void)
{
	int input_current = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_INPUT_CURRENT, &input_current);
	return input_current;
}

static inline bool bat_charger_get_detect_status(void)
{
	bool chr_status = false;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_CHARGER_DET_STATUS, &chr_status);

	return chr_status != 0;
}

static inline bool bat_charger_get_charging_status(void)
{
	u32 status = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_CHARGING_STATUS, &status);

	return status != 0;
}

static inline void bat_charger_set_input_current(int val)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_INPUT_CURRENT, &val);
}

static inline void bat_charger_set_current(int val)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_CURRENT, &val);
}

static inline void bat_charger_boost_enable(bool enable)
{
	u32 boost_enable = enable;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_BOOST_ENABLE, &boost_enable);
}

static inline void bat_charger_set_ta_pattern(bool enable)
{
	u32 ta_enable = enable;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_TA_CURRENT_PATTERN, &ta_enable);
}

/*****************************************************************************
 *  Extern Function
 ****************************************************************************/
extern int read_tbat_value(void);
extern bool pmic_chrdet_status(void);
extern bool bat_is_charger_exist(void);
extern bool bat_is_charging_full(void);
extern u32 bat_get_ui_percentage(void);
extern u32 get_charging_setting_current(void);
extern u32 bat_is_recharging_phase(void);
extern u32 set_bat_charging_current_limit(int limit);

extern void mt_power_off(void);
extern bool mt_usb_is_device(void);
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
extern bool mt_usb_is_ready(void);
extern int set_rtc_spare_fg_value(int val);
extern int get_rtc_spare_fg_value(void);

extern bool mt_usb_is_device(void);
#if defined(CONFIG_USB_MTK_HDRC) || defined(CONFIG_SSUSB_DRV)
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
#else
#define mt_usb_connect()                                                       \
	do {                                                                   \
	} while (0)
#define mt_usb_disconnect()                                                    \
	do {                                                                   \
	} while (0)
#endif

#ifdef CONFIG_MTK_SMART_BATTERY
extern void wake_up_bat(void);
extern unsigned long BAT_Get_Battery_Voltage(int polling_mode);
extern void mt_battery_charging_algorithm(void);
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
extern int do_jeita_state_machine(void);
#endif

#else

#define wake_up_bat()                                                          \
	do {                                                                   \
	} while (0)
#define BAT_Get_Battery_Voltage(polling_mode) ({ 0; })

#endif

#endif /* #ifndef BATTERY_COMMON_H */
