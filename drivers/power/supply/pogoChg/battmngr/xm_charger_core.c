
#include <linux/battmngr/qti_use_pogo.h>

struct xm_charger *g_xm_charger;
EXPORT_SYMBOL(g_xm_charger);

#if 0
static int charger_check_battery_psy(struct xm_charger *charger)
{
	if (!charger->batt_psy) {
		charger->batt_psy = power_supply_get_by_name("battery");
		if (!charger->batt_psy) {
			charger_err("%s batt psy not found!\n", __func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int charger_check_usb_psy(struct xm_charger *charger)
{
	if (!charger->usb_psy) {
		charger->usb_psy = power_supply_get_by_name("usb");
		if (!charger->usb_psy) {
			charger_err("%s usb psy not found!\n", __func__);
			return -EINVAL;
		}
	}

	return 0;
}
#endif

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
		chg->real_type = POWER_SUPPLY_TYPE_USB_PD;
		charger_err("%s: pd_active=%d\n", __func__, chg->pd_active);
	} else {
		chg->bc12_type = g_battmngr_noti->mainchg_msg.chg_type;
		switch (chg->bc12_type) {
		case POWER_SUPPLY_USB_TYPE_SDP:
			chg->real_type = POWER_SUPPLY_TYPE_USB;
			break;
		case POWER_SUPPLY_TYPE_USB_FLOAT:
		case POWER_SUPPLY_USB_TYPE_DCP:
		case POWER_SUPPLY_TYPE_USB_HVDCP:
		case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
			chg->real_type = POWER_SUPPLY_TYPE_USB_DCP;
			break;
		case POWER_SUPPLY_USB_TYPE_CDP:
			chg->real_type = POWER_SUPPLY_TYPE_USB_CDP;
			break;
		default:
			chg->real_type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
	}

	if (0) {
		if (last_bc12_type != chg->bc12_type || last_pd_active != chg->pd_active) {
			cancel_delayed_work(&(chg->chg_feature->xm_prop_change_work));
			chg->chg_feature->update_cont = 0;
			schedule_delayed_work(&(chg->chg_feature->xm_prop_change_work), msecs_to_jiffies(120));
		}
	}

	last_bc12_type = chg->bc12_type;
	last_pd_active = chg->pd_active;

	charger_info("%s: usb_type=%d, chg->real_type=%d\n", __func__,
			chg->bc12_type, chg->real_type);

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
		charger_jeita_start_stop(g_xm_charger, g_xm_charger->chg_jeita);
		xm_charger_set_fastcharge_mode(g_xm_charger, false);
#if 0
		night_charging_start_stop(g_xm_charger, g_xm_charger->chg_feature);
#endif
		break;
	case BATTMNGR_MSG_MAINCHG_OTG_ENABLE:
		g_xm_charger->otg_enable = g_battmngr_noti->mainchg_msg.otg_enable;
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
		charger_jeita_start_stop(g_xm_charger, g_xm_charger->chg_jeita);
		//night_charging_start_stop(g_xm_charger, g_xm_charger->chg_feature);
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

static int xm_charger_votable_init(struct xm_charger *charger)
{
	charger->fcc_votable = find_votable("FCC");
	charger->fv_votable = find_votable("FV");
	charger->usb_icl_votable = find_votable("ICL");

	if (!charger->fcc_votable || !charger->fv_votable
		|| !charger->usb_icl_votable)
		return -EINVAL;

	vote(charger->fcc_votable, CHG_INIT_VOTER, true, charger->dt.batt_current_max);
	vote(charger->fv_votable, CHG_INIT_VOTER, true, charger->dt.chg_voltage_max);
	vote(charger->usb_icl_votable, CHG_INIT_VOTER, true, charger->dt.input_batt_current_max);

	return 0;
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
		charger->dt.chg_voltage_max = 4480;

	rc = of_property_read_u32(node,
				"xm,fcc-max-ua", &charger->dt.batt_current_max);
	if (rc < 0)
		charger->dt.batt_current_max = 3000;

	rc = of_property_read_u32(node,
				"xm,fv-max-design-uv", &charger->dt.chg_design_voltage_max);
	if (rc < 0)
		charger->dt.chg_design_voltage_max = 9000;

	rc = of_property_read_u32(node,
				"xm,icl-max-ua", &charger->dt.input_batt_current_max);
	if (rc < 0)
		charger->dt.input_batt_current_max = 2500;

	rc = of_property_read_u32(node,
				"xm,non_ffc_iterm", &charger->dt.non_ffc_ieoc);
	if (rc < 0)
		charger->dt.non_ffc_ieoc = 220;

	rc = of_property_read_u32(node,
				"xm,non_ffc_cv", &charger->dt.non_ffc_cv);
	if (rc < 0)
		charger->dt.non_ffc_cv = 4480;

	rc = of_property_read_u32(node,
				"xm,ffc_cv", &charger->dt.ffc_cv);
	if (rc < 0)
		charger->dt.ffc_cv = 4480;

	rc = of_property_read_u32(node,
				"xm,non_ffc_cc", &charger->dt.non_ffc_cc);
	if (rc < 0)
		charger->dt.non_ffc_cc = 3000;

	rc = xm_charger_parse_dt_therm(charger);
	if (rc < 0) {
		charger_err("%s: Couldn't initialize parse_dt_therm rc = %d\n",
				__func__, rc);
		return rc;
	}
	return 0;
};

int xm_charger_init(struct xm_charger *charger)
{
	int rc = 0;

	charger_err("%s: Start\n", __func__);

	rc = check_qti_ops(&charger->battmg_dev);
	if (!rc) {
		charger_err("%s: xm_charger_check_qti_ops fail, rc=%d\n", __func__, rc);
		return rc;
	}

	device_init_wakeup(charger->dev, true);

	rc = xm_charger_parse_dt(charger);
	if (rc < 0) {
		charger_err("%s: Couldn't parse device tree rc=%d\n", __func__, rc);
		return rc;
	}

	rc = xm_charger_create_votable(charger);
	if (rc < 0) {
		charger_err("%s: xm_charger_create_votable fail, rc=%d\n", __func__, rc);
		return rc;
	}

	rc = xm_charger_votable_init(charger);
	if (rc < 0) {
		charger_err("%s: xm_charger_votable_init fail, rc=%d\n", __func__, rc);
		return rc;
	}

	rc = charger_jeita_init(charger);
	if (rc < 0) {
		charger_err("%s: xm_stepchg_jeita_init fail rc=%d\n", __func__, rc);
		return rc;
	}

	// rc = xm_chg_feature_init(charger);
	// if (rc < 0) {
	// 	charger_err("%s: Couldn't init xm_chg_feature_init rc=%d\n", __func__, rc);
	// 	return;
	// }

	charger->batt_psy = power_supply_get_by_name("battery");
	charger->usb_psy = power_supply_get_by_name("usb");
	if(charger->batt_psy && charger->usb_psy) {
		charger_err("init successfully \n");
	}

	charger_err("%s: End\n", __func__);

	return rc;
}
EXPORT_SYMBOL(xm_charger_init);

void xm_charger_deinit(struct xm_charger *charger)
{
	charger_jeita_deinit(charger);
	//xm_chg_feature_deinit();

	return;
}
EXPORT_SYMBOL(xm_charger_deinit);

MODULE_DESCRIPTION("Xiaomi charger core");
MODULE_AUTHOR("getian@xiaomi.com");
MODULE_LICENSE("GPL v2");
