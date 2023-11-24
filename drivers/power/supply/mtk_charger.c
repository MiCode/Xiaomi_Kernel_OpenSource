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
/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 start*/
#include "mtk_pe5.h"
/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 end*/
/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 start*/
#include <tcpm.h>
/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 end*/

static struct mtk_charger *pinfo = NULL;
struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};
/* N17 code for HQ-292280 by tongjiacheng at 20230610 start */

/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 start*/
SRCU_NOTIFIER_HEAD(charger_notifier);
EXPORT_SYMBOL_GPL(charger_notifier);

int charger_reg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(charger_reg_notifier);

int charger_unreg_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(charger_unreg_notifier);

int charger_notifier_call_cnain(unsigned long event, int val)
{
	return srcu_notifier_call_chain(&charger_notifier, event, &val);
}
EXPORT_SYMBOL_GPL(charger_notifier_call_cnain);
/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 end*/

/* N17 code for HQ-319580 by p-xuyechen at 2023/08/21 */
bool is_disable_charger(struct mtk_charger *info);

int charger_manager_get_thermal_level(void)
{
	struct power_supply *psy;
	static struct mtk_charger *info;

	if (info == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if  (psy == NULL)
			return -PTR_ERR(psy);
		else {
			info = (struct mtk_charger *)power_supply_get_drvdata(psy);
			return info->thermal_level;
		}
	}
	else
		return info->thermal_level;
}
EXPORT_SYMBOL(charger_manager_get_thermal_level);

/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
static void monitor_smart_chg(struct mtk_charger *info)
{
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 start*/
	struct mtk_battery *gm = NULL;
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 end*/
/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 start*/
	struct chg_alg_device *alg = NULL;
	struct pe50_algo_info *pinfo = NULL;
/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 end*/
	struct power_supply *psy = NULL;
/*N17 code for HQ-308735 by xm tianye9 at 2023/07/25 start*/
	int g_ui_soc = 50;
	static int g_smart_soc = 50;
/*N17 code for HQ-308735 by xm tianye9 at 2023/07/25 end*/

/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 start*/
	alg = get_chg_alg_by_name("pe5");

	if (alg == NULL){
		chr_err("get pe5 fail\n");
		return;
	}
/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 end*/
	psy = power_supply_get_by_name("battery");
	if (psy != NULL) {
		gm = (struct mtk_battery *)power_supply_get_drvdata(psy);
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 start*/
		if (gm != NULL){
			chr_err("N17 get mtk_battery drvdata success in %s\n", __func__);
		}
	} else
		return;

/*N17 code for HQ-308735 by xm tianye9 at 2023/07/25 start*/
	g_ui_soc = get_uisoc(info);
/*N17 code for HQ-308735 by xm tianye9 at 2023/07/25 end*/

/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 start*/
	pinfo = chg_alg_dev_get_drvdata(alg);
/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 end*/

/*N17 code for HQ-308735 by xm tianye9 at 2023/07/25 start*/
	chr_err("N17 call monitor_smart_chg, en_ret = %d, fun_val = %d, active_status = %d, smart_soc = %d, ui_soc =%d\n",
		gm->smart_charge[SMART_CHG_NAVIGATION].en_ret,
		gm->smart_charge[SMART_CHG_NAVIGATION].func_val,
		gm->smart_charge[SMART_CHG_NAVIGATION].active_status,
		g_smart_soc,
		g_ui_soc);
/*N17 code for HQ-308735 by xm tianye9 at 2023/07/25 end*/
/*N17 code for HQ-308735 by xm tianye9 at 2023/07/25 start*/
/*N17 code for HQ-309838 by xm tianye9 at 2023/08/02 start*/
	if(gm->smart_charge[SMART_CHG_NAVIGATION].en_ret && g_ui_soc >= gm->smart_charge[SMART_CHG_NAVIGATION].func_val && !gm->smart_charge[SMART_CHG_NAVIGATION].active_status)
	{
		gm->smart_charge[SMART_CHG_NAVIGATION].active_status = true;
		g_smart_soc = gm->smart_charge[SMART_CHG_NAVIGATION].func_val;
		charger_dev_enable(info->chg1_dev, false);
		chr_err("N17 monitor disable charger,soc >= %d\n", gm->smart_charge[SMART_CHG_NAVIGATION].func_val);
	}
/*N17 code for HQ-319856 by xm tianye9 at 2023/08/21 start*/
	else if((((!gm->smart_charge[SMART_CHG_NAVIGATION].en_ret || g_ui_soc <= gm->smart_charge[SMART_CHG_NAVIGATION].func_val - 5) && gm->smart_charge[SMART_CHG_NAVIGATION].active_status) ||
		(!gm->smart_charge[SMART_CHG_NAVIGATION].en_ret && pinfo->cp_stop_flag && !gm->smart_charge[SMART_CHG_NAVIGATION].active_status)) &&
		/* N17 code for HQ-319580 by p-xuyechen at 2023/08/21 */
		(is_disable_charger(info) != true)
		/*N17 code for HQ-314179 by hankang at 2023/08/31*/
		&& info->sw_jeita.charging == true)
	{
/*N17 code for HQ-319856 by xm tianye9 at 2023/08/21 end*/
/*N17 code for HQ-308735 by xm tianye9 at 2023/07/25 end*/
		gm->smart_charge[SMART_CHG_NAVIGATION].active_status = false;
		charger_dev_enable(info->chg1_dev, true);
/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 start*/
		if (pinfo->cp_stop_flag){
			chg_alg_cp_statemachine_restart(alg, false);
			chg_alg_start_algo(alg);
			chr_err("n17:wake up cp state machine\n");
		}
/*N17 code for HQ-308229 by xm tianye9 at 2023/07/20 end*/
		chr_err("N17 monitor enable charger, soc <= %d\n", gm->smart_charge[SMART_CHG_NAVIGATION].func_val - 5);
/*N17 code for HQ-309838 by xm tianye9 at 2023/08/02 end*/
/*N17 code for HQ-306722 by xm tianye9 at 2023/07/08 end*/
	}

/*N17 code for low_fast by xm liluting at 2023/07/07 start*/
        if(gm->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
                chr_err("N17 set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, enable low_fast\n", gm->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
	else if(!gm->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
		gm->smart_charge[SMART_CHG_LOW_FAST].active_status = false;
                chr_err("N17 set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, enable low_fast\n", gm->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
/*N17 code for low_fast by xm liluting at 2023/07/07 end*/
}
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/

void charger_manager_set_thermal_level(int level)
{
	struct power_supply *psy;
	static struct mtk_charger *info;

	if (info == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if  (psy == NULL)
			return;
		else
			info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	}
/* N17 code for HQ-292280 by tongjiacheng at 20230613 start */
	chr_err("curr thermal level:%d, new level:%d, pe5 state:%s\n",
			info->thermal_level, level,
			chg_alg_is_algo_running(info->alg[0]) ? "running" : "stop");
/* N17 code for HQ-292280 by tongjiacheng at 20230613 end */
	if (level != info->thermal_level) {
		info->thermal_level = level;
		_wake_up_charger(info);			//wake up charger
	}
}
EXPORT_SYMBOL(charger_manager_set_thermal_level);
/* N17 code for HQ-292280 by tongjiacheng at 20230610 end */
#ifdef MODULE
static char __chg_cmdline[COMMAND_LINE_SIZE];
static char *chg_cmdline = __chg_cmdline;
/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 start*/
static void usbpd_mi_vdm_received_cb(struct mtk_charger *pinfo,
				     struct tcp_ny_uvdm uvdm);
/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 end*/
const char *chg_get_cmd(void)
{
	struct device_node *of_chosen = NULL;
	char *bootargs = NULL;

	if (__chg_cmdline[0] != 0)
		return chg_cmdline;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs =
		    (char *) of_get_property(of_chosen, "bootargs", NULL);
		if (!bootargs)
			chr_err("%s: failed to get bootargs\n", __func__);
		else {
			strncpy(__chg_cmdline, bootargs, 100);
			chr_err("%s: bootargs: %s\n", __func__, bootargs);
		}
	} else
		chr_err("%s: failed to get /chosen \n", __func__);

	return chg_cmdline;
}

#else
const char *chg_get_cmd(void)
{
	return saved_command_line;
}
#endif

int chr_get_debug_level(void)
{
	struct power_supply *psy;
	static struct mtk_charger *info;
	int ret;

	if (info == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL)
			ret = CHRLOG_DEBUG_LEVEL;
		else {
			info =
			    (struct mtk_charger *)
			    power_supply_get_drvdata(psy);
			if (info == NULL)
				ret = CHRLOG_DEBUG_LEVEL;
			else
				ret = info->log_level;
		}
	} else
		ret = info->log_level;

	return ret;
}

EXPORT_SYMBOL(chr_get_debug_level);

void _wake_up_charger(struct mtk_charger *info)
{
	unsigned long flags;

	if (info == NULL)
		return;
	info->timer_cb_duration[2] = ktime_get_boottime();
	spin_lock_irqsave(&info->slock, flags);
	info->timer_cb_duration[3] = ktime_get_boottime();
	if (!info->charger_wakelock->active)
		__pm_stay_awake(info->charger_wakelock);
	info->timer_cb_duration[4] = ktime_get_boottime();
	spin_unlock_irqrestore(&info->slock, flags);
	info->charger_thread_timeout = true;
	info->timer_cb_duration[5] = ktime_get_boottime();
	wake_up_interruptible(&info->wait_que);
}
EXPORT_SYMBOL(_wake_up_charger);

bool is_disable_charger(struct mtk_charger *info)
{
	if (info == NULL)
		return true;

	if (info->disable_charger == true || IS_ENABLED(CONFIG_POWER_EXT))
		return true;
	else
		return false;
}

int _mtk_enable_charging(struct mtk_charger *info, bool en)
{
	chr_debug("%s en:%d\n", __func__, en);
	if (info->algo.enable_charging != NULL)
		return info->algo.enable_charging(info, en);
	return false;
}

int mtk_charger_notifier(struct mtk_charger *info, int event)
{
	return srcu_notifier_call_chain(&info->evt_nh, event, NULL);
}

static void mtk_charger_parse_dt(struct mtk_charger *info,
				 struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val = 0;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;
        int ret = 0;
        int i = 0;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		chr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *) of_get_property(boot_node,
							      "atag,boot",
							      NULL);
		if (!tag)
			chr_err("%s: failed to get atag,boot\n", __func__);
		else {
			chr_err
			    ("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
			     __func__, tag->size, tag->tag, tag->bootmode,
			     tag->boottype);
			info->bootmode = tag->bootmode;
			info->boottype = tag->boottype;
		}
	}

	if (of_property_read_string(np, "algorithm_name",
		&info->algorithm_name) < 0) {
		chr_err("%s: no algorithm_name name\n", __func__);
		info->algorithm_name = "Basic";
	}

	if (strcmp(info->algorithm_name, "Basic") == 0) {
		chr_err("found Basic\n");
		mtk_basic_charger_init(info);
	} else if (strcmp(info->algorithm_name, "Pulse") == 0) {
		chr_err("found Pulse\n");
		mtk_pulse_charger_init(info);
	}

	info->disable_charger =
	    of_property_read_bool(np, "disable_charger");
	info->charger_unlimited =
	    of_property_read_bool(np, "charger_unlimited");
	info->atm_enabled = of_property_read_bool(np, "atm_is_enabled");
	info->enable_sw_safety_timer =
	    of_property_read_bool(np, "enable_sw_safety_timer");
	info->sw_safety_timer_setting = info->enable_sw_safety_timer;
	info->disable_aicl = of_property_read_bool(np, "disable_aicl");

	/* common */

	if (of_property_read_u32(np, "charger_configuration", &val) >= 0)
		info->config = val;
	else {
		chr_err("use default charger_configuration:%d\n",
			SINGLE_CHARGER);
		info->config = SINGLE_CHARGER;
	}

	if (of_property_read_u32(np, "battery_cv", &val) >= 0)
		info->data.battery_cv = val;
	else {
		chr_err("use default BATTERY_CV:%d\n", BATTERY_CV);
		info->data.battery_cv = BATTERY_CV;
	}

	if (of_property_read_u32(np, "max_charger_voltage", &val) >= 0)
		info->data.max_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MAX:%d\n", V_CHARGER_MAX);
		info->data.max_charger_voltage = V_CHARGER_MAX;
	}
	info->data.max_charger_voltage_setting =
	    info->data.max_charger_voltage;

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		info->data.min_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		info->data.min_charger_voltage = V_CHARGER_MIN;
	}
	info->enable_vbat_mon =
	    of_property_read_bool(np, "enable_vbat_mon");
	if (info->enable_vbat_mon == true)
		info->setting.vbat_mon_en = true;
	chr_err("use 6pin bat, enable_vbat_mon:%d\n",
		info->enable_vbat_mon);
	info->enable_vbat_mon_bak =
	    of_property_read_bool(np, "enable_vbat_mon");
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 start*/
	/* sw jeita */
	info->enable_sw_jeita =
	    of_property_read_bool(np, "enable_sw_jeita");
	if (of_property_read_u32(np, "jeita_temp_above_t6_cv", &val) >= 0)
		info->data.jeita_temp_above_t6_cv = val;
	else {
		chr_err("use default JEITA_TEMP_ABOVE_T6_CV:%d\n",
			JEITA_TEMP_ABOVE_T6_CV);
		info->data.jeita_temp_above_t6_cv = JEITA_TEMP_ABOVE_T6_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t5_to_t6_cv", &val) >= 0)
		info->data.jeita_temp_t5_to_t6_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T5_TO_T6_CV:%d\n",
			JEITA_TEMP_T5_TO_T6_CV);
		info->data.jeita_temp_t5_to_t6_cv = JEITA_TEMP_T5_TO_T6_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t4_to_t5_cv", &val) >= 0)
		info->data.jeita_temp_t4_to_t5_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T4_TO_T5_CV:%d\n",
			JEITA_TEMP_T4_TO_T5_CV);
		info->data.jeita_temp_t4_to_t5_cv = JEITA_TEMP_T4_TO_T5_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CV:%d\n",
			JEITA_TEMP_T3_TO_T4_CV);
		info->data.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CV:%d\n",
			JEITA_TEMP_T2_TO_T3_CV);
		info->data.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cv", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CV:%d\n",
			JEITA_TEMP_T1_TO_T2_CV);
		info->data.jeita_temp_t1_to_t2_cv = JEITA_TEMP_T1_TO_T2_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cv = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CV:%d\n",
			JEITA_TEMP_T0_TO_T1_CV);
		info->data.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		info->data.jeita_temp_below_t0_cv = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CV:%d\n",
			JEITA_TEMP_BELOW_T0_CV);
		info->data.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t5_to_t6_cc", &val) >= 0)
		info->data.jeita_temp_t5_to_t6_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T5_TO_T6_CC:%d\n",
			JEITA_TEMP_T5_TO_T6_CC);
		info->data.jeita_temp_t5_to_t6_cc = JEITA_TEMP_T5_TO_T6_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t4_to_t5_cc", &val) >= 0)
		info->data.jeita_temp_t4_to_t5_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T4_TO_T5_CC:%d\n",
			JEITA_TEMP_T4_TO_T5_CC);
		info->data.jeita_temp_t4_to_t5_cc = JEITA_TEMP_T4_TO_T5_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cc", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T3_TO_T4_CC:%d\n",
			JEITA_TEMP_T3_TO_T4_CC);
		info->data.jeita_temp_t3_to_t4_cc = JEITA_TEMP_T3_TO_T4_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cc", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T2_TO_T3_CC:%d\n",
			JEITA_TEMP_T2_TO_T3_CC);
		info->data.jeita_temp_t2_to_t3_cc = JEITA_TEMP_T2_TO_T3_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cc", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T1_TO_T2_CC:%d\n",
			JEITA_TEMP_T1_TO_T2_CC);
		info->data.jeita_temp_t1_to_t2_cc = JEITA_TEMP_T1_TO_T2_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cc", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cc = val;
	else {
		chr_err("use default JEITA_TEMP_T0_TO_T1_CC:%d\n",
			JEITA_TEMP_T0_TO_T1_CC);
		info->data.jeita_temp_t0_to_t1_cc = JEITA_TEMP_T0_TO_T1_CC;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cc", &val) >= 0)
		info->data.jeita_temp_below_t0_cc = val;
	else {
		chr_err("use default JEITA_TEMP_BELOW_T0_CC:%d\n",
			JEITA_TEMP_BELOW_T0_CC);
		info->data.jeita_temp_below_t0_cc = JEITA_TEMP_BELOW_T0_CC;
	}
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 end*/
	/* battery temperature protection */
	info->thermal.sm = BAT_TEMP_NORMAL;
	info->thermal.enable_min_charge_temp =
	    of_property_read_bool(np, "enable_min_charge_temp");

	if (of_property_read_u32(np, "min_charge_temp", &val) >= 0)
		info->thermal.min_charge_temp = val;
	else {
		chr_err("use default MIN_CHARGE_TEMP:%d\n",
			MIN_CHARGE_TEMP);
		info->thermal.min_charge_temp = MIN_CHARGE_TEMP;
	}

	if (of_property_read_u32(np, "min_charge_temp_plus_x_degree", &val)
	    >= 0) {
		info->thermal.min_charge_temp_plus_x_degree = val;
	} else {
		chr_err("use default MIN_CHARGE_TEMP_PLUS_X_DEGREE:%d\n",
			MIN_CHARGE_TEMP_PLUS_X_DEGREE);
		info->thermal.min_charge_temp_plus_x_degree =
		    MIN_CHARGE_TEMP_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "max_charge_temp", &val) >= 0)
		info->thermal.max_charge_temp = val;
	else {
		chr_err("use default MAX_CHARGE_TEMP:%d\n",
			MAX_CHARGE_TEMP);
		info->thermal.max_charge_temp = MAX_CHARGE_TEMP;
	}

	if (of_property_read_u32
	    (np, "max_charge_temp_minus_x_degree", &val)
	    >= 0) {
		info->thermal.max_charge_temp_minus_x_degree = val;
	} else {
		chr_err("use default MAX_CHARGE_TEMP_MINUS_X_DEGREE:%d\n",
			MAX_CHARGE_TEMP_MINUS_X_DEGREE);
		info->thermal.max_charge_temp_minus_x_degree =
		    MAX_CHARGE_TEMP_MINUS_X_DEGREE;
	}

	/* charging current */
	if (of_property_read_u32(np, "usb_charger_current", &val) >= 0) {
		info->data.usb_charger_current = val;
	} else {
		chr_err("use default USB_CHARGER_CURRENT:%d\n",
			USB_CHARGER_CURRENT);
		info->data.usb_charger_current = USB_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_current", &val) >= 0) {
		info->data.ac_charger_current = val;
	} else {
		chr_err("use default AC_CHARGER_CURRENT:%d\n",
			AC_CHARGER_CURRENT);
		info->data.ac_charger_current = AC_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "ac_charger_input_current", &val) >=
	    0)
		info->data.ac_charger_input_current = val;
	else {
		chr_err("use default AC_CHARGER_INPUT_CURRENT:%d\n",
			AC_CHARGER_INPUT_CURRENT);
		info->data.ac_charger_input_current =
		    AC_CHARGER_INPUT_CURRENT;
	}
/*N17 code for HQ-290909 by miaozhichao at 2023/5/25 start*/
	if (of_property_read_u32(np, "non_std_ac_charger_current", &val) >=
	    0)
		info->data.non_std_ac_charger_current = val;
	else {
		chr_err("use default non_std_ac_charger_current:%d\n",
			NON_STD_AC_CHARGER_CURRENT);
		info->data.non_std_ac_charger_current =
		    NON_STD_AC_CHARGER_CURRENT;
	}
/*N17 code for HQ-290909 by miaozhichao at 2023/5/25 end*/
	if (of_property_read_u32(np, "charging_host_charger_current", &val)
	    >= 0) {
		info->data.charging_host_charger_current = val;
	} else {
		chr_err("use default CHARGING_HOST_CHARGER_CURRENT:%d\n",
			CHARGING_HOST_CHARGER_CURRENT);
		info->data.charging_host_charger_current =
		    CHARGING_HOST_CHARGER_CURRENT;
	}

	/* dynamic mivr */
	info->enable_dynamic_mivr =
	    of_property_read_bool(np, "enable_dynamic_mivr");

	if (of_property_read_u32(np, "min_charger_voltage_1", &val) >= 0)
		info->data.min_charger_voltage_1 = val;
	else {
		chr_err("use default V_CHARGER_MIN_1: %d\n",
			V_CHARGER_MIN_1);
		info->data.min_charger_voltage_1 = V_CHARGER_MIN_1;
	}

	if (of_property_read_u32(np, "min_charger_voltage_2", &val) >= 0)
		info->data.min_charger_voltage_2 = val;
	else {
		chr_err("use default V_CHARGER_MIN_2: %d\n",
			V_CHARGER_MIN_2);
		info->data.min_charger_voltage_2 = V_CHARGER_MIN_2;
	}

	if (of_property_read_u32(np, "max_dmivr_charger_current", &val) >=
	    0)
		info->data.max_dmivr_charger_current = val;
	else {
		chr_err("use default MAX_DMIVR_CHARGER_CURRENT: %d\n",
			MAX_DMIVR_CHARGER_CURRENT);
		info->data.max_dmivr_charger_current =
		    MAX_DMIVR_CHARGER_CURRENT;
	}

/*N17 code for dropfv by liluting at 2023/7/3 start*/
        ret = of_property_read_u32_array(np, "cyclecount",
                                                info->data.cyclecount,
                                                CYCLE_COUNT_MAX);
        if(ret){
                chr_err("use default CYCLE_COUNT: 0\n");
        for(i = 0; i < CYCLE_COUNT_MAX; i++)
                info->data.cyclecount[i] = 0;
        }

        ret = of_property_read_u32_array(np, "dropfv_ffc",
                                                info->data.dropfv_ffc,
                                                CYCLE_COUNT_MAX);
        if(ret){
                chr_err("use default DROP_FV: 0\n");
        for(i = 0; i < CYCLE_COUNT_MAX; i++)
                info->data.dropfv_ffc[i] = 0;
        }

        ret = of_property_read_u32_array(np, "dropfv_noffc",
                                                info->data.dropfv_noffc,
                                                CYCLE_COUNT_MAX);
        if(ret){
                chr_err("use default DROP_FV: 0\n");
        for(i = 0; i < CYCLE_COUNT_MAX; i++)
                info->data.dropfv_noffc[i] = 0;
        }
/*N17 code for dropfv by liluting at 2023/7/3 end*/

	/* fast charging algo support indicator */
	info->enable_fast_charging_indicator =
	    of_property_read_bool(np, "enable_fast_charging_indicator");
}

static void mtk_charger_start_timer(struct mtk_charger *info)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	/* If the timer was already set, cancel it */
	ret = alarm_try_to_cancel(&info->charger_timer);
	if (ret < 0) {
		chr_err("%s: callback was running, skip timer\n",
			__func__);
		return;
	}

	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);
	end_time.tv_sec = time_now.tv_sec + info->polling_interval;
	end_time.tv_nsec = time_now.tv_nsec + 0;
	info->endtime = end_time;
	ktime = ktime_set(info->endtime.tv_sec, info->endtime.tv_nsec);

	chr_err("%s: alarm timer start:%d, %ld %ld\n", __func__, ret,
		info->endtime.tv_sec, info->endtime.tv_nsec);
	alarm_start(&info->charger_timer, ktime);
}

static void check_battery_exist(struct mtk_charger *info)
{
	unsigned int i = 0;
	int count = 0;
	//int boot_mode = get_boot_mode();

	if (is_disable_charger(info))
		return;

	for (i = 0; i < 3; i++) {
		if (is_battery_exist(info) == false)
			count++;
	}

#ifdef FIXME
	if (count >= 3) {
		if (boot_mode == META_BOOT || boot_mode == ADVMETA_BOOT ||
		    boot_mode == ATE_FACTORY_BOOT)
			chr_info("boot_mode = %d, bypass battery check\n",
				 boot_mode);
		else {
			chr_err("battery doesn't exist, shutdown\n");
			orderly_poweroff(true);
		}
	}
#endif
}

static void check_dynamic_mivr(struct mtk_charger *info)
{
	int i = 0, ret = 0;
	int vbat = 0;
	bool is_fast_charge = false;
	struct chg_alg_device *alg = NULL;

	if (!info->enable_dynamic_mivr)
		return;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_fast_charge = true;
			break;
		}
	}

	if (!is_fast_charge) {
		vbat = get_battery_voltage(info);
		if (vbat < info->data.min_charger_voltage_2 / 1000 - 200)
			charger_dev_set_mivr(info->chg1_dev,
					     info->data.
					     min_charger_voltage_2);
		else if (vbat <
			 info->data.min_charger_voltage_1 / 1000 - 200)
			charger_dev_set_mivr(info->chg1_dev,
					     info->data.
					     min_charger_voltage_1);
		else
			charger_dev_set_mivr(info->chg1_dev,
					     info->data.
					     min_charger_voltage);
	}
}


static int mtk_chgstat_notify(struct mtk_charger *info)
{
	int ret = 0;
	char *env[2] = { "CHGSTAT=1", NULL };

	chr_err("%s: 0x%x\n", __func__, info->notify_code);
	ret = kobject_uevent_env(&info->pdev->dev.kobj, KOBJ_CHANGE, env);
	if (ret)
		chr_err("%s: kobject_uevent_fail, ret=%d", __func__, ret);

	return ret;
}

static void mtk_charger_set_algo_log_level(struct mtk_charger *info,
					   int level)
{
	struct chg_alg_device *alg;
	int i = 0, ret = 0;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		ret = chg_alg_set_prop(alg, ALG_LOG_LEVEL, level);
		if (ret < 0)
			chr_err("%s: set ALG_LOG_LEVEL fail, ret =%d",
				__func__, ret);
	}
}

static ssize_t sw_jeita_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_sw_jeita);
	return sprintf(buf, "%d\n", pinfo->enable_sw_jeita);
}

static ssize_t sw_jeita_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_sw_jeita = false;
		else
			pinfo->enable_sw_jeita = true;

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(sw_jeita);
/* sw jeita end*/

static ssize_t chr_type_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->chr_type);
	return sprintf(buf, "%d\n", pinfo->chr_type);
}

static ssize_t chr_type_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0)
		pinfo->chr_type = temp;
	else
		chr_err("%s: format error!\n", __func__);

	return size;
}

static DEVICE_ATTR_RW(chr_type);

static ssize_t Pump_Express_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret = 0, i = 0;
	bool is_ta_detected = false;
	struct mtk_charger *pinfo = dev->driver_data;
	struct chg_alg_device *alg = NULL;

	if (!pinfo) {
		chr_err("%s: pinfo is null\n", __func__);
		return sprintf(buf, "%d\n", is_ta_detected);
	}

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = pinfo->alg[i];
		if (alg == NULL)
			continue;
		ret = chg_alg_is_algo_ready(alg);
		if (ret == ALG_RUNNING) {
			is_ta_detected = true;
			break;
		}
	}
	chr_err("%s: idx = %d, detect = %d\n", __func__, i,
		is_ta_detected);
	return sprintf(buf, "%d\n", is_ta_detected);
}

static DEVICE_ATTR_RO(Pump_Express);

static ssize_t fast_chg_indicator_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->fast_charging_indicator);
	return sprintf(buf, "%d\n", pinfo->fast_charging_indicator);
}

static ssize_t fast_chg_indicator_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->fast_charging_indicator = temp;
	else
		chr_err("%s: format error!\n", __func__);

	if (pinfo->fast_charging_indicator > 0) {
		pinfo->log_level = CHRLOG_DEBUG_LEVEL;
		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(fast_chg_indicator);

static ssize_t enable_meta_current_limit_show(struct device *dev,
					      struct device_attribute
					      *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->enable_meta_current_limit);
	return sprintf(buf, "%d\n", pinfo->enable_meta_current_limit);
}

static ssize_t enable_meta_current_limit_store(struct device *dev,
					       struct device_attribute
					       *attr, const char *buf,
					       size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0)
		pinfo->enable_meta_current_limit = temp;
	else
		chr_err("%s: format error!\n", __func__);

	if (pinfo->enable_meta_current_limit > 0) {
		pinfo->log_level = CHRLOG_DEBUG_LEVEL;
		mtk_charger_set_algo_log_level(pinfo, pinfo->log_level);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(enable_meta_current_limit);

static ssize_t vbat_mon_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->enable_vbat_mon);
	return sprintf(buf, "%d\n", pinfo->enable_vbat_mon);
}

static ssize_t vbat_mon_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int temp;

	if (kstrtouint(buf, 10, &temp) == 0) {
		if (temp == 0)
			pinfo->enable_vbat_mon = false;
		else
			pinfo->enable_vbat_mon = true;
	} else {
		chr_err("%s: format error!\n", __func__);
	}

	_wake_up_charger(pinfo);
	return size;
}

static DEVICE_ATTR_RW(vbat_mon);

static ssize_t ADC_Charger_Voltage_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int vbus = get_vbus(pinfo);	/* mV */

	chr_err("%s: %d\n", __func__, vbus);
	return sprintf(buf, "%d\n", vbus);
}

static DEVICE_ATTR_RO(ADC_Charger_Voltage);

static ssize_t ADC_Charging_Current_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	//N17 HQHW-4750 by p-xiepengfu at 20230804 star
	int ibat = get_ibat(pinfo);	/* mA */
	//N17 HQHW-4750 by p-xiepengfu at 20230804 end
	chr_err("%s: %d\n", __func__, ibat);
	return sprintf(buf, "%d\n", ibat);
}

static DEVICE_ATTR_RO(ADC_Charging_Current);

static ssize_t input_current_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int aicr = 0;

	aicr = pinfo->chg_data[CHG1_SETTING].thermal_input_current_limit;
	chr_err("%s: %d\n", __func__, aicr);
	return sprintf(buf, "%d\n", aicr);
}

static ssize_t input_current_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	struct charger_data *chg_data;
	signed int temp;

	chg_data = &pinfo->chg_data[CHG1_SETTING];
	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0)
			chg_data->thermal_input_current_limit = 0;
		else
			chg_data->thermal_input_current_limit = temp;
	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(input_current);

static ssize_t charger_log_level_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->log_level);
	return sprintf(buf, "%d\n", pinfo->log_level);
}

static ssize_t charger_log_level_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0) {
			chr_err("%s: val is invalid: %d\n", __func__,
				temp);
			temp = 0;
		}
		pinfo->log_level = temp;
		chr_err("%s: log_level=%d\n", __func__, pinfo->log_level);

	} else {
		chr_err("%s: format error!\n", __func__);
	}
	return size;
}

static DEVICE_ATTR_RW(charger_log_level);

static ssize_t BatteryNotify_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_info("%s: 0x%x\n", __func__, pinfo->notify_code);

	return sprintf(buf, "%u\n", pinfo->notify_code);
}

static ssize_t BatteryNotify_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
	unsigned int reg = 0;
	int ret = 0;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 16, &reg);
		if (ret < 0) {
			chr_err("%s: failed, ret = %d\n", __func__, ret);
			return ret;
		}
		pinfo->notify_code = reg;
		chr_info("%s: store code=0x%x\n", __func__,
			 pinfo->notify_code);
		mtk_chgstat_notify(pinfo);
	}
	return size;
}

static DEVICE_ATTR_RW(BatteryNotify);

/* procfs */
static int mtk_chg_set_cv_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;

	seq_printf(m, "%d\n", pinfo->data.battery_cv);
	return 0;
}

static int mtk_chg_set_cv_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_set_cv_show, PDE_DATA(node));
}

static ssize_t mtk_chg_set_cv_write(struct file *file,
				    const char *buffer, size_t count,
				    loff_t * data)
{
	int len = 0, ret = 0;
	char desc[32] = { 0 };
	unsigned int cv = 0;
	struct mtk_charger *info = PDE_DATA(file_inode(file));
	struct power_supply *psy = NULL;
	union power_supply_propval dynamic_cv;

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &cv);
	if (ret == 0) {
		if (cv >= BATTERY_CV) {
			info->data.battery_cv = BATTERY_CV;
			chr_info
			    ("%s: adjust charge voltage %dV too high, use default cv\n",
			     __func__, cv);
		} else {
			info->data.battery_cv = cv;
			chr_info("%s: adjust charge voltage = %dV\n",
				 __func__, cv);
		}
		psy = power_supply_get_by_name("battery");
		if (!IS_ERR_OR_NULL(psy)) {
			dynamic_cv.intval = info->data.battery_cv;
			ret = power_supply_set_property(psy,
							POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
							&dynamic_cv);
			if (ret < 0)
				chr_err("set gauge cv fail\n");
		}
		return count;
	}

	chr_err("%s: bad argument\n", __func__);
	return count;
}

static const struct proc_ops mtk_chg_set_cv_fops = {
	.proc_open = mtk_chg_set_cv_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_set_cv_write,
};

static int mtk_chg_current_cmd_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;

	seq_printf(m, "%d %d\n", pinfo->usb_unlimited,
		   pinfo->cmd_discharging);
	return 0;
}

static int mtk_chg_current_cmd_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_current_cmd_show, PDE_DATA(node));
}

static ssize_t mtk_chg_current_cmd_write(struct file *file,
					 const char *buffer, size_t count,
					 loff_t * data)
{
	int len = 0;
	char desc[32] = { 0 };
	int current_unlimited = 0;
	int cmd_discharging = 0;
	struct mtk_charger *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &current_unlimited, &cmd_discharging) ==
	    2) {
		info->usb_unlimited = current_unlimited;
		if (cmd_discharging == 1) {
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev,
					     EVENT_DISCHARGE, 0);
		} else if (cmd_discharging == 0) {
			info->cmd_discharging = false;
			charger_dev_enable(info->chg1_dev, true);
			charger_dev_do_event(info->chg1_dev,
					     EVENT_RECHARGE, 0);
		}

		chr_info("%s: current_unlimited=%d, cmd_discharging=%d\n",
			 __func__, current_unlimited, cmd_discharging);
		return count;
	}

	chr_err
	    ("bad argument, echo [usb_unlimited] [disable] > current_cmd\n");
	return count;
}

static const struct proc_ops mtk_chg_current_cmd_fops = {
	.proc_open = mtk_chg_current_cmd_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_current_cmd_write,
};

static int mtk_chg_en_power_path_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;
	bool power_path_en = true;

	charger_dev_is_powerpath_enabled(pinfo->chg1_dev, &power_path_en);
	seq_printf(m, "%d\n", power_path_en);

	return 0;
}

static int mtk_chg_en_power_path_open(struct inode *node,
				      struct file *file)
{
	return single_open(file, mtk_chg_en_power_path_show,
			   PDE_DATA(node));
}

static ssize_t mtk_chg_en_power_path_write(struct file *file,
					   const char *buffer,
					   size_t count, loff_t * data)
{
	int len = 0, ret = 0;
	char desc[32] = { 0 };
	unsigned int enable = 0;
	struct mtk_charger *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_powerpath(info->chg1_dev, enable);
		chr_info("%s: enable power path = %d\n", __func__, enable);
		return count;
	}

	chr_err("bad argument, echo [enable] > en_power_path\n");
	return count;
}

static const struct proc_ops mtk_chg_en_power_path_fops = {
	.proc_open = mtk_chg_en_power_path_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_en_power_path_write,
};

static int mtk_chg_en_safety_timer_show(struct seq_file *m, void *data)
{
	struct mtk_charger *pinfo = m->private;
	bool safety_timer_en = false;

	charger_dev_is_safety_timer_enabled(pinfo->chg1_dev,
					    &safety_timer_en);
	seq_printf(m, "%d\n", safety_timer_en);

	return 0;
}

static int mtk_chg_en_safety_timer_open(struct inode *node,
					struct file *file)
{
	return single_open(file, mtk_chg_en_safety_timer_show,
			   PDE_DATA(node));
}

static ssize_t mtk_chg_en_safety_timer_write(struct file *file,
					     const char *buffer,
					     size_t count, loff_t * data)
{
	int len = 0, ret = 0;
	char desc[32] = { 0 };
	unsigned int enable = 0;
	struct mtk_charger *info = PDE_DATA(file_inode(file));

	if (!info)
		return -EINVAL;
	if (count <= 0)
		return -EINVAL;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &enable);
	if (ret == 0) {
		charger_dev_enable_safety_timer(info->chg1_dev, enable);
		info->safety_timer_cmd = (int) enable;
		chr_info("%s: enable safety timer = %d\n", __func__,
			 enable);

		/* SW safety timer */
		if (info->sw_safety_timer_setting == true) {
			if (enable)
				info->enable_sw_safety_timer = true;
			else
				info->enable_sw_safety_timer = false;
		}

		return count;
	}

	chr_err("bad argument, echo [enable] > en_safety_timer\n");
	return count;
}

static const struct proc_ops mtk_chg_en_safety_timer_fops = {
	.proc_open = mtk_chg_en_safety_timer_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_en_safety_timer_write,
};

int sc_get_sys_time(void)
{
	struct rtc_time tm_android = { 0 };
	struct timespec64 tv_android = { 0 };
	int timep = 0;

	ktime_get_real_ts64(&tv_android);
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	tv_android.tv_sec -= (uint64_t) sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	timep =
	    tm_android.tm_sec + tm_android.tm_min * 60 +
	    tm_android.tm_hour * 3600;

	return timep;
}

int sc_get_left_time(int s, int e, int now)
{
	if (e >= s) {
		if (now >= s && now < e)
			return e - now;
	} else {
		if (now >= s)
			return 86400 - now + e;
		else if (now < e)
			return e - now;
	}
	return 0;
}

char *sc_solToStr(int s)
{
	switch (s) {
	case SC_IGNORE:
		return "ignore";
	case SC_KEEP:
		return "keep";
	case SC_DISABLE:
		return "disable";
	case SC_REDUCE:
		return "reduce";
	default:
		return "none";
	}
}

void smart_batt_set_diff_fv(int val)
{
	if(pinfo == NULL)
		return;
	chr_err("%s diff_fv_val=%d\n", __func__, val);
	pinfo->diff_fv_val = val;
}
EXPORT_SYMBOL(smart_batt_set_diff_fv);

int smart_batt_get_diff_fv(void)
{
	if(pinfo == NULL)
		return 0;
	chr_err("%s diff_fv_val=%d\n", __func__, pinfo->diff_fv_val);
	return pinfo->diff_fv_val;
}
EXPORT_SYMBOL(smart_batt_get_diff_fv);


int smart_charging(struct mtk_charger *info)
{
	int time_to_target = 0;
	int time_to_full_default_current = -1;
	int time_to_full_default_current_limit = -1;
	int ret_value = SC_KEEP;
	int sc_real_time = sc_get_sys_time();
	int sc_left_time =
	    sc_get_left_time(info->sc.start_time, info->sc.end_time,
			     sc_real_time);
	int sc_battery_percentage = get_uisoc(info) * 100;
	int sc_charger_current = get_battery_current(info);

	time_to_target = sc_left_time - info->sc.left_time_for_cv;

	if (info->sc.enable == false || sc_left_time <= 0
	    || sc_left_time < info->sc.left_time_for_cv
	    || (sc_charger_current <= 0
		&& info->sc.last_solution != SC_DISABLE))
		ret_value = SC_IGNORE;
	else {
		if (sc_battery_percentage >
		    info->sc.target_percentage * 100) {
			if (time_to_target > 0)
				ret_value = SC_DISABLE;
		} else {
			if (sc_charger_current != 0)
				time_to_full_default_current =
				    info->sc.battery_size * 3600 / 10000 *
				    (10000 - sc_battery_percentage)
				    / sc_charger_current;
			else
				time_to_full_default_current =
				    info->sc.battery_size * 3600 / 10000 *
				    (10000 - sc_battery_percentage);
			chr_err("sc1: %d %d %d %d %d\n",
				time_to_full_default_current,
				info->sc.battery_size,
				sc_battery_percentage,
				sc_charger_current,
				info->sc.current_limit);

			if (time_to_full_default_current < time_to_target
			    && info->sc.current_limit != -1
			    && sc_charger_current >
			    info->sc.current_limit) {
				time_to_full_default_current_limit =
				    info->sc.battery_size / 10000 *
				    (10000 - sc_battery_percentage)
				    / info->sc.current_limit;

				chr_err("sc2: %d %d %d %d\n",
					time_to_full_default_current_limit,
					info->sc.battery_size,
					sc_battery_percentage,
					info->sc.current_limit);

				if (time_to_full_default_current_limit <
				    time_to_target
				    && sc_charger_current >
				    info->sc.current_limit)
					ret_value = SC_REDUCE;
			}
		}
	}
	info->sc.last_solution = ret_value;
	if (info->sc.last_solution == SC_DISABLE)
		info->sc.disable_charger = true;
	else
		info->sc.disable_charger = false;
	chr_err("[sc]disable_charger: %d\n", info->sc.disable_charger);
	chr_err
	    ("[sc1]en:%d t:%d,%d,%d,%d t:%d,%d,%d,%d c:%d,%d ibus:%d uisoc: %d,%d s:%d ans:%s\n",
	     info->sc.enable, info->sc.start_time, info->sc.end_time,
	     sc_real_time, sc_left_time, info->sc.left_time_for_cv,
	     time_to_target, time_to_full_default_current,
	     time_to_full_default_current_limit, sc_charger_current,
	     info->sc.current_limit, get_ibus(info), get_uisoc(info),
	     info->sc.target_percentage, info->sc.battery_size,
	     sc_solToStr(info->sc.last_solution));

	return ret_value;
}

void sc_select_charging_current(struct mtk_charger *info,
				struct charger_data *pdata)
{
	if (info->bootmode == 4 || info->bootmode == 1
	    || info->bootmode == 8 || info->bootmode == 9) {
		info->sc.sc_ibat = -1;	/* not normal boot */
		return;
	}
	info->sc.solution = info->sc.last_solution;
	chr_debug("debug: %d, %d, %d\n", info->bootmode,
		  info->sc.disable_in_this_plug, info->sc.solution);
	if (info->sc.disable_in_this_plug == false) {
		chr_debug("sck: %d %d %d %d %d\n",
			  info->sc.pre_ibat,
			  info->sc.sc_ibat,
			  pdata->charging_current_limit,
			  pdata->thermal_charging_current_limit,
			  info->sc.solution);
		if (info->sc.pre_ibat == -1
		    || info->sc.solution == SC_IGNORE
		    || info->sc.solution == SC_DISABLE) {
			info->sc.sc_ibat = -1;
		} else {
			if (info->sc.pre_ibat ==
			    pdata->charging_current_limit
			    && info->sc.solution == SC_REDUCE
			    && ((pdata->charging_current_limit - 100000) >=
				500000)) {
				if (info->sc.sc_ibat == -1)
					info->sc.sc_ibat =
					    pdata->charging_current_limit -
					    100000;

				else {
					if (info->sc.sc_ibat - 100000 >=
					    500000)
						info->sc.sc_ibat =
						    info->sc.sc_ibat -
						    100000;
					else
						info->sc.sc_ibat = 500000;
				}
			}
		}
	}
	info->sc.pre_ibat = pdata->charging_current_limit;

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
		    pdata->charging_current_limit)
			pdata->charging_current_limit =
			    pdata->thermal_charging_current_limit;
		info->sc.disable_in_this_plug = true;
	} else
	    if ((info->sc.solution == SC_REDUCE
		 || info->sc.solution == SC_KEEP)
		&& info->sc.sc_ibat < pdata->charging_current_limit
		&& info->sc.disable_in_this_plug == false) {
		pdata->charging_current_limit = info->sc.sc_ibat;
	}
}

void sc_init(struct smartcharging *sc)
{
	sc->enable = false;
	sc->battery_size = 3000;
	sc->start_time = 0;
	sc->end_time = 80000;
	sc->current_limit = 2000;
	sc->target_percentage = 80;
	sc->left_time_for_cv = 3600;
	sc->pre_ibat = -1;
}

static ssize_t enable_sc_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err("[enable smartcharging] : %d\n", info->sc.enable);

	return sprintf(buf, "%d\n", info->sc.enable);
}

static ssize_t enable_sc_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[enable smartcharging] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val == 0)
			info->sc.enable = false;
		else
			info->sc.enable = true;

		chr_err("[enable smartcharging]enable smartcharging=%d\n",
			info->sc.enable);
	}
	return size;
}

static DEVICE_ATTR_RW(enable_sc);

static ssize_t sc_stime_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err("[smartcharging stime] : %d\n", info->sc.start_time);

	return sprintf(buf, "%d\n", info->sc.start_time);
}

static ssize_t sc_stime_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging stime] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err("[smartcharging stime] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.start_time = (int) val;

		chr_err("[smartcharging stime]enable smartcharging=%d\n",
			info->sc.start_time);
	}
	return size;
}

static DEVICE_ATTR_RW(sc_stime);

static ssize_t sc_etime_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err("[smartcharging etime] : %d\n", info->sc.end_time);

	return sprintf(buf, "%d\n", info->sc.end_time);
}

static ssize_t sc_etime_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging etime] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err("[smartcharging etime] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.end_time = (int) val;

		chr_err("[smartcharging stime]enable smartcharging=%d\n",
			info->sc.end_time);
	}
	return size;
}

static DEVICE_ATTR_RW(sc_etime);

static ssize_t sc_tuisoc_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err("[smartcharging target uisoc] : %d\n",
		info->sc.target_percentage);

	return sprintf(buf, "%d\n", info->sc.target_percentage);
}

static ssize_t sc_tuisoc_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging tuisoc] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err("[smartcharging tuisoc] val is %ld ??\n",
				val);
			val = 0;
		}

		if (val >= 0)
			info->sc.target_percentage = (int) val;

		chr_err("[smartcharging stime]tuisoc=%d\n",
			info->sc.target_percentage);
	}
	return size;
}

static DEVICE_ATTR_RW(sc_tuisoc);

static ssize_t sc_ibat_limit_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err("[smartcharging ibat limit] : %d\n",
		info->sc.current_limit);

	return sprintf(buf, "%d\n", info->sc.current_limit);
}

static ssize_t sc_ibat_limit_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *) power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging ibat limit] buf is %s\n", buf);
		ret = kstrtol(buf, 10, &val);
		if (ret == -ERANGE || ret == -EINVAL)
			return -EINVAL;
		if (val < 0) {
			chr_err
			    ("[smartcharging ibat limit] val is %ld ??\n",
			     (int) val);
			val = 0;
		}

		if (val >= 0)
			info->sc.current_limit = (int) val;

		chr_err("[smartcharging ibat limit]=%d\n",
			info->sc.current_limit);
	}
	return size;
}

static DEVICE_ATTR_RW(sc_ibat_limit);

int mtk_chg_enable_vbus_ovp(bool enable)
{
	static struct mtk_charger *pinfo;
	int ret = 0;
	u32 sw_ovp = 0;
	struct power_supply *psy;

	if (pinfo == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL) {
			chr_err("[%s]psy is not rdy\n", __func__);
			return -1;
		}

		pinfo =
		    (struct mtk_charger *) power_supply_get_drvdata(psy);
		if (pinfo == NULL) {
			chr_err("[%s]mtk_gauge is not rdy\n", __func__);
			return -1;
		}
	}

	if (enable)
		sw_ovp = pinfo->data.max_charger_voltage_setting;
	else
		sw_ovp = 15000000;

	/* Enable/Disable SW OVP status */
	pinfo->data.max_charger_voltage = sw_ovp;

	disable_hw_ovp(pinfo, enable);

	chr_err("[%s] en:%d ovp:%d\n", __func__, enable, sw_ovp);
	return ret;
}

EXPORT_SYMBOL(mtk_chg_enable_vbus_ovp);

/* return false if vbus is over max_charger_voltage */
static bool mtk_chg_check_vbus(struct mtk_charger *info)
{
	int vchr = 0;

	vchr = get_vbus(info) * 1000;	/* uV */
	if (vchr > info->data.max_charger_voltage) {
		chr_err("%s: vbus(%d mV) > %d mV\n", __func__, vchr / 1000,
			info->data.max_charger_voltage / 1000);
		return false;
	}
	return true;
}

static void mtk_battery_notify_VCharger_check(struct mtk_charger *info)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	int vchr = 0;

	vchr = get_vbus(info) * 1000;	/* uV */
	if (vchr < info->data.max_charger_voltage)
		info->notify_code &= ~CHG_VBUS_OV_STATUS;
	else {
		info->notify_code |= CHG_VBUS_OV_STATUS;
		chr_err("[BATTERY] charger_vol(%d mV) > %d mV\n",
			vchr / 1000,
			info->data.max_charger_voltage / 1000);
		mtk_chgstat_notify(info);
	}
#endif
}

static void mtk_battery_notify_VBatTemp_check(struct mtk_charger *info)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)
	if (info->battery_temp >= info->thermal.max_charge_temp) {
		info->notify_code |= CHG_BAT_OT_STATUS;
		chr_err("[BATTERY] bat_temp(%d) out of range(too high)\n",
			info->battery_temp);
		mtk_chgstat_notify(info);
	} else {
		info->notify_code &= ~CHG_BAT_OT_STATUS;
	}

	if (info->enable_sw_jeita == true) {
		if (info->battery_temp < info->data.temp_neg_10_thres) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
	} else {
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
		if (info->battery_temp < info->thermal.min_charge_temp) {
			info->notify_code |= CHG_BAT_LT_STATUS;
			chr_err("bat_temp(%d) out of range(too low)\n",
				info->battery_temp);
			mtk_chgstat_notify(info);
		} else {
			info->notify_code &= ~CHG_BAT_LT_STATUS;
		}
#endif
	}
#endif
}

static void mtk_battery_notify_UI_test(struct mtk_charger *info)
{
	switch (info->notify_test_mode) {
	case 1:
		info->notify_code = CHG_VBUS_OV_STATUS;
		chr_debug("[%s] CASE_0001_VCHARGER\n", __func__);
		break;
	case 2:
		info->notify_code = CHG_BAT_OT_STATUS;
		chr_debug("[%s] CASE_0002_VBATTEMP\n", __func__);
		break;
	case 3:
		info->notify_code = CHG_OC_STATUS;
		chr_debug("[%s] CASE_0003_ICHARGING\n", __func__);
		break;
	case 4:
		info->notify_code = CHG_BAT_OV_STATUS;
		chr_debug("[%s] CASE_0004_VBAT\n", __func__);
		break;
	case 5:
		info->notify_code = CHG_ST_TMO_STATUS;
		chr_debug("[%s] CASE_0005_TOTAL_CHARGINGTIME\n", __func__);
		break;
	case 6:
		info->notify_code = CHG_BAT_LT_STATUS;
		chr_debug("[%s] CASE6: VBATTEMP_LOW\n", __func__);
		break;
	case 7:
		info->notify_code = CHG_TYPEC_WD_STATUS;
		chr_debug("[%s] CASE7: Moisture Detection\n", __func__);
		break;
	default:
		chr_debug("[%s] Unknown BN_TestMode Code: %x\n",
			  __func__, info->notify_test_mode);
	}
	mtk_chgstat_notify(info);
}

static void mtk_battery_notify_check(struct mtk_charger *info)
{
	if (info->notify_test_mode == 0x0000) {
		mtk_battery_notify_VCharger_check(info);
		mtk_battery_notify_VBatTemp_check(info);
	} else {
		mtk_battery_notify_UI_test(info);
	}
}

static void mtk_chg_get_tchg(struct mtk_charger *info)
{
	int ret;
	int tchg_min = -127, tchg_max = -127;
	struct charger_data *pdata;
	bool en = false;

	pdata = &info->chg_data[CHG1_SETTING];
	ret =
	    charger_dev_get_temperature(info->chg1_dev, &tchg_min,
					&tchg_max);
	if (ret < 0) {
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
	} else {
		pdata->junction_temp_min = tchg_min;
		pdata->junction_temp_max = tchg_max;
	}

	if (info->chg2_dev) {
		pdata = &info->chg_data[CHG2_SETTING];
		ret = charger_dev_get_temperature(info->chg2_dev,
						  &tchg_min, &tchg_max);

		if (ret < 0) {
			pdata->junction_temp_min = -127;
			pdata->junction_temp_max = -127;
		} else {
			pdata->junction_temp_min = tchg_min;
			pdata->junction_temp_max = tchg_max;
		}
	}

	if (info->dvchg1_dev) {
		pdata = &info->chg_data[DVCHG1_SETTING];
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
		ret = charger_dev_is_enabled(info->dvchg1_dev, &en);
		if (ret >= 0 && en) {
			ret = charger_dev_get_adc(info->dvchg1_dev,
						  ADC_CHANNEL_TEMP_JC,
						  &tchg_min, &tchg_max);
			if (ret >= 0) {
				pdata->junction_temp_min = tchg_min;
				pdata->junction_temp_max = tchg_max;
			}
		}
	}

	if (info->dvchg2_dev) {
		pdata = &info->chg_data[DVCHG2_SETTING];
		pdata->junction_temp_min = -127;
		pdata->junction_temp_max = -127;
		ret = charger_dev_is_enabled(info->dvchg2_dev, &en);
		if (ret >= 0 && en) {
			ret = charger_dev_get_adc(info->dvchg2_dev,
						  ADC_CHANNEL_TEMP_JC,
						  &tchg_min, &tchg_max);
			if (ret >= 0) {
				pdata->junction_temp_min = tchg_min;
				pdata->junction_temp_max = tchg_max;
			}
		}
	}
}

/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
static void xm_charge_work(struct work_struct *work)
{
	struct mtk_charger *chip = container_of(work, struct mtk_charger, xm_charge_work.work);

	chr_err("N17:check xm_charge_work\n");
	monitor_smart_chg(chip);

	schedule_delayed_work(&chip->xm_charge_work, msecs_to_jiffies(1000));
}
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/

/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 start*/
static void pe_stop_enable_termination_work(struct work_struct *work)
{
	struct mtk_charger *chip = container_of(work, struct mtk_charger, pe_stop_enable_termination_work.work);

	chr_err("pe50 stop and enable main charge termination\n");
	charger_dev_enable_termination(chip->chg1_dev, true);
	/*N17 code for HQ-329243 by yeyinzi at 2023/09/21 start*/
	chip->during_switching = false;
	/*N17 code for HQ-329243 by yeyinzi at 2023/09/21 end*/
}
/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 end*/

static void charger_check_status(struct mtk_charger *info)
{
	bool charging = true;
	bool chg_dev_chgen = true;
	int temperature;
	struct battery_thermal_protection_data *thermal;
	int uisoc = 0;

	if (get_charger_type(info) == POWER_SUPPLY_TYPE_UNKNOWN)
		return;

	temperature = info->battery_temp;
	thermal = &info->thermal;
	uisoc = get_uisoc(info);

	info->setting.vbat_mon_en = true;
	if (info->enable_sw_jeita == true || info->enable_vbat_mon != true
	    || info->batpro_done == true)
		info->setting.vbat_mon_en = false;

	if (info->enable_sw_jeita == true) {
		do_sw_jeita_state_machine(info);
		if (info->sw_jeita.charging == false) {
			charging = false;
			goto stop_charging;
		}
	} else {

		if (thermal->enable_min_charge_temp) {
			if (temperature < thermal->min_charge_temp) {
				chr_err
				    ("Battery Under Temperature or NTC fail %d %d\n",
				     temperature,
				     thermal->min_charge_temp);
				thermal->sm = BAT_TEMP_LOW;
				charging = false;
				goto stop_charging;
			} else if (thermal->sm == BAT_TEMP_LOW) {
				if (temperature >=
				    thermal->
				    min_charge_temp_plus_x_degree) {
					chr_err
					    ("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
					     thermal->min_charge_temp,
					     temperature,
					     thermal->
					     min_charge_temp_plus_x_degree);
					thermal->sm = BAT_TEMP_NORMAL;
				} else {
					charging = false;
					goto stop_charging;
				}
			}
		}

		if (temperature >= thermal->max_charge_temp) {
			chr_err
			    ("Battery over Temperature or NTC fail %d %d\n",
			     temperature, thermal->max_charge_temp);
			thermal->sm = BAT_TEMP_HIGH;
			charging = false;
			goto stop_charging;
		} else if (thermal->sm == BAT_TEMP_HIGH) {
			if (temperature
			    < thermal->max_charge_temp_minus_x_degree) {
				chr_err
				    ("Battery Temperature raise from %d to %d(%d), allow charging!!\n",
				     thermal->max_charge_temp, temperature,
				     thermal->
				     max_charge_temp_minus_x_degree);
				thermal->sm = BAT_TEMP_NORMAL;
			} else {
				charging = false;
				goto stop_charging;
			}
		}
	}

	mtk_chg_get_tchg(info);

	if (!mtk_chg_check_vbus(info)) {
		charging = false;
		goto stop_charging;
	}

	if (info->cmd_discharging)
		charging = false;
	if (info->safety_timeout)
		charging = false;
	if (info->vbusov_stat)
		charging = false;
	if (info->sc.disable_charger == true)
		charging = false;
      stop_charging:
	mtk_battery_notify_check(info);

	if (charging && uisoc < 80 && info->batpro_done == true) {
		info->setting.vbat_mon_en = true;
		info->batpro_done = false;
		info->stop_6pin_re_en = false;
	}

	chr_err
	    ("tmp:%d (jeita:%d sm:%d cv:%d en:%d) (sm:%d) en:%d c:%d s:%d ov:%d sc:%d %d %d saf_cmd:%d bat_mon:%d %d\n",
	     temperature, info->enable_sw_jeita, info->sw_jeita.sm,
	     info->sw_jeita.cv, info->sw_jeita.charging, thermal->sm,
	     charging, info->cmd_discharging, info->safety_timeout,
	     info->vbusov_stat, info->sc.disable_charger,
	     info->can_charging, charging, info->safety_timer_cmd,
	     info->enable_vbat_mon, info->batpro_done);

	charger_dev_is_enabled(info->chg1_dev, &chg_dev_chgen);

	if (charging != info->can_charging)
		_mtk_enable_charging(info, charging);
	else if (charging == false && chg_dev_chgen == true)
		_mtk_enable_charging(info, charging);

	info->can_charging = charging;
}

static bool charger_init_algo(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int idx = 0;

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("%s, Found primary charger\n", __func__);
	else {
		chr_err("%s, *** Error : can't find primary charger ***\n",
			__func__);
		return false;
	}

	alg = get_chg_alg_by_name("pe5");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe5 fail\n");
	else {
		chr_err("get pe5 success\n");
		alg->config = info->config;
		alg->alg_id = PE5_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe4");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe4 fail\n");
	else {
		chr_err("get pe4 success\n");
		alg->config = info->config;
		alg->alg_id = PE4_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pd");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pd fail\n");
	else {
		chr_err("get pd success\n");
		alg->config = info->config;
		alg->alg_id = PDC_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe2");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe2 fail\n");
	else {
		chr_err("get pe2 success\n");
		alg->config = info->config;
		alg->alg_id = PE2_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe fail\n");
	else {
		chr_err("get pe success\n");
		alg->config = info->config;
		alg->alg_id = PE_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}

	chr_err("config is %d\n", info->config);
	if (info->config == DUAL_CHARGERS_IN_SERIES) {
		info->chg2_dev = get_charger_by_name("secondary_chg");
		if (info->chg2_dev)
			chr_err("Found secondary charger\n");
		else {
			chr_err
			    ("*** Error : can't find secondary charger ***\n");
			return false;
		}
	} else if (info->config == DIVIDER_CHARGER ||
		   info->config == DUAL_DIVIDER_CHARGERS) {
		info->dvchg1_dev = get_charger_by_name("primary_dvchg");
		if (info->dvchg1_dev)
			chr_err("Found primary divider charger\n");
		else {
			chr_err
			    ("*** Error : can't find primary divider charger ***\n");
			return false;
		}
		if (info->config == DUAL_DIVIDER_CHARGERS) {
			info->dvchg2_dev =
			    get_charger_by_name("secondary_dvchg");
			if (info->dvchg2_dev)
				chr_err
				    ("Found secondary divider charger\n");
			else {
				chr_err
				    ("*** Error : can't find secondary divider charger ***\n");
				return false;
			}
		}
	}

	chr_err("register chg1 notifier %d %d\n",
		info->chg1_dev != NULL, info->algo.do_event != NULL);
	if (info->chg1_dev != NULL && info->algo.do_event != NULL) {
		chr_err("register chg1 notifier done\n");
		info->chg1_nb.notifier_call = info->algo.do_event;
		register_charger_device_notifier(info->chg1_dev,
						 &info->chg1_nb);
		charger_dev_set_drvdata(info->chg1_dev, info);
	}

	chr_err("register dvchg chg1 notifier %d %d\n",
		info->dvchg1_dev != NULL,
		info->algo.do_dvchg1_event != NULL);
	if (info->dvchg1_dev != NULL && info->algo.do_dvchg1_event != NULL) {
		chr_err("register dvchg chg1 notifier done\n");
		info->dvchg1_nb.notifier_call = info->algo.do_dvchg1_event;
		register_charger_device_notifier(info->dvchg1_dev,
						 &info->dvchg1_nb);
		charger_dev_set_drvdata(info->dvchg1_dev, info);
	}

	chr_err("register dvchg chg2 notifier %d %d\n",
		info->dvchg2_dev != NULL,
		info->algo.do_dvchg2_event != NULL);
	if (info->dvchg2_dev != NULL && info->algo.do_dvchg2_event != NULL) {
		chr_err("register dvchg chg2 notifier done\n");
		info->dvchg2_nb.notifier_call = info->algo.do_dvchg2_event;
		register_charger_device_notifier(info->dvchg2_dev,
						 &info->dvchg2_nb);
		charger_dev_set_drvdata(info->dvchg2_dev, info);
	}

	return true;
}

static int mtk_charger_force_disable_power_path(struct mtk_charger *info,
						int idx, bool disable);
static int mtk_charger_plug_out(struct mtk_charger *info)
{
	struct charger_data *pdata1 = &info->chg_data[CHG1_SETTING];
	struct charger_data *pdata2 = &info->chg_data[CHG2_SETTING];
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i;

	chr_err("%s\n", __func__);
	info->chr_type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->charger_thread_polling = false;
	info->pd_reset = false;
        info->is_full_flag = false;

	pdata1->disable_charging_count = 0;
	pdata1->input_current_limit_by_aicl = -1;
	pdata2->disable_charging_count = 0;

	notify.evt = EVT_PLUG_OUT;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
	}
	memset(&info->sc.data, 0, sizeof(struct scd_cmd_param_t_1));
	charger_dev_set_input_current(info->chg1_dev, 100000);
	charger_dev_set_mivr(info->chg1_dev,
			     info->data.min_charger_voltage);
	/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 start*/
	cancel_delayed_work(&info->pe_stop_enable_termination_work);
	/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 end*/
	charger_dev_plug_out(info->chg1_dev);
	mtk_charger_force_disable_power_path(info, CHG1_SETTING, true);

	if (info->enable_vbat_mon)
		charger_dev_enable_6pin_battery_charging(info->chg1_dev,
							 false);
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
	cancel_delayed_work(&info->xm_charge_work);
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/
/*N17 code for low_fast by xm liluting at 2023/07/07 start*/
        info->first_low_plugin_flag = false;
        info->pps_fast_mode = false;
/*N17 code for low_fast by xm liluting at 2023/07/07 end*/
/*N17 code for cp_mode test by xm liluting at 2023/07/31 start*/
        info->fake_thermal_vote_current = 0;
/*N17 code for cp_mode test by xm liluting at 2023/07/31 end*/
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 start*/
        info->b_flag = NORMAL;
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 end*/
	return 0;
}

static int mtk_charger_plug_in(struct mtk_charger *info, int chr_type)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i, vbat;
/*N17 code for low_fast by xm liluting at 2023/07/07 start*/
        struct power_supply *psy = NULL;
        union power_supply_propval cval;
        int ret = 0;
/*N17 code for low_fast by xm liluting at 2023/07/07 end*/

	chr_debug("%s\n", __func__);

	info->chr_type = chr_type;
	info->usb_type = get_usb_type(info);
	info->charger_thread_polling = true;

	info->can_charging = true;
	//info->enable_dynamic_cv = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;
	info->old_cv = 0;
	info->stop_6pin_re_en = false;
	info->batpro_done = false;
	smart_charging(info);
	chr_err("mtk_is_charger_on plug in, type:%d\n", chr_type);

	vbat = get_battery_voltage(info);

	notify.evt = EVT_PLUG_IN;
	notify.value = 0;
	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
		chg_alg_set_prop(alg, ALG_REF_VBAT, vbat);
	}

	memset(&info->sc.data, 0, sizeof(struct scd_cmd_param_t_1));
	info->sc.disable_in_this_plug = false;

/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
	schedule_delayed_work(&info->xm_charge_work, msecs_to_jiffies(3000));
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/
	charger_dev_plug_in(info->chg1_dev);
	mtk_charger_force_disable_power_path(info, CHG1_SETTING, false);
/*N17 code for low_fast by xm liluting at 2023/07/07 start*/
        psy = power_supply_get_by_name("battery");
        if (psy != NULL) {
                ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &cval);
                if(!ret && (cval.intval <= 20) && (info->thermal_board_temp <= 390))
                {
                        info->first_low_plugin_flag = true;
                        chr_err("%s first_low_plugin_flag = %d, pps_fast_mode = %d", __func__, info->first_low_plugin_flag, info->pps_fast_mode);
                }
                else {
                        chr_err("%s failed to get capacity", __func__);
                }
        }
        else {
                chr_err("%s failed to get battery psy", __func__);
        }
/*N17 code for low_fast by xm liluting at 2023/07/07 end*/
/*N17 code for cp_mode test by xm liluting at 2023/07/31 start*/
        info->fake_thermal_vote_current = 0;
/*N17 code for cp_mode test by xm liluting at 2023/07/31 end*/
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 start*/
        info->b_flag = NORMAL;
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 end*/
	return 0;
}

static bool mtk_is_charger_on(struct mtk_charger *info)
{
	int chr_type;

	chr_type = get_charger_type(info);
	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		if (info->chr_type != POWER_SUPPLY_TYPE_UNKNOWN) {
			mtk_charger_plug_out(info);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			/*N17 code for HQ-314179 by hankang at 2023/08/31*/
			info->sw_jeita.charging = true;
			mutex_unlock(&info->cable_out_lock);
		}
	} else {
		if (info->chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
			mtk_charger_plug_in(info, chr_type);
		else {
			info->chr_type = chr_type;
			info->usb_type = get_usb_type(info);
		}

		if (info->cable_out_cnt > 0) {
			mtk_charger_plug_out(info);
			mtk_charger_plug_in(info, chr_type);
			mutex_lock(&info->cable_out_lock);
			info->cable_out_cnt = 0;
			mutex_unlock(&info->cable_out_lock);
		}
	}

	if (chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
		return false;

	return true;
}

static void charger_send_kpoc_uevent(struct mtk_charger *info)
{
	static bool first_time = true;
	ktime_t ktime_now;

	if (first_time) {
		info->uevent_time_check = ktime_get();
		first_time = false;
	} else {
		ktime_now = ktime_get();
		if ((ktime_ms_delta(ktime_now, info->uevent_time_check) /
		     1000) >= 60) {
			mtk_chgstat_notify(info);
			info->uevent_time_check = ktime_now;
		}
	}
}

static void kpoc_power_off_check(struct mtk_charger *info)
{
	unsigned int boot_mode = info->bootmode;
	int vbus = 0;
	int counter = 0;
	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if (boot_mode == 8 || boot_mode == 9) {
		vbus = get_vbus(info);
		if (vbus >= 0 && vbus < 2500 && !mtk_is_charger_on(info)
		    && !info->pd_reset) {
			chr_err
			    ("Unplug Charger/USB in KPOC mode, vbus=%d, shutdown\n",
			     vbus);
			while (1) {
				if (counter >= 20000) {
					chr_err("%s, wait too long\n",
						__func__);
/*N17 code for HQ-291689 by miaozhichao at 2023/5/26 start*/
					msleep(4000);
/*N17 code for HQ-291689 by miaozhichao at 2023/5/26 end*/
					kernel_power_off();
					break;
				}
				if (info->is_suspend == false) {
					chr_err
					    ("%s, not in suspend, shutdown\n",
					     __func__);
					/*N17 code for HQ-291689 by miaozhichao at 2023/5/26 start */
					msleep(4000);
					/*N17 code for HQ-291689 by miaozhichao at 2023/5/26 end */
					kernel_power_off();
				} else {
					chr_err
					    ("%s, suspend! cannot shutdown\n",
					     __func__);
					msleep(20);
				}
				counter++;
			}
		}
		charger_send_kpoc_uevent(info);
	}
}

static void charger_status_check(struct mtk_charger *info)
{
	union power_supply_propval online, status;
	struct power_supply *chg_psy = NULL;
	int ret;
	bool charging = true;

	chg_psy = devm_power_supply_get_by_phandle(&info->pdev->dev,
						   "charger");
	if (IS_ERR_OR_NULL(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
	} else {
		ret = power_supply_get_property(chg_psy,
						POWER_SUPPLY_PROP_ONLINE,
						&online);

		ret = power_supply_get_property(chg_psy,
						POWER_SUPPLY_PROP_STATUS,
						&status);

		if (!online.intval)
			charging = false;
		else {
			if (status.intval ==
			    POWER_SUPPLY_STATUS_NOT_CHARGING)
				charging = false;
		}
	}
	if (charging != info->is_charging)
		power_supply_changed(info->psy1);
	info->is_charging = charging;
}


static char *dump_charger_type(int chg_type, int usb_type)
{
	switch (chg_type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		return "none";
	case POWER_SUPPLY_TYPE_USB:
		if (usb_type == POWER_SUPPLY_USB_TYPE_SDP)
			return "usb";
		else
			return "nonstd";
	case POWER_SUPPLY_TYPE_USB_CDP:
		return "usb-h";
	case POWER_SUPPLY_TYPE_USB_DCP:
		return "std";
		//case POWER_SUPPLY_TYPE_USB_FLOAT:
		//      return "nonstd";
/*N17 code for HQ-293343 by miaozhichao at 2023/4/24 start*/
	case POWER_SUPPLY_TYPE_USB_ACA:
		return "hvdcp";
/*N17 code for HQ-293343 by miaozhichao at 2023/4/24 end*/
	default:
		return "unknown";
	}
}

static int charger_routine_thread(void *arg)
{
	struct mtk_charger *info = arg;
	unsigned long flags;
	unsigned int init_times = 3;
	static bool is_module_init_done;
	bool is_charger_on;
	int ret;
	int vbat_min, vbat_max;
	u32 chg_cv = 0;
	/* N17 code for HQ-292525 by tongjiacheng at 20230506 start */
	int i = 0;
	union power_supply_propval val;
	/* N17 code for HQ-292525 by tongjiacheng at 20230506 end */

	while (1) {
		ret = wait_event_interruptible(info->wait_que,
					       (info->
						charger_thread_timeout ==
						true));
		if (ret < 0) {
			chr_err("%s: wait event been interrupted(%d)\n",
				__func__, ret);
			continue;
		}

		while (is_module_init_done == false) {
			if (charger_init_algo(info) == true) {
				is_module_init_done = true;
				if (info->charger_unlimited) {
					info->enable_sw_safety_timer =
					    false;
					charger_dev_enable_safety_timer
					    (info->chg1_dev, false);
				}
			} else {
				if (init_times > 0) {
					chr_err("retry to init charger\n");
					init_times = init_times - 1;
					msleep(10000);
				} else {
					chr_err
					    ("holding to init charger\n");
					msleep(60000);
				}
			}
		}

		mutex_lock(&info->charger_lock);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock->active)
			__pm_stay_awake(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);

		/*N17 code for HQHW-4728 by yeyinzi at 2023/08/07 start*/
		info->battery_temp = get_battery_temperature(info);

		/*N17 code for HQHW-4654 by tongjiacheng at 2023/08/04 start*/
		if (get_uisoc(info) >= 100 &&
				info->polling_interval > CHARGING_100_PERCENT_INTERVAL) {
			info->polling_interval = CHARGING_100_PERCENT_INTERVAL;
			chr_err("uisoc reach 100, change charging interval(%d)\n",
				info->polling_interval);
		} else if (info->battery_temp > 46 &&
					info->polling_interval > CHARGING_ABNORMAL_TEMP_INTERVAL) {
			info->polling_interval = CHARGING_ABNORMAL_TEMP_INTERVAL;
			chr_err("battery temp > 46, change charging interval(%d)\n",
				info->polling_interval);
		} else if (get_uisoc(info) < 100 && info->battery_temp <= 46){
			info->polling_interval = CHARGING_INTERVAL;
			chr_err("nomal, switch to change charging interval(%d)\n",
				info->polling_interval);
		}
		/*N17 code for HQHW-4654 by tongjiacheng at 2023/08/04 end*/
		/*N17 code for HQHW-4728 by yeyinzi at 2023/08/07 end*/

		info->charger_thread_timeout = false;

		ret = charger_dev_get_adc(info->chg1_dev,
					  ADC_CHANNEL_VBAT, &vbat_min,
					  &vbat_max);
		ret =
		    charger_dev_get_constant_voltage(info->chg1_dev,
						     &chg_cv);

		if (vbat_min != 0)
			vbat_min = vbat_min / 1000;

		chr_err
		    ("Vbat=%d vbats=%d vbus:%d ibus:%d I=%d T=%d uisoc:%d type:%s>%s pd:%d swchg_ibat:%d cv:%d\n",
		     get_battery_voltage(info), vbat_min, get_vbus(info),
		     get_ibus(info), get_battery_current(info),
		     info->battery_temp, get_uisoc(info),
		     dump_charger_type(info->chr_type, info->usb_type),
		     dump_charger_type(get_charger_type(info),
				       get_usb_type(info)), info->pd_type,
		     get_ibat(info), chg_cv);

		is_charger_on = mtk_is_charger_on(info);

		if (info->charger_thread_polling == true)
			mtk_charger_start_timer(info);

		check_battery_exist(info);
		check_dynamic_mivr(info);
		charger_check_status(info);
		kpoc_power_off_check(info);

		if (is_disable_charger(info) == false &&
		    is_charger_on == true && info->can_charging == true) {
			if (info->algo.do_algorithm)
				info->algo.do_algorithm(info);
			charger_status_check(info);
		} else {
			/* N17 code for HQ-292513 by tongjiacheng at 20230510 start */
			if (is_disable_charger(info) == true) {
				/* N17 code for HQ-292525 by tongjiacheng at 20230506 start */
				charger_dev_enable_powerpath(info->
							     chg1_dev,
							     false);

				for (i = 0; i < MAX_ALG_NO; i++) {
					if (info->alg[i] == NULL)
						continue;
					chg_alg_stop_algo(info->alg[i]);
				}

				val.intval = true;
				ret =
				    power_supply_set_property(info->
							      chg_psy,
							      POWER_SUPPLY_PROP_STATUS,
							      &val);
				if (ret) {
					chr_err
					    ("set charger enable fail(%d)\n",
					     ret);
					return ret;
				}
				power_supply_changed(info->psy1);
			}
			/* N17 code for HQ-292525 by tongjiacheng at 20230506 end */
			/* N17 code for HQ-292513 by tongjiacheng at 20230510 end */
			chr_debug("disable charging %d %d %d\n",
				  is_disable_charger(info), is_charger_on,
				  info->can_charging);
		}
		if (info->bootmode != 1 && info->bootmode != 2
		    && info->bootmode != 4 && info->bootmode != 8
		    && info->bootmode != 9)
			smart_charging(info);
		spin_lock_irqsave(&info->slock, flags);
		__pm_relax(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		chr_debug("%s end , %d\n",
			  __func__, info->charger_thread_timeout);
		mutex_unlock(&info->charger_lock);
	}

	return 0;
}


#ifdef CONFIG_PM
static int charger_pm_event(struct notifier_block *notifier,
			    unsigned long pm_event, void *unused)
{
	ktime_t ktime_now;
	struct timespec64 now;
	struct mtk_charger *info;

	info = container_of(notifier, struct mtk_charger, pm_notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		info->is_suspend = true;
		chr_debug("%s: enter PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		info->is_suspend = false;
		chr_debug("%s: enter PM_POST_SUSPEND\n", __func__);
		ktime_now = ktime_get_boottime();
		now = ktime_to_timespec64(ktime_now);

		if (timespec64_compare(&now, &info->endtime) >= 0 &&
		    info->endtime.tv_sec != 0 &&
		    info->endtime.tv_nsec != 0) {
			chr_err("%s: alarm timeout, wake up charger\n",
				__func__);
			__pm_relax(info->charger_wakelock);
			info->endtime.tv_sec = 0;
			info->endtime.tv_nsec = 0;
			_wake_up_charger(info);
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
#endif				/* CONFIG_PM */

static enum alarmtimer_restart
mtk_charger_alarm_timer_func(struct alarm *alarm, ktime_t now)
{
	struct mtk_charger *info =
	    container_of(alarm, struct mtk_charger, charger_timer);
	ktime_t *time_p = info->timer_cb_duration;

	info->timer_cb_duration[0] = ktime_get_boottime();
	if (info->is_suspend == false) {
		chr_debug("%s: not suspend, wake up charger\n", __func__);
		info->timer_cb_duration[1] = ktime_get_boottime();
		_wake_up_charger(info);
		info->timer_cb_duration[6] = ktime_get_boottime();
	} else {
		chr_debug("%s: alarm timer timeout\n", __func__);
		__pm_stay_awake(info->charger_wakelock);
	}

	info->timer_cb_duration[7] = ktime_get_boottime();

	if (ktime_us_delta(time_p[7], time_p[0]) > 5000)
		chr_err("%s: delta_t: %ld %ld %ld %ld %ld %ld %ld (%ld)\n",
			__func__,
			ktime_us_delta(time_p[1], time_p[0]),
			ktime_us_delta(time_p[2], time_p[1]),
			ktime_us_delta(time_p[3], time_p[2]),
			ktime_us_delta(time_p[4], time_p[3]),
			ktime_us_delta(time_p[5], time_p[4]),
			ktime_us_delta(time_p[6], time_p[5]),
			ktime_us_delta(time_p[7], time_p[6]),
			ktime_us_delta(time_p[7], time_p[0]));

	return ALARMTIMER_NORESTART;
}

static void mtk_charger_init_timer(struct mtk_charger *info)
{
	alarm_init(&info->charger_timer, ALARM_BOOTTIME,
		   mtk_charger_alarm_timer_func);
	mtk_charger_start_timer(info);

}

static int mtk_charger_setup_files(struct platform_device *pdev)
{
	int ret = 0;
	struct proc_dir_entry *battery_dir = NULL, *entry = NULL;
	struct mtk_charger *info = platform_get_drvdata(pdev);

	ret = device_create_file(&(pdev->dev), &dev_attr_sw_jeita);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_chr_type);
	if (ret)
		goto _out;

	ret =
	    device_create_file(&(pdev->dev),
			       &dev_attr_enable_meta_current_limit);
	if (ret)
		goto _out;

	ret =
	    device_create_file(&(pdev->dev), &dev_attr_fast_chg_indicator);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_vbat_mon);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Pump_Express);
	if (ret)
		goto _out;

	ret =
	    device_create_file(&(pdev->dev),
			       &dev_attr_ADC_Charger_Voltage);
	if (ret)
		goto _out;
	ret =
	    device_create_file(&(pdev->dev),
			       &dev_attr_ADC_Charging_Current);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_input_current);
	if (ret)
		goto _out;

	ret =
	    device_create_file(&(pdev->dev), &dev_attr_charger_log_level);
	if (ret)
		goto _out;

	/* Battery warning */
	ret = device_create_file(&(pdev->dev), &dev_attr_BatteryNotify);
	if (ret)
		goto _out;

	/* sysfs node */
	ret = device_create_file(&(pdev->dev), &dev_attr_enable_sc);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_stime);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_etime);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_tuisoc);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_sc_ibat_limit);
	if (ret)
		goto _out;

	battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
	if (!battery_dir) {
		chr_err("%s: mkdir /proc/mtk_battery_cmd failed\n",
			__func__);
		return -ENOMEM;
	}

	entry = proc_create_data("current_cmd", 0644, battery_dir,
				 &mtk_chg_current_cmd_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("en_power_path", 0644, battery_dir,
				 &mtk_chg_en_power_path_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("en_safety_timer", 0644, battery_dir,
				 &mtk_chg_en_safety_timer_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}
	entry = proc_create_data("set_cv", 0644, battery_dir,
				 &mtk_chg_set_cv_fops, info);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}

	return 0;

      fail_procfs:
	remove_proc_subtree("mtk_battery_cmd", NULL);
      _out:
	return ret;
}

void mtk_charger_get_atm_mode(struct mtk_charger *info)
{
	char atm_str[64] = { 0 };
	char *ptr = NULL, *ptr_e = NULL;
	char keyword[] = "androidboot.atm=";
	int size = 0;

	ptr = strstr(chg_get_cmd(), keyword);
	if (ptr != 0) {
		ptr_e = strstr(ptr, " ");
		if (ptr_e == 0)
			goto end;

		size = ptr_e - (ptr + strlen(keyword));
		if (size <= 0)
			goto end;
		strncpy(atm_str, ptr + strlen(keyword), size);
		atm_str[size] = '\0';
		chr_err("%s: atm_str: %s\n", __func__, atm_str);

		if (!strncmp(atm_str, "enable", strlen("enable")))
			info->atm_enabled = true;
	}
      end:
	chr_err("%s: atm_enabled = %d\n", __func__, info->atm_enabled);
}

static int psy_charger_property_is_writeable(struct power_supply *psy,
					     enum power_supply_property
					     psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return 1;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property charger_psy_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 start*/
	POWER_SUPPLY_PROP_CHARGE_NOW,
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 end*/
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static int psy_charger_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct mtk_charger *info;
	struct charger_device *chg;
	struct charger_data *pdata;
	int ret = 0;
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 start*/
	int ret_tmp = 0;
	struct chg_alg_device *alg = NULL;
	struct power_supply *cp_psy = NULL;
	union power_supply_propval val_new;
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 end*/
	info = (struct mtk_charger *) power_supply_get_drvdata(psy);
	if (info == NULL) {
		chr_err("%s: get info failed\n", __func__);
		return -EINVAL;
	}
	chr_debug("%s psp:%d\n", __func__, psp);

	if (info->psy1 != NULL && info->psy1 == psy)
		chg = info->chg1_dev;
	else if (info->psy2 != NULL && info->psy2 == psy)
		chg = info->chg2_dev;
	else if (info->psy_dvchg1 != NULL && info->psy_dvchg1 == psy)
		chg = info->dvchg1_dev;
	else if (info->psy_dvchg2 != NULL && info->psy_dvchg2 == psy)
		chg = info->dvchg2_dev;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (chg == info->dvchg1_dev) {
			val->intval = false;
			alg = get_chg_alg_by_name("pe5");
			if (alg == NULL)
				chr_err("get pe5 fail\n");
			else {
				ret = chg_alg_is_algo_ready(alg);
				if (ret == ALG_RUNNING)
					val->intval = true;
			}
			break;
		}

		val->intval = is_charger_exist(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (chg != NULL)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->enable_hv_charging;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_vbus(info);
		break;
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 start*/
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		cp_psy = power_supply_get_by_name("ln8000-charger");

		if (IS_ERR_OR_NULL(cp_psy)) {
			cp_psy =
			    power_supply_get_by_name("sc-cp-standalone");
			if (IS_ERR_OR_NULL(cp_psy)) {
				chr_err("%s cp psy fail\n", __func__);
				return 0;
			} else {
				ret_tmp = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val_new);	//ma
				val->intval =
				    get_ibus(info) + val_new.intval;
				break;
			}
		} else {
			ret_tmp =
			    power_supply_get_property(cp_psy,
						      POWER_SUPPLY_PROP_CURRENT_NOW,
						      &val_new);
			val_new.intval = val_new.intval / 1000;	//ma
			val->intval = get_ibus(info) + val_new.intval;
			break;
		}
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 end*/
	case POWER_SUPPLY_PROP_TEMP:
		if (chg == info->chg1_dev)
			val->intval =
			    info->chg_data[CHG1_SETTING].
			    junction_temp_max * 10;
		else if (chg == info->chg2_dev)
			val->intval =
			    info->chg_data[CHG2_SETTING].
			    junction_temp_max * 10;
		else if (chg == info->dvchg1_dev) {
			pdata = &info->chg_data[DVCHG1_SETTING];
			val->intval = pdata->junction_temp_max;
		} else if (chg == info->dvchg2_dev) {
			pdata = &info->chg_data[DVCHG2_SETTING];
			val->intval = pdata->junction_temp_max;
		} else
			val->intval = -127;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = get_charger_charging_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = get_charger_input_current(info, chg);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->chr_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		val->intval = get_charger_zcv(info, chg);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mtk_charger_enable_power_path(struct mtk_charger *info,
					 int idx, bool en)
{
	int ret = 0;
	bool is_en = true;
	struct charger_device *chg_dev = NULL;

	if (!info)
		return -EINVAL;

	switch (idx) {
	case CHG1_SETTING:
		chg_dev = get_charger_by_name("primary_chg");
		break;
	case CHG2_SETTING:
		chg_dev = get_charger_by_name("secondary_chg");
		break;
	default:
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(chg_dev)) {
		chr_err("%s: chg_dev not found\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);
	info->enable_pp[idx] = en;

	if (info->force_disable_pp[idx])
		goto out;

	ret = charger_dev_is_powerpath_enabled(chg_dev, &is_en);
	if (ret < 0) {
		chr_err("%s: get is power path enabled failed\n",
			__func__);
		goto out;
	}
	if (is_en == en) {
		chr_err("%s: power path is already en = %d\n", __func__,
			is_en);
		goto out;
	}

	pr_info("%s: enable power path = %d\n", __func__, en);
	ret = charger_dev_enable_powerpath(chg_dev, en);
      out:
	mutex_unlock(&info->pp_lock[idx]);
	return ret;
}

static int mtk_charger_force_disable_power_path(struct mtk_charger *info,
						int idx, bool disable)
{
	int ret = 0;
	struct charger_device *chg_dev = NULL;

	if (!info)
		return -EINVAL;

	switch (idx) {
	case CHG1_SETTING:
		chg_dev = get_charger_by_name("primary_chg");
		break;
	case CHG2_SETTING:
		chg_dev = get_charger_by_name("secondary_chg");
		break;
	default:
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(chg_dev)) {
		chr_err("%s: chg_dev not found\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);

	if (disable == info->force_disable_pp[idx])
		goto out;

	info->force_disable_pp[idx] = disable;
	ret = charger_dev_enable_powerpath(chg_dev,
					   info->
					   force_disable_pp[idx] ? false :
					   info->enable_pp[idx]);
      out:
	mutex_unlock(&info->pp_lock[idx]);
	return ret;
}

int psy_charger_set_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	struct mtk_charger *info;
	int idx;
/*N17 code for HQHW-4469 by miaozhichao at 2023/7/04 start*/
	int value_1;
/*N17 code for HQHW-4469 by miaozhichao at 2023/7/04 end*/
	chr_err("%s: prop:%d %d\n", __func__, psp, val->intval);

	info = (struct mtk_charger *) power_supply_get_drvdata(psy);

	if (info == NULL) {
		chr_err("%s: failed to get info\n", __func__);
		return -EINVAL;
	}

	if (info->psy1 != NULL && info->psy1 == psy)
		idx = CHG1_SETTING;
	else if (info->psy2 != NULL && info->psy2 == psy)
		idx = CHG2_SETTING;
	else if (info->psy_dvchg1 != NULL && info->psy_dvchg1 == psy)
		idx = DVCHG1_SETTING;
	else if (info->psy_dvchg2 != NULL && info->psy_dvchg2 == psy)
		idx = DVCHG2_SETTING;
	else {
		chr_err("%s fail\n", __func__);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (val->intval > 0)
			info->enable_hv_charging = true;
		else
			info->enable_hv_charging = false;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		info->chg_data[idx].thermal_charging_current_limit =
		    val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		info->chg_data[idx].thermal_input_current_limit =
		    val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (val->intval > 0)
			mtk_charger_enable_power_path(info, idx, false);
		else
			mtk_charger_enable_power_path(info, idx, true);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		if (val->intval > 0)
			mtk_charger_force_disable_power_path(info, idx,
							     true);
		else
			mtk_charger_force_disable_power_path(info, idx,
							     false);
		break;
/*N17 code for HQHW-4469 by miaozhichao at 2023/7/04 start*/
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		value_1 = val->intval;
		break;
/*N17 code for HQHW-4469 by miaozhichao at 2023/7/04 end*/
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}

static void mtk_charger_external_power_changed(struct power_supply *psy)
{
	struct mtk_charger *info;
	union power_supply_propval prop = { 0 };
	union power_supply_propval prop2 = { 0 };
	union power_supply_propval vbat0 = { 0 };
	struct power_supply *chg_psy = NULL;
	int ret;
/* N17 code for HQ-291012 by tongjiacheng at 20230601 start */
	char chr_type_str[64];
	char *envp[] = { chr_type_str, NULL };
/* N17 code for HQ-291012 by tongjiacheng at 20230601 end */

	info = (struct mtk_charger *) power_supply_get_drvdata(psy);

	if (info == NULL) {
		pr_notice("%s: failed to get info\n", __func__);
		return;
	}
	chg_psy = info->chg_psy;

	if (chg_psy == NULL) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);
		chg_psy =
		    devm_power_supply_get_by_phandle(&info->pdev->dev,
						     "charger");
		info->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
						POWER_SUPPLY_PROP_ONLINE,
						&prop);
		ret =
		    power_supply_get_property(chg_psy,
					      POWER_SUPPLY_PROP_USB_TYPE,
					      &prop2);
		ret =
		    power_supply_get_property(chg_psy,
					      POWER_SUPPLY_PROP_ENERGY_EMPTY,
					      &vbat0);
	}

	if (info->vbat0_flag != vbat0.intval) {
		if (vbat0.intval) {
			info->enable_vbat_mon = false;
			charger_dev_enable_6pin_battery_charging(info->
								 chg1_dev,
								 false);
		} else
			info->enable_vbat_mon = info->enable_vbat_mon_bak;

		info->vbat0_flag = vbat0.intval;
	}

	pr_notice("%s event, name:%s online:%d type:%d vbus:%d\n",
		  __func__, psy->desc->name, prop.intval, prop2.intval,
		  get_vbus(info));

	/* N17 code for HQ-291012 by tongjiacheng at 20230601 start */
	if (prop2.intval == POWER_SUPPLY_USB_TYPE_ACA) {
		sprintf(envp[0], "POWER_SUPPLY_QUICK_CHARGE_TYPE=1");

		ret = kobject_uevent_env(&(info->psy1->dev.kobj),
			   KOBJ_CHANGE, envp);
		if (ret)
			pr_err("%s send uevent fail(%d)\n", __func__, ret);
	}
	/* N17 code for HQ-291012 by tongjiacheng at 20230601 end */

	_wake_up_charger(info);
}

int notify_adapter_event(struct notifier_block *notifier,
			 unsigned long evt, void *val)
{
	struct mtk_charger *pinfo = NULL;

	chr_err("%s %lu\n", __func__, evt);

	pinfo = container_of(notifier, struct mtk_charger, pd_nb);

	switch (evt) {
	case MTK_PD_CONNECT_NONE:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify Detach\n");
		pinfo->pd_type = MTK_PD_CONNECT_NONE;
		pinfo->pd_reset = false;
		mutex_unlock(&pinfo->pd_lock);
		mtk_chg_alg_notify_call(pinfo, EVT_DETACH, 0);
		/* reset PE40 */
		break;

	case MTK_PD_CONNECT_HARD_RESET:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify HardReset\n");
		pinfo->pd_type = MTK_PD_CONNECT_NONE;
		pinfo->pd_reset = true;
		mutex_unlock(&pinfo->pd_lock);
		mtk_chg_alg_notify_call(pinfo, EVT_HARDRESET, 0);
		_wake_up_charger(pinfo);
		/* reset PE40 */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify fixe voltage ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK;
		pinfo->pd_reset = false;
		mutex_unlock(&pinfo->pd_lock);
		/* PD is ready */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK_PD30:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify PD30 ready\r\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
		pinfo->pd_reset = false;
		mutex_unlock(&pinfo->pd_lock);
		/* PD30 is ready */
		break;

	case MTK_PD_CONNECT_PE_READY_SNK_APDO:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify APDO Ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
		pinfo->pd_reset = false;
		mutex_unlock(&pinfo->pd_lock);
		/* PE40 is ready */
		_wake_up_charger(pinfo);
		break;

	case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
		mutex_lock(&pinfo->pd_lock);
		chr_err("PD Notify Type-C Ready\n");
		pinfo->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
		pinfo->pd_reset = false;
		mutex_unlock(&pinfo->pd_lock);
		/* type C is ready */
		_wake_up_charger(pinfo);
		break;
	case MTK_TYPEC_WD_STATUS:
		chr_err("wd status = %d\n", *(bool *) val);
		pinfo->water_detected = *(bool *) val;
		if (pinfo->water_detected == true)
			pinfo->notify_code |= CHG_TYPEC_WD_STATUS;
		else
			pinfo->notify_code &= ~CHG_TYPEC_WD_STATUS;
		mtk_chgstat_notify(pinfo);
		break;
/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 start*/
	case MTK_PD_UVDM:
		mutex_lock(&pinfo->pd_lock);
		usbpd_mi_vdm_received_cb(pinfo,
					 *(struct tcp_ny_uvdm *) val);
		mutex_unlock(&pinfo->pd_lock);
		break;
/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 end*/
	}
	return NOTIFY_DONE;
}

/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 start*/
static void usbpd_mi_vdm_received_cb(struct mtk_charger *pinfo,
				     struct tcp_ny_uvdm uvdm)
{
	int i, cmd;
	if (uvdm.uvdm_svid != USB_PD_MI_SVID)
		return;
	cmd = UVDM_HDR_CMD(uvdm.uvdm_data[0]);
	chr_err("cmd = %d\n", cmd);
	chr_err
	    ("uvdm.ack: %d, uvdm.uvdm_cnt: %d, uvdm.uvdm_svid: 0x%04x\n",
	     uvdm.ack, uvdm.uvdm_cnt, uvdm.uvdm_svid);
	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		pinfo->pd_adapter->vdm_data.ta_version = uvdm.uvdm_data[1];
		chr_err("ta_version:%x\n",
			pinfo->pd_adapter->vdm_data.ta_version);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		pinfo->pd_adapter->vdm_data.ta_temp =
		    (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		chr_err("pinfo->pd_adapter->vdm_data.ta_temp:%d\n",
			pinfo->pd_adapter->vdm_data.ta_temp);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		pinfo->pd_adapter->vdm_data.ta_voltage =
		    (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		pinfo->pd_adapter->vdm_data.ta_voltage *= 1000;
		chr_err("ta_voltage:%d\n",
			pinfo->pd_adapter->vdm_data.ta_voltage);
		break;
	case USBPD_UVDM_SESSION_SEED:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			pinfo->pd_adapter->vdm_data.s_secert[i] =
			    uvdm.uvdm_data[i + 1];
			chr_err("usbpd s_secert uvdm.uvdm_data[%d]=0x%x",
				i + 1, uvdm.uvdm_data[i + 1]);
		}
		break;
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			pinfo->pd_adapter->vdm_data.digest[i] =
			    uvdm.uvdm_data[i + 1];
			chr_err("usbpd digest[%d]=0x%x", i + 1,
				uvdm.uvdm_data[i + 1]);
		}
		break;
	case USBPD_UVDM_REVERSE_AUTHEN:
		pinfo->pd_adapter->vdm_data.reauth =
		    (uvdm.uvdm_data[1] & 0xFFFF);
		break;
	default:
		break;
	}
	pinfo->pd_adapter->uvdm_state = cmd;
}

/*N17 code for HQ-291625 by miaozhichao at 2023/04/28 end*/
int chg_alg_event(struct notifier_block *notifier,
		  unsigned long event, void *data)
{
/*N17 code for HQ-305659 by tongjiacheng at 2023/07/07 start*/
	struct mtk_charger *info = container_of(notifier,
		struct mtk_charger, chg_alg_nb);
	struct chg_alg_notify *noti = (struct chg_alg_notify *)data;
	chr_err("%s: evt:%lu\n", __func__, event);

	/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 start*/
	if (noti->evt == EVT_ALGO_STOP){
		_wake_up_charger(info);
		schedule_delayed_work(&info->pe_stop_enable_termination_work, msecs_to_jiffies(2000));
	}
	/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 end*/
/*N17 code for HQ-305659 by tongjiacheng at 2023/07/07 end*/
	return NOTIFY_DONE;
}


/*N17 code for screen_state by xm liluting at 2023/07/11 start*/
static int screen_state_for_charger_callback(struct notifier_block *nb,
                                            unsigned long val, void *v)
{
        int blank = *(int *)v;
        struct power_supply *psy = NULL;
	struct mtk_charger *info = NULL;

	if (info == NULL) {
		psy = power_supply_get_by_name("mtk-master-charger");
		if (psy == NULL)
			return -PTR_ERR(psy);
		else
			info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	}

        if (!(val == MTK_DISP_EARLY_EVENT_BLANK|| val == MTK_DISP_EVENT_BLANK)) {
                chr_err("%s event(%lu) do not need process\n", __func__, val);
                return NOTIFY_OK;
        }

        switch (blank) {
        case MTK_DISP_BLANK_UNBLANK: //power on
                info->sm.screen_state = 0;
                chr_err("%s screen_state = %d\n", __func__, info->sm.screen_state);
                break;
        case MTK_DISP_BLANK_POWERDOWN: //power off
                info->sm.screen_state = 1;
                chr_err("%s screen_state = %d\n", __func__, info->sm.screen_state);
                break;
        }
        return NOTIFY_OK;
}
/*N17 code for screen_state by xm liluting at 2023/07/11 end*/

/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 start*/
static int charger_notifier_event(struct notifier_block *notifier,
			unsigned long chg_event, void *val)
{
	struct mtk_charger *info;

	info = container_of(notifier,
		struct mtk_charger, chg_nb);

	switch (chg_event) {
	case THERMAL_BOARD_TEMP:
		info->thermal_board_temp = *(int *)val;
		chr_err("%s: get thermal_board_temp: %d\n", __func__, info->thermal_board_temp);
		break;
	default:
		chr_err("%s: not supported charger notifier event: %d\n", __func__, chg_event);
		break;
	}

	return NOTIFY_DONE;
}
/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 end*/

static char *mtk_charger_supplied_to[] = {
	"battery"
};

static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger *info = NULL;

	int i;
	char *name = NULL;
        int ret = 0;

	chr_err("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	platform_set_drvdata(pdev, info);
	info->pdev = pdev;

	mtk_charger_parse_dt(info, &pdev->dev);

	mutex_init(&info->cable_out_lock);
	mutex_init(&info->charger_lock);
	mutex_init(&info->pd_lock);
	for (i = 0; i < CHG2_SETTING + 1; i++) {
		mutex_init(&info->pp_lock[i]);
		info->force_disable_pp[i] = false;
		info->enable_pp[i] = true;
	}
	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s",
			      "charger suspend wakelock");
	info->charger_wakelock = wakeup_source_register(NULL, name);
	spin_lock_init(&info->slock);

	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	mtk_charger_init_timer(info);
#ifdef CONFIG_PM
	if (register_pm_notifier(&info->pm_notifier)) {
		chr_err("%s: register pm failed\n", __func__);
		return -ENODEV;
	}
	info->pm_notifier.notifier_call = charger_pm_event;
#endif				/* CONFIG_PM */
	srcu_init_notifier_head(&info->evt_nh);
	mtk_charger_setup_files(pdev);
	mtk_charger_get_atm_mode(info);

	for (i = 0; i < CHGS_SETTING_MAX; i++) {
		info->chg_data[i].thermal_charging_current_limit = -1;
		info->chg_data[i].thermal_input_current_limit = -1;
		info->chg_data[i].input_current_limit_by_aicl = -1;
	}
	info->enable_hv_charging = true;


	info->psy_desc1.name = "mtk-master-charger";
	info->psy_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc1.properties = charger_psy_properties;
	info->psy_desc1.num_properties =
	    ARRAY_SIZE(charger_psy_properties);
	info->psy_desc1.get_property = psy_charger_get_property;
	info->psy_desc1.set_property = psy_charger_set_property;
	info->psy_desc1.property_is_writeable =
	    psy_charger_property_is_writeable;
	info->psy_desc1.external_power_changed =
	    mtk_charger_external_power_changed;
	info->psy_cfg1.drv_data = info;
	info->psy_cfg1.supplied_to = mtk_charger_supplied_to;
	info->psy_cfg1.num_supplicants =
	    ARRAY_SIZE(mtk_charger_supplied_to);
	info->psy1 =
	    power_supply_register(&pdev->dev, &info->psy_desc1,
				  &info->psy_cfg1);

	info->chg_psy = devm_power_supply_get_by_phandle(&pdev->dev,
							 "charger");
	if (IS_ERR_OR_NULL(info->chg_psy))
		chr_err("%s: devm power fail to get chg_psy\n", __func__);

	info->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev,
							 "gauge");
	if (IS_ERR_OR_NULL(info->bat_psy))
		chr_err("%s: devm power fail to get bat_psy\n", __func__);

	if (IS_ERR(info->psy1))
		chr_err("register psy1 fail:%ld\n", PTR_ERR(info->psy1));

	info->psy_desc2.name = "mtk-slave-charger";
	info->psy_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc2.properties = charger_psy_properties;
	info->psy_desc2.num_properties =
	    ARRAY_SIZE(charger_psy_properties);
	info->psy_desc2.get_property = psy_charger_get_property;
	info->psy_desc2.set_property = psy_charger_set_property;
	info->psy_desc2.property_is_writeable =
	    psy_charger_property_is_writeable;
	info->psy_cfg2.drv_data = info;
	info->psy2 = power_supply_register(&pdev->dev, &info->psy_desc2,
					   &info->psy_cfg2);

	if (IS_ERR(info->psy2))
		chr_err("register psy2 fail:%ld\n", PTR_ERR(info->psy2));

	info->psy_dvchg_desc1.name = "mtk-mst-div-chg";
	info->psy_dvchg_desc1.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_dvchg_desc1.properties = charger_psy_properties;
	info->psy_dvchg_desc1.num_properties =
	    ARRAY_SIZE(charger_psy_properties);
	info->psy_dvchg_desc1.get_property = psy_charger_get_property;
	info->psy_dvchg_desc1.set_property = psy_charger_set_property;
	info->psy_dvchg_desc1.property_is_writeable =
	    psy_charger_property_is_writeable;
	info->psy_dvchg_cfg1.drv_data = info;
	info->psy_dvchg1 = power_supply_register(&pdev->dev,
						 &info->psy_dvchg_desc1,
						 &info->psy_dvchg_cfg1);
	if (IS_ERR(info->psy_dvchg1))
		chr_err("register psy dvchg1 fail:%ld\n",
			PTR_ERR(info->psy_dvchg1));

	info->psy_dvchg_desc2.name = "mtk-slv-div-chg";
	info->psy_dvchg_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_dvchg_desc2.properties = charger_psy_properties;
	info->psy_dvchg_desc2.num_properties =
	    ARRAY_SIZE(charger_psy_properties);
	info->psy_dvchg_desc2.get_property = psy_charger_get_property;
	info->psy_dvchg_desc2.set_property = psy_charger_set_property;
	info->psy_dvchg_desc2.property_is_writeable =
	    psy_charger_property_is_writeable;
	info->psy_dvchg_cfg2.drv_data = info;
	info->psy_dvchg2 = power_supply_register(&pdev->dev,
						 &info->psy_dvchg_desc2,
						 &info->psy_dvchg_cfg2);
	if (IS_ERR(info->psy_dvchg2))
		chr_err("register psy dvchg2 fail:%ld\n",
			PTR_ERR(info->psy_dvchg2));

	info->log_level = CHRLOG_ERROR_LEVEL;

	info->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!info->pd_adapter)
		chr_err("%s: No pd adapter found\n", __func__);
	else {
		info->pd_nb.notifier_call = notify_adapter_event;
		register_adapter_device_notifier(info->pd_adapter,
						 &info->pd_nb);
	}

	sc_init(&info->sc);
	info->chg_alg_nb.notifier_call = chg_alg_event;

	info->fast_charging_indicator = 0;
	info->enable_meta_current_limit = 1;
	info->is_charging = false;
	info->safety_timer_cmd = -1;
	info->diff_fv_val = 0;
	/*N17 code for HQ-319688 by p-xiepengfu at 20230907 start*/
	info->deltaFv = 0;
	/*N17 code for HQ-319688 by p-xiepengfu at 20230907 end*/
        info->is_full_flag = false;
/*N17 code for low_fast by xm liluting at 2023/07/07 start*/
        info->first_low_plugin_flag = false;
        info->pps_fast_mode = false;
/*N17 code for low_fast by xm liluting at 2023/07/07 end*/
/*N17 code for cp_mode test by xm liluting at 2023/07/31 start*/
        info->fake_thermal_vote_current = 0;
/*N17 code for cp_mode test by xm liluting at 2023/07/31 end*/
/*N17 code for HQ-329243 by yeyinzi at 2023/09/21 start*/
	info->during_switching = false;
/*N17 code for HQ-329243 by yeyinzi at 2023/09/21 end*/

	/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
	/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
	if (info != NULL && info->bootmode != 8 && info->bootmode != 9)
		mtk_charger_force_disable_power_path(info, CHG1_SETTING,
						     true);

	kthread_run(charger_routine_thread, info, "charger_thread");

	/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 start*/
	INIT_DELAYED_WORK(&info->pe_stop_enable_termination_work, pe_stop_enable_termination_work);
	/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 end*/

/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
	INIT_DELAYED_WORK(&info->xm_charge_work, xm_charge_work);
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/
/*N17 code for screen_state by xm liluting at 2023/07/11 start*/
        info->sm.charger_panel_notifier.notifier_call = screen_state_for_charger_callback;
        ret = mtk_disp_notifier_register("screen state", &info->sm.charger_panel_notifier);
        if (ret) {
               chr_err("[%s]: register screen state callback failed\n", __func__);
        }
/*N17 code for screen_state by xm liluting at 2023/07/11 end*/

/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 start*/
        info->chg_nb.notifier_call = charger_notifier_event;
	charger_reg_notifier(&info->chg_nb);
/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 end*/
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 start*/
        info->b_flag = NORMAL;
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 end*/
        info->thermal_board_temp = 250;

	return 0;
}

static int mtk_charger_remove(struct platform_device *dev)
{
/*N17 code for screen_state by xm liluting at 2023/07/11 start*/
        struct mtk_charger *info = platform_get_drvdata(dev);

        mtk_disp_notifier_unregister(&info->sm.charger_panel_notifier);
/*N17 code for screen_state by xm liluting at 2023/07/11 end*/

/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 start*/
	charger_unreg_notifier(&info->chg_nb);
/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 end*/

	return 0;
}

static void mtk_charger_shutdown(struct platform_device *dev)
{
	struct mtk_charger *info = platform_get_drvdata(dev);
	int i;

	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i] == NULL)
			continue;
		chg_alg_stop_algo(info->alg[i]);
	}
}

static const struct of_device_id mtk_charger_of_match[] = {
	{.compatible = "mediatek,charger",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_charger_of_match);

struct platform_device mtk_charger_device = {
	.name = "charger",
	.id = -1,
};

static struct platform_driver mtk_charger_driver = {
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
	.driver = {
		   .name = "charger",
		   .of_match_table = mtk_charger_of_match,
		   },
};

static int __init mtk_charger_init(void)
{
	return platform_driver_register(&mtk_charger_driver);
}

#if IS_BUILTIN(CONFIG_MTK_CHARGER)
late_initcall(mtk_charger_init);
#else
module_init(mtk_charger_init);
#endif

static void __exit mtk_charger_exit(void)
{
	platform_driver_unregister(&mtk_charger_driver);
}

module_exit(mtk_charger_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Driver");
MODULE_LICENSE("GPL");
