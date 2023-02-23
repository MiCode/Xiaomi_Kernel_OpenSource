/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

/*
 *
 * Filename:
 * ---------
 *    mtk_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
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
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/of_device.h>
#include <linux/hwid.h>
#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/mtk_battery.h>
#include <mt-plat/mtk_boot.h>
#include <pmic.h>
#include <mtk_gauge_time_service.h>
#include <linux/ktime.h>

#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"
#include <tcpm.h>

extern void mp2762_direct_set_fcc(struct charger_device *chg_dev, int value);

enum product_name {
	UNKNOW,
	RUBY,
	RUBYPRO,
	RUBYPLUS,
};

//extern int get_charge_mode(void);
static int product_name = UNKNOW;
static struct charger_manager *pinfo;
static struct list_head consumer_head = LIST_HEAD_INIT(consumer_head);
static DEFINE_MUTEX(consumer_mutex);

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

extern int get_pd_usb_connected(void);
static void usbpd_mi_vdm_received_cb(struct tcp_ny_uvdm uvdm);

static bool check_usb_psy(void)
{
	if (!pinfo->usb_psy)
		pinfo->usb_psy = power_supply_get_by_name("usb");

	if (pinfo->usb_psy)
		return true;
	else
		return false;
}

static bool mtk_check_psy(struct charger_manager *info)
{
	if (!pinfo->usb_psy)
		pinfo->usb_psy = power_supply_get_by_name("usb");

	if (!pinfo->usb_psy) {
		chr_err("failed to get usb_psy\n");
		return false;
	}

	if (!pinfo->batt_psy)
		pinfo->batt_psy = power_supply_get_by_name("battery");

	if (!pinfo->batt_psy) {
		chr_err("failed to get batt_psy\n");
		return false;
	}

	if (!pinfo->bms_psy)
		pinfo->bms_psy = power_supply_get_by_name("bms");

	if (!pinfo->bms_psy) {
		chr_err("failed to get bms_psy\n");
		return false;
	}

	if (!pinfo->charger_psy)
		pinfo->charger_psy = power_supply_get_by_name("charger");

	if (!pinfo->charger_psy) {
		chr_err("failed to get charger_psy\n");
		return false;
	}

	if (product_name == RUBYPLUS){
		if (!pinfo->xmusb350_psy)
		      pinfo->xmusb350_psy = power_supply_get_by_name("xmusb350");

		if (!pinfo->xmusb350_psy) {
			chr_err("failed to get xmusb350_psy\n");
			return false;
		}
	}

	return true;
}

static bool mtk_check_votable(struct charger_manager *info)
{
	if (!info->bbc_icl_votable)
		info->bbc_icl_votable = find_votable("BBC_ICL");

	if (!info->bbc_icl_votable) {
		chr_err("failed to get bbc_icl_votable\n");
		return false;
	}

	if (!info->bbc_fcc_votable)
		info->bbc_fcc_votable = find_votable("BBC_FCC");

	if (!info->bbc_fcc_votable) {
		chr_err("failed to get bbc_fcc_votable\n");
		return false;
	}

	if (!info->bbc_fv_votable)
		info->bbc_fv_votable = find_votable("BBC_FV");

	if (!info->bbc_fv_votable) {
		chr_err("failed to get bbc_fv_votable\n");
		return false;
	}

	if (!info->bbc_vinmin_votable)
		info->bbc_vinmin_votable = find_votable("BBC_VINMIN");

	if (!info->bbc_vinmin_votable) {
		chr_err("failed to get bbc_vinmin_votable\n");
		return false;
	}

	if (!info->bbc_iterm_votable)
		info->bbc_iterm_votable = find_votable("BBC_ITERM");

	if (!info->bbc_iterm_votable) {
		chr_err("failed to get bbc_iterm_votable\n");
		return false;
	}

	if (!info->bbc_en_votable)
		info->bbc_en_votable = find_votable("BBC_ENABLE");

	if (!info->bbc_en_votable) {
		chr_err("failed to get bbc_en_votable\n");
		return false;
	}

	if (!info->bbc_suspend_votable)
		info->bbc_suspend_votable = find_votable("BBC_SUSPEND");

	if (!info->bbc_suspend_votable) {
		chr_err("failed to get bbc_suspend_votable\n");
		return false;
	}

	return true;
}

bool mtk_is_TA_support_pd_pps(struct charger_manager *pinfo)
{
	if (pinfo->enable_pe_4 == false && pinfo->enable_pe_5 == false)
		return false;

	if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
		return true;
	return false;
}

bool is_power_path_supported(void)
{
	if (pinfo == NULL)
		return false;

	if (pinfo->data.power_path_support == true)
		return true;

	return false;
}

bool is_disable_charger(void)
{
	if (pinfo == NULL)
		return true;

	if (pinfo->disable_charger == true || IS_ENABLED(CONFIG_POWER_EXT))
		return true;
	else
		return false;
}

void BATTERY_SetUSBState(int usb_state_value)
{
	if (is_disable_charger()) {
		chr_err("[%s] in FPGA/EVB, no service\n", __func__);
	} else {
		if ((usb_state_value < USB_SUSPEND) ||
			((usb_state_value > USB_CONFIGURED))) {
			chr_err("%s Fail! Restore to default value\n",
				__func__);
			usb_state_value = USB_UNCONFIGURED;
		} else {
			chr_err("%s Success! Set %d\n", __func__,
				usb_state_value);
			if (pinfo)
				pinfo->usb_state = usb_state_value;
		}
	}
}

unsigned int set_chr_input_current_limit(int current_limit)
{
	return 500;
}

int get_chr_temperature(int *min_temp, int *max_temp)
{
	*min_temp = 25;
	*max_temp = 30;

	return 0;
}

int set_chr_boost_current_limit(unsigned int current_limit)
{
	return 0;
}

int set_chr_enable_otg(unsigned int enable)
{
	return 0;
}

int mtk_chr_is_charger_exist(unsigned char *exist)
{
	if (mt_get_charger_type() == CHARGER_UNKNOWN)
		*exist = 0;
	else
		*exist = 1;
	return 0;
}

/*=============== fix me==================*/
int chargerlog_level = CHRLOG_ERROR_LEVEL;

int chr_get_debug_level(void)
{
	return chargerlog_level;
}

#ifdef MTK_CHARGER_EXP
#include <linux/string.h>

char chargerlog[1000];
#define LOG_LENGTH 500
int chargerlog_level = 10;
int chargerlogIdx;

int charger_get_debug_level(void)
{
	return chargerlog_level;
}

void charger_log(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsprintf(chargerlog + chargerlogIdx, fmt, args);
	va_end(args);
	chargerlogIdx = strlen(chargerlog);
	if (chargerlogIdx >= LOG_LENGTH) {
		chr_err("%s", chargerlog);
		chargerlogIdx = 0;
		memset(chargerlog, 0, 1000);
	}
}

void charger_log_flash(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsprintf(chargerlog + chargerlogIdx, fmt, args);
	va_end(args);
	chr_err("%s", chargerlog);
	chargerlogIdx = 0;
	memset(chargerlog, 0, 1000);
}
#endif

void _wake_up_charger(struct charger_manager *info)
{
	unsigned long flags;

	if (info == NULL)
		return;

	spin_lock_irqsave(&info->slock, flags);
	if (!info->charger_wakelock->active)
		__pm_stay_awake(info->charger_wakelock);
	spin_unlock_irqrestore(&info->slock, flags);
	info->charger_thread_timeout = true;
	wake_up_interruptible(&info->wait_que);
}

/* charger_manager ops  */
static int _mtk_charger_change_current_setting(struct charger_manager *info)
{
/*
	if (info != NULL && info->change_current_setting)
		return info->change_current_setting(info);
*/
	return 0;
}

static int _mtk_charger_do_charging(struct charger_manager *info, bool en)
{
	if (info != NULL && info->do_charging)
		info->do_charging(info, en);
	return 0;
}
/* charger_manager ops end */


/* user interface */
struct charger_consumer *charger_manager_get_by_name(struct device *dev,
	const char *name)
{
	struct charger_consumer *puser;

	puser = kzalloc(sizeof(struct charger_consumer), GFP_KERNEL);
	if (puser == NULL)
		return NULL;

	mutex_lock(&consumer_mutex);
	puser->dev = dev;

	list_add(&puser->list, &consumer_head);
	if (pinfo != NULL)
		puser->cm = pinfo;

	mutex_unlock(&consumer_mutex);

	return puser;
}
EXPORT_SYMBOL(charger_manager_get_by_name);

int charger_manager_enable_high_voltage_charging(
			struct charger_consumer *consumer, bool en)
{
	struct charger_manager *info = consumer->cm;
	struct list_head *pos = NULL;
	struct list_head *phead = &consumer_head;
	struct charger_consumer *ptr = NULL;

	if (!info)
		return -EINVAL;

	pr_debug("[%s] %s, %d\n", __func__, dev_name(consumer->dev), en);

	if (!en && consumer->hv_charging_disabled == false)
		consumer->hv_charging_disabled = true;
	else if (en && consumer->hv_charging_disabled == true)
		consumer->hv_charging_disabled = false;
	else {
		pr_info("[%s] already set: %d %d\n", __func__,
			consumer->hv_charging_disabled, en);
		return 0;
	}

	mutex_lock(&consumer_mutex);
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct charger_consumer, list);
		if (ptr->hv_charging_disabled == true) {
			info->enable_hv_charging = false;
			break;
		}
		if (list_is_last(pos, phead))
			info->enable_hv_charging = true;
	}
	mutex_unlock(&consumer_mutex);

	pr_info("%s: user: %s, en = %d\n", __func__, dev_name(consumer->dev),
		info->enable_hv_charging);

	if (mtk_pe50_get_is_connect(info) && !info->enable_hv_charging)
		mtk_pe50_stop_algo(info, true);

	_wake_up_charger(info);

	return 0;
}
EXPORT_SYMBOL(charger_manager_enable_high_voltage_charging);

int charger_manager_enable_power_path(struct charger_consumer *consumer,
	int idx, bool en)
{
	int ret = 0;
	bool is_en = true;
	struct charger_manager *info = consumer->cm;
	struct charger_device *chg_dev = NULL;

	if (!info)
		return -EINVAL;

	switch (idx) {
	case MAIN_CHARGER:
		chg_dev = info->chg1_dev;
		break;
	case SLAVE_CHARGER:
		chg_dev = info->chg2_dev;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);
	info->enable_pp[idx] = en;

	if (info->force_disable_pp[idx])
		goto out;

	ret = charger_dev_is_powerpath_enabled(chg_dev, &is_en);
	if (ret < 0) {
		chr_err("%s: get is power path enabled failed\n", __func__);
		goto out;
	}
	if (is_en == en) {
		chr_err("%s: power path is already en = %d\n", __func__, is_en);
		goto out;
	}

	pr_info("%s: enable power path = %d\n", __func__, en);
	if(en)
	      vote(info->bbc_suspend_votable, CHARGER_MANAGER_VOTER, false, 0);
	else
	      vote(info->bbc_suspend_votable, CHARGER_MANAGER_VOTER, true, 1);
out:
	mutex_unlock(&info->pp_lock[idx]);
	return ret;
}

int charger_manager_force_disable_power_path(struct charger_consumer *consumer,
	int idx, bool disable)
{
	struct charger_manager *info = consumer->cm;
	struct charger_device *chg_dev = NULL;
	int ret = 0;
	if (info == NULL)
		return -ENODEV;
	switch (idx) {
	case MAIN_CHARGER:
		chg_dev = info->chg1_dev;
		break;
	case SLAVE_CHARGER:
		chg_dev = info->chg2_dev;
		break;
	default:
		ret = -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);

	if (disable == info->force_disable_pp[idx])
		goto out;

	info->force_disable_pp[idx] = disable;
	if(info->force_disable_pp[idx] || !(info->enable_pp[idx]))
	      vote(info->bbc_suspend_votable, CHARGER_MANAGER_VOTER, true, 1);
	else
	      vote(info->bbc_suspend_votable, CHARGER_MANAGER_VOTER, false, 0);
out:
	mutex_unlock(&info->pp_lock[idx]);
	return ret;
}

static int _charger_manager_enable_charging(struct charger_consumer *consumer,
	int idx, bool en)
{
	struct charger_manager *info = consumer->cm;

	chr_err("%s: dev:%s idx:%d en:%d\n", __func__, dev_name(consumer->dev),
		idx, en);

	if (info != NULL) {
		struct charger_data *pdata = NULL;

		if (idx == MAIN_CHARGER)
			pdata = &info->chg1_data;
		else if (idx == SLAVE_CHARGER)
			pdata = &info->chg2_data;
		else
			return -ENOTSUPP;

		if (en == false) {
			_mtk_charger_do_charging(info, en);
			pdata->disable_charging_count++;
		} else {
			if (pdata->disable_charging_count == 1) {
				_mtk_charger_do_charging(info, en);
				pdata->disable_charging_count = 0;
			} else if (pdata->disable_charging_count > 1)
				pdata->disable_charging_count--;
		}
		chr_err("%s: dev:%s idx:%d en:%d cnt:%d\n", __func__,
			dev_name(consumer->dev), idx, en,
			pdata->disable_charging_count);

		return 0;
	}
	return -EBUSY;

}

int charger_manager_enable_charging(struct charger_consumer *consumer,
	int idx, bool en)
{
	struct charger_manager *info = consumer->cm;
	int ret = 0;

	mutex_lock(&info->charger_lock);
	ret = _charger_manager_enable_charging(consumer, idx, en);
	mutex_unlock(&info->charger_lock);
	return ret;
}

int charger_manager_get_input_current_limit(struct charger_consumer *consumer,
	int idx, int *input_current)
{
	struct charger_manager *info = consumer->cm;

	if (info != NULL) {
		struct charger_data *pdata;

		if (idx == MAIN_CHARGER)
			pdata = &info->chg1_data;
		else if (idx == SLAVE_CHARGER)
			pdata = &info->chg2_data;
		else
			return -ENOTSUPP;

		charger_dev_get_input_current(info->chg1_dev, input_current);
		return 0;
	}

	return -EBUSY;
}

int charger_manager_set_input_current_limit(struct charger_consumer *consumer,
	int idx, int input_current)
{
	struct charger_manager *info = consumer->cm;

	if (info != NULL) {
		struct charger_data *pdata;

		if (info->data.parallel_vbus) {
			if (idx == TOTAL_CHARGER) {
				info->chg1_data.thermal_input_current_limit =
					input_current;
				info->chg2_data.thermal_input_current_limit =
					input_current;
			} else
				return -ENOTSUPP;
		} else {
			if (idx == MAIN_CHARGER)
				pdata = &info->chg1_data;
			else if (idx == SLAVE_CHARGER)
				pdata = &info->chg2_data;
			else if (idx == MAIN_DIVIDER_CHARGER)
				pdata = &info->dvchg1_data;
			else if (idx == SLAVE_DIVIDER_CHARGER)
				pdata = &info->dvchg2_data;
			else
				return -ENOTSUPP;
			pdata->thermal_input_current_limit = input_current;
		}

		chr_err("%s: dev:%s idx:%d en:%d\n", __func__,
			dev_name(consumer->dev), idx, input_current);
		_mtk_charger_change_current_setting(info);
		_wake_up_charger(info);
		return 0;
	}
	return -EBUSY;
}

int charger_manager_set_charging_current_limit(
	struct charger_consumer *consumer, int idx, int charging_current)
{
	struct charger_manager *info = consumer->cm;

	if (info != NULL) {
		struct charger_data *pdata;

		if (idx == MAIN_CHARGER)
			pdata = &info->chg1_data;
		else if (idx == SLAVE_CHARGER)
			pdata = &info->chg2_data;
		else
			return -ENOTSUPP;

		pdata->thermal_charging_current_limit = charging_current;
		chr_err("%s: dev:%s idx:%d en:%d\n", __func__,
			dev_name(consumer->dev), idx, charging_current);
		_mtk_charger_change_current_setting(info);
		_wake_up_charger(info);
		return 0;
	}
	return -EBUSY;
}

int charger_manager_get_charger_temperature(struct charger_consumer *consumer,
	int idx, int *tchg_min,	int *tchg_max)
{
	struct charger_manager *info = consumer->cm;

	if (info != NULL) {
		struct charger_data *pdata = NULL;

		if (!upmu_get_rgs_chrdet()) {
			pr_debug("[%s] No cable in, skip it\n", __func__);
			*tchg_min = -127;
			*tchg_max = -127;
			return -EINVAL;
		}

		if (idx == MAIN_CHARGER)
			pdata = &info->chg1_data;
		else if (idx == SLAVE_CHARGER)
			pdata = &info->chg2_data;
		else if (idx == MAIN_DIVIDER_CHARGER)
			pdata = &info->dvchg1_data;
		else if (idx == SLAVE_DIVIDER_CHARGER)
			pdata = &info->dvchg2_data;
		else
			return -ENOTSUPP;

		*tchg_min = pdata->junction_temp_min;
		*tchg_max = pdata->junction_temp_max;

		return 0;
	}
	return -EBUSY;
}

int charger_manager_force_charging_current(struct charger_consumer *consumer,
	int idx, int charging_current)
{
	struct charger_manager *info = consumer->cm;

	if (info != NULL) {
		struct charger_data *pdata = NULL;

		if (idx == MAIN_CHARGER)
			pdata = &info->chg1_data;
		else if (idx == SLAVE_CHARGER)
			pdata = &info->chg2_data;
		else
			return -ENOTSUPP;

		pdata->force_charging_current = charging_current;
		_mtk_charger_change_current_setting(info);
		_wake_up_charger(info);
		return 0;
	}
	return -EBUSY;
}

int charger_manager_get_current_charging_type(struct charger_consumer *consumer)
{
	struct charger_manager *info = consumer->cm;

	if (info != NULL) {
		if (mtk_pe20_get_is_connect(info))
			return 2;
	}

	return 0;
}

int charger_manager_get_thermal_level(void)
{
	if (pinfo == NULL)
		return 0;

	return pinfo->thermal_level;
}

int charger_manager_get_max_thermal_level(void)
{
	return THERMAL_LIMIT_COUNT;
}

void charger_manager_set_thermal_level(int thermal_level)
{
	if (pinfo == NULL)
		return;

	if (thermal_level < 0 || pinfo->thermal_level < 0) {
		if (thermal_level <= -888)
			pinfo->thermal_level = 0;
		else if (thermal_level < 0)
			pinfo->thermal_level = thermal_level;
		return;
	}

	if (thermal_level > THERMAL_LIMIT_COUNT - 1)
		thermal_level = THERMAL_LIMIT_COUNT - 1;

	pinfo->thermal_level = thermal_level;
}

int charger_manager_get_thermal_limit_fcc(void)
{
	if (pinfo == NULL)
		return 0;

	return pinfo->thermal_limit_fcc;
}

void charger_manager_set_thermal_limit_fcc(int thermal_limit_fcc)
{
	if (pinfo == NULL)
		return;

	if (!is_between(0, pinfo->max_fcc, thermal_limit_fcc))
		return;

	pinfo->thermal_limit_fcc = thermal_limit_fcc;
}

int charger_manager_get_sic_current(void)
{
	if (pinfo == NULL)
		return 0;

	chr_err("%s sic_current=%d\n", __func__, pinfo->sic_current);
	return pinfo->sic_current * 1000;
}
EXPORT_SYMBOL(charger_manager_get_sic_current);

void charger_manager_set_sic_current(int sic_current)
{
	if (pinfo == NULL || !pinfo->bbc_fcc_votable)
		return;
	chr_err("%s sic_current=%d, sic_support=%d\n", __func__, sic_current, pinfo->sic_support);

	if (!pinfo->sic_support || !is_between(0, pinfo->max_fcc, sic_current))
		return;

	pinfo->sic_current = sic_current;
	vote(pinfo->bbc_fcc_votable, SIC_VOTER, true, sic_current);
}
EXPORT_SYMBOL(charger_manager_set_sic_current);

int charger_manager_get_input_suspend(void)
{
	if (pinfo == NULL)
		return false;

	return pinfo->input_suspend;
}

void charger_manager_set_input_suspend(bool input_suspend)
{
	if (pinfo == NULL)
		return;

	pinfo->input_suspend = input_suspend;
	if(input_suspend)
	      vote(pinfo->bbc_suspend_votable, BAT_SUSPEND_VOTER, true, 1);
	else
	      vote(pinfo->bbc_suspend_votable, BAT_SUSPEND_VOTER, false, 0);
	power_supply_changed(pinfo->usb_psy);
}

#define MAX_NOTCHARGE_IBAT	20

int charger_manager_get_charge_status(void)
{
	int charge_status = POWER_SUPPLY_STATUS_DISCHARGING;

	if(!pinfo)
		return charge_status;

	if (pinfo->chr_type) {
		if (pinfo->typec_burn || pinfo->input_suspend || pinfo->bms_i2c_error_count >= 10 ||
			!is_between(pinfo->jeita_fcc_cfg[0].low_threshold, pinfo->jeita_fcc_cfg[pinfo->step_jeita_tuple_count - 1].high_threshold, pinfo->tbat)) {
			charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			if (pinfo->charge_full && pinfo->soc == 100 && pinfo->ibat > (product_name == RUBYPLUS ? -150 : -80) && pinfo->ibat < (product_name == RUBYPLUS ? 150 : 80)) {
				charge_status = POWER_SUPPLY_STATUS_FULL;
			} else {
				charge_status = POWER_SUPPLY_STATUS_CHARGING;
			}
		}
	} else {
		charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	return charge_status;
}

int charger_manager_get_battery_health(void)
{
	union power_supply_propval pval = {0,};
	int tbat = 0, battery_health = POWER_SUPPLY_HEALTH_GOOD;

	if (!pinfo || !pinfo->bms_psy)
		return battery_health;

	if (pinfo->psy_type) {
		tbat = pinfo->tbat;
	} else {
		power_supply_get_property(pinfo->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
		tbat = pval.intval;
	}

	if (tbat <= -100)
		battery_health = POWER_SUPPLY_HEALTH_COLD;
	else if (tbat <= 150)
		battery_health = POWER_SUPPLY_HEALTH_COOL;
	else if (tbat <= 480)
		battery_health = POWER_SUPPLY_HEALTH_GOOD;
	else if (tbat <= 520)
		battery_health = POWER_SUPPLY_HEALTH_WARM;
	else if (tbat < 600)
		battery_health = POWER_SUPPLY_HEALTH_HOT;
	else
		battery_health = POWER_SUPPLY_HEALTH_OVERHEAT;

	return battery_health;
}

int charger_manager_get_zcv(struct charger_consumer *consumer, int idx, u32 *uV)
{
	struct charger_manager *info = consumer->cm;
	int ret = 0;
	struct charger_device *pchg = NULL;


	if (info != NULL) {
		if (idx == MAIN_CHARGER) {
			pchg = info->chg1_dev;
			ret = charger_dev_get_zcv(pchg, uV);
		} else if (idx == SLAVE_CHARGER) {
			pchg = info->chg2_dev;
			ret = charger_dev_get_zcv(pchg, uV);
		} else
			ret = -1;

	} else {
		chr_err("%s info is null\n", __func__);
	}
	chr_err("%s zcv:%d ret:%d\n", __func__, *uV, ret);

	return 0;
}

int charger_manager_enable_sc(struct charger_consumer *consumer,
	bool en, int stime, int etime)
{
	struct charger_manager *info = consumer->cm;

	chr_err("%s en:%d %d %d\n", __func__,
		en, stime, etime);
	info->sc.start_time = stime;
	info->sc.end_time = etime;
	info->sc.enable = en;
	_wake_up_charger(info);
	return 0;
}

int charger_manager_set_sc_current_limit(struct charger_consumer *consumer,
	int cl)
{
	struct charger_manager *info = consumer->cm;

	chr_err("%s %d\n", __func__,
		cl);
	info->sc.current_limit = cl;
	_wake_up_charger(info);
	return 0;
}

int charger_manager_set_bh(struct charger_consumer *consumer,
	int bh)
{
	struct charger_manager *info = consumer->cm;

	chr_err("%s %d\n", __func__,
		bh);
	info->sc.bh = bh;
	_wake_up_charger(info);
	return 0;
}

int charger_manager_enable_chg_type_det(struct charger_consumer *consumer, bool en)
{
	struct charger_manager *info = consumer->cm;
	int ret = 0;

	chr_info("enable_chg_type_det, enable = %d\n", en);

	if (en) {
		if (!info->attach_wakelock->active)
			__pm_stay_awake(info->attach_wakelock);
	} else {
		__pm_relax(info->attach_wakelock);
	}

	ret = charger_dev_enable_chg_type_det(info->chg2_dev, en);
	if (ret < 0) {
		chr_err("%s: en chgdet fail, en = %d\n", __func__, en);
		return ret;
		}

	return ret;
}

int register_charger_manager_notifier(struct charger_consumer *consumer,
	struct notifier_block *nb)
{
	int ret = 0;
	struct charger_manager *info = consumer->cm;


	mutex_lock(&consumer_mutex);
	if (info != NULL)
		ret = srcu_notifier_chain_register(&info->evt_nh, nb);
	else
		consumer->pnb = nb;
	mutex_unlock(&consumer_mutex);

	return ret;
}

int unregister_charger_manager_notifier(struct charger_consumer *consumer,
				struct notifier_block *nb)
{
	int ret = 0;
	struct charger_manager *info = consumer->cm;

	mutex_lock(&consumer_mutex);
	if (info != NULL)
		ret = srcu_notifier_chain_unregister(&info->evt_nh, nb);
	else
		consumer->pnb = NULL;
	mutex_unlock(&consumer_mutex);

	return ret;
}

/* user interface end*/

/* factory mode */
#define CHARGER_DEVNAME "charger_ftm"
#define GET_IS_SLAVE_CHARGER_EXIST _IOW('k', 13, int)

static struct class *charger_class;
static struct cdev *charger_cdev;
static int charger_major;
static dev_t charger_devno;

static int is_slave_charger_exist(void)
{
	if (get_charger_by_name("secondary_chg") == NULL)
		return 0;
	return 1;
}

static long charger_ftm_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;
	int out_data = 0;
	void __user *user_data = (void __user *)arg;

	switch (cmd) {
	case GET_IS_SLAVE_CHARGER_EXIST:
		out_data = is_slave_charger_exist();
		ret = copy_to_user(user_data, &out_data, sizeof(out_data));
		chr_err("[%s] SLAVE_CHARGER_EXIST: %d\n", __func__, out_data);
		break;
	default:
		chr_err("[%s] Error ID\n", __func__);
		break;
	}

	return ret;
}
#ifdef CONFIG_COMPAT
static long charger_ftm_compat_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case GET_IS_SLAVE_CHARGER_EXIST:
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);
		break;
	default:
		chr_err("[%s] Error ID\n", __func__);
		break;
	}

	return ret;
}
#endif
static int charger_ftm_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int charger_ftm_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations charger_ftm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = charger_ftm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = charger_ftm_compat_ioctl,
#endif
	.open = charger_ftm_open,
	.release = charger_ftm_release,
};

void charger_ftm_init(void)
{
	struct class_device *class_dev = NULL;
	int ret = 0;

	ret = alloc_chrdev_region(&charger_devno, 0, 1, CHARGER_DEVNAME);
	if (ret < 0) {
		chr_err("[%s]Can't get major num for charger_ftm\n", __func__);
		return;
	}

	charger_cdev = cdev_alloc();
	if (!charger_cdev) {
		chr_err("[%s]cdev_alloc fail\n", __func__);
		goto unregister;
	}
	charger_cdev->owner = THIS_MODULE;
	charger_cdev->ops = &charger_ftm_fops;

	ret = cdev_add(charger_cdev, charger_devno, 1);
	if (ret < 0) {
		chr_err("[%s] cdev_add failed\n", __func__);
		goto free_cdev;
	}

	charger_major = MAJOR(charger_devno);
	charger_class = class_create(THIS_MODULE, CHARGER_DEVNAME);
	if (IS_ERR(charger_class)) {
		chr_err("[%s] class_create failed\n", __func__);
		goto free_cdev;
	}

	class_dev = (struct class_device *)device_create(charger_class,
				NULL, charger_devno, NULL, CHARGER_DEVNAME);
	if (IS_ERR(class_dev)) {
		chr_err("[%s] device_create failed\n", __func__);
		goto free_class;
	}

	pr_debug("%s done\n", __func__);
	return;

free_class:
	class_destroy(charger_class);
free_cdev:
	cdev_del(charger_cdev);
unregister:
	unregister_chrdev_region(charger_devno, 1);
}
/* factory mode end */

void mtk_charger_get_atm_mode(struct charger_manager *info)
{
	char atm_str[64] = {0};
	char *ptr = NULL, *ptr_e = NULL;
	char keyword[] = "androidboot.atm=";
	int size = 0;

	info->atm_enabled = false;

	ptr = strstr(saved_command_line, keyword);
	if (ptr != 0) {
		ptr_e = strstr(ptr, " ");
		if (ptr_e == NULL)
			goto end;

		size = ptr_e - (ptr + strlen(keyword));
		if (size <= 0)
			goto end;
		strncpy(atm_str, ptr + strlen(keyword), size);
		atm_str[size] = '\0';

		if (!strncmp(atm_str, "enable", strlen("enable")))
			info->atm_enabled = true;
	}
end:
	pr_info("%s: atm_enabled = %d\n", __func__, info->atm_enabled);
}

/* internal algorithm common function */
bool is_dual_charger_supported(struct charger_manager *info)
{
	if (info->chg2_dev == NULL)
		return false;
	return true;
}

int charger_enable_vbus_ovp(struct charger_manager *pinfo, bool enable)
{
	int ret = 0;
	u32 sw_ovp = 0;

	if (enable)
		sw_ovp = pinfo->data.max_charger_voltage_setting;
	else
		sw_ovp = 15000000;

	/* Enable/Disable SW OVP status */
	pinfo->data.max_charger_voltage = sw_ovp;

	chr_err("[%s] en:%d ovp:%d\n",
			    __func__, enable, sw_ovp);
	return ret;
}

bool is_typec_adapter(struct charger_manager *info)
{
	int rp;

	rp = adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL);
	if (info->pd_type == MTK_PD_CONNECT_TYPEC_ONLY_SNK &&
			rp != 500 &&
			info->chr_type != STANDARD_HOST &&
			info->chr_type != CHARGING_HOST &&
			info->chr_type != NONSTANDARD_CHARGER &&
			info->chr_type != HVDCP_CHARGER &&
			mtk_pe20_get_is_connect(info) == false &&
			mtk_pe_get_is_connect(info) == false &&
			info->enable_type_c == true)
		return true;

	return false;
}

int charger_get_vbus(void)
{
	int ret = 0;
	int vchr = 0;

	if (pinfo == NULL)
		return 0;
	ret = charger_dev_get_vbus(pinfo->chg1_dev, &vchr);
	if (ret < 0) {
		chr_err("%s: get vbus failed: %d\n", __func__, ret);
		return ret;
	}

	vchr = vchr / 1000;
	return vchr;
}

/* internal algorithm common function end */

/* sw jeita */
void sw_jeita_state_machine_init(struct charger_manager *info)
{
	struct sw_jeita_data *sw_jeita;

	if (info->enable_sw_jeita == true) {
		sw_jeita = &info->sw_jeita;
		info->battery_temp = battery_get_bat_temperature();

		if (info->battery_temp >= info->data.temp_t4_thres)
			sw_jeita->sm = TEMP_ABOVE_T4;
		else if (info->battery_temp > info->data.temp_t3_thres)
			sw_jeita->sm = TEMP_T3_TO_T4;
		else if (info->battery_temp >= info->data.temp_t2_thres)
			sw_jeita->sm = TEMP_T2_TO_T3;
		else if (info->battery_temp >= info->data.temp_t1_thres)
			sw_jeita->sm = TEMP_T1_TO_T2;
		else if (info->battery_temp >= info->data.temp_t0_thres)
			sw_jeita->sm = TEMP_T0_TO_T1;
		else
			sw_jeita->sm = TEMP_BELOW_T0;

		chr_err("[SW_JEITA] tmp:%d sm:%d\n",
			info->battery_temp, sw_jeita->sm);
	}
}

void do_sw_jeita_state_machine(struct charger_manager *info)
{
	struct sw_jeita_data *sw_jeita;

	sw_jeita = &info->sw_jeita;
	sw_jeita->pre_sm = sw_jeita->sm;
	sw_jeita->charging = true;

	/* JEITA battery temp Standard */
	if (info->battery_temp >= info->data.temp_t4_thres) {
		chr_err("[SW_JEITA] Battery Over high Temperature(%d) !!\n",
			info->data.temp_t4_thres);

		sw_jeita->sm = TEMP_ABOVE_T4;
		sw_jeita->charging = false;
	} else if (info->battery_temp > info->data.temp_t3_thres) {
		/* control 45 degree to normal behavior */
		if ((sw_jeita->sm == TEMP_ABOVE_T4)
		    && (info->battery_temp
			>= info->data.temp_t4_thres_minus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t4_thres_minus_x_degree,
				info->data.temp_t4_thres);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t3_thres,
				info->data.temp_t4_thres);

			sw_jeita->sm = TEMP_T3_TO_T4;
		}
	} else if (info->battery_temp >= info->data.temp_t2_thres) {
		if (((sw_jeita->sm == TEMP_T3_TO_T4)
		     && (info->battery_temp
			 >= info->data.temp_t3_thres_minus_x_degree))
		    || ((sw_jeita->sm == TEMP_T1_TO_T2)
			&& (info->battery_temp
			    <= info->data.temp_t2_thres_plus_x_degree))) {
			chr_err("[SW_JEITA] Battery Temperature not recovery to normal temperature charging mode yet!!\n");
		} else {
			chr_err("[SW_JEITA] Battery Normal Temperature between %d and %d !!\n",
				info->data.temp_t2_thres,
				info->data.temp_t3_thres);
			sw_jeita->sm = TEMP_T2_TO_T3;
		}
	} else if (info->battery_temp >= info->data.temp_t1_thres) {
		if ((sw_jeita->sm == TEMP_T0_TO_T1
		     || sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t1_thres_plus_x_degree)) {
			if (sw_jeita->sm == TEMP_T0_TO_T1) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
					info->data.temp_t1_thres_plus_x_degree,
					info->data.temp_t2_thres);
			}
			if (sw_jeita->sm == TEMP_BELOW_T0) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
					info->data.temp_t1_thres,
					info->data.temp_t1_thres_plus_x_degree);
				sw_jeita->charging = false;
			}
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t1_thres,
				info->data.temp_t2_thres);

			sw_jeita->sm = TEMP_T1_TO_T2;
		}
	} else if (info->battery_temp >= info->data.temp_t0_thres) {
		if ((sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t0_thres_plus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t0_thres,
				info->data.temp_t0_thres_plus_x_degree);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t0_thres,
				info->data.temp_t1_thres);

			sw_jeita->sm = TEMP_T0_TO_T1;
		}
	} else {
		chr_err("[SW_JEITA] Battery below low Temperature(%d) !!\n",
			info->data.temp_t0_thres);
		sw_jeita->sm = TEMP_BELOW_T0;
		sw_jeita->charging = false;
	}

	/* set CV after temperature changed */
	/* In normal range, we adjust CV dynamically */
	if (sw_jeita->sm != TEMP_T2_TO_T3) {
		if (sw_jeita->sm == TEMP_ABOVE_T4)
			sw_jeita->cv = info->data.jeita_temp_above_t4_cv;
		else if (sw_jeita->sm == TEMP_T3_TO_T4)
			sw_jeita->cv = info->data.jeita_temp_t3_to_t4_cv;
		else if (sw_jeita->sm == TEMP_T2_TO_T3)
			sw_jeita->cv = 0;
		else if (sw_jeita->sm == TEMP_T1_TO_T2)
			sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv;
		else if (sw_jeita->sm == TEMP_T0_TO_T1)
			sw_jeita->cv = info->data.jeita_temp_t0_to_t1_cv;
		else if (sw_jeita->sm == TEMP_BELOW_T0)
			sw_jeita->cv = info->data.jeita_temp_below_t0_cv;
		else
			sw_jeita->cv = info->data.battery_cv;
	} else {
		sw_jeita->cv = 0;
	}

	chr_err("[SW_JEITA]preState:%d newState:%d tmp:%d cv:%d\n",
		sw_jeita->pre_sm, sw_jeita->sm, info->battery_temp,
		sw_jeita->cv);
}

static ssize_t show_sw_jeita(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_sw_jeita);
	return sprintf(buf, "%d\n", pinfo->enable_sw_jeita);
}

static ssize_t store_sw_jeita(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_sw_jeita = false;
		else {
			pinfo->enable_sw_jeita = true;
			sw_jeita_state_machine_init(pinfo);
		}

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR(sw_jeita, 0644, show_sw_jeita,
		   store_sw_jeita);
/* sw jeita end*/

/* pump express series */
bool mtk_is_pep_series_connect(struct charger_manager *info)
{
	if (mtk_pe20_get_is_connect(info) || mtk_pe_get_is_connect(info))
		return true;

	return false;
}

static ssize_t show_pe20(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_pe_2);
	return sprintf(buf, "%d\n", pinfo->enable_pe_2);
}

static ssize_t store_pe20(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_pe_2 = false;
		else
			pinfo->enable_pe_2 = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR(pe20, 0644, show_pe20, store_pe20);

static ssize_t show_pe40(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_pe_4);
	return sprintf(buf, "%d\n", pinfo->enable_pe_4);
}

static ssize_t store_pe40(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_pe_4 = false;
		else
			pinfo->enable_pe_4 = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR(pe40, 0644, show_pe40, store_pe40);

/* pump express series end*/

static ssize_t show_charger_log_level(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	chr_err("%s: %d\n", __func__, chargerlog_level);
	return sprintf(buf, "%d\n", chargerlog_level);
}

static ssize_t store_charger_log_level(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = 0;

	chr_err("%s\n", __func__);

	if (buf != NULL && size != 0) {
		chr_err("%s: buf is %s\n", __func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (ret < 0) {
			chr_err("%s: kstrtoul fail, ret = %d\n", __func__, ret);
			return ret;
		}
		if (val < 0) {
			chr_err("%s: val is inavlid: %ld\n", __func__, val);
			val = 0;
		}
		chargerlog_level = val;
		chr_err("%s: log_level=%d\n", __func__, chargerlog_level);
	}
	return size;
}
static DEVICE_ATTR(charger_log_level, 0644, show_charger_log_level,
		store_charger_log_level);

static ssize_t show_pdc_max_watt_level(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	return sprintf(buf, "%d\n", mtk_pdc_get_max_watt(pinfo));
}

static ssize_t store_pdc_max_watt_level(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		mtk_pdc_set_max_watt(pinfo, temp);
		chr_err("[store_pdc_max_watt]:%d\n", temp);
	} else
		chr_err("[store_pdc_max_watt]: format error!\n");

	return size;
}
static DEVICE_ATTR(pdc_max_watt, 0644, show_pdc_max_watt_level,
		store_pdc_max_watt_level);

int mtk_get_dynamic_cv(struct charger_manager *info, unsigned int *cv)
{
	int ret = 0;
	u32 _cv, _cv_temp;
	unsigned int vbat_threshold[4] = {3400000, 0, 0, 0};
	u32 vbat_bif = 0, vbat_auxadc = 0, vbat = 0;
	u32 retry_cnt = 0;

	if (pmic_is_bif_exist()) {
		do {
			vbat_auxadc = battery_get_bat_voltage() * 1000;
			ret = pmic_get_bif_battery_voltage(&vbat_bif);
			vbat_bif = vbat_bif * 1000;
			if (ret >= 0 && vbat_bif != 0 &&
			    vbat_bif < vbat_auxadc) {
				vbat = vbat_bif;
				chr_err("%s: use BIF vbat = %duV, dV to auxadc = %duV\n",
					__func__, vbat, vbat_auxadc - vbat_bif);
				break;
			}
			retry_cnt++;
		} while (retry_cnt < 5);

		if (retry_cnt == 5) {
			ret = 0;
			vbat = vbat_auxadc;
			chr_err("%s: use AUXADC vbat = %duV, since BIF vbat = %duV\n",
				__func__, vbat_auxadc, vbat_bif);
		}

		/* Adjust CV according to the obtained vbat */
		vbat_threshold[1] = info->data.bif_threshold1;
		vbat_threshold[2] = info->data.bif_threshold2;
		_cv_temp = info->data.bif_cv_under_threshold2;

		if (!info->enable_dynamic_cv && vbat >= vbat_threshold[2]) {
			_cv = info->data.battery_cv;
			goto out;
		}

		if (vbat < vbat_threshold[1])
			_cv = 4608000;
		else if (vbat >= vbat_threshold[1] && vbat < vbat_threshold[2])
			_cv = _cv_temp;
		else {
			_cv = info->data.battery_cv;
			info->enable_dynamic_cv = false;
		}
out:
		*cv = _cv;
		chr_err("%s: CV = %duV, enable_dynamic_cv = %d\n",
			__func__, _cv, info->enable_dynamic_cv);
	} else
		ret = -ENOTSUPP;

	return ret;
}

int charger_manager_notifier(struct charger_manager *info, int event)
{
	return srcu_notifier_call_chain(&info->evt_nh, event, NULL);
}

int charger_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, psy_nb);
	struct power_supply *psy = v;
	union power_supply_propval val;
	int ret;
	int tmp = 0;

	if (strcmp(psy->desc->name, "battery") == 0) {
		ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_TEMP, &val);
		if (!ret) {
			tmp = val.intval / 10;
			if (info->battery_temp != tmp
			    && mt_get_charger_type() != CHARGER_UNKNOWN) {
				_wake_up_charger(info);
				chr_err("%s: %ld %s tmp:%d %d chr:%d\n",
					__func__, event, psy->desc->name, tmp,
					info->battery_temp,
					mt_get_charger_type());
			}
		}
	}

	return NOTIFY_DONE;
}

void mtk_charger_int_handler(void)
{
	chr_err("%s\n", __func__);

	if (pinfo == NULL) {
		chr_err("charger is not rdy ,skip1\n");
		return;
	}

	if (pinfo->init_done != true) {
		chr_err("charger is not rdy ,skip2\n");
		return;
	}

	if (mt_get_charger_type() == CHARGER_UNKNOWN) {
		mutex_lock(&pinfo->cable_out_lock);
		pinfo->cable_out_cnt++;
		chr_err("cable_out_cnt=%d\n", pinfo->cable_out_cnt);
		mutex_unlock(&pinfo->cable_out_lock);
		charger_manager_notifier(pinfo, CHARGER_NOTIFY_STOP_CHARGING);
	} else
		charger_manager_notifier(pinfo, CHARGER_NOTIFY_START_CHARGING);

	chr_err("wake_up_charger\n");
	_wake_up_charger(pinfo);
}

static int set_default_charger_parameters(struct charger_manager *info)
{
	int ret = 0;

	vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DEFAULT_ICL);
	vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, DEFAULT_FCC);
	vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, DEFAULT_VINMIN);
	vote(info->bbc_fv_votable, FFC_VOTER, true, info->fv - (product_name != RUBYPLUS ? pinfo->diff_fv_val : 2 * pinfo->diff_fv_val));
	vote(info->bbc_iterm_votable, FFC_VOTER, true, info->iterm);

	return ret;
}

static int mtk_charger_plug_in(struct charger_manager *info)
{
	union power_supply_propval pval = {0,};
	struct timespec time;

	chr_info("%s\n", __func__);

	info->charger_thread_polling = true;
	info->can_charging = true;
	info->enable_dynamic_cv = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;
	info->cv_wa_count = 0;
	info->ffc_enable = false;
	info->pd_verify_done = false;
	info->pd_verifed = false;
	info->charge_full = false;
	info->recharge = false;
	info->cp_taper = false;
	info->mtbf_test = false;
	info->last_thermal_level = -1;
	info->full_wa_iterm = 0;

	get_monotonic_boottime(&time);
	if (time.tv_sec <= 25)
		info->strong_qc2 = 5;
	else
		info->strong_qc2 = 2;

	/* Check whether charge pump trigger reset after plug out */
	if (!info->master_cp_psy)
		info->master_cp_psy = power_supply_get_by_name("cp_master");
	if (!info->slave_cp_psy)
		info->slave_cp_psy = power_supply_get_by_name("cp_slave");
	if (!info->third_cp_psy)
		info->third_cp_psy = power_supply_get_by_name("cp_third");
	if (info->master_cp_psy)
		power_supply_get_property(info->master_cp_psy,
				POWER_SUPPLY_PROP_LN_RESET_CHECK, &pval);
	if (info->slave_cp_psy)
		power_supply_get_property(info->slave_cp_psy,
				POWER_SUPPLY_PROP_LN_RESET_CHECK, &pval);
	if (info->third_cp_psy)
		power_supply_get_property(info->third_cp_psy,
				POWER_SUPPLY_PROP_LN_RESET_CHECK, &pval);

	reset_mi_charge_alg(info);
	charger_dev_enable_termination(info->chg1_dev, true);
	set_default_charger_parameters(info);
	vote(info->bbc_icl_votable, PDM_VOTER, false, 0);
	vote(info->bbc_icl_votable, QCM_VOTER, false, 0);
	vote(info->bbc_fcc_votable, PDM_VOTER, false, 0);
	vote(info->bbc_en_votable, BBC_ENABLE_VOTER, true, 1);
	vote(info->bbc_vinmin_votable, PDM_VOTER, false, 0);
	schedule_delayed_work(&info->charge_monitor_work, 0);
	charger_dev_plug_in(info->chg1_dev);

	power_supply_get_property(pinfo->bms_psy, POWER_SUPPLY_PROP_DEVICE_CHEM, &pval);
	strcpy(info->device_chem, pval.strval);

	return 0;
}

static int mtk_charger_plug_out(struct charger_manager *info)
{
	struct charger_data *pdata1 = &info->chg1_data;
	struct charger_data *pdata2 = &info->chg2_data;
	union power_supply_propval pval = {0,};
	struct timespec time;

	chr_info("%s\n", __func__);

	info->cv_wa_count = 0;
	info->apdo_max = 0;
	info->charger_thread_polling = false;
	pdata1->disable_charging_count = 0;
	pdata1->input_current_limit_by_aicl = -1;
	pdata2->disable_charging_count = 0;
	info->ffc_enable = false;
	info->entry_soc = 0;
	info->pd_verify_done = false;
	info->pd_verifed = false;
	info->charge_full = false;
	info->recharge = false;
	info->cp_taper = false;
	info->mtbf_test = false;
	info->last_thermal_level = -1;
	info->full_wa_iterm = 0;
	info->chr_type == CHARGER_UNKNOWN;
	pval.intval = 0;
	power_supply_set_property(pinfo->bms_psy, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	pval.intval = CHARGER_UNKNOWN;
	power_supply_set_property(info->charger_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);

	get_monotonic_boottime(&time);
	if (time.tv_sec <= 25)
		info->strong_qc2 = 5;
	else
		info->strong_qc2 = 2;

	set_default_charger_parameters(info);
	vote(info->bbc_icl_votable, PDM_VOTER, false, 0);
	vote(info->bbc_icl_votable, QCM_VOTER, false, 0);
	vote(info->bbc_fcc_votable, PDM_VOTER, false, 0);
	vote(info->bbc_fcc_votable, THERMAL_VOTER, false, 0);
	vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, false, 0);
	if (product_name != RUBYPLUS)
		vote(info->bbc_en_votable, BBC_ENABLE_VOTER, true, 0);
	vote(info->bbc_en_votable, FULL_ENABLE_VOTER, true, 1);
	vote(info->bbc_vinmin_votable, PDM_VOTER, false, 0);
	vote(info->bbc_icl_votable, FFC_VOTER, false, 0);
	vote(info->bbc_iterm_votable, ITERM_WA_VOTER, false, 0);
	if (!info->typec_burn)
		cancel_delayed_work_sync(&info->charge_monitor_work);

	if (product_name == RUBYPLUS) {
		info->max_power_flag = 0;
		cancel_delayed_work_sync(&info->max_power_work);
		vote(info->bbc_fcc_votable, MAX_POWER_VOTER, false, 0);
	}

	reset_mi_charge_alg(info);
	charger_dev_plug_out(info->chg1_dev);

	if (product_name == RUBYPLUS) {
		charger_dev_enable_wdt(info->cp_master, false, 0);
		charger_dev_enable_wdt(info->cp_slave, false, 0);
		charger_dev_enable_wdt(info->cp_third, false, 0);
	}

	return 0;
}
/*
static bool mtk_is_charger_on(struct charger_manager *info)
{
	enum charger_type chr_type;

	chr_type = mt_get_charger_type();
	if (chr_type == CHARGER_UNKNOWN) {
		if (info->chr_type != CHARGER_UNKNOWN) {
			mtk_charger_plug_out(info);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	} else {
		if (info->chr_type == CHARGER_UNKNOWN)
			mtk_charger_plug_in(info);
		else
			info->chr_type = chr_type;

		if (info->cable_out_cnt > 0) {
			mtk_charger_plug_out(info);
			mtk_charger_plug_in(info);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt--;
			mutex_unlock(&info->cable_out_lock);
		}
	}

	if (chr_type == CHARGER_UNKNOWN)
		return false;

	return true;
}

static void charger_update_data(struct charger_manager *info)
{
	info->battery_temp = battery_get_bat_temperature();
}*/

/*
static int check_charge_parameters(struct charger_manager *info)
{
	int div_rate = (product_name == RUBYPLUS ? 2 : 1);
	//bool bbc_wa = (product_name == RUBY && info->vbus >= 6500);

	switch(info->psy_type) {
	case POWER_SUPPLY_TYPE_USB:
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, SDP_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, SDP_FCC / div_rate);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, SDP_VINMIN);
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, CDP_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, CDP_FCC / div_rate);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, CDP_VINMIN);
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DCP_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, DCP_FCC / div_rate);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, DCP_VINMIN);

		if (info->i350_type == XMUSB350_TYPE_DCP) {
			vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DCP_ICL / (info->recheck_count >= 2 ? 1 : 2));
			vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, DCP_FCC / div_rate);
			vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, DCP_VINMIN);
		} else if (info->i350_type == XMUSB350_TYPE_OCP) {
			vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, OCP_ICL);
			vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, OCP_FCC / div_rate);
			vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, OCP_VINMIN);
		}
		break;
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, FLOAT_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, FLOAT_FCC / div_rate);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, FLOAT_VINMIN);
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC2_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC2_FCC / div_rate);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, QC2_VINMIN);

		if (info->recheck_count >= 2 && info->i350_type == XMUSB350_TYPE_HVDCP_2) {
			if (info->strong_qc2 && info->cv_wa_count < CV_MP2762_WA_COUNT) {
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC2_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC2_FCC / div_rate);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, QC2_VINMIN);
			} else {
				if (bbc_wa) {
					charger_dev_enable_powerpath(info->chg1_dev, false);
					msleep(500);
				}
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, DCP_FCC / div_rate);
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DCP_ICL);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, DCP_VINMIN);
				charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_5);
				msleep(250);
				if (bbc_wa)
					charger_dev_enable_powerpath(info->chg1_dev, true);
			}
		}
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS:
		if (info->recheck_count >= 2) {
			switch(info->qc3_type) {
			case HVDCP3_18:
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC3_18_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC3_18_FCC / div_rate);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus > QC3_18_VINMIN) ? QC3_18_VINMIN : DEFAULT_VINMIN);
				break;
			case HVDCP3_27:
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC3_27_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC3_27_FCC / div_rate);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus > QC3_27_VINMIN) ? QC3_27_VINMIN : DEFAULT_VINMIN);
				break;
			case HVDCP35_18:
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC35_18_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC35_18_FCC / div_rate);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus > QC35_18_VINMIN) ? QC35_18_VINMIN : DEFAULT_VINMIN);
				break;
			case HVDCP35_27:
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC35_27_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC35_27_FCC / div_rate);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus > QC35_27_VINMIN) ? QC35_27_VINMIN : DEFAULT_VINMIN);
				break;
			default:
				chr_err("not support qc3_type to check charger parameters");
			}
		}
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, PD3_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, info->max_fcc);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus > PD3_VINMIN) ? PD3_VINMIN : DEFAULT_VINMIN);
		if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->recheck_count < 4)
			power_supply_changed(info->usb_psy);
		break;
	default:
		chr_err("not support psy_type to check charger parameters");
	}

	charger_dev_dump_registers(info->chg1_dev);
	charger_dev_dump_registers(info->chg2_dev);

	return 0;
}
*/

static int check_vinlim(struct charger_manager *info)
{
	int pulse_type = 0, count = 0, ret = 0;

	if (info->recheck_count < 2 && product_name == RUBYPLUS) {
		return ret;
	}
	switch(info->psy_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		if (info->i350_type == XMUSB350_TYPE_HVDCP_2) {
			if (product_name == RUBY || product_name == RUBYPRO) {
				if (info->strong_qc2)
					charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_9);
				if (info->strong_qc2 && info->vbus < 8000)
					info->strong_qc2--;
			} else if (product_name == RUBYPLUS) {
				if (info->strong_qc2 && info->cv_wa_count < CV_MP2762_WA_COUNT)
					charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_12);
				if (info->strong_qc2 && info->vbus < 10000)
					info->strong_qc2--;
			}
		}
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		if (info->vbus < BBC_BUCK_LOW_VBUS) {
			pulse_type = QC3_DP_PULSE;
			count = (BBC_BUCK_LOW_VBUS - info->vbus) / QC3_PULSE_STEP;
		} else if (info->vbus > BBC_BUCK_HIGH_VBUS) {
			pulse_type = QC3_DM_PULSE;
			count = (info->vbus - BBC_BUCK_HIGH_VBUS) / QC3_PULSE_STEP;
		}
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS:
		if (info->vbus < BBC_BUCK_LOW_VBUS) {
			pulse_type = QC35_DP_PULSE;
			count = (BBC_BUCK_LOW_VBUS - info->vbus) / QC35_PULSE_STEP;
		} else if (info->vbus > BBC_BUCK_HIGH_VBUS) {
			pulse_type = QC35_DM_PULSE;
			count = (info->vbus - BBC_BUCK_HIGH_VBUS) / QC35_PULSE_STEP;
		}
		break;
	}

	if ((product_name == RUBYPLUS)&& count) {
		chr_info("[CHARGE_LOOP] pulse_type = %d, count = %d\n", pulse_type, count);
		charger_dev_qc3_dpdm_pulse(info->chg2_dev, pulse_type, count);
	}

	return ret;
}

static void check_cv_vbus(struct charger_manager *info)
{
	if (product_name != RUBYPLUS || info->recheck_count < 2)
		return;

	if (info->psy_type != POWER_SUPPLY_TYPE_USB_HVDCP && info->psy_type != POWER_SUPPLY_TYPE_USB_PD)
		return;

	if (info->cv_wa_count < CV_MP2762_WA_COUNT) {
		if (info->vbat > (info->jeita_fv_cfg[info->jeita_chg_index[0]].value - 100) && info->ibat > -CV_IBAT_MP2762_WA && info->ibat < 0
			&& info->vbus > CV_VBUS_MP2762_WA && info->vbus < 16000)
			info->cv_wa_count++;
		else
			info->cv_wa_count = 0;
	} else {
		if (info->cv_wa_count == CV_MP2762_WA_COUNT) {
			if (info->psy_type == POWER_SUPPLY_TYPE_USB_HVDCP) {
				vote(info->bbc_suspend_votable, CV_WA_VOTER, true, 1);
				msleep(400);
				charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_5);
				vote(info->bbc_suspend_votable, CV_WA_VOTER, false, 0);
			} else if (info->psy_type == POWER_SUPPLY_TYPE_USB_PD) {
				power_supply_changed(info->usb_psy);
			}
		}
		if (info->vbat > (info->jeita_fv_cfg[info->jeita_chg_index[0]].value - 100)) {
			if (info->cv_wa_count < (2 * CV_MP2762_WA_COUNT))
				info->cv_wa_count++;
		} else {
			info->cv_wa_count--;
		}
	}
}

static void update_real_type_data(struct charger_manager *info)
{
	static enum power_supply_type real_type_old = POWER_SUPPLY_TYPE_UNKNOWN;
	int ret = 0;
	union power_supply_propval real_type = {0,};
	union power_supply_propval pval = {0,};

	ret = power_supply_get_property(info->charger_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
	if (ret)
		chr_err("failed to get mtk charger type\n");
	else
		info->chr_type = pval.intval;

	chr_err("%s: %d\n", __func__, info->chr_type);

	if (info->chr_type == CHARGER_UNKNOWN) {
		if(real_type_old != POWER_SUPPLY_TYPE_UNKNOWN)
		{
			real_type.intval = POWER_SUPPLY_TYPE_UNKNOWN;
			power_supply_set_property(info->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &real_type);
			power_supply_changed(info->usb_psy);
		}
		real_type_old = POWER_SUPPLY_TYPE_UNKNOWN;
		return;
	}

	if (info->mtbf_test) {
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, CDP_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, CDP_FCC);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, CDP_VINMIN);
		real_type.intval = POWER_SUPPLY_TYPE_USB_CDP;
		chr_err("mtbf_test, set input current charging 1.5A\n");
	} else if ((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK || info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) && info->chr_type != STANDARD_HOST) {
		if (info->pd_verifed){
			vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, PD3_ICL);
			vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, info->max_fcc);
		}
		else {
			if (product_name == RUBYPLUS){
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, (info->vbus > PD3_VINMIN) ? PD3_ICL : PD3_RUBYPLUS_DFAULT_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, 3000);
			}
			else{
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, PD3_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, 5800);
			}
		}
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus > PD3_VINMIN) ? PD3_VINMIN : DEFAULT_VINMIN);
		//if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->recheck_count < 4)
		//	power_supply_changed(info->usb_psy);
		real_type.intval = POWER_SUPPLY_TYPE_USB_PD;
		chr_err("pd use vote current charging = %d\n", (info->vbus > PD3_VINMIN) ? PD3_ICL : PD3_RUBYPLUS_DFAULT_ICL);
	} else if (info->chr_type == STANDARD_HOST && get_pd_usb_connected()) {
		if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->pd_verifed) {
			vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, PD3_ICL);
			vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, info->max_fcc);
			vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus > PD3_VINMIN) ? PD3_VINMIN : DEFAULT_VINMIN);
			real_type.intval = POWER_SUPPLY_TYPE_USB_PD;
			chr_err("when pd connect hifi,  use vote current charging = %d\n", PD3_ICL);
		} else {
			vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, C2C_ICL);
			vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, C2C_FCC);
			vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, C2C_VINMIN);
			if (product_name != RUBYPLUS)
				real_type.intval = POWER_SUPPLY_TYPE_USB_PD;
			else
				real_type.intval = POWER_SUPPLY_TYPE_USB_CDP;
			chr_err("C to C set input current charging = %d\n", C2C_ICL);
		}
	} else if (info->chr_type == HVDCP_CHARGER) {
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC2_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC2_FCC);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus > QC2_VINMIN) ? QC2_VINMIN : DEFAULT_VINMIN);
		real_type.intval = POWER_SUPPLY_TYPE_USB_HVDCP;
		chr_err("HVDCP set input current charging = %d\n", QC2_ICL);
	} else if (info->chr_type == STANDARD_HOST) {
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, SDP_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, SDP_FCC);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, SDP_VINMIN);
		real_type.intval = POWER_SUPPLY_TYPE_USB;
		chr_err("SDP_USB set input current charging = %d\n", SDP_ICL);
	} else if (info->chr_type == CHARGING_HOST) {
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, CDP_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, CDP_FCC);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, CDP_VINMIN);
		real_type.intval = POWER_SUPPLY_TYPE_USB_CDP;
		chr_err("CDP set input current charging = %d\n", CDP_ICL);
	} else if (info->chr_type == STANDARD_CHARGER) {
		if (!pinfo->xmusb350_psy){
			vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DCP_ICL);
			vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, DCP_FCC);
			vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, DCP_VINMIN);
			real_type.intval = POWER_SUPPLY_TYPE_USB_DCP;
			chr_err("DCP set input current charging = %d\n", DCP_ICL);
		}else{
			if(info->qc3_type == HVDCP3_18 || info->i350_type == XMUSB350_TYPE_HVDCP_3){
				real_type.intval = POWER_SUPPLY_TYPE_USB_HVDCP_3;
				if(info->recheck_count >= 2){
					vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC3_18_RUBYPLUS_ICL);
					vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC3_18_RUBYPLUS_FCC);
					vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus >= QC3_18_RUBYPLUS_VINMIN) ? QC3_18_RUBYPLUS_VINMIN : DEFAULT_VINMIN);
					chr_err("HVDCP_3_18 set input current charging = %d\n", QC3_18_RUBYPLUS_ICL);
				}
			}else if(info->qc3_type == HVDCP3_27){
				real_type.intval = POWER_SUPPLY_TYPE_USB_HVDCP_3;
				if(info->recheck_count >= 2){
					vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC3_27_RUBYPLUS_ICL);
					vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC3_27_RUBYPLUS_FCC);
					vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus >= QC3_27_RUBYPLUS_VINMIN) ? QC3_27_RUBYPLUS_VINMIN : DEFAULT_VINMIN);
					chr_err("HVDCP_3_27 set input current charging = %d\n", QC3_27_RUBYPLUS_ICL);
				}
			}else if(info->qc3_type == HVDCP35_18){
				real_type.intval = POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS;
				if(info->recheck_count >= 2){
					vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC35_18_RUBYPLUS_ICL);
					vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC35_18_RUBYPLUS_FCC);
					vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus >= QC35_18_RUBYPLUS_VINMIN) ? QC35_18_RUBYPLUS_VINMIN : DEFAULT_VINMIN);
					chr_err("HVDCP_3_PLUS set input current charging = %d\n", QC35_18_RUBYPLUS_ICL);
				}
			}else if(info->qc3_type == HVDCP35_27){
				real_type.intval = POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS;
				if(info->recheck_count >= 2){
					vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC35_27_RUBYPLUS_ICL);
					vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC35_27_RUBYPLUS_FCC);
					vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus >= QC35_27_RUBYPLUS_VINMIN) ? QC35_27_RUBYPLUS_VINMIN : DEFAULT_VINMIN);
					chr_err("HVDCP_3_PLUS set input current charging = %d\n", QC35_27_RUBYPLUS_ICL);
				}
			}else if(info->i350_type == XMUSB350_TYPE_HVDCP_2 || info->i350_type == XMUSB350_TYPE_HVDCP){
				real_type.intval = POWER_SUPPLY_TYPE_USB_HVDCP;
				if(info->recheck_count >= 2){
					vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC2_RUBYPLUS_ICL);
					vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC2_RUBYPLUS_FCC);
					vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, (info->vbus >= QC2_RUBYPLUS_VINMIN) ? QC2_RUBYPLUS_VINMIN : DEFAULT_VINMIN);
					chr_err("HVDCP 2 set input current charging = %d\n", QC2_RUBYPLUS_ICL);
				}
			}else {
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DCP_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, DCP_FCC);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, DCP_VINMIN);
				real_type.intval = POWER_SUPPLY_TYPE_USB_DCP;
				chr_err("DCP 2 set input current charging = %d\n", DCP_ICL);
			}
		}
	} else if (info->chr_type == NONSTANDARD_CHARGER) {
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, FLOAT_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, FLOAT_FCC);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, FLOAT_VINMIN);
		real_type.intval = POWER_SUPPLY_TYPE_USB_FLOAT;
		chr_err("float type set input current charging = %d\n", FLOAT_ICL);
	} else {
		vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, OTHER_ICL);
		vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, OTHER_FCC);
		vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, OTHER_VINMIN);
		chr_err("others set input current charging = %d\n", OTHER_ICL);
	}

	if(real_type_old != real_type.intval){
		real_type_old = real_type.intval;
		power_supply_set_property(info->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &real_type);
		power_supply_changed(info->usb_psy);
		chr_err("%s,update real type to %d\n",__func__, real_type_old);
	}

	charger_dev_dump_registers(info->chg1_dev);
	charger_dev_dump_registers(info->chg2_dev);
}

static void check_charge_data(struct charger_manager *info)
{
	union power_supply_propval pval = {0,};
	enum power_supply_type psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
	enum charger_type chr_type = CHARGER_UNKNOWN;
	enum hvdcp3_type qc3_type = HVDCP3_NONE;
	enum xmusb350_chg_type i350_type = XMUSB350_TYPE_UNKNOW;
	int charge_mode = 0, ret = 0;

	ret = power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &pval);
	if (ret)
		chr_err("failed to get psy_type\n");
	else
		psy_type = pval.intval;

	ret = power_supply_get_property(info->charger_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
	if (ret)
		chr_err("failed to get mtk charger type\n");
	else
		chr_type = pval.intval;

	if (product_name == RUBYPLUS){
		if (!pinfo->xmusb350_psy)
		      pinfo->xmusb350_psy = power_supply_get_by_name("xmusb350");

		if (pinfo->xmusb350_psy) {
			ret = power_supply_get_property(info->xmusb350_psy, POWER_SUPPLY_PROP_HVDCP3_TYPE, &pval);
			if (ret)
			      chr_err("failed to get qc3 type\n");
			else
			      qc3_type = pval.intval;

			ret = power_supply_get_property(info->xmusb350_psy, POWER_SUPPLY_PROP_CHG_TYPE, &pval);
			if (ret)
			      chr_err("failed to get qc3 type\n");
			else
			      i350_type = pval.intval;

			ret = power_supply_get_property(info->xmusb350_psy, POWER_SUPPLY_PROP_RECHECK_COUNT, &pval);
			if (ret)
			      chr_err("failed to get recheck_count\n");
			else
			      info->recheck_count = pval.intval;
		}
	}

	if (info->chr_type == CHARGER_UNKNOWN && chr_type != CHARGER_UNKNOWN)
		info->charger_status = CHARGER_PLUGIN;
	else if (info->chr_type != CHARGER_UNKNOWN && chr_type == CHARGER_UNKNOWN)
		info->charger_status = CHARGER_PLUGOUT;
	else
		info->charger_status = CHARGER_UNCHANGED;

	info->psy_type = psy_type;
	info->chr_type = chr_type;
	info->qc3_type = qc3_type;
	info->i350_type = i350_type;

	charger_dev_get_vbus(info->chg1_dev, &info->vbus);
	charger_dev_get_ibus(info->chg1_dev, &info->ibus);
	charger_dev_is_enabled(info->chg1_dev, &info->bbc_enable);
	charger_dev_is_powerpath_enabled(info->chg1_dev, &info->pp_enable);
	charger_dev_get_charge_status(pinfo->chg1_dev, &info->charge_status);
	charger_dev_get_temperature(info->chg1_dev, &info->bbc_temp, &info->bbc_temp);
	charger_dev_get_temperature(info->cp_master, &info->cp_master_temp, &info->cp_master_temp);
	charger_dev_get_temperature(info->cp_slave, &info->cp_slave_temp, &info->cp_slave_temp);

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret)
		chr_err("failed to get soc\n");
	else
		info->soc = pval.intval;

	ret = power_supply_get_property(info->bms_psy,POWER_SUPPLY_PROP_CHARGE_DONE, &pval);
	if (ret)
		chr_err("failed to get fg_full\n");
	else
		info->fg_full = !!pval.intval;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret)
		chr_err("failed to get vbat\n");
	else
		info->vbat = pval.intval / 1000;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret)
		chr_err("failed to get ibat\n");
	else
		info->ibat = pval.intval / 1000;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret)
		chr_err("failed to get tbat\n");
	else
		info->tbat = pval.intval;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret)
		chr_err("failed to get cycle_count\n");
	else
		info->cycle_count = pval.intval;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_AUTHENTIC, &pval);
	if (ret)
		chr_err("failed to get bat_verifed\n");
	else
		info->bat_verifed = !!pval.intval;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_I2C_ERROR_COUNT, &pval);
	if (ret)
		chr_err("failed to get bms_i2c_error_count\n");
	else
		info->bms_i2c_error_count = pval.intval;

	/*
	if (info->bat_verifed)
		vote(info->bbc_fcc_votable, BAT_VERIFY_VOTER, false, 0);
	else
		vote(info->bbc_fcc_votable, BAT_VERIFY_VOTER, true, 3000);
	*/

	//charge_mode = get_charge_mode();

	if (info->charger_status == CHARGER_PLUGIN)
		info->entry_soc = info->soc;

	if (info->charger_status == CHARGER_PLUGIN) {
		pval.intval = FG_MONITOR_DELAY_8S;
	} else if (info->charger_status == CHARGER_PLUGOUT) {
		pval.intval = FG_MONITOR_DELAY_17P5S;
	} else if (info->ibat < 0 && info->psy_type) {
		if (info->ibat <= -HIGH_CHARGE_SPEED)
			pval.intval = FG_MONITOR_DELAY_2P5S;
		else if (info->ibat <= -NORMAL_CHARGE_SPEED)
			pval.intval = FG_MONITOR_DELAY_5S;
		else
			pval.intval = FG_MONITOR_DELAY_8S;
	} else {
		pval.intval = FG_MONITOR_DELAY_8S;
	}
	power_supply_set_property(pinfo->bms_psy, POWER_SUPPLY_PROP_MONITOR_DELAY, &pval);

	if (info->pp_enable == !!get_effective_result(info->bbc_suspend_votable)) {
		rerun_election(info->bbc_suspend_votable);
		power_supply_changed(info->usb_psy);
	}

	chr_info("[CHARGE_LOOP]TYPE = [%d %d %d %d %d], BUS = [%d %d], BAT = [%d %d %d %d %d %d], BBC = [%d %d %d %d %d], TL_SJ = [%d %d %d %d], TEMP = [%d %d %d], FFC = [%d %d], COUNT = [%d %d]\n",
		info->psy_type, info->qc3_type, info->i350_type, info->pd_type, info->pd_verifed,
		info->vbus, info->ibus,
		info->soc, info->vbat, info->ibat, info->tbat, info->fg_full, info->cycle_count,
		info->bbc_enable, info->pp_enable, info->charge_status, info->charge_full, info->recharge,
		info->sic_current, info->thermal_level, info->step_chg_index[0], info->jeita_chg_index[0],
		info->bbc_temp, info->cp_master_temp, info->cp_slave_temp, info->ffc_enable, charge_mode, info->cv_wa_count, info->recheck_count);

	if (info->input_suspend || info->typec_burn || !info->bat_verifed || info->bms_i2c_error_count >= 10)
		chr_err("[CHARGE_LOOP] input_suspend = %d, typec_burn = %d, bat_verifed = %d, bms_i2c_error_count = %d\n", info->input_suspend, info->typec_burn, info->bat_verifed, info->bms_i2c_error_count);
}

static void check_battery_connector(struct charger_manager *info)
{
	int third_cp_vbat = 0;
	chr_err("check_battery_connector ++\n");
	charger_dev_get_vbat(info->cp_third, &third_cp_vbat);
	chr_err("check_battery_connector third_cp_vbat = %d\n", third_cp_vbat);

	if(info->bms_i2c_error_count >= 10){
		if(product_name != RUBYPLUS)
			vote(info->bbc_icl_votable, BATTERY_CONNECTOR_VOTER, true, 500);
		if(product_name == RUBYPLUS)
			vote(info->bbc_icl_votable, BATTERY_CONNECTOR_VOTER, true, 250);
	}
	else if((product_name == RUBYPLUS) && (third_cp_vbat < 6000))
		vote(info->bbc_icl_votable, BATTERY_CONNECTOR_VOTER, true, 250);
}

static int init_charger_hw_limit(struct charger_manager *info)
{
	int ret = 0;

	ret = vote(info->bbc_fv_votable, HW_LIMIT_VOTER, true, info->fv_ffc - (product_name != RUBYPLUS ? pinfo->diff_fv_val : 2 * pinfo->diff_fv_val));
	if (ret)
		chr_err("failed to init FV HW_LIMIT\n");

	ret = vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DEFAULT_ICL);
	if (ret)
		chr_err("failed to init ICL HW_LIMIT\n");

	ret = vote(info->bbc_fcc_votable, HW_LIMIT_VOTER, true, info->max_fcc);
	if (ret)
		chr_err("failed to init FCC HW_LIMIT\n");

	return ret;
}

static int mtk_chgstat_notify(struct charger_manager *info)
{
	int ret = 0;
	char *env[2] = { "CHGSTAT=1", NULL };

	chr_err("%s: 0x%x\n", __func__, info->notify_code);
	ret = kobject_uevent_env(&info->pdev->dev.kobj, KOBJ_CHANGE, env);
	if (ret)
		chr_err("%s: kobject_uevent_fail, ret=%d", __func__, ret);

	return ret;
}

#if 0
/* return false if vbus is over max_charger_voltage */
static bool mtk_chg_check_vbus(struct charger_manager *info)
{
	int vchr = 0;

	vchr = battery_get_vbus() * 1000; /* uV */
	if (vchr > info->data.max_charger_voltage) {
		chr_err("%s: vbus(%d mV) > %d mV\n", __func__, vchr / 1000,
			info->data.max_charger_voltage / 1000);
		return false;
	}

	return true;

static void mtk_battery_notify_VCharger_check(struct charger_manager *info)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	int vchr = 0;

	vchr = battery_get_vbus() * 1000; /* uV */
	if (vchr < info->data.max_charger_voltage)
		info->notify_code &= ~CHG_VBUS_OV_STATUS;
	else {
		info->notify_code |= CHG_VBUS_OV_STATUS;
		chr_err("[BATTERY] charger_vol(%d mV) > %d mV\n",
			vchr / 1000, info->data.max_charger_voltage / 1000);
		mtk_chgstat_notify(info);
	}
#endif
}

static void mtk_battery_notify_VBatTemp_check(struct charger_manager *info)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)
	if (info->battery_temp >= info->thermal.max_charge_temp) {
		info->notify_code |= CHG_BAT_OT_STATUS;
		chr_err("[BATTERY] bat_temp(%d) out of range(too high)\n",
			info->battery_temp);
		mtk_chgstat_notify(info);
	} else {
		info->notify_code &= ~CHG_BAT_OT_STATUS;
	}

	if (info->enable_sw_jeita == true) {
		if (info->battery_temp < info->data.temp_neg_10_thres) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
	} else {
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
		if (info->battery_temp < info->thermal.min_charge_temp) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
#endif
	}
#endif
}

static void mtk_battery_notify_UI_test(struct charger_manager *info)
{
	switch (info->notify_test_mode) {
	case 1:
		info->notify_code = CHG_VBUS_OV_STATUS;
		pr_debug("[%s] CASE_0001_VCHARGER\n", __func__);
		break;
	case 2:
		info->notify_code = CHG_BAT_OT_STATUS;
		pr_debug("[%s] CASE_0002_VBATTEMP\n", __func__);
		break;
	case 3:
		info->notify_code = CHG_OC_STATUS;
		pr_debug("[%s] CASE_0003_ICHARGING\n", __func__);
		break;
	case 4:
		info->notify_code = CHG_BAT_OV_STATUS;
		pr_debug("[%s] CASE_0004_VBAT\n", __func__);
		break;
	case 5:
		info->notify_code = CHG_ST_TMO_STATUS;
		pr_debug("[%s] CASE_0005_TOTAL_CHARGINGTIME\n", __func__);
		break;
	case 6:
		info->notify_code = CHG_BAT_LT_STATUS;
		pr_debug("[%s] CASE6: VBATTEMP_LOW\n", __func__);
		break;
	case 7:
		info->notify_code = CHG_TYPEC_WD_STATUS;
		pr_debug("[%s] CASE7: Moisture Detection\n", __func__);
		break;
	default:
		pr_debug("[%s] Unknown BN_TestMode Code: %x\n",
			__func__, info->notify_test_mode);
	}
	mtk_chgstat_notify(info);
}

static void mtk_battery_notify_check(struct charger_manager *info)
{
	if (info->notify_test_mode == 0x0000) {
		mtk_battery_notify_VCharger_check(info);
		mtk_battery_notify_VBatTemp_check(info);
	} else {
		mtk_battery_notify_UI_test(info);
	}
}
static void check_battery_exist(struct charger_manager *info)
{
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	unsigned int i = 0;
	int count = 0;
	int boot_mode = 11;//UNKNOWN_BOOT
// workaround for mt6768 
	//int boot_mode = get_boot_mode();
	dev = &(info->pdev->dev);
	if (dev != NULL){
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node){
			chr_err("%s: failed to get boot mode phandle\n", __func__);
			return;
		}
		else {
			tag = (struct tag_bootmode *)of_get_property(boot_node,
								"atag,boot", NULL);
			if (!tag){
				chr_err("%s: failed to get atag,boot\n", __func__);
				return;
			}
			else
				boot_mode = tag->bootmode;
		}
	}
	if (is_disable_charger())
		return;

	for (i = 0; i < 3; i++) {
		if (pmic_is_battery_exist() == false)
			count++;
	}

	if (count >= 3) {
		if (boot_mode == META_BOOT || boot_mode == ADVMETA_BOOT ||
		    boot_mode == ATE_FACTORY_BOOT)
			chr_info("boot_mode = %d, bypass battery check\n",
				boot_mode);
		else {
			chr_err("battery doesn't exist, shutdown\n");
			orderly_poweroff(true);
		}
	}
}

static void check_dynamic_mivr(struct charger_manager *info)
{
	int vbat = 0;

	if (info->enable_dynamic_mivr) {
		if (!mtk_pe40_get_is_connect(info) &&
			!mtk_pe20_get_is_connect(info) &&
			!mtk_pe_get_is_connect(info) &&
			!mtk_pdc_check_charger(info)) {

			vbat = battery_get_bat_voltage();
			if (vbat <
				info->data.min_charger_voltage_2 / 1000 - 200)
				charger_dev_set_mivr(info->chg1_dev,
					info->data.min_charger_voltage_2);
			else if (vbat <
				info->data.min_charger_voltage_1 / 1000 - 200)
				charger_dev_set_mivr(info->chg1_dev,
					info->data.min_charger_voltage_1);
			else
				charger_dev_set_mivr(info->chg1_dev,
					info->data.min_charger_voltage);
		}
	}
}
static void mtk_chg_get_tchg(struct charger_manager *info)
{
	int ret;
	int tchg_min = -127, tchg_max = -127;
	struct charger_data *pdata;
	bool en = false;

	pdata = &info->chg1_data;
	ret = charger_dev_get_temperature(info->chg1_dev, &tchg_min, &tchg_max);

	if (ret < 0) {
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
	} else {
		pdata->junction_temp_min = tchg_min;
		pdata->junction_temp_max = tchg_max;
	}

	if (is_slave_charger_exist()) {
		pdata = &info->chg2_data;
		ret = charger_dev_get_temperature(info->chg2_dev,
			&tchg_min, &tchg_max);

		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->dvchg1_dev) {
		pdata = &info->dvchg1_data;
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
		ret = charger_dev_is_enabled(info->dvchg1_dev, &en);
		if (ret >= 0 && en) {
			ret = charger_dev_get_adc(info->dvchg1_dev,
						  ADC_CHANNEL_TEMP_JC,
						  &tchg_min, &tchg_max);
			if (ret >= 0) {
				pdata->junction_temp_min = tchg_min;
				pdata->junction_temp_max = tchg_max;
			}
		}
	}

	if (info->dvchg2_dev) {
		pdata = &info->dvchg2_data;
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
		ret = charger_dev_is_enabled(info->dvchg2_dev, &en);
		if (ret >= 0 && en) {
			ret = charger_dev_get_adc(info->dvchg2_dev,
						  ADC_CHANNEL_TEMP_JC,
						  &tchg_min, &tchg_max);
			if (ret >= 0) {
				pdata->junction_temp_min = tchg_min;
				pdata->junction_temp_max = tchg_max;
			}
		}
	}
}

static void charger_check_status(struct charger_manager *info)
{
	bool charging = true;
	int temperature = 0;
	struct battery_thermal_protection_data *thermal = NULL;

	if (mt_get_charger_type() == CHARGER_UNKNOWN)
		return;

	temperature = info->battery_temp;
	thermal = &info->thermal;

	if (info->enable_sw_jeita == true) {
		do_sw_jeita_state_machine(info);
		if (info->sw_jeita.charging == false) {
			charging = false;
			goto stop_charging;
		}
	} else {

		if (thermal->enable_min_charge_temp) {
			if (temperature < thermal->min_charge_temp) {
				chr_err("Battery Under Temperature or NTC fail %d %d\n",
					temperature, thermal->min_charge_temp);
				thermal->sm = BAT_TEMP_LOW;
				charging = false;
				goto stop_charging;
			} else if (thermal->sm == BAT_TEMP_LOW) {
				if (temperature >=
				    thermal->min_charge_temp_plus_x_degree) {
					chr_err("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
					thermal->min_charge_temp,
					temperature,
					thermal->min_charge_temp_plus_x_degree);
					thermal->sm = BAT_TEMP_NORMAL;
				} else {
					charging = false;
					goto stop_charging;
				}
			}
		}

		if (temperature >= thermal->max_charge_temp) {
			chr_err("Battery over Temperature or NTC fail %d %d\n",
				temperature, thermal->max_charge_temp);
			thermal->sm = BAT_TEMP_HIGH;
			charging = false;
			goto stop_charging;
		} else if (thermal->sm == BAT_TEMP_HIGH) {
			if (temperature
			    < thermal->max_charge_temp_minus_x_degree) {
				chr_err("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
				thermal->max_charge_temp,
				temperature,
				thermal->max_charge_temp_minus_x_degree);
				thermal->sm = BAT_TEMP_NORMAL;
			} else {
				charging = false;
				goto stop_charging;
			}
		}
	}

	mtk_chg_get_tchg(info);

	if (!mtk_chg_check_vbus(info)) {
		charging = false;
		goto stop_charging;
	}

	if (info->cmd_discharging)
		charging = false;
	if (info->safety_timeout)
		charging = false;
	if (info->vbusov_stat)
		charging = false;
	if (info->sc.disable_charger == true)
		charging = false;

stop_charging:
	mtk_battery_notify_check(info);

	chr_err("tmp:%d (jeita:%d sm:%d cv:%d en:%d) (sm:%d) en:%d c:%d s:%d ov:%d sc:%d %d %d\n",
		temperature, info->enable_sw_jeita, info->sw_jeita.sm,
		info->sw_jeita.cv, info->sw_jeita.charging, thermal->sm,
		charging, info->cmd_discharging, info->safety_timeout,
		info->vbusov_stat, info->sc.disable_charger,
		info->can_charging, charging);

	if (charging != info->can_charging)
		_charger_manager_enable_charging(info->chg1_consumer,
						0, charging);

	info->can_charging = charging;
}
#endif

void smart_batt_set_diff_fv(int val)
{
	if (pinfo == NULL)
		return;
	chr_err("%s: diff_fv_val = %d\n", __func__, val);
	pinfo->diff_fv_val = val;
}
EXPORT_SYMBOL(smart_batt_set_diff_fv);

int smart_batt_get_diff_fv()
{
	if (pinfo == NULL)
		return 0;
	chr_err("%s: diff_fv_val = %d\n", __func__, pinfo->diff_fv_val);
	return pinfo->diff_fv_val;
}
EXPORT_SYMBOL(smart_batt_get_diff_fv);

void night_charging_set_status(int val)
{
	if (pinfo == NULL)
		return;
	pinfo->night_charging = !!val;
	chr_err("%s: night_charging = %d\n", __func__, pinfo->night_charging);
}
EXPORT_SYMBOL(night_charging_set_status);

int night_charging_get_status()
{
	if (pinfo == NULL)
		return 0;
	chr_err("%s: night_charging = %d\n", __func__, pinfo->night_charging);
	return pinfo->night_charging;
}
EXPORT_SYMBOL(night_charging_get_status);

void set_soft_reset_status(int val)
{
	if (pinfo == NULL)
		return;
	pinfo->pd_soft_reset = !!val;
	chr_err("%s:pd_soft_reset = %d\n", __func__, pinfo->pd_soft_reset);
}
EXPORT_SYMBOL(set_soft_reset_status);

int get_soft_reset_status()
{
	if (pinfo == NULL)
		return 0;
	return pinfo->pd_soft_reset;
}
EXPORT_SYMBOL(get_soft_reset_status);

static void kpoc_power_off_check(struct charger_manager *info)
{
	int vbus = 0;
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT
	struct timespec64 time_now;
	ktime_t ktime_now;
	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);

// workaround for mt6768 
	//int boot_mode = get_boot_mode();
	dev = &(info->pdev->dev);
	if (dev != NULL){
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node){
			chr_err("%s: failed to get boot mode phandle\n", __func__);
		}
		else {
			tag = (struct tag_bootmode *)of_get_property(boot_node,
								"atag,boot", NULL);
			if (!tag){
				chr_err("%s: failed to get atag,boot\n", __func__);
			}
			else
				boot_mode = tag->bootmode;
		}
	}

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		if (atomic_read(&info->enable_kpoc_shdn)) {
			vbus = battery_get_vbus();
			if (vbus >= 0 && vbus < 2500 && !mt_charger_plugin()  && (time_now.tv_sec > 10)) {
				chr_err("Unplug Charger/USB in KPOC mode, shutdown\n");
				chr_err("%s: system_state=%d\n", __func__,
					system_state);
				if (system_state != SYSTEM_POWER_OFF)
					kernel_power_off();
			}
		}
	}
}

#ifdef CONFIG_PM
static int charger_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec now;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pinfo->is_suspend = true;
		chr_debug("%s: enter PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		pinfo->is_suspend = false;
		chr_debug("%s: enter PM_POST_SUSPEND\n", __func__);
		get_monotonic_boottime(&now);

		if (timespec_compare(&now, &pinfo->endtime) >= 0 &&
			pinfo->endtime.tv_sec != 0 &&
			pinfo->endtime.tv_nsec != 0) {
			chr_err("%s: alarm timeout, wake up charger\n",
				__func__);
			pinfo->endtime.tv_sec = 0;
			pinfo->endtime.tv_nsec = 0;
			_wake_up_charger(pinfo);
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block charger_pm_notifier_func = {
	.notifier_call = charger_pm_event,
	.priority = 0,
};
#endif /* CONFIG_PM */

static enum alarmtimer_restart
	mtk_charger_alarm_timer_func(struct alarm *alarm, ktime_t now)
{
	struct charger_manager *info =
	container_of(alarm, struct charger_manager, charger_timer);
	unsigned long flags;

	if (info->is_suspend == false) {
		chr_err("%s: not suspend, wake up charger\n", __func__);
		_wake_up_charger(info);
	} else {
		chr_err("%s: alarm timer timeout\n", __func__);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock->active)
			__pm_stay_awake(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
	}

	return ALARMTIMER_NORESTART;
}

static void mtk_charger_start_timer(struct charger_manager *info)
{
	struct timespec time, time_now;
	ktime_t ktime;
	int ret = 0;

	/* If the timer was already set, cancel it */
	ret = alarm_try_to_cancel(&pinfo->charger_timer);
	if (ret < 0) {
		chr_err("%s: callback was running, skip timer\n", __func__);
		return;
	}

	get_monotonic_boottime(&time_now);
	time.tv_sec = info->polling_interval;
	time.tv_nsec = 0;
	info->endtime = timespec_add(time_now, time);

	ktime = ktime_set(info->endtime.tv_sec, info->endtime.tv_nsec);

	chr_err("%s: alarm timer start:%d, %ld %ld\n", __func__, ret,
		info->endtime.tv_sec, info->endtime.tv_nsec);
	alarm_start(&pinfo->charger_timer, ktime);
}

static void mtk_charger_init_timer(struct charger_manager *info)
{
	alarm_init(&info->charger_timer, ALARM_BOOTTIME,
			mtk_charger_alarm_timer_func);
	mtk_charger_start_timer(info);

#ifdef CONFIG_PM
	if (register_pm_notifier(&charger_pm_notifier_func))
		chr_err("%s: register pm failed\n", __func__);
#endif /* CONFIG_PM */
}

static int calc_delta_time(ktime_t time_last, int *delta_time)
{
	ktime_t time_now;

	time_now = ktime_get();

	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;

	return 0;
}

static void monitor_mp2762_ibat(struct charger_manager *info)
{
	int target_fcc = 0, mp2762_force_fcc = 0, delta_fcc = 0;
	static int up_force_count = 0, down_force_count = 0, last_ibat = 0;
	static ktime_t last_change_time = -1;
	int change_delta = 0, vote_icl = 0;

	if(last_change_time == -1)
	      last_change_time = ktime_get();
	calc_delta_time(last_change_time, &change_delta);
	if(change_delta < 3000)
	      return;

	vote_icl = get_effective_result(info->bbc_icl_votable);
	target_fcc = get_effective_result(info->bbc_fcc_votable);

	/*adjust policy not apply the following scenarios
	 *1.bbc not enabled
	 *2.master cp enabled
	 *3.ICL limit
	 *4.ibat > 0
	 *5.ibat far less than fcc_target
	 *6.TODO             */
	if (!info->bbc_enable || info->master_cp_enable || vote_icl < info->ibus + 300 || info->ibat >= 0 || -info->ibat + 500 < target_fcc)
	{
		if(info->is_mp2762_adjust){
			info->is_mp2762_adjust = 0;
			mp2762_force_fcc = target_fcc;
			mp2762_direct_set_fcc(info->chg1_dev, mp2762_force_fcc);
			up_force_count = 0;
			down_force_count = 0;
			last_ibat = 0;
			chr_err("[monitor_mp2762_ibat] up_count = %d, down_count = %d, target_fcc = %d, mp2762_force_fcc = %d\n", up_force_count, down_force_count, target_fcc, mp2762_force_fcc);
		}
		return;
	}

	if((last_ibat + 150 < target_fcc) && (-info->ibat + 150 < target_fcc))
	{
		up_force_count++;
		if(up_force_count >= 5){
			up_force_count = 0;
			delta_fcc = (target_fcc + info->ibat) > 300 ? 300 : (target_fcc + info->ibat);
			mp2762_force_fcc = target_fcc + delta_fcc;
			mp2762_direct_set_fcc(info->chg1_dev, mp2762_force_fcc);
		}
	}
	else if((last_ibat > target_fcc + 150) && (-info->ibat > target_fcc + 150))
	{
		down_force_count++;
		if(down_force_count >= 5){
			down_force_count = 0;
			delta_fcc = (target_fcc + info->ibat) < -300 ? -300 : (target_fcc + info->ibat);
			mp2762_force_fcc = target_fcc + delta_fcc;
			mp2762_direct_set_fcc(info->chg1_dev, mp2762_force_fcc);
		}
	}else{
		up_force_count = 0;
		down_force_count = 0;
	}

	info->is_mp2762_adjust = 1;
	last_ibat = -info->ibat;
	last_change_time = ktime_get();
	chr_err("[monitor_mp2762_ibat] up_count = %d, down_count = %d, target_fcc = %d, mp2762_force_fcc = %d\n", up_force_count, down_force_count, target_fcc, mp2762_force_fcc);
}

static int charger_routine_thread(void *arg)
{
	struct charger_manager *info = arg;
	unsigned long flags = 0;
	//bool is_charger_on = false;
	int ret;
	init_charger_hw_limit(info);

	while (1) {
		ret = wait_event_interruptible(info->wait_que,
			(info->charger_thread_timeout == true));
		if (ret < 0) {
			chr_err("%s: wait event been interrupted(%d)\n", __func__, ret);
			continue;
		}

		mutex_lock(&info->charger_lock);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock->active)
			__pm_stay_awake(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);

		info->charger_thread_timeout = false;

		check_charge_data(info);
		check_battery_connector(info);

		if (info->charger_status == CHARGER_PLUGIN)
			mtk_charger_plug_in(info);
		else if (info->charger_status == CHARGER_PLUGOUT)
			mtk_charger_plug_out(info);

		if (info->charger_thread_polling == true)
			mtk_charger_start_timer(info);

		update_real_type_data(info);
		if (info->chr_type != CHARGER_UNKNOWN) {
			//check_charge_parameters(info);
			check_vinlim(info);
			check_cv_vbus(info);
		}

		/*charger_update_data(info);
		check_battery_exist(info);
		check_dynamic_mivr(info);
		charger_check_status(info);*/
		kpoc_power_off_check(info);
		if(product_name == RUBYPLUS)
		      monitor_mp2762_ibat(info);

		/*if (is_disable_charger() == false) {
			if (is_charger_on == true) {
				if (info->do_algorithm)
					info->do_algorithm(info);
				wakeup_sc_algo_cmd(&pinfo->sc.data, SC_EVENT_CHARGING, 0);
			} else
				wakeup_sc_algo_cmd(&pinfo->sc.data, SC_EVENT_STOP_CHARGING, 0);
		} else
			chr_debug("disable charging\n");*/

		spin_lock_irqsave(&info->slock, flags);
		__pm_relax(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		chr_debug("%s end , %d\n",
			__func__, info->charger_thread_timeout);
		mutex_unlock(&info->charger_lock);
	}

	return 0;
}

static int mtk_charger_parse_dt(struct charger_manager *info,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	chr_err("%s: starts\n", __func__);

	if (!np) {
		chr_err("%s: no device node\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "fv", &info->fv);
	if (ret)
		chr_err("failed to parse fv\n");

	ret = of_property_read_u32(np, "fv_ffc", &info->fv_ffc);
	if (ret)
		chr_err("failed to parse fv_ffc\n");

	ret = of_property_read_u32(np, "fv_ffc_delta", &info->fv_ffc_delta);
	if (ret){
		chr_err("failed to parse fv_ffc_delta\n");
		info->fv_ffc_delta = 0;
	}

	ret = of_property_read_u32(np, "fv_ffc_large_cycle", &info->fv_ffc_large_cycle);
	if (ret){
		info->fv_ffc_large_cycle = info->fv_ffc;
		chr_err("failed to parse fv_ffc_large_cycle, use fv_ffc\n");
	}

	ret = of_property_read_u32(np, "iterm", &info->iterm);
	if (ret)
		chr_err("failed to parse iterm\n");

	ret = of_property_read_u32(np, "iterm_ffc", &info->iterm_ffc);
	if (ret)
		chr_err("failed to parse iterm_ffc\n");

	ret = of_property_read_u32(np, "iterm_ffc_warm", &info->iterm_ffc_warm);
	if (ret) {
		info->iterm_ffc_warm = info->iterm_ffc;
		chr_err("failed to parse iterm_ffc_warm, use iterm_ffc\n");
	}

	ret = of_property_read_u32(np, "max_fcc", &info->max_fcc);
	if (ret)
		chr_err("failed to parse max_fcc\n");

	ret = of_property_read_u32(np, "max_ibus", &info->max_ibus);
	if (ret)
		chr_err("failed to parse max_ibus\n");

	ret = of_property_read_u32(np, "ffc_low_tbat", &info->ffc_low_tbat);
	if (ret)
		chr_err("failed to parse ffc_low_tbat\n");

	ret = of_property_read_u32(np, "ffc_medium_tbat", &info->ffc_medium_tbat);
	if (ret)
		chr_err("failed to parse ffc_medium_tbat\n");

	ret = of_property_read_u32(np, "ffc_high_tbat", &info->ffc_high_tbat);
	if (ret)
		chr_err("failed to parse ffc_high_tbat\n");

	ret = of_property_read_u32(np, "ffc_high_soc", &info->ffc_high_soc);
	if (ret)
		chr_err("failed to parse ffc_high_soc\n");

	chr_info("parse fv = %d, fv_ffc_large_cycle = %d, fv_ffc = %d, fv_ffc_delta = %d, max_fcc = %d, max_ibus = %d, ffc_low_tbat = %d, ffc_high_tbat = %d, ffc_high_soc = %d\n",
		info->fv, info->fv_ffc_large_cycle, info->fv_ffc, info->fv_ffc_delta, info->max_fcc, info->max_ibus, info->ffc_low_tbat, info->ffc_high_tbat, info->ffc_high_soc);

	if (of_property_read_string(np, "algorithm_name",
		&info->algorithm_name) < 0) {
		chr_err("%s: no algorithm_name name\n", __func__);
		info->algorithm_name = "SwitchCharging";
	}

	if (strcmp(info->algorithm_name, "SwitchCharging2") == 0) {
		chr_err("found SwitchCharging2\n");
		mtk_switch_charging_init(info);
	}

	return 0;
}


static ssize_t show_Pump_Express(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;
	int is_ta_detected = 0;

	pr_debug("[%s] chr_type:%d UISOC:%d startsoc:%d stopsoc:%d\n", __func__,
		mt_get_charger_type(), battery_get_uisoc(),
		pinfo->data.ta_start_battery_soc,
		pinfo->data.ta_stop_battery_soc);

	if (IS_ENABLED(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT)) {
		/* Is PE+20 connect */
		if (mtk_pe20_get_is_connect(pinfo))
			is_ta_detected = 1;
	}

	if (IS_ENABLED(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)) {
		/* Is PE+ connect */
		if (mtk_pe_get_is_connect(pinfo))
			is_ta_detected = 1;
	}

	if (mtk_is_TA_support_pd_pps(pinfo) == true || pinfo->is_pdc_run == true)
		is_ta_detected = 1;

	pr_debug("%s: detected = %d, pe20_connect = %d, pe_connect = %d\n",
		__func__, is_ta_detected,
		mtk_pe20_get_is_connect(pinfo),
		mtk_pe_get_is_connect(pinfo));

	return sprintf(buf, "%u\n", is_ta_detected);
}

static DEVICE_ATTR(Pump_Express, 0444, show_Pump_Express, NULL);

static ssize_t show_input_current(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] %s : %x\n",
		__func__, pinfo->chg1_data.thermal_input_current_limit);
	return sprintf(buf, "%u\n",
			pinfo->chg1_data.thermal_input_current_limit);
}

static ssize_t store_input_current(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret;

	pr_debug("[Battery] %s\n", __func__);
	if (buf != NULL && size != 0) {
		pr_debug("[Battery] buf is %s and size is %zu\n", buf, size);
		ret = kstrtouint(buf, 16, &reg);
		pinfo->chg1_data.thermal_input_current_limit = reg;
		if (pinfo->data.parallel_vbus)
			pinfo->chg2_data.thermal_input_current_limit = reg;
		pr_debug("[Battery] %s: %x\n",
			__func__, pinfo->chg1_data.thermal_input_current_limit);
	}
	return size;
}
static DEVICE_ATTR(input_current, 0644, show_input_current,
		store_input_current);

static ssize_t show_chg1_current(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] %s : %x\n",
		__func__, pinfo->chg1_data.thermal_charging_current_limit);
	return sprintf(buf, "%u\n",
			pinfo->chg1_data.thermal_charging_current_limit);
}

static ssize_t store_chg1_current(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret;

	pr_debug("[Battery] %s\n", __func__);
	if (buf != NULL && size != 0) {
		pr_debug("[Battery] buf is %s and size is %zu\n", buf, size);
		ret = kstrtouint(buf, 16, &reg);
		pinfo->chg1_data.thermal_charging_current_limit = reg;
		pr_debug("[Battery] %s: %x\n", __func__,
			pinfo->chg1_data.thermal_charging_current_limit);
	}
	return size;
}
static DEVICE_ATTR(chg1_current, 0644, show_chg1_current, store_chg1_current);

static ssize_t show_chg2_current(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] %s : %x\n",
		__func__, pinfo->chg2_data.thermal_charging_current_limit);
	return sprintf(buf, "%u\n",
			pinfo->chg2_data.thermal_charging_current_limit);
}

static ssize_t store_chg2_current(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret;

	pr_debug("[Battery] %s\n", __func__);
	if (buf != NULL && size != 0) {
		pr_debug("[Battery] buf is %s and size is %zu\n", buf, size);
		ret = kstrtouint(buf, 16, &reg);
		pinfo->chg2_data.thermal_charging_current_limit = reg;
		pr_debug("[Battery] %s: %x\n", __func__,
			pinfo->chg2_data.thermal_charging_current_limit);
	}
	return size;
}
static DEVICE_ATTR(chg2_current, 0644, show_chg2_current, store_chg2_current);

static ssize_t show_BatNotify(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] show_BatteryNotify: 0x%x\n", pinfo->notify_code);

	return sprintf(buf, "%u\n", pinfo->notify_code);
}

static ssize_t store_BatNotify(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret;

	pr_debug("[Battery] store_BatteryNotify\n");
	if (buf != NULL && size != 0) {
		pr_debug("[Battery] buf is %s and size is %zu\n", buf, size);
		ret = kstrtouint(buf, 16, &reg);
		pinfo->notify_code = reg;
		pr_debug("[Battery] store code: 0x%x\n", pinfo->notify_code);
		mtk_chgstat_notify(pinfo);
	}
	return size;
}

static DEVICE_ATTR(BatteryNotify, 0644, show_BatNotify, store_BatNotify);

static ssize_t show_BN_TestMode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] %s : %x\n", __func__, pinfo->notify_test_mode);
	return sprintf(buf, "%u\n", pinfo->notify_test_mode);
}

static ssize_t store_BN_TestMode(struct device *dev,
		struct device_attribute *attr, const char *buf,  size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret;

	pr_debug("[Battery] %s\n", __func__);
	if (buf != NULL && size != 0) {
		pr_debug("[Battery] buf is %s and size is %zu\n", buf, size);
		ret = kstrtouint(buf, 16, &reg);
		pinfo->notify_test_mode = reg;
		pr_debug("[Battery] store mode: %x\n", pinfo->notify_test_mode);
	}
	return size;
}
static DEVICE_ATTR(BN_TestMode, 0644, show_BN_TestMode, store_BN_TestMode);

static ssize_t show_ADC_Charger_Voltage(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int vbus = battery_get_vbus();

	if (!atomic_read(&pinfo->enable_kpoc_shdn) || vbus < 0) {
		chr_err("HardReset or get vbus failed, vbus:%d:5000\n", vbus);
		vbus = 5000;
	}

	pr_debug("[%s]: %d\n", __func__, vbus);
	return sprintf(buf, "%d\n", vbus);
}

static DEVICE_ATTR(ADC_Charger_Voltage, 0444, show_ADC_Charger_Voltage, NULL);

static ssize_t show_thermal_limit(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 tmpbuf[150];
	int thermal_level = 0, len = 0, idx = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "THERMAL_LIMIT");
	for (thermal_level = 0; thermal_level <= 15; thermal_level++) {
		len = snprintf(tmpbuf, PAGE_SIZE - idx, "level[%d] = %d\n", thermal_level, pinfo->thermal_limit[5][thermal_level]);
		memcpy(&buf[idx], tmpbuf, len);
		idx += len;
	}

	return idx;
}

static DEVICE_ATTR(thermal_limit, 0444, show_thermal_limit, NULL);
/* procfs */
static int mtk_chg_current_cmd_show(struct seq_file *m, void *data)
{
	struct charger_manager *pinfo = m->private;

	seq_printf(m, "%d %d\n", pinfo->usb_unlimited, pinfo->cmd_discharging);
	return 0;
}

static ssize_t mtk_chg_current_cmd_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[32] = {0};
	int current_unlimited = 0;
	int cmd_discharging = 0;
	struct charger_manager *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &current_unlimited, &cmd_discharging) == 2) {
		info->usb_unlimited = current_unlimited;
		if (cmd_discharging == 1) {
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_manager_notifier(info,
						CHARGER_NOTIFY_STOP_CHARGING);
		} else if (cmd_discharging == 0) {
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_manager_notifier(info,
						CHARGER_NOTIFY_START_CHARGING);
		}

		pr_debug("%s current_unlimited=%d, cmd_discharging=%d\n",
			__func__, current_unlimited, cmd_discharging);
		return count;
	}

	chr_err("bad argument, echo [usb_unlimited] [disable] > current_cmd\n");
	return count;
}

static int mtk_chg_en_power_path_show(struct seq_file *m, void *data)
{
	struct charger_manager *pinfo = m->private;
	bool power_path_en = true;

	charger_dev_is_powerpath_enabled(pinfo->chg1_dev, &power_path_en);
	seq_printf(m, "%d\n", power_path_en);

	return 0;
}

static ssize_t mtk_chg_en_power_path_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int enable = 0;
	struct charger_manager *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_powerpath(info->chg1_dev, enable);
		pr_debug("%s: enable power path = %d\n", __func__, enable);
		return count;
	}

	chr_err("bad argument, echo [enable] > en_power_path\n");
	return count;
}

static int mtk_chg_en_safety_timer_show(struct seq_file *m, void *data)
{
	struct charger_manager *pinfo = m->private;
	bool safety_timer_en = false;

	charger_dev_is_safety_timer_enabled(pinfo->chg1_dev, &safety_timer_en);
	seq_printf(m, "%d\n", safety_timer_en);

	return 0;
}

static ssize_t mtk_chg_en_safety_timer_write(struct file *file,
	const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int enable = 0;
	struct charger_manager *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_safety_timer(info->chg1_dev, enable);
		pr_debug("%s: enable safety timer = %d\n", __func__, enable);

		/* SW safety timer */
		if (info->sw_safety_timer_setting == true) {
			if (enable)
				info->enable_sw_safety_timer = true;
			else
				info->enable_sw_safety_timer = false;
		}

		return count;
	}

	chr_err("bad argument, echo [enable] > en_safety_timer\n");
	return count;
}

/* PROC_FOPS_RW(battery_cmd); */
/* PROC_FOPS_RW(discharging_cmd); */
PROC_FOPS_RW(current_cmd);
PROC_FOPS_RW(en_power_path);
PROC_FOPS_RW(en_safety_timer);

/* Create sysfs and procfs attributes */
static int mtk_charger_setup_files(struct platform_device *pdev)
{
	int ret = 0;
	struct proc_dir_entry *battery_dir = NULL;
	struct charger_manager *info = platform_get_drvdata(pdev);
	/* struct charger_device *chg_dev; */

	ret = device_create_file(&(pdev->dev), &dev_attr_sw_jeita);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_pe20);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_pe40);
	if (ret)
		goto _out;

	/* Battery warning */
	ret = device_create_file(&(pdev->dev), &dev_attr_BatteryNotify);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_BN_TestMode);
	if (ret)
		goto _out;
	/* Pump express */
	ret = device_create_file(&(pdev->dev), &dev_attr_Pump_Express);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_charger_log_level);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_pdc_max_watt);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_ADC_Charger_Voltage);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_input_current);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_chg1_current);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_chg2_current);
	ret = device_create_file(&(pdev->dev), &dev_attr_thermal_limit);
	if (ret)
		goto _out;

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		chr_err("[%s]: mkdir /proc/mtk_battery_cmd failed\n", __func__);
		return -ENOMEM;
	}

	proc_create_data("current_cmd", 0640, battery_dir,
			&mtk_chg_current_cmd_fops, info);
	proc_create_data("en_power_path", 0640, battery_dir,
			&mtk_chg_en_power_path_fops, info);
	proc_create_data("en_safety_timer", 0640, battery_dir,
			&mtk_chg_en_safety_timer_fops, info);

_out:
	return ret;
}

void notify_adapter_event(enum adapter_type type, enum adapter_event evt,
	void *val)
{
	if (!check_usb_psy())
		return;

	chr_err("%s %d %d\n", __func__, type, evt);
	switch (type) {
	case MTK_PD_ADAPTER:
		switch (evt) {
		case MTK_PD_CONNECT_NONE:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify Detach\n");
			pinfo->pd_type = MTK_PD_CONNECT_NONE;
			pinfo->pd_adapter->vdm_data.current_auth = 0;
			mutex_unlock(&pinfo->charger_pd_lock);
			charger_dev_update_chgtype(pinfo->chg2_dev, XMUSB350_TYPE_UNKNOW);
			charger_dev_cp_reset_check(pinfo->cp_master);
			charger_dev_cp_reset_check(pinfo->cp_slave);
			/* reset PE40 */
			break;

		case MTK_PD_CONNECT_HARD_RESET:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify HardReset\n");
			pinfo->pd_type = MTK_PD_CONNECT_NONE;
			pinfo->pd_reset = true;
			mutex_unlock(&pinfo->charger_pd_lock);
			_wake_up_charger(pinfo);
			/* reset PE40 */
			break;

		case MTK_PD_CONNECT_PE_READY_SNK:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify fixe voltage ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK;
			mutex_unlock(&pinfo->charger_pd_lock);
			/* PD is ready */
			charger_dev_update_chgtype(pinfo->chg2_dev, XMUSB350_TYPE_PD);
			break;

		case MTK_PD_CONNECT_PE_READY_SNK_PD30:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify PD30 ready\r\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
			mutex_unlock(&pinfo->charger_pd_lock);
			/* PD30 is ready */
			charger_dev_update_chgtype(pinfo->chg2_dev, XMUSB350_TYPE_PD);
			_wake_up_charger(pinfo);
			break;

		case MTK_PD_CONNECT_PE_READY_SNK_APDO:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify APDO Ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
			mutex_unlock(&pinfo->charger_pd_lock);
			//charger_dev_update_chgtype(pinfo->chg2_dev, XMUSB350_TYPE_PD);
			/* PE40 is ready */
			_wake_up_charger(pinfo);
			break;

		case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify Type-C Ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
			mutex_unlock(&pinfo->charger_pd_lock);
			/* type C is ready */
			_wake_up_charger(pinfo);
			break;
		case MTK_TYPEC_WD_STATUS:
			chr_err("wd status = %d\n", *(bool *)val);
			mutex_lock(&pinfo->charger_pd_lock);
			pinfo->water_detected = *(bool *)val;
			mutex_unlock(&pinfo->charger_pd_lock);

			if (pinfo->water_detected == true)
				pinfo->notify_code |= CHG_TYPEC_WD_STATUS;
			else
				pinfo->notify_code &= ~CHG_TYPEC_WD_STATUS;
			mtk_chgstat_notify(pinfo);
			break;
		case MTK_TYPEC_HRESET_STATUS:
			chr_err("hreset status = %d\n", *(bool *)val);
			mutex_lock(&pinfo->charger_pd_lock);
			if (*(bool *)val)
				atomic_set(&pinfo->enable_kpoc_shdn, 1);
			else
				atomic_set(&pinfo->enable_kpoc_shdn, 0);
			mutex_unlock(&pinfo->charger_pd_lock);
			break;
		case MTK_PD_UVDM:
			mutex_lock(&pinfo->charger_pd_lock);
			usbpd_mi_vdm_received_cb(*(struct tcp_ny_uvdm *)val);
			mutex_unlock(&pinfo->charger_pd_lock);
			break;
		case MTK_PD_CONNECT_SOFT_RESET:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify SoftReset\n");
			pinfo->pd_soft_reset = true;
			mutex_unlock(&pinfo->charger_pd_lock);
			break;
		};
	}
	//mtk_pe50_notifier_call(pinfo, MTK_PE50_NOTISRC_TCP, evt, val);
}

static int proc_dump_log_show(struct seq_file *m, void *v)
{
	struct adapter_power_cap cap;
	int i;

	cap.nr = 0;
	cap.pdp = 0;
	for (i = 0; i < ADAPTER_CAP_MAX_NR; i++) {
		cap.max_mv[i] = 0;
		cap.min_mv[i] = 0;
		cap.ma[i] = 0;
		cap.type[i] = 0;
		cap.pwr_limit[i] = 0;
	}

	if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		seq_puts(m, "********** PD APDO cap Dump **********\n");

		adapter_dev_get_cap(pinfo->pd_adapter, MTK_PD_APDO, &cap);
		for (i = 0; i < cap.nr; i++) {
			seq_printf(m,
			"%d: mV:%d,%d mA:%d type:%d pwr_limit:%d pdp:%d\n", i,
			cap.max_mv[i], cap.min_mv[i], cap.ma[i],
			cap.type[i], cap.pwr_limit[i], cap.pdp);
		}
	} else if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK
		|| pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) {
		seq_puts(m, "********** PD cap Dump **********\n");

		adapter_dev_get_cap(pinfo->pd_adapter, MTK_PD, &cap);
		for (i = 0; i < cap.nr; i++) {
			seq_printf(m, "%d: mV:%d,%d mA:%d type:%d\n", i,
			cap.max_mv[i], cap.min_mv[i], cap.ma[i], cap.type[i]);
		}
	}

	return 0;
}

static ssize_t proc_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *f_pos)
{
	return count;
}


static int proc_dump_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_dump_log_show, NULL);
}

static const struct file_operations charger_dump_log_proc_fops = {
	.open = proc_dump_log_open,
	.read = seq_read,
	.llseek	= seq_lseek,
	.write = proc_write,
};

void charger_debug_init(void)
{
	struct proc_dir_entry *charger_dir;

	charger_dir = proc_mkdir("charger", NULL);
	if (!charger_dir) {
		chr_err("fail to mkdir /proc/charger\n");
		return;
	}

	proc_create("dump_log", 0640,
		charger_dir, &charger_dump_log_proc_fops);
}

void scd_ctrl_cmd_from_user(void *nl_data, struct sc_nl_msg_t *ret_msg)
{
	struct sc_nl_msg_t *msg;

	msg = nl_data;
	ret_msg->sc_cmd = msg->sc_cmd;

	switch (msg->sc_cmd) {

	case SC_DAEMON_CMD_PRINT_LOG:
		{
			chr_err("%s", &msg->sc_data[0]);
		}
	break;

	case SC_DAEMON_CMD_SET_DAEMON_PID:
		{
			memcpy(&pinfo->sc.g_scd_pid, &msg->sc_data[0],
				sizeof(pinfo->sc.g_scd_pid));
			chr_err("[fr] SC_DAEMON_CMD_SET_DAEMON_PID = %d(first launch)\n",
				pinfo->sc.g_scd_pid);
		}
	break;

	case SC_DAEMON_CMD_SETTING:
		{
			struct scd_cmd_param_t_1 data;

			memcpy(&data, &msg->sc_data[0],
				sizeof(struct scd_cmd_param_t_1));

			chr_debug("rcv data:%d %d %d %d %d %d %d %d %d %d %d %d %d %d Ans:%d\n",
				data.data[0],
				data.data[1],
				data.data[2],
				data.data[3],
				data.data[4],
				data.data[5],
				data.data[6],
				data.data[7],
				data.data[8],
				data.data[9],
				data.data[10],
				data.data[11],
				data.data[12],
				data.data[13],
				data.data[14]);

			pinfo->sc.solution = data.data[SC_SOLUTION];
			if (data.data[SC_SOLUTION] == SC_DISABLE)
				pinfo->sc.disable_charger = true;
			else if (data.data[SC_SOLUTION] == SC_REDUCE)
				pinfo->sc.disable_charger = false;
			else
				pinfo->sc.disable_charger = false;
		}
	break;
	default:
		chr_err("bad sc_DAEMON_CTRL_CMD_FROM_USER 0x%x\n", msg->sc_cmd);
		break;
	}

}

static void sc_nl_send_to_user(u32 pid, int seq, struct sc_nl_msg_t *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	/* int size=sizeof(struct fgd_nl_msg_t); */
	int size = reply_msg->sc_data_len + SCD_NL_MSG_T_HDR_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	reply_msg->identity = SCD_NL_MAGIC;

	if (in_interrupt())
		skb = alloc_skb(len, GFP_ATOMIC);
	else
		skb = alloc_skb(len, GFP_KERNEL);

	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	if (!nlh) {
		chr_err("[Netlink] nlmsg_put failed\n");
		if (skb)
			kfree_skb(skb);
		return;
	}

	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;	/* unicast */

	ret = netlink_unicast(pinfo->sc.daemo_nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		chr_err("[Netlink] sc send failed %d\n", ret);
		if (skb)
			kfree_skb(skb);
		return;
	}

}


static void chg_nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct sc_nl_msg_t *sc_msg, *sc_ret_msg;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	data = NLMSG_DATA(nlh);

	sc_msg = (struct sc_nl_msg_t *)data;

	size = sc_msg->sc_ret_data_len + SCD_NL_MSG_T_HDR_LEN;

	if (size > (PAGE_SIZE << 1))
		sc_ret_msg = vmalloc(size);
	else {
		if (in_interrupt())
			sc_ret_msg = kmalloc(size, GFP_ATOMIC);
	else
		sc_ret_msg = kmalloc(size, GFP_KERNEL);
	}

	if (sc_ret_msg == NULL) {
		if (size > PAGE_SIZE)
			sc_ret_msg = vmalloc(size);

		if (sc_ret_msg == NULL)
			return;
	}

	memset(sc_ret_msg, 0, size);

	scd_ctrl_cmd_from_user(data, sc_ret_msg);
	sc_nl_send_to_user(pid, seq, sc_ret_msg);

	kvfree(sc_ret_msg);
}

void sc_select_charging_current(struct charger_manager *info, struct charger_data *pdata)
{
	chr_err("sck: en:%d pid:%d %d %d %d %d %d thermal.dis:%d\n",
			info->sc.enable,
			info->sc.g_scd_pid,
			info->sc.pre_ibat,
			info->sc.sc_ibat,
			pdata->charging_current_limit,
			pdata->thermal_charging_current_limit,
			info->sc.solution,
			pinfo->sc.disable_in_this_plug);


	if (pinfo->sc.g_scd_pid != 0 && pinfo->sc.disable_in_this_plug == false) {
		if (info->sc.pre_ibat == -1 || info->sc.solution == SC_IGNORE
			|| info->sc.solution == SC_DISABLE) {
			info->sc.sc_ibat = -1;
		} else {
			if (info->sc.pre_ibat == pdata->charging_current_limit
				&& info->sc.solution == SC_REDUCE
				&& ((pdata->charging_current_limit - 100000) >= 500000)) {
				if (info->sc.sc_ibat == -1)
					info->sc.sc_ibat = pdata->charging_current_limit - 100000;
				else if (info->sc.sc_ibat - 100000 >= 500000)
					info->sc.sc_ibat = info->sc.sc_ibat - 100000;
			}
		}
	}
	info->sc.pre_ibat =  pdata->charging_current_limit;

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
		    pdata->charging_current_limit)
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
		pinfo->sc.disable_in_this_plug = true;
	} else if ((info->sc.solution == SC_REDUCE || info->sc.solution == SC_KEEP)
		&& info->sc.sc_ibat <
		pdata->charging_current_limit && pinfo->sc.g_scd_pid != 0 &&
		pinfo->sc.disable_in_this_plug == false && info->sc.sc_ibat != -1) {
		pdata->charging_current_limit = info->sc.sc_ibat;
	}
}

void sc_init(struct smartcharging *sc)
{
	sc->enable = false;
	sc->start_time = 0;
	sc->end_time = 80000;
	sc->target_percentage = 80;
	sc->pre_ibat = -1;
	sc->bh = 100;
	chr_err("%s: en:%d time:%d,%d tsoc:%d %d %d %d\n",
		__func__,
		sc->enable,
		sc->start_time,
		sc->end_time,
		sc->target_percentage,
		sc->battery_size,
		sc->left_time_for_cv,
		sc->current_limit);
}

void sc_update(struct charger_manager *pinfo)
{
	int time = pinfo->sc.left_time_for_cv;
	int bh = pinfo->sc.bh;

	memset(&pinfo->sc.data, 0, sizeof(struct scd_cmd_param_t_1));
	pinfo->sc.data.data[SC_VBAT] = battery_get_bat_voltage();
	pinfo->sc.data.data[SC_BAT_TMP] = battery_get_bat_temperature();
	pinfo->sc.data.data[SC_UISOC] = battery_get_uisoc();
	pinfo->sc.data.data[SC_SOC] = battery_get_soc();

	if (bh <= 80) {
		pinfo->sc.enable = false;
		chr_err("battery health(%d) is too low to enable sc\n", bh);
	}
	pinfo->sc.data.data[SC_ENABLE] = pinfo->sc.enable;
	pinfo->sc.data.data[SC_BAT_SIZE] = pinfo->sc.battery_size;
	pinfo->sc.data.data[SC_START_TIME] = pinfo->sc.start_time;
	pinfo->sc.data.data[SC_END_TIME] = pinfo->sc.end_time;
	pinfo->sc.data.data[SC_IBAT_LIMIT] = pinfo->sc.current_limit;
	pinfo->sc.data.data[SC_TARGET_PERCENTAGE] = pinfo->sc.target_percentage;

	if (bh <= 90) {
		chr_err("battery health(%d) is low ,time from %d => %d\n",
			bh, time, time * 3 / 2);
		time = time * 3 / 2;
	}

	pinfo->sc.data.data[SC_LEFT_TIME_FOR_CV] = time;

	charger_dev_get_charging_current(pinfo->chg1_dev, &pinfo->sc.data.data[SC_IBAT_SETTING]);
	pinfo->sc.data.data[SC_IBAT_SETTING] = pinfo->sc.data.data[SC_IBAT_SETTING] / 1000;
	pinfo->sc.data.data[SC_IBAT] = battery_get_bat_current() / 10;
	charger_dev_get_ibus(pinfo->chg1_dev, &pinfo->sc.data.data[SC_IBUS]);
	if (chargerlog_level == 1)
		pinfo->sc.data.data[SC_DBGLV] = 3;
	else
		pinfo->sc.data.data[SC_DBGLV] = 7;

}

int wakeup_sc_algo_cmd(struct scd_cmd_param_t_1 *data, int subcmd, int para1)
{

	if (pinfo->sc.g_scd_pid != 0) {
		struct sc_nl_msg_t *sc_msg;
		int size = SCD_NL_MSG_T_HDR_LEN + sizeof(struct scd_cmd_param_t_1);

		if (size > (PAGE_SIZE << 1))
			sc_msg = vmalloc(size);
		else {
			if (in_interrupt())
				sc_msg = kmalloc(size, GFP_ATOMIC);
		else
			sc_msg = kmalloc(size, GFP_KERNEL);

		}

		if (sc_msg == NULL) {
			if (size > PAGE_SIZE)
				sc_msg = vmalloc(size);

			if (sc_msg == NULL)
				return -1;
		}

		sc_update(pinfo);

		chr_debug(
			"[wakeup_fg_algo] malloc size=%d pid=%d\n",
			size, pinfo->sc.g_scd_pid);
		memset(sc_msg, 0, size);
		sc_msg->sc_cmd = SC_DAEMON_CMD_NOTIFY_DAEMON;
		sc_msg->sc_subcmd = subcmd;
		sc_msg->sc_subcmd_para1 = para1;
		memcpy(sc_msg->sc_data, data, sizeof(struct scd_cmd_param_t_1));
		sc_msg->sc_data_len += sizeof(struct scd_cmd_param_t_1);
		sc_nl_send_to_user(pinfo->sc.g_scd_pid, 0, sc_msg);

		kvfree(sc_msg);

		return 0;
	}
	chr_debug("pid is NULL\n");
	return -1;
}

static ssize_t show_sc_en(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{

	chr_err(
	"[enable smartcharging] : %d\n",
	pinfo->sc.enable);

	return sprintf(buf, "%d\n", pinfo->sc.enable);
}

static ssize_t store_sc_en(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	if (buf != NULL && size != 0) {
		chr_err("[enable smartcharging] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[enable smartcharging] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val == 0)
			pinfo->sc.enable = false;
		else
			pinfo->sc.enable = true;

		chr_err(
			"[enable smartcharging]enable smartcharging=%d\n",
			pinfo->sc.enable);
	}
	return size;
}
static DEVICE_ATTR(enable_sc, 0664,
	show_sc_en, store_sc_en);

static ssize_t show_sc_stime(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{

	chr_err(
	"[smartcharging stime] : %d\n",
	pinfo->sc.start_time);

	return sprintf(buf, "%d\n", pinfo->sc.start_time);
}

static ssize_t store_sc_stime(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging stime] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging stime] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0)
			pinfo->sc.start_time = val;

		chr_err(
			"[smartcharging stime]enable smartcharging=%d\n",
			pinfo->sc.start_time);
	}
	return size;
}
static DEVICE_ATTR(sc_stime, 0664,
	show_sc_stime, store_sc_stime);

static ssize_t show_sc_etime(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{

	chr_err(
	"[smartcharging etime] : %d\n",
	pinfo->sc.end_time);

	return sprintf(buf, "%d\n", pinfo->sc.end_time);
}

static ssize_t store_sc_etime(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging etime] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging etime] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0)
			pinfo->sc.end_time = val;

		chr_err(
			"[smartcharging stime]enable smartcharging=%d\n",
			pinfo->sc.end_time);
	}
	return size;
}
static DEVICE_ATTR(sc_etime, 0664,
	show_sc_etime, store_sc_etime);

static ssize_t show_sc_tuisoc(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{

	chr_err(
	"[smartcharging target uisoc] : %d\n",
	pinfo->sc.target_percentage);

	return sprintf(buf, "%d\n", pinfo->sc.target_percentage);
}

static ssize_t store_sc_tuisoc(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging tuisoc] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging tuisoc] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0)
			pinfo->sc.target_percentage = val;

		chr_err(
			"[smartcharging stime]tuisoc=%d\n",
			pinfo->sc.target_percentage);
	}
	return size;
}
static DEVICE_ATTR(sc_tuisoc, 0664,
	show_sc_tuisoc, store_sc_tuisoc);

static ssize_t show_sc_ibat_limit(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{

	chr_err(
	"[smartcharging ibat limit] : %d\n",
	pinfo->sc.current_limit);

	return sprintf(buf, "%d\n", pinfo->sc.current_limit);
}

static ssize_t store_sc_ibat_limit(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging ibat limit] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging ibat limit] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0)
			pinfo->sc.current_limit = val;

		chr_err(
			"[smartcharging ibat limit]=%d\n",
			pinfo->sc.current_limit);
	}
	return size;
}
static DEVICE_ATTR(sc_ibat_limit, 0664,
	show_sc_ibat_limit, store_sc_ibat_limit);

static ssize_t show_sc_test(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", 0);
}

static ssize_t store_sc_test(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging test] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging test] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val == 1) {
			charger_manager_enable_sc(pinfo->chg1_consumer,
				true, 1, 1111);
		} else if (val == 2) {
			charger_manager_enable_sc(pinfo->chg1_consumer,
				false, 2, 2222);
		} else if (val == 3) {
			charger_manager_set_sc_current_limit(pinfo->chg1_consumer,
				1000);
		} else {
			charger_manager_set_bh(pinfo->chg1_consumer,
				val);
		}

	}
	return size;
}
static DEVICE_ATTR(sc_test, 0664,
	show_sc_test, store_sc_test);

static void usbpd_mi_vdm_received_cb(struct tcp_ny_uvdm uvdm)
{
	int i, cmd;

	if (uvdm.uvdm_svid != USB_PD_MI_SVID) {
		pr_info("SVID don't match XIAOMI: %x\n", uvdm.uvdm_svid);
		return;
	}

	cmd = UVDM_HDR_CMD(uvdm.uvdm_data[0]);
	pr_info("cmd: %d, uvdm.ack: %d, uvdm.uvdm_cnt: %d, uvdm.uvdm_svid: 0x%04x\n", cmd, uvdm.ack, uvdm.uvdm_cnt, uvdm.uvdm_svid);

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		pinfo->pd_adapter->vdm_data.ta_version = uvdm.uvdm_data[1];
		pr_info("ta_version:%x\n", pinfo->pd_adapter->vdm_data.ta_version);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		pinfo->pd_adapter->vdm_data.ta_temp = (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		pr_info("pinfo->pd_adapter->vdm_data.ta_temp:%d\n", pinfo->pd_adapter->vdm_data.ta_temp);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		pinfo->pd_adapter->vdm_data.ta_voltage = (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		pinfo->pd_adapter->vdm_data.ta_voltage *= 1000;
		pr_info("ta_voltage:%d\n", pinfo->pd_adapter->vdm_data.ta_voltage);
		break;
	case USBPD_UVDM_SESSION_SEED:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			pinfo->pd_adapter->vdm_data.s_secert[i] = uvdm.uvdm_data[i+1];
			pr_info("usbpd s_secert uvdm.uvdm_data[%d]=0x%x", i+1, uvdm.uvdm_data[i+1]);
		}
		break;
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			pinfo->pd_adapter->vdm_data.digest[i] = uvdm.uvdm_data[i+1];
			pr_info("usbpd digest[%d]=0x%x", i+1, uvdm.uvdm_data[i+1]);
		}
		break;
	case USBPD_UVDM_REVERSE_AUTHEN:
		pinfo->pd_adapter->vdm_data.reauth = (uvdm.uvdm_data[1] & 0xFFFF);
		break;
	case USBPD_UVDM_10A_AUTHEN:
		pinfo->pd_adapter->vdm_data.current_auth = 1;
		break;
	default:
		break;
	}

	pinfo->pd_adapter->uvdm_state = cmd;
}

static void mtk_charger_parse_cmdline(void)
{
	char *ruby = NULL, *rubypro = NULL, *rubyplus = NULL;
	const char *sku = get_hw_sku();

	ruby = strnstr(sku, "ruby", strlen(sku));
	rubypro = strnstr(sku, "rubypro", strlen(sku));
	rubyplus = strnstr(sku, "rubyplus", strlen(sku));

	if (rubyplus)
		product_name = RUBYPLUS;
	else if (rubypro)
		product_name = RUBYPRO;
	else if (ruby)
		product_name = RUBY;

	chr_info("product_name = %d, ruby = %d, rubypro = %d, rubyplus = %d\n", product_name, ruby ? 1 : 0, rubypro ? 1 : 0, rubyplus ? 1 : 0);
}

static const struct platform_device_id mtk_charger_id[] = {
	{ "ruby_charger", RUBY },
	{ "rubypro_charger", RUBYPRO },
	{ "rubyplus_charger", RUBYPLUS },
	{},
};
MODULE_DEVICE_TABLE(platform, mtk_charger_id);

static const struct of_device_id mtk_charger_of_match[] = {
	{.compatible = "ruby_charger", .data = &mtk_charger_id[0], },
	{.compatible = "rubypro_charger", .data = &mtk_charger_id[1], },
	{.compatible = "rubyplus_charger", .data = &mtk_charger_id[2], },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_charger_of_match);

static int mtk_charger_probe(struct platform_device *pdev)
{
	struct charger_manager *info = NULL;
	struct list_head *pos = NULL;
	struct list_head *phead = &consumer_head;
	struct charger_consumer *ptr = NULL;
	const struct of_device_id *of_id;
	int i, ret;
	int ret_device_file;
	struct netlink_kernel_cfg cfg = {
		.input = chg_nl_data_handler,
	};
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT

	chr_err("%s: starts\n", __func__);
	mtk_charger_parse_cmdline();

	of_id = of_match_device(mtk_charger_of_match, &pdev->dev);
	pdev->id_entry = of_id->data;

	if (pdev->id_entry->driver_data == product_name) {
		chr_info("CHARGE_ALG probe start\n");
	} else {
		chr_info("driver_data and product_name not match, don't probe, %d\n", pdev->id_entry->driver_data);
		return -ENODEV;
	}

	// workaround for mt6768 
	//int boot_mode = get_boot_mode();
	dev = &(pdev->dev);
	if (dev != NULL){
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node){
			chr_err("%s: failed to get boot mode phandle\n", __func__);
		}
		else {
			tag = (struct tag_bootmode *)of_get_property(boot_node,
								"atag,boot", NULL);
			if (!tag){
				chr_err("%s: failed to get atag,boot\n", __func__);
			}
			else
				boot_mode = tag->bootmode;
		}
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	pinfo = info;

	platform_set_drvdata(pdev, info);
	info->pdev = pdev;
	info->diff_fv_val = 0;
	info->night_charging = false;

	if (product_name == RUBYPLUS){
		info->data.bc12_charger = 2; // BC12 usb xm350
	}else{
		info->data.bc12_charger = 1; // BC12 usb mt6360
	}

	mtk_charger_parse_dt(info, &pdev->dev);

	if (!mtk_check_votable(info))
		chr_err("failed to check votable\n");

	if (!mtk_check_psy(info))
		chr_err("failed to check psy\n");

	mutex_init(&info->charger_lock);
	mutex_init(&info->charger_pd_lock);
	mutex_init(&info->cable_out_lock);
	for (i = 0; i < TOTAL_CHARGER; i++) {
		mutex_init(&info->pp_lock[i]);
		info->force_disable_pp[i] = false;
		info->enable_pp[i] = true;
	}
	/*work around for mt6768*/
	atomic_set(&info->enable_kpoc_shdn, 1);
	info->charger_wakelock = wakeup_source_register(NULL, "charger suspend wakelock");
	info->attach_wakelock = wakeup_source_register(NULL, "attach_wakelock");
	info->typec_burn_wakelock = wakeup_source_register(NULL, "typec_burn_wakelock");
	spin_lock_init(&info->slock);

	ret = step_jeita_init(info, &pdev->dev, product_name);
	if (ret)
		chr_err("failed to init step_jeita\n");

	/* init thread */
	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	info->enable_dynamic_cv = true;

	info->chg1_data.thermal_charging_current_limit = -1;
	info->chg1_data.thermal_input_current_limit = -1;
	info->chg1_data.input_current_limit_by_aicl = -1;
	info->chg2_data.thermal_charging_current_limit = -1;
	info->chg2_data.thermal_input_current_limit = -1;
	info->dvchg1_data.thermal_input_current_limit = -1;
	info->dvchg2_data.thermal_input_current_limit = -1;

	info->sw_jeita.error_recovery_flag = true;
	info->sic_current = info->max_fcc;

	mtk_charger_init_timer(info);
	info->is_pdc_run = false;
	kthread_run(charger_routine_thread, info, "charger_thread");

	if (info->chg1_dev != NULL && info->do_event != NULL) {
		info->chg1_nb.notifier_call = info->do_event;
		register_charger_device_notifier(info->chg1_dev,
						&info->chg1_nb);
		charger_dev_set_drvdata(info->chg1_dev, info);
	}

	info->psy_nb.notifier_call = charger_psy_event;
	power_supply_reg_notifier(&info->psy_nb);

	srcu_init_notifier_head(&info->evt_nh);
	ret = mtk_charger_setup_files(pdev);
	if (ret)
		chr_err("Error creating sysfs interface\n");

	info->pd_adapter = get_adapter_by_name("pd_adapter");
	if (info->pd_adapter)
		chr_err("Found PD adapter [%s]\n",
			info->pd_adapter->props.alias_name);
	else
		chr_err("*** Error : can't find PD adapter ***\n");

	/*if (mtk_pe_init(info) < 0)
		info->enable_pe_plus = false;

	if (mtk_pe20_init(info) < 0)
		info->enable_pe_2 = false;

	if (mtk_pe40_init(info) == false)
		info->enable_pe_4 = false;

	if (mtk_pe50_init(info) < 0)
		info->enable_pe_5 = false;*/

	mtk_pdc_init(info);
	charger_ftm_init();
	mtk_charger_get_atm_mode(info);
	sw_jeita_state_machine_init(info);

#ifdef CONFIG_MTK_CHARGER_UNLIMITED
	info->usb_unlimited = true;
	info->enable_sw_safety_timer = false;
	charger_dev_enable_safety_timer(info->chg1_dev, false);
#endif

	info->sc.daemo_nl_sk = netlink_kernel_create(&init_net, NETLINK_CHG, &cfg);

	if (info->sc.daemo_nl_sk == NULL)
		chr_err("sc netlink_kernel_create error id:%d\n", NETLINK_CHG);
	else
		chr_err("sc_netlink_kernel_create success id:%d\n", NETLINK_CHG);
	sc_init(&info->sc);

	charger_debug_init();

	mutex_lock(&consumer_mutex);
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct charger_consumer, list);
		ptr->cm = info;
		if (ptr->pnb != NULL) {
			srcu_notifier_chain_register(&info->evt_nh, ptr->pnb);
			ptr->pnb = NULL;
		}
	}
	mutex_unlock(&consumer_mutex);

	/* sysfs node */
	ret_device_file = device_create_file(&(pdev->dev),
		&dev_attr_enable_sc);
	ret_device_file = device_create_file(&(pdev->dev),
		&dev_attr_sc_stime);
	ret_device_file = device_create_file(&(pdev->dev),
		&dev_attr_sc_etime);
	ret_device_file = device_create_file(&(pdev->dev),
		&dev_attr_sc_tuisoc);
	ret_device_file = device_create_file(&(pdev->dev),
		&dev_attr_sc_ibat_limit);
	ret_device_file = device_create_file(&(pdev->dev),
		&dev_attr_sc_test);

	info->chg1_consumer =
		charger_manager_get_by_name(&pdev->dev, "charger_port1");

	if (info->chg1_consumer != NULL &&
	    boot_mode != KERNEL_POWER_OFF_CHARGING_BOOT &&
	    boot_mode != LOW_POWER_OFF_CHARGING_BOOT)
		charger_manager_force_disable_power_path(
			info->chg1_consumer, MAIN_CHARGER, true);

	info->init_done = true;
	_wake_up_charger(info);

	return 0;
}

static int mtk_charger_remove(struct platform_device *dev)
{
	struct charger_manager *info = platform_get_drvdata(dev);

	mtk_pe50_deinit(info);
	return 0;
}

static void mtk_charger_shutdown(struct platform_device *dev)
{
	struct charger_manager *info = platform_get_drvdata(dev);
	int ret;

	if (mtk_pe20_get_is_connect(info) || mtk_pe_get_is_connect(info)) {
		if (info->chg2_dev)
			charger_dev_enable(info->chg2_dev, false);
		ret = mtk_pe20_reset_ta_vchr(info);
		if (ret == -ENOTSUPP)
			mtk_pe_reset_ta_vchr(info);
		pr_debug("%s: reset TA before shutdown\n", __func__);
	}
}

struct platform_device charger_device = {
	.name = "charger",
	.id = -1,
};

static struct platform_driver charger_driver = {
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
	.id_table = mtk_charger_id,
	.driver = {
		   .name = "charger",
		   .of_match_table = mtk_charger_of_match,
	},
};

static int __init mtk_charger_init(void)
{
	return platform_driver_register(&charger_driver);
}
late_initcall(mtk_charger_init);

static void __exit mtk_charger_exit(void)
{
	platform_driver_unregister(&charger_driver);
}
module_exit(mtk_charger_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Driver");
MODULE_LICENSE("GPL");
