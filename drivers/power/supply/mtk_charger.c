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
#include <linux/init.h>
#include <linux/module.h>
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
#include <tcpm.h>
#include <linux/regmap.h>
#include <linux/of_platform.h>
#include <linux/hrtimer.h>

#include <../drivers/misc/hwid/hwid.h>
#include "mtk_charger.h"
#include "mtk_battery.h"
#include "bq28z610.h"
#include "../../misc/mediatek/typec/tcpc/inc/tcpci.h"

static struct platform_driver mtk_charger_driver;
static struct mtk_charger *pinfo = NULL;
#define MT6368_STRUP_ANA_CON1 0x989
struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

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

static void charger_parse_cmdline(struct mtk_charger *info)
{
	char *zircon_match= strnstr(product_name_get(), "zircon", strlen("zircon"));
	char *corot_match = strnstr(product_name_get(), "corot", strlen("corot"));

	if(zircon_match)
		info->product_name = ZIRCON;
	else if(corot_match)
		info->product_name = COROT;
	else
		info->product_name = UNKNOWN;
}

static int screen_state_for_charger_callback(struct notifier_block *nb, unsigned long val, void *v)
{
    struct mi_disp_notifier *evdata = v;
	struct mtk_charger *pinfo = container_of(nb,
		struct mtk_charger, charger_notifier);
    unsigned int blank;
	  if (!(val == MI_DISP_DPMS_EARLY_EVENT ||
	      val == MI_DISP_DPMS_EVENT)) {
		   return NOTIFY_OK;
	    }
    if (evdata && evdata->data){
      blank = *(int *)(evdata->data);
		if ((val == MI_DISP_DPMS_EVENT) && (blank == MI_DISP_DPMS_POWERDOWN
                   || blank == MI_DISP_DPMS_LP1 || blank == MI_DISP_DPMS_LP2)) {
			    pinfo->screen_status = DISPLAY_SCREEN_OFF;
		} else if ((val == MI_DISP_DPMS_EVENT) && (blank == MI_DISP_DPMS_ON)) {
			    pinfo->screen_status = DISPLAY_SCREEN_ON;
		}
	}else {
		return -1;
	}

    return NOTIFY_OK;
}

#ifdef MODULE
static char __chg_cmdline[COMMAND_LINE_SIZE];
static char *chg_cmdline = __chg_cmdline;
static void usbpd_mi_vdm_received_cb(struct mtk_charger *info, struct tcp_ny_uvdm uvdm);

const char *chg_get_cmd(void)
{
	struct device_node * of_chosen = NULL;
	char *bootargs = NULL;

	if (__chg_cmdline[0] != 0)
		return chg_cmdline;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(
					of_chosen, "bootargs", NULL);
		if (!bootargs)
			chr_err("%s: failed to get bootargs\n", __func__);
		else {
			strcpy(__chg_cmdline, bootargs);
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
			(struct mtk_charger *)power_supply_get_drvdata(psy);
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

	info->timer_cb_duration[2] = ktime_get_boottime();
	if (info == NULL)
		return;

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

bool is_disable_charger(struct mtk_charger *info)
{
	if (info == NULL)
		return true;

	if (info->disable_charger == true || IS_ENABLED(CONFIG_POWER_EXT))
		return true;
	else
		return false;
}

int _mtk_enable_charging(struct mtk_charger *info,
	bool en)
{
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

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		chr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			chr_err("%s: failed to get atag,boot\n", __func__);
		else {
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
		mtk_basic_charger_init(info);
	} else if (strcmp(info->algorithm_name, "Pulse") == 0) {
		mtk_pulse_charger_init(info);
	}

	info->disable_charger = of_property_read_bool(np, "disable_charger");
	info->charger_unlimited = of_property_read_bool(np, "charger_unlimited");
	info->atm_enabled = of_property_read_bool(np, "atm_is_enabled");
	info->enable_sw_safety_timer =
			of_property_read_bool(np, "enable_sw_safety_timer");
	info->sw_safety_timer_setting = info->enable_sw_safety_timer;


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
		info->data.battery_cv = BATTERY_CV;
	}

	if (of_property_read_u32(np, "max_charger_voltage", &val) >= 0)
		info->data.max_charger_voltage = val;
	else {
		info->data.max_charger_voltage = V_CHARGER_MAX;
	}
	info->data.max_charger_voltage_setting = info->data.max_charger_voltage;

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		info->data.min_charger_voltage = val;
	else {
		chr_err("use default V_CHARGER_MIN:%d\n", V_CHARGER_MIN);
		info->data.min_charger_voltage = V_CHARGER_MIN;
	}

	if (of_property_read_u32(np, "enable_vbat_mon", &val) >= 0) {
		info->enable_vbat_mon = val;
		info->enable_vbat_mon_bak = val;
	} else if (of_property_read_u32(np, "enable-vbat-mon", &val) >= 0) {
		info->enable_vbat_mon = val;
		info->enable_vbat_mon_bak = val;
	} else {
		info->enable_vbat_mon = 0;
		info->enable_vbat_mon_bak = 0;
	}

	info->enable_sw_jeita = of_property_read_bool(np, "enable_sw_jeita");
	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		info->data.jeita_temp_above_t4_cv = val;
	else {
		info->data.jeita_temp_above_t4_cv = JEITA_TEMP_ABOVE_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		info->data.jeita_temp_t3_to_t4_cv = val;
	else {
		info->data.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		info->data.jeita_temp_t2_to_t3_cv = val;
	else {
		info->data.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t2_cv", &val) >= 0)
		info->data.jeita_temp_t1_to_t2_cv = val;
	else {
		info->data.jeita_temp_t1_to_t2_cv = JEITA_TEMP_T1_TO_T2_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		info->data.jeita_temp_t0_to_t1_cv = val;
	else {
		info->data.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		info->data.jeita_temp_below_t0_cv = val;
	else {
		info->data.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		info->data.temp_t4_thres = val;
	else {
		info->data.temp_t4_thres = TEMP_T4_THRES;
	}

	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t4_thres_minus_x_degree = val;
	else {
		info->data.temp_t4_thres_minus_x_degree =
					TEMP_T4_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		info->data.temp_t3_thres = val;
	else {
		info->data.temp_t3_thres = TEMP_T3_THRES;
	}

	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		info->data.temp_t3_thres_minus_x_degree = val;
	else {
		info->data.temp_t3_thres_minus_x_degree =
					TEMP_T3_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		info->data.temp_t2_thres = val;
	else {
		chr_err("use default TEMP_T2_THRES:%d\n",
			TEMP_T2_THRES);
		info->data.temp_t2_thres = TEMP_T2_THRES;
	}

	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t2_thres_plus_x_degree = val;
	else {
		info->data.temp_t2_thres_plus_x_degree =
					TEMP_T2_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		info->data.temp_t1_thres = val;
	else {
		info->data.temp_t1_thres = TEMP_T1_THRES;
	}

	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t1_thres_plus_x_degree = val;
	else {
		info->data.temp_t1_thres_plus_x_degree =
					TEMP_T1_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		info->data.temp_t0_thres = val;
	else {
		info->data.temp_t0_thres = TEMP_T0_THRES;
	}

	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		info->data.temp_t0_thres_plus_x_degree = val;
	else {
		info->data.temp_t0_thres_plus_x_degree =
					TEMP_T0_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		info->data.temp_neg_10_thres = val;
	else {
		chr_err("use default TEMP_NEG_10_THRES:%d\n",
			TEMP_NEG_10_THRES);
		info->data.temp_neg_10_thres = TEMP_NEG_10_THRES;
	}

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

	if (of_property_read_u32(np, "max_charge_temp_minus_x_degree", &val)
		>= 0) {
		info->thermal.max_charge_temp_minus_x_degree = val;
	} else {
		chr_err("use default MAX_CHARGE_TEMP_MINUS_X_DEGREE:%d\n",
			MAX_CHARGE_TEMP_MINUS_X_DEGREE);
		info->thermal.max_charge_temp_minus_x_degree =
					MAX_CHARGE_TEMP_MINUS_X_DEGREE;
	}

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

	if (of_property_read_u32(np, "ac_charger_input_current", &val) >= 0)
		info->data.ac_charger_input_current = val;
	else {
		chr_err("use default AC_CHARGER_INPUT_CURRENT:%d\n",
			AC_CHARGER_INPUT_CURRENT);
		info->data.ac_charger_input_current = AC_CHARGER_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "charging_host_charger_current", &val)
		>= 0) {
		info->data.charging_host_charger_current = val;
	} else {
		chr_err("use default CHARGING_HOST_CHARGER_CURRENT:%d\n",
			CHARGING_HOST_CHARGER_CURRENT);
		info->data.charging_host_charger_current =
					CHARGING_HOST_CHARGER_CURRENT;
	}

	info->enable_dynamic_mivr =
			of_property_read_bool(np, "enable_dynamic_mivr");

	if (of_property_read_u32(np, "min_charger_voltage_1", &val) >= 0)
		info->data.min_charger_voltage_1 = val;
	else {
		chr_err("use default V_CHARGER_MIN_1: %d\n", V_CHARGER_MIN_1);
		info->data.min_charger_voltage_1 = V_CHARGER_MIN_1;
	}

	if (of_property_read_u32(np, "min_charger_voltage_2", &val) >= 0)
		info->data.min_charger_voltage_2 = val;
	else {
		chr_err("use default V_CHARGER_MIN_2: %d\n", V_CHARGER_MIN_2);
		info->data.min_charger_voltage_2 = V_CHARGER_MIN_2;
	}

	if (of_property_read_u32(np, "max_dmivr_charger_current", &val) >= 0)
		info->data.max_dmivr_charger_current = val;
	else {
		chr_err("use default MAX_DMIVR_CHARGER_CURRENT: %d\n",
			MAX_DMIVR_CHARGER_CURRENT);
		info->data.max_dmivr_charger_current =
					MAX_DMIVR_CHARGER_CURRENT;
	}
	info->enable_fast_charging_indicator =
			of_property_read_bool(np, "enable_fast_charging_indicator");

	if (of_property_read_u32(np, "fv", &val) >= 0)
		info->fv = val;
	else {
		info->fv = 4450;
	}

	if (of_property_read_u32(np, "fv_ffc", &val) >= 0)
		info->fv_ffc = val;
	else {
		info->fv_ffc = 4450;
	}

	if(info->product_name != ZIRCON){
		if (of_property_read_u32(np, "iterm", &val) >= 0)
			info->iterm = val;
		else {
			info->iterm = 200;
		}
		if (of_property_read_u32(np, "iterm_warm", &val) >= 0)
			info->iterm_warm = val;
		else {
			info->iterm_warm = info->iterm;
		}

		if (of_property_read_u32(np, "iterm_ffc", &val) >= 0)
			info->iterm_ffc = val;
		else {
			info->iterm_ffc = 700;
		}

		if (of_property_read_u32(np, "iterm_ffc_warm", &val) >= 0)
			info->iterm_ffc_warm = val;
		else {
			info->iterm_ffc_warm = 800;
		}
	}

	if (of_property_read_u32(np, "ffc_low_tbat", &val) >= 0)
		info->ffc_low_tbat = val;
	else {
		info->ffc_low_tbat = 160;
	}

	if (of_property_read_u32(np, "ffc_medium_tbat", &val) >= 0)
		info->ffc_medium_tbat = val;
	else {
		info->ffc_medium_tbat = 360;
	}

	if (of_property_read_u32(np, "ffc_high_tbat", &val) >= 0)
		info->ffc_high_tbat = val;
	else {
		info->ffc_high_tbat = 460;
	}

	if (of_property_read_u32(np, "ffc_high_soc", &val) >= 0)
		info->ffc_high_soc = val;
	else {
		info->ffc_high_soc = 96;
	}

	if (of_property_read_u32(np, "max_fcc", &val) >= 0)
	{
		info->max_fcc = val;
	}
	else {
		info->max_fcc = 22000;
	}

	if (of_property_read_u32(np, "max_charge_power", &val) >= 0)
	{
		info->max_charge_power = val;
	}
	else {
		info->max_charge_power = 120;
	}

}

static void mtk_charger_start_timer(struct mtk_charger *info)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;

	ret = alarm_try_to_cancel(&info->charger_timer);
	if (ret < 0) {
		chr_err("%s: callback was running, skip timer\n", __func__);
		return;
	}

	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);
	end_time.tv_sec = time_now.tv_sec + info->polling_interval;
	end_time.tv_nsec = time_now.tv_nsec + 0;
	info->endtime = end_time;
	ktime = ktime_set(info->endtime.tv_sec, info->endtime.tv_nsec);

	alarm_start(&info->charger_timer, ktime);
}

static void check_battery_exist(struct mtk_charger *info)
{
	unsigned int i = 0;
	int count = 0;

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
		if (vbat < 4000)
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage_2);
		else if (vbat < 4200)
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage_1);
		else
			charger_dev_set_mivr(info->chg1_dev,
				info->data.min_charger_voltage);
	}
}

int charger_manager_get_sic_current()
{
	if (pinfo == NULL)
	{
		return 0;
	}
	chr_err("%s sic_current=%d\n", __func__, pinfo->sic_current);
	return pinfo->sic_current * 1000;
}
EXPORT_SYMBOL(charger_manager_get_sic_current);

void charger_manager_set_sic_current(int sic_current)
{
	if (pinfo == NULL || !pinfo->fcc_votable)
	{
		chr_err("%s error pinfo is null\n", __func__);
		return;
	}
	if (sic_current < 0)
		return;
	pinfo->sic_current = sic_current;
	if(pinfo->smart_chg[SMART_CHG_BATT_LOW_FAST].active_status == false)
		vote(pinfo->fcc_votable, SIC_VOTER, true, sic_current);
	else
		vote(pinfo->fcc_votable, SIC_VOTER, false, sic_current);

}
EXPORT_SYMBOL(charger_manager_set_sic_current);

void night_charging_set_flag(bool night_charging)
{
	if(pinfo == NULL)
		return;
#ifdef CONFIG_FACTORY_BUILD
	pinfo->night_charging = true;
#else
	pinfo->night_charging = night_charging;
#endif
}
EXPORT_SYMBOL(night_charging_set_flag);

int night_charging_get_flag()
{
	if(pinfo ==NULL)
		return 0;
#ifdef CONFIG_FACTORY_BUILD
	return true;
#else
	return pinfo->night_charging;
#endif
}
EXPORT_SYMBOL(night_charging_get_flag);

void smart_batt_set_diff_fv(int val)
{
	if(pinfo == NULL)
		return;
	pinfo->smart_batt_reduceXMv = val;
}
EXPORT_SYMBOL(smart_batt_set_diff_fv);

void manual_set_cc_toggle(bool en)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;
	if(pinfo == NULL)
		return;
	if(pinfo->tcpc == NULL)
		return;
	pinfo->ui_cc_toggle = en;
	if(!pinfo->typec_attach && en)
	{
		tcpci_set_cc(pinfo->tcpc, TYPEC_CC_DRP);
	}else if(!pinfo->typec_attach && !en){
		tcpci_set_cc(pinfo->tcpc, TYPEC_CC_RD);
	}
	if(en && !pinfo->cid_status)
	{
		ret = alarm_try_to_cancel(&pinfo->rust_det_work_timer);
		if (ret < 0) {
			return;
		}
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		end_time.tv_sec = time_now.tv_sec + 600;
		end_time.tv_nsec = time_now.tv_nsec + 0;
		ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

		alarm_start(&pinfo->rust_det_work_timer, ktime);
	}else{
		ret = alarm_try_to_cancel(&pinfo->rust_det_work_timer);
		if (ret < 0) {
			return;
		}
	}
	return;
}
EXPORT_SYMBOL(manual_set_cc_toggle);

void manual_get_cc_toggle(bool *cc_toggle)
{
	if(pinfo == NULL)
		return;
	*cc_toggle = pinfo->ui_cc_toggle;
	return;
}
EXPORT_SYMBOL(manual_get_cc_toggle);

bool manual_get_cid_status()
{
	if(pinfo == NULL)
		return true;
	return pinfo->cid_status;
}
EXPORT_SYMBOL(manual_get_cid_status);

static void hrtime_otg_work_func(struct work_struct *work)
{
	if(pinfo != NULL && pinfo->tcpc != NULL)
		tcpci_set_cc(pinfo->tcpc, TYPEC_CC_RD);
}

static enum alarmtimer_restart rust_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	if(pinfo != NULL)
	{
		pinfo->ui_cc_toggle = false;
		schedule_delayed_work(&pinfo->hrtime_otg_work, 0);
	}
    return ALARMTIMER_NORESTART;
}

#if defined(CONFIG_RUST_DETECTION)
int lpd_dp_res_get_from_charger(int i)
{
	if(pinfo == NULL || i < 1 || i > 4)
		return 255;
	return pinfo->lpd_res[i];
}
EXPORT_SYMBOL(lpd_dp_res_get_from_charger);

void lpd_update_en_set_to_charger(int en)
{
	if(pinfo == NULL)
		return;
	pinfo->lpd_update_en = !!en;
}
EXPORT_SYMBOL(lpd_update_en_set_to_charger);
#endif

void do_sw_jeita_state_machine(struct mtk_charger *info)
{
	struct sw_jeita_data *sw_jeita;

	sw_jeita = &info->sw_jeita;
	sw_jeita->pre_sm = sw_jeita->sm;
	sw_jeita->charging = true;

	if (info->battery_temp >= info->data.temp_t4_thres) {
		chr_err("[SW_JEITA] Battery Over high Temperature(%d) !!\n",
			info->data.temp_t4_thres);

		sw_jeita->sm = TEMP_ABOVE_T4;
		sw_jeita->charging = false;
	} else if (info->battery_temp > info->data.temp_t3_thres) {
		if ((sw_jeita->sm == TEMP_ABOVE_T4)
		    && (info->battery_temp
			>= info->data.temp_t4_thres_minus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t4_thres_minus_x_degree,
				info->data.temp_t4_thres);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t3_thres,
				info->data.temp_t4_thres);

			sw_jeita->sm = TEMP_T3_TO_T4;
		}
	} else if (info->battery_temp >= info->data.temp_t2_thres) {
		if (((sw_jeita->sm == TEMP_T3_TO_T4)
		     && (info->battery_temp
			 >= info->data.temp_t3_thres_minus_x_degree))
		    || ((sw_jeita->sm == TEMP_T1_TO_T2)
			&& (info->battery_temp
			    <= info->data.temp_t2_thres_plus_x_degree))) {
			chr_err("[SW_JEITA] Battery Temperature not recovery to normal temperature charging mode yet!!\n");
		} else {
			chr_err("[SW_JEITA] Battery Normal Temperature between %d and %d !!\n",
				info->data.temp_t2_thres,
				info->data.temp_t3_thres);
			sw_jeita->sm = TEMP_T2_TO_T3;
		}
	} else if (info->battery_temp >= info->data.temp_t1_thres) {
		if ((sw_jeita->sm == TEMP_T0_TO_T1
		     || sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t1_thres_plus_x_degree)) {
			if (sw_jeita->sm == TEMP_T0_TO_T1) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
					info->data.temp_t1_thres_plus_x_degree,
					info->data.temp_t2_thres);
			}
			if (sw_jeita->sm == TEMP_BELOW_T0) {
				chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
					info->data.temp_t1_thres,
					info->data.temp_t1_thres_plus_x_degree);
				sw_jeita->charging = false;
			}
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t1_thres,
				info->data.temp_t2_thres);

			sw_jeita->sm = TEMP_T1_TO_T2;
		}
	} else if (info->battery_temp >= info->data.temp_t0_thres) {
		if ((sw_jeita->sm == TEMP_BELOW_T0)
		    && (info->battery_temp
			<= info->data.temp_t0_thres_plus_x_degree)) {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d,not allow charging yet!!\n",
				info->data.temp_t0_thres,
				info->data.temp_t0_thres_plus_x_degree);

			sw_jeita->charging = false;
		} else {
			chr_err("[SW_JEITA] Battery Temperature between %d and %d !!\n",
				info->data.temp_t0_thres,
				info->data.temp_t1_thres);

			sw_jeita->sm = TEMP_T0_TO_T1;
		}
	} else {
		chr_err("[SW_JEITA] Battery below low Temperature(%d) !!\n",
			info->data.temp_t0_thres);
		sw_jeita->sm = TEMP_BELOW_T0;
		sw_jeita->charging = false;
	}

	if (sw_jeita->sm != TEMP_T2_TO_T3) {
		if (sw_jeita->sm == TEMP_ABOVE_T4)
			sw_jeita->cv = info->data.jeita_temp_above_t4_cv;
		else if (sw_jeita->sm == TEMP_T3_TO_T4)
			sw_jeita->cv = info->data.jeita_temp_t3_to_t4_cv;
		else if (sw_jeita->sm == TEMP_T2_TO_T3)
			sw_jeita->cv = 0;
		else if (sw_jeita->sm == TEMP_T1_TO_T2)
			sw_jeita->cv = info->data.jeita_temp_t1_to_t2_cv;
		else if (sw_jeita->sm == TEMP_T0_TO_T1)
			sw_jeita->cv = info->data.jeita_temp_t0_to_t1_cv;
		else if (sw_jeita->sm == TEMP_BELOW_T0)
			sw_jeita->cv = info->data.jeita_temp_below_t0_cv;
		else
			sw_jeita->cv = info->data.battery_cv;
	} else {
		sw_jeita->cv = 0;
	}

	chr_err("[SW_JEITA]preState:%d newState:%d tmp:%d cv:%d\n",
		sw_jeita->pre_sm, sw_jeita->sm, info->battery_temp,
		sw_jeita->cv);
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

static void mtk_charger_set_algo_log_level(struct mtk_charger *info, int level)
{
	struct chg_alg_device *alg;
	int i = 0, ret = 0;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		ret = chg_alg_set_prop(alg, ALG_LOG_LEVEL, level);
		if (ret < 0)
			chr_err("%s: set ALG_LOG_LEVEL fail, ret =%d", __func__, ret);
	}
}

static ssize_t sw_jeita_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->enable_sw_jeita);
	return sprintf(buf, "%d\n", pinfo->enable_sw_jeita);
}

static ssize_t sw_jeita_store(struct device *dev, struct device_attribute *attr,
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

static ssize_t chr_type_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_err("%s: %d\n", __func__, pinfo->chr_type);
	return sprintf(buf, "%d\n", pinfo->chr_type);
}

static ssize_t chr_type_store(struct device *dev, struct device_attribute *attr,
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

static ssize_t pd_type_show(struct device *dev, struct device_attribute *attr,
                           char *buf)
{
    struct mtk_charger *pinfo = dev->driver_data;
    char *pd_type_name = "None";

    switch (pinfo->pd_type) {
    case MTK_PD_CONNECT_NONE:
        pd_type_name = "None";
    break;
    case MTK_PD_CONNECT_PE_READY_SNK:
        pd_type_name = "PD";
    break;
    case MTK_PD_CONNECT_PE_READY_SNK_PD30:
        pd_type_name = "PD";
    break;
    case MTK_PD_CONNECT_PE_READY_SNK_APDO:
        pd_type_name = "PD with PPS";
    break;
    case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
        pd_type_name = "normal";
    break;
    }
    chr_err("%s: %d\n", __func__, pinfo->pd_type);
    return sprintf(buf, "%s\n", pd_type_name);
}

static DEVICE_ATTR_RO(pd_type);

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
	chr_err("%s: idx = %d, detect = %d\n", __func__, i, is_ta_detected);
	return sprintf(buf, "%d\n", is_ta_detected);
}

static DEVICE_ATTR_RO(Pump_Express);

static ssize_t Charging_mode_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    int ret = 0, i = 0;
    char *alg_name = "normal";
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
    if (alg == NULL)
        return sprintf(buf, "%s\n", alg_name);

    switch (alg->alg_id) {
    case PE_ID:
        alg_name = "PE";
        break;
    case PE2_ID:
        alg_name = "PE2";
        break;
    case PDC_ID:
        alg_name = "PDC";
        break;
    case PE4_ID:
        alg_name = "PE4";
        break;
    case PE5_ID:
        alg_name = "P5";
        break;
    case PE5P_ID:
        alg_name = "P5P";
        break;
    }
    chr_err("%s: charging_mode: %s\n", __func__, alg_name);
    return sprintf(buf, "%s\n", alg_name);
}

static DEVICE_ATTR_RO(Charging_mode);

static ssize_t High_voltage_chg_enable_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    struct mtk_charger *pinfo = dev->driver_data;

    chr_err("%s: hv_charging = %d\n", __func__, pinfo->enable_hv_charging);
    return sprintf(buf, "%d\n", pinfo->enable_hv_charging);
}

static DEVICE_ATTR_RO(High_voltage_chg_enable);

static ssize_t Rust_detect_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    struct mtk_charger *pinfo = dev->driver_data;

    chr_err("%s: Rust detect = %d\n", __func__, pinfo->record_water_detected);
    return sprintf(buf, "%d\n", pinfo->record_water_detected);
}

static DEVICE_ATTR_RO(Rust_detect);

static ssize_t Thermal_throttle_show(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    struct mtk_charger *pinfo = dev->driver_data;
    struct charger_data *chg_data = &(pinfo->chg_data[CHG1_SETTING]);

    return sprintf(buf, "%d\n", chg_data->thermal_throttle_record);
}

static DEVICE_ATTR_RO(Thermal_throttle);

static ssize_t fast_chg_indicator_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->fast_charging_indicator);
	return sprintf(buf, "%d\n", pinfo->fast_charging_indicator);
}

static ssize_t fast_chg_indicator_store(struct device *dev, struct device_attribute *attr,
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

static ssize_t enable_meta_current_limit_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->enable_meta_current_limit);
	return sprintf(buf, "%d\n", pinfo->enable_meta_current_limit);
}

static ssize_t enable_meta_current_limit_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
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

static ssize_t vbat_mon_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	chr_debug("%s: %d\n", __func__, pinfo->enable_vbat_mon);
	return sprintf(buf, "%d\n", pinfo->enable_vbat_mon);
}

static ssize_t vbat_mon_store(struct device *dev, struct device_attribute *attr,
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
				struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int vbus = get_vbus(pinfo);

	chr_err("%s: %d\n", __func__, vbus);
	return sprintf(buf, "%d\n", vbus);
}

static DEVICE_ATTR_RO(ADC_Charger_Voltage);

static ssize_t ADC_Charging_Current_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;
	int ibat = get_battery_current(pinfo);

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
				      struct device_attribute *attr, char *buf)
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
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < 0) {
			chr_err("%s: val is invalid: %ld\n", __func__, temp);
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
		struct device_attribute *attr, const char *buf, size_t size)
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
		chr_info("%s: store code=0x%x\n", __func__, pinfo->notify_code);
		mtk_chgstat_notify(pinfo);
	}
	return size;
}

static DEVICE_ATTR_RW(BatteryNotify);

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
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int cv = 0;
	struct mtk_charger *info = PDE_DATA(file_inode(file));
	struct power_supply *psy = NULL;
	union  power_supply_propval dynamic_cv;

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
			chr_err("%s: adjust charge voltage %dV too high, use default cv\n",
				  __func__, cv);
		} else {
			info->data.battery_cv = cv;
			chr_err("%s: adjust charge voltage = %dV\n", __func__, cv);
		}
		psy = power_supply_get_by_name("battery");
		if (!IS_ERR_OR_NULL(psy)) {
			dynamic_cv.intval = info->data.battery_cv;
			ret = power_supply_set_property(psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &dynamic_cv);
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

	seq_printf(m, "%d %d\n", pinfo->usb_unlimited, pinfo->cmd_discharging);
	return 0;
}

static int mtk_chg_current_cmd_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_current_cmd_show, PDE_DATA(node));
}

static ssize_t mtk_chg_current_cmd_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[32] = {0};
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

	if (sscanf(desc, "%d %d", &current_unlimited, &cmd_discharging) == 2) {
		info->usb_unlimited = current_unlimited;
		if (cmd_discharging == 1) {
			info->cmd_discharging = true;
			charger_dev_enable(info->chg1_dev, false);
			charger_dev_do_event(info->chg1_dev,
					EVENT_DISCHARGE, 0);
		} else if (cmd_discharging == 0) {
			info->cmd_discharging = false;
			if (!info->night_charge_enable)
				charger_dev_enable(info->chg1_dev, !info->charge_full);
			charger_dev_do_event(info->chg1_dev,
					EVENT_RECHARGE, 0);
		}

		return count;
	}

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

static int mtk_chg_en_power_path_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_en_power_path_show, PDE_DATA(node));
}

static ssize_t mtk_chg_en_power_path_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
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
		return count;
	}

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

	charger_dev_is_safety_timer_enabled(pinfo->chg1_dev, &safety_timer_en);
	seq_printf(m, "%d\n", safety_timer_en);

	return 0;
}

static int mtk_chg_en_safety_timer_open(struct inode *node, struct file *file)
{
	return single_open(file, mtk_chg_en_safety_timer_show, PDE_DATA(node));
}

static ssize_t mtk_chg_en_safety_timer_write(struct file *file,
	const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
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
		info->safety_timer_cmd = (int)enable;

		if (info->sw_safety_timer_setting == true) {
			if (enable)
				info->enable_sw_safety_timer = true;
			else
				info->enable_sw_safety_timer = false;
		}

		return count;
	}

	return count;
}

static const struct proc_ops mtk_chg_en_safety_timer_fops = {
	.proc_open = mtk_chg_en_safety_timer_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = mtk_chg_en_safety_timer_write,
};

void scd_ctrl_cmd_from_user(void *nl_data, struct sc_nl_msg_t *ret_msg)
{
	struct sc_nl_msg_t *msg;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return;

	msg = nl_data;
	ret_msg->sc_cmd = msg->sc_cmd;

	switch (msg->sc_cmd) {

	case SC_DAEMON_CMD_PRINT_LOG:
		{
			chr_err("[%s] %s", SC_TAG, &msg->sc_data[0]);
		}
	break;

	case SC_DAEMON_CMD_SET_DAEMON_PID:
		{
			memcpy(&info->sc.g_scd_pid, &msg->sc_data[0],
				sizeof(info->sc.g_scd_pid));
			chr_err("[%s][fr] SC_DAEMON_CMD_SET_DAEMON_PID = %d(first launch)\n",
				SC_TAG, info->sc.g_scd_pid);
		}
	break;

	case SC_DAEMON_CMD_SETTING:
		{
			struct scd_cmd_param_t_1 data;

			memcpy(&data, &msg->sc_data[0],
				sizeof(struct scd_cmd_param_t_1));

			chr_debug("[%s] rcv data:%d %d %d %d %d %d %d %d %d %d %d %d %d %d Ans:%d\n",
				SC_TAG,
				data.data[0],
				data.data[1],
				data.data[2],
				data.data[3],
				data.data[4],
				data.data[5],
				data.data[6],
				data.data[7],
				data.data[8],
				data.data[9],
				data.data[10],
				data.data[11],
				data.data[12],
				data.data[13],
				data.data[14]);

			info->sc.solution = data.data[SC_SOLUTION];
			if (data.data[SC_SOLUTION] == SC_DISABLE)
				info->sc.disable_charger = true;
			else if (data.data[SC_SOLUTION] == SC_REDUCE)
				info->sc.disable_charger = false;
			else
				info->sc.disable_charger = false;
		}
	break;
	default:
		chr_err("[%s] bad sc_DAEMON_CTRL_CMD_FROM_USER 0x%x\n", SC_TAG, msg->sc_cmd);
		break;
	}

}

static void sc_nl_send_to_user(u32 pid, int seq, struct sc_nl_msg_t *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int size = reply_msg->sc_data_len + SCD_NL_MSG_T_HDR_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return;

	reply_msg->identity = SCD_NL_MAGIC;

	if (in_interrupt())
		skb = alloc_skb(len, GFP_ATOMIC);
	else
		skb = alloc_skb(len, GFP_KERNEL);

	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	ret = netlink_unicast(info->sc.daemo_nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		chr_err("[Netlink] sc send failed %d\n", ret);
		return;
	}

}

static void chg_nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct sc_nl_msg_t *sc_msg, *sc_ret_msg;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	data = NLMSG_DATA(nlh);

	sc_msg = (struct sc_nl_msg_t *)data;

	size = sc_msg->sc_ret_data_len + SCD_NL_MSG_T_HDR_LEN;

	if (size > (PAGE_SIZE << 1))
		sc_ret_msg = vmalloc(size);
	else {
		if (in_interrupt())
			sc_ret_msg = kmalloc(size, GFP_ATOMIC);
	else
		sc_ret_msg = kmalloc(size, GFP_KERNEL);
	}

	if (sc_ret_msg == NULL) {
		if (size > PAGE_SIZE)
			sc_ret_msg = vmalloc(size);

		if (sc_ret_msg == NULL)
			return;
	}

	memset(sc_ret_msg, 0, size);

	scd_ctrl_cmd_from_user(data, sc_ret_msg);
	sc_nl_send_to_user(pid, seq, sc_ret_msg);

	kvfree(sc_ret_msg);
}

void sc_select_charging_current(struct mtk_charger *info, struct charger_data *pdata)
{

	if (info->sc.g_scd_pid != 0 && info->sc.disable_in_this_plug == false) {
		chr_debug("sck: %d %d %d %d %d\n",
			info->sc.pre_ibat,
			info->sc.sc_ibat,
			pdata->charging_current_limit,
			pdata->thermal_charging_current_limit,
			info->sc.solution);
		if (info->sc.pre_ibat == -1 || info->sc.solution == SC_IGNORE
			|| info->sc.solution == SC_DISABLE) {
			info->sc.sc_ibat = -1;
		} else {
			if (info->sc.pre_ibat == pdata->charging_current_limit
				&& info->sc.solution == SC_REDUCE
				&& ((pdata->charging_current_limit - 100000) >= 500000)) {
				if (info->sc.sc_ibat == -1)
					info->sc.sc_ibat = pdata->charging_current_limit - 100000;
				else if (info->sc.sc_ibat - 100000 >= 500000)
					info->sc.sc_ibat = info->sc.sc_ibat - 100000;
			}
		}
	}
	info->sc.pre_ibat =  pdata->charging_current_limit;

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
		    pdata->charging_current_limit)
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
		info->sc.disable_in_this_plug = true;
	} else if ((info->sc.solution == SC_REDUCE || info->sc.solution == SC_KEEP)
		&& info->sc.sc_ibat <
		pdata->charging_current_limit && info->sc.g_scd_pid != 0 &&
		info->sc.disable_in_this_plug == false) {
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

void sc_update(struct mtk_charger *info)
{
	memset(&info->sc.data, 0, sizeof(struct scd_cmd_param_t_1));
	info->sc.data.data[SC_VBAT] = get_battery_voltage(info);
	info->sc.data.data[SC_BAT_TMP] = get_battery_temperature(info);
	info->sc.data.data[SC_UISOC] = get_uisoc(info);
	info->sc.data.data[SC_ENABLE] = info->sc.enable;
	info->sc.data.data[SC_BAT_SIZE] = info->sc.battery_size;
	info->sc.data.data[SC_START_TIME] = info->sc.start_time;
	info->sc.data.data[SC_END_TIME] = info->sc.end_time;
	info->sc.data.data[SC_IBAT_LIMIT] = info->sc.current_limit;
	info->sc.data.data[SC_TARGET_PERCENTAGE] = info->sc.target_percentage;
	info->sc.data.data[SC_LEFT_TIME_FOR_CV] = info->sc.left_time_for_cv;

	charger_dev_get_charging_current(info->chg1_dev, &info->sc.data.data[SC_IBAT_SETTING]);
	info->sc.data.data[SC_IBAT_SETTING] = info->sc.data.data[SC_IBAT_SETTING] / 1000;
	info->sc.data.data[SC_IBAT] = get_battery_current(info);


}

int wakeup_sc_algo_cmd(struct scd_cmd_param_t_1 *data, int subcmd, int para1)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (info->sc.g_scd_pid != 0) {
		struct sc_nl_msg_t *sc_msg;
		int size = SCD_NL_MSG_T_HDR_LEN + sizeof(struct scd_cmd_param_t_1);

		if (size > (PAGE_SIZE << 1))
			sc_msg = vmalloc(size);
		else {
			if (in_interrupt())
				sc_msg = kmalloc(size, GFP_ATOMIC);
		else
			sc_msg = kmalloc(size, GFP_KERNEL);

		}

		if (sc_msg == NULL) {
			if (size > PAGE_SIZE)
				sc_msg = vmalloc(size);

			if (sc_msg == NULL)
				return -1;
		}

		chr_debug(
			"[wakeup_fg_algo] malloc size=%d pid=%d\n",
			size, info->sc.g_scd_pid);
		memset(sc_msg, 0, size);
		sc_msg->sc_cmd = SC_DAEMON_CMD_NOTIFY_DAEMON;
		sc_msg->sc_subcmd = subcmd;
		sc_msg->sc_subcmd_para1 = para1;
		memcpy(sc_msg->sc_data, data, sizeof(struct scd_cmd_param_t_1));
		sc_msg->sc_data_len += sizeof(struct scd_cmd_param_t_1);
		sc_nl_send_to_user(info->sc.g_scd_pid, 0, sc_msg);

		kvfree(sc_msg);

		return 0;
	}
	chr_debug("pid is NULL\n");
	return -1;
}

static ssize_t enable_sc_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[enable smartcharging] : %d\n",
	info->sc.enable);

	return sprintf(buf, "%d\n", info->sc.enable);
}

static ssize_t enable_sc_store(
	struct device *dev, struct device_attribute *attr,
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
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[enable smartcharging] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[enable smartcharging] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val == 0)
			info->sc.enable = false;
		else
			info->sc.enable = true;

		chr_err(
			"[enable smartcharging]enable smartcharging=%d\n",
			info->sc.enable);
	}
	return size;
}
static DEVICE_ATTR_RW(enable_sc);

static ssize_t sc_stime_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging stime] : %d\n",
	info->sc.start_time);

	return sprintf(buf, "%d\n", info->sc.start_time);
}

static ssize_t sc_stime_store(
	struct device *dev, struct device_attribute *attr,
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
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging stime] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging stime] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0)
			info->sc.start_time = val;

		chr_err(
			"[smartcharging stime]enable smartcharging=%d\n",
			info->sc.start_time);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_stime);

static ssize_t sc_etime_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging etime] : %d\n",
	info->sc.end_time);

	return sprintf(buf, "%d\n", info->sc.end_time);
}

static ssize_t sc_etime_store(
	struct device *dev, struct device_attribute *attr,
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
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging etime] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging etime] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0)
			info->sc.end_time = val;

		chr_err(
			"[smartcharging stime]enable smartcharging=%d\n",
			info->sc.end_time);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_etime);

static ssize_t sc_tuisoc_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging target uisoc] : %d\n",
	info->sc.target_percentage);

	return sprintf(buf, "%d\n", info->sc.target_percentage);
}

static ssize_t sc_tuisoc_store(
	struct device *dev, struct device_attribute *attr,
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
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging tuisoc] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging tuisoc] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0)
			info->sc.target_percentage = val;

		chr_err(
			"[smartcharging stime]tuisoc=%d\n",
			info->sc.target_percentage);
	}
	return size;
}
static DEVICE_ATTR_RW(sc_tuisoc);

static ssize_t sc_ibat_limit_show(
	struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		chr_err("%s Couldn't get chg_psy\n", __func__);
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	chr_err(
	"[smartcharging ibat limit] : %d\n",
	info->sc.current_limit);

	return sprintf(buf, "%d\n", info->sc.current_limit);
}
int input_suspend_get_flag(void)
{
	if(pinfo ==NULL)
		return 0;
	return pinfo->input_suspend;
}
EXPORT_SYMBOL(input_suspend_get_flag);
int input_suspend_set_flag(int val)
{
	int input_suspend = 0;

	input_suspend = !!val;
	if (pinfo) {
		pinfo->input_suspend = input_suspend;
		charger_dev_enable_powerpath(pinfo->chg1_dev, !input_suspend);
		power_supply_changed(pinfo->psy1);
		if (!input_suspend)
			power_supply_changed(pinfo->usb_psy);
	}
	return 0;
}
EXPORT_SYMBOL(input_suspend_set_flag);

bool get_pd_hrst_state(void)
{
	if (pinfo) {
		if (!pinfo->pd_adapter)
			pinfo->pd_adapter = get_adapter_by_name("pd_adapter");

		if (pinfo->pd_adapter)
			return adapter_dev_get_hardreset_state(pinfo->pd_adapter);
	}

	return false;
}
EXPORT_SYMBOL(get_pd_hrst_state);

static ssize_t sc_ibat_limit_store(
	struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;
	struct power_supply *chg_psy = NULL;
	struct mtk_charger *info = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		return -EINVAL;
	}
	info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
	if (info == NULL)
		return -EINVAL;

	if (buf != NULL && size != 0) {
		chr_err("[smartcharging ibat limit] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			chr_err(
				"[smartcharging ibat limit] val is %d ??\n",
				(int)val);
			val = 0;
		}

		if (val >= 0)
			info->sc.current_limit = val;

		chr_err(
			"[smartcharging ibat limit]=%d\n",
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

		pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
		if (pinfo == NULL) {
			chr_err("[%s]mtk_gauge is not rdy\n", __func__);
			return -1;
		}
	}

	if (enable)
		sw_ovp = pinfo->data.max_charger_voltage_setting;
	else
		sw_ovp = 15000000;

	pinfo->data.max_charger_voltage = sw_ovp;

	disable_hw_ovp(pinfo, enable);

	chr_err("[%s] en:%d ovp:%d\n",
			    __func__, enable, sw_ovp);
	return ret;
}
EXPORT_SYMBOL(mtk_chg_enable_vbus_ovp);

static bool mtk_chg_check_vbus(struct mtk_charger *info)
{
	int vchr = 0;

	vchr = get_vbus(info) * 1000;
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

	vchr = get_vbus(info) * 1000;
	if (vchr < info->data.max_charger_voltage)
		info->notify_code &= ~CHG_VBUS_OV_STATUS;
	else {
		info->notify_code |= CHG_VBUS_OV_STATUS;
		chr_err("[BATTERY] charger_vol(%d mV) > %d mV\n",
			vchr / 1000, info->data.max_charger_voltage / 1000);
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
	ret = charger_dev_get_temperature(info->chg1_dev, &tchg_min, &tchg_max);
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
static int first_charger_type = 0;
static void get_first_charger_type(struct mtk_charger *info)
{
	struct timespec64 time_now;
	ktime_t ktime_now;
	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);
	if (time_now.tv_sec <= 15 && (get_charger_type(info) == POWER_SUPPLY_TYPE_USB_CDP))
		first_charger_type = POWER_SUPPLY_TYPE_USB_CDP;
}
static void charger_check_status(struct mtk_charger *info)
{
	bool charging = true;
	bool chg_dev_chgen = true;
	int temperature;
	struct battery_thermal_protection_data *thermal;
	int uisoc = 0;

	if (get_charger_type(info) == POWER_SUPPLY_TYPE_UNKNOWN)
		return;
	if (get_charger_type(info) == POWER_SUPPLY_TYPE_USB_CDP)
		get_first_charger_type(info);
	temperature = info->battery_temp;
	thermal = &info->thermal;
	uisoc = get_uisoc(info);

	info->setting.vbat_mon_en = true;
	if (info->enable_sw_jeita == true || info->enable_vbat_mon != true ||
	    info->batpro_done == true)
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
				if (get_usb_type(info) == POWER_SUPPLY_USB_TYPE_SDP)
					charger_dev_set_input_current(info->chg1_dev, 500000);
				else
					charger_dev_set_input_current(info->chg1_dev, 1500000);
				thermal->sm = BAT_TEMP_LOW;
				charging = false;
				goto stop_charging;
			} else if (thermal->sm == BAT_TEMP_LOW) {
				if (temperature >=
				    thermal->min_charge_temp_plus_x_degree) {
					thermal->sm = BAT_TEMP_NORMAL;
				} else {
					charging = false;
					goto stop_charging;
				}
			}
		}

		if (temperature >= thermal->max_charge_temp) {
			thermal->sm = BAT_TEMP_HIGH;
			charging = false;
			goto stop_charging;
		} else if (thermal->sm == BAT_TEMP_HIGH) {
			if (temperature
			    < thermal->max_charge_temp_minus_x_degree) {
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
		chr_err("%s, *** Error : can't find primary charger ***\n"
			, __func__);
		return false;
	}

	alg = get_chg_alg_by_name("pe5");
	info->alg[idx] = alg;
	if (alg == NULL)
		chr_err("get pe5 fail, not set pe5\n");
	else {
		chr_err("get pe5 success\n");
		alg->config = info->config;
		alg->alg_id = PE5_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe4, not set pe4");
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
		chr_err("get pd fail, not set mtk pd\n");
	else {
		chr_err("get pd success\n");
		alg->config = info->config;
		alg->alg_id = PDC_ID;
		chg_alg_init_algo(alg);
		register_chg_alg_notifier(alg, &info->chg_alg_nb);
	}
	idx++;

	alg = get_chg_alg_by_name("pe2, not set pe2");
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
		chr_err("get pe fail, not set pe\n");
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
			chr_err("*** Error : can't find secondary charger ***\n");
			return false;
		}
	} else if (info->config == DIVIDER_CHARGER ||
		   info->config == DUAL_DIVIDER_CHARGERS) {
		info->dvchg1_dev = get_charger_by_name("primary_dvchg");
		if (info->dvchg1_dev)
			chr_err("Found primary divider charger\n");
		else {
			chr_err("*** Error : can't find primary divider charger ***\n");
			return false;
		}
		if (info->config == DUAL_DIVIDER_CHARGERS) {
			info->dvchg2_dev =
				get_charger_by_name("secondary_dvchg");
			if (info->dvchg2_dev)
				chr_err("Found secondary divider charger\n");
			else {
				chr_err("*** Error : can't find secondary divider charger ***\n");
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
		info->dvchg1_dev != NULL, info->algo.do_dvchg1_event != NULL);
	if (info->dvchg1_dev != NULL && info->algo.do_dvchg1_event != NULL) {
		chr_err("register dvchg chg1 notifier done\n");
		info->dvchg1_nb.notifier_call = info->algo.do_dvchg1_event;
		register_charger_device_notifier(info->dvchg1_dev,
						&info->dvchg1_nb);
		charger_dev_set_drvdata(info->dvchg1_dev, info);
	}

	chr_err("register dvchg chg2 notifier %d %d\n",
		info->dvchg2_dev != NULL, info->algo.do_dvchg2_event != NULL);
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
	int i, data = 0;

	chr_err("%s\n", __func__);
	info->chr_type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->charger_thread_polling = false;
	info->pd_reset = false;
	info->pd_verify_done = false;
	info->pd_verifed = false;
	info->entry_soc = 0;
	info->high_temp_rec_soc = 0;
	info->apdo_max = 0;
	info->suspend_recovery = false;
	info->fg_full = false;
	info->charge_full = false;
	info->warm_term = false;
	info->real_type = XMUSB350_TYPE_UNKNOW;
	info->last_thermal_level = -1;
	info->disable_te_count = 0;
	info->thermal_current = 0;
	info->mt6363_auxadc4_enable = false;
	info->salve_conn_connected = false;
	info->ov_check_only_once = false;
	info->charge_status = 0;
	info->cycle_count_reduceXMv = 0;
	charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
	if (info->bms_psy) {
		data = false;
		bms_set_property(BMS_PROP_FASTCHARGE_MODE, data);
		data = FG_MONITOR_DELAY_30S;
		bms_set_property(BMS_PROP_MONITOR_DELAY, data);
	}
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
	wakeup_sc_algo_cmd(&info->sc.data, SC_EVENT_PLUG_OUT, 0);
	charger_dev_set_input_current(info->chg1_dev, 100000);
	charger_dev_set_mivr(info->chg1_dev, info->data.min_charger_voltage);
	charger_dev_plug_out(info->chg1_dev);
	if (info->jeita_support) {
		if(!info->typec_burn)
		{
			cancel_delayed_work_sync(&info->charge_monitor_work);
		}
		reset_step_jeita_charge(info);
	}
	if(info->product_name == ZIRCON){
		mod_delayed_work(system_wq,&info->delay_reset_full_flag_work, msecs_to_jiffies(60000));
	}

	mtk_charger_force_disable_power_path(info, CHG1_SETTING, false);
	charger_dev_enable_powerpath(info->chg1_dev, true);
	if (info->enable_vbat_mon)
		charger_dev_enable_6pin_battery_charging(info->chg1_dev, false);

	vote(info->fv_votable, FV_DEC_VOTER, false, 0);

	return 0;
}

static int mtk_charger_plug_in(struct mtk_charger *info,
				int chr_type)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	int i, vbat, ret = 0;
	union power_supply_propval pval = {0,};

	chr_debug("%s\n",
		__func__);

	if (!info->bms_psy) {
		info->bms_psy = power_supply_get_by_name("bms");
	}

	if (info->bms_psy) {
		ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
			info->vbat_now = pval.intval / 1000;

		ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
			info->temp_now = pval.intval;

		if (info->fcc_votable)
			reset_step_jeita_charge(info);
	}

	info->chr_type = chr_type;
	info->usb_type = get_usb_type(info);
	info->charger_thread_polling = true;

	info->can_charging = true;
	info->safety_timeout = false;
	info->vbusov_stat = false;
	info->old_cv = 0;
	info->stop_6pin_re_en = false;
	info->batpro_done = false;
	info->pd_verify_done = false;
	info->pd_verifed = false;
	info->entry_soc = get_uisoc(info);
	info->fg_full = false;
	info->charge_full = false;
	info->warm_term = false;
	info->last_thermal_level = -1;
	info->disable_te_count = 0;
	info->mt6363_auxadc4_enable = false;
	info->salve_conn_connected = false;
	info->cycle_count_reduceXMv = 0;

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
	wakeup_sc_algo_cmd(&info->sc.data, SC_EVENT_PLUG_IN, 0);
	charger_dev_plug_in(info->chg1_dev);
	if (info->jeita_support) {
		schedule_delayed_work(&info->charge_monitor_work, msecs_to_jiffies(2000));
	}
	mtk_charger_force_disable_power_path(info, CHG1_SETTING, false);
	vote(info->fv_votable, FV_DEC_VOTER, false, 0);

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
		if ((ktime_ms_delta(ktime_now, info->uevent_time_check) / 1000) >= 60) {
			mtk_chgstat_notify(info);
			info->uevent_time_check = ktime_now;
		}
	}
}

void set_soft_reset_status(int val)
{
	if (pinfo == NULL)
		return;
	pinfo->pd_soft_reset = !!val;
}
EXPORT_SYMBOL(set_soft_reset_status);

int get_soft_reset_status()
{
	if (pinfo == NULL)
		return 0;
	return pinfo->pd_soft_reset;
}
EXPORT_SYMBOL(get_soft_reset_status);

static void kpoc_power_off_check(struct mtk_charger *info)
{
	unsigned int boot_mode = info->bootmode;
	int vbus = 0;
	struct timespec64 time_now;
	ktime_t ktime_now;
	int vcount = 0;
	ktime_now = ktime_get_boottime();
	time_now = ktime_to_timespec64(ktime_now);

	if (boot_mode == 8 || boot_mode == 9) {
		vbus = get_vbus(info);
		if (vbus < 2500 && (first_charger_type == POWER_SUPPLY_TYPE_USB_CDP) && time_now.tv_sec <= 15) {
			msleep(3500);
		}
	}
	if (boot_mode == 8 || boot_mode == 9) {
		vbus = get_vbus(info);
		if (vbus >= 0 && vbus < 2500 && !mtk_is_charger_on(info) && !info->pd_reset && (time_now.tv_sec > 5)) {
			if (vcount > 6) {
				if (info->is_suspend == false) {
					if (system_state != SYSTEM_POWER_OFF)
					{
						msleep(5000);
						kernel_power_off();
					}
				} else {
					msleep(20);
				}
			} else {
				vcount++;
			}
		} else {
			vcount = 0;
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
			POWER_SUPPLY_PROP_ONLINE, &online);

		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_STATUS, &status);

		if (!online.intval)
			charging = false;
		else {
			if (status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
				charging = false;
		}
	}
	if (charging != info->is_charging)
		power_supply_changed(info->psy1);
	info->is_charging = charging;
}


__maybe_unused char *dump_charger_type(int chg_type, int usb_type)
{
	switch (chg_type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		return "none";

	case POWER_SUPPLY_TYPE_USB:
		if (usb_type == POWER_SUPPLY_USB_TYPE_SDP)
			return "usb";

	case POWER_SUPPLY_TYPE_USB_CDP:
		if (usb_type == POWER_SUPPLY_USB_TYPE_CDP)
			return "usb-h";
		else if (usb_type == POWER_SUPPLY_USB_TYPE_DCP && pinfo!=NULL && !pinfo->pd_type) {
			pinfo->real_type = XMUSB350_TYPE_FLOAT;
			return "nonstd";
		}
	case POWER_SUPPLY_TYPE_USB_DCP:
		return "std";
	default:
		return "unknown";
	}
}

int notify_adapter_event(struct notifier_block *notifier,
			unsigned long evt, void *val)
{
	struct mtk_charger *pinfo = NULL;

	chr_err("%s %d\n", __func__, evt);

	pinfo = container_of(notifier,
		struct mtk_charger, pd_nb);

	if (!pinfo->usb350_dev)
		pinfo->usb350_dev = get_charger_by_name("xmusb350");
	switch (evt) {
	case  MTK_PD_CONNECT_NONE:
		mutex_lock(&pinfo->pd_lock);
		pinfo->pd_type = MTK_PD_CONNECT_NONE;
		mutex_unlock(&pinfo->pd_lock);
		mtk_chg_alg_notify_call(pinfo, EVT_DETACH, 0);
		if (!pinfo->usb350_dev)
			pinfo->real_type = XMUSB350_TYPE_UNKNOW;
		else
			charger_dev_update_chgtype(pinfo->usb350_dev, XMUSB350_TYPE_UNKNOW);
		power_supply_changed(pinfo->usb_psy);
		break;

	case MTK_PD_CONNECT_HARD_RESET:
		mutex_lock(&pinfo->pd_lock);
		pinfo->pd_type = MTK_PD_CONNECT_NONE;
		pinfo->pd_reset = true;
		mutex_unlock(&pinfo->pd_lock);
		mtk_chg_alg_notify_call(pinfo, EVT_HARDRESET, 0);
		_wake_up_charger(pinfo);
		break;

	case MTK_PD_CONNECT_PE_READY_SNK:
		mutex_lock(&pinfo->pd_lock);
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK;
		mutex_unlock(&pinfo->pd_lock);
		if (!pinfo->usb350_dev)
			pinfo->real_type = XMUSB350_TYPE_PD;
		else
			charger_dev_update_chgtype(pinfo->usb350_dev, XMUSB350_TYPE_PD);
		power_supply_changed(pinfo->usb_psy);
		break;

	case MTK_PD_CONNECT_PE_READY_SNK_PD30:
		mutex_lock(&pinfo->pd_lock);
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
		mutex_unlock(&pinfo->pd_lock);
		if (!pinfo->usb350_dev)
			pinfo->real_type = XMUSB350_TYPE_PD;
		else
			charger_dev_update_chgtype(pinfo->usb350_dev, XMUSB350_TYPE_PD);
		power_supply_changed(pinfo->usb_psy);
		break;

	case MTK_PD_CONNECT_PE_READY_SNK_APDO:
		mutex_lock(&pinfo->pd_lock);
		pinfo->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
		mutex_unlock(&pinfo->pd_lock);
		if (!pinfo->usb350_dev)
			pinfo->real_type = XMUSB350_TYPE_PD;
		else
			charger_dev_update_chgtype(pinfo->usb350_dev, XMUSB350_TYPE_PD);
		msleep(300);
		power_supply_changed(pinfo->usb_psy);
		_wake_up_charger(pinfo);
		break;

	case MTK_PD_CONNECT_TYPEC_ONLY_SNK:
		mutex_lock(&pinfo->pd_lock);
		pinfo->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
		mutex_unlock(&pinfo->pd_lock);
		_wake_up_charger(pinfo);
		break;
	case MTK_TYPEC_WD_STATUS:
		pinfo->water_detected = *(bool *)val;
		if (pinfo->water_detected == true) {
			pinfo->notify_code |= CHG_TYPEC_WD_STATUS;
			pinfo->record_water_detected = true;
		} else
			pinfo->notify_code &= ~CHG_TYPEC_WD_STATUS;
		mtk_chgstat_notify(pinfo);
		break;
	case MTK_PD_UVDM:
		mutex_lock(&pinfo->pd_lock);
		usbpd_mi_vdm_received_cb(pinfo, *(struct tcp_ny_uvdm *)val);
		mutex_unlock(&pinfo->pd_lock);
		break;
	case MTK_PD_CONNECT_SOFT_RESET:
		mutex_lock(&pinfo->pd_lock);
		pinfo->pd_soft_reset = true;
		mutex_unlock(&pinfo->pd_lock);
		break;
	}
	return NOTIFY_DONE;
}

static void set_cc_drp_work_func(struct work_struct *work)
{
	if(pinfo != NULL && pinfo->tcpc != NULL)
		tcpci_set_cc(pinfo->tcpc, TYPEC_CC_DRP);
}

static int rust_det_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;
#if defined(CONFIG_RUST_DETECTION)
	int i = 0;
#endif

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

		if (old_state == TYPEC_UNATTACHED &&
				new_state != TYPEC_UNATTACHED &&
				!pinfo->typec_attach) {
			pinfo->typec_attach = true;
			pinfo->cid_status = true;
			if(pinfo->ui_cc_toggle){
				ret = alarm_try_to_cancel(&pinfo->rust_det_work_timer);
				if (ret < 0) {
				}
			}
#if defined(CONFIG_RUST_DETECTION)
			cancel_delayed_work_sync(&pinfo->rust_detection_work);
			for(i=0;i<5;i++)
				pinfo->lpd_res[i] = 255;
#endif
		} else if (old_state != TYPEC_UNATTACHED &&
				new_state == TYPEC_UNATTACHED &&
				pinfo->typec_attach) {
			pinfo->typec_attach = false;
			pinfo->cid_status = false;
			if(pinfo->ui_cc_toggle){
				if(pinfo->tcpc != NULL)
				{
					schedule_delayed_work(&pinfo->set_cc_drp_work, msecs_to_jiffies(500));
				}
				ret = alarm_try_to_cancel(&pinfo->rust_det_work_timer);

				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 600;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);


				alarm_start(&pinfo->rust_det_work_timer, ktime);

			}
#if defined(CONFIG_RUST_DETECTION)
			pinfo->lpd_debounce_times = 0;
			schedule_delayed_work(&pinfo->rust_detection_work, msecs_to_jiffies(5000));
#endif
		}
		break;
	default:
		chr_info("%s default event\n", __func__);
	}
	return NOTIFY_OK;
}

static int charger_routine_thread(void *arg)
{
	struct mtk_charger *info = arg;
	unsigned long flags;
	unsigned int init_times = 3;
	static bool is_module_init_done;
	bool is_charger_on;
	int ret, vbus = 0;
	int vbat_min, vbat_max;
	u32 chg_cv = 0;

	while (1) {
		 if (!info->pd_adapter)
		    info->pd_adapter = get_adapter_by_name("pd_adapter");
	    else if (info->pd_adapter && (info->flag == 0)) {
			info->pd_nb.notifier_call = notify_adapter_event;
			register_adapter_device_notifier(info->pd_adapter,
							 &info->pd_nb);
			info->flag = 1;
	    }

		if (!info->tcpc)
		{
			info->tcpc = tcpc_dev_get_by_name("type_c_port0");
			chr_err("get tcpc dev again\n");
			if(info->tcpc)
			{
				info->tcpc_rust_det_nb.notifier_call = rust_det_notifier_call;
				register_tcp_dev_notifier(info->tcpc,
						 &info->tcpc_rust_det_nb, TCP_NOTIFY_TYPE_ALL);
				chr_err("register tcpc_rust_det_nb ok\n");
			}
			else
				chr_err("get tcpc dev again failed\n");
		}

		ret = wait_event_interruptible(info->wait_que,
			(info->charger_thread_timeout == true));
		if (ret < 0) {
			chr_err("%s: wait event been interrupted(%d)\n", __func__, ret);
			continue;
		}

		while (is_module_init_done == false) {
			if (charger_init_algo(info) == true) {
				is_module_init_done = true;
				if (info->charger_unlimited) {
					info->enable_sw_safety_timer = false;
					charger_dev_enable_safety_timer(info->chg1_dev, false);
				}
			}
			else {
				if (init_times > 0) {
					init_times = init_times - 1;
					msleep(10000);
				} else {
					msleep(60000);
				}
			}
		}

		mutex_lock(&info->charger_lock);
		spin_lock_irqsave(&info->slock, flags);
		if (!info->charger_wakelock->active)
			__pm_stay_awake(info->charger_wakelock);
		spin_unlock_irqrestore(&info->slock, flags);
		info->charger_thread_timeout = false;
		info->battery_temp = get_battery_temperature(info);
		ret = charger_dev_get_adc(info->chg1_dev,
			ADC_CHANNEL_VBAT, &vbat_min, &vbat_max);
		ret = charger_dev_get_constant_voltage(info->chg1_dev, &chg_cv);

		if (vbat_min != 0)
			vbat_min = vbat_min / 1000;

		vbus = get_vbus(info);
		if (vbus < 7000 && info->real_type == XMUSB350_TYPE_HVDCP_2)
			charger_dev_select_qc_mode(info->usb350_dev, QC_MODE_QC2_9);
		if (get_charger_type(info) == POWER_SUPPLY_TYPE_USB_CDP && get_usb_type(info) == POWER_SUPPLY_USB_TYPE_DCP && (!info->pd_type))
			info->real_type = XMUSB350_TYPE_FLOAT;

		is_charger_on = mtk_is_charger_on(info);

		if (info->charger_thread_polling == true)
			mtk_charger_start_timer(info);

		check_battery_exist(info);
		check_dynamic_mivr(info);
		charger_check_status(info);
		kpoc_power_off_check(info);

		if (is_disable_charger(info) == false &&
			is_charger_on == true &&
			info->can_charging == true) {
			if (info->algo.do_algorithm)
				info->algo.do_algorithm(info);
			sc_update(info);
			wakeup_sc_algo_cmd(&info->sc.data, SC_EVENT_CHARGING, 0);
			charger_status_check(info);
		} else {
			chr_debug("disable charging %d %d %d\n",
			    is_disable_charger(info), is_charger_on, info->can_charging);
			sc_update(info);
			wakeup_sc_algo_cmd(&info->sc.data, SC_EVENT_STOP_CHARGING, 0);
		}

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

	info = container_of(notifier,
		struct mtk_charger, pm_notifier);

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
#endif

static int charger_notifier_event(struct notifier_block *notifier,
			unsigned long chg_event, void *val)
{
	struct mtk_charger *info;

	info = container_of(notifier,
		struct mtk_charger, chg_nb);

	switch (chg_event) {
	case THERMAL_BOARD_TEMP:
		info->thermal_board_temp = *(int *)val;
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static enum alarmtimer_restart
	mtk_charger_alarm_timer_func(struct alarm *alarm, ktime_t now)
{
	struct mtk_charger *info =
	container_of(alarm, struct mtk_charger, charger_timer);
	ktime_t *time_p = info->timer_cb_duration;

	info->timer_cb_duration[0] = ktime_get_boottime();
	if (info->is_suspend == false) {
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

#ifdef CONFIG_PM
	if (register_pm_notifier(&info->pm_notifier))
		chr_err("%s: register pm failed\n", __func__);
#endif
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

	ret = device_create_file(&(pdev->dev), &dev_attr_enable_meta_current_limit);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_fast_chg_indicator);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Charging_mode);
	if (ret)
	goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_pd_type);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_High_voltage_chg_enable);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Rust_detect);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Thermal_throttle);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_vbat_mon);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_Pump_Express);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_ADC_Charger_Voltage);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_ADC_Charging_Current);
	if (ret)
		goto _out;
	ret = device_create_file(&(pdev->dev), &dev_attr_input_current);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_charger_log_level);
	if (ret)
		goto _out;

	ret = device_create_file(&(pdev->dev), &dev_attr_BatteryNotify);
	if (ret)
		goto _out;

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
		chr_err("%s: mkdir /proc/mtk_battery_cmd failed\n", __func__);
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
	char atm_str[64] = {0};
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
					       enum power_supply_property psp)
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
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,

};

static int psy_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;
	struct charger_device *chg;
	struct charger_data *pdata;
	int ret = 0;
	struct chg_alg_device *alg = NULL;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	chr_debug("%s psp:%d\n", __func__, psp);

	if (info->psy1 != NULL &&
		info->psy1 == psy)
		chg = info->chg1_dev;
	else if (info->psy2 != NULL &&
		info->psy2 == psy)
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
	case POWER_SUPPLY_PROP_TEMP:
		if (chg == info->chg1_dev)
			val->intval =
				info->chg_data[CHG1_SETTING].junction_temp_max * 10;
		else if (chg == info->chg2_dev)
			val->intval =
				info->chg_data[CHG2_SETTING].junction_temp_max * 10;
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
		goto out;
	}
	if (is_en == en) {
		goto out;
	}
	if (info->input_suspend)
		en = !info->input_suspend;

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
		return -EINVAL;
	}

	mutex_lock(&info->pp_lock[idx]);

	if (disable == info->force_disable_pp[idx])
		goto out;
	if (info->input_suspend)
		disable = info->input_suspend;
	info->force_disable_pp[idx] = disable;
	ret = charger_dev_enable_powerpath(chg_dev,
		info->force_disable_pp[idx] ? false : info->enable_pp[idx]);
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

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	if (info->psy1 != NULL &&
		info->psy1 == psy)
		idx = CHG1_SETTING;
	else if (info->psy2 != NULL &&
		info->psy2 == psy)
		idx = CHG2_SETTING;
	else if (info->psy_dvchg1 != NULL && info->psy_dvchg1 == psy)
		idx = DVCHG1_SETTING;
	else if (info->psy_dvchg2 != NULL && info->psy_dvchg2 == psy)
		idx = DVCHG2_SETTING;
	else {
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
			mtk_charger_force_disable_power_path(info, idx, true);
		else
			mtk_charger_force_disable_power_path(info, idx, false);
		break;
	default:
		return -EINVAL;
	}
	_wake_up_charger(info);

	return 0;
}

static void mtk_charger_external_power_changed(struct power_supply *psy)
{
	struct mtk_charger *info;
	union power_supply_propval prop, prop2, vbat0;
	struct power_supply *chg_psy = NULL;
	int ret;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	chg_psy = info->chg_psy;

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(&info->pdev->dev,
						       "charger");
		info->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop2);
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ENERGY_EMPTY, &vbat0);
	}

	if (info->vbat0_flag != vbat0.intval) {
		if (vbat0.intval) {
			info->enable_vbat_mon = false;
			charger_dev_enable_6pin_battery_charging(info->chg1_dev, false);
		} else
			info->enable_vbat_mon = info->enable_vbat_mon_bak;

		info->vbat0_flag = vbat0.intval;
	}

	_wake_up_charger(info);
}

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uevent(struct mtk_charger *info)
{
	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SOC_DECIMAL=\n",
		"POWER_SUPPLY_SOC_DECIMAL_RATE=\n",
		"POWER_SUPPLY_QUICK_CHARGE_TYPE=\n",
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n",
		"POWER_SUPPLY_CONNECTOR_TEMP=\n",
	};
	int val;
	u32 cnt=0, i=0;
	char *envp[6] = { NULL };

	bms_get_property(BMS_PROP_SOC_DECIMAL, &val);
	sprintf(uevent_string[0]+25,"%d",val);
	envp[cnt++] = uevent_string[0];

	bms_get_property(BMS_PROP_SOC_DECIMAL_RATE, &val);
	sprintf(uevent_string[1]+30,"%d",val);
	envp[cnt++] = uevent_string[1];

	usb_get_property(USB_PROP_QUICK_CHARGE_TYPE, &val);
	sprintf(uevent_string[2]+31,"%d",val);
	envp[cnt++] = uevent_string[2];

	bms_get_property(BMS_PROP_SHUTDOWN_DELAY, &val);
	sprintf(uevent_string[3]+28,"%d",val);
	envp[cnt++] = uevent_string[3];

	usb_get_property(USB_PROP_CONNECTOR_TEMP, &val);
	sprintf(uevent_string[4]+28,"%d",val);
	envp[cnt++] = uevent_string[4];

	envp[cnt]=NULL;
	for(i = 0; i < cnt; ++i)
	      chr_err("%s\n", envp[i]);
	kobject_uevent_env(&info->pdev->dev.kobj, KOBJ_CHANGE, envp);
	return;
}
void update_quick_chg_type(struct mtk_charger *info)
{
	if (info->bms_psy)
		generate_xm_charge_uevent(info);
}
EXPORT_SYMBOL_GPL(update_quick_chg_type);
void update_connect_temp(struct mtk_charger *info)
{
	if(info)
		generate_xm_charge_uevent(info);
}
EXPORT_SYMBOL_GPL(update_connect_temp);
static void mtk_charger_external_power_usb_changed(struct power_supply *psy)
{
	struct mtk_charger *info;
	union power_supply_propval prop;
	struct power_supply *chg_psy = NULL;
	int ret;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);
	chg_psy = info->chg_psy;

	if (!info->bms_psy) {
		info->bms_psy = power_supply_get_by_name("bms");
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(&info->pdev->dev,
						       "charger");
		info->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
	}

	if (info->bms_psy)
		generate_xm_charge_uevent(info);
}

static const char * const power_supply_type_text[] = {
	"Unknown", "Battery", "UPS", "Mains", "USB", "USB_DCP", "USB_CDP", "USB_ACA",
	"USB_C", "USB_PD", "USB_PD_DRP", "BrickID", "Wireless"
};

static const char *get_type_name(int type)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(power_supply_type_text); i++) {
		if (i == type)
			return power_supply_type_text[i];
	}

	return "Unknown";
}

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger *info;

	info = (struct mtk_charger *)power_supply_get_drvdata(psy);

	info->usb_desc.type = get_charger_type(info);
	val->strval = get_type_name(info->usb_desc.type);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_charger_exist(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (info != NULL)
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
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_ibus(info);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

struct quick_charge_desc {
	enum xmusb350_chg_type psy_type;
	enum quick_charge_type type;
};

struct quick_charge_desc quick_charge_table[14] = {
	{ XMUSB350_TYPE_SDP,		QUICK_CHARGE_NORMAL },
	{ XMUSB350_TYPE_CDP,		QUICK_CHARGE_NORMAL },
	{ XMUSB350_TYPE_DCP,		QUICK_CHARGE_NORMAL },
	{ XMUSB350_TYPE_FLOAT,		QUICK_CHARGE_NORMAL },
	{ XMUSB350_TYPE_HVDCP,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_2,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_3,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_PD,		QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_35_18,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_35_27,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_3_18,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVCHG,	QUICK_CHARGE_FAST },
	{ XMUSB350_TYPE_HVDCP_3_27,	QUICK_CHARGE_FLASH },
	{0, 0},
};

static int get_quick_charge_type(struct mtk_charger *info)
{
	int i = 0;

	if (!info || !info->usb_psy || info->typec_burn)
		return QUICK_CHARGE_NORMAL;

	if (info->temp_now > 520 || info->temp_now < 0)
		return QUICK_CHARGE_NORMAL;

	if (info->real_type == XMUSB350_TYPE_PD && info->pd_verifed) {
		if (info->apdo_max >= 50)
			return QUICK_CHARGE_SUPER;
		else
			return QUICK_CHARGE_TURBE;
	}

	while (quick_charge_table[i].psy_type != 0) {
		if (info->real_type == quick_charge_table[i].psy_type) {
			return quick_charge_table[i].type;
		}

		i++;
	}

	return QUICK_CHARGE_NORMAL;
}

static int real_type_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->real_type;
	else
		*val = 0;
	return 0;
}

static int pmic_ibat_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = get_ibat(gm);
	else
		*val = 0;
	return 0;
}

static int real_type_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->real_type = val;
	return 0;
}

static int quick_charge_type_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	*val = get_quick_charge_type(gm);
	return 0;
}

static int pd_authentication_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->pd_verifed;
	else
		*val = 0;
	return 0;
}

static int pd_authentication_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm) {
		gm->pd_verifed = !!val;
		power_supply_changed(gm->usb_psy);
	}
	return 0;
}

static int pd_verifying_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->pd_verifying;
	else
		*val = 0;
	return 0;
}

static int pd_verifying_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->pd_verifying = val;
	return 0;
}

static int pd_type_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->pd_type;
	else
		*val = MTK_PD_CONNECT_NONE;
	return 0;
}

static int apdo_max_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm) {
		if (gm->typec_burn)
			*val = 0;
		else
			*val = gm->apdo_max;
	} else
		*val = 0;
	return 0;
}

static int apdo_max_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->apdo_max = val;
	update_quick_chg_type(gm);
	return 0;
}

static int typec_mode_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->typec_mode;
	else
		*val = 0;

	return 0;
}

static int typec_mode_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->typec_mode = val;

	return 0;
}

static int typec_cc_orientation_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->cc_orientation + 1;
	else
		*val = 0;
	return 0;
}

static int typec_cc_orientation_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->cc_orientation = val;
	return 0;
}

static int ffc_enable_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->ffc_enable;
	else
		*val = 0;
	return 0;
}

static int charge_full_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->charge_full;
	else
		*val = 0;

	return 0;
}

static int connector_temp_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm) {
		if (!gm->fake_typec_temp)
			charger_dev_get_ts_temp(gm->chg1_dev, val);
		else
			*val = gm->fake_typec_temp;
	} else
		*val = 0;
	return 0;
}

static int connector_temp_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->fake_typec_temp = val;
	return 0;
}

static int typec_burn_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->typec_burn;
	else
		*val = 0;
	return 0;
}

static int sw_cv_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->sw_cv;
	else
		*val = 0;
	return 0;
}

static int input_suspend_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->input_suspend;
	else
		*val = 0;
	return 0;
}

static int input_suspend_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	bool input_suspend = 0;

	input_suspend = !!val;
	if (gm) {
		gm->input_suspend = input_suspend;
		charger_dev_enable_powerpath(gm->chg1_dev, !input_suspend);
		power_supply_changed(gm->psy1);
		if (!input_suspend) {
			gm->suspend_recovery = true;
			power_supply_changed(gm->usb_psy);
		}
	}
	return 0;
}

static int jeita_chg_index_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->jeita_chg_index[0];
	else
		*val = 0;
	return 0;
}

static int power_max_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm) {
		if (gm->typec_burn)
			*val = 0;
		else
			*val = gm->apdo_max;
	} else
		*val = 0;
	return 0;
}

static int qc3_type_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->qc3_type;
	else
		*val = 0;
	return 0;
}

static int qc3_type_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm)
		gm->qc3_type = val;
	return 0;
}

static int otg_enable_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->otg_enable;
	else
		*val = 0;
	return 0;
}

static int otg_enable_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (!gm)
          return -1;
	gm->otg_enable = !!val;
	if (gm->otg_enable)
	{
		schedule_delayed_work(&gm->usb_otg_monitor_work, 0);
	}else
	{
		gm->typec_otg_burn = false;
		cancel_delayed_work_sync(&gm->usb_otg_monitor_work);
	}
	return 0;
}

static int pd_verify_done_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->pd_verify_done;
	else
		*val = 0;
	return 0;
}

static int pd_verify_done_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	if (gm) {
		gm->pd_verify_done = !!val;
	}
	return 0;
}

static int cp_ibus_delta_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	u32 master_ibus = 0, slave_ibus = 0;

	if (gm) {
		if (gm->cp_master && gm->cp_slave) {
			charger_dev_get_ibus(gm->cp_master, &master_ibus);
			charger_dev_get_ibus(gm->cp_slave, &slave_ibus);
		}
		*val = (master_ibus > slave_ibus) ? master_ibus - slave_ibus : slave_ibus - master_ibus;
	 } else
		*val = 0;
	return 0;
}

static int mtbf_test_get(struct mtk_charger *gm,
        struct mtk_usb_sysfs_field_info *attr,
        int *val)
{
        if (gm)
                *val = gm->usb_unlimited;
        else
                *val = 0;
        return 0;
}


static int cp_charge_recovery_get(struct mtk_charger *gm,
        struct mtk_usb_sysfs_field_info *attr,
        int *val)
{
        if (gm)
                *val = gm->suspend_recovery;
        else
                *val = 0;
        return 0;
}

static int mtbf_test_set(struct mtk_charger *gm,
        struct mtk_usb_sysfs_field_info *attr,
        int val)
{
        if (gm)
                gm->usb_unlimited  = !!val;
        return 0;
}

static int pmic_vbus_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = get_vbus(gm);
	else
		*val = 0;
	return 0;
}

static int input_current_now_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = get_ibus(gm);
	else
		*val = 0;
	return 0;
}

static int sconfig_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	*val = gm->charge_control_sconfig;

	return 0;
}

static int sconfig_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	gm->charge_control_sconfig = val;

	return 0;
}

static int battcont_online_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	switch (gm->product_name)
	{
	case ZIRCON:
		if (gm->battcont_disconnected)
			*val = 0;
		else
			*val = 1;
		break;
	case COROT:
		if(gm->battcont_online_adc >= 1400)
			*val = 0;
		else
			*val = 1;
		break;
	default:
		break;
	}

	return 0;
}

static int battcont_online_set(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int val)
{
	switch (gm->product_name)
	{
	case ZIRCON:
		gm->battcont_disconnected = val;
		break;
	case COROT:
		gm->battcont_online_adc = val;
		break;
	default:
		gm->battcont_online_adc = 1400;
		gm->battcont_disconnected = true;
		break;
	}

	return 0;
}

static int warm_term_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->warm_term;
	else
		*val = 0;

	return 0;
}

static int div_jeita_fcc_flag_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{

	if (gm)
		*val = gm->div_jeita_fcc_flag;
	else
		*val = -1;

	return 0;
}

static int jeita_chg_fcc_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{

	if (gm)
		*val = gm->jeita_chg_fcc;
	else
		*val = -1;

	return 0;
}

static int source_jeita_fcc_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{

	if (gm)
		*val = gm->source_jeita_fcc;
	else
		*val = -1;
	return 0;
}

static int real_full_get(struct mtk_charger *gm,
	struct mtk_usb_sysfs_field_info *attr,
	int *val)
{
	if (gm)
		*val = gm->real_full;
	else
		*val = -1;
	return 0;
}

static const char * const power_supply_typec_mode_text[] = {
	"Nothing attached", "Sink attached", "Powered cable w/ sink",
	"Debug Accessory", "Audio Adapter", "Powered cable w/o sink",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Non compliant",
};

static const char *get_typec_mode_name(int typec_mode)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(power_supply_typec_mode_text); i++) {
		if (i == typec_mode)
			return power_supply_typec_mode_text[i];
	}

	return "Nothing attached";
}

static const char * const power_supply_usb_type_text[] = {
	"Unknown", "OCP", "USB_FLOAT", "SDP", "CDP", "DCP", "USB_HVDCP_2", "USB_HVDCP_3",
	"USB_HVDCP_3P5", "USB_HVDCP_3P5", "USB_HVDCP_3", "USB_HVDCP_3", "USB_PD", "PD_DRP", "HVCHG", "Unknown", "USB_HVDCP"
};

static const char *get_usb_type_name(int usb_type)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}

	return "Unknown";
}

static ssize_t usb_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct mtk_charger *gm;
	struct mtk_usb_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gm = (struct mtk_charger *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_usb_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(gm, usb_attr, val);

	return count;
}

static ssize_t usb_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct mtk_charger *gm;
	struct mtk_usb_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gm = (struct mtk_charger *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_usb_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(gm, usb_attr, &val);

	if (usb_attr->prop == USB_PROP_REAL_TYPE) {
		count = scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
		return count;
	} else if (usb_attr->prop == USB_PROP_TYPEC_MODE) {
		count = scnprintf(buf, PAGE_SIZE, "%s\n", get_typec_mode_name(val));
		return count;
	}

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

static struct mtk_usb_sysfs_field_info usb_sysfs_field_tbl[] = {
	USB_SYSFS_FIELD_RW(real_type, USB_PROP_REAL_TYPE),
	USB_SYSFS_FIELD_RO(quick_charge_type, USB_PROP_QUICK_CHARGE_TYPE),
	USB_SYSFS_FIELD_RW(pd_authentication, USB_PROP_PD_AUTHENTICATION),
	USB_SYSFS_FIELD_RW(pd_verifying, USB_PROP_PD_VERIFYING),
	USB_SYSFS_FIELD_RO(pd_type, USB_PROP_PD_TYPE),
	USB_SYSFS_FIELD_RW(apdo_max, USB_PROP_APDO_MAX),
	USB_SYSFS_FIELD_RW(typec_mode, USB_PROP_TYPEC_MODE),
	USB_SYSFS_FIELD_RW(typec_cc_orientation, USB_PROP_TYPEC_CC_ORIENTATION),
	USB_SYSFS_FIELD_RO(ffc_enable, USB_PROP_FFC_ENABLE),
	USB_SYSFS_FIELD_RO(charge_full, USB_PROP_CHARGE_FULL),
	USB_SYSFS_FIELD_RW(connector_temp, USB_PROP_CONNECTOR_TEMP),
	USB_SYSFS_FIELD_RO(typec_burn, USB_PROP_TYPEC_BURN),
	USB_SYSFS_FIELD_RO(sw_cv, USB_PROP_SW_CV),
	USB_SYSFS_FIELD_RW(input_suspend, USB_PROP_INPUT_SUSPEND),
	USB_SYSFS_FIELD_RO(jeita_chg_index, USB_PROP_JEITA_CHG_INDEX),
	USB_SYSFS_FIELD_RO(power_max, USB_PROP_POWER_MAX),
	USB_SYSFS_FIELD_RW(qc3_type, USB_PROP_QC3_TYPE),
	USB_SYSFS_FIELD_RW(otg_enable, USB_PROP_OTG_ENABLE),
	USB_SYSFS_FIELD_RW(pd_verify_done, USB_PROP_PD_VERIFY_DONE),
	USB_SYSFS_FIELD_RO(cp_ibus_delta, USB_PROP_CP_IBUS_DELTA),
	USB_SYSFS_FIELD_RW(mtbf_test, USB_PROP_MTBF_TEST),
	USB_SYSFS_FIELD_RO(cp_charge_recovery, USB_PROP_CP_CHARGE_RECOVERY),
	USB_SYSFS_FIELD_RO(pmic_ibat, USB_PROP_PMIC_IBAT),
	USB_SYSFS_FIELD_RO(pmic_vbus, USB_PROP_PMIC_VBUS),
	USB_SYSFS_FIELD_RO(input_current_now, USB_PROP_INPUT_CURRENT_NOW),
	USB_SYSFS_FIELD_RW(battcont_online, USB_PROP_BATTCONT_ONLINE),
	USB_SYSFS_FIELD_RO(warm_term, USB_PROP_WARM_TERM),
	USB_SYSFS_FIELD_RW(sconfig, USB_PROP_SCONFIG),
	USB_SYSFS_FIELD_RO(div_jeita_fcc_flag, USB_PROP_DIV_JEITA_FCC_FLAG),
	USB_SYSFS_FIELD_RO(jeita_chg_fcc, USB_PROP_JEITA_CHG_FCC),
	USB_SYSFS_FIELD_RO(source_jeita_fcc, USB_PROP_SOURCE_JEITA_CHG_FCC),
	USB_SYSFS_FIELD_RO(real_full, USB_PROP_REAL_FULL),
};

int usb_get_property(enum usb_property bp,
			    int *val)
{
	struct mtk_charger *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("usb");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (usb_sysfs_field_tbl[bp].prop == bp)
		usb_sysfs_field_tbl[bp].get(gm,
			&usb_sysfs_field_tbl[bp], val);
	else {
		chr_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(usb_get_property);

int usb_set_property(enum usb_property bp,
			    int val)
{
	struct mtk_charger *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("usb");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct mtk_charger *)power_supply_get_drvdata(psy);

	if (usb_sysfs_field_tbl[bp].prop == bp)
		usb_sysfs_field_tbl[bp].set(gm,
			&usb_sysfs_field_tbl[bp], val);
	else {
		chr_err("%s usb bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}
	return 0;
}
EXPORT_SYMBOL(usb_set_property);

static struct attribute *
	usb_sysfs_attrs[ARRAY_SIZE(usb_sysfs_field_tbl) + 1];

static const struct attribute_group usb_sysfs_attr_group = {
	.attrs = usb_sysfs_attrs,
};

static void usb_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(usb_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		usb_sysfs_attrs[i] = &usb_sysfs_field_tbl[i].attr.attr;

	usb_sysfs_attrs[limit] = NULL;
}

static int usb_sysfs_create_group(struct power_supply *psy)
{
	usb_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&usb_sysfs_attr_group);
}

static void usbpd_mi_vdm_received_cb(struct mtk_charger *pinfo, struct tcp_ny_uvdm uvdm)
{
	int i, cmd;
	if (uvdm.uvdm_svid != USB_PD_MI_SVID)
		return;
	cmd = UVDM_HDR_CMD(uvdm.uvdm_data[0]);
	chr_err("cmd = %d\n", cmd);
	chr_err("uvdm.ack: %d, uvdm.uvdm_cnt: %d, uvdm.uvdm_svid: 0x%04x\n",
			uvdm.ack, uvdm.uvdm_cnt, uvdm.uvdm_svid);
	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		pinfo->pd_adapter->vdm_data.ta_version = uvdm.uvdm_data[1];
		chr_err("ta_version:%x\n", pinfo->pd_adapter->vdm_data.ta_version);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		pinfo->pd_adapter->vdm_data.ta_temp = (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		chr_err("pinfo->pd_adapter->vdm_data.ta_temp:%d\n", pinfo->pd_adapter->vdm_data.ta_temp);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		pinfo->pd_adapter->vdm_data.ta_voltage = (uvdm.uvdm_data[1] & 0xFFFF) * 10;
		pinfo->pd_adapter->vdm_data.ta_voltage *= 1000;
		chr_err("ta_voltage:%d\n", pinfo->pd_adapter->vdm_data.ta_voltage);
		break;
	case USBPD_UVDM_SESSION_SEED:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			pinfo->pd_adapter->vdm_data.s_secert[i] = uvdm.uvdm_data[i+1];
			chr_err("usbpd s_secert uvdm.uvdm_data[%d]=0x%x", i+1, uvdm.uvdm_data[i+1]);
		}
		break;
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			pinfo->pd_adapter->vdm_data.digest[i] = uvdm.uvdm_data[i+1];
			chr_err("usbpd digest[%d]=0x%x", i+1, uvdm.uvdm_data[i+1]);
		}
		break;
	case USBPD_UVDM_REVERSE_AUTHEN:
		pinfo->pd_adapter->vdm_data.reauth = (uvdm.uvdm_data[1] & 0xFFFF);
		break;
	default:
		break;
	}
	pinfo->pd_adapter->uvdm_state = cmd;
}

int chg_alg_event(struct notifier_block *notifier,
			unsigned long event, void *data)
{
	chr_err("%s: evt:%d\n", __func__, event);

	return NOTIFY_DONE;
}

static char *mtk_charger_supplied_to[] = {
	"battery"
};

static char *mtk_usb_supplied_to[] = {
        "battery",
        "usb",
};

static struct regmap *pmic_get_regmap(const char *name)
{
        struct device_node *np;
        struct platform_device *pdev;

        np = of_find_node_by_name(NULL, name);
        if (!np) {
               chr_err("%s: device node %s not found!\n", __func__, name);
               return NULL;
        }
        pdev = of_find_device_by_node(np->child);
        if (!pdev) {
               chr_err("%s: mt6368 platform device not found!\n", __func__);
               return NULL;
        }
        return dev_get_regmap(pdev->dev.parent, NULL);
}

int mtk_set_mt6373_moscon1(struct mtk_charger *info, bool en, int drv_sel)
{
        if(en)
                return regmap_set_bits(info-> mt6373_regmap, MT6368_STRUP_ANA_CON1,
                               (en << 1 | drv_sel << 2));
        else
               return regmap_clear_bits(info-> mt6373_regmap, MT6368_STRUP_ANA_CON1, 0x6);

}
EXPORT_SYMBOL(mtk_set_mt6373_moscon1);

static const struct of_device_id mtk_charger_of_match[] = {
	{.compatible = "mediatek,charger",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_charger_of_match);

static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger *info = NULL;
	struct mtk_battery *battery_drvdata;

	int i, ret = 0;
	char *name = NULL;
	struct netlink_kernel_cfg cfg = {
		.input = chg_nl_data_handler,
	};

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	pinfo = info;
	platform_set_drvdata(pdev, info);
	info->pdev = pdev;
	info->diff_fv_val = 0;
	info->smart_batt_reduceXMv = 0;
	info->disable_te_count = 0;
	info->mt6363_auxadc4_enable = false;
	info->salve_conn_connected = false;
	info->battcont_online_adc = 0;
	info->high_temp_rec_soc = 0;
	info->battcont_disconnected = 0;
	info->div_jeita_fcc_flag = false;
	info->ov_check_only_once = false;
	info->cycle_count_reduceXMv = 0;

#ifdef CONFIG_FACTORY_BUILD
	info->night_charging = true;
#else
	info->night_charging = false;
#endif

	info->battery_psy = power_supply_get_by_name("battery");

	if (!info->battery_psy) {
		info->smart_chg = devm_kzalloc(&pdev->dev, sizeof(info->smart_chg)*(SMART_CHG_FEATURE_MAX_NUM+1), GFP_KERNEL);
	} else if (!info->smart_chg){
		battery_drvdata = power_supply_get_drvdata(info->battery_psy);
		info->smart_chg = battery_drvdata->smart_chg;
		info->charger_mode = battery_drvdata->sport_mode;
	}

	charger_parse_cmdline(info);

	mtk_charger_parse_dt(info, &pdev->dev);
	info->sic_current = info->max_fcc;
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
	info->charger_wakelock =
		wakeup_source_register(NULL, name);
	spin_lock_init(&info->slock);

	init_waitqueue_head(&info->wait_que);
	info->polling_interval = CHARGING_INTERVAL;
	mtk_charger_init_timer(info);
#ifdef CONFIG_PM
	info->pm_notifier.notifier_call = charger_pm_event;
#endif
	srcu_init_notifier_head(&info->evt_nh);

	info->chg_nb.notifier_call = charger_notifier_event;
	charger_reg_notifier(&info->chg_nb);

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
	info->psy_desc1.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc1.get_property = psy_charger_get_property;
	info->psy_desc1.set_property = psy_charger_set_property;
	info->psy_desc1.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_desc1.external_power_changed =
		mtk_charger_external_power_changed;
	info->psy_cfg1.drv_data = info;
	info->psy_cfg1.supplied_to = mtk_charger_supplied_to;
	info->psy_cfg1.num_supplicants = ARRAY_SIZE(mtk_charger_supplied_to);
	info->psy1 = power_supply_register(&pdev->dev, &info->psy_desc1,
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
		chr_err("register psy1 fail:%d\n",
			PTR_ERR(info->psy1));

	info->psy_desc2.name = "mtk-slave-charger";
	info->psy_desc2.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc2.properties = charger_psy_properties;
	info->psy_desc2.num_properties = ARRAY_SIZE(charger_psy_properties);
	info->psy_desc2.get_property = psy_charger_get_property;
	info->psy_desc2.set_property = psy_charger_set_property;
	info->psy_desc2.property_is_writeable =
			psy_charger_property_is_writeable;
	info->psy_cfg2.drv_data = info;
	info->psy2 = power_supply_register(&pdev->dev, &info->psy_desc2,
			&info->psy_cfg2);

	if (IS_ERR(info->psy2))
		chr_err("register psy2 fail:%d\n",
			PTR_ERR(info->psy2));

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
		chr_err("register psy dvchg1 fail:%d\n",
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
		chr_err("register psy dvchg2 fail:%d\n",
			PTR_ERR(info->psy_dvchg2));

    info->usb_desc.name = "usb";
	info->usb_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->usb_desc.properties = mt_usb_properties;
	info->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	info->usb_desc.get_property = mt_usb_get_property;
	info->usb_desc.external_power_changed = mtk_charger_external_power_usb_changed;
	info->usb_cfg.supplied_to = mtk_usb_supplied_to;
	info->usb_cfg.num_supplicants = ARRAY_SIZE(mtk_usb_supplied_to);
	info->usb_cfg.drv_data = info;

	info->usb_psy = power_supply_register(&pdev->dev,
		&info->usb_desc, &info->usb_cfg);
	if (IS_ERR(info->usb_psy))
		chr_err("register psy usb fail:%d\n",
			PTR_ERR(info->usb_psy));
	else
        usb_sysfs_create_group(info->usb_psy);

	info->log_level = CHRLOG_ERROR_LEVEL;
    info->flag = 0;
	info->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!info->pd_adapter)
		chr_err("%s: No pd adapter found\n");
	else {
		info->pd_nb.notifier_call = notify_adapter_event;
		register_adapter_device_notifier(info->pd_adapter,
						 &info->pd_nb);
		chr_err("register adapter ok\n");
	}

	alarm_init(&info->rust_det_work_timer, ALARM_BOOTTIME,rust_det_work_timer_handler);
	INIT_DELAYED_WORK(&info->hrtime_otg_work, hrtime_otg_work_func);
	INIT_DELAYED_WORK(&info->set_cc_drp_work, set_cc_drp_work_func);
	info->cid_status = false;

	info->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!info->tcpc)
		chr_err("get tcpc dev failed\n");
	else {
		info->tcpc_rust_det_nb.notifier_call = rust_det_notifier_call;
		register_tcp_dev_notifier(info->tcpc,
						 &info->tcpc_rust_det_nb, TCP_NOTIFY_TYPE_ALL);
		chr_err("register tcpc_rust_det_nb ok\n");
	}

    ret = step_jeita_init(info, &pdev->dev);
	if (ret < 0) {
		info->jeita_support = false;
	} else
		info->jeita_support = true;

	info->sc.daemo_nl_sk = netlink_kernel_create(&init_net, NETLINK_CHG, &cfg);

	if (info->sc.daemo_nl_sk == NULL)
		chr_err("sc netlink_kernel_create error id:%d\n", NETLINK_CHG);
	else
		chr_err("sc_netlink_kernel_create success id:%d\n", NETLINK_CHG);
	sc_init(&info->sc);

	info->chg_alg_nb.notifier_call = chg_alg_event;

	info->fast_charging_indicator = 0;
	info->enable_meta_current_limit = 1;
	info->is_charging = false;
	info->pd_verifying = true;
	info->night_charge_enable = false;
	info->safety_timer_cmd = -1;
	info->mt6373_regmap = pmic_get_regmap("second_pmic");
	if(IS_ERR_OR_NULL(info->mt6373_regmap))
		 chr_err("%s: mt6368 regmap not found!\n", __func__);

	if (info != NULL && info->bootmode != 8 && info->bootmode != 9)
		mtk_charger_force_disable_power_path(info, CHG1_SETTING, true);

	info->charger_notifier.notifier_call = screen_state_for_charger_callback;
	ret = mi_disp_register_client(&info->charger_notifier);
	if (ret < 0) {
		chr_err("%s register screen state callback failed\n",__func__);
	}

	kthread_run(charger_routine_thread, info, "charger_thread");
	return 0;
}

static int mtk_charger_remove(struct platform_device *dev)
{
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
module_init(mtk_charger_init);

static void __exit mtk_charger_exit(void)
{
	platform_driver_unregister(&mtk_charger_driver);
}
module_exit(mtk_charger_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Driver");
MODULE_LICENSE("GPL");
