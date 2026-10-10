// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Xiaomi Inc.
 * Author Tianye<tianye9@xiaomi.com>
 */
#include <linux/delay.h>
#include "xm_smart_chg.h"
#include "../lx_printk.h"
#include "../charger_class/lx_cp_class.h"
#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG][SMART]"
#endif
#define XM_SMART_CHG_DEBUG 0
#define SMART_BYPASS_ENTRY_SOC 40
#define SMART_BYPASS_EXIT_SOC 90

void set_error(struct charger_manager *manager)
{
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 1;
	lx_err("xm %s en_ret=%d\n", __func__, manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

void set_success(struct charger_manager *manager)
{
	manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 0;
	lx_err("xm %s en_ret=%d\n", __func__, manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

int smart_chg_is_error(struct charger_manager *manager)
{
	return manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret? true : false;
}

void handle_smart_chg_functype(struct charger_manager *manager,
	const int func_type, const int en_ret, const int func_val)
{
	switch (func_type)
	{
	case SMART_CHG_FEATURE_MIN_NUM ... SMART_CHG_FEATURE_MAX_NUM:
		manager->smart_charge[func_type].en_ret = en_ret;
		manager->smart_charge[func_type].active_status = false;
		manager->smart_charge[func_type].func_val = func_val;
		set_success(manager);
		lx_err("xm set func_type:%d, en_ret = %d\n", func_type, en_ret);
		break;
	default:
		lx_err("xm ERROR: Not supported func type: %d\n", func_type);
		set_error(manager);
		break;
	}
}

int handle_smart_chg_functype_status(struct charger_manager *manager)
{
	int i;
	int all_func_status = 0;
	all_func_status |= !!manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret;	//handle bit0

	lx_err("all_func_status =%#X, en_ret=%d\n",all_func_status, manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);

	/* save functype[i] enable status in all_func_status bit[i] */
	for(i = SMART_CHG_FEATURE_MIN_NUM; i <= SMART_CHG_FEATURE_MAX_NUM; i++){  //handle bit1 ~ bit SMART_CHG_FEATURE_MAX_NUM
		if(manager->smart_charge[i].en_ret)
			all_func_status |= BIT_MASK(i);
		else
			all_func_status &= ~BIT_MASK(i);

		lx_err("type:%d, en_ret=%d, active_status=%d,func_val=%d, all_func_status=%#X\n",
			i, manager->smart_charge[i].en_ret, manager->smart_charge[i].active_status, manager->smart_charge[i].func_val,all_func_status);
	}

	lx_err("all_func_status:%#X\n", all_func_status);
	return all_func_status;
}

#define BYPASS_CURR_DEFAULT 8000
static void xm_smart_chg_handle_bypass_chg(struct charger_manager *manager)
{
    struct lx_jeita_info *jeita_chip = manager->jeita_chip;
    int bypass_mode = manager->smart_charge[SMART_CHG_BYPASS].en_ret;
    union power_supply_propval pval = {0,};
    int curr_lmt = 0;
    int ret = 0;
    int jeita_index;

    if (!bypass_mode)
        goto _bypass_chg_quit;
    
    ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
    if (ret < 0) {
        lx_err("%s get battery soc error.\n", __func__);
        goto _bypass_chg_quit;
    } else {
        manager->soc = pval.intval;
    }

    if (manager->soc < SMART_BYPASS_ENTRY_SOC || manager-> soc > SMART_BYPASS_EXIT_SOC) {
        lx_err("soc not in range, quit bypass mode\n");
        bypass_mode = 0;
        goto _bypass_chg_quit;
    }

    switch(manager->thermal_scene) {
        case SMART_CHG_SCENE_TGAME:
        case SMART_CHG_SCENE_MGAME:
        case SMART_CHG_SCENE_HPGAME:
        case SMART_CHG_SCENE_CGAME:
        case SMART_CHG_SCENE_CGAME_2:
            jeita_index = lx_step_jeita_get_index(jeita_chip->jeita_bypass_high_lmt_cfg, manager->thermal_board_temp);
            curr_lmt = jeita_chip->jeita_bypass_high_lmt_cfg[jeita_index].value;
            manager->smart_charge[SMART_CHG_BYPASS].active_status = true;
            break;
        default:
            curr_lmt = BYPASS_CURR_DEFAULT;
            manager->smart_charge[SMART_CHG_BYPASS].active_status = true;
            break;
    }

    if (curr_lmt != manager->smart_bypass_fcc) {
        vote(manager->total_fcc_votable, SMART_BYPASS_VOTE, true, curr_lmt);
    }

_bypass_chg_quit:
        lx_debug("bypass_mode[%d %d] soc[%d %d %d] fcc[%d %d %d %d] thermal_scene[%d] thermal_board_temp[%d].\n",
                        manager->smart_bypass_mode, bypass_mode, manager->soc, SMART_BYPASS_ENTRY_SOC, SMART_BYPASS_EXIT_SOC,
                        curr_lmt, manager->smart_bypass_fcc, get_client_vote(manager->total_fcc_votable, SMART_BYPASS_VOTE), get_effective_result(manager->total_fcc_votable),
                        manager->thermal_scene, manager->thermal_board_temp);

        if (bypass_mode != manager->smart_bypass_mode) {
                manager->smart_bypass_mode = bypass_mode;
                if (!bypass_mode)
                        vote(manager->total_fcc_votable, SMART_BYPASS_VOTE, false, 0);
        }
        if (curr_lmt != manager->smart_bypass_fcc)
                manager->smart_bypass_fcc = curr_lmt;
}

static void xm_smart_chg_endurance_protect(struct charger_manager *manager)
{
    static int g_smart_soc = 80;
    union power_supply_propval pval = {0,};
    int ret = 0;

    ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
    if (ret < 0) {
        lx_err("%s get battery soc error.\n", __func__);
        return;
    } else {
        manager->soc = pval.intval;
    }

    lx_debug("enter, en_ret = %d, fun_val = %d, active_status = %d, smart_soc = %d, ui_soc =%d, smart_ctrl_en = %d\n",
            manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret,
            manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val,
            manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status,
            g_smart_soc,
            manager->soc,
            manager->smart_ctrl_en);

    if(manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret &&
                manager->soc >= manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val &&
                manager->endurance_protect_flag == false) {
        manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = true;
        manager->endurance_protect_flag = true;
        g_smart_soc = manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val;
        lx_info("soc>=80 enter xm_smart_chg_endurance_protect\n");
        vote(manager->total_fcc_votable, ENDURANCE_PRO_VOTER, true, 0);
        // if (g_policy->state == POLICY_RUNNING){
        //         chargerpump_policy_stop(g_policy);
        //         lx_err("monitor disable cp\n");
        // }
        lx_info("disable charger,soc >= %d\n", manager->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val);
    } else if (manager->endurance_protect_flag && (!manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret ||
                manager->soc <= 75)) {
        manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
        manager->endurance_protect_flag = false;
        lx_info("soc<=75 exit xm_smart_chg_endurance_protect\n");
        vote(manager->total_fcc_votable, ENDURANCE_PRO_VOTER, false, 0);
        // if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START)){
        //         chargerpump_policy_start(g_policy);
        //         lx_err("monitor enable cp\n");
        // }
        lx_info("enable charger, soc = %d\n", manager->soc);
    }

    return;
}

void monitor_smart_chg(struct charger_manager *manager)
{
	union power_supply_propval pval = {0,};
	int ret = 0;
	static int g_smart_soc = 50;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0)
		lx_err("get battery soc error.\n");
	else
		manager->soc = pval.intval;

	if (g_policy == NULL)
		return;

	lx_debug("monitor_smart_chg, en_ret = %d, fun_val = %d, active_status = %d, smart_soc = %d, ui_soc =%d, smart_ctrl_en = %d, state = %d\n",
		manager->smart_charge[SMART_CHG_NAVIGATION].en_ret,
		manager->smart_charge[SMART_CHG_NAVIGATION].func_val,
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status,
		g_smart_soc,
		manager->soc,
		manager->smart_ctrl_en,
		g_policy->state);

	if(manager->smart_charge[SMART_CHG_NAVIGATION].en_ret && manager->soc >= manager->smart_charge[SMART_CHG_NAVIGATION].func_val && !manager->smart_charge[SMART_CHG_NAVIGATION].active_status)
	{
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status = true;
		g_smart_soc = manager->smart_charge[SMART_CHG_NAVIGATION].func_val;
		//charger_set_chg(manager->charger, false);
                vote(manager->total_fcc_votable, "CHG_NAVIGATION_VOTER", true, 0);
		if (g_policy->state == POLICY_RUNNING){
			chargerpump_policy_stop(g_policy);
			lx_err("monitor disable cp\n");
		}
		manager->smart_ctrl_en = true;
		lx_err("monitor disable charger,soc >= %d\n", manager->smart_charge[SMART_CHG_NAVIGATION].func_val);
	}
	else if((((!manager->smart_charge[SMART_CHG_NAVIGATION].en_ret || manager->soc <= manager->smart_charge[SMART_CHG_NAVIGATION].func_val - 5) && manager->smart_charge[SMART_CHG_NAVIGATION].active_status) ||
		(!manager->smart_charge[SMART_CHG_NAVIGATION].en_ret && !manager->smart_charge[SMART_CHG_NAVIGATION].active_status)) && manager->smart_ctrl_en)
	{
		manager->smart_charge[SMART_CHG_NAVIGATION].active_status = false;
		//charger_set_chg(manager->charger, true);
                vote(manager->total_fcc_votable, "CHG_NAVIGATION_VOTER", false, 0);
		if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START)){
			chargerpump_policy_start(g_policy);
			lx_err("monitor enable cp\n");
		}
		manager->smart_ctrl_en = false;
		lx_err("monitor enable charger, soc <= %d\n", manager->smart_charge[SMART_CHG_NAVIGATION].func_val - 5);
	}

        if(manager->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
                manager->smart_charge[SMART_CHG_LOW_FAST].active_status = true;
                lx_err("set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, enable low_fast\n", manager->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
	else if(!manager->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
		manager->smart_charge[SMART_CHG_LOW_FAST].active_status = false;
                lx_debug( "set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, disable low_fast\n", manager->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
}

void get_fv_againg(struct charger_manager *manager, int cyclecount, int *fv_aging)
{
        int i = 0;

        while (cyclecount > manager->cyclecount[i])
                i++;
        *fv_aging = manager->dropfv[i];

        lx_debug("%s i = %d, fv_aging = %d\n", __func__, i, *fv_aging);
        return;
}

void get_drop_floatvolatge(struct charger_manager *manager)
{
        int ret = 0;
        union power_supply_propval pval;
        int cycle_count = 0;
        static last_fv_againg = 0;
        struct votable	*fv_votable = NULL;

        ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
        if (ret < 0) {
                lx_err("%s failed to get cycle_count prop", __func__);
                return;
        }
        cycle_count = pval.intval;

        get_fv_againg(manager, cycle_count, &manager->fv_againg);
        if(manager->fv_againg != last_fv_againg){
                fv_votable = find_votable("MAIN_FV");
                if (!fv_votable) {
                        lx_err("%s failed to get fv_votable\n", __func__);
                }else{
                        rerun_election(fv_votable);
                }
        }
        last_fv_againg = manager->fv_againg;
}

void monitor_night_charging(struct charger_manager *manager)
{
        if ((manager == NULL) || !manager->cp_disable_votable)
		return;

        lx_debug("%s night_charging = %d, soc = %d\n", __func__, manager->night_charging, manager->soc);
        if (manager->night_charging && (manager->soc >= 80)) {
                manager->night_charging_flag = true;
                lx_err("%s disable charging\n", __func__);
                //charger_set_chg(manager->charger, false);
                vote(manager->total_fcc_votable, "NIGHT_CHARGING_VOTER", true, 0);
		if (g_policy->state == POLICY_RUNNING){
			chargerpump_policy_stop(g_policy);
			lx_err("%s monitor disable cp\n", __func__);
		}
	} else if(manager->night_charging_flag && (!manager->night_charging || manager->soc <=75)) {
                manager->night_charging_flag = false;
                lx_err("%s enable charging\n", __func__);
		//charger_set_chg(manager->charger, true);
                vote(manager->total_fcc_votable, "NIGHT_CHARGING_VOTER", false, 0);
		if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (g_policy->state == POLICY_NO_START)){
			chargerpump_policy_start(g_policy);
			lx_err("%s monitor enable cp\n", __func__);
		}
	}
}

void monitor_low_fast_strategy(struct charger_manager *manager)
{
        bool fast_flag = false;
        time64_t time_now = 0, delta_time = 0;
        static time64_t time_last = 0;
        static int last_level = 0;
        static bool hot_flag = false;

        if (manager == NULL)
		return;
        if (manager->system_temp_level < 0)
		goto err;

        lx_debug("%s soc = %d, thermal_level = %d, thermal_board_temp = %d, pd_active = %d, low_fast_plugin_flag = %d, low_fast_enable = %d, screen_state = %d, b_flag = %d\n", 
                __func__, manager->soc, manager->system_temp_level, manager->thermal_board_temp, manager->pd_active, manager->low_fast_plugin_flag, manager->smart_charge[SMART_CHG_LOW_FAST].active_status, 
                manager->sm.screen_state, manager->b_flag);

        if ((manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (manager->soc <= 40) && (manager->low_fast_plugin_flag) && manager->smart_charge[SMART_CHG_LOW_FAST].active_status) {
		if (manager->thermal_parse_flags & PD_THERM_PARSE_ERROR) {
			lx_err("%s: pd thermal dtsi parse error\n", __func__);
			goto err;
		}
		if (manager->system_temp_level > manager->pd_thermal_levels) {
			lx_err("%s: system_temp_level is invalid\n", __func__);
			goto err;
		}

                /*manager->sm.screen_state 0:bright, 1:black*/
                if(((manager->b_flag == NORMAL) || (manager->b_flag == BLACK)) && !manager->sm.screen_state) {  //black to bright
                        manager->b_flag = BLACK_TO_BRIGHT;
                        time_last = ktime_get_seconds();
                        fast_flag = true;
                        lx_err("%s switch to bright time_last = %d\n", __func__, time_last);
                }
                else if((manager->b_flag == BLACK_TO_BRIGHT || manager->b_flag == BRIGHT) && !manager->sm.screen_state) {  //still bright
                        manager->b_flag = BRIGHT;
                        time_now = ktime_get_seconds();
                        delta_time = time_now - time_last;
                        lx_err("%s still_bright time_now = %d, time_last = %d, delta_time = %d\n", __func__, time_now, time_last, delta_time);
                        if(delta_time <= 10) {
                                fast_flag = true;
                                lx_err("%s still_bright delta_time = %d, stay fast\n", __func__, delta_time);
                        }
                        else {
                                fast_flag = false;
                                lx_err("%s still_bright delta_time = %d, exit fast\n", __func__, delta_time);
                        }
                }
                else { //black
                        manager->b_flag = BLACK;
                        fast_flag = true;
                        lx_err("%s black stay fast\n", __func__, delta_time);
                }

                /*avoid thermal_board_temp raise too fast*/
                if((last_level == 8) && (manager->system_temp_level == 7) && (manager->thermal_board_temp > 410)){
                        hot_flag = true;
                        fast_flag = false;
                        lx_err("%s avoid thermal_board_temp raise too fast, exit fast mode\n", __func__);
                }
                else if((last_level == 7) && ((manager->system_temp_level == 7) || (manager->system_temp_level == 8)) && hot_flag && (manager->thermal_board_temp > 410)){
                        fast_flag = false;
                }
                else{
                        hot_flag = false;
                }

                if(manager->thermal_board_temp > 420){
                        fast_flag = false;
                }

                if(fast_flag) {  //stay fast strategy
                        manager->pps_fast_mode = true;
                        manager->low_fast_ffc = manager->pd_thermal_mitigation_fast[manager->system_temp_level];

                        if((manager->soc > 38) && (manager->thermal_board_temp > 380)){
                                if(manager->low_fast_ffc >= 5450){
                                        manager->low_fast_ffc -= 3300;
                                }
                                else{
                                        manager->low_fast_ffc = 2150;
                                }
                                lx_err("%s stay fast but decrease 3.3, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc > 35) && (manager->thermal_board_temp > 390)){
                                if(manager->low_fast_ffc >= 3950){
                                        manager->low_fast_ffc -= 1800;
                                }
                                else{
                                        manager->low_fast_ffc = 2150;
                                }
                                lx_err("%s stay fast but decrease 1.8, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc > 30) && (manager->thermal_board_temp > 409)){
                                if(manager->low_fast_ffc >= 3150){
                                        manager->low_fast_ffc -= 2000;
                                }
                                else{
                                        manager->low_fast_ffc = 2150;
                                }
                                lx_err("%s stay fast but decrease 1, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc < 30) && (manager->thermal_board_temp > 409)){
                                if(manager->low_fast_ffc >= 3150){
                                        manager->low_fast_ffc -= 1000;
                                }
                                else{
                                        manager->low_fast_ffc = 2150;
                                }
                                lx_err("%s stay fast but decrease 1, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc > 30) && (manager->soc < 39) && (manager->thermal_board_temp < 389)){
                                if(manager->low_fast_ffc <= 10200){
                                        manager->low_fast_ffc += 2000;
                                }
                                else{
                                        manager->low_fast_ffc = 12200;
                                }
                                lx_err("%s stay fast but add 2, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        else if((manager->soc <= 30) && (manager->thermal_board_temp < 366)){
                                if(manager->low_fast_ffc <= 10200){
                                        manager->low_fast_ffc += 2000;
                                }
                                else{
                                        manager->low_fast_ffc = 12200;
                                }
                                lx_err("%s stay fast but add 3, manager->thermal_board_temp = %d, manager->low_fast_ffc = %d\n", __func__, manager->thermal_board_temp, manager->low_fast_ffc);
                        }
                        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
                        lx_err("%s stay fast, manager->low_fast_ffc = %d\n", __func__, manager->low_fast_ffc);
                }
                else { //exit fast strategy
                        manager->pps_fast_mode = false;
                        manager->low_fast_ffc = manager->pd_thermal_mitigation[manager->system_temp_level];
                        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->low_fast_ffc);
                        lx_err("%s exit fast, manager->low_fast_ffc = %d\n", __func__, manager->low_fast_ffc);
                }
                last_level = manager->system_temp_level;
	}

	return;
err:
        vote(manager->total_fcc_votable, CALL_THERMAL_DAEMON_VOTER, true, manager->pd_thermal_mitigation[manager->system_temp_level]);
	return;
}

#define OUTDOOR_CURR 1900
#define NON_OUTDOOR_CURR 1600
void monitor_outdoor_charging(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)){
        lx_err("manager is NULL\n");
        return;
    }

    if (IS_ERR_OR_NULL(manager->charger)) {
        lx_err("%s:manager->charger is_err_or_null\n", __func__);
        return;
    }

    lx_debug("smart_chg en:%d, outdoor_flag:%d\n", manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret, manager->outdoor_flag);
    if (manager->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret ) {
        lx_err("outdoor charging\n");
        if (manager->outdoor_flag != 1) {
            manager->outdoor_flag = 1;
        }
    } else {
        lx_debug("not outdoor charging");
        manager->outdoor_flag = 0;
    }
}

static void xm_bat_protect_work(struct charger_manager *manager)
{
    static int fv_overvoltage_count = 0;
	int fcc_vote = 0, fv = 0;

    if(IS_ERR_OR_NULL(manager)){
        lx_err("%s manager is err or null\n", __func__);
        return;
    }
	fcc_vote = get_effective_result(manager->total_fcc_votable);
	charger_get_term_volt(manager->charger, &fv);
	lx_debug("%d:%d:%d:%d:%d:%d\n", manager->fv_overvoltage_flag, fv_overvoltage_count, manager->vbat, fv, manager->pd_active, fcc_vote);

	if((manager->vbat > fv) && (manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (!manager->fv_overvoltage_flag)) {
		fv_overvoltage_count++;
		if(fv_overvoltage_count > 1000)
			fv_overvoltage_count = 3;
	}
	if(!manager->fv_overvoltage_flag && (fv_overvoltage_count >= 3) && (manager->pd_active == CHARGE_PD_PPS_ACTIVE) && (fcc_vote >= 4000)) {
		lx_err("enable, avg_ibat = %d(%d)\n", manager->avg_ibat, manager->ibat);
		manager->fv_overvoltage_flag = true;
		fv_overvoltage_count = 0;
        fcc_vote = manager->avg_ibat - 1000;
        if (fcc_vote < 3000)
            fcc_vote = 3000;
		vote(manager->total_fcc_votable, FV_OVERVOLTAGE_VOTER,true, fcc_vote);
	}
	if(manager->fv_overvoltage_flag && (manager->pd_active != CHARGE_PD_PPS_ACTIVE)) {
		lx_err("disable\n");
		fv_overvoltage_count = 0;
		manager->fv_overvoltage_flag = false;
		vote(manager->total_fcc_votable, FV_OVERVOLTAGE_VOTER, false, 0);
	}
}

#if IS_ENABLED(CONFIG_MIEV)
static void mievent_upload(int miev_code, int para_cnt, ...)
{
    int i = 0;
    char* key;
    char* type;
    char* value_str;
    int val_long;
    va_list arg;

    struct misight_mievent *event = cdev_tevent_alloc(miev_code);
    lx_info("miev_code=%d, para_cnt=%d \n", miev_code, para_cnt);
    va_start(arg, para_cnt);
    while (i < para_cnt) {
        type = va_arg(arg, char*);
        key = va_arg(arg, char*);
        if (strcmp(type, "char") == 0) {
            value_str = va_arg(arg, char*);
            lx_info("type:%s,%s:%s\n",type,key,value_str);
            cdev_tevent_add_str(event, key, value_str);
        } else if (strcmp(type, "int") == 0) {
            val_long = va_arg(arg, int);
            lx_info("type:%s,%s:%d\n",type,key,val_long);
            cdev_tevent_add_int(event, key, val_long);
        } else {
            lx_err("CHG:DFX:unsupported type");
            break;
        }
        i++;
    }
    va_end(arg);
    cdev_tevent_write(event);
    cdev_tevent_destroy(event);

    return;
}

static void xm_handle_dfx_report(u8 type, int data)
{
    if (type >= CHG_DFX_MAX_INDEX) {
        lx_err("CHG:DFX: unknown type to report\n");
        return;
    }
    switch (type) {
    case CHG_DFX_FG_IIC_ERR:
        mievent_upload(DFX_ID_CHG_FG_IIC_ERR, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_CP_ERR:
        mievent_upload(DFX_ID_CHG_CP_ERR, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_BATT_LINKER_ABSENT:
        mievent_upload(DFX_ID_CHG_BATT_LINKER_ABSENT, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_LPD_DISCHARGE:
        mievent_upload(DFX_ID_CHG_LPD_DISCHARGE,  1, "int", "lpdFlag", 1);
        break;
    case CHG_DFX_LPD_DISCHARGE_RESET:
        mievent_upload(DFX_ID_CHG_LPD_DISCHARGE, 1, "int","lpdFlag", 0);
        break;
    case CHG_DFX_CORROSION_DISCHARGE:
        mievent_upload(DFX_ID_CHG_CORROSION_DISCHARGE, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_NONE_STANDARD_CHG:
        mievent_upload(DFX_ID_CHG_NONE_STANDARD_CHG, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_CP_VBUS_OVP:
        mievent_upload(DFX_ID_CHG_CP_VBUS_OVP, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_CP_IBUS_OCP:
        mievent_upload(DFX_ID_CHG_CP_IBUS_OCP, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_CP_VBAT_OVP:
        mievent_upload(DFX_ID_CHG_CP_VBAT_OVP, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "vbat", data);
        break;
    case CHG_DFX_CP_IBAT_OCP:
        mievent_upload(DFX_ID_CHG_CP_IBAT_OCP, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_PPS_AUTH_FAIL:
        mievent_upload(DFX_ID_CHG_PD_AUTHEN_FAIL, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "adapterId", data);
        break;
    case CHG_DFX_CP_ENABLE_FAIL:
        mievent_upload(DFX_ID_CHG_CP_ENABLE_FAIL, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_BATTERY_CYCLECOUNT:
        mievent_upload(DFX_ID_CHG_BATTERY_CYCLECOUNT, 2, "char", "chgStatInfo", xm_dfx_chg_report_text[type], "int", "cycleCnt", data);
		break;
    case CHG_DFX_LOW_TEMP_DISCHARGING:
        mievent_upload(DFX_ID_CHG_LOW_TEMP_DISCHARGING, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "tbat", data);
        break;
    case CHG_DFX_HIGH_TEMP_DISCHARGING:
        mievent_upload(DFX_ID_CHG_HIGH_TEMP_DISCHARGING, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "tbat", data);
		break;
    case CHG_DFX_BATTERY_AUTH_FAIL:
        mievent_upload(DFX_ID_CHG_BATTERY_AUTH_FAIL, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type], "int", "status", data);
        break;
    case CHG_DFX_BATTERY_TEMP_HIGH:
        mievent_upload(DFX_ID_CHG_BATTERY_TEMP_HIGH, 2, "char", "chgStatInfo", xm_dfx_chg_report_text[type], "int", "tbat", data);
        break;
    case CHG_DFX_BATTERY_TEMP_LOW:
        mievent_upload(DFX_ID_CHG_BATTERY_TEMP_LOW, 2, "char", "chgStatInfo", xm_dfx_chg_report_text[type], "int", "tbat", data);
        lx_err("CHG:DFX: CHG_DFX_BATTERY_TEMP_LOW data:%d\n", data);
        break;
    case CHG_DFX_ANTIBURN_FAIL:
        mievent_upload(DFX_ID_CHG_ANTIBURN_FAIL, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
        /*add new*/
    case CHG_DFX_GLOBAL_SOC_OVER_80:
        mievent_upload(DFX_ID_CHG_GLOBAL_SOC_OVER_80_ID, 1, "char", "chgErrInfo", xm_dfx_chg_report_text[type]);
        break;
    case CHG_DFX_GLOBAL_SOC_TRIG:
        mievent_upload(DFX_ID_CHG_GLOBAL_SOC_TRIG_ID, 2, "char", "chgStatInfo", xm_dfx_chg_report_text[type],"int", "soc", data);
        break;
    case CHG_DFX_SOC_NOT_FULL:
        mievent_upload(DFX_ID_CHG_SOC_NOT_FULL, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type],"int", "soc", data);
        break;
    case CHG_DFX_CP_TDIE_HOT:
        mievent_upload(DFX_ID_CHG_CP_TDIE_HOT_ID, 3, "char", "chgStatInfo", xm_dfx_chg_report_text[type], "int", "cptdie", data, "int", "cp", data);
        break;
    case CHG_DFX_NONE_CHG_SLOW:
        mievent_upload(DFX_ID_CHG_NONE_STANDARD_CHG_LOW, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type],"int", "soc", data);
        break; 
    case CHG_DFX_STAND_CHG_SLOW:
        mievent_upload(DFX_ID_STAND_CHG_SLOW, 2, "char", "chgErrInfo", xm_dfx_chg_report_text[type],"int", "diff_fcc", data);
        break;   
    default:
        lx_err("CHG:DFX: unknown type to report\n");
    }
    return;
}

static int dfx_none_standard_check(struct charger_manager *manager)
{
    int ret = 0;
    enum vbus_type vbus_type = VBUS_TYPE_NONE;

    if (IS_ERR_OR_NULL(manager)){
        lx_err("CHG:DFX: manager is NULL\n");
        return PTR_ERR(manager);
    }

    if (IS_ERR_OR_NULL(manager->charger)) {
        lx_err("CHG:DFX: manager->charger is_err_or_null\n");
        return -EINVAL;
    }

    ret = charger_get_vbus_type(manager->charger, &vbus_type);
    if (ret < 0){
        lx_err("CHG:DFX: Couldn't get usb type ret=%d\n", ret);
        return -EINVAL;
    }

    if (vbus_type == VBUS_TYPE_FLOAT && (manager->dfx_none_standard_flag < 1)) {
        manager->dfx_none_standard_flag = (manager->dfx_none_standard_flag) + 1;
        xm_handle_dfx_report(CHG_DFX_NONE_STANDARD_CHG, 0);
        lx_err("CHG:DFX: type: %d, none standard send dfx report\n", vbus_type);
    }
    lx_debug("CHG:DFX: type: %d, dfx_none_standard_flag: %d\n", vbus_type, manager->dfx_none_standard_flag);
    return ret;
}

static int dfx_cp_present_check(struct charger_manager *manager)
{
    struct power_supply *cp_psy = NULL;

    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return -EINVAL;
    }

    cp_psy = power_supply_get_by_name("sc-cp-master");
    if (IS_ERR_OR_NULL(cp_psy)) {
        cp_psy = power_supply_get_by_name("sc-cp-slave");
        if (IS_ERR_OR_NULL(cp_psy) && (manager->dfx_cp_err_flag < 1)) {
            manager->dfx_cp_err_flag = (manager->dfx_cp_err_flag) + 1;
            lx_err("CHG:DFX: charger pump not present, send dfx report\n");
            xm_handle_dfx_report(CHG_DFX_CP_ERR, 0);
        }
    }

    lx_debug("CHG:DFX:dfx_cp_err_flag:%d\n", manager->dfx_cp_err_flag);
    return 0;
}

static int dfx_batt_linker_absent_check(struct charger_manager *manager)
{
    int ret = -EINVAL;
    bool chip_ok_status = true;

    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return ret;
    }

    if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
        lx_err("CHG:DFX: point fuel_gauge is null\n");
        return ret;
    }

    chip_ok_status = fuel_gauge_get_chip_ok(manager->fuel_gauge);
    if (!chip_ok_status && manager->dfx_batt_linker_absent_flag < 1) {
        manager->dfx_batt_linker_absent_flag = (manager->dfx_batt_linker_absent_flag) + 1;
        xm_handle_dfx_report(CHG_DFX_BATT_LINKER_ABSENT, 0);
        lx_err("CHG:DFX: batt_linker_absent not present, send dfx report\n");
    }

    lx_debug("CHG:DFX:dfx_batt_linker_absent_flag:%d\n", manager->dfx_batt_linker_absent_flag);
    return 0;
}

static int dfx_lpd_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }
#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
    if (manager->decrease_volt && manager->dfx_lpd_check_flag < 1) {
        manager->dfx_lpd_check_flag = manager->dfx_lpd_check_flag + 1;
        xm_handle_dfx_report(CHG_DFX_LPD_DISCHARGE, 0);
        lx_err("CHG:DFX: dfx_lpd_check, send dfx report\n");
    }
#endif
#endif
    lx_debug("CHG:DFX:dfx_lpd_check_flag:%d\n", manager->dfx_lpd_check_flag);
    return 0;
}

__maybe_unused
static int dfx_fg_i2c_communication_check(struct charger_manager *manager)
{
    int i2c_status = 0;

    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
        manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
        if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
            lx_err("CHG:DFX:point fuel_gauge is err or null\n");
            return PTR_ERR(manager->fuel_gauge);
        }
    }

    i2c_status = fuel_gauge_check_i2c_function(manager->fuel_gauge);
    if (!i2c_status && manager->dfx_fg_i2c_err_flag != 0) {
        manager->dfx_fg_i2c_err_flag = 0;
    } else if (i2c_status && manager->dfx_fg_i2c_err_flag < 1) {
        manager->dfx_fg_i2c_err_flag = manager->dfx_fg_i2c_err_flag + 1;
        xm_handle_dfx_report(CHG_DFX_FG_IIC_ERR, 0);
        lx_err("CHG:DFX: I2C communication failure, send dfx report\n");
    }

    lx_debug("CHG:DFX:fg dfx_fg_i2c_err_flag:%d\n",  manager->dfx_fg_i2c_err_flag);
    return 0;
}

__maybe_unused
static int dfx_batt_temp_abnormal_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if (manager->tbat > 550 && manager->dfx_bat_temp_abnormal_flag < 1) {
        manager->dfx_bat_temp_abnormal_flag = manager->dfx_bat_temp_abnormal_flag + 1;
        xm_handle_dfx_report(CHG_DFX_BATTERY_TEMP_HIGH, manager->tbat);
        lx_err("CHG:DFX: batt_temp_abnormal, send dfx report temp:%d\n", manager->tbat);
    } else if ((manager->tbat < 0 || manager->tbat < -200) && manager->dfx_bat_temp_abnormal_flag < 1) {
        manager->dfx_bat_temp_abnormal_flag = manager->dfx_bat_temp_abnormal_flag + 1;
        xm_handle_dfx_report(CHG_DFX_BATTERY_TEMP_LOW, manager->tbat);
        lx_err("CHG:DFX: batt_temp_abnormal, send dfx report temp:%d\n", manager->tbat);
    }

    lx_debug("CHG:DFX:dfx_bat_temp_abnormal_flag:%d\n", manager->dfx_bat_temp_abnormal_flag);
    return 0;
}

#define VBAT_OVP 4770  
static int dfx_cp_vbat_ovp_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }
    
    if((manager->vbat) > VBAT_OVP && manager->dfx_cp_vbat_ovp_flag < 1){
        manager->dfx_cp_vbat_ovp_flag = manager->dfx_cp_vbat_ovp_flag+1;
        xm_handle_dfx_report(CHG_DFX_CP_VBAT_OVP,manager->vbat);
        lx_err("CHG:DFX: cp_vbat_ovp_abnormal, send dfx report vbat:%d\n",manager->vbat);
    }
    return 0;
}
#define IBAT_OCP 8100
static int dfx_cp_ibat_ocp_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }
    
    if((manager->ibat/1000) > IBAT_OCP && manager->dfx_cp_ibat_ocp_flag < 1){
        manager->dfx_cp_ibat_ocp_flag = manager->dfx_cp_ibat_ocp_flag+1;
        xm_handle_dfx_report(CHG_DFX_CP_IBAT_OCP,0);
        lx_err("CHG:DFX: cp_ibat_ocp_abnormal, send dfx report ibat:%d\n",manager->ibat);
    }
    
    return 0;
}
static int dfx_pd_anthen_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }
    
    if (IS_ERR_OR_NULL(manager->pd_adapter)) {
        lx_err("CHG:DFX: pd_pps_authen: pd_adapter error\n");
        return PTR_ERR(manager->pd_adapter);
    }

    if(manager->pd_adapter->verifed == 0 && manager->dfx_pd_pps_authen_flag < 1){
        manager->dfx_pd_pps_authen_flag = manager->dfx_pd_pps_authen_flag+1;
        xm_handle_dfx_report(CHG_DFX_PPS_AUTH_FAIL,manager->pd_adapter->verifed);
        lx_err("CHG:DFX: pd_pps_authen_abnormal\n");
    }

    return 0;
}
static int dfx_chg_global_soc_over_80(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if(manager->charger_online==1 && manager->chg_status == POWER_SUPPLY_STATUS_CHARGING
    && manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret == 1
    && manager->soc > 80 && manager->ibat > 0 && manager->dfx_global_soc_over_80_flag < 1){
        manager->dfx_global_soc_over_80_flag = manager->dfx_global_soc_over_80_flag+1;
        xm_handle_dfx_report(CHG_DFX_GLOBAL_SOC_OVER_80,manager->soc);
        lx_err("CHG:DFX: dfx_chg_global_soc_over_80\n");
    }
    return 0;
}
static int dfx_chg_global_soc_trig(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if(manager->charger_online == 1 && manager->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret == 1
    && manager->soc == 80 &&  manager->ibat < 0 
    && manager->dfx_global_soc_trig_flag < 1){
       manager->dfx_global_soc_trig_flag = manager->dfx_global_soc_trig_flag+1;
       xm_handle_dfx_report(CHG_DFX_GLOBAL_SOC_TRIG,manager->soc);
       lx_err("CHG:DFX: dfx_chg_global_soc_trig\n");
    }
    return 0;
}
static int dfx_high_temp_discharging_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if(manager->tbat > 480 && manager->tbat < 550 && manager->adapter_plug_in
    && manager->ibat <= 0 
    && manager->chg_status != POWER_SUPPLY_STATUS_FULL
    && manager->dfx_high_temp_discharging_flag < 1){
       manager->dfx_high_temp_discharging_flag = manager->dfx_high_temp_discharging_flag+1;
       xm_handle_dfx_report(CHG_DFX_HIGH_TEMP_DISCHARGING,0);
       lx_err("CHG:DFX: dfx_high_temp_discharging_err\n");
    }
    return 0;
}
static int dfx_low_temp_discharging_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if(manager->tbat < 50 && manager->tbat > -100 && manager->adapter_plug_in
    && manager->ibat <= 0
    && manager->chg_status != POWER_SUPPLY_STATUS_FULL 
    && manager->dfx_low_temp_discharging_flag < 1){
       manager->dfx_low_temp_discharging_flag = manager->dfx_low_temp_discharging_flag+1;
       xm_handle_dfx_report(CHG_DFX_LOW_TEMP_DISCHARGING,0);
       lx_err("CHG:DFX: dfx_low_temp_discharging_err\n");
    }
    return 0;
}
static int dfx_soc_not_full_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }
    
    if(manager->soc < 100 && manager->chg_status == POWER_SUPPLY_STATUS_FULL
    && manager->dfx_soc_not_full_flag < 1 ){
       manager->dfx_soc_not_full_flag = manager->dfx_soc_not_full_flag+1;
       xm_handle_dfx_report(CHG_DFX_SOC_NOT_FULL,manager->soc);
       lx_err("CHG:DFX: dfx_soc_not_full\n");
    }
    return 0;
}
static int dfx_battery_auth_fail_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if((!manager->authenticate) && manager->dfx_battery_auth_fail_flag <1){
        manager->dfx_battery_auth_fail_flag = manager->dfx_battery_auth_fail_flag+1;
        xm_handle_dfx_report(CHG_DFX_BATTERY_AUTH_FAIL,0);
        lx_err("CHG:DFX: dfx_battery_auth_fail\n");
    }
    return 0;
}

static int dfx_cp_tdie_hot_check(struct charger_manager *manager)
{
    struct chargerpump_dev *master_cp;
    int ret = 0;
    int val = 0;

    master_cp = chargerpump_find_dev_by_name("master_cp_chg");
    if (master_cp == NULL) {
        lx_err("can not find master_cp dev\n");
        return -EINVAL;
    }
    ret = chargerpump_get_adc_value(master_cp, ADC_GET_TDIE, &val);
    if (ret < 0) {
        lx_err("failed to get tdie adc\n");
        return -EINVAL;
    }

    if(val > 90 && manager->dfx_cp_tdie_hot_flag < 1) {
        manager->dfx_cp_tdie_hot_flag = manager->dfx_cp_tdie_hot_flag + 1;
        xm_handle_dfx_report(CHG_DFX_CP_TDIE_HOT, val);
        lx_err("CHG:DFX: cp_tide_hot_abnormal, send dfx report tide: %d\n", val);
    }

    return 0;
}

#define VBUS_OVP 10000
static int dfx_cp_vbus_ovp_check(struct charger_manager *manager)
{
    int Vbus;
    charger_get_adc(manager->charger, ADC_GET_VBUS, &Vbus);
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if(Vbus > VBUS_OVP && manager->dfx_cp_vbus_ovp_flag < 1){
        manager->dfx_cp_vbus_ovp_flag = manager->dfx_cp_vbus_ovp_flag+1;
        xm_handle_dfx_report(CHG_DFX_CP_VBUS_OVP,Vbus);
        lx_err("CHG:DFX: cp_vbus_ovp_abnormal, send dfx report vbus:%d\n",Vbus);
    } 
    return 0;
}
#define IBUS_OCP 5500
static int dfx_cp_ibus_ocp_check(struct charger_manager *manager)
{
    int Ibus;
    charger_get_adc(manager->charger, ADC_GET_IBUS, &Ibus);
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if(Ibus > IBUS_OCP && manager->dfx_cp_ibus_ocp_flag < 1){
        manager->dfx_cp_ibus_ocp_flag = manager->dfx_cp_ibus_ocp_flag+1;
        xm_handle_dfx_report(CHG_DFX_CP_IBUS_OCP,0);
        lx_err("CHG:DFX: cp_ibus_ocp_abnormal, send dfx report ibus:%d\n",Ibus);
    }
    
    return 0;
}
static int dfx_battery_cyclecount_check(struct charger_manager *manager)
{
    int ret;
    union power_supply_propval pval = {0,};
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX: point manager is err or null\n");
        return PTR_ERR(manager);
    }
    ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
    if (ret < 0) {
        lx_err("Couldn't read batt voltage_now, ret=%d\n", ret);
        }
        manager->cycle_count = pval.intval;
    if((manager->cycle_count %100) == 0){
        xm_handle_dfx_report(CHG_DFX_BATTERY_CYCLECOUNT,manager->cycle_count);
        lx_err("CHG:DFX: dfx_battery_cyclecount, send dfx report cyclecount:%d\n",manager->cycle_count);
    }
    return 0;
}
static int dfx_cp_enable_check(struct charger_manager *manager)
{
    if (IS_ERR_OR_NULL(manager)) {
        lx_err("CHG:DFX:dfx_cp_enable_check point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if (IS_ERR_OR_NULL(g_policy)) {
        lx_err("CHG:DFX:dfx_cp_enable_check g_policy is err or null\n");
        return PTR_ERR(g_policy);
    }

    if (!g_policy->cp_enable_fail && manager->dfx_cp_enable_flag != 0) {
       manager->dfx_cp_enable_flag = 0;
    } else if (g_policy->cp_enable_fail && manager->dfx_cp_enable_flag < 1) {
        manager->dfx_cp_enable_flag = manager->dfx_cp_enable_flag + 1;
        xm_handle_dfx_report(CHG_DFX_CP_ENABLE_FAIL, 0);
        lx_err("CHG:DFX: cp enable fail\n");
    }

    return 0;
}
static int dfx_none_standard_charger_slow_check(struct charger_manager *manager)
{
    if(IS_ERR_OR_NULL(manager)){
        lx_err("CHG:DFX: dfx_none_standard_charger_slow_check point manager is err or null\n");
        return PTR_ERR(manager);
    }
    if(manager->adapter_plug_in){
        if(manager->none_standard_count >= 60){
            manager->none_standard_count = 0;
            if((manager->pd_adapter->verifed != 1) && (manager->dfx_none_standard_charge_slow_flag < 1)){
                manager->dfx_none_standard_charge_slow_flag = manager->dfx_none_standard_charge_slow_flag + 1;
                xm_handle_dfx_report(CHG_DFX_NONE_CHG_SLOW,0);
                lx_err("CHG:DFX: chg_none_chg_slow%d\n");
                } 
        }else{
            manager->none_standard_count++;
        } 
    }  
    return 0;
}

static int dfx_stand_chg_slow_check(struct charger_manager *manager)
{ 
    if(IS_ERR_OR_NULL(manager)){
        lx_err("CHG:DFX: dfx_standa_charger_slow_check point manager is err or null\n");
        return PTR_ERR(manager);
    }

    if(manager->pd_adapter->verifed == 1 && manager->dfx_first_check_flag == false){
        lx_err("CHG:DFX:stand charger first check\n");
        schedule_delayed_work(&manager->dfx_stand_chg_slow_check_work, msecs_to_jiffies(1000));
        manager->dfx_first_check_flag = true;
    } 

    if(manager->soc < 80 && manager->dfx_stand_chg_check_flag && manager->pd_adapter->verifed == 1){
        manager->dfx_stand_chg_check_flag = false;
        xm_handle_dfx_report(CHG_DFX_STAND_CHG_SLOW, 0);
        lx_err("CHG:DFX:stand charger is slow\n");
    }
    return 0;
}
void dfx_stand_chg_slow_work(struct work_struct *work)
{
    struct charger_manager *chip = container_of(work, struct charger_manager, dfx_stand_chg_slow_check_work.work);
    union power_supply_propval pval;
    int ret;
    int new_fcc; 
    int diff_fcc;

    ret = power_supply_get_property(chip->fg_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
    if(chip->dfx_first_work_flag == false){
        lx_err("CHG:DFX:stand charger first work\n");
        chip->last_fcc = pval.intval;
        chip->dfx_first_work_flag = true;
        schedule_delayed_work(&chip->dfx_stand_chg_slow_check_work, msecs_to_jiffies(1000*60*10));
        return ;
    }
    new_fcc = pval.intval;
    diff_fcc = new_fcc - chip->last_fcc;
    chip->last_fcc= new_fcc;
    lx_err("CHG:DFX: dfx_stand_chg_slow_work:new_fcc:%d  last_fcc:%d  diff_fcc:%d\n",new_fcc,chip->last_fcc,diff_fcc);
    if(diff_fcc < 200000){
        chip->dfx_stand_chg_check_flag = true;
    }
    schedule_delayed_work(&chip->dfx_stand_chg_slow_check_work, msecs_to_jiffies(1000*60*10));
}
static void xm_dfx_check(struct charger_manager *manager)
{
    int ret = 0;
    ret = dfx_none_standard_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx none standard check fail\n");
    }
    ret = dfx_cp_present_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx cp err check fail\n");
    }
    ret = dfx_batt_linker_absent_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx batt linker absent check fail\n");
    }

    ret = dfx_lpd_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx lpd check fail\n");
    }
   
    ret = dfx_fg_i2c_communication_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx fg i2c communication check fail\n");
    }
    ret = dfx_batt_temp_abnormal_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx battery temp abnormal check fail\n");
    }
  
    ret = dfx_cp_vbat_ovp_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx cp vbat ovp check fail\n"); 
    }
    ret = dfx_cp_ibat_ocp_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx cp ibat ovp check fail\n"); 
    }
    ret = dfx_pd_anthen_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx pd anthen check fail\n");
    }
    ret = dfx_chg_global_soc_trig(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx chg global soc trig check fail\n"); 
    }
    ret = dfx_chg_global_soc_over_80(manager);     
    if (ret < 0) {
        lx_err("CHG:DFX: dfx chg global soc over 80 check fail\n"); 
    }
    ret = dfx_high_temp_discharging_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx high temp discharging check fail\n");
    }
    ret = dfx_low_temp_discharging_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx low temp discharging check fail\n");
    }
    ret = dfx_soc_not_full_check(manager);
    if (ret < 0) {
        lx_err("CHG:DFX: dfx soc not full check fail\n");
    }
    ret = dfx_battery_auth_fail_check(manager);
    if (ret < 0){
        lx_err("CHG:DFX: dfx battery auth fail check fail\n");
    }
    ret = dfx_cp_tdie_hot_check(manager);
    if (ret < 0){
        lx_err("CHG:DFX: dfx cp tdie hot check fail\n");
    }
    ret = dfx_cp_vbus_ovp_check(manager);
    if (ret < 0){
        lx_err("CHG:DFX: dfx cp vbus ovp check fail\n");
    }
    ret = dfx_cp_ibus_ocp_check(manager);
    if (ret < 0){
        lx_err("CHG:DFX: dfx cp ibus ocp check fail\n");
    }
    ret = dfx_battery_cyclecount_check(manager);
    if (ret < 0){
        lx_err("CHG:DFX: dfx battery cyclecount check fail\n");
    }
    ret = dfx_cp_enable_check(manager);
    if (ret < 0){
        lx_err("CHG:DFX: dfx cp enable check fail\n");
    }
    ret = dfx_none_standard_charger_slow_check(manager);
    if (ret < 0){
        lx_err("CHG:DFX: dfx none standard charger slow check fail\n");
    }
    ret = dfx_stand_chg_slow_check(manager);
    if (ret < 0){
        lx_err("CHG:DFX: dfx stand chg slow check fail\n");
    }
    if (!IS_ERR_OR_NULL(manager)) {
        lx_debug("xm_dfx_check dfx_pd_pps_authen_flag:%d,dfx_global_soc_over_80_flag:%d,dfx_global_soc_trig_flag:%d,\
        dfx_high_temp_discharging_flag:%d,dfx_low_temp_discharging_flag:%d,\
        dfx_soc_not_full_flag:%d,dfx_battery_auth_fail_flag:%d,\
        dfx_cp_tdie_hot_flag:%d,dfx_cp_vbus_ovp_flag:%d,\
        dfx_cp_ibus_ocp_flag:%d,dfx_cp_enable_flag:%d",
        manager->dfx_pd_pps_authen_flag,
        manager->dfx_global_soc_over_80_flag,manager->dfx_global_soc_trig_flag,
        manager->dfx_high_temp_discharging_flag,manager->dfx_low_temp_discharging_flag,
        manager->dfx_soc_not_full_flag,manager->dfx_battery_auth_fail_flag,
        manager->dfx_cp_tdie_hot_flag,manager->dfx_cp_vbus_ovp_flag,
        manager->dfx_cp_ibus_ocp_flag,manager->dfx_cp_enable_flag);
    }
    return;
}
#endif

void xm_charge_work(struct work_struct *work)
{
	struct charger_manager *chip = container_of(work, struct charger_manager, xm_charge_work.work);

	lx_debug("check xm_charge_work\n");
	monitor_smart_chg(chip);
	monitor_night_charging(chip);
	monitor_low_fast_strategy(chip);
#if IS_ENABLED(CONFIG_MIEV)
	monitor_outdoor_charging(chip);
#endif
	xm_smart_chg_endurance_protect(chip);
	xm_smart_chg_handle_bypass_chg(chip);
	xm_bat_protect_work(chip);
	get_drop_floatvolatge(chip);

#if IS_ENABLED(CONFIG_MIEV)
	xm_dfx_check(chip);
#endif
	/* *
	* Move to lx_charger_manager fv_votable callback
	* monitor_smart_batt(chip);
	* monitor_cycle_count(chip);
	* */

	schedule_delayed_work(&chip->xm_charge_work, msecs_to_jiffies(1000));
}
