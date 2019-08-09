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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *    battery_common.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of mt6323 Battery charging algorithm
 *   and the Anroid Battery service for updating the battery status
 *
 * Author:
 * -------
 * Oscar Liu
 *
 ****************************************************************************/
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <linux/suspend.h>
#include <linux/reboot.h>


#include <linux/scatterlist.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/irq.h>

#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_rtc.h>

#include <mach/mt_charging.h>
#include <mt-plat/upmu_common.h>

#include <mt-plat/charging.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mach/mt_battery_meter.h>
#include <mach/mt_charging.h>
#include <mach/mt_pmic.h>
#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) || defined(CONFIG_ARCH_MT6753)
#include <mach/mt_pmic_wrap.h>
#endif

#include "mtk_pep_intf.h"
#include "mtk_pep20_intf.h"
#if defined(CONFIG_ONTIM_POWER_DRIVER)
#include <mach/mt_charging_sel_intf.h>
#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) || defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
#ifndef PUMP_EXPRESS_SERIES
#define PUMP_EXPRESS_SERIES
#endif
#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT)
#ifndef PUMP_EXPRESS_SERIES
#define PUMP_EXPRESS_SERIES
#endif
#endif

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
#include <mt-plat/diso.h>
#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
#include <mach/mt_pe.h>
#endif
/* ////////////////////////////////////////////////////////////////////////// */
/* Battery Logging Entry */
/* ////////////////////////////////////////////////////////////////////////// */
int Enable_BATDRV_LOG = BAT_LOG_CRTI;

/* ///////////////////////////////////////////////////////////////////////// */
/* // Smart Battery Structure */
/* ///////////////////////////////////////////////////////////////////////// */
PMU_ChargerStruct BMT_status;
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
DISO_ChargerStruct DISO_data;
/* Debug Msg */
static char *DISO_state_s[8] = {
	"IDLE",
	"OTG_ONLY",
	"USB_ONLY",
	"USB_WITH_OTG",
	"DC_ONLY",
	"DC_WITH_OTG",
	"DC_WITH_USB",
	"DC_USB_OTG",
};
#endif

/*
 * Thermal related flags
 */
int g_battery_thermal_throttling_flag = 3;
/*  0:nothing,
 *	1:enable batTT&chrTimer,
 *	2:disable batTT&chrTimer,
 *	3:enable batTT,
 *	disable chrTimer
 */
int battery_cmd_thermal_test_mode;
int battery_cmd_thermal_test_mode_value;
int g_battery_tt_check_flag;
/* 0:default enable check batteryTT, 1:default disable check batteryTT */


/*
 *  Global Variable
 */
bool gDisableGM;
struct wakeup_source *battery_suspend_lock;
struct wakeup_source *battery_fg_lock;
CHARGING_CONTROL battery_charging_control;
unsigned int g_BatteryNotifyCode;
unsigned int g_BN_TestMode;
bool g_bat_init_flag;
unsigned int g_call_state = CALL_IDLE;
bool g_charging_full_reset_bat_meter;
int g_platform_boot_mode;
struct timespec g_bat_time_before_sleep;
int g_smartbook_update;
int cable_in_uevent;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
bool g_vcdt_irq_delay_flag;
#endif

bool skip_battery_update;

unsigned int g_batt_temp_status = TEMP_POS_NORMAL;


bool battery_suspended;
/* #ifdef MTK_ENABLE_AGING_ALGORITHM
 * extern unsigned int suspend_time;
 * #endif
 */

#if defined(CUST_SYSTEM_OFF_VOLTAGE)
#define SYSTEM_OFF_VOLTAGE CUST_SYSTEM_OFF_VOLTAGE
#endif

#if !defined(CONFIG_POWER_EXT)
static unsigned int V_0Percent_Tracking = V_0PERCENT_TRACKING;
#endif

struct battery_custom_data batt_cust_data;

/*
 * Integrate with NVRAM
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

int adc_cali_slop[14] = {
	1000, 1000, 1000, 1000, 1000, 1000, 1000,
	1000, 1000, 1000, 1000, 1000, 1000, 1000
};

int adc_cali_offset[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int adc_cali_cal[1] = { 0 };
int battery_in_data[1] = { 0 };
int battery_out_data[1] = { 0 };
int charging_level_data[1] = { 0 };

bool g_ADC_Cali;
bool g_ftm_battery_flag;
#if !defined(CONFIG_POWER_EXT)
static int g_wireless_state;
#endif

/*
 *  Thread related
 */
#define BAT_MS_TO_NS(x) (x * 1000 * 1000)
static bool bat_thread_timeout;
static bool chr_wake_up_bat;	/* charger in/out to wake up battery thread */
static bool fg_wake_up_bat;
static bool bat_meter_timeout;
static DEFINE_MUTEX(bat_mutex);
static DEFINE_MUTEX(charger_type_mutex);
static DECLARE_WAIT_QUEUE_HEAD(bat_thread_wq);
static struct hrtimer charger_hv_detect_timer;
static struct task_struct *charger_hv_detect_thread;
static bool charger_hv_detect_flag;
static DECLARE_WAIT_QUEUE_HEAD(charger_hv_detect_waiter);
static struct hrtimer battery_kthread_timer;
bool g_battery_soc_ready;
/*extern BOOL bat_spm_timeout;
 *extern unsigned int _g_bat_sleep_total_time;
 */

/*
 * FOR ADB CMD
 */
/* Dual battery */
int g_status_smb = POWER_SUPPLY_STATUS_DISCHARGING;
int g_capacity_smb = 50;
int g_present_smb;
/* ADB charging CMD */
static int cmd_discharging = -1;
static int adjust_power = -1;
static int suspend_discharging = -1;

#if !defined(CONFIG_POWER_EXT)
static int is_uisoc_ever_100;
#endif
#if defined(CONFIG_ONTIM_POWER_DRIVER)
bool chargin_hw_init_done;
#endif
/* ////////////////////////////////////////////////////////////////////////////// */
/* FOR ANDROID BATTERY SERVICE */
/* ////////////////////////////////////////////////////////////////////////// */

struct wireless_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int WIRELESS_ONLINE;
};

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
	int BAT_batt_vol;
	int BAT_batt_temp;
	/* Add for EM */
	int BAT_TemperatureR;
	int BAT_TempBattVoltage;
	int BAT_InstatVolt;
	int BAT_BatteryAverageCurrent;
	int BAT_BatterySenseVoltage;
	int BAT_ISenseVoltage;
	int BAT_ChargerVoltage;
	int BAT_CURRENT_NOW;
	/* Dual battery */
	int status_smb;
	int capacity_smb;
	int present_smb;
	int adjust_power;
};

static enum power_supply_property wireless_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_COUNTER
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	/* Add for Battery Service */
	POWER_SUPPLY_PROP_batt_vol,
	POWER_SUPPLY_PROP_batt_temp,
	/* Add for EM */
	POWER_SUPPLY_PROP_TemperatureR,
	POWER_SUPPLY_PROP_TempBattVoltage,
	POWER_SUPPLY_PROP_InstatVolt,
	POWER_SUPPLY_PROP_BatteryAverageCurrent,
	POWER_SUPPLY_PROP_BatterySenseVoltage,
	POWER_SUPPLY_PROP_ISenseVoltage,
	POWER_SUPPLY_PROP_ChargerVoltage,
	/* Dual battery */
	POWER_SUPPLY_PROP_status_smb,
	POWER_SUPPLY_PROP_capacity_smb,
	POWER_SUPPLY_PROP_present_smb,
	/* ADB CMD Discharging */
	POWER_SUPPLY_PROP_adjust_power,
};

/*void check_battery_exist(void);*/
void charging_suspend_enable(void)
{
	unsigned int charging_enable = true;

	suspend_discharging = 0;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
}

void charging_suspend_disable(void)
{
	unsigned int charging_enable = false;

	suspend_discharging = 1;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
}

int read_tbat_value(void)
{
	return BMT_status.temperature;
}

int get_charger_detect_status(void)
{
	bool chr_status;

	battery_charging_control(
			CHARGING_CMD_GET_CHARGER_DET_STATUS, &chr_status);
	return chr_status;
}

#if defined(CONFIG_MTK_POWER_EXT_DETECT)
bool bat_is_ext_power(void)
{
	bool pwr_src = 0;

	battery_charging_control(CHARGING_CMD_GET_POWER_SOURCE, &pwr_src);
	battery_log(BAT_LOG_FULL,
			"[BAT_IS_EXT_POWER] is_ext_power = %d\n", pwr_src);
	return pwr_src;
}
#endif
/* ////////////////////////////////////////////////////////////////////////// */
/* // PMIC PCHR Related APIs */
/* ////////////////////////////////////////////////////////////////////////// */
bool __attribute__ ((weak)) mt_usb_is_device(void)
{
	return 1;
}

bool upmu_is_chr_det(void)
{
#if !defined(CONFIG_POWER_EXT)
	unsigned int tmp32;
#endif
#if defined(CONFIG_ONTIM_POWER_DRIVER)
	int idx = 0;

	if (battery_charging_control == NULL) {
		for (idx = 0; idx < sizeof(charger_candidate_func)/sizeof(struct charger_candidate_table); idx++) {
			if (charger_candidate_func[idx].exist_fun() == 0) {
				battery_log(BAT_LOG_CRTI, "charger %s found\n", charger_candidate_func[idx].name);
				battery_charging_control = charger_candidate_func[idx].chr_ctrl_intf;
				break;
			}
		}

		if (battery_charging_control == NULL)
			battery_log(BAT_LOG_CRTI, "can't find any charger driver\n");
	}
#else
	if (battery_charging_control == NULL)
		battery_charging_control = chr_control_interface;
#endif
#if defined(CONFIG_POWER_EXT)
	/* return true; */
	return get_charger_detect_status();
#else
	if (suspend_discharging == 1)
		return false;

	tmp32 = get_charger_detect_status();

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power())
		return tmp32;
#endif

	if (tmp32 == 0)
		return false;
	/*else { */
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (mt_usb_is_device()) {
		battery_log(BAT_LOG_FULL,
		    "[upmu_is_chr_det] Charger exist and USB is not host\n");

		return true;
	}			/*else { */
	battery_log(BAT_LOG_CRTI,
			"[upmu_is_chr_det] Charger exist but USB is host\n");

	return false;
	/*} */
#else
	return true;
#endif
	/*} */
#endif
}
EXPORT_SYMBOL(upmu_is_chr_det);


void wake_up_bat(void)
{
	battery_log(BAT_LOG_FULL, "[BATTERY] wake_up_bat. \r\n");

	chr_wake_up_bat = true;
	bat_thread_timeout = true;
#ifdef MTK_ENABLE_AGING_ALGORITHM
	suspend_time = 0;
#endif
	battery_meter_reset_sleep_time();

	wake_up(&bat_thread_wq);
}
EXPORT_SYMBOL(wake_up_bat);


#ifdef FG_BAT_INT
void wake_up_bat2(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] wake_up_bat2. \r\n");

	__pm_stay_awake(battery_fg_lock);
	fg_wake_up_bat = true;
	bat_thread_timeout = true;
#ifdef MTK_ENABLE_AGING_ALGORITHM
	suspend_time = 0;
#endif
	battery_meter_reset_sleep_time();
	wake_up(&bat_thread_wq);
}
EXPORT_SYMBOL(wake_up_bat2);
#endif				/* #ifdef FG_BAT_INT */

void wake_up_bat3(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] wake_up_bat3. \r\n");

	bat_thread_timeout = true;
#ifdef MTK_ENABLE_AGING_ALGORITHM
	suspend_time = 0;
#endif
	battery_meter_reset_sleep_time();
	wake_up(&bat_thread_wq);
}
EXPORT_SYMBOL(wake_up_bat3);




static ssize_t bat_log_write(struct file *filp,
		const char __user *buff, size_t len, loff_t *data)
{
	char proc_bat_data;

	if ((len <= 0) || copy_from_user(&proc_bat_data, buff, 1)) {
		battery_log(BAT_LOG_FULL, "bat_log_write error.\n");
		return -EFAULT;
	}

	if (proc_bat_data == '1') {
		battery_log(BAT_LOG_CRTI,
				"enable battery driver log system\n");
		Enable_BATDRV_LOG = 1;
	} else if (proc_bat_data == '2') {
		battery_log(BAT_LOG_CRTI,
				"enable battery driver log system:2\n");
		Enable_BATDRV_LOG = 2;
	} else {
		battery_log(BAT_LOG_CRTI,
				"Disable battery driver log system\n");
		Enable_BATDRV_LOG = 0;
	}

	return len;
}

static const struct file_operations bat_proc_fops = {
	.write = bat_log_write,
};

int init_proc_log(void)
{
	int ret = 0;

#if 1
	proc_create("batdrv_log", 0644, NULL, &bat_proc_fops);
	battery_log(BAT_LOG_CRTI, "proc_create bat_proc_fops\n");
#else
	proc_entry = create_proc_entry("batdrv_log", 0644, NULL);

	if (proc_entry == NULL) {
		ret = -ENOMEM;
		battery_log(BAT_LOG_FULL,
				"init_proc_log: Couldn't create proc entry\n");
	} else {
		proc_entry->write_proc = bat_log_write;
		battery_log(BAT_LOG_CRTI, "init_proc_log loaded.\n");
	}
#endif

	return ret;
}


static int wireless_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct wireless_data *data = container_of(psy->desc,
						struct wireless_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->WIRELESS_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int ac_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
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
		enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct usb_data *data = container_of(psy->desc, struct usb_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#if defined(CONFIG_POWER_EXT)
		/* #if 0 */
		data->USB_ONLINE = 1;
		val->intval = data->USB_ONLINE;
#else
#if defined(CONFIG_MTK_POWER_EXT_DETECT)
		if (true == bat_is_ext_power())
			data->USB_ONLINE = 1;
#endif
		val->intval = data->USB_ONLINE;
#endif
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = battery_meter_get_QMAX25() * 1000;
		/* QMAX from battery, ma to ua */
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int battery_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct battery_data *data = container_of(psy->desc,
						struct battery_data, psd);

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
	case POWER_SUPPLY_PROP_batt_vol:
		val->intval = data->BAT_batt_vol * 1000;
		break;
	case POWER_SUPPLY_PROP_batt_temp:
		val->intval = data->BAT_batt_temp;
		break;
	case POWER_SUPPLY_PROP_TemperatureR:
		val->intval = data->BAT_TemperatureR;
		break;
	case POWER_SUPPLY_PROP_TempBattVoltage:
		val->intval = data->BAT_TempBattVoltage;
		break;
	case POWER_SUPPLY_PROP_InstatVolt:
		val->intval = data->BAT_InstatVolt;
		break;
	case POWER_SUPPLY_PROP_BatteryAverageCurrent:
		val->intval = data->BAT_BatteryAverageCurrent;
		break;
	case POWER_SUPPLY_PROP_BatterySenseVoltage:
		val->intval = data->BAT_BatterySenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ISenseVoltage:
		val->intval = data->BAT_ISenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ChargerVoltage:
		val->intval = data->BAT_ChargerVoltage;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = data->BAT_CURRENT_NOW; /* charge_current */
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 3000000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = battery_meter_get_QMAX25() * 1000;
		/* QMAX from battery, ma to ua */
		break;
		/* Dual battery */
	case POWER_SUPPLY_PROP_status_smb:
		val->intval = data->status_smb;
		break;
	case POWER_SUPPLY_PROP_capacity_smb:
		val->intval = data->capacity_smb;
		break;
	case POWER_SUPPLY_PROP_present_smb:
		val->intval = data->present_smb;
		break;
	case POWER_SUPPLY_PROP_adjust_power:
		val->intval = data->adjust_power;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* wireless_data initialization */
static struct wireless_data wireless_main = {
	.psd = {
		.name = "wireless",
		.type = POWER_SUPPLY_TYPE_WIRELESS,
		.properties = wireless_props,
		.num_properties = ARRAY_SIZE(wireless_props),
		.get_property = wireless_get_property,
		},
	.WIRELESS_ONLINE = 0,
};

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
	.BAT_batt_vol = 4200,
	.BAT_batt_temp = 22,
	/* Dual battery */
	.status_smb = POWER_SUPPLY_STATUS_DISCHARGING,
	.capacity_smb = 50,
	.present_smb = 0,
	/* ADB CMD discharging */
	.adjust_power = -1,
#else
	.BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
#if defined(PUMP_EXPRESS_SERIES)
	.BAT_CAPACITY = -1,
#else
	.BAT_CAPACITY = 50,
#endif
	.BAT_batt_vol = 0,
	.BAT_batt_temp = 0,
	/* Dual battery */
	.status_smb = POWER_SUPPLY_STATUS_DISCHARGING,
	.capacity_smb = 50,
	.present_smb = 0,
	/* ADB CMD discharging */
	.adjust_power = -1,
#endif
};


#if !defined(CONFIG_POWER_EXT)
/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Charger_Voltage */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Charger_Voltage(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "[EM] show_ADC_Charger_Voltage : %d\n",
			BMT_status.charger_vol);
	return sprintf(buf, "%d\n", BMT_status.charger_vol);
}

static ssize_t store_ADC_Charger_Voltage(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Charger_Voltage, 0664,
		show_ADC_Charger_Voltage, store_ADC_Charger_Voltage);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_0_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_0_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 0));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_0_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Slope, 0664,
		show_ADC_Channel_0_Slope, store_ADC_Channel_0_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_1_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_1_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 1));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_1_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Slope, 0664,
		show_ADC_Channel_1_Slope, store_ADC_Channel_1_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_2_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_2_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 2));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_2_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Slope, 0664,
		show_ADC_Channel_2_Slope, store_ADC_Channel_2_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_3_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_3_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 3));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_3_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Slope, 0664,
		show_ADC_Channel_3_Slope, store_ADC_Channel_3_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_4_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_4_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 4));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_4_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Slope, 0664,
		show_ADC_Channel_4_Slope, store_ADC_Channel_4_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_5_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_5_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 5));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_5_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Slope, 0664,
		show_ADC_Channel_5_Slope, store_ADC_Channel_5_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_6_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_6_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 6));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_6_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Slope, 0664,
		show_ADC_Channel_6_Slope, store_ADC_Channel_6_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_7_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_7_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 7));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_7_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Slope, 0664,
		show_ADC_Channel_7_Slope, store_ADC_Channel_7_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_8_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_8_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 8));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_8_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Slope, 0664,
		show_ADC_Channel_8_Slope, store_ADC_Channel_8_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_9_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_9_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 9));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_9_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Slope, 0664,
		show_ADC_Channel_9_Slope, store_ADC_Channel_9_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_10_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_10_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 10));
	battery_log(BAT_LOG_CRTI,
			"[EM] ADC_Channel_10_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Slope, 0664, show_ADC_Channel_10_Slope,
		   store_ADC_Channel_10_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_11_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_11_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 11));
	battery_log(BAT_LOG_CRTI,
			"[EM] ADC_Channel_11_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Slope, 0664, show_ADC_Channel_11_Slope,
		   store_ADC_Channel_11_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_12_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_12_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 12));
	battery_log(BAT_LOG_CRTI,
			"[EM] ADC_Channel_12_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Slope, 0664, show_ADC_Channel_12_Slope,
		   store_ADC_Channel_12_Slope);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_13_Slope */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_13_Slope(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 13));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_13_Slope: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Slope(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Slope, 0664, show_ADC_Channel_13_Slope,
		   store_ADC_Channel_13_Slope);


/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_0_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_0_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 0));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_0_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Offset, 0664, show_ADC_Channel_0_Offset,
		   store_ADC_Channel_0_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_1_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_1_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 1));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_1_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Offset, 0664, show_ADC_Channel_1_Offset,
		   store_ADC_Channel_1_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_2_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_2_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 2));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_2_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Offset, 0664, show_ADC_Channel_2_Offset,
		   store_ADC_Channel_2_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_3_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_3_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 3));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_3_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Offset, 0664, show_ADC_Channel_3_Offset,
		   store_ADC_Channel_3_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_4_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_4_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 4));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_4_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Offset, 0664, show_ADC_Channel_4_Offset,
		   store_ADC_Channel_4_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_5_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_5_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 5));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_5_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Offset, 0664, show_ADC_Channel_5_Offset,
		   store_ADC_Channel_5_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_6_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_6_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 6));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_6_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Offset, 0664, show_ADC_Channel_6_Offset,
		   store_ADC_Channel_6_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_7_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_7_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 7));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_7_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Offset, 0664, show_ADC_Channel_7_Offset,
		   store_ADC_Channel_7_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_8_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_8_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 8));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_8_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Offset, 0664, show_ADC_Channel_8_Offset,
		   store_ADC_Channel_8_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_9_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_9_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 9));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_9_Offset: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Offset, 0664, show_ADC_Channel_9_Offset,
		   store_ADC_Channel_9_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_10_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_10_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 10));
	battery_log(BAT_LOG_CRTI,
			"[EM] ADC_Channel_10_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Offset, 0664, show_ADC_Channel_10_Offset,
		   store_ADC_Channel_10_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_11_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_11_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 11));
	battery_log(BAT_LOG_CRTI,
			"[EM] ADC_Channel_11_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Offset, 0664, show_ADC_Channel_11_Offset,
		   store_ADC_Channel_11_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_12_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_12_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 12));
	battery_log(BAT_LOG_CRTI,
			"[EM] ADC_Channel_12_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Offset, 0664, show_ADC_Channel_12_Offset,
		   store_ADC_Channel_12_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_13_Offset */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_13_Offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 13));
	battery_log(BAT_LOG_CRTI,
			"[EM] ADC_Channel_13_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Offset, 0664, show_ADC_Channel_13_Offset,
		   store_ADC_Channel_13_Offset);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_Is_Calibration */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_Is_Calibration(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 2;

	ret_value = g_ADC_Cali;
	battery_log(BAT_LOG_CRTI,
			"[EM] ADC_Channel_Is_Calibration : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_Is_Calibration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_Is_Calibration, 0664,
	show_ADC_Channel_Is_Calibration, store_ADC_Channel_Is_Calibration);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Power_On_Voltage */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Power_On_Voltage(struct device *dev,
		struct device_attribute *attr, char *buf) {
	int ret_value = 1;

	ret_value = 3400;
	battery_log(BAT_LOG_CRTI, "[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(struct device *dev,
		struct device_atttribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_On_Voltage, 0664,
		show_Power_On_Voltage, store_Power_On_Voltage);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Power_Off_Voltage */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Power_Off_Voltage(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	battery_log(BAT_LOG_CRTI, "[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_Off_Voltage, 0664,
		show_Power_Off_Voltage, store_Power_Off_Voltage);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Charger_TopOff_Value */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Charger_TopOff_Value(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 4110;
	battery_log(BAT_LOG_CRTI, "[EM] Charger_TopOff_Value: %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Charger_TopOff_Value(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_TopOff_Value, 0664, show_Charger_TopOff_Value,
		   store_Charger_TopOff_Value);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : FG_Battery_CurrentConsumption */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_FG_Battery_CurrentConsumption(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret_value = 8888;

	ret_value = battery_meter_get_battery_current();
	battery_log(BAT_LOG_CRTI,
			"[EM] FG_Battery_CurrentConsumption : %d/10 mA\n",
			ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_Battery_CurrentConsumption, 0664,
		show_FG_Battery_CurrentConsumption,
		store_FG_Battery_CurrentConsumption);

/* ////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : FG_SW_CoulombCounter */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_FG_SW_CoulombCounter(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	signed int ret_value = 7777;

	ret_value = battery_meter_get_car();
	battery_log(BAT_LOG_CRTI,
			"[EM] FG_SW_CoulombCounter : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_SW_CoulombCounter(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_SW_CoulombCounter, 0664, show_FG_SW_CoulombCounter,
		   store_FG_SW_CoulombCounter);


static ssize_t show_Charging_CallState(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "call state = %d\n", g_call_state);
	return sprintf(buf, "%u\n", g_call_state);
}

static ssize_t store_Charging_CallState(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int rv;

	/*rv = sscanf(buf, "%u", &g_call_state); */
	rv = kstrtouint(buf, 0, &g_call_state);
	if (rv != 0)
		return -EINVAL;
	battery_log(BAT_LOG_CRTI, "call state = %d\n", g_call_state);
	return size;
}

static DEVICE_ATTR(Charging_CallState, 0664, show_Charging_CallState,
		store_Charging_CallState);

/* V_0Percent_Tracking */
static ssize_t show_V_0Percent_Tracking(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI,
			"V_0Percent_Tracking = %d\n", V_0Percent_Tracking);
	return sprintf(buf, "%u\n", V_0Percent_Tracking);
}

static ssize_t store_V_0Percent_Tracking(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int rv;

	/*rv = sscanf(buf, "%u", &V_0Percent_Tracking); */
	rv = kstrtouint(buf, 0, &V_0Percent_Tracking);
	if (rv != 0)
		return -EINVAL;
	battery_log(BAT_LOG_CRTI,
			"V_0Percent_Tracking = %d\n", V_0Percent_Tracking);
	return size;
}

static DEVICE_ATTR(V_0Percent_Tracking, 0664,
		show_V_0Percent_Tracking, store_V_0Percent_Tracking);


static ssize_t show_Charger_Type(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int chr_ype = CHARGER_UNKNOWN;

	chr_ype = BMT_status.charger_exist ?
			BMT_status.charger_type : CHARGER_UNKNOWN;

	battery_log(BAT_LOG_CRTI, "CHARGER_TYPE = %d\n", chr_ype);
	return sprintf(buf, "%u\n", chr_ype);
}

static ssize_t store_Charger_Type(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_Type, 0664, show_Charger_Type, store_Charger_Type);

static ssize_t show_Pump_Express(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(PUMP_EXPRESS_SERIES)
	int icount = 20;	/* max debouncing time 20 * 0.2 sec */
#endif
	int is_ta_detected = 0;

	battery_log(BAT_LOG_CRTI,
	"[%s]show_Pump_Express chr_type:%d UISOC:%d startsoc:%d stopsoc:%d\n",
	__func__,
	BMT_status.charger_type, BMT_status.UI_SOC,
	batt_cust_data.ta_start_battery_soc,
	batt_cust_data.ta_stop_battery_soc);

#if defined(PUMP_EXPRESS_SERIES)
	if (BMT_status.charger_type ==  STANDARD_CHARGER &&
		(mtk_pep20_get_to_check_chr_type() || mtk_pep_get_to_check_chr_type()) &&
		BMT_status.UI_SOC < batt_cust_data.ta_stop_battery_soc) {
		do {
			icount--;
			msleep(200);
			battery_log(BAT_LOG_CRTI,
					"[%s]icount:%d\n", __func__, icount);
		} while (icount &&
		(mtk_pep20_get_to_check_chr_type() || mtk_pep_get_to_check_chr_type()));
	}

	/* Is PE+20 connect */
	if (mtk_pep20_get_is_connect() && (BMT_status.UI_SOC > 0) &&
			(BMT_status.UI_SOC < 95))
		is_ta_detected = 1;
	battery_log(BAT_LOG_FULL, "%s: pep20_is_connect = %d\n",
		    __func__, mtk_pep20_get_is_connect());

	/* Is PE+ connect */
	if (mtk_pep_get_is_connect() && (BMT_status.UI_SOC > 0) &&
			(BMT_status.UI_SOC < 95))
		is_ta_detected = 1;
	battery_log(BAT_LOG_FULL, "%s: pep_is_connect = %d\n", __func__,
			mtk_pep_get_is_connect());

#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
	/* Is PE connect */
	if ((true == ta_check_chr_type) &&
			(BMT_status.charger_type == STANDARD_CHARGER)) {
		battery_log(BAT_LOG_CRTI,
				"[%s]Wait for PE detection\n", __func__);
		do {
			msleep(200);
		} while (ta_check_chr_type);
	}
	if (is_ta_connect == true)
		is_ta_detected = 1;
#endif

	battery_log(BAT_LOG_CRTI, "%s: detected = %d\n", __func__,
			is_ta_detected);

	return sprintf(buf, "%u\n", is_ta_detected);
}

static ssize_t store_Pump_Express(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(Pump_Express, 0664, show_Pump_Express, store_Pump_Express);

static ssize_t show_FG_daemon_disable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "[FG] show FG disable : %d\n", gDisableGM);
	return sprintf(buf, "%d\n", gDisableGM);
}

static ssize_t store_FG_daemon_disable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[disable FG ]\n");
	gDisableGM = true;
	return size;
}
static DEVICE_ATTR(FG_daemon_disable, 0664,
			show_FG_daemon_disable, store_FG_daemon_disable);


static void mt_battery_update_EM(struct battery_data *bat_data)
{
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC;
	bat_data->BAT_TemperatureR = BMT_status.temperatureR;	/* API */
	bat_data->BAT_TempBattVoltage = BMT_status.temperatureV;/* API */
	bat_data->BAT_InstatVolt = BMT_status.bat_vol;	/* VBAT */
	bat_data->BAT_BatteryAverageCurrent = BMT_status.ICharging;
	bat_data->BAT_BatterySenseVoltage = BMT_status.bat_vol;
	bat_data->BAT_ISenseVoltage = BMT_status.Vsense;	/* API */
	bat_data->BAT_ChargerVoltage = BMT_status.charger_vol;
	bat_data->BAT_CURRENT_NOW = BMT_status.CURRENT_NOW * 100; /* 0.1mA to uA */
	/* Dual battery */
	bat_data->status_smb = g_status_smb;
	bat_data->capacity_smb = g_capacity_smb;
	bat_data->present_smb = g_present_smb;
	battery_log(BAT_LOG_FULL,
		"status_smb = %d, capacity_smb = %d, present_smb = %d\n",
		bat_data->status_smb, bat_data->capacity_smb,
		bat_data->present_smb);
	if ((BMT_status.UI_SOC == 100) && (BMT_status.charger_exist == true)
	    && (BMT_status.bat_charging_state != CHR_ERROR))
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;

#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
	if (bat_data->BAT_CAPACITY <= 0)
		bat_data->BAT_CAPACITY = 1;

	battery_log(BAT_LOG_CRTI,
		    "BAT_CAPACITY=1, due to define CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION\r\n");
#endif
}


static bool mt_battery_100Percent_tracking_check(void)
{
	bool resetBatteryMeter = false;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	unsigned int cust_sync_time = CUST_SOC_JEITA_SYNC_TIME;
	static unsigned int timer_counter =
		(CUST_SOC_JEITA_SYNC_TIME / BAT_TASK_PERIOD);
#else
	unsigned int cust_sync_time =
		batt_cust_data.onehundred_percent_tracking_time;
	static unsigned int timer_counter;
	static int timer_counter_init;
#endif


#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
#else
	/* Init timer_counter for 1st time */
	if (timer_counter_init == 0) {
		timer_counter =
			(batt_cust_data.onehundred_percent_tracking_time
			 / BAT_TASK_PERIOD);
		timer_counter_init = 1;
	}
#endif

	/* charging full first, UI tracking to 100% */
	if (BMT_status.bat_full == true) {
		if (BMT_status.UI_SOC >= 100) {
			BMT_status.UI_SOC = 100;

			if ((g_charging_full_reset_bat_meter == true)
			    && (BMT_status.bat_charging_state == CHR_BATFULL)) {
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

		if (BMT_status.UI_SOC == 100)
			is_uisoc_ever_100 = true;

		if ((BMT_status.UI_SOC - BMT_status.SOC) > 10
				&& is_uisoc_ever_100 == true) {
			is_uisoc_ever_100 = false;
			BMT_status.bat_full = false;
		}

		battery_log(BAT_LOG_CRTI,
			    "[100percent], UI_SOC(%d), reset(%d) bat_full(%d) ever100(%d)\n",
			    BMT_status.UI_SOC, resetBatteryMeter,
			    BMT_status.bat_full, is_uisoc_ever_100);
	} else if (is_uisoc_ever_100 == true) {
		battery_log(BAT_LOG_CRTI,
				"[100percent-ever100],UI_SOC=%d SOC=%d\n",
				BMT_status.UI_SOC, BMT_status.UI_SOC);
	} else {
		/* charging is not full,  UI keep 99% if reaching 100%, */

		if (BMT_status.UI_SOC >= 99) {
			BMT_status.UI_SOC = 99;
			resetBatteryMeter = false;
			battery_log(BAT_LOG_CRTI, "[100percent],UI_SOC = %d\n",
					BMT_status.UI_SOC);
		}

		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);

	}

	return resetBatteryMeter;
}


static bool mt_battery_nPercent_tracking_check(void)
{
	bool resetBatteryMeter = false;
#if defined(SOC_BY_HW_FG)
	static unsigned int timer_counter;
	static int timer_counter_init;

	if (timer_counter_init == 0) {
		timer_counter_init = 1;
		timer_counter =
		(batt_cust_data.npercent_tracking_time / BAT_TASK_PERIOD);
	}


	if (BMT_status.nPrecent_UI_SOC_check_point == 0)
		return false;

	/* fuel gauge ZCV < 15%, but UI > 15%,  15% can be customized */
	if ((BMT_status.ZCV <= BMT_status.nPercent_ZCV)
	    && (BMT_status.UI_SOC > BMT_status.nPrecent_UI_SOC_check_point)) {
		if (timer_counter ==
		(batt_cust_data.npercent_tracking_time / BAT_TASK_PERIOD)) {
			/* every x sec decrease UI percentage */
			BMT_status.UI_SOC--;
			timer_counter = 1;
		} else {
			timer_counter++;
			return resetBatteryMeter;
		}

		resetBatteryMeter = true;

		battery_log(BAT_LOG_CRTI,
			    "[nPercent] ZCV %d <= nPercent_ZCV %d, UI_SOC=%d., tracking UI_SOC=%d\n",
			    BMT_status.ZCV, BMT_status.nPercent_ZCV,
			    BMT_status.UI_SOC,
			    BMT_status.nPrecent_UI_SOC_check_point);
	} else if ((BMT_status.ZCV > BMT_status.nPercent_ZCV)
		   && (BMT_status.UI_SOC ==
			   BMT_status.nPrecent_UI_SOC_check_point)) {
	/* UI less than 15 , but fuel gague is more than 15, hold UI 15% */
		timer_counter =
		(batt_cust_data.npercent_tracking_time / BAT_TASK_PERIOD);
		resetBatteryMeter = true;

		battery_log(BAT_LOG_CRTI,
			    "[nPercent] ZCV %d > BMT_status.nPercent_ZCV %d and UI SOC=%d, then keep %d.\n",
			    BMT_status.ZCV, BMT_status.nPercent_ZCV,
			    BMT_status.UI_SOC,
			    BMT_status.nPrecent_UI_SOC_check_point);
	} else {
		timer_counter = (batt_cust_data.npercent_tracking_time / BAT_TASK_PERIOD);
	}
#endif
	return resetBatteryMeter;

}

static bool mt_battery_0Percent_tracking_check(void)
{
	bool resetBatteryMeter = true;

	if (BMT_status.UI_SOC <= 0) {
		BMT_status.UI_SOC = 0;
	} else {
		if (BMT_status.bat_vol > SYSTEM_OFF_VOLTAGE &&
				BMT_status.UI_SOC > 1)
			BMT_status.UI_SOC--;
		else if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE)
			BMT_status.UI_SOC--;

	}

	battery_log(BAT_LOG_CRTI, "0Percent, VBAT < %d UI_SOC=%d\r\n",
			SYSTEM_OFF_VOLTAGE,
			BMT_status.UI_SOC);

	return resetBatteryMeter;
}


static void mt_battery_Sync_UI_Percentage_to_Real(void)
{
	static struct timespec lasttime;
	struct timespec now_time, diff;

	get_monotonic_boottime(&now_time);
	diff = timespec_sub(now_time, lasttime);
	if ((BMT_status.UI_SOC > BMT_status.SOC) && ((BMT_status.UI_SOC != 1))) {
#if !defined(SYNC_UI_SOC_IMM)
		/* reduce after xxs */
		if (chr_wake_up_bat == false) {
			if (diff.tv_sec >= batt_cust_data.sync_to_real_tracking_time) {
				BMT_status.UI_SOC--;
				get_monotonic_boottime(&lasttime);
			}
#ifdef FG_BAT_INT
			else if (fg_wake_up_bat == true)
				BMT_status.UI_SOC--;

#endif				/* #ifdef FG_BAT_INT */

		} else {
			battery_log(BAT_LOG_CRTI,
			"[Sync_Real] chr_wake_up_bat=1, don't update UI_SOC\n");
		}
#else
		BMT_status.UI_SOC--;
#endif
		battery_log(BAT_LOG_CRTI, "[Sync_Real] UI_SOC=%d, SOC=%d diff=%d\n",
			    BMT_status.UI_SOC, BMT_status.SOC, (int)diff.tv_sec);
	} else {
		get_monotonic_boottime(&lasttime);

		BMT_status.UI_SOC = BMT_status.SOC;

	}

	if (BMT_status.UI_SOC <= 0) {
		BMT_status.UI_SOC = 1;
		battery_log(BAT_LOG_CRTI, "[Battery]UI_SOC get 0 first (%d)\r\n",
			    BMT_status.UI_SOC);
	}
}

static void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = bat_data->psy;
	bool resetBatteryMeter = false;
	static unsigned int update_cnt = 3;
	static unsigned int pre_uisoc;
	static unsigned int pre_chr_state;

	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	bat_data->BAT_batt_vol = BMT_status.bat_vol;
	bat_data->BAT_batt_temp = BMT_status.temperature * 10;
	bat_data->BAT_PRESENT = BMT_status.bat_exist;

	if ((BMT_status.charger_exist == true) &&
			(BMT_status.bat_charging_state != CHR_ERROR)) {
		if (BMT_status.bat_exist) {	/* charging */
			if (BMT_status.bat_vol <=
					batt_cust_data.v_0percent_tracking)
				resetBatteryMeter =
					mt_battery_0Percent_tracking_check();
			else
				resetBatteryMeter =
					mt_battery_100Percent_tracking_check();


			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
		} else {	/* No Battery, Only Charger */

			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
			BMT_status.UI_SOC = 0;
		}

	} else {		/* Only Battery */

		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING;
		if (BMT_status.bat_vol <= batt_cust_data.v_0percent_tracking)
			resetBatteryMeter =
				mt_battery_0Percent_tracking_check();
		else
			resetBatteryMeter =
				mt_battery_nPercent_tracking_check();
	}

	if (resetBatteryMeter == true) {
		battery_meter_reset();
	} else {
		if (BMT_status.bat_full == true &&
				is_uisoc_ever_100 == true) {
			BMT_status.UI_SOC = 100;
			battery_log(BAT_LOG_CRTI,
					"[recharging] UI_SOC=%d, SOC=%d\n",
					BMT_status.UI_SOC, BMT_status.SOC);
		} else {
			mt_battery_Sync_UI_Percentage_to_Real();
		}
	}

	battery_log(BAT_LOG_CRTI, "UI_SOC=(%d), resetBatteryMeter=(%d)\n",
		    BMT_status.UI_SOC, resetBatteryMeter);


	/* set RTC SOC to 1 to avoid SOC jump in charger boot. */
	if (BMT_status.UI_SOC <= 1)
		set_rtc_spare_fg_value(1);
	else
		set_rtc_spare_fg_value(BMT_status.UI_SOC);


	mt_battery_update_EM(bat_data);

	if (cmd_discharging == 1)
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CMD_DISCHARGING;

	if (adjust_power != -1) {
		bat_data->adjust_power = adjust_power;
		battery_log(BAT_LOG_CRTI,
				"adjust_power=(%d)\n", adjust_power);
	}
#ifdef DLPT_POWER_OFF_EN
	/*extern int dlpt_check_power_off(void); */
	if (bat_data->BAT_CAPACITY <= DLPT_POWER_OFF_THD) {
		static signed char cnt;

		battery_log(BAT_LOG_CRTI, "[DLPT_POWER_OFF_EN] run\n");

		if (dlpt_check_power_off() == 1) {
			bat_data->BAT_CAPACITY = 0;
			cnt++;
			battery_log(BAT_LOG_CRTI,
				"[DLPT_POWER_OFF_EN] SOC=%d to power off\n",
				bat_data->BAT_CAPACITY);
			if (cnt >= 2)
				kernel_restart("DLPT reboot system");

		} else
			cnt = 0;
	} else {
		battery_log(BAT_LOG_CRTI, "[DLPT_POWER_OFF_EN] disable(%d)\n",
			    bat_data->BAT_CAPACITY);
	}
#endif

	if (update_cnt >= 3) {
		/* Update per 60 seconds */
		power_supply_changed(bat_psy);
		pre_uisoc = BMT_status.UI_SOC;
		update_cnt = 0;
		pre_chr_state = BMT_status.bat_charging_state;
		if (cable_in_uevent == 1)
			cable_in_uevent = 0;
	} else if ((pre_uisoc != BMT_status.UI_SOC) ||
			(BMT_status.UI_SOC == 0)) {
		/* Update when soc change */
		power_supply_changed(bat_psy);
		pre_uisoc = BMT_status.UI_SOC;
		update_cnt = 0;
	} else if ((BMT_status.charger_exist == true) &&
		   ((pre_chr_state != BMT_status.bat_charging_state) ||
		    (BMT_status.bat_charging_state == CHR_ERROR))) {
		/* Update when changer status change */
		power_supply_changed(bat_psy);
		pre_chr_state = BMT_status.bat_charging_state;
		update_cnt = 0;
	} else if (cable_in_uevent == 1) {
		/*To prevent interrupt-trigger update from being filtered */
		power_supply_changed(bat_psy);
		cable_in_uevent = 0;
	} else {
		/* No update */
		update_cnt++;
	}
}

void update_charger_info(int wireless_state)
{
#if defined(CONFIG_POWER_VERIFY)
	battery_log(BAT_LOG_CRTI, "[update_charger_info] no support\n");
#else
	g_wireless_state = wireless_state;
	battery_log(BAT_LOG_CRTI,
			"[update_charger_info] get wireless_state=%d\n",
			wireless_state);

	wake_up_bat();
#endif
}

static void wireless_update(struct wireless_data *wireless_data)
{
	static int wireless_status = -1;
	struct power_supply *wireless_psy = wireless_data->psy;
	struct power_supply_desc *wireless_psd = &wireless_data->psd;

	if (BMT_status.charger_exist == true || g_wireless_state) {
		if ((BMT_status.charger_type == WIRELESS_CHARGER) ||
				g_wireless_state) {
			wireless_data->WIRELESS_ONLINE = 1;
			wireless_psd->type = POWER_SUPPLY_TYPE_WIRELESS;
		} else {
			wireless_data->WIRELESS_ONLINE = 0;
		}
	} else {
		wireless_data->WIRELESS_ONLINE = 0;
	}

	if (wireless_status != wireless_data->WIRELESS_ONLINE) {
		wireless_status = wireless_data->WIRELESS_ONLINE;
		power_supply_changed(wireless_psy);
	}
}

static void ac_update(struct ac_data *ac_data)
{
	static int ac_status = -1;
	struct power_supply *ac_psy = ac_data->psy;
	struct power_supply_desc *ac_psd = &ac_data->psd;

	if (BMT_status.charger_exist == true) {
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if ((BMT_status.charger_type == NONSTANDARD_CHARGER) ||
		    (BMT_status.charger_type == STANDARD_CHARGER) ||
		    (BMT_status.charger_type == APPLE_2_1A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_1_0A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_0_5A_CHARGER)) {
#else
		if ((BMT_status.charger_type == NONSTANDARD_CHARGER) ||
		    (BMT_status.charger_type == STANDARD_CHARGER) ||
		    (BMT_status.charger_type == APPLE_2_1A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_1_0A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_0_5A_CHARGER) ||
		    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif
			ac_data->AC_ONLINE = 1;
			ac_psd->type = POWER_SUPPLY_TYPE_MAINS;
		} else {
			ac_data->AC_ONLINE = 0;
		}
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

	if (BMT_status.charger_exist == true) {
		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST)) {
			usb_data->USB_ONLINE = 1;
			usb_psd->type = POWER_SUPPLY_TYPE_USB;
		} else {
			usb_data->USB_ONLINE = 0;
		}
	} else {
		usb_data->USB_ONLINE = 0;
	}

	if (usb_status != usb_data->USB_ONLINE) {
		usb_status = usb_data->USB_ONLINE;
		power_supply_changed(usb_psy);
	}
}

#endif

/* ////////////////////////////////////////////////////////////////////////// */
/* // Battery Temprature Parameters and functions */
/* ////////////////////////////////////////////////////////////////////////// */
bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det() == true)
		return true;
	/*else { */
	battery_log(BAT_LOG_CRTI, "[pmic_chrdet_status] No charger\r\n");
	return false;
	/*} */
}

/* ////////////////////////////////////////////////////////////////////////// */
/* // Pulse Charging Algorithm */
/* ////////////////////////////////////////////////////////////////////////// */
bool bat_is_charger_exist(void)
{
	return get_charger_detect_status();
}


bool bat_is_charging_full(void)
{
	if ((BMT_status.bat_full == true) &&
			(BMT_status.bat_in_recharging_state == false))
		return true;
	else
		return false;
}


unsigned int bat_get_ui_percentage(void)
{
	/* for plugging out charger in recharge phase, using SOC as UI_SOC */

#if defined(CONFIG_POWER_EXT)
	battery_log(BAT_LOG_CRTI,
			"[BATTERY] bat_get_ui_percentage return 100 !!\n\r");
	return 100;
#else
	if (chr_wake_up_bat == true)
		return BMT_status.SOC;
	else
		return BMT_status.UI_SOC;
#endif
}

/* Full state --> recharge voltage --> full state */
unsigned int bat_is_recharging_phase(void)
{
	return BMT_status.bat_in_recharging_state ||
		BMT_status.bat_full == true;
}


int get_bat_charging_current_level(void)
{
	CHR_CURRENT_ENUM charging_current;

	battery_charging_control(CHARGING_CMD_GET_CURRENT, &charging_current);

	return charging_current;
}


unsigned int do_batt_temp_state_machine(void)
{
	if (BMT_status.temperature == batt_cust_data.err_charge_temperature)
		return PMU_STATUS_FAIL;



	if (batt_cust_data.bat_low_temp_protect_enable) {
		if (BMT_status.temperature <
				batt_cust_data.min_charge_temperature) {
			battery_log(BAT_LOG_CRTI,
			"[BATTERY] Battery Under Temperature or NTC fail!\n\r");
			g_batt_temp_status = TEMP_POS_LOW;
			return PMU_STATUS_FAIL;
		} else if (g_batt_temp_status == TEMP_POS_LOW) {
			if (BMT_status.temperature >=
			    batt_cust_data.min_charge_temperature_plus_x_degree) {
				battery_log(BAT_LOG_CRTI,
					    "[BATTERY] Battery Temperature raise from %d to %d(%d), allow charging!!\n\r",
					    batt_cust_data.min_charge_temperature,
					    BMT_status.temperature,
					    batt_cust_data.min_charge_temperature_plus_x_degree);
				g_batt_temp_status = TEMP_POS_NORMAL;
				BMT_status.bat_charging_state = CHR_PRE;
				return PMU_STATUS_OK;
			} else {
				return PMU_STATUS_FAIL;
			}
		}
	}

	if (BMT_status.temperature >= batt_cust_data.max_charge_temperature) {
		battery_log(BAT_LOG_CRTI,
				"[BATTERY] Battery Over Temperature !!\n\r");
		g_batt_temp_status = TEMP_POS_HIGH;
		return PMU_STATUS_FAIL;
	} else if (g_batt_temp_status == TEMP_POS_HIGH) {
		if (BMT_status.temperature <
			batt_cust_data.max_charge_temperature_minus_x_degree) {
			battery_log(BAT_LOG_CRTI,
				"[BATTERY] Battery Temperature down from %d to %d(%d), allow charging!!\n\r",
				batt_cust_data.max_charge_temperature,
				BMT_status.temperature,
				batt_cust_data.max_charge_temperature_minus_x_degree);
			g_batt_temp_status = TEMP_POS_NORMAL;
			BMT_status.bat_charging_state = CHR_PRE;
			return PMU_STATUS_OK;
		} else {
			return PMU_STATUS_FAIL;
		}
	} else {
		g_batt_temp_status = TEMP_POS_NORMAL;
	}
	return PMU_STATUS_OK;
}


unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
	unsigned long ret_val = 0;

#if defined(CONFIG_POWER_EXT)
	ret_val = 4000;
#else
	ret_val = battery_meter_get_battery_voltage(false);
#endif

	return ret_val;
}


static void mt_battery_average_method_init(BATTERY_AVG_ENUM type,
		unsigned int *bufferdata, unsigned int data, signed int *sum)
{
	unsigned int i;
	static bool batteryBufferFirst = true;
	static bool previous_charger_exist;
	static bool previous_in_recharge_state;

	static unsigned char index;

	/* reset charging current window while plug in/out { */
	if (type == BATTERY_AVG_CURRENT) {
		if (BMT_status.charger_exist == true) {
			if (previous_charger_exist == false) {
				batteryBufferFirst = true;
				previous_charger_exist = true;
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
				if (BMT_status.charger_type == STANDARD_CHARGER) {
#else
				if ((BMT_status.charger_type == STANDARD_CHARGER) ||
				    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif
					data = batt_cust_data.ac_charger_current / 100;
				} else if (BMT_status.charger_type == CHARGING_HOST) {
					data = batt_cust_data.charging_host_charger_current / 100;
				} else if (BMT_status.charger_type == NONSTANDARD_CHARGER)
					data = batt_cust_data.non_std_ac_charger_current / 100;	/* mA */
				else	/* USB */
					data = batt_cust_data.usb_charger_current / 100;	/* mA */
#ifdef AVG_INIT_WITH_R_SENSE
				data = AVG_INIT_WITH_R_SENSE(data);
#endif
			} else if ((previous_in_recharge_state == false)
				   && (BMT_status.bat_in_recharging_state == true)) {
				batteryBufferFirst = true;
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
				if (BMT_status.charger_type == STANDARD_CHARGER) {
#else
				if ((BMT_status.charger_type == STANDARD_CHARGER) ||
				    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif
					data = batt_cust_data.ac_charger_current / 100;
				} else if (BMT_status.charger_type == CHARGING_HOST) {
					data = batt_cust_data.charging_host_charger_current / 100;
				} else if (BMT_status.charger_type == NONSTANDARD_CHARGER)
					data = batt_cust_data.non_std_ac_charger_current / 100;	/* mA */
				else	/* USB */
					data = batt_cust_data.usb_charger_current / 100;	/* mA */
#ifdef AVG_INIT_WITH_R_SENSE
				data = AVG_INIT_WITH_R_SENSE(data);
#endif
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
	/* reset charging current window while plug in/out } */

	battery_log(BAT_LOG_FULL,
			"batteryBufferFirst =%d, data= (%d)\n",
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


static unsigned int mt_battery_average_method(BATTERY_AVG_ENUM type,
		unsigned int *bufferdata, unsigned int data,
		signed int *sum, unsigned char batteryIndex)
{
	unsigned int avgdata;

	mt_battery_average_method_init(type, bufferdata, data, sum);

	*sum -= bufferdata[batteryIndex];
	*sum += data;
	bufferdata[batteryIndex] = data;
	avgdata = (*sum) / BATTERY_AVERAGE_SIZE;

	battery_log(BAT_LOG_FULL, "bufferdata[%d]= (%d)\n",
			batteryIndex, bufferdata[batteryIndex]);
	return avgdata;
}

void mt_battery_GetBatteryData(void)
{
	unsigned int bat_vol, charger_vol, Vsense, ZCV;
	signed int ICharging, temperature, temperatureR, temperatureV, SOC;
	static signed int bat_sum, icharging_sum, temperature_sum;
	static signed int batteryVoltageBuffer[BATTERY_AVERAGE_SIZE];
	static signed int batteryCurrentBuffer[BATTERY_AVERAGE_SIZE];
	static signed int batteryTempBuffer[BATTERY_AVERAGE_SIZE];
	static unsigned char batteryIndex;
	static signed int previous_SOC = -1;
	bool current_sign;

	bat_vol = battery_meter_get_battery_voltage(true);
	Vsense = battery_meter_get_VSense();
	if (upmu_is_chr_det() == true)
		ICharging = battery_meter_get_charging_current();
	else
		ICharging = 0;


	charger_vol = battery_meter_get_charger_voltage();
	temperature = battery_meter_get_battery_temperature();
	temperatureV = battery_meter_get_tempV();
	temperatureR = battery_meter_get_tempR(temperatureV);

	if (bat_meter_timeout == true || bat_spm_timeout == TRUE ||
			fg_wake_up_bat == true) {
		SOC = battery_meter_get_battery_percentage();
		/*if (bat_spm_timeout == true) */
		/*BMT_status.UI_SOC = battery_meter_get_battery_percentage();*/

		bat_meter_timeout = false;
		bat_spm_timeout = FALSE;
	} else {
		if (previous_SOC == -1)
			SOC = battery_meter_get_battery_percentage();
		else
			SOC = previous_SOC;
	}

	ZCV = battery_meter_get_battery_zcv();

	BMT_status.ICharging =
	    mt_battery_average_method(BATTERY_AVG_CURRENT,
			    &batteryCurrentBuffer[0], ICharging,
			    &icharging_sum, batteryIndex);


	if (previous_SOC == -1 &&
			bat_vol <= batt_cust_data.v_0percent_tracking) {
		battery_log(BAT_LOG_CRTI,
		 "battery voltage too low, use ZCV to init average data.\n");
		BMT_status.bat_vol =
		    mt_battery_average_method(BATTERY_AVG_VOLT,
				    &batteryVoltageBuffer[0], ZCV,
				    &bat_sum, batteryIndex);
	} else {
		BMT_status.bat_vol =
		    mt_battery_average_method(BATTERY_AVG_VOLT,
				    &batteryVoltageBuffer[0], bat_vol,
				    &bat_sum, batteryIndex);
	}


	if (battery_cmd_thermal_test_mode == 1) {
		battery_log(BAT_LOG_CRTI,
				"test mode , battery temperature is fixed.\n");
	} else {
		BMT_status.temperature =
		    mt_battery_average_method(BATTERY_AVG_TEMP,
				    &batteryTempBuffer[0], temperature,
				    &temperature_sum, batteryIndex);
	}


	BMT_status.Vsense = Vsense;
	BMT_status.charger_vol = charger_vol;
	BMT_status.temperatureV = temperatureV;
	BMT_status.temperatureR = temperatureR;
	BMT_status.SOC = SOC;
	BMT_status.ZCV = ZCV;
	BMT_status.IBattery = battery_meter_get_battery_current();
	BMT_status.CURRENT_NOW = BMT_status.IBattery;
	current_sign = battery_meter_get_battery_current_sign();
	BMT_status.IBattery *= (current_sign ? 1 : (-1));

	if (BMT_status.charger_exist == false) {
		if (BMT_status.SOC > previous_SOC && previous_SOC >= 0)
			BMT_status.SOC = previous_SOC;
	}


	previous_SOC = BMT_status.SOC;

	batteryIndex++;
	if (batteryIndex >= BATTERY_AVERAGE_SIZE)
		batteryIndex = 0;


	if (g_battery_soc_ready == false)
		g_battery_soc_ready = true;

	battery_log(BAT_LOG_CRTI,
	"AvgVbat=(%d,%d),AvgI=(%d,%d),VChr=%d,AvgT=(%d,%d),SOC=(%d,%d),UI_SOC=%d,ZCV=%d,CHR_Type=%d bcct:%d:%d I:%d Ibat:%d fg:%d\n",
		    BMT_status.bat_vol, bat_vol, BMT_status.ICharging,
		    ICharging, BMT_status.charger_vol, BMT_status.temperature,
		    temperature, previous_SOC, BMT_status.SOC,
		    BMT_status.UI_SOC, BMT_status.ZCV, BMT_status.charger_type,
		    g_bcct_flag, get_usb_current_unlimited(),
		    get_bat_charging_current_level(), BMT_status.IBattery / 10,
		    gDisableGM);
}

static unsigned int mt_battery_CheckBatteryTemp(
		void)
{
	unsigned int status = PMU_STATUS_OK;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

	battery_log(BAT_LOG_CRTI, "[BATTERY] support JEITA, temperature=%d\n",
		    BMT_status.temperature);

	if (do_jeita_state_machine() == PMU_STATUS_FAIL) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] JEITA : fail\n");
		status = PMU_STATUS_FAIL;
	}
#else


	if (batt_cust_data.mtk_temperature_recharge_support) {
		if (do_batt_temp_state_machine() == PMU_STATUS_FAIL) {
			battery_log(BAT_LOG_CRTI,
					"[BATTERY] Batt temp check : fail\n");
			status = PMU_STATUS_FAIL;
		}
	} else {
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
		if ((BMT_status.temperature < MIN_CHARGE_TEMPERATURE)
		    || (BMT_status.temperature == ERR_CHARGE_TEMPERATURE)) {
			battery_log(BAT_LOG_CRTI,
			"[BATTERY] Battery Under Temperature or NTC fail\n\r");
			status = PMU_STATUS_FAIL;
		}
#endif
		if (BMT_status.temperature >= MAX_CHARGE_TEMPERATURE) {
			battery_log(BAT_LOG_CRTI,
				"[BATTERY] Battery Over Temperature !!\n\r");
			status = PMU_STATUS_FAIL;
		}
	}

#endif

	return status;
}


static unsigned int mt_battery_CheckChargerVoltage(void)
{
	unsigned int status = PMU_STATUS_OK;
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	unsigned int v_charger_max = DISO_data.hv_voltage;
#endif

	if (BMT_status.charger_exist == true) {

		if (batt_cust_data.v_charger_enable) {
			if (BMT_status.charger_vol <=
					batt_cust_data.v_charger_min) {
				battery_log(BAT_LOG_CRTI,
					"[BATTERY]Charger under voltage!!\r\n");
				BMT_status.bat_charging_state = CHR_ERROR;
				status = PMU_STATUS_FAIL;
			}
		}
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (BMT_status.charger_vol >= batt_cust_data.v_charger_max) {
#else
		if (BMT_status.charger_vol >= v_charger_max) {
#endif
			battery_log(BAT_LOG_CRTI,
					"[BATTERY]Charger over voltage !!\r\n");
			BMT_status.charger_protect_status = charger_OVER_VOL;
			BMT_status.bat_charging_state = CHR_ERROR;
			status = PMU_STATUS_FAIL;
		}
	}

	return status;
}


static unsigned int mt_battery_CheckChargingTime(void)
{
	unsigned int status = PMU_STATUS_OK;

	if ((g_battery_thermal_throttling_flag == 2) ||
			(g_battery_thermal_throttling_flag == 3)) {
		battery_log(BAT_LOG_FULL,
			    "[TestMode] Disable Safety Timer. bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			    g_battery_thermal_throttling_flag,
			    battery_cmd_thermal_test_mode,
			    battery_cmd_thermal_test_mode_value);

	} else {
		/* Charging OT */
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME) {
			battery_log(BAT_LOG_CRTI,
					"[BATTERY] Charging Over Time.\n");

			status = PMU_STATUS_FAIL;
		}
	}

	return status;

}

#if defined(STOP_CHARGING_IN_TAKLING)
static unsigned int mt_battery_CheckCallState(void)
{
	unsigned int status = PMU_STATUS_OK;

	if ((g_call_state == CALL_ACTIVE)
	    && (BMT_status.bat_vol > batt_cust_data.v_cc2topoff_thres))
		status = PMU_STATUS_FAIL;

	return status;
}
#endif

static void mt_battery_CheckBatteryStatus(void)
{
	battery_log(BAT_LOG_FULL,
		"[mt_battery_CheckBatteryStatus] cmd_discharging=(%d)\n",
		cmd_discharging);
	if (cmd_discharging == 1) {
		battery_log(BAT_LOG_CRTI,
			"[mt_battery_CheckBatteryStatus]cmd_discharging=(%d)\n",
			cmd_discharging);
		BMT_status.bat_charging_state = CHR_ERROR;
		battery_charging_control(CHARGING_CMD_SET_ERROR_STATE,
				&cmd_discharging);
		return;
	} else if (cmd_discharging == 0) {
		BMT_status.bat_charging_state = CHR_PRE;
		battery_charging_control(CHARGING_CMD_SET_ERROR_STATE,
				&cmd_discharging);
		cmd_discharging = -1;
	}
	if (mt_battery_CheckBatteryTemp() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

	if (mt_battery_CheckChargerVoltage() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}
#if defined(STOP_CHARGING_IN_TAKLING)
	if (mt_battery_CheckCallState() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_HOLD;
		return;
	}
#endif

	if (mt_battery_CheckChargingTime() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}
}


static void mt_battery_notify_TotalChargingTime_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME)
	if ((g_battery_thermal_throttling_flag == 2) ||
			(g_battery_thermal_throttling_flag == 3)) {
		battery_log(BAT_LOG_FULL,
			"[TestMode] Disable Safety Timer : no UI display\n");
	} else {
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME)
			/* if(BMT_status.total_charging_time >= 60) //test */
		{
			g_BatteryNotifyCode |= 0x0010;
			battery_log(BAT_LOG_CRTI,
					"[BATTERY] Charging Over Time\n");
		} else {
			g_BatteryNotifyCode &= ~(0x0010);
		}
	}

	battery_log(BAT_LOG_CRTI,
		"[BATTERY] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME (%x)\n",
		g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_VBat_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0004_VBAT)
	if (BMT_status.bat_vol > 4350)
		/* if(BMT_status.bat_vol > 3800) //test */
	{
		g_BatteryNotifyCode |= 0x0008;
		battery_log(BAT_LOG_CRTI, "[BATTERY] bat_vlot(%ld) > 4350mV\n",
				BMT_status.bat_vol);
	} else {
		g_BatteryNotifyCode &= ~(0x0008);
	}

	battery_log(BAT_LOG_CRTI,
			"[BATTERY] BATTERY_NOTIFY_CASE_0004_VBAT (%x)\n",
			g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_ICharging_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0003_ICHARGING)
	if ((BMT_status.ICharging > 1000) &&
			(BMT_status.total_charging_time > 300)) {
		g_BatteryNotifyCode |= 0x0004;
		battery_log(BAT_LOG_CRTI,
				"[BATTERY] I_charging(%ld) > 1000mA\n",
				BMT_status.ICharging);
	} else {
		g_BatteryNotifyCode &= ~(0x0004);
	}

	battery_log(BAT_LOG_CRTI,
			"[BATTERY] BATTERY_NOTIFY_CASE_0003_ICHARGING (%x)\n",
			g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_VBatTemp_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)

	if (BMT_status.temperature >= batt_cust_data.max_charge_temperature) {
		g_BatteryNotifyCode |= 0x0002;
		battery_log(BAT_LOG_CRTI,
			"[BATTERY] bat_temp(%d) out of range(too high)\n",
			BMT_status.temperature);
	}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	else if (BMT_status.temperature < TEMP_NEG_10_THRESHOLD) {
		g_BatteryNotifyCode |= 0x0020;
		battery_log(BAT_LOG_CRTI,
			"[BATTERY] bat_temp(%d) out of range(too low)\n",
			BMT_status.temperature);
	}
#else
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
	else if (BMT_status.temperature < MIN_CHARGE_TEMPERATURE) {
		g_BatteryNotifyCode |= 0x0020;
		battery_log(BAT_LOG_CRTI,
			"[BATTERY] bat_temp(%d) out of range(too low)\n",
			BMT_status.temperature);
	}
#endif
#endif

	battery_log(BAT_LOG_FULL,
			"[BATTERY] BATTERY_NOTIFY_CASE_0002_VBATTEMP (%x)\n",
			g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_VCharger_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	unsigned int v_charger_max = DISO_data.hv_voltage;
#endif

#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (BMT_status.charger_vol > batt_cust_data.v_charger_max) {
#else
	if (BMT_status.charger_vol > v_charger_max) {
#endif
		g_BatteryNotifyCode |= 0x0001;
		battery_log(BAT_LOG_CRTI,
			"[BATTERY] BMT_status.charger_vol(%d) > %d mV\n",
			BMT_status.charger_vol, batt_cust_data.v_charger_max);
	} else {
		g_BatteryNotifyCode &= ~(0x0001);
	}
	if (g_BatteryNotifyCode != 0x0000)
		battery_log(BAT_LOG_CRTI,
			"[BATTERY] BATTERY_NOTIFY_CASE_0001_VCHARGER (%x)\n",
			g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_UI_test(void)
{
	if (g_BN_TestMode == 0x0001) {
		g_BatteryNotifyCode = 0x0001;
		battery_log(BAT_LOG_CRTI,
		"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0001_VCHARGER\n");
	} else if (g_BN_TestMode == 0x0002) {
		g_BatteryNotifyCode = 0x0002;
		battery_log(BAT_LOG_CRTI,
		"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0002_VBATTEMP\n");
	} else if (g_BN_TestMode == 0x0003) {
		g_BatteryNotifyCode = 0x0004;
		battery_log(BAT_LOG_CRTI,
		"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0003_ICHARGING\n");
	} else if (g_BN_TestMode == 0x0004) {
		g_BatteryNotifyCode = 0x0008;
		battery_log(BAT_LOG_CRTI,
		"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0004_VBAT\n");
	} else if (g_BN_TestMode == 0x0005) {
		g_BatteryNotifyCode = 0x0010;
		battery_log(BAT_LOG_CRTI,
		"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME\n");
	} else {
		battery_log(BAT_LOG_CRTI,
				"[BATTERY] Unknown BN_TestMode Code : %x\n",
				g_BN_TestMode);
	}
}


void mt_battery_notify_check(void)
{
	g_BatteryNotifyCode = 0x0000;

	if (g_BN_TestMode == 0x0000) {	/* for normal case */
		battery_log(BAT_LOG_FULL,
				"[BATTERY] mt_battery_notify_check\n");

		mt_battery_notify_VCharger_check();

		mt_battery_notify_VBatTemp_check();

		mt_battery_notify_ICharging_check();

		mt_battery_notify_VBat_check();

		mt_battery_notify_TotalChargingTime_check();
	} else {		/* for UI test */

		mt_battery_notify_UI_test();
	}
}

static void mt_battery_thermal_check(void)
{
	if ((g_battery_thermal_throttling_flag == 1) ||
			(g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature =
				battery_cmd_thermal_test_mode_value;
			battery_log(BAT_LOG_FULL,
				    "[Battery] In thermal_test_mode, Tbat=%d\n",
				    BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		/* ignore default rule */
#else
		if (BMT_status.temperature >= 60) {
#if defined(CONFIG_POWER_EXT)
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] CONFIG_POWER_EXT, no update battery update power down.\n");
#else
			{
				if ((g_platform_boot_mode == META_BOOT)
				    || (g_platform_boot_mode == ADVMETA_BOOT)
				    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
					battery_log(BAT_LOG_FULL,
						    "[BATTERY] boot mode = %d, bypass temperature check\n",
						    g_platform_boot_mode);
				} else {
					struct battery_data *bat_data = &battery_main;
					struct power_supply *bat_psy = bat_data->psy;

					battery_log(BAT_LOG_CRTI,
						    "[Battery] Tbat(%d)>=60, system need power down.\n",
						    BMT_status.temperature);

					bat_data->BAT_CAPACITY = 0;

					power_supply_changed(bat_psy);

					if (BMT_status.charger_exist == true) {
						/* can not power down due to charger exist, so need reset system */
						battery_charging_control
						    (CHARGING_CMD_SET_PLATFORM_RESET, NULL);
					}
					/* avoid SW no feedback */
					battery_charging_control(
						CHARGING_CMD_SET_POWER_OFF, NULL);
				}
			}
#endif
		}
#endif

	}

}


static void mt_battery_update_status(void)
{
#if defined(CONFIG_POWER_EXT)
	battery_log(BAT_LOG_CRTI,
			"[BATTERY] CONFIG_POWER_EXT, no update Android.\n");
#else
	{
		if (skip_battery_update == false) {
			battery_log(BAT_LOG_FULL, "mt_battery_update_status\n");
			usb_update(&usb_main);
			ac_update(&ac_main);
			wireless_update(&wireless_main);
			battery_update(&battery_main);
		} else {
			battery_log(BAT_LOG_CRTI,
					"skip mt_battery_update_status.\n");
			skip_battery_update = false;
		}
	}

#endif
}


CHARGER_TYPE mt_charger_type_detection(void)
{
	CHARGER_TYPE CHR_Type_num = CHARGER_UNKNOWN;

	mutex_lock(&charger_type_mutex);

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
#if defined(CONFIG_USB_MTK_CHARGER_DETECT)
	if (mt_get_usb11_port_status() == true) {
		battery_log(BAT_LOG_CRTI,
				"========use_usb_detect===========\n");
		CHR_Type_num = usb_charger_type_detect();
		BMT_status.charger_type = CHR_Type_num;
	} else
#endif
	{
		battery_charging_control(
				CHARGING_CMD_GET_CHARGER_TYPE, &CHR_Type_num);
		BMT_status.charger_type = CHR_Type_num;
	}
#else
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (BMT_status.charger_type == CHARGER_UNKNOWN) {
#else
	if ((BMT_status.charger_type == CHARGER_UNKNOWN) &&
	    (DISO_data.diso_state.cur_vusb_state == DISO_ONLINE)) {
#endif
#if defined(CONFIG_USB_MTK_CHARGER_DETECT)
		if (mt_get_usb11_port_status() == true) {
			battery_log(BAT_LOG_CRTI,
					"========use_usb_detect===========\n");
			CHR_Type_num = usb_charger_type_detect();
			BMT_status.charger_type = CHR_Type_num;
		} else
#endif
		{
			battery_charging_control(
				CHARGING_CMD_GET_CHARGER_TYPE, &CHR_Type_num);
			BMT_status.charger_type = CHR_Type_num;
		}
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#if defined(PUMP_EXPRESS_SERIES)
		if (BMT_status.UI_SOC == 100) {
			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = true;
			g_charging_full_reset_bat_meter = true;
		}

		if (g_battery_soc_ready == false) {
			if (BMT_status.nPercent_ZCV == 0)
				battery_meter_initial();

			BMT_status.SOC = battery_meter_get_battery_percentage();
		}

		if (BMT_status.bat_vol > 0)
			mt_battery_update_status();

#endif
#endif
	}
#endif
	mutex_unlock(&charger_type_mutex);

	return BMT_status.charger_type;
}

CHARGER_TYPE mt_get_charger_type(void)
{
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	return STANDARD_HOST;
#else
	return BMT_status.charger_type;
#endif
}

static void mt_battery_charger_detect_check(void)
{
#ifdef CONFIG_MTK_BQ25896_SUPPORT
/*New low power feature of MT6531: disable charger CLK without CHARIN.
 * MT6351 API abstracted in charging_hw_bw25896.c.
 * Any charger with MT6351 needs to set this.
 * Compile option is not limited to CONFIG_MTK_BQ25896_SUPPORT.
 * PowerDown = 0
 */
	unsigned int pwr;
#endif
	if (upmu_is_chr_det() == true) {
		__pm_stay_awake(battery_suspend_lock);

#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		BMT_status.charger_exist = true;
#endif

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
		mt_charger_type_detection();

		if ((BMT_status.charger_type == STANDARD_HOST)
		    || (BMT_status.charger_type == CHARGING_HOST)) {
			mt_usb_connect();
		}
#else
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (BMT_status.charger_type == CHARGER_UNKNOWN) {
#else
		if ((BMT_status.charger_type == CHARGER_UNKNOWN) &&
		    (DISO_data.diso_state.cur_vusb_state == DISO_ONLINE)) {
#endif
			mt_charger_type_detection();

			if ((BMT_status.charger_type == STANDARD_HOST)
			    || (BMT_status.charger_type == CHARGING_HOST)) {
				mt_usb_connect();
			}
		}
#endif

#ifdef CONFIG_MTK_BQ25896_SUPPORT
/*New low power feature of MT6531: disable charger CLK without CHARIN.
 * MT6351 API abstracted in charging_hw_bw25896.c.
 * Any charger with MT6351 needs to set this.
 * Compile option is not limited to CONFIG_MTK_BQ25896_SUPPORT.
 * PowerDown = 0
 */
		pwr = 0;
		battery_charging_control(CHARGING_CMD_SET_CHRIND_CK_PDN, &pwr);
#endif

		battery_log(BAT_LOG_FULL,
				"[BAT_thread]Cable in, CHR_Type_num=%d\r\n",
				BMT_status.charger_type);


	} else {
		__pm_relax(battery_suspend_lock);

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

		battery_log(BAT_LOG_CRTI, "[BAT_thread]Cable out \r\n");

		mt_usb_disconnect();


#ifdef CONFIG_MTK_BQ25896_SUPPORT
/*New low power feature of MT6531: disable charger CLK without CHARIN.
 * MT6351 API abstracted in charging_hw_bw25896.c.
 * Any charger with MT6351 needs to set this.
 * Compile option is not limited to CONFIG_MTK_BQ25896_SUPPORT.
 * PowerDown = 1
 */
		pwr = 1;
		battery_charging_control(CHARGING_CMD_SET_CHRIND_CK_PDN, &pwr);
#endif
	}
}

static void mt_kpoc_power_off_check(void)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	battery_log(BAT_LOG_FULL,
		    "[mt_kpoc_power_off_check] , chr_vol=%d, boot_mode=%d\r\n",
		    BMT_status.charger_vol, g_platform_boot_mode);
	if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		if ((upmu_is_chr_det() == false) &&
				(BMT_status.charger_vol < 2500)) {/*vbus<2.5V*/
			battery_log(BAT_LOG_CRTI,
				    "[bat_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
			battery_charging_control(CHARGING_CMD_SET_POWER_OFF, NULL);
		}
	}
#endif
}

void update_battery_2nd_info(int status_smb, int capacity_smb, int present_smb)
{
#if defined(CONFIG_POWER_VERIFY)
	battery_log(BAT_LOG_CRTI, "[update_battery_smb_info] no support\n");
#else
	g_status_smb = status_smb;
	g_capacity_smb = capacity_smb;
	g_present_smb = present_smb;
	battery_log(BAT_LOG_CRTI,
		    "[update_battery_smb_info] get status_smb=%d,capacity_smb=%d,present_smb=%d\n",
		    status_smb, capacity_smb, present_smb);

	wake_up_bat();
	g_smartbook_update = 1;
#endif
}

void do_chrdet_int_task(void)
{
	if (g_bat_init_flag == true) {
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (upmu_is_chr_det() == true) {
#else
		battery_charging_control(
				CHARGING_CMD_GET_DISO_STATE, &DISO_data);
		if ((DISO_data.diso_state.cur_vusb_state == DISO_ONLINE) ||
		    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif
			battery_log(BAT_LOG_CRTI,
				"[do_chrdet_int_task] charger exist!\n");
			BMT_status.charger_exist = true;

			__pm_stay_awake(battery_suspend_lock);

#if defined(CONFIG_POWER_EXT)
			mt_usb_connect();
			battery_log(BAT_LOG_CRTI,
			"[do_chrdet_int_task] call mt_usb_connect() in EVB\n");
#elif defined(CONFIG_MTK_POWER_EXT_DETECT)
			if (true == bat_is_ext_power()) {
				mt_usb_connect();
				battery_log(BAT_LOG_CRTI,
					    "[do_chrdet_int_task] call mt_usb_connect() in EVB\n");
				return;
			}
#endif
		} else {
			battery_log(BAT_LOG_CRTI,
				"[do_chrdet_int_task] charger NOT exist!\n");
			BMT_status.charger_exist = false;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
			battery_log(BAT_LOG_CRTI,
				    "turn off charging for no available charging source\n");
			battery_charging_control(
				CHARGING_CMD_ENABLE, &BMT_status.charger_exist);
#endif

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
			    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
				battery_log(BAT_LOG_CRTI,
					    "[pmic_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
				battery_charging_control(CHARGING_CMD_SET_POWER_OFF, NULL);
			}
#endif

			__pm_relax(battery_suspend_lock);

#if defined(CONFIG_POWER_EXT)
			mt_usb_disconnect();
			battery_log(BAT_LOG_CRTI,
				    "[do_chrdet_int_task] call mt_usb_disconnect() in EVB\n");
#elif defined(CONFIG_MTK_POWER_EXT_DETECT)
			if (true == bat_is_ext_power()) {
				mt_usb_disconnect();
				battery_log(BAT_LOG_CRTI,
					    "[do_chrdet_int_task] call mt_usb_disconnect() in EVB\n");
				return;
			}
#endif

			mtk_pep20_set_is_cable_out_occur(true);
			mtk_pep_set_is_cable_out_occur(true);

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
			is_ta_connect = false;
			ta_check_chr_type = true;
			ta_cable_out_occur = true;
#endif
		}

		/* Place charger detection and battery update here
		 * is used to speed up charging icon display.
		 */

		cable_in_uevent = 1;

		mt_battery_charger_detect_check();
		if (BMT_status.UI_SOC == 100 &&
				BMT_status.charger_exist == true) {
			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = true;
			g_charging_full_reset_bat_meter = true;
		}

		if (g_battery_soc_ready == false) {
			if (BMT_status.nPercent_ZCV == 0)
				battery_meter_initial();

			BMT_status.SOC = battery_meter_get_battery_percentage();
		}

		if (BMT_status.bat_vol > 0) {
			mt_battery_update_status();
			skip_battery_update = true;

		}
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		DISO_data.chr_get_diso_state = true;
#endif

		wake_up_bat();
	} else {
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		g_vcdt_irq_delay_flag = true;
#endif
		battery_log(BAT_LOG_CRTI,
			    "[do_chrdet_int_task] battery thread not ready, will do after bettery init.\n");
	}

}

void BAT_thread(void)
{
	static bool battery_meter_initilized;

	if (battery_meter_initilized == false) {
		/* move from battery_probe() to decrease booting time */
		battery_meter_initial();
		BMT_status.nPercent_ZCV =
			battery_meter_get_battery_nPercent_zcv();
		battery_meter_initilized = true;
#if defined(CONFIG_POWER_EXT)
#else
		BMT_status.SOC = gFG_capacity_by_c;
		BMT_status.UI_SOC = gFG_capacity_by_c;
		BMT_status.ZCV = battery_meter_get_battery_zcv();
		BMT_status.temperatureV = battery_meter_get_tempV();
		BMT_status.temperatureR = battery_meter_get_tempR(BMT_status.temperatureV);
		BMT_status.bat_vol = battery_meter_get_battery_voltage(true);
		BMT_status.temperature = battery_meter_get_battery_temperature();
		battery_update(&battery_main);
		battery_log(BAT_LOG_CRTI, "[battery_meter_initilized] uisoc=soc=%d.\n", gFG_capacity_by_c);
#endif

	}

	mt_battery_charger_detect_check();
	mt_battery_GetBatteryData();
	if (BMT_status.charger_exist == true)
		check_battery_exist();

	mt_battery_thermal_check();
	mt_battery_notify_check();

	if ((BMT_status.charger_exist == true) && (battery_suspended == false)) {
		mt_battery_CheckBatteryStatus();
		mt_battery_charging_algorithm();
	}

	mt_battery_update_status();
	mt_kpoc_power_off_check();
}

/* ////////////////////////////////////////////////////////////////////////// */
/* // Internal API */
/* ////////////////////////////////////////////////////////////////////////// */

#ifdef BATTERY_CDP_WORKAROUND2
/*extern bool is_usb_rdy(void);*/
#endif

int bat_thread_kthread(void *x)
{
	ktime_t ktime = ktime_set(3, 0);	/* 10s, 10* 1000 ms */

#ifdef BATTERY_CDP_WORKAROUND2
	if (is_usb_rdy() == false) {
		battery_log(BAT_LOG_CRTI, "CDP, block\n");
		wait_event(bat_thread_wq, (is_usb_rdy() == true));
		battery_log(BAT_LOG_CRTI, "CDP, free\n");
	} else {
		battery_log(BAT_LOG_CRTI, "CDP, PASS\n");
	}
#endif
#if defined(BATTERY_SW_INIT)
	battery_charging_control(CHARGING_CMD_SW_INIT, NULL);
#endif

	/* Run on a process content */
	while (1) {
		mutex_lock(&bat_mutex);

		if (((chargin_hw_init_done == true) && (battery_suspended == false))
		    || ((chargin_hw_init_done == true) && (fg_wake_up_bat == true)))
			BAT_thread();

		mutex_unlock(&bat_mutex);

#ifdef FG_BAT_INT
		if (fg_wake_up_bat == true) {
			__pm_relax(battery_fg_lock);
			fg_wake_up_bat = false;
			battery_log(BAT_LOG_CRTI, "unlock battery_fg_lock\n");
		}
#endif				/* #ifdef FG_BAT_INT */

		battery_log(BAT_LOG_FULL, "wait event\n");

		wait_event(bat_thread_wq, (bat_thread_timeout == true));

		bat_thread_timeout = false;
		hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);
		ktime = ktime_set(BAT_TASK_PERIOD, 0);	/* 10s, 10* 1000 ms */
		/* for charger plug in/ out */
		if (chr_wake_up_bat == true && g_smartbook_update != 1) {
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
			if (DISO_data.chr_get_diso_state) {
				DISO_data.chr_get_diso_state = false;
				battery_charging_control(
				CHARGING_CMD_GET_DISO_STATE, &DISO_data);
			}
#endif

			g_smartbook_update = 0;
			battery_meter_reset();
			chr_wake_up_bat = false;

			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Charger plug in/out, Call battery_meter_reset. (%d)\n",
				    BMT_status.UI_SOC);
		}

	}

	return 0;
}

void bat_thread_wakeup(void)
{
	battery_log(BAT_LOG_FULL,
			"******** battery : bat_thread_wakeup  ********\n");

	bat_thread_timeout = true;
	bat_meter_timeout = true;
#ifdef MTK_ENABLE_AGING_ALGORITHM
	suspend_time = 0;
#endif
	_g_bat_sleep_total_time = 0;
	wake_up(&bat_thread_wq);
}

/* ////////////////////////////////////////////////////////////////////////// */
/* // fop API */
/* ////////////////////////////////////////////////////////////////////////// */
static long adc_cali_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int *naram_data_addr;
	int i = 0;
	int ret = 0;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_ADC_Cali = false;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		/* enable calibration after setting ADC_CALI_Cal */
		g_ADC_Cali = false;
		/* Protection */
		for (i = 0; i < 14; i++) {
			if ((*(adc_cali_slop + i) == 0) ||
					(*(adc_cali_slop + i) == 1))
				*(adc_cali_slop + i) = 1000;

		}
		for (i = 0; i < 14; i++)
			battery_log(BAT_LOG_CRTI, "adc_cali_slop[%d] = %d\n", i,
				    *(adc_cali_slop + i));
		battery_log(BAT_LOG_FULL,
			"**** unlocked_ioctl : SET_ADC_CALI_Slop Done!\n");
		break;

	case SET_ADC_CALI_Offset:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_offset, naram_data_addr, 36);
		/* enable calibration after setting ADC_CALI_Cal */
		g_ADC_Cali = false;
		for (i = 0; i < 14; i++)
			battery_log(BAT_LOG_CRTI, "adc_cali_offset[%d] = %d\n",
					i, *(adc_cali_offset + i));
		battery_log(BAT_LOG_FULL,
			"**** unlocked_ioctl : SET_ADC_CALI_Offset Done!\n");
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
			battery_log(BAT_LOG_CRTI, "adc_cali_cal[%d] = %d\n",
					i, *(adc_cali_cal + i));
		battery_log(BAT_LOG_FULL,
			"**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		/* g_ADC_Cali = false; *//* 20100508 Infinity */
		user_data_addr = (int *)arg;
		/* 2*int = 2*4 */
		ret = copy_from_user(adc_in_data, user_data_addr, 8);

		if (adc_in_data[0] == 0) {	/* I_SENSE */
			adc_out_data[0] =
				battery_meter_get_VSense() * adc_in_data[1];
		} else if (adc_in_data[0] == 1) {	/* BAT_SENSE */
			adc_out_data[0] =
				battery_meter_get_battery_voltage(true)
				* adc_in_data[1];
		} else if (adc_in_data[0] == 3) {	/* V_Charger */
			adc_out_data[0] =
				battery_meter_get_charger_voltage()
				* adc_in_data[1];
			/* adc_out_data[0] = adc_out_data[0] / 100; */
		} else if (adc_in_data[0] == 30) {
			/* V_Bat_temp magic number */
			adc_out_data[0] =
				battery_meter_get_battery_temperature()
				* adc_in_data[1];
		} else if (adc_in_data[0] == 66) {
			adc_out_data[0] =
				(battery_meter_get_battery_current()) / 10;

			/* charging */
			if (battery_meter_get_battery_current_sign() == true)
				adc_out_data[0] = 0 - adc_out_data[0];

		} else {
			battery_log(BAT_LOG_FULL, "unknown channel(%d,%d)\n",
				    adc_in_data[0], adc_in_data[1]);
		}

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1;	/* failed */
		else
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0;	/* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		battery_log(BAT_LOG_CRTI,
			    "*** unlocked_ioctl : Channel %d * %d times = %d\n",
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
		battery_log(BAT_LOG_CRTI, "**** unlocked_ioctl : CAL:%d\n",
				battery_out_data[0]);
		break;

	case Set_Charger_Current:	/* For Factory Mode */
		mutex_lock(&bat_mutex);

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

		mutex_unlock(&bat_mutex);

		wake_up_bat();
		battery_log(BAT_LOG_CRTI,
				"*** unlocked_ioctl : set_Charger_Current:%d\n",
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
#if defined(CONFIG_DIS_CHECK_BATTERY)
	battery_log(BAT_LOG_CRTI, "[BATTERY] Disable check battery exist.\n");
#else
	unsigned int baton_count = 0;
	unsigned int charging_enable = false;
	unsigned int battery_status;
	unsigned int i;

	for (i = 0; i < 3; i++) {
		battery_charging_control(
			CHARGING_CMD_GET_BATTERY_STATUS, &battery_status);
		baton_count += battery_status;

	}

	if (baton_count >= 3) {
		if ((g_platform_boot_mode == META_BOOT) ||
				(g_platform_boot_mode == ADVMETA_BOOT)
		    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
			battery_log(BAT_LOG_FULL,
				    "[BATTERY] boot mode = %d, bypass battery check\n",
				    g_platform_boot_mode);
		} else {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery is not exist, power off FAN5405 and system (%d)\n",
				    baton_count);

			battery_charging_control(
					CHARGING_CMD_ENABLE, &charging_enable);
#ifdef CONFIG_MTK_POWER_PATH_MANAGEMENT_SUPPORT
			battery_charging_control(
					CHARGING_CMD_SET_PLATFORM_RESET, NULL);
#else
			battery_charging_control(CHARGING_CMD_SET_POWER_OFF, NULL);
#endif
		}
	}
#endif
}


#if defined(MTK_PLUG_OUT_DETECTION)

void charger_plug_out_sw_mode(void)
{
	signed int ICharging = 0;
	signed short i;
	signed short cnt = 0;
	bool enable;
	unsigned int charging_enable;
	signed int VCharger = 0;

	if (BMT_status.charger_exist == true) {
		if (chargin_hw_init_done && upmu_is_chr_det() == true) {

			for (i = 0; i < 4; i++) {
				enable = pmic_get_register_value(PMIC_RG_CHR_EN);

				if (enable == 1) {

					ICharging = battery_meter_get_charging_current_imm();
					VCharger = battery_meter_get_charger_voltage();
					if (ICharging < 70 && VCharger < 4400) {
						cnt++;
						battery_log(BAT_LOG_CRTI,
							    "[charger_hv_detect_sw_thread_handler] fail ICharging=%d , VCHR=%d cnt=%d\n",
							    ICharging, VCharger, cnt);
					} else {
						/* battery_log(BAT_LOG_CRTI,
						 * "[charger_hv_detect_sw_thread_handler] success ICharging=%d,
						 * VCHR=%d cnt=%d\n",ICharging,VCharger,cnt);
						 */
						break;
					}
				} else {
					break;
				}
			}

			if (cnt >= 3) {
				charging_enable = false;
				battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
				battery_log(BAT_LOG_CRTI,
					    "[charger_hv_detect_sw_thread_handler] ICharging=%d VCHR=%d cnt=%d turn off charging\n",
					    ICharging, VCharger, cnt);
			}

		}
	}

}


/*extern unsigned int upmu_get_reg_value(unsigned int reg);*/
void hv_sw_mode(void)
{
	bool hv_status;
	unsigned int charging_enable;

	if (upmu_is_chr_det() == true)
		check_battery_exist();



	if (chargin_hw_init_done)
		battery_charging_control(
				CHARGING_CMD_GET_HV_STATUS, &hv_status);

	if (hv_status == true) {
		battery_log(BAT_LOG_CRTI,
			"[charger_hv_detect_sw_thread_handler] charger hv\n");

		charging_enable = false;
		if (chargin_hw_init_done)
			battery_charging_control(
					CHARGING_CMD_ENABLE, &charging_enable);
	} else
		battery_log(BAT_LOG_FULL,
			    "[charger_hv_detect_sw_thread_handler] upmu_chr_get_vcdt_hv_det() != 1\n");


	/* battery_log(BAT_LOG_CRTI,
	 * "[PMIC_BIAS_GEN_EN & PMIC_BIAS_GEN_EN_SEL] 0xa=0x%x\n",
	 * upmu_get_reg_value(0x000a));
	 */
	if (pmic_get_register_value(PMIC_BIAS_GEN_EN) == 1
	    || pmic_get_register_value(PMIC_BIAS_GEN_EN_SEL) == 0) {
		battery_log(BAT_LOG_CRTI,
			    "[PMIC_BIAS_GEN_EN & PMIC_BIAS_GEN_EN_SEL] be writen 0xa=0x%x\n",
			    upmu_get_reg_value(0x000a));

		#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) || defined(CONFIG_ARCH_MT6753)
		pr_info("HWCID:0x%x\n", pmic_get_register_value(PMIC_HWCID));
		pr_info("VCORE1_CON9:0x%x\n", upmu_get_reg_value(0x0612));
		pr_info("DEW_DIO_EN:0x%x\n", upmu_get_reg_value(0x02d4));
		pr_info("DEW_READ_TEST:0x%x\n", upmu_get_reg_value(0x02d6));
		pmic_config_interface(0x2d8, 0x1234, 0xffff, 0);
		pr_info("DEW_WRITE_TEST:0x%x\n", upmu_get_reg_value(0x02d8));
		pr_info("INT_STATUS0:0x%x\n", upmu_get_reg_value(0x02C4));
		pr_info("INT_STATUS0:0x%x\n", upmu_get_reg_value(0x02C4));
		pmic_config_interface(0x2d8, 0xabcd, 0xffff, 0);
		pr_info("DEW_WRITE_TEST:0x%x\n", upmu_get_reg_value(0x02d8));

		pwrap_dump_ap_register();
		#endif
		WARN_ON(1);
	}

	if (chargin_hw_init_done)
		battery_charging_control(
				CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

}

int charger_hv_detect_sw_thread_handler(void *unused)
{
	ktime_t ktime;
	unsigned int hv_voltage = batt_cust_data.v_charger_max * 1000;


	unsigned char cnt = 0;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	hv_voltage = DISO_data.hv_voltage;
#endif

	do {

		if (BMT_status.charger_exist == true)
			ktime = ktime_set(0, BAT_MS_TO_NS(200));
		else
			ktime = ktime_set(0, BAT_MS_TO_NS(1000));



		if (chargin_hw_init_done)
			battery_charging_control(
				CHARGING_CMD_SET_HV_THRESHOLD, &hv_voltage);

		wait_event_interruptible(charger_hv_detect_waiter,
					 (charger_hv_detect_flag == true));

		if (BMT_status.charger_exist == true) {
			if (cnt >= 5) {
				/* battery_log(BAT_LOG_CRTI, */
				/* "[charger_hv_detect_sw_thread_handler] charger in do hv_sw_mode\n"); */
				hv_sw_mode();
				cnt = 0;
			} else {
				cnt++;
			}

			/* battery_log(BAT_LOG_CRTI, */
			/* "[charger_hv_detect_sw_thread_handler] charger in cnt=%d\n",cnt); */
			charger_plug_out_sw_mode();
		} else {
			/* battery_log(BAT_LOG_CRTI, */
			/* "[charger_hv_detect_sw_thread_handler] charger out do hv_sw_mode\n"); */
			hv_sw_mode();
		}


		charger_hv_detect_flag = false;
		hrtimer_start(&charger_hv_detect_timer,
				ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;
}

#else
int charger_hv_detect_sw_thread_handler(void *unused)
{
	ktime_t ktime;
	unsigned int charging_enable;
	unsigned int hv_voltage = batt_cust_data.v_charger_max * 1000;
	bool hv_status;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	hv_voltage = DISO_data.hv_voltage;
#endif

	do {
#ifdef CONFIG_MTK_BQ25896_SUPPORT
		/*this annoying SW workaround wakes up bat_thread. 10 secs is set instead of 1 sec */
		ktime = ktime_set(BAT_TASK_PERIOD, 0);
#else
		ktime = ktime_set(0, BAT_MS_TO_NS(1000));
#endif
		if (chargin_hw_init_done)
			battery_charging_control(
				CHARGING_CMD_SET_HV_THRESHOLD, &hv_voltage);
*
		wait_event_interruptible(charger_hv_detect_waiter,
					 (charger_hv_detect_flag == true));

		if (upmu_is_chr_det() == true)
			check_battery_exist();




		charger_hv_detect_flag = false;

		if (chargin_hw_init_done)
			battery_charging_control(
					CHARGING_CMD_GET_HV_STATUS, &hv_status);

		if (hv_status == true) {
			battery_log(BAT_LOG_CRTI,
			"[charger_hv_detect_sw_thread_handler] charger hv\n");

			charging_enable = false;
			if (chargin_hw_init_done)
				battery_charging_control(
					CHARGING_CMD_ENABLE, &charging_enable);
		} else {
			battery_log(BAT_LOG_FULL,
				    "[charger_hv_detect_sw_thread_handler] upmu_chr_get_vcdt_hv_det() != 1\n");
		}



		if (chargin_hw_init_done)
			battery_charging_control(
				CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

		hrtimer_start(&charger_hv_detect_timer,
				ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;
}
#endif				/* #if defined(MTK_PLUG_OUT_DETECTION) */

enum hrtimer_restart charger_hv_detect_sw_workaround(struct hrtimer *timer)
{
	charger_hv_detect_flag = true;
	wake_up_interruptible(&charger_hv_detect_waiter);

	battery_log(BAT_LOG_FULL, "[charger_hv_detect_sw_workaround]\n");

	return HRTIMER_NORESTART;
}

void charger_hv_detect_sw_workaround_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, BAT_MS_TO_NS(2000));
	hrtimer_init(&charger_hv_detect_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	charger_hv_detect_timer.function = charger_hv_detect_sw_workaround;
	hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);

	charger_hv_detect_thread =
	    kthread_run(charger_hv_detect_sw_thread_handler, 0,
			"mtk charger_hv_detect_sw_workaround");
	if (IS_ERR(charger_hv_detect_thread)) {
		battery_log(BAT_LOG_FULL,
			    "[%s]: failed to create charger_hv_detect_sw_workaround thread\n",
			    __func__);
	}
	battery_log(BAT_LOG_CRTI, "charger_hv_detect_sw_workaround_init : done\n");
}


enum hrtimer_restart battery_kthread_hrtimer_func(struct hrtimer *timer)
{
	bat_thread_wakeup();

	return HRTIMER_NORESTART;
}

void battery_kthread_hrtimer_init(void)
{
	ktime_t ktime;
#ifdef CONFIG_MTK_BQ25896_SUPPORT
/*watchdog timer before 40 secs*/
	ktime = ktime_set(BAT_TASK_PERIOD, 0);	/* 3s, 10* 1000 ms */
#else
	ktime = ktime_set(1, 0);
#endif
	hrtimer_init(&battery_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	battery_kthread_timer.function = battery_kthread_hrtimer_func;
	hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);

	battery_log(BAT_LOG_CRTI, "battery_kthread_hrtimer_init : done\n");
}


static void get_charging_control(void)
{
#if defined(CONFIG_ONTIM_POWER_DRIVER)
	int idx = 0;

	if (battery_charging_control == NULL) {
		for (idx = 0; idx < sizeof(charger_candidate_func)/sizeof(struct charger_candidate_table); idx++) {
			if (charger_candidate_func[idx].exist_fun() == 0) {
					battery_log(BAT_LOG_CRTI, "charger %s found\n",
						charger_candidate_func[idx].name);
				battery_charging_control = charger_candidate_func[idx].chr_ctrl_intf;
				break;
			}
		}

		if (battery_charging_control == NULL)
			battery_log(BAT_LOG_CRTI, "can't find any charger driver\n");
	}
#else
	battery_charging_control = chr_control_interface;
#endif
}

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
static irqreturn_t diso_auxadc_irq_thread(int irq, void *dev_id)
{
	int pre_diso_state = (DISO_data.diso_state.pre_otg_state |
			      (DISO_data.diso_state.pre_vusb_state << 1) |
			      (DISO_data.diso_state.pre_vdc_state << 2)) & 0x7;

	battery_log(BAT_LOG_CRTI,
	"[DISO]auxadc IRQ threaded handler triggered, pre_diso_state is %s\n",
	DISO_state_s[pre_diso_state]);

	switch (pre_diso_state) {
#ifdef MTK_DISCRETE_SWITCH	/*for DSC DC plugin handle */
	case USB_ONLY:
#endif
	case OTG_ONLY:
		BMT_status.charger_exist = true;
		__pm_stay_awake(battery_suspend_lock);
		wake_up_bat();
		break;
	case DC_WITH_OTG:
		BMT_status.charger_exist = false;
		/* need stop charger quickly */
		battery_charging_control(
				CHARGING_CMD_ENABLE, &BMT_status.charger_exist);
		/* reset charger status */
		BMT_status.charger_exist = false;
		BMT_status.charger_type = CHARGER_UNKNOWN;
		__pm_relax(battery_suspend_lock);
		wake_up_bat();
		break;
	case DC_WITH_USB:
		/*usb delayed work will reflact BMT_staus,
		 * so need update state ASAP
		 */
		if ((BMT_status.charger_type == STANDARD_HOST)
		    || (BMT_status.charger_type == CHARGING_HOST))
			mt_usb_disconnect();	/* disconnect if connected */
		BMT_status.charger_type = CHARGER_UNKNOWN;/* reset chr_type */
		wake_up_bat();
		break;
	case DC_ONLY:
		BMT_status.charger_type = CHARGER_UNKNOWN;
		/* plug in VUSB, check if need connect usb */
		mt_battery_charger_detect_check();
		break;
	default:
		battery_log(BAT_LOG_CRTI,
		"[DISO]VUSB auxadc threaded handler triggered ERROR OR TEST\n");
		break;
	}
	return IRQ_HANDLED;
}

static void dual_input_init(void)
{
	DISO_data.irq_callback_func = diso_auxadc_irq_thread;
	battery_charging_control(CHARGING_CMD_DISO_INIT, &DISO_data);
}
#endif

int __batt_init_cust_data_from_cust_header(void)
{
	/* mt_charging.h */
	/* stop charging while in talking mode */
#if defined(STOP_CHARGING_IN_TAKLING)
	batt_cust_data.stop_charging_in_takling = 1;
#else				/* #if defined(STOP_CHARGING_IN_TAKLING) */
	batt_cust_data.stop_charging_in_takling = 0;
#endif				/* #if defined(STOP_CHARGING_IN_TAKLING) */

#if defined(TALKING_RECHARGE_VOLTAGE)
	batt_cust_data.talking_recharge_voltage = TALKING_RECHARGE_VOLTAGE;
#endif

#if defined(TALKING_SYNC_TIME)
	batt_cust_data.talking_sync_time = TALKING_SYNC_TIME;
#endif

	/* Battery Temperature Protection */
#if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT)
	batt_cust_data.mtk_temperature_recharge_support = 1;
#else	/* #if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT) */
	batt_cust_data.mtk_temperature_recharge_support = 0;
#endif	/* #if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT) */

#if defined(MAX_CHARGE_TEMPERATURE)
	batt_cust_data.max_charge_temperature = MAX_CHARGE_TEMPERATURE;
#endif

#if defined(MAX_CHARGE_TEMPERATURE_MINUS_X_DEGREE)
	batt_cust_data.max_charge_temperature_minus_x_degree =
	    MAX_CHARGE_TEMPERATURE_MINUS_X_DEGREE;
#endif

#if defined(MIN_CHARGE_TEMPERATURE)
	batt_cust_data.min_charge_temperature = MIN_CHARGE_TEMPERATURE;
#endif

#if defined(MIN_CHARGE_TEMPERATURE_PLUS_X_DEGREE)
	batt_cust_data.min_charge_temperature_plus_x_degree =
				MIN_CHARGE_TEMPERATURE_PLUS_X_DEGREE;
#endif

#if defined(ERR_CHARGE_TEMPERATURE)
	batt_cust_data.err_charge_temperature = ERR_CHARGE_TEMPERATURE;
#endif

	/* Linear Charging Threshold */
#if defined(V_PRE2CC_THRES)
	batt_cust_data.v_pre2cc_thres = V_PRE2CC_THRES;
#endif
#if defined(V_CC2TOPOFF_THRES)
	batt_cust_data.v_cc2topoff_thres = V_CC2TOPOFF_THRES;
#endif
#if defined(RECHARGING_VOLTAGE)
	batt_cust_data.recharging_voltage = RECHARGING_VOLTAGE;
#endif
#if defined(CHARGING_FULL_CURRENT)
	batt_cust_data.charging_full_current = CHARGING_FULL_CURRENT;
#endif

	/* Charging Current Setting */
#if defined(CONFIG_USB_IF)
	batt_cust_data.config_usb_if = 1;
#else				/* #if defined(CONFIG_USB_IF) */
	batt_cust_data.config_usb_if = 0;
#endif				/* #if defined(CONFIG_USB_IF) */

#if defined(USB_CHARGER_CURRENT_SUSPEND)
	batt_cust_data.usb_charger_current_suspend =
				USB_CHARGER_CURRENT_SUSPEND;
#endif
#if defined(USB_CHARGER_CURRENT_UNCONFIGURED)
	batt_cust_data.usb_charger_current_unconfigured =
				USB_CHARGER_CURRENT_UNCONFIGURED;
#endif
#if defined(USB_CHARGER_CURRENT_CONFIGURED)
	batt_cust_data.usb_charger_current_configured =
				USB_CHARGER_CURRENT_CONFIGURED;
#endif
#if defined(USB_CHARGER_CURRENT)
	batt_cust_data.usb_charger_current =
				USB_CHARGER_CURRENT;
#endif
#if defined(AC_CHARGER_INPUT_CURRENT)
	batt_cust_data.ac_charger_input_current =
				AC_CHARGER_INPUT_CURRENT;
#endif
#if defined(AC_CHARGER_CURRENT)
	batt_cust_data.ac_charger_current =
				AC_CHARGER_CURRENT;
#endif
#if defined(NON_STD_AC_CHARGER_CURRENT)
	batt_cust_data.non_std_ac_charger_current =
				NON_STD_AC_CHARGER_CURRENT;
#endif
#if defined(CHARGING_HOST_CHARGER_CURRENT)
	batt_cust_data.charging_host_charger_current =
				CHARGING_HOST_CHARGER_CURRENT;
#endif
#if defined(APPLE_0_5A_CHARGER_CURRENT)
	batt_cust_data.apple_0_5a_charger_current =
				APPLE_0_5A_CHARGER_CURRENT;
#endif
#if defined(APPLE_1_0A_CHARGER_CURRENT)
	batt_cust_data.apple_1_0a_charger_current =
				APPLE_1_0A_CHARGER_CURRENT;
#endif
#if defined(APPLE_2_1A_CHARGER_CURRENT)
	batt_cust_data.apple_2_1a_charger_current =
				APPLE_2_1A_CHARGER_CURRENT;
#endif

	/* Precise Tunning
	 * batt_cust_data.battery_average_data_number =
	 * BATTERY_AVERAGE_DATA_NUMBER;
	 * batt_cust_data.battery_average_size = BATTERY_AVERAGE_SIZE;
	 */


	/* charger error check */
#if defined(BAT_LOW_TEMP_PROTECT_ENABLE)
	batt_cust_data.bat_low_temp_protect_enable = 1;
#else	/* #if defined(BAT_LOW_TEMP_PROTECT_ENABLE) */
	batt_cust_data.bat_low_temp_protect_enable = 0;
#endif	/* #if defined(BAT_LOW_TEMP_PROTECT_ENABLE) */

#if defined(V_CHARGER_ENABLE)
	batt_cust_data.v_charger_enable = V_CHARGER_ENABLE;
#endif
#if defined(V_CHARGER_MAX)
	batt_cust_data.v_charger_max = V_CHARGER_MAX;
#endif
#if defined(V_CHARGER_MIN)
	batt_cust_data.v_charger_min = V_CHARGER_MIN;
#endif


	/* Tracking TIME */
#if defined(ONEHUNDRED_PERCENT_TRACKING_TIME)
	batt_cust_data.onehundred_percent_tracking_time =
					ONEHUNDRED_PERCENT_TRACKING_TIME;
#endif
#if defined(NPERCENT_TRACKING_TIME)
	batt_cust_data.npercent_tracking_time =
					NPERCENT_TRACKING_TIME;
#endif
#if defined(SYNC_TO_REAL_TRACKING_TIME)
	batt_cust_data.sync_to_real_tracking_time =
					SYNC_TO_REAL_TRACKING_TIME;
#endif
#if defined(V_0PERCENT_TRACKING)
	batt_cust_data.v_0percent_tracking =
					V_0PERCENT_TRACKING;
#endif

	/* High battery support */
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	batt_cust_data.high_battery_voltage_support = 1;
#else	/* #if defined(HIGH_BATTERY_VOLTAGE_SUPPORT) */
	batt_cust_data.high_battery_voltage_support = 0;
#endif	/* #if defined(HIGH_BATTERY_VOLTAGE_SUPPORT) */

#if	defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	batt_cust_data.mtk_pump_express_plus_support = 1;

#if defined(TA_START_BATTERY_SOC)
	batt_cust_data.ta_start_battery_soc = TA_START_BATTERY_SOC;
#endif
#if defined(TA_STOP_BATTERY_SOC)
	batt_cust_data.ta_stop_battery_soc = TA_STOP_BATTERY_SOC;
#endif
#if defined(TA_AC_12V_INPUT_CURRENT)
	batt_cust_data.ta_ac_12v_input_current = TA_AC_12V_INPUT_CURRENT;
#endif
#if defined(TA_AC_9V_INPUT_CURRENT)
	batt_cust_data.ta_ac_9v_input_current = TA_AC_9V_INPUT_CURRENT;
#endif
#if defined(TA_AC_7V_INPUT_CURRENT)
	batt_cust_data.ta_ac_7v_input_current = TA_AC_7V_INPUT_CURRENT;
#endif
#if defined(TA_AC_CHARGING_CURRENT)
	batt_cust_data.ta_ac_charging_current = TA_AC_CHARGING_CURRENT;
#endif
#if defined(TA_9V_SUPPORT)
	batt_cust_data.ta_9v_support = 1;
#endif
#if defined(TA_12V_SUPPORT)
	batt_cust_data.ta_12v_support = 1;
#endif
#endif

	return 0;
}

#if defined(BATTERY_DTS_SUPPORT) && defined(CONFIG_OF)
static void __batt_parse_node(const struct device_node *np,
		const char *node_srting, int *cust_val)
{
	u32 val;

	if (of_property_read_u32(np, node_srting, &val) == 0) {
		(*cust_val) = (int)val;
		battery_log(BAT_LOG_FULL,
				"Get %s: %d\n", node_srting, (*cust_val));
	} else {
		battery_log(BAT_LOG_CRTI, "Get %s failed\n", node_srting);
	}
}

static int __batt_init_cust_data_from_dt(void)
{
	/* struct device_node *np = dev->dev.of_node; */
	struct device_node *np;

	/* check customer setting */
	np = of_find_compatible_node(NULL, NULL, "mediatek,battery");
	if (!np) {
		battery_log(BAT_LOG_CRTI,
				"Failed to find device-tree node: bat_comm\n");
		return -ENODEV;
	}

	__batt_parse_node(np, "stop_charging_in_takling",
			&batt_cust_data.stop_charging_in_takling);

	__batt_parse_node(np, "talking_recharge_voltage",
			&batt_cust_data.talking_recharge_voltage);

	__batt_parse_node(np, "talking_sync_time",
			&batt_cust_data.talking_sync_time);

	__batt_parse_node(np, "mtk_temperature_recharge_support",
			  &batt_cust_data.mtk_temperature_recharge_support);

	__batt_parse_node(np, "max_charge_temperature",
			&batt_cust_data.max_charge_temperature);

	__batt_parse_node(np, "max_charge_temperature_minus_x_degree",
			  &batt_cust_data.max_charge_temperature_minus_x_degree);

	__batt_parse_node(np, "min_charge_temperature",
			&batt_cust_data.min_charge_temperature);

	__batt_parse_node(np, "min_charge_temperature_plus_x_degree",
			  &batt_cust_data.min_charge_temperature_plus_x_degree);

	__batt_parse_node(np, "err_charge_temperature",
			&batt_cust_data.err_charge_temperature);

	__batt_parse_node(np, "v_pre2cc_thres",
			&batt_cust_data.v_pre2cc_thres);

	__batt_parse_node(np, "v_cc2topoff_thres",
			&batt_cust_data.v_cc2topoff_thres);

	__batt_parse_node(np, "recharging_voltage",
			&batt_cust_data.recharging_voltage);

	__batt_parse_node(np, "charging_full_current",
			&batt_cust_data.charging_full_current);

	__batt_parse_node(np, "config_usb_if",
			&batt_cust_data.config_usb_if);

	__batt_parse_node(np, "usb_charger_current_suspend",
			&batt_cust_data.usb_charger_current_suspend);

	__batt_parse_node(np, "usb_charger_current_unconfigured",
			&batt_cust_data.usb_charger_current_unconfigured);

	__batt_parse_node(np, "usb_charger_current_configured",
			&batt_cust_data.usb_charger_current_configured);

	__batt_parse_node(np, "usb_charger_current",
			&batt_cust_data.usb_charger_current);

	__batt_parse_node(np, "ac_charger_input_current",
			&batt_cust_data.ac_charger_input_current);

	__batt_parse_node(np, "ac_charger_current",
			&batt_cust_data.ac_charger_current);

	__batt_parse_node(np, "non_std_ac_charger_current",
			&batt_cust_data.non_std_ac_charger_current);

	__batt_parse_node(np, "charging_host_charger_current",
			&batt_cust_data.charging_host_charger_current);

	__batt_parse_node(np, "apple_0_5a_charger_current",
			&batt_cust_data.apple_0_5a_charger_current);

	__batt_parse_node(np, "apple_1_0a_charger_current",
			&batt_cust_data.apple_1_0a_charger_current);

	__batt_parse_node(np, "apple_2_1a_charger_current",
			&batt_cust_data.apple_2_1a_charger_current);

	__batt_parse_node(np, "bat_low_temp_protect_enable",
			&batt_cust_data.bat_low_temp_protect_enable);

	__batt_parse_node(np, "v_charger_enable",
			&batt_cust_data.v_charger_enable);

	__batt_parse_node(np, "v_charger_max",
			&batt_cust_data.v_charger_max);

	__batt_parse_node(np, "v_charger_min",
			&batt_cust_data.v_charger_min);

	__batt_parse_node(np, "onehundred_percent_tracking_time",
			&batt_cust_data.onehundred_percent_tracking_time);

	__batt_parse_node(np, "npercent_tracking_time",
			&batt_cust_data.npercent_tracking_time);

	__batt_parse_node(np, "sync_to_real_tracking_time",
			&batt_cust_data.sync_to_real_tracking_time);

	__batt_parse_node(np, "v_0percent_tracking",
			&batt_cust_data.v_0percent_tracking);

	__batt_parse_node(np, "high_battery_voltage_support",
			&batt_cust_data.high_battery_voltage_support);

	__batt_parse_node(np, "mtk_jeita_standard_support",
			&batt_cust_data.mtk_jeita_standard_support);

	__batt_parse_node(np, "cust_soc_jeita_sync_time",
			&batt_cust_data.cust_soc_jeita_sync_time);

	__batt_parse_node(np, "jeita_recharge_voltage",
			&batt_cust_data.jeita_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_above_pos_60_cv_voltage",
			&batt_cust_data.jeita_temp_above_pos_60_cv_voltage);

	__batt_parse_node(np, "jeita_temp_pos_10_to_pos_45_cv_voltage",
			&batt_cust_data.jeita_temp_pos_10_to_pos_45_cv_voltage);

	__batt_parse_node(np, "jeita_temp_pos_0_to_pos_10_cv_voltage",
			&batt_cust_data.jeita_temp_pos_0_to_pos_10_cv_voltage);

	__batt_parse_node(np, "jeita_temp_neg_10_to_pos_0_cv_voltage",
			&batt_cust_data.jeita_temp_neg_10_to_pos_0_cv_voltage);

	__batt_parse_node(np, "jeita_temp_below_neg_10_cv_voltage",
			&batt_cust_data.jeita_temp_below_neg_10_cv_voltage);

	__batt_parse_node(np, "jeita_neg_10_to_pos_0_full_current",
			&batt_cust_data.jeita_neg_10_to_pos_0_full_current);

	__batt_parse_node(np, "jeita_temp_pos_45_to_pos_60_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_45_to_pos_60_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_pos_10_to_pos_45_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_10_to_pos_45_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_pos_0_to_pos_10_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_0_to_pos_10_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_neg_10_to_pos_0_recharge_voltage",
		&batt_cust_data.jeita_temp_neg_10_to_pos_0_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_pos_45_to_pos_60_cc2topoff_threshold",
	&batt_cust_data.jeita_temp_pos_45_to_pos_60_cc2topoff_threshold);

	__batt_parse_node(np, "jeita_temp_pos_10_to_pos_45_cc2topoff_threshold",
	&batt_cust_data.jeita_temp_pos_10_to_pos_45_cc2topoff_threshold);

	__batt_parse_node(np, "jeita_temp_pos_0_to_pos_10_cc2topoff_threshold",
		&batt_cust_data.jeita_temp_pos_0_to_pos_10_cc2topoff_threshold);

	__batt_parse_node(np, "jeita_temp_neg_10_to_pos_0_cc2topoff_threshold",
		&batt_cust_data.jeita_temp_neg_10_to_pos_0_cc2topoff_threshold);

#if	defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	__batt_parse_node(np, "mtk_pump_express_plus_support",
			  &batt_cust_data.mtk_pump_express_plus_support);

	__batt_parse_node(np, "ta_start_battery_soc",
			&batt_cust_data.ta_start_battery_soc);

	__batt_parse_node(np, "ta_stop_battery_soc",
			&batt_cust_data.ta_stop_battery_soc);

	__batt_parse_node(np, "ta_ac_12v_input_current",
			&batt_cust_data.ta_ac_12v_input_current);

	__batt_parse_node(np, "ta_ac_9v_input_current",
			&batt_cust_data.ta_ac_9v_input_current);

	__batt_parse_node(np, "ta_ac_7v_input_current",
			&batt_cust_data.ta_ac_7v_input_current);

	__batt_parse_node(np, "ta_ac_charging_current",
			&batt_cust_data.ta_ac_charging_current);

	__batt_parse_node(np, "ta_9v_support",
			&batt_cust_data.ta_9v_support);

	__batt_parse_node(np, "ta_12v_support",
			&batt_cust_data.ta_12v_support);
#endif

	of_node_put(np);
	return 0;
}
#endif

int batt_init_cust_data(void)
{
	__batt_init_cust_data_from_cust_header();

#if defined(BATTERY_DTS_SUPPORT) && defined(CONFIG_OF)
	battery_log(BAT_LOG_CRTI, "battery custom init by DTS\n");
	__batt_init_cust_data_from_dt();
#endif
	return 0;
}

static int battery_probe(struct platform_device *dev)
{
	struct class_device *class_dev = NULL;
	int ret = 0;

	battery_log(BAT_LOG_CRTI, "******** battery driver probe!! ********\n");

	/* Integrate with NVRAM */
	ret = alloc_chrdev_region(&adc_cali_devno, 0, 1, ADC_CALI_DEVNAME);
	if (ret)
		battery_log(BAT_LOG_CRTI,
				"Error: Can't Get Major number for adc_cali\n");
	adc_cali_cdev = cdev_alloc();
	adc_cali_cdev->owner = THIS_MODULE;
	adc_cali_cdev->ops = &adc_cali_fops;
	ret = cdev_add(adc_cali_cdev, adc_cali_devno, 1);
	if (ret)
		battery_log(BAT_LOG_CRTI, "adc_cali Error: cdev_add\n");
	adc_cali_major = MAJOR(adc_cali_devno);
	adc_cali_class = class_create(THIS_MODULE, ADC_CALI_DEVNAME);
	class_dev = (struct class_device *)device_create(adc_cali_class,
			NULL, adc_cali_devno, NULL, ADC_CALI_DEVNAME);
	battery_log(BAT_LOG_CRTI, "[BAT_probe] adc_cali prepare : done !!\n ");

	get_charging_control();

	batt_init_cust_data();

	battery_charging_control(
		CHARGING_CMD_GET_PLATFORM_BOOT_MODE, &g_platform_boot_mode);
	battery_log(BAT_LOG_CRTI,
			"[BAT_probe] g_platform_boot_mode = %d\n ",
			g_platform_boot_mode);

	battery_fg_lock = wakeup_source_register("battery fg wakelock");

	battery_suspend_lock =
		wakeup_source_register("battery suspend wakelock");
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
	wake_lock_init(&TA_charger_suspend_lock, WAKE_LOCK_SUSPEND,
			"TA charger suspend wakelock");
#endif

	mtk_pep_init();
	mtk_pep20_init();

	/* Integrate with Android Battery Service */
	ac_main.psy = power_supply_register(&(dev->dev), &ac_main.psd, NULL);
	if (IS_ERR(ac_main.psy)) {
		battery_log(BAT_LOG_CRTI,
			"[BAT_probe] power_supply_register AC Fail !!\n");
		ret = PTR_ERR(ac_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI,
			"[BAT_probe] power_supply_register AC Success !!\n");

	usb_main.psy = power_supply_register(&(dev->dev), &usb_main.psd, NULL);
	if (IS_ERR(usb_main.psy)) {
		battery_log(BAT_LOG_CRTI,
			"[BAT_probe] power_supply_register USB Fail !!\n");
		ret = PTR_ERR(usb_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI,
			"[BAT_probe] power_supply_register USB Success !!\n");

	wireless_main.psy = power_supply_register(
				&(dev->dev), &wireless_main.psd, NULL);
	if (IS_ERR(wireless_main.psy)) {
		battery_log(BAT_LOG_CRTI,
			"[BAT_probe] power_supply_register WIRELESS Fail !!\n");
		ret = PTR_ERR(wireless_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI,
		"[BAT_probe] power_supply_register WIRELESS Success !!\n");

	battery_main.psy = power_supply_register(
				&(dev->dev), &battery_main.psd, NULL);
	if (IS_ERR(battery_main.psy)) {
		battery_log(BAT_LOG_CRTI,
			"[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(battery_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI,
		"[BAT_probe] power_supply_register Battery Success !!\n");

#if !defined(CONFIG_POWER_EXT)

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power()) {
		battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
		battery_main.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
		battery_main.BAT_PRESENT = 1;
		battery_main.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
		battery_main.BAT_CAPACITY = 100;
		battery_main.BAT_batt_vol = 4200;
		battery_main.BAT_batt_temp = 220;

		g_bat_init_flag = true;
		return 0;
	}
#endif
	/* For EM */
	{
		int ret_device_file = 0;

		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Charger_Voltage);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_0_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_1_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_2_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_3_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_4_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_5_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_6_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_7_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_8_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_9_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_10_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_11_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_12_Slope);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_13_Slope);

		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_0_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_1_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_2_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_3_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_4_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_5_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_6_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_7_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_8_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_9_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_10_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_11_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_12_Offset);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_ADC_Channel_13_Offset);

		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_Is_Calibration);

		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_Power_On_Voltage);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_Power_Off_Voltage);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_Charger_TopOff_Value);

		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_FG_Battery_CurrentConsumption);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_FG_SW_CoulombCounter);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_Charging_CallState);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_V_0Percent_Tracking);

		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_Charger_Type);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_Pump_Express);
		ret_device_file = device_create_file(
				&(dev->dev), &dev_attr_FG_daemon_disable);
	}

	/* move to mt_battery_GetBatteryData() to decrease booting time */
	/* battery_meter_initial(); */

	/* Initialization BMT Struct */
	BMT_status.bat_exist = true;		/* phone must have battery */
	BMT_status.charger_exist = false;	/* for default, no charger */
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
	BMT_status.UI_SOC = 0;

	BMT_status.bat_charging_state = CHR_PRE;
	BMT_status.bat_in_recharging_state = false;
	BMT_status.bat_full = false;
	BMT_status.nPercent_ZCV = 0;
	BMT_status.nPrecent_UI_SOC_check_point =
		battery_meter_get_battery_nPercent_UI_SOC();

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	dual_input_init();
#endif

	/* battery kernel thread for 10s check and charger in/out event */
	/* Replace GPT timer by hrtime */
	battery_kthread_hrtimer_init();

	kthread_run(bat_thread_kthread, NULL, "bat_thread_kthread");
	battery_log(BAT_LOG_CRTI, "[battery_probe] bat_thread_kthread Done\n");


	charger_hv_detect_sw_workaround_init();


	/*LOG System Set */
	init_proc_log();

#else
	/* keep HW alive */
	/* charger_hv_detect_sw_workaround_init(); */
#endif
	g_bat_init_flag = true;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if ((g_vcdt_irq_delay_flag == true) || (upmu_is_chr_det() == true))
		do_chrdet_int_task();
#endif

	return 0;
}

static void battery_timer_pause(void)
{


	/* battery_log(BAT_LOG_CRTI, "**** battery driver suspend!! ****n" ); */
#ifdef CONFIG_POWER_EXT
#else

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power())
		return 0;
#endif
	mutex_lock(&bat_mutex);
	/* cancel timer */
	hrtimer_cancel(&battery_kthread_timer);
	hrtimer_cancel(&charger_hv_detect_timer);

	battery_suspended = true;
	mutex_unlock(&bat_mutex);

	battery_log(BAT_LOG_FULL, "@bs=1@\n");
#endif

	get_monotonic_boottime(&g_bat_time_before_sleep);
}

static void battery_timer_resume(void)
{
#ifdef CONFIG_POWER_EXT
#else
	bool is_pcm_timer_trigger = false;
	struct timespec bat_time_after_sleep;
	ktime_t ktime, hvtime;

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power())
		return 0;
#endif

	ktime = ktime_set(BAT_TASK_PERIOD, 0);	/* 10s, 10* 1000 ms */
	hvtime = ktime_set(0, BAT_MS_TO_NS(2000));

	get_monotonic_boottime(&bat_time_after_sleep);
	battery_charging_control(
		CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER, &is_pcm_timer_trigger);

	if (is_pcm_timer_trigger == true || bat_spm_timeout) {
		mutex_lock(&bat_mutex);
		BAT_thread();
		mutex_unlock(&bat_mutex);
	} else {
		battery_log(BAT_LOG_CRTI, "battery resume NOT by pcm timer!\n");
	}

	if (g_call_state == CALL_ACTIVE &&
	    (bat_time_after_sleep.tv_sec - g_bat_time_before_sleep.tv_sec >=
	     batt_cust_data.talking_sync_time)) {
		/* phone call last than x min */
		BMT_status.UI_SOC = battery_meter_get_battery_percentage();
		battery_log(BAT_LOG_CRTI, "Sync UI SOC to SOC immediately\n");
	}

	mutex_lock(&bat_mutex);

	/* restore timer */
	hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);
	hrtimer_start(&charger_hv_detect_timer, hvtime, HRTIMER_MODE_REL);

	battery_suspended = false;
	battery_log(BAT_LOG_FULL, "@bs=0@\n");
	mutex_unlock(&bat_mutex);

#endif
}

static int battery_remove(struct platform_device *dev)
{
	battery_log(BAT_LOG_CRTI,
			"******** battery driver remove!! ********\n");

	return 0;
}

static void battery_shutdown(struct platform_device *dev)
{
	if (mtk_pep_get_is_connect() || mtk_pep20_get_is_connect()) {
		CHR_CURRENT_ENUM input_current = CHARGE_CURRENT_70_00_MA;

		battery_charging_control(
				CHARGING_CMD_SET_INPUT_CURRENT, &input_current);
		battery_log(BAT_LOG_CRTI,
				"%s: reset TA before shutdown\n", __func__);
	}
}

/* ////////////////////////////////////////////////////////////////////////// */
/* // Battery Notify API */
/* ////////////////////////////////////////////////////////////////////////// */
static ssize_t show_BatteryNotify(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI,
		"[Battery] show_BatteryNotify : %x\n", g_BatteryNotifyCode);

	return sprintf(buf, "%u\n", g_BatteryNotifyCode);
}

static ssize_t store_BatteryNotify(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	/*char *pvalue = NULL; */
	int rv;
	unsigned long reg_BatteryNotifyCode = 0;

	battery_log(BAT_LOG_CRTI, "[Battery] store_BatteryNotify\n");
	if (buf != NULL && size != 0) {
		battery_log(BAT_LOG_CRTI,
			"[Battery] buf is %s and size is %Zu\n", buf, size);
		rv = kstrtoul(buf, 0, &reg_BatteryNotifyCode);
		if (rv != 0)
			return -EINVAL;
		g_BatteryNotifyCode = reg_BatteryNotifyCode;
		battery_log(BAT_LOG_CRTI,
			"[Battery] store code : %x\n", g_BatteryNotifyCode);
	}
	return size;
}

static DEVICE_ATTR(BatteryNotify, 0664,
		show_BatteryNotify, store_BatteryNotify);

static ssize_t show_BN_TestMode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI,
			"[Battery] show_BN_TestMode : %x\n", g_BN_TestMode);
	return sprintf(buf, "%u\n", g_BN_TestMode);
}

static ssize_t store_BN_TestMode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	/*char *pvalue = NULL; */
	int rv;
	unsigned long reg_BN_TestMode = 0;

	battery_log(BAT_LOG_CRTI, "[Battery] store_BN_TestMode\n");
	if (buf != NULL && size != 0) {
		battery_log(BAT_LOG_CRTI,
			"[Battery] buf is %s and size is %Zu\n", buf, size);
		rv = kstrtoul(buf, 0, &reg_BN_TestMode);
		if (rv != 0)
			return -EINVAL;
		g_BN_TestMode = reg_BN_TestMode;
		battery_log(BAT_LOG_CRTI,
			"[Battery] store g_BN_TestMode : %x\n", g_BN_TestMode);
	}
	return size;
}

static DEVICE_ATTR(BN_TestMode, 0664, show_BN_TestMode, store_BN_TestMode);


/* ////////////////////////////////////////////////////////////////////////// */
/* // platform_driver API */
/* ////////////////////////////////////////////////////////////////////////// */
#if 0
static int battery_cmd_read(char *buf,
		char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p,
		     "g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		     g_battery_thermal_throttling_flag, battery_cmd_thermal_test_mode,
		     battery_cmd_thermal_test_mode_value);

	*start = buf + off;

	len = p - buf;
	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
}
#endif

static ssize_t battery_cmd_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, bat_tt_enable = 0, bat_thr_test_mode = 0, bat_thr_test_value = 0;
	char desc[32];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d", &bat_tt_enable, &bat_thr_test_mode, &bat_thr_test_value) == 3) {
		g_battery_thermal_throttling_flag = bat_tt_enable;
		battery_cmd_thermal_test_mode = bat_thr_test_mode;
		battery_cmd_thermal_test_mode_value = bat_thr_test_value;

		battery_log(BAT_LOG_CRTI,
			    "bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			    g_battery_thermal_throttling_flag,
			    battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);

		return count;
	}			/*else { */
	battery_log(BAT_LOG_CRTI,
		    "  bad argument, echo [bat_tt_enable] [bat_thr_test_mode] [bat_thr_test_value] > battery_cmd\n");
	/*} */

	return -EINVAL;
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		   "=> g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		   g_battery_thermal_throttling_flag, battery_cmd_thermal_test_mode,
		   battery_cmd_thermal_test_mode_value);

	seq_printf(m, "=> get_usb_current_unlimited=%d,\ncmd_discharging = %d\n",
		   get_usb_current_unlimited(), cmd_discharging);
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
	int cmd_current_unlimited = false;
	unsigned int charging_enable = false;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &cmd_current_unlimited, &cmd_discharging) == 2) {
		set_usb_current_unlimited(cmd_current_unlimited);
		if (cmd_discharging == 1) {
			charging_enable = false;
			adjust_power = -1;
		} else if (cmd_discharging == 0) {
			charging_enable = true;
			adjust_power = -1;
		}
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

		battery_log(BAT_LOG_CRTI,
			    "[current_cmd_write] cmd_current_unlimited=%d, cmd_discharging=%d\n",
			    cmd_current_unlimited, cmd_discharging);
		return count;
	}			/*else { */
	battery_log(BAT_LOG_CRTI,
			"  bad argument, echo [enable] > current_cmd\n");
	/*} */

	return -EINVAL;
}

static int current_cmd_read(struct seq_file *m, void *v)
{
	unsigned int charging_enable = false;

	cmd_discharging = 1;
	charging_enable = false;
	adjust_power = -1;

	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	battery_log(BAT_LOG_CRTI,
			"[current_cmd_write] cmd_discharging=%d\n",
			cmd_discharging);

	return 0;
}

static int proc_utilization_open_cur_stop(struct inode *inode,
		struct file *file)
{
	return single_open(file, current_cmd_read, NULL);
}

static ssize_t discharging_cmd_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[32];
	unsigned int charging_enable = false;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &charging_enable, &adjust_power) == 2) {
		battery_log(BAT_LOG_CRTI,
				"[current_cmd_write] adjust_power = %d\n",
				adjust_power);
		return count;
	}			/*else { */
	battery_log(BAT_LOG_CRTI,
			"  bad argument, echo [enable] > current_cmd\n");
	/*} */

	return -EINVAL;
}

static const struct file_operations discharging_cmd_proc_fops = {
	.open = proc_utilization_open,
	.read = seq_read,
	.write = discharging_cmd_write,
};

static const struct file_operations current_cmd_proc_fops = {
	.open = proc_utilization_open_cur_stop,
	.read = seq_read,
	.write = current_cmd_write,
};

static int mt_batteryNotify_probe(struct platform_device *dev)
{
	int ret_device_file = 0;
	/* struct proc_dir_entry *entry = NULL; */
	struct proc_dir_entry *battery_dir = NULL;

	battery_log(BAT_LOG_CRTI,
			"******** mt_batteryNotify_probe!! ********\n");

	ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_BatteryNotify);
	ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_BN_TestMode);

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		pr_info("[%s]:mkdir /proc/mtk_battery_cmd failed\n", __func__);
	} else {
#if 1
		proc_create("battery_cmd", 0644,
				battery_dir, &battery_cmd_proc_fops);
		battery_log(BAT_LOG_CRTI,
				"proc_create battery_cmd_proc_fops\n");

		proc_create("current_cmd", 0644,
				battery_dir, &current_cmd_proc_fops);
		battery_log(BAT_LOG_CRTI,
				"proc_create current_cmd_proc_fops\n");

		proc_create("discharging_cmd", 0644,
				battery_dir, &discharging_cmd_proc_fops);
		battery_log(BAT_LOG_CRTI,
				"proc_create discharging_cmd_proc_fops\n");


#else
		entry = create_proc_entry("battery_cmd", 0644, battery_dir);
		if (entry) {
			entry->read_proc = battery_cmd_read;
			entry->write_proc = battery_cmd_write;
		}
#endif
	}

	battery_log(BAT_LOG_CRTI, "******** mtk_battery_cmd!! ********\n");

	return 0;

}

#ifdef CONFIG_OF
static const struct of_device_id mt_battery_of_match[] = {
	{.compatible = "mediatek,battery",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_battery_of_match);
#endif

static int battery_pm_suspend(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_resume(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_freeze(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_restore(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_restore_noirq(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

const struct dev_pm_ops battery_pm_ops = {
	.suspend = battery_pm_suspend,
	.resume = battery_pm_resume,
	.freeze = battery_pm_freeze,
	.thaw = battery_pm_restore,
	.restore = battery_pm_restore,
	.restore_noirq = battery_pm_restore_noirq,
};

#if defined(CONFIG_OF) || defined(BATTERY_MODULE_INIT)
struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};
#endif

static struct platform_driver battery_driver = {
	.probe = battery_probe,
	.remove = battery_remove,
	.shutdown = battery_shutdown,
	.driver = {
		   .name = "battery",
		   .pm = &battery_pm_ops,
		   },
};

#ifdef CONFIG_OF
static int battery_dts_probe(struct platform_device *dev)
{
	int ret = 0;

	battery_log(BAT_LOG_CRTI, "******** battery_dts_probe!! ********\n");

	battery_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&battery_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
		"***[battery_dts_probe] Unable to register device (%d)\n", ret);
		return ret;
	}
	return 0;

}

static struct platform_driver battery_dts_driver = {
	.probe = battery_dts_probe,
	.remove = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "battery-dts",
#ifdef CONFIG_OF
		   .of_match_table = mt_battery_of_match,
#endif
		   },
};

/* -------------------------------------------------------- */

static const struct of_device_id mt_bat_notify_of_match[] = {
	{.compatible = "mediatek,bat_notify",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_bat_notify_of_match);
#endif

struct platform_device MT_batteryNotify_device = {
	.name = "mt-battery",
	.id = -1,
};

static struct platform_driver mt_batteryNotify_driver = {
	.probe = mt_batteryNotify_probe,
	.driver = {
		   .name = "mt-battery",
		   },
};

#ifdef CONFIG_OF
static int mt_batteryNotify_dts_probe(struct platform_device *dev)
{
	int ret = 0;
	/* struct proc_dir_entry *entry = NULL; */

	battery_log(BAT_LOG_CRTI,
			"******** mt_batteryNotify_dts_probe!! ********\n");

	MT_batteryNotify_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
		"mt_batteryNotify_dts] Unable to register device (%d)\n", ret);
		return ret;
	}
	return 0;

}


static struct platform_driver mt_batteryNotify_dts_driver = {
	.probe = mt_batteryNotify_dts_probe,
	.driver = {
		   .name = "mt-dts-battery",
#ifdef CONFIG_OF
		   .of_match_table = mt_bat_notify_of_match,
#endif
		   },
};
#endif
/* -------------------------------------------------------- */

static int battery_pm_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:	/* Going to hibernate */
	case PM_RESTORE_PREPARE:	/* Going to restore a saved image */
	case PM_SUSPEND_PREPARE:	/* Going to suspend the system */
		battery_log(BAT_LOG_FULL,
				"[%s] pm_event %lu\n", __func__, pm_event);
		battery_timer_pause();
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:	/* Hibernation finished */
	case PM_POST_SUSPEND:	/* Suspend finished */
	case PM_POST_RESTORE:	/* Restore failed */
		battery_log(BAT_LOG_FULL,
				"[%s] pm_event %lu\n", __func__, pm_event);
		battery_timer_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block battery_pm_notifier_block = {
	.notifier_call = battery_pm_event,
	.priority = 0,
};

static int __init battery_init(void)
{
	int ret;

	pr_info("battery_init\n");

#ifdef CONFIG_OF
	/*  */
#else

#ifdef BATTERY_MODULE_INIT
	ret = platform_device_register(&battery_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
		"****[battery_device] Unable to device register(%d)\n", ret);
		return ret;
	}
#endif
#endif

	ret = platform_driver_register(&battery_driver);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
		"****[battery_driver] Unable to register driver (%d)\n", ret);
		return ret;
	}
	/* battery notofy UI */
#ifdef CONFIG_OF
	/*  */
#else
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
		"****[mt_batteryNotify] Unable to device register(%d)\n", ret);
		return ret;
	}
#endif
	ret = platform_driver_register(&mt_batteryNotify_driver);
	if (ret) {
		battery_log(BAT_LOG_CRTI,
		"****[mt_batteryNotify] Unable to register driver (%d)\n", ret);
		return ret;
	}
#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_dts_driver);
	ret = platform_driver_register(&mt_batteryNotify_dts_driver);
#endif
	ret = register_pm_notifier(&battery_pm_notifier_block);
	if (ret)
		pr_info("[%s] failed to register PM notifier %d\n",
				__func__, ret);

	battery_log(BAT_LOG_CRTI,
			"****[battery_driver] Initialization : DONE !!\n");
	return 0;
}

#ifdef BATTERY_MODULE_INIT
late_initcall(battery_init);
#else
static void __exit battery_exit(void)
{
}
late_initcall(battery_init);

#endif

MODULE_AUTHOR("Oscar Liu");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
