/*
 * Copyright (C) 2016 MediaTek, Inc.
 *
 * Author: Dongdong Cheng <dongdong.cheng@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/iopoll.h>
#include <linux/io.h>
#include <linux/iio/iio.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/mfd/mt6392/core.h>
#include <linux/mfd/mt6392/registers.h>
#include <linux/mfd/mt6397/core.h>

/* AUXADC  register mask */
/* STRUP */
#define MT6392_AUXADC_RSTB_SW_MASK		(1 << 5)
#define MT6392_AUXADC_RSTB_SW_VAL(x)		((x) << 5)
#define MT6392_AUXADC_RSTB_SEL_MASK		(1 << 7)
#define MT6392_AUXADC_RSTB_SEL_VAL(x)		((x) << 7)

/* AUXADC */
#define MT6392_AUXADC_RQST0_SET_MASK		(0xffff << 0)
#define MT6392_AUXADC_RQST0_SET_VAL(x)		((x) << 0)
#define MT6392_AUXADC_CK_AON_MASK		(0x1 << 15)
#define MT6392_AUXADC_CK_AON_VAL(x)		((x) << 15)
#define MT6392_AUXADC_12M_CK_AON_MASK		(0x1 << 15)
#define MT6392_AUXADC_12M_CK_AON_VAL(x)		((x) << 15)
#define MT6392_AUXADC_AVG_NUM_LARGE_MASK	(0x7 << 3)
#define MT6392_AUXADC_AVG_NUM_LARGE_VAL(x)	((x) << 3)
#define MT6392_AUXADC_AVG_NUM_SMALL_MASK	(0x7 << 0)
#define MT6392_AUXADC_AVG_NUM_SMALL_VAL(x)	((x) << 0)
#define MT6392_AUXADC_AVG_NUM_SEL_MASK		(0xffff << 0)
#define MT6392_AUXADC_VBUF_EN_MASK		(0x1 << 9)
#define MT6392_AUXADC_VBUF_EN_VAL(x)		((x) << 9)
#define MT6392_AUXADC_VBUF_EN_MASK		(0x1 << 9)
#define MT6392_AUXADC_VBUF_EN_VAL(x)		((x) << 9)

#define MT6392_AUXADC_RDY_COUNT			5
#define VOLTAGE_FULL_RANGE			1800

struct mt6392_auxadc_device {
	struct device *dev;
	struct mutex lock;
	struct regmap *regmap;
};

#define MT6392_AUXADC_CHANNEL(idx) {				    \
		.type = IIO_VOLTAGE,				    \
		.indexed = 1,					    \
		.channel = (idx),				    \
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED), \
}

static const struct iio_chan_spec mt6392_auxadc_iio_channels[] = {
	MT6392_AUXADC_CHANNEL(0),
	MT6392_AUXADC_CHANNEL(1),
	MT6392_AUXADC_CHANNEL(2),
	MT6392_AUXADC_CHANNEL(3),
	MT6392_AUXADC_CHANNEL(4),
	MT6392_AUXADC_CHANNEL(5),
	MT6392_AUXADC_CHANNEL(6),
	MT6392_AUXADC_CHANNEL(7),
	MT6392_AUXADC_CHANNEL(8),
	MT6392_AUXADC_CHANNEL(9),
	MT6392_AUXADC_CHANNEL(10),
	MT6392_AUXADC_CHANNEL(11),
	MT6392_AUXADC_CHANNEL(12),
	MT6392_AUXADC_CHANNEL(13),
	MT6392_AUXADC_CHANNEL(14),
	MT6392_AUXADC_CHANNEL(15),
};

static const u32 mt6392_auxadc_regs[] = {
	MT6392_AUXADC_ADC0, MT6392_AUXADC_ADC1, MT6392_AUXADC_ADC2,
	MT6392_AUXADC_ADC3, MT6392_AUXADC_ADC4, MT6392_AUXADC_ADC5,
	MT6392_AUXADC_ADC6, MT6392_AUXADC_ADC7, MT6392_AUXADC_ADC8,
	MT6392_AUXADC_ADC9, MT6392_AUXADC_ADC10, MT6392_AUXADC_ADC11,
	MT6392_AUXADC_ADC12, MT6392_AUXADC_ADC12, MT6392_AUXADC_ADC12,
	MT6392_AUXADC_ADC12,
};

/*
 * mt6392_auxadc_read() - Get voltage from a auxadc channel
 *
 * @Channel: Auxadc channel to choose
 *             0 : BATSNS
 *             0 : ISENSE
 *             2 : VCDT
 *             3 : BATON
 *             4 : PMIC_TEMP
 *             6 : TYPE-C
 *             8 : DP/DM
 *             9-12 : Reserve
 *	       12-15 : shared
 *
 * This returns the current voltage of the specified channel in mV,
 * zero for not supported channel, a negative number on error.
 */
static int mt6392_auxadc_read(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan)
{
	int ret, regval, adc_rdy;
	int raw_data, raw_mask, adc_div, adc_result, r_val_temp;
	int count = 0;
	struct mt6392_auxadc_device *adc_dev = iio_priv(indio_dev);

	mutex_lock(&adc_dev->lock);

	ret = regmap_update_bits(adc_dev->regmap, MT6392_AUXADC_RQST0_SET,
		MT6392_AUXADC_RQST0_SET_MASK, (MT6392_AUXADC_RQST0_SET_VAL(0x1) << chan->channel));
	if (ret < 0) {
		mutex_unlock(&adc_dev->lock);
		return ret;
	}

	/* the spec is 10us */
	udelay(50);

	/* check auxadc is ready */
	do {
		ret = regmap_read(adc_dev->regmap, mt6392_auxadc_regs[chan->channel], &regval);
		if (ret < 0) {
			mutex_unlock(&adc_dev->lock);
			return ret;
		}
		adc_rdy = (regval >> 15) & 1;
		if (adc_rdy != 0)
			break;
		count++;
	} while (count < MT6392_AUXADC_RDY_COUNT);

	if (adc_rdy != 1) {
		mutex_unlock(&adc_dev->lock);
		dev_err(adc_dev->dev, "Failed to read adc ready register: %d\n", adc_rdy);
		return -ETIMEDOUT;
	}

	/* get the raw data and calculate the adc result of adc */
	ret = regmap_read(adc_dev->regmap, mt6392_auxadc_regs[chan->channel], &regval);
	if (ret < 0) {
		mutex_unlock(&adc_dev->lock);
		return ret;
	}

	switch (chan->channel) {
	case 0:
	case 1:
		r_val_temp = 3;
		raw_mask = 0x7fff;
		adc_div = 32768;
		break;

	case 2:
	case 3:
	case 4:
		r_val_temp = 1;
		raw_mask = 0xfff;
		adc_div = 4096;
		break;

	case 5:
	case 6:
	case 7:
	case 8:
		r_val_temp = 2;
		raw_mask = 0xfff;
		adc_div = 4096;
		break;

	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		r_val_temp = 1;
		raw_mask = 0xfff;
		adc_div = 4096;
		break;
	}

	/* get auxadc raw data */
	raw_data = regval & raw_mask;

	/* get auxadc real result*/
	adc_result = (raw_data * r_val_temp * VOLTAGE_FULL_RANGE) / adc_div;

	mutex_unlock(&adc_dev->lock);

	return adc_result;
}

static int mt6392_auxadc_read_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int *val,
				  int *val2,
				  long info)
{
	int ret;

	switch (info) {
	case IIO_CHAN_INFO_PROCESSED:
		*val = mt6392_auxadc_read(indio_dev, chan);
		if (*val < 0) {
			dev_err(indio_dev->dev.parent,
				"failed to sample data on channel[%d]\n",
				chan->channel);
			return *val;
		}
		ret = IIO_VAL_INT;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mt6392_auxadc_hw_init(struct mt6392_auxadc_device *adc_dev)
{
	int ret;

	ret = regmap_update_bits(adc_dev->regmap, MT6392_AUXADC_CON0,
		MT6392_AUXADC_CK_AON_MASK | MT6392_AUXADC_12M_CK_AON_MASK,
		MT6392_AUXADC_CK_AON_VAL(0x0) | MT6392_AUXADC_12M_CK_AON_VAL(0x0));
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(adc_dev->regmap, MT6392_STRUP_CON10,
		MT6392_AUXADC_RSTB_SW_MASK | MT6392_AUXADC_RSTB_SEL_MASK,
		MT6392_AUXADC_RSTB_SW_VAL(0x1) | MT6392_AUXADC_RSTB_SEL_VAL(0x1));
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(adc_dev->regmap, MT6392_AUXADC_CON1,
		MT6392_AUXADC_AVG_NUM_LARGE_MASK | MT6392_AUXADC_AVG_NUM_SMALL_MASK,
		MT6392_AUXADC_AVG_NUM_LARGE_VAL(0x6) | MT6392_AUXADC_AVG_NUM_SMALL_VAL(0x2));
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(adc_dev->regmap, MT6392_AUXADC_CON2,
		MT6392_AUXADC_AVG_NUM_SEL_MASK, MT6392_AUXADC_RSTB_SW_VAL(0x3));
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(adc_dev->regmap, MT6392_AUXADC_CON10,
		MT6392_AUXADC_VBUF_EN_MASK, MT6392_AUXADC_VBUF_EN_VAL(0x1));
	if (ret < 0)
		return ret;

	return 0;
}

static const struct iio_info mt6392_auxadc_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &mt6392_auxadc_read_raw,
};

static int mt6392_auxadc_probe(struct platform_device *pdev)
{
	struct mt6392_auxadc_device *adc_dev;
	struct iio_dev *indio_dev;
	int ret;
	u32 val;
	struct mt6397_chip *mt6392_chip = dev_get_drvdata(pdev->dev.parent);

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc_dev));
	if (!indio_dev)
		return -ENOMEM;

	adc_dev = iio_priv(indio_dev);
	adc_dev->regmap = mt6392_chip->regmap;

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->info = &mt6392_auxadc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = mt6392_auxadc_iio_channels;
	indio_dev->num_channels = ARRAY_SIZE(mt6392_auxadc_iio_channels);

	mutex_init(&adc_dev->lock);

	platform_set_drvdata(pdev, indio_dev);

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register iio device!\n");
		return ret;
	}

	ret = mt6392_auxadc_hw_init(adc_dev);
	if (ret < 0)
		return ret;

	return 0;
}

static int mt6392_auxadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	iio_device_unregister(indio_dev);

	return 0;
}

static const struct of_device_id mt6392_auxadc_of_match[] = {
	{ .compatible = "mediatek,mt6392-adc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6392_auxadc_of_match);

static struct platform_driver mt6392_auxadc_driver = {
	.driver = {
		.name   = "mt6392-adc",
		.of_match_table = mt6392_auxadc_of_match,
	},
	.probe	= mt6392_auxadc_probe,
	.remove	= mt6392_auxadc_remove,
};
module_platform_driver(mt6392_auxadc_driver);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dongdong Cheng <dongdong.cheng@mediatek.com>");
MODULE_DESCRIPTION("AUXADC Driver for MediaTek MT6392 PMIC");
MODULE_ALIAS("platform:mt6392-adc");
