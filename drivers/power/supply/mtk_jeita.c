// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_charger.c
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
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/rtc.h>
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

#include <asm/setup.h>

#include "mtk_charger.h"
#include "mtk_battery.h"
/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 start*/
#include <tcpm.h>
/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 end*/

static int vbat_get(struct mtk_charger *info)
{
	int val,ret;
	struct charger_device *dvchg1_dev = get_charger_by_name("primary_dvchg");

	ret = charger_dev_get_adc(dvchg1_dev, ADC_CHANNEL_VBAT, &val, &val);
	if (ret < 0 || val == 0) {
		chr_err("[SW_JEITA] get vbat from cp error!\n");
		val = get_battery_voltage(info);
	} else
		val = val / 1000;
	return val;
}

/*N17 code for dropfv by liluting at 2023/7/3 start*/
void get_battaging_deltafv(struct mtk_charger *info, int cyclecount, int *fv_aging)
{
	int i = 0;

	while (cyclecount > info->data.cyclecount[i])
		i++;

	if(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO){
		*fv_aging = info->data.dropfv_ffc[i];
	}
	else{
		*fv_aging = info->data.dropfv_noffc[i];
	}

	chr_err("%s i = %d, fv_aging = %d\n", __func__, i, *fv_aging);
	return;
}

void get_max_deltafv(struct mtk_charger *info, int cyclecount, int *deltaFv)
{
	int fv_aging;

	get_battaging_deltafv(info, cyclecount, &fv_aging);

	*deltaFv = fv_aging + smart_batt_get_diff_fv();

	chr_err("%s smart_batt = %d, cyclecount = %d, deltaFv = %d\n", __func__, smart_batt_get_diff_fv(), cyclecount, *deltaFv);
	return;
}

void get_drop_floatvolatge(struct mtk_charger *info)
{
	int cyclecount = 0, deltafv = 0;
	union power_supply_propval val;
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL) {
		chr_err("[%s] battery psy is not rdy\n", __func__);
		return;
	}
	else {
		power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
		cyclecount = val.intval;
	}
	chr_err("%s cyclecount = %d\n", __func__, cyclecount);
	get_max_deltafv(info, cyclecount, &deltafv);
	/*N17 code for HQ-319688 by p-xiepengfu at 20230907 start*/
	if(deltafv)
	{
		info->deltaFv = deltafv * 1000;
		chr_err("%s cyclecount = %d, deltaFv = %d\n", __func__, cyclecount, info->deltaFv);
	}
	/*N17 code for HQ-319688 by p-xiepengfu at 20230907 end*/
}
/*N17 code for dropfv by liluting at 2023/7/3 end*/

/* sw jeita */
void do_sw_jeita_state_machine(struct mtk_charger *info)
{
	struct sw_jeita_data *sw_jeita;
	int ibat_jeita = 0;
	int vbat_jeita = 0;
/*N17 code for HQ-299665 by miaozhichao at 2023/6/14 start*/
	int xm_ieoc = 250000;
/*N17 code for HQHW-43339 by tongjiacheng at 2023/6/30 start*/
	bool chg_done = false;
/*N17 code for HQHW-43339 by tongjiacheng at 2023/6/30 end*/
	sw_jeita = &info->sw_jeita;
	sw_jeita->sm = BAT_TEMP_NORMAL;
	ibat_jeita = get_battery_current(info);	//ma
	vbat_jeita = vbat_get(info);	//mv
	sw_jeita->cc = 2000000;	//ma

	charger_dev_is_charging_done(info->chg1_dev, &chg_done);

	if (sw_jeita->charging == false) {
		if (info->battery_temp > 46 && vbat_jeita > 3960)
			sw_jeita->charging = false;
		else if (info->battery_temp <= 57 && info->battery_temp >= -8)
			sw_jeita->charging = true;
	}
	/*N17 code for HQ-319688 by p-xiepengfu at 20230907 start*/
	get_drop_floatvolatge(info);
	/*N17 code for HQ-319688 by p-xiepengfu at 20230907 end*/

	if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {	//pps
		if (!chg_alg_is_algo_running(info->alg[0]) &&
				sw_jeita->charging == true &&
				get_uisoc(info) <= 80 &&
				info->battery_temp > 5 &&
				info->battery_temp < 58 &&
				(info->chg_data[DVCHG1_SETTING].thermal_input_current_limit >= 1050000
				|| info->thermal_level == 0) &&
				info->disable_charger == false) {
				chg_alg_thermal_restart(info->alg[0], false);
				chg_alg_start_algo(info->alg[0]);
				msleep(5);
				chg_alg_thermal_restart(info->alg[0], true);
				chr_err("[%s]Return to normal temperature, restart pe50\n",__func__);
		}

		switch (info->battery_temp) {
		case -100 ... -10:
			sw_jeita->cc = 0;	//<-10
			sw_jeita->charging = false;
			charger_dev_set_constant_voltage(info->chg1_dev,
							 info->data.
							 jeita_temp_below_t0_cv - info->diff_fv_val);
			break;
		case -9 ... 0:
			xm_ieoc = 250000;	//-9~0
			sw_jeita->cc = info->data.jeita_temp_below_t0_cc;
			charger_dev_set_constant_voltage(info->chg1_dev,
							 info->data.
							 jeita_temp_below_t0_cv - info->diff_fv_val);
			break;

		case 1 ... 5:
			xm_ieoc = 250000;
			sw_jeita->cc = info->data.jeita_temp_t0_to_t1_cc;
			charger_dev_set_constant_voltage(info->chg1_dev,
							 info->data.
							 jeita_temp_t0_to_t1_cv - info->diff_fv_val);
			break;

		case 6 ... 9:
			xm_ieoc = 250000;
			//sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv;
			charger_dev_set_constant_voltage(info->chg1_dev,
							 info->data.
							 jeita_temp_t1_to_t2_cv - info->diff_fv_val);
			break;

		case 10 ... 14:
			xm_ieoc = 250000;
			//sw_jeita->cv = info->data.jeita_temp_t2_to_t3_cv;
			charger_dev_set_constant_voltage(info->chg1_dev,
							 info->data.
							 jeita_temp_t2_to_t3_cv - info->diff_fv_val);
			break;

		case 15 ... 34:
			if (info->pd_adapter->verifed == true) {	//mi adapter 33w
/*N17 code for HQHW-4275 by wangtingting at 2023/06/30 start*/
			/*N17 code for HQ-319688 by p-xiepengfu at 20230907 start*/
				if (chg_alg_is_algo_running(info->alg[0])) {
					xm_ieoc = 750000;
					if (vbat_jeita <= 4250) {
						charger_dev_set_constant_voltage
							(info->chg1_dev, 4250000 - info->diff_fv_val - info->deltaFv);
					} else if (vbat_jeita <= 4480) {
						charger_dev_set_constant_voltage
							(info->chg1_dev, 4480000 - info->diff_fv_val - info->deltaFv);
					} else {
						charger_dev_set_constant_voltage
							(info->chg1_dev, 4510000 - info->diff_fv_val - info->deltaFv);
					}
				} else {
					if (chg_alg_cp_charge_finished(info->alg[0])) { // use ffc term configs
						charger_dev_set_constant_voltage
							(info->chg1_dev, 4510000 - info->diff_fv_val - info->deltaFv);
						xm_ieoc = 750000;
					} else {  // use normal term configs
						xm_ieoc = 250000;	//normal adapter 33w
						charger_dev_set_constant_voltage(info->
										chg1_dev,
										4460000 - info->diff_fv_val - info->deltaFv);
					}
					chr_err
					("[%s]cp_charge_finished:%d xm_ieoc:%d\n",__func__,
                                         chg_alg_cp_charge_finished(info->alg[0]), xm_ieoc);
				}
/*N17 code for HQHW-4275 by wangtingting at 2023/06/30 end*/
			} else {
				xm_ieoc = 250000;	//normal adapter 33w
				charger_dev_set_constant_voltage(info->chg1_dev,
								 4460000 - info->diff_fv_val);
			}

			break;

		case 35 ... 47:
			if (info->pd_adapter->verifed == true) {	//mi adapter 33w ffc
/*N17 code for HQHW-4275 by wangtingting at 2023/06/30 start*/
				if (chg_alg_is_algo_running(info->alg[0])) {
					xm_ieoc = 850000;
					if (vbat_jeita <= 4250) {
						charger_dev_set_constant_voltage
							(info->chg1_dev, 4250000 - info->diff_fv_val - info->deltaFv);
					} else if (vbat_jeita <= 4480) {
						charger_dev_set_constant_voltage
							(info->chg1_dev, 4480000 - info->diff_fv_val - info->deltaFv);
					} else {
						charger_dev_set_constant_voltage
							(info->chg1_dev, 4500000 - info->diff_fv_val - info->deltaFv);
					}
				} else {
					if (chg_alg_cp_charge_finished(info->alg[0])) { // use ffc term configs
						charger_dev_set_constant_voltage
							(info->chg1_dev, 4510000 - info->diff_fv_val - info->deltaFv);
						xm_ieoc = 850000;
					} else {  // use normal term configs
						xm_ieoc = 250000;	//normal adapter 33w
						charger_dev_set_constant_voltage(info->chg1_dev,
										4460000 - info->diff_fv_val);
					}
					chr_err
					("[%s]cp_charge_finished:%d xm_ieoc:%d\n",__func__,
                                         chg_alg_cp_charge_finished(info->alg[0]), xm_ieoc);
				}
/*N17 code for HQHW-4275 by wangtingting at 2023/06/30 end*/
			} else {
				xm_ieoc = 250000;	//normal adapter 33w
				charger_dev_set_constant_voltage(info->chg1_dev,
								 4460000 - info->diff_fv_val);
			}
			/*N17 code for HQ-319688 by p-xiepengfu at 20230907 end*/

			break;

		case 48 ... 57:
			xm_ieoc = 250000;
			if (vbat_jeita >= 4100) {
				sw_jeita->charging = false;
				sw_jeita->cc = 0;
			}
			//sw_jeita->cv = info->data.jeita_temp_t5_to_t6_cv;
/*N17 code for HQHW-43339 by tongjiacheng at 2023/6/30 start*/
			sw_jeita->sm = BAT_TEMP_HIGH;
/*N17 code for HQHW-43339 by tongjiacheng at 2023/6/30 end*/
			break;

		case 58 ... 200:
			sw_jeita->charging = false;
			sw_jeita->cc = 0;
			//sw_jeita->cv = info->data.jeita_temp_above_t6_cv;
			break;
		default:
			chr_err
			    ("[SW_JEITA] The battery temperature is not within the range (%d) !!\n",
			     info->battery_temp);
		}

	} else {		//not adpo(hvdcp,dcp,cdp,sdp,float)

		switch (info->battery_temp) {
		case -100 ... -10:
			sw_jeita->charging = false;
			sw_jeita->cc = 0;
			sw_jeita->cv = info->data.jeita_temp_below_t0_cv - info->diff_fv_val;	//<-10
			break;
		case -9 ... 0:
			sw_jeita->cv = info->data.jeita_temp_below_t0_cv - info->diff_fv_val;
			sw_jeita->cc = info->data.jeita_temp_below_t0_cc - info->diff_fv_val;
			break;

		case 1 ... 5:
			sw_jeita->cv = info->data.jeita_temp_t0_to_t1_cv - info->diff_fv_val;
			sw_jeita->cc = info->data.jeita_temp_t0_to_t1_cc - info->diff_fv_val;
			break;

		case 6 ... 9:
			sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv - info->diff_fv_val;
			sw_jeita->cc = info->data.jeita_temp_t1_to_t2_cc - info->diff_fv_val;
			break;

		case 10 ... 14:
			sw_jeita->cv = info->data.jeita_temp_t2_to_t3_cv - info->diff_fv_val;
			sw_jeita->cc = info->data.jeita_temp_t2_to_t3_cc - info->diff_fv_val;
			break;

		case 15 ... 34:
			sw_jeita->cv = info->data.jeita_temp_t3_to_t4_cv - info->diff_fv_val;
			sw_jeita->cc = info->data.jeita_temp_t3_to_t4_cc - info->diff_fv_val;
			break;

		case 35 ... 47:
			sw_jeita->cv = info->data.jeita_temp_t4_to_t5_cv - info->diff_fv_val;
			sw_jeita->cc = info->data.jeita_temp_t4_to_t5_cc - info->diff_fv_val;
			break;

		case 48 ... 57:
			xm_ieoc = 250000;
			if (vbat_jeita >= 4100) {
				sw_jeita->charging = false;
				sw_jeita->cc = 0;
			}
/*N17 code for HQHW-43339 by tongjiacheng at 2023/6/30 start*/
			sw_jeita->sm = BAT_TEMP_HIGH;
/*N17 code for HQHW-43339 by tongjiacheng at 2023/6/30 end*/
			break;

		case 58 ... 200:
			sw_jeita->charging = false;
			sw_jeita->cc = 0;
			break;
		default:
			chr_err
			    ("[SW_JEITA] The battery temperature is not within the range (%d) !!\n",
			     info->battery_temp);
		}

	}

	/*N17 code for HQHW-4744 by hankang at 2023/8/8 start*/
	if (xm_ieoc == 250000 && ibat_jeita <= 250 && info->battery_temp < 0) {
		if (vbat_jeita > (sw_jeita->cv / 1000 - 10))
			xm_ieoc += 50000;
	} else if (xm_ieoc == 250000 && ibat_jeita <= 270 && info->battery_temp >= 0) {
		if (vbat_jeita > (sw_jeita->cv / 1000 - 10))
			xm_ieoc += 50000;
	}else if (xm_ieoc == 300000 && ibat_jeita <= 320) {
		if (vbat_jeita > (sw_jeita->cv / 1000 - 10))
			xm_ieoc += 60000;
	} else if (xm_ieoc == 750000 && ibat_jeita <= 800) {
		if (vbat_jeita > (sw_jeita->cv / 1000 - 10))
			xm_ieoc += 100000;
	} else if (xm_ieoc == 850000 && ibat_jeita <= 950) {

	} else {
		if (!chg_done)
			xm_ieoc = 100000;
	}

	charger_dev_set_eoc_current(info->chg1_dev, xm_ieoc);
	if (!chg_done && xm_ieoc != 100000) {
		chr_err("charge done,wake up charge");
		_wake_up_charger(info);
	}
	/*N17 code for HQHW-4744 by hankang at 2023/8/8 enf*/
/*N17 code for HQ-299665 by miaozhichao at 2023/6/14 end*/
	chr_err
	    ("[SW_JEITA]tmp:%d cv:%d cc:%d ibat_jeita:%d vbat_jeita:%d xm_ieoc:%d pd_type:%d diff_fv_val:%d chg_done:%d jeita_chg_en:%d\n",
	     info->battery_temp, sw_jeita->cv, sw_jeita->cc, ibat_jeita,
	     vbat_jeita, xm_ieoc, info->pd_type, info->diff_fv_val, chg_done, sw_jeita->charging);
}
