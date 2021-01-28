// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/mtk_boot.h>
#include <pmic.h>
#include <mtk_gauge_time_service.h>

#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"

static struct charger_manager *pinfo;
static struct list_head consumer_head = LIST_HEAD_INIT(consumer_head);
static DEFINE_MUTEX(consumer_mutex);


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
	if (!info->charger_wakelock.active)
		__pm_stay_awake(&info->charger_wakelock);
	spin_unlock_irqrestore(&info->slock, flags);
	info->charger_thread_timeout = true;
	wake_up(&info->wait_que);
}

/* charger_manager ops  */
static int _mtk_charger_change_current_setting(struct charger_manager *info)
{
	if (info != NULL && info->change_current_setting)
		return info->change_current_setting(info);

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
	struct list_head *pos;
	struct list_head *phead = &consumer_head;
	struct charger_consumer *ptr;

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
	struct charger_device *chg_dev;


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
		struct charger_data *pdata;

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
		struct charger_data *pdata;

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
		struct charger_data *pdata;

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

int charger_manager_get_zcv(struct charger_consumer *consumer, int idx, u32 *uV)
{
	struct charger_manager *info = consumer->cm;
	int ret = 0;
	struct charger_device *pchg;


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

int charger_manager_enable_chg_type_det(struct charger_consumer *consumer,
	bool en)
{
	struct charger_manager *info = consumer->cm;
	struct charger_device *chg_dev;
	int ret = 0;

	if (info != NULL) {
		switch (info->data.bc12_charger) {
		case MAIN_CHARGER:
			chg_dev = info->chg1_dev;
			break;
		case SLAVE_CHARGER:
			chg_dev = info->chg2_dev;
			break;
		default:
			chg_dev = info->chg1_dev;
			chr_err("%s: invalid number, use main charger as default\n",
				__func__);
			break;
		}

		chr_err("%s: chg%d is doing bc12\n", __func__,
			info->data.bc12_charger + 1);
		ret = charger_dev_enable_chg_type_det(chg_dev, en);
		if (ret < 0) {
			chr_err("%s: en chgdet fail, en = %d\n", __func__, en);
			return ret;
		}
	} else
		chr_err("%s: charger_manager is null\n", __func__);



	return 0;
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
	char atm_str[64];
	char *ptr, *ptr_e;

	memset(atm_str, 0x0, sizeof(atm_str));
	ptr = strstr(saved_command_line, "androidboot.atm=");
	if (ptr != 0) {
		ptr_e = strstr(ptr, " ");

		if (ptr_e != 0) {
			strncpy(atm_str, ptr + 16, ptr_e - ptr - 16);
			atm_str[ptr_e - ptr - 16] = '\0';
		}

		if (!strncmp(atm_str, "enable", strlen("enable")))
			info->atm_enabled = true;
		else
			info->atm_enabled = false;
	} else
		info->atm_enabled = false;

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

static ssize_t sw_jeita_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_sw_jeita);
	return sprintf(buf, "%d\n", pinfo->enable_sw_jeita);
}

static ssize_t sw_jeita_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_sw_jeita = false;
		else
			pinfo->enable_sw_jeita = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(sw_jeita, 0644, sw_jeita_show,
		   sw_jeita_store);
/* sw jeita end*/

/* pump express series */
bool mtk_is_pep_series_connect(struct charger_manager *info)
{
	if (mtk_pe20_get_is_connect(info) || mtk_pe_get_is_connect(info))
		return true;

	return false;
}

static ssize_t pe20_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_pe_2);
	return sprintf(buf, "%d\n", pinfo->enable_pe_2);
}

static ssize_t pe20_store(struct device *dev, struct device_attribute *attr,
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

static DEVICE_ATTR_RW(pe20, 0644, pe20_show, pe20_store);

static ssize_t pe40_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_pe_4);
	return sprintf(buf, "%d\n", pinfo->enable_pe_4);
}

static ssize_t pe40_store(struct device *dev, struct device_attribute *attr,
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

static DEVICE_ATTR_RW(pe40, 0644, pe40_show, pe40_store);

/* pump express series end*/

static ssize_t charger_log_level_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	chr_err("%s: %d\n", __func__, chargerlog_level);
	return sprintf(buf, "%d\n", chargerlog_level);
}

static ssize_t charger_log_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	chr_err("%s\n", __func__);

	if (buf != NULL && size != 0) {
		chr_err("%s: buf is %s\n", __func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err("%s: val is invalid: %ld\n", __func__, val);
			val = 0;
		}
		chargerlog_level = val;
		chr_err("%s: log_level=%d\n", __func__, chargerlog_level);
	}
	return size;
}
static DEVICE_ATTR_RW(charger_log_level, 0644, charger_log_level_show,
		charger_log_level_store);

static ssize_t pdc_max_watt_level_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	return sprintf(buf, "%d\n", mtk_pdc_get_max_watt(pinfo));
}

static ssize_t pdc_max_watt_level_store(struct device *dev,
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
static DEVICE_ATTR_RW(pdc_max_watt, 0644, pdc_max_watt_level_show,
		pdc_max_watt_level_store);

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

static int mtk_charger_plug_in(struct charger_manager *info,
				enum charger_type chr_type)
{
	info->chr_type = chr_type;
	info->charger_thread_polling = true;

	info->can_charging = true;
	info->enable_dynamic_cv = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;

	chr_err("mtk_is_charger_on plug in, type:%d\n", chr_type);
	if (info->plug_in != NULL)
		info->plug_in(info);

	charger_dev_set_input_current(info->chg1_dev,
				info->chg1_data.input_current_limit);
	charger_dev_plug_in(info->chg1_dev);
	return 0;
}

static int mtk_charger_plug_out(struct charger_manager *info)
{
	struct charger_data *pdata1 = &info->chg1_data;
	struct charger_data *pdata2 = &info->chg2_data;

	chr_err("%s\n", __func__);
	info->chr_type = CHARGER_UNKNOWN;
	info->charger_thread_polling = false;

	pdata1->disable_charging_count = 0;
	pdata1->input_current_limit_by_aicl = -1;
	pdata2->disable_charging_count = 0;

	if (info->plug_out != NULL)
		info->plug_out(info);

	charger_dev_set_input_current(info->chg1_dev, 100000);
	charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
	charger_dev_plug_out(info->chg1_dev);
	return 0;
}

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
			mtk_charger_plug_in(info, chr_type);
		else
			info->chr_type = chr_type;

		if (info->cable_out_cnt > 0) {
			mtk_charger_plug_out(info);
			mtk_charger_plug_in(info, chr_type);
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
}

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
	unsigned int i = 0;
	int count = 0;
	int boot_mode = get_boot_mode();

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
}

static void charger_check_status(struct charger_manager *info)
{
	bool charging = true;
	int temperature;
	struct battery_thermal_protection_data *thermal;

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

stop_charging:
	mtk_battery_notify_check(info);

	chr_err("tmp:%d (jeita:%d sm:%d cv:%d en:%d) (sm:%d) en:%d c:%d s:%d ov:%d %d %d\n",
		temperature, info->enable_sw_jeita, info->sw_jeita.sm,
		info->sw_jeita.cv, info->sw_jeita.charging, thermal->sm,
		charging, info->cmd_discharging, info->safety_timeout,
		info->vbusov_stat, info->can_charging, charging);

	if (charging != info->can_charging)
		_charger_manager_enable_charging(info->chg1_consumer,
						0, charging);

	info->can_charging = charging;
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
			__pm_relax(&pinfo->charger_wakelock);
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

	if (info->is_suspend == false) {
		chr_err("%s: not suspend, wake up charger\n", __func__);
		_wake_up_charger(info);
	} else {
		chr_err("%s: alarm timer timeout\n", __func__);
		__pm_stay_awake(&info->charger_wakelock);
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
	unsigned long flags;
	bool is_charger_on;
	int bat_current, chg_current;

	while (1) {
		wait_event(info->wait_que,
			(info->charger_thread_timeout == true));

		mutex_lock(&info->charger_lock);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock.active)
			__pm_stay_awake(&info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);

		info->charger_thread_timeout = false;
		bat_current = battery_get_bat_current();
		chg_current = pmic_get_charging_current();
		chr_err("Vbat=%d,Ibat=%d,I=%d,VChr=%d,T=%d,Soc=%d:%d,CT:%d:%d hv:%d pd:%d:%d\n",
			battery_get_bat_voltage(), bat_current, chg_current,
			battery_get_vbus(), battery_get_bat_temperature(),
			battery_get_soc(), battery_get_uisoc(),
			mt_get_charger_type(), info->chr_type,
			info->enable_hv_charging, info->pd_type,
			info->pd_reset);

		if (info->pd_reset == true) {
			mtk_pe40_plugout_reset(info);
			info->pd_reset = false;
		}

		is_charger_on = mtk_is_charger_on(info);

		if (info->charger_thread_polling == true)
			mtk_charger_start_timer(info);

		charger_update_data(info);
		check_battery_exist(info);
		check_dynamic_mivr(info);
		charger_check_status(info);
		kpoc_power_off_check(info);

		if (is_disable_charger() == false) {
			if (is_charger_on == true) {
				if (info->do_algorithm)
					info->do_algorithm(info);
			}
		} else
			chr_debug("disable charging\n");

		spin_lock_irqsave(&info->slock, flags);
		__pm_relax(&info->charger_wakelock);
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
	u32 val;

	chr_err("%s: starts\n", __func__);

	if (!np) {
		chr_err("%s: no device node\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "algorithm_name",
		&info->algorithm_name) < 0) {
		chr_err("%s: no algorithm_name name\n", __func__);
		info->algorithm_name = "SwitchCharging";
	}

	if (strcmp(info->algorithm_name, "SwitchCharging") == 0) {
		chr_err("found SwitchCharging\n");
		mtk_switch_charging_init(info);
	}
#ifdef CONFIG_MTK_DUAL_CHARGER_SUPPORT
	if (strcmp(info->algorithm_name, "DualSwitchCharging") == 0) {
		pr_debug("found DualSwitchCharging\n");
		mtk_dual_switch_charging_init(info);
	}
#endif

	info->disable_charger = of_property_read_bool(np, "disable_charger");
	info->enable_sw_safety_timer =
			of_property_read_bool(np, "enable_sw_safety_timer");
	info->sw_safety_timer_setting = info->enable_sw_safety_timer;
	info->enable_sw_jeita = of_property_read_bool(np, "enable_sw_jeita");
	info->enable_pe_plus = of_property_read_bool(np, "enable_pe_plus");
	info->enable_pe_2 = of_property_read_bool(np, "enable_pe_2");
	info->enable_pe_4 = of_property_read_bool(np, "enable_pe_4");
	info->enable_type_c = of_property_read_bool(np, "enable_type_c");
	info->enable_dynamic_mivr =
			of_property_read_bool(np, "enable_dynamic_mivr");
	info->disable_pd_dual = of_property_read_bool(np, "disable_pd_dual");

	info->enable_hv_charging = true;

	/* common */
	if (of_property_read_u32(np, "battery_cv", &val) >= 0)
		info->data.battery_cv = val;
	else {
		chr_err("use default BATTERY_CV:%d\n", BATTERY_CV);
		info->data.battery_cv = BATTERY_CV;
	}

	if (of_property_read_u32(np, "max_charger_voltage", &val) >= 0)
		info->data.max_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MAX:%d\n", V_CHARGER_MAX);
		info->data.max_charger_voltage = V_CHARGER_MAX;
	}
	info->data.max_charger_voltage_setting = info->data.max_charger_voltage;

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		info->data.min_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		info->data.min_charger_voltage = V_CHARGER_MIN;
	}

	/* dynamic mivr */
	if (of_property_read_u32(np, "min_charger_voltage_1", &val) >= 0)
		info->data.min_charger_voltage_1 = val;
	else {
		chr_err("use default V_CHARGER_MIN_1:%d\n", V_CHARGER_MIN_1);
		info->data.min_charger_voltage_1 = V_CHARGER_MIN_1;
	}

	if (of_property_read_u32(np, "min_charger_voltage_2", &val) >= 0)
		info->data.min_charger_voltage_2 = val;
	else {
		chr_err("use default V_CHARGER_MIN_2:%d\n", V_CHARGER_MIN_2);
		info->data.min_charger_voltage_2 = V_CHARGER_MIN_2;
	}

	if (of_property_read_u32(np, "max_dmivr_charger_current", &val) >= 0)
		info->data.max_dmivr_charger_current = val;
	else {
		chr_err("use default MAX_DMIVR_CHARGER_CURRENT:%d\n",
			MAX_DMIVR_CHARGER_CURRENT);
		info->data.max_dmivr_charger_current =
					MAX_DMIVR_CHARGER_CURRENT;
	}

	/* charging current */
	if (of_property_read_u32(np, "usb_charger_current_suspend", &val) >= 0)
		info->data.usb_charger_current_suspend = val;
	else {
		chr_err("use default USB_CHARGER_CURRENT_SUSPEND:%d\n",
			USB_CHARGER_CURRENT_SUSPEND);
		info->data.usb_charger_current_suspend =
						USB_CHARGER_CURRENT_SUSPEND;
	}

	if (of_property_read_u32(np, "usb_charger_current_unconfigured", &val)
		>= 0) {
		info->data.usb_charger_current_unconfigured = val;
	} else {
		chr_err("use default USB_CHARGER_CURRENT_UNCONFIGURED:%d\n",
			USB_CHARGER_CURRENT_UNCONFIGURED);
		info->data.usb_charger_current_unconfigured =
					USB_CHARGER_CURRENT_UNCONFIGURED;
	}

	if (of_property_read_u32(np, "usb_charger_current_configured", &val)
		>= 0) {
		info->data.usb_charger_current_configured = val;
	} else {
		chr_err("use default USB_CHARGER_CURRENT_CONFIGURED:%d\n",
			USB_CHARGER_CURRENT_CONFIGURED);
		info->data.usb_charger_current_configured =
					USB_CHARGER_CURRENT_CONFIGURED;
	}

	if (of_property_read_u32(np, "usb_charger_current", &val) >= 0) {
		info->data.usb_charger_current = val;
	} else {
		chr_err("use default USB_CHARGER_CURRENT:%d\n",
			USB_CHARGER_CURRENT);
		info->data.usb_charger_current = USB_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_current", &val) >= 0) {
		info->data.ac_charger_current = val;
	} else {
		chr_err("use default AC_CHARGER_CURRENT:%d\n",
			AC_CHARGER_CURRENT);
		info->data.ac_charger_current = AC_CHARGER_CURRENT;
	}

	info->data.pd_charger_current = 3000000;

	if (of_property_read_u32(np, "ac_charger_input_current", &val) >= 0)
		info->data.ac_charger_input_current = val;
	else {
		chr_err("use default AC_CHARGER_INPUT_CURRENT:%d\n",
			AC_CHARGER_INPUT_CURRENT);
		info->data.ac_charger_input_current = AC_CHARGER_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "non_std_ac_charger_current", &val) >= 0)
		info->data.non_std_ac_charger_current = val;
	else {
		chr_err("use default NON_STD_AC_CHARGER_CURRENT:%d\n",
			NON_STD_AC_CHARGER_CURRENT);
		info->data.non_std_ac_charger_current =
					NON_STD_AC_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "charging_host_charger_current", &val)
		>= 0) {
		info->data.charging_host_charger_current = val;
	} else {
		chr_err("use default CHARGING_HOST_CHARGER_CURRENT:%d\n",
			CHARGING_HOST_CHARGER_CURRENT);
		info->data.charging_host_charger_current =
					CHARGING_HOST_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "apple_1_0a_charger_current", &val) >= 0)
		info->data.apple_1_0a_charger_current = val;
	else {
		chr_err("use default APPLE_1_0A_CHARGER_CURRENT:%d\n",
			APPLE_1_0A_CHARGER_CURRENT);
		info->data.apple_1_0a_charger_current =
					APPLE_1_0A_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "apple_2_1a_charger_current", &val) >= 0)
		info->data.apple_2_1a_charger_current = val;
	else {
		chr_err("use default APPLE_2_1A_CHARGER_CURRENT:%d\n",
			APPLE_2_1A_CHARGER_CURRENT);
		info->data.apple_2_1a_charger_current =
					APPLE_2_1A_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ta_ac_charger_current", &val) >= 0)
		info->data.ta_ac_charger_current = val;
	else {
		chr_err("use default TA_AC_CHARGING_CURRENT:%d\n",
			TA_AC_CHARGING_CURRENT);
		info->data.ta_ac_charger_current =
					TA_AC_CHARGING_CURRENT;
	}

	/* sw jeita */
	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		info->data.jeita_temp_above_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T4_CV:%d\n",
			JEITA_TEMP_ABOVE_T4_CV);
		info->data.jeita_temp_above_t4_cv = JEITA_TEMP_ABOVE_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CV:%d\n",
			JEITA_TEMP_T3_TO_T4_CV);
		info->data.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CV:%d\n",
			JEITA_TEMP_T2_TO_T3_CV);
		info->data.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cv", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CV:%d\n",
			JEITA_TEMP_T1_TO_T2_CV);
		info->data.jeita_temp_t1_to_t2_cv = JEITA_TEMP_T1_TO_T2_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CV:%d\n",
			JEITA_TEMP_T0_TO_T1_CV);
		info->data.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		info->data.jeita_temp_below_t0_cv = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CV:%d\n",
			JEITA_TEMP_BELOW_T0_CV);
		info->data.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		info->data.temp_t4_thres = val;
	else {
		chr_err("use default TEMP_T4_THRES:%d\n",
			TEMP_T4_THRES);
		info->data.temp_t4_thres = TEMP_T4_THRES;
	}

	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t4_thres_minus_x_degree = val;
	else {
		chr_err("use default TEMP_T4_THRES_MINUS_X_DEGREE:%d\n",
			TEMP_T4_THRES_MINUS_X_DEGREE);
		info->data.temp_t4_thres_minus_x_degree =
					TEMP_T4_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		info->data.temp_t3_thres = val;
	else {
		chr_err("use default TEMP_T3_THRES:%d\n",
			TEMP_T3_THRES);
		info->data.temp_t3_thres = TEMP_T3_THRES;
	}

	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t3_thres_minus_x_degree = val;
	else {
		chr_err("use default TEMP_T3_THRES_MINUS_X_DEGREE:%d\n",
			TEMP_T3_THRES_MINUS_X_DEGREE);
		info->data.temp_t3_thres_minus_x_degree =
					TEMP_T3_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		info->data.temp_t2_thres = val;
	else {
		chr_err("use default TEMP_T2_THRES:%d\n",
			TEMP_T2_THRES);
		info->data.temp_t2_thres = TEMP_T2_THRES;
	}

	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t2_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T2_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T2_THRES_PLUS_X_DEGREE);
		info->data.temp_t2_thres_plus_x_degree =
					TEMP_T2_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		info->data.temp_t1_thres = val;
	else {
		chr_err("use default TEMP_T1_THRES:%d\n",
			TEMP_T1_THRES);
		info->data.temp_t1_thres = TEMP_T1_THRES;
	}

	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t1_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T1_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T1_THRES_PLUS_X_DEGREE);
		info->data.temp_t1_thres_plus_x_degree =
					TEMP_T1_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		info->data.temp_t0_thres = val;
	else {
		chr_err("use default TEMP_T0_THRES:%d\n",
			TEMP_T0_THRES);
		info->data.temp_t0_thres = TEMP_T0_THRES;
	}

	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t0_thres_plus_x_degree = val;
	else {
		chr_err("use default TEMP_T0_THRES_PLUS_X_DEGREE:%d\n",
			TEMP_T0_THRES_PLUS_X_DEGREE);
		info->data.temp_t0_thres_plus_x_degree =
					TEMP_T0_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		info->data.temp_neg_10_thres = val;
	else {
		chr_err("use default TEMP_NEG_10_THRES:%d\n",
			TEMP_NEG_10_THRES);
		info->data.temp_neg_10_thres = TEMP_NEG_10_THRES;
	}

	/* battery temperature protection */
	info->thermal.sm = BAT_TEMP_NORMAL;
	info->thermal.enable_min_charge_temp =
		of_property_read_bool(np, "enable_min_charge_temp");

	if (of_property_read_u32(np, "min_charge_temp", &val) >= 0)
		info->thermal.min_charge_temp = val;
	else {
		chr_err("use default MIN_CHARGE_TEMP:%d\n",
			MIN_CHARGE_TEMP);
		info->thermal.min_charge_temp = MIN_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "min_charge_temp_plus_x_degree", &val)
	    >= 0) {
		info->thermal.min_charge_temp_plus_x_degree = val;
	} else {
		chr_err("use default MIN_CHARGE_TEMP_PLUS_X_DEGREE:%d\n",
			MIN_CHARGE_TEMP_PLUS_X_DEGREE);
		info->thermal.min_charge_temp_plus_x_degree =
					MIN_CHARGE_TEMP_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "max_charge_temp", &val) >= 0)
		info->thermal.max_charge_temp = val;
	else {
		chr_err("use default MAX_CHARGE_TEMP:%d\n",
			MAX_CHARGE_TEMP);
		info->thermal.max_charge_temp = MAX_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "max_charge_temp_minus_x_degree", &val)
	    >= 0) {
		info->thermal.max_charge_temp_minus_x_degree = val;
	} else {
		chr_err("use default MAX_CHARGE_TEMP_MINUS_X_DEGREE:%d\n",
			MAX_CHARGE_TEMP_MINUS_X_DEGREE);
		info->thermal.max_charge_temp_minus_x_degree =
					MAX_CHARGE_TEMP_MINUS_X_DEGREE;
	}

	/* PE */
	info->data.ta_12v_support = of_property_read_bool(np, "ta_12v_support");
	info->data.ta_9v_support = of_property_read_bool(np, "ta_9v_support");

	if (of_property_read_u32(np, "pe_ichg_level_threshold", &val) >= 0)
		info->data.pe_ichg_level_threshold = val;
	else {
		chr_err("use default PE_ICHG_LEAVE_THRESHOLD:%d\n",
			PE_ICHG_LEAVE_THRESHOLD);
		info->data.pe_ichg_level_threshold = PE_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "ta_ac_12v_input_current", &val) >= 0)
		info->data.ta_ac_12v_input_current = val;
	else {
		chr_err("use default TA_AC_12V_INPUT_CURRENT:%d\n",
			TA_AC_12V_INPUT_CURRENT);
		info->data.ta_ac_12v_input_current = TA_AC_12V_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "ta_ac_9v_input_current", &val) >= 0)
		info->data.ta_ac_9v_input_current = val;
	else {
		chr_err("use default TA_AC_9V_INPUT_CURRENT:%d\n",
			TA_AC_9V_INPUT_CURRENT);
		info->data.ta_ac_9v_input_current = TA_AC_9V_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "ta_ac_7v_input_current", &val) >= 0)
		info->data.ta_ac_7v_input_current = val;
	else {
		chr_err("use default TA_AC_7V_INPUT_CURRENT:%d\n",
			TA_AC_7V_INPUT_CURRENT);
		info->data.ta_ac_7v_input_current = TA_AC_7V_INPUT_CURRENT;
	}

	/* PE 2.0 */
	if (of_property_read_u32(np, "pe20_ichg_level_threshold", &val) >= 0)
		info->data.pe20_ichg_level_threshold = val;
	else {
		chr_err("use default PE20_ICHG_LEAVE_THRESHOLD:%d\n",
			PE20_ICHG_LEAVE_THRESHOLD);
		info->data.pe20_ichg_level_threshold =
						PE20_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "ta_start_battery_soc", &val) >= 0)
		info->data.ta_start_battery_soc = val;
	else {
		chr_err("use default TA_START_BATTERY_SOC:%d\n",
			TA_START_BATTERY_SOC);
		info->data.ta_start_battery_soc = TA_START_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "ta_stop_battery_soc", &val) >= 0)
		info->data.ta_stop_battery_soc = val;
	else {
		chr_err("use default TA_STOP_BATTERY_SOC:%d\n",
			TA_STOP_BATTERY_SOC);
		info->data.ta_stop_battery_soc = TA_STOP_BATTERY_SOC;
	}

	/* PE 4.0 */
	if (of_property_read_u32(np, "high_temp_to_leave_pe40", &val) >= 0) {
		info->data.high_temp_to_leave_pe40 = val;
	} else {
		chr_err("use default high_temp_to_leave_pe40:%d\n",
			HIGH_TEMP_TO_LEAVE_PE40);
		info->data.high_temp_to_leave_pe40 = HIGH_TEMP_TO_LEAVE_PE40;
	}

	if (of_property_read_u32(np, "high_temp_to_enter_pe40", &val) >= 0) {
		info->data.high_temp_to_enter_pe40 = val;
	} else {
		chr_err("use default high_temp_to_enter_pe40:%d\n",
			HIGH_TEMP_TO_ENTER_PE40);
		info->data.high_temp_to_enter_pe40 = HIGH_TEMP_TO_ENTER_PE40;
	}

	if (of_property_read_u32(np, "low_temp_to_leave_pe40", &val) >= 0) {
		info->data.low_temp_to_leave_pe40 = val;
	} else {
		chr_err("use default low_temp_to_leave_pe40:%d\n",
			LOW_TEMP_TO_LEAVE_PE40);
		info->data.low_temp_to_leave_pe40 = LOW_TEMP_TO_LEAVE_PE40;
	}

	if (of_property_read_u32(np, "low_temp_to_enter_pe40", &val) >= 0) {
		info->data.low_temp_to_enter_pe40 = val;
	} else {
		chr_err("use default low_temp_to_enter_pe40:%d\n",
			LOW_TEMP_TO_ENTER_PE40);
		info->data.low_temp_to_enter_pe40 = LOW_TEMP_TO_ENTER_PE40;
	}

	/* PE 4.0 single */
	if (of_property_read_u32(np,
		"pe40_single_charger_input_current", &val) >= 0) {
		info->data.pe40_single_charger_input_current = val;
	} else {
		chr_err("use default pe40_single_charger_input_current:%d\n",
			3000);
		info->data.pe40_single_charger_input_current = 3000;
	}

	if (of_property_read_u32(np, "pe40_single_charger_current", &val)
	    >= 0) {
		info->data.pe40_single_charger_current = val;
	} else {
		chr_err("use default pe40_single_charger_current:%d\n", 3000);
		info->data.pe40_single_charger_current = 3000;
	}

	/* PE 4.0 dual */
	if (of_property_read_u32(np, "pe40_dual_charger_input_current", &val)
	    >= 0) {
		info->data.pe40_dual_charger_input_current = val;
	} else {
		chr_err("use default pe40_dual_charger_input_current:%d\n",
			3000);
		info->data.pe40_dual_charger_input_current = 3000;
	}

	if (of_property_read_u32(np, "pe40_dual_charger_chg1_current", &val)
	    >= 0) {
		info->data.pe40_dual_charger_chg1_current = val;
	} else {
		chr_err("use default pe40_dual_charger_chg1_current:%d\n",
			2000);
		info->data.pe40_dual_charger_chg1_current = 2000;
	}

	if (of_property_read_u32(np, "pe40_dual_charger_chg2_current", &val)
	    >= 0) {
		info->data.pe40_dual_charger_chg2_current = val;
	} else {
		chr_err("use default pe40_dual_charger_chg2_current:%d\n",
			2000);
		info->data.pe40_dual_charger_chg2_current = 2000;
	}

	if (of_property_read_u32(np, "dual_polling_ieoc", &val) >= 0)
		info->data.dual_polling_ieoc = val;
	else {
		chr_err("use default dual_polling_ieoc :%d\n", 750000);
		info->data.dual_polling_ieoc = 750000;
	}

	if (of_property_read_u32(np, "pe40_stop_battery_soc", &val) >= 0)
		info->data.pe40_stop_battery_soc = val;
	else {
		chr_err("use default pe40_stop_battery_soc:%d\n", 80);
		info->data.pe40_stop_battery_soc = 80;
	}

	if (of_property_read_u32(np, "pe40_max_vbus", &val) >= 0)
		info->data.pe40_max_vbus = val;
	else {
		chr_err("use default pe40_max_vbus:%d\n", PE40_MAX_VBUS);
		info->data.pe40_max_vbus = PE40_MAX_VBUS;
	}

	if (of_property_read_u32(np, "pe40_max_ibus", &val) >= 0)
		info->data.pe40_max_ibus = val;
	else {
		chr_err("use default pe40_max_ibus:%d\n", PE40_MAX_IBUS);
		info->data.pe40_max_ibus = PE40_MAX_IBUS;
	}

	/* PE 4.0 cable impedance (mohm) */
	if (of_property_read_u32(np, "pe40_r_cable_1a_lower", &val) >= 0)
		info->data.pe40_r_cable_1a_lower = val;
	else {
		chr_err("use default pe40_r_cable_1a_lower:%d\n", 530);
		info->data.pe40_r_cable_1a_lower = 530;
	}

	if (of_property_read_u32(np, "pe40_r_cable_2a_lower", &val) >= 0)
		info->data.pe40_r_cable_2a_lower = val;
	else {
		chr_err("use default pe40_r_cable_2a_lower:%d\n", 390);
		info->data.pe40_r_cable_2a_lower = 390;
	}

	if (of_property_read_u32(np, "pe40_r_cable_3a_lower", &val) >= 0)
		info->data.pe40_r_cable_3a_lower = val;
	else {
		chr_err("use default pe40_r_cable_3a_lower:%d\n", 252);
		info->data.pe40_r_cable_3a_lower = 252;
	}

	/* PD */
	if (of_property_read_u32(np, "pd_vbus_upper_bound", &val) >= 0) {
		info->data.pd_vbus_upper_bound = val;
	} else {
		chr_err("use default pd_vbus_upper_bound:%d\n",
			PD_VBUS_UPPER_BOUND);
		info->data.pd_vbus_upper_bound = PD_VBUS_UPPER_BOUND;
	}

	if (of_property_read_u32(np, "pd_vbus_low_bound", &val) >= 0) {
		info->data.pd_vbus_low_bound = val;
	} else {
		chr_err("use default pd_vbus_low_bound:%d\n",
			PD_VBUS_LOW_BOUND);
		info->data.pd_vbus_low_bound = PD_VBUS_LOW_BOUND;
	}

	if (of_property_read_u32(np, "pd_ichg_level_threshold", &val) >= 0)
		info->data.pd_ichg_level_threshold = val;
	else {
		chr_err("use default pd_ichg_level_threshold:%d\n",
			PD_ICHG_LEAVE_THRESHOLD);
		info->data.pd_ichg_level_threshold = PD_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "pd_stop_battery_soc", &val) >= 0)
		info->data.pd_stop_battery_soc = val;
	else {
		chr_err("use default pd_stop_battery_soc:%d\n",
			PD_STOP_BATTERY_SOC);
		info->data.pd_stop_battery_soc = PD_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "vsys_watt", &val) >= 0) {
		info->data.vsys_watt = val;
	} else {
		chr_err("use default vsys_watt:%d\n",
			VSYS_WATT);
		info->data.vsys_watt = VSYS_WATT;
	}

	if (of_property_read_u32(np, "ibus_err", &val) >= 0) {
		info->data.ibus_err = val;
	} else {
		chr_err("use default ibus_err:%d\n",
			IBUS_ERR);
		info->data.ibus_err = IBUS_ERR;
	}

	/* dual charger */
	if (of_property_read_u32(np, "chg1_ta_ac_charger_current", &val) >= 0)
		info->data.chg1_ta_ac_charger_current = val;
	else {
		chr_err("use default TA_AC_MASTER_CHARGING_CURRENT:%d\n",
			TA_AC_MASTER_CHARGING_CURRENT);
		info->data.chg1_ta_ac_charger_current =
						TA_AC_MASTER_CHARGING_CURRENT;
	}

	if (of_property_read_u32(np, "chg2_ta_ac_charger_current", &val) >= 0)
		info->data.chg2_ta_ac_charger_current = val;
	else {
		chr_err("use default TA_AC_SLAVE_CHARGING_CURRENT:%d\n",
			TA_AC_SLAVE_CHARGING_CURRENT);
		info->data.chg2_ta_ac_charger_current =
						TA_AC_SLAVE_CHARGING_CURRENT;
	}

	if (of_property_read_u32(np, "slave_mivr_diff", &val) >= 0)
		info->data.slave_mivr_diff = val;
	else {
		chr_err("use default SLAVE_MIVR_DIFF:%d\n", SLAVE_MIVR_DIFF);
		info->data.slave_mivr_diff = SLAVE_MIVR_DIFF;
	}

	/* slave charger */
	if (of_property_read_u32(np, "chg2_eff", &val) >= 0)
		info->data.chg2_eff = val;
	else {
		chr_err("use default CHG2_EFF:%d\n", CHG2_EFF);
		info->data.chg2_eff = CHG2_EFF;
	}

	info->data.parallel_vbus = of_property_read_bool(np, "parallel_vbus");

	/* cable measurement impedance */
	if (of_property_read_u32(np, "cable_imp_threshold", &val) >= 0)
		info->data.cable_imp_threshold = val;
	else {
		chr_err("use default CABLE_IMP_THRESHOLD:%d\n",
			CABLE_IMP_THRESHOLD);
		info->data.cable_imp_threshold = CABLE_IMP_THRESHOLD;
	}

	if (of_property_read_u32(np, "vbat_cable_imp_threshold", &val) >= 0)
		info->data.vbat_cable_imp_threshold = val;
	else {
		chr_err("use default VBAT_CABLE_IMP_THRESHOLD:%d\n",
			VBAT_CABLE_IMP_THRESHOLD);
		info->data.vbat_cable_imp_threshold = VBAT_CABLE_IMP_THRESHOLD;
	}

	/* BIF */
	if (of_property_read_u32(np, "bif_threshold1", &val) >= 0)
		info->data.bif_threshold1 = val;
	else {
		chr_err("use default BIF_THRESHOLD1:%d\n",
			BIF_THRESHOLD1);
		info->data.bif_threshold1 = BIF_THRESHOLD1;
	}

	if (of_property_read_u32(np, "bif_threshold2", &val) >= 0)
		info->data.bif_threshold2 = val;
	else {
		chr_err("use default BIF_THRESHOLD2:%d\n",
			BIF_THRESHOLD2);
		info->data.bif_threshold2 = BIF_THRESHOLD2;
	}

	if (of_property_read_u32(np, "bif_cv_under_threshold2", &val) >= 0)
		info->data.bif_cv_under_threshold2 = val;
	else {
		chr_err("use default BIF_CV_UNDER_THRESHOLD2:%d\n",
			BIF_CV_UNDER_THRESHOLD2);
		info->data.bif_cv_under_threshold2 = BIF_CV_UNDER_THRESHOLD2;
	}

	info->data.power_path_support =
				of_property_read_bool(np, "power_path_support");
	chr_debug("%s: power_path_support: %d\n",
		__func__, info->data.power_path_support);

	if (of_property_read_u32(np, "max_charging_time", &val) >= 0)
		info->data.max_charging_time = val;
	else {
		chr_err("use default MAX_CHARGING_TIME:%d\n",
			MAX_CHARGING_TIME);
		info->data.max_charging_time = MAX_CHARGING_TIME;
	}

	if (of_property_read_u32(np, "bc12_charger", &val) >= 0)
		info->data.bc12_charger = val;
	else {
		chr_err("use default BC12_CHARGER:%d\n",
			DEFAULT_BC12_CHARGER);
		info->data.bc12_charger = DEFAULT_BC12_CHARGER;
	}

	if (strcmp(info->algorithm_name, "SwitchCharging2") == 0) {
		chr_err("found SwitchCharging2\n");
		mtk_switch_charging_init2(info);
	}

	chr_err("algorithm name:%s\n", info->algorithm_name);

	return 0;
}


static ssize_t Pump_Express_show(struct device *dev,
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

	if (mtk_is_TA_support_pd_pps(pinfo) == true)
		is_ta_detected = 1;

	pr_debug("%s: detected = %d, pe20_connect = %d, pe_connect = %d\n",
		__func__, is_ta_detected,
		mtk_pe20_get_is_connect(pinfo),
		mtk_pe_get_is_connect(pinfo));

	return sprintf(buf, "%u\n", is_ta_detected);
}

static DEVICE_ATTR_RO(Pump_Express, 0444, Pump_Express_show, NULL);

static ssize_t input_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] %s : %x\n",
		__func__, pinfo->chg1_data.thermal_input_current_limit);
	return sprintf(buf, "%u\n",
			pinfo->chg1_data.thermal_input_current_limit);
}

static ssize_t input_current_store(struct device *dev,
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
static DEVICE_ATTR_RW(input_current, 0644, input_current_show,
		input_current_store);

static ssize_t chg1_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] %s : %x\n",
		__func__, pinfo->chg1_data.thermal_charging_current_limit);
	return sprintf(buf, "%u\n",
			pinfo->chg1_data.thermal_charging_current_limit);
}

static ssize_t chg1_current_store(struct device *dev,
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
static DEVICE_ATTR_RW(chg1_current, 0644, chg1_current_show,
		chg1_current_store);

static ssize_t chg2_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] %s : %x\n",
		__func__, pinfo->chg2_data.thermal_charging_current_limit);
	return sprintf(buf, "%u\n",
			pinfo->chg2_data.thermal_charging_current_limit);
}

static ssize_t chg2_current_store(struct device *dev,
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
static DEVICE_ATTR_RW(chg2_current, 0644, chg2_current_show,
		chg2_current_store);

static ssize_t BatNotify_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] show_BatteryNotify: 0x%x\n", pinfo->notify_code);

	return sprintf(buf, "%u\n", pinfo->notify_code);
}

static ssize_t BatNotify_store(struct device *dev,
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

static DEVICE_ATTR_RW(BatteryNotify, 0644, BatNotify_show, BatNotify_store);

static ssize_t BN_TestMode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_debug("[Battery] %s : %x\n", __func__, pinfo->notify_test_mode);
	return sprintf(buf, "%u\n", pinfo->notify_test_mode);
}

static ssize_t BN_TestMode_store(struct device *dev,
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
static DEVICE_ATTR_RW(BN_TestMode, 0644, BN_TestMode_show, BN_TestMode_store);

static ssize_t ADC_Charger_Voltage_show(struct device *dev,
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

static DEVICE_ATTR_RO(ADC_Charger_Voltage, 0444, ADC_Charger_Voltage_show,
		NULL);

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
	char desc[32];
	int current_unlimited = 0;
	int cmd_discharging = 0;
	struct charger_manager *info = PDE_DATA(file_inode(file));

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

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
	char desc[32];
	unsigned int enable = 0;
	struct charger_manager *info = PDE_DATA(file_inode(file));


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

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
	char desc[32];
	unsigned int enable = 0;
	struct charger_manager *info = PDE_DATA(file_inode(file));

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

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
	if (ret)
		goto _out;

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		chr_err("[%s]: mkdir /proc/mtk_battery_cmd failed\n", __func__);
		return -ENOMEM;
	}

	proc_create_data("current_cmd", 0644, battery_dir,
			&mtk_chg_current_cmd_fops, info);
	proc_create_data("en_power_path", 0644, battery_dir,
			&mtk_chg_en_power_path_fops, info);
	proc_create_data("en_safety_timer", 0644, battery_dir,
			&mtk_chg_en_safety_timer_fops, info);

_out:
	return ret;
}

void notify_adapter_event(enum adapter_type type, enum adapter_event evt,
	void *val)
{
	chr_err("%s %d %d\n", __func__, type, evt);
	switch (type) {
	case MTK_PD_ADAPTER:
		switch (evt) {
		case  MTK_PD_CONNECT_NONE:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify Detach\n");
			pinfo->pd_type = MTK_PD_CONNECT_NONE;
			mutex_unlock(&pinfo->charger_pd_lock);
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
			break;

		case MTK_PD_CONNECT_PE_READY_SNK_PD30:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify PD30 ready\r\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
			mutex_unlock(&pinfo->charger_pd_lock);
			/* PD30 is ready */
			break;

		case MTK_PD_CONNECT_PE_READY_SNK_APDO:
			mutex_lock(&pinfo->charger_pd_lock);
			chr_err("PD Notify APDO Ready\n");
			pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
			mutex_unlock(&pinfo->charger_pd_lock);
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
		};
	}
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

	proc_create("dump_log", 0644,
		charger_dir, &charger_dump_log_proc_fops);
}


static int mtk_charger_probe(struct platform_device *pdev)
{
	struct charger_manager *info = NULL;
	struct list_head *pos;
	struct list_head *phead = &consumer_head;
	struct charger_consumer *ptr;
	int ret;

	chr_err("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	pinfo = info;

	platform_set_drvdata(pdev, info);
	info->pdev = pdev;
	mtk_charger_parse_dt(info, &pdev->dev);

	mutex_init(&info->charger_lock);
	mutex_init(&info->charger_pd_lock);
	mutex_init(&info->cable_out_lock);
	atomic_set(&info->enable_kpoc_shdn, 1);
	wakeup_source_init(&info->charger_wakelock, "charger suspend wakelock");
	spin_lock_init(&info->slock);

	/* init thread */
	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	info->enable_dynamic_cv = true;

	info->chg1_data.thermal_charging_current_limit = -1;
	info->chg1_data.thermal_input_current_limit = -1;
	info->chg1_data.input_current_limit_by_aicl = -1;
	info->chg2_data.thermal_charging_current_limit = -1;
	info->chg2_data.thermal_input_current_limit = -1;

	info->sw_jeita.error_recovery_flag = true;

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

	if (mtk_pe_init(info) < 0)
		info->enable_pe_plus = false;

	if (mtk_pe20_init(info) < 0)
		info->enable_pe_2 = false;

	if (mtk_pe40_init(info) == false)
		info->enable_pe_4 = false;

	mtk_pdc_init(info);
	charger_ftm_init();
	mtk_charger_get_atm_mode(info);

#ifdef CONFIG_MTK_CHARGER_UNLIMITED
	info->usb_unlimited = true;
	info->enable_sw_safety_timer = false;
	charger_dev_enable_safety_timer(info->chg1_dev, false);
#endif

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
	info->chg1_consumer =
		charger_manager_get_by_name(&pdev->dev, "charger_port1");
	info->init_done = true;
	_wake_up_charger(info);

	return 0;
}

static int mtk_charger_remove(struct platform_device *dev)
{
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

static const struct of_device_id mtk_charger_of_match[] = {
	{.compatible = "mediatek,charger",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_charger_of_match);

struct platform_device charger_device = {
	.name = "charger",
	.id = -1,
};

static struct platform_driver charger_driver = {
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
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
