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

/*
 *
 * Filename:
 * ---------
 *    mtk_switch_charging.c
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

#include <mt-plat/mtk_boot.h>
#include "mtk_charger_intf.h"

#include "mtk_charger_init.h"
#include "mtk_switch_charging.h"
#include "mtk_intf.h"

extern bool get_charging_call_state(void);
extern int get_mtbf_current(void);
#define BAT_CURR_2000MA       2000000
#define BAT_CURR_100MA         100000

#define CHG_BAT_TEMP_CHG_MIN        (-100)
#define CHG_BAT_SCP_TEMP_MIN		 100
#define CHG_BAT_TEMP_MIN      		 150
#define CHG_BAT_TEMP_MAX      		 480
#define CHG_BAT_TEMP_DISCHG          600

#define CHG_NORMAL_TERM_CUR     256000
#define CHG_FFS_TERM1_CUR      	896000
#define CHG_FFS_TERM2_CUR      	960000
#define CHG_FFS_TERM3_CUR      	1024000

extern bool IS_STD_BATTERY;
#define  NOSTD_BAT_INPUTCURR_LIMIT 	2000000
#define  NOSTD_BAT_CHGCURR_LIMIT 		2048000
static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

static void _disable_all_charging(struct charger_manager *info)
{
	charger_dev_enable(info->chg1_dev, false);

	if (mtk_pe20_get_is_enable(info)) {
		mtk_pe20_set_is_enable(info, false);
		if (mtk_pe20_get_is_connect(info))
			mtk_pe20_reset_ta_vchr(info);
	}

	if (mtk_pe_get_is_enable(info)) {
		mtk_pe_set_is_enable(info, false);
		if (mtk_pe_get_is_connect(info))
			mtk_pe_reset_ta_vchr(info);
	}

	if (info->enable_pe_5)
		pe50_stop();

	if (info->enable_pe_4)
		pe40_stop();

	if (pdc_is_ready())
		pdc_stop();
}

extern bool is_usb_pd;
static void swchg_select_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata = NULL;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	u32 ichg1_min = 0, aicr1_min = 0;
	int charging_current_limit;
	int mtbf_current;
	struct power_supply *bms = NULL;
	struct power_supply *apdo_psy = NULL;
	union power_supply_propval val = {0,};
	int ret = 0;
	u32 bat_vol = 0;
	int current_now;
	int pd_auth = 0, batt_auth = 0, last_ichg = 0;
	int bat_temp = 0;

	apdo_psy = power_supply_get_by_name("usb");
	if (!apdo_psy) {
		pr_err("apdo psy not found!\n");
		return;
	}

	ret = power_supply_get_property(apdo_psy,
			POWER_SUPPLY_PROP_PD_AUTHENTICATION, &val);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		pd_auth = val.intval;

	bms = power_supply_get_by_name("bms");
	if (!bms) {
  		pr_err("%s %d: get power supply failed!\n", __func__, __LINE__);
		return;
	}

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_AUTHENTIC, &val);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		batt_auth = val.intval;

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (ret)
		pr_err("Failed to read volt\n");
	else
		bat_vol = val.intval / 1000;

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (ret)
		pr_err("Failed to read ibat\n");
	else
		current_now = val.intval;

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		pr_err("Failed to read bat temp\n");
	else
		bat_temp = val.intval;
	pr_err("wt read bat temp = %d\n", bat_temp);
	if (info->pe5.online) {
		chr_err("In PE5.0\n");
		return;
	}

	pdata = &info->chg1_data;
	mutex_lock(&swchgalg->ichg_aicr_access_mutex);

	/* AICL */
	if (!mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info) && !mtk_pdc_check_charger(info)) {
		charger_dev_run_aicl(info->chg1_dev,
				&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
	}

	if (pdata->force_charging_current > 0) {

		pdata->charging_current_limit = pdata->force_charging_current;
		if (pdata->force_charging_current <= 450000) {
			pdata->input_current_limit = 500000;
		} else {
			pdata->input_current_limit =
					info->data.ac_charger_input_current;
			pdata->charging_current_limit =
					info->data.ac_charger_current;
		}
		goto done;
	}

	if (info->usb_unlimited) {
		pdata->input_current_limit = 2000000;

		pdata->charging_current_limit =
					info->data.ac_charger_current;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		goto done;
	}

	if ((get_boot_mode() == META_BOOT) ||
	    (get_boot_mode() == ADVMETA_BOOT)) {
		pdata->input_current_limit = 200000; /* 200mA */
		goto done;
	}

	if (info->atm_enabled == true && (info->chr_type == STANDARD_HOST ||
	    info->chr_type == CHARGING_HOST)) {
		pdata->input_current_limit = 500000; /* 500mA */
		pdata->charging_current_limit = 400000;
		goto done;
	}

	if (info->chr_type == PPS_CHARGER) {
		pr_err("%s %d is_usb_pd = %d\n", __func__, __LINE__, is_usb_pd);
		if (is_usb_pd) {
			pdata->input_current_limit = 1500000;
			pdata->charging_current_limit = 1500000;
		} else {
			pdata->input_current_limit = 3000000;
			pdata->charging_current_limit = 3000000;
			if (bat_vol >= 4450)
				pdata->charging_current_limit = 2000000;
		}
		chr_err("PPS_CHARGER: ibus_curr :%d, ibat_curr : %d \n", pdata->input_current_limit,pdata->charging_current_limit);
	}else if (is_typec_adapter(info)) {
		if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
			== 3000) {
			pdata->input_current_limit = 2000000;
			pdata->charging_current_limit = 2000000;
		} else if (adapter_dev_get_property(info->pd_adapter,
			TYPEC_RP_LEVEL) == 1500) {
			pdata->input_current_limit =1500000;
			pdata->charging_current_limit = 2000000;
		} else {
			chr_err("type-C: inquire rp error\n");
			pdata->input_current_limit = 500000;
			pdata->charging_current_limit = 500000;
		}

		chr_err("type-C:%d current:%d\n",
			info->pd_type,
			adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL));
	} else if (info->chr_type == STANDARD_HOST) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)) {
			if (info->usb_state == USB_SUSPEND)
				pdata->input_current_limit =
					info->data.usb_charger_current_suspend;
			else if (info->usb_state == USB_UNCONFIGURED)
				pdata->input_current_limit =
				info->data.usb_charger_current_unconfigured;
			else if (info->usb_state == USB_CONFIGURED)
				pdata->input_current_limit =
				info->data.usb_charger_current_configured;
			else
				pdata->input_current_limit =
				info->data.usb_charger_current_unconfigured;

			pdata->charging_current_limit =
					pdata->input_current_limit;
		} else {
			pdata->input_current_limit =
					info->data.usb_charger_current;
			/* it can be larger */
			pdata->charging_current_limit = 
					info->data.usb_charger_current;
		}
	} else if (info->chr_type == NONSTANDARD_CHARGER) {
		pdata->input_current_limit =
				info->data.non_std_ac_charger_current;
		pdata->charging_current_limit =
				info->data.non_std_ac_charger_current;
	} else if (info->chr_type == STANDARD_CHARGER) {
		pdata->input_current_limit =
				info->data.ac_charger_input_current;
		pdata->charging_current_limit = 2000000;
		if (charger_manager_pd_is_online()) {
			pdata->input_current_limit = 3000000;
			pdata->charging_current_limit = 3000000;
		}
		mtk_pe20_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);
		mtk_pe_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);
	} else if (info->chr_type == CHARGING_HOST) {
		pdata->input_current_limit =
				info->data.charging_host_charger_current;
		pdata->charging_current_limit =
				info->data.charging_host_charger_current;
	} else if (info->chr_type == APPLE_1_0A_CHARGER) {
		pdata->input_current_limit =
				info->data.apple_1_0a_charger_current;
		pdata->charging_current_limit =
				info->data.apple_1_0a_charger_current;
	} else if (info->chr_type == APPLE_2_1A_CHARGER) {
		pdata->input_current_limit =
				info->data.apple_2_1a_charger_current;
		pdata->charging_current_limit =
				info->data.apple_2_1a_charger_current;
	} else if (info->chr_type == CHECK_HV) {
		pdata->input_current_limit =
				info->data.ac_charger_input_current;
		pdata->charging_current_limit =
				info->data.ac_charger_current;
	} else if (info->chr_type == HVDCP_CHARGER) {
		pdata->input_current_limit =
				info->data.ac_charger_input_current;
		pdata->charging_current_limit =
				info->data.ac_charger_current;
	} else if (info->chr_type == CHARGER_UNKNOWN) {
		pdata->input_current_limit = 0;
		pdata->charging_current_limit = 0;
		chr_err("chr_type is charger_unknown\n");
	}
#ifndef CONFIG_MTK_DISABLE_TEMP_PROTECT
	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
		    && info->chr_type == STANDARD_HOST)
			chr_err("USBIF & STAND_HOST skip current check\n");
		else {
			/*
					check min chg between jeita_cc and chg_type_cc;
			*/
			pdata->charging_current_limit =min(info->sw_jeita.cc ,pdata->charging_current_limit) ; 
		}
	}


	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
	}
#endif

	if (pdata->input_current_limit_by_aicl != -1 &&
	    !mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info)) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
done:
#if 0	
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = 0;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = 0;
#endif
	chr_err("force:%d thermal:%d,%d pe4:%d,%d,%d setting:%d %d sc:%d,%d,%d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d,thermal_mitigation_current:%d,info->cp_status:%d \n",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit_setting),
		_uA_to_mA(info->pe4.input_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(info->sc.pre_ibat),
		_uA_to_mA(info->sc.sc_ibat),
		info->sc.solution,
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		_uA_to_mA(info->thermal_mitigation_current),
		info->cp_status);
	if(info->chr_type == PPS_CHARGER){
		if((info->thermal_mitigation_current) >= BAT_CURR_2000MA && (info->cp_status)) {
			pdata->input_current_limit = BAT_CURR_100MA;
			charging_current_limit = BAT_CURR_100MA;
		} else {
			charging_current_limit = min(pdata->charging_current_limit,info->thermal_mitigation_current);
			if (pd_auth && batt_auth) {
				if ((bat_vol >= 4473) || (bat_vol >= 4440 && (bat_temp >= CHG_BAT_TEMP_CHG_MIN && bat_temp < CHG_BAT_TEMP_MIN))
					|| (bat_vol >= 4090 && (bat_temp >= CHG_BAT_TEMP_MAX && bat_temp < CHG_BAT_TEMP_DISCHG))) {
					if (current_now > (info->iterm_curr)) {
						charging_current_limit = current_now - 64000;
						if ((info->iterm_curr == CHG_NORMAL_TERM_CUR) && (charging_current_limit < CHG_NORMAL_TERM_CUR)) {
							charging_current_limit = CHG_NORMAL_TERM_CUR;
						}
						if (bat_vol >= 4477) {
							charging_current_limit = current_now - 128000;
							pr_info("batt_volt %d too high, curr-128=%d\n", __func__, bat_vol, charging_current_limit);
						}
					} else {
						charger_dev_get_charging_current(info->chg1_dev, &last_ichg);
						charging_current_limit = min(info->iterm_curr, last_ichg);
					}
				} else if ((bat_vol >= 4450 && (current_now > 800000)) || (bat_vol >= 4420 && (bat_temp >= CHG_BAT_TEMP_CHG_MIN && bat_temp < CHG_BAT_TEMP_MIN))
					|| (bat_vol >= 4080 && (bat_temp >= CHG_BAT_TEMP_MAX && bat_temp < CHG_BAT_TEMP_DISCHG))) {
					charger_dev_get_charging_current(info->chg1_dev, &last_ichg);
					charging_current_limit = min(pdata->charging_current_limit, last_ichg);
				}
				pr_err("%s batt_volt:%d curr_now:%d iterm:%d ichg:%d", __func__, bat_vol,
						current_now, info->iterm_curr, charging_current_limit);
			} else {
				if (bat_vol >= 4445){
					if (current_now > info->iterm_curr) {
						charging_current_limit = current_now - 64000;
					} else {
						charger_dev_get_charging_current(info->chg1_dev, &last_ichg);
						charging_current_limit = min(info->iterm_curr, last_ichg);
					}
				} else if (bat_vol >= 4430 && (current_now > 200000)){
					charger_dev_get_charging_current(info->chg1_dev, &last_ichg);
					charging_current_limit = min(pdata->charging_current_limit, last_ichg);
				}
				pr_err("%s batt_volt:%d curr_now:%d iterm:%d ichg:%d", __func__, bat_vol,
						current_now, info->iterm_curr, charging_current_limit);
			}
		}
	} else {
		charging_current_limit = min(pdata->charging_current_limit,info->thermal_mitigation_current);
		if (bat_vol >= 4445){
			if (current_now > info->iterm_curr) {
					charging_current_limit = current_now - 64000;
			} else {
				charger_dev_get_charging_current(info->chg1_dev, &last_ichg);
				charging_current_limit = min(info->iterm_curr, last_ichg);
			}
		} else if (bat_vol >= 4430 && (current_now > 200000)){
			charger_dev_get_charging_current(info->chg1_dev, &last_ichg);
			charging_current_limit = min(pdata->charging_current_limit, last_ichg);
		}
		pr_err("%s batt_volt:%d curr_now:%d iterm:%d ichg:%d", __func__, bat_vol,
				current_now, info->iterm_curr, charging_current_limit);
	}

	if (get_charging_call_state()) {
		charging_current_limit = min(1000000,charging_current_limit);
		chr_err("is charging call state:%d\n",_uA_to_mA(charging_current_limit));
	}

	mtbf_current = get_mtbf_current();
	if (mtbf_current == 1500  && info->chr_type == CHARGING_HOST) {
		charging_current_limit = mtbf_current * 1000;
		pdata->input_current_limit = mtbf_current * 1000;
		chr_err("pdata->charging_current_limit = %d, pdata->input_current_limit = %d, gm.mtbf_current= %d\n",
				 charging_current_limit, pdata->input_current_limit, mtbf_current);
	}
	if((!IS_STD_BATTERY) && ((pdata->input_current_limit > NOSTD_BAT_INPUTCURR_LIMIT) 
						|| (charging_current_limit >  NOSTD_BAT_CHGCURR_LIMIT))){
		charger_dev_set_input_current(info->chg1_dev,NOSTD_BAT_INPUTCURR_LIMIT);
		charger_dev_set_charging_current(info->chg1_dev,NOSTD_BAT_CHGCURR_LIMIT);
	}else{
		charger_dev_set_input_current(info->chg1_dev,pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,charging_current_limit);
	}
	/* If AICR < 300mA, stop PE+/PE+20 */
	if (pdata->input_current_limit < 300000) {
		if (mtk_pe20_get_is_enable(info)) {
			mtk_pe20_set_is_enable(info, false);
			if (mtk_pe20_get_is_connect(info))
				mtk_pe20_reset_ta_vchr(info);
		}

		if (mtk_pe_get_is_enable(info)) {
			mtk_pe_set_is_enable(info, false);
			if (mtk_pe_get_is_connect(info))
				mtk_pe_reset_ta_vchr(info);
		}
	}

	/*
	 * If thermal current limit is larger than charging IC's minimum
	 * current setting, enable the charger immediately
	 */
	if (pdata->input_current_limit > aicr1_min &&
	    pdata->charging_current_limit > ichg1_min && info->can_charging)
		charger_dev_enable(info->chg1_dev, true);
	mutex_unlock(&swchgalg->ichg_aicr_access_mutex);
}

static void swchg_select_cv(struct charger_manager *info)
{
	u32 constant_voltage;
	int pd_auth = 0, batt_auth = 0;
	struct power_supply *apdo_psy = NULL;
	struct power_supply *bms = NULL;
	union power_supply_propval val = {0,};
	int ret = 0;
	u32 bat_vol = 0;
	u32 current_now;
	int bat_temp = 0;

	apdo_psy = power_supply_get_by_name("usb");
	if (!apdo_psy) {
		pr_err("apdo psy not found!\n");
		return;
	}

	ret = power_supply_get_property(apdo_psy,
			POWER_SUPPLY_PROP_PD_AUTHENTICATION, &val);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		pd_auth = val.intval;

	bms = power_supply_get_by_name("bms");
	if (!bms) {
  		pr_err("%s %d: get power supply failed!\n", __func__, __LINE__);
		return;
	}

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_AUTHENTIC, &val);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		batt_auth = val.intval;

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (ret)
		pr_err("Failed to read volt\n");
	else
		bat_vol = val.intval / 1000;

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (ret)
		pr_err("Failed to read ibat\n");
	else
		current_now = val.intval;

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		pr_err("Failed to read ibat\n");
	else
		bat_temp = val.intval;

	if (info->enable_sw_jeita) {
		chr_err("swchg_select_cv bat :%d temp:%d\n", bat_vol, bat_temp);
		if (bat_temp >= CHG_BAT_TEMP_MIN && bat_temp < CHG_BAT_TEMP_MAX) {
			if (batt_auth && pd_auth) {
				constant_voltage = 4496000;
				chr_err("swchg_select_cv 4496\n");
				if (bat_vol < 4478) {
					constant_voltage = 4496000;
					chr_err("swchg_select_cv 4496\n");
				} else if (current_now < (info->iterm_curr+64000)) {
					constant_voltage = 4448000;
					chr_err("swchg_select_cv 4448\n");
				} else {
					constant_voltage = 4480000;
					chr_err("swchg_select_cv 4480\n");
				}
			} else {
				constant_voltage = info->sw_jeita.cv;
				chr_err("swchg_select_cv %d \n",info->sw_jeita.cv);
			}
		} else {
			constant_voltage = info->sw_jeita.cv;
			chr_err("swchg_select_cv %d \n",info->sw_jeita.cv);
		}
	} else {
			constant_voltage = 4450000;
			chr_err("swchg_select_cv %d \n", 4450000);
	}

	if (bat_vol < (((constant_voltage/1000)*102)/100))
		charger_dev_set_constant_voltage(info->chg1_dev, constant_voltage);

	chr_err("swchg_select_cv bat :%d cv:%d\n", bat_vol, constant_voltage);
}

static int get_battery_id(void) {
	int ret = 0;
	union power_supply_propval propval;
	struct power_supply *bms = NULL;

	bms = power_supply_get_by_name("bms");
	if (!bms) {
		pr_err("%s: get power supply failed\n", __func__);
		return 2;
	}
	ret = power_supply_get_property(bms,POWER_SUPPLY_PROP_MI_BATTERY_ID,&propval);
	if (ret < 0) {
		pr_err("%s: psy type failed, ret = %d\n", __func__, ret);
		return 2;
	}
	return propval.intval;
}

static int get_battery_temp(void)
{
	struct power_supply *battery_psy;
	union power_supply_propval pval = {0, };

	battery_psy = power_supply_get_by_name("bms");
	if(battery_psy)
		power_supply_get_property(battery_psy,
			POWER_SUPPLY_PROP_TEMP, &pval);
	return pval.intval;
}

static int swchg_select_eoc(struct charger_manager *info)
{
	int temp, batt_id = 2;
	bool charge_full;
	int pd_auth = 0, batt_auth = 0;
	struct power_supply *apdo_psy = NULL;
	struct power_supply *bms = NULL;
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!info)
		return -EINVAL;

	apdo_psy = power_supply_get_by_name("usb");
	if (!apdo_psy) {
		pr_err("apdo psy not found!\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(apdo_psy,
			POWER_SUPPLY_PROP_PD_AUTHENTICATION, &val);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		pd_auth = val.intval;

	bms = power_supply_get_by_name("bms");
	if (!bms) {
  		pr_err("%s %d: get power supply failed!\n", __func__, __LINE__);
		return 0;
	}

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_AUTHENTIC, &val);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		batt_auth = val.intval;

	batt_id = get_battery_id();
	temp = get_battery_temp();
	charger_dev_is_charging_done(info->chg1_dev, &charge_full);
	if(!charge_full)
		charge_full = 0;

	if (batt_auth && pd_auth) {
		if (temp >= 150 && (temp < 350)) {
			info->iterm_curr = CHG_FFS_TERM1_CUR;
		} else if (temp >= 350 && (temp < 480)) {
			if (batt_id == 0) {//SWN 960
				info->iterm_curr = CHG_FFS_TERM2_CUR;
			} else if (batt_id == 1) { //NVT 1024
				info->iterm_curr = CHG_FFS_TERM3_CUR;
			}
		} else if (temp >= 480) {
			info->iterm_curr = CHG_NORMAL_TERM_CUR;
		} else {
			info->iterm_curr = CHG_NORMAL_TERM_CUR;
		}
	} else {
		info->iterm_curr = CHG_NORMAL_TERM_CUR;
	}

	pr_err("batt_auth %d, pd_auth %d, batt_id %d temp %d,charger_term %d\n",
		batt_auth, pd_auth, batt_id, temp, info->iterm_curr);

	charger_dev_set_eoc_current(info->chg1_dev, info->iterm_curr);
	return 0;
}

#define CP_CHARGING_CV			(4608000)
static void swchg_turn_on_charging(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	bool charging_enable = true;
	struct power_supply *bms = NULL;
	struct power_supply *apdo_psy = NULL;
	union power_supply_propval propval;
	int pd_auth = 0, batt_auth = 0, ret = 0;

	apdo_psy = power_supply_get_by_name("usb");
	if (!apdo_psy) {
		pr_err("apdo psy not found!\n");
		return;
	}

	ret = power_supply_get_property(apdo_psy,
			POWER_SUPPLY_PROP_PD_AUTHENTICATION, &propval);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		pd_auth = propval.intval;

	bms = power_supply_get_by_name("bms");
	if (!bms) {
  		pr_err("%s %d: get power supply failed!\n", __func__, __LINE__);
		return;
	}

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_AUTHENTIC, &propval);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		batt_auth = propval.intval;

	if (swchgalg->state == CHR_ERROR) {
		charging_enable = false;
		chr_err("[charger]Charger Error, turn OFF charging !\n");
	} else if ((get_boot_mode() == META_BOOT) ||
			((get_boot_mode() == ADVMETA_BOOT))) {
		charging_enable = false;
		info->chg1_data.input_current_limit = 200000; /* 200mA */
		charger_dev_set_input_current(info->chg1_dev,
					info->chg1_data.input_current_limit);
		chr_err("In meta mode, disable charging and set input current limit to 200mA\n");
	} else {
		mtk_pe20_start_algorithm(info);
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_start_algorithm(info);

		swchg_select_charging_current_limit(info);
		swchg_select_eoc(info);
		if (info->chg1_data.input_current_limit == 0
		    || info->chg1_data.charging_current_limit == 0) {
			charging_enable = false;
			chr_err("[charger]charging current is set 0mA, turn off charging !\n");
		} else {
			if(info->cp_status) {
				charger_dev_enable_termination(info->chg1_dev, false);
				charger_dev_set_constant_voltage(info->chg1_dev, CP_CHARGING_CV);
			} else {
				charger_dev_enable_termination(info->chg1_dev, true);
				swchg_select_cv(info);
			}
			if (pd_auth && batt_auth)
				propval.intval = true;
			else
				propval.intval = false;
			chr_err("[charger]charger_dev_set_constant_voltage!\n");
		}
	}

	power_supply_set_property(bms,
			POWER_SUPPLY_PROP_FASTCHARGE_MODE, &propval);
	charger_dev_enable(info->chg1_dev, charging_enable);
}

static int mtk_switch_charging_plug_in(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	swchgalg->state = CHR_CC;
	info->polling_interval = CHARGING_INTERVAL;
	swchgalg->disable_charging = false;
	get_monotonic_boottime(&swchgalg->charging_begin_time);

	return 0;
}

static int mtk_switch_charging_plug_out(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	swchgalg->total_charging_time = 0;
	   
	mtk_pe20_set_is_cable_out_occur(info, true);
	mtk_pe_set_is_cable_out_occur(info, true);
	mtk_pdc_plugout(info);

	if (info->enable_pe_5)
		pe50_stop();

	if (info->enable_pe_4)
		pe40_stop();

	info->leave_pe5 = false;
	info->leave_pe4 = false;
	info->leave_pdc = false;

	return 0;
}

static int mtk_switch_charging_do_charging(struct charger_manager *info,
						bool en)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	chr_err("%s: en:%d %s\n", __func__, en, info->algorithm_name);
	if (en) {
		swchgalg->disable_charging = false;
		swchgalg->state = CHR_CC;
		get_monotonic_boottime(&swchgalg->charging_begin_time);
		charger_manager_notifier(info, CHARGER_NOTIFY_NORMAL);
	} else {
		/* disable charging might change state, so call it first */
		_disable_all_charging(info);
		swchgalg->disable_charging = true;
		swchgalg->state = CHR_ERROR;
		charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
	}

	return 0;
}

static int mtk_switch_chr_pe50_init(struct charger_manager *info)
{
	int ret;

	ret = pe50_init();

	if (ret == 0)
		set_charger_manager(info);
	else
		chr_err("pe50 init fail\n");

	info->leave_pe5 = false;

	return ret;
}

static int mtk_switch_chr_pe50_run(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	/* struct charger_custom_data *pdata = &info->data; */
	/* struct pe50_data *data; */
	int ret = 0;

	if (info->enable_hv_charging == false)
		goto stop;

	ret = pe50_run();

	if (ret == 1) {
		pr_info("retry pe5\n");
		goto retry;
	}

	if (ret == 2) {
		chr_err("leave pe5\n");
		info->leave_pe5 = true;
		swchgalg->state = CHR_CC;
	}

	return 0;

stop:
	pe50_stop();
retry:
	swchgalg->state = CHR_CC;

	return 0;
}


static int mtk_switch_chr_pe40_init(struct charger_manager *info)
{
	int ret;

	ret = pe40_init();

	if (ret == 0)
		set_charger_manager(info);

	info->leave_pe4 = false;

	return 0;
}

static int select_pe40_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret = 0;

	pdata = &info->chg1_data;

	pdata->input_current_limit =
		info->data.pe40_single_charger_input_current;
	pdata->charging_current_limit =
		info->data.pe40_single_charger_current;

	sc_select_charging_current(info, pdata);

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
	}

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = 0;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = 0;

	chr_err("force:%d thermal:%d,%d setting:%d %d sc:%d %d %d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d\n",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled);

	return 0;
}

static int mtk_switch_chr_pe40_run(struct charger_manager *info)
{
	struct charger_custom_data *pdata = &info->data;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct pe40_data *data = NULL;
	int ret = 0;

	charger_dev_enable(info->chg1_dev, true);
	select_pe40_charging_current_limit(info);

	data = pe40_get_data();
	if (!data) {
		chr_err("%s: data is NULL\n", __func__);
		goto stop;
	}

	data->input_current_limit = info->chg1_data.input_current_limit;
	data->charging_current_limit = info->chg1_data.charging_current_limit;
	data->pe40_max_vbus = pdata->pe40_max_vbus;
	data->high_temp_to_leave_pe40 = pdata->high_temp_to_leave_pe40;
	data->high_temp_to_enter_pe40 = pdata->high_temp_to_enter_pe40;
	data->low_temp_to_leave_pe40 = pdata->low_temp_to_leave_pe40;
	data->low_temp_to_enter_pe40 = pdata->low_temp_to_enter_pe40;
	data->pe40_r_cable_1a_lower = pdata->pe40_r_cable_1a_lower;
	data->pe40_r_cable_2a_lower = pdata->pe40_r_cable_2a_lower;
	data->pe40_r_cable_3a_lower = pdata->pe40_r_cable_3a_lower;

	data->battery_cv = pdata->battery_cv;
	if (info->enable_sw_jeita) {
		if (info->sw_jeita.cv != 0)
			data->battery_cv = info->sw_jeita.cv;
	}

	if (info->enable_hv_charging == false)
		goto stop;
	if (info->pd_reset == true) {
		chr_err("encounter hard reset, stop pe4.0\n");
		info->pd_reset = false;
		goto stop;
	}

	ret = pe40_run();

	if (ret == 1) {
		chr_err("retry pe4\n");
		goto retry;
	}

	if (ret == 2 &&
		info->chg1_data.thermal_charging_current_limit == -1 &&
		info->chg1_data.thermal_input_current_limit == -1) {
		chr_err("leave pe4\n");
		info->leave_pe4 = true;
		swchgalg->state = CHR_CC;
	}

	return 0;

stop:
	pe40_stop();
retry:
	swchgalg->state = CHR_CC;

	return 0;
}


static int mtk_switch_chr_pdc_init(struct charger_manager *info)
{
	int ret;

	ret = pdc_init();

	if (ret == 0)
		set_charger_manager(info);

	info->leave_pdc = false;

	return 0;
}

static int select_pdc_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret = 0;

	pdata = &info->chg1_data;

	if ((info->chr_type == HVDCP_CHARGER)
			|| (info->chr_type == CHECK_HV)
			|| (info->chr_type == STANDARD_CHARGER)) {
		pdata->input_current_limit =
			info->data.pd_charger_current;
		pdata->charging_current_limit =
			info->data.pd_charger_current;
	} else if (info->chr_type == STANDARD_HOST) {
		pdata->input_current_limit = 1500000;
		pdata->charging_current_limit = 1500000;
	} else {
		pdata->input_current_limit = 0;
		pdata->charging_current_limit = 0;
	}

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
	}

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = 0;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = 0;

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
		    && info->chr_type == STANDARD_HOST)
			chr_err("USBIF & STAND_HOST skip current check\n");
		else {
			pdata->charging_current_limit = info->sw_jeita.cc;
		}
	}
	if (get_charging_call_state()) {
		pdata->charging_current_limit = min(1000000,pdata->charging_current_limit);
		chr_err("is charging call state:%d\n",_uA_to_mA(pdata->charging_current_limit));
	}
	charger_dev_set_input_current(info->chg1_dev,pdata->input_current_limit);
	charger_dev_set_charging_current(info->chg1_dev,
					pdata->charging_current_limit);

	chr_err("force:%d thermal:%d,%d setting:%d %d sc:%d %d %d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d\n",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled);

	return 0;
}

static int mtk_switch_chr_pdc_run(struct charger_manager *info)
{
	struct charger_custom_data *pdata = &info->data;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct pdc_data *data = NULL;
	int ret = 0;

	charger_dev_enable(info->chg1_dev, true);
	select_pdc_charging_current_limit(info);

	data = pdc_get_data();

	data->input_current_limit = info->chg1_data.input_current_limit;
	data->charging_current_limit = info->chg1_data.charging_current_limit;
	data->pd_vbus_low_bound = pdata->pd_vbus_low_bound;
	data->pd_vbus_upper_bound = pdata->pd_vbus_upper_bound;

	data->battery_cv = pdata->battery_cv;
	if (info->enable_sw_jeita) {
		if (info->sw_jeita.cv != 0)
			data->battery_cv = info->sw_jeita.cv;
	}

	if (info->enable_hv_charging == false)
		goto stop;

	ret = pdc_run();

	if (ret == 2 &&
		info->chg1_data.thermal_charging_current_limit == -1 &&
		info->chg1_data.thermal_input_current_limit == -1) {
		chr_err("leave pdc\n");
		info->leave_pdc = true;
		swchgalg->state = CHR_CC;
	}

	return 0;

stop:
	pdc_stop();
	swchgalg->state = CHR_CC;

	return 0;
}


/* return false if total charging time exceeds max_charging_time */
static bool mtk_switch_check_charging_time(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct timespec time_now;

	if (info->enable_sw_safety_timer) {
		get_monotonic_boottime(&time_now);
		chr_debug("%s: begin: %ld, now: %ld\n", __func__,
			swchgalg->charging_begin_time.tv_sec, time_now.tv_sec);

		if (swchgalg->total_charging_time >=
		    info->data.max_charging_time) {
			chr_err("%s: SW safety timeout: %d sec > %d sec\n",
				__func__, swchgalg->total_charging_time,
				info->data.max_charging_time);
			charger_dev_notify(info->chg1_dev,
					CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
			return false;
		}
	}

	return true;
}

static int mtk_switch_chr_cc(struct charger_manager *info)
{
	bool chg_done = false;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct timespec time_now, charging_time;
	int tmp = battery_get_bat_temperature();
        struct power_supply *psy_usb = NULL;

	/* check bif */
	if (IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)) {
		if (pmic_is_bif_exist() != 1) {
			chr_err("CONFIG_MTK_BIF_SUPPORT but no bif , stop charging\n");
			swchgalg->state = CHR_ERROR;
			charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
		}
	}

	get_monotonic_boottime(&time_now);
	charging_time = timespec_sub(time_now, swchgalg->charging_begin_time);

	swchgalg->total_charging_time = charging_time.tv_sec;

	chr_err("pe40_ready:%d pps:%d hv:%d thermal:%d,%d tmp:%d,%d,%d\n",
		info->enable_pe_4,
		pe40_is_ready(),
		info->enable_hv_charging,
		info->chg1_data.thermal_charging_current_limit,
		info->chg1_data.thermal_input_current_limit,
		tmp,
		info->data.high_temp_to_enter_pe40,
		info->data.low_temp_to_enter_pe40);

	if (info->enable_pe_5 && pe50_is_ready() && !info->leave_pe5) {
		if (info->enable_hv_charging == true) {
			chr_err("enter PE5.0\n");
			swchgalg->state = CHR_PE50;
			info->pe5.online = true;
			return 1;
		}
	}

	if (info->enable_pe_4 &&
		pe40_is_ready() &&
		!info->leave_pe4) {
		if (info->enable_hv_charging == true &&
			info->chg1_data.thermal_charging_current_limit == -1 &&
			info->chg1_data.thermal_input_current_limit == -1) {
			chr_err("enter PE4.0!\n");
			swchgalg->state = CHR_PE40;
			return 1;
		}
	}
	chr_err("pdc_is_ready:%d,%d,%d\n",pdc_is_ready(),info->leave_pdc,info->enable_hv_charging);
#if 0
	if (pdc_is_ready() &&
		!info->leave_pdc) {
		if (info->enable_hv_charging == true) {
			chr_err("enter PDC!\n");
			swchgalg->state = CHR_PDC;
			return 1;
		}
	}
#endif
        if (info->pre_batt_tmp >= 48 && tmp < 48){
             psy_usb = power_supply_get_by_name("usb");
		if (!IS_ERR_OR_NULL(psy_usb)){
			power_supply_changed(psy_usb);
              	}
	}
	info->pre_batt_tmp = tmp;
	swchg_turn_on_charging(info);
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	if (chg_done) {
		swchgalg->state = CHR_BATFULL;
		charger_dev_do_event(info->chg1_dev, EVENT_EOC, 0);
		chr_err("battery full!\n");
	}

	/* If it is not disabled by throttling,
	 * enable PE+/PE+20, if it is disabled
	 */
	if (info->chg1_data.thermal_input_current_limit != -1 &&
		info->chg1_data.thermal_input_current_limit < 300)
		return 0;

	if (!mtk_pe20_get_is_enable(info)) {
		mtk_pe20_set_is_enable(info, true);
		mtk_pe20_set_to_check_chr_type(info, true);
	}

	if (!mtk_pe_get_is_enable(info)) {
		mtk_pe_set_is_enable(info, true);
		mtk_pe_set_to_check_chr_type(info, true);
	}
	return 0;
}

static int mtk_switch_chr_err(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	if (info->enable_sw_jeita) {
		if ((info->battery_temp >= TEMP_LCD_ON_T7) ||
			(info->battery_temp < TEMP_LCD_ON_NEG_10))
			info->sw_jeita.error_recovery_flag = false;

		if ((info->sw_jeita.error_recovery_flag == false) &&
			(info->battery_temp < TEMP_LCD_ON_T7) &&
			(info->battery_temp >= TEMP_LCD_ON_NEG_10)) {
			info->sw_jeita.error_recovery_flag = true;
			swchgalg->state = CHR_CC;
			get_monotonic_boottime(&swchgalg->charging_begin_time);
		}
	}

	swchgalg->total_charging_time = 0;

	_disable_all_charging(info);
	return 0;
}

#define CDP_RECHARGE_RAW_SOC    9540
#define SDP_RECHARGE_RAW_SOC    9750
static int mtk_switch_chr_full(struct charger_manager *info)
{
	bool chg_done = false;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	int raw_soc = 0, ret = 0, rm_soc = 0, fcc_soc = 0;
	struct power_supply *bms = NULL;
	struct power_supply *psy_usb = NULL;
	union power_supply_propval propval;
	static int over_heat_flag;

	bms = power_supply_get_by_name("bms");
	if (!bms) {
  		pr_err("%s %d: get power supply failed!\n", __func__, __LINE__);
		return 0;
	}
 
 	ret = power_supply_get_property(bms, POWER_SUPPLY_PROP_BQ_TRUE_RM, &propval);
  	if (ret < 0){
 		pr_err("%s %d: psy type failed, ret = %d\n", __func__, __LINE__, ret);
 		return 0;
 	} else {
 		pr_err("%s: capacity = %d\n", __func__, propval.intval);
		rm_soc = propval.intval;
 	}

	ret = power_supply_get_property(bms, POWER_SUPPLY_PROP_BQ_TRUE_FCC, &propval);
  	if (ret < 0){
 		pr_err("%s %d: psy type failed, ret = %d\n", __func__, __LINE__, ret);
 		return 0;
 	} else {
 		pr_err("%s: capacity = %d\n", __func__, propval.intval);
		fcc_soc = propval.intval;
 	}
	/*
	 * If CV is set to lower value by JEITA,
	 * Reset CV to normal value if temperture is in normal zone
	 */
	swchgalg->total_charging_time = 0;
	swchg_select_cv(info);
	info->polling_interval = CHARGING_FULL_INTERVAL;
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	raw_soc = (rm_soc * 10000) / fcc_soc;
	chr_err("[%s] chg_done : %d , raw_soc : %d",__func__, chg_done, raw_soc);
	if (!chg_done || (raw_soc < CDP_RECHARGE_RAW_SOC && info->chr_type == CHARGING_HOST)
					||  (raw_soc < SDP_RECHARGE_RAW_SOC && info->chr_type == STANDARD_HOST)) {
		/*
			if battery raw_soc lower 9910 , enter CHR_CC
		*/
		if (chg_done) {
			charger_dev_plug_out(info->chg1_dev);
			charger_dev_plug_in(info->chg1_dev);
		}
		swchgalg->state = CHR_CC;
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		mtk_pe20_set_to_check_chr_type(info, true);
		mtk_pe_set_to_check_chr_type(info, true);
		info->enable_dynamic_cv = true;
		get_monotonic_boottime(&swchgalg->charging_begin_time);
		chr_err("battery recharging!\n");
		info->polling_interval = CHARGING_INTERVAL;
		over_heat_flag = 0;
	} else {
		if ((!over_heat_flag) && swchgalg->state == CHR_BATFULL) {
			if (battery_get_bat_temperature() <= 48) {
				over_heat_flag = 1;
				psy_usb = power_supply_get_by_name("usb");
				if(!IS_ERR_OR_NULL(psy_usb)) {
	                    	     power_supply_changed(psy_usb);
				}
			}
		}
	}

	return 0;
}

static int mtk_switch_charging_current(struct charger_manager *info)
{
	swchg_select_charging_current_limit(info);
	return 0;
}

static int mtk_switch_charging_run(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	int ret = 0;

	chr_err("%s [%d %d], timer=%d\n", __func__, swchgalg->state,
		info->pd_type,
		swchgalg->total_charging_time);

	if (mtk_pdc_check_charger(info) == false &&
	    mtk_is_TA_support_pd_pps(info) == false) {
		mtk_pe20_check_charger(info);
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_check_charger(info);
	}

	do {
		switch (swchgalg->state) {
		case CHR_CC:
			ret = mtk_switch_chr_cc(info);
			break;

		case CHR_PE50:
			ret = mtk_switch_chr_pe50_run(info);
			break;

		case CHR_PE40:
			ret = mtk_switch_chr_pe40_run(info);
			break;

		case CHR_PDC:
			ret = mtk_switch_chr_pdc_run(info);
			break;

		case CHR_BATFULL:
			ret = mtk_switch_chr_full(info);
			break;

		case CHR_ERROR:
			ret = mtk_switch_chr_err(info);
			break;
		}
	} while (ret != 0);
	mtk_switch_check_charging_time(info);

	charger_dev_dump_registers(info->chg1_dev);
	return 0;
}

static int charger_dev_event(struct notifier_block *nb,
	unsigned long event, void *v)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, chg1_nb);
	struct chgdev_notify *data = v;

	chr_info("%s %ld", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		charger_manager_notifier(info, CHARGER_NOTIFY_EOC);
		pr_info("%s: end of charge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		charger_manager_notifier(info, CHARGER_NOTIFY_START_CHARGING);
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		chr_err("%s: safety timer timeout\n", __func__);

		/* If sw safety timer timeout, do not wake up charger thread */
		if (info->enable_sw_safety_timer)
			return NOTIFY_DONE;
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		chr_err("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

static int dvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, dvchg1_nb);

	chr_info("%s %ld", __func__, event);

	return mtk_pe50_notifier_call(info, MTK_PE50_NOTISRC_CHG, event, data);
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, dvchg2_nb);

	chr_info("%s %ld", __func__, event);

	return mtk_pe50_notifier_call(info, MTK_PE50_NOTISRC_CHG, event, data);
}

int mtk_switch_charging_init2(struct charger_manager *info)
{
	struct switch_charging_alg_data *swch_alg;

	swch_alg = devm_kzalloc(&info->pdev->dev,
				sizeof(*swch_alg), GFP_KERNEL);
	if (!swch_alg)
		return -ENOMEM;

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger [%s]\n",
			info->chg1_dev->props.alias_name);
	else
		chr_err("*** Error : can't find primary charger ***\n");

	info->dvchg1_dev = get_charger_by_name("primary_divider_chg");
	if (info->dvchg1_dev) {
		chr_err("Found primary divider charger [%s]\n",
			info->dvchg1_dev->props.alias_name);
		info->dvchg1_nb.notifier_call = dvchg1_dev_event;
		register_charger_device_notifier(info->dvchg1_dev,
						 &info->dvchg1_nb);
	} else
		chr_err("Can't find primary divider charger\n");
	info->dvchg2_dev = get_charger_by_name("secondary_divider_chg");
	if (info->dvchg2_dev) {
		chr_err("Found secondary divider charger [%s]\n",
			info->dvchg2_dev->props.alias_name);
		info->dvchg2_nb.notifier_call = dvchg2_dev_event;
		register_charger_device_notifier(info->dvchg2_dev,
						 &info->dvchg2_nb);
	} else
		chr_err("Can't find secondary divider charger\n");

	mutex_init(&swch_alg->ichg_aicr_access_mutex);

	info->algorithm_data = swch_alg;
	info->do_algorithm = mtk_switch_charging_run;
	info->plug_in = mtk_switch_charging_plug_in;
	info->plug_out = mtk_switch_charging_plug_out;
	info->do_charging = mtk_switch_charging_do_charging;
	info->do_event = charger_dev_event;
	info->change_current_setting = mtk_switch_charging_current;

	mtk_switch_chr_pe50_init(info);
	mtk_switch_chr_pe40_init(info);
	mtk_switch_chr_pdc_init(info);

	return 0;
}
