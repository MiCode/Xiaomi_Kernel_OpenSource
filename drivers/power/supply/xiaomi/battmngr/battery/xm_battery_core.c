
#include <linux/battmngr/xm_battery_core.h>

struct xm_battery *g_xm_battery;
EXPORT_SYMBOL(g_xm_battery);

static void batt_check_charger_psy(struct xm_battery *battery)
{
	if (!battery->usb_psy) {
		battery->usb_psy = power_supply_get_by_name("usb");
		if (!battery->usb_psy)
			battery_err("%s usb psy not found!\n", __func__);
	}
}

int battery_process_event_fg(struct battmngr_notify *noti_data)
{
	int rc = 0;

	battery_err("%s: msg_type %d\n", __func__, noti_data->fg_msg.msg_type);

	switch (noti_data->fg_msg.msg_type) {
	case BATTMNGR_MSG_FG:
		cancel_delayed_work(&(g_xm_battery->batt_feature->xm_prop_change_work));
		g_xm_battery->batt_feature->update_cont = 1;
		schedule_delayed_work(&(g_xm_battery->batt_feature->xm_prop_change_work), 0);
		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(battery_process_event_fg);

static int get_prop_batt_health(struct xm_battery *battery, union power_supply_propval *val)
{
	int rc = 0;

	if (!battery)
		return -EINVAL;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TEMP, &val->intval);
	if (rc < 0)
		return -EINVAL;

	if (val->intval >= BATT_OVERHEAT_THRESHOLD)
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (val->intval >= BATT_WARM_THRESHOLD && val->intval < BATT_OVERHEAT_THRESHOLD)
		val->intval = POWER_SUPPLY_HEALTH_WARM;
	else if (val->intval >= BATT_COOL_THRESHOLD && val->intval < BATT_WARM_THRESHOLD)
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
	else if (val->intval >= BATT_COLD_THRESHOLD && val->intval < BATT_COOL_THRESHOLD)
		val->intval = POWER_SUPPLY_HEALTH_COOL;
	else if (val->intval < BATT_COLD_THRESHOLD)
		val->intval = POWER_SUPPLY_HEALTH_COLD;

	return 0;
}

static int get_prop_batt_capacity_level(struct xm_battery *battery, union power_supply_propval *val)
{
	int rc = 0, capacity;

	if (!battery)
		return -EINVAL;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CAPACITY, &capacity);
	if (rc < 0)
		return -EINVAL;

	if (capacity == 0)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else if (capacity > 0 && capacity <= 20)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (capacity > 20 && capacity <= 80)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (capacity > 80 && capacity <= 99)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (capacity == 100)
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_FULL;

	return rc;
}

static int get_prop_batt_capacity(struct xm_battery *battery, union power_supply_propval *val)
{
	int rc = 0;

	if (!battery)
		return -EINVAL;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CAPACITY, &val->intval);
	if (rc < 0)
		return -EINVAL;

	if (val->intval == 0)
		power_supply_changed(battery->batt_psy);
	return rc;
}

static int set_prop_system_temp_level(struct xm_battery *battery,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	int rc = 0;

	if (val->intval < 0)
		return -EINVAL;

	if (val->intval >= MAX_TEMP_LEVEL)
		return -EINVAL;

	rc = power_supply_set_property(battery->usb_psy, psp, val);

	return rc;
}

static enum power_supply_property batt_psy_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
};

static int batt_psy_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	int rc = 0;
	int chg_online = 0, chg_present = 0, fg_status = 0;
	struct xm_battery *battery = power_supply_get_drvdata(psy);

	batt_check_charger_psy(battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_PRESENT, &chg_present);
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_ONLINE, &chg_online);
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_STATUS, &pval->intval);
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_STATUS, &fg_status);
		if (chg_present && chg_online) {
			if(fg_status == POWER_SUPPLY_STATUS_FULL || pval->intval == POWER_SUPPLY_STATUS_FULL)
				pval->intval = POWER_SUPPLY_STATUS_FULL;
			else if ((pval->intval != POWER_SUPPLY_STATUS_FULL) && (g_xm_charger->input_suspend == 0))
				pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = get_prop_batt_health(battery, pval);
		if(rc != 0)
			pval->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_PRESENT, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (battery->usb_psy)
			rc = power_supply_get_property(battery->usb_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_TYPE, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		rc = get_prop_batt_capacity_level(battery, pval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = get_prop_batt_capacity(battery, pval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (battery->usb_psy)
			rc = power_supply_get_property(battery->usb_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		if (battery->usb_psy)
			rc = power_supply_get_property(battery->usb_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_VOLTAGE_NOW, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (battery->usb_psy)
			rc = power_supply_get_property(battery->usb_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CURRENT_NOW, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (battery->usb_psy)
			rc = power_supply_get_property(battery->usb_psy, POWER_SUPPLY_PROP_CURRENT_MAX, pval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pval->intval = get_effective_result(g_xm_charger->fcc_votable);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_TERM, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TEMP, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		pval->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_RM, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CYCLE_COUNT, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CHARGE_FULL, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CHARGE_FULL_DESIGN, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TIME_TO_FULL_NOW, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TIME_TO_EMPTY_NOW, &pval->intval);
		break;
	default:
		battery_err("%s: batt power supply prop %d not supported\n", __func__, psp);
		return -EINVAL;
	}

	if (rc < 0) {
		battery_err("%s: Couldn't get prop %d rc = %d\n", __func__, psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int batt_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct xm_battery *battery = power_supply_get_drvdata(psy);

	batt_check_charger_psy(battery);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (battery->usb_psy)
			rc = set_prop_system_temp_level(battery, prop, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = xm_battmngr_write_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CAPACITY, val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = xm_battmngr_write_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TEMP, val->intval);
		break;
	default:
		battery_err("%s: batt power supply prop %d not supported\n", __func__, prop);
		return -EINVAL;
	}

	return rc;
}

static int batt_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_TEMP:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = batt_psy_props,
	.num_properties = ARRAY_SIZE(batt_psy_props),
	.get_property = batt_psy_get_prop,
	.set_property = batt_psy_set_prop,
	.property_is_writeable = batt_psy_prop_is_writeable,
};

static int xm_init_batt_psy(struct xm_battery *battery)
{
	struct power_supply_config batt_cfg = {};
	int rc = 0;

	batt_cfg.drv_data = battery;
	batt_cfg.of_node = battery->dev->of_node;
	battery->batt_psy = devm_power_supply_register(battery->dev,
					   &batt_psy_desc,
					   &batt_cfg);
	if (IS_ERR(battery->batt_psy)) {
		battery_err("%s: Couldn't register battery power supply\n", __func__);
		return PTR_ERR(battery->batt_psy);
	}

	return rc;
}

static int xm_battery_parse_dt(struct xm_battery *battery)
{
	struct device_node *node = battery->dev->of_node;

	if (!node) {
		battery_err("%s: device tree node missing\n", __func__);
		return -EINVAL;
	}

	return 0;
};

int xm_battery_init(struct xm_battery *battery)
{
	int rc = 0;

	battery_err("%s: Start\n", __func__);

	rc = xm_battery_parse_dt(battery);
	if (rc < 0) {
		battery_err("%s: Couldn't parse device tree rc=%d\n", __func__, rc);
		return rc;
	}

	rc = xm_init_batt_psy(battery);
	if (rc < 0) {
		battery_err("%s: Couldn't initialize batt psy rc=%d\n", __func__, rc);
		return -ENODATA;
	}

	rc = xm_batt_feature_init(battery);
	if (rc < 0) {
		battery_err("%s: Couldn't init xm_batt_feature rc=%d\n", __func__, rc);
		return rc;
	}

	battery->usb_psy = power_supply_get_by_name("usb");
	if (!battery->usb_psy)
		battery_err("%s get usb psy fail\n", __func__);

	battery_err("%s: End\n", __func__);

	return rc;
}
EXPORT_SYMBOL(xm_battery_init);

MODULE_DESCRIPTION("Xiaomi Battery core");
MODULE_AUTHOR("getian@xiaomi.com");
MODULE_LICENSE("GPL v2");

