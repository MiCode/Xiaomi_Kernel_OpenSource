/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

static struct thermal_zone_of_device_ops adc_tm_ops_iio = {
	.get_temp = adc_tm_get_temp,
};

static int adc_tm_register_tzd(struct adc_tm_chip *adc_tm, int dt_chan_num,
					bool set_trips)
{
	unsigned int i;
	struct thermal_zone_device *tzd;

	for (i = 0; i < dt_chan_num; i++) {
		adc_tm->sensor[i].chip = adc_tm;
		if (!adc_tm->sensor[i].non_thermal) {
			if (set_trips)
				tzd = devm_thermal_zone_of_sensor_register(
					adc_tm->dev, adc_tm->sensor[i].adc_ch,
					&adc_tm->sensor[i],	&adc_tm_ops);
			else
				tzd = devm_thermal_zone_of_sensor_register(
					adc_tm->dev, adc_tm->sensor[i].adc_ch,
					&adc_tm->sensor[i], &adc_tm_ops_iio);

			if (IS_ERR(tzd)) {
				pr_err("Error registering TZ zone:%d for dt_ch:%d\n",
					PTR_ERR(tzd), adc_tm->sensor[i].adc_ch);
				continue;
			}
			adc_tm->sensor[i].tzd = tzd;
		} else
			adc_tm->sensor[i].tzd = NULL;
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

struct adc_tm_chip *get_adc_tm(struct device *dev, const char *name)
{
	struct platform_device *pdev;
	struct adc_tm_chip *chip;
	struct device_node *node = NULL;
	char prop_name[MAX_PROP_NAME_LEN];

	snprintf(prop_name, MAX_PROP_NAME_LEN, "qcom,%s-adc_tm", name);

	node = of_parse_phandle(dev->of_node, prop_name, 0);
	if (node == NULL)
		return ERR_PTR(-ENODEV);

	list_for_each_entry(chip, &adc_tm_device_list, list) {
		pdev = to_platform_device(chip->dev);
		if (pdev->dev.of_node == node)
			return chip;
	}
	return ERR_PTR(-EPROBE_DEFER);
}
EXPORT_SYMBOL(get_adc_tm);

int adc_tm_is_valid(struct adc_tm_chip *chip)
{
	struct adc_tm_chip *adc_tm_chip = NULL;

	list_for_each_entry(adc_tm_chip, &adc_tm_device_list, list)
		if (chip == adc_tm_chip)
			return 0;

	return -EINVAL;
}

static const struct of_device_id adc_tm_match_table[] = {
	{
		.compatible = "qcom,adc-tm5",
		.data = &data_adc_tm5,
	},
	{
		.compatible = "qcom,adc-tm5-iio",
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
		int channel_num, i = 0, adc_rscale_fn = 0;
		int calib_type = 0, ret, hw_settle_time = 0;
		int prescal = 0;
		struct iio_channel *chan_adc;
		bool non_thermal = false;

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

		if (of_property_read_bool(child, "qcom,kernel-client"))
			non_thermal = true;

		ret = of_property_read_u32(child, "qcom,scale-type",
							&adc_rscale_fn);
		if (ret)
			adc_rscale_fn = SCALE_RSCALE_NONE;

		ret = of_property_read_u32(child, "qcom,prescaling", &prescal);
		if (ret)
			prescal = 1;

		/* Individual channel properties */
		adc_tm->sensor[idx].adc_ch = channel_num;
		adc_tm->sensor[idx].cal_sel = calib_type;
		/* Default to 1 second timer select */
		adc_tm->sensor[idx].timer_select = ADC_TIMER_SEL_2;
		adc_tm->sensor[idx].hw_settle_time = hw_settle_time;
		adc_tm->sensor[idx].adc_rscale_fn = adc_rscale_fn;
		adc_tm->sensor[idx].non_thermal = non_thermal;
		adc_tm->sensor[idx].prescaling = prescal;

		if (adc_tm->sensor[idx].non_thermal) {
			adc_tm->sensor[idx].req_wq = alloc_workqueue(
				"qpnp_adc_notify_wq", WQ_HIGHPRI, 0);
			if (!adc_tm->sensor[idx].req_wq) {
				pr_err("Requesting priority wq failed\n");
				return -ENOMEM;
			}
			INIT_WORK(&adc_tm->sensor[idx].work, notify_adc_tm_fn);
		}
		INIT_LIST_HEAD(&adc_tm->sensor[idx].thr_list);

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
	struct device_node *child, *revid_dev_node, *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct adc_tm_chip *adc_tm;
	struct regmap *regmap;
	struct iio_channel *channels;
	int ret = 0, dt_chan_num = 0, indio_chan_count = 0, i = 0;
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

	revid_dev_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (revid_dev_node) {
		adc_tm->pmic_rev_id = get_revid_data(revid_dev_node);
		if (IS_ERR(adc_tm->pmic_rev_id)) {
			pr_debug("Unable to get revid\n");
			return -EPROBE_DEFER;
		}
	}

	ret = adc_tm_get_dt_data(pdev, adc_tm, channels, dt_chan_num);
	if (ret) {
		dev_err(dev, "adc-tm get dt data failed\n");
		return ret;
	}

	if (of_device_is_compatible(node, "qcom,adc-tm5-iio")) {
		ret = adc_tm_register_tzd(adc_tm, dt_chan_num, false);
		if (ret) {
			dev_err(dev, "adc-tm failed to register with of thermal\n");
			goto fail;
		}
		return 0;
	}

	ret = adc_tm_init(adc_tm, dt_chan_num);
	if (ret) {
		dev_err(dev, "adc-tm init failed\n");
		goto fail;
	}

	ret = adc_tm_register_tzd(adc_tm, dt_chan_num, true);
	if (ret) {
		dev_err(dev, "adc-tm failed to register with of thermal\n");
		goto fail;
	}

	ret = adc_tm_register_interrupts(adc_tm);
	if (ret) {
		pr_err("adc-tm register interrupts failed:%d\n", ret);
		goto fail;
	}

	list_add_tail(&adc_tm->list, &adc_tm_device_list);
	platform_set_drvdata(pdev, adc_tm);
	return 0;
fail:
	i = 0;
	while (i < dt_chan_num) {
		if (adc_tm->sensor[i].req_wq)
			destroy_workqueue(adc_tm->sensor[i].req_wq);
		i++;
	}
	return ret;
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
