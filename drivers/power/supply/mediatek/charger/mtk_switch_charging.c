/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#include <linux/of.h>

#include <mt-plat/mtk_boot.h>
/* #include <musb_core.h> */ /* FIXME */
#include "mtk_charger_intf.h"
#include "mtk_switch_charging.h"

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

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

	if (mtk_pe50_get_is_enable(info)) {
		if (mtk_pe50_get_is_connect(info))
			mtk_pe50_stop_algo(info, true);
	}

	if (mtk_pe40_get_is_enable(info)) {
		if (mtk_pe40_get_is_connect(info))
			mtk_pe40_end(info, 3, true);
	}

	if (mtk_pdc_check_charger(info))
		mtk_pdc_reset(info);
}

/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 start */
static void swchg_select_cv(struct charger_manager *info);
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 end*/
static void swchg_select_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata = NULL;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret = 0;
/*C3T code for HQ-234455 by zhaohan at 2022/8/22 start*/
	struct power_supply *psy;
	union power_supply_propval val;
/*C3T code for HQ-234455 by zhaohan at 2022/8/22 end*/
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT
/* C3T code for HQ-252263 by tongjiacheng at 2022/10/08 start*/
	int input_current_val;
	int charging_current_val;
/* C3T code for HQ-252263 by tongjiacheng at 2022/10/08 end*/
// workaround for mt6768 
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
		if (pdata->input_current_limit_by_aicl != -1) {
			pdata->input_current_limit =
				pdata->input_current_limit_by_aicl;
		} else {
			pdata->input_current_limit =
				info->data.usb_unlimited_current;
		}
		pdata->charging_current_limit =
			info->data.ac_charger_current;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		goto done;
	}

// workaround for mt6768 
	if ((boot_mode == META_BOOT) ||
		(boot_mode == ADVMETA_BOOT)) {
		pdata->input_current_limit = 200000; /* 200mA */
		goto done;
	}

	if (info->atm_enabled == true && (info->chr_type == STANDARD_HOST ||
	    info->chr_type == CHARGING_HOST)) {
	/* C3T code for HQ-227758 by tongjiacheng at 2022/08/01 start*/
		pdata->input_current_limit = 500000; /* 100mA */
	/*C3T code for HQ-223425 by  tongjiacheng at 2022/8/2 start*/
		pdata->charging_current_limit = 500000;
	/*C3T code for HQ-223425 by  tongjiacheng at 2022/8/2 end*/
	/* C3T code for HQ-227758 by tongjiacheng at 2022/08/01 end*/
		goto done;
	}

	if (mtk_is_TA_support_pd_pps(info)) {
		pdata->input_current_limit =
			info->data.pe40_single_charger_input_current;
		pdata->charging_current_limit =
			info->data.pe40_single_charger_current;
	} else if (is_typec_adapter(info)) {
		if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
			== 3000) {
			pdata->input_current_limit = 3000000;
			pdata->charging_current_limit = 3000000;
		} else if (adapter_dev_get_property(info->pd_adapter,
			TYPEC_RP_LEVEL) == 1500) {
			pdata->input_current_limit = 1500000;
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
	} else if (mtk_pdc_check_charger(info)) {
		int vbus = 0, cur = 0, idx = 0;
		info->is_pdc_run = true;
		ret = mtk_pdc_get_setting(info, &vbus, &cur, &idx);
		if (ret != -1 && idx != -1) {
			pdata->input_current_limit = cur * 1000;
			pdata->charging_current_limit =
				info->data.pd_charger_current;
			mtk_pdc_setup(info, idx);
		} else {
			pdata->input_current_limit =
				info->data.usb_charger_current_configured;
			pdata->charging_current_limit =
				info->data.usb_charger_current_configured;
		}
		chr_err("[%s]vbus:%d input_cur:%d idx:%d current:%d\n",
			__func__, vbus, cur, idx,
			info->data.pd_charger_current);

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
/* C3T code for HQ-219166 by tongjiacheng at 2022/08/12 start */
		pdata->charging_current_limit =
				info->sw_jeita.cc;
/* C3T code for HQ-219166 by tongjiacheng at 2022/08/12 end*/
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
	}

	if (info->enable_sw_jeita) {
/* C3T code for HQ-219166 by tongjiacheng at 2022/08/12 start */
		if (pdata->charging_current_limit > info->sw_jeita.cc)
			pdata->charging_current_limit = info->sw_jeita.cc;
	}
	pr_info("%s: charging current = %d, sm = [%d], jeita current = %d\n",
					__func__, pdata->charging_current_limit,
					info->sw_jeita.sm, info->sw_jeita.cc);
/* C3T code for HQ-219166 by tongjiacheng at 2022/08/12 end*/

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
		    pdata->charging_current_limit)
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
	}

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
	}

	if (mtk_pe40_get_is_connect(info)) {
		if (info->pe4.pe4_input_current_limit != -1 &&
		    info->pe4.pe4_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
				info->pe4.pe4_input_current_limit;

		info->pe4.input_current_limit = pdata->input_current_limit;

		if (info->pe4.pe4_input_current_limit_setting != -1 &&
		    info->pe4.pe4_input_current_limit_setting <
		    pdata->input_current_limit)
			pdata->input_current_limit =
				info->pe4.pe4_input_current_limit_setting;
	}

	if (pdata->input_current_limit_by_aicl != -1 &&
	    !mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info)) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
done:
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = 0;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = 0;
	
/*C3T code for HQ-234455 by zhaohan at 2022/8/22 start*/
	psy = power_supply_get_by_name("charger");
	if(IS_ERR(psy)){
		pr_err("%s : power_supply_get_by_name error!\n", __func__);
	}
/* C3T code for HQ-252263 by tongjiacheng at 2022/10/08 start*/
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_MTBF, &val);
	if(ret){
		pr_err("%s : power_supply_get_property error!\n", __func__);
	}
	charging_current_val = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &val);
	if(ret){
		pr_err("%s : power_supply_get_property error!\n", __func__);
	}
	input_current_val = val.intval;
/*C3T code for HQ-223437 by zhaohan at 2022/10/17 start*/
	if (input_current_val > 0 ) {
		if (pdata->thermal_input_current_limit != -1)
		pdata->input_current_limit = pdata->thermal_input_current_limit > (input_current_val * 1000)
												 ? (input_current_val * 1000) : pdata->thermal_input_current_limit;
		else
		pdata->input_current_limit = input_current_val * 1000;
	}
/*C3T code for HQ-223437 by zhaohan at 2022/10/17 end*/		

	if(charging_current_val > 0) {
		pdata->charging_current_limit = charging_current_val* 1000;
		pdata->input_current_limit = charging_current_val* 1000;
	}
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 start */
	if (info->chg_error) {
		pdata->charging_current_limit = 0;
		swchg_select_cv(info);
	}
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 end*/
/* C3T code for HQ-252263 by tongjiacheng at 2022/10/08 end*/
/*C3T code for HQ-234455 by zhaohan at 2022/8/22 end*/
	chr_err("force:%d thermal:%d,%d pe4:%d,%d,%d setting:%d %d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d\n",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit_setting),
		_uA_to_mA(info->pe4.input_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled);

	charger_dev_set_input_current(info->chg1_dev,
					pdata->input_current_limit);
	charger_dev_set_charging_current(info->chg1_dev,
					pdata->charging_current_limit);

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

/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 start */
static u32 swchg_get_cycle_count_level(struct charger_manager *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	u32 ffc_constant_voltage = 0;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		chr_err("%s: failed to get battery psy\n", __func__);
		return PTR_ERR(psy);
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	if (ret) {
		chr_err("%s: failed to get prop: %d\n", __func__, POWER_SUPPLY_PROP_CYCLE_COUNT);
		return ret;
	}

	chr_err("%s: prop cycle count = %d\n", __func__, val.intval);

	if (val.intval >=1 && val.intval < info->data.cycle_count_level1)
		ffc_constant_voltage = info->data.ffc_cv_1;
	else if (val.intval >= info->data.cycle_count_level1 &&
		val.intval < info->data.cycle_count_level2)
		ffc_constant_voltage = info->data.ffc_cv_2;
	else if (val.intval >= info->data.cycle_count_level2 &&
		val.intval < info->data.cycle_count_level3)
		ffc_constant_voltage = info->data.ffc_cv_3;
	else if (val.intval >= info->data.cycle_count_level3)
		ffc_constant_voltage = info->data.ffc_cv_4;

	return ffc_constant_voltage;
}
/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30  end*/

static void swchg_select_cv(struct charger_manager *info)
{
	u32 constant_voltage;
/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 start */
	u32 ffc_constant_voltage;
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 start */
	u32 vbat = battery_get_bat_voltage() * 1000;

	ffc_constant_voltage = swchg_get_cycle_count_level(info);

	if (info->battery_temp >= info->data.temp_t3_thres && vbat > info->sw_jeita.cv) {
		charger_dev_set_constant_voltage(info->chg1_dev, ffc_constant_voltage);
		info->chg_error = true;
		return;
	}
	else
		info->chg_error = false;
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 end*/

	if (info->enable_sw_fcc) {
		if (ffc_constant_voltage != 0) {
			if (info->enable_sw_jeita && info->sw_jeita.cv != 0)
				ffc_constant_voltage = ffc_constant_voltage > info->sw_jeita.cv ?
							info->sw_jeita.cv : ffc_constant_voltage;

			chr_err("%s: ffc constant voltage = %d\n", __func__, ffc_constant_voltage);
			charger_dev_set_constant_voltage(info->chg1_dev, ffc_constant_voltage);
			return;
		}
	}
/* C3T code for HQ-223445 by tongjiacheng at 2022/08/30 end*/

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			charger_dev_set_constant_voltage(info->chg1_dev,
							info->sw_jeita.cv);
			return;
		}

	/* dynamic cv*/
	constant_voltage = info->data.battery_cv;
	mtk_get_dynamic_cv(info, &constant_voltage);

	charger_dev_set_constant_voltage(info->chg1_dev, constant_voltage);
}

static void swchg_turn_on_charging(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	bool charging_enable = true;
	
	struct device *dev = NULL;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
	int boot_mode = 11;//UNKNOWN_BOOT
// workaround for mt6768 
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

	if (swchgalg->state == CHR_ERROR) {
		charging_enable = false;
		chr_err("[charger]Charger Error, turn OFF charging !\n");
	} 
// workaround for mt6768 
	else if ((boot_mode == META_BOOT) ||
			(boot_mode == ADVMETA_BOOT)) {
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
		if (info->chg1_data.input_current_limit == 0
		    || info->chg1_data.charging_current_limit == 0) {
			charging_enable = false;
			chr_err("[charger]charging current is set 0mA, turn off charging !\n");
		} else {
			swchg_select_cv(info);
		}
	}

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
	mtk_pe40_plugout_reset(info);
	mtk_pe50_plugout_reset(info);

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
		mtk_pe40_set_is_enable(info, en);
		mtk_pe50_set_is_enable(info, en);
	} else {
		/* disable charging might change state, so call it first */
		_disable_all_charging(info);
		swchgalg->disable_charging = true;
		swchgalg->state = CHR_ERROR;
		charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
	}

	return 0;
}

static int mtk_switch_chr_pe40_init(struct charger_manager *info)
{
	swchg_turn_on_charging(info);
	return mtk_pe40_init_state(info);
}

static int mtk_switch_chr_pe40_cc(struct charger_manager *info)
{
	swchg_turn_on_charging(info);
	return mtk_pe40_cc_state(info);
}

static int mtk_switch_chr_pe50_ready(struct charger_manager *info)
{
	int ret;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	ret = mtk_pe50_start(info);
	if (ret < 0) {
		info->pe5.online = false;
		swchgalg->state = CHR_CC;
	} else
		swchgalg->state = CHR_PE50_RUNNING;
	return 0;
}

static int mtk_switch_chr_pe50_running(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct charger_data *dvchg_data = &info->dvchg1_data;

	if (!mtk_pe50_is_running(info))
		goto stop;
	if (!info->enable_hv_charging) {
		mtk_pe50_stop_algo(info, true);
		goto stop;
	}

	mtk_pe50_thermal_throttling(info,
				    dvchg_data->thermal_input_current_limit);
	if (info->enable_sw_jeita)
		mtk_pe50_set_jeita_vbat_cv(info, info->sw_jeita.cv);
	return 0;

stop:
	chr_info("%s PE5 stops\n", __func__);
	info->pe5.online = false;
	swchgalg->state = CHR_CC;
	/* Let charging algorithm run CHR_CC immediately */
	return -EINVAL;
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

/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 start */
static int judge_recharger_flag(struct charger_manager *info)
{
	bool chg_done = false;
	int health = 0;
	int rawsoc = 0;
/*C3T code for HQ-242556 by tongjiacheng at 2022/09/20 start*/
	int uisoc;
/*C3T code for HQ-242556 by tongjiacheng at 2022/09/20 end*/
	int recharger_soc;
/*C3T code for HQ-244409 by tongjiacheng at 2022/09/22 start*/
	int bat_temp;
/*C3T code for HQ-244409 by tongjiacheng at 2022/09/22 end*/
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 start */
	int vendor;
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 end*/
	int ret;

	struct power_supply *bat_psy;
	union power_supply_propval val;

	bat_psy = power_supply_get_by_name("battery");
	if (!bat_psy) {
		chr_err("failed to find battery psy\n");
		return PTR_ERR(bat_psy);
	}

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_HEALTH, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_HEALTH);
		return -EINVAL;
	}

	health = val.intval;

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_BATT_SOC, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_BATT_SOC);
		return -EINVAL;
	}

	rawsoc = val.intval;

/*C3T code for HQ-242556 by tongjiacheng at 2022/09/20 start*/
	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_CAPACITY);
		return -EINVAL;
	}

	uisoc = val.intval;
/*C3T code for HQ-242556 by tongjiacheng at 2022/09/20 end*/

/*C3T code for HQ-244409 by tongjiacheng at 2022/09/22 start*/
	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_TEMP);
		return -EINVAL;
	}

	bat_temp = val.intval;
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 start */
	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_BATTERY_ID, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_BATTERY_ID);
		return -EINVAL;
	}
	vendor = val.intval;
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 end*/
/*C3T code for HQ-244409 by tongjiacheng at 2022/09/22 end*/
/*C3T code for HQHW-3590 by wangtingting at 2022/10/18 start*/
	if (health == POWER_SUPPLY_HEALTH_OVERHEAT) {
		pr_err("over heat, battery recharging!\n");
		chg_done = true;
	}
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 start */
	else {
		recharger_soc = info->data.recharger_soc_limit_1;
          /* C3T code for HQHW-3590 by wangtingting at 2022/10/18 start*/
		if (vendor ==0 || vendor ==1)
			recharger_soc = info->data.recharger_soc_limit_1;
		else
			recharger_soc = info->data.recharger_soc_limit_2;
/*C3T code for HQ-242556 by tongjiacheng at 2022/09/20 start*/
/*C3T code for HQ-244409 by tongjiacheng at 2022/09/22 start*/
/* C3T code for HQ-253634 by tongjiacheng at 2022/10/09 start*/
		if (rawsoc <= recharger_soc && uisoc >= 100 && bat_temp >= 250
			&& mt_get_charger_type() == STANDARD_HOST) {
			pr_err("not over heat, battery recharging!\n");
			chg_done = true;
		}
         /* C3T code for HQHW-3590 by wangtingting at 2022/10/18 end*/
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 end*/
/*C3T code for HQHW-3590 by wangtingting at 2022/10/18 end*/
/* C3T code for HQ-253634 by tongjiacheng at 2022/10/09 end*/
		pr_err("%s: battery:health = %d,  soc = %d, recharger_soc_limit = %d\n",
					__func__, health, rawsoc, recharger_soc);
/*C3T code for HQ-244409 by tongjiacheng at 2022/09/22 end*/
/*C3T code for HQ-242556 by tongjiacheng at 2022/09/20 end*/
	}

	return chg_done;
}
/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 end*/

/* C3T code for HQ-253634 by tongjiacheng at 2022/10/09 start*/
static bool judge_charger_full_flag(struct charger_manager *info)
{
	int ret;
	int bat_temp;
	int rawsoc;
	int uisoc;
	int vendor;
	int recharger_soc;
	int bat_status;
	bool chg_done;
	bool chg_full = false;

	struct power_supply *bat_psy;
	union power_supply_propval val;

	bat_psy = power_supply_get_by_name("battery");
	if (!bat_psy) {
		chr_err("failed to find battery psy\n");
		return PTR_ERR(bat_psy);
	}

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_TEMP);
		return -EINVAL;
	}

	bat_temp = val.intval;

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_BATT_SOC, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_BATT_SOC);
		return -EINVAL;
	}

	rawsoc = val.intval;

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_CAPACITY);
		return -EINVAL;
	}

	uisoc = val.intval;

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_BATTERY_ID, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_BATTERY_ID);
		return -EINVAL;
	}
	vendor = val.intval;

	if (vendor ==0 || vendor ==1)
		recharger_soc = info->data.recharger_soc_limit_1;
	else
		recharger_soc = info->data.recharger_soc_limit_2;

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret) {
		chr_err("failed to get battery prop: %d\n", POWER_SUPPLY_PROP_STATUS);
		return -EINVAL;
	}
	bat_status = val.intval;

	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 start*/
	if (bat_temp >= 10 * 10 ) {
		if (rawsoc > recharger_soc && bat_status != POWER_SUPPLY_STATUS_DISCHARGING&& bat_temp <= 45 * 10
			&& chg_done && uisoc >= 100)
			chg_full = true;
	}
/* C3T code for HQ-256470 by tongjiacheng at 2022/10/31 end*/
	else if (chg_done && bat_status != POWER_SUPPLY_STATUS_DISCHARGING
				&& uisoc >= 100)
		chg_full = true;

	pr_info("%s: rawsoc = %d, bat_status = %d, chg_done = %d, uisoc = %d, chg_full = %d\n",
				__func__, rawsoc, bat_status, chg_done, uisoc, chg_full);

	return chg_full;
}
/* C3T code for HQ-253634 by tongjiacheng at 2022/10/09 end*/

static int mtk_switch_chr_cc(struct charger_manager *info)
{
	bool chg_done = false;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct timespec time_now, charging_time;

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

	if (mtk_pe50_is_ready(info)) {
		chr_err("enter PE5.0\n");
		swchgalg->state = CHR_PE50_READY;
		info->pe5.online = true;
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
		return 1;
	}

	if (mtk_pe40_is_ready(info)) {
		chr_err("enter PE4.0!\n");
		swchgalg->state = CHR_PE40_INIT;
		info->pe4.is_connect = true;
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
		return 1;
	}

	swchg_turn_on_charging(info);

	/* C3T code for HQ-253634 by tongjiacheng at 2022/10/09 start*/
	chg_done = judge_charger_full_flag(info);
	/* C3T code for HQ-253634 by tongjiacheng at 2022/10/09 end*/
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

int mtk_switch_chr_err(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	if (info->enable_sw_jeita) {
		if ((info->sw_jeita.sm == TEMP_BELOW_T0) ||
			(info->sw_jeita.sm == TEMP_ABOVE_T4))
			info->sw_jeita.error_recovery_flag = false;

		if ((info->sw_jeita.error_recovery_flag == false) &&
			(info->sw_jeita.sm != TEMP_BELOW_T0) &&
			(info->sw_jeita.sm != TEMP_ABOVE_T4)) {
			info->sw_jeita.error_recovery_flag = true;
			swchgalg->state = CHR_CC;
			get_monotonic_boottime(&swchgalg->charging_begin_time);
		}
	}

	swchgalg->total_charging_time = 0;

	_disable_all_charging(info);
	return 0;
}

int mtk_switch_chr_full(struct charger_manager *info)
{
	/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 start */
	//bool chg_done = false;
	int recharger_flag;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	swchgalg->total_charging_time = 0;

	recharger_flag = judge_recharger_flag(info);
	/* turn off LED */
	/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 end*/

	/*
	 * If CV is set to lower value by JEITA,
	 * Reset CV to normal value if temperture is in normal zone
	 */
	swchg_select_cv(info);
	info->polling_interval = CHARGING_FULL_INTERVAL;
	/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 start */
	//charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	if (recharger_flag) {
	/* C3T code for HQ-HQ-218837 by tongjiacheng at 2022/08/26 end*/
		swchgalg->state = CHR_CC;
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		mtk_pe20_set_to_check_chr_type(info, true);
		mtk_pe_set_to_check_chr_type(info, true);
		mtk_pe40_set_is_enable(info, true);
		mtk_pe50_set_is_enable(info, true);
		info->enable_dynamic_cv = true;
		get_monotonic_boottime(&swchgalg->charging_begin_time);
		chr_err("battery recharging!\n");
		info->polling_interval = CHARGING_INTERVAL;
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
	    mtk_is_TA_support_pd_pps(info) == false &&
	    !info->pe5.online) {
		mtk_pe20_check_charger(info);
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_check_charger(info);
	}

	if (mtk_pe40_get_is_connect(info)) {
		if (mtk_pe50_is_ready(info))
			mtk_pe40_end(info, 4, true);
	}

	do {
		switch (swchgalg->state) {
			chr_err("%s_2 [%d] %d\n", __func__, swchgalg->state,
				info->pd_type);
		case CHR_CC:
			ret = mtk_switch_chr_cc(info);
			break;

		case CHR_PE40_INIT:
			ret = mtk_switch_chr_pe40_init(info);
			break;

		case CHR_PE40_CC:
			ret = mtk_switch_chr_pe40_cc(info);
			break;

		case CHR_PE50_READY:
			ret = mtk_switch_chr_pe50_ready(info);
			break;

		case CHR_PE50_RUNNING:
			ret = mtk_switch_chr_pe50_running(info);
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

int charger_dev_event(struct notifier_block *nb, unsigned long event, void *v)
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
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	chr_info("%s %ld", __func__, event);

	if (swchgalg->state == CHR_PE50_READY ||
	    swchgalg->state == CHR_PE50_RUNNING)
		return mtk_pe50_notifier_call(info, MTK_PE50_NOTISRC_CHG, event,
					      data);
	return 0;
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, dvchg2_nb);
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	chr_info("%s %ld", __func__, event);

	if (swchgalg->state == CHR_PE50_READY ||
	    swchgalg->state == CHR_PE50_RUNNING)
		return mtk_pe50_notifier_call(info, MTK_PE50_NOTISRC_CHG, event,
					      data);
	return 0;
}

int mtk_switch_charging_init(struct charger_manager *info)
{
	int ret = 0;
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
		chr_err("*** Error : can't find primary divider charger ***\n");
	info->dvchg2_dev = get_charger_by_name("secondary_divider_chg");
	if (info->dvchg2_dev) {
		chr_err("Found secondary divider charger [%s]\n",
			info->dvchg2_dev->props.alias_name);
		info->dvchg2_nb.notifier_call = dvchg2_dev_event;
		register_charger_device_notifier(info->dvchg2_dev,
						 &info->dvchg2_nb);
	} else
		chr_err("*** Error : can't find secondary divider charger ***\n");

	mutex_init(&swch_alg->ichg_aicr_access_mutex);

	info->algorithm_data = swch_alg;
	info->do_algorithm = mtk_switch_charging_run;
	info->plug_in = mtk_switch_charging_plug_in;
	info->plug_out = mtk_switch_charging_plug_out;
	info->do_charging = mtk_switch_charging_do_charging;
	info->do_event = charger_dev_event;
	info->change_current_setting = mtk_switch_charging_current;

	return ret;
}
