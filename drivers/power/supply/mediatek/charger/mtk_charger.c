/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/mtk_boot.h>
#include <pmic.h>
#include <mtk_gauge_time_service.h>

#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"

#include <tcpm.h>

enum product_name {
	PISSARRO,
	PISSARROPRO,
};

extern int get_charge_mode(void);
static int product_name = PISSARRO;
static bool g_hwc_cn = false;
static struct charger_manager *pinfo = NULL;
static struct list_head consumer_head = LIST_HEAD_INIT(consumer_head);
static DEFINE_MUTEX(consumer_mutex);

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

	if (!pinfo->xmusb350_psy)
		pinfo->xmusb350_psy = power_supply_get_by_name("xmusb350");

	if (!pinfo->xmusb350_psy) {
		chr_err("failed to get xmusb350_psy\n");
		return false;
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

void BATTERY_SetUSBState(int usb_state_value)
{
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
int chargerlog_level = CHRLOG_INFO_LEVEL;

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
	if (!info->charger_wakelock.active)
		__pm_stay_awake(&info->charger_wakelock);
	spin_unlock_irqrestore(&info->slock, flags);
	info->charger_thread_timeout = true;
	wake_up(&info->wait_que);
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

	ret = charger_dev_is_powerpath_enabled(chg_dev, &is_en);
	if (ret < 0) {
		chr_err("%s: get is power path enabled failed\n", __func__);
		return ret;
	}
	if (is_en == en) {
		chr_err("%s: power path is already en = %d\n", __func__, is_en);
		return 0;
	}

	pr_info("%s: enable power path = %d\n", __func__, en);
	return charger_dev_enable_powerpath(chg_dev, en);
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
	/* disable mtk thermal limit */
	return 0;
}

int charger_manager_set_charging_current_limit(
	struct charger_consumer *consumer, int idx, int charging_current)
{
	/* disable mtk thermal limit */
	return 0;
}

int charger_manager_get_charger_temperature(struct charger_consumer *consumer, int idx, int *tchg_min,	int *tchg_max)
{
	int retry_count = 3, bbc_temp = 0, cp_master_temp = 0, cp_slave_temp = 0;

retry:
	if (!pinfo || !pinfo->chg1_dev || charger_dev_get_temperature(pinfo->chg1_dev, &bbc_temp, &bbc_temp))
		bbc_temp = 30;

	if (!pinfo || !pinfo->cp_master || charger_dev_get_temperature(pinfo->cp_master, &cp_master_temp, &cp_master_temp))
		cp_master_temp = 30;

	if (!pinfo || !pinfo->cp_slave || charger_dev_get_temperature(pinfo->cp_slave, &cp_slave_temp, &cp_slave_temp))
		cp_slave_temp = 30;

	if(!is_between(0, 80, bbc_temp)) {
		bbc_temp = 30;
	}

	if (retry_count && (!is_between(0, 80, cp_master_temp) || !is_between(0, 80, cp_slave_temp))) {
		retry_count--;
		msleep(50);
		goto retry;
	}

	chr_err("thermal %d %d %d\n", bbc_temp, cp_master_temp, cp_slave_temp);
	*tchg_min = *tchg_max = (bbc_temp + cp_master_temp + cp_slave_temp) / 3;

	return 0;
}

int charger_manager_force_charging_current(struct charger_consumer *consumer,
	int idx, int charging_current)
{
	/* disable mtk thermal limit */
	return 0;
}

int charger_manager_get_current_charging_type(struct charger_consumer *consumer)
{
	/* disable mtk thermal limit */
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

	return pinfo->sic_current * 1000;
}

void charger_manager_set_sic_current(int sic_current)
{
	if (pinfo == NULL || !pinfo->bbc_fcc_votable)
		return;

	if (!pinfo->sic_support || product_name != PISSARROPRO || !is_between(0, pinfo->max_fcc, sic_current))
		return;

	pinfo->sic_current = sic_current;
	vote(pinfo->bbc_fcc_votable, SIC_VOTER, true, sic_current);
}

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
}

#define MAX_NOTCHARGE_IBAT	20

enum mt6360_charge_status {
	MT6360_CHARGE_STATUS_PROGRESS = 1,
	MT6360_CHARGE_STATUS_DONE = 2,
};

enum mp2762_charge_status {
	MP2762_CHARGE_STATUS_PRECHARGE = 1,
	MP2762_CHARGE_STATUS_FASTCHARGE = 2,
	MP2762_CHARGE_STATUS_DONE = 3,
};

int charger_manager_get_charge_status(void)
{
	int charge_status = POWER_SUPPLY_STATUS_DISCHARGING;

	if(!pinfo)
		return charge_status;

	if (pinfo->psy_type) {
		if (pinfo->typec_burn || pinfo->input_suspend || pinfo->bms_i2c_error_count >= 10) {
			charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			if (((product_name == PISSARRO && (pinfo->charge_status == MT6360_CHARGE_STATUS_DONE || pinfo->battery_full))
				|| (product_name == PISSARROPRO && pinfo->charge_status == MP2762_CHARGE_STATUS_DONE))
				&& pinfo->soc == 100 && pinfo->ibat > -35 && pinfo->ibat < 35)
				charge_status = POWER_SUPPLY_STATUS_FULL;
			else
				charge_status = POWER_SUPPLY_STATUS_CHARGING;
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

int charger_manager_enable_chg_type_det(struct charger_consumer *consumer, bool en)
{
	struct charger_manager *info = consumer->cm;
	int ret = 0;

	chr_info("enable_chg_type_det, enable = %d\n", en);

	if (en) {
		if (!info->attach_wakelock.active)
			__pm_stay_awake(&info->attach_wakelock);
	} else {
		__pm_relax(&info->attach_wakelock);
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
	/* disable MTK dual charge */
	return false;
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
			info->enable_type_c == true)
		return true;

	return false;
}

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

bool mtk_is_pep_series_connect(struct charger_manager *info)
{
	/* disable MTK PE */
	return false;
}

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

static int set_default_charger_parameters(struct charger_manager *info)
{
	int ret = 0;

	vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DEFAULT_ICL);
	vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, DEFAULT_FCC);
	vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, DEFAULT_VINMIN);
	vote(info->bbc_fv_votable, FFC_VOTER, true, info->fv);
	vote(info->bbc_iterm_votable, FFC_VOTER, true, info->iterm);

	return ret;
}

static int check_charge_parameters(struct charger_manager *info)
{
	int div_rate = (product_name == PISSARROPRO ? 2 : 1);

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
		if (info->i350_type == XMUSB350_TYPE_DCP) {
			vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DCP_ICL);
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
		if (info->recheck_count >= 2) {
			if (info->strong_qc2 && info->cv_wa_count < CV_MP2762_WA_COUNT) {
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, QC2_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, QC2_FCC / div_rate);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, QC2_VINMIN);
			} else {
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, DCP_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, DCP_FCC / div_rate);
				vote(info->bbc_vinmin_votable, CHARGER_TYPE_VOTER, true, DCP_VINMIN);
				charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_5);
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
		if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && info->recheck_count < 2)
			power_supply_changed(info->usb_psy);
		break;
	default:
		chr_err("not support psy_type to check charger parameters");
	}

	return 0;
}

#define ATL_LOW_FFC_ITEM  810
#define ATL_HIGH_FFC_ITEM 890
#define LWN_LOW_FFC_ITEM  900
#define LWN_HIGH_FFC_ITEM 1000
#define MI_LOW_FFC_ITEM 700
#define MI_HIGH_FFC_ITEM 900
static int handle_ffc_charge(struct charger_manager *info)
{
	int ret = 0;
	union power_supply_propval pval = {0,};

	if(info->bms_psy){
		power_supply_get_property(info->bms_psy,POWER_SUPPLY_PROP_CHARGE_DONE,&pval);
		info->bq_charge_done = pval.intval;
	}

	if (info->entry_soc <= info->ffc_high_soc && is_between(info->ffc_low_tbat, info->ffc_high_tbat, info->tbat) &&
		(info->qc3_type == HVDCP3_27 || info->qc3_type == HVDCP35_18 || info->qc3_type == HVDCP35_27 ||
		(pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO && pinfo->apdo_max >= 33)))
		info->ffc_enable = true;
	else
		info->ffc_enable = false;

	if (is_between(info->ffc_low_tbat, info->ffc_medium_tbat, info->tbat)){
		if(!strncmp(info->device_chem, "ATL", 3) && product_name == PISSARRO){
			info->iterm_ffc = ATL_LOW_FFC_ITEM;
		}else if(!strncmp(info->device_chem, "LWN", 3) && product_name == PISSARRO){
			info->iterm_ffc = LWN_LOW_FFC_ITEM;
		}

		if(product_name == PISSARROPRO){
			info->iterm_ffc = MI_LOW_FFC_ITEM;
		}
	}

	if (is_between(info->ffc_medium_tbat, info->ffc_high_tbat, info->tbat)){
		if(!strncmp(info->device_chem, "ATL", 3) && product_name == PISSARRO){
			info->iterm_ffc = ATL_HIGH_FFC_ITEM;
		}else if(!strncmp(info->device_chem, "LWN", 3) && product_name == PISSARRO){
			info->iterm_ffc = LWN_HIGH_FFC_ITEM;
		}

		if(product_name == PISSARROPRO){
			info->iterm_ffc = MI_HIGH_FFC_ITEM;
		}
	}

	if(info->ffc_enable && product_name == PISSARRO){
		if((-info->ibat < info->iterm_ffc) && info->bq_charge_done) {
			charger_dev_enable(info->chg1_dev, false);
			info->battery_full = true;
		}else if(!info->bq_charge_done){
			charger_dev_enable(info->chg1_dev, true);
			info->battery_full = false;
		}
	}

	if (info->ffc_enable) {
		vote(info->bbc_fv_votable, FFC_VOTER, true, info->fv_ffc);
		vote(info->bbc_iterm_votable, FFC_VOTER, true, info->iterm_ffc);
		pval.intval = 1;
		power_supply_set_property(pinfo->bms_psy, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	} else {
		vote(info->bbc_fv_votable, FFC_VOTER, true, info->fv);
		vote(info->bbc_iterm_votable, FFC_VOTER, true, info->iterm);
		pval.intval = 0;
		power_supply_set_property(pinfo->bms_psy, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	}

	return ret;
}

static int check_vinlim(struct charger_manager *info)
{
	int pulse_type = 0, count = 0, ret = 0;

	if (info->recheck_count < 2) {
		return ret;
	}

	switch(info->psy_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
		if (product_name == PISSARRO) {
			if (info->strong_qc2)
				charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_9);
			if (info->strong_qc2 && info->vbus < 8000)
				info->strong_qc2--;
		} else if (product_name == PISSARROPRO) {
			if (info->strong_qc2 && info->cv_wa_count < CV_MP2762_WA_COUNT)
				charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_12);
			if (info->strong_qc2 && info->vbus < 10000)
				info->strong_qc2--;
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

	if (product_name == PISSARROPRO && count) {
		chr_info("[CHARGE_LOOP] pulse_type = %d, count = %d\n", pulse_type, count);
		charger_dev_qc3_dpdm_pulse(info->chg2_dev, pulse_type, count);
	}

	return ret;
}

static void check_cv_vbus(struct charger_manager *info)
{
	if (product_name != PISSARROPRO || info->recheck_count < 2)
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
				charger_dev_enable_powerpath(info->chg1_dev, false);
				msleep(400);
				charger_dev_select_qc_mode(info->chg2_dev, QC_MODE_QC2_5);
				charger_dev_enable_powerpath(info->chg1_dev, true);
			} else if (info->psy_type == POWER_SUPPLY_TYPE_USB_PD) {
				power_supply_changed(info->usb_psy);
			}
		}
		if (info->cv_wa_count < (2 * CV_MP2762_WA_COUNT))
			info->cv_wa_count++;
	}
}

static void check_charge_data(struct charger_manager *info)
{
	union power_supply_propval pval = {0,};
	enum power_supply_type psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
	enum charger_type chr_type = CHARGER_UNKNOWN;
	enum hvdcp3_type qc3_type = HVDCP3_NONE;
	enum xmusb350_chg_type i350_type = XMUSB350_TYPE_UNKNOW;
	int charge_mode = 0, ret = 0;
	unsigned int boot_mode = get_boot_mode();

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

	if (info->psy_type == POWER_SUPPLY_TYPE_UNKNOWN && psy_type != POWER_SUPPLY_TYPE_UNKNOWN)
		info->charger_status = CHARGER_PLUGIN;
	else if (info->psy_type != POWER_SUPPLY_TYPE_UNKNOWN && psy_type == POWER_SUPPLY_TYPE_UNKNOWN)
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
	charger_dev_get_charge_status(pinfo->chg1_dev, &info->charge_status);
	charger_dev_get_temperature(info->chg1_dev, &info->bbc_temp, &info->bbc_temp);
	charger_dev_get_temperature(info->cp_master, &info->cp_master_temp, &info->cp_master_temp);
	charger_dev_get_temperature(info->cp_slave, &info->cp_slave_temp, &info->cp_slave_temp);

	ret = power_supply_get_property(info->xmusb350_psy, POWER_SUPPLY_PROP_RECHECK_COUNT, &pval);
	if (ret)
		chr_err("failed to get recheck_count\n");
	else
		info->recheck_count = pval.intval;

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret)
		chr_err("failed to get soc\n");
	else
		info->soc = pval.intval;

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

	if (info->bat_verifed)
		vote(info->bbc_fcc_votable, BAT_VERIFY_VOTER, false, 0);
	else
		vote(info->bbc_fcc_votable, BAT_VERIFY_VOTER, true, 3000);

	charge_mode = get_charge_mode();
	if (product_name == PISSARROPRO && (charge_mode == 0 || charge_mode == 8 ||
		(g_hwc_cn && (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || boot_mode == LOW_POWER_OFF_CHARGING_BOOT))))
		info->sic_support = true;
	else
		info->sic_support = false;

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

	chr_info("[CHARGE_LOOP]OPEN TYPE = [%d %d %d %d %d], BUS = [%d %d], BAT = [%d %d %d %d %d], BBC = [%d %d], TL_SJ = [%d %d %d %d], TEMP = [%d %d %d], FFC = [%d %d], COUNT = [%d %d]\n",
		info->psy_type, info->qc3_type, info->i350_type, info->pd_type, info->pd_verifed,
		info->vbus, info->ibus,
		info->soc, info->vbat, info->ibat, info->tbat, info->cycle_count, info->bbc_enable, info->charge_status,
		info->sic_current, info->thermal_level, info->step_chg_index[0], info->jeita_chg_index[0],
		info->bbc_temp, info->cp_master_temp, info->cp_slave_temp, info->ffc_enable, charge_mode, info->cv_wa_count, info->recheck_count);

	if (info->input_suspend || info->typec_burn || !info->bat_verifed || info->bms_i2c_error_count >= 10)
		chr_err("[CHARGE_LOOP] input_suspend = %d, typec_burn = %d, bat_verifed = %d, bms_i2c_error_count = %d\n", info->input_suspend, info->typec_burn, info->bat_verifed, info->bms_i2c_error_count);
}

static int init_charger_hw_limit(struct charger_manager *info)
{
	int ret = 0;

	ret = vote(info->bbc_fv_votable, HW_LIMIT_VOTER, true, info->fv_ffc);
	if (ret)
		chr_err("failed to init FV HW_LIMIT\n");

	ret = vote(info->bbc_icl_votable, HW_LIMIT_VOTER, true, info->max_ibus);
	if (ret)
		chr_err("failed to init ICL HW_LIMIT\n");

	ret = vote(info->bbc_fcc_votable, HW_LIMIT_VOTER, true, info->max_fcc);
	if (ret)
		chr_err("failed to init FCC HW_LIMIT\n");

	return ret;
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

	if (mt_get_charger_type() == CHARGER_UNKNOWN)
		charger_manager_notifier(pinfo, CHARGER_NOTIFY_STOP_CHARGING);
	else
		charger_manager_notifier(pinfo, CHARGER_NOTIFY_START_CHARGING);

	chr_err("wake_up_charger\n");
	_wake_up_charger(pinfo);
}

static int mtk_charger_plug_in(struct charger_manager *info)
{
	union power_supply_propval pval = {0,};

	chr_info("%s\n", __func__);

	info->charger_thread_polling = true;
	info->can_charging = true;
	info->enable_dynamic_cv = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;
	info->cv_wa_count = 0;
	info->ffc_enable = false;
	info->strong_qc2 = 2;
	info->pd_verify_done = false;

	if (!info->master_cp_psy)
		info->master_cp_psy = power_supply_get_by_name("cp_master");
	if (!info->slave_cp_psy)
		info->slave_cp_psy = power_supply_get_by_name("cp_slave");
	if (info->master_cp_psy)
		power_supply_get_property(info->master_cp_psy,
				POWER_SUPPLY_PROP_LN_RESET_CHECK, &pval);
	if (info->slave_cp_psy)
		power_supply_get_property(info->slave_cp_psy,
				POWER_SUPPLY_PROP_LN_RESET_CHECK, &pval);

	reset_mi_charge_alg(info);
	charger_dev_enable_termination(info->chg1_dev, true);
	set_default_charger_parameters(info);
	vote(info->bbc_icl_votable, PDM_VOTER, false, 0);
	vote(info->bbc_icl_votable, QCM_VOTER, false, 0);
	vote(info->bbc_fcc_votable, PDM_VOTER, false, 0);
	vote(info->bbc_en_votable, BBC_ENABLE_VOTER, true, 0);
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
	info->strong_qc2 = 2;
	info->battery_full = false;

	set_default_charger_parameters(info);
	vote(info->bbc_icl_votable, PDM_VOTER, false, 0);
	vote(info->bbc_icl_votable, QCM_VOTER, false, 0);
	vote(info->bbc_fcc_votable, PDM_VOTER, false, 0);
	vote(info->bbc_en_votable, BBC_ENABLE_VOTER, false, 0);
	vote(info->bbc_vinmin_votable, PDM_VOTER, false, 0);
	vote(info->bbc_icl_votable, FFC_VOTER, false, 0);
	if (!info->typec_burn)
		cancel_delayed_work_sync(&info->charge_monitor_work);
	reset_mi_charge_alg(info);
	charger_dev_plug_out(info->chg1_dev);

	return 0;
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

static void kpoc_power_off_check(struct charger_manager *info)
{
	unsigned int boot_mode = get_boot_mode();
	int vbus = 0;

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		if (atomic_read(&info->enable_kpoc_shdn)) {
			vbus = battery_get_vbus();
			if (vbus >= 0 && vbus < 2500 && !mt_charger_plugin()) {
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
		if (!info->charger_wakelock.active)
			__pm_stay_awake(&info->charger_wakelock);
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

static int charger_routine_thread(void *arg)
{
	struct charger_manager *info = arg;
	unsigned long flags = 0;

	init_charger_hw_limit(info);

	while (1) {
		wait_event(info->wait_que,
			(info->charger_thread_timeout == true));

		mutex_lock(&info->charger_lock);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock.active)
			__pm_stay_awake(&info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);

		info->charger_thread_timeout = false;

		check_charge_data(info);

		if (info->charger_status == CHARGER_PLUGIN)
			mtk_charger_plug_in(info);
		else if (info->charger_status == CHARGER_PLUGOUT)
			mtk_charger_plug_out(info);

		if (info->charger_thread_polling == true)
			mtk_charger_start_timer(info);

		if (info->chr_type != CHARGER_UNKNOWN) {
			check_charge_parameters(info);
			handle_ffc_charge(info);
			check_vinlim(info);
			check_cv_vbus(info);
		}

		kpoc_power_off_check(info);

		spin_lock_irqsave(&info->slock, flags);
		__pm_relax(&info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
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

	ret = of_property_read_u32(np, "iterm", &info->iterm);
	if (ret)
		chr_err("failed to parse iterm\n");

	ret = of_property_read_u32(np, "iterm_ffc", &info->iterm_ffc);
	if (ret)
		chr_err("failed to parse iterm_ffc\n");

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

	chr_info("parse fv = %d, fv_ffc = %d, max_fcc = %d, max_ibus = %d, ffc_low_tbat = %d, ffc_high_tbat = %d, ffc_high_soc = %d\n",
		info->fv, info->fv_ffc, info->max_fcc, info->max_ibus, info->ffc_low_tbat, info->ffc_high_tbat, info->ffc_high_soc);

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

	ret = device_create_file(&(pdev->dev), &dev_attr_sw_jeita);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_BatteryNotify);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_BN_TestMode);
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

void notify_adapter_event(enum adapter_type type, enum adapter_event evt, void *val)
{
	union power_supply_propval pval = {0,};

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
			mutex_unlock(&pinfo->charger_pd_lock);
			pval.intval = 0;
			power_supply_set_property(pinfo->usb_psy, POWER_SUPPLY_PROP_PD_AUTHENTICATION, &pval);
			charger_dev_update_chgtype(pinfo->chg2_dev, XMUSB350_TYPE_UNKNOW);
			break;

		case MTK_PD_CONNECT_HARD_RESET:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify HardReset\n");
			pinfo->pd_type = MTK_PD_CONNECT_NONE;
			mutex_unlock(&pinfo->charger_pd_lock);
			break;

		case MTK_PD_CONNECT_PE_READY_SNK:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify fixe voltage ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK;
			mutex_unlock(&pinfo->charger_pd_lock);
			charger_dev_update_chgtype(pinfo->chg2_dev, XMUSB350_TYPE_PD);
			break;

		case MTK_PD_CONNECT_PE_READY_SNK_PD30:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify PD30 ready\r\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
			mutex_unlock(&pinfo->charger_pd_lock);
			charger_dev_update_chgtype(pinfo->chg2_dev, XMUSB350_TYPE_PD);
			break;

		case MTK_PD_CONNECT_PE_READY_SNK_APDO:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify APDO Ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
			mutex_unlock(&pinfo->charger_pd_lock);
			charger_dev_update_chgtype(pinfo->chg2_dev, XMUSB350_TYPE_PD);
			break;

		case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify Type-C Ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
			mutex_unlock(&pinfo->charger_pd_lock);
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
		};
	}
}

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
	default:
		break;
	}

	pinfo->pd_adapter->uvdm_state = cmd;
}

static void mtk_charger_parse_cmdline(void)
{
	char *pissarro = NULL, *pissarropro = NULL, *pissarroinpro = NULL, *hwc_cn = NULL;

	pissarro = strnstr(saved_command_line, "pissarro", strlen(saved_command_line));
	pissarropro = strnstr(saved_command_line, "pissarropro", strlen(saved_command_line));
	pissarroinpro = strnstr(saved_command_line, "pissarroinpro", strlen(saved_command_line));
	hwc_cn = strnstr(saved_command_line, "hwc=CN", strlen(saved_command_line));

	chr_info("pissarro = %d, pissarropro = %d, pissarroinpro = %d, hwc_cn = %d\n",
		pissarro ? 1 : 0, pissarropro ? 1 : 0, pissarroinpro ? 1 : 0, hwc_cn ? 1 : 0);

	if (pissarropro || pissarroinpro)
		product_name = PISSARROPRO;
	else if (pissarro)
		product_name = PISSARRO;

	if (hwc_cn)
		g_hwc_cn = true;
	else
		g_hwc_cn = false;
}

static const struct platform_device_id mtk_charger_id[] = {
	{ "pissarro_charger", PISSARRO },
	{ "pissarropro_charger", PISSARROPRO },
	{},
};
MODULE_DEVICE_TABLE(platform, mtk_charger_id);

static const struct of_device_id mtk_charger_of_match[] = {
	{.compatible = "pissarro_charger", .data = &mtk_charger_id[0], },
	{.compatible = "pissarropro_charger", .data = &mtk_charger_id[1], },
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
	int ret = 0;

	mtk_charger_parse_cmdline();

	of_id = of_match_device(mtk_charger_of_match, &pdev->dev);
	pdev->id_entry = of_id->data;

	if (pdev->id_entry->driver_data == product_name) {
		chr_info("CHARGE_ALG probe start\n");
	} else {
		chr_info("driver_data and product_name not match, don't probe, %d\n", pdev->id_entry->driver_data);
		return -ENODEV;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	pinfo = info;

	platform_set_drvdata(pdev, info);
	info->pdev = pdev;
	mtk_charger_parse_dt(info, &pdev->dev);

	if (!mtk_check_votable(info))
		chr_err("failed to check votable\n");

	if (!mtk_check_psy(info))
		chr_err("failed to check psy\n");

	mutex_init(&info->charger_lock);
	mutex_init(&info->charger_pd_lock);
	atomic_set(&info->enable_kpoc_shdn, 1);
	wakeup_source_init(&info->charger_wakelock, "charger suspend wakelock");
	wakeup_source_init(&info->attach_wakelock, "attach_wakelock");
	wakeup_source_init(&info->typec_burn_wakelock, "typec_burn_wakelock");
	spin_lock_init(&info->slock);

	ret = step_jeita_init(info, &pdev->dev, product_name);
	if (ret)
		chr_err("failed to init step_jeita\n");

	/* init thread */
	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	info->enable_dynamic_cv = true;

	info->chg1_data.input_current_limit_by_aicl = -1;

	info->sw_jeita.error_recovery_flag = true;
	info->sic_current = info->max_fcc;
	mtk_charger_init_timer(info);

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

	mtk_pdc_init(info);
	charger_ftm_init();
	mtk_charger_get_atm_mode(info);
	sw_jeita_state_machine_init(info);

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

	info->chg1_consumer =
		charger_manager_get_by_name(&pdev->dev, "charger_port1");
	info->init_done = true;
	_wake_up_charger(info);

	chr_info("CHARGE_ALG probe success\n");

	return 0;
}

static int mtk_charger_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_charger_shutdown(struct platform_device *dev)
{
	return;
}

static struct platform_driver charger_driver = {
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
	.id_table = mtk_charger_id,
	.driver = {
		   .name = "charger",
		   .of_match_table = of_match_ptr(mtk_charger_of_match),
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
