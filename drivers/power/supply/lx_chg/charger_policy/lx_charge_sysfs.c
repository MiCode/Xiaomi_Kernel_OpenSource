// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019 The Linux Foundation. All rights reserved.
 */

#include "lx_charger_manager.h"
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "../common/lx_voter.h"
#endif
#include "../lx_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG][CM]"
#endif
#include "../fuelgauge/bq28z610.h"
#include "../fuelgauge/charger_partition.h"
#include <linux/thermal.h>
//cqr vbat ctrol start

#define CQR_VBAT_MIM       3750000
#define CQR_VBAT_MAX       4300000
#define CQR_READ_VBAT_TIMES 5
#define CQR_VBAT_OUT_RANGE_CNT 2

//cqr vbat ctrol end
static enum power_supply_usb_type charge_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_UNKNOWN
};
enum {
    SEAL_STATE_RSVED,
    SEAL_STATE_FA,
    SEAL_STATE_UNSEALED,
    SEAL_STATE_SEALED,
};
static int charger_usb_get_property(struct power_supply *psy,
						enum power_supply_property psp,
						union power_supply_propval *val)
{
	struct charger_manager *manager = power_supply_get_drvdata(psy);
	enum vbus_type vbus_type;
	int ret = 0;
	int volt = 0;
	int curr = 0;
	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->charger)) {
		lx_err("manager charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = charger_get_vbus_type(manager->charger, &vbus_type);
		if (ret < 0)
			lx_err("Couldn't get usb type ret=%d\n", ret);
		if (vbus_type == VBUS_TYPE_SDP)
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
		else if(vbus_type == VBUS_TYPE_CDP)
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
		else if(vbus_type >= VBUS_TYPE_DCP && vbus_type <= VBUS_TYPE_HVDCP_3P5)
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
		else
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		ret = charger_get_vbus_type(manager->charger, &vbus_type);
		if (ret < 0)
			lx_err("Couldn't get usb type ret=%d\n", ret);
		if (vbus_type == VBUS_TYPE_SDP || vbus_type == VBUS_TYPE_CDP)
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		if (manager->otg_status) {
			val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = manager->charger_online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = VOLTAGE_MAX;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = charger_get_adc(manager->charger, ADC_GET_VBUS, &volt);
		if (ret < 0) {
			lx_err("Couldn't read input volt ret=%d\n", ret);
			return ret;
		}
		val->intval = volt * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = charger_manager_get_current(manager, &curr);
		if (ret < 0) {
			lx_err("Couldn't read input curr ret=%d\n", ret);
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
		lx_err("manager is_err_or_null\n");
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
	POWER_SUPPLY_PROP_USB_TYPE,
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
	.usb_types  = charge_usb_types,
	.num_usb_types = ARRAY_SIZE(charge_usb_types),
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
		lx_err("usb psy register failed\n");
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
		lx_err("failed to get temp prop\n");
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
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
	int  bat_volt = 0;
#endif
	int ret = 0;

	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lx_err("manager->fuel_gauge is_err_or_null\n");
		}
	}

	if (manager->fg_psy == NULL)
		manager->fg_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(manager->fg_psy)) {
		lx_err("failed to get bms psy\n");
		return PTR_ERR(manager->fg_psy);
	}

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = manager->chg_status;
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
				lx_err("failed to get online prop\n");
				break;
			}
			val->intval = pval.intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			val->intval = manager->chg_state;
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
			if (!ret) {
#endif
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
				if (ret < 0) {
					lx_err("failed to get capaticy prop\n");
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
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
				if (ret < 0) {
					lx_err("failed to get voltage-now prop\n");
					break;
				}
				val->intval = pval.intval;
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			} else {
				charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
				if (!bat_volt) {
					//charger_adc_enable(manager->charger, true);
					charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
				}
				if (!bat_volt) {
					bat_volt =3995;
					lx_err("failed to get voltage-now set 3.995V\n");
				}
				lx_debug("fg i2c err, ADC_GET_VBAT voltage-now\n");
				val->intval = bat_volt * 1000;
			}
#endif
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			ret = fuel_gauge_get_fastcharge_mode(manager->fuel_gauge);
			if (ret)
				val->intval = FAST_CHG_VOLTAGE_MAX;
			else
				val->intval = NORMAL_CHG_VOLTAGE_MAX;
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			val->intval = manager->system_temp_level;
			break;

		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
			val->intval = 20;
			break;

		case POWER_SUPPLY_PROP_CURRENT_NOW:
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			ret = fuel_gauge_check_i2c_function(manager->fuel_gauge);
			if (!ret) {
#endif
				ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
				if (ret < 0) {
					lx_err("failed to get current_now prop\n");
					break;
				}
				val->intval = pval.intval;
#if IS_ENABLED(CONFIG_XM_FG_I2C_ERR)
			} else {
				val->intval = FG_I2C_ERR_CURRENT_NOW;
			}
#endif
			break;

		case POWER_SUPPLY_PROP_TEMP:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
			if (ret < 0) {
				lx_err("failed to get temp prop\n");
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
				lx_err("failed to get cycle_count prop\n");
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
				lx_err("failed to get charge_full prop\n");
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
				lx_err("failed to get model_name prop\n");
				break;
			}
			val->strval = pval.strval;
			#endif
			break;

		case POWER_SUPPLY_PROP_CHARGE_COUNTER:
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
			if (ret < 0) {
				lx_err("failed to get charge_counter prop\n");
				break;
			}
			val->intval = pval.intval;
			break;
		case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		#ifdef FACTORY_BUILD
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		#else
			ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CAPACITY_LEVEL, &pval);
			if (ret < 0) {
				//lx_err("failed to get charge_counter prop\n");
				val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
				break;
			}
			val->intval = pval.intval;
			if(val->intval == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL){
				lx_err("soc capacity level critical true\n");
			}
		#endif
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
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	switch (prop) {
		case POWER_SUPPLY_PROP_STATUS:
			smblib_set_prop_batt_status(manager, val);
			break;
		case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
			manager->system_temp_level = val->intval;
			lx_set_prop_system_temp_level(manager, TEMP_THERMAL_DAEMON_VOTER);
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			manager->system_temp_level = val->intval;
			lx_set_prop_system_temp_level(manager, CALL_THERMAL_DAEMON_VOTER);
			break;
		default:
			break;
	}
	return 0;
}

static int batt_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
		case POWER_SUPPLY_PROP_STATUS:
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
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
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
static int psy_register_cooler(struct power_supply *psy);
int charger_manager_batt_psy_register(struct charger_manager *manager)
{
	struct power_supply_config batt_psy_cfg = { .drv_data = manager,};
	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	manager->batt_psy = devm_power_supply_register(manager->dev, &batt_psy_desc,
							&batt_psy_cfg);
	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		lx_err("batt psy register failed\n");
		return PTR_ERR(manager->batt_psy);
	}
	lx_info("batt psy register success\n");
	psy_register_cooler(manager->batt_psy);
	return 0;
}
EXPORT_SYMBOL(charger_manager_batt_psy_register);

static void charger_manager_from_psy(struct device *dev,
					struct power_supply *psy, struct charger_manager **manager)
{
	if (IS_ERR_OR_NULL(dev)) {
		lx_err("dev is_err_or_null\n");
		return;
	}

	psy = dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(psy)) {
		lx_err("psy is_err_or_null\n");
		return;
	}

	*manager = power_supply_get_drvdata(psy);
	if (IS_ERR_OR_NULL(*manager)) {
		lx_err("manager is_err_or_null\n");
		return;
	}
}
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
void xm_uevent_report(struct charger_manager *manager);
#endif

#ifdef FACTORY_BUILD
static ssize_t ato_soc_user_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->ato_soc_user_control);
}

static ssize_t ato_soc_user_control_store(struct device *dev,
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

	manager->ato_soc_user_control = val;
	lx_info("ato_soc_user_control = %d\n", manager->ato_soc_user_control);

	return count;
}
static struct device_attribute ato_soc_user_control_attr =
	__ATTR(ato_soc_user_control, 0644, ato_soc_user_control_show, ato_soc_user_control_store);
#endif

static ssize_t real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	int ret;
	bool usbpd_verifed = false;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->charger)) {
		lx_err("%s:manager->charger is_err_or_null\n");
		goto out;
	}

	ret = adapter_get_usbpd_verifed(manager->pd_adapter, &usbpd_verifed);
	if (ret < 0){
		lx_err("Couldn't get usbpd verifed ret=%d\n", ret);
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		lx_err("Couldn't get usb type ret=%d\n", ret);
		goto out;
	}

        if (usbpd_verifed && manager->pd_active == CHARGE_PD_PPS_ACTIVE)
		vbus_type = VBUS_TYPE_MI_PPS;

out:
	if (manager->pd_active == CHARGE_PD_ACTIVE) {
		vbus_type = VBUS_TYPE_PD;
	} else if (manager->pd_active == CHARGE_PD_PPS_ACTIVE) {
		vbus_type = VBUS_TYPE_PPS;
		if (usbpd_verifed)
			vbus_type = VBUS_TYPE_MI_PPS;
	}

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	xm_uevent_report(manager);
#endif
	if (manager->otg_status) {
		vbus_type = VBUS_TYPE_UNKNOW;
	}

	lx_debug("real_type = %d\n", vbus_type);

	return sprintf(buf, "%s\n", real_type_txt[vbus_type]);
}

static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

static ssize_t pd_verifed_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	int ret;
	bool usbpd_verifed = false;

	lx_info("pd_verifed_show start\n");

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	ret = adapter_get_usbpd_verifed(manager->pd_adapter, &usbpd_verifed);
	if (ret < 0){
		lx_err("Couldn't get usbpd verifed ret=%d\n", ret);
	}

	return sprintf(buf, "%d\n", usbpd_verifed);
}

static struct device_attribute pd_verifed_attr =
	__ATTR(pd_verifed, 0644, pd_verifed_show, NULL);

static ssize_t is_cp_enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	int ret;
	bool is_cp_enabled = false;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	ret = chargerpump_get_is_enable(manager->master_cp_chg, &is_cp_enabled);
	if (ret < 0){
		lx_err("Couldn't get is_cp_enabled ret=%d\n", ret);
	}

	lx_info("cp_enabled = %d\n", is_cp_enabled);

	return sprintf(buf, "%d\n", is_cp_enabled);
}

static struct device_attribute is_cp_enabled_attr =
	__ATTR(is_cp_enabled, 0644, is_cp_enabled_show, NULL);

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
			lx_err("manager->tcpc is_err_or_null\n");
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
		lx_err("can not get otg status\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", otg_value);
}
static struct device_attribute usb_otg_attr =
	__ATTR(usb_otg, 0644, usb_otg_show, NULL);

static ssize_t disable_chg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "manager is err or null !!\n", 0);
	else
		return sprintf(buf, "%d\n", manager->disable_chg);
}

static ssize_t disable_chg_store(struct device *dev,
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

	manager->disable_chg = val;
	if (val) {
		disable_chg_comm_ctrl(manager, DISABLE_CHARGE_VOTER, true);
	} else {
		disable_chg_comm_ctrl(manager, DISABLE_CHARGE_VOTER, false);
	}
	lx_info("charge_disable = %d\n", val);
	return count;
}
static struct device_attribute disable_chg_attr =
	__ATTR(disable_chg, 0644, disable_chg_show, disable_chg_store);

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
static int get_apdo_max(struct charger_manager *manager) {
	struct adapter_dev *adapter = NULL;
	int apdo_max = 0;
	int ret =0;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	if (manager->pd_active != CHARGE_PD_PPS_ACTIVE) {
		goto done;
	}

	adapter = adapter_find_dev_by_name("pd_adapter1");
	if (adapter == NULL || g_policy == NULL )
		goto done;

	ret = adapter_get_cap(adapter, &g_policy->cap);
		if (ret < 0) {
			lx_info("adapter get cap failed\n");
			goto done;
		}
	chargerpump_policy_check_adapter_cap(g_policy, &g_policy->cap);

	apdo_max = g_policy->cap.volt_max[g_policy->cap_nr] *
		g_policy->cap.curr_max[g_policy->cap_nr] / 1000000;

done:
	return apdo_max;
}

static ssize_t apdo_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	manager->apdo_max = get_apdo_max(manager);

	lx_info("apdo_max = %d\n", manager->apdo_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->apdo_max);
}
static struct device_attribute apdo_max_attr =
	__ATTR(apdo_max, 0644, apdo_max_show, NULL);

static int get_power_max(struct charger_manager *manager) {
	struct adapter_dev *adapter = NULL;
	int power_max = 0;
	int ret =0;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	if (manager->pd_active != CHARGE_PD_PPS_ACTIVE) {
		goto done;
	}

	adapter = adapter_find_dev_by_name("pd_adapter1");
	if (adapter == NULL || g_policy == NULL )
		goto done;

	ret = adapter_get_cap(adapter, &g_policy->cap);
		if (ret < 0) {
			lx_info("adapter get cap failed\n");
			goto done;
		}
	chargerpump_policy_check_adapter_cap(g_policy, &g_policy->cap);

	power_max = g_policy->cap.volt_max[g_policy->cap_nr] *
		g_policy->cap.curr_max[g_policy->cap_nr] / 1000000;

done:
	return power_max;
}

static ssize_t power_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;
	int power_w;
	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	power_w = get_power_max(manager);
	if( power_w >= 33 ){
		manager->power_max = 45;
	}else{
	  	manager->power_max = power_w;
    }

	lx_info("power_max = %d\n", manager->power_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->power_max);
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
		lx_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->charger);
	}

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		lx_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->batt_psy);
	}

	if (IS_ERR_OR_NULL(manager->usb_psy)) {
		lx_err("manager->charger is_err_or_null\n");
		return PTR_ERR(manager->usb_psy);
	}

#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	if (IS_ERR_OR_NULL(manager->pd_adapter)) {
		lx_err("manager->pd_adapter is_err_or_null\n");
		return PTR_ERR(manager->pd_adapter);
	}

	ret = adapter_get_usbpd_verifed(manager->pd_adapter, &usbpd_verifed);
	if (ret < 0){
		lx_err("Couldn't get usbpd verifed ret=%d\n", ret);
		return ret;
	}
#endif

	ret = power_supply_get_property(manager->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		lx_err("Couldn't get usb online ret=%d\n", ret);
		return -EINVAL;
	}

	if (!(pval.intval))
		return -EINVAL;

	ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		lx_err("Couldn't get bat temp ret=%d\n", ret);
		return -EINVAL;
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		lx_err("Couldn't get usb type ret=%d\n", ret);
		return ret;
	}

	while (quick_charge_map[i].adap_type != 0) {
		if (vbus_type == quick_charge_map[i].adap_type) {
			quick_charge_type = quick_charge_map[i].adap_cap;
		}
		i++;
	}

	if (manager->pd_active)
		quick_charge_type = QUICK_CHARGE_FAST;

	if (manager->pd_active == CHARGE_PD_PPS_ACTIVE)
		quick_charge_type = QUICK_CHARGE_TURBE;

	if (pval.intval >= BATTERY_WARM_TEMP || pval.intval <= BATTERY_COLD_TEMP) {
		quick_charge_type = QUICK_CHARGE_NORMAL;
	}

	lx_debug("quick_charge_type = %d\n", quick_charge_type);

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
			lx_err("manager->tcpc is_err_or_null\n");
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

	lx_info("mtbf_mode_store start\n");

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val != 0) {
		is_mtbf_mode = 1;
		lx_info("is_mtbf_mode = 1\n");
	} else {
		is_mtbf_mode = 0;
		lx_info("is_mtbf_mode = 0\n");
	}
	return count;
}
static int ps_get_max_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long *state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	psy = tcd->devdata;
	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, &val);
	if (ret)
		return ret;
	*state = val.intval;
	return ret;
}

static int ps_get_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long *state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	psy = tcd->devdata;
	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);
	if (ret)
		return ret;
	*state = val.intval;
	return ret;
}
static int ps_set_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long state)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	psy = tcd->devdata;
	val.intval = state;
	ret = charger_batt_set_property(psy,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);
	return ret;
}
static const struct thermal_cooling_device_ops psy_tcd_ops = {
	.get_max_state = ps_get_max_charge_cntl_limit,
	.get_cur_state = ps_get_cur_charge_cntl_limit,
	.set_cur_state = ps_set_cur_charge_cntl_limit,
};
static int psy_register_cooler(struct power_supply *psy)
{
	int i;
	for (i = 0; i < psy->desc->num_properties; i++) {
		if (psy->desc->properties[i] ==
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT) {
			if (psy->dev.parent)
				psy->tcd = thermal_of_cooling_device_register(
						dev_of_node(psy->dev.parent),
						(char *)psy->desc->name,
						psy, &psy_tcd_ops);
			else
				psy->tcd = thermal_cooling_device_register(
						(char *)psy->desc->name,
						psy, &psy_tcd_ops);
			return PTR_ERR_OR_ZERO(psy->tcd);
		}
	}
	return 0;
}
int psy_unregister_cooler(struct charger_manager *manager)
{
	if (IS_ERR_OR_NULL(manager)) {
		lx_err("manager is_err_or_null\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
	lx_err("batt psy register failed\n");
		return PTR_ERR(manager->batt_psy);
	}

	if (IS_ERR_OR_NULL(manager->batt_psy->tcd)){
		return -1;
        }
	thermal_cooling_device_unregister(manager->batt_psy->tcd);
      return 0;
}
EXPORT_SYMBOL(psy_unregister_cooler);

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

static ssize_t audio_usb_cctoggle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	static bool last_audio_toggle = 0;
	struct power_supply *usb_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->audio_usb_cctoggle = !!val;

	lx_info("audio_usb_cctoggle = %d, cid_status =%d, typec_attach = %d, soft_cid_start = %d\n",
		manager->audio_usb_cctoggle, manager->cid_status, manager->typec_attach, manager->soft_cid_start);

	if (manager->soft_cid_start && manager->audio_usb_cctoggle != last_audio_toggle
		&& manager->cid_status && !manager->typec_attach)
		schedule_delayed_work(&manager->soft_cid_work, 0);

	last_audio_toggle = manager->audio_usb_cctoggle;
	return count;
}

static ssize_t audio_usb_cctoggle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charger_manager *manager = NULL;
	struct power_supply *usb_psy = NULL;

	charger_manager_from_psy(dev, usb_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->audio_usb_cctoggle);
}

static struct device_attribute audio_usb_cctoggle_attr =
	__ATTR(audio_usb_cctoggle, 0644, audio_usb_cctoggle_show, audio_usb_cctoggle_store);

static struct attribute *usb_psy_attrs[] = {
	&real_type_attr.attr,
	&pd_verifed_attr.attr,
	&is_cp_enabled_attr.attr,
	&typec_cc_orientation_attr.attr,
	&usb_otg_attr.attr,
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	&apdo_max_attr.attr,
	&quick_charge_type_attr.attr,
	&power_max_attr.attr,
#endif
	&typec_mode_attr.attr,
	&mtbf_mode_attr.attr,
	&audio_usb_cctoggle_attr.attr,
	NULL,
};

static const struct attribute_group usb_psy_attrs_group = {
	.attrs = usb_psy_attrs,
};
int lx_usb_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->usb_psy->dev.kobj,
								&usb_psy_attrs_group);
}
EXPORT_SYMBOL(lx_usb_sysfs_create_group);

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
	struct bq_fg_chip *bp = NULL;
	int val;

	lx_info("input_suspend_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bp = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bp))
		return PTR_ERR(bp);


	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bp->input_suspend = val;
	manager->input_suspend = val;
	if (manager->input_suspend) {
		vote(manager->main_icl_votable, FACTORY_KIT_VOTER, true, 0);
	} else {
		vote(manager->main_icl_votable, FACTORY_KIT_VOTER, false, 0);
	}
	lx_info("manager->input_suspend = %d\n", manager->input_suspend);
	return count;
}
static struct device_attribute input_suspend_attr =
	__ATTR(input_suspend, 0644, input_suspend_show, input_suspend_store);

#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
static ssize_t lpd_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->lpd_control);
}
static ssize_t lpd_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val;

	lx_info("lpd_control_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->lpd_control = val;
	lx_info("manager->lpd_control = %d\n", manager->lpd_control);
	return count;
}
static struct device_attribute lpd_control_attr =
	__ATTR(lpd_control, 0644, lpd_control_show, lpd_control_store);

static ssize_t lpd_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->lpd_status);
}
static ssize_t lpd_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val;

	lx_info("lpd_status_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->lpd_status = val;
	lx_info("manager->lpd_status = %d\n", manager->lpd_status);
	return count;
}
static struct device_attribute lpd_status_attr =
	__ATTR(lpd_status, 0644, lpd_status_show, lpd_status_store);

static ssize_t decrease_volt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->decrease_volt);
}

static ssize_t decrease_volt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val;

	lx_info("decrease_volt_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->decrease_volt = val;
	lx_info("manager->decrease_volt = %d\n", manager->decrease_volt);
	return count;
}
static struct device_attribute decrease_volt_attr =
	__ATTR(decrease_volt, 0644, decrease_volt_show, decrease_volt_store);
#endif
#endif

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
	union power_supply_propval volt;
	int shipmode_cnt = 0;
	int i = 0;
	int ret;
	int val;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	batt_psy = dev_get_drvdata(dev);
	if (IS_ERR_OR_NULL(batt_psy)) {
		lx_err("batt_psy is_err_or_null\n");
		return PTR_ERR(batt_psy);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->shippingmode = val;

//cqr vbat ctrol start

	for (i = 0; i < CQR_READ_VBAT_TIMES; i++) {
			ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &volt);
			if (ret) {
				lx_err("get vbat fail\n");
				continue;
			}

			msleep(1);

			if (volt.intval < CQR_VBAT_MIM || volt.intval > CQR_VBAT_MAX) {
				shipmode_cnt++;
				lx_info("%s %d %d %d %d %d\n",
					__func__, manager->shippingmode, volt.intval, CQR_VBAT_MAX, CQR_VBAT_MIM, shipmode_cnt);
			}
		}

	if (shipmode_cnt >= CQR_VBAT_OUT_RANGE_CNT) {
		manager->shippingmode = false;
		lx_info("%s after %d\n", __func__, manager->shippingmode);
		ret = charger_shipmode_count_reset(manager->charger, manager->shippingmode);
		if (ret < 0)
			lx_err("can not set shipmode\n");
		return -EINVAL;
	}
//cqr vbat ctrol end

	ret = charger_shipmode_count_reset(manager->charger, manager->shippingmode);
	if (ret < 0)
		lx_err("can not set shipmode\n");

	lx_info("shippingmode = %d\n", manager->shippingmode);

	return count;
}
static struct device_attribute shipmode_count_reset_attr =
	__ATTR(shipmode_count_reset, 0644, shipmode_count_reset_show, shipmode_count_reset_store);

static ssize_t reverse_quick_charge_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->reverse_quick_charge);
}

static ssize_t reverse_quick_charge_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	lx_info("reverse_quick_charge = %d\n", val);
	manager->reverse_quick_charge = val;
	manager->reverse_disconnect = val;
	if (val) {
		schedule_delayed_work(&manager->reverse_quick_charge_work, 0);
	}

	return count;
}
static struct device_attribute reverse_quick_charge_attr =
	__ATTR(reverse_quick_charge, 0660, reverse_quick_charge_show, reverse_quick_charge_store);

static ssize_t revchg_bcl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->revchg_bcl);
}

static ssize_t revchg_bcl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	lx_info("revchg_bcl = %d\n", val);
	manager->revchg_bcl = val;

	return count;
}

static struct device_attribute revchg_bcl_attr =
	__ATTR(revchg_bcl, 0664, revchg_bcl_show, revchg_bcl_store);

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
		lx_err("manager->fuel_gauge is_err_or_null\n");
		soc_decimal = 0;
		goto out;
	}

	soc_decimal = fuel_gauge_get_soc_decimal(manager->fuel_gauge);
	if (soc_decimal < 0)
		soc_decimal = 0;

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", soc_decimal);

}
static struct device_attribute soc_decimal_attr =
	__ATTR(soc_decimal, 0644, soc_decimal_show, NULL);

static ssize_t mtbf_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", is_mtbf_mode);
}

static ssize_t mtbf_current_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bp = NULL;
	int val;

	lx_info("mtbf_mode_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bp = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bp))
		return PTR_ERR(bp);

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	bp->mtbf_current = val;

	if (val != 0) {
		is_mtbf_mode = 1;
		lx_info("is_mtbf_mode = 1\n");
	} else {
		is_mtbf_mode = 0;
		lx_info("is_mtbf_mode = 0\n");
	}
	return count;
}
static struct device_attribute mtbf_current_attr =
	__ATTR(mtbf_current, 0644, mtbf_current_show, mtbf_current_store);

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
		lx_err("%s:manager->fuel_gauge is_err_or_null\n");
		goto out;
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

	lx_debug("envp[0]:%s envp[1]:%s envp[2]:%s",envp[0],envp[1],envp[2]);
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

	lx_info("buf = %s\n", buf);
	if (buf[0] == '0') {
		if (kstrtoint(buf, 16, &val))
			return -EINVAL;
	} else {
		if (kstrtoint(buf, 10, &val))
			return -EINVAL;
	}

	en_ret = val & 0x1;
	func_type = (val & 0xFFFE) >> 1;
	func_val = val >> 16;

	lx_info("get val:%#X, func_type:%#X, en_ret:%d, func_val:%d\n",
			val, func_type, en_ret, func_val);

	bit_pos = find_first_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM);

   if(bit_pos == SMART_CHG_FEATURE_MAX_NUM || find_next_bit(&func_type, SMART_CHG_FEATURE_MAX_NUM , bit_pos + 1) != SMART_CHG_FEATURE_MAX_NUM){
           lx_info("ERROR: zero or more than one func type!\n");
           lx_info("find_next_bit = %d, bit_pos = %d\n",
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
        lx_info("smart_batt = %d\n", manager->smart_batt);
        fv_votable = find_votable("MAIN_FV");
        if (!fv_votable) {
                lx_info("failed to get fv_votable\n");
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

static ssize_t smart_fv_store(struct device *dev,
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

        manager->smart_fv = val;
        lx_info("smart_fv = %d\n", manager->smart_fv);
        fv_votable = find_votable("MAIN_FV");
        if (!fv_votable) {
                lx_info("failed to get fv_votable\n");
        }else{
                rerun_election(fv_votable);
        }
        return count;
}

static ssize_t smart_fv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
			return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->smart_fv);
}

static struct device_attribute smart_fv_attr =
		__ATTR(smart_fv, 0644, smart_fv_show, smart_fv_store);

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
#ifndef FACTORY_BUILD
static ssize_t otg_ui_support_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

        charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	return sprintf(buf, "%d\n", 1);
}
static struct device_attribute otg_ui_support_attr =
		__ATTR(otg_ui_support, 0644, otg_ui_support_show, NULL);
static ssize_t cid_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int val = manual_get_attach_status();
	lx_info("[%s] val: %d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}
static struct device_attribute cid_status_attr =
		__ATTR(cid_status, 0644, cid_status_show, NULL);
static ssize_t cc_toggle_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val = 0;

        charger_manager_from_psy(dev, batt_psy, &manager);

	manual_get_cc_toggle(&manager->control_cc_toggle);

	val = !!manager->control_cc_toggle;
	lx_info("[%s] val: %d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t cc_toggle_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int val = 0;

        charger_manager_from_psy(dev, batt_psy, &manager);
        if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	manager->control_cc_toggle = val;
	manual_set_cc_toggle(!!val);
	lx_info("[%s] *val: %d\n", __func__, manager->control_cc_toggle);
	return count;
}
static struct device_attribute cc_toggle_attr =
		__ATTR(cc_toggle, 0644, cc_toggle_show, cc_toggle_store);
#endif

#define	FG_MAC_CMD_MANU_DATE	0x004D
#define 	FG_MAC_CMD_MANU_INFO	0x0070

static ssize_t manufacturing_date_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	int ret;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	lx_info("manufacturing_date_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->bat_sn[0]) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFO, bq->bat_sn, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			lx_err("failed to get FG_MAC_CMD_MANU_INFO:%d\n", ret);
			return -EINVAL;
		}
	}
	lx_info("battery manufacture date:%02X %02X %02X %02X\n",bq->bat_sn[6], bq->bat_sn[7], bq->bat_sn[8], bq->bat_sn[9]);
	// Year
	ubuf[0] = '2';
	ubuf[1] = '0';
	ubuf[2] = '2';
	ubuf[3] = bq->bat_sn[6];
	// month
	if(bq->bat_sn[7] >= '0' && bq->bat_sn[7] <= '9'){
		ubuf[4] = '0';
		ubuf[5] = bq->bat_sn[7];
	}else if (bq->bat_sn[7] >= 'A' && bq->bat_sn[7] <= 'F'){
		ubuf[4] = '1';
		ubuf[5] = bq->bat_sn[7] - ('A'-'0');		
	}else if(bq->bat_sn[7] >= 'a' && bq->bat_sn[7] <= 'f'){
		ubuf[4] = '1';
		ubuf[5] = bq->bat_sn[7] - ('a'-'0');		
	}
	// day
	ubuf[6] = bq->bat_sn[8];
	ubuf[7] = bq->bat_sn[9];
	ubuf[8] = '\0';
	lx_info("battery manufacture date:%s\n", ubuf);

	return strlen(ubuf);
}
static struct device_attribute manufacturing_date_attr =
	__ATTR(manufacturing_date, 0644, manufacturing_date_show, NULL);

static ssize_t soh_sn_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	int ret;

	lx_info("soh_sn_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->bat_sn[0]) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFO, bq->bat_sn, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '0', 32);
			ubuf[32] = '\0';
			lx_err("failed to get FG_MAC_CMD_MANU_INFO:%d\n", ret);
			return -EINVAL;
		} 
	}
	memcpy(ubuf, bq->bat_sn, 32);
	ubuf[32] = '\0';
	print_hex_dump(KERN_ERR, "battery sn hex:", DUMP_PREFIX_NONE, 16, 1, ubuf, 32, 0);
	lx_info("battery sn string:%s\n", ubuf);

	return strlen(ubuf);
}
static struct device_attribute soh_sn_attr =
	__ATTR(soh_sn, 0644, soh_sn_show, NULL);

static ssize_t fg1_df_check_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	u8 data[32] = {0};
	int ret;

	lx_info("fg1_df_check_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!data[0]) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_FG1_DF_CHECK, data, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '0', 32);
			ubuf[32] = '\0';
			lx_err("failed to get FG_MAC_CMD_FG1_DF_CHECK:%d\n", ret);
			return -EINVAL;
		} 
	}
	return snprintf(ubuf, PAGE_SIZE, "%d\n", (data[0]<<8 | data[1]));
}
static struct device_attribute fg1_df_check_attr =
	__ATTR(fg1_df_check, 0644, fg1_df_check_show, NULL);

static ssize_t fg1_chemid_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	u8 data[32] = {0};
	int ret;

	lx_info("fg1_chemid_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!data[0]) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_FG1_CHEMID, data, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '0', 32);
			ubuf[32] = '\0';
			lx_err("failed to get FG_MAC_CMD_FG1_CHEMID:%d\n", ret);
			return -EINVAL;
		} 
	}
	return snprintf(ubuf, PAGE_SIZE, "%d\n", (data[0]<<8 | data[1]));
}
static struct device_attribute fg1_chemid_attr =
	__ATTR(fg1_chemid, 0644, fg1_chemid_show, NULL);

static ssize_t pack_vendor_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	int pack_vendor = 0;

	lx_info("pack_vendor_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lx_err("%s:manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	pack_vendor = fuel_gauge_get_battery_id(manager->fuel_gauge);

out:
	return snprintf(ubuf, PAGE_SIZE, "%d\n", pack_vendor);
}
static struct device_attribute pack_vendor_attr =
	__ATTR(pack_vendor, 0644, pack_vendor_show, NULL);

static ssize_t fg1_soh_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("fg1_soh_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", bq->soh);
}
static struct device_attribute fg1_soh_attr =
	__ATTR(fg1_soh, 0644, fg1_soh_show, NULL);

static ssize_t fg1_cycle_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("fg1_cycle_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", bq->cycle_count);
}
static struct device_attribute fg1_cycle_attr =
	__ATTR(fg1_cycle, 0644, fg1_cycle_show, NULL);

static ssize_t authentic_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("authentic_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", bq->authenticate);
}
static struct device_attribute authentic_attr =
	__ATTR(authentic, 0644, authentic_show, NULL);

static ssize_t chip_ok_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("chip_ok_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	return snprintf(ubuf, PAGE_SIZE, "%d\n", bq->chip_ok);
}
static struct device_attribute chip_ok_attr =
	__ATTR(chip_ok, 0644, chip_ok_show, NULL);

static ssize_t ui_soh_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	int ret;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("ui_soh_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '9', 8);
			ubuf[8] = '\0';
			lx_err("failed to get FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EINVAL;
		} else 
			bq->mi_infoC_valid = 1;
	}

	return snprintf(ubuf, PAGE_SIZE, "%u %u %u %u %u %u %u %u %u %u %u\n",
				bq->mi_infoC[0], bq->mi_infoC[1], bq->mi_infoC[2], bq->mi_infoC[3], bq->mi_infoC[4],
				bq->mi_infoC[5], bq->mi_infoC[6], bq->mi_infoC[7], bq->mi_infoC[8], bq->mi_infoC[9], bq->mi_infoC[10]);
}
#define UISOH_LEN 11
static ssize_t ui_soh_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, cnt = 0;
    ssize_t len = 0;
	char tx_data[64], tx_char[64];
	char *pchar = NULL, *qchar = NULL;
    u8 val, ui_soh_data[UISOH_LEN];
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("ui_soh_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			lx_err("failed to read FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}

	memset(tx_data, 0, sizeof(tx_data));
	memset(ui_soh_data, 0, sizeof(ui_soh_data));
	if (count > sizeof(tx_data)) {
		lx_err("%s: data len:%d invalid\n", __func__, count);
		return -EINVAL;
	}
	strncpy(tx_data, buf, count);
	qchar = tx_data;
	while ((pchar = strsep(&qchar, " "))) {
		if (kstrtou8(pchar, 10, &val) < 0) {
			lx_err("parse data:%d failed\n", cnt);
			return -EINVAL;
		}
		if (cnt == UISOH_LEN) {
			lx_err("write ui soh data len invalid, force quit\n");
			break;
		}
		ui_soh_data[cnt++] = val;
	}
	memset(tx_char, 0, sizeof(tx_char));
	len += snprintf(tx_char, sizeof(tx_char), "ui soh data:");
	for (cnt = 0; cnt < UISOH_LEN; cnt++)
		len += snprintf(tx_char + len, sizeof(tx_char), " %u", ui_soh_data[cnt]);
	lx_err("%s\n", tx_char);
	memcpy(bq->mi_infoC, ui_soh_data, UISOH_LEN);
	mutex_lock(&bq->rw_lock);
	ret = fuel_guage_mac_write_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
	mutex_unlock(&bq->rw_lock);
	if (ret) {
		bq->mi_infoC_valid = 0;
		lx_err("get old FG_MAC_CMD_MANU_INFOC failed:%d\n", ret);
		return -EPERM;
	}

	return count;
}
static struct device_attribute ui_soh_attr =
	__ATTR(ui_soh, 0644, ui_soh_show, ui_soh_store);

static ssize_t soh_new_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	int ret;

	lx_info("soh_new_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);
	
	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '9', 8);
			ubuf[8] = '\0';
			lx_err("failed to get FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EINVAL;
		} else 
			bq->mi_infoC_valid = 1;
	}

	return snprintf(ubuf, PAGE_SIZE, "%u\n", bq->mi_infoC[0]);
}
static struct device_attribute soh_new_attr =
	__ATTR(soh_new, 0644, soh_new_show, NULL);

static ssize_t first_usage_date_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	int ret;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("first_usage_date_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			memset(ubuf, '9', 8);
			ubuf[8] = '\0';
			lx_err("failed to get FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EINVAL;
		} else 
			bq->mi_infoC_valid = 1;
	}
	lx_info("battery activiate date hex: %02X%02X%02X\n",
	bq->mi_infoC[11], bq->mi_infoC[12], bq->mi_infoC[13]);
	if (bq->mi_infoC[11] == 0x00 && bq->mi_infoC[12] == 0x00 && bq->mi_infoC[13] == 0x00) {
		memset(ubuf, '0', 8);
		ubuf[8] = '\0';
		lx_err("reset data to 0\n");
	} else {
		ubuf[0] = '2';
		ubuf[1] = '0';
		ubuf[2] = '0' + bq->mi_infoC[11] / 10;
		ubuf[3] = '0' + bq->mi_infoC[11] % 10;
		ubuf[4] = '0' + bq->mi_infoC[12] / 10;
		ubuf[5] = '0' + bq->mi_infoC[12] % 10;
		ubuf[6] = '0' + bq->mi_infoC[13] / 10;
		ubuf[7] = '0' + bq->mi_infoC[13] % 10;
		ubuf[8] = '\0';
		lx_info("battery activiate date string:%s\n", ubuf);
	}

	return strlen(ubuf);
}

static ssize_t first_usage_date_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	char date_str[8], date_u8[3];
	int i, j = 0, ret = -EINVAL, date_len;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("first_usage_date_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			lx_err("failed to read FG_MAC_CMD_MANU_INFOC:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}

	date_len = strlen(buf);
	for (i = 0; i < date_len; i++) {
		if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n') {
			if (j >= 8) {
				lx_err("date length too large\n");
				return -E2BIG;
			}
			if (buf[i] < '0' || buf[i] > '9') {
				lx_err("date has invalid char:%c(0x%02x)\n", buf[i], buf[i]);
				return -EINVAL;
			}
			date_str[j++] = buf[i];
		}
	}
	lx_info("activiate date hex: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		date_str[0], date_str[1], date_str[2], date_str[3], date_str[4], date_str[5],  date_str[6],  date_str[7]);
	date_u8[0] = (date_str[2] - '0') * 10 + (date_str[3] - '0');
	date_u8[1] = (date_str[4] - '0') * 10 + (date_str[5] - '0');
	date_u8[2] = (date_str[6] - '0') * 10 + (date_str[7] - '0');
	if (date_u8[0] == bq->mi_infoC[11] && date_u8[1] == bq->mi_infoC[12] && date_u8[2] == bq->mi_infoC[13]) {
		lx_err("repeated date write, not allowed\n");
		return len;
	}
	bq->mi_infoC[11] = (date_str[2] - '0') * 10 + (date_str[3] - '0');
	bq->mi_infoC[12] = (date_str[4] - '0') * 10 + (date_str[5] - '0');
	bq->mi_infoC[13] = (date_str[6] - '0') * 10 + (date_str[7] - '0');
	mutex_lock(&bq->rw_lock);
	ret = fuel_guage_mac_write_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, bq->mi_infoC, 32);
	mutex_unlock(&bq->rw_lock);
	if (ret) {
		bq->mi_infoC_valid = 0;
		lx_err("failed to read FG_MAC_CMD_MANU_INFOC:%d\n", ret);
		return -EPERM;
	}
	return len;
}
static struct device_attribute first_usage_date_attr =
	__ATTR(first_usage_date, 0644, first_usage_date_show, first_usage_date_store);


static ssize_t charger_partition_poweroffmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int val = 0;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	lx_info("first_usage_date_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	rc = charger_partition1_get_prop(manager->fuel_gauge, CHARGER_PARTITION_PROP_POWER_OFF_MODE, &val);
	if(rc < 0){
		lx_err("[charger] %s get power_off_mode from charger parition failed, ret = %d\n", __func__, rc);
		return -EINVAL;
	}
	lx_err("[charger] %s power_off_mode: %d \n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t charger_partition_poweroffmode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	int val = 0;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;

	lx_info("first_usage_date_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
			return -EINVAL;

	rc = charger_partition1_set_prop(manager->fuel_gauge, CHARGER_PARTITION_PROP_POWER_OFF_MODE, val);
	if (rc < 0) {
		lx_err("[charger] %s set power_off_mode to charger parition failed, ret = %d\n", __func__, rc);
		return -EINVAL;
	}
	return count;
}
static struct device_attribute charger_partition_poweroffmode_attr =
	__ATTR(charger_partition_poweroffmode, 0644, charger_partition_poweroffmode_show, charger_partition_poweroffmode_store);

static int fg_get_seal(struct charger_manager *manager, int *value)
{
	int ret = 0;
	u8 opr_data[32] = {0,};
	struct bq_fg_chip *bq = NULL;

	if (manager == NULL || manager->fuel_gauge == NULL)
		return -EINVAL;
	lx_info("fg_get_seal start\n");
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	mutex_lock(&bq->rw_lock);
	ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_OPR_STAT, opr_data, 32);
	if (ret) {
		mutex_unlock(&bq->rw_lock);
		lx_err("%s, read operation status failed:%d\n", __func__, ret);
		return -EINVAL;
	}
	mutex_unlock(&bq->rw_lock);
	*value = opr_data[1] & 0x03;

	return ret;
}
static int fg_set_seal(struct charger_manager *manager, int val)
{
	int ret = 0;
	int count = 0;
	int seal_state = 0;
	u8 unseal_key[4] = { 0xA6, 0x35, 0xCF, 0x41 };
	u8 seal_fg[2] = { 0x30, 0x00 };
	struct bq_fg_chip *bq = NULL;

	lx_info("fg_set_seal start, val = %d\n", val);
	if (manager == NULL || manager->fuel_gauge == NULL)
		return -EINVAL;
	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (val) {
		ret = fg_get_seal(manager, &seal_state);
		lx_info("seal status:%d\n", seal_state);
		if (seal_state == SEAL_STATE_FA || seal_state == SEAL_STATE_UNSEALED) {
			lx_err("FG is unsealed1\n");
			return 0;
		}
		while (count++ < 3) {
			mutex_lock(&bq->rw_lock);
			ret = fuel_guage_write_reg_block(manager->fuel_gauge, bq->regs[BQ_FG_REG_ALT_MAC], &unseal_key[0], 2);
			if (ret < 0) {
				lx_err("%s, write first unseal key failed:%d\n", __func__, ret);
			}
			ret = fuel_guage_write_reg_block(manager->fuel_gauge, bq->regs[BQ_FG_REG_ALT_MAC], &unseal_key[2], 2);
			if (ret < 0) {
				lx_err("%s, write second unseal key failed:%d\n", __func__, ret);
			}
			mutex_unlock(&bq->rw_lock);
			msleep(300);
			ret = fg_get_seal(manager, &seal_state);
			if (seal_state == SEAL_STATE_FA || seal_state == SEAL_STATE_UNSEALED) {
				lx_err("FG is unsealed2\n");
				break;
			}
			msleep(100);
		}
	} else {
		ret = fg_get_seal(manager, &seal_state);
		if (seal_state == SEAL_STATE_SEALED) {
			lx_err("FG is sealed1\n");
			return 0;
		}
		while (count++ < 3) {
			mutex_lock(&bq->rw_lock);
			ret = fuel_guage_write_reg_block(manager->fuel_gauge, bq->regs[BQ_FG_REG_ALT_MAC], &seal_fg[0], 2);
			mutex_unlock(&bq->rw_lock);
			if (ret < 0) {
				lx_err("write seal key failed\n");
			}
			msleep(250);
			ret = fg_get_seal(manager, &seal_state);
			if (seal_state == SEAL_STATE_SEALED) {
				lx_info("FG is sealed2\n");
				break;
			}
			msleep(100);
		}
	}

	lx_info("fg_set_seal end\n");
	return ret;
}
static ssize_t clear_cycle_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int i, ret, cycle_count;
	int seal_state = SEAL_STATE_SEALED;
	bool verify_success = false;
	u8 manuInfoC_data[32] = {0,};
	u8 wr_stat_data[32] = {0,};
	u8 rd_stat_data[32] = {0,};
	union power_supply_propval pval = {0};
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	char reset_key[16] = {
		'c',  'l',  'r',  'c',  'l',  's',  '\0', '\0',
		'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
	};

	lx_info("clear_cycle_store start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (count < strlen(reset_key)) {
		lx_err("%s: len invalid:%d\n", __func__, count);
		return -EINVAL;
	}

	if (memcmp(buf, reset_key, strlen(reset_key))) {
		lx_err("%s: key error\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&bq->rw_lock);
	ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, manuInfoC_data, 32);
	mutex_unlock(&bq->rw_lock);
	if (ret) {
		lx_err("%s, read FG_MAC_CMD_MANU_INFOC failed:%d\n", __func__, ret);
		return -EPERM;
	} 

	if (manuInfoC_data[14] == 0x01) {
		lx_err("%s, cycle count already been reset\n", __func__);
		return -EPERM;
	}

	ret = power_supply_get_property(manager->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if (ret < 0) {
		lx_err("failed to get temp prop\n");
		return -EINVAL;
	}
	cycle_count = pval.intval;
	if (cycle_count >= 10) {
		lx_err("%s, invalid cycle count:%d\n", __func__, cycle_count);
		return -EINVAL;
	}

	/* step 1: 解锁电量计 */
	fg_set_seal(manager, true);
	fg_get_seal(manager, &seal_state);
	if (seal_state != SEAL_STATE_UNSEALED) {
		lx_err("Failed to unseal gauge, seal_state=%d\n", seal_state);
		return -EINVAL;
	}

	/* step 2: 清零cyclecount */
	for (i = 0; i < 3; i++) {
		/* 读取数据 */
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_FG_STAT, wr_stat_data, 32);
		if (ret) {
			mutex_unlock(&bq->rw_lock);
			lx_err("read back fg status failed:%d\n", ret);
			msleep(100);
			continue;
		}
		/* 写入清零 */
		wr_stat_data[14] = wr_stat_data[15] = 0x00;
		ret = fuel_guage_mac_write_block(manager->fuel_gauge, FG_MAC_CMD_FG_STAT, wr_stat_data, 32);
		msleep(100);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			lx_err("write fg status failed:%d\n", ret);
			msleep(100);
			continue;
		}
		/* 回读验证 */
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_FG_STAT, rd_stat_data, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			lx_err("read back fg status failed:%d\n", ret);
		}
		if (memcmp(wr_stat_data, rd_stat_data, 32)) {
			lx_err("updata fg status failed\n");
			msleep(100);
			continue;
		} else {
			lx_info("Cycle count clear verified successfully (retry %d)\n", i);
			verify_success = true;
			break;
		}
	}

	if (!verify_success) {
		lx_err("Cycle count clear failed after 3 retries\n");
		fg_set_seal(manager, false);
		return -EINVAL;
	}

	/* step 3: 上锁电量计 */
	fg_set_seal(manager, false);

	/* step 4: 设置清零标志位 */
	verify_success = false;
	for (i = 0; i < 3; i++) {
		/* 读取007B数据 */
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, wr_stat_data, 32);
		if (ret < 0) {
			mutex_unlock(&bq->rw_lock);
			lx_err("read wr_stat_data fail (retry %d)\n", i);
			msleep(100);
			continue;
		}

		/* 写入清零标志位 */
		wr_stat_data[14] = 0x01;
		ret = fuel_guage_mac_write_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, wr_stat_data, 32);
		msleep(100);
		mutex_unlock(&bq->rw_lock);
		if (ret < 0) {
			lx_err("update reset cycle count flag failed (retry %d)\n", i);
			msleep(100);
			continue;
		}

		/* 回读007B数据 */
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, rd_stat_data, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret < 0) {
			lx_err("read rd_stat_data fail (retry %d)\n", i);
			msleep(100);
			continue;
		}
		
		if (memcmp(wr_stat_data, rd_stat_data, 32)) {
			lx_err("updata fg status failed\n");
			msleep(100);
			continue;
		} else {
			lx_info("Clear flag set and verified successfully\n");
			verify_success = true;
			break;
		}
	}

	if (!verify_success) {
		lx_err("Failed to set clear flag after 3 retries\n");
		return -EINVAL;
	}

	return count;
}
static struct device_attribute clear_cycle_attr =
	__ATTR(clear_cycle, 0644, NULL, clear_cycle_store);

static ssize_t clear_cycle_flag_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	int ret;
	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;
	u8 manuInfoC_data[32] = {0,};

	lx_info("clear_cycle_flag_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	mutex_lock(&bq->rw_lock);
	ret = fuel_guage_mac_read_block(manager->fuel_gauge, FG_MAC_CMD_MANU_INFOC, manuInfoC_data, 32);
	mutex_unlock(&bq->rw_lock);
	if (ret) {
		lx_err("get FG_MAC_CMD_MANU_INFOC failed:%d\n", ret);
		return -EPERM;
	}
	lx_info("cycle count flag:%d\n", manuInfoC_data[14]);
	return snprintf(ubuf, PAGE_SIZE, "%u\n", manuInfoC_data[14]);
}
static struct device_attribute clear_cycle_flag_attr =
	__ATTR(clear_cycle_flag, 0644, clear_cycle_flag_show, NULL);

static u16 calc_rvalue_8bit_convert_to_16bit(u8 lsb, u8 hsb)
{
	u16 sum = 0;

	sum |= hsb;
	sum = (sum << 8) | lsb;

	return sum;
}
static ssize_t calc_rvalue_show(struct device *dev, struct device_attribute *attr, char *ubuf)
{
	int ret;
	static u8 K1 = 45;
	static u8 K2 = 45;
	static u8 K3 = 70;
	static u8 K4 = 45;
	static u8 K5 = 100;
	static u8 K6 = 70;
	static u8 K7 = 45;

	u16 CCcc = 0;
	u16 FFff = 0;
	u16 IIii = 0;
	u16 JJjj = 0;
	u16 MMmm = 0;
	u16 NNnn = 0;
	u16 OOoo = 0;
	int Rvalue = 0;
	int Rvalue_temp = 0;

	struct power_supply *batt_psy = NULL;
	struct charger_manager *manager = NULL;
	struct bq_fg_chip *bq = NULL;

	lx_info("calc_rvalue_show start\n");
	charger_manager_from_psy(dev, batt_psy, &manager);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bq = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bq))
		return PTR_ERR(bq);

	if (!bq->mi_infoC_valid) {
		mutex_lock(&bq->rw_lock);
		ret = fuel_guage_mac_read_block(manager->fuel_gauge, 0x0066, bq->mi_infoC, 32);
		mutex_unlock(&bq->rw_lock);
		if (ret) {
			lx_err("get 0x0066 failed:%d\n", ret);
			return -EPERM;
		} else
			bq->mi_infoC_valid = 1;
	}

	CCcc = calc_rvalue_8bit_convert_to_16bit(bq->mi_infoC[4], bq->mi_infoC[5]);
	FFff = calc_rvalue_8bit_convert_to_16bit(bq->mi_infoC[10], bq->mi_infoC[11]);
	IIii = calc_rvalue_8bit_convert_to_16bit(bq->mi_infoC[16], bq->mi_infoC[17]);
	JJjj = calc_rvalue_8bit_convert_to_16bit(bq->mi_infoC[18], bq->mi_infoC[19]);
	MMmm = calc_rvalue_8bit_convert_to_16bit(bq->mi_infoC[24], bq->mi_infoC[25]);
	NNnn = calc_rvalue_8bit_convert_to_16bit(bq->mi_infoC[26], bq->mi_infoC[27]);
	OOoo = calc_rvalue_8bit_convert_to_16bit(bq->mi_infoC[28], bq->mi_infoC[29]);

	Rvalue_temp = (CCcc * K1 + FFff * K2 + IIii * K3 + JJjj * K4 + MMmm * K5 + NNnn * K6 + OOoo * K7) / 10;

	lx_info("CCcc:%d, FFff:%d, IIii:%d, JJjj:%d, MMmm:%d, NNnn:%d, OOoo:%d, Rvalue_temp:%d\n", CCcc, FFff, IIii, JJjj, MMmm, NNnn, OOoo, Rvalue_temp);
	if (Rvalue <= Rvalue_temp)
		Rvalue = Rvalue_temp;

	return snprintf(ubuf, PAGE_SIZE, "%d\n", Rvalue);
}
static struct device_attribute calc_rvalue_attr =
	__ATTR(calc_rvalue, 0644, calc_rvalue_show, NULL);

static struct attribute *batt_psy_attrs[] = {
	&input_suspend_attr.attr,
	&shipmode_count_reset_attr.attr,
	&reverse_quick_charge_attr.attr,
	&revchg_bcl_attr.attr,
	&manufacturing_date_attr.attr,
	&first_usage_date_attr.attr,
	&soh_sn_attr.attr,
	&charger_partition_poweroffmode_attr.attr,
	&disable_chg_attr.attr,
	&fg1_soh_attr.attr,
	&fg1_cycle_attr.attr,
	&authentic_attr.attr,
	&ui_soh_attr.attr,
	&soh_new_attr.attr,
	&chip_ok_attr.attr,
	&fg1_df_check_attr.attr,
	&fg1_chemid_attr.attr,
	&pack_vendor_attr.attr,
	&clear_cycle_attr.attr,
	&clear_cycle_flag_attr.attr,
	&calc_rvalue_attr.attr,
#ifndef FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	&lpd_control_attr.attr,
	&decrease_volt_attr.attr,
	&lpd_status_attr.attr,
#endif
#endif
#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	&soc_decimal_attr.attr,
	&soc_decimal_rate_attr.attr,
#endif
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
	&smart_chg_attr.attr,
	&smart_batt_attr.attr,
	&smart_fv_attr.attr,
	&night_charging_attr.attr,
#endif
	&mtbf_current_attr.attr,
#ifndef FACTORY_BUILD
	&otg_ui_support_attr.attr,
	&cid_status_attr.attr,
	&cc_toggle_attr.attr,
#endif
#ifdef FACTORY_BUILD
	&ato_soc_user_control_attr.attr,
#endif
	NULL,
};

static const struct attribute_group batt_psy_attrs_group = {
	.attrs = batt_psy_attrs,
};

int lx_batt_sysfs_create_group(struct charger_manager *manager)
{
	return sysfs_create_group(&manager->batt_psy->dev.kobj,
								&batt_psy_attrs_group);
}
EXPORT_SYMBOL(lx_batt_sysfs_create_group);

MODULE_DESCRIPTION("LX Charger sysfs");
MODULE_LICENSE("GPL v2");
