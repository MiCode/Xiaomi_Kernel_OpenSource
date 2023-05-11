#define pr_fmt(fmt) "nopmi_chg_common %s: " fmt, __func__

#include <linux/usb/typec/maxim/max77729_usbc.h>
#include "nopmi_chg_common.h"

static NOPMI_CHARGER_IC_TYPE nopmi_charger_ic = NOPMI_CHARGER_IC_NONE;
extern struct max77729_usbc_platform_data *g_usbc_data;
extern int adapter_dev_get_pd_verified(void);

int nopmi_chg_is_usb_present(struct power_supply *usb_psy)
{
	union power_supply_propval prop = {0, };
	int ret = 0;
	int usb_present;

	if(!usb_psy)
	{
		usb_psy = power_supply_get_by_name("usb");
		if (!usb_psy) {
			pr_err("usb supply not found, defer probe\n");
			return -EINVAL;
		}
	}

	ret = power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &prop);
	if (ret < 0) {
		pr_err("couldn't read usb_present property, ret=%d\n", ret);
		return -EINVAL;
	}
	usb_present = prop.intval;

	return usb_present;

}

char nopmi_set_charger_ic_type(NOPMI_CHARGER_IC_TYPE nopmi_type)
{
	char ret = 0;

	if(NOPMI_CHARGER_IC_MAX > nopmi_type && NOPMI_CHARGER_IC_NONE < nopmi_type)
	{
		nopmi_charger_ic = nopmi_type;
	}
	else
	{
		ret = -ENODEV;
	}
	pr_err("2012.09.04 wsy %s: start nopmi_type=%d nopmi_charger_ic=%d\n", __func__, nopmi_type, nopmi_charger_ic);
	return ret;
}

NOPMI_CHARGER_IC_TYPE nopmi_get_charger_ic_type(void)
{
	return nopmi_charger_ic;
}

EXPORT_SYMBOL_GPL(nopmi_set_charger_ic_type);
EXPORT_SYMBOL_GPL(nopmi_get_charger_ic_type);

int nopmi_set_charge_enable(bool en)
{
	int ret = 0;
	switch(nopmi_get_charger_ic_type())
	{
		case NOPMI_CHARGER_IC_MAXIM:
			//need maxim port
			break;
		case NOPMI_CHARGER_IC_SYV:
			ret = main_set_charge_enable(en);
			break;
		case NOPMI_CHARGER_IC_SC:
			break;
		default:
			break;
	}

	return ret;
}

struct quick_charge adapter_cap[10] = {
	{ POWER_SUPPLY_TYPE_USB,        QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_ACA,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP,    QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_WIRELESS,     QUICK_CHARGE_FAST },
	{0, 0},
};

int nopmi_get_quick_charge_type(struct power_supply *usb_psy)
{
	union power_supply_propval prop = {0, };
	int i = 0;
	int ret = 0;
	enum power_supply_type chg_type;
	struct power_supply *batt_psy;
	int pd_verifed;
	static bool is_single_flash = false;

	if(!usb_psy)
	{
		usb_psy = power_supply_get_by_name("usb");
		if (!usb_psy) {
			pr_err("usb supply not found, defer probe\n");
			return -EINVAL;
		}
	}

	ret = power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_REAL_TYPE, &prop);
	if (ret < 0) {
		pr_err("couldn't read usb real type, ret=%d\n", ret);
		return -EINVAL;
	}
	chg_type = prop.intval;

	if(!batt_psy)
	{
		batt_psy = power_supply_get_by_name("battery");
		if(!batt_psy){
			pr_err("battery supply not found, defer probe\n");
			return -EINVAL;
		}
	}

	ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_TEMP, &prop);
	if (ret < 0) {
		pr_err("couldn't read batt temp property, ret=%d\n", ret);
		return -EINVAL;
	}

	pr_info("battery temp: %d", prop.intval);
	if (prop.intval < 50 || prop.intval >= 480) {
		if (usb_psy && !is_single_flash){
			pr_info("battery temp is under 5 or above 48");
			power_supply_changed(usb_psy);
		}
		is_single_flash = true;
		return 0;
	} else {
		if (usb_psy && is_single_flash){
			pr_info("battery temp returned to normal");
			power_supply_changed(usb_psy);
		}
		is_single_flash = false;
	}

	if (NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
		pd_verifed = g_usbc_data->verifed;
	else if (NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type())
		pd_verifed = adapter_dev_get_pd_verified();
	else
		pd_verifed = 1;

	if ((chg_type == POWER_SUPPLY_TYPE_USB_PD) && pd_verifed) {
		return QUICK_CHARGE_TURBE;
	}

	while (adapter_cap[i].adap_type != 0) {
		if (chg_type == adapter_cap[i].adap_type) {
			return adapter_cap[i].adap_cap;
		}
		i++;
	}

	return 0;
}




