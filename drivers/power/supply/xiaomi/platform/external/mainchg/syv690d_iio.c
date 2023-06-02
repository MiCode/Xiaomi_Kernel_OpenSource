
#include "inc/syv690d.h"
#include "inc/syv690d_reg.h"
#include "inc/syv690d_iio.h"

static int bq_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct bq2589x *bq = iio_priv(indio_dev);
	int rc = 0;
	u8 state;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_SYV_CHARGE_PRESENT:
		*val1 = bq2589x_is_charge_present(bq);
		break;
	case PSY_IIO_SYV_CHARGE_ONLINE:
		*val1 = bq2589x_is_charge_online(bq);
		break;
	case PSY_IIO_SYV_CHARGE_DONE:
		*val1 = bq2589x_is_charge_done(bq);
		break;
	case PSY_IIO_SYV_CHAGER_HZ:
		rc = bq2589x_get_hiz_mode(bq, &state);
		if(!rc)
			*val1 = (int)state;
		break;
	case PSY_IIO_SYV_INPUT_CURRENT_SETTLED:
		*val1 = bq->cfg.input_current_limit;
		break;
	case PSY_IIO_SYV_INPUT_VOLTAGE_SETTLED:
		*val1 = bq2589x_read_vindpm_volt(bq);
		break;
	case PSY_IIO_SYV_CHAGER_CURRENT:
		*val1 = bq2589x_adc_read_charge_current(bq);
		break;
	case PSY_IIO_SYV_CHARGING_ENABLED:
		*val1 = bq2589x_get_charger_enable(bq);
		break;
	case PSY_IIO_SYV_BUS_VOLTAGE:
		*val1 = bq2589x_adc_read_vbus_volt(bq);
		break;
	case PSY_IIO_SYV_BATTERY_VOLTAGE:
		*val1 = bq2589x_adc_read_battery_volt(bq);
		break;
	case PSY_IIO_SYV_OTG_ENABLE:
		*val1 = bq->cfg.otg_status;
		break;
	case PSY_IIO_SYV_CHAGER_TERM:
		*val1 = bq->cfg.term_current;
		break;
	case PSY_IIO_SYV_BATTERY_VOLTAGE_TERM:
		*val1 = bq->cfg.battery_voltage_term;
		break;
	case PSY_IIO_SYV_CHARGER_STATUS:
		*val1 = bq2589x_charge_status(bq);
		break;
	case PSY_IIO_SYV_CHARGE_TYPE:
		*val1 = bq2589x_get_chg_type(bq);;
		break;
	case PSY_IIO_SYV_CHARGE_USB_TYPE:
		*val1 = bq2589x_get_chg_usb_type(bq);;
		break;
	case PSY_IIO_SYV_ENABLE_CHAGER_TERM:
		*val1 = bq->cfg.enable_term;
		break;
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int bq_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct bq2589x *bq = iio_priv(indio_dev);
	int rc = 0;
	union power_supply_propval val = {0,};

	switch (chan->channel) {
	case PSY_IIO_SYV_CHAGER_HZ:
		if(val1) {
			bq2589x_enter_hiz_mode(bq);
			bq->hz_flag = true;
		} else {
			bq2589x_exit_hiz_mode(bq);
			bq->hz_flag = false;
		}
		power_supply_changed(bq->batt_psy);
		power_supply_changed(bq->usb_psy);
		pr_err("iio_write: hz_flag %d\n", bq->hz_flag);
		break;
	case PSY_IIO_SYV_INPUT_CURRENT_SETTLED:
		if(val1 >= 3100)
			val1 = 3100;
		bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_ENILIM_MASK,
			BQ2589X_ENILIM_DISABLE << BQ2589X_ENILIM_SHIFT);
		bq2589x_set_input_current_limit(bq, val1);
		bq->cfg.input_current_limit = val1;
		break;
	case PSY_IIO_SYV_INPUT_VOLTAGE_SETTLED:
		bq2589x_use_absolute_vindpm(bq, true);
		bq2589x_set_input_volt_limit(bq, val1);
		pr_err("bq2589x_set_input_volt_limit %d\n", val1);
		break;
	case PSY_IIO_SYV_CHAGER_CURRENT:
		if(val1 >= 3100)
			val1 = 3100;
		bq->cfg.charge_current = val1;
		bq2589x_set_charge_current(bq, val1);
		break;
	case PSY_IIO_SYV_CHARGING_ENABLED:
		pr_err("%s: PSY_IIO_SYV_CHARGING_ENABLED, val = %d\n", __func__, val1);
		if(val1)
			bq2589x_enable_charger(bq);
		else
			bq2589x_disable_charger(bq);
		power_supply_changed(bq->batt_psy);
		power_supply_changed(bq->usb_psy);
		break;
	case PSY_IIO_SYV_OTG_ENABLE:
		if (!g_bq2589x) {
			pr_err("%s: g_bq2589x is NULL\n", __func__);
			return -EINVAL;
		}
		gpio_set_value(g_bq2589x->otg_gpio, val1);
		pr_err("%s: PSY_IIO_SYV_OTG_ENABLE, val = %d\n", __func__, val1);
		if(val1) {
			bq->cfg.otg_status = true;
			bq2589x_exit_hiz_mode(bq);
			bq2589x_disable_charger(bq);
			bq2589x_enable_otg(bq);
			bq2589x_set_otg_volt(bq, 5300);
			if(bq->batt_psy)
				power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
			if(val.intval > 5)
				bq2589x_set_otg_current(bq, 1800);
			else
				bq2589x_set_otg_current(bq, 1500);
			if(!bq->wakeup_flag) {
				__pm_stay_awake(bq->xm_ws);
				bq->wakeup_flag = 1;
				pr_err("otg workup\n");
			}
		} else {
			bq->cfg.otg_status = false;
			bq2589x_disable_otg(bq);
			bq2589x_enable_charger(bq);
			if(bq->wakeup_flag) {
				__pm_relax(bq->xm_ws);
				bq->wakeup_flag = 0;
				pr_err("xm otg relax\n");
			}
		}
		break;
	case PSY_IIO_SYV_CHAGER_TERM:
		bq->cfg.term_current = val1;
		bq2589x_set_term_current(bq, val1);
		break;
	case PSY_IIO_SYV_BATTERY_VOLTAGE_TERM:
		bq2589x_set_chargevoltage(bq, val1);
		bq->cfg.battery_voltage_term = val1;
		break;
	case PSY_IIO_SYV_ENABLE_CHAGER_TERM:
		bq->cfg.enable_term = val1;
		bq2589x_enable_term(bq, bq->cfg.enable_term);
		break;
	default:
		pr_debug("Unsupported BQ25890 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int bq_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct bq2589x *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(syv690d_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info syv690d_iio_info = {
	.read_raw	= bq_iio_read_raw,
	.write_raw	= bq_iio_write_raw,
	.of_xlate	= bq_iio_of_xlate,
};

int bq_init_iio_psy(struct bq2589x *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(syv690d_iio_psy_channels);
	int rc, i;

	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &syv690d_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->name = "syv690d,mainchg";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;

	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = syv690d_iio_psy_channels[i].channel_num;
		chan->type = syv690d_iio_psy_channels[i].type;
		chan->datasheet_name =
			syv690d_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			syv690d_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			syv690d_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register QG IIO device, rc=%d\n", rc);

	pr_err("BQ25890H IIO device, rc=%d\n", rc);
	return rc;
}

