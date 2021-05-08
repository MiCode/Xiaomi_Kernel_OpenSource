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
#include "mtk_intf.h"

#include "mtk_charger_intf.h"

#define ANTIBURN_VREF	1800
#define ANTIBURN_RP	3900
#define DEFAULT_TEMP	250

int fake_temp = 0;

struct ntc_desc {
	int temp;	/* 0.1 deg.C */
	int res;	/* ohm */
};

static struct ntc_desc ntc_table[] = {
	{-400, 195652},
	{-350, 148171},
	{-300, 113347},
	{-250, 87559},
	{-200, 68237},
	{-150, 53650},
	{-100, 42506},
	{-50, 33892},
	{0, 27219},
	{50, 22021},
	{100, 17926},
	{150, 14674},
	{200, 12081},
	{250, 10000},
	{300, 8315},
	{350, 6948},
	{400, 5833},
	{450, 4917},
	{500, 4161},
	{550, 3535},
	{600, 3014},
	{650, 2586},
	{700, 2228},
	{750, 1925},
	{800, 1669},
	{850, 1452},
	{900, 1268},
	{950, 1110},
	{1000, 974},
	{1050, 858},
	{1100, 758},
	{1150, 672},
	{1200, 596},
	{1250, 531}
};

/* 2021.01.21 longcheer jiangshitian change for mic noise begin */
#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
extern void set_chg_exist_flag(bool chgflag);
#endif
/* 2021.01.21 longcheer jiangshitian change for mic noise end */


static int get_connector_temp(struct mt_charger *mtk_chg)
{
	int volt = 0, connector_temp = DEFAULT_TEMP, res = 0, lower = 0, upper = 0, rc = 0, i = 0, vts = 0;
	int table_size = sizeof(ntc_table) / sizeof(ntc_table[0]), retry_count = 0;

retry:
	rc = charger_dev_get_adc(mtk_chg->chg1_dev, ADC_CHANNEL_TS, &vts, &vts);
	if (rc < 0) {
		pr_err("%s: get ts_volt failed\n", __func__);
		return connector_temp;
	} else if (vts / 1000 >= ANTIBURN_VREF) {
		pr_err("%s: ts_volt is unwanted\n", __func__);
		return connector_temp;
	}

	volt = vts / 1000;
	res = ANTIBURN_RP * volt / (ANTIBURN_VREF - volt);

	if (res >= ntc_table[0].res)
		return ntc_table[0].temp;
	else if (res <= ntc_table[table_size - 1].res)
		return ntc_table[table_size - 1].temp;

	for (i = 0; i < table_size; i++) {
		if (res >= ntc_table[i].res) {
			upper = i;
			break;
		}
		lower = i;
	}

	connector_temp = (ntc_table[lower].temp * (res - ntc_table[upper].res) + ntc_table[upper].temp *
			(ntc_table[lower].res - res)) / (ntc_table[lower].res - ntc_table[upper].res);

	/* if result < -40C or > 80C, retry */
	if (retry_count < 3 && (connector_temp <= ntc_table[0].temp || connector_temp >= ntc_table[24].temp)) {
		retry_count++;
		mdelay(10);
		goto retry;
	}

	return connector_temp;
}

void __attribute__((weak)) fg_charger_in_handler(void)
{
	pr_notice("%s not defined\n", __func__);
}

struct chg_type_info {
	struct device *dev;
	struct charger_consumer *chg_consumer;
	struct tcpc_device *tcpc_dev;
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
	int usb_plug;
	int voltage_max;
#ifdef CONFIG_MTBF_SUPPORT
	int mtbf_chg_curr;
#endif
#ifdef CONFIG_BQ2597X_CHARGE_PUMP
	int pd_verifed;
	int pd_active;
	int pd_type;
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
	"CHECK HV",
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
	case CHECK_HV:
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
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *extcon_info;
	struct delayed_work extcon_work;
	#endif
	bool chg_online; /* Has charger in or not */
	enum charger_type chg_type;
};
#endif

extern void kpd_pmic_pwrkey_hal(unsigned long pressed);
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
			//if (system_state != SYSTEM_POWER_OFF)
			//	kernel_power_off();
			kpd_pmic_pwrkey_hal(1);
			mdelay(200);
			kpd_pmic_pwrkey_hal(0);
			mdelay(1500);
		}
	}

	return ret;
}

/* Power Supply Functions */
static int mt_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	int input_suspend;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		input_suspend = charger_manager_is_input_suspend();
		//if (mtk_chg->chg_type != CHARGER_UNKNOWN && !input_suspend && mtk_chg->cti->typec_mode != POWER_SUPPLY_TYPEC_NONE)
		if (mtk_chg->chg_type != CHARGER_UNKNOWN && !input_suspend)
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (mtk_chg->cti->typec_mode == POWER_SUPPLY_TYPEC_NONE)
			mtk_chg->chg_type = CHARGER_UNKNOWN;
		val->intval = mtk_chg->chg_type;
		break;
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

static int mt_charger_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	struct chg_type_info *cti = NULL;
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *info;
	#endif

	struct power_supply *usb_psy;
	union power_supply_propval pval = {0,};
	
	pr_info("%s\n", __func__);

	usb_psy = power_supply_get_by_name("usb");

	if (!mtk_chg || !usb_psy) {
		pr_notice("%s: no mtk chg data or usb psy\n", __func__);
		return -EINVAL;
	}

#ifdef CONFIG_EXTCON_USB_CHG
	info = mtk_chg->extcon_info;
#endif

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
	power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_PD_VERIFY_IN_PROCESS, &pval);
	if (pval.intval == 1) {
		pr_info("pd verifing, don't switch data role\n", __func__);
	} else if (!cti->ignore_usb) {
		/* usb */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST)) {
			mt_usb_connect();
			#ifdef CONFIG_EXTCON_USB_CHG
			info->vbus_state = 1;
			#endif
		} else {
			mt_usb_disconnect();
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

	power_supply_changed(mtk_chg->ac_psy);
	power_supply_changed(mtk_chg->usb_psy);

	return 0;
}

static int mt_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	int input_suspend;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		input_suspend = charger_manager_is_input_suspend();
		//if (mtk_chg->chg_type != CHARGER_UNKNOWN && !input_suspend && mtk_chg->cti->typec_mode != POWER_SUPPLY_TYPEC_NONE)
		if (mtk_chg->chg_type != CHARGER_UNKNOWN && !input_suspend)
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

enum quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,
	QUICK_CHARGE_FAST,
	QUICK_CHARGE_FLASH,
	QUICK_CHARGE_TURBE,
	QUICK_CHARGE_MAX,
};

struct quick_charge {
	enum power_supply_type adap_type;
	enum quick_charge_type adap_cap;
};

struct quick_charge adapter_cap[11] = {
	{ POWER_SUPPLY_TYPE_USB,        QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_ACA,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP,    QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS,  QUICK_CHARGE_FLASH },
	{ POWER_SUPPLY_TYPE_WIRELESS,     QUICK_CHARGE_FAST },
	{0, 0},
};

#define	CLEAR_SOC_DECIMAL_RATE_MS	10000
int get_quick_charge_type(struct mt_charger *mtk_chg)
{
	int i = 0, rc;
	union power_supply_propval pval = {0, };
	union power_supply_propval pval_usb = {0, };
	static bool soc_decimal_rate_changed;
	struct power_supply	*battery_psy;
	struct power_supply	*Charger_Identify_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*usb_psy;
	int soc_decimal_flag;

	battery_psy = power_supply_get_by_name("battery");
	Charger_Identify_psy = power_supply_get_by_name("Charger_Identify");
	bms_psy = power_supply_get_by_name("bms");
	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		rc = power_supply_get_property(usb_psy,
			POWER_SUPPLY_PROP_REAL_TYPE, &pval_usb);
		if (rc < 0)
		return -EINVAL;
	}

	if (!mtk_chg) {
		pr_err("get quick charge type faied.\n");
		return -EINVAL;
	}

	if ((!battery_psy) || (!Charger_Identify_psy)) {
		pr_err("psy is null, get quick charge type failed.\n");
		//return -EINVAL; //2020.12.24 longcheer jiangshitian edit for miui quick charge type check
	}
	rc = power_supply_get_property(battery_psy,
			POWER_SUPPLY_PROP_STATUS, &pval);
	if (rc < 0)
		return -EINVAL;

	/* 2021.03.29 longcheer jiangshitian HTH-142171 start */
	pr_info("%s: %d: charging_status = %d \n", __func__, __LINE__, pval.intval);
	if ((pval.intval == POWER_SUPPLY_STATUS_DISCHARGING) || (pval.intval == POWER_SUPPLY_STATUS_NOT_CHARGING))//2: discharging 3:not_charging
		return 0;

	/* 2021.03.29 longcheer jiangshitian HTH-142171 start */
	rc = power_supply_get_property(battery_psy,
			POWER_SUPPLY_PROP_HEALTH, &pval);
	if (rc < 0)
		return -EINVAL;

	pr_info("%s: %d: battery_health_status = %d \n", __func__, __LINE__, pval.intval);
	if ((pval.intval == POWER_SUPPLY_HEALTH_COLD) || (pval.intval == POWER_SUPPLY_HEALTH_HOT))//6:cold 12:hot
		return 0;
	/* 2021.03.29 longcheer jiangshitian HTH-142171 end */

	soc_decimal_flag = get_disable_soc_decimal_flag();
	pr_info("%s: %d: soc_decimal_flag = %d usb_desc.type = %d pd_verifed = %d \n", __func__, __LINE__, soc_decimal_flag, pval_usb.intval, mtk_chg->cti->pd_verifed);

	if ((pval_usb.intval == POWER_SUPPLY_TYPE_USB_PD) && mtk_chg->cti->pd_verifed) {
		// config disable_soc_decimal, soc_decimal_flag will be 1, and not show soc decimal.
		if (soc_decimal_flag != 1) {
			if (!soc_decimal_rate_changed) {
				/* ui_soc 0.01% */
				pval.intval = 100;
				rc = power_supply_set_property(bms_psy, POWER_SUPPLY_PROP_MTK_SOC_DECIMAL_RATE, &pval);
				schedule_delayed_work(&mtk_chg->clear_soc_decimal_rate_work, msecs_to_jiffies(CLEAR_SOC_DECIMAL_RATE_MS));
				power_supply_changed(bms_psy);
				soc_decimal_rate_changed = true;
				pr_info("%s, soc_decimal_rate_changed:QUICK_CHARGE_TURBE.\n", __func__);
			}
		}

		return QUICK_CHARGE_TURBE;

	} else {
		// config disable_soc_decimal, soc_decimal_flag will be 1, and not show soc decimal.
		if (soc_decimal_flag != 1) {
			if (soc_decimal_rate_changed) {
				pval.intval = 0;
				rc = power_supply_set_property(bms_psy, POWER_SUPPLY_PROP_MTK_SOC_DECIMAL_RATE, &pval);
				soc_decimal_rate_changed = false;
				pr_info("%s, soc_decimal_rate_changed:Normal_charger_type.\n", __func__);
			}
		}
	}
#ifdef CONFIG_XMUSB350_DET_CHG
	rc = power_supply_get_property(Charger_Identify_psy,
			POWER_SUPPLY_PROP_QC35_CHG_TYPE, &pval);
	if (rc < 0)
		return -EINVAL;
#endif
	if (pval.intval == QC35_HVDCP_3_PLUS_27
#ifdef CONFIG_XMUSB350_DET_CHG
			|| pval.intval == QC35_HVDCP_30_27
#endif
		)
		return QUICK_CHARGE_FLASH;

	while (adapter_cap[i].adap_type != 0) {
		if (pval_usb.intval == adapter_cap[i].adap_type) {
			pr_info("%s: %d: adapter_cap[i].adap_cap = %d\n", __func__, __LINE__, adapter_cap[i].adap_cap);
			return adapter_cap[i].adap_cap;
		}
		i++;
	}

	return 0;
}

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);
	union power_supply_propval pval = {0,};
	int input_suspend = 0;
	int pd_online = 0;
	
#ifdef CONFIG_XMUSB350_DET_CHG
	int rc = 0;

	if (mtk_chg->charger_identify_psy == NULL)
		mtk_chg->charger_identify_psy = power_supply_get_by_name("Charger_Identify");
	if ((psp == POWER_SUPPLY_PROP_REAL_TYPE) ||
		(psp == POWER_SUPPLY_PROP_HVDCP3_TYPE)) {
		if (mtk_chg->charger_identify_psy == NULL) {
			pr_err("charger_identify_psy is NULL\n");
			return -ENODATA;
		}
	}
#endif

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		input_suspend = charger_manager_is_input_suspend();
		//if (((mtk_chg->chg_type == STANDARD_HOST) ||
		//	(mtk_chg->chg_type == CHARGING_HOST) || charger_manager_pd_is_online()) && !input_suspend && mtk_chg->cti->typec_mode != POWER_SUPPLY_TYPEC_NONE)
		if (((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST) || charger_manager_pd_is_online()) && !input_suspend)
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
		pd_online = charger_manager_pd_is_online();
		if (pd_online) {
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		} else {
			if (mtk_chg->chg_type == STANDARD_HOST){
				val->intval = POWER_SUPPLY_TYPE_USB;
			}else if(mtk_chg->chg_type == CHECK_HV){
				val->intval = POWER_SUPPLY_TYPE_USB_DCP;
			}else if(mtk_chg->chg_type == STANDARD_CHARGER){
				val->intval = POWER_SUPPLY_TYPE_USB_DCP;
			}else if(mtk_chg->chg_type ==CHARGING_HOST){
				val->intval = POWER_SUPPLY_TYPE_USB_CDP;
			}else if(mtk_chg->chg_type == NONSTANDARD_CHARGER){
				val->intval = POWER_SUPPLY_TYPE_USB_FLOAT;
			}else if(mtk_chg->chg_type == HVDCP_CHARGER){
				val->intval = POWER_SUPPLY_TYPE_USB_HVDCP;
			}else if(mtk_chg->chg_type == CHARGER_UNKNOWN){
				val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
			}else{
				val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
			}
		}
		//val->intval = mtk_chg->usb_desc.type;
		pr_err("%s  %d  POWER_SUPPLY_PROP_REAL_TYPE: mtk_chg->chg_type=%d pd_active=%d real_type=%d\n", __func__, __LINE__, mtk_chg->chg_type, mtk_chg->cti->pd_active, val->intval);
		break;
#ifdef CONFIG_XMUSB350_DET_CHG
	case POWER_SUPPLY_PROP_HVDCP3_TYPE:
		rc = power_supply_get_property(mtk_chg->charger_identify_psy,
				POWER_SUPPLY_PROP_QC35_CHG_TYPE, &pval);
		if (rc)
			return rc;
		if (pval.intval == QC35_HVDCP_30)
			val->intval = HVDCP3_NONE;
		else if (pval.intval == QC35_HVDCP_30_18)
			val->intval = HVDCP3_CLASSA_18W;
		else if (pval.intval == QC35_HVDCP_30_27)
			val->intval = HVDCP3_CLASSB_27W;
		else if (pval.intval == QC35_HVDCP_3_PLUS_18)
			val->intval = HVDCP3_P_CLASSA_18W;
		else if (pval.intval == QC35_HVDCP_3_PLUS_27)
			val->intval = HVDCP3_P_CLASSB_27W;
		else
			val->intval = HVDCP3_NONE;
		break;
#else
    case POWER_SUPPLY_PROP_HVDCP3_TYPE:
		val->intval = HVDCP3_NONE;
#endif
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery_get_vbus();
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		charger_manager_get_ibus(&pval.intval);
		val->intval = pval.intval;
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		val->intval = 1 + mtk_chg->cti->cc_orientation;
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		val->intval = tcpm_inquire_typec_role(mtk_chg->cti->tcpc_dev);
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		val->intval = mtk_chg->cti->typec_mode;
		break;
#ifdef CONFIG_MTBF_SUPPORT
	case POWER_SUPPLY_PROP_MTBF_CUR:
		val->intval = mtk_chg->cti->mtbf_chg_curr;
		break;
#endif
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
		if (!fake_temp)
			val->intval = get_connector_temp(mtk_chg);
		else
			val->intval = fake_temp;
		break;
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
		if (!mtk_chg->charger_identify_psy) {
			mtk_chg->charger_identify_psy = power_supply_get_by_name("Charger_Identify");
			if (!mtk_chg->charger_identify_psy)
				chr_err("%s: get power supply failed\n", __func__);
			val->intval = 0;
		} else {
			power_supply_get_property(mtk_chg->charger_identify_psy,
				POWER_SUPPLY_PROP_VBUS_DISABLE, &pval);
			val->intval = pval.intval;
		}
		break;
#ifdef CONFIG_BQ2597X_CHARGE_PUMP
	case POWER_SUPPLY_PROP_PD_TYPE:
		if (mtk_chg->cti->typec_mode == POWER_SUPPLY_TYPEC_NONE)
			mtk_chg->cti->pd_type = 0;
		val->intval = mtk_chg->cti->pd_type;
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		if (mtk_chg->cti->typec_mode == POWER_SUPPLY_TYPEC_NONE)
			mtk_chg->cti->pd_active = 0;
		val->intval = mtk_chg->cti->pd_active;
		break;
	case POWER_SUPPLY_PROP_PD_AUTHENTICATION:
		if (mtk_chg->cti->typec_mode == POWER_SUPPLY_TYPEC_NONE)
			mtk_chg->cti->pd_verifed = 0;
		val->intval = mtk_chg->cti->pd_verifed;
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		val->intval = chg_get_fastcharge_mode();
		break;
#endif
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		val->intval = get_quick_charge_type(mtk_chg);
		break;
	case POWER_SUPPLY_PROP_TYPE_RECHECK:
		mtk_charger_get_prop_type_recheck(val);
		break;
	case POWER_SUPPLY_PROP_PD_VERIFY_IN_PROCESS:
		mtk_charger_get_prop_pd_verify_process(val);
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
	int rc;

	if (!mtk_chg) {
		pr_notice("%s: no mtk chg data\n", __func__);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
		if (!mtk_chg->charger_identify_psy) {
			mtk_chg->charger_identify_psy = power_supply_get_by_name("charger_identify");
			if (!mtk_chg->charger_identify_psy)
				chr_err("%s: get power supply failed\n", __func__);
		} else {
			power_supply_set_property(mtk_chg->charger_identify_psy,
				POWER_SUPPLY_PROP_VBUS_DISABLE, val);
		}
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
		fake_temp = val->intval;
		break;
#ifdef CONFIG_MTBF_SUPPORT
	case POWER_SUPPLY_PROP_MTBF_CUR:
		mtk_chg->cti->mtbf_chg_curr = val->intval;
		break;
#endif
#ifdef CONFIG_BQ2597X_CHARGE_PUMP
	case POWER_SUPPLY_PROP_PD_TYPE:
		mtk_chg->cti->pd_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		mtk_chg->cti->pd_active = val->intval;
		break;
	case POWER_SUPPLY_PROP_PD_AUTHENTICATION:
		mtk_chg->cti->pd_verifed = val->intval;
		/*if set pd authentication auto set fastcharge mode*/
		/*do not break here*/
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		pr_info("ffc pd authentic enable ffc\n");
		chg_set_fastcharge_mode(val->intval);
		power_supply_changed(mtk_chg->usb_psy);
		break;
	case POWER_SUPPLY_PROP_TYPE_RECHECK:
		rc = mtk_charger_set_prop_type_recheck(val);
		break;
	case POWER_SUPPLY_PROP_PD_VERIFY_IN_PROCESS:
		rc = mtk_charger_set_prop_pd_verify_process(val);
		break;
#endif
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
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
	case POWER_SUPPLY_PROP_PD_AUTHENTICATION:
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
	case POWER_SUPPLY_PROP_PD_ACTIVE:
	case POWER_SUPPLY_PROP_PD_TYPE:
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
#ifdef CONFIG_MTBF_SUPPORT
	case POWER_SUPPLY_PROP_MTBF_CUR:
#endif
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

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
		val->intval = ibat_6360;
		break;
	case POWER_SUPPLY_PROP_TEMP_MAX:
		charger_dev_get_temperature(mtk_chg->chg1_dev, &tchg_min, &tchg_max);
		val->intval = tchg_max;
	case POWER_SUPPLY_PROP_TEMP_MIN:
		charger_dev_get_temperature(mtk_chg->chg1_dev, &tchg_min, &tchg_max);
		val->intval = tchg_min;
		break;
	case POWER_SUPPLY_PROP_TEMP_CONNECT:
		charger_dev_get_adc(mtk_chg->chg1_dev, ADC_CHANNEL_TS, &vts, &vts);
		val->intval = vts;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = mtk_chg->cti->voltage_max;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_main_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		mtk_chg->cti->voltage_max = val->intval;
		break;
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
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
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
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_TYPEC_POWER_ROLE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_HVDCP3_TYPE,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_CONNECTOR_TEMP,
	POWER_SUPPLY_PROP_VBUS_DISABLE,
#ifdef CONFIG_BQ2597X_CHARGE_PUMP
	POWER_SUPPLY_PROP_PD_AUTHENTICATION,
	POWER_SUPPLY_PROP_PD_ACTIVE,
	POWER_SUPPLY_PROP_FASTCHARGE_MODE,
	POWER_SUPPLY_PROP_PD_TYPE,
#endif
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TYPE_RECHECK,
	POWER_SUPPLY_PROP_PD_VERIFY_IN_PROCESS,
#ifdef CONFIG_MTBF_SUPPORT
	POWER_SUPPLY_PROP_MTBF_CUR,
#endif
};

static enum power_supply_property mt_main_properties[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP_MAX,
	POWER_SUPPLY_PROP_TEMP_MIN,
	POWER_SUPPLY_PROP_TEMP_CONNECT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static void tcpc_power_off_work_handler(struct work_struct *work)
{
	pr_info("%s\n", __func__);
	//kernel_power_off();
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
	/* 2021.01.21 longcheer jiangshitian change for mic noise begin */
	#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
	set_chg_exist_flag(en);
	#endif
	/* 2021.01.21 longcheer jiangshitian change for mic noise end */
	atomic_inc(&cti->chgdet_cnt);
	wake_up_interruptible(&cti->waitq);
	mutex_unlock(&cti->chgdet_lock);
}

static int get_source_mode(struct tcp_notify *noti)
{
	if (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC || noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)
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
	//int rc;
	union power_supply_propval pval = {0,};
	struct power_supply	*charger_identify_psy;
	struct power_supply	*usb_psy;

	charger_identify_psy = power_supply_get_by_name("Charger_Identify");
	usb_psy = power_supply_get_by_name("usb");

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			cti->cc_orientation = noti->typec_state.polarity;
			pr_info("%s USB Plug in, pol = %d, state = %d, rp_level = %d, kpoc = %d\n", __func__,
					noti->typec_state.polarity, noti->typec_state.new_state, noti->typec_state.rp_level, cti->tcpc_kpoc);
			cti->usb_plug = 1;
			cti->typec_mode = get_source_mode(noti);
			plug_in_out_handler(cti, true, false);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (cti->tcpc_kpoc) {
				vbus = battery_get_vbus();
				// fix vbus to 0v, when cdp plug in.
				if (charger_identify_psy) {
#ifdef CONFIG_XMUSB350_DET_CHG
					rc = power_supply_get_property(charger_identify_psy,
						POWER_SUPPLY_PROP_QC35_CHG_TYPE, &pval);
#endif
					if ((!vbus)
#ifdef CONFIG_XMUSB350_DET_CHG
							&& (!rc) && (pval.intval == QC35_CDP)
#endif
					) {
						mdelay(3000);
						vbus = battery_get_vbus();
						pr_info("%s vbus = %d\n", __func__, vbus);
						if (vbus)
							return NOTIFY_OK;
					}
				}
				pr_info("%s KPOC Plug out, vbus = %d\n",
					__func__, vbus);
				queue_work_on(cpumask_first(cpu_online_mask),
					      cti->pwr_off_wq,
					      &cti->pwr_off_work);
				plug_in_out_handler(cti, false, false);
				break;
			}
			pr_info("%s USB Plug out\n", __func__);
			cti->usb_plug = 0;
			cti->typec_mode = POWER_SUPPLY_TYPEC_NONE;
			if (usb_psy) {
				pval.intval = POWER_SUPPLY_PD_NONE;
				power_supply_set_property(usb_psy,
						POWER_SUPPLY_PROP_PD_TYPE, &pval);
			}
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
	case TCP_NOTIFY_EXT_DISCHARGE:
		if (noti->en_state.en == false && cti->typec_mode == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER)
			plug_in_out_handler(cti, false, false);
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

		if (noti->typec_state.old_state == TYPEC_UNATTACHED && noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO)
			plug_in_out_handler(cti, true, true);
		else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO && noti->typec_state.new_state == TYPEC_UNATTACHED)
			plug_in_out_handler(cti, false, false);

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

static void smblib_clear_soc_decimal_rate_work(struct work_struct *work)
{
	struct power_supply *bms_psy;
	union power_supply_propval pval;

	bms_psy = power_supply_get_by_name("bms");
	power_supply_get_property(bms_psy,
			 POWER_SUPPLY_PROP_MTK_SOC_DECIMAL_RATE, &pval);
	if (pval.intval == 100) {
		pval.intval = 0;
		power_supply_set_property(bms_psy,
				POWER_SUPPLY_PROP_MTK_SOC_DECIMAL_RATE, &pval);
		pr_info("%s: clear succ.\n", __func__);
	} else {
		 pr_info("%s: didn't need.\n", __func__);
	}
}

static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct chg_type_info *cti = NULL;
	struct mt_charger *mt_chg = NULL;
	#ifdef CONFIG_EXTCON_USB_CHG
	struct usb_extcon_info *info;
	#endif

	pr_info("%s\n", __func__);

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
	mt_chg->usb_desc.property_is_writeable = mt_usb_is_writeable,
	mt_chg->usb_desc.get_property = mt_usb_get_property;
	mt_chg->usb_desc.set_property = mt_usb_set_property;
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

	mt_chg->usb_psy = power_supply_register(&pdev->dev, &mt_chg->usb_desc,
		&mt_chg->usb_cfg);
	if (IS_ERR(mt_chg->usb_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->usb_psy));
		ret = PTR_ERR(mt_chg->usb_psy);
		goto err_usb_psy;
	}

	mt_chg->main_psy = power_supply_register(&pdev->dev, &mt_chg->main_desc,
		&mt_chg->main_cfg);
	if (IS_ERR(mt_chg->main_psy)) {
		dev_notice(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->main_psy));
		ret = PTR_ERR(mt_chg->main_psy);
		goto err_main_psy;
	}

	pr_err("%s: usb_psy ac_psy chg_psy main_psy power supply register succcess.\n", __func__);

#ifdef CONFIG_XMUSB350_DET_CHG
	/* Get Charger_Identify power supply */
	mt_chg->charger_identify_psy = power_supply_get_by_name("Charger_Identify");
	if (!mt_chg->charger_identify_psy) {
		pr_err("%s: get Charger_Identify power supply failed\n", __func__);
		//return -EINVAL;
	}
#endif
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
	cti->otg_nb.notifier_call = otg_tcp_notifier_call;
	ret = register_tcp_dev_notifier(cti->tcpc_dev,
		&cti->otg_nb, TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_info("%s: register otg tcpc notifer fail\n",
			__func__);
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

	#ifdef CONFIG_EXTCON_USB_CHG
	info = devm_kzalloc(mt_chg->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = mt_chg->dev;
	mt_chg->extcon_info = info;

	INIT_DELAYED_WORK(&mt_chg->extcon_work, init_extcon_work);
	schedule_delayed_work(&mt_chg->extcon_work, 0);
	#endif

	INIT_DELAYED_WORK(&mt_chg->clear_soc_decimal_rate_work, smblib_clear_soc_decimal_rate_work);

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


	cancel_delayed_work_sync(&mt_charger->clear_soc_decimal_rate_work);

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

	if (charger_manager_pd_is_online())
		return STANDARD_CHARGER;

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
