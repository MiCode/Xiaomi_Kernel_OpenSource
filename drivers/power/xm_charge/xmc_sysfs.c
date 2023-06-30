/*******************************************************************************************
* Description:	XIAOMI-BSP-CHARGE
* 		This xmc_sysfs.c is the manager of file system, include usb/battery/bms power_supply.
*		For GKI, we create extra sysfs node. 
*		Usually use sysfs to report charge status up to Android.
* ------------------------------ Revision History: --------------------------------
* <version>	<date>		<author>			<desc>
* 1.0		2022-02-22	chenyichun@xiaomi.com		Created for new architecture
********************************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "xmc_core.h"

#define MAX_UEVENT_PROP_NUM	20

#define XMC_SYSFS_FIELD_RW(_name, _prop, _report)			\
{									\
	.attr	= __ATTR(_name, 0644, xmc_sysfs_show, xmc_sysfs_store),	\
	.prop	= _prop,						\
	.report_uevent = _report,					\
}

#define XMC_SYSFS_FIELD_RO(_name, _prop, _report)			\
{									\
	.attr	= __ATTR(_name, 0444, xmc_sysfs_show, NULL),		\
	.prop	= _prop,				  		\
	.report_uevent = _report,					\
}

struct xmc_sysfs_info {
	struct device_attribute attr;
	enum xmc_sysfs_prop prop;
	bool report_uevent;
};

/* must in same order with xmc_sysfs_prop */
static const char * const xmc_sysfs_prop_name[] = {
	/* /sys/power_supply/usb */
	[POWER_SUPPLY_PROP_REAL_TYPE]		= "POWER_SUPPLY_REAL_TYPE",
	[POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE]	= "POWER_SUPPLY_QUICK_CHARGE_TYPE",
	[POWER_SUPPLY_PROP_TYPEC_MODE]		= "POWER_SUPPLY_TYPEC_MODE",
	[POWER_SUPPLY_PROP_CONNECTOR_TEMP]	= "POWER_SUPPLY_CONNECTOR_TEMP",
	[POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION]= "POWER_SUPPLY_TYPEC_CC_ORIENTATION",
	[POWER_SUPPLY_PROP_PD_AUTHENTICATION]	= "POWER_SUPPLY_PD_AUTHENTICATION",
	[POWER_SUPPLY_PROP_APDO_MAX]		= "POWER_SUPPLY_APDO_MAX",
	[POWER_SUPPLY_PROP_POWER_MAX]		= "POWER_SUPPLY_POWER_MAX",
	[POWER_SUPPLY_PROP_FFC_ENABLE]		= "POWER_SUPPLY_FFC_ENABLE",
	[POWER_SUPPLY_PROP_INPUT_SUSPEND_USB]	= "POWER_SUPPLY_INPUT_SUSPEND_USB",
	[POWER_SUPPLY_PROP_MTBF_TEST]		= "POWER_SUPPLY_MTBF_TEST",

	/* /sys/power_supply/battery */
	[POWER_SUPPLY_PROP_INPUT_SUSPEND_BATTERY]	= "POWER_SUPPLY_INPUT_SUSPEND_BATTERY",
	[POWER_SUPPLY_PROP_NIGHT_CHARGING]	= "POWER_SUPPLY_NIGHT_CHARGING",
	[POWER_SUPPLY_PROP_SMART_BATT]		= "POWER_SUPPLY_SMART_BATT",

	/* /sys/power_supply/bms */
	[POWER_SUPPLY_PROP_FASTCHARGE_MODE]	= "POWER_SUPPLY_FASTCHARGE_MODE",
	[POWER_SUPPLY_PROP_RESISTANCE_ID]	= "POWER_SUPPLY_RESISTANCE_ID",
	[POWER_SUPPLY_PROP_SOC_DECIMAL]		= "POWER_SUPPLY_SOC_DECIMAL",
	[POWER_SUPPLY_PROP_SOC_DECIMAL_RATE]	= "POWER_SUPPLY_SOC_DECIMAL_RATE",
	[POWER_SUPPLY_PROP_SHUTDOWN_DELAY]	= "POWER_SUPPLY_SHUTDOWN_DELAY",
	[POWER_SUPPLY_PROP_SHUTDOWN_MODE]	= "POWER_SUPPLY_SHUTDOWN_MODE",
	[POWER_SUPPLY_PROP_SOH]			= "POWER_SUPPLY_SOH",
	[POWER_SUPPLY_PROP_RM]			= "POWER_SUPPLY_RM",
	[POWER_SUPPLY_PROP_FCC]			= "POWER_SUPPLY_FCC",
	[POWER_SUPPLY_PROP_MAX_TEMP]		= "POWER_SUPPLY_TEMP_MAX",
	[POWER_SUPPLY_PROP_TIME_OT]		= "POWER_SUPPLY_TIME_OT",
	[POWER_SUPPLY_PROP_QMAX0]		= "POWER_SUPPLY_QMAX0",
	[POWER_SUPPLY_PROP_QMAX1]		= "POWER_SUPPLY_QMAX1",
	[POWER_SUPPLY_PROP_TRUE_REM_Q]		= "POWER_SUPPLY_TRUE_REM_Q",
	[POWER_SUPPLY_PROP_INITIAL_Q]		= "POWER_SUPPLY_INITIAL_Q",
	[POWER_SUPPLY_PROP_TRUE_FULL_CHG_Q]	= "POWER_SUPPLY_TRUE_FULL_CHG_Q",
	[POWER_SUPPLY_PROP_T_SIM]		= "POWER_SUPPLY_T_SIM",
	[POWER_SUPPLY_PROP_CELL_GRID]		= "POWER_SUPPLY_CELL_GRID",
	[POWER_SUPPLY_PROP_CHIP_OK]			= "POWER_SUPPLY_PROP_CHIP_OK",
	[POWER_SUPPLY_PROP_RSOC]			= "POWER_SUPPLY_PROP_RSOC",
	[POWER_SUPPLY_PROP_CAPACITY_RAW]			= "POWER_SUPPLY_PROP_CAPACITY_RAW",
};

static const char * const bc12_type_text[] = {
	[XMC_BC12_TYPE_NONE]	= "Unknown",
	[XMC_BC12_TYPE_SDP]	= "SDP",
	[XMC_BC12_TYPE_DCP]	= "DCP",
	[XMC_BC12_TYPE_CDP]	= "CDP",
	[XMC_BC12_TYPE_OCP]	= "OCP",
	[XMC_BC12_TYPE_FLOAT]	= "USB_FLOAT",
};

static const char * const qc_type_text[] = {
	[XMC_QC_TYPE_NONE]		= "Unknown",
	[XMC_QC_TYPE_HVCHG]		= "HVCHG",
	[XMC_QC_TYPE_HVDCP]		= "USB_HVDCP",
	[XMC_QC_TYPE_HVDCP_2]		= "USB_HVDCP_2",
	[XMC_QC_TYPE_HVDCP_3]		= "USB_HVDCP_3",
	[XMC_QC_TYPE_HVDCP_3_18W]	= "USB_HVDCP_3_18W",
	[XMC_QC_TYPE_HVDCP_3_27W]	= "USB_HVDCP_3_27W",
	[XMC_QC_TYPE_HVDCP_3P5]		= "USB_HVDCP_3P5",
};

static const char * const pd_type_text[] = {
	[XMC_PD_TYPE_NONE]	= "Unknown",
	[XMC_PD_TYPE_PD2]	= "USB_PD2",
	[XMC_PD_TYPE_PD3]	= "USB_PD",
	[XMC_PD_TYPE_PPS]	= "USB_PD",
};

static const char * const typec_mode_text[] = {
	[POWER_SUPPLY_TYPEC_NONE]			= "Nothing attached",
	[POWER_SUPPLY_TYPEC_SINK]			= "Sink attached",
	[POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE]		= "Powered cable w/ sink",
	[POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY]	= "Debug Accessory",
	[POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER]		= "Audio Adapter",
	[POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY]		= "Powered cable w/o sink",
	[POWER_SUPPLY_TYPEC_SOURCE_DEFAULT]		= "Source attached (default current)",
	[POWER_SUPPLY_TYPEC_SOURCE_MEDIUM]		= "Source attached (medium current)",
	[POWER_SUPPLY_TYPEC_SOURCE_HIGH]		= "Source attached (high current)",
	[POWER_SUPPLY_TYPEC_NON_COMPLIANT]		= "Non compliant",
};

static int xmc_get_charge_status(struct charge_chip *chip)
{
	int charge_status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (chip->battery.shutdown_flag)
		return charge_status;

	if (chip->usb_typec.bc12_type || chip->usb_typec.qc_type || chip->usb_typec.pd_type || chip->fake_charger_present) {
		if (chip->usb_typec.cmd_input_suspend) {
			charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			if (chip->charge_full)
				charge_status = POWER_SUPPLY_STATUS_FULL;
			else
				charge_status = POWER_SUPPLY_STATUS_CHARGING;
		}
	} else {
		charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	return charge_status;
}

static int xmc_get_battery_health(struct charge_chip *chip)
{
	union power_supply_propval pval = {0,};
	int battery_health = POWER_SUPPLY_HEALTH_GOOD;

	power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_TEMP, &pval);

	if (pval.intval <= -100)
		battery_health = POWER_SUPPLY_HEALTH_COLD;
	else if (pval.intval <= 150)
		battery_health = POWER_SUPPLY_HEALTH_COOL;
	else if (pval.intval <= 480)
		battery_health = POWER_SUPPLY_HEALTH_GOOD;
	else if (pval.intval <= 520)
		battery_health = POWER_SUPPLY_HEALTH_WARM;
	else if (pval.intval < 600)
		battery_health = POWER_SUPPLY_HEALTH_HOT;
	else
		battery_health = POWER_SUPPLY_HEALTH_OVERHEAT;

	return battery_health;
}

static int xmc_get_quick_charge_type(struct charge_chip *chip)
{
	if (chip->battery.tbat < 0)
		return QUICK_CHARGE_NORMAL;

	if (chip->usb_typec.pd_type == XMC_PD_TYPE_PPS && (chip->adapter.adapter_svid == USBPD_MI_SVID || chip->adapter.adapter_svid == 0x2B01)) {
		if (chip->adapter.apdo_max >= 50)
			return QUICK_CHARGE_SUPER;
		else
			return QUICK_CHARGE_TURBE;
	}

	if (chip->usb_typec.qc_type == XMC_QC_TYPE_HVDCP_3_27W)
		return QUICK_CHARGE_FLASH;
	else if (chip->usb_typec.qc_type || chip->usb_typec.pd_type)
		return QUICK_CHARGE_FAST;
	else if (chip->usb_typec.qc_type)
		return QUICK_CHARGE_NORMAL;
	else
		return QUICK_CHARGE_NORMAL;
}

static int xmc_get_soc_decimal(struct charge_chip *chip)
{
	return chip->battery.rawsoc % 100;
}

static int xmc_get_soc_decimal_rate(struct charge_chip *chip)
{
	int rsoc = 0, i = 0;

	rsoc = chip->battery.rsoc;

	if (chip->battery.dec_rate_len <= 0)
		return 0;

	for (i = 0; i < chip->battery.dec_rate_len; i += 2) {
		if (rsoc < chip->battery.dec_rate_seq[i])
			return chip->battery.dec_rate_seq[i - 1];
	}

	return chip->battery.dec_rate_seq[chip->battery.dec_rate_len - 1];
}

static int xmc_shutdown_delay(struct charge_chip *chip)
{
	union power_supply_propval pval = {0, };
	static bool last_shutdown_delay = false;
	int result = 0;

	if (chip->battery.uisoc == 0) {
		power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_STATUS, &pval);
		if (pval.intval == POWER_SUPPLY_STATUS_CHARGING) {
			chip->battery.shutdown_delay = false;
			if (chip->battery.vbat > chip->battery.critical_shutdown_vbat)
				result = 1;
			else
				chip->battery.shutdown_flag = true;
		} else {
			if (chip->battery.vbat > chip->battery.normal_shutdown_vbat) {
				chip->battery.shutdown_delay = false;
				result = 1;
			} else if (chip->battery.vbat > chip->battery.critical_shutdown_vbat) {
				chip->battery.shutdown_delay = true;
				result = 1;
			} else {
				chip->battery.shutdown_delay = false;
				chip->battery.shutdown_flag = true;
				result = 0;
			}
		}
	} else {
		chip->battery.shutdown_delay = false;
		result = chip->battery.uisoc;
	}

	if (chip->battery.shutdown_flag)
		result = 0;

	if (last_shutdown_delay != chip->battery.shutdown_delay || result == 0) {
		last_shutdown_delay = chip->battery.shutdown_delay;
		xmc_sysfs_report_uevent(chip->bms_psy);
	}

	return result;
}

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TYPE,
};

static int battery_get_prop(struct power_supply *psy, enum power_supply_property prop, union power_supply_propval *val)
{
	struct charge_chip *chip = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = xmc_get_charge_status(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = xmc_get_battery_health(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (chip->battery.fake_vbat)
			val->intval = chip->battery.fake_vbat;
		else
			power_supply_get_property(chip->bms_psy, prop, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = chip->sic_current * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		val->intval = chip->thermal_level;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (chip->battery.fake_uisoc) {
			val->intval = chip->battery.fake_uisoc;
		} else {
			if (chip->init_done)
				val->intval = xmc_shutdown_delay(chip);
			else
				power_supply_get_property(chip->bms_psy, prop, val);
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chip->battery.fake_tbat != 8888)
			val->intval = chip->battery.fake_tbat;
		else
			power_supply_get_property(chip->bms_psy, prop, val);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = psy->desc->type;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		power_supply_get_property(chip->bms_psy, prop, val);
		break;
	default:
		xmc_err("battery unsupported property %d\n", prop);
		return -EINVAL;
	}

	return ret;
}

static int battery_set_prop(struct power_supply *psy, enum power_supply_property psp, const union power_supply_propval *val)
{
	struct charge_chip *chip = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		chip->battery.fake_vbat = val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (val->intval > 0)
			chip->sic_current = cut_cap(val->intval / 1000, 300, chip->thermal_limit[THERMAL_TABLE_NUM - 1][0]);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (val->intval < 0 || chip->thermal_level < 0) {
			if (val->intval <= -THERMAL_LEVEL_NUM - 1)
				chip->thermal_level = 0;
			else if (val->intval < 0)
				chip->thermal_level = val->intval;
		} else if (val->intval <= THERMAL_LEVEL_NUM - 1) {
			chip->thermal_level = val->intval;
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->battery.fake_uisoc = val->intval;
		power_supply_changed(chip->battery_psy);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		chip->battery.fake_tbat = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int battery_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int ret = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_TEMP:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static const struct power_supply_desc battery_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = battery_props,
	.num_properties = ARRAY_SIZE(battery_props),
	.get_property = battery_get_prop,
	.set_property = battery_set_prop,
	.property_is_writeable = battery_prop_is_writeable,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TYPE,
};

static int usb_get_prop(struct power_supply *psy, enum power_supply_property prop, union power_supply_propval *val)
{
	struct charge_chip *chip = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (chip->usb_typec.bc12_type || chip->usb_typec.qc_type || chip->usb_typec.pd_type || chip->fake_charger_present);
		if (chip->battery.shutdown_flag)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (chip->bbc.vbus_disable)
			xmc_ops_get_vbus(chip->master_cp_dev, &val->intval);
		else
			xmc_ops_get_vbus(chip->bbc_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		xmc_ops_get_ibus(chip->bbc_dev, &val->intval);
		if (chip->pdm.master_cp_enable)
			val->intval += chip->pdm.master_cp_ibus;
		if (chip->pdm.slave_cp_enable)
			val->intval += chip->pdm.slave_cp_ibus;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		if (!(chip->usb_typec.bc12_type || chip->usb_typec.qc_type || chip->usb_typec.pd_type) && chip->fake_charger_present) {
			val->intval = POWER_SUPPLY_TYPE_USB;
			break;
		}
		if (chip->usb_typec.bc12_type == XMC_BC12_TYPE_SDP || chip->usb_typec.bc12_type == XMC_BC12_TYPE_CDP || chip->usb_typec.pd_type)
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	default:
		xmc_err("usb unsupported property %d\n", prop);
		return -EINVAL;
	}

	return ret;
}

static int usb_set_prop(struct power_supply *psy, enum power_supply_property psp, const union power_supply_propval *val)
{
//	struct charge_chip *chip = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	default:
		return -EINVAL;
	}

	return ret;
}

static int usb_prop_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	int ret = 0;

	switch (prop) {
	default:
		ret = 0;
		break;
	}
	return ret;
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = usb_props,
	.num_properties = ARRAY_SIZE(usb_props),
	.get_property = usb_get_prop,
	.set_property = usb_set_prop,
	.property_is_writeable = usb_prop_is_writeable,
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
};

static int ac_get_prop(struct power_supply *psy, enum power_supply_property prop, union power_supply_propval *val)
{
	struct charge_chip *chip = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (chip->usb_typec.bc12_type == XMC_BC12_TYPE_DCP || chip->usb_typec.bc12_type == XMC_BC12_TYPE_OCP ||
			chip->usb_typec.bc12_type == XMC_BC12_TYPE_FLOAT || chip->usb_typec.qc_type || chip->usb_typec.pd_type);
		if (chip->battery.shutdown_flag)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = psy->desc->type;
		break;
	default:
		xmc_err("ac unsupported property %d\n", prop);
		return -EINVAL;
	}

	return ret;
}

static const struct power_supply_desc ac_psy_desc = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = ac_props,
	.num_properties = ARRAY_SIZE(ac_props),
	.get_property = ac_get_prop,
};

int xmc_sysfs_get_property(struct power_supply *psy, enum xmc_sysfs_prop prop, union power_supply_propval *val)
{
	struct charge_chip *chip = NULL;

	if (psy)
		chip = power_supply_get_drvdata(psy);

	if (!psy || !chip) {
		xmc_err("%s failed to get charge_chip\n", psy->desc->name);
		return -ENODEV;
	}

	switch (prop) {
	/* /sys/power_supply/usb */
	case POWER_SUPPLY_PROP_REAL_TYPE:
		if (chip->usb_typec.pd_type)
			val->strval = pd_type_text[chip->usb_typec.pd_type];
		else if (chip->usb_typec.qc_type)
			val->strval = qc_type_text[chip->usb_typec.qc_type];
		else if (chip->usb_typec.bc12_type)
			val->strval = bc12_type_text[chip->usb_typec.bc12_type];
		else
			val->strval = "Unknown";
		break;
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		val->intval = xmc_get_quick_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		val->strval = typec_mode_text[chip->usb_typec.typec_mode];
		break;
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
		val->intval = chip->usb_typec.fake_temp ? chip->usb_typec.fake_temp : chip->usb_typec.temp;
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		val->intval = chip->usb_typec.cc_orientation;
		break;
	case POWER_SUPPLY_PROP_PD_AUTHENTICATION:
		val->intval = chip->adapter.authenticate_success;
		break;
	case POWER_SUPPLY_PROP_APDO_MAX:
	case POWER_SUPPLY_PROP_POWER_MAX:
		val->intval = chip->adapter.apdo_max;
		break;
	case POWER_SUPPLY_PROP_FFC_ENABLE:
		val->intval = chip->ffc_enable;
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND_USB:
		val->intval = chip->usb_typec.cmd_input_suspend;
		break;
	case POWER_SUPPLY_PROP_MTBF_TEST:
		val->intval = chip->mtbf_test;
		break;

	/* /sys/power_supply/battery */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND_BATTERY:
		val->intval = chip->usb_typec.cmd_input_suspend;
		break;
	case POWER_SUPPLY_PROP_NIGHT_CHARGING:
		val->intval = chip->night_charging;
		break;
	case POWER_SUPPLY_PROP_SMART_BATT:
		val->intval = chip->smart_fv_shift;
		break;

	/* /sys/power_supply/bms */
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		val->intval = 100000;
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL:
		val->intval = xmc_get_soc_decimal(chip);
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL_RATE:
		val->intval = xmc_get_soc_decimal_rate(chip);
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		val->intval = chip->battery.shutdown_delay;
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_MODE:
		val->intval = chip->battery.shutdown_mode;
		break;
	case POWER_SUPPLY_PROP_SOH:
		val->intval = chip->battery.soh ? 95 : chip->battery.soh;
		break;
	case POWER_SUPPLY_PROP_RM:
		val->intval = chip->battery.rm;
		break;
	case POWER_SUPPLY_PROP_FCC:
		val->intval = chip->battery.fcc;
		break;
	case POWER_SUPPLY_PROP_MAX_TEMP:
		xmc_ops_get_gauge_temp_max(chip->gauge_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_OT:
		xmc_ops_get_gauge_time_ot(chip->gauge_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_QMAX0:
		xmc_ops_get_gauge_qmax(chip->gauge_dev, &val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_QMAX1:
		xmc_ops_get_gauge_qmax(chip->gauge_dev, &val->intval, 1);
		break;
	case POWER_SUPPLY_PROP_TRUE_REM_Q:
		xmc_ops_get_gauge_true_rem_q(chip->gauge_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_INITIAL_Q:
		xmc_ops_get_gauge_initial_q(chip->gauge_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TRUE_FULL_CHG_Q:
		xmc_ops_get_gauge_true_full_chg_q(chip->gauge_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_T_SIM:
		xmc_ops_get_gauge_t_sim(chip->gauge_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CELL_GRID:
		xmc_ops_get_gauge_cell_grid(chip->gauge_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		val->intval = chip->battery.chip_ok;
		break;
	case POWER_SUPPLY_PROP_RSOC:
		xmc_ops_get_rsoc(chip->gauge_dev, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		val->intval = chip->battery.rm*10000/chip->battery.fcc;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(xmc_sysfs_get_property);

int xmc_sysfs_set_property(struct power_supply *psy, enum xmc_sysfs_prop prop, const union power_supply_propval *val)
{
	struct charge_chip *chip = NULL;

	if (psy)
		chip = power_supply_get_drvdata(psy);

	if (!psy || !chip) {
		xmc_err("%s failed to get charge_chip\n", psy->desc->name);
		return -ENODEV;
	}

	switch (prop) {
	/* /sys/power_supply/usb */
	case POWER_SUPPLY_PROP_CONNECTOR_TEMP:
		chip->usb_typec.fake_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND_USB:
		chip->usb_typec.cmd_input_suspend = val->intval;
		break;
	case POWER_SUPPLY_PROP_MTBF_TEST:
		chip->mtbf_test = val->intval;
		break;

	/* /sys/power_supply/battery */
	case POWER_SUPPLY_PROP_INPUT_SUSPEND_BATTERY:
		chip->usb_typec.cmd_input_suspend = val->intval;
		break;
	case POWER_SUPPLY_PROP_NIGHT_CHARGING:
		chip->night_charging = val->intval;
		break;
	case POWER_SUPPLY_PROP_SMART_BATT:
		chip->smart_fv_shift = val->intval;
		break;

	/* /sys/power_supply/bms */
	case POWER_SUPPLY_PROP_SHUTDOWN_MODE:
		if (val->intval == 1)
			xmc_ops_set_gauge_shutdown_mode(chip->gauge_dev);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(xmc_sysfs_set_property);

static ssize_t xmc_sysfs_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy = NULL;
	struct xmc_sysfs_info *usb_attr;
	union power_supply_propval pval = {0,};
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	pval.intval = val;

	psy = dev_get_drvdata(dev);
	if (!psy) {
		xmc_err("invalid psy");
		return ret;
	}

	usb_attr = container_of(attr, struct xmc_sysfs_info, attr);
	xmc_sysfs_set_property(psy, usb_attr->prop, &pval);

	return count;
}

static ssize_t xmc_sysfs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = NULL;
	struct xmc_sysfs_info *usb_attr;
	union power_supply_propval pval = {0,};
	ssize_t count = 0;

	psy = dev_get_drvdata(dev);
	if (!psy) {
		xmc_err("invalid psy");
		return count;
	}

	usb_attr = container_of(attr, struct xmc_sysfs_info, attr);
	xmc_sysfs_get_property(psy, usb_attr->prop, &pval);
	if (usb_attr->prop == POWER_SUPPLY_PROP_REAL_TYPE || usb_attr->prop == POWER_SUPPLY_PROP_TYPEC_MODE)
		count = scnprintf(buf, PAGE_SIZE, "%s\n", pval.strval);
	else
		count = scnprintf(buf, PAGE_SIZE, "%d\n", pval.intval);

	return count;
}

static struct xmc_sysfs_info bms_sysfs_field_tbl[] = {
	XMC_SYSFS_FIELD_RO(fastcharge_mode, POWER_SUPPLY_PROP_FASTCHARGE_MODE, false),
	XMC_SYSFS_FIELD_RO(resistance_id, POWER_SUPPLY_PROP_RESISTANCE_ID, false),
	XMC_SYSFS_FIELD_RO(soc_decimal, POWER_SUPPLY_PROP_SOC_DECIMAL, true),
	XMC_SYSFS_FIELD_RO(soc_decimal_rate, POWER_SUPPLY_PROP_SOC_DECIMAL_RATE, true),
	XMC_SYSFS_FIELD_RO(shutdown_delay, POWER_SUPPLY_PROP_SHUTDOWN_DELAY, true),
	XMC_SYSFS_FIELD_RW(shutdown_mode, POWER_SUPPLY_PROP_SHUTDOWN_MODE, false),
	XMC_SYSFS_FIELD_RO(soh, POWER_SUPPLY_PROP_SOH, false),
	XMC_SYSFS_FIELD_RO(rm, POWER_SUPPLY_PROP_RM, false),
	XMC_SYSFS_FIELD_RO(fcc, POWER_SUPPLY_PROP_FCC, false),
	XMC_SYSFS_FIELD_RO(temp_max, POWER_SUPPLY_PROP_MAX_TEMP, false),
	XMC_SYSFS_FIELD_RO(time_ot, POWER_SUPPLY_PROP_TIME_OT, false),
	XMC_SYSFS_FIELD_RO(qmax0, POWER_SUPPLY_PROP_QMAX0, false),
	XMC_SYSFS_FIELD_RO(qmax1, POWER_SUPPLY_PROP_QMAX1, false),
	XMC_SYSFS_FIELD_RO(true_rem_q, POWER_SUPPLY_PROP_TRUE_REM_Q, false),
	XMC_SYSFS_FIELD_RO(initial_q, POWER_SUPPLY_PROP_INITIAL_Q, false),
	XMC_SYSFS_FIELD_RO(true_full_chg_q, POWER_SUPPLY_PROP_TRUE_FULL_CHG_Q, false),
	XMC_SYSFS_FIELD_RO(t_sim, POWER_SUPPLY_PROP_T_SIM, false),
	XMC_SYSFS_FIELD_RO(cell_grid, POWER_SUPPLY_PROP_CELL_GRID, false),
	XMC_SYSFS_FIELD_RO(chip_ok, POWER_SUPPLY_PROP_CHIP_OK, false),
	XMC_SYSFS_FIELD_RO(rsoc, POWER_SUPPLY_PROP_RSOC, false),
	XMC_SYSFS_FIELD_RO(raw_soc, POWER_SUPPLY_PROP_CAPACITY_RAW, false),
};

static struct attribute * bms_sysfs_attrs[ARRAY_SIZE(bms_sysfs_field_tbl) + 1];

static const struct attribute_group bms_sysfs_attr_group = {
	.attrs = bms_sysfs_attrs,
};

static struct xmc_sysfs_info usb_sysfs_field_tbl[] = {
	XMC_SYSFS_FIELD_RO(real_type, POWER_SUPPLY_PROP_REAL_TYPE, true),
	XMC_SYSFS_FIELD_RO(quick_charge_type, POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE, true),
	XMC_SYSFS_FIELD_RO(typec_mode, POWER_SUPPLY_PROP_TYPEC_MODE, false),
	XMC_SYSFS_FIELD_RW(connector_temp, POWER_SUPPLY_PROP_CONNECTOR_TEMP, false),
	XMC_SYSFS_FIELD_RO(typec_cc_orientation, POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION, false),
	XMC_SYSFS_FIELD_RO(pd_authentication, POWER_SUPPLY_PROP_PD_AUTHENTICATION, false),
	XMC_SYSFS_FIELD_RO(apdo_max, POWER_SUPPLY_PROP_APDO_MAX, true),
	XMC_SYSFS_FIELD_RO(power_max, POWER_SUPPLY_PROP_POWER_MAX, true),
	XMC_SYSFS_FIELD_RO(ffc_enable, POWER_SUPPLY_PROP_FFC_ENABLE, false),
	XMC_SYSFS_FIELD_RW(input_suspend, POWER_SUPPLY_PROP_INPUT_SUSPEND_USB, false),
	XMC_SYSFS_FIELD_RW(mtbf_test, POWER_SUPPLY_PROP_MTBF_TEST, false),
};

static struct attribute * usb_sysfs_attrs[ARRAY_SIZE(usb_sysfs_field_tbl) + 1];

static const struct attribute_group usb_sysfs_attr_group = {
	.attrs = usb_sysfs_attrs,
};

static struct xmc_sysfs_info battery_sysfs_field_tbl[] = {
	XMC_SYSFS_FIELD_RW(input_suspend, POWER_SUPPLY_PROP_INPUT_SUSPEND_BATTERY, false),
	XMC_SYSFS_FIELD_RW(night_charging, POWER_SUPPLY_PROP_NIGHT_CHARGING, false),
	XMC_SYSFS_FIELD_RW(smart_batt, POWER_SUPPLY_PROP_SMART_BATT, false),
};

static struct attribute * battery_sysfs_attrs[ARRAY_SIZE(battery_sysfs_field_tbl) + 1];

static const struct attribute_group battery_sysfs_attr_group = {
	.attrs = battery_sysfs_attrs,
};

static int xmc_sysfs_create_group(struct charge_chip *chip)
{
	int i = 0, limit = 0, rc = 0;

	limit = ARRAY_SIZE(battery_sysfs_field_tbl);
	for (i = 0; i < limit; i++)
		battery_sysfs_attrs[i] = &battery_sysfs_field_tbl[i].attr.attr;

	battery_sysfs_attrs[limit] = NULL;
	rc = sysfs_create_group(&chip->battery_psy->dev.kobj, &battery_sysfs_attr_group);
	if (rc) {
		xmc_err("[XMC_PROBE] failed to create battery_sysfs\n");
		return rc;
	}

	limit = ARRAY_SIZE(usb_sysfs_field_tbl);
	for (i = 0; i < limit; i++)
		usb_sysfs_attrs[i] = &usb_sysfs_field_tbl[i].attr.attr;

	usb_sysfs_attrs[limit] = NULL;
	rc = sysfs_create_group(&chip->usb_psy->dev.kobj, &usb_sysfs_attr_group);
	if (rc) {
		xmc_err("[XMC_PROBE] failed to create usb_sysfs\n");
		return rc;
	}

	limit = ARRAY_SIZE(bms_sysfs_field_tbl);
	for (i = 0; i < limit; i++)
		bms_sysfs_attrs[i] = &bms_sysfs_field_tbl[i].attr.attr;

	bms_sysfs_attrs[limit] = NULL;
	rc = sysfs_create_group(&chip->bms_psy->dev.kobj, &bms_sysfs_attr_group);
	if (rc) {
		xmc_err("[XMC_PROBE] failed to create bms_sysfs\n");
		return rc;
	}

	return rc;
}

int xmc_sysfs_report_uevent(struct power_supply *psy)
{
	struct charge_chip *chip = NULL;
	struct xmc_sysfs_info *table = NULL;
	union power_supply_propval pval = {0,};
	char *envp[MAX_UEVENT_PROP_NUM] = { NULL };
	int table_len = 0, report_num = 0, i = 0, ret = 0;

	chip = power_supply_get_drvdata(psy);
	if (!chip) {
		xmc_err("%s failed to get charge_chip\n", psy->desc->name);
		goto out;
	}

	if (!strcmp(psy->desc->name, "usb")) {
		table = kcalloc(ARRAY_SIZE(usb_sysfs_field_tbl), sizeof(*table), GFP_KERNEL);
		if (!table) {
			xmc_err("%s failed to calloc usb sysfs table\n");
			goto out;
		}
		memcpy(table, usb_sysfs_field_tbl, sizeof(usb_sysfs_field_tbl));
		table_len = ARRAY_SIZE(usb_sysfs_field_tbl);
	} else if (!strcmp(psy->desc->name, "battery")) {
		table = kcalloc(ARRAY_SIZE(battery_sysfs_field_tbl), sizeof(*table), GFP_KERNEL);
		if (!table) {
			xmc_err("%s failed to calloc battery sysfs table\n");
			goto out;
		}
		memcpy(table, battery_sysfs_field_tbl, sizeof(battery_sysfs_field_tbl));
		table_len = ARRAY_SIZE(battery_sysfs_field_tbl);
	} else if (!strcmp(psy->desc->name, "bms")) {
		table = kcalloc(ARRAY_SIZE(bms_sysfs_field_tbl), sizeof(*table), GFP_KERNEL);
		if (!table) {
			xmc_err("%s failed to calloc bms sysfs table\n");
			goto out;
		}
		memcpy(table, bms_sysfs_field_tbl, sizeof(bms_sysfs_field_tbl));
		table_len = ARRAY_SIZE(bms_sysfs_field_tbl);
	} else {
		xmc_err("don't support report this uevent\n");
		goto out;
	}

	envp[0] = kasprintf(GFP_KERNEL, "POWER_SUPPLY_NAME=%s", psy->desc->name);

	for (i = 0; i < table_len; i++) {
		if (!table[i].report_uevent)
			continue;
		report_num++;
		if (report_num > MAX_UEVENT_PROP_NUM - 2) {
			xmc_err("uevent number over MAX range\n");
			goto out;
		}

		xmc_sysfs_get_property(psy, table[i].prop, &pval);
		if (table[i].prop == POWER_SUPPLY_PROP_REAL_TYPE || table[i].prop == POWER_SUPPLY_PROP_TYPEC_MODE)
			envp[report_num] = kasprintf(GFP_KERNEL, "%s=%s", xmc_sysfs_prop_name[table[i].prop], pval.strval);
		else
			envp[report_num] = kasprintf(GFP_KERNEL, "%s=%d", xmc_sysfs_prop_name[table[i].prop], pval.intval);
	}

	envp[report_num + 1] = NULL;
	ret = kobject_uevent_env(&chip->dev->kobj, KOBJ_CHANGE, envp);

out:
	kfree(table);
	for (i = 0; i < MAX_UEVENT_PROP_NUM; i++)
		kfree(envp[i]);

	return ret;
}
EXPORT_SYMBOL(xmc_sysfs_report_uevent);

bool xmc_sysfs_init(struct charge_chip *chip)
{
	struct power_supply_config psy_cfg = {};
	int rc = 0;

	if (!chip->master_psy) {
		chip->master_psy = power_supply_get_by_name("cp_master");
		if (!chip->master_psy) {
			xmc_err("[XMC_PROBE] failed to get master_psy\n");
			return false;
		}
	}

	if (!chip->slave_psy) {
		chip->slave_psy = power_supply_get_by_name("cp_slave");
		if (!chip->slave_psy) {
			xmc_err("[XMC_PROBE] failed to get slave_psy\n");
			return false;
		}
	}

	if (!chip->bbc_psy) {
		chip->bbc_psy = power_supply_get_by_name("bbc");
		if (!chip->bbc_psy) {
			xmc_err("[XMC_PROBE] failed to get bbc_psy\n");
			return false;
		}
	}

	if (!chip->bms_psy) {
		chip->bms_psy = power_supply_get_by_name("bms");
		if (!chip->bms_psy) {
			xmc_err("[XMC_PROBE] failed to get bms_psy\n");
			return false;
		}
	}

	psy_cfg.drv_data = chip;
	psy_cfg.of_node = chip->dev->of_node;

	chip->battery_psy = devm_power_supply_register(chip->dev, &battery_psy_desc, &psy_cfg);
	if (IS_ERR(chip->battery_psy)) {
		xmc_err("[XMC_PROBE] failed to register battery_psy\n");
		return false;
	}

	chip->usb_psy = devm_power_supply_register(chip->dev, &usb_psy_desc, &psy_cfg);
	if (IS_ERR(chip->usb_psy)) {
		xmc_err("[XMC_PROBE] failed to register usb_psy\n");
		return false;
	}

	chip->ac_psy = devm_power_supply_register(chip->dev, &ac_psy_desc, &psy_cfg);
	if (IS_ERR(chip->ac_psy)) {
		xmc_err("[XMC_PROBE] failed to register ac_psy\n");
		return false;
	}

	rc = xmc_sysfs_create_group(chip);
	if (rc) {
		xmc_err("[XMC_PROBE] failed to create xmc_sysfs\n");
		return false;
	}

	return true;
}
