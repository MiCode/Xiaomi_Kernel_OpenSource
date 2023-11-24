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
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
#include "mtk_battery.h"
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/

/* N17 code for HQHW-5241 by p-xuyechen at 2023/9/11 */
#define PD_CHARGER_CURRENT_LIMIT 3600000                         //3.6A

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

static void select_cv(struct mtk_charger *info)
{
	u32 constant_voltage;

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			info->setting.cv = info->sw_jeita.cv;
			return;
		}

	constant_voltage = info->data.battery_cv;
	info->setting.cv = constant_voltage;
}

/*N17 code for HQ-293343 by miaozhichao at 2023/4/24 start*/
static bool is_typec_adapter(struct mtk_charger *info)
{
	int rp;

	rp = adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL);
	if (info->pd_type == MTK_PD_CONNECT_TYPEC_ONLY_SNK &&
	    rp != 500 &&
	    info->chr_type != POWER_SUPPLY_TYPE_USB &&
	    info->chr_type != POWER_SUPPLY_TYPE_USB_CDP
	    && info->chr_type != POWER_SUPPLY_TYPE_USB_DCP
/*N17 code for HQHW-4133 by miaozhichao at 2023/4/24 start*/
	    && info->chr_type != POWER_SUPPLY_TYPE_USB_ACA)
/*N17 code for HQHW-4133 by miaozhichao at 2023/4/24 end*/
		return true;

	return false;
}

/*N17 code for HQ-293343 by miaozhichao at 2023/4/24 end*/

static bool support_fast_charging(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int i = 0, state = 0;
	bool ret = false;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;
/*N17 code for HQ-291625 by miaozhichao at 2023/04/26 start*/
		chr_err("%s: alg:%d\n", __func__, i);
		chr_err("%s: %d %d %d\n", __func__,
			info->enable_fast_charging_indicator, alg->alg_id,
			info->fast_charging_indicator);
/*N17 code for HQ-291625 by miaozhichao at 2023/04/26 end*/
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
/* N17 code for HQ-307331 by tongjiacheng at 20230726 start */
	if (info->battery_temp <= 5)
		return false;
/* N17 code for HQ-307331 by tongjiacheng at 20230726 end */
	return ret;
}

static bool select_charging_current_limit(struct mtk_charger *info,
					  struct chg_limit_setting
					  *setting)
{
	struct charger_data *pdata, *pdata2, *pdata_dvchg, *pdata_dvchg2;
	bool is_basic = false;
/*N17 code for HQHW-4676 by wangtingting at 2023/7/24 start*/
	bool id_flag = false;
/*N17 code for HQHW-4676 by wangtingting at 2023/7/24 end*/
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;
/*N17 code for HQ-299665 by miaozhichao at 2023/6/14 start*/
	union power_supply_propval val_full;
	struct power_supply *psy;
/*N17 code for HQ-299665 by miaozhichao at 2023/6/14 end*/
	/* N17 code for HQ-292280 by tongjiacheng at 20230610 start */
	int level;
	/* N17 code for HQ-292280 by tongjiacheng at 20230610 end */
	/* N17 code for HQ-292280 by tongjiacheng at 20230613 start */
	union power_supply_propval val;
	/* N17 code for HQ-292280 by tongjiacheng at 20230613 end */
	/* N17 code for HQHW-4275 by tongjiacheng at 20230625 start */
	static bool pe5_thermal_stop = true;
	/* N17 code for HQHW-4275 by tongjiacheng at 20230625 end */
        /*N17 code for low_fast by xm liluting at 2023/07/07 start*/
        int thermal_vote_current;
        struct mtk_battery *gm;
        union power_supply_propval val_capacity;
        int blankState = 0, low_fast_enable = 0;
        bool fast_flag = false;
        time64_t time_now = 0, delta_time = 0;
        static time64_t time_last = 0;
        static int last_level = 0;
        static bool hot_flag = false;
        /*N17 code for low_fast by xm liluting at 2023/07/07 end*/

	select_cv(info);

/* N17 code for HQ-292280 by tongjiacheng at 20230610 start */
	level = info->thermal_level;
/* N17 code for HQ-292280 by tongjiacheng at 20230610 end */
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
		pdata->input_current_limit =
		    info->data.usb_charger_current;
		pdata->charging_current_limit =
		    info->data.usb_charger_current;
		is_basic = true;
		goto done;
	}

	if (((info->bootmode == 1) ||
	     (info->bootmode == 5))
	    && info->enable_meta_current_limit != 0) {
		pdata->input_current_limit = 200000;	// 200mA
		is_basic = true;
		goto done;
	}

	if (info->atm_enabled == true
	    && (info->chr_type == POWER_SUPPLY_TYPE_USB ||
		info->chr_type == POWER_SUPPLY_TYPE_USB_CDP)
	    ) {
		pdata->input_current_limit = 100000;	/* 100mA */
		is_basic = true;
		goto done;
	}

	if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
/*N17 code for HQHW-4133 by miaozhichao at 2023/4/24 start*/
		if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 ||info->pd_type == MTK_PD_CONNECT_PE_READY_SNK) {
			pdata->input_current_limit =
				info->data.charging_host_charger_current;
			pdata->charging_current_limit =
				info->data.charging_host_charger_current;
		/* N17 code for HQ-319411 by p-xuyechen at 2023/9/28 start */
		} else if (MTK_PD_CONNECT_PE_READY_SNK_APDO == info->pd_type) {
			pdata->input_current_limit = info->data.ac_charger_input_current;
			pdata->charging_current_limit = info->data.ac_charger_input_current;
		/* N17 code for HQ-319411 by p-xuyechen at 2023/9/28 end */
		} else {
			pdata->input_current_limit =
				info->data.usb_charger_current;
			/* it can be larger */
			pdata->charging_current_limit =
				info->data.usb_charger_current;
		}
/*N17 code for HQHW-4133 by miaozhichao at 2023/4/24 end*/
		is_basic = true;
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		pdata->input_current_limit =
		    info->data.charging_host_charger_current;
		pdata->charging_current_limit =
		    info->data.charging_host_charger_current;
		is_basic = true;

	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		pdata->input_current_limit =
		    info->data.ac_charger_input_current;
		pdata->charging_current_limit =
		    info->data.ac_charger_current;
		if (info->config == DUAL_CHARGERS_IN_SERIES) {
			pdata2->input_current_limit =
			    pdata->input_current_limit;
			pdata2->charging_current_limit = 2000000;
		}
	}
/*N17 code for HQ-293343 by miaozhichao at 2023/4/24 start*/
	else if (info->chr_type == POWER_SUPPLY_TYPE_USB_ACA) {
		pdata->input_current_limit =
		    info->data.ac_charger_input_current;
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 start*/
		pdata->charging_current_limit = 3600000;
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 end*/
		is_basic = true;
/*N17 code for HQ-293343 by miaozhichao at 2023/4/24 end*/
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
		   info->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
		/* NONSTANDARD_CHARGER */
/*N17 code for HQ-290909 by miaozhichao at 2023/5/25 start*/
		pdata->input_current_limit =
		    info->data.non_std_ac_charger_current;
		pdata->charging_current_limit =
		    info->data.non_std_ac_charger_current;
/*N17 code for HQHW-4567 by wangtingting at 2023/7/10 start*/
          if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK) {
			pdata->input_current_limit =
				info->data.ac_charger_input_current;
			/* N17 code for HQHW-5241 by p-xuyechen at 2023/9/11 */
			pdata->charging_current_limit = PD_CHARGER_CURRENT_LIMIT;
		/* N17 code for HQ-319411 by p-xuyechen at 2023/9/28 start */
		}  else if (MTK_PD_CONNECT_PE_READY_SNK_APDO == info->pd_type) {
			pdata->input_current_limit = info->data.ac_charger_input_current;
			pdata->charging_current_limit = info->data.ac_charger_input_current;
		}
		/* N17 code for HQ-319411 by p-xuyechen at 2023/9/28 end */
/*N17 code for HQHW-4567 by wangtingting at 2023/7/10 end*/
/*N17 code for HQ-290909 by miaozhichao at 2023/5/25 end*/
		is_basic = true;
	} else {
		/*chr_type && usb_type cannot match above, set 500mA */
		pdata->input_current_limit =
		    info->data.usb_charger_current;
		pdata->charging_current_limit =
		    info->data.usb_charger_current;
		is_basic = true;
	}

/*N17 code for HQHW-4676 by wangtingting at 2023/7/24 start*/
	if (!id_flag) {
		psy = power_supply_get_by_name("batt_verify");
		if (psy) {
			power_supply_get_property(psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &val);
			if(val.intval)
				id_flag = true;
			else
				id_flag = false;
		}
	}

	if (support_fast_charging(info) && id_flag)
/*N17 code for HQHW-4676 by wangtingting at 2023/7/24 end*/
		is_basic = false;
	/* N17 code for HQHW-4815 by p-xuyechen at 2023/8/15 start */
	else if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || info->pd_type == MTK_PD_CONNECT_PE_READY_SNK) {
		pr_info("pd_type = %d, set basic to false\n", info->pd_type);
		is_basic = false;
	} else {
	/* N17 code for HQHW-4815 by p-xuyechen at 2023/8/15 end */
		is_basic = true;
		/* AICL */
		if (!info->disable_aicl)
			charger_dev_run_aicl(info->chg1_dev,
					     &pdata->
					     input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
			    info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
				    info->data.max_dmivr_charger_current;
		}
		if (is_typec_adapter(info)) {
			if (adapter_dev_get_property
			    (info->pd_adapter, TYPEC_RP_LEVEL)
			    == 3000) {
				pdata->input_current_limit = 3000000;
				pdata->charging_current_limit = 3000000;
			} else
			    if (adapter_dev_get_property
				(info->pd_adapter,
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
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 start*/
	if (info->enable_sw_jeita) {
		if (pdata->charging_current_limit >= info->sw_jeita.cc)
			pdata->charging_current_limit = info->sw_jeita.cc;
	}
	pr_info("charging_current_limit: %d, sw_jeita.cc: %d\n",
		pdata->charging_current_limit, info->sw_jeita.cc);
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 end*/
	sc_select_charging_current(info, pdata);

/* N17 code for HQ-299825 by tongjiacheng at 20230616 start */
	ret = power_supply_get_property(info->bat_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &val);
	if (ret)
		chr_err("get const current limit fail(%d)\n", ret);

	if (val.intval != 0)	//phone mode, all type need set ibat limit to 500mA
		pdata->charging_current_limit = 500000;
/* N17 code for HQ-299825 by tongjiacheng at 20230616 end */

/* N17 code for HQ-292280 by tongjiacheng at 20230610 start */
	if (level != 0) {
		if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
/*N17 code for low_fast by xm liluting at 2023/07/07 start*/
                        psy = power_supply_get_by_name("battery");
		        if (psy != NULL) {
                                gm = (struct mtk_battery *)power_supply_get_drvdata(psy);
                                if (gm != NULL){
                                        low_fast_enable = gm->smart_charge[SMART_CHG_LOW_FAST].en_ret;
                                } else {
                                        chr_err("%s failed to get mtk_battery drvdata\n", __func__);
                                }
                        } else {
                                chr_err("%s failed to get battery psy\n", __func__);
                        }
                        ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_CAPACITY, &val_capacity);
                        if((val_capacity.intval <= 40) && (info->first_low_plugin_flag) && low_fast_enable) {
                                blankState = info->sm.screen_state;
                                chr_err("%s capacity = %d, level = %d, first_low_plugin_flag = %d, low_fast_enable = %d, blankState = %d, b_flag = %d\n", __func__, val_capacity.intval, level, info->first_low_plugin_flag, low_fast_enable, blankState, info->b_flag);
                                //info->sm.screen_state 0:bright, 1:black
                                if(info->b_flag == NORMAL && !blankState) {  //black to bright
                                        info->b_flag = BLACK_TO_BRIGHT;
                                        time_last = ktime_get_seconds();
                                        fast_flag = true;
                                        chr_err("%s switch to bright time_last = %d\n", __func__, time_last);
                                }
                                else if((info->b_flag == BLACK_TO_BRIGHT || info->b_flag == BRIGHT) && !blankState) {  //still bright
                                        info->b_flag = BRIGHT;
                                        time_now = ktime_get_seconds();
                                        delta_time = time_now - time_last;
                                        chr_err("%s still_bright time_now = %d, time_last = %d, delta_time = %d\n", __func__, time_now, time_last, delta_time);
                                        if(delta_time <= 15) {
                                                fast_flag = true;
                                                chr_err("%s still_bright delta_time = %d, stay fast\n", __func__, delta_time);
                                        }
                                        else {
                                                fast_flag = false;
                                                chr_err("%s still_bright delta_time = %d, exit fast\n", __func__, delta_time);
                                        }
                                }
                                else { //black
                                        info->b_flag = BLACK;
                                        fast_flag = true;
                                        chr_err("%s black stay fast\n", __func__, delta_time);
                                }

                                /*avoid thermal_board_temp raise too fast*/
                                if((last_level == 8) && (level == 5) && (info->thermal_board_temp > 410)){
                                        hot_flag = true;
                                        fast_flag = false;
                                        chr_err("%s avoid thermal_board_temp raise too fast, exit fast mode\n", __func__);
                                }
                                else if((last_level == 5) && ((level == 5) || (level == 8)) && hot_flag && (info->thermal_board_temp > 410)){
                                        fast_flag = false;
                                }
                                else{
                                        hot_flag = false;
                                }

                                if(info->thermal_board_temp > 420){
                                        fast_flag = false;
                                }

                                if(fast_flag) {  //stay fast strategy
                                        info->pps_fast_mode = true;
                                        thermal_vote_current = thermal_mitigation_pps_fast[level];

                                        if((val_capacity.intval > 38) && (info->thermal_board_temp > 380)){
                                                if(thermal_vote_current >= 5550000){
                                                        thermal_vote_current -= 3300000;
                                                }
                                                else{
                                                        thermal_vote_current = 2250000;
                                                }
                                                chr_err("%s stay fast but decrease 3.3, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
                                        }
                                        else if(info->thermal_board_temp > 410){
                                                if(thermal_vote_current >= 5250000){
                                                        thermal_vote_current -= 3000000;
                                                }
                                                else{
                                                        thermal_vote_current = 2250000;
                                                }
                                                chr_err("%s stay fast but decrease 3, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
                                        }
                                        else if(info->thermal_board_temp > 400){
                                                if(thermal_vote_current >= 3250000){
                                                        thermal_vote_current -= 1500000;
                                                }
                                                else{
                                                        thermal_vote_current = 2250000;
                                                }
                                                chr_err("%s stay fast but decrease 1.5, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
                                        }
                                        else if((val_capacity.intval > 30) && (val_capacity.intval < 39) && (info->thermal_board_temp < 390)){
                                                if(thermal_vote_current <= 4000000){
                                                        thermal_vote_current += 2000000;
                                                }
                                                else{
                                                        thermal_vote_current = 6000000;
                                                }
                                                chr_err("%s stay fast but add 2, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
                                        }
                                        else if((val_capacity.intval > 25) && ((val_capacity.intval <= 30)) && (info->thermal_board_temp < 400)){
                                                if(thermal_vote_current <= 2000000){
                                                        thermal_vote_current += 3000000;
                                                }
                                                else{
                                                        thermal_vote_current = 6000000;
                                                }
                                                chr_err("%s stay fast but add 3, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
                                        }
                                        chr_err("%s stay fast, thermal_vote_current = %d\n", __func__, thermal_vote_current);
                                }
                                else { //exit fast strategy
                                        info->pps_fast_mode = false;
                                        thermal_vote_current = thermal_mitigation_pps[level];
                                        chr_err("%s exit fast, thermal_vote_current = %d\n", __func__, thermal_vote_current);
                                }
                        }
                        else { //capacity > 40, or low_fast was not triggered, use default strategy
                                info->b_flag = NROMAL;
                                info->pps_fast_mode = false;
                                thermal_vote_current = thermal_mitigation_pps[level];
                                chr_err("%s use default strategy, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
                        }
/*N17 code for cp_mode test by xm liluting at 2023/07/31 start*/
                        if(info->fake_thermal_vote_current > 0){
                                thermal_vote_current = info->fake_thermal_vote_current;
                        }
                        last_level = level;
/*N17 code for cp_mode test by xm liluting at 2023/07/31 end*/
/*N17 code for low_fast by xm liluting at 2023/07/07 end*/
			pdata_dvchg->thermal_input_current_limit =
				thermal_vote_current / 2;

			if (pdata_dvchg->thermal_input_current_limit  < 1050000) { //退出PE5，设置swchg thermal
				pdata->thermal_charging_current_limit =
					thermal_vote_current;
	/* N17 code for HQHW-4275 by tongjiacheng at 20230625 start */
				if (chg_alg_is_algo_running(info->alg[0])) {
					pe5_thermal_stop = true;
	/* N17 code for HQHW-4275 by tongjiacheng at 20230625 end */
        /*N17 code for HQHW-4632 by tongjiacheng at 20230718 start*/
					chg_alg_stop_algo(info->alg[0]);
        /*N17 code for HQHW-4632 by tongjiacheng at 20230718 end*/
				}
	/* N17 code for HQHW-4275 by tongjiacheng at 20230627 start */
				is_basic = true;
	/* N17 code for HQHW-4275 by tongjiacheng at 20230627 end */
			}
			else {
/*N17 code for low_fast by xm liluting at 2023/07/26 start*/
                                if((val_capacity.intval <= 40) && (info->first_low_plugin_flag) && low_fast_enable && (pdata_dvchg->thermal_input_current_limit > 1050000)){
                                        chg_alg_start_algo(info->alg[0]);
                                }
/*N17 code for low_fast by xm liluting at 2023/07/26 end*/
				pdata->thermal_charging_current_limit = -1;
				/* N17 code for HQHW-4275 by tongjiacheng at 20230625 start */
				if (!chg_alg_is_algo_running(info->alg[0]) &&
					pe5_thermal_stop) { //唤醒PE5
			/* N17 code for HQHW-4275 by tongjiacheng at 20230627 start */
					chg_alg_thermal_restart(info->alg[0], false);
					chg_alg_start_algo(info->alg[0]);
					msleep(5);
					chg_alg_thermal_restart(info->alg[0], true);
					is_basic = false;
			/* N17 code for HQHW-4275 by tongjiacheng at 20230627 end */
					pe5_thermal_stop = false;
				}
				/* N17 code for HQHW-4275 by tongjiacheng at 20230625 end */
			}
		/* N17 code for HQHW-4815 by p-xuyechen at 2023/8/15 end */
		} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_ACA ||
					(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK) ||
					(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30))
		/* N17 code for HQHW-4815 by p-xuyechen at 2023/8/15 end */
			pdata->thermal_charging_current_limit =
				thermal_mitigation_qc[level];
	}
	else {
/*N17 code for HQHW-4632 by tongjiacheng at 20230718 start*/
		if (!chg_alg_is_algo_running(info->alg[0]) &&
			pe5_thermal_stop &&
			info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
			chg_alg_thermal_restart(info->alg[0], false);
			chg_alg_start_algo(info->alg[0]);
			msleep(5);
			chg_alg_thermal_restart(info->alg[0], true);
			is_basic = false;
			pe5_thermal_stop = false;
		}
/*N17 code for HQHW-4632 by tongjiacheng at 20230718 end*/
		pdata->thermal_charging_current_limit = -1;
		pdata_dvchg->thermal_input_current_limit = -1;
	}
/* N17 code for HQ-292280 by tongjiacheng at 20230610 end */
/*N17 code for HQ-299665 by miaozhichao at 2023/6/14 start*/
	psy = power_supply_get_by_name("battery");
	if (psy == NULL)
		return -ENODEV;
	ret = power_supply_get_property(psy,POWER_SUPPLY_PROP_ENERGY_FULL,&val_full);
	chr_err("val_full :%d\n",val_full.intval);
	if(val_full.intval == true) {
		pdata->charging_current_limit = 0;
	}
/*N17 code for HQ-299665 by miaozhichao at 2023/6/14 end*/
	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <=
		    pdata->charging_current_limit) {
			pdata->charging_current_limit =
			    pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
			    pdata->thermal_charging_current_limit;
		}
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
	    && !info->charger_unlimited && !info->disable_aicl) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
			    pdata->input_current_limit_by_aicl;
	}
	info->setting.input_current_limit_dvchg1 =
	    pdata_dvchg->thermal_input_current_limit;

      done:

	ret =
	    charger_dev_get_min_charging_current(info->chg1_dev,
						 &ichg1_min);
	if (ret != -EOPNOTSUPP
	    && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret =
	    charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -EOPNOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}
	/* For TC_018, pleasae don't modify the format */
	chr_err
	    ("m:%d chg1:%d,%d,%d,%d chg2:%d,%d,%d,%d dvchg1:%d sc:%d %d %d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
	     info->config, _uA_to_mA(pdata->thermal_input_current_limit),
	     _uA_to_mA(pdata->thermal_charging_current_limit),
	     _uA_to_mA(pdata->input_current_limit),
	     _uA_to_mA(pdata->charging_current_limit),
	     _uA_to_mA(pdata2->thermal_input_current_limit),
	     _uA_to_mA(pdata2->thermal_charging_current_limit),
	     _uA_to_mA(pdata2->input_current_limit),
	     _uA_to_mA(pdata2->charging_current_limit),
	     _uA_to_mA(pdata_dvchg->thermal_input_current_limit),
	     info->sc.pre_ibat, info->sc.sc_ibat, info->sc.solution,
	     info->chr_type, info->pd_type, info->usb_unlimited,
	     IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
	     pdata->input_current_limit_by_aicl, info->atm_enabled,
	     info->bootmode, is_basic);

	return is_basic;
}


static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 start*/
	struct mtk_battery *gm;
	struct power_supply *psy = NULL;
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 end*/
	bool is_basic = true;
	bool chg_done = false;
	int i;
	int ret;
	int val = 0;
	union power_supply_propval pval;
	/* N17 code for HQHW-5009 by p-xuyechen at 2023/8/18 */
	struct chg_alg_device *alg_pd = NULL;
/* N17 code for HQ-308497 by miaozhichao at 20230722 start */
	if (!info->bat_psy)
		info->bat_psy = power_supply_get_by_name("battery");
	if (!info->bat_psy)
		return -ENODEV;
/* N17 code for HQ-308497 by miaozhichao at 20230722 end */

	pdata = &info->chg_data[CHG1_SETTING];
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	is_basic = select_charging_current_limit(info, &info->setting);
        alg = get_chg_alg_by_name("pe5");

        ret = power_supply_get_property(info->bat_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);

	if (info->is_chg_done != chg_done) {
		if (chg_done) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL,
					     0);
			info->polling_interval = CHARGING_FULL_INTERVAL;
                        if(pval.intval == 100)
                        {
                                info->is_full_flag = true;
			        chr_err("%s battery full, is_full_flag = %d\n", __func__, info->is_full_flag);
                        }
		} else {
                        if(pval.intval < 100)
                        {
			        charger_dev_do_event(info->chg1_dev,
					        EVENT_RECHARGE, 0);
			        info->polling_interval = CHARGING_INTERVAL;
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
			    ((alg->alg_id & info->
			      fast_charging_indicator) == 0))
				continue;

			if (!info->enable_hv_charging ||
			    pdata->charging_current_limit == 0 ||
			    pdata->input_current_limit == 0) {
				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000)
					chg_alg_stop_algo(alg);
				chr_err("%s: alg:%s alg_vbus:%d\n",
					__func__, dev_name(&alg->dev),
					val);
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
				chr_err("%s notify:%d\n", __func__,
					notify.evt);
			}

			chg_alg_set_current_limit(alg, &info->setting);
			ret = chg_alg_is_algo_ready(alg);

			chr_err("%s %s ret:%s\n", __func__,
				dev_name(&alg->dev),
				chg_alg_state_to_str(ret));

			if (ret == ALG_INIT_FAIL
			    || ret == ALG_TA_NOT_SUPPORT) {
				/* try next algorithm */
				continue;
			} else if (ret == ALG_TA_CHECKING
				   || ret == ALG_DONE
				   || ret == ALG_NOT_READY) {
				/* wait checking , use basic first */
				is_basic = true;
				break;
			} else if (ret == ALG_READY || ret == ALG_RUNNING) {
				is_basic = false;
				//chg_alg_set_setting(alg, &info->setting);
				/* N17 code for HQHW-5009 by p-xuyechen at 2023/8/18 start */
				if (i == 0) {
					alg_pd = get_chg_alg_by_name("pd");
					if (alg_pd && (chg_alg_is_algo_ready(alg_pd) == ALG_RUNNING)) {
						chr_err("pe5 is ready or running, stop pd algo\n");
						chg_alg_stop_algo(alg_pd);
					}
				}
				/* N17 code for HQHW-5009 by p-xuyechen at 2023/8/18 end */
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
				if (val > 5000
				    && chg_alg_is_algo_running(alg))
					chg_alg_stop_algo(alg);

				chr_err
				    ("%s: Stop hv charging. en_hv:%d alg:%s alg_vbus:%d\n",
				     __func__, info->enable_hv_charging,
				     dev_name(&alg->dev), val);
			}
		}
		/* N17 code for HQHW-5009 by p-xuyechen at 2023/8/18 start */
		alg_pd = get_chg_alg_by_name("pd");
		if (alg_pd && (chg_alg_is_algo_ready(alg_pd) == ALG_RUNNING)) {
			chr_err("is_basic is change to false, stop pd algo\n");
			chg_alg_stop_algo(alg_pd);
		}
		/* N17 code for HQHW-5009 by p-xuyechen at 2023/8/18 end */
	}
	info->is_chg_done = chg_done;

	if (is_basic == true) {
		charger_dev_set_input_current(info->chg1_dev,
					      pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
						 pdata->
						 charging_current_limit);

		chr_debug("%s:old_cv=%d,cv=%d, vbat_mon_en=%d\n",
			  __func__,
			  info->old_cv,
			  info->setting.cv, info->setting.vbat_mon_en);
		if (info->old_cv == 0 || (info->old_cv != info->setting.cv)
		    || info->setting.vbat_mon_en == 0) {
			charger_dev_enable_6pin_battery_charging(info->
								 chg1_dev,
								 false);
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 start*/
			if (info->pd_type !=
			    MTK_PD_CONNECT_PE_READY_SNK_APDO) {
				charger_dev_set_constant_voltage(info->
								 chg1_dev,
								 info->
								 setting.
								 cv);
			}
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 end*/
			if (info->setting.vbat_mon_en
			    && info->stop_6pin_re_en != 1)
				charger_dev_enable_6pin_battery_charging
				    (info->chg1_dev, true);
			info->old_cv = info->setting.cv;
		} else {
			if (info->setting.vbat_mon_en
			    && info->stop_6pin_re_en != 1) {
				info->stop_6pin_re_en = 1;
				charger_dev_enable_6pin_battery_charging
				    (info->chg1_dev, true);
			}
		}
	}

	if (pdata->input_current_limit == 0 ||
	    pdata->charging_current_limit == 0)
		charger_dev_enable(info->chg1_dev, false);
	else {
		alg = get_chg_alg_by_name("pe5");
		ret = chg_alg_is_algo_ready(alg);

/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 start*/
		psy = power_supply_get_by_name("battery");
		if (psy != NULL) {
			gm = (struct mtk_battery *)power_supply_get_drvdata(psy);
			if (gm != NULL){
				chr_err("N17 get mtk_battery drvdata success in %s\n", __func__);
			}
		} else
			return 0;
		chr_err("xm %s enable charger ret = %d, active_status = %d\n", __func__, ret, gm->smart_charge[SMART_CHG_NAVIGATION].active_status);
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 end*/
		if (!(ret == ALG_READY || ret == ALG_RUNNING)
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 start*/
/*N17 code for HQHW-4654 by tongjiacheng at 2023/08/04 start*/
			&& !gm->smart_charge[SMART_CHG_NAVIGATION].active_status && !chg_done &&
			/*N17 code for HQ-314179 by hankang at 2023/08/31 start*/
			info->sw_jeita.charging == true){
			/*N17 code for HQ-314179 by hankang at 2023/08/31 end*/
/*N17 code for HQHW-4654 by tongjiacheng at 2023/08/04 end*/
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 end*/
			charger_dev_enable(info->chg1_dev, true);
			chr_err("xm %s enable charger ret = %d\n", __func__, ret);
		}
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/
	}

	if (info->chg1_dev != NULL)
		charger_dev_dump_registers(info->chg1_dev);

	if (info->chg2_dev != NULL)
		charger_dev_dump_registers(info->chg2_dev);

	return 0;
}

static int enable_charging(struct mtk_charger *info, bool en)
{
	int i;
	struct chg_alg_device *alg;
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 start*/
	struct mtk_battery *gm = NULL;
	struct power_supply *psy = NULL;
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 end*/

	chr_err("%s %d\n", __func__, en);

/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 start*/
	psy = power_supply_get_by_name("battery");
	if (psy != NULL) {
		gm = (struct mtk_battery *)power_supply_get_drvdata(psy);
		if (gm != NULL){
			chr_err("N17 get mtk_battery drvdata success in %s\n", __func__);
		}
	} else
		return 0;
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 end*/

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
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 start*/
	chr_err("xm in %s enable charger, active_status = %d\n", __func__,
			gm->smart_charge[SMART_CHG_NAVIGATION].active_status);
		if (!gm->smart_charge[SMART_CHG_NAVIGATION].active_status){
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 end*/
			charger_dev_enable(info->chg1_dev, true);
			chr_err("xm %s enable charger\n", __func__);
		}
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	}

	return 0;
}

static int charger_dev_event(struct notifier_block *nb,
			     unsigned long event, void *v)
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
		pr_info("%s: vbus ovp = %d\n", __func__,
			info->vbusov_stat);
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
		pr_info("%s: batpro_done = %d\n", __func__,
			info->batpro_done);
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
