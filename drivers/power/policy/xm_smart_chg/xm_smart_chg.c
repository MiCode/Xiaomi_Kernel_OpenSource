// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Xiaomi Inc.
 * Author Tianye<tianye9@xiaomi.com>
 */
#include <linux/delay.h>
#include "xm_smart_chg.h"

#define XM_LOW_BAT_FAST_CHG		0
#define XM_OUTDOOR_CHG			0

struct xm_smart_chg_info *info;

void set_error(struct xm_smart_chg_info *info)
{
	info->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 1;
	lc_info("xm %s en_ret=%d\n", __func__, info->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

void set_success(struct xm_smart_chg_info *info)
{
	info->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = 0;
	lc_info("xm %s en_ret=%d\n", __func__, info->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);
}

int smart_chg_is_error(struct xm_smart_chg_info *info)
{
	return info->smart_charge[SMART_CHG_STATUS_FLAG].en_ret? true : false;
}

void handle_smart_chg_functype(struct xm_smart_chg_info *info,
	const int func_type, const int en_ret, const int func_val)
{
	switch (func_type)
	{
	case SMART_CHG_FEATURE_MIN_NUM ... SMART_CHG_FEATURE_MAX_NUM:
		info->smart_charge[func_type].en_ret = en_ret;
		info->smart_charge[func_type].active_status = false;
		info->smart_charge[func_type].func_val = func_val;
		set_success(info);
		lc_info("xm set func_type:%d, en_ret = %d\n", func_type, en_ret);
		break;
	default:
		lc_info("xm ERROR: Not supported func type: %d\n", func_type);
		set_error(info);
		break;
	}
}

int handle_smart_chg_functype_status(struct xm_smart_chg_info *info)
{
	int i;
	int all_func_status = 0;
	all_func_status |= !!info->smart_charge[SMART_CHG_STATUS_FLAG].en_ret;	//handle bit0

	lc_info("all_func_status =%#X, en_ret=%d\n", all_func_status, info->smart_charge[SMART_CHG_STATUS_FLAG].en_ret);

	/* save functype[i] enable status in all_func_status bit[i] */
	for (i = SMART_CHG_FEATURE_MIN_NUM; i <= SMART_CHG_FEATURE_MAX_NUM; i++){  //handle bit1 ~ bit SMART_CHG_FEATURE_MAX_NUM
		if (info->smart_charge[i].en_ret)
			all_func_status |= BIT_MASK(i);
		else
			all_func_status &= ~BIT_MASK(i);

		lc_info("%s type:%d, en_ret=%d, active_status=%d,func_val=%d, all_func_status=%#X\n", __func__,
			i, info->smart_charge[i].en_ret, info->smart_charge[i].active_status, info->smart_charge[i].func_val,all_func_status);
	}

	lc_info( "all_func_status:%#X\n", all_func_status);
	return all_func_status;
}

static int endurance_old_mode = 0;
static int navigation_old_mode = 0;
void monitor_smart_chg(struct xm_smart_chg_info *info)
{
	static int g_smart_soc = 50;
	static bool ignore_upload_endurance_pro = true;
	static bool ignore_upload_navigation = true;

	lc_info("SMART_CHG_NAVIGATION, en_ret = %d, fun_val = %d, active_status = %d, smart_soc = %d, ui_soc =%d, smart_ctrl_en = %d, ffc_en = %d\n",
		info->smart_charge[SMART_CHG_NAVIGATION].en_ret,
		info->smart_charge[SMART_CHG_NAVIGATION].func_val,
		info->smart_charge[SMART_CHG_NAVIGATION].active_status,
		g_smart_soc,
		info->soc,
		info->smart_ctrl_en,
		info->ffc_en);

	info->navigation_changed_flag = (info->smart_charge[SMART_CHG_NAVIGATION].en_ret == navigation_old_mode) ? false : true;
	lc_info("navigation changed flag %d, smart mode %d, navigation old mode %d", info->navigation_changed_flag,
											info->smart_charge[SMART_CHG_NAVIGATION].en_ret, navigation_old_mode);
	if (info->smart_charge[SMART_CHG_NAVIGATION].en_ret != navigation_old_mode)
	{
		navigation_old_mode = info->smart_charge[SMART_CHG_NAVIGATION].en_ret;
	}

	if (info->smart_charge[SMART_CHG_NAVIGATION].en_ret && info->soc >= info->smart_charge[SMART_CHG_NAVIGATION].func_val && !info->smart_charge[SMART_CHG_NAVIGATION].active_status)
	{
		info->smart_charge[SMART_CHG_NAVIGATION].active_status = true;
		g_smart_soc = info->smart_charge[SMART_CHG_NAVIGATION].func_val;
		info->chg_info->nav_stop_flag = true;
		info->smart_ctrl_en = true;
		lc_info("navigation monitor disable charger, soc >= %d\n", info->smart_charge[SMART_CHG_NAVIGATION].func_val);
	}
	else if((((!info->smart_charge[SMART_CHG_NAVIGATION].en_ret || info->soc <= info->smart_charge[SMART_CHG_NAVIGATION].func_val - 5) && info->smart_charge[SMART_CHG_NAVIGATION].active_status) ||
		(!info->smart_charge[SMART_CHG_NAVIGATION].en_ret && !info->smart_charge[SMART_CHG_NAVIGATION].active_status)) && info->smart_ctrl_en)
	{
		info->smart_charge[SMART_CHG_NAVIGATION].active_status = false;
		info->chg_info->nav_stop_flag = false;
		info->smart_ctrl_en = false;
		lc_info("navigation monitor enable charger, soc <= %d\n", info->smart_charge[SMART_CHG_NAVIGATION].func_val - 5);
	}

	if (info->navigation_changed_flag)
	{
		if (info->smart_charge[SMART_CHG_NAVIGATION].en_ret) {
			xm_dfs_work_report_event(CHG_DFX_NAVIGATION, true);
			ignore_upload_navigation = info->soc >= info->smart_charge[SMART_CHG_NAVIGATION].func_val ? true :false;
		} else {
			xm_dfs_work_report_event(CHG_DFX_NAVIGATION, false);
		}
	}
	if (info->soc >= info->smart_charge[SMART_CHG_NAVIGATION].func_val) {
		if (info->soc >= info->smart_charge[SMART_CHG_NAVIGATION].func_val + 2) {
			if (info->smart_charge[SMART_CHG_NAVIGATION].active_status && !ignore_upload_navigation){
				xm_dfs_work_report_event(CHG_DFX_NAVIGATION_OVER_SOC, true);
				lc_info("navigation soc error\n");
				ignore_upload_navigation = false;
			}
		}
	}

#if XM_LOW_BAT_FAST_CHG
	if (info->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
		info->smart_charge[SMART_CHG_LOW_FAST].active_status = true;
		lc_info("set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, enable low_fast\n", info->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
	else if(!info->smart_charge[SMART_CHG_LOW_FAST].en_ret)
	{
		info->smart_charge[SMART_CHG_LOW_FAST].active_status = false;
		lc_info("set smart_charge[SMART_CHG_LOW_FAST].en_ret = %d, disable low_fast\n", info->smart_charge[SMART_CHG_LOW_FAST].en_ret);
	}
#endif

	lc_info("SMART_CHG_ENDURANCE_PRO, en_ret = %d, fun_val = %d, active_status = %d\n",
		info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret,
		info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val,
		info->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status);

	info->endurance_changed_flag = (info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret == endurance_old_mode) ? false : true;
	lc_info("navigation changed flag %d, smart mode %d, navigation old mode %d", info->endurance_changed_flag, 
											info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret, endurance_old_mode);
	if (info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret != endurance_old_mode)
	{
		endurance_old_mode = info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret;
	}

	if((info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && info->soc >= info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) ||
		((info->soc > (info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val -5) && 
		(info->soc < info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val)) &&
		(info->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status))) {
		info->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = true;
		g_smart_soc = info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val;
		info->chg_info->pro_stop_flag = true;
		lc_info("endurance monitor disable charger,soc >= %d\n", info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val);
	} else if (((!info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret || info->soc <= (info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val - 5)) && info->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status) ||
		(!info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret && !info->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status)) {
		info->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status = false;
		info->chg_info->pro_stop_flag = false;
		lc_info("endurance monitor enable charger, soc <= %d\n", info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val - 5);
	}

	if (info->endurance_changed_flag)
	{
		if (info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret) {
			xm_dfs_work_report_event(CHG_DFX_SMART_ENDURANCE_TRIGGERED, true);
		} else {
			xm_dfs_work_report_event(CHG_DFX_SMART_ENDURANCE_TRIGGERED, false);
		}
	}

	if (info->endurance_changed_flag)
	{
		if (info->smart_charge[SMART_CHG_ENDURANCE_PRO].en_ret) {
			xm_dfs_work_report_event(CHG_DFX_SMART_ENDURANCE_TRIGGERED, true);
			ignore_upload_endurance_pro = info->soc >= info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val ? true :false;
		} else {
			xm_dfs_work_report_event(CHG_DFX_SMART_ENDURANCE_TRIGGERED, false);
		}
	}
	if (info->soc >= info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val) {
		if (info->soc >= info->smart_charge[SMART_CHG_ENDURANCE_PRO].func_val + 1) {
			if (info->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status && !ignore_upload_endurance_pro){
				xm_dfs_work_report_event(CHG_DFX_SMART_ENDURANCE_SOC_ERR, true);
				lc_info("navigation soc error\n");
				ignore_upload_endurance_pro = false;
			}
		}
	}

#if XM_OUTDOOR_CHG
	if (info->smart_charge[SMART_CHG_OUTDOOR_CHARGE].en_ret) {
		info->chg_info->outdoor_flag = 1;
	} else {
		info->chg_info->outdoor_flag = 0;
	}
#endif
}

static ssize_t smart_chg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	// DECLEAR_BITMAP(func_type, SMART_CHG_FEATURE_MAX_NUM);
	int val;
	bool en_ret;
	unsigned long func_type;
	int func_val;
	int bit_pos;
	int all_func_status;

	if (IS_ERR_OR_NULL(info))
		return PTR_ERR(info);

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	en_ret = val & 0x1;
	func_type = (val & 0xFFFE) >> 1;
	func_val = val >> 16;

	lc_info("%s:get val:%#X, func_type:%#lX, en_ret:%d, func_val:%d\n",
		__func__, val, func_type, en_ret, func_val);
	bit_pos = find_first_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM);

	if (bit_pos == SMART_CHG_FEATURE_MAX_NUM || find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1) != SMART_CHG_FEATURE_MAX_NUM){
		lc_err("ERROR: zero or more than one func type!\n");
		lc_err("find_next_bit = %ld, bit_pos = %d\n",
		find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1), bit_pos);
		set_error(info);
	} else
		set_success(info);

	// if func_type bit0 is 1, bit_pos = 0, not 1. so ++bit_pos.
	if (!smart_chg_is_error(info))
		handle_smart_chg_functype(info, ++bit_pos, en_ret, func_val);

	/* update smart_chg[0] status */
	all_func_status = handle_smart_chg_functype_status(info);
	info->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = all_func_status & 0x1;
	info->smart_charge[SMART_CHG_STATUS_FLAG].active_status = (all_func_status & 0xFFFE) >> 1;

	return count;
}

static ssize_t smart_chg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int val;

	if (IS_ERR_OR_NULL(info))
		return PTR_ERR(info);

	val = handle_smart_chg_functype_status(info);

	return sprintf(buf, "%d\n", val);
}

static struct device_attribute smart_chg_attr =
		__ATTR(smart_chg, 0644, smart_chg_show, smart_chg_store);

static ssize_t smart_batt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	lc_info("%s:smart_batt = %d", __func__, info->chg_info->smart_fv);
	return sprintf(buf, "%d\n", info->chg_info->smart_fv);
}

static ssize_t smart_batt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int smart_batt = 0;

	if (kstrtoint(buf, 0, &smart_batt))
		return -EINVAL;
	lc_info("%s smart_batt = %d", __func__, smart_batt);

	info->chg_info->smart_fv = smart_batt;

	return size;
}
static struct device_attribute smart_batt_attrs =
	__ATTR(smart_batt, 0644, smart_batt_show, smart_batt_store);

static ssize_t night_charging_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	lc_info("%s:night_charging = %d", __func__, info->night_charging);
	return sprintf(buf, "%d\n", info->night_charging);
}
static ssize_t night_charging_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (kstrtobool(buf, &info->night_charging))
		return -EINVAL;
	lc_info("%s:night_charging = %d", __func__, info->night_charging);

	return size;
}
static struct device_attribute night_charging_attrs =
	__ATTR(night_charging, 0644, night_charging_show, night_charging_store);

static ssize_t rsoc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	lc_info("%s:raw_soc = %d", __func__, info->chg_info->desc->raw_cap);
	return sprintf(buf, "%d\n", info->chg_info->desc->raw_cap);
}

static struct device_attribute rsoc_attrs =
	__ATTR_RO(rsoc);

static struct attribute *batt_psy_attrs[] = {
	&smart_batt_attrs.attr,
	&night_charging_attrs.attr,
	&smart_chg_attr.attr,
	&rsoc_attrs.attr,
	NULL,
};

static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};

int xm_smart_charge_add_to_batt_psy(void)
{
	int ret = 0;

	info->batt_psy = power_supply_get_by_name("battery");
	if (!info->batt_psy){
		lc_err("%s failed to get batt_psy", __func__);
		return -EPROBE_DEFER;
	}
	lc_info("%s success to get batt_psy", __func__);

	ret = sysfs_create_group(&(info->batt_psy->dev.kobj), &batt_psy_attrs_group);
	if(ret){
		pr_err("%s failed to creat batt_psy group, ret = %d\n", __func__, ret);
		return -EPROBE_DEFER;
	}

	info->chg_info = (struct charger_manager *)power_supply_get_drvdata(info->batt_psy);
	if(info->chg_info == NULL)
	{
		pr_err("%s failed to get charger_manager\n", __func__);
		return -EPROBE_DEFER;
	}
	return ret;
}

void get_fv_ageing(struct xm_smart_chg_info *info, int cyclecount, int *fv_aging)
{
	int i = 0;

	while (cyclecount > cycle_count_conf[i])
		i++;
	*fv_aging = dropfv_conf[i];

	lc_dbg("%s:i = %d, fv_aging = %d\n", __func__, i, *fv_aging);

	return;
}

void get_drop_floatvolatge(struct xm_smart_chg_info *info)
{
	int ret = 0;
	union power_supply_propval pval;
	int cycle_count = 0;
	union power_supply_propval pval1;
	int temp = 0;
	static int last_fv_ageing = 0;

	ret = power_supply_get_property(info->batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		lc_err("%s:failed to get cycle_count prop", __func__);
		return;
	}
	cycle_count = pval.intval;

	ret = power_supply_get_property(info->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval1);
	if (ret < 0) {
		lc_err("%s:failed to get temp prop", __func__);
		return;
	}
	temp = pval1.intval;

	get_fv_ageing(info, cycle_count, &info->fv_ageing);
	if (info->fv_ageing != last_fv_ageing && (temp >= CYCLE_LOW_TEMP && temp <= CYCLE_HIGH_TEMP)){
		info->chg_info->cycle_fv = info->fv_ageing;
	}
	last_fv_ageing = info->fv_ageing;
}

void monitor_night_charging(struct xm_smart_chg_info *info)
{
	if (info == NULL)
		return;

	lc_dbg("%s:night_charging = %d, soc = %d\n", __func__, info->night_charging, info->soc);
	if (info->night_charging && (info->soc >= 80)) {
		info->night_charging_flag = true;
		lc_info("%s disable charging\n", __func__);
		info->chg_info->nig_stop_flag = true;
	} else if (info->night_charging_flag && (!info->night_charging || info->soc <= 75)) {
		info->night_charging_flag = false;
		lc_info("%s enable charging\n", __func__);
		info->chg_info->nig_stop_flag = false;
	}
}

#if XM_LOW_BAT_FAST_CHG
void monitor_low_fast_strategy(struct xm_smart_chg_info *info)
{
	bool fast_flag = false;
	time64_t time_now = 0, delta_time = 0;
	static time64_t time_last = 0;
	int thermal_vote_current;

	if (info == NULL)
		return;

	lc_info("%s:soc = %d, thermal_level = %d, thermal_board_temp = %d, pd_active = %d, low_fast_plugin_flag = %d, low_fast_enable = %d, screen_state = %d, b_flag = %d\n", 
		__func__, info->soc, info->thermal_level, info->thermal_board_temp, info->chg_info->bat.ffc, info->low_fast_plugin_flag, info->smart_charge[SMART_CHG_LOW_FAST].active_status, 
		info->sm.screen_state, info->b_flag);

	if ((info->chg_info->bat.ffc) && (info->soc <= 40) && (info->low_fast_plugin_flag) && info->smart_charge[SMART_CHG_LOW_FAST].active_status) {
		if (info->thermal_level > MAX_THERMAL_LEVELS) {
			lc_err("%s:thermal level is Invalid\n", __func__);
			goto err;
		}

		// sm.screen_state 0:bright, 1:black
		if ((info->b_flag == SNORMAL || info->b_flag == BLACK) && !info->sm.screen_state) {  //black to bright
			info->b_flag = BLACK_TO_BRIGHT;
			time_last = ktime_get_seconds();
			fast_flag = true;
			lc_info("%s:switch to bright time_last = %lld\n", __func__, time_last);
		} else if ((info->b_flag == BLACK_TO_BRIGHT || info->b_flag == BRIGHT) && !info->sm.screen_state) {  //still bright
			info->b_flag = BRIGHT;
			time_now = ktime_get_seconds();
			delta_time = time_now - time_last;
			lc_info("%s:still_bright time_now = %lld, time_last = %lld, delta_time = %lld\n", __func__, time_now, time_last, delta_time);
			if (delta_time <= 15) {
				fast_flag = true;
				lc_info("%s:still_bright delta_time = %lld, stay fast\n", __func__, delta_time);
			} else {
				fast_flag = false;
				lc_info("%s:still_bright delta_time = %lld, exit fast\n", __func__, delta_time);
			}
		} else { //black
			info->b_flag = BLACK;
			fast_flag = true;
			lc_info("%s:black stay fast\n", __func__);
		}

		if(fast_flag) {  //stay fast strategy
			info->pps_fast_mode = true;
			thermal_vote_current = info->pps_thermal_mitigation_fast[info->thermal_level];

			if ((info->soc > 38) && (info->thermal_board_temp > 380)) {
				if(thermal_vote_current >= 5450000){
					thermal_vote_current -= 3300000;
				} else {
					thermal_vote_current = 2150000;
				}
				lc_info("%s:stay fast but decrease 3.3, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
			} else if ((info->soc > 35) && (info->thermal_board_temp > 390)){
				if (thermal_vote_current >= 3950000) {
					thermal_vote_current -= 1800000;
				} else {
					thermal_vote_current = 2150000;
				}
				lc_info("%s:stay fast but decrease 1.8, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
			} else if ((info->soc > 30) && (info->thermal_board_temp > 400)){
				if (thermal_vote_current >= 3150000) {
					thermal_vote_current -= 1000000;
				} else {
					thermal_vote_current = 2150000;
				}
				lc_info("%s:stay fast but decrease 1, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
			} else if ((info->soc < 30) && (info->thermal_board_temp > 400)) {
				if (thermal_vote_current >= 3150000) {
					thermal_vote_current -= 1000000;
				} else {
					thermal_vote_current = 2150000;
				}
				lc_info("%s:stay fast but decrease 1, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
			} else if ((info->soc > 30) && (info->soc < 39) && (info->thermal_board_temp < 390)){
				if(thermal_vote_current <= 4000000){
					thermal_vote_current += 2000000;
				} else {
					thermal_vote_current = 6000000;
				}
				lc_info("%s:stay fast but add 2, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
			} else if ((info->soc > 25) && ((info->soc <= 30)) && (info->thermal_board_temp < 400)){
				if(thermal_vote_current <= 3000000){
					thermal_vote_current += 3000000;
				} else {
					thermal_vote_current = 6000000;
				}
				lc_info("%s:stay fast but add 3, info->thermal_board_temp = %d, thermal_vote_current = %d\n", __func__, info->thermal_board_temp, thermal_vote_current);
			}

			lc_info("%s stay fast, thermal_vote_current = %d\n", __func__, thermal_vote_current);
		} else { //exit fast strategy
			info->pps_fast_mode = false;
			thermal_vote_current = info->pps_thermal_mitigation[info->thermal_level];
			lc_info("%s:exit fast, thermal_vote_current = %d\n", __func__, thermal_vote_current);
		}
		info->chg_info->low_fast_current = thermal_vote_current;
		return;
	}

err:
	thermal_vote_current = info->pps_thermal_mitigation[info->thermal_level];
	info->chg_info->low_fast_current = thermal_vote_current;
	lc_info("%s:use default thermal current=%d\n", __func__, thermal_vote_current);
	return;
}
#endif

void xm_charge_update_date(void)
{
	union power_supply_propval pval = {0,};
	int ret;

	if (!info->batt_psy) {
		info->batt_psy = power_supply_get_by_name("battery");
		if (!info->batt_psy){
			lc_err("%s:failed to get batt_psy", __func__);
			return;
		}
	}

	ret = power_supply_get_property(info->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret < 0) {
		lc_err("get battery soc error.\n");
	} else {
		info->soc = pval.intval;
	}

	ret = power_supply_get_property(info->batt_psy, POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &pval);
	if (ret < 0) {
		lc_err("get thermal_level fail\n");
	} else {
		info->thermal_level = pval.intval;
	}

#if XM_LOW_BAT_FAST_CHG
	if (!info->bms_psy) {
		info->bms_psy = power_supply_get_by_name("bms");
		if (!info->bms_psy){
			lc_err("%s:failed to get bms_psy", __func__);
			return;
		}
	}

	ret = power_supply_get_property(info->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		lc_err("get board temp fail\n");
	} else {
		info->thermal_board_temp = pval.intval;
	}
#endif
}

void xm_charge_work(struct work_struct *work)
{
	//struct xm_smart_chg_info *info = container_of(work, struct xm_smart_chg_info, xm_charge_work.work);

	//lc_info("check xm_charge_work\n");
	xm_charge_update_date();
	monitor_smart_chg(info);
	monitor_night_charging(info);
#if XM_LOW_BAT_FAST_CHG
	monitor_low_fast_strategy(info);
#endif
//	get_drop_floatvolatge(info);

	schedule_delayed_work(&info->xm_charge_work, msecs_to_jiffies(1000));
}

#if XM_LOW_BAT_FAST_CHG
static int usb_notifier_event_callback(struct notifier_block *notifier,
	unsigned long chg_event, void *val)
{
	switch (chg_event) {
	/*charger_plugin_event 0:none 1:plugin 2:plugout*/
	case MTK_CHARGER_PLUGIN_EVENT:
		info->charger_plugin_event = *(int *)val;
		lc_info("%s: get charger_plugin_event: %d\n", __func__, info->charger_plugin_event);

		lc_err("%s soc = %d\n", __func__, info->soc);
		if ((info->charger_plugin_event == 1) && (info->soc <= 40) && (info->thermal_board_temp <= 390)) {
			info->low_fast_plugin_flag = 1;
		} else if(info->charger_plugin_event == 2) {
			info->low_fast_plugin_flag = 0;
		}
		info->smart_ctrl_en = false;
		info->pps_fast_mode = false;
		info->b_flag = SNORMAL;

		break;
	default:
		lc_err("%s: not supported charger notifier event: %lu\n", __func__, chg_event);
		break;
	}

	return NOTIFY_DONE;
}

static int screen_state_for_charger_callback(struct notifier_block *nb,
	unsigned long val, void *v)
{
	int blank = *(int *)v;
	struct xm_smart_chg_info *info = container_of(nb, struct xm_smart_chg_info, sm.charger_panel_notifier);

	if (!(val == MTK_DISP_EARLY_EVENT_BLANK|| val == MTK_DISP_EVENT_BLANK)) {
		lc_err("%s:event(%lu) do not need process\n", __func__, val);
		return NOTIFY_OK;
	}
 	switch (blank) {
		case MTK_DISP_BLANK_UNBLANK: //power on
			info->sm.screen_state = 0;
			lc_info("%s:screen_state = %d\n", __func__, info->sm.screen_state);
			break;
		case MTK_DISP_BLANK_POWERDOWN: //power off
			info->sm.screen_state = 1;
			lc_info("%s:screen_state = %d\n", __func__, info->sm.screen_state);
			break;
		}
		return NOTIFY_OK;
}
#endif

int xm_smart_chg_parse_dt(struct platform_device *pdev)
{
	int ret = 0;

	ret = of_property_read_string(pdev->dev.of_node, "label", &info->label);
	if (ret < 0) {
		lc_err("%s:failed to read label(%d), use default label\n", __func__, ret);
		info->label = "smart_charger";
	}
	lc_dbg("%s:label:%s\n", __func__, info->label);

#if XM_LOW_BAT_FAST_CHG
	int byte_len = 0, i = 0;
	if (of_find_property(pdev->dev.of_node, "xiaomi,pd-thermal-mitigation-fast", &byte_len)) {
		info->pps_thermal_mitigation_fast = kzalloc(byte_len, GFP_KERNEL);
		if (info->pps_thermal_mitigation_fast == NULL){
			lc_err("%s:failed to alloc mem for pps_thermal_mitigation_fast\n", __func__);
			return -ENOMEM;
		}
		info->thermal_levels = byte_len / sizeof(u32);
		ret = of_property_read_u32_array(pdev->dev.of_node, "xiaomi,pd-thermal-mitigation-fast",
			info->pps_thermal_mitigation_fast,
			info->thermal_levels);
		if (ret < 0) {
			lc_err("%s:failed read pps_thermal_mitigation_fast from dtsi, ret = %d\n", __func__, ret);
			return ret;
		}
		for (i = 0; i < info->thermal_levels; i++){
			lc_dbg("%s:pps_thermal_mitigation_fast[%d] = %d\n", __func__, i, info->pps_thermal_mitigation_fast[i]);
		}
	}

	if (of_find_property(pdev->dev.of_node, "xiaomi,pd-thermal-mitigation", &byte_len)) {
		info->pps_thermal_mitigation = kzalloc(byte_len, GFP_KERNEL);
		if (info->pps_thermal_mitigation == NULL){
			lc_err("%s:failed to alloc mem for pps_thermal_mitigation\n", __func__);
			return -ENOMEM;
		}
		info->thermal_levels = byte_len / sizeof(u32);
		ret = of_property_read_u32_array(pdev->dev.of_node, "xiaomi,pd-thermal-mitigation",
			info->pps_thermal_mitigation,
			info->thermal_levels);
		if (ret < 0) {
			lc_err("%s:failed read pps_thermal_mitigation from dtsi, ret = %d\n", __func__, ret);
			return ret;
		}
		for (i = 0; i < info->thermal_levels; i++){
			lc_dbg("%s:pps_thermal_mitigation[%d] = %d\n", __func__, i, info->pps_thermal_mitigation[i]);
		}
	}
#endif
	return 0;
}

int xm_smart_chg_post_init(void)
{
	int ret = 0;

	/*add sysfs node to battery psy*/
	ret = xm_smart_charge_add_to_batt_psy();
	if(ret){
		pr_err("%s xm_smart_charge_add_to_batt_psy failed(%d)\n", __func__, ret);
		return ret;
	}

#if XM_LOW_BAT_FAST_CHG
	info->smart_chg_nb.notifier_call = usb_notifier_event_callback;
	smart_charger_reg_notifier(&info->smart_chg_nb);

	info->sm.charger_panel_notifier.notifier_call = screen_state_for_charger_callback;
 	ret = mtk_disp_notifier_register("screen state", &info->sm.charger_panel_notifier);
	if (ret) {
		lc_err("%s: register screen state callback failed\n", __func__);
	}
#endif

	return ret;
}

int xm_smart_chg_info_init(struct platform_device *pdev)
{
	struct xm_smart_chg_info;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info){
		lc_err("%s failed to alloc memory for indio_dev", __func__);
		return -ENOMEM;
	}

	info->night_charging_flag = false;
	info->smart_ctrl_en = false;

	INIT_DELAYED_WORK(&info->xm_charge_work, xm_charge_work);
	return 0;
}

static int xm_smart_chg_probe(struct platform_device *pdev)
{
	int ret = 0;
	lc_info("%s:start \n", __func__);

	ret = xm_smart_chg_info_init(pdev);
	if(ret < 0){
		lc_err("%s:xm_smart_chg_info_init failed, xm_smart_chg driver exit\n", __func__);
		return ret;
	}
	info->dev = pdev->dev;

	ret = xm_smart_chg_parse_dt(pdev);
	if(ret < 0){
		lc_err("%s:xm_smart_chg_parse_dt failed, xm_smart_chg driver exit\n", __func__);
		return ret;
	}

	ret = xm_smart_chg_post_init();
	if(ret < 0){
		lc_err("%s:xm_smart_chg_post_init failed, xm_smart_chg driver exit\n", __func__);
		return ret;
	}else{
		lc_err("%s:xm_smart_chg_post_init success\n", __func__);
	}

	schedule_delayed_work(&info->xm_charge_work, msecs_to_jiffies(XM_CHARGE_WORK_MS*3));

#if IS_ENABLED(CONFIG_XM_DFX_CHARGER)
	init_xm_dfs_info(&pdev->dev);
#endif

	lc_err("%s:successful\n", __func__);

	return 0;
}

static int xm_smart_chg_remove(struct platform_device *pdev)
{
	lc_info("%s\n", __func__);
	return 0;
}

static const struct of_device_id xm_smart_chg_of_ids[] = {
	{
		.compatible = "xiaomi,xm_smart_chg",
	},
	{},
};
MODULE_DEVICE_TABLE(of, xm_smart_chg_of_ids);
static struct platform_driver xm_smart_chg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "xm_smart_chg",
		.of_match_table = xm_smart_chg_of_ids,
	},
	.probe = xm_smart_chg_probe,
	.remove = xm_smart_chg_remove,
};

/******************* Module Init ***********************************/
static int __init xm_smart_chg_init(void)
{
	lc_info("%s\n", __func__);
	return platform_driver_register(&xm_smart_chg_driver);
}
static void __exit xm_smart_chg_exit(void)
{
	struct power_supply *batt_psy;

	lc_info("%s\n", __func__);
	cancel_delayed_work(&info->xm_charge_work);

	batt_psy = power_supply_get_by_name("battery");
	if(!batt_psy){
		lc_err("%s failed to get batt_psy", __func__);
		return;
	}
	sysfs_remove_group(&batt_psy->dev.kobj, &batt_psy_attrs_group);

#if IS_ENABLED(CONFIG_XM_DFX_CHARGER)
	deinit_xm_dfs_info();
#endif

	platform_driver_unregister(&xm_smart_chg_driver);
}
late_initcall(xm_smart_chg_init);
module_exit(xm_smart_chg_exit);

MODULE_DESCRIPTION("XM SMART CHARGE Driver");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("lc charger");