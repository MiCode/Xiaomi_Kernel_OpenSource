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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/thermal.h>
#include <linux/iio/iio.h>
#include "adc-tm.h"

LIST_HEAD(adc_tm_device_list);

static int adc_tm_get_temp(void *data, int *temp)
{
	struct adc_tm_sensor *s = data;
	struct adc_tm_chip *adc_tm = s->chip;

	return adc_tm->ops->get_temp(s, temp);
}

static int adc_tm_set_trip_temp(void *data, int low_temp, int high_temp)
{
	struct adc_tm_sensor *s = data;
	struct adc_tm_chip *adc_tm = s->chip;

	if (adc_tm->ops->set_trips)
		return adc_tm->ops->set_trips(s, low_temp, high_temp);

	return 0;
}

static int adc_tm_register_interrupts(struct adc_tm_chip *adc_tm)
{
	if (adc_tm->ops->interrupts_reg)
		return adc_tm->ops->interrupts_reg(adc_tm);

	return 0;
}

static int adc_tm_init(struct adc_tm_chip *adc_tm, uint32_t dt_chans)
{
	if (adc_tm->ops->init)
		return adc_tm->ops->init(adc_tm, dt_chans);

	return 0;
}

static struct thermal_zone_of_device_ops adc_tm_ops = {
	.get_temp = adc_tm_get_temp,
	.set_trips = adc_tm_set_trip_temp,
};

static int adc_tm_register_tzd(struct adc_tm_chip *adc_tm, int dt_chan_num)
{
	unsigned int i;
	struct thermal_zone_device *tzd;

	for (i = 0; i < dt_chan_num; i++) {
		adc_tm->sensor[i].chip = adc_tm;
		tzd = devm_thermal_zone_of_sensor_register(adc_tm->dev,
						adc_tm->sensor[i].adc_ch,
						&adc_tm->sensor[i],
						&adc_tm_ops);
		if (IS_ERR(tzd)) {
			pr_err("Error registering TZ zone:%d for dt_ch:%d\n",
				PTR_ERR(tzd), adc_tm->sensor[i].adc_ch);
			continue;
		}
		adc_tm->sensor[i].tzd = tzd;
	}

	return 0;
}

static int adc_tm_avg_samples_from_dt(u32 value)
{
	if (!is_power_of_2(value) || value > ADC_TM_AVG_SAMPLES_MAX)
		return -EINVAL;

	return __ffs64(value);
}

static int adc_tm_hw_settle_time_from_dt(u32 value,
					const unsigned int *hw_settle)
{
	unsigned int i;

	for (i = 0; i < ADC_TM_HW_SETTLE_SAMPLES_MAX; i++) {
		if (value == hw_settle[i])
			return i;
	}

	return -EINVAL;
}

static int adc_tm_decimation_from_dt(u32 value, const unsigned int *decimation)
{
	unsigned int i;

	for (i = 0; i < ADC_TM_DECIMATION_SAMPLES_MAX; i++) {
		if (value == decimation[i])
			return i;
	}

	return -EINVAL;
}

static const struct of_device_id adc_tm_match_table[] = {
	{
		.compatible = "qcom,adc-tm5",
		.data = &data_adc_tm5,
	},
	{}
};

static int adc_tm_get_dt_data(struct platform_device *pdev,
				struct adc_tm_chip *adc_tm,
				struct iio_channel *chan,
				uint32_t dt_chan_num)
{
	struct device_node *child, *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	const struct of_device_id *id;
	const struct adc_tm_data *data;
	int ret, idx = 0;

	if (!node)
		return -EINVAL;

	id = of_match_node(adc_tm_match_table, node);
	if (id)
		data = id->data;
	else
		data = &data_adc_tm5;
	adc_tm->data = data;
	adc_tm->ops = data->ops;

	ret = of_property_read_u32(node, "qcom,decimation",
						&adc_tm->prop.decimation);
	if (!ret) {
		ret = adc_tm_decimation_from_dt(adc_tm->prop.decimation,
							data->decimation);
		if (ret < 0) {
			dev_err(dev, "Invalid decimation value\n");
			return ret;
		}
		adc_tm->prop.decimation = ret;
	} else {
		adc_tm->prop.decimation = ADC_TM_DECIMATION_DEFAULT;
	}

	ret = of_property_read_u32(node, "qcom,avg-samples",
						&adc_tm->prop.fast_avg_samples);
	if (!ret) {
		ret = adc_tm_avg_samples_from_dt(adc_tm->prop.fast_avg_samples);
		if (ret < 0) {
			dev_err(dev, "Invalid fast average with%d\n", ret);
			return -EINVAL;
		}
	} else {
		adc_tm->prop.fast_avg_samples = ADC_TM_DEF_AVG_SAMPLES;
	}

	adc_tm->prop.timer1 = ADC_TM_TIMER1;
	adc_tm->prop.timer2 = ADC_TM_TIMER2;
	adc_tm->prop.timer3 = ADC_TM_TIMER3;

	for_each_child_of_node(node, child) {
		int channel_num, i = 0;
		int calib_type = 0, ret, hw_settle_time = 0;
		struct iio_channel *chan_adc;

		ret = of_property_read_u32(child, "reg", &channel_num);
		if (ret) {
			dev_err(dev, "Invalid channel num\n");
			return -EINVAL;
		}

		ret = of_property_read_u32(child, "qcom,hw-settle-time",
							&hw_settle_time);
		if (!ret) {
			ret = adc_tm_hw_settle_time_from_dt(hw_settle_time,
							data->hw_settle);
			if (ret < 0) {
				pr_err("Invalid channel hw settle time property\n");
				return ret;
			}
			hw_settle_time = ret;
		} else {
			hw_settle_time = ADC_TM_DEF_HW_SETTLE_TIME;
		}

		if (of_property_read_bool(child, "qcom,ratiometric"))
			calib_type = ADC_RATIO_CAL;
		else
			calib_type = ADC_ABS_CAL;

		/* Individual channel properties */
		adc_tm->sensor[idx].adc_ch = channel_num;
		adc_tm->sensor[idx].cal_sel = calib_type;
		/* Default to 1 second timer select */
		adc_tm->sensor[idx].timer_select = ADC_TIMER_SEL_2;
		adc_tm->sensor[idx].hw_settle_time = hw_settle_time;
		while (i < dt_chan_num) {
			chan_adc = &chan[i];
			if (chan_adc->channel->channel == channel_num)
				adc_tm->sensor[idx].adc = chan_adc;
			i++;
		}
		idx++;
	}

	return 0;
}

static int adc_tm_probe(struct platform_device *pdev)
{
	struct device_node *child, *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct adc_tm_chip *adc_tm;
	struct regmap *regmap;
	struct iio_channel *channels;
	int ret, dt_chan_num = 0, indio_chan_count = 0;
	u32 reg;

	if (!node)
		return -EINVAL;

	for_each_child_of_node(node, child)
		dt_chan_num++;

	if (!dt_chan_num) {
		dev_err(dev, "No channel listing\n");
		return -EINVAL;
	}

	channels = iio_channel_get_all(dev);
	if (IS_ERR(channels))
		return PTR_ERR(channels);

	while (channels[indio_chan_count].indio_dev)
		indio_chan_count++;

	if (indio_chan_count != dt_chan_num) {
		dev_err(dev, "VADC IIO channel missing in main node\n");
		return -EINVAL;
	}

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap)
		return -ENODEV;

	ret = of_property_read_u32(node, "reg", &reg);
	if (ret < 0)
		return ret;

	adc_tm = devm_kzalloc(&pdev->dev,
			sizeof(struct adc_tm_chip) + (dt_chan_num *
			(sizeof(struct adc_tm_sensor))), GFP_KERNEL);
	if (!adc_tm)
		return -ENOMEM;

	adc_tm->regmap = regmap;
	adc_tm->dev = dev;
	adc_tm->base = reg;
	adc_tm->dt_channels = dt_chan_num;

	ret = adc_tm_get_dt_data(pdev, adc_tm, channels, dt_chan_num);
	if (ret) {
		dev_err(dev, "adc-tm get dt data failed\n");
		return ret;
	}

	ret = adc_tm_init(adc_tm, dt_chan_num);
	if (ret) {
		dev_err(dev, "adc-tm init failed\n");
		return ret;
	}

	ret = adc_tm_register_tzd(adc_tm, dt_chan_num);
	if (ret) {
		dev_err(dev, "adc-tm failed to register with of thermal\n");
		return ret;
	}

	ret = adc_tm_register_interrupts(adc_tm);
	if (ret) {
		pr_err("adc-tm register interrupts failed:%d\n", ret);
		return ret;
	}

	list_add_tail(&adc_tm->list, &adc_tm_device_list);
	platform_set_drvdata(pdev, adc_tm);

	return 0;
}

static int adc_tm_remove(struct platform_device *pdev)
{
	struct adc_tm_chip *adc_tm = platform_get_drvdata(pdev);

	if (adc_tm->ops->shutdown)
		adc_tm->ops->shutdown(adc_tm);

	return 0;
}

static struct platform_driver adc_tm_driver = {
	.driver = {
		.name = "qcom,adc-tm",
		.of_match_table	= adc_tm_match_table,
	},
	.probe = adc_tm_probe,
	.remove = adc_tm_remove,
};
module_platform_driver(adc_tm_driver);

MODULE_DESCRIPTION("Qualcomm Technologies Inc. PMIC ADC_TM driver");
MODULE_LICENSE("GPL v2");
