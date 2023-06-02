
#include <linux/battmngr/xm_charger_core.h>

struct xm_charger *g_xm_charger;
EXPORT_SYMBOL(g_xm_charger);

static void charger_check_battery_psy(struct xm_charger *charger)
{
	if (!charger->batt_psy) {
		charger->batt_psy = power_supply_get_by_name("battery");
		if (!charger->batt_psy)
			charger_err("%s batt psy not found!\n", __func__);
	}
}

static void update_pd_bc12_active(struct xm_charger *charger, struct battmngr_notify *noti_data)
{
	static int active_flag;
	charger->pd_active = noti_data->pd_msg.pd_active;
	if (!charger->pd_active) {
		charger->pd_verified = 0;
		g_battmngr_noti->pd_msg.pd_verified = 0;
	}

	charger->bc12_active = noti_data->mainchg_msg.chg_plugin;

	if (!active_flag && (charger->pd_active || charger->bc12_active)) {
		vote(charger->awake_votable, CHG_AWAKE_VOTER, true, 0);
		active_flag = 1;
		charger_err("%s: chg pm_awake\n", __func__);
	} else if (active_flag && !charger->pd_active && !charger->bc12_active) {
		g_battmngr_noti->misc_msg.disable_thermal = 0;
		g_battmngr_noti->misc_msg.vindpm_temp = 0;
		vote(charger->awake_votable, CHG_AWAKE_VOTER, false, 0);
		active_flag = 0;
		charger_err("%s: chg pm_relax\n", __func__);
	}

	return;
}

static void update_usb_type(struct xm_charger *chg)
{
	static int last_bc12_type;
	static int last_pd_active;
	if (chg->pd_active) {
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;
		charger_err("%s: pd_active=%d\n", __func__, chg->pd_active);
	} else {
		chg->bc12_type = g_battmngr_noti->mainchg_msg.chg_type;
		switch (chg->bc12_type) {
		case POWER_SUPPLY_USB_TYPE_SDP:
			usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
			break;
		case POWER_SUPPLY_TYPE_USB_FLOAT:
		case POWER_SUPPLY_USB_TYPE_DCP:
		case POWER_SUPPLY_TYPE_USB_HVDCP:
			usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			break;
		case POWER_SUPPLY_USB_TYPE_CDP:
			usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			break;
		default:
			usb_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
		charger_err("%s: usb_type=%d, usb_psy_desc.type=%d\n", __func__,
				chg->bc12_type, usb_psy_desc.type);
	}

	if (last_bc12_type != chg->bc12_type || last_pd_active != chg->pd_active) {
		cancel_delayed_work(&(chg->chg_feature->xm_prop_change_work));
		chg->chg_feature->update_cont = 0;
		schedule_delayed_work(&(chg->chg_feature->xm_prop_change_work), msecs_to_jiffies(120));
	}
	last_bc12_type = chg->bc12_type;
	last_pd_active = chg->pd_active;
	chg->real_type = usb_psy_desc.type;
	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	charger_err("%s: usb_type=%d, usb_psy_desc.type=%d\n", __func__,
			chg->bc12_type, usb_psy_desc.type);

	return;
}

int charger_process_event_cp(struct battmngr_notify *noti_data)
{
	int rc = 0;

	charger_err("%s: msg_type %d\n", __func__, noti_data->cp_msg.msg_type);

	switch (noti_data->cp_msg.msg_type) {
	case BATTMNGR_MSG_CP_MASTER:
		break;
	case BATTMNGR_MSG_CP_SLAVE:
		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(charger_process_event_cp);

int charger_process_event_mainchg(struct battmngr_notify *noti_data)
{
	int rc = 0;

	charger_err("%s: msg_type %d\n", __func__, noti_data->mainchg_msg.msg_type);

	switch (noti_data->mainchg_msg.msg_type) {
	case BATTMNGR_MSG_MAINCHG_TYPE:
		update_pd_bc12_active(g_xm_charger, noti_data);
		update_usb_type(g_xm_charger);
		typec_conn_therm_start_stop(g_xm_charger, g_xm_charger->chg_feature);
		stepchg_jeita_start_stop(g_xm_charger, g_xm_charger->step_chg);
		night_charging_start_stop(g_xm_charger, g_xm_charger->chg_feature);
		xm_charger_set_fastcharge_mode(g_xm_charger, g_xm_charger->pd_verified);
		if(g_xm_charger->input_suspend == 1)
			xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_ENABLED, 0);
		break;
	case BATTMNGR_MSG_MAINCHG_OTG_ENABLE:
		g_xm_charger->otg_enable = g_battmngr_noti->mainchg_msg.otg_enable;
		typec_conn_therm_start_stop(g_xm_charger, g_xm_charger->chg_feature);
		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(charger_process_event_mainchg);

int charger_process_event_pd(struct battmngr_notify *noti_data)
{
	int rc = 0;

	charger_err("%s: msg_type %d\n", __func__, noti_data->pd_msg.msg_type);

	switch (noti_data->pd_msg.msg_type) {
	case BATTMNGR_MSG_PD_ACTIVE:
		update_pd_bc12_active(g_xm_charger, noti_data);
		update_usb_type(g_xm_charger);
		typec_conn_therm_start_stop(g_xm_charger, g_xm_charger->chg_feature);
		stepchg_jeita_start_stop(g_xm_charger, g_xm_charger->step_chg);
		night_charging_start_stop(g_xm_charger, g_xm_charger->chg_feature);
		xm_charger_set_fastcharge_mode(g_xm_charger, g_xm_charger->pd_verified);
		break;
	case BATTMNGR_MSG_PD_VERIFED:
		xm_charger_set_fastcharge_mode(g_xm_charger, g_xm_charger->pd_verified);
        	cancel_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work));
		g_xm_charger->chg_feature->update_cont = 0;
		schedule_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work), msecs_to_jiffies(600));
		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(charger_process_event_pd);

static int chg_votable_init(struct xm_charger *chg)
{
	chg->fcc_votable = find_votable("FCC");
	chg->fv_votable = find_votable("FV");
	chg->usb_icl_votable = find_votable("ICL");

	if (!chg->fcc_votable || !chg->fv_votable
		|| !chg->usb_icl_votable)
		return -EINVAL;

	vote(chg->fcc_votable, CHG_INIT_VOTER, true, chg->dt.batt_current_max);
	vote(chg->fv_votable, CHG_INIT_VOTER, true, chg->dt.chg_voltage_max);
	vote(chg->usb_icl_votable, CHG_INIT_VOTER, true, chg->dt.input_batt_current_max);
	vote(chg->input_suspend_votable, CHG_INIT_VOTER, false, 0);

	return 0;
}

static enum power_supply_property usb_psy_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
};

static int usb_psy_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int rc = 0, mode;
	struct xm_charger *charger = power_supply_get_drvdata(psy);

	charger_check_battery_psy(charger);
	//update_usb_type(charger);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_PRESENT, &val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_ONLINE, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = charger->dt.chg_design_voltage_max;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_FASTCHARGE_MODE, &mode);
		if (mode)
			val->intval = charger->dt.chg_voltage_max;
		else
			val->intval = charger->dt.non_ffc_cv;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_BUS_VOLTAGE, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_CURRENT, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->dt.batt_current_max;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = charger->dt.input_batt_current_max;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		val->intval = charger->system_temp_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		val->intval = charger->thermal_levels;
		break;
	default:
		charger_err("%s: get prop %d is not supported in usb\n", __func__, psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		charger_err("%s: Couldn't get prop %d rc = %d\n\n", __func__, psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int usb_psy_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct xm_charger *charger = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		charger->system_temp_level = val->intval;
		rc = xm_charger_thermal(charger);
		break;
	default:
		charger_err("%s: Set prop %d is not supported in usb psy\n", __func__, psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int usb_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}

static struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = usb_psy_props,
	.num_properties = ARRAY_SIZE(usb_psy_props),
	.get_property = usb_psy_get_prop,
	.set_property = usb_psy_set_prop,
	.property_is_writeable = usb_psy_prop_is_writeable,
};

static int xm_init_usb_psy(struct xm_charger *charger)
{
	struct power_supply_config usb_cfg = {};
	int rc = 0;

	usb_cfg.drv_data = charger;
	usb_cfg.of_node = charger->dev->of_node;
	charger->usb_psy = devm_power_supply_register(charger->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(charger->usb_psy)) {
		charger_err("%s: Couldn't register USB power supply\n", __func__);
		return PTR_ERR(charger->usb_psy);
	}

	return rc;
}

static int xm_charger_parse_dt(struct xm_charger *charger)
{
	struct device_node *node = charger->dev->of_node;
	int rc = 0;

	if (!node) {
		charger_err("%s: device tree node missing\n", __func__);
		return -EINVAL;
	}

	rc = of_property_read_u32(node,
				"xm,fv-max-uv", &charger->dt.chg_voltage_max);
	if (rc < 0)
		charger->dt.chg_voltage_max = 4450000;

	rc = of_property_read_u32(node,
				"xm,fcc-max-ua", &charger->dt.batt_current_max);
	if (rc < 0)
		charger->dt.batt_current_max = 10000000;

	rc = of_property_read_u32(node,
				"xm,fv-max-design-uv", &charger->dt.chg_design_voltage_max);
	if (rc < 0)
		charger->dt.chg_design_voltage_max = 10000000;

	rc = of_property_read_u32(node,
				"xm,icl-max-ua", &charger->dt.input_batt_current_max);
	if (rc < 0)
		charger->dt.input_batt_current_max = 3000000;

	rc = of_property_read_u32(node,
				"xm,step-chg-enable", &charger->dt.step_chg_enable);
	if (rc < 0)
		charger->dt.step_chg_enable = false;

	rc = of_property_read_u32(node,
				"xm,sw-jeita-enable", &charger->dt.sw_jeita_enable);
	if (rc < 0)
		charger->dt.sw_jeita_enable = false;

	rc = of_property_read_u32(node,
				"xm,ffc_ieoc_l", &charger->dt.ffc_ieoc_l);
	if (rc < 0)
		charger->dt.ffc_ieoc_l = 850000;

	rc = of_property_read_u32(node,
				"xm,ffc_ieoc_h", &charger->dt.ffc_ieoc_h);
	if (rc < 0)
		charger->dt.ffc_ieoc_h = 890000;

	rc = of_property_read_u32(node,
				"xm,non_ffc_ieoc", &charger->dt.non_ffc_ieoc);
	if (rc < 0)
		charger->dt.non_ffc_ieoc = 200000;

	rc = of_property_read_u32(node,
				"xm,non_ffc_cv", &charger->dt.non_ffc_cv);
	if (rc < 0)
		charger->dt.non_ffc_cv = 4480000;

	rc = of_property_read_u32(node,
				"xm,non_ffc_cc", &charger->dt.non_ffc_cc);
	if (rc < 0)
		charger->dt.non_ffc_cc = 4000000;

	rc = xm_charger_parse_dt_therm(charger);
	if (rc < 0) {
		charger_err("%s: Couldn't initialize parse_dt_therm rc = %d\n",
				__func__, rc);
		return rc;
	}
/*
	rc = xm_get_iio_channel(g_battmngr_iio, "chg_pump_therm",
				&g_battmngr_iio->chg_pump_therm);
	if (rc < 0)
		charger_err("%s: Couldn't get IIO channel chg_pump_therm rc = %d\n",
				__func__, rc);
*/
	rc = xm_get_iio_channel(g_battmngr_iio, "typec_conn_therm",
				&g_battmngr_iio->typec_conn_therm);
	if (rc < 0)
		charger_err("%s: Couldn't get IIO channel typec_conn_therm rc = %d\n",
				__func__, rc);

	return 0;
};

int xm_charger_init(struct xm_charger *charger)
{
	int rc = 0;

	charger_err("%s: Start\n", __func__);

	rc = xm_charger_parse_dt(charger);
	if (rc < 0) {
		charger_err("%s: Couldn't parse device tree rc=%d\n", __func__, rc);
		return rc;
	}

	device_init_wakeup(charger->dev, true);

	rc = xm_charger_create_votable(charger);
	if (rc < 0) {
		charger_err("%s: Couldn't init xm_charger_create_votable rc=%d\n", __func__, rc);
		return rc;
	}

	rc = xm_init_usb_psy(charger);
	if (rc < 0) {
		charger_err("%s: Couldn't initialize usb psy rc=%d\n", __func__, rc);
		return rc;
	}

	rc = xm_stepchg_jeita_init(charger, charger->dt.step_chg_enable,
			charger->dt.sw_jeita_enable);
	if (rc < 0) {
		charger_err("%s: Couldn't init xm_stepchg_jeita_init rc=%d\n", __func__, rc);
		return rc;
	}

	rc = xm_chg_feature_init(charger);
	if (rc < 0) {
		charger_err("%s: Couldn't init xm_chg_feature_init rc=%d\n", __func__, rc);
		return rc;
	}

	charger->batt_psy = power_supply_get_by_name("battery");
	if (!charger->usb_psy)
		charger_err("%s get batt psy fail\n", __func__);

	chg_votable_init(charger);
	xm_charger_thermal(charger);

	charger_err("%s: End\n", __func__);

	return rc;
}
EXPORT_SYMBOL(xm_charger_init);

void xm_charger_deinit(void)
{
	xm_step_chg_deinit();
	xm_chg_feature_deinit();

	return;
}
EXPORT_SYMBOL(xm_charger_deinit);

MODULE_DESCRIPTION("Xiaomi charger core");
MODULE_AUTHOR("getian@xiaomi.com");
MODULE_LICENSE("GPL v2");

