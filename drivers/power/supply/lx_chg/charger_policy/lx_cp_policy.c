// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 LX Technology(Shanghai) Co., Ltd.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include "lx_charger_manager.h"
#include "lx_cp_policy.h"
#include "../fuelgauge/bq28z610.h"
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
#include "../charger_class/lx_fg_class.h"
#endif
#include "../lx_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG][PDM]"
#endif

static const char *sm_str[] = {
	"PM_STATE_CHECK_DEV",
	"PM_STATE_INIT",
	"PM_STATE_MEASURE_RCABLE",
	"PM_STATE_ENABLE_CHARGERPUMP",
	"PM_STATE_CHARGERPUMP_CC_CV",
	"PM_STATE_CHARGERPUMP_EXIT",
};

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int switch2_1_fcc_set = 2000;
static int switch1_1_fcc_set = 1000;
#endif

struct chargerpump_policy *g_policy;
EXPORT_SYMBOL_GPL(g_policy);


static const struct chargerpump_policy_config default_cfg = {
	.cv_offset =        50,
	.exit_cc =          2100,
	.min_vbat =         3500,
	.step_volt =        20,
	.warm_temp =        480,

	.max_request_volt = 10000,
	.min_request_volt = 3000,   
	.min_request_curr = 1800,
	.max_request_curr = 4000,   //3000

	.max_ibat =         8000,   //6000
	.max_vbat =         4520,

	.s_cp_enable =      false,
};

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static void init_cp_workmode(struct chargerpump_policy *policy)
{
	policy->switch2_1_enable = true;
	policy->switch2_1_single_enable = false;
	policy->switch1_1_enable = false;
	policy->switch1_1_single_enable = false;
	return;
}
static void usbpd_pm_disconnect(struct chargerpump_policy *policy)
{
	policy->switch_mode = false;
	policy->switch2_1_enable = false;
	policy->switch2_1_single_enable = false;
	policy->switch1_1_enable = false;
	policy->switch1_1_single_enable = false;
	policy->div_rate = 0;
	policy->cp_charge_done = false;
	policy->initial_vbus_flag = false;
	return;
}

static void pdm_multi_mode_switch(struct chargerpump_policy *policy)
{
   struct chg_info *info = NULL;
   int ibat_request;

   if (policy == NULL)
		return;

	info = &policy->info;
	ibat_request = info->total_fcc;

	lx_info("ibat_request = %d, 2_1 mode = %d, 1_1 mode = %d, soc = %d\n",
			ibat_request, policy->switch2_1_enable, policy->switch1_1_enable, info->soc);

	if (info->soc > 90) {
		if (policy->switch1_1_enable) {
			policy->switch1_1_enable = false;
			policy->switch2_1_enable = true;
		}

		goto STOP_DETECT;
	}

	if (policy->switch2_1_enable)
	{
		if (ibat_request <= switch1_1_fcc_set)
		{
			policy->switch_mode = true;
			policy->switch2_1_enable = false;
			policy->switch1_1_enable = true;
			policy->div_rate = 1;
		} else {
			policy->switch_mode = false;
			policy->div_rate = 2;
		}
	} else if (policy->switch1_1_enable) {
		if (ibat_request >= switch2_1_fcc_set)
		{
			policy->switch_mode = true;
			policy->switch2_1_enable = true;
			policy->switch1_1_enable = false;
			policy->div_rate = 2;
		} else {
			policy->switch_mode = false;
			policy->div_rate = 1;
		}
	} else {
				policy->switch_mode = false;
				policy->switch2_1_enable = false;
				policy->switch1_1_enable = false;
				policy->div_rate = 0;
	}
STOP_DETECT:
	return;
}

static bool update_smart_chg_cp_workmode(struct chargerpump_policy *policy)
{
	struct power_supply *psy = NULL;
	struct charger_manager *manager;

	chargerpump_get_cp_workmode(policy->master_cp, &policy->m_cp_mode);
	chargerpump_get_cp_workmode(policy->slave_cp, &policy->s_cp_mode);
	lx_debug("master cp mode = %d, slave cp mode = %d\n", policy->m_cp_mode, policy->s_cp_mode);

	psy = power_supply_get_by_name("battery");
	if (psy != NULL) {
		manager = (struct charger_manager *)power_supply_get_drvdata(psy);
		if (manager != NULL){
			policy->authenticate  = manager->authenticate;
			policy->ch = manager->ch;
			lx_debug("policy->authenticate = %d, policy->ch = %d\n", policy->authenticate , policy->ch);
#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
			policy->decrease_volt = manager->decrease_volt;
			lx_debug("policy->decrease_volt = %d\n", policy->decrease_volt);
#endif
#endif
			lx_debug("call monitor_smart_chg\n");
			if (manager->pps_fast_mode) {
				if (policy->switch2_1_enable && (!((policy->m_cp_mode == 0) && (policy->s_cp_mode == 0)))) {
					chargerpump_set_enable(policy->master_cp, false);
					chargerpump_set_enable(policy->slave_cp, false);
					msleep(1000);
					chargerpump_set_cp_workmode(policy->master_cp, 0);
					chargerpump_set_cp_workmode(policy->slave_cp, 0);
					lx_info("low_fast working, set switch2_1_enable\n");
				}
				lx_info("low_fast working, cp_mode not switch\n");
				return false;
			}
		} else {
			return false;
		}
	} else {
		return false;
	}

	if (policy->switch2_1_enable && (!((policy->m_cp_mode == 0) && (policy->s_cp_mode == 0)))) {
		chargerpump_set_enable(policy->master_cp, false);
		chargerpump_set_enable(policy->slave_cp, false);
		msleep(1000);
		chargerpump_set_cp_workmode(policy->master_cp, 0);
		chargerpump_set_cp_workmode(policy->slave_cp, 0);
		lx_info("set switch2_1_enable\n");
		return true;
	} else if (policy->switch1_1_enable && (!((policy->m_cp_mode == 1) && (policy->s_cp_mode == 1)))) {
		chargerpump_set_enable(policy->master_cp, false);
		chargerpump_set_enable(policy->slave_cp, false);
		msleep(1000);
		chargerpump_set_cp_workmode(policy->master_cp, 1);
		chargerpump_set_cp_workmode(policy->slave_cp, 1);
		lx_info("set switch1_1_enable\n");
		return true;
	} else if (policy->switch2_1_single_enable && (!(policy->m_cp_mode == 0))) {
		chargerpump_set_enable(policy->master_cp, false);
		chargerpump_set_enable(policy->slave_cp, false);
		msleep(1000);
		chargerpump_set_cp_workmode(policy->master_cp, 0);
		lx_info("set switch2_1_single_enable\n");
		return true;
	} else if (policy->switch1_1_single_enable && (!(policy->m_cp_mode == 1))) {
		chargerpump_set_enable(policy->master_cp, false);
		chargerpump_set_enable(policy->slave_cp, false);
		msleep(1000);
		chargerpump_set_cp_workmode(policy->master_cp, 1);
		lx_info("set switch1_1_single_enable\n");
		return true;
	}
	return false;
}
#endif

static int chargerpump_policy_set_state(struct chargerpump_policy *policy,
								enum policy_state state)
{
	mutex_lock(&policy->state_lock);
	policy->state = state;
	mutex_unlock(&policy->state_lock);
	return 0;
}

static int chargerpump_policy_wake_thread(struct chargerpump_policy *policy)
{
	policy->run_thread = true;
	wake_up(&policy->wait_queue);
	return 0;
}

static void chargerpump_policy_timer_func(struct timer_list *timer)
{
	struct chargerpump_policy *policy = container_of(timer,
							struct chargerpump_policy, policy_timer);
	chargerpump_policy_wake_thread(policy);
}

static int chargerpump_policy_start_timer(struct chargerpump_policy *policy, uint32_t ms)
{
	del_timer(&policy->policy_timer);
	if (ms < 0)
		return 0;
	policy->policy_timer.expires = jiffies + msecs_to_jiffies(ms);
	policy->policy_timer.function = chargerpump_policy_timer_func;
	add_timer(&policy->policy_timer);
	return 0;
}

static int chargerpump_policy_end_timer(struct chargerpump_policy *policy)
{
	del_timer(&policy->policy_timer);
	return 0;
}

static int chargerpump_policy_check_cp_enable(struct chargerpump_policy *policy)
{
	int ret;
	struct chg_info *info = &policy->info;

	ret = chargerpump_get_is_enable(policy->master_cp, &info->m_cp_chging);

	return ret;
}

static int chargerpump_policy_check_sec_cp_enable(struct chargerpump_policy *policy)
{
	int ret;
	struct chg_info *info = &policy->info;

	ret = chargerpump_get_is_enable(policy->slave_cp, &info->s_cp_chging);
	return ret;
}

static struct adapter_dev *chargerpump_policy_check_adapter(struct chargerpump_policy *policy);
static int chargerpump_policy_update_chg_info(struct chargerpump_policy *policy)
{
	struct power_supply *batt_psy = NULL;
	struct chg_info *info = &policy->info;
	struct chargerpump_policy_config *cfg = &policy->cfg;
	struct charger_manager *manager;

	union power_supply_propval pval = {0, };
	int ret = 0;

	if (policy->adapter == NULL) {
		policy->adapter = chargerpump_policy_check_adapter(policy);
		if (policy->adapter == NULL) {
			lx_err("can not find pd adapter\n");
			return -EINVAL;
		}
	}

	if (policy->charger == NULL) {
		policy->charger = charger_find_dev_by_name("primary_chg");
		if (policy->charger == NULL) {
			lx_err("can not find charger dev\n");
			return -EINVAL;
		}
	}

	if (policy->master_cp == NULL) {
		policy->master_cp = chargerpump_find_dev_by_name("master_cp_chg");
		if (policy->master_cp == NULL) {
			lx_err("can not find master_cp dev\n");
			return -EINVAL;
		}
	}

	if (policy->cfg.s_cp_enable) {
		if (policy->slave_cp == NULL) {
			policy->slave_cp = chargerpump_find_dev_by_name("slave_cp_chg");
			if (policy->slave_cp == NULL) {
				lx_err("can not find slave_cp dev\n");
				return -EINVAL;
			}
		}
	}

	if (policy->fg_psy == NULL) {
		policy->fg_psy = power_supply_get_by_name("bms");
		if (policy->fg_psy == NULL) {
			lx_err("cannot find fg psy\n");
			return -EINVAL;
		}
	}

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy == NULL) {
		lx_err("cannot find battery psy\n");
		return -EINVAL;
	}

	manager = (struct charger_manager *)power_supply_get_drvdata(batt_psy);
	if (manager == NULL) {
		lx_err("cannot find charger_manager\n");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (policy->fuel_gauge == NULL) {
		policy->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (!policy->fuel_gauge) {
			lx_err("can not find fuel_gauge device\n");
			return -EINVAL;
		}
	}
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	ret = power_supply_get_property(policy->fg_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		lx_err("failed to get soc\n");
		info->soc = 50;
	} else {
		info->soc = pval.intval;
		lx_debug("get soc = %d\n", info->soc);
	}
#endif

	ret = power_supply_get_property(policy->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0) {
		lx_err("failed to get ibat\n");
	}
	info->ibat = pval.intval / 1000;

	ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0) {
		lx_err("failed to get vbat\n");
	}
	info->vbat = pval.intval / 1000;

	ret = power_supply_get_property(policy->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		lx_err("failed to get tbat\n");
	}
	info->tbat = pval.intval;

	if(cfg->s_cp_enable){
		ret = chargerpump_get_adc_value(policy->master_cp, ADC_GET_IBUS, &(info->m_ibus));
		if (ret < 0) {
			lx_err("failed to get m_ibus\n");
		}
		ret = chargerpump_get_adc_value(policy->slave_cp, ADC_GET_IBUS, &(info->s_ibus));
		if (ret < 0) {
			lx_err("failed to get s_ibus\n");
		}
		info->ibus = info->m_ibus + info->s_ibus;
	} else {
		ret = chargerpump_get_adc_value(policy->master_cp, ADC_GET_IBUS, &(info->ibus));
		if (ret < 0) {
			lx_err("failed to get ibus\n");
		}
	}

	ret = charger_get_adc(policy->charger, ADC_GET_VBUS, &(info->vbus));
	if (ret < 0) {
		lx_err("failed to get vbus\n");
	}

	policy->fv_votable = find_votable("MAIN_FV");
	if (!policy->fv_votable)
		lx_err("find fv_votable voltable failed\n");

	policy->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!policy->total_fcc_votable)
		lx_err("find TOTAL_FCC voltable failed\n");

	policy->main_fcc_votable = find_votable("MAIN_FCC");
	if (!policy->main_fcc_votable)
		lx_err("find MAIN_FCC voltable failed\n");
	
	policy->main_icl_votable = find_votable("MAIN_ICL");
	if (!policy->main_icl_votable)
		lx_err("find MAIN_ICL voltable failed\n");

	policy->iterm_votable = find_votable("MAIN_ITERM");
	if (!policy->iterm_votable)
		lx_err("find MAIN_FCC voltable failed\n");

	ret = get_effective_result(policy->fv_votable);
	if (ret < 0) {
		lx_err("failed to get fv_votable\n");
	}
	lx_info("%s:get_vote_fv=%d, smart_batt=%d, smart_fv=%d, ffc_cv=%d\n", __func__, ret, manager->smart_batt, manager->smart_fv, info->ffc_cv);
	info->ffc_cv = ret - max(manager->smart_batt, manager->smart_fv);

	ret = get_effective_result(policy->total_fcc_votable);
	if (ret < 0) {
		lx_err("failed to get total_fcc\n");
	}
	info->total_fcc = ret;

	ret = !get_effective_result(policy->main_icl_votable);
	if (ret < 0) {
		lx_err("failed to get main_chg_disable\n");
	}
	info->main_chg_disable = ret;

	ret = get_effective_result(policy->iterm_votable);
	if (ret < 0) {
		lx_err("failed to get v_iterm vote\n");
	}
	info->v_iterm = ret;

	if (cfg->s_cp_enable) {
		chargerpump_get_is_enable(policy->master_cp, &info->m_cp_chging);
		chargerpump_get_is_enable(policy->slave_cp, &info->s_cp_chging);
	} else {
		chargerpump_get_is_enable(policy->master_cp, &info->m_cp_chging);
	}

	if (info->v_iterm > CHARGERPUMP_POLICY_MAIN_CHG_ITERM_MAX)
		cfg->exit_cc = info->v_iterm + CHARGERPUMP_POLICY_EXIT_CC_OFFSET;
	else
		cfg->exit_cc = 2100;

	adapter_get_temp(policy->adapter, &info->tadapter);

	info->adapter_soft_reset = adapter_get_softreset(policy->adapter);

	return 0;
}

static int chargerpump_policy_state_init(struct chargerpump_policy *policy)
{
	struct chg_info *info = &policy->info;
	struct chargerpump_policy_config *cfg = &policy->cfg;

	lx_info("total_fcc = %d, exit_cc = %d, main_chg_disable = %d\n", info->total_fcc, cfg->exit_cc, info->main_chg_disable);
	if (info->soc > CHARGERPUMP_POLICY_HIGH_SOC
#if !IS_ENABLED(FACTORY_BUILD) && IS_ENABLED(CONFIG_RUST_DETECTION)
	|| (policy->decrease_volt == 1)
#endif
		) {
		lx_err("soc too high or lpd decrease_volt true\n");
		policy->next_time = 50;
		policy->recover = false;
		policy->sm = PM_STATE_CHARGERPUMP_EXIT;
	} else if (info->total_fcc < cfg->exit_cc || info->main_chg_disable || info->total_fcc == -EINVAL) {
		lx_info("total fcc too low or input_suspend or night_charging is true\n");
		policy->next_time = 1000;
	} else {
		policy->sm = PM_STATE_MEASURE_RCABLE;
		policy->next_time = 50;
	}

	return 0;
}

static int chargerpump_policy_state_measure_rcable(struct chargerpump_policy *policy)
{
	rerun_election(policy->total_fcc_votable);
	policy->sm = PM_STATE_ENABLE_CHARGERPUMP;
	policy->next_time = 50;
	return 0;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static bool check_cp_enable_for_switch_mode(struct chargerpump_policy *policy)
{
	lx_info("switch enable=%d %d, %d %d\n", policy->switch2_1_enable, policy->switch1_1_enable, policy->switch2_1_single_enable, policy->switch1_1_single_enable);
	if ((policy->switch2_1_enable || policy->switch1_1_enable)) {
		chargerpump_policy_check_cp_enable(policy);
		if (!policy->info.m_cp_chging) {
			chargerpump_set_enable(policy->master_cp, true);
			chargerpump_policy_check_cp_enable(policy);
		}

		if(policy->cfg.s_cp_enable) {
			chargerpump_policy_check_sec_cp_enable(policy);
			if (!policy->info.s_cp_chging) {
				chargerpump_set_enable(policy->slave_cp, true);
				chargerpump_policy_check_sec_cp_enable(policy);
			}
		}
		if (policy->info.m_cp_chging) {
			if((policy->cfg.s_cp_enable && policy->info.s_cp_chging) || !policy->cfg.s_cp_enable) {
				return true;
			}
		}
		return false;
	} else if (policy->switch2_1_single_enable || policy->switch1_1_single_enable) {
		chargerpump_policy_check_cp_enable(policy);
		if (!policy->info.m_cp_chging) {
			chargerpump_set_enable(policy->master_cp, true);
			chargerpump_policy_check_cp_enable(policy);
		}
		if(policy->cfg.s_cp_enable) {
			chargerpump_policy_check_sec_cp_enable(policy);
			if (policy->info.s_cp_chging) {
				chargerpump_set_enable(policy->slave_cp, false);
				chargerpump_policy_check_sec_cp_enable(policy);
			}
		}
		if (policy->info.m_cp_chging) {
			if((policy->cfg.s_cp_enable && !policy->info.s_cp_chging) || !policy->cfg.s_cp_enable) {
				return true;
			}
		}
		return false;
	}
	return false;
}

static int chargerpump_policy_state_enable_chargerpump(struct chargerpump_policy *policy)
{
	struct chg_info *info = &policy->info;
	struct chargerpump_policy_config *cfg = &policy->cfg;
	static int cnt = 0;
	uint32_t status = 0;
	uint32_t sec_status = 0;
	int chip_id = 0;
	bool get_cp_en = 0;
    
	chargerpump_get_chip_id(policy->master_cp, &chip_id);
	if (policy->switch2_1_enable || policy->switch2_1_single_enable) {
		if (!policy->initial_vbus_flag) {
			if (chip_id == CP_DEV_ID_NU2115)
				policy->request_volt = info->vbat * 200 / 100 + 300;
			else
				policy->request_volt = info->vbat * 200 / 100 + 300;

			policy->initial_vbus_flag = true;
		}
	} else if (policy->switch1_1_enable || policy->switch1_1_single_enable) {
		if (!policy->initial_vbus_flag) {
			if (chip_id == CP_DEV_ID_NU2115)
				policy->request_volt = info->vbat + 300;
			else
				policy->request_volt = info->vbat + 300;

			policy->initial_vbus_flag = true;
		}
	}
	policy->request_curr = CHARGERPUMP_POLICY_FIRST_REQUEST_CURR;

	adapter_set_cap(policy->adapter, policy->cap_nr, policy->request_volt,
									policy->request_curr);

	get_cp_en = check_cp_enable_for_switch_mode(policy);

	if (cnt++ > CHARGERPUMP_POLICY_ENABLE_RETRY_CNT) {
		lx_info("enable chargerpump failed\n");
		cnt = 0;
		chargerpump_set_enable(policy->master_cp, false);
		if(policy->cfg.s_cp_enable) {
			chargerpump_set_enable(policy->slave_cp, false);
		}
		policy->recover = false;
		policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		policy->next_time = 50;
		policy->cp_enable_fail = 1;
		goto out;
	}
	
	chargerpump_get_status(policy->master_cp, &status);
	if(cfg->s_cp_enable) {
		chargerpump_get_status(policy->slave_cp, &sec_status);
	}

	if ((status & CHARGERPUMP_ERROR_VBUS_HIGH) || (cfg->s_cp_enable && (sec_status & CHARGERPUMP_ERROR_VBUS_HIGH))) {
		lx_info("vbus too high\n");
		policy->request_volt -= 100;
		policy->next_time = 100;
		chargerpump_set_enable(policy->master_cp, false);
		if(policy->cfg.s_cp_enable) {
			chargerpump_set_enable(policy->slave_cp, false);
		}
		policy->sm = PM_STATE_ENABLE_CHARGERPUMP;
		goto out;
	} else if((status & CHARGERPUMP_ERROR_VBUS_LOW) || (cfg->s_cp_enable && (sec_status & CHARGERPUMP_ERROR_VBUS_LOW))) {
		lx_info("%s vbus too low\n", __func__);
		policy->request_volt += 100;
		policy->next_time = 100;
		chargerpump_set_enable(policy->master_cp, false);
		if(policy->cfg.s_cp_enable) {
			chargerpump_set_enable(policy->slave_cp, false);
		}
		policy->sm = PM_STATE_ENABLE_CHARGERPUMP;
		goto out;
	}

	if (get_cp_en) {
		policy->sm = PM_STATE_CHARGERPUMP_CC_CV;
		policy->next_time = 50;
		policy->initial_vbus_flag = false;
		cnt = 0;
		charger_set_term(policy->charger, false);
		lx_info("enable cp successful!\n");
		policy->cp_enable_fail = 0;
	}
out:
	return 0;
}
#else
static int chargerpump_policy_state_enable_chargerpump(struct chargerpump_policy *policy)
{
	struct chg_info *info = &policy->info;
	struct chargerpump_policy_config *cfg = &policy->cfg;
	static int cnt = 0;
	uint32_t status = 0;
	uint32_t sec_status = 0;
	int chip_id = 0;

	chargerpump_get_chip_id(policy->master_cp, &chip_id);

	chargerpump_policy_check_cp_enable(policy);
	if (!info->m_cp_chging) {
		chargerpump_set_enable(policy->master_cp, true);
		chargerpump_policy_check_cp_enable(policy);
	}

	if (cfg->s_cp_enable) {
		chargerpump_policy_check_sec_cp_enable(policy);
		if (!info->s_cp_chging) {
			chargerpump_set_enable(policy->slave_cp, true);
			chargerpump_policy_check_sec_cp_enable(policy);
		}
	}

	if (info->m_cp_chging) {
		if((cfg->s_cp_enable && info->s_cp_chging) || !cfg->s_cp_enable) {
			policy->sm = PM_STATE_CHARGERPUMP_CC_CV;
			policy->next_time = 50;
			cnt = 0;
			goto out;
		}
	}

	chargerpump_get_status(policy->master_cp, &status);
	if (cfg->s_cp_enable) {
		chargerpump_get_status(policy->slave_cp, &sec_status);
	}

	if (policy->request_volt <= CHARGERPUMP_POLICY_REQUEST_SAFE5V) {
		if (chip_id == CP_DEV_ID_NU2115) {
			policy->request_volt = info->vbat * 220 / 100;
		} else{
			policy->request_volt = info->vbat * 220 / 100;
		}
	} else if((status & CHARGERPUMP_ERROR_VBUS_HIGH) ||
				(cfg->s_cp_enable && (sec_status & CHARGERPUMP_ERROR_VBUS_HIGH))) {
		lx_info("vbus too high\n");
		policy->request_volt -= 100;
	} else {
		policy->request_volt += 100;
	}
	policy->request_curr = CHARGERPUMP_POLICY_FIRST_REQUEST_CURR;

	adapter_set_cap(policy->adapter, policy->cap_nr, policy->request_volt,
									policy->request_curr);

	policy->next_time = 100;

	if (cnt++ > CHARGERPUMP_POLICY_ENABLE_RETRY_CNT) {
		lx_info("enable chargerpump failed\n");
		cnt = 0;
		policy->next_time = CHARGERPUMP_POLICY_EXIT_THREAD_TIME;
		chargerpump_policy_set_state(policy, POLICY_STOP);
		goto out;
	}
	charger_set_term(policy->charger, false);

out:
	return 0;
}
#endif

static int __chargerpump_cc_cv_algo(struct chargerpump_policy *policy)
{
	int step = 0, step_vbat = 0, step_ibus = 0, step_ibat = 0;
	int ibus_lmt, vbat_lmt, ibat_lmt;
	struct chg_info *info = &policy->info;
	struct chargerpump_policy_config *cfg = &policy->cfg;
	static int iterm_cnt = 0;
	const uint32_t min_cv_mv = 4050;
	uint32_t final_cv;
	struct bq_fg_chip *bq = NULL;

	bq = fuel_gauge_get_private(policy->fuel_gauge);
	if (IS_ERR_OR_NULL(bq)){
		lx_err("failed to get fuel_gauge\n");
		return 0;
   }

	if ((info->tbat >= 151 && info->tbat <= 480) && (info->ffc_cv >= 4490))
		final_cv = info->ffc_cv - CHARGERPUMP_POLICY_FFC_CV_OFFSET;
	else
		final_cv = info->ffc_cv - CHARGERPUMP_POLICY_FFC_CV_OFFSET;

	if (final_cv < min_cv_mv)
		final_cv = min_cv_mv;

	// normal fast chg/ffc fast chg, end chargerpump judge ffc:4560-50=4510
	lx_info("vbat = %d, ibat = %d, exit_cc = %d, rsoc = %d\n",info->vbat, info->ibat, cfg->exit_cc, bq->rsoc);
	if ((info->vbat > (final_cv - cfg->cv_offset)) && (info->ibat < cfg->exit_cc)) {
		if (iterm_cnt++ > CHARGERPUMP_POLICY_END_CHG_CNT) {
			vote(policy->total_fcc_votable, MAIN_FCC_TAPER_VOTER, true, cfg->exit_cc);
			return PM_ALGO_RET_CHARGERPUMP_DONE;
		}
	} else {
		iterm_cnt = 0;
	}

	policy->fast_request = true;

	vbat_lmt = min(final_cv, cfg->max_vbat);
	ibus_lmt = min(cfg->max_ibat / 2, policy->cap.curr_max[policy->cap_nr]);

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if(fuel_gauge_check_i2c_function(policy->fuel_gauge)) {
		lx_err("%d:FG i2c error\n", __LINE__);
		return PM_ALGO_RET_FG_I2C_ERROR;
	}
#endif

	if (info->adapter_soft_reset) {
		return PM_ALGO_RET_SOFT_RESET;
	}

	if (info->main_chg_disable)
		return PM_ALGO_RET_INPUT_SUSPEND;

	if (info->total_fcc < CHARGERPUMP_POLICY_THERMAL_EXIT_CC)
		return PM_ALGO_RET_CP_LMT_CURR;
	else {
		ibat_lmt = min(info->total_fcc, cfg->max_ibat);
		ibat_lmt -= CHARGERPUMP_POLICY_MAIN_CHG_CURR;
	}

	// vbat loop
	if (info->vbat > vbat_lmt)
		step_vbat = -cfg->step_volt;
	else if (info->vbat < vbat_lmt - 7)
		step_vbat = cfg->step_volt;

	if (abs(info->vbat - vbat_lmt) < 200)
		policy->fast_request = false;

	// ibus loop mA
	if (info->ibus < ibus_lmt - 50) {
		step_ibus = cfg->step_volt;
	} else if (info->ibus > ibus_lmt - 40) {
		step_ibus = -cfg->step_volt;
	}

	if (abs(info->ibus - ibus_lmt) < 600)
		policy->fast_request = false;

	// ibat loop
	if (info->ibat < ibat_lmt - 100) {
		step_ibat = cfg->step_volt;
	} else if (info->ibat > ibat_lmt) {
		step_ibat = -cfg->step_volt;
	}

	if (abs(info->ibat - ibat_lmt) < 1200)
		policy->fast_request = false;

	if ((info->ibat == info->last_ibat) && (info->ibat < ibat_lmt)) {
		policy->fast_request = false;
		step_ibat = 0;
		lx_debug("fg has not been updated \n");
	}
	info->last_ibat = info->ibat;

	step = min(min(step_ibus, step_vbat), step_ibat);

	if (policy->fast_request)
		policy->request_volt += step * 5;
	else
		policy->request_volt += step;

	if (policy->request_volt >= policy->cap.volt_max[policy->cap_nr])
		policy->request_volt = policy->cap.volt_max[policy->cap_nr];

	policy->request_curr = ibus_lmt;

	lx_debug("cp algo fast_request = %d, request_volt = %d\n",
				policy->fast_request, policy->request_volt);
	lx_info("cp algo [vbat_lmt ibat_lmt ibus_lmt] %d ,%d ,%d\n",
				vbat_lmt, ibat_lmt, ibus_lmt);
	lx_debug("cp algo [step_vbat step_ibat step_ibus final_step] %d ,%d ,%d %d\n",
				step_vbat, step_ibat, step_ibus, step);
	
	adapter_set_cap(policy->adapter, policy->cap_nr, policy->request_volt,
									policy->request_curr);
	return PM_ALGO_RET_OK;
}

static int chargerpump_policy_state_chargerpump_cc_cv(struct chargerpump_policy *policy)
{
	struct chg_info *info = &policy->info;
	struct chargerpump_policy_config *cfg = &policy->cfg;
	int ret;

	ret = __chargerpump_cc_cv_algo(policy);

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (ret == PM_ALGO_RET_FG_I2C_ERROR) {
		policy->recover = false;
		policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		lx_err("%d:FG i2c error\n", __LINE__);
	}
#endif

	if (!info->m_cp_chging || (cfg->s_cp_enable && !info->s_cp_chging)) {
		policy->recover = true;
		policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		policy->next_time = 50;
		lx_info("master cp %d, exit cc charging\n", info->m_cp_chging);
		return 0;
	}

	if (ret == PM_ALGO_RET_SOFT_RESET) {
		policy->recover = true;
		policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		policy->next_time = 50;
		lx_info("soft reset.\n");
		return 0;
	}

	if (ret == PM_ALGO_RET_CHARGERPUMP_DONE) {
		policy->cp_charge_done = true;
		policy->recover = false;
		policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		lx_info("cp charge done, exit state machine.\n");
	}

	if (ret == PM_ALGO_RET_CP_LMT_CURR) {
		policy->recover = true;
		policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		policy->next_time = 50;
		lx_info("total fcc less than min fast chg limit, stop state machine.\n");
		return 0;
	}

	if (ret == PM_ALGO_RET_INPUT_SUSPEND) {
		policy->recover = true;
		policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		policy->next_time = 50;
		lx_info("input_suspend is true, stop state machine.\n");
		return 0;
	}
	policy->next_time = CHARGERPUMP_POLICY_LOOP_TIME;
	return 0;
}

static int dv_policy_state_chargerpump_exit(struct chargerpump_policy *policy)
{
	struct chg_info *info = &policy->info;
	struct chargerpump_policy_config *cfg = &policy->cfg;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc = NULL;

	tcpc = tcpc_dev_get_by_name("type_c_port0");
#endif
	if (policy->request_volt != CHARGERPUMP_POLICY_REQUEST_SAFE5V) {
		policy->request_volt = 9000;
	}
#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	if (policy->decrease_volt) {
		policy->request_volt = 5000;
		lx_info("lpd request_volt 5V\n");
	}
#endif
#endif

	if(!policy->authenticate || (policy->authenticate && !policy->ch) || !policy->master_cp) {
		policy->request_volt = 5000;
		if (tcpc)
			tcpm_dpm_pd_request(tcpc, 5000, 2000, NULL);
		lx_info("bms authenticate or cp adsent limit 5V\n");
	} else
		adapter_set_cap(policy->adapter, policy->cap_nr, policy->request_volt,
										policy->cap.curr_max[policy->cap_nr]);	

	lx_info("m_cp_chging=%d, s_cp_chging=%d\n", info->m_cp_chging, info->s_cp_chging);
	if (info->m_cp_chging) {
		chargerpump_set_enable(policy->master_cp, false);
		chargerpump_policy_check_cp_enable(policy);
		if(cfg->s_cp_enable && info->s_cp_chging){
			chargerpump_set_enable(policy->slave_cp, false);
			chargerpump_policy_check_sec_cp_enable(policy);
			goto out;
		}
		goto out;
	}
	
	if (policy->recover && policy->recover_cnt++ < CHARGERPUMP_POLICY_RECOVER_CNT) {
		policy->recover = false;
		policy->sm = PM_STATE_INIT;
		if (info->adapter_soft_reset) {
			policy->sm = PM_STATE_CHECK_DEV;
			adapter_set_softreset(policy->adapter, false);
			msleep(500);
		}
		goto out;
	}

	policy->next_time = CHARGERPUMP_POLICY_EXIT_THREAD_TIME;
	chargerpump_policy_set_state(policy, POLICY_STOP);
	rerun_election(policy->total_fcc_votable);
	rerun_election(policy->main_fcc_votable);
	msleep(500);
	charger_set_term(policy->charger, true);
	return 0;

out:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if(fuel_gauge_check_i2c_function(policy->fuel_gauge)) {
		lx_err("%d:FG i2c error\n", __LINE__);
		chargerpump_policy_set_state(policy, POLICY_STOP);
	}
#endif
	rerun_election(policy->total_fcc_votable);
	rerun_election(policy->main_fcc_votable);
	msleep(500);
	charger_set_term(policy->charger, true);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if(fuel_gauge_check_i2c_function(policy->fuel_gauge)) {
		policy->next_time = CHARGERPUMP_POLICY_EXIT_THREAD_TIME;
		lx_err("%d:FG i2c error\n", __LINE__);
	} else {
#endif
		policy->next_time = 50;
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	}
#endif
	return 0;
}

static int dv_policy_state_check_dev(struct chargerpump_policy *policy);

static int chargerpump_policy_thread_fn(void *data)
{
	struct chargerpump_policy *policy = data;
	struct chg_info *info = &policy->info;
	int ret = 0;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	bool need_switch = false;
#endif

	while (true) {
		ret = wait_event_interruptible(policy->wait_queue,
						   policy->run_thread);
		if (kthread_should_stop() || ret) {
			lx_err("exits(%d)\n", ret);
			break;
		}

		policy->run_thread = false;

		if (policy->state != POLICY_RUNNING)
			continue;

		mutex_lock(&policy->running_lock);
		chargerpump_policy_update_chg_info(policy);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
		pdm_multi_mode_switch(policy);
		need_switch = update_smart_chg_cp_workmode(policy);
		if (need_switch) {
			policy->sm = PM_STATE_INIT;
			lx_info("need switch cp mode\n");
		}
#endif
#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
		if (policy->decrease_volt)
			policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		lx_debug("decrease_volt %d\n",policy->decrease_volt);
#endif
#endif
		if(!policy->authenticate || (policy->authenticate && !policy->ch)) {
			policy->sm = PM_STATE_CHARGERPUMP_EXIT;
		}
		lx_info("state phase [%s]\n", sm_str[policy->sm]);
		lx_info("ibat = %d, vbat = %d, tbat = %d, total_fcc = %d, vbus = %d, ibus = %d\n",
					info->ibat, info->vbat, info->tbat, info->total_fcc, info->vbus, info->ibus);

		switch (policy->sm) {
			case PM_STATE_CHECK_DEV:
				dv_policy_state_check_dev(policy);
				break;
			case PM_STATE_INIT:
				chargerpump_policy_state_init(policy);
				break;
			case PM_STATE_MEASURE_RCABLE:
				chargerpump_policy_state_measure_rcable(policy);
				break;
			case PM_STATE_ENABLE_CHARGERPUMP:
				chargerpump_policy_state_enable_chargerpump(policy);
				break;
			case PM_STATE_CHARGERPUMP_CC_CV:
				chargerpump_policy_state_chargerpump_cc_cv(policy);
				break;
			case PM_STATE_CHARGERPUMP_EXIT:
				dv_policy_state_chargerpump_exit(policy);
				break;
			default:
				break;
		}

		chargerpump_policy_start_timer(policy, policy->next_time);

		mutex_unlock(&policy->running_lock);
	}
	return 0;
}

bool chargerpump_policy_check_adapter_cap(struct chargerpump_policy *policy,
										struct adapter_cap *cap)
{
	int i = 0;
	uint32_t max_pwr = 0, curr_pwr = 0, cap_nr = 0;
	struct chargerpump_policy_config *cfg = &policy->cfg;

	for (i = 0; i < cap->cnt; i++) {
		lx_info("max volt : %d, min volt : %d, max curr : %d, min curr : %d\n",
				cap->volt_max[i], cap->volt_min[i], cap->curr_max[i], cap->curr_min[i]);
		//ignore fix gear
		if (cap->volt_min[i] == cap->volt_max[i])
			continue;

		if (cap->volt_min[i] >= cfg->min_request_volt &&
			cap->curr_max[i] >= cfg->min_request_curr) {
			if (cap->volt_max[i] >= cfg->max_request_volt)
				max_pwr = cfg->max_request_volt * cap->curr_max[i] / 1000000;
			else
				max_pwr = cap->volt_max[i] * cap->curr_max[i] / 1000000;

			if (max_pwr < curr_pwr)
				continue;
			curr_pwr = max_pwr;
			cap_nr = i;
		}
	}

	policy->cap_nr = cap_nr;
	lx_info("the final choice: max volt : %d, min volt : %d, max curr : %d, min curr : %d\n",
				cap->volt_max[policy->cap_nr], cap->volt_min[policy->cap_nr],
				cap->curr_max[policy->cap_nr], cap->curr_min[policy->cap_nr]);
	if (!curr_pwr)
		return false;
	return true;
}

EXPORT_SYMBOL(chargerpump_policy_check_adapter_cap);

static struct adapter_dev *chargerpump_policy_check_adapter(struct chargerpump_policy *policy)
{
	int i = 0, ret = 0;
	struct adapter_dev *adapter = NULL;
	for (i = 0; i < ARRAY_SIZE(policy->adapter_name); i++) {
		adapter = NULL;
		if (policy->adapter_name[i] == NULL)
			continue;

		adapter = adapter_find_dev_by_name(policy->adapter_name[i]);
		if (adapter == NULL)
			continue;

		ret = adapter_handshake(adapter);
		if (ret < 0) {
			lx_err("%s adapter handshake failed\n", policy->adapter_name[i]);
			goto out;
		}

		ret = adapter_get_cap(adapter, &policy->cap);
		if (ret < 0) {
			lx_err("%s adapter get cap failed\n", policy->adapter_name[i]);
			goto out;
		}

		if (chargerpump_policy_check_adapter_cap(policy, &policy->cap)) {
			break;
		}
out:
		adapter_reset(adapter);
	}
	return adapter;
}

static int dv_policy_state_check_dev(struct chargerpump_policy *policy)
{
	struct chargerpump_policy_config *cfg = &policy->cfg;

	if (!policy->charger || !policy->master_cp ||
			!policy->adapter || (cfg->s_cp_enable && !policy->slave_cp)) {
		chargerpump_policy_set_state(policy, POLICY_NO_SUPPORT);
		goto out;
	}

	adapter_set_wdt(policy->adapter, 0);

	policy->sm = PM_STATE_INIT;
	policy->recover = false;
	policy->recover_cnt = 0;
	policy->next_time = 0;
	policy->request_volt = 0;
	policy->request_curr = 0;
	mutex_unlock(&policy->access_lock);
	return 0;

out:
	policy->next_time = -1;
	mutex_unlock(&policy->access_lock);
	return -EOPNOTSUPP;
}

int chargerpump_policy_start(struct chargerpump_policy *policy)
{
	lx_info("\n");
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (policy->fuel_gauge == NULL)
		policy->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");

	if (policy->fuel_gauge) {
		if (fuel_gauge_check_i2c_function(policy->fuel_gauge)) {
			lx_err("%d:FG i2c error\n", __LINE__);
			return 0;
		}
	}
#endif
	if (mutex_trylock(&policy->access_lock) == 0)
		return -EBUSY;
	policy->sm = PM_STATE_CHECK_DEV;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	init_cp_workmode(policy);
#endif
	chargerpump_policy_set_state(policy, POLICY_RUNNING);
	chargerpump_policy_wake_thread(policy);
	return 0;
}
EXPORT_SYMBOL(chargerpump_policy_start);

int chargerpump_policy_stop(struct chargerpump_policy *policy)
{
	mutex_lock(&policy->running_lock);
	mutex_unlock(&policy->access_lock);
	policy->request_volt = 0;
	chargerpump_policy_end_timer(policy);
	adapter_reset(policy->adapter);
	chargerpump_policy_set_state(policy, POLICY_NO_START);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	usbpd_pm_disconnect(policy);
#endif
	mutex_unlock(&policy->running_lock);
	return 0;
}
EXPORT_SYMBOL(chargerpump_policy_stop);

static int chargerpump_policy_probe(struct platform_device *pdev)
{
	struct chargerpump_policy *policy;

	lx_info("start\n");

	policy = devm_kzalloc(&pdev->dev, sizeof(*policy), GFP_KERNEL);
	if (!policy)
		return -ENOMEM;

	g_policy = policy;

	policy->dev = &pdev->dev;
	platform_set_drvdata(pdev, policy);
	init_waitqueue_head(&policy->wait_queue);
	memcpy(&policy->cfg, &default_cfg, sizeof(default_cfg));
	policy->thread = kthread_run(chargerpump_policy_thread_fn, policy,
							"chargerpump_policy_thread");

	//policy->adapter_name[0] = "vfcp adapter";
	//policy->adapter_name[1] = "ufcs_port2";
	policy->adapter_name[0] = "pd_adapter1";
#ifndef CONFIG_FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	policy->decrease_volt = 0;
#endif
#endif
	mutex_init(&policy->state_lock);
	mutex_init(&policy->running_lock);
	mutex_init(&policy->access_lock);

	lx_info("success\n");

	return 0;
}

static int chargerpump_policy_remove(struct platform_device *pdev)
{
	//struct chargerpump_policy *policy = platform_get_drvdata(pdev);

	return 0;
}

static const struct of_device_id chargerpump_policy_match[] = {
	{.compatible = "lx,lx_cp_policy",},
	{},
};
MODULE_DEVICE_TABLE(of, chargerpump_policy_match);

static struct platform_driver cp_policy = {
	.probe = chargerpump_policy_probe,
	.remove = chargerpump_policy_remove,
	.driver = {
		.name = "lx_cp_policy",
		.of_match_table = chargerpump_policy_match,
	},
};

module_platform_driver(cp_policy);
MODULE_DESCRIPTION("LX Charger Pump Policy Core");
MODULE_LICENSE("GPL v2");
