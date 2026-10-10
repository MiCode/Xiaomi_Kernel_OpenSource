// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 LX Technology(Shanghai) Co., Ltd.
 */

#include <linux/usb/typec.h>
#include "lx_charger_manager.h"
#include "lx_jeita.h"
#include "../lx_printk.h"
#include "../fuelgauge/charger_partition.h"
#include "../../../../../drivers/usb/typec/tcpc/inc/tcpci.h"
#include <linux/hrtimer.h>
#include <linux/pm_qos.h>
#include "../common/lx_notify.h"
#include "../fuelgauge/bq28z610.h"
#if IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
#include "../fuelgauge/lx_adaptive_poweroff_voltage.h"
#endif
#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG][CM]"
#endif

static int shutdown_delay_voltage = SHUTDOWN_DELAY_VOL_HIGH;
static int shutdown_force_voltage = SHUTDOWN_DELAY_VOL_LOW;
static int shutdown_delay_count = 0;
static bool full_flag = false;
static bool is_replenish_full = false;

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "../../../../thermal/thermal_core.h"
static void *cookie = NULL;
struct charger_manager *info = NULL;
static int screen_light = 0;
static struct pm_qos_request cp_qos_request;
#endif

extern int tcpc_class_complete_init(void);

static void lxchg_charge_status_update(struct charger_manager *manager);
static const char *chg_stat[] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not charging",
	"Full"
};

static int lx_charger_chain_notify(struct notifier_block *notifier, unsigned long event, void *data)
{
	struct charger_manager *manager =
		container_of(notifier, struct charger_manager, lx_charger_chain_nb);

#if IS_ENABLED(CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE)
	struct poweroff_voltage_config *poweroff_vol_conf;
	if (event == CHARGER_EVENT_SHUTDOWN_VOLTAGE_CHANGED) {
		poweroff_vol_conf = (struct poweroff_voltage_config *)data;
		if (IS_ERR_OR_NULL(poweroff_vol_conf)) {
			return NOTIFY_OK;
		}
		shutdown_force_voltage = poweroff_vol_conf->poweroff_voltage;
		shutdown_delay_voltage = poweroff_vol_conf->shutdown_delay_voltage;
		lx_err("%s get evt:%lu reset shutdown_force_voltage:%d shutdown_delay_voltage:%d \n",
				__func__,  event, shutdown_force_voltage, shutdown_delay_voltage);
	}
#endif /* CONFIG_LX_ADAPTIVE_POWEROFF_VOLTAGE */

	if (event == CHARGER_EVENT_BMS_AUTH_DONE) {
		manager->bms_auth_done = true;
		lx_info("bms auth done\n");
	}
	return NOTIFY_OK;
}

static int charger_manager_wake_thread(struct charger_manager *manager)
{
	manager->run_thread = true;
	wake_up(&manager->wait_queue);
	return 0;
}

int charger_manager_get_current(struct charger_manager *manager, int *curr)
{
	int val;
	int ret = 0;
	union power_supply_propval pval;

	*curr = 0;

	ret = charger_get_adc(manager->charger, ADC_GET_IBUS, &val);
	if (ret < 0) {
		lx_err("Couldn't read input curr ret=%d\n", ret);
	} else
		*curr += val;

	if(IS_ERR_OR_NULL(manager->cp_master_psy))
		lx_err("cp_master_psy is NULL.\n");
	else {
		ret = power_supply_get_property(manager->cp_master_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (ret < 0)
			lx_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
		else
			*curr += pval.intval;
	}

	if (manager->cp_slave_use) {
		if(IS_ERR_OR_NULL(manager->cp_slave_psy))
			lx_err("cp_slave_psy is NULL.\n");
		else {
			ret = power_supply_get_property(manager->cp_slave_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
			if (ret < 0)
				lx_err("Couldn't get cp curr  by power supply ret=%d\n", ret);
			else
				*curr += pval.intval;
		}
	}

	return 0;
}
EXPORT_SYMBOL(charger_manager_get_current);
void smblib_set_prop_batt_status(struct charger_manager *manager,  const union power_supply_propval *val)
{
	if (val->intval < 0) {
		manager->fake_batt_status = -EINVAL;
	} else {
		manager->fake_batt_status = val->intval;
	}

	//power_supply_changed(chg->batt_psy);
}
EXPORT_SYMBOL(smblib_set_prop_batt_status);
void lx_set_prop_system_temp_level(struct charger_manager *manager,  char *voter_name)
{
	int rc;

	if (manager->thermal_enable == false) {
		lx_err("thermal ibat limit is disable\n");
		return;
	}

	if (manager->system_temp_level < 0)
		goto err;

	if (manager->pd_active == CHARGE_PD_PPS_ACTIVE || !strcmp(voter_name, CALL_THERMAL_DAEMON_VOTER)) {
		if (manager->thermal_parse_flags & PD_THERM_PARSE_ERROR) {
			lx_err("pd thermal dtsi parse error\n");
			goto err;
		}
		if (manager->system_temp_level > manager->pd_thermal_levels) {
			lx_err("system_temp_level is invalid\n");
			goto err;
		}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
		if (manager->pps_fast_mode && (manager->low_fast_ffc >= 2150)) {
			vote(manager->total_fcc_votable, voter_name, true, manager->low_fast_ffc);
		} else {
			vote(manager->total_fcc_votable, voter_name, true,
					manager->pd_thermal_mitigation[manager->system_temp_level]);
		}
#else
		vote(manager->total_fcc_votable, voter_name, true,
				manager->pd_thermal_mitigation[manager->system_temp_level]);
#endif
	} else {
		if (manager->thermal_parse_flags & QC2_THERM_PARSE_ERROR) {
			lx_err("qc thermal dtsi parse error\n");
			goto err;
		}
		if (manager->system_temp_level > manager->qc2_thermal_levels) {
			lx_err("system_temp_level is invalid\n");
			goto err;
		}
		vote(manager->total_fcc_votable, voter_name, true,
			manager->qc2_thermal_mitigation[manager->system_temp_level]);
	}

	rc = get_client_vote_locked(manager->total_fcc_votable, voter_name);
	lx_info("%s: thermal vote susessful val = %d, current = %d\n", voter_name, manager->system_temp_level, rc);
	rerun_election(manager->total_fcc_votable);
	return;
err:
	vote(manager->total_fcc_votable, voter_name, false, 0);
	return;
}
EXPORT_SYMBOL(lx_set_prop_system_temp_level);
#ifndef FACTORY_BUILD
void manual_set_cc_toggle(bool en)
{
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	int ret = 0;
	if(info == NULL)
		return;
	if(info->tcpc == NULL)
		return;
	info->ui_cc_toggle = en;

	if(!info->typec_attach && en)
	{
		lx_err("typec is not attached set cc toggle\n");
		charger_soft_cid_set_toggle(info->charger, true);

	}else if(!info->typec_attach && !en){
		lx_err("set cc not toggle\n");
		charger_soft_cid_set_toggle(info->charger, false);
	}else{
		lx_err("typec is attached, not set cc toggle\n");
	}
	if(en && !info->cid_status)
	{
		ret = alarm_try_to_cancel(&info->rust_det_work_timer);
		if (ret < 0) {
			lx_err("%s: callback was running, skip timer\n", __func__);
			return;
		}
		ktime_now = ktime_get_boottime();
		time_now = ktime_to_timespec64(ktime_now);
		end_time.tv_sec = time_now.tv_sec + 600;
		end_time.tv_nsec = time_now.tv_nsec + 0;
		ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

		lx_err("%s: alarm timer start:%d, %ld %ld\n", __func__, ret,
			end_time.tv_sec, end_time.tv_nsec);
		alarm_start(&info->rust_det_work_timer, ktime);
		lx_err("ui set cc toggle : start hrtimer\n");
	} else {
		ret = alarm_try_to_cancel(&info->rust_det_work_timer);
		if (ret < 0) {
			lx_err("%s: callback was running, skip timer\n", __func__);
			return;
		}
		lx_err("ui disable cc toggle : stop hrtimer\n");
	}
	lx_err("%s\n", __func__);
	return;
}
EXPORT_SYMBOL(manual_set_cc_toggle);

void manual_get_cc_toggle(bool *cc_toggle)
{
	if(info == NULL)
		return;
	*cc_toggle = info->ui_cc_toggle;
	lx_err("%s\n = %d", __func__, *cc_toggle);
	return;
}
EXPORT_SYMBOL(manual_get_cc_toggle);

bool manual_get_cid_status(void)
{
	if(info == NULL)
		return true;
	lx_err("%s\n = %d", __func__, info->cid_status);
	return info->cid_status;
}
EXPORT_SYMBOL(manual_get_cid_status);

void manual_set_cid_status(bool cid_status)
{
	if(info == NULL)
		return;
	info->cid_status = cid_status;
	if (cid_status) {
		info->cc_toggle_en = true;
		schedule_delayed_work(&info->soft_cid_work, msecs_to_jiffies(5000));
	} else {
		info->last_cc_toggle_en = true;
		info->cc_toggle_en = false;
		info->soft_cid_start = false;
		cancel_delayed_work_sync(&info->soft_cid_work);
	}
	lx_info("cid_status = %d", info->cid_status);
}
EXPORT_SYMBOL(manual_set_cid_status);

bool manual_get_attach_status(void)
{
	if(info == NULL)
		return true;
	lx_err("%s = %d", info->typec_attach);
	return info->typec_attach;
}
EXPORT_SYMBOL(manual_get_attach_status);

static void hrtime_otg_work_func(struct work_struct *work)
{
	if(info != NULL && info->tcpc != NULL)
		tcpci_set_cc(info->tcpc, TYPEC_CC_RD);
	lx_err("hrtime_otg_work_func enter\n");
}

static void set_cc_drp_work_func(struct work_struct *work)
{
	if (info != NULL && info->tcpc != NULL)
		tcpci_set_cc(info->tcpc, TYPEC_CC_DRP);
	lx_err("set_cc_drp_work_func enter\n");
}

static enum alarmtimer_restart rust_det_work_timer_handler(struct alarm *alarm, ktime_t now)
{
	if (info != NULL)
	{
		info->ui_cc_toggle = false;
		schedule_delayed_work(&info->hrtime_otg_work, 0);
	}
	lx_err("rust_det_work_timer_handler enter\n");
	return ALARMTIMER_NORESTART;
}
#endif
static int of_property_get_array(struct device *dev, char *name, int *size, int **data)
{
	struct device_node *node = dev->of_node;
	int byte_len, rc;
	int *out_value;
	lx_info("get array out_value!\n");
	if (of_find_property(node, name, &byte_len)) {
		out_value = devm_kzalloc(dev, byte_len, GFP_KERNEL);
		*data = out_value;
		if (IS_ERR_OR_NULL(out_value)) {
			lx_err("out_value kzalloc error\n");
			return -ENOMEM;
		} else {
			*size = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, name, out_value, *size);
			if (rc < 0){
				lx_err("parse error\n");
				return -ENOMEM;
			}
		}
	}else{
		lx_err("node not found\n");
		return -ENOMEM;
	}
	return 0;
}

static int charge_manager_thermal_init(struct charger_manager *manager)
{
	int byte_len, rc, ret = 0;
	struct device_node *node = manager->dev->of_node;
#ifdef FACTORY_BUILD
	/* force disable thermal policy in factory build */
	manager->thermal_enable = false;
#else
	manager->thermal_enable = true;
#endif //FACTORY_BUILD

	if (manager->thermal_enable == false) {
		lx_err("thermal ibat limit is disable\n");
		return -EINVAL;
	}

	if (of_find_property(node, "lx,pd-thermal-mitigation", &byte_len)) {
		manager->pd_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->pd_thermal_mitigation)) {
			ret |= PD_THERM_PARSE_ERROR;
			lx_err("pd_thermal_mitigation kzalloc error\n");
		} else {
			manager->pd_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,pd-thermal-mitigation",
				manager->pd_thermal_mitigation, manager->pd_thermal_levels);
			if (rc < 0) {
				ret |= PD_THERM_PARSE_ERROR;
				lx_err("pd_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= PD_THERM_PARSE_ERROR;
		lx_err("pd_thermal_mitigation not found\n");
	}

	if (of_find_property(node, "lx,qc2-thermal-mitigation", &byte_len)) {
		manager->qc2_thermal_mitigation = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->qc2_thermal_mitigation)) {
			ret |= QC2_THERM_PARSE_ERROR;
			lx_err("qc2_thermal_mitigation kzalloc error\n");
		} else {
			manager->qc2_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,qc2-thermal-mitigation",
				manager->qc2_thermal_mitigation, manager->qc2_thermal_levels);
			if (rc < 0) {
				ret |= QC2_THERM_PARSE_ERROR;
				lx_err("qc2_thermal_mitigation parse error\n");
			}
		}
	} else {
		ret |= QC2_THERM_PARSE_ERROR;
		lx_err("qc2_thermal_mitigation not found\n");
	}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        if (of_find_property(node, "lx,pd-thermal-mitigation-fast", &byte_len)) {
		manager->pd_thermal_mitigation_fast = devm_kzalloc(manager->dev, byte_len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(manager->pd_thermal_mitigation_fast)) {
			ret |= PD_THERM_PARSE_ERROR;
			lx_err("pd_thermal_mitigation_fast kzalloc error\n");
		} else {
			manager->pd_thermal_levels = byte_len / sizeof(u32);
			rc = of_property_read_u32_array(node, "lx,pd-thermal-mitigation-fast",
				manager->pd_thermal_mitigation_fast, manager->pd_thermal_levels);
			if (rc < 0) {
				ret |= PD_THERM_PARSE_ERROR;
				lx_err("pd_thermal_mitigation_fast parse error\n");
			}
		}
	} else {
		ret |= PD_THERM_PARSE_ERROR;
		lx_err("pd_thermal_mitigation_fast not found\n");
	}
#endif
	manager->thermal_parse_flags = ret;
	if (ret == (QC2_THERM_PARSE_ERROR | PD_THERM_PARSE_ERROR)) {
		manager->thermal_enable = false;
		ret = -EINVAL;
	}

	return ret;
}

static int main_chg_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		lx_err("the value of main fcc is error.\n");
		return value;
	}

	ret = charger_set_ichg(manager->charger, value);
	if (ret < 0) {
		lx_err("charger set ichg fail.\n");
	}
	return ret;
}

static int main_chg_fv_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;
	static int ffc_now;

	#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	lx_debug("%s old value = %d\n", __func__, value);
	value = value - max(manager->smart_batt, manager->smart_fv);
	lx_debug("new value = %d, smart_batt = %d, smart_fv = %d, fv_againg = %d\n", value, manager->smart_batt, manager->smart_fv, manager->fv_againg);
#endif
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (fuel_gauge_check_i2c_function(manager->fuel_gauge)) {
#ifndef FACTORY_BUILD
		value = 4100;
#endif
		lx_err("%d:FG i2c error, new value = %d\n", __LINE__, value);
	}
#endif
	ffc_now = fuel_gauge_get_fastcharge_mode(manager->fuel_gauge);
	lx_info("last_ffc=%d, now_ffc=%d, original_fv_value=%d\n", manager->ffc, ffc_now, value);
	manager->ffc = ffc_now;
	if (manager->ffc) {
		if (manager->prp_is_enable == true)
			if (manager->cycle_count >= 500)
				ret = charger_set_term_volt(manager->charger, (value + 10));
			else
				ret = charger_set_term_volt(manager->charger, (value + 20));
		else
			ret = charger_set_term_volt(manager->charger, (value + 200));// FFC use sw terminal to config PMIC fv + 100mv
		if (ret < 0) {
			lx_err("charger set term volt + 100 fail.\n");
		}
	} else {
		ret = charger_set_term_volt(manager->charger, (value + 10)); //normal use PMIC terminal to config fv + 30mv
		if (ret < 0) {
			lx_err("charger set term volt fail.\n");
		}
	}

	return ret;
}

static int main_chg_icl_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	if (value < 0) {
		lx_err("the value of main chg icl is error.\n");
		return value;
	}
	if (value < 100) {
		ret = charger_disable_power_path(manager->charger, true);
	} else {
		ret = charger_disable_power_path(manager->charger, false);
		ret |= charger_set_input_curr_lmt(manager->charger, value);
	}

	if (ret < 0) {
		lx_err("charger set icl fail.\n");
	} else {
		lx_info("%s vote val=%d\n", client, value);
	}
	return ret;
}

static int main_chg_iterm_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_set_term_curr(manager->charger, value);
	if (ret < 0) {
		lx_err("charger set iterm fail.\n");
	}
	return ret;
}

static int total_fcc_vote_callback(struct votable *votable, void *data, int value, const char *client)
{
	struct charger_manager *manager = data;

	if ((value >= max(FASTCHARGE_MIN_CURR, manager->v_iterm + 400)) && (g_policy->state == POLICY_RUNNING)) {
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			lx_err("main_icl_votable not found\n");
			return PTR_ERR(manager->main_icl_votable);
		} else
			vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			lx_err("main_fcc_votable not found\n");
			return PTR_ERR(manager->main_fcc_votable);
		} else
			vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, CP_EN_MAIN_CHG_CURR);
	} else {
		if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
			lx_err("-->main_icl_votable2 not found\n");
			return PTR_ERR(manager->main_icl_votable);
		} else
			vote(manager->main_icl_votable, MAIN_FCC_MAX_VOTER, false, 0);
		if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			lx_err("-->main_fcc_votable not found\n");
			return PTR_ERR(manager->main_fcc_votable);
		} else {
			if (value >= 0)
				vote(manager->main_fcc_votable, MAIN_FCC_MAX_VOTER, true, value);
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
static int is_full_vote_callback(struct votable *votable,
			void *data, int is_full_flag, const char *client)
{
	struct charger_manager *manager = data;

	manager->is_full_flag = is_full_flag;
	lx_err("%s client: %s is_full_flag: %d\n", __func__, client, is_full_flag);
	return 0;
}
#endif

static int main_chg_disable_real_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct charger_manager *manager = data;
	int ret = 0;

	ret = charger_set_chg(manager->charger, !enable);
	if (ret < 0) {
		lx_err("charger_set_chg fail.\n");
	}
	return ret;
}

static int cp_disable_vote_callback(struct votable *votable, void *data, int enable, const char *client)
{
	struct charger_manager *manager = data;
	struct chargerpump_dev *master_cp_chg = manager->master_cp_chg;
	struct chargerpump_dev *slave_cp_chg = manager->slave_cp_chg;
	int ret = 0;
	if(manager->cp_master_use){
		ret = chargerpump_set_enable(master_cp_chg, enable);
		if (ret < 0) {
			lx_err("master_cp_chg set chg fail.\n");
		}
	}
	if(manager->cp_slave_use){
		ret = chargerpump_set_enable(slave_cp_chg, enable);
		if (ret < 0) {
			lx_err("slave_cp_chg set chg fail.\n");
		}
	}
	return ret;
}

static int charger_manager_create_votable(struct charger_manager *manager)
{
	int ret = 0;

	if (manager->charger) {
		manager->main_fcc_votable = create_votable("MAIN_FCC", VOTE_MIN, main_chg_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_fcc_votable)) {
			lx_err("fail create MAIN_FCC voter.\n");
			return PTR_ERR(manager->main_fcc_votable);
		}

		manager->fv_votable = create_votable("MAIN_FV", VOTE_MIN, main_chg_fv_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->fv_votable)) {
			lx_err("fail create MAIN_FV voter.\n");
			return PTR_ERR(manager->fv_votable);
		}

		manager->main_icl_votable = create_votable("MAIN_ICL", VOTE_MIN, main_chg_icl_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_icl_votable)) {
			lx_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->main_icl_votable);
		}

		manager->iterm_votable = create_votable("MAIN_ITERM", VOTE_MIN, main_chg_iterm_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->iterm_votable)) {
			lx_err("fail create MAIN_ICL voter.\n");
			return PTR_ERR(manager->iterm_votable);
		}

		manager->main_chg_disable_real_votable = create_votable("MAIN_CHG_DISABLE_REAL", VOTE_SET_ANY, main_chg_disable_real_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->main_chg_disable_real_votable)) {
			lx_err("fail create MAIN_CHG_DISABLE voter.\n");
			return PTR_ERR(manager->main_chg_disable_real_votable);
		}

		manager->total_fcc_votable = create_votable("TOTAL_FCC", VOTE_MIN, total_fcc_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->total_fcc_votable)) {
			lx_err("fail create TOTAL_FCC voter.\n");
			return PTR_ERR(manager->total_fcc_votable);
		}
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
                manager->is_full_votable = create_votable("IS_FULL", VOTE_SET_ANY, is_full_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->is_full_votable)) {
			lx_err("fail create IS_FULL voter.\n");
			return PTR_ERR(manager->is_full_votable);
		}
#endif
	}

	if (manager->cp_master_use || manager->cp_slave_use) {
		manager->cp_disable_votable = create_votable("CP_DISABLE", VOTE_SET_ANY, cp_disable_vote_callback, manager);
		if(IS_ERR_OR_NULL(manager->cp_disable_votable)) {
			lx_err("fail create CP_DISABLE voter.\n");
			return PTR_ERR(manager->cp_disable_votable);
		}
	}
	return ret;
}

#define MCA_EVENT_NOTIFY_SIZE 128
static void charger_manager_generate_reverse_charge_uevent(struct charger_manager *manager, int val)
{
	int len;
	static char uevent_string[][MCA_EVENT_NOTIFY_SIZE + 1] = { 0 };
	static char *envp[] = {
		uevent_string[0],
		NULL,
	};

	len = snprintf(uevent_string[0], sizeof(uevent_string[0]), "POWER_SUPPLY_REVERSE_QUICK_CHARGE=%d", val);

	lx_err("%s", envp[0]);

	kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);
}

static int charger_manager_xiaomi_check_reverse_chg_mode(struct charger_manager *manager)
{
	static int ret = 0;
	int rc = 0;
	int ui_soc = 0;
	struct power_supply *batt_psy;
	union power_supply_propval pval = {0,};
	int soc_th_high = IPHONE_16_22W5_SOC;

	if (manager->is_iphone == IPHONE_17_PID)
		soc_th_high = IPHONE_17_22W5_SOC;

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		lx_err("reverse_charge Failed to get batt_psy\n");
		return 0;
	}
	rc = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		lx_err("reverse_charge failed to get soc");
		return 0;
	}
	ui_soc = pval.intval;

	if (info->sm.screen_state) {
		return 0;
	}

	if (manager->cur_mode == REVCHG_NORMAL || manager->cur_mode == REVCHG_QUICK_9) {
		if (ui_soc < soc_th_high)
			ret = 1;
		else
			ret = 2;
	} else if (manager->cur_mode == REVCHG_QUICK_22W5) {
		if (ui_soc < soc_th_high)
			ret = 1;
		else
			ret = 2;
	}

	lx_info("check_quick_rev_chg_mode:: soc:%d, is_iphone:0x%04x, ret:%d", ui_soc, manager->is_iphone, ret);

	return ret;
}

static void update_pdo_caps(struct charger_manager *manager, int pdo_caps)
{
	struct pd_port *pd_port = &manager->tcpc->pd_port;

	lx_info("[REVCHG] the pdo_caps is %d\n", pdo_caps);
	switch(pdo_caps) {
		case REVCHG_NORMAL:
			//5V1.5A
			pd_port->local_src_cap_default.pdos[0] =  0x26019096;
			pd_port->local_src_cap_default.nr = 1;
			break;
		case REVCHG_QUICK_9:
			//9V1A
			pd_port->local_src_cap_default.pdos[0] =  0x26019096;
			pd_port->local_src_cap_default.pdos[1] =  0x2602D064;
			pd_port->local_src_cap_default.nr = 2;
			break;
		case REVCHG_QUICK_18:
			//9V2A
			pd_port->local_src_cap_default.pdos[0] =  0x26019096;
			pd_port->local_src_cap_default.pdos[1] =  0x2602D0C8;
			pd_port->local_src_cap_default.nr = 2;
			break;
		case REVCHG_QUICK_22W5:
			//9V2.5A
			pd_port->local_src_cap_default.pdos[0] =  0x26019096;
			pd_port->local_src_cap_default.pdos[1] =  0x2602D0FA;
			pd_port->local_src_cap_default.nr = 2;
			break;
		default:
			break;
	}

	tcpm_dpm_pd_soft_reset(manager->tcpc, NULL);
	return;
}

static int charger_manager_check_reverse_charge(struct charger_manager *manager)
{
	int batt_temp = 0;
	int ui_soc = 0;
	int rc;
	struct power_supply *batt_psy;
	union power_supply_propval pval = {0,};
	int vin_adc = 0;
	int reverse_soc_th = IPHONE_16_18W_SOC;

	if (manager->is_iphone == IPHONE_17_PID)
		reverse_soc_th = IPHONE_17_18W_SOC;

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		lx_err("reverse_charge Failed to get batt_psy\n");
		return 0;
	}

	rc = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		lx_err("reverse_charge failed to get soc");
		return 0;
	}
	ui_soc = pval.intval;

	rc = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		lx_err("reverse_charge failed to get bat_temp");
		return 0;
	}
	batt_temp = pval.intval;


	charger_adc_enable(manager->charger, true);
	charger_get_adc(manager->charger, ADC_GET_VBUS, &vin_adc);

	lx_err("reverse_charge otg_status = %d, cur_mode = %d, batt_temp = %d, thermal_board_temp = %d, ui_soc = %d, reverse_soc_th = %d, vin_adc = %d\n",
			manager->otg_status, manager->cur_mode, batt_temp, manager->thermal_board_temp, ui_soc, reverse_soc_th, vin_adc);

	if (!manager->otg_status || batt_temp < REVERSE_CHARGE_BATTEMP_TH ||
		manager->thermal_board_temp > REVERSE_CHARGE_THERMAL_TH || ui_soc < reverse_soc_th) {
		lx_err("reverse_charge exit for otg_closed or abnormal_temp or low_soc");
		return 0;
	} else {
		return 1;
	}
}

static void reverse_quick_charge_work_handler(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, reverse_quick_charge_work.work);
	int revchg_enable = charger_manager_check_reverse_charge(manager);
	static int reverse_power_mode = REVCHG_QUICK_9;
	int revchg_fast_mode = 0;

	if (!revchg_enable) {
		lx_info("[REVCHG] in case REVCHG_NORMAL");
		manager->cur_mode = REVCHG_NORMAL;
		reverse_power_mode = REVCHG_QUICK_9;
		update_pdo_caps(manager, REVCHG_NORMAL);
		goto NEXT_LOOP;
	}

	if (manager->cur_mode != reverse_power_mode) {
		manager->last_mode = manager->cur_mode;
		manager->cur_mode = reverse_power_mode;
		update_pdo_caps(manager, reverse_power_mode);
	}

	switch(reverse_power_mode) {
		case REVCHG_QUICK_9:
			lx_info("[REVCHG] in case REVCHG_QUICK_9\n");
			revchg_fast_mode = charger_manager_xiaomi_check_reverse_chg_mode(manager);
			if (revchg_fast_mode == 1 || revchg_fast_mode == 2) {
				if (!manager->revchg_bcl) {
					charger_manager_generate_reverse_charge_uevent(manager, 3);// open bcl
				} else {
					if (revchg_fast_mode == 1) {
						reverse_power_mode = REVCHG_QUICK_18;
					} else {
						reverse_power_mode = REVCHG_QUICK_22W5;
					}

					if (manager->cur_mode != reverse_power_mode) {
						lx_info("[REVCHG]REVCHG_QUICK_9 cur_mode = %d, reverse_power_mode = %d\n",
							manager->cur_mode, reverse_power_mode);
						manager->last_mode = manager->cur_mode;
						manager->cur_mode = reverse_power_mode;
						update_pdo_caps(manager, reverse_power_mode);
					}
				}
			}

			if((info->sm.screen_state) && (reverse_power_mode != REVCHG_QUICK_18 && reverse_power_mode != REVCHG_QUICK_22W5)) {
				charger_manager_generate_reverse_charge_uevent(manager, 2);// close bcl
			}
			break;
		case REVCHG_QUICK_18:
			lx_info("[REVCHG] in case REVCHG_QUICK_18");
			if(info->sm.screen_state) {
				reverse_power_mode = REVCHG_QUICK_9;
				if (manager->cur_mode != reverse_power_mode) {
					lx_info("[REVCHG]SCREEN_ON REVCHG_QUICK_18 cur_mode = %d, reverse_power_mode = %d",
							manager->cur_mode, reverse_power_mode);
					manager->last_mode = manager->cur_mode;
					manager->cur_mode = reverse_power_mode;
					update_pdo_caps(manager, reverse_power_mode);
				}
			}
			break;
		case REVCHG_QUICK_22W5:
			lx_info("[REVCHG] in case REVCHG_QUICK_22W5");
			if(info->sm.screen_state) {
				reverse_power_mode = REVCHG_QUICK_9;
				if (manager->cur_mode != reverse_power_mode) {
					lx_info("[REVCHG]SCREEN_ON REVCHG_QUICK_22W5 cur_mode = %d, reverse_power_mode = %d",
							manager->cur_mode, reverse_power_mode);
					manager->last_mode = manager->cur_mode;
					manager->cur_mode = reverse_power_mode;
					update_pdo_caps(manager, reverse_power_mode);
					break;
				}
			}

			revchg_fast_mode = charger_manager_xiaomi_check_reverse_chg_mode(manager);
			if (revchg_fast_mode == 1) {
				if (!manager->revchg_bcl) {
					charger_manager_generate_reverse_charge_uevent(manager, 3);// open bcl
					lx_info("[REVCHG] in case REVCHG_QUICK_22W5, open bcl");
				} else {
					reverse_power_mode = REVCHG_QUICK_18;
					if (manager->cur_mode != reverse_power_mode) {
						lx_info("[REVCHG]REVCHG_QUICK_22W5 cur_mode = %d, reverse_power_mode = %d",
							manager->cur_mode, reverse_power_mode);
						manager->last_mode = manager->cur_mode;
						manager->cur_mode = reverse_power_mode;
						update_pdo_caps(manager, reverse_power_mode);
					}
				}
			}
			break;
		default:
			break;

	}
NEXT_LOOP:
	if (revchg_enable) {
		schedule_delayed_work(&manager->reverse_quick_charge_work, msecs_to_jiffies(1000));
	} else {
		schedule_delayed_work(&manager->reverse_quick_charge_work, msecs_to_jiffies(5000));
	}
}

static void force_set_src_work_handler(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, force_set_src_work.work);

	tcpm_typec_change_role_postpone(manager->tcpc, TYPEC_ROLE_SRC, false);
}

static void src_switch_drp_work_handler(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, src_switch_drp_work.work);

	if (!manager->otg_status && !info->typec_attach) {
		tcpm_typec_change_role_postpone(manager->tcpc, TYPEC_ROLE_TRY_SNK, false);
	}
}

#define SC853X_CHARGERPUMP 8531
static void charger_manager_handle_reverse_charge_event(struct charger_manager *manager, int mv, bool notify)
{
	static int last_pos = REVERSE_CHARGE_5V_BOOOST;
	int pos;
	int status = 0;
	int rc = 0;
	struct bq_fg_chip *bp = NULL;
	int vendor = 0;
	struct pd_port *pd_port = &manager->tcpc->pd_port;

	pos = mv;
	if ((mv == 0) && (!manager->otg_status))
		return;
	else if ((mv == 0) && (manager->otg_status))
		manager->otg_status = false;
	else
		manager->otg_status = true;

	vendor = chargerpump_get_cp_vendor(manager->master_cp_chg);

	bp = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bp))
		return;

	lx_err("reverse_charge source request: %d, pos: %d, last_pos: %d, notify: %d, otg_status: %d\n",
		mv, pos, last_pos, notify, manager->otg_status);

	if ((mv == REVERSE_CHARGE_5V_BOOOST) && notify) {
		status = charger_manager_check_reverse_charge(manager);
		charger_manager_generate_reverse_charge_uevent(manager, status);
		return;
	}

	if (mv && (last_pos == pos)) {
		lx_err("reverse_charge same handler, ignore...");
		return;
	}
	last_pos = pos;

	if (manager->otg_status) {
		if (pos == REVERSE_CHARGE_5V_BOOOST) {
			lx_err("reverse_charge start buck 5v otg, cur_mode = %d, last_mode = %d\n", manager->cur_mode, manager->last_mode);
			if ((manager->cur_mode > REVCHG_NORMAL) && (manager->last_mode > REVCHG_NORMAL))
				return;

			rc = chargerpump_set_otg(manager->master_cp_chg, false);
			if (rc < 0)
				lx_err("reverse_charge Couldn't close cp otg rc=%d\n", rc);

			rc = charger_set_otg(manager->charger, true);
			if (rc < 0)
				lx_err("reverse_charge Couldn't enable buck OTG rc=%d\n", rc);
			pd_port->vbus_no_change = false;
		} else if (pos == REVERSE_CHARGE_9V_BOOOST) {
			lx_err("start revert 1_2 cp, cur_mode = %d, last_mode = %d\n", manager->cur_mode, manager->last_mode);
			if ((manager->cur_mode > REVCHG_NORMAL) && (manager->last_mode > REVCHG_NORMAL)) {
				pd_port->vbus_no_change = true;
				return;
			}

			bp->input_suspend = 1;
			manager->input_suspend = 1;
			vote(manager->main_icl_votable, FACTORY_KIT_VOTER, true, 0);

			charger_set_otg_ovp_protect(manager->charger, true);

			rc = chargerpump_set_enable_adc(manager->master_cp_chg, true);
			if (rc < 0)
				lx_err("reverse_charge Couldn't enable cp adc rc=%d\n", rc);

			rc = chargerpump_set_otg(manager->master_cp_chg, true);
			if (rc < 0)
				lx_err("reverse_charge Couldn't enable cp otg rc=%d\n", rc);

			charger_set_otg_ovp_protect(manager->charger, false);
		}
	} else {
		lx_err("reverse_charge end\n");

		chargerpump_set_enable_adc(manager->master_cp_chg, false);
		if (rc < 0)
			lx_err("reverse_charge Couldn't enable cp adc rc=%d\n", rc);

		rc = chargerpump_set_otg(manager->master_cp_chg, false);
		if (rc < 0)
			lx_err("reverse_charge Couldn't close cp otg rc=%d\n", rc);

		rc = charger_set_otg(manager->charger, false);
		if (rc < 0)
			lx_err("reverse_charge Couldn't enable buck OTG rc=%d\n", rc);

		charger_adc_enable(manager->charger, false);
		vote(manager->main_icl_votable, FACTORY_KIT_VOTER, false, 0);
		bp->input_suspend = 0;
		manager->input_suspend = 0;

		last_pos = REVERSE_CHARGE_5V_BOOOST;
		manager->source_boost_status = 0;
		manager->rc_curr_min_cnt = 0;
		manager->reverse_quick_charge = false;
		manager->force_source = false;
		manager->is_iphone = false;
		pd_port->vbus_no_change = false;
		pd_port->local_src_cap_default.pdos[0] =  0x26019096;
		pd_port->local_src_cap_default.nr = 1;
		if (manager->reverse_disconnect) {
			manager->reverse_disconnect = 0;
			manager->cur_mode = REVCHG_NORMAL;
			manager->last_mode = REVCHG_NORMAL;
			schedule_delayed_work(&manager->force_set_src_work, msecs_to_jiffies(100));
			schedule_delayed_work(&manager->src_switch_drp_work, msecs_to_jiffies(1500));
		}
		cancel_delayed_work_sync(&manager->reverse_quick_charge_work);
		/*if (manager->last_otg_status) {
			lx_info("reverse 18w going try TYPEC_ROLE_SRC!\n");
			tcpm_typec_change_role_postpone(manager->tcpc, TYPEC_ROLE_SRC, true);
		} else {
			lx_info("reverse 18w plugout try TYPEC_ROLE_TRY_SNK!\n");
			tcpm_typec_change_role_postpone(manager->tcpc, TYPEC_ROLE_TRY_SNK, true);
		}*/
		charger_manager_generate_reverse_charge_uevent(manager, status);
	}
}

static bool pd_dr_device(struct charger_manager *manager)
{
	int ret;
	struct tcpm_power_cap cap;

	ret = tcpm_inquire_pd_source_cap(manager->tcpc, &cap);
	if (ret != TCPM_SUCCESS) {
		lx_err("%s ret = %d", __func__, ret);
		return false;
	}
	if ((cap.cnt <= 2) && (cap.pdos[0] & PDO_FIXED_DUAL_ROLE)) {
		lx_err("%s is DualRolePower set ctocchg true", __func__);
		return true;
	} else {
		return false;
	}
}

#define HIFI_VENDOR_ID_MAX	2
static int hifi_vendor_id[HIFI_VENDOR_ID_MAX] = {
	0x2109, 0x2D79
};

static bool charger_manager_reverse_check_hifi(int vendor_id)
{
	int i = 0;

	for(i = 0; i < HIFI_VENDOR_ID_MAX; i++) {
		lx_info("vendor id: %x %x\n", hifi_vendor_id[i], vendor_id);
		if (hifi_vendor_id[i] == vendor_id)
			return true;
	}
	return false;
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
static int charger_manager_tcpc_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	int ret;
	struct timespec64 end_time, time_now;
	ktime_t ktime, ktime_now;
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	struct charger_manager *manager =
		container_of(nb, struct charger_manager, pd_nb);
	uint32_t partner_vdos[VDO_MAX_NR];
	uint16_t vid;
	uint16_t pid;

	lx_info("noti event: %d %d\n", (int)event, (int)noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT) {
			manager->pd_curr_max = noti->vbus_state.ma;
			manager->pd_volt_max = noti->vbus_state.mv;
			vote(manager->main_icl_votable, TYPEC_SINK_VBUS_VOTER, true, manager->pd_curr_max);
			lx_info("TCP_NOTIFY_SINK_VBUS pd_curr_max = %d\n", manager->pd_curr_max);
			if (manager->pd_curr_max == 0)
				chargerpump_set_enable(manager->master_cp_chg, false);
			if (manager->ctoc_chg)
				charger_manager_wake_thread(manager);
		}
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		charger_manager_handle_reverse_charge_event(manager, noti->vbus_state.mv, false);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;
		if (old_state == TYPEC_UNATTACHED &&
				new_state != TYPEC_UNATTACHED &&
				!info->typec_attach) {
			lx_info("%s typec plug in, polarity = %d\n",
					__func__, noti->typec_state.polarity);
			info->typec_attach = true;
			manager->soft_cid_start = false;
			if (new_state == TYPEC_ATTACHED_SNK ||
		     new_state == TYPEC_ATTACHED_NORP_SRC ||
		     new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		     new_state == TYPEC_ATTACHED_DBGACC_SNK) {
				lx_info("charger plug in, polarity = %d\n", noti->typec_state.polarity);
				manager->charger_online = true;
				charger_manager_wake_thread(manager);
			} else if (new_state == TYPEC_ATTACHED_SRC) {
				lx_info("otg plug in\n");
				charger_adc_enable(manager->charger, true);
				chargerpump_set_enable_adc(manager->master_cp_chg, true);
				chargerpump_set_enable_adc(manager->slave_cp_chg, true);
			}
			if (info->ui_cc_toggle) {
				ret = alarm_try_to_cancel(&info->rust_det_work_timer);
				if (ret < 0) {
					lx_err("%s: callback was running, skip timer\n", __func__);
				}
				lx_info("typec plug in, cancel hrtimer\n");
			}
		} else if (old_state != TYPEC_UNATTACHED &&
				new_state == TYPEC_UNATTACHED &&
				info->typec_attach) {
			lx_info("%s typec plug out\n", __func__);
			manager->pd_active = CHARGE_PD_INVALID;
			if (old_state == TYPEC_ATTACHED_SNK ||
			    old_state == TYPEC_ATTACHED_NORP_SRC ||
			    old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			    old_state == TYPEC_ATTACHED_DBGACC_SNK) {
				lx_info("charger plug out\n");
				manager->charger_online = false;
				charger_manager_wake_thread(manager);
			} else if (old_state == TYPEC_ATTACHED_SRC) {
				lx_info("otg plug out\n");
				charger_adc_enable(manager->charger, false);
				chargerpump_set_enable_adc(manager->master_cp_chg, false);
				chargerpump_set_enable_adc(manager->slave_cp_chg, false);
			}
			info->typec_attach = false;
			schedule_delayed_work(&manager->soft_cid_work, msecs_to_jiffies(2000));
			info->fv_overvoltage_flag = false;
			if (info->ui_cc_toggle) {
				if (info->tcpc != NULL) {
					lx_err("typec plug out, ui set cc toggle\n");
					schedule_delayed_work(&info->set_cc_drp_work, msecs_to_jiffies(500));
				}
				ret = alarm_try_to_cancel(&info->rust_det_work_timer);
				if (ret < 0) {
					lx_err("%s: callback was running, skip timer\n", __func__);
				}
				ktime_now = ktime_get_boottime();
				time_now = ktime_to_timespec64(ktime_now);
				end_time.tv_sec = time_now.tv_sec + 600;
				end_time.tv_nsec = time_now.tv_nsec + 0;
				ktime = ktime_set(end_time.tv_sec,end_time.tv_nsec);

				lx_err("%s: alarm timer start:%d, %ld %ld\n", __func__, ret,
						end_time.tv_sec, end_time.tv_nsec);
				alarm_start(&info->rust_det_work_timer, ktime);
				lx_info("typec plug out, start hrtimer\n");
			}
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		manager->is_pr_swap = true;
		if (noti->swap_state.new_role == PD_ROLE_SINK) {
			manager->otg_status = false;
			manager->pd_active = 10;
		} else if (noti->swap_state.new_role == PD_ROLE_SOURCE) {
			manager->pd_active = CHARGE_PD_INVALID;
			manager->charger_online = false;
		}
		break;
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			manager->pd_curr_max = 0;
			manager->pd_active = CHARGE_PD_INVALID;
			manager->is_pr_swap = false;
			manager->pd_contract_update = false;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			manager->pd_contract_update = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_PPS_ACTIVE;
			lx_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
		#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			xm_uevent_report(manager);
		#endif
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			manager->charger_online = true;
			manager->pd_active = noti->pd_state.connected = CHARGE_PD_ACTIVE;
			if (pd_dr_device(manager)) {
				manager->ctoc_chg = true;
			}
			break;
		case PD_CONNECT_HARD_RESET:
			charger_get_online(manager->charger, &manager->vbus_online);
			if (manager->pd_active == CHARGE_PD_PPS_ACTIVE && !manager->vbus_online)
				manager->charger_online = false;
			break;
		case PD_CONNECT_PE_READY_SRC:
		case PD_CONNECT_PE_READY_SRC_PD30:
			ret = tcpm_inquire_pd_partner_inform(manager->tcpc, partner_vdos);
			lx_info("%s: partner_vdo0:0x%16x, pid=0x%04x\n", __func__, partner_vdos[0], (PD_PRODUCT_PID(partner_vdos[0])));
			lx_info("%s: partner_vdo1:0x%16x, pid=0x%04x\n", __func__, partner_vdos[1], (PD_PRODUCT_PID(partner_vdos[1])));
			lx_info("%s: partner_vdo2:0x%16x, pid=0x%04x\n", __func__, partner_vdos[2], (PD_PRODUCT_PID(partner_vdos[2])));

			if (ret == TCPM_SUCCESS) {
				vid = partner_vdos[0] & 0xFFFF;
				pid = (partner_vdos[2] >> 16) & 0xFFFF;
				lx_info("%s:New USB device found, vid:0x%04x, pid=0x%04x\n", __func__, vid, pid);

				if (vid != APLLE_SVID) {
					manager->is_iphone = false;
				        lx_info("Not an Apple device: VID=0x%04x\n", vid);
				} else {
					manager->is_iphone = true;
				}

				if (pid == IPHONE_15_PID) {
					manager->is_iphone = IPHONE_15_PID;
					lx_info("Detected iPhone 15\n");
				} else if (pid == IPHONE_16_PID) {
					manager->is_iphone = IPHONE_16_PID;
				        lx_info("Detected iPhone 16\n");
				} else if (pid == IPHONE_17_PID || pid == IPHONE_17_PRO_PID || pid == IPHONE_17_PM_PID) {
					manager->is_iphone = IPHONE_17_PID;
				        lx_info("Detected iPhone 17\n");
				} else {
				        lx_info("Unknown Apple device: PID=0x%04x\n", pid);
				}

				if (charger_manager_reverse_check_hifi(PD_IDH_VID(partner_vdos[0])))
					break;
			}
			charger_manager_handle_reverse_charge_event(manager, REVERSE_CHARGE_5V_BOOOST, true);
			break;
		default:
			break;
		}
		charger_manager_wake_thread(manager);
		break;

	default:
		break;
	}
	return NOTIFY_OK;
}
#endif

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
static int charger_monitor_fg_i2c_status(struct charger_manager *manager) {
	int ret = 0;

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lx_err("manager->fuel_gauge is_err_or_null\n");
			return -EINVAL;
		}
	}

	ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
	if (!manager->vbus_type)
		return ret;
	if (ret) {
		if (manager->vbus_type == VBUS_TYPE_HVDCP || manager->vbus_type == VBUS_TYPE_HVDCP_3
			|| manager->vbus_type == VBUS_TYPE_HVDCP_3P5 || manager->vbus_type == VBUS_TYPE_PD) {
			vote(manager->main_fcc_votable, FG_I2C_ERR, true, 1500);
			vote(manager->main_icl_votable, FG_I2C_ERR, true, 1500);
		}
	} else {
		vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
		vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
	}
	return ret;
}
#endif
#define	EEA_CHARGE_FULL "EEA_CHARGE_FULL"
static int charge_enable_eea(struct charger_manager *manager, bool enable)
{
	if (enable) {
		vote(manager->main_chg_disable_real_votable, EEA_CHARGE_FULL, true, 0);
		/* recharge delay */
		msleep(50);
		vote(manager->main_chg_disable_real_votable, EEA_CHARGE_FULL, false, 0);
	} else {
		vote(manager->main_chg_disable_real_votable, EEA_CHARGE_FULL, true, 0);
	}

	return 0;
}

static int input_current_limit_eea(struct charger_manager *manager, bool en)
{
	struct votable *main_icl_votable;

	main_icl_votable = find_votable("MAIN_ICL");
	if (main_icl_votable == NULL) {
		lx_err("Couldn't find MAIN_ICL votable\n");
		return -EINVAL;
	}

	if(en)
		vote(main_icl_votable, EEA_CHARGE_FULL, true, 150);
	else
		vote(main_icl_votable, EEA_CHARGE_FULL, false, 0);

	return 0;
}

bool is_input_suspend_by_client(struct charger_manager *manager, const char *client_str)
{
	if ((manager == NULL) || !manager->main_icl_votable)
		return false;

	return !get_client_vote(manager->main_icl_votable, client_str);
}
EXPORT_SYMBOL(is_input_suspend_by_client);

bool is_input_suspend(struct charger_manager *manager)
{
	if ((manager == NULL) || !manager->main_icl_votable)
		return false;

	return !get_effective_result(manager->main_icl_votable);
}
EXPORT_SYMBOL(is_input_suspend);

#ifdef FACTORY_BUILD
#define BAT_CAPACITY_MIN 75
#define BAT_CAPACITY_MAX 80

void ato_control_soc(struct charger_manager *manager)
{
	bool ato_soc_limit = is_input_suspend_by_client(manager, ATO_SOC_LIMIT_VOTER);

	if (!ato_soc_limit && manager->soc >= BAT_CAPACITY_MAX && manager->ato_soc_user_control == false) {
		lx_err("factory_version : capacity 80 stop charger\n");
		vote(manager->main_icl_votable, ATO_SOC_LIMIT_VOTER, true, 0);
	} else if (ato_soc_limit && (manager->soc <= BAT_CAPACITY_MIN || manager->ato_soc_user_control == true)) {
		vote(manager->main_icl_votable, ATO_SOC_LIMIT_VOTER, false, 0);
		lx_err("factory_version : capacity 75 start charger\n");
	}
	lx_info("factory_version : ato_soc_limit = %d, ato_soc_user_control = %d, input_suspend = %d\n",
		ato_soc_limit, manager->ato_soc_user_control, is_input_suspend(manager));
}
#endif

#define EEA_RECHARGE_SOC             95
#define RECHARGE_COUNT               5
#define CHARGE_FULL_COUNT            3
#define POWER_SUPPLY_STATUS_INVAILD -1
static void handle_recharge_eea(struct charger_manager *manager)
{
	int ret = 0;
	union power_supply_propval pval = {0,};

	if(manager->chg_status == POWER_SUPPLY_STATUS_DISCHARGING || manager->vbus_type == VBUS_TYPE_NONE){
		manager->charge_full = 0;
		lx_info("%s: adapter pluged, reset charge_full flag \n", __func__);
	}

	lx_info("%s: temp:%d, uisoc:%d, volt:%d, curr:%d\n", __func__,
		manager->tbat, manager->soc, manager->vbat, manager->ibat);

	if (manager->charge_full) {
		manager->full_cnt = 0;
		if (manager->soc < EEA_RECHARGE_SOC && (-100 <= manager->tbat && manager->tbat <= 450)) {//not in charging pause with jeita
			manager->recharge_cnt++;
		} else {
			manager->recharge_cnt = 0;
			if (manager->soc >= EEA_RECHARGE_SOC) {
				lx_info("%s: keep full and limit ibus for after full before recharge \n", __func__);
				manager->fake_batt_status = POWER_SUPPLY_STATUS_FULL;
				charge_enable_eea(manager, false);
				input_current_limit_eea(manager, false);

				ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
				if (POWER_SUPPLY_STATUS_FULL != pval.intval) {
					lxchg_charge_status_update(manager);
					power_supply_changed(manager->batt_psy);
				}
				if (manager->soc == 99 && manager->ibat >= 0) {
					fuel_gauge_status_update(manager->fuel_gauge);
					lx_info("%s:charge_full update 100\n", __func__);
				}
			}
		}

		if ((manager->recharge_cnt > RECHARGE_COUNT)) {
			manager->recharge_cnt = 0;
			manager->charge_full = false;
			manager->fake_batt_status = -EINVAL;
			manager->fg_full_cnt = 0;
			input_current_limit_eea(manager, false);
			charge_enable_eea(manager, true);
			vote(manager->total_fcc_votable, EEA_CHARGE_FULL, false, 0);
			lx_info("%s: Forced recharge\n", __func__);
		}

	} else {
		manager->recharge_cnt = 0;

		if (manager->soc >= 99 && manager->bbc_charge_done) {
			manager->full_cnt++;
		} else if (manager->soc == 100) {
			manager->full_cnt = CHARGE_FULL_COUNT;
		} else {
			manager->full_cnt = 0;
		}

		if (manager->full_cnt >= CHARGE_FULL_COUNT) {
			manager->full_cnt = 0;
			manager->charge_full = true;
			manager->fake_batt_status = POWER_SUPPLY_STATUS_FULL;
			manager->recharge = false;
			full_flag = true;
			vote(manager->total_fcc_votable, EEA_CHARGE_FULL, true, 0);
			//fuel_gauge_set_charger_to_full(manager->fuel_gauge);
		}
	}

	lx_info("%s:full=%d, full_cnt=%d, recharge_cnt=%d\n",
		__func__, manager->charge_full, manager->full_cnt, manager->recharge_cnt);
}

#define CHARGE_FULL_CURR_BUFFER  	20 // P19 30
#define CHARGE_FULL_VOLT_BUFFER  	20
static bool is_replenish_done(struct charger_manager *manager)
{
	int eff_fv, iterm;
	static int full_cnt;
	int battery_id;

	battery_id = fuel_gauge_get_battery_id(manager->fuel_gauge);
	if (battery_id == 1) {
		iterm = ATL_ITERM;
	} else
		iterm = 575;
	eff_fv = get_effective_result(manager->fv_votable);

	if ((manager->vbat >= eff_fv - CHARGE_FULL_VOLT_BUFFER) && (manager->ibat/1000 <= iterm + CHARGE_FULL_CURR_BUFFER)) {
		full_cnt ++;
	} else {
		full_cnt = 0;
	}
	lx_info("%s: eff_fv:%d ~ %d, iterm:%d ~ %d, batt_volt:%d, batt_curr:%d, full_cnt:%d \n", __func__,
		eff_fv, CHARGE_FULL_VOLT_BUFFER, iterm, CHARGE_FULL_CURR_BUFFER, manager->vbat, manager->ibat/1000, full_cnt);
	if (full_cnt > CHARGE_FULL_COUNT) {
		return true;
	}

	return false;
}

#define POWER_REPLENISH_FCC     2880
#define POWER_REPLENISH_ICL     2000
static int set_fv_fcc_cofig_for_replenish(struct charger_manager *manager)
{
	int rp_fv;
	int rc = 0;
	int ret;
	int battery_id;
	int iterm = 600;
	union power_supply_propval pval = {0,};


	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		lx_err("Couldn't read batt voltage_now, ret=%d\n", ret);
	}
	manager->cycle_count = pval.intval;

	if(manager->is_eu_mode && manager->cycle_count <= 300){
		pr_info("%s: cycle_count is less than 300, not support replenish for EEA \n", __func__);
		return -1;
	}

	charge_enable_eea(manager, true);
	manager->fake_batt_status = -EINVAL;
	manager->fake_batt_status_hot = -EINVAL;
	rp_fv = get_effective_result(manager->fv_votable);

	switch (manager->tbat) {
	case 201 ... 350:
		if(is_between(0, 100, manager->cycle_count)) {
			rp_fv = 4490;
		} else if (is_between(101, 300, manager->cycle_count)) {
			rp_fv = 4480;
		} else if (is_between(301, 800, manager->cycle_count)) {
			rp_fv = 4470;
		} else if (is_between(801, 2000, manager->cycle_count)) {
			rp_fv = 4450;
		}
		break;
	case 351 ... 400:
		if (is_between(0, 100,  manager->cycle_count)) {
			rp_fv = 4480;
		} else if (is_between(101, 300,  manager->cycle_count)) {
			rp_fv = 4470;
		} else if (is_between(301, 800,  manager->cycle_count)) {
			rp_fv = 4460;
		} else if (is_between(801, 2000,  manager->cycle_count)) {
			rp_fv = 4440;
		}
		break;
	case 401 ... 450:
		if (is_between(0, 100,  manager->cycle_count)) {
			rp_fv = 4470;
		} else if (is_between(101, 300,  manager->cycle_count)) {
			rp_fv = 4460;
		} else if (is_between(301, 800,  manager->cycle_count)) {
			rp_fv = 4450;
		} else if (is_between(801, 2000,  manager->cycle_count)) {
			rp_fv = 4430;
		}
		break;
	default:
		lx_info("%s: not support power replenish \n", __func__);
		break;
	}

	battery_id = fuel_gauge_get_battery_id(manager->fuel_gauge);
	if (battery_id == 1)
		iterm = ATL_ITERM;
	else
		iterm = 575;

	lx_info("%s: temp:%d, cycle_count:%d, rp_fv:%d, iterm:%d\n", __func__, manager->tbat, manager->cycle_count, rp_fv, iterm);

	input_current_limit_eea(manager, false);
	vote(manager->iterm_votable, POWER_REPLENISH_VOTER, true, iterm);
	vote(manager->fv_votable, POWER_REPLENISH_VOTER, true, rp_fv);
	vote(manager->main_icl_votable, POWER_REPLENISH_VOTER, true, POWER_REPLENISH_ICL);
	vote(manager->total_fcc_votable, POWER_REPLENISH_VOTER, true, POWER_REPLENISH_FCC);

	return rc;
}

static void replenish_work_func(struct work_struct *work)
{
	int ret = 0;
        union power_supply_propval pval = {0,};
	struct charger_manager *manager = container_of(work, struct charger_manager, replenish_work.work);
	int rc = 0;

	manager->prp_is_enable = true;
	lx_info("%s: replenish_en:%d, work_count:%d, replenish_done:%d \n", __func__, manager->prp_is_enable, manager->rp_work_count, is_replenish_done(manager));
	if (manager->rp_work_count == 1) {
		rc = set_fv_fcc_cofig_for_replenish(manager);
		if (rc < 0)
			goto reset;
	} else if (is_replenish_done(manager)) {
		manager->fake_batt_status = POWER_SUPPLY_STATUS_FULL;
		manager->fake_batt_status_hot = POWER_SUPPLY_STATUS_FULL;
		charge_enable_eea(manager, false);

		ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
		if (POWER_SUPPLY_STATUS_FULL != pval.intval) {
			is_replenish_full = true;
			lxchg_charge_status_update(manager);
			power_supply_changed(manager->batt_psy);
		}
		lx_info("power replenish done, reset replenish status\n");
		goto reset;
	}
	if (manager->chg_status == POWER_SUPPLY_STATUS_DISCHARGING || manager->vbus_type == VBUS_TYPE_NONE) {
		lx_info("adapter plug out, reset replenish status\n");
		goto reset;
	}

	manager->rp_work_count ++;
	schedule_delayed_work(&manager->replenish_work, msecs_to_jiffies(REPLENISH_LOOP_WAIT_S));// 3s

	return;

reset:
	// manager->prp_is_enable = false;
	manager->rp_work_count = 0;
	cancel_delayed_work(&manager->replenish_work);
	vote(manager->iterm_votable, POWER_REPLENISH_VOTER, false, 0);
	vote(manager->fv_votable, POWER_REPLENISH_VOTER, false, 0);
	vote(manager->main_icl_votable, POWER_REPLENISH_VOTER, false, POWER_REPLENISH_ICL);
	vote(manager->total_fcc_votable, POWER_REPLENISH_VOTER, false, 0);
	return;
}

static int handle_power_replenish(struct charger_manager *manager)
{
	int rc = 0;

	lx_info("%s: charge_done:%d, replenish_en:%d \n", __func__, manager->bbc_charge_done, manager->prp_is_enable);
	if((manager->bbc_charge_done || manager->fake_batt_status_hot == POWER_SUPPLY_STATUS_FULL ||
		manager->fake_batt_status == POWER_SUPPLY_STATUS_FULL)&& (manager->prp_is_enable == false)){
		schedule_delayed_work(&manager->replenish_work, msecs_to_jiffies(REPLENISH_START_WAIT_S)); //5 mins
	}
	if (!manager->bbc_charge_done && (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING) && (manager->prp_is_enable == false)) {
		cancel_delayed_work(&manager->replenish_work);
		//manager->prp_is_enable = false;
		lx_info("%s:battery charging no need replenish \n", __func__);
	}

	return rc;
}

static void handle_recharge_eea_by_iterm(struct charger_manager *manager)
{
	int ret = 0;
	union power_supply_propval pval = {0,};
	struct lx_jeita_info *jeita_chip = manager->jeita_chip;
	int total_fcc;
	int eff_fv;
	int ibat = 0;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0)
		lx_err("get battery current error.\n");
	else
		ibat = pval.intval;

	if(manager->chg_status == POWER_SUPPLY_STATUS_DISCHARGING || manager->vbus_type == VBUS_TYPE_NONE){
		manager->charge_full = 0;
		manager->fake_batt_status = -EINVAL;
		lx_info("%s: adapter pluged, reset charge_full flag \n", __func__);
	}

	eff_fv = get_effective_result(manager->fv_votable) - max(manager->smart_batt, manager->smart_fv);

	if (manager->charge_full) {
		manager->full_cnt = 0;
		if (manager->soc < EEA_RECHARGE_SOC && (-100 <= manager->tbat && manager->tbat <= 450)) {//not in charging pause with jeita
			manager->recharge_cnt++;
		} else {
			manager->recharge_cnt = 0;
			if ((manager->soc >= EEA_RECHARGE_SOC) && (manager->prp_is_enable == false)) {
				lx_info("%s: keep full and limit ibus between full and recharge unless replenish happend\n", __func__);
				manager->fake_batt_status = POWER_SUPPLY_STATUS_FULL;
				charge_enable_eea(manager, false);
				input_current_limit_eea(manager, false);

				ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
				if (POWER_SUPPLY_STATUS_FULL != pval.intval) {
					lxchg_charge_status_update(manager);
					power_supply_changed(manager->batt_psy);
				}
				if (manager->soc == 99 && manager->ibat >= 0) {
					fuel_gauge_status_update(manager->fuel_gauge);
					lx_info("%s:charge_full update 100\n", __func__);
				}
			}
		}

		if ((manager->recharge_cnt > RECHARGE_COUNT)) {
			manager->recharge_cnt = 0;
			manager->charge_full = false;
			manager->fake_batt_status = -EINVAL;
			manager->fg_full_cnt = 0;
			manager->prp_is_enable = false;
			input_current_limit_eea(manager, false);
			charge_enable_eea(manager, true);
			vote(manager->total_fcc_votable, MAIN_FCC_TAPER_VOTER, false, 0);
			lx_info("%s: Forced recharge\n", __func__);
		}

	} else {
		manager->recharge_cnt = 0;

		if (manager->vbat > (eff_fv - 10) || (g_policy->cp_charge_done && manager->iterm_changed && ((ibat / 1000) > 1000))) {
			total_fcc = get_effective_result(manager->total_fcc_votable);
			if ((total_fcc > (ibat / 1000)) && ((ibat / 1000) > manager->v_iterm)) {
				switch ((total_fcc - (ibat / 1000)) / FCC_TAPER_STEP_MA) {
				case 1:
					total_fcc = (ibat / 1000) + FCC_TAPER_STEP_MA;
					break;
				case 2:
					total_fcc = (ibat / 1000) + (2 * FCC_TAPER_STEP_MA);
					break;
				default:
					total_fcc = ibat / 1000;
					break;
				} // for soft CV: FCC decrease by 50-100mA a step
			}
			if (((total_fcc - FCC_TAPER_STEP_MA) < manager->v_iterm) || (((ibat / 1000) - FCC_TAPER_STEP_MA) < manager->v_iterm))
				total_fcc =  manager->v_iterm + FCC_TAPER_STEP_MA;
			vote(manager->total_fcc_votable, MAIN_FCC_TAPER_VOTER, true, total_fcc - FCC_TAPER_STEP_MA);
			if (manager->iterm_changed)
				manager->iterm_changed = false;
		}

		if (manager->vbat > eff_fv - 2) {
			lx_info("%s: fg vbat:%d over eff_fv:%d\n", __func__, manager->vbat, eff_fv);
			manager->full_cnt = CHARGE_FULL_COUNT;
		}

		if (manager->soc >= 99 && (manager->vbat > (eff_fv - 10)) && ((ibat / 1000) < jeita_chip->iterm_curr)) {
			manager->full_cnt++;
		} else if (manager->full_cnt == CHARGE_FULL_COUNT) {

		} else {
			manager->full_cnt = 0;
		}

		if (manager->full_cnt >= CHARGE_FULL_COUNT) {
			manager->full_cnt = 0;
			manager->charge_full = true;
			manager->fake_batt_status = POWER_SUPPLY_STATUS_FULL;
			full_flag = true;
			//fuel_gauge_set_charger_to_full(manager->fuel_gauge);
		}
	}

	lx_info("%s:full=%d, full_cnt=%d, recharge_cnt=%d %d fv:%d vbat:%d ibat:%d iterm:%d\n",
		__func__, manager->charge_full, manager->full_cnt, manager->recharge_cnt, manager->bbc_charge_done, eff_fv, manager->vbat, ibat/1000, jeita_chip->iterm_curr);
}

static void charger_done_by_iterm(struct charger_manager *manager)
{
	int ret = 0;
	union power_supply_propval pval = {0,};
	int threshold_mv = 150;
	struct lx_jeita_info *jeita_chip = manager->jeita_chip;
	int total_fcc;
	int eff_fv;
	int ibat = 0;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0)
		lx_err("get battery current error.\n");
	else
		ibat = pval.intval;

	if(manager->chg_status == POWER_SUPPLY_STATUS_DISCHARGING || manager->vbus_type == VBUS_TYPE_NONE){
		manager->charge_full = 0;
		manager->fake_batt_status_hot = -EINVAL;
		lx_info("%s: adapter pluged, reset charge_full flag \n", __func__);
	}

	eff_fv = get_effective_result(manager->fv_votable) - max(manager->smart_batt, manager->smart_fv);

	//check battery full or rechage
	if (manager->charge_full) {
		manager->full_cnt = 0;
		if (manager->vbat < eff_fv - threshold_mv) {//not in jeita hot
			manager->recharge_cnt++;
		} else {
			manager->recharge_cnt = 0;
		}

		if (manager->recharge_cnt > RECHARGE_COUNT && manager->rsoc < 9730) {
			manager->recharge_cnt = 0;
			manager->charge_full = false;
			manager->fg_full_cnt = 0;
			manager->prp_is_enable = false;
			manager->fake_batt_status_hot = -EINVAL;
			charge_enable_eea(manager, true);
			vote(manager->total_fcc_votable, MAIN_FCC_TAPER_VOTER, false, 0);
		}
	} else {
		manager->recharge_cnt = 0;
		if (manager->vbat > (eff_fv - 10) || (g_policy->cp_charge_done && manager->iterm_changed && ((ibat / 1000) > 1000))) {
			total_fcc = get_effective_result(manager->total_fcc_votable);
			lx_info("total_fcc = %d, ibat = %d, v_iterm = %d, iterm_changed = %d\n", total_fcc, ibat / 1000, manager->v_iterm, manager->iterm_changed);
			if ((total_fcc > (ibat / 1000)) && ((ibat / 1000) > manager->v_iterm)) {
				switch ((total_fcc - (ibat / 1000)) / FCC_TAPER_STEP_MA) {
				case 1:
					total_fcc = (ibat / 1000) + FCC_TAPER_STEP_MA;
					break;
				case 2:
					total_fcc = (ibat / 1000) + (2 * FCC_TAPER_STEP_MA);
					break;
				default:
					total_fcc = (ibat / 1000);
					break;
				} // for soft CV: FCC decrease by 50-100mA a step
			}
			if (((total_fcc - FCC_TAPER_STEP_MA) < manager->v_iterm) || (((ibat / 1000) - FCC_TAPER_STEP_MA) < manager->v_iterm))
				total_fcc =  manager->v_iterm + FCC_TAPER_STEP_MA;
			vote(manager->total_fcc_votable, MAIN_FCC_TAPER_VOTER, true, total_fcc - FCC_TAPER_STEP_MA);
			if (manager->iterm_changed)
				manager->iterm_changed = false;
		}
		if (manager->vbat > eff_fv - 2) {
			lx_info("%s: fg vbat:%d over eff_fv:%d\n", __func__, manager->vbat, eff_fv);
			manager->full_cnt = CHARGE_FULL_COUNT;
		}

		if (manager->soc == 100 && (manager->vbat > (eff_fv - 10)) && ((ibat / 1000) < jeita_chip->iterm_curr)) {
			manager->full_cnt++;
		} else if (manager->full_cnt == CHARGE_FULL_COUNT) {

		} else {
			manager->full_cnt = 0;
		}

		if (manager->full_cnt >= CHARGE_FULL_COUNT) {
			manager->full_cnt = 0;
			manager->charge_full = true;
			manager->fake_batt_status_hot = POWER_SUPPLY_STATUS_FULL;
			//fuel_gauge_set_charger_to_full(manager->fuel_gauge);
			charge_enable_eea(manager, false);
			full_flag = true;

			ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_STATUS, &pval);
			if (POWER_SUPPLY_STATUS_FULL != pval.intval) {
				lxchg_charge_status_update(manager);
				power_supply_changed(manager->batt_psy);
			}
		}
	}
	lx_info("%s:ffc=%d, soc=%d, temp=%d, pd_active=%d, fv=%d, vbat=%d, iterm=%d, ibat=%d, full=%d, full_cnt=%d, recharge_cnt=%d fake_batt_status_hot:%d\n",
					__func__, manager->ffc, manager->soc, manager->tbat, manager->pd_active, eff_fv, manager->vbat, jeita_chip->iterm_curr,\
					ibat/1000, manager->charge_full, manager->full_cnt, manager->recharge_cnt, manager->fake_batt_status_hot);
}

#define RECHG_ENABLE_CP_SOC 90
static void charge_iterm_work_func(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work, struct charger_manager, charge_iterm_monitor_work.work);
	union power_supply_propval pval = {0,};
	int ret;
	struct bq_fg_chip *bp = NULL;
	int eff_fv;

	bp = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bp))
		return;

	ret = chargerpump_get_is_enable(manager->master_cp_chg, &manager->cp_enable);
	if (ret < 0) {
		lx_err("cp_enabled channel read failed, ret=%d\n", ret);
	}

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0)
		lx_err("get battery volt error.\n");
	else
		manager->vbat = pval.intval / 1000;

	manager->ffc = fuel_gauge_get_fastcharge_mode(manager->fuel_gauge);
	manager->is_eu_mode = get_eu_mode();
	charger_is_charge_done(manager->charger, &(manager->bbc_charge_done));
	eff_fv = get_effective_result(manager->fv_votable) - max(manager->smart_batt, manager->smart_fv);
	if (manager->vbat >= (eff_fv - 40) && (manager->ibat / 1000) <= (manager->v_iterm + 100)\
		&& !manager->cp_enable && manager->rsoc >= 9000 && (manager->ibat / 1000) > 0\
		&& (-100 <= manager->tbat) && !get_warm_stop_charge_state()){
		if (manager->fg_full_cnt == 0) {
			manager->fg_full_cnt++;
			fuel_gauge_set_charger_to_full(manager->fuel_gauge);
		}
	}
	manager->rsoc = bp->raw_soc;
	if (full_flag && manager->soc < RECHG_ENABLE_CP_SOC && manager->pd_active >= CHARGE_PD_ACTIVE && g_policy->state == POLICY_STOP) {
		chargerpump_policy_start(g_policy);
		full_flag = false;
		ret = chargerpump_get_is_enable(manager->master_cp_chg, &manager->cp_enable);
		if (ret < 0) {
			lx_err("cp_enabled channel read failed, ret=%d\n", ret);
		}
		lx_info("soc(%d) < 90,enable cp!!", manager->soc);
	}
	if (manager->is_eu_mode) {
		if (manager->ffc && !manager->cp_enable) {
			handle_recharge_eea_by_iterm(manager);
		} else {
			handle_recharge_eea(manager);
		}
	} else {
		if (manager->ffc && !manager->cp_enable) {
			charger_done_by_iterm(manager);
		} else if (!manager->ffc && manager->bbc_charge_done && manager->rsoc < 9730) {
			if ((-100 <= manager->tbat) && !get_warm_stop_charge_state()){
				charge_enable_eea(manager, true);
				manager->fg_full_cnt = 0;
			}
		}
	}

	//xiao pps adapter
	if (manager->ffc) {
		handle_power_replenish(manager);
	}

	schedule_delayed_work(&manager->charge_iterm_monitor_work, msecs_to_jiffies(2000));
}

#define CIS_ALERT 			"CIS_ALERT"
static int handle_battery_cis(struct charger_manager *manager)
{
	union power_supply_propval val = {0,};
	int rc = 0;
	int cis_alert_status = 0;
	int jeia_fcc;
	int main_fcc;
	struct lx_jeita_info *jeita_chip = manager->jeita_chip;

	if (IS_ERR_OR_NULL(manager->fg_psy)) {
		manager->fg_psy = power_supply_get_by_name("bms");
		if (IS_ERR_OR_NULL(manager->fg_psy)) {
			lx_err("failed to get bms psy\n");
			return PTR_ERR(manager->fg_psy);
		}
	}

	rc = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP_AMBIENT_ALERT_MIN, &val);
	if (rc < 0) {
		lx_err("Failed to get cis alert, rc=%d\n", rc);
		return rc;
	}
	cis_alert_status = val.intval;

	if (cis_alert_status &&
		(manager->chg_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		manager->chg_status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
		manager->vbus_type == POWER_SUPPLY_TYPE_UNKNOWN)) {
			vote(manager->total_fcc_votable, CIS_ALERT, false, 0);
			vote(manager->main_fcc_votable, CIS_ALERT, false, 0);
			vote(manager->fv_votable, CIS_ALERT, false, 0);
			lx_info("adapter plug out while cis_alert, reset cis status \n");
			return 0;
	}

	jeia_fcc = get_client_vote(manager->total_fcc_votable, JEITA_VOTER);
	if (jeia_fcc < 0) {
		lx_info("jeia_fcc:%d \n", jeia_fcc);
		return -EINVAL;
	}

	main_fcc = get_client_vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER);
	if (main_fcc < 0) {
		lx_info("main_fcc:%d \n", main_fcc);
		return -EINVAL;
	}

	if (cis_alert_status >= 3) {
		if (manager->pd_active == CHARGE_PD_PPS_ACTIVE) {
			vote(manager->total_fcc_votable, CIS_ALERT, true, (4 * jeia_fcc / 5));
		} else {
			vote(manager->main_fcc_votable, CIS_ALERT, true, (4 * main_fcc / 5));
		}
		vote(manager->fv_votable, CIS_ALERT, true, (jeita_chip->fv - 20));
		lx_info("cis vote fv:%d, total_fcc:%d, main_fcc:%d\n", jeita_chip->fv - 20, 4 * jeia_fcc / 5, 4 * main_fcc / 5);
	} else if (is_client_vote_enabled(manager->total_fcc_votable, CIS_ALERT) || is_client_vote_enabled(manager->main_fcc_votable, CIS_ALERT)) {
		vote(manager->total_fcc_votable, CIS_ALERT, false, 0);
		vote(manager->main_fcc_votable, CIS_ALERT, false, 0);
		vote(manager->fv_votable, CIS_ALERT, false, 0);
		lx_info("cis alert is cancled, reset cis status \n");
	}

	return rc;
}

static void lxchg_charge_status_update(struct charger_manager *manager)
{
	int ret = 0;
	int state = 0, status = 0;
	int vindpm_status = 0;
	int ibus;
	int vbus;
	union power_supply_propval pval;
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	int warm_stop_charge = 0;
	bool online = false;
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        struct votable	*is_full_votable = NULL;
        int is_full_flag = 0;
#endif
	ret = charger_get_chg_status(manager->charger, &state, &status);
	if (ret < 0) {
		lx_err("failed to get chg status prop\n");
		return;
	}
	if ((status == POWER_SUPPLY_STATUS_DISCHARGING) && !manager->otg_status) {
		charger_get_online(manager->charger, &online);
		if (online)
			status = POWER_SUPPLY_STATUS_CHARGING;
	}

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	warm_stop_charge = get_warm_stop_charge_state();
	if (!IS_ERR_OR_NULL(manager->batt_psy)) {
		ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret < 0)
			lx_err("get battery volt error.\n");
		else
			manager->vbat = pval.intval/1000;
	} else {
		ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (ret < 0)
			lx_err("get battery volt error.\n");
		else
			manager->vbat = pval.intval/1000;
	}

	if ((status != POWER_SUPPLY_STATUS_DISCHARGING) &&
		(!is_input_suspend(manager))) {
		if (manager->tbat >= BATTERY_HOT_TEMP)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else if ((manager->tbat >= BATTERY_WARM_TEMP || warm_stop_charge))
			status = POWER_SUPPLY_STATUS_CHARGING;
		else if ((manager->tbat < BATTERY_WARM_TEMP && manager->vbat < 4100))
			status = POWER_SUPPLY_STATUS_CHARGING;
	}
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	if ((status != POWER_SUPPLY_STATUS_FULL && manager->soc < 99) && manager->charger_online) {
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else if ((status == POWER_SUPPLY_STATUS_CHARGING) && (!IS_ERR_OR_NULL(manager->batt_psy))) {
		ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
		if (ret < 0)
			lx_err("get battery soc error.\n");
		else
			manager->soc = pval.intval;

		is_full_votable = find_votable("IS_FULL");
		if (!is_full_votable) {
			lx_err("failed to get is_full_votable\n");
			return;
		}
		is_full_flag = get_effective_result(is_full_votable);
		if (is_full_flag < 0) {
			lx_err("failed to get is_full_flag\n");
			return;
		}

	}
	lx_debug("new soc is 100, don't keep report full, status = %d\n", status);
	if (manager->is_eu_mode && (manager->fake_batt_status == POWER_SUPPLY_STATUS_FULL)) {
		status = manager->fake_batt_status;
	}else if (manager->charger_online && manager->fake_batt_status_hot == POWER_SUPPLY_STATUS_FULL && manager->ffc && !manager->cp_enable) {
		status = manager->fake_batt_status_hot;
	}
#endif
	vindpm_status = charger_get_vindpm_status(manager->charger);
	charger_get_adc(manager->charger, ADC_GET_VBUS, &vbus);
	charger_get_adc(manager->charger, ADC_GET_IBUS, &ibus);

	if (vindpm_status && ibus < 100 && vbus < manager->vindpm_vot) {
		lx_err("vindpm_status && ibus is 0 \n");
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	/* input suspend overlay charge status */
	if (manager->input_suspend || manager->otg_status)
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	
	if (is_replenish_full)
		status = POWER_SUPPLY_STATUS_FULL;
	lx_debug("chg_status=%d, fake_batt_status = %d, fake_batt_status_hot = %d, chg_state = %d, is_replenish_full = %d\n", status, manager->fake_batt_status, manager->fake_batt_status_hot, state, is_replenish_full);
	manager->chg_status = status;
	manager->chg_state = state;
}

static const char *get_vote_effective_info(struct votable *votable, int *result)
{
	*result = get_effective_result(votable);
	return get_effective_client(votable);
}

void lxchg_dump_vote_info(struct charger_manager *manager)
{
	manager->total_effect_voter = get_vote_effective_info(manager->total_fcc_votable, &manager->vote_total_fcc);
	manager->icharge_effect_voter = get_vote_effective_info(manager->main_fcc_votable, &manager->vote_icharge);
	manager->input_effect_voter = get_vote_effective_info(manager->main_icl_votable, &manager->vote_input_limit);
	manager->fv_effect_voter = get_vote_effective_info(manager->fv_votable, &manager->vote_fv);
	manager->iterm_effect_voter = get_vote_effective_info(manager->iterm_votable, &manager->vote_iterm);

	lx_info("total_ffc=%d[%s], ichg=%d[%s], input=%d[%s], fv=%d[%s], iterm=%d[%s]\n",
		manager->vote_total_fcc, manager->total_effect_voter,
		manager->vote_icharge, manager->icharge_effect_voter, manager->vote_input_limit,
		manager->input_effect_voter, manager->vote_fv, manager->fv_effect_voter,
		manager->vote_iterm, manager->iterm_effect_voter);
}

static void charger_manager_monitor(struct charger_manager *manager)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	uint32_t adc_buf_len = 0;
	uint8_t i = 0;
	char adc_buf[MIAN_CHG_ADC_LENGTH + 1];
	uint32_t iterm = 0;
	uint32_t fv = 0;
	uint32_t ilimit = 0;
	int ichg = 0;

	lxchg_charge_status_update(manager);

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		lx_err("get battery soc error.\n");
	else
		manager->soc = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0)
		lx_err("get battery volt error.\n");
	else
		manager->vbat = pval.intval / 1000;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0)
		lx_err("get battery current error.\n");
	else
		manager->ibat = pval.intval;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0)
		lx_err("get battery current error.\n");
	else
		manager->tbat = pval.intval;

	charger_get_term_curr(manager->charger, &iterm);

	manager->avg_ibat = fuel_gauge_get_avg_ibat(manager->fuel_gauge);

	ret = get_effective_result(manager->iterm_votable);
	if (ret < 0) {
		lx_err("failed to get iterm vote\n");
	}
	if (manager->v_iterm != ret) {
		manager->v_iterm = ret;
		manager->iterm_changed = true;
	}

	charger_get_term_volt(manager->charger, &fv);

	manager->charge_en = charger_get_chg(manager->charger);

	charger_get_adc(manager->charger, ADC_GET_VBUS, &manager->vbus);
	charger_manager_get_current(manager, &manager->ibus);

	if (g_policy->sm == PM_STATE_CHARGERPUMP_CC_CV) {
		ichg = manager->vote_total_fcc;
		ilimit = g_policy->request_curr;
	} else {
		charger_get_ichg(manager->charger, &ichg);
		charger_get_input_curr_lmt(manager->charger, &ilimit);
	}

	ktime_get_real_ts64(&manager->ts64);
	manager->tv = *(const struct timespec *)&manager->ts64;
	manager->tv.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(manager->tv.tv_sec, &manager->tm);

#ifdef FACTORY_BUILD
	if (manager->charger_online)
		ato_control_soc(manager);
#else
	manager->tm.tm_hour += 8;
	if (manager->tm.tm_hour >= 24)
		manager->tm.tm_hour -= 24;
#endif

	ret = handle_battery_cis(manager);
	if (ret < 0)
		lx_err("handle_battery_cis fail.\n");

	lx_info("[%02d-%02d %02d:%02d:%02d] vbus:%d(%d),vbat:%d,tbat:%d,chg_status:%s,vbus_type:%s(%d-%d),ibus:%d(%d),ibat:%d(%d),term:%d|%d,input_suspend:%d,uisoc:%d(%d)\n",
				manager->tm.tm_mon + 1, manager->tm.tm_mday, manager->tm.tm_hour, manager->tm.tm_min, manager->tm.tm_sec,
				manager->vbus, manager->charger_online, manager->vbat, manager->tbat, chg_stat[manager->chg_status], bc12_result[manager->vbus_type],
				manager->pd_active, manager->pd_adapter->verifed, manager->ibus, ilimit, manager->ibat/1000, ichg, iterm, fv,
				manager->input_suspend, manager->soc,manager->rsoc);

	lxchg_dump_vote_info(manager);

	power_supply_changed(manager->usb_psy);
	power_supply_changed(manager->batt_psy);
	if (manager->soc <= 1 && !manager->poweroff_flag) {
		manager->poweroff_flag = true;
		schedule_delayed_work(&manager->power_off_check_work, msecs_to_jiffies(2000));
	} else if (manager->soc > 1 && manager->poweroff_flag) {
		manager->poweroff_flag = false;
		shutdown_delay_count = 0;
	}

	for (i = 0; i < ADC_GET_MAX; i++) {
		ret = charger_get_adc(manager->charger, i, &manager->chg_adc[i]);
		if (ret < 0) {
			lx_info("get adc failed\n");
			continue;
		}
		adc_buf_len += sprintf(adc_buf + adc_buf_len,
						"%s : %d,", adc_name[i], manager->chg_adc[i]);
	}

	if (adc_buf_len > MIAN_CHG_ADC_LENGTH)
		adc_buf[MIAN_CHG_ADC_LENGTH] = '\0';
	lx_debug("%s\n", adc_buf);
}

static void power_off_check_work(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int rc = 0;
#endif
	struct charger_manager *manager = container_of(work,
					struct charger_manager, power_off_check_work.work);

	static char uevent_string[][MAX_UEVENT_LENGTH + 1] = {
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n", //28
	};

	static char *envp[] = {
		uevent_string[0],
		NULL,
	};
	int vbat = 0, ibat = 0;
	int ret = 0;
	union power_supply_propval pval = {0,};

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	if (ret < 0)
		lx_err("get battery volt error.\n");
	else
		vbat = pval.intval / 1000;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if (ret < 0)
		lx_err("get battery curr error.\n");
	else
		ibat = pval.intval;

	lx_info("vbat = %d, ibat = %d, shutdown_delay_voltage = %d, shutdown_force_voltage = %d, shutdown_delay_count = %d\n", vbat, ibat, shutdown_delay_voltage, shutdown_force_voltage, shutdown_delay_count);

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	rc = fuel_gauge_check_i2c_function(manager->fuel_gauge);
	if (manager->soc == 1 || (rc && manager->vbat)) {
#else
	if (manager->soc == 1) {
#endif
		if (((vbat < shutdown_delay_voltage) || (ibat < SHUTDOWN_DELAY_IBAT)) &&
			(vbat > shutdown_force_voltage) && (manager->chg_status != POWER_SUPPLY_STATUS_CHARGING)) {
				shutdown_delay_count ++;
				if (shutdown_delay_count >= 4) {
					manager->shutdown_delay = true;
				}
		} else if (manager->chg_status == POWER_SUPPLY_STATUS_CHARGING
						&& manager->shutdown_delay) {
				manager->shutdown_delay = false;
				shutdown_delay_count = 0;
		} else {
			manager->shutdown_delay = false;
			shutdown_delay_count = 0;
		}
	} else {
		manager->shutdown_delay = false;
		shutdown_delay_count = 0;
	}

	if (manager->last_shutdown_delay != manager->shutdown_delay) {
		manager->last_shutdown_delay = manager->shutdown_delay;
		power_supply_changed(manager->usb_psy);
		power_supply_changed(manager->batt_psy);
		if (manager->shutdown_delay == true)
			strncpy(uevent_string[0] + 28, "1", MAX_UEVENT_LENGTH - 28);
		else
			strncpy(uevent_string[0] + 28, "0", MAX_UEVENT_LENGTH - 28);
		mdelay(1000);
		lx_err("envp[0] = %s\n", envp[0]);
		lx_info("shutdown_delay_count:%d\n", shutdown_delay_count);
		kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);
	}

	if (manager->soc <= 1) {
		if (shutdown_delay_count < 4 && shutdown_delay_count > 0) {
			schedule_delayed_work(&manager->power_off_check_work, msecs_to_jiffies(1000));
		} else {
			schedule_delayed_work(&manager->power_off_check_work, msecs_to_jiffies(2000));
		}
	}
}

static int charger_manager_check_vindpm(struct charger_manager *manager, uint32_t vbat)
{
	struct charger_dev *charger = manager->charger;
	int ret = 0;

	if (manager->ctoc_chg) {
		manager->vindpm_vot = CHARGER_VINDPM_DYNAMIC_VALUE3;
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
		if (ret < 0) {
			lx_err("Failed to set vindpm, ret = %d\n", ret);
			return ret;
		}
		return 0;
	}

#if CHARGER_VINDPM_USE_DYNAMIC
	if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT1) {
		manager->vindpm_vot = CHARGER_VINDPM_DYNAMIC_VALUE1;
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE1);
	} else if (vbat < CHARGER_VINDPM_DYNAMIC_BY_VBAT2) {
		manager->vindpm_vot = CHARGER_VINDPM_DYNAMIC_VALUE2;
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE2);
	} else {
		manager->vindpm_vot = CHARGER_VINDPM_DYNAMIC_VALUE3;
		ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
	}
#else
	manager->vindpm_vot = CHARGER_VINDPM_DYNAMIC_VALUE3;
	ret = charger_set_input_volt_lmt(charger, CHARGER_VINDPM_DYNAMIC_VALUE3);
#endif

	if (ret < 0) {
		lx_err("Failed to set vindpm, ret = %d\n", ret);
		return ret;
	}
	return 0;
}

#define PD_BOOST_VBUS_THD             6000
#define DEFAULT_ICL 100
#define OUTDOOR_CURR 1900
static int charger_manager_check_iindpm(struct charger_manager *manager, uint32_t vbus_type)
{
	int ret = 0;
	int ichg_ma = 0;
	int icl_ma = 0;
	int vbus_val = 0;
	static int mtbf_current = 0;

	switch (vbus_type) {
	case VBUS_TYPE_NONE:
		ichg_ma = 100;//100mA
		icl_ma = DEFAULT_ICL;//0A
		break;
	case VBUS_TYPE_FLOAT:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_SDP:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	case VBUS_TYPE_NON_STAND:
		ichg_ma = manager->float_current;
		icl_ma = manager->float_current;
		break;
	case VBUS_TYPE_CDP:
		ichg_ma = manager->cdp_current;
		icl_ma = manager->cdp_current;
		break;
	case VBUS_TYPE_DCP:
		ichg_ma = manager->dcp_current;
		icl_ma = manager->dcp_current;
		if(manager->outdoor_flag == 1) {
			ichg_ma = OUTDOOR_CURR;
			icl_ma = OUTDOOR_CURR;
		}
		break;
	case VBUS_TYPE_HVDCP:
		ichg_ma = manager->hvdcp_charge_current;
		icl_ma = manager->hvdcp_input_current;
		break;
	case VBUS_TYPE_HVDCP_3:
	case VBUS_TYPE_HVDCP_3P5:
		ichg_ma = manager->hvdcp3_charge_current;
		icl_ma = manager->hvdcp3_input_current;
		break;
	default:
		ichg_ma = manager->usb_current;
		icl_ma = manager->usb_current;
		break;
	}

	if (manager->pd_active != CHARGE_PD_INVALID && vbus_type) {
		if (manager->pd_volt_max == 5000) {  //C-to-C
			ichg_ma = manager->pd_curr_max;
			icl_ma = manager->pd_curr_max;
			if (manager->ctoc_chg && manager->pd_curr_max > 1000)
				icl_ma = 1000;
		} else {  //PD2.0
			ichg_ma = manager->pd_curr_max * PD20_ICHG_MULTIPLE / 1000;  //1.8 of fixed current
			icl_ma = min(manager->pd_curr_max, 2000);

			if (charger_get_adc(manager->charger, ADC_GET_VBUS, &vbus_val) < 0)
				lx_info("%s get vbus_val failed\n", __func__);
/*
			if (vbus_val > PD_BOOST_VBUS_THD) {
				msleep(200);
				icl_ma = manager->pd_curr_max;
			} else {
				lx_info("icl default for ctoc 18w, vbus=%d icl=%d\n", vbus_val, DEFAULT_ICL);
			}
*/
		}
		if(manager->soc > 95 && !manager->ctoc_chg)
			vote(manager->main_icl_votable, CHARGER_HIGH_SOC_VOTER, true, 2300);
		else
			vote(manager->main_icl_votable, CHARGER_HIGH_SOC_VOTER, false, 2300);
		// normal 1950 Max 3A maybe vbus err, for bat1 Buck 3138mA iterm
	}

	if (is_mtbf_mode && (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP)) {
		mtbf_current = fuel_gauge_get_mtbf_current(manager->fuel_gauge);
		ichg_ma = mtbf_current;
		if (mtbf_current < 2100)
			icl_ma = mtbf_current;
		lx_info("is_mtbf_mode=%d icl=%d ichg=%d\n", is_mtbf_mode, icl_ma, ichg_ma);
	}

	if (manager->delay_iindpm) {
		msleep(1500);
		manager->delay_iindpm = false;
	}

	if (IS_ERR_OR_NULL(manager->main_icl_votable)) {
		lx_err("main_icl_votable not found\n");
		return PTR_ERR(manager->main_icl_votable);
	} else {
		if(manager->soc > 95 && manager->jeita_chip->iterm_curr > 3000 && manager->pd_adapter->verifed == 1 && !manager->ctoc_chg)
			vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 2300); //for iterm > 3A
		else
			vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, icl_ma);
	}

	if (IS_ERR_OR_NULL(manager->main_fcc_votable)) {
		lx_err("main_fcc_votable not found\n");
		return PTR_ERR(manager->main_fcc_votable);
	} else
		vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, true, ichg_ma);

	/* IINDPM bits are changed automatically after input source type detection is completed. So need rerun_election */
	rerun_election(manager->main_icl_votable);
	lx_info("ichg_ma = %d, icl_ma = %d\n", ichg_ma, icl_ma);
	return ret;
}

static void charger_manager_timer_func(struct timer_list *timer)
{
	struct charger_manager *manager = container_of(timer,
							struct charger_manager, charger_timer);
	charger_manager_wake_thread(manager);
}

int charger_manager_start_timer(struct charger_manager *manager, uint32_t ms)
{
	del_timer(&manager->charger_timer);
	manager->charger_timer.expires = jiffies + msecs_to_jiffies(ms);
	manager->charger_timer.function = charger_manager_timer_func;
	add_timer(&manager->charger_timer);
	return 0;
}
EXPORT_SYMBOL(charger_manager_start_timer);

void disable_chg_comm_ctrl(struct charger_manager *manager, const char *client_str, bool en)
{
	if ((manager == NULL) || !manager->total_fcc_votable)
		return;

	if (en) {
		vote(manager->total_fcc_votable, client_str, true, 0);

		if (g_policy->state == POLICY_RUNNING){
			chargerpump_policy_stop(g_policy);
			lx_info("disable cp\n");
		}
	} else {
		vote(manager->total_fcc_votable, client_str, false, 0);

		if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START)){
			chargerpump_policy_start(g_policy);
			lx_info("enable cp\n");
		}
	}
}
EXPORT_SYMBOL(disable_chg_comm_ctrl);

static int reset_vote(struct charger_manager *manager)
{
	vote(manager->main_fcc_votable, CHARGER_TYPE_VOTER, false, 0);
	vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, false, 0);
	vote(manager->main_icl_votable, CHARGER_HIGH_SOC_VOTER, false, 0);
	vote(manager->main_icl_votable, BMS_AUTHENTIC_VOTER, false, 0);
	vote(manager->main_icl_votable, TYPEC_SINK_VBUS_VOTER, false, 0);
	vote(manager->total_fcc_votable, JEITA_VOTER, false, 0);
	vote(manager->total_fcc_votable, MAIN_FCC_TAPER_VOTER, false, 0);
	vote(manager->total_fcc_votable, EEA_CHARGE_FULL, false, 0);
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	vote(manager->main_fcc_votable, FG_I2C_ERR, false, 0);
	vote(manager->main_icl_votable, FG_I2C_ERR, false, 0);
#endif
	vote(manager->total_fcc_votable, CIS_ALERT, false, 0);
	vote(manager->fv_votable, CIS_ALERT, false, 0);
	vote(manager->fv_votable, POWER_REPLENISH_VOTER, false, 0);
	vote(manager->iterm_votable, POWER_REPLENISH_VOTER, false, 0);
	vote(manager->total_fcc_votable, FV_OVERVOLTAGE_VOTER, false, 0);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	vote(manager->total_fcc_votable, SMART_BYPASS_VOTE, false, 0);
#endif
	vote(manager->main_icl_votable, POWER_REPLENISH_VOTER, false, POWER_REPLENISH_ICL);
	vote(manager->total_fcc_votable, POWER_REPLENISH_VOTER, false, 0);
	vote(manager->total_fcc_votable, ENDURANCE_PRO_VOTER, false, 0);
#ifdef FACTORY_BUILD
	vote(manager->main_icl_votable, ATO_SOC_LIMIT_VOTER, false, 0);
#endif
	return 0;
}

static int rerun_vote(struct charger_manager *manager)
{
	rerun_election(manager->total_fcc_votable);
	rerun_election(manager->main_fcc_votable);
	rerun_election(manager->main_icl_votable);
	return 0;
}

static void apsd_second_detect_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, second_detect_work.work);
	lx_info("apsd enter!\n");
	if (manager->float_count < 10) {
		manager->float_count++;
		charger_get_vbus_type(manager->charger, &manager->vbus_type);
		lx_info("bc12 retry count = %d,bc_type = %d\n", manager->float_count, manager->vbus_type);
		if ((manager->vbus_type != VBUS_TYPE_NONE && manager->vbus_type != VBUS_TYPE_NON_STAND &&
			manager->vbus_type != VBUS_TYPE_FLOAT) || (manager->pd_active == 1)) {
			lx_info("bc12 retry is useful !\n");
			manager->float_count = 0;
			return;
		}
		charger_force_dpdm(manager->charger, false);
		charger_force_dpdm(manager->charger, true);
		schedule_delayed_work(&manager->second_detect_work, msecs_to_jiffies(FLOAT_DELAY_TIME));
	} else {
		lx_err("bc12 retry count > 50s,bc_type still Non-standard!\n");
	}
}

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
static bool get_usb_ready(struct charger_manager *manager)
{
	bool ready = true;

	if (IS_ERR_OR_NULL(manager->usb_node))
		manager->usb_node = of_parse_phandle(manager->dev->of_node, "usb", 0);
	if (!IS_ERR_OR_NULL(manager->usb_node)) {
		ready = !of_property_read_bool(manager->usb_node, "cdp-block");
		if (ready || manager->get_usb_rdy_cnt % 10 == 0)
			lx_info("usb ready = %d\n", ready);
	} else
		lx_err("usb node missing or invalid\n");

	if (ready == false && (manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT || manager->pd_active)) {
		if (manager->pd_active)
			manager->get_usb_rdy_cnt = 0;
		lx_info("cdp-block timeout or pd adapter\n");
		return true;
	}

	return ready;
}

static void wait_usb_ready_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, wait_usb_ready_work.work);

	if (get_usb_ready(manager) || manager->get_usb_rdy_cnt >= WAIT_USB_RDY_MAX_CNT)
		wake_up(&manager->wait_queue);
	else {
		manager->get_usb_rdy_cnt++;
		schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(WAIT_USB_RDY_TIME));
	}
}
#endif

#if IS_ENABLED(CONFIG_MIEV)
void dfx_flag_reset_zero(struct charger_manager *manager)
{
    manager->dfx_none_standard_flag = 0;
    manager->dfx_batt_linker_absent_flag =0;
    manager->dfx_cp_err_flag = 0;
    manager->dfx_fg_i2c_err_flag = 0;
    manager->dfx_bat_temp_abnormal_flag = 0;
    manager->dfx_lpd_check_flag = 0;
	/*add new*/
	manager->dfx_cp_ibat_ocp_flag = 0;
	manager->dfx_cp_vbat_ovp_flag = 0;
	manager->dfx_cp_ibus_ocp_flag = 0;
	manager->dfx_cp_vbus_ovp_flag = 0;
	manager->dfx_cp_tdie_hot_flag = 0;
	manager->dfx_cp_enable_flag = 0;
	manager->dfx_pd_pps_authen_flag = 0;
	manager->dfx_global_soc_over_80_flag = 0;
	manager->dfx_global_soc_trig_flag = 0;
	manager->dfx_high_temp_discharging_flag = 0;
	manager->dfx_low_temp_discharging_flag = 0;
	manager->dfx_soc_not_full_flag = 0;
	manager->dfx_battery_auth_fail_flag = 0;
	manager->dfx_none_standard_charge_slow_flag = 0;
}
#endif

#define PD_VOUT_9V			9000
#define PD_VOUT_5V			5000
#define PD_IBUS_2A			2000
#define PD_BOOST_9V_CNT_MAX		3
#define PD_CONFIG_CHECK_INTERVAL	5000
static void pd_config_check(struct charger_manager *manager)
{
	int vbus_val = 0;
	struct pd_port *pd_port = &manager->tcpc->pd_port;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;

	if (manager->charger_online && manager->vbus_type != VBUS_TYPE_NONE &&
	    manager->pd_active == CHARGE_PD_ACTIVE) {
		if (charger_get_adc(manager->charger, ADC_GET_VBUS, &vbus_val) < 0)
			lx_info("get vbus_val failed\n");
		if (src_cap->nr >=2 && vbus_val < PD_BOOST_VBUS_THD && !manager->decrease_volt &&
		    manager->chg_status == POWER_SUPPLY_STATUS_CHARGING &&
                    manager->pd_boost_cnt < PD_BOOST_9V_CNT_MAX && !((!manager->authenticate || (manager->authenticate && !manager->ch)) && (manager->bms_auth_done))
                    && !manager->lpd_charging) {
			tcpm_set_pd_charging_policy(manager->tcpc,
				DPM_CHARGING_POLICY_MAX_POWER_LVIC, NULL);
			tcpm_dpm_pd_request(manager->tcpc, PD_VOUT_9V, PD_IBUS_2A, NULL);
			manager->pd_boost_cnt++;
			lx_info("pd boost cnt:%d %d %d\n", manager->pd_boost_cnt,manager->lpd_charging,manager->decrease_volt);
		}
		if (vbus_val >= PD_BOOST_VBUS_THD &&
		    manager->chg_status == POWER_SUPPLY_STATUS_FULL) {
			tcpm_dpm_pd_request(manager->tcpc, PD_VOUT_5V, PD_IBUS_2A, NULL);
			manager->pd_boost_cnt = 0;
			lx_info("pd buck vbus:%d ibus:%d\n", PD_VOUT_5V, PD_IBUS_2A);
		}
	} else {
		if (manager->pd_boost_cnt)
                  manager->pd_boost_cnt = 0;
	}
	lx_debug("chg_status:%d vbus_type:%d vbus_val:%d %d %d\n",
		manager->chg_status, manager->vbus_type, vbus_val,src_cap->nr,manager->pd_boost_cnt);
}

static void pd_config_check_work(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
					struct charger_manager, pd_config_check_work.work);
	lx_debug("pd_config_check_work enter!\n");
	pd_config_check(manager);
	schedule_delayed_work(&manager->pd_config_check_work,
				msecs_to_jiffies(PD_CONFIG_CHECK_INTERVAL));
}

static void tcpc_class_complete_init_work_func(struct work_struct *work)
{
	lx_err("start tcpc complete init\n");
	tcpc_class_complete_init();
}

static void charger_manager_charger_type_detect(struct charger_manager *manager)
{
	struct charger_dev *charger = manager->charger;
	struct chargerpump_dev *master_cp_chg = manager->master_cp_chg;
	struct chargerpump_dev *slave_cp_chg = manager->slave_cp_chg;
	static int mtbf_current = 0;
	struct bq_fg_chip *bp = NULL;
	int capacity;
  	int rc;
  	struct power_supply *batt_psy;
	union power_supply_propval pval = {0,};
	static bool cpu_qos_request = false;
#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
		static int lpd_first_detect = 1;
#endif
#endif

	charger_get_vbus_type(manager->charger, &manager->vbus_type);

	bp = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bp))
		return;

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		lx_err("Failed to get batt_psy\n");
	}
	rc = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		lx_err("failed to get soc");
	}
	capacity = pval.intval;
	if (manager->charger_online != manager->adapter_plug_in) {
		manager->adapter_plug_in = manager->charger_online;
		if (manager->adapter_plug_in) {
			pm_stay_awake(manager->dev);
			vote(manager->main_icl_votable, CHARGER_TYPE_VOTER, true, 100);
			manager->delay_iindpm = true;
			lx_info("adapter plug in\n");
			if (capacity >= 95)
				manager->first_plug = true;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			if ((manager->soc <= 20) && (manager->thermal_board_temp <= 390)) {
				manager->low_fast_plugin_flag = true;
			}
			manager->outdoor_flag = 0;
			manager->dfx_first_check_flag = false;
			manager->dfx_stand_chg_check_flag = false;
			manager->dfx_first_work_flag = false;
			manager->none_standard_count = 0;
			manager->last_fcc = 0;
#if IS_ENABLED(CONFIG_MIEV)
			dfx_flag_reset_zero(manager);
#endif
			schedule_delayed_work(&manager->xm_charge_work, msecs_to_jiffies(3000));
#endif
			manager->qc_detected = false;
			charger_adc_enable(charger, true);
			chargerpump_set_enable_adc(master_cp_chg, true);
			chargerpump_set_enable_adc(slave_cp_chg, true);
			charger_set_term(charger, true);
			schedule_delayed_work(&manager->charge_iterm_monitor_work, msecs_to_jiffies(1000));
			rerun_vote(manager);
			schedule_delayed_work(&manager->pd_config_check_work,
					      msecs_to_jiffies(PD_CONFIG_CHECK_INTERVAL));
		} else {
			chargerpump_set_enable_adc(master_cp_chg, false);
			chargerpump_set_enable_adc(slave_cp_chg, false);
			charger_adc_enable(charger, false);
			lx_info("adapter plug out\n");
			manager->pd_boost_cnt = 0;
			manager->full_cnt = 0;
			manager->recharge_cnt = 0;
			manager->charge_full = false;
			manager->first_plug = false;
			manager->fg_full_cnt = 0;
			manager->fake_batt_status_hot = -EINVAL;
			manager->fake_batt_status = -EINVAL;
			manager->prp_is_enable = false;
			manager->rp_work_count = 0;
			manager->ctoc_chg = false;
			manager->delay_iindpm = false;
			charge_enable_eea(manager, true);
			full_flag = false;
			is_replenish_full = false;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			manager->smart_ctrl_en = false;
			cancel_delayed_work(&manager->xm_charge_work);
			cancel_delayed_work(&manager->dfx_stand_chg_slow_check_work);
			manager->low_fast_plugin_flag = false;
			manager->pps_fast_mode = false;
			manager->b_flag = NORMAL;
                  	manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
        		manager->endurance_protect_flag = false;
#endif
			cancel_delayed_work_sync(&manager->second_detect_work);
			cancel_delayed_work_sync(&manager->charge_iterm_monitor_work);
			manager->float_count = 0;
			fuel_gauge_set_fastcharge_mode(manager->fuel_gauge, false);
			if (g_policy->state == POLICY_RUNNING)
				chargerpump_policy_stop(g_policy);
			reset_vote(manager);
			charger_force_dpdm(charger, false);
			pm_relax(manager->dev);
			cancel_delayed_work_sync(&manager->pd_config_check_work);
		}
	}

	mtbf_current = fuel_gauge_get_mtbf_current(manager->fuel_gauge);

	if (mtbf_current != 0) {
		is_mtbf_mode = 1;
	} else {
		is_mtbf_mode = 0;
	}

	manager->input_suspend = fuel_gauge_get_input_suspend(manager->fuel_gauge);
	if (!cpu_qos_request && manager->pd_active >= CHARGE_PD_ACTIVE) {
		cpu_qos_request = true;
		cpu_latency_qos_add_request(&cp_qos_request, 50);
		lx_err("cpu_qos_request open\n");
	} else if (cpu_qos_request && manager->pd_active == CHARGE_PD_INVALID){
		cpu_qos_request = false;
		cpu_latency_qos_remove_request(&cp_qos_request);
		lx_err("cpu_qos_request close\n");
	}
	if (!manager->adapter_plug_in)
		return;

	if (!manager->is_pr_swap) {
		switch (manager->vbus_type) {
			case VBUS_TYPE_NONE:
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
				if (!get_usb_ready(manager)) {
					if (manager->get_usb_rdy_cnt == 0)
						schedule_delayed_work(&manager->wait_usb_ready_work, msecs_to_jiffies(0));
					} else
#endif
				charger_force_dpdm(charger, true);
				break;
			case VBUS_TYPE_NON_STAND:
			case VBUS_TYPE_FLOAT:
				if (manager->float_count == 0 && manager->pd_active != 1) {
					lx_info("float type!\n");
					schedule_delayed_work(&manager->second_detect_work, msecs_to_jiffies(FLOAT_DELAY_TIME));
				}
				rerun_election(manager->main_icl_votable);
				break;

			default:
				break;
		}
	} else
		manager->vbus_type = VBUS_TYPE_FLOAT;

	if (manager->vbus_type == VBUS_TYPE_SDP || manager->vbus_type == VBUS_TYPE_CDP)
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
	else
		manager->usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;

#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	if (charger_monitor_fg_i2c_status(manager)) {
		lx_err("%d:FG i2c error\n", __LINE__);
}
#endif

	charger_manager_check_iindpm(manager, manager->vbus_type);

	if (manager->pd_contract_update) {
		manager->pd_contract_update = false;
		if (g_policy->state != POLICY_RUNNING)
			chargerpump_policy_stop(g_policy);
	}

	manager->authenticate = bp->authenticate;

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START) && (!manager->smart_ctrl_en) && manager->authenticate) {
		chargerpump_policy_start(g_policy);
	}
#else
	if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START))
		chargerpump_policy_start(g_policy);
#endif

	charger_manager_check_vindpm(manager, manager->chg_adc[ADC_GET_VBAT]);

#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
		if (manager->decrease_volt) {
			vote(manager->main_icl_votable,LPD_TRIG_VOTER, true, 1500);
			if (manager->vbus_type == VBUS_TYPE_HVDCP) {
				lx_info("LPD detect SET qc2 vbus 5v\n");
				charger_qc2_vbus_mode(manager->charger, 5000);
			} else if (manager->pd_active == CHARGE_PD_ACTIVE) {
				tcpm_dpm_pd_request(manager->tcpc, 5000, 1800, NULL);
				lx_info("LPD detect SET pd vbus 5v\n");
			}
		}
	
		if (manager->chg_status == POWER_SUPPLY_STATUS_DISCHARGING || manager->vbus_type == VBUS_TYPE_NONE) {
			/* reset lpd protect votes */
			vote(manager->main_icl_votable,LPD_TRIG_VOTER, false, 0);
			manager->lpd_status = 0;
			lpd_first_detect = 1;
		} else if(lpd_first_detect) {
			lpd_first_detect = 0;
			manager->lpd_status = 0;
			if(manager->lpd_enable && manager->lpd_control)
				schedule_delayed_work(&manager->rust_detection_work, msecs_to_jiffies(300));
		}
#endif
#endif

	if (g_policy->state == POLICY_RUNNING)
		return;

	if (manager->vbus_type == VBUS_TYPE_DCP && !manager->qc_detected && !manager->pd_active && manager->cp_master_psy) {
		manager->qc_detected = true;
		charger_qc_identify(charger, manager->qc3_mode);
	}

	manager->ch = bp->ch;
	lx_info("ch = %d, authenticate = %d\n", manager->ch, manager->authenticate);
	if ((!manager->authenticate || (manager->authenticate && !manager->ch)) && (manager->bms_auth_done)) {
		if (fuel_gauge_check_i2c_function(manager->fuel_gauge) && !manager->authenticate) {
			lx_err("%d:FG i2c error\n", __LINE__);
			vote(manager->main_icl_votable, BMS_AUTHENTIC_VOTER, true, 500);
		} else {
			vote(manager->main_icl_votable, BMS_AUTHENTIC_VOTER, true, 2000);
		}
		rerun_election(manager->main_icl_votable);
		if (manager->vbus_type == VBUS_TYPE_HVDCP) {
			lx_info("BMS_AUTHENTIC SET qc2 vbus 5v\n");
			charger_qc2_vbus_mode(manager->charger, 5000);
		} else if (manager->pd_active >= CHARGE_PD_ACTIVE) {
			if (fuel_gauge_check_i2c_function(manager->fuel_gauge) && !manager->authenticate) {
				tcpm_dpm_pd_request(manager->tcpc, 5000, 500, NULL);
			} else {
				tcpm_dpm_pd_request(manager->tcpc, 5000, 2000, NULL);
			}
			lx_info("BMS_AUTHENTIC SET pd vbus 5v\n");
		}
		if (manager->chg_status == POWER_SUPPLY_STATUS_DISCHARGING || manager->vbus_type == VBUS_TYPE_NONE) {
			vote(manager->main_icl_votable,BMS_AUTHENTIC_VOTER, false, 0);
			lx_info("BMS_AUTHENTIC_VOTER vote fase\n");
		}
	}

	if (!manager->cp_master_psy) {
		vote(manager->main_icl_votable, CP_I2C_ERR, true, 900);
		lx_err("%d:CP i2c error, set 5v-900ma\n", __LINE__);
	} else
		vote(manager->main_icl_votable, CP_I2C_ERR, false, 0);

	lx_debug("end!\n");
}

static int charger_manager_thread_fn(void *data)
{
	struct charger_manager *manager = data;
	int ret = 0;
	int loop_time = 0;

	while (true) {
		ret = wait_event_interruptible(manager->wait_queue,
							manager->run_thread);
		if (kthread_should_stop() || ret) {
			lx_err("exits(%d)\n", ret);
			break;
		}

		manager->run_thread = false;

		charger_manager_monitor(manager);

		charger_manager_charger_type_detect(manager);

		if (!manager->adapter_plug_in)
			loop_time = CHARGER_MANAGER_LOOP_TIME_OUT;
		else {
			loop_time = CHARGER_MANAGER_LOOP_TIME;
			if (manager->tbat < TEMP_LEVEL_NEGATIVE_7 || manager->tbat > TEMP_LEVEL_42)
				loop_time = CHARGER_MANAGER_LOOP_TIME_TEMP;
		}
		charger_manager_start_timer(manager, loop_time);
	}
	return 0;
}

static int charger_manager_notifer_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct charger_manager *manager = container_of(nb,
							struct charger_manager, charger_nb);
	charger_manager_wake_thread(manager);

	return NOTIFY_OK;
}

static int charger_manager_check_dev(struct charger_manager *manager)
{

	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is err or null\n");
		return -ENOMEM;
	}

	manager->charger = charger_find_dev_by_name("primary_chg");
	if (!manager->charger) {
		lx_err("failed to master_charge device\n");
		return -EPROBE_DEFER;
	}

	manager->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (!manager->master_cp_chg) {
		lx_err("failed to master_cp_chg device\n");
		return -EPROBE_DEFER;
	}

	if (manager->cp_slave_use) {
		manager->slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
		if (!manager->slave_cp_chg)
			lx_err("failed to slave_cp_chg device\n");
	}

	manager->cp_master_psy = power_supply_get_by_name("sc-cp-master");
	if (!manager->cp_master_psy) {
		lx_err("failed to cp_master_psy\n");
		return -EPROBE_DEFER;
	}

	if (manager->cp_slave_use) {
		manager->cp_slave_psy = power_supply_get_by_name("sc-cp-slave");
		if (!manager->cp_slave_psy)
			lx_err("failed to cp_slave_psy\n");
	}

	manager->fg_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(manager->fg_psy)) {
		lx_err("failed to get bms psy\n");
		return -EPROBE_DEFER;
	}

	manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
	if (!manager->fuel_gauge) {
		lx_err("failed to fuel_gauge device\n");
		return -EPROBE_DEFER;
	}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!manager->tcpc) {
		lx_err("get tcpc dev failed\n");
		return -EPROBE_DEFER;
	}
#endif

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	manager->pd_adapter = get_adapter_by_name("pd_adapter");
	if (!manager->pd_adapter) {
		lx_err("failed to pd_adapter\n");
		return -EPROBE_DEFER;
	}
#endif

	return 0;
}

static int charger_manager_parse_dts(struct charger_manager *manager)
{
	struct device_node *node = manager->dev->of_node;
	int ret = false;
	int size, i;
	int batt_temp[TEMP_LEVEL_MAX] = { 250, };
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	int retv = false;
#endif

	ret = of_property_get_array(manager->dev, "lx_chg_manager,battery_temp_level", &size, &manager->battery_temp);
	if (ret < 0) {
		manager->battery_temp = batt_temp;
		lx_err("battery_temp user default, ret = %d\n", ret);
	} else {
		if (size > TEMP_LEVEL_MAX) {
			manager->battery_temp = batt_temp;
			lx_err("battery_temp user default, size = %d\n", size);
		} else {
			for (i = 0; i < TEMP_LEVEL_MAX; i++) {
				lx_info("battery_temp level%d = %d\n", i, manager->battery_temp[i]);
			}
		}
	}

	manager->cp_master_use = of_property_read_bool(node, "chargerpump,master");
	manager->cp_slave_use = of_property_read_bool(node, "chargerpump,slave");
	lx_info("cp master:slave %d:%d\n", manager->cp_master_use, manager->cp_slave_use);

	ret |= of_property_read_u32(node, "lx_chg_manager,QC3_mode", &manager->qc3_mode);
	ret |= of_property_read_u32(node, "lx_chg_manager,usb_charger_current", &manager->usb_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,float_charger_current", &manager->float_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,ac_charger_current", &manager->dcp_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,cdp_charger_current", &manager->cdp_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,hvdcp_charger_current", &manager->hvdcp_charge_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,hvdcp_input_current", &manager->hvdcp_input_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,hvdcp3_charger_current", &manager->hvdcp3_charge_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,hvdcp3_input_current", &manager->hvdcp3_input_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,pd2_charger_current", &manager->pd2_charge_current);
	ret |= of_property_read_u32(node, "lx_chg_manager,pd2_input_current", &manager->pd2_input_current);

#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	ret |= of_property_read_u32(node, "lx_chg_manager,lpd_enable", &manager->lpd_enable);
#endif
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	retv = of_property_read_u32_array(node, "lx_chg_manager,cyclecount", manager->cyclecount, CYCLE_COUNT_MAX);
	if (retv) {
		lx_info("use default CYCLE_COUNT: 0\n");
		for(i = 0; i < CYCLE_COUNT_MAX; i++)
			manager->cyclecount[i] = 0;
	}
	ret |= retv;
	retv = of_property_read_u32_array(node, "lx_chg_manager,dropfv", manager->dropfv, CYCLE_COUNT_MAX);
	if (retv) {
		lx_info("use default DROP_FV: 0\n");
		for(i = 0; i < CYCLE_COUNT_MAX; i++)
			manager->dropfv[i] = 0;
	}
	ret |= retv;
#endif

	if (ret)
		return false;
	else
		return true;
}

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
void set_board_temp(int board_temp)
{
	info->thermal_board_temp = board_temp;
	lx_info("%s: get thermal_board_temp: %d\n", __func__, info->thermal_board_temp);

	return;
}
EXPORT_SYMBOL_GPL(set_board_temp);

void set_thermal_scene(int thermal_scene)
{
	info->thermal_scene = thermal_scene;
	lx_info("%s: get thermal_scene: %d\n", __func__, info->thermal_scene);

	return;
}
EXPORT_SYMBOL_GPL(set_thermal_scene);

#if IS_ENABLED(CONFIG_DISP_MTK)
static int screen_state_for_charger_callback(struct notifier_block *nb,
                                            unsigned long val, void *v)
{
        int blank = *(int *)v;
        struct charger_manager *manager = container_of(nb, struct charger_manager, sm.charger_panel_notifier);

        if (!(val == MTK_DISP_EARLY_EVENT_BLANK|| val == MTK_DISP_EVENT_BLANK)) {
                lx_err("%s event(%lu) do not need process\n", __func__, val);
                return NOTIFY_OK;
        }

        switch (blank) {
        case MTK_DISP_BLANK_UNBLANK: //power on
                manager->sm.screen_state = 0;
                lx_info("%s screen_state = %d\n", __func__, manager->sm.screen_state);
                break;
        case MTK_DISP_BLANK_POWERDOWN: //power off
                manager->sm.screen_state = 1;
                lx_info("%s screen_state = %d\n", __func__, manager->sm.screen_state);
                break;
        }
        return NOTIFY_OK;
}
#else
static struct drm_panel *prim_panel;
#define MAX_CHECK_DISPLAY_READY_COUNT 1000
static int xm_smart_chg_check_panel(void)
{
	struct device_node *charger_screen_node, *panel_node;
	struct drm_panel *panel;
	int retry_num = 0;
	int count;
	int i;

	charger_screen_node = of_find_node_by_name(NULL, "thermal-screen");
	if (!charger_screen_node) {
		lx_err("ERROR: Cannot find charger_screen_node with panel!");
		return -ENODEV;
	}

	count = of_count_phandle_with_args(charger_screen_node, "panel", NULL);
	lx_err("count of panel in node is: %d\n" ,count);
	if (count <= 0){
		return -ENODEV;
	}

	do {
		for (i = 0; i < count; i++) {
			panel_node = of_parse_phandle(charger_screen_node, "panel", i);
			lx_err("retry_num: %d, try to add of node panel: %s\n", retry_num, panel_node);
			panel = of_drm_find_panel(panel_node);
			of_node_put(panel_node);
			if (!IS_ERR(panel)) {
				prim_panel = panel;
				retry_num = 3;
				break;
			} else {
				prim_panel = NULL;
				msleep(500);
			}
		}
		++retry_num;
	} while (retry_num < 3);

	if (PTR_ERR(prim_panel) == -EPROBE_DEFER) {
		lx_err("ERROR: Cannot find prim_panel of panel_node!");
		return -EPROBE_DEFER;
	}

	lx_err("count of panel in panel_node PTR_ERR_prim_panel  is: %d\n", PTR_ERR(prim_panel));
	return 0;
}

static void screen_state_for_xm_smart_chg_callback(enum panel_event_notifier_tag tag,
		struct panel_event_notification *notification, void *client_data)
{
	static int last_screen_state = 0;
	if (!notification) {
		lx_err("Invalid notification\n");
		return;
	}

	if(notification->notif_data.early_trigger) {
		return;
	}
	if(tag == PANEL_EVENT_NOTIFICATION_PRIMARY){
		switch (notification->notif_type) {
		case DRM_PANEL_EVENT_UNBLANK:
			screen_light = screen_light | 0x1;
#ifndef FACTORY_BUILD
			info->swcid_bright = 1;
			//if (info->audio_cctog == 0 && info->cid_status ==0)
				//tcpci_set_cc(info->tcpc, TYPEC_CC_DRP);
#endif
			break;
		case DRM_PANEL_EVENT_BLANK:
		case DRM_PANEL_EVENT_BLANK_LP:
			screen_light = screen_light & 0x2;
#ifndef FACTORY_BUILD
			info->swcid_bright = 0;
			//if (info->audio_cctog == 0 && info->cid_status ==0)
				//tcpci_set_cc(info->tcpc, TYPEC_CC_RD);
#endif
			break;
		case DRM_PANEL_EVENT_FPS_CHANGE:
			return;
		default:
			return;
		}
		if (screen_light)
			info->sm.screen_state = 1;
		else
			info->sm.screen_state = 0;

		if (info->soft_cid_start && info->sm.screen_state != last_screen_state
			&& info->cid_status && !info->typec_attach)
			schedule_delayed_work(&info->soft_cid_work, 0);

		last_screen_state = info->sm.screen_state;
		lx_err("screen_state = %d\n", info->sm.screen_state);
	}
}

static struct drm_panel *prim_panel;
#define MAX_XM_SMART_CHG_CHECK_PANEL_COUNT 10
int xm_smart_chg_register_panel_notifier(void)
{
	int retval = 0;
	void *pvt_data = NULL;
	int i = 0;

	for(i = 0; i < MAX_XM_SMART_CHG_CHECK_PANEL_COUNT; i++){
		retval = xm_smart_chg_check_panel();
		if (retval < 0) {
			lx_err("check panel fail(%d), i = %d\n", retval, i);
			if (retval == -EPROBE_DEFER) {
				return retval;
			}
		}
		if (prim_panel) {
			lx_err("success to check panel\n");
			break;
		}
	}

	if (prim_panel) {
		if (!cookie) {
 			cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
								PANEL_EVENT_NOTIFIER_CLIENT_XM_SMART_CHG, prim_panel,
								screen_state_for_xm_smart_chg_callback, pvt_data);
			if (IS_ERR(cookie)){
				lx_err("Failed to register for prim_panel events\n");
				retval = -EPROBE_DEFER;
				return retval;
			} else {
				lx_err("prim_panel_event_notifier_register register succeed\n");
				retval = 0;
			}
		}
	} else {
		lx_err("Failed to get prim_panel\n");
		retval = -ENODEV;
	}

	return retval;
}

#endif
#endif

//rust detection start
#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
#define LPD_PROTECT_CUR 1500
#define MAX_UEVENT_LPD_LENGTH 100
static void __maybe_unused generate_xm_lpd_uevent(struct charger_manager *manager)
{
	static char uevent_string[MAX_UEVENT_LPD_LENGTH+1] = {
		"POWER_SUPPLY_MOISTURE_DET_STS=\n"  //length=30
	};
	u32 cnt=0, i=0;
	char *envp[5] = { NULL };  //the length of array need adjust when uevent number increase

	sprintf(uevent_string+30,"%d", manager->lpd_status);
	envp[cnt++] = uevent_string;

	envp[cnt]=NULL;
	for(i = 0; i < cnt; ++i)
	      lx_err("%s\n", envp[i]);
	kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);
	lx_err("%s: LPD_KOBJECT_UEVENT END\n", __func__);
	return;
}
static void rust_detection_work_func(struct work_struct *work)
{
	static int i = 0;
	struct charger_manager *manager = container_of(work, struct charger_manager, rust_detection_work.work);

	if (manager->fsa4480_chg_dev == NULL) {
		manager->fsa4480_chg_dev = charger_find_dev_by_name("fsa4480_chg");
		if (manager->fsa4480_chg_dev) {
			lx_err("Found fsa4480 charger\n");
			i = 0;
                } else {
			lx_err("*** Error : can't find fsa4480 charger ***\n");
			if (i < 10) {
				schedule_delayed_work(&manager->rust_detection_work, msecs_to_jiffies(2000));
				i++;
			}
			return;
		}
	}

	charger_dev_rust_detection_init(manager->fsa4480_chg_dev);
	charger_dev_rust_detection_enable(manager->fsa4480_chg_dev, true);
	msleep(15);
	manager->lpd_status = charger_dev_rust_detection_read_res(manager->fsa4480_chg_dev);
	manager->decrease_volt = 0;
	if (manager->lpd_status) {
		generate_xm_lpd_uevent(manager);
		manager->decrease_volt = 1;
		vote(manager->main_icl_votable,LPD_TRIG_VOTER, true, LPD_PROTECT_CUR);
		if (manager->vbus_type == VBUS_TYPE_HVDCP) {
			lx_err("LPD detect SET qc2 vbus 5v\n");
			charger_qc2_vbus_mode(manager->charger, 5000);
		} else if (manager->pd_active == CHARGE_PD_ACTIVE) {
			tcpm_dpm_pd_request(manager->tcpc, 5000, 1800, NULL);
			lx_err("LPD detect SET pd vbus 5v\n");
		}
	} else  {
		lx_err("LPD detect lpd_status = 0\n");
	}
 	lx_err("LPD detect lpd_status= %d decrease_volt = %d\n", manager->lpd_status, manager->lpd_charging);
 	charger_dev_rust_detection_enable(manager->fsa4480_chg_dev, false);
}
#endif
#endif
//rust detection end
static void soft_cid_work_handler(struct work_struct *work)
{
	struct charger_manager *manager = container_of(work,
				struct charger_manager, soft_cid_work.work);

	lx_info("soft_cid_work_handler run\n");
	mutex_lock(&manager->scid_lock);
	if(manager->cid_status && !manager->typec_attach) {
		lx_info("screen_state = %d, audio_usb_cctoggle = %d, last_cc_toggle = %d\n",
			manager->sm.screen_state, manager->audio_usb_cctoggle, manager->last_cc_toggle_en);
		manager->soft_cid_start = true;
		manager->cc_toggle_en = manager->sm.screen_state ? true : false;
		if (manager->audio_usb_cctoggle)
			manager->cc_toggle_en = true;
		if (manager->last_cc_toggle_en != manager->cc_toggle_en)
			charger_soft_cid_set_toggle(manager->charger, manager->cc_toggle_en);

		manager->last_cc_toggle_en = manager->cc_toggle_en;
	}
	mutex_unlock(&manager->scid_lock);
}

#define RETRY_COUNT_MAX 100
static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_manager *manager;
	int ret = 0;
	static int retry_count = 0;

	lx_info("running (%s)\n", CHARGER_MANAGER_VERSION);

	manager = devm_kzalloc(&pdev->dev, sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	manager->dev = &pdev->dev;
	platform_set_drvdata(pdev, manager);

	info = manager;

	ret = charger_manager_parse_dts(manager);
	if (!ret)
		lx_err("charger_manager_parse_dts failed\n");

	ret = charger_manager_check_dev(manager);
	if (ret < 0) {
		retry_count ++;
		lx_err("failed to check dev\n");
		if (retry_count < RETRY_COUNT_MAX) {
			devm_kfree(manager->dev, manager);
			return -EPROBE_DEFER;
		}
	}
#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	INIT_DELAYED_WORK(&manager->rust_detection_work, rust_detection_work_func);
	manager->lpd_charging = 0;
	manager->lpd_control = 1;
	schedule_delayed_work(&manager->rust_detection_work, msecs_to_jiffies(0));
#endif
#endif

	charger_manager_create_votable(manager);

	charger_manager_usb_psy_register(manager);

	charger_manager_batt_psy_register(manager);

	lx_jeita_init(manager->dev);
	manager->jeita_chip = get_jeita_info();
	if (!IS_ERR_OR_NULL(manager->usb_psy)) {
		ret = lx_usb_sysfs_create_group(manager);
		if (ret < 0)
			lx_err("create some usb nodes failed\n");
	}
	ret = charge_manager_thermal_init(manager);
	if (ret < 0)
		lx_err("charge_manager_thermal_init failed, ret = %d\n", ret);

	if (!IS_ERR_OR_NULL(manager->batt_psy)) {
		ret = lx_batt_sysfs_create_group(manager);
		if (ret < 0)
			lx_err("create some batt nodes failed\n");
	}
	lx_charger_node_init(manager);
	init_waitqueue_head(&manager->wait_queue);
	manager->charger_nb.notifier_call = charger_manager_notifer_call;
	charger_register_notifier(&manager->charger_nb);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	INIT_DELAYED_WORK(&manager->xm_charge_work, xm_charge_work);
#if IS_ENABLED(CONFIG_MIEV)
	INIT_DELAYED_WORK(&manager->dfx_stand_chg_slow_check_work, dfx_stand_chg_slow_work);
#endif
	manager->smart_ctrl_en = false;
#endif
	mutex_init(&manager->scid_lock);

	INIT_DELAYED_WORK(&manager->second_detect_work, apsd_second_detect_work);
	INIT_DELAYED_WORK(&manager->pd_config_check_work, pd_config_check_work);
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	INIT_DELAYED_WORK(&manager->wait_usb_ready_work, wait_usb_ready_work);
#endif
	INIT_DELAYED_WORK(&manager->force_set_src_work, force_set_src_work_handler);
	INIT_DELAYED_WORK(&manager->src_switch_drp_work, src_switch_drp_work_handler);
	INIT_DELAYED_WORK(&manager->reverse_quick_charge_work, reverse_quick_charge_work_handler);
	INIT_DELAYED_WORK(&manager->soft_cid_work, soft_cid_work_handler);
#ifndef FACTORY_BUILD
	alarm_init(&info->rust_det_work_timer, ALARM_BOOTTIME,rust_det_work_timer_handler);
	INIT_DELAYED_WORK(&info->hrtime_otg_work, hrtime_otg_work_func);
	INIT_DELAYED_WORK(&info->set_cc_drp_work, set_cc_drp_work_func);
#endif
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	manager->pd_nb.notifier_call = charger_manager_tcpc_notifier_call;
	if (IS_ERR_OR_NULL(manager->tcpc)) {
		lx_err("manager->tcpc is null\n");
	} else {
		ret = register_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
								TCP_NOTIFY_TYPE_ALL);
		if (ret < 0) {
			lx_err("register tcpc notifier fail(%d)\n", ret);
		return ret;
		}
	}
#endif

	device_init_wakeup(manager->dev, true);
	INIT_DELAYED_WORK(&manager->power_off_check_work, power_off_check_work);
	INIT_DELAYED_WORK(&manager->replenish_work, replenish_work_func);
	INIT_DELAYED_WORK(&manager->charge_iterm_monitor_work, charge_iterm_work_func);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	manager->thermal_board_temp = 250;
	manager->thermal_scene = 0;

#if IS_ENABLED(CONFIG_DISP_MTK)
	manager->sm.charger_panel_notifier.notifier_call = screen_state_for_charger_callback;
	ret = mtk_disp_notifier_register("screen state", &manager->sm.charger_panel_notifier);
	if (ret) {
		lx_err("register screen state callback fail(%d)\n", ret);
		return ret;
	}
#else

#ifndef FACTORY_BUILD
	ret = xm_smart_chg_register_panel_notifier();
	if (ret < 0) {
		lx_err("xm_smart_chg_register_panel_notifier failed(%d)\n", ret);
	}else
	lx_info("xm_smart_chg_register_panel_notifier success\n");
#endif

#endif
	manager->smart_batt = 0;
	manager->smart_fv = 0;
	manager->night_charging = false;
	manager->night_charging_flag = false;
	manager->fv_againg = 0;
	manager->low_fast_plugin_flag = false;
	manager->pps_fast_mode = false;
	manager->smart_bypass_fcc = 0;
	manager->smart_bypass_mode = 0;
	manager->b_flag = NORMAL;
#if IS_ENABLED(CONFIG_MIEV)
	manager->dfx_none_standard_flag = 0;
	manager->dfx_batt_linker_absent_flag =0;
	manager->dfx_cp_err_flag = 0;
	manager->dfx_fg_i2c_err_flag = 0;
	manager->dfx_bat_temp_abnormal_flag = 0;
	manager->dfx_lpd_check_flag = 0;
	/*add new*/
	manager->dfx_cp_ibat_ocp_flag = 0;
	manager->dfx_cp_vbat_ovp_flag = 0;
	manager->dfx_cp_ibus_ocp_flag = 0;
	manager->dfx_cp_vbus_ovp_flag = 0;
	manager->dfx_cp_tdie_hot_flag = 0;
	manager->dfx_cp_enable_flag = 0;
	manager->dfx_pd_pps_authen_flag = 0;
	manager->dfx_global_soc_over_80_flag = 0;
	manager->dfx_global_soc_trig_flag = 0;
	manager->dfx_high_temp_discharging_flag = 0;
	manager->dfx_low_temp_discharging_flag = 0;
	manager->dfx_soc_not_full_flag = 0;
	manager->dfx_battery_auth_fail_flag = 0;
	manager->dfx_none_standard_charge_slow_flag = 0;
#endif
#endif
	manager->float_count = 0;
	manager->last_cc_toggle_en = true;
	manager->run_thread = true;
	manager->thread = kthread_run(charger_manager_thread_fn, manager,
								"charger_manager_thread");
	manager->lx_charger_chain_nb.notifier_call = lx_charger_chain_notify;
	lx_charger_notifier_register(&manager->lx_charger_chain_nb);
	INIT_DELAYED_WORK(&manager->tcpc_class_complete_init_work, tcpc_class_complete_init_work_func);
	schedule_delayed_work(&manager->tcpc_class_complete_init_work, msecs_to_jiffies(500));
	lx_info("success\n");
	return 0;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct charger_manager *manager = platform_get_drvdata(pdev);
	psy_unregister_cooler(manager);
	lx_jeita_deinit();
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	cancel_delayed_work(&manager->xm_charge_work);
	cancel_delayed_work(&manager->dfx_stand_chg_slow_check_work);
#endif
	cancel_delayed_work(&manager->power_off_check_work);

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = unregister_tcp_dev_notifier(manager->tcpc, &manager->pd_nb,
					  TCP_NOTIFY_TYPE_ALL);
	if (ret < 0)
		lx_err("unregister tcpc notifier fail(%d)\n", ret);
#endif
	charger_unregister_notifier(&manager->charger_nb);
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#if IS_ENABLED(CONFIG_DISP_MTK)
	ret = mtk_disp_notifier_unregister(&manager->sm.charger_panel_notifier);
	if (ret < 0)
		lx_err("unregister screen state notifier fail(%d)\n", ret);
#else
	if (prim_panel && !IS_ERR(cookie)) {
		panel_event_notifier_unregister(cookie);
	} else {
		lx_err("prim_panel_event_notifier_unregister falt\n");
	}
#endif
#endif
	return 0;
}

static const struct of_device_id charger_manager_match[] = {
	{.compatible = "lx,lx_chg_manager",},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static struct platform_driver charger_manager_driver = {
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.driver = {
		.name = "lx_chg_manager",
		.of_match_table = charger_manager_match,
	},
};

static int __init charger_manager_init(void)
{
	lx_err("---->\n");
	return platform_driver_register(&charger_manager_driver);
}

late_initcall(charger_manager_init);

MODULE_DESCRIPTION("LX Charger Manager Core");
MODULE_LICENSE("GPL v2");
