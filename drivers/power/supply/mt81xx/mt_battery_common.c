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

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/fs.h>
#include <linux/fs.h>
#include <linux/init.h> /* For init/exit macros */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h> /* For MODULE_ marcros  */
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <linux/io.h>
#include <asm/irq.h>
#include <linux/uaccess.h>

#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>

#include "mt_battery_common.h"
#include "mt_battery_custom_data.h"
#include "mt_battery_meter.h"
#include "mt_charging.h"
#include <linux/irq.h>
#include <linux/reboot.h>

struct battery_common_data g_bat;

/* Battery Notify */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP
/* #define BATTERY_NOTIFY_CASE_0003_ICHARGING */
#define BATTERY_NOTIFY_CASE_0004_VBAT
#define BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME

/* Precise Tunning */
#define BATTERY_AVERAGE_DATA_NUMBER 3
#define BATTERY_AVERAGE_SIZE 30

/* ///////////////////////////////////////////////////////////////////////////
 *     Smart Battery Structure
 * ///////////////////////////////////////////////////////////////////////////
 */
struct PMU_ChargerStruct BMT_status;

/* ///////////////////////////////////////////////////////////////////////////
 *     Thermal related flags
 * ///////////////////////////////////////////////////////////////////////////
 */
/* 0:nothing,
 * 1:enable batTT&chrTimer,
 * 2:disable batTT&chrTimer,
 * 3:enable batTT, disable chrTimer
 */
int g_battery_thermal_throttling_flag = 3;
int battery_cmd_thermal_test_mode;
int battery_cmd_thermal_test_mode_value;

/*
 * 0:default enable check batteryTT,
 * 1:default disable check batteryTT
 */
int g_battery_tt_check_flag;

/* ///////////////////////////////////////////////////////////////////////////
 *     Global Variable
 * ///////////////////////////////////////////////////////////////////////////
 */

#ifdef CONFIG_OF
static const struct of_device_id mt_battery_common_id[] = {
	{.compatible = "mediatek,battery_common"}, {},
};

MODULE_DEVICE_TABLE(of, mt_battery_common_id);
#endif

struct wakeup_source battery_suspend_lock;
unsigned int g_BatteryNotifyCode;
unsigned int g_BN_TestMode;
bool g_bat_init_flag;
bool g_call_state = CALL_IDLE;
bool g_charging_full_reset_bat_meter;
int g_platform_boot_mode;
static bool battery_meter_initilized;
bool g_cmd_hold_charging;
s32 g_custom_charging_current = -1;
bool battery_suspended;
bool g_refresh_ui_soc;
static bool fg_battery_shutdown;
struct mt_battery_charging_custom_data *p_bat_charging_data;

static struct mt_battery_charging_custom_data default_charging_data = {

	.talking_recharge_voltage = 3800,
	.talking_sync_time = 60,

	/* Battery Temperature Protection */
	.max_discharge_temperature = 60,
	.min_discharge_temperature = -10,
	.max_charge_temperature = 50,
	.min_charge_temperature = 0,
	.err_charge_temperature = 0xFF,
	.use_avg_temperature = 1,

	/* Linear Charging Threshold */
	.v_pre2cc_thres = 3400, /* mV */
	.v_cc2topoff_thres = 4050,
	.recharging_voltage = 4110,
	.charging_full_current = 150, /* mA */

	/* CONFIG_USB_IF */
	.usb_charger_current_suspend = 0,
	.usb_charger_current_unconfigured = CHARGE_CURRENT_70_00_MA,
	.usb_charger_current_configured = CHARGE_CURRENT_500_00_MA,

	.usb_charger_current = CHARGE_CURRENT_500_00_MA,
	.ac_charger_current = 204800,
	.ac_charger_input_current = 180000,
	.non_std_ac_charger_current = CHARGE_CURRENT_500_00_MA,
	.charging_host_charger_current = CHARGE_CURRENT_650_00_MA,
	.apple_0_5a_charger_current = CHARGE_CURRENT_500_00_MA,
	.apple_1_0a_charger_current = CHARGE_CURRENT_1000_00_MA,
	.apple_2_1a_charger_current = CHARGE_CURRENT_2000_00_MA,

	/* Charger error check */
	/* BAT_LOW_TEMP_PROTECT_ENABLE */
	.v_charger_enable = 0, /* 1:ON , 0:OFF */
	.v_charger_max = 6500, /* 6.5 V */
	.v_charger_min = 4400, /* 4.4 V */
	.battery_cv_voltage = BATTERY_VOLT_04_200000_V,

	/* Tracking time */
	.onehundred_percent_tracking_time = 10, /* 10 second */
	.npercent_tracking_time = 20,		/* 20 second */
	.sync_to_real_tracking_time = 30,       /* 30 second */

	/* JEITA parameter */
	.cust_soc_jeita_sync_time = 30,
	.jeita_recharge_voltage = 4110, /* for linear charging */
	.jeita_temp_above_pos_60_cv_voltage = BATTERY_VOLT_04_000000_V,
	.jeita_temp_pos_45_to_pos_60_cv_voltage = BATTERY_VOLT_04_000000_V,
	.jeita_temp_pos_10_to_pos_45_cv_voltage = BATTERY_VOLT_04_200000_V,
	.jeita_temp_pos_0_to_pos_10_cv_voltage = BATTERY_VOLT_04_000000_V,
	.jeita_temp_neg_10_to_pos_0_cv_voltage = BATTERY_VOLT_04_000000_V,
	.jeita_temp_below_neg_10_cv_voltage = BATTERY_VOLT_04_000000_V,
	.temp_pos_60_threshold = 50,
	.temp_pos_60_thres_minus_x_degree = 47,
	.temp_pos_45_threshold = 45,
	.temp_pos_45_thres_minus_x_degree = 39,
	.temp_pos_10_threshold = 10,
	.temp_pos_10_thres_plus_x_degree = 16,
	.temp_pos_0_threshold = 0,
	.temp_pos_0_thres_plus_x_degree = 6,
	.temp_neg_10_threshold = 0,
	.temp_neg_10_thres_plus_x_degree = 0,

	/* For JEITA Linear Charging Only */
	.jeita_neg_10_to_pos_0_full_current = 120, /* mA */
	.jeita_temp_pos_45_to_pos_60_recharge_voltage = 4000,
	.jeita_temp_pos_10_to_pos_45_recharge_voltage = 4100,
	.jeita_temp_pos_0_to_pos_10_recharge_voltage = 4000,
	.jeita_temp_neg_10_to_pos_0_recharge_voltage = 3800,
	.jeita_temp_pos_45_to_pos_60_cc2topoff_threshold = 4050,
	.jeita_temp_pos_10_to_pos_45_cc2topoff_threshold = 4050,
	.jeita_temp_pos_0_to_pos_10_cc2topoff_threshold = 4050,
	.jeita_temp_neg_10_to_pos_0_cc2topoff_threshold = 3850,

	/* For Pump Express Plus */
	.ta_start_battery_soc = 1,
	.ta_stop_battery_soc = 95,
	.ta_ac_9v_input_current = CHARGE_CURRENT_1500_00_MA,
	.ta_ac_7v_input_current = CHARGE_CURRENT_1500_00_MA,
	.ta_ac_charging_current = CHARGE_CURRENT_2200_00_MA,
	.ta_9v_support = 1,
};

/* //////////////////////////////////////////////////////////
 *    Integrate with NVRAM
 * //////////////////////////////////////////////////////////
 */
#define ADC_CALI_DEVNAME "MT_pmic_adc_cali"
#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal _IOW('k', 3, int)
#define ADC_CHANNEL_READ _IOW('k', 4, int)
#define BAT_STATUS_READ _IOW('k', 5, int)
#define Set_Charger_Current _IOW('k', 6, int)
/* add for meta tool----------------------------------------- */
#define Get_META_BAT_VOL _IOW('k', 10, int)
#define Get_META_BAT_SOC _IOW('k', 11, int)
/* add for meta tool----------------------------------------- */

static struct class *adc_cali_class;
static int adc_cali_major;
static dev_t adc_cali_devno;
static struct cdev *adc_cali_cdev;

int adc_cali_slop[14] = {1000, 1000, 1000, 1000, 1000, 1000, 1000,
	1000, 1000, 1000, 1000, 1000, 1000, 1000};
int adc_cali_offset[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int adc_cali_cal[1] = {0};
int battery_in_data[1] = {0};
int battery_out_data[1] = {0};
int charging_level_data[1] = {0};

bool g_ADC_Cali;
bool g_ftm_battery_flag;
static bool need_clear_current_window;

/* ////////////////////////////////////////////////////////////
 *     Thread related
 * ////////////////////////////////////////////////////////////
 */
#define BAT_MS_TO_NS(x) (x * 1000 * 1000)
static atomic_t bat_thread_wakeup;
static bool chr_wake_up_bat; /* charger in/out to wake up battery thread */
static bool bat_meter_timeout;
static DEFINE_MUTEX(bat_mutex);
static DEFINE_MUTEX(charger_type_mutex);
static DECLARE_WAIT_QUEUE_HEAD(bat_thread_wq);

/* ////////////////////////////////////////////////////////////
 * FOR ANDROID BATTERY SERVICE
 * ///////////////////////////////////////////////////////////
 */
struct ac_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int AC_ONLINE;
};

struct usb_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int USB_ONLINE;
};

struct battery_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int BAT_STATUS;
	int BAT_HEALTH;
	int BAT_PRESENT;
	int BAT_TECHNOLOGY;
	int BAT_CAPACITY;
	/* Add for Battery Service */
	int BAT_VOLTAGE_NOW;
	int BAT_VOLTAGE_AVG;
	int BAT_TEMP;
	/* Add for EM */
	int BAT_TemperatureR;
	int BAT_TempBattVoltage;
	int BAT_InstatVolt;
	int BAT_BatteryAverageCurrent;
	int BAT_BatterySenseVoltage;
	int BAT_ISenseVoltage;
	int BAT_ChargerVoltage;
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	/* Add for Battery Service */
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_TEMP,
	/* Add for EM
	 * POWER_SUPPLY_PROP_TemperatureR,
	 * POWER_SUPPLY_PROP_TempBattVoltage,
	 * POWER_SUPPLY_PROP_InstatVolt,
	 * POWER_SUPPLY_PROP_BatteryAverageCurrent,
	 * POWER_SUPPLY_PROP_BatterySenseVoltage,
	 * POWER_SUPPLY_PROP_ISenseVoltage,
	 * POWER_SUPPLY_PROP_ChargerVoltage,
	 */
};

int read_tbat_value(void)
{
	return BMT_status.temperature;
}

/* ////////////////////////////////////////////////////////////////
 *    PMIC PCHR Related APIs
 * ////////////////////////////////////////////////////////////////
 */
__attribute__((weak)) bool mt_usb_pd_support(void)
{
	return false;
}
__attribute__((weak)) bool mt_is_power_sink(void)
{
	return true;
}
__attribute__((weak)) bool mt_usb_is_ready(void)
{
	return true;
}

bool upmu_is_chr_det(void)
{
#if defined(CONFIG_POWER_EXT)
	return upmu_get_rgs_chrdet();
#else
	u32 tmp32;

	tmp32 = bat_charger_get_detect_status();

#if defined(CONFIG_ANALOGIX_OHIO) || defined(CONFIG_TYPE_C_FUSB302)
	if (tmp32 == 0 && !(battery_meter_get_charger_voltage() >= 4300))
		return false;
#else
	if (tmp32 == 0)
		return false;
#endif

	if (mt_usb_pd_support()) {
		pr_debug("[upmu_is_chr_det] usb device mode(%d). power role(%d)\n",
			mt_usb_is_device(), mt_is_power_sink());
		if (mt_is_power_sink())
			return true;
		else
			return false;
	} else if (mt_usb_is_device()) {
		pr_debug("[upmu_is_chr_det] Charger exist and USB is not host\n");
		return true;
	}

	pr_debug("[upmu_is_chr_det] Charger exist but USB is host\n");
	return false;

#endif
}
EXPORT_SYMBOL(upmu_is_chr_det);

static inline void _do_wake_up_bat_thread(void)
{
	atomic_inc(&bat_thread_wakeup);
	wake_up(&bat_thread_wq);
}

/* for charger plug-in/out */
void wake_up_bat(void)
{
	pr_debug("%s:\n", __func__);
	chr_wake_up_bat = true;
	_do_wake_up_bat_thread();
}
EXPORT_SYMBOL(wake_up_bat);

/* for meter update */
static void wake_up_bat_update_meter(void)
{
	pr_debug("%s:\n", __func__);
	bat_meter_timeout = true;
	_do_wake_up_bat_thread();
}

static int ac_get_property(struct power_supply *psy,
			   enum power_supply_property psp,
			   union power_supply_propval *val)
{
	int ret = 0;
	struct ac_data *data = container_of(psy->desc, struct ac_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->AC_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int usb_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	struct usb_data *data = container_of(psy->desc, struct usb_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->USB_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int battery_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int ret = 0;
	struct battery_data *data =
		container_of(psy->desc, struct battery_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->BAT_STATUS;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->BAT_HEALTH;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->BAT_PRESENT;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = data->BAT_TECHNOLOGY;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->BAT_CAPACITY;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->BAT_VOLTAGE_NOW;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->BAT_TEMP;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = data->BAT_VOLTAGE_AVG;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* ac_data initialization */
static struct ac_data ac_main = {
	.psd = {

			.name = "ac",
			.type = POWER_SUPPLY_TYPE_MAINS,
			.properties = ac_props,
			.num_properties = ARRAY_SIZE(ac_props),
			.get_property = ac_get_property,
		},
	.AC_ONLINE = 0,
};

/* usb_data initialization */
static struct usb_data usb_main = {
	.psd = {

			.name = "usb",
			.type = POWER_SUPPLY_TYPE_USB,
			.properties = usb_props,
			.num_properties = ARRAY_SIZE(usb_props),
			.get_property = usb_get_property,
		},
	.USB_ONLINE = 0,
};

/* battery_data initialization */
static struct battery_data battery_main = {
	.psd = {

			.name = "battery",
			.type = POWER_SUPPLY_TYPE_BATTERY,
			.properties = battery_props,
			.num_properties = ARRAY_SIZE(battery_props),
			.get_property = battery_get_property,
		},
/* CC: modify to have a full power supply status */
#if defined(CONFIG_POWER_EXT)
	.BAT_STATUS = POWER_SUPPLY_STATUS_FULL,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = 100,
	.BAT_VOLTAGE_NOW = 4200000,
	.BAT_VOLTAGE_AVG = 4200000,
	.BAT_TEMP = 22,
#else
	.BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	.BAT_CAPACITY = -1,
#else
	.BAT_CAPACITY = 50,
#endif
	.BAT_VOLTAGE_NOW = 0,
	.BAT_VOLTAGE_AVG = 0,
	.BAT_TEMP = 0,
#endif
};

#if !defined(CONFIG_POWER_EXT)
/* ///////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Charger_Voltage
 * ///////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Charger_Voltage(struct device *dev,
	struct device_attribute *attr,
					char *buf)
{
	pr_debug("[EM] show_ADC_Charger_Voltage : %d\n",
		BMT_status.charger_vol);
	return sprintf(buf, "%d\n", BMT_status.charger_vol);
}

static ssize_t store_ADC_Charger_Voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Charger_Voltage, 0664,
	show_ADC_Charger_Voltage, store_ADC_Charger_Voltage);

/* ///////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_0_Slope
 * ///////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_0_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 0));
	pr_debug("[EM] ADC_Channel_0_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Slope, 0664,
	show_ADC_Channel_0_Slope, store_ADC_Channel_0_Slope);
/* /////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_1_Slope
 * /////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_1_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 1));
	pr_debug("[EM] ADC_Channel_1_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Slope, 0664,
	show_ADC_Channel_1_Slope, store_ADC_Channel_1_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_2_Slope
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_2_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 2));
	pr_debug("[EM] ADC_Channel_2_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Slope, 0664,
	show_ADC_Channel_2_Slope, store_ADC_Channel_2_Slope);
/* ///////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_3_Slope
 * ///////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_3_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 3));
	pr_debug("[EM] ADC_Channel_3_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Slope, 0664,
	show_ADC_Channel_3_Slope, store_ADC_Channel_3_Slope);
/* /////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_4_Slope
 * /////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_4_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 4));
	pr_debug("[EM] ADC_Channel_4_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Slope, 0664,
	show_ADC_Channel_4_Slope, store_ADC_Channel_4_Slope);
/* //////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_5_Slope
 * //////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_5_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 5));
	pr_debug("[EM] ADC_Channel_5_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Slope, 0664,
	show_ADC_Channel_5_Slope, store_ADC_Channel_5_Slope);
/* ////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_6_Slope
 * ////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_6_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 6));
	pr_debug("[EM] ADC_Channel_6_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Slope, 0664,
	show_ADC_Channel_6_Slope, store_ADC_Channel_6_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_7_Slope
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_7_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 7));
	pr_debug("[EM] ADC_Channel_7_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Slope, 0664,
	show_ADC_Channel_7_Slope, store_ADC_Channel_7_Slope);
/* /////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_8_Slope
 * /////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_8_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 8));
	pr_debug("[EM] ADC_Channel_8_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Slope, 0664,
	show_ADC_Channel_8_Slope, store_ADC_Channel_8_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_9_Slope
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_9_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 9));
	pr_debug("[EM] ADC_Channel_9_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Slope(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Slope, 0664,
	show_ADC_Channel_9_Slope, store_ADC_Channel_9_Slope);
/* /////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_10_Slope
 * /////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_10_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 10));
	pr_debug("[EM] ADC_Channel_10_Slope : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Slope, 0664,
	show_ADC_Channel_10_Slope, store_ADC_Channel_10_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_11_Slope
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_11_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 11));
	pr_debug("[EM] ADC_Channel_11_Slope : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Slope, 0664,
	show_ADC_Channel_11_Slope, store_ADC_Channel_11_Slope);
/* ////////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_12_Slope
 * ////////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_12_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 12));
	pr_debug("[EM] ADC_Channel_12_Slope : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Slope, 0664,
	show_ADC_Channel_12_Slope, store_ADC_Channel_12_Slope);
/* ////////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_13_Slope
 * ////////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_13_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 13));
	pr_debug("[EM] ADC_Channel_13_Slope : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Slope, 0664,
	show_ADC_Channel_13_Slope, store_ADC_Channel_13_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_0_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_0_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 0));
	pr_debug("[EM] ADC_Channel_0_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Offset, 0664,
	show_ADC_Channel_0_Offset, store_ADC_Channel_0_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_1_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_1_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 1));
	pr_debug("[EM] ADC_Channel_1_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Offset, 0664,
	show_ADC_Channel_1_Offset, store_ADC_Channel_1_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_2_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_2_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 2));
	pr_debug("[EM] ADC_Channel_2_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Offset, 0664,
	show_ADC_Channel_2_Offset, store_ADC_Channel_2_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_3_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_3_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 3));
	pr_debug("[EM] ADC_Channel_3_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Offset, 0664,
	show_ADC_Channel_3_Offset, store_ADC_Channel_3_Offset);
/* ///////////////////////////////////////////////////////////////
 * // Create File For EM : ADC_Channel_4_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_4_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 4));
	pr_debug("[EM] ADC_Channel_4_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Offset, 0664,
	show_ADC_Channel_4_Offset, store_ADC_Channel_4_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_5_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_5_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 5));
	pr_debug("[EM] ADC_Channel_5_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Offset, 0664,
	show_ADC_Channel_5_Offset, store_ADC_Channel_5_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_6_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_6_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 6));
	pr_debug("[EM] ADC_Channel_6_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Offset, 0664,
	show_ADC_Channel_6_Offset, store_ADC_Channel_6_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_7_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_7_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 7));
	pr_debug("[EM] ADC_Channel_7_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Offset, 0664,
	show_ADC_Channel_7_Offset, store_ADC_Channel_7_Offset);
/* ///////////////////////////////////////////////////////////////
 *      Create File For EM : ADC_Channel_8_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_8_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 8));
	pr_debug("[EM] ADC_Channel_8_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Offset, 0664,
	show_ADC_Channel_8_Offset, store_ADC_Channel_8_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_9_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_9_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 9));
	pr_debug("[EM] ADC_Channel_9_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Offset, 0664,
	show_ADC_Channel_9_Offset, store_ADC_Channel_9_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_10_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_10_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 10));
	pr_debug("[EM] ADC_Channel_10_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Offset, 0664,
	show_ADC_Channel_10_Offset, store_ADC_Channel_10_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_11_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_11_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 11));
	pr_debug("[EM] ADC_Channel_11_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Offset, 0664,
	show_ADC_Channel_11_Offset, store_ADC_Channel_11_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_12_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_12_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 12));
	pr_debug("[EM] ADC_Channel_12_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Offset, 0664,
	show_ADC_Channel_12_Offset, store_ADC_Channel_12_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_13_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_13_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 13));
	pr_debug("[EM] ADC_Channel_13_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Offset, 0664,
	show_ADC_Channel_13_Offset, store_ADC_Channel_13_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_Is_Calibration
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_Is_Calibration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 2;

	ret_value = g_ADC_Cali;
	pr_debug("[EM] ADC_Channel_Is_Calibration : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_Is_Calibration(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_Is_Calibration, 0664,
	show_ADC_Channel_Is_Calibration, store_ADC_Channel_Is_Calibration);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : Power_On_Voltage
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_Power_On_Voltage(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	pr_debug("[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_On_Voltage, 0664,
	show_Power_On_Voltage, store_Power_On_Voltage);
/* ///////////////////////////////////////////////////////////////
 * // Create File For EM : Power_Off_Voltage
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_Power_Off_Voltage(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	pr_debug("[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_Off_Voltage, 0664,
	show_Power_Off_Voltage, store_Power_Off_Voltage);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : Charger_TopOff_Value
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_Charger_TopOff_Value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 4110;
	pr_debug("[EM] Charger_TopOff_Value : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Charger_TopOff_Value(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_TopOff_Value, 0664,
	show_Charger_TopOff_Value, store_Charger_TopOff_Value);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : FG_Battery_CurrentConsumption
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_FG_Battery_CurrentConsumption(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 8888;

	ret_value = battery_meter_get_battery_current();
	pr_debug("[EM] FG_Battery_CurrentConsumption : %d/10 mA\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_Battery_CurrentConsumption, 0664,
	show_FG_Battery_CurrentConsumption,
	store_FG_Battery_CurrentConsumption);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : FG_SW_CoulombCounter
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_FG_SW_CoulombCounter(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	s32 ret_value = 7777;

	ret_value = battery_meter_get_car();
	pr_debug("[EM] FG_SW_CoulombCounter : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_SW_CoulombCounter(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_SW_CoulombCounter, 0664,
	show_FG_SW_CoulombCounter, store_FG_SW_CoulombCounter);

static ssize_t show_Charging_CallState(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	pr_debug("call state = %d\n", g_call_state);
	return sprintf(buf, "%u\n", g_call_state);
}

static ssize_t store_Charging_CallState(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, call_state;

	ret = kstrtoint(buf, 0, &call_state);
	if (ret) {
		pr_debug("wrong format!\n");
		return size;
	}

	g_call_state = (call_state ? true : false);
	pr_debug("call state = %d\n", g_call_state);
	return size;
}

static DEVICE_ATTR(Charging_CallState, 0664,
	show_Charging_CallState, store_Charging_CallState);

static ssize_t show_Charging_Enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("hold charging = %d\n", g_cmd_hold_charging);
	return sprintf(buf, "%u\n", !g_cmd_hold_charging);
}

static ssize_t store_Charging_Enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, charging_enable = 1;

	ret = kstrtoint(buf, 0, &charging_enable);
	if (ret) {
		pr_debug("wrong format!\n");
		return size;
	}

	if (charging_enable == 1)
		g_cmd_hold_charging = false;
	else if (charging_enable == 0)
		g_cmd_hold_charging = true;
	wake_up_bat_update_meter();
	pr_debug("hold charging = %d\n", g_cmd_hold_charging);
	return size;
}

static DEVICE_ATTR(Charging_Enable, 0664,
	show_Charging_Enable, store_Charging_Enable);

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
static ssize_t show_Pump_Express(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int icount = 20; /* max debouncing time 20 * 0.2 sec */

	if (true == ta_check_chr_type &&
	    BMT_status.charger_type == STANDARD_CHARGER &&
	    BMT_status.SOC >= p_bat_charging_data->ta_start_battery_soc &&
	    BMT_status.SOC < p_bat_charging_data->ta_stop_battery_soc) {
		pr_debug("[%s]Wait for PE detection\n",
			__func__);
		do {
			icount--;
			msleep(200);
		} while (icount && ta_check_chr_type);
	}

	pr_debug("Pump express = %d\n", is_ta_connect);
	return sprintf(buf, "%u\n", is_ta_connect);
}

static ssize_t store_Pump_Express(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rv;
	u32 value;

	rv = kstrtouint(buf, 0, &value);
	if (rv != 1)
		return -EINVAL;
	is_ta_connect = (value != 0) ? true : false;
	pr_debug("Pump express= %d\n", is_ta_connect);
	return size;
}

static DEVICE_ATTR(Pump_Express, 0664,
	show_Pump_Express, store_Pump_Express);
#endif

static ssize_t show_Custom_Charging_Current(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("custom charging current = %d\n",
		g_custom_charging_current);
	return sprintf(buf, "%d\n", g_custom_charging_current);
}

static ssize_t store_Custom_Charging_Current(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, cur;

	ret = kstrtoint(buf, 0, &cur);
	if (ret) {
		pr_debug("wrong format!\n");
		return size;
	}

	g_custom_charging_current = cur;
	pr_debug("custom charging current = %d\n",
		g_custom_charging_current);
	wake_up_bat_update_meter();
	return size;
}

static DEVICE_ATTR(Custom_Charging_Current, 0664,
	show_Custom_Charging_Current, store_Custom_Charging_Current);

static void mt_battery_update_EM(struct battery_data *bat_data)
{
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC;
	bat_data->BAT_TemperatureR = BMT_status.temperatureR;    /* API */
	bat_data->BAT_TempBattVoltage = BMT_status.temperatureV; /* API */
	bat_data->BAT_InstatVolt = BMT_status.bat_vol;		 /* VBAT */
	bat_data->BAT_BatteryAverageCurrent = BMT_status.ICharging;
	bat_data->BAT_BatterySenseVoltage = BMT_status.bat_vol;
	bat_data->BAT_ISenseVoltage = BMT_status.Vsense; /* API */
	bat_data->BAT_ChargerVoltage = BMT_status.charger_vol;

	if ((BMT_status.UI_SOC == 100) && BMT_status.charger_exist)
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;

#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
	if (bat_data->BAT_CAPACITY <= 0)
		bat_data->BAT_CAPACITY = 1;

	pr_debug("BAT_CAPACITY=1, due to define MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION\r\n");
#endif
}

static bool mt_battery_100Percent_tracking_check(void)
{
	bool resetBatteryMeter = false;
	u32 cust_sync_time;
	static u32 timer_counter;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	cust_sync_time = p_bat_charging_data->cust_soc_jeita_sync_time;
	if (timer_counter == 0)
		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);
#else
	cust_sync_time = p_bat_charging_data->onehundred_percent_tracking_time;
	if (timer_counter == 0)
		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if (g_temp_status != TEMP_POS_10_TO_POS_45 &&
	    g_temp_status != TEMP_POS_0_TO_POS_10) {
		pr_debug("Skip 100percent tracking due to not 4.2V full-charging.\n");
		return false;
	}
#endif

	if (BMT_status.bat_full == true) {
		/* charging full first, UI tracking to 100% */
		if (BMT_status.bat_in_recharging_state == true) {
			if (BMT_status.UI_SOC >= 100)
				BMT_status.UI_SOC = 100;

			resetBatteryMeter = false;
		} else if (BMT_status.UI_SOC >= 100) {
			BMT_status.UI_SOC = 100;

			if ((g_charging_full_reset_bat_meter == true) &&
			    (BMT_status.bat_charging_state == CHR_BATFULL)) {
				resetBatteryMeter = true;
				g_charging_full_reset_bat_meter = false;
			} else {
				resetBatteryMeter = false;
			}
		} else {
			/* increase UI percentage every xxs */
			if (timer_counter >=
			    (cust_sync_time / BAT_TASK_PERIOD)) {
				timer_counter = 1;
				BMT_status.UI_SOC++;
			} else {
				timer_counter++;

				return resetBatteryMeter;
			}

			resetBatteryMeter = true;
		}

		pr_debug("[Battery] mt_battery_100percent_tracking(), Charging full first UI(%d), reset(%d) \r\n",
			BMT_status.UI_SOC, resetBatteryMeter);
	} else {
		/* charging is not full,  UI keep 99% if reaching 100%, */

		if (BMT_status.UI_SOC >= 99 &&
		    battery_meter_get_battery_current_sign()) {
			BMT_status.UI_SOC = 99;
			resetBatteryMeter = false;

			pr_debug("[Battery] mt_battery_100percent_tracking(), UI full first, keep (%d) \r\n",
				BMT_status.UI_SOC);
		}

		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);
	}

	return resetBatteryMeter;
}

static bool mt_battery_nPercent_tracking_check(void)
{
	bool resetBatteryMeter = false;
#if defined(CONFIG_SOC_BY_HW_FG)
	static u32 timer_counter;

	if (timer_counter == 0)
		timer_counter = (p_bat_charging_data->npercent_tracking_time /
			BAT_TASK_PERIOD);

	if (BMT_status.nPrecent_UI_SOC_check_point == 0)
		return false;

	/* fuel gauge ZCV < 15%, but UI > 15%,  15% can be customized */
	if ((BMT_status.ZCV <= BMT_status.nPercent_ZCV) &&
	    (BMT_status.UI_SOC > BMT_status.nPrecent_UI_SOC_check_point)) {
		if (timer_counter ==
		    (p_bat_charging_data->npercent_tracking_time /
		     BAT_TASK_PERIOD)) {
			/* every x sec decrease UI	percentage */
			BMT_status.UI_SOC--;
			timer_counter = 1;
		} else {
			timer_counter++;
			return resetBatteryMeter;
		}

		resetBatteryMeter = true;

		pr_debug("[Battery]mt_battery_nPercent_tracking_check(), ZCV(%d) <= BMT_status.nPercent_ZCV(%d), UI_SOC=%d., tracking UI_SOC=%d \r\n",
			BMT_status.ZCV, BMT_status.nPercent_ZCV,
			BMT_status.UI_SOC,
			BMT_status.nPrecent_UI_SOC_check_point);
	} else if ((BMT_status.ZCV > BMT_status.nPercent_ZCV) &&
		   (BMT_status.UI_SOC ==
		    BMT_status.nPrecent_UI_SOC_check_point)) {
		/*
		 * UI less than 15 , but fuel gague is more than 15,
		 * hold UI 15%
		 */
		timer_counter = (p_bat_charging_data->npercent_tracking_time /
				 BAT_TASK_PERIOD);
		resetBatteryMeter = true;

		pr_debug("[Battery]mt_battery_nPercent_tracking_check() ZCV(%d) > BMT_status.nPercent_ZCV(%d) and UI SOC (%d), then keep %d. \r\n",
			BMT_status.ZCV, BMT_status.nPercent_ZCV,
			BMT_status.UI_SOC,
			BMT_status.nPrecent_UI_SOC_check_point);
	} else {
		timer_counter = (p_bat_charging_data->npercent_tracking_time /
				 BAT_TASK_PERIOD);
	}
#endif
	return resetBatteryMeter;
}

static bool mt_battery_0Percent_tracking_check(void)
{
	bool resetBatteryMeter = true;

	if (BMT_status.UI_SOC <= 0)
		BMT_status.UI_SOC = 0;
	else
		BMT_status.UI_SOC--;

	pr_debug("[Battery] mt_battery_0Percent_tracking_check(), VBAT < %d UI_SOC = (%d)\r\n",
		SYSTEM_OFF_VOLTAGE, BMT_status.UI_SOC);

	return resetBatteryMeter;
}

static void mt_battery_Sync_UI_Percentage_to_Real(void)
{
	static u32 timer_counter;

	if (BMT_status.bat_in_recharging_state == true) {
		BMT_status.UI_SOC = 100;
		return;
	}

	if ((BMT_status.UI_SOC > BMT_status.SOC) &&
	    ((BMT_status.UI_SOC != 1))) {
		/* reduce after xxs */
		if (g_refresh_ui_soc ||
		    timer_counter ==
			    (p_bat_charging_data->sync_to_real_tracking_time /
			     BAT_TASK_PERIOD)) {
			BMT_status.UI_SOC--;
			timer_counter = 0;
			g_refresh_ui_soc = false;
		} else {
			timer_counter++;
		}

		pr_debug("Sync UI percentage to Real one, BMT_status.UI_SOC=%d, BMT_status.SOC=%d, counter = %d\r\n",
			BMT_status.UI_SOC, BMT_status.SOC, timer_counter);
	} else {
		timer_counter = 0;

		if (BMT_status.UI_SOC == -1)
			BMT_status.UI_SOC = BMT_status.SOC;
		else if (BMT_status.charger_exist &&
			 BMT_status.bat_charging_state != CHR_ERROR) {
			if (BMT_status.UI_SOC < BMT_status.SOC &&
			    (BMT_status.SOC - BMT_status.UI_SOC > 1))
				BMT_status.UI_SOC++;
			else
				BMT_status.UI_SOC = BMT_status.SOC;
		}
	}

	if (BMT_status.bat_full != true && BMT_status.UI_SOC == 100 &&
	    battery_meter_get_battery_current_sign()) {
		pr_debug("[Sync_UI] keep UI_SOC at 99 due to battery not full yet.\r\n");
		BMT_status.UI_SOC = 99;
	}

	if (BMT_status.UI_SOC <= 0) {
		BMT_status.UI_SOC = 1;
		pr_debug("[Battery]UI_SOC get 0 first (%d)\r\n",
		  BMT_status.UI_SOC);
	}
}

static void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = bat_data->psy;

	bool resetBatteryMeter = false;

	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	bat_data->BAT_VOLTAGE_AVG = BMT_status.bat_vol * 1000;
	/* voltage_now unit is microvolt */
	bat_data->BAT_VOLTAGE_NOW = BMT_status.bat_vol * 1000;
	bat_data->BAT_TEMP = BMT_status.temperature * 10;
	bat_data->BAT_PRESENT = BMT_status.bat_exist;

	if (BMT_status.charger_exist &&
	    (BMT_status.bat_charging_state != CHR_ERROR) &&
	    !g_cmd_hold_charging) {
		if (BMT_status.bat_exist) { /* charging */
			if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE)
				resetBatteryMeter =
					mt_battery_0Percent_tracking_check();
			else
				resetBatteryMeter =
					mt_battery_100Percent_tracking_check();

			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
		} else { /* No Battery, Only Charger */

			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
			BMT_status.UI_SOC = 0;
		}

	} else { /* Only Battery */

		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING;
		if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE)
			resetBatteryMeter =
				mt_battery_0Percent_tracking_check();
		else
			resetBatteryMeter =
				mt_battery_nPercent_tracking_check();
	}

	if (resetBatteryMeter == true)
		battery_meter_reset(true);
	else
		mt_battery_Sync_UI_Percentage_to_Real();

	pr_debug("UI_SOC=(%d), resetBatteryMeter=(%d)\n",
		    BMT_status.UI_SOC, resetBatteryMeter);

	if (battery_meter_ocv2cv_trans_support()) {
		/* We store capacity before loading compenstation in RTC */
		if (battery_meter_get_battery_soc() <= 1)
			set_rtc_spare_fg_value(1);
		else
			set_rtc_spare_fg_value(
				battery_meter_get_battery_soc());
			/*use battery_soc */
	} else {
		/* set RTC SOC to 1 to avoid SOC jump in charger boot. */
		if (BMT_status.UI_SOC <= 1)
			set_rtc_spare_fg_value(1);
		else
			set_rtc_spare_fg_value(BMT_status.UI_SOC);
	}
	pr_debug("RTC_SOC=(%d)\n", get_rtc_spare_fg_value());

	mt_battery_update_EM(bat_data);
	power_supply_changed(bat_psy);
}

#endif

static void ac_update(struct ac_data *ac_data)
{
	static int ac_status = -1;
	struct power_supply *ac_psy = ac_data->psy;
	struct power_supply_desc *ac_psd = &ac_data->psd;

	if (BMT_status.charger_exist) {
		if ((BMT_status.charger_type == NONSTANDARD_CHARGER) ||
		    (BMT_status.charger_type == STANDARD_CHARGER) ||
		    (BMT_status.charger_type == APPLE_1_0A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_2_1A_CHARGER) ||
		    (BMT_status.charger_type == TYPEC_1_5A_CHARGER) ||
		    (BMT_status.charger_type == TYPEC_3A_CHARGER) ||
		    (BMT_status.charger_type == TYPEC_PD_5V_CHARGER) ||
		    (BMT_status.charger_type == TYPEC_PD_12V_CHARGER)) {
			ac_data->AC_ONLINE = 1;
			ac_psd->type = POWER_SUPPLY_TYPE_MAINS;
		} else
			ac_data->AC_ONLINE = 0;

	} else {
		ac_data->AC_ONLINE = 0;
	}

	if (ac_status != ac_data->AC_ONLINE) {
		ac_status = ac_data->AC_ONLINE;
		power_supply_changed(ac_psy);
	}
}

static void usb_update(struct usb_data *usb_data)
{
	static int usb_status = -1;
	struct power_supply *usb_psy = usb_data->psy;
	struct power_supply_desc *usb_psd = &usb_data->psd;

	if (BMT_status.charger_exist) {
		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST)) {
			usb_data->USB_ONLINE = 1;
			usb_psd->type = POWER_SUPPLY_TYPE_USB;
		} else
			usb_data->USB_ONLINE = 0;
	} else {
		usb_data->USB_ONLINE = 0;
	}

	if (usb_status != usb_data->USB_ONLINE) {
		usb_status = usb_data->USB_ONLINE;
		power_supply_changed(usb_psy);
	}
}

/* ///////////////////////////////////////////////////////////////
 *     Battery Temprature Parameters and functions
 * ///////////////////////////////////////////////////////////////
 */
bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det() == true)
		return true;

	pr_debug("[pmic_chrdet_status] No charger\r\n");
	return false;
}

/* ///////////////////////////////////////////////////////////////
 *     Pulse Charging Algorithm
 * ///////////////////////////////////////////////////////////////
 */
bool bat_is_charger_exist(void)
{
	return bat_charger_get_detect_status();
}

bool bat_is_charging_full(void)
{
	if ((BMT_status.bat_full == true) &&
	    (BMT_status.bat_in_recharging_state == false))
		return true;
	else
		return false;
}

u32 bat_get_ui_percentage(void)
{
	/* for plugging out charger in recharge phase, using SOC as UI_SOC */
	if (chr_wake_up_bat == true)
		return BMT_status.SOC;
	else
		return BMT_status.UI_SOC;
}

/* Full state --> recharge voltage --> full state */
u32 bat_is_recharging_phase(void)
{
	if (BMT_status.bat_in_recharging_state || BMT_status.bat_full == true)
		return true;
	else
		return false;
}

unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
	unsigned long ret_val = 0;

#if defined(CONFIG_POWER_EXT)
	ret_val = 4000;
#else
	ret_val = battery_meter_get_battery_voltage();
#endif

	return ret_val;
}

static void mt_battery_average_method_init(u32 *bufferdata, u32 data, s32 *sum)
{
	u32 i;
	static bool batteryBufferFirst = true;
	static bool previous_charger_exist;
	static bool previous_in_recharge_state;
	static u8 index;

	/* reset charging current window while plug in/out */
	if (need_clear_current_window) {
		if (BMT_status.charger_exist) {
			if (previous_charger_exist == false) {
				batteryBufferFirst = true;
				previous_charger_exist = true;
				if ((BMT_status.charger_type ==
				     STANDARD_CHARGER) ||
				    (BMT_status.charger_type ==
				     APPLE_1_0A_CHARGER) ||
				    (BMT_status.charger_type ==
				     APPLE_2_1A_CHARGER) ||
				    (BMT_status.charger_type == CHARGING_HOST))
					data = 650; /* mA */
				else /* USB or non-stadanrd charger */
					data = 450; /* mA */
			} else if ((previous_in_recharge_state == false) &&
				   (BMT_status.bat_in_recharging_state ==
				    true)) {
				batteryBufferFirst = true;
				if ((BMT_status.charger_type ==
				     STANDARD_CHARGER) ||
				    (BMT_status.charger_type ==
				     APPLE_1_0A_CHARGER) ||
				    (BMT_status.charger_type ==
				     APPLE_2_1A_CHARGER) ||
				    (BMT_status.charger_type == CHARGING_HOST))
					data = 650; /* mA */
				else /* USB or non-stadanrd charger */
					data = 450; /* mA */
			}

			previous_in_recharge_state =
				BMT_status.bat_in_recharging_state;
		} else {
			if (previous_charger_exist == true) {
				batteryBufferFirst = true;
				previous_charger_exist = false;
				data = 0;
			}
		}
	}

	pr_debug("batteryBufferFirst =%d, data= (%d)\n",
		batteryBufferFirst, data);

	if (batteryBufferFirst == true) {
		for (i = 0; i < BATTERY_AVERAGE_SIZE; i++)
			bufferdata[i] = data;

		*sum = data * BATTERY_AVERAGE_SIZE;
	}

	index++;
	if (index >= BATTERY_AVERAGE_DATA_NUMBER) {
		index = BATTERY_AVERAGE_DATA_NUMBER;
		batteryBufferFirst = false;
	}
}

static u32 mt_battery_average_method(u32 *bufferdata, u32 data, s32 *sum,
		u8 batteryIndex)
{
	u32 avgdata;

	mt_battery_average_method_init(bufferdata, data, sum);

	*sum -= bufferdata[batteryIndex];
	*sum += data;
	bufferdata[batteryIndex] = data;
	avgdata = (*sum) / BATTERY_AVERAGE_SIZE;

	pr_debug("bufferdata[%d]= (%d)\n", batteryIndex,
		    bufferdata[batteryIndex]);
	return avgdata;
}

static int filter_battery_temperature(int instant_temp)
{
	int check_count;

	/* recheck 3 times for critical temperature */
	for (check_count = 0; check_count < 3; check_count++) {
		if (instant_temp >
			    p_bat_charging_data->max_discharge_temperature ||
		    instant_temp <
			    p_bat_charging_data->min_discharge_temperature) {

			instant_temp = battery_meter_get_battery_temperature();
			pr_debug("recheck battery temperature result: %d\n",
				instant_temp);
			msleep(20);
			continue;
		}
	}
	return instant_temp;
}

void mt_battery_GetBatteryData(void)
{
	u32 bat_vol, charger_vol, Vsense, ZCV;
	s32 ICharging, temperature, temperatureR, temperatureV, SOC;
	s32 avg_temperature;
	static s32 bat_sum, icharging_sum, temperature_sum;
	static s32 batteryVoltageBuffer[BATTERY_AVERAGE_SIZE];
	static s32 batteryCurrentBuffer[BATTERY_AVERAGE_SIZE];
	static s32 batteryTempBuffer[BATTERY_AVERAGE_SIZE];
	static u8 batteryIndex;
	static s32 previous_SOC = -1;

	bat_vol = battery_meter_get_battery_voltage();
	Vsense = battery_meter_get_VSense();
	ICharging = battery_meter_get_charging_current();
	charger_vol = battery_meter_get_charger_voltage();
	temperature = battery_meter_get_battery_temperature();
	temperatureV = battery_meter_get_tempV();
	temperatureR = battery_meter_get_tempR(temperatureV);

	if (bat_meter_timeout == true) {
		SOC = battery_meter_get_battery_percentage();
		bat_meter_timeout = false;
	} else {
		if (previous_SOC == -1)
			SOC = battery_meter_get_battery_percentage();
		else
			SOC = previous_SOC;
	}

	ZCV = battery_meter_get_battery_zcv();

	need_clear_current_window = true;
	BMT_status.ICharging =
		mt_battery_average_method(&batteryCurrentBuffer[0], ICharging,
			&icharging_sum, batteryIndex);
	need_clear_current_window = false;
#if 1
	if (previous_SOC == -1 && bat_vol <= SYSTEM_OFF_VOLTAGE) {
		pr_debug("battery voltage too low, use ZCV to init average data.\n");
		BMT_status.bat_vol = mt_battery_average_method(
			&batteryVoltageBuffer[0], ZCV, &bat_sum, batteryIndex);
	} else {
		BMT_status.bat_vol = mt_battery_average_method(
			&batteryVoltageBuffer[0], bat_vol, &bat_sum,
			batteryIndex);
	}
#else
	BMT_status.bat_vol = mt_battery_average_method(
		&batteryVoltageBuffer[0], bat_vol, &bat_sum, batteryIndex);
#endif
	avg_temperature =
		mt_battery_average_method(&batteryTempBuffer[0], temperature,
			&temperature_sum, batteryIndex);

	if (p_bat_charging_data->use_avg_temperature)
		BMT_status.temperature = avg_temperature;
	else
		BMT_status.temperature =
			filter_battery_temperature(temperature);

	if ((g_battery_thermal_throttling_flag == 1) ||
	    (g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature =
				battery_cmd_thermal_test_mode_value;
			pr_debug("[Battery] In thermal_test_mode , Tbat=%d\n",
				BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		if (BMT_status.temperature >
			    p_bat_charging_data->max_discharge_temperature ||
		    BMT_status.temperature <
			    p_bat_charging_data->min_discharge_temperature) {
			struct battery_data *bat_data = &battery_main;
			struct power_supply *bat_psy = &bat_data->psy;

			pr_debug("[Battery] instant Tbat(%d) out of range, power down device.\n",
				BMT_status.temperature);

			bat_data->BAT_CAPACITY = 0;
			power_supply_changed(bat_psy);

			/* can not power down due to charger exist, so need
			 * reset system
			 */
			if (BMT_status.charger_exist)
				bat_charger_set_platform_reset();
			else
				orderly_poweroff(true);
		}
#endif
	}

	BMT_status.Vsense = Vsense;
	BMT_status.charger_vol = charger_vol;
	BMT_status.temperatureV = temperatureV;
	BMT_status.temperatureR = temperatureR;
	BMT_status.SOC = SOC;
	BMT_status.ZCV = ZCV;

	if (BMT_status.charger_exist == false &&
	    !battery_meter_ocv2cv_trans_support()) {
		if (BMT_status.SOC > previous_SOC && previous_SOC >= 0)
			BMT_status.SOC = previous_SOC;
	}

	previous_SOC = BMT_status.SOC;

	batteryIndex++;
	if (batteryIndex >= BATTERY_AVERAGE_SIZE)
		batteryIndex = 0;

	pr_debug("AvgVbat=(%d),bat_vol=(%d),AvgI=(%d),I=(%d),VChr=(%d),AvgT=(%d),T=(%d),pre_SOC=(%d),SOC=(%d),ZCV=(%d)\n",
		BMT_status.bat_vol, bat_vol, BMT_status.ICharging, ICharging,
		BMT_status.charger_vol, BMT_status.temperature, temperature,
		previous_SOC, BMT_status.SOC, BMT_status.ZCV);
}

static int mt_battery_CheckBatteryTemp(void)
{
	int status = PMU_STATUS_OK;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

	pr_debug("[BATTERY] support JEITA, temperature=%d\n",
		BMT_status.temperature);

	if (do_jeita_state_machine() == PMU_STATUS_FAIL) {
		pr_debug("[BATTERY] JEITA : fail\n");
		status = PMU_STATUS_FAIL;
	}
#else

#ifdef CONFIG_BAT_LOW_TEMP_PROTECT_ENABLE
	if ((BMT_status.temperature <
	     p_bat_charging_data->min_charge_temperature) ||
	    (BMT_status.temperature ==
	     p_bat_charging_data->err_charge_temperature)) {
		pr_debug("[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
		status = PMU_STATUS_FAIL;
	}
#endif
	if (BMT_status.temperature >=
	    p_bat_charging_data->max_charge_temperature) {
		pr_debug("[BATTERY] Battery Over Temperature !!\n\r");
		status = PMU_STATUS_FAIL;
	}
#endif

	return status;
}

static int mt_battery_CheckChargerVoltage(void)
{
	int status = PMU_STATUS_OK;

	if (BMT_status.charger_exist) {
		if (p_bat_charging_data->v_charger_enable == 1) {
			if (BMT_status.charger_vol <=
			    p_bat_charging_data->v_charger_min) {
				pr_debug("[BATTERY]Charger under voltage!!\r\n");
				BMT_status.bat_charging_state = CHR_ERROR;
				status = PMU_STATUS_FAIL;
			}
		}
		if (BMT_status.charger_vol >=
		    p_bat_charging_data->v_charger_max) {
			pr_debug("[BATTERY]Charger over voltage !!\r\n");
			BMT_status.charger_protect_status = charger_OVER_VOL;
			BMT_status.bat_charging_state = CHR_ERROR;
			status = PMU_STATUS_FAIL;
		}
	}

	return status;
}

static int mt_battery_CheckChargingTime(void)
{
	int status = PMU_STATUS_OK;

	if ((g_battery_thermal_throttling_flag == 2) ||
	    (g_battery_thermal_throttling_flag == 3)) {
		pr_debug("[TestMode] Disable Safety Timer. bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			g_battery_thermal_throttling_flag,
			battery_cmd_thermal_test_mode,
			battery_cmd_thermal_test_mode_value);

	} else {
		/* Charging OT */
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME) {
			pr_debug("[BATTERY] Charging Over Time.\n");

			status = PMU_STATUS_FAIL;
		}
	}

	return status;
}

#if defined(CONFIG_STOP_CHARGING_IN_TAKLING)
static int mt_battery_CheckCallState(void)
{
	int status = PMU_STATUS_OK;

	if ((g_call_state == CALL_ACTIVE) &&
	    (BMT_status.bat_vol > p_bat_charging_data->v_cc2topoff_thres))
		status = PMU_STATUS_FAIL;

	return status;
}
#endif

static void mt_battery_CheckBatteryStatus(void)
{
	if (mt_battery_CheckBatteryTemp() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

	if (mt_battery_CheckChargerVoltage() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}
#if defined(CONFIG_STOP_CHARGING_IN_TAKLING)
	if (mt_battery_CheckCallState() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_HOLD;
		return;
	}
#endif

	if (mt_battery_CheckChargingTime() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

	if (g_cmd_hold_charging == true) {
		BMT_status.bat_charging_state = CHR_CMD_HOLD;
		return;
	} else if (BMT_status.bat_charging_state == CHR_CMD_HOLD) {
		BMT_status.bat_charging_state = CHR_PRE;
		return;
	}
}

static void mt_battery_notify_TatalChargingTime_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME)
	if ((g_battery_thermal_throttling_flag == 2) ||
	    (g_battery_thermal_throttling_flag == 3)) {
		pr_debug("[TestMode] Disable Safety Timer : no UI display\n");
	} else {
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME) {
			g_BatteryNotifyCode |= 0x0010;
			pr_debug("[BATTERY] Charging Over Time\n");
		} else
			g_BatteryNotifyCode &= ~(0x0010);
	}

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME (%x)\n",
		g_BatteryNotifyCode);
#endif
}

static void mt_battery_notify_VBat_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0004_VBAT)
	if (BMT_status.bat_vol > 4350) {
		g_BatteryNotifyCode |= 0x0008;
		pr_debug("[BATTERY] bat_vlot(%d) > 4350mV\n",
			BMT_status.bat_vol);
	} else
		g_BatteryNotifyCode &= ~(0x0008);

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0004_VBAT (%x)\n",
		g_BatteryNotifyCode);

#endif
}

static void mt_battery_notify_ICharging_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0003_ICHARGING)
	if ((BMT_status.ICharging > 1000) &&
	    (BMT_status.total_charging_time > 300)) {
		g_BatteryNotifyCode |= 0x0004;
		pr_debug("[BATTERY] I_charging(%ld) > 1000mA\n",
			BMT_status.ICharging);
	} else
		g_BatteryNotifyCode &= ~(0x0004);

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0003_ICHARGING (%x)\n",
		g_BatteryNotifyCode);

#endif
}

static void mt_battery_notify_VBatTemp_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if ((BMT_status.temperature >=
	     p_bat_charging_data->max_charge_temperature) ||
	    (BMT_status.temperature <
	     p_bat_charging_data->temp_neg_10_threshold)) {
#else
#ifdef CONFIG_BAT_LOW_TEMP_PROTECT_ENABLE
	if ((BMT_status.temperature >=
	     p_bat_charging_data->max_charge_temperature) ||
	    (BMT_status.temperature <
	     p_bat_charging_data->min_charge_temperature)) {
#else
	if (BMT_status.temperature >=
	    p_bat_charging_data->max_charge_temperature) {
#endif
#endif /* #if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT) */
		g_BatteryNotifyCode |= 0x0002;
		pr_debug("[BATTERY] bat_temp(%d) out of range\n",
			BMT_status.temperature);
	} else
		g_BatteryNotifyCode &= ~(0x0002);

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0002_VBATTEMP (%x)\n",
		g_BatteryNotifyCode);

#endif
}

static void mt_battery_notify_VCharger_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	if (BMT_status.charger_vol > p_bat_charging_data->v_charger_max) {
		g_BatteryNotifyCode |= 0x0001;
		pr_debug("[BATTERY] BMT_status.charger_vol(%d) > %d mV\n",
			BMT_status.charger_vol,
			p_bat_charging_data->v_charger_max);
	} else
		g_BatteryNotifyCode &= ~(0x0001);

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0001_VCHARGER (%x)\n",
		g_BatteryNotifyCode);
#endif
}

static void mt_battery_notify_UI_test(void)
{
	if (g_BN_TestMode == 0x0001) {
		g_BatteryNotifyCode = 0x0001;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0001_VCHARGER\n");
	} else if (g_BN_TestMode == 0x0002) {
		g_BatteryNotifyCode = 0x0002;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0002_VBATTEMP\n");
	} else if (g_BN_TestMode == 0x0003) {
		g_BatteryNotifyCode = 0x0004;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0003_ICHARGING\n");
	} else if (g_BN_TestMode == 0x0004) {
		g_BatteryNotifyCode = 0x0008;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0004_VBAT\n");
	} else if (g_BN_TestMode == 0x0005) {
		g_BatteryNotifyCode = 0x0010;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME\n");
	} else {
		pr_debug("[BATTERY] Unknown BN_TestMode Code : %x\n",
			g_BN_TestMode);
	}
}

void mt_battery_notify_check(void)
{
	g_BatteryNotifyCode = 0x0000;

	if (g_BN_TestMode == 0x0000) { /* for normal case */
		pr_debug("[BATTERY] mt_battery_notify_check\n");

		mt_battery_notify_VCharger_check();

		mt_battery_notify_VBatTemp_check();

		mt_battery_notify_ICharging_check();

		mt_battery_notify_VBat_check();

		mt_battery_notify_TatalChargingTime_check();
	} else
		mt_battery_notify_UI_test(); /* for UI test */
}

static void mt_battery_thermal_check(void)
{
	if ((g_battery_thermal_throttling_flag == 1) ||
	    (g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature =
				battery_cmd_thermal_test_mode_value;
			pr_debug("[Battery] In thermal_test_mode , Tbat=%d\n",
				BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		if (BMT_status.temperature >
			    p_bat_charging_data->max_discharge_temperature ||
		    BMT_status.temperature <
			    p_bat_charging_data->min_discharge_temperature) {
			struct battery_data *bat_data = &battery_main;
			struct power_supply *bat_psy = bat_data->psy;

			pr_debug("[Battery] Tbat(%d)out of range, power down device.\n",
				BMT_status.temperature);

			bat_data->BAT_CAPACITY = 0;
			power_supply_changed(bat_psy);

			/* can not power down due to charger exist, so need
			 * reset system
			 */
			if (BMT_status.charger_exist)
				bat_charger_set_platform_reset();
			else
				orderly_poweroff(true);
		}
#else
		if (BMT_status.temperature >=
		    p_bat_charging_data->max_discharge_temperature) {
#if defined(CONFIG_POWER_EXT)
			pr_debug("[BATTERY] CONFIG_POWER_EXT, no update battery update power down.\n");
#else
			if ((g_platform_boot_mode == META_BOOT) ||
				(g_platform_boot_mode == ADVMETA_BOOT) ||
				(g_platform_boot_mode ==
				 ATE_FACTORY_BOOT)) {
				pr_debug("[BATTERY] boot mode = %d, bypass temperature check\n",
					g_platform_boot_mode);
			} else {
				struct battery_data *bat_data =
					&battery_main;
				struct power_supply *bat_psy =
					bat_data->psy;

				pr_debug("[Battery] Tbat(%d)>=%d, system need power down.\n",
					p_bat_charging_data
					->max_discharge_temperature,
					BMT_status.temperature);

				bat_data->BAT_CAPACITY = 0;

				power_supply_changed(bat_psy);

				/* can not power down due to charger
				 * exist, so need reset system
				 */
				if (BMT_status.charger_exist)
					bat_charger_set_platform_reset();
				else
					orderly_poweroff(true);
			}
#endif
		}
#endif
	}
}

int bat_charger_type_detection(void)
{
	mutex_lock(&charger_type_mutex);
	if (BMT_status.charger_type == CHARGER_UNKNOWN)
		BMT_status.charger_type = bat_charger_get_charger_type();
	mutex_unlock(&charger_type_mutex);

	return BMT_status.charger_type;
}

void bat_update_charger_type(int new_type)
{
	mutex_lock(&charger_type_mutex);
	BMT_status.charger_type = new_type;
	mutex_unlock(&charger_type_mutex);
	pr_debug("update new charger type: %d\n", new_type);
	wake_up_bat();
}

static void mt_battery_charger_detect_check(void)
{
	static bool fg_first_detect;

	if (upmu_is_chr_det() == true) {

		if (!BMT_status.charger_exist)
			__pm_stay_awake(&battery_suspend_lock);

		BMT_status.charger_exist = true;

		/* re-detect once after 10s if it is non-standard type */
		if (BMT_status.charger_type == NONSTANDARD_CHARGER &&
		    fg_first_detect) {
			mutex_lock(&charger_type_mutex);
			BMT_status.charger_type =
				bat_charger_get_charger_type();
			mutex_unlock(&charger_type_mutex);
			fg_first_detect = false;
			if (BMT_status.charger_type != NONSTANDARD_CHARGER) {
				pr_debug("Update charger type to %d!\n",
					BMT_status.charger_type);
				if ((BMT_status.charger_type ==
				     STANDARD_HOST) ||
				    (BMT_status.charger_type ==
				     CHARGING_HOST)) {
					mt_usb_connect();
				}
			}
		}

		if (BMT_status.charger_type == CHARGER_UNKNOWN) {
			bat_charger_type_detection();
			if ((BMT_status.charger_type == STANDARD_HOST) ||
			    (BMT_status.charger_type == CHARGING_HOST)) {
				mt_usb_connect();
			}
			if (BMT_status.charger_type != CHARGER_UNKNOWN)
				fg_first_detect = true;
		}

		pr_debug("[BAT_thread]Cable in, CHR_Type_num=%d\r\n",
			    BMT_status.charger_type);

	} else {
		if (BMT_status.charger_exist) {
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode ==
			    KERNEL_POWER_OFF_CHARGING_BOOT)
				__pm_stay_awake(&battery_suspend_lock);
			else
#endif
				__pm_wakeup_event(&battery_suspend_lock,
						  HZ / 2);
		}
		fg_first_detect = false;

		BMT_status.charger_exist = false;
		BMT_status.charger_type = CHARGER_UNKNOWN;
		BMT_status.bat_full = false;
		BMT_status.bat_in_recharging_state = false;
		BMT_status.bat_charging_state = CHR_PRE;
		BMT_status.total_charging_time = 0;
		BMT_status.PRE_charging_time = 0;
		BMT_status.CC_charging_time = 0;
		BMT_status.TOPOFF_charging_time = 0;
		BMT_status.POSTFULL_charging_time = 0;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
		if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {

			/* in case of pd hw reset. wait for vbus re-assert */
			if (mt_usb_pd_support())
				msleep(1000);
			if (upmu_is_chr_det() == false) {
				pr_debug("Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
				orderly_poweroff(true);
			}
		}
#endif

		if (g_cmd_hold_charging) {
			g_cmd_hold_charging = false;
			bat_charger_enable_power_path(true);
		}

		pr_debug("[BAT_thread]Cable out \r\n");
		mt_usb_disconnect();
	}
}

static void mt_battery_update_status(void)
{
#if !defined(CONFIG_POWER_EXT)
	if (battery_meter_initilized == true)
		battery_update(&battery_main);
#endif
	ac_update(&ac_main);
	usb_update(&usb_main);
}

static void do_chrdet_int_task(void)
{
	if (g_bat_init_flag == true) {
		if (upmu_is_chr_det() == true) {
			pr_debug("[do_chrdet_int_task] charger exist!\n");

			if (!BMT_status.charger_exist)
				__pm_stay_awake(&battery_suspend_lock);
			BMT_status.charger_exist = true;

#if defined(CONFIG_POWER_EXT)
			bat_charger_type_detection();
			mt_usb_connect();
			pr_notice("[do_chrdet_int_task] call mt_usb_connect() in EVB\n");
#endif
		} else {
			pr_debug("[do_chrdet_int_task] charger NOT exist!\n");
			if (BMT_status.charger_exist) {
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
				if (g_platform_boot_mode ==
				    KERNEL_POWER_OFF_CHARGING_BOOT)
					__pm_stay_awake(&battery_suspend_lock);
				else
#endif
					__pm_wakeup_event(&battery_suspend_lock,
							  HZ / 2);
			}
			BMT_status.charger_exist = false;
			BMT_status.charger_type = CHARGER_UNKNOWN;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode ==
				    KERNEL_POWER_OFF_CHARGING_BOOT ||
			    g_platform_boot_mode ==
				    LOW_POWER_OFF_CHARGING_BOOT) {
				/* in case of pd hw reset. wait for vbus
				 * re-assert
				 */
				if (mt_usb_pd_support())
					msleep(1000);
				if (upmu_is_chr_det() == false) {
					pr_debug("Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
					orderly_poweroff(true);
				}
			}
#endif

#if defined(CONFIG_POWER_EXT)
			mt_usb_disconnect();
			pr_debug("[do_chrdet_int_task] call mt_usb_disconnect() in EVB\n");
#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
			is_ta_connect = false;
			ta_check_chr_type = true;
			ta_cable_out_occur = true;
#endif
		}

		if (BMT_status.UI_SOC == 100 && BMT_status.charger_exist) {
			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = true;
			g_charging_full_reset_bat_meter = true;
		}
#if defined(CONFIG_POWER_EXT)
		mt_battery_update_status();
#endif
		wake_up_bat();
	} else
		pr_debug("[do_chrdet_int_task] battery thread not ready, will do after bettery init.\n");
}

irqreturn_t ops_chrdet_int_handler(int irq, void *dev_id)
{
	pr_debug("[Power/Battery][chrdet_bat_int_handler]....\n");

	do_chrdet_int_task();

	return IRQ_HANDLED;
}

void BAT_thread(void)
{
	if (battery_meter_initilized == false) {
		/* move from battery_probe() to decrease booting time */
		battery_meter_initial();
		BMT_status.nPercent_ZCV =
			battery_meter_get_battery_nPercent_zcv();
		battery_meter_initilized = true;
	}

	if (g_bat.usb_connect_ready)
		mt_battery_charger_detect_check();

	if (fg_battery_shutdown)
		return;

	mt_battery_GetBatteryData();

	if (fg_battery_shutdown)
		return;

	mt_battery_thermal_check();
	mt_battery_notify_check();

	if (BMT_status.charger_exist && !fg_battery_shutdown) {
		mt_battery_CheckBatteryStatus();
		mt_battery_charging_algorithm();
	}

	if (!BMT_status.charger_exist)
		bat_charger_enable(false);

	bat_charger_reset_watchdog_timer();

	mt_battery_update_status();
}

/* ///////////////////////////////////////////////////////////////
 *     Internal API
 * ///////////////////////////////////////////////////////////////
 */
int bat_thread_kthread(void *x)
{
	ktime_t ktime = ktime_set(3, 0); /* 10s, 10* 1000 ms */
	int usb_timeout = 0;

	/* Run on a process content */
	while (!fg_battery_shutdown) {
		int ret;

		/* check usb ready once at boot time */
		if (g_bat.usb_connect_ready == false) {
			while (!mt_usb_is_ready()) {
				msleep(100);
				if (usb_timeout++ > 100) {
					pr_info("wait usb config ready timeout!\n");
					break;
				}
			}
			g_bat.usb_connect_ready = true;
		}

		mutex_lock(&bat_mutex);

		if (g_bat.init_done && !battery_suspended)
			BAT_thread();

		mutex_unlock(&bat_mutex);

		ret = wait_event_hrtimeout(
			bat_thread_wq, atomic_read(&bat_thread_wakeup), ktime);
		if (ret == -ETIME)
			bat_meter_timeout = true;
		else
			atomic_dec(&bat_thread_wakeup);

		pr_debug("%s: waking up: on %s; wake_flag=%d\n", __func__,
			ret == -ETIME ? "timer" : "event",
			atomic_read(&bat_thread_wakeup));

		/* 10s, 10* 1000 ms */
		if (!fg_battery_shutdown)
			ktime = ktime_set(BAT_TASK_PERIOD, 0);

		if (chr_wake_up_bat == true) {
			/* for charger plug in/ out */
			if (g_bat.init_done)
				battery_meter_reset_aging();
			chr_wake_up_bat = false;
		}
	}
	mutex_lock(&bat_mutex);
	g_bat.down = true;
	mutex_unlock(&bat_mutex);

	return 0;
}

/*
 * This is charger interface to USB OTG code.
 * If OTG is host, charger functionality, and charger interrupt
 * must be disabled
 */
void bat_detect_set_usb_host_mode(bool usb_host_mode)
{
	mutex_lock(&bat_mutex);
	/* Don't change the charger event state
	 * if charger logic is not running
	 */
	if (g_bat.init_done) {
		if (usb_host_mode && !g_bat.usb_host_mode)
			disable_irq(g_bat.irq);
		if (!usb_host_mode && g_bat.usb_host_mode)
			enable_irq(g_bat.irq);

		g_bat.usb_host_mode = usb_host_mode;
	}
	mutex_unlock(&bat_mutex);
}
EXPORT_SYMBOL(bat_detect_set_usb_host_mode);

static int bat_setup_charger_locked(void)
{
	int ret = -EAGAIN;

#if defined(CONFIG_POWER_EXT)
	g_bat.usb_connect_ready = true;
#endif

	if (g_bat.common_init_done && g_bat.charger && !g_bat.init_done) {

		/* AP:
		 * Both common_battery and charger code are ready to go.
		 * Finalize init of common_battery.
		 */
		g_platform_boot_mode = bat_charger_get_platform_boot_mode();
		pr_debug("[BAT_probe] g_platform_boot_mode = %d\n ",
			g_platform_boot_mode);

		/* AP:
		 * MTK implementation requires that BAT_thread() be called at
		 * least once
		 * before battery event is enabled.
		 * Although this should not be necessary, we maintain
		 * compatibility
		 * until rework is complete.
		 */

		BAT_thread();
		g_bat.init_done = true;

		ret = irq_set_irq_wake(g_bat.irq, true);
		if (ret)
			pr_debug("%s: irq_set_irq_wake err = %d\n",
				__func__, ret);

		enable_irq(g_bat.irq);

		pr_debug("%s: charger setup done\n", __func__);
	}

/* if there is no external charger, we just enable detect irq */
#if defined(CONFIG_POWER_EXT)
	if (!g_bat.charger) {
		ret = irq_set_irq_wake(g_bat.irq, true);
		if (ret)
			pr_debug("%s: irq_set_irq_wake err = %d\n",
				__func__, ret);

		enable_irq(g_bat.irq);
		pr_debug("%s: no charger. just enable detect irq.\n", __func__);
	}
#endif

	return ret;
}

int bat_charger_register(CHARGING_CONTROL ctrl)
{
	int ret;

	mutex_lock(&bat_mutex);
	g_bat.charger = ctrl;
	ret = bat_setup_charger_locked();
	mutex_unlock(&bat_mutex);

	return ret;
}
EXPORT_SYMBOL(bat_charger_register);

/* ///////////////////////////////////////////////////////////////
 *     fop API
 * ///////////////////////////////////////////////////////////////
 */
static long adc_cali_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int *naram_data_addr;
	int i = 0;
	int ret = 0;
	int adc_in_data[2] = {1, 1};
	int adc_out_data[2] = {1, 1};

	mutex_lock(&bat_mutex);

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_ADC_Cali = false;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		g_ADC_Cali = false;
		/* enable calibration after setting ADC_CALI_Cal
		 * Protection
		 */
		for (i = 0; i < 14; i++) {
			if ((*(adc_cali_slop + i) == 0) ||
			    (*(adc_cali_slop + i) == 1))
				*(adc_cali_slop + i) = 1000;
		}
		break;

	case SET_ADC_CALI_Offset:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_offset, naram_data_addr, 36);
		g_ADC_Cali = false;
		break;

	case SET_ADC_CALI_Cal:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_cal, naram_data_addr, 4);
		g_ADC_Cali = true;
		if (adc_cali_cal[0] == 1)
			g_ADC_Cali = true;
		else
			g_ADC_Cali = false;

		for (i = 0; i < 1; i++)
			pr_debug("adc_cali_cal[%d] = %d\n", i,
				    *(adc_cali_cal + i));
		pr_debug(
			    "**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		user_data_addr = (int *)arg;
		/* 2*int = 2*4 */
		ret = copy_from_user(adc_in_data, user_data_addr, 8);

		if (adc_in_data[0] == 0) /* I_SENSE */
			adc_out_data[0] =
				battery_meter_get_VSense() * adc_in_data[1];
		else if (adc_in_data[0] == 1) /* BAT_SENSE */
			adc_out_data[0] = battery_meter_get_battery_voltage() *
					  adc_in_data[1];
		else if (adc_in_data[0] == 3) /* V_Charger */
			adc_out_data[0] = battery_meter_get_charger_voltage() *
					  adc_in_data[1];
		else if (adc_in_data[0] == 30) /* V_Bat_temp magic number */
			adc_out_data[0] =
				battery_meter_get_battery_temperature() *
				adc_in_data[1];
		else if (adc_in_data[0] == 66) {
			adc_out_data[0] =
				(battery_meter_get_battery_current()) / 10;

			if (battery_meter_get_battery_current_sign() == true)
				adc_out_data[0] =
					0 - adc_out_data[0]; /* charging */

		} else
			pr_debug("unknown channel(%d,%d)\n",
				adc_in_data[0], adc_in_data[1]);

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1; /* failed */
		else
			adc_out_data[1] = 0; /* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0; /* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0; /* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		pr_debug("**** unlocked_ioctl : Channel %d * %d times = %d\n",
			adc_in_data[0], adc_in_data[1], adc_out_data[0]);
		break;

	case BAT_STATUS_READ:
		user_data_addr = (int *)arg;
		ret = copy_from_user(battery_in_data, user_data_addr, 4);
		/* [0] is_CAL */
		if (g_ADC_Cali)
			battery_out_data[0] = 1;
		else
			battery_out_data[0] = 0;

		ret = copy_to_user(user_data_addr, battery_out_data, 4);
		pr_debug("**** unlocked_ioctl : CAL:%d\n", battery_out_data[0]);
		break;

	case Set_Charger_Current: /* For Factory Mode */
		user_data_addr = (int *)arg;
		ret = copy_from_user(charging_level_data, user_data_addr, 4);
		g_ftm_battery_flag = true;
		if (charging_level_data[0] == 0)
			charging_level_data[0] = CHARGE_CURRENT_70_00_MA;
		else if (charging_level_data[0] == 1)
			charging_level_data[0] = CHARGE_CURRENT_200_00_MA;
		else if (charging_level_data[0] == 2)
			charging_level_data[0] = CHARGE_CURRENT_400_00_MA;
		else if (charging_level_data[0] == 3)
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		else if (charging_level_data[0] == 4)
			charging_level_data[0] = CHARGE_CURRENT_550_00_MA;
		else if (charging_level_data[0] == 5)
			charging_level_data[0] = CHARGE_CURRENT_650_00_MA;
		else if (charging_level_data[0] == 6)
			charging_level_data[0] = CHARGE_CURRENT_700_00_MA;
		else if (charging_level_data[0] == 7)
			charging_level_data[0] = CHARGE_CURRENT_800_00_MA;
		else if (charging_level_data[0] == 8)
			charging_level_data[0] = CHARGE_CURRENT_900_00_MA;
		else if (charging_level_data[0] == 9)
			charging_level_data[0] = CHARGE_CURRENT_1000_00_MA;
		else if (charging_level_data[0] == 10)
			charging_level_data[0] = CHARGE_CURRENT_1100_00_MA;
		else if (charging_level_data[0] == 11)
			charging_level_data[0] = CHARGE_CURRENT_1200_00_MA;
		else if (charging_level_data[0] == 12)
			charging_level_data[0] = CHARGE_CURRENT_1300_00_MA;
		else if (charging_level_data[0] == 13)
			charging_level_data[0] = CHARGE_CURRENT_1400_00_MA;
		else if (charging_level_data[0] == 14)
			charging_level_data[0] = CHARGE_CURRENT_1500_00_MA;
		else if (charging_level_data[0] == 15)
			charging_level_data[0] = CHARGE_CURRENT_1600_00_MA;
		else
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;

		wake_up_bat();
		pr_debug("**** unlocked_ioctl : set_Charger_Current:%d\n",
			charging_level_data[0]);
		break;
	/* add for meta tool------------------------------- */
	case Get_META_BAT_VOL:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = BMT_status.bat_vol;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		break;
	case Get_META_BAT_SOC:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = BMT_status.UI_SOC;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		break;
	/* add bing meta tool------------------------------- */

	default:
		g_ADC_Cali = false;
		break;
	}

	mutex_unlock(&bat_mutex);

	return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations adc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_cali_ioctl,
	.open = adc_cali_open,
	.release = adc_cali_release,
};

void check_battery_exist(void)
{
#if defined(CONFIG_CONFIG_DIS_CHECK_BATTERY)
	pr_debug("[BATTERY] Disable check battery exist.\n");
#else
	u32 baton_count = 0;
	u32 i;

	for (i = 0; i < 3; i++)
		baton_count += bat_charger_get_battery_status();

	if (baton_count >= 3) {
		if ((g_platform_boot_mode == META_BOOT) ||
		    (g_platform_boot_mode == ADVMETA_BOOT) ||
		    (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
			pr_debug("[BATTERY] boot mode = %d, bypass battery check\n",
				g_platform_boot_mode);
		} else {
			pr_debug("[BATTERY] Battery is not exist, power off FAN5405 and system (%d)\n",
				baton_count);

			bat_charger_enable(false);
			bat_charger_set_platform_reset();
		}
	}
#endif
}

static void bat_parse_node(struct device_node *np, char *name, int *cust_val)
{
	u32 val;

	if (of_property_read_u32(np, name, &val) == 0) {
		(*cust_val) = (int)val;
		pr_debug("%s get %s :%d\n", __func__, name, *cust_val);
	}
}

static void init_charging_data_from_dt(struct device_node *np)
{
	bat_parse_node(np, "battery_cv_voltage",
		       &p_bat_charging_data->battery_cv_voltage);
	bat_parse_node(np, "v_charger_max",
		       &p_bat_charging_data->v_charger_max);
	bat_parse_node(np, "v_charger_min",
		       &p_bat_charging_data->v_charger_min);
	bat_parse_node(np, "max_discharge_temperature",
		       &p_bat_charging_data->max_discharge_temperature);
	bat_parse_node(np, "min_discharge_temperature",
		       &p_bat_charging_data->min_discharge_temperature);
	bat_parse_node(np, "max_charge_temperature",
		       &p_bat_charging_data->max_charge_temperature);
	bat_parse_node(np, "min_charge_temperature",
		       &p_bat_charging_data->min_charge_temperature);
	bat_parse_node(np, "use_avg_temperature",
		       &p_bat_charging_data->use_avg_temperature);
	bat_parse_node(np, "usb_charger_current",
		       &p_bat_charging_data->usb_charger_current);
	bat_parse_node(np, "ac_charger_current",
		       &p_bat_charging_data->ac_charger_current);
	bat_parse_node(np, "ac_charger_input_current",
		       &p_bat_charging_data->ac_charger_input_current);
	bat_parse_node(np, "non_std_ac_charger_current",
		       &p_bat_charging_data->non_std_ac_charger_current);
	bat_parse_node(np, "charging_host_charger_current",
		       &p_bat_charging_data->charging_host_charger_current);
	bat_parse_node(np, "apple_0_5a_charger_current",
		       &p_bat_charging_data->apple_0_5a_charger_current);
	bat_parse_node(np, "apple_1_0a_charger_current",
		       &p_bat_charging_data->apple_1_0a_charger_current);
	bat_parse_node(np, "apple_2_1a_charger_current",
		       &p_bat_charging_data->apple_2_1a_charger_current);
	bat_parse_node(np, "ta_start_battery_soc",
		       &p_bat_charging_data->ta_start_battery_soc);
	bat_parse_node(np, "ta_stop_battery_soc",
		       &p_bat_charging_data->ta_stop_battery_soc);
	bat_parse_node(np, "ta_ac_9v_input_current",
		       &p_bat_charging_data->ta_ac_9v_input_current);
	bat_parse_node(np, "ta_ac_7v_input_current",
		       &p_bat_charging_data->ta_ac_7v_input_current);
	bat_parse_node(np, "ta_ac_charging_current",
		       &p_bat_charging_data->ta_ac_charging_current);
	bat_parse_node(np, "ta_9v_support",
		       &p_bat_charging_data->ta_9v_support);
	bat_parse_node(np, "temp_pos_60_threshold",
		       &p_bat_charging_data->temp_pos_60_threshold);
	bat_parse_node(np, "temp_pos_60_thres_minus_x_degree",
		       &p_bat_charging_data->temp_pos_60_thres_minus_x_degree);
	bat_parse_node(np, "temp_pos_45_threshold",
		       &p_bat_charging_data->temp_pos_45_threshold);
	bat_parse_node(np, "temp_pos_45_thres_minus_x_degree",
		       &p_bat_charging_data->temp_pos_45_thres_minus_x_degree);
	bat_parse_node(np, "temp_pos_10_threshold",
		       &p_bat_charging_data->temp_pos_10_threshold);
	bat_parse_node(np, "temp_pos_10_thres_plus_x_degree",
		       &p_bat_charging_data->temp_pos_10_thres_plus_x_degree);
	bat_parse_node(np, "temp_pos_0_threshold ",
		       &p_bat_charging_data->temp_pos_0_threshold);
	bat_parse_node(np, "temp_pos_0_thres_plus_x_degree",
		       &p_bat_charging_data->temp_pos_0_thres_plus_x_degree);
	bat_parse_node(np, "temp_neg_10_threshold",
		       &p_bat_charging_data->temp_neg_10_threshold);
	bat_parse_node(np, "temp_neg_10_thres_plus_x_degree",
		       &p_bat_charging_data->temp_neg_10_thres_plus_x_degree);
	bat_parse_node(
		np, "jeita_temp_above_pos_60_cv_voltage ",
		&p_bat_charging_data->jeita_temp_above_pos_60_cv_voltage);
	bat_parse_node(
		np, "jeita_temp_pos_45_to_pos_60_cv_voltage",
		&p_bat_charging_data->jeita_temp_pos_45_to_pos_60_cv_voltage);
	bat_parse_node(
		np, "jeita_temp_pos_10_to_pos_45_cv_voltage",
		&p_bat_charging_data->jeita_temp_pos_10_to_pos_45_cv_voltage);
	bat_parse_node(
		np, "jeita_temp_pos_0_to_pos_10_cv_voltage",
		&p_bat_charging_data->jeita_temp_pos_0_to_pos_10_cv_voltage);
	bat_parse_node(
		np, "jeita_temp_neg_10_to_pos_0_cv_voltage",
		&p_bat_charging_data->jeita_temp_neg_10_to_pos_0_cv_voltage);
	bat_parse_node(
		np, "jeita_temp_below_neg_10_cv_voltage",
		&p_bat_charging_data->jeita_temp_below_neg_10_cv_voltage);
}

static int battery_probe(struct platform_device *pdev)
{
	struct class_device *class_dev = NULL;
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_debug("******** battery driver probe!! ********\n");

	/* AP:
	 * Use PMIC events as interrupts through kernel IRQ API.
	 */
	atomic_set(&bat_thread_wakeup, 0);

	g_bat.irq = platform_get_irq(pdev, 0);
	if (g_bat.irq <= 0)
		return -EINVAL;

	p_bat_charging_data =
		(struct mt_battery_charging_custom_data *)dev_get_platdata(dev);
	if (!p_bat_charging_data) {
		pr_debug("%s: no platform data. replace with default settings.\n",
			__func__);
		p_bat_charging_data = &default_charging_data;

		/* populate property here */
		init_charging_data_from_dt(pdev->dev.of_node);
	}

	irq_set_status_flags(g_bat.irq, IRQ_NOAUTOEN);
	ret = request_threaded_irq(g_bat.irq, NULL, ops_chrdet_int_handler,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
			"ops_mt6397_chrdet", pdev);
	if (ret) {
		pr_debug("%s: request_threaded_irq err = %d\n", __func__, ret);
		return ret;
	}

	/* Integrate with NVRAM */
	ret = alloc_chrdev_region(&adc_cali_devno, 0, 1, ADC_CALI_DEVNAME);
	if (ret)
		pr_debug("Error: Can't Get Major number for adc_cali\n");
	adc_cali_cdev = cdev_alloc();
	adc_cali_cdev->owner = THIS_MODULE;
	adc_cali_cdev->ops = &adc_cali_fops;
	ret = cdev_add(adc_cali_cdev, adc_cali_devno, 1);
	if (ret)
		pr_debug("adc_cali Error: cdev_add\n");
	adc_cali_major = MAJOR(adc_cali_devno);
	adc_cali_class = class_create(THIS_MODULE, ADC_CALI_DEVNAME);
	class_dev = (struct class_device *)device_create(
		adc_cali_class, NULL, adc_cali_devno, NULL, ADC_CALI_DEVNAME);
	pr_debug("[BAT_probe] adc_cali prepare : done !!\n ");

	wakeup_source_init(&battery_suspend_lock, "battery suspend wakelock");
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	wakeup_source_init(&TA_charger_suspend_lock,
		"TA charger suspend wakelock");
#endif

	/* Integrate with Android Battery Service */
	ac_main.psy = power_supply_register(dev, &ac_main.psd, NULL);
	if (IS_ERR(ac_main.psy)) {
		pr_debug("[BAT_probe] power_supply_register AC Fail !!\n");
		ret = PTR_ERR(ac_main.psy);
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register AC Success !!\n");

	usb_main.psy = power_supply_register(dev, &usb_main.psd, NULL);
	if (IS_ERR(usb_main.psy)) {
		pr_debug("[BAT_probe] power_supply_register USB Fail !!\n");
		ret = PTR_ERR(usb_main.psy);
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register USB Success !!\n");

	battery_main.psy = power_supply_register(dev, &battery_main.psd, NULL);
	if (IS_ERR(battery_main.psy)) {
		pr_debug("[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(battery_main.psy);
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register Battery Success !!\n");

#if !defined(CONFIG_POWER_EXT)
	/* For EM */
	{
		int ret_device_file = 0;

		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Charger_Voltage);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_0_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_1_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_2_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_3_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_4_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_5_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_6_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_7_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_8_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_9_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_10_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_11_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_12_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_13_Slope);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_0_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_1_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_2_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_3_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_4_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_5_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_6_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_7_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_8_Offset);
		ret_device_file =
			device_create_file(dev, &dev_attr_ADC_Channel_9_Offset);

		ret_device_file = device_create_file(
			dev, &dev_attr_ADC_Channel_10_Offset);
		ret_device_file = device_create_file(
			dev, &dev_attr_ADC_Channel_11_Offset);
		ret_device_file = device_create_file(
			dev, &dev_attr_ADC_Channel_12_Offset);
		ret_device_file = device_create_file(
			dev, &dev_attr_ADC_Channel_13_Offset);
		ret_device_file = device_create_file(
			dev, &dev_attr_ADC_Channel_Is_Calibration);

		ret_device_file =
			device_create_file(dev, &dev_attr_Power_On_Voltage);
		ret_device_file =
			device_create_file(dev, &dev_attr_Power_Off_Voltage);
		ret_device_file =
			device_create_file(dev, &dev_attr_Charger_TopOff_Value);
		ret_device_file = device_create_file(
			dev, &dev_attr_FG_Battery_CurrentConsumption);
		ret_device_file =
			device_create_file(dev, &dev_attr_FG_SW_CoulombCounter);
		ret_device_file =
			device_create_file(dev, &dev_attr_Charging_CallState);
		ret_device_file =
			device_create_file(dev, &dev_attr_Charging_Enable);
		ret_device_file = device_create_file(
			dev, &dev_attr_Custom_Charging_Current);
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
		ret_device_file =
			device_create_file(dev, &dev_attr_Pump_Express);
#endif
	}

	/* Initialization BMT Struct */
	BMT_status.bat_exist = true;      /* phone must have battery */
	BMT_status.charger_exist = false; /* for default, no charger */
	BMT_status.bat_vol = 0;
	BMT_status.ICharging = 0;
	BMT_status.temperature = 0;
	BMT_status.charger_vol = 0;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.SOC = 0;
	BMT_status.UI_SOC = -1;

	BMT_status.bat_charging_state = CHR_PRE;
	BMT_status.bat_in_recharging_state = false;
	BMT_status.bat_full = false;
	BMT_status.nPercent_ZCV = 0;
	BMT_status.nPrecent_UI_SOC_check_point =
		battery_meter_get_battery_nPercent_UI_SOC();

	kthread_run(bat_thread_kthread, NULL, "bat_thread_kthread");
	pr_debug("[battery_probe] bat_thread_kthread Done\n");

#endif
	g_bat_init_flag = true;

	mutex_lock(&bat_mutex);
	g_bat.common_init_done = true;
	bat_setup_charger_locked();
	mutex_unlock(&bat_mutex);

	return 0;
}

static int battery_remove(struct platform_device *dev)
{
	pr_debug("******** battery driver remove!! ********\n");

	return 0;
}

static void battery_shutdown(struct platform_device *pdev)
{
#if !defined(CONFIG_POWER_EXT)
	int count = 0;
#endif
	pr_debug("******** battery driver shutdown!! ********\n");

	disable_irq(g_bat.irq);

	mutex_lock(&bat_mutex);
	fg_battery_shutdown = true;
	wake_up_bat_update_meter();

#if !defined(CONFIG_POWER_EXT)
	while (!g_bat.down && count < 5) {
		mutex_unlock(&bat_mutex);
		msleep(20);
		count++;
		mutex_lock(&bat_mutex);
	}

	if (!g_bat.down)
		pr_debug("%s: failed to terminate battery thread\n", __func__);
#endif
	/* turn down interrupt thread and wakeup ability */

	if (g_bat.init_done)
		irq_set_irq_wake(g_bat.irq, false);
	free_irq(g_bat.irq, pdev);

	mutex_unlock(&bat_mutex);
}

static int battery_suspend(struct platform_device *dev, pm_message_t state)
{
	mutex_lock(&bat_mutex);
	battery_suspended = true;
	mutex_unlock(&bat_mutex);

	return 0;
}

static int battery_resume(struct platform_device *dev)
{
	battery_suspended = false;
	g_refresh_ui_soc = true;
	if (bat_charger_is_pcm_timer_trigger())
		wake_up_bat_update_meter();

	return 0;
}

/* ///////////////////////////////////////////////////////////////
 *     Battery Notify API
 * ///////////////////////////////////////////////////////////////
 */
#if !defined(CONFIG_POWER_EXT)
static ssize_t show_BatteryNotify(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[Battery] show_BatteryNotify : %x\n",
		g_BatteryNotifyCode);

	return sprintf(buf, "%u\n", g_BatteryNotifyCode);
}

static ssize_t store_BatteryNotify(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int reg_BatteryNotifyCode, ret;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 0, &reg_BatteryNotifyCode);
		if (ret) {
			pr_debug("wrong format!\n");
			return size;
		}
		g_BatteryNotifyCode = reg_BatteryNotifyCode;
		pr_debug("[Battery] store code : %x\n",
			    g_BatteryNotifyCode);
	}
	return size;
}

static DEVICE_ATTR(BatteryNotify, 0664,
	show_BatteryNotify, store_BatteryNotify);

static ssize_t show_BN_TestMode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[Battery] show_BN_TestMode : %x\n",
		g_BN_TestMode);
	return sprintf(buf, "%u\n", g_BN_TestMode);
}

static ssize_t store_BN_TestMode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int reg_BN_TestMode, ret;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 0, &reg_BN_TestMode);
		if (ret) {
			pr_debug("wrong format!\n");
			return size;
		}
		g_BN_TestMode = reg_BN_TestMode;
		pr_debug(
			"[Battery] store g_BN_TestMode : %x\n",
			g_BN_TestMode);
	}
	return size;
}

static DEVICE_ATTR(BN_TestMode, 0664,
	show_BN_TestMode, store_BN_TestMode);
#endif

/* ///////////////////////////////////////////////////////////////
 *     platform_driver API
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t battery_cmd_write(struct file *file,
	const char *buffer, size_t count, loff_t *data)
{
	int len = 0, bat_tt_enable = 0;
	int bat_thr_test_mode = 0, bat_thr_test_value = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d", &bat_tt_enable, &bat_thr_test_mode,
		   &bat_thr_test_value) == 3) {
		g_battery_thermal_throttling_flag = bat_tt_enable;
		battery_cmd_thermal_test_mode = bat_thr_test_mode;
		battery_cmd_thermal_test_mode_value = bat_thr_test_value;

		pr_debug(
			"bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			g_battery_thermal_throttling_flag,
			battery_cmd_thermal_test_mode,
			battery_cmd_thermal_test_mode_value);

		return count;
	}

	pr_debug(
		"  bad argument, echo [bat_tt_enable] [bat_thr_test_mode] [bat_thr_test_value] > battery_cmd\n");

	return -EINVAL;
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		"=> g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		g_battery_thermal_throttling_flag,
		battery_cmd_thermal_test_mode,
		battery_cmd_thermal_test_mode_value);
	return 0;
}

static int proc_utilization_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations battery_cmd_proc_fops = {
	.open = proc_utilization_open,
	.read = seq_read,
	.write = battery_cmd_write,
};

static ssize_t current_cmd_write(struct file *file,
	const char *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[32];
	int cmd_current_unlimited = 0, cmd_discharging = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &cmd_current_unlimited, &cmd_discharging) ==
	    2) {

		if (cmd_current_unlimited) {
			g_custom_charging_current =
				p_bat_charging_data->ac_charger_current;
			pr_debug("custom charging current = %d\n",
				    g_custom_charging_current);
		} else {
			g_custom_charging_current = -1;
			pr_debug("custom charging current = %d\n",
				    g_custom_charging_current);
		}

		if (cmd_discharging == 1)
			g_cmd_hold_charging = true;
		else
			g_cmd_hold_charging = false;

		wake_up_bat_update_meter();

		return count;
	}

	pr_debug("bad argument, echo [usb_limit] [chr_enable] > current_cmd\n");

	return -EINVAL;
}

static int current_cmd_read(struct seq_file *m, void *v)
{
	pr_debug("g_custom_charging_current=%d g_cmd_hold_charging=%d\n",
		 g_custom_charging_current, g_cmd_hold_charging);

	return 0;
}

static int proc_utilization_open_cur_stop(struct inode *inode,
			struct file *file)
{
	return single_open(file, current_cmd_read, NULL);
}

static const struct file_operations current_cmd_proc_fops = {
	.open = proc_utilization_open_cur_stop,
	.read = seq_read,
	.write = current_cmd_write,
};

static int mt_batteryNotify_probe(struct platform_device *pdev)
{
#if defined(CONFIG_POWER_EXT)
#else
	struct device *dev = &pdev->dev;
	int ret_device_file = 0;
	/* struct proc_dir_entry *entry = NULL; */
	struct proc_dir_entry *battery_dir = NULL;

	pr_debug("******** mt_batteryNotify_probe!! ********\n");

	ret_device_file = device_create_file(dev, &dev_attr_BatteryNotify);
	ret_device_file = device_create_file(dev, &dev_attr_BN_TestMode);

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		pr_debug("[%s]: mkdir /proc/mtk_battery_cmd failed\n",
			 __func__);
	} else {
		proc_create("battery_cmd", 0644, battery_dir,
			&battery_cmd_proc_fops);
		pr_debug("proc_create battery_cmd_proc_fops\n");

		proc_create("current_cmd", 0644, battery_dir,
			&current_cmd_proc_fops);
		pr_debug("proc_create current_cmd_proc_fops\n");
	}

	pr_debug("******** mtk_battery_cmd!! ********\n");
#endif
	return 0;
}

#if 0 /* move to board-common-battery.c */
struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};
#endif

static struct platform_driver battery_driver = {
	.probe = battery_probe,
	.remove = battery_remove,
	.shutdown = battery_shutdown,
	/* #ifdef CONFIG_PM */
	.suspend = battery_suspend,
	.resume = battery_resume,
	/* #endif */
	.driver = {

			.name = "battery",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(mt_battery_common_id),
#endif
		},
};

struct platform_device MT_batteryNotify_device = {
	.name = "mt-battery", .id = -1,
};

static struct platform_driver mt_batteryNotify_driver = {
	.probe = mt_batteryNotify_probe,
	.driver = {

			.name = "mt-battery",
		},
};

static int __init battery_init(void)
{
	int ret;

#if 0 /* move to board-common-battery.c */
	ret = platform_device_register(&battery_device);
	if (ret) {
		pr_debug("[BAT] Unable to device register(%d)\n", ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&battery_driver);
	if (ret) {
		pr_debug("****[battery_driver] Unable to register driver (%d)\n",
			ret);
		return ret;
	}
	/* battery notofy UI */
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		pr_debug("****[mt_batteryNotify] Unable to device register(%d)\n",
			ret);
		return ret;
	}
	ret = platform_driver_register(&mt_batteryNotify_driver);
	if (ret) {
		pr_debug("****[mt_batteryNotify] Unable to register driver (%d)\n",
			ret);
		return ret;
	}

	pr_debug("****[battery_driver] Initialization : DONE !!\n");
	return 0;
}

static void __exit battery_exit(void)
{
}

/* move to late_initcall to ensure battery_meter probe first */
/* module_init(battery_init); */
late_initcall(battery_init);
module_exit(battery_exit);

MODULE_AUTHOR("Oscar Liu");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
