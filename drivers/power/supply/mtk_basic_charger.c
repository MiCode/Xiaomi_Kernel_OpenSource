// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_basic_charger.c
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

#include "mtk_charger.h"
#include "mtk_battery.h"

#define recharger_val  97
#define wait_time      5          //5 seconds each time

int recharger_flag = EVENT_RECHG_INIT;

/* add for mtbf_current*/
int mtbf_current = -1;
EXPORT_SYMBOL(mtbf_current);

int real_type = 0;
EXPORT_SYMBOL(real_type);

int gmsoc = -1;
EXPORT_SYMBOL(gmsoc);
static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

/* add cycle count*/
static u32 get_charge_cycle_count_level(struct mtk_charger *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	u32 ffc_constant_voltage = 0;
	psy = power_supply_get_by_name("battery");
	if (psy == NULL || IS_ERR(psy)) {
		pr_err("%s: failed to get battery psy\n", __func__);
		return PTR_ERR(psy);
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	if (ret) {
		pr_err("%s: failed to read prop cycle count\n", __func__);
		return ret;
	}
	pr_err("%s: prop cycle count = %d\n", __func__, val.intval);
	if ((val.intval >= info->chg_cycle_count_level1) &&
		(val.intval < info->chg_cycle_count_level2))
			ffc_constant_voltage = info->ffc_cv_1;
	else if ((val.intval >= info->chg_cycle_count_level2) &&
		(val.intval < info->chg_cycle_count_level3))
			ffc_constant_voltage = info->ffc_cv_2;
	else if ((val.intval >= info->chg_cycle_count_level3) &&
		(val.intval < info->chg_cycle_count_level4))
			ffc_constant_voltage = info->ffc_cv_3;
	else if (val.intval >= info->chg_cycle_count_level4)
			ffc_constant_voltage = info->ffc_cv_4;
	return ffc_constant_voltage;
}

static void select_cv(struct mtk_charger *info)
{
	u32 constant_voltage;
	u32 ffc_constant_voltage = get_charge_cycle_count_level(info);
	if (info->enable_sw_ffc) {
		if (ffc_constant_voltage != 0) {
			if ((info->enable_sw_jeita) && ( info->sw_jeita.cv != 0))
				ffc_constant_voltage = (ffc_constant_voltage < info->sw_jeita.cv) ? 
							ffc_constant_voltage : info->sw_jeita.cv;
			pr_err("%s: constant voltage = %d\n", __func__, ffc_constant_voltage);
			info->setting.cv = ffc_constant_voltage;
			return;
		}
	}

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			pr_err("%s: sw_jeita.cv = %d\n", __func__, info->sw_jeita.cv);
			info->setting.cv = info->sw_jeita.cv;
			return;
		}

	constant_voltage = info->data.battery_cv;
	info->setting.cv = constant_voltage;
}

static bool is_typec_adapter(struct mtk_charger *info)
{
	int rp;

	rp = adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL);
	if (info->pd_type == MTK_PD_CONNECT_TYPEC_ONLY_SNK &&
			rp != 500 &&
			info->chr_type != POWER_SUPPLY_TYPE_USB &&
			info->chr_type != POWER_SUPPLY_TYPE_USB_CDP)
		return true;

	return false;
}

static bool support_fast_charging(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int i = 0, state = 0;
	bool ret = false;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		if (info->enable_fast_charging_indicator &&
		    ((alg->alg_id & info->fast_charging_indicator) == 0))
			continue;

		chg_alg_set_current_limit(alg, &info->setting);
		state = chg_alg_is_algo_ready(alg);
		chr_debug("%s %s ret:%s\n", __func__, dev_name(&alg->dev),
			chg_alg_state_to_str(state));

		if (state == ALG_READY || state == ALG_RUNNING) {
			ret = true;
			break;
		}
	}
	return ret;
}

#ifndef MIN
#define MIN(x, y)   ((x) <= (y))? (x): (y)
#endif  // MIN
int jeita_current_limit(struct mtk_charger *info)
{
	if(IS_ERR_OR_NULL(info)) {
		return false;
	}

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
			&& info->chr_type == POWER_SUPPLY_TYPE_USB) {
			chr_debug("USBIF & STAND_HOST skip current check\n");
		} else {
			if (info->sw_jeita.sm == TEMP_T0_TO_T1) {
				info->jeita_input_current_limit = info->data.ac_charger_input_current;
				info->jeita_charging_current_limit = info->data.jeita_current_t0_to_t1;
			} else if (info->sw_jeita.sm == TEMP_T1_TO_T2) {
				info->jeita_input_current_limit = info->data.ac_charger_input_current;
				info->jeita_charging_current_limit = info->data.jeita_current_t1_to_t2;
			} else if (info->sw_jeita.sm == TEMP_T2_TO_T3) {
				info->jeita_input_current_limit = info->data.ac_charger_input_current;
				info->jeita_charging_current_limit = info->data.jeita_current_t2_to_t3;
			} else if (info->sw_jeita.sm == TEMP_T3_TO_T4) {
				info->jeita_input_current_limit = info->data.ac_charger_input_current;
				info->jeita_charging_current_limit = info->data.jeita_current_t3_to_t4;
			} else if (info->sw_jeita.sm == TEMP_T4_TO_T5) {
				info->jeita_input_current_limit = info->data.ac_charger_input_current;
				info->jeita_charging_current_limit = info->data.jeita_current_t4_to_t5;
			} else {
				info->jeita_input_current_limit = info->data.ac_charger_input_current;
				info->jeita_charging_current_limit = 0;
			}
		}
	}

	chr_info("%s: jeita_input_current_limit: %d, jeita_charging_current_limit: %d\n",
				__func__,
				info->jeita_input_current_limit,
				info->jeita_charging_current_limit);

	return true;
}

extern int charger_manager_pd_is_online(void);
static bool select_charging_current_limit(struct mtk_charger *info,
	struct chg_limit_setting *setting)
{
	struct charger_data *pdata, *pdata2, *pdata_dvchg, *pdata_dvchg2;
	bool is_basic = false;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;
	bool is_powerpath_en = true;
	bool is_chg_suspend = true;
	bool is_online_pd = false;
	struct tcpm_power_cap cap;
	struct tcpm_power_cap_val capv;

	if(!info->tcpc) {
		info->tcpc = tcpc_dev_get_by_name("type_c_port0");
	}
	
	is_online_pd = charger_manager_pd_is_online();
	select_cv(info);

	pdata = &info->chg_data[CHG1_SETTING];
	pdata2 = &info->chg_data[CHG2_SETTING];
	pdata_dvchg = &info->chg_data[DVCHG1_SETTING];
	pdata_dvchg2 = &info->chg_data[DVCHG2_SETTING];
	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
		is_basic = true;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		is_basic = true;
		goto done;
	}

	if (((info->bootmode == 1) ||
	    (info->bootmode == 5)) && info->enable_meta_current_limit != 0) {
		pdata->input_current_limit = 200000; // 200mA
		is_basic = true;
		goto done;
	}

	if (info->atm_enabled == true
		&& (info->chr_type == POWER_SUPPLY_TYPE_USB ||
		info->chr_type == POWER_SUPPLY_TYPE_USB_CDP)
		) {
		if(info->usb_type == POWER_SUPPLY_USB_TYPE_DCP){
			pdata->input_current_limit = 1000000;
			pdata->charging_current_limit = 1000000;
		    is_basic = true;
			goto done;
		} else {	
			pdata->input_current_limit = 500000; /* 500mA */
			is_basic = true;
			goto done;
		}
	}

	if (is_online_pd) {
		if(info->tcpc) {
			ret = tcpm_inquire_pd_source_cap(info->tcpc, &cap);
			tcpm_extract_power_cap_val(cap.pdos[0],	&capv);
		}
		if((cap.cnt == 1) && (cap.pdos[0] & PDO_FIXED_DUAL_ROLE)){
			pdata->input_current_limit = 1500000;//capv.ma*1000;
			pdata->charging_current_limit = 3500000;
		} else {
			pdata->input_current_limit = 2000000;
			pdata->charging_current_limit = 3500000;
		}
		is_basic = false;
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
		real_type = 1;
		pdata->input_current_limit =
				info->data.usb_charger_current;
		/* it can be larger */
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		real_type = 2;
		pdata->input_current_limit =
			info->data.charging_host_charger_current;
		pdata->charging_current_limit =
			info->data.charging_host_charger_current;
		is_basic = true;

	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		real_type = 3;
		pdata->input_current_limit =
			info->data.ac_charger_input_current;
		pdata->charging_current_limit =
			info->data.ac_charger_current;
		if (info->config == DUAL_CHARGERS_IN_SERIES) {
			pdata2->input_current_limit =
				pdata->input_current_limit;
			pdata2->charging_current_limit = 2000000;
		}

	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
		/* NONSTANDARD_CHARGER */
		real_type = 5;
		pdata->input_current_limit = 1000000;
		pdata->charging_current_limit = 1000000;
		is_basic = true;
	} else {
		/*chr_type && usb_type cannot match above, set 500mA*/
		real_type = 5;
		pdata->input_current_limit =
				info->data.usb_charger_current;
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	}

	pr_info("real_type2:%d  info->chr_type:%d info->usb_type:%d capv.ma:%d\n", real_type, info->chr_type, info->usb_type,capv.ma);

	if (support_fast_charging(info))
		is_basic = false;
	else {
		is_basic = true;
		/* AICL */
		if (!info->disable_aicl)
			charger_dev_run_aicl(info->chg1_dev,
				&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}

		if (is_typec_adapter(info)) {
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
		}
	}
	chr_info("%s: pdata->input_current_limit: %d, pdata->charging_current_limit: %d\n",
					__func__,
					pdata->input_current_limit,
					pdata->charging_current_limit);

	ret = jeita_current_limit(info);
	if (!ret) {
		chr_err("%s: jeita_current_limit FAIL\n", __func__);
	}

	pdata->input_current_limit = MIN(pdata->input_current_limit, info->jeita_input_current_limit);
	pdata->charging_current_limit = MIN(pdata->charging_current_limit, info->jeita_charging_current_limit);

	sc_select_charging_current(info, pdata);

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <=
			pdata->charging_current_limit) {
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
					pdata->thermal_charging_current_limit;
		}
		pdata->thermal_throttle_record = true;
	} else
		info->setting.charging_current_limit1 = info->sc.sc_ibat;

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <=
			pdata->input_current_limit) {
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
			info->setting.input_current_limit1 =
					pdata->input_current_limit;
		}
		pdata->thermal_throttle_record = true;
	} else
		info->setting.input_current_limit1 = -1;

	if (pdata2->thermal_charging_current_limit != -1) {
		if (pdata2->thermal_charging_current_limit <=
			pdata2->charging_current_limit) {
			pdata2->charging_current_limit =
					pdata2->thermal_charging_current_limit;
			info->setting.charging_current_limit2 =
					pdata2->charging_current_limit;
		}
	} else
		info->setting.charging_current_limit2 = info->sc.sc_ibat;

	if (pdata2->thermal_input_current_limit != -1) {
		if (pdata2->thermal_input_current_limit <=
			pdata2->input_current_limit) {
			pdata2->input_current_limit =
					pdata2->thermal_input_current_limit;
			info->setting.input_current_limit2 =
					pdata2->input_current_limit;
		}
	} else
		info->setting.input_current_limit2 = -1;

	if (is_basic == true && pdata->input_current_limit_by_aicl != -1
		&& !info->charger_unlimited
		&& !info->disable_aicl) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
	info->setting.input_current_limit_dvchg1 =
		pdata_dvchg->thermal_input_current_limit;

		/* add mtbf */
	if ((info->chr_type == POWER_SUPPLY_TYPE_USB_CDP) || (info->chr_type == POWER_SUPPLY_TYPE_USB)) {
		if (mtbf_current >= 1500) {
			pdata->charging_current_limit = 1500000;
			pdata->input_current_limit = mtbf_current * 1000;
		}

		ret = charger_dev_is_powerpath_enabled(info->chg1_dev, &is_powerpath_en);
		if (ret < 0)
			{
				chr_info("%s: mtbf get is power path enabled failed\n", __func__);
			}

			is_chg_suspend = input_suspend_get_flag();

			chr_info("%s: mtbf before is_powerpath_en = %d, is_chg_suspend = %d\n", __func__, is_powerpath_en, is_chg_suspend);

			if(0 == is_chg_suspend)
			{
				charger_dev_enable_powerpath(info->chg1_dev, true); //charging status: force open power path
			}
			chr_info("%s: mtbf after is_powerpath_en = %d, is_chg_suspend = %d\n", __func__, is_powerpath_en, is_chg_suspend);
	}

done:

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -EOPNOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -EOPNOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}
	/* For TC_018, pleasae don't modify the format */
	chr_err("m:%d chg1:%d,%d,%d,%d chg2:%d,%d,%d,%d dvchg1:%d sc:%d %d %d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
		info->config,
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(pdata2->thermal_input_current_limit),
		_uA_to_mA(pdata2->thermal_charging_current_limit),
		_uA_to_mA(pdata2->input_current_limit),
		_uA_to_mA(pdata2->charging_current_limit),
		_uA_to_mA(pdata_dvchg->thermal_input_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->pd_type,
		info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		info->bootmode, is_basic);

	return is_basic;
}

int get_fg_batt_info(struct mtk_charger *info){
	return gmsoc;
}

static int wait_fg_update_soc (bool flag, struct mtk_charger *info){
	int loop;
	int gauge_soc = 0;
	if(flag == true){
		for (loop = 0;loop < wait_time;loop++){
			gauge_soc = get_fg_batt_info(info);
			if (gauge_soc != 100){
				msleep(5000);
			}else return true;
		}
		return false;
	} else return false;
}

static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
	bool is_basic = true;
	bool chg_done = false,block_flag = false;
	int i;
	int ret;
	int val = 0;
	int real_soc = 0;
	int current_bat_temp = 0;

	pdata = &info->chg_data[CHG1_SETTING];
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	is_basic = select_charging_current_limit(info, &info->setting);
	current_bat_temp = get_battery_temperature(info);

	real_soc = get_fg_batt_info(info);
	chr_err("do_algorithm info->is_chg_done:%d,chg_done:%d,recharger_flag:%d,real_soc:%d\n",
				info->is_chg_done,chg_done,recharger_flag,real_soc);

	if (current_bat_temp < info->data.temp_t4_thres) {
	if (info->is_chg_done != chg_done) {
		if (chg_done) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
				recharger_flag = EVENT_RECHG_FULL;
				block_flag = true;
				chr_err("%s %d  battery full\n", __func__,__LINE__);
			} else {
				if (recharger_flag == EVENT_RECHG_FULL && real_soc <= recharger_val) {
					charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
					recharger_flag = EVENT_RECHG_START;
					chr_err("%s %d  battery recharge\n", __func__,__LINE__);
				} else if (recharger_flag == EVENT_RECHG_BLOCK && real_soc <= recharger_val) {
					charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
					recharger_flag = EVENT_RECHG_START;
					chr_err("%s %d real_soc:%d  <= recharger_val:%d ,battery recharge is success\n",
						__func__,__LINE__,real_soc,recharger_val);
				} else if(recharger_flag == EVENT_RECHG_FULL && real_soc > recharger_val) {
					recharger_flag = EVENT_RECHG_BLOCK;
					chr_err("%s %dreal_soc:%d  > recharger_val:%d ,battery recharge is block\n",
							__func__,__LINE__,real_soc,recharger_val);
				} else if (recharger_flag == EVENT_RECHG_BLOCK && real_soc > recharger_val) {
					chr_err("%s %d real_soc:%d  > recharger_val:%d ,battery recharge is block\n",
							__func__,__LINE__,real_soc,recharger_val);
				}
			}
		}
		chr_err("%s %d :block_flag = %d\n", __func__,__LINE__,block_flag);

		switch (recharger_flag) {
		case EVENT_RECHG_FULL:
			if (real_soc <= recharger_val && block_flag == false) {
				charger_dev_enable(info->chg1_dev, false);
				charger_dev_enable(info->chg1_dev, true);
				charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
				recharger_flag = EVENT_RECHG_START;
				chg_done = false;
				chr_err("%s %d battery recharge\n", __func__,__LINE__);
				break;
			} else break;
		case EVENT_RECHG_START:
			break;
		case EVENT_RECHG_BLOCK:
			chg_done = info->is_chg_done;
			break;
		default:
			break;
		}
	} else {
		if (info->is_chg_done != chg_done) {
			if (chg_done) {
				charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
				recharger_flag = EVENT_RECHG_FULL;
				block_flag = true;
				chr_err("%s battery full\n", __func__);
			} else {
				charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
				recharger_flag = EVENT_RECHG_START;
				chr_err("%s battery recharge\n", __func__);
			}
		}
	}

	chr_err("%s is_basic:%d\n", __func__, is_basic);
	if (is_basic != true) {
		is_basic = true;
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;

			if (info->enable_fast_charging_indicator &&
			    ((alg->alg_id & info->fast_charging_indicator) == 0))
				continue;

			if (!info->enable_hv_charging ||
			    pdata->charging_current_limit == 0 ||
			    pdata->input_current_limit == 0) {
				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000)
					chg_alg_stop_algo(alg);
				chr_err("%s: alg:%s alg_vbus:%d\n", __func__,
					dev_name(&alg->dev), val);
				continue;
			}

			if (chg_done != info->is_chg_done) {
				if (chg_done) {
					notify.evt = EVT_FULL;
					notify.value = 0;
				} else {
					notify.evt = EVT_RECHARGE;
					notify.value = 0;
				}
				chg_alg_notifier_call(alg, &notify);
				chr_err("%s notify:%d\n", __func__, notify.evt);
			}

			chg_alg_set_current_limit(alg, &info->setting);
			ret = chg_alg_is_algo_ready(alg);

			chr_err("%s %s ret:%s\n", __func__,
				dev_name(&alg->dev),
				chg_alg_state_to_str(ret));

			if (ret == ALG_INIT_FAIL || ret == ALG_TA_NOT_SUPPORT) {
				/* try next algorithm */
				continue;
			} else if (ret == ALG_TA_CHECKING || ret == ALG_DONE ||
						ret == ALG_NOT_READY) {
				/* wait checking , use basic first */
				is_basic = true;
				break;
			} else if (ret == ALG_READY || ret == ALG_RUNNING) {
				is_basic = false;
				//chg_alg_set_setting(alg, &info->setting);
				chg_alg_start_algo(alg);
				break;
			} else {
				chr_err("algorithm ret is error");
				is_basic = true;
			}
		}
	} else {
		if (info->enable_hv_charging != true ||
		    pdata->charging_current_limit == 0 ||
		    pdata->input_current_limit == 0) {
			for (i = 0; i < MAX_ALG_NO; i++) {
				alg = info->alg[i];
				if (alg == NULL)
					continue;

				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000 && chg_alg_is_algo_running(alg))
					chg_alg_stop_algo(alg);

				chr_err("%s: Stop hv charging. en_hv:%d alg:%s alg_vbus:%d\n",
					__func__, info->enable_hv_charging,
					dev_name(&alg->dev), val);
			}
		}
	}
	info->is_chg_done = chg_done;

	if (is_basic == true) {
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);

		chr_err("%s:old_cv=%d,cv=%d, vbat_mon_en=%d\n",
			__func__,
			info->old_cv,
			info->setting.cv,
			info->setting.vbat_mon_en);
		if (info->old_cv == 0 || (info->old_cv != info->setting.cv)
		    || info->setting.vbat_mon_en == 0) {
			charger_dev_enable_6pin_battery_charging(
				info->chg1_dev, false);
			charger_dev_set_constant_voltage(info->chg1_dev,
				info->setting.cv);
			if (info->setting.vbat_mon_en && info->stop_6pin_re_en != 1)
				charger_dev_enable_6pin_battery_charging(
					info->chg1_dev, true);
			info->old_cv = info->setting.cv;
		} else {
			if (info->setting.vbat_mon_en && info->stop_6pin_re_en != 1) {
				info->stop_6pin_re_en = 1;
				charger_dev_enable_6pin_battery_charging(
					info->chg1_dev, true);
			}
		}
	}

	chr_info("%s recharger_flag = %d\n",__func__,recharger_flag);
	if (pdata->input_current_limit == 0 ||
	    pdata->charging_current_limit == 0 || recharger_flag == EVENT_RECHG_BLOCK)
		charger_dev_enable(info->chg1_dev, false);
	else {
		alg = get_chg_alg_by_name("pe5");
		ret = chg_alg_is_algo_ready(alg);
		if (!(ret == ALG_READY || ret == ALG_RUNNING))
			charger_dev_enable(info->chg1_dev, true);
	}

	if (info->chg1_dev != NULL)
		charger_dev_dump_registers(info->chg1_dev);

	if (info->chg2_dev != NULL)
		charger_dev_dump_registers(info->chg2_dev);

	wait_fg_update_soc(block_flag, info);

	chr_err("algo end  info->is_chg_done:%d,chg_done:%d,recharger_flag:%d\n",
                                info->is_chg_done,chg_done,recharger_flag);
	return 0;
}

static int enable_charging(struct mtk_charger *info,
						bool en)
{
	int i;
	struct chg_alg_device *alg;


	chr_err("%s %d\n", __func__, en);

	if (en == false) {
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;
			chg_alg_stop_algo(alg);
		}
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
	} else {
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	}

	return 0;
}

static int charger_dev_event(struct notifier_block *nb, unsigned long event,
				void *v)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	struct mtk_charger *info =
			container_of(nb, struct mtk_charger, chg1_nb);
	struct chgdev_notify *data = v;
	int i;

	chr_err("%s %lu\n", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		info->stop_6pin_re_en = 1;
		notify.evt = EVT_FULL;
		notify.value = 0;
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}

		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		pr_info("%s: safety timer timeout\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		pr_info("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	case CHARGER_DEV_NOTIFY_BATPRO_DONE:
		info->batpro_done = true;
		info->setting.vbat_mon_en = 0;
		notify.evt = EVT_BATPRO_DONE;
		notify.value = 0;
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}
		pr_info("%s: batpro_done = %d\n", __func__, info->batpro_done);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

static int to_alg_notify_evt(unsigned long evt)
{
	switch (evt) {
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		return EVT_VBUSOVP;
	case CHARGER_DEV_NOTIFY_IBUSOCP:
		return EVT_IBUSOCP;
	case CHARGER_DEV_NOTIFY_IBUSUCP_FALL:
		return EVT_IBUSUCP_FALL;
	case CHARGER_DEV_NOTIFY_BAT_OVP:
		return EVT_VBATOVP;
	case CHARGER_DEV_NOTIFY_IBATOCP:
		return EVT_IBATOCP;
	case CHARGER_DEV_NOTIFY_VBATOVP_ALARM:
		return EVT_VBATOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VBUSOVP_ALARM:
		return EVT_VBUSOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VOUTOVP:
		return EVT_VOUTOVP;
	case CHARGER_DEV_NOTIFY_VDROVP:
		return EVT_VDROVP;
	default:
		return -EINVAL;
	}
}

static int dvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}


int mtk_basic_charger_init(struct mtk_charger *info)
{

	info->algo.do_algorithm = do_algorithm;
	info->algo.enable_charging = enable_charging;
	info->algo.do_event = charger_dev_event;
	info->algo.do_dvchg1_event = dvchg1_dev_event;
	info->algo.do_dvchg2_event = dvchg2_dev_event;
	//info->change_current_setting = mtk_basic_charging_current;
	return 0;
}
