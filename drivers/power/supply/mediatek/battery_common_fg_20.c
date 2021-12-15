/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
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
#include <linux/wakelock.h>
/*#include <linux/scatterlist.h>*/
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <linux/suspend.h>

#include <asm/irq.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <linux/reboot.h>

#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_rtc.h>

#include <mt-plat/upmu_common.h>

#include <mach/mtk_battery_meter.h>
#include <mach/mtk_charging.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/charging.h>

#include "mtk_pep20_intf.h"
#include "mtk_pep_intf.h"

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) ||                                \
	defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
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
#include <mach/mtk_diso.h>
#endif

/* Battery Logging Entry */
int Enable_BATDRV_LOG = BAT_LOG_CRTI;

/* Smart Battery Structure */
PMU_ChargerStruct BMT_status;
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
DISO_ChargerStruct DISO_data;
/* Debug Msg */
static char *DISO_state_s[8] = {
	"IDLE",    "OTG_ONLY",    "USB_ONLY",    "USB_WITH_OTG",
	"DC_ONLY", "DC_WITH_OTG", "DC_WITH_USB", "DC_USB_OTG",
};
#endif


/* Thermal related flags */
/* 0:nothing, */
/* 1:enable batTT&chrTimer */
/* 2:disable batTT&chrTimer, */
/* 3:enable batTT, disable chrTimer */
int g_battery_thermal_throttling_flag = 3;
int battery_cmd_thermal_test_mode;
int battery_cmd_thermal_test_mode_value;
/* 0:default enable check batteryTT, */
/* 1:default disable check batteryTT */
int g_battery_tt_check_flag;

/* Global Variable */
static int battery_charging_control_dummy(CHARGING_CTRL_CMD cmd, void *data)
{
	battery_log(BAT_LOG_CRTI, "%s: no charger is ready\n", __func__);
	return -ENOTSUPP;
}

struct wake_lock battery_suspend_lock, battery_meter_lock;
CHARGING_CONTROL battery_charging_control = battery_charging_control_dummy;
unsigned int g_BatteryNotifyCode;
unsigned int g_BN_TestMode;
kal_bool g_bat_init_flag;
unsigned int g_call_state = CALL_IDLE;
kal_bool g_charging_full_reset_bat_meter = KAL_FALSE;
int g_platform_boot_mode;
struct timespec g_bat_time_before_sleep;
int g_smartbook_update;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
kal_bool g_vcdt_irq_delay_flag;
#endif

#if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT)
unsigned int g_batt_temp_status = TEMP_POS_NORMAL;
#endif

kal_bool battery_suspended = KAL_FALSE;

struct timespec battery_duration_time[DURATION_NUM]; /* sec */
unsigned int wake_up_smooth_time;		     /* sec */
unsigned int battery_tracking_time;

signed int batterypseudo1 = BATTERYPSEUDO1;
signed int batterypseudo100 = BATTERYPSEUDO100;

int Is_In_IPOH;
struct battery_custom_data batt_cust_data;
int pending_wake_up_bat;

int cable_in_uevent;

/* Integrate with NVRAM */

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
#define Get_META_BAT_CAR_TUNE_VALUE _IOW('k', 12, int)
#define Set_META_BAT_CAR_TUNE_VALUE _IOW('k', 13, int)
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

kal_bool g_ADC_Cali = KAL_FALSE;
kal_bool g_ftm_battery_flag = KAL_FALSE;
#if !defined(CONFIG_POWER_EXT)
static int g_wireless_state;
#endif

/* Thread related */
#define BAT_MS_TO_NS(x) (x * 1000 * 1000)
static kal_bool bat_routine_thread_timeout = KAL_FALSE;
static kal_bool bat_update_thread_timeout = KAL_FALSE;
static kal_bool chr_wake_up_bat =
	KAL_FALSE; /* charger in/out to wake up battery thread */
static kal_bool bat_meter_timeout = KAL_FALSE;
static DEFINE_MUTEX(bat_mutex);
static DEFINE_MUTEX(bat_update_mutex);
static DEFINE_MUTEX(charger_type_mutex);
static DECLARE_WAIT_QUEUE_HEAD(bat_routine_wq);
static DECLARE_WAIT_QUEUE_HEAD(bat_update_wq);
static struct hrtimer charger_hv_detect_timer;
static struct task_struct *charger_hv_detect_thread;
static kal_bool charger_hv_detect_flag = KAL_FALSE;
static DECLARE_WAIT_QUEUE_HEAD(charger_hv_detect_waiter);
static struct hrtimer battery_kthread_timer;
kal_bool g_battery_soc_ready = KAL_FALSE;
unsigned char fg_ipoh_reset;

/* FOR ADB CMD */
/* Dual battery */
int g_status_smb = POWER_SUPPLY_STATUS_DISCHARGING;
int g_capacity_smb = 50;
int g_present_smb;
/* ADB charging CMD */
static int cmd_discharging = -1;
static int adjust_power = -1;
static int suspend_discharging = -1;

/* FOR ANDROID BATTERY SERVICE */
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
	POWER_SUPPLY_PROP_ONLINE, POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX, POWER_SUPPLY_PROP_CHARGE_COUNTER};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS, POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT, POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY, POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX, POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	/* Add for Battery Service */
	POWER_SUPPLY_PROP_batt_vol, POWER_SUPPLY_PROP_batt_temp,
	/* Add for EM */
	POWER_SUPPLY_PROP_TemperatureR, POWER_SUPPLY_PROP_TempBattVoltage,
	POWER_SUPPLY_PROP_InstatVolt, POWER_SUPPLY_PROP_BatteryAverageCurrent,
	POWER_SUPPLY_PROP_BatterySenseVoltage, POWER_SUPPLY_PROP_ISenseVoltage,
	POWER_SUPPLY_PROP_ChargerVoltage,
	/* Dual battery */
	POWER_SUPPLY_PROP_status_smb, POWER_SUPPLY_PROP_capacity_smb,
	POWER_SUPPLY_PROP_present_smb,
	/* ADB CMD Discharging */
	POWER_SUPPLY_PROP_adjust_power,
};

struct timespec batteryThreadRunTime;

void mt_battery_update_time(struct timespec *pre_time,
			    BATTERY_TIME_ENUM duration_type)
{
	struct timespec time;

	time.tv_sec = 0;
	time.tv_nsec = 0;
	get_monotonic_boottime(&time);

	battery_duration_time[duration_type] = timespec_sub(time, *pre_time);

	battery_log(
		BAT_LOG_FULL,
		"[Battery] mt_battery_update_duration_time , last_time=%d current_time=%d duration=%d\n",
		(int)pre_time->tv_sec, (int)time.tv_sec,
		(int)battery_duration_time[duration_type].tv_sec);

	pre_time->tv_sec = time.tv_sec;
	pre_time->tv_nsec = time.tv_nsec;
}

unsigned int mt_battery_get_duration_time(BATTERY_TIME_ENUM duration_type)
{
	return battery_duration_time[duration_type].tv_sec;
}

struct timespec
mt_battery_get_duration_time_act(BATTERY_TIME_ENUM duration_type)
{
	return battery_duration_time[duration_type];
}

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
	kal_bool chr_status;

	battery_charging_control(CHARGING_CMD_GET_CHARGER_DET_STATUS,
				 &chr_status);
	return chr_status;
}

#if defined(CONFIG_MTK_POWER_EXT_DETECT)
kal_bool bat_is_ext_power(void)
{
	kal_bool pwr_src = 0;

	battery_charging_control(CHARGING_CMD_GET_POWER_SOURCE, &pwr_src);
	battery_log(BAT_LOG_FULL, "[BAT_IS_EXT_POWER] is_ext_power = %d\n",
		    pwr_src);
	return pwr_src;
}
#endif

/* PMIC PCHR Related APIs */

bool __attribute__((weak)) mt_usb_is_device(void)
{
	return true;
}

int __attribute__((weak)) mtk_chr_reset_aicr_upper_bound(void)
{
	return 0;
}

kal_bool upmu_is_chr_det(void)
{
#if !defined(CONFIG_POWER_EXT)
	unsigned int tmp32;
#endif
	if (!g_bat_init_flag) {
		battery_log(
			BAT_LOG_CRTI,
			"[%s] battery thread not ready, will do after bettery init.\n",
			__func__);
		return KAL_FALSE;
	}
#if defined(CONFIG_POWER_EXT)
	/* return KAL_TRUE; */
	return get_charger_detect_status();
#else
	if (suspend_discharging == 1)
		return KAL_FALSE;

	tmp32 = get_charger_detect_status();

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (bat_is_ext_power() == KAL_TRUE)
		return tmp32;
#endif

	if (tmp32 == 0)
		return KAL_FALSE;

#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (mt_usb_is_device()) {
		battery_log(
			BAT_LOG_FULL,
			"[%s] Charger exist and USB is not host\n".
			__func__);

		return KAL_TRUE;
	}

	battery_log(BAT_LOG_CRTI,
		    "[%s] Charger exist but USB is host\n",
			__func__);

	return KAL_FALSE;

#else
	return KAL_TRUE;
#endif

#endif
}
EXPORT_SYMBOL(upmu_is_chr_det);

void wake_up_bat(void)
{
	battery_log(BAT_LOG_FULL, "[BATTERY] %s. \r\n", __func__);

	chr_wake_up_bat = KAL_TRUE;
	bat_routine_thread_timeout = KAL_TRUE;
	battery_meter_reset_sleep_time();

	if (!Is_In_IPOH)
		wake_up(&bat_routine_wq);
	else
		pending_wake_up_bat = TRUE;
}
EXPORT_SYMBOL(wake_up_bat);

void wake_up_bat3(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] %s 3 \r\n", __func__);

	wake_up(&bat_routine_wq);
}
EXPORT_SYMBOL(wake_up_bat3);

static ssize_t bat_log_write(struct file *filp, const char __user *buff,
			     size_t len, loff_t *data)
{
	char proc_bat_data;

	if ((len <= 0) || copy_from_user(&proc_bat_data, buff, 1)) {
		battery_log(BAT_LOG_FULL, "%s error.\n", __func__);
		return -EFAULT;
	}

	if (proc_bat_data == '1') {
		battery_log(BAT_LOG_CRTI, "enable battery driver log system\n");
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
			    "%s: Couldn't create proc entry\n",
				__func__);
	} else {
		proc_entry->write_proc = bat_log_write;
		battery_log(BAT_LOG_CRTI, "%s loaded.\n",
					__func__);
	}
#endif

	return ret;
}

static int wireless_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	int ret = 0;
	struct wireless_data *data =
		container_of(psy->desc, struct wireless_data, psd);

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
#if defined(CONFIG_POWER_EXT)
		/* #if 0 */
		data->USB_ONLINE = 1;
		val->intval = data->USB_ONLINE;
#else
#if defined(CONFIG_MTK_POWER_EXT_DETECT)
		if (bat_is_ext_power() == KAL_TRUE)
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

void mt_battery_set_init_vol(int init_voltage)
{
	BMT_status.bat_vol = init_voltage;
	battery_main.BAT_batt_vol = init_voltage;
}

#if !defined(CONFIG_POWER_EXT)

/* Create File For EM : ADC_Charger_Voltage */
static ssize_t show_ADC_Charger_Voltage(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	battery_log(BAT_LOG_CRTI, "[EM] %s : %d\n",
		    __func__, BMT_status.charger_vol);
	return sprintf(buf, "%d\n", BMT_status.charger_vol);
}

static ssize_t store_ADC_Charger_Voltage(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Charger_Voltage, 0664, show_ADC_Charger_Voltage,
		   store_ADC_Charger_Voltage);

/* Create File For EM : ADC_Channel_0_Slope */
static ssize_t show_ADC_Channel_0_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 0));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_0_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Slope, 0664, show_ADC_Channel_0_Slope,
		   store_ADC_Channel_0_Slope);


/* Create File For EM : ADC_Channel_1_Slope */
static ssize_t show_ADC_Channel_1_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 1));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_1_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Slope, 0664, show_ADC_Channel_1_Slope,
		   store_ADC_Channel_1_Slope);


/* Create File For EM : ADC_Channel_2_Slope */
static ssize_t show_ADC_Channel_2_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 2));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_2_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Slope, 0664, show_ADC_Channel_2_Slope,
		   store_ADC_Channel_2_Slope);

/* Create File For EM : ADC_Channel_3_Slope */
static ssize_t show_ADC_Channel_3_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 3));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_3_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Slope, 0664, show_ADC_Channel_3_Slope,
		   store_ADC_Channel_3_Slope);


/* Create File For EM : ADC_Channel_4_Slope */
static ssize_t show_ADC_Channel_4_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 4));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_4_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Slope, 0664, show_ADC_Channel_4_Slope,
		   store_ADC_Channel_4_Slope);


/* Create File For EM : ADC_Channel_5_Slope */
static ssize_t show_ADC_Channel_5_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 5));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_5_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Slope, 0664, show_ADC_Channel_5_Slope,
		   store_ADC_Channel_5_Slope);

/* Create File For EM : ADC_Channel_6_Slope */
static ssize_t show_ADC_Channel_6_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 6));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_6_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Slope, 0664, show_ADC_Channel_6_Slope,
		   store_ADC_Channel_6_Slope);

/* Create File For EM : ADC_Channel_7_Slope */
static ssize_t show_ADC_Channel_7_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 7));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_7_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Slope, 0664, show_ADC_Channel_7_Slope,
		   store_ADC_Channel_7_Slope);

/* Create File For EM : ADC_Channel_8_Slope */
static ssize_t show_ADC_Channel_8_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 8));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_8_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Slope, 0664, show_ADC_Channel_8_Slope,
		   store_ADC_Channel_8_Slope);

/* Create File For EM : ADC_Channel_9_Slope */
static ssize_t show_ADC_Channel_9_Slope(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 9));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_9_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Slope(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Slope, 0664, show_ADC_Channel_9_Slope,
		   store_ADC_Channel_9_Slope);

/* Create File For EM : ADC_Channel_10_Slope */
static ssize_t show_ADC_Channel_10_Slope(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 10));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_10_Slope : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Slope(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Slope, 0664, show_ADC_Channel_10_Slope,
		   store_ADC_Channel_10_Slope);

/* Create File For EM : ADC_Channel_11_Slope */
static ssize_t show_ADC_Channel_11_Slope(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 11));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_11_Slope : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Slope(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Slope, 0664, show_ADC_Channel_11_Slope,
		   store_ADC_Channel_11_Slope);

/* Create File For EM : ADC_Channel_12_Slope */
static ssize_t show_ADC_Channel_12_Slope(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 12));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_12_Slope : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Slope(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Slope, 0664, show_ADC_Channel_12_Slope,
		   store_ADC_Channel_12_Slope);

/* Create File For EM : ADC_Channel_13_Slope */
static ssize_t show_ADC_Channel_13_Slope(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 13));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_13_Slope : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Slope(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Slope, 0664, show_ADC_Channel_13_Slope,
		   store_ADC_Channel_13_Slope);

/* Create File For EM : ADC_Channel_0_Offset */
static ssize_t show_ADC_Channel_0_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 0));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_0_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Offset, 0664, show_ADC_Channel_0_Offset,
		   store_ADC_Channel_0_Offset);

/* Create File For EM : ADC_Channel_1_Offset */
static ssize_t show_ADC_Channel_1_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 1));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_1_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Offset, 0664, show_ADC_Channel_1_Offset,
		   store_ADC_Channel_1_Offset);

/* Create File For EM : ADC_Channel_2_Offset */
static ssize_t show_ADC_Channel_2_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 2));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_2_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Offset, 0664, show_ADC_Channel_2_Offset,
		   store_ADC_Channel_2_Offset);

/* Create File For EM : ADC_Channel_3_Offset */
static ssize_t show_ADC_Channel_3_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 3));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_3_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Offset, 0664, show_ADC_Channel_3_Offset,
		   store_ADC_Channel_3_Offset);

/* Create File For EM : ADC_Channel_4_Offset */
static ssize_t show_ADC_Channel_4_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 4));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_4_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Offset, 0664, show_ADC_Channel_4_Offset,
		   store_ADC_Channel_4_Offset);

/* Create File For EM : ADC_Channel_5_Offset */
static ssize_t show_ADC_Channel_5_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 5));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_5_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Offset, 0664, show_ADC_Channel_5_Offset,
		   store_ADC_Channel_5_Offset);

/* Create File For EM : ADC_Channel_6_Offset */
static ssize_t show_ADC_Channel_6_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 6));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_6_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Offset, 0664, show_ADC_Channel_6_Offset,
		   store_ADC_Channel_6_Offset);

/* Create File For EM : ADC_Channel_7_Offset */
static ssize_t show_ADC_Channel_7_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 7));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_7_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Offset, 0664, show_ADC_Channel_7_Offset,
		   store_ADC_Channel_7_Offset);

/* Create File For EM : ADC_Channel_8_Offset */
static ssize_t show_ADC_Channel_8_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 8));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_8_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Offset, 0664, show_ADC_Channel_8_Offset,
		   store_ADC_Channel_8_Offset);

/* Create File For EM : ADC_Channel_9_Offset */
static ssize_t show_ADC_Channel_9_Offset(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 9));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_9_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Offset(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Offset, 0664, show_ADC_Channel_9_Offset,
		   store_ADC_Channel_9_Offset);

/* Create File For EM : ADC_Channel_10_Offset */
static ssize_t show_ADC_Channel_10_Offset(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 10));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_10_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Offset(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Offset, 0664, show_ADC_Channel_10_Offset,
		   store_ADC_Channel_10_Offset);

/* Create File For EM : ADC_Channel_11_Offset */
static ssize_t show_ADC_Channel_11_Offset(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 11));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_11_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Offset(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Offset, 0664, show_ADC_Channel_11_Offset,
		   store_ADC_Channel_11_Offset);

/* Create File For EM : ADC_Channel_12_Offset */
static ssize_t show_ADC_Channel_12_Offset(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 12));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_12_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Offset(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Offset, 0664, show_ADC_Channel_12_Offset,
		   store_ADC_Channel_12_Offset);


/* Create File For EM : ADC_Channel_13_Offset */
static ssize_t show_ADC_Channel_13_Offset(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 13));
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_13_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Offset(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Offset, 0664, show_ADC_Channel_13_Offset,
		   store_ADC_Channel_13_Offset);

/* Create File For EM : ADC_Channel_Is_Calibration */
static ssize_t show_ADC_Channel_Is_Calibration(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	int ret_value = 2;

	ret_value = g_ADC_Cali;
	battery_log(BAT_LOG_CRTI, "[EM] ADC_Channel_Is_Calibration : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_Is_Calibration(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_Is_Calibration, 0664,
		   show_ADC_Channel_Is_Calibration,
		   store_ADC_Channel_Is_Calibration);

/* Create File For EM : Power_On_Voltage */
static ssize_t show_Power_On_Voltage(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	battery_log(BAT_LOG_CRTI, "[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_On_Voltage, 0664, show_Power_On_Voltage,
		   store_Power_On_Voltage);

/* Create File For EM : Power_Off_Voltage */
static ssize_t show_Power_Off_Voltage(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	battery_log(BAT_LOG_CRTI, "[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_Off_Voltage, 0664, show_Power_Off_Voltage,
		   store_Power_Off_Voltage);


/* Create File For EM : Charger_TopOff_Value */
static ssize_t show_Charger_TopOff_Value(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = 4110;
	battery_log(BAT_LOG_CRTI, "[EM] Charger_TopOff_Value : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Charger_TopOff_Value(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_TopOff_Value, 0664, show_Charger_TopOff_Value,
		   store_Charger_TopOff_Value);


/* Create File For EM : FG_Battery_CurrentConsumption */
static ssize_t show_FG_Battery_CurrentConsumption(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	int ret_value = 8888;

	ret_value = battery_meter_get_battery_current();
	battery_log(BAT_LOG_CRTI,
		    "[EM] FG_Battery_CurrentConsumption : %d/10 mA\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t
store_FG_Battery_CurrentConsumption(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_Battery_CurrentConsumption, 0664,
		   show_FG_Battery_CurrentConsumption,
		   store_FG_Battery_CurrentConsumption);

/* Create File For EM : FG_SW_CoulombCounter */
static ssize_t show_FG_SW_CoulombCounter(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	signed int ret_value = 7777;

	ret_value = battery_meter_get_car();
	battery_log(BAT_LOG_CRTI, "[EM] FG_SW_CoulombCounter : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_SW_CoulombCounter(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
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
					struct device_attribute *attr,
					const char *buf, size_t size)
{

	if (kstrtouint(buf, 10, &g_call_state) == 0) {
		battery_log(BAT_LOG_CRTI, "call state = %d\n", g_call_state);
		return size;
	}

	/* hidden else, for sscanf format error */
	{
		battery_log(BAT_LOG_CRTI,
			    "bad argument, echo [enable] > current_cmd\n");
	}

	return 0;
}

static DEVICE_ATTR(Charging_CallState, 0664, show_Charging_CallState,
		   store_Charging_CallState);

static ssize_t show_Charger_Type(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned int chr_ype = CHARGER_UNKNOWN;

	chr_ype = BMT_status.charger_exist ? BMT_status.charger_type
					   : CHARGER_UNKNOWN;

	battery_log(BAT_LOG_CRTI, "charger_type = %d\n", chr_ype);
	return sprintf(buf, "%u\n", chr_ype);
}

static ssize_t store_Charger_Type(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	battery_log(BAT_LOG_CRTI, "[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_Type, 0664, show_Charger_Type, store_Charger_Type);

static ssize_t show_Pump_Express(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int is_ta_detected = 0;

	battery_log(
		BAT_LOG_CRTI,
		"[%s] chr_type:%d UISOC2:%d startsoc:%d stopsoc:%d\n",
		__func__, BMT_status.charger_type, BMT_status.UI_SOC2,
		batt_cust_data.ta_start_battery_soc,
		batt_cust_data.ta_stop_battery_soc);

#if defined(PUMP_EXPRESS_SERIES)
	/* Is PE+20 connect */
	if (mtk_pep20_get_is_connect())
		is_ta_detected = 1;
	battery_log(BAT_LOG_FULL, "%s: pep20_is_connect = %d\n", __func__,
		    mtk_pep20_get_is_connect());

	/* Is PE+ connect */
	if (mtk_pep_get_is_connect())
		is_ta_detected = 1;
	battery_log(BAT_LOG_FULL, "%s: pep_is_connect = %d\n", __func__,
		    mtk_pep_get_is_connect());

#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
	/* Is PE connect */
	if (is_ta_connect == KAL_TRUE)
		is_ta_detected = 1;
#endif

	battery_log(BAT_LOG_CRTI, "%s: detected = %d\n", __func__,
		    is_ta_detected);

	return sprintf(buf, "%u\n", is_ta_detected);
}

static ssize_t store_Pump_Express(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(Pump_Express, 0664, show_Pump_Express, store_Pump_Express);

static void mt_battery_update_EM(struct battery_data *bat_data)
{
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC2;
	bat_data->BAT_TemperatureR = BMT_status.temperatureR;    /* API */
	bat_data->BAT_TempBattVoltage = BMT_status.temperatureV; /* API */
	bat_data->BAT_InstatVolt = BMT_status.bat_vol;		 /* VBAT */
	bat_data->BAT_BatteryAverageCurrent = BMT_status.ICharging;
	bat_data->BAT_BatterySenseVoltage = BMT_status.bat_vol;
	bat_data->BAT_ISenseVoltage = BMT_status.Vsense; /* API */
	bat_data->BAT_ChargerVoltage = BMT_status.charger_vol;
	bat_data->BAT_CURRENT_NOW =
		BMT_status.CURRENT_NOW * 100; /* 0.1mA to uA */
	/* Dual battery */
	bat_data->status_smb = g_status_smb;
	bat_data->capacity_smb = g_capacity_smb;
	bat_data->present_smb = g_present_smb;
	battery_log(BAT_LOG_FULL,
		    "status_smb = %d, capacity_smb = %d, present_smb = %d\n",
		    bat_data->status_smb, bat_data->capacity_smb,
		    bat_data->present_smb);
	if ((BMT_status.UI_SOC2 == 100) &&
	    (BMT_status.charger_exist == KAL_TRUE) &&
	    (BMT_status.bat_charging_state != CHR_ERROR))
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;

#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
	if (bat_data->BAT_CAPACITY <= 0)
		bat_data->BAT_CAPACITY = 1;

	battery_log(
		BAT_LOG_CRTI,
		"BAT_CAPACITY=1, due to define CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION\r\n");
#endif
}

static void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = bat_data->psy;
	static unsigned int shutdown_cnt = 0xBADDCAFE;
	static unsigned int shutdown_cnt_chr = 0xBADDCAFE;
	static unsigned int update_cnt = 6;
	static unsigned int pre_uisoc2;
	static unsigned int pre_chr_state;

	if (shutdown_cnt == 0xBADDCAFE)
		shutdown_cnt = 0;

	if (shutdown_cnt_chr == 0xBADDCAFE)
		shutdown_cnt_chr = 0;

	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	bat_data->BAT_batt_vol = BMT_status.bat_vol;
	bat_data->BAT_batt_temp = BMT_status.temperature * 10;
	bat_data->BAT_PRESENT = BMT_status.bat_exist;

	if ((BMT_status.charger_exist == KAL_TRUE) &&
	    (BMT_status.bat_charging_state != CHR_ERROR)) {
		if (BMT_status.bat_exist) {
			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
		} else {
			/* No Battery, Only Charger */
			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
		}
	} else {
		/* Only Battery */
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	mt_battery_update_EM(bat_data);

	if (cmd_discharging == 1)
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CMD_DISCHARGING;

	if (adjust_power != -1) {
		bat_data->adjust_power = adjust_power;
		battery_log(BAT_LOG_CRTI, "adjust_power=(%d)\n", adjust_power);
	}

#ifdef DLPT_POWER_OFF_EN
#ifndef DISABLE_DLPT_FEATURE
	if (bat_data->BAT_CAPACITY <= DLPT_POWER_OFF_THD) {
		static unsigned char cnt = 0xff;

		if (cnt == 0xff)
			cnt = 0;

		if (dlpt_check_power_off() == 1) {
			bat_data->BAT_CAPACITY = 0;
			BMT_status.UI_SOC2 = 0;
			cnt++;
			battery_log(
				BAT_LOG_CRTI,
				"[DLPT_POWER_OFF_EN] SOC=%d to power off , cnt=%d\n",
				bat_data->BAT_CAPACITY, cnt);

			if (cnt >= 4)
				kernel_restart("DLPT reboot system");

		} else {
			cnt = 0;
		}
	} else {
		battery_log(BAT_LOG_CRTI, "[DLPT_POWER_OFF_EN] disable(%d)\n",
			    bat_data->BAT_CAPACITY);
	}
#endif
#endif

	if (update_cnt == 6) {
		/* Update per 60 seconds */
		power_supply_changed(bat_psy);
		pre_uisoc2 = BMT_status.UI_SOC2;
		pre_chr_state = BMT_status.bat_charging_state;
		update_cnt = 0;
	} else if ((pre_uisoc2 != BMT_status.UI_SOC2) ||
		   (BMT_status.UI_SOC2 == 0)) {
		/* Update when soc change */
		power_supply_changed(bat_psy);
		pre_uisoc2 = BMT_status.UI_SOC2;
		update_cnt = 0;
	} else if ((BMT_status.charger_exist == KAL_TRUE) &&
		   ((pre_chr_state != BMT_status.bat_charging_state) ||
		    (BMT_status.bat_charging_state == CHR_ERROR))) {
		/* Update when changer status change */
		power_supply_changed(bat_psy);
		pre_chr_state = BMT_status.bat_charging_state;
		update_cnt = 0;
	} else if (cable_in_uevent == 1) {
		/*To prevent interrupt-trigger update from being filtered*/
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
	battery_log(BAT_LOG_CRTI, "[%s] no support\n",
				__func__);
#else
	g_wireless_state = wireless_state;
	battery_log(BAT_LOG_CRTI,
			"[%s] get wireless_state=%d\n",
			__func__, wireless_state);

	wake_up_bat();
#endif
}

static void wireless_update(struct wireless_data *wireless_data)
{
	static int wireless_status = -1;
	struct power_supply *wireless_psy = wireless_data->psy;
	struct power_supply_desc *wireless_psd = &wireless_data->psd;

	if (BMT_status.charger_exist == KAL_TRUE || g_wireless_state) {
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

	if (BMT_status.charger_exist == KAL_TRUE) {
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

	if (BMT_status.charger_exist == KAL_TRUE) {
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

unsigned char bat_is_kpoc(void)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		return KAL_TRUE;
	}
#endif
	return KAL_FALSE;
}

/* Battery Temprature Parameters and functions */
kal_bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det() == KAL_TRUE)
		return KAL_TRUE;

	battery_log(BAT_LOG_CRTI, "[%s] No charger\r\n", __func__);
	return KAL_FALSE;
}


/* Pulse Charging Algorithm */
kal_bool bat_is_charger_exist(void)
{
	return get_charger_detect_status();
}

kal_bool bat_is_charging_full(void)
{
	if ((BMT_status.bat_full == KAL_TRUE) &&
	    (BMT_status.bat_in_recharging_state == KAL_FALSE))
		return KAL_TRUE;
	else
		return KAL_FALSE;
}

unsigned int bat_get_ui_percentage(void)
{
	if ((g_platform_boot_mode == META_BOOT) ||
	    (g_platform_boot_mode == ADVMETA_BOOT) ||
	    (g_platform_boot_mode == FACTORY_BOOT) ||
	    (g_platform_boot_mode == ATE_FACTORY_BOOT))
		return 75;

	return BMT_status.UI_SOC2;
}

/* Full state --> recharge voltage --> full state */
unsigned int bat_is_recharging_phase(void)
{
	return (BMT_status.bat_in_recharging_state ||
		BMT_status.bat_full == KAL_TRUE);
}

int get_bat_charging_current_level(void)
{
	CHR_CURRENT_ENUM charging_current;

	battery_charging_control(CHARGING_CMD_GET_CURRENT, &charging_current);

	return charging_current;
}

#if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT)
PMU_STATUS do_batt_temp_state_machine(void)
{
	if (BMT_status.temperature == batt_cust_data.err_charge_temperature)
		return PMU_STATUS_FAIL;

	if (batt_cust_data.bat_low_temp_protect_enable) {
		if (BMT_status.temperature <
		    batt_cust_data.min_charge_temperature) {
			battery_log(
				BAT_LOG_CRTI,
				"[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
			g_batt_temp_status = TEMP_POS_LOW;
			return PMU_STATUS_FAIL;
		} else if (g_batt_temp_status == TEMP_POS_LOW) {
			if (BMT_status.temperature >=
			    batt_cust_data
				    .min_charge_temperature_plus_x_degree) {
				battery_log(
				BAT_LOG_CRTI,
				"[BATTERY] Battery Temperature raise from %d to %d(%d), allow charging!!\n\r",
				batt_cust_data.min_charge_temperature,
				BMT_status.temperature,
				batt_cust_data
				.min_charge_temperature_plus_x_degree);
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
			battery_log(
				BAT_LOG_CRTI,
				"[BATTERY] Battery Temperature down from %d to %d(%d), allow charging!!\n\r",
				batt_cust_data.max_charge_temperature,
				BMT_status.temperature,
				batt_cust_data
					.max_charge_temperature_minus_x_degree);
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
#endif

unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
	unsigned long ret_val = 0;

#if defined(CONFIG_POWER_EXT)
	ret_val = 4000;
#else
	ret_val = battery_meter_get_battery_voltage(KAL_FALSE);
#endif

	return ret_val;
}

static void mt_battery_average_method_init(BATTERY_AVG_ENUM type,
					   unsigned int *bufferdata,
					   unsigned int data, signed int *sum)
{
	unsigned int i;
	static kal_bool batteryBufferFirst = KAL_TRUE;
	static kal_bool previous_charger_exist = KAL_FALSE;
	static kal_bool previous_in_recharge_state = KAL_FALSE;
	static unsigned char index;

	/* reset charging current window while plug in/out { */
	if (type == BATTERY_AVG_CURRENT) {
		if (BMT_status.charger_exist == KAL_TRUE) {
			if (previous_charger_exist == KAL_FALSE) {
				batteryBufferFirst = KAL_TRUE;
				previous_charger_exist = KAL_TRUE;
				if ((BMT_status.charger_type ==
				     STANDARD_CHARGER)
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
				    && (DISO_data.diso_state.cur_vdc_state ==
					DISO_ONLINE)
#endif
					    )
					data = batt_cust_data
						.ac_charger_current /
					    100;
				else if (BMT_status.charger_type ==
					 CHARGING_HOST)
					data = batt_cust_data
						.charging_host_charger_current /
					    100;
				else if (BMT_status.charger_type ==
					 NONSTANDARD_CHARGER)
					data = batt_cust_data
						.non_std_ac_charger_current /
					    100; /* mA */
				else		    /* USB */
					data = batt_cust_data
						.usb_charger_current /
					    100; /* mA */
			} else if ((previous_in_recharge_state == KAL_FALSE) &&
				   (BMT_status.bat_in_recharging_state ==
				    KAL_TRUE)) {
				batteryBufferFirst = KAL_TRUE;

				if ((BMT_status.charger_type ==
				     STANDARD_CHARGER)
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
				    && (DISO_data.diso_state.cur_vdc_state ==
					DISO_ONLINE)
#endif
					    )
					data = batt_cust_data
						.ac_charger_current /
					    100;
				else if (BMT_status.charger_type ==
					 CHARGING_HOST)
					data = batt_cust_data
						.charging_host_charger_current /
					    100;
				else if (BMT_status.charger_type ==
					 NONSTANDARD_CHARGER)
					data = batt_cust_data
						.non_std_ac_charger_current /
					    100; /* mA */
				else		    /* USB */
					data = batt_cust_data
						.usb_charger_current /
					    100; /* mA */
			}

			previous_in_recharge_state =
				BMT_status.bat_in_recharging_state;
		} else {
			if (previous_charger_exist == KAL_TRUE) {
				batteryBufferFirst = KAL_TRUE;
				previous_charger_exist = KAL_FALSE;
				data = 0;
			}
		}
	}
	/* reset charging current window while plug in/out } */

	battery_log(BAT_LOG_FULL, "batteryBufferFirst =%d, data= (%d)\n",
		    batteryBufferFirst, data);

	if (batteryBufferFirst == KAL_TRUE) {
		for (i = 0; i < BATTERY_AVERAGE_SIZE; i++)
			bufferdata[i] = data;

		*sum = data * BATTERY_AVERAGE_SIZE;
	}

	index++;
	if (index >= BATTERY_AVERAGE_DATA_NUMBER) {
		index = BATTERY_AVERAGE_DATA_NUMBER;
		batteryBufferFirst = KAL_FALSE;
	}
}

static unsigned int mt_battery_average_method(BATTERY_AVG_ENUM type,
					      unsigned int *bufferdata,
					      unsigned int data,
					      signed int *sum,
					      unsigned char batteryIndex)
{
	unsigned int avgdata;

	mt_battery_average_method_init(type, bufferdata, data, sum);

	*sum -= bufferdata[batteryIndex];
	*sum += data;
	bufferdata[batteryIndex] = data;
	avgdata = (*sum) / BATTERY_AVERAGE_SIZE;

	battery_log(BAT_LOG_FULL, "bufferdata[%d]= (%d)\n", batteryIndex,
		    bufferdata[batteryIndex]);
	return avgdata;
}

void mt_battery_GetBatteryData(void)
{
	unsigned int bat_vol, charger_vol, Vsense, ZCV;
	signed int ICharging, temperature, temperatureR, temperatureV;
	static signed int bat_sum, icharging_sum, temperature_sum;
	static signed int batteryVoltageBuffer[BATTERY_AVERAGE_SIZE];
	static signed int batteryCurrentBuffer[BATTERY_AVERAGE_SIZE];
	static signed int batteryTempBuffer[BATTERY_AVERAGE_SIZE];
	static unsigned char batteryIndex = 0xff;
	static signed int previous_SOC = -1;
	kal_bool current_sign;

	if (batteryIndex == 0xff)
		batteryIndex = 0;

	bat_vol = battery_meter_get_battery_voltage(KAL_TRUE);
	Vsense = battery_meter_get_VSense();
	if (upmu_is_chr_det() == KAL_TRUE) {
		ICharging = battery_meter_get_charging_current();
		charger_vol = battery_meter_get_charger_voltage();
	} else {
		ICharging = 0;
		charger_vol = 0;
	}
	temperature = battery_meter_get_battery_temperature();
	temperatureV = battery_meter_get_tempV();
	temperatureR = battery_meter_get_tempR(temperatureV);
	ZCV = battery_meter_get_battery_zcv();

	BMT_status.ICharging = mt_battery_average_method(
		BATTERY_AVG_CURRENT, &batteryCurrentBuffer[0], ICharging,
		&icharging_sum, batteryIndex);

	if (previous_SOC == -1 &&
	    bat_vol <= batt_cust_data.v_0percent_tracking) {
		previous_SOC = 0;
		if (ZCV != 0) {
			battery_log(
				BAT_LOG_CRTI,
				"battery voltage too low, use ZCV to init average data.\n");
			BMT_status.bat_vol = mt_battery_average_method(
				BATTERY_AVG_VOLT, &batteryVoltageBuffer[0], ZCV,
				&bat_sum, batteryIndex);
		} else {
			battery_log(
				BAT_LOG_CRTI,
				"battery voltage too low, use V_0PERCENT_TRACKING + 100 to init average data.\n");
			BMT_status.bat_vol = mt_battery_average_method(
				BATTERY_AVG_VOLT, &batteryVoltageBuffer[0],
				batt_cust_data.v_0percent_tracking + 100,
				&bat_sum, batteryIndex);
		}
	} else {
		BMT_status.bat_vol = mt_battery_average_method(
			BATTERY_AVG_VOLT, &batteryVoltageBuffer[0], bat_vol,
			&bat_sum, batteryIndex);
	}

	BMT_status.temperature = mt_battery_average_method(
		BATTERY_AVG_TEMP, &batteryTempBuffer[0], temperature,
		&temperature_sum, batteryIndex);
	BMT_status.Vsense = Vsense;
	BMT_status.charger_vol = charger_vol;
	BMT_status.temperatureV = temperatureV;
	BMT_status.temperatureR = temperatureR;
	BMT_status.ZCV = ZCV;
	BMT_status.IBattery = battery_meter_get_battery_current();
	BMT_status.CURRENT_NOW = BMT_status.IBattery;
	current_sign = battery_meter_get_battery_current_sign();
	BMT_status.IBattery *= (current_sign ? 1 : (-1));

	batteryIndex++;
	if (batteryIndex >= BATTERY_AVERAGE_SIZE)
		batteryIndex = 0;

	battery_log(
		BAT_LOG_CRTI,
		"[kernel]AvgVbat %d,bat_vol %d, AvgI %d, I %d, VChr %d, AvgT %d, T %d, ZCV %d, CHR_Type %d, SOC %3d:%3d:%3d, bcct %d:%d, Ichg %d, IBat %d\n",
		BMT_status.bat_vol, bat_vol, BMT_status.ICharging, ICharging,
		BMT_status.charger_vol, BMT_status.temperature, temperature,
		BMT_status.ZCV, BMT_status.charger_type, BMT_status.SOC,
		BMT_status.UI_SOC, BMT_status.UI_SOC2, g_bcct_flag,
		get_usb_current_unlimited(),
		get_bat_charging_current_level() / 100,
		BMT_status.IBattery / 10);
}

static PMU_STATUS mt_battery_CheckBatteryTemp(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

	battery_log(BAT_LOG_CRTI, "[BATTERY] support JEITA, temperature=%d\n",
		    BMT_status.temperature);

	if (do_jeita_state_machine() == PMU_STATUS_FAIL) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] JEITA : fail\n");
		status = PMU_STATUS_FAIL;
	}
#else

#if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT)
	if (do_batt_temp_state_machine() == PMU_STATUS_FAIL) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] Batt temp check : fail\n");
		status = PMU_STATUS_FAIL;
	}
#else
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
	if ((BMT_status.temperature < batt_cust_data.min_charge_temperature) ||
	    (BMT_status.temperature == batt_cust_data.err_charge_temperature)) {
		battery_log(
			BAT_LOG_CRTI,
			"[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
		status = PMU_STATUS_FAIL;
	}
#endif
	if (BMT_status.temperature >= batt_cust_data.max_charge_temperature) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] Battery Over Temperature !!\n\r");
		status = PMU_STATUS_FAIL;
	}
#endif

#endif

	return status;
}

static PMU_STATUS mt_battery_CheckChargerVoltage(void)
{
	PMU_STATUS status = PMU_STATUS_OK;
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	unsigned int v_charger_max = DISO_data.hv_voltage;
#endif

	if (BMT_status.charger_exist == KAL_TRUE) {
#if (V_CHARGER_ENABLE == 1)
		if (BMT_status.charger_vol <= batt_cust_data.v_charger_min) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY]Charger under voltage!!\r\n");
			BMT_status.bat_charging_state = CHR_ERROR;
			status = PMU_STATUS_FAIL;
		}
#endif
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

static PMU_STATUS mt_battery_CheckChargingTime(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

	if ((g_battery_thermal_throttling_flag == 2) ||
	    (g_battery_thermal_throttling_flag == 3)) {
		battery_log(
			BAT_LOG_FULL,
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
static PMU_STATUS mt_battery_CheckCallState(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

	if ((g_call_state == CALL_ACTIVE) &&
	    (BMT_status.bat_vol > V_CC2TOPOFF_THRES))
		status = PMU_STATUS_FAIL;

	return status;
}
#endif

static void mt_battery_CheckBatteryStatus(void)
{
	battery_log(BAT_LOG_FULL,
		    "[%s] cmd_discharging=(%d)\n",
		    __func__,
			cmd_discharging);
	if (cmd_discharging == 1) {
		battery_log(
			BAT_LOG_CRTI,
			"[%s] cmd_discharging=(%d)\n",
			__func__,
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
		battery_log(
			BAT_LOG_FULL,
			"[TestMode] Disable Safety Timer : no UI display\n");
	} else {
		if (BMT_status.total_charging_time >= MAX_CHARGING_TIME)
		/* if (BMT_status.total_charging_time >= 60) //test */
		{
			g_BatteryNotifyCode |= 0x0010;
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Charging Over Time\n");
		} else {
			g_BatteryNotifyCode &= ~(0x0010);
		}
	}

	battery_log(
		BAT_LOG_CRTI,
		"[BATTERY] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME (%x)\n",
		g_BatteryNotifyCode);
#endif
}

static void mt_battery_notify_VBat_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0004_VBAT)
	if (BMT_status.bat_vol > 4350)
	/* if (BMT_status.bat_vol > 3800) //test */
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
	else if (BMT_status.temperature <
		 batt_cust_data.min_charge_temperature) {
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
			    BMT_status.charger_vol,
			    batt_cust_data.v_charger_max);
	} else {
		g_BatteryNotifyCode &= ~(0x0001);
	}
	if (g_BatteryNotifyCode != 0x0000)
		battery_log(
			BAT_LOG_CRTI,
			"[BATTERY] BATTERY_NOTIFY_CASE_0001_VCHARGER (%x)\n",
			g_BatteryNotifyCode);
#endif
}

static void mt_battery_notify_UI_test(void)
{
	if (g_BN_TestMode == 0x0001) {
		g_BatteryNotifyCode = 0x0001;
		battery_log(
			BAT_LOG_CRTI,
			"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0001_VCHARGER\n");
	} else if (g_BN_TestMode == 0x0002) {
		g_BatteryNotifyCode = 0x0002;
		battery_log(
			BAT_LOG_CRTI,
			"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0002_VBATTEMP\n");
	} else if (g_BN_TestMode == 0x0003) {
		g_BatteryNotifyCode = 0x0004;
		battery_log(
			BAT_LOG_CRTI,
			"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0003_ICHARGING\n");
	} else if (g_BN_TestMode == 0x0004) {
		g_BatteryNotifyCode = 0x0008;
		battery_log(
			BAT_LOG_CRTI,
			"[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0004_VBAT\n");
	} else if (g_BN_TestMode == 0x0005) {
		g_BatteryNotifyCode = 0x0010;
		battery_log(
			BAT_LOG_CRTI,
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

	if (g_BN_TestMode == 0x0000) { /* for normal case */
		battery_log(BAT_LOG_FULL,
			    "[BATTERY] %s\n", __func__);

		mt_battery_notify_VCharger_check();

		mt_battery_notify_VBatTemp_check();

		mt_battery_notify_ICharging_check();

		mt_battery_notify_VBat_check();

		mt_battery_notify_TotalChargingTime_check();
	} else { /* for UI test */

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
			battery_log(
				BAT_LOG_FULL,
				"[Battery] In thermal_test_mode , Tbat=%d\n",
				BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
/* ignore default rule */
#else
		if (BMT_status.temperature >= 60) {
#if defined(CONFIG_POWER_EXT)
			battery_log(
				BAT_LOG_CRTI,
				"[BATTERY] CONFIG_POWER_EXT, no update battery update power down.\n");
#else
			{
				if ((g_platform_boot_mode == META_BOOT) ||
				    (g_platform_boot_mode == ADVMETA_BOOT) ||
				    (g_platform_boot_mode ==
				     ATE_FACTORY_BOOT)) {
					battery_log(
						BAT_LOG_FULL,
						"[BATTERY] boot mode = %d, bypass temperature check\n",
						g_platform_boot_mode);
				} else {
					struct battery_data *bat_data =
						&battery_main;
					struct power_supply *bat_psy =
						bat_data->psy;

					battery_log(
						BAT_LOG_CRTI,
						"[Battery] Tbat(%d)>=60, system need power down.\n",
						BMT_status.temperature);

					bat_data->BAT_CAPACITY = 0;

					power_supply_changed(bat_psy);

					if (BMT_status.charger_exist ==
					    KAL_TRUE) {
						/* can not power down due to */
						/* charger exist, */
						/*so need reset system */
						battery_charging_control(
						CHARGING_CMD_SET_PLATFORM_RESET,
						NULL);
					}
					/* avoid SW no feedback */
					battery_charging_control(
						CHARGING_CMD_SET_POWER_OFF,
						NULL);
					/* mt_power_off(); */
				}
			}
#endif
		}
#endif
	}
}

void mt_battery_update_status(void)
{
#if defined(CONFIG_POWER_EXT)
	battery_log(BAT_LOG_CRTI,
		    "[BATTERY] CONFIG_POWER_EXT, no update Android.\n");
#else
	if (g_battery_soc_ready) {
		wireless_update(&wireless_main);
		battery_update(&battery_main);
		ac_update(&ac_main);
		usb_update(&usb_main);
	} else {
		battery_log(BAT_LOG_CRTI,
			    "User space SOC init still waiting\n");
		return;
	}

#endif
}

charger_type mt_charger_type_detection(void)
{
	charger_type CHR_Type_num = CHARGER_UNKNOWN;

	mutex_lock(&charger_type_mutex);

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
	battery_charging_control(CHARGING_CMD_GET_CHARGER_TYPE, &CHR_Type_num);
	BMT_status.charger_type = CHR_Type_num;
#else
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (BMT_status.charger_type == CHARGER_UNKNOWN) {
#else
	if ((BMT_status.charger_type == CHARGER_UNKNOWN) &&
	    (DISO_data.diso_state.cur_vusb_state == DISO_ONLINE)) {
#endif
		battery_charging_control(CHARGING_CMD_GET_CHARGER_TYPE,
					 &CHR_Type_num);
		BMT_status.charger_type = CHR_Type_num;

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#if defined(PUMP_EXPRESS_SERIES)

		if (g_battery_soc_ready == KAL_FALSE) {
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

void mt_charger_enable_DP_voltage(int ison)
{
	mutex_lock(&charger_type_mutex);
	battery_charging_control(CHARGING_CMD_SET_DP, &ison);
	mutex_unlock(&charger_type_mutex);
}

charger_type mt_get_charger_type(void)
{
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_FPGA_EARLY_PORTING)
	return STANDARD_HOST;
#else
	return BMT_status.charger_type;
#endif
}

static void mt_battery_charger_detect_check(void)
{
#ifdef CONFIG_MTK_SWCHR_SUPPORT
	unsigned int pwr;
#endif
	if (upmu_is_chr_det() == KAL_TRUE) {
		wake_lock(&battery_suspend_lock);

		BMT_status.charger_vol = battery_meter_get_charger_voltage();

#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		BMT_status.charger_exist = KAL_TRUE;
#endif

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
		mt_charger_type_detection();

		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST)) {
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

			if ((BMT_status.charger_type == STANDARD_HOST) ||
			    (BMT_status.charger_type == CHARGING_HOST)) {
				mt_usb_connect();
			}
		}
#endif

#ifdef CONFIG_MTK_SWCHR_SUPPORT
		/* Set CHRIND's clock to power up */
		pwr = 0;
		battery_charging_control(CHARGING_CMD_SET_CHRIND_CK_PDN, &pwr);
#endif

		battery_log(BAT_LOG_FULL,
			    "[BAT_thread]Cable in, CHR_Type_num=%d\r\n",
			    BMT_status.charger_type);

	} else {
		wake_unlock(&battery_suspend_lock);

		BMT_status.charger_exist = KAL_FALSE;
		BMT_status.charger_type = CHARGER_UNKNOWN;
		BMT_status.bat_full = KAL_FALSE;
		BMT_status.bat_in_recharging_state = KAL_FALSE;
		BMT_status.bat_charging_state = CHR_PRE;
		BMT_status.total_charging_time = 0;
		BMT_status.PRE_charging_time = 0;
		BMT_status.CC_charging_time = 0;
		BMT_status.TOPOFF_charging_time = 0;
		BMT_status.POSTFULL_charging_time = 0;

		BMT_status.charger_vol = 0;

		battery_log(BAT_LOG_FULL, "[BAT_thread]Cable out \r\n");

		mt_usb_disconnect();

		battery_log(BAT_LOG_FULL, "[PE+] Cable OUT\n");

#ifdef CONFIG_MTK_SWCHR_SUPPORT
		/* Set CHRIND's clock to power down */
		pwr = 1;
		battery_charging_control(CHARGING_CMD_SET_CHRIND_CK_PDN, &pwr);
#endif
	}
}

static void mt_kpoc_power_off_check(void)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {

		battery_log(
			BAT_LOG_CRTI,
			"[%s] chr_vol=%d, boot_mode=%d\r\n",
			__func__,
			BMT_status.charger_vol, g_platform_boot_mode);

		if ((upmu_is_chr_det() == KAL_FALSE) &&
		    (BMT_status.charger_vol < 2500)) { /* vbus < 2.5V */
			battery_log(
				BAT_LOG_CRTI,
				"[%s] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n",
				__func__);
			battery_charging_control(CHARGING_CMD_SET_POWER_OFF,
						 NULL);
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
	battery_log(
		BAT_LOG_CRTI,
		"[update_battery_smb_info] get status_smb=%d,capacity_smb=%d,present_smb=%d\n",
		status_smb, capacity_smb, present_smb);

	wake_up_bat();
	g_smartbook_update = 1;
#endif
}

void do_chrdet_int_task(void)
{
	u32 plug_out_aicr = 50000; /* 10uA */

	if (g_bat_init_flag == KAL_TRUE) {
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (upmu_is_chr_det() == KAL_TRUE) {
#else
		battery_charging_control(CHARGING_CMD_GET_DISO_STATE,
					 &DISO_data);
		if ((DISO_data.diso_state.cur_vusb_state == DISO_ONLINE) ||
		    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif
			battery_log(BAT_LOG_CRTI,
				    "[%s] charger exist!\n", __func__);
			BMT_status.charger_exist = KAL_TRUE;

			wake_lock(&battery_suspend_lock);

#if defined(CONFIG_POWER_EXT)
			mt_usb_connect();
			battery_log(
				BAT_LOG_CRTI,
				"[%s] call mt_usb_connect() in EVB\n",
				__func__);
#elif defined(CONFIG_MTK_POWER_EXT_DETECT)
			if (bat_is_ext_power() == KAL_TRUE) {
				mt_usb_connect();
				battery_log(
					BAT_LOG_CRTI,
					"[%s] call mt_usb_connect() in EVB\n",
					__func__);
				return;
			}
#endif
		} else {
			battery_log(
				BAT_LOG_CRTI,
				"[%s] charger NOT exist!\n",
				__func__);
			BMT_status.charger_exist = KAL_FALSE;

			/* Reset AICR's upper bound calculated by AICL */
			mtk_chr_reset_aicr_upper_bound();

			/* Set AICR to 500mA if it is plugged out */
			battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
						 &plug_out_aicr);

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
			battery_log(
				BAT_LOG_CRTI,
				"turn off charging for no available charging source\n");
			battery_charging_control(CHARGING_CMD_ENABLE,
						 &BMT_status.charger_exist);
#endif

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode ==
				    KERNEL_POWER_OFF_CHARGING_BOOT ||
			    g_platform_boot_mode ==
				    LOW_POWER_OFF_CHARGING_BOOT) {
				battery_log(
					BAT_LOG_CRTI,
					"[pmic_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
				battery_charging_control(
					CHARGING_CMD_SET_POWER_OFF, NULL);
				/* mt_power_off(); */
			}
#endif

			wake_unlock(&battery_suspend_lock);

#if defined(CONFIG_POWER_EXT)
			mt_usb_disconnect();
			battery_log(
				BAT_LOG_CRTI,
				"[%s] call mt_usb_disconnect() in EVB\n",
				__func__);
#elif defined(CONFIG_MTK_POWER_EXT_DETECT)
			if (bat_is_ext_power() == KAL_TRUE) {
				mt_usb_disconnect();
				battery_log(
					BAT_LOG_CRTI,
					"[%s] call mt_usb_disconnect() in EVB\n",
					__func__);
				return;
			}
#endif

			mtk_pep20_set_is_cable_out_occur(true);
			mtk_pep_set_is_cable_out_occur(true);

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
			is_ta_connect = KAL_FALSE;
			ta_check_chr_type = KAL_TRUE;
			ta_cable_out_occur = KAL_TRUE;
#endif
		}

		cable_in_uevent = 1;

		/* reset_parameter_dod_charger_plug_event(); */
		wakeup_fg_algo(FG_CHARGER);
		/* Place charger detection and battery update here is used to */
		/* speed up charging icon display. */

		mt_battery_charger_detect_check();
		if (BMT_status.UI_SOC2 == 100 &&
		    BMT_status.charger_exist == KAL_TRUE &&
		    BMT_status.bat_charging_state != CHR_ERROR) {
			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = KAL_TRUE;
			g_charging_full_reset_bat_meter = KAL_TRUE;
		}

		if (g_battery_soc_ready == KAL_FALSE) {
			if (BMT_status.nPercent_ZCV == 0)
				battery_meter_initial();

			BMT_status.SOC = battery_meter_get_battery_percentage();
		}

		if (BMT_status.bat_vol > 0)
			mt_battery_update_status();

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		DISO_data.chr_get_diso_state = KAL_TRUE;
#endif

		wake_up_bat();
	} else {
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		g_vcdt_irq_delay_flag = KAL_TRUE;
#endif
		battery_log(
			BAT_LOG_CRTI,
			"[%s] battery thread not ready, will do after bettery init.\n",
			__func__);
	}
}

void BAT_thread(void)
{
	static kal_bool battery_meter_initilized = KAL_FALSE;

	if (battery_meter_initilized == KAL_FALSE) {
		battery_meter_initial();
		BMT_status.nPercent_ZCV =
			battery_meter_get_battery_nPercent_zcv();
		battery_meter_initilized = KAL_TRUE;
	}

	mt_battery_update_time(&batteryThreadRunTime, BATTERY_THREAD_TIME);

	if (fg_ipoh_reset) {
		battery_log(BAT_LOG_CRTI,
			    "[FG %s]FG_MAIN because IPOH  .\n",
				__func__);
		battery_meter_set_init_flag(false);
		fgauge_algo_run_get_init_data();
		wakeup_fg_algo((FG_MAIN));
		fg_ipoh_reset = 0;
		bat_spm_timeout = FALSE;
	} else if (bat_spm_timeout) {
		wakeup_fg_algo((FG_MAIN + FG_RESUME));
		bat_spm_timeout = FALSE;
	} else {
		wakeup_fg_algo(FG_MAIN);
	}
	mt_battery_charger_detect_check();
	mt_battery_GetBatteryData();
	if (BMT_status.charger_exist == KAL_TRUE)
		check_battery_exist();

	mt_battery_thermal_check();
	mt_battery_notify_check();

	if (BMT_status.charger_exist == KAL_TRUE) {
		mt_battery_CheckBatteryStatus();
		mt_battery_charging_algorithm();
	}

	mt_kpoc_power_off_check();
}

/* Internal API */
int bat_routine_thread(void *x)
{
	ktime_t ktime = ktime_set(3, 0); /* 10s, 10* 1000 ms */

/* Run on a process content */
#if defined(BATTERY_SW_INIT)
	battery_charging_control(CHARGING_CMD_SW_INIT, NULL);
#endif

	while (1) {
		wake_lock(&battery_meter_lock);
		mutex_lock(&bat_mutex);

		if (((chargin_hw_init_done == KAL_TRUE) &&
		     (battery_suspended == KAL_FALSE)) ||
		    ((chargin_hw_init_done == KAL_TRUE) &&
		     (chr_wake_up_bat == KAL_TRUE)))
			BAT_thread();

		if (chr_wake_up_bat == KAL_TRUE)
			chr_wake_up_bat = KAL_FALSE;

		mutex_unlock(&bat_mutex);
		wake_unlock(&battery_meter_lock);

		battery_log(BAT_LOG_FULL, "wait event 1\n");

		wait_event(bat_routine_wq,
			   (bat_routine_thread_timeout == KAL_TRUE));

		bat_routine_thread_timeout = KAL_FALSE;
		hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);
		ktime = ktime_set(BAT_TASK_PERIOD, 0); /* 10s, 10* 1000 ms */
		if (chr_wake_up_bat == KAL_TRUE && g_smartbook_update != 1) {
/* for charger plug in/ out */
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
			if (DISO_data.chr_get_diso_state) {
				DISO_data.chr_get_diso_state = KAL_FALSE;
				battery_charging_control(
					CHARGING_CMD_GET_DISO_STATE,
					&DISO_data);
			}
#endif

			g_smartbook_update = 0;
			/* battery_meter_reset(); */
			/* chr_wake_up_bat = KAL_FALSE; */
		}
	}

	return 0;
}

void bat_thread_wakeup(void)
{
	battery_log(BAT_LOG_FULL,
		    "******** battery : %s  ********\n",
			__func__);

	bat_routine_thread_timeout = KAL_TRUE;
	bat_meter_timeout = KAL_TRUE;
	battery_meter_reset_sleep_time();

	wake_up(&bat_routine_wq);
}

int bat_update_thread(void *x)
{
	/* Run on a process content */
	while (1) {
		mutex_lock(&bat_update_mutex);
#ifdef USING_SMOOTH_UI_SOC2
		battery_meter_smooth_uisoc2();
#endif
		mt_battery_update_status();
		mutex_unlock(&bat_update_mutex);

		battery_log(BAT_LOG_FULL, "wait event 2\n");
		wait_event(bat_update_wq,
			   (bat_update_thread_timeout == KAL_TRUE));

		bat_update_thread_timeout = KAL_FALSE;
	}

	return 0;
}

void bat_update_thread_wakeup(void)
{
	battery_log(BAT_LOG_FULL,
		    "******** battery : %s  ********\n",
			__func__);
	bat_update_thread_timeout = KAL_TRUE;
	wake_up(&bat_update_wq);
}

/* fop API */
static long adc_cali_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
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
		g_ADC_Cali = KAL_FALSE;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		g_ADC_Cali = KAL_FALSE;
		/* enable calibration after setting ADC_CALI_Cal */
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
		g_ADC_Cali = KAL_FALSE;
		/* enable calibration after setting ADC_CALI_Cal */
		for (i = 0; i < 14; i++)
			battery_log(BAT_LOG_CRTI, "adc_cali_offset[%d] = %d\n",
				    i, *(adc_cali_offset + i));
		battery_log(
			BAT_LOG_FULL,
			"**** unlocked_ioctl : SET_ADC_CALI_Offset Done!\n");
		break;

	case SET_ADC_CALI_Cal:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_cal, naram_data_addr, 4);
		g_ADC_Cali = KAL_TRUE;
		if (adc_cali_cal[0] == 1)
			g_ADC_Cali = KAL_TRUE;
		else
			g_ADC_Cali = KAL_FALSE;

		for (i = 0; i < 1; i++)
			battery_log(BAT_LOG_CRTI, "adc_cali_cal[%d] = %d\n", i,
				    *(adc_cali_cal + i));
		battery_log(BAT_LOG_FULL,
			    "**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		/* g_ADC_Cali = KAL_FALSE; */ /* 20100508 Infinity */
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr,
				     8); /* 2*int = 2*4 */

		if (adc_in_data[0] == 0) {
			/* I_SENSE */
			adc_out_data[0] =
				battery_meter_get_VSense() * adc_in_data[1];
		} else if (adc_in_data[0] == 1) {
			/* BAT_SENSE */
			adc_out_data[0] =
				battery_meter_get_battery_voltage(KAL_TRUE) *
				adc_in_data[1];
		} else if (adc_in_data[0] == 3) {
			/* V_Charger */
			adc_out_data[0] = battery_meter_get_charger_voltage() *
					  adc_in_data[1];
			/* adc_out_data[0] = adc_out_data[0] / 100; */
		} else if (adc_in_data[0] == 30) {
			/* V_Bat_temp magic number */
			adc_out_data[0] =
				battery_meter_get_battery_temperature() *
				adc_in_data[1];
		} else if (adc_in_data[0] == 66) {
			adc_out_data[0] =
				(battery_meter_get_battery_current()) / 10;

			if (battery_meter_get_battery_current_sign() ==
			    KAL_TRUE)
				adc_out_data[0] =
					0 - adc_out_data[0]; /* charging */

		} else {
			battery_log(BAT_LOG_FULL, "unknown channel(%d,%d)\n",
				    adc_in_data[0], adc_in_data[1]);
		}

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1; /* failed */
		else
			adc_out_data[1] = 0; /* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0; /* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0; /* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		battery_log(
			BAT_LOG_CRTI,
			"**** unlocked_ioctl : Channel %d * %d times = %d\n",
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

	case Set_Charger_Current: /* For Factory Mode */
		user_data_addr = (int *)arg;
		ret = copy_from_user(charging_level_data, user_data_addr, 4);
		g_ftm_battery_flag = KAL_TRUE;
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
		battery_log(BAT_LOG_CRTI,
			    "**** unlocked_ioctl : set_Charger_Current:%d\n",
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
		adc_out_data[0] = BMT_status.UI_SOC2;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);

		break;

	case Get_META_BAT_CAR_TUNE_VALUE:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = batt_meter_cust_data.car_tune_value;
		battery_log(BAT_LOG_CRTI, "Get_BAT_CAR_TUNE_VALUE, res=%d\n",
			    adc_out_data[0]);
		ret = copy_to_user(user_data_addr, adc_out_data, 8);

		break;

	case Set_META_BAT_CAR_TUNE_VALUE:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		/* Input X mA, div 1k => car_tune_value */
		batt_meter_cust_data.car_tune_value = adc_in_data[1] / 1000;
		adc_out_data[0] = batt_meter_cust_data.car_tune_value;
		battery_log(BAT_LOG_CRTI,
			    "Set_BAT_CAR_TUNE_VALUE[%d], res=%d\n",
			    adc_in_data[1], adc_out_data[0]);
		ret = copy_to_user(user_data_addr, adc_out_data, 8);

		break;
	/* add bing meta tool------------------------------- */

	default:
		g_ADC_Cali = KAL_FALSE;
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
#if defined(CONFIG_DIS_CHECK_BATTERY)
	battery_log(BAT_LOG_CRTI, "[BATTERY] Disable check battery exist.\n");
#else
	unsigned int baton_count = 0;
	unsigned int charging_enable = KAL_FALSE;
	unsigned int battery_status;
	unsigned int i;

	for (i = 0; i < 3; i++) {
		battery_charging_control(CHARGING_CMD_GET_BATTERY_STATUS,
					 &battery_status);
		baton_count += battery_status;
	}

	if (baton_count >= 3) {
		if ((g_platform_boot_mode == META_BOOT) ||
		    (g_platform_boot_mode == ADVMETA_BOOT) ||
		    (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
			battery_log(
				BAT_LOG_FULL,
				"[BATTERY] boot mode = %d, bypass battery check\n",
				g_platform_boot_mode);
		} else {
			battery_log(
				BAT_LOG_CRTI,
				"[BATTERY] Battery is not exist, power off FAN5405 and system (%d)\n",
				baton_count);

			battery_charging_control(CHARGING_CMD_ENABLE,
						 &charging_enable);
#ifdef CONFIG_MTK_POWER_PATH_MANAGEMENT_SUPPORT
			battery_charging_control(
				CHARGING_CMD_SET_PLATFORM_RESET, NULL);
#else
			battery_charging_control(CHARGING_CMD_SET_POWER_OFF,
						 NULL);
#endif
		}
	}
#endif
}

int charger_hv_detect_sw_thread_handler(void *unused)
{
	ktime_t ktime;
	unsigned int charging_enable;
	unsigned int hv_voltage = batt_cust_data.v_charger_max * 1000;
	kal_bool hv_status = KAL_FALSE;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	hv_voltage = DISO_data.hv_voltage;
#endif

	do {
#ifdef CONFIG_MTK_SWCHR_SUPPORT
		/*this annoying SW workaround wakes up bat_thread. */
		/* 10 secs is set instead of 1 sec */
		ktime = ktime_set(BAT_TASK_PERIOD, 0);
#else
		ktime = ktime_set(0, BAT_MS_TO_NS(1000));
#endif

		if (chargin_hw_init_done)
			battery_charging_control(CHARGING_CMD_SET_HV_THRESHOLD,
						 &hv_voltage);

		wait_event_interruptible(charger_hv_detect_waiter,
					 (charger_hv_detect_flag == KAL_TRUE));

		if (upmu_is_chr_det() == KAL_TRUE)
			check_battery_exist();

		charger_hv_detect_flag = KAL_FALSE;

		if (chargin_hw_init_done)
			battery_charging_control(CHARGING_CMD_GET_HV_STATUS,
						 &hv_status);

		if (hv_status == KAL_TRUE) {
			battery_log(
				BAT_LOG_CRTI,
				"[%s] charger hv\n", __func__);

			charging_enable = KAL_FALSE;
			if (chargin_hw_init_done)
				battery_charging_control(CHARGING_CMD_ENABLE,
							 &charging_enable);
		} else {
			battery_log(
				BAT_LOG_FULL,
				"[%s] upmu_chr_get_vcdt_hv_det() != 1\n",
				__func__);
		}

		if (chargin_hw_init_done)
			battery_charging_control(
				CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

		hrtimer_start(&charger_hv_detect_timer, ktime,
			      HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;
}

enum hrtimer_restart charger_hv_detect_sw_workaround(struct hrtimer *timer)
{
	charger_hv_detect_flag = KAL_TRUE;
	wake_up_interruptible(&charger_hv_detect_waiter);

	battery_log(BAT_LOG_FULL, "[%s]\n", __func__);

	return HRTIMER_NORESTART;
}

void charger_hv_detect_sw_workaround_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, BAT_MS_TO_NS(2000));
	hrtimer_init(&charger_hv_detect_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	charger_hv_detect_timer.function = charger_hv_detect_sw_workaround;
	hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);

	charger_hv_detect_thread =
		kthread_run(charger_hv_detect_sw_thread_handler, 0,
			    "mtk charger_hv_detect_sw_workaround");
	if (IS_ERR(charger_hv_detect_thread)) {
		battery_log(
			BAT_LOG_FULL,
			"[%s]: failed to create charger_hv_detect_sw_workaround thread\n",
			__func__);
	}
	battery_log(BAT_LOG_CRTI,
		    "%s : done\n", __func__);
}

enum hrtimer_restart battery_kthread_hrtimer_func(struct hrtimer *timer)
{
	bat_thread_wakeup();

	return HRTIMER_NORESTART;
}

void battery_kthread_hrtimer_init(void)
{
	ktime_t ktime;

#ifdef CONFIG_MTK_SWCHR_SUPPORT
	/*watchdog timer before 40 secs*/
	ktime = ktime_set(BAT_TASK_PERIOD, 0); /* 3s, 10* 1000 ms */
#else
	ktime = ktime_set(1, 0);
#endif
	hrtimer_init(&battery_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	battery_kthread_timer.function = battery_kthread_hrtimer_func;
	hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);

	battery_log(BAT_LOG_CRTI, "%s : done\n", __func__);
}

static void get_charging_control(void)
{
	battery_log(BAT_LOG_CRTI, "%s: starts\n", __func__);
	WARN_ON(battery_charging_control == battery_charging_control_dummy);
	battery_charging_control = chr_control_interface;
}

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
static irqreturn_t diso_auxadc_irq_thread(int irq, void *dev_id)
{
	int pre_diso_state = (DISO_data.diso_state.pre_otg_state |
			      (DISO_data.diso_state.pre_vusb_state << 1) |
			      (DISO_data.diso_state.pre_vdc_state << 2)) &
			     0x7;

	battery_log(
		BAT_LOG_CRTI,
		"[DISO]auxadc IRQ threaded handler triggered, pre_diso_state is %s\n",
		DISO_state_s[pre_diso_state]);

	switch (pre_diso_state) {
#ifdef MTK_DISCRETE_SWITCH /*for DSC DC plugin handle */
	case USB_ONLY:
#endif
	case OTG_ONLY:
		BMT_status.charger_exist = KAL_TRUE;
		wake_lock(&battery_suspend_lock);
		wake_up_bat();
		break;
	case DC_WITH_OTG:
		BMT_status.charger_exist = KAL_FALSE;
		/* need stop charger quickly */
		battery_charging_control(CHARGING_CMD_ENABLE,
					 &BMT_status.charger_exist);
		BMT_status.charger_exist = KAL_FALSE; /* reset charger status */
		BMT_status.charger_type = CHARGER_UNKNOWN;
		wake_unlock(&battery_suspend_lock);
		wake_up_bat();
		break;
	case DC_WITH_USB:
		/* usb delayed work will reflact BMT_staus */
		/* so need update state ASAP */
		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST))
			mt_usb_disconnect(); /* disconnect if connected */
		BMT_status.charger_type = CHARGER_UNKNOWN; /* reset chr_type */
		wake_up_bat();
		break;
	case DC_ONLY:
		BMT_status.charger_type = CHARGER_UNKNOWN;
		mt_battery_charger_detect_check();
		/* plug in VUSB, check if need connect usb */
		break;
	default:
		battery_log(
			BAT_LOG_CRTI,
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
/* mtk_charging.h */
/* stop charging while in talking mode */
#if defined(STOP_CHARGING_IN_TAKLING)
	batt_cust_data.stop_charging_in_takling = 1;
#else  /* #if defined(STOP_CHARGING_IN_TAKLING) */
	batt_cust_data.stop_charging_in_takling = 0;
#endif /* #if defined(STOP_CHARGING_IN_TAKLING) */

#if defined(TALKING_RECHARGE_VOLTAGE)
	batt_cust_data.talking_recharge_voltage = TALKING_RECHARGE_VOLTAGE;
#endif

#if defined(TALKING_SYNC_TIME)
	batt_cust_data.talking_sync_time = TALKING_SYNC_TIME;
#endif

/* Battery Temperature Protection */
#if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT)
	batt_cust_data.mtk_temperature_recharge_support = 1;
#else  /* #if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT) */
	batt_cust_data.mtk_temperature_recharge_support = 0;
#endif /* #if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT) */

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
#else  /* #if defined(CONFIG_USB_IF) */
	batt_cust_data.config_usb_if = 0;
#endif /* #if defined(CONFIG_USB_IF) */

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
	batt_cust_data.usb_charger_current = USB_CHARGER_CURRENT;
#endif
#if defined(AC_CHARGER_INPUT_CURRENT)
	batt_cust_data.ac_charger_input_current = AC_CHARGER_INPUT_CURRENT;
#endif
#if defined(AC_CHARGER_CURRENT)
	batt_cust_data.ac_charger_current = AC_CHARGER_CURRENT;
#endif
#if defined(NON_STD_AC_CHARGER_CURRENT)
	batt_cust_data.non_std_ac_charger_current = NON_STD_AC_CHARGER_CURRENT;
#endif
#if defined(CHARGING_HOST_CHARGER_CURRENT)
	batt_cust_data.charging_host_charger_current =
		CHARGING_HOST_CHARGER_CURRENT;
#endif
#if defined(APPLE_0_5A_CHARGER_CURRENT)
	batt_cust_data.apple_0_5a_charger_current = APPLE_0_5A_CHARGER_CURRENT;
#endif
#if defined(APPLE_1_0A_CHARGER_CURRENT)
	batt_cust_data.apple_1_0a_charger_current = APPLE_1_0A_CHARGER_CURRENT;
#endif
#if defined(APPLE_2_1A_CHARGER_CURRENT)
	batt_cust_data.apple_2_1a_charger_current = APPLE_2_1A_CHARGER_CURRENT;
#endif

/* charger error check */
#if defined(BAT_LOW_TEMP_PROTECT_ENABLE)
	batt_cust_data.bat_low_temp_protect_enable = 1;
#else  /* #if defined(BAT_LOW_TEMP_PROTECT_ENABLE) */
	batt_cust_data.bat_low_temp_protect_enable = 0;
#endif /* #if defined(BAT_LOW_TEMP_PROTECT_ENABLE) */

#if defined(V_CHARGER_ENABLE)
	batt_cust_data.v_charger_enable = V_CHARGER_ENABLE;
#endif
#if defined(V_CHARGER_MAX)
	batt_cust_data.v_charger_max = V_CHARGER_MAX;
#endif
#if defined(V_CHARGER_MIN)
	batt_cust_data.v_charger_min = V_CHARGER_MIN;
#endif

#if defined(V_0PERCENT_TRACKING)
	batt_cust_data.v_0percent_tracking = V_0PERCENT_TRACKING;
#endif

/* High battery support */
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	batt_cust_data.high_battery_voltage_support = 1;
#else  /* #if defined(HIGH_BATTERY_VOLTAGE_SUPPORT) */
	batt_cust_data.high_battery_voltage_support = 0;
#endif /* #if defined(HIGH_BATTERY_VOLTAGE_SUPPORT) */

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
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
		battery_log(BAT_LOG_FULL, "Get %s: %d\n", node_srting,
			    (*cust_val));
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

	__batt_parse_node(
		np, "max_charge_temperature_minus_x_degree",
		&batt_cust_data.max_charge_temperature_minus_x_degree);

	__batt_parse_node(np, "min_charge_temperature",
			  &batt_cust_data.min_charge_temperature);

	__batt_parse_node(np, "min_charge_temperature_plus_x_degree",
			  &batt_cust_data.min_charge_temperature_plus_x_degree);

	__batt_parse_node(np, "err_charge_temperature",
			  &batt_cust_data.err_charge_temperature);

	__batt_parse_node(np, "v_pre2cc_thres", &batt_cust_data.v_pre2cc_thres);

	__batt_parse_node(np, "v_cc2topoff_thres",
			  &batt_cust_data.v_cc2topoff_thres);

	__batt_parse_node(np, "recharging_voltage",
			  &batt_cust_data.recharging_voltage);

	__batt_parse_node(np, "charging_full_current",
			  &batt_cust_data.charging_full_current);

	__batt_parse_node(np, "config_usb_if", &batt_cust_data.config_usb_if);

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

	__batt_parse_node(np, "v_charger_max", &batt_cust_data.v_charger_max);

	__batt_parse_node(np, "v_charger_min", &batt_cust_data.v_charger_min);

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

	__batt_parse_node(
		np, "jeita_temp_pos_10_to_pos_45_cv_voltage",
		&batt_cust_data.jeita_temp_pos_10_to_pos_45_cv_voltage);

	__batt_parse_node(
		np, "jeita_temp_pos_0_to_pos_10_cv_voltage",
		&batt_cust_data.jeita_temp_pos_0_to_pos_10_cv_voltage);

	__batt_parse_node(
		np, "jeita_temp_neg_10_to_pos_0_cv_voltage",
		&batt_cust_data.jeita_temp_neg_10_to_pos_0_cv_voltage);

	__batt_parse_node(np, "jeita_temp_below_neg_10_cv_voltage",
			  &batt_cust_data.jeita_temp_below_neg_10_cv_voltage);

	__batt_parse_node(np, "jeita_neg_10_to_pos_0_full_current",
			  &batt_cust_data.jeita_neg_10_to_pos_0_full_current);

	__batt_parse_node(
		np, "jeita_temp_pos_45_to_pos_60_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_45_to_pos_60_recharge_voltage);

	__batt_parse_node(
		np, "jeita_temp_pos_10_to_pos_45_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_10_to_pos_45_recharge_voltage);

	__batt_parse_node(
		np, "jeita_temp_pos_0_to_pos_10_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_0_to_pos_10_recharge_voltage);

	__batt_parse_node(
		np, "jeita_temp_neg_10_to_pos_0_recharge_voltage",
		&batt_cust_data.jeita_temp_neg_10_to_pos_0_recharge_voltage);

	__batt_parse_node(
		np, "jeita_temp_pos_45_to_pos_60_cc2topoff_threshold",
		&batt_cust_data
			 .jeita_temp_pos_45_to_pos_60_cc2topoff_threshold);

	__batt_parse_node(
		np, "jeita_temp_pos_10_to_pos_45_cc2topoff_threshold",
		&batt_cust_data
			 .jeita_temp_pos_10_to_pos_45_cc2topoff_threshold);

	__batt_parse_node(
		np, "jeita_temp_pos_0_to_pos_10_cc2topoff_threshold",
		&batt_cust_data.jeita_temp_pos_0_to_pos_10_cc2topoff_threshold);

	__batt_parse_node(
		np, "jeita_temp_neg_10_to_pos_0_cc2topoff_threshold",
		&batt_cust_data.jeita_temp_neg_10_to_pos_0_cc2topoff_threshold);

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
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

	__batt_parse_node(np, "ta_9v_support", &batt_cust_data.ta_9v_support);

	__batt_parse_node(np, "ta_12v_support", &batt_cust_data.ta_12v_support);
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

	get_monotonic_boottime(&batteryThreadRunTime);

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
	class_dev = (struct class_device *)device_create(
		adc_cali_class, NULL, adc_cali_devno, NULL, ADC_CALI_DEVNAME);
	battery_log(BAT_LOG_CRTI, "[BAT_probe] adc_cali prepare : done !!\n ");

	get_charging_control();

	batt_init_cust_data();

	battery_charging_control(CHARGING_CMD_GET_PLATFORM_BOOT_MODE,
				 &g_platform_boot_mode);
	battery_log(BAT_LOG_CRTI, "[BAT_probe] g_platform_boot_mode = %d\n ",
		    g_platform_boot_mode);

	wake_lock_init(&battery_suspend_lock, WAKE_LOCK_SUSPEND,
		       "battery suspend wakelock");
	wake_lock_init(&battery_meter_lock, WAKE_LOCK_SUSPEND,
		       "battery meter wakelock");
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

	wireless_main.psy =
		power_supply_register(&(dev->dev), &wireless_main.psd, NULL);
	if (IS_ERR(wireless_main.psy)) {
		battery_log(
			BAT_LOG_CRTI,
			"[BAT_probe] power_supply_register WIRELESS Fail !!\n");
		ret = PTR_ERR(wireless_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI,
		    "[BAT_probe] power_supply_register WIRELESS Success !!\n");

	battery_main.psy =
		power_supply_register(&(dev->dev), &battery_main.psd, NULL);
	if (IS_ERR(battery_main.psy)) {
		battery_log(
			BAT_LOG_CRTI,
			"[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(battery_main.psy);
		return ret;
	}
	battery_log(BAT_LOG_CRTI,
		    "[BAT_probe] power_supply_register Battery Success !!\n");

#if !defined(CONFIG_POWER_EXT)

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (bat_is_ext_power() == KAL_TRUE) {
		battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
		battery_main.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
		battery_main.BAT_PRESENT = 1;
		battery_main.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
		battery_main.BAT_CAPACITY = 100;
		battery_main.BAT_batt_vol = 4200;
		battery_main.BAT_batt_temp = 220;

		g_bat_init_flag = KAL_TRUE;
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
		ret_device_file =
			device_create_file(&(dev->dev), &dev_attr_Charger_Type);
		ret_device_file =
			device_create_file(&(dev->dev), &dev_attr_Pump_Express);
	}

	/* battery_meter_initial();*/
	/* move to mt_battery_GetBatteryData() */
	/* to decrease booting time */

	/* Initialization BMT Struct */
	BMT_status.bat_exist = KAL_TRUE;      /* phone must have battery */
	BMT_status.charger_exist = KAL_FALSE; /* for default, no charger */
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
	BMT_status.UI_SOC = -100;
	BMT_status.UI_SOC2 = -1;

	BMT_status.bat_charging_state = CHR_PRE;
	BMT_status.bat_in_recharging_state = KAL_FALSE;
	BMT_status.bat_full = KAL_FALSE;
	BMT_status.nPercent_ZCV = 0;
	BMT_status.nPrecent_UI_SOC_check_point =
		battery_meter_get_battery_nPercent_UI_SOC();

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	dual_input_init();
#endif

	/* battery kernel thread for 10s check and charger in/out event */
	/* Replace GPT timer by hrtime */
	battery_kthread_hrtimer_init();

	kthread_run(bat_routine_thread, NULL, "bat_routine_thread");
	kthread_run(bat_update_thread, NULL, "bat_update_thread");
	battery_log(BAT_LOG_CRTI,
		    "[%s] battery kthread init done\n",
			__func__);

	charger_hv_detect_sw_workaround_init();

	/* LOG System Set */
	init_proc_log();

#else
	/* keep HW alive */
	charger_hv_detect_sw_workaround_init();
#endif
	g_bat_init_flag = KAL_TRUE;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (g_vcdt_irq_delay_flag == KAL_TRUE)
		do_chrdet_int_task();
#endif

	return 0;
}

static void battery_timer_pause(void)
{
/* battery_log(BAT_LOG_CRTI, "******** battery driver suspend!! ********\n" );*/
#ifdef CONFIG_POWER_EXT
#else

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (bat_is_ext_power() == KAL_TRUE)
		return 0;
#endif
	mutex_lock(&bat_mutex);
	/* cancel timer */
	hrtimer_cancel(&battery_kthread_timer);
	hrtimer_cancel(&charger_hv_detect_timer);

	battery_suspended = KAL_TRUE;
	mutex_unlock(&bat_mutex);

	battery_log(BAT_LOG_FULL, "@bs=1@\n");
#endif

	get_monotonic_boottime(&g_bat_time_before_sleep);
}

static void battery_timer_resume(void)
{
#ifdef CONFIG_POWER_EXT
#else
	kal_bool is_pcm_timer_trigger = KAL_FALSE;
	struct timespec bat_time_after_sleep;
	ktime_t ktime, hvtime;

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (bat_is_ext_power() == KAL_TRUE)
		return 0;
#endif

	ktime = ktime_set(BAT_TASK_PERIOD, 0); /* 10s, 10* 1000 ms */
	hvtime = ktime_set(0, BAT_MS_TO_NS(2000));

	get_monotonic_boottime(&bat_time_after_sleep);
	battery_charging_control(CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER,
				 &is_pcm_timer_trigger);

	battery_log(
		BAT_LOG_CRTI,
		"[%s] is_pcm_timer_trigger %d bat_spm_timeout %d lbat %d\n",
		__func__,
		is_pcm_timer_trigger, bat_spm_timeout,
		battery_meter_get_low_battery_interrupt_status());

	if (is_pcm_timer_trigger == KAL_TRUE || bat_spm_timeout ||
	    battery_meter_get_low_battery_interrupt_status()) {
		mutex_lock(&bat_mutex);
		battery_meter_reset_sleep_time();
		BAT_thread();
		mutex_unlock(&bat_mutex);
	} else {
		battery_log(BAT_LOG_CRTI,
			    "battery resume NOT by pcm timer!!\n");
	}

	/* phone call last than x min */
	if (g_call_state == CALL_ACTIVE &&
	    (bat_time_after_sleep.tv_sec - g_bat_time_before_sleep.tv_sec >=
	     TALKING_SYNC_TIME)) {
		BMT_status.UI_SOC = battery_meter_get_battery_percentage();
		battery_log(BAT_LOG_CRTI, "Sync UI SOC to SOC immediately\n");
	}

	mutex_lock(&bat_mutex);

	/* restore timer */
	hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);
	hrtimer_start(&charger_hv_detect_timer, hvtime, HRTIMER_MODE_REL);

	battery_suspended = KAL_FALSE;
	battery_log(BAT_LOG_CRTI, "@bs=0@\n");
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

		battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
					 &input_current);
		battery_log(BAT_LOG_CRTI, "%s: reset TA before shutdown\n",
			    __func__);
	}
}

/* Battery Notify API */
static ssize_t show_BatteryNotify(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "[Battery] %s : %x\n",
		    __func__, g_BatteryNotifyCode);

	return sprintf(buf, "%u\n", g_BatteryNotifyCode);
}

static ssize_t store_BatteryNotify(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned int reg_BatteryNotifyCode = 0;
	int ret;

	battery_log(BAT_LOG_CRTI, "[Battery] %s\n", __func__);
	if (buf != NULL && size != 0) {
		battery_log(BAT_LOG_CRTI,
			    "[Battery] buf is %s and size is %zu\n", buf, size);
		ret = kstrtouint(buf, 16, &reg_BatteryNotifyCode);
		g_BatteryNotifyCode = reg_BatteryNotifyCode;
		battery_log(BAT_LOG_CRTI, "[Battery] store code : %x\n",
			    g_BatteryNotifyCode);
	}
	return size;
}

static DEVICE_ATTR(BatteryNotify, 0664, show_BatteryNotify,
		   store_BatteryNotify);

static ssize_t show_BN_TestMode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_CRTI, "[Battery] %s : %x\n",
		    __func__, g_BN_TestMode);
	return sprintf(buf, "%u\n", g_BN_TestMode);
}

static ssize_t store_BN_TestMode(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	unsigned int reg_BN_TestMode = 0;
	int ret;

	battery_log(BAT_LOG_CRTI, "[Battery] %s\n", __func__);
	if (buf != NULL && size != 0) {
		battery_log(BAT_LOG_CRTI,
			    "[Battery] buf is %s and size is %zu\n", buf, size);
		ret = kstrtouint(buf, 16, &reg_BN_TestMode);
		g_BN_TestMode = reg_BN_TestMode;
		battery_log(BAT_LOG_CRTI,
			    "[Battery] store g_BN_TestMode : %x\n",
			    g_BN_TestMode);
	}
	return size;
}

static DEVICE_ATTR(BN_TestMode, 0664, show_BN_TestMode, store_BN_TestMode);

/*platform_driver API */
#if 0
static int battery_cmd_read(char *buf, char **start, off_t off,
				int count, int *eof, void *data)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p,
		"g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\n",
		"battery_cmd_thermal_test_mode_value=%d\n",
		g_battery_thermal_throttling_flag,
		battery_cmd_thermal_test_mode,
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

static ssize_t battery_cmd_write(struct file *file, const char *buffer,
				 size_t count, loff_t *data)
{
	int len = 0, bat_tt_enable = 0, bat_thr_test_mode = 0,
	    bat_thr_test_value = 0;
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

		battery_log(
			BAT_LOG_CRTI,
			"bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			g_battery_thermal_throttling_flag,
			battery_cmd_thermal_test_mode,
			battery_cmd_thermal_test_mode_value);

		return count;
	}

	/* hidden else, for sscanf format error */
	{
		battery_log(
			BAT_LOG_CRTI,
			"bad argument, echo [bat_tt_enable] [bat_thr_test_mode] [bat_thr_test_value] > battery_cmd\n");
	}

	return -EINVAL;
}

static int proc_utilization_show(struct seq_file *m, void *v)
{
	seq_printf(
		m,
		"=> g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		g_battery_thermal_throttling_flag,
		battery_cmd_thermal_test_mode,
		battery_cmd_thermal_test_mode_value);

	seq_printf(m,
		   "=> get_usb_current_unlimited=%d,\ncmd_discharging = %d\n",
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

static ssize_t current_cmd_write(struct file *file, const char *buffer,
				 size_t count, loff_t *data)
{
	int len = 0;
	char desc[32];
	int cmd_current_unlimited = false;
	unsigned int charging_enable = false;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &cmd_current_unlimited, &cmd_discharging) ==
	    2) {
		set_usb_current_unlimited(cmd_current_unlimited);
		if (cmd_discharging == 1) {
			charging_enable = false;
			adjust_power = -1;
		} else if (cmd_discharging == 0) {
			charging_enable = true;
			adjust_power = -1;
		}
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

		battery_log(
			BAT_LOG_CRTI,
			"[%s] cmd_current_unlimited=%d, cmd_discharging=%d\n",
			__func__,
			cmd_current_unlimited, cmd_discharging);
		return count;
	}

	/* hidden else, for sscanf format error */
	{
		battery_log(BAT_LOG_CRTI,
			    "  bad argument, echo [enable] > current_cmd\n");
	}

	return -EINVAL;
}

static int current_cmd_read(struct seq_file *m, void *v)
{
	unsigned int charging_enable = false;

	cmd_discharging = 1;
	charging_enable = false;
	adjust_power = -1;

	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	battery_log(BAT_LOG_CRTI, "[current_cmd_write] cmd_discharging=%d\n",
		    cmd_discharging);

	return 0;
}

static int proc_utilization_open_cur_stop(struct inode *inode,
					  struct file *file)
{
	return single_open(file, current_cmd_read, NULL);
}

static ssize_t discharging_cmd_write(struct file *file, const char *buffer,
				     size_t count, loff_t *data)
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
	}

	/* hidden else, for sscanf format error */
	{
		battery_log(BAT_LOG_CRTI,
			    "  bad argument, echo [enable] > current_cmd\n");
	}

	return -EINVAL;
}

static int cmd_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t power_path_cmd_read(struct file *file, char __user *user_buffer,
				   size_t count, loff_t *position)
{
	char buf[256];
	bool power_path_en = true;

	battery_charging_control(CHARGING_CMD_GET_IS_POWER_PATH_ENABLE,
				 &power_path_en);
	count = sprintf(buf, "%d\n", power_path_en);

	return simple_read_from_buffer(user_buffer, count, position, buf,
				       strlen(buf));
}

static ssize_t power_path_cmd_write(struct file *file, const char *buffer,
				    size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32];
	u32 enable = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		battery_charging_control(CHARGING_CMD_ENABLE_POWER_PATH,
					 &enable);
		battery_log(BAT_LOG_CRTI, "%s: enable power path = %d\n",
			    __func__, enable);
		return count;
	}

	battery_log(BAT_LOG_CRTI, "bad argument, echo [enable] > power_path\n");
	return count;
}

static ssize_t safety_timer_cmd_read(struct file *file,
				     char __user *user_buffer, size_t count,
				     loff_t *position)
{
	char buf[256];
	bool safety_timer_en = true;

	battery_charging_control(CHARGING_CMD_GET_IS_SAFETY_TIMER_ENABLE,
				 &safety_timer_en);
	count = sprintf(buf, "%d\n", safety_timer_en);

	return simple_read_from_buffer(user_buffer, count, position, buf,
				       strlen(buf));
}

static ssize_t safety_timer_cmd_write(struct file *file, const char *buffer,
				      size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32];
	u32 enable = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		battery_charging_control(CHARGING_CMD_ENABLE_SAFETY_TIMER,
					 &enable);
		battery_log(BAT_LOG_CRTI, "%s: enable safety timer = %d\n",
			    __func__, enable);
		return count;
	}

	battery_log(BAT_LOG_CRTI,
		    "bad argument, echo [enable] > safety_timer\n");
	return count;
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

static const struct file_operations power_path_cmd_fops = {
	.owner = THIS_MODULE,
	.open = cmd_open,
	.read = power_path_cmd_read,
	.write = power_path_cmd_write,
};

static const struct file_operations safety_timer_cmd_fops = {
	.owner = THIS_MODULE,
	.open = cmd_open,
	.read = safety_timer_cmd_read,
	.write = safety_timer_cmd_write,
};

static int mt_batteryNotify_probe(struct platform_device *dev)
{
	int ret_device_file = 0;
	struct proc_dir_entry *battery_dir = NULL;

	battery_log(BAT_LOG_CRTI,
		    "******** %s!! ********\n", __func__);

	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_BatteryNotify);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_BN_TestMode);

	/* Create mtk_battery_cmd directory */
	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		pr_notice("[%s]: mkdir /proc/mtk_battery_cmd failed\n",
					__func__);
		goto _out;
	}

	/* Create nodes */
	proc_create("battery_cmd", 0644, battery_dir, &battery_cmd_proc_fops);
	battery_log(BAT_LOG_CRTI, "proc_create battery_cmd_proc_fops\n");

	proc_create("current_cmd", 0644, battery_dir, &current_cmd_proc_fops);
	battery_log(BAT_LOG_CRTI, "proc_create current_cmd_proc_fops\n");

	proc_create("discharging_cmd", 0644, battery_dir,
		    &discharging_cmd_proc_fops);
	battery_log(BAT_LOG_CRTI, "proc_create discharging_cmd_proc_fops\n");

	proc_create("en_power_path", 0644, battery_dir, &power_path_cmd_fops);
	battery_log(BAT_LOG_CRTI, "proc_create power_path_proc_fops\n");

	proc_create("en_safety_timer", 0644, battery_dir,
		    &safety_timer_cmd_fops);
	battery_log(BAT_LOG_CRTI, "proc_create safety_timer_proc_fops\n");

_out:
	battery_log(BAT_LOG_CRTI, "******** mtk_battery_cmd!! ********\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_battery_of_match[] = {
	{
		.compatible = "mediatek,battery",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mt_battery_of_match);
#endif

static int battery_pm_suspend(struct device *device)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(device);

	WARN(pdev == NULL, "pdev is null");
	return ret;
}

static int battery_pm_resume(struct device *device)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(device);

	WARN(pdev == NULL, "pdev is null");
	return ret;
}

static int battery_pm_freeze(struct device *device)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(device);

	WARN(pdev == NULL, "pdev is null");
	return ret;
}

static int battery_pm_restore(struct device *device)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(device);

	WARN(pdev == NULL, "pdev is null");
	return ret;
}

static int battery_pm_restore_noirq(struct device *device)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(device);

	WARN(pdev == NULL, "pdev is null");
	return ret;
}

struct dev_pm_ops const battery_pm_ops = {
	.suspend = battery_pm_suspend,
	.resume = battery_pm_resume,
	.freeze = battery_pm_freeze,
	.thaw = battery_pm_restore,
	.restore = battery_pm_restore,
	.restore_noirq = battery_pm_restore_noirq,
};

#if defined(CONFIG_OF) || defined(BATTERY_MODULE_INIT)
struct platform_device battery_device = {
	.name = "battery", .id = -1,
};
#endif

static struct platform_driver battery_driver = {
	.probe = battery_probe,
	.remove = battery_remove,
	.shutdown = battery_shutdown,
	.driver = {


			.name = "battery", .pm = &battery_pm_ops,
		},
};

#ifdef CONFIG_OF
static int battery_dts_probe(struct platform_device *dev)
{
	int ret = 0;

	battery_log(BAT_LOG_CRTI, "******** %s!! ********\n", __func__);
	battery_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&battery_device);
	if (ret) {
		battery_log(
			BAT_LOG_CRTI,
			"****[%s] Unable to register device (%d)\n",
			__func__, ret);
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
	{
		.compatible = "mediatek,bat_notify",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mt_bat_notify_of_match);
#endif

struct platform_device MT_batteryNotify_device = {
	.name = "mt-battery", .id = -1,
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

	battery_log(BAT_LOG_CRTI,
		    "******** %s!! ********\n",
			__func__);

	MT_batteryNotify_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		battery_log(
			BAT_LOG_CRTI,
			"****[%s] Unable to register device (%d)\n",
			__func__, ret);
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
	case PM_HIBERNATION_PREPARE: /* Going to hibernate */
		battery_log(BAT_LOG_FULL, "[%s] pm_event %lu (IPOH)\n",
			    __func__, pm_event);
		Is_In_IPOH = TRUE;
		battery_timer_pause();
		return NOTIFY_DONE;

	case PM_RESTORE_PREPARE: /* Going to restore a saved image */
		battery_timer_pause();
		return NOTIFY_DONE;

	case PM_SUSPEND_PREPARE: /* Going to suspend the system */
		battery_log(BAT_LOG_FULL, "[%s] pm_event %lu\n", __func__,
			    pm_event);
		battery_timer_pause();
		return NOTIFY_DONE;

	case PM_POST_SUSPEND: /* Suspend finished */
	case PM_POST_RESTORE: /* Restore failed */
		battery_log(BAT_LOG_FULL, "[%s] pm_event %lu\n", __func__,
			    pm_event);
		battery_timer_resume();
		return NOTIFY_DONE;

	case PM_POST_HIBERNATION: /* Hibernation finished */
		battery_log(BAT_LOG_FULL, "[%s] pm_event %lu\n", __func__,
			    pm_event);
		fg_ipoh_reset = 1;
		battery_timer_resume();
		if (pending_wake_up_bat) {
			battery_log(
				BAT_LOG_FULL,
				"[%s] PM_POST_HIBERNATION b4r wakeup bat_routine_wq\n",
				__func__);
			wake_up(&bat_routine_wq);
		}
		pending_wake_up_bat = FALSE;
		Is_In_IPOH = FALSE;

		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static int battery_pm_event(struct notifier_block *notifier,
			    unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE: /* Going to hibernate */
	case PM_RESTORE_PREPARE:     /* Going to restore a saved image */
	case PM_SUSPEND_PREPARE:     /* Going to suspend the system */
		battery_log(BAT_LOG_FULL, "[%s] pm_event %lu\n", __func__,
			    pm_event);
		battery_timer_pause();
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION: /* Hibernation finished */
	case PM_POST_SUSPEND:     /* Suspend finished */
	case PM_POST_RESTORE:     /* Restore failed */
		battery_log(BAT_LOG_FULL, "[%s] pm_event %lu\n", __func__,
			    pm_event);
		battery_timer_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static int battery_pm_event(struct notifier_block *notifier,
			    unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE: /* Going to hibernate */
	case PM_RESTORE_PREPARE:     /* Going to restore a saved image */
	case PM_SUSPEND_PREPARE:     /* Going to suspend the system */
		battery_log(BAT_LOG_FULL, "[%s] pm_event %lu\n", __func__,
			    pm_event);
		battery_timer_pause();
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION: /* Hibernation finished */
	case PM_POST_SUSPEND:     /* Suspend finished */
	case PM_POST_RESTORE:     /* Restore failed */
		battery_log(BAT_LOG_FULL, "[%s] pm_event %lu\n", __func__,
			    pm_event);
		battery_timer_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block battery_pm_notifier_block = {
	.notifier_call = battery_pm_event, .priority = 0,
};

static int __init battery_init(void)
{
	int ret;

	battery_log(BAT_LOG_CRTI, "%s\n", __func__);

#ifdef CONFIG_OF
/* */
#else

#ifdef BATTERY_MODULE_INIT
	ret = platform_device_register(&battery_device);
	if (ret) {
		battery_log(
			BAT_LOG_CRTI,
			"****[battery_device] Unable to device register(%d)\n",
			ret);
		return ret;
	}
#endif
#endif

	ret = platform_driver_register(&battery_driver);
	if (ret) {
		battery_log(
			BAT_LOG_CRTI,
			"****[battery_driver] Unable to register driver (%d)\n",
			ret);
		return ret;
	}
/* battery notofy UI */
#ifdef CONFIG_OF
/* */
#else
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		battery_log(
			BAT_LOG_CRTI,
			"****[mt_batteryNotify] Unable to device register(%d)\n",
			ret);
		return ret;
	}
#endif
	ret = platform_driver_register(&mt_batteryNotify_driver);
	if (ret) {
		battery_log(
			BAT_LOG_CRTI,
			"****[mt_batteryNotify] Unable to register driver (%d)\n",
			ret);
		return ret;
	}
#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_dts_driver);
	ret = platform_driver_register(&mt_batteryNotify_dts_driver);
#endif
	ret = register_pm_notifier(&battery_pm_notifier_block);
	if (ret)
		battery_log(BAT_LOG_CRTI,
			    "[%s] failed to register PM notifier %d\n",
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
module_init(battery_init);
module_exit(battery_exit);
#endif

MODULE_AUTHOR("Oscar Liu");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
