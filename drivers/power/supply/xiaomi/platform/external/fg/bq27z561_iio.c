
#include "inc/bq27z561.h"
#include "inc/bq27z561_iio.h"
#include "inc/xm_battery_auth.h"
#include "inc/xm_soc_smooth.h"

static int fg_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct bq_fg_chip *bq = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_BQFG_TEMP:
		bq->fake_temp = val1;
		break;
	case PSY_IIO_BQFG_CAPACITY:
		bq->fake_soc = val1;
		break;
	case PSY_IIO_BQFG_UPDATE_NOW:
		fg_dump_registers(bq);
		break;
	case PSY_IIO_BQFG_THERM_CURR:
		//bq->fcc_curr = val1;
		break;
	case PSY_IIO_BQFG_CHIP_OK:
		bq->fake_chip_ok = !!val1;
		break;
	case PSY_IIO_BQFG_BATTERY_AUTH:
		bq->verify_digest_success = !!val1;
		break;
	case PSY_IIO_BQFG_SHUTDOWN_DELAY:
		bq->shutdown_delay = val1;
		break;
	case PSY_IIO_BQFG_FASTCHARGE_MODE:
		fg_set_fastcharge_mode(bq, val1);
		break;
	default:
		pr_debug("Unsupported FG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int fg_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct bq_fg_chip *bq = iio_priv(indio_dev);
	static int ov_count;
	int vbat_mv = 0;
	static bool shutdown_delay_cancel;
	static bool last_shutdown_delay;
	int rc = 0;
	int ret = 0, status;

	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_BQFG_PRESENT:
		*val1 = 1;
		break;
	case PSY_IIO_BQFG_STATUS:
		status = fg_get_batt_status(bq);
		if (bq->charge_full)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (status == POWER_SUPPLY_STATUS_FULL && !(bq->charge_full)) {
			if (bq->batt_curr <= 0)
					status =  POWER_SUPPLY_STATUS_CHARGING;
				else
					status =  POWER_SUPPLY_STATUS_DISCHARGING;
		}
		*val1 = status;
		break;
	case PSY_IIO_BQFG_VOLTAGE_NOW:
		ret = fg_read_volt(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_volt = ret;
		*val1 = bq->batt_volt * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case PSY_IIO_BQFG_VOLTAGE_MAX:
		*val1 = 4450 * 1000;
		*val1 = fg_read_charging_voltage(bq);
		if (*val1 == BQ_MAXIUM_VOLTAGE_FOR_CELL) {
			if (bq->batt_volt > BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC_SAFETY) {
				ov_count++;
				if (ov_count > 2) {
					ov_count = 0;
					bq->cell_ov_check++;
				}
			} else {
				ov_count = 0;
			}
			if (bq->cell_ov_check > 4)
				bq->cell_ov_check = 4;

			*val1 = BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC - bq->cell_ov_check * 10;
			if ((bq->batt_soc == 100) && (*val1 == BQ_PACK_MAXIUM_VOLTAGE_FOR_PMIC))
				*val1 = BQ_MAXIUM_VOLTAGE_FOR_CELL;
		}
		*val1 *= 1000;
		break;
	case PSY_IIO_BQFG_CURRENT_NOW:
		mutex_lock(&bq->data_lock);
		fg_read_current(bq, &bq->batt_curr);
		if(bq->batt_curr != BQ_I2C_FAILED_SOC){
			bq->old_batt_curr = bq->batt_curr;
		}
		if(bq->batt_soc >= 98 && bq->batt_curr == BQ_I2C_FAILED_SOC) {
			bq->batt_curr = bq->old_batt_curr;
		}
		*val1 = bq->batt_curr * 1000;
		mutex_unlock(&bq->data_lock);
		break;
	case PSY_IIO_BQFG_CAPACITY:
		//add shutdown delay feature
		if (bq->fake_soc >= 0) {
			*val1 = bq->fake_soc;
			if(bq->batt_volt < 3450){
				*val1 = 1;
				bq->fake_soc = -1;
			}
			break;
		}

		if (bq->batt_soc < 0)
			bq->batt_soc = bq->batt_soc_old;
		else
			bq->batt_soc_old = bq->batt_soc;
		*val1 = bq->batt_soc;

		if (bq->shutdown_delay_enable) {
			if (*val1 == 0) {
				vbat_mv = fg_read_volt(bq);
				if (g_battmngr_noti->mainchg_msg.chg_plugin || g_battmngr_noti->pd_msg.pd_active)
					status = 1;
				else
					status = 0;
				if (vbat_mv > SHUTDOWN_DELAY_VOL
					&& !status) {
					bq->shutdown_delay = true;
					*val1 = 1;
				} else if (status && bq->shutdown_delay) {
					bq->shutdown_delay = false;
					shutdown_delay_cancel = true;
					*val1 = 1;
				} else {
					bq->shutdown_delay = false;
					if (vbat_mv > SHUTDOWN_DELAY_VOL + 20)
						*val1 = 1;
				}
			} else {
				bq->shutdown_delay = false;
				shutdown_delay_cancel = false;
			}

			if (last_shutdown_delay != bq->shutdown_delay) {
				last_shutdown_delay = bq->shutdown_delay;
				if (bq->batt_psy)
					power_supply_changed(bq->batt_psy);
			}
		}
		break;
	case PSY_IIO_BQFG_CAPACITY_LEVEL:
		*val1 = fg_get_batt_capacity_level(bq);
		break;
	case PSY_IIO_BQFG_TEMP:
		if (bq->fake_temp != -EINVAL) {
			*val1 = bq->fake_temp;
			break;
		}
		*val1 = bq->batt_temp;
		break;
	case PSY_IIO_BQFG_TIME_TO_EMPTY_NOW:
		ret = fg_read_tte(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_tte = ret;

		*val1 = bq->batt_tte * 60;
		mutex_unlock(&bq->data_lock);
		break;

	case PSY_IIO_BQFG_CHARGE_FULL:
		ret = fg_read_fcc(bq);
		mutex_lock(&bq->data_lock);
		if (ret > 0)
			bq->batt_fcc = ret;
		*val1 = bq->batt_fcc * 1000;
		mutex_unlock(&bq->data_lock);
		break;

	case PSY_IIO_BQFG_CHARGE_FULL_DESIGN:
		ret = fg_read_dc(bq);
		mutex_lock(&bq->data_lock);
		if (ret > 0)
			bq->batt_dc = ret;
		*val1 = bq->batt_dc * 1000;
		mutex_unlock(&bq->data_lock);
		break;

	case PSY_IIO_BQFG_CYCLE_COUNT:
		ret = fg_read_cyclecount(bq);
		mutex_lock(&bq->data_lock);
		if (ret >= 0)
			bq->batt_cyclecnt = ret;
		*val1 = bq->batt_cyclecnt;
		mutex_unlock(&bq->data_lock);
		break;
	case PSY_IIO_BQFG_TIME_TO_FULL_NOW:
		*val1 = fg_read_ttf(bq) * 60;
		break;

	case PSY_IIO_BQFG_RESISTANCE_ID:
		*val1 = 10000;
		break;
	case PSY_IIO_BQFG_UPDATE_NOW:
		*val1 = 0;
		break;
	case PSY_IIO_BQFG_THERM_CURR:
		//*val1 = bq->fcc_curr;
		break;
	case PSY_IIO_BQFG_CHIP_OK:
		if (bq->fake_chip_ok != -EINVAL) {
			*val1 = bq->fake_chip_ok;
			break;
		}
		*val1 = fg_get_chip_ok(bq);
		break;
	case PSY_IIO_BQFG_BATTERY_AUTH:
		*val1 = bq->verify_digest_success;
		break;
	case PSY_IIO_BQFG_SOC_DECIMAL:
		*val1 = fg_get_soc_decimal(bq);
		break;
	case PSY_IIO_BQFG_SOC_DECIMAL_RATE:
		*val1 = fg_get_soc_decimal_rate(bq);
		break;
	case PSY_IIO_BQFG_SOH:
		*val1 = fg_read_soh(bq);
		break;
	case PSY_IIO_BQFG_BATTERY_ID:
		*val1 = bq->batt_id;
		break;
	case PSY_IIO_BQFG_RSOC:
		*val1 = (bq->batt_rm * 10000) / bq->batt_fcc;
		break;
	case PSY_IIO_BQFG_SHUTDOWN_DELAY:
		*val1 = bq->shutdown_delay;
		pr_err("vbat_mv %d bq->shutdown_delay = %d, shutdown_delay_enable = %d, batt_soc = %d\n",
			bq->batt_volt, bq->shutdown_delay, bq->shutdown_delay_enable, bq->batt_soc);
		break;
	case PSY_IIO_BQFG_FASTCHARGE_MODE:
		*val1 = bq->fast_mode;
		break;
	case PSY_IIO_BQFG_TEMP_MAX:
		*val1 = fg_get_temp_max_fac(bq);
		break;
	case PSY_IIO_BQFG_TIME_OT:
		*val1 = fg_get_time_ot(bq);
		break;
	case PSY_IIO_BQFG_REG_RSOC:
		*val1 = fg_read_rsoc(bq);
		break;
	case PSY_IIO_BQFG_RM:
		*val1 = bq->batt_rm * 1000;
		break;
	default:
		pr_debug("Unsupported FG IIO chan %d\n", chan->channel);
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

static int fg_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct bq_fg_chip *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(bq27z561_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info fg_iio_info = {
	.read_raw	= fg_iio_read_raw,
	.write_raw	= fg_iio_write_raw,
	.of_xlate	= fg_iio_of_xlate,
};

int bq27z561_init_iio_psy(struct bq_fg_chip *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int fg_num_iio_channels = ARRAY_SIZE(bq27z561_iio_psy_channels);
	int rc, i;

	chip->iio_chan = devm_kcalloc(chip->dev, fg_num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				fg_num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	chip->ext_iio_chans = devm_kcalloc(chip->dev,
				ARRAY_SIZE(bq27z561_iio_psy_channels),
				sizeof(*chip->ext_iio_chans),
				GFP_KERNEL);
	if (!chip->ext_iio_chans)
		return -ENOMEM;

	indio_dev->info = &fg_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->name = "bq27z561,fg";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = fg_num_iio_channels;

	for (i = 0; i < fg_num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = bq27z561_iio_psy_channels[i].channel_num;
		chan->type = bq27z561_iio_psy_channels[i].type;
		chan->datasheet_name =
			bq27z561_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			bq27z561_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			bq27z561_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register FG IIO device, rc=%d\n", rc);

	pr_err("BQ27z561 IIO device, rc=%d\n", rc);
	return rc;
}

