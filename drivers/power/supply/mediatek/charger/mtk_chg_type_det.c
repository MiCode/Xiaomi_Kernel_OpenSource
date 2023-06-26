/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/gpio.h>
#include <linux/of_gpio.h>
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

#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/charger_type.h>
#include <pmic.h>
#include <tcpm.h>
#include "../../../../misc/mediatek/typec/tcpc/inc/tcpci_core.h"
#include "mtk_charger_intf.h"

#ifdef CONFIG_MTK_REVERSE_CHG_ENABLE
#define REVERSE_CHG_SOURCE				0X01
#define REVERSE_CHG_SINK				0X02
#define REVERSE_CHG_DRP					0X03
#define REVERSE_CHG_TEST				0X04
struct pinctrl *reverse_pinctrl;
struct pinctrl_state *reverse_enable;
struct pinctrl_state *reverse_disable;
int dpdm_disshort;
int reverse_gpio;
int reverse_flage;
extern int is_otg;
#endif
extern bool usb_otg;
extern enum hvdcp_status hvdcp_type_tmp;
int call_mode = -1;
uint8_t  typec_cc_orientation;


void __attribute__((weak)) fg_charger_in_handler(void)
{
	pr_notice("%s not defined\n", __func__);
}

#if 0
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
};

#endif

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
	"HVDCP_CHARGER",
};

static void dump_charger_name(enum charger_type type)
{
	switch (type) {
	case CHARGER_UNKNOWN:
	case STANDARD_HOST:
	case CHARGING_HOST:
	case NONSTANDARD_CHARGER:
	case STANDARD_CHARGER:
	case APPLE_2_1A_CHARGER:
	case APPLE_1_0A_CHARGER:
	case APPLE_0_5A_CHARGER:
	case HVDCP_CHARGER:
		pr_info("%s: charger type: %d, %s\n", __func__, type,
			mtk_chg_type_name[type]);
		break;
	default:
		pr_info("%s: charger type: %d, Not Defined!!!\n", __func__,
			type);
		break;
	}
}

#if 0
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
#endif

#ifdef CONFIG_MTK_REVERSE_CHG_ENABLE
void reverse_charger(bool en)
{
	int gpio_state;
	static struct charger_device *primary_charger;
	struct tcpc_device *tcpc = tcpc_dev_get_by_name("type_c_port0");

	primary_charger = get_charger_by_name("primary_chg");
	pr_err("dhx---is otg : %d\n", is_otg);
	if (!primary_charger) {
		pr_err("primary charger otg is null\n");
		return;
	}
	if (en) {
		reverse_flage = 1;
		charger_dev_enable_otg(primary_charger, false);
		msleep(1000);
		pinctrl_select_state(reverse_pinctrl, reverse_enable);
		gpio_state = gpio_get_value(reverse_gpio);
		pr_err("dhx-- short DM/DM gpio: %d\n", gpio_state);
		dpdm_disshort = 1;
		msleep(1000);
		if (is_otg == 1)
			charger_dev_enable_otg(primary_charger, true);
		tcpc->ops->set_role(tcpc, REVERSE_CHG_SOURCE);
	} else {
		// reverse_flage = 1;
		// msleep(200);
		pinctrl_select_state(reverse_pinctrl, reverse_disable);
		dpdm_disshort = 0;
		gpio_state = gpio_get_value(reverse_gpio);
		pr_err("dhx-- dis short DM/DM gpio: %d\n", gpio_state);
		charger_dev_enable_otg(primary_charger, false);
		tcpc->ops->set_role(tcpc, REVERSE_CHG_DRP);
		// if (is_otg == 1){
		// 	pr_err("dhx---is otg\n");
		// 	charger_dev_enable_otg(primary_charger, true);
		// }
	}
	msleep(500);
	reverse_flage = 0;
}
#endif

static int mt_charger_online(struct mt_charger *mtk_chg)
{
	int ret = 0;
	int boot_mode = 0;
	int vbus = 0;
	if (!mtk_chg->chg_online) {
		boot_mode = get_boot_mode();
		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_notice("%s: Unplug1 Charger/USB\n", __func__);
			msleep(4000);
			vbus = battery_get_vbus();
			if (vbus > 3000) {
				pr_err("vbus is hight return\n");
				return 0;
			}
			pr_err("vbus is hight return %d\n", vbus);
			pr_notice("%s: system_state2=%d\n", __func__,
				system_state);
			machine_power_off();
		}
	}

	return ret;
}

/* Power Supply Functions */
static int mt_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = mtk_chg->chg_type;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		/*K19A HQ-129052 K19A charger of thermal current limit by wangqi at 2021/5/14 start*/
		val->intval = call_mode;
		/*K19A HQ-129052 K19A charger of thermal current limit by wangqi at 2021/5/14 end*/
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
	struct chg_type_info *cti;

	pr_info("%s\n", __func__);

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
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		call_mode = val->intval;
		break;
	default:
		return -EINVAL;
	}

	dump_charger_name(mtk_chg->chg_type);

	cti = mtk_chg->cti;
	if (!cti->ignore_usb) {
		/* usb */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST) ||
			(mtk_chg->chg_type == HVDCP_CHARGER) ||
			(mtk_chg->chg_type == NONSTANDARD_CHARGER))
			mt_usb_connect();
		else
			mt_usb_disconnect();
	}

	queue_work(cti->chg_in_wq, &cti->chg_in_work);

	power_supply_changed(mtk_chg->usb_psy);
	power_supply_changed(mtk_chg->ac_psy);

	return 0;
}

static int mt_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		/* Reset to 0 if charger type is USB */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
/*K19A HQ-124188 provide node quick_charge_type by miaozhichao at 2021/4/26 start*/
enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,
	QUICK_CHARGE_FAST,
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURPE,
	QUICK_CHARGE_MAX,
};
/*K19A HQ-124188 provide node quick_charge_type by miaozhichao at 2021/4/26 end*/
static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	/*K19A HQ-134474 K19A for typec mode by langjunjun at 2021/6/1 start*/
	struct tcpc_device *tcpc = tcpc_dev_get_by_name("type_c_port0");
	/*K19A HQ-134474 K19A for typec mode by langjunjun at 2021/6/1 end*/
	int typec_mode = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_USB_OTG:
		if (usb_otg)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		/*K19A HQ-134474 K19A for typec mode by langjunjun at 2021/6/1 start*/
		if (tcpc && tcpc->ops ) {
			tcpc->ops->get_mode(tcpc, &typec_mode);
			pr_err("dev %s get mode =  %d\n",tcpc->desc.name,typec_mode);
		} else {
			pr_err("tcpc or tcpc->ops is NULL\n");
		}
		/*K19A HQ-134474 K19A for typec mode by langjunjun at 2021/6/1 end*/
		val->intval = typec_mode;
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		val->intval = typec_cc_orientation;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		pr_err("dhx--hvdcp:%d\n", hvdcp_type_tmp);
		if (hvdcp_type_tmp == HVDCP_3) {
			val->intval = POWER_SUPPLY_TYPE_USB_HVDCP_3;
			break;
		} else if (hvdcp_type_tmp == HVDCP) {
			val->intval = POWER_SUPPLY_TYPE_USB_HVDCP;
			break;
		}
		switch (mtk_chg->chg_type) {
		case  STANDARD_HOST:
			val->intval = POWER_SUPPLY_TYPE_USB;
			break;
		case  CHARGING_HOST:
			val->intval = POWER_SUPPLY_TYPE_USB_CDP;
			break;
		case  STANDARD_CHARGER:
			val->intval = POWER_SUPPLY_TYPE_USB_DCP;
			break;
		case  HVDCP_CHARGER:
			val->intval = POWER_SUPPLY_TYPE_USB_HVDCP;
			break;
/* Huaqin add  for HQ-134476  by miaozhichao 2021/5/28 start */
		case NONSTANDARD_CHARGER:
			val->intval = POWER_SUPPLY_TYPE_USB_FLOAT;
			break;
/* Huaqin add  for HQ-134476  by miaozhichao 2021/5/28 end */
		default:
			val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
	}
		break;
#ifdef CONFIG_MTK_REVERSE_CHG_ENABLE
	case POWER_SUPPLY_PROP_REVERSE_CHG_OTG:
		val->intval = dpdm_disshort;
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_STATUS:
		val->intval = gpio_get_value(reverse_gpio);
		break;
#endif
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery_get_vbus();
		break;
/*K19A HQ-124188 provide node quick_charge_type by miaozhichao at 2021/4/26 start*/
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		if (hvdcp_type_tmp == HVDCP_3) {
			val->intval = QUICK_CHARGE_FAST;
			break;
		} else if (hvdcp_type_tmp == HVDCP) {
			val->intval = QUICK_CHARGE_FAST;
			break;
		}
		switch (mtk_chg->chg_type) {
		case  STANDARD_HOST:
			val->intval = QUICK_CHARGE_NORMAL;
			break;
		case  CHARGING_HOST:
			val->intval = QUICK_CHARGE_NORMAL;
			break;
		case  STANDARD_CHARGER:
			val->intval = QUICK_CHARGE_NORMAL;
			break;
		case  HVDCP_CHARGER:
			val->intval = QUICK_CHARGE_FAST;
			break;
		default:
			val->intval = QUICK_CHARGE_NORMAL;
			break;
	}
		break;
/*K19A HQ-124188 provide node quick_charge_type by miaozhichao at 2021/4/26 end*/
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_chg_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int mt_usb_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
#ifdef CONFIG_MTK_REVERSE_CHG_ENABLE
	case POWER_SUPPLY_PROP_REVERSE_CHG_OTG:
		rc = 1;
		break;
#endif
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int mt_usb_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	switch (psp) {
#ifdef CONFIG_MTK_REVERSE_CHG_ENABLE
	case POWER_SUPPLY_PROP_REVERSE_CHG_OTG:
		if (val->intval == 1)
			reverse_charger(true);
		else
			reverse_charger(false);
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int fake_temp;
static int mt_main_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	int vts, tchg_min, tchg_max;
	u32 input_current_now, ibat_6360;

	if (!mtk_chg->chg1_dev) {
		mtk_chg->chg1_dev = get_charger_by_name("primary_chg");
		if (mtk_chg->chg1_dev)
			chr_err("Found primary charger [%s]\n",
				mtk_chg->chg1_dev->props.alias_name);
		else {
			chr_err("*** Error : can't find primary charger ***\n");
			return 0;
		}
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		charger_dev_get_charging_current(mtk_chg->chg1_dev, &input_current_now);
		val->intval = input_current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger_dev_get_adc(mtk_chg->chg1_dev, ADC_CHANNEL_IBAT, &ibat_6360, &ibat_6360);
		val->intval = -ibat_6360;
		break;
	case POWER_SUPPLY_PROP_TEMP_MAX:
		charger_dev_get_temperature(mtk_chg->chg1_dev, &tchg_min, &tchg_max);
		if (!fake_temp)
			val->intval = tchg_max;
		else
			val->intval = fake_temp;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		charger_dev_get_charging_current(mtk_chg->chg1_dev, &input_current_now);
		val->intval = input_current_now;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 4450000;
		break;
	case POWER_SUPPLY_PROP_TEMP_MIN:
		charger_dev_get_temperature(mtk_chg->chg1_dev, &tchg_min, &tchg_max);
		val->intval = tchg_min;
		break;
	case POWER_SUPPLY_PROP_TEMP_CONNECT:
		charger_dev_get_adc(mtk_chg->chg1_dev, ADC_CHANNEL_TS, &vts, &vts);
		val->intval = vts;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		charger_dev_get_input_current(mtk_chg->chg1_dev, &input_current_now);
		val->intval = input_current_now;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_main_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP_MAX:
		fake_temp = val->intval;
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_main_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP_MAX:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static enum power_supply_property mt_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
};

static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_USB_OTG,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
#ifdef CONFIG_MTK_REVERSE_CHG_ENABLE
	POWER_SUPPLY_PROP_REVERSE_CHG_OTG,
	POWER_SUPPLY_PROP_REVERSE_CHG_STATUS,
#endif
/*K19A HQ-124188 provide node quick_charge_type by miaozhichao at 2021/4/26 start*/
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
/*K19A HQ-124188 provide node quick_charge_type by miaozhichao at 2021/4/26 end*/
};

static enum power_supply_property mt_main_properties[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP_MAX,
	POWER_SUPPLY_PROP_TEMP_MIN,
	POWER_SUPPLY_PROP_TEMP_CONNECT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
};

static void tcpc_power_off_work_handler(struct work_struct *work)
{
	pr_info("%s\n", __func__);
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
	charger_manager_enable_chg_type_det(en);
	// atomic_inc(&cti->chgdet_cnt);
	// wake_up_interruptible(&cti->waitq);
	mutex_unlock(&cti->chgdet_lock);
}

static void notify_plug_out(void)
{
	union power_supply_propval propval;
	int ret;
	struct power_supply *charger_psy = power_supply_get_by_name("charger");

	if (charger_psy == NULL)
		return;
	propval.intval = CHARGER_UNKNOWN;
	ret = power_supply_set_property(charger_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);
	propval.intval = !!(0);
	ret = power_supply_set_property(charger_psy,
					POWER_SUPPLY_PROP_ONLINE, &propval);
}

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct chg_type_info *cti = container_of(pnb,
	struct chg_type_info, pd_nb);

	static struct charger_device *primary_charger;
	primary_charger = get_charger_by_name("primary_chg");
	if (!primary_charger) {
		pr_err("primary charger notifier is null\n");
		return 0;
	}
	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_info("%s USB Plug in, pol = %d\n", __func__,
					noti->typec_state.polarity);
			plug_in_out_handler(cti, true, false);
		} else if (noti->typec_state.new_state == TYPEC_UNATTACHED) {
			pr_info("%s USB Plug out\n", __func__);
			notify_plug_out();
			charger_dev_enable_otg(primary_charger, false);
			plug_in_out_handler(cti, false, false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("%s Source_to_Sink\n", __func__);
			charger_dev_enable_otg(primary_charger, false);
			plug_in_out_handler(cti, true, true);
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s Sink_to_Source\n", __func__);
			plug_in_out_handler(cti, false, true);
		}
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
			charger_manager_enable_chg_type_det(attach);
#else
		mtk_pmic_enable_chr_type_det(attach);
#endif
		pm_relax(cti->dev);
	}
	pr_info("%s: --\n", __func__);
	return 0;
}

static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct chg_type_info *cti = NULL;
	struct mt_charger *mt_chg = NULL;

	pr_info("%s\n", __func__);

	mt_chg = devm_kzalloc(&pdev->dev, sizeof(*mt_chg), GFP_KERNEL);
	if (!mt_chg)
		return -ENOMEM;

	mt_chg->dev = &pdev->dev;
	mt_chg->chg_online = false;
	mt_chg->chg_type = CHARGER_UNKNOWN;
	mt_chg->chg1_dev = NULL;
#ifdef CONFIG_MTK_REVERSE_CHG_ENABLE
	reverse_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(reverse_pinctrl)) {
		pr_err("Failed to get reverse_pinctrl.\n");
		ret = PTR_ERR(reverse_pinctrl);
		return ret;
	}

	reverse_enable = pinctrl_lookup_state(
			reverse_pinctrl, "reverse_high");
	if (IS_ERR(reverse_enable)) {
		pr_err("Failed to init reverse_high\n");
		ret = PTR_ERR(reverse_enable);
	}
	reverse_disable = pinctrl_lookup_state(
			reverse_pinctrl, "reverse_low");
	if (IS_ERR(reverse_disable)) {
		pr_err("Failed to init reverse_low\n");
		ret = PTR_ERR(reverse_disable);
	}

	reverse_gpio = of_get_named_gpio(pdev->dev.of_node, "reverse-gpio", 0);
	pr_err("dhx--rever gpio: %d\n", reverse_gpio);
#endif
	mt_chg->chg_desc.name = "charger";
	mt_chg->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	mt_chg->chg_desc.properties = mt_charger_properties;
	mt_chg->chg_desc.num_properties = ARRAY_SIZE(mt_charger_properties);
	mt_chg->chg_desc.set_property = mt_charger_set_property;
	mt_chg->chg_desc.get_property = mt_charger_get_property;
	mt_chg->chg_desc.property_is_writeable = mt_chg_is_writeable;
	mt_chg->chg_cfg.drv_data = mt_chg;

	mt_chg->ac_desc.name = "ac";
	mt_chg->ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	mt_chg->ac_desc.properties = mt_ac_properties;
	mt_chg->ac_desc.num_properties = ARRAY_SIZE(mt_ac_properties);
	mt_chg->ac_desc.get_property = mt_ac_get_property;
	mt_chg->ac_cfg.drv_data = mt_chg;

	mt_chg->usb_desc.name = "usb";
	mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB;
	mt_chg->usb_desc.properties = mt_usb_properties;
	mt_chg->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	mt_chg->usb_desc.get_property = mt_usb_get_property;
	mt_chg->usb_desc.set_property = mt_usb_set_property;
	mt_chg->usb_desc.property_is_writeable = mt_usb_is_writeable;
	mt_chg->usb_cfg.drv_data = mt_chg;

	mt_chg->main_desc.name = "main";
	mt_chg->main_desc.type = POWER_SUPPLY_TYPE_MAINS;
	mt_chg->main_desc.properties = mt_main_properties;
	mt_chg->main_desc.num_properties = ARRAY_SIZE(mt_main_properties);
	mt_chg->main_desc.get_property = mt_main_get_property;
	mt_chg->main_desc.set_property = mt_main_set_property;
	mt_chg->main_desc.property_is_writeable = mt_main_is_writeable;
	mt_chg->main_cfg.drv_data = mt_chg;

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

	mt_chg->main_psy = power_supply_register(&pdev->dev, &mt_chg->main_desc,
		&mt_chg->main_cfg);
	if (IS_ERR(mt_chg->main_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->main_psy));
		ret = PTR_ERR(mt_chg->main_psy);
		goto err_main_psy;
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

#ifdef CONFIG_TCPC_CLASS
	cti->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (cti->tcpc_dev == NULL) {
		pr_info("%s: tcpc device not ready, defer\n", __func__);
		ret = -EPROBE_DEFER;
		goto err_get_tcpc_dev;
	}
	cti->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(cti->tcpc_dev,
		&cti->pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_info("%s: register tcpc notifer fail\n", __func__);
		ret = -EINVAL;
		goto err_get_tcpc_dev;
	}
#endif

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

	/* Init Charger Detection */
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
err_main_psy:
	power_supply_unregister(mt_chg->main_psy);
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
	power_supply_unregister(mt_charger->main_psy);

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

	power_supply_changed(mt_charger->chg_psy);
	power_supply_changed(mt_charger->ac_psy);
	power_supply_changed(mt_charger->usb_psy);
	power_supply_changed(mt_charger->main_psy);
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
	struct mt_charger *mtk_chg;
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
	struct mt_charger *mtk_chg;
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
	struct mt_charger *mtk_chg;
	struct power_supply *psy = power_supply_get_by_name("charger");
	struct chg_type_info *cti;

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

MODULE_DESCRIPTION("mt-charger-detection");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");

#endif /* CONFIG_MTK_FPGA */
