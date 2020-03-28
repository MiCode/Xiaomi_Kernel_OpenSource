// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <dt-bindings/mfd/mt6362.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/ktime.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

#define MT6362_REG_ADCCFG1	(0xA4)
#define MT6362_REG_ADCCFG3	(0xA6)
#define MT6362_REG_ADCEN1	(0xA7)
#define MT6362_CHRPT_BASEADDR	(0xAA)

#define MT6362_REG_CHGAICR	(0x22)
#define MT6362_IAICR_MASK	(0x7F)
#define MT6362_IAICR_525mA	(0x13)

#define MT6362_TIMEMS_PERCH	(1)
#define MT6362_DESIRECH_SHIFT	(4)
#define MT6362_IRQRPTCH_SHIFT	(0)

#define MT6362_CHRPT_ADDR(_idx) (MT6362_CHRPT_BASEADDR + ((_idx) - 1) * 2)

struct mt6362_adc_data {
	struct device *dev;
	struct regmap *regmap;
	struct mutex adc_lock;
	struct completion adc_comp;
};

static const int mt6362_adcch_offsets[] = {
	0, 0, 0, 0, 0, 0, -64, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const int mt6362_adcch_times[] = {
	/* chgvin, vsys, vbat, ibus, ibat, vddp, tempjc*/
	6250, 1250, 1250, 2500, 2500, 1250, 1,
	/* verfts, ts, pdvbus, cc1, cc2, sbu1, sbu2 */
	1250, 1250, 25000, 10300, 10300, 2575, 2575,
	/* zcv */
	1250,
};

static int mt6362_adc_read_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long mask)
{
	struct mt6362_adc_data *data = iio_priv(indio_dev);
	u8 oneshot_ch;
	u16 raw = 0;
	ktime_t start_t, end_t;
	int rv;

	mutex_lock(&data->adc_lock);
	if (chan->scan_index == MT6362_ADCCH_ZCV)
		goto direct_read;
	/* read all channels enable flag first */
	rv = regmap_bulk_read(data->regmap, MT6362_REG_ADCEN1, &raw, 2);
	if (rv)
		goto out_read;

	raw = le16_to_cpu(raw);
	raw &= (~(1 << chan->channel));
	raw = cpu_to_le16(raw);

	/* disable this channel first */
	rv = regmap_bulk_write(data->regmap, MT6362_REG_ADCEN1, &raw, 2);
	if (rv)
		goto out_read;

	/* whatever, after disable this channel, enable all channels */
	raw = 0xffff;
	rv = regmap_bulk_write(data->regmap, MT6362_REG_ADCEN1, &raw, 2);
	if (rv)
		goto out_read;

	/* config onshot channel and irq reported channel */
	oneshot_ch = chan->channel << MT6362_DESIRECH_SHIFT;
	oneshot_ch |= chan->channel << MT6362_IRQRPTCH_SHIFT;
	rv = regmap_write(data->regmap, MT6362_REG_ADCCFG3, oneshot_ch);
	if (rv)
		goto out_read;

	start_t = ktime_get();
adc_rerun:
	reinit_completion(&data->adc_comp);
	wait_for_completion_timeout(&data->adc_comp,
				    msecs_to_jiffies(2 * MT6362_TIMEMS_PERCH));

	end_t = ktime_get();
	if (ktime_ms_delta(end_t, start_t) < MT6362_TIMEMS_PERCH) {
		dev_dbg(&indio_dev->dev, "wait for next conversion\n");
		goto adc_rerun;
	}

	/* clear oneshot channel and irq report channel */
	rv = regmap_write(data->regmap, MT6362_REG_ADCCFG3, 0);
	if (rv)
		goto out_read;
direct_read:
	/* dummy write then read */
	rv = regmap_bulk_write(data->regmap, chan->address, &raw, 2);
	if (rv)
		goto out_read;
	rv = regmap_bulk_read(data->regmap, chan->address, &raw, 2);
	if (rv)
		goto out_read;

	raw = be16_to_cpu(raw);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = raw;
		break;
	case IIO_CHAN_INFO_PROCESSED:
		*val = mt6362_adcch_offsets[chan->scan_index];
		*val += (raw * mt6362_adcch_times[chan->scan_index]);
		if (chan->scan_index == MT6362_ADCCH_IBUS) {
			rv = regmap_bulk_read(data->regmap,
					      MT6362_REG_CHGAICR, &raw, 1);
			if (rv)
				goto out_read;
			/* if iaicr < 525mA, additional gain is 0.76 */
			if ((raw & MT6362_IAICR_MASK) < MT6362_IAICR_525mA)
				*val = *val * 76 / 100;
		}
		break;
	default:
		rv = -EINVAL;
	}
out_read:
	mutex_unlock(&data->adc_lock);

	return rv ? : IIO_VAL_INT;
}

static const struct iio_info mt6362_adc_info = {
	.read_raw = mt6362_adc_read_raw,
};

#define MT6362_ADC_CHAN(_idx, _si, _type) \
{\
	.type = (_type),					\
	.channel = (_idx),					\
	.address = MT6362_CHRPT_ADDR(_idx),			\
	.scan_index = MT6362_ADCCH_##_si,			\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 32,					\
		.storagebits = 32,				\
	},							\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			      BIT(IIO_CHAN_INFO_PROCESSED),	\
	.datasheet_name = "ADC_CH" #_idx,			\
	.indexed = 1,						\
}

static const struct iio_chan_spec mt6362_adc_channels[] = {
	MT6362_ADC_CHAN(1, CHGVINDIV5, IIO_VOLTAGE),
	MT6362_ADC_CHAN(3, VSYS, IIO_VOLTAGE),
	MT6362_ADC_CHAN(4, VBAT, IIO_VOLTAGE),
	MT6362_ADC_CHAN(5, IBUS, IIO_CURRENT),
	MT6362_ADC_CHAN(6, IBAT, IIO_CURRENT),
	MT6362_ADC_CHAN(7, RESV5, IIO_VOLTAGE),
	MT6362_ADC_CHAN(8, TEMPJC, IIO_TEMP),
	MT6362_ADC_CHAN(9, VREFTS, IIO_VOLTAGE),
	MT6362_ADC_CHAN(10, TS, IIO_VOLTAGE),
	MT6362_ADC_CHAN(11, PDVBUSDIV10, IIO_VOLTAGE),
	MT6362_ADC_CHAN(12, PDCC1DIV4, IIO_VOLTAGE),
	MT6362_ADC_CHAN(13, PDCC2DIV4, IIO_VOLTAGE),
	MT6362_ADC_CHAN(14, PDSBU1DIV4, IIO_VOLTAGE),
	MT6362_ADC_CHAN(15, PDSBU2DIV4, IIO_VOLTAGE),
	MT6362_ADC_CHAN(17, ZCV, IIO_VOLTAGE),
	IIO_CHAN_SOFT_TIMESTAMP(15),
};

static irqreturn_t mt6362_adc_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	int vals[16] = {0}; /* 14 ch s32 numbers + 1 s64 timestamp */
	int dummy = 0, i, rv;

	for_each_set_bit(i,
			 indio_dev->active_scan_mask, indio_dev->masklength) {
		const struct iio_chan_spec *chan = indio_dev->channels + i;

		rv = mt6362_adc_read_raw(indio_dev, chan, vals + i,
					  &dummy, IIO_CHAN_INFO_PROCESSED);
		if (rv) {
			dev_warn(&indio_dev->dev, "failed to get %d val\n", i);
			goto out_trigger;
		}
	}
	iio_push_to_buffers_with_timestamp(indio_dev,
					   vals, iio_get_time_ns(indio_dev));
out_trigger:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static irqreturn_t mt6362_adc_donei_irq(int irq, void *devid)
{
	struct mt6362_adc_data *data = devid;

	complete(&data->adc_comp);
	return IRQ_HANDLED;
}

static int mt6362_adc_init(struct mt6362_adc_data *data)
{
	/* adc en = 1, zcv = 0, all channels to be enabled */
	const u8 adc_configs[] = { 0x80, 0x00, 0x00, 0xff, 0xff };

	return regmap_bulk_write(data->regmap, MT6362_REG_ADCCFG1,
				 adc_configs, sizeof(adc_configs));
}

static int mt6362_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;
	struct mt6362_adc_data *data;
	int irq, rv;

	dev_info(&pdev->dev, "%s\n", __func__);
	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;
	data = iio_priv(indio_dev);

	data->dev = &pdev->dev;
	mutex_init(&data->adc_lock);
	init_completion(&data->adc_comp);

	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap) {
		dev_err(&pdev->dev, "failed to allocate regmap\n");
		return -ENODEV;
	}

	rv = mt6362_adc_init(data);
	if (rv) {
		dev_err(&pdev->dev, "failed to init adc [%d]\n", rv);
		return rv;
	}

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &mt6362_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = mt6362_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(mt6362_adc_channels);

	rv = devm_iio_triggered_buffer_setup(&pdev->dev, indio_dev, NULL,
					     mt6362_adc_trigger_handler, NULL);
	if (rv) {
		dev_err(&pdev->dev, "failed to allocate iio trigger buffer\n");
		return rv;
	}

	irq = platform_get_irq_byname(pdev, "adc_donei");
	if (irq <= 0) {
		dev_err(&pdev->dev, "failed to get irq number\n");
		return -EINVAL;
	}

	rv = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				       mt6362_adc_donei_irq, 0, NULL, data);
	if (rv) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return rv;
	}

	rv = devm_iio_device_register(&pdev->dev, indio_dev);
	if (rv) {
		dev_err(&pdev->dev, "failed to register iio device\n");
		return rv;
	}

	platform_set_drvdata(pdev, indio_dev);

	dev_info(&pdev->dev, "%s: successful\n", __func__);
	return 0;
}

static const struct of_device_id __maybe_unused mt6362_adc_ofid_tbls[] = {
	{ .compatible = "mediatek,mt6362-adc", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6362_adc_ofid_tbls);

static struct platform_driver mt6362_adc_driver = {
	.driver = {
		.name = "mt6362-adc",
		.of_match_table = of_match_ptr(mt6362_adc_ofid_tbls),
	},
	.probe = mt6362_adc_probe,
};
#if 0
module_platform_driver(mt6362_adc_driver);
#else
static int __init mt6362_adc_driver_init(void)
{
	return platform_driver_register(&mt6362_adc_driver);
}

static void __exit mt6362_adc_driver_exit(void)
{
	platform_driver_unregister(&mt6362_adc_driver);
}
subsys_initcall(mt6362_adc_driver_init);
module_exit(mt6362_adc_driver_exit);
#endif

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6362 SPMI ADC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
