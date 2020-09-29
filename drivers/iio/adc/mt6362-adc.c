// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <dt-bindings/mfd/mt6362.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

#define MT6362_REG_DEVINFO	(0x00)
#define MT6362_REG_TMINFO	(0x0F)
#define MT6362_REG_ADCCFG1	(0xA4)
#define MT6362_REG_ADCCFG3	(0xA6)
#define MT6362_REG_ADCEN1	(0xA7)
#define MT6362_CHRPT_BASEADDR	(0xAA)
#define MT6362_CHG_STAT2	(0xE2)

#define MT6362_CHIPREV_MASK	(0x0F)
#define MT6362_CHIPREV_E2	(0x2)
#define MT6362_TMID3_MASK	BIT(4)
#define MT6362_STSYSMIN_MASK	BIT(1)
#define MT6362_ADCEN_MASK	BIT(7)

/* ADC conversion time in microseconds */
#define MT6362_TIME_PERCH	(1300)
#define MT6362_LTIME_PERCH	(2200)
#define MT6362_DESIRECH_SHIFT	(4)
#define MT6362_IRQRPTCH_SHIFT	(0)
#define MT6362_VSYSMINST_CNT	(3)

#define MT6362_ADCIBAT_OFFSET	(100 * 1000)
#define MT6362_ADCSYSMIN_OFFSET	(150 * 1000)

#define MT6362_CHRPT_ADDR(_idx) (MT6362_CHRPT_BASEADDR + ((_idx) - 1) * 2)

struct mt6362_adc_data {
	struct device *dev;
	struct regmap *regmap;
	struct mutex adc_lock;
	struct completion adc_comp;
	bool ibat_offset_flag;
	bool conv_ltime_flag;
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

static int mt6362_adc_is_sysmin_state(struct mt6362_adc_data *data, bool *state)
{
	unsigned int val = 0;
	int i, rv;

	*state = true;
	for (i = 0; i < MT6362_VSYSMINST_CNT; i++) {
		rv = regmap_read(data->regmap, MT6362_CHG_STAT2, &val);
		if (rv)
			return rv;

		/* either one is not in vsys min, treat as VBAT>VSYSMIN */
		if (!(val & MT6362_STSYSMIN_MASK)) {
			*state = false;
			break;
		}
	}

	return 0;
}

static int mt6362_adc_read_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long mask)
{
	struct mt6362_adc_data *data = iio_priv(indio_dev);
	u8 oneshot_ch;
	u16 raw = 0;
	unsigned int conv_time =
		data->conv_ltime_flag ? MT6362_LTIME_PERCH : MT6362_TIME_PERCH;
	bool state;
	int rv;

	mutex_lock(&data->adc_lock);
	if (chan->scan_index == MT6362_ADCCH_ZCV)
		goto direct_read;

	/* change ADC_EN=0 to prevent the other channels routine task */
	rv = regmap_update_bits(data->regmap,
				MT6362_REG_ADCCFG1, MT6362_ADCEN_MASK, 0);
	if (rv)
		goto out_read;

	/* config onshot channel and irq reported channel */
	oneshot_ch = chan->channel << MT6362_DESIRECH_SHIFT;
	oneshot_ch |= chan->channel << MT6362_IRQRPTCH_SHIFT;
	rv = regmap_write(data->regmap, MT6362_REG_ADCCFG3, oneshot_ch);
	if (rv)
		goto out_read;

	/* change ADC_EN=1 to start oneshot channel as the first run */
	rv = regmap_update_bits(data->regmap, MT6362_REG_ADCCFG1,
				MT6362_ADCEN_MASK, MT6362_ADCEN_MASK);
	if (rv)
		goto out_read;

	reinit_completion(&data->adc_comp);
	wait_for_completion_timeout(&data->adc_comp,
				    usecs_to_jiffies(2 * conv_time));

	/* clear oneshot channel and irq report channel */
	rv = regmap_write(data->regmap, MT6362_REG_ADCCFG3, 0);
	if (rv)
		goto out_read;
direct_read:
	/* dummy write high byte then read */
	rv = regmap_write(data->regmap, chan->address, 0);
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

		if (chan->scan_index == MT6362_ADCCH_IBAT &&
				data->ibat_offset_flag) {
			rv = mt6362_adc_is_sysmin_state(data, &state);
			if (rv)
				goto out_read;
			if (state)
				*val += MT6362_ADCSYSMIN_OFFSET;

			*val -= MT6362_ADCIBAT_OFFSET;
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
	/* adc en = 1, zcv = 0, all channels to be disabled */
	const u8 adc_configs[] = { 0x80, 0x00, 0x00, 0x00, 0x00 };

	return regmap_bulk_write(data->regmap, MT6362_REG_ADCCFG1,
				 adc_configs, sizeof(adc_configs));
}

static int mt6362_adc_init_offset_flags(struct mt6362_adc_data *data)
{
	unsigned int devinfo, tminfo;
	int rv;

	rv = regmap_read(data->regmap, MT6362_REG_DEVINFO, &devinfo);
	if (rv)
		return rv;
	rv = regmap_read(data->regmap, MT6362_REG_TMINFO, &tminfo);
	if (rv)
		return rv;

	/* if rev > e2, adc convert time will be another one */
	if ((devinfo & MT6362_CHIPREV_MASK) > MT6362_CHIPREV_E2)
		data->conv_ltime_flag = true;

	/* if rev > e2 or tmid3 == 1, need ibat offset, otherwise not */
	if ((devinfo & MT6362_CHIPREV_MASK) > MT6362_CHIPREV_E2 ||
			(tminfo & MT6362_TMID3_MASK))
		data->ibat_offset_flag = true;

	return 0;
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

	rv = mt6362_adc_init_offset_flags(data);
	if (rv) {
		dev_notice(&pdev->dev, "failed to init adc offset flags\n");
		return rv;
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

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6362 SPMI ADC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
