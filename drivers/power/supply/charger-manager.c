// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This driver enables to monitor battery health and control charger
 * during suspend-to-mem.
 * Charger manager depends on other devices. Register this later than
 * the depending devices.
 *
**/

//This file has been modified by Unisoc (Shanghai) Technologies Co., Ltd in 2023.

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/firmware.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/usb/sprd_pd.h>
#include <linux/workqueue.h>
#include <linux/usb/sprd_tcpm.h>
#include <linux/usb/sprd_pd.h>
#include <linux/syscore_ops.h>
#include <linux/iio/consumer.h>
#include "lc_charger_class.h"
#include "../../usb/musb/musb_sprd.h"
#include "battery_secret/battery_secret_class.h"
#include "battery_secret/battery_secret_logic.h"
#include "charger_partition.h"

/*
 * Default temperature threshold for charging.
 * Every temperature units are in tenth of centigrade.
 */
#define CM_DEFAULT_RECHARGE_TEMP_DIFF		50
#define CM_DEFAULT_CHARGE_TEMP_MAX		500
#define CM_UVLO_OFFSET				50000
#define CM_FORCE_SET_FUEL_CAP_FULL		1000
#define CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD	3400000
#define CM_UVLO_CALIBRATION_CNT_THRESHOLD	5
#define CM_LOW_CAP_SHUTDOWN_VOLTAGE_THRESHOLD	3400000
#define CM_UNKNOW_TYPE_CURRENT_THRESHOLD_H	2000000
#define CM_UNKNOW_TYPE_CURRENT_THRESHOLD_L	500000

#define CM_CAP_ONE_PERCENT			10
#define CM_HCAP_THRESHOLD			1001
#define CM_CAP_FULL_PERCENT			1000
#define CM_MAGIC_NUM				0x5A5AA5A5
#define CM_CAPACITY_LEVEL_CRITICAL		0
#define CM_CAPACITY_LEVEL_LOW			15
#define CM_CAPACITY_LEVEL_NORMAL		85
#define CM_CAPACITY_LEVEL_FULL			100
#define CM_CAPACITY_LEVEL_CRITICAL_VOLTAGE	3400000

/* Fast charge public parameters */
#define CM_FAST_CHARGE_ENABLE_CURRENT		1200000
#define CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT	1000000
#define CM_FAST_CHARGE_CURRENT_2A		2000000
#define CM_FAST_CHARGE_VOLTAGE_20V		20000000
#define CM_FAST_CHARGE_VOLTAGE_15V		15000000
#define CM_FAST_CHARGE_VOLTAGE_12V		12000000
#define CM_FAST_CHARGE_VOLTAGE_9V		9000000
#define CM_FAST_CHARGE_VOLTAGE_5V		5000000
#define CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD	3520000
#define CM_FAST_CHARGE_START_VOLTAGE_HTHRESHOLD	4200000
#define CM_FAST_CHARGE_VOLTAGE_CHECK_COUNT	3
#define CM_FAST_CHARGE_START_SOC_HTHRESHOLD	950

#define CM_QUICK_CHARGE_TYPE_RETRY_MAX	10
#define CM_QUICK_CHARGE_TYPE_WORK_INTERVAL_MS	100

/* Fixed fast charge parameters */
#define CM_FIXED_FCHG_DISABLE_BATTERY_VOLTAGE	3400000
#define CM_FIXED_FCHG_DISABLE_CURRENT		1000000
#define CM_FIXED_FCHG_C2C_CURRENT		1000000
#define CM_FIXED_FCHG_C2C_IBUS_THRESHOLD	1500000
#define CM_FIXED_FCHG_TRANSITION_CURRENT_1P5A	1500000
#define CM_FIXED_FCHG_VOLTAGE_5V_THRESHOLD	6500000
#define CM_FIXED_FCHG_VOLTAGE_9V_THRESHOLD	10500000
#define CM_FIXED_FCHG_ENABLE_COUNT		3
#define CM_FIXED_FCHG_DISABLE_COUNT		2
#define CM_FIXED_FCHG_CHK_DIS_WORK_MS		5000
#define CM_FIXED_FCHG_TRY_DIS_WORK_MS		100

/* Dynamic fast charge parameters */
#define CM_CP_VSTEP				20000
#define CM_CP_VSTEP_MAX				(5 * CM_CP_VSTEP)
#define CM_CP_ISTEP				50000
#define CM_CP_PRIMARY_CHARGER_DIS_TIMEOUT	20
#define CM_CP_TAPER_DELTA_VBAT_THRESHOLD	50000
#define CM_CP_TAPER_UCP_THRESHOLD		5
#define CM_CP_IBAT_UCP_THRESHOLD		8
#define CM_CP_ADJUST_VOLTAGE_THRESHOLD		(5 * 1000 / CM_CP_WORK_TIME_MS)
#define CM_CP_ACC_VBAT_HTHRESHOLD		3850000
#define CM_CP_BUCK_IBUS_START			100000
#define CM_CP_BUCK_IBAT_START			100000
#define CM_CP_BUCK_IBAT_MAX_P			40
#define CM_CP_BUCK_ISTEP			(CM_CP_ISTEP)
#define CM_CP_BUCK_CHG_EFFICIENCY_P		90
#define CM_CP_CHG_EFFICIENCY_P			97
#define CM_CP_VBAT_STEP1			300000
#define CM_CP_VBAT_STEP2			200000
#define CM_CP_VBAT_STEP3			100000
#define CM_CP_VBAT_STEP4			50000
#define CM_CP_VBAT_STEP5			10000
#define CM_CP_IBAT_STEP1			2000000
#define CM_CP_IBAT_STEP2			1000000
#define CM_CP_IBAT_STEP3			100000
#define CM_CP_VBUS_STEP1			2000000
#define CM_CP_VBUS_STEP2			1000000
#define CM_CP_VBUS_STEP3			50000
#define CM_CP_IBUS_STEP1			1000000
#define CM_CP_IBUS_STEP2			500000
#define CM_CP_IBUS_STEP3			100000
#define CM_CP_THERMAL_STEP1			8000000
#define CM_CP_THERMAL_STEP2			4000000
#define CM_CP_BUCK_IBUS_STEP1			1000000
#define CM_CP_BUCK_IBUS_STEP2			500000
#define CM_CP_BUCK_IBUS_STEP3			100000
#define CM_CP_DEFAULT_TAPER_CURRENT		1000000

#define CM_CP_STEP_CHG_DOWN_COUNT		2

#define CM_PPS_5V_PROG_MAX			6200000
#define CM_PPS_VOLTAGE_11V			11000000
#define CM_PPS_VOLTAGE_16V			16000000
#define CM_PPS_VOLTAGE_21V			21000000

#define CM_CP_VBUS_ERRORLO_THRESHOLD(x)		((int)(x * 205 / 100))
#define CM_CP_VBUS_ERRORHI_THRESHOLD(x)		((int)(x * 240 / 100))

#define CM_IR_COMPENSATION_TIME			3

#define CM_CP_WORK_TIME_MS			500

#define CM_CHARGER_TYPE_WORK_TIME_MS		500
#define CM_CHARGER_TYPE_TIME_OUT_CNT		20

#define CM_DCD_WORK_FIRST_TIME_5S		5000
#define CM_DCD_WORK_TIME_10S		10000
#define CM_DCD_TIME_OUT_CNT		5

#define CM_CAP_ONE_TIME_16S			16
#define CM_CAP_ONE_TIME_8S			8
#define CM_CAP_ONE_TIME_4S			4
#define CM_CAP_CYCLE_TRACK_TIME_15S		15
#define CM_CAP_CYCLE_TRACK_TIME_8S		8
#define CM_CAP_CYCLE_TRACK_TIME_4S		4
#define CM_CAP_CYCLE_TRACK_TIME_2S		2
#define CM_CAP_CALC_BATT_WORKS_LOW_TEMP		50
#define CM_CAP_CALC_BATT_WORKS_LOW_CAP		50
#define CM_CAP_CALC_BATT_WORKS_BIG_CUR_UA	3000000
#define CM_CAP_CALC_BATT_WORKS_SOC_GAP		50
#define CM_INIT_BOARD_TEMP			250

#define TYPEC_MODE_SNK			        0
#define TYPEC_MODE_SRC			        1
#define TYPEC_MODE_DRP			        2

/* Google limit power transfer support */
#define CM_LIMIT_POWER_TRANSFER_MA		110

/* Single soft multi hard scheme parameters */
#define CM_CHECK_ALT_CP_PSY_TH_MS		600

/* Charge power detect */
#define POWER_DETECT_ILIMIT_1000MA 1000*1000
#define POWER_DETECT_ILIMIT_1500MA 1500*1000
#define POWER_DETECT_ILIMIT_2000MA 2000*1000
#define POWER_DETECT_ILIMIT_2200MA 2200*1000
#define POWER_DETECT_ILIMIT_2500MA 2500*1000
#define POWER_DETECT_ILIMIT_3000MA 3000*1000
#define POWER_DETECT_STEP_100MA 100*1000
#define VBUS_INT_VOL_3300MV 3300*1000

/* dynamic modify DPM */
#define VINDPM_4400 4400
#define VINDPM_4500 4500
#define VINDPM_4600 4600
#define VBAT_4100 4100000
#define VBAT_4300 4300000
#define POWER_DET_CNT 8
#define POWER_DETECT_NUM 2

/* plugout reset cur */
#define PLUGOUT_LIMIT_200MA 	200*1000
#define PLUGOUT_CUR_100MA 	100*1000

/* USB enumeration current limit value */
#define CM_SDP_TYPE_USB_ENUM_LIMIT_CUR_UA	500000

#define SHUTDOWN_DELAY_VOL_MAX		3400000
#define SHUTDOWN_DELAY_VOL_MIN		3300000

#define MTBF_CURRENT_MIN		0     // 0mA
#define MTBF_CURRENT_MAX		2000  // 2000mA
#define MTBF_CURRENT_DEFAULT		1500  // 1500mA

#define NONSTAND_BATT_CURR		500
#define NONSTAND_BATT_CAP		15
#define NONSTAND_BATT_TEMP		250

#define SDP_FULLBAT_UA		210000
#define SDP_ITERM_MA		120

static const char * const cm_cp_state_names[] = {
	[CM_CP_STATE_UNKNOWN] = "Charge pump state: UNKNOWN",
	[CM_CP_STATE_RECOVERY] = "Charge pump state: RECOVERY",
	[CM_CP_STATE_ENTRY] = "Charge pump state: ENTRY",
	[CM_CP_STATE_CHECK_VBUS] = "Charge pump state: CHECK VBUS",
	[CM_CP_STATE_TUNE] = "Charge pump state: TUNE",
	[CM_CP_STATE_EXIT] = "Charge pump state: EXIT",
};

static const char * const REAL_TYPE_TEXT[] = {
	[POWER_SUPPLY_USB_TYPE_UNKNOWN]		= "Unknown",
	[POWER_SUPPLY_USB_TYPE_SDP]		= "USB",
	[POWER_SUPPLY_USB_TYPE_CDP]		= "USB_CDP",
	[POWER_SUPPLY_USB_TYPE_DCP]		= "USB_DCP",
	[POWER_SUPPLY_USB_TYPE_PD]		= "USB_PD",
	[POWER_SUPPLY_USB_TYPE_C]		= "USB_FLOAT",
};

static char *charger_manager_supplied_to[] = {
	"audio-ldo",
};

/*
 * Regard CM_JIFFIES_SMALL jiffies is small enough to ignore for
 * delayed works so that we can run delayed works with CM_JIFFIES_SMALL
 * without any delays.
 */
#define	CM_JIFFIES_SMALL	(2)

/* If y is valid (> 0) and smaller than x, do x = y */
#define CM_MIN_VALID(x, y)	x = (((y > 0) && ((x) > (y))) ? (y) : (x))

/*
 * Regard CM_RTC_SMALL (sec) is small enough to ignore error in invoking
 * rtc alarm. It should be 2 or larger
 */
#define CM_RTC_SMALL		(2)

#define CM_EVENT_TYPE_NUM	6

static struct charger_type charger_usb_type[20] = {
	{POWER_SUPPLY_USB_TYPE_SDP, CM_CHARGER_TYPE_SDP},
	{POWER_SUPPLY_USB_TYPE_DCP, CM_CHARGER_TYPE_DCP},
	{POWER_SUPPLY_USB_TYPE_CDP, CM_CHARGER_TYPE_CDP},
	{POWER_SUPPLY_USB_TYPE_UNKNOWN, CM_CHARGER_TYPE_UNKNOWN},
};

static struct charger_type charger_fchg_type[20] = {
	{POWER_SUPPLY_CHARGE_TYPE_FAST, CM_CHARGER_TYPE_FAST},
	{POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE, CM_CHARGER_TYPE_ADAPTIVE},
	{POWER_SUPPLY_CHARGE_TYPE_UNKNOWN, CM_CHARGER_TYPE_UNKNOWN},
};

static struct charger_type charger_wireless_type[20] = {
	{POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP, CM_WIRELESS_CHARGER_TYPE_BPP},
	{POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP, CM_WIRELESS_CHARGER_TYPE_EPP},
	{POWER_SUPPLY_WIRELESS_CHARGER_TYPE_UNKNOWN, CM_CHARGER_TYPE_UNKNOWN},
};

enum battery_id{
	B_UNKNOWN = 0,
	B_THE_1_SUPPLIER,
	B_THE_2_SUPPLIER,
	B_THE_3_SUPPLIER,
	B_THE_4_SUPPLIER,
	B_THE_5_SUPPLIER,
	B_THE_6_SUPPLIER,
	B_THE_7_SUPPLIER,
	B_THE_8_SUPPLIER,
	B_MAX,
};
struct r_item{
	int vmin;
	int vmax;
	int id;
	int bat_resistance_id;
	int charge_full_design;
	const char* manufacturer;
	const char* batt_type;
};

static const struct r_item r_items_param_dual[] = {
	//unknown
	{
		.vmin = 0,
		.vmax = 0,
		.id		= B_UNKNOWN,
		.bat_resistance_id = 0,
		.charge_full_design = 0,
		.manufacturer = "UNKNOWN",
		.batt_type = "UNKNOWN",
	},
	//68k
	{
		.vmin = 720,
		.vmax = 850,
		.id		= B_THE_1_SUPPLIER,
		.bat_resistance_id = 68000,
		.charge_full_design = 6000000,
		.manufacturer = "NVT",
		.batt_type = "Somalia-A_NVT_68K_6000mAh",
	},
	//330k
	{
		.vmin = 1010,
		.vmax = 1170,
		.id		= B_THE_2_SUPPLIER,
		.bat_resistance_id = 330000,
		.charge_full_design = 6000000,
		.manufacturer = "SWD",
		.batt_type = "Somalia-A_SWD_330K_6000mAh",
	},
	//100k
	{
		.vmin = 851,
		.vmax = 970,
		.id		= B_THE_3_SUPPLIER,
		.bat_resistance_id = 100000,
		.charge_full_design = 6000000,
		.manufacturer = "COS",
		.batt_type = "Somalia-A_COS_100K_6000mAh",
	},
};

static const struct r_item r_items_param_single[] = {
	//unknown
	{
		.vmin = 0,
		.vmax = 0,
		.id		= B_UNKNOWN,
		.bat_resistance_id = 0,
		.charge_full_design = 0,
		.manufacturer = "UNKNOWN",
		.batt_type = "UNKNOWN",
	},
	//330k
	{
		.vmin = 1010,
		.vmax = 1170,
		.id		= B_THE_1_SUPPLIER,
		.bat_resistance_id = 330000,
		.charge_full_design = 6300000,
		.manufacturer = "SWD",
		.batt_type = "Somalia-A_SWD_330K_6300mAh",
	},
	//68k
	{
		.vmin = 720,
		.vmax = 850,
		.id		= B_THE_2_SUPPLIER,
		.bat_resistance_id = 68000,
		.charge_full_design = 6300000,
		.manufacturer = "NVT",
		.batt_type = "Somalia-A_NVT_68K_6300mAh",
	},
};

static const struct r_item r_items_param_secret[] = {
	//unknown
	{
		.vmin = 0,
		.vmax = 0,
		.id		= B_UNKNOWN,
		.bat_resistance_id = 0,
		.charge_full_design = 0,
		.manufacturer = "UNKNOWN",
		.batt_type = "UNKNOWN",
	},
	//68k
	{
		.vmin = 720,
		.vmax = 850,
		.id		= B_THE_1_SUPPLIER,
		.bat_resistance_id = 68000,
		.charge_full_design = 6000000,
		.manufacturer = "NVT",
		.batt_type = "Somalia-A_NVT_68K_6000mAh",
	},
	//100k
	{
		.vmin = 851,
		.vmax = 970,
		.id		= B_THE_2_SUPPLIER,
		.bat_resistance_id = 100000,
		.charge_full_design = 6000000,
		.manufacturer = "COS",
		.batt_type = "Somalia-A_COS_100K_6000mAh",
	},
};
static const struct r_item *r_items_param = r_items_param_dual;
static int r_item_len = ARRAY_SIZE(r_items_param_dual);

static LIST_HEAD(cm_list);
static DEFINE_MUTEX(cm_list_mtx);

static struct charger_manager *g_cm = NULL;
/* About in-suspend (suspend-again) monitoring */
static struct alarm *cm_timer;

static bool cm_suspended;
static bool cm_timer_set;
static unsigned long cm_suspend_duration_ms;
static int cm_event_num;
static enum cm_event_types cm_event_type[CM_EVENT_TYPE_NUM];
static char *cm_event_msg[CM_EVENT_TYPE_NUM];

/* About normal (not suspended) monitoring */
static unsigned long polling_jiffy = ULONG_MAX; /* ULONG_MAX: no polling */
static unsigned long next_polling; /* Next appointed polling time */
static struct workqueue_struct *cm_wq; /* init at driver add */
static struct delayed_work cm_monitor_work; /* init at driver add */

static bool allow_charger_enable;
static bool is_charger_mode;

#define  THERMAL_MAX_LEVEL  16
static int  thermal_battery_current_limit_buf[THERMAL_MAX_LEVEL]={
	3000000,2700000,2400000,2100000,1800000,1500000,1200000,900000,600000,300000,300000,300000,300000,300000,300000,300000
};

static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg);
static bool cm_manager_adjust_current(struct charger_manager *cm, int jeita_status);
static void cm_update_charger_type_status(struct charger_manager *cm);
static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp);
static bool cm_charger_is_support_fchg(struct charger_manager *cm);
static int cm_get_battery_temperature(struct charger_manager *cm, int *temp);
static bool cm_pd_is_ac_online(struct charger_manager *cm);
static int cm_cp_step_algo(struct charger_manager *cm);
static void cm_adjust_buck_ibat_limit_algo(struct charger_manager *cm);
static void cm_adjust_cp_ibus_limit_algo(struct charger_manager *cm, int cp_step);
static int cm_get_bat_id(struct charger_manager *cm);
static void cm_get_uisoc(struct charger_manager *cm, int *uisoc);
extern int sprd_battery_parse_cmdline_match_by_split(char *match_str, char* split, char *result, int size);

static bool is_need_set_drp_to_sink(struct charger_manager *cm)
{
	if(IS_ERR_OR_NULL(cm)) {
		pr_err("%s cm is err or null\n", __func__);
		return false;
	}
	pr_info("%s screen_on is settled to %d, audio_on: %d, cid_enable: %d\n",
			__func__,
			cm->screen_on,
			cm->audio_on,
			cm->cid_enable);
	if ((!cm->screen_on) &&
		(!cm->audio_on) &&
		cm->cid_enable) {
		pr_info("%s need set drp to sink\n", __func__);
		return true;
	} else {
		pr_info("%s needn't set drp to sink\n", __func__);
		return false;
	}
}

static void soft_cid_detect_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  cid_detect_work);
	if(IS_ERR_OR_NULL(cm)) {
		pr_err("%s cm is err or null\n", __func__);
		return;
	}
	if (sc27xx_get_current_status_detach_or_attach()) {
		pr_info("%s typec_is_attach, do nothing\n", __func__);
		return;
	}
	if (is_need_set_drp_to_sink(cm)) {
		pr_info("%s set drp status to sink\n", __func__);
		sc27xx_typec_set_mode(TYPEC_MODE_SNK);
	} else {
		pr_info("%s set drp status to try_sink\n", __func__);
		sc27xx_typec_set_mode(TYPEC_MODE_DRP);
	}
}

/**
 * is_batt_present - See if the battery presents in place.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_batt_present(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool present = false;
	int i, ret;

	switch (cm->desc->battery_present) {
	case CM_BATTERY_PRESENT:
		present = true;
		break;
	case CM_NO_BATTERY:
		break;
	case CM_FUEL_GAUGE:
		psy = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!psy)
			break;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
		if (ret == 0 && val.intval)
			present = true;
		power_supply_put(psy);
		break;
	case CM_CHARGER_STAT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
			power_supply_put(psy);
			if (ret == 0 && val.intval) {
				present = true;
				break;
			}
		}
		break;
	}

	return present;
}

static bool is_ext_wl_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool online = false;
	int i, ret;

	if (!cm->desc->psy_wl_charger_stat)
		return online;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_wl_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		power_supply_put(psy);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

/**
 * is_ext_usb_pwr_online - See if an external power source is attached to charge
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if at least one of the chargers of the battery has an external
 * power source attached to charge the battery regardless of whether it is
 * actually charging or not.
 */
static bool is_ext_usb_pwr_online(struct charger_manager *cm)
{
	bool online = false;

	if (cm->vchg_info->ops && cm->vchg_info->ops->is_charger_online)
		return cm->vchg_info->ops->is_charger_online(cm->vchg_info);

	return online;
}

static bool is_ext_pwr_online(struct charger_manager *cm)
{
	bool online = false;

	if (is_ext_usb_pwr_online(cm) || is_ext_wl_pwr_online(cm))
		online = true;

	return online;
}

/**
 * get_cp_ibat_uA - Get the charge current of the battery from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uA: the current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_ibat_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	*uA = 0;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_IBAT_CURRENT_NOW_CMD;
		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0)
			*uA += val.intval;
	}

	return ret;
}

/**
 * get_cp_vbat_uV - Get the voltage level of the battery from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_vbat_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0) {
			*uV = val.intval;
			break;
		}
	}

	return ret;
}

/**
 * get_cp_vbus_uV - Get the voltage level of the bus from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_vbus_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
		power_supply_put(cp_psy);
		if (ret == 0) {
			*uV = val.intval;
			break;
		}
	}

	return ret;
}

 /**
  * get_cp_ibus_uA - Get the current level of the charge pump
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_cp_ibus_uA(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm->desc->psy_cp_stat)
		return 0;

	*cur = 0;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_IBUS_CURRENT_NOW_CMD;
		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static int get_cp_ibat_uA_by_id(struct charger_manager *cm, int *cur, int id)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret = -ENODEV;

	*cur = 0;

	if (!cm->desc->psy_cp_stat || !cm->desc->psy_cp_stat[id])
		return 0;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[id]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
			cm->desc->psy_cp_stat[id]);
		return ret;
	}

	ret = power_supply_get_property(cp_psy,
					POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(cp_psy);
	if (ret == 0)
		*cur = val.intval;

	return ret;
}

 /**
  * get_ibat_avg_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_ibat_avg_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

 /**
  * get_ibat_now_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_ibat_now_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = 0;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

/**
 *
 * get_vbat_avg_uV - Get the voltage level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_vbat_avg_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uV = val.intval;
	return 0;
}

/*
 * get_batt_ocv - Get the battery ocv
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_ocv(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_OCV, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/*
 * get_batt_now - Get the battery voltage now
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_vbat_now_uV(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/**
 * get_batt_cap - Get the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_CAPACITY;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

static int get_batt_raw_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_RAW_CAPACITY;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval / 10;
	return 0;
}

/**
 * get_batt_total_uah - Get the total capacity level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the total_cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_total_uah(struct charger_manager *cm, u32 *total_uah)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CHARGE_FULL,
					&val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*total_uah = val.intval;

	return 0;
}

/*
 * get_boot_cap - Get the battery boot capacity
 * of the battery.
 * @cm: the Charger Manager representing the battery.
 * @cap: the battery capacity returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_boot_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_BOOT_CAPACITY;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

static int cm_get_charge_cycle(struct charger_manager *cm, int *cycle)
{
	struct power_supply *fuel_gauge = NULL;
	union power_supply_propval val;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		ret = -ENODEV;
		return ret;
	}

	*cycle = 0;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	if (ret)
		return ret;

	power_supply_put(fuel_gauge);
	*cycle = val.intval;

	return 0;
}

static int cm_get_bat_aging_id(struct charger_manager *cm, int *aging_bat_id)
{
	int charge_cycle, ret = 0;

	*aging_bat_id = 0;

	if (cm->fake_charge_cycle >= 0) {
		charge_cycle = cm->fake_charge_cycle;
		dev_info(cm->dev, "use fake cycle = %d\n", cm->fake_charge_cycle);
	} else {
		ret = cm_get_charge_cycle(cm, &charge_cycle);
		if (ret) {
			dev_err(cm->dev, "failed to get charge cycle, ret = %d\n", ret);
			return ret;
		}
	}

	*aging_bat_id = sprd_battery_get_aging_bat_id(cm->charger_psy, charge_cycle);
	dev_info(cm->dev, "%s %d, aging_bat_id = %d\n", __func__, __LINE__, *aging_bat_id);

	return ret;
}

static int cm_get_bc1p2_type(struct charger_manager *cm, u32 *type)
{
	int ret = -EINVAL;

	*type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (cm->vchg_info->ops && cm->vchg_info->ops->get_bc1p2_type) {
		*type = cm->vchg_info->ops->get_bc1p2_type(cm->vchg_info);
		ret = 0;
	}

	return ret;
}

static void cm_get_charger_type(struct charger_manager *cm,
				enum cm_charger_type_flag chg_type_flag,
				u32 *type)
{
	struct charger_type *chg_type;
	enum cm_charger_type match_type = CM_CHARGER_TYPE_UNKNOWN;

	switch (chg_type_flag) {
	case CM_FCHG_TYPE:
		chg_type = charger_fchg_type;
		break;
	case CM_WL_TYPE:
		chg_type = charger_wireless_type;
		break;
	case CM_USB_TYPE:
	default:
		chg_type = charger_usb_type;
		break;
	}

	if (!chg_type) {
		dev_err(cm->dev, "%s, chg_type is NULL\n", __func__);
		*type = CM_CHARGER_TYPE_UNKNOWN;
		return;
	}

	while ((chg_type)->adap_type != CM_CHARGER_TYPE_UNKNOWN) {
		if (*type == chg_type->psy_type) {
			match_type = chg_type->adap_type;
			break;
		}

		chg_type++;
	}

	*type = match_type;
}

/**
 * get_usb_charger_type - Get the charger type
 * @cm: the Charger Manager representing the battery.
 * @type: the charger type returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_usb_charger_type(struct charger_manager *cm, u32 *type)
{
	int ret;

	mutex_lock(&cm->desc->charger_type_mtx);
	if (cm->desc->is_fast_charge) {
		mutex_unlock(&cm->desc->charger_type_mtx);
		return 0;
	}

	ret = cm_get_bc1p2_type(cm, type);
	cm_get_charger_type(cm, CM_USB_TYPE, type);

	mutex_unlock(&cm->desc->charger_type_mtx);
	return ret;
}

/**
 * get_wireless_charger_type - Get the wireless_charger type
 * @cm: the Charger Manager representing the battery.
 * @type: the wireless charger type returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_wireless_charger_type(struct charger_manager *cm, u32 *type)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = -EINVAL, i;

	if (!cm->desc->psy_wl_charger_stat)
		return 0;

	mutex_lock(&cm->desc->charger_type_mtx);
	for (i = 0; cm->desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_wl_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TYPE, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*type = val.intval;
			break;
		}
	}

	cm_get_charger_type(cm, CM_WL_TYPE, type);
	mutex_unlock(&cm->desc->charger_type_mtx);

	return ret;
}

/**
 * set_batt_cap - Set the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int set_batt_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "can not find fuel gauge device\n");
		return -ENODEV;
	}

	dev_dbg(cm->dev, "%s:line%d cap = %d\n", __func__, __LINE__, cap);

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to save current battery capacity\n");

	return ret;
}
/**
 * get_charger_voltage - Get the charging voltage from fgu
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input voltage returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_charger_voltage(struct charger_manager *cm, int *vol)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret = -ENODEV;

	if (!is_ext_pwr_online(cm))
		return 0;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "Cannot find power supply  %s\n",
			cm->desc->psy_fuel_gauge);
		return	ret;
	}

	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
	power_supply_put(fuel_gauge);
	if (ret == 0)
		*vol = val.intval;

	return ret;
}

/**
 * adjust_fuel_cap - Adjust the fuel cap level
 * @cm: the Charger Manager representing the battery.
 * @cap: the adjust fuel cap level.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int adjust_fuel_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	dev_dbg(cm->dev, "%s:line%d cap = %d\n", __func__, __LINE__, cap);

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge,
					POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to adjust fuel cap\n");

	return ret;
}

/**
 * get_constant_charge_current - Get the charging current from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_constant_charge_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	*cur = 0;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*cur += val.intval;
		}
	}

	return ret;
}

/**
 * get_input_current_limit - Get the input current limit from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input limit current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_input_current_limit(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	*cur = 0;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
						&val);
		power_supply_put(psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static void cm_set_charger_present(struct charger_manager *cm, bool present)
{
	int ret, i;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = present;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "Fail to set present[%d] of %s, ret = %d\n",
				present, cm->desc->psy_charger_stat[i], ret);
			continue;
		}
	}
}

static void cm_reset_charger_current(struct charger_manager *cm)
{
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				SPRD_VOTE_TYPE_IBUS,
				SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
				SPRD_VOTE_CMD_MIN, PLUGOUT_LIMIT_200MA, cm);

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				SPRD_VOTE_TYPE_IBAT,
				SPRD_VOTE_TYPE_IBAT_ID_CHARGER_TYPE,
				SPRD_VOTE_CMD_MIN, PLUGOUT_CUR_100MA, cm);
}

static void cm_power_path_enable(struct charger_manager *cm, int cmd)
{
	int ret, i;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = cmd;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "Fail to set power_path[%d] of %s, ret = %d\n",
				cmd, cm->desc->psy_charger_stat[i], ret);
		}
	}
}

static bool cm_is_power_path_enabled(struct charger_manager *cm)
{
	int ret, i;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = CM_POWER_PATH_ENABLE_CMD;
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (!ret) {
			if (val.intval) {
				enabled = true;
				break;
			}
		}
	}

	dev_info(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");
	return enabled;
}

/**
 * is_charging - Returns true if the battery is being charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_charging(struct charger_manager *cm)
{
	bool charging = false;
	struct power_supply *psy;
	union power_supply_propval val;
	int i, ret;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	if (!is_ext_pwr_online(cm))
		return charging;

	/* If at least one of the charger is charging, return yes */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		/* 1. The charger sholuld not be DISABLED */
		if (cm->emergency_stop)
			continue;
		if (!cm->charger_enabled)
			continue;

		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		/*
		 * 2. The charger should not be FULL, DISCHARGING,
		 * or NOT_CHARGING.
		 */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS,
				&val);
		power_supply_put(psy);
		if (ret) {
			dev_warn(cm->dev, "Cannot read STATUS value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			continue;
		}
		if (val.intval == POWER_SUPPLY_STATUS_FULL ||
		    val.intval == POWER_SUPPLY_STATUS_DISCHARGING ||
		    val.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
			continue;

		/* Then, this is charging. */
		charging = true;
		break;
	}

	return charging;
}

static int cm_primary_charger_enable(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = 0;

	if (!cm->desc->psy_charger_stat || !cm->desc->psy_charger_stat[0])
		return -ENODEV;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return -ENODEV;
	}

	val.intval = enable;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev, "failed to %s primary charger, ret = %d\n",
			enable ? "enable" : "disable", ret);
		return ret;
	}

	return 0;
}

/**
 * is_full_charged - Returns true if the battery is fully charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_full_charged(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	bool is_full = false;
	int ret = 0;
	int uV, uA;
	int ui_soc = 0;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return false;

	/* Full, if it's over the fullbatt voltage */
	if (desc->fullbatt_uV > 0 && desc->fullbatt_uA > 0) {
		ret = get_vbat_now_uV(cm, &uV);
		if (ret)
			goto out;

		ret = get_ibat_now_uA(cm, &uA);
		if (ret)
			goto out;

		dev_info(cm->dev, "battery vbat=%d ibat=%d\n", uV, uA);
		/* Battery is already full, checks voltage drop. */
		if (cm->battery_status == POWER_SUPPLY_STATUS_FULL && desc->fullbatt_vchkdrop_uV) {
			int batt_ocv, raw_cap;

			if (cm->erp_config) {
				cm_get_uisoc(cm, &ui_soc);
				if (ui_soc >= 95)
					is_full = true;
				else {
					cm->erp_full_flag = 0;
					desc->force_set_full = false;
				}
				goto out;
			}

			ret = get_batt_ocv(cm, &batt_ocv);
			if (ret || batt_ocv < 0)
				goto out;

			dev_info(cm->dev, "battery ocv=%d\n", batt_ocv);
			if ((u32)batt_ocv > (cm->desc->fullbatt_uV - cm->desc->fullbatt_vchkdrop_uV))
				is_full = true;

			ret = get_batt_raw_cap(cm, &raw_cap);
			if (ret) {
				goto out;
			}

			dev_info(cm->dev, "raw_cap=%d\n", raw_cap);
			if (raw_cap <= 97) {
				is_full = false;
				cm->desc->raw_cap = raw_cap;
			}
			goto out;
		}

		if (desc->first_fullbatt_uA > 0 && uV >= desc->fullbatt_uV &&
		    uA > desc->fullbatt_uA && uA <= desc->first_fullbatt_uA && uA >= 0) {
			if (++desc->first_trigger_cnt > 1)
				cm->desc->force_set_full = true;
		} else {
			desc->first_trigger_cnt = 0;
		}

		if (uV >= desc->fullbatt_uV && uA <= desc->fullbatt_uA && uA >= 0) {
			if (++desc->trigger_cnt >= 4) {
				if (cm->desc->cap >= CM_CAP_FULL_PERCENT) {
					if (desc->trigger_cnt == 4)
						adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
					is_full = true;
				} else {
					if (cm->erp_config && cm->desc->cap > 993) {
						is_full = true;
					} else {
						is_full = false;
						adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
						if (desc->trigger_cnt == 4)
							cm_primary_charger_enable(cm, false);
					}
				}
				cm->desc->force_set_full = true;
			} else {
				is_full = false;
			}
			goto out;
		} else {
			is_full = false;
			desc->trigger_cnt = 0;
			goto out;
		}
	}

	/* Full, if the capacity is more than fullbatt_soc */
	if (desc->fullbatt_soc > 0) {
		val.intval = 0;

		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
		if (!ret && val.intval >= desc->fullbatt_soc) {
			is_full = true;
			goto out;
		}
	}

out:
	if (cm->erp_config ) {
		cm_get_uisoc(cm, &ui_soc);
		if (cm->uisoc_100_adapter_plugin && ui_soc == 100)
			is_full = true;
	}
	dev_info(cm->dev, "battery is_full=%d, uisoc_100_plugin=%d\n", is_full, cm->uisoc_100_adapter_plugin);
	power_supply_put(fuel_gauge);
	return is_full;
}

/**
 * is_polling_required - Return true if need to continue polling for this CM.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_polling_required(struct charger_manager *cm)
{
	switch (cm->desc->polling_mode) {
	case CM_POLL_DISABLE:
		return false;
	case CM_POLL_ALWAYS:
		return true;
	case CM_POLL_EXTERNAL_POWER_ONLY:
		return is_ext_pwr_online(cm);
	case CM_POLL_CHARGING_ONLY:
		return is_charging(cm);
	default:
		dev_warn(cm->dev, "Incorrect polling_mode (%d)\n",
			 cm->desc->polling_mode);
	}

	return false;
}

static bool cm_update_current_jeita_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct cm_jeita_info *jeita_info = &cm->desc->jeita_info;
	int cur_jeita_status, ret;
	bool is_normal = true;

	/**
	 * Note that it need to vote for ibat before the caller of this function
	 * if does not define jeita table
	 */
	if (!desc->jeita_tab_size)
		return true;

	if (unlikely(desc->jeita_disabled)) {
		cur_jeita_status = desc->force_jeita_status;
	} else {
		ret = cm_get_battery_temperature(cm, &desc->temperature);
		if (ret) {
			dev_err(cm->dev, "failed to get battery temperature\n");
			return false;
		}

		cur_jeita_status = cm_manager_get_jeita_status(cm, desc->temperature);
	}

	if (jeita_info->jeita_changed) {
		if (desc->jeita_disabled)
			dev_info(cm->dev, "current-last jeita status: Disable jeita and force jeita_status: %d-%d, temperature: %d\n",
				 cur_jeita_status, desc->force_jeita_status, desc->temperature);
		else
			dev_info(cm->dev, "current-last jeita status: %s %d-%d, temperature: %d\n",
				 __func__, cur_jeita_status, jeita_info->jeita_status,
				 desc->temperature);
		jeita_info->jeita_status = cur_jeita_status;
		jeita_info->jeita_temperature = desc->temperature;
		is_normal = cm_manager_adjust_current(cm, jeita_info->jeita_status);
		jeita_info->jeita_changed = false;
		return is_normal;
	}

	if (cur_jeita_status > jeita_info->jeita_status) {
		jeita_info->temp_down_trigger = 0;

		if (++jeita_info->temp_up_trigger > 1) {
			is_normal = cm_manager_adjust_current(cm, cur_jeita_status);
			dev_info(cm->dev, "current-last jeita status: %s %d-%d, temperature: %d\n",
				 __func__, cur_jeita_status, jeita_info->jeita_status,
				 desc->temperature);
			jeita_info->jeita_status = cur_jeita_status;
			jeita_info->jeita_temperature = desc->temperature;
			jeita_info->temp_up_trigger = 0;
		}
	} else if (cur_jeita_status < jeita_info->jeita_status) {
		jeita_info->temp_up_trigger = 0;

		if (++jeita_info->temp_down_trigger > 1) {
			is_normal = cm_manager_adjust_current(cm, cur_jeita_status);
			dev_info(cm->dev, "current-last jeita status: %s %d-%d, temperature: %d\n",
				 __func__, cur_jeita_status, jeita_info->jeita_status,
				 desc->temperature);
			jeita_info->jeita_status = cur_jeita_status;
			jeita_info->jeita_temperature = desc->temperature;
			jeita_info->temp_down_trigger = 0;
		}
	} else {
		jeita_info->temp_up_trigger = 0;
		jeita_info->temp_down_trigger = 0;
	}

	return is_normal;
}
#define RP_1_5_LIMINT 1500000
#define RP_3_0_LIMINT 3000000
#define RP_DEF_LIMINT 100000
static void cm_update_charge_info(struct charger_manager *cm, int cmd)
{
	struct charger_desc *desc = cm->desc;
	struct cm_thermal_info *thm_info = &cm->desc->thm_info;
	u32 last_jeita_tab_size;
	int ret;
	u32 type;
	static int previous_quick_charge_type = -1;

	mutex_lock(&cm->desc->charge_info_mtx);

	last_jeita_tab_size = desc->jeita_tab_size;

	switch (desc->charger_type) {
	case CM_CHARGER_TYPE_DCP:
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
			desc->force_jeita_status =
				desc->max_current_jeita_index[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;

		break;
	case CM_CHARGER_TYPE_SDP:
		desc->charge_limit_cur = desc->cur.sdp_cur;
		if (cm->vchg_info->usb_limit == 0 &&
		    (cm->desc->limit_status & CM_CHARGE_PD_LIMIT_CMD)) {
			desc->input_limit_cur = desc->cur.sdp_limit;
		} else if (cm->vchg_info->usb_limit == -EINVAL) {
			desc->input_limit_cur = desc->cur.sdp_limit;
		} else {
			desc->input_limit_cur = min(desc->cur.sdp_limit, cm->vchg_info->usb_limit);
			if (cm->vchg_info->usb_limit != 2000 &&
			    desc->input_limit_cur == cm->vchg_info->usb_limit &&
			    cm->vchg_info->usb_limit < min(CM_SDP_TYPE_USB_ENUM_LIMIT_CUR_UA,
							   desc->cur.sdp_limit)) {
				cm->desc->limit_status &= ~CM_CHARGE_USB_LIMIT_CMD;
				schedule_delayed_work(&cm->limit_current_work,
						      msecs_to_jiffies(2000));
			}
		}

		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_SDP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_SDP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_SDP];
			desc->force_jeita_status =
				desc->max_current_jeita_index[SPRD_BATTERY_JEITA_SDP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_CHARGER_TYPE_CDP:
		desc->charge_limit_cur = desc->cur.cdp_cur;
		desc->input_limit_cur = desc->cur.cdp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_CDP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_CDP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_CDP];
			desc->force_jeita_status =
				desc->max_current_jeita_index[SPRD_BATTERY_JEITA_CDP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_CHARGER_TYPE_FAST:
		if (desc->enable_fast_charge) {
			desc->charge_limit_cur = desc->cur.fchg_cur;
			desc->input_limit_cur = desc->cur.fchg_limit;
			thm_info->adapter_default_charge_vol = 9;
			if (desc->jeita_size[SPRD_BATTERY_JEITA_FCHG]) {
				desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_FCHG];
				desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_FCHG];
				desc->force_jeita_status =
					desc->max_current_jeita_index[SPRD_BATTERY_JEITA_FCHG];
			}
			if (desc->fast_charge_voltage_max)
				desc->charge_voltage_max = desc->fast_charge_voltage_max;
			if (desc->fast_charge_voltage_drop)
				desc->charge_voltage_drop = desc->fast_charge_voltage_drop;
			break;
		}

		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;

		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
			desc->force_jeita_status =
				desc->max_current_jeita_index[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_CHARGER_TYPE_ADAPTIVE:
		if (desc->cp_sm.running && !desc->cp_sm.recovery) {
			desc->charge_limit_cur = desc->cur.flash_cur;
			desc->input_limit_cur = desc->cur.flash_limit;
			thm_info->adapter_default_charge_vol = 11;
			if (desc->jeita_size[SPRD_BATTERY_JEITA_FLASH]) {
				desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_FLASH];
				desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_FLASH];
				desc->force_jeita_status =
					desc->max_current_jeita_index[SPRD_BATTERY_JEITA_FLASH];
			}
			if (desc->flash_charge_voltage_max)
				desc->charge_voltage_max = desc->flash_charge_voltage_max;
			if (desc->flash_charge_voltage_drop)
				desc->charge_voltage_drop = desc->flash_charge_voltage_drop;
			break;
		}
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
			desc->force_jeita_status =
				desc->max_current_jeita_index[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_WIRELESS_CHARGER_TYPE_BPP:
		desc->charge_limit_cur = desc->cur.wl_bpp_cur;
		desc->input_limit_cur = desc->cur.wl_bpp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_WL_BPP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_WL_BPP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_WL_BPP];
			desc->force_jeita_status =
				desc->max_current_jeita_index[SPRD_BATTERY_JEITA_WL_BPP];
		}
		if (desc->wireless_normal_charge_voltage_max)
			desc->charge_voltage_max = desc->wireless_normal_charge_voltage_max;
		if (desc->wireless_normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->wireless_normal_charge_voltage_drop;
		break;
	case CM_WIRELESS_CHARGER_TYPE_EPP:
		desc->charge_limit_cur = desc->cur.wl_epp_cur;
		desc->input_limit_cur = desc->cur.wl_epp_limit;
		thm_info->adapter_default_charge_vol = 11;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_WL_EPP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_WL_EPP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_WL_EPP];
			desc->force_jeita_status =
				desc->max_current_jeita_index[SPRD_BATTERY_JEITA_WL_EPP];
		}
		if (desc->wireless_fast_charge_voltage_max)
			desc->charge_voltage_max = desc->wireless_fast_charge_voltage_max;
		if (desc->wireless_fast_charge_voltage_drop)
			desc->charge_voltage_drop = desc->wireless_fast_charge_voltage_drop;
		break;
	default:
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;

		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_UNKNOWN];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN];
			desc->force_jeita_status =
				desc->max_current_jeita_index[SPRD_BATTERY_JEITA_UNKNOWN];
		}

		if (!is_ext_pwr_online(cm)) {
			desc->charge_limit_cur = 0;
			desc->input_limit_cur = 0;
			break;
		}

		desc->charge_limit_cur = desc->cur.unknown_cur;
		desc->input_limit_cur = desc->cur.unknown_limit;

		break;
	}

	switch (desc->charger_type) {
	case CM_CHARGER_TYPE_DCP:
	case CM_CHARGER_TYPE_SDP:
	case CM_CHARGER_TYPE_CDP:
	case CM_CHARGER_TYPE_UNKNOWN:
		if (desc->rp_limit_current == RP_1_5_LIMINT ||
		    desc->rp_limit_current == RP_3_0_LIMINT ||
		    (desc->rp_limit_current == RP_DEF_LIMINT &&
		     cm->vchg_info->usb_limit == -EINVAL))
			desc->input_limit_cur = min(desc->rp_limit_current, desc->input_limit_cur);

		if ((cm->desc->limit_status & CM_CHARGE_PD_LIMIT_CMD) &&
		    cm->desc->pd_req_vol_uv < CM_FIXED_FCHG_VOLTAGE_5V_THRESHOLD) {
			desc->input_limit_cur = cm->desc->pd_req_cur_ua;
			if (cm->vchg_info->usb_limit == 2000 &&
			    desc->charger_type == CM_CHARGER_TYPE_SDP)
				desc->input_limit_cur = min(desc->input_limit_cur,
							    cm->vchg_info->usb_limit);
		}
		break;
	case CM_CHARGER_TYPE_FAST:
		if (!cm_pd_is_ac_online(cm)) {
			desc->charge_limit_cur = CM_FIXED_FCHG_C2C_CURRENT;
			desc->input_limit_cur = CM_FIXED_FCHG_C2C_CURRENT;
		}

		if ((cm->desc->limit_status & CM_CHARGE_PD_LIMIT_CMD) &&
		    cm->desc->pd_req_vol_uv < CM_FIXED_FCHG_VOLTAGE_9V_THRESHOLD) {
			desc->input_limit_cur = cm->desc->pd_req_cur_ua;
			ret = cm_get_bc1p2_type(cm, &type);
			if (!ret && type == POWER_SUPPLY_USB_TYPE_SDP &&
			    cm->vchg_info->usb_limit == 2000)
				desc->input_limit_cur = min(desc->input_limit_cur,
							    cm->vchg_info->usb_limit);
		}
		break;
	}

	if (desc->jeita_tab_size && desc->jeita_tab_size != last_jeita_tab_size)
		desc->jeita_info.jeita_changed = true;

	mutex_unlock(&cm->desc->charge_info_mtx);

	if (thm_info->thm_pwr && thm_info->adapter_default_charge_vol)
		thm_info->thm_adjust_cur = (int)(thm_info->thm_pwr /
			thm_info->adapter_default_charge_vol) * 1000;

	if (cm->mtbf_current != 0) {
		if (desc->input_limit_cur / 1000 < MTBF_CURRENT_DEFAULT)
			cm->mtbf_current = MTBF_CURRENT_DEFAULT;
		desc->charge_limit_cur = cm->mtbf_current * 1000;
		desc->input_limit_cur = cm->mtbf_current * 1000;
	}

	if (!cm->bat_id) {
		dev_info(cm->dev,"origin max chg_lmt_cur= %duA, max inpt_lmt_cur= %duA\n", desc->charge_limit_cur, desc->input_limit_cur);
		if (desc->input_limit_cur > NONSTAND_BATT_CURR * 1000) {
			if (!cm->cm_charge_vote) {
				dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
				return;
			}
			if (cm->cm_charge_vote->ibus_client[SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE].value == NONSTAND_BATT_CURR * 1000) {
				desc->charge_limit_cur = (NONSTAND_BATT_CURR + 1) * 1000;
				desc->input_limit_cur = (NONSTAND_BATT_CURR + 1) * 1000;
			} else {
				desc->charge_limit_cur = NONSTAND_BATT_CURR * 1000;
				desc->input_limit_cur = NONSTAND_BATT_CURR * 1000;
			}
		}
	}

	dev_info(cm->dev, "%s, chgr type= %d, fchg_en= %d, cp_running= %d, cp_recovery= %d, max chg_lmt_cur= %duA, max inpt_lmt_cur= %duA, max chg_volt= %duV, chg_volt_drop= %d, adapter_chg_volt= %dmV, thm_cur= %d, chg_info_cmd= 0x%x, jeita_size= %d, jeita_changed= %d, force_jeita_status= %d\n",
		 __func__, desc->charger_type, desc->enable_fast_charge, desc->cp_sm.running,
		 desc->cp_sm.recovery, desc->charge_limit_cur, desc->input_limit_cur,
		 desc->charge_voltage_max, desc->charge_voltage_drop,
		 thm_info->adapter_default_charge_vol * 1000, thm_info->thm_adjust_cur, cmd,
		 desc->jeita_tab_size, desc->jeita_info.jeita_changed,
		 desc->force_jeita_status);

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return;
	}

	if (cmd & CM_CHARGE_INFO_CHARGE_LIMIT)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, desc->charge_limit_cur, cm);

	if (cmd & CM_CHARGE_INFO_INPUT_LIMIT)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, desc->input_limit_cur, cm);
	if (cmd & CM_CHARGE_INFO_THERMAL_LIMIT && cm->desc->thermal_limit_ibat > 0) {
		/* The ChargerIC with linear charging cannot set Ibus, only Ibat. */
		if (cm->desc->thm_info.need_calib_charge_lmt)
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CHARGE_CONTROL_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 cm->desc->thermal_limit_ibat, cm);
		else
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
						 SPRD_VOTE_TYPE_IBAT,
						 SPRD_VOTE_TYPE_IBAT_ID_CHARGE_CONTROL_LIMIT,
						 SPRD_VOTE_CMD_MIN,
						 cm->desc->thermal_limit_ibat, cm);
	}

	if (cmd & CM_CHARGE_INFO_JEITA_LIMIT) {
		desc->jeita_info.jeita_changed = true;
		cm_update_current_jeita_status(cm);
		if (cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD))
			mod_delayed_work(cm_wq, &cm_monitor_work, 0);
	}

	if(desc->charger_type - previous_quick_charge_type != 0) {
		// Do not schedule again when the work is running!
		if (!delayed_work_pending(&cm->quick_charge_type_update_work))
		{
			schedule_delayed_work(&cm->quick_charge_type_update_work, 0);
		}
	}
	previous_quick_charge_type = desc->charger_type;
}

static void cm_vote_property(struct charger_manager *cm, int target_val,
			     const char **name, enum power_supply_property psp)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret;

	if (!name) {
		dev_err(cm->dev, "psy name is null!!!\n");
		return;
	}

	for (i = 0; name[i]; i++) {
		psy = power_supply_get_by_name(name[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n", name[i]);
			continue;
		}

		val.intval = target_val;
		ret = power_supply_set_property(psy, psp, &val);
		power_supply_put(psy);
		if (ret)
			dev_err(cm->dev, "failed to %s set power_supply_property[%d], ret = %d\n",
				name[i], psp, ret);
	}
}

static int cm_check_parallel_charger(struct charger_manager *cm, int cur)
{
	if (cm->desc->enable_fast_charge && cm->desc->psy_charger_stat[1])
		cur /= 2;

	return cur;
}

static void cm_sprd_vote_callback(struct sprd_vote *vote_gov, int vote_type,
				  int value, void *data)
{
	struct charger_manager *cm = (struct charger_manager *)data;
	const char **psy_charger_name;

	dev_info(cm->dev, "%s, %s[%d], vote_type:%d\n", __func__, vote_type_names[vote_type], value, vote_type);
	switch (vote_type) {
	case SPRD_VOTE_TYPE_IBAT:
		psy_charger_name = cm->desc->psy_charger_stat;
		value = cm_check_parallel_charger(cm, value);
		dev_info(cm->dev, "func:%s, line:%d, %s[%d], vote_type:%d\n", __func__, __LINE__, vote_type_names[vote_type], value, vote_type);
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT);
		break;
	case SPRD_VOTE_TYPE_IBUS:
		psy_charger_name = cm->desc->psy_charger_stat;
		value = cm_check_parallel_charger(cm, value);
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
		break;
	case SPRD_VOTE_TYPE_CCCV:
		psy_charger_name = cm->desc->psy_charger_stat;
		if (cm->desc->cp_sm.running)
			psy_charger_name = cm->desc->psy_cp_stat;
#if IS_ENABLED(CONFIG_XM_SMART_CHG)
		pr_err("%s, value = %d, soft_value = %d, smart_fv = %d\n", __func__, value, cm->desc->origin_fullbatt_uV, cm->smart_fv);
		value -= cm->smart_fv * 1000;
		if(cm->desc->origin_fullbatt_uV > cm->smart_fv) {
			cm->desc->fullbatt_uV = cm->desc->origin_fullbatt_uV - cm->smart_fv * 1000;
			if (cm->desc->charger_type == CM_CHARGER_TYPE_SDP && r_items_param != r_items_param_single) {
				cm->desc->fullbatt_uV -= 20000;
				pr_info("sdp type, set fullbatt_uV to %duV, cccv to %duV\n", cm->desc->fullbatt_uV, value);
			}
		}
#else
		cm->desc->fullbatt_uV = cm->desc->origin_fullbatt_uV ;
		if (cm->desc->charger_type == CM_CHARGER_TYPE_SDP && r_items_param != r_items_param_single) {
			cm->desc->fullbatt_uV -= 20000;
			pr_info("sdp type, set fullbatt_uV to %duV, cccv to %duV\n", cm->desc->fullbatt_uV, value);
		}
#endif
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX);
		break;
	default:
		dev_err(cm->dev, "vote_gov: vote_type[%d] error!!!\n", vote_type);
		break;
	}
}

static int cm_get_fchg_adapter_max_voltage(struct charger_manager *cm, int *max_vol)
{
	int ret;

	*max_vol = 0;
	if (!cm->fchg_info->ops || !cm->fchg_info->ops->get_fchg_vol_max) {
		dev_err(cm->dev, "%s, fchg ops or get_fchg_vol_max is null\n", __func__);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->get_fchg_vol_max(cm->fchg_info, max_vol);
	if (ret)
		dev_err(cm->dev, "%s, failed to get fchg max voltage, ret=%d\n",
			__func__, ret);

	return ret;
}

static int cm_get_fchg_adapter_max_current(struct charger_manager *cm, int input_vol, int *max_cur)
{
	int ret;

	*max_cur = 0;
	if (!cm->fchg_info->ops || !cm->fchg_info->ops->get_fchg_cur_max) {
		dev_err(cm->dev, "%s, fchg ops or get_fchg_cur_max is null\n", __func__);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->get_fchg_cur_max(cm->fchg_info, input_vol, max_cur);
	if (ret)
		dev_err(cm->dev, "%s, failed to get fchg max current, ret=%d\n",
			__func__, ret);

	return ret;
}

static int cm_set_charger_ovp(struct charger_manager *cm, int cmd)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, i;

	if (!desc->psy_charger_stat) {
		dev_err(cm->dev, "psy_charger_stat is null!!!\n");
		return -ENODEV;
	}

	/*
	 * make the psy_charger_stat[0] to be main charger,
	 * set the main charger charge current and limit current
	 * in 9V/5V fast charge status.
	 */
	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			return -ENODEV;
		}

		val.intval = cmd;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "failed to set \"%s\" ovp cmd = %d, ret = %d\n",
				desc->psy_charger_stat[i], cmd, ret);
			return ret;
		}
	}

	return 0;
}

static int cm_enable_second_charger(struct charger_manager *cm, bool enable)
{

	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!desc->psy_charger_stat[1])
		return 0;

	psy = power_supply_get_by_name(desc->psy_charger_stat[1]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_charger_stat[1]);
		return -ENODEV;
	}

	/*
	 * enable/disable the second charger to start/stop charge
	 */
	val.intval = enable;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to %s second charger \n", enable ? "enable" : "disable");
		return ret;
	}

	return 0;
}

static int cm_adjust_fchg_voltage(struct charger_manager *cm, int vol)
{
	int ret;

	if (!cm->fchg_info->ops || !cm->fchg_info->ops->adj_fchg_vol) {
		dev_err(cm->dev, "%s, fchg ops or adj_fchg_vol is null\n", __func__);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->adj_fchg_vol(cm->fchg_info, vol);
	if (ret)
		dev_err(cm->dev, "%s, failed to adjust fchg voltage vol=%d, ret=%d\n",
			__func__, vol, ret);

	return ret;
}

static bool cm_is_reach_fchg_threshold(struct charger_manager *cm)
{
	int ret, adapter_max_vbus, batt_ocv, batt_uA, batt_soc, fchg_ocv_threshold, thm_cur;
	int cur_jeita_status, target_cur;

	/*
	 * Eg: Failure to obtain the voltage of the charging device for
	 *     the first time when the charging type changes.
	 */
	if (cm->desc->adapter_max_vbus == 0 ||
	    cm->desc->fchg_voltage_check_count < CM_FAST_CHARGE_VOLTAGE_CHECK_COUNT) {
		ret = cm_get_fchg_adapter_max_voltage(cm, &adapter_max_vbus);
		if (ret) {
			dev_err(cm->dev, "%s, failed to obtain the adapter max voltage, ret=%d\n",
				__func__, ret);
			return false;
		}

		cm->desc->adapter_max_vbus = adapter_max_vbus;
	}

	if (cm->desc->adapter_max_vbus <= CM_PPS_5V_PROG_MAX) {
		if (++cm->desc->fchg_voltage_check_count > CM_FAST_CHARGE_VOLTAGE_CHECK_COUNT)
			cm->desc->fchg_voltage_check_count = CM_FAST_CHARGE_VOLTAGE_CHECK_COUNT;
		else
			dev_info(cm->dev, "%s, adapter max vol %duV lower than th %duV, cnt: %d\n",
				 __func__, cm->desc->adapter_max_vbus, CM_PPS_5V_PROG_MAX,
				 cm->desc->fchg_voltage_check_count);

		return false;
	}

	cm->desc->fchg_voltage_check_count = 0;
	if (get_batt_ocv(cm, &batt_ocv)) {
		dev_err(cm->dev, "get_batt_ocv error.\n");
		return false;
	}

	if (get_ibat_now_uA(cm, &batt_uA)) {
		dev_err(cm->dev, "get_ibat_now_uA error.\n");
		return false;
	}

	if (get_batt_cap(cm, &batt_soc)) {
		dev_err(cm->dev, "get_batt_cap error.\n");
		return false;
	}

	target_cur = batt_uA;
	if (cm->desc->jeita_tab_size) {
		cur_jeita_status = cm_manager_get_jeita_status(cm, cm->desc->temperature);
		if (cm->desc->jeita_disabled)
			cur_jeita_status = cm->desc->force_jeita_status;

		target_cur = 0;
		if (cur_jeita_status != cm->desc->jeita_tab_size)
			target_cur = cm->desc->jeita_tab[cur_jeita_status].current_ua;
	}

	fchg_ocv_threshold = CM_FAST_CHARGE_START_VOLTAGE_HTHRESHOLD;
	if (cm->desc->fchg_ocv_threshold > 0)
		fchg_ocv_threshold = cm->desc->fchg_ocv_threshold;

	thm_cur = CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT;
	if (cm->desc->thm_info.thm_adjust_cur > 0)
		thm_cur = cm->desc->thm_info.thm_adjust_cur;

	if (target_cur >= CM_FAST_CHARGE_ENABLE_CURRENT &&
		thm_cur >= CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT &&
		batt_ocv >= CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD &&
		batt_ocv < fchg_ocv_threshold &&
		batt_soc < CM_FAST_CHARGE_START_SOC_HTHRESHOLD)
		return true;
	else if (batt_ocv >= CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD &&
		batt_uA >= CM_FAST_CHARGE_ENABLE_CURRENT &&
		batt_soc < CM_FAST_CHARGE_START_SOC_HTHRESHOLD)
		return true;

	return false;
}

static int cm_fixed_fchg_enable(struct charger_manager *cm)
{
	int ret, adapter_max_vbus;

	/*
	 * if it occurs emergency event, don't enable fast charge.
	 */
	if (cm->emergency_stop)
		return -EAGAIN;

	if (!cm->desc) {
		dev_err(cm->dev, "cm->desc is a null pointer!!!\n");
		return 0;
	}

	/*
	 * if it don't define sprd,support-fchg in dts,
	 * we think that it don't plan to use fast charge.
	 */
	if (!cm->fchg_info->support_fchg)
		return 0;

	if (!cm->desc->is_fast_charge || cm->desc->enable_fast_charge)
		return 0;

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return 0;
	}

	/*
	 * cm->desc->enable_fast_charge should be set to true when the transient
	 * current is voting, otherwise the current of the parallel charging
	 * scheme cannot be halved.
	 *
	 * In the normal fast charge voltage regulation process, add the normal
	 * fast charge transition current to prevent overload and other abnormal
	 * situations.
	 */
	cm->desc->enable_fast_charge = true;
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FIXED_FCHG_TRANSITION_CURRENT_1P5A, cm);

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	/*
	 * adjust over voltage protection in 9V
	 */
	ret = cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_ENABLE_CMD);
	if (ret) {
		dev_err(cm->dev, "failed to enable fchg ovp\n");
		/*
		 * if it failed to set fast charge ovp, reset to DCP setting
		 * first so that the charging ovp can reach the condition again.
		 */
		goto tran_cur_err;
	}

	/*
	 * adjust fast charger output voltage from 5V to 9V
	 */
	ret = cm_get_fchg_adapter_max_voltage(cm, &adapter_max_vbus);
	if (ret) {
		dev_err(cm->dev, "failed to obtain the adapter max voltage\n");
		goto ovp_err;
	}

	if (adapter_max_vbus > CM_FAST_CHARGE_VOLTAGE_9V)
		adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_9V;

	ret = cm_adjust_fchg_voltage(cm, adapter_max_vbus);
	if (ret) {
		dev_err(cm->dev, "failed to adjust fast charger voltage\n");
		goto ovp_err;
	}

	ret = cm_enable_second_charger(cm, true);
	if (ret) {
		dev_err(cm->dev, "failed to enable second charger\n");
		goto adj_vol_err;
	}

	goto out;

adj_vol_err:
	cm_adjust_fchg_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V);

ovp_err:
	cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_DISABLE_CMD);

tran_cur_err:
	cm->desc->enable_fast_charge = false;
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

out:
	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FIXED_FCHG_TRANSITION_CURRENT_1P5A, cm);
	return ret;
}

static int cm_fixed_fchg_disable(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int ret, charge_vol;

	if (!desc->enable_fast_charge)
		return 0;

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return 0;
	}

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FIXED_FCHG_TRANSITION_CURRENT_1P5A, cm);

	/*
	 * If defined psy_charger_stat[1], then disable the second
	 * charger first.
	 */
	ret = cm_enable_second_charger(cm, false);
	if (ret) {
		dev_err(cm->dev, "failed to disable second charger\n");
		goto out;
	}

	/*
	 * Adjust fast charger output voltage from 9V to 5V
	 */
	if (!desc->wait_vbus_stable &&
	    cm_adjust_fchg_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V)) {
		dev_err(cm->dev, "%s, failed to adjust 5V fast charger voltage\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Waiting for the charger to step down to prevent the occurrence
	 * of Vbus overvoltage.
	 * Reason: It takes a certain time for the charger to switch from
	 *         9V to 5V. At this time, if the OVP is directly set to
	 *         6.5V, there is a small probability that Vbus overvoltage
	 *         will occur.
	 */
	ret = get_charger_voltage(cm, &charge_vol);
	if (ret) {
		dev_err(cm->dev, "%s, fail to get charge vol\n", __func__);
		goto out;
	}

	if (charge_vol > desc->normal_charge_voltage_max) {
		dev_err(cm->dev, "%s, waiting for the charger to step down\n", __func__);
		desc->wait_vbus_stable = true;
		ret = -EINVAL;
		goto out;
	}

	desc->wait_vbus_stable = false;

	ret = cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_DISABLE_CMD);
	if (ret) {
		dev_err(cm->dev, "%s, failed to disable fchg ovp\n", __func__);
		goto out;
	}

	desc->enable_fast_charge = false;
	desc->fast_charge_disable_count = 0;
	/*
	 * Adjust over voltage protection in 5V
	 */
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

out:
	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FIXED_FCHG_TRANSITION_CURRENT_1P5A, cm);
	return ret;
}

static bool cm_is_disable_fixed_fchg_check(struct charger_manager *cm, int *delay_work_ms)
{
	int batt_uV, batt_uA, ret, chg_vol = 0;

	*delay_work_ms = cm->desc->polling_interval_ms;
	if (!cm->desc->enable_fast_charge)
		return true;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get batt uV, ret=%d\n", __func__, ret);
		return false;
	}

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get batt uA, ret=%d\n", __func__, ret);
		return false;
	}

	ret = get_charger_voltage(cm, &chg_vol);
	if (ret)
		dev_err(cm->dev, "%s, get chg_vol error, ret=%d\n", __func__, ret);

	if (batt_uV < CM_FIXED_FCHG_DISABLE_BATTERY_VOLTAGE ||
	    batt_uA < CM_FIXED_FCHG_DISABLE_CURRENT ||
	    (!ret && chg_vol < CM_FIXED_FCHG_VOLTAGE_5V_THRESHOLD)) {
		cm->desc->fast_charge_disable_count++;
		*delay_work_ms = CM_FIXED_FCHG_CHK_DIS_WORK_MS;
		pm_wakeup_event(cm->dev, CM_FIXED_FCHG_CHK_DIS_WORK_MS + 500);
	} else {
		cm->desc->fast_charge_disable_count = 0;
	}

	if (cm->desc->fast_charge_disable_count < CM_FIXED_FCHG_DISABLE_COUNT)
		return false;

	dev_info(cm->dev, "%s, vbat: %d, ibat: %d, vbus: %d, exit fixed fchg\n",
		 __func__, batt_uV, batt_uA, chg_vol);
	return true;
}

static int cm_get_ibat_avg(struct charger_manager *cm, int *ibat)
{
	int ret, batt_uA, min, max, i, sum = 0;
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "get bat_uA error.\n");
		return ret;
	}

	if (ir_sts->ibat_index >= CM_IBAT_BUFF_CNT)
		ir_sts->ibat_index = 0;
	ir_sts->ibat_buf[ir_sts->ibat_index++] = batt_uA;

	if (ir_sts->ibat_buf[CM_IBAT_BUFF_CNT - 1] == CM_MAGIC_NUM)
		return -EINVAL;

	min = max = ir_sts->ibat_buf[0];
	for (i = 0; i < CM_IBAT_BUFF_CNT; i++) {
		if (max < ir_sts->ibat_buf[i])
			max = ir_sts->ibat_buf[i];
		if (min > ir_sts->ibat_buf[i])
			min = ir_sts->ibat_buf[i];
		sum += ir_sts->ibat_buf[i];
	}

	sum  = sum - min - max;

	*ibat = DIV_ROUND_CLOSEST(sum, (CM_IBAT_BUFF_CNT - 2));

	if (*ibat < 0)
		*ibat = 0;

	return ret;
}

static void cm_ir_compensation_init(struct charger_manager *cm)
{
	cm->desc->ir_comp.ibat_buf[CM_IBAT_BUFF_CNT - 1] = CM_MAGIC_NUM;
	cm->desc->ir_comp.ibat_index = 0;
	cm->desc->ir_comp.last_target_cccv = 0;
	if (cm->cm_charge_vote)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
					 SPRD_VOTE_TYPE_CCCV,
					 SPRD_VOTE_TYPE_CCCV_ID_IR,
					 SPRD_VOTE_CMD_MIN,
					 0, cm);
}

static void cm_ir_compensation_enable(struct charger_manager *cm, bool enable)
{
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;

	cm_ir_compensation_init(cm);

	if (enable) {
		if (ir_sts->rc && !ir_sts->ir_compensation_en) {
			dev_info(cm->dev, "%s enable ir compensation\n", __func__);
			ir_sts->ir_compensation_en = true;
			queue_delayed_work(system_power_efficient_wq,
					   &cm->ir_compensation_work,
					   CM_IR_COMPENSATION_TIME * HZ);
		}
		ir_sts->ir_compensation_en = true;
	} else {
		if (ir_sts->ir_compensation_en) {
			dev_info(cm->dev, "%s stop ir compensation\n", __func__);
			cancel_delayed_work_sync(&cm->ir_compensation_work);
			ir_sts->ir_compensation_en = false;
		}
	}
}

static void cm_ir_compensation(struct charger_manager *cm, enum cm_ir_comp_state state, int *target)
{
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;
	int ibat_avg, target_cccv;

	if (!ir_sts->rc)
		return;

	if (cm_get_ibat_avg(cm, &ibat_avg))
		return;

	if (state == CM_IR_COMP_STATE_CP && cm->desc->psy_cp_stat && cm->desc->psy_cp_stat[1])
		ibat_avg /= 2;

	ir_sts->ir_drop = (ibat_avg / 1000) * ir_sts->rc;
	target_cccv = ir_sts->us + ir_sts->ir_drop;

	if (target_cccv < ir_sts->us_lower_limit)
		target_cccv = ir_sts->us_lower_limit;
	else if (target_cccv > ir_sts->us_upper_limit)
		target_cccv = ir_sts->us_upper_limit;

	*target = target_cccv;

	if ((*target / 1000) == (ir_sts->last_target_cccv / 1000))
		return;

	dev_info(cm->dev, "%s, us = %d, rc = %d, upper_limit = %d, lower_limit = %d, "
		 "target_cccv = %d, ibat_avg = %d, offset = %d\n",
		 __func__, ir_sts->us, ir_sts->rc, ir_sts->us_upper_limit,
		 ir_sts->us_lower_limit, target_cccv, ibat_avg,
		 ir_sts->cp_upper_limit_offset);

	ir_sts->last_target_cccv = *target;
	switch (state) {
	case CM_IR_COMP_STATE_CP:
		target_cccv = min(ir_sts->us_upper_limit,
				  (*target + ir_sts->cp_upper_limit_offset));
		fallthrough;
	case CM_IR_COMP_STATE_NORMAL:
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					SPRD_VOTE_TYPE_CCCV,
					SPRD_VOTE_TYPE_CCCV_ID_IR,
					SPRD_VOTE_CMD_MAX,
					target_cccv, cm);
		break;
	default:
		break;
	}
}

static void cm_ir_compensation_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  ir_compensation_work);

	int target_cccv;

	cm_ir_compensation(cm, CM_IR_COMP_STATE_NORMAL, &target_cccv);
	queue_delayed_work(system_power_efficient_wq,
			   &cm->ir_compensation_work,
			   CM_IR_COMPENSATION_TIME * HZ);
}

static void cm_fixed_fchg_control_switch(struct charger_manager *cm, bool enable)
{
	dev_dbg(cm->dev, "%s enable = %d start\n", __func__, enable);

	if (!cm->fchg_info->support_fchg)
		return;

	cm->desc->check_fixed_fchg_threshold = enable;
	if (!enable && cm->desc->fixed_fchg_running) {
		cancel_delayed_work_sync(&cm->fixed_fchg_work);
		schedule_delayed_work(&cm->fixed_fchg_work, 0);
	}
}

static bool cm_is_need_start_fixed_fchg(struct charger_manager *cm)
{
	bool need = false;

	if (!cm->desc->support_fixed_fchg || !cm->fchg_info->support_fchg
	    || cm->desc->fixed_fchg_running)
		return false;

	cm_charger_is_support_fchg(cm);
	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_FAST &&
	    cm->charger_enabled && cm->desc->check_fixed_fchg_threshold &&
	    cm_is_reach_fchg_threshold(cm))
		need = true;

	return need;
}

static void cm_start_fixed_fchg(struct charger_manager *cm, bool start)
{
	if (!cm->desc->fixed_fchg_running && start) {
		dev_info(cm->dev, "%s, reach fchg threshold, enable it\n", __func__);
		cm->desc->fixed_fchg_running = true;
		schedule_delayed_work(&cm->fixed_fchg_work, 0);
	}
}

static void cm_fixed_fchg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  fixed_fchg_work);
	int ret, delay_work_ms = cm->desc->polling_interval_ms;

	/*
	 * Effects:
	 *   1. Prevent CM_FIXED_FCHG_ENABLE_COUNT from becoming PPS
	 *      within the time and enable the fast charge status.
	 */
	if (!cm->charger_enabled || cm->desc->fast_charger_type != CM_CHARGER_TYPE_FAST)
		goto stop_fixed_fchg;

	/*
	 * The first if branch: fix the problem that the Xiaomi 65W
	 *                      charger PD2.0 and PPS follow closely.
	 */
	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_FAST &&
	    cm->desc->fast_charge_enable_count < CM_FIXED_FCHG_ENABLE_COUNT) {
		cm->desc->fast_charge_enable_count++;
		delay_work_ms = CM_CP_WORK_TIME_MS;
	} else if (cm->desc->enable_fast_charge) {
		if (cm_is_disable_fixed_fchg_check(cm, &delay_work_ms))
			goto stop_fixed_fchg;
	} else {
		ret = cm_fixed_fchg_enable(cm);
		if (ret) {
			dev_err(cm->dev, "%s, failed to enable fixed fchg\n", __func__);
			cm->desc->fixed_fchg_running = false;
			cm->desc->fast_charge_enable_count = 0;
			return;
		}
	}

	schedule_delayed_work(&cm->fixed_fchg_work, msecs_to_jiffies(delay_work_ms));
	return;

stop_fixed_fchg:
	ret = cm_fixed_fchg_disable(cm);
	if (ret) {
		dev_err(cm->dev, "%s, failed to disable fixed fchg, try again!\n", __func__);
		schedule_delayed_work(&cm->fixed_fchg_work,
				      msecs_to_jiffies(CM_FIXED_FCHG_TRY_DIS_WORK_MS));
		return;
	}
	cm->desc->fixed_fchg_running = false;
	cm->desc->fast_charge_enable_count = 0;
}

static void cm_cp_state_change(struct charger_manager *cm, int state)
{
	cm->desc->cp_sm.state = state;
	dev_dbg(cm->dev, "%s, current cp_state = %d\n", __func__, state);
}

static int cm_cp_charger_enable_one(struct charger_manager *cm, bool enable, const char *psy_name)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret = 0;

	cp_psy = power_supply_get_by_name(psy_name);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n", psy_name);
		return ret;
	}

	val.intval = enable;
	ret = power_supply_set_property(cp_psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(cp_psy);
	if (ret)
		dev_err(cm->dev, "failed to %s %s charge pump, ret = %d\n",
			enable ? "enabel" : "disable", psy_name, ret);

	return ret;
}

static bool cm_cp_charger_enable(struct charger_manager *cm, bool enable)
{
	int i;

	if (!cm->desc->psy_cp_stat)
		return true;

	if (enable) {
		for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
			if (cm_cp_charger_enable_one(cm, enable, cm->desc->psy_cp_stat[i]))
				return false;
		}
	} else {
		for (i = cm->desc->cp_nums - 1; cm->desc->psy_cp_stat[i]; i--) {
			if (cm_cp_charger_enable_one(cm, enable, cm->desc->psy_cp_stat[i]))
				return false;
		}
	}

	return true;
}

static void cm_init_cp(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm->desc->psy_cp_stat)
		return;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_USB_PRESENT_CMD;
		ret = power_supply_set_property(cp_psy, POWER_SUPPLY_PROP_PRESENT, &val);
		power_supply_put(cp_psy);
		if (ret) {
			dev_err(cm->dev, "fail to init cp[%d], ret = %d\n", i, ret);
			break;
		}
	}
}

static int cm_adjust_fchg_current(struct charger_manager *cm, int cur)
{
	union power_supply_propval val;
	int ret;

	if (!cm->fchg_info->ops || !cm->fchg_info->ops->adj_fchg_cur) {
		dev_err(cm->dev, "%s, fchg ops or adj_fchg_cur is null\n", __func__);
		return -EINVAL;
	}

	val.intval = cur;
	ret = cm->fchg_info->ops->adj_fchg_cur(cm->fchg_info, cur);
	if (ret) {
		dev_err(cm->dev, "%s, failed to adjust fchg current = %d, ret=%d\n",
			__func__, cur, ret);
		return ret;
	}

	return 0;
}

/*
 *  Relying on the fast charging protocol of DP/DM for handshake,
 *  the handshake can only be perfomed after the BC1.2 result is
 *  identified as DCP, such as the SFCP protocol.
 */
static void cm_enable_fixed_fchg_handshake(struct charger_manager *cm, bool enable)
{
	dev_dbg(cm->dev, "%s, %s fixed fchg handshake\n", __func__, enable ? "enable" : "disable");
	if (!cm->fchg_info || !cm->fchg_info->ops || !cm->fchg_info->ops->enable_fixed_fchg) {
		dev_err(cm->dev, "%s, fchg_info or ops or enable_fixed_fchg is null\n", __func__);
		return;
	}

	if (!cm->desc->support_fixed_fchg || !cm->fchg_info->support_fchg)
		return;

	if (enable && !cm->desc->is_fast_charge &&
	    cm->desc->charger_type == CM_CHARGER_TYPE_DCP)
		cm->fchg_info->ops->enable_fixed_fchg(cm->fchg_info, true);
	else if (!enable)
		cm->fchg_info->ops->enable_fixed_fchg(cm->fchg_info, false);
}

static int cm_fast_enable_pps(struct charger_manager *cm, bool enable)
{
	int ret;

	dev_dbg(cm->dev, "%s, pps %s\n", __func__, enable ? "enable" : "disable");
	if (!cm->fchg_info->ops || !cm->fchg_info->ops->enable_dynamic_fchg) {
		dev_err(cm->dev, "%s, ops or enable_dynamic_fchg is null\n", __func__);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->enable_dynamic_fchg(cm->fchg_info, enable);
	if (ret)
		dev_err(cm->dev, "%s, failed to %s pps, ret=%d\n",
			__func__, enable ? "enable" : "disable", ret);

	return ret;
}

static bool cm_check_primary_charger_enabled(struct charger_manager *cm)
{
	int ret;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(psy);
	if (!ret) {
		if (val.intval)
			enabled = true;
	}

	dev_dbg(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");
	return enabled;
}

static bool cm_check_cp_charger_enabled(struct charger_manager *cm)
{
	int ret, i;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *cp_psy;

	if (!cm->desc->psy_cp_stat)
		return false;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		power_supply_put(cp_psy);
		if (!ret) {
			enabled = !!val.intval;
			if (!enabled) {
				dev_dbg(cm->dev, "%s: cp charger enabled status of %s is disabled\n",
					__func__, cm->desc->psy_cp_stat[i]);
				break;
			}
		} else {
			enabled = false;
			dev_err(cm->dev, "%s: fail to get cp charger enabled status of %s\n",
				__func__, cm->desc->psy_cp_stat[i]);
			break;
		}
	}

	dev_dbg(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");

	return enabled;
}

static void cm_cp_clear_soft_alarm_status(struct charger_manager *cm)
{
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_cp_alarm_status *alarm = &cm->desc->cp_sm.cp_info.alm;

	dev_info(cm->dev, "%s\n", __func__);
	cp_info->cp_soft_alarm_event = false;

	alarm->bat_ovp_alarm = false;
	alarm->bat_ocp_alarm = false;
	alarm->bus_ovp_alarm = false;
	alarm->bus_ocp_alarm = false;
	alarm->bat_ucp_alarm = false;
}

static void cm_cp_clear_fault_status(struct charger_manager *cm)
{
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_cp_fault_status *fault = &cm->desc->cp_sm.cp_info.flt;
	struct cm_cp_alarm_status *alarm = &cm->desc->cp_sm.cp_info.alm;

	dev_info(cm->dev, "%s\n", __func__);
	cp_info->cp_fault_event = false;

	fault->bat_ovp_fault = false;
	fault->bat_ocp_fault = false;
	fault->bus_ovp_fault = false;
	fault->bus_ocp_fault = false;
	fault->bat_therm_fault = false;
	fault->bus_therm_fault = false;
	fault->die_therm_fault = false;

	alarm->bat_ovp_alarm = false;
	alarm->bat_ocp_alarm = false;
	alarm->bus_ovp_alarm = false;
	alarm->bus_ocp_alarm = false;
	alarm->bat_therm_alarm = false;
	alarm->bus_therm_alarm = false;
	alarm->die_therm_alarm = false;
	alarm->bat_ucp_alarm = false;
}

static void cm_check_cp_soft_monitor_alarm_status(struct charger_manager *cm)
{
	struct cm_cp_alarm_status *alarm = &cm->desc->cp_sm.cp_info.alm;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct power_supply *psy;
	union power_supply_propval val;
	u32 cp_soft_monitor_alarm = 0;
	int ret, i;

	if (!cm->desc->psy_cp_stat)
		return;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "%s, Cannot find power supply \"%s\"\n",
				__func__, cm->desc->psy_cp_stat[i]);
			continue;
		}

		/*
		 *  If CP has an alarm register, return 0 without software monitoring.
		 *  Such as bq25970.
		 */
		val.intval = CM_SOFT_ALARM_HEALTH_CMD;
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "%s, failed to get soft monitor alarm staus.\n", __func__);
			continue;
		}

		if (!val.intval)
			continue;

		cp_soft_monitor_alarm |= val.intval;
		dev_info(cm->dev, "%s, cp_name: %s, soft monitor alarm status = 0x%x\n",
			 __func__, cm->desc->psy_cp_stat[i], val.intval);
	}

	if (!cp_soft_monitor_alarm)
		return;

	cp_info->cp_soft_alarm_event = true;

	alarm->bat_ovp_alarm = !!(cp_soft_monitor_alarm & CM_CHARGER_BAT_OVP_ALARM_MASK);
	alarm->bat_ocp_alarm = !!(cp_soft_monitor_alarm & CM_CHARGER_BAT_OCP_ALARM_MASK);
	alarm->bus_ovp_alarm = !!(cp_soft_monitor_alarm & CM_CHARGER_BUS_OVP_ALARM_MASK);
	alarm->bus_ocp_alarm = !!(cp_soft_monitor_alarm & CM_CHARGER_BUS_OCP_ALARM_MASK);
	alarm->bat_ucp_alarm = !!(cp_soft_monitor_alarm & CM_CHARGER_BAT_UCP_ALARM_MASK);
}

static void cm_check_cp_fault_status(struct charger_manager *cm)
{
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_cp_fault_status *fault = &cm->desc->cp_sm.cp_info.flt;
	struct cm_cp_alarm_status *alarm = &cm->desc->cp_sm.cp_info.alm;
	struct power_supply *psy;
	union power_supply_propval val;
	u32 cp_hw_monitor_fault = 0;
	int ret, i;

	if (!cm->desc->psy_cp_stat || !cm->desc->cm_check_int)
		return;

	dev_info(cm->dev, "%s\n", __func__);

	cm->desc->cm_check_int = false;
	cp_info->cp_fault_event = true;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "%s, Cannot find power supply \"%s\"\n",
				__func__, cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_FAULT_HEALTH_CMD;
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "%s, failed to get fault status of %s, ret = %d\n",
				__func__, cm->desc->psy_cp_stat[i], ret);
			continue;
		}

		if (!val.intval)
			continue;

		cp_hw_monitor_fault |= val.intval;
		dev_info(cm->dev, "%s, cp_name: %s, hw monitor fault status = 0x%x\n",
			 __func__, cm->desc->psy_cp_stat[i], val.intval);
	}

	if (!cp_hw_monitor_fault)
		return;

	fault->bat_ovp_fault = !!(cp_hw_monitor_fault & CM_CHARGER_BAT_OVP_FAULT_MASK);
	fault->bat_ocp_fault = !!(cp_hw_monitor_fault & CM_CHARGER_BAT_OCP_FAULT_MASK);
	fault->bus_ovp_fault = !!(cp_hw_monitor_fault & CM_CHARGER_BUS_OVP_FAULT_MASK);
	fault->bus_ocp_fault = !!(cp_hw_monitor_fault & CM_CHARGER_BUS_OCP_FAULT_MASK);
	fault->bat_therm_fault = !!(cp_hw_monitor_fault & CM_CHARGER_BAT_THERM_FAULT_MASK);
	fault->bus_therm_fault = !!(cp_hw_monitor_fault & CM_CHARGER_BUS_THERM_FAULT_MASK);
	fault->die_therm_fault = !!(cp_hw_monitor_fault & CM_CHARGER_DIE_THERM_FAULT_MASK);

	alarm->bat_ovp_alarm = !!(cp_hw_monitor_fault & CM_CHARGER_BAT_OVP_ALARM_MASK);
	alarm->bat_ocp_alarm = !!(cp_hw_monitor_fault & CM_CHARGER_BAT_OCP_ALARM_MASK);
	alarm->bus_ovp_alarm = !!(cp_hw_monitor_fault & CM_CHARGER_BUS_OVP_ALARM_MASK);
	alarm->bus_ocp_alarm = !!(cp_hw_monitor_fault & CM_CHARGER_BUS_OCP_ALARM_MASK);
	alarm->bat_therm_alarm = !!(cp_hw_monitor_fault & CM_CHARGER_BAT_THERM_ALARM_MASK);
	alarm->bus_therm_alarm = !!(cp_hw_monitor_fault & CM_CHARGER_BUS_THERM_ALARM_MASK);
	alarm->die_therm_alarm = !!(cp_hw_monitor_fault & CM_CHARGER_DIE_THERM_ALARM_MASK);
	alarm->bat_ucp_alarm = !!(cp_hw_monitor_fault & CM_CHARGER_BAT_UCP_ALARM_MASK);
}

static void cm_update_cp_charger_status(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	bool is_cp_val;
	int bat_temp = cm->desc->temperature;

	is_cp_val = !!(cp->running & cm->desc->enable_fast_charge);
	cp_info->ibus_uA = 0;
	cp_info->vbat_uV = 0;
	cp_info->vbus_uV = 0;
	cp_info->ibat_uA = 0;

	if (is_cp_val) {
		if (get_cp_ibus_uA(cm, &cp_info->ibus_uA)) {
			cp_info->ibus_uA = 0;
			dev_err(cm->dev, "get ibus current error.\n");
		}

		if (get_cp_vbat_uV(cm, &cp_info->vbat_uV)) {
			cp_info->vbat_uV = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

		if (get_cp_vbus_uV(cm, &cp_info->vbus_uV)) {
			cp_info->vbat_uV = 0;
			dev_err(cm->dev, "get vbus error.\n");
		}

		if (get_cp_ibat_uA(cm, &cp_info->ibat_uA)) {
			cp_info->ibat_uA = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

	} else {
		if (get_vbat_now_uV(cm, &cp_info->vbat_uV)) {
			cp_info->vbat_uV = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

		if (get_charger_voltage(cm, &cp_info->vbus_uV)) {
			cp_info->vbat_uV = 0;
			dev_err(cm->dev, "get vbus error.\n");
		}


		if (get_ibat_now_uA(cm, &cp_info->ibat_uA)) {
			cp_info->ibat_uA = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}
	}

	if (buck_info->buck_is_inited && cm_get_battery_temperature(cm, &bat_temp))
		dev_err(cm->dev, "%s, failed to get battery temperature\n", __func__);

	cp->bat_temp = bat_temp;
	cp->vbat_uV = cp_info->vbat_uV;
	cp->vbus_uV = cp_info->vbus_uV;
	cp->ibat_uA = cp_info->ibat_uA;
	cp->ibus_uA = cp_info->ibus_uA;

	dev_dbg(cm->dev, "%s, %s, Vbat: %duV, Vbus: %duV, Ibat: %duA, Ibus: %duA, Tbat: %d\n",
	       __func__, is_cp_val ? "charge pump" : "Primary charger",
	       cp_info->vbat_uV, cp_info->vbus_uV, cp_info->ibat_uA, cp_info->ibus_uA,
	       cp->bat_temp);
}

static void cm_cp_check_vbus_status(struct charger_manager *cm)
{
	struct cm_cp_fault_status *fault = &cm->desc->cp_sm.cp_info.flt;
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret, i;

	fault->vbus_error_lo = false;
	fault->vbus_error_hi = false;

	if (!cm->desc->psy_cp_stat || !cm->desc->cp_sm.running)
		return;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_BUS_ERR_HEALTH_CMD;
		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_HEALTH, &val);
		power_supply_put(cp_psy);
		if (!ret) {
			fault->vbus_error_lo = !!(val.intval & CM_CHARGER_BUS_ERR_LO_MASK);
			fault->vbus_error_hi = !!(val.intval & CM_CHARGER_BUS_ERR_HI_MASK);
		} else {
			dev_err(cm->dev, "failed to get vbus status of  %s, ret = %d\n",
				cm->desc->psy_cp_stat[i], ret);
		}
	}

	if (fault->vbus_error_lo || fault->vbus_error_hi)
		dev_info(cm->dev, "%s, vbus_error_lo = %d, vbus_error_hi = %d\n",
			 __func__, fault->vbus_error_lo, fault->vbus_error_hi);
}

static void cm_step_interval_polling(struct charger_manager *cm, int ir_drop)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	int i, step_chg_target_ibat = -EINVAL, step_chg_target_vbat = -EINVAL;

	for (i = 0; i < cm->desc->step_chg_table_size; i++) {
		if (cp->jeita_status == cm->desc->step_chg_table[i].jeita_inr &&
		    cp->vbat_uV < cm->desc->step_chg_table[i].term_volt + ir_drop) {
			step_chg_target_ibat = cm->desc->step_chg_table[i].current_ua;
			step_chg_target_vbat = cm->desc->step_chg_table[i].term_volt;
			break;
		}
	}

	cp->cur_step_chg_ibat = step_chg_target_ibat;
	cp->cur_step_chg_vbat = step_chg_target_vbat;
}

static void cm_step_chg_update_interval_status(struct charger_manager *cm,
					       int last_step_chg_vbat,
					       int last_step_chg_ibat,
					       int ir_drop)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;

	/* The last step interval parameter is invalid. */
	if (last_step_chg_ibat <= 0) {
		cp->step_down_trigger = 0;
		cm_step_interval_polling(cm, ir_drop);
		goto update_step_interval;
	}

	/* Step invariant interval. */
	if (last_step_chg_ibat > 0 && cp->cur_step_chg_ibat == last_step_chg_ibat) {
		cm_step_interval_polling(cm, ir_drop);
		/* The step interval parameter is invalid this time. */
		if (cp->cur_step_chg_ibat < 0) {
			cp->step_down_trigger = 0;
			goto update_step_interval;
		}

		return;
	}

	/* Step down interval. */
	if (last_step_chg_ibat > 0 && cp->cur_step_chg_ibat < last_step_chg_ibat) {
		/* Step charging interval CV stage */
		if (cp->ibat_uA < cp->cur_step_chg_ibat) {
			cp->step_down_trigger++;
			if (cp->step_down_trigger >= CM_CP_STEP_CHG_DOWN_COUNT) {
				cp->step_down_trigger = 0;
				goto update_step_interval;
			}
		} else {
			cp->step_down_trigger = 0;
		}

		return;
	}

	/* Step up interval. */
	if (last_step_chg_ibat > 0 && cp->cur_step_chg_ibat > last_step_chg_ibat) {
		cp->step_down_trigger = 0;
		/* Stpe charging, anti-shake section */
		if (cp->vbat_uV > cp->cur_step_chg_vbat + ir_drop - 50000) {
			cp->cur_step_chg_ibat = last_step_chg_ibat;
			cp->cur_step_chg_vbat = last_step_chg_vbat;
		}

		goto update_step_interval;
	}

update_step_interval:
	cp->step_chg_ibat = cp->cur_step_chg_ibat;
	cp->step_chg_vbat = cp->cur_step_chg_vbat;
}

static void cm_update_step_chg_status(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	int last_step_chg_ibat, last_step_chg_vbat;
	int ir_drop = 0;

	if (!cm->desc->support_step_chg)
		return;

	if (cm->desc->step_chg_disabled) {
		cp->step_chg_ibat = -EINVAL;
		cp->step_chg_vbat = -EINVAL;
		if (!cp->disable_step_chg_log) {
			dev_err(cm->dev, "%s, force off step chg.\n", __func__);
			cp->disable_step_chg_log = true;
		}

		return;
	}

	/* The current jeita interval anomaly scene. */
	if (cp->jeita_status < 0) {
		cp->step_chg_ibat = -EINVAL;
		cp->step_chg_vbat = -EINVAL;
		dev_err(cm->dev, "%s, jeita_status = %d, the current jeita interval is abnormal\n",
			__func__, cp->jeita_status);
		return;
	}

	if (cm->desc->ir_comp.rc && cm->desc->ir_comp.ir_drop > 0)
		ir_drop = cm->desc->ir_comp.ir_drop;

	last_step_chg_ibat = cp->step_chg_ibat;
	last_step_chg_vbat = cp->step_chg_vbat;

	/* Jeita interval changed. */
	if (cp->jeita_status != cp->last_jeita_status) {
		last_step_chg_ibat = -EINVAL;
		cp->cur_step_chg_ibat = -EINVAL;
		cp->cur_step_chg_vbat = -EINVAL;
		cp->step_down_trigger = 0;
		cp->last_jeita_status = cp->jeita_status;

		cm_step_interval_polling(cm, ir_drop);
		cp->step_chg_ibat = cp->cur_step_chg_ibat;
		cp->step_chg_vbat = cp->cur_step_chg_vbat;
	} else {
		cm_step_chg_update_interval_status(cm,
						   last_step_chg_vbat,
						   last_step_chg_ibat,
						   ir_drop);
	}

	dev_dbg(cm->dev, "%s, stpe_chg_cv = [%duV %duA], step_chg_real_cv = [%duV %duA %duV %duA]\n"
		, __func__, cp->step_chg_vbat, cp->step_chg_ibat, cp->cur_step_chg_vbat,
		cp->cur_step_chg_ibat, last_step_chg_vbat, last_step_chg_ibat);

	dev_dbg(cm->dev, "%s, jeita_status = %d, ibat_uA = %d, vbat_uV = %d, ir_drop = %d\n",
		__func__, cp->jeita_status, cp->ibat_uA, cp->vbat_uV, ir_drop);
}

static void cm_check_target_vbat(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	int target_vbat, ir_drop = 0;

	if (cm->desc->ir_comp.rc && cm->desc->ir_comp.ir_drop > 0)
		ir_drop = cm->desc->ir_comp.ir_drop;

	target_vbat = cm->desc->constant_charge_voltage_max_uv + ir_drop;

	if (cp->step_chg_vbat > 0)
		target_vbat = min(target_vbat, cp->step_chg_vbat + ir_drop);

	if (cp->jeita_vbat > 0)
		target_vbat = min(target_vbat, cp->jeita_vbat + ir_drop);

	if (cp->ir_vbat > 0)
		target_vbat = min(target_vbat, cp->ir_vbat);

	cp->target_vbat = target_vbat;

	dev_dbg(cm->dev, "%s, target_vbat = %d, constant_charge_voltage_max_uv = %d\n",
		__func__, cp->target_vbat, cm->desc->constant_charge_voltage_max_uv);

	dev_dbg(cm->dev, "%s, step_chg_vbat = %d, jeita_vbat = %d, ir_vbat = %d, ir_drop = %d\n",
		__func__, cp->step_chg_vbat, cp->jeita_vbat, cp->ir_vbat, ir_drop);
}

static void cm_check_target_ibat(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	int target_ibat;

	target_ibat = cp->default_max_ibat;

	if (cp->jeita_ibat > 0)
		target_ibat = min(target_ibat, cp->jeita_ibat);

	if (cp->step_chg_ibat > 0)
		target_ibat = min(target_ibat, cp->step_chg_ibat);

	cp->target_ibat = target_ibat;

	dev_dbg(cm->dev, "%s, Ibat, default_max: %d, jeita: %d, step_chg: %d, target: %d\n",
	       __func__, cp->default_max_ibat, cp->jeita_ibat, cp->step_chg_ibat, cp->target_ibat);
}

static void cm_check_target_ibus(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	struct cm_adaptive_fchg_info *adaptive_fchg = &cm->desc->cp_sm.adaptive_fchg;
	int target_ibus;

	target_ibus = min(adaptive_fchg->adapter_max_ibus, cp->default_max_ibus);
	if (buck_info->buck_is_running &&
	    cp_info->cp_ibus_limit_max > 0 && buck_info->buck_ibus_limit_max > 0)
		target_ibus = min(target_ibus,
				  cp_info->cp_ibus_limit_max + buck_info->buck_ibus_limit_max);
	else if (cp_info->cp_ibus_limit_max > 0)
		target_ibus = min(target_ibus, cp_info->cp_ibus_limit_max);

	cp->target_ibus = target_ibus;

	dev_dbg(cm->dev, "%s, Ibus, default_max: %d, adapter_max: %d, thermal: %d, target: %d\n",
	       __func__, cp->default_max_ibus, adaptive_fchg->adapter_max_ibus,
	       cm->desc->thm_info.thm_adjust_cur, cp->target_ibus);
}

static void cm_check_request_ibus(struct charger_manager *cm)
{
	struct cm_adaptive_fchg_info *adaptive_fchg = &cm->desc->cp_sm.adaptive_fchg;
	int request_ibus;

	request_ibus = adaptive_fchg->request_ibus;

	if (adaptive_fchg->adapter_max_ibus > 0)
		request_ibus = min(request_ibus, adaptive_fchg->adapter_max_ibus);

	adaptive_fchg->request_ibus = request_ibus;

	dev_dbg(cm->dev, "%s, adp_max_ibus: %d, request_ibus: %d\n",
	       __func__, adaptive_fchg->adapter_max_ibus, adaptive_fchg->request_ibus);
}

static void cm_check_request_vbus(struct charger_manager *cm)
{
	struct cm_adaptive_fchg_info *adaptive_fchg = &cm->desc->cp_sm.adaptive_fchg;
	int request_vbus;

	request_vbus = adaptive_fchg->request_vbus;
	if (adaptive_fchg->adapter_max_vbus > 0)
		request_vbus = min(request_vbus, adaptive_fchg->adapter_max_vbus);

	adaptive_fchg->request_vbus = request_vbus;

	dev_dbg(cm->dev, "%s, adp_max_vbus = %d, request_vbus = %d\n",
	       __func__, adaptive_fchg->adapter_max_vbus, adaptive_fchg->request_vbus);
}

static int cm_vbat_step_algo(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	int vbat_step = 0, delta_vbat_uV;

	delta_vbat_uV = cp->target_vbat - cp->vbat_uV;

	if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP1)
		vbat_step = CM_CP_VSTEP * 5;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP2)
		vbat_step = CM_CP_VSTEP * 4;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP3)
		vbat_step = CM_CP_VSTEP * 3;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP4)
		vbat_step = CM_CP_VSTEP * 2;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP5)
		vbat_step = CM_CP_VSTEP;
	else if (cp->vbat_uV > 0 && delta_vbat_uV < 0)
		vbat_step = -CM_CP_VSTEP * 2;

	return vbat_step;
}

static int cm_ibat_step_algo(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	int ibat_step = 0, delta_ibat_uA;

	delta_ibat_uA = cp->target_ibat - cp->ibat_uA;

	if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP1)
		ibat_step = CM_CP_VSTEP * 3;
	else if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP2)
		ibat_step = CM_CP_VSTEP * 2;
	else if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP3)
		ibat_step = CM_CP_VSTEP;
	else if (cp->ibat_uA > 0 && delta_ibat_uA < -CM_CP_IBAT_STEP1 / 2)
		ibat_step = -CM_CP_VSTEP * 6;
	else if (cp->ibat_uA > 0 && delta_ibat_uA < -CM_CP_IBAT_STEP2 / 2)
		ibat_step = -CM_CP_VSTEP * 4;
	else if (cp->ibat_uA > 0 && delta_ibat_uA < 0)
		ibat_step = -CM_CP_VSTEP * 2;

	return ibat_step;
}

static int cm_vbus_step_algo(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_adaptive_fchg_info *adaptive_fchg = &cm->desc->cp_sm.adaptive_fchg;
	int vbus_step = 0, delta_vbus_uV;

	cp->target_vbus = adaptive_fchg->adapter_max_vbus;
	delta_vbus_uV = cp->target_vbus - cp->vbus_uV;

	if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP1)
		vbus_step = CM_CP_VSTEP * 3;
	else if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP2)
		vbus_step = CM_CP_VSTEP * 2;
	else if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP3)
		vbus_step = CM_CP_VSTEP;
	else if (cp->vbus_uV > 0 && delta_vbus_uV < 0)
		vbus_step = -CM_CP_VSTEP * 2;

	return vbus_step;
}

static int cm_ibus_step_algo(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	int ibus_step = 0, delta_ibus_uA;

	delta_ibus_uA = cp->target_ibus - cp->ibus_uA;

	if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP1)
		ibus_step = CM_CP_VSTEP * 3;
	else if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP2)
		ibus_step = CM_CP_VSTEP * 2;
	else if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP3)
		ibus_step = CM_CP_VSTEP;
	else if (cp->ibus_uA > 0 && delta_ibus_uA < 0)
		ibus_step = -CM_CP_VSTEP * 2;

	return ibus_step;
}

static int cm_thermal_step_algo(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_adaptive_fchg_info *adaptive_fchg = &cm->desc->cp_sm.adaptive_fchg;
	int thermal_ibus_step = 0, delta_power_uw, power_now_uw;

	if (!cm->desc->thm_info.thm_pwr)
		return CM_CP_VSTEP * 5;

	if (cp->ibus_uA <= 0)
		return thermal_ibus_step;

	power_now_uw = (adaptive_fchg->request_vbus / 1000) * (cp->ibus_uA / 1000);
	delta_power_uw = cm->desc->thm_info.thm_pwr * 1000 - power_now_uw;

	dev_dbg(cm->dev, "%s, power_now_uw = %d, delta_power_uw = %d\n",
		__func__, power_now_uw, delta_power_uw);

	if (delta_power_uw > CM_CP_THERMAL_STEP1)
		thermal_ibus_step = CM_CP_VSTEP * 3;
	else if (delta_power_uw > CM_CP_THERMAL_STEP2)
		thermal_ibus_step = CM_CP_VSTEP * 2;
	else if (delta_power_uw > 0)
		thermal_ibus_step = CM_CP_VSTEP;
	else if (delta_power_uw < -CM_CP_THERMAL_STEP1)
		thermal_ibus_step = -CM_CP_VSTEP * 3;
	else if (delta_power_uw < -CM_CP_THERMAL_STEP2)
		thermal_ibus_step = -CM_CP_VSTEP * 2;
	else if (delta_power_uw < 0)
		thermal_ibus_step = -CM_CP_VSTEP;

	return thermal_ibus_step;
}

static bool cm_is_taper_done(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;

	bool is_taper_done = false;

	/* check taper done*/
	if (cp->vbat_uV >= cp->target_vbat - CM_CP_TAPER_DELTA_VBAT_THRESHOLD) {
		if (cp->ibat_uA < cp->taper_current) {
			if (cp->taper_trigger_cnt++ > CM_CP_TAPER_UCP_THRESHOLD) {
				is_taper_done = true;
				cp->taper_trigger_cnt = 0;
				dev_info(cm->dev, "%s, vbatt = %duV, target_vbat = %duV, taper_trigger_cnt=%d\n",
					 __func__, cp->vbat_uV, cp->target_vbat,
					 cp->taper_trigger_cnt);
				return is_taper_done;
			}
		} else {
			cp->taper_trigger_cnt = 0;
		}
	}

	return is_taper_done;
}

static void cm_cp_tune_algo(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	struct cm_adaptive_fchg_info *adaptive_fchg = &cm->desc->cp_sm.adaptive_fchg;
	struct cm_cp_alarm_status *alarm = &cm->desc->cp_sm.cp_info.alm;
	int vbat_step = 0, ibat_step = 0;
	int vbus_step = 0, ibus_step = 0;
	int alarm_step = 0;
	int thermal_step = 0;
	int cp_step = 0;
	int target_step = 0;

	/* check battery current*/
	cm_check_target_ibat(cm);
	ibat_step = cm_ibat_step_algo(cm);

	/* check battery voltage*/
	cm_check_target_vbat(cm);
	vbat_step = cm_vbat_step_algo(cm);

	/* check bus voltage*/
	vbus_step = cm_vbus_step_algo(cm);

	/* check bus current*/
	ibus_step = cm_ibus_step_algo(cm);

	/* check thermal power*/
	thermal_step = cm_thermal_step_algo(cm);

	/* check alarm status*/
	if (alarm->bat_ovp_alarm || alarm->bat_ocp_alarm ||
	    alarm->bus_ovp_alarm || alarm->bus_ocp_alarm ||
	    alarm->bat_therm_alarm || alarm->bus_therm_alarm ||
	    alarm->die_therm_alarm) {
		dev_warn(cm->dev, "%s, alarm, bat_ovp: %d, bat_ocp: %d, bus_ovp: %d, bus_ocp: %d\n",
			 __func__, alarm->bat_ovp_alarm, alarm->bat_ocp_alarm, alarm->bus_ovp_alarm,
			 alarm->bus_ocp_alarm);

		dev_warn(cm->dev, "%s, alarm, bat_therm: %d, bus_therm: %d, die_therm: %d\n",
			 __func__, alarm->bat_therm_alarm, alarm->bus_therm_alarm,
			 alarm->die_therm_alarm);
		if (cp_info->cp_soft_alarm_event)
			alarm_step = -CM_CP_VSTEP * 3;
		else
			alarm_step = -CM_CP_VSTEP * 2;
	} else {
		alarm_step = CM_CP_VSTEP * 3;
	}

	target_step = min(vbat_step, ibat_step);
	target_step = min(target_step, vbus_step);
	target_step = min(target_step, ibus_step);
	target_step = min(target_step, alarm_step);
	target_step = min(target_step, thermal_step);

	if (buck_info->buck_is_running) {
		cp_step = cm_cp_step_algo(cm);
		target_step = min(target_step, cp_step);
		cm_adjust_cp_ibus_limit_algo(cm, cp_step);
		cm_adjust_buck_ibat_limit_algo(cm);
		dev_info(cm->dev, "%s, tune_step = [%d %d %d %d %d %d %d]\n",
			  __func__, vbus_step, ibus_step, vbat_step, ibat_step, alarm_step,
			  thermal_step, cp_step);
	} else {
		dev_info(cm->dev, "%s, tune_step = [%d %d %d %d %d %d]\n",
			  __func__, vbus_step, ibus_step, vbat_step, ibat_step, alarm_step,
			  thermal_step);
	}

	adaptive_fchg->request_vbus += target_step;
	cm_check_request_vbus(cm);

	dev_info(cm->dev, "%s, bus: %duV %duA, bat: %duV %duA, ir_drop: %duV, ucp_cnt: %d\n",
		 __func__, cp->vbus_uV, cp->ibus_uA, cp->vbat_uV, cp->ibat_uA,
		 cm->desc->ir_comp.ir_drop, cp->ibat_ucp_cnt);

	dev_info(cm->dev, "%s, target, bus: %duV %duA, bat: %duV %duA, request: %duV %duA\n",
		 __func__, cp->target_vbus, cp->target_ibus, cp->target_vbat, cp->target_ibat,
		 adaptive_fchg->request_vbus, adaptive_fchg->request_ibus);

	if (adaptive_fchg->last_request_vbus != adaptive_fchg->request_vbus) {
		if (cm_adjust_fchg_voltage(cm, adaptive_fchg->request_vbus)) {
			dev_info(cm->dev, "%s, failed to adjust volatge, vol=%d\n",
				 __func__, adaptive_fchg->request_vbus);
			adaptive_fchg->request_vbus = adaptive_fchg->last_request_vbus;
		} else {
			adaptive_fchg->last_request_vbus = adaptive_fchg->request_vbus;
			adaptive_fchg->adjust_cnt = 0;
		}
	} else if (adaptive_fchg->adjust_cnt++ > CM_CP_ADJUST_VOLTAGE_THRESHOLD) {
		if (!cm_adjust_fchg_voltage(cm, adaptive_fchg->request_vbus))
			adaptive_fchg->adjust_cnt = 0;
	}
}

static bool cm_check_ibat_ucp_status(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_cp_alarm_status *alarm = &cm->desc->cp_sm.cp_info.alm;
	bool status = false;
	bool ibat_ucp_flag = false;

	if (alarm->bat_ucp_alarm) {
		dev_warn(cm->dev, "%s, bat_ucp_alarm = %d\n", __func__, alarm->bat_ucp_alarm);
		cp->ibat_ucp_cnt++;
		ibat_ucp_flag = true;
	}

	if (!cp->ibat_ucp_cnt)
		return status;

	if (cp->vbat_uV >= cp->target_vbat - CM_CP_TAPER_DELTA_VBAT_THRESHOLD) {
		cp->ibat_ucp_cnt = 0;
		return status;
	}

	if (cp->ibat_uA < cp->taper_current && !(ibat_ucp_flag))
		cp->ibat_ucp_cnt++;
	else if (cp->ibat_uA >= cp->taper_current)
		cp->ibat_ucp_cnt = 0;

	if (cp->ibat_ucp_cnt > CM_CP_IBAT_UCP_THRESHOLD)
		status = true;

	return status;
}

static int cm_get_buck_max_termina_vol(struct charger_manager *cm, int *term_vol)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!desc->psy_charger_stat) {
		dev_err(cm->dev, "%s, psy_charger_stat is null!!!\n", __func__);
		return -ENODEV;
	}

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "%s, cannot find power supply \"%s\"\n",
			__func__, cm->desc->psy_charger_stat[0]);
		return -ENODEV;
	}

	val.intval = CM_BUCK_MAX_TERMINA_VOL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get \"%s\" max terminal voltage, ret: %d\n",
			__func__, desc->psy_charger_stat[0], ret);
		return ret;
	}

	*term_vol = val.intval;

	return 0;
}

static int cm_set_buck_max_termina_vol(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, i;

	if (!desc->psy_charger_stat) {
		dev_err(cm->dev, "%s, psy_charger_stat is null!!!\n", __func__);
		return -ENODEV;
	}

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "%s, cannot find power supply \"%s\"\n",
				__func__, desc->psy_charger_stat[i]);
			return -ENODEV;
		}

		val.intval = CM_BUCK_MAX_TERMINA_VOL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "%s, failed to set \"%s\" max terminal voltage, ret: %d\n",
				__func__, desc->psy_charger_stat[i], ret);
			return ret;
		}
	}

	return 0;
}

static void cm_init_buck_parameter(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	int ret, buck_target_ibat_max, buck_max_term_vol, cp_target_ibat_max, cp_target_ibat_min;

	cp_info->cp_ibus_limit_max = cp->default_max_ibus;
	if (!cm->desc->support_cp_buck_work_tgt)
		return;

	ret = cm_get_buck_max_termina_vol(cm, &buck_max_term_vol);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get max buck terminal voltage, ret = %d\n",
			__func__, ret);
		buck_info->buck_is_inited = false;
		return;
	}

	buck_info->buck_cv_vol = buck_max_term_vol - 100000;
	buck_target_ibat_max = min(cp->default_max_ibat, buck_info->buck_default_ibat_max);
	buck_info->buck_target_ibat_max = buck_target_ibat_max;
	buck_info->buck_ibat_limit_max = roundup(buck_info->buck_target_ibat_max, 100000) + 200000;

	buck_info->buck_ibus_limit_max = roundup(buck_target_ibat_max / CM_CP_BUCK_CHG_EFFICIENCY_P
						 * 100 / 2, 50000) + 100000;

	cp_target_ibat_max = cp->default_max_ibat - buck_target_ibat_max;
	cp_info->cp_ibus_limit_max = roundup(cp_target_ibat_max / CM_CP_CHG_EFFICIENCY_P * 100 / 2,
					     50000) + 100000;

	cp_target_ibat_min = buck_info->buck_target_ibat_max / buck_info->buck_ibat_max_p * 100 -
			     buck_info->buck_target_ibat_max;
	cp_info->cp_ibus_limit_min = rounddown(cp_target_ibat_min / CM_CP_CHG_EFFICIENCY_P * 100
					       / 2, 50000) + 100000;

	cp->is_need_disable_buck = false;
	buck_info->buck_ibus_limit = -EINVAL;
	buck_info->buck_last_ibat_limit = -EINVAL;
	cp_info->cp_ibus_limit = -EINVAL;
	buck_info->buck_is_inited = true;

	dev_info(cm->dev, "%s, buck, max_term_vol: %duV, cv_vol: %duV, target_ibat_max: %duA\n",
		 __func__, buck_max_term_vol, buck_info->buck_cv_vol,
		 buck_info->buck_target_ibat_max);

	dev_info(cm->dev, "%s, buck, ibat_max_p: %d, ibus_limit_max: %duA, ibat_limit_max: %duA\n",
		 __func__, buck_info->buck_ibat_max_p, buck_info->buck_ibus_limit_max,
		 buck_info->buck_ibat_limit_max);

	dev_info(cm->dev, "%s, cp, ibus_limit_max: %duA, ibus_limit_min: %duA\n",
		 __func__, cp_info->cp_ibus_limit_max, cp_info->cp_ibus_limit_min);
}

static bool cm_is_start_buck_charge_check(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;

	if (!buck_info->buck_is_inited)
		return false;

	if (buck_info->buck_is_running || cp->bat_temp < buck_info->buck_start_work_temp_th * 10)
		return false;

	if (cp->ibat_uA < buck_info->buck_start_work_ibat_th)
		return false;

	return true;
}

static bool cm_is_stop_buck_charge_check(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;

	if (!buck_info->buck_is_running || !cp->is_need_disable_buck)
		return false;

	return true;
}

static int cm_enable_buck_charge(struct charger_manager *cm)
{
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	int ret = 0;

	ret = cm_set_buck_max_termina_vol(cm);
	if (ret) {
		dev_err(cm->dev, "%s, failed to set max buck terminal voltage, ret = %d\n",
			__func__, ret);
		return ret;
	}

	buck_info->buck_ibus_limit = buck_info->buck_ibus_limit_max;
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_BUCK_CHARGEIC_ASSIT,
				 SPRD_VOTE_CMD_MIN, buck_info->buck_ibus_limit, cm);

	ret = cm_primary_charger_enable(cm, true);
	if (ret) {
		dev_err(cm->dev, "%s, failed to enable primary charger\n", __func__);
		goto enable_primary_chg_err;
	}

	ret = cm_enable_second_charger(cm, true);
	if (ret) {
		dev_err(cm->dev, "%s, failed to enable second charger\n", __func__);
		goto enable_second_chg_err;
	}

	buck_info->buck_ibat_limit = CM_CP_BUCK_IBAT_START;
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_BUCK_CHARGEIC_ASSIT,
				 SPRD_VOTE_CMD_MIN, buck_info->buck_ibat_limit, cm);

	buck_info->buck_is_running = true;
	cp_info->cp_ibus_limit = cp_info->cp_ibus_limit_max;
	buck_info->buck_last_ibat_limit = buck_info->buck_ibat_limit;
	cm_check_target_ibus(cm);

	goto done;

enable_second_chg_err:
	cm_primary_charger_enable(cm, false);
	buck_info->buck_ibus_limit = -EINVAL;

enable_primary_chg_err:
	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_BUCK_CHARGEIC_ASSIT,
				 SPRD_VOTE_CMD_MIN, buck_info->buck_ibus_limit, cm);

done:
	return ret;
}

static int cm_disable_buck_charge(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	int ret = 0;

	ret = cm_enable_second_charger(cm, false);
	if (ret) {
		dev_err(cm->dev, "%s, failed to enable second charger\n", __func__);
		return ret;
	}

	ret = cm_primary_charger_enable(cm, false);
	if (ret) {
		dev_err(cm->dev, "%s, failed to disable primary charger\n", __func__);
		goto disable_primary_chg_err;
	}

	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_BUCK_CHARGEIC_ASSIT,
				 SPRD_VOTE_CMD_MIN, buck_info->buck_ibat_limit, cm);

	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_BUCK_CHARGEIC_ASSIT,
				 SPRD_VOTE_CMD_MIN, buck_info->buck_ibus_limit, cm);

	buck_info->buck_is_running = false;
	cp->is_need_disable_buck = false;
	cp_info->cp_ibus_limit = -EINVAL;
	buck_info->buck_ibus_limit = -EINVAL;
	buck_info->buck_last_ibat_limit = -EINVAL;
	cm_check_target_ibus(cm);

	goto done;

disable_primary_chg_err:
	cm_enable_second_charger(cm, true);

done:
	return ret;
}

static void cm_update_buck_status(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	int buck_est_ibat = 0;

	if (!buck_info->buck_is_running)
		return;

	cp_info->cp_est_ibat = cp_info->ibus_uA / 100 * 2 * CM_CP_CHG_EFFICIENCY_P;
	buck_info->buck_est_ibat = cp->ibat_uA - cp_info->cp_est_ibat;

	if (buck_info->buck_est_ibat > 0)
		buck_est_ibat = buck_info->buck_est_ibat;

	buck_info->buck_est_ibus = buck_est_ibat / CM_CP_BUCK_CHG_EFFICIENCY_P * 100 / 2;

	cp->ibus_uA = cp_info->ibus_uA + buck_info->buck_est_ibus;
	buck_info->buck_target_ibat = min(cp->ibat_uA / 100 * buck_info->buck_ibat_max_p,
					  buck_info->buck_target_ibat_max);

	buck_info->buck_bat_ovp_alarm = false;
	buck_info->buck_bat_ovp = false;
	if (cp->vbat_uV > buck_info->buck_cv_vol)
		buck_info->buck_bat_ovp = true;
	else if (cp->vbat_uV > buck_info->buck_cv_vol - 50000)
		buck_info->buck_bat_ovp_alarm = true;

	dev_info(cm->dev, "%s, buck_est:[%d %d]uA, buck_target_ibat:%d, cp_ibus_limit:%d, Tbat:%d\n"
		 , __func__, buck_info->buck_est_ibus, buck_info->buck_est_ibat,
		 buck_info->buck_target_ibat, cp_info->cp_ibus_limit, cp->bat_temp);
}

static void cm_adjust_buck_ibat_limit(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;

	if (buck_info->buck_ibat_limit < 0)
		buck_info->buck_ibat_limit = 0;
	else if (buck_info->buck_ibat_limit > buck_info->buck_ibat_limit_max)
		buck_info->buck_ibat_limit = buck_info->buck_ibat_limit_max;

	if (buck_info->buck_ibat_limit == buck_info->buck_last_ibat_limit) {
		buck_info->buck_ibat_limit = buck_info->buck_last_ibat_limit;
		return;
	}

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_BUCK_CHARGEIC_ASSIT,
				 SPRD_VOTE_CMD_MIN, buck_info->buck_ibat_limit, cm);

	dev_info(cm->dev, "%s, buck_ibat_limit: [%d %d]\n",
		 __func__, buck_info->buck_ibat_limit, buck_info->buck_last_ibat_limit);

	if (buck_info->buck_ibat_limit <= CM_CP_BUCK_IBAT_START)
		cp->is_need_disable_buck = true;

	buck_info->buck_last_ibat_limit = buck_info->buck_ibat_limit;
}

static void cm_adjust_cp_ibus_limit_algo(struct charger_manager *cm, int cp_step)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	int ibat_th = 0;

	if (buck_info->buck_bat_ovp) {
		ibat_th = buck_info->buck_target_ibat_max / buck_info->buck_ibat_max_p * 100;
		if (cp->ibat_uA > ibat_th)
			cp_info->cp_ibus_limit -= 50000;
		else
			cp_info->cp_ibus_limit = cp_info->cp_ibus_limit_min;
	} else if (buck_info->buck_bat_ovp_alarm) {
		if (cp_step > 0 && cp_info->cp_ibus_limit > cp_info->cp_ibus_limit_min)
			cp_info->cp_ibus_limit -= 50000;

		if (cp_step < 0 && cp_info->cp_ibus_limit < cp_info->cp_ibus_limit_max)
			cp_info->cp_ibus_limit += 50000;
	} else {
		cp_info->cp_ibus_limit = cp_info->cp_ibus_limit_max;
		return;
	}

	dev_dbg(cm->dev, "%s, buck_bat_ovp: %d, buck_bat_ovp_alarm: %d, cp_ibus_limit: %duA\n",
		 __func__, buck_info->buck_bat_ovp, buck_info->buck_bat_ovp_alarm,
		 cp_info->cp_ibus_limit);

	if (cp_info->cp_ibus_limit > cp_info->cp_ibus_limit_max)
		cp_info->cp_ibus_limit = cp_info->cp_ibus_limit_max;
	else if (cp_info->cp_ibus_limit < cp_info->cp_ibus_limit_min)
		cp_info->cp_ibus_limit = cp_info->cp_ibus_limit_min;
}

static void cm_adjust_buck_ibat_limit_algo(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;

	if (cp->bat_temp < buck_info->buck_start_work_temp_th * 10 ||
	    (buck_info->buck_bat_ovp && cp_info->ibus_uA <= cp_info->cp_ibus_limit_min) ||
	    cp->ibat_uA < buck_info->buck_start_work_ibat_th - 2 * CM_CP_BUCK_IBAT_START) {
		buck_info->buck_ibat_limit -= CM_CP_BUCK_ISTEP * 2;
		goto done;
	}

	if (buck_info->buck_est_ibat > buck_info->buck_target_ibat + 300000) {
		buck_info->buck_ibat_limit -= CM_CP_BUCK_ISTEP * 3;
		goto done;
	} else if (buck_info->buck_est_ibat > buck_info->buck_target_ibat + 200000) {
		buck_info->buck_ibat_limit -= CM_CP_BUCK_ISTEP * 2;
		goto done;
	} else if (buck_info->buck_est_ibat > buck_info->buck_target_ibat + 100000) {
		buck_info->buck_ibat_limit -= CM_CP_BUCK_ISTEP;
		goto done;
	}

	if (buck_info->buck_est_ibat <= 0 &&
	    buck_info->buck_ibat_limit < buck_info->buck_ibus_limit * 2)
		buck_info->buck_ibat_limit += CM_CP_BUCK_ISTEP * 3;
	else if (buck_info->buck_est_ibat > 0 &&
		 buck_info->buck_est_ibat < buck_info->buck_target_ibat - 200000)
		buck_info->buck_ibat_limit += CM_CP_BUCK_ISTEP * 2;
	else if (buck_info->buck_est_ibat > 0 &&
		 buck_info->buck_est_ibat < buck_info->buck_target_ibat - 100000)
		buck_info->buck_ibat_limit += CM_CP_BUCK_ISTEP;

done:
	dev_dbg(cm->dev, "%s, buck_est_ibat: %d, buck_target_ibat: %d, buck_ibus_limit: %d\n",
		 __func__, buck_info->buck_est_ibat, buck_info->buck_target_ibat,
		 buck_info->buck_ibus_limit);
	cm_adjust_buck_ibat_limit(cm);
}

static int cm_cp_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	int cp_step = 0;
	int delta_cp_ibus;

	if (buck_info->buck_est_ibat < 50000 || cp_info->ibus_uA < 50000)
		return CM_CP_VSTEP_MAX;

	delta_cp_ibus = cp_info->cp_ibus_limit - cp_info->ibus_uA;
	if (delta_cp_ibus > CM_CP_BUCK_IBUS_STEP1)
		cp_step = CM_CP_VSTEP * 3;
	else if (delta_cp_ibus > CM_CP_BUCK_IBUS_STEP2)
		cp_step = CM_CP_VSTEP * 2;
	else if (delta_cp_ibus > CM_CP_BUCK_IBUS_STEP3)
		cp_step = CM_CP_VSTEP;
	else if (delta_cp_ibus < -CM_CP_BUCK_IBUS_STEP3 * 2)
		cp_step = -CM_CP_VSTEP * 2;
	else if (delta_cp_ibus < 0)
		cp_step = -CM_CP_VSTEP;

	dev_dbg(cm->dev, "%s, buck_est_ibat: %duA, delta_cp_ibus: %duA, cp_step: %d\n",
		 __func__, buck_info->buck_est_ibat, delta_cp_ibus, cp_step);

	return cp_step;
}

static void cm_cp_state_recovery(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
		 cp->state, cm_cp_state_names[cp->state]);

	if (is_ext_pwr_online(cm) && cm_is_reach_fchg_threshold(cm)) {
		cm_cp_state_change(cm, CM_CP_STATE_ENTRY);
	} else {
		cp->recovery = false;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	}
}

static void cm_cp_state_entry(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	struct cm_adaptive_fchg_info *adaptive_fchg = &cm->desc->cp_sm.adaptive_fchg;
	static int primary_charger_dis_retry;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
		 cp->state, cm_cp_state_names[cp->state]);

	cm->desc->cm_check_fault = false;
	cm_fast_enable_pps(cm, false);
	if (cm_fast_enable_pps(cm, true)) {
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		dev_err(cm->dev, "fail to enable pps\n");
		return;
	}

	cm_cp_charger_enable(cm, false);
	cm_primary_charger_enable(cm, false);
	cm_ir_compensation_enable(cm, false);

	if (cm_check_primary_charger_enabled(cm)) {
		if (primary_charger_dis_retry++ > CM_CP_PRIMARY_CHARGER_DIS_TIMEOUT) {
			cm_cp_state_change(cm, CM_CP_STATE_EXIT);
			primary_charger_dis_retry = 0;
		}
		return;
	}

	if (cm_get_fchg_adapter_max_voltage(cm, &adaptive_fchg->adapter_max_vbus)) {
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	/*
	 * The CM_PPS_5V_PROG_MAX reference value is derived from
	 * section 10.2.3.2 of the PD3.0 spec. The CP turn-on voltage
	 * is required to be greater than 2.05 times the battery
	 * voltage, and the battery voltage must be at least greater
	 * than 3.5V.
	 */
	if (adaptive_fchg->adapter_max_vbus <= CM_PPS_5V_PROG_MAX) {
		dev_info(cm->dev, "%s, APDO max_vol %d can't start the cp, exit pps!!!\n",
			 __func__, adaptive_fchg->adapter_max_vbus);
		cm->desc->force_pps_diasbled = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	if (cm_get_fchg_adapter_max_current(cm, 0, &adaptive_fchg->adapter_max_ibus)) {
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	if (adaptive_fchg->adapter_max_ibus <= 0) {
		dev_info(cm->dev, "%s, APDO ibus %d is abnormal, exit pps!!!\n",
			 __func__, adaptive_fchg->adapter_max_ibus);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	dev_info(cm->dev, "%s, adapter = [%duV %duA]\n",
		 __func__, adaptive_fchg->adapter_max_vbus, adaptive_fchg->adapter_max_ibus);
	cm_init_cp(cm);

	cp->recovery = false;
	cm->desc->enable_fast_charge = true;
	cp->jeita_status = -EINVAL;
	cp->last_jeita_status = -EINVAL;
	cp->jeita_ibat = -EINVAL;
	cp->jeita_vbat = -EINVAL;
	cp->step_chg_ibat = -EINVAL;
	cp->step_chg_vbat = -EINVAL;
	cp->ir_vbat = -EINVAL;

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	cp->tune_vbus_retry = 0;
	primary_charger_dis_retry = 0;
	cp->ibat_ucp_cnt = 0;

	cm_update_cp_charger_status(cm);
	if (cp->vbat_uV < cm->desc->shutdown_voltage) {
		dev_err(cm->dev, "%s, the vbat_uV %d obtained by cp state machine is abnormal!!!\n",
			__func__, cp->vbat_uV);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	cm_init_buck_parameter(cm);
	if (buck_info->buck_is_inited &&
	    cm_set_charger_ovp(cm, CM_FAST_CHARGE_MAX_OVP_ENABLE_CMD)) {
		dev_err(cm->dev, "%s, failed to enable buck max ovp\n", __func__);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	if (cp->vbat_uV <= CM_CP_ACC_VBAT_HTHRESHOLD)
		adaptive_fchg->request_vbus = (3 * CM_CP_VBUS_ERRORLO_THRESHOLD(cp->vbat_uV) +
					       CM_CP_VBUS_ERRORHI_THRESHOLD(cp->vbat_uV)) / 4;
	else
		adaptive_fchg->request_vbus = CM_CP_VBUS_ERRORLO_THRESHOLD(cp->vbat_uV) +
					      2 * CM_CP_VSTEP;

	dev_dbg(cm->dev, "%s, jeita_ibat = %d, request_vbus = %d\n",
		 __func__, cp->jeita_ibat, adaptive_fchg->request_vbus);
	cm_check_request_vbus(cm);
	if (cm_adjust_fchg_voltage(cm, adaptive_fchg->request_vbus)) {
		dev_err(cm->dev, "%s, failed to adjust pps voltage: %duA\n",
			__func__, adaptive_fchg->request_vbus);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	adaptive_fchg->last_request_vbus = adaptive_fchg->request_vbus;

	adaptive_fchg->request_ibus = cp->default_max_ibus;
	cm_check_request_ibus(cm);
	if (cm_adjust_fchg_current(cm, adaptive_fchg->request_ibus)) {
		dev_err(cm->dev, "%s, failed to adjust pps current: %duA\n",
			__func__, adaptive_fchg->request_ibus);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	cm_check_target_ibus(cm);
	cm_cp_state_change(cm, CM_CP_STATE_CHECK_VBUS);
}

static void cm_cp_state_check_vbus(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_adaptive_fchg_info *adaptive_fchg = &cm->desc->cp_sm.adaptive_fchg;
	struct cm_cp_fault_status *fault = &cm->desc->cp_sm.cp_info.flt;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
		 cp->state, cm_cp_state_names[cp->state]);

	cm_cp_check_vbus_status(cm);

	if (fault->vbus_error_lo &&
	    cp->vbus_uV <  CM_CP_VBUS_ERRORHI_THRESHOLD(cp->vbat_uV)) {
		cp->tune_vbus_retry++;
		adaptive_fchg->request_vbus += 2 * CM_CP_VSTEP;
		cm_check_request_vbus(cm);

		if (cm_adjust_fchg_voltage(cm, adaptive_fchg->request_vbus)) {
			dev_err(cm->dev, "%s, fail to adjust pps voltage = %duV\n",
				__func__, adaptive_fchg->request_vbus);
			adaptive_fchg->request_vbus -= 2 * CM_CP_VSTEP;
		}
	} else if (fault->vbus_error_hi &&
		   cp->vbus_uV >  CM_CP_VBUS_ERRORLO_THRESHOLD(cp->vbat_uV)) {
		cp->tune_vbus_retry++;
		adaptive_fchg->request_vbus -= CM_CP_VSTEP;
		if (cm_adjust_fchg_voltage(cm, adaptive_fchg->request_vbus)) {
			dev_err(cm->dev, "%s, fail to adjust pps voltage = %duV\n",
				__func__, adaptive_fchg->request_vbus);
			adaptive_fchg->request_vbus += CM_CP_VSTEP;
		}
	} else {
		cm_cp_charger_enable(cm, true);
		dev_info(cm->dev, "adapter volt tune ok, retry %d times\n",
			 cp->tune_vbus_retry);
		cm_cp_state_change(cm, CM_CP_STATE_TUNE);

		if (!cm_check_cp_charger_enabled(cm))
			cm_cp_charger_enable(cm, true);

		cm->desc->cm_check_fault = true;
		return;
	}

	dev_info(cm->dev, "%s, target_ibat = %duA, request_vbus = %duV, vbus_err_lo = %d, "
		 "vbus_err_hi = %d, retry_time = %d",
		 __func__, cp->target_ibat, adaptive_fchg->request_vbus,
		 fault->vbus_error_lo, fault->vbus_error_hi, cp->tune_vbus_retry);

	if (cp->tune_vbus_retry >= 50) {
		dev_info(cm->dev, "Failed to tune adapter volt into valid range,move to CM_CP_STATE_EXIT\n");
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	}
}

static void cm_cp_state_tune(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;
	struct cm_cp_fault_status *fault = &cm->desc->cp_sm.cp_info.flt;
	int target_vbat = 0;

	if (!cp->state_tune_log) {
		dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
			 cp->state, cm_cp_state_names[cp->state]);
		cp->state_tune_log = true;
	}

	cm_ir_compensation(cm, CM_IR_COMP_STATE_CP, &target_vbat);
	if (target_vbat > 0)
		cp->ir_vbat = target_vbat;

	cm_update_step_chg_status(cm);
	cm_check_target_vbat(cm);

	if (fault->bat_therm_fault || fault->die_therm_fault ||
	    fault->bus_therm_fault) {
		dev_err(cm->dev, "bat_therm_fault = %d, die_therm_fault = %d, exit cp\n",
			fault->bat_therm_fault, fault->die_therm_fault);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (fault->bat_ocp_fault || fault->bat_ovp_fault ||
		fault->bus_ocp_fault || fault->bus_ovp_fault) {
		dev_err(cm->dev, "bat_ocp_fault = %d, bat_ovp_fault = %d, "
			 "bus_ocp_fault = %d, bus_ovp_fault = %d, exit cp\n",
			 fault->bat_ocp_fault, fault->bat_ovp_fault,
			 fault->bus_ocp_fault, fault->bus_ovp_fault);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (!cm_check_cp_charger_enabled(cm)) {
		dev_err(cm->dev, "%s cp charger is disabled, exit cp\n", __func__);
		cp->recovery = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	} else if (cm_check_ibat_ucp_status(cm)) {
		dev_err(cm->dev, "ibat_ucp_cnt =%d, exit cp!\n", cp->ibat_ucp_cnt);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	} else if (cm_is_taper_done(cm)) {
		dev_info(cm->dev, "taper done, exit cp machine\n");
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		cp->recovery = false;
	} else {
		dev_info(cm->dev, "cp is ok, fine tune\n");
		cm_cp_tune_algo(cm);
		if (cm_is_start_buck_charge_check(cm)) {
			if (cm_enable_buck_charge(cm))
				dev_err(cm->dev, "%s, failed to enable buck charge\n", __func__);
		} else if (cm_is_stop_buck_charge_check(cm)) {
			if (cm_disable_buck_charge(cm))
				dev_err(cm->dev, "%s, failed to disable buck charge\n", __func__);
		}
	}

	if (cp_info->cp_soft_alarm_event)
		cm_cp_clear_soft_alarm_status(cm);

	if (cp_info->cp_fault_event)
		cm_cp_clear_fault_status(cm);
}

static void cm_cp_state_exit(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;
	struct cm_charge_pump_info *cp_info = &cm->desc->cp_sm.cp_info;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
		 cp->state, cm_cp_state_names[cp->state]);

	if (!cm_cp_charger_enable(cm, false))
		return;

	if (buck_info->buck_is_running && cm_disable_buck_charge(cm)) {
		dev_err(cm->dev, "%s, failed to disable bcuk charge\n", __func__);
		return;
	}

	/* Hardreset will request 5V/2A or 5V/3A default.
	 * Disable pps will request sink-pdos PDO_FIXED value.
	 * And PDO_FIXED defined in dts is 5V/2A or 5V/3A, so
	 * we does not need requeset 5V/2A or 5V/3A when exit cp
	 */
	if (cm_fast_enable_pps(cm, false)) {
		/* At the next EXIT state, try to close pps */
		dev_err(cm->dev, "%s, failed to disable pps\n", __func__);
		return;
	}

	if (buck_info->buck_is_inited &&
	    cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_DISABLE_CMD)) {
		dev_err(cm->dev, "%s, failed to disable fchg ovp\n", __func__);
		return;
	}

	if (!cp->recovery)
		cp->running = false;

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	if (!cm->charging_status && !cm->emergency_stop) {
		cm_primary_charger_enable(cm, true);
		cm_ir_compensation_enable(cm, true);
	}

	if (cp->recovery)
		cm_cp_state_change(cm, CM_CP_STATE_RECOVERY);

	cm->desc->cm_check_fault = false;
	cm->desc->enable_fast_charge = false;
	cp_info->cp_soft_alarm_event = false;
	cp_info->cp_fault_event = false;
	cp->ibat_ucp_cnt = 0;
	cp->state_tune_log = false;
	cp->disable_step_chg_log = false;
	cp->taper_trigger_cnt = 0;
}

static int cm_cp_state_machine(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;

	dev_dbg(cm->dev, "%s, state %d, %s\n", __func__,
		cp->state, cm_cp_state_names[cp->state]);

	switch (cp->state) {
	case CM_CP_STATE_RECOVERY:
		cm_cp_state_recovery(cm);
		break;
	case CM_CP_STATE_ENTRY:
		cm_cp_state_entry(cm);
		break;
	case CM_CP_STATE_CHECK_VBUS:
		cm_cp_state_check_vbus(cm);
		break;
	case CM_CP_STATE_TUNE:
		cm_cp_state_tune(cm);
		break;
	case CM_CP_STATE_EXIT:
		cm_cp_state_exit(cm);
		break;
	case CM_CP_STATE_UNKNOWN:
	default:
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		break;
	}

	return 0;
}

static void cm_cp_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  cp_work);
	struct cm_buck_info *buck_info = &cm->desc->cp_sm.buck_info;

	if (cm->desc->cp_sm.state != CM_CP_STATE_ENTRY)
		cm_update_cp_charger_status(cm);

	if (buck_info->buck_is_running)
		cm_update_buck_status(cm);

	cm_check_cp_soft_monitor_alarm_status(cm);

	if (cm->desc->cm_check_int && cm->desc->cm_check_fault)
		cm_check_cp_fault_status(cm);

	if (cm->desc->cp_sm.running && !cm_cp_state_machine(cm))
		schedule_delayed_work(&cm->cp_work, msecs_to_jiffies(CM_CP_WORK_TIME_MS));
}

static void cm_cp_control_switch(struct charger_manager *cm, bool enable)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;

	dev_dbg(cm->dev, "%s enable = %d start\n", __func__, enable);

	if (!cm->desc->psy_cp_stat)
		return;

	if (enable) {
		cp->check_cp_threshold = enable;
	} else {
		cp->check_cp_threshold = enable;
		cp->recovery = false;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		if (cp->running) {
			cancel_delayed_work_sync(&cm->cp_work);
			cm_cp_state_machine(cm);
		}
		__pm_relax(cm->cp_ws);
	}
}

static bool cm_is_need_start_cp(struct charger_manager *cm)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	bool need = false;
	int ret;

	if (!cm->desc->support_adaptive_fchg || !cm->desc->psy_cp_stat ||
	    cp->running || cm->desc->force_pps_diasbled ||
	    cm->desc->fast_charger_type != CM_CHARGER_TYPE_ADAPTIVE)
		return false;

	if (cp->default_max_ibus <= 0 || cp->default_max_ibat <= 0) {
		dev_err(cm->dev, "%s, default_max_ibus or default_max_ibat do not exist!!!\n",
			__func__);
		return false;
	}

	/*
	 * Before starting the cp state machine, you need to turn
	 * off fixed_fchg. If the shutdown fails, the next charging
	 * cycle will be judged again.
	 */
	if (cm->desc->fixed_fchg_running) {
		cancel_delayed_work_sync(&cm->fixed_fchg_work);
		ret = cm_fixed_fchg_disable(cm);
		if (ret) {
			dev_err(cm->dev, "%s, failed to disable fixed fchg\n", __func__);
			return false;
		}
	}

	cm_charger_is_support_fchg(cm);
	dev_info(cm->dev, "%s, check_cp_threshold = %d, pps_running = %d, fast_charger_type = %d\n",
		 __func__, cp->check_cp_threshold, cp->running, cm->desc->fast_charger_type);
	if (cp->check_cp_threshold && !cp->running && cm->charger_enabled &&
	    cm_is_reach_fchg_threshold(cm))
		need = true;

	return need;
}

static void cm_start_cp_state_machine(struct charger_manager *cm, bool start)
{
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;

	if (!cp->running && start) {
		dev_info(cm->dev, "%s, reach pps threshold\n", __func__);
		cp->running = start;
		cm->desc->cm_check_fault = false;
		__pm_stay_awake(cm->cp_ws);
		cm_cp_state_change(cm, CM_CP_STATE_ENTRY);
		/* wait for the PD charger wire communication to complete */
		schedule_delayed_work(&cm->cp_work, msecs_to_jiffies(CM_CP_WORK_TIME_MS));
	}
}

static int try_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS,
						&val);
		power_supply_put(psy);
		if (err)
			return err;

		if (desc->psy_charger_stat[1])
			break;
	}

	return 0;
}

static int try_wireless_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	if (!cm->desc->psy_wl_charger_stat)
		return 0;

	for (i = 0; desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_wl_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static int try_wireless_cp_converter_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	if (!cm->desc->psy_cp_converter_stat)
		return 0;

	for (i = 0; desc->psy_cp_converter_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_cp_converter_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static int cm_set_primary_charge_wirless_type(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = 0;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	if (enable) {
		switch (cm->desc->charger_type) {
		case CM_WIRELESS_CHARGER_TYPE_BPP:
			val.intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP;
			break;
		case CM_WIRELESS_CHARGER_TYPE_EPP:
			val.intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP;
			break;
		default:
			val.intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_UNKNOWN;
		}
	} else {
		val.intval = 0;
	}

	dev_info(cm->dev, "set wirless type = %d\n", val.intval);
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_TYPE, &val);
	power_supply_put(psy);

	return ret;
}

static void try_wireless_charger_enable(struct charger_manager *cm, bool enable)
{
	int ret = 0;

	ret = cm_set_primary_charge_wirless_type(cm, enable);
	if (ret) {
		dev_err(cm->dev, "set wl type to primary charge fail, ret = %d\n", ret);
		return;
	}

	ret = try_wireless_charger_enable_by_psy(cm, enable);
	if (ret) {
		dev_err(cm->dev, "enable wl charger fail, ret = %d\n", ret);
		return;
	}

	ret = try_wireless_cp_converter_enable_by_psy(cm, enable);
	if (ret)
		dev_err(cm->dev, "enable wl charger fail, ret = %d\n", ret);
}


static int cm_set_charging_status(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret = 0;

	val.intval = enable ? POWER_SUPPLY_STATUS_CHARGING : POWER_SUPPLY_STATUS_DISCHARGING;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENOMEM;

	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_STATUS, &val);
	power_supply_put(fuel_gauge);

	return ret;
}

/**
 * try_charger_enable - Enable/Disable chargers altogether
 * @cm: the Charger Manager representing the battery.
 * @enable: true: enable / false: disable
 *
 * Note that Charger Manager keeps the charger enabled regardless whether
 * the charger is charging or not (because battery is full or no external
 * power source exists) except when CM needs to disable chargers forcibly
 * because of emergency causes; when the battery is overheated or too cold.
 */
static int try_charger_enable(struct charger_manager *cm, bool enable)
{
	int err = 0, ret = 0;

	/* Ignore if it's redundant command */
	if (enable == cm->charger_enabled)
		return 0;

	if (enable) {
		if (cm->emergency_stop)
			return -EAGAIN;

		/*
		 * Enable charge is permitted in calibration mode
		 * even if use fake battery.
		 * So it will not return in calibration mode.
		 */
		if (!is_batt_present(cm) && !allow_charger_enable)
			return 0;
		/*
		 * Save start time of charging to limit
		 * maximum possible charging time.
		 */
		cm->charging_start_time = ktime_to_ms(ktime_get_boottime());
		cm->charging_end_time = 0;

		err = try_charger_enable_by_psy(cm, enable);
		if (!err) {
			ret = cm_set_charging_status(cm, enable);
			if (ret)
				dev_err(cm->dev, "failed set charging status, ret = %d\n", ret);
		}
		mutex_lock(&cm->desc->keep_awake_mtx);
		if (!err)
			cm->charger_enabled = enable;
		if (!err && cm->desc->keep_awake) {
			dev_info(cm->dev, "acquire charger_manager_wakelock when enable charge\n");
			__pm_stay_awake(cm->charge_ws);
		}
		mutex_unlock(&cm->desc->keep_awake_mtx);
		cm_ir_compensation_enable(cm, enable);
		cm_fixed_fchg_control_switch(cm, enable);
		cm_cp_control_switch(cm, enable);
	} else {
		/*
		 * Save end time of charging to maintain fully charged state
		 * of battery after full-batt.
		 */
		cm->charging_start_time = 0;
		cm->charging_end_time = ktime_to_ms(ktime_get_boottime());
		cm_cp_control_switch(cm, enable);
		cm_fixed_fchg_disable(cm);
		cm_ir_compensation_enable(cm, enable);
		err = try_charger_enable_by_psy(cm, enable);
		if (!err) {
			ret = cm_set_charging_status(cm, enable);
			if (ret)
				dev_err(cm->dev, "failed set discharging status, ret = %d\n", ret);
		}
		mutex_lock(&cm->desc->keep_awake_mtx);
		if (!err)
			cm->charger_enabled = enable;
		cm_fixed_fchg_control_switch(cm, enable);
		if (!err && cm->desc->keep_awake) {
			dev_info(cm->dev, "Release charger_manager_wakelock when disable charge\n");
			__pm_relax(cm->charge_ws);
		}
		mutex_unlock(&cm->desc->keep_awake_mtx);
	}

	if (!err)
		power_supply_changed(cm->charger_psy);

	return err;
}

/**
 * try_charger_restart - Restart charging.
 * @cm: the Charger Manager representing the battery.
 *
 * Restart charging by turning off and on the charger.
 */
static int try_charger_restart(struct charger_manager *cm)
{
	int err;

	if (cm->emergency_stop)
		return -EAGAIN;

	err = try_charger_enable(cm, false);
	if (err)
		return err;

	return try_charger_enable(cm, true);
}

/**
 * fullbatt_vchk - Check voltage drop some times after "FULL" event.
 * @work: the work_struct appointing the function
 *
 * If a user has designated "fullbatt_vchkdrop_ms/uV" values with
 * charger_desc, Charger Manager checks voltage drop after the battery
 * "FULL" event. It checks whether the voltage has dropped more than
 * fullbatt_vchkdrop_uV by calling this function after fullbatt_vchkrop_ms.
 */
static void fullbatt_vchk(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
			struct charger_manager, fullbatt_vchk_work);
	struct charger_desc *desc = cm->desc;
	int batt_ocv, err, diff;

	/* remove the appointment for fullbatt_vchk */
	cm->fullbatt_vchk_jiffies_at = 0;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		return;

	err = get_batt_ocv(cm, &batt_ocv);
	if (err) {
		dev_err(cm->dev, "%s: get_batt_ocV error(%d)\n", __func__, err);
		return;
	}

	diff = desc->fullbatt_uV - batt_ocv;
	if (diff < 0)
		return;

	dev_info(cm->dev, "VBATT dropped %duV after full-batt\n", diff);

	if (diff >= desc->fullbatt_vchkdrop_uV)
		try_charger_restart(cm);

}

/**
 * check_charging_duration - Monitor charging/discharging duration
 * @cm: the Charger Manager representing the battery.
 *
 * If whole charging duration exceed 'charging_max_duration_ms',
 * cm stop charging to prevent overcharge/overheat. If discharging
 * duration exceed 'discharging _max_duration_ms', charger cable is
 * attached, after full-batt, cm start charging to maintain fully
 * charged state for battery.
 */
static void check_charging_duration(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	u64 curr = ktime_to_ms(ktime_get_boottime());
	u64 duration;
	int ret = false;

	if (!desc->charging_max_duration_ms && !desc->discharging_max_duration_ms)
		return;

	if (cm->charger_enabled) {
		int batt_ocv, diff;

		ret = get_batt_ocv(cm, &batt_ocv);
		if (ret) {
			dev_err(cm->dev, "failed to get battery OCV\n");
			return;
		}

		diff = desc->fullbatt_uV - batt_ocv;
		duration = curr - cm->charging_start_time;

		if (duration > desc->charging_max_duration_ms &&
		    diff < desc->fullbatt_vchkdrop_uV) {
			dev_info(cm->dev, "Charging duration exceed %ums\n",
				 desc->charging_max_duration_ms);
			cm->charging_status |= CM_CHARGE_DURATION_ABNORMAL;
			try_charger_enable(cm, false);
		}
	} else if (!cm->charger_enabled && (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL)) {
		duration = curr - cm->charging_end_time;

		if (duration > desc->discharging_max_duration_ms) {
			dev_info(cm->dev, "Discharging duration exceed %ums\n",
				 desc->discharging_max_duration_ms);
			cm->charging_status &= ~CM_CHARGE_DURATION_ABNORMAL;
		}
	}

	return;
}

static int cm_get_battery_temperature(struct charger_manager *cm, int *temp)
{
	struct power_supply *fuel_gauge;
	int ret = 0;
	int64_t temp_val;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_TEMP,
				(union power_supply_propval *)&temp_val);
	power_supply_put(fuel_gauge);

	if (ret == 0)
		*temp = (int)temp_val;
	return ret;
}

static int cm_get_board_temperature(struct charger_manager *cm, int *temp)
{
	int ret = 0;

	*temp = CM_INIT_BOARD_TEMP;
	if (!cm->desc->measure_battery_temp)
		return -ENODEV;

#if IS_ENABLED(CONFIG_THERMAL)
	if (cm->tzd_batt) {
		ret = thermal_zone_get_temp(cm->tzd_batt, temp);
		if (!ret) {
			/* Calibrate temperature unit */
			*temp /= 100;
			return ret;
		}
	}
#endif
	dev_err(cm->dev, "Can not to get board temperature, return init_temp=%d\n", *temp);

	return ret;
}

static int cm_check_thermal_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int temp, upper_limit, lower_limit;
	int ret = 0;

	ret = cm_get_board_temperature(cm, &temp);
	if (ret) {
		/* FIXME:
		 * No information of battery temperature might
		 * occur hazardous result. We have to handle it
		 * depending on battery type.
		 */
		dev_err(cm->dev, "Failed to get board temperature\n");
		return 0;
	}

	upper_limit = desc->temp_max;
	lower_limit = desc->temp_min;

	if (cm->emergency_stop) {
		upper_limit -= desc->temp_diff;
		lower_limit += desc->temp_diff;
	}

	if (temp > upper_limit)
		ret = CM_EVENT_BATT_OVERHEAT;
	else if (temp < lower_limit)
		ret = CM_EVENT_BATT_COLD;

	cm->emergency_stop = ret;
	dev_dbg(cm->dev, "%s:line%d: temp = %d, upper_limit = %d, lower_limit = %d\n",
		__func__, __LINE__, temp, upper_limit, lower_limit);

	return ret;
}

static void cm_check_charge_voltage(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int ret, charge_vol;

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop)
		return;

	mutex_lock(&cm->desc->charge_info_mtx);
	ret = get_charger_voltage(cm, &charge_vol);
	if (ret) {
		mutex_unlock(&cm->desc->charge_info_mtx);
		dev_warn(cm->dev, "Fail to get charge vol, ret = %d.\n", ret);
		return;
	}

	if (cm->charger_enabled && charge_vol > desc->charge_voltage_max) {
		dev_info(cm->dev, "Charging voltage %d is larger than %d\n",
			 charge_vol, desc->charge_voltage_max);
		cm->charging_status |= CM_CHARGE_VOLTAGE_ABNORMAL;
		mutex_unlock(&cm->desc->charge_info_mtx);
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled &&
		   charge_vol <= (desc->charge_voltage_max - desc->charge_voltage_drop) &&
		   (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL)) {
		dev_info(cm->dev, "Charging voltage %d less than %d, recharging\n",
			 charge_vol, desc->charge_voltage_max - desc->charge_voltage_drop);
		mutex_unlock(&cm->desc->charge_info_mtx);
		cm->charging_status &= ~CM_CHARGE_VOLTAGE_ABNORMAL;
	} else {
		mutex_unlock(&cm->desc->charge_info_mtx);
	}
}

static int cm_set_health_cmd(struct charger_manager *cm)
{
	int ret;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;

	val.intval = CM_GOOD_HEALTH_CMD;
	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENOMEM;

	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_HEALTH, &val);
	power_supply_put(fuel_gauge);

	return ret;
}

static void cm_check_battery_voltage(struct charger_manager *cm)
{
	int ret, batt_uV, health;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_HEALTH, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return;

	health = val.intval;

	mutex_lock(&cm->desc->charge_info_mtx);
	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get batt uV, ret=%d\n", __func__, ret);
		mutex_unlock(&cm->desc->charge_info_mtx);
		return;
	}

	if (cm->charger_enabled && (health == POWER_SUPPLY_HEALTH_OVERVOLTAGE)) {
		dev_info(cm->dev, "battery voltage too high, stop charge!!!\n");
		cm->charging_status |= CM_CHARGE_BATT_OVERVOLTAGE;
		mutex_unlock(&cm->desc->charge_info_mtx);
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled && batt_uV <= cm->desc->constant_charge_voltage_max_uv &&
		   (cm->charging_status & CM_CHARGE_BATT_OVERVOLTAGE)) {
		ret = cm_set_health_cmd(cm);
		if (ret) {
			dev_err(cm->dev, "failed to set health cmd, ret=%d\n", ret);
			mutex_unlock(&cm->desc->charge_info_mtx);
			return;
		}

		dev_info(cm->dev, "battery voltage %d less than %d, recharging\n",
			 batt_uV, cm->desc->constant_charge_voltage_max_uv);
		mutex_unlock(&cm->desc->charge_info_mtx);
		cm->charging_status &= ~CM_CHARGE_BATT_OVERVOLTAGE;
	} else {
		mutex_unlock(&cm->desc->charge_info_mtx);
	}
}

static void cm_check_charge_health(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int health = POWER_SUPPLY_HEALTH_UNKNOWN;
	int ret, i;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
		power_supply_put(psy);
		if (ret)
			return;
		health = val.intval;
	}

	if (health == POWER_SUPPLY_HEALTH_UNKNOWN)
		return;

	if (cm->charger_enabled && health != POWER_SUPPLY_HEALTH_GOOD) {
		dev_info(cm->dev, "Charging health is not good\n");
		cm->charging_status |= CM_CHARGE_HEALTH_ABNORMAL;
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled && health == POWER_SUPPLY_HEALTH_GOOD &&
		   (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL)) {
		dev_info(cm->dev, "Charging health is recover good\n");
		cm->charging_status &= ~CM_CHARGE_HEALTH_ABNORMAL;
	}
}

static bool cm_manager_adjust_current(struct charger_manager *cm, int jeita_status)
{
	struct charger_desc *desc = cm->desc;
	struct cm_cp_state_machine *cp = &cm->desc->cp_sm;
	int term_volt, target_cur;

	if (jeita_status > desc->jeita_tab_size)
		jeita_status = desc->jeita_tab_size;

	if (jeita_status == 0 || jeita_status == desc->jeita_tab_size) {
		dev_warn(cm->dev,
			 "stop charging due to battery overheat or cold\n");

		if (jeita_status == 0) {
			cm->charging_status &= ~CM_CHARGE_TEMP_OVERHEAT;
			cm->charging_status |= CM_CHARGE_TEMP_COLD;
		} else {
			cm->charging_status &= ~CM_CHARGE_TEMP_COLD;
			cm->charging_status |= CM_CHARGE_TEMP_OVERHEAT;
		}
		return false;
	}

#if IS_ENABLED(CONFIG_FACTORY_BUILD)
	if (desc->jeita_tab[jeita_status].term_volt == 4100000)
		desc->jeita_tab[jeita_status].term_volt = 4000000;
#endif

	term_volt = desc->jeita_tab[jeita_status].term_volt;
	target_cur = desc->jeita_tab[jeita_status].current_ua;

	cm->desc->ir_comp.us = term_volt;
	cm->desc->ir_comp.us_lower_limit = term_volt;

	if (cp->running && !cm_check_primary_charger_enabled(cm)) {
		dev_info(cm->dev, "%s, jeita_status: %d, jeita_vbat: %d, jeita_ibat: %dn",
			 __func__, jeita_status, term_volt, target_cur);
		cp->jeita_status = jeita_status;
		cp->jeita_ibat = target_cur;
		cp->jeita_vbat = term_volt;
		goto done;
	}

	if (cm->desc->charger_type == CM_CHARGER_TYPE_SDP && r_items_param != r_items_param_single)
		term_volt -= 16000;

	dev_info(cm->dev, "target terminate voltage = %d, target current = %d\n",
		 term_volt, target_cur);

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_CCCV,
				 SPRD_VOTE_TYPE_CCCV_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 term_volt, cm);

	if (cm->desc->charger_type == CM_CHARGER_TYPE_SDP && r_items_param != r_items_param_single) {
		cm->desc->fullbatt_uA = SDP_FULLBAT_UA;
		cm->desc->jeita_charge_term_ma = SDP_ITERM_MA;
	} else {
		cm->desc->fullbatt_uA = cm->desc->origin_fullbatt_uA;
		cm->desc->jeita_charge_term_ma = cm->desc->charge_term_ua / 1000;
	}
	if (term_volt <= 4100000)
		cm->desc->jeita_charge_term_ma += 120;
	dev_info(cm->dev, "target terminate current = %dmA, fullbatt_uA = %duA\n", cm->desc->jeita_charge_term_ma,
									cm->desc->fullbatt_uA);
	charger_set_iterm(cm->charger, cm->desc->jeita_charge_term_ma);
done:
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 target_cur, cm);

	cm->charging_status &= ~(CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD);
	return true;
}

static void cm_jeita_temp_goes_down(struct charger_desc *desc, int status,
				    int recovery_status, int *jeita_status)
{
	if (recovery_status == desc->jeita_tab_size) {
		if (*jeita_status >= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (desc->jeita_tab[recovery_status].temp > desc->jeita_tab[recovery_status].recovery_temp) {
		if (*jeita_status >= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (*jeita_status >= status)
		*jeita_status = status;
}

static void cm_jeita_temp_goes_up(struct charger_desc *desc, int status,
				  int recovery_status, int *jeita_status)
{
	if (recovery_status == desc->jeita_tab_size) {
		if (*jeita_status <= status)
			*jeita_status = status;
		return;
	}

	if (desc->jeita_tab[recovery_status].temp < desc->jeita_tab[recovery_status].recovery_temp) {
		if (*jeita_status <= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (*jeita_status <= status)
		*jeita_status = status;
}

static void jeita_info_init(struct cm_jeita_info *jeita_info)
{
	jeita_info->temp_up_trigger = 0;
	jeita_info->temp_down_trigger = 0;
	jeita_info->jeita_changed = true;
	jeita_info->jeita_status = 4;
	jeita_info->jeita_temperature = 250;
}

static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp)
{
	struct charger_desc *desc = cm->desc;
	struct cm_jeita_info *jeita_info = &desc->jeita_info;
	int i, jeita_status, temp_status, recovery_temp_status = -1;

	jeita_status = jeita_info->jeita_status;

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
#if IS_ENABLED(CONFIG_FACTORY_BUILD)
		if (desc->jeita_tab[i].temp == 450 )
			desc->jeita_tab[i].temp = 430;
#endif
		if ((cur_temp >= desc->jeita_tab[i].temp && i > 0) ||
		    (cur_temp > desc->jeita_tab[i].temp && i == 0)) {
			break;
		}
	}

	temp_status = i + 1;

	if (temp_status == desc->jeita_tab_size) {
		jeita_status = desc->jeita_tab_size;
		recovery_temp_status = desc->jeita_tab_size;
		goto out;
	} else if (temp_status == 0) {
		jeita_status = 0;
		recovery_temp_status = 0;
		goto out;
	}

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
#if IS_ENABLED(CONFIG_FACTORY_BUILD)
		if (desc->jeita_tab[i].recovery_temp == 430)
			desc->jeita_tab[i].recovery_temp = 410;
#endif
		if ((cur_temp > desc->jeita_tab[i].recovery_temp && i > 0) ||
		    (cur_temp >= desc->jeita_tab[i].recovery_temp && i == 0)) {
			break;
		}
	}

	recovery_temp_status = i + 1;

	if (jeita_info->jeita_changed) {
		jeita_status = 4;
		jeita_info_init(&desc->jeita_info);
		dev_info(cm->dev, "%s: jeita_changed= %d\n", __func__,
			 jeita_info->jeita_changed);
	}

	/* temperature goes down */
	if (jeita_info->jeita_temperature > cur_temp)
		cm_jeita_temp_goes_down(desc, temp_status, recovery_temp_status, &jeita_status);
	/* temperature goes up */
	else
		cm_jeita_temp_goes_up(desc, temp_status, recovery_temp_status, &jeita_status);

out:
	dev_info(cm->dev, "%s: jeita status:(%d) %d %d, temperature:%d, jeita_size:%d\n",
		 __func__, jeita_status, temp_status, recovery_temp_status,
		 cur_temp, desc->jeita_tab_size);

	return jeita_status;
}

/**
 * cm_charger_type_polling - Polling charger type.
 * @cm: the Charger Manager representing the battery.
 */
static int cm_charger_type_polling(struct charger_manager *cm)
{
	int ret;
	u32 type;

	if (!is_ext_usb_pwr_online(cm))
		return 0;

	if (cm->desc->is_fast_charge)
		return 0;

	if (cm->desc->charger_type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
		return 0;

	if (cm->bat_id)
		ret = cm_get_bc1p2_type(cm, &type);
	else
		ret = charger_get_vbus_type(cm->charger, &type);
	if (!ret && type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
		cm->vchg_info->charger_type_cnt++;

	if (cm->vchg_info->charger_type_cnt > 1) {
		dev_info(cm->dev, "%s: update charger type:%d\n", __func__, type);
		cm->desc->charger_type = type;
		cm_update_charger_type_status(cm);
		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT |
					   CM_CHARGE_INFO_THERMAL_LIMIT |
					   CM_CHARGE_INFO_JEITA_LIMIT));
		power_supply_changed(cm->charger_psy);
	}

	return ret;
}

static int dw3_msm_get_usb_device_state(struct charger_manager *cm, int *state)
{
	struct device_node *np;
	struct platform_device *pdev;
	struct sprd_glue *glue;

	np = of_find_compatible_node(NULL, NULL, "sprd,qogirl6-musb");
	if (!np) {
		dev_err(cm->dev, "%s: can not find udc node!\n", __func__);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_err(cm->dev, "%s: can not find udc platform device!\n", __func__);
		return -ENODEV;
	}

	glue = platform_get_drvdata(pdev);
	if (!glue || !glue->musb) {
		dev_err(cm->dev, "%s: glue or musb NULL!\n", __func__);
		return -ENODEV;
	}

	*state = glue->musb->g.state;
	dev_info(cm->dev, "%s:get gadget state:%d\n", __func__, *state);

	return 0;
}

static void cm_charger_type_update_check_start(struct charger_manager *cm)
{
	if (cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_UNKNOWN &&
	    cm->desc->charge_type_poll_count < CM_CHARGER_TYPE_TIME_OUT_CNT) {
		cm->desc->charge_type_poll_count++;
		schedule_delayed_work(&cm->charger_type_update_work,
				      msecs_to_jiffies(CM_CHARGER_TYPE_WORK_TIME_MS));
	} else if ((cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_DCP) ||
				(cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_ACA)) {
			schedule_delayed_work(&cm->power_detect_work, msecs_to_jiffies(1000));
	} else if ((cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_SDP) ||
			   (cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)) {
			dev_err(cm->dev, "%s %d chg_type = %d\n", __func__, __LINE__, cm->desc->charger_type);
			schedule_delayed_work(&cm->dcd_work, msecs_to_jiffies(CM_DCD_WORK_FIRST_TIME_5S));
	}
}

static void cm_charger_type_update_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  charger_type_update_work);

	cm_charger_type_polling(cm);
	cm_charger_type_update_check_start(cm);
}
static bool charge_vdpm_state(struct charger_manager *cm)
{
	bool stat = false;

	charger_get_vindpm_state(cm->charger, &stat);
	return (stat ? true : false);
}

static bool charge_power_ico_trigered(struct charger_manager *cm, int *vbus_uv)
{
	bool vdpm_state = false;
	bool vbus_state = false;
	int ret = -ENODEV;
	// int vindpm = cm->desc->constant_charge_voltage_max_uv;
	vdpm_state = charge_vdpm_state(cm);
	ret = get_charger_voltage(cm, vbus_uv);
	if (ret) {
		dev_err(cm->dev, "%s, fail to get charge vbus, ret =%d \n", __func__, ret);
		return ret;
	}
	if ((*vbus_uv/1000) <= cm->vindpm_voltage_mv)
		vbus_state = true;
	dev_info(cm->dev, "vdpm_state=%s, vbus_state=%s, vbus=%dmv, vindpm_voltage_mv=%dmv\n",
			vdpm_state ? "true" : "false",
			vbus_state ? "true" : "false",
			*vbus_uv/1000, cm->vindpm_voltage_mv);
	return (vdpm_state || vbus_state);
}

static int charge_set_adjust_vindpm(struct charger_manager *cm)
{
	int batt_uV, vindpm, ret;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		return ret;
	}

	if (batt_uV < VBAT_4100) {
		vindpm = VINDPM_4400;
	} else if (batt_uV >= VBAT_4100 && batt_uV < VBAT_4300) {
		vindpm = VINDPM_4500;
	} else if (batt_uV >= VBAT_4300) {
		vindpm = VINDPM_4600;
	} else {
		vindpm = VINDPM_4500;
	}

	cm->vindpm_voltage_mv = vindpm;
	charger_set_vindpm(cm->charger, vindpm);

	return 0;
}

static void charge_power_detect_work(struct work_struct *work)
{
	struct charger_manager *cm =
		container_of(work, struct charger_manager, power_detect_work.work);
	int ico_trioggered = 0;
	int iindpm = POWER_DETECT_ILIMIT_2000MA;
	int cnt = 0, cnt_max = 20;
	int vbus_uv = 0;
	int i = 0, ret = 0;
	int last_vbus_uv = 0;
	bool sc_device = false;

	// if (chip->drv_state == CHARGER_DRV_SHUTDOWN || chip->drv_state == CHARGER_DRV_REMOVE) {
	// 	dev_info(cm->dev,"charger driver state: %s, skip\n",
	// 			map_state_string(chip->drv_state, dirver_state_strings, ARRAY_SIZE(dirver_state_strings)));
	// 	return;
	// }
	if (!cm->usb_present) {
		dev_err(cm->dev, "usb not present\n");
		return;
	}
	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_FAST) {
		dev_err(cm->dev,"pd is already detected, no need ico detect\n");
		return;
	}
	if (cm->input_suspend) {
		dev_err(cm->dev, "input_suspend is 1\n");
		return;
	}
	if (!cm->bat_id) {
		dev_err(cm->dev, "non-standard battery\n");
		return;
	}

	if (cm->desc->psy_charger_stat[0]) {
		if (strcmp(cm->desc->psy_charger_stat[0], "sc89601_charger") == 0)
			sc_device = true;
	}

	charge_set_adjust_vindpm(cm);
	// charge_power_det_config(chip);
	if (!cm->usb_present)
		goto out;
	// if (is_atboot && chip->real_type == POWER_SUPPLY_TYPE_USB_DCP) {
	// 	vote(chip->usb_icl_votable, ICO_VOTER, true, ICO_AT_CURRENT_2000MA);
	// 	dev_info(cm->dev, "AT mode skip power detect,set 2000ma for DCP.\n");
	// 	goto out;
	// }
	cm->desc->input_limit_cur = POWER_DETECT_ILIMIT_1500MA;
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				SPRD_VOTE_TYPE_IBUS,
				SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
				SPRD_VOTE_CMD_MIN, cm->desc->input_limit_cur, cm);
	msleep(500); // step1, keep 500ms
	if (charge_power_ico_trigered(cm, &vbus_uv)) {
		cm->desc->input_limit_cur = POWER_DETECT_ILIMIT_1000MA;
		ico_trioggered = true;
		goto out;
	}
	cm->desc->input_limit_cur = POWER_DETECT_ILIMIT_2000MA;
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				SPRD_VOTE_TYPE_IBUS,
				SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
				SPRD_VOTE_CMD_MIN, cm->desc->input_limit_cur, cm);
	msleep(500); // step2, keep 500ms
	if (charge_power_ico_trigered(cm, &vbus_uv)) {
		cm->desc->input_limit_cur = POWER_DETECT_ILIMIT_1500MA;
		ico_trioggered = true;
		goto out;
	}
	do {
		iindpm += POWER_DETECT_STEP_100MA;
		cm->desc->input_limit_cur = iindpm;
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, cm->desc->input_limit_cur, cm);

		if (iindpm < POWER_DETECT_ILIMIT_2500MA && iindpm >= POWER_DETECT_ILIMIT_2200MA) {
			for (i = 0; i < 10; i++) {
				ret = get_charger_voltage(cm, &vbus_uv);
				if (ret) {
					dev_err(cm->dev, "%s, fail to get charge vbus, ret =%d \n", __func__, ret);
					cm->desc->input_limit_cur = POWER_DETECT_ILIMIT_2000MA;
					break;
				}
				if (last_vbus_uv && (last_vbus_uv - vbus_uv > 100000)) {
					dev_info(cm->dev, "vbus:%d, iindpm:%d last_vbus:%d count:%d\n", vbus_uv, iindpm, last_vbus_uv, i);
					iindpm = POWER_DETECT_ILIMIT_1000MA;
					cm->desc->input_limit_cur = iindpm;
					ico_trioggered = true;
					cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
								SPRD_VOTE_TYPE_IBUS,
								SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
								SPRD_VOTE_CMD_MIN, cm->desc->input_limit_cur, cm);
					break;
				}
				last_vbus_uv = vbus_uv;
				if(!sc_device)
					msleep(2);
			}
			msleep(180);
		} else {
			msleep(200);
		}
		if (iindpm == POWER_DETECT_ILIMIT_2000MA)
			break;
		if (charge_power_ico_trigered(cm, &vbus_uv) && !ico_trioggered) {
			iindpm = POWER_DETECT_ILIMIT_1000MA;
			cm->desc->input_limit_cur = iindpm;
			ico_trioggered = true;
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
						SPRD_VOTE_TYPE_IBUS,
						SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
						SPRD_VOTE_CMD_MIN, cm->desc->input_limit_cur, cm);
			msleep(200);
		}
	} while (iindpm < POWER_DETECT_ILIMIT_3000MA && cnt++ <= cnt_max);
out:
	if ((vbus_uv < VBUS_INT_VOL_3300MV) && cm->usb_present) {
		dev_info(cm->dev,"Hard reset, need try again\n");
		schedule_delayed_work(&cm->power_detect_work, msecs_to_jiffies(3000));
		return;
	}
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				SPRD_VOTE_TYPE_IBUS,
				SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
				SPRD_VOTE_CMD_MIN, cm->desc->input_limit_cur, cm);
	schedule_delayed_work(&cm->adjust_dpm_work, msecs_to_jiffies(10000));
	dev_info(cm->dev,"input_limit_cur=%d, ico trioggered=%d\n", cm->desc->input_limit_cur, ico_trioggered);
	return;
}


static void charge_adjust_dpm_work(struct work_struct *work)
{

	struct charger_manager *cm =
		container_of(work, struct charger_manager, adjust_dpm_work.work);

	if (!cm->usb_present) {
		dev_err(cm->dev, "usb not present\n");
		return;
	}

	charge_set_adjust_vindpm(cm);

	schedule_delayed_work(&cm->adjust_dpm_work, msecs_to_jiffies(10000));
}

static void charge_dcd_work(struct work_struct *work) {
	int bc1p2_result = 0;
	int usb_state = 0;
	struct charger_manager *cm = container_of(work, struct charger_manager, dcd_work.work);

	dev_err(cm->dev, "%s start, usb_conndoned = %d\n", __func__, cm->usb_conndoned);
	if (!cm) {
		dev_err(cm->dev, "get cm fail!\n");
		return;
	}

	if (!cm->usb_present) {
		dev_err(cm->dev, "usb not present\n");
		return;
	}

	if (!cm->usb_conndoned) {
		dw3_msm_get_usb_device_state(cm, &usb_state);
		dev_info(cm->dev, "%s: usb_state:%d\n", __func__, usb_state);
		if (usb_state == USB_STATE_CONFIGURED) {
			cm->usb_conndoned = true;
			return;
		}
	}

	charger_retry_bc1p2(cm->charger, &bc1p2_result);

	if (!cm->usb_present) {
		dev_err(cm->dev, "after bc1p2, usb not present\n");
		return;
	}

	cm->desc->charger_type = bc1p2_result;
	cm->vchg_info->usb_phy->chg_type = bc1p2_result;
	cm_update_charger_type_status(cm);
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT |
					   CM_CHARGE_INFO_THERMAL_LIMIT |
					   CM_CHARGE_INFO_JEITA_LIMIT));
	power_supply_changed(cm->charger_psy);

	if ((cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_DCP) ||
				(cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_ACA)) {
		schedule_delayed_work(&cm->power_detect_work, msecs_to_jiffies(100));
	}

  	cm->desc->dcd_work_count++;
	if ((cm->desc->dcd_work_count < CM_DCD_TIME_OUT_CNT) &&
				((cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_SDP) ||
                                 (cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_C)  ||
				(cm->desc->charger_type == POWER_SUPPLY_USB_TYPE_UNKNOWN))) {
		schedule_delayed_work(&cm->dcd_work, msecs_to_jiffies(CM_DCD_WORK_TIME_10S));
	}
	dev_info(cm->dev, "dcd_work, charger_type=%d, retry:%d\n", cm->desc->charger_type, cm->desc->dcd_work_count);

}

/**
 * cm_get_target_status - Check current status and get next target status.
 * @cm: the Charger Manager representing the battery.
 */
static int cm_get_target_status(struct charger_manager *cm)
{
	bool is_normal = true;

	if (!is_ext_pwr_online(cm))
		return POWER_SUPPLY_STATUS_DISCHARGING;

	/*
	 * Adjust the charging current according to current battery
	 * temperature jeita table.
	 */
	is_normal = cm_update_current_jeita_status(cm);
	if (!is_normal)
		dev_warn(cm->dev, "Errors orrurs when adjusting charging current\n");

	if (!is_batt_present(cm) && !allow_charger_enable)
		return POWER_SUPPLY_STATUS_DISCHARGING;

	if (cm_check_thermal_status(cm)) {
		dev_warn(cm->dev, "board temperature is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	if (cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD)) {
		dev_warn(cm->dev, "battery overheat or cold is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_charge_health(cm);
	if (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL) {
		dev_warn(cm->dev, "Charging health is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_charge_voltage(cm);
	if (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL) {
		dev_warn(cm->dev, "Charging voltage is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_battery_voltage(cm);
	if (cm->charging_status & CM_CHARGE_BATT_OVERVOLTAGE) {
		dev_warn(cm->dev, "battery over voltage is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	check_charging_duration(cm);
	if (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL) {
		dev_warn(cm->dev, "Charging duration is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	if (is_full_charged(cm)) {
		cm->battery_status = POWER_SUPPLY_STATUS_FULL;
		dev_info(cm->dev, "battery is full charged\n");
		return POWER_SUPPLY_STATUS_FULL;
	}

	if (cm->desc->xts_limit_cur) {
		dev_info(cm->dev, "xts limit cur is still working\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

#if IS_ENABLED(CONFIG_XM_SMART_CHG)
	if (cm->nig_stop_flag || cm->nav_stop_flag || cm->pro_stop_flag)
	{
		dev_info(cm->dev, "[SMART_CHG] disable charger ni:%d, na:%d, pro:%d\n", cm->nig_stop_flag, \
							cm->nav_stop_flag, cm->pro_stop_flag);
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
#endif

	if(cm->input_suspend)
	{
		dev_info(cm->dev, "disable charger by input_suspend\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	/* Charging is allowed. */
	return POWER_SUPPLY_STATUS_CHARGING;
}

/**
 * _cm_monitor - Monitor the temperature and return true for exceptions.
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if there is an event to notify for the battery.
 * (True if the status of "emergency_stop" changes)
 */
static bool _cm_monitor(struct charger_manager *cm)
{
	int i, target;
	static int last_target = -1;

	for (i = 0; i < cm->desc->num_sysfs; i++) {
		if (cm->desc->sysfs[i].externally_control) {
			dev_info(cm->dev, "Charger has been controlled externally, so no need monitoring\n");
			last_target = -1;
			return false;
		}
	}

	target = cm_get_target_status(cm);

	if (target == POWER_SUPPLY_STATUS_CHARGING) {
		cm->emergency_stop = 0;
		cm->charging_status = 0;

		try_charger_enable(cm, true);

		if (!cm->desc->cp_sm.running && !cm_check_primary_charger_enabled(cm)
		    && !cm->desc->force_set_full) {
			dev_info(cm->dev, "%s, primary charger does not enable,enable it\n", __func__);
			cm_primary_charger_enable(cm, true);
		}

		if (cm_is_need_start_cp(cm))
			cm_start_cp_state_machine(cm, true);
		else if (!cm->desc->cp_sm.running && cm_is_need_start_fixed_fchg(cm))
			cm_start_fixed_fchg(cm, true);
	} else {
		try_charger_enable(cm, false);
	}

	if (last_target != target) {
		last_target = target;
		power_supply_changed(cm->charger_psy);
	}

	dev_info(cm->dev, "target %d, charging_status %d\n", target, cm->charging_status);
	return (target == POWER_SUPPLY_STATUS_NOT_CHARGING);

}

/**
 * cm_monitor - Monitor every battery.
 *
 * Returns true if there is an event to notify from any of the batteries.
 * (True if the status of "emergency_stop" changes)
 */
static bool cm_monitor(void)
{
	bool stop = false;
	struct charger_manager *cm;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (_cm_monitor(cm))
			stop = true;
	}

	mutex_unlock(&cm_list_mtx);

	return stop;
}

/**
 * _setup_polling - Setup the next instance of polling.
 * @work: work_struct of the function _setup_polling.
 */
static void _setup_polling(struct work_struct *work)
{
	unsigned long min = ULONG_MAX;
	struct charger_manager *cm;
	bool keep_polling = false;
	unsigned long _next_polling;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (is_polling_required(cm) && cm->desc->polling_interval_ms) {
			keep_polling = true;

			if (min > cm->desc->polling_interval_ms)
				min = cm->desc->polling_interval_ms;
		}
	}

	polling_jiffy = msecs_to_jiffies(min);
	if (polling_jiffy <= CM_JIFFIES_SMALL)
		polling_jiffy = CM_JIFFIES_SMALL + 1;

	if (!keep_polling)
		polling_jiffy = ULONG_MAX;
	if (polling_jiffy == ULONG_MAX)
		goto out;

	WARN(cm_wq == NULL, "charger-manager: workqueue not initialized"
			    ". try it later. %s\n", __func__);

	/*
	 * Use mod_delayed_work() iff the next polling interval should
	 * occur before the currently scheduled one.  If @cm_monitor_work
	 * isn't active, the end result is the same, so no need to worry
	 * about stale @next_polling.
	 */
	_next_polling = jiffies + polling_jiffy;

	if (time_before(_next_polling, next_polling)) {
		mod_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy);
		next_polling = _next_polling;
	} else {
		if (queue_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy))
			next_polling = _next_polling;
	}
out:
	mutex_unlock(&cm_list_mtx);
}
static DECLARE_WORK(setup_polling, _setup_polling);

/**
 * cm_monitor_poller - The Monitor / Poller.
 * @work: work_struct of the function cm_monitor_poller
 *
 * During non-suspended state, cm_monitor_poller is used to poll and monitor
 * the batteries.
 */
static void cm_monitor_poller(struct work_struct *work)
{
	cm_monitor();
	schedule_work(&setup_polling);
}

/**
 * fullbatt_handler - Event handler for CM_EVENT_BATT_FULL
 * @cm: the Charger Manager representing the battery.
 */
static void fullbatt_handler(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		goto out;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	mod_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
			 msecs_to_jiffies(desc->fullbatt_vchkdrop_ms));
	cm->fullbatt_vchk_jiffies_at = jiffies + msecs_to_jiffies(
				       desc->fullbatt_vchkdrop_ms);

	if (cm->fullbatt_vchk_jiffies_at == 0)
		cm->fullbatt_vchk_jiffies_at = 1;

out:
	dev_info(cm->dev, "EVENT_HANDLE: Battery Fully Charged\n");
}

/**
 * battout_handler - Event handler for CM_EVENT_BATT_OUT
 * @cm: the Charger Manager representing the battery.
 */
static void battout_handler(struct charger_manager *cm)
{
	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (!is_batt_present(cm)) {
		if(cm->desc->temperature <= -200){
			dev_emerg(cm->dev, "Battery Pulled Out!\n");
			try_charger_enable(cm, false);
			kernel_power_off();
		}
	} else {
		dev_emerg(cm->dev, "Battery Pulled in!\n");

		if (cm->charging_status) {
			dev_emerg(cm->dev, "Charger status abnormal, stop charge!\n");
			try_charger_enable(cm, false);
		} else {
			try_charger_enable(cm, true);
		}
	}
}

static bool cm_charger_is_support_fchg(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	u32 fchg_type;
	int ret;

	if (!cm->fchg_info->support_fchg || !cm->fchg_info->ops ||
	    !cm->fchg_info->ops->get_fchg_type)
		return false;

	ret = cm->fchg_info->ops->get_fchg_type(cm->fchg_info, &fchg_type);
	if (!ret) {
		if (fchg_type == POWER_SUPPLY_CHARGE_TYPE_FAST ||
		    fchg_type == POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE) {
			mutex_lock(&cm->desc->charger_type_mtx);
			desc->is_fast_charge = true;
			if (!desc->psy_cp_stat &&
			    fchg_type == POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE) {
				fchg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
				cm->fchg_info->ops->force_set_fixed_fchg_type(cm->fchg_info);
			}

			cm_get_charger_type(cm, CM_FCHG_TYPE, &fchg_type);
			desc->fast_charger_type = fchg_type;
			desc->charger_type = fchg_type;
			mutex_unlock(&cm->desc->charger_type_mtx);
			return true;
		}
	}

	return false;
}

static void cm_charger_int_handler(struct charger_manager *cm)
{
	dev_info(cm->dev, "%s\n", __func__);
	cm->desc->cm_check_int = true;
}

static int cm_charger_pd_limit_current(struct charger_manager *cm)
{
	int ret;
	int max_vol, max_cur;

	if (!cm->fchg_info->support_fchg || !cm->fchg_info->pd_enable ||
	    !cm->fchg_info->ops || !cm->fchg_info->ops->get_fchg_vol_max ||
	    !cm->fchg_info->ops->get_fchg_cur_max)
		return -EINVAL;

	ret = cm->fchg_info->ops->get_fchg_vol_max(cm->fchg_info, &max_vol);
	if (ret)
		dev_err(cm->dev, "%s, failed to get fchg max voltage, ret=%d\n", __func__, ret);

	dev_dbg(cm->dev, "%s:max_vol = %d\n", __func__, max_vol);
	if (max_vol > 5000000) {
		dev_dbg(cm->dev, "%s:max_vol = %d\n", __func__, max_vol);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->get_fchg_cur_max(cm->fchg_info, max_vol, &max_cur);
	if (ret)
		dev_err(cm->dev, "%s, failed to get fchg max current, ret=%d\n", __func__, ret);

	dev_dbg(cm->dev, "%s:max_cur = %d\n", __func__, max_cur);
	if (max_cur == CM_LIMIT_POWER_TRANSFER_MA * 1000 && max_cur > 0) {
		cm->desc->xts_limit_cur = true;
		try_charger_enable(cm, false);
		cm_power_path_enable(cm, CM_POWER_PATH_DISABLE_CMD);
		dev_info(cm->dev, "%s:line%d limit cur\n", __func__, __LINE__);
		return 1;
	} else if (cm->desc->xts_limit_cur && max_cur <= 1000000 &&
		   max_cur != CM_LIMIT_POWER_TRANSFER_MA * 1000) {
		cm->desc->xts_limit_cur = false;
		try_charger_enable(cm, true);
		cm_power_path_enable(cm, CM_POWER_PATH_ENABLE_CMD);
		dev_info(cm->dev, "%s:line%d not limit cur\n", __func__, __LINE__);
		return 1;
	}

	return 0;
}


/**
 * fast_charge_handler - Event handler for CM_EVENT_FAST_CHARGE
 * @cm: the Charger Manager representing the battery.
 */
static void fast_charge_handler(struct charger_manager *cm)
{
	bool ext_pwr_online;
	int ret, adapter_max_vbus;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	cancel_delayed_work_sync(&cm->charger_type_update_work);

	cm_charger_is_support_fchg(cm);
	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_FAST ||
	    cm->desc->fast_charger_type == CM_CHARGER_TYPE_ADAPTIVE) {
		ret = cm_get_fchg_adapter_max_voltage(cm, &adapter_max_vbus);
		if (ret)
			dev_err(cm->dev, "%s, failed to obtain the adapter max voltage, ret=%d\n",
				__func__, ret);
		else
			cm->desc->adapter_max_vbus = adapter_max_vbus;
	}

	if (cm_charger_pd_limit_current(cm) > 0) {
		dev_info(cm->dev, "%s, xts update limit cur\n", __func__);
		return;
	}
	ext_pwr_online = is_ext_pwr_online(cm);

	dev_info(cm->dev, "%s, fast_charger_type = %d, cp_running = %d, "
		 "charger_enabled = %d, ext_pwr_online = %d\n",
		 __func__, cm->desc->fast_charger_type, cm->desc->cp_sm.running,
		 cm->charger_enabled, ext_pwr_online);

	if (!ext_pwr_online)
		return;

	cm_update_charger_type_status(cm);

	if (cm->desc->is_fast_charge && !cm->desc->enable_fast_charge)
		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT |
					   CM_CHARGE_INFO_JEITA_LIMIT));

	/*
	 * Once the fast charge is identified, it is necessary to open
	 * the charge in the first time to avoid the fast charge to boost
	 * the voltage in the next charging cycle, especially the SFCP
	 * fast charge.
	 */
	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_FAST &&
	    cm->charger_enabled)
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);

	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_ADAPTIVE &&
	    !cm->desc->cp_sm.running && cm->charger_enabled) {
		cm_cp_control_switch(cm, true);
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);
	}
}

static void cm_pd_negotiated_init_cfg(struct charger_manager *cm)
{
	cancel_delayed_work_sync(&cm->limit_current_work);

	if (cm->desc->pd_negotiated_limit_cur)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_PD_NEGOIIATED_LIMIT,
					 SPRD_VOTE_CMD_MIN, 0, cm);

	cm->desc->pd_negotiated_limit_cur = false;
}

/**
 * misc_event_handler - Handler for other events
 * @cm: the Charger Manager representing the battery.
 * @type: the Charger Manager representing the battery.
 */
static void misc_event_handler(struct charger_manager *cm, enum cm_event_types type)
{
	int ret;
	int ui_soc = 0;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (is_ext_pwr_online(cm)) {
		if (cm->erp_config) {
			cm_get_uisoc(cm, &ui_soc);
			if (ui_soc == 100)
				cm->uisoc_100_adapter_plugin = true;
		}
		cm_set_charger_present(cm, true);
		cm->usb_present = true;
          	cm->desc->dcd_work_count = 0;
		cm->usb_conndoned = false;
		if (is_ext_wl_pwr_online(cm)) {
			if (cm->desc->usb_charge_en) {
				cm_enable_fixed_fchg_handshake(cm, false);
				try_charger_enable(cm, false);
				cm->desc->force_pps_diasbled = false;
				cm->desc->is_fast_charge = false;
				cm->desc->enable_fast_charge = false;
				cm->desc->fchg_voltage_check_count = 0;
				cm->desc->fast_charge_enable_count = 0;
				cm->desc->fast_charge_disable_count = 0;
				cm->desc->fixed_fchg_running = false;
				cm->desc->wait_vbus_stable = false;
				cm->desc->cp_sm.running = false;
				cm->desc->fast_charger_type = 0;
				cm->desc->usb_charge_en = false;
				cm->desc->charger_type = 0;
			}

			ret = get_wireless_charger_type(cm, &cm->desc->charger_type);
			if (ret)
				dev_warn(cm->dev, "Fail to get wl charger type, ret = %d\n", ret);

			try_wireless_charger_enable(cm, true);
			cm->desc->wl_charge_en = true;
		} else {
			if (cm->desc->wl_charge_en) {
				try_wireless_charger_enable(cm, false);
				try_charger_enable(cm, false);
				cm->desc->wl_charge_en = false;
			}

			if (!cm->desc->is_fast_charge) {
				ret = get_usb_charger_type(cm, &cm->desc->charger_type);
				if (ret)
					dev_warn(cm->dev, "Fail to get usb charger type, ret = %d",
						 ret);

				cm_enable_fixed_fchg_handshake(cm, true);
			}

			cm->desc->usb_charge_en = true;
		}

		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT |
					   CM_CHARGE_INFO_THERMAL_LIMIT |
					   CM_CHARGE_INFO_JEITA_LIMIT));
		dev_err(cm->dev, "%s %d usb_conndoned= %d\n", __func__, __LINE__, cm->usb_conndoned);
		cm_charger_type_update_check_start(cm);
	} else {
		cm->desc->fullbatt_uA = cm->desc->origin_fullbatt_uA;
		cm->desc->jeita_charge_term_ma = cm->desc->charge_term_ua / 1000;
		charger_set_iterm(cm->charger, cm->desc->jeita_charge_term_ma);
		if (cm->desc->xts_limit_cur)
			cm_power_path_enable(cm, CM_POWER_PATH_ENABLE_CMD);

		cm_pd_negotiated_init_cfg(cm);
		try_wireless_charger_enable(cm, false);
		cm_enable_fixed_fchg_handshake(cm, false);
		cm->usb_present = false;
          	cm->desc->dcd_work_count = 0;
		try_charger_enable(cm, false);
		cm->charger_enabled = false;
		cm->desc->thm_info.thm_adjust_cur = -EINVAL;
		cm->desc->rp_limit_current = -EINVAL;
		if (cm->vchg_info->ops && cm->vchg_info->ops->update_vchg_info) {
			cancel_delayed_work_sync(&cm->limit_current_work);
			cm->vchg_info->ops->update_vchg_info(cm->vchg_info,
							     SPRD_VCHG_VAL_CMD_USB_LIMIT,
							     -EINVAL);
		}

		cm->desc->limit_status = 0;
		cancel_delayed_work_sync(&cm->fixed_fchg_work);
		cm_set_charger_present(cm, false);
		cm_reset_charger_current(cm);
		cancel_delayed_work(&cm->power_detect_work);
		cancel_delayed_work(&cm->adjust_dpm_work);
		cancel_delayed_work(&cm->dcd_work);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work_sync(&cm->cp_work);
		cancel_delayed_work_sync(&cm->charger_type_update_work);
		_cm_monitor(cm);

		cm->desc->force_pps_diasbled = false;
		cm->desc->is_fast_charge = false;
		cm->desc->ir_comp.ir_compensation_en = false;
		cm->desc->enable_fast_charge = false;
		cm->desc->fchg_voltage_check_count = 0;
		cm->desc->fast_charge_enable_count = 0;
		cm->desc->fast_charge_disable_count = 0;
		cm->desc->fixed_fchg_running = false;
		cm->desc->wait_vbus_stable = false;
		cm->desc->cp_sm.running = false;
		cm->desc->cm_check_int = false;
		cm->desc->fast_charger_type = 0;
		cm->desc->charger_type = 0;
		cm->desc->trigger_cnt = 0;
		cm->desc->first_trigger_cnt = 0;
		cm->desc->force_set_full = false;
		cm->emergency_stop = 0;
		cm->charging_status = 0;
		cm->desc->jeita_tab_size = 0;
		jeita_info_init(&cm->desc->jeita_info);

		cm->desc->thm_info.thm_pwr = 0;
		cm->desc->thm_info.adapter_default_charge_vol = 5;
		cm->desc->wl_charge_en = 0;
		cm->desc->usb_charge_en = 0;
		cm->vchg_info->charger_type_cnt = 0;
		cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
					 SPRD_VOTE_TYPE_ALL, 0, 0, 0, cm);
		cm->desc->xts_limit_cur = false;
		cm->desc->adapter_max_vbus = 0;
		cm->desc->charge_type_poll_count = 0;
		if (cm->erp_config)
			cm->uisoc_100_adapter_plugin = false;
	}

	cm_update_charger_type_status(cm);
	cm->desc->pd_port_partner = 0;

	if (is_polling_required(cm) && cm->desc->polling_interval_ms)
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);

	power_supply_changed(cm->charger_psy);
	dev_info(cm->dev, "%s usb_present=%d\n", __func__, cm->usb_present );
}

static void cm_get_charging_status(struct charger_manager *cm, int *status)
{
	if (is_charging(cm)) {
		cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
	} else if (is_ext_pwr_online(cm)) {
		if (is_full_charged(cm)) {
			if (cm->erp_config) {
				if (cm->desc->cap >= 945) {
					cm->erp_full_flag = 1;
					cm->battery_status = POWER_SUPPLY_STATUS_FULL;
				} else {
					cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
				}
			} else {
				if (cm->desc->cap >= CM_CAP_FULL_PERCENT)
					cm->battery_status = POWER_SUPPLY_STATUS_FULL;
				else
					cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
			}
		} else {
			cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
		}
	} else {
		cm->erp_full_flag = 0;
		cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	if(cm->input_suspend)
		cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;

	*status = cm->battery_status;
}

static void cm_get_charging_health_status(struct charger_manager *cm, int *status)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TEMP, &val);

	if (val.intval >=580) {
		*status = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if(val.intval >=560 && val.intval < 580) {
		*status = POWER_SUPPLY_HEALTH_HOT;
	} else if(val.intval >=480 && val.intval < 560) {
		*status = POWER_SUPPLY_HEALTH_WARM;
	} else if(val.intval >=150 && val.intval < 480) {
		*status = POWER_SUPPLY_HEALTH_GOOD;
	} else if(val.intval >=0 && val.intval < 150) {
		*status = POWER_SUPPLY_HEALTH_COOL;
	} else if(val.intval < 0) {
		*status = POWER_SUPPLY_HEALTH_COLD;
	}
}

static int cm_get_battery_technology(struct charger_manager *cm, union power_supply_propval *val)
{
	struct power_supply *fuel_gauge = NULL;
	int ret;

	val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TECHNOLOGY, val);
	power_supply_put(fuel_gauge);

	return ret;
}

static void cm_get_uisoc(struct charger_manager *cm, int *uisoc)
{
	if (!is_batt_present(cm)) {
		/* There is no battery. Assume 100% */
//		*uisoc = 100;
		return;
	}

	*uisoc = DIV_ROUND_CLOSEST(cm->desc->cap, 10);
	if (*uisoc > 100)
		*uisoc = 100;
	else if (*uisoc < 0)
		*uisoc = 0;
}

int cm_get_batt_capacity_level(struct charger_manager *cm, int *level)
{
	int ret;
	struct power_supply *fuel_gauge;
	union power_supply_propval val;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		pr_err("get fuel_gauge failed\n");
	else {
		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY_LEVEL, &val);
		if (ret) {
			pr_err("get capacity level from gauge failed\n");
			val.intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		}
		*level = val.intval;
		if (*level == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL)
			pr_info("soc capacity level critical true\n");
	}
	return 0;
}

static int cm_get_charge_full_design(struct charger_manager *cm, union power_supply_propval *val)
{
	int ret = 0, batt_id = 0;

	ret = cm_get_bat_id(cm);
	if (!ret)
		batt_id = cm->bat_id;
	val->intval = r_items_param[batt_id].charge_full_design;
#if 0
	struct power_supply *fuel_gauge = NULL;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, val);
	power_supply_put(fuel_gauge);
#endif

	return ret;
}

static int cm_get_charge_now(struct charger_manager *cm, int *charge_now)
{
	int total_uah;
	int ret;

	ret = get_batt_total_uah(cm, &total_uah);
	if (ret) {
		dev_err(cm->dev, "failed to get total uah.\n");
		return ret;
	}

	*charge_now = total_uah / CM_CAP_FULL_PERCENT * cm->desc->cap;

	return ret;
}

static int cm_get_charge_counter(struct charger_manager *cm, int *charge_counter)
{
	int ret;

	*charge_counter = 0;
	ret = cm_get_charge_now(cm, charge_counter);

	if (*charge_counter <= 0) {
		*charge_counter = 1;
		ret = 0;
	}

	return ret;
}

#if 0
static int cm_get_charge_control_limit(struct charger_manager *cm,
				       union power_supply_propval *val)
{
	struct power_supply *psy = NULL;
	int i, ret = 0;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, val);
		power_supply_put(psy);
		if (!ret) {
			if (cm->desc->enable_fast_charge && cm->desc->psy_charger_stat[1])
				val->intval *= 2;

			break;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
						val);
		if (!ret)
			break;
	}

	return ret;
}
#endif

static int cm_get_charge_full_uah(struct charger_manager *cm, union power_supply_propval *val)
{
	int ret, total_uah;

	val->intval = 0;
	ret = get_batt_total_uah(cm, &total_uah);
	if (ret) {
		dev_err(cm->dev, "failed to get total uah.\n");
		return ret;
	}

	val->intval = total_uah;

	return ret;
}

static int cm_get_time_to_full_now(struct charger_manager *cm, int *time)
{
	unsigned int total_uah = 0;
	int chg_cur = 0;
	int ret;

	ret = get_constant_charge_current(cm, &chg_cur);
	if (ret) {
		dev_err(cm->dev, "get chg_cur error.\n");
		return ret;
	}

	chg_cur = chg_cur / 1000;

	ret = get_batt_total_uah(cm, &total_uah);
	if (ret) {
		dev_err(cm->dev, "failed to get total cap.\n");
		return ret;
	}

	total_uah = total_uah / 1000;

	*time = ((1000 - cm->desc->cap) * total_uah / 1000) * 3600 / chg_cur;

	if (*time <= 0)
		*time = 1;

	return ret;
}

static void cm_get_voltage_max(struct charger_manager *cm, int *voltage_max)
{
	int adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V, chg_type_max_vbus = 0;
	int ret = 0;

	if (!is_ext_pwr_online(cm)) {
		*voltage_max = min(chg_type_max_vbus, adapter_max_vbus);
		return;
	}

	switch (cm->desc->charger_type) {
	case CM_CHARGER_TYPE_FAST:
		if (!cm->desc->fast_charge_voltage_max) {
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			break;
		}

		if (cm->desc->fast_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_20V)
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_20V;
		else if (cm->desc->fast_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_15V)
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_15V;
		else if (cm->desc->fast_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_12V)
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_12V;
		else if (cm->desc->fast_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_9V)
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_9V;

		ret = cm_get_fchg_adapter_max_voltage(cm, &adapter_max_vbus);
		if (ret) {
			adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			dev_err(cm->dev,
				"%s, failed to obtain the adapter max_vol in fixed fchg\n",
				__func__);
		}
		break;
	case CM_CHARGER_TYPE_ADAPTIVE:
		if (!cm->desc->flash_charge_voltage_max) {
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			break;
		}

		if (cm->desc->flash_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_20V)
			chg_type_max_vbus = CM_PPS_VOLTAGE_21V;
		else if (cm->desc->flash_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_15V)
			chg_type_max_vbus = CM_PPS_VOLTAGE_16V;
		else if (cm->desc->flash_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_9V)
			chg_type_max_vbus = CM_PPS_VOLTAGE_11V;

		ret = cm_get_fchg_adapter_max_voltage(cm, &adapter_max_vbus);
		if (ret) {
			adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			dev_err(cm->dev,
				"%s, failed to obtain the adapter max_vol in pps\n",
				__func__);
			break;
		}

		if (cm->desc->charger_type == CM_CHARGER_TYPE_ADAPTIVE &&
		    cm->desc->force_pps_diasbled)
			adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
		break;
	case CM_WIRELESS_CHARGER_TYPE_EPP:
		if (!cm->desc->wireless_fast_charge_voltage_max) {
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			break;
		}

		chg_type_max_vbus = cm->desc->wireless_fast_charge_voltage_max;
		break;
	case CM_CHARGER_TYPE_DCP:
	case CM_CHARGER_TYPE_CDP:
	case CM_CHARGER_TYPE_SDP:
	case CM_CHARGER_TYPE_UNKNOWN:
	case CM_WIRELESS_CHARGER_TYPE_BPP:
	default:
		chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
		break;
	}

	*voltage_max = min(chg_type_max_vbus, adapter_max_vbus);
}

static void cm_get_current_max(struct charger_manager *cm, int *current_max)
{
	int adapter_max_ibus = CM_FAST_CHARGE_CURRENT_2A, chg_type_max_ibus = 0;
	int opt_max_vbus;
	int ret = 0;

	if (!is_ext_pwr_online(cm)) {
		*current_max = min(chg_type_max_ibus, adapter_max_ibus);
		return;
	}

	switch (cm->desc->charger_type) {
	case CM_CHARGER_TYPE_DCP:
		chg_type_max_ibus = cm->desc->cur.dcp_limit;
		break;
	case CM_CHARGER_TYPE_SDP:
		chg_type_max_ibus = cm->desc->cur.sdp_limit;
		break;
	case CM_CHARGER_TYPE_CDP:
		chg_type_max_ibus = cm->desc->cur.cdp_limit;
		break;
	case CM_CHARGER_TYPE_FAST:
		chg_type_max_ibus = cm->desc->cur.fchg_limit;
		cm_get_voltage_max(cm, &opt_max_vbus);
		ret = cm_get_fchg_adapter_max_current(cm, opt_max_vbus, &adapter_max_ibus);
		if (ret) {
			adapter_max_ibus = CM_FAST_CHARGE_CURRENT_2A;
			dev_err(cm->dev,
				"%s, failed to obtain the adapter max_cur in fixed fchg\n",
				__func__);
		}
		break;
	case CM_CHARGER_TYPE_ADAPTIVE:
		chg_type_max_ibus = cm->desc->cur.flash_limit;
		cm_get_voltage_max(cm, &opt_max_vbus);
		ret = cm_get_fchg_adapter_max_current(cm, opt_max_vbus, &adapter_max_ibus);
		if (ret) {
			adapter_max_ibus = CM_FAST_CHARGE_CURRENT_2A;
			dev_err(cm->dev,
				"%s, failed to obtain the adapter max_cur in pps\n", __func__);
			break;
		}

		if (cm->desc->charger_type == CM_CHARGER_TYPE_ADAPTIVE &&
		    cm->desc->force_pps_diasbled)
			adapter_max_ibus = CM_FAST_CHARGE_CURRENT_2A;
		break;
	case CM_WIRELESS_CHARGER_TYPE_BPP:
		chg_type_max_ibus = cm->desc->cur.wl_bpp_limit;
		break;
	case CM_WIRELESS_CHARGER_TYPE_EPP:
		chg_type_max_ibus = cm->desc->cur.wl_epp_limit;
		break;
	case CM_CHARGER_TYPE_UNKNOWN:
	default:
		chg_type_max_ibus = cm->desc->cur.unknown_limit;
		break;
	}

	*current_max = min(chg_type_max_ibus, adapter_max_ibus);
}

#if 0
static int cm_source_try_sink_limit_current(struct charger_manager *cm, int limit)
{
	const u32 pdo_limit[1] = {SPRD_PDO_FIXED(5000, CM_LIMIT_POWER_TRANSFER_MA,
						 SPRD_PDO_FIXED_USB_COMM)};
	const u32 pdo_no_limit[1] = {SPRD_PDO_FIXED(5000, 400, SPRD_PDO_FIXED_USB_COMM)};
	int ret;

	if (!cm->fchg_info->support_fchg || !cm->fchg_info->ops ||
	    !cm->fchg_info->ops->update_src_cap) {
		dev_err(cm->dev, "%s:%d not support\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (limit)
		ret = cm->fchg_info->ops->update_src_cap(cm->fchg_info, pdo_limit, 1);
	else
		ret = cm->fchg_info->ops->update_src_cap(cm->fchg_info, pdo_no_limit, 1);

	if (ret) {
		dev_err(cm->dev, "[%s]failed to update src cap, ret = %d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#endif

static int cm_set_charge_control_limit(struct charger_manager *cm, int value)
{
	if ((value<0) || (value >= THERMAL_MAX_LEVEL)) {
		dev_err(cm->dev, "thermal level not match:%d\n", value);
		return -ENOMEM;
	}

	dev_err(cm->dev, "%s:line%d set thermal_level = %d, set current_limit:%d\n",
		__func__, __LINE__, value, thermal_battery_current_limit_buf[value]);

	cm->desc->thermal_limit_level = value;
	cm->desc->thermal_limit_ibat = thermal_battery_current_limit_buf[value];

	cm_update_charge_info(cm, CM_CHARGE_INFO_THERMAL_LIMIT);

	return 0;
}

static int cm_set_fake_batt_temp(struct charger_manager *cm, int fake_temp)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "can not find fuel gauge device\n");
		return -ENODEV;
	}

	dev_dbg(cm->dev, "%s:line%d fake_temp = %d\n", __func__, __LINE__, fake_temp);
	val.intval = fake_temp;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_TEMP, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to save fake battery temp\n");

	return ret;
}

static ssize_t otg_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf){

	return snprintf(buf, PAGE_SIZE, "%d\n", g_cm->otg_debug);
}

static ssize_t otg_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count){
	int ret;
	int value;
	if (!g_cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}
	ret =  kstrtoint(buf, 10, &value);
	if (ret)
		return ret;
	if(value)
		g_cm->otg_debug = 1;
	dev_info(g_cm->dev, "%s:line%d otg_debug = %d\n",
		__func__, __LINE__, g_cm->otg_debug);
	return count;
}

static struct device_attribute otg_debug_attr =
	__ATTR(otg_debug, 0644, otg_debug_show, otg_debug_store);

enum power_supply_quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,
	QUICK_CHARGE_FAST,
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,
	QUICK_CHARGE_SUPER,
	QUICK_CHARGE_MAX,
};

static int uevent_report_qucik_charge_type(struct charger_manager *cm){
	char sd_str[64];
	char *envp[] = { sd_str, NULL };

	sprintf(envp[0], "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", cm->quick_charge_type);
	if(cm)
		kobject_uevent_env(&cm->charger_psy->dev.kobj, KOBJ_CHANGE, envp);
	else
		return -EINVAL;
	pr_err("uevent_report_quick_charge_type=%d\n",cm->quick_charge_type);

	return 0;
}

static int cm_get_quick_charge_type(struct charger_manager *cm){
	int ret = 0;
	int temp = 0;
	struct power_supply *pd_psy = NULL;
	union power_supply_propval pval;

	if(!(is_ext_pwr_online(cm))) {
		pr_err("external power offline\n");
		cm->quick_charge_type = QUICK_CHARGE_NORMAL;
		uevent_report_qucik_charge_type(cm);
		return -EINVAL;
	}
	ret = cm_get_battery_temperature(cm,&temp);
	if(ret)
		return -EINVAL;

	pd_psy = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (pd_psy) {
		if (!power_supply_get_property(pd_psy, POWER_SUPPLY_PROP_USB_TYPE, &pval)) {
			if (pval.intval == POWER_SUPPLY_USB_TYPE_PD || pval.intval == POWER_SUPPLY_USB_TYPE_PD_PPS){
				cm->quick_charge_type = QUICK_CHARGE_FAST;
			}
			else
				cm->quick_charge_type = QUICK_CHARGE_NORMAL;
		}
	}
	if(temp >= 480 || temp <= 50){
		cm->quick_charge_type = QUICK_CHARGE_NORMAL;
	}

	uevent_report_qucik_charge_type(cm);

	return 0;
}

static void cm_quick_charge_type_update_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
									struct charger_manager, quick_charge_type_update_work);

	cm_get_quick_charge_type(cm);
	cm->quick_charge_type_poll_count++;

	//pr_info("quick_charge_type_poll_count=%d", cm->quick_charge_type_poll_count);

	if (cm->quick_charge_type_poll_count < CM_QUICK_CHARGE_TYPE_RETRY_MAX)
	{
		schedule_delayed_work(&cm->quick_charge_type_update_work,
									msecs_to_jiffies(CM_QUICK_CHARGE_TYPE_WORK_INTERVAL_MS));
	} else {
		cm->quick_charge_type_poll_count = 0; //reset
	}

}

static int cm_get_power_supply_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct cm_power_supply_data *data = container_of(psy->desc, struct  cm_power_supply_data, psd);

	if (!data || !data->cm)
		return -ENOMEM;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->ONLINE;
		if (data->cm->input_suspend)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		cm_get_current_max(data->cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		cm_get_voltage_max(data->cm, &val->intval);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void cm_get_charge_type(struct charger_manager *cm, int *charge_type)
{
	switch (cm->desc->charger_type) {
	case CM_CHARGER_TYPE_SDP:
	case CM_CHARGER_TYPE_DCP:
	case CM_CHARGER_TYPE_CDP:
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_STANDARD;
		break;

	case CM_CHARGER_TYPE_FAST:
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;

	case CM_CHARGER_TYPE_ADAPTIVE:
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE;
		break;
	default:
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}

static bool cm_add_battery_psy_property(struct charger_manager *cm,
					enum power_supply_property psp,
					enum power_supply_property *properties,
					size_t *num_properties)
{
	u32 i;

	for (i = 0; i < *num_properties; i++)
		if (properties[i] == psp)
			break;

	if (i == *num_properties) {
		properties[*num_properties] = psp;
		(*num_properties)++;
		return true;
	}
	return false;
}

static int charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	int ret = 0;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	int chip_cycle = -EINVAL;

	if (!cm)
		return -ENOMEM;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		cm_get_charging_status(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		cm_get_charging_health_status(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = is_batt_present(cm);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = get_vbat_avg_uV(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = get_vbat_now_uV(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = get_ibat_avg_uA(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = get_ibat_now_uA(cm, &val->intval);
		if(cm->battery_status == POWER_SUPPLY_STATUS_FULL && val->intval <= 5000){
			val->intval = 0;
		}
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = cm_get_battery_technology(cm, val);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (!cm->bat_id) {
			val->intval = NONSTAND_BATT_TEMP;
			break;
		}
		val->intval = cm->desc->temperature;
		break;

	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		return cm_get_board_temperature(cm, &val->intval);

	case POWER_SUPPLY_PROP_CAPACITY:
		if (cm->fake_capacity != -EINVAL) {
			val->intval = cm->fake_capacity;
			break;
		}
		if (!cm->bat_id) {
			val->intval = NONSTAND_BATT_CAP;
			break;
		}
		cm_get_uisoc(cm, &val->intval);
		if (val->intval <= 1)
			val->intval = 1;
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = cm_get_batt_capacity_level(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_ext_pwr_online(cm);
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = cm_get_charge_now(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = get_constant_charge_current(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = get_input_current_limit(cm,  &val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = cm_get_charge_counter(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		val->intval = cm->desc->thermal_limit_level;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = cm_get_charge_full_design(cm, val);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = cm_get_charge_full_uah(cm, val);
		break;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = cm_get_time_to_full_now(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = cm_get_bc1p2_type(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		cm_get_charge_type(cm, &val->intval);
		charger_get_vbus_type(cm->charger, &chg_type);
		if (chg_type == POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID ||
			chg_type == POWER_SUPPLY_USB_TYPE_C)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_STANDARD;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		if (cm->fake_charge_cycle >= 0) {
			val->intval = cm->fake_charge_cycle;
			dev_info(cm->dev, "use fake cycle = %d\n", cm->fake_charge_cycle);
		} else {
			// read platform charge cycle
			ret = cm_get_charge_cycle(cm, &val->intval);
			if (val->intval < 0)
				val->intval = 0;
			val->intval /= 1000;
			if (CHIP_ONLINE == ll_check_secret_chip_online()) {
				chip_cycle = lc_get_cycle_count();
				if (chip_cycle >= 0)
					val->intval = chip_cycle;
				else
					dev_err(cm->dev, "use platform cycle:%d\n", val->intval);
			}
		}
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		if (r_items_param == r_items_param_single)
		{
			val->strval = "Somalia_6300mah_15w";
		}
		else
		{
			val->strval = "Somalia_6000mah_15w";
		}
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static int charger_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		dev_dbg(cm->dev, "%s:line%d const cur = %d\n", __func__, __LINE__, val->intval);
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CONSTANT_CHARGE_CURRENT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		/* The ChargerIC with linear charging cannot set Ibus, only Ibat. */
		dev_dbg(cm->dev, "%s:line%d input limit cur = %d\n",
			__func__, __LINE__, val->intval);
		if (cm->desc->thm_info.need_calib_charge_lmt) {
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_INPUT_CURRENT_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
			break;
		}

		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_INPUT_CURRENT_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		dev_info(cm->dev, "%s:line%d control limit cur = %d\n",
			__func__, __LINE__, val->intval);
		ret = cm_set_charge_control_limit(cm, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		dev_dbg(cm->dev, "%s:line%d set fake capacity = %d\n",
			__func__, __LINE__, val->intval);
		cm->fake_capacity = val->intval;
		power_supply_changed(cm->charger_psy);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = cm_set_fake_batt_temp(cm, val->intval);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int charger_property_is_writeable(struct power_supply *psy, enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_TEMP:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}
#define NUM_CHARGER_PSY_OPTIONAL	(4)

static enum power_supply_property wireless_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property default_charger_props[] = {
	/* Guaranteed to provide */
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_MODEL_NAME,
	/*
	 * Optional properties are:
	 * POWER_SUPPLY_PROP_CHARGE_NOW,
	 */
};

/* wireless_data initialization */
static struct cm_power_supply_data wireless_main = {
	.psd = {
		.name = "wireless",
		.type =	POWER_SUPPLY_TYPE_WIRELESS,
		.properties = wireless_props,
		.num_properties = ARRAY_SIZE(wireless_props),
		.get_property = cm_get_power_supply_property,
	},
	.ONLINE = 0,
};

/* ac_data initialization */
static struct cm_power_supply_data ac_main = {
	.psd = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = cm_get_power_supply_property,
	},
	.ONLINE = 0,
};

/* usb_data initialization */
static struct cm_power_supply_data usb_main = {
	.psd = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = cm_get_power_supply_property,
	},
	.ONLINE = 0,
};

static enum power_supply_usb_type default_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static const struct power_supply_desc psy_default = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = default_charger_props,
	.num_properties = ARRAY_SIZE(default_charger_props),
	.get_property = charger_get_property,
	.set_property = charger_set_property,
	.property_is_writeable	= charger_property_is_writeable,
	.usb_types		= default_usb_types,
	.num_usb_types		= ARRAY_SIZE(default_usb_types),
	.no_thermal = false,
};

#if IS_ENABLED(CONFIG_SPRD_TYPEC_TCPM)
static bool cm_pd_is_ac_online(struct charger_manager *cm)
{
	struct adapter_power_cap pd_source_cap;
	struct sprd_tcpm_port *port;
	struct power_supply *psy;
	int i, index = 0;

	if (!cm->fchg_info->pd_enable)
		return true;

	psy = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);
	if (!psy) {
		dev_err(cm->dev, "failed to get tcpm psy!\n");
		return true;
	}

	port = power_supply_get_drvdata(psy);
	if (!port) {
		dev_err(cm->dev, "failed to get tcpm port!\n");
		return true;
	}

	sprd_tcpm_get_source_capabilities(port, &pd_source_cap);

	for (i = 0; i < pd_source_cap.nr_source_caps; i++) {
		if ((pd_source_cap.max_mv[i] <= 5000) &&
		    (pd_source_cap.ma[i] < CM_FIXED_FCHG_C2C_IBUS_THRESHOLD / 1000) &&
		    (pd_source_cap.type[i] == SPRD_PDO_TYPE_FIXED))
			index++;
	}

	if (pd_source_cap.nr_source_caps == index) {
		dev_info(cm->dev, "pd type ac online is false, index = %d\n", index);
		return false;
	}

	return true;
}
#else
static bool cm_pd_is_ac_online(struct charger_manager *cm)
{
	return true;
}
#endif

static void cm_update_charger_type_status(struct charger_manager *cm)
{

	if (is_ext_wl_pwr_online(cm)) {
		wireless_main.ONLINE = 1;
		ac_main.ONLINE = 0;
		usb_main.ONLINE = 0;
	} else if (is_ext_usb_pwr_online(cm)) {
		switch (cm->desc->charger_type) {
		case CM_CHARGER_TYPE_DCP:
		case CM_CHARGER_TYPE_FAST:
		case CM_CHARGER_TYPE_ADAPTIVE:
			if (cm->fchg_info->pd_enable && !cm->fchg_info->pps_enable &&
				!cm_pd_is_ac_online(cm) && !cm->desc->pd_port_partner) {
				wireless_main.ONLINE = 0;
				ac_main.ONLINE = 0;
				usb_main.ONLINE = 1;
				dev_info(cm->dev, "usb online--pd type 5V\n");
			} else {
				wireless_main.ONLINE = 0;
				usb_main.ONLINE = 0;
				ac_main.ONLINE = 1;
				dev_info(cm->dev, "ac online\n");
			}
			break;
		default:
			dev_info(cm->dev, "default usb online\n");
			wireless_main.ONLINE = 0;
			ac_main.ONLINE = 0;
			usb_main.ONLINE = 1;
			break;
		}
	} else {
		wireless_main.ONLINE = 0;
		ac_main.ONLINE = 0;
		usb_main.ONLINE = 0;
	}

	dev_info(cm->dev, "%s:line%d usb = %d, ac = %d, wireless = %d\n",
		__func__, __LINE__, usb_main.ONLINE, ac_main.ONLINE, wireless_main.ONLINE);
}

static void cm_limit_current_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  limit_current_work);
	int ret;
	u32 type;

	if (!(cm->desc->limit_status & CM_CHARGE_USB_LIMIT_CMD) &&
	    cm->vchg_info->usb_limit != -EINVAL &&
	    cm->vchg_info->usb_limit != 2000 &&
	    cm->vchg_info->usb_limit < min(CM_SDP_TYPE_USB_ENUM_LIMIT_CUR_UA,
					   cm->desc->cur.sdp_limit)) {
		ret = cm_get_bc1p2_type(cm, &type);
		if (!ret && type == POWER_SUPPLY_USB_TYPE_SDP &&
		    cm->vchg_info->ops && cm->vchg_info->ops->update_vchg_info) {
			cm->vchg_info->ops->update_vchg_info(cm->vchg_info,
							     SPRD_VCHG_VAL_CMD_USB_LIMIT,
							     cm->desc->cur.sdp_limit);
			cm_update_charge_info(cm, CM_CHARGE_INFO_INPUT_LIMIT);
			goto done;
		}
	}

	if (cm->desc->limit_status)
		cm_update_charge_info(cm, CM_CHARGE_INFO_INPUT_LIMIT);

done:
	dev_info(cm->dev, "%s, usb_limit %d, rp_limit %d, pd_req_cur_ua %d, limit_status %d\n",
		 __func__, cm->vchg_info->usb_limit, cm->desc->rp_limit_current,
		 cm->desc->pd_req_cur_ua, cm->desc->limit_status);
}

void cm_check_pd_negotiated_limit_current(enum sprd_pd_pdo_type pdo_type,
					  int req_vol_uv,
					  int req_cur_ua,
					  bool enable_limit)
{
	struct charger_manager *cm;
	bool found_power_supply = false;

	if (g_cm) {
		cm = g_cm;
	} else {
		mutex_lock(&cm_list_mtx);
		list_for_each_entry(cm, &cm_list, entry) {
			if (cm->charger_psy->desc) {
				if (strcmp(cm->charger_psy->desc->name, "battery") == 0) {
					found_power_supply = true;
					break;
				}
			}
		}

		mutex_unlock(&cm_list_mtx);

		if (!found_power_supply) {
			pr_err("%s:line%d no cm found!!!\n", __func__, __LINE__);
			return;
		}

		if (!cm) {
			pr_err("%s:line%d NULL pointer!!!\n", __func__, __LINE__);
			return;
		}
	}

	if (pdo_type != SPRD_PDO_TYPE_FIXED) {
		pr_err("%s, Unsupport pdo_type[%d]!!!\n", __func__, pdo_type);
		return;
	}
	cm->desc->pd_req_vol_uv = req_vol_uv;
	cm->desc->pd_req_cur_ua = req_cur_ua;
	dev_info(cm->dev, "sprd: %s, Requesting APDO: %d mV, %d mA, enable_limit: %d\n",
		 __func__, cm->desc->pd_req_vol_uv / 1000, cm->desc->pd_req_cur_ua / 1000,
		 cm->desc->pd_enable_limit);
	dev_info(cm->dev, "sprd: %s, pd_negotiated, limit_cur: %d\n",
		 __func__, cm->desc->pd_negotiated_limit_cur);

	if (cm->desc->pd_req_vol_uv != 0 &&
	    cm->desc->pd_req_vol_uv < CM_FIXED_FCHG_VOLTAGE_9V_THRESHOLD) {
		cm->desc->limit_status |= CM_CHARGE_PD_LIMIT_CMD;
		schedule_delayed_work(&cm->limit_current_work, 0);
	}
}

void cm_check_pd_update_ac_usb_online(bool is_pd_hub)
{
	struct charger_manager *cm;
	bool found_power_supply = false;

	if (g_cm) {
		cm = g_cm;
	} else {
		mutex_lock(&cm_list_mtx);
		list_for_each_entry(cm, &cm_list, entry) {
			if (cm->charger_psy->desc) {
				if (strcmp(cm->charger_psy->desc->name, "battery") == 0) {
					found_power_supply = true;
					break;
				}
			}
		}

		mutex_unlock(&cm_list_mtx);

		if (!found_power_supply) {
			pr_err("%s:line%d no cm found!!!\n", __func__, __LINE__);
			return;
		}

		if (!cm) {
			pr_err("%s:line%d NULL pointer!!!\n", __func__, __LINE__);
			return;
		}
	}
	dev_info(cm->dev, "%s is_pd_hub = %d\n", __func__, is_pd_hub);

	if (!is_ext_usb_pwr_online(cm))
		dev_info(cm->dev, "%s pd notify before vchg/typec\n", __func__);

	cm->desc->pd_port_partner = is_pd_hub;

	if (usb_main.ONLINE)
		cm_update_charger_type_status(cm);

	if (ac_main.ONLINE) {
		power_supply_changed(cm->charger_psy);
		dev_info(cm->dev, "%s pd notify after vchg/typec\n", __func__);
	}
}

void cm_check_rp_limit_current(int rp_limit)
{
	struct charger_manager *cm;
	bool found_power_supply = false;
	int rp_limit_current = rp_limit * 1000;

	if (g_cm) {
		cm = g_cm;
	} else {
		mutex_lock(&cm_list_mtx);
		list_for_each_entry(cm, &cm_list, entry) {
			if (cm->charger_psy->desc) {
				if (strcmp(cm->charger_psy->desc->name, "battery") == 0) {
					found_power_supply = true;
					break;
				}
			}
		}

		mutex_unlock(&cm_list_mtx);

		if (!found_power_supply) {
			pr_err("%s:line%d no cm found!!!\n", __func__, __LINE__);
			return;
		}

		if (!cm) {
			pr_err("%s:line%d NULL pointer!!!\n", __func__, __LINE__);
			return;
		}
	}

	if (cm->desc->rp_limit_current != rp_limit_current) {
		cm->desc->rp_limit_current = rp_limit_current;
		cm->desc->limit_status |= CM_CHARGE_RP_LIMIT_CMD;
		schedule_delayed_work(&cm->limit_current_work, 0);
	}
}

static struct sprd_charger_ops cm_sprd_charger_ops = {
	.name = "sprd_charger_manager",
	.update_ac_usb_online = cm_check_pd_update_ac_usb_online,
	.negotiated_limit_current = cm_check_pd_negotiated_limit_current,
	.set_rp_limit_current = cm_check_rp_limit_current,
};

/**
 * cm_setup_timer - For in-suspend monitoring setup wakeup alarm
 *		    for suspend_again.
 *
 * Returns true if the alarm is set for Charger Manager to use.
 * Returns false if
 *	cm_setup_timer fails to set an alarm,
 *	cm_setup_timer does not need to set an alarm for Charger Manager,
 *	or an alarm previously configured is to be used.
 */
static bool cm_setup_timer(void)
{
	struct charger_manager *cm;
	unsigned int wakeup_ms = UINT_MAX;
	int timer_req = 0;

	if (time_after(next_polling, jiffies))
		CM_MIN_VALID(wakeup_ms,
			jiffies_to_msecs(next_polling - jiffies));

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		unsigned int fbchk_ms = 0;

		/* fullbatt_vchk is required. setup timer for that */
		if (cm->fullbatt_vchk_jiffies_at) {
			fbchk_ms = jiffies_to_msecs(cm->fullbatt_vchk_jiffies_at
						    - jiffies);
			if (time_is_before_eq_jiffies(
				cm->fullbatt_vchk_jiffies_at) ||
				msecs_to_jiffies(fbchk_ms) < CM_JIFFIES_SMALL) {
				fullbatt_vchk(&cm->fullbatt_vchk_work.work);
				fbchk_ms = 0;
			}
		}
		CM_MIN_VALID(wakeup_ms, fbchk_ms);

		/* Skip if polling is not required for this CM */
		if (!is_polling_required(cm) && !cm->emergency_stop)
			continue;
		timer_req++;
		if (cm->desc->polling_interval_ms == 0)
			continue;
		if (cm->desc->ir_comp.ir_compensation_en)
			CM_MIN_VALID(wakeup_ms, CM_IR_COMPENSATION_TIME * 1000);
		else
			CM_MIN_VALID(wakeup_ms, cm->desc->polling_interval_ms);
	}
	mutex_unlock(&cm_list_mtx);

	if (timer_req && cm_timer) {
		ktime_t now, add;

		/*
		 * Set alarm with the polling interval (wakeup_ms)
		 * The alarm time should be NOW + CM_RTC_SMALL or later.
		 */
		if (wakeup_ms == UINT_MAX ||
			wakeup_ms < CM_RTC_SMALL * MSEC_PER_SEC)
			wakeup_ms = 2 * CM_RTC_SMALL * MSEC_PER_SEC;

		pr_info("Charger Manager wakeup timer: %u ms\n", wakeup_ms);

		now = ktime_get_boottime();
		add = ktime_set(wakeup_ms / MSEC_PER_SEC,
				(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
		alarm_start(cm_timer, ktime_add(now, add));
		pr_info("cm_timer set alarm, triggered at [%lld]ms\n",
			ktime_to_ms(ktime_add(now, add)));

		cm_suspend_duration_ms = wakeup_ms;

		return true;
	}
	return false;
}

static ssize_t jeita_control_show(struct device *dev,  struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_jeita_control);
	struct charger_desc *desc;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	desc = sysfs->cm->desc;
	if (!desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", !desc->jeita_disabled);
}

static ssize_t jeita_control_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_jeita_control);
	struct charger_manager *cm;
	struct charger_desc *desc;
	bool enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->dev) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	desc = sysfs->cm->desc;
	if (!desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	dev_info(cm->dev, "%s[%d], enabled=%d, jeita_disabled=%d\n",
		 __func__, __LINE__, enabled, desc->jeita_disabled);
	if (desc->jeita_disabled == enabled)
		desc->jeita_info.jeita_changed = true;
	desc->jeita_disabled = !enabled;
	desc->step_chg_disabled = !enabled;

	return count;
}

static ssize_t step_chg_control_show(struct device *dev,  struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_step_chg_control);
	struct charger_desc *desc;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	desc = sysfs->cm->desc;
	if (!desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", !desc->step_chg_disabled);
}

static ssize_t step_chg_control_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_step_chg_control);
	struct charger_manager *cm;
	bool enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc || !cm->dev) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	cm->desc->step_chg_disabled = !enabled;
	dev_info(cm->dev, "%s[%d], step_chg_disabled=%d\n",
		 __func__, __LINE__, cm->desc->step_chg_disabled);

	return count;
}

static ssize_t
charge_pump_present_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_charge_pump_present);
	struct charger_manager *cm;
	bool status = false;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (cm_check_cp_charger_enabled(cm))
		status = true;

	return snprintf(buf, PAGE_SIZE, "%d\n", status);
}

static ssize_t charge_pump_present_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_charge_pump_present);
	struct charger_manager *cm;
	bool enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	dev_info(cm->dev, "%s:line%d charge pump enable = %d\n", __func__, __LINE__, enabled);

	if (enabled) {
		cm_init_cp(cm);
		cm_primary_charger_enable(cm, false);
		if (cm_check_primary_charger_enabled(cm)) {
			dev_err(cm->dev, "Fail to disable primary charger\n");
			return -EINVAL;
		}

		cm_cp_charger_enable(cm, true);
		if (!cm_check_cp_charger_enabled(cm))
			dev_err(cm->dev, "Fail to enable charge pump\n");
	} else {
		if (!cm_cp_charger_enable(cm, false))
			dev_err(cm->dev, "Fail to disable charge pump\n");
	}

	return count;
}

static ssize_t
charge_pump_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_charge_pump_current);
	struct charger_manager *cm;
	int cur, ret;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (sysfs->cp_id < 0) {
		dev_err(cm->dev, "charge pump id is error!!!!!!\n");
		cur = 0;
		return snprintf(buf, PAGE_SIZE, "%d\n", cur);
	}

	ret = get_cp_ibat_uA_by_id(cm, &cur, sysfs->cp_id);
	if (ret)
		cur = 0;

	return snprintf(buf, PAGE_SIZE, "%d\n", cur);
}

static ssize_t charge_pump_current_id_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_charge_pump_current);
	struct charger_manager *cm;
	int cp_id;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtoint(buf, 10, &cp_id);
	if (ret)
		return ret;

	dev_info(cm->dev, "%s:line%d charge pump id = %d\n", __func__, __LINE__, cp_id);

	if (cp_id < 0) {
		dev_err(cm->dev, "charge pump id is error!!!!!!\n");
		cp_id = 0;
	}
	sysfs->cp_id = cp_id;

	return count;
}

static ssize_t charger_stop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_stop_charge);
	bool stop_charge;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}
	stop_charge = is_charging(sysfs->cm);

	return snprintf(buf, PAGE_SIZE, "%d\n", !stop_charge);
}

static ssize_t charger_stop_store(struct device *dev,
				  struct device_attribute *attr, const char *buf,
				  size_t count)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_stop_charge);
	struct charger_manager *cm;
	int stop_charge, ret;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->charger_psy) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret = sscanf(buf, "%d", &stop_charge);
	if (!ret)
		return -EINVAL;

	sysfs->externally_control = !!stop_charge;
	if (!is_ext_pwr_online(cm))
		return -EINVAL;

	dev_info(cm->dev, "%s, stop_charge=%d\n", __func__, stop_charge);
	if (!stop_charge) {
		ret = try_charger_enable(cm, true);
		if (ret) {
			dev_err(cm->dev, "failed to start charger.\n");
			return ret;
		}
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);
	} else {
		ret = try_charger_enable(cm, false);
		if (ret) {
			dev_err(cm->dev, "failed to stop charger.\n");
			return ret;
		}
	}

	power_supply_changed(cm->charger_psy);
	return count;
}

static ssize_t charger_externally_control_show(struct device *dev,
					       struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_externally_control);

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs->externally_control);
}

static ssize_t charger_externally_control_store(struct device *dev,
						struct device_attribute *attr, const char *buf,
						size_t count)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_externally_control);
	struct charger_manager *cm;
	struct charger_desc *desc;
	int i;
	int ret;
	int externally_control;
	int chargers_externally_control = 1;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	desc = cm->desc;
	if (!desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret = sscanf(buf, "%d", &externally_control);
	if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

	if (!externally_control) {
		sysfs->externally_control = 0;
		return count;
	}

	for (i = 0; i < desc->num_sysfs; i++) {
		if (&desc->sysfs[i] != sysfs &&
			!desc->sysfs[i].externally_control) {
			/*
			 * At least, one charger is controlled by
			 * charger-manager
			 */
			chargers_externally_control = 0;
			break;
		}
	}

	if (!chargers_externally_control) {
		if (cm->charger_enabled) {
			try_charger_enable(sysfs->cm, false);
			sysfs->externally_control = externally_control;
			try_charger_enable(sysfs->cm, true);
		} else {
			sysfs->externally_control = externally_control;
		}
	} else {
		dev_warn(cm->dev,
			 "regulator should be controlled in charger-manager because charger-manager"
			 "must need at least one charger for charging\n");
	}

	return count;
}

static ssize_t cp_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_cp_num);
	struct charger_manager *cm;
	int cp_num = 0;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cp_num = cm->desc->cp_nums;
	return snprintf(buf, PAGE_SIZE, "%d\n", cp_num);
}

static ssize_t enable_power_path_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_enable_power_path);
	struct charger_manager *cm;
	bool power_path_enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	power_path_enabled = cm_is_power_path_enabled(cm);

	return snprintf(buf, PAGE_SIZE, "%d\n", power_path_enabled);
}

static ssize_t enable_power_path_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_enable_power_path);
	struct charger_manager *cm;
	bool power_path_enabled;
	int ret;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->charger_psy) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &power_path_enabled);
	if (ret)
		return ret;

	dev_info(cm->dev, "%s:line%d power_path_enabled = %d\n",
		 __func__, __LINE__, power_path_enabled);

	if (power_path_enabled)
		cm_power_path_enable(cm, CM_POWER_PATH_ENABLE_CMD);
	else
		cm_power_path_enable(cm, CM_POWER_PATH_DISABLE_CMD);

	power_supply_changed(cm->charger_psy);

	return count;
}

static ssize_t keep_awake_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_keep_awake);
	struct charger_manager *cm;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", cm->desc->keep_awake);
}

static ssize_t keep_awake_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_keep_awake);
	struct charger_manager *cm;
	bool enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	if (cm->desc->keep_awake != enabled) {
		mutex_lock(&cm->desc->keep_awake_mtx);
		if (cm->charger_enabled && enabled) {
			dev_info(cm->dev, "Acquire charger_manager_wakelock when enable charge\n");
			__pm_stay_awake(cm->charge_ws);
		} else if (cm->charger_enabled && !enabled) {
			dev_info(cm->dev, "Release charger_manager_wakelock when disable charge\n");
			__pm_relax(cm->charge_ws);
		}
		cm->desc->keep_awake = enabled;
		mutex_unlock(&cm->desc->keep_awake_mtx);
	}

	return count;
}

static ssize_t support_fast_charge_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_support_fast_charge);
	struct charger_manager *cm;
	bool support_fast_charge = false;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->fchg_info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	support_fast_charge = cm->fchg_info->support_fchg;

	return snprintf(buf, PAGE_SIZE, "%d\n", support_fast_charge);
}

static ssize_t support_step_chg_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_support_step_chg);
	struct charger_manager *cm;
	bool support_step_chg = false;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	support_step_chg = cm->desc->support_step_chg;

	return snprintf(buf, PAGE_SIZE, "%d\n", support_step_chg);
}

static ssize_t unknow_type_cur_control_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_unknow_type_cur_control);
	struct charger_manager *cm;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return snprintf(buf, PAGE_SIZE, "Charge Limit =%d, Input Limit =%d\n",
			cm->desc->cur.unknown_cur, cm->desc->cur.unknown_limit);
}

static ssize_t unknow_type_cur_control_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	int ret, i, size;
	struct charger_manager *cm;
	struct sprd_battery_jeita_table *table;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_unknow_type_cur_control);
	int value;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	if (value > CM_UNKNOW_TYPE_CURRENT_THRESHOLD_H)
		value = CM_UNKNOW_TYPE_CURRENT_THRESHOLD_H;
	else if (value < CM_UNKNOW_TYPE_CURRENT_THRESHOLD_L)
		value = CM_UNKNOW_TYPE_CURRENT_THRESHOLD_L;

	cm->desc->cur.unknown_cur = value;
	cm->desc->cur.unknown_limit = value;

	table = cm->desc->jeita_tab_array[SPRD_BATTERY_JEITA_UNKNOWN];
	size = cm->desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN];

	for (i = 0; i < size; i++) {
		table[i].current_ua = value;
		dev_info(cm->dev, "set jeita current_ua from %d to %d\n",
			 table[i].current_ua, value);
	}

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	return count;
}

/**
 * charger_manager_prepare_sysfs - Prepare sysfs entry for each charger
 * @cm: the Charger Manager representing the battery.
 *
 * This function add sysfs entry for charger to control charger from
 * user-space. If some development board use one more chargers for charging
 * but only need one charger on specific case which is dependent on user
 * scenario or hardware restrictions, the user enter 1 or 0(zero) to '/sys/
 * class/power_supply/battery/charger.[index]/externally_control'. For example,
 * if user enter 1 to 'sys/class/power_supply/battery/charger.[index]/
 * externally_control, this charger isn't controlled from charger-manager and
 * always stay off state.
 */
static int charger_manager_prepare_sysfs(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_sysfs_ctl_item *sysfs;
	int chargers_externally_control = 1;
	char *name;
	int i;

	desc->num_sysfs = 1;

	desc->sysfs_groups = devm_kcalloc(cm->dev, desc->num_sysfs + 1, sizeof(*desc->sysfs_groups),
					  GFP_KERNEL);
	if (!desc->sysfs_groups)
		return -ENOMEM;

	/* Create sysfs entry to control charger */
	for (i = 0; i < desc->num_sysfs; i++) {
		sysfs = devm_kzalloc(cm->dev, sizeof(*sysfs), GFP_KERNEL);
		if (!sysfs)
			return -ENOMEM;

		desc->sysfs = sysfs;
		desc->sysfs->cm = cm;
		name = devm_kasprintf(cm->dev, GFP_KERNEL, "charger.%d", i);
		if (!name)
			return -ENOMEM;

		sysfs->attrs[0] = &sysfs->attr_externally_control.attr;
		sysfs->attrs[1] = &sysfs->attr_stop_charge.attr;
		sysfs->attrs[2] = &sysfs->attr_jeita_control.attr;
		sysfs->attrs[3] = &sysfs->attr_step_chg_control.attr;
		sysfs->attrs[4] = &sysfs->attr_cp_num.attr;
		sysfs->attrs[5] = &sysfs->attr_charge_pump_present.attr;
		sysfs->attrs[6] = &sysfs->attr_charge_pump_current.attr;
		sysfs->attrs[7] = &sysfs->attr_enable_power_path.attr;
		sysfs->attrs[8] = &sysfs->attr_keep_awake.attr;
		sysfs->attrs[9] = &sysfs->attr_support_fast_charge.attr;
		sysfs->attrs[10] = &sysfs->attr_support_step_chg.attr;
		sysfs->attrs[11] = &sysfs->attr_unknow_type_cur_control.attr;
		sysfs->attrs[12] = NULL;

		sysfs->attr_grp.name = name;
		sysfs->attr_grp.attrs = sysfs->attrs;
		desc->sysfs_groups[i] = &sysfs->attr_grp;

		sysfs_attr_init(&sysfs->attr_stop_charge.attr);
		sysfs->attr_stop_charge.attr.name = "stop_charge";
		sysfs->attr_stop_charge.attr.mode = 0644;
		sysfs->attr_stop_charge.show = charger_stop_show;
		sysfs->attr_stop_charge.store = charger_stop_store;

		sysfs_attr_init(&sysfs->attr_jeita_control.attr);
		sysfs->attr_jeita_control.attr.name = "jeita_control";
		sysfs->attr_jeita_control.attr.mode = 0644;
		sysfs->attr_jeita_control.show = jeita_control_show;
		sysfs->attr_jeita_control.store = jeita_control_store;

		sysfs_attr_init(&sysfs->attr_step_chg_control.attr);
		sysfs->attr_step_chg_control.attr.name = "step_chg_control";
		sysfs->attr_step_chg_control.attr.mode = 0644;
		sysfs->attr_step_chg_control.show = step_chg_control_show;
		sysfs->attr_step_chg_control.store = step_chg_control_store;

		sysfs_attr_init(&sysfs->attr_cp_num.attr);
		sysfs->attr_cp_num.attr.name = "cp_num";
		sysfs->attr_cp_num.attr.mode = 0444;
		sysfs->attr_cp_num.show = cp_num_show;

		sysfs_attr_init(&sysfs->attr_charge_pump_present.attr);
		sysfs->attr_charge_pump_present.attr.name = "charge_pump_present";
		sysfs->attr_charge_pump_present.attr.mode = 0644;
		sysfs->attr_charge_pump_present.show = charge_pump_present_show;
		sysfs->attr_charge_pump_present.store = charge_pump_present_store;

		sysfs_attr_init(&sysfs->attr_charge_pump_current.attr);
		sysfs->attr_charge_pump_current.attr.name = "charge_pump_current";
		sysfs->attr_charge_pump_current.attr.mode = 0644;
		sysfs->attr_charge_pump_current.show = charge_pump_current_show;
		sysfs->attr_charge_pump_current.store = charge_pump_current_id_store;

		sysfs_attr_init(&sysfs->attr_enable_power_path.attr);
		sysfs->attr_enable_power_path.attr.name = "enable_power_path";
		sysfs->attr_enable_power_path.attr.mode = 0644;
		sysfs->attr_enable_power_path.show = enable_power_path_show;
		sysfs->attr_enable_power_path.store = enable_power_path_store;

		sysfs_attr_init(&sysfs->attr_keep_awake.attr);
		sysfs->attr_keep_awake.attr.name = "keep_awake";
		sysfs->attr_keep_awake.attr.mode = 0644;
		sysfs->attr_keep_awake.show = keep_awake_show;
		sysfs->attr_keep_awake.store = keep_awake_store;

		sysfs_attr_init(&sysfs->attr_support_fast_charge.attr);
		sysfs->attr_support_fast_charge.attr.name = "support_fast_charge";
		sysfs->attr_support_fast_charge.attr.mode = 0444;
		sysfs->attr_support_fast_charge.show = support_fast_charge_show;

		sysfs_attr_init(&sysfs->attr_support_step_chg.attr);
		sysfs->attr_support_step_chg.attr.name = "support_step_chg";
		sysfs->attr_support_step_chg.attr.mode = 0444;
		sysfs->attr_support_step_chg.show = support_step_chg_show;

		sysfs_attr_init(&sysfs->attr_externally_control.attr);
		sysfs->attr_externally_control.attr.name = "externally_control";
		sysfs->attr_externally_control.attr.mode = 0644;
		sysfs->attr_externally_control.show
				= charger_externally_control_show;
		sysfs->attr_externally_control.store
				= charger_externally_control_store;

		sysfs_attr_init(&sysfs->attr_unknow_type_cur_control.attr);
		sysfs->attr_unknow_type_cur_control.attr.name = "unknow_type_cur_control";
		sysfs->attr_unknow_type_cur_control.attr.mode = 0644;
		sysfs->attr_unknow_type_cur_control.show = unknow_type_cur_control_show;
		sysfs->attr_unknow_type_cur_control.store = unknow_type_cur_control_store;

		if (!desc->sysfs[i].externally_control || !chargers_externally_control)
			chargers_externally_control = 0;

		dev_info(cm->dev, "regulator's externally_control is %d\n",
			 sysfs->externally_control);
	}

	if (chargers_externally_control) {
		dev_err(cm->dev, "Cannot register regulator because charger-manager"
			"must need at least one charger for charging battery\n");
		return -EINVAL;
	}

	return 0;
}

static int is_between(int left, int right, int value)
{
	if(left >= right && left >= value && value >= right)
		return 1;
	if(left <= right && left <= value && value <= right)
		return 1;
	return 0;
}
static int cm_bat_id_from_voltage(struct charger_manager *cm, int bat_id_adc_mv)
{
	int index;
	for(index = 0;index < r_item_len; index++){
		if(is_between(r_items_param[index].vmin,r_items_param[index].vmax,bat_id_adc_mv))
			return r_items_param[index].id;
	}
	return 0;
}
static int cm_get_bat_id(struct charger_manager *cm)
{
	int bat_id_adc_mv = 0, ret = 0;
	int bat_id = 0;

	if ((r_items_param == r_items_param_secret) || (cm->bat_id != 0))
		return 0;

	if(cm->battery_chan == NULL)
	{
		pr_err("%s:line%d:batt_id_channel error,return.\n", __func__, __LINE__);
		return -EINVAL;
	}
	ret = iio_read_channel_processed(cm->battery_chan, &bat_id_adc_mv);
	if(ret < 0)
		return ret;
	bat_id = cm_bat_id_from_voltage(cm, bat_id_adc_mv);
	cm->bat_id = bat_id;
	cm->bat_id_adc_mv = bat_id_adc_mv;
	pr_err("cm->bat_id:%d cm->bat_id_adc_mv:%d\n",cm->bat_id, cm->bat_id_adc_mv);
	return 0;
}

/*add sys/class/power_supply/usb node */
static ssize_t real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 type = 0;
	int ret;
	struct power_supply *pd_psy = NULL;
	union power_supply_propval pval;

	if (IS_ERR_OR_NULL(g_cm))
		return PTR_ERR(g_cm);

	if (!is_ext_pwr_online(g_cm))
		return sprintf(buf, "%s\n", REAL_TYPE_TEXT[type]);

	ret = charger_get_vbus_type(g_cm->charger, &type);
	pr_info("real_type =%d, %s\n", type, REAL_TYPE_TEXT[type]);

	if (ret < 0)
		pr_err("fail to get bc1p2 type\n");

	pd_psy = power_supply_get_by_name("sprd-tcpm-source-psy-sc27xx-pd");
	if (pd_psy) {
		if (!power_supply_get_property(pd_psy, POWER_SUPPLY_PROP_USB_TYPE, &pval)) {
			if (pval.intval == POWER_SUPPLY_USB_TYPE_PD || pval.intval == POWER_SUPPLY_USB_TYPE_PD_PPS)
				type = POWER_SUPPLY_USB_TYPE_PD;
		}
	}
	return sprintf(buf, "%s\n", REAL_TYPE_TEXT[type]);
}
static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

static ssize_t typec_cc_orientation_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cc_orientation = 0;

	cc_orientation = typec_get_cc_polarity();

	return sprintf(buf, "%d\n", cc_orientation + 1);
}

static struct device_attribute typec_cc_orientation_attr =
	__ATTR(typec_cc_orientation, 0644, typec_cc_orientation_show, NULL);

static ssize_t usb_otg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int usb_otg = 0;

	usb_otg = typec_get_power_role();

	return sprintf(buf, "%d\n", usb_otg);
}
static struct device_attribute usb_otg_attr =
	__ATTR(usb_otg, 0644, usb_otg_show, NULL);

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};
static ssize_t typec_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int typec_mode;
	typec_mode = sc27xx_get_typec_mode();
	return scnprintf(buf, PAGE_SIZE, "%s\n",usb_typec_mode_text[typec_mode]);
}
static struct device_attribute typec_mode_attr =
	__ATTR(typec_mode,0644,typec_mode_show,NULL);

static ssize_t quick_charge_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (IS_ERR_OR_NULL(g_cm))
		return PTR_ERR(g_cm);

	return scnprintf(buf, PAGE_SIZE, "%d\n", g_cm->quick_charge_type);
}

static struct device_attribute quick_charge_type_attr =
	__ATTR(quick_charge_type, 0644, quick_charge_type_show, NULL);

static ssize_t enable_pfm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (IS_ERR_OR_NULL(g_cm))
		return PTR_ERR(g_cm);

	pr_err("func:%s, g_cm->enable_pfm:%d\n", __func__, g_cm->enable_pfm);
	return sprintf(buf, "%d\n", g_cm->enable_pfm);
}

static ssize_t enable_pfm_store(struct device *dev,
				  struct device_attribute *attr, const char *buf,
				  size_t count)
{
	bool value = 0;
	int ret = 0;
	if (IS_ERR_OR_NULL(g_cm))
		return -ENOMEM;
	ret = sscanf(buf, "%d", &value);
	if (!ret)
		return -EINVAL;
//	charger_set_pfm(g_cm->charger, value);
	g_cm->enable_pfm = value;

	pr_err("func:%s, value:%d\n", __func__, value);
	return count;
}
static struct device_attribute enable_pfm_attr =
	__ATTR(enable_pfm, 0644, enable_pfm_show, enable_pfm_store);


static struct attribute *usb_psy_attrs[] = {
	&real_type_attr.attr,
	&typec_cc_orientation_attr.attr,
	&usb_otg_attr.attr,
	&typec_mode_attr.attr,
	&quick_charge_type_attr.attr,
	&enable_pfm_attr.attr,
	NULL,
};
static const struct attribute_group usb_psy_attrs_group = {
	.attrs = usb_psy_attrs,
};
static int cm_usb_sysfs_create_group(struct charger_manager *cm)
{
	return sysfs_create_group(&usb_main.psy->dev.kobj,&usb_psy_attrs_group);
}
/*add sys/class/power_supply/battery node */
static ssize_t shutdown_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!g_cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return sprintf(buf, "%d\n", g_cm->shutdown_delay);
}
static struct device_attribute shutdown_delay_attr =
	__ATTR(shutdown_delay, 0444, shutdown_delay_show, NULL);
static ssize_t input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool input_suspend = false;
	if (IS_ERR_OR_NULL(g_cm)) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return -ENOMEM;
	}
	input_suspend = g_cm->input_suspend;
	return sprintf(buf, "%d\n", input_suspend);
}
static ssize_t input_suspend_store(struct device *dev,
				  struct device_attribute *attr, const char *buf,
				  size_t count)
{
	int input_suspend = 0;
	int ret = 0;
	if (IS_ERR_OR_NULL(g_cm))
		return -ENOMEM;
	ret = sscanf(buf, "%d", &input_suspend);
	if (!ret)
		return -EINVAL;
	if (!input_suspend)
	{
		ret = try_charger_enable(g_cm, true);
		if (ret) {
			dev_err(g_cm->dev, "failed to start charger.\n");
			return ret;
		}
		cm_power_path_enable(g_cm, CM_POWER_PATH_ENABLE_CMD);
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);
		dev_info(g_cm->dev, "start charger by input_suspend.\n");
	}
	else
	{
		ret = try_charger_enable(g_cm, false);
		if (ret) {
			dev_err(g_cm->dev, "failed to stop charger.\n");
			return ret;
		}
		cm_power_path_enable(g_cm, CM_POWER_PATH_DISABLE_CMD);
		dev_info(g_cm->dev, "stop charger by input_suspend.\n");
	}

	g_cm->input_suspend = input_suspend;
	power_supply_changed(g_cm->charger_psy);
	return count;
}
static struct device_attribute input_suspend_attr =
	__ATTR(input_suspend, 0644, input_suspend_show, input_suspend_store);

static ssize_t mtbf_current_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!g_cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", g_cm->mtbf_current);
}
static ssize_t mtbf_current_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	int value;
	if (!g_cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}
	ret =  kstrtoint(buf, 10, &value);
	if (ret)
		return ret;
	if (value < MTBF_CURRENT_MIN) {
		value = MTBF_CURRENT_MIN;
	} else if (value > MTBF_CURRENT_MAX) {
		value = MTBF_CURRENT_MAX;
	}
	g_cm->mtbf_current = value;
	cm_update_charge_info(g_cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				CM_CHARGE_INFO_INPUT_LIMIT));
	dev_info(g_cm->dev, "%s:line%d mtbf_current = %d\n",
		__func__, __LINE__, g_cm->mtbf_current);
	return count;
}
static struct device_attribute mtbf_current_attr =
	__ATTR(mtbf_current, 0644, mtbf_current_show, mtbf_current_store);

static ssize_t authentic_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int is_auth;

	if (CHIP_UNSUPRT == ll_check_secret_chip_online()) {
		is_auth = 0;
		dev_info(dev, "force authentic true\n");
	} else {
		is_auth = lc_is_battery_auth_success();
		if (is_auth)
			dev_err(dev, "%s get battery_auth succ\n", __func__);
		else
			dev_err(dev, "%s get battery_auth fail\n", __func__);
	}

	return sprintf(buf, "%d\n", is_auth);
}

static struct device_attribute authentic_attr =
	__ATTR(authentic, 0644, authentic_show, NULL);

static ssize_t chip_ok_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int is_auth;

	if (CHIP_UNSUPRT == ll_check_secret_chip_online()) {
		is_auth = 0;
		dev_info(dev, "force chip ok true\n");
	} else {
		is_auth = lc_is_battery_auth_success();
		if (is_auth)
			dev_err(dev, "%s get battery_auth succ\n", __func__);
		else
			dev_err(dev, "%s get battery_auth fail\n", __func__);
	}

	return sprintf(buf, "%d\n", is_auth);
}

static struct device_attribute chip_ok_attr =
	__ATTR(chip_ok, 0644, chip_ok_show, NULL);


static ssize_t fake_cycle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_cm->fake_charge_cycle);
}

static ssize_t fake_cycle_store(struct device *dev,
				  struct device_attribute *attr, const char *buf,
				  size_t count)
{
	if (kstrtoint(buf, 10, &g_cm->fake_charge_cycle)) {
		dev_err(dev, "%s write fake cycle failed\n", __func__);
		return -EINVAL;
	}
	dev_info(dev, "%s new fake_charge_cycle:%d\n",
		__func__, g_cm->fake_charge_cycle);

	return count;
}

static struct device_attribute fake_cycle_attr =
	__ATTR(fake_cycle, 0644, fake_cycle_show, fake_cycle_store);

static ssize_t batt_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int batt_id = 0;
	int ret;

	if (IS_ERR_OR_NULL(g_cm)) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return -ENOMEM;
	}

	ret = cm_get_bat_id(g_cm);
	if (!ret)
		batt_id = g_cm->bat_id;

	return sprintf(buf, "%d\n", batt_id);
}

static struct device_attribute batt_id_attr =
	__ATTR(batt_id, 0644, batt_id_show, NULL);

static ssize_t batt_id_voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int batt_id_adc_mv = 0;
	int ret;

	if (IS_ERR_OR_NULL(g_cm)) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return -ENOMEM;
	}

	ret = cm_get_bat_id(g_cm);
	if (!ret)
		batt_id_adc_mv = g_cm->bat_id_adc_mv;

	return sprintf(buf, "%d\n", batt_id_adc_mv);
}

static struct device_attribute batt_id_voltage_attr =
	__ATTR(batt_id_voltage, 0644, batt_id_voltage_show, NULL);

static ssize_t resistance_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int batt_id = 0;
	int ret;

	if (IS_ERR_OR_NULL(g_cm)) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return -ENOMEM;
	}

	ret = cm_get_bat_id(g_cm);
	if (!ret)
		batt_id = g_cm->bat_id;

	return sprintf(buf, "%d\n", r_items_param[batt_id].bat_resistance_id);
}

static struct device_attribute resistance_id_attr =
	__ATTR(resistance_id, 0644, resistance_id_show, NULL);

static ssize_t manufacturer_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int batt_id = 0;
	int ret;

	if (IS_ERR_OR_NULL(g_cm)) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return -ENOMEM;
	}

	ret = cm_get_bat_id(g_cm);
	if (!ret)
		batt_id = g_cm->bat_id;

	return sprintf(buf, "%s\n", r_items_param[batt_id].manufacturer);
}

static struct device_attribute manufacturer_attr =
	__ATTR(manufacturer, 0644, manufacturer_show, NULL);


static ssize_t shipmode_count_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (IS_ERR_OR_NULL(g_cm)) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return -ENOMEM;
	}

	return sprintf(buf, "%d\n", g_cm->shipmode_flag);
}
static ssize_t shipmode_count_reset_store(struct device *dev,
				  struct device_attribute *attr, const char *buf,
				  size_t count)
{
	bool shipmode_flag = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(g_cm))
		return -ENOMEM;

	ret = sscanf(buf, "%d", &shipmode_flag);
	if (!ret)
		return -EINVAL;

	g_cm->shipmode_flag = shipmode_flag;
	return count;
}
static struct device_attribute shipmode_count_reset_attr =
	__ATTR(shipmode_count_reset, 0644, shipmode_count_reset_show, shipmode_count_reset_store);

static ssize_t sw_manufacturer_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	union power_supply_propval val = {0, };
	struct power_supply *chg_psy = NULL;
	if (!g_cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}
	chg_psy = power_supply_get_by_name(g_cm->desc->psy_charger_stat[0]);
	if (chg_psy) {
		ret = power_supply_get_property(chg_psy, POWER_SUPPLY_PROP_MANUFACTURER, &val);
		if (ret){
			pr_err("get charger ic name fail, ret=%d \n", ret);
			return -ENOMEM;
		}
	}
	return sprintf(buf, "%s\n", val.strval);
}
static struct device_attribute sw_manufacturer_attr =
	__ATTR(sw_manufacturer, 0644, sw_manufacturer_show, NULL);

static ssize_t battery_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int batt_id = 0;
	int ret;

	if (IS_ERR_OR_NULL(g_cm)) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return -ENOMEM;
	}

	ret = cm_get_bat_id(g_cm);
	if (!ret)
		batt_id = g_cm->bat_id;

	return sprintf(buf, "%s\n", r_items_param[batt_id].batt_type);
}
static struct device_attribute battery_type_attr =
	__ATTR(battery_type, 0644, battery_type_show, NULL);

static ssize_t cid_enable_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	bool cid_enable = false;

	if (IS_ERR_OR_NULL(g_cm)){
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return PTR_ERR(g_cm);
	}
	cid_enable = g_cm->cid_enable;
	pr_err("%s cid_enable is %d \n", __func__, cid_enable);
	return snprintf(buf, PAGE_SIZE, "%d\n", cid_enable);
}

static ssize_t cid_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	int ret;

	if (IS_ERR_OR_NULL(g_cm)){
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return PTR_ERR(g_cm);
	}
	ret =  kstrtoint(buf, 10, &value);
	if (ret) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return ret;
	}
	g_cm->cid_enable = !!value;
	pr_err("%s cid_enable is settled to %d \n", __func__, g_cm->cid_enable);
	return count;
}

static struct device_attribute cid_enable_attr =
	__ATTR(cid_enable, 0644, cid_enable_show, cid_enable_store);

static ssize_t screen_on_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	bool screen_on = false;

	if (IS_ERR_OR_NULL(g_cm)){
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return PTR_ERR(g_cm);
	}
	screen_on = g_cm->screen_on;
	pr_err("%s screen_on is %d \n", __func__, screen_on);
	return snprintf(buf, PAGE_SIZE, "%d\n", screen_on);
}

static ssize_t screen_on_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	int value;
	int ret;

	if (IS_ERR_OR_NULL(g_cm)){
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return PTR_ERR(g_cm);
	}
	ret =  kstrtoint(buf, 10, &value);
	if (ret) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return ret;
	}
	g_cm->screen_on = !!value;
	schedule_delayed_work(&g_cm->cid_detect_work, msecs_to_jiffies(100));
	return count;
}

static struct device_attribute screen_on_attr =
	__ATTR(screen_on, 0644, screen_on_show, screen_on_store);
static ssize_t audio_on_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	bool audio_on = false;

	if (IS_ERR_OR_NULL(g_cm)){
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return PTR_ERR(g_cm);
	}
	audio_on = g_cm->audio_on;
	pr_err("%s audio_on is %d \n", __func__, audio_on);
	return snprintf(buf, PAGE_SIZE, "%d\n", audio_on);
}

static ssize_t audio_on_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	int value;
	int ret;

	if (IS_ERR_OR_NULL(g_cm)){
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return PTR_ERR(g_cm);
	}
	ret =  kstrtoint(buf, 10, &value);
	if (ret) {
		pr_err("%s: Couldn't get the g_cm \n", __func__);
		return ret;
	}
	g_cm->audio_on = !!value;
	schedule_delayed_work(&g_cm->cid_detect_work, msecs_to_jiffies(200));
	pr_err("%s audio_on is settled to %d \n", __func__, g_cm->audio_on);
	return count;
}

static struct device_attribute audio_on_attr =
	__ATTR(audio_on, 0644, audio_on_show, audio_on_store);

static ssize_t charger_partition_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	static charger_partition_info_1 *info_1 = NULL;

	/* 分配缓冲区 */
	info_1 = kzalloc(CHARGER_CONFIG_SIZE, GFP_KERNEL);
	if (!info_1) {
		pr_err("Failed to allocate config buffer\n");
		return -EINVAL;
	}

	/* 读取分区数据 */
	ret = charger_partition_read(CHARGER_PARTITION_NAME, info_1, CHARGER_CONFIG_SIZE, (CHARGER_PARTITION_INFO_1 * CHARGER_PARTITION_RWSIZE));
	if (ret < 0) {
		pr_err("Failed to read charger config\n");
		return -EINVAL;
	}
	pr_info("[charger] %s ret: %d, info_1->power_off_mode: %u\n", __func__, ret, info_1->test);
	return sprintf(buf, "%d\n", info_1->test);
}

static ssize_t charger_partition_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	uint64_t offset;
	static charger_partition_info_1 *info_1 = NULL;
	info_1 = kzalloc(CHARGER_CONFIG_SIZE, GFP_KERNEL);
	if (!info_1) {
		pr_err("Failed to allocate config buffer\n");
		return -EINVAL;
	}

	if (kstrtoint(buf, 10, &info_1->test)) {
		pr_err("parsing number fail\n");
		return -EINVAL;
	}
	offset = CHARGER_PARTITION_INFO_1 * CHARGER_PARTITION_RWSIZE;
	/* 写入分区数据 */
	ret = charger_partition_write(CHARGER_PARTITION_NAME, info_1, CHARGER_PARTITION_RWSIZE, offset);
	if (ret < 0) {
		pr_err("Failed to read charger config\n");
		return -EINVAL;
	}

	return count;
}
static struct device_attribute charger_partition_test_attr =
__ATTR(charger_partition_test, 0644, charger_partition_test_show, charger_partition_test_store);

static ssize_t charger_partition_poweroffmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	static charger_partition_info_1 *info_1 = NULL;

	/* 分配缓冲区 */
	info_1 = kzalloc(CHARGER_CONFIG_SIZE, GFP_KERNEL);
	if (!info_1) {
		pr_err("Failed to allocate config buffer\n");
		return -EINVAL;
	}

	/* 读取分区数据 */
	ret = charger_partition_read(CHARGER_PARTITION_NAME, info_1, CHARGER_CONFIG_SIZE, (CHARGER_PARTITION_INFO_1 * CHARGER_PARTITION_RWSIZE));
	if (ret < 0) {
		pr_err("Failed to read charger config\n");
		return -EINVAL;
	}
	pr_info("[charger] %s ret: %d, info_1->power_off_mode: %u\n", __func__, ret, info_1->power_off_mode);
	return sprintf(buf, "%d\n", info_1->power_off_mode);
}

static ssize_t charger_partition_poweroffmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = count;
	uint64_t offset;
	charger_partition_info_1 *info_1 = kzalloc(CHARGER_CONFIG_SIZE, GFP_KERNEL);
	if (!info_1) {
		pr_err("Failed to allocate config buffer\n");
		return -EINVAL;
	}
	info_1->power_off_mode = 2;
	info_1->zero_speed_mode = 2;
	info_1->test = 0x34567890;
	info_1->reserved = 0;

	if (kstrtoint(buf, 10, &info_1->power_off_mode)) {
		pr_err("parsing number fail\n");
		ret = -EINVAL;
		goto out_free;
	}
	offset = CHARGER_PARTITION_INFO_1 * CHARGER_PARTITION_RWSIZE;
	/* 写入分区数据 */
	ret = charger_partition_write(CHARGER_PARTITION_NAME, info_1, CHARGER_PARTITION_RWSIZE, offset);
	if (ret < 0) {
		pr_err("Failed to read charger config\n");
		ret = -EINVAL;
		goto out_free;
	}
	ret  = count;
out_free:
	kfree(info_1);
	return ret;
}
static struct device_attribute charger_partition_poweroffmode_attr =
__ATTR(charger_partition_poweroffmode, 0644, charger_partition_poweroffmode_show, charger_partition_poweroffmode_store);

static struct attribute *batt_psy_attrs[] = {
	&shutdown_delay_attr.attr,
	&input_suspend_attr.attr,
	&mtbf_current_attr.attr,
	&authentic_attr.attr,
	&chip_ok_attr.attr,
	&fake_cycle_attr.attr,
	&batt_id_attr.attr,
	&shipmode_count_reset_attr.attr,
	&batt_id_voltage_attr.attr,
	&resistance_id_attr.attr,
	&manufacturer_attr.attr,
	&sw_manufacturer_attr.attr,
	&battery_type_attr.attr,
	&cid_enable_attr.attr,
	&screen_on_attr.attr,
	&audio_on_attr.attr,
	&charger_partition_test_attr.attr,
	&charger_partition_poweroffmode_attr.attr,
	&otg_debug_attr.attr,
	NULL,
};
static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};
static int cm_batt_sysfs_create_group(struct charger_manager *cm)
{
	struct power_supply *psy = NULL;
	psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(psy)) {
		pr_err("couldn't get psy of battery!\n");
		return -ENODEV;
	}
	cm = power_supply_get_drvdata(psy);
	if (IS_ERR_OR_NULL(cm)) {
		pr_err("couldn't get cm of battery!\n");
		return -ENODEV;
	}else{
		return sysfs_create_group(&psy->dev.kobj,&batt_psy_attrs_group);
	}
}

static int cm_init_thermal_data(struct charger_manager *cm,
				struct power_supply *fuel_gauge,
				enum power_supply_property *properties,
				size_t *num_properties)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	int ret;

	/* Verify whether fuel gauge provides battery temperature */
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TEMP, &val);

	if (!ret) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_TEMP,
						 properties, num_properties))
			dev_warn(cm->dev, "POWER_SUPPLY_PROP_TEMP is present\n");
		cm->desc->measure_battery_temp = true;
	}
#if IS_ENABLED(CONFIG_THERMAL)
	if (desc->thermal_zone) {
		cm->tzd_batt =
			thermal_zone_get_zone_by_name(desc->thermal_zone);
		if (IS_ERR(cm->tzd_batt))
			return PTR_ERR(cm->tzd_batt);

		/* Use external thermometer */
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_TEMP_AMBIENT, properties, num_properties))
			dev_warn(cm->dev, "POWER_SUPPLY_PROP_TEMP_AMBIENT is present\n");
		cm->desc->measure_battery_temp = true;
		ret = 0;
	}
#endif
	if (cm->desc->measure_battery_temp) {
		/* NOTICE : Default allowable minimum charge temperature is 0 */
		if (!desc->temp_max)
			desc->temp_max = CM_DEFAULT_CHARGE_TEMP_MAX;
		if (!desc->temp_diff)
			desc->temp_diff = CM_DEFAULT_RECHARGE_TEMP_DIFF;
	}

	return ret;
}

static int cm_init_jeita_table(struct sprd_battery_info *info,
			       struct charger_desc *desc, struct device *dev)
{
	int i;

	for (i = SPRD_BATTERY_JEITA_DCP; i < SPRD_BATTERY_JEITA_MAX; i++) {
		desc->jeita_size[i] = info->sprd_battery_jeita_size[i];
		if (!desc->jeita_size[i]) {
			dev_dbg(dev, "%s jeita_size is zero\n",
				 sprd_battery_jeita_type_names[i]);
			continue;
		}

		desc->max_current_jeita_index[i] = info->max_current_jeita_index[i];

		desc->jeita_tab_array[i] = devm_kmemdup(dev, info->jeita_table[i],
							desc->jeita_size[i] *
							sizeof(struct sprd_battery_jeita_table),
							GFP_KERNEL);
		if (!desc->jeita_tab_array[i]) {
			dev_warn(dev, "Fail to kmemdup %s\n",
				 sprd_battery_jeita_type_names[i]);
			return -ENOMEM;
		}
	}

	desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_UNKNOWN];
	desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN];
	jeita_info_init(&desc->jeita_info);

	return 0;
}

static bool cm_init_step_chg_table(struct sprd_battery_info *info,
				   struct charger_desc *desc, struct device *dev)
{
	if (!info->sprd_battery_step_chg_size || !info->step_chg_table)
		return false;

	desc->step_chg_table_size = info->sprd_battery_step_chg_size;

	desc->step_chg_table = devm_kmemdup(dev, info->step_chg_table,
					    desc->step_chg_table_size *
					    sizeof(struct sprd_battery_step_chg_table),
					    GFP_KERNEL);
	if (!desc->step_chg_table) {
		dev_warn(dev, "%s, fail to kmemdup %s\n",
			 __func__, SPRD_BATTERY_STEP_CHG_TABLE_NAME);
		return false;
	}

	return true;
}

static const struct of_device_id charger_manager_match[] = {
	{
		.compatible = "charger-manager",
	},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static int cm_parse_alt_psy_desc(struct device *dev, struct charger_desc *desc)
{
	struct device_node *np = dev->of_node;
	int i, num_psys;

	desc->enable_alt_charger_adapt =
		device_property_read_bool(dev, "cm-alt-charger-adapt-enable");
	if (!desc->enable_alt_charger_adapt)
		goto done1;

	/* alternative charger power supply */
	num_psys = of_property_count_strings(np, "cm-alt-charger-power-supplys");
	if (num_psys > 0) {
		desc->alt_charger_nums = num_psys;
		/* Allocate empty bin at the tail of array */
		desc->psy_alt_charger_adpt_stat = devm_kzalloc(dev,
							       sizeof(char *) * (u32)(num_psys + 1),
							       GFP_KERNEL);
		if (!desc->psy_alt_charger_adpt_stat)
			return -ENOMEM;

		for (i = 0; i < num_psys; i++)
			of_property_read_string_index(np, "cm-alt-charger-power-supplys", i,
						      &desc->psy_alt_charger_adpt_stat[i]);
	}

done1:
	desc->enable_alt_cp_adapt =
		device_property_read_bool(dev, "cm-alt-cp-adapt-enable");
	if (!desc->enable_alt_cp_adapt)
		return 0;

	/* alternative charge pupms power supply */
	num_psys = of_property_count_strings(np, "cm-alt-cp-power-supplys");
	if (num_psys > 0) {
		desc->alt_cp_nums = num_psys;
		/* Allocate empty bin at the tail of array */
		desc->psy_alt_cp_adpt_stat = devm_kzalloc(dev, sizeof(char *) * (u32)(num_psys + 1),
							  GFP_KERNEL);
		if (!desc->psy_alt_cp_adpt_stat)
			return -ENOMEM;

		for (i = 0; i < num_psys; i++)
			of_property_read_string_index(np, "cm-alt-cp-power-supplys", i,
						      &desc->psy_alt_cp_adpt_stat[i]);
	}

	return 0;
}

static int cm_parse_psy_desc(struct device *dev, struct charger_desc *desc)
{
	struct device_node *np = dev->of_node;
	int i, num_chgs;

	/* chargers */
	num_chgs = of_property_count_strings(np, "cm-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_charger_stat = devm_kzalloc(dev, sizeof(char *) * (u32)(num_chgs + 1),
						      GFP_KERNEL);
		if (!desc->psy_charger_stat)
			return -ENOMEM;

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-chargers", i,
						      &desc->psy_charger_stat[i]);
	}

	/* charge pumps */
	num_chgs = of_property_count_strings(np, "cm-charge-pumps");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->cp_nums = num_chgs;
		desc->psy_cp_stat = devm_kzalloc(dev, sizeof(char *) * (u32)(num_chgs + 1),
						 GFP_KERNEL);
		if (!desc->psy_cp_stat)
			return -ENOMEM;

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-charge-pumps", i,
						      &desc->psy_cp_stat[i]);
	}

	/* wireless chargers */
	num_chgs = of_property_count_strings(np, "cm-wireless-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_wl_charger_stat = devm_kzalloc(dev, sizeof(char *) * (u32)(num_chgs + 1),
							 GFP_KERNEL);
		if (!desc->psy_wl_charger_stat)
			return -ENOMEM;

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-wireless-chargers", i,
						      &desc->psy_wl_charger_stat[i]);
	}

	/* wireless charge pump converters */
	num_chgs = of_property_count_strings(np, "cm-wireless-charge-pump-converters");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_cp_converter_stat = devm_kzalloc(dev,
							   sizeof(char *) * (u32)(num_chgs + 1),
							   GFP_KERNEL);
		if (!desc->psy_cp_converter_stat)
			return -ENOMEM;

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-wireless-charge-pump-converters", i,
						      &desc->psy_cp_converter_stat[i]);
	}

	return 0;
}

static void cm_parse_buck_parameter(struct device *dev, struct charger_desc *desc)
{
	struct device_node *np = dev->of_node;

	if (device_property_read_bool(dev, "cm-cp-buck-tgt-work")) {
		of_property_read_u32_index(np, "cm-cp-buck-tgt-work", 0,
					   &desc->cp_sm.buck_info.buck_start_work_temp_th);
		of_property_read_u32_index(np, "cm-cp-buck-tgt-work", 1,
					   &desc->cp_sm.buck_info.buck_start_work_ibat_th);
		of_property_read_u32_index(np, "cm-cp-buck-tgt-work", 2,
					   &desc->cp_sm.buck_info.buck_ibat_max_p);
		of_property_read_u32_index(np, "cm-cp-buck-tgt-work", 3,
					   &desc->cp_sm.buck_info.buck_default_ibat_max);
		if (desc->cp_sm.buck_info.buck_start_work_ibat_th > 0 &&
		    desc->cp_sm.buck_info.buck_default_ibat_max > 0 &&
		    desc->cp_sm.buck_info.buck_ibat_max_p > 0 &&
		    desc->cp_sm.buck_info.buck_ibat_max_p <= 100)
			desc->support_cp_buck_work_tgt = true;
		else
			dev_err(dev, "%s, dts parameter are abnormal!!!\n", __func__);
	}
}

static struct charger_desc *of_cm_parse_desc(struct device *dev)
{
	struct charger_desc *desc;
	struct device_node *np = dev->of_node;
	u32 poll_mode = CM_POLL_DISABLE;
	u32 battery_stat = CM_NO_BATTERY;
	int ret;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "cm-name", &desc->psy_name);

	of_property_read_u32(np, "cm-poll-mode", &poll_mode);
	desc->polling_mode = poll_mode;

	desc->uvlo_shutdown_mode = CM_SHUTDOWN_MODE_ANDROID;
	of_property_read_u32(np, "cm-uvlo-shutdown-mode", &desc->uvlo_shutdown_mode);

	of_property_read_u32(np, "cm-poll-interval",
				&desc->polling_interval_ms);

	of_property_read_u32(np, "cm-fullbatt-vchkdrop-ms",
					&desc->fullbatt_vchkdrop_ms);
	of_property_read_u32(np, "cm-fullbatt-vchkdrop-volt",
					&desc->fullbatt_vchkdrop_uV);
	of_property_read_u32(np, "cm-fullbatt-soc", &desc->fullbatt_soc);
	of_property_read_u32(np, "cm-fullbatt-capacity",
					&desc->fullbatt_full_capacity);
	of_property_read_u32(np, "cm-shutdown-voltage", &desc->shutdown_voltage);
	of_property_read_u32(np, "cm-tickle-time-out", &desc->trickle_time_out);
	of_property_read_u32(np, "cm-one-cap-time", &desc->cap_one_time);
	of_property_read_u32(np, "cm-one-cap-time", &desc->default_cap_one_time);
	of_property_read_u32(np, "cm-wdt-interval", &desc->wdt_interval);

	of_property_read_u32(np, "cm-battery-stat", &battery_stat);
	desc->battery_present = battery_stat;

	cm_parse_buck_parameter(dev, desc);

	ret = cm_parse_alt_psy_desc(dev, desc);
	if (ret)
		return ERR_PTR(ret);

	ret = cm_parse_psy_desc(dev, desc);
	if (ret)
		return ERR_PTR(ret);

	of_property_read_string(np, "cm-fuel-gauge", &desc->psy_fuel_gauge);

	of_property_read_string(np, "cm-thermal-zone", &desc->thermal_zone);

	of_property_read_u32(np, "cm-battery-cold", &desc->temp_min);
	if (of_get_property(np, "cm-battery-cold-in-minus", NULL))
		desc->temp_min *= -1;
	of_property_read_u32(np, "cm-battery-hot", &desc->temp_max);
	of_property_read_u32(np, "cm-battery-temp-diff", &desc->temp_diff);

	of_property_read_u32(np, "cm-charging-max",
				&desc->charging_max_duration_ms);
	of_property_read_u32(np, "cm-discharging-max",
				&desc->discharging_max_duration_ms);
	of_property_read_u32(np, "cm-charge-voltage-max",
			     &desc->normal_charge_voltage_max);
	of_property_read_u32(np, "cm-charge-voltage-drop",
			     &desc->normal_charge_voltage_drop);
	of_property_read_u32(np, "cm-fast-charge-voltage-max",
			     &desc->fast_charge_voltage_max);
	of_property_read_u32(np, "cm-fast-charge-voltage-drop",
			     &desc->fast_charge_voltage_drop);
	of_property_read_u32(np, "cm-flash-charge-voltage-max",
			     &desc->flash_charge_voltage_max);
	of_property_read_u32(np, "cm-flash-charge-voltage-drop",
			     &desc->flash_charge_voltage_drop);
	of_property_read_u32(np, "cm-wireless-charge-voltage-max",
			     &desc->wireless_normal_charge_voltage_max);
	of_property_read_u32(np, "cm-wireless-charge-voltage-drop",
			     &desc->wireless_normal_charge_voltage_drop);
	of_property_read_u32(np, "cm-wireless-fast-charge-voltage-max",
			     &desc->wireless_fast_charge_voltage_max);
	of_property_read_u32(np, "cm-wireless-fast-charge-voltage-drop",
			     &desc->wireless_fast_charge_voltage_drop);
	of_property_read_u32(np, "cm-cp-taper-current",
			     &desc->cp_sm.taper_current);

	if (desc->psy_cp_stat && !desc->cp_sm.taper_current)
		desc->cp_sm.taper_current = CM_CP_DEFAULT_TAPER_CURRENT;

	return desc;
}

static inline struct charger_desc *cm_get_drv_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		return of_cm_parse_desc(&pdev->dev);
	return dev_get_platdata(&pdev->dev);
}

static int cm_get_bat_info(struct charger_manager *cm, int bat_aging_id)
{
	struct sprd_battery_info info = {};
	int ret;

	ret = sprd_battery_get_battery_info(cm->charger_psy, &info, bat_aging_id);
	if (ret) {
		dev_err(cm->dev, "failed to get battery information\n");
		sprd_battery_put_battery_info(cm->charger_psy, &info);
		return ret;
	}

	cm->desc->internal_resist = info.factory_internal_resistance_uohm / 1000;
	cm->desc->ir_comp.us = info.constant_charge_voltage_max_uv;
	cm->desc->ir_comp.us_upper_limit = info.ir.us_upper_limit_uv;
	cm->desc->ir_comp.rc = info.ir.rc_uohm / 1000;
	cm->desc->ir_comp.cp_upper_limit_offset = info.ir.cv_upper_limit_offset_uv;
	cm->desc->constant_charge_voltage_max_uv = info.constant_charge_voltage_max_uv;
	cm->desc->fullbatt_voltage_offset_uv = info.fullbatt_voltage_offset_uv;
	cm->desc->fchg_ocv_threshold = info.fast_charge_ocv_threshold_uv;
	cm->desc->cp_sm.default_max_ibat = info.cur.flash_cur;
	cm->desc->cp_sm.default_max_ibus = info.cur.flash_limit;
	cm->desc->cur.sdp_limit = info.cur.sdp_limit;
	cm->desc->cur.sdp_cur = info.cur.sdp_cur;
	cm->desc->cur.dcp_limit = info.cur.dcp_limit;
	cm->desc->cur.dcp_cur = info.cur.dcp_cur;
	cm->desc->cur.cdp_limit = info.cur.cdp_limit;
	cm->desc->cur.cdp_cur = info.cur.cdp_cur;
	cm->desc->cur.unknown_limit = info.cur.unknown_limit;
	cm->desc->cur.unknown_cur = info.cur.unknown_cur;
	cm->desc->cur.fchg_limit = info.cur.fchg_limit;
	cm->desc->cur.fchg_cur = info.cur.fchg_cur;
	cm->desc->cur.flash_limit = info.cur.flash_limit;
	cm->desc->cur.flash_cur = info.cur.flash_cur;
	cm->desc->cur.wl_bpp_limit = info.cur.wl_bpp_limit;
	cm->desc->cur.wl_bpp_cur = info.cur.wl_bpp_cur;
	cm->desc->cur.wl_epp_limit = info.cur.wl_epp_limit;
	cm->desc->cur.wl_epp_cur = info.cur.wl_epp_cur;
	cm->desc->fullbatt_uV = info.fullbatt_voltage_uv;
	cm->desc->origin_fullbatt_uV = info.fullbatt_voltage_uv;
	cm->desc->fullbatt_uA = info.fullbatt_current_uA;
	cm->desc->origin_fullbatt_uA = info.fullbatt_current_uA;
	cm->desc->first_fullbatt_uA = info.first_fullbatt_current_uA;
	cm->desc->charge_term_ua = info.charge_term_current_ua;
	cm->desc->jeita_charge_term_ma = cm->desc->charge_term_ua / 1000;

	dev_info(cm->dev, "SPRD_BATTERY_INFO: internal_resist= %d, us= %d, constant_charge_voltage_max_uv= %d, fchg_ocv_threshold= %d, sdp_limit= %d, sdp_cur= %d, dcp_limit= %d, dcp_cur= %d, cdp_limit= %d, cdp_cur= %d unknown_limit= %d, unknown_cur= %d, fchg_limit= %d, fchg_cur= %d, flash_limit= %d, flash_cur= %d, wl_bpp_limit= %d, wl_bpp_cur= %d, wl_epp_limit= %d, wl_epp_cur= %d, fullbatt_uV= %d, fullbatt_uA= %d, cm->desc->first_fullbatt_uA= %d, us_upper_limit= %d, rc= %d, cp_upper_limit_offset= %d, charge_term_ua= %d\n",
		 cm->desc->internal_resist, cm->desc->ir_comp.us,
		 cm->desc->constant_charge_voltage_max_uv, cm->desc->fchg_ocv_threshold,
		 cm->desc->cur.sdp_limit,
		 cm->desc->cur.sdp_cur, cm->desc->cur.dcp_limit, cm->desc->cur.dcp_cur,
		 cm->desc->cur.cdp_limit, cm->desc->cur.cdp_cur, cm->desc->cur.unknown_limit,
		 cm->desc->cur.unknown_cur, cm->desc->cur.fchg_limit, cm->desc->cur.fchg_cur,
		 cm->desc->cur.flash_limit, cm->desc->cur.flash_cur, cm->desc->cur.wl_bpp_limit,
		 cm->desc->cur.wl_bpp_cur, cm->desc->cur.wl_epp_limit, cm->desc->cur.wl_epp_cur,
		 cm->desc->fullbatt_uV, cm->desc->fullbatt_uA, cm->desc->first_fullbatt_uA,
		 cm->desc->ir_comp.us_upper_limit, cm->desc->ir_comp.rc,
		 cm->desc->ir_comp.cp_upper_limit_offset, cm->desc->charge_term_ua);

	ret = cm_init_jeita_table(&info, cm->desc, cm->dev);
	if (ret) {
		sprd_battery_put_battery_info(cm->charger_psy, &info);
		return ret;
	}

	cm->desc->support_step_chg = cm_init_step_chg_table(&info, cm->desc, cm->dev);
	dev_info(cm->dev, "%s, step charging: %s\n",
		 __func__, cm->desc->support_step_chg ? "support" : "nonsupport");

	if (cm->desc->fullbatt_uV == 0)
		dev_info(cm->dev, "Ignoring full-battery voltage threshold as it is not supplied\n");

	if (cm->desc->fullbatt_uA == 0)
		dev_info(cm->dev, "Ignoring full-battery current threshold as it is not supplied\n");

	if (cm->desc->fullbatt_voltage_offset_uv == 0)
		dev_info(cm->dev, "Ignoring full-battery voltage offset as it is not supplied\n");

	sprd_battery_put_battery_info(cm->charger_psy, &info);

	if (cm->desc->cur.fchg_limit > 0 && cm->desc->cur.fchg_cur > 0)
		cm->desc->support_fixed_fchg = true;

	if (cm->desc->cur.flash_limit > 0 && cm->desc->cur.flash_cur > 0)
		cm->desc->support_adaptive_fchg = true;

	return 0;
}

static void cm_batt_aging_algo(struct charger_manager *cm)
{
	int ret = 0, bat_aging_id;

	ret = cm_get_bat_aging_id(cm, &bat_aging_id);
	if (ret)
		return;

	cm_get_bat_info(cm, bat_aging_id);
}

static void cm_shutdown_handle(struct charger_manager *cm)
{
	pr_err("%s: shutdown mode = %d\n", __func__, cm->desc->uvlo_shutdown_mode);
	switch (cm->desc->uvlo_shutdown_mode) {
	case CM_SHUTDOWN_MODE_ORDERLY:
		orderly_poweroff(true);
		break;

	case CM_SHUTDOWN_MODE_KERNEL:
		kernel_power_off();
		break;

	case CM_SHUTDOWN_MODE_ANDROID:
//		cancel_delayed_work_sync(&cm->cap_update_work);
		cm->desc->cap = 0;
		power_supply_changed(cm->charger_psy);
		break;

	default:
		dev_warn(cm->dev, "Incorrect uvlo_shutdown_mode (%d)\n",
			 cm->desc->uvlo_shutdown_mode);
	}
}

static void cm_uvlo_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, uvlo_work);
	int batt_uV, ret;

	if (unlikely(cm->shutdown_flag)) {
		dev_info(cm->dev, "don't check uvlo when shutdown\n");
		return;
	}

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret || batt_uV < 0) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		return;
	}

	dev_dbg(cm->dev, "%s:line%d: batt_uV = %d\n", __func__, __LINE__, batt_uV);
	if ((u32)batt_uV <= cm->desc->shutdown_voltage)
		cm->desc->uvlo_trigger_cnt++;
	else
		cm->desc->uvlo_trigger_cnt = 0;

	if (cm->desc->uvlo_trigger_cnt >= CM_UVLO_CALIBRATION_CNT_THRESHOLD) {
		if (DIV_ROUND_CLOSEST(cm->desc->cap, 10) <= 1) {
			dev_err(cm->dev, "WARN: trigger uvlo, will shutdown with uisoc less than 1%%\n");
			cm_shutdown_handle(cm);
		} else if ((u32)batt_uV <= cm->desc->shutdown_voltage) {
			dev_err(cm->dev, "WARN: batt_uV less than shutdown voltage, will shutdown,"
				"and force capacity to 0%%\n");
			set_batt_cap(cm, 0);
			cm_shutdown_handle(cm);
		}
	}

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD)
		queue_delayed_work(system_unbound_wq, &cm->uvlo_work, msecs_to_jiffies(800));
}

static int cm_get_charging_works_cycle(struct charger_manager *cm,
				       int ibat_avg_ma, int *work_cycle)
{
	int ret = 0, one_cap_time, mas_one_percent, total_uah, total_mah;

	*work_cycle = CM_CAP_CYCLE_TRACK_TIME_15S;

	if (ibat_avg_ma <= 0)
		return ret;

	ret = get_batt_total_uah(cm, &total_uah);
	if (ret) {
		dev_err(cm->dev, "%s failed to get total uah.\n", __func__);
		return ret;
	}

	/*
	 * When fast charging and high current charging,
	 * the work cycle needs to be updated according to the current value.
	 * formula: 1 mAh = 1mA * 3600s = 3600mAs.
	 * mas_one_percent = 3600mAs / 100.
	 * mas_one_percent represents how many mAs there are in 1% battery capacity.
	 * one_cap_time = mas_one_percent / ibat_avg_ma.
	 * one_cap_time represents 1% battery capacity the fastest update time(unit/s).
	 */
	total_mah = total_uah / 1000;
	mas_one_percent = total_mah * 3600 / 100;
	one_cap_time = DIV_ROUND_CLOSEST(mas_one_percent, ibat_avg_ma);
	if (one_cap_time <= 10) {
		cm->desc->cap_one_time = CM_CAP_ONE_TIME_4S;
		*work_cycle = CM_CAP_CYCLE_TRACK_TIME_2S;
	} else if (one_cap_time <= 20) {
		cm->desc->cap_one_time = CM_CAP_ONE_TIME_8S;
		*work_cycle = CM_CAP_CYCLE_TRACK_TIME_4S;
	} else if (one_cap_time < 30) {
		cm->desc->cap_one_time = CM_CAP_ONE_TIME_16S;
		*work_cycle = CM_CAP_CYCLE_TRACK_TIME_8S;
	}

	return ret;
}

static int cm_get_discharging_works_cycle(struct charger_manager *cm,
					  int ibat_avg_ua, int *work_cycle)
{
	int ret = 0, batt_uV;
	static int uvlo_check_cnt;

	*work_cycle = CM_CAP_CYCLE_TRACK_TIME_15S;

	if (ibat_avg_ua >= 0)
		return ret;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "%s failed to get vbat_now_uV\n", __func__);
		return ret;
	}

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD) {
		if (++uvlo_check_cnt > 2) {
			cm->desc->cap_one_time = CM_CAP_ONE_TIME_16S;
			*work_cycle = CM_CAP_CYCLE_TRACK_TIME_8S;
		}
	} else {
		uvlo_check_cnt = 0;
	}

	return ret;
}

static int cm_calc_batt_works_cycle(struct charger_manager *cm, int uisoc)
{
	int ibat_avg_ua, ret = 0, bat_soc, work_cycle = CM_CAP_CYCLE_TRACK_TIME_15S, bat_temp = 250;

	cm->desc->cap_one_time = cm->desc->default_cap_one_time;

	ret = cm_get_battery_temperature(cm, &bat_temp);
	if (ret) {
		dev_err(cm->dev, "%s failed to get battery temperature\n", __func__);
		goto out;
	}

	ret = get_ibat_avg_uA(cm, &ibat_avg_ua);
	if (ret) {
		dev_err(cm->dev, "%s failed to get ibat_avg_uA.\n", __func__);
		goto out;
	}

	ret = cm_get_charging_works_cycle(cm, ibat_avg_ua / 1000, &work_cycle);
	if (ret || work_cycle != CM_CAP_CYCLE_TRACK_TIME_15S)
		goto out;

	ret = cm_get_discharging_works_cycle(cm, ibat_avg_ua, &work_cycle);
	if (ret || work_cycle != CM_CAP_CYCLE_TRACK_TIME_15S)
		goto out;

	ret = get_batt_cap(cm, &bat_soc);
	if (ret) {
		dev_err(cm->dev, "%s failed to get bat_soc\n", __func__);
		goto out;
	}

	bat_soc = clamp(bat_soc, 0, CM_CAP_FULL_PERCENT);

	if (bat_temp < CM_CAP_CALC_BATT_WORKS_LOW_TEMP || uisoc < CM_CAP_CALC_BATT_WORKS_LOW_CAP ||
	    abs(ibat_avg_ua) > CM_CAP_CALC_BATT_WORKS_BIG_CUR_UA ||
	    abs(bat_soc - uisoc) > CM_CAP_CALC_BATT_WORKS_SOC_GAP) {
		cm->desc->cap_one_time = CM_CAP_ONE_TIME_16S;
		work_cycle = CM_CAP_CYCLE_TRACK_TIME_8S;
	}

out:
	/*adjust work cycle for heavier loading in low temp*/
	if(bat_temp < 0 && uisoc < 350){
		dev_info(cm->dev, "LCY, battery loading is too high, should more quick update batt monitor\n");
		if(ibat_avg_ua <= -1500)
			work_cycle = 1;
		else if(ibat_avg_ua < -1200)
			work_cycle = 3;
	}
	return work_cycle;
}

static void cm_batt_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, cap_update_work);
	struct charger_desc *desc = cm->desc;
	struct timespec64 cur_time;
	int batt_uV, batt_ocV, batt_uA, fuel_cap, raw_cap, ret;
	int period_time, flush_time, board_temp = 0;
	int chg_cur = 0, chg_limit_cur = 0, input_cur = -EINVAL;
	int chg_vol = 0, vbat_avg = 0, ibat_avg = 0, recharge_uv = 0;
	int work_cycle = CM_CAP_CYCLE_TRACK_TIME_15S;
	int ui_soc = 0;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_vbat_avg_uV(cm, &vbat_avg);
	if (ret)
		dev_err(cm->dev, "get_vbat_avg_uV error.\n");

	ret = get_batt_ocv(cm, &batt_ocV);
	if (ret) {
		dev_err(cm->dev, "get_batt_ocV error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "get batt_uA error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_ibat_avg_uA(cm, &ibat_avg);
	if (ret)
		dev_err(cm->dev, "get ibat_avg_uA error.\n");

	ret = get_batt_cap(cm, &fuel_cap);
	if (ret) {
		dev_err(cm->dev, "get fuel_cap error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_batt_raw_cap(cm, &raw_cap);
	if (ret) {
		dev_err(cm->dev, "get raw_cap error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_constant_charge_current(cm, &chg_cur);
	if (ret)
		dev_warn(cm->dev, "get constant charge error.\n");

	ret = get_input_current_limit(cm, &chg_limit_cur);
	if (ret)
		dev_dbg(cm->dev, "get chg_limit_cur error.\n");

	if (desc->cp_sm.running) {
		ret = get_cp_ibus_uA(cm, &input_cur);
		if (ret)
			dev_warn(cm->dev, "cant not get input_cur.\n");
	}

	ret = get_charger_voltage(cm, &chg_vol);
	if (ret)
		dev_warn(cm->dev, "get chg_vol error.\n");

	ret = cm_get_battery_temperature(cm, &desc->temperature);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		goto schedule_cap_update_work;
	}

	ret = cm_get_board_temperature(cm, &board_temp);
	if (ret)
		dev_warn(cm->dev, "failed to get board temperature\n");

	fuel_cap = clamp(fuel_cap, 0, CM_CAP_FULL_PERCENT);
	cur_time = ktime_to_timespec64(ktime_get_boottime());

	if (is_full_charged(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_FULL;
	else if (is_charging(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
	else if (is_ext_pwr_online(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;

	/*
	 * Record the charging time when battery
	 * capacity is larger than 99%.
	 */
	if (cm->battery_status == POWER_SUPPLY_STATUS_CHARGING) {
		if (desc->cap >= 985) {
			desc->trickle_time = cur_time.tv_sec - desc->trickle_start_time;
		} else {
			desc->trickle_start_time = cur_time.tv_sec;
			desc->trickle_time = 0;
		}
	} else {
		desc->trickle_start_time = cur_time.tv_sec;
		desc->trickle_time = desc->trickle_time_out + desc->cap_one_time;
	}

	flush_time = cur_time.tv_sec - desc->update_capacity_time;
	period_time = cur_time.tv_sec - desc->last_query_time;
	desc->last_query_time = cur_time.tv_sec;

	if (desc->force_set_full && is_ext_pwr_online(cm))
		desc->charger_status = POWER_SUPPLY_STATUS_FULL;
	else
		desc->charger_status = cm->battery_status;

	dev_info(cm->dev, "vbat: %d, vbat_avg: %d, OCV: %d, ibat: %d, ibat_avg: %d, ibus: %d,"
		 " vbus: %d, msoc: %d, raw_soc: %d, chg_sts: %d, frce_full: %d, chg_lmt_cur: %d,"
		 " inpt_lmt_cur: %d, chgr_type: %d, Tboard: %d, Tbatt: %d, thm_cur: %d,"
		 " thm_pwr: %d, is_fchg: %d, fchg_en: %d, tflush: %d, tperiod: %d\n",
		 batt_uV, vbat_avg, batt_ocV, batt_uA, ibat_avg, input_cur, chg_vol, fuel_cap, raw_cap,
		 desc->charger_status, desc->force_set_full, chg_cur, chg_limit_cur,
		 desc->charger_type, board_temp, desc->temperature,
		 desc->thm_info.thm_adjust_cur, desc->thm_info.thm_pwr,
		 desc->is_fast_charge, desc->enable_fast_charge, flush_time, period_time);

	switch (desc->charger_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (fuel_cap < desc->cap) {
			if (batt_uA >= 0) {
				fuel_cap = desc->cap;
			} else {
				if (period_time < desc->cap_one_time) {
					/*
					 * The percentage of electricity is not
					 * allowed to change by 1% in desc->cap_one_time.
					 */
					if ((desc->cap - fuel_cap) >= 5)
						fuel_cap = desc->cap - 5;
					if (flush_time < desc->cap_one_time &&
					    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
					    DIV_ROUND_CLOSEST(desc->cap, 10))
						fuel_cap = desc->cap;
				} else {
					/*
					 * If wake up from long sleep mode,
					 * will make a percentage compensation based on time.
					 */
					if ((desc->cap - fuel_cap) >=
					    (period_time / desc->cap_one_time) * 10)
						fuel_cap = desc->cap -
							  (period_time / desc->cap_one_time) * 10;
				}
			}
		} else if (fuel_cap > desc->cap) {
			if (period_time < desc->cap_one_time) {
				if ((fuel_cap - desc->cap) >= 5)
					fuel_cap = desc->cap + 5;
				if (flush_time < desc->cap_one_time &&
				    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
				    DIV_ROUND_CLOSEST(desc->cap, 10))
					fuel_cap = desc->cap;
			} else {
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((fuel_cap - desc->cap) >=
				    (period_time / desc->cap_one_time) * 10)
					fuel_cap = desc->cap +
						  (period_time / desc->cap_one_time) * 10;
			}
		}

		if (desc->cap >= 985 && desc->cap <= 994 &&
		    fuel_cap >= CM_CAP_FULL_PERCENT)
			fuel_cap = 994;
		/*
		 * Record 99% of the charging time.
		 * if it is greater than 1500s,
		 * it will be mandatory to display 100%,
		 * but the background is still charging.
		 */
		if (cm->erp_config)
		{
			if ((fuel_cap > 994) && (!is_full_charged(cm)))
				fuel_cap = 994;

		} else {
			if (desc->cap >= 985 &&
			    desc->trickle_time >= desc->trickle_time_out &&
			    desc->trickle_time_out > 0 && batt_uA > 0)
				desc->force_set_full = true;
		}
		break;

	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		/*
		 * In not charging status,
		 * the cap is not allowed to increase.
		 */
		if (fuel_cap >= desc->cap) {
			fuel_cap = desc->cap;
		} else {
			if (period_time < desc->cap_one_time) {
				if ((desc->cap - fuel_cap) >= 5)
					fuel_cap = desc->cap - 5;
				if (flush_time < desc->cap_one_time &&
				    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
				    DIV_ROUND_CLOSEST(desc->cap, 10))
					fuel_cap = desc->cap;
			} else {
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((desc->cap - fuel_cap) >=
				    (period_time / desc->cap_one_time) * 10)
					fuel_cap = desc->cap -
						  (period_time / desc->cap_one_time) * 10;
			}
		}
		break;

	case POWER_SUPPLY_STATUS_FULL:
		desc->update_capacity_time = cur_time.tv_sec;
		recharge_uv = desc->fullbatt_uV - desc->fullbatt_vchkdrop_uV - 50000;
		if (cm->erp_config) {
			if (cm->erp_full_flag)
				cm_primary_charger_enable(cm, false);
			cm_get_uisoc(cm, &ui_soc);

			if (ui_soc < 95) {
				cm->erp_full_flag = 0;
				cm_primary_charger_enable(cm, true);
				desc->force_set_full = false;
				dev_info(cm->dev, "force_set_full:%d, ui_soc = %d\n", desc->force_set_full, ui_soc);
			}
		} else {
			if ((batt_ocV < recharge_uv) && (batt_uA < 0)) {
				desc->force_set_full = false;
				dev_info(cm->dev, "recharge_uv = %d\n", recharge_uv);
			}
		}

		if (is_ext_pwr_online(cm)) {
			if (!cm->erp_config) {
				if (fuel_cap != CM_CAP_FULL_PERCENT)
					fuel_cap = CM_CAP_FULL_PERCENT;
			}

			if (fuel_cap > desc->cap) {
				if (desc->cap < 900)
					fuel_cap = desc->cap + 10;
				else if (desc->cap < 960)
					fuel_cap = desc->cap + 5;
				else if (desc->cap < 990)
					fuel_cap = desc->cap + 3;
				else
					fuel_cap = desc->cap + 1;
			}
		} else {
			fuel_cap = desc->cap;
		}

		break;
	default:
		break;
	}

	work_cycle = cm_calc_batt_works_cycle(cm, fuel_cap);

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD) {
		dev_info(cm->dev, "batt_uV is less than UVLO calib volt\n");
		queue_delayed_work(system_unbound_wq, &cm->uvlo_work, msecs_to_jiffies(100));
	}

	dev_info(cm->dev, "new_uisoc = %d, old_uisoc = %d, work_cycle = %ds, cap_one_time = %ds\n",
		 fuel_cap, desc->cap, work_cycle, desc->cap_one_time);

	if (fuel_cap != desc->cap) {
		if (DIV_ROUND_CLOSEST(fuel_cap, 10) != DIV_ROUND_CLOSEST(desc->cap, 10)) {
			if (cm->erp_config) {
				if (fuel_cap > 994 && cm->erp_full_flag == 0) {
					desc->cap = 994;
				} else {
					desc->cap = fuel_cap;
				}
			} else {
				desc->cap = fuel_cap;
			}
			desc->update_capacity_time = cur_time.tv_sec;
			power_supply_changed(cm->charger_psy);
		}

		if (cm->erp_config) {
			if (fuel_cap > 994 && cm->erp_full_flag == 0) {
				desc->cap = 994;
			} else {
				desc->cap = fuel_cap;
			}
		} else {
			desc->cap = fuel_cap;
		}

		if (desc->uvlo_trigger_cnt < CM_UVLO_CALIBRATION_CNT_THRESHOLD)
			set_batt_cap(cm, desc->cap);
	}
	desc->raw_cap = raw_cap;

schedule_cap_update_work:
	queue_delayed_work(system_power_efficient_wq,
			   &cm->cap_update_work,
			   work_cycle * HZ);
}

static int cm_check_alt_cp_psy_ready_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	int i;
	static bool is_first_probe = true;
	static int check_alt_cp_count;
	static u64 alt_cp_probe_time;
	u64 cur_time = 0;

	if (!desc->psy_cp_stat) {
		dev_err(cm->dev, "%s, preferred cp is undefined, cp not exit\n", __func__);
		return 0;
	}

	if (is_first_probe) {
		is_first_probe = false;
		alt_cp_probe_time = ktime_to_ms(ktime_get_boottime());
	}

	psy = power_supply_get_by_name(desc->psy_cp_stat[0]);
	if (psy) {
		dev_info(cm->dev, "%s, find preferred cp \"%s\", count: %d\n",
			 __func__, desc->psy_cp_stat[0], check_alt_cp_count);
		goto done;
	}

	if (!desc->psy_alt_cp_adpt_stat || !desc->alt_cp_nums) {
		cur_time = ktime_to_ms(ktime_get_boottime());
		if (cur_time - alt_cp_probe_time > CM_CHECK_ALT_CP_PSY_TH_MS) {
			dev_err(cm->dev, "%s, alt cp is undefined, cp not exit, count: %d\n",
				__func__, check_alt_cp_count);
			desc->psy_cp_stat = NULL;
			return 0;
		}

		check_alt_cp_count++;
		return -EPROBE_DEFER;
	}

	for (i = 0; desc->psy_alt_cp_adpt_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_alt_cp_adpt_stat[i]);
		if (!psy) {
			dev_warn(cm->dev, "%s, cannot find alt cp \"%s\"\n",
				 __func__, desc->psy_alt_cp_adpt_stat[i]);
		} else {
			dev_info(cm->dev, "%s, find alt cp \"%s\"\n",
				 __func__, desc->psy_alt_cp_adpt_stat[i]);
			desc->psy_cp_stat[0] = desc->psy_alt_cp_adpt_stat[i];
			goto done;
		}
	}

	if (i == desc->alt_cp_nums) {
		dev_err(cm->dev, "%s, cannot find all cp\n", __func__);
		return -EPROBE_DEFER;
	}

done:
	if (psy)
		power_supply_put(psy);
	return 0;
}

static int get_boot_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "sprdboot.mode=cali") ||
	    strstr(cmd_line, "sprdboot.mode=autotest"))
		allow_charger_enable = true;
	else if (strstr(cmd_line, "sprdboot.mode=charger"))
		is_charger_mode =  true;

	return 0;
}

static int cm_check_alt_charger_psy_ready_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	int i;

	if (!desc->psy_charger_stat || !desc->psy_alt_charger_adpt_stat) {
		dev_err(cm->dev, "%s, chargeIC not exit\n", __func__);
		return 0;
	}

	psy = power_supply_get_by_name(desc->psy_charger_stat[0]);
	if (psy) {
		dev_info(cm->dev, "%s, find preferred chargeIC \"%s\"\n",
			 __func__, desc->psy_charger_stat[0]);
		goto done;
	}

	for (i = 0; desc->psy_alt_charger_adpt_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_alt_charger_adpt_stat[i]);
		if (!psy) {
			dev_warn(cm->dev, "%s, cannot find alt chargeIC \"%s\"\n",
				 __func__, desc->psy_alt_charger_adpt_stat[i]);
		} else {
			dev_info(cm->dev, "%s, find alt chargeIC \"%s\"\n",
				 __func__, desc->psy_alt_charger_adpt_stat[i]);
			desc->psy_charger_stat[0] = desc->psy_alt_charger_adpt_stat[i];
			goto done;
		}
	}

	if (i == desc->alt_charger_nums) {
		dev_err(cm->dev, "%s, cannot find all chargeIC\n", __func__);
		return -EPROBE_DEFER;
	}

done:
	power_supply_put(psy);
	return 0;
}

static int cm_check_charger_psy_ready_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	int i;

	/* Check if charger's supplies are present at probe */
	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			return -EPROBE_DEFER;
		}
		power_supply_put(psy);
	}

	return 0;
}

static void charger_manager_check_dev(struct charger_manager *cm)
{
	cm->charger = charger_find_dev_by_name("master_chg");
	if (!cm->charger)
		dev_err(cm->dev, "failed to master_charge device\n");
}

static void shipmode_syscore_shutdown(void)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(g_cm)) {
		pr_err("g_cm is err or null\n");
		return;
	}

	pr_info("shipmode syscore shutdown flag:%d\n", g_cm->shipmode_flag);

	if (g_cm->shipmode_flag) {
		g_cm->shipmode_flag = false;
		if (g_cm->charger) {
			ret = charger_set_shipmode(g_cm->charger, true);
			if (ret < 0)
				pr_err("set ship mode fail\n");
			else
				pr_err("set ship mode success\n");
		}
	}
}
static struct syscore_ops shipmode_syscore_ops = {
	.shutdown = shipmode_syscore_shutdown,
};
static int cm_get_shutdown_delay(struct charger_manager *cm, int capacity)
{
	static int count = 0;
	int usb_online;
	int vbat_now;
	int ret;
	char sd_str[32];
	char *envp[] = { sd_str, NULL };
	static bool last_shutdown_delay;

	usb_online = is_ext_pwr_online(cm);
	ret = get_vbat_now_uV(cm, &vbat_now);

	if (capacity <= 1) {
		if (vbat_now <= SHUTDOWN_DELAY_VOL_MAX && vbat_now >= SHUTDOWN_DELAY_VOL_MIN) {
			if (!usb_online)
				cm->shutdown_delay = true;
			else if (cm->shutdown_delay)
				cm->shutdown_delay = false;
		} else {
			cm->shutdown_delay = false;
		}
	}

	if (cm->shutdown_delay != last_shutdown_delay){
		sprintf(envp[0], "POWER_SUPPLY_SHUTDOWN_DELAY=%d", cm->shutdown_delay);
		kobject_uevent_env(&cm->charger_psy->dev.kobj, KOBJ_CHANGE, envp);
		dev_info(cm->dev, "%s\n", envp[0]);
		last_shutdown_delay = cm->shutdown_delay;
	}

	if (cm->shutdown_delay && count <= 60) {
		count ++;
		sprintf(envp[0], "POWER_SUPPLY_SHUTDOWN_DELAY=%d", cm->shutdown_delay);
		kobject_uevent_env(&cm->charger_psy->dev.kobj, KOBJ_CHANGE, envp);
		dev_info(cm->dev, "%s 2\n", envp[0]);
	}

	if (!cm->shutdown_delay){
		count = 0;
	}

	dev_info(cm->dev, "capacity(%d), vbat(%d), online(%d), shutdown_delay(%d), count(%d)\n",
		capacity, vbat_now, usb_online, cm->shutdown_delay, count);

	return 0;
}

static void cm_shutdown_delay_work(struct work_struct *work)
{
	int capacity;
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
			struct charger_manager, shutdown_delay_work);

	cm_get_uisoc(cm, &capacity);
	if (capacity <= 1)
		cm_get_shutdown_delay(cm, capacity);

	if (capacity < 5)
		schedule_delayed_work(&cm->shutdown_delay_work, msecs_to_jiffies(1000));
	else
		schedule_delayed_work(&cm->shutdown_delay_work, msecs_to_jiffies(5000));
}

static ssize_t soh_sn_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const char *soh_sn;

	soh_sn = lc_get_battery_sn();
	if (soh_sn) {
		dev_err(dev, "%s get sn succ\n", __func__);
		return sprintf(buf, "%s\n", soh_sn);
	} else {
		dev_err(dev, "%s get sn fail\n", __func__);
		return -EINVAL;
	}
}
static DEVICE_ATTR_RO(soh_sn);

static ssize_t manufacturing_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const char *manufacturing_date;

	manufacturing_date = lc_get_battery_manufacture_date();
	if (manufacturing_date) {
		dev_err(dev, "%s get battery_manufacture_date succ\n", __func__);
		return sprintf(buf, "%s\n", manufacturing_date);
	} else {
		dev_err(dev, "%s get battery_manufacture_date fail\n", __func__);
		manufacturing_date = "00000000";
		return sprintf(buf, "%s\n", manufacturing_date);
	}
}
static DEVICE_ATTR_RO(manufacturing_date);

static ssize_t first_usage_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const char *first_usage_time;
	char first_usage_date[9] = {'0','0','0','0','0','0','0','0','\0'};

	if (CHIP_UNSUPRT == ll_check_secret_chip_online()) {
		dev_info(dev, "force first usage date to 00000000\n");
		return sprintf(buf, "99999999\n");
	} else  {
		first_usage_time = lc_get_battery_first_use_time();
		if (!first_usage_time) {
			memcpy(first_usage_date, "99999999", 8);
			dev_err(dev, "%s get first_usage_date error\n", __func__);
			return sprintf(buf, "%s\n", first_usage_date);
		}
		memcpy(&first_usage_date[2], first_usage_time, 6);
		if (strncmp(&first_usage_date[2], "000000", 6)) //read date != 00000000, show date
			first_usage_date[0] = '2';

		return sprintf(buf, "%s\n", first_usage_date);
	}
}

static ssize_t first_usage_date_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	char first_usage_date[7] = {0};

	dev_err(dev, "first_usage_date:%s\n", buf);
	memcpy(first_usage_date, &buf[2], 6);
	first_usage_date[6] = '\0';
	ret = lc_set_battery_first_use_time(first_usage_date);

	return count;
}
static DEVICE_ATTR_RW(first_usage_date);

static ssize_t reset_cycle_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	const char *key = "clrcls";

	ret = strncmp(key, buf, 6);
	if (ret == 0) {
		ret = lc_clear_cycle_count();
			if (ret == 0) {
				dev_err(dev, "reset_cycle succ\n");
				return count;
			} else {
				dev_err(dev, "reset_cycle fail\n");
				return -EINVAL;
			}
	} else {
		dev_err(dev, "reset_cycle_key error=%d\n", ret);
		return -EINVAL;
	}
}
static DEVICE_ATTR_WO(reset_cycle);

static ssize_t soh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	union power_supply_propval pval;
	static struct power_supply *fg_psy = NULL;

	if (!fg_psy)
		fg_psy = power_supply_get_by_name("sc27xx-fgu");
	if (!fg_psy) {
		dev_err(dev, "get sc27xx-fgu psy failed\n");
		return -EINVAL;
	}
	if (power_supply_get_property(fg_psy, POWER_SUPPLY_PROP_SOH, &pval)) {
		dev_err(dev, "get sc27xx-fgu soh failed\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d\n", pval.intval);
}
static DEVICE_ATTR_RO(soh);

static ssize_t bms_authentic_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int is_auth;

	if (CHIP_UNSUPRT == ll_check_secret_chip_online()) {
		is_auth = 0;
		dev_info(dev, "force bms authentic true\n");
	} else {
		is_auth = lc_is_battery_auth_success();
		if (is_auth)
			dev_err(dev, "%s get battery_auth succ\n", __func__);
		else
			dev_err(dev, "%s get battery_auth fail\n", __func__);
	}

	return sprintf(buf, "%d\n", is_auth);
}
static struct device_attribute dev_attr_bms_authentic =
	__ATTR(authentic, 0444, bms_authentic_show, NULL);

static ssize_t ui_soh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u8  ui_soh_data[16] = {0};

	ret = lc_get_uisoh(ui_soh_data, UISOH_LEN);
	ret = snprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d %d %d %d %d %d \n", ui_soh_data[0],
		ui_soh_data[1], ui_soh_data[2], ui_soh_data[3], ui_soh_data[4], ui_soh_data[5],
		ui_soh_data[6], ui_soh_data[7], ui_soh_data[8], ui_soh_data[9], ui_soh_data[10]);
	dev_err(dev, "%s, latest_ui_soh = %d \n", __func__, ui_soh_data[0]);

	return ret;
}

static ssize_t ui_soh_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char t_data[70] = {0};
	char *pchar = NULL, *qchar = NULL;
	u8 ui_soh_data[40] = {0,};
	int ret = 0, i = 0;
	u8 val = 0;

	memset(t_data, 0, sizeof(t_data));
	strncpy(t_data, buf, count);
	dev_err(dev, "%s t_data: %s\n", __func__, t_data);
	qchar = t_data;

	while ((pchar = strsep(&qchar, " "))) {
		ret = kstrtou8(pchar, 10, &val);
		if (ret < 0) {
			dev_err(dev, "kstrtou8 error return %d \n", ret);
			return count;
		}
		ui_soh_data[i] = val;
		val = 0;
		dev_err(dev, "%s ui_soh_data[%d]: %d \n", __func__ ,i, ui_soh_data[i]);
		i++;
	}

	ret = lc_set_uisoh(ui_soh_data, UISOH_LEN);
	//desc->gm->ui_soh = ui_soh_data[0];
	dev_err(dev, "%s: ui_soh = %d\n", __func__, ui_soh_data[0]);

	return count;
}
static DEVICE_ATTR_RW(ui_soh);

static enum power_supply_property bms_psy_properties[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_TEMP,
};

static int psy_bms_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = cm->board_temp;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int psy_bms_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
  	struct charger_manager *cm = power_supply_get_drvdata(psy);
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

  	switch (psp) {
  	case POWER_SUPPLY_PROP_TEMP:
		dev_info(cm->dev, "%s:line%d board temp = %d\n",
				__func__, __LINE__, val->intval);
  		cm->board_temp = val->intval;
  		break;
  	default:
  		return -EINVAL;
  	}
  	return 0;
}

static struct power_supply_desc bms_psy_desc = {
	.name = "bms",
	.properties = bms_psy_properties,
	.num_properties = ARRAY_SIZE(bms_psy_properties),
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property = psy_bms_get_property,
	.set_property = psy_bms_set_property,
};

static void init_psy_bms(struct charger_manager *cm)
{
	struct power_supply_config cfg = {
		.drv_data = cm,
	};

	cm->bms_psy = power_supply_register(cm->dev, &bms_psy_desc, &cfg);
	if (IS_ERR(cm->bms_psy)) {
		dev_err(cm->dev, "%s register bms psy fail\n", __func__);
		return;
	}
	device_create_file(&cm->bms_psy->dev, &dev_attr_soh);
	device_create_file(&cm->bms_psy->dev, &dev_attr_bms_authentic);
	device_create_file(cm->bms_psy->dev.parent, &dev_attr_ui_soh);
}

static int charger_manager_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct charger_desc *desc = cm_get_drv_data(pdev);
	struct charger_manager *cm;
	int ret, i = 0;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	enum power_supply_property *properties;
	size_t num_properties;
	struct power_supply_config psy_cfg = {};
	struct timespec64 cur_time;
	char result[32] = {0};
	int id = 0;

	if (IS_ERR(desc)) {
		dev_err(&pdev->dev, "No platform data (desc) found\n");
		return PTR_ERR(desc);
	}

	cm = devm_kzalloc(&pdev->dev, sizeof(*cm), GFP_KERNEL);
	if (!cm)
		return -ENOMEM;

	/* Basic Values. Unspecified are Null or 0 */
	cm->dev = &pdev->dev;
	cm->desc = desc;
	psy_cfg.drv_data = cm;

	cm->battery_chan = devm_iio_channel_get(cm->dev, "batt-id");
	if (IS_ERR_OR_NULL(cm->battery_chan)) {
		dev_err(cm->dev, "failed to get battery IIO channel, ret = %ld\n",
			PTR_ERR(cm->battery_chan));
		return PTR_ERR(cm->battery_chan);
	}

	//r_items_param is point to r_items_param_dual default
	ret = sprd_battery_parse_cmdline_match_by_split("bat.id=", " ", result, sizeof(result));
	if (!ret && !(ret = kstrtoint(result, 10, &id))) {
		if (id == -1) {
			cm->erp_config = 1;
			cm->erp_full_flag = 0;
			r_items_param = r_items_param_secret;
			r_item_len = ARRAY_SIZE(r_items_param_secret);
			ret = sprd_battery_parse_cmdline_match_by_split("batt_id:", ";", result, sizeof(result));
			if (!ret && !(ret = kstrtoint(result, 10, &id)) && (id == 1 || id == 2))
				cm->bat_id = id;
			else
				cm->bat_id = 0;
			cm->bat_id_adc_mv = 0;
		} else if (id >= 3 && id <= 4) {
			r_items_param = r_items_param_single;
			r_item_len = ARRAY_SIZE(r_items_param_single);
		}
	}
	for(i = 0; i < 3; i++) {
		cm_get_bat_id(cm);
		if(cm->bat_id)
			break;
	}

	/* Initialize alarm timer */
	if (alarmtimer_get_rtcdev()) {
		cm_timer = devm_kzalloc(cm->dev, sizeof(*cm_timer), GFP_KERNEL);
		if (!cm_timer)
			return -ENOMEM;
		alarm_init(cm_timer, ALARM_BOOTTIME, NULL);
	}

	charger_manager_check_dev(cm);
	cm->vchg_info = sprd_vchg_info_register(cm->dev);
	if (IS_ERR(cm->vchg_info)) {
		dev_info(&pdev->dev, "Fail to init vchg info\n");
		return -ENOMEM;
	}

	if (cm->vchg_info->ops && cm->vchg_info->ops->parse_dts &&
	    cm->vchg_info->ops->parse_dts(cm->vchg_info)) {
		dev_err(&pdev->dev, "failed to parse sprd vchg parameters\n");
		return -EPROBE_DEFER;
	}

	cm->fchg_info = sprd_fchg_info_register(cm->dev);
	if (IS_ERR(cm->fchg_info)) {
		dev_err(&pdev->dev, "Fail to register fchg info\n");
		return -ENOMEM;
	}

	/*
	 * Some of the following do not need to be errors.
	 * Users may intentionally ignore those features.
	 */

	if (!desc->fullbatt_vchkdrop_ms || !desc->fullbatt_vchkdrop_uV) {
		dev_info(&pdev->dev, "Disabling full-battery voltage drop checking mechanism as it is not supplied\n");
		desc->fullbatt_vchkdrop_ms = 0;
		desc->fullbatt_vchkdrop_uV = 0;
	}
	if (desc->fullbatt_soc == 0)
		dev_info(&pdev->dev, "Ignoring full-battery soc(state of charge) threshold as it is not supplied\n");

	if (desc->fullbatt_full_capacity == 0)
		dev_info(&pdev->dev, "Ignoring full-battery full capacity threshold as it is not supplied\n");

	if (!desc->psy_charger_stat || !desc->psy_charger_stat[0]) {
		dev_err(&pdev->dev, "No power supply defined\n");
		return -EINVAL;
	}

	if (!desc->psy_fuel_gauge) {
		dev_err(&pdev->dev, "No fuel gauge power supply defined\n");
		return -EINVAL;
	}

	ret = get_boot_mode();
	if (ret) {
		pr_err("boot_mode can't not parse bootargs property\n");
		return ret;
	}

	if (desc->enable_alt_charger_adapt && desc->alt_charger_nums > 0) {
		ret = cm_check_alt_charger_psy_ready_status(cm);
		if (ret < 0) {
			dev_err(&pdev->dev, "can't find chargeIC\n");
			return ret;
		}
	} else {
		ret = cm_check_charger_psy_ready_status(cm);
		if (ret < 0) {
			dev_err(&pdev->dev, "can't find chargeIC\n");
			return ret;
		}
	}

	/*
	 * CP single software and multiple hardware scheme.
	 * Currently, only the single CP hardware scheme is
	 * supported.
	 */
	if (desc->enable_alt_cp_adapt && desc->cp_nums == 1) {
		ret = cm_check_alt_cp_psy_ready_status(cm);
		if (ret < 0) {
			dev_err(&pdev->dev, "can't find cp\n");
			return ret;
		}
	}

	if (cm->desc->polling_mode != CM_POLL_DISABLE &&
	    (desc->polling_interval_ms == 0 ||
	     msecs_to_jiffies(desc->polling_interval_ms) <= CM_JIFFIES_SMALL)) {
		dev_err(&pdev->dev, "polling_interval_ms is too small\n");
		return -EINVAL;
	}

	if (!desc->charging_max_duration_ms ||
			!desc->discharging_max_duration_ms) {
		dev_info(&pdev->dev, "Cannot limit charging duration checking mechanism to prevent overcharge/overheat and control discharging duration\n");
		desc->charging_max_duration_ms = 0;
		desc->discharging_max_duration_ms = 0;
	}

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop) {
		dev_info(&pdev->dev, "Cannot validate charge voltage\n");
		desc->charge_voltage_max = 0;
		desc->charge_voltage_drop = 0;
	}

	platform_set_drvdata(pdev, cm);

	memcpy(&cm->charger_psy_desc, &psy_default, sizeof(psy_default));

	if (!desc->psy_name)
		strncpy(cm->psy_name_buf, psy_default.name, PSY_NAME_MAX);
	else
		strncpy(cm->psy_name_buf, desc->psy_name, PSY_NAME_MAX);
	cm->charger_psy_desc.name = cm->psy_name_buf;

	/* Allocate for psy properties because they may vary */
	properties = devm_kcalloc(&pdev->dev,
			     ARRAY_SIZE(default_charger_props) +
				NUM_CHARGER_PSY_OPTIONAL,
			     sizeof(*properties), GFP_KERNEL);
	if (!properties)
		return -ENOMEM;

	memcpy(properties, default_charger_props,
		sizeof(enum power_supply_property) *
		ARRAY_SIZE(default_charger_props));
	num_properties = ARRAY_SIZE(default_charger_props);

	/* Find which optional psy-properties are available */
	fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fuel_gauge);
		return -EPROBE_DEFER;
	}

	if (!power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CHARGE_NOW, &val)) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_CHARGE_NOW, properties, &num_properties))
			dev_warn(&pdev->dev, "POWER_SUPPLY_PROP_CHARGE_NOW is present\n");
	}

	val.intval = CM_IBAT_CURRENT_NOW_CMD;
	if (!power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW, &val)) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_CURRENT_NOW, properties, &num_properties))
			dev_warn(&pdev->dev, "POWER_SUPPLY_PROP_CURRENT_NOW is present\n");
	}

	ret = get_boot_cap(cm, &cm->desc->cap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get initial battery capacity\n");
		return ret;
	}
	if (device_property_read_bool(&pdev->dev, "cm-keep-awake"))
		cm->desc->keep_awake = true;
	cm->desc->thm_info.thm_adjust_cur = -EINVAL;
	cm->desc->thermal_limit_ibat = 0;
	cm->desc->thermal_limit_level = 0;
	cm->desc->ir_comp.ibat_buf[CM_IBAT_BUFF_CNT - 1] = CM_MAGIC_NUM;
	cm->desc->ir_comp.us_lower_limit = cm->desc->ir_comp.us;

	if (device_property_read_bool(&pdev->dev, "cm-support-linear-charge"))
		cm->desc->thm_info.need_calib_charge_lmt = true;

	ret = cm_get_battery_temperature(cm, &cm->desc->temperature);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		return ret;
	}

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cm->desc->update_capacity_time = cur_time.tv_sec;
	cm->desc->last_query_time = cur_time.tv_sec;

	ret = cm_init_thermal_data(cm, fuel_gauge, properties, &num_properties);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize thermal data\n");
		cm->desc->measure_battery_temp = false;
	}
	power_supply_put(fuel_gauge);

	cm->charger_psy_desc.properties = properties;
	cm->charger_psy_desc.num_properties = num_properties;

	INIT_DELAYED_WORK(&cm->fullbatt_vchk_work, fullbatt_vchk);
	INIT_DELAYED_WORK(&cm->cap_update_work, cm_batt_works);
	INIT_DELAYED_WORK(&cm->fixed_fchg_work, cm_fixed_fchg_work);
	INIT_DELAYED_WORK(&cm->cp_work, cm_cp_work);
	INIT_DELAYED_WORK(&cm->ir_compensation_work, cm_ir_compensation_works);
	INIT_DELAYED_WORK(&cm->charger_type_update_work, cm_charger_type_update_work);
	INIT_DELAYED_WORK(&cm->power_detect_work, charge_power_detect_work);
	INIT_DELAYED_WORK(&cm->adjust_dpm_work, charge_adjust_dpm_work);
	INIT_DELAYED_WORK(&cm->limit_current_work, cm_limit_current_work);
	INIT_DELAYED_WORK(&cm->dcd_work, charge_dcd_work);
	INIT_DELAYED_WORK(&cm->cid_detect_work, soft_cid_detect_work);
	INIT_DELAYED_WORK(&cm->shutdown_delay_work, cm_shutdown_delay_work);
	INIT_DELAYED_WORK(&cm->quick_charge_type_update_work, cm_quick_charge_type_update_work);

	mutex_init(&cm->desc->charge_info_mtx);

	/* Register sysfs entry for charger */
	ret = charger_manager_prepare_sysfs(cm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Cannot prepare sysfs entry of regulators\n");
		return ret;
	}

	if (!desc->sysfs || desc->num_sysfs < 1) {
		dev_err(&pdev->dev, "sysfs undefined\n");
		return -EINVAL;
	}

	psy_cfg.attr_grp = desc->sysfs_groups;
	psy_cfg.of_node = np;

	cm->charger_psy = devm_power_supply_register(&pdev->dev,
						&cm->charger_psy_desc,
						&psy_cfg);
	if (IS_ERR(cm->charger_psy)) {
		dev_err(&pdev->dev, "Cannot register charger-manager with name \"%s\"\n",
			cm->charger_psy_desc.name);
		return PTR_ERR(cm->charger_psy);
	}
	cm->charger_psy->supplied_to = charger_manager_supplied_to;
	cm->charger_psy->num_supplicants =
		ARRAY_SIZE(charger_manager_supplied_to);
	init_psy_bms(cm);
	device_create_file(&cm->charger_psy->dev, &dev_attr_soh_sn);
	device_create_file(&cm->charger_psy->dev, &dev_attr_manufacturing_date);
	device_create_file(&cm->charger_psy->dev, &dev_attr_first_usage_date);
	device_create_file(&cm->charger_psy->dev, &dev_attr_reset_cycle);

	wireless_main.cm = cm;
	wireless_main.psy = devm_power_supply_register(&pdev->dev, &wireless_main.psd, NULL);
	if (IS_ERR(wireless_main.psy)) {
		dev_err(&pdev->dev, "Cannot register wireless_main.psy with name \"%s\"\n",
			wireless_main.psd.name);
		return PTR_ERR(wireless_main.psy);
	}

	ac_main.cm = cm;
	ac_main.psy = devm_power_supply_register(&pdev->dev, &ac_main.psd, NULL);
	if (IS_ERR(ac_main.psy)) {
		dev_err(&pdev->dev, "Cannot register usb_main.psy with name \"%s\"\n",
			ac_main.psd.name);
		return PTR_ERR(ac_main.psy);
	}

	usb_main.cm = cm;
	usb_main.psy = devm_power_supply_register(&pdev->dev, &usb_main.psd, NULL);
	if (IS_ERR(usb_main.psy)) {
		dev_err(&pdev->dev, "Cannot register usb_main.psy with name \"%s\"\n",
			usb_main.psd.name);
		return PTR_ERR(usb_main.psy);
	}

	g_cm = cm;
	cm->input_suspend = 0;
	cm->mtbf_current = 0;
	cm->shutdown_delay = false;
	cm->fake_capacity = -EINVAL;
	cm->fake_charge_cycle = -EINVAL;
	cm->otg_debug = 0;

	cm_usb_sysfs_create_group(cm);
	cm_batt_sysfs_create_group(cm);

	mutex_init(&cm->desc->keep_awake_mtx);

	/*
	 * Charger-manager is capable of waking up the system from sleep
	 * when event is happened through cm_notify_event()
	 */
	device_init_wakeup(&pdev->dev, true);
	device_set_wakeup_capable(&pdev->dev, false);
	cm->charge_ws = wakeup_source_create("charger_manager_wakelock");
	wakeup_source_add(cm->charge_ws);
	cm->cp_ws = wakeup_source_create("charger_pump_wakelock");
	wakeup_source_add(cm->cp_ws);
	mutex_init(&cm->desc->charger_type_mtx);

	ret = cm_get_bat_info(cm, 0);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get battery information\n");
		goto err;
	}

	cm->cm_charge_vote = sprd_charge_vote_register("cm_charge_vote",
						       cm_sprd_vote_callback,
						       cm,
						       &cm->charger_psy->dev);
	if (IS_ERR(cm->cm_charge_vote)) {
		dev_err(&pdev->dev, "Failed to register charge vote\n");
		ret = PTR_ERR(cm->cm_charge_vote);
		goto err;
	}

	if(cm->bat_id) {
		if (cm->fchg_info->ops && cm->fchg_info->ops->extcon_init &&
		    cm->fchg_info->ops->extcon_init(cm->fchg_info, cm->charger_psy)) {
			dev_err(&pdev->dev, "Failed to initialize fchg extcon\n");
			ret = -EPROBE_DEFER;
			goto err;
		}
	}

	if (cm->vchg_info->ops && cm->vchg_info->ops->init &&
	    cm->vchg_info->ops->init(cm->vchg_info, cm->charger_psy)) {
		dev_err(&pdev->dev, "Failed to register vchg detect notify\n");
		ret = -EPROBE_DEFER;
		goto err;
	}

	/* Add to the list */
	mutex_lock(&cm_list_mtx);
	list_add(&cm->entry, &cm_list);
	mutex_unlock(&cm_list_mtx);

	g_cm = cm;
	desc->rp_limit_current = -EINVAL;
	if (is_ext_usb_pwr_online(cm) && cm->fchg_info->ops && cm->fchg_info->ops->fchg_detect)
		cm->fchg_info->ops->fchg_detect(cm->fchg_info);

	sprd_tcpm_charger_ops_register(&cm_sprd_charger_ops);
	if (cm_event_num > 0) {
		for (i = 0; i < cm_event_num; i++)
			cm_notify_type_handle(cm, cm_event_type[i], cm_event_msg[i]);
		cm_event_num = 0;
	}
	/*
	 * Charger-manager have to check the charging state right after
	 * initialization of charger-manager and then update current charging
	 * state.
	 */
	cm_monitor();

	schedule_work(&setup_polling);
	schedule_delayed_work(&cm->shutdown_delay_work, msecs_to_jiffies(2000));
	queue_delayed_work(system_power_efficient_wq, &cm->cap_update_work, CM_CAP_CYCLE_TRACK_TIME_15S * HZ);
	INIT_DELAYED_WORK(&cm->uvlo_work, cm_uvlo_check_work);
	register_syscore_ops(&shipmode_syscore_ops);

	charger_partition_init();

	dev_info(cm->dev, "%s:line%d probe successfully\n", __func__, __LINE__);

	return 0;

err:

	wakeup_source_remove(cm->charge_ws);

	return ret;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);

	charger_partition_exit();

	/* Remove from the list */
	mutex_lock(&cm_list_mtx);
	list_del(&cm->entry);
	mutex_unlock(&cm_list_mtx);

	if (cm->fchg_info->ops && cm->fchg_info->ops->remove)
		cm->fchg_info->ops->remove(cm->fchg_info);

	if (cm->vchg_info->ops && cm->vchg_info->ops->remove)
		cm->vchg_info->ops->remove(cm->vchg_info);

	cancel_work_sync(&setup_polling);
	cancel_delayed_work(&cm->power_detect_work);
	cancel_delayed_work(&cm->adjust_dpm_work);
	cancel_delayed_work_sync(&cm_monitor_work);
	cancel_delayed_work_sync(&cm->cap_update_work);
	cancel_delayed_work_sync(&cm->fullbatt_vchk_work);
	cancel_delayed_work_sync(&cm->uvlo_work);
	cancel_delayed_work_sync(&cm->dcd_work);
	cancel_delayed_work(&cm->cid_detect_work);
	cancel_delayed_work_sync(&cm->shutdown_delay_work);

	power_supply_unregister(cm->charger_psy);

	try_charger_enable(cm, false);

	return 0;
}

static void charger_manager_shutdown(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);

	cm->shutdown_flag = true;
	if (cm->desc->uvlo_trigger_cnt < CM_UVLO_CALIBRATION_CNT_THRESHOLD)
		set_batt_cap(cm, cm->desc->cap);

	if (cm->fchg_info->ops && cm->fchg_info->ops->shutdown)
		cm->fchg_info->ops->shutdown(cm->fchg_info);

	if (cm->vchg_info->ops && cm->vchg_info->ops->shutdown)
		cm->vchg_info->ops->shutdown(cm->vchg_info);

	cancel_delayed_work(&cm->power_detect_work);
	cancel_delayed_work(&cm->adjust_dpm_work);
	cancel_delayed_work_sync(&cm->dcd_work);
	cancel_delayed_work_sync(&cm_monitor_work);
	cancel_delayed_work_sync(&cm->fullbatt_vchk_work);
	cancel_delayed_work_sync(&cm->cap_update_work);
	cancel_delayed_work(&cm->uvlo_work);
	cancel_delayed_work_sync(&cm->ir_compensation_work);
	cancel_delayed_work(&cm->cid_detect_work);
}

static const struct platform_device_id charger_manager_id[] = {
	{ "charger-manager", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, charger_manager_id);

static int cm_suspend_noirq(struct device *dev)
{
	if (device_may_wakeup(dev)) {
		device_set_wakeup_capable(dev, false);
		return -EAGAIN;
	}

	return 0;
}

static int cm_suspend_prepare(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (!cm_suspended)
		cm_suspended = true;

	cm_timer_set = cm_setup_timer();

	if (cm_timer_set) {
		dev_dbg(cm->dev, "%s:line%d prepare\n", __func__, __LINE__);
		cancel_work_sync(&setup_polling);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work(&cm->fullbatt_vchk_work);
		cancel_delayed_work_sync(&cm->cap_update_work);
		cancel_delayed_work_sync(&cm->uvlo_work);
	}

	return 0;
}

static void cm_suspend_complete(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (cm_suspended)
		cm_suspended = false;

	if (cm_timer_set) {
		ktime_t remain;

		alarm_cancel(cm_timer);
		cm_timer_set = false;
		remain = alarm_expires_remaining(cm_timer);
		if (remain > 0)
			cm_suspend_duration_ms -= ktime_to_ms(remain);
		schedule_work(&setup_polling);
	}

	_cm_monitor(cm);
	cm_batt_works(&cm->cap_update_work.work);

	/* Re-enqueue delayed work (fullbatt_vchk_work) */
	if (cm->fullbatt_vchk_jiffies_at) {
		unsigned long delay = 0;
		unsigned long now = jiffies + CM_JIFFIES_SMALL;

		if (time_after_eq(now, cm->fullbatt_vchk_jiffies_at)) {
			delay = (unsigned long)((long)now
				- (long)(cm->fullbatt_vchk_jiffies_at));
			delay = jiffies_to_msecs(delay);
		} else {
			delay = 0;
		}

		/*
		 * Account for cm_suspend_duration_ms with assuming that
		 * timer stops in suspend.
		 */
		if (delay > cm_suspend_duration_ms)
			delay -= cm_suspend_duration_ms;
		else
			delay = 0;

		queue_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
				   msecs_to_jiffies(delay));
	}
	device_set_wakeup_capable(cm->dev, false);
	dev_dbg(cm->dev, "%s:line%d complete\n", __func__, __LINE__);
}

static const struct dev_pm_ops charger_manager_pm = {
	.prepare	= cm_suspend_prepare,
	.suspend_noirq	= cm_suspend_noirq,
	.complete	= cm_suspend_complete,
};

static struct platform_driver charger_manager_driver = {
	.driver = {
		.name = "charger-manager",
		.pm = &charger_manager_pm,
		.of_match_table = charger_manager_match,
	},
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.id_table = charger_manager_id,
};

static int __init charger_manager_init(void)
{
	cm_wq = create_freezable_workqueue("charger_manager");
	if (unlikely(!cm_wq))
		return -ENOMEM;

	INIT_DELAYED_WORK(&cm_monitor_work, cm_monitor_poller);

	return platform_driver_register(&charger_manager_driver);
}
late_initcall(charger_manager_init);

static void __exit charger_manager_cleanup(void)
{
	destroy_workqueue(cm_wq);
	cm_wq = NULL;

	platform_driver_unregister(&charger_manager_driver);
}
module_exit(charger_manager_cleanup);

/**
 * cm_notify_type_handle - charger driver handle charger event
 * @cm: the Charger Manager representing the battery
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg)
{
	if (cm->shutdown_flag)
		return;

	switch (type) {
	case CM_EVENT_BATT_FULL:
		fullbatt_handler(cm);
		break;
	case CM_EVENT_BATT_IN:
	case CM_EVENT_BATT_OUT:
		battout_handler(cm);
		break;
	case CM_EVENT_WL_CHG_START_STOP:
	case CM_EVENT_EXT_PWR_IN_OUT ... CM_EVENT_CHG_START_STOP:
		misc_event_handler(cm, type);
		break;
	case CM_EVENT_FAST_CHARGE:
		fast_charge_handler(cm);
		break;
	case CM_EVENT_INT:
		cm_charger_int_handler(cm);
		break;
	case CM_EVENT_BATT_OVERVOLTAGE:
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);
		break;
	case CM_EVENT_IGNORE_HARD_RESET:
		cm_enable_fixed_fchg_handshake(cm, false);
		break;
	case CM_EVENT_BATT_AGING:
		cm_batt_aging_algo(cm);
		break;
	case CM_EVENT_UPDATE_USB_LIMINT:
		if (!(cm->desc->limit_status & CM_CHARGE_USB_LIMIT_CMD))
			cancel_delayed_work_sync(&cm->limit_current_work);

		cm->desc->limit_status |= CM_CHARGE_USB_LIMIT_CMD;
		schedule_delayed_work(&cm->limit_current_work, 0);
		break;
	case CM_EVENT_UNKNOWN:
	case CM_EVENT_OTHERS:
	default:
		dev_err(cm->dev, "%s: type not specified\n", __func__);
		break;
	}

	power_supply_changed(cm->charger_psy);

}

/**
 * cm_notify_event - charger driver notify Charger Manager of charger event
 * @psy: pointer to instance of charger's power_supply
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
void cm_notify_event(struct power_supply *psy, enum cm_event_types type,
		     char *msg)
{
	struct charger_manager *cm;
	bool found_power_supply = false;

	if (psy == NULL)
		return;

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		if (cm->charger_psy->desc) {
			if (strcmp(psy->desc->name, cm->charger_psy->desc->name) == 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_charger_stat) {
			if (match_string(cm->desc->psy_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_fuel_gauge) {
			/*
			 * fgu has only one string and no null pointer at the end,
			 * only needs to compare once before exiting th loop, so 1 here and -1 elsewhere.
			 */
			if (match_string(&cm->desc->psy_fuel_gauge, 1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_cp_stat) {
			if (match_string(cm->desc->psy_cp_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_wl_charger_stat) {
			if (match_string(cm->desc->psy_wl_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}
	}

	mutex_unlock(&cm_list_mtx);

	if (!found_power_supply || !cm->cm_charge_vote) {
		if (cm_event_num < CM_EVENT_TYPE_NUM) {
			cm_event_msg[cm_event_num] = msg;
			cm_event_type[cm_event_num++] = type;
		} else {
			pr_err("%s: too many cm_event_num!!\n", __func__);
		}
		return;
	}

	cm_notify_type_handle(cm, type, msg);
}
EXPORT_SYMBOL_GPL(cm_notify_event);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("Charger Manager");
MODULE_LICENSE("GPL");
