/*
 * dc_xpwr_gpadc.c - Dollar Cove(Xpower) GPADC Driver
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/intel_mid_pm.h>
#include <linux/rpmsg.h>
#include <linux/debugfs.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/iio/adc/dc_xpwr_gpadc.h>

#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/buffer.h>
#include <linux/iio/driver.h>
#include <linux/iio/types.h>
#include <linux/iio/consumer.h>

#define DC_PMIC_ADC_EN_REG		0x82
#define ADC_EN_VBAT			(1 << 7)
#define ADC_EN_BAT_CUR			(1 << 6)
#define ADC_EN_PMICTEMP			(1 << 5)
#define ADC_EN_SYSTHERM			(1 << 4)
#define ADC_EN_BATTEMP			(1 << 0)
#define ADC_EN_MASK			0xF1

#define DC_PMIC_ADC_CNTL_REG		0x84

#define DC_PMIC_TSP_CNTL_REG		0x85
#define ADC_GPIO0_OUTPUT_CURRENT	0xB4

#define ADC_PMIC_TEMP_DATAH_REG		0x56
#define ADC_PMIC_TEMP_DATAL_REG		0x57
#define ADC_TSP_DATAH_REG		0x58
#define ADC_TSP_DATAL_REG		0x59
#define ADC_SYST_DATAH_REG		0x5A
#define ADC_SYST_DATAL_REG		0x5B
#define ADC_VBAT_DATAH_REG		0x78
#define ADC_VBAT_DATAL_REG		0x79
#define ADC_BAT_CCUR_DATAH_REG		0x7A
#define ADC_BAT_CCUR_DATAL_REG		0x7B
#define ADC_BAT_DCIR_DATAH_REG		0x7C
#define ADC_BAT_DCIR_DATAL_REG		0x7D

#define ADC_CHANNEL0_MASK		(1 << 0)
#define ADC_CHANNEL1_MASK		(1 << 1)
#define ADC_CHANNEL2_MASK		(1 << 2)
#define ADC_CHANNEL3_MASK		(1 << 3)
#define ADC_CHANNEL4_MASK		(1 << 4)
#define ADC_CHANNEL5_MASK		(1 << 5)

#define ADC_BAT_CUR_DATAL_MASK		0x1F
#define ADC_NON_BAT_CUR_DATAL_MASK	0x0F

#define ADC_TS_PIN_CNRTL_REG           0x84
#define ADC_TS_PIN_ON                  0x32
#define ADC_GP_CURSRC_MASK		0xC0
#define ADC_GP_CURSRC_SHIFT		6
#define ADC_VOLTS_PER_BIT		80
#define ADC_CURSRC_STEP			20

#define DEV_NAME			"dollar_cove_adc"

static struct gpadc_regmap_t {
	char *name;
	int rslth;	/* GPADC Conversion Result Register Addr High */
	int rsltl;	/* GPADC Conversion Result Register Addr Low */
} gpadc_regmaps[GPADC_CH_NUM] = {
	{"BATTEMP",	0x58, 0x59, },
	{"PMICTEMP",	0x56, 0x57, },
	{"SYSTEMP0",	0x5A, 0x5B, },
	{"BATCCUR",	0x7A, 0x7B, },
	{"BATDCUR",	0x7C, 0x7D, },
	{"VBAT",	0x78, 0x79, },
};

struct gpadc_info {
	struct mutex lock;
	struct device *dev;
};

#define ADC_CHANNEL(_type, _channel, _datasheet_name) \
	{				\
		.indexed = 1,		\
		.type = _type,		\
		.channel = _channel,	\
		.datasheet_name = _datasheet_name,	\
	}

static const struct iio_chan_spec const dc_xpwr_adc_channels[] = {
	ADC_CHANNEL(IIO_TEMP, 0, "CH0"),
	ADC_CHANNEL(IIO_TEMP, 1, "CH1"),
	ADC_CHANNEL(IIO_TEMP, 2, "CH2"),
	ADC_CHANNEL(IIO_CURRENT, 3, "CH3"),
	ADC_CHANNEL(IIO_CURRENT, 4, "CH4"),
	ADC_CHANNEL(IIO_VOLTAGE, 5, "CH5"),
};

#define ADC_MAP(_adc_channel_label,			\
		     _consumer_dev_name,			\
		     _consumer_channel)				\
	{							\
		.adc_channel_label = _adc_channel_label,	\
		.consumer_dev_name = _consumer_dev_name,	\
		.consumer_channel = _consumer_channel,		\
	}

static struct iio_map iio_maps[] = {
	ADC_MAP("CH0", "THERMAL", "BATTEMP"),
	ADC_MAP("CH1", "THERMAL", "PMICTEMP"),
	ADC_MAP("CH2", "THERMAL", "SYSTEMP0"),
	ADC_MAP("CH3", "CURRENT", "BATCCUR"),
	ADC_MAP("CH4", "CURRENT", "BATDCUR"),
	ADC_MAP("CH5", "VIBAT", "VBAT"),
	{},
};

/**
 * iio_dc_xpwr_gpadc_sample - do gpadc sample.
 * @indio_dev: industrial IO GPADC device handle
 * @ch: gpadc bit set of channels to sample, for example, set ch = (1<<0)|(1<<2)
 *	means you are going to sample both channel 0 and 2 at the same time.
 * @res:gpadc sampling result
 *
 * Returns 0 on success or an error code.
 *
 * This function may sleep.
 */
static int iio_dc_xpwr_gpadc_sample(struct iio_dev *indio_dev,
				int ch, struct gpadc_result *res)
{
	struct gpadc_info *info = iio_priv(indio_dev);
	int i;
	int ret, cursrc;
	u8 th, tl;

	mutex_lock(&info->lock);
	for (i = 0; i < GPADC_CH_NUM; i++) {
		if (ch & (1 << i)) {
			th = intel_soc_pmic_readb(gpadc_regmaps[i].rslth);
			tl = intel_soc_pmic_readb(gpadc_regmaps[i].rsltl);
			res->data[i] = (th << 4) + ((tl >> 4) & 0x0F);
			if (strcmp(gpadc_regmaps[i].name, "SYSTEMP0") == 0) {
				pr_info("chn:%s adc val:%x\n",
					gpadc_regmaps[i].name,
					res->data[i]);
				ret = intel_soc_pmic_readb(
						ADC_TS_PIN_CNRTL_REG);
				cursrc = ((ret & ADC_GP_CURSRC_MASK) >>
					ADC_GP_CURSRC_SHIFT) + 1;
				cursrc *= ADC_CURSRC_STEP;
				res->data[i] = ((res->data[i] *
						ADC_VOLTS_PER_BIT) /
						cursrc);
				pr_info("chn:%s conv adc val:%x\n",
					gpadc_regmaps[i].name,
					res->data[i]);
			}
		}
	}

	mutex_unlock(&info->lock);
	return 0;
}

static int dc_xpwr_adc_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long m)
{
	int ret;
	int ch = chan->channel;
	struct gpadc_info *info = iio_priv(indio_dev);
	struct gpadc_result res;

	ret = iio_dc_xpwr_gpadc_sample(indio_dev, (1 << ch), &res);
	if (ret) {
		dev_err(info->dev, "sample failed\n");
		return ret;
	}

	*val = res.data[ch];

	return ret;
}

static const struct iio_info dc_xpwr_adc_info = {
	.read_raw = &dc_xpwr_adc_read_raw,
	.driver_module = THIS_MODULE,
};

static int dc_xpwr_gpadc_probe(struct platform_device *pdev)
{
	int err;
	struct gpadc_info *info;
	struct iio_dev *indio_dev;

	indio_dev = iio_device_alloc(sizeof(struct gpadc_info));
	if (indio_dev == NULL) {
		dev_err(&pdev->dev, "allocating iio device failed\n");
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, indio_dev);
	mutex_init(&info->lock);

	/* Use TS current source */
	intel_soc_pmic_writeb(ADC_TS_PIN_CNRTL_REG, ADC_TS_PIN_ON);

	/*
	 * To enable X-power PMIC Fuel Gauge functionality
	 * ADC channels(VBATT, IBATT and TS channels)
	 * must be enabled all the time.
	 */
	intel_soc_pmic_writeb(DC_PMIC_ADC_EN_REG, ADC_EN_MASK);

	/* Set the GPIO0 ADC output current */
	intel_soc_pmic_writeb(DC_PMIC_TSP_CNTL_REG, ADC_GPIO0_OUTPUT_CURRENT);

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = pdev->name;
	indio_dev->channels = dc_xpwr_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(dc_xpwr_adc_channels);
	indio_dev->info = &dc_xpwr_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	err = iio_map_array_register(indio_dev, iio_maps);
	if (err)
		goto err_free_device;

	err = iio_device_register(indio_dev);
	if (err < 0)
		goto err_array_unregister;

	dev_info(&pdev->dev, "dc_xpwr adc probed\n");

	return 0;

err_array_unregister:
	iio_map_array_unregister(indio_dev);
err_free_device:
	iio_device_free(indio_dev);
	return err;
}

static int dc_xpwr_gpadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	iio_device_unregister(indio_dev);
	iio_map_array_unregister(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

#ifdef CONFIG_PM
static int dc_xpwr_gpadc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct gpadc_info *info = iio_priv(indio_dev);

	mutex_lock(&info->lock);
	return 0;
}

static int dc_xpwr_gpadc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct gpadc_info *info = iio_priv(indio_dev);

	mutex_unlock(&info->lock);
	return 0;
}
#else
#define dc_xpwr_gpadc_suspend		NULL
#define dc_xpwr_gpadc_resume		NULL
#endif

static const struct dev_pm_ops dc_xpwr_gpadc_pm_ops = {
	.suspend_late = dc_xpwr_gpadc_suspend,
	.resume_early = dc_xpwr_gpadc_resume,
};

static struct platform_driver dc_xpwr_gpadc_driver = {
	.probe = dc_xpwr_gpadc_probe,
	.remove = dc_xpwr_gpadc_remove,
	.driver = {
		.name = DEV_NAME,
		.pm = &dc_xpwr_gpadc_pm_ops,
	},
};

static int __init dc_pmic_adc_init(void)
{
	return platform_driver_register(&dc_xpwr_gpadc_driver);
}
device_initcall(dc_pmic_adc_init);

static void __exit dc_pmic_adc_exit(void)
{
	platform_driver_unregister(&dc_xpwr_gpadc_driver);
}
module_exit(dc_pmic_adc_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("Dollar Cove(Xpower) GPADC Driver");
MODULE_LICENSE("GPL");
