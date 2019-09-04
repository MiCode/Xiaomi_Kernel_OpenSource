/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#include "mtk_charger_intf.h"


void __attribute__((weak)) fg_charger_in_handler(void)
{
	pr_notice("%s not defined\n", __func__);
}

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
		pr_info("%s: charger type: %d, %s\n", __func__, type,
			mtk_chg_type_name[type]);
		break;
	default:
		pr_info("%s: charger type: %d, Not Defined!!!\n", __func__,
			type);
		break;
	}
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
	default:
		return -EINVAL;
	}

	dump_charger_name(mtk_chg->chg_type);

	cti = mtk_chg->cti;
	if (!cti->ignore_usb) {
		/* usb */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST) ||
			(mtk_chg->chg_type == NONSTANDARD_CHARGER))
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

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	enum hvdcp_status hvdcp_type_tmp;
	int input_suspend;
	union power_supply_propval tmpval;

	hvdcp_type_tmp = charger_manager_check_hvdcp_status();
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		input_suspend = charger_manager_is_input_suspend();
		if (((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST)) && !input_suspend)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		if (hvdcp_type_tmp == HVDCP_3) {
			val->intval = POWER_SUPPLY_TYPE_USB_HVDCP_3;
			break;
		} else if (hvdcp_type_tmp == HVDCP ||
			charger_manager_pe2_is_online()) {
			val->intval = POWER_SUPPLY_TYPE_USB_HVDCP;
			break;
		} else if (charger_manager_pd_is_online() ||
			charger_manager_pe4_is_online()) {
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
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
		default:
			val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
		}
		break;

/*	case POWER_SUPPLY_PROP_HVDCP3_TYPE:
		if (charger_manager_pd_is_online())
			val->intval = USB_PD;
		else if (hvdcp_type_tmp == HVDCP)
			val->intval = HVDCP2_TYPE;
		else if (hvdcp_type_tmp == HVDCP_3)
			val->intval = HVDCP3_CLASSA_18W;
		else
			val->intval = HVDCP3_NONE;
		break;
*/	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery_get_vbus();
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		charger_manager_get_ibus(&val->intval);
		val->intval = -val->intval;
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		val->intval = 1 + mtk_chg->cti->cc_orientation;
		break;
	case POWER_SUPPLY_PROP_RA_DETECTED:
		val->intval = charger_manager_check_ra_detected();
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
		power_supply_get_property(mtk_chg->main_psy,
			POWER_SUPPLY_PROP_TEMP_MAX, &tmpval);
		val->intval = tmpval.intval;
		break;
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
		if (!mtk_chg->parallel_psy) {
			mtk_chg->parallel_psy = power_supply_get_by_name("parallel");
			if (!mtk_chg->parallel_psy)
				chr_err("%s: get power supply failed\n", __func__);
			val->intval = 0;
		} else {
			power_supply_get_property(mtk_chg->parallel_psy,
				POWER_SUPPLY_PROP_VBUS_DISABLE, &tmpval);
			val->intval = tmpval.intval;
		}
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		val->intval = mtk_chg->cti->typec_mode;
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

	switch (psp) {
	case POWER_SUPPLY_PROP_RA_DETECTED:
		charger_manager_set_ra_detected(val->intval);
		return 0;
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
		if (!mtk_chg->parallel_psy) {
			mtk_chg->parallel_psy = power_supply_get_by_name("parallel");
			if (!mtk_chg->parallel_psy)
				chr_err("%s: get power supply failed\n", __func__);
		} else {
			power_supply_set_property(mtk_chg->parallel_psy,
				POWER_SUPPLY_PROP_VBUS_DISABLE, val);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
static int mt_usb_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_RA_DETECTED:
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
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
	case POWER_SUPPLY_PROP_TEMP_MIN:
		charger_dev_get_temperature(mtk_chg->chg1_dev, &tchg_min, &tchg_max);
		val->intval = tchg_min;
		break;
	case POWER_SUPPLY_PROP_TEMP_CONNECT:
		charger_dev_get_adc(mtk_chg->chg1_dev, ADC_CHANNEL_TS, &vts, &vts);
		val->intval = vts;
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
};

static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_REAL_TYPE,

	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_RA_DETECTED,
	POWER_SUPPLY_PROP_CONNECTOR_TEMP,
	POWER_SUPPLY_PROP_VBUS_DISABLE,
	POWER_SUPPLY_PROP_TYPEC_MODE,
};

static enum power_supply_property mt_main_properties[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP_MAX,
	POWER_SUPPLY_PROP_TEMP_MIN,
	POWER_SUPPLY_PROP_TEMP_CONNECT,
};

static void tcpc_power_off_work_handler(struct work_struct *work)
{
	pr_info("%s\n", __func__);

}

static void charger_in_work_handler(struct work_struct *work)
{
	mtk_charger_int_handler();
	fg_charger_in_handler();
}

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
			pr_info("%s USB Plug in, pol = %d, state = %d\n", __func__,
					noti->typec_state.polarity, noti->typec_state.new_state);
			cti->typec_mode = get_source_mode(noti);
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
				plug_in_out_handler(cti, false, false);
				break;
			}
			cti->typec_mode = POWER_SUPPLY_TYPEC_NONE;
			pr_info("%s USB Plug out\n", __func__);
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
static int chgdet_task_threadfn(void *data)
{
	struct chg_type_info *cti = data;
	bool attach = false;
	int ret = 0;
	int i = 0;
	int max_wait_cnt = 40;

	for (i = 0; i < max_wait_cnt; i++) {
		msleep(500);

		cti->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
		if (!cti->tcpc_dev) {
			pr_info("%s get tcpc device type_c_port0 fail\n",
				__func__);
			continue;
		} else {
			cti->pd_nb.notifier_call = pd_tcp_notifier_call;
			ret = register_tcp_dev_notifier(cti->tcpc_dev,
				&cti->pd_nb, TCP_NOTIFY_TYPE_ALL);
			if (ret < 0) {
				pr_info("%s: register tcpc notifer fail\n",
					__func__);
			}
			cti->otg_nb.notifier_call = otg_tcp_notifier_call;
			ret = register_tcp_dev_notifier(cti->tcpc_dev,
				&cti->otg_nb, TCP_NOTIFY_TYPE_ALL);
			if (ret < 0) {
				pr_info("%s: register tcpc notifer fail\n",
					__func__);
			}
		}

		cti->chg_consumer = charger_manager_get_by_name(cti->dev,
			"charger_port1");
		if (!cti->chg_consumer) {
			pr_info("%s: get charger consumer device failed\n",
				__func__);
		}

		pr_info("%s: get tcpc and charger consumer done\n", __func__);
		break;
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
		mutex_unlock(&cti->chgdet_lock);
		pr_info("%s start bc12\n", __func__);
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


static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct chg_type_info *cti = NULL;
	struct mt_charger *mt_chg = NULL;

	mt_chg = devm_kzalloc(&pdev->dev, sizeof(*mt_chg), GFP_KERNEL);
	if (!mt_chg)
		return -ENOMEM;

	mt_chg->dev = &pdev->dev;
	mt_chg->chg_online = false;
	mt_chg->chg_type = CHARGER_UNKNOWN;
	mt_chg->chg1_dev = NULL;

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
	mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB;
	mt_chg->usb_desc.properties = mt_usb_properties;
	mt_chg->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	mt_chg->usb_desc.set_property = mt_usb_set_property;
	mt_chg->usb_desc.get_property = mt_usb_get_property;
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
	if (!cti)
		return -ENOMEM;
	cti->dev = &pdev->dev;

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

	pr_info("%s\n", __func__);
	return 0;
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
