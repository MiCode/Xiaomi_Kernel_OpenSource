// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_pulse_charger.c
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

#define MAX_TOPOFF_CHARGING_TIME (3 * 60 * 60) /* 3 hours */

#define RECHARGE_OFFSET 150000 /* uV */
#define TOPOFF_VOLTAGE 4200000 /* uV */
#define CHG_FULL_CURRENT 150000 /* uA */

struct pcharger_data {
	int state;
	bool disable_charging;

	unsigned int total_charging_time;
	unsigned int cc_charging_time;
	unsigned int topoff_charging_time;
	unsigned int full_charging_time;
	struct timespec topoff_begin_time;
	struct timespec charging_begin_time;

	int recharge_offset; /* uv */
	int topoff_voltage; /* uv */
	int chg_full_current; /* uA */

};

enum pcharger_state_enum {
	CHR_CC,
	CHR_TOPOFF,
	CHR_BATFULL,
	CHR_ERROR
};

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

static void pchr_select_cv(struct mtk_charger *info)
{
	u32 constant_voltage;

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			info->setting.cv = info->sw_jeita.cv;
			return;
		}

	constant_voltage = info->data.battery_cv;
	info->setting.cv = constant_voltage;
	charger_dev_set_constant_voltage(info->chg1_dev,
		info->setting.cv);
}

static bool pchr_select_charging_current_limit(struct mtk_charger *info,
	struct chg_limit_setting *setting)
{
	struct charger_data *pdata;
	bool is_basic = false;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;

	pchr_select_cv(info);

	pdata = &info->chg_data[CHG1_SETTING];
	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
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
		&& (info->chr_type == POWER_SUPPLY_USB_TYPE_SDP ||
		info->chr_type == POWER_SUPPLY_USB_TYPE_CDP)
		) {
		pdata->input_current_limit = 100000; /* 100mA */
		is_basic = true;
		goto done;
	}

	if (info->chr_type == POWER_SUPPLY_USB_TYPE_SDP) {
		pdata->input_current_limit =
				info->data.usb_charger_current;
		/* it can be larger */
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;

	} else if (info->chr_type == POWER_SUPPLY_USB_TYPE_DCP) {
		pdata->input_current_limit =
			info->data.ac_charger_input_current;
		pdata->charging_current_limit =
			info->data.ac_charger_current;
	} else if (info->chr_type == POWER_SUPPLY_USB_TYPE_CDP) {
		pdata->input_current_limit =
			info->data.charging_host_charger_current;
		pdata->charging_current_limit =
			info->data.charging_host_charger_current;
		is_basic = true;
	}

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
			&& info->chr_type == POWER_SUPPLY_USB_TYPE_SDP)
			chr_debug("USBIF & STAND_HOST skip current check\n");
		else {
			if (info->sw_jeita.sm == TEMP_T0_TO_T1) {
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 350000;
			}
		}
	}

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
			pdata->charging_current_limit)

			pdata->charging_current_limit =
				pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
				pdata->thermal_charging_current_limit;
	} else
		info->setting.charging_current_limit1 = -1;

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
			pdata->input_current_limit)
			pdata->input_current_limit =
				pdata->thermal_input_current_limit;
			info->setting.input_current_limit1 =
				pdata->thermal_input_current_limit;
	} else
		info->setting.input_current_limit1 = -1;

	if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK ||
		info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 ||
		info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
		is_basic = false;

done:

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = 0;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = 0;

	chr_err("thermal:%d %d setting:%d %d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		info->chr_type, info->pd_type,
		info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		info->bootmode, is_basic);

	return is_basic;
}

static void linear_chg_turn_on_charging(struct mtk_charger *info)
{
	struct pcharger_data *algo_data = info->algo.algo_data;
	bool charging_enable = true;
	struct charger_data *pdata;

	pdata = &info->chg_data[CHG1_SETTING];

	if (algo_data->state == CHR_ERROR) {
		charging_enable = false;
		chr_err("[charger]Charger Error, turn OFF charging !\n");
#ifdef SUPPORT_BOOTMODE
	} else if ((get_boot_mode() == META_BOOT) ||
			((get_boot_mode() == ADVMETA_BOOT))) {
		charging_enable = false;
		chr_err("[charger]In meta or advanced meta mode, disable charging\n");
#endif
	} else {
		pchr_select_charging_current_limit(info, &info->setting);
		if (info->chg_data[CHG1_SETTING].charging_current_limit == 0) {
			charging_enable = false;
			chr_err("[charger]charging current is set 0mA, turn off charging\n");
		}
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);
		charger_dev_set_constant_voltage(info->chg1_dev,
			info->setting.cv);

	}
	charger_dev_enable(info->chg1_dev, charging_enable);
}


static int mtk_linear_chr_cc(struct mtk_charger *info)
{
	struct timespec time_now, charging_time;
	u32 vbat;
	struct pcharger_data *algo_data;

	pr_notice("%s time:%d %d %d %d\n", __func__,
		algo_data->total_charging_time,
		algo_data->cc_charging_time,
		algo_data->topoff_charging_time,
		algo_data->full_charging_time);

	algo_data = info->algo.algo_data;
	get_monotonic_boottime(&time_now);
	charging_time = timespec_sub(time_now, algo_data->charging_begin_time);

	algo_data->cc_charging_time = charging_time.tv_sec;
	algo_data->topoff_charging_time = 0;
	algo_data->total_charging_time = charging_time.tv_sec;

	/* discharge for 1 second and charge for 9 seconds */
	charger_dev_enable(info->chg1_dev, false);
	msleep(1000);

	vbat = get_battery_voltage(info) * 1000; /* uV */
	if (vbat > algo_data->topoff_voltage) {
		algo_data->state = CHR_TOPOFF;
		get_monotonic_boottime(&algo_data->topoff_begin_time);
		pr_notice("%s: enter TOPOFF mode on vbat = %d uV\n",
			__func__, vbat);
	}

	linear_chg_turn_on_charging(info);

	return 0;
}

static bool charging_full_check(struct mtk_charger *info)
{
	struct pcharger_data *algo_data = info->algo.algo_data;
	static u32 full_check_count;
	bool chg_full_status = false;
	int chg_current = get_battery_current(info) * 1000; /* uA */

	if (chg_current > algo_data->chg_full_current)
		full_check_count = 0;
	else {
		full_check_count++;
		if (full_check_count >= 6) {
			full_check_count = 0;
			chg_full_status = true;
		}
	}

	return chg_full_status;
}

static int mtk_linear_chr_topoff(struct mtk_charger *info)
{
	struct pcharger_data *algo_data = info->algo.algo_data;
	struct timespec time_now, charging_time, topoff_time;


	pr_notice("%s time:%d %d %d %d\n", __func__,
		algo_data->total_charging_time,
		algo_data->cc_charging_time,
		algo_data->topoff_charging_time,
		algo_data->full_charging_time);
	get_monotonic_boottime(&time_now);
	charging_time = timespec_sub(time_now, algo_data->charging_begin_time);
	topoff_time = timespec_sub(time_now, algo_data->topoff_begin_time);

	algo_data->cc_charging_time = 0;
	algo_data->topoff_charging_time = topoff_time.tv_sec;
	algo_data->total_charging_time = charging_time.tv_sec;

	linear_chg_turn_on_charging(info);

	if (algo_data->topoff_charging_time >= MAX_TOPOFF_CHARGING_TIME
	    || charging_full_check(info) == true) {
		algo_data->state = CHR_BATFULL;

		/* Disable charging */
		charger_dev_enable(info->chg1_dev, false);
		pr_notice("%s: disable charging\n", __func__);
	}

	return 0;
}

static int mtk_linear_chr_full(struct mtk_charger *info)
{
	struct pcharger_data *algo_data = info->algo.algo_data;
	u32 vbat;
	bool is_recharging = false;

	algo_data->total_charging_time = 0;
	algo_data->cc_charging_time = 0;
	algo_data->topoff_charging_time = 0;

	pr_notice("%s time:%d %d %d %d\n", __func__,
		algo_data->total_charging_time,
		algo_data->cc_charging_time,
		algo_data->topoff_charging_time,
		algo_data->full_charging_time);

	/*
	 * If CV is set to lower value by JEITA,
	 * Reset CV to normal value if temperture is in normal zone
	 */
	pchr_select_cv(info);
	info->polling_interval = CHARGING_FULL_INTERVAL;

	vbat = get_battery_voltage(info) * 1000; /* uV */
	if (info->enable_sw_jeita && info->sw_jeita.cv != 0) {
		if (vbat < (info->sw_jeita.cv - algo_data->recharge_offset))
			is_recharging = true;
	} else {
		if (vbat < (info->data.battery_cv - algo_data->recharge_offset))
			is_recharging = true;
	}

	if (is_recharging) {
		algo_data->state = CHR_CC;
		get_monotonic_boottime(&algo_data->charging_begin_time);
		pr_notice("battery recharging on vbat = %d uV\n", vbat);
		info->polling_interval = CHARGING_INTERVAL;
	}

	return 0;
}

static void pchr_disable_all_charging(struct mtk_charger *info)
{
	charger_dev_enable(info->chg1_dev, false);
}


static int mtk_linear_chr_err(struct mtk_charger *info)
{
	struct pcharger_data *algo_data = info->algo.algo_data;

	pr_notice("%s time:%d %d %d %d\n", __func__,
		algo_data->total_charging_time,
		algo_data->cc_charging_time,
		algo_data->topoff_charging_time,
		algo_data->full_charging_time);


	if (info->enable_sw_jeita) {
		if ((info->sw_jeita.sm == TEMP_BELOW_T0) ||
			(info->sw_jeita.sm == TEMP_ABOVE_T4))
			info->sw_jeita.error_recovery_flag = false;

		if ((info->sw_jeita.error_recovery_flag == false) &&
			(info->sw_jeita.sm != TEMP_BELOW_T0) &&
			(info->sw_jeita.sm != TEMP_ABOVE_T4)) {
			info->sw_jeita.error_recovery_flag = true;
			algo_data->state = CHR_CC;
			get_monotonic_boottime(&algo_data->charging_begin_time);
		}
	}

	algo_data->total_charging_time = 0;
	algo_data->cc_charging_time = 0;
	algo_data->topoff_charging_time = 0;

	pchr_disable_all_charging(info);
	return 0;
}


static int pchr_do_algorithm(struct mtk_charger *info)
{
	struct charger_data *pdata;
	bool is_basic = true;
	int ret;

	struct pcharger_data *algo_data;

	charger_dev_kick_wdt(info->chg1_dev);

	chr_err("%s: %d\n", __func__, algo_data->state);

	switch (algo_data->state) {
	case CHR_CC:
		ret = mtk_linear_chr_cc(info);
		break;

	case CHR_TOPOFF:
		ret = mtk_linear_chr_topoff(info);
		break;

	case CHR_BATFULL:
		ret = mtk_linear_chr_full(info);
		break;

	case CHR_ERROR:
		ret = mtk_linear_chr_err(info);
		break;
	}

	if (is_basic == true) {
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);
		charger_dev_set_constant_voltage(info->chg1_dev,
			info->setting.cv);
	}

	charger_dev_dump_registers(info->chg1_dev);
	return 0;
}

static void mtk_pulse_charger_parse_dt(struct mtk_charger *info,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;
	struct pcharger_data *algo_data;

	algo_data = info->algo.algo_data;

	if (of_property_read_u32(np, "recharge_offset", &val) >= 0) {
		algo_data->recharge_offset = val;
	} else {
		chr_err("use default RECHARGE_OFFSET: %d\n", RECHARGE_OFFSET);
		algo_data->recharge_offset = RECHARGE_OFFSET;
	}

	if (of_property_read_u32(np, "topoff_voltage", &val) >= 0) {
		algo_data->topoff_voltage = val;
	} else {
		chr_err("use default TOPOFF_VOLTAGE: %d\n", TOPOFF_VOLTAGE);
		algo_data->topoff_voltage = TOPOFF_VOLTAGE;
	}

	if (of_property_read_u32(np, "chg_full_current", &val) >= 0) {
		algo_data->chg_full_current = val;
	} else {
		chr_err("use default CHG_FULL_CURRENT: %d\n", CHG_FULL_CURRENT);
		algo_data->chg_full_current = CHG_FULL_CURRENT;
	}

}

static int mtk_linear_charging_do_charging(struct mtk_charger *info,
						bool en)
{
	struct pcharger_data *algo_data = info->algo.algo_data;

	pr_info("%s en:%d %s\n", __func__, en, info->algorithm_name);
	if (en) {
		algo_data->disable_charging = false;
		algo_data->state = CHR_CC;
		get_monotonic_boottime(&algo_data->charging_begin_time);

	} else {
		algo_data->disable_charging = true;
		algo_data->state = CHR_ERROR;

		pchr_disable_all_charging(info);
	}

	return 0;
}

int mtk_pulse_charger_init(struct mtk_charger *info)
{
	static struct pcharger_data pdata;

	pr_notice("%s\n", __func__);
	info->algo.do_algorithm = pchr_do_algorithm;
	info->algo.enable_charging = mtk_linear_charging_do_charging;
	//info->algo.do_event = charger_dev_event;
	info->algo.algo_data = &pdata;
	mtk_pulse_charger_parse_dt(info, &info->pdev->dev);
	pr_notice("%s end\n", __func__);

	return 0;
}



