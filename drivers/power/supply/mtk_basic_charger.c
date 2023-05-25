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

//longcheer 2022/6/7 nielianjie10 Percent 98 recharger
#define recharger_val  98
//longcheer 2022/7/19 nielianjie10 Wait time 5 times
#define wait_time      5          //5 seconds each time

static int recharger_flag = EVENT_RECHG_INIT;

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

static bool select_charging_current_limit(struct mtk_charger *info,
	struct chg_limit_setting *setting)
{
	struct charger_data *pdata, *pdata2;
	bool is_basic = false;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;
	// longcheer nielianjie10 2022/06/14 float charger type
	int float_input_current = 1000000;         //float input current 1000 mA
	int float_charger_current = 1000000;       //float charger current 1000 mA

        struct power_supply     *battery_psy = power_supply_get_by_name("battery");//begin 234935
        union power_supply_propval val = {0,}; //end 234935

	select_cv(info);

	pdata = &info->chg_data[CHG1_SETTING];
	pdata2 = &info->chg_data[CHG2_SETTING];
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

	if ((info->bootmode == 1) ||
	    (info->bootmode == 5)) {
		pdata->input_current_limit = 200000; /* 200mA */
		is_basic = true;
		goto done;
	}

	if (info->atm_enabled == true
		&& (info->chr_type == POWER_SUPPLY_TYPE_USB ||
		info->chr_type == POWER_SUPPLY_TYPE_USB_CDP)
		) {
		pdata->input_current_limit = 100000; /* 100mA */
		is_basic = true;
		goto done;
	}

	if (info->chr_type == POWER_SUPPLY_TYPE_USB) {
		pdata->input_current_limit =
				info->data.usb_charger_current;
		/* it can be larger */
		pdata->charging_current_limit =
				info->data.usb_charger_current;
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
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		/* NONSTANDARD_CHARGER */
		pdata->input_current_limit = float_input_current;
		pdata->charging_current_limit = float_charger_current;
		is_basic = true;
	}

	if (support_fast_charging(info))
		is_basic = false;
	else {
		is_basic = true;
		/* AICL */
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

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
			&& info->chr_type == POWER_SUPPLY_TYPE_USB)
			chr_debug("USBIF & STAND_HOST skip current check\n");
		else {
			if (info->sw_jeita.sm == TEMP_T1_TO_T2 && info->chr_type != POWER_SUPPLY_TYPE_USB){ //modfiy 235618
				pdata->input_current_limit = 1500000;
				pdata->charging_current_limit = info->sw_jeita.curr;
			} else if (info->sw_jeita.sm == TEMP_T0_TO_T1 ) {
                                pdata->input_current_limit = 500000;
                                pdata->charging_current_limit = info->sw_jeita.curr;
                        }else if (info->sw_jeita.sm == TEMP_T3_TO_T4 && info->chr_type == POWER_SUPPLY_TYPE_USB_DCP){ //begin 235618
				pdata->input_current_limit = info->sw_jeita.curr;
				pdata->charging_current_limit = info->sw_jeita.curr;
			} //end 235618

                        chr_err("enable_sw_jeita,sw_jeita.curr=%d\n",
                        info->sw_jeita.curr);
		}
	}

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
			pdata->charging_current_limit) {
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
					pdata->thermal_charging_current_limit;
		}
	} else
		info->setting.charging_current_limit1 = -1;

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
			pdata->input_current_limit) {
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
			info->setting.input_current_limit1 =
					pdata->input_current_limit;
		}
	} else
		info->setting.input_current_limit1 = -1;

	if (pdata2->thermal_charging_current_limit != -1) {
		if (pdata2->thermal_charging_current_limit <
			pdata2->charging_current_limit) {
			pdata2->charging_current_limit =
					pdata2->thermal_charging_current_limit;
			info->setting.charging_current_limit2 =
					pdata2->charging_current_limit;
		}
	} else
		info->setting.charging_current_limit2 = -1;

	if (pdata2->thermal_input_current_limit != -1) {
		if (pdata2->thermal_input_current_limit <
			pdata2->input_current_limit) {
			pdata2->input_current_limit =
					pdata2->thermal_input_current_limit;
			info->setting.input_current_limit2 =
					pdata2->input_current_limit;
		}
	} else
		info->setting.input_current_limit2 = -1;

	if (is_basic == true && pdata->input_current_limit_by_aicl != -1) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
done:

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}
	//degin	234935
	if(pdata->input_current_limit < 1500000){
                if (battery_psy) {
                        ret = power_supply_get_property(battery_psy,
                                POWER_SUPPLY_PROP_MTBF_CUR, &val);
                        if (ret) {
                                chr_err("get mtbf current failed!!\n");
                        } else {
				if(val.intval >= 1500){
					chr_err("mtbf current limit is %d\n", val.intval);
					pdata->input_current_limit = val.intval * 1000;
				}
			}
		} else {
                        chr_err("mtbf battery_psy not found\n");
                }
	} //end 234935

	chr_err("m:%d chg1:%d,%d,%d,%d chg2:%d,%d,%d,%d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
		info->config,
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(pdata2->thermal_input_current_limit),
		_uA_to_mA(pdata2->thermal_charging_current_limit),
		_uA_to_mA(pdata2->input_current_limit),
		_uA_to_mA(pdata2->charging_current_limit),
		info->chr_type, info->pd_type,
		info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		info->bootmode, is_basic);

	return is_basic;
}

/* longcheer  2022/7/19 nielianjie10 add get gauge for battery info */
int get_fg_batt_info(struct mtk_charger *info){
	int fg_soc = 0;
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	if (gm == NULL) {
		chr_err("%s gm is null,get gm is false!\n", __func__);
		fg_soc = get_uisoc(info);
		chr_err("%s real_soc is uisoc, fg_soc = %d \n", __func__, fg_soc);
	} else {
		fg_soc = gm->soc;
		chr_err("%s get gm is success, fg_soc = %d \n", __func__, fg_soc);
	}

	return fg_soc;
}
/* longcheer  2022/7/19 nielianjie10 add get gauge for battery info end*/
/* longcheer  2022/7/19 nielianjie10 add wait update for soc */
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
/* longcheer  2022/7/19 nielianjie10 add wait update for soc end*/

static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
	struct mtk_battery *gm;
	bool is_basic = true;
	bool chg_done = false,block_flag = false;
	int i,ret;
	int val = 0;
	int real_soc = 0;
	int current_bat_temp = 0;

	pdata = &info->chg_data[CHG1_SETTING];
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	is_basic = select_charging_current_limit(info, &info->setting);
	current_bat_temp = get_battery_temperature(info);

	/*longcheer 2022/6/7 nielianjie10 Percent 98 recharger */
	real_soc = get_fg_batt_info(info);

	if (current_bat_temp < info->data.temp_t3_thres) {
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
                        	chr_err("%s %d battery full\n", __func__,__LINE__);
			} else {
                        	charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
				recharger_flag = EVENT_RECHG_START;
                        	chr_err("%s %d battery recharge\n", __func__,__LINE__);
                	}
        	}
	}
	/*longcheer 2022/6/7 nielianjie10 Percent 98 recharger end*/
	chr_err("%s is_basic:%d\n", __func__, is_basic);
	if (is_basic != true) {
		is_basic = true;
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
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
		charger_dev_set_constant_voltage(info->chg1_dev,
			info->setting.cv);
	}

	//longcheer 2022/6/7 nielianjie10 Percent 98 recharger
	chr_info("%s recharger_flag = %d\n",__func__,recharger_flag);
	if (pdata->input_current_limit == 0 ||
	    pdata->charging_current_limit == 0 || recharger_flag == EVENT_RECHG_BLOCK)
		charger_dev_enable(info->chg1_dev, false);
	else
		charger_dev_enable(info->chg1_dev, true);

	if (info->chg1_dev != NULL)
		charger_dev_dump_registers(info->chg1_dev);

	if (info->chg2_dev != NULL)
		charger_dev_dump_registers(info->chg2_dev);

	//longcheer  2022/7/19 nielianjie add wait update for soc
	wait_fg_update_soc(block_flag, info);

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

	chr_err("%s %d\n", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		notify.evt = EVT_FULL;
		notify.value = 0;
		//begin 233028
		battery_status_back(CHARGER_NOTIFY_EOC);
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}//end 233028

		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		//begin 233028
		battery_status_back(CHARGER_NOTIFY_START_CHARGING);//end 233028
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		//begin 233028
		battery_status_back(CHARGER_NOTIFY_ERROR);//end 233028
		info->safety_timeout = true;
		pr_info("%s: safety timer timeout\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		//begin 233028
		battery_status_back(CHARGER_NOTIFY_ERROR);//end 233028
		info->vbusov_stat = data->vbusov_stat;
		pr_info("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}



int mtk_basic_charger_init(struct mtk_charger *info)
{

	info->algo.do_algorithm = do_algorithm;
	info->algo.enable_charging = enable_charging;
	info->algo.do_event = charger_dev_event;
	//info->change_current_setting = mtk_basic_charging_current;
	return 0;
}



