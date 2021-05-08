/*
 *  Sysfs interface for the universal power supply monitor class
 *
 *  Copyright © 2007  David Woodhouse <dwmw2@infradead.org>
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/stat.h>

#include "power_supply.h"

/*
 * This is because the name "current" breaks the device attr macro.
 * The "current" word resolves to "(get_current())" so instead of
 * "current" "(get_current())" appears in the sysfs.
 *
 * The source of this definition is the device.h which calls __ATTR
 * macro in sysfs.h which calls the __stringify macro.
 *
 * Only modification that the name is not tried to be resolved
 * (as a macro let's say).
 */

#define POWER_SUPPLY_ATTR(_name)					\
{									\
	.attr = { .name = #_name },					\
	.show = power_supply_show_property,				\
	.store = power_supply_store_property,				\
}

static struct device_attribute power_supply_attrs[];

static const char * const power_supply_type_text[] = {
	"Unknown", "Battery", "UPS", "Mains", "USB","USB_FLOAT",
	"USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "USB_PD_DRP", "BrickID",
	"USB_HVDCP", "USB_HVDCP_3", "USB_HVDCP_3P5", "Wireless",
	"Bms", "Charger_Identify", "Batt_verify"
};

static const char * const power_supply_status_text[] = {
	"Unknown", "Charging", "Discharging", "Not charging", "Full",
	"Cmd discharging"
};

static const char * const power_supply_charge_type_text[] = {
	"Unknown", "N/A", "Trickle", "Fast"
};

static const char * const power_supply_health_text[] = {
	"Unknown", "Good", "Overheat", "Dead", "Over voltage",
	"Unspecified failure", "Cold", "Watchdog timer expire",
	"Safety timer expire", "Over current", "Warm", "Cool", "Hot"
};

static const char * const power_supply_technology_text[] = {
	"Unknown", "NiMH", "Li-ion", "Li-poly", "LiFe", "NiCd",
	"LiMn"
};

static const char * const power_supply_capacity_level_text[] = {
	"Unknown", "Critical", "Low", "Normal", "High", "Full"
};

static const char * const power_supply_scope_text[] = {
	"Unknown", "System", "Device"
};

static const char * const typec_text[] = {
	"Nothing attached", "Sink attached", "Non compliant",
	"Debug Accessory", "Sink attached", "Powered cable w/o sink",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Non compliant",
};

static const char * const power_supply_usbc_pr_text[] = {
	"Unknown", "sink only", "source only", "dual-role ports",
	"try source", "try sink", "wrong role"
};

static ssize_t power_supply_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf) {
	ssize_t ret = 0;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;

	if (off == POWER_SUPPLY_PROP_TYPE) {
		value.intval = psy->desc->type;
	} else {
		ret = power_supply_get_property(psy, off, &value);

		if (ret < 0) {
			if (ret == -ENODATA)
				dev_dbg(dev, "driver has no data for `%s' property\n",
					attr->attr.name);
			else if (ret != -ENODEV && ret != -EAGAIN)
				dev_err_ratelimited(dev,
					"driver failed to report `%s' property: %zd\n",
					attr->attr.name, ret);
			return ret;
		}
	}

	if (off == POWER_SUPPLY_PROP_STATUS)
		return sprintf(buf, "%s\n",
			       power_supply_status_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CHARGE_TYPE)
		return sprintf(buf, "%s\n",
			       power_supply_charge_type_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_HEALTH)
		return sprintf(buf, "%s\n",
			       power_supply_health_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TECHNOLOGY)
		return sprintf(buf, "%s\n",
			       power_supply_technology_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CAPACITY_LEVEL)
		return sprintf(buf, "%s\n",
			       power_supply_capacity_level_text[value.intval]);
	else if ((off == POWER_SUPPLY_PROP_TYPE) ||
				(off == POWER_SUPPLY_PROP_REAL_TYPE))
		return sprintf(buf, "%s\n",
			       power_supply_type_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TYPEC_MODE)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			       typec_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_SCOPE)
		return sprintf(buf, "%s\n",
			       power_supply_scope_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TYPEC_POWER_ROLE)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			       power_supply_usbc_pr_text[value.intval]);
#ifdef CONFIG_XMUSB350_DET_CHG
	else if (off == POWER_SUPPLY_PROP_QC35_VID)
		return scnprintf(buf, PAGE_SIZE, "%02x,%02x,%02x,%02x\n",
			       value.arrayval[0], value.arrayval[1], value.arrayval[2], value.arrayval[3]);
	else if (off == POWER_SUPPLY_PROP_QC35_VERSION)
		return scnprintf(buf, PAGE_SIZE, "%02x,%02x\n",
			       value.arrayval[0], value.arrayval[1]);
#endif
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	else if ((off == POWER_SUPPLY_PROP_ROMID) || (off == POWER_SUPPLY_PROP_DS_STATUS))
		return scnprintf(buf, PAGE_SIZE, "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
			value.arrayval[0], value.arrayval[1], value.arrayval[2], value.arrayval[3],
			value.arrayval[4], value.arrayval[5], value.arrayval[6], value.arrayval[7]);
	else if ((off == POWER_SUPPLY_PROP_PAGE0_DATA) ||
			(off == POWER_SUPPLY_PROP_PAGE1_DATA) ||
			(off == POWER_SUPPLY_PROP_PAGEDATA))
		return scnprintf(buf, PAGE_SIZE, "%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
			value.arrayval[0], value.arrayval[1], value.arrayval[2], value.arrayval[3],
			value.arrayval[4], value.arrayval[5], value.arrayval[6], value.arrayval[7],
			value.arrayval[8], value.arrayval[9], value.arrayval[10], value.arrayval[11],
			value.arrayval[12], value.arrayval[13], value.arrayval[14], value.arrayval[15]);
	else if (off == POWER_SUPPLY_PROP_VERIFY_MODEL_NAME)
		return sprintf(buf, "%s\n", value.strval);
#endif
	else if (off >= POWER_SUPPLY_PROP_MODEL_NAME)
		return sprintf(buf, "%s\n", value.strval);

	if (off == POWER_SUPPLY_PROP_CHARGE_COUNTER_EXT)
		return sprintf(buf, "%lld\n", value.int64val);
	else
		return sprintf(buf, "%d\n", value.intval);
}

static ssize_t power_supply_store_property(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count) {
	ssize_t ret;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;

	/* maybe it is a enum property? */
	switch (off) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = sysfs_match_string(power_supply_status_text, buf);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = sysfs_match_string(power_supply_charge_type_text, buf);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = sysfs_match_string(power_supply_health_text, buf);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = sysfs_match_string(power_supply_technology_text, buf);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = sysfs_match_string(power_supply_capacity_level_text, buf);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		ret = sysfs_match_string(power_supply_scope_text, buf);
		break;
	default:
		ret = -EINVAL;
	}

	/*
	 * If no match was found, then check to see if it is an integer.
	 * Integer values are valid for enums in addition to the text value.
	 */
	if (ret < 0) {
		long long_val;

		ret = kstrtol(buf, 10, &long_val);
		if (ret < 0)
			return ret;

		ret = long_val;
	}

	value.intval = ret;

	ret = power_supply_set_property(psy, off, &value);
	if (ret < 0)
		return ret;

	return count;
}

/* Must be in the same order as POWER_SUPPLY_PROP_* */
static struct device_attribute power_supply_attrs[] = {
	/* Properties of type `int' */
	POWER_SUPPLY_ATTR(status),
	POWER_SUPPLY_ATTR(charge_type),
	POWER_SUPPLY_ATTR(health),
	POWER_SUPPLY_ATTR(present),
	POWER_SUPPLY_ATTR(online),
	POWER_SUPPLY_ATTR(authentic),
	POWER_SUPPLY_ATTR(technology),
	POWER_SUPPLY_ATTR(cycle_count),
	POWER_SUPPLY_ATTR(voltage_max),
	POWER_SUPPLY_ATTR(voltage_min),
	POWER_SUPPLY_ATTR(voltage_max_design),
	POWER_SUPPLY_ATTR(voltage_min_design),
	POWER_SUPPLY_ATTR(voltage_now),
	POWER_SUPPLY_ATTR(voltage_avg),
	POWER_SUPPLY_ATTR(voltage_ocv),
	POWER_SUPPLY_ATTR(voltage_boot),
	POWER_SUPPLY_ATTR(current_max),
	POWER_SUPPLY_ATTR(current_now),
	POWER_SUPPLY_ATTR(current_avg),
	POWER_SUPPLY_ATTR(current_boot),
	POWER_SUPPLY_ATTR(power_now),
	POWER_SUPPLY_ATTR(power_avg),
	POWER_SUPPLY_ATTR(charge_full_design),
	POWER_SUPPLY_ATTR(charge_empty_design),
	POWER_SUPPLY_ATTR(charge_full),
	POWER_SUPPLY_ATTR(charge_empty),
	POWER_SUPPLY_ATTR(charge_now),
	POWER_SUPPLY_ATTR(charge_avg),
	POWER_SUPPLY_ATTR(charge_counter),
	POWER_SUPPLY_ATTR(constant_charge_current),
	POWER_SUPPLY_ATTR(constant_charge_current_max),
	POWER_SUPPLY_ATTR(constant_charge_voltage),
	POWER_SUPPLY_ATTR(constant_charge_voltage_max),
	POWER_SUPPLY_ATTR(charge_control_limit),
	POWER_SUPPLY_ATTR(charge_control_limit_max),
	POWER_SUPPLY_ATTR(fast_charge_current),
	POWER_SUPPLY_ATTR(thermal_input_current),
	POWER_SUPPLY_ATTR(input_current_now),
	POWER_SUPPLY_ATTR(input_current_limit),
	POWER_SUPPLY_ATTR(energy_full_design),
	POWER_SUPPLY_ATTR(energy_empty_design),
	POWER_SUPPLY_ATTR(energy_full),
	POWER_SUPPLY_ATTR(energy_empty),
	POWER_SUPPLY_ATTR(energy_now),
	POWER_SUPPLY_ATTR(energy_avg),
	POWER_SUPPLY_ATTR(capacity),
	POWER_SUPPLY_ATTR(capacity_alert_min),
	POWER_SUPPLY_ATTR(capacity_alert_max),
	POWER_SUPPLY_ATTR(capacity_level),
	POWER_SUPPLY_ATTR(soc_decimal),
	POWER_SUPPLY_ATTR(soc_decimal_rate),
	POWER_SUPPLY_ATTR(mtk_soc_decimal_rate),
	POWER_SUPPLY_ATTR(capacity_raw),
	POWER_SUPPLY_ATTR(temp),
	/* 2021.02.04 longcheer jiangshitian change for pd-chg and main-cam thermal begin */
	#if defined(CONFIG_TARGET_PROJECT_K7B)
	POWER_SUPPLY_ATTR(main_cam_temp),
	POWER_SUPPLY_ATTR(pd_charger_temp),
	POWER_SUPPLY_ATTR(set_temp_enable),
	POWER_SUPPLY_ATTR(set_temp_num),
	#endif
	/* 2021.02.04 longcheer jiangshitian change for pd-chg and main-cam thermal end */
	POWER_SUPPLY_ATTR(temp_max),
	POWER_SUPPLY_ATTR(temp_min),
	POWER_SUPPLY_ATTR(temp_connect),
	POWER_SUPPLY_ATTR(temp_alert_min),
	POWER_SUPPLY_ATTR(temp_alert_max),
	POWER_SUPPLY_ATTR(temp_ambient),
	POWER_SUPPLY_ATTR(temp_ambient_alert_min),
	POWER_SUPPLY_ATTR(temp_ambient_alert_max),
	POWER_SUPPLY_ATTR(time_to_empty_now),
	POWER_SUPPLY_ATTR(time_to_empty_avg),
	POWER_SUPPLY_ATTR(time_to_full_now),
	POWER_SUPPLY_ATTR(time_to_full_avg),
	POWER_SUPPLY_ATTR(type),
	POWER_SUPPLY_ATTR(scope),
	POWER_SUPPLY_ATTR(precharge_current),
	POWER_SUPPLY_ATTR(charge_term_current),
	POWER_SUPPLY_ATTR(calibrate),
	POWER_SUPPLY_ATTR(fastcharge_mode),
	POWER_SUPPLY_ATTR(real_type),
	POWER_SUPPLY_ATTR(hvdcp3_type),
	POWER_SUPPLY_ATTR(quick_charge_type),
	POWER_SUPPLY_ATTR(type_recheck),
	POWER_SUPPLY_ATTR(pd_verify_in_process),
#ifdef CONFIG_MTBF_SUPPORT
	POWER_SUPPLY_ATTR(mtbf_current),
#endif
#ifdef CONFIG_BQ2597X_CHARGE_PUMP
	/* Bq charge pump properties */
	POWER_SUPPLY_ATTR(dp_dm_bq),
	POWER_SUPPLY_ATTR(ti_bus_error_status),
	POWER_SUPPLY_ATTR(ti_charge_mode),
	POWER_SUPPLY_ATTR(ti_bypass_mode_enable),
	POWER_SUPPLY_ATTR(ti_battery_present),
	POWER_SUPPLY_ATTR(ti_vbus_present),
	POWER_SUPPLY_ATTR(ti_battery_voltage),
	POWER_SUPPLY_ATTR(ti_battery_current),
	POWER_SUPPLY_ATTR(ti_battery_temperature),
	POWER_SUPPLY_ATTR(ti_bus_voltage),
	POWER_SUPPLY_ATTR(ti_bus_current),
	POWER_SUPPLY_ATTR(ti_bus_temperature),
	POWER_SUPPLY_ATTR(ti_die_temperature),
	POWER_SUPPLY_ATTR(ti_alarm_status),
	POWER_SUPPLY_ATTR(ti_fault_status),
	POWER_SUPPLY_ATTR(ti_reg_status),
	POWER_SUPPLY_ATTR(ti_set_bus_protection_for_qc3),
	POWER_SUPPLY_ATTR(bq_charge_done),
	POWER_SUPPLY_ATTR(hv_charge_enable),
	POWER_SUPPLY_ATTR(pd_active),
	POWER_SUPPLY_ATTR(pd_authentication),
	POWER_SUPPLY_ATTR(pd_type),
#endif
#ifdef CONFIG_XMUSB350_DET_CHG
	POWER_SUPPLY_ATTR(qc35_chg_type),
	POWER_SUPPLY_ATTR(qc35_error_state),
	POWER_SUPPLY_ATTR(qc35_vin),
	POWER_SUPPLY_ATTR(qc35_version),
	POWER_SUPPLY_ATTR(qc35_vid),
	POWER_SUPPLY_ATTR(qc35_chip_ok),
	POWER_SUPPLY_ATTR(qc35_rerun_apsd),
	POWER_SUPPLY_ATTR(qc35_soft_reset),
	POWER_SUPPLY_ATTR(qc35_mode_select),
	POWER_SUPPLY_ATTR(qc35_intb_enable),
	POWER_SUPPLY_ATTR(qc35_hvdcp_enable),
	POWER_SUPPLY_ATTR(qc35_bc12_enable),
	POWER_SUPPLY_ATTR(qc35_sleep_enable),
	POWER_SUPPLY_ATTR(qc35_hvdcp_dpdm),
	POWER_SUPPLY_ATTR(qc35_detect_done),
#endif
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	/* battery verify properties */
	POWER_SUPPLY_ATTR(romid),
	POWER_SUPPLY_ATTR(ds_status),
	POWER_SUPPLY_ATTR(pagenumber),
	POWER_SUPPLY_ATTR(pagedata),
	POWER_SUPPLY_ATTR(authen_result),
	POWER_SUPPLY_ATTR(session_seed),
	POWER_SUPPLY_ATTR(s_secret),
	POWER_SUPPLY_ATTR(challenge),
	POWER_SUPPLY_ATTR(auth_anon),
	POWER_SUPPLY_ATTR(auth_bdconst),
	POWER_SUPPLY_ATTR(page0_data),
	POWER_SUPPLY_ATTR(page1_data),
	POWER_SUPPLY_ATTR(verify_model_name),
	POWER_SUPPLY_ATTR(chip_ok),
	POWER_SUPPLY_ATTR(maxim_batt_cycle_count),
#endif
	/* Local extensions */
	POWER_SUPPLY_ATTR(usb_hc),
	POWER_SUPPLY_ATTR(usb_otg),
	POWER_SUPPLY_ATTR(charge_enabled),
	POWER_SUPPLY_ATTR(charging_enabled),
	POWER_SUPPLY_ATTR(typec_mode),
	POWER_SUPPLY_ATTR(typec_cc_orientation),
	POWER_SUPPLY_ATTR(typec_power_role),
	POWER_SUPPLY_ATTR(resistance),
	POWER_SUPPLY_ATTR(resistance_id),
	POWER_SUPPLY_ATTR(input_suspend),
	POWER_SUPPLY_ATTR(connector_temp),
	POWER_SUPPLY_ATTR(vbus_disable),
	/* Local extensions of type int64_t */
	POWER_SUPPLY_ATTR(charge_counter_ext),
	/* Properties of type `const char *' */
	POWER_SUPPLY_ATTR(model_name),
	POWER_SUPPLY_ATTR(manufacturer),
	POWER_SUPPLY_ATTR(serial_number),
	POWER_SUPPLY_ATTR(battery_type),
};

static struct attribute *
__power_supply_attrs[ARRAY_SIZE(power_supply_attrs) + 1];

static umode_t power_supply_attr_is_visible(struct kobject *kobj,
					   struct attribute *attr,
					   int attrno)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct power_supply *psy = dev_get_drvdata(dev);
	umode_t mode = S_IRUSR | S_IRGRP | S_IROTH;
	int i;

	if (attrno == POWER_SUPPLY_PROP_TYPE)
		return mode;

	for (i = 0; i < psy->desc->num_properties; i++) {
		int property = psy->desc->properties[i];

		if (property == attrno) {
			if (psy->desc->property_is_writeable &&
			    psy->desc->property_is_writeable(psy, property) > 0)
				mode |= S_IWUSR;

			return mode;
		}
	}

	return 0;
}

static struct attribute_group power_supply_attr_group = {
	.attrs = __power_supply_attrs,
	.is_visible = power_supply_attr_is_visible,
};

static const struct attribute_group *power_supply_attr_groups[] = {
	&power_supply_attr_group,
	NULL,
};

void power_supply_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = power_supply_attr_groups;

	for (i = 0; i < ARRAY_SIZE(power_supply_attrs); i++)
		__power_supply_attrs[i] = &power_supply_attrs[i].attr;
}

static char *kstruprdup(const char *str, gfp_t gfp)
{
	char *ret, *ustr;

	ustr = ret = kmalloc(strlen(str) + 1, gfp);

	if (!ret)
		return NULL;

	while (*str)
		*ustr++ = toupper(*str++);

	*ustr = 0;

	return ret;
}

int power_supply_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	int ret = 0, j;
	char *prop_buf;
	char *attrname;

	if (!psy || !psy->desc) {
		dev_dbg(dev, "No power supply yet\n");
		return ret;
	}

	ret = add_uevent_var(env, "POWER_SUPPLY_NAME=%s", psy->desc->name);
	if (ret)
		return ret;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return -ENOMEM;

	for (j = 0; j < psy->desc->num_properties; j++) {
		struct device_attribute *attr;
		char *line;

		attr = &power_supply_attrs[psy->desc->properties[j]];

		ret = power_supply_show_property(dev, attr, prop_buf);
		if (ret == -ENODEV || ret == -ENODATA) {
			/* When a battery is absent, we expect -ENODEV. Don't abort;
			   send the uevent with at least the the PRESENT=0 property */
			ret = 0;
			continue;
		}

		if (ret < 0)
			goto out;

		line = strchr(prop_buf, '\n');
		if (line)
			*line = 0;

		attrname = kstruprdup(attr->attr.name, GFP_KERNEL);
		if (!attrname) {
			ret = -ENOMEM;
			goto out;
		}

		ret = add_uevent_var(env, "POWER_SUPPLY_%s=%s", attrname, prop_buf);
		kfree(attrname);
		if (ret)
			goto out;
	}

out:
	free_page((unsigned long)prop_buf);

	return ret;
}
