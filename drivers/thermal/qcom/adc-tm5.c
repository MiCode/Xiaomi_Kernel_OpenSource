/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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
	int ret, i = 0;
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
		int temp;

		if (IS_ERR(chip->sensor[i].tzd))
			continue;

		ret = adc_tm5_get_temp(&chip->sensor[i], &temp);
		if (ret < 0)
			continue;

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
		if (upper_set || lower_set) {
			/*
			 * Expected behavior is while notifying of_thermal,
			 * thermal core will call set_trips with new thresholds
			 * and activate/disable the appropriate trips.
			 */
			pr_debug("notifying of_thermal\n");
			of_thermal_handle_trip(chip->sensor[i].tzd);
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
	int ret, i;

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

	for (i = 0; i < dt_chans; i++)
		chip->sensor[i].btm_ch = adc_tm_ch_data[i].btm_amux_ch;

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
