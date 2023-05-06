
#include "inc/xm_battmngr_init.h"


/* Standard usb_type definitions similar to power_supply_sysfs.c */
static const char * const power_supply_usb_type_text[] = {
	"Unknown", "USB", "USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "PD_DRP", "PD_PPS", "BrickID"
};

/* Custom usb_type definitions */
static const char * const qc_power_supply_usb_type_text[] = {
	"HVDCP", "HVDCP_3", "HVDCP_3P5", "USB_FLOAT"
};

/* Custom typec mode definitions */
static const char * const typec_mode_type_text[] = {
	"UNATTACHED", "ATTACHED_SNK", "ATTACHED_SRC", "ATTACHED_AUDIO",
	"ATTACHED_DEBUG", "ATTACHED_DBGACC_SNK", "ATTACHED_CUSTOM_SRC",
	"ATTACHED_NORP_SRC"
};

static const char *get_typec_mode_name(int typec_mode)
{
	int i;
	pr_err("%s: typec_mode=%d\n", __func__, typec_mode);

	if (typec_mode < 0 || typec_mode > 7) {
		return "Unkown";
	}

	for (i = 0; i < ARRAY_SIZE(typec_mode_type_text); i++) {
		if (i == typec_mode)
			return typec_mode_type_text[i];
	}

	return "Unknown";

}

static const char *get_usb_type_name(int usb_type)
{
	int i;

	pr_err("%s: usb_type=%d\n", __func__, usb_type);

	if (usb_type >= POWER_SUPPLY_TYPE_USB_HVDCP &&
	    usb_type <= POWER_SUPPLY_TYPE_USB_FLOAT) {
		for (i = 0; i < ARRAY_SIZE(qc_power_supply_usb_type_text);
		     i++) {
			if (i == (usb_type - POWER_SUPPLY_TYPE_USB_HVDCP))
				return qc_power_supply_usb_type_text[i];
		}
		return "Unknown";
	}

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}

	return "Unknown";
}

static ssize_t real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int real_type = 0;

	if (g_xm_charger->pd_active) {
		if (g_xm_charger->pd_active == QTI_POWER_SUPPLY_PD_ACTIVE)
			real_type = POWER_SUPPLY_USB_TYPE_PD;
		else if (g_xm_charger->pd_active == QTI_POWER_SUPPLY_PD_PPS_ACTIVE)
			real_type = POWER_SUPPLY_USB_TYPE_PD_PPS;
	} else {
		real_type = g_xm_charger->bc12_type;
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(real_type));
}
static CLASS_ATTR_RO(real_type);

static ssize_t pd_verifed_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	val = g_xm_charger->pd_verified;
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t pd_verifed_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_xm_charger->pd_verified = val;
	g_battmngr_noti->pd_msg.msg_type = BATTMNGR_MSG_PD_VERIFED;
	g_battmngr_noti->pd_msg.pd_verified = g_xm_charger->pd_verified;
	battmngr_notifier_call_chain(BATTMNGR_EVENT_PD, g_battmngr_noti);

	pr_err("pd_verified = %d\n", g_xm_charger->pd_verified);

	return count;
}
static CLASS_ATTR_RW(pd_verifed);

static ssize_t input_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val, temp;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER, CHARGE_PUMP_LN_BUS_CURRENT, &temp);
	val = temp;
	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE, CHARGE_PUMP_LN_BUS_CURRENT, &temp);
	val += temp;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(input_current);

static ssize_t cp_ibus_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE, CHARGE_PUMP_LN_BUS_CURRENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_ibus_slave);

static ssize_t cp_ibus_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER, CHARGE_PUMP_LN_BUS_CURRENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_ibus_master);

static ssize_t cp_ibus_delta_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val,val1,val2;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER, CHARGE_PUMP_LN_BUS_CURRENT, &val1);
	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE, CHARGE_PUMP_LN_BUS_CURRENT, &val2);
	val = val1 - val2;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_ibus_delta);

static ssize_t cp_present_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE, CHARGE_PUMP_LN_PRESENT, &val);
	pr_err("cp_present_slave_show = %d\n", val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_present_slave);

static ssize_t cp_present_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER, CHARGE_PUMP_LN_PRESENT, &val);
	pr_err("cp_present_master_show = %d\n", val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_present_master);

static ssize_t cp_vbus_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER, CHARGE_PUMP_LN_BUS_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_vbus_voltage);

static ssize_t cp_vbat_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER, CHARGE_PUMP_LN_BATTERY_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_vbat_voltage);

static ssize_t vbus_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_BUS_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(vbus_voltage);

static ssize_t vbat_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_VBAT_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(vbat_voltage);

static ssize_t cell_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_VOLTAGE_NOW, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cell_voltage);

static ssize_t soc_decimal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	val = xm_get_soc_decimal();

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(soc_decimal);

static ssize_t soc_decimal_rate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	val = xm_get_soc_decimal_rate();

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(soc_decimal_rate);

static ssize_t verify_digest_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	get_verify_digest(buf);

	return scnprintf(buf, PAGE_SIZE, "%s", buf);
}

static ssize_t verify_digest_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	char kbuf[70] = {0};

	pr_err("verify_digest_store = %s\n", buf);
	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);
	set_verify_digest(kbuf);

	return count;
}
static CLASS_ATTR_RW(verify_digest);

static ssize_t chip_ok_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CHIP_OK, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(chip_ok);

static ssize_t apdo_max_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;
	int apdo_max_volt, apdo_max_curr;

	xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY, PD_APDO_VOLT_MAX, &apdo_max_volt);
	xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY, PD_APDO_CURR_MAX, &apdo_max_curr);
	val = (apdo_max_volt * apdo_max_curr) / 1000000;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(apdo_max);

static ssize_t fastchg_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_FASTCHARGE_MODE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(fastchg_mode);

static ssize_t soh_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_SOH, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(soh);

static ssize_t connector_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val = 0;

	if (!g_battmngr_iio->typec_conn_therm) {
		xm_get_iio_channel(g_battmngr_iio, "typec_conn_therm",
				&g_battmngr_iio->typec_conn_therm);
	}

	if (g_battmngr_iio->typec_conn_therm) {
		iio_read_channel_processed(g_battmngr_iio->typec_conn_therm,
				&val);
		val /= 100;
		if (g_xm_charger->chg_feature->fake_conn_temp != 0)
			val = g_xm_charger->chg_feature->fake_conn_temp;
		return scnprintf(buf, PAGE_SIZE, "%d\n", val);
	} else {
		pr_err("Failed to get IIO channel typec_conn_therm\n");
		return -ENODATA;
	}
}

static ssize_t connector_temp_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_xm_charger->chg_feature->fake_conn_temp = val;

	pr_err("fake_conn_temp = %d\n",
		g_xm_charger->chg_feature->fake_conn_temp);

	return count;
}
static CLASS_ATTR_RW(connector_temp);

static ssize_t authentic_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val = 0;
	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_BATTERY_AUTH, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t authentic_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	xm_battmngr_write_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_BATTERY_AUTH, val);

	return count;
}
static CLASS_ATTR_RW(authentic);

static ssize_t cc_orientation_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY, PD_TYPEC_CC_ORIENTATION, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t cc_orientation_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	xm_battmngr_write_iio_prop(g_battmngr_iio, PD_PHY, PD_TYPEC_CC_ORIENTATION, val);

	return count;
}
static CLASS_ATTR_RW(cc_orientation);

static ssize_t typec_mode_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY, PD_TYPEC_MODE, &val);

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_typec_mode_name(val));

}

static CLASS_ATTR_RO(typec_mode);

static ssize_t resistance_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val = 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static CLASS_ATTR_RO(resistance);


static ssize_t resistance_id_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_RESISTANCE_ID, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(resistance_id);

static ssize_t input_suspend_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	val = (get_client_vote(g_xm_charger->input_suspend_votable, USER_VOTER) == 1);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static ssize_t input_suspend_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	int rc;
	static int last_input_suspend;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	pr_err("input_suspend = %d\n", val);
	g_xm_charger->input_suspend = val;

	if (g_xm_charger->input_suspend != last_input_suspend) {
		cancel_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work));
		g_xm_charger->chg_feature->update_cont = 0;
		schedule_delayed_work(&(g_xm_charger->chg_feature->xm_prop_change_work), 0);
		last_input_suspend = g_xm_charger->input_suspend;
	}

	rc = vote(g_xm_charger->input_suspend_votable, USER_VOTER, (bool)val, 0);


	return count;
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t usb_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int real_type = 0;

	if (g_xm_charger->pd_active) {
		if (g_xm_charger->pd_active == QTI_POWER_SUPPLY_PD_ACTIVE)
			real_type = POWER_SUPPLY_USB_TYPE_PD;
		else if (g_xm_charger->pd_active == QTI_POWER_SUPPLY_PD_PPS_ACTIVE)
			real_type = POWER_SUPPLY_USB_TYPE_PD_PPS;
	} else {
		real_type = g_xm_charger->bc12_type;
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(real_type));
}
static CLASS_ATTR_RO(usb_real_type);

static ssize_t adapter_id_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	uint32_t adapter_id;

	xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY, PD_TYPEC_ADAPTER_ID, &adapter_id);
	pr_err("%s: adapter_id is %08x\n", __func__, adapter_id);

	return snprintf(buf, PAGE_SIZE, "%08x\n", adapter_id);
}
static CLASS_ATTR_RO(adapter_id);

static ssize_t quick_charge_type_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	val = xm_get_quick_charge_type();

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(quick_charge_type);

static ssize_t power_max_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	val = xm_get_adapter_power_max();

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(power_max);

static ssize_t shutdown_delay_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_SHUTDOWN_DELAY, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(shutdown_delay);

static ssize_t fg_temp_max_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TEMP_MAX, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(fg_temp_max);

static ssize_t fg_time_ot_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TIME_OT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(fg_time_ot);

static ssize_t fg_rsoc_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_RSOC, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(fg_rsoc);

static ssize_t fg_reg_rsoc_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_REG_ROC, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(fg_reg_rsoc);

static ssize_t smart_batt_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	val = g_xm_charger->smartBatVal;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static ssize_t smart_batt_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	int rc, mode;
	bool ffc_enable = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_FASTCHARGE_MODE, &mode);

	ffc_enable = mode;

	pr_err("Before process fv_profile:%d, non fv_profile:%d, ffc is: %d, Down value: %d.\n",g_xm_charger->dt.chg_voltage_max,g_xm_charger->dt.non_ffc_cv,
			ffc_enable,val);

	if(g_xm_charger->dt.chg_voltage_max < 0 || g_xm_charger->dt.non_ffc_cv < 0)
		return -EINVAL;

	if(val != 0) {
		if(!ffc_enable) {
			rc = vote(g_xm_charger->smart_batt_votable, SMART_BATTERY_FV, true, 0);
			rc = vote(g_xm_charger->fv_votable, SMART_BATTERY_FV, true, g_xm_charger->dt.non_ffc_cv - val*1000);
		}
		else {
			rc = vote(g_xm_charger->fv_votable, SMART_BATTERY_FV, false, 0);
			rc = vote(g_xm_charger->smart_batt_votable, SMART_BATTERY_FV, true, val);
		}
	} else {
		if(!ffc_enable)
			rc = vote(g_xm_charger->fv_votable, SMART_BATTERY_FV, false, 0);
		else
			rc = vote(g_xm_charger->smart_batt_votable, SMART_BATTERY_FV, true, 0);
	}

	g_xm_charger->smartBatVal = val;

	return count;
}
static CLASS_ATTR_RW(smart_batt);

static ssize_t night_charging_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	val = g_xm_charger->chg_feature->night_chg_flag;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static ssize_t night_charging_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_xm_charger->chg_feature->night_chg_flag = val;

	return count;
}
static CLASS_ATTR_RW(night_charging);

static ssize_t battery_input_suspend_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	val = (get_client_vote(g_xm_charger->fcc_votable, USER_VOTER) == 0);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static ssize_t battery_input_suspend_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val,rc;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = vote(g_xm_charger->fcc_votable, USER_VOTER, (bool)val, 0);
	if (rc < 0) {
		pr_err("%s: Couldn't vote to %s fcc rc=%d\n",__func__,
			(bool)val ? "suspend" : "resume", rc);
	}
	g_xm_charger->chg_feature->battery_input_suspend = val;

	return count;
}
static CLASS_ATTR_RW(battery_input_suspend);

static ssize_t set_icl_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_INPUT_CURRENT_SETTLED, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static ssize_t set_icl_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val,rc;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_INPUT_CURRENT_SETTLED, val);
	if (rc < 0) {
		pr_err("%s: Couldn't set icl\n",__func__);
	}

	return count;
}
static CLASS_ATTR_RW(set_icl);

static ssize_t vindpm_volt_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_INPUT_VOLTAGE_SETTLED, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static ssize_t vindpm_volt_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_INPUT_VOLTAGE_SETTLED, val);
	g_battmngr_noti->misc_msg.vindpm_temp = val;

	pr_err("%s: g_battmngr_noti->misc_msg.vindpm:%d\n", __func__, g_battmngr_noti->misc_msg.vindpm_temp);

	return count;
}
static CLASS_ATTR_RW(vindpm_volt);

static ssize_t disable_thermal_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int val;

	val = g_battmngr_noti->misc_msg.disable_thermal;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t disable_thermal_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val,rc;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_battmngr_noti->misc_msg.disable_thermal = val;
	pr_err("%s: disable_thermal:%d\n", __func__, g_battmngr_noti->misc_msg.disable_thermal);

	rc = xm_charger_thermal(g_xm_charger);
	if (rc < 0)
		pr_err("%s: Couldn't xm_charger_thermal\n", __func__);

	return count;
}
static CLASS_ATTR_RW(disable_thermal);

static struct attribute *battmngr_class_attrs[] = {
	&class_attr_real_type.attr,
	&class_attr_pd_verifed.attr,
	&class_attr_input_current.attr,
	&class_attr_cp_ibus_slave.attr,
	&class_attr_cp_ibus_master.attr,
	&class_attr_cp_present_slave.attr,
	&class_attr_cp_present_master.attr,
	&class_attr_cp_vbus_voltage.attr,
	&class_attr_cp_vbat_voltage.attr,
	&class_attr_vbus_voltage.attr,
	&class_attr_vbat_voltage.attr,
	&class_attr_cell_voltage.attr,
	&class_attr_soc_decimal.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_verify_digest.attr,
	&class_attr_chip_ok.attr,
	&class_attr_apdo_max.attr,
	&class_attr_fastchg_mode.attr,
	&class_attr_soh.attr,
	&class_attr_connector_temp.attr,
	&class_attr_authentic.attr,
	&class_attr_cc_orientation.attr,
	&class_attr_typec_mode.attr,
	&class_attr_resistance.attr,
	&class_attr_resistance_id.attr,
	&class_attr_input_suspend.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_adapter_id.attr,
	&class_attr_quick_charge_type.attr,
	&class_attr_power_max.attr,
	&class_attr_shutdown_delay.attr,
	&class_attr_fg_temp_max.attr,
	&class_attr_fg_time_ot.attr,
	&class_attr_cp_ibus_delta.attr,
	&class_attr_fg_rsoc.attr,
	&class_attr_smart_batt.attr,
	&class_attr_night_charging.attr,
	&class_attr_battery_input_suspend.attr,
	&class_attr_set_icl.attr,
	&class_attr_vindpm_volt.attr,
	&class_attr_disable_thermal.attr,
	&class_attr_fg_reg_rsoc.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battmngr_class);

int battmngr_class_init(struct xm_battmngr *battmngr)
{
	int rc = 0;

	if(!battmngr)
		return -EINVAL;

	battmngr->battmngr_class.name = "qcom-battery";
	battmngr->battmngr_class.class_groups = battmngr_class_groups;
	rc = class_register(&battmngr->battmngr_class);
	if (rc < 0) {
		pr_err("Failed to create battmngr_class rc=%d\n", rc);
	}

	return rc;
}

void battmngr_class_exit(struct xm_battmngr *battmngr)
{
	class_destroy(&battmngr->battmngr_class);
}

