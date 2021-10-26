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

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>

#include <linux/of.h>
#include <linux/extcon.h>

#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/charger_type.h>
#include <pmic.h>
#include <tcpm.h>

#include "mtk_charger_intf.h"

void __attribute__((weak)) fg_charger_in_handler(void)
{
	pr_notice("%s not defined\n", __func__);
}

struct chg_type_info {
	struct device *dev;
	struct charger_consumer *chg_consumer;
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	bool tcpc_kpoc;
	/* Charger Detection */
	struct mutex chgdet_lock;
	bool chgdet_en;
	atomic_t chgdet_cnt;
	wait_queue_head_t waitq;
	struct task_struct *chgdet_task;
	struct workqueue_struct *pwr_off_wq;
	struct work_struct pwr_off_work;
	struct workqueue_struct *chg_in_wq;
	struct work_struct chg_in_work;
	bool ignore_usb;
	bool plugin;
	int cc_orientation;
	int typec_mode;
};

#ifdef CONFIG_FPGA_EARLY_PORTING
/*  FPGA */
int hw_charging_get_charger_type(void)
{
	return STANDARD_HOST;
}

#else

/* EVB / Phone */
static const char * const mtk_chg_type_name[] = {
	"Charger Unknown",
	"Standard USB Host",
	"Charging USB Host",
	"Non-standard Charger",
	"Standard Charger",
	"Apple 2.1A Charger",
	"Apple 1.0A Charger",
	"Apple 0.5A Charger",
	"Wireless Charger",
};

/* Power Supply */
struct mt_charger {
	struct device *dev;
	struct power_supply_desc chg_desc;
	struct power_supply_config chg_cfg;
	struct power_supply *chg_psy;
	struct power_supply_desc ac_desc;
	struct power_supply_config ac_cfg;
	struct power_supply *ac_psy;
	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;
	struct chg_type_info *cti;
	bool chg_online; /* Has charger in or not */
	enum charger_type chg_type;
};

enum product_name {
	PISSARRO,
	PISSARROPRO,
};

static int product_name = PISSARRO;

struct quick_charge_desc {
	enum power_supply_type psy_type;
	enum quick_charge_type type;
};

struct quick_charge_desc quick_charge_table[11] = {
	{ POWER_SUPPLY_TYPE_USB,		QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_DCP,		QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_CDP,		QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_ACA,		QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_FLOAT,		QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_PD,		QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP,		QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3,	QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS,	QUICK_CHARGE_FLASH },
	{ POWER_SUPPLY_TYPE_WIRELESS,		QUICK_CHARGE_FAST },
	{0, 0},
};

static int get_quick_charge_type(struct charger_manager *info)
{
	enum power_supply_type psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
	enum hvdcp3_type qc3_type = HVDCP3_NONE;
	union power_supply_propval pval = {0,};
	int charge_status = 0, i = 0;

	if (!info || !info->usb_psy || !info->xmusb350_psy)
		return QUICK_CHARGE_NORMAL;

	charge_status = charger_manager_get_charge_status();
	power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &pval);
	psy_type = pval.intval;
	power_supply_get_property(info->xmusb350_psy, POWER_SUPPLY_PROP_HVDCP3_TYPE, &pval);
	qc3_type = pval.intval;

	if (charge_status == POWER_SUPPLY_STATUS_DISCHARGING || info->tbat > 580)
		return QUICK_CHARGE_NORMAL;

	if (psy_type == POWER_SUPPLY_TYPE_USB_PD && info->pd_verifed) {
		if (info->apdo_max == 120 && product_name == PISSARROPRO)
			return QUICK_CHARGE_SUPER;
		else
			return QUICK_CHARGE_TURBE;
	}

	if (qc3_type == HVDCP3_27)
		return QUICK_CHARGE_FLASH;

	while (quick_charge_table[i].psy_type != 0) {
		if (psy_type == quick_charge_table[i].psy_type) {
			return quick_charge_table[i].type;
		}

		i++;
	}

	return QUICK_CHARGE_NORMAL;
}

static int mt_charger_online(struct mt_charger *mtk_chg)
{
	int ret = 0;
	int boot_mode = 0;

	if (!mtk_chg->chg_online) {
		boot_mode = get_boot_mode();
		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_notice("%s: Unplug Charger/USB\n", __func__);
			pr_notice("%s: system_state=%d\n", __func__,
				system_state);
			if (system_state != SYSTEM_POWER_OFF)
				kernel_power_off();
		}
	}

	return ret;
}

static int mt_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	struct charger_manager *info = NULL;

	if (mtk_chg->cti->chg_consumer)
		info = mtk_chg->cti->chg_consumer->cm;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		if (info && info->input_suspend)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = mtk_chg->chg_type;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_charger_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	struct chg_type_info *cti = NULL;

	if (!mtk_chg) {
		pr_notice("%s: no mtk chg data\n", __func__);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mtk_chg->chg_online = val->intval;
		mt_charger_online(mtk_chg);
		return 0;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		mtk_chg->chg_type = val->intval;
		break;
	default:
		return -EINVAL;
	}

	cti = mtk_chg->cti;
	if (!cti->ignore_usb && is_usb_rdy()) {
		if (mtk_chg->chg_type == STANDARD_HOST || mtk_chg->chg_type == CHARGING_HOST || mtk_chg->chg_type == NONSTANDARD_CHARGER)
			mt_usb_connect();
		else
			mt_usb_disconnect();
	}

	queue_work(cti->chg_in_wq, &cti->chg_in_work);

	power_supply_changed(mtk_chg->ac_psy);
	power_supply_changed(mtk_chg->usb_psy);

	return 0;
}

static int mt_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	struct charger_manager *info = NULL;

	if (mtk_chg->cti->chg_consumer)
		info = mtk_chg->cti->chg_consumer->cm;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		if (mtk_chg->chg_type == STANDARD_HOST || mtk_chg->chg_type == CHARGING_HOST)
			val->intval = 0;
		if (info && info->input_suspend)
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_usb_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_PD_AUTHENTICATION:
	case POWER_SUPPLY_PROP_APDO_MAX:
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	struct charger_manager *info = NULL;

	if (!mtk_chg) {
		chr_err("failed to init mt_charger\n");
		return -EINVAL;
	}

	if (mtk_chg->cti->chg_consumer)
		info = mtk_chg->cti->chg_consumer->cm;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (mtk_chg->chg_type == STANDARD_HOST || mtk_chg->chg_type == CHARGING_HOST)
			val->intval = 1;
		else
			val->intval = 0;
		if (info && info->input_suspend)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (info && info->chg1_dev)
			charger_dev_get_vbus(info->chg1_dev, &val->intval);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = mtk_chg->usb_desc.type;
		break;
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		val->intval = get_quick_charge_type(info);
		break;
	case POWER_SUPPLY_PROP_PD_AUTHENTICATION:
		if (info)
			val->intval = info->pd_verifed;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PD_VERIFY_DONE:
		if (info)
			val->intval = info->pd_verify_done;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PD_TYPE:
		if (info)
			val->intval = info->pd_type;
		else
			val->intval = MTK_PD_CONNECT_NONE;
		break;
	case POWER_SUPPLY_PROP_APDO_MAX:
		if (info)
			val->intval = info->apdo_max;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_POWER_MAX:
		if (info)
			val->intval = info->apdo_max;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		val->intval = 1 + mtk_chg->cti->cc_orientation;
		pr_err("typec_cc_orientation = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		val->intval = mtk_chg->cti->typec_mode;
		break;
	case POWER_SUPPLY_PROP_FFC_ENABLE:
		if (info)
			val->intval = info->ffc_enable;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
		if (info && info->pmic_dev) {
			if (!info->fake_typec_temp)
				charger_dev_get_ts_temp(info->pmic_dev, &val->intval);
			else
				val->intval = info->fake_typec_temp;
		} else {
			val->intval = 250;
		}
		break;
	case POWER_SUPPLY_PROP_TYPEC_BURN:
		if (info)
			val->intval = info->typec_burn;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_SW_CV:
		if (info)
			val->intval = info->sw_cv;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		if (info)
			val->intval = info->input_suspend;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_JEITA_CHG_INDEX:
		if (info)
			val->intval = info->jeita_chg_index[0];
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CV_WA_COUNT:
		if (info)
			val->intval = info->cv_wa_count;
		else
			val->intval = 0;
        break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 0;
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		break;
#ifdef CONFIG_FACTORY_BUILD
	case POWER_SUPPLY_PROP_CP_VBUS:
		if (info && info->chg1_dev)
			charger_dev_get_vbus(info->chg1_dev, &val->intval);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_IBUS_MASTER:
		if (info && info->cp_master)
			charger_dev_get_ibus(info->cp_master, &val->intval);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_IBUS_SLAVE:
		if (info && info->cp_slave)
			charger_dev_get_ibus(info->cp_slave, &val->intval);
		else
			val->intval = 0;
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_usb_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	struct charger_manager *info = NULL;

	if (!mtk_chg) {
		chr_err("failed to init mt_charger\n");
		return -EINVAL;
	}

	if (mtk_chg->cti->chg_consumer)
		info = mtk_chg->cti->chg_consumer->cm;

	switch (psp) {
	case POWER_SUPPLY_PROP_REAL_TYPE:
		mtk_chg->usb_desc.type = val->intval;
		break;
	case POWER_SUPPLY_PROP_PD_AUTHENTICATION:
		if (info)
			info->pd_verifed = val->intval;
		if(mtk_chg->usb_psy)
			power_supply_changed(mtk_chg->usb_psy);
		break;
	case POWER_SUPPLY_PROP_PD_VERIFY_DONE:
		if (info)
			info->pd_verify_done = val->intval;
		break;
	case POWER_SUPPLY_PROP_APDO_MAX:
		if (info)
			info->apdo_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
		if (info)
			info->fake_typec_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		if (info)
			info->input_suspend = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property mt_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PD_AUTHENTICATION,
	POWER_SUPPLY_PROP_PD_VERIFY_DONE,
	POWER_SUPPLY_PROP_PD_TYPE,
	POWER_SUPPLY_PROP_APDO_MAX,
	POWER_SUPPLY_PROP_POWER_MAX,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_FFC_ENABLE,
	POWER_SUPPLY_PROP_CONNECTOR_TEMP,
	POWER_SUPPLY_PROP_TYPEC_BURN,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_JEITA_CHG_INDEX,
	POWER_SUPPLY_PROP_CV_WA_COUNT,
	POWER_SUPPLY_PROP_PRESENT,
#ifdef CONFIG_FACTORY_BUILD
	POWER_SUPPLY_PROP_CP_VBUS,
	POWER_SUPPLY_PROP_CP_IBUS_MASTER,
	POWER_SUPPLY_PROP_CP_IBUS_SLAVE,
#endif
};

static void tcpc_power_off_work_handler(struct work_struct *work)
{
	struct charger_manager *info = NULL;
	struct chg_type_info *cti = container_of(work, struct chg_type_info, pwr_off_work);
	struct timespec time;
	enum power_supply_type psy_type = POWER_SUPPLY_TYPE_UNKNOWN;
	int vbus = 0;

	pr_info("%s\n", __func__);

	if (cti && cti->chg_consumer)
		info = cti->chg_consumer->cm;

	get_monotonic_boottime(&time);
	if (time.tv_sec <= 15) {
		if (info && info->chg1_dev && info->chg2_dev) {
			psy_type = charger_dev_get_first_charger_type(info->chg2_dev);
			if (psy_type == POWER_SUPPLY_TYPE_USB_DCP || psy_type == POWER_SUPPLY_TYPE_USB_CDP || psy_type == POWER_SUPPLY_TYPE_USB_HVDCP) {
				msleep(3500);
				charger_dev_get_vbus(info->chg1_dev, &vbus);
				if (vbus > 3500)
					return;
			}
		}
	}

	kernel_power_off();
}

static void charger_in_work_handler(struct work_struct *work)
{
	mtk_charger_int_handler();
	fg_charger_in_handler();
}

#ifdef CONFIG_TCPC_CLASS
static void plug_in_out_handler(struct chg_type_info *cti, bool en, bool ignore)
{
	mutex_lock(&cti->chgdet_lock);
	cti->chgdet_en = en;
	cti->ignore_usb = ignore;
	cti->plugin = en;
	atomic_inc(&cti->chgdet_cnt);
	wake_up_interruptible(&cti->waitq);
	mutex_unlock(&cti->chgdet_lock);
}

static int get_source_mode(struct tcp_notify *noti)
{
	/* if source is debug acc src A to C cable report typec mode as default */
	if (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC)
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;

	switch (noti->typec_state.rp_level) {
	case TYPEC_CC_VOLT_SNK_1_5:
		return POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case TYPEC_CC_VOLT_SNK_3_0:
		return POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	case TYPEC_CC_VOLT_SNK_DFT:
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct chg_type_info *cti = container_of(pnb,
		struct chg_type_info, pd_nb);
	int vbus = 0;

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			cti->cc_orientation = noti->typec_state.polarity;
			cti->typec_mode = get_source_mode(noti);
			pr_info("%s USB Plug in, cc_orientation = %d\n", __func__, cti->cc_orientation);
			plug_in_out_handler(cti, true, false);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (cti->tcpc_kpoc) {
				vbus = battery_get_vbus();
				pr_info("%s KPOC Plug out, vbus = %d\n",
					__func__, vbus);
				queue_work_on(cpumask_first(cpu_online_mask),
					      cti->pwr_off_wq,
					      &cti->pwr_off_work);
				break;
			}
			pr_info("%s USB Plug out\n", __func__);
			cti->typec_mode = POWER_SUPPLY_TYPEC_NONE;
			plug_in_out_handler(cti, false, false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("%s Source_to_Sink\n", __func__);
			cti->typec_mode = POWER_SUPPLY_TYPEC_SINK;
			plug_in_out_handler(cti, true, true);
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s Sink_to_Source\n", __func__);
			cti->typec_mode = get_source_mode(noti);
			plug_in_out_handler(cti, false, true);
		}
		cti->cc_orientation = noti->typec_state.polarity;
		break;
	}
	return NOTIFY_OK;
}
#endif

static int chgdet_task_threadfn(void *data)
{
	struct chg_type_info *cti = data;
	bool attach = false;
	int ret = 0;

	pr_info("%s: ++\n", __func__);
	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(cti->waitq,
					     atomic_read(&cti->chgdet_cnt) > 0);
		if (ret < 0) {
			pr_info("%s: wait event been interrupted(%d)\n",
				__func__, ret);
			continue;
		}

		pm_stay_awake(cti->dev);
		mutex_lock(&cti->chgdet_lock);
		atomic_set(&cti->chgdet_cnt, 0);
		attach = cti->chgdet_en;
		mutex_unlock(&cti->chgdet_lock);

#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
		if (cti->chg_consumer)
			charger_manager_enable_chg_type_det(cti->chg_consumer,
							attach);
#else
		mtk_pmic_enable_chr_type_det(attach);
#endif
		pm_relax(cti->dev);
	}
	pr_info("%s: --\n", __func__);
	return 0;
}

static void mtk_charger_parse_cmdline(void)
{
	char *pissarro = NULL, *pissarropro = NULL, *pissarroinpro = NULL;

	pissarro = strnstr(saved_command_line, "pissarro", strlen(saved_command_line));
	pissarropro = strnstr(saved_command_line, "pissarropro", strlen(saved_command_line));
	pissarroinpro = strnstr(saved_command_line, "pissarroinpro", strlen(saved_command_line));

	chr_info("pissarro = %d, pissarropro = %d, pissarroinpro = %d\n", pissarro ? 1 : 0, pissarropro ? 1 : 0, pissarroinpro ? 1 : 0);

	if (pissarropro || pissarroinpro)
		product_name = PISSARROPRO;
	else if (pissarro)
		product_name = PISSARRO;
}

static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct chg_type_info *cti = NULL;
	struct mt_charger *mt_chg = NULL;

	pr_info("%s\n", __func__);

	mtk_charger_parse_cmdline();

	mt_chg = devm_kzalloc(&pdev->dev, sizeof(*mt_chg), GFP_KERNEL);
	if (!mt_chg)
		return -ENOMEM;

	mt_chg->dev = &pdev->dev;
	mt_chg->chg_online = false;
	mt_chg->chg_type = CHARGER_UNKNOWN;

	mt_chg->chg_desc.name = "charger";
	mt_chg->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	mt_chg->chg_desc.properties = mt_charger_properties;
	mt_chg->chg_desc.num_properties = ARRAY_SIZE(mt_charger_properties);
	mt_chg->chg_desc.set_property = mt_charger_set_property;
	mt_chg->chg_desc.get_property = mt_charger_get_property;
	mt_chg->chg_cfg.drv_data = mt_chg;

	mt_chg->ac_desc.name = "ac";
	mt_chg->ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	mt_chg->ac_desc.properties = mt_ac_properties;
	mt_chg->ac_desc.num_properties = ARRAY_SIZE(mt_ac_properties);
	mt_chg->ac_desc.get_property = mt_ac_get_property;
	mt_chg->ac_cfg.drv_data = mt_chg;

	mt_chg->usb_desc.name = "usb";
	mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	mt_chg->usb_desc.properties = mt_usb_properties;
	mt_chg->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	mt_chg->usb_desc.property_is_writeable = mt_usb_is_writeable,
	mt_chg->usb_desc.get_property = mt_usb_get_property;
	mt_chg->usb_desc.set_property = mt_usb_set_property;
	mt_chg->usb_cfg.drv_data = mt_chg;

	mt_chg->chg_psy = power_supply_register(&pdev->dev,
		&mt_chg->chg_desc, &mt_chg->chg_cfg);
	if (IS_ERR(mt_chg->chg_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->chg_psy));
		ret = PTR_ERR(mt_chg->chg_psy);
		return ret;
	}

	mt_chg->ac_psy = power_supply_register(&pdev->dev, &mt_chg->ac_desc,
		&mt_chg->ac_cfg);
	if (IS_ERR(mt_chg->ac_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->ac_psy));
		ret = PTR_ERR(mt_chg->ac_psy);
		goto err_ac_psy;
	}

	mt_chg->usb_psy = power_supply_register(&pdev->dev, &mt_chg->usb_desc,
		&mt_chg->usb_cfg);
	if (IS_ERR(mt_chg->usb_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->usb_psy));
		ret = PTR_ERR(mt_chg->usb_psy);
		goto err_usb_psy;
	}

	cti = devm_kzalloc(&pdev->dev, sizeof(*cti), GFP_KERNEL);
	if (!cti) {
		ret = -ENOMEM;
		goto err_no_mem;
	}
	cti->dev = &pdev->dev;

	cti->chg_consumer = charger_manager_get_by_name(cti->dev,
							"charger_port1");
	if (!cti->chg_consumer) {
		pr_info("%s: get charger consumer device failed\n", __func__);
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}

	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    ret == LOW_POWER_OFF_CHARGING_BOOT)
		cti->tcpc_kpoc = true;
	pr_info("%s KPOC(%d)\n", __func__, cti->tcpc_kpoc);

	mutex_init(&cti->chgdet_lock);
	atomic_set(&cti->chgdet_cnt, 0);

	init_waitqueue_head(&cti->waitq);
	cti->chgdet_task = kthread_run(
				chgdet_task_threadfn, cti, "chgdet_thread");
	ret = PTR_ERR_OR_ZERO(cti->chgdet_task);
	if (ret < 0) {
		pr_info("%s: create chg det work fail\n", __func__);
		return ret;
	}

	/* Init power off work */
	cti->pwr_off_wq = create_singlethread_workqueue("tcpc_power_off");
	INIT_WORK(&cti->pwr_off_work, tcpc_power_off_work_handler);

	cti->chg_in_wq = create_singlethread_workqueue("charger_in");
	INIT_WORK(&cti->chg_in_work, charger_in_work_handler);

	mt_chg->cti = cti;
	platform_set_drvdata(pdev, mt_chg);
	device_init_wakeup(&pdev->dev, true);

	pr_info("%s done\n", __func__);
	return 0;

err_get_tcpc_dev:
	devm_kfree(&pdev->dev, cti);
err_no_mem:
	power_supply_unregister(mt_chg->usb_psy);
err_usb_psy:
	power_supply_unregister(mt_chg->ac_psy);
err_ac_psy:
	power_supply_unregister(mt_chg->chg_psy);
	return ret;
}

static int mt_charger_remove(struct platform_device *pdev)
{
	struct mt_charger *mt_charger = platform_get_drvdata(pdev);
	struct chg_type_info *cti = mt_charger->cti;

	power_supply_unregister(mt_charger->chg_psy);
	power_supply_unregister(mt_charger->ac_psy);
	power_supply_unregister(mt_charger->usb_psy);

	pr_info("%s\n", __func__);
	if (cti->chgdet_task) {
		kthread_stop(cti->chgdet_task);
		atomic_inc(&cti->chgdet_cnt);
		wake_up_interruptible(&cti->waitq);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mt_charger_suspend(struct device *dev)
{
	/* struct mt_charger *mt_charger = dev_get_drvdata(dev); */
	return 0;
}

static int mt_charger_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mt_charger *mt_charger = platform_get_drvdata(pdev);

	if (!mt_charger) {
		pr_info("%s: get mt_charger failed\n", __func__);
		return -ENODEV;
	}

	power_supply_changed(mt_charger->chg_psy);
	power_supply_changed(mt_charger->ac_psy);
	power_supply_changed(mt_charger->usb_psy);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mt_charger_pm_ops, mt_charger_suspend,
	mt_charger_resume);

static const struct of_device_id mt_charger_match[] = {
	{ .compatible = "mediatek,mt-charger", },
	{ },
};
static struct platform_driver mt_charger_driver = {
	.probe = mt_charger_probe,
	.remove = mt_charger_remove,
	.driver = {
		.name = "mt-charger-det",
		.owner = THIS_MODULE,
		.pm = &mt_charger_pm_ops,
		.of_match_table = mt_charger_match,
	},
};

/* Legacy api to prevent build error */
bool upmu_is_chr_det(void)
{
	struct mt_charger *mtk_chg = NULL;
	struct power_supply *psy = power_supply_get_by_name("charger");

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	return mtk_chg->chg_online;
}

/* Legacy api to prevent build error */
bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det())
		return true;

	pr_notice("%s: No charger\n", __func__);
	return false;
}

enum charger_type mt_get_charger_type(void)
{
	struct mt_charger *mtk_chg = NULL;
	struct power_supply *psy = power_supply_get_by_name("charger");

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	return mtk_chg->chg_type;
}

bool mt_charger_plugin(void)
{
	struct mt_charger *mtk_chg = NULL;
	struct power_supply *psy = power_supply_get_by_name("charger");
	struct chg_type_info *cti = NULL;

	if (!psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	mtk_chg = power_supply_get_drvdata(psy);
	cti = mtk_chg->cti;
	pr_info("%s plugin:%d\n", __func__, cti->plugin);

	return cti->plugin;
}

static s32 __init mt_charger_det_init(void)
{
	return platform_driver_register(&mt_charger_driver);
}

static void __exit mt_charger_det_exit(void)
{
	platform_driver_unregister(&mt_charger_driver);
}

subsys_initcall(mt_charger_det_init);
module_exit(mt_charger_det_exit);

#ifdef CONFIG_TCPC_CLASS
static int __init mt_charger_det_notifier_call_init(void)
{
	int ret = 0;
	struct power_supply *psy = power_supply_get_by_name("charger");
	struct mt_charger *mt_chg = NULL;
	struct chg_type_info *cti = NULL;

	if (!psy) {
		pr_notice("%s: get power supply fail\n", __func__);
		return -ENODEV;
	}
	mt_chg = power_supply_get_drvdata(psy);
	cti = mt_chg->cti;

	cti->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (cti->tcpc_dev == NULL) {
		pr_notice("%s: get tcpc dev fail\n", __func__);
		ret = -ENODEV;
		goto out;
	}
	cti->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(cti->tcpc_dev,
		&cti->pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_notice("%s: register tcpc notifier fail(%d)\n",
			  __func__, ret);
		goto out;
	}
	pr_info("%s done\n", __func__);
out:
	power_supply_put(psy);
	return ret;
}
late_initcall(mt_charger_det_notifier_call_init);
#endif

MODULE_DESCRIPTION("mt-charger-detection");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");

#endif /* CONFIG_MTK_FPGA */
