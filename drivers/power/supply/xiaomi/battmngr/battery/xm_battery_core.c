
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
	struct xm_battery *battery = power_supply_get_drvdata(psy);

	batt_check_charger_psy(battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_STATUS, &pval->intval);
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
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CAPACITY, &pval->intval);
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
		pval->intval = battery->dt.batt_iterm;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TEMP, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		pval->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CHARGE_FULL, &pval->intval);
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
	int rc = 0;

	if (!node) {
		battery_err("%s: device tree node missing\n", __func__);
		return -EINVAL;
	}

	rc = of_property_read_u32(node,
				"xm,batt-iterm", &battery->dt.batt_iterm);
	if (rc < 0)
		battery->dt.batt_iterm = 300000;

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

	battery->usb_psy = power_supply_get_by_name("usb");
	if (!battery->usb_psy)
		battery_err("%s get usb psy fail\n", __func__);

	g_xm_battery = battery;

	battery_err("%s: End\n", __func__);

	return rc;
}
EXPORT_SYMBOL(xm_battery_init);

MODULE_DESCRIPTION("Xiaomi Battery core");
MODULE_AUTHOR("getian@xiaomi.com");
MODULE_LICENSE("GPL v2");

