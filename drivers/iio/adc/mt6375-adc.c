// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <dt-bindings/iio/adc/mediatek,mt6375_adc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

#define MT6375_REG_ADC_MONRPTH	0x19C
#define MT6375_REG_ADC_CFG1	0x1A4
#define MT6375_REG_ADC_CFG3	0x1A6
#define MT6375_REG_ADC_CHEN	0x1A7
#define MT6375_REG_ADC_CH1RPTH	0x1AA

#define MT6375_ADCEN_MASK	BIT(7)
#define MT6375_ZCVEN_MASK	BIT(6)
#define MT6375_ADCDONEI_MASK	BIT(4)
#define MT6375_ONESHOT_MASK	GENMASK(7, 4)
#define MT6375_ONESHOT_SHIFT	4
#define MT6375_CHANNEL_OFFSET	1

#define ADC_CONV_TIME_US	(2200 * 2)
#define ADC_POLL_TIME_US	100
#define ADC_POLL_TIMEOUT_US	1000

struct mt6375_priv {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
};

static int mt6375_adc_read_channel(struct mt6375_priv *priv, int chan, int *val)
{
	__be16 be_val;
	unsigned int addr = MT6375_REG_ADC_MONRPTH, regval;
	int ret;

	mutex_lock(&priv->lock);
	pm_stay_awake(priv->dev);

	if (chan == MT6375_ADC_FGCIC1)
		goto bypass_oneshot;

	ret = regmap_write(priv->regmap, MT6375_REG_ADC_CFG3, chan << MT6375_ONESHOT_SHIFT);
	if (ret)
		goto adc_unlock;

	usleep_range(ADC_CONV_TIME_US, ADC_CONV_TIME_US * 3 / 2);

bypass_oneshot:
	ret = regmap_read_poll_timeout(priv->regmap, MT6375_REG_ADC_CFG3, regval,
				       !(regval & MT6375_ONESHOT_MASK), ADC_POLL_TIME_US,
				       ADC_POLL_TIMEOUT_US);
	if (ret && ret != -ETIMEDOUT)
		goto adc_unlock;

	if (chan != MT6375_ADC_VBATMON)
		addr = MT6375_REG_ADC_CH1RPTH + (chan - MT6375_CHANNEL_OFFSET) * 2;

	ret = regmap_raw_read(priv->regmap, addr, &be_val, sizeof(be_val));
	if (ret)
		goto adc_unlock;

	*val = be16_to_cpu(be_val);
	ret = IIO_VAL_INT;

adc_unlock:
	pm_relax(priv->dev);
	mutex_unlock(&priv->lock);

	return ret;
}

static int mt6375_adc_read_scale(struct mt6375_priv *priv, int chan, int *val1, int *val2)
{
	switch (chan) {
	case MT6375_ADC_VBATMON:
	case MT6375_ADC_VSYS:
	case MT6375_ADC_VBAT:
	case MT6375_ADC_VREFTS:
	case MT6375_ADC_TS:
		*val1 = 1250;
		return IIO_VAL_INT;
	case MT6375_ADC_CHGVIN:
		*val1 = 6250;
		return IIO_VAL_INT;
	case MT6375_ADC_USBDP:
	case MT6375_ADC_IBUS:
	case MT6375_ADC_IBAT:
	case MT6375_ADC_USBDM:
	case MT6375_ADC_CC1:
	case MT6375_ADC_CC2:
	case MT6375_ADC_SBU1:
	case MT6375_ADC_SBU2:
		*val1 = 2500;
		return IIO_VAL_INT;
	case MT6375_ADC_TEMPJC:
	case MT6375_ADC_FGCIC1:
		*val1 = 1;
		return IIO_VAL_INT;
	case MT6375_ADC_PDVBUS:
		*val1 = 25000;
		return IIO_VAL_INT;
	}

	return -EINVAL;
}

static int mt6375_adc_read_offset(struct mt6375_priv *priv, int chan, int *val)
{
	*val = (chan == MT6375_ADC_TEMPJC) ? -56 : 0;
	return IIO_VAL_INT;
}

static int mt6375_adc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
			       int *val, int *val2, long mask)
{
	struct mt6375_priv *priv = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = mt6375_adc_read_channel(priv, chan->channel, val);
		return ret;
	case IIO_CHAN_INFO_SCALE:
		return mt6375_adc_read_scale(priv, chan->channel, val, val2);
	case IIO_CHAN_INFO_OFFSET:
		return mt6375_adc_read_offset(priv, chan->channel, val);
	}

	return -EINVAL;
}

static const struct iio_info mt6375_iio_info = {
	.read_raw = mt6375_adc_read_raw,
};

#define MT6375_ADC_CHAN(_idx, _type) {				\
	.type = _type,						\
	.channel = MT6375_ADC_##_idx,				\
	.scan_index = MT6375_ADC_##_idx,			\
	.datasheet_name = #_idx,				\
	.scan_type =  {						\
		.sign = 'u',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.endianness = IIO_CPU,				\
	},							\
	.indexed = 1,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
				BIT(IIO_CHAN_INFO_SCALE) |	\
				BIT(IIO_CHAN_INFO_OFFSET),	\
}

static const struct iio_chan_spec mt6375_adc_channels[] = {
	MT6375_ADC_CHAN(VBATMON, IIO_VOLTAGE),
	MT6375_ADC_CHAN(CHGVIN, IIO_VOLTAGE),
	MT6375_ADC_CHAN(USBDP, IIO_VOLTAGE),
	MT6375_ADC_CHAN(VSYS, IIO_VOLTAGE),
	MT6375_ADC_CHAN(VBAT, IIO_VOLTAGE),
	MT6375_ADC_CHAN(IBUS, IIO_CURRENT),
	MT6375_ADC_CHAN(IBAT, IIO_CURRENT),
	MT6375_ADC_CHAN(USBDM, IIO_VOLTAGE),
	MT6375_ADC_CHAN(TEMPJC, IIO_TEMP),
	MT6375_ADC_CHAN(VREFTS, IIO_VOLTAGE),
	MT6375_ADC_CHAN(TS, IIO_VOLTAGE),
	MT6375_ADC_CHAN(PDVBUS, IIO_VOLTAGE),
	MT6375_ADC_CHAN(CC1, IIO_VOLTAGE),
	MT6375_ADC_CHAN(CC2, IIO_VOLTAGE),
	MT6375_ADC_CHAN(SBU1, IIO_VOLTAGE),
	MT6375_ADC_CHAN(SBU2, IIO_VOLTAGE),
	MT6375_ADC_CHAN(FGCIC1, IIO_CURRENT),
	IIO_CHAN_SOFT_TIMESTAMP(MT6375_ADC_MAX_CHANNEL)
};

static irqreturn_t mt6375_adc_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct mt6375_priv *priv = iio_priv(indio_dev);
	struct {
		u16 values[MT6375_ADC_MAX_CHANNEL];
		int64_t timestamp;
	} data __aligned(8);
	int i = 0, bit, val, ret;

	memset(&data, 0, sizeof(data));
	for_each_set_bit(bit, indio_dev->active_scan_mask, indio_dev->masklength) {
		ret = mt6375_adc_read_channel(priv, bit, &val);
		if (ret < 0) {
			dev_err(priv->dev, "Failed to get channel %d conversion val\n", bit);
			goto out;
		}

		data.values[i++] = val;
	}
	iio_push_to_buffers_with_timestamp(indio_dev, &data, iio_get_time_ns(indio_dev));
out:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static inline int mt6375_adc_reset(struct mt6375_priv *priv)
{
	u16 chan_enable = 0;
	int ret;

	/* Keep ADCEN only */
	ret = regmap_update_bits(priv->regmap, MT6375_REG_ADC_CFG1,
				 MT6375_ADCEN_MASK | MT6375_ZCVEN_MASK, MT6375_ADCEN_MASK);
	if (ret)
		return ret;

	return regmap_raw_write(priv->regmap, MT6375_REG_ADC_CHEN, &chan_enable,
				sizeof(chan_enable));
}

static int mt6375_adc_probe(struct platform_device *pdev)
{
	struct mt6375_priv *priv;
	struct regmap *regmap;
	struct iio_dev *indio_dev;
	int ret;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "Failed to get regmap\n");
		return -ENODEV;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!indio_dev)
		return -ENOMEM;

	priv = iio_priv(indio_dev);
	priv->dev = &pdev->dev;
	priv->regmap = regmap;
	mutex_init(&priv->lock);
	device_init_wakeup(&pdev->dev, true);

	ret = mt6375_adc_reset(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to reset\n");
		return ret;
	}

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &mt6375_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = mt6375_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(mt6375_adc_channels);

	ret = devm_iio_triggered_buffer_setup(&pdev->dev, indio_dev, NULL,
					      mt6375_adc_trigger_handler, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to allocate iio trigger buffer\n");
		return ret;
	}

	return devm_iio_device_register(&pdev->dev, indio_dev);
}

static const struct of_device_id __maybe_unused mt6375_adc_of_match[] = {
	{ .compatible = "mediatek,mt6375-adc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6375_adc_of_match);

static struct platform_driver mt6375_adc_driver = {
	.probe = mt6375_adc_probe,
	.driver = {
		.name = "mt6375-adc",
		.of_match_table = mt6375_adc_of_match,
	},
};
module_platform_driver(mt6375_adc_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6375 ADC Driver");
MODULE_LICENSE("GPL v2");
