// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/iio/consumer.h>
#include "adc-tm.h"
#include "../thermal_core.h"

#define ADC_TM_STATUS1				0x08
#define ADC_TM_STATUS_LOW_SET		0x09
#define ADC_TM_STATUS_LOW_CLR		0x0a
#define ADC_TM_STATUS_HIGH_SET		0x0b
#define ADC_TM_STATUS_HIGH_CLR		0x0c

#define ADC_TM_CFG_HS_SET			0x0d
#define ADC_TM_CFG_HS_FLAG			BIT(0)
#define ADC_TM_CFG_HS_CLR			0x0e

#define ADC_TM_NUM_BTM_CHAN			0x0f

#define ADC_TM_SID					0x40

#define ADC_TM_CH_CTL				0x41
#define ADC_TM_TM_CH_SEL			GENMASK(7, 5)
#define ADC_TM_TM_CH_SEL_MASK_SHIFT		5
#define ADC_TM_MEAS_INT_SEL			GENMASK(3, 2)
#define ADC_TM_MEAS_INT_SEL_MASK_SHIFT		2

#define ADC_TM_ADC_DIG_PARAM		0x42
#define ADC_TM_CTL_DEC_RATIO_MASK		GENMASK(3, 2)
#define ADC_TM_CTL_DEC_RATIO_SHIFT		2
#define ADC_TM_CTL_CAL_SEL			GENMASK(5, 4)
#define ADC_TM_CTL_CAL_SEL_MASK_SHIFT		4

#define ADC_TM_FAST_AVG_CTL			0x43
#define ADC_TM_FAST_AVG_EN			BIT(7)

#define ADC_TM_ADC_CH_SEL_CTL		0x44

#define ADC_TM_DELAY_CTL			0x45

#define ADC_TM_EN_CTL1				0x46
#define ADC_TM_EN				BIT(7)

#define ADC_TM_CONV_REQ				0x47
#define ADC_TM_CONV_REQ_EN			BIT(7)

#define ADC_TM_DATA_HOLD_CTL		0x48

#define ADC_TM_LOW_THR0			0x49
#define ADC_TM_LOW_THR1			0x4a
#define ADC_TM_HIGH_THR0		0x4b
#define ADC_TM_HIGH_THR1		0x4c
#define ADC_TM_LOWER_MASK(n)			((n) & GENMASK(7, 0))
#define ADC_TM_UPPER_MASK(n)			(((n) & GENMASK(15, 8)) >> 8)

#define ADC_TM_MEAS_IRQ_EN		0x4d
#define ADC_TM_MEAS_EN			BIT(7)

#define ADC_TM_MEAS_INT_LSB		0x50
#define ADC_TM_MEAS_INT_MSB		0x51
#define ADC_TM_MEAS_INT_MODE	0x52

#define ADC_TM_Mn_DATA0(n)			((n * 2) + 0xa0)
#define ADC_TM_Mn_DATA1(n)			((n * 2) + 0xa1)
#define ADC_TM_DATA_SHIFT			8

#define ADC_TM_POLL_DELAY_MIN_US		100
#define ADC_TM_POLL_DELAY_MAX_US		110
#define ADC_TM_POLL_RETRY_COUNT			3

static struct adc_tm_reverse_scale_fn_adc7 adc_tm_rscale_fn[] = {
	[SCALE_R_ABSOLUTE] = {adc_tm_absolute_rthr_adc7},
};

static int adc_tm_get_temp(struct adc_tm_sensor *sensor, int *temp)
{
	int ret, milli_celsius;

	if (!sensor || !sensor->adc)
		return -EINVAL;

	ret = iio_read_channel_processed(sensor->adc, &milli_celsius);
	if (ret < 0)
		return ret;

	*temp = milli_celsius;

	return 0;
}

static int32_t adc_tm_read_reg(struct adc_tm_chip *chip,
					int16_t reg, u8 *data, int len)
{
	int ret;

	ret = regmap_bulk_read(chip->regmap, (chip->base + reg), data, len);
	if (ret < 0)
		pr_err("adc-tm read reg %d failed with %d\n", reg, ret);

	return ret;
}

static int32_t adc_tm_write_reg(struct adc_tm_chip *chip,
					int16_t reg, u8 *data, int len)
{
	int ret;

	ret = regmap_bulk_write(chip->regmap, (chip->base + reg), data, len);
	if (ret < 0)
		pr_err("adc-tm write reg %d failed with %d\n", reg, ret);

	return ret;
}

static int32_t adc_tm7_conv_req(struct adc_tm_chip *chip)
{
	int rc = 0;
	u8 data = 0;
	unsigned int count;

	data = ADC_TM_EN;
	rc = adc_tm_write_reg(chip, ADC_TM_EN_CTL1, &data, 1);
	if (rc < 0) {
		pr_err("adc-tm enable failed with %d\n", rc);
		return rc;
	}

	data = ADC_TM_CFG_HS_FLAG;
	rc = adc_tm_write_reg(chip, ADC_TM_CFG_HS_SET, &data, 1);
	if (rc < 0) {
		pr_err("adc-tm handshake failed with %d\n", rc);
		return rc;
	}

	data = ADC_TM_CONV_REQ_EN;
	rc = adc_tm_write_reg(chip, ADC_TM_CONV_REQ, &data, 1);
	if (rc < 0) {
		pr_err("adc-tm request conversion failed with %d\n", rc);
		return rc;
	}

	for (count = 0; count < ADC_TM_POLL_RETRY_COUNT; count++) {
		rc = adc_tm_read_reg(chip, ADC_TM_CFG_HS_SET, &data, 1);
		if (rc < 0) {
			pr_err("adc-tm read failed with %d\n", rc);
			return rc;
		}

		if (!(data & ADC_TM_CFG_HS_FLAG))
			return rc;
		usleep_range(ADC_TM_POLL_DELAY_MIN_US,
			ADC_TM_POLL_DELAY_MAX_US);
	}

	pr_err("adc-tm conversion request handshake timed out\n");

	return -ETIMEDOUT;
}

static int adc_tm7_configure(struct adc_tm_sensor *sensor)
{
	struct adc_tm_chip *chip = sensor->chip;
	u8 buf[14];
	uint32_t mask = 0;
	int ret;

	ret = adc_tm_read_reg(chip, ADC_TM_SID, buf, 14);
	if (ret < 0) {
		pr_err("adc-tm block read failed with %d\n", ret);
		return ret;
	}

	buf[0] = sensor->sid;
	buf[1] &= ~ADC_TM_TM_CH_SEL;
	buf[1] |= sensor->btm_ch << ADC_TM_TM_CH_SEL_MASK_SHIFT;
	buf[1] &= ~ADC_TM_MEAS_INT_SEL;
	buf[1] |= sensor->meas_time << ADC_TM_MEAS_INT_SEL_MASK_SHIFT;

	/* Set calibration select, hw_settle delay */
	buf[2] &= ~ADC_TM_CTL_DEC_RATIO_MASK;
	buf[2] |= sensor->decimation << ADC_TM_CTL_DEC_RATIO_SHIFT;
	buf[2] &= ~ADC_TM_CTL_CAL_SEL;
	buf[2] |= sensor->cal_sel << ADC_TM_CTL_CAL_SEL_MASK_SHIFT;

	buf[3] = sensor->fast_avg_samples | ADC_TM_FAST_AVG_EN;

	buf[4] = sensor->adc_ch;

	buf[5] = sensor->hw_settle_time;

	mask = lower_32_bits(sensor->low_thr_voltage);
	buf[9] = ADC_TM_LOWER_MASK(mask);
	buf[10] = ADC_TM_UPPER_MASK(mask);

	mask = lower_32_bits(sensor->high_thr_voltage);
	buf[11] = ADC_TM_LOWER_MASK(mask);
	buf[12] = ADC_TM_UPPER_MASK(mask);

	buf[13] |= (sensor->meas_en | sensor->high_thr_en << 1 |
				sensor->low_thr_en);

	ret = adc_tm_write_reg(chip, ADC_TM_SID, buf, 14);
	if (ret < 0)
		pr_err("adc-tm block write failed with %d\n", ret);

	return ret;
}

static int32_t adc_tm_add_to_list(struct adc_tm_chip *chip,
				uint32_t dt_index,
				struct adc_tm_param *param)
{
	struct adc_tm_client_info *client_info = NULL;
	bool client_info_exists = false;

	list_for_each_entry(client_info,
			&chip->sensor[dt_index].thr_list, list) {
		if (client_info->param->id == param->id) {
			client_info->low_thr_requested = param->low_thr;
			client_info->high_thr_requested = param->high_thr;
			client_info->state_request = param->state_request;
			client_info->notify_low_thr = false;
			client_info->notify_high_thr = false;
			client_info_exists = true;
			pr_debug("client found\n");
		}
	}

	if (!client_info_exists) {
		client_info = devm_kzalloc(chip->dev,
			sizeof(struct adc_tm_client_info), GFP_KERNEL);
		if (!client_info)
			return -ENOMEM;

		pr_debug("new client\n");
		client_info->param->id = (uintptr_t) client_info;
		client_info->low_thr_requested = param->low_thr;
		client_info->high_thr_requested = param->high_thr;
		client_info->state_request = param->state_request;

		list_add_tail(&client_info->list,
					&chip->sensor[dt_index].thr_list);
	}
	return 0;
}

static int32_t adc_tm7_manage_thresholds(struct adc_tm_sensor *sensor)
{
	int high_thr = INT_MAX, low_thr = INT_MIN;
	struct adc_tm_client_info *client_info = NULL;
	struct list_head *thr_list;
	uint32_t scale_type = 0;
	struct adc_tm_config tm_config;

	sensor->high_thr_en = 0;
	sensor->low_thr_en = 0;

	/*
	 * Reset the high_thr_set and low_thr_set of all
	 * clients since the thresholds will be recomputed.
	 */
	list_for_each(thr_list, &sensor->thr_list) {
		client_info = list_entry(thr_list,
					struct adc_tm_client_info, list);
		client_info->high_thr_set = false;
		client_info->low_thr_set = false;
	}

	/* Find the min of high_thr and max of low_thr */
	list_for_each(thr_list, &sensor->thr_list) {
		client_info = list_entry(thr_list,
					struct adc_tm_client_info, list);

		if ((client_info->state_request == ADC_TM_HIGH_THR_ENABLE) ||
			(client_info->state_request ==
				ADC_TM_HIGH_LOW_THR_ENABLE))
			if (client_info->high_thr_requested < high_thr)
				high_thr = client_info->high_thr_requested;

		if ((client_info->state_request == ADC_TM_LOW_THR_ENABLE) ||
			(client_info->state_request ==
				ADC_TM_HIGH_LOW_THR_ENABLE))
			if (client_info->low_thr_requested > low_thr)
				low_thr = client_info->low_thr_requested;

		pr_debug("threshold compared is high:%d and low:%d\n",
				client_info->high_thr_requested,
				client_info->low_thr_requested);
		pr_debug("current threshold is high:%d and low:%d\n",
							high_thr, low_thr);
	}

	/* Check which of the high_thr and low_thr got set */
	list_for_each(thr_list, &sensor->thr_list) {
		client_info = list_entry(thr_list,
					struct adc_tm_client_info, list);

		if ((client_info->state_request == ADC_TM_HIGH_THR_ENABLE) ||
			(client_info->state_request ==
				ADC_TM_HIGH_LOW_THR_ENABLE))
			if (high_thr == client_info->high_thr_requested) {
				sensor->high_thr_en = 1;
				client_info->high_thr_set = true;
			}

		if ((client_info->state_request == ADC_TM_LOW_THR_ENABLE) ||
			(client_info->state_request ==
				ADC_TM_HIGH_LOW_THR_ENABLE))
			if (low_thr == client_info->low_thr_requested) {
				sensor->low_thr_en = 1;
				client_info->low_thr_set = true;
			}
	}

	tm_config.high_thr_voltage = (int64_t)high_thr;
	tm_config.low_thr_voltage = (int64_t)low_thr;
	tm_config.prescal = sensor->prescaling;

	scale_type = sensor->adc_rscale_fn;
	if (scale_type >= SCALE_RSCALE_NONE)
		return -EBADF;

	adc_tm_rscale_fn[scale_type].chan(&tm_config);

	sensor->low_thr_voltage = tm_config.low_thr_voltage;
	sensor->high_thr_voltage = tm_config.high_thr_voltage;

	pr_debug("threshold written is high:%d and low:%d\n",
							high_thr, low_thr);

	return 0;
}

/* Used to notify non-thermal clients of threshold crossing */
void notify_adc_tm7_fn(struct adc_tm_sensor *adc_tm)
{
	struct adc_tm_client_info *client_info = NULL;
	struct adc_tm_chip *chip;
	struct list_head *thr_list;
	int ret;

	chip = adc_tm->chip;

	mutex_lock(&chip->adc_mutex_lock);
	if (adc_tm->low_thr_triggered) {
		/* adjust thr, calling manage_thr */
		list_for_each(thr_list, &adc_tm->thr_list) {
			client_info = list_entry(thr_list,
					struct adc_tm_client_info, list);
			if (client_info->low_thr_set) {
				client_info->low_thr_set = false;
				client_info->notify_low_thr = true;
				if (client_info->state_request ==
						ADC_TM_HIGH_LOW_THR_ENABLE)
					client_info->state_request =
							ADC_TM_HIGH_THR_ENABLE;
				else
					client_info->state_request =
							ADC_TM_LOW_THR_DISABLE;
			}
		}
		adc_tm->low_thr_triggered = false;
	}

	if (adc_tm->high_thr_triggered) {
		/* adjust thr, calling manage_thr */
		list_for_each(thr_list, &adc_tm->thr_list) {
			client_info = list_entry(thr_list,
					struct adc_tm_client_info, list);
			if (client_info->high_thr_set) {
				client_info->high_thr_set = false;
				client_info->notify_high_thr = true;
				if (client_info->state_request ==
						ADC_TM_HIGH_LOW_THR_ENABLE)
					client_info->state_request =
							ADC_TM_LOW_THR_ENABLE;
				else
					client_info->state_request =
							ADC_TM_HIGH_THR_DISABLE;
			}
		}
		adc_tm->high_thr_triggered = false;
	}
	ret = adc_tm7_manage_thresholds(adc_tm);
	if (ret < 0)
		pr_err("Error in reverse scaling:%d\n", ret);

	ret = adc_tm7_configure(adc_tm);
	if (ret < 0)
		pr_err("Error during adc-tm configure:%d\n", ret);

	ret = adc_tm7_conv_req(chip);
	if (ret < 0)
		pr_err("Error enabling adc-tm with %d\n", ret);

	mutex_unlock(&chip->adc_mutex_lock);

	list_for_each_entry(client_info, &adc_tm->thr_list, list) {
		if (client_info->notify_low_thr) {
			if (client_info->param->threshold_notification
								!= NULL) {
				pr_debug("notify kernel with low state\n");
				client_info->param->threshold_notification(
					ADC_TM_LOW_STATE,
					client_info->param->btm_ctx);
				client_info->notify_low_thr = false;
			}
		}

		if (client_info->notify_high_thr) {
			if (client_info->param->threshold_notification
								!= NULL) {
				pr_debug("notify kernel with high state\n");
				client_info->param->threshold_notification(
					ADC_TM_HIGH_STATE,
					client_info->param->btm_ctx);
				client_info->notify_high_thr = false;
			}
		}
	}
}

/* Used by non-thermal clients to configure an ADC_TM channel */
static int32_t adc_tm7_channel_measure(struct adc_tm_chip *chip,
					struct adc_tm_param *param)

{
	int ret, i;
	uint32_t v_channel, dt_index = 0;
	bool chan_found = false;

	ret = adc_tm_is_valid(chip);
	if (ret || (param == NULL))
		return -EINVAL;

	if (param->threshold_notification == NULL) {
		pr_debug("No notification for high/low temp\n");
		return -EINVAL;
	}

	for (i = 0; i < chip->dt_channels; i++) {
		v_channel = V_CHAN(chip->sensor[i]);
		if (v_channel == param->channel) {
			dt_index = i;
			chan_found = true;
			break;
		}
	}

	if (!chan_found)  {
		pr_err("not a valid ADC_TM channel\n");
		return -EINVAL;
	}

	mutex_lock(&chip->adc_mutex_lock);

	/* add channel client to channel list */
	adc_tm_add_to_list(chip, dt_index, param);

	/* set right thresholds for the sensor */
	ret = adc_tm7_manage_thresholds(&chip->sensor[dt_index]);
	if (ret < 0)
		pr_err("Error in reverse scaling:%d\n", ret);

	chip->sensor[dt_index].meas_en = ADC_TM_MEAS_EN;

	/* configure channel */
	ret = adc_tm7_configure(&chip->sensor[dt_index]);
	if (ret < 0) {
		pr_err("Error during adc-tm configure:%d\n", ret);
		goto fail_unlock;
	}

	ret = adc_tm7_conv_req(chip);
	if (ret < 0)
		pr_err("Error enabling adc-tm with %d\n", ret);

fail_unlock:
	mutex_unlock(&chip->adc_mutex_lock);
	return ret;
}

/* Used by non-thermal clients to release an ADC_TM channel */
static int32_t adc_tm7_disable_chan_meas(struct adc_tm_chip *chip,
					struct adc_tm_param *param)
{
	int ret, i;
	uint32_t dt_index = 0, v_channel;
	struct adc_tm_client_info *client_info = NULL;

	ret = adc_tm_is_valid(chip);
	if (ret || (param == NULL))
		return -EINVAL;

	for (i = 0; i < chip->dt_channels; i++) {
		v_channel = V_CHAN(chip->sensor[i]);
		if (v_channel == param->channel) {
			dt_index = i;
			break;
		}
	}

	if (i == chip->dt_channels)  {
		pr_err("not a valid ADC_TM channel\n");
		return -EINVAL;
	}

	mutex_lock(&chip->adc_mutex_lock);
	list_for_each_entry(client_info,
			&chip->sensor[i].thr_list, list) {
		if (client_info->param->id == param->id) {
			client_info->state_request =
				ADC_TM_HIGH_LOW_THR_DISABLE;
			ret = adc_tm7_manage_thresholds(&chip->sensor[i]);
			if (ret < 0) {
				pr_err("Error in reverse scaling:%d\n",
						ret);
				goto fail;
			}
			ret = adc_tm7_configure(&chip->sensor[i]);
			if (ret < 0) {
				pr_err("Error during adc-tm configure:%d\n",
						ret);
				goto fail;
			}
			ret = adc_tm7_conv_req(chip);
			if (ret < 0) {
				pr_err("Error enabling adc-tm with %d\n", ret);
				goto fail;
			}
		}
	}

fail:
	mutex_unlock(&chip->adc_mutex_lock);
	return ret;
}

static int adc_tm7_set_trip_temp(struct adc_tm_sensor *sensor,
					int low_temp, int high_temp)
{
	struct adc_tm_chip *chip;
	struct adc_tm_config tm_config;
	int ret;

	if (!sensor)
		return -EINVAL;

	pr_debug("%s:low_temp(mdegC):%d, high_temp(mdegC):%d\n", __func__,
							low_temp, high_temp);

	chip = sensor->chip;
	tm_config.high_thr_temp = tm_config.low_thr_temp = 0;
	if (high_temp != INT_MAX)
		tm_config.high_thr_temp = high_temp;
	if (low_temp != INT_MIN)
		tm_config.low_thr_temp = low_temp;

	if ((high_temp == INT_MAX) && (low_temp == INT_MIN)) {
		pr_err("No trips to set\n");
		return -EINVAL;
	}

	pr_debug("requested a low temp- %d and high temp- %d\n",
			tm_config.low_thr_temp, tm_config.high_thr_temp);
	adc_tm_scale_therm_voltage_100k_adc7(&tm_config);

	pr_debug("high_thr:0x%llx, low_thr:0x%llx\n",
		tm_config.high_thr_voltage, tm_config.low_thr_voltage);

	mutex_lock(&chip->adc_mutex_lock);

	if (high_temp != INT_MAX) {
		sensor->low_thr_voltage = tm_config.low_thr_voltage;
		sensor->low_thr_en = 1;
	} else
		sensor->low_thr_en = 0;

	if (low_temp != INT_MIN) {
		sensor->high_thr_voltage = tm_config.high_thr_voltage;
		sensor->high_thr_en = 1;
	} else
		sensor->high_thr_en = 0;

	sensor->meas_en = ADC_TM_MEAS_EN;

	ret = adc_tm7_configure(sensor);
	if (ret < 0) {
		pr_err("Error during adc-tm configure:%d\n", ret);
		goto fail;
	}

	ret = adc_tm7_conv_req(chip);
	if (ret < 0) {
		pr_err("Error enabling adc-tm with %d\n", ret);
		goto fail;
	}

fail:
	mutex_unlock(&chip->adc_mutex_lock);

	return ret;
}

static irqreturn_t adc_tm7_handler(int irq, void *data)
{
	struct adc_tm_chip *chip = data;
	u8 status_low, status_high, buf[16], val;
	int ret, i;

	ret = adc_tm_read_reg(chip, ADC_TM_STATUS_LOW_CLR, &status_low, 1);
	if (ret < 0) {
		pr_err("adc-tm read status low failed with %d\n", ret);
		goto handler_end;
	}

	ret = adc_tm_read_reg(chip, ADC_TM_STATUS_HIGH_CLR, &status_high, 1);
	if (ret < 0) {
		pr_err("adc-tm read status high failed with %d\n", ret);
		goto handler_end;
	}

	ret = adc_tm_write_reg(chip, ADC_TM_STATUS_LOW_CLR, &status_low, 1);
	if (ret < 0) {
		pr_err("adc-tm clear status low failed with %d\n", ret);
		goto handler_end;
	}

	ret = adc_tm_write_reg(chip, ADC_TM_STATUS_HIGH_CLR, &status_high, 1);
	if (ret < 0) {
		pr_err("adc-tm clear status high failed with %d\n", ret);
		goto handler_end;
	}

	val = BIT(0);
	ret = adc_tm_write_reg(chip, ADC_TM_DATA_HOLD_CTL, &val, 1);
	if (ret < 0) {
		pr_err("adc-tm set hold failed with %d\n", ret);
		goto handler_end;
	}

	ret = adc_tm_read_reg(chip, ADC_TM_Mn_DATA0(0), buf, 16);
	if (ret < 0) {
		pr_err("adc-tm read conversion data failed with %d\n", ret);
		goto handler_end;
	}

	val = 0;
	ret = adc_tm_write_reg(chip, ADC_TM_DATA_HOLD_CTL, &val, 1);
	if (ret < 0) {
		pr_err("adc-tm clear hold failed with %d\n", ret);
		goto handler_end;
	}

	for (i = 0; i < chip->dt_channels; i++) {
		bool upper_set = false, lower_set = false;
		u8 data_low = 0, data_high = 0;
		u16 code = 0;
		int temp;

		if (!chip->sensor[i].non_thermal &&
				IS_ERR(chip->sensor[i].tzd)) {
			pr_err("thermal device not found\n");
			continue;
		}

		if (!chip->sensor[i].non_thermal) {
			data_low = buf[2 * i];
			data_high = buf[2 * i + 1];
			code = ((data_high << ADC_TM_DATA_SHIFT) | data_low);
		}

		mutex_lock(&chip->adc_mutex_lock);

		if ((status_low & 0x1) && (chip->sensor[i].meas_en)
				&& (chip->sensor[i].low_thr_en))
			lower_set = true;

		if ((status_high & 0x1) && (chip->sensor[i].meas_en) &&
					(chip->sensor[i].high_thr_en))
			upper_set = true;

		status_low >>= 1;
		status_high >>= 1;
		mutex_unlock(&chip->adc_mutex_lock);
		if (!(upper_set || lower_set))
			continue;

		if (!chip->sensor[i].non_thermal) {
			/*
			 * Expected behavior is while notifying
			 * of_thermal, thermal core will call set_trips
			 * with new thresholds and activate/disable
			 * the appropriate trips.
			 */
			pr_debug("notifying of_thermal\n");
			temp = therm_fwd_scale_adc7((int64_t)code);
			if (temp == -EINVAL) {
				pr_err("Invalid temperature reading\n");
				continue;
			}
			of_thermal_handle_trip_temp(chip->sensor[i].tzd,
						temp);
		} else {
			if (lower_set) {
				mutex_lock(&chip->adc_mutex_lock);
				chip->sensor[i].low_thr_en = 0;
				chip->sensor[i].low_thr_triggered = true;
				mutex_unlock(&chip->adc_mutex_lock);
				queue_work(chip->sensor[i].req_wq,
						&chip->sensor[i].work);
			}

			if (upper_set) {
				mutex_lock(&chip->adc_mutex_lock);
				chip->sensor[i].high_thr_en = 0;
				chip->sensor[i].high_thr_triggered = true;
				mutex_unlock(&chip->adc_mutex_lock);
				queue_work(chip->sensor[i].req_wq,
						&chip->sensor[i].work);
			}
		}
	}

handler_end:
	return IRQ_HANDLED;
}

static int adc_tm7_register_interrupts(struct adc_tm_chip *chip)
{
	struct platform_device *pdev;
	int ret, irq;

	if (!chip)
		return -EINVAL;

	pdev = to_platform_device(chip->dev);

	irq = platform_get_irq_byname(pdev, "thr-int-en");
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get irq %s\n",
			"thr-int-en");
		return irq;
	}

	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
			adc_tm7_handler,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"thr-int-en", chip);
	if (ret) {
		dev_err(&pdev->dev, "failed to get irq %s\n",
				"thr-int-en");
		return ret;
	}

	enable_irq_wake(irq);

	return ret;
}

static int adc_tm7_init(struct adc_tm_chip *chip, uint32_t dt_chans)
{
	u8 channels_available;
	int ret;

	ret = adc_tm_read_reg(chip, ADC_TM_NUM_BTM_CHAN,
			&channels_available, 1);
	if (ret < 0) {
		pr_err("read failed for BTM channels\n");
		return ret;
	}

	if (dt_chans > channels_available) {
		pr_err("Number of nodes greater than channels supported:%d\n",
							channels_available);
		return -EINVAL;
	}

	mutex_init(&chip->adc_mutex_lock);

	return ret;
}

static int adc_tm7_shutdown(struct adc_tm_chip *chip)
{
	u8 data = 0;
	int i;

	for (i = 0; i < chip->dt_channels; i++)
		if (chip->sensor[i].req_wq)
			destroy_workqueue(chip->sensor[i].req_wq);

	mutex_lock(&chip->adc_mutex_lock);

	adc_tm_write_reg(chip, ADC_TM_EN_CTL1, &data, 1);
	data = ADC_TM_CONV_REQ_EN;
	adc_tm_write_reg(chip, ADC_TM_CONV_REQ, &data, 1);

	mutex_unlock(&chip->adc_mutex_lock);

	mutex_destroy(&chip->adc_mutex_lock);

	list_del(&chip->list);
	return 0;
}

static const struct adc_tm_ops ops_adc_tm7 = {
	.init		= adc_tm7_init,
	.set_trips	= adc_tm7_set_trip_temp,
	.interrupts_reg = adc_tm7_register_interrupts,
	.get_temp	= adc_tm_get_temp,
	.channel_measure = adc_tm7_channel_measure,
	.disable_chan = adc_tm7_disable_chan_meas,
	.notify = notify_adc_tm7_fn,
	.shutdown		= adc_tm7_shutdown,
};

const struct adc_tm_data data_adc_tm7 = {
	.ops			= &ops_adc_tm7,
	.decimation = (unsigned int []) {85, 340, 1360},
	.hw_settle = (unsigned int []) {15, 100, 200, 300, 400, 500, 600, 700,
					1000, 2000, 4000, 8000, 16000, 32000,
					64000, 128000},
};
