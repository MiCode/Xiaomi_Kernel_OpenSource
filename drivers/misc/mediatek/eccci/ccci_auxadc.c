// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_debug.h"

#define TAG "ccci_adc"

static struct platform_device *md_adc_pdev;
static int adc_num;
static int adc_val;

static int ccci_get_adc_info(struct device *dev)
{
	int ret, val;
	struct iio_channel *md_channel;

	md_channel = iio_channel_get(dev, "md-channel");

	ret = IS_ERR(md_channel);
	if (ret) {
		if (PTR_ERR(md_channel) == -EPROBE_DEFER) {
			CCCI_ERROR_LOG(-1, TAG, "%s EPROBE_DEFER\r\n",
					__func__);
			return -EPROBE_DEFER;
		}
		CCCI_ERROR_LOG(-1, TAG, "fail to get iio channel (%d)", ret);
		goto Fail;
	}
	adc_num = md_channel->channel->channel;
	ret = iio_read_channel_raw(md_channel, &val);
	iio_channel_release(md_channel);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "iio_read_channel_raw fail");
		goto Fail;
	}

	adc_val = val;
	CCCI_NORMAL_LOG(0, TAG, "md_ch = %d, val = %d\n", adc_num, adc_val);
	return ret;
Fail:
	return -1;

}

int ccci_get_adc_num(void)
{
	return adc_num;
}
EXPORT_SYMBOL(ccci_get_adc_num);

int ccci_get_adc_val(void)
{
	return adc_val;
}
EXPORT_SYMBOL(ccci_get_adc_val);

#if 0
signed int battery_get_bat_voltage(void)
{
	struct iio_channel *channel;
	int ret, val, number;

	if (!md_adc_pdev)
		return -1;
	channel = iio_channel_get(&md_adc_pdev->dev, "md-battery");

	ret = IS_ERR(channel);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "fail to get iio channel 1 (%d)", ret);
		goto BAT_Fail;
	}
	number = channel->channel->channel;
	ret = iio_read_channel_processed(channel, &val);
	iio_channel_release(channel);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "iio_read_channel_processed fail");
		goto BAT_Fail;
	}
	CCCI_NORMAL_LOG(0, TAG, "md_battery = %d, val = %d", number, val);

	return val;
BAT_Fail:
	return -1;

}
EXPORT_SYMBOL(battery_get_bat_voltage);
#endif

int get_auxadc_probe(struct platform_device *pdev)
{
	int ret;

	ret = ccci_get_adc_info(&pdev->dev);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccci get adc info fail");
		return ret;
	}
	md_adc_pdev = pdev;
	return 0;
}


static const struct of_device_id ccci_auxadc_of_ids[] = {
	{.compatible = "mediatek,md_auxadc"},
	{}
};


static struct platform_driver ccci_auxadc_driver = {

	.driver = {
			.name = "ccci_auxadc",
			.of_match_table = ccci_auxadc_of_ids,
	},

	.probe = get_auxadc_probe,
};

static int __init ccci_auxadc_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_auxadc_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "ccci auxadc driver init fail %d", ret);
		return ret;
	}
	return 0;
}

module_init(ccci_auxadc_init);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci auxadc driver");
MODULE_LICENSE("GPL");
