
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

	val = g_xm_charger->pd_verifed;
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t pd_verifed_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_xm_charger->pd_verifed = val;
	pr_err("pd_verifed = %d\n", g_xm_charger->pd_verifed);

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

static ssize_t cp_present_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE, CHARGE_PUMP_LN_PRESENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_present_slave);

static ssize_t cp_present_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER, CHARGE_PUMP_LN_PRESENT, &val);

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

static ssize_t vbus_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, MAIN_CHG, MAIN_CHARGER_BUS_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(vbus_voltage);

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

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_SOC_DECIMAL, &val);

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(soc_decimal);

static ssize_t soc_decimal_rate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_SOC_DECIMAL_RATE, &val);
	if(val > 100 || val < 0)
		val = 0;

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

static ssize_t fastcharge_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_FASTCHARGE_MODE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(fastcharge_mode);

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

	val = (get_client_vote(g_xm_charger->icl_votable, USER_VOTER) == 0);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static ssize_t input_suspend_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	int rc;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = vote(g_xm_charger->icl_votable, USER_VOTER, (bool)val, 0);

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

static struct attribute *battmngr_class_attrs[] = {
	&class_attr_real_type.attr,
	&class_attr_pd_verifed.attr,
	&class_attr_input_current.attr,
	&class_attr_cp_ibus_slave.attr,
	&class_attr_cp_ibus_master.attr,
	&class_attr_cp_present_slave.attr,
	&class_attr_cp_present_master.attr,
	&class_attr_cp_vbus_voltage.attr,
	&class_attr_vbus_voltage.attr,
	&class_attr_cell_voltage.attr,
	&class_attr_soc_decimal.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_verify_digest.attr,
	&class_attr_chip_ok.attr,
	&class_attr_apdo_max.attr,
	&class_attr_fastcharge_mode.attr,
	&class_attr_soh.attr,
	&class_attr_connector_temp.attr,
	&class_attr_authentic.attr,
	&class_attr_cc_orientation.attr,
	&class_attr_resistance_id.attr,
	&class_attr_input_suspend.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_adapter_id.attr,
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

