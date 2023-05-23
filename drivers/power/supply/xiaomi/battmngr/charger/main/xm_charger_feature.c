
#include <linux/battmngr/xm_charger_core.h>

static int xm_set_vbus_disable(struct chg_feature_info *chip, bool disable)
{
	int ret = 0;

	charger_err("%s: set vbus disable:%d\n", __func__, disable);

	if (disable)
		gpio_set_value(chip->vbus_ctrl_gpio, 1);
	else
		gpio_set_value(chip->vbus_ctrl_gpio, 0);

	chip->vbus_disable = disable;

	return ret;
}

static void typec_conn_therm_work(struct work_struct *work)
{
	struct chg_feature_info *chip = container_of(work,
			struct chg_feature_info, typec_conn_therm_work.work);
	//union power_supply_propval pval = {0, };
	int ret = 0;

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

	schedule_delayed_work(&chip->typec_conn_therm_work,
			msecs_to_jiffies(CONN_THERM_DELAY_2S));

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
	chip = NULL;

	return;
}

