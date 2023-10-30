
#include "inc/ln8000.h"
#include "inc/ln8000_reg.h"
#include "inc/ln8000_iio.h"

static int ln_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct ln8000_info *info = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_SC_CHARGING_ENABLED:
		ln_info("POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
				val1 ? "enable" : "disable");
		rc = psy_chg_set_charging_enable(info, val1);
		break;
	case PSY_IIO_SC_PRESENT:
        //rc = psy_chg_set_present(info, !!val1);
		break;
	default:
		pr_debug("Unsupported LN8000 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int ln_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct ln8000_info *info = iio_priv(indio_dev);
	int ret = 0;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_SC_CHARGING_ENABLED:
		*val1 = psy_chg_get_charging_enabled(info);
		break;
	case PSY_IIO_SC_STATUS:
		*val1 = 0;
		break;
	case PSY_IIO_SC_PRESENT:
		*val1 = info->usb_present;
		pr_err("val = %d, usb_present = %d\n", *val1, info->usb_present);
		break;
	case PSY_IIO_SC_BATTERY_PRESENT:
        ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
        if (info->vbat_uV > LN8000_ADC_VBAT_MIN) {
                info->batt_present = 1;    /* detected battery */
        } else {
                info->batt_present = 0;    /* non-detected battery */
        }
		*val1 = info->batt_present;
		break;
	case PSY_IIO_SC_VBUS_PRESENT:
        ret = ln8000_check_status(info);
       	*val1 = !(info->vac_unplug);
		break;
	case PSY_IIO_SC_BATTERY_VOLTAGE:
        ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
        *val1 = info->vbat_uV/1000;
		break;
	case PSY_IIO_SC_BATTERY_CURRENT:
        ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
        *val1 = (info->iin_uA * 2)/1000;         /* return to IBUS_ADC x 2 */
		break;
	case PSY_IIO_SC_BATTERY_TEMPERATURE:
        if (info->pdata->tbat_mon_disable) {
                *val1 = 0;
        } else {
                ln8000_get_adc_data(info, LN8000_ADC_CH_TSBAT, &info->tbat_uV);
                *val1 = info->tbat_uV;
                ln_info("ti_battery_temperature: adc_tbat=%d\n", *val1);
        }
		break;
	case PSY_IIO_SC_BUS_VOLTAGE:
        ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
        *val1 = info->vbus_uV/1000;
		break;
	case PSY_IIO_SC_BUS_CURRENT:
        ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
        *val1 = info->iin_uA/1000;
		break;
	case PSY_IIO_SC_BUS_TEMPERATURE:
        if (info->pdata->tbus_mon_disable) {
                *val1 = 0;
        } else {
                ln8000_get_adc_data(info, LN8000_ADC_CH_TSBUS, &info->tbus_uV);
                *val1 = info->tbus_uV;
                ln_info("ti_bus_temperature: adc_tbus=%d\n", *val1);
        }
		break;
	case PSY_IIO_SC_DIE_TEMPERATURE:
        ln8000_get_adc_data(info, LN8000_ADC_CH_DIETEMP, &info->tdie_dC);
        *val1 = info->tdie_dC;
        ln_info("ti_die_temperature: adc_tdie=%d\n", *val1);
		break;

	case PSY_IIO_SC_ALARM_STATUS:
        *val1 = psy_chg_get_ti_alarm_status(info);
		break;
	case PSY_IIO_SC_FAULT_STATUS:
        *val1 = psy_chg_get_ti_fault_status(info);
		break;
	case PSY_IIO_SC_VBUS_ERROR_STATUS:
        *val1 = psy_chg_get_it_bus_error_status(info);
		break;
	case PSY_IIO_SC_REG_STATUS:
		ln8000_check_status(info);
		*val1 = ((info->vbat_regulated << VBAT_REG_STATUS_SHIFT) |
				/* ln8000 not support ibat_reg, we are can be ibus_reg */
				(info->iin_regulated << IBAT_REG_STATUS_SHIFT));
		if (*val1) {
			ln_info("ln_reg_status: val1=0x%x\n", *val1);
		}
		break;
	default:
		pr_err("Unsupported QG IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		pr_err("Couldn't read IIO channel %d, ret = %d\n",
			chan->channel, ret);
		return ret;
	}

	return IIO_VAL_INT;
}

static int ln_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct ln8000_info *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(ln8000_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info ln_iio_info = {
	.read_raw	= ln_iio_read_raw,
	.write_raw	= ln_iio_write_raw,
	.of_xlate	= ln_iio_of_xlate,
};

int ln_init_iio_psy(struct ln8000_info *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(ln8000_iio_psy_channels);
	int rc, i;

	pr_err("LN8000 ln_init_iio_psy start\n");
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

	indio_dev->info = &ln_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	if (chip->dev_role == LN_ROLE_MASTER) {
		indio_dev->name = "ln8000-master";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = ln8000_iio_psy_channels[i].channel_num;
			chan->type = ln8000_iio_psy_channels[i].type;
			chan->datasheet_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				ln8000_iio_psy_channels[i].info_mask;
		}
	} else if (chip->dev_role == LN_ROLE_SLAVE) {
		indio_dev->name = "ln8000-slave";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = ln8000_slave_iio_psy_channels[i].channel_num;
			chan->type = ln8000_slave_iio_psy_channels[i].type;
			chan->datasheet_name =
				ln8000_slave_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				ln8000_slave_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				ln8000_slave_iio_psy_channels[i].info_mask;
		}
	} else {
		indio_dev->name = "ln8000-standalone";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = ln8000_iio_psy_channels[i].channel_num;
			chan->type = ln8000_iio_psy_channels[i].type;
			chan->datasheet_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				ln8000_iio_psy_channels[i].info_mask;
		}
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register LN8000 IIO device, rc=%d\n", rc);

	pr_err("LN8000 IIO device, rc=%d\n", rc);
	return rc;
}

