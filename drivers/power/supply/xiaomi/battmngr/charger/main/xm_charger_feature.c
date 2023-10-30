
#include <linux/battmngr/xm_charger_core.h>
#include <linux/battmngr/xm_battery_core.h>

typedef void (*usb_host_cb)(void);
usb_host_cb stop_usb_host_cb = NULL;
usb_host_cb start_usb_host_cb = NULL;

void stop_usb_host_cb_set(usb_host_cb cb)
{
	stop_usb_host_cb = cb;
	return;
}
EXPORT_SYMBOL(stop_usb_host_cb_set);

void start_usb_host_cb_set(usb_host_cb cb)
{
	start_usb_host_cb = cb;
	return;
}
EXPORT_SYMBOL(start_usb_host_cb_set);

static int xm_set_vbus_disable(struct chg_feature_info *chip, bool disable)
{
	int ret = 0;
	static bool conn_therm_flag = false;

	charger_err("%s: set vbus disable:%d\n", __func__, disable);

	if (disable && !conn_therm_flag) {
		conn_therm_flag = true;
		gpio_set_value(chip->vbus_ctrl_gpio, 1);
		if (stop_usb_host_cb)
			stop_usb_host_cb();
	} else if (!disable && conn_therm_flag) {
		conn_therm_flag = false;
		gpio_set_value(chip->vbus_ctrl_gpio, 0);
		if (start_usb_host_cb)
			start_usb_host_cb();
	}

	chip->vbus_disable = disable;

	return ret;
}

int typec_conn_therm_start_stop(struct xm_charger *charger,
			struct chg_feature_info *info)
{
	charger_err("%s\n", __func__);

	if (g_xm_charger->bc12_active || g_xm_charger->pd_active || g_xm_charger->otg_enable) {
		cancel_delayed_work_sync(&info->typec_conn_therm_work);
		schedule_delayed_work(&info->typec_conn_therm_work,
			msecs_to_jiffies(CONN_THERM_DELAY_5S));
	} else {
		if (!info->vbus_disable)
			cancel_delayed_work_sync(&info->typec_conn_therm_work);
	}

	return 0;
}

static void typec_conn_therm_work(struct work_struct *work)
{
	struct chg_feature_info *chip = container_of(work,
			struct chg_feature_info, typec_conn_therm_work.work);
	//union power_supply_propval pval = {0, };
	int ret = 0;

	if (!g_battmngr_iio->typec_conn_therm) {
		xm_get_iio_channel(g_battmngr_iio, "typec_conn_therm",
				&g_battmngr_iio->typec_conn_therm);
	}

	if (g_battmngr_iio->typec_conn_therm) {
		ret = iio_read_channel_processed(g_battmngr_iio->typec_conn_therm,
				&chip->connector_temp);
		if (ret < 0) {
		    charger_err("Couldn't read connector_temp, rc=%d\n", ret);
		    return;
		}
		chip->connector_temp /= 100;
	} else {
		charger_err("Failed to get IIO channel typec_conn_therm\n");
		return;
	}

	if (chip->fake_conn_temp != 0)
		chip->connector_temp = chip->fake_conn_temp;

	if (chip->connector_temp >= CONN_THERM_TOOHIG_70DEC)
		xm_set_vbus_disable(chip, true);
	else if (chip->connector_temp <
			(CONN_THERM_TOOHIG_70DEC - CONN_THERM_HYS_2DEC))
		xm_set_vbus_disable(chip, false);
/*
	ret = power_supply_get_property(g_xm_charger->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		charger_err("%s: Get usb online failed, rc=%d\n",
					__func__, ret);
		return;
	}
*/
	schedule_delayed_work(&chip->typec_conn_therm_work,
		msecs_to_jiffies(CONN_THERM_DELAY_5S));
	charger_err("%s: fake_conn_temp = %d, connector_temp = %d\n",
			__func__, chip->fake_conn_temp, chip->connector_temp);

	return;
}

int xm_get_adapter_power_max(void)
{
	int val;
	int apdo_max = 0;
	int apdo_max_volt, apdo_max_curr;

	if (g_xm_charger->input_suspend == 1)
		return 0;

	xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY, PD_APDO_VOLT_MAX, &apdo_max_volt);
	xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY, PD_APDO_CURR_MAX, &apdo_max_curr);
	apdo_max = (apdo_max_volt * apdo_max_curr) / 1000000;
	charger_err("apdo_max:%d\n", apdo_max);

	if (apdo_max == 65)
		val = APDO_MAX_65W; /* only for J1 65W adapter */
	else if (apdo_max >= 60)
		val = APDO_MAX_67W;
	else if (apdo_max >= 55 && apdo_max < 60)
		val = APDO_MAX_55W;
	else if (apdo_max >= 50 && apdo_max < 55)
		val = APDO_MAX_50W;
	else
		val = apdo_max;

	return val;
}
EXPORT_SYMBOL(xm_get_adapter_power_max);

int xm_get_soc_decimal(void)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_SOC_DECIMAL, &val);

	return val;
}
EXPORT_SYMBOL(xm_get_soc_decimal);

int xm_get_soc_decimal_rate(void)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_SOC_DECIMAL_RATE, &val);
	if(val > 100 || val < 0)
		val = 0;

	return val;
}
EXPORT_SYMBOL(xm_get_soc_decimal_rate);

struct quick_charge adapter_cap[11] = {
	{ POWER_SUPPLY_USB_TYPE_SDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_ACA,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP,    QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3P5,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_WIRELESS,     QUICK_CHARGE_FAST },
	{0, 0},
};
int xm_get_quick_charge_type(void)
{
	int val, i=0;
	int power_max = 0;
	int real_charger_type;

	if (g_xm_charger->pd_active) {
			real_charger_type = POWER_SUPPLY_USB_TYPE_PD;
	} else {
		real_charger_type = g_xm_charger->bc12_type;
	}

	charger_err("real charger type : %d\n ", real_charger_type);

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_TEMP, &val);
	if ((val >= BATT_WARM_THRESHOLD) || (val < LIGHTING_ICON_CHANGE)) {
		charger_err("battery temp is under 5 or above 48\n");
		return QUICK_CHARGE_NORMAL;
	}
	else if ((real_charger_type == POWER_SUPPLY_USB_TYPE_PD) && g_xm_charger->pd_verified) {
		power_max = xm_get_adapter_power_max();
		charger_err("power_max : %d\n ", power_max);

		if (power_max >= 50)
			return QUICK_CHARGE_SUPER;
		else
			return QUICK_CHARGE_TURBE;
	}
	else if (real_charger_type == POWER_SUPPLY_USB_TYPE_PD) {
		return QUICK_CHARGE_FAST;
	}
	else {
		while (adapter_cap[i].adap_type != 0) {
			if (real_charger_type == adapter_cap[i].adap_type) {
				return adapter_cap[i].adap_cap;
			}
			i++;
		}
	}

	return 0;
}
EXPORT_SYMBOL(xm_get_quick_charge_type);

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uvent(struct work_struct *work)
{
	int count;
	struct chg_feature_info *chip = container_of(work,
			struct chg_feature_info, xm_prop_change_work.work);


	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SOC_DECIMAL=\n",	//length=31+8
		"POWER_SUPPLY_SOC_DECIMAL_RATE=\n",	//length=31+8
		"POWER_SUPPLY_QUICK_CHARGE_TYPE=\n",
	};
	static char *envp[] = {
		uevent_string[0],
		uevent_string[1],
		uevent_string[2],
		NULL,
	};
	char *prop_buf = NULL;

	count = chip->update_cont;
	if(chip->update_cont < 0)
		return;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return;

	scnprintf(prop_buf, PAGE_SIZE, "%u", xm_get_soc_decimal());
	strncpy(uevent_string[0]+25, prop_buf,MAX_UEVENT_LENGTH-25);

	scnprintf(prop_buf, PAGE_SIZE, "%u", xm_get_soc_decimal_rate());
	strncpy(uevent_string[1]+30, prop_buf,MAX_UEVENT_LENGTH-30);

	scnprintf(prop_buf, PAGE_SIZE, "%u", xm_get_quick_charge_type());
	strncpy(uevent_string[2]+31, prop_buf,MAX_UEVENT_LENGTH-31);

	charger_err("uevent test : %s, %s, %s, count %d\n",
			envp[0], envp[1], envp[2], count);

	/*add our prop end*/

	kobject_uevent_env(&chip->dev->kobj, KOBJ_CHANGE, envp);

	free_page((unsigned long)prop_buf);
	chip->update_cont = count - 1;
	schedule_delayed_work(&chip->xm_prop_change_work, msecs_to_jiffies(CONN_THERM_DELAY_2S));
	return;
}

static int xm_get_prop_battery_input_suspend(void)
{
	int val;
	val = (get_client_vote(g_xm_charger->fcc_votable, USER_VOTER) == 0);
	return val;
}

static int xm_set_prop_battery_input_suspend(int val)
{
	int rc;

	/* vote 0mA when battery suspended */
	rc = vote(g_xm_charger->fcc_votable, USER_VOTER, (bool)val, 0);
	if (rc < 0) {
		battery_err("%s: Couldn't vote to %s fcc rc=%d\n",__func__,
			(bool)val ? "suspend" : "resume", rc);
		return rc;
	}
	g_xm_charger->chg_feature->battery_input_suspend = val;

	power_supply_changed(g_xm_charger->batt_psy);
	return rc;
}

static void night_chargig_change_work(struct work_struct *work)
{
	struct chg_feature_info *chip = container_of(work,
			struct chg_feature_info, night_chargig_change_work.work);

	static int pre_night_chg_flag = 0;
	int capacity, battery_input_suspend;
	int rc = 0, val = 0;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CAPACITY, &capacity);
	battery_input_suspend = xm_get_prop_battery_input_suspend();
	battery_err("%s: capacity:%d, battery_input_suspend:%d.\n", __func__,
			capacity, battery_input_suspend);
	battery_err("%s: pre_nchg:%d, nchg:%d.\n", __func__,
			pre_night_chg_flag, g_xm_charger->chg_feature->night_chg_flag);

	if (pre_night_chg_flag != g_xm_charger->chg_feature->night_chg_flag) {
		if (g_xm_charger->chg_feature->night_chg_flag && capacity >= 80) {
			val = 1;
			rc = xm_set_prop_battery_input_suspend(val);
			battery_err("%s: open night charging.\n", __func__);
			pre_night_chg_flag = g_xm_charger->chg_feature->night_chg_flag;
		}
	}

	if (battery_input_suspend &&
			(!g_xm_charger->chg_feature->night_chg_flag || capacity <= 75)) {
		val = 0;
		xm_set_prop_battery_input_suspend(val);
		battery_err("%s: close night charging.\n", __func__);
		pre_night_chg_flag = 0;
	}

	schedule_delayed_work(&chip->night_chargig_change_work, msecs_to_jiffies(CONN_THERM_DELAY_10S));
	return;
}

int night_charging_start_stop(struct xm_charger *charger,
			struct chg_feature_info *chip)
{
	charger_err("%s\n", __func__);

	if (g_xm_charger->bc12_active || g_xm_charger->pd_active) {
		cancel_delayed_work_sync(&chip->night_chargig_change_work);
		schedule_delayed_work(&chip->night_chargig_change_work, 0);
	} else {
		cancel_delayed_work_sync(&chip->night_chargig_change_work);
	}

	return 0;
}

static int xm_chg_feature_parse_dt(struct device *dev,
				struct chg_feature_info *chip)
{
	int ret = 0;
	struct device_node *np = dev->of_node;

	chip->vbus_ctrl_gpio = of_get_named_gpio(np, "vbus_ctrl_gpio", 0);
	if (chip->vbus_ctrl_gpio < 0) {
		charger_err("%s no vbus_ctrl_gpio info\n", __func__);
		return ret;
	} else {
		charger_err("%s vbus_ctrl_gpio info %d\n", __func__,
				chip->vbus_ctrl_gpio);
	}

	return 0;
}

int xm_chg_feature_init(struct xm_charger *charger)
{
	int ret;
	struct chg_feature_info *chip;

	charger_err("%s: Start\n", __func__);

	if (charger->chg_feature) {
		charger_err("%s: Already initialized\n", __func__);
		return -EINVAL;
	}

	chip = devm_kzalloc(charger->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->dev = charger->dev;
	chip->update_cont = 0;

	ret = xm_chg_feature_parse_dt(chip->dev, chip);
	if (ret) {
		charger_err("%s parse dt fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = devm_gpio_request(chip->dev, chip->vbus_ctrl_gpio, "vbus ctrl gpio");
	if (ret) {
		charger_err("%s: %d gpio request failed\n", __func__, chip->vbus_ctrl_gpio);
		return ret;
	}
	gpio_direction_output(chip->vbus_ctrl_gpio, false);

	INIT_DELAYED_WORK(&chip->typec_conn_therm_work, typec_conn_therm_work);
	INIT_DELAYED_WORK( &chip->xm_prop_change_work, generate_xm_charge_uvent);
	INIT_DELAYED_WORK( &chip->night_chargig_change_work, night_chargig_change_work);

	schedule_delayed_work(&chip->typec_conn_therm_work,
			msecs_to_jiffies(CONN_THERM_DELAY_5S));
	schedule_delayed_work(&chip->xm_prop_change_work,
			msecs_to_jiffies(30000));

	charger->chg_feature = chip;
	charger_err("%s: End\n", __func__);

	return 0;
}

void xm_chg_feature_deinit(void)
{
	struct chg_feature_info *chip = g_xm_charger->chg_feature;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->typec_conn_therm_work);
	cancel_delayed_work_sync(&chip->xm_prop_change_work);
	cancel_delayed_work_sync(&chip->night_chargig_change_work);
	chip = NULL;

	return;
}

