// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019 The Linux Foundation. All rights reserved.
 */

#include "hq_charger_manager.h"
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "../common/hq_voter.h"
#endif
#include "../hq_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[HQ_CHG][CM]"
#endif

static int charger_usb_get_property(struct power_supply *psy,
						enum power_supply_property psp,
						union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	bool online = false;
	enum vbus_type vbus_type;
	int ret = 0;
	int volt = 0;
	int curr = 0;
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->charger)) {
		hq_err("manager charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		ret = charger_get_vbus_type(manager->charger, &vbus_type);
		if (ret < 0)
			hq_err("Couldn't get usb type ret=%d\n", ret);
		if (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP)
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = charger_get_online(manager->charger, &online);
		if (ret < 0)
			val->intval = 0;
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
		/*****when detect MI-PD adapter plug in, it would do once hard reset,
		Causing charging interruption, so we nedd get bc_type to set online*****/
		else if (manager->vbus_type == 0)
			val->intval = 0;
#endif
		else
			val->intval = online;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = VOLTAGE_MAX;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = charger_get_adc(manager->charger, ADC_GET_VBUS, &volt);
		if (ret < 0) {
			hq_err("Couldn't read input volt ret=%d\n", ret);
			return ret;
		}
		val->intval = volt;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = charger_manager_get_current(manager, &curr);
		if (ret < 0) {
			hq_err("Couldn't read input curr ret=%d\n", ret);
			return ret;
		}
		val->intval = curr;
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = CURRENT_MAX;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = INPUT_CURRENT_LIMIT;
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = POWER_SUPPLY_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = POWER_SUPPLY_MODEL_NAME;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int charger_usb_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	switch (prop) {
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static int usb_psy_is_writeable(struct power_supply *psy, enum power_supply_property psp)
{
	switch(psp) {
	default:
		return 0;
	}
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = usb_props,
	.num_properties = ARRAY_SIZE(usb_props),
	.get_property = charger_usb_get_property,
	.set_property = charger_usb_set_property,
	.property_is_writeable = usb_psy_is_writeable,
};

int charger_manager_usb_psy_register(struct charger_manager *manager)
{
	struct power_supply_config usb_psy_cfg = { .drv_data = manager,};

	memcpy(&manager->usb_psy_desc, &usb_psy_desc, sizeof(manager->usb_psy_desc));

	manager->usb_psy = devm_power_supply_register(manager->dev, &manager->usb_psy_desc,
							&usb_psy_cfg);
	if (IS_ERR(manager->usb_psy)) {
		hq_err("usb psy register failed\n");
		return PTR_ERR(manager->usb_psy);
	}
	return 0;
}
EXPORT_SYMBOL(charger_manager_usb_psy_register);

static int get_battery_health(struct charger_manager *manager)
{
	union power_supply_propval pval;
	int battery_health = POWER_SUPPLY_HEALTH_GOOD;
	int ret = 0;

	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		hq_err("failed to get temp prop\n");
		return -EINVAL;
	}

	if (pval.intval <= manager->battery_temp[TEMP_LEVEL_COLD])
		battery_health = POWER_SUPPLY_HEALTH_COLD;
	else if (pval.intval <= manager->battery_temp[TEMP_LEVEL_COOL])
		battery_health = POWER_SUPPLY_HEALTH_COOL;
	else if (pval.intval <= manager->battery_temp[TEMP_LEVEL_GOOD])
		battery_health = POWER_SUPPLY_HEALTH_GOOD;
	else if (pval.intval <= manager->battery_temp[TEMP_LEVEL_WARM])
		battery_health = POWER_SUPPLY_HEALTH_WARM;
	else if (pval.intval < manager->battery_temp[TEMP_LEVEL_HOT])
		battery_health = POWER_SUPPLY_HEALTH_HOT;
	else
		battery_health = POWER_SUPPLY_HEALTH_OVERHEAT;

	return battery_health;
}

static int charger_batt_get_property(struct power_supply *psy,
						 enum power_supply_property psp,
						 union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	union power_supply_propval pval;
	int state = 0, status = 0;
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	int warm_stop_charge = 0;
	bool online = false;
#endif
	int  bat_volt = 0;
	int ret = 0;
	bool otg_value = false;
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
        struct votable	*is_full_votable = NULL;
        int is_full_flag = 0;
#endif

	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (manager->fg_psy == NULL)
		manager->fg_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(manager->fg_psy)) {
		hq_err("failed to get bms psy\n");
		return PTR_ERR(manager->fg_psy);
	}

	if (IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
		manager->main_chg_disable_votable = find_votable("MAIN_CHG_DISABLE");
		if(IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
			hq_err("failed to get main_chg_disable_votable\n");
			return PTR_ERR(manager->main_chg_disable_votable);
		}
	}

	charger_get_otg_status(manager->charger, &otg_value);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = charger_get_chg_status(manager->charger, &state, &status);
			if (ret < 0) {
				hq_err("failed to get chg status prop\n");
				break;
			}
			is_full_votable = find_votable("IS_FULL");
			if (!is_full_votable) {
				hq_err("failed to get is_full_votable\n");
				return -EINVAL;
			}
			if (status == POWER_SUPPLY_STATUS_FULL && manager->soc >= 99)
				vote(is_full_votable, SMOOTH_NEW_VOTER, true, 0);

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			if (g_policy == NULL) {
				hq_err("g_policy is NULL\n");
			} else {
				if ((g_policy->switch1_1_enable || g_policy->switch1_1_single_enable) &&
					(g_policy->state == POLICY_RUNNING)) {
					charger_get_online(manager->charger, &online);
					if (online)
						status = POWER_SUPPLY_STATUS_CHARGING;
				}
			}
#endif
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			warm_stop_charge = get_warm_stop_charge_state();
			if(manager->usb_online || otg_value){
				ret = charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
				if (!bat_volt) {
					charger_adc_enable(manager->charger, true);
					ret = charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
				}
				if (ret < 0)
						hq_err("get battery volt error1.\n");
				else
						manager->vbat = bat_volt;
			}else{
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
				if (ret < 0)
						hq_err("get battery volt error.\n");
				else
						manager->vbat = pval.intval/1000;
			}

			if ((status != POWER_SUPPLY_STATUS_DISCHARGING) &&
					(!get_effective_result(manager->main_chg_disable_votable))) {
				if (manager->tbat >= BATTERY_HOT_TEMP)
					status = POWER_SUPPLY_STATUS_NOT_CHARGING;
				else if ((manager->tbat >= BATTERY_WARM_TEMP || warm_stop_charge))
					status = POWER_SUPPLY_STATUS_CHARGING;
				else if ((manager->tbat < BATTERY_WARM_TEMP && manager->vbat < 4100))
					status = POWER_SUPPLY_STATUS_CHARGING;
			}
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
			// if((manager->smart_charge[SMART_CHG_NAVIGATION].active_status) && (status != POWER_SUPPLY_STATUS_CHARGING)){
			// 	status = POWER_SUPPLY_STATUS_CHARGING;
			// }
			if((manager->smart_charge[SMART_CHG_ENDURANCE_PRO].active_status) && (status != POWER_SUPPLY_STATUS_CHARGING)){
				hq_err("smart_chg: endurance is working, set charging.\n");
				status = POWER_SUPPLY_STATUS_CHARGING;
			}
                        else if(status == POWER_SUPPLY_STATUS_CHARGING){
                                ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
                                if (ret < 0)
                                        hq_err("get battery soc error.\n");
                                else
                                        manager->soc = pval.intval;

                                is_full_votable = find_votable("IS_FULL");
                                if (!is_full_votable) {
                                        hq_err("failed to get is_full_votable\n");
                                        return -EINVAL;
                                }
                                is_full_flag = get_effective_result(is_full_votable);
                                if (is_full_flag < 0) {
                                        hq_err("failed to get is_full_flag\n");
                                        return -EINVAL;
                                }
                                if((manager->soc == 100) && is_full_flag){
                                        status = POWER_SUPPLY_STATUS_FULL;
                                        hq_err("new soc is 100, keep report full, status = %d\n", status);
                                }
				if (manager->tbat <= BATTERY_COLD_TEMP)
					status = POWER_SUPPLY_STATUS_NOT_CHARGING;
                        }
#endif

			val->intval = status;
		break;

		case POWER_SUPPLY_PROP_HEALTH:
			ret = get_battery_health(manager);
			if (ret < 0)
				break;
			val->intval = ret;
			break;

		case POWER_SUPPLY_PROP_PRESENT:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_PRESENT, &pval);
			if (ret < 0) {
				hq_err("failed to get online prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			ret = charger_get_chg_status(manager->charger, &state, &status);
			if (ret < 0) {
				hq_err("failed to get chg type prop\n");
				break;
			}
			val->intval = state;
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
			if (!ret) {
#endif
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
				if (ret < 0) {
					hq_err("failed to get capaticy prop\n");
					break;
				}
				val->intval = pval.intval;
				if (pval.intval <= 1) {
					val->intval = 1;
				}
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			} else {
				val->intval = FG_I2C_ERR_SOC;
			}
#endif
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
			if (!ret) {
#endif
				if(manager->usb_online || otg_value){
					ret = charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
					if (!bat_volt) {
						charger_adc_enable(manager->charger, true);
						ret = charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
					}
					if (ret < 0) {
						hq_err("failed to get voltage-now prop\n");
						break;
					}
					val->intval = bat_volt * 1000;
				}else{
					ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
					if (ret < 0)
							hq_err("get battery volt error2.\n");
					else
							val->intval = pval.intval;
				}
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			} else {
				charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
				if (!bat_volt) {
					charger_adc_enable(manager->charger, true);
					charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
				}

				val->intval = bat_volt * 1000;
			}
#endif
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			#if IS_ENABLED(CONFIG_BQ_FUELGAUGE)
			ret = fuel_gauge_get_fastcharge_mode(manager->fuel_gauge);
			if (ret)
				val->intval = FAST_CHG_VOLTAGE_MAX;
			else
				val->intval = NORMAL_CHG_VOLTAGE_MAX;
			#else
			val->intval = FAST_CHG_VOLTAGE_MAX;
			#endif
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			val->intval = manager->system_temp_level;
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
			val->intval = 18;
			break;

		case POWER_SUPPLY_PROP_CURRENT_NOW:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
			if (ret < 0) {
				hq_err("failed to get current_now prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_TEMP:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
			if (ret < 0) {
				hq_err("failed to get temp prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
			break;

		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
			if (ret < 0) {
				hq_err("failed to get cycle_count prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			val->intval = TYPICAL_CAPACITY;
			break;

		case POWER_SUPPLY_PROP_CHARGE_FULL:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
			if (ret < 0) {
				hq_err("failed to get charge_full prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_MODEL_NAME:
			#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
			val->strval = TO_STR(MODEL_NAME(PROJECT_NAME, TYPICAL_CAPACITY_MAH, INPUT_POWER_LIMIT));
			#else
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_MODEL_NAME, &pval);
			if (ret < 0) {
				hq_err("failed to get model_name prop\n");
				break;
			}
			val->strval = pval.strval;
			#endif
			break;

		case POWER_SUPPLY_PROP_CHARGE_COUNTER:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
			if (ret < 0) {
				hq_err("failed to get charge_counter prop\n");
				break;
			}
			val->intval = pval.intval / 1000;		//mAh
			break;

		default:
			break;
	}
	return 0;
}

static int charger_batt_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
			manager->system_temp_level = val->intval;
			hq_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			manager->system_temp_level = val->intval;
			hq_set_prop_system_temp_level(manager, CALL_THERMAL_DAEMON_VOTER);
			break;
		default:
			break;
	}
	return 0;
}

static int batt_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			return 1;
		default:
			break;
	}
	return 0;
}

static enum power_supply_property batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
};

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = batt_props,
	.num_properties = ARRAY_SIZE(batt_props),
	.get_property = charger_batt_get_property,
	.set_property = charger_batt_set_property,
	.property_is_writeable = batt_prop_is_writeable,
};

int charger_manager_batt_psy_register(struct charger_manager *manager)
{
	struct power_supply_config batt_psy_cfg = { .drv_data = manager,};
	if (IS_ERR_OR_NULL(manager)) {
		hq_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	manager->batt_psy = devm_power_supply_register(manager->dev, &batt_psy_desc,
							&batt_psy_cfg);
	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		hq_err("batt psy register failed\n");
		return PTR_ERR(manager->batt_psy);
	}
	hq_info("batt psy register success\n");
	return 0;
}
EXPORT_SYMBOL(charger_manager_batt_psy_register);

static void charger_manager_from_psy(struct device *dev,
					struct power_supply *psy, struct charger_manager **manager)
{
	if (IS_ERR_OR_NULL(dev)) {
		hq_err("dev is_err_or_null\n");
		return;
	}

	psy = dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(psy)) {
		hq_err("psy is_err_or_null\n");
		return;
	}

	*manager = power_supply_get_drvdata(psy);
	if (IS_ERR_OR_NULL(*manager)) {
		hq_err("manager is_err_or_null\n");
		return;
	}
}
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
void xm_uevent_report(struct charger_manager *manager);
#endif
static ssize_t real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	int ret;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->charger)) {
		hq_err("%s:manager->charger is_err_or_null\n");
		goto out;
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		hq_err("Couldn't get usb type ret=%d\n", ret);
		goto out;
	}

out:
	if (manager->pd_active)
		vbus_type = VBUS_TYPE_PD;
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	xm_uevent_report(manager);
#endif
	return sprintf(buf, "%s\n", real_type_txt[vbus_type]);
}

static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

static ssize_t typec_cc_orientation_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	bool usb_online = false;
	bool otg_value = false;


	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	charger_get_otg_status(manager->charger, &otg_value);
	charger_get_online(manager->charger, &usb_online);

	if (usb_online == false && otg_value == false)
		return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
	else if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->tcpc->typec_polarity + 1);
}
static struct device_attribute typec_cc_orientation_attr =
	__ATTR(typec_cc_orientation, 0644, typec_cc_orientation_show, NULL);

static ssize_t usb_otg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	bool otg_value = false;
	int ret;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	ret = charger_get_otg_status(manager->charger, &otg_value);
	if (ret < 0)
		hq_err("can not get otg status\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", otg_value);
}
static struct device_attribute usb_otg_attr =
	__ATTR(usb_otg, 0644, usb_otg_show, NULL);

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
static int get_apdo_max(struct charger_manager *manager) {
	int apdo_max = 0;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	if (manager->pd_active != CHARGE_PD_PPS_ACTIVE) {
		goto done;
	}

	apdo_max = g_policy->cap.volt_max[g_policy->cap_nr] *
		g_policy->cap.curr_max[g_policy->cap_nr] / 1000000;

done:
	return apdo_max;

}

static ssize_t power_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	manager->apdo_max = get_apdo_max(manager);

	if (manager->apdo_max >= INPUT_POWER_OVER_33W)
		manager->apdo_max = INPUT_POWER_LIMIT;

	hq_info("apdo_max = %d\n", manager->apdo_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->apdo_max);

}
static struct device_attribute power_max_attr =
	__ATTR(power_max, 0644, power_max_show, NULL);

static int quick_charge_type(struct charger_manager *manager)
{
	enum quick_charge_type quick_charge_type = QUICK_CHARGE_NORMAL;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	union power_supply_propval pval = {0, };

	bool usbpd_verifed = false;
	int ret = 0;
	int i = 0;

	if (IS_ERR_OR_NULL(manager->charger)) {
		hq_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		hq_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->batt_psy);
	}

	if (IS_ERR_OR_NULL(manager->usb_psy)) {
		hq_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->usb_psy);
	}

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	if (IS_ERR_OR_NULL(manager->pd_adapter)) {
		hq_err("manager->pd_adapter is_err_or_null\n");
		return PTR_ERR(manager->pd_adapter);
	}

	ret = adapter_get_usbpd_verifed(manager->pd_adapter, &usbpd_verifed);
	if (ret < 0){
		hq_err("Couldn't get usbpd verifed ret=%d\n", ret);
		return ret;
	}
#endif

	ret = power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		hq_err("Couldn't get usb online ret=%d\n", ret);
		return -EINVAL;
	}

	if (!(pval.intval))
		return -EINVAL;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		hq_err("Couldn't get bat temp ret=%d\n", ret);
		return -EINVAL;
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		hq_err("Couldn't get usb type ret=%d\n", ret);
		return ret;
	}

	while (quick_charge_map[i].adap_type != 0) {
		if (vbus_type == quick_charge_map[i].adap_type) {
			quick_charge_type = quick_charge_map[i].adap_cap;
		}
		i++;
	}

	if(manager->pd_active)
		quick_charge_type = QUICK_CHARGE_FAST;
	if(usbpd_verifed) {
		if (get_apdo_max(manager) >= SUPER_CHARGE_POWER)
			quick_charge_type = QUICK_CHARGE_TURBE;
		else
			quick_charge_type = QUICK_CHARGE_TURBE;
	}
	if(pval.intval >= BATTERY_WARM_TEMP){
		quick_charge_type = QUICK_CHARGE_NORMAL;
	}

	hq_debug("quick_charge_type = %d\n", quick_charge_type);

	return quick_charge_type;
}

static ssize_t quick_charge_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	struct power_supply *usb_psy = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return scnprintf(buf, PAGE_SIZE, "%d\n", quick_charge_type(manager));
}

static struct device_attribute quick_charge_type_attr =
	__ATTR(quick_charge_type, 0644, quick_charge_type_show, NULL);
#endif

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};
static ssize_t typec_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if(IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			hq_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n",usb_typec_mode_text[manager->tcpc->typec_mode]);
}

static struct device_attribute typec_mode_attr = __ATTR(typec_mode,0644,typec_mode_show,NULL);

static ssize_t mtbf_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;

	hq_info("mtbf_mode_store start\n");

	if (kstrtoint(buf, 10, &val)) {
		hq_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val != 0) {
		is_mtbf_mode = 1;
		hq_info("is_mtbf_mode = 1\n");
	} else {
		is_mtbf_mode = 0;
		hq_info("is_mtbf_mode = 0\n");
	}
	return count;
}

static ssize_t mtbf_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_mtbf_mode);
}

static struct device_attribute mtbf_mode_attr = __ATTR(mtbf_mode,0644,mtbf_mode_show,mtbf_mode_store);

bool is_mtbf_mode_func(void)
{
	return is_mtbf_mode;
}
EXPORT_SYMBOL_GPL(is_mtbf_mode_func);

static struct attribute *usb_psy_attrs[] = {
	&real_type_attr.attr,
	&typec_cc_orientation_attr.attr,
	&usb_otg_attr.attr,
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	&power_max_attr.attr,
	&quick_charge_type_attr.attr,
#endif
	&typec_mode_attr.attr,
	&mtbf_mode_attr.attr,
	NULL,
};

static const struct attribute_group usb_psy_attrs_group = {
	.attrs = usb_psy_attrs,
};
int hq_usb_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->usb_psy->dev.kobj,
								&usb_psy_attrs_group);
}
EXPORT_SYMBOL(hq_usb_sysfs_create_group);

static ssize_t input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->input_suspend);
}
static ssize_t input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val;

	hq_info("input_suspend_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->main_chg_disable_votable)) {
		hq_info("main_chg_disable_votable not found\n");
		return PTR_ERR(manager->main_chg_disable_votable);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->input_suspend = val;
	if (manager->input_suspend) {
		vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, true, 1);
	} else {
		vote(manager->main_chg_disable_votable, FACTORY_KIT_VOTER, false, 0);
	}
	hq_info("manager->input_suspend = %d\n", manager->input_suspend);
	return count;
}
static struct device_attribute input_suspend_attr =
	__ATTR(input_suspend, 0644, input_suspend_show, input_suspend_store);

static ssize_t shipmode_count_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->shippingmode);
}

static ssize_t shipmode_count_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->shippingmode = val;
	hq_info("shippingmode = %d\n", manager->shippingmode);
	manager->charger->shipmode_flag = manager->shippingmode;

	return count;
}
static struct device_attribute shipmode_count_reset_attr =
	__ATTR(shipmode_count_reset, 0644, shipmode_count_reset_show, shipmode_count_reset_store);

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
static ssize_t soc_decimal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int soc_decimal = 0;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("%s:manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	soc_decimal = fuel_gauge_get_soc_decimal(manager->fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", soc_decimal);

}
static struct device_attribute soc_decimal_attr =
	__ATTR(soc_decimal, 0644, soc_decimal_show, NULL);

static ssize_t soc_decimal_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int soc_decimal_rate = 0;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("%s:manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	soc_decimal_rate = fuel_gauge_get_soc_decimal_rate(manager->fuel_gauge);
	if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
		soc_decimal_rate = 0;

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", soc_decimal_rate);
}
static struct device_attribute soc_decimal_rate_attr =
	__ATTR(soc_decimal_rate, 0644, soc_decimal_rate_show, NULL);

void xm_uevent_report(struct charger_manager *manager)
{
	int soc_decimal_rate = 0;
	int soc_decimal = 0;

	char quick_charge_string[64];
	char soc_decimal_string[64];
	char soc_decimal_string_rate[64];

	char *envp[] = {
		quick_charge_string,
		soc_decimal_string,
		soc_decimal_string_rate,
		NULL,
	};

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			hq_err("%s:manager->fuel_gauge is_err_or_null\n");
			return;
		}
	}

	sprintf(quick_charge_string, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", quick_charge_type(manager));

	soc_decimal = fuel_gauge_get_soc_decimal(manager->fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;
	sprintf(soc_decimal_string, "POWER_SUPPLY_SOC_DECIMAL=%d", soc_decimal);

	soc_decimal_rate = fuel_gauge_get_soc_decimal_rate(manager->fuel_gauge);
	if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
		soc_decimal_rate = 0;
	sprintf(soc_decimal_string_rate, "POWER_SUPPLY_SOC_DECIMAL_RATE=%d", soc_decimal_rate);

	kobject_uevent_env(&manager->dev->kobj, KOBJ_CHANGE, envp);

	hq_debug("envp[0]:%s envp[1]:%s envp[2]:%s",envp[0],envp[1],envp[2]);
}
EXPORT_SYMBOL(xm_uevent_report);
#endif

#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
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
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	if (kstrtoint(buf, 16, &val))
			return -EINVAL;

	en_ret = val & 0x1;
	func_type = (val & 0xFFFE) >> 1;
	func_val = val >> 16;

	hq_info("get val:%#X, func_type:%#X, en_ret:%d, func_val:%d\n",
			val, func_type, en_ret, func_val);

	bit_pos = find_first_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM);

   if(bit_pos == SMART_CHG_FEATURE_MAX_NUM || find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1) != SMART_CHG_FEATURE_MAX_NUM){
           hq_info("ERROR: zero or more than one func type!\n");
           hq_info("find_next_bit = %d, bit_pos = %d\n",
                   find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1), bit_pos);
           set_error(manager);
   } else
           set_success(manager);

  // if func_type bit0 is 1, bit_pos = 0, not 1. so ++bit_pos.
   if(!smart_chg_is_error(manager))
           handle_smart_chg_functype(manager, ++bit_pos, en_ret, func_val);

   /* update smart_chg[0] status */
   all_func_status = handle_smart_chg_functype_status(manager);
   manager->smart_chg_cmd = all_func_status;
   manager->smart_charge[SMART_CHG_STATUS_FLAG].en_ret = all_func_status & 0x1;
   manager->smart_charge[SMART_CHG_STATUS_FLAG].active_status = (all_func_status & 0xFFFE) >> 1;

   return count;
}

static ssize_t smart_chg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	//int *val = 0;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->smart_chg_cmd);
}

static struct device_attribute smart_chg_attr =
		__ATTR(smart_chg, 0644, smart_chg_show, smart_chg_store);

static ssize_t smart_batt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
        int val = 0;
        struct votable	*fv_votable = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
			return -EINVAL;

        manager->smart_batt = val;
        hq_info("smart_batt = %d\n", manager->smart_batt);
        fv_votable = find_votable("MAIN_FV");
        if (!fv_votable) {
                hq_info("failed to get fv_votable\n");
        }else{
                rerun_election(fv_votable);
        }
        return count;
}

static ssize_t smart_batt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->smart_batt);
}

static struct device_attribute smart_batt_attr =
		__ATTR(smart_batt, 0644, smart_batt_show, smart_batt_store);

static ssize_t night_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
        bool val;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtobool(buf, &val))
		return -EINVAL;

        manager->night_charging = val;

        return count;
}

static ssize_t night_charging_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->night_charging);
}

static struct device_attribute night_charging_attr =
		__ATTR(night_charging, 0644, night_charging_show, night_charging_store);
#endif

static struct attribute *batt_psy_attrs[] = {
	&input_suspend_attr.attr,
	&shipmode_count_reset_attr.attr,
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	&soc_decimal_attr.attr,
	&soc_decimal_rate_attr.attr,
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	&smart_chg_attr.attr,
	&smart_batt_attr.attr,
	&night_charging_attr.attr,
#endif
	NULL,
};

static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};

int hq_batt_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->batt_psy->dev.kobj,
								&batt_psy_attrs_group);
}
EXPORT_SYMBOL(hq_batt_sysfs_create_group);

MODULE_DESCRIPTION("Huaqin Charger sysfs");
MODULE_LICENSE("GPL v2");
