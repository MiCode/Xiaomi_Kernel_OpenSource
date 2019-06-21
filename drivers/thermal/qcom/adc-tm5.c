// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/iio/consumer.h>
#include "adc-tm.h"
#include "../thermal_core.h"

#define ADC_TM_STATUS2				0x09
#define ADC_TM_STATUS_LOW			0x0a
#define ADC_TM_STATUS_HIGH			0x0b
#define ADC_TM_NUM_BTM				0x0f

#define ADC_TM_ADC_DIG_PARAM			0x42
#define ADC_TM_FAST_AVG_CTL			0x43
#define ADC_TM_FAST_AVG_EN			BIT(7)

#define ADC_TM_MEAS_INTERVAL_CTL		0x44
#define ADC_TM_MEAS_INTERVAL_CTL2		0x45

#define ADC_TM_MEAS_INTERVAL_CTL2_SHIFT		0x4
#define ADC_TM_MEAS_INTERVAL_CTL2_MASK		0xf0
#define ADC_TM_MEAS_INTERVAL_CTL3_MASK		0xf

#define ADC_TM_EN_CTL1				0x46
#define ADC_TM_EN				BIT(7)
#define ADC_TM_CONV_REQ				0x47
#define ADC_TM_CONV_REQ_EN			BIT(7)

#define ADC_TM_Mn_ADC_CH_SEL_CTL(n)		((n * 8) + 0x60)
#define ADC_TM_Mn_LOW_THR0(n)			((n * 8) + 0x61)
#define ADC_TM_Mn_LOW_THR1(n)			((n * 8) + 0x62)
#define ADC_TM_Mn_HIGH_THR0(n)			((n * 8) + 0x63)
#define ADC_TM_Mn_HIGH_THR1(n)			((n * 8) + 0x64)
#define ADC_TM_Mn_MEAS_INTERVAL_CTL(n)		((n * 8) + 0x65)
#define ADC_TM_Mn_CTL(n)			((n * 8) + 0x66)
#define ADC_TM_CTL_HW_SETTLE_DELAY_MASK		0xf
#define ADC_TM_CTL_CAL_SEL			0x30
#define ADC_TM_CTL_CAL_SEL_MASK_SHIFT		4
#define ADC_TM_CTL_CAL_VAL			0x40

#define ADC_TM_Mn_EN(n)				((n * 8) + 0x67)
#define ADC_TM_Mn_MEAS_EN			BIT(7)
#define ADC_TM_Mn_HIGH_THR_INT_EN		BIT(1)
#define ADC_TM_Mn_LOW_THR_INT_EN		BIT(0)
#define ADC_TM_LOWER_MASK(n)			((n) & 0x000000ff)
#define ADC_TM_UPPER_MASK(n)			(((n) & 0xffffff00) >> 8)

#define ADC_TM_Mn_DATA0(n)			((n * 2) + 0xa0)
#define ADC_TM_Mn_DATA1(n)			((n * 2) + 0xa1)
#define ADC_TM_DATA_SHIFT			8

static struct adc_tm_trip_reg_type adc_tm_ch_data[] = {
	[ADC_TM_CHAN0] = {ADC_TM_M0_ADC_CH_SEL_CTL},
	[ADC_TM_CHAN1] = {ADC_TM_M1_ADC_CH_SEL_CTL},
	[ADC_TM_CHAN2] = {ADC_TM_M2_ADC_CH_SEL_CTL},
	[ADC_TM_CHAN3] = {ADC_TM_M3_ADC_CH_SEL_CTL},
	[ADC_TM_CHAN4] = {ADC_TM_M4_ADC_CH_SEL_CTL},
	[ADC_TM_CHAN5] = {ADC_TM_M5_ADC_CH_SEL_CTL},
	[ADC_TM_CHAN6] = {ADC_TM_M6_ADC_CH_SEL_CTL},
	[ADC_TM_CHAN7] = {ADC_TM_M7_ADC_CH_SEL_CTL},
};

static struct adc_tm_reverse_scale_fn adc_tm_rscale_fn[] = {
	[SCALE_R_ABSOLUTE] = {adc_tm_absolute_rthr},
};

static int adc_tm5_get_temp(struct adc_tm_sensor *sensor, int *temp)
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

static int32_t adc_tm5_read_reg(struct adc_tm_chip *chip,
					int16_t reg, u8 *data, int len)
{
	int ret;

	ret = regmap_bulk_read(chip->regmap, (chip->base + reg), data, len);
	if (ret < 0)
		pr_err("adc-tm read reg %d failed with %d\n", reg, ret);

	return ret;
}

static int32_t adc_tm5_write_reg(struct adc_tm_chip *chip,
					int16_t reg, u8 *data, int len)
{
	int ret;

	ret = regmap_bulk_write(chip->regmap, (chip->base + reg), data, len);
	if (ret < 0)
		pr_err("adc-tm write reg %d failed with %d\n", reg, ret);

	return ret;
}

static int32_t adc_tm5_reg_update(struct adc_tm_chip *chip,
				uint16_t addr, u8 mask, bool state)
{
	u8 reg_value = 0;
	int ret;

	ret = adc_tm5_read_reg(chip, addr, &reg_value, 1);
	if (ret < 0) {
		pr_err("read failed for addr:0x%x\n", addr);
		return ret;
	}

	reg_value = reg_value & ~mask;
	if (state)
		reg_value |= mask;

	pr_debug("state:%d, reg:0x%x with bits:0x%x and mask:0x%x\n",
					state, addr, reg_value, ~mask);
	ret = adc_tm5_write_reg(chip, addr, &reg_value, 1);
	if (ret < 0) {
		pr_err("write failed for addr:%x\n", addr);
		return ret;
	}

	return ret;
}

static int32_t adc_tm5_get_btm_idx(struct adc_tm_chip *chip,
				uint32_t btm_chan, uint32_t *btm_chan_idx)
{
	int i;

	for (i = 0; i < ADC_TM_CHAN_NONE; i++) {
		if (adc_tm_ch_data[i].btm_amux_ch == btm_chan) {
			*btm_chan_idx = i;
			return 0;
		}
	}

	return -EINVAL;
}

static int32_t adc_tm5_enable(struct adc_tm_chip *chip)
{
	int rc = 0;
	u8 data = 0;

	data = ADC_TM_EN;
	rc = adc_tm5_write_reg(chip, ADC_TM_EN_CTL1, &data, 1);
	if (rc < 0) {
		pr_err("adc-tm enable failed\n");
		return rc;
	}

	data = ADC_TM_CONV_REQ_EN;
	rc = adc_tm5_write_reg(chip, ADC_TM_CONV_REQ, &data, 1);
	if (rc < 0) {
		pr_err("adc-tm request conversion failed\n");
		return rc;
	}

	return rc;
}

static int adc_tm5_configure(struct adc_tm_sensor *sensor,
					uint32_t btm_chan_idx)
{
	struct adc_tm_chip *chip = sensor->chip;
	u8 buf[8], cal_sel;
	int ret = 0;

	ret = adc_tm5_read_reg(chip,
			ADC_TM_Mn_ADC_CH_SEL_CTL(btm_chan_idx), buf, 8);
	if (ret < 0) {
		pr_err("adc-tm block read failed with %d\n", ret);
		return ret;
	}

	/* Update ADC channel select */
	buf[0] = sensor->adc_ch;

	/* Update timer select */
	buf[5] = sensor->timer_select;

	/* Set calibration select, hw_settle delay */
	cal_sel = (u8) (sensor->cal_sel << ADC_TM_CTL_CAL_SEL_MASK_SHIFT);
	buf[6] &= (u8) ~ADC_TM_CTL_HW_SETTLE_DELAY_MASK;
	buf[6] |= (u8) sensor->hw_settle_time;
	buf[6] &= (u8) ~ADC_TM_CTL_CAL_SEL;
	buf[6] |= (u8) cal_sel;

	buf[7] |= ADC_TM_Mn_MEAS_EN;

	ret = adc_tm5_write_reg(chip,
			ADC_TM_Mn_ADC_CH_SEL_CTL(btm_chan_idx), buf, 8);
	if (ret < 0) {
		pr_err("adc-tm block write failed with %d\n", ret);
		return ret;
	}

	return 0;
}

static int32_t adc_tm_add_to_list(struct adc_tm_chip *chip,
				uint32_t dt_index,
				struct adc_tm_param *param)
{
	struct adc_tm_client_info *client_info = NULL;
	bool client_info_exists = false;

	list_for_each_entry(client_info,
			&chip->sensor[dt_index].thr_list, list) {
		if (client_info->param == param) {
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
		client_info->param = param;
		client_info->low_thr_requested = param->low_thr;
		client_info->high_thr_requested = param->high_thr;
		client_info->state_request = param->state_request;

		list_add_tail(&client_info->list,
					&chip->sensor[dt_index].thr_list);
	}
	return 0;
}

static int32_t adc_tm5_thr_update(struct adc_tm_sensor *sensor,
			int32_t high_thr, int32_t low_thr)
{
	int ret = 0;
	u8 trip_low_thr[2], trip_high_thr[2];
	uint16_t reg_low_thr_lsb, reg_high_thr_lsb;
	uint32_t scale_type = 0, mask = 0, btm_chan_idx = 0;
	struct adc_tm_config tm_config;
	struct adc_tm_chip *chip = NULL;

	ret = adc_tm5_get_btm_idx(chip,
		sensor->btm_ch, &btm_chan_idx);
	if (ret < 0) {
		pr_err("Invalid btm channel idx\n");
		return ret;
	}

	chip = sensor->chip;

	tm_config.high_thr_voltage = (int64_t)high_thr;
	tm_config.low_thr_voltage = (int64_t)low_thr;
	tm_config.prescal = sensor->prescaling;

	scale_type = sensor->adc_rscale_fn;
	if (scale_type >= SCALE_RSCALE_NONE) {
		ret = -EBADF;
		return ret;
	}

	adc_tm_rscale_fn[scale_type].chan(chip->data, &tm_config);

	mask = lower_32_bits(tm_config.high_thr_voltage);
	trip_high_thr[0] = ADC_TM_LOWER_MASK(mask);
	trip_high_thr[1] = ADC_TM_UPPER_MASK(mask);

	mask = lower_32_bits(tm_config.low_thr_voltage);
	trip_low_thr[0] = ADC_TM_LOWER_MASK(mask);
	trip_low_thr[1] = ADC_TM_UPPER_MASK(mask);

	pr_debug("high_thr:0x%llx, low_thr:0x%llx\n",
		tm_config.high_thr_voltage, tm_config.low_thr_voltage);

	reg_low_thr_lsb = ADC_TM_Mn_LOW_THR0(btm_chan_idx);
	reg_high_thr_lsb = ADC_TM_Mn_HIGH_THR0(btm_chan_idx);

	if (low_thr != INT_MIN) {
		ret = adc_tm5_write_reg(chip, reg_low_thr_lsb,
						trip_low_thr, 2);
		if (ret) {
			pr_err("Low set threshold err\n");
			return ret;
		}
	}

	if (high_thr != INT_MAX) {
		ret = adc_tm5_write_reg(chip, reg_high_thr_lsb,
						trip_high_thr, 2);
		if (ret) {
			pr_err("High set threshold err\n");
			return ret;
		}
	}
	return ret;
}

static int32_t adc_tm5_manage_thresholds(struct adc_tm_sensor *sensor)
{
	int ret = 0, high_thr = INT_MAX, low_thr = INT_MIN;
	struct adc_tm_client_info *client_info = NULL;
	struct list_head *thr_list;
	uint32_t btm_chan_idx = 0;
	struct adc_tm_chip *chip = sensor->chip;

	ret = adc_tm5_get_btm_idx(chip, sensor->btm_ch, &btm_chan_idx);
	if (ret < 0) {
		pr_err("Invalid btm channel idx with %d\n", ret);
		return ret;
	}
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
			if (high_thr == client_info->high_thr_requested)
				client_info->high_thr_set = true;

		if ((client_info->state_request == ADC_TM_LOW_THR_ENABLE) ||
			(client_info->state_request ==
				ADC_TM_HIGH_LOW_THR_ENABLE))
			if (low_thr == client_info->low_thr_requested)
				client_info->low_thr_set = true;
	}

	ret = adc_tm5_thr_update(sensor, high_thr, low_thr);
	if (ret < 0)
		pr_err("setting chan:%d threshold failed\n", btm_chan_idx);

	pr_debug("threshold written is high:%d and low:%d\n",
							high_thr, low_thr);

	return 0;
}

void notify_adc_tm_fn(struct work_struct *work)
{
	struct adc_tm_client_info *client_info = NULL;
	struct adc_tm_chip *chip;
	struct list_head *thr_list;
	uint32_t btm_chan_num = 0, btm_chan_idx = 0;
	int ret = 0;

	struct adc_tm_sensor *adc_tm = container_of(work,
		struct adc_tm_sensor, work);

	chip = adc_tm->chip;

	btm_chan_num = adc_tm->btm_ch;
	ret = adc_tm5_get_btm_idx(chip, btm_chan_num, &btm_chan_idx);
	if (ret < 0) {
		pr_err("Invalid btm channel idx\n");
		return;
	}

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
		adc_tm5_manage_thresholds(adc_tm);

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
		adc_tm5_manage_thresholds(adc_tm);

		adc_tm->high_thr_triggered = false;
	}
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

int32_t adc_tm5_channel_measure(struct adc_tm_chip *chip,
					struct adc_tm_param *param)

{
	int ret = 0, i = 0;
	uint32_t channel, dt_index = 0, btm_chan_idx = 0;
	bool chan_found = false, high_thr_set = false, low_thr_set = false;
	struct adc_tm_client_info *client_info = NULL;

	ret = adc_tm_is_valid(chip);
	if (ret || (param == NULL))
		return -EINVAL;

	if (param->threshold_notification == NULL) {
		pr_debug("No notification for high/low temp\n");
		return -EINVAL;
	}

	mutex_lock(&chip->adc_mutex_lock);

	channel = param->channel;

	while (i < chip->dt_channels) {
		if (chip->sensor[i].adc_ch == channel) {
			dt_index = i;
			chan_found = true;
			break;
		}
		i++;
	}

	if (!chan_found)  {
		pr_err("not a valid ADC_TM channel\n");
		ret = -EINVAL;
		goto fail_unlock;
	}

	ret = adc_tm5_get_btm_idx(chip,
		chip->sensor[dt_index].btm_ch, &btm_chan_idx);
	if (ret < 0) {
		pr_err("Invalid btm channel idx with %d\n", ret);
		goto fail_unlock;
	}

	/* add channel client to channel list */
	adc_tm_add_to_list(chip, dt_index, param);

	/* set right thresholds for the sensor */
	adc_tm5_manage_thresholds(&chip->sensor[dt_index]);

	/* enable low/high irqs */
	list_for_each_entry(client_info,
			&chip->sensor[dt_index].thr_list, list) {
		if (client_info->high_thr_set)
			high_thr_set = true;
		if (client_info->low_thr_set)
			low_thr_set = true;
	}

	if (low_thr_set) {
		/* Enable low threshold's interrupt */
		pr_debug("low sensor:%x with state:%d\n",
				dt_index, param->state_request);
		ret = adc_tm5_reg_update(chip,
			ADC_TM_Mn_EN(btm_chan_idx),
			ADC_TM_Mn_LOW_THR_INT_EN, true);
		if (ret < 0) {
			pr_err("low thr enable err:%d\n",
				chip->sensor[dt_index].btm_ch);
			goto fail_unlock;
		}
	}

	if (high_thr_set) {
		/* Enable high threshold's interrupt */
		pr_debug("high sensor mask:%x with state:%d\n",
			dt_index, param->state_request);
		ret = adc_tm5_reg_update(chip,
			ADC_TM_Mn_EN(btm_chan_idx),
			ADC_TM_Mn_HIGH_THR_INT_EN, true);
		if (ret < 0) {
			pr_err("high thr enable err:%d\n",
				chip->sensor[dt_index].btm_ch);
			goto fail_unlock;
		}
	}

	/* configure channel */
	ret = adc_tm5_configure(&chip->sensor[dt_index], btm_chan_idx);
	if (ret < 0) {
		pr_err("Error during adc-tm configure:%d\n", ret);
		goto fail_unlock;
	}

	ret = adc_tm5_enable(chip);
	if (ret < 0)
		pr_err("Error enabling adc-tm with %d\n", ret);

fail_unlock:
	mutex_unlock(&chip->adc_mutex_lock);
	return ret;
}
EXPORT_SYMBOL(adc_tm5_channel_measure);

int32_t adc_tm5_disable_chan_meas(struct adc_tm_chip *chip,
					struct adc_tm_param *param)
{
	int ret = 0, i = 0;
	uint32_t channel, dt_index = 0, btm_chan_idx = 0;
	unsigned long flags;

	ret = adc_tm_is_valid(chip);
	if (ret || (param == NULL))
		return -EINVAL;

	channel = param->channel;

	while (i < chip->dt_channels) {
		if (chip->sensor[i].adc_ch == channel) {
			dt_index = i;
			break;
		}
		i++;
	}

	if (i == chip->dt_channels)  {
		pr_err("not a valid ADC_TM channel\n");
		return -EINVAL;
	}

	ret = adc_tm5_get_btm_idx(chip,
		chip->sensor[dt_index].btm_ch, &btm_chan_idx);
	if (ret < 0) {
		pr_err("Invalid btm channel idx with %d\n", ret);
		return ret;
	}

	spin_lock_irqsave(&chip->adc_tm_lock, flags);

	ret = adc_tm5_reg_update(chip, ADC_TM_Mn_EN(btm_chan_idx),
				ADC_TM_Mn_HIGH_THR_INT_EN, false);
	if (ret < 0) {
		pr_err("high thr disable err\n");
		goto fail;
	}

	ret = adc_tm5_reg_update(chip, ADC_TM_Mn_EN(btm_chan_idx),
			ADC_TM_Mn_LOW_THR_INT_EN, false);
	if (ret < 0) {
		pr_err("low thr disable err\n");
		goto fail;
	}

	ret = adc_tm5_reg_update(chip, ADC_TM_Mn_EN(btm_chan_idx),
			ADC_TM_Mn_MEAS_EN, false);
	if (ret < 0)
		pr_err("multi measurement disable failed\n");

fail:
	spin_unlock_irqrestore(&chip->adc_tm_lock, flags);
	return ret;
}
EXPORT_SYMBOL(adc_tm5_disable_chan_meas);

static int adc_tm5_set_mode(struct adc_tm_sensor *sensor,
			      enum thermal_device_mode mode)
{
	struct adc_tm_chip *chip = sensor->chip;
	int ret = 0;
	uint32_t btm_chan_idx = 0;

	ret = adc_tm5_get_btm_idx(chip, sensor->btm_ch, &btm_chan_idx);
	if (ret < 0) {
		pr_err("Invalid btm channel idx with %d\n", ret);
		return ret;
	}

	if (mode == THERMAL_DEVICE_ENABLED) {
		ret = adc_tm5_configure(sensor, btm_chan_idx);
		if (ret < 0) {
			pr_err("Error during adc-tm configure:%d\n", ret);
			return ret;
		}

		ret = adc_tm5_enable(chip);
		if (ret < 0)
			pr_err("Error enabling adc-tm with %d\n", ret);

	} else if (mode == THERMAL_DEVICE_DISABLED) {
		ret = adc_tm5_reg_update(chip,
				ADC_TM_Mn_EN(btm_chan_idx),
				ADC_TM_Mn_MEAS_EN, false);
		if (ret < 0)
			pr_err("Disable failed for ch:%d\n", btm_chan_idx);
	}

	return ret;
}

static int adc_tm5_activate_trip_type(struct adc_tm_sensor *adc_tm,
			int trip, enum thermal_device_mode mode)
{
	struct adc_tm_chip *chip = adc_tm->chip;
	int ret = 0;
	bool state = false;
	uint32_t btm_chan_idx = 0, btm_chan = 0;

	if (mode == THERMAL_DEVICE_ENABLED)
		state = true;

	btm_chan = adc_tm->btm_ch;
	ret = adc_tm5_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (ret < 0) {
		pr_err("Invalid btm channel idx\n");
		return ret;
	}

	switch (trip) {
	case THERMAL_TRIP_CONFIGURABLE_HI:
		/* low_thr (lower voltage) for higher temp */
		ret = adc_tm5_reg_update(chip,
				ADC_TM_Mn_EN(btm_chan_idx),
				ADC_TM_Mn_LOW_THR_INT_EN, state);
		if (ret)
			pr_err("channel:%x failed\n", btm_chan);
	break;
	case THERMAL_TRIP_CONFIGURABLE_LOW:
		/* high_thr (higher voltage) for cooler temp */
		ret = adc_tm5_reg_update(chip,
				ADC_TM_Mn_EN(btm_chan_idx),
				ADC_TM_Mn_HIGH_THR_INT_EN, state);
		if (ret)
			pr_err("channel:%x failed\n", btm_chan);
	break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int adc_tm5_set_trip_temp(struct adc_tm_sensor *sensor,
					int low_temp, int high_temp)
{
	struct adc_tm_chip *chip;
	struct adc_tm_config tm_config;
	u8 trip_low_thr[2], trip_high_thr[2];
	uint16_t reg_low_thr_lsb, reg_high_thr_lsb;
	int ret;
	uint32_t btm_chan = 0, btm_chan_idx = 0, mask = 0;
	unsigned long flags;

	if (!sensor)
		return -EINVAL;

	pr_debug("%s:low_temp(mdegC):%d, high_temp(mdegC):%d\n", __func__,
							low_temp, high_temp);

	chip = sensor->chip;
	tm_config.channel = sensor->adc_ch;
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
	adc_tm_scale_therm_voltage_100k(&tm_config, chip->data);

	/* Cool temperature corresponds to high voltage threshold */
	mask = lower_32_bits(tm_config.high_thr_voltage);
	trip_high_thr[0] = ADC_TM_LOWER_MASK(mask);
	trip_high_thr[1] = ADC_TM_UPPER_MASK(mask);
	/* Warm temperature corresponds to low voltage threshold */
	mask = lower_32_bits(tm_config.low_thr_voltage);
	trip_low_thr[0] = ADC_TM_LOWER_MASK(mask);
	trip_low_thr[1] = ADC_TM_UPPER_MASK(mask);

	pr_debug("high_thr:0x%llx, low_thr:0x%llx\n",
		tm_config.high_thr_voltage, tm_config.low_thr_voltage);

	btm_chan = sensor->btm_ch;
	ret = adc_tm5_get_btm_idx(chip, btm_chan, &btm_chan_idx);
	if (ret < 0) {
		pr_err("Invalid btm channel idx\n");
		return ret;
	}

	spin_lock_irqsave(&chip->adc_tm_lock, flags);

	reg_low_thr_lsb = ADC_TM_Mn_LOW_THR0(btm_chan_idx);
	reg_high_thr_lsb = ADC_TM_Mn_HIGH_THR0(btm_chan_idx);

	if (high_temp != INT_MAX) {
		ret = adc_tm5_write_reg(chip, reg_low_thr_lsb,
						trip_low_thr, 2);
		if (ret) {
			pr_err("Warm set threshold err\n");
			goto fail;
		}

		ret = adc_tm5_activate_trip_type(sensor,
				THERMAL_TRIP_CONFIGURABLE_HI,
				THERMAL_DEVICE_ENABLED);
		if (ret) {
			pr_err("adc-tm warm activation failed\n");
			goto fail;
		}
	} else {
		ret = adc_tm5_activate_trip_type(sensor,
				THERMAL_TRIP_CONFIGURABLE_HI,
				THERMAL_DEVICE_DISABLED);
		if (ret) {
			pr_err("adc-tm warm deactivation failed\n");
			goto fail;
		}
	}

	if (low_temp != INT_MIN) {
		ret = adc_tm5_write_reg(chip, reg_high_thr_lsb,
						trip_high_thr, 2);
		if (ret) {
			pr_err("adc-tm cool temp set threshold err\n");
			goto fail;
		}

		ret = adc_tm5_activate_trip_type(sensor,
				THERMAL_TRIP_CONFIGURABLE_LOW,
				THERMAL_DEVICE_ENABLED);
		if (ret) {
			pr_err("adc-tm cool activation failed\n");
			goto fail;
		}
	} else {
		ret = adc_tm5_activate_trip_type(sensor,
				THERMAL_TRIP_CONFIGURABLE_LOW,
				THERMAL_DEVICE_DISABLED);
		if (ret) {
			pr_err("adc-tm cool deactivation failed\n");
			goto fail;
		}
	}

	if ((high_temp != INT_MAX) || (low_temp != INT_MIN)) {
		ret = adc_tm5_set_mode(sensor, THERMAL_DEVICE_ENABLED);
		if (ret)
			pr_err("sensor enabled failed\n");
	} else {
		ret = adc_tm5_set_mode(sensor, THERMAL_DEVICE_DISABLED);
		if (ret)
			pr_err("sensor disable failed\n");
	}

fail:
	spin_unlock_irqrestore(&chip->adc_tm_lock, flags);

	return ret;
}

static irqreturn_t adc_tm5_handler(int irq, void *data)
{
	struct adc_tm_chip *chip = data;
	u8 status_low, status_high, ctl;
	int ret = 0, i = 0;
	unsigned long flags;

	ret = adc_tm5_read_reg(chip, ADC_TM_STATUS_LOW, &status_low, 1);
	if (ret < 0) {
		pr_err("adc-tm-tm read status low failed with %d\n", ret);
		return IRQ_HANDLED;
	}

	ret = adc_tm5_read_reg(chip, ADC_TM_STATUS_HIGH, &status_high, 1);
	if (ret < 0) {
		pr_err("adc-tm-tm read status high failed with %d\n", ret);
		return IRQ_HANDLED;
	}

	while (i < chip->dt_channels) {
		bool upper_set = false, lower_set = false;
		u8 data_low = 0, data_high = 0;
		u16 code = 0;
		int temp;

		if (!chip->sensor[i].non_thermal &&
				IS_ERR(chip->sensor[i].tzd)) {
			pr_err("thermal device not found\n");
			i++;
			continue;
		}

		if (!chip->sensor[i].non_thermal) {
			ret = adc_tm5_get_temp(&chip->sensor[i], &temp);
			if (ret < 0) {
				i++;
				continue;
			}
			ret = adc_tm5_read_reg(chip, ADC_TM_Mn_DATA0(i),
						&data_low, 1);
			if (ret)
				pr_err("adc_tm data_low read failed with %d\n",
							ret);
			ret = adc_tm5_read_reg(chip, ADC_TM_Mn_DATA1(i),
						&data_high, 1);
			if (ret)
				pr_err("adc_tm data_high read failed with %d\n",
							ret);
			code = ((data_high << ADC_TM_DATA_SHIFT) | data_low);
		}

		spin_lock_irqsave(&chip->adc_tm_lock, flags);

		ret = adc_tm5_read_reg(chip, ADC_TM_Mn_EN(i), &ctl, 1);
		if (ret) {
			pr_err("ctl read failed with %d\n", ret);
			goto fail;
		}

		if ((status_low & 0x1) && (ctl & ADC_TM_Mn_MEAS_EN)
				&& (ctl & ADC_TM_Mn_LOW_THR_INT_EN))
			lower_set = true;

		if ((status_high & 0x1) && (ctl & ADC_TM_Mn_MEAS_EN) &&
					(ctl & ADC_TM_Mn_HIGH_THR_INT_EN))
			upper_set = true;
fail:
		status_low >>= 1;
		status_high >>= 1;
		spin_unlock_irqrestore(&chip->adc_tm_lock, flags);
		if (!(upper_set || lower_set)) {
			i++;
			continue;
		}

		if (!chip->sensor[i].non_thermal) {
			/*
			 * Expected behavior is while notifying
			 * of_thermal, thermal core will call set_trips
			 * with new thresholds and activate/disable
			 * the appropriate trips.
			 */
			pr_debug("notifying of_thermal\n");
			temp = therm_fwd_scale((int64_t)code,
						ADC_HC_VDD_REF, chip->data);
			of_thermal_handle_trip_temp(chip->sensor[i].tzd,
						temp);
		} else {
			if (lower_set) {
				ret = adc_tm5_reg_update(chip,
					ADC_TM_Mn_EN(i),
					ADC_TM_Mn_LOW_THR_INT_EN,
					false);
				if (ret < 0) {
					pr_err("low thr disable failed\n");
					return IRQ_HANDLED;
				}

				chip->sensor[i].low_thr_triggered
				= true;

				queue_work(chip->sensor[i].req_wq,
						&chip->sensor[i].work);
			}

			if (upper_set) {
				ret = adc_tm5_reg_update(chip,
					ADC_TM_Mn_EN(i),
					ADC_TM_Mn_HIGH_THR_INT_EN,
					false);
				if (ret < 0) {
					pr_err("high thr disable failed\n");
					return IRQ_HANDLED;
				}

				chip->sensor[i].high_thr_triggered = true;

				queue_work(chip->sensor[i].req_wq,
						&chip->sensor[i].work);
			}
		}
		i++;
	}
	return IRQ_HANDLED;
}

static int adc_tm5_register_interrupts(struct adc_tm_chip *chip)
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
			adc_tm5_handler,
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

static int adc_tm5_init(struct adc_tm_chip *chip, uint32_t dt_chans)
{
	u8 buf[4], channels_available, meas_int_timer_2_3 = 0;
	int ret;
	unsigned int offset_btm_idx = 0, i;

	ret = adc_tm5_read_reg(chip, ADC_TM_NUM_BTM, &channels_available, 1);
	if (ret < 0) {
		pr_err("read failed for BTM channels\n");
		return ret;
	}

	if (dt_chans > channels_available) {
		pr_err("Number of nodes greater than channels supported:%d\n",
							channels_available);
		return -EINVAL;
	}

	ret = adc_tm5_read_reg(chip,
			ADC_TM_ADC_DIG_PARAM, buf, 4);
	if (ret < 0) {
		pr_err("adc-tm block read failed with %d\n", ret);
		return ret;
	}

	/* Select decimation */
	buf[0] = chip->prop.decimation;

	/* Select number of samples in fast average mode */
	buf[1] = chip->prop.fast_avg_samples | ADC_TM_FAST_AVG_EN;

	/* Select timer1 */
	buf[2] = chip->prop.timer1;

	/* Select timer2 and timer3 */
	meas_int_timer_2_3 |= chip->prop.timer2 <<
				ADC_TM_MEAS_INTERVAL_CTL2_SHIFT;
	meas_int_timer_2_3 |= chip->prop.timer3;
	buf[3] = meas_int_timer_2_3;

	ret = adc_tm5_write_reg(chip,
			ADC_TM_ADC_DIG_PARAM, buf, 4);
	if (ret < 0)
		pr_err("adc-tm block write failed with %d\n", ret);

	spin_lock_init(&chip->adc_tm_lock);
	mutex_init(&chip->adc_mutex_lock);

	if (chip->pmic_rev_id) {
		switch (chip->pmic_rev_id->pmic_subtype)
		/*
		 * PM8150B 1.0 CH_0 and CH_1 is already used.
		 * Therefore configure to use CH_2 onwards.
		 */
		case PM8150B_SUBTYPE:
			if (chip->pmic_rev_id->rev4 == PM8150B_V1P0_REV4)
				offset_btm_idx = ADC_TM_CHAN2;
	}

	for (i = 0; i < dt_chans; i++) {
		if ((i + offset_btm_idx) > ADC_TM_CHAN7) {
			pr_err("Invalid BTM index %d\n", (i + offset_btm_idx));
			return -EINVAL;
		}

		chip->sensor[i].btm_ch =
				adc_tm_ch_data[i + offset_btm_idx].btm_amux_ch;
	}

	return ret;
}

static const struct adc_tm_ops ops_adc_tm5 = {
	.init		= adc_tm5_init,
	.set_trips	= adc_tm5_set_trip_temp,
	.interrupts_reg = adc_tm5_register_interrupts,
	.get_temp	= adc_tm5_get_temp,
};

const struct adc_tm_data data_adc_tm5 = {
	.ops			= &ops_adc_tm5,
	.full_scale_code_volt	= 0x70e4,
	.decimation = (unsigned int []) {250, 420, 840},
	.hw_settle = (unsigned int []) {15, 100, 200, 300, 400, 500, 600, 700,
					1, 2, 4, 8, 16, 32, 64, 128},
};
