/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#include <linux/hwid.h>
#include <linux/of.h>
#include <linux/extcon.h>

#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/mtk_charger.h>
#include <pmic.h>
#include <tcpm.h>
#include <linux/hwid.h>
#include "mtk_charger_intf.h"

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

#ifdef CONFIG_EXTCON_USB_CHG
struct usb_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;

	unsigned int vbus_state;
	unsigned long debounce_jiffies;
	struct delayed_work wq_detcable;
};

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};
#endif

void __attribute__((weak)) fg_charger_in_handler(void)
{
	pr_notice("%s not defined\n", __func__);
}

struct chg_type_info {
	struct device *dev;
	struct charger_consumer *chg_consumer;
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	struct notifier_block otg_nb;
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
	bool bypass_chgdet;
#ifdef CONFIG_MACH_MT6771
	struct power_supply *chr_psy;
	struct notifier_block psy_nb;
#endif
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
	"HVDCP Charger",
};

enum product_name {
	UNKNOW,
	RUBY,
	RUBYPRO,
	RUBYPLUS,
};

static int product_name = UNKNOW;

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

	if (!info || !info->usb_psy || (product_name == RUBYPLUS && !info->xmusb350_psy))
		return QUICK_CHARGE_NORMAL;

	charge_status = charger_manager_get_charge_status();
	power_supply_get_property(info->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &pval);
	psy_type = pval.intval;

	if (product_name == RUBYPLUS && info->xmusb350_psy) {
		power_supply_get_property(info->xmusb350_psy, POWER_SUPPLY_PROP_HVDCP3_TYPE, &pval);
		qc3_type = pval.intval;
        }

	if (charge_status == POWER_SUPPLY_STATUS_DISCHARGING || info->tbat > 520 || info->tbat < 0)
		return QUICK_CHARGE_NORMAL;

	if ((psy_type == POWER_SUPPLY_TYPE_USB_PD && info->pd_verifed) || 
		(info->pd_adapter != NULL && info->pd_adapter->adapter_svid == USB_PD_MI_SVID && info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)) {
		if (info->apdo_max >= 50)
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
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *extcon_info;
	struct delayed_work extcon_work;
	#endif
	bool chg_online; /* Has charger in or not */
	enum charger_type chg_type;
};

static int mt_charger_online(struct mt_charger *mtk_chg)
{
	int ret = 0;
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT
	dev = mtk_chg->dev;
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

	if (!mtk_chg->chg_online) {
// workaround for mt6768
		//boot_mode = get_boot_mode();
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
	case POWER_SUPPLY_PROP_USB_TYPE:
		switch (mtk_chg->chg_type) {
		case STANDARD_HOST:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			pr_info("%s: Charger Type: STANDARD_HOST\n", __func__);
			break;
		case NONSTANDARD_CHARGER:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			pr_info("%s: Charger Type: NONSTANDARD_CHARGER\n", __func__);
			break;
		case CHARGING_HOST:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			pr_info("%s: Charger Type: CHARGING_HOST\n", __func__);
			break;
		case STANDARD_CHARGER:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			pr_info("%s: Charger Type: STANDARD_CHARGER\n", __func__);
			break;
		case CHARGER_UNKNOWN:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			pr_info("%s: Charger Type: CHARGER_UNKNOWN\n", __func__);
			break;
		default:
		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_EXTCON_USB_CHG
static void usb_extcon_detect_cable(struct work_struct *work)
{
	struct usb_extcon_info *info = container_of(to_delayed_work(work),
						struct usb_extcon_info,
						wq_detcable);

	/* check and update cable state */
	if (info->vbus_state)
		extcon_set_state_sync(info->edev, EXTCON_USB, true);
	else
		extcon_set_state_sync(info->edev, EXTCON_USB, false);
}
#endif

static void otg_enable_handler(struct charger_manager *info, int val)
{
	struct charger_device *xmusb350_dev = NULL;

	if (info)
		info->otg_enable = !!val;

	if  (product_name  == RUBYPLUS) {
		xmusb350_dev = get_charger_by_name("xmusb350");
		if (!xmusb350_dev) {
			pr_err("%s: failed to get xmusb350_dev\n", __func__);
			return;
		}

		charger_dev_enable_otg(xmusb350_dev, info->otg_enable);

		if(info->otg_enable)
			schedule_delayed_work(&info->usb_otg_monitor_work, 0);
		else {
			info->typec_otg_burn = false;
			cancel_delayed_work_sync(&info->usb_otg_monitor_work);
		}
	} else if (product_name  == RUBY || product_name  == RUBYPRO) {
		if(info->otg_enable)
			schedule_delayed_work(&info->usb_otg_monitor_work, 0);
		else {
			info->typec_otg_burn = false;
			cancel_delayed_work_sync(&info->usb_otg_monitor_work);
		}
	}
	pr_err("%s: otg_enable = %d\n", __func__, info->otg_enable);
}

static int mt_charger_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	struct chg_type_info *cti = NULL;
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *info;
	#endif

	pr_info("%s\n", __func__);

	if (!mtk_chg) {
		pr_notice("%s: no mtk chg data\n", __func__);
		return -EINVAL;
	}

#ifdef CONFIG_EXTCON_USB_CHG
	info = mtk_chg->extcon_info;
#endif

	cti = mtk_chg->cti;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mtk_chg->chg_online = val->intval;
		mt_charger_online(mtk_chg);
		return 0;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		mtk_chg->chg_type = val->intval;
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			charger_manager_force_disable_power_path(
				cti->chg_consumer, MAIN_CHARGER, false);
		else if ((!cti->tcpc_kpoc) && (product_name != RUBYPLUS))
			charger_manager_force_disable_power_path(
				cti->chg_consumer, MAIN_CHARGER, true);
		break;
	default:
		return -EINVAL;
	}

	dump_charger_name(mtk_chg->chg_type);

	if (!cti->ignore_usb) {
		/* usb */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST) ||
			(mtk_chg->chg_type == NONSTANDARD_CHARGER)) {
				mt_usb_connect_v1();
			#ifdef CONFIG_EXTCON_USB_CHG
			info->vbus_state = 1;
			#endif
		} else {
			mt_usb_disconnect_v1();
			#ifdef CONFIG_EXTCON_USB_CHG
			info->vbus_state = 0;
			#endif
		}
	}

	queue_work(cti->chg_in_wq, &cti->chg_in_work);
	#ifdef CONFIG_EXTCON_USB_CHG
	if (!IS_ERR(info->edev))
		queue_delayed_work(system_power_efficient_wq,
			&info->wq_detcable, info->debounce_jiffies);
	#endif

#ifdef CONFIG_MACH_MT6771
	power_supply_changed(mtk_chg->chg_psy);
#endif
	power_supply_changed(mtk_chg->ac_psy);
	power_supply_changed(mtk_chg->usb_psy);

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

static int mt_usb_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_PD_AUTHENTICATION:
	case POWER_SUPPLY_PROP_APDO_MAX:
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	case POWER_SUPPLY_PROP_CP_TAPER:
	case POWER_SUPPLY_PROP_MTBF_TEST:
	case POWER_SUPPLY_PROP_OTG_ENABLE:
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
	u32 master_ibus = 0, slave_ibus = 0, third_ibus = 0;

	if (!mtk_chg) {
		chr_err("failed to init mt_charger\n");
		return -EINVAL;
	}

	if (mtk_chg->cti->chg_consumer)
		info = mtk_chg->cti->chg_consumer->cm;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
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
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		if (info && info->chg1_dev)
			charger_dev_get_ibus(info->chg1_dev, &val->intval);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = mtk_chg->usb_desc.type;
		break;
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		val->intval = get_quick_charge_type(info);
		chr_err("%s: quick_charge_type = %d\n", __func__, val->intval);
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
		chr_err("get power_max = %d\n", val->intval);
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
	case POWER_SUPPLY_PROP_STEPCHG_FV:
		if (info)
			val->intval = info->step_chg_fv;
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
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_PMIC_VBUS:
		if (info && info->chg1_dev)
			charger_dev_get_vbus(info->chg1_dev, &val->intval);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_IBUS_DELTA:
		if (info && info->cp_master && info->cp_slave) {
			charger_dev_get_ibus(info->cp_master, &master_ibus);
			charger_dev_get_ibus(info->cp_slave, &slave_ibus);
			val->intval = (master_ibus > slave_ibus) ? master_ibus - slave_ibus : slave_ibus - master_ibus;
			pr_info("%s delta = %d, master_ibus = %d, slave_ibus = %d\n", __func__, val->intval, master_ibus, slave_ibus);
                } else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_IBUS_DELTA13:
		if ((product_name == RUBYPLUS) && info && info->cp_master && info->cp_third) {
			charger_dev_get_ibus(info->cp_master, &master_ibus);
			charger_dev_get_ibus(info->cp_third, &third_ibus);
			val->intval = (master_ibus > third_ibus) ? master_ibus - third_ibus : third_ibus - master_ibus;
			pr_info("%s delta = %d, master_ibus = %d, third_ibus = %d\n", __func__, val->intval, master_ibus, third_ibus);
                } else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_IBUS_DELTA23:
		if ((product_name == RUBYPLUS) && info && info->cp_slave && info->cp_third) {
			charger_dev_get_ibus(info->cp_slave, &slave_ibus);
			charger_dev_get_ibus(info->cp_third, &third_ibus);
			val->intval = (slave_ibus > third_ibus) ? slave_ibus - third_ibus : third_ibus - slave_ibus;
			pr_info("%s delta = %d, slave_ibus = %d, third_ibus = %d\n", __func__, val->intval, slave_ibus, third_ibus);
                } else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_TAPER:
		if(info)
			val->intval = info->cp_taper;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_MTBF_TEST:
		if(info)
			val->intval = info->mtbf_test;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (info)
			val->intval = info->charge_full;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_OTG_ENABLE:
		if (info)
			val->intval = info->otg_enable;
		else
			val->intval = 0;
		break;
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
		if (info) {
			info->pd_verifed = val->intval;
			if (info->pd_verifed){
				vote(info->bbc_icl_votable, CHARGER_TYPE_VOTER, true, PD3_ICL);
				vote(info->bbc_fcc_votable, CHARGER_TYPE_VOTER, true, info->max_fcc);
			}
		}
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
		if (info){
			info->input_suspend = val->intval;
			if(info->input_suspend)
			      vote(info->bbc_suspend_votable, USB_SUSPEND_VOTER, true, 1);
			else
			      vote(info->bbc_suspend_votable, USB_SUSPEND_VOTER, false, 0);
			power_supply_changed(info->usb_psy);
		}
		break;
	case POWER_SUPPLY_PROP_CP_TAPER:
		if (info)
			info->cp_taper = val->intval;
		break;
	case POWER_SUPPLY_PROP_MTBF_TEST:
		if (info)
			info->mtbf_test = val->intval;
		break;
	case POWER_SUPPLY_PROP_OTG_ENABLE:
		if (info)
			otg_enable_handler(info,  val->intval);
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
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
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
	POWER_SUPPLY_PROP_PMIC_VBUS,
	POWER_SUPPLY_PROP_CP_IBUS_DELTA,
	POWER_SUPPLY_PROP_CP_IBUS_DELTA13,
	POWER_SUPPLY_PROP_CP_IBUS_DELTA23,
	POWER_SUPPLY_PROP_CP_TAPER,
	POWER_SUPPLY_PROP_MTBF_TEST,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_OTG_ENABLE,
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
	if (cti->chgdet_en == en)
		goto skip;
	cti->chgdet_en = en;
	cti->ignore_usb = ignore;
	cti->plugin = en;
	atomic_inc(&cti->chgdet_cnt);
	wake_up_interruptible(&cti->waitq);
skip:
	mutex_unlock(&cti->chgdet_lock);
}

static int get_source_mode(struct tcp_notify *noti)
{
	chr_err("%s, new_state = %d, rp_level = %d\n", __func__, noti->typec_state.new_state, noti->typec_state.rp_level);
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
	union power_supply_propval propval = {0,};
	struct power_supply *ac_psy = power_supply_get_by_name("ac");
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	struct mt_charger *mtk_chg_ac;
	struct mt_charger *mtk_chg_usb;
	struct timespec64 time_now;
	ktime_t ktime_now;
	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);

	if (IS_ERR_OR_NULL(usb_psy)) {
		chr_err("%s, fail to get usb_psy\n", __func__);
		return NOTIFY_BAD;
	}
	if (IS_ERR_OR_NULL(ac_psy)) {
		chr_err("%s, fail to get ac_psy\n", __func__);
		return NOTIFY_BAD;
	}

	mtk_chg_ac = power_supply_get_drvdata(ac_psy);
	if (IS_ERR_OR_NULL(mtk_chg_ac)) {
		chr_err("%s, fail to get mtk_chg_ac\n", __func__);
		return NOTIFY_BAD;
	}

	mtk_chg_usb = power_supply_get_drvdata(usb_psy);
	if (IS_ERR_OR_NULL(mtk_chg_usb)) {
		chr_err("%s, fail to get mtk_chg_usb\n", __func__);
		return NOTIFY_BAD;
	}

	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		if (tcpm_inquire_typec_attach_state(cti->tcpc) ==
						   TYPEC_ATTACHED_AUDIO)
			plug_in_out_handler(cti, !!noti->vbus_state.mv, true);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			cti->cc_orientation = noti->typec_state.polarity;
			cti->typec_mode = get_source_mode(noti);
			pr_info("%s USB Plug in, pol = %d\n", __func__,
					noti->typec_state.polarity);
			plug_in_out_handler(cti, true, false);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			propval.intval = POWER_SUPPLY_TYPE_UNKNOWN;
			power_supply_set_property(usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &propval);
			if (cti->tcpc_kpoc && (time_now.tv_sec > 10)) {
				vbus = battery_get_vbus();
				pr_info("%s KPOC Plug out, vbus = %d\n",
					__func__, vbus);
				mtk_chg_ac->chg_type = CHARGER_UNKNOWN;
				mtk_chg_usb->chg_type = CHARGER_UNKNOWN;
				power_supply_changed(ac_psy);
				power_supply_changed(usb_psy);
				msleep(3000);
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

static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct chg_type_info *cti = container_of(nb,
		struct chg_type_info, otg_nb);
	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		pr_info("%s, TCP_NOTIFY_TYPEC_STATE, old_state=%d, new_state=%d\n",
				__func__, noti->typec_state.old_state,
				noti->typec_state.new_state);
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			cti->typec_mode = POWER_SUPPLY_TYPEC_SINK;
		} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
			cti->typec_mode = POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER;
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO) &&
		    noti->typec_state.new_state == TYPEC_UNATTACHED) {
			cti->typec_mode = POWER_SUPPLY_TYPEC_NONE;
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
	bool attach = false, ignore_usb = false;
	int ret = 0;
	struct power_supply *psy = power_supply_get_by_name("charger");
	union power_supply_propval val = {.intval = 0};

	if (!psy) {
		pr_notice("%s: power supply get fail\n", __func__);
		return -ENODEV;
	}

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
		ignore_usb = cti->ignore_usb;
		mutex_unlock(&cti->chgdet_lock);

		if (attach && ignore_usb) {
			cti->bypass_chgdet = true;
			goto bypass_chgdet;
		} else if (!attach && cti->bypass_chgdet) {
			cti->bypass_chgdet = false;
			goto bypass_chgdet;
		}

#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
		if (cti->chg_consumer)
			charger_manager_enable_chg_type_det(cti->chg_consumer,
							attach);
#else
		mtk_pmic_enable_chr_type_det(attach);
#endif
		goto pm_relax;
bypass_chgdet:
		val.intval = attach;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE,
						&val);
		if (ret < 0)
			pr_notice("%s: power supply set online fail(%d)\n",
				  __func__, ret);
		if (tcpm_inquire_typec_attach_state(cti->tcpc) ==
						   TYPEC_ATTACHED_AUDIO)
			val.intval = attach ? NONSTANDARD_CHARGER :
					      CHARGER_UNKNOWN;
		else
			val.intval = attach ? STANDARD_HOST : CHARGER_UNKNOWN;
		ret = power_supply_set_property(psy,
						POWER_SUPPLY_PROP_CHARGE_TYPE,
						&val);
		if (ret < 0)
			pr_notice("%s: power supply set charge type fail(%d)\n",
				  __func__, ret);
pm_relax:
		pm_relax(cti->dev);
	}
	pr_info("%s: --\n", __func__);
	return 0;
}

#ifdef CONFIG_EXTCON_USB_CHG
static void init_extcon_work(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct mt_charger *mt_chg =
		container_of(dw, struct mt_charger, extcon_work);
	struct device_node *node = mt_chg->dev->of_node;
	struct usb_extcon_info *info;

	info = mt_chg->extcon_info;
	if (!info)
		return;

	if (of_property_read_bool(node, "extcon")) {
		info->edev = extcon_get_edev_by_phandle(mt_chg->dev, 0);
		if (IS_ERR(info->edev)) {
			schedule_delayed_work(&mt_chg->extcon_work,
				msecs_to_jiffies(50));
			return;
		}
	}

	INIT_DELAYED_WORK(&info->wq_detcable, usb_extcon_detect_cable);
}
#endif

#ifdef CONFIG_MACH_MT6771
static int mt6370_psy_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct power_supply *type_psy = NULL;

	struct chg_type_info *cti = container_of(nb,
					struct chg_type_info, psy_nb);

	union power_supply_propval pval;
	int ret;

	if (event != PSY_EVENT_PROP_CHANGED) {
		pr_info("%s, event not equal\n", __func__);
		return NOTIFY_DONE;
	}

	if (IS_ERR_OR_NULL(cti->chr_psy)) {
		cti->chr_psy = power_supply_get_by_name("mt6370_pmu_charger");
		if (IS_ERR_OR_NULL(cti->chr_psy)) {
			pr_info("fail to get chr_psy\n");
			cti->chr_psy = NULL;
			return NOTIFY_DONE;
		}
	} else
		pr_info("%s, get mt6370 psy success, event(%d)\n", __func__, event);

	/*psy is mt6370, type_psy is charger_type psy*/
	if (psy != cti->chr_psy) {
		pr_info("power supply not equal\n");
		return NOTIFY_DONE;
	}

	type_psy = power_supply_get_by_name("charger");
	if (!type_psy) {
		pr_info("%s: get power supply failed\n",
			__func__);
		return NOTIFY_DONE;
	}

	if (event != PSY_EVENT_PROP_CHANGED) {
		pr_info("%s: get event power supply failed\n",
			__func__);
		return NOTIFY_DONE;
	}

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		pr_info("psy failed to get online prop\n");
		return NOTIFY_DONE;
	}

	ret = power_supply_set_property(type_psy, POWER_SUPPLY_PROP_ONLINE,
		&pval);
	if (ret < 0)
		pr_info("%s: type_psy online failed, ret = %d\n",
			__func__, ret);

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_USB_TYPE, &pval);
	if (ret < 0) {
		pr_info("failed to get usb type prop\n");
		return NOTIFY_DONE;
	}

	switch (pval.intval) {
	case POWER_SUPPLY_USB_TYPE_SDP:
		pval.intval = STANDARD_HOST;
		break;
	case POWER_SUPPLY_USB_TYPE_DCP:
		pval.intval = STANDARD_CHARGER;
		break;
	case POWER_SUPPLY_USB_TYPE_CDP:
		pval.intval = CHARGING_HOST;
		break;
	default:
		pval.intval = CHARGER_UNKNOWN;
		break;
	}

	ret = power_supply_set_property(type_psy, POWER_SUPPLY_PROP_CHARGE_TYPE,
		&pval);
	if (ret < 0)
		pr_info("%s: type_psy type failed, ret = %d\n",
			__func__, ret);

	return NOTIFY_DONE;
}
#endif

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

	pr_info("product_name = %d, ruby = %d, rubypro = %d, rubyplus = %d\n", product_name, ruby ? 1 : 0, rubypro ? 1 : 0, rubyplus ? 1 : 0);
}

static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct chg_type_info *cti = NULL;
	struct mt_charger *mt_chg = NULL;
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *info;
	#endif
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT

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
	mt_chg->usb_desc.property_is_writeable = mt_usb_is_writeable;
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

	dev = &(pdev->dev);
	if (dev != NULL) {
		boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
		if (!boot_node) {
			chr_err("%s: failed to get boot mode phandle\n", __func__);
		} else {
			tag = (struct tag_bootmode *)of_get_property(boot_node, "atag,boot", NULL);
			if (!tag)
				chr_err("%s: failed to get atag,boot\n", __func__);
			else
				boot_mode = tag->bootmode;
		}
	}
	//ret = get_boot_mode();
	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
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

#ifdef CONFIG_MACH_MT6771
	cti->psy_nb.notifier_call = mt6370_psy_notifier;
	ret = power_supply_reg_notifier(&cti->psy_nb);
	if (ret)
		pr_info("fail to register notifer\n");

	cti->chr_psy = power_supply_get_by_name("mt6370_pmu_charger");
	if (IS_ERR_OR_NULL(cti->chr_psy))
		pr_info("%s, fail to get chr_psy\n", __func__);
#endif

	#ifdef CONFIG_EXTCON_USB_CHG
	info = devm_kzalloc(mt_chg->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = mt_chg->dev;
	mt_chg->extcon_info = info;

	INIT_DELAYED_WORK(&mt_chg->extcon_work, init_extcon_work);
	schedule_delayed_work(&mt_chg->extcon_work, 0);
	#endif

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

device_initcall(mt_charger_det_init);
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

	cti->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (cti->tcpc == NULL) {
		pr_notice("%s: get tcpc dev fail\n", __func__);
		ret = -ENODEV;
		goto out;
	}
	cti->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(cti->tcpc,
		&cti->pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_notice("%s: register tcpc notifier fail(%d)\n",
			  __func__, ret);
		goto out;
	}
	cti->otg_nb.notifier_call = otg_tcp_notifier_call;
	ret = register_tcp_dev_notifier(cti->tcpc,
		&cti->otg_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_info("%s: register otg tcpc notifer fail\n",
			__func__);
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
