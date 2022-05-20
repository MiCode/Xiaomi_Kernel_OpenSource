// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <dt-bindings/iio/adc/richtek,rt9490-adc.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <linux/iio/iio.h>

#define RT9490_REG_ADCCTRL	0x2E
#define RT9490_REG_ADCCHAN0	0x2F
#define RT9490_REG_IBUSADC	0x31
#define RT9490_REG_IBATADC	0x33
#define RT9490_REG_VBUSADC	0x35
#define RT9490_REG_VAC1ADC	0x37
#define RT9490_REG_VAC2ADC	0x39
#define RT9490_REG_VBATADC	0x3B
#define RT9490_REG_VSYSADC	0x3D
#define RT9490_REG_TSADC	0x3F
#define RT9490_REG_TDIEADC	0x41
#define RT9490_REG_DPADC	0x43
#define RT9490_REG_DMADC	0x45

#define RT9490_ADCDONE_MASK	BIT(7)
#define RT9490_ONESHOT_ENVAL	(BIT(7) | BIT(6))
#define RT9490_ADCCONV_TIMEUS	5860
#define RT9490_ADC_SIGN_BIT	BIT(15)

struct rt9490_adc_data {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
};

static int rt9490_read_channel(struct rt9490_adc_data *data, const struct iio_chan_spec *chan,
			       int *val)
{
	__le16 chan_enable;
	__be16 chan_raw_data;
	unsigned int status;
	u16 tmp;
	int ret;

	mutex_lock(&data->lock);

	ret = regmap_write(data->regmap, RT9490_REG_ADCCTRL, 0);
	if (ret)
		goto out_read;

	chan_enable = cpu_to_le16(~BIT(chan->scan_index));
	ret = regmap_raw_write(data->regmap, RT9490_REG_ADCCHAN0, &chan_enable,
			       sizeof(chan_enable));
	if (ret)
		goto out_read;

	ret = regmap_write(data->regmap, RT9490_REG_ADCCTRL, RT9490_ONESHOT_ENVAL);
	if (ret)
		goto out_read;

	usleep_range(RT9490_ADCCONV_TIMEUS, RT9490_ADCCONV_TIMEUS + 1000);

	ret = regmap_read_poll_timeout(data->regmap, RT9490_REG_ADCCTRL, status,
				       !(status & RT9490_ADCDONE_MASK), 100,
				       RT9490_ADCCONV_TIMEUS);
	if (ret) {
		dev_err(data->dev, "adc read done flag [%d]\n", ret);
		goto out_read;
	}

	ret = regmap_raw_read(data->regmap, chan->address, &chan_raw_data,
			      sizeof(chan_raw_data));
	if (ret) {
		dev_err(data->dev, "Failed to read channel raw data\n");
		goto out_read;
	}

	tmp = be16_to_cpu(chan_raw_data);
	if (tmp & RT9490_ADC_SIGN_BIT) {
		tmp &= ~RT9490_ADC_SIGN_BIT;
		*val = (s16)(~tmp + 1);
	} else
		*val = tmp;

	ret = IIO_VAL_INT;

out_read:
	mutex_unlock(&data->lock);
	return ret;
}

static int rt9490_adc_read_raw(struct iio_dev *iio_dev, const struct iio_chan_spec *chan,
			       int *val, int *val2, long mask)
{
	struct rt9490_adc_data *data = iio_priv(iio_dev);

	return rt9490_read_channel(data, chan, val);
}

static const struct iio_info rt9490_adc_info = {
	.read_raw = rt9490_adc_read_raw,
};

#define RT9490_ADC_CHANNEL(_ch_name, _sc_idx, _type) \
{ \
	.type = _type, \
	.channel = RT9490_CHAN_##_ch_name, \
	.datasheet_name = #_ch_name, \
	.address = RT9490_REG_##_ch_name##ADC, \
	.scan_index = _sc_idx, \
	.indexed = 1, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
}

static const struct iio_chan_spec rt9490_adc_channels[] = {
	RT9490_ADC_CHANNEL(TDIE, 1, IIO_TEMP),
	RT9490_ADC_CHANNEL(TS, 2, IIO_VOLTAGE),
	RT9490_ADC_CHANNEL(VSYS, 3, IIO_VOLTAGE),
	RT9490_ADC_CHANNEL(VBAT, 4, IIO_VOLTAGE),
	RT9490_ADC_CHANNEL(VBUS, 5, IIO_VOLTAGE),
	RT9490_ADC_CHANNEL(IBAT, 6, IIO_CURRENT),
	RT9490_ADC_CHANNEL(IBUS, 7, IIO_CURRENT),
	RT9490_ADC_CHANNEL(VAC1, 12, IIO_VOLTAGE),
	RT9490_ADC_CHANNEL(VAC2, 13, IIO_VOLTAGE),
	RT9490_ADC_CHANNEL(DM, 14, IIO_VOLTAGE),
	RT9490_ADC_CHANNEL(DP, 15, IIO_VOLTAGE)
};

static int rt9490_adc_reset(struct rt9490_adc_data *data)
{
	/* For each chan, 0 is enable, 1 is disable */
	__le16 chan_enable = 0xffff;
	int ret;

	/* ADCEN = 0 */
	ret = regmap_write(data->regmap, RT9490_REG_ADCCTRL, 0);
	if (ret)
		return ret;

	/* All channel EN to disable */
	return regmap_raw_write(data->regmap, RT9490_REG_ADCCHAN0, &chan_enable,
				sizeof(chan_enable));
}

static int rt9490_adc_probe(struct platform_device *pdev)
{
	struct rt9490_adc_data *data;
	struct iio_dev *iio_dev;
	int ret;

	iio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*data));
	if (IS_ERR_OR_NULL(iio_dev)) {
		dev_err(&pdev->dev, "Failed to allocate iio device\n");
		return PTR_ERR_OR_ZERO(iio_dev);
	}

	data = iio_priv(iio_dev);
	data->dev = &pdev->dev;
	mutex_init(&data->lock);

	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap) {
		dev_err(&pdev->dev, "Failed to get parent regmap\n");
		return -ENODEV;
	}

	ret = rt9490_adc_reset(data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to reset adc hardware\n");
		return ret;
	}

	iio_dev->name = dev_name(&pdev->dev);
	iio_dev->info = &rt9490_adc_info;
	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->channels = rt9490_adc_channels;
	iio_dev->num_channels = ARRAY_SIZE(rt9490_adc_channels);

	return devm_iio_device_register(&pdev->dev, iio_dev);
}

static const struct of_device_id rt9490_adc_of_match_table[] = {
	{ .compatible = "richtek,rt9490-adc", },
	{ }
};
MODULE_DEVICE_TABLE(of, rt9490_adc_of_match_table);

static struct platform_driver rt9490_adc_driver = {
	.driver = {
		.name = "rt9490-adc",
		.of_match_table = rt9490_adc_of_match_table,
	},
	.probe = rt9490_adc_probe,
};
module_platform_driver(rt9490_adc_driver);

MODULE_DESCRIPTION("Richtek RT9490 ADC driver");
MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_LICENSE("GPL");
